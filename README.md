
     _____     _____   _____   _   ______                  
    /  _  \   | ____| |  _  \ | | /  ___/                        
    | | | |   | |__   | | | | | | | |____
    | | | |   |  __|  | | | | | |  \__   \
    | |_| |_  | |___  | |_| | | |  ___|  |
    \_______| |_____| |_____/ |_| /_____/


[![Build Status](https://travis-ci.org/loveyacper/Qedis.svg?branch=master)](https://travis-ci.org/loveyacper/Qedis)

[看中文说明请点我](README.zh.md)

A C++11 implementation of distributed redis server, use Leveldb for persist storage.(including cluster)

## Requirements
* C++11 & CMake
* Linux or OS X

## Cluster Features
 Use zookeeper for leader election, to reach high availability.

 Of course you can also use redis-sentinel.

 See details in [cluster Readme](cluster/README.md), still in development.

## Fully compatible with redis
 You can test Qedis with redis-cli, redis-benchmark, or use redis as master with Qedis as slave or conversely, it also can work with redis sentinel.
 In a word, Qedis is full compatible with Redis.

## Persistence: Not limited to memory
 Leveldb can be configured as backend for Qedis.

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
 Qedis supports them all :-)
 
## Command List
#### show all supported commands list, about 140 commands
- cmdlist

## TODO
* Support lua
* Golang Cluster client

