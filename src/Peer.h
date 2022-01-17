//
// Created by jarod on 2019-06-03.
//

#ifndef LIBRAFT_PEER_H
#define LIBRAFT_PEER_H

#include "Raft.h"

using namespace raft;

class RaftNode;
class Snapshot;
class Peer;
class Replicator;
class InstallSnapshotCallback;

class InstallSnapshotThread : public TC_Thread
{
public:
    InstallSnapshotThread(const shared_ptr<RaftNode> &raftNode, const shared_ptr<Peer> &peer);

    ~InstallSnapshotThread();

protected:
    virtual void run();

protected:
    weak_ptr<RaftNode>  _raftNode;
    weak_ptr<Peer> _peer;
};

//代表某个节点
//每个节点都启动一个线程, 这个线程专门用于发送数据 or 心跳
class Peer : public TC_Thread, public enable_shared_from_this<Peer>
{
public:
	/**
	 * 构造
	 * @param server
	 * @param raftNode
	 */
    Peer(const Server &server, const shared_ptr<RaftNode> &raftNode);

    /**
     * 析构
     */
    ~Peer();

    //结束线程
    void terminate();

    //唤醒线程同步数据
    void notify();

    //设置下一个需要同步的log index
    void setNextIndex(int64_t nextIndex);

    //获取下一个需要同步的log index
    int64_t getNextIndex() const { return _nextIndex; }

    //Follower节点已经接受到的Log Index
    void setMatchIndex(int64_t matchIndex) { _matchIndex = matchIndex; }
    int64_t getMatchIndex() { return _matchIndex; }

    //设置是否可以发起投票
    void setVoteGranted(bool granted) { _voteGranted = granted; }
    bool isVoteGranted() const { return _voteGranted;}

    PeerStatus getStatus() const { return _status;}
    void setStatus(PeerStatus s) { _status = s;}

    const Server &getServer() { return _server; }

    void async_preVote(const VoteRequest &req, RaftPrxCallbackPtr callback);

    void async_requestVote(const VoteRequest &req, RaftPrxCallbackPtr callback);

    void installSnapshot(const InstallSnapshotRequest&request, InstallSnapshotResponse &response);

    //启动安装快照线程, 如果已经在安装了, 则直接返回
    void installSnapshot();

    //安装快照成功回调
    void onInstallSnapshotSucc();

    void setOk();

    bool isOk();

    bool isTerminate() const { return _terminate; }

protected:

    virtual void run();

    friend class InstallSnapshotThread;
    friend class AppendEntriesCallback;
    friend class InstallSnapshotCallback;

protected:
    bool _terminate = false;

    std::mutex               _mutex;
	std::condition_variable  _cond;

    std::mutex               _installMutex;
    std::condition_variable  _installCond;

    std::mutex               _installShapshotMutex;
    std::condition_variable  _installShapshotCond;

    Server                  _server;
    int64_t                 _nextIndex      = 1;
    int64_t                 _matchIndex     = 1;
    bool                    _voteGranted    = false;
    RaftPrx                 _raftPrx;
    PeerStatus              _status         = PS_INIT;
    weak_ptr<RaftNode>      _raftNode;

	shared_ptr<Replicator>  _replicator;

    shared_ptr<InstallSnapshotThread> _installThread;
};


#endif //LIBRAFT_PEER_H
