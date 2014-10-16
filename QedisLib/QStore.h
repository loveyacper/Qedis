#ifndef BERT_QSTORE_H
#define BERT_QSTORE_H

#include "QCommon.h"
#include "QSet.h"
#include "QSortedSet.h"
#include "QHash.h"
#include "QList.h"
#include <map>
#include "SmartPtr/SharedPtr.h"
#include "Timer.h"

#include <vector>

typedef SharedPtr<QString>      PSTRING;
typedef SharedPtr<QList>        PLIST;
typedef SharedPtr<QSet>         PSET;
typedef SharedPtr<QSortedSet>   PSSET;
typedef SharedPtr<QHash>        PHASH;

struct  QObject
{
    unsigned int type : 4;
    unsigned int nouse: 2;
    unsigned int encoding: 4;
    unsigned int lru  : 22;

    SharedPtr<void>  value;
    
    explicit QObject(QType  t = QType_invalid) : type(t)
    {
        encoding = QEncode_invalid;
        nouse = 0;
        lru   = 0;
    }

    PSTRING  CastString()       const { return StaticPointerCast<QString>(value); }
    PLIST    CastList()         const { return StaticPointerCast<QList>(value);   }
    PSET     CastSet()          const { return StaticPointerCast<QSet>(value);    }
    PSSET    CastSortedSet()    const { return StaticPointerCast<QSortedSet>(value); }
    PHASH    CastHash()         const { return StaticPointerCast<QHash>(value);   }
};

class QClient;

#ifdef __APPLE__
typedef std::unordered_map<QString, QObject,
        my_hash,
        std::equal_to<QString> >  QDB;

typedef std::unordered_map<QString, uint64_t>  Q_EXPIRE_DB;

typedef std::unordered_map<QString, SharedPtr<QClient>,
        my_hash,
        std::equal_to<QString> >  QCLIENTS;
#else
typedef std::tr1::unordered_map<QString, QObject>  QDB;

typedef std::tr1::unordered_map<QString, uint64_t>  Q_EXPIRE_DB;

typedef std::tr1::unordered_map<QString, SharedPtr<QClient> >  QCLIENTS;
#endif

class ExpireTimer  : public Timer
{
public:
    ExpireTimer(int db) : Timer(1)
    {
        m_dbno = db;
    }
private:
    int  m_dbno;
    bool _OnTimer();
};


class QStore
{
public:
    static QStore& Instance();

    QStore(int dbNum = 16) : m_store(dbNum), m_expiresDb(dbNum), m_dbno(0)
    {
        m_db   = &m_store[0];
    }

    int SelectDB(int dbno);
  
    // Key operation
    bool DeleteKey(const QString& key);
    bool ExistsKey(const QString& key) const;
    QType  KeyType(const QString& key) const;
    QString RandomKey() const;
    size_t DBSize() const { return m_db->size(); }

    // iterator
    QDB::const_iterator begin() const   { return m_db->begin(); }
    QDB::const_iterator end()   const   { return m_db->end(); }
    QDB::iterator       begin()         { return m_db->begin(); }
    QDB::iterator       end()           { return m_db->end(); }
    
    QError  GetValue(const QString& key, QObject*& value);
    QError  GetValueByType(const QString& key, QObject*& value, QType  type = QType_invalid);
    QObject*SetValue(const QString& key, const QObject& value);
    bool    SetValueIfNotExist(const QString& key, const QObject& value);

    // for expire key
    void    SetExpire(const QString& key, uint64_t when);
    int64_t TTL(const QString& key, uint64_t now) const;
    bool    ClearExpire(const QString& key);
    bool    ExpireIfNeed(const QString& key, uint64_t now);
    int     LoopCheck(uint64_t now);
    void    InitExpireTimer();

private:
    class QExpiresDB
    {
    public:
        void    SetExpire(const QString& key, uint64_t when);
        int64_t TTL(const QString& key, uint64_t now) const;
        bool    ClearExpire(const QString& key);
        bool    ExpireIfNeed(const QString& key, uint64_t now);

        int     LoopCheck(uint64_t now);
        
    private:
        Q_EXPIRE_DB            m_expireKeys;  // all the keys to be expired, unorder.
    };

    QError  _SetValue(const QString& key, QObject& value, bool exclusive = false);

    std::vector<QDB>  m_store;
    std::vector<QExpiresDB> m_expiresDb;
    int               m_dbno;
    QDB              *m_db;
};

#define QSTORE  QStore::Instance()

#endif

