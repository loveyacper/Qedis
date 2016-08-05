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
}

StreamSocket::~StreamSocket()
{
    INF << __FUNCTION__ << " peer ("
        << peerAddr_.GetIP()
        << ":"
        << peerAddr_.GetPort()
        << ")";
}

bool StreamSocket::Init(int fd)
{
    if (fd < 0)
        return false;

    Socket::GetPeerAddr(fd, peerAddr_);
    localSock_ = fd;
    SetNonBlock(localSock_);

    INF << __FUNCTION__ << " peer address ("
        << peerAddr_.GetIP()
        << ":"
        << peerAddr_.GetPort()
        << "), local socket is " << fd;
    
#if defined(__APPLE__)
    int set = 1;
    setsockopt(localSock_, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
#endif

    return  true;
}

int StreamSocket::Recv()
{
    if (recvBuf_.Capacity() == 0)
    {
        recvBuf_.InitCapacity(64 * 1024); // First recv data, allocate buffer
    }
    
    BufferSequence  buffers;
    recvBuf_.GetSpace(buffers);
    if (buffers.count == 0)
    {
        WRN << "Recv buffer is full";
        return 0;
    }

    int ret = static_cast<int>(::readv(localSock_, buffers.buffers, static_cast<int>(buffers.count)));
    if (ret == ERRORSOCKET && (EAGAIN == errno || EWOULDBLOCK == errno))
        return 0;

    if (ret > 0)
        recvBuf_.AdjustWritePtr(ret);

    return (0 == ret) ? EOFSOCKET : ret;
}


int StreamSocket::_Send(const BufferSequence& bf)
{
    auto  total = bf.TotalBytes();
    if (total == 0)
        return 0;

    int ret = static_cast<int>(::writev(localSock_, bf.buffers, static_cast<int>(bf.count)));
    if (ERRORSOCKET == ret && (EAGAIN == errno || EWOULDBLOCK == errno))
    {
        epollOut_ = true;
        ret = 0;
    }
    else if (ret > 0 && static_cast<size_t>(ret) < total)
    {
        epollOut_ = true;
    }
    else if (static_cast<size_t>(ret) == total)
    {
        epollOut_ = false;
    }

    return ret;
}


bool StreamSocket::SendPacket(const char* pData, size_t nBytes)
{
    if (pData && nBytes > 0)
        sendBuf_.Write(pData, nBytes);

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

bool  StreamSocket::SendPacket(qedis::UnboundedBuffer& ubf)
{
    return  SendPacket(ubf.ReadAddr(), ubf.ReadableSize());
}

bool  StreamSocket::OnReadable()
{
    int nBytes = StreamSocket::Recv();
    if (nBytes < 0)
    {
        INF << __FUNCTION__ << " failed, peer address ("
            << peerAddr_.GetIP()
            << ":"
            << peerAddr_.GetPort()
            << "), local socket is "
            << localSock_;
        
        Internal::NetThreadPool::Instance().DisableRead(shared_from_this());
        return false;
    }

    return true;
}

bool StreamSocket::Send()
{
    if (epollOut_)
        return true;

    BufferSequence  bf;
    sendBuf_.ProcessBuffer(bf);
    
    size_t  total = bf.TotalBytes();
    if (total == 0)  return true;
    
    int  nSent = _Send(bf);
    
    if (nSent > 0)
    {
        sendBuf_.Skip(nSent);
    }
        
    if (epollOut_)
    {
        Internal::NetThreadPool::Instance().EnableWrite(shared_from_this());
        INF << __FUNCTION__ << " peer address ("
            << peerAddr_.GetIP()
            << ":"
            << peerAddr_.GetPort()
            << "), local socket is "
            << localSock_
            << ", register write event";
    }
    
    return  nSent >= 0;
}

// drive by EPOLLOUT
bool StreamSocket::OnWritable()
{
    BufferSequence  bf;
    sendBuf_.ProcessBuffer(bf);
    
    size_t  total = bf.TotalBytes();
    int     nSent = 0;
    if (total > 0)
    {
        nSent = _Send(bf);
        if (nSent > 0)
            sendBuf_.Skip(nSent);
    }
    else
    {
        epollOut_ = false;
    }

    if (!epollOut_)
    {
        INF << __FUNCTION__ << " peer address ("
            << peerAddr_.GetIP()
            << ":"
            << peerAddr_.GetPort()
            << "), local socket is "
            << localSock_
            << ", unregister write event";
        Internal::NetThreadPool::Instance().DisableWrite(shared_from_this());
    }

    return  nSent >= 0;
}

bool StreamSocket::OnError()
{
    if (Socket::OnError())
    {
        ERR << __FUNCTION__ << " peer address ("
            << peerAddr_.GetIP()
            << ":"
            << peerAddr_.GetPort()
            << "), local socket is "
            << localSock_;

        if (onDisconnect_)
            onDisconnect_();

        return true;
    }
        
    return false;
}

bool StreamSocket::DoMsgParse()
{
    bool busy = false;
    while (!recvBuf_.IsEmpty())
    {
        BufferSequence  datum;
        recvBuf_.GetDatum(datum, recvBuf_.ReadableSize());

        AttachedBuffer af(datum);
        auto  bodyLen = _HandlePacket(af.ReadAddr(), af.ReadableSize());
        if (bodyLen > 0)
        {
            busy = true;
            recvBuf_.AdjustReadPtr(bodyLen);
        }
        else
        {
            break;
        }
    }

    return  busy;
}

