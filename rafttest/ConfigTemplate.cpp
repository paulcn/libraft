
#include "ConfigTemplate.h"
#include "util/tc_common.h"
#include "servant/RemoteLogger.h"

#define CONFIG_TEMPLATE "<tars> \n\
  <application> \n\
    <client> \n\
        max-invoke-timeout          = 5000 \n\
        refresh-endpoint-interval   = 2000 \n\
        asyncthread                 = 2 \n\
        modulename                  = $APP.$SERVER \n\
    </client> \n\
    <server> \n\
		start_output = ERROR \n\
        closecout = 0 \n\
        app      = $APP \n\
        server   = $SERVER \n\
        basepath = ./app_log/$SERVER/data/$BUSS_PORT \n\
        datapath = ./app_log/$SERVER/data/$BUSS_PORT \n\
        logpath  = ./app_log/$BUSS_PORT/ \n\
        netthread = 1 \n\
        <$APP.$SERVER.RaftObjAdapter> \n\
            endpoint = tcp -h 127.0.0.1 -p $RAFT_PORT -t 10000 \n\
            allow	 = \n\
            maxconns = 4096 \n\
            threads	 = 1 \n\
            servant = $APP.$SERVER.RaftObj \n\
            queuecap = 100000 \n\
        </$APP.$SERVER.RaftObjAdapter> \n\
        <$APP.$SERVER.$SERVANTAdapter> \n\
            endpoint = tcp -h 127.0.0.1 -p $BUSS_PORT -t 10000 \n\
            allow	 = \n\
            maxconns = 4096 \n\
            threads	 = 2  \n\
            servant = $APP.$SERVER.$SERVANT \n\
            queuecap = 100000  \n\
        </$APP.$SERVER.$SERVANTAdapter> \n\
    </server> \n\
  </application> \n\
</tars>"

TC_Config ConfigTemplate::getConfig(const string &app, const string &serverName, const string &servantName, int raftPort, int bussPort)
{
	string config = CONFIG_TEMPLATE;
	config = TC_Common::replace(config, "$APP", app);
	config = TC_Common::replace(config, "$SERVER", serverName);
	config = TC_Common::replace(config, "$SERVANT", servantName);
	config = TC_Common::replace(config, "$RAFT_PORT", TC_Common::tostr(raftPort));
	config = TC_Common::replace(config, "$BUSS_PORT", TC_Common::tostr(bussPort));

//	LOG_CONSOLE_DEBUG << config << endl;

	TC_Config conf;
	conf.parseString(config);

	return conf;
}
