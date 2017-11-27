#ifndef BERT_PROXYCONFIG_H
#define BERT_PROXYCONFIG_H

#include <map>
#include <vector>
#include <string>

namespace qedis
{

struct ProxyConfig
{
    std::string bindAddr;
    std::string logDir;
    std::vector<std::string> clusters;

    static const std::string kProxyPrefixPath;
    static const std::string kQedisSetsPath;
    
    ProxyConfig();
};

extern ProxyConfig g_config;

extern bool LoadProxyConfig(const char* cfgFile, ProxyConfig& cfg);

}

#endif

