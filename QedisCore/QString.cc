#include "QString.h"
#include "QStore.h"
#include "Log/Logger.h"
#include <cassert>

namespace qedis
{

QObject QObject::CreateString(const QString& value)
{
    QObject obj(QType_string);

    long val;
    if (Strtol(value.c_str(), value.size(), &val))
    {
        obj.encoding = QEncode_int;
        obj.value = (void*)val;
        DBG << "set long value " << val;
    }
    else
    {
        obj.encoding = QEncode_raw;
        obj.value = new QString(value);
    }

    return obj;
}

QObject QObject::CreateString(long val)
{
    QObject obj(QType_string);
    
    obj.encoding = QEncode_int;
    obj.value = (void*)val;
    
    return obj;
}

    
static void DeleteString(QString* s)
{
    delete s;
}

static void NotDeleteString(QString* )
{
}

std::unique_ptr<QString, void (*)(QString* )>
    GetDecodedString(const QObject* value)
{
    if (value->encoding == QEncode_raw)
    {
        return std::unique_ptr<QString, void (*)(QString* )>(value->CastString(), NotDeleteString);
    }
    else if (value->encoding == QEncode_int)
    {
        intptr_t val = (intptr_t)value->value;
        
        char vbuf[32];
        snprintf(vbuf, sizeof vbuf - 1, "%ld",  val);
        return std::unique_ptr<QString, void (*)(QString* )>(new QString(vbuf), DeleteString);
    }
    else
    {
        assert (!!!"error string encoding");
    }
        
    return std::unique_ptr<QString, void (*)(QString* )>(nullptr, NotDeleteString);
}

static bool SetValue(const QString& key, const QString& value, bool exclusive = false)
{
    if (exclusive)
    {
        QObject* val;
        if (QSTORE.GetValue(key, val) == QError_ok)
            return false;
    }

    QSTORE.ClearExpire(key); // clear key's old ttl
    QSTORE.SetValue(key, QObject::CreateString(value));

    return true;
}

QError set(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    SetValue(params[1], params[2]);
    FormatOK(reply);
    return QError_ok;
}

QError setnx(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    if (SetValue(params[1], params[2], true))
        Format1(reply);
    else
        Format0(reply);

    return QError_ok;
}

QError mset(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    if (params.size() % 2 != 1)
    {
        ReplyError(QError_param, reply);
        return QError_param;
    }

    for (size_t i = 1; i < params.size(); i += 2)
    {
        g_dirtyKeys.push_back(params[i]);
        SetValue(params[i], params[i + 1]);
    }
    
    FormatOK(reply);
    return QError_ok;
}

QError msetnx(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    if (params.size() % 2 != 1)
    {
        ReplyError(QError_param, reply);
        return QError_param;
    }

    for (size_t i = 1; i < params.size(); i += 2)
    {
        QObject* val;
        if (QSTORE.GetValue(params[i], val) == QError_ok)
        {
            Format0(reply);
            return QError_ok;
        }
    }

    for (size_t i = 1; i < params.size(); i += 2)
    {
        g_dirtyKeys.push_back(params[i]);
        SetValue(params[i], params[i + 1]);
    }

    Format1(reply);
    return QError_ok;
}

QError setex(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    long seconds;
    if (!Strtol(params[2].c_str(), params[2].size(), &seconds))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }
    
    const auto& key = params[1];
    QSTORE.SetValue(key, QObject::CreateString(params[3]));
    QSTORE.SetExpire(key, ::Now() + seconds * 1000);

    FormatOK(reply);
    return QError_ok;
}

QError psetex(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    long milliseconds;
    if (!Strtol(params[2].c_str(), params[2].size(), &milliseconds))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }
    
    const auto& key = params[1];
    QSTORE.SetValue(key, QObject::CreateString(params[3]));
    QSTORE.SetExpire(key, ::Now() + milliseconds);
    
    FormatOK(reply);
    return QError_ok;
}

QError setrange(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    long offset;
    if (!Strtol(params[2].c_str(), params[2].size(), &offset))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }

    QObject* value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_string);
    if (err != QError_ok)
    {
        if (err == QError_notExist)
        {
            value = QSTORE.SetValue(params[1], QObject::CreateString(""));
        }
        else
        {
            ReplyError(err, reply); 
            return err;  
        }
    }

    auto str = GetDecodedString(value);
    const size_t newSize = offset + params[3].size();

    if (newSize > str->size())  str->resize(newSize, '\0');
    str->replace(offset, params[3].size(), params[3]);

    FormatInt(static_cast<long>(str->size()), reply);
    return QError_ok;
}


static void AddReply(QObject* value, UnboundedBuffer* reply)
{
    auto str = GetDecodedString(value);
    FormatBulk(str->c_str(), str->size(), reply);
}

