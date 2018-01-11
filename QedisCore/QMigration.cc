#include "Server.h"
#include "QMigration.h"
#include "Timer.h"
#include "QDB.h"

namespace qedis
{

static void ProcessItem(MigrationItem& item, QMigrateClient* conn)
{
    int pendings = 0;
    UnboundedBuffer request;
    for (auto it = item.keys.begin();
              it != item.keys.end(); )
    {
        const auto& key = *it;
        if (key.empty())
        {
            it = item.keys.erase(it);
            continue;
        }

        const QObject* obj = QSTORE.GetObject(key);
        if (!obj)
        {
            it = item.keys.erase(it);
            continue;
        }

        ++ pendings;
        std::string contents = DumpObject(*obj);
        if (item.replace)
        {
            PreFormatMultiBulk(5, &request);
            FormatMultiBulk({"restore", key, "0", contents, "replace"}, &request);
        }
        else
        {
            PreFormatMultiBulk(4, &request);
            FormatMultiBulk({"restore", key, "0", contents}, &request);
        }

        ++ it;
    }

    if (pendings > 0)
    {
        UnboundedBuffer select;
        PreFormatMultiBulk(2, &select);
        FormatMultiBulk({"select", std::to_string(item.dstDb)}, &select);

        conn->SendPacket(select.ReadAddr(), select.ReadableSize());
        conn->SendPacket(request.ReadAddr(), request.ReadableSize());

        item.state = MigrateState::Processing;
    }
    else
    {
        item.state = MigrateState::Done;
        auto c = item.client.lock();
        if (c)
            c->SendPacket("+NOKEY\r\n", 8);
    }
}


QMigrationManager& QMigrationManager::Instance()
{
    static QMigrationManager mgr;
    return mgr;
}
    
void QMigrationManager::Add(const MigrationItem& item)
{
    items_[item.dst].push_back(item);
}

void QMigrationManager::Add(MigrationItem&& item)
{
    items_[item.dst].push_back(std::move(item));
}

void QMigrationManager::InitMigrationTimer()
{
    auto timer = TimerManager::Instance().CreateTimer();
    timer->Init(20);
    timer->SetCallback([]() {
       QMigrationManager::Instance().LoopCheck();
    });
    
    TimerManager::Instance().AddTimer(timer);
}

void QMigrationManager::LoopCheck()
{
    auto now = ::time(nullptr);

    for (auto iter(items_.begin()); iter != items_.end(); )
    {
        auto& items = iter->second;

        for (auto it = items.begin(); it != items.end(); )
        {
            auto& item = *it;
            if (item.state != MigrateState::Done && now > item.timeout)
                item.state = MigrateState::Timeout;
            
            bool erased = false;
            switch (item.state)
            {
                case MigrateState::None:
                    {
                        auto c = GetConnection(item.dst);
                        if (c)
                            ProcessItem(item, c.get());
                    }
                    break;

                case MigrateState::Processing:
                    //waiting....
                    break;

                case MigrateState::Timeout:
                    {
                        auto c = item.client.lock();
                        if (c)
                        {
                            const char err[] = "-IOERR error or timeout for target instance\r\n";
                            c->SendPacket(err, sizeof err - 1);
                            item.state = MigrateState::Done;
                        }
                    }
        
                case MigrateState::Done:
                    it = items.erase(it);
                    erased = true;
                    break;

                default:
                    assert(false);
                    break;
            }

            if (!erased)
                ++ it;
        }

        if (items.empty())
            iter = items_.erase(iter);
        else
            ++ iter;
    }
}

std::shared_ptr<QMigrateClient>
QMigrationManager::GetConnection(const SocketAddr& dst)
{
    auto it = conns_.find(dst);
    if (it != conns_.end())
    {
        auto c = it->second.lock();
        if (c)
            return c;
        else
            conns_.erase(it);

    }

    if (pendingConnect_.count(dst))
        return nullptr; // already connecting

    pendingConnect_.insert(dst);
    Server::Instance()->TCPConnect(dst,
                                   std::bind(&QMigrationManager::OnConnectMigrateFail, this, dst),
                                   ConnectionTag::kMigrateClient);

    return nullptr;
}

void QMigrationManager::OnConnectMigrateFail(const SocketAddr& dst)
{
    pendingConnect_.erase(dst);
    auto it = items_.find(dst);
    if (it == items_.end())
        return;

    for (auto& item : it->second)
    {
        if (item.state == MigrateState::Done)
            continue;

        // can not ensure success, so retry
        if (item.state == MigrateState::Processing)
            item.state = MigrateState::None;
    }
}

void QMigrationManager::OnConnect(QMigrateClient* client)
{
    const auto& dst = client->GetPeerAddr();
    std::weak_ptr<QMigrateClient> wc = std::static_pointer_cast<QMigrateClient>(client->shared_from_this());

    bool succ = conns_.insert({dst, wc}).second;
    assert (succ);

    size_t tmp = pendingConnect_.erase(dst);
    assert (tmp == 1);

    auto it = items_.find(dst);
    if (it == items_.end())
        client->OnError();
}

void QMigrateClient::OnConnect()
{
    auto& mgr = QMigrationManager::Instance();
    mgr.OnConnect(this);

    auto it = mgr.items_.find(peerAddr_);
    assert (it != mgr.items_.end());
    for (auto& item : it->second)
    {
        auto c = item.client.lock();
        if (!c)
        {
            item.state = MigrateState::Done;
            continue;
        }

        if (item.state == MigrateState::None)
        {
            ProcessItem(item, this);
        }
    }
}

PacketLength QMigrateClient::_HandlePacket(const char* msg, std::size_t len)
{
    if (len < 5)
        return 0;

    const char* crlf = SearchCRLF(msg, len);
    if (!crlf)
        return 0;

    ++ readyRsp_;

    auto it = QMigrationManager::Instance().items_.find(peerAddr_);
    auto& item = it->second.front();
    if (item.keys.size() + 1 == readyRsp_) // plus select db
    {
        readyRsp_ = 0;
        auto c = item.client.lock();

        if (c)
            c->SendPacket("+OK\r\n", 5);

        if (!item.copy)
        {
            std::vector<QString> params {"del"};
            params.insert(params.end(), item.keys.begin(), item.keys.end());

            extern QError del(const std::vector<QString>& , UnboundedBuffer* );
            del(params, nullptr);
            Propogate(params);
        }
        
        item.state = MigrateState::Done;
    }

    return static_cast<PacketLength>(crlf + 2 - msg);
}

// migrate host port key dst-db timeout COPY REPLACE KEYS key1 key2
QError migrate(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    try {
        struct MigrationItem item;
        item.dst.Init(params[1].data(), std::stoi(params[2]));
        item.dstDb = std::stoi(params[4]);
        item.timeout = ::time(nullptr) + (std::stoi(params[5]) + 999) / 1000;

        const int kUnused = -1;
        int copy = kUnused;
        int replace = kUnused;
        bool withKeys = false;
        for (size_t i = 6; i < params.size(); ++ i)
        {
            if (withKeys)
            {
                item.keys.push_back(params[i]);
            }
            else
            {
                // must be COPY REPLACE or KEYS
                if (strncasecmp(params[i].c_str(), "copy", 4))
                {
                    if (copy == kUnused)
                        copy = 1;
                    else
                        throw std::runtime_error("wrong syntax for migrate");
                }
                else if (strncasecmp(params[i].c_str(), "replace", 7))
                {
                    if (replace == kUnused)
                        replace = 1;
                    else
                        throw std::runtime_error("wrong syntax for migrate");
                }
                else if (strncasecmp(params[i].c_str(), "keys", 4))
                {
                    withKeys = true;
                }
                else
                {
                    throw std::runtime_error("wrong syntax for migrate");
                }
            }
        }

        if (!withKeys && !params[3].empty())
            item.keys.push_back(params[3]);

        if (copy == 1)
            item.copy = true;
        if (replace == 1)
            item.replace = true;

        item.client = std::static_pointer_cast<StreamSocket>(QClient::Current()->shared_from_this());
        QMigrationManager::Instance().Add(std::move(item));

        return QError_ok;
    } catch (...) {
        ReplyError(QError_syntax, reply);
        return QError_syntax;
    }
        
    return QError_ok;
}

} // end namespace qedis

