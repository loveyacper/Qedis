#include "QStore.h"
#include "Log/Logger.h"
#include "QGlobRegex.h"
#include <iostream>
#include <cassert>

using namespace std;


QError  type(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "type");

    const char* info = 0;
    QType type = QSTORE.KeyType(params[1]);
    switch (type) {
        case QType_hash:
            info = "hash";
            break;
            
        case QType_set:
            info = "set";
            break;
            
        case QType_string:
            info = "string";
            break;
            
        case QType_list:
            info = "list";
            break;
            
        case QType_sortedSet:
            info = "sortedSet";
            break;
            
        default:
            info = "none";
            break;
    }

    FormatSingle(info, strlen(info), reply);
    return   QError_ok;
}

QError  exists(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "exists");

    bool exist = QSTORE.ExistsKey(params[1]);
    
    exist ? Format1(reply) : Format0(reply);
    return   QError_ok;
}

QError  del(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "del");
    
    int nDel = 0;
    for (size_t i = 1; i < params.size(); ++ i)
    {
        const QString&  key = params[i];
    
        if (QSTORE.DeleteKey(key))
        {
            QSTORE.ClearExpire(key);
            ++ nDel;
        }
    }
    
    FormatInt(nDel, reply);
    return   QError_ok;
}

static int _SetExpireByMs(const QString& key, uint64_t absTimeout)
{
    LOG_ERR(g_log) << "try set expire, key " << key.c_str() << ", timeout is " << absTimeout;

    int ret = 0;
    if (QSTORE.ExistsKey(key))
    {
        QSTORE.SetExpire(key, absTimeout);
        ret = 1;
    }

    return ret;
}

QError  expire(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "expire");
    
    const QString& key     = params[1];
    const uint64_t timeout = atoi(params[2].c_str()); // by seconds;
        
    int ret = _SetExpireByMs(key, ::Now() + timeout * 1000);

    FormatInt(ret, reply);
    return   QError_ok;
}

QError  pexpire(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "pexpire");
    
    const QString& key     = params[1];
    const uint64_t timeout = atoi(params[2].c_str()); // by milliseconds;
        
    int ret = _SetExpireByMs(key, ::Now() + timeout);

    FormatInt(ret, reply);
    return   QError_ok;
}

QError  expireat(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "expireat");
    
    const QString& key     = params[1];
    const uint64_t timeout = atoi(params[2].c_str()); // by seconds;
        
    int ret = _SetExpireByMs(key, timeout * 1000);

    FormatInt(ret, reply);
    return   QError_ok;
}

QError  pexpireat(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "pexpireat");
    
    const QString& key     = params[1];
    const uint64_t timeout = atoi(params[2].c_str()); // by milliseconds;
        
    int ret = _SetExpireByMs(key, timeout);

    FormatInt(ret, reply);
    return   QError_ok;
}


static int64_t _ttl(const QString& key)
{
    int64_t ret = -2; // not exist key
    if (QSTORE.ExistsKey(key))
    {
        int64_t  ttl = QSTORE.TTL(key, ::Now());
        if (ttl < 0)
            ret = -1; // key exist, but persist;
        else
            ret = ttl;
    }
    else
    {
        LOG_ERR(g_log) << "ttl key not exist " << key.c_str();
    }

    return  ret;
}

QError  ttl(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "ttl");
    
    const QString& key = params[1];

    int64_t ret = _ttl(key);
    if (ret > 0)  ret /= 1000; // by seconds

    FormatInt(ret, reply);

    return   QError_ok;
}

QError  pttl(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "pttl");
    
    const QString& key = params[1];

    int64_t ret = _ttl(key); // by milliseconds
    FormatInt(ret, reply);

    return   QError_ok;
}

QError  persist(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "persist");
    
    const QString& key = params[1];

    int ret = QSTORE.ClearExpire(key) ? 1 : 0;

    FormatInt(ret, reply);
    return   QError_ok;
}

QError  move(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "move");
    
    const QString& key = params[1];
    int   toDb         = atoi(params[2].c_str());

    int    ret = 0;

    QObject* val;
    if (QSTORE.GetValue(key, val) == QError_ok)
    {
        LOG_DBG(g_log) << "move " << key.c_str() << " to db " << toDb;
        int fromDb = QSTORE.SelectDB(toDb);
        if (fromDb >= 0 && fromDb != toDb && !QSTORE.ExistsKey(key))
        {
            QSTORE.SelectDB(fromDb);
            QSTORE.ClearExpire(key);
            QSTORE.DeleteKey(key); // delete from old db

            QSTORE.SelectDB(toDb);
            QSTORE.SetValue(key, *val); // set to new db
            ret = 1;
        }
        else
        {
            LOG_ERR(g_log) << "move " << key.c_str() << " failed to db " << toDb << ", from db " << fromDb;
        }
    }
    else
    {
        LOG_ERR(g_log) << "move " << key.c_str() << " failed to db " << toDb;
    }

    FormatInt(ret, reply);
    return   QError_ok;
}

QError  keys(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "keys");
    
    const QString& pattern = params[1];
    
    std::vector<const QString* > results;
    for (QDB::const_iterator it(QSTORE.begin());
         it != QSTORE.end();
         ++ it)
    {
        if (glob_match(pattern, it->first))
        {
            results.push_back(&it->first);
        }
    }
    
    PreFormatMultiBulk(results.size(), reply);
    for (std::vector<const QString* >::const_iterator it(results.begin());
         it != results.end();
         ++ it)
    {
        FormatBulk((*it)->c_str(), (*it)->size(), reply); 
    }

    return   QError_ok;
}


QError  randomkey(const vector<QString>& params, UnboundedBuffer* reply)
{
    const QString& res = QSTORE.RandomKey();
  
    if (res.empty())
    {
        FormatNull(reply);
    }
    else
    {
        FormatSingle(res.c_str(), res.size(), reply);
    }
    
    return   QError_ok;
}
