//
// Created by jarod on 2019-07-22.
//

#include "RaftImp.h"
#include "RaftNode.h"

RaftImp::RaftImp(const shared_ptr<RaftNode> &raftNode) : _raftNode(raftNode)
{
}

void RaftImp::initialize()
{
	_localFuncs.insert({"preVote", "requestVote", "appendEntries", "installSnapshot", "tars_ping"});
}

void RaftImp::destroy()
{
}

void RaftImp::appendEntries(const raft::AppendEntriesRequest &request, raft::AppendEntriesResponse &rsp, tars::TarsCurrentPtr current)
{
    rsp = _raftNode->onAppendEntries(request);
}

void RaftImp::installSnapshot(const raft::InstallSnapshotRequest &request, raft::InstallSnapshotResponse &rsp, tars::TarsCurrentPtr current)
{
    rsp = _raftNode->onInstallSnapshot(request);
}

void RaftImp::preVote(const raft::VoteRequest &request, raft::VoteResponse &rsp, tars::TarsCurrentPtr current)
{
    rsp = _raftNode->onPreVote(request);
}

void RaftImp::requestVote(const raft::VoteRequest &request, raft::VoteResponse &rsp, tars::TarsCurrentPtr current)
{
    rsp = _raftNode->onRequestVote(request);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

raft::GetConfigurationResponse RaftImp::getConfiguration(tars::TarsCurrentPtr current)
{
    return _raftNode->onGetConfiguration();
}

raft::GetLeaderResponse RaftImp::getLeader(tars::TarsCurrentPtr current)
{
    return _raftNode->onGetLeader();
}

void RaftImp::addPeers(const raft::PeersRequest &request, raft::PeersResponse &rsp, tars::TarsCurrentPtr current)
{
    _raftNode->onAddPeers(request, rsp);
}

void RaftImp::clearAddPeers(tars::TarsCurrentPtr current)
{
    _raftNode->clearAddPeers();
}

void RaftImp::removePeers(const raft::PeersRequest &request, raft::PeersResponse &rsp, tars::TarsCurrentPtr current)
{
    current->setResponse(false);

    shared_ptr<RaftNode::PeersReplicateCallback> callback = std::make_shared<RaftNode::PeersReplicateCallback>(current);

    _raftNode->onRemovePeers(request, callback);
}

class RaftServantProxyCallback : public ServantProxyCallback
{
  public:
    RaftServantProxyCallback(const TarsCurrentPtr &current) : _current(current)
    {
//        _ms = TNOWMS;
    }

  protected:
    virtual int onDispatch(ReqMessagePtr msg)
    {
        // TLOG_ERROR("onDispatch cost:" << TNOWMS - _ms << endl);
        _current->sendResponse(msg->response->iRet, *msg->response.get(), msg->response->status, msg->response->sResultDesc);
        return 0;
    }

  protected:
//    int64_t _ms;
    CurrentPtr _current;
};

int RaftImp::onDispatch(tars::TarsCurrentPtr current, vector<char> &_sResponseBuffer)
{
    int flag = TARSSERVERSUCCESS;

    try
    {
        if (_raftNode->isLeader() || _localFuncs.find(current->getFuncName()) != _localFuncs.end())
        {
            flag = Raft::onDispatch(current, _sResponseBuffer);
        }
        else if(_raftNode->isFollower())
        {
            const map<std::string, std::string> & context = current->getContext();
            if(context.find(FORWARD_FLAG) != context.end())
            {
                //已经转发过一次了，可能已经死循环了，所以不能再转发了
                TLOGEX_ERROR(_raftNode ->getLocalServerId(), "forward from follower:" << current->getIp() << ":" << current->getPort() << endl);
            }
            else
            {
                //转发到Leader, 且重定向客户端
                RaftPrx sPrx = _raftNode->getLeaderPrx();

                current->setResponse(false);

                ServantProxyCallbackPtr callback = new RaftServantProxyCallback(current);

                sPrx->tars_invoke_async(current->getPacketType(),
                                    current->getFuncName(),
                                    current->getRequestBuffer(),
                                    context,
                                    current->getRequestStatus(),
                                    callback);    
            }
        }
        else
        {
        	current->close();
        }
    }
    catch (exception &ex)
    {
	    TLOGEX_ERROR(_raftNode ->getLocalServerId(), "funcName:" << current->getFuncName() << ", error:" << ex.what() << endl);
        current->close();
    }


    return flag;
}
