//
// Created by jarod on 2020/10/29.
//

#ifndef RAFT_REPLICATOR_H
#define RAFT_REPLICATOR_H

#include "Raft.h"
#include "util/tc_thread.h"

using namespace raft;
using namespace tars;

class RaftNode;
class Peer;

/**
 * 日志复制器
 */
class Replicator : public TC_Thread, public enable_shared_from_this<Replicator>
{
public:

	/**
	 * 构造函数
	 */
	Replicator(const shared_ptr<RaftNode> &raftNode, const shared_ptr<Peer> &peer, const RaftPrx &raftPrx);

	/**
	 * 析构函数
	 */ 
	~Replicator();

	/**
	 * 下一条复制的log Index 
	 */
	int64_t nextIndex();

	/**
	 * push数据
	 * @param request
	 */
	bool push(const shared_ptr<AppendEntriesRequest> &request);

	/**
	 * 结束 
	 */
	void terminate();

	/**
	 * 同步日志成功的回调
	 */
	void appendEntriesSucc(const shared_ptr<AppendEntriesRequest> &request, const AppendEntriesResponse &response);

	/**
	 * 同步日志失败的回调
	 */
	void appendEntriesFailed(int ret, const shared_ptr<AppendEntriesRequest> &request);

	/**
	 * 状态是否ok
	 */
	bool isOk();

	/**
	 * 设置状态为ok
	 */
	void setOk();

protected:
	virtual void run();

protected:

	weak_ptr<RaftNode>	_raftNode;

	weak_ptr<Peer> 		_peer;

	RaftPrx     _raftPrx;

	// 当前队列中日志的条数
	atomic<size_t> _curLogEntriesMemQueue {0};

	//网络传输中的日志条数
	atomic<size_t> _curLogEntriesTransfering {0};

	//失败重试次数
	size_t      _retrys = 0;

	//连续成功的次数
	size_t      _continueSuccCount = 0;

	bool        _terminate;

	std::mutex               _mutex;

	std::condition_variable  _cond;

	deque<shared_ptr<AppendEntriesRequest>> _requestList;

};


#endif //RAFT_REPLICATOR_H
