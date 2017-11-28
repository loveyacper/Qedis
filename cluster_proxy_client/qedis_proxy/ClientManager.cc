#include "ClientManager.h"
#include "ClientConn.h"
#include "ProxyLog.h"

#include "net/EventLoop.h"
#include "net/Connection.h"
#include "net/Socket.h"

ClientManager& ClientManager::Instance()
{
    static ClientManager mgr;
    return mgr;
}

ClientManager::ClientManager() :
    loop_(nullptr),
    listening_(false)
{
}

void ClientManager::SetEventLoop(ananas::EventLoop* loop)
{
    assert (!loop_);
    loop_ = loop;
}

bool ClientManager::Listen(const std::string& addr)
{
    assert (!listening_);
    listening_ = loop_->Listen(ananas::SocketAddr(addr),
                               std::bind(&ClientManager::OnNewConnection, this, std::placeholders::_1));
    return listening_;
}

void ClientManager::OnNewConnection(ananas::Connection* conn)
{
    INF(g_logger) << "ClientManager::OnNewConnection " << conn->Identifier();

    auto ctx = std::make_shared<ClientConn>(conn);

    using ananas::Connection;

    conn->SetUserData(ctx);
    conn->SetMinPacketSize(4);
    conn->SetOnMessage(std::bind(&ClientConn::OnRecv,
                                 ctx.get(),
                                 std::placeholders::_1,
                                 std::placeholders::_2,
                                 std::placeholders::_3));
    conn->SetOnConnect(std::bind(&ClientManager::OnConnect, this, std::placeholders::_1));
    conn->SetOnDisconnect(std::bind(&ClientManager::OnDisconnect, this, std::placeholders::_1));
}

void ClientManager::OnConnect(ananas::Connection* conn)
{
    INF(g_logger) << "ClientManager::OnConnect " << conn->Identifier();
}

void ClientManager::OnDisconnect(ananas::Connection* conn)
{
    INF(g_logger) << "ClientManager::OnDisconnect " << conn->Identifier();
}

