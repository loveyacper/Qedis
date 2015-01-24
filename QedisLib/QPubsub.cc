#include "QPubsub.h"
#include "QClient.h"
#include "Log/Logger.h"
#include "Timer.h"
#include "QGlobRegex.h"

using namespace std;

QPubsub&    QPubsub::Instance()
{
    static QPubsub  ps;
    return ps;
}
    
size_t QPubsub::Subscribe(QClient* client, const QString& channel)
{
    if (client && client->Subscribe(channel))
    {
        ChannelClients::iterator it(m_channels.find(channel));
        if (it == m_channels.end())
            it = m_channels.insert(ChannelClients::value_type(channel, Clients())).first;

        assert (it != m_channels.end());

        bool succ = it->second.insert(client->ShareMe()).second;
        assert (succ);
        return 1;
    }

    return 0;
}


std::size_t QPubsub::UnSubscribe(QClient* client, const QString& channel)
{
    if (client && client->UnSubscribe(channel))
    {
        ChannelClients::iterator it(m_channels.find(channel));
        assert (it != m_channels.end());

        Clients& clientSet = it->second;

        std::size_t n = clientSet.erase(client->ShareMe());
        assert (n == 1);

        if (clientSet.empty())
            m_channels.erase(it);

        return client->ChannelCount();
    }

    return 0;
}

std::size_t QPubsub::UnSubscribeAll(QClient* client)
{
    if (!client)  return 0;

    std::size_t  n = 0;
    const std::set<QString>& channels = client->GetChannels();
    for (std::set<QString>::const_iterator it(channels.begin());
            it != channels.end();
            ++ it)
    {
        n += UnSubscribe(client, *it);
    }

    return n;
}

size_t QPubsub::PSubscribe(QClient* client, const QString& channel)
{
    if (client && client->PSubscribe(channel))
    {
        ChannelClients::iterator it(m_patternChannels.find(channel));
        if (it == m_patternChannels.end())
            it = m_patternChannels.insert(ChannelClients::value_type(channel, Clients())).first;

        assert (it != m_patternChannels.end());

        bool succ = it->second.insert(client->ShareMe()).second;
        assert (succ);
        return 1;
    }

    return 0;
}


std::size_t QPubsub::PUnSubscribe(QClient* client, const QString& channel)
{
    if (client && client->PUnSubscribe(channel))
    {
        ChannelClients::iterator it(m_patternChannels.find(channel));
        assert (it != m_patternChannels.end());

        Clients& clientSet = it->second;

        std::size_t n = clientSet.erase(client->ShareMe());
        assert (n == 1);

        if (clientSet.empty())
            m_patternChannels.erase(it);

        return client->PatternChannelCount();
    }

    return 0;
}

std::size_t QPubsub::PUnSubscribeAll(QClient* client)
{
    if (!client)  return 0;

    std::size_t  n = 0;
    const std::set<QString>& channels = client->GetPatternChannels();
    for (std::set<QString>::const_iterator it(channels.begin());
            it != channels.end();
            ++ it)
    {
        n += PUnSubscribe(client, *it);
    }

    return n;
}


