//
// Created by jarod on 2019-06-02.
//

#include "RaftNode.h"
#include "StateMachine.h"
#include "storage/RaftLog.h"
#include "storage/Snapshot.h"
#include "util/tc_common.h"
#include "util/tc_hash_fun.h"
#include "RaftImp.h"
#include <algorithm>
#include "servant/QueryF.h"

using namespace tars;

#define ELECTIONTIMEOUT_INTERVAL (15 + rand()%10)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class VoteResponseCallback : public RaftPrxCallback
{
public:
      VoteResponseCallback(const shared_ptr<RaftNode> &raftNode, const shared_ptr<Peer> &peer, const VoteRequest &req)
          : _raftNode(raftNode), _peer(peer), _request(req)
    {
    }

    virtual void callback_preVote(const raft::VoteResponse &response)
    {
        auto raftNode = _raftNode.lock();
        auto peer = _peer.lock();
        if(raftNode && peer)
        {
            raftNode->callbackPreVote(peer, _request, response);
        }
    }

    virtual void callback_preVote_exception(tars::Int32 ret)
    {
      	auto raftNode = _raftNode.lock();

        auto peer = _peer.lock();
        if(raftNode && peer)
        {
        	RAFT_DEBUG_LOG(raftNode, "callback_preVote_exception VoteGranted false, from server: " << peer->getServer().serverId << ", " << peer->getServer().endPoint << endl);

            peer->setVoteGranted(false);
        }
    }

    virtual void callback_requestVote(const raft::VoteResponse &response)
    {
        auto raftNode = _raftNode.lock();
		auto peer = _peer.lock();
		if(raftNode && peer)
        {
            raftNode->callbackRequestVote(peer, _request, response);
        }
    }

    virtual void callback_requestVote_exception(tars::Int32 ret)
    {
      	auto raftNode = _raftNode.lock();

        auto peer = _peer.lock();
        if(raftNode && peer)
        {
        	RAFT_DEBUG_LOG(raftNode, "callback_requestVote_exception VoteGranted false, from server: " << peer->getServer().serverId << ", " << peer->getServer().endPoint << endl);

            peer->setVoteGranted(false);
        }
    }

  protected:
    weak_ptr<RaftNode> _raftNode;
    weak_ptr<Peer> _peer;
    VoteRequest _request;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RaftNode::callbackPreVote(const shared_ptr<Peer> &peer, const VoteRequest &request, const VoteResponse &response)
{
    RAFT_DEBUG_LOG(this, "from server:" << peer->getServer().endPoint << ", req=" << request.writeToJsonString() << ", rsp=" << response.writeToJsonString() << endl);

    std::lock_guard<std::recursive_mutex> lock(_mutex);

    peer->setVoteGranted(response.voteGranted);

    int64_t currentTerm = _currentTerm;

    //????????????, ?????? ????????????, ????????????
    if (currentTerm != request.term || _state != RaftNode::STATE_PRE_CANDIDATE)
    {
    	RAFT_DEBUG_LOG(this, "ignore preVote currentTerm:" << currentTerm << ", request.term:" << request.term << ", state:" << _state << ", from server:" << peer->getServer().serverId << ", " << peer->getServer().endPoint << endl);
        return;
    }

    if (response.term > currentTerm)
    {
        //???????????????????????????, ?????????????????????
        RAFT_DEBUG_LOG(this, "received pre vote, response term:" << response.term << ", currentTerm:" << currentTerm << ", from server:" << peer->getServer().serverId << ", " << peer->getServer().endPoint << endl);
        stepDown(response.term);
    }
    else
    {
        if (response.voteGranted)
        {
            size_t voteGrantedNum = 1;

            for (auto &server : _configuration.servers)
            {
                if (server.serverId == _localServer.serverId)
                {
                    continue;
                }
                if (getPeerNoLock(server.serverId)->isVoteGranted())
                {
                    voteGrantedNum += 1;
                }
            }

            RAFT_DEBUG_LOG(this, "get pre vote granted from server:" << peer->getServer().serverId << ", " << peer->getServer().endPoint << ", currentTerm:" << currentTerm << ", response.voteGranted:" << response.voteGranted << ", preVote GrantedNum:" << voteGrantedNum  << endl);

            if (voteGrantedNum > _configuration.servers.size() / 2)
            {
                RAFT_DEBUG_LOG(this, "get majority pre vote, start vote, local serverId:" << _localServer.serverId << ", " << _localServer.endPoint << endl);
				startVoteNoLock();
            }
        }
        else
        {
        	RAFT_DEBUG_LOG(this, "pre vote denied from server: " << peer->getServer().serverId << ", " << peer->getServer().endPoint << ", response term:" << response.term << ", currentTerm:" << currentTerm  << endl);
        }
    }
}

void RaftNode::callbackRequestVote(const shared_ptr<Peer> &peer, const VoteRequest &request, const raft::VoteResponse &response)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    peer->setVoteGranted(response.voteGranted);

    int64_t currentTerm = _currentTerm;

    //????????????, ?????? ????????????, ????????????
    if (currentTerm != request.term || _state != RaftNode::STATE_CANDIDATE)
    {
    	RAFT_DEBUG_LOG(this, "ignore requestVote, currentTerm(" << currentTerm << ") != request.term(" << request.term << ") or _state != STATE_CANDIDATE, _state:" << _state << ", from server:" << peer->getServer().serverId << ", " << peer->getServer().endPoint<< endl);
        return;
    }

    if (response.term > currentTerm)
    {
        //???????????????????????????, ?????????????????????
        RAFT_DEBUG_LOG(this, "received request vote response from server:" << peer->getServer().serverId << ", " << peer->getServer().endPoint << " response term:" << response.term << ", currentTerm:" << currentTerm << endl);
        stepDown(response.term);
    }
    else
    {
        if (response.voteGranted)
        {
            size_t voteGrantedNum = 1;

            for (auto &server : _configuration.servers)
            {
                if (server.serverId == _localServer.serverId)
                {
                    continue;
                }
                if (getPeerNoLock(server.serverId)->isVoteGranted())
                {
                    voteGrantedNum += 1;
                }
            }

            RAFT_DEBUG_LOG(this, "get request vote granted from server:" << peer->getServer().serverId << ", " << peer->getServer().endPoint  << " response term:" << response.term << ", currentTerm:" << currentTerm << ", VoteGrantedNum:" << voteGrantedNum << endl);

            if (voteGrantedNum > _configuration.servers.size() / 2)
            {
                RAFT_DEBUG_LOG(this, "get majority becomeLeader, serverId:" << _localServer.serverId << ", " << _localServer.endPoint << endl);
                becomeLeader();
            }
        }
        else
        {
			RAFT_DEBUG_LOG(this, "vote denied from server: " << peer->getServer().serverId << ", " << peer->getServer().endPoint << ", response term:" << response.term << ", currentTerm:" << currentTerm << endl);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//??????

RaftNode::RaftNode(const shared_ptr<StateMachine> &stateMachine) : _stateMachine(stateMachine)
{
    // LOG_CONSOLE_DEBUG << this << endl;
}

RaftNode::~RaftNode()
{
    // LOG_CONSOLE_DEBUG << this << endl;
}

Configuration RaftNode::getConfiguration()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    return _configuration;
}

string RaftNode::getPeerObjHost(uint32_t serverId)
{
	if(serverId == _localServer.serverId)
	{
		return _localServer.endPoint;
	}
	else
	{
		shared_ptr<Peer> peer = getPeer(serverId);

		return peer->getServer().endPoint;
	}
}

string RaftNode::getPeerObjName(uint32_t serverId)
{
	if(serverId == _localServer.serverId)
	{
		return _localServer.obj + "@" + _localServer.endPoint;
	}
	else
	{
		shared_ptr<Peer> peer = getPeer(serverId);

		return peer->getServer().obj + "@" + peer->getServer().endPoint;
	}
}

const Server &RaftNode::getLocalServer() const
{
    return _localServer;
}

int64_t RaftNode::getCurrentTerm()
{
    return _currentTerm;
}

Server RaftNode::getLeaderServer()
{
    for (auto &server : _configuration.servers)
    {
        if (server.serverId == _leaderId)
        {
            return server;
        }
    }

    assert(false);

    return Server();
}

RaftNode::NodeState RaftNode::getState()
{
    return _state;
}

uint32_t RaftNode::getLeaderId()
{
    return _leaderId;
}

bool RaftNode::isLeader()
{
    return _leaderId == getLocalServer().serverId && _state == STATE_LEADER;
}

bool RaftNode::isFollower()
{
	return _leaderId != 0 && _state == STATE_FOLLOWER && _leaderId != getLocalServer().serverId;
}

bool RaftNode::inCluster()
{
	return _leaderId != 0 && (_state == STATE_FOLLOWER || _state == STATE_LEADER);
}

void RaftNode::setLeaderId(uint32_t leaderId)
{
	uint32_t oldLeaderId = _leaderId;

    _leaderId = leaderId;

    if(oldLeaderId == 0 && leaderId != 0 && (_state == STATE_FOLLOWER || _state == STATE_LEADER))
    {
    	RAFT_DEBUG_LOG(this, "setLeaderId, old leaderId:" << oldLeaderId << ", new leaderId: " << leaderId << ", join cluster, _state:" << _state << endl);
        if(_raftOptions.bindWhenJoinCluster)
        {
            //????????????
            _bussAdapter->manualListen();
        }
    	//??????????????????
		_stateMachine->onJoinCluster();
    }
    else if(oldLeaderId != 0 && leaderId == 0)
    {
    	RAFT_DEBUG_LOG(this, "setLeaderId, old leaderId:" << oldLeaderId << ", new leaderId: " << leaderId << ", leave cluster, _state:" << _state << endl);
        if(_raftOptions.bindWhenJoinCluster)
        {
            //??????????????????
            _bussAdapter->cancelListen();
        }
    	//????????????
	    _stateMachine->onLeaveCluster();
    }
}

shared_ptr<StateMachine> RaftNode::getStateMachine()
{
    return _stateMachine;
}

bool RaftNode::containsServer(const vector<Server> &servers)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    // ??????serverId????????????
    for (auto &server : servers)
    {
        if (!RaftNode::containsServer(_configuration, server.serverId))
        {
            return false;
        }
    }
    return true;
}

shared_ptr<Peer> RaftNode::getPeerNoLock(uint32_t serverId)
{
	auto it = _peers.find(serverId);
	if (it != _peers.end())
	{
		return it->second;
	}

	throw std::logic_error("getPeerNoLock serverId: " + TC_Common::tostr(serverId) + " not exists, peers size:" + TC_Common::tostr(_peers.size()) + ", " + _localServer.endPoint);
}

shared_ptr<Peer> RaftNode::getPeer(uint32_t serverId)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    return getPeerNoLock(serverId);
}

