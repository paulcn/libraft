//
// Created by jarod on 2019-05-26.
//

#include "Snapshot.h"
#include "Peer.h"
#include "RaftLog.h"
#include "RaftNode.h"
#include "StateMachine.h"
#include "servant/RemoteLogger.h"
#include "util/tc_file.h"

using namespace tars;

Snapshot::Snapshot(RaftNode *raftNode, const string &raftDataDir, shared_ptr<RaftLog> &raftLog) : _raftNode(raftNode), _raftLog(raftLog)
{
    _snapshotDir = raftDataDir + FILE_SEP + "snapshot";

    TC_File::makeDirRecursive(getSnapshotDataDir());
}

SnapshotMetaData Snapshot::getMetaDataLock()
{
    std::unique_lock<std::mutex> lock(_mutex);

    return *_raftLog->getSnapshotMetaData().get();
}

int64_t Snapshot::createSnapshot(int64_t lastIncludedIndex,
                                 int64_t lastIncludedTerm,
                                 const Configuration &configuration)
{
	//创建snapshot文件, 需要加锁
	std::unique_lock<std::mutex> lock(_mutex);

	//创建临时目录, 用于创建snapshot
	string tmpSnapshotDataDir = getSnapshotTmpDir();

	TC_File::removeFile(tmpSnapshotDataDir, true);

	//调用业务层写snapshot, snapshot的格式和业务层强相关的, 业务层自己搞定

	_raftNode->getStateMachine()->onSaveSnapshot(tmpSnapshotDataDir);

	//把之前snapshot目录删除
	TC_File::removeFile(getSnapshotDataDir(), true);

	//重命名快照目录
	TC_File::renameFile(tmpSnapshotDataDir, getSnapshotDataDir());

	return _raftLog->updateSnapshotMetaData(lastIncludedIndex, lastIncludedTerm, configuration);
}

shared_ptr<SnapshotMetaData> Snapshot::getMetaData()
{
    return _raftLog->getSnapshotMetaData();
}

vector<Snapshot::SnapshotFile> Snapshot::getSnapshotFiles(int maxSnapshotBytesPerRequest)
{
    vector<Snapshot::SnapshotFile> data;

    vector<string> fileNames;
    TC_File::listDirectory(getSnapshotDataDir(), fileNames, true);

    for (auto &fileName : fileNames)
    {
        SnapshotFile sf;
        sf.fileName  = fileName;
        if (TC_File::isFileExist(sf.fileName, S_IFDIR))
        {
            sf.isDir = true;
        }
    
        sf.totalSize = TC_File::getFileSize(sf.fileName);
        int length   = (int)sf.totalSize;
        int startPos = 0;
        while (length > 0)
        {
            SnapshotTrans st;
            st.pos    = startPos;
            st.length = min(length, maxSnapshotBytesPerRequest);
            length -= maxSnapshotBytesPerRequest;
            startPos = startPos + st.length;
            sf.trans.push_back(st);
        }

        data.push_back(sf);
    }

    return data;
}

