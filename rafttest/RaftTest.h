//
// Created by jarod on 2021/8/28.
//

#ifndef RAFT_RAFTTEST_H
#define RAFT_RAFTTEST_H

#include "ConfigTemplate.h"
#include "framework/framework.h"
#include "framework/FrameworkServer.h"
#include "framework/DbHandle.h"
#include "RaftNode.h"

template<typename T>
class RaftTest
{
public:
	void initialize(const string &app, const string &serverName, const string &servantName, const string &snapshotDir, int raftPort, int bussPort);

	void setBussFunc(std::function<void(const shared_ptr<T> &)> bussFunc) { _bussFunc = bussFunc; }

	void setInitFunc(std::function<void(const shared_ptr<T> &)> initFunc) { _initFunc = initFunc; }

	void testBaseProgress();

	void testCheckLeaderNotStopLeader();

	void testCheckLeaderStopLeader();

	void testRestartProgress();

	void testLostData();

	void testInstallSnapshot();

	void testStopLeader();

	void testRaftAddNode();

	void testRaftDelNode();

	void testBenchmark();

	void testAutoAddPeer();

	void testAutoDelPeer();

	void testAll();

	void createServers(int size, bool fromRegistry = false);

	size_t waitLeader();

	void waitNewLeader(uint32_t leaderIndex);

	void waitCluster(const shared_ptr<T> &server);
	void waitCluster(size_t index);

	void waitCluster();

	void startFramework(FrameworkServer &fs);

	void stopFramework(FrameworkServer &fs);

	void testPing(const shared_ptr<T> &server);

	void startAll();

	void start(size_t index);

	void start(const shared_ptr<T> &server);

	void stopAll();
	void stop(size_t index);
	void stop(const shared_ptr<T> &server);

	void testConnect(const TC_Endpoint &ep, int iRet);

	set<int> randNoLeaderIndex(size_t nodeCount, uint32_t leaderIndex, size_t count);

	void refreshRegistry(const RaftNode::NodeInfo &nodeInfo, int count);

	void execAll(std::function<void(const shared_ptr<T> &)>);

	void execBussFunc();

	size_t size() { return _servers.size(); }

	shared_ptr<T> get(size_t index) { return _servers.at(index); }

	string getRaftObj() { return _app + "." + _server + ".RaftObj"; }
	string getBussObj() { return _app + "." + _server + "." + _servant; }

	int getRaftPort(size_t index) { return _raftPort + index; }
	int getBussPort(size_t index) { return _bussPort + index; }

	const string &app() { return _app; }
	const string &server() { return _server; }

protected:

	TC_Config getConfig(size_t index);

	pair<TC_Endpoint, TC_Endpoint> node(size_t index);

protected:
	string _app;
	string _server;
	string _servant;
	string _snapshotDir;
	int    _raftPort;
	int    _bussPort;

	vector<shared_ptr<T>> _servers;

	std::function<void(const shared_ptr<T> &)> _initFunc;

	std::function<void(const shared_ptr<T> &)> _bussFunc;
};

template<typename T>
void RaftTest<T>::initialize(const string &app, const string &serverName, const string &servantName, const string &snapshotDir, int raftPort, int bussPort)
{
	_app = app;
	_server = serverName;
	_servant = servantName;
	_snapshotDir = snapshotDir;
	_raftPort = raftPort;
	_bussPort = bussPort;

	_servers.clear();

	TC_File::removeFile("./app_log/" + serverName + "/data", true);
}

template<typename T>
TC_Config RaftTest<T>::getConfig(size_t index)
{
	return ConfigTemplate::getConfig(_app, _server, _servant, _raftPort + index, _bussPort + index);
}

template<typename T>
pair<TC_Endpoint, TC_Endpoint> RaftTest<T>::node(size_t index)
{
	TC_Config config = getConfig(index);

	string confPath = "/tars/application/server/" + _app + "." + _server;

	return make_pair(TC_Endpoint(config[confPath + ".RaftObjAdapter<endpoint>"]),
			TC_Endpoint(config[confPath +  "." + _servant + "Adapter<endpoint>"]));
}

