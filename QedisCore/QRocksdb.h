#ifndef BERT_QROCKSDB_H
#define BERT_QROCKSDB_H

#include "QDumpInterface.h"
#include "QStore.h"

namespace rocksdb
{
class DB;
}

namespace qedis
{

class UnboundedBuffer;

class QRocksdb : public QDumpInterface
{
public:
    QRocksdb();
    ~QRocksdb();

    bool Open(const char* path);
    bool IsOpen() const ;

    QObject Get(const QString& key) override;
    bool Put(const QString& key) override;

    bool Put(const QString& key, const QObject& obj, int64_t ttl = 0) override;
    bool Delete(const QString& key) override;

private:
    rocksdb::DB* db_;

    // encoding stuff same as leveldb
    void _EncodeObject(const QObject& obj, int64_t absttl, UnboundedBuffer& v);

    void _EncodeString(const QString& str, UnboundedBuffer& v);
    void _EncodeHash(const PHASH&, UnboundedBuffer& v);
    void _EncodeList(const PLIST&, UnboundedBuffer& v);
    void _EncodeSet(const PSET&, UnboundedBuffer& v);
    void _EncodeSSet(const PSSET&, UnboundedBuffer& v);

    // decoding stuff same as leveldb
    QObject _DecodeObject(const char* data, size_t len, int64_t& remainTtlSeconds);

    QString _DecodeString(const char* data, size_t len);
    QObject _DecodeHash(const char* data, size_t len);
    QObject _DecodeList(const char* data, size_t len);
    QObject _DecodeSet(const char* data, size_t len);
    QObject _DecodeSSet(const char* data, size_t len);
};

}

#endif