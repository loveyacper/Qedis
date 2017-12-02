
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


ClientSocket::ClientSocket(int tag) : tag_(tag)
{
}

ClientSocket::~ClientSocket()
{
    if (localSock_ != INVALID_SOCKET)
        WRN << "Destruct ClientSocket " << localSock_;
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
        INF << "ClientSocket immediately connected to ["
            << peerAddr_.GetIP()
            << ":"
            << peerAddr_.GetPort()
            << "]";
       
        Server::Instance()->NewConnection(localSock_, tag_, onConnectFail_);
        localSock_ = INVALID_SOCKET;
        return  true;
    }
    else
    {
        if (EINPROGRESS == errno)
        {
            
            INF << "EINPROGRESS: ClientSocket connected to ("
                << peerAddr_.GetIP()
                << ":"
                << peerAddr_.GetPort()
                << ")";
            
            Internal::NetThreadPool::Instance().AddSocket(shared_from_this(), EventTypeWrite);
            epollOut_ = true;
            return true;
        }

        ERR << "Failed: ClientSocket connected to ("
            << peerAddr_.GetIP()
            << ":"
            << peerAddr_.GetPort()
            << ")";

        if (onConnectFail_)
            onConnectFail_();

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
        ERR << "Failed: ClientSocket connected to ("
            << peerAddr_.GetIP()
            << ":"
            << peerAddr_.GetPort()
            << "), error:" << error;

        return false;
    }

    INF << "Success: ClientSocket connected to ("
        << peerAddr_.GetIP()
        << ":"
        << peerAddr_.GetPort() << ")";

    Server::Instance()->NewConnection(localSock_, tag_, onConnectFail_);
    localSock_ = INVALID_SOCKET; 

    return true;
}

bool ClientSocket::OnError()
{
    if (Socket::OnError())
    {
        ERR << __FUNCTION__ << " connected to ("
            << peerAddr_.GetIP()
            << ":"
            << peerAddr_.GetPort() << ")";
        
        if (onConnectFail_)
            onConnectFail_();

        return  true;
    }
        
    return  false;
}