//void RaftNode::erasePeerNotInConfiguration(uint32_t serverId)
//{
//    std::lock_guard<std::recursive_mutex> lock(_mutex);
//    if (!containsServer(_configuration, serverId))
//    {
//        _peers.erase(serverId);
//    }
//}

void RaftNode::setLastAppliedIndex(int64_t index)
{
    _lastAppliedIndex = index;
}

void RaftNode::delPeerNoLock(uint32_t serverId)
{
	shared_ptr<Peer> peer;

    auto it = _peers.find(serverId);
    if (it != _peers.end())
    {
	    peer = it->second;

	    _peers.erase(it);

		RAFT_DEBUG_LOG(this, "erase peer serverId:" << serverId << endl);
    }

    if(peer)
	{
		//??????????????????, ???????????????????????????peer
		std::thread th([=]()
		{
			peer->terminate();

			if (peer->joinable())
			{
				peer->join();
			}
		});

		th.detach();
	}
}

shared_ptr<Peer> RaftNode::addPeerNoLock(const Server &server, int64_t index)
{
    shared_ptr<Peer> peer = std::make_shared<Peer>(server, shared_from_this());
    peer->setNextIndex(index);
    peer->start();
    _peers[server.serverId] = peer;
    
    return peer;
}

bool RaftNode::containsServer(const Configuration &configuration, uint32_t serverId)
{
    for (auto &server : configuration.servers)
    {
        if (server.serverId == serverId)
            return true;
    }

    return false;
}

bool RaftNode::checkTakeSnapshot(int64_t &lastAppliedIndex, int64_t &lastAppliedTerm)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    //applyIndex???log?????????snapshot??????, ????????????snapshot???
    //???????????????, ??????????????????????????????, ??????commitIndex???????????????, ????????????apply????????????, ?????????????????????
    if (_lastAppliedIndex <= _snapshot->getMetaData()->lastIncludedIndex)
    {
        RAFT_DEBUG_LOG(this, "lastAppliedIndex:" << _lastAppliedIndex << " <= snapshot lastIncludedIndex:" << _snapshot->getMetaData()->lastIncludedIndex << endl);
        return false;
    }

    lastAppliedIndex = _lastAppliedIndex;

    //????????????apply????????????????????????log???term
    if (_lastAppliedIndex >= _raftLog->getFirstLogIndex() && _lastAppliedIndex <= _raftLog->getLastLogIndex())
    {
        lastAppliedTerm = _raftLog->getEntryTerm(_lastAppliedIndex);
    }

    RAFT_DEBUG_LOG(this, "firstLogIndex:" << _raftLog->getFirstLogIndex() << ", lastLogIndex:"
                                                     << _raftLog->getLastLogIndex() << ", localLastAppliedIndex:"
                                                     << lastAppliedIndex << endl);

    //	assert(lastAppliedTerm != 0);

    return true;
}

void RaftNode::truncatePrefix(int64_t lastSnapshotIndex)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    //???snapshot?????????index, ???log?????????
    if (lastSnapshotIndex > 0 && _raftLog->getFirstLogIndex() <= lastSnapshotIndex)
    {
        _raftLog->truncatePrefix(lastSnapshotIndex + 1);
    }

    RAFT_DEBUG_LOG(this, "firstLogIndex:" << _raftLog->getFirstLogIndex() << ", lastLogIndex:" << _raftLog->getLastLogIndex() << endl);
}

void RaftNode::installSnapshotSucc(const SnapshotMetaData &snapshotMetaData)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    //???????????????meta??????
    _raftLog->updateSnapshotMetaData(snapshotMetaData.lastIncludedIndex, snapshotMetaData.lastIncludedTerm, snapshotMetaData.configuration);

    //??????Log
    _raftLog->reset(snapshotMetaData.lastIncludedIndex + 1);

    //?????????????????????applyIndex
    setLastAppliedIndex(snapshotMetaData.lastIncludedIndex);
}

void RaftNode::startElection()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (!containsServer(_configuration, _localServer.serverId))
    {
//        assert(false);
        resetElectionTimer();
        return;
    }
//    RAFT_DEBUG_LOG(this, "running pre vote currentTerm: " << _currentTerm << ", last time:" << _resetTime << ", diff:" << (TC_Common::now2ms() - _resetTime)
//    	<< ", _electionId:" << _electionId << ", exists:" << _timer->exist(_electionId) << ", timer count: " << _timer->count()<< endl);

//    if(_state == STATE_PRE_CANDIDATE || _state == STATE_CANDIDATE)
//    {
//    	RAFT_DEBUG_LOG(this, "running _state is STATE_PRE_CANDIDATE or STATE_CANDIDATE, not startElection, _state:" << _state << endl);
//    	resetElectionTimer();
//    	return;
//    }

    _state = STATE_PRE_CANDIDATE;

    shared_ptr<RaftNode> raftNode = shared_from_this();

    for (Server &server : _configuration.servers)
    {
        if (server.serverId == _localServer.serverId)
        {
            continue;
        }

        shared_ptr<Peer> peer = getPeerNoLock(server.serverId);

        peer->setVoteGranted(false);

        RAFT_DEBUG_LOG(this, "currentTerm: " << _currentTerm << ", send pre vote to serverId:" << server.obj << "@" << server.endPoint << endl);

        VoteRequest requestBuilder;

        requestBuilder.serverId     = _localServer.serverId;
        requestBuilder.term         = _currentTerm;
        requestBuilder.lastLogIndex = _raftLog->getLastLogIndex();
        requestBuilder.lastLogTerm  = getLastLogTerm();

        RaftPrxCallbackPtr callback = new VoteResponseCallback(raftNode, peer, requestBuilder);

        peer->async_preVote(requestBuilder, callback);
    }
    if(_stateMachine)
    {
        _stateMachine->onStartElection(_currentTerm);
    }
    resetElectionTimer();
}

void RaftNode::terminate()
{
	{
		std::unique_lock<std::mutex> lock(_waitMutex);

		_terminate = true;

		_cond.notify_all();
	}

    if (this->joinable())
    {
		getThreadControl().join();
	}

	if (_timer)
	{
		_timer->stopTimer();
	}

    unordered_map<uint32_t, shared_ptr<Peer>> peers;
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);

        peers = _peers;

        _peers.clear();
    }

    for (auto e : peers)
    {
        e.second->terminate();

        if (e.second->joinable())
        {
            e.second->join();
        }
	}

    if (_raftLog)
    {
        _raftLog->close();
        _raftLog.reset();
    }
    
    if (_timer)
	{
		delete _timer;
		_timer = NULL;
	}

}

