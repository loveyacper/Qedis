#include "QString.h"
#include "QStore.h"
#include "Log/Logger.h"
#include <cassert>

using namespace std;

QObject  CreateStringObject(const QString&  value)
{
    QObject   obj(QType_string);

    long  val;
    if (Strtol(value.c_str(), value.size(), &val))
    {
        obj.encoding = QEncode_int;
        obj.value.reset((void*)val, [](void* ) {});
        LOG_DBG(g_log) << "set long value " << val;
    }
    else
    {
        obj.encoding = QEncode_raw;
        obj.value = std::make_shared<QString>(value);
    }

    return obj;
}

QObject  CreateStringObject(long val)
{
    QObject   obj(QType_string);
    
    obj.encoding = QEncode_int;
    obj.value.reset((void*)val, [](void* ) {});
    DBG << "set long val " << val;
    
    return obj;
}

static PSTRING GetDecodedString(QObject* value)
{
    if (value->encoding == QEncode_raw)
    {
        return value->CastString();
    }
    else if (value->encoding == QEncode_int)
    {
        intptr_t val = (intptr_t)value->value.get();
        
        char ret[32];
        snprintf(ret, sizeof ret - 1, "%ld",  val);
        return PSTRING(new QString(ret));
    }
    else
    {
        assert (false);
    }
        
    return PSTRING();
}

static bool SetValue(const QString& key, const QString& value, bool exclusive = false)
{
    if (exclusive)
    {
        QObject*   val;
        if (QSTORE.GetValue(key, val) == QError_ok)
        {
            return false;
        }
    }

    const QObject& obj = CreateStringObject(value);

    QSTORE.ClearExpire(key); // clear key's old ttl
    QSTORE.SetValue(key, obj);

    return  true;
}

QError  set(const vector<QString>& params, UnboundedBuffer* reply)
{
    SetValue(params[1], params[2]);
    FormatOK(reply);
    return   QError_ok;
}

QError  setnx(const vector<QString>& params, UnboundedBuffer* reply)
{
    if (SetValue(params[1], params[2], true))
        Format1(reply);
    else
        Format0(reply);

    return  QError_ok;
}

QError  mset(const vector<QString>& params, UnboundedBuffer* reply)
{
    if (params.size() % 2 != 1)
    {
        ReplyError(QError_param, reply);
        return QError_param;
    }

    for (size_t i = 1; i < params.size(); i += 2)
    {
        QSTORE.SetValue(params[i], CreateStringObject(params[i + 1]));
    }
    
    FormatOK(reply);
    
    return   QError_ok;
}

QError  msetnx(const vector<QString>& params, UnboundedBuffer* reply)
{
    if (params.size() % 2 != 1)
    {
        ReplyError(QError_param, reply);
        return QError_param;
    }

    for (size_t i = 1; i < params.size(); i += 2)
    {
        QObject*  val;
        if (QSTORE.GetValue(params[i], val) == QError_ok)
        {
            Format0(reply);
            return QError_ok;
        }
    }

    for (size_t i = 1; i < params.size(); i += 2)
    {
        SetValue(params[i], params[i + 1]);
    }

    Format1(reply);
    return  QError_ok;
}

QError  setex(const vector<QString>& params, UnboundedBuffer* reply)
{
    long  seconds;
    if (!Strtol(params[2].c_str(), params[2].size(), &seconds))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }
    
    const auto& key = params[1];
    QSTORE.SetValue(key, CreateStringObject(params[3]));
    QSTORE.SetExpire(key, ::Now() + seconds * 1000);

    FormatOK(reply);
    return  QError_ok;
}

QError  setrange(const vector<QString>& params, UnboundedBuffer* reply)
{
    long offset;
    if (!Strtol(params[2].c_str(), params[2].size(), &offset))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }

    QObject*   value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_string);
    if (err != QError_ok)
    {
        if (err == QError_notExist)
        {
            value = QSTORE.SetValue(params[1], CreateStringObject(""));
        }
        else
        {
            ReplyError(err, reply); 
            return err;  
        }
    }

    const PSTRING& str = GetDecodedString(value);
    const size_t newSize = offset + params[3].size();

    if (newSize > str->size())  str->resize(newSize, '0');
    str->replace(offset, params[3].size(), params[3]);

    FormatInt(static_cast<long>(str->size()), reply);
    return   QError_ok;
}


