#include <sys/utsname.h>
#include <cassert>
#include <unistd.h>

#include "QStore.h"
#include "QClient.h"
#include "Log/Logger.h"
#include "Server.h"
#include "QDB.h"
#include "QAOF.h"
#include "QConfig.h"
#include "QSlowLog.h"
#include "QGlobRegex.h"
#include "Delegate.h"


namespace qedis
{

QError  select(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    int newDb = atoi(params[1].c_str());
    
    auto client = QClient::Current();
    
    if (client)
    {
        if (client->SelectDB(newDb))
            FormatOK(reply);
        else
            ReplyError(QError_invalidDB, reply);
    }
    else
    {
        QSTORE.SelectDB(newDb);
    }

    return   QError_ok;
}


QError  dbsize(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    FormatInt(static_cast<long>(QSTORE.DBSize()), reply);
    return   QError_ok;
}

QError  flushdb(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QSTORE.dirty_ += QSTORE.DBSize();
    QSTORE.ClearCurrentDB();
    Propogate(QSTORE.GetDB(), params);
    
    FormatOK(reply);
    return   QError_ok;
}

QError  flushall(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    int currentDb = QSTORE.GetDB();
    
    QEDIS_DEFER {
        QSTORE.SelectDB(currentDb);
        Propogate(-1, params);
        QSTORE.ResetDb();
    };
    
    for (int dbno = 0; true; ++ dbno)
    {
        if (QSTORE.SelectDB(dbno) == -1)
            break;
  
        QSTORE.dirty_ += QSTORE.DBSize();
    }
    
    FormatOK(reply);
    return   QError_ok;
}

QError  bgsave(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    if (g_qdbPid != -1 || g_rewritePid != -1)
    {
        FormatBulk("-ERR Background save or aof already in progress",
                   sizeof "-ERR Background save or aof already in progress" - 1,
                   reply);

        return QError_ok;
    }
   
    int ret = fork();
    if (ret == 0)
    {
        {
        QDBSaver  qdb;
        qdb.Save(g_config.rdbfullname.c_str());
        }
        _exit(0);
    }
    else if (ret == -1)
    {
        FormatSingle("Background saving FAILED", 24, reply);
    }
    else
    {
        g_qdbPid = ret;
        FormatSingle("Background saving started", 25, reply);
    }

    return   QError_ok;
}

QError  save(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    if (g_qdbPid != -1 || g_rewritePid != -1)
    {
        FormatBulk("-ERR Background save or aof already in progress",
                   sizeof "-ERR Background save or aof already in progress" - 1,
                   reply);
        
        return QError_ok;
    }
    
    QDBSaver  qdb;
    qdb.Save(g_config.rdbfullname.c_str());
    g_lastQDBSave = time(NULL);

    FormatOK(reply);
    return   QError_ok;
}

QError  lastsave(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    FormatInt(g_lastQDBSave, reply);
    return   QError_ok;
}

QError  client(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    // getname   setname    kill  list
    QError   err = QError_ok;
    
    if (params[1].size() == 7 && strncasecmp(params[1].c_str(), "getname", 7) == 0)
    {
        if (params.size() != 2)
            ReplyError(err = QError_param, reply);
        else
            FormatBulk(QClient::Current()->GetName(),
                       reply);
    }
    else if (params[1].size() == 7 && strncasecmp(params[1].c_str(), "setname", 7) == 0)
    {
        if (params.size() != 3)
        {
            ReplyError(err = QError_param, reply);
        }
        else
        {
            QClient::Current()->SetName(params[2]);
            FormatOK(reply);
        }
    }
    else if (params[1].size() == 4 && strncasecmp(params[1].c_str(), "kill", 4) == 0)
    {
        // only kill current client
        //QClient::Current()->OnError();
        FormatOK(reply);
    }
    else if (params[1].size() == 4 && strncasecmp(params[1].c_str(), "list", 4) == 0)
    {
        FormatOK(reply);
    }
    else
    {
        ReplyError(err = QError_param, reply);
    }
    
    return   err;
}

static int Suicide()
{
    int* ptr = nullptr;
    *ptr = 0;
    
    return *ptr;
}

QError  debug(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QError err = QError_ok;
    
    if (strncasecmp(params[1].c_str(), "segfault", 8) == 0 && params.size() == 2)
    {
        Suicide();
        assert (false);
    }
    else if (strncasecmp(params[1].c_str(), "object", 6) == 0 && params.size() == 3)
    {
        QObject* obj = nullptr;
        err = QSTORE.GetValue(params[2], obj, false);
        
        if (err != QError_ok)
        {
            ReplyError(err, reply);
        }
        else
        {
            // ref count,  encoding, idle time
            char buf[512];
            int  len = snprintf(buf, sizeof buf, "ref count:%ld, encoding:%s, idletime:%u",
                                obj->value.use_count(),
                                EncodingStringInfo(obj->encoding),
                                EstimateIdleTime(obj->lru));
            FormatBulk(buf, len, reply);
        }
    }
    else
    {
        ReplyError(err = QError_param, reply);
    }
    
    return   err;
}


QError  shutdown(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    if (params.size() == 2 && strncasecmp(params[1].c_str(), "save", 4) == 0)
    {
        QDBSaver  qdb;
        qdb.Save(g_config.rdbfullname.c_str());
    }

    Server::Instance()->Terminate();
    return   QError_ok;
}


QError  ping(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    FormatSingle("PONG", 4, reply);
    return   QError_ok;
}

QError  echo(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    FormatBulk(params[1], reply);
    return   QError_ok;
}

void OnMemoryInfoCollect(UnboundedBuffer& res)
{
    // memory info
    auto minfo = getMemoryInfo();

    char buf[1024];
    int n = snprintf(buf, sizeof buf - 1,
                 "# Memory\r\n"
                 "used_memory_peak:%lu\r\n"
                 "used_memory:%lu\r\n"
                 "used_memory_human:%sMB\r\n"
                 "used_memory_rss_peak:%lu\r\n"
                 "used_memory_rss:%lu\r\n"
                 "used_memory_rss_human:%sMB\r\n"
                 "used_memory_lock:%lu\r\n"
                 "used_memory_swap:%lu\r\n"
                 , minfo[VmPeak]
                 , minfo[VmSize]
                 , std::to_string(minfo[VmSize] / 1024.0f / 1024.0f).data()
                 , minfo[VmHWM]
                 , minfo[VmRSS]
                 , std::to_string(minfo[VmRSS] / 1024.0f / 1024.0f).data()
                 , minfo[VmLck]
                 , minfo[VmSwap]
            );
    
    if (!res.IsEmpty())
        res.PushData("\r\n", 2);

    res.PushData(buf, n);
}

void OnServerInfoCollect(UnboundedBuffer& res)
{
    char buf[1024];
    
    // server
    struct utsname  name;
    uname(&name);
    int n = snprintf(buf, sizeof buf - 1,
                 "# Server\r\n"
                 "redis_mode:standalone\r\n" // not cluster node yet
                 "os:%s %s %s\r\n"
                 "run_id:%s\r\n"
                 "hz:%d\r\n"
                 "tcp_port:%hu\r\n"
                 , name.sysname, name.release, name.machine
                 , g_config.runid.data()
                 , g_config.hz
                 , g_config.port);
    
    if (!res.IsEmpty())
        res.PushData("\r\n", 2);

    res.PushData(buf, n);
}
    
    
void OnClientInfoCollect(UnboundedBuffer& res)
{
    char buf[1024];

    int n = snprintf(buf, sizeof buf - 1,
                 "# Clients\r\n"
                 "connected_clients:%lu\r\n"
                 "blocked_clients:%lu\r\n"
                 , Server::Instance()->TCPSize()
                 , QSTORE.BlockedSize());
    
    
    if (!res.IsEmpty())
        res.PushData("\r\n", 2);

    res.PushData(buf, n);
}

QError info(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    UnboundedBuffer res;

    extern Delegate<void (UnboundedBuffer& )> g_infoCollector;
    g_infoCollector(res);
    
    FormatBulk(res.ReadAddr(), res.ReadableSize(), reply);
    return QError_ok;
}


QError  monitor(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QClient::AddCurrentToMonitor();
    
    FormatOK(reply);
    return   QError_ok;
}

QError  auth(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    if (g_config.CheckPassword(params[1]))
    {
        QClient::Current()->SetAuth();
        FormatOK(reply);
    }
    else
    {
        ReplyError(QError_errAuth, reply);
    }
    
    return   QError_ok;
}

QError  slowlog(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    if (params[1] == "len")
    {
        FormatInt(static_cast<long>(QSlowLog::Instance().GetLogsCount()), reply);
    }
    else if (params[1] == "reset")
    {
        QSlowLog::Instance().ClearLogs();
        FormatOK(reply);
    }
    else if (params[1] == "get")
    {
        const long limit = static_cast<long>(QSlowLog::Instance().GetLogsCount());
        long realCnt = limit;
        if (params.size() == 3)
        {
            if (!Strtol(params[2].c_str(), params[2].size(), &realCnt))
            {
                ReplyError(QError_syntax, reply);
                return QError_syntax;
            }
        }
        
        if (realCnt > limit)
            realCnt = limit;
        
        PreFormatMultiBulk(realCnt, reply);
        for (const auto& item : QSlowLog::Instance().GetLogs())
        {
            if (realCnt -- == 0)
                break;
            
            PreFormatMultiBulk(2, reply);
            FormatInt(static_cast<long>(item.used), reply);
            
            PreFormatMultiBulk(static_cast<long>(item.cmds.size()), reply);
            for (const auto& c : item.cmds)
            {
                FormatBulk(c, reply);
            }
        }
    }
    else
    {
        ReplyError(QError_syntax, reply);
        return QError_syntax;
    }
    
    return   QError_ok;
}


// Config options get/set
//
enum ConfigType {
    Config_string,
    Config_bool,
    Config_int,
    Config_int64,
};

struct ConfigInfo
{
    ConfigType type;
    bool canModify;
    void* value;
};

                    
// TODO sanity check: use function setter
std::map<QString, ConfigInfo> configOptions = {
    {"appendonly", {Config_bool, true, &g_config.appendonly }},
    {"bind", {Config_string, false, &g_config.ip}},
    {"dbfilename", {Config_string, true, &g_config.rdbfullname}},
    {"databases", {Config_int, false, &g_config.databases}},
    {"daemonize", {Config_bool, false, &g_config.daemonize}},
    {"hz", {Config_int, false, &g_config.hz}},
    {"logfile", {Config_string, false, &g_config.logdir}},
    {"loglevel",  {Config_string, true, &g_config.loglevel}},
    {"masterauth", {Config_string, true, &g_config.masterauth}},
    {"maxclients", {Config_int, true, &g_config.maxclients}},
    {"port", {Config_int, false, &g_config.port}},
    {"requirepass", {Config_string, true, &g_config.password}},
    {"rdbchecksum", {Config_bool, false, &g_config.rdbchecksum}},
    {"rdbcompression", {Config_bool, false, &g_config.rdbcompression}},
    {"slowlog-log-slower-than", {Config_int, true, &g_config.slowlogtime}},
    {"slowlog-max-len", {Config_int, true, &g_config.slowlogmaxlen}},
    {"slaveof", {Config_string, false, &g_config.masterIp}},
    {"maxmemory", {Config_int64, true, &g_config.maxmemory}},
    {"maxmemorySamples", {Config_int, true, &g_config.maxmemorySamples}},
    {"maxmemory-noevict", {Config_bool, true, &g_config.noeviction}},
};

static std::vector<QString> GetConfig(const QString& option)
{
    std::vector<QString>  res;
    std::vector<std::map<QString, ConfigInfo>::const_iterator> iters;

    if (NotGlobRegex(option.data(), option.size()))
    {
        auto it = configOptions.find(option);
        if (it == configOptions.end())
            return  res;

        iters.push_back(it);
    }
    else
    {
        // try glob match
        for (auto it(configOptions.begin()); it != configOptions.end(); ++ it) 
        {
            if (glob_match(option, it->first))
                iters.push_back(it);
        }
    }

    for (const auto& it : iters)
    {
        res.push_back(it->first); // push option

        // push value
        switch (it->second.type)
        {
            case Config_bool:
                if (*(bool*)(it->second.value))
                    res.push_back("true");
                else
                    res.push_back("false");
                break;

            case Config_string:
                res.push_back(*(const QString*)it->second.value);
                break;

            case Config_int:
            case Config_int64:
                {
                    int64_t val = 0;
                    if (it->second.type == Config_int)
                        val = *(int*)it->second.value;
                    else
                        val = *(int64_t*)it->second.value;

                    char buf[16] = "";
                    Number2Str(buf, sizeof buf, val);
                    res.push_back(buf);
                }

                break;

            default:
                assert(!!!"invalid type");
        }
    }

    return res;
}

static QError SetConfig(const QString& option, const QString& value)
{
    auto it = configOptions.find(option);
    if (it == configOptions.end())
        return QError_syntax;

    if (!it->second.canModify)
        return QError_syntax;
        
    // set option value
    switch (it->second.type)
    {
        case Config_bool:
            *(bool*)(it->second.value) = (value == "true");
            break;

        case Config_string:
            *(QString*)it->second.value = value;
            break;

        case Config_int:
        case Config_int64:
            {
                long val = 0;
                if (Strtol(value.data(), value.size(), &val))
                {
                    if (it->second.type == Config_int)
                        *(int*)it->second.value = static_cast<int>(val);
                    else
                        *(int64_t*)it->second.value = static_cast<int64_t>(val);

                    // ugly... process slow log option
                    if (option.find("slowlog") == 0)
                    {
                        QSlowLog::Instance().SetThreshold(g_config.slowlogtime);
                        QSlowLog::Instance().SetLogLimit(static_cast<std::size_t>(g_config.slowlogmaxlen));
                    }
                }
                else
                {
                    return QError_syntax;
                }
            }
                
            break;

        default:
            assert(!!!"invalid type");
    }

    return QError_ok;
}

QError config(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    // at least 3 params
    if (strncasecmp(params[1].c_str(), "get", 3) == 0)
    {
        auto res = GetConfig(params[2]);
        PreFormatMultiBulk(res.size(), reply);
        for (const auto& e : res)
        {
            FormatBulk(e, reply);
        }
    }
    else if (strncasecmp(params[1].c_str(), "set", 3) == 0)
    {
        if (params.size() != 4)
        {
            ReplyError(QError_param, reply);
            return QError_param;
        }

        auto err = SetConfig(params[2], params[3]);
        if (err == QError_ok)
        {
            FormatOK(reply);
        }
        else
        {
            const char* format = "-ERR Invalid argument '%s' for CONFIG SET '%s'\r\n";
            char info[128];
            auto len = snprintf(info, sizeof info, format, params[3].data(), params[2].data());

            reply->PushData(info, len);
        }

        return err;
    }
    else
    {
        ReplyError(QError_syntax, reply);
        return QError_syntax;
    }

    return   QError_ok;
}
    
}