size_t RaftNode::orderIndex()
{
	srand(TC_Common::now2ms());
	return rand() % (_configuration.servers.size() * 3);
}

void RaftNode::init(const RaftOptions &raftOptions, const NodeInfo &nodeInfo, Application *application)
{
	if(nodeInfo.nodes.size() < 3)
	{
		TLOG_ERROR("cluster size is:" << nodeInfo.nodes.size() << ", must > 3" << endl);
        throw std::logic_error("cluster size is:" + TC_Common::tostr(nodeInfo.nodes.size())  + string(", must > 3") );
    }
    
    //????????????
    map<string, TC_Endpoint> mapEpsBus;
	vector<TC_Endpoint> eps;
	for(auto ni : nodeInfo.nodes)
	{
		eps.push_back(ni.first);
        string hostKey = ni.first.toString();
        mapEpsBus[hostKey] =ni.second;
	}
    
    std::sort(eps.begin(), eps.end(), [](const TC_Endpoint&e1, const TC_Endpoint &e2){
		if(e1.getHost() != e2.getHost())
			return e1.getHost() < e2.getHost();

		return e1.getPort() < e2.getPort();
	});

	uint32_t localIndex = -1;

	Server localServer;
	vector<Server> servers;
	TLOG_DEBUG("cluster server ip list [" << nodeInfo.nodes.size() << "]:" << endl);

	for(size_t i = 0; i < eps.size(); i++)
	{
	    TC_Endpoint epsBuss = mapEpsBus[eps[i].toString()];
		TLOG_DEBUG("cluster index:" << i << ", ip:" << eps[i].toString() << ", buss:" << epsBuss.toString() << endl);

		uint32_t serverId = createServerId(eps[i]);
		if(serverId == 0)
		{
			TLOG_ERROR("cluster index:" << i << ", ip:" << eps[i].toString() << ", serverId:" << serverId << ", exit!!" << endl);
            throw std::logic_error("cluster index:" + TC_Common::tostr(i)+  ", ip:" +  eps[i].toString() + ", serverId:" + TC_Common::tostr(serverId) +  ", exit!!" );
		}

		Server server;
		server.serverId = serverId;
		server.endPoint = eps[i].toString();
		server.obj      = nodeInfo.raftObj;
		server.bussEndPoint = epsBuss.toString();
		server.bussObj  = nodeInfo.bussObj;

		servers.push_back(server);

//		LOG_CONSOLE_DEBUG << "cluster index:" << i << ", ep:" << eps[i].toString() << ", node:" << nodeInfo.raftEp.toString() << endl;

		if(eps[i].getHost() == nodeInfo.raftEp.getHost() && eps[i].getPort() == nodeInfo.raftEp.getPort())
		{
			localIndex = server.serverId;
			localServer = server;
		}
	}

	if(localIndex == (uint32_t)-1)
	{
		TLOG_ERROR("no local server:" << nodeInfo.raftEp.toString() << ", server list size: " << eps.size() << ", exit." << endl);
		throw std::logic_error("no local server:" + nodeInfo.raftEp.toString() + ", server list size: " + TC_Common::tostr(eps.size()) + ", exit.");
	}

    _serverId = TC_Common::tostr(localServer.serverId);
    _logger = LocalRollLogger::getInstance()->logger(_serverId + ".0");

    RAFT_DEBUG_LOG(this, "----------------------" << _serverId << "----------------------------------------------" << endl);
    for (size_t i = 0; i < servers.size(); i++)
    {
        TLOG_DEBUG("cluster index:" << i << ", serverId:" << servers[i].serverId << ", endPoint:" << servers[i].endPoint << ", bussEndPoint:" << servers[i].bussEndPoint << endl);
    }

    RAFT_DEBUG_LOG(this, "local server:" << nodeInfo.raftEp.toString() << ", index:" << localIndex << endl);

	_nodeInfo = nodeInfo;

	_bussAdapter = application->getEpollServer()->getBindAdapter(application->getServantHelper()->getServantAdapter(_nodeInfo.bussObj));

    if(raftOptions.bindWhenJoinCluster)
    {
	    _bussAdapter->enableManualListen();
    }

	init(raftOptions, servers, localServer);

	shared_ptr<RaftNode> raftNode = shared_from_this();

	application->addServantWithParams<RaftImp>(nodeInfo.raftObj, raftNode);

	application->addAdminCommandPrefix("raft", std::bind(&RaftNode::cmdConfig, raftNode, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void RaftNode::init(const RaftOptions &ro, const vector<Server> &servers, const Server &localServer)//, StateMachine *stateMachine)
{
    _raftOptions = ro;

    _configuration.servers = servers;
    _localServer           = localServer;

    _raftLog = std::make_shared<RaftLog>(this);
    _raftLog->initialize(_raftOptions);

    _snapshot = std::make_shared<Snapshot>(this, ro.dataDir, _raftLog);

    _currentTerm = _raftLog->getLogMetaData()->currentTerm;
    _votedFor    = _raftLog->getLogMetaData()->votedFor;
//    _commitIndex = max(_snapshot->getMetaData()->lastIncludedIndex, _commitIndex);

	if (_snapshot->getMetaData()->lastIncludedIndex > 0 && _raftLog->getFirstLogIndex() <= _snapshot->getMetaData()->lastIncludedIndex)
    {
        //??????????????????????????????, ??????binlog????????????
        _raftLog->truncatePrefix(_snapshot->getMetaData()->lastIncludedIndex + 1);
    }

    Configuration snapshotConfiguration = _snapshot->getMetaData()->configuration;
    if (!snapshotConfiguration.servers.empty())
    {
        _configuration = snapshotConfiguration;
    }

    //????????????, ???????????????apply index
	_lastAppliedIndex = _stateMachine->onLoadData();

	_commitIndex = _lastAppliedIndex;

    //?????????????????????, ????????????????????????index???
    for (auto &server : _configuration.servers)
    {
        if (_peers.find(server.serverId) == _peers.end() && server.serverId != _localServer.serverId)
        {
            //??????????????????, ??????????????????????????????
			addPeerNoLock(server, _raftLog->getLastLogIndex() + 1);
        }
    }

    _timer = new TC_Timer();

    _timer->startTimer(1);

    int64_t id = ((int64_t)_localServer.serverId) + time_t(NULL);

	srand(id/1000000);

	size_t index = orderIndex();

    //????????????
	_electionId = _timer->postDelayed(index * ELECTIONTIMEOUT_INTERVAL, std::bind(&RaftNode::startElection, this));

    //?????????????????????
    _timer->postRepeated(_raftOptions.snapshotPeriodSeconds * 1000, false, std::bind(&Snapshot::takeSnapshot, _snapshot.get()));

    if (!Application::getCommunicator()->getProperty("locator").empty())
    {
        start();
    }

    RAFT_DEBUG_LOG(this, "succ, orderIndex:" << index << endl);
}

vector<TC_Endpoint> RaftNode::getEndpoints(const string &objName)
{
	vector<TC_Endpoint> eps;

    if(!Application::getCommunicator()->getProperty("locator").empty())
	{
    	QueryFPrx queryFPrx = Application::getCommunicator()->stringToProxy<QueryFPrx>(Application::getCommunicator()->getProperty("locator"));

//		ServantPrx prx = Application::getCommunicator()->stringToProxy<ServantPrx>(objName);

        vector<EndpointF> eps1;
        vector<EndpointF> eps2;
        queryFPrx->findObjectById4All(objName, eps1, eps2);

        for_each(eps1.begin(), eps1.end(), [&](const EndpointF & e)
		{
        	TC_Endpoint ep;
        	ep.setHost(e.host);
        	ep.setPort(e.port);
        	ep.setType((TC_Endpoint::EType)e.istcp);
        	ep.setTimeout(e.timeout);

			eps.push_back(ep);
		});
        for_each(eps2.begin(), eps2.end(), [&](const EndpointF & e)
		{
        	TC_Endpoint ep;
        	ep.setHost(e.host);
        	ep.setPort(e.port);
        	ep.setType((TC_Endpoint::EType)e.istcp);
			ep.setTimeout(e.timeout);

        	eps.push_back(ep);
		});
	}

	return eps;
}


vector<TC_Endpoint> RaftNode::getNodeList(const string &objName)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    bool isRaft = (objName == _nodeInfo.raftObj);
    bool isBuss = (objName == _nodeInfo.bussObj);
    vector<TC_Endpoint> res;
    if(!isRaft && !isBuss)
    {
        return res;
    }
    for(auto node : _nodeInfo.nodes)
    {
        if(isRaft) res.push_back(node.first);
        else res.push_back(node.second);        
    }
    return res;
}

void RaftNode::addNodeInfoNoLock(const Server &server )
{
    TC_Endpoint ep(server.endPoint);
    for (auto & item :_nodeInfo.nodes )
    {
        if (item.first.getHost() == ep.getHost() && item.first.getPort() == ep.getPort()  )
        {
            TLOG_INFO("add peer repeat:" << server << endl);
            return;
        }
    }
    
    //??????nodeinfo????????????
    _nodeInfo.nodes.push_back(std::make_pair(TC_Endpoint(server.endPoint), TC_Endpoint(server.bussEndPoint)));
    TLOG_INFO("nodes info add peer :" << server.endPoint  << "|nodes size:" << _nodeInfo.nodes.size()<< endl);
}

void RaftNode::delNodeInfoNoLock(const Server &server)
{
    //??????node??????????????????
    bool found = false;
    {
        TC_Endpoint ep(server.endPoint);
        for (auto it = _nodeInfo.nodes.begin(); it != _nodeInfo.nodes.end(); it ++)
        {
            if (it->first.getHost() == ep.getHost() && it->first.getPort() == ep.getPort() )
            {
                TLOG_INFO("nodes info erase peer:" << ep.toString() << "|nodes size:" << _nodeInfo.nodes.size() << endl);
                _nodeInfo.nodes.erase(it);
                found = true;
                break;
            }
        }
    }
    if (!found)
    {
        TLOG_INFO("del peer not found:" << server << endl);
    }
}

RaftNode::NodeInfo RaftNode::getNodeInfo()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    return _nodeInfo;
}

TC_Endpoint RaftNode::getLeaderBussEndpoint()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    string host = getPeerObjHost(getLeaderId());
    
    TC_Endpoint ep;
    ep.parse(host);
    
    for (auto & ni : _nodeInfo.nodes)
    {
        if (ni.first.getHost() == ep.getHost() && ni.first.getPort() == ep.getPort())
        {
            return ni.second;
        }
    }
    RAFT_ERROR_LOG(this, host << "not match leader buss obj endpoint" << endl);
    
    return TC_Endpoint();
}

