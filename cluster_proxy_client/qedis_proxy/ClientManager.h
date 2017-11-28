#ifndef BERT_CLIENTMANAGER_H
#define BERT_CLIENTMANAGER_H

#include <unordered_map>
#include <vector>
#include <string>

namespace ananas
{
    class EventLoop;
    class Connection;
}

class ClientManager
{
public:
    static ClientManager& Instance();

    ClientManager();

    void SetEventLoop(ananas::EventLoop* loop);
    bool Listen(const std::string& addr);

    void OnNewConnection(ananas::Connection* conn);
    void OnConnect(ananas::Connection* conn);
    void OnDisconnect(ananas::Connection* conn);

private:
    //std::unordered_map<int, ananas::Connection* > connMap_;

    ananas::EventLoop* loop_;
    bool listening_;
};

#endif

