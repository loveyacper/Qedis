#ifndef BERT_CLUSTERMANAGER_H
#define BERT_CLUSTERMANAGER_H

#include <unordered_map>
#include <vector>
#include <string>

namespace ananas
{
    class EventLoop;
    class Connection;
    struct SocketAddr;
}

class ClusterManager
{
public:
    static ClusterManager& Instance();

    ClusterManager();

    void SetEventLoop(ananas::EventLoop* loop);
    void AddClusterAddr(const std::string& host);
    bool Connect();

    void OnNewConnection(ananas::Connection* conn);
    void OnConnect(ananas::Connection* conn);
    void OnDisconnect(ananas::Connection* conn);
    void OnConnFail(ananas::EventLoop* loop, const ananas::SocketAddr& peer);

    void AddShardingInfo(int setid, const std::vector<std::string>& shardings);
    void AddServerInfo(int setid, const std::string& host);

    const std::string& GetServer(const std::string& key) const;

private:
    int index_ = -1;
    std::vector<ananas::SocketAddr> cluster_;

    // sharding-id -> set-id
    std::unordered_map<int, int> shardingInfo_;

    // set-id -> server-list
    std::unordered_map<int, std::vector<std::string>> hostInfo_;

    ananas::EventLoop* loop_;
};

#endif

