#include "Log/Logger.h"

#include <algorithm>
#include "QConfig.h"
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
    conn_.reset(new ZookeeperConn(me)); 
#else
#error "Only support zookeeper for now, supporting etcd is in progress"

#endif

    return true; 
}

void QClusterClient::OnConnect()
{
    conn_->OnConnect();

#if 0
    // cluster 2.when connect, create my temp node
    SocketAddr myAddr(g_config.ip.c_str(), g_config.port);
    conn_->RunForMaster(g_config.setid, myAddr.ToString());
#endif
}

} // end namespace qedis

