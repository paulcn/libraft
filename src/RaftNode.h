//
// Created by jarod on 2019-06-02.
//

#ifndef LIBRAFT_RAFTNODE_H
#define LIBRAFT_RAFTNODE_H

#include <memory>
#include <string>
#include <mutex>
#include <type_traits>
#include <tuple>
#include <condition_variable>
#include "Raft.h"
#include "RaftOptions.h"
#include "Peer.h"
#include "storage/Snapshot.h"
#include "servant/Application.h"
#include "StateMachine.h"
#include "util/tc_timer.h"

using namespace std;
using namespace raft;

class StateMachine;
class RaftLog;
class Snapshot;
class ApplyContext;
class LogEntry;
class AppendEntriesCallback;
class RaftServantProxyCallback;

/**
 * apply上下文
 * replicate时传入, apply的时候传回来, 业务可以用于回包
 */
class ApplyContext
{
public:
	ApplyContext(const CurrentPtr &current) : _current(current)
	{
	}

	ApplyContext() : _current(NULL)
	{
	}

	virtual ~ApplyContext(){}

	CurrentPtr getCurrentPtr() { return _current; }

	virtual void onAfterApply() {}; //apply调用后调用
protected:
	CurrentPtr   _current;
};

#define RAFT_DEBUG_LOG(raft, msg...) LOGEX_MSG(raft->logger(), LocalRollLogger::DEBUG_LOG, msg)
#define RAFT_ERROR_LOG(raft, msg...) LOGEX_MSG(raft->logger(), LocalRollLogger::ERROR_LOG, msg)

class RaftNode : public TC_Thread, public std::enable_shared_from_this<RaftNode>
{
protected:
	class RaftServantProxyCallback : public ServantProxyCallback
	{
	public:
		RaftServantProxyCallback(const CurrentPtr &current) : _current(current)
		{
		}

	protected:
		virtual int onDispatch(ReqMessagePtr msg)
		{
			_current->sendResponse(msg->response->iRet, *msg->response.get(), msg->response->status, msg->response->sResultDesc);
			return 0;
		}

	protected:
		CurrentPtr _current;
	};

public:

	/**
	 * 节点基本信息
	 */
	struct NodeInfo
	{
        string raftObj;     //raft obj
        string bussObj;     //业务 obj
        TC_Endpoint raftEp;     //本地raft地址
        vector<pair<TC_Endpoint, TC_Endpoint>> nodes;   //绑定定制<raft地址, 业务地址>
	};

	/**
	 * 节点状态
	 */
    enum NodeState
    {
    	STATE_INIT,
        STATE_FOLLOWER,
        STATE_PRE_CANDIDATE,
        STATE_CANDIDATE,
        STATE_LEADER
    };

    static bool containsServer(const Configuration &configuration, uint32_t serverId);
    static Configuration removeServers(const Configuration &configuration, const vector<Server> &servers);

    /**
     * contructor
     * @param stateMachine
     */
    RaftNode(const shared_ptr<StateMachine> &stateMachine);

    /**
     * decontructor
     */
    ~RaftNode();

	/**
	 * init, bind local port
	 * @param eps
	 * @param raftOptions
	 * @param application
	 */
	void init(const RaftOptions &raftOptions, const NodeInfo &nodeInfo, Application *application);

	/**
	 * add command
	 * @param command
	 * @param params
	 * @param result
	 * @return
	 */
	bool cmdConfig(const string &command, const string &params, string &result);

	/**
	 * get curr node raft prx
	 * @return
	 */
	RaftPrx getRaftPrx()
	{
		return Application::getCommunicator()->stringToProxy<RaftPrx>(getPeerObjName(_localServer.serverId));
	}

    /**
     * get leader raft prx
     * @return
     */
    RaftPrx getLeaderPrx()
    {
        return Application::getCommunicator()->stringToProxy<RaftPrx>(getPeerObjName(_leaderId));
    }

    /**
     * 业务leader的地址
     * @return
     */
	string getBussLeaderObj()
	{
		string host = getPeerObjHost(getLeaderId());

		TC_Endpoint ep;
		ep.parse(host);

		std::lock_guard<std::recursive_mutex> lock(_mutex);
		for (auto ni : _nodeInfo.nodes)
		{
			if (ni.first.getHost() == ep.getHost() && ni.first.getPort() == ep.getPort())
			{
				return _nodeInfo.bussObj + "@" + ni.second.toString();
			}
		}

		TLOG_ERROR(host << "not match buss obj endpoint" << endl);

		return "";
	}

	/**
	 * get buss server prx
	 * @tparam T
	 * @return
	 */
	template<typename T>
	T getBussLeaderPrx()
	{
		string obj = getBussLeaderObj();
		if(!obj.empty())
		{
			return Application::getCommunicator()->stringToProxy<T>(obj);
		}

		return NULL;
	}

	/**
	 * 请求转发给leader
	 * @param current
	 */
	bool forwardToLeader(const CurrentPtr &current);

