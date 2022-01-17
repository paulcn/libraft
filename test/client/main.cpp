#include "Count.h"
#include <iostream>
#include "servant/Application.h"
#include "util/tc_option.h"

using namespace tars;
using namespace raft;

Communicator *_comm;
string matchObj = "raft.UnitTestServer.UnitTestObj@tcp -h 127.0.0.1 -p 30001:tcp -h 127.0.0.1 -p 30002:tcp -h 127.0.0.1 -p 30003";

struct Param
{
	int count;
	string call;
	int thread;

	CountPrx pPrx;
};

Param param;
std::atomic<int> callback(0);

struct CountCallback : public CountPrxCallback
{
	CountCallback(int64_t t, int i, int c) : start(t), cur(i), count(c)
	{

	}

	//call back
	virtual void callback_count(tars::Int32 ret,  const raft::CountRsp& rsp)
	{
		++callback;

		if(cur == count-1)
		{
			int64_t cost = TC_Common::now2us() - start;
			cout << "callback_count count:" << count << ", " << cost << " us, avg:" << 1.*cost/count << "us" << endl;
		}
	}

	virtual void callback_count_exception(tars::Int32 ret)
	{
		cout << "callback_count exception:" << ret << endl;
	}

	int64_t start;
	int     cur;
	int     count;
};

void syncCall(int c)
{
	int64_t t = TC_Common::now2us();

	//发起远程调用
	for (int i = 0; i < c; ++i)
	{
		try
		{
			CountReq req;
			req.sBusinessName = "app";
			req.sKey = "key";

			CountRsp rsp;
			param.pPrx->count(req, rsp);

			TC_Common::msleep(20);
		}
		catch(exception& e)
		{
			cout << "exception:" << e.what() << endl;
		}
		++callback;
	}

	int64_t cost = TC_Common::now2us() - t;
	cout << "syncCall total:" << cost << "us, avg:" << 1.*cost/c << "us" << endl;
}

void asyncCall(int c)
{
	int64_t t = TC_Common::now2us();

	//发起远程调用
	for (int i = 0; i < c; ++i)
	{
		if(i - callback < 1000)
		{
			CountPrxCallbackPtr p = new CountCallback(t, i, c);

			CountReq req;
			req.sBusinessName = "app";
			req.sKey = "key";

			try
			{
				param.pPrx->async_count(p, req);
			}
			catch(exception& e)
			{
				cout << "exception:" << e.what() << endl;
			}
		}
		else
		{
			--i;
			TC_Common::msleep(50);
		}
	}

	int64_t cost = TC_Common::now2us() - t;
	cout << "asyncCall send:" << cost << "us, avg:" << 1.*cost/c << "us" << endl;
}


int main(int argc, char *argv[])
{
	try
	{
		if (argc < 2)
		{
			cout << "Usage:" << argv[0] << "--count=1000 --call=[sync|async] --thread=1" << endl;

			return 0;
		}

		TC_Option option;
		option.decode(argc, argv);

		param.count = TC_Common::strto<int>(option.getValue("count"));
		if(param.count <= 0) param.count = 1000;
		param.call = option.getValue("call");
		if(param.call.empty()) param.call = "sync";
		param.thread = TC_Common::strto<int>(option.getValue("thread"));
		if(param.thread <= 0) param.thread = 1;

		_comm = new Communicator();

//		TC_Config conf;
//		conf.parseFile(option.getValue("config"));
//		_comm->setProperty(conf);

//        LocalRollLogger::getInstance()->logger()->setLogLevel(6);

		_comm->setProperty("sendqueuelimit", "1000000");
		_comm->setProperty("asyncqueuecap", "1000000");

		param.pPrx = _comm->stringToProxy<CountPrx>(matchObj);

		param.pPrx->taf_connect_timeout(50000);
		param.pPrx->taf_set_timeout(60 * 1000);
		param.pPrx->taf_async_timeout(60*1000);
		param.pPrx->taf_ping();

		int64_t start = TC_Common::now2us();

		std::function<void(int)> func;

		if (param.call == "sync")
		{
			func = syncCall;
		}
		else if (param.call == "async")
		{
			func = asyncCall;
		}
//		else if (param.call == "print")
//		{
//			string buff;
//			param.pPrx->printOrder("100001.XS", buff);
//			cout << "printOrder" << endl;
//			cout << buff << endl;
//			exit(0);
//		}

		vector<std::thread*> vt;
		for(int i = 0 ; i< param.thread; i++)
		{
			vt.push_back(new std::thread(func, param.count));
		}

		std::thread print([&]{while(callback != param.count * param.thread) {
			cout << "Auth:" << param.call << " : ----------finish count:" << callback << endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		};});

		for(size_t i = 0 ; i< vt.size(); i++)
		{
			vt[i]->join();
			delete vt[i];
		}

		cout << "(pid:" << std::this_thread::get_id() << ")"
		     << "(count:" << param.count << ")"
		     << "(use ms:" << (TC_Common::now2us() - start)/1000 << ")"
		     << endl;


		while(callback != param.count * param.thread) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		print.join();
		cout << "order:" << param.call << " ----------finish count:" << callback << endl;
	}
	catch(exception &ex)
	{
		cout << ex.what() << endl;
	}
	cout << "main return." << endl;

	return 0;
}
