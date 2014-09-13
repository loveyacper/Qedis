
#ifndef BERT_STREAMSOCKET_H
#define BERT_STREAMSOCKET_H

#include "Buffer.h"
#include "UnboundedBuffer.h"
#include "Socket.h"
#include "Threads/IPC.h"
#include <sys/types.h>
#include <sys/socket.h>

typedef int32_t  HEAD_LENGTH_T;
typedef int32_t  BODY_LENGTH_T;

// Abstraction for a TCP connection
class StreamSocket : public Socket
{
    friend class SendThread;
public:
    StreamSocket();
   ~StreamSocket();

    bool       Init(int localfd);
    SocketType GetSocketType() const { return SocketType_Stream; }

public:
    // Receive data
    int    Recv();
public:
    // Send data
    bool   SendPacket(const char* pData, int nBytes);
    bool   SendPacket(Buffer&  bf);
    bool   SendPacket(AttachedBuffer& abf);
    template <int N>
    bool   SendPacket(StackBuffer<N>&  sb);

    bool   OnReadable();
    bool   OnWritable();
    bool   OnError();

    bool  DoMsgParse(); // false if no msg

    int   RecvBufSize() const {  return  m_recvBuf.Capacity(); }
    void  SetRecvBufSize(int size) {  m_recvBuf.InitCapacity(size); }

    void  SetReconn(bool retry) { m_retry = retry; }
    bool  Timeout(time_t  now)  const    {  return   now > m_activeTime + m_timeout; }
    void  SetTimeout(int timeout = 1800) {  m_timeout = timeout; }
    
    bool  HasDataToSend() const { return !m_sendBuf.IsEmpty(); }

private:
    bool   m_retry;

    int    _Send(const char* data, int len);
    virtual HEAD_LENGTH_T _HandleHead(AttachedBuffer& buf, BODY_LENGTH_T* bodyLen) = 0;
    virtual void _HandlePacket(AttachedBuffer& buf) = 0;
    BODY_LENGTH_T m_bodyLen;

    time_t  m_activeTime;
    int     m_timeout;

private:
    SocketAddr  m_peerAddr;

    // For human readability
    enum
    {
        TIMEOUTSOCKET =  0,
        ERRORSOCKET   = -1,
        EOFSOCKET     = -2,
    };

    Buffer   m_recvBuf;

    Mutex    m_sendLock;
    UnboundedBuffer  m_sendBuf;
};

template <int N>
inline bool  StreamSocket::SendPacket(StackBuffer<N>& sf)
{
    return  SendPacket(sf.ReadAddr(), sf.ReadableSize());
}

#endif