template<typename T>
void RaftTest<T>::createServers(int size, bool fromRegistry)
{
	LOG_CONSOLE_DEBUG << endl;
	TC_File::removeFile("./app_log/" + server() + "/data", true);

	_servers.clear();

	RaftNode::NodeInfo nodeInfo;

	//启动测试，写死IP, 和CONFIG里面匹配
	if (!fromRegistry)
	{
		for(size_t i = 0; i < size; i++)
		{
			nodeInfo.nodes.push_back(node(i));
		}
	}

	for(size_t i = 0; i < size; i++)
	{
		_servers.push_back(std::make_shared<T>());
		_servers.back()->initForUnitTest(i, getConfig(i), nodeInfo);
	}
}

template<typename T>
size_t RaftTest<T>::waitLeader()
{
	LOG_CONSOLE_DEBUG << endl;
	int64_t now = TNOW;

	retry:
	for(size_t i = 0; i < _servers.size(); i++)
	{
		if(_servers[i]->node()->isLeader())
		{
			return i;
		}
	}

	TC_Common::msleep(10);
	//长时间未加入集群, 认为失败
	if(TNOW - now > 10)
	{
		EXPECT_TRUE(false);
	}

	goto retry;

}

template<typename T>
void RaftTest<T>::waitNewLeader(uint32_t leaderIndex)
{
	LOG_CONSOLE_DEBUG << endl;
	uint32_t leaderId = _servers[leaderIndex]->node()->getLeaderId();
	retrys:
	for (size_t i = 0; i < _servers.size(); i++)
	{
		if (i != leaderIndex)
		{
			// LOG_CONSOLE_DEBUG << servers[i]->node->node()->getLeaderId() << ", " << leaderId << endl;

			if (_servers[i]->node()->getLeaderId() == leaderId || _servers[i]->node()->getLeaderId() == 0)
			{
				TC_Common::msleep(20);

				goto retrys;
			}
		}
	}
}

template<typename T>
void RaftTest<T>::waitCluster(size_t index)
{
	LOG_CONSOLE_DEBUG << endl;
	waitCluster(_servers.at(index));
}

template<typename T>
void RaftTest<T>::waitCluster(const shared_ptr<T> &server)
{
	LOG_CONSOLE_DEBUG << endl;
	int64_t now = TNOW;
	retry:
	if(server->node()->inCluster())
	{
		return;
	}
	else
	{
		TC_Common::msleep(20);

		//长时间未加入集群, 认为失败
		if(TNOW - now > 10)
		{
			ASSERT_TRUE(false);
		}
		goto retry;
	}
}

template<typename T>
void RaftTest<T>::waitCluster()
{
	LOG_CONSOLE_DEBUG << endl;
	int64_t now = TNOW;

	retry:
	for(auto server : _servers)
	{
		if(server->node()->inCluster())
		{
			continue;
		}
		else
		{
			TC_Common::msleep(20);

			//长时间未加入集群, 认为失败
			if(TNOW - now > 10)
			{
				ASSERT_TRUE(false);
			}
			goto retry;
		}
	}
}

template<typename T>
void RaftTest<T>::startFramework(FrameworkServer &fs)
{
	LOG_CONSOLE_DEBUG << endl;
	fs.main(FRAMEWORK_CONFIG);
	fs.start();
	fs.waitForReady();
}

template<typename T>
void RaftTest<T>::stopFramework(FrameworkServer &fs)
{
	LOG_CONSOLE_DEBUG << endl;
	fs.terminate();
}

template<typename T>
void RaftTest<T>::testPing(const shared_ptr<T> &server)
{
	try
	{
		RaftPrx prx = server->node()->getRaftPrx();

		prx->tars_ping();

		ASSERT_TRUE(true);
	}
	catch(exception &ex)
	{
		LOG_CONSOLE_DEBUG << ex.what() << endl;
		// ASSERT_TRUE(false);
	}
}

template<typename T>
void RaftTest<T>::execBussFunc()
{
	execAll([&](const shared_ptr<T> &server){
		testPing(server);
		_bussFunc(server);
	});
}

template<typename T>
void RaftTest<T>::execAll(std::function<void(const shared_ptr<T> &)> func)
{
	for(size_t i = 0; i < _servers.size(); i++)
	{
		func(_servers[i]);
	}
}


template<typename T>
void RaftTest<T>::startAll()
{
	LOG_CONSOLE_DEBUG << endl;
	for(size_t i = 0; i < _servers.size(); i++)
	{
		start(i);
	}
}

template<typename T>
void RaftTest<T>::stopAll()
{
	LOG_CONSOLE_DEBUG << endl;
	for(size_t i = 0; i < _servers.size(); i++)
	{
		stop(i);
	}
}

