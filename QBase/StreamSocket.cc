#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/tcp.h>

#include "StreamSocket.h"
#include "Server.h"
#include "NetThreadPool.h"
#include "Log/Logger.h"

using std::size_t;

StreamSocket::StreamSocket()
{
    m_retry   = false;
}

StreamSocket::~StreamSocket()
{
    INF << __FUNCTION__ << ": Try close tcp connection " << (m_localSock != INVALID_SOCKET ? m_localSock : -1);
}

bool StreamSocket::Init(int fd)
{
    if (fd < 0)
        return false;

    Socket::GetPeerAddr(fd, m_peerAddr);
    m_localSock = fd;
    SetNonBlock(m_localSock);
    USR << "Init new fd = " << m_localSock
        << ", peer addr = " << m_peerAddr.GetIP()
        << ", peer port = " << m_peerAddr.GetPort();
    
#if defined(__APPLE__)
    int set = 1;
    setsockopt(m_localSock, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
#endif

    return  true;
}

int StreamSocket::Recv()
{
    if (m_recvBuf.Capacity() == 0)
    {
        m_recvBuf.InitCapacity(64 * 1024); // First recv data, allocate buffer
        INF << "First expand recv buffer, capacity " << m_recvBuf.Capacity();
    }
    
    BufferSequence  buffers;
    m_recvBuf.GetSpace(buffers);
    if (buffers.count == 0)
    {
        WRN << "Recv buffer is full";
        return 0;
    }

    int ret = static_cast<int>(::readv(m_localSock, buffers.buffers, static_cast<int>(buffers.count)));
    if (ret == ERRORSOCKET && (EAGAIN == errno || EWOULDBLOCK == errno))
        return 0;

    if (ret > 0)
    {
        m_recvBuf.AdjustWritePtr(ret);
    }

    return (0 == ret) ? EOFSOCKET : ret;
}


int StreamSocket::_Send(const BufferSequence& bf)
{
    auto  total = bf.TotalBytes();
    if (total == 0)
        return 0;

    int ret = static_cast<int>(::writev(m_localSock, bf.buffers, static_cast<int>(bf.count)));
    if (ERRORSOCKET == ret && (EAGAIN == errno || EWOULDBLOCK == errno))
    {
        m_epollOut = true;
        ret = 0;
    }
    else if (ret > 0 && static_cast<size_t>(ret) < total)
    {
        m_epollOut = true;
    }
    else if (static_cast<size_t>(ret) == total)
    {
        m_epollOut = false;
    }

    return ret;
}


bool StreamSocket::SendPacket(const char* pData, size_t nBytes)
{
    if (pData && nBytes > 0)
        m_sendBuf.Write(pData, nBytes);

    return true;
}

bool  StreamSocket::SendPacket(Buffer& bf)
{
    return  SendPacket(bf.ReadAddr(), bf.ReadableSize());
}


bool  StreamSocket::SendPacket(AttachedBuffer& af)
{
    return  SendPacket(af.ReadAddr(), af.ReadableSize());
}


bool  StreamSocket::OnReadable()
{
    int nBytes = StreamSocket::Recv();
    if (nBytes < 0)
    {
        ERR << "socket " << m_localSock <<", OnReadable error, nBytes = " << nBytes << ", errno " << errno;
        Internal::NetThreadPool::Instance().DisableRead(shared_from_this());
        return false;
    }

    return true;
}

bool StreamSocket::Send()
{
    if (m_epollOut)
        return true;

    BufferSequence  bf;
    m_sendBuf.ProcessBuffer(bf);
    
    size_t  total = bf.TotalBytes();
    if (total == 0)  return true;
    
    int  nSent = _Send(bf);
    
    if (nSent > 0)
    {
        m_sendBuf.Skip(nSent);
    }
        
    if (m_epollOut)
    {
        Internal::NetThreadPool::Instance().EnableWrite(shared_from_this());
        INF << __FUNCTION__ << ": epoll out = true, socket = " << m_localSock;
    }
    
    return  nSent >= 0;
}

// drive by EPOLLOUT
bool StreamSocket::OnWritable()
{
    BufferSequence  bf;
    m_sendBuf.ProcessBuffer(bf);
    
    size_t  total = bf.TotalBytes();
    int     nSent = 0;
    if (total > 0)
    {
        nSent = _Send(bf);
        if (nSent > 0)
            m_sendBuf.Skip(nSent);
    }
    else
    {
        m_epollOut = false;
    }

    if (!m_epollOut)
    {  
        INF << __FUNCTION__ << ": epoll out = false, socket = " << m_localSock;
        Internal::NetThreadPool::Instance().DisableWrite(shared_from_this());
    }

    return  nSent >= 0;
}

bool StreamSocket::OnError()
{
    if (Socket::OnError())
    {
        ERR << "OnError stream socket " << m_localSock;

        if (m_retry)
            Server::Instance()->TCPReconnect(m_peerAddr);

        return true;
    }
        
    return false;
}

bool StreamSocket::DoMsgParse()
{
    bool busy = false;
    while (!m_recvBuf.IsEmpty())
    {
        BufferSequence  datum;
        m_recvBuf.GetDatum(datum, m_recvBuf.ReadableSize());

        AttachedBuffer af(datum);
        auto  bodyLen = _HandlePacket(af);
        if (bodyLen > 0)
        {
            busy = true;
            m_recvBuf.AdjustReadPtr(bodyLen);
        }
        else
        {
            break;
        }
    }

    return  busy;
}

