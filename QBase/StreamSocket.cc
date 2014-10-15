#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/tcp.h>

#include "StreamSocket.h"
#include "Server.h"
#include "Timer.h"
#include "NetThreadPool.h"
#include "Log/Logger.h"

using std::size_t;

StreamSocket::StreamSocket()
{
    m_bodyLen = -1;
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

    return  true;
}

int StreamSocket::Recv()
{
    if (m_recvBuf.Capacity() == 0)
    {
        m_recvBuf.InitCapacity(DEFAULT_BUFFER_SIZE); // First recv data, allocate buffer
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
    size_t  total = bf.TotalBytes();
    if (total == 0)
        return 0;

    int ret = static_cast<int>(::writev(m_localSock, bf.buffers, bf.count));
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


// main and timer call it
bool  StreamSocket::MoveToSendBuffer()
{
    if (m_backBuf.IsEmpty())
        return true;
    
    size_t  avail = m_sendBuf.WritableSize();
    
    WRN << __FUNCTION__ << " socket " << m_localSock <<", move bytes " << (std::min(avail, m_backBuf.ReadableSize()));

    if (avail < m_backBuf.ReadableSize())
    {
        m_sendBuf.PushData(m_backBuf.ReadAddr(), avail);
        m_backBuf.AdjustReadPtr(avail);
    }
    else
    {
        m_sendBuf.PushData(m_backBuf.ReadAddr(), m_backBuf.ReadableSize());
        m_backBuf.Clear();
    }
    
    if (!m_backBuf.IsEmpty())
    {
        WRN << __FUNCTION__ << " socket " << m_localSock <<", has back bytes " << m_backBuf.ReadableSize();
    }
    return   m_backBuf.IsEmpty();
}

bool StreamSocket::SendPacket(const char* pData, size_t nBytes)
{
    if (!pData || nBytes == 0)
        return true;
    
    if (m_sendBuf.Capacity() == 0)
    {
        m_sendBuf.InitCapacity(DEFAULT_BUFFER_SIZE);
    }
    
    if (!MoveToSendBuffer() || !m_sendBuf.PushData(pData, nBytes))
    {
        m_backBuf.PushData(pData, nBytes);
        if (!m_timer)
        {
            m_timer.Reset(new SendTimer());
            m_timer->m_sock.Reset(StaticPointerCast<StreamSocket>(ShareMe()));
            INF << __FUNCTION__ << " socket " << m_localSock << " create send timer";
        }
        TimerManager::Instance().AddTimer(m_timer);
    }

    return true;
}

StreamSocket::SendTimer::SendTimer() : Timer(50)
{
}

bool StreamSocket::SendTimer::_OnTimer()
{
    SharedPtr<StreamSocket>  sock = m_sock.Lock();
    
    if (sock)
    {
        WRN << "socket " << sock->GetSocket() << ", OnSendTimer";
    }
    
    return  sock && !sock->MoveToSendBuffer();
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
        Internal::NetThreadPool::Instance().DisableRead(ShareMe());
        return false;
    }

    return true;
}

bool StreamSocket::Send()
{
    if (m_epollOut)
        return true;

    BufferSequence  bf;
    m_sendBuf.GetDatum(bf);
    
    size_t  total = bf.TotalBytes();
    if (total == 0)  return true;
    
    int  nSent = _Send(bf);
    
    if (nSent > 0)
    {
        m_sendBuf.AdjustReadPtr(nSent);
    }
        
    if (m_epollOut)
    {
        assert (!m_sendBuf.IsEmpty());
        Internal::NetThreadPool::Instance().EnableWrite(ShareMe());
        INF << __FUNCTION__ << ": epoll out = true, socket = " << m_localSock;
    }
    
    return  nSent >= 0;
}

// drive by EPOLLOUT
bool StreamSocket::OnWritable()
{
    BufferSequence  bf;
    m_sendBuf.GetDatum(bf);
    
    size_t  total = bf.TotalBytes();
    int     nSent = 0;
    if (total > 0)
    {
        nSent = _Send(bf);
        if (nSent > 0)
            m_sendBuf.AdjustReadPtr(nSent);
    }
    else
    {
        m_epollOut = false;
    }

    if (!m_epollOut)
    {  
        INF << __FUNCTION__ << ": epoll out = false, socket = " << m_localSock;
        assert (m_sendBuf.Capacity() == 0 || m_sendBuf.IsEmpty());
        Internal::NetThreadPool::Instance().DisableWrite(ShareMe());
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
        m_recvBuf.GetDatum(datum);

        AttachedBuffer af(datum);

        if (m_bodyLen == -1)
        {
            const HEAD_LENGTH_T headLen = _HandleHead(af, &m_bodyLen);
            if (headLen < 0 || static_cast<size_t>(headLen + 1) >= m_recvBuf.Capacity())
            {
                OnError();
                return false;
            }
            else
            {
                m_recvBuf.AdjustReadPtr(headLen);
            }
        }

        // tmp some defensive code
        if (m_bodyLen >= 0 &&
            static_cast<size_t>(3 * m_bodyLen) > 2 * m_recvBuf.Capacity())
        {
            ERR << "Too big packet " << m_bodyLen << " on socket " << m_localSock;
            OnError();
            return  false;
        }
        
        if (m_bodyLen == -1 ||
            static_cast<int>(m_recvBuf.ReadableSize()) < m_bodyLen)
        {
            return busy;
        }

        busy = true;

        BufferSequence   bf;
        m_recvBuf.GetDatum(bf, m_bodyLen); 
        assert (m_bodyLen == static_cast<int>(bf.TotalBytes()));

        AttachedBuffer   cmd(bf);
        _HandlePacket(cmd);

        m_recvBuf.AdjustReadPtr(m_bodyLen);
        m_bodyLen = -1;
    }

    return  busy;
}