template<typename T>
void RaftTest<T>::start(size_t index)
{
	LOG_CONSOLE_DEBUG << endl;
	_servers.at(index)->waitStartForTest();
	testConnect(_servers[index]->getNodeInfo().raftEp, 0);
}

template<typename T>
void RaftTest<T>::start(const shared_ptr<T> &server)
{
	LOG_CONSOLE_DEBUG << endl;
	server->waitStartForTest();
	testConnect(server->getNodeInfo().raftEp, 0);
}

template<typename T>
void RaftTest<T>::stop(size_t index)
{
	LOG_CONSOLE_DEBUG << endl;
	_servers.at(index)->terminate();
	_servers.at(index)->join();

//	testConnect(_servers.at(index)->getNodeInfo().raftEp, -1);
}


template<typename T>
void RaftTest<T>::stop(const shared_ptr<T> &server)
{
	LOG_CONSOLE_DEBUG << endl;
	server->terminate();
	server->join();

	testConnect(server->getNodeInfo().raftEp, -1);
}

template<typename T>
void RaftTest<T>::testConnect(const TC_Endpoint &ep, int iRet)
{
	LOG_CONSOLE_DEBUG << ep.toString() << endl;
	TC_Socket s;
	s.createSocket(SOCK_STREAM, AF_INET);
	int ret = s.connectNoThrow(ep.getHost(), ep.getPort());

	LOG_CONSOLE_DEBUG << "ret:" << ret << ", ep:" << ep.toString() << endl;

	ASSERT_TRUE(ret == iRet);
}

template<typename T>
set<int> RaftTest<T>::randNoLeaderIndex(size_t nodeCount, uint32_t leaderIndex, size_t count)
{
	LOG_CONSOLE_DEBUG << endl;
	set<int> data;

	do
	{
		int index = rand()%nodeCount;
		if(index != leaderIndex && data.find(index) == data.end())
		{
			data.insert(index);
		}

	}while(data.size() < count);

	return data;

}

template<typename T>
void RaftTest<T>::refreshRegistry(const RaftNode::NodeInfo &nodeInfo, int count)
{
	LOG_CONSOLE_DEBUG << endl;
	ServantPrx prx1 = Application::getCommunicator()->stringToProxy<ServantPrx>(nodeInfo.raftObj);
	prx1->getEndpoint();

	ServantPrx prx2 = Application::getCommunicator()->stringToProxy<ServantPrx>(nodeInfo.bussObj);
	prx2->getEndpoint();

	do
	{
		TC_Common::sleep(1);

		auto eps1 = prx1->getEndpoint();

		auto eps2 = prx2->getEndpoint();

		LOG_CONSOLE_DEBUG << "active eps1 size:" << eps1.size() << ", eps2 size:" << eps2.size() << endl;

		if(eps1.size() != eps2.size())
		{
			continue;
		}

		if(eps1.size() == count)
		{
			break;
		}

	}while(true);
}

/////////////////////////////////////////////////////////////////////////////////

template<typename T>
void RaftTest<T>::testBaseProgress()
{
	createServers(5, false);

	startAll();

	waitLeader();

	waitCluster();

	if(_initFunc)
	{
		_initFunc(get(0));
	}

	execBussFunc();

	stopAll();

	LOG_CONSOLE_DEBUG << endl;
}

template<typename T>
void RaftTest<T>::testCheckLeaderNotStopLeader()
{
	createServers(5, false);

	startAll();

	int leaderIndex = waitLeader();

	waitCluster();

	if(_initFunc)
	{
		_initFunc(get(0));
	}

	execBussFunc();

	auto stopIndex = randNoLeaderIndex(size(), leaderIndex, 2);

	for(auto i : stopIndex)
	{
		stop(i);
	}

	vector<uint32_t> leftLeader;

	execAll([&](const shared_ptr<T> &server){
		if(stopIndex.find(server->getIndexForTest()) != stopIndex.end())
		{
			leftLeader.push_back(server->node()->getLeaderId());
		}
	});

	//leaderId保持不变
	ASSERT_TRUE(leftLeader[0] == get(leaderIndex)->node()->getLeaderId());

	for(size_t i = 1; i < leftLeader.size(); i++)
	{
		ASSERT_TRUE(leftLeader[i] == leftLeader[i-1]);
	}

	stopAll();
}


