
#ifndef BERT_CLIENTSOCKET_H
#define BERT_CLIENTSOCKET_H

#include <functional>
#include "Socket.h"

// Abstraction for a TCP client socket
class ClientSocket : public Socket
{
public:
    ClientSocket();
    ~ClientSocket();
    bool    Connect(const SocketAddr& addr);
    bool    OnWritable();
    bool    OnError();
    SocketType GetSockType() const { return SocketType_Client; }

    void SetFailCallback(const std::function<void ()>& cb) { onConnectFail_ = cb; }

private:
    SocketAddr peerAddr_;
    std::function<void ()> onConnectFail_;
};

#endif

