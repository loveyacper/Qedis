#ifndef BERT_QZIPLIST_H
#define BERT_QZIPLIST_H

#include <string>

#include "QVarInt.h"
#include "QList.h"

// totalbyte(2) + tail_offset(2) + num(2) + entry1 + ... + special endbyte
// entry = prelen(2) + encoding(1) + len(var) + data(len)
// if list num > 512 || totalbyte > 1MB, turn to list

class QZipList
{
public:

#pragma pack(1)
    struct Entry
    {
        uint8_t   preLen;
        uint8_t   encoding; // 0 == raw, 1 == int

        union
        {
            int8_t  value[QVarInt::kMaxBytes];
            struct
            {
                uint8_t   len;
                int8_t    data[];
            } ;
        };
    };
#pragma pack()

    // may read from config
    static const uint16_t kMaxEntry = 512;
    static const uint32_t kMaxListBytes= 1 * 1024 * 1024;
    static const uint32_t kMaxEntryBytes = 256;
    
    QZipList();
    ~QZipList();
    
    size_t ByteSize() const { return m_list ? m_list->bytes : 0; }
    size_t Size() const;
    bool   Empty() const;
    
    bool   PushBack(const QString& val);
    bool   PushFront(const QString& val);
    Entry* InsertBefore(const QString* pos, const QString& val);
    Entry* InsertAfter(const QString* pos, const QString& val);

    Entry* Erase(size_t idx, size_t& num);
    Entry* Erase(Entry* e);

    bool   GetEntryValue(size_t  idx, const int8_t*& str, uint64_t& valOrLen) const;
    static void GetEntryValue(const Entry* e, const int8_t*& str, uint64_t& valOrLen);
    Entry* GetEntry(size_t  idx) const;
    Entry* FindEntry(const QString& val) const;

    Entry* Head() const;
    Entry* Tail() const;
    Entry* Prev(const Entry* entry) const;
    Entry* Next(const Entry* entry) const;
    size_t EntrySize(const Entry* entry) const;

    void   ConvertToList(QList& l) const;
    
    void   Print() const;
    static void   PrintEntry(const Entry* entry);
    static bool   CompareEntry(const Entry* , const Entry* );
    
private:
    bool   _CheckLimit();
    Entry* _Insert(const QString* pos, const QString& val, bool before);
    bool   _AssureSpace(int size);
    static size_t _MakeEntry(const QString& val, Entry* entry);

    Entry* _Erase(Entry* e, size_t& num);

    struct List
    {
        uint32_t  bytes;
        uint16_t  tail ;
        uint16_t  num  ;
        int8_t    entries[];
    };
    
    union
    {
        int8_t* m_ptr;
        List*   m_list;
    };
};

#endif

