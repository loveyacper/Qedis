# Qedis
[![Build Status](https://travis-ci.org/loveyacper/Qedis.svg?branch=master)](https://travis-ci.org/loveyacper/Qedis)

[Click me switch to English](README.en.md)

C++11实现的增强版分布式Redis服务器,使用Leveldb作为持久化存储引擎。

## 环境需求
* C++11、CMake
* Linux 或 MAC OS

## 集群特性
 可以搭建Zookeeper，监视一组互为主备的Qedis进程以实现高可用;

 当然也可以使用官方redis-sentinel。

 详见[cluster Readme](QCluster/README.md)

 scale-out集群正在开发中...

## 与Redis完全兼容
 你可以用redis的各种工具来测试Qedis，比如官方的redis-cli, redis-benchmark。

 Qedis可以和redis之间进行复制，可以读取redis的rdb文件或aof文件。当然，Qedis生成的aof或rdb文件也可以被redis读取。

 你还可以用redis-sentinel来实现Qedis的高可用！

 总之，Qedis与Redis完全兼容。

## 高性能
- Qedis性能大约比Redis3.2高出20%(使用redis-benchmark测试pipeline请求，比如设置-P=50或更高)
- Qedis的高性能有一部分得益于独立的网络线程处理IO，因此和redis比占了便宜。但Qedis逻辑仍然是单线程的。
- 另一部分得益于C++ STL的高效率（CLANG的表现比GCC更好）。
- 在测试前，你要确保std::list的size()是O(1)复杂度，这才遵循C++11的标准。否则list相关命令不可测。

运行下面这个命令，试试和redis比一比~
```bash
./redis-benchmark -q -n 1000000 -P 50 -c 50
```

 我在rMBP late2013笔记本上测试结果如图：

![image](https://github.com/loveyacper/Qedis/blob/master/performance.png)


## 编写扩展模块
 Qedis支持动态库模块，可以在运行时添加新命令。
 我添加了三个命令(ldel, skeys, hgets)作为演示。

## 支持冷数据淘汰
 是的，在内存受限的情况下，你可以让Qedis根据简单的LRU算法淘汰一些key以释放内存。

## 主从复制，事务，RDB/AOF持久化，慢日志，发布订阅
 这些特性Qedis都有:-)

## 持久化：内存不再是上限
 Leveldb可以配置为Qedis的持久化存储引擎，可以存储更多的数据。


## 命令列表
#### 展示Qedis支持的所有命令，目前支持137个命令
- cmdlist

## TODO
* 支持lua
* Qedis Cluster多语言客户端
