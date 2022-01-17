//
// Created by jarod on 2019-05-22.
//

#ifndef LIBRAFT_RAFTLOG_H
#define LIBRAFT_RAFTLOG_H


#include <string>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include "tup/tup.h"
#include "Raft.h"
#include "RaftOptions.h"
#include <fstream>

using namespace std;
using namespace raft;

namespace rocksdb
{
class DB;
class Iterator;
class Comparator;
class ColumnFamilyHandle;
struct Options;
struct ColumnFamilyDescriptor;
}

class RaftOptions;
class RaftNode;

class LogEntry
{
public:

//	LogEntry(int64_t index, string &value) : _index(index)
//	{
//		//减少一次内存copy
//		_value.swap(value);
//	}

	LogEntry(int64_t index, const string &value) : _index(index), _value(value)
	{
		assert(_value.size() >= sizeof(int64_t) + sizeof(EntryType));
	}

	LogEntry(int64_t term, raft::EntryType type, const string &data)
	{
		_value.reserve(data.size() + sizeof(int64_t) + sizeof(EntryType));

		_value.append((const char *)&term, sizeof(term));
		_value.append((const char *)&type, sizeof(type));
		_value.append(data.data(), data.size());
	}

	string &value() { return _value; }

	const string &value() const { return _value; }

	int64_t index() const { return _index; }

	int64_t term() const { return *(int64_t*)_value.c_str(); }

	raft::EntryType type() const { return *(raft::EntryType*)(_value.c_str() + sizeof(int64_t)); }

	pair<const char*, size_t> log() const { return std::make_pair(_value.c_str() + sizeof(int64_t) + sizeof(EntryType), _value.size() - (sizeof(int64_t) + sizeof(EntryType))); }

protected:
	int64_t _index;
	string  _value;
};

class RaftLog
{
public:
    //构造
    RaftLog(RaftNode *raftNode);
    //析构
    ~RaftLog();
    //根据大小初始化log文件
    void initialize(const RaftOptions &ro);

    //关闭db
    void close();

    //根据log index获取日志的具体信息
    shared_ptr<LogEntry> getEntry(int64_t index);

    //获取startIndex 到 endIndex的数据
	void getEntry(int64_t startIndex, int64_t endIndex, list<shared_ptr<LogEntry>> &entries);

	//获取startIndex 到 endIndex的数据
	void getEntry(int64_t startIndex, int64_t endIndex, vector<string> &entries);

	//根据log index获取那日志的term
    int64_t getEntryTerm(int64_t index);

    //获取第一条log的index
    int64_t getFirstLogIndex();

    //获取最后一条log的index, 如果是空则返回0
    int64_t getLastLogIndex();

    //重置Log索引
    void reset(int64_t firstLogIndex);

    //添加log, 写入到文件中
    int64_t append(const shared_ptr<LogEntry> &entry);
    //批量添加log, 写入到文件中
    int64_t append(const list<shared_ptr<LogEntry>> &entries);

    //删除newFirstIndex之前的文件
    void truncatePrefix(int64_t newFirstIndex);

    //删除newEndIndex之后的文件
    void truncateSuffix(int64_t newEndIndex);

    //更新元数据
    void updateLogMetaData(int64_t currentTerm, int votedFor, int64_t firstLogIndex);

    //更新快照的meta
    int64_t updateSnapshotMetaData(int64_t lastIncludedIndex, int64_t lastIncludedTerm, const Configuration &configuration);

    //获取元数据
    shared_ptr<LogMetaData> getLogMetaData();

    shared_ptr<SnapshotMetaData> getSnapshotMetaData();

protected:

    shared_ptr<LogMetaData> readLogMetaData();
    shared_ptr<SnapshotMetaData> readShapshotMetaData();

protected:
    RaftNode                        *_raftNode;
    string                          _logDataDir;            //log data dir路径
    shared_ptr<LogMetaData>         _logMetaData;           //记录当然log文件的基本信息
    shared_ptr<SnapshotMetaData>    _snapshotMetaData;
    rocksdb::ColumnFamilyHandle *_logHandle;
    rocksdb::ColumnFamilyHandle     *_snapshotHandle;
    rocksdb::DB                     *_db = NULL;

    rocksdb::Options *_options;

    RaftOptions _raftOptions;
};

#endif //LIBRAFT_RAFTLOG_H

