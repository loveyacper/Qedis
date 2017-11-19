#ifndef BERT_QPUBSUB_H
#define BERT_QPUBSUB_H

#include <map>
#include <set>
#include <vector>
#include "QString.h"
#include <memory>

namespace qedis
{

class QClient;
class QPubsub
{
public:
    static QPubsub& Instance();
    
    QPubsub(const QPubsub& ) = delete;
    void operator= (const QPubsub& ) = delete;

    std::size_t Subscribe(QClient* client, const QString& channel);
    std::size_t UnSubscribe(QClient* client, const QString& channel);
    std::size_t UnSubscribeAll(QClient* client);
    std::size_t PublishMsg(const QString& channel, const QString& msg);

    std::size_t PSubscribe(QClient* client, const QString& pchannel);
    std::size_t PUnSubscribeAll(QClient* client);
    std::size_t PUnSubscribe(QClient* client, const QString& pchannel);
    
    // introspect
    void  PubsubChannels(std::vector<QString>& res, const char* pattern = 0) const;
    std::size_t PubsubNumsub(const QString& channel) const;
    std::size_t PubsubNumpat() const;

    void InitPubsubTimer();
    void RecycleClients(QString& startChannel, QString& startPattern);

private:
    QPubsub() {}

    using Clients = std::set<std::weak_ptr<QClient>, std::owner_less<std::weak_ptr<QClient> > >;
    using ChannelClients = std::map<QString, Clients>;

    ChannelClients   channels_;
    ChannelClients   patternChannels_;
    
    QString  startChannel_;
    QString  startPattern_;
    static void _RecycleClients(ChannelClients& channels, QString& start);
    
    size_t _Publish(QPubsub::Clients& clients, const std::vector<QString>& args);
};
    
}

#endif

