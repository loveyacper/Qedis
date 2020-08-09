#include "QRocksdb.h"
#include "rocksdb/db.h"
#include "Log/Logger.h"
#include "UnboundedBuffer.h"
#include "QConfig.h"

namespace qedis 
{

QRocksdb::QRocksdb() : db_(nullptr)
{
}

QRocksdb::~QRocksdb()
{
}

bool QRocksdb::IsOpen() const 
{

    return db_ != nullptr;
}

bool QRocksdb::Open(const char* path)
{
    rocksdb::Options options;
    // set options
    options.create_if_missing = true;
    options.write_buffer_size = g_config.write_buffer_size;
    options.level0_file_num_compaction_trigger = g_config.level0_file_num_compaction_trigger;
    options.level0_slowdown_writes_trigger = g_config.level0_slowdown_writes_trigger;
    options.level0_stop_writes_trigger = g_config.level0_stop_writes_trigger;
    options.max_write_buffer_number = g_config.max_write_buffer_number;
    options.min_write_buffer_number_to_merge = g_config.min_write_buffer_number_to_merge;
    options.max_background_jobs = g_config.max_background_jobs;
    options.max_subcompactions = g_config.max_subcompactions;
    options.max_open_files = g_config.max_open_files;
    options.max_log_file_size = g_config.max_log_file_size;
    options.max_manifest_file_size = g_config.max_manifest_file_size;
    options.enable_pipelined_write = g_config.enable_pipelined_write;
    if (!g_config.enable_pipelined_write) {
        options.unordered_write = g_config.unordered_write;
    }
    options.two_write_queues = g_config.two_write_queues;

    auto s = rocksdb::DB::Open(options, path, &db_);
    if (!s.ok()) {
        ERR << "Open db_ failed:" << s.ToString();
    }

    return s.ok();
}

QObject QRocksdb::Get(const QString& key)
{
    std::string value;
    auto status = db_->Get(rocksdb::ReadOptions(), db_->DefaultColumnFamily(), rocksdb::Slice(key.data(), key.size()), &value);
    if (!status.ok())
        return QObject(QType_invalid);

    int64_t remainTtlSeconds = 0;
    QObject obj = _DecodeObject(value.data(), value.size(), remainTtlSeconds);

    if (remainTtlSeconds > 0)
        obj.lru = static_cast<uint32_t>(remainTtlSeconds);
    else
        obj.lru = 0;
    return obj;
}

bool QRocksdb::Put(const QString& key)
{
    QObject* obj;
    QError ok = QSTORE.GetValue(key, obj, false);

    if (ok != QError_ok)
        return false;

    uint64_t now = ::Now();
    int64_t ttl = QSTORE.TTL(key, now);
    if (ttl > 0) 
        ttl += now;
    else if (ttl == QStore::ExpireResult::expired)
        return false;
    
    return Put(key, *obj, ttl);
}

bool QRocksdb::Put(const QString& key, const QObject& obj, int64_t absttl)
{
    UnboundedBuffer v;
    _EncodeObject(obj, absttl, v);

    rocksdb::Slice lkey(key.data(), key.size());
    rocksdb::Slice lval(v.ReadAddr(), v.ReadableSize());

    auto s = db_->Put(rocksdb::WriteOptions(), lkey, lval);
    return s.ok();
}

bool QRocksdb::Delete(const QString& key)
{
    rocksdb::Slice lkey(key.data(), key.size());
    auto s = db_->Delete(rocksdb::WriteOptions(), lkey);
    return s.ok();
}

void QRocksdb::_EncodeObject(const QObject& obj, int64_t absttl, UnboundedBuffer& v)
{
    // value format: | ttl flag 1byte| ttl 8bytes if has|type 1byte| object contents

    // write ttl, if has
    int8_t ttlflag = (absttl > 0 ? 1 : 0);
    v.Write(&ttlflag, sizeof ttlflag);
    if (ttlflag)
        v.Write(&absttl, sizeof absttl);
    
    // write type
    int8_t type = obj.type;
    v.Write(&type, sizeof type);

    switch (obj.encoding)
    {
        case QEncode_raw:
        case QEncode_int:
            {
                auto str = GetDecodedString(&obj);
                _EncodeString(*str, v);
            }
            break;

        case QEncode_list:
            {
                _EncodeList(obj.CastList(), v);
            }
            break;
        
        case QEncode_set:
            {
                _EncodeSet(obj.CastSet(), v);
            }
            break;

        case QEncode_hash:
            {
                _EncodeHash(obj.CastHash(), v);
            }
            break;

        case QEncode_sset:
            {
                _EncodeSSet(obj.CastSortedSet(), v);
            }
            break;

        default:
            break;   
    }
}

void QRocksdb::_EncodeString(const QString& str, UnboundedBuffer& v)
{
    // write size
    auto len = static_cast<uint32_t>(str.size());
    v.Write(&len, 4);
    // write content
    v.Write(str.data(), len);
}

void QRocksdb::_EncodeHash(const PHASH& h, UnboundedBuffer& v) 
{
    // write size
    auto len = static_cast<uint32_t>(h->size());
    v.Write(&len, 4);

    for (const auto& e : *h)
    {
        _EncodeString(e.first, v);
        _EncodeString(e.second, v);
    }
}

void QRocksdb::_EncodeList(const PLIST& l, UnboundedBuffer& v)
{
    // write size
    auto len = static_cast<uint32_t>(l->size());
    v.Write(&len, 4);

    for (const auto& e : *l)
    {
        _EncodeString(e, v);
    }
}

void QRocksdb::_EncodeSet(const PSET& s, UnboundedBuffer& v)
{
    auto len = static_cast<uint32_t>(s->size());
    v.Write(&len, 4);

    for (const auto& e : *s) 
    {
        _EncodeString(e, v);
    }
}

void QRocksdb::_EncodeSSet(const PSSET& ss, UnboundedBuffer& v) 
{
    auto len = static_cast<uint32_t>(ss->Size());
    v.Write(&len, 4);

    for (const auto& e : *ss) 
    {
        _EncodeString(e.first, v);

        auto s(std::to_string(e.second));
        _EncodeString(s, v);
    }
}

QObject QRocksdb::_DecodeObject(const char* data, size_t len, int64_t& remainTtl)
{
    // | type 1byte | ttl flag 1byte| ttl 8bytes, if has|

    remainTtl = 0;

    size_t offset = 0;

    int8_t hasttl = *(int8_t*)(data + offset);
    offset += sizeof hasttl;

    int64_t absttl = 0;
    if (hasttl)
    {
        absttl = *(int64_t*)(data + offset);
        offset += sizeof absttl;
    }

    if (absttl != 0)
    {
        int64_t now = static_cast<uint64_t>(::Now());
        if (absttl <= now)
        {
            DBG << "Load from rocksdb is timeout " << absttl;
            return QObject(QType_invalid);
        }
        else
        {
            remainTtl = (absttl - now) / 1000;
            INF << "Load from rocksdb remainTtlSeconds: " << remainTtl;
        }
    }

    int8_t type = *(int8_t*)(data + offset);
    offset += sizeof type;

    switch (type)
    {
        case QType_string:
        {
            return QObject::CreateString(_DecodeString(data + offset, len - offset));
        }
        case QType_list:
        {
            return _DecodeList(data + offset, len - offset);
        }
        case QType_set:
        {
            return _DecodeSet(data + offset, len - offset);
        }
        case QType_sortedSet:
        {
            return _DecodeSSet(data + offset, len - offset);
        }
        case QType_hash:
        {
            return _DecodeHash(data + offset, len - offset);
        }

        default:
            break;
    }

    assert(false);
    return QObject(QType_invalid);
}

QString QRocksdb::_DecodeString(const char* data, size_t len)
{
    assert(len > 4);
    // read length
    uint32_t slen = *(uint32_t*)(data);
    // read content
    const char* sdata = data + 4;

    return QString(sdata, slen);
}

QObject QRocksdb::_DecodeHash(const char* data, size_t len)
{
    assert(len >= 4);
    uint32_t hlen = *(uint32_t*)(data);

    QObject obj(QObject::CreateHash());
    PHASH hash(obj.CastHash());

    size_t offset = 4;
    for (uint32_t i = 0; i < hlen; ++ i)
    {
        auto key = _DecodeString(data + offset, len - offset);
        offset += key.size() + 4;

        auto value = _DecodeString(data + offset, len - offset);
        offset += value.size() + 4;

        hash->insert(QHash::value_type(key, value));
        DBG << "Load from rocksdb: hash key : " << key << " val : " << value;
    }

    return obj;
}

QObject QRocksdb::_DecodeList(const char* data, size_t len)
{
    assert(len >= 4);
    uint32_t llen = *(uint32_t*)(data);

    QObject obj(QObject::CreateList());
    PLIST list(obj.CastList());

    size_t offset = 4;
    for (uint32_t i = 0; i < llen; ++ i)
    {
        auto elem = _DecodeString(data + offset, len - offset);
        offset += elem.size() + 4; 

        list->push_back(elem);
        DBG << "Load list elem from leveldb: " << elem;       
    }

    return obj;
}

QObject QRocksdb::_DecodeSet(const char* data, size_t len)
{
    assert(len >= 4);
    uint32_t slen = *(uint32_t*)(data);

    QObject obj(QObject::CreateSet());
    PSET set(obj.CastSet());

    size_t offset = 4;
    for (uint32_t i = 0; i < slen; ++ i)
    {
        auto elem = _DecodeString(data + offset, len - offset);
        offset += elem.size() + 4;

        set->insert(elem);
        DBG << "Load set elem from rocksdb: " << elem;
    }

    return obj;
}

QObject QRocksdb::_DecodeSSet(const char* data, size_t len)
{
    assert(len >= 4);
    uint32_t sslen = *(uint32_t*)(data);

    QObject obj(QObject::CreateSSet());
    PSSET sset(obj.CastSortedSet());

    size_t offset = 4;
    for (uint32_t i = 0; i < sslen; ++ i)
    {
        auto member = _DecodeString(data + offset, len - offset);
        offset += member.size() + 4;

        auto scoreStr = _DecodeString(data + offset, len - offset);
        offset += scoreStr.size() + 4;

        double score = std::stod(scoreStr);
        sset->AddMember(member, static_cast<long>(score));

        DBG << "Load rocksdb sset member : " << member << " score : " << score;
    }

    return obj;
}

}