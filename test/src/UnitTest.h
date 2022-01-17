// **********************************************************************
// This file was generated by a TARS parser!
// TARS version 3.0.0.
// **********************************************************************

#ifndef __UNITTEST_H_
#define __UNITTEST_H_

#include <map>
#include <string>
#include <vector>
#include "tup/Tars.h"
#include "tup/TarsJson.h"
using namespace std;
#include "servant/ServantProxy.h"
#include "servant/Servant.h"
#include "promise/promise.h"


namespace raft
{
    enum RetValue
    {
        RT_SUCC = 0,
        RT_DATA_ERROR = -1,
        RT_APPLY_ERROR = -2,
    };
    inline string etos(const RetValue & e)
    {
        switch(e)
        {
            case RT_SUCC: return "RT_SUCC";
            case RT_DATA_ERROR: return "RT_DATA_ERROR";
            case RT_APPLY_ERROR: return "RT_APPLY_ERROR";
            default: return "";
        }
    }
    inline int stoe(const string & s, RetValue & e)
    {
        if(s == "RT_SUCC")  { e=RT_SUCC; return 0;}
        if(s == "RT_DATA_ERROR")  { e=RT_DATA_ERROR; return 0;}
        if(s == "RT_APPLY_ERROR")  { e=RT_APPLY_ERROR; return 0;}

        return -1;
    }

    struct CountReq : public tars::TarsStructBase
    {
    public:
        static string className()
        {
            return "raft.CountReq";
        }
        static string MD5()
        {
            return "a7a3989fcd822deff6d9bf877859a020";
        }
        CountReq()
        {
            resetDefautlt();
        }
        void resetDefautlt()
        {
            sBusinessName = "";
            sKey = "";
            iNum = 1;
            tTimestamp = 0;
        }
        template<typename WriterT>
        void writeTo(tars::TarsOutputStream<WriterT>& _os) const
        {
            if (sBusinessName != "")
            {
                _os.write(sBusinessName, 0);
            }
            if (sKey != "")
            {
                _os.write(sKey, 1);
            }
            if (iNum != 1)
            {
                _os.write(iNum, 2);
            }
            if (tTimestamp != 0)
            {
                _os.write(tTimestamp, 3);
            }
        }
        template<typename ReaderT>
        void readFrom(tars::TarsInputStream<ReaderT>& _is)
        {
            resetDefautlt();
            _is.read(sBusinessName, 0, false);
            _is.read(sKey, 1, false);
            _is.read(iNum, 2, false);
            _is.read(tTimestamp, 3, false);
        }
        tars::JsonValueObjPtr writeToJson() const
        {
            tars::JsonValueObjPtr p = new tars::JsonValueObj();
            p->value["sBusinessName"] = tars::JsonOutput::writeJson(sBusinessName);
            p->value["sKey"] = tars::JsonOutput::writeJson(sKey);
            p->value["iNum"] = tars::JsonOutput::writeJson(iNum);
            p->value["tTimestamp"] = tars::JsonOutput::writeJson(tTimestamp);
            return p;
        }
        string writeToJsonString() const
        {
            return tars::TC_Json::writeValue(writeToJson());
        }
        void readFromJson(const tars::JsonValuePtr & p, bool isRequire = true)
        {
            resetDefautlt();
            if(NULL == p.get() || p->getType() != tars::eJsonTypeObj)
            {
                char s[128];
                snprintf(s, sizeof(s), "read 'struct' type mismatch, get type: %d.", (p.get() ? p->getType() : 0));
                throw tars::TC_Json_Exception(s);
            }
            tars::JsonValueObjPtr pObj=tars::JsonValueObjPtr::dynamicCast(p);
            tars::JsonInput::readJson(sBusinessName,pObj->value["sBusinessName"], false);
            tars::JsonInput::readJson(sKey,pObj->value["sKey"], false);
            tars::JsonInput::readJson(iNum,pObj->value["iNum"], false);
            tars::JsonInput::readJson(tTimestamp,pObj->value["tTimestamp"], false);
        }
        void readFromJsonString(const string & str)
        {
            readFromJson(tars::TC_Json::getValue(str));
        }
        ostream& display(ostream& _os, int _level=0) const
        {
            tars::TarsDisplayer _ds(_os, _level);
            _ds.display(sBusinessName,"sBusinessName");
            _ds.display(sKey,"sKey");
            _ds.display(iNum,"iNum");
            _ds.display(tTimestamp,"tTimestamp");
            return _os;
        }
        ostream& displaySimple(ostream& _os, int _level=0) const
        {
            tars::TarsDisplayer _ds(_os, _level);
            _ds.displaySimple(sBusinessName, true);
            _ds.displaySimple(sKey, true);
            _ds.displaySimple(iNum, true);
            _ds.displaySimple(tTimestamp, false);
            return _os;
        }
    public:
        std::string sBusinessName;
        std::string sKey;
        tars::Int32 iNum;
        tars::Int64 tTimestamp;
    };
    inline bool operator==(const CountReq&l, const CountReq&r)
    {
        return l.sBusinessName == r.sBusinessName && l.sKey == r.sKey && l.iNum == r.iNum && l.tTimestamp == r.tTimestamp;
    }
    inline bool operator!=(const CountReq&l, const CountReq&r)
    {
        return !(l == r);
    }
    inline ostream& operator<<(ostream & os,const CountReq&r)
    {
        os << r.writeToJsonString();
        return os;
    }
    inline istream& operator>>(istream& is,CountReq&l)
    {
        std::istreambuf_iterator<char> eos;
        std::string s(std::istreambuf_iterator<char>(is), eos);
        l.readFromJsonString(s);
        return is;
    }

