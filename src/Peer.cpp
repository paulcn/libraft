//
// Created by jarod on 2019-06-03.
//

#include "Peer.h"
#include "RaftNode.h"
#include "servant/Application.h"
#include "Replicator.h"

InstallSnapshotThread::InstallSnapshotThread(const shared_ptr<RaftNode> &raftNode, const shared_ptr<Peer> &peer)
	: _raftNode(raftNode), _peer(peer)
{
}

InstallSnapshotThread ::~InstallSnapshotThread()
{
}

void InstallSnapshotThread::run()
{
	auto raftNode = _raftNode.lock();
	auto peer = _peer.lock();

	if (raftNode && peer)
	{
		bool succ = raftNode->getSnapshot()->installSnapshot(peer,
															 raftNode->getRaftOptions().maxSnapshotBytesPerRequest, raftNode->getLocalServer().serverId,
															 raftNode->getCurrentTerm());
		if (succ)
		{
			peer->onInstallSnapshotSucc();
		}
	}
}

//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class InstallSnapshotCallback : public RaftPrxCallback
{
public:
	InstallSnapshotCallback(const shared_ptr<Peer> &peer)
		: _peer(peer)
	{
	}

	virtual void callback_installSnapshot(const raft::InstallSnapshotResponse &response)
	{
		auto peer = _peer.lock();
		if(peer)
		{
			std::unique_lock<std::mutex> lock(peer->_installShapshotMutex);
			_return = true;
			_response = response;
			peer->_installShapshotCond.notify_one();
		}
	}

	virtual void callback_installSnapshot_exception(tars::Int32 ret)
	{
		auto peer = _peer.lock();
		if (peer)
		{
			std::unique_lock<std::mutex> lock(peer->_installShapshotMutex);
			_return = true;
			_response.resCode = RES_CODE_FAIL;
			peer->_installShapshotCond.notify_one();
		}
	}

	bool _return = false;
	raft::InstallSnapshotResponse _response;
	weak_ptr<Peer> _peer;
};

typedef TC_AutoPtr<InstallSnapshotCallback> InstallSnapshotCallbackPtr;

/////////////////////////////////////////////////////////////////////////////////////////////////

Peer::Peer(const Server &server, const shared_ptr<RaftNode> &raftNode) : _server(server),
																		 _raftNode(raftNode)
{
	_raftPrx = Application::getCommunicator()->stringToProxy<RaftPrx>(_server.obj + "@" + _server.endPoint);
}

Peer::~Peer()
{
}

void Peer::installSnapshot(const InstallSnapshotRequest &request, InstallSnapshotResponse &response)
{
	auto peer = shared_from_this();

	InstallSnapshotCallbackPtr callback = new InstallSnapshotCallback(peer);

	std::unique_lock<std::mutex> lock(_installShapshotMutex);

	_raftPrx->tars_set_timeout(10 * 1000)->async_installSnapshot(callback, request);

	_installShapshotCond.wait(lock, [&] { return _terminate || callback->_return; });

	response = callback->_response;
}

void Peer::setNextIndex(int64_t nextIndex)
{
    assert(nextIndex > 0);
    _nextIndex = nextIndex;
}

void Peer::async_preVote(const VoteRequest &req, RaftPrxCallbackPtr callback)
{
	shared_ptr<RaftNode> raftNode = _raftNode.lock();

	if(raftNode)
	{
		_raftPrx->tars_set_timeout(raftNode->getRaftOptions().electionTimeoutMilliseconds/2.5)->async_preVote(callback, req);
	}
}

void Peer::async_requestVote(const VoteRequest &req, RaftPrxCallbackPtr callback)
{
	shared_ptr<RaftNode> raftNode = _raftNode.lock();
	if(raftNode)
	{
		_raftPrx->tars_set_timeout(raftNode->getRaftOptions().electionTimeoutMilliseconds/2.5)->async_requestVote(callback, req);
	}
}

