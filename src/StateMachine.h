//
// Created by jarod on 2019-05-27.
//

#ifndef LIBRAFT_STATEMACHINE_H
#define LIBRAFT_STATEMACHINE_H

#include <string>
#include <vector>
#include "servant/Application.h"
#include "RaftOptions.h"

using namespace std;
using namespace tars;

class RaftNode;
class ApplyContext;

class StateMachine
{
public:
    virtual ~StateMachine() {}

    /**
     * 对状态机中数据进行snapshot
     * 业务实现该函数时, 需要把快照文件输出到snapshotDir目录
     * 当前其他节点需要同步快照时, 会从Leader的snapshotDir目录同步快照文件过去!
     * 注意: snapshotDir这个目录是raft这个库来管理的, 是RaftOptions.dataDir下面的子目录
     *
     * @param snapshotDir snapshot数据输出目录
     */
    virtual void onSaveSnapshot(const string &snapshotDir) = 0;

    /**
     * 读取snapshot到状态机，节点安装快照后 调用
     * 业务数据是StateMachine调用apply形成的, 可以存在自己指定的目录(注意: 不要存放在RaftOptions.dataDir 目录, 避免冲突)
     *
     * 如果业务用rocksdb来存储实际的数据, 那么通常做法是:
     * 1 实现该函数时, 先把快照目录snapshotDir(RaftOptions.dataDir下的子目录, raft库管理的), copy一份到自己的数据目录, 然后加载
     * 2 后续的log都apply都到这个目录下
     *
     * @param snapshotDir snapshot数据目录
     */
    virtual bool onLoadSnapshot(const string &snapshotDir) = 0;

    /**
     * 服务启动时加载业务数据, 并返回当前业务数据的lastAppliedIndex
     * 如果你的业务服务, 不需要存储数据, 那么返回 0 即可, 这会导致log里面的数据, 都会重新apply一次!
     *
     * @return lastAppliedIndex
     */
    virtual int64_t onLoadData() = 0;

    /**
     * 将数据应用到状态机
     * 注意: 如果你需要保存数据, 则需要dataBytes, appliedIndex 原子化保存, 否则其中一条记录失败, 会引起数据的不一致!
     * 保存的appliedIndex, 当业务启动时, onLoadData回调中加载数据, 并返回最后的appliedIndex
     * 因此, 通常你只需要保存最新的appliedIndex即可!
     *
     * @param dataBytes log二进制数据
     * @param appliedIndex, 本次数据的appliedIndex
     * @param callback, 上下文, 可能为空NULL
     */
    virtual void onApply(const char *buff, size_t length, int64_t appliedIndex, const shared_ptr<ApplyContext> &context) = 0;

    /**
     * 变成leader
     */
    virtual void onBecomeLeader(int64_t term) {}

    /**
     * 变成Follower(注意, 此时LeaderId可能并不是正确的)
     */
    virtual void onBecomeFollower() {}

    /**
     * 开始选举的回调
     * @param term 选举轮数
     */
    virtual void onStartElection(int64_t term){}
    /**
     * 节点加入集群(Leader or Follower) & LeaderId 已经设置好!
     * 此时能够正常对外提供服务了, 对于Follower收到请求也可以转发给Leader了
     */
    virtual void onJoinCluster() {}

    /**
     * 节点离开集群(重新发起投票, LeaderId不存在了)
     * 此时无法正常对外提供服务了, 请求不能发送到当前节点
     */
    virtual void onLeaveCluster() {}

    /**
     * 开始从Leader同步快照文件
     */
    virtual void onBeginSyncShapshot() {}

    /**
     * 结束同步快照
     */
    virtual void onEndSyncShapshot() {}

protected:
	/**
     * 初始化
     * @param raftNode
     */
	void initialize(const shared_ptr<RaftNode> &raftNode) { _raftNode = raftNode; }

	 template<typename S, typename I>
	 friend class RaftServer;

protected:
	weak_ptr<RaftNode> _raftNode;

};

#endif //LIBRAFT_STATEMACHINE_H