    struct CountRsp : public tars::TarsStructBase
    {
    public:
        static string className()
        {
            return "raft.CountRsp";
        }
        static string MD5()
        {
            return "e3e2c290ceec0c08c24111a502d92b13";
        }
        CountRsp()
        {
            resetDefautlt();
        }
        void resetDefautlt()
        {
            iRet = 0;
            sMsg = "";
            iCount = 0;
        }
        template<typename WriterT>
        void writeTo(tars::TarsOutputStream<WriterT>& _os) const
        {
            if (iRet != 0)
            {
                _os.write(iRet, 0);
            }
            if (sMsg != "")
            {
                _os.write(sMsg, 1);
            }
            if (iCount != 0)
            {
                _os.write(iCount, 2);
            }
        }
        template<typename ReaderT>
        void readFrom(tars::TarsInputStream<ReaderT>& _is)
        {
            resetDefautlt();
            _is.read(iRet, 0, false);
            _is.read(sMsg, 1, false);
            _is.read(iCount, 2, false);
        }
        tars::JsonValueObjPtr writeToJson() const
        {
            tars::JsonValueObjPtr p = new tars::JsonValueObj();
            p->value["iRet"] = tars::JsonOutput::writeJson(iRet);
            p->value["sMsg"] = tars::JsonOutput::writeJson(sMsg);
            p->value["iCount"] = tars::JsonOutput::writeJson(iCount);
            return p;
        }
        string writeToJsonString() const
        {
            return tars::TC_Json::writeValue(writeToJson());
        }
        void readFromJson(const tars::JsonValuePtr & p, bool isRequire = true)
        {
            resetDefautlt();
            if(NULL == p.get() || p->getType() != tars::eJsonTypeObj)
            {
                char s[128];
                snprintf(s, sizeof(s), "read 'struct' type mismatch, get type: %d.", (p.get() ? p->getType() : 0));
                throw tars::TC_Json_Exception(s);
            }
            tars::JsonValueObjPtr pObj=tars::JsonValueObjPtr::dynamicCast(p);
            tars::JsonInput::readJson(iRet,pObj->value["iRet"], false);
            tars::JsonInput::readJson(sMsg,pObj->value["sMsg"], false);
            tars::JsonInput::readJson(iCount,pObj->value["iCount"], false);
        }
        void readFromJsonString(const string & str)
        {
            readFromJson(tars::TC_Json::getValue(str));
        }
        ostream& display(ostream& _os, int _level=0) const
        {
            tars::TarsDisplayer _ds(_os, _level);
            _ds.display(iRet,"iRet");
            _ds.display(sMsg,"sMsg");
            _ds.display(iCount,"iCount");
            return _os;
        }
        ostream& displaySimple(ostream& _os, int _level=0) const
        {
            tars::TarsDisplayer _ds(_os, _level);
            _ds.displaySimple(iRet, true);
            _ds.displaySimple(sMsg, true);
            _ds.displaySimple(iCount, false);
            return _os;
        }
    public:
        tars::Int32 iRet;
        std::string sMsg;
        tars::Int64 iCount;
    };
    inline bool operator==(const CountRsp&l, const CountRsp&r)
    {
        return l.iRet == r.iRet && l.sMsg == r.sMsg && l.iCount == r.iCount;
    }
    inline bool operator!=(const CountRsp&l, const CountRsp&r)
    {
        return !(l == r);
    }
    inline ostream& operator<<(ostream & os,const CountRsp&r)
    {
        os << r.writeToJsonString();
        return os;
    }
    inline istream& operator>>(istream& is,CountRsp&l)
    {
        std::istreambuf_iterator<char> eos;
        std::string s(std::istreambuf_iterator<char>(is), eos);
        l.readFromJsonString(s);
        return is;
    }


