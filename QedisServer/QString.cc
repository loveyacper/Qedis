#include "QString.h"
#include "QStore.h"
#include "Log/Logger.h"
#include "QClient.h" // test
#include <iostream>
#include <cassert>

using namespace std;

QObject  CreateString(const QString&  value)
{
    QObject   obj(QType_string);

    long  val;
    if (Strtol(value.c_str(), value.size(), &val))
    {
        obj.value.SetIntValue(val);
        LOG_DBG(g_logger) << "set int value " << val;
        obj.encoding = QEncode_int;
    }
    else
    {
        obj.value.Reset(new QString(value));
    }

    return obj;
}

QError  set(const vector<QString>& params, UnboundedBuffer& reply)
{
    const QObject& obj = CreateString(params[2]);

    QSTORE.ClearExpire(params[1]); // clear key's old ttl
    QError err = QSTORE.SetValue(params[1], obj);
    if (err != QError_ok)
    {
        ReplyErrorInfo(err, reply);
        return err;
    }

//    QClient::Current()->SendPacket("+OK\r\n", 5);
    FormatSingle("OK", 2, reply);
    return   QError_ok;
}

QError  get(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "get");

    QObject  value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_string);
    if (err != QError_ok) 
    {
        ReplyErrorInfo(err, reply); 
        return err;  
    }

    if (value.encoding == QEncode_raw)
    {
        const PSTRING& str = value.CastString();
        FormatSingle(str->c_str(), static_cast<int>(str->size()), reply);
    }
    else if (value.encoding == QEncode_int)
    {
        intptr_t val = value.value.GetIntValue();
        
        char ret[32];
        int  len = snprintf(ret, sizeof ret - 1, "%ld",  val);
        FormatSingle(ret, len, reply);
    }
    else
    {
        LOG_ERR(g_logger) << "wrong encoding " << value.encoding;
        assert (false);
    }
    
    return   QError_ok;
}

QError  getset(const vector<string>& params, UnboundedBuffer& reply)
{
    QObject   value(QType_string);
    QError err = QSTORE.GetValueByType(params[1], value, QType_string);
    if (err != QError_ok && err != QError_notExist)
    {  
        ReplyErrorInfo(err, reply); 
        return err;  
    } 

    if (err == QError_notExist)
    {
        value.value.Reset(new QString); 
        err = QSTORE.SetValue(params[1], value);  
        if (err != QError_ok) 
        {  
            ReplyErrorInfo(err, reply);  
            return err; 
        } 
    }

    const PSTRING& str = value.CastString();
    if (str->empty())
        FormatNull(reply);
    else    
        FormatSingle(str->c_str(), static_cast<int>(str->size()), reply); 

    *str = params[2];
    return   QError_ok;
}

QError  append(const vector<QString>& params, UnboundedBuffer& reply)
{
    QObject  value(QType_string);
    QError err = QSTORE.GetValueByType(params[1], value, QType_string);
    if (err != QError_ok)
    {
        if (err != QError_notExist)
        {
            ReplyErrorInfo(err, reply);
            return err;
        }
        else
        {
            value.value.Reset(new QString);
        }
    }

    const PSTRING& str = value.CastString();
    (*str) += params[2];

    err = QSTORE.SetValue(params[1], value);
    if (err != QError_ok)
    {
        ReplyErrorInfo(err, reply);
        return err;
    }

    FormatInt(static_cast<int>(str->size()), reply);

    return   QError_ok;
}

QError  bitcount(const vector<QString>& params, UnboundedBuffer& reply)
{
    QObject  value(QType_string);
    QError err = QSTORE.GetValueByType(params[1], value, QType_string);
    if (err != QError_ok)
    {
        FormatInt(0, reply);
        return  QError_ok;
    }

    if (params.size() != 2 || params.size() != 4)
    {
        ReplyErrorInfo(QError_paramNotMatch, reply);
        return QError_paramNotMatch;
    }

    long start = 0;
    long end   = -1;
    if (params.size() == 4)
    {
        if (!Strtol(params[2].c_str(), params[2].size(), &start) ||
            !Strtol(params[3].c_str(), params[3].size(), &end))
        {
            ReplyErrorInfo(QError_paramNotMatch, reply);
            return QError_paramNotMatch;
        }
    }


    const PSTRING& str = value.CastString();
    const int strLen = static_cast<int>(str->size());

    if (start < 0)
        start += strLen;
    if (end < 0)
        end += strLen;

    if (start < 0)
        start = 0;
    if (end < 0)
        end = 0;

    int cnt = 0;
    if (end >= start)
        cnt = BitCount((const uint8_t* )(str->c_str() + start),  end - start + 1);

    FormatInt(cnt, reply);

    return   QError_ok;
}