QError get(const std::vector<QString>& params, UnboundedBuffer* reply)
{
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
    return QError_ok;
}

QError mget(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    PreFormatMultiBulk(params.size() - 1, reply);
    for (size_t i = 1; i < params.size(); ++ i)
    {
        QObject* value;
        QError err = QSTORE.GetValueByType(params[i], value, QType_string);
        if (err != QError_ok)
            FormatNull(reply);
        else
            AddReply(value, reply);
    }
    
    return QError_ok;
}

QError getrange(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QObject* value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_string);
    if (err != QError_ok)
    {
        if (err == QError_notExist)
            FormatBulk("", 0, reply);
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

    auto str = GetDecodedString(value);
    AdjustIndex(start, end, str->size());

    if (start <= end)
        FormatBulk(&(*str)[start], end - start + 1, reply);
    else
        FormatEmptyBulk(reply);

    return QError_ok;
}

QError  getset(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QObject* value = nullptr;
    QError err = QSTORE.GetValueByType(params[1], value, QType_string);

    switch (err)
    {
    case QError_notExist:
        // fall through

    case QError_ok:
        if (!value)
            FormatNull(reply);
        else    
            FormatBulk(*GetDecodedString(value), reply);

        QSTORE.SetValue(params[1], QObject::CreateString(params[2]));
        break;

    default:
        ReplyError(err, reply); 
        return err;  
    }

    return QError_ok;
}

QError  append(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QObject* value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_string);

    switch (err)
    {
    case QError_ok:
        {
            auto s = GetDecodedString(value);
            value = QSTORE.SetValue(params[1], QObject::CreateString(*s + params[2]));
        }
        break;

    case QError_notExist:
        value = QSTORE.SetValue(params[1], QObject::CreateString(params[2]));
        break;

    default:
        ReplyError(err, reply);
        return err;
    };

    auto s = GetDecodedString(value);
    FormatInt(static_cast<long>(s->size()), reply);
    return QError_ok;
}

QError bitcount(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QObject* value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_string);
    if (err != QError_ok)
    {
        if (err == QError_type)
            ReplyError(QError_type, reply);
        else
            Format0(reply);

        return QError_ok;
    }

    if (params.size() != 2 && params.size() != 4)
    {
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

    auto str = GetDecodedString(value);
    AdjustIndex(start, end, str->size());

    size_t cnt = 0;
    if (end >= start)
    {
        cnt = BitCount((const uint8_t*)str->data() + start,  end - start + 1);
    }

    FormatInt(static_cast<long>(cnt), reply);
    return QError_ok;
}

QError getbit(const std::vector<QString>& params, UnboundedBuffer* reply)
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
    
    auto str = GetDecodedString(value);
    const uint8_t* buf = (const uint8_t*)str->c_str();
    size_t  size = 8 * str->size();

    if (offset < 0 || offset >= static_cast<long>(size))
    {
        Format0(reply);
        return QError_ok;
    }

    size_t  bytesOffset = offset / 8;
    size_t  bitsOffset  = offset % 8;
    uint8_t byte = buf[bytesOffset];
    if (byte & (0x1 << bitsOffset))
        Format1(reply);
    else
        Format0(reply);

    return QError_ok;
}

QError setbit(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QObject* value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_string);
    if (err == QError_notExist)
    {
        value = QSTORE.SetValue(params[1], QObject::CreateString("0"));
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

    if (offset < 0 || offset > kStringMaxBytes)
    {
        Format0(reply);
        return QError_ok;
    }
    
    auto str = GetDecodedString(value);
    QString newVal(*str);

    size_t  bytes = offset / 8;
    size_t  bits  = offset % 8;

    if (bytes + 1 > newVal.size())     newVal.resize(bytes + 1, '0');

    const char oldByte = newVal[bytes];
    char& byte = newVal[bytes];
    if (on)
        byte |= (0x1 << bits);
    else
        byte &= ~(0x1 << bits);

    value->Reset(new QString(newVal));
    FormatInt((oldByte & (0x1 << bits)) ? 1 : 0, reply);

    return QError_ok;
}

static QError  ChangeFloatValue(const QString& key, float delta, UnboundedBuffer* reply)
{
    QObject* value;
    QError err = QSTORE.GetValueByType(key, value, QType_string);
    if (err == QError_notExist)
    {
        value = QSTORE.SetValue(key, QObject::CreateString(0));
        err = QError_ok;
    }

    if (err != QError_ok)
    {
        ReplyError(err, reply);
        return  err;
    }

    auto val = GetDecodedString(value);
    float oldVal = 0;
    if (!Strtof(val->c_str(), val->size(), &oldVal))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }

    char newVal[32];
    int len = snprintf(newVal, sizeof newVal - 1, "%.6g", (oldVal + delta));
    value->Reset(new QString(newVal, len));

    FormatBulk(newVal, len, reply);
    return QError_ok;
}

