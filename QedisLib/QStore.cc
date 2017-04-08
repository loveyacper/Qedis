#include "QStore.h"
#include "QClient.h"
#include "QConfig.h"
#include "QAOF.h"
#include "QMulti.h"
#include "Log/Logger.h"
#include "QLeveldb.h"
#include <limits>
#include <cassert>


namespace qedis
{

uint32_t QObject::lruclock = static_cast<uint32_t>(::time(nullptr));
    

QObject::QObject(QType t) : type(t)
{
    switch (type)
    {
        case QType_list:
            encoding = QEncode_list;
            break;
                    
        case QType_set:
            encoding = QEncode_set;
            break;
                    
        case QType_sortedSet:
            encoding = QEncode_sset;
            break;
                    
        case QType_hash:
            encoding = QEncode_hash;
            break;
                    
        default:
            encoding = QEncode_invalid;
            break;
    }

    lru = 0;
    value = nullptr;
}
        
QObject::~QObject()
{
    Clear();
}
    
void QObject::Clear()
{
    _FreeValue();
            
    type = QType_invalid;
    encoding = QEncode_invalid;
    lru = 0;
    value = nullptr;
}
        
void QObject::Reset(void* newvalue)
{
    _FreeValue();
    value = newvalue;
}
        
QObject::QObject(QObject&& obj)
{
    _MoveFrom(std::move(obj));
}
        
QObject& QObject::operator= (QObject&& obj)
{
    _MoveFrom(std::move(obj));
    return *this;
}
    
void QObject::_MoveFrom(QObject&& obj)
{
    this->Reset();
            
    this->encoding = obj.encoding;
    this->type = obj.type;
    this->value = obj.value;
    this->lru = obj.lru;
            
    obj.encoding = QEncode_invalid;
    obj.type = QType_invalid;
    obj.value = nullptr;
    obj.lru = 0;
}
        
void QObject::_FreeValue()
{
    switch (encoding)
    {
        case QEncode_raw:
            delete CastString();
            break;
                    
        case QEncode_list:
            delete CastList();
            break;
                    
        case QEncode_set:
            delete CastSet();
            break;
                    
        case QEncode_sset:
            delete CastSortedSet();
            break;
                    
        case QEncode_hash:
            delete CastHash();
            break;
                    
        default:
            break;
    }
}

int QStore::dirty_ = 0;

void QStore::ExpiresDB::SetExpire(const QString& key, uint64_t when)
{
    expireKeys_[key] = when;
}

int64_t QStore::ExpiresDB::TTL(const QString& key, uint64_t now)
{
    if (!QSTORE.ExistsKey(key))
        return ExpireResult::notExist;

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
            return ExpireResult::notExpire;
        
        WRN << "Delete timeout key " << it->first;
        QSTORE.DeleteKey(it->first);
        expireKeys_.erase(it);
        return ExpireResult::expired;
    }
    
    return  ExpireResult::persist;
}

