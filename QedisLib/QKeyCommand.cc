#include "QStore.h"
#include "Log/Logger.h"
#include "QGlobRegex.h"
#include <cassert>


QError  type(const std::vector<QString>& params, UnboundedBuffer* reply)
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

QError  exists(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "exists");

    if (QSTORE.ExistsKey(params[1]))
        Format1(reply);
    else
        Format0(reply);

    return   QError_ok;
}

QError  del(const std::vector<QString>& params, UnboundedBuffer* reply)
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
    INF << "try set expire, key " << key.c_str() << ", timeout is " << absTimeout;

    int ret = 0;
    if (QSTORE.ExistsKey(key))
    {
        QSTORE.SetExpire(key, absTimeout);
        ret = 1;
    }

    return ret;
}

QError  expire(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    const QString& key     = params[1];
    const uint64_t timeout = atoi(params[2].c_str()); // by seconds;
        
    int ret = _SetExpireByMs(key, ::Now() + timeout * 1000);

    FormatInt(ret, reply);
    return   QError_ok;
}

QError  pexpire(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    const QString& key     = params[1];
    const uint64_t timeout = atoi(params[2].c_str()); // by milliseconds;
        
    int ret = _SetExpireByMs(key, ::Now() + timeout);

    FormatInt(ret, reply);
    return   QError_ok;
}

QError  expireat(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    const QString& key     = params[1];
    const uint64_t timeout = atoi(params[2].c_str()); // by seconds;
        
    int ret = _SetExpireByMs(key, timeout * 1000);

    FormatInt(ret, reply);
    return   QError_ok;
}

QError  pexpireat(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    const QString& key     = params[1];
    const uint64_t timeout = atoi(params[2].c_str()); // by milliseconds;
        
    int ret = _SetExpireByMs(key, timeout);

    FormatInt(ret, reply);
    return   QError_ok;
}


static int64_t _ttl(const QString& key)
{
    int64_t ret = QStore::ExpireResult::notExist;
    if (QSTORE.ExistsKey(key))
    {
        int64_t  ttl = QSTORE.TTL(key, ::Now());
        if (ttl < 0)
            ret = QStore::ExpireResult::persist;
        else
            ret = ttl;
    }
    else
    {
        ERR << "ttl key not exist " << key.c_str();
    }

    return  ret;
}

QError  ttl(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "ttl");
    
    const QString& key = params[1];

    int64_t ret = _ttl(key);
    if (ret > 0)  ret /= 1000; // by seconds

    FormatInt(ret, reply);

    return   QError_ok;
}

QError  pttl(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "pttl");
    
    const QString& key = params[1];

    int64_t ret = _ttl(key); // by milliseconds
    FormatInt(ret, reply);

    return   QError_ok;
}

QError  persist(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "persist");
    
    const QString& key = params[1];

    int ret = QSTORE.ClearExpire(key) ? 1 : 0;

    FormatInt(ret, reply);
    return   QError_ok;
}

QError  move(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "move");
    
    const QString& key = params[1];
    int   toDb         = atoi(params[2].c_str());

    int    ret = 0;

    QObject* val;
    if (QSTORE.GetValue(key, val) == QError_ok)
    {
        DBG << "move " << key.c_str() << " to db " << toDb;
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
            ERR << "move " << key.c_str() << " failed to db " << toDb << ", from db " << fromDb;
        }
    }
    else
    {
        ERR << "move " << key.c_str() << " failed to db " << toDb;
    }

    FormatInt(ret, reply);
    return   QError_ok;
}

QError  keys(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "keys");
    
    const QString& pattern = params[1];
    
    std::vector<const QString* > results;
    for (const auto& kv : QSTORE)
    {
        if (glob_match(pattern, kv.first))
            results.push_back(&kv.first);
    }
    
    PreFormatMultiBulk(results.size(), reply);
    for (auto e : results)
    {
        FormatBulk(*e, reply);
    }

    return   QError_ok;
}


QError  randomkey(const std::vector<QString>& params, UnboundedBuffer* reply)
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

static QError  RenameKey(const QString& oldKey, const QString& newKey, bool force)
{
    QObject* val;
    
    QError err = QSTORE.GetValue(oldKey, val);
    if (err != QError_ok)
        return  err;
    
    if (!force && QSTORE.ExistsKey(newKey))
        return QError_exist;
    
    auto now = ::Now();
    auto ttl = QSTORE.TTL(oldKey, now);
    
    if (ttl == QStore::expired)
        return QError_notExist;
    
    QSTORE.SetValue(newKey, *val);
    if (ttl > 0)
        QSTORE.SetExpire(newKey, ttl + now);
    else if (ttl == QStore::persist)
        QSTORE.ClearExpire(newKey);
    
    QSTORE.ClearExpire(oldKey);
    QSTORE.DeleteKey(oldKey);
    
    return QError_ok;
}

QError  rename(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QError err = RenameKey(params[1], params[2], true);
    
    ReplyError(err, reply);
    return  err;
}

QError  renamenx(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QError err = RenameKey(params[1], params[2], false);
    
    if (err == QError_ok)
        Format1(reply);
    else
        ReplyError(err, reply);
    
    return  err;
}

