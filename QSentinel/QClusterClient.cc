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
    QClusterConn* conn = new ZookeeperConn(me, g_config.setid, myAddr.ToString());
#else
#error "Only support zookeeper for now, supporting etcd is in progress"
#endif

    conn->SetOnBecomeMaster([](const std::vector<SocketAddr>& slaves) {
        INF << "I become master";
        std::vector<QString> cmd {"slaveof", "no", "one"};
        slaveof(cmd, nullptr);

        for (const auto& addr : slaves)
        {
            INF << "Try connect to slave " << addr.ToString();
            // connect to slaves and send 'slave of me' 
            Server::Instance()->TCPConnect(addr, ConnectionTag::kSlaveClient);
        }
        // 读取所有set的数据保存其版本号 
        // *set的数据格式是  1,3,4,7|2:1,4
        // 含义:本set负责slot 1347，但是现在正在将slot 1,4迁移到set2上
    });

    conn->SetOnBecomeSlave([](const std::string& master) {
        INF << "I become slave of " << master;
        std::vector<QString> cmd(SplitString(master, ':'));
        slaveof({"slaveof", cmd[0], cmd[1]}, nullptr);
    });

    conn_.reset(conn);

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

