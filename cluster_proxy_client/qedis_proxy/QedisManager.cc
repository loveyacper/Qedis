#include "QedisManager.h"
#include "QedisConn.h"
#include "ProxyLog.h"

#include "net/EventLoop.h"
#include "net/Connection.h"
#include "net/Socket.h"

QedisManager& QedisManager::Instance()
{
    static QedisManager mgr;
    return mgr;
}

QedisManager::QedisManager() :
    loop_(nullptr)
{
}

void QedisManager::SetEventLoop(ananas::EventLoop* loop)
{
    assert (!loop_);
    loop_ = loop;
}

ananas::Future<QedisConn* > QedisManager::Connect(const std::string& addr)
{
    ConnectPromise promise;
    auto fut = promise.GetFuture();
    auto& promiseList = pending_[addr];
    promiseList.emplace_back(std::move(promise));

    if (promiseList.size() == 1)
    {
        bool succ = loop_->Connect(ananas::SocketAddr(addr),
                                   std::bind(&QedisManager::OnNewConnection, this, std::placeholders::_1),
                                   std::bind(&QedisManager::OnConnFail, this, std::placeholders::_1, std::placeholders::_2), 
                                   ananas::DurationMs(1000));
        if (!succ)
        {
            pending_.erase(addr);
            return ananas::MakeExceptionFuture<QedisConn* >(std::runtime_error("connect " + addr + " failed"));
        }
    }

    return fut;
}

void QedisManager::OnNewConnection(ananas::Connection* conn)
{
    INF(g_logger) << "QedisManager::OnNewConnection " << conn->Identifier();

    auto ctx = std::make_shared<QedisConn>(conn);

    conn->SetUserData(ctx);
    conn->SetMinPacketSize(4);
    conn->SetOnMessage(std::bind(&QedisConn::OnRecv,
                                 ctx.get(),
                                 std::placeholders::_1,
                                 std::placeholders::_2,
                                 std::placeholders::_3));
    conn->SetOnConnect(std::bind(&QedisManager::OnConnect, this, std::placeholders::_1));
    conn->SetOnDisconnect(std::bind(&QedisManager::OnDisconnect, this, std::placeholders::_1));
}

void QedisManager::OnConnect(ananas::Connection* conn)
{
    INF(g_logger) << "QedisManager::OnConnect " << conn->Peer().ToString();
    auto ctx = conn->GetUserData<QedisConn>();

    auto req = pending_.find(conn->Peer().ToString());
    assert (req != pending_.end());

    bool succ = connMap_.insert({conn->Peer().ToString(), conn}).second;
    assert (succ);

    for (auto& prom : req->second)
        prom.SetValue(ctx.get());

    pending_.erase(req);
}

void QedisManager::OnDisconnect(ananas::Connection* conn)
{
    INF(g_logger) << "QedisManager::OnDisconnect " << conn->Identifier();
    size_t n = connMap_.erase(conn->Peer().ToString());
    assert (n > 0);
}

void QedisManager::OnConnFail(ananas::EventLoop* loop, const ananas::SocketAddr& peer)
{
    INF(g_logger) << "QedisManager::OnConnFail " << peer.ToString();

    auto req = pending_.find(peer.ToString());
    if (req != pending_.end())
    {
        for (auto& prom : req->second)
            prom.SetException(std::make_exception_ptr(std::runtime_error("Failed connect to " + peer.ToString())));

        pending_.erase(req);
    }
}

ananas::Future<QedisConn* > QedisManager::GetConnection(const std::string& peer)
{
    auto it = connMap_.find(peer);
    if (it != connMap_.end())
        return ananas::MakeReadyFuture<QedisConn* >(it->second->GetUserData<QedisConn>().get());

    return this->Connect(peer);
}