void Peer::terminate()
{
	_replicator->terminate();

	if(_replicator->joinable())
	{
		_replicator->join();
	}

	{
		std::unique_lock<std::mutex> lck(_mutex);

		_terminate = true;

		_cond.notify_all();

	}

	{
		std::unique_lock<std::mutex> lck(_installMutex);
		_installCond.notify_all();
	}

	{
		std::unique_lock<std::mutex> lck(_installShapshotMutex);
		_installShapshotCond.notify_all();
	}
}

void Peer::notify()
{
	//????????????, ??????, peer??????
    std::unique_lock<std::mutex> lck(_mutex);

    _cond.notify_all();
}

void Peer::installSnapshot()
{
    if (_installThread->isAlive())
        return;

    //????????????, ???????????????????????????????????????
    _installThread->start();
}

void Peer::onInstallSnapshotSucc()
{
	std::unique_lock<std::mutex> lck(_installMutex);

	_installCond.notify_one();
}

void Peer::setOk()
{
	if(_replicator) {
		_replicator->setOk();
	}
}

bool Peer::isOk()
{
	return _replicator ? _replicator->isOk() : false;
}

void Peer::run()
{
	this->setThreadName("peer-[" + _server.endPoint + "]");

	shared_ptr<RaftNode> raftNode = _raftNode.lock();
	auto peer = shared_from_this();

	if (raftNode && peer)
	{
		_replicator = std::make_shared<Replicator>(raftNode, peer, _raftPrx);

		_replicator->setThreadName("replicator-[" + _server.endPoint + "]");

		_replicator->start();

		_installThread.reset(new InstallSnapshotThread(raftNode, peer));

		int64_t lastCheckOkTime = TNOW;

		while (!_terminate)
		{
			if (raftNode->isLeader())
			{
				bool install = raftNode->needInstallSnapshot(peer);
				if(install)
				{
					//??????????????????, ??????
					std::unique_lock<std::mutex> lck(_installMutex);

					if(_installCond.wait_for(lck, std::chrono::milliseconds(1000)) == std::cv_status::timeout)
					{
						//?????????, ??????????????????
						continue;
					}
					else
					{
						//????????????????????????
						_replicator->setOk();
					}
				}

				//???leader, ????????????????????????, ???????????????
				try
				{
					//???????????????, ???????????????????????????,
					if(_replicator->isOk())
					{
						lastCheckOkTime = TNOW;

						//???????????????????????????, ??????????????????????????????
						shared_ptr<AppendEntriesRequest> request = std::make_shared<AppendEntriesRequest>();

						int64_t nextIndex = _replicator->nextIndex();

						if (nextIndex < 0)
						{
							break;
						}

						bool flag = raftNode->getAppendEntriesRequest(nextIndex, request);

						if (!flag || !_replicator->push(request) || request->entries.empty())
						{
							//??????????????????, ????????????????????????????????????(push???, ?????????????????????????????????), ??????????????????
							std::unique_lock<std::mutex> lck(_mutex);

							_cond.wait_for(lck, std::chrono::milliseconds(raftNode->getRaftOptions().heartbeatPeriodMilliseconds));
						}

					}
					else
					{
						{
							//???????????????, ??????????????????, ??????????????????????????????????????????
							std::unique_lock<std::mutex> lck(_mutex);

							_cond.wait_for(lck, std::chrono::milliseconds(raftNode->getRaftOptions().heartbeatPeriodMilliseconds));
						}

						if (TNOW - lastCheckOkTime > raftNode->getRaftOptions().peerFailureIsolationTime)
						{
							_replicator->setOk();
						}
					}
				}
				catch (exception &ex)
				{
					RAFT_ERROR_LOG(raftNode, "error:" << ex.what() << endl);
				}
			}
			else
			{
				std::unique_lock<std::mutex> lck(_mutex);
				_cond.wait_for(lck, std::chrono::milliseconds(1000));
			}
		}

		if(_installThread->joinable())
		{
			_installThread->join();
		}

		_installThread.reset();
	}
}
