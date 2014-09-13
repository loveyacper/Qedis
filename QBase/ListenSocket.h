
#ifndef BERT_LISTENSOCKET_H
#define BERT_LISTENSOCKET_H

#include "Socket.h"

namespace Internal
{

class ListenSocket : public Socket
{
    static const int LISTENQ;
public:
    ListenSocket();
    ~ListenSocket();
    
    SocketType GetSocketType() const { return SocketType_Listen; }

    bool Bind(const SocketAddr& addr);
    bool OnReadable();
    bool OnWritable();
    bool OnError();

private:
    int _Accept();
    sockaddr_in     m_addrClient;
    unsigned short  m_localPort;
};

}

#endif
