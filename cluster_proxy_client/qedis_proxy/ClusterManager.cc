#include "ClusterManager.h"
#include "ProxyLog.h"

#include "net/EventLoop.h"
#include "net/Connection.h"
#include "net/Socket.h"

#if USE_ZOOKEEPER
    #include "ZookeeperConn.h"
#else
    #error "Please define USE_ZOOKEEPER to non-zero"
#endif


ClusterManager& ClusterManager::Instance()
{
    static ClusterManager mgr;
    return mgr;
}

ClusterManager::ClusterManager() :
    loop_(nullptr)
{
}

void ClusterManager::SetEventLoop(ananas::EventLoop* loop)
{
    assert (!loop_);
    loop_ = loop;
}

bool ClusterManager::Connect()
{
    if (cluster_.empty())
        return false;

    index_ ++;
    if (index_ >= cluster_.size())
        index_ = 0;

    const ananas::SocketAddr& addr = cluster_[index_];
    return loop_->Connect(addr,
                          std::bind(&ClusterManager::OnNewConnection, this, std::placeholders::_1),
                          std::bind(&ClusterManager::OnConnFail, this, std::placeholders::_1, std::placeholders::_2), 
                          ananas::DurationMs(1000));
}

void ClusterManager::AddClusterAddr(const std::string& host)
{
    ananas::SocketAddr addr(host);
    cluster_.push_back(addr);
}

void ClusterManager::OnNewConnection(ananas::Connection* conn)
{
    INF(g_logger) << "ClusterManager::OnNewConnection " << conn->Identifier();

#if USE_ZOOKEEPER
    auto ctx = std::make_shared<ZookeeperConn>(conn);
#else
    #error "Please define USE_ZOOKEEPER to non-zero"
#endif

    using ananas::PacketLen_t;
    using ananas::Connection;

    conn->SetUserData(ctx);
    conn->SetMinPacketSize(4);
    conn->SetOnMessage([ctx](Connection* c, const char* data, PacketLen_t len) -> PacketLen_t {
         const char* ptr = data;
         if (!ctx->OnData(ptr, len)) {
             c->ActiveClose();
         }

         return static_cast<PacketLen_t>(ptr - data);
    });
    conn->SetOnConnect(std::bind(&ClusterManager::OnConnect, this, std::placeholders::_1));
    conn->SetOnDisconnect(std::bind(&ClusterManager::OnDisconnect, this, std::placeholders::_1));
}

void ClusterManager::OnConnect(ananas::Connection* conn)
{
    INF(g_logger) << "ClusterManager::OnConnect " << conn->Identifier();

    auto ctx = conn->GetUserData<qedis::ClusterConn>();
    ctx->OnConnect();
}

void ClusterManager::OnDisconnect(ananas::Connection* conn)
{
    INF(g_logger) << "ClusterManager::OnDisconnect " << conn->Identifier();

    auto ctx = conn->GetUserData<qedis::ClusterConn>();
    ctx->OnDisconnect();
}

void ClusterManager::OnConnFail(ananas::EventLoop* loop, const ananas::SocketAddr& peer)
{
    INF(g_logger) << "ClusterManager::OnConnFail " << peer.ToString();

    loop_->Sleep(ananas::DurationMs(2000))
          .Then(std::bind(&ClusterManager::Connect, this));
}

void ClusterManager::AddShardingInfo(int setid, const std::vector<std::string>& shardings)
{
    for (const auto& shard : shardings)
    {
        int id = std::stoi(shard);
        bool succ = shardingInfo_.insert({id, setid}).second;
        if (succ)
            DBG(g_logger) << "SUCC: Insert sharding info " << id << ":" << setid;
        else
            DBG(g_logger) << "FAILED: Insert sharding info " << id << ":" << setid;
    }
}

void ClusterManager::AddServerInfo(int setid, const std::string& host)
{
    auto& svrlist = hostInfo_[setid];
    svrlist.push_back(host);
    DBG(g_logger) << "AddServerInfo : " << setid << ":" << host;
}


const std::string& ClusterManager::GetServer(const std::string& key) const
{
    // TODO calc key's hash value
    const int hash = 1;

    auto it = shardingInfo_.find(hash);
    if (it != shardingInfo_.end())
    {
        auto itHost  = hostInfo_.find(it->second);
        if (itHost != hostInfo_.end())
        {
            return itHost->second[0]; // return the first addr
        }
    }

    static const std::string kEmpty;
    return kEmpty;
}

