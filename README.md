# Qedis
[![Build Status](https://travis-ci.org/loveyacper/Qedis.svg?branch=master)](https://travis-ci.org/loveyacper/Qedis)
A C++11 implementation of Redis Sever(not including sentinel, cluster yet)

## Requirements
* C++11
* Linux or OS X (Port to Windows is under way)

## Full compatiable with redis
 You can test Qedis with redis-cli, redis-benchmark, or use redis as master with Qedis as slave, viceversa.
 
## Command List
###key commands
-type
-exists
-del
-expire
-pexpire
-expireat
-pexpireat
-ttl
-pttl
-persist
-move
-keys
-randomkey
-rename
-renamenx
-scan

###server commands
-select
-dbsize
-bgsave
-save
-lastsave
-flushdb
-flushall
-client
-debug
-shutdown
-bgrewriteaof
-ping
-echo
-info
-monitor
-auth

###string commands
-set
-get
-getrange
-setrange
-getset
-append
-bitcount
-bitop
-getbit
-setbit
-incr
-incrby
-incrbyfloat
-decr
-decrby
-mget
-mset
-msetnx
-setnx
-setex
-psetex
-strlen

###list commands
-lpush
-rpush
-lpushx
-rpushx
-lpop
-rpop
-lindex
-llen
-lset
-ltrim
-lrange
-linsert
-lrem
-rpoplpush
-blpop
-brpop
-brpoplpush

###hash commands
-hget
-hmget
-hgetall
-hset
-hsetnx
-hmset
-hlen
-hexists
-hkeys
-hvals
-hdel
-hincrby
-hincrbyfloat
-hscan
-hstrlen

###set commands
-sadd
-scard
-srem
-sismember
-smembers
-sdiff
-sdiffstore
-sinter
-sinterstore
-sunion
-sunionstore
-smove
-spop
-srandmember
-sscan

###sset
-zadd
-zcard
-zrank
-zrevrank
-zrem
-zincrby
-zscore
-zrange
-zrevrange
-zrangebyscore
-zrevrangebyscore
-zremrangebyrank
-zremrangebyscore

###pubsub
-subscribe
-unsubscribe
-publish
-psubscribe
-punsubscribe
-pubsub

###multi
-watch
-unwatch
-multi
-exec
-discard

###replication
-sync
-slaveof


## High Performance
 Run this command, compare with redis, try it!
 ```bash
 ./redis-benchmark -q -n 1000000 -P 50 -c 50
 ```

## TODO
* Support lua
* etc
