#ifndef BERT_QSTORE_H
#define BERT_QSTORE_H

#include "QCommon.h"
#include "QSet.h"
#include "QSortedSet.h"
#include "QHash.h"
#include "QList.h"
#include "Timer.h"

#include <vector>
#include <map>
#include <memory>

namespace qedis
{

using PSTRING = std::shared_ptr<QString>;
using PLIST = std::shared_ptr<QList>;
using PSET = std::shared_ptr<QSet>;
using PSSET = std::shared_ptr<QSortedSet>;
using PHASH = std::shared_ptr<QHash>;

    
static const int kLRUBits = 24;
static const uint32_t kMaxLRUValue = (1 << kLRUBits) - 1;

uint32_t EstimateIdleTime(uint32_t lru);

struct  QObject
{
    static uint32_t lruclock;

    unsigned int type : 4;
    unsigned int encoding : 4;
    unsigned int lru : kLRUBits;

    std::shared_ptr<void>  value;
    
    explicit QObject(QType  t = QType_invalid) : type(t)
    {
        switch (type)
        {
            case QType_list:
                encoding = QEncode_list;
                break;
                
            case QType_hash:
                encoding = QEncode_hash;
                break;
                
            case QType_set:
                encoding = QEncode_set;
                break;

            case QType_sortedSet:
                encoding = QEncode_sset;
                break;
                
            default:
                encoding = QEncode_invalid;
                break;
        }
        
        lru   = 0;
    }
    
    QObject(const QObject& obj) = default;
    QObject& operator= (const QObject& obj) =  default;
    
    PSTRING  CastString()       const { return std::static_pointer_cast<QString>(value); }
    PLIST    CastList()         const { return std::static_pointer_cast<QList>(value);   }
    PSET     CastSet()          const { return std::static_pointer_cast<QSet>(value);    }
    PSSET    CastSortedSet()    const { return std::static_pointer_cast<QSortedSet>(value); }
    PHASH    CastHash()         const { return std::static_pointer_cast<QHash>(value);   }
};

class QClient;

using QDB = std::unordered_map<QString, QObject,
        my_hash,
        std::equal_to<QString> >;


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
    
    QError  GetValue(const QString& key, QObject*& value, bool touch = true);
    QError  GetValueByType(const QString& key, QObject*& value, QType type = QType_invalid);
    // do not update lru time
    QError  GetValueByTypeNoTouch(const QString& key, QObject*& value, QType type = QType_invalid);

    QObject*SetValue(const QString& key, const QObject& value);
    bool    SetValueIfNotExist(const QString& key, const QObject& value);

    // for expire key
    enum ExpireResult : std::int8_t
    {
        notExpire=  0,
        persist  = -1,
        expired  = -2,
        notExist = -2,
    };
    void    SetExpire(const QString& key, uint64_t when);
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
    
private:
    QStore() : dbno_(0)
    {
    }
    
    QError  _GetValueByType(const QString& key, QObject*& value, QType type = QType_invalid, bool touch = true);

    ExpireResult    _ExpireIfNeed(const QString& key, uint64_t now);
    
    class ExpiresDB
    {
    public:
        void    SetExpire(const QString& key, uint64_t when);
        int64_t TTL(const QString& key, uint64_t now);
        bool    ClearExpire(const QString& key);
        ExpireResult    ExpireIfNeed(const QString& key, uint64_t now);

        int     LoopCheck(uint64_t now);
        
    private:
        using Q_EXPIRE_DB = std::unordered_map<QString, uint64_t,
                                    my_hash,
                                    std::equal_to<QString> >;
        Q_EXPIRE_DB            expireKeys_;  // all the keys to be expired, unorder.
    };
    
    class BlockedClients
    {
    public:
        bool    BlockClient(const QString& key,
                            QClient* client,
                            uint64_t timeout,
                            ListPosition  pos,
                            const QString* dstList = 0);
        size_t  UnblockClient(QClient* client);
        size_t  ServeClient(const QString& key, const PLIST& list);
        
        int    LoopCheck(uint64_t now);
        size_t Size() const { return blockedClients_.size(); }
    private:
        using Clients = std::list<std::tuple<std::weak_ptr<QClient>, uint64_t, ListPosition> >;
        using WaitingList = std::unordered_map<QString, Clients>;
        
        WaitingList  blockedClients_;
    };

    QError  _SetValue(const QString& key, QObject& value, bool exclusive = false);

    std::vector<QDB>  store_;
    std::vector<ExpiresDB> expiresDb_;
    std::vector<BlockedClients> blockedClients_;
    int dbno_;
};

#define QSTORE  QStore::Instance()

// ugly, but I don't want to write signalModifiedKey() every where
extern std::vector<QString> g_dirtyKeys;
extern void Propogate(const std::vector<QString>& params);
extern void Propogate(int dbno, const std::vector<QString>& params);
    
}

#endif

