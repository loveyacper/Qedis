#ifndef BERT_QSLAVECLIENT_H
#define BERT_QSLAVECLIENT_H

#include "StreamSocket.h"

namespace qedis
{

class QSlaveClient : public StreamSocket
{
public:
    void OnConnect() override;
private:
    PacketLength _HandlePacket(const char* msg, std::size_t len) override;
};
    
}

#endif

