#include <iostream>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "QCommon.h"
#include "QZipList.h"
    
QZipList::QZipList() : m_ptr(nullptr)
{
}


QZipList::~QZipList()
{
    delete [] m_ptr;
}

QZipList::Entry* QZipList::Head() const
{
    if (m_ptr == nullptr)
        return nullptr;

    return  (Entry* )m_list->entries;
}
    
QZipList::Entry* QZipList::Tail() const
{
    if (m_ptr == nullptr)
        return nullptr;
    
    return  (Entry* )(m_list->entries + m_list->tail);
}

QZipList::Entry* QZipList::Prev(const Entry* entry) const
{
    if (reinterpret_cast<const int8_t* >(entry) == m_list->entries)
        return nullptr;

    return  (Entry* )((const int8_t*)entry - entry->preLen);
}

QZipList::Entry* QZipList::Next(const Entry* entry) const
{
    if (reinterpret_cast<const int8_t* >(entry) == (m_list->entries + m_list->tail))
        return nullptr;

    return (Entry* )((const int8_t*)entry + EntrySize(entry));
}

size_t QZipList::Size() const
{
    if (m_ptr == nullptr)
        return 0;
        
    return m_list->num;
}

bool QZipList::Empty() const
{
    return  Size() == 0;
}
    
bool QZipList::GetEntryValue(size_t  idx, const int8_t*& str, uint64_t& valOrLen) const
{
    const Entry* entry = GetEntry(idx);
    if (entry)
    {
        GetEntryValue(entry, str, valOrLen);
        return true;
    }

    return  false;
}

void QZipList::GetEntryValue(const Entry* entry, const int8_t*& str, uint64_t& valOrLen)
{
    if (entry->encoding == QEncode_raw)
    {
        valOrLen = entry->len;
        str = entry->data;
    }
    else if (entry->encoding == QEncode_int)
    {
        valOrLen = QVarInt::Get(entry->value);
        str = 0;
    }
    else
    {
        assert(false);
    }
}

QZipList::Entry* QZipList::GetEntry(size_t  idx) const
{
    if (Empty() || idx >= m_list->num)    return nullptr;

    for (Entry* iter = (Entry* )m_list->entries; iter; iter = Next(iter))
    {
        if (idx -- == 0)
            return iter;
    }

    return nullptr;
}


QZipList::Entry* QZipList::FindEntry(const QString& val) const
{
    if (Empty())    return nullptr;

    long  longval;

    uint8_t encode;
    if (Strtol(val.c_str(), val.size(), &longval))
        encode = QEncode_int;
    else
        encode = QEncode_raw;
    
    for (Entry* entry = Head(); entry; entry = Next(entry))
    {
        if (entry->encoding != encode)
            continue;
            
        if (entry->encoding == QEncode_int)
        {
            if (longval == QVarInt::Get(entry->value))
                return entry;
        }
        else
        {
            if (val.size() == entry->len &&
                memcmp(val.c_str(), entry->data, entry->len) == 0)
                return entry;
        }
    }

    return nullptr;
}


void   QZipList::ConvertToList(QList& l) const
{
    for (const Entry* e = Head(); e; e = Next(e))
    {
        const int8_t*   str;
        uint64_t        valOrLen;
        
        QZipList::GetEntryValue(e, str, valOrLen);
        if (str != nullptr)
        {
            l.push_back(QString((const char* )str, valOrLen));
        }
        else
        {
            QString  val;
            val.resize(10);
            int len = snprintf(&val[0], val.size(), "%ld", valOrLen);
            l.push_back(val.substr(0, len));
        }
        
    }
    
}

QZipList::Entry* QZipList::InsertBefore(const QString* pos, const QString& val)
{
    return _Insert(pos, val, true);
}

QZipList::Entry* QZipList::InsertAfter(const QString* pos, const QString& val)
{
    return _Insert(pos, val, false);
}

QZipList::Entry* QZipList::Erase(size_t idx, size_t& num)
{
    if (num == 0) return nullptr;

    Entry* e = GetEntry(idx);
    if (!e)  return nullptr;

    return _Erase(e, num);
}

QZipList::Entry* QZipList::Erase(Entry* e)
{
    size_t n = 1;
    return _Erase(e, n);
}

QZipList::Entry* QZipList::_Erase(Entry* e, size_t& num)
{
    if (Empty() || !e || num == 0)
        return nullptr;

    const size_t eraseOffset = (int8_t* )e - m_list->entries;

    Entry* end = e;
    size_t eraseSize = 0;
    size_t eraseNum = 0;
    while (end != 0 && eraseNum != num)
    {
        ++ eraseNum;
        eraseSize += EntrySize(end);
        end = Next(end);
    }

    Entry* oldTail = Tail();
    size_t oldTailSize = EntrySize(oldTail);

    if (end == 0)
    {
        m_list->tail -= (eraseSize - oldTailSize);
        m_list->tail -= e->preLen;
    }
    else
    {
        m_list->tail -= eraseSize;
        end->preLen  = e->preLen;
        memmove(e, end, (int8_t*)oldTail - (int8_t*)end + oldTailSize);
    }

    num = eraseNum;
    m_list->num -= eraseNum;
    _AssureSpace(static_cast<int>(-eraseSize));

    if (end == 0)
        return 0;
    else
        return (Entry*)(m_list->entries + eraseOffset);
}