bool Snapshot::installSnapshot(const shared_ptr<Peer> &peer, int maxSnapshotBytesPerRequest, int localServerId, int64_t currentTerm)
{

    //    //检查是否已经被设置了, 即已经在安装快照了
    //    if (std::atomic_exchange(&getIsInstallSnapshot(), true)) {
    //        RAFT_DEBUG_LOG(_raftNode,"Snapshot::installSnapshot already in install snapshot" << endl);
    //        return false;
    //    }

    //只有leader才会通知其他节点安装快照

    std::lock_guard<std::mutex> lock(getTakeSnapshotLock());

    Clourse c(getIsInstallSnapshot());

    peer->setStatus(PS_INSTALLING_SNAPSHOT);

    vector<Snapshot::SnapshotFile> data = getSnapshotFiles(maxSnapshotBytesPerRequest);

    try
    {
        RAFT_DEBUG_LOG(_raftNode,"begin install snapshot serverId:" << peer->getServer().serverId
                                                    << ", file count:" << data.size()
                                                    << ", snapshot:" << getMetaData()->writeToJsonString() << endl);

        InstallSnapshotRequest request;
        request.snapshotMetaData = *(getMetaData().get());
        request.isFirst          = false;
        request.isLast           = false;
        request.term             = currentTerm;
        request.serverId         = localServerId;

        for (size_t i = 0; i < data.size(); i++)
        {
            RAFT_DEBUG_LOG(_raftNode,"sync serverId:" << peer->getServer().serverId << ", file:"
                                                                 << data[i].fileName << ", size:" << data[i].totalSize << ",isDir:" <<request.isDir  << endl);

            //目录传输或者空文件
            if (data[i].isDir || (data[i].trans.size() ==0) )
            {
                request.isFirst  = (i == 0);
                request.isLast   = (i == data.size() - 1);
                request.fileName = TC_Common::replace(data[i].fileName ,getSnapshotDataDir() + FILE_SEP ,"");
                request.isDir = data[i].isDir;
    
                InstallSnapshotResponse response;
                peer->installSnapshot(request, response);
    
                if (response.resCode != RES_CODE_SUCCESS)
                {
                    RAFT_ERROR_LOG(_raftNode,"sync serverId: " << peer->getServer().serverId <<" install failed! " << response <<endl);
                    return false;
                }
    
                RAFT_DEBUG_LOG(_raftNode,"install :" << peer->getServer().serverId << ", file:" << request.fileName << ", dir:" << request.isDir   << endl);
            }
            //文件内容传输
            else
            {
                fstream f(data[i].fileName);
                for (size_t j = 0; j < data[i].trans.size(); j++)
                {
                    SnapshotTrans &tran = data[i].trans[j];
    
                    request.isFirst  = (i == 0) && (j == 0);
                    request.isLast   = (i == data.size() - 1) && (j == data[i].trans.size() - 1);
                    request.fileName = TC_Common::replace(data[i].fileName ,getSnapshotDataDir()  + FILE_SEP ,"");
                    request.offset   = tran.pos;
                    request.isDir = false;
                    
                    request.data.resize(tran.length);
                    f.seekg(request.offset);
                    f.read(&request.data[0], tran.length);
    
                    InstallSnapshotResponse response;
                    peer->installSnapshot(request, response);
    
                    //如果中途有安装失败，应该直接推出
                    if (response.resCode != RES_CODE_SUCCESS)
                    {
                        //break;
                        RAFT_ERROR_LOG(_raftNode, "sync serverId: " << peer->getServer().serverId << " install failed! " << response << endl);
                        return false;
                    }

                    RAFT_DEBUG_LOG(_raftNode, "install succ :" << peer->getServer().serverId << ", file:" << request.fileName << ", offset:" << request.offset << ",size:" << tran.length << endl);
                }
            }
        }
    }
    catch (exception &ex)
    {
        RAFT_ERROR_LOG(_raftNode, "server, serverId:" << peer->getServer().endPoint << ", error:" << ex.what() << endl);
        return false;
    }

    //同步快照到另一个节点成功,
    peer->setNextIndex(getMetaData()->lastIncludedIndex + 1);

    RAFT_DEBUG_LOG(_raftNode, "end install snapshot succ, serverId:" << peer->getServer().serverId << ", nextIndex:" << peer->getNextIndex() << endl);
    return true;
}

