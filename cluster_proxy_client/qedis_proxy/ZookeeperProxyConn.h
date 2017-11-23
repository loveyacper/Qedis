#ifndef BERT_ZOOKEEPERCONN_H
#define BERT_ZOOKEEPERCONN_H

#include <memory>
#include "ClusterConn.h"
#include "zookeeper/ZookeeperContext.h"

namespace ananas
{
class Connection;
}

// 当连接zk成功时，先发起握手，再获取QEDIS节点列表，然后再监听集群客户端
//
// 对于集群客户端，先连接zk，成功后，同时注册自己和获取PROXY节点列表，然后就可以发起请求
// PROXY节点无状态，可以随意负载均衡的请求
class ZookeeperProxyConn : public qedis::ClusterConn
{
public:
    explicit
    ZookeeperProxyConn(ananas::Connection* c);

    virtual ~ZookeeperProxyConn();

public:
    bool OnData(const char*& data, size_t len) override final;
    void OnConnect() override final;
    void OnDisconnect() override final;

private:
    ananas::Connection* conn_;
    std::unique_ptr<qedis::ZookeeperContext> ctx_;
};

#endif

