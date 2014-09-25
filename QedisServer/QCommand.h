#ifndef BERT_QCOMMAND_H
#define BERT_QCOMMAND_H

#include <vector>
#include <map>
#include "QCommon.h"
#include "QString.h"

enum QCommandAttr
{
    QCommandAttr_read  = 0x1,
    QCommandAttr_write = 0x1 << 1,
};


class   UnboundedBuffer;
typedef QError  QCommandHandler(const std::vector<QString>& params, UnboundedBuffer& reply);

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

// server commands
QCommandHandler  select;

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


class QCommandTable
{
public:
    static QCommandTable& Instance();

    QError ExecuteCmd(const std::vector<QString>& params, UnboundedBuffer& reply);

private:
    
    QCommandTable();

    struct QCommandInfo
    {
        QString     cmd;
        int         attr;
        int         params;
        QCommandHandler* handler;
        bool  CheckParamsCount(int nParams) const;
    };

    static const QCommandInfo s_info[];
    std::map<QString, const QCommandInfo* >  m_handlers;
};


#endif