// helper func scan
static QError  ParseScanOption(const std::vector<QString>& params, int start, long& count, const char*& pattern)
{
    // scan cursor  MATCH pattern  COUNT 1
    count  = -1;
    pattern = nullptr;
    for (std::size_t i = start; i < params.size(); i += 2)
    {
        if (params[i].size() == 5)
        {
            if (strncasecmp(params[i].c_str(), "match", 5) == 0)
            {
                if (!pattern)
                {
                    pattern = params[i + 1].c_str();
                    continue;
                }
            }
            else if (strncasecmp(params[i].c_str(), "count", 5) == 0)
            {
                if (count == -1)
                {
                    if (Strtol(params[i+1].c_str(), params[i+1].size(), &count))
                        continue;
                }
            }
        }
        
        return QError_param;
    }

    return  QError_ok;
}

QError  scan(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    if (params.size() % 2 != 0)
    {
        ReplyError(QError_param, reply);
        return QError_param;
    }
    
    long   cursor = 0;

    if (!Strtol(params[1].c_str(), params[1].size(), &cursor))
    {
        ReplyError(QError_param, reply);
        return QError_param;
    }
    
    // scan cursor  MATCH pattern  COUNT 1
    long   count  = -1;
    const char* pattern = nullptr;
    
    QError err  = ParseScanOption(params, 2, count, pattern);
    if (err != QError_ok)
    {
        ReplyError(err, reply);
        return err;
    }
    
    if (count < 0)  count = 5;
    
    std::vector<QString>  res;
    auto newCursor = QSTORE.ScanKey(cursor, count, res);
  
    // filter by pattern
    if (pattern)
    {
        for (auto it = res.begin(); it != res.end(); )
        {
            if (!glob_match(pattern, (*it).c_str()))
                it = res.erase(it);
            else
                ++ it;
        }
    }
    
    // reply
    PreFormatMultiBulk(2, reply);

    char buf[32];
    auto len = snprintf(buf, sizeof buf -1, "%lu", newCursor);
    FormatBulk(buf, len, reply);

    PreFormatMultiBulk(res.size(), reply);
    for (const auto& s : res)
    {
        FormatBulk(s, reply);
    }
    
    return   QError_ok;
}


QError  hscan(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    // hscan key cursor COUNT 0 MATCH 0
    if (params.size() % 2 == 0)
    {
        ReplyError(QError_param, reply);
        return QError_param;
    }
    
    long   cursor = 0;
    
    if (!Strtol(params[2].c_str(), params[2].size(), &cursor))
    {
        ReplyError(QError_param, reply);
        return QError_param;
    }
    
    // find hash
    QObject* value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_hash);
    if (err != QError_ok)
    {
        ReplyError(err, reply);
        return err;
    }
    
    // parse option
    long   count  = -1;
    const char* pattern = nullptr;
    
    err  = ParseScanOption(params, 3, count, pattern);
    if (err != QError_ok)
    {
        ReplyError(err, reply);
        return err;
    }
    
    if (count < 0)  count = 5;
    
    // scan
    std::vector<QString>  res;
    auto newCursor = HScanKey(*value->CastHash(), cursor, count, res);
    
    // filter by pattern
    if (pattern)
    {
        for (auto it = res.begin(); it != res.end(); )
        {
            if (!glob_match(pattern, (*it).c_str()))
            {
                it = res.erase(it); // erase key
                it = res.erase(it); // erase value
            }
            else
            {
                ++ it, ++ it;
            }
        }
    }
    
    // reply
    PreFormatMultiBulk(2, reply);
    
    char buf[32];
    auto len = snprintf(buf, sizeof buf -1, "%lu", newCursor);
    FormatBulk(buf, len, reply);
    
    PreFormatMultiBulk(res.size(), reply);
    for (const auto& s : res)
    {
        FormatBulk(s, reply);
    }
    
    return   QError_ok;
}


QError  sscan(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    // sscan key cursor COUNT 0 MATCH 0
    if (params.size() % 2 == 0)
    {
        ReplyError(QError_param, reply);
        return QError_param;
    }
    
    long   cursor = 0;
    
    if (!Strtol(params[2].c_str(), params[2].size(), &cursor))
    {
        ReplyError(QError_param, reply);
        return QError_param;
    }
    
    // find set
    QObject* value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_set);
    if (err != QError_ok)
    {
        ReplyError(err, reply);
        return err;
    }
    
    // parse option
    long   count  = -1;
    const char* pattern = nullptr;
    
    err  = ParseScanOption(params, 3, count, pattern);
    if (err != QError_ok)
    {
        ReplyError(err, reply);
        return err;
    }
    
    if (count < 0)  count = 5;
    
    // scan
    std::vector<QString>  res;
    auto newCursor = SScanKey(*value->CastSet(), cursor, count, res);
    
    // filter by pattern
    if (pattern)
    {
        for (auto it = res.begin(); it != res.end(); )
        {
            if (!glob_match(pattern, (*it).c_str()))
            {
                it = res.erase(it);
            }
            else
            {
                ++ it;
            }
        }
    }
    
    // reply
    PreFormatMultiBulk(2, reply);
    
    char buf[32];
    auto len = snprintf(buf, sizeof buf -1, "%lu", newCursor);
    FormatBulk(buf, len, reply);
    
    PreFormatMultiBulk(res.size(), reply);
    for (const auto& s : res)
    {
        FormatBulk(s, reply);
    }
    
    return   QError_ok;
}

