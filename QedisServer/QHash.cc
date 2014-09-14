#include "QHash.h"
#include "QStore.h"
#include "Log/Logger.h"
#include <iostream>
#include <cassert>

using namespace std;

QObject  CreateHash()
{
    QObject obj(QType_hash);
    obj.encoding = QEncode_hash;
    return  obj;
}

#define GET_HASH(hashname)  \
    QObject value(QType_hash);  \
    QError err = QSTORE.GetValueByType(hashname, value, QType_hash);  \
    if (err != QError_ok)  {  \
        ReplyErrorInfo(err, reply); \
        return err;  \
}

#define GET_OR_SET_HASH(hashname)  \
    QObject  value(QType_hash); \
    QError err = QSTORE.GetValueByType(hashname, value, QType_hash);  \
    if (err == QError_ok && err != QError_notExist)  {  \
        ReplyErrorInfo(err, reply); \
        return err;  \
    }   \
    if (err == QError_notExist) { \
        value.value.Reset(new QHash); \
        err = QSTORE.SetValue(hashname, value);  \
        if (err != QError_ok)  {  \
            ReplyErrorInfo(err, reply);  \
            return err; \
        } \
    }


QHash::iterator  _set_hash_force(QHash& hash, const QString& key, const QString& val)
{
    QHash::iterator it(hash.find(key));
    if (it != hash.end())
        it->second = val;
    else
    {
        it = hash.insert(QHash::value_type(key, val)).first;
    }

    return it;
}

bool _set_hash_if_notexist(QHash& hash, const QString& key, const QString& val)
{
    return hash.insert(QHash::value_type(key, val)).second;
}

QError  hset(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "hset");
    assert (params.size() == 4);
    
    GET_OR_SET_HASH(params[1]);
    
    const PHASH& hash= value.CastHash();
    _set_hash_force(*hash, params[2], params[3]);
    
    FormatInt(1, reply);

    return   QError_ok;
}

QError  hmset(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "hmset");

    if (params.size() % 2 != 0)
    {
        ReplyErrorInfo(QError_paramNotMatch, reply);
        return QError_paramNotMatch;
    }
    
    GET_OR_SET_HASH(params[1]);

    const PHASH& hash= value.CastHash();
    for (int i = 2; i < static_cast<int>(params.size()); i += 2)
        _set_hash_force(*hash, params[i], params[i + 1]);
    
    FormatSingle("OK", 2, reply);
    return   QError_ok;
}


QError  hget(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "hget");

    GET_HASH(params[1]);
    
    const PHASH& hash= value.CastHash();
    QHash::const_iterator it = hash->find(params[2]);

    if  (it != hash->end())
        FormatSingle(it->second.c_str(), static_cast<int>(it->second.size()), reply);
    else
        FormatNull(reply);

    return   QError_ok;
}


QError  hmget(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "hmget");

    GET_HASH(params[1]);

    PreFormatMultiBulk(static_cast<int>(params.size()) - 2, reply);

    const PHASH& hash= value.CastHash();
    for (int i = 2; i < static_cast<int>(params.size()); ++ i)
    {
        QHash::const_iterator it = hash->find(params[i]);
        if (it != hash->end())
            FormatSingle(it->second.c_str(), static_cast<int>(it->second.size()), reply);
        else
            FormatNull(reply);
    }
    
    return   QError_ok;
}

QError  hgetall(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "hgetall");

    GET_HASH(params[1]);

    const PHASH& hash= value.CastHash();
    PreFormatMultiBulk(2 * static_cast<int>(hash->size()), reply);

    QHash::const_iterator  it(hash->begin());
    for ( ; it != hash->end(); ++ it)
    {
        FormatBulk(it->first.c_str(), static_cast<int>(it->first.size()), reply);
        FormatBulk(it->second.c_str(), static_cast<int>(it->second.size()), reply);
    }
    
    return   QError_ok;
}

QError  hkeys(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "hkeys");

    GET_HASH(params[1]);

    const PHASH& hash= value.CastHash();
    PreFormatMultiBulk(static_cast<int>(hash->size()), reply);

    QHash::const_iterator  it(hash->begin());
    for ( ; it != hash->end(); ++ it)
    {
        FormatBulk(it->first.c_str(), static_cast<int>(it->first.size()), reply);
    }
    
    return   QError_ok;
}

QError  hvals(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "hvals");

    GET_HASH(params[1]);

    const PHASH& hash= value.CastHash();
    PreFormatMultiBulk(static_cast<int>(hash->size()), reply);

    QHash::const_iterator  it(hash->begin());
    for ( ; it != hash->end(); ++ it)
    {
        FormatBulk(it->second.c_str(), static_cast<int>(it->second.size()), reply);
    }
    
    return   QError_ok;
}

QError  hdel(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "hdel");

    QObject  value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_hash);
    if (err != QError_ok)
    {
        ReplyErrorInfo(err, reply);
        return err;
    }

    int    del  = 0;
    const PHASH&  hash = value.CastHash();
    for (int i = 2; i < static_cast<int>(params.size()); ++ i)
    {
        QHash::iterator it = hash->find(params[i]);

        if (it != hash->end())
        {
            hash->erase(it);
            ++ del;
        }
    }
            
    FormatInt(del, reply);

    return QError_ok;
}

QError  hexists(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "hexists");
  
    GET_HASH(params[1]);

    const PHASH& hash= value.CastHash();
    QHash::const_iterator it = hash->find(params[2]);

    if (it != hash->end())
        FormatInt(1, reply);
    else
        FormatInt(0, reply);

    return QError_ok;
}

QError  hlen(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "hlen");

    GET_HASH(params[1]);

    const PHASH& hash= value.CastHash();
    FormatInt(static_cast<int>(hash->size()), reply);

    return QError_ok;
}

QError  hincrby(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "hincrby");

    GET_OR_SET_HASH(params[1]);
    
    const PHASH& hash = value.CastHash();
    long    val  = 0;
    QString* str = 0;
    QHash::iterator it(hash->find(params[2]));
    if (it != hash->end())
    {
        str = &it->second;
        if (Strtol(str->c_str(), static_cast<int>(str->size()), &val))
        {
            val += atoi(params[3].c_str());
        }
        else
        {
            ReplyErrorInfo(QError_paramNotMatch, reply);
            return QError_paramNotMatch;
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

QError  hincrbyfloat(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "hincrbyfloat");

    GET_OR_SET_HASH(params[1]);
    
    const PHASH& hash = value.CastHash();
    float  val  = 0;
    QString* str = 0;
    QHash::iterator it(hash->find(params[2]));
    if (it != hash->end())
    {
        str = &it->second;
        if (Strtof(str->c_str(), static_cast<int>(str->size()), &val))
        {
            val += atof(params[3].c_str());
        }
        else
        {
            ReplyErrorInfo(QError_paramNotMatch, reply);
            return QError_paramNotMatch;
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

    FormatSingle(str->c_str(), static_cast<int>(str->size()), reply);

    return QError_ok;
}

QError  hsetnx(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "hsetnx");

    GET_OR_SET_HASH(params[1]);
    
    const PHASH& hash= value.CastHash();
    if (_set_hash_if_notexist(*hash, params[2], params[3]))
        FormatInt(1, reply);
    else
        FormatInt(0, reply);

    return   QError_ok;
}

