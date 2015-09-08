
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
    m_localPort  =  INVALID_PORT;
}

ListenSocket::~ListenSocket()
{
    Server::Instance()->DelListenSock(m_localSock);
    USR << "close LISTEN socket " << m_localSock;
}

bool ListenSocket::Bind(const SocketAddr& addr)
{
    if (addr.Empty())
        return  false;

    if (m_localSock != INVALID_SOCKET)
        return false;

    m_localPort = addr.GetPort();

    m_localSock = CreateTCPSocket();
    SetNonBlock(m_localSock);
    SetNodelay(m_localSock);
    SetReuseAddr(m_localSock);
    SetRcvBuf(m_localSock);
    SetSndBuf(m_localSock);

    struct sockaddr_in serv = addr.GetAddr();

    int ret = ::bind(m_localSock, (struct sockaddr*)&serv, sizeof serv);
    if (SOCKET_ERROR == ret)
    {
        CloseSocket(m_localSock);
        ERR << "Cannot bind port " << addr.GetPort();
        return false;
    }
    ret = ::listen(m_localSock, ListenSocket::LISTENQ);
    if (SOCKET_ERROR == ret)
    {
        CloseSocket(m_localSock);
        ERR << "Cannot listen on port " << addr.GetPort();
        return false;
    }

    if (!NetThreadPool::Instance().AddSocket(shared_from_this(), EventTypeRead))
        return false;

    INF << "CREATE LISTEN socket " << m_localSock << " on port " <<  m_localPort;
    return true;
}

int ListenSocket::_Accept()
{
    socklen_t   addrLength = sizeof m_addrClient;
    return ::accept(m_localSock, (struct sockaddr *)&m_addrClient, &addrLength);
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
        Server::Instance()->DelListenSock(m_localSock);
        return  true;
    }

    return false;
}

}

