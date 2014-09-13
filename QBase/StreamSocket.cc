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

StreamSocket::StreamSocket()
{
    m_bodyLen = 0;
    m_retry   = false;
    m_activeTime = g_now.MilliSeconds() / 1000;
    SetTimeout();
}

StreamSocket::~StreamSocket()
{
    LOCK_SDK_LOG; 
    INF << "Try close tcp connection " << (m_localSock != INVALID_SOCKET ? m_localSock : 0);
    UNLOCK_SDK_LOG; 
}

bool StreamSocket::Init(int fd)
{
    if (fd < 0)
        return false;

    Socket::GetPeerAddr(fd, m_peerAddr);
    m_localSock = fd;
    SetNonBlock(m_localSock);
    LOCK_SDK_LOG; 
    USR << "New task, fd = " << m_localSock
        << ", peer addr = " << m_peerAddr.GetIP()
        << ", peer port = " << m_peerAddr.GetPort();
    UNLOCK_SDK_LOG; 

    return  true;
}

int StreamSocket::Recv()
{
    if (0 == RecvBufSize())
    {
        SetRecvBufSize(DEFAULT_BUFFER_SIZE); // First recv data, allocate buffer
    LOCK_SDK_LOG; 
        INF << "EXPAND recv buffer, capacity " << RecvBufSize();
    UNLOCK_SDK_LOG; 
    }
    
    BufferSequence  buffers;
    m_recvBuf.GetSpace(buffers);
    if (buffers.count == 0)
    {
    LOCK_SDK_LOG; 
        ERR << "recv buffer is full";
    UNLOCK_SDK_LOG; 
        return 0;
    }

    int ret = static_cast<int>(::readv(m_localSock, buffers.buffers, buffers.count));
    if (ret == ERRORSOCKET && (EAGAIN == errno || EWOULDBLOCK == errno))
        return 0;

    if (ret > 0)
    {
        m_recvBuf.AdjustWritePtr(ret);
    }

    return (0 == ret) ? EOFSOCKET : ret;
}


int StreamSocket::_Send(const char* data, int len)
{
    assert (len >= 0);

    if (!data || len == 0)
        return 0;

    int ret = static_cast<int>(::send(m_localSock, data, len, 0));
    if (ERRORSOCKET == ret && (EAGAIN == errno || EWOULDBLOCK == errno))
    {
        ret = 0;
    }

    return ret;
}

bool StreamSocket::SendPacket(const char* pData, int nBytes)
{
    if (!pData || nBytes <= 0)
        return true;

    if (!HasDataToSend())
    {
        int nSent = _Send(pData, nBytes);
        if (nSent < 0)
        {
            this->OnError();
            return false;
        }

        if (nSent < nBytes)
        {
            ScopeMutex guard(m_sendLock);
            m_sendBuf.PushData(pData + nSent, nBytes - nSent);
       
            Internal::NetThreadPool::Instance().EnableWrite(ShareMe());
        }
    }
    else
    {
        ScopeMutex guard(m_sendLock);
        m_sendBuf.PushData(pData, nBytes);
        
        Internal::NetThreadPool::Instance().EnableWrite(ShareMe()); // 让sender自己记录epollout数量
    }

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
    LOCK_SDK_LOG; 
        ERR << "socket " << m_localSock <<", OnRead error , nBytes = " << nBytes << ", errno " << errno;
    UNLOCK_SDK_LOG; 
        Internal::NetThreadPool::Instance().DisableRead(ShareMe());
        return false;
    }

    return true;
}

// drive by EPOLLOUT
bool StreamSocket::OnWritable()
{
    ScopeMutex  guard(m_sendLock);

    int total =  m_sendBuf.ReadableSize();
    int nSent = _Send(m_sendBuf.ReadAddr(), total);
   
    if (nSent < 0)
    {
        m_sendBuf.Clear();
        return false;
    }
    else if (nSent > 0)
    {
        m_sendBuf.AdjustReadPtr(nSent); // consumebytes ,  producebytes
    }

    if (nSent == total)
    {
        assert (m_sendBuf.IsEmpty());
        Internal::NetThreadPool::Instance().DisableWrite(ShareMe());
    }

    return true;
}

bool StreamSocket::OnError()
{
    if (Socket::OnError())
    {
    LOCK_SDK_LOG; 
        ERR << "OnError stream socket " << m_localSock;
    UNLOCK_SDK_LOG; 

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

        if (0 == m_bodyLen)
        {
            const HEAD_LENGTH_T headLen = _HandleHead(af, &m_bodyLen);
            if (headLen < 0 || headLen + 1 >= m_recvBuf.Capacity())
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
        if (m_bodyLen < 0 ||
            3 * m_bodyLen > 2 * m_recvBuf.Capacity())
        {
    LOCK_SDK_LOG; 
            ERR << "Too big packet " << m_bodyLen << " on socket " << m_localSock;
    UNLOCK_SDK_LOG; 
            OnError();
            return  false;
        }
        
        if (0 == m_bodyLen ||
            m_recvBuf.ReadableSize() < m_bodyLen)
        {
            return busy;
        }

        busy = true;

        BufferSequence   bf;
        m_recvBuf.GetDatum(bf, m_bodyLen); 
        assert (m_bodyLen == bf.TotalBytes());

        m_activeTime = g_now.MilliSeconds() / 1000;
        AttachedBuffer   cmd(bf);
        _HandlePacket(cmd);

        m_recvBuf.AdjustReadPtr(m_bodyLen);
        m_bodyLen = 0;
    }

    return  busy;
}

