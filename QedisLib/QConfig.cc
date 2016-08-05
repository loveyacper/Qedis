#include <vector>
#include <iostream>

#include "QConfig.h"
#include "ConfigParser.h"

namespace qedis
{

extern std::vector<QString>  SplitString(const QString& str, char seperator);

QConfig  g_config;

QConfig::QConfig()
{
    daemonize  = false;
    pidfile = "/var/run/qedis.pid";
    
    ip = "127.0.0.1";
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
    rdbfullname    = "./dump.rdb";
    
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

    maxmemory = 2 * 1024 * 1024 * 1024UL;
    maxmemorySamples = 5;
    noeviction = true;
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
    
    cfg.pidfile = parser.GetData<QString>("pidfile", cfg.pidfile);
    
    cfg.ip      = parser.GetData<QString>("bind", cfg.ip);
    cfg.port    = parser.GetData<unsigned short>("port");
    cfg.timeout = parser.GetData<int>("timeout");
    
    cfg.loglevel = parser.GetData<QString>("loglevel", cfg.loglevel);
    cfg.logdir   = parser.GetData<QString>("logfile", cfg.logdir);
    if (cfg.logdir == "\"\"")
        cfg.logdir = "stdout";
    
    cfg.databases = parser.GetData<int>("databases", cfg.databases);
    cfg.password  = parser.GetData<QString>("requirepass");

    // alias command
    {
        std::vector<QString>  alias = std::move(SplitString(parser.GetData<QString>("rename-command"), ' '));
        if (alias.size() % 2 == 0)
        {
            for (auto it(alias.begin()); it != alias.end(); )
            {
                const QString& oldCmd =  *(it ++);
                const QString& newCmd =  *(it ++);
                cfg.aliases[oldCmd] = newCmd;
            }
        }
    }
    
    // load rdb config
    std::vector<QString>  saveInfo = std::move(SplitString(parser.GetData<QString>("save"), ' '));
    if (!saveInfo.empty() && saveInfo.size() != 2)
    {
        if (!(saveInfo.size() == 1 && saveInfo[0] == "\"\""))
        {
            std::cerr << "bad format save rdb interval, bad string "
                      << parser.GetData<QString>("save")
                      << std::endl;
            return false;
        }
    }
    else if (!saveInfo.empty())
    {
        cfg.saveseconds = std::stoi(saveInfo[0]);
        cfg.savechanges = std::stoi(saveInfo[1]);
    }
    
    if (cfg.saveseconds == 0)
        cfg.saveseconds = 999999999;
    if (cfg.savechanges == 0)
        cfg.savechanges = 999999999;
    
    cfg.rdbcompression = (parser.GetData<QString>("rdbcompression") == "yes");
    cfg.rdbchecksum    = (parser.GetData<QString>("rdbchecksum") == "yes");
    
    cfg.rdbfullname    = parser.GetData<QString>("dir", "./") + \
                         parser.GetData<QString>("dbfilename", "dump.rdb");
    
    cfg.maxclients = parser.GetData<int>("maxclients", 10000);
    cfg.appendonly = (parser.GetData<QString>("appendonly", "no") == "yes");
    cfg.appendfilename = parser.GetData<const char* >("appendfilename", "appendonly.aof");
    if (cfg.appendfilename.size() <= 2)
        return false;

    if (cfg.appendfilename[0] == '"') // redis.conf use quote for string, but qedis do not. For compatiable...
        cfg.appendfilename = cfg.appendfilename.substr(1, cfg.appendfilename.size() - 2);

    QString tmpfsync = parser.GetData<const char* >("appendfsync", "no");
    // qedis always use "always", fsync is done in another thread
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
    cfg.slowlogmaxlen = parser.GetData<int>("slowlog-max-len", cfg.slowlogmaxlen);
    
    cfg.hz = parser.GetData<int>("hz", 10);

    // load master ip port
    std::vector<QString>  master(SplitString(parser.GetData<QString>("slaveof"), ' '));
    if (master.size() == 2)
    {
        cfg.masterIp   = std::move(master[0]);
        cfg.masterPort = static_cast<unsigned short>(std::stoi(master[1]));
    }
    cfg.masterauth = parser.GetData<QString>("masterauth");

    // load modules' names
    cfg.modules = parser.GetDataVector("loadmodule");
    
    cfg.includefile = parser.GetData<QString>("include"); //TODO multi files include

    // lru cache
    cfg.maxmemory = parser.GetData<uint64_t>("maxmemory", 2 * 1024 * 1024 * 1024UL);
    cfg.maxmemorySamples = parser.GetData<int>("maxmemory-samples", 5);
    cfg.noeviction = (parser.GetData<QString>("maxmemory-policy", "noeviction") == "noeviction");
    
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
    RETURN_IF_FAIL(maxmemory >= 512 * 1024 * 1024UL);
    RETURN_IF_FAIL(maxmemorySamples > 0 && maxmemorySamples < 10);


#undef RETURN_IF_FAIL
    
    return  true;
}

bool QConfig::CheckPassword(const QString& pwd) const 
{
    return password.empty() || password == pwd;
}
    
}
