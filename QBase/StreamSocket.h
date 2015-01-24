
#ifndef BERT_STREAMSOCKET_H
#define BERT_STREAMSOCKET_H

#include "OutputBuffer.h"
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
    bool   SendPacket(const char* pData, std::size_t nBytes);
    bool   SendPacket(Buffer&  bf);
    bool   SendPacket(AttachedBuffer& abf);
    template <int N>
    bool   SendPacket(StackBuffer<N>&  sb);

    bool   OnReadable();
    bool   OnWritable();
    bool   OnError();

    bool  DoMsgParse(); // false if no msg

    void  SetReconn(bool retry) { m_retry = retry; }
    
    // send thread
    bool  Send();

private:
    bool   m_retry;

    int    _Send(const BufferSequence& bf);
    virtual HEAD_LENGTH_T _HandleHead(AttachedBuffer& buf, BODY_LENGTH_T* bodyLen) = 0;
    virtual void _HandlePacket(AttachedBuffer& buf) = 0;
    BODY_LENGTH_T m_bodyLen;

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
    OutputBuffer   m_sendBuf;
};

template <int N>
inline bool  StreamSocket::SendPacket(StackBuffer<N>& sf)
{
    return  SendPacket(sf.ReadAddr(), sf.ReadableSize());
}

#endif

