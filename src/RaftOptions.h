//
// Created by jarod on 2019-06-02.
//

#ifndef LIBRAFT_RAFTOPTIONS_H
#define LIBRAFT_RAFTOPTIONS_H

/**
 * 心跳时间 < 选举超时时间, 否则会不断发起心跳
 */
class RaftOptions
{
  public:
    // A follower would become a candidate if it doesn't receive any message
    // from the leader in electionTimeoutMs milliseconds
    int electionTimeoutMilliseconds = 300;

    // A leader sends RPCs at least this often, even if there is no data to send
    int heartbeatPeriodMilliseconds = 100;

    // snapshot定时器执行间隔
    int snapshotPeriodSeconds = 3600;

    // 同步快照, 每次传输文件的大小
    int maxSnapshotBytesPerRequest = 500 * 1024;

    // 每次同步日志时, 最大日志条数
    int maxLogEntriesPerRequest = 500;

    // 网络中正在复制的日志的条数
	int maxLogEntriesTransfering = 1000;

	// 从log文件加载到内存中, 等待复制的日志条数(通常大于maxLogEntriesTransfering)
	int maxLogEntriesMemQueue = 2000;

	// leader给peer发送心跳时, peer失败屏蔽peer的时间(秒)
	int peerFailureIsolationTime = 10;

	// follower与leader差距在catchupMargin，才可以参与选举和提供服务
    int64_t catchupMargin = 500;

    // raft的log和snapshot父目录，绝对路径
    string dataDir;

    // 是否同步写log
    bool sync = false;

    // 仅仅进入了集群才监听, 设置为true表示进入集群才监听，false表示启动就立即监听
    bool bindWhenJoinCluster = true;
};

#endif //LIBRAFT_RAFTOPTIONS_H
