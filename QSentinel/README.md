# Qedis高可用集群

利用Zookeeper实现的Qedis高可用集群，功能类似redis-sentinel。在此基础上可实现sacle-out集群。

## 环境需求
* C++11、CMake
* zookeeper
* Linux 或 MAC OS

## 集群特性
 搭建Zookeeper，监视一组互为主备的Qedis进程以实现高可用;

 当然也可以使用官方redis-sentinel。

 scale-out集群正在开发中...

## 简单原理

    /servers
        /set-1
            /qedis-(127.0.0.1:6379)-0001
            /qedis-(127.0.0.1:6381)-0003
            /qedis-(127.0.0.1:6382)-0004
            /qedis-(127.0.0.1:6385)-0007
        /set-2
            /qedis-(127.0.0.1:16379)-0004
            /qedis-(127.0.0.1:16389)-0007
            /qedis-(127.0.0.1:16399)-0008

 一组Qedis进程形成一个set，set内最多只有一个master，其它都是slave，且没有级联复制结构。

 通过配置文件中setid来配置set，相同setid的Qedis进程将形成一个set。

 通过设置配置文件中cluster开关，Qedis在启动时，将尝试向Zookeeper的/servers/set-{id}/下创建自己的临时顺序节点。

 创建成功后，获取孩子列表，看自己的节点序号是不是最小。

 如果是最小，则是master，向所有孩子发送slaveof my_addr的命令；

 如果不是，则监视比自己序号大的孩子中序号最小的节点。比如我的序号是7，孩子列表序号是1，3，4，7，则我监视4节点。

 当我收到监视的节点被删除的通知，则判断自己是否是master（因为启动时已经获得孩子列表了）。

 是或不是，都继续重复上述过的逻辑。
