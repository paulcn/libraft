//
// Created by jarod on 2019-06-06.
//

#include "UnitTestStateMachine.h"
#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/utilities/checkpoint.h"
#include "servant/Application.h"
#include "RaftNode.h"
#include "UnitTest.h"

UnitTestStateMachine::UnitTestStateMachine(const string &dataPath) : _raftDataDir(dataPath)
{
}

UnitTestStateMachine::~UnitTestStateMachine()
{
    close();
}

//void UnitTestStateMachine::initialize(const shared_ptr<RaftNode> &raftNode, const string &dataPath)
//{
//	_raftNode = raftNode;
//	_raftDataDir = dataPath;
//}
//
//void UnitTestStateMachine::initialize(const string &dataPath)
//{
//	_raftDataDir = dataPath;
//}

void UnitTestStateMachine::open(const string &dbDir)
{
    TLOG_DEBUG("db: " << dbDir << endl);

    tars::TC_File::makeDirRecursive(dbDir);

    // open rocksdb data dir
    rocksdb::Options options;
    options.create_if_missing = true;
    rocksdb::Status status = rocksdb::DB::Open(options, dbDir, &_db);
    if (!status.ok()) {
        throw std::runtime_error(status.ToString());
    }
}

void UnitTestStateMachine::close()
{
    if (_db) {
        _db->Close();
        delete _db;
        _db = NULL;
    }
}

void UnitTestStateMachine::onJoinCluster()
{
	auto raftNode = _raftNode.lock();
	if(!raftNode)
	{
		return;
	}

//	LOG_CONSOLE_DEBUG << "onJoinCluster: " << raftNode->getLocalServer().endPoint << endl;

}

void UnitTestStateMachine::onLeaveCluster()
{
	auto raftNode = _raftNode.lock();
	if(!raftNode)
	{
		return;
	}
//	LOG_CONSOLE_DEBUG << "onLeaveCluster: " << raftNode->getLocalServer().endPoint << endl;
}

void UnitTestStateMachine::onBecomeLeader(int64_t term)
{
	auto raftNode = _raftNode.lock();
	if(!raftNode)
	{
		return;
	}

//	LOG_CONSOLE_DEBUG << "onBecomeLeader: " << raftNode->getLocalServer().endPoint << ", term:" << term << endl;
	TLOG_DEBUG("term:" << term << endl);
}

void UnitTestStateMachine::onBecomeFollower()
{
	auto raftNode = _raftNode.lock();
	if(!raftNode)
		return;

//	LOG_CONSOLE_DEBUG << "onBecomeFollower: " << raftNode->getLocalServer().endPoint << endl;

    TLOG_DEBUG("onBecomeFollower" << endl);
}

void UnitTestStateMachine::onBeginSyncShapshot()
{
    TLOG_DEBUG("onBeginSyncShapshot" << endl);
}

void UnitTestStateMachine::onEndSyncShapshot()
{
    TLOG_DEBUG("onEndSyncShapshot" << endl);
}

int64_t UnitTestStateMachine::onLoadData()
{
	TLOG_DEBUG("onLoadData" << endl);

	string dataDir = _raftDataDir + FILE_SEP + "rocksdb_data";

	TC_File::makeDirRecursive(dataDir);

	//把正在使用的db关闭
	close();

	open(dataDir);

	int64_t lastAppliedIndex = 0;

	string value;
	auto s = _db->Get(rocksdb::ReadOptions(), "lastAppliedIndex", &value);
	if(s.ok())
	{
		lastAppliedIndex = *(int64_t*)value.c_str();

	}
	else if(s.IsNotFound())
	{
		lastAppliedIndex = 0;
	}
	else if(!s.ok())
	{
		TLOG_ERROR("Get lastAppliedIndex error!" << s.ToString() << endl);
		exit(-1);
	}

	TLOG_DEBUG("lastAppliedIndex:" << lastAppliedIndex << endl);
//	LOG_CONSOLE_DEBUG << "lastAppliedIndex:" << lastAppliedIndex << endl;

	return lastAppliedIndex;
}