void RaftNode::replicate(const string &data, const shared_ptr<ApplyContext> &callback)
{
	replicate(data, ENTRY_TYPE_DATA, callback);
}

void RaftNode::replicate(const string &data, EntryType type, const shared_ptr<ApplyContext> &callback)
{
	if (_state != STATE_LEADER)
	{
		return;
	}

    std::unique_lock<std::recursive_mutex> lock(_mutex);

    shared_ptr<LogEntry> logEntry = std::make_shared<LogEntry>(_currentTerm, type, data);

	int64_t newLastLogIndex = _raftLog->append(logEntry);

    _callbacks[newLastLogIndex] = callback;

	//????????????, ??????, peer??????
	for(auto e : _peers)
	{
		e.second->notify();
	}
}

bool RaftNode::cmdConfig(const string &command, const string &params, string &result)
{
	result = getConfiguration().writeToJsonString();

	return true;
}

bool RaftNode::forwardToLeader(const CurrentPtr &current)
{
    if(!inCluster())
    {
        TLOGEX_ERROR(_serverId,"cluster not ready! discard forward" << endl);
        return true;
    }
    if(isLeader())
    {
        return false;
    }
	else if(isFollower() && getLeaderId() != _localServer.serverId)
	{
		current->setResponse(false);
        ServantPrx leaderPrx = getBussLeaderPrx<ServantPrx>();
        if(leaderPrx)
        {
            ServantProxyCallbackPtr callback = new RaftServantProxyCallback(current);
            
            map<std::string, std::string> context = current->getResponseContext();
            //???????????????????????????
            context[FORWARD_FLAG] = "1";

            leaderPrx->tars_invoke_async(current->getPacketType(),
                                                            current->getFuncName(),
                                                            current->getRequestBuffer(),
                                                            context,
                                                            current->getRequestStatus(),
                                                            callback);
            return true;
        }
	}
	
    TLOGEX_ERROR(_serverId,"cluster not right! leaderid=" << getLeaderId() << ",localid=" << getLocalServer().serverId << endl);
    current->close();
    return true;
}

void RaftNode::forwardOrReplicate(const CurrentPtr &current, std::function<string()> build_data_func)
{
	if(this->isLeader())
	{
		//?????????, ???????????????????????????????????????
		current->setResponse(false);

		shared_ptr<ApplyContext> callback = std::make_shared<ApplyContext>(current);

		//?????????????????????
		replicate(build_data_func(), callback);
	}
	else if(this->isFollower())
	{
		forwardToLeader(current);
	}
	else
	{
		current->close();
	}
}

uint32_t RaftNode::createServerId(const TC_Endpoint &ep)
{
	uint32_t serverId = tars::hash<string>()(ep.getHost() + ":" + TC_Common::tostr(ep.getPort()));

    TLOG_DEBUG("serverId: " << serverId << ", ep:" << ep.toString() << endl);

    return serverId;
}

void RaftNode::takeSnapshot()
{
	_snapshot->takeSnapshot();
}

int64_t RaftNode::packEntries(int64_t nextIndex, shared_ptr<AppendEntriesRequest> &requestBuilder)
{
    //????????????????????????????????????index
    int64_t lastIndex = min(_raftLog->getLastLogIndex(), nextIndex + _raftOptions.maxLogEntriesPerRequest - 1);

	_raftLog->getEntry(nextIndex, lastIndex, requestBuilder->entries);

    return lastIndex - nextIndex + 1;
}

//leader????????????/??????, ??????
void RaftNode::appendEntriesSucc(const shared_ptr<Peer> &peer, const shared_ptr<AppendEntriesRequest> &request, const raft::AppendEntriesResponse &response)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (response.term > _currentTerm)
    {
        //???????????????term?????????, ??????????????????leader???, ?????????leader???????????????????????????????????????, ???????????????????????????????????????leader
        RAFT_DEBUG_LOG(this, "response.term:" << response.term << "> _currentTerm:" << _currentTerm << ", from server:" << peer->getServer().serverId << ", " << peer->getServer().endPoint << endl);

        stepDown(response.term);
    }
    else
    {
        if (response.resCode == RES_CODE_SUCCESS)
        {
            //???????????????????????????log Index
            peer->setMatchIndex(request->prevLogIndex + request->entries.size());
            //??????????????????????????????log index
            peer->setNextIndex(peer->getMatchIndex() + 1);

            if (containsServer(_configuration, peer->getServer().serverId))
            {
				peer->setStatus(PS_NORMAL);

                //??????????????????, ?????????????????????????????????, ???????????????????????????????????????, ??????commitIndex
                advanceCommitIndexLeader();

            }
            else
            {
                //?????????????????????, ???????????????????????????, ????????????????????????, ????????????????????????????????????
                if (_raftLog->getLastLogIndex() - peer->getMatchIndex() <= _raftOptions.catchupMargin)
                {
                    RAFT_DEBUG_LOG(this, "peer catch up the leader, local serverId: " << _localServer.endPoint << ", from server:" << peer->getServer().endPoint << endl);
                    onCatchUp(peer);
                }
            }
        }
        else
        {
            //????????????gap???, ??????nextIndex, ??????????????????Log?????????response.lastLogIndex + 1??????
            peer->setNextIndex(response.lastLogIndex + 1);

            RAFT_DEBUG_LOG(this, "has gap, current leader: " << _localServer.endPoint << ", from server:" << peer->getServer().endPoint << ", peer nextIndex:" << peer->getNextIndex() << endl);
        }
    }

}

bool RaftNode::needInstallSnapshot(const shared_ptr<Peer> &peer)
{
    std::unique_lock<std::recursive_mutex> lock(_mutex, try_to_lock);

    if(lock.owns_lock())
	{
		//Leader???????????????????????????
		//?????????????????????log Index?????????????????????????????????, ????????????????????????????????????, ?????????????????????????????????????????????????????????, ????????????????????????
		if (peer->getNextIndex() < _raftLog->getFirstLogIndex())
		{
			RAFT_DEBUG_LOG(this,
					"isNeedInstallSnapshot:" << peer->getServer().endPoint << ", nextIndex:" << peer->getNextIndex()
											 << ", firstLogIndex:" << _raftLog->getFirstLogIndex() << endl);

			peer->installSnapshot();

			return true;
		}
	}

    return false;
}

