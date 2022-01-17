
# raft核心说明
- raft协议简单的说就是可以提供分布式数据一致性.
- libraft是基于taf实现的raft库, 底层rpc通讯依赖rpc
- 可以基于libraft, 你可以快速实现自己的业务服务, 完成数据的分布式一致性, 极大的提高了类似业务的开发效率
- 部署在一台服务器上, 三个node节点, 支持 > 1w 的tps
- 每个raft节点都有一个serverId, 目前是raft库自己计算的, 采用ip+port, 然后hash计算得到

# raft库安装说明
- 源码目下: mkdir build; cd build; cmake ..; make install
- 它依赖了rocksdb/snappy, 编译时会自动下载rocksdb/snapppy的包, 并编译
- rocksdb主要用于服务间的日志同步相关处理
- make install后
>- libraft/librafttest默认安装在 /usr/local/taf/raft 目录下
>- rocksdb/snappy默认安装在 /usr/local/taf/raft/thirdparty/ 目下下
- 如果业务服务也需要依赖rocksdb/snappy, 可以依赖这个目录下的rocksdb/snappy

# 如何基于raft实现业务服务
## 代码架构基础说明
- 实例代码可以参考: test/src 这是一个计数服务, 写业务服务代码之前, 请务必参考!!!
- 每个业务服务需要绑定搞两个端口, 一个raft端口, 一个buss端口(业务服务端口)
- raft端口对应RaftImp(libraft库提供), 业务开发不用管理
- buss端口对应业务服务, 自己实现, 目前模式只支持一个业务处理servant

## 状态机说明
- raft本质上就是一个状态机, 简单的说:
>- 接受请求(日志)
>- 按照顺序记录本地日志应用到状态机
>- 同步到其他节点
>- 当大多数节点都复制成功以后, 回调状态机, 形成业务数据
>- 各节点定期会生成业务快照, 这个实现由业务自己实现

这个过程中, 对业务服务而言, 需要注意:
- lastAppliedIndex 参数, 这个参数表示日志的索引
- 当应用成本地业务数据以后, 也需要将该数据存储起来, 最好也业务数据一起存, 保证数据一致性
- 当服务重启恢复时, 需要加载这个lastAppliedIndex (StateMachine::onLoadData中返回, 业务需要实现!)

## 服务的实现
- raft本质上就是一个状态机, 简单的说就是: 接受请求(日志), 应用到状态机, 形成业务数据
- 因此每个业务服务都需要继承libraft中的状态机基类(StateMachine)
- 业务服务初始化时, 完成initialize初始化, 重点是初始化
>- RaftOptions, 配置参数, 可以配置心跳时间, 选举实现等参数
>- RaftNode::NodeInfo, 多机节点信息
>- RaftNode: raftnode信息
>- YourStateMachine: 状态机, 你需要继承libraft中的StateMachine, 实现相关接口, 重要的接口包括以下几个:
>>- onLoadData: 启动时加载数据用, 返回当前数据的lastAppliedIndex
>>- onLoadSnapshot: 加载快照数据, 新加入节点时, 从leader同步数据后, 回调完成
>>- onSaveSnapshot: 生成快照数据, 业务实现(countserver中使用rocksdb作为存储的原因就是因为, rocksdb生成快照很快, 它有checkpoint机制!!)
>>- replicate: leader接收数据后, 主动调用该函数, 将数据赋值到其他节点, 这个接口中ApplyContext相当于上下文
>>- onApply: 当大多数节点都接收日志后, 会回调该接口(业务服务在这个回调接口中, 完成处理比如给客户端回包之类的), 这个服务会把replicate中的ApplyContext带过去
>>- doForward: 对于业务服务中, 如果非leader节点收到请求, 但是需要转发到leader处理的, 可以通过该函数自动转发给leader, 当业务服务的客户端连接到业务服务时, 请求如果不是到Leader, 业务服务自己通过doForward转发到leader来完成服务的转发

### onLoadSnapshot和onLoadData的关联和区别
- snapshot和data都是业务数据，都需要自己按照对应格式存储。但两者有一些区别
>- snapshot由框架触发存储，并且只能按照文件方式持久化(因为增加新节点会自动从leader中copy快照文件到新的节点)
>- onLoadSnapshot 只会当新的机器加入集群后，新机器与Leader之间的差距太大，才会调用此函数并从Leader拷贝文件到新机器
>- 只要服务重启都会调用onLoadData加载数据, onLoadData需要返回lastAppliedIndex 


## 自动测试

- 为了方便业务做基本的自动测试, 框架已经提供了自动测试的lib, 这样业务代码很简单的实现即可完成基本的自动测试
- 自动测试是将多个服务都放在一个进程中运行, 这样方便控制服务的启停, 来模拟真实情况
- 自动测试框架已经提供了核心测试逻辑, 参考RaftTest.h, 业务代码只需要实现基本的功能接口接口(RaftTest::setBussFunc)

业务如果希望实现自动测试, 需要注意以下方面:

### RaftServer
- 业务服务需要继承RaftServer即可, 注意: RaftServer基类中已经包含了RaftNode和StateMachine对象, 初始化完服务后, 直接使用即可
- RaftServer为模板基类, 其中模板参数:
>- S表示实际业务服务的StateMachine类
>- I表示业务的Imp处理类
- 业务服务必须实现initialize, 且注意必须调用RaftServer::onInitializeRaft, 来初始化raft
- 业务服务必须实现destroyApp, 且注意必须调用RaftServer::onDestroyRaft, 来释放raft
- 注意: 在通常RaftServer::onDestroyRaft之前, 必须先调用状态机的资源释放(根据各状态机自己来决定)
- 注意: onInitializeRaft最后一个参数是业务StateMachine的构造函数的参数, 这样可以传递参数到自己的StateMachine中
- 在initiallize中初始化数据路径时, 注意要考虑增加index做标示符, 避免自动测试时数据目录相同

### Imp处理类  
- 为了支持自动测试, Imp处理类不能直接使用全局RaftServer中的RaftNode和StateMachine (自动测试会在一个进程启动多个服务, 如果是全局的无法区分!!), 获取方式如下:
>- _raftNode = ((UnitTestServer*)this->getApplication())->node()
>- _stateMachine = ((StorageServer*)this->getApplication())->getStateMachine()

### StateMachine
- raft的状态机, 业务服务需要继承该类, 并实现相关的回调, 尤其纯虚拟函数是必须要实现
- 回调可能在不同线程里面发生, 因此实现回调时需要注意线程安全, 且不要阻塞
- onApply这个函数很关键, 它里面需要把数据保存到本地, leader和follower都会被调用, 如果是leader你还需要回包给客户端(通过context的getCurrentPtr判断是否是leader)
- StateMachine中有一个weak_ptr<RaftNode> _raftNode, 因为RaftNode里面包含了shared_ptr<StateMachine>, 所以这个是weak_ptr, 避免循环引用
- 在继承StateMachine的业务类中, 可以直接使用这个_raftNode
- 业务StateMachine资源释放, 需要自己在服务的destroyApp中调用

自动测试最终会编译成librafttest.a, 给自动测试使用, 业务服务也可以用他快速构建自动测试!!

#todo
peer的status无用, 可以去掉 ??
