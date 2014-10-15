
#include <cassert>

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <net/if.h>

#if defined(__APPLE__)
#include <unistd.h>
#endif

#include "Socket.h"
#include "NetThreadPool.h"
#include "Log/Logger.h"

unsigned int Socket::sm_id = 0;

Socket::Socket() : m_localSock(INVALID_SOCKET),
                   m_epollOut(false),
                   m_invalid(0)
{
    AtomicChange(&sm_id, 1);
    if (sm_id == 0)
        AtomicChange(&sm_id, 1);
                   
    m_id = AtomicGet(&sm_id);
}

Socket::~Socket()
{
    CloseSocket(m_localSock);
}

int Socket::CreateUDPSocket()
{
    return ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

bool Socket::OnError()   
{
    if (0 == AtomicTestAndSet(&m_invalid, 0, 1))
    {
        //CloseSocket(m_localSock);
        return true;
    }

    return false;
}

int Socket::CreateTCPSocket()
{
    return ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

void Socket::CloseSocket(int& sock)
{
    if (sock != INVALID_SOCKET)
    {
        TEMP_FAILURE_RETRY(::shutdown(sock, SHUT_RDWR));
        TEMP_FAILURE_RETRY(::close(sock));
        USR << "CloseSocket " << sock;
        sock = INVALID_SOCKET;
    }
}

void  Socket::SetNonBlock(int sock, bool nonblock)
{
    int flag = ::fcntl(sock, F_GETFL, 0); 
    assert(flag >= 0 && "Non Block failed");

    if (nonblock)
        flag = ::fcntl(sock, F_SETFL, flag | O_NONBLOCK);
    else
        flag = ::fcntl(sock, F_SETFL, flag & ~O_NONBLOCK);
    
}

void Socket::SetNodelay(int sock)
{
    int nodelay = 1;
    ::setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(int));
}

void Socket::SetSndBuf(int sock, socklen_t winsize)
{
    ::setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&winsize, sizeof(winsize));
}

void Socket::SetRcvBuf(int sock, socklen_t winsize)
{
    ::setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&winsize, sizeof(winsize));
}

void Socket::SetReuseAddr(int sock)
{
    int reuse = 1;
    ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
}

bool Socket::GetLocalAddr(int  sock, SocketAddr& addr)///std::string&  ip, unsigned short& port)
{
    sockaddr_in localAddr;
    socklen_t   len = sizeof(localAddr);

    if (0 == ::getsockname(sock, (struct sockaddr*)&localAddr, &len))
    {
        addr.Init(localAddr);
    }
    else
    {
        return  false;
    }

    return  true;
}

bool  Socket::GetPeerAddr(int  sock, SocketAddr& addr)
{
    sockaddr_in  remoteAddr;
    socklen_t    len = sizeof(remoteAddr);
    if (0 == ::getpeername(sock, (struct sockaddr*)&remoteAddr, &len))
    {
        addr.Init(remoteAddr);
    }
    else
    {
        return  false;
    }

    return  true;
}

void Socket::GetMyAddrInfo(unsigned int* addrs, int num)
{
    char buff[BUFSIZ];
    struct ifconf conf;
    conf.ifc_len = BUFSIZ;
    conf.ifc_buf = buff;

    int sock = CreateUDPSocket();
    ioctl(sock, SIOCGIFCONF, &conf);
    int maxnum  = conf.ifc_len / sizeof(struct ifreq);
    struct ifreq* ifr = conf.ifc_req;

    int cnt = 0;
    for(int i=0; cnt < num && i < maxnum;i++)
    {
        if (NULL == ifr)
            break;

        ioctl(sock, SIOCGIFFLAGS, ifr);

        if(((ifr->ifr_flags & IFF_LOOPBACK) == 0) && (ifr->ifr_flags & IFF_UP))
        {
            struct sockaddr_in *pAddr = (struct sockaddr_in *)(&ifr->ifr_addr);
            addrs[cnt ++] = pAddr->sin_addr.s_addr;
        }
        ++ ifr;
    }

    for ( ; cnt < num; ++ cnt)
    {
        addrs[cnt] = 0;
    }

    close(sock);
}


