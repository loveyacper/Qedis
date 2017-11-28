#ifndef BERT_QEDISMANAGER_H
#define BERT_QEDISMANAGER_H

#include <unordered_map>
#include <vector>
#include <string>

#include "future/Future.h"


class QedisConn;

namespace ananas
{
    class EventLoop;
    class Connection;
    struct SocketAddr;
}

class QedisManager
{
public:
    static QedisManager& Instance();

    QedisManager();

    void SetEventLoop(ananas::EventLoop* loop);

    ananas::Future<QedisConn* > Connect(const std::string& addr);

    void OnNewConnection(ananas::Connection* conn);
    void OnConnect(ananas::Connection* conn);
    void OnDisconnect(ananas::Connection* conn);
    void OnConnFail(ananas::EventLoop* loop, const ananas::SocketAddr& peer);

    ananas::Future<QedisConn* > GetConnection(const std::string& peer);

private:
    ananas::EventLoop* loop_;
    std::unordered_map<std::string, ananas::Connection* > connMap_;

    struct Request
    {
        std::string peer;
        ananas::Promise<QedisConn* > promise;

        Request()
        {
        }

        Request(Request const& ) = delete;
        void operator= (Request const& ) = delete;

        Request(Request&& ) = default;
        Request& operator= (Request&& ) = default;
    };

    using ConnectPromise = ananas::Promise<QedisConn* >;
    std::unordered_map<std::string, ConnectPromise> pending_;
};

#endif