	/**
	 * 如果不是leader则将当前转发给leader
	 * 如果是leader, 则调用build_data_func返回需要复制的数据, 这个数据会同步到多台节点, 并回调onApply, onApply的回调参数就是传输过去的数据
	 * 注意: 如果当前节点都没有加入到集群, 则直接关闭连接了
	 * @param data
	 * @param current
	 */
	void forwardOrReplicate(const CurrentPtr &current, std::function<string()> build_data_func);

	/**
	 * 结束
	 */
    void terminate();

    /**
     * 配置信息
     * @return
     */
    const RaftOptions &getRaftOptions() const { return _raftOptions; }

	/**
	 * 复制节点
	 * @param data
	 * @param callback
	 */
	void replicate(const string &data, const shared_ptr<ApplyContext> &callback);

	/**
	 * 从主控查询到对应的地址, 并且已经排序好, 注意直接查询的主控, 不要频繁调用, 否则对主控产生比较大压力!
	 * @param objName
	 * @return
	 */
	vector<TC_Endpoint> getEndpoints(const string &objName);

    /**
     * 从本地内存中获取集群节点信息
     */
    vector<TC_Endpoint> getNodeList(const string &objName);
    
    /**
     * 获取集群节点信息
     */
    NodeInfo getNodeInfo();

    /*
     * 获取主节点的 servantObj
     * */
    TC_Endpoint getLeaderBussEndpoint();
    
	/**
	 * 状态机信息
	 * @return
	 */
    shared_ptr<StateMachine> getStateMachine();

    /**
     * 获取leader Id
     * @return
     */
    uint32_t getLeaderId();

    /**
     * 是否leader
     * @return
     */
    bool isLeader();

    /**
     * 是否是Follower
     * @return
     */
    bool isFollower();

    /**
     * 是否已经加入到集群可以正常工作了(Leader or Follower)
     *
     * @return
     */
    bool inCluster();

    /**
     * 获取节点状态
     *
     * @return
     */
    NodeState getState();

    /**
     * 获取当前服务的信息
     * @return
     */
    const Server &getLocalServer() const;

    /**
     * local serverId
     */ 
    const string &getLocalServerId() const { return _serverId;  }
    
    /**
     * 获取配置信息
     */
    Configuration getConfiguration();

    /**
     * 获取目前的term
     * @return
     */
    int64_t getCurrentTerm();

    /**
     * 获取最后apply的index
     * @return
     */
	int64_t getLastAppliedIndex() const { return _lastAppliedIndex; }

	/**
	 * 设置最后的apply index
	 * @param index
	 */
	void setLastAppliedIndex(int64_t index);

	/**
	 * 获取serverId对应的endpoint 信息
	 *
	 * @param serverId
	 * @return
	 */
	string getPeerObjHost(uint32_t serverId);

	/**
	 * 获取serverId对应的obj全地址
	 * @param serverId
	 * @return
	 */
	string getPeerObjName(uint32_t serverId);

	/**
	 * 主动生成快照
	 */
	void takeSnapshot();

    /**
     * 获取raft logger
     */
    LocalRollLogger::RollLogger *logger() { return _logger; }

    /**
	 * 根据ep生成serverId
	 * @param ep
	 * @return
	 */
    static uint32_t createServerId(const TC_Endpoint &ep);

protected:
 
	virtual void run();

	//初始化
	void init(const RaftOptions &ro, const vector<Server> &servers, const Server &localServer);

	//复制数据数据, 异步回调, ReplicateCallback只回调一次, 不是每个节点都回调!
	void replicate(const string &data, raft::EntryType type, const shared_ptr<ApplyContext> &callback);

    shared_ptr<Peer> addPeerNoLock(const Server &server, int64_t index);
    shared_ptr<Peer> getPeer(uint32_t serverId);
	shared_ptr<Peer> getPeerNoLock(uint32_t serverId);
	void delPeerNoLock(uint32_t serverId);

	void addNodeInfoNoLock(const Server &server);
	void delNodeInfoNoLock(const Server &server);

//    void erasePeerNotInConfiguration(uint32_t serverId);

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //leader给节点发送请求后收到的响应
    //发送PreVote后, 收到的请求回调
    void callbackPreVote(const shared_ptr<Peer> &peer, const VoteRequest &request, const VoteResponse &response);
    void callbackRequestVote(const shared_ptr<Peer> &peer, const VoteRequest &request, const raft::VoteResponse &response);

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //leader过来的请求
    //leader过来的同步日志的请求
    AppendEntriesResponse onAppendEntries(const AppendEntriesRequest & request);
    //leader过来安装快照的请求
    InstallSnapshotResponse onInstallSnapshot(const InstallSnapshotRequest & request);
    //leader过来的preVote请求
    VoteResponse onPreVote(const VoteRequest & request);
    //leader过来的requestVote请求
    VoteResponse onRequestVote(const VoteRequest & request);

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //其他请求查询配置请求

    //节点复制的回调
    class PeersReplicateCallback : public ApplyContext
    {
    public:
        PeersReplicateCallback(const CurrentPtr &current) : ApplyContext(current)
        {
        }

        PeersReplicateCallback() : ApplyContext()
        {
        }

        void response()
        {
            if (getCurrentPtr())
            {
                Raft::async_response_removePeers(getCurrentPtr(), _rsp);
            }
        }

