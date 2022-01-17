//
// Created by jarod on 2019-05-22.
//

#include "RaftLog.h"
#include "servant/RemoteLogger.h"
#include "util/tc_common.h"
#include "util/tc_file.h"
#include <vector>
#include "RaftOptions.h"
#include "RaftNode.h"
#include "rocksdb/comparator.h"
#include "rocksdb/db.h"
#include "rocksdb/options.h"

using namespace tars;

class KeyComparator : public rocksdb::Comparator
{
  protected:
    void FindShortestSeparator(std::string *, const rocksdb::Slice &) const
    {
    }
    void FindShortSuccessor(std::string *) const
    {
    }

  public:
    int Compare(const rocksdb::Slice &a, const rocksdb::Slice &b) const
    {
        assert(a.size() == sizeof(int64_t));
        assert(b.size() == sizeof(int64_t));

        if (*(int64_t *)a.data() < *(int64_t *)b.data())
            return -1;
        if (*(int64_t *)a.data() > *(int64_t *)b.data())
            return +1;
        return 0;
    }

    // Ignore the following methods for now:
    const char *Name() const
    {
        return "RaftLog.KeyComparator";
    }
};

RaftLog::RaftLog(RaftNode *raftNode) : _raftNode(raftNode)
{
}

RaftLog::~RaftLog()
{
}

void RaftLog::initialize(const RaftOptions &ro)
{
    _logDataDir = ro.dataDir + FILE_SEP + "log";

    TC_File::makeDirRecursive(_logDataDir);

    _raftOptions = ro;

    // open rocksdb data dir
    _options = new rocksdb::Options();
    _options->create_if_missing = true;
    _options->comparator = new KeyComparator();

    std::vector<rocksdb::ColumnFamilyDescriptor> column_families;

    vector<string> columnFamilies;

    rocksdb::Status status = rocksdb::DB::ListColumnFamilies(*_options, _logDataDir, &columnFamilies);

    if (columnFamilies.empty())
    {
        status = rocksdb::DB::Open(*_options, _logDataDir, &_db);
        if (!status.ok())
        {
            throw std::runtime_error(status.ToString());
        }

        status = _db->CreateColumnFamily(rocksdb::ColumnFamilyOptions(), "log-meta", &_logHandle);
        if (!status.ok())
        {
            RAFT_ERROR_LOG(_raftNode, "CreateColumnFamily error:" << status.ToString() << endl);
            throw std::runtime_error(status.ToString());
        }

        status = _db->CreateColumnFamily(rocksdb::ColumnFamilyOptions(), "snapshot-meta", &_snapshotHandle);
        if (!status.ok())
        {
            RAFT_ERROR_LOG(_raftNode, "CreateColumnFamily error:" << status.ToString() << endl);
            throw std::runtime_error(status.ToString());
        }
    }
    else
    {
        std::vector<rocksdb::ColumnFamilyDescriptor> columnFamiliesDesc;
        for (auto &f : columnFamilies)
        {
            rocksdb::ColumnFamilyDescriptor c;
            c.name = f;
            if (c.name == "default")
            {
                c.options.comparator = new KeyComparator();
            }
            columnFamiliesDesc.push_back(c);
        }

        std::vector<rocksdb::ColumnFamilyHandle *> handles;
        status = rocksdb::DB::Open(*_options, _logDataDir, columnFamiliesDesc, &handles, &_db);
        if (!status.ok())
        {
            RAFT_ERROR_LOG(_raftNode, "Open " << _logDataDir << ", error:" << status.ToString() << endl);
            throw std::runtime_error(status.ToString());
        }

        for (auto &h : handles)
        {
            if (h->GetName() == "log-meta")
            {
                _logHandle = h;
            }
            else if (h->GetName() == "snapshot-meta")
            {
                _snapshotHandle = h;
            }
        }

        assert(_logHandle != NULL);
        assert(_snapshotHandle != NULL);
    }

    _logMetaData      = readLogMetaData();
    _snapshotMetaData = readShapshotMetaData();
}

