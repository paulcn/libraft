#pragma once

#include "util/tc_option.h"
#include "util/tc_config.h"
#include "util/tc_timer.h"
#include "util/tc_thread_rwlock.h"
#include "Raft.h"
#include "servant/Application.h"

using namespace tars;

namespace raft
{

struct RaftClientOption
{
    std::string operateObj; //数据Obj配置 Raft.RocksDataServer.RocksOperateObj
    int64_t leaderCheckIntervalMs = 10000; //校验leader是否发生变化的时间间隔
};

//raft 后端服务客户端
template<typename T>
class RaftClient
{
public:
    bool init(const RaftClientOption &option)
    {
        try
        {
            const string & operateObj = option.operateObj;
            size_t pos = operateObj.find_first_of("@");
            if(pos != string::npos)
            {
                _servantName = operateObj.substr(0, pos);
                _fixedAddr = TC_Common::sepstr<string>(operateObj.substr(pos + 1), ":");
            }
            else
            {
                _servantName = operateObj;
            }
        
            //用户配置得节点
            _autoPrx = Application::getCommunicator()->stringToProxy<T> (option.operateObj);
        
            //更新当前leader节点
            refreshLeader();
        
            //启动timer
            _timer.postRepeated(option.leaderCheckIntervalMs,  false,std::bind(&RaftClient::refreshLeader ,this));
            _timer.startTimer();
        
           // TLOG_INFO("client init succ!_servantName" << _servantName << "|operateObj=" << operateObj <<endl);
            return true;
        }
        catch (std::exception &e)
        {
           // LOGIC_ERROR("exception "  << e.what() << endl);
        }
        catch (...)
        {
            //LOGIC_ERROR("unknown exception." << endl);
        }
    
        return false;
    }
    void uninit()
    {
        _timer.stopTimer();
    }
    
    T gePrx()
    {
        TC_ThreadRLock rLock(_rwLocker);
        if (_leaderPrx )
        {
            return _leaderPrx;
        }
        return _autoPrx;
    }
    
    //强制需要更新主节点，如果遇到调用超时，可以尝试强制更新主节点
    void forceRefreshLeader()
    {
        _timer.postDelayed(0,  std::bind(&RaftClient::refreshLeader ,this));
    }
    
protected:
    bool refreshLeader()
    {
        try
        {
            string from;
            string leaderServantObjTmp;
            int ret = _autoPrx->getLeaderEndPoint(from,leaderServantObjTmp);
            if (ret != 0)
            {
                //LOGIC_ERROR("get config fail! ret:" << ret <<endl);
                return false;
            }
            
            //如果leader发生变化
            if (!leaderServantObjTmp.empty() && leaderServantObjTmp != _leaderServantObj)
            {
                //LOG_LOGIC_ANY("leader change ! lastLeader:" << _leaderServantObj << ",current leader:" << leaderServantObjTmp << endl);
                //LOG_CONSOLE_DEBUG <<"leader change ! lastLeader:" << _leaderServantObj << ",current leader:" << leaderServantObjTmp << endl ;
                TC_ThreadWLock wLock(_rwLocker);
                _leaderPrx = Application::getCommunicator()->stringToProxy<T> (leaderServantObjTmp);
                _leaderServantObj = leaderServantObjTmp;
            }
    
            return true;
        }
        catch (std::exception &e)
        {
            //LOGIC_ERROR("exception|" << e.what() << endl);
        }
        catch (...)
        {
            //LOGIC_ERROR("unknown exception." << endl);
        }
        return false;
    }
protected:
    
    TC_ThreadRWLocker _rwLocker;
    T _autoPrx = nullptr;    //taf框架自己根据名字选择得节点接入
    T _leaderPrx = nullptr;  //节点中leader得节点
    string _servantName ;   //servant的名称
    vector<string>   _fixedAddr;    //配置文件中配置固定的地址
    TC_Timer _timer;
    string _leaderServantObj ;
};

}