bool RaftNode::getAppendEntriesRequest(int64_t nextIndex, shared_ptr<AppendEntriesRequest> &request)
{
	SnapshotMetaData smd = _snapshot->getMetaDataLock();

	int64_t lastSnapshotIndex = smd.lastIncludedIndex;
	int64_t lastSnapshotTerm  = smd.lastIncludedTerm;

    //??????????????????????????????, ?????????????????????????????????Index???Term???????????????
	std::unique_lock<std::recursive_mutex> lock(_mutex);

	if (nextIndex < _raftLog->getFirstLogIndex())
	{
		//????????????????????????, ????????????firstLogIndex?????????, ???????????????????????????, ??????????????????
		//???????????????, peer????????????????????????????????????
		return false;
	}

	//??????????????????????????????log, term, ?????????????????????, ???log index ??? term ??????0
	int64_t prevLogIndex = nextIndex - 1;

	int64_t prevLogTerm;
	if (prevLogIndex == 0)
	{
		prevLogTerm = 0;
	}
	else if (prevLogIndex == lastSnapshotIndex)
	{
		prevLogTerm = lastSnapshotTerm;
	}
	else
	{
		prevLogTerm = _raftLog->getEntryTerm(prevLogIndex);
	}

	request->serverId = _localServer.serverId;
	request->term = _currentTerm;
	request->prevLogTerm = prevLogTerm;
	request->prevLogIndex = prevLogIndex;

	//????????????????????????log???????????????
	int64_t numEntries = packEntries(nextIndex, request);

	//leader??????commit???Index, ??? ?????????????????????????????????????????????, ??????follower???commitIndex
	request->leaderCommit = min(_commitIndex, prevLogIndex + numEntries);

	return true;

	return false;
}

void RaftNode::stepDown(int64_t newTerm)
{
    if (_currentTerm > newTerm)
    {
        assert(false);
        return;
    }

	bool becomeFollower = false;

	if (_state != STATE_FOLLOWER)
	{
		_state = STATE_FOLLOWER;
		becomeFollower = true;
	}

    //?????????follower
    if (_currentTerm < newTerm)
    {
    	RAFT_DEBUG_LOG(this, "stepDown, _currentTerm:" << _currentTerm << ", newTerm: " << newTerm << endl);

        _currentTerm = newTerm;
	    setLeaderId(0);
        _votedFor    = 0;
        _raftLog->updateLogMetaData(_currentTerm, _votedFor, 0);
    }

    if (becomeFollower)
    {
    	RAFT_DEBUG_LOG(this, "onBecomeFollower" << endl);

        _stateMachine->onBecomeFollower();
    }

    //???????????????, ????????????????????????????????????, ?????????????????????????????????!!!!
    //?????? ???????????? < ??????????????????
    resetElectionTimer();
}

//????????????
void RaftNode::applyConfiguration(const LogEntry &entry)
{
    //??????????????????peer????????????????, ???????????????????????????????????????????????, ??????leader?????????????????????!
    auto log = entry.log();

    TarsInputStream<> is;
	is.setBuffer(log.first, log.second);

	Configuration newConfiguration;
    newConfiguration.readFrom(is);

	RAFT_DEBUG_LOG(this, "configuration:" << this->configurationToString(_configuration) << endl);

	vector<uint32_t> delPeerServerId;
    //?????????????????????peer
    for (auto &server : _configuration.servers)
    {
        bool exist = false;

        for (auto s : newConfiguration.servers)
        {
            if(server.serverId == s.serverId)
            {
                exist = true;
                break;
            }
        }

        if(!exist)
        {
            RAFT_DEBUG_LOG(this, "del server: " << server.endPoint << ", newConfiguration:" << this->configurationToString(newConfiguration) << endl);

            TARS_NOTIFY_NORMAL("del peer:" + server.endPoint);

			delNodeInfoNoLock(server);

			delPeerServerId.push_back(server.serverId);
        }
    }

    //???????????????, ???????????????peer
    _configuration = newConfiguration;

    for(auto serverId : delPeerServerId)
	{
		delPeerNoLock(serverId);
	}
	//????????????????????????peer
    for (auto &server : newConfiguration.servers)
    {
        if (_peers.find(server.serverId) == _peers.end() && server.serverId != _localServer.serverId)
        {
            RAFT_DEBUG_LOG(this, "add peer: " << server.endPoint << endl);

            TARS_NOTIFY_NORMAL("add peer:" + server.endPoint);
			addPeerNoLock(server, _raftLog->getLastLogIndex() + 1);

			addNodeInfoNoLock(server);
        }
    }

    RAFT_DEBUG_LOG(this, "newConfiguration: " << this->configurationToString(newConfiguration) << endl);
}

//??????????????????log?????????term
int64_t RaftNode::getLastLogTerm()
{
    int64_t lastLogIndex = _raftLog->getLastLogIndex();

    if (lastLogIndex >= _raftLog->getFirstLogIndex())
    {
        return _raftLog->getEntryTerm(lastLogIndex);
    }
    else
    {
        // log????????????, log?????????snapshot???, lastLogIndex = 0, firstLogIndex=1???
        return _snapshot->getMetaData()->lastIncludedTerm;
    }
}

//???????????????????????????????????????
int RaftNode::getElectionTimeoutMs()
{
    int randomElectionTimeout = _raftOptions.electionTimeoutMilliseconds + ELECTIONTIMEOUT_INTERVAL * orderIndex();

	return randomElectionTimeout;
}

//?????????????????????, ???????????????, ????????????
void RaftNode::resetElectionTimer()
{
	_resetTime = tars::TC_Common::now2ms();

    removeElectionTimer();

//    if(_timer->count() > 1)
//    {
//    	RAFT_DEBUG_LOG(this, "resetElectionTimer timer not empty, must have bug!!!! count:" << _timer->count() << ", _electionId:" << _electionId << endl );
//    }

    _electionId = _timer->postDelayed(getElectionTimeoutMs(), std::bind(&RaftNode::startElection, this));
}

void RaftNode::removeElectionTimer()
{
    _timer->erase(_electionId);
}

void RaftNode::startVoteNoLock()
{
    //????????????
    if (!containsServer(_configuration, _localServer.serverId))
    {
        assert(false);
        resetElectionTimer();
        return;
    }

    //??????????????????, term??????
    _currentTerm++;

    _state    = STATE_CANDIDATE;
	setLeaderId(0);
    _votedFor = _localServer.serverId;

	RAFT_DEBUG_LOG(this, "running for election in term: " << _currentTerm << ", voteFor: " << _localServer.serverId << endl);

    shared_ptr<RaftNode> raftNode = shared_from_this();

    for (Server &server : _configuration.servers)
    {
        if (server.serverId == _localServer.serverId)
        {
            continue;
        }

        shared_ptr<Peer> peer = getPeerNoLock(server.serverId);

        RAFT_DEBUG_LOG(this, "currentTerm: " << _currentTerm << ", send vote to serverId:" << server.obj << "@" << server.endPoint << endl);

        VoteRequest requestBuilder;

        peer->setVoteGranted(false);

        requestBuilder.serverId     = _localServer.serverId;
        requestBuilder.term         = _currentTerm;
        requestBuilder.lastLogIndex = _raftLog->getLastLogIndex();
        requestBuilder.lastLogTerm  = getLastLogTerm();

        peer->async_requestVote(requestBuilder, new VoteResponseCallback(raftNode, peer, requestBuilder));
    }

    resetElectionTimer();
}

