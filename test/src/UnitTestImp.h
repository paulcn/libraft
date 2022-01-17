//
// Created by jarod on 2019-07-22.
//

#ifndef LIBRAFT_COUNTIDIMP_H
#define LIBRAFT_COUNTIDIMP_H

#include "UnitTest.h"

using namespace raft;

class RaftNode;

class UnitTestImp : public Count
{
public:

//	UnitTestImp(const shared_ptr<RaftNode> &raftNode);

    virtual void initialize();

    virtual void destroy();

    virtual int count(const CountReq &req, CountRsp &rsp, tars::CurrentPtr current);

protected:

	shared_ptr<RaftNode>    _raftNode;
};


#endif //LIBRAFT_RAFTCLIENTIMP_H