QZipList::Entry* QZipList::_Insert(const QString* pos, const QString& val, bool before)
{
    Entry* pivot = 0;

    if (pos)    pivot = FindEntry(*pos);

    char buf[kMaxEntryBytes];
    Entry* entry = (Entry*)buf;
    size_t entrySize = _MakeEntry(val, entry);
    if (entrySize == 0)
        return nullptr;

    assert (entrySize <= kMaxEntryBytes);

    if (!_AssureSpace(static_cast<int>(entrySize)))
        return nullptr;

    if (!pivot)
    {
        if (before)
            pivot = Head();
        else
            pivot = Tail();
    }

    size_t offset =  0;
    for (Entry* iter = pivot; iter != Head(); iter = Prev(iter))
    {
        offset += iter->preLen;
    }

    if (before)
    {
        entry->preLen = m_list->num ? pivot->preLen : 0;
        if (m_list->num)
            pivot->preLen = entrySize;
    }
    else
    {
        entry->preLen = m_list->num ? EntrySize(pivot) : 0;
        offset += entry->preLen;
    }

    size_t oldTailSize = (m_list->num ? EntrySize(Tail()) : 0);
    size_t movedSize = m_list->tail + oldTailSize - offset;

    memmove(m_list->entries + offset + entrySize, m_list->entries + offset, movedSize);
    memcpy(m_list->entries + offset, buf, entrySize);

    ++ m_list->num;

    if (offset == m_list->tail + oldTailSize)
        m_list->tail = offset;
    else
        m_list->tail += entrySize;

    return  (Entry*)(m_list->entries + offset);
}

size_t QZipList::EntrySize(const Entry* entry) const
{
    if (entry->encoding == QEncode_int)
    {
        size_t valSize = QVarInt::GetVarSize((int8_t*)entry->value);
        return sizeof(entry->preLen) + sizeof(entry->encoding) + valSize;
    }
    else if (entry->encoding == QEncode_raw)
    {
        return sizeof(entry->preLen) + sizeof(entry->encoding) + sizeof(entry->len) + entry->len;
    }
    
    assert (false);
    return 0;
}

size_t  QZipList::_MakeEntry(const QString& val, Entry* entry)
{
    if (val.size() + sizeof(Entry) > kMaxEntryBytes)
        return 0;

    size_t  size = sizeof(entry->preLen) + sizeof(entry->encoding); // skip prelen and encoding
    
    long  longval;
    if (Strtol(val.c_str(), val.size(), &longval))
    {
        // | encode1 | the var long
        std::cerr << "Make entry long value " << longval << std::endl;
        entry->encoding = QEncode_int;
        size += QVarInt::Set(longval, entry->value);
    }
    else
    {
        // | encode1 | uint16 len | len string
        entry->encoding = QEncode_raw;
        entry->len      = val.size();
        std::cerr << "Make entry raw " << val << ", len " << (int)entry->len<< std::endl;
        memcpy(entry->data, val.c_str(), val.size());
        
        size += sizeof(entry->len) + entry->len;
    }
    
    return  size;
}

bool  QZipList::_CheckLimit()
{
    if (m_list == 0)
        return true;

    return m_list->num  < kMaxEntry;
}

bool QZipList::_AssureSpace(int size)
{
    size_t newSize = size + (m_list ? m_list->bytes : sizeof(List));
    if (newSize > kMaxListBytes || newSize == 0)
        return false;

    List* tmp = m_list;
    m_ptr = new int8_t[newSize];
    if (tmp)
    {
        memcpy(m_ptr, tmp, tmp->bytes < newSize ? tmp->bytes : newSize);
        delete [] tmp;
    }
    else
    {
        m_list->tail  = 0;
        m_list->num   = 0;
    }
        
    m_list->bytes = static_cast<uint32_t>(newSize);
    
    return  true;
}

bool  QZipList::PushFront(const QString& val)
{
    // if false, should turn to double list
    if (!_CheckLimit())
        return false;

    return InsertBefore(0, val) != 0;
}

bool  QZipList::PushBack(const QString& val)
{
    // if false, should turn to double list
    if (!_CheckLimit())
        return false;

    return InsertAfter(0, val) != 0;
}

void    QZipList::Print() const
{
    if (Empty()) return;

    const QZipList::Entry* it = Head();
    while (it)
    {
        PrintEntry(it);
        it = Next(it);
    }
}

void   QZipList::PrintEntry(const Entry* entry)
{
    std::cerr << "pre len = " << (int)entry->preLen;
    std::cerr << " encoding = " << (entry->encoding ? "raw" : "int");
    if (entry->encoding == QEncode_int) // int
    {
        std::cerr << " long = " << QVarInt::Get(entry->value) << std::endl
        << std::endl;
    }
    else if (entry->encoding == QEncode_raw)// string
    {
        printf(" string = %.*s\n\n", entry->len, entry->data);
    }
    else
    {
        std::cerr << "!!! WRONG encoding = " << (int)entry->encoding << std::endl;
        assert (0);
    }
}

bool  QZipList::CompareEntry(const Entry* e1, const Entry* e2)
{
    if (e1->encoding != e2->encoding)
        return false;

    if (e1->encoding == QEncode_raw)
        return e1->len == e2->len && memcmp(e1->data, e2->data, e1->len) == 0;
    else if (e1->encoding == QEncode_int)
        return QVarInt::Get(e1->value) == QVarInt::Get(e2->value);

    assert (false);
    return false;
}

#if 0
int main()
{
    QZipList  mems;

    mems.PushFront("123");
    mems.PushBack("456");
    mems.PushFront("afuckme");
    mems.PushBack("b");
    mems.Print();

    QZipList::Entry* entry = mems.FindEntry("123");

    const int8_t* str;
    uint64_t  val;

    entry = mems.Head();
    while (entry)
    {
        QZipList::GetEntryValue(entry, str, val);

        if (str)
        {
            printf("%.*s\n", val, str);
            entry = mems.Erase(entry);
        }
        else
        {
            printf("%d\n", val);
            entry = mems.Next(entry);
        }
    }
    std::cerr << mems.ByteSize() << std::endl;
    mems.Print();
}
#endif


