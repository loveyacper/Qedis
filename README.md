# Qedis
[![Build Status](https://travis-ci.org/loveyacper/Qedis.svg?branch=master)](https://travis-ci.org/loveyacper/Qedis)
A C++11 implementation of Redis(not including sentinel, cluster yet)

## Requirements
* C++11
* Linux or OS X (Port to Windows is under way)

## Full compatiable with redis
 You can test Qedis with redis-cli, redis-benchmark, or use redis as master with Qedis as slave, viceversa.

## High Performance
 Run this command, compare with redis, try it!
 ```bash
 ./redis-benchmark -q -n 1000000 -P 50 -c 50
 ```

## TODO
* Fix propogate problem (not exact correct so far)
* Support lua
* etc
