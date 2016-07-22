
#ifndef BERT_STREAMSOCKET_H
#define BERT_STREAMSOCKET_H

#include "AsyncBuffer.h"
#include "Socket.h"
#include <sys/types.h>
#include <sys/socket.h>

using PacketLength = int32_t;

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
    bool   SendPacket(qedis::UnboundedBuffer& ubf);
    template <int N>
    bool   SendPacket(StackBuffer<N>&  sb);

    bool   OnReadable();
    bool   OnWritable();
    bool   OnError();

    bool  DoMsgParse(); // false if no msg

    void  SetOnDisconnect(const std::function<void ()>& cb = std::function<void ()>()) { onDisconnect_ = cb; }
    
    // send thread
    bool  Send();
    
    const SocketAddr& GetPeerAddr() const { return peerAddr_; }

protected:
    SocketAddr  peerAddr_;

private:
    std::function<void ()> onDisconnect_;

    int    _Send(const BufferSequence& bf);
    virtual PacketLength _HandlePacket(AttachedBuffer& buf) = 0;

    // For human readability
    enum
    {
        TIMEOUTSOCKET =  0,
        ERRORSOCKET   = -1,
        EOFSOCKET     = -2,
    };

    Buffer recvBuf_;
    AsyncBuffer sendBuf_;
};

template <int N>
inline bool  StreamSocket::SendPacket(StackBuffer<N>& sf)
{
    return  SendPacket(sf.ReadAddr(), sf.ReadableSize());
}

#endif

