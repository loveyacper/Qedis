
#ifndef BERT_CLIENTSOCKET_H
#define BERT_CLIENTSOCKET_H

#include "Socket.h"

// Abstraction for a TCP client socket
class ClientSocket : public Socket
{
public:
    explicit ClientSocket(bool retry = true);
    virtual ~ClientSocket();
    bool    Connect(const SocketAddr& addr);
    bool    OnWritable();
    bool    OnError();
    SocketType GetSockType() const { return SocketType_Client; }

private:
    SocketAddr       m_peerAddr;
    bool             m_retry;
};

#endif