static void AddReply(QObject* value, UnboundedBuffer* reply)
{
    const PSTRING& str = GetDecodedString(value);
    FormatBulk(str->c_str(), str->size(), reply);
}

QError  get(const vector<QString>& params, UnboundedBuffer* reply)
{
    assert (params[0] == "get");

    QObject* value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_string);
    if (err != QError_ok) 
    {
        if (err == QError_notExist)
            FormatNull(reply); 
        else
            ReplyError(err, reply); 
        return err;  
    }

    AddReply(value, reply);
    
    return   QError_ok;
}

QError  mget(const vector<QString>& params, UnboundedBuffer* reply)
{
    PreFormatMultiBulk(params.size() - 1, reply);

    for (size_t i = 1; i < params.size(); ++ i)
    {
        QObject* value;
        QError   err = QSTORE.GetValueByType(params[i], value, QType_string);
        if (err != QError_ok) 
        {
            FormatNull(reply); 
        }
        else
        {
            AddReply(value, reply);
        }
    }
    
    return   QError_ok;
}

QError  getrange(const vector<QString>& params, UnboundedBuffer* reply)
{
    QObject*   value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_string);
    if (err != QError_ok)
    {
        if (err == QError_notExist)
            FormatSingle("", 0, reply);
        else
            ReplyError(err, reply); 
        return err;  
    }

    long start = 0, end = 0;
    if (!Strtol(params[2].c_str(), params[2].size(), &start) ||
        !Strtol(params[3].c_str(), params[3].size(), &end))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }

    const PSTRING& str = GetDecodedString(value);
    AdjustIndex(start, end, str->size());

    if (start < end)
    {
        const QString& substr = str->substr(start, end);
        FormatSingle(substr.c_str(), substr.size(), reply);
    }
    else
    {
        FormatSingle("", 0, reply);
    }

    return   QError_ok;
}

QError  getset(const vector<QString>& params, UnboundedBuffer* reply)
{
    QObject*  value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_string);

    switch (err)
    {
    case QError_notExist:
        value = QSTORE.SetValue(params[1], CreateStringObject(""));
        // fall through

    case QError_ok:
        {
        const PSTRING& str = value->CastString();
        if (str->empty())
            FormatNull(reply);
        else    
            FormatSingle(str->c_str(), str->size(), reply); 

        *str = params[2];
        }
        break;

    default:
        ReplyError(err, reply); 
        return err;  
    }

    return   QError_ok;
}

QError  append(const vector<QString>& params, UnboundedBuffer* reply)
{
    QObject* value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_string);

    switch (err)
    {
    case QError_ok:
        *(value->CastString()) += params[2];
        break;

    case QError_notExist:
        value = QSTORE.SetValue(params[1], CreateStringObject(params[2]));
        break;

    default:
        ReplyError(err, reply);
        return err;
    };

    FormatInt(static_cast<long>(value->CastString()->size()), reply);
    return   QError_ok;
}

QError  bitcount(const vector<QString>& params, UnboundedBuffer* reply)
{
    QObject* value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_string);
    if (err != QError_ok)
    {
        if (err == QError_type)
        {
            ReplyError(QError_type, reply);
        }
        else
        {
            Format0(reply);
        }
        return  QError_ok;
    }

    if (params.size() != 2 && params.size() != 4)
    {
        LOG_ERR(g_log) << "bitcount wrong params size = " << params.size();
        ReplyError(QError_param, reply);
        return QError_param;
    }

    long start = 0;
    long end   = -1;
    if (params.size() == 4)
    {
        if (!Strtol(params[2].c_str(), params[2].size(), &start) ||
            !Strtol(params[3].c_str(), params[3].size(), &end))
        {
            ReplyError(QError_nan, reply);
            return QError_nan;
        }
    }

    const PSTRING& str = GetDecodedString(value);
    AdjustIndex(start, end, str->size());

    size_t cnt = 0;
    if (end >= start)
    {
        LOG_DBG(g_log) << "start = " << start << ", end = " << end << ", size = " << str->size();
        cnt = BitCount((const uint8_t*)str->data() + start,  end - start + 1);
    }

    FormatInt(static_cast<long>(cnt), reply);
    return   QError_ok;
}

