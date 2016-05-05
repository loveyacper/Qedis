
#ifndef BERT_SOCKET_H
#define BERT_SOCKET_H

#include <arpa/inet.h>
#include <string.h>
#include <memory>
#include <atomic>

#define INVALID_SOCKET  (int)(~0)
#define SOCKET_ERROR    (-1)
#define INVALID_PORT (-1)


struct SocketAddr
{
    SocketAddr()
    {
        Clear();
    }
    
    SocketAddr(const SocketAddr& other)
    {
        memcpy(&addr_, &other.addr_, sizeof addr_);
    }

    SocketAddr& operator= (const SocketAddr&  other)
    {
        if (this != &other)
            memcpy(&addr_, &other.addr_, sizeof addr_);

        return *this;
    }

    SocketAddr(const sockaddr_in& addr)
    {
        Init(addr);
    }

    SocketAddr(uint32_t  netip, uint16_t netport)
    {
        Init(netip, netport);
    }

    SocketAddr(const char* ip, uint16_t hostport)
    {
        Init(ip, hostport);
    }

    void Init(const sockaddr_in& addr)
    {
        memcpy(&addr_, &addr, sizeof(addr));
    }

    void Init(uint32_t  netip, uint16_t netport)
    {
        addr_.sin_family = AF_INET;       
        addr_.sin_addr.s_addr = netip;       
        addr_.sin_port   = netport;
    }

    void Init(const char* ip, uint16_t hostport)
    {
        addr_.sin_family = AF_INET;
        addr_.sin_addr.s_addr = ::inet_addr(ip);
        addr_.sin_port   = htons(hostport);
    }

    const sockaddr_in& GetAddr() const
    {
        return addr_;
    }

    const char* GetIP() const
    {
        return ::inet_ntoa(addr_.sin_addr);
    }
    
    const char* GetIP(char* buf, socklen_t size) const
    {
        return ::inet_ntop(AF_INET, (const char*)&addr_.sin_addr, buf, size);
    }

    unsigned short  GetPort() const
    {
        return ntohs(addr_.sin_port);
    }

    bool  Empty() const { return  0 == addr_.sin_family; }
    void  Clear()       { memset(&addr_, 0, sizeof addr_); }

    sockaddr_in  addr_;
    
    inline friend bool operator== (const SocketAddr& a, const SocketAddr& b)
    {
        return a.addr_.sin_family      ==  b.addr_.sin_family &&
               a.addr_.sin_addr.s_addr ==  b.addr_.sin_addr.s_addr &&
               a.addr_.sin_port        ==  b.addr_.sin_port ;
    }
    
    
    inline friend bool operator!= (const SocketAddr& a, const SocketAddr& b)
    {
        return  !(a == b);
    }
};


namespace Internal
{
class SendThread;
}

// Abstraction for a UDP/TCP socket
class Socket : public std::enable_shared_from_this<Socket>
{
    friend class Internal::SendThread;

public:
    virtual ~Socket();
    
    Socket(const Socket& ) = delete;
    void operator= (const Socket& ) = delete;

    enum SocketType
    {
        SocketType_Invalid= -1,
        SocketType_Listen,
        SocketType_Client,
        SocketType_Stream,
    };

    virtual SocketType GetSocketType() const { return SocketType_Invalid; }
    bool Invalid() const { return invalid_; }
    
    int  GetSocket() const {   return localSock_;   }
    std::size_t GetID() const     {   return id_;   }

    virtual bool OnReadable() { return false; }
    virtual bool OnWritable() { return false; }
    virtual bool OnError();
    virtual void OnConnect()  {  }

    static int CreateTCPSocket();
    static int CreateUDPSocket();
    static void CloseSocket(int &sock);
    static void SetNonBlock(int sock, bool nonBlock = true);
    static void SetNodelay(int sock);
    static void SetSndBuf(int sock, socklen_t size = 128 * 1024);
    static void SetRcvBuf(int sock, socklen_t size = 128 * 1024);
    static void SetReuseAddr(int sock);
    static bool GetLocalAddr(int sock, SocketAddr& );
    static bool GetPeerAddr(int sock,  SocketAddr& );
    static void GetMyAddrInfo(unsigned int* addrs, int num);

protected:
    Socket();

    // The local socket
    int    localSock_;
    bool   epollOut_;

private:
    std::atomic<bool> invalid_;
    std::size_t    id_;
    static std::atomic<std::size_t> sid_;
};


#endif