    /* callback of async proxy for client */
    class CountPrxCallback: public tars::ServantProxyCallback
    {
    public:
        virtual ~CountPrxCallback(){}
        virtual void callback_count(tars::Int32 ret,  const raft::CountRsp& rsp)
        { throw std::runtime_error("callback_count() override incorrect."); }
        virtual void callback_count_exception(tars::Int32 ret)
        { throw std::runtime_error("callback_count_exception() override incorrect."); }

    public:
        virtual const map<std::string, std::string> & getResponseContext() const
        {
            CallbackThreadData * pCbtd = CallbackThreadData::getData();
            assert(pCbtd != NULL);

            if(!pCbtd->getContextValid())
            {
                throw TC_Exception("cann't get response context");
            }
            return pCbtd->getResponseContext();
        }

    public:
        virtual int onDispatch(tars::ReqMessagePtr msg)
        {
            static ::std::string __Count_all[]=
            {
                "count"
            };
            pair<string*, string*> r = equal_range(__Count_all, __Count_all+1, string(msg->request.sFuncName));
            if(r.first == r.second) return tars::TARSSERVERNOFUNCERR;
            switch(r.first - __Count_all)
            {
                case 0:
                {
                    if (msg->response->iRet != tars::TARSSERVERSUCCESS)
                    {
                        callback_count_exception(msg->response->iRet);

                        return msg->response->iRet;
                    }
                    tars::TarsInputStream<tars::BufferReader> _is;

                    _is.setBuffer(msg->response->sBuffer);
                    tars::Int32 _ret;
                    _is.read(_ret, 0, true);

                    raft::CountRsp rsp;
                    _is.read(rsp, 2, true);
                    CallbackThreadData * pCbtd = CallbackThreadData::getData();
                    assert(pCbtd != NULL);

                    pCbtd->setResponseContext(msg->response->context);

                    callback_count(_ret, rsp);

                    pCbtd->delResponseContext();

                    return tars::TARSSERVERSUCCESS;

                }
            }
            return tars::TARSSERVERNOFUNCERR;
        }

    };
    typedef tars::TC_AutoPtr<CountPrxCallback> CountPrxCallbackPtr;

    //callback of promise async proxy for client
    class CountPrxCallbackPromise: public tars::ServantProxyCallback
    {
    public:
        virtual ~CountPrxCallbackPromise(){}
    public:
        struct Promisecount: virtual public TC_HandleBase
        {
        public:
            tars::Int32 _ret;
            raft::CountRsp rsp;
            map<std::string, std::string> _mRspContext;
        };
        
        typedef tars::TC_AutoPtr< CountPrxCallbackPromise::Promisecount > PromisecountPtr;