//leader节点过来的要求当前节点安装快照的请求
void Snapshot::onInstallSnapshot(const raft::InstallSnapshotRequest &request, InstallSnapshotResponse &rsp)
{
    try
    {
        std::lock_guard<std::mutex> lock(getTakeSnapshotLock());

        getIsInstallSnapshot() = true;

        {
            std::unique_lock<std::mutex> lock(_mutex);

            // 写快照文件数据到一个临时目录
            string currentDataDirName = getSnapshotTmpDir();

            if (request.isFirst)
            {
		        _raftNode->getStateMachine()->onBeginSyncShapshot();

                //删除选举定时器, 因为安装快照时, 没有心跳, 这样保证不会心跳超时从而重新选举
                _raftNode->removeElectionTimer();

                RAFT_DEBUG_LOG(_raftNode,"begin accept snapshot request from serverId:" << request.serverId << endl);
                //是第一个请求, 把临时目录都删除
                TC_File::removeFile(currentDataDirName, true);
                if (!TC_File::makeDirRecursive(currentDataDirName))
                {
                    RAFT_ERROR_LOG(_raftNode, "create dir failed:" << currentDataDirName << endl);
                    rsp.resCode = RES_CODE_FAIL;
                    return;
                }
            }

            //具体数据文件
            string currentDataFileName = currentDataDirName + FILE_SEP + request.fileName;

            RAFT_DEBUG_LOG(_raftNode,"accept snapshot request from serverId:" << request.serverId
                                                                << ", fileName:"<< currentDataFileName
                                                                << ", size:"<< request.data.size()
                                                                << ", isFirst:"<< request.isFirst
                                                                << ", isLast:"<< request.isLast
                                                                << ", isDir:" << request.isDir
                                                                << endl);
            if (request.isDir)
            {
                bool bret = TC_File::makeDirRecursive(currentDataFileName);
                if (!bret)
                {
                    RAFT_ERROR_LOG(_raftNode, "create dir failed:" << currentDataFileName << endl);
                    rsp.resCode = RES_CODE_FAIL;
                    return;
                }
                else
                {
                    RAFT_DEBUG_LOG(_raftNode,"create dir succ:" << currentDataFileName << endl);
                }
            }
            else
            {
                fstream f;
                f.open(currentDataFileName, std::fstream::in | std::fstream::out | std::fstream::app);
                if (!f)
                {
                    RAFT_ERROR_LOG(_raftNode,"open file fail :" << currentDataFileName << endl);
                    rsp.resCode = RES_CODE_FAIL;
                    return;
                }

                f.seekg(request.offset);
                f.write(request.data.data(), request.data.size());
                f.close();
            }
            if (request.isLast)
            {
                RAFT_DEBUG_LOG(_raftNode,"last  snapshot request from serverId:" << request.serverId << ",move dir from :" <<currentDataDirName << "->" << getSnapshotDataDir() << endl);
                //最后一个请求, 把临时目录移动到正式目录
                TC_File::removeFile(getSnapshotDataDir(), true);

                int rret = TC_File::renameFile(currentDataDirName, getSnapshotDataDir());
                if (rret != 0)
                {
                    RAFT_ERROR_LOG(_raftNode,"rename file fail :" << currentDataDirName  << "-->"<< getSnapshotDataDir()  << endl);
                    rsp.resCode = RES_CODE_FAIL;
                    return;
                }
            }

            rsp.resCode = RES_CODE_SUCCESS;
        }

        if (request.isLast && rsp.resCode == RES_CODE_SUCCESS)
        {
            RAFT_DEBUG_LOG(_raftNode,"end accept install snapshot request from server:" << request.serverId << ", lastIncludedIndex:" << request.snapshotMetaData.lastIncludedIndex << endl);

            // 把快照应用到状态机中
	        if(!_raftNode->getStateMachine()->onLoadSnapshot(getSnapshotDataDir()))
            {
                RAFT_ERROR_LOG(_raftNode,"onLoadSnapshot fail :" << getSnapshotDataDir() << endl);
                rsp.resCode = RES_CODE_FAIL;
                return;
            }

            //安装快照成功
            _raftNode->installSnapshotSucc(request.snapshotMetaData);
        }

        if (request.isLast)
        {
            getIsInstallSnapshot() = false;

	        _raftNode->getStateMachine()->onEndSyncShapshot();

            _raftNode->resetElectionTimer();
        }
    }
    catch (exception &ex)
    {
        rsp.resCode = RES_CODE_FAIL;
        RAFT_ERROR_LOG(_raftNode,"error:" << ex.what() << endl);
    }
}

void Snapshot::takeSnapshot()
{
    do
    {
        try
        {
            int64_t ms = TNOWMS;

            RAFT_DEBUG_LOG(_raftNode,"Snapshot::takeSnapshot" << endl);

            //正在让其他节点install snapshot, 当前就不能做了
            if (getIsInstallSnapshot())
            {
                RAFT_DEBUG_LOG(_raftNode,"already in install snapshot, ignore take snapshot" << endl);
                break;
            }

            std::lock_guard<std::mutex> lock(getTakeSnapshotLock());

            int64_t localLastAppliedIndex;
            int64_t lastAppliedTerm = 0;

            //检查是否需要做快照
            if (!_raftNode->checkTakeSnapshot(localLastAppliedIndex, lastAppliedTerm))
            {
                break;
            }

            //创建快照文件
            int64_t lastSnapshotIndex = createSnapshot(localLastAppliedIndex, lastAppliedTerm, _raftNode->getConfiguration());

            RAFT_DEBUG_LOG(_raftNode,"succ lastSnapshotIndex:" << lastSnapshotIndex << endl);

            //删除事前的Log
            _raftNode->truncatePrefix(lastSnapshotIndex);

            RAFT_DEBUG_LOG(_raftNode,"succ, cost: " << TNOWMS - ms << " ms" << endl);
        }
        catch (exception &ex)
        {
            RAFT_ERROR_LOG(_raftNode,"error:" << ex.what() << endl);
        }
        break;

    } while (true);

    //    _raftNode->resetTakeSnapshot();
}
