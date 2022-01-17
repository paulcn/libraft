//
// Created by jarod on 2019-07-22.
//

#include "UnitTestImp.h"
#include "RaftNode.h"
#include "UnitTestServer.h"
//extern UnitTestServer g_app;

//UnitTestImp::UnitTestImp(const shared_ptr<RaftNode> &raftNode) : _raftNode(raftNode)
//{
//
//}

void UnitTestImp::initialize()
{
	_raftNode = ((UnitTestServer*)this->getApplication())->node();
}

void UnitTestImp::destroy()
{
}

int UnitTestImp::count(const CountReq &req, CountRsp &rsp, tars::CurrentPtr current)
{
	_raftNode->forwardOrReplicate(current, [&](){
		TarsOutputStream<BufferWriterString> os;
		req.writeTo(os);
		return os.getByteBuffer();

	});

	return 0;
}

