#ifndef BERT_QSTORE_H
#define BERT_QSTORE_H

#include "QCommon.h"
#include "QSet.h"
#include "QSortedSet.h"
#include "QHash.h"
#include "QList.h"
#include "Timer.h"
#include "QDumpInterface.h"

#include <vector>
#include <map>
#include <memory>

namespace qedis
{

using PSTRING = QString*;
using PLIST = QList*;
using PSET = QSet*;
using PSSET = QSortedSet*;
using PHASH = QHash*;

    
static const int kLRUBits = 24;
static const uint32_t kMaxLRUValue = (1 << kLRUBits) - 1;

uint32_t EstimateIdleTime(uint32_t lru);

struct QObject
{
public:
    static uint32_t lruclock;

    unsigned int type : 4;
    unsigned int encoding : 4;
    unsigned int lru : kLRUBits;

    void* value;
    
    explicit
    QObject(QType = QType_invalid);
    ~QObject();

    QObject(const QObject& obj) = delete;
    QObject& operator= (const QObject& obj) = delete;
    
    QObject(QObject&& obj);
    QObject& operator= (QObject&& obj);
    
    void Clear();
    void Reset(void* newvalue = nullptr);
    
    static QObject CreateString(const QString& value);
    static QObject CreateString(long value);
    static QObject CreateList();
    static QObject CreateSet();
    static QObject CreateSSet();
    static QObject CreateHash();
    
    PSTRING  CastString()       const { return reinterpret_cast<PSTRING>(value); }
    PLIST    CastList()         const { return reinterpret_cast<PLIST>(value);   }
    PSET     CastSet()          const { return reinterpret_cast<PSET>(value);    }
    PSSET    CastSortedSet()    const { return reinterpret_cast<PSSET>(value); }
    PHASH    CastHash()         const { return reinterpret_cast<PHASH>(value);   }
   
private:
    void _MoveFrom(QObject&& obj);
    void _FreeValue();
};

class QClient;

using QDB = std::unordered_map<QString, QObject, qedis::Hash>;


const int kMaxDbNum = 65536;

class QStore
{
public:
    static QStore& Instance();
    
    QStore(const QStore& ) = delete;
    void operator= (const QStore& ) = delete;
    
    void Init(int dbNum = 16);

    int SelectDB(int dbno);
    int GetDB() const;
    
    // Key operation
    bool DeleteKey(const QString& key);
    bool ExistsKey(const QString& key) const;
    QType  KeyType(const QString& key) const;
    QString RandomKey(QObject** val = nullptr) const;
    size_t DBSize() const { return store_[dbno_].size(); }
    size_t ScanKey(size_t cursor, size_t count, std::vector<QString>& res) const;

    // iterator
    QDB::const_iterator begin() const   { return store_[dbno_].begin(); }
    QDB::const_iterator end()   const   { return store_[dbno_].end(); }
    QDB::iterator       begin()         { return store_[dbno_].begin(); }
    QDB::iterator       end()           { return store_[dbno_].end(); }
    
    const QObject* GetObject(const QString& key) const;
    QError GetValue(const QString& key, QObject*& value, bool touch = true);
    QError GetValueByType(const QString& key, QObject*& value, QType type = QType_invalid);
    // do not update lru time
    QError  GetValueByTypeNoTouch(const QString& key, QObject*& value, QType type = QType_invalid);

    QObject* SetValue(const QString& key, QObject&& value);

    // for expire key
    enum ExpireResult : std::int8_t
    {
        notExpire=  0,
        persist  = -1,
        expired  = -2,
        notExist = -2,
    };
    void    SetExpire(const QString& key, uint64_t when) const;
    void    SetExpireAfter(const QString& key, uint64_t ttl) const;
    int64_t TTL(const QString& key, uint64_t now);
    bool    ClearExpire(const QString& key);
    int     LoopCheckExpire(uint64_t now);
    void    InitExpireTimer();
    
    // danger cmd
    void    ClearCurrentDB() { store_[dbno_].clear(); }
    void    ResetDb();
    
    // for blocked list
    bool    BlockClient(const QString& key,
                        QClient* client,
                        uint64_t timeout,
                        ListPosition pos,
                        const QString* dstList = 0);
    size_t  UnblockClient(QClient* client);
    size_t  ServeClient(const QString& key, const PLIST& list);
    
    int     LoopCheckBlocked(uint64_t now);
    void    InitBlockedTimer();
    
    size_t  BlockedSize() const;
    
    static  int dirty_;

    // eviction timer for lru
    void    InitEvictionTimer();
    // for backends
    void    InitDumpBackends();
    void    DumpToBackends(int dbno);
    void    AddDirtyKey(const QString& key);
    void    AddDirtyKey(const QString& key, const QObject* value);
    
private:
    QStore() : dbno_(0)
    {
    }
    
    QError  _GetValueByType(const QString& key, QObject*& value, QType type = QType_invalid, bool touch = true);

    ExpireResult    _ExpireIfNeed(const QString& key, uint64_t now);
    
    class ExpiresDB
    {
    public:
        void SetExpire(const QString& key, uint64_t when);
        int64_t TTL(const QString& key, uint64_t now);
        bool ClearExpire(const QString& key);
        ExpireResult ExpireIfNeed(const QString& key, uint64_t now);

        int LoopCheck(uint64_t now);
        
    private:
        using Q_EXPIRE_DB = std::unordered_map<QString, uint64_t, Hash>;
        Q_EXPIRE_DB expireKeys_;  // all the keys to be expired, unorder.
    };
    
    class BlockedClients
    {
    public:
        bool BlockClient(const QString& key,
                            QClient* client,
                            uint64_t timeout,
                            ListPosition  pos,
                            const QString* dstList = 0);
        size_t UnblockClient(QClient* client);
        size_t ServeClient(const QString& key, const PLIST& list);
        
        int LoopCheck(uint64_t now);
        size_t Size() const { return blockedClients_.size(); }
    private:
        using Clients = std::list<std::tuple<std::weak_ptr<QClient>, uint64_t, ListPosition> >;
        using WaitingList = std::unordered_map<QString, Clients, Hash>;
        
        WaitingList blockedClients_;
    };

    QError _SetValue(const QString& key, QObject& value, bool exclusive = false);

    // Because GetObject() must be const, so mutable them
    mutable std::vector<QDB> store_;
    mutable std::vector<ExpiresDB> expiresDb_;
    std::vector<BlockedClients> blockedClients_;
    std::vector<std::unique_ptr<QDumpInterface> > backends_;
        
    using ToSyncDb = std::unordered_map<QString, const QObject*, Hash>;
    std::vector<ToSyncDb> waitSyncKeys_;
    int dbno_;
};

#define QSTORE  QStore::Instance()

// ugly, but I don't want to write signalModifiedKey() every where
extern std::vector<QString> g_dirtyKeys;
extern void Propogate(const std::vector<QString>& params);
extern void Propogate(int dbno, const std::vector<QString>& params);
    
}

#endif