template<typename T>
void RaftTest<T>::testCheckLeaderStopLeader()
{
	createServers(5, false);

	startAll();

	int leaderIndex = waitLeader();
	waitCluster();

	uint32_t leaderId = get(leaderIndex)->node()->getLeaderId();

	if(_initFunc)
	{
		_initFunc(get(0));
	}

	execBussFunc();

	stop(leaderIndex);

	waitNewLeader(leaderIndex);

	vector<uint32_t> leftLeader;
	execAll([&](const shared_ptr<T> &server){
		if(server->getIndexForTest() != leaderIndex)
		{
			waitCluster();

			leftLeader.push_back(server->node()->getLeaderId());
		}
	});

	ASSERT_TRUE(leftLeader[0] != leaderId);

	for(size_t i = 1; i < leftLeader.size(); i++)
	{
		ASSERT_TRUE(leftLeader[i] == leftLeader[i-1]);
	}

	stopAll();
}

template<typename T>
void RaftTest<T>::testRestartProgress()
{
	createServers(5, false);

	startAll();

	waitLeader();

	waitCluster();

	if(_initFunc)
	{
		_initFunc(get(0));
	}

	execBussFunc();

	stop(0);
	stop(1);

	waitCluster();

	TC_Common::sleep(2);

	_bussFunc(get(2));
	_bussFunc(get(3));
	_bussFunc(get(4));

	start(0);
	testPing(get(0));

	start(1);
	testPing(get(1));

	waitLeader();

	waitCluster();

	execBussFunc();

	stopAll();

}

template<typename T>
void RaftTest<T>::testLostData()
{
	createServers(5, false);

	startAll();

	size_t leaderIndex = waitLeader();

	waitCluster();

	if(_initFunc)
	{
		_initFunc(get(0));
	}

	auto noLeaderIndex = *randNoLeaderIndex(size(), leaderIndex, 1).begin();

	stop(noLeaderIndex);

	for(size_t i = 0; i < 10; i++) {

		_bussFunc(get(leaderIndex));
	}

	TC_File::removeFile(get(noLeaderIndex)->node()->getRaftOptions().dataDir, true);

	TC_File::removeFile(ServerConfig::DataPath + _snapshotDir + "-" + TC_Common::tostr(noLeaderIndex) + "/", true);

	start(noLeaderIndex);
	waitCluster(noLeaderIndex);

	_bussFunc(get(noLeaderIndex));

	execBussFunc();

	stopAll();
}

template<typename T>
void RaftTest<T>::testInstallSnapshot()
{
	createServers(5, false);

	startAll();

	size_t leaderIndex = waitLeader();

	shared_ptr<T> leader = get(leaderIndex);

	waitCluster();

	if(_initFunc)
	{
		_initFunc(get(0));
	}

	auto noLeaderIndex = *randNoLeaderIndex(size(), leaderIndex, 1).begin();

	stop(noLeaderIndex);

	for(size_t i = 0; i < 100; i++) {
		_bussFunc(get(leaderIndex));
	}

	leader->node()->takeSnapshot();

	TC_File::removeFile("./app_log/" + app() + "/data/count-log-" + TC_Common::tostr(noLeaderIndex) + "/", true);
	TC_File::removeFile("./app_log/" + app() + "/data/raft-log-" + TC_Common::tostr(noLeaderIndex) + "/", true);

	start(noLeaderIndex);

	waitCluster(noLeaderIndex);

	_bussFunc(get(noLeaderIndex));

	execBussFunc();

	stopAll();
}

template<typename T>
void RaftTest<T>::testStopLeader()
{
	createServers(5, false);

	startAll();

	size_t leaderIndex = waitLeader();

	waitCluster();

	if(_initFunc)
	{
		_initFunc(get(0));
	}

	execBussFunc();

	stop(leaderIndex);

	waitNewLeader(leaderIndex);

	execAll([&](const shared_ptr<T> &server){

		if (server->getIndexForTest() != leaderIndex)
		{
			_bussFunc(server);
		}
	});

	start(leaderIndex);

	execAll([&](const shared_ptr<T> &server){

		waitCluster(server);
		_bussFunc(server);
	});

	stopAll();
}

