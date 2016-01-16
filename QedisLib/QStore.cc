#include "QStore.h"
#include "QClient.h"
#include "QConfig.h"
#include "QAOF.h"
#include "QMulti.h"
#include "Log/Logger.h"
#include <limits>
#include <cassert>

namespace qedis
{

int QStore::dirty_ = 0;

void QStore::ExpiresDB::SetExpire(const QString& key, uint64_t when)
{
    expireKeys_[key] = when;
    INF << "Set timeout key " << key.c_str() << ", timeout is " << when;
}

int64_t  QStore::ExpiresDB::TTL(const QString& key, uint64_t now)
{
    if (!QSTORE.ExistsKey(key))
    {
        return ExpireResult::notExist;
    }

    ExpireResult ret = ExpireIfNeed(key, now);
    switch (ret)
    {
        case ExpireResult::expired:
        case ExpireResult::persist:
            return ret;
            
        default:
            break;
    }
    
    auto it(expireKeys_.find(key));
    return static_cast<int64_t>(it->second - now);
}

bool QStore::ExpiresDB::ClearExpire(const QString& key)
{
    return ExpireResult::expired == ExpireIfNeed(key, std::numeric_limits<uint64_t>::max());
}

QStore::ExpireResult  QStore::ExpiresDB::ExpireIfNeed(const QString& key, uint64_t now)
{
    auto    it(expireKeys_.find(key));
    
    if (it != expireKeys_.end())
    {
        if (it->second > now)
            return ExpireResult::notExpire; // not expire
        
        WRN << "Delete timeout key " << it->first.c_str() << ", timeout is " << it->second;
        expireKeys_.erase(it);
        return ExpireResult::expired;
    }
    
    return  ExpireResult::persist;
}

int   QStore::ExpiresDB::LoopCheck(uint64_t now)
{
    const int MAX_DEL = 100;
    const int MAX_CHECK = 2000;
    int  nDel = 0;
    int  nLoop = 0;

    for (auto  it = expireKeys_.begin();
               it!= expireKeys_.end() && nDel < MAX_DEL && nLoop < MAX_CHECK;
               ++ nLoop)
    {
        if (it->second <= now)
        {
            // time to delete
            WRN << "LoopCheck try delete key " << it->first.c_str() << ", " << ::Now();
            
            QSTORE.DeleteKey(it->first);
            expireKeys_.erase(it ++);

            ++ nDel;
        }
        else
        {
            ++ it;
        }
    }

    return nDel;

}

bool ExpireTimer::_OnTimer()
{
    int oldDb = QSTORE.SelectDB(dbno_);
    QSTORE.LoopCheckExpire(::Now());
    QSTORE.SelectDB(oldDb);
    return  true;
}

bool QStore::BlockedClients::BlockClient(const QString& key,
                                         QClient* client,
                                         uint64_t timeout,
                                         ListPosition pos,
                                         const QString* target)
{
    if (!client->WaitFor(key, target))
    {
        ERR << key << " is already waited by " << client->GetName();
        return  false;
    }
        
    Clients& clients = blockedClients_[key];
    clients.push_back(Clients::value_type(std::static_pointer_cast<QClient>(client->shared_from_this()), timeout, pos));
    INF << key << " is waited by " << client->GetName() << ", timeout " << timeout;
    return true;
}

size_t QStore::BlockedClients::UnblockClient(QClient* client)
{
    size_t n = 0;
    const auto& keys = client->WaitingKeys();
    
    for (const auto& key : keys)
    {
        Clients&  clients = blockedClients_[key];
        assert(!clients.empty());
        
        for (auto it(clients.begin()); it != clients.end(); ++ it)
        {
            auto  cli(std::get<0>(*it).lock());
            if (cli && cli.get() == client)
            {
                INF << "unblock " << client->GetName() << " for key " << key;
                clients.erase(it);
                
                ++ n;
                break;
            }
        }
    }
    
    client->ClearWaitingKeys();
    return n;
}


size_t  QStore::BlockedClients::ServeClient(const QString& key, const PLIST& list)
{
    assert(!list->empty());
    
    auto it = blockedClients_.find(key);
    if (it == blockedClients_.end())
        return 0;

    Clients& clients = it->second;
    if (clients.empty())
        return 0;
    
    size_t nServed = 0;
        
    while (!list->empty() && !clients.empty())
    {
        auto  cli(std::get<0>(clients.front()).lock());
        auto  pos(std::get<2>(clients.front()));

        if (cli)
        {
            bool  errorTarget     = false;
            const QString& target = cli->GetTarget();
            
            QObject*  dst = nullptr;

            if (!target.empty())
            {
                INF << list->front() << " is try lpush to target list " << target;
                
                // check target list
                QError  err = QSTORE.GetValueByType(target, dst, QType_list);
                if (err != QError_ok)
                {
                    if (err != QError_notExist)
                    {
                        UnboundedBuffer  reply;
                        ReplyError(err, &reply);
                        cli->SendPacket(reply.ReadAddr(), reply.ReadableSize());
                        errorTarget = true;
                    }
                    else
                    {
                        QObject  dstObj(QType_list);
                        dstObj.value = std::make_shared<QList>();
                        dst = QSTORE.SetValue(target, dstObj);
                    }
                }
            }
            
            if (!errorTarget)
            {
                if (dst)
                {
                    const PLIST& dstlist = dst->CastList();
                    dstlist->push_front(list->back());
                    INF << list->front() << " success lpush to target list " << target;

                    std::vector<QString> params;
                    params.push_back("lpush");
                    params.push_back(target); // key
                    params.push_back(list->back());

                    Propogate(params);
                }
                
                UnboundedBuffer reply;
            
                if (!dst)
                {
                    PreFormatMultiBulk(2, &reply);
                    FormatBulk(key.c_str(), key.size(), &reply);
                }

                if (pos == ListPosition::head)
                {
                    FormatBulk(list->front().c_str(), list->front().size(), &reply);
                    list->pop_front();

                    std::vector<QString> params;
                    params.push_back("lpop");
                    params.push_back(key);

                    Propogate(params);
                }
                else
                {
                    FormatBulk(list->back().c_str(), list->back().size(), &reply);
                    list->pop_back();

                    std::vector<QString> params;
                    params.push_back("rpop");
                    params.push_back(key);

                    Propogate(params);
                }
                
                cli->SendPacket(reply.ReadAddr(), reply.ReadableSize());
                INF << "Serve client " << cli->GetName() << " list key : " << key;
            }
            
            UnblockClient(cli.get());
            ++ nServed;
        }
        else
        {
            clients.pop_front();
        }
    }
    
    return nServed;
}
    
int QStore::BlockedClients::LoopCheck(uint64_t now)
{
    int  n = 0;

    for (auto  it(blockedClients_.begin());
         it != blockedClients_.end() && n < 100;
         )
    {
        Clients&  clients = it->second;
        for (auto cli(clients.begin()); cli != clients.end(); )
        {
            if (std::get<1>(*cli) < now) // timeout
            {
                ++ n;
                
                const QString&  key = it->first;
                auto  scli(std::get<0>(*cli).lock());
                if (scli && scli->WaitingKeys().count(key))
                {
                    INF << scli->GetName() << " is timeout for waiting key " << key;
                    UnboundedBuffer  reply;
                    FormatNull(&reply);
                    scli->SendPacket(reply.ReadAddr(), reply.ReadableSize());
                    scli->ClearWaitingKeys();
                }

                clients.erase(cli ++);
            }
            else
            {
                ++ cli;
            }
        }
        
        if (clients.empty())
        {
            blockedClients_.erase(it ++);
        }
        else
        {
            ++ it;
        }
    }
    
    return n;
}

bool BlockedListTimer::_OnTimer()
{
    int oldDb = QSTORE.SelectDB(dbno_);
    QSTORE.LoopCheckBlocked(::Now());
    QSTORE.SelectDB(oldDb);
    return  true;
}

QStore& QStore::Instance()
{
    static QStore  store;
    return store;
}

void  QStore::Init(int dbNum)
{
    if (dbNum < 1)
        dbNum = 1;
    else if (dbNum > kMaxDbNum)
        dbNum = kMaxDbNum;
    
    store_.resize(dbNum);
    expiresDb_.resize(dbNum);
    blockedClients_.resize(dbNum);
}

int  QStore::LoopCheckExpire(uint64_t now)
{
    return expiresDb_[dbno_].LoopCheck(now);
}

int  QStore::LoopCheckBlocked(uint64_t now)
{
    return blockedClients_[dbno_].LoopCheck(now);
}


int QStore::SelectDB(int dbno)
{
    if (dbno == dbno_)
    {
        return  dbno_;
    }
    
    if (dbno >= 0 && dbno < static_cast<int>(store_.size()))
    {
        int oldDb = dbno_;

        dbno_    = dbno;
        return  oldDb;
    }
        
    return  -1;
}

int  QStore::GetDB() const
{
    return  dbno_;
}

bool QStore::DeleteKey(const QString& key)
{
    auto db = &store_[dbno_];
    return db->erase(key) != 0;
}

bool QStore::ExistsKey(const QString& key) const
{
    auto db = &store_[dbno_];
    return  db->count(key) != 0;
}

QType  QStore::KeyType(const QString& key) const
{
    auto db = &store_[dbno_];
    QDB::const_iterator it(db->find(key));
    if (it == db->end())
        return  QType_invalid;
    
    return  QType(it->second.type);
}

static bool RandomMember(const QDB& hash, QString& res)
{
    QDB::const_local_iterator it = RandomHashMember(hash);
    
    if (it != QDB::const_local_iterator())
    {
        res = it->first;
        return true;
    }
    
    return false;
}

QString QStore::RandomKey() const
{
    QString  res;
    if (!store_.empty() && !store_[dbno_].empty())
    {
        RandomMember(store_[dbno_], res);
    }

    return  res;
}

size_t QStore::ScanKey(size_t cursor, size_t count, std::vector<QString>& res) const
{
    if (store_.empty() || store_[dbno_].empty())
        return 0;

    std::vector<QDB::const_local_iterator>  iters;
    size_t newCursor = ScanHashMember(store_[dbno_], cursor, count, iters);

    res.reserve(iters.size());
    for (auto it : iters)
        res.push_back(it->first);

    return newCursor;
}

QError  QStore::GetValue(const QString& key, QObject*& value)
{
    return GetValueByType(key, value);
}

QError  QStore::GetValueByType(const QString& key, QObject*& value, QType type)
{
    if (_ExpireIfNeed(key, ::Now()) == ExpireResult::expired)
    {
        return QError_notExist;
    }
    
    auto db = &store_[dbno_];
    QDB::iterator    it(db->find(key));

    if (it != db->end())
    {
        if (type != QType_invalid && type != QType(it->second.type))
        {
            return QError_type;
        }
        else
        {
            value = &it->second;
            return QError_ok;
        }
    }
    else
    {
        return QError_notExist;
    }

    return  QError_ok; // never here
}


QObject* QStore::SetValue(const QString& key, const QObject& value)
{
    auto db = &store_[dbno_];
    QObject& obj = ((*db)[key] = value);
    return &obj;
}

bool QStore::SetValueIfNotExist(const QString& key, const QObject& value)
{
    auto db = &store_[dbno_];
    QDB::iterator    it(db->find(key));

    if (it == db->end())
        (*db)[key] = value;

    return it == db->end();
}


void    QStore::SetExpire(const QString& key, uint64_t when)
{
    expiresDb_[dbno_].SetExpire(key, when);
}


int64_t QStore::TTL(const QString& key, uint64_t now)
{
    return  expiresDb_[dbno_].TTL(key, now);
}

bool    QStore::ClearExpire(const QString& key)
{
    return expiresDb_[dbno_].ClearExpire(key);
}

QStore::ExpireResult QStore::_ExpireIfNeed(const QString& key, uint64_t now)
{
    return  expiresDb_[dbno_].ExpireIfNeed(key, now);
}

void    QStore::InitExpireTimer()
{
    for (int i = 0; i < static_cast<int>(expiresDb_.size()); ++ i)
        TimerManager::Instance().AddTimer(std::make_shared<ExpireTimer>(i));
}

void    QStore::ResetDb()
{
    std::vector<QDB>(store_.size()).swap(store_);
    std::vector<ExpiresDB>(expiresDb_.size()).swap(expiresDb_);
    std::vector<BlockedClients>(blockedClients_.size()).swap(blockedClients_);
    dbno_ = 0;
}

size_t  QStore::BlockedSize() const
{
    size_t s = 0;
    for (const auto& b : blockedClients_)
        s += b.Size();
    
    return s;
}

bool    QStore::BlockClient(const QString& key, QClient* client, uint64_t timeout, ListPosition pos, const QString* dstList)
{
    return blockedClients_[dbno_].BlockClient(key, client, timeout, pos, dstList);
}
size_t  QStore::UnblockClient(QClient* client)
{
    return blockedClients_[dbno_].UnblockClient(client);
}
size_t  QStore::ServeClient(const QString& key, const PLIST& list)
{
    return blockedClients_[dbno_].ServeClient(key, list);
}

void    QStore::InitBlockedTimer()
{
    for (int i = 0; i < static_cast<int>(blockedClients_.size()); ++ i)
        TimerManager::Instance().AddTimer(std::make_shared<BlockedListTimer>(i));
}


std::vector<QString>  g_dirtyKeys;

void Propogate(const std::vector<QString>& params)
{
    if (!g_dirtyKeys.empty())
    {
        for (const auto& k : g_dirtyKeys)
        {
            ++ QStore::dirty_;
            QMulti::Instance().NotifyDirty(k);
        }
        g_dirtyKeys.clear();
    }
    else
    {
        ++ QStore::dirty_;
        QMulti::Instance().NotifyDirty(params[1]);
    }

    if (params.empty())
        return;

    if (g_config.appendonly)
        QAOFThreadController::Instance().SaveCommand(params, QSTORE.GetDB());

    QReplication::Instance().SendToSlaves(params);
}

}