void UnitTestStateMachine::onSaveSnapshot(const string &snapshotDir)
{
    TLOG_DEBUG("onSaveSnapshot:" << snapshotDir << endl);

    rocksdb::Checkpoint *checkpoint = NULL;

    rocksdb::Status s = rocksdb::Checkpoint::Create(_db, &checkpoint);

    assert(s.ok());

    checkpoint->CreateCheckpoint(snapshotDir);

    delete checkpoint;
}

bool UnitTestStateMachine::onLoadSnapshot(const string &snapshotDir)
{
    string dataDir = _raftDataDir + FILE_SEP + "rocksdb_data";

    //把正在使用的db关闭
    close();

    //非启动时(安装节点)
    TC_File::removeFile(dataDir, true);
    TC_File::makeDirRecursive(dataDir);

    TLOG_DEBUG("copy: " << snapshotDir << " to " << dataDir << endl);

    //把快照文件copy到数据目录
    TC_File::copyFile(snapshotDir, dataDir);

    onLoadData();
    
    return true;
}

void UnitTestStateMachine::onApply(const char *buff, size_t length, int64_t appliedIndex, const shared_ptr<ApplyContext> &callback)
{
	auto raftNode = _raftNode.lock();
	if(!raftNode)
	{
		return;
	}
//	cout << "onApply:" << raftNode->getLocalServer().endPoint << ", appliedIndex:" << appliedIndex << endl;

    TarsInputStream<> is;
    is.setBuffer(buff, length);

    CountReq req;
    req.readFrom(is);

    string key = req.sBusinessName + "-" + req.sKey;
    // int64_t ms = TNOWMS;

    CountRsp rsp;

    rsp.iRet = getNoLock(key, rsp.iCount);
	// TLOG_DEBUG("UnitTestStateMachine::onApply getNoLock ms: " << ", " << TNOWMS - ms << ", isLeader:" << this->isLeader()<< endl);

    if(rsp.iRet != RT_SUCC)
    {
        rsp.sMsg = "get data from rocksdb error!"; 
        // TLOG_ERROR("UnitTestStateMachine::onApply getNoLock req: " << req.writeToJsonString() << ", rsp:" << rsp.iRet << ", isLeader:" << this->isLeader()<< endl);
    }
    else
    {
        rsp.iCount = rsp.iCount + req.iNum;

        string sCount = TC_Common::tostr(rsp.iCount);

        rocksdb::WriteBatch batch;
        batch.Put(key, sCount);
        batch.Put("lastAppliedIndex", rocksdb::Slice((const char *)&appliedIndex, sizeof(appliedIndex)));

	    rocksdb::WriteOptions wOption;
	    wOption.sync = false;

        auto s = _db->Write(wOption, &batch);

//	    TLOG_DEBUG("onApply:" << appliedIndex << endl);

        if(!s.ok())
        {
            rsp.iRet = RT_APPLY_ERROR;
            rsp.sMsg = "save data to rocksdb error!";
            TLOG_ERROR("Put: key:" << key << ", error!" << endl);
            exit(-1);
        }
    }

    if(callback && callback->getCurrentPtr())
    {
        //如果客户端请求过来的, 直接回包
        //如果是其他服务器同步过来, 不用回包了
	    Count::async_response_count(callback->getCurrentPtr(), rsp.iRet, rsp);
        // TLOG_DEBUG("-----------CountImp::async_response_count:" << TNOWMS - req.tTimestamp  << endl);
    }
}

int UnitTestStateMachine::getNoLock(const string &key, tars::Int64 &count)
{
    std::string value;
    rocksdb::Status s = _db->Get(rocksdb::ReadOptions(), key, &value);
    if (s.ok()) 
    {
        count = TC_Common::strto<tars::Int64>(value); 
    }
    else if (s.IsNotFound()) {
        count = 1;
    }
    else
    {
        TLOG_ERROR("Get: " << key << ", error:" << s.ToString() << endl);

        return RT_DATA_ERROR;
    }

    return RT_SUCC;
}
