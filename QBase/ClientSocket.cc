
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
    retry_(retry)
{
}

ClientSocket::~ClientSocket()
{
    WITH_LOG(if (localSock_ == INVALID_SOCKET) \
        ERR << "close invalid client socket key " << this; \
    else \
        INF << __FUNCTION__ << " close Client socket " <<  localSock_);
}

bool ClientSocket::Connect(const SocketAddr& dst)
{
    if (dst.Empty())
        return false;

    if (INVALID_SOCKET != localSock_)
        return false;

    peerAddr_ = dst;

    localSock_ = CreateTCPSocket();
    SetNonBlock(localSock_);
    SetNodelay(localSock_);
    SetRcvBuf(localSock_);
    SetSndBuf(localSock_);

    int  result = ::connect(localSock_, (sockaddr*)&peerAddr_.GetAddr(), sizeof(sockaddr_in));

    if (0 == result)
    {
        WITH_LOG(INF << "CLIENT socket " << localSock_ << ", immediately connected to port " << peerAddr_.GetPort());
       
        Server::Instance()->NewConnection(localSock_, retry_);
        localSock_ = INVALID_SOCKET;
        return  true;
    }
    else
    {
        if (EINPROGRESS == errno)
        {
            WITH_LOG(INF << "EINPROGRESS : client socket " << localSock_ <<", connected to " << dst.GetIP() << ":" << peerAddr_.GetPort());
            
            Internal::NetThreadPool::Instance().AddSocket(shared_from_this(), EventTypeWrite);
            epollOut_ = true;
            return true;
        }

        WITH_LOG(ERR << "Error client socket " << localSock_ << ", connected to " << peerAddr_.GetIP());
        if (retry_)
            Server::Instance()->TCPReconnect(peerAddr_);
        return false;
    }
    
    return true;
}


bool ClientSocket::OnWritable()
{
    epollOut_ = false;
    int         error  = 0;
    socklen_t   len    = sizeof error;

    bool bSucc = (::getsockopt(localSock_, SOL_SOCKET, SO_ERROR, (char*)&error, &len) >= 0 && 0 == error);
    if (!bSucc)    
    {
        errno = error;
        WITH_LOG(ERR << "FAILED client socket " << localSock_ \
            << ", connect to " << peerAddr_.GetPort() \
            << ", error " << error);

        return false;
    }

    WITH_LOG(INF << "Success client socket " << localSock_ << ", connect to " << peerAddr_.GetPort());

    Server::Instance()->NewConnection(localSock_, retry_);
    localSock_ = INVALID_SOCKET; 

    return true;
}

bool ClientSocket::OnError()
{
    if (Socket::OnError())
    {
        WITH_LOG(ERR << "OnError clientsocket " << localSock_ << ", and this " << this);
        
        if (retry_)
            Server::Instance()->TCPReconnect(peerAddr_);

        return  true;
    }
        
    return  false;
}