void RaftNode::run()
{
	while(!_terminate)
	{
		try
		{
			if(this->isLeader())
			{
				// RAFT_DEBUG_LOG(this, "----------------------------------------------------------------------" << endl);

				const vector<TC_Endpoint> allEndpoint = getEndpoints(_nodeInfo.raftObj);
                if (allEndpoint.empty())
                {
                    break;
                }

				//????????????
                map<string, TC_Endpoint> mapEpsBus;
                const vector<TC_Endpoint> allEndpointBuss = getEndpoints(_nodeInfo.bussObj);
                for(size_t i = 0; i < allEndpointBuss.size(); i++)
                {
                    string host = allEndpointBuss[i].getHost();
                    mapEpsBus[host] = allEndpointBuss[i];
                }
                
                for (auto & item : allEndpoint)
                {
					// RAFT_DEBUG_LOG(this, item.toString() << endl);

                    if (mapEpsBus.find(item.getHost()) == mapEpsBus.end())
                    {
                        TLOGEX_ERROR(_serverId,"raft host not match buss host :" << item.toString() <<endl);
                        break;
                    }
                }
                
//                RAFT_DEBUG_LOG(this, "total eps size:" << allEndpoint.size() << endl);

                std::vector<raft::Server> servers;

                //??????????????????????????????
                {
                    std::unique_lock<std::recursive_mutex> lock(_mutex);
                    servers = _configuration.servers;
                }

//				RAFT_DEBUG_LOG(this, "now servers:" << this->configurationToString(_configuration) << endl);

                {
                    vector<TC_Endpoint> eps = allEndpoint;

                    for (auto s : servers)
                    {
                        TC_Endpoint ep;
                        ep.parse(s.endPoint);

                        for(size_t i = 0; i < eps.size(); i++)
                        {
                            //???????????????
                            if(ep.getHost() == eps[i].getHost() && ep.getPort() == eps[i].getPort())
                            {
                                eps.erase(eps.begin() + i);
                                --i;
                                break;
                            }
                        }
                    }

//                    RAFT_DEBUG_LOG(this, "add server count:" << eps.size() << endl);

                    if(!eps.empty())
                    {
                        //???????????????????????????
                        PeersRequest request;

                        for(auto e : eps)
                        {
							RAFT_DEBUG_LOG(this, "add server info:" << e.toString() << endl);

                            Server s;
                            s.obj = _nodeInfo.raftObj;
                            s.serverId = createServerId(e);//taf::hash<string>()(e.getHost() + ":" + TC_Common::tostr(e.getPort()));
                            s.endPoint = e.toString();
                            
                            s.bussObj = _nodeInfo.bussObj;
                            s.bussEndPoint = mapEpsBus[e.getHost()].toString();
                            
                            request.servers.push_back(s);
                        }

                        PeersResponse rsp;

                        onAddPeers(request, rsp);

//                        RAFT_DEBUG_LOG(this, "onAddPeers: " << request.writeToJsonString() << endl);
                    }
                }

                //??????????????????????????????
                {
                    vector<TC_Endpoint> eps = allEndpoint;
                    
                    PeersRequest request;
                    request.servers = servers;

                    for (auto s : eps)
                    {
                        for (size_t i = 0; i < request.servers.size(); i++)
                        {
                            TC_Endpoint ep;
                            ep.parse(request.servers[i].endPoint);

                            // RAFT_DEBUG_LOG(this, "check erase servers :" << request.servers[i].endPoint << ", " << s.toString() << endl);

                            //???????????????
                            if (ep.getHost() == s.getHost() && ep.getPort() == s.getPort())
                            {
                                request.servers.erase(request.servers.begin() + i);
                                --i;
                                break;
                            }
                        }
                    }

                    if (!request.servers.empty())
                    {
                        shared_ptr<RaftNode::PeersReplicateCallback> callback = std::make_shared<RaftNode::PeersReplicateCallback>();

                        onRemovePeers(request, callback);

                        RAFT_DEBUG_LOG(this, "onRemovePeers: " << request.writeToJsonString() << endl);
                    }
                }
                //??????????????????????????????????????????
                {
                    std::lock_guard<std::recursive_mutex> lock(_mutex);
                    if(false == checkClusterNetwork())
                    {
                        RAFT_DEBUG_LOG(this, "network failed, leader stepDown " << endl);
                        stepDown(_currentTerm+1);
                    }
                }
            }
            std::unique_lock<std::mutex> lock(_waitMutex);
            _cond.wait_for(lock, std::chrono::milliseconds(1000));
		}
		catch(exception &ex)
		{
			RAFT_DEBUG_LOG(this, "run error " << ex.what() << endl);
		}
	}
}

void RaftNode::becomeLeader()
{
    //??????leader
    _state    = STATE_LEADER;

    setLeaderId(_localServer.serverId);

    RAFT_DEBUG_LOG(this, "term: " << _currentTerm << ", _leaderId:" << _leaderId << ", obj:" << _localServer.endPoint << endl);

    TARS_NOTIFY_NORMAL("leader:" + _localServer.endPoint);

	_stateMachine->onBecomeLeader(_currentTerm);

    for (auto e : _peers)
    {
        e.second->setNextIndex(_raftLog->getLastLogIndex() + 1);
        e.second->setMatchIndex(0);
	    //??????peer??????, ??????????????????!
        e.second->notify();
    }

    //?????????????????????
    removeElectionTimer();

    //??????, ??????Leader???, ?????????????????????: _lastAppliedIndex???_commitIndex??????????????????
    //?????????????????????, ????????????, ?????????Follower, ?????????????????????????????????, ????????????_lastAppliedIndex???_commitIndex?????????
    //??????: https://www.jianshu.com/p/ddbe4209be0f
}

AppendEntriesResponse RaftNode::onAppendEntries(const AppendEntriesRequest &request)
{
    raft::AppendEntriesResponse rsp;

    int64_t ms = TC_Common::now2ms();

    std::lock_guard<std::recursive_mutex> lock(_mutex);

//    int64_t lockendTime = TC_Common::now2ms();

    rsp.term         = _currentTerm;
    rsp.resCode      = RES_CODE_FAIL;
    rsp.lastLogIndex = _raftLog->getLastLogIndex();

    //???????????????leader??????????????????, ????????????
    if (request.term < _currentTerm)
    {
	    RAFT_DEBUG_LOG(this, "request.term < _currentTerm" << endl);

        return rsp;
    }

    if (_snapshot->getIsInstallSnapshot())
    {
        RAFT_DEBUG_LOG(this, "installing snapshot." << endl);
        return rsp;
    }

    //??????????????????????????????, ?????????????????????
    stepDown(request.term);

    if (_leaderId == 0)
    {
        setLeaderId(request.serverId);
        RAFT_DEBUG_LOG(this, "new leaderId:" << _leaderId  << endl);
    }

    //?????????leader, ??????????????????leader???????????????, ???????????????????????????(Leader???????????????????)
    if (_leaderId != request.serverId)
    {
        RAFT_DEBUG_LOG(this, "another peer:" << request.serverId << " declares that it is the leader at term:"
                                                            << request.term << ", which was occupied by leader:" << _leaderId << endl);

        //????????????, ?????????????????????, term+1
        stepDown(request.term + 1);
        rsp.resCode = RES_CODE_FAIL;
        rsp.term    = request.term + 1;
        return rsp;
    }

    //if (!request.entries.empty() && TNOW - _lastOnAppendEntriesTime > 1)
    if (TNOW - _lastOnAppendEntriesTime > 1)
    {
        //???????????????????????????, ??????????????????
        _lastOnAppendEntriesTime = TNOW;

        RAFT_DEBUG_LOG(this, "request FROM server:" << request.serverId << " in term:" << request.term
                                                                << ", my term:" << _currentTerm << ", size:" << request.entries.size() << ", entry range:"
                                                                << request.prevLogIndex+1 << " - " << request.prevLogIndex + request.entries.size()
                                                                << ", cost:" << TC_Common::now2ms() - ms << endl);
    }

    //??????????????????gap???, ???????????????????????????, ????????????, Leader?????????nextIndex
    if (request.prevLogIndex > _raftLog->getLastLogIndex())
    {
        RAFT_DEBUG_LOG(this, "request size:" << request.entries.size() << ", prevLogIndex:" << request.prevLogIndex << " > lastLogIndex:" << _raftLog->getLastLogIndex() << ", has gap, notify leader to resend data!" << endl);
        return rsp;
    }

    //???????????????, ????????????????????????, ????????????, ????????????Log??????????????????
    //??????: Leader A????????????, ????????????Log???, ??????, ???????????????????????????Leader, ??????????????????, ????????????????????????term????????????, ??????????????????????????????commit???
    //???????????????????????????, ????????????Leader???????????????????????????

    if (request.prevLogIndex >= _raftLog->getFirstLogIndex() && _raftLog->getEntryTerm(request.prevLogIndex) != request.prevLogTerm)
    {
        RAFT_DEBUG_LOG(this, "move prev prevLogIndex:" << request.prevLogIndex << ", lastLogIndex:" << _raftLog->getLastLogIndex() << endl);
        assert(request.prevLogIndex > 0);
        rsp.lastLogIndex = request.prevLogIndex - 1;
        return rsp;
    }

    //log entry??????, ????????????
    if (request.entries.empty())
    {
        rsp.resCode      = RES_CODE_SUCCESS;
        rsp.term         = _currentTerm;
        rsp.lastLogIndex = _raftLog->getLastLogIndex();

        advanceCommitIndexFollower(request);

        return rsp;
    }
    else
    {
        rsp.resCode = RES_CODE_SUCCESS;

        list<shared_ptr<LogEntry>> entries;

        int64_t index = request.prevLogIndex;

        for (auto &value : request.entries)
        {
            index++;
            if (index < _raftLog->getFirstLogIndex())
            {
                //Log???????????????????????????, ???????????????, ???????????????
                RAFT_DEBUG_LOG(this, "continue, index:" << index << ", firstLogIndex:" << _raftLog->getLastLogIndex() << endl);
                continue;
            }

	        shared_ptr<LogEntry> entry = std::make_shared<LogEntry>(index, value);

            //?????????leader or follower, ?????????log, ???????????????commit?????????, ???????????????
            //????????????, ??????log index ??? term?????????, ?????????????????????????????????
            if (_raftLog->getLastLogIndex() >= index)
            {
                if (_raftLog->getEntryTerm(index) == entry->term())
                {
                    //???????????????????????????, ????????????????????????, ???????????????????????????(?????????????????????)
                    continue;
                }
                // ?????????????????????????????????
                _raftLog->truncateSuffix(index - 1);
            }
            entries.push_back(entry);
        }
        rsp.lastLogIndex = _raftLog->append(entries);

        advanceCommitIndexFollower(request);

//        RAFT_DEBUG_LOG(this, "serverId: " << request.serverId << ", entries size: " << request.entries.size() << ", cost:" << TNOWMS - ms << endl);
        return rsp;
    }
}

