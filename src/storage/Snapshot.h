//
// Created by jarod on 2019-05-26.
//

#ifndef LIBRAFT_SNAPSHOT_H
#define LIBRAFT_SNAPSHOT_H

#include <string>
#include <memory>
#include <mutex>
#include <map>
#include <fstream>
#include <atomic>
#include "Raft.h"

using namespace std;
using namespace raft;

class StateMachine;
class RaftLog;
class Peer;
class RaftNode;

class Snapshot
{
public:

    //闭包,析构的时候比如释放take snapshot的标示
    struct Clourse
    {
        Clourse(std::atomic<bool> &value) : _value(value)
        {
        }

        ~Clourse()
        {
            std::atomic_exchange(&_value, false);
        }

        std::atomic<bool> &_value;
    };

    //快照传输
    struct SnapshotTrans
    {
        int pos;            //起始位置
        int length;         //读取长度
    };

    struct SnapshotFile
    {
        string  fileName;               //文件名
        size_t  totalSize;              //文件大小
        bool isDir = false;             //是否是目录
        vector<SnapshotTrans> trans;   //需要传输的分片
    };

    Snapshot(RaftNode *raftNode, const string &raftDataDir, shared_ptr<RaftLog> &raftLog);

    std::mutex &getTakeSnapshotLock() { return _takeSnapshotMutex; }

    //返回智能指针,不加锁
    shared_ptr<SnapshotMetaData> getMetaData();

    //返回具体数据, 加锁
    SnapshotMetaData getMetaDataLock();

    //snapshot data目录
    string getSnapshotDataDir() { return _snapshotDir + ".data"; }

    //snapshot tmp目录
    string getSnapshotTmpDir() { return _snapshotDir + ".tmp"; }

    //leader发起调用, 让peer安装同步快照文件
    bool installSnapshot(const shared_ptr<Peer> &peer, int maxSnapshotBytesPerRequest, int localServerId, int64_t currentTerm);

    //对端peer接受安装快照的请求
    void onInstallSnapshot(const raft::InstallSnapshotRequest & request, InstallSnapshotResponse &response);

    //创建快照
    int64_t createSnapshot(int64_t lastIncludedIndex,
                        int64_t lastIncludedTerm,
                        const Configuration &configuration);

    std::atomic<bool> &getIsInstallSnapshot() { return _isInstallSnapshot; }

    void takeSnapshot();

protected:
    //分析需要传输的文件信息
    vector<SnapshotFile> getSnapshotFiles(int maxSnapshotBytesPerRequest);

protected:
    RaftNode            *_raftNode;
    string              _snapshotDir;
    shared_ptr<RaftLog> _raftLog;
    std::atomic<bool>   _isInstallSnapshot{false};
    std::mutex          _mutex;
    std::mutex          _takeSnapshotMutex;
};


#endif //LIBRAFT_SNAPSHOT_H
