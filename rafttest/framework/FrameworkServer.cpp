#include "FrameworkServer.h"
#include "QueryImp.h"

using namespace std;

FrameworkServer::~FrameworkServer()
{

}


void
FrameworkServer::initialize()
{
	addServant<QueryImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".QueryObj");
}

void FrameworkServer::destroyApp()
{
}

void FrameworkServer::run()
{
	waitForShutdown();
}
