#include "ServerManager.h"
#include <iostream>

ServerManager& ServerManager::Instance()
{
    static ServerManager mgr;
    return mgr;
}

void ServerManager::AddShardingInfo(int setid, const std::vector<std::string>& shardings)
{
    for (const auto& shard : shardings)
    {
        int id = std::stoi(shard);
        bool succ = shardingInfo_.insert({id, setid}).second;
        if (succ)
            std::cout << "SUCC: Insert sharding info " << id << ":" << setid << std::endl;
        else
            std::cout << "FAILED: Insert sharding info " << id << ":" << setid << std::endl;
    }
}

void ServerManager::AddServerInfo(int setid, const std::string& host)
{
    auto& svrlist = hostInfo_[setid];
    svrlist.push_back(ananas::SocketAddr(host));
    std::cout << "AddServerInfo : " << setid << ":" << host << std::endl;
}

