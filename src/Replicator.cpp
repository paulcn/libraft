//
// Created by jarod on 2020/10/29.
//

#include "Replicator.h"
#include "RaftNode.h"

class AppendEntriesCallback : public RaftPrxCallback
{
  public:
    AppendEntriesCallback(const shared_ptr<Replicator> &replicator, const shared_ptr<AppendEntriesRequest> &request)
        : _replicator(replicator), _request(request)
    {
    }

    virtual void callback_appendEntries(const raft::AppendEntriesResponse &rsp)
    {
//    	if(!_request->entries.empty()) {
//		    TLOGERROR("AppendEntriesCallback callback_appendEntries:" << _request->entries.size() << ", cost:"
//		                                                              << TNOWMS - _lastAppendEntriesTime << "ms"
//		                                                              << endl);
//	    }

		auto replicator = _replicator.lock();

		if (replicator)
		{
			replicator->appendEntriesSucc(_request, rsp);
		}
	}

    virtual void callback_appendEntries_exception(tars::Int32 ret)
    {
		auto replicator = _replicator.lock();

		if (replicator)
		{
			replicator->appendEntriesFailed(ret, _request);
		}
    }

  protected:
	weak_ptr<Replicator> _replicator;

    shared_ptr<AppendEntriesRequest> _request;

};

typedef TC_AutoPtr<AppendEntriesCallback> AppendEntriesCallbackPtr;

Replicator::Replicator(const shared_ptr<RaftNode> &raftNode, const shared_ptr<Peer> &peer, const RaftPrx &raftPrx) : _raftNode(raftNode), _peer(peer), _raftPrx(raftPrx), _terminate(false)
{
}

Replicator ::~Replicator()
{
}

bool Replicator::push(const shared_ptr<AppendEntriesRequest> &request)
{
	auto raftNode = _raftNode.lock();

	if (raftNode)
	{
		if (_curLogEntriesMemQueue >= raftNode->getRaftOptions().maxLogEntriesMemQueue)
		{
			//当前内存中的数据太多了, 暂时先不要发送了
			return false;
		}

		std::unique_lock<std::mutex> lck(_mutex);

		_curLogEntriesMemQueue += request->entries.size();

		_requestList.push_back(request);

		_cond.notify_one();
	}

	return true;
}

int64_t Replicator::nextIndex()
{
	auto peer = _peer.lock();

	if(peer)
	{
		shared_ptr<AppendEntriesRequest> request;

		{
			std::unique_lock<std::mutex> lck(_mutex);

			if (_requestList.empty())
			{
				return peer->getNextIndex();
			}
			else
			{
				request = _requestList.back();
			}
		}

		if(request->entries.empty())
		{
			return peer->getNextIndex();
		}

		return request->prevLogIndex + request->entries.size() + 1;
	}

	return -1;
}

void Replicator::terminate()
{
	std::unique_lock<std::mutex> lck(_mutex);

	_terminate = true;

	_cond.notify_one();
}

void Replicator::appendEntriesSucc(const shared_ptr<AppendEntriesRequest> &request, const AppendEntriesResponse &rsp)
{
//	cout << TC_Common::now2str() << ", "<< "_replicator succ peer:" << _peer->getServer().endPoint << ", curLogEntriesTransfering:" << _curLogEntriesTransfering << ", curLogEntriesMemQueue:" << _curLogEntriesMemQueue << endl;

//	TLOG_DEBUG("_replicator peer:" << _peer->getServer().endPoint << ", curLogEntriesTransfering:" << _curLogEntriesTransfering << ", curLogEntriesMemQueue:" << _curLogEntriesMemQueue << endl);

	auto raftNode = _raftNode.lock();
	auto peer = _peer.lock();

	if (raftNode && peer)
	{
		raftNode->appendEntriesSucc(peer, request, rsp);

		std::unique_lock<std::mutex> lck(_mutex);

		++_continueSuccCount;

		_retrys = 0;

		if (_curLogEntriesTransfering > 0)
		{
			_curLogEntriesTransfering -= request->entries.size();
		}

		if(_curLogEntriesMemQueue > 0) {
			_curLogEntriesMemQueue -= request->entries.size();
		}

		_cond.notify_one();
	}
}


void Replicator::appendEntriesFailed(int ret, const shared_ptr<AppendEntriesRequest> &request)
{
//	cout << TC_Common::now2str() << ", "<< "_replicator ret:" << ret << ", peer:" << _peer->getServer().endPoint << ", curLogEntriesTransfering:" << _curLogEntriesTransfering << ", curLogEntriesMemQueue:" << _curLogEntriesMemQueue << endl;
	// RAFT_ERROR_LOG(_raftNode, "ret:" << ret << ", peer:" << _peer->getServer().endPoint << ", curLogEntriesTransfering:" << _curLogEntriesTransfering << ", curLogEntriesMemQueue:" << _curLogEntriesMemQueue << endl);

	std::unique_lock<std::mutex> lck(_mutex);

	_continueSuccCount = 0;

	++_retrys;

	_requestList.clear();

	_curLogEntriesTransfering = 0;

	_curLogEntriesMemQueue = 0;

	_cond.notify_one();
}

bool Replicator::isOk()
{
	return _retrys < 2;
}

void Replicator::setOk()
{
	_retrys = 0;
}

void Replicator::run()
{
	while(!_terminate)
	{
		auto raftNode = _raftNode.lock();

		if (raftNode->isLeader())
		{
			try
			{

				shared_ptr<AppendEntriesRequest> request;

				{
					std::unique_lock<std::mutex> lck(_mutex);

					//等待, 传输中的日志条数小于限定值 && 队列中有数据 才会继续传输
					_cond.wait_for(lck,
								   std::chrono::milliseconds(raftNode->getRaftOptions().heartbeatPeriodMilliseconds),
								   [&]()
								   {
									   return _terminate || (!_requestList.empty() && _curLogEntriesTransfering < raftNode->getRaftOptions().maxLogEntriesTransfering && _curLogEntriesMemQueue < raftNode->getRaftOptions().maxLogEntriesMemQueue);
								   });

					if(_terminate)
					{
						return;
					}

					if (!_requestList.empty())
					{
						request = _requestList.front();

						_requestList.pop_front();
					}
				}

				if (request)
				{
					_curLogEntriesTransfering += request->entries.size();

					auto replicator = shared_from_this();

					AppendEntriesCallbackPtr callback = new AppendEntriesCallback(replicator, request);

					_raftPrx->async_appendEntries(callback, *request.get());
				}

			}
			catch (exception & ex) {
				RAFT_ERROR_LOG(raftNode, "error:" << ex.what() << endl);
			}
		}
		else
		{
			std::unique_lock<std::mutex> lck(_mutex);
			_cond.wait_for(lck, std::chrono::milliseconds(100));
		}
	}
}