#ifndef BERT_QCOMMAND_H
#define BERT_QCOMMAND_H

#include <vector>
#include <map>
#include "QCommon.h"
#include "QString.h"

enum QCommandAttr
{
    QAttr_read  = 0x1,
    QAttr_write = 0x1 << 1,
};


class   UnboundedBuffer;
typedef QError  QCommandHandler(const std::vector<QString>& params, UnboundedBuffer* reply);

// key commands
QCommandHandler  type;
QCommandHandler  exists;
QCommandHandler  del;
QCommandHandler  expire;
QCommandHandler  pexpire;
QCommandHandler  expireat;
QCommandHandler  pexpireat;
QCommandHandler  ttl;
QCommandHandler  pttl;
QCommandHandler  persist;
QCommandHandler  move;
QCommandHandler  keys;
QCommandHandler  randomkey;

// server commands
QCommandHandler  select;
QCommandHandler  dbsize;
QCommandHandler  bgsave;
QCommandHandler  save;
QCommandHandler  lastsave;
QCommandHandler  flushdb;
QCommandHandler  flushall;
QCommandHandler  client;
QCommandHandler  debug;
QCommandHandler  shutdown;
QCommandHandler  bgrewriteaof;

// string commands
QCommandHandler  set;
QCommandHandler  get;
QCommandHandler  getrange;
QCommandHandler  setrange;
QCommandHandler  getset;
QCommandHandler  append;
QCommandHandler  bitcount;
QCommandHandler  getbit;
QCommandHandler  setbit;
QCommandHandler  incr;
QCommandHandler  incrby;
QCommandHandler  decr;
QCommandHandler  decrby;
QCommandHandler  mget;
QCommandHandler  mset;
QCommandHandler  msetnx;
QCommandHandler  setnx;
QCommandHandler  setex;
QCommandHandler  strlen;

// list commands
QCommandHandler  lpush;
QCommandHandler  rpush;
QCommandHandler  lpushx;
QCommandHandler  rpushx;
QCommandHandler  lpop;
QCommandHandler  rpop;
QCommandHandler  lindex;
QCommandHandler  llen;
QCommandHandler  lset;
QCommandHandler  ltrim;
QCommandHandler  lrange;
QCommandHandler  linsert;
QCommandHandler  lrem;
QCommandHandler  rpoplpush;
QCommandHandler  blpop;
QCommandHandler  brpop;
QCommandHandler  brpoplpush;

// hash commands
QCommandHandler  hget;
QCommandHandler  hmget;
QCommandHandler  hgetall;
QCommandHandler  hset;
QCommandHandler  hsetnx;
QCommandHandler  hmset;
QCommandHandler  hlen;
QCommandHandler  hexists;
QCommandHandler  hkeys;
QCommandHandler  hvals;
QCommandHandler  hdel;
QCommandHandler  hincrby; // if hash is not exist, create!
QCommandHandler  hincrbyfloat;

// set commands
QCommandHandler  sadd;
QCommandHandler  scard;
QCommandHandler  srem;
QCommandHandler  sismember;
QCommandHandler  smembers;
QCommandHandler  sdiff;
QCommandHandler  sdiffstore;
QCommandHandler  sinter;
QCommandHandler  sinterstore;
QCommandHandler  sunion;
QCommandHandler  sunionstore;
QCommandHandler  smove;
QCommandHandler  spop;
QCommandHandler  srandmember;


// sset
QCommandHandler  zadd;
QCommandHandler  zcard;
QCommandHandler  zrank;
QCommandHandler  zrevrank;
QCommandHandler  zrem;
QCommandHandler  zincrby;
QCommandHandler  zscore;
QCommandHandler  zrange;
QCommandHandler  zrevrange;
QCommandHandler  zrangebyscore;
QCommandHandler  zrevrangebyscore;
QCommandHandler  zremrangebyrank;
QCommandHandler  zremrangebyscore;

// pubsub
QCommandHandler  subscribe;
QCommandHandler  unsubscribe;
QCommandHandler  publish;
QCommandHandler  psubscribe;
QCommandHandler  punsubscribe;
QCommandHandler  pubsub;

//multi
QCommandHandler  watch;
QCommandHandler  unwatch;
QCommandHandler  multi;
QCommandHandler  exec;
QCommandHandler  discard;

struct QCommandInfo
{
    QString     cmd;
    int         attr;
    int         params;
    QCommandHandler* handler;
    bool  CheckParamsCount(int nParams) const;
};

class QCommandTable
{
public:
    QCommandTable();
    
    static void Init();

    static const QCommandInfo* GetCommandInfo(const QString& cmd);
    static QError ExecuteCmd(const std::vector<QString>& params, const QCommandInfo* info, UnboundedBuffer* reply = nullptr);
    static QError ExecuteCmd(const std::vector<QString>& params, UnboundedBuffer* reply = nullptr);

private:
    static const QCommandInfo s_info[];
    static std::map<QString, const QCommandInfo* >  s_handlers;
};


#endif