        CountPrxCallbackPromise(const tars::Promise< CountPrxCallbackPromise::PromisecountPtr > &promise)
        : _promise_count(promise)
        {}
        
        virtual void callback_count(const CountPrxCallbackPromise::PromisecountPtr &ptr)
        {
            _promise_count.setValue(ptr);
        }
        virtual void callback_count_exception(tars::Int32 ret)
        {
            std::string str("");
            str += "Function:count_exception|Ret:";
            str += TC_Common::tostr(ret);
            _promise_count.setException(tars::copyException(str, ret));
        }

    protected:
        tars::Promise< CountPrxCallbackPromise::PromisecountPtr > _promise_count;

    public:
        virtual int onDispatch(tars::ReqMessagePtr msg)
        {
            static ::std::string __Count_all[]=
            {
                "count"
            };

            pair<string*, string*> r = equal_range(__Count_all, __Count_all+1, string(msg->request.sFuncName));
            if(r.first == r.second) return tars::TARSSERVERNOFUNCERR;
            switch(r.first - __Count_all)
            {
                case 0:
                {
                    if (msg->response->iRet != tars::TARSSERVERSUCCESS)
                    {
                        callback_count_exception(msg->response->iRet);

                        return msg->response->iRet;
                    }
                    tars::TarsInputStream<tars::BufferReader> _is;

                    _is.setBuffer(msg->response->sBuffer);

                    CountPrxCallbackPromise::PromisecountPtr ptr = new CountPrxCallbackPromise::Promisecount();

                    try
                    {
                        _is.read(ptr->_ret, 0, true);

                        _is.read(ptr->rsp, 2, true);
                    }
                    catch(std::exception &ex)
                    {
                        callback_count_exception(tars::TARSCLIENTDECODEERR);

                        return tars::TARSCLIENTDECODEERR;
                    }
                    catch(...)
                    {
                        callback_count_exception(tars::TARSCLIENTDECODEERR);

                        return tars::TARSCLIENTDECODEERR;
                    }

                    ptr->_mRspContext = msg->response->context;

                    callback_count(ptr);

                    return tars::TARSSERVERSUCCESS;

                }
            }
            return tars::TARSSERVERNOFUNCERR;
        }

    };
    typedef tars::TC_AutoPtr<CountPrxCallbackPromise> CountPrxCallbackPromisePtr;

    /* callback of coroutine async proxy for client */
    class CountCoroPrxCallback: public CountPrxCallback
    {
    public:
        virtual ~CountCoroPrxCallback(){}
    public:
        virtual const map<std::string, std::string> & getResponseContext() const { return _mRspContext; }

        virtual void setResponseContext(const map<std::string, std::string> &mContext) { _mRspContext = mContext; }

    public:
        int onDispatch(tars::ReqMessagePtr msg)
        {
            static ::std::string __Count_all[]=
            {
                "count"
            };

            pair<string*, string*> r = equal_range(__Count_all, __Count_all+1, string(msg->request.sFuncName));
            if(r.first == r.second) return tars::TARSSERVERNOFUNCERR;
            switch(r.first - __Count_all)
            {
                case 0:
                {
                    if (msg->response->iRet != tars::TARSSERVERSUCCESS)
                    {
                        callback_count_exception(msg->response->iRet);

                        return msg->response->iRet;
                    }
                    tars::TarsInputStream<tars::BufferReader> _is;

                    _is.setBuffer(msg->response->sBuffer);
                    try
                    {
                        tars::Int32 _ret;
                        _is.read(_ret, 0, true);

                        raft::CountRsp rsp;
                        _is.read(rsp, 2, true);
                        setResponseContext(msg->response->context);

                        callback_count(_ret, rsp);

                    }
                    catch(std::exception &ex)
                    {
                        callback_count_exception(tars::TARSCLIENTDECODEERR);

                        return tars::TARSCLIENTDECODEERR;
                    }
                    catch(...)
                    {
                        callback_count_exception(tars::TARSCLIENTDECODEERR);

                        return tars::TARSCLIENTDECODEERR;
                    }

                    return tars::TARSSERVERSUCCESS;

                }
            }
            return tars::TARSSERVERNOFUNCERR;
        }

