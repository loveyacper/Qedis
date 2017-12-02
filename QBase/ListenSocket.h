
#ifndef BERT_LISTENSOCKET_H
#define BERT_LISTENSOCKET_H

#include "Socket.h"

namespace Internal
{

class ListenSocket : public Socket
{
    static const int LISTENQ;
public:
    explicit
    ListenSocket(int tag);
    ~ListenSocket();
    
    SocketType GetSocketType() const { return SocketType_Listen; }

    bool Bind(const SocketAddr& addr);
    bool OnReadable();
    bool OnWritable();
    bool OnError();

private:
    int _Accept();
    sockaddr_in     addrClient_;
    unsigned short  localPort_;
    const int       tag_;
};

}

#endif
