#include "QHash.h"
#include "QStore.h"
#include <cassert>

namespace qedis
{

QObject QObject::CreateHash()
{
    QObject obj(QType_hash);
    obj.value = std::make_shared<QHash>();
    return obj;
}

#define GET_HASH(hashname)  \
    QObject* value;  \
    QError err = QSTORE.GetValueByType(hashname, value, QType_hash);  \
    if (err != QError_ok)  {  \
        ReplyError(err, reply); \
        return err;  \
}

#define GET_OR_SET_HASH(hashname)  \
    QObject* value;  \
    QError err = QSTORE.GetValueByType(hashname, value, QType_hash);  \
    if (err != QError_ok && err != QError_notExist)  {  \
        ReplyError(err, reply); \
        return err;  \
    }   \
    if (err == QError_notExist) { \
        value = QSTORE.SetValue(hashname, QObject::CreateHash());  \
    }


QHash::iterator  _set_hash_force(QHash& hash, const QString& key, const QString& val)
{
    QHash::iterator it(hash.find(key));
    if (it != hash.end())
        it->second = val;
    else
        it = hash.insert(QHash::value_type(key, val)).first;

    return it;
}

bool _set_hash_if_notexist(QHash& hash, const QString& key, const QString& val)
{
    return hash.insert(QHash::value_type(key, val)).second;
}

QError  hset(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    GET_OR_SET_HASH(params[1]);
    
    const PHASH& hash= value->CastHash();
    _set_hash_force(*hash, params[2], params[3]);
    
    FormatInt(1, reply);

    return   QError_ok;
}

QError  hmset(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    if (params.size() % 2 != 0)
    {
        ReplyError(QError_param, reply);
        return QError_param;
    }
    
    GET_OR_SET_HASH(params[1]);

    const PHASH& hash= value->CastHash();
    for (size_t i = 2; i < params.size(); i += 2)
        _set_hash_force(*hash, params[i], params[i + 1]);
    
    FormatOK(reply);
    return   QError_ok;
}


QError  hget(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    GET_HASH(params[1]);
    
    const PHASH& hash= value->CastHash();
    auto it = hash->find(params[2]);

    if  (it != hash->end())
        FormatBulk(it->second, reply);
    else
        FormatNull(reply);

    return   QError_ok;
}


QError  hmget(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    GET_HASH(params[1]);

    PreFormatMultiBulk(params.size() - 2, reply);

    const PHASH& hash= value->CastHash();
    for (size_t i = 2; i < params.size(); ++ i)
    {
        auto it = hash->find(params[i]);
        if (it != hash->end())
            FormatBulk(it->second, reply);
        else
            FormatNull(reply);
    }
    
    return   QError_ok;
}

QError  hgetall(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    GET_HASH(params[1]);

    const PHASH& hash= value->CastHash();
    PreFormatMultiBulk(2 * hash->size(), reply);
    
    for (const auto& kv : *hash)
    {
        FormatBulk(kv.first, reply);
        FormatBulk(kv.second, reply);
    }
    
    return   QError_ok;
}

QError  hkeys(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    GET_HASH(params[1]);

    const PHASH& hash= value->CastHash();
    PreFormatMultiBulk(hash->size(), reply);

    for (const auto& kv : *hash)
    {
        FormatBulk(kv.first, reply);
    }
    
    return   QError_ok;
}

QError  hvals(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    GET_HASH(params[1]);

    const PHASH& hash= value->CastHash();
    PreFormatMultiBulk(hash->size(), reply);
    
    for (const auto& kv : *hash)
    {
        FormatBulk(kv.second, reply);
    }
    
    return   QError_ok;
}

QError  hdel(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QObject* value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_hash);
    if (err != QError_ok)
    {
        ReplyError(err, reply);
        return err;
    }

    int    del  = 0;
    const PHASH&  hash = value->CastHash();
    for (size_t i = 2; i < params.size(); ++ i)
    {
        auto it = hash->find(params[i]);

        if (it != hash->end())
        {
            hash->erase(it);
            ++ del;
        }
    }
            
    FormatInt(del, reply);

    return QError_ok;
}

QError  hexists(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    GET_HASH(params[1]);

    const PHASH& hash= value->CastHash();
    auto it = hash->find(params[2]);

    if (it != hash->end())
        FormatInt(1, reply);
    else
        FormatInt(0, reply);

    return QError_ok;
}

QError  hlen(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    GET_HASH(params[1]);

    const PHASH& hash= value->CastHash();
    FormatInt(hash->size(), reply);

    return QError_ok;
}

QError  hincrby(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    GET_OR_SET_HASH(params[1]);
    
    const PHASH& hash = value->CastHash();
    long    val  = 0;
    QString* str = 0;
    auto it(hash->find(params[2]));
    if (it != hash->end())
    {
        str = &it->second;
        if (Strtol(str->c_str(), static_cast<int>(str->size()), &val))
        {
            val += atoi(params[3].c_str());
        }
        else
        {
            ReplyError(QError_nan, reply);
            return QError_nan;
        }
    }
    else
    {
        val  = atoi(params[3].c_str());
        QHash::iterator it = _set_hash_force(*hash, params[2], "");
        str  = &it->second;
    }

    char tmp[32];
    int len = snprintf(tmp, sizeof tmp - 1, "%ld", val);
    *str = tmp;

    (void)len;

    FormatInt(val, reply);

    return QError_ok;
}

QError  hincrbyfloat(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    GET_OR_SET_HASH(params[1]);
    
    const PHASH& hash = value->CastHash();
    float  val  = 0;
    QString* str = 0;
    auto it(hash->find(params[2]));
    if (it != hash->end())
    {
        str = &it->second;
        if (Strtof(str->c_str(), static_cast<int>(str->size()), &val))
        {
            val += atof(params[3].c_str());
        }
        else
        {
            ReplyError(QError_param, reply);
            return QError_param;
        }
    }
    else
    {
        val  = atof(params[3].c_str());
        QHash::iterator it = _set_hash_force(*hash, params[2], "");
        str  = &it->second;
    }

    char tmp[32];
    int len = snprintf(tmp, sizeof tmp - 1, "%f", val);
    *str = tmp;
    (void)len;

    FormatBulk(*str, reply);

    return QError_ok;
}

QError  hsetnx(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    GET_OR_SET_HASH(params[1]);
    
    const PHASH& hash= value->CastHash();
    if (_set_hash_if_notexist(*hash, params[2], params[3]))
        FormatInt(1, reply);
    else
        FormatInt(0, reply);

    return   QError_ok;
}

QError  hstrlen(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QObject* value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_hash);
    if (err != QError_ok) 
    {
        Format0(reply);
        return err;
    }
    
    const PHASH& hash= value->CastHash();
    auto it = hash->find(params[2]);
    if (it == hash->end())
        Format0(reply);
    else
        FormatInt(static_cast<long>(it->second.size()), reply);

    return   QError_ok;
}

size_t   HScanKey(const QHash& hash, size_t cursor, size_t count, std::vector<QString>& res)
{
    if (hash.empty())
        return 0;
    
    std::vector<QHash::const_local_iterator>  iters;
    size_t  newCursor = ScanHashMember(hash, cursor, count, iters);
    
    res.reserve(iters.size());
    for (auto it : iters)
        res.push_back(it->first), res.push_back(it->second);
    
    return newCursor;
}

}