void RaftLog::close()
{
	if(_db)
	{
        if (_logHandle)
        {
            _db->DestroyColumnFamilyHandle(_logHandle);
            _logHandle = NULL;
        }
        if (_snapshotHandle)
        {
            _db->DestroyColumnFamilyHandle(_snapshotHandle);
            _snapshotHandle = NULL;
        }
        _db->Close();
        delete _db;
		_db = NULL;
	}

    if (_options && _options->comparator)
    {
        if (_options->comparator)
        {
            delete _options->comparator;
            _options->comparator = NULL;
        }
        if(_options)
        {
            delete _options;
            _options = NULL;
        }
    }
}

shared_ptr<SnapshotMetaData> RaftLog::readShapshotMetaData()
{
    shared_ptr<SnapshotMetaData> lp;

    std::string key("meta");
    std::string value;

    rocksdb::Status status = _db->Get(rocksdb::ReadOptions(), _snapshotHandle, key, &value);
    if (status.IsNotFound())
    {
        lp                    = std::make_shared<SnapshotMetaData>();
        lp->lastIncludedIndex = 0;
        lp->lastIncludedTerm  = 0;

        return lp;
    }
    else if (!status.ok())
    {
        RAFT_ERROR_LOG(_raftNode, "error:" << status.ToString() << endl);
        throw std::runtime_error(status.ToString());
    }

    lp = std::make_shared<SnapshotMetaData>();

    tars::TarsInputStream<> is;
    is.setBuffer(value.c_str(), value.length());
    lp->readFrom(is);

    return lp;
}

shared_ptr<LogMetaData> RaftLog::readLogMetaData()
{
    shared_ptr<LogMetaData> lp;

    std::string key("meta");
    std::string value;

    rocksdb::Status status = _db->Get(rocksdb::ReadOptions(), _logHandle, key, &value);
    if (status.IsNotFound())
    {
        lp = std::make_shared<LogMetaData>();
        //注意默认firstlog是1
        lp->firstLogIndex = 1;

        return lp;
    }
    else if (!status.ok())
    {
        RAFT_ERROR_LOG(_raftNode, "error:" << status.ToString() << endl);
        throw std::runtime_error(status.ToString());
    }

    lp = std::make_shared<LogMetaData>();

    tars::TarsInputStream<> is;
    is.setBuffer(value.c_str(), value.length());
    lp->readFrom(is);

    return lp;
}

void RaftLog::updateLogMetaData(int64_t currentTerm, int votedFor, int64_t firstLogIndex)
{
    if (currentTerm > 0)
    {
        _logMetaData->currentTerm = currentTerm;
    }

    if (firstLogIndex > 0)
    {
        _logMetaData->firstLogIndex = firstLogIndex;
    }

    if (votedFor > 0)
    {
        _logMetaData->votedFor = votedFor;
    }

    tars::TarsOutputStream<BufferWriterString> os;
    _logMetaData->writeTo(os);

    rocksdb::WriteOptions wOption;
    wOption.sync = _raftOptions.sync;

    rocksdb::Status status = _db->Put(wOption, _logHandle, "meta", os.getByteBuffer());
    if (!status.ok())
    {
        RAFT_ERROR_LOG(_raftNode, "error:" << status.ToString() << endl);
        throw std::runtime_error(status.ToString());
    }
}

int64_t RaftLog::updateSnapshotMetaData(int64_t lastIncludedIndex,
                                        int64_t lastIncludedTerm,
                                        const Configuration &configuration)
{
    _snapshotMetaData->lastIncludedTerm  = lastIncludedTerm;
    _snapshotMetaData->lastIncludedIndex = lastIncludedIndex;
    _snapshotMetaData->configuration     = configuration;

    tars::TarsOutputStream<BufferWriterString> os;
    _snapshotMetaData->writeTo(os);

    rocksdb::WriteOptions wOption;
    wOption.sync = _raftOptions.sync;

    rocksdb::Status status = _db->Put(wOption, _snapshotHandle, "meta", os.getByteBuffer());
    if (!status.ok())
    {
        RAFT_ERROR_LOG(_raftNode, "error:" << status.ToString() << endl);
        throw std::runtime_error(status.ToString());
    }
    return lastIncludedIndex;
}

shared_ptr<LogMetaData> RaftLog::getLogMetaData()
{
    return _logMetaData;
}

shared_ptr<SnapshotMetaData> RaftLog::getSnapshotMetaData()
{
    return _snapshotMetaData;
}

