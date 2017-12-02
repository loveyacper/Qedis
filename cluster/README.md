# Qedis分布式集群

## 环境需求
* C++11、CMake
* zookeeper
* Linux 或 MAC OS

## 代码目录

* ananas

  一个C++11编写的网络库，提供了强大的future异步编程模式.

* cluster_conn

  针对zookeeper或etcd的包装。目前只提供了zookeeper.

* qedis_proxy

  Qedis代理服务器，负责发现Qedis服务，转发客户端请求和Qedis服务器响应.
 
## Future模式
  本目录代码采用了基于Future模式的异步编程,例如与zookeeper集群连接时,需要进行7个步骤:


  * 握手
  * 注册自己，同时获取Qedis set信息,这是两个并行的异步请求
  * 根据set信息，获取分片信息
  * 存储分片信息
  * 发起异步请求:获取Qedis服务列表
  * 存储Qedis服务信息
  * 初始化ping定时器
  * 如若以上操作任一无响应，触发超时逻辑
  这一连串的操作使用future模式编写如下,每一个Then都是前面异步请求的回调:
  ```cpp
ctx_->DoHandshake()
    .Then([me = this](const ZkResponse& rsp) mutable {
        return me->_ProcessHandshake(rsp);
    })
    .Then([me = this](ananas::Try<qedis::ZookeeperContext* >&& tctx) mutable {
        return me->_RegisterAndGetServers(std::move(tctx));
    })
    .Then([me = this](const std::vector<ananas::Try<ZkResponse> >& rsps) mutable {
        return me->_GetShardingInfo(rsps);
    })
    .Then([me = this](const std::vector<ananas::Try<ZkResponse> >& vrsp) mutable {
        if (!me->_ProcessShardingInfo(vrsp)) {
            using InnerType = std::vector<ananas::Try<ZkResponse>>;
            auto exp = std::runtime_error("ProcessShardingInfo failed");
            return ananas::MakeExceptionFuture<InnerType>(exp);
        }

        // 5. get qedis server's list and watch the qedis server list
        return me->_GetServers(vrsp);
    })
    .Then([me = this](const std::vector<ananas::Try<ZkResponse> >& vrsp) mutable {
        return me->_ProcessServerInfo(vrsp);
    })
    .Then([me = this](bool succ) {
        if (succ)
            me->_InitPingTimer();
    })
    .OnTimeout(std::chrono::seconds(3), []() {
            // 3秒钟超时
            std::cout << "OnTimeout handshake\n";
            ananas::EventLoop::ExitApplication();
        }, conn_->GetLoop()
    );
  ```

## 集群特性
 待写，代码实现中。。。

