#include "QCommand.h"

const QCommandTable::QCommandInfo QCommandTable::s_info[] =
{
    // key
    {"type",        QCommandAttr_read,                2,  &type},
    {"exists",      QCommandAttr_read,                2,  &exists},
    {"del",         QCommandAttr_write,              -2,  &del},
    {"expire",      QCommandAttr_write,               3,  &expire},
    {"ttl",         QCommandAttr_read,                2,  &ttl},
    {"pexpire",     QCommandAttr_write,               3,  &pexpire},
    {"pttl",        QCommandAttr_read,                2,  &pttl},
    {"expireat",    QCommandAttr_write,               3,  &expireat},
    {"pexpireat",   QCommandAttr_write,               3,  &pexpireat},
    {"persist",     QCommandAttr_read,                2,  &persist},
    {"move",        QCommandAttr_write,               3,  &move},

    // server
    {"select",      QCommandAttr_read,                2,  &select},
    
    // string
    {"set",         QCommandAttr_write,               3,  &set},
    {"get",         QCommandAttr_read,                2,  &get},
    {"getset",      QCommandAttr_write,               3,  &getset},
    {"append",      QCommandAttr_write,               3,  &append},

    // list
    {"lpush",       QCommandAttr_write,              -3,  &lpush},
    {"rpush",       QCommandAttr_write,              -3,  &rpush},
    {"lpushx",      QCommandAttr_write,              -3,  &lpushx},
    {"rpushx",      QCommandAttr_write,              -3,  &rpushx},
    {"lpop",        QCommandAttr_write,               2,  &lpop},
    {"rpop",        QCommandAttr_write,               2,  &rpop},
    {"lindex",      QCommandAttr_read,                3,  &lindex},
    {"llen",        QCommandAttr_read,                2,  &llen},
    {"lset",        QCommandAttr_write,               4,  &lset},
    {"ltrim",       QCommandAttr_write,               4,  &ltrim},
    
    
    // hash
    {"hget",        QCommandAttr_read,                3,  &hget},
    {"hgetall",     QCommandAttr_read,                2,  &hgetall},
    {"hmget",       QCommandAttr_read,               -3,  &hmget},
    {"hset",        QCommandAttr_write,               4,  &hset},
    {"hsetnx",      QCommandAttr_write,               4,  &hsetnx},
    {"hmset",       QCommandAttr_write,              -4,  &hmset},
    {"hlen",        QCommandAttr_read,                2,  &hlen},
    {"hexists",     QCommandAttr_read,                3,  &hexists},
    {"hkeys",       QCommandAttr_read,                2,  &hkeys},
    {"hvals",       QCommandAttr_read,                2,  &hvals},
    {"hdel",        QCommandAttr_write,              -3,  &hdel},
    {"hincrby",     QCommandAttr_write,               4,  &hincrby},
    {"hincrbyfloat",QCommandAttr_write,               4,  &hincrbyfloat},

    // set
    {"sadd",        QCommandAttr_write,              -3,  &sadd},
    {"scard",       QCommandAttr_read,                2,  &scard},
    {"sismember",   QCommandAttr_read,                3,  &sismember},
    {"srem",        QCommandAttr_write,              -3,  &srem},
    {"smembers",    QCommandAttr_read,                2,  &smembers},
    {"sdiff",       QCommandAttr_read,               -2,  &sdiff},
    {"sdiffstore",  QCommandAttr_write,              -3,  &sdiffstore},
    {"sinter",      QCommandAttr_read,               -2,  &sinter},
    {"sinterstore", QCommandAttr_write,              -3,  &sinterstore},
    {"sunion",      QCommandAttr_read,               -2,  &sunion},
    {"sunionstore", QCommandAttr_write,              -3,  &sunionstore},
    {"smove",       QCommandAttr_write,               4,  &smove},
    {"spop",        QCommandAttr_write,               2,  &spop},
    {"srandmember", QCommandAttr_write,               2,  &srandmember},
};

QCommandTable& QCommandTable::Instance()
{
    static QCommandTable  tbl;
    return tbl;
}

QCommandTable::QCommandTable()
{
    for (int i = 0; i < sizeof s_info / sizeof s_info[0]; ++ i)
    {
        const QCommandInfo& info = s_info[i];
        m_handlers[info.cmd] = &info;
    }
}

QError QCommandTable::ExecuteCmd(const std::vector<QString>& params, UnboundedBuffer& reply)
{
    if (params.empty())
    {
        ReplyErrorInfo(QError_paramNotMatch, reply);
        return   QError_paramNotMatch;
    }

    std::map<QString, const QCommandInfo* >::const_iterator it(m_handlers.find(params[0]));
    if (it == m_handlers.end())
    {
        const QErrorInfo& err = g_errorInfo[QError_unknowCmd];
        FormatError(err.errorStr, err.len, reply);
        return QError_unknowCmd;
    }

    const QCommandInfo* info = it->second;
    if (!info->CheckParamsCount(static_cast<int>(params.size())))
    {
        ReplyErrorInfo(QError_paramNotMatch, reply);
        return   QError_paramNotMatch;
    }

    return info->handler(params, reply);
}

    
bool QCommandTable::QCommandInfo::CheckParamsCount(int nParams) const
{
    if (params > 0)
        return params == nParams;
    else
        return nParams + params >= 0;
}

