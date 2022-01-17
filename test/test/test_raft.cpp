//
// Created by jarod on 2020/2/20.
//

#include "gtest/gtest.h"
#include "util/tc_common.h"
#include "framework/FrameworkServer.h"
#include "UnitTestServer.h"
#include "UnitTest.h"
#include "RaftTest.h"

using namespace tars;

class RaftUnitTest : public testing::Test
{
protected:
	shared_ptr<RaftTest<UnitTestServer>> _raftTest;

public:
	RaftUnitTest()
	{
	}

	~RaftUnitTest()
	{
	}

	static void SetUpTestCase()
    {
	}
    static void TearDownTestCase()
    {

    }

    virtual void SetUp()
    {
		_raftTest = std::make_shared<RaftTest<UnitTestServer>>();
		_raftTest->initialize("raft", "UnitTestServer", "UnitTestObj", "count-log", 25000, 35000);
		_raftTest->setBussFunc(std::bind(&RaftUnitTest::testCount, this, std::placeholders::_1, false));
	}

	virtual void TearDown()
    {
		_raftTest.reset();
    }

	inline void testCount(const shared_ptr<UnitTestServer> &server, bool print = false)
    {
	    try
	    {
		    CountReq req;
		    req.sBusinessName = "app";
		    req.sKey = "autotest";

		    int ret;

		    CountPrx prx = server->node()->getBussLeaderPrx<CountPrx>();

		    CountRsp rsp1;
		    ret = prx->count(req, rsp1);

		    ASSERT_TRUE(ret == 0);

		    CountRsp rsp2;
		    ret = prx->count(req, rsp2);
//		    LOG_CONSOLE_DEBUG << rsp2.writeToJsonString() << endl;

		    ASSERT_TRUE(ret == 0);

		    if(print)
			{
				LOG_CONSOLE_DEBUG << rsp1.writeToJsonString() << endl;
			    LOG_CONSOLE_DEBUG << rsp2.writeToJsonString() << endl;
			}

		    ASSERT_TRUE(rsp1.iCount + 1 == rsp2.iCount);
	    }
	    catch (exception &ex) {
	    	LOG_CONSOLE_DEBUG << ex.what() << endl;
	    }
    }

};


TEST_F(RaftUnitTest, StartServer)
{
	_raftTest->createServers(3, false);

	LOG_CONSOLE_DEBUG << endl;

	_raftTest->startAll();
	LOG_CONSOLE_DEBUG << endl;

	_raftTest->waitLeader();
	LOG_CONSOLE_DEBUG << endl;

	_raftTest->waitCluster();
	LOG_CONSOLE_DEBUG << endl;

	testCount(_raftTest->get(0));
	LOG_CONSOLE_DEBUG << endl;

	_raftTest->execBussFunc();

	LOG_CONSOLE_DEBUG << endl;
//	TC_Common::sleep(1000);

	_raftTest->stopAll();
}

TEST_F(RaftUnitTest, BaseProgress)
{
	_raftTest->testBaseProgress();
}

TEST_F(RaftUnitTest, CheckLeaderNotStopLeader)
{
	_raftTest->testCheckLeaderNotStopLeader();
}

TEST_F(RaftUnitTest, CheckLeaderStopLeader)
{
	_raftTest->testCheckLeaderStopLeader();
}

TEST_F(RaftUnitTest, RestartProgress)
{
	_raftTest->testRestartProgress();
}

TEST_F(RaftUnitTest, LostData)
{
	_raftTest->testLostData();
}

TEST_F(RaftUnitTest, InstallSnapshot)
{
	_raftTest->testInstallSnapshot();
}

TEST_F(RaftUnitTest, StopLeader)
{
	_raftTest->testStopLeader();
}

TEST_F(RaftUnitTest, RaftAddNode)
{
	_raftTest->testRaftAddNode();
}

TEST_F(RaftUnitTest, RaftDelNode)
{
	_raftTest->testRaftDelNode();
}

TEST_F(RaftUnitTest, Benchmark)
{
	_raftTest->testBenchmark();
}

TEST_F(RaftUnitTest, AutoAddPeer)
{
	_raftTest->testAutoAddPeer();
}

TEST_F(RaftUnitTest, AutoDelPeer)
{
	_raftTest->testAutoDelPeer();
}

TEST_F(RaftUnitTest, TestAll)
{
	auto raftTest = std::make_shared<RaftTest<UnitTestServer>>();
	raftTest->initialize("raft", "UnitTestServer", "UnitTestObj", "count-log", 25000, 35000);
	raftTest->setBussFunc(std::bind(&RaftUnitTest::testCount, this, std::placeholders::_1, false));

	raftTest->testAll();
}