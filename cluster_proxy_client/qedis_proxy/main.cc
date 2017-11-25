#include <unistd.h>
#include <iostream>
#include <assert.h>
#include <sys/time.h>
#include "ananas/net/Connection.h"
#include "ananas/net/EventLoop.h"
#include "ananas/net/log/Logger.h"

#define USE_ZOOKEEPER 1

#if USE_ZOOKEEPER
    #include "ZookeeperProxyConn.h"
#endif

std::shared_ptr<ananas::Logger> logger;

using namespace qedis;

void OnConnect(ananas::Connection* conn)
{
    INF(logger) << "OnConnect " << conn->Identifier();

    auto ctx = conn->GetUserData<ClusterConn>();
    ctx->OnConnect();
}
            
void OnNewConnection(ananas::Connection* conn)
{
    std::cout << "OnNewConnection " << conn->Identifier() << std::endl;

    using namespace ananas;
#if USE_ZOOKEEPER
    auto ctx = std::make_shared<ZookeeperProxyConn>(conn);
#else
#error "Please define USE_ZOOKEEPER to non-zero"
#endif
    conn->SetUserData(ctx);
    conn->SetMinPacketSize(4);
    conn->SetOnMessage([ctx](Connection* c, const char* data, PacketLen_t len) -> PacketLen_t {
         const char* ptr = data;
         if (!ctx->OnData(ptr, len)) {
             c->ActiveClose();
         }

         return static_cast<PacketLen_t>(ptr - data);
    });
    conn->SetOnConnect(OnConnect); // must be last
}
    
void OnConnFail(ananas::EventLoop* loop, const ananas::SocketAddr& peer)
{
    INF(logger) << "OnConnFail " << peer.ToString();

    loop->Sleep(std::chrono::seconds(2)).Then([=]() {
        loop->Connect(peer, OnNewConnection, OnConnFail, ananas::DurationMs(1000));
    });
}


int main(int ac, char* av[])
{
    ananas::LogManager::Instance().Start();
    logger = ananas::LogManager::Instance().CreateLog(logALL, logALL, "logger_qedisproxy");

    const uint16_t port = 2181;

    ananas::EventLoop loop;
    loop.Connect("localhost", port, OnNewConnection, OnConnFail, ananas::DurationMs(1000));

    loop.Run();
    return 0;
}

