//
// Created by jarod on 2019-07-22.
//

#ifndef LIBRAFT_RAFTIMP_H
#define LIBRAFT_RAFTIMP_H

#include "Raft.h"
#include "servant/Application.h"

class StateMachine;
class RaftNode;

using namespace raft;

class RaftImp: public Raft
{
public:
	RaftImp(const shared_ptr<RaftNode> &raftNode);

    virtual ~RaftImp() { }
    virtual void initialize();

    virtual void destroy();
    virtual void appendEntries(const raft::AppendEntriesRequest & request,raft::AppendEntriesResponse &rsp,tars::TarsCurrentPtr current);
    virtual void installSnapshot(const raft::InstallSnapshotRequest & request,raft::InstallSnapshotResponse &rsp,tars::TarsCurrentPtr current);
    virtual void preVote(const raft::VoteRequest & request,raft::VoteResponse &rsp,tars::TarsCurrentPtr current);
    virtual void requestVote(const raft::VoteRequest & request, raft::VoteResponse &rsp, tars::TarsCurrentPtr current);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    virtual raft::GetConfigurationResponse getConfiguration(tars::TarsCurrentPtr current);
    virtual raft::GetLeaderResponse getLeader(tars::TarsCurrentPtr current);
    virtual void addPeers(const raft::PeersRequest & request, raft::PeersResponse &rsp,tars::TarsCurrentPtr current);
    virtual void clearAddPeers(tars::TarsCurrentPtr current);
    virtual void removePeers(const raft::PeersRequest & request, raft::PeersResponse &rsp,tars::TarsCurrentPtr current);
    virtual int onDispatch(tars::TarsCurrentPtr _current, vector<char> &_sResponseBuffer);

protected:
	unordered_set<string>   _localFuncs;
	shared_ptr<RaftNode> _raftNode;
};


#endif //LIBRAFT_RAFTCLIENTIMP_H
