#include "QStore.h"
#include "QClient.h"
#include "Log/Logger.h"
#include <iostream>
#include <limits>
#include <cassert>


void QStore::ExpiresDB::SetExpire(const QString& key, uint64_t when)
{
    m_expireKeys[key] = when;
    INF << "Set timeout key " << key.c_str() << ", timeout is " << when;
}

int64_t  QStore::ExpiresDB::TTL(const QString& key, uint64_t now)
{
    if (!QSTORE.ExistsKey(key))
    {
        return -2; // not exist key
    }

    switch (ExpireIfNeed(key, now))
    {
        case ExpireResult::expired:
            return -2;
            
        case ExpireResult::persist:
            return -1;
            
        default:
            break;
    }
    
    auto it(m_expireKeys.find(key));
    return static_cast<int64_t>(it->second - now);
}

bool QStore::ExpiresDB::ClearExpire(const QString& key)
{
    return ExpireResult::expired == ExpireIfNeed(key, std::numeric_limits<uint64_t>::max());
}

QStore::ExpireResult  QStore::ExpiresDB::ExpireIfNeed(const QString& key, uint64_t now)
{
    auto    it(m_expireKeys.find(key));
    
    if (it != m_expireKeys.end())
    {
        if (it->second > now)
            return ExpireResult::notExpire; // not expire
        
        WRN << "Delete timeout key " << it->first.c_str() << ", timeout is " << it->second;
        m_expireKeys.erase(it);
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

    for (auto  it = m_expireKeys.begin();
               it!= m_expireKeys.end() && nDel < MAX_DEL && nLoop < MAX_CHECK;
               ++ nLoop)
    {
        if (it->second <= now)
        {
            // time to delete
            WRN << "LoopCheck try delete key " << it->first.c_str() << ", " << ::Now();
            
            QSTORE.DeleteKey(it->first);
            m_expireKeys.erase(it ++);

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
    int oldDb = QSTORE.SelectDB(m_dbno);
    QSTORE.LoopCheckExpire(::Now());
    QSTORE.SelectDB(oldDb);
    return  true;
}

bool QStore::BlockedClients::BlockClient(const QString& key, QClient* client, uint64_t timeout)
{
    if (!client->WaitFor(key))
    {
        ERR << key << " is already waited by " << client->GetName();
        return  false;
    }
        
    Clients& clients = m_blockedClients[key];
    clients.push_back(Clients::value_type(std::static_pointer_cast<QClient>(client->shared_from_this()), timeout));
    INF << key << " is waited by " << client->GetName() << ", timeout " << timeout;
    return true;
}

size_t QStore::BlockedClients::UnblockClient(QClient* client)
{
    size_t n = 0;
    const auto& keys = client->WaitingKeys();
    
    for (const auto& key : keys)
    {
        Clients&  clients = m_blockedClients[key];
        assert(!clients.empty());
        
        bool  found = false;
        for (Clients::iterator it(clients.begin()); it != clients.end(); ++ it)
        {
            auto  cli(it->first.lock());
            if (cli && cli.get() == client)
            {
                INF << "unblock " << client->GetName() << " for key " << key;
                clients.erase(it);
                
              //  if (clients.empty())
                //    m_blockedClients.erase(key);
                
                ++ n;
                found = true;
                break;
            }
        }
    
        if (!found)
            ERR << "FAILED: unblock " << client->GetName() << " for key " << key;
    }
    
    client->ClearWaitingKeys();
    return n;
}


size_t  QStore::BlockedClients::ServeClient(const QString& key, const PLIST& list)
{
    assert(!list->empty());
    
    auto it = m_blockedClients.find(key);
    if (it == m_blockedClients.end())
        return 0;

    Clients& clients = it->second;
    if (clients.empty())
        return 0;
    
    size_t nServed = 0;
        
    while (!list->empty())
    {
        auto cli(clients.begin()->first.lock());
        if (cli)
        {
            UnboundedBuffer reply;
            
            PreFormatMultiBulk(2, reply);
            FormatBulk(key.c_str(), key.size(), reply);
            FormatBulk(list->begin()->c_str(), list->begin()->size(), reply);
                
            cli->SendPacket(reply.ReadAddr(), reply.ReadableSize());
            list->pop_front();
            INF << "server client " << cli->GetName() << " list member " << *(list->begin());
            
            UnblockClient(cli.get());
            ++ nServed;
        }
    }
    
    return nServed;
}
    
int QStore::BlockedClients::LoopCheck(uint64_t now)
{
    int n = 0;
    for (auto it(m_blockedClients.begin()); it != m_blockedClients.end() && n < 100; )
    {
        Clients& clients = it->second;
        for (auto cli(clients.begin()); cli != clients.end(); )
        {
            if (cli->second < now) // timeout
            {
                INF << "client blocked is timeout";
                ++ n;
                
                auto scli(cli->first.lock());
                if (scli)
                {
                    UnboundedBuffer reply;
                    FormatNull(reply);
                    scli->SendPacket(reply.ReadAddr(), reply.ReadableSize());
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
            m_blockedClients.erase(it ++);
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
    int oldDb = QSTORE.SelectDB(m_dbno);
    QSTORE.LoopCheckBlocked(::Now());
    QSTORE.SelectDB(oldDb);
    return  true;
}

QStore& QStore::Instance()
{
    static QStore  store;
    return store;
}

int  QStore::LoopCheckExpire(uint64_t now)
{
    return m_expiresDb[m_dbno].LoopCheck(now);
}

int  QStore::LoopCheckBlocked(uint64_t now)
{
    return m_blockedClients[m_dbno].LoopCheck(now);
}


int QStore::SelectDB(int dbno)
{
    if (dbno >= 0 && dbno < static_cast<int>(m_store.size()))
    {
        int oldDb = m_dbno;

        m_dbno    = dbno;
        m_db      = &m_store[dbno];
        return  oldDb;
    }
        
    return  -1;
}


bool QStore::DeleteKey(const QString& key)
{
    return m_db->erase(key) != 0;
}

bool QStore::ExistsKey(const QString& key) const
{
    return  m_db->count(key) != 0;
}

QType  QStore::KeyType(const QString& key) const
{
    QDB::const_iterator it(m_db->find(key));
    if (it == m_db->end())
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
    if (m_db && !m_db->empty())
    {
        RandomMember(*m_db, res);
    }

    return  res;
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
    
    QDB::iterator    it(m_db->find(key));

    if (it != m_db->end())
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
    QObject& obj = ((*m_db)[key] = value);
    return &obj;
}

bool QStore::SetValueIfNotExist(const QString& key, const QObject& value)
{
    QDB::iterator    it(m_db->find(key));

    if (it == m_db->end())
        (*m_db)[key] = value;

    return it == m_db->end();
}


void    QStore::SetExpire(const QString& key, uint64_t when)
{
    m_expiresDb[m_dbno].SetExpire(key, when);
}


int64_t QStore::TTL(const QString& key, uint64_t now)
{
    return  m_expiresDb[m_dbno].TTL(key, now);
}

bool    QStore::ClearExpire(const QString& key)
{
    return m_expiresDb[m_dbno].ClearExpire(key);
}

QStore::ExpireResult QStore::_ExpireIfNeed(const QString& key, uint64_t now)
{
    return  m_expiresDb[m_dbno].ExpireIfNeed(key, now);
}

void    QStore::InitExpireTimer()
{
    for (int i = 0; i < static_cast<int>(m_expiresDb.size()); ++ i)
        TimerManager::Instance().AddTimer(PTIMER(new ExpireTimer(i)));
}

bool    QStore::BlockClient(const QString& key, QClient* client, uint64_t timeout)
{
    return m_blockedClients[m_dbno].BlockClient(key, client, timeout);
}
size_t  QStore::UnblockClient(QClient* client)
{
    return m_blockedClients[m_dbno].UnblockClient(client);
}
size_t  QStore::ServeClient(const QString& key, const PLIST& list)
{
    return m_blockedClients[m_dbno].ServeClient(key, list);
}

void    QStore::InitBlockedTimer()
{
    for (int i = 0; i < static_cast<int>(m_blockedClients.size()); ++ i)
        TimerManager::Instance().AddTimer(PTIMER(new BlockedListTimer(i)));
}