        raft::PeersResponse &rsp() { return _rsp; }

        virtual void onAfterApply()
        {
            rsp().resCode = RES_CODE_SUCCESS;
            rsp().resMsg = "removed server succ";
	        TLOG_DEBUG(_rsp.writeToJsonString() << endl);
            response();
        }

    protected:
        raft::PeersResponse _rsp;
    };

    GetConfigurationResponse onGetConfiguration();
    GetLeaderResponse onGetLeader();
    void onAddPeers(const PeersRequest &request, raft::PeersResponse &rsp);
    void onRemovePeers(const PeersRequest &request, const shared_ptr<PeersReplicateCallback> &callback);
    void clearAddPeers();
    
    /**
     * @brief 检查网络状况，避免网络故障是出现脑裂问题, 工作原理：检查Leader与Peer之间的链接状态，一半以上ok，则认为正常
     * @return true:网络正常，false：网络异常，Leader需要退出
     */
    bool checkClusterNetwork();

    //检查是否需要安装快照, true:需要安装, false:不需要安装
    bool needInstallSnapshot(const shared_ptr<Peer> &peer);

    //获取数据
	bool getAppendEntriesRequest(int64_t nextIndex, shared_ptr<AppendEntriesRequest> &request);

	//同步数据或心跳
    void appendEntriesSucc(const shared_ptr<Peer> &peer, const shared_ptr<AppendEntriesRequest> &request, const AppendEntriesResponse &response);

    int64_t getLastLogTerm();
	int getElectionTimeoutMs();

	//重置选举定时器, 每次心跳的时候都reset一下, 这样保证, 选举定时器不会被触发
    void resetElectionTimer();
    //删除选举定时器
    void removeElectionTimer();

    //设置节点为LeaderId
    void setLeaderId(uint32_t leaderId);

    //降级, 从leader降级为follower
    void stepDown(int64_t newTerm);

    Server getLeaderServer();

    //应用配置
    void applyConfiguration(const LogEntry &logEntry);

    //开始投票
    void startVoteNoLock();
    //变成leader
    void becomeLeader();

    //开始选举
    void startElection();

    void advanceCommitIndexLeader();
    void advanceCommitIndexFollower(const AppendEntriesRequest &request);

    int64_t packEntries(int64_t index, shared_ptr<AppendEntriesRequest> &request);

    void onCatchUp(const shared_ptr<Peer> &peer);

    bool containsServer(const vector<Server> &servers);

    const shared_ptr<Snapshot> &getSnapshot() const { return _snapshot; }

    //检查是否可以做快照, 如果可以返回快照的apply Log的的index和term
    bool checkTakeSnapshot(int64_t &lastAppliedIndex, int64_t &lastAppliedTerm);

    //去掉lastSnapshotIndex之前的Log
    void truncatePrefix(int64_t lastSnapshotIndex);

    //作为follower节点安装快照成功
    void installSnapshotSucc(const SnapshotMetaData &data);

    size_t orderIndex();

    //转换成字符串输出
    string configurationToString(const Configuration &configuration);

    friend class RaftImp;
    friend class Replicator;
    friend class VoteResponseCallback;
    friend class Snapshot;
    friend class Peer;
    friend class InstallSnapshotThread;
    friend class AppendEntriesCallback;

protected:
    RaftOptions                 _raftOptions;

	NodeInfo                    _nodeInfo;

	NodeState                   _state = STATE_INIT;
    Configuration               _configuration;
    Server                      _localServer;
    string                      _serverId;
    LocalRollLogger::RollLogger *_logger = NULL;

    shared_ptr<StateMachine>    _stateMachine;

    std::recursive_mutex        _mutex;

    std::mutex                  _waitMutex;
	std::condition_variable     _cond;

    bool                        _terminate = false;
    unordered_map<uint32_t, shared_ptr<Peer>>  _peers;

    shared_ptr<RaftLog>         _raftLog;
    shared_ptr<Snapshot>        _snapshot;

    TC_Timer                    *_timer = NULL;
    uint64_t                    _electionId;

    int64_t                     _currentTerm;       //服务器最后一次知道的任期号（初始化为 0，持续递增）
    uint32_t                    _votedFor   = 0;   	//当前节点投票给哪个节点的id, 用来判断是否已经投票过
    uint32_t           			_leaderId    = 0;  	//leader节点id
    int64_t                     _commitIndex = 0;   //日志同步到多数节点, 最大的日志索引
    int64_t                     _lastAppliedIndex;  //日志应用到本地状态机, 最大的日志索引
	int64_t 					_resetTime = 0;		//重置的时间

    unordered_map<int64_t, shared_ptr<ApplyContext>> _callbacks;   //请求的回调对象

    Configuration               _oldConfiguration;
    int                         _catchUpNum = 0;
    int                         _requestPeers   = 0;

    int64_t                     _lastOnAppendEntriesTime = 0;

    TC_EpollServer::BindAdapterPtr _bussAdapter;		//业务adapter, 用于控制端口的绑定
};


#endif //LIBRAFT_RAFTNODE_H