QError incrbyfloat(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    float delta = 0;
    if (!Strtof(params[2].c_str(), params[2].size(), &delta))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }

    return ChangeFloatValue(params[1], delta, reply);
}

static QError ChangeIntValue(const QString& key, long delta, UnboundedBuffer* reply)
{
    QObject* value;
    QError err = QSTORE.GetValueByType(key, value, QType_string);
    if (err == QError_notExist)
    {
        value = QSTORE.SetValue(key, QObject::CreateString(0));
        err = QError_ok;
    }

    if (err != QError_ok)
    {
        ReplyError(err, reply);
        return  err;
    }

    if (value->encoding != QEncode_int)
    {
        ReplyError(QError_nan, reply);
        return QError_ok;
    }

    intptr_t oldVal = (intptr_t)value->value;
    value->Reset((void*)(oldVal + delta));

    FormatInt(oldVal + delta, reply);
    return QError_ok;
}

QError incr(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    return ChangeIntValue(params[1], 1, reply);
}
QError decr(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    return ChangeIntValue(params[1], -1, reply);
}

QError incrby(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    long delta = 0;
    if (!Strtol(params[2].c_str(), params[2].size(), &delta))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }

    return ChangeIntValue(params[1], delta, reply);
}

QError decrby(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    long delta = 0;
    if (!Strtol(params[2].c_str(), params[2].size(), &delta))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }

    return ChangeIntValue(params[1], -delta, reply);
}

QError strlen(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QObject* val;
    QError   err = QSTORE.GetValueByType(params[1], val, QType_string);
    if (err != QError_ok)
    {
        Format0(reply);
        return  err;
    }
    
    auto str = GetDecodedString(val);
    FormatInt(static_cast<long>(str->size()), reply);
    return QError_ok;
}

enum BitOp
{
    BitOp_and,
    BitOp_or,
    BitOp_not,
    BitOp_xor,
};

static QString StringBitOp(const std::vector<const QString* >& keys, BitOp op)
{
    QString res;
    
    switch (op)
    {
        case BitOp_and:
        case BitOp_or:
        case BitOp_xor:
            for (auto k : keys)
            {
                QObject* val;
                if (QSTORE.GetValueByType(*k, val, QType_string) != QError_ok)
                    continue;
                
                auto str = GetDecodedString(val);
                if (res.empty())
                {
                    res = *str;
                    continue;
                }
                
                if (str->size() > res.size())
                    res.resize(str->size());

                for (size_t i = 0; i < str->size(); ++ i)
                {
                    if (op == BitOp_and)
                        res[i] &= (*str)[i];
                    else if (op == BitOp_or)
                        res[i] |= (*str)[i];
                    else if (op == BitOp_xor)
                        res[i] ^= (*str)[i];
                }
            }
            break;
            
        case BitOp_not:
        {
            assert(keys.size() == 1);
            QObject* val;
            if (QSTORE.GetValueByType(*keys[0], val, QType_string) != QError_ok)
                break;
            
            auto str = GetDecodedString(val);
            res.resize(str->size());

            for (size_t i = 0; i < str->size(); ++ i)
            {
                res[i] = ~(*str)[i];
            }
    
            break;
        }

        default:
            break;
    }
    
    return res;
}


QError  bitop(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    std::vector<const QString* > keys;
    for (size_t i = 3; i < params.size(); ++ i)
        keys.push_back(&params[i]);
    
    QError err = QError_param;
    QString res;
    if (params[1].size() == 2)
    {
        if (strncasecmp(params[1].c_str(), "or", 2) == 0)
        {
            err = QError_ok;
            res = StringBitOp(keys, BitOp_or);
        }
    }
    else if (params[1].size() == 3)
    {
        if (strncasecmp(params[1].c_str(), "xor", 3) == 0)
        {
            err = QError_ok;
            res = StringBitOp(keys, BitOp_xor);
        }
        else if (strncasecmp(params[1].c_str(), "and", 3) == 0)
        {
            err = QError_ok;
            res = StringBitOp(keys, BitOp_and);
        }
        else if (strncasecmp(params[1].c_str(), "not", 3) == 0)
        {
            if (params.size() == 4)
            {
                err = QError_ok;
                res = StringBitOp(keys, BitOp_not);
            }
        }
        else
        {
            ;
        }
    }
    
    if (err != QError_ok)
    {
        ReplyError(err, reply);
    }
    else
    {
        QSTORE.SetValue(params[2], QObject::CreateString(res));
        FormatInt(static_cast<long>(res.size()), reply);
    }

    return QError_ok;
}

}