std::size_t QPubsub::PublishMsg(const QString& channel, const QString& msg)
{
    std::size_t n = 0;

    ChannelClients::iterator it(m_channels.find(channel));
    if (it != m_channels.end())
    {
        Clients&  clientSet = it->second;
        for (Clients::iterator itCli(clientSet.begin());
                               itCli != clientSet.end();
            )
        {
            SharedPtr<QClient>  cli = itCli->Lock();
            if (!cli)
            {
                clientSet.erase(itCli ++);
            }
            else
            {
                SocketAddr peer;
                Socket::GetPeerAddr(cli->GetSocket(), peer);
                LOG_INF(g_log) << "Publish msg:" << msg.c_str() << " to " << peer.GetIP() << ":" << peer.GetPort();

                UnboundedBuffer   reply;
                PreFormatMultiBulk(3, reply);
                FormatSingle("message", 7, reply);
                FormatSingle(channel.c_str(), channel.size(), reply);
                FormatSingle(msg.c_str(), msg.size(), reply);
                cli->SendPacket(reply.ReadAddr(), reply.ReadableSize());

                ++ itCli;
                ++ n;
            }
        }
    }

    // TODO fuck me
    for (ChannelClients::iterator it(m_patternChannels.begin());
            it != m_patternChannels.end();
            ++ it)
    {
        if (glob_match(it->first, channel))
        {
            LOG_INF(g_log) << channel.c_str() << " match " << it->first.c_str();
            Clients&  clientSet = it->second;
            for (Clients::iterator itCli(clientSet.begin());
                                   itCli != clientSet.end();
                )
            {
                SharedPtr<QClient>  cli = itCli->Lock();
                if (!cli)
                {
                    clientSet.erase(itCli ++);
                }
                else
                {
                    SocketAddr peer;
                    Socket::GetPeerAddr(cli->GetSocket(), peer);
                    LOG_INF(g_log) << "Publish msg:" << msg.c_str() << " to " << peer.GetIP() << ":" << peer.GetPort();

                    UnboundedBuffer   reply;
                    PreFormatMultiBulk(4, reply);
                    FormatSingle("pmessage", 8, reply);
                    FormatSingle(it->first.c_str(), it->first.size(), reply);
                    FormatSingle(channel.c_str(), channel.size(), reply);
                    FormatSingle(msg.c_str(), msg.size(), reply);
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
    _RecycleClients(m_channels, startChannel);
    _RecycleClients(m_patternChannels, startPattern);
}

void QPubsub::_RecycleClients(ChannelClients& channels, QString& start)
{
    ChannelClients::iterator it(start.empty() ? channels.begin() : channels.find(start));
    if (it == channels.end())
        it = channels.begin();

    size_t  n = 0;
    while (it != channels.end() && n < 20)
    {
        Clients& cls = it->second;
        for (Clients::iterator itCli(cls.begin());
                itCli != cls.end();
                )
        {
            if (itCli->Expired())
            {
                LOG_INF(g_log) << "erase client";
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
            LOG_INF(g_log) << "erase channel " << it->first.c_str();
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
    QString  m_startChannel;
    QString  m_startPattern;
    bool    _OnTimer()
    {
        QPubsub::Instance().RecycleClients(m_startChannel, m_startPattern);
        return  true;
    }
};

void QPubsub::InitPubsubTimer()
{
    TimerManager::Instance().AddTimer(PTIMER(new PubsubTimer));
}

void QPubsub::PubsubChannels(vector<QString>& res, const char* pattern) const
{
    res.clear();

    for (ChannelClients::const_iterator it(m_channels.begin());
         it != m_channels.end();
         ++ it)
    {
        if (!pattern || glob_match(pattern, it->first))
        {
            res.push_back(it->first);
        }
    }
}


size_t  QPubsub::PubsubNumsub(const QString& channel) const
{
    ChannelClients::const_iterator it = m_channels.find(channel);
    
    if (it != m_channels.end())
        return it->second.size();
    
    return 0;
}

size_t QPubsub::PubsubNumpat() const
{
    std::size_t n = 0;
    ChannelClients::const_iterator it(m_patternChannels.begin());
    for (; it != m_patternChannels.end(); ++ it)
    {
        n += it->second.size();
    }
    
    return n;
}

// pubsub commands
QError  subscribe(const vector<QString>& params, UnboundedBuffer& reply)
{
    QClient* client = QClient::Current();
    for (size_t i = 1; i < params.size(); ++ i)
    {
        size_t n = QPubsub::Instance().Subscribe(client, params[i]);
        if (n == 1)
        {
            PreFormatMultiBulk(3, reply);
            FormatSingle("subscribe", 9, reply);
            FormatSingle(params[i].c_str(),  params[i].size(), reply);
            FormatInt(client->ChannelCount(), reply);

            SocketAddr peer;
            Socket::GetPeerAddr(client->GetSocket(), peer);
            LOG_INF(g_log) << "subscribe " << params[i].c_str() << " by " << peer.GetIP() << ":" << peer.GetPort();
        }
    }

    return QError_ok;
}

QError  psubscribe(const vector<QString>& params, UnboundedBuffer& reply)
{
    QClient* client = QClient::Current();
    for (size_t i = 1; i < params.size(); ++ i)
    {
        size_t n = QPubsub::Instance().PSubscribe(client, params[i]);
        if (n == 1)
        {
            PreFormatMultiBulk(3, reply);
            FormatSingle("psubscribe", 9, reply);
            FormatSingle(params[i].c_str(),  params[i].size(), reply);
            FormatInt(client->PatternChannelCount(), reply);

            SocketAddr peer;
            Socket::GetPeerAddr(client->GetSocket(), peer);
            LOG_INF(g_log) << "psubscribe " << params[i].c_str() << " by " << peer.GetIP() << ":" << peer.GetPort();
        }
    }

    return QError_ok;
}


QError  unsubscribe(const vector<QString>& params, UnboundedBuffer& reply)
{
    QClient* client = QClient::Current();

    if (params.size() == 1)
    {
        const set<QString>& channels = client->GetChannels();
        for (set<QString>::const_iterator it(channels.begin());
                it != channels.end();
                ++ it)
        {
            FormatSingle(it->c_str(), it->size(), reply);
        }
        
        QPubsub::Instance().UnSubscribeAll(client);
    }
    else
    {
        set<QString> channels;

        for (size_t i = 1; i < params.size(); ++ i)
        {
            size_t n = QPubsub::Instance().UnSubscribe(client, params[i]);
            if (n == 1)
            {
                channels.insert(params[i]);

                SocketAddr peer;
                Socket::GetPeerAddr(client->GetSocket(), peer);
                LOG_INF(g_log) << "unsubscribe " << params[i].c_str() << " by " << peer.GetIP() << ":" << peer.GetPort();
            }
        }

        PreFormatMultiBulk(channels.size(), reply);
        for (set<QString>::const_iterator it(channels.begin());
                it != channels.end();
                ++ it)
        {
            FormatSingle(it->c_str(), it->size(), reply);
        }
    }

    return  QError_ok;
}

QError  punsubscribe(const vector<QString>& params, UnboundedBuffer& reply)
{
    QClient* client = QClient::Current();

    if (params.size() == 1)
    {
        const set<QString>& channels = client->GetPatternChannels();
        for (set<QString>::const_iterator it(channels.begin());
                it != channels.end();
                ++ it)
        {
            FormatSingle(it->c_str(), it->size(), reply);
        }
        
        QPubsub::Instance().PUnSubscribeAll(client);
    }
    else
    {
        set<QString> channels;

        for (size_t i = 1; i < params.size(); ++ i)
        {
            size_t n = QPubsub::Instance().PUnSubscribe(client, params[i]);
            if (n == 1)
            {
                channels.insert(params[i]);

                SocketAddr peer;
                Socket::GetPeerAddr(client->GetSocket(), peer);
                LOG_INF(g_log) << "punsubscribe " << params[i].c_str() << " by " << peer.GetIP() << ":" << peer.GetPort();
            }
        }

        PreFormatMultiBulk(channels.size(), reply);
        for (set<QString>::const_iterator it(channels.begin());
                it != channels.end();
                ++ it)
        {
            FormatSingle(it->c_str(), it->size(), reply);
        }
    }

    return  QError_ok;
}

QError  publish(const vector<QString>& params, UnboundedBuffer& reply)
{
    size_t n = QPubsub::Instance().PublishMsg(params[1], params[2]);
    FormatInt(n, reply);

    return QError_ok;
}

// neixing command
QError  pubsub(const vector<QString>& params, UnboundedBuffer& reply)
{
    if (params[1] == "channels")
    {
        if (params.size() > 3)
        {
            ReplyError(QError_param, reply);
            return QError_param;
        }

        vector<QString> res;
        QPubsub::Instance().PubsubChannels(res, params.size() == 3 ? params[2].c_str() : 0);
        PreFormatMultiBulk(res.size(), reply);
        for (vector<QString>::const_iterator it(res.begin());
             it != res.end();
             ++ it)
        {
            FormatSingle(it->c_str(), it->size(), reply);
        }
    }
    else if (params[1] == "numsub")
    {
        PreFormatMultiBulk(2 * (params.size() - 2), reply);
        for (size_t i = 2; i < params.size(); ++ i)
        {
            size_t n = QPubsub::Instance().PubsubNumsub(params[i]);
            FormatSingle(params[i].c_str(), params[i].size(), reply);
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
        LOG_ERR(g_log) << "Unknown pubsub subcmd " << params[1].c_str();
    }

    return QError_ok;
}

