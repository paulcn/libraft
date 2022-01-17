//
// Created by jarod on 2019-08-08.
//

#include "UnitTestServer.h"
#include "UnitTestImp.h"
#include "UnitTestStateMachine.h"

void UnitTestServer::initialize()
{
	RaftOptions raftOptions;
	raftOptions.electionTimeoutMilliseconds = 1000;
	raftOptions.heartbeatPeriodMilliseconds = 300;
	raftOptions.dataDir                     = ServerConfig::DataPath + "raft-log-" + TC_Common::tostr(_index);
	raftOptions.snapshotPeriodSeconds       = 60;
	raftOptions.maxLogEntriesPerRequest     = 100;
	raftOptions.maxLogEntriesMemQueue       = 2000;
	raftOptions.maxLogEntriesTransfering    = 1000;

	onInitializeRaft(raftOptions, "UnitTestObj", ServerConfig::DataPath + "unittest-log-" + TC_Common::tostr(_index));
}

void UnitTestServer::destroyApp()
{
	_stateMachine->close();

	onDestroyRaft();
}