#include "QStore.h"
#include "QClient.h"
#include "Log/Logger.h"
#include "Server.h"
#include <cassert>
#include <unistd.h>
#include "QDB.h"
#include "QAOF.h"
#include "QReplication.h"
#include "QConfig.h"

using namespace std;


QError  select(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "select");

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


QError  dbsize(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "dbsize");

    FormatInt(static_cast<long>(QSTORE.DBSize()), reply);
    return   QError_ok;
}

QError  flushdb(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "flushdb");

    QSTORE.ClearCurrentDB();
    
    FormatOK(reply);
    return   QError_ok;
}

QError  flushall(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "flushall");
    
    QSTORE.ResetDb();
    
    FormatOK(reply);
    return   QError_ok;
}

QError  bgsave(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "bgsave");
    
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
        qdb.Save(g_config.rdbfilename.c_str());
        INF << "child save rdb done, exiting child";
        }
        exit(0);
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

QError  save(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "save");
    
    if (g_qdbPid != -1 || g_rewritePid != -1)
    {
        FormatBulk("-ERR Background save or aof already in progress",
                   sizeof "-ERR Background save or aof already in progress" - 1,
                   reply);
        
        return QError_ok;
    }
    
    QDBSaver  qdb;
    qdb.Save(g_config.rdbfilename.c_str());
    g_lastQDBSave = time(NULL);

    FormatOK(reply);
    return   QError_ok;
}

QError  lastsave(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "lastsave");
    
    FormatInt(g_lastQDBSave, reply);
    return   QError_ok;
}

QError  client(const vector<QString>& params, UnboundedBuffer* reply)
{
    // getname   setname    kill  list
    assert (params[0] == "client");
    
    QError   err = QError_ok;
    
    if (params[1] == "getname")
    {
        if (params.size() != 2)
            ReplyError(err = QError_param, reply);
        else
            FormatBulk(QClient::Current()->GetName(),
                       reply);
    }
    else if (params[1] == "setname")
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
    else if (params[1] == "kill")
    {
        // only kill current client
        QClient::Current()->OnError();
        FormatOK(reply);
    }
    else if (params[1] == "list")
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

QError  debug(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "debug");
    
    QError err = QError_ok;
    
    if (strcasecmp(params[1].c_str(), "segfault") == 0 && params.size() == 2)
    {
        Suicide();
        assert (false);
    }
    else if (strcasecmp(params[1].c_str(), "object") == 0 && params.size() == 3)
    {
        QObject* obj = nullptr;
        err = QSTORE.GetValue(params[2], obj);
        
        if (err != QError_ok)
        {
            ReplyError(err, reply);
        }
        else
        {
            // ref count,  encoding
            char buf[512];
            int  len = snprintf(buf, sizeof buf, "ref count:%ld, encoding:%s",
                                obj->value.use_count(),
                                EncodingStringInfo(obj->encoding));
            FormatBulk(buf, len, reply);
        }
    }
    else
    {
        ReplyError(err = QError_param, reply);
    }
    
    return   err;
}


QError  shutdown(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "shutdown");
    
    Server::Instance()->Terminate();
    return   QError_ok;
}



QError  sync(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QClient* cli = QClient::Current();
    auto   slave = cli->GetSlaveInfo();
    if (!slave)
    {
        cli->SetSlaveInfo();
        slave = cli->GetSlaveInfo();
        QReplication::Instance().AddSlave(cli);
    }
    
    if (slave->state == QSlaveState_wait_bgsave_end ||
        slave->state == QSlaveState_online)
    {
        WRN << cli->GetName() << " state is "
            << slave->state << ", ignore this request sync";
        return QError_ok;
    }
    
    slave->state = QSlaveState_wait_bgsave_start;
    QReplication::Instance().TryBgsave();

    return QError_ok;
}


QError  ping(const vector<QString>& params, UnboundedBuffer* reply)
{
    FormatSingle("PONG", 4, reply);
    return   QError_ok;
}


QError  slaveof(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    auto& info = QReplication::Instance().GetMasterInfo();
    
    if (params[1] == "no" && params[2] == "one")
    {
        info.addr.Clear();
    }
    else
    {
        info.addr.Init(params[1].c_str(), std::stoi(params[2]));
        info.state = QReplState_none;
    }
    
    FormatOK(reply);
    return QError_ok;
}


QError  info(const vector<QString>& params, UnboundedBuffer* reply)
{
    char buf[1024];
    int  offset = 0;
    int  n = 0;
    
    // clients
    n = snprintf(buf + offset, sizeof buf - 1 - offset,
                                          "# Clients\n"
                                          "connected_clients:%lu\n"
                                          "blocked_clients:%lu\n"
                                          , Server::Instance()->TCPSize()
                                          , QSTORE.BlockedSize());
    offset += n;
    // replication
    /*# Replication
     role:master
     connected_slaves:1
     slave0:127.0.0.1,0,online*/
    
    FormatSingle(buf, offset, reply);

    return   QError_ok;
}


QError  monitor(const vector<QString>& params, UnboundedBuffer* reply)
{
    QClient::AddCurrentToMonitor();
    
    FormatOK(reply);
    return   QError_ok;
}