void RaftLog::getEntry(int64_t startIndex, int64_t endIndex, vector<string> &entries)
{
    entries.reserve(endIndex - startIndex + 1);
    rocksdb::Slice key((const char *)&startIndex, sizeof(startIndex));

    auto_ptr<rocksdb::Iterator> it(_db->NewIterator(rocksdb::ReadOptions()));
    it->Seek(key);
    while(it->Valid())
    {
        int64_t index = *(int64_t*)it->key().data();

        if(index >= startIndex && index <= endIndex)
        {
            entries.emplace_back(it->value().ToString());
            it->Next();
        }
        else
        {
            break;
        }
    }
}
void RaftLog::getEntry(int64_t startIndex, int64_t endIndex, list<shared_ptr<LogEntry>> &entries)
{
	rocksdb::Slice key((const char *)&startIndex, sizeof(startIndex));

	auto_ptr<rocksdb::Iterator> it(_db->NewIterator(rocksdb::ReadOptions()));
	it->Seek(key);
	while(it->Valid())
	{
		int64_t index = *(int64_t*)it->key().data();

		if(index >= startIndex && index <= endIndex)
		{
			entries.emplace_back(std::make_shared<LogEntry>(index, it->value().ToString()));
			it->Next();
		}
		else
		{
			break;
		}
	}
}

shared_ptr<LogEntry> RaftLog::getEntry(int64_t index)
{
	shared_ptr<LogEntry> logEntry;

    int64_t firstLogIndex = getFirstLogIndex();
    int64_t lastLogIndex  = getLastLogIndex();

    if (index == 0 || index < firstLogIndex || index > lastLogIndex)
    {
        return logEntry;
    }

    rocksdb::Slice key((const char *)&index, sizeof(index));
    std::string value;

    rocksdb::Status status = _db->Get(rocksdb::ReadOptions(), key, &value);
    if (!status.ok())
    {
        RAFT_ERROR_LOG(_raftNode, "get index:" << index << ", error:" << status.ToString() << endl);
        throw std::runtime_error(status.ToString());
    }

    if (status.IsNotFound())
    {
        return logEntry;
    }

	return std::make_shared<LogEntry>(index, value);
}

int64_t RaftLog::getEntryTerm(int64_t index)
{
	shared_ptr<LogEntry> entry = getEntry(index);
	if(!entry)
	{
		return 0;
	}

	return entry->term();
}

void RaftLog::reset(int64_t firstLogIndex)
{
    int64_t startIndex = _logMetaData->firstLogIndex;
    int64_t endIndex   = getLastLogIndex();
    if (endIndex > 0)
    {
	    rocksdb::Slice begin_key((const char *)&startIndex, sizeof(startIndex));
        ++endIndex;
	    rocksdb::Slice end_key((const char *)&endIndex, sizeof(endIndex));

        rocksdb::WriteOptions options;
        options.sync = _raftOptions.sync;

        rocksdb::Status status = _db->DeleteRange(options, _db->DefaultColumnFamily(), begin_key, end_key);
        if (!status.ok())
        {
            RAFT_ERROR_LOG(_raftNode, "firstLogIndex: " << firstLogIndex << ", DeleteRange error:" << status.ToString() << endl);
            throw std::runtime_error(status.ToString());
        }
    }

    updateLogMetaData(0, 0, firstLogIndex);

    // TLOG_DEBUG("firstLogIndex:" << firstLogIndex << ", lastLogIndex:" << getLastLogIndex() << endl);
}

int64_t RaftLog::getFirstLogIndex()
{
    return _logMetaData->firstLogIndex;
}

int64_t RaftLog::getLastLogIndex()
{
    rocksdb::Iterator *it = _db->NewIterator(rocksdb::ReadOptions());
    assert(it != NULL);

    it->SeekToLast();
    if (it->Valid())
    {
        int64_t index = *(int64_t*)it->key().data();
        delete it;
        return index;
    }

    delete it;
    // 有两种情况segment为空
    // 1、第一次初始化，firstLogIndex = 1，lastLogIndex = 0
    // 2、snapshot刚完成，日志正好被清理掉，firstLogIndex = snapshotIndex + 1， lastLogIndex = snapshotIndex
    return getFirstLogIndex() - 1;
}

