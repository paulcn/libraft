
#pragma once

#include "util/tc_config.h"

using namespace tars;

class ConfigTemplate
{
public:
	static TC_Config getConfig(const string &app, const string &serverName, const string &servantName, int raftPort, int bussPort);
};
