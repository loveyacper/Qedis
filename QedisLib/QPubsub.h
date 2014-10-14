#ifndef BERT_QPUBSUB_H
#define BERT_QPUBSUB_H

#include <map>
#include <set>
#include <vector>
#include <string>
#include "SmartPtr/WeakPtr.h"

class QClient;
class QPubsub
{
public:
    static QPubsub& Instance();

    std::size_t Subscribe(QClient* client, const std::string& channel);
    std::size_t UnSubscribe(QClient* client, const std::string& channel);
    std::size_t UnSubscribeAll(QClient* client);
    std::size_t PublishMsg(const std::string& channel, const std::string& msg);

    std::size_t PSubscribe(QClient* client, const std::string& pchannel);
    std::size_t PUnSubscribeAll(QClient* client);
    std::size_t PUnSubscribe(QClient* client, const std::string& pchannel);
    
    // introspect
    void  PubsubChannels(std::vector<std::string>& res, const char* pattern = 0) const;
    std::size_t PubsubNumsub(const std::string& channel) const;
    std::size_t PubsubNumpat() const;

    void InitPubsubTimer();
    void RecycleClients(std::string& startChannel, std::string& startPattern);

private:
    QPubsub() {}

    typedef std::set<WeakPtr<QClient> >   Clients;
    typedef std::map<std::string, Clients>  ChannelClients;

    ChannelClients   m_channels;
    ChannelClients   m_patternChannels;

    static void _RecycleClients(ChannelClients& channels, std::string& start);
};

#endif

