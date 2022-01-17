//
// Created by jarod on 2019-08-08.
//

#ifndef LIBRAFT_COUNT_SERVER_H
#define LIBRAFT_COUNT_SERVER_H

#include "servant/Application.h"
#include "RaftNode.h"

using namespace tars;

class RaftNode;

/**
 * 使用说明:
 * @tparam S
 */
template<typename S, typename I>
class RaftServer : public Application, public TC_Thread
{
public:

	/**
	 * 析构
	 */
	virtual ~RaftServer() {};

	/**
	 * 服务初始化
	 **/
	virtual void initialize() = 0;

	/**
	 * 服务销毁
	 **/
	virtual void destroyApp() = 0;

	/**
	 * 一般给自动测试使用
	 * @param index
	 */
	void initForUnitTest(size_t index, const TC_Config &config, const RaftNode::NodeInfo &nodeInfo)
	{
		_index = index;

		_config = config;

		_nodeInfo = nodeInfo;
	}

	/**
	 * 获得服务索引, 给自动测试用
	 * @return
	 */
	size_t getIndexForTest() { return _index; }

	/**
	 * 等待服务启动成功, 可以响应网络请求
	 * 给自动测试框架使用
	 */
	void waitStartForTest()
	{
		main(_config.tostr());

		start();

		waitForReady();
	}

	/**
	 * 返回nodeInfo, 一般给自动测试用
	 * @return
	 */
	const RaftNode::NodeInfo &getNodeInfo() { return _nodeInfo; }

	/**
	 * 返回raftnode
	 * @return
	 */
	const shared_ptr<RaftNode> &node() { return _raftNode; }

	/**
	 * 获取状态机
	 * @return
	 */
	const shared_ptr<S> & getStateMachine() { return _stateMachine; }

protected:
	virtual void run()
	{
		waitForShutdown();
	}

	/**
	 * 释放资源
	 */
	void onDestroyRaft();

	/**
	 *
	 * @param raftOptions
	 * @param bussObj , 业务服务Obj名称, 比如CountObj
	 */
	template<typename... Args>
	void onInitializeRaft(const RaftOptions &raftOptions, const string &bussObj, const Args &...args)
	{
		//测试需要, 如果是空的设置好主控地址, 自动更新
		if (_nodeInfo.nodes.empty() && Application::getCommunicator()->getProperty("locator").empty())
		{
			Application::getCommunicator()->setProperty("locator", "raft.FrameworkServer.QueryObj@tcp -h 127.0.0.1 -p 13004");
		}
		else if (!_nodeInfo.nodes.empty())
		{
			Application::getCommunicator()->setProperty("locator", "");
		}

		_stateMachine.reset(new S(args...));

		_raftNode = std::make_shared<RaftNode>(_stateMachine);

		_stateMachine->initialize(_raftNode);

		_nodeInfo.bussObj = ServerConfig::Application + "." + ServerConfig::ServerName + "." + bussObj;
		_nodeInfo.raftObj = ServerConfig::Application + "." + ServerConfig::ServerName + ".RaftObj";

		string adapter = this->getServantHelper()->getServantAdapter(_nodeInfo.raftObj);

		if(adapter.empty())
		{
			cerr << _nodeInfo.raftObj << " not exists!" << endl;
			TLOG_ERROR(_nodeInfo.raftObj << " not exists!" << endl);
			exit(-1);
		}

		_nodeInfo.raftEp = getEpollServer()->getBindAdapter(adapter)->getEndpoint();

		TLOG_ERROR("raft Endpoint: " << _nodeInfo.raftObj << endl);
		retry:
		//从主控获取
		if (_nodeInfo.nodes.empty())
		{
			vector<TC_Endpoint> eps1 = _raftNode->getEndpoints(_nodeInfo.raftObj);
			vector<TC_Endpoint> eps2 = _raftNode->getEndpoints(_nodeInfo.bussObj);

			if(eps1.size() != eps2.size())
			{
				TC_Common::msleep(100);
				goto retry;
			}

			for (size_t i = 0; i < eps1.size(); i++)
			{
				_nodeInfo.nodes.push_back(make_pair(eps1[i], eps2[i]));
			}

			if(eps1.empty())
			{
				TLOG_ERROR(_nodeInfo.raftObj << " ep size:" << eps1.size() << ", " << _nodeInfo.bussObj << " ep size:" << eps2.size() << endl);
				TC_Common::msleep(100);
				goto retry;				
			}
		}

		assert(!_nodeInfo.nodes.empty());

		_raftNode->init(raftOptions, _nodeInfo, this);

		//注册服务
		addServant<I>(_nodeInfo.bussObj);
	}

protected:;
	size_t _index = 0;

	TC_Config _config;

	shared_ptr<S>   _stateMachine;

	shared_ptr<RaftNode>            _raftNode;

	RaftNode::NodeInfo 	_nodeInfo;

};

template<typename S, typename I>
void RaftServer<S, I>::onDestroyRaft()
{
	_raftNode->terminate();

	if(_stateMachine)
	{
		_stateMachine.reset();
	}
}

#endif //LIBRAFT_KVSERVER_H
