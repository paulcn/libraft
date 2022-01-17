//
// Created by jarod on 2019-06-06.
//

#ifndef LIBRAFT_EXAMPLESTATEMACHINE_H
#define LIBRAFT_EXAMPLESTATEMACHINE_H

#include <mutex>
#include "util/tc_thread_rwlock.h"
#include "UnitTest.h"
#include "StateMachine.h"

namespace rocksdb
{
class DB;
class Iterator;
class Comparator;
}

using namespace raft;

class ApplyContext;

class UnitTestStateMachine : public StateMachine
{
public:
    /**
     * 构造
     * @param dataPath
     */
    UnitTestStateMachine(const string &dataPath);

    /**
     * 析构
     */
    virtual ~UnitTestStateMachine();

	/**
     * 对状态机中数据进行snapshot，每个节点本地定时调用
     * @param snapshotDir snapshot数据输出目录
     */
    virtual void onSaveSnapshot(const string &snapshotDir);

    /**
     * 读取snapshot到状态机，节点启动时 或者 节点安装快照后 调用
     * @param snapshotDir snapshot数据目录
     */
    virtual bool onLoadSnapshot(const string &snapshotDir);

    /**
     * 启动时加载数据
     * @return
     */
    virtual int64_t onLoadData();

	/**
     * 将数据应用到状态机
     * @param dataBytes 数据二进制
     * @param appliedIndex, appliedIndex
     * @param callback, 如果是Leader, 且网路请求过来的, 则callback有值, 否则为NULL
     */
	virtual void onApply(const char *buff, size_t length, int64_t appliedIndex, const shared_ptr<ApplyContext> &context);

    /**
     * 变成Leader
     * @param term
     */
    virtual void onBecomeLeader(int64_t term);

    /**
     * 变成Follower
     */
    virtual void onBecomeFollower();

	/**
	 * 节点加入集群(Leader or Follower) & LeaderId 已经设置好!
	 * 此时能够正常对外提供服务了, 对于Follower收到请求也可以转发给Leader了
	 */
	virtual void onJoinCluster();

	/**
	 * 节点离开集群(LeaderId不存在了)
	 * 此时无法正常对外提供服务了, 请求不能发送到当前节点
	 */
	virtual void onLeaveCluster();

    /**
    * 开始从Leader同步快照文件
    */
    virtual void onBeginSyncShapshot();

    /**
     * 结束同步快照
     */
    virtual void onEndSyncShapshot();

    void close();

protected:
    void open(const string &dbDir);

    int getNoLock(const string &key, tars::Int64 &value);

protected:
    string          _raftDataDir;

    rocksdb::DB     *_db = NULL;
};


#endif //LIBRAFT_EXAMPLESTATEMACHINE_H
