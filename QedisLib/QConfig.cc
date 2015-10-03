#include <vector>
#include "QConfig.h"
#include "ConfigParser.h"

#include <iostream>
using namespace std;


extern std::vector<QString>  SplitString(const QString& str, char seperator);

QConfig  g_config;

QConfig::QConfig()
{
    daemonize  = false;
    pidfile = "/var/run/qedis.pid";
    
    port = 6379;
    timeout = 0;
    
    loglevel = "notice";
    logdir = "stdout";
    
    databases = 16;
    
    // rdb
    saveseconds = 999999999;
    savechanges = 999999999;
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
    cfg.logdir   = parser.GetData<QString>("logfile");
    
    cfg.databases = parser.GetData<int>("databases");
 
    
    // load rdb config
    std::vector<QString>  saveInfo = std::move(SplitString(parser.GetData<QString>("save"), ' '));
    if (saveInfo.size() != 2)
    {
        std::cerr << "bad format save rdb interval, bad string "
                  << parser.GetData<QString>("save")
                  << std::endl;
        return false;
    }
    
    cfg.saveseconds = std::stoi(saveInfo[0]);
    cfg.savechanges = std::stoi(saveInfo[1]);
    
    if (cfg.saveseconds == 0)
        cfg.saveseconds = 999999999;
    if (cfg.savechanges == 0)
        cfg.savechanges = 999999999;
    
    cfg.rdbcompression = (parser.GetData<QString>("rdbcompression") == "yes");
    cfg.rdbchecksum    = (parser.GetData<QString>("rdbchecksum") == "yes");
    cfg.rdbfilename    = parser.GetData<QString>("dbfilename", "dump.rdb");
    cfg.rdbdir         = parser.GetData<QString>("dir", "./");
    
    cfg.maxclients = parser.GetData<int>("maxclients", 10000);
    cfg.appendonly = (parser.GetData<QString>("appendonly", "no") == "yes");
    cfg.appendfilename = parser.GetData<const char* >("appendfilename", "appendonly.aof");

    QString tmpfsync = parser.GetData<const char* >("appendfsync", "no");
    if (tmpfsync == "everysec")
    {
    }
    else if (tmpfsync == "always")
    {
    }
    else
    {
    }
    
    cfg.slowlogtime = parser.GetData<int>("slowlog-log-slower-than", 0);
    cfg.slowlogmaxlen = parser.GetData<int>("slowlog-max-len");
    
    cfg.hz = parser.GetData<int>("hz", 10);

    // load master ip port
    std::vector<QString>  master(SplitString(parser.GetData<QString>("slaveof"), ' '));
    if (master.size() == 2)
    {
        cfg.masterIp   = std::move(master[0]);
        cfg.masterPort = static_cast<unsigned short>(std::stoi(master[1]));
    }
    
    cfg.includefile = parser.GetData<QString>("include"); //TODO multi files include
    
    return  cfg.CheckArgs();
}

bool  QConfig::CheckArgs() const
{
#define RETURN_IF_FAIL(cond)\
    if (!(cond)) { \
        std::cerr << #cond " failed\n"; \
        return  false; \
    }
    
    RETURN_IF_FAIL(port > 0);
    RETURN_IF_FAIL(databases > 0);
    RETURN_IF_FAIL(maxclients > 0);
    RETURN_IF_FAIL(hz > 0 && hz < 500);

#undef RETURN_IF_FAIL
    
    return  true;
}
