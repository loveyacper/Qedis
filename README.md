# Qedis
[![Build Status](https://travis-ci.org/loveyacper/Qedis.svg?branch=master)](https://travis-ci.org/loveyacper/Qedis)
A C++11 implementation of Redis Server(not including sentinel, cluster yet)

## Requirements
* C++11
* Linux or OS X (Port to Windows is under way)

## Support module for write your own extensions
 Qedis supports module now, still in progress, much work to do.
 I added three commands(ldel skeys hgets) to give a demonstration.
 
## Full compatible with redis
 You can test Qedis with redis-cli, redis-benchmark, or use redis as master with Qedis as slave or conversely.

## High Performance
- Qedis is approximately 20-25% faster than redis if run benchmark with pipeline requests(set -P = 50).
- Average 80K requests per seconds for write, and 90K requests per seconds for read.
- Before run test, please ensure that std::list::size() is O(1), obey the C++11 standards.

Run this command, compare with redis use pipeline commands, try it.
```bash
./redis-benchmark -q -n 1000000 -P 50 -c 50
```
 
## Command List
#### show all supported commands list
- cmdlist

#### module commands
- module

#### key commands
- type exists del expire pexpire expireat pexpireat ttl pttl persist move keys randomkey rename renamenx scan

#### server commands
- select dbsize bgsave save lastsave flushdb flushall client debug shutdown bgrewriteaof ping echo info monitor auth

#### string commands
- set get getrange setrange getset append bitcount bitop getbit setbit incr incrby incrbyfloat decr decrby mget mset msetnx setnx setex psetex strlen

#### list commands
- lpush rpush lpushx rpushx lpop rpop lindex llen lset ltrim lrange linsert lrem rpoplpush blpop brpop brpoplpush

#### hash commands
- hget hmget hgetall hset hsetnx hmset hlen hexists hkeys hvals hdel hincrby hincrbyfloat hscan hstrlen

#### set commands
- sadd scard srem sismember smembers sdiff sdiffstore sinter sinterstore sunion sunionstore smove spop srandmember sscan

#### sorted set commands
- zadd zcard zrank zrevrank zrem zincrby zscore zrange zrevrange zrangebyscore zrevrangebyscore zremrangebyrank zremrangebyscore

#### pubsub commands
- subscribe unsubscribe publish psubscribe punsubscribe pubsub

#### multi commands
- watch unwatch multi exec discard

#### replication commands
- sync slaveof


## TODO
* Support lua
* Sentinel & Cluster
* etc
