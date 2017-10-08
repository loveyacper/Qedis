#ifndef BERT_QCLUSTERCLIENT_H
#define BERT_QCLUSTERCLIENT_H

#if QEDIS_CLUSTER

#include "StreamSocket.h"
#include "QClusterInterface.h"

namespace qedis
{

class QClusterClient: public StreamSocket
{
public:
    void OnConnect() override;
    bool Init(int fd, const SocketAddr& peer);
    
private:
    PacketLength _HandlePacket(const char*, std::size_t) override;

    std::unique_ptr<QClusterConn> conn_;
};
    
}

#endif // endif QEDIS_CLUSTER

#endif // endif BERT_QCLUSTERCLIENT_H

