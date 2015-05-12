#include "QConfig.h"
#include "ConfigParser.h"

extern  QConfig  g_config;

#include <iostream>
using namespace std;


QConfig  g_config;

QConfig::QConfig()
{
    daemonize  = false;
    QString   pidfile = "/var/run/qedis.pid";
    
    port = 6379;
    timeout = 0;
    
    loglevel = "notice";
    logfile = "stdout";
    
    databases = 16;
    
    // rdb
    saveseconds = 0;
    savechanges = 0;
    rdbcompression = true;
    rdbchecksum    = true;
    rdbfilename    = "dump.rdb";
    rdbdir         = "./";
    
    maxclients = 10000;
    
    // aof
    appendonly = false;
    appendfilename = "appendonly.aof";
    appendfsync = 0;
    
    // slow log
    slowlogtime = 0;
    slowlogmaxlen = 128;
    
    hz = 10;
    
    includefile = "";
}

bool  LoadQedisConfig(const char* cfgFile, QConfig& cfg)
{
    ConfigParser  parser;
    if (!parser.Load(cfgFile))
        return false;
    
    if (parser.GetData<QString>("daemonize") == "yes")
        cfg.daemonize = true;
    else
        cfg.daemonize = false;
    
    cfg.pidfile = parser.GetData<QString>("pidfile");
    
    cfg.port    = parser.GetData<unsigned short>("port");
    cfg.timeout = parser.GetData<int>("timeout");
    
    cfg.loglevel = parser.GetData<QString>("loglevel");
    cfg.logfile  = parser.GetData<QString>("logfile");
    
    cfg.databases = parser.GetData<int>("databases");
    if (cfg.databases <= 0)
    {
        std::cerr << "wrong databases num " << cfg.databases << std::endl;
        return false;
    }
    
    return  true;
}