void RaftNode::advanceCommitIndexFollower(const AppendEntriesRequest &request)
{
    //leader??????commit?????????????????????????????????Index, ??????follower???commitIndex
    int64_t newCommitIndex = std::min((int64_t)request.leaderCommit, (int64_t)(request.prevLogIndex + request.entries.size()));

    _commitIndex = newCommitIndex;

    if (_lastAppliedIndex < _commitIndex)
    {
        //        int64_t index = min(_raftLog->getFirstLogIndex(), _lastAppliedIndex + 1);
        // leader??????, ????????????????????????leader, ??????????????????Follower?????????????????????Leader?????????A, ????????????, ??????Leader???????????????????????????B(??????????????????????????????)
        // Leader????????????, ??????LOG, ???????????????????????????, ?????????????????????Log????????????B?????????LOG??????
        // ????????????appliedIndex???Leader?????????commitIndex, ????????????gap??????, ?????????????????????gap???log??????Leader ??????B????????????apply???
        // ??????????????????, ??????follower?????????log?????????????????????!

        //?????????apply?????????, ??????????????????log apply?????????
	    list<shared_ptr<LogEntry>> entries;

	    _raftLog->getEntry(_lastAppliedIndex + 1, _commitIndex, entries);

	    for (auto &entry : entries)
	    {
		    auto log = entry->log();

		    if (entry->type() == ENTRY_TYPE_DATA) {
			    _stateMachine->onApply(log.first, log.second, entry->index(), NULL);
		    }
		    else if (entry->type() == ENTRY_TYPE_CONFIGURATION) {
			    applyConfiguration(*entry.get());
		    }
		    _lastAppliedIndex = entry->index();
	    }
    }
}

void RaftNode::advanceCommitIndexLeader()
{
    // ??????matchIndex
    int peerNum = _configuration.servers.size();

    vector<int64_t> matchIndexes;
    matchIndexes.resize(peerNum);

    int i = 0;

    //??????????????????(????????????), ???????????????log????????????
    for (Server &server : _configuration.servers)
    {
        if (server.serverId != _localServer.serverId)
        {
            matchIndexes[i++] = getPeerNoLock(server.serverId)->getMatchIndex();
        }
    }
    matchIndexes[i] = _raftLog->getLastLogIndex();

    //??????????????????
    std::sort(matchIndexes.begin(), matchIndexes.end());

    //?????????????????????commit???commitIndex????????????(??????sort???????????????, ??????????????????, ?????????)
    int64_t newCommitIndex = matchIndexes[peerNum / 2];

// ?????????????????????, ?????????????????????????????????, ??????????????????, ?????????leader????????????+log
	if (_raftLog->getEntryTerm(newCommitIndex) != _currentTerm)
    {
        //??????????????????????????????commitIndex??????log???term, ???leader?????????term????????????, ???????????????????????????
        return;
    }

    if (_commitIndex < newCommitIndex)
    {
        int64_t oldCommitIndex = _commitIndex;

        _commitIndex = newCommitIndex;

        //???oldCommitIndex???newCommitIndex???log??????????????????
	    list<shared_ptr<LogEntry>> entries;

	    _raftLog->getEntry(oldCommitIndex + 1, newCommitIndex, entries);

	    for (auto &entry : entries)
	    {
		    auto it = _callbacks.find(entry->index());

		    if (entry->type() == ENTRY_TYPE_DATA)
		    {
			    auto log = entry->log();

			    if (it != _callbacks.end())
			    {
				    _stateMachine->onApply(log.first, log.second, entry->index(), it->second);
			    }
			    else
		        {
				    _stateMachine->onApply(log.first, log.second, entry->index(), NULL);
			    }
		    }
		    else if (entry->type() == ENTRY_TYPE_CONFIGURATION)
		    {
			    applyConfiguration(*entry.get());
		    }

		    if (it != _callbacks.end())
		    {
			    if (it->second)
			    {
				    it->second->onAfterApply();
			    }

			    _callbacks.erase(it);
		    }
	    }

        _lastAppliedIndex = _commitIndex;
    }
}

//leader??????????????????????????????????????????????????????
InstallSnapshotResponse RaftNode::onInstallSnapshot(const raft::InstallSnapshotRequest &request)
{
    InstallSnapshotResponse rsp;
    {
        //??????????????????
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        rsp.resCode = RES_CODE_FAIL;

        rsp.term = _currentTerm;

        if (request.term < _currentTerm)
        {
            return rsp;
        }

	    RAFT_DEBUG_LOG(this, "request.term:" << request.term << ", _currentTerm:" << _currentTerm << endl);

	    stepDown(request.term);

        if (_leaderId <= 0)
        {
            setLeaderId(request.serverId);
        }
    }

    _snapshot->onInstallSnapshot(request, rsp);

    return rsp;
}