    protected:
        map<std::string, std::string> _mRspContext;
    };
    typedef tars::TC_AutoPtr<CountCoroPrxCallback> CountCoroPrxCallbackPtr;

    /* proxy for client */
    class CountProxy : public tars::ServantProxy
    {
    public:
        typedef map<string, string> TARS_CONTEXT;
        tars::Int32 count(const raft::CountReq & req,raft::CountRsp &rsp,const map<string, string> &context = TARS_CONTEXT(),map<string, string> * pResponseContext = NULL)
        {
            tars::TarsOutputStream<tars::BufferWriterVector> _os;
            _os.write(req, 1);
            _os.write(rsp, 2);
            std::map<string, string> _mStatus;
            shared_ptr<tars::ResponsePacket> rep = tars_invoke(tars::TARSNORMAL,"count", _os, context, _mStatus);
            if(pResponseContext)
            {
                pResponseContext->swap(rep->context);
            }

            tars::TarsInputStream<tars::BufferReader> _is;
            _is.setBuffer(rep->sBuffer);
            tars::Int32 _ret;
            _is.read(_ret, 0, true);
            _is.read(rsp, 2, true);
            return _ret;
        }

        void async_count(CountPrxCallbackPtr callback,const raft::CountReq &req,const map<string, string>& context = TARS_CONTEXT())
        {
            tars::TarsOutputStream<tars::BufferWriterVector> _os;
            _os.write(req, 1);
            std::map<string, string> _mStatus;
            tars_invoke_async(tars::TARSNORMAL,"count", _os, context, _mStatus, callback);
        }
        
        tars::Future< CountPrxCallbackPromise::PromisecountPtr > promise_async_count(const raft::CountReq &req,const map<string, string>& context)
        {
            tars::Promise< CountPrxCallbackPromise::PromisecountPtr > promise;
            CountPrxCallbackPromisePtr callback = new CountPrxCallbackPromise(promise);

            tars::TarsOutputStream<tars::BufferWriterVector> _os;
            _os.write(req, 1);
            std::map<string, string> _mStatus;
            tars_invoke_async(tars::TARSNORMAL,"count", _os, context, _mStatus, callback);

            return promise.getFuture();
        }

        void coro_count(CountCoroPrxCallbackPtr callback,const raft::CountReq &req,const map<string, string>& context = TARS_CONTEXT())
        {
            tars::TarsOutputStream<tars::BufferWriterVector> _os;
            _os.write(req, 1);
            std::map<string, string> _mStatus;
            tars_invoke_async(tars::TARSNORMAL,"count", _os, context, _mStatus, callback, true);
        }

        CountProxy* tars_hash(int64_t key)
        {
            return (CountProxy*)ServantProxy::tars_hash(key);
        }

        CountProxy* tars_consistent_hash(int64_t key)
        {
            return (CountProxy*)ServantProxy::tars_consistent_hash(key);
        }

        CountProxy* tars_set_timeout(int msecond)
        {
            return (CountProxy*)ServantProxy::tars_set_timeout(msecond);
        }

        static const char* tars_prxname() { return "CountProxy"; }
    };
    typedef tars::TC_AutoPtr<CountProxy> CountPrx;

