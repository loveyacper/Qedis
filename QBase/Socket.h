
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
        memcpy(&m_addr, &other.m_addr, sizeof m_addr);
    }

    SocketAddr& operator= (const SocketAddr&  other)
    {
        if (this != &other)
            memcpy(&m_addr, &other.m_addr, sizeof m_addr);

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
        memcpy(&m_addr, &addr, sizeof(addr));
    }

    void Init(uint32_t  netip, uint16_t netport)
    {
        m_addr.sin_family = AF_INET;       
        m_addr.sin_addr.s_addr = netip;       
        m_addr.sin_port   = netport;
    }

    void Init(const char* ip, uint16_t hostport)
    {
        m_addr.sin_family = AF_INET;
        m_addr.sin_addr.s_addr = ::inet_addr(ip);
        m_addr.sin_port   = htons(hostport);
    }

    const sockaddr_in& GetAddr() const
    {
        return m_addr;
    }

    const char* GetIP() const
    {
        return ::inet_ntoa(m_addr.sin_addr);
    }

    unsigned short  GetPort() const
    {
        return ntohs(m_addr.sin_port);
    }

    bool  Empty() const { return  0 == m_addr.sin_family; }
    void  Clear()       { memset(&m_addr, 0, sizeof m_addr); }

    sockaddr_in  m_addr;
    
    inline friend bool operator== (const SocketAddr& a, const SocketAddr& b)
    {
        return a.m_addr.sin_family      ==  b.m_addr.sin_family &&
               a.m_addr.sin_addr.s_addr ==  b.m_addr.sin_addr.s_addr &&
               a.m_addr.sin_port        ==  b.m_addr.sin_port ;
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

    enum SocketType
    {
        SocketType_Invalid= -1,
        SocketType_Listen,
        SocketType_Client,
        SocketType_Stream,
    };

    virtual SocketType GetSocketType() const { return SocketType_Invalid; }
    bool Invalid() const { return m_invalid; }
    
    int  GetSocket() const {   return m_localSock;   }
    std::size_t GetID() const     {   return m_id;   }

    virtual bool OnReadable() { return false; }
    virtual bool OnWritable() { return false; }
    virtual bool OnError();

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
    int    m_localSock;
    bool   m_epollOut;

private:
    std::atomic<bool> m_invalid;
    std::size_t    m_id;
    static std::atomic<std::size_t> sm_id;
};


#endif

