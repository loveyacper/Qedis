#if QEDIS_CLUSTER

#include "Log/Logger.h"
#include "Server.h"

#include "QConfig.h"
#include "QCommand.h"
#include "QClusterClient.h"

#if USE_ZOOKEEPER
#include "zookeeper/ZookeeperConn.h"
#endif

namespace qedis
{

PacketLength QClusterClient::_HandlePacket(const char* data, std::size_t bytes)
{
    const char* ptr = data;
    
    bool result = conn_->ParseMessage(ptr, bytes);
    if (!result)
    {
        OnError(); 
        return 0;
    }

    return static_cast<PacketLength>(ptr - data);
}

bool QClusterClient::Init(int fd, const SocketAddr& peer)
{ 
    if (!StreamSocket::Init(fd, peer))
        return false;

    auto me = std::static_pointer_cast<QClusterClient>(shared_from_this()); 
    SocketAddr myAddr(g_config.ip.c_str(), g_config.port);

#if USE_ZOOKEEPER
    conn_.reset(new ZookeeperConn(me, g_config.setid, myAddr.ToString()));
    QClusterConn* const conn = conn_.get();
#else
#error "Only support zookeeper for now, supporting etcd is in progress"
#endif

    extern
    void MigrateClusterData(const std::unordered_map<SocketAddr, std::set<int>>& ,
                            std::function<void ()> );

    conn->SetOnMigration([conn](const std::unordered_map<SocketAddr, std::set<int>>& migration) {
            // *set的数据格式是  1,3,4,7|ipport@1,4|ipport@3,7
            // 含义:本set负责slot 1347，但是现在正在将slot 1,4迁移到6379机器
            MigrateClusterData(migration, [conn]() {
                INF << "MigrateClusterData done";
                conn->UpdateShardData();
            });
    });

    conn->SetOnBecomeMaster([](const std::vector<SocketAddr>& slaves) {
        INF << "I become master!";
        std::vector<QString> cmd {"slaveof", "no", "one"};
        slaveof(cmd, nullptr);

        for (const auto& addr : slaves)
        {
            INF << "Try connect to slave " << addr.ToString();
            // connect to slaves and send 'slave of me' 
            Server::Instance()->TCPConnect(addr, ConnectionTag::kSlaveClient);
        }
    });

    conn->SetOnBecomeSlave([](const std::string& master) {
        INF << "I become slave of " << master;
        std::vector<QString> cmd(SplitString(master, ':'));
        slaveof({"slaveof", cmd[0], cmd[1]}, nullptr);
    });

    return true; 
}

void QClusterClient::OnConnect()
{
    conn_->OnConnect();
}

void QClusterClient::OnDisconnect()
{
    conn_->OnDisconnect();
}

} // end namespace qedis

#endif

