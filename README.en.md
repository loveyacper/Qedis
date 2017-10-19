# Qedis
[![Build Status](https://travis-ci.org/loveyacper/Qedis.svg?branch=master)](https://travis-ci.org/loveyacper/Qedis)

[看中文说明请点我](README.md)

A C++11 implementation of Distributed Redis Server, use Leveldb for persist storage.(including cluster)

## Requirements
* C++11
* Linux or OS X

## Support module for write your own extensions
 Qedis supports module now, still in progress, much work to do.
 I added three commands(ldel, skeys, hgets) for demonstration.

## Persistence: Not limited to memory
 Leveldb can be configured as backend for Qedis.

## Fully compatible with redis
 You can test Qedis with redis-cli, redis-benchmark, or use redis as master with Qedis as slave or conversely, it also can work with redis sentinel.

## High Performance
- Qedis is approximately 20-25% faster than redis if run benchmark with pipeline requests(set -P = 50 or higher).
- Average 80K requests per seconds for write, and 90K requests per seconds for read.
- Before run test, please ensure that std::list::size() is O(1), obey the C++11 standards.

Run this command, compare with redis use pipeline commands, try it.
```bash
./redis-benchmark -q -n 1000000 -P 50 -c 50
```

![image](https://github.com/loveyacper/Qedis/blob/master/performance.png)

## Support LRU cache
 When memory is low, you can make Qedis to free memory by evict some key according to LRU.

## Master-slave Replication, transaction, RDB/AOF, slow log, publish-subscribe
 Qedis supports all of them :-)
 
## Command List
#### show all supported commands list, about 137 commands
- cmdlist

## TODO
* Support lua
* Golang Cluster client