    /* servant for server */
    class Count : public tars::Servant
    {
    public:
        virtual ~Count(){}
        virtual tars::Int32 count(const raft::CountReq & req,raft::CountRsp &rsp,tars::TarsCurrentPtr current) = 0;
        static void async_response_count(tars::TarsCurrentPtr current, tars::Int32 _ret, const raft::CountRsp &rsp)
        {
            if (current->getRequestVersion() == TUPVERSION )
            {
                UniAttribute<tars::BufferWriterVector, tars::BufferReader>  tarsAttr;
                tarsAttr.setVersion(current->getRequestVersion());
                tarsAttr.put("", _ret);
                tarsAttr.put("tars_ret", _ret);
                tarsAttr.put("rsp", rsp);

                vector<char> sTupResponseBuffer;
                tarsAttr.encode(sTupResponseBuffer);
                current->sendResponse(tars::TARSSERVERSUCCESS, sTupResponseBuffer);
            }
            else if (current->getRequestVersion() == JSONVERSION)
            {
                tars::JsonValueObjPtr _p = new tars::JsonValueObj();
                _p->value["rsp"] = tars::JsonOutput::writeJson(rsp);
                _p->value["tars_ret"] = tars::JsonOutput::writeJson(_ret);
                vector<char> sJsonResponseBuffer;
                tars::TC_Json::writeValue(_p, sJsonResponseBuffer);
                current->sendResponse(tars::TARSSERVERSUCCESS, sJsonResponseBuffer);
            }
            else
            {
                tars::TarsOutputStream<tars::BufferWriterVector> _os;
                _os.write(_ret, 0);

                _os.write(rsp, 2);

                current->sendResponse(tars::TARSSERVERSUCCESS, _os.getByteBuffer());
            }
        }

    public:
        int onDispatch(tars::TarsCurrentPtr _current, vector<char> &_sResponseBuffer)
        {
            static ::std::string __raft__Count_all[]=
            {
                "count"
            };

            pair<string*, string*> r = equal_range(__raft__Count_all, __raft__Count_all+1, _current->getFuncName());
            if(r.first == r.second) return tars::TARSSERVERNOFUNCERR;
            switch(r.first - __raft__Count_all)
            {
                case 0:
                {
                    tars::TarsInputStream<tars::BufferReader> _is;
                    _is.setBuffer(_current->getRequestBuffer());
                    raft::CountReq req;
                    raft::CountRsp rsp;
                    if (_current->getRequestVersion() == TUPVERSION)
                    {
                        UniAttribute<tars::BufferWriterVector, tars::BufferReader>  tarsAttr;
                        tarsAttr.setVersion(_current->getRequestVersion());
                        tarsAttr.decode(_current->getRequestBuffer());
                        tarsAttr.get("req", req);
                        tarsAttr.getByDefault("rsp", rsp, rsp);
                    }
                    else if (_current->getRequestVersion() == JSONVERSION)
                    {
                        tars::JsonValueObjPtr _jsonPtr = tars::JsonValueObjPtr::dynamicCast(tars::TC_Json::getValue(_current->getRequestBuffer()));
                        tars::JsonInput::readJson(req, _jsonPtr->value["req"], true);
                        tars::JsonInput::readJson(rsp, _jsonPtr->value["rsp"], false);
                    }
                    else
                    {
                        _is.read(req, 1, true);
                        _is.read(rsp, 2, false);
                    }
                    tars::Int32 _ret = count(req,rsp, _current);
                    if(_current->isResponse())
                    {
                        if (_current->getRequestVersion() == TUPVERSION)
                        {
                            UniAttribute<tars::BufferWriterVector, tars::BufferReader>  tarsAttr;
                            tarsAttr.setVersion(_current->getRequestVersion());
                            tarsAttr.put("", _ret);
                            tarsAttr.put("tars_ret", _ret);
                            tarsAttr.put("rsp", rsp);
                            tarsAttr.encode(_sResponseBuffer);
                        }
                        else if (_current->getRequestVersion() == JSONVERSION)
                        {
                            tars::JsonValueObjPtr _p = new tars::JsonValueObj();
                            _p->value["rsp"] = tars::JsonOutput::writeJson(rsp);
                            _p->value["tars_ret"] = tars::JsonOutput::writeJson(_ret);
                            tars::TC_Json::writeValue(_p, _sResponseBuffer);
                        }
                        else
                        {
                            tars::TarsOutputStream<tars::BufferWriterVector> _os;
                            _os.write(_ret, 0);
                            _os.write(rsp, 2);
                            _os.swap(_sResponseBuffer);
                        }
                    }
                    return tars::TARSSERVERSUCCESS;

                }
            }
            return tars::TARSSERVERNOFUNCERR;
        }
    };


}



#endif
