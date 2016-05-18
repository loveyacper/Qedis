
#include <errno.h>
#include <sys/socket.h>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include "Server.h"
#include "NetThreadPool.h"
#include "ListenSocket.h"
#include "Log/Logger.h"

namespace Internal
{

const int ListenSocket::LISTENQ = 1024;

ListenSocket::ListenSocket() 
{
    localPort_  =  INVALID_PORT;
}

ListenSocket::~ListenSocket()
{
    Server::Instance()->DelListenSock(localSock_);
    USR << "Close ListenSocket " << localSock_;
}

bool ListenSocket::Bind(const SocketAddr& addr)
{
    if (addr.Empty())
        return  false;

    if (localSock_ != INVALID_SOCKET)
        return false;

    localPort_ = addr.GetPort();

    localSock_ = CreateTCPSocket();
    SetNonBlock(localSock_);
    SetNodelay(localSock_);
    SetReuseAddr(localSock_);
    SetRcvBuf(localSock_);
    SetSndBuf(localSock_);

    struct sockaddr_in serv = addr.GetAddr();

    int ret = ::bind(localSock_, (struct sockaddr*)&serv, sizeof serv);
    if (SOCKET_ERROR == ret)
    {
        CloseSocket(localSock_);
        ERR << "Cannot bind port " << addr.GetPort();
        return false;
    }
    ret = ::listen(localSock_, ListenSocket::LISTENQ);
    if (SOCKET_ERROR == ret)
    {
        CloseSocket(localSock_);
        ERR << "Cannot listen on port " << addr.GetPort();
        return false;
    }

    if (!NetThreadPool::Instance().AddSocket(shared_from_this(), EventTypeRead))
        return false;

    INF << "Success: listen on ("
        << addr.GetIP()
        << ":"
        << addr.GetPort()
        << ")";
    
    return true;
}

int ListenSocket::_Accept()
{
    socklen_t   addrLength = sizeof addrClient_;
    return ::accept(localSock_, (struct sockaddr *)&addrClient_, &addrLength);
}

bool ListenSocket::OnReadable()
{
    while (true)
    {
        int connfd = _Accept();
        if (connfd >= 0)
        {
            Server::Instance()->NewConnection(connfd);
        }
        else
        {
            bool result = false;
            switch (errno)
            {
            case EWOULDBLOCK:
            case ECONNABORTED:
            case EINTR:
                result = true;
                break;

            case EMFILE:
            case ENFILE:
                ERR << "Not enough file descriptor available!!!";
                result = true;
                break;

            case ENOBUFS:
                ERR << "Not enough memory";          
                result = true;

            default:
                ERR << "When accept, unknown error happened : " << errno;          
                break;
            }

            return result;
        }
    }
    
    return true;
}

bool ListenSocket::OnWritable()
{
    return false;
}

bool ListenSocket::OnError()
{
    if (Socket::OnError())
    {
        Server::Instance()->DelListenSock(localSock_);
        return  true;
    }

    return false;
}

}

