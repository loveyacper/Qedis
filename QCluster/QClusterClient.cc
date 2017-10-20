#if QEDIS_CLUSTER

#include "Log/Logger.h"

#include <algorithm>
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
    const char* ptr  = data;
    
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
#if USE_ZOOKEEPER
    SocketAddr myAddr(g_config.ip.c_str(), g_config.port);
    ZookeeperConn* zkconn = new ZookeeperConn(me, g_config.setid, myAddr.ToString());
    zkconn->SetOnBecomeMaster([]() {
        INF << "I become master";
        std::vector<QString> cmd {"slaveof", "no", "one"};
        slaveof(cmd, nullptr);
    });
    zkconn->SetOnBecomeSlave([](const std::string& master) {
        INF << "I become slave of " << master;
        std::vector<QString> cmd(SplitString(master, ':'));
        slaveof({"slaveof", cmd[0], cmd[1]}, nullptr);
    });
    conn_.reset(zkconn);
#else
#error "Only support zookeeper for now, supporting etcd is in progress"

#endif

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