int64_t RaftLog::append(const shared_ptr<LogEntry> &entry)
{
    int64_t newLastLogIndex = getLastLogIndex() + 1;

	rocksdb::Slice key((const char *)&newLastLogIndex, sizeof(int64_t));

    rocksdb::WriteBatch batch;
    batch.Put(_db->DefaultColumnFamily(), key, entry->value());

    if (_logMetaData->currentTerm != entry->term())
    {
        _logMetaData->currentTerm = entry->term();

        tars::TarsOutputStream<BufferWriterString> os;
        _logMetaData->writeTo(os);

        batch.Put(_logHandle, "meta", os.getByteBuffer());
    }

    rocksdb::WriteOptions wOption;
    wOption.sync = _raftOptions.sync;

    rocksdb::Status status = _db->Write(wOption, &batch);
    if (!status.ok())
    {
        RAFT_ERROR_LOG(_raftNode, "error:" << status.ToString() << endl);
        throw std::runtime_error(status.ToString());
    }

    return newLastLogIndex;
}

int64_t RaftLog::append(const list<shared_ptr<LogEntry>> &entries)
{
    int64_t newLastLogIndex = getLastLogIndex();

    rocksdb::WriteBatch batch;

    for (auto &entry : entries)
    {
        ++newLastLogIndex;

	    rocksdb::Slice key((const char *)&newLastLogIndex, sizeof(int64_t));

        batch.Put(_db->DefaultColumnFamily(), key, entry->value());
    }

    if (!entries.empty())
    {
        if (_logMetaData->currentTerm != (*entries.rbegin())->term())
        {
            _logMetaData->currentTerm = (*entries.rbegin())->term();

            tars::TarsOutputStream<BufferWriterString> os;
            _logMetaData->writeTo(os);

            batch.Put(_logHandle, "meta", os.getByteBuffer());
        }
    }

    rocksdb::WriteOptions wOption;
    wOption.sync = _raftOptions.sync;
    rocksdb::Status status = _db->Write(wOption, &batch);
    if (!status.ok())
    {
        RAFT_ERROR_LOG(_raftNode, "error:" << status.ToString() << endl);
        throw std::runtime_error(status.ToString());
    }

    return newLastLogIndex;
}

void RaftLog::truncatePrefix(int64_t newFirstIndex)
{
    int64_t oldFirstIndex = getFirstLogIndex();

    if (newFirstIndex <= oldFirstIndex)
    {
        return;
    }

    int64_t startIndex = 0;
	rocksdb::Slice begin_key((const char *)&startIndex, sizeof(startIndex));
	rocksdb::Slice end_key((const char *)&newFirstIndex, sizeof(newFirstIndex));

    rocksdb::WriteOptions wOption;
    wOption.sync = _raftOptions.sync;

    rocksdb::Status status = _db->DeleteRange(wOption, _db->DefaultColumnFamily(), begin_key, end_key);
    if (!status.ok())
    {
        RAFT_ERROR_LOG(_raftNode, "DeleteRange error:" << status.ToString() << endl);
        throw std::runtime_error(status.ToString());
    }

    updateLogMetaData(0, 0, newFirstIndex);

	rocksdb::FlushOptions fOptions;
	fOptions.wait = false;
	_db->Flush(fOptions);
}

void RaftLog::truncateSuffix(int64_t newEndIndex)
{
    if (newEndIndex >= getLastLogIndex())
    {
        return;
    }

    int64_t nextIndex = newEndIndex + 1;
    int64_t lastIndex = std::numeric_limits<int64_t>::max();

	rocksdb::Slice begin_key((const char *)&nextIndex, sizeof(nextIndex));
	rocksdb::Slice end_key((const char *)&lastIndex, sizeof(lastIndex));

    rocksdb::WriteOptions wOption;
    wOption.sync = _raftOptions.sync;

    rocksdb::Status status = _db->DeleteRange(wOption, _db->DefaultColumnFamily(), begin_key, end_key);
    if (!status.ok())
    {
        RAFT_ERROR_LOG(_raftNode, "DeleteRange error:" << status.ToString() << endl);
        throw std::runtime_error(status.ToString());
    }
}
