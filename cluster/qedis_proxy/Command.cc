#include "Command.h"

const CommandInfo CommandTable::s_info[] =
{
    // key
    {"type",        Attr_read,                2,  },
    {"exists",      Attr_read,                2,  },
    {"del",         Attr_write,              -2,  },
    {"expire",      Attr_read,                3,  },
    {"ttl",         Attr_read,                2,  },
    {"pexpire",     Attr_read,                3,  },
    {"pttl",        Attr_read,                2,  },
    {"expireat",    Attr_read,                3,  },
    {"pexpireat",   Attr_read,                3,  },
    {"persist",     Attr_read,                2, },

    // local
    {"ping",        Attr_read,                1,  &ping},
    {"info",        Attr_read,               -1,  &info},
    
    // string
    {"strlen",      Attr_read,                2,  },
    {"set",         Attr_write,               3,  },
    //{"mset",        Attr_write,              -3,  },
    //{"msetnx",      Attr_write,              -3,  },
    {"setnx",       Attr_write,               3,  },
    {"setex",       Attr_write,               4,  },
    {"psetex",      Attr_write,               4,  },
    {"get",         Attr_read,                2,  },
    {"getset",      Attr_write,               3,  },
    //{"mget",        Attr_read,               -2,  &mget},
    {"append",      Attr_write,               3,  },
    {"bitcount",    Attr_read,               -2,  },
    {"bitop",       Attr_write,              -4,  },
    {"getbit",      Attr_read,                3,  },
    {"setbit",      Attr_write,               4,  },
    {"incr",        Attr_write,               2,  },
    {"decr",        Attr_write,               2,  },
    {"incrby",      Attr_write,               3,  },
    {"incrbyfloat", Attr_write,               3,  },
    {"decrby",      Attr_write,               3,  },
    {"getrange",    Attr_read,                4,  },
    {"setrange",    Attr_write,               4,  },

    // list
    {"lpush",       Attr_write,              -3,  },
    {"rpush",       Attr_write,              -3,  },
    {"lpushx",      Attr_write,              -3,  },
    {"rpushx",      Attr_write,              -3,  },
    {"lpop",        Attr_write,               2,  },
    {"rpop",        Attr_write,               2,  },
    {"lindex",      Attr_read,                3,  },
    {"llen",        Attr_read,                2,  },
    {"lset",        Attr_write,               4,  },
    {"ltrim",       Attr_write,               4,  },
    {"lrange",      Attr_read,                4,  },
    {"linsert",     Attr_write,               5,  },
    {"lrem",        Attr_write,               4,  },

    // hash
    {"hget",        Attr_read,                3,  },
    {"hgetall",     Attr_read,                2,  },
    {"hmget",       Attr_read,               -3,  },
    {"hset",        Attr_write,               4,  },
    {"hsetnx",      Attr_write,               4,  },
    {"hmset",       Attr_write,              -4,  },
    {"hlen",        Attr_read,                2,  },
    {"hexists",     Attr_read,                3,  },
    {"hkeys",       Attr_read,                2,  },
    {"hvals",       Attr_read,                2,  },
    {"hdel",        Attr_write,              -3,  },
    {"hincrby",     Attr_write,               4,  },
    {"hincrbyfloat",Attr_write,               4,  },
    {"hscan",       Attr_read,               -3,  },
    {"hstrlen",     Attr_read,                3,  },

    // set
    {"sadd",        Attr_write,              -3,  },
    {"scard",       Attr_read,                2,  },
    {"sismember",   Attr_read,                3,  },
    {"srem",        Attr_write,              -3,  },
    {"smembers",    Attr_read,                2,  },
    //{"sdiff",       Attr_read,               -2,  },
    //{"sdiffstore",  Attr_write,              -3,  &sdiffstore},
    //{"sinter",      Attr_read,               -2,  &sinter},
    //{"sinterstore", Attr_write,              -3,  &sinterstore},
    //{"sunion",      Attr_read,               -2,  &sunion},
    //{"sunionstore", Attr_write,              -3,  &sunionstore},
    //{"smove",       Attr_write,               4,  &smove},
    {"spop",        Attr_write,               2,  },
    {"srandmember", Attr_read,                2,  },
    {"sscan",       Attr_read,               -3,  },

    //
    {"zadd",        Attr_write,              -4,  },
    {"zcard",       Attr_read,                2,  },
    {"zrank",       Attr_read,                3,  },
    {"zrevrank",    Attr_read,                3,  },
    {"zrem",        Attr_write,              -3,  },
    {"zincrby",     Attr_write,               4,  },
    {"zscore",      Attr_read,                3,  },
    {"zrange",      Attr_read,               -4,  },
    {"zrevrange",   Attr_read,               -4,  },
    {"zrangebyscore",   Attr_read,           -4,  },
    {"zrevrangebyscore",Attr_read,           -4,  },
    {"zremrangebyrank", Attr_write,           4,  },
    {"zremrangebyscore",Attr_write,           4,  },
};
    

std::map<std::string, const CommandInfo*>  CommandTable::s_handlers;

void CommandTable::Init()
{
    for (const auto& info : s_info)
    {
        s_handlers[info.cmd] = &info;
    }
}

const CommandInfo* CommandTable::GetCommandInfo(const std::string& cmd)
{
    auto it(s_handlers.find(cmd));
    if (it != s_handlers.end())
    {
        return it->second;
    }
    
    return 0;
}

struct QedisErrorInfo g_errorInfo[] = {
    {sizeof "+OK\r\n"- 1, "+OK\r\n"},
    {sizeof "-ERR wrong number of arguments\r\n"- 1, "-ERR wrong number of arguments\r\n"},
    {sizeof "-ERR Unknown command\r\n"- 1,   "-ERR Unknown command\r\n"},
    {sizeof "-ERR syntax error\r\n"-1, "-ERR syntax error\r\n"},
    {sizeof "-ERR Server not ready\r\n"-1, "-ERR Server not ready\r\n"},
    {sizeof "-ERR Server response timeout\r\n"-1, "-ERR Server response timeout\r\n"},
    {sizeof "-ERR Server not alive\r\n"-1, "-ERR Server not alive\r\n"},
};

std::string ping(const std::vector<std::string>& params)
{
    return std::string("+PONG\r\n");
}

std::string info(const std::vector<std::string>& params)
{
    return std::string("$9\r\nTODO_INFO\r\n");
}

