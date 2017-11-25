#ifndef BERT_SERVERMANAGER_H
#define BERT_SERVERMANAGER_H

#include <map>
#include <vector>
#include <string>
#include "net/Socket.h"

class ServerManager
{
public:
    static ServerManager& Instance();

    void AddShardingInfo(int setid, const std::vector<std::string>& shardings);

    void AddServerInfo(int setid, const std::string& host);

private:
    ServerManager() {}

    // sharding-id -> set-id
    std::map<int, int> shardingInfo_;

    // set-id -> server-list
    std::map<int, std::vector<ananas::SocketAddr>> hostInfo_;
};

#endif