template<typename T>
void RaftTest<T>::testRaftAddNode()
{
	createServers(3, false);

	startAll();

	size_t index = waitLeader();

	if(_initFunc)
	{
		_initFunc(get(0));
	}

	for(int i = 0; i < 100; i++) {
		execBussFunc();
	}

	get(index)->node()->takeSnapshot();

	bool terminate = false;

	std::thread th([&]{
		while(!terminate) {

			execBussFunc();

			TC_Common::msleep(20);
		}});

	TC_Common::sleep(5);

	RaftNode::NodeInfo nodeInfo = get(0)->getNodeInfo();
	nodeInfo.nodes.push_back(node(4));

	shared_ptr<T> server4 = std::make_shared<T>();
	server4->initForUnitTest(4, getConfig(4), nodeInfo);

	start(server4);

	Server server;
	server.serverId = RaftNode::createServerId(nodeInfo.nodes.back().first);
	server.endPoint = server4->getEpollServer()->getBindAdapter(server4->getServantHelper()->getServantAdapter(getRaftObj()))->getEndpoint().toString();
	server.bussEndPoint = server4->getEpollServer()->getBindAdapter(server4->getServantHelper()->getServantAdapter(getBussObj()))->getEndpoint().toString();
	server.obj = getRaftObj();
	server.bussObj = getBussObj();

	LOG_CONSOLE_DEBUG << server.writeToJsonString() << endl;

	raft::PeersRequest req;
	req.servers.push_back(server);

	raft::PeersResponse rsp;
	get(index)->node()->getRaftPrx()->addPeers(req, rsp);
	LOG_CONSOLE_DEBUG << "rsp: " << rsp.writeToJsonString() << endl;

	waitCluster(server4);

	terminate = true;

	th.join();

	_bussFunc(server4);

	stopAll();

	stop(server4);
}


template<typename T>
void RaftTest<T>::testRaftDelNode()
{
	bool terminate = false;

	createServers(5, false);

	startAll();

	waitCluster();

	size_t leaderIndex = waitLeader();

	if(_initFunc)
	{
		_initFunc(get(0));
	}

	for (int i = 0; i < 100; i++)
	{
		execBussFunc();
	}

	get(leaderIndex)->node()->takeSnapshot();

	Server server;

	for(size_t i = 0; i < size(); i++) {
		if(i != leaderIndex) {
			server = get(i)->node()->getLocalServer();
			break;
		}
	}

	std::thread th([&]
	{
		while (!terminate)
		{
			execAll([&](const shared_ptr<T> &s){

				if (s->node()->getLocalServer().serverId != server.serverId)
				{
					_bussFunc(s);
				}
			});

			TC_Common::msleep(20);
		}
	});

	TC_Common::sleep(5);

	raft::PeersRequest req;
	req.servers.push_back(server);

	get(leaderIndex)->node()->getRaftPrx()->tars_ping();

	raft::PeersResponse rsp;
	LOG_CONSOLE_DEBUG << "removePeers ep: " << server.endPoint << endl;

	try
	{

		get(leaderIndex)->node()->getRaftPrx()->removePeers(req, rsp);
	}
	catch (exception &ex)
	{
		LOG_CONSOLE_DEBUG << ex.what() << endl;
	}

	terminate = true;

	th.join();

	_bussFunc(get(leaderIndex));

	stopAll();
}

template<typename T>
void RaftTest<T>::testBenchmark()
{
	createServers(5, false);

	startAll();

	waitLeader();

	waitCluster();

	if(_initFunc)
	{
		_initFunc(get(0));
	}

	int count = 1000;
	int64_t start = TC_Common::now2us();
	for(size_t i = 0; i < count; i++) {

		execBussFunc();
	}

	LOG_CONSOLE_DEBUG << "avg cost:" << (TC_Common::now2us() - start)/(count * size()) << endl;
	stopAll();
}

