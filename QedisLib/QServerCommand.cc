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