Configuration RaftNode::removeServers(const Configuration &configuration, const vector<Server> &servers)
{
    Configuration newConfiguration = configuration;

    for (auto &server : servers)
    {
        for (auto it = newConfiguration.servers.begin(); it != newConfiguration.servers.end();)
        {
            if (it->serverId == server.serverId)
            {
                it = newConfiguration.servers.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
    return newConfiguration;
}

VoteResponse RaftNode::onPreVote(const VoteRequest &request)
{
//    RAFT_DEBUG_LOG(this, request.writeToJsonString() << endl);

    std::lock_guard<std::recursive_mutex> lock(_mutex);

    VoteResponse rsp;

    rsp.voteGranted = false;
    rsp.term        = _currentTerm;

    if (!RaftNode::containsServer(_configuration, request.serverId))
    {
        RAFT_ERROR_LOG(this, "no server, configuration:" << configurationToString(_configuration) << ", request:" << request.writeToJsonString() << endl);
        return rsp;
    }

    auto peer = getPeerNoLock(request.serverId);

    if (request.term < _currentTerm)
    {
    	RAFT_DEBUG_LOG(this, "request.term:" << request.term << " < currentTerm:" << _currentTerm << ", from server:" << peer->getServer().serverId << ", " << peer->getServer().endPoint << endl);

        return rsp;
    }

    bool isLogOk = request.lastLogTerm > getLastLogTerm() || (request.lastLogTerm == getLastLogTerm() && request.lastLogIndex >= _raftLog->getLastLogIndex());

    RAFT_DEBUG_LOG(this, "isLogOk:" << isLogOk << ", request.term:" << request.term << ", currentTerm:" << _currentTerm << ", request.lastLogIndex:" << request.lastLogIndex << ", getLastLogIndex:" << _raftLog->getLastLogIndex() << ", from server:" << peer->getServer().serverId << ", " << peer->getServer().endPoint << endl);

    if (!isLogOk)
    {
    	RAFT_DEBUG_LOG(this, "voteGranted false, isLeader:" << isLeader() << ", request.term:" << request.term << " < currentTerm:" << _currentTerm << ", from server:" << peer->getServer().serverId << ", " << peer->getServer().endPoint << endl);

    	if(isLeader())
	    {
    		//peer????????????, ????????????replicator, ?????????leader??????????????????
    		peer->setOk();
	    }
        return rsp;
    }
    else
    {
        rsp.voteGranted = true;
        rsp.term        = _currentTerm;

        RAFT_DEBUG_LOG(this, "voteGranted true, isLeader:" << isLeader() << ", request.term:" << request.term << " < currentTerm:" << _currentTerm << ", from server:" << peer->getServer().serverId << ", " << peer->getServer().endPoint << endl);
    }

    return rsp;
}

VoteResponse RaftNode::onRequestVote(const VoteRequest &request)
{
    RAFT_DEBUG_LOG(this, request.writeToJsonString() << endl);

    std::lock_guard<std::recursive_mutex> lock(_mutex);

    VoteResponse rsp;

    rsp.voteGranted = false;
    rsp.term        = _currentTerm;

    if (!RaftNode::containsServer(_configuration, request.serverId))
    {
        return rsp;
    }

    auto peer = getPeerNoLock(request.serverId);

    if (request.term < _currentTerm)
    {
    	RAFT_DEBUG_LOG(this, "request.term:" << request.term << " < _currentTerm:" << _currentTerm << ", from server:" << peer->getServer().serverId << ", " << peer->getServer().endPoint << endl);
        return rsp;
    }

    if (request.term > _currentTerm)
    {
    	RAFT_DEBUG_LOG(this, "request.term:" << request.term << " > _currentTerm:" << _currentTerm << ", from server:" << peer->getServer().serverId << ", " << peer->getServer().endPoint << endl);
        stepDown(request.term);
    }

    //???????????????????????????, ????????????????????????, ?????????????????????????????????, ??????????????????????????????
    bool isLogOk = request.lastLogTerm > getLastLogTerm() || (request.lastLogTerm == getLastLogTerm() && request.lastLogIndex >= _raftLog->getLastLogIndex());

    RAFT_DEBUG_LOG(this, "votedFor:" << _votedFor << ", isLogOk:" << isLogOk << ", request.term:"
                                                   << request.term << ", currentTerm:" << _currentTerm << ", request.lastLogIndex:" << request.lastLogIndex << ", getLastLogIndex:" << _raftLog->getLastLogIndex()
                                                   << ", from server:" << peer->getServer().serverId << ", " << peer->getServer().endPoint << endl);

    //???????????????
    if (_votedFor == 0 && isLogOk)
    {
    	RAFT_DEBUG_LOG(this, "voteGranted true, request.term:" << request.term << ", _currentTerm:" << _currentTerm << ", from server:" << peer->getServer().serverId << ", " << peer->getServer().endPoint << endl);

	    stepDown(request.term);

        _votedFor = request.serverId;

        _raftLog->updateLogMetaData(_currentTerm, _votedFor, 0);

        rsp.voteGranted = true;
        rsp.term        = _currentTerm;
    }

    RAFT_DEBUG_LOG(this, "voteGranted false, request.term:" << request.term << ", _currentTerm:" << _currentTerm << ", from server:" << peer->getServer().serverId << ", " << peer->getServer().endPoint << endl);

    return rsp;
}

string RaftNode::configurationToString(const Configuration &configuration)
{
    string buff = "{";
    for(auto & server : configuration.servers)
    {
        buff += "[" + server.endPoint + ", " + TC_Common::tostr(server.serverId) + "]";
    }
    buff += "}";
    return buff;
}

GetConfigurationResponse RaftNode::onGetConfiguration()
{
    GetConfigurationResponse rsp;
    rsp.resCode = RES_CODE_SUCCESS;

    std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (_leaderId <= 0)
    {
        rsp.resCode = RES_CODE_FAIL;
    }
    else
    {
        rsp.leader = getLeaderServer();

        rsp.servers = _configuration.servers;
    }
	RAFT_DEBUG_LOG(this, rsp.writeToJsonString() << endl);

    return rsp;
}

GetLeaderResponse RaftNode::onGetLeader()
{
    GetLeaderResponse rsp;
    rsp.resCode = RES_CODE_SUCCESS;

    std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (_leaderId <= 0)
    {
        rsp.resCode = RES_CODE_FAIL;
    }
    else if (_leaderId == _localServer.serverId)
    {
        assert(_state == RaftNode::STATE_LEADER);
        rsp.leaderEndpoint = _localServer.endPoint;
    }
    else
    {
        rsp.leaderEndpoint = getLeaderServer().endPoint;
    }

	RAFT_DEBUG_LOG(this, rsp.writeToJsonString() << endl);

    return rsp;
}

void RaftNode::clearAddPeers()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    vector<int> ids;
    for (auto e : _peers)
    {
        if (!containsServer(_configuration, e.second->getServer().serverId))
        {
            ids.push_back(e.second->getServer().serverId);
        }
    }

    for (auto &id : ids)
    {
        _peers.erase(id);
    }
}

void RaftNode::onCatchUp(const shared_ptr<Peer> &peer)
{
    _catchUpNum++;

    _oldConfiguration.servers.push_back(peer->getServer());

    peer->setStatus(PS_INSTALLING_SNAPSHOT_SUCC);

    //???????????????????????????
    if (_catchUpNum == _requestPeers)
    {
        TarsOutputStream<BufferWriterString> os;

        _oldConfiguration.writeTo(os);

        replicate(os.getByteBuffer(), ENTRY_TYPE_CONFIGURATION, NULL);
    }
}

void RaftNode::onAddPeers(const raft::PeersRequest &request, raft::PeersResponse &rsp)
{
    RAFT_DEBUG_LOG(this, request.writeToJsonString() << endl);

    std::lock_guard<std::recursive_mutex> lock(_mutex);

    rsp.resCode = RES_CODE_FAIL;

    // if (request.servers.size() % 2 != 0)
    // {
    //     rsp.resMsg = "added server's size can only multiple of 2";
    //     return;
    // }

    if (_state != STATE_LEADER)
    {
        rsp.resMsg = "current raft node must be leader.";
        RAFT_ERROR_LOG(this, rsp.resMsg << ", request: " << request.writeToJsonString() << endl);
        return;
    }

    set<int> ids;
    for (const Server &server : request.servers)
    {
        if (_peers.find(server.serverId) != _peers.end())
        {
            rsp.resMsg = "already be added to configuration";
            RAFT_ERROR_LOG(this, rsp.resMsg << ", request: " << request.writeToJsonString() << endl);
            return;
        }

        if (ids.find(server.serverId) != ids.end())
        {
            rsp.resMsg = "serverId conflicts.";
            RAFT_ERROR_LOG(this, rsp.resMsg << ", request: " << request.writeToJsonString() << endl);
            return;
        }

        ids.insert(server.serverId);
    }

    _oldConfiguration = _configuration;
    _catchUpNum       = 0;
    _requestPeers     = request.servers.size();

    //???????????????????????????, ?????????install ??????
    for (auto &server : request.servers)
    {
        shared_ptr<Peer> peer = std::make_shared<Peer>(server, shared_from_this());

        peer->setNextIndex(1);
        peer->start();

        //?????????peers???, ?????????????????????!
        _peers[peer->getServer().serverId] = peer;
    }

	rsp.resCode = RES_CODE_SUCCESS;

}

void RaftNode::onRemovePeers(const raft::PeersRequest &request, const shared_ptr<PeersReplicateCallback> &callback)
{
    RAFT_DEBUG_LOG(this, request.writeToJsonString() << endl);

    callback->rsp().resCode = RES_CODE_FAIL;

    // if (request.servers.size() % 2 != 0)
    // {
    //     callback->rsp().resMsg = "removed server's size can only multiple of 2";
    //     callback->response();
    //     return;
    // }

    if (_state != STATE_LEADER)
    {
        callback->rsp().resMsg = "must be leader.";
        callback->response();
        return;
    }

    //?????????????????????????????????server, ????????????
    if (!containsServer(request.servers))
    {
        callback->rsp().resMsg = "removed servers are not exists in configuration";
        callback->response();
        return;
    }

    //????????????
    Configuration newConfiguration = RaftNode::removeServers(getConfiguration(), request.servers);

    TarsOutputStream<BufferWriterString> os;
    newConfiguration.writeTo(os);

    RAFT_DEBUG_LOG(this, "newConfiguration:" << this->configurationToString(newConfiguration) << endl);

    //??????????????????????????????
    replicate(os.getByteBuffer(), ENTRY_TYPE_CONFIGURATION, callback);
}


 bool RaftNode::checkClusterNetwork()
 {    
    int normalPeerCount = 0;
    for (auto e : _peers)
    {
        if(e.second->isOk()) normalPeerCount ++;
    }
    if(normalPeerCount * 2 >= _peers.size())
    {
        return true;
    }
    return false;
 }