template<typename T>
void RaftTest<T>::testAutoAddPeer()
{
	RaftNode::NodeInfo nodeInfo;
	nodeInfo.raftObj = getRaftObj();
	nodeInfo.bussObj = getBussObj();

	CDbHandle::cleanEndPoint();
	CDbHandle::addActiveEndPoint(getRaftObj(), getRaftPort(0), 1);
	CDbHandle::addActiveEndPoint(getRaftObj(), getRaftPort(1), 1);
	CDbHandle::addActiveEndPoint(getRaftObj(), getRaftPort(2), 1);

	CDbHandle::addActiveEndPoint(getBussObj(), getBussPort(0), 1);
	CDbHandle::addActiveEndPoint(getBussObj(), getBussPort(1), 1);
	CDbHandle::addActiveEndPoint(getBussObj(), getBussPort(2), 1);

	FrameworkServer fs;
	startFramework(fs);

	refreshRegistry(nodeInfo, 3);

	createServers(3, true);

	startAll();

	waitCluster();

	waitLeader();
	if(_initFunc)
	{
		_initFunc(get(0));
	}
	for (int i = 0; i < 100; i++)
	{
		execBussFunc();
	}

	//主控增加增加一个节点
	CDbHandle::addActiveEndPoint(getRaftObj(), getRaftPort(3), 1);
	CDbHandle::addActiveEndPoint(getBussObj(), getBussPort(3), 1);

	refreshRegistry(nodeInfo, 4);

	TC_Common::msleep(100);

	shared_ptr<T> server3 = std::make_shared<T>();
	server3->initForUnitTest(3, getConfig(3), nodeInfo);
	start(server3);

	waitCluster(server3);

	_bussFunc(server3);

	stop(server3);

	stopAll();

	stopFramework(fs);
}

template<typename T>
void RaftTest<T>::testAutoDelPeer()
{
	RaftNode::NodeInfo nodeInfo;
	nodeInfo.raftObj = getRaftObj();
	nodeInfo.bussObj = getBussObj();

	FrameworkServer fs;
	startFramework(fs);

	CDbHandle::cleanEndPoint();
	CDbHandle::addActiveEndPoint(nodeInfo.raftObj, getRaftPort(0), 1);
	CDbHandle::addActiveEndPoint(nodeInfo.raftObj, getRaftPort(1), 1);
	CDbHandle::addActiveEndPoint(nodeInfo.raftObj, getRaftPort(2), 1);
	CDbHandle::addActiveEndPoint(nodeInfo.raftObj, getRaftPort(3), 1);
	CDbHandle::addActiveEndPoint(nodeInfo.raftObj, getRaftPort(4), 1);

	CDbHandle::addActiveEndPoint(nodeInfo.bussObj, getBussPort(0), 1);
	CDbHandle::addActiveEndPoint(nodeInfo.bussObj, getBussPort(1), 1);
	CDbHandle::addActiveEndPoint(nodeInfo.bussObj, getBussPort(2), 1);
	CDbHandle::addActiveEndPoint(nodeInfo.bussObj, getBussPort(3), 1);
	CDbHandle::addActiveEndPoint(nodeInfo.bussObj, getBussPort(4), 1);

	refreshRegistry(nodeInfo, 5);

	createServers(5, true);

	startAll();

	waitCluster();

	size_t leaderIndex = waitLeader();
	if(_initFunc)
	{
		_initFunc(get(leaderIndex));
	}
	for (int i = 0; i < 100; i++)
	{
		execBussFunc();
	}

	int noLeaderIndex = *(randNoLeaderIndex(5, leaderIndex, 1).begin());

	//删除一个节点
	CDbHandle::cleanEndPoint();
	for(size_t i = 0; i < 5; i++)
	{
		if(i != noLeaderIndex)
		{
			CDbHandle::addActiveEndPoint(nodeInfo.raftObj, getRaftPort(i), 1);
			CDbHandle::addActiveEndPoint(nodeInfo.bussObj, getBussPort(i), 1);
		}
	}

	refreshRegistry(nodeInfo, 4);

	TC_Common::sleep(1);

	auto config = get(leaderIndex)->node()->getConfiguration();
	ASSERT_TRUE(config.servers.size() == 4);

	for (int i = 0; i < 100; i++)
	{
		execAll([&](const shared_ptr<T> &server){
			if (server->node()->getLocalServer().serverId != get(noLeaderIndex)->node()->getLocalServer().serverId)
			{
				_bussFunc(server);
			}

		});
	}

	stopAll();

	stopFramework(fs);
}

template<typename T>
void RaftTest<T>::testAll()
{
	testBaseProgress();
	testCheckLeaderNotStopLeader();
	testCheckLeaderStopLeader();
	testRestartProgress();
	testLostData();
	testInstallSnapshot();
	testStopLeader();
	testRaftAddNode();
	testRaftDelNode();
	testBenchmark();
	testAutoAddPeer();
	testAutoDelPeer();
}

#endif //RAFT_RAFTTEST_H