QError  getbit(const vector<QString>& params, UnboundedBuffer* reply)
{
    QObject* value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_string);
    if (err != QError_ok)
    {
        Format0(reply);
        return  QError_ok;
    }

    long offset = 0;
    if (!Strtol(params[2].c_str(), params[2].size(), &offset))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }
    
    const PSTRING& str = GetDecodedString(value);
    const uint8_t*  buf = (const uint8_t*)str->c_str();
    size_t  size = 8 * str->size();

    if (offset < 0 || offset >= static_cast<long>(size))
    {
        Format0(reply);
        return QError_ok;
    }

    size_t  bytesOffset = offset / 8;
    size_t  bitsOffset  = offset % 8;
    uint8_t byte = buf[bytesOffset];
    LOG_DBG(g_log) << "bytes offset " << bytesOffset << ", bitsOff " << bitsOffset << ", byte = " << byte;
    if (byte & (0x1 << bitsOffset))
        Format1(reply);
    else
        Format0(reply);

    return QError_ok;
}

QError  setbit(const vector<QString>& params, UnboundedBuffer* reply)
{
    QObject* value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_string);
    if (err == QError_notExist)
    {
        value = QSTORE.SetValue(params[1], CreateStringObject("0"));
        err = QError_ok;
    }

    if (err != QError_ok)
    {
        Format0(reply);
        return  err;
    }

    long offset = 0;
    long on     = 0;
    if (!Strtol(params[2].c_str(), params[2].size(), &offset) || 
        !Strtol(params[3].c_str(), params[3].size(), &on))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }

    if (offset < 0 || offset > 8 * 1024 * 1024)
    {
        Format0(reply);
        return QError_ok;
    }
    
    const PSTRING& str = GetDecodedString(value);
    QString  newVal(*str);

    size_t  bytes = offset / 8;
    size_t  bits  = offset % 8;

    if (bytes + 1 > newVal.size())     newVal.resize(bytes + 1, '0');

    const char oldByte = newVal[bytes];
    char& byte = newVal[bytes];
    if (on)
        byte |= (0x1 << bits);
    else
        byte &= ~(0x1 << bits);

    value->encoding = QEncode_raw;
    value->value = std::make_shared<QString>(newVal);

    FormatInt((oldByte & (0x1 << bits)) ? 1 : 0, reply);

    return QError_ok;
}

static QError  ChangeIntValue(const QString& key, long delta, UnboundedBuffer* reply)
{
    QObject* value;
    QError err = QSTORE.GetValueByType(key, value, QType_string);
    if (err == QError_notExist)
    {
        value = QSTORE.SetValue(key, CreateStringObject("0"));
        err = QError_ok;
    }

    if (err != QError_ok)
    {
        ReplyError(err, reply);
        return  err;
    }

    if (value->encoding != QEncode_int)
    {
        ReplyError(QError_param, reply);
        return QError_ok;
    }

    intptr_t oldVal = (intptr_t)value->value.get();
    value->value.reset((void*)(oldVal + delta), [](void* ) {} );

    FormatInt(oldVal + delta, reply);

    return QError_ok;
}
//
QError  incr(const vector<QString>& params, UnboundedBuffer* reply)
{
    return ChangeIntValue(params[1], 1, reply);
}
QError  decr(const vector<QString>& params, UnboundedBuffer* reply)
{
    return ChangeIntValue(params[1], -1, reply);
}

QError  incrby(const vector<QString>& params, UnboundedBuffer* reply)
{
    long delta = 0;
    if (!Strtol(params[2].c_str(), params[2].size(), &delta))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }

    return ChangeIntValue(params[1], delta, reply);
}

QError  decrby(const vector<QString>& params, UnboundedBuffer* reply)
{
    long delta = 0;
    if (!Strtol(params[2].c_str(), params[2].size(), &delta))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }

    return ChangeIntValue(params[1], -delta, reply);
}

QError  strlen(const vector<QString>& params, UnboundedBuffer* reply)
{
    QObject* val;
    QError   err = QSTORE.GetValueByType(params[1], val, QType_string);
    if (err != QError_ok)
    {
        Format0(reply);
        return  err;
    }

    const PSTRING& str = val->CastString();
    FormatInt(static_cast<long>(str->size()), reply);
    
    return   QError_ok;
}

