#include "QCommand.h"

using std::size_t;

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
    {"keys",        QCommandAttr_read,                2,  &keys},
    {"randomkey",   QCommandAttr_read,                1,  &randomkey},

    // server
    {"select",      QCommandAttr_read,                2,  &select},
    {"dbsize",      QCommandAttr_read,                1,  &dbsize},
    
    // string
    {"strlen",      QCommandAttr_read,                2,  &strlen},
    {"set",         QCommandAttr_write,               3,  &set},
    {"mset",        QCommandAttr_write,              -3,  &mset},
    {"msetnx",      QCommandAttr_write,              -3,  &msetnx},
    {"setnx",       QCommandAttr_write,               3,  &setnx},
    {"setex",       QCommandAttr_write,               4,  &setex},
    {"get",         QCommandAttr_read,                2,  &get},
    {"getset",      QCommandAttr_write,               3,  &getset},
    {"mget",        QCommandAttr_read,               -2,  &mget},
    {"append",      QCommandAttr_write,               3,  &append},
    {"bitcount",    QCommandAttr_read,               -2,  &bitcount},
    {"getbit",      QCommandAttr_read,                3,  &getbit},
    {"setbit",      QCommandAttr_write,               4,  &setbit},
    {"incr",        QCommandAttr_write,               2,  &incr},
    {"decr",        QCommandAttr_write,               2,  &decr},
    {"incrby",      QCommandAttr_write,               3,  &incrby},
    {"decrby",      QCommandAttr_write,               3,  &decrby},
    {"getrange",    QCommandAttr_read,                4,  &getrange},
    {"setrange",    QCommandAttr_write,               4,  &setrange},

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
    {"lrange",      QCommandAttr_write,               4,  &lrange},
    {"linsert",     QCommandAttr_write,               5,  &linsert},
    {"lrem",        QCommandAttr_write,               4,  &lrem},
    {"rpoplpush",   QCommandAttr_write,               3,  &rpoplpush},
    
    
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

    //
    {"zadd",        QCommandAttr_write,              -4,  &zadd},
    {"zcard",       QCommandAttr_read,                2,  &zcard},
    {"zrank",       QCommandAttr_read,                3,  &zrank},
    {"zrevrank",    QCommandAttr_read,                3,  &zrevrank},
    {"zrem",        QCommandAttr_write,              -3,  &zrem},
    {"zincrby",     QCommandAttr_write,               4,  &zincrby},
    {"zscore",      QCommandAttr_read,                3,  &zscore},
    {"zrange",      QCommandAttr_read,               -4,  &zrange},
    {"zrevrange",   QCommandAttr_read,               -4,  &zrevrange},
    {"zrangebyscore",   QCommandAttr_read,           -4,  &zrangebyscore},
    {"zrevrangebyscore",QCommandAttr_read,           -4,  &zrevrangebyscore},
    {"zremrangebyrank", QCommandAttr_write,           4,  &zremrangebyrank},
    {"zremrangebyscore",QCommandAttr_write,           4,  &zremrangebyscore},

    // pubsub
    {"subscribe",   QCommandAttr_read,               -2,  &subscribe},
    {"unsubscribe", QCommandAttr_read,               -1,  &unsubscribe},
    {"publish",     QCommandAttr_read,                3,  &publish},
    {"psubscribe",  QCommandAttr_read,               -2,  &psubscribe},
    {"punsubscribe",QCommandAttr_read,               -1,  &punsubscribe},
};

QCommandTable& QCommandTable::Instance()
{
    static QCommandTable  tbl;
    return tbl;
}

QCommandTable::QCommandTable()
{
    for (size_t i = 0; i < sizeof s_info / sizeof s_info[0]; ++ i)
    {
        const QCommandInfo& info = s_info[i];
        m_handlers[info.cmd] = &info;
    }
}

QError QCommandTable::ExecuteCmd(const std::vector<QString>& params, UnboundedBuffer& reply)
{
    if (params.empty())
    {
        ReplyError(QError_param, reply);
        return   QError_param;
    }

    std::map<QString, const QCommandInfo* >::const_iterator it(m_handlers.find(params[0]));
    if (it == m_handlers.end())
    {
        ReplyError(QError_unknowCmd, reply);
        return QError_unknowCmd;
    }

    const QCommandInfo* info = it->second;
    if (!info->CheckParamsCount(static_cast<int>(params.size())))
    {
        ReplyError(QError_param, reply);
        return   QError_param;
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

