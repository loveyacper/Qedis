
#include "QDB.h"
#include <assert.h>
#include <iostream>

extern "C"
{
#include "lzf.h"
}

extern "C"
uint64_t crc64(uint64_t crc, const unsigned char *s, uint64_t l);


// encoding
static const int8_t kTypeString = 0;
static const int8_t kTypeList   = 1;
static const int8_t kTypeSet    = 2;
static const int8_t kTypeZSet   = 3;
static const int8_t kTypeHash   = 4;

static const int8_t kTypeZipMap = 9;
static const int8_t kTYpeZipList=10;
static const int8_t kTypeIntSet =11;
static const int8_t kTypeZSetZipList = 12;
static const int8_t kTypeHashZipList = 13;

static const int8_t kQDBVersion = 6;
static const int8_t kExipreMs   = 0xFC;
static const int8_t kExipre     = 0xFD;
static const int8_t kSelectDB   = 0xFE;
static const int8_t kEOF        = 0xFF;

static const int8_t  k6Bits    = 0;
static const int8_t  k14bits   = 1;
static const int8_t  k32bits   = 2;
static const int8_t  kSpecial  = 3;  //  the string may be interger, or lzf
static const int8_t  kLow6Bits = 0x3F;

static const int8_t  kEnc8Bits  = 0;
static const int8_t  kEnc16Bits = 1;
static const int8_t  kEnc32Bits = 2;
static const int8_t  kEncLZF    = 3;

// not support expire, TODOx
void  QDBSaver::Save()
{
    const char* const fileName = "dump.qdb.rdb";
    if (!m_qdb.Open(fileName, false))
        assert (false);
    
    char buf[16];
    snprintf(buf, sizeof buf, "REDIS%04d", kQDBVersion);
    m_qdb.Write(buf, 9);

    for (int dbno = 0; dbno < 16; ++ dbno)
    {
        QSTORE.SelectDB(dbno);
        m_qdb.Write(&kSelectDB, 1);
        SaveLength(dbno);
            
        for (auto kv(QSTORE.begin()); kv != QSTORE.end(); ++ kv)
        {
            SaveType(kv->second);
            SaveKey(kv->first);
            SaveObject(kv->second);
        }
    }

    m_qdb.Write(&kEOF, 1);
    
    // crc 8 bytes
    MemoryFile  file;
    file.OpenForRead(fileName);
    
    const void* data;
    size_t  len = m_qdb.Offset();
    file.Read(data, len);
    
    const uint64_t  crc = crc64(0, (const unsigned char* )data, len);
    m_qdb.Write(&crc, sizeof crc);
    
    std::cerr << "rdb saved\n";
}

void QDBSaver::SaveType(const QObject& obj)
{
    switch (obj.encoding)
    {
        case QEncode_raw:
        case QEncode_int:
            SaveLength(kTypeString);
            break;
                
        case QEncode_list:
            SaveLength(kTypeList);
            break;
                
        case QEncode_hash:
            SaveLength(kTypeHash);
            break;
            
        case QEncode_set:
            SaveLength(kTypeSet);
            break;
            
        case QEncode_sset:
            SaveLength(kTypeZSet);
            break;
            
        default:
            assert (false);
            break;
    }
}
    
void QDBSaver::SaveKey(const QString& key)
{
    SaveString(key);
}

void QDBSaver::SaveObject(const QObject& obj)
{
    switch (obj.encoding)
    {
        case QEncode_raw:
            SaveString(*obj.CastString());
            break;
    
        case QEncode_int:
            SaveString((int64_t)obj.value.get());
            break;
            
        case QEncode_list:
            _SaveList(obj.CastList());
            break;
            
        case QEncode_set:
            _SaveSet(obj.CastSet());
            break;
            
        case QEncode_hash:
            _SaveHash(obj.CastHash());
            break;
            
        case QEncode_sset:
            _SaveSSet(obj.CastSortedSet());
            break;
            
        default:
            break;
    }
}

/* Copy from redis~
 * Save a double value. Doubles are saved as strings prefixed by an unsigned
 * 8 bit integer specifying the length of the representation.
 * This 8 bit integer has special values in order to specify the following
 * conditions:
 * 253: not a number
 * 254: + inf
 * 255: - inf
 */
