#include <iostream>

#include "ProxyLog.h"
#include "ProxyConfig.h"
#include "ananas/net/EventLoop.h"
#include "ClusterManager.h"
#include "ClientManager.h"

std::shared_ptr<ananas::Logger> g_logger;

int main(int ac, char* av[])
{
    using namespace qedis;

    if (ac > 1)
    {
        if (!LoadProxyConfig(av[1], g_config))
        {
            std::cout << "LoadProxyConfig " << av[1] << " failed!\n";
            return -1;
        }
    }

    ananas::LogManager::Instance().Start();
    g_logger = ananas::LogManager::Instance().CreateLog(logALL, logALL, g_config.logDir.data());

    for (const auto& addr : g_config.clusters)
        ClusterManager::Instance().AddClusterAddr(addr);

    ananas::EventLoop loop;
    ClusterManager::Instance().SetEventLoop(&loop);
    if (!ClusterManager::Instance().Connect())
        return -1;

    ClientManager::Instance().SetEventLoop(&loop);
    if (!ClientManager::Instance().Listen(g_config.bindAddr))
        return -2;

    loop.Run();

    return 0;
}

