
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include "Server.h"
#include "ClientSocket.h"
#include "Log/Logger.h"
#include "NetThreadPool.h"


ClientSocket::ClientSocket(bool retry) :
    m_retry(retry)
{
}

ClientSocket::~ClientSocket()
{
    if (m_localSock == INVALID_SOCKET)
        ERR << "close invalid client socket key " << this;
    else
        INF << __FUNCTION__ << " close Client socket " <<  m_localSock;
}

bool ClientSocket::Connect(const SocketAddr& dst)
{
    if (dst.Empty())
        return false;

    if (INVALID_SOCKET != m_localSock)
        return false;

    m_peerAddr = dst;

    m_localSock = CreateTCPSocket();
    SetNonBlock(m_localSock);
    SetNodelay(m_localSock);
    SetRcvBuf(m_localSock);
    SetSndBuf(m_localSock);

    int  result = ::connect(m_localSock, (sockaddr*)&m_peerAddr.GetAddr(), sizeof(sockaddr_in));

    if (0 == result)
    {
        INF << "CLIENT socket " << m_localSock << ", immediately connected to port " << m_peerAddr.GetPort();
       
        Server::Instance()->NewConnection(m_localSock, m_retry);
        m_localSock = INVALID_SOCKET;
        return  true;
    }
    else
    {
        if (EINPROGRESS == errno)
        {
            INF << "EINPROGRESS : client socket " << m_localSock <<", connected to " << dst.GetIP() << ":" << m_peerAddr.GetPort();
            
            Internal::NetThreadPool::Instance().AddSocket(shared_from_this(), EventTypeWrite);
            m_epollOut = true;
            return true;
        }

        ERR << "Error client socket " << m_localSock << ", connected to " << m_peerAddr.GetIP();
        if (m_retry)
            Server::Instance()->TCPReconnect(m_peerAddr);
        return false;
    }
    
    return true;
}


bool ClientSocket::OnWritable()
{
    m_epollOut = false;
    int         error  = 0;
    socklen_t   len    = sizeof error;

    bool bSucc = (::getsockopt(m_localSock, SOL_SOCKET, SO_ERROR, (char*)&error, &len) >= 0 && 0 == error);
    if (!bSucc)    
    {
        errno = error;
        ERR << "FAILED client socket " << m_localSock 
            << ", connect to " << m_peerAddr.GetPort()
            << ", error " << error;

        return false;
    }

    INF << "Success client socket " << m_localSock << ", connect to " << m_peerAddr.GetPort();

    Server::Instance()->NewConnection(m_localSock, m_retry);
    m_localSock = INVALID_SOCKET; 

    return true;
}

bool ClientSocket::OnError()
{
    if (Socket::OnError())
    {
        ERR << "OnError clientsocket " << m_localSock << ", and this " << this;
        
        if (m_retry)
            Server::Instance()->TCPReconnect(m_peerAddr);

        return  true;
    }
        
    return  false;
}

