#include "QStore.h"
#include "QClient.h"
#include "Log/Logger.h"
#include <iostream>
#include <cassert>
#include <unistd.h>
#include "QDB.h"

using namespace std;


QError  select(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "select");

    int newDb = atoi(params[1].c_str());
    
    if (QClient::Current()->SelectDB(newDb))
        FormatOK(reply);
    else
        ReplyError(QError_invalidDB, reply);

    return   QError_ok;
}


QError  dbsize(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "dbsize");

    FormatInt(static_cast<long>(QSTORE.DBSize()), reply);
    return   QError_ok;
}

QError  flushdb(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "flushdb");

    QSTORE.ClearCurrentDB();
    
    FormatOK(reply);
    return   QError_ok;
}

QError  flushall(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "flushall");
    
    QSTORE.ClearAllDB();
    
    FormatOK(reply);
    return   QError_ok;
}

QError  bgsave(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "bgsave");
   
    int ret = fork();
    if (ret == 0)
    {
        {
        QDBSaver  qdb;
        qdb.Save();
        std::cerr << "child save rdb\n";
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

QError  save(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "save");
    
    QDBSaver  qdb;
    qdb.Save();
    g_lastQDBSave = time(NULL);

    FormatOK(reply);
    return   QError_ok;
}

QError  lastsave(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "lastsave");
    
    FormatInt(g_lastQDBSave, reply);
    return   QError_ok;
}

QError  client(const vector<QString>& params, UnboundedBuffer& reply)
{
    // getname   setname    kill  list
    assert (params[0] == "client");
    
    QError   err = QError_ok;
    
    if (params[1] == "getname")
    {
        if (params.size() != 2)
            ReplyError(err = QError_param, reply);
        else
            FormatBulk(QClient::Current()->GetName().c_str(),
                       QClient::Current()->GetName().size(),
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

QError  debug(const vector<QString>& params, UnboundedBuffer& reply)
{
    // SEGFAULT   OBJECT
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
            int len = snprintf(buf, sizeof buf, "ref count:%d, encoding:%s", obj->value.use_count(), EncodingStringInfo(obj->encoding));
            FormatBulk(buf, len, reply);
        }
    }
    else
    {
        ReplyError(err = QError_param, reply);
    }
    
    return   err;
    
}