int QStore::ExpiresDB::LoopCheck(uint64_t now)
{
    const int kMaxDel = 100;
    const int kMaxCheck = 2000;

    int  nDel = 0;
    int  nLoop = 0;

    for (auto  it = expireKeys_.begin();
               it!= expireKeys_.end() && nDel < kMaxDel && nLoop < kMaxCheck;
               ++ nLoop)
    {
        if (it->second <= now)
        {
            // time to delete
            INF << "LoopCheck try delete key:" << it->first;
            
            std::vector<QString>  params(2);
            params[0] = "del";
            params[1] = it->first;
            Propogate(params);
            
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
                        cli->SendPacket(reply);
                        errorTarget = true;
                    }
                    else
                    {
                        dst = QSTORE.SetValue(target, QObject::CreateList());
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
                
                cli->SendPacket(reply);
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
         it != blockedClients_.end() && n < 100; )
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
                    scli->SendPacket(reply);
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

const QObject* QStore::GetObject(const QString& key) const
{
    auto db = &store_[dbno_];
    QDB::const_iterator it(db->find(key));
    if (it != db->end())
        return &it->second;

    if (!backends_.empty())
    {
        // if it's in dirty list, it must be deleted, wait sync to backend
        if (waitSyncKeys_[dbno_].count(key))
            return nullptr;

        // load from leveldb, if has, insert to qedis cache
        QObject obj = backends_[dbno_]->Get(key);
        if (obj.type != QType_invalid)
        {
            DBG << "GetKey from leveldb:" << key;

            QObject& realobj = ((*db)[key] = std::move(obj));
            realobj.lru = QObject::lruclock;

            // trick: use lru field to store the remain seconds to be expired.
            unsigned int remainTtlSeconds = obj.lru;
            if (remainTtlSeconds > 0)
                SetExpire(key, ::Now() + remainTtlSeconds * 1000);

            return &realobj;
        }
    }

    return nullptr;
}

bool QStore::DeleteKey(const QString& key)
{
    auto db = &store_[dbno_];
    // add to dirty queue
    if (!waitSyncKeys_.empty())
    {
        waitSyncKeys_[dbno_][key] = nullptr; // null implies delete data
    }

    return db->erase(key) != 0;
}

bool QStore::ExistsKey(const QString& key) const
{
    const QObject* obj = GetObject(key);
    return obj != nullptr;
}

QType  QStore::KeyType(const QString& key) const
{
    const QObject* obj = GetObject(key);
    if (!obj)
        return QType_invalid;
    
    return  QType(obj->type);
}

static bool RandomMember(const QDB& hash, QString& res, QObject** val)
{
    QDB::const_local_iterator it = RandomHashMember(hash);
    
    if (it != QDB::const_local_iterator())
    {
        res = it->first;
        if (val) *val = const_cast<QObject*>(&it->second);
        return true;
    }
    
    return false;
}

QString QStore::RandomKey(QObject** val) const
{
    QString  res;
    if (!store_.empty() && !store_[dbno_].empty())
        RandomMember(store_[dbno_], res, val);

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

QError  QStore::GetValue(const QString& key, QObject*& value, bool touch)
{
    if (touch)
        return GetValueByType(key, value);
    else
        return GetValueByTypeNoTouch(key, value);
}

QError  QStore::GetValueByType(const QString& key, QObject*& value, QType type)
{
    return _GetValueByType(key, value, type, true);
}

QError  QStore::GetValueByTypeNoTouch(const QString& key, QObject*& value, QType type)
{
    return _GetValueByType(key, value, type, false);
}

QError  QStore::_GetValueByType(const QString& key, QObject*& value, QType type, bool touch)
{
    if (_ExpireIfNeed(key, ::Now()) == ExpireResult::expired)
    {
        return QError_notExist;
    }
    
    auto cobj = GetObject(key);
    if (cobj)
    {
        if (type != QType_invalid && type != QType(cobj->type))
        {
            return QError_type;
        }
        else
        {
            value = const_cast<QObject*>(cobj);

            // Do not update if child process exists
            extern pid_t g_qdbPid;
            if (touch && g_rewritePid == -1 && g_qdbPid == -1)
                value->lru = QObject::lruclock;

            return QError_ok;
        }
    }
    else
    {
        return QError_notExist;
    }

    return  QError_ok; // never here
}


QObject* QStore::SetValue(const QString& key, QObject&& value)
{
    auto db = &store_[dbno_];
    QObject& obj = ((*db)[key] = std::move(value));
    obj.lru = QObject::lruclock;

    // put this key to sync list
    if (!waitSyncKeys_.empty())
        waitSyncKeys_[dbno_][key] = &obj;

    return &obj;
}

void QStore::SetExpire(const QString& key, uint64_t when) const
{
    expiresDb_[dbno_].SetExpire(key, when);
}


int64_t QStore::TTL(const QString& key, uint64_t now)
{
    return  expiresDb_[dbno_].TTL(key, now);
}

bool QStore::ClearExpire(const QString& key)
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
    {
        auto timer = TimerManager::Instance().CreateTimer();
        timer->Init(1);
        timer->SetCallback([&, i] () {
                int oldDb = QSTORE.SelectDB(i);
                QSTORE.LoopCheckExpire(::Now());
                QSTORE.SelectDB(oldDb);
        });

        TimerManager::Instance().AddTimer(timer);
    }
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
    {
        auto timer = TimerManager::Instance().CreateTimer();
        timer->Init(3);
        timer->SetCallback([&, i] () {
                int oldDb = QSTORE.SelectDB(i);
                QSTORE.LoopCheckBlocked(::Now());
                QSTORE.SelectDB(oldDb);
        });

        TimerManager::Instance().AddTimer(timer);
    }
}


static void EvictItems()
{
    QObject::lruclock = static_cast<uint32_t>(::time(nullptr));
    QObject::lruclock &= kMaxLRUValue;

    int currentDb = QSTORE.GetDB();
    
    QEDIS_DEFER {
        QSTORE.SelectDB(currentDb);
    };

    int tryCnt = 0;
    size_t usedMem = 0;
    while (tryCnt ++ < 32 && (usedMem = getMemoryInfo(VmRSS)) > g_config.maxmemory)
    {
        if (g_config.noeviction)
        {
            WRN << "noeviction policy, but memory usage exceeds: " << usedMem;
            return;
        }

        for (int dbno = 0; true; ++ dbno)
        {
            if (QSTORE.SelectDB(dbno) == -1)
                break;

            if (QSTORE.DBSize() == 0)
                continue;
        
            QString evictKey;
            uint32_t choosedIdle = 0;
            for (int i = 0; i < g_config.maxmemorySamples; ++ i)
            {
                QObject* val = nullptr;

                auto key = QSTORE.RandomKey(&val);
                if (!val) continue;
                
                auto idle = EstimateIdleTime(val->lru);
                if (evictKey.empty() || choosedIdle < idle)
                {
                    evictKey = std::move(key);
                    choosedIdle = idle;
                }
            }

            if (!evictKey.empty())
            {
                QSTORE.DeleteKey(evictKey);
                WRN << "Evict '" << evictKey << "' in db " << dbno << ", idle time: " << choosedIdle << ", used mem: " << usedMem;
            }
        }
    }
}

uint32_t EstimateIdleTime(uint32_t lru)
{
    if (lru <= QObject::lruclock)
        return QObject::lruclock - lru;
    else
        return (kMaxLRUValue - lru) + QObject::lruclock;
}


void QStore::InitEvictionTimer()
{
    auto timer = TimerManager::Instance().CreateTimer();
    timer->Init(1000); // emit eviction every second.
    timer->SetCallback([] () {
        EvictItems();
    });

    TimerManager::Instance().AddTimer(timer);
}

void QStore::InitDumpBackends()
{
    assert (waitSyncKeys_.empty());

    if (g_config.backend == BackEndNone)
        return;

    if (g_config.backend == BackEndLeveldb)
    {
        waitSyncKeys_.resize(store_.size());
        for (size_t i = 0; i < store_.size(); ++ i)
        {
            std::unique_ptr<QLeveldb> db(new QLeveldb);
            QString dbpath = g_config.backendPath + std::to_string(i);
            if (!db->Open(dbpath.data()))
                assert(false);
            else
                USR << "Open leveldb " << dbpath;

            backends_.push_back(std::move(db));
        }
    }
    else 
    {
        // ERROR: unsupport backend
        return;
    }
        
    for (int i = 0; i < static_cast<int>(backends_.size()); ++ i)
    {
        auto timer = TimerManager::Instance().CreateTimer();
        timer->Init(1000 / g_config.backendHz);
        timer->SetCallback([&, i] () {
                int oldDb = QSTORE.SelectDB(i);
                QSTORE.DumpToBackends(i);
                QSTORE.SelectDB(oldDb);
        });

        TimerManager::Instance().AddTimer(timer);
    }
}

void  QStore::DumpToBackends(int dbno)
{
    if (static_cast<int>(waitSyncKeys_.size()) <= dbno)
        return;

    const int kMaxSync = 100;
    int processed = 0;
    auto& dirtyKeys = waitSyncKeys_[dbno];
            
    uint64_t now = ::Now();
    for (auto it = dirtyKeys.begin(); processed++ < kMaxSync && it != dirtyKeys.end(); )
    {
        // check ttl
        int64_t when = QSTORE.TTL(it->first, now);

        if (it->second && when != QStore::ExpireResult::expired)
        {
            assert (when != QStore::ExpireResult::notExpire);

            if (when > 0)
                when += now;

            backends_[dbno]->Put(it->first, *it->second, when);
            DBG << "UPDATE leveldb key " << it->first << ", when = " << when;
        }
        else
        {
            backends_[dbno]->Delete(it->first);
            DBG << "DELETE leveldb key " << it->first;
        }
            
        it = dirtyKeys.erase(it);
    }
}
   
void QStore::AddDirtyKey(const QString& key)
{
    // put this key to sync list
    if (!waitSyncKeys_.empty())
    {
        QObject* obj = nullptr;
        GetValue(key, obj);
        waitSyncKeys_[dbno_][key] = obj;
    }
}
    
void QStore::AddDirtyKey(const QString& key, const QObject* value)
{
    // put this key to sync list
    if (!waitSyncKeys_.empty())
        waitSyncKeys_[dbno_][key] = value;
}

std::vector<QString>  g_dirtyKeys;

void Propogate(const std::vector<QString>& params)
{
    assert (!params.empty());

    if (!g_dirtyKeys.empty())
    {
        for (const auto& k : g_dirtyKeys)
        {
            ++ QStore::dirty_;
            QMulti::Instance().NotifyDirty(QSTORE.GetDB(), k);
            
            QSTORE.AddDirtyKey(k); // TODO optimize
        }
        g_dirtyKeys.clear();
    }
    else if (params.size() > 1)
    {
        ++ QStore::dirty_;
        QMulti::Instance().NotifyDirty(QSTORE.GetDB(), params[1]);
        QSTORE.AddDirtyKey(params[1]); // TODO optimize
    }

    if (g_config.appendonly)
        QAOFThreadController::Instance().SaveCommand(params, QSTORE.GetDB());

    QREPL.SendToSlaves(params);
}

void Propogate(int dbno, const std::vector<QString>& params)
{
    QMulti::Instance().NotifyDirtyAll(dbno);
    Propogate(params);
}

}
