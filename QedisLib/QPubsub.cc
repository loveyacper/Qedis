#include "QPubsub.h"
#include "QClient.h"
#include "Log/Logger.h"
#include "Timer.h"
#include "QGlobRegex.h"

namespace qedis
{

QPubsub&    QPubsub::Instance()
{
    static QPubsub  ps;
    return ps;
}
    
size_t QPubsub::Subscribe(QClient* client, const QString& channel)
{
    if (client && client->Subscribe(channel))
    {
        auto it(channels_.find(channel));
        if (it == channels_.end())
            it = channels_.insert(ChannelClients::value_type(channel, Clients())).first;

        assert (it != channels_.end());

        bool succ = it->second.insert(std::static_pointer_cast<QClient>(client->shared_from_this())).second;
        assert (succ);
        return 1;
    }

    return 0;
}


std::size_t QPubsub::UnSubscribe(QClient* client, const QString& channel)
{
    if (client && client->UnSubscribe(channel))
    {
        auto it(channels_.find(channel));
        assert (it != channels_.end());

        Clients& clientSet = it->second;

        std::size_t n = clientSet.erase(std::static_pointer_cast<QClient>(client->shared_from_this()));
        assert (n == 1);

        if (clientSet.empty())
            channels_.erase(it);

        return client->ChannelCount();
    }

    return 0;
}

std::size_t QPubsub::UnSubscribeAll(QClient* client)
{
    if (!client)  return 0;

    std::size_t  n = 0;
    const auto& channels = client->GetChannels();
    for (const auto& channel : channels)
    {
        n += UnSubscribe(client, channel);
    }

    return n;
}

size_t QPubsub::PSubscribe(QClient* client, const QString& channel)
{
    if (client && client->PSubscribe(channel))
    {
        auto it(patternChannels_.find(channel));
        if (it == patternChannels_.end())
            it = patternChannels_.insert(ChannelClients::value_type(channel, Clients())).first;

        assert (it != patternChannels_.end());

        bool succ = it->second.insert(std::static_pointer_cast<QClient>(client->shared_from_this())).second;
        assert (succ);
        return 1;
    }

    return 0;
}


std::size_t QPubsub::PUnSubscribe(QClient* client, const QString& channel)
{
    if (client && client->PUnSubscribe(channel))
    {
        auto  it(patternChannels_.find(channel));
        assert (it != patternChannels_.end());

        Clients& clientSet = it->second;

        std::size_t n = clientSet.erase(std::static_pointer_cast<QClient>(client->shared_from_this()));
        assert (n == 1);

        if (clientSet.empty())
            patternChannels_.erase(it);

        return client->PatternChannelCount();
    }

    return 0;
}

std::size_t QPubsub::PUnSubscribeAll(QClient* client)
{
    if (!client)  return 0;

    std::size_t  n = 0;
    const auto& channels = client->GetPatternChannels();
    for (const auto& channel : channels)
    {
        n += PUnSubscribe(client, channel);
    }

    return n;
}


std::size_t QPubsub::PublishMsg(const QString& channel, const QString& msg)
{
    std::size_t n = 0;

    auto it(channels_.find(channel));
    if (it != channels_.end())
    {
        Clients&  clientSet = it->second;
        for (auto  itCli(clientSet.begin());
                   itCli != clientSet.end();
            )
        {
            auto cli = itCli->lock();
            if (!cli)
            {
                clientSet.erase(itCli ++);
            }
            else
            {
                SocketAddr peer;
                Socket::GetPeerAddr(cli->GetSocket(), peer);
                WITH_LOG(INF << "Publish msg:" << msg.c_str() << " to " << peer.GetIP() << ":" << peer.GetPort());

                UnboundedBuffer   reply;
                PreFormatMultiBulk(3, &reply);
                FormatSingle("message", 7, &reply);
                FormatSingle(channel, &reply);
                FormatSingle(msg, &reply);
                cli->SendPacket(reply.ReadAddr(), reply.ReadableSize());

                ++ itCli;
                ++ n;
            }
        }
    }

    // TODO fuck me
    for (auto& pattern : patternChannels_)
    {
        if (glob_match(pattern.first, channel))
        {
            WITH_LOG(INF << channel.c_str() << " match " << pattern.first.c_str());
            Clients&  clientSet = pattern.second;
            for (auto itCli(clientSet.begin());
                      itCli != clientSet.end();
                )
            {
                auto cli = itCli->lock();
                if (!cli)
                {
                    clientSet.erase(itCli ++);
                }
                else
                {
                    SocketAddr peer;
                    Socket::GetPeerAddr(cli->GetSocket(), peer);
                    WITH_LOG(INF << "Publish msg:" << msg.c_str() << " to " << peer.GetIP() << ":" << peer.GetPort());

                    UnboundedBuffer   reply;
                    PreFormatMultiBulk(4, &reply);
                    FormatSingle("pmessage", 8, &reply);
                    FormatSingle(pattern.first, &reply);
                    FormatSingle(channel, &reply);
                    FormatSingle(msg, &reply);
                    cli->SendPacket(reply.ReadAddr(), reply.ReadableSize());

                    ++ itCli;
                    ++ n;
                }
            }
        }
    }

    return  n;
}

void QPubsub::RecycleClients(QString& startChannel, QString& startPattern)
{
    _RecycleClients(channels_, startChannel);
    _RecycleClients(patternChannels_, startPattern);
}

void QPubsub::_RecycleClients(ChannelClients& channels, QString& start)
{
    auto it(start.empty() ? channels.begin() : channels.find(start));
    if (it == channels.end())
        it = channels.begin();

    size_t  n = 0;
    while (it != channels.end() && n < 20)
    {
        Clients& cls = it->second;
        for (auto itCli(cls.begin());
                  itCli != cls.end();
            )
        {
            if (itCli->expired())
            {
                WITH_LOG(INF << "erase client");
                cls.erase(itCli ++);
                ++ n;
            }
            else
            {
                ++ itCli;
            }
        }
        
        if (cls.empty())
        {
            WITH_LOG(INF << "erase channel " << it->first.c_str());
            channels.erase(it ++);
        }
        else
        {
            ++ it;
        }
    }

    if (it != channels.end())
        start = it->first;
    else
        start.clear();
}

class PubsubTimer : public Timer
{
public:
    PubsubTimer() : Timer(100)
    {
    }
private:
    QString  startChannel_;
    QString  startPattern_;

    bool    _OnTimer()
    {
        QPubsub::Instance().RecycleClients(startChannel_, startPattern_);
        return  true;
    }
};

void QPubsub::InitPubsubTimer()
{
    TimerManager::Instance().AddTimer(PTIMER(new PubsubTimer));
}

void QPubsub::PubsubChannels(std::vector<QString>& res, const char* pattern) const
{
    res.clear();

    for (const auto& elem : channels_)
    {
        if (!pattern || glob_match(pattern, elem.first))
        {
            res.push_back(elem.first);
        }
    }
}


size_t  QPubsub::PubsubNumsub(const QString& channel) const
{
    auto it = channels_.find(channel);
    
    if (it != channels_.end())
        return it->second.size();
    
    return 0;
}

size_t QPubsub::PubsubNumpat() const
{
    std::size_t n = 0;
    
    for (const auto& elem : patternChannels_)
    {
        n += elem.second.size();
    }
    
    return n;
}

// pubsub commands
QError  subscribe(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QClient* client = QClient::Current();
    for (const auto& pa : params)
    {
        size_t n = QPubsub::Instance().Subscribe(client, pa);
        if (n == 1)
        {
            PreFormatMultiBulk(3, reply);
            FormatSingle("subscribe", 9, reply);
            FormatSingle(pa, reply);
            FormatInt(client->ChannelCount(), reply);

            SocketAddr peer;
            Socket::GetPeerAddr(client->GetSocket(), peer);
            WITH_LOG(INF << "subscribe " << pa.c_str() << " by " << peer.GetIP() << ":" << peer.GetPort());
        }
    }

    return QError_ok;
}

QError  psubscribe(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QClient* client = QClient::Current();
    for (const auto& pa : params)
    {
        size_t n = QPubsub::Instance().PSubscribe(client, pa);
        if (n == 1)
        {
            PreFormatMultiBulk(3, reply);
            FormatSingle("psubscribe", 9, reply);
            FormatSingle(pa, reply);
            FormatInt(client->PatternChannelCount(), reply);

            SocketAddr peer;
            Socket::GetPeerAddr(client->GetSocket(), peer);
            WITH_LOG(INF << "psubscribe " << pa.c_str() << " by " << peer.GetIP() << ":" << peer.GetPort());
        }
    }

    return QError_ok;
}


QError  unsubscribe(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QClient* client = QClient::Current();

    if (params.size() == 1)
    {
        const auto& channels = client->GetChannels();
        for (const auto& channel : channels)
        {
            FormatSingle(channel, reply);
        }
        
        QPubsub::Instance().UnSubscribeAll(client);
    }
    else
    {
        std::set<QString> channels;

        for (size_t i = 1; i < params.size(); ++ i)
        {
            size_t n = QPubsub::Instance().UnSubscribe(client, params[i]);
            if (n == 1)
            {
                channels.insert(params[i]);

                SocketAddr peer;
                Socket::GetPeerAddr(client->GetSocket(), peer);
                WITH_LOG(INF << "unsubscribe " << params[i].c_str() << " by " << peer.GetIP() << ":" << peer.GetPort());
            }
        }

        PreFormatMultiBulk(channels.size(), reply);
        for (const auto& channel : channels)
        {
            FormatSingle(channel, reply);
        }
    }

    return  QError_ok;
}

QError  punsubscribe(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QClient* client = QClient::Current();

    if (params.size() == 1)
    {
        const auto& channels = client->GetPatternChannels();
        for (const auto& channel : channels)
        {
            FormatSingle(channel, reply);
        }
        
        QPubsub::Instance().PUnSubscribeAll(client);
    }
    else
    {
        std::set<QString> channels;

        for (size_t i = 1; i < params.size(); ++ i)
        {
            size_t n = QPubsub::Instance().PUnSubscribe(client, params[i]);
            if (n == 1)
            {
                channels.insert(params[i]);

                SocketAddr peer;
                Socket::GetPeerAddr(client->GetSocket(), peer);
                WITH_LOG(INF << "punsubscribe " << params[i].c_str() << " by " << peer.GetIP() << ":" << peer.GetPort());
            }
        }

        PreFormatMultiBulk(channels.size(), reply);
        for (const auto& channel : channels)
        {
            FormatSingle(channel, reply);
        }
    }

    return  QError_ok;
}

QError  publish(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    size_t n = QPubsub::Instance().PublishMsg(params[1], params[2]);
    FormatInt(n, reply);

    return QError_ok;
}

// neixing command
QError  pubsub(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    if (params[1] == "channels")
    {
        if (params.size() > 3)
        {
            ReplyError(QError_param, reply);
            return QError_param;
        }

        std::vector<QString> res;
        QPubsub::Instance().PubsubChannels(res, params.size() == 3 ? params[2].c_str() : 0);
        PreFormatMultiBulk(res.size(), reply);
        for (const auto& channel : res)
        {
            FormatSingle(channel, reply);
        }
    }
    else if (params[1] == "numsub")
    {
        PreFormatMultiBulk(2 * (params.size() - 2), reply);
        for (size_t i = 2; i < params.size(); ++ i)
        {
            size_t n = QPubsub::Instance().PubsubNumsub(params[i]);
            FormatSingle(params[i], reply);
            FormatInt(n, reply);
        }
    }
    else if (params[1] == "numpat")
    {
        if (params.size() != 2)
        {
            ReplyError(QError_param, reply);
            return QError_param;
        }
        
        FormatInt(QPubsub::Instance().PubsubNumpat(), reply);
    }
    else
    {
        WITH_LOG(ERR << "Unknown pubsub subcmd " << params[1].c_str());
    }

    return QError_ok;
}

}
