//
// Created by jarod on 2019-08-08.
//

#ifndef LIBRAFT_UNITTEST_SERVER_H
#define LIBRAFT_UNITTEST_SERVER_H

#include "servant/Application.h"
#include "RaftNode.h"
#include "RaftServer.h"
#include "UnitTestImp.h"

using namespace tars;

class UnitTestStateMachine;
class RaftNode;

class UnitTestServer : public RaftServer<UnitTestStateMachine, UnitTestImp>
{
public:

	/**
	 * 服务初始化
	 **/
	virtual void initialize();

	/**
	 * 服务销毁
	 **/
	virtual void destroyApp();
};


#endif //LIBRAFT_KVSERVER_H