void  QDBSaver::_SaveDoubleValue(double val)
{
    unsigned char buf[128];
    int len;
    
    if (isnan(val)) {
        buf[0] = 253;
        len = 1;
    } else if (!isfinite(val)) {
        len = 1;
        buf[0] = (val < 0) ? 255 : 254;
    } else {
        snprintf((char*)buf+1,sizeof(buf)-1,"%.17g",val);
        buf[0] = strlen((char*)buf+1);
        len = buf[0]+1;
    }
    return m_qdb.Write(buf,len);
}


void QDBSaver::_SaveList(const PLIST& l)
{
    SaveLength(l->size());
    
    for (auto elem(l->begin()); elem != l->end(); ++ elem)
    {
        SaveString(*elem);
    }
}


void  QDBSaver::_SaveSet(const PSET& s)
{
    SaveLength(s->size());
    
    for (auto elem(s->begin()); elem != s->end(); ++ elem)
    {
        SaveString(*elem);
    }
}

void  QDBSaver::_SaveHash(const PHASH& h)
{
    SaveLength(h->size());
    
    for (auto elem(h->begin()); elem != h->end(); ++ elem)
    {
        SaveString(elem->first);
        SaveString(elem->second);
    }
}


void    QDBSaver::_SaveSSet(const PSSET& ss)
{
    SaveLength(ss->Size());
    
    for (auto elem(ss->begin()); elem != ss->end(); ++ elem)
    {
        SaveString(elem->first);
        _SaveDoubleValue(elem->second);
    }
}

void QDBSaver::SaveString(const QString& str)
{
    if (str.size() < 10)
    {
        long lVal;
        if (Strtol(str.data(), str.size(), &lVal))
        {
            SaveString(lVal);
            return;
        }
    }
    
    if (!SaveLZFString(str))
    {
        SaveLength(str.size());
        m_qdb.Write(str.data(), str.size());
    }
}
    

void  QDBSaver::SaveLength(uint64_t  len)
{
    assert ((len & ~0xFFFFFFFF) == 0);
  
    if (len < (1 << 6))
    {
        len &= kLow6Bits;
        len |= k6Bits << 6;
        m_qdb.Write(&len, 1);
    }
    else if (len < (1 << 14))
    {
        uint16_t encodeLen = (len >> 8) & kLow6Bits;
        encodeLen |= k14bits << 6;
        encodeLen |= (len & 0xFF) << 8;
        m_qdb.Write(&encodeLen, 2);
    }
    else
    {
        int8_t  encFlag = k32bits << 6;
        m_qdb.Write(&encFlag, 1);
        len = htonl(len);
        m_qdb.Write(&len, 4);
    }
}
    
void QDBSaver::SaveString(int64_t intVal)
{
    uint8_t specialByte = kSpecial << 6;
    
    if ((intVal & ~0x7F) == 0)
    {
        specialByte |= kEnc8Bits;
        m_qdb.Write(&specialByte, 1);
        m_qdb.Write(&intVal, 1);
    }
    else if ((intVal & ~0x7FFF) == 0)
    {
        specialByte |= kEnc16Bits;
        m_qdb.Write(&specialByte, 1);
        m_qdb.Write(&intVal, 2);
    }
    else if ((intVal & ~0x7FFFFFFF) == 0)
    {
        specialByte |= kEnc32Bits;
        m_qdb.Write(&specialByte, 1);
        m_qdb.Write(&intVal, 4);
    }
    else
    {
        assert (false);
    }
}
    

bool QDBSaver::SaveLZFString(const QString& str)
{
    if (str.size() < 5)
        return false;
        
    size_t outlen = str.size() - 4;
    std::unique_ptr<char []> outBuf(new char[outlen + 1]);
        
    uint32_t compressLen = lzf_compress((const void*)str.data(), str.size(),
                                            outBuf.get(), outlen);
    
    if (compressLen == 0)
        return false;
        
    uint8_t specialByte = kSpecial << 6 | kEncLZF;
    
    // compress len + raw len + str data;
    SaveLength(compressLen);
    SaveLength(str.size());
    m_qdb.Write(outBuf.get(), compressLen);
        
    return  true;
}


