
#include "QDB.h"
#include "Log/Logger.h"
#include <sstream>
#include <unistd.h>
#include <math.h>
#include <arpa/inet.h>

extern "C"
{
#include "lzf/lzf.h"
#include "redisZipList.h"
#include "redisIntset.h"
}

extern "C"
uint64_t crc64(uint64_t crc, const unsigned char *s, uint64_t l);

namespace qedis
{

time_t  g_lastQDBSave = 0;
pid_t   g_qdbPid = -1;


// encoding
static const int8_t kTypeString = 0;
static const int8_t kTypeList   = 1;
static const int8_t kTypeSet    = 2;
static const int8_t kTypeZSet   = 3;
static const int8_t kTypeHash   = 4;

static const int8_t kTypeZipMap = 9;
static const int8_t kTypeZipList=10;
static const int8_t kTypeIntSet =11;
static const int8_t kTypeZSetZipList = 12;
static const int8_t kTypeHashZipList = 13;
static const int8_t kTypeQuickList = 14;

static const int8_t kQDBVersion = 6;

static const int8_t kAux        = 0xFA;
static const int8_t kResizeDb   = 0xFB;
static const int8_t kExpireMs   = 0xFC;
static const int8_t kExpire     = 0xFD;
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

QDBSaver::QDBSaver(const char* qdbFile)
{
    if (qdbFile)
    {
        if (!qdb_.Open(qdbFile, false))
            ERR << "QDBSaver can not open file " << qdbFile;
    }
}

void  QDBSaver::Save(const char* qdbFile)
{
    char     tmpFile[64] = "";
    snprintf(tmpFile, sizeof tmpFile, "tmp_qdb_file_%d", getpid());
    
    if (!qdb_.Open(tmpFile, false))
        assert (false);
    
    char buf[16];
    snprintf(buf, sizeof buf, "REDIS%04d", kQDBVersion);
    qdb_.Write(buf, 9);

    for (int dbno = 0; true; ++ dbno)
    {
        if (QSTORE.SelectDB(dbno) == -1)
            break;

        if (QSTORE.DBSize() == 0)
            continue;  // But redis will save empty db
        
        qdb_.Write(&kSelectDB, 1);
        SaveLength(dbno);
        
        uint64_t now = ::Now();
        for (const auto& kv : QSTORE)
        {
            int64_t ttl = QSTORE.TTL(kv.first, now);
            if (ttl > 0)
            {
                ttl += now;

                qdb_.Write(&kExpireMs, 1);
                qdb_.Write(&ttl, sizeof ttl);
            }
            else if (ttl == QStore::ExpireResult::expired)
            {
                continue;
            }

            SaveType(kv.second);
            SaveKey(kv.first);
            SaveObject(kv.second);
        }
    }

    qdb_.Write(&kEOF, 1);
    
    // crc 8 bytes
    InputMemoryFile  file;
    file.Open(tmpFile);
    
    auto len  = qdb_.Offset();
    auto data = file.Read(len);
    
    const uint64_t crc = crc64(0, (const unsigned char* )data, len);
    qdb_.Write(&crc, sizeof crc);
    
    if (::rename(tmpFile, qdbFile) != 0)
    {
        perror("rename error");
        assert (false);
    }
}

void QDBSaver::SaveType(const QObject& obj)
{
    switch (obj.encoding)
    {
        case QEncode_raw:
        case QEncode_int:
            qdb_.Write(&kTypeString, 1);
            break;
                
        case QEncode_list:
            qdb_.Write(&kTypeList, 1);
            break;
                
        case QEncode_hash:
            qdb_.Write(&kTypeHash, 1);
            break;
            
        case QEncode_set:
            qdb_.Write(&kTypeSet, 1);
            break;
            
        case QEncode_sset:
            qdb_.Write(&kTypeZSet, 1);
            break;
            
        default:
            assert(!!!"Wrong encoding");
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
        case QEncode_int:
            SaveString(*GetDecodedString(&obj));
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
    } else if (!std::isfinite(val)) {
        len = 1;
        buf[0] = (val < 0) ? 255 : 254;
    } else {
        snprintf((char*)buf+1,sizeof(buf)-1,"%.6g",val);
        buf[0] = strlen((char*)buf+1);
        len = buf[0]+1;
    }
    
    qdb_.Write(buf,len);
}


void QDBSaver::_SaveList(const PLIST& l)
{
    SaveLength(l->size());
    
    for (const auto& e : *l)
    {
        SaveString(e);
    }
}


void  QDBSaver::_SaveSet(const PSET& s)
{
    SaveLength(s->size());
    
    for (const auto& e : *s)
    {
        SaveString(e);
    }
}

void  QDBSaver::_SaveHash(const PHASH& h)
{
    SaveLength(h->size());
    
    for (const auto& e : *h)
    {
        SaveString(e.first);
        SaveString(e.second);
    }
}


void    QDBSaver::_SaveSSet(const PSSET& ss)
{
    SaveLength(ss->Size());
    
    for (const auto& e : *ss)
    {
        SaveString(e.first);
        _SaveDoubleValue(e.second);
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
        qdb_.Write(str.data(), str.size());
    }
}
    

void QDBSaver::SaveLength(uint64_t len)
{
    assert ((len & ~0xFFFFFFFF) == 0);
  
    if (len < (1 << 6))
    {
        len &= kLow6Bits;
        len |= k6Bits << 6;
        qdb_.Write(&len, 1);
    }
    else if (len < (1 << 14))
    {
        uint16_t encodeLen = (len >> 8) & kLow6Bits;
        encodeLen |= k14bits << 6;
        encodeLen |= (len & 0xFF) << 8;
        qdb_.Write(&encodeLen, 2);
    }
    else
    {
        int8_t  encFlag = static_cast<int8_t>(k32bits << 6);
        qdb_.Write(&encFlag, 1);
        len = htonl(len);
        qdb_.Write(&len, 4);
    }
}
    
void QDBSaver::SaveString(int64_t intVal)
{
    uint8_t specialByte = kSpecial << 6;
    
    if ((intVal & ~0x7F) == 0)
    {
        specialByte |= kEnc8Bits;
        qdb_.Write(&specialByte, 1);
        qdb_.Write(&intVal, 1);
    }
    else if ((intVal & ~0x7FFF) == 0)
    {
        specialByte |= kEnc16Bits;
        qdb_.Write(&specialByte, 1);
        qdb_.Write(&intVal, 2);
    }
    else if ((intVal & ~0x7FFFFFFF) == 0)
    {
        specialByte |= kEnc32Bits;
        qdb_.Write(&specialByte, 1);
        qdb_.Write(&intVal, 4);
    }
    else
    {
        char buf[64];
        auto len = Number2Str(buf, sizeof buf, intVal);
        SaveLength(static_cast<uint64_t>(len));
        qdb_.Write(buf, len);
    }
}
    

bool QDBSaver::SaveLZFString(const QString& str)
{
    if (str.size() < 20)
        return false;
        
    unsigned outlen = static_cast<unsigned>(str.size() - 4);
    std::unique_ptr<char []> outBuf(new char[outlen + 1]);
        
    auto compressLen = lzf_compress((const void*)str.data(), static_cast<unsigned>(str.size()),
                                        outBuf.get(), outlen);
    
    if (compressLen == 0)
    {
        ERR << "compress len = 0";
        return false;
    }
    
    int8_t specialByte = static_cast<int8_t>(kSpecial << 6) | kEncLZF;
    qdb_.Write(&specialByte, 1);
    
    // compress len + raw len + str data;
    SaveLength(compressLen);
    SaveLength(str.size());
    qdb_.Write(outBuf.get(), compressLen);
    
    DBG << "compress len " << compressLen << ", raw len " << str.size();
    
    return  true;
}


void QDBSaver::SaveDoneHandler(int exitRet, int whatSignal)
{
    if (exitRet == 0 && whatSignal == 0)
    {
        INF << "save rdb success";
        g_lastQDBSave = time(NULL);
        QStore::dirty_ = 0;
    }
    else
    {
        ERR << "save rdb failed with exit result " << exitRet << ", signal " << whatSignal;
    }
    
    g_qdbPid = -1;
}


QDBLoader::QDBLoader(const char *data, size_t len)
{
    if (data && len)
        qdb_.Attach(data, len);
}

int  QDBLoader::Load(const char *filename)
{
    if (!qdb_.Open(filename))
    {
        return - __LINE__;
    }
    
    // check the magic string "REDIS" and version number
    size_t len = 9;
    const char* data = qdb_.Read(len);
    
    if (len != 9)
    {
        return - __LINE__;
    }
    
    long qdbversion;
    if (!Strtol(data + 5, 4, &qdbversion) ||
        qdbversion < 6)
    {
        return - __LINE__;
    }
    qdb_.Skip(9);
    
    //  SELECTDB + dbno
    //  type1 + key + obj
    //  EOF + crc

    int64_t absTimeout = 0;
    int8_t indicator = 0;
    bool eof = false;
    while (!eof)
    {
        try {
            indicator = LoadByte();
        }
        catch (const std::runtime_error& e) {
            ERR << "LoadByte with exception: " << e.what();
            return - __LINE__;
        }
        
        switch (indicator)
        {
            case kEOF:
                DBG << "encounter EOF";
                eof = true;
                break;

            case kAux:
                DBG << "encounter AUX";
                try {
                    _LoadAux();
                }
                catch (const std::runtime_error& e) {
                    ERR << "_LoadAux with exception: " << e.what();
                    return - __LINE__;
                }
                break;

            case kResizeDb:
                DBG << "encounter kResizeDb";
                try {
                    _LoadResizeDB();
                }
                catch (const std::runtime_error& e) {
                    ERR << "_LoadResizeDB with exception: " << e.what();
                    return - __LINE__;
                }
                break;
                
            case kSelectDB:
            {
                bool special;
                size_t dbno = 0;
                try {
                    dbno = LoadLength(special);
                }
                catch (const std::runtime_error& e) {
                    ERR << "LoadLength with exception: " << e.what();
                    return - __LINE__;
                }

                if (special)
                {
                    ERR << "LoadLength should not be special";
                    return - __LINE__;
                }

                // check db no
                if (dbno > kMaxDbNum)
                {
                    ERR << "Abnormal db number " << dbno;
                    return __LINE__;
                }

                if (QSTORE.SelectDB(static_cast<int>(dbno)) == -1)
                {
                    ERR << "DB NUMBER is differ from RDB file";
                    return __LINE__;
                }
                DBG << "encounter Select DB " << dbno;
                break;
            }
                
            case kExpireMs:
                try {
                    absTimeout = qdb_.Read<int64_t>();
                }
                catch (const std::runtime_error& e) {
                    ERR << "Read kExpireMs with exception: " << e.what();
                    return - __LINE__;
                }

                break;
                
            case kExpire:
                try {
                    absTimeout = qdb_.Read<int64_t>();
                    absTimeout *= 1000;
                }
                catch (const std::runtime_error& e) {
                    ERR << "Read kExpire with exception: " << e.what();
                    return - __LINE__;
                }

                break;
                
            case kTypeString:
            case kTypeList:
            case kTypeZipList:
            case kTypeSet:
            case kTypeIntSet:
            case kTypeHash:
            case kTypeHashZipList:
            case kTypeZipMap:
            case kTypeZSet:
            case kTypeZSetZipList:
            case kTypeQuickList:
            {
                QString key;
                QObject obj;
                try {
                    key = LoadKey();
                    obj = LoadObject(indicator);
                    assert (obj.type != QType_invalid);
                    DBG << "encounter key = " << key << ", obj.encoding = " << obj.encoding;
                }
                catch (const std::runtime_error& e) {
                    ERR << "Read object with exception: " << e.what();
                    return - __LINE__;
                }

                if (absTimeout < 0)
                {
                    ERR << "Wrong absTimeout " << absTimeout;
                    return - __LINE__;
                }
                
                if (absTimeout == 0)
                {
                    QSTORE.SetValue(key, std::move(obj));
                }
                else if (absTimeout > 0)
                {
                    if (absTimeout > static_cast<int64_t>(::Now()))
                    {
                        DBG << key << " load timeout " << absTimeout;
                        QSTORE.SetValue(key, std::move(obj));
                        QSTORE.SetExpire(key, absTimeout);
                    }
                    else
                    {
                        INF << key << " is already time out";
                    }
                    
                    absTimeout = 0;
                }
                break;
            }
                
            default:
                ERR << indicator << " is unknown type";
                return - __LINE__;
        }
    }
    
    return 0;
}
    
int8_t QDBLoader::LoadByte()
{
    return qdb_.Read<int8_t>();
}

size_t  QDBLoader::LoadLength(bool& special)
{
    const int8_t byte = qdb_.Read<int8_t>();
    
    special = false;
    size_t  lenResult = 0;
    
    switch ((byte & 0xC0) >> 6)
    {
        case k6Bits:
        {
            lenResult = byte & kLow6Bits;
            break;
        }
            
        case k14bits:
        {
            lenResult = byte & kLow6Bits; // high 6 bits;
            lenResult <<= 8;
            
            const uint8_t bytelow = qdb_.Read<uint8_t>();
            
            lenResult |= bytelow;
            break;
        }
            
        case k32bits:
        {
            const int32_t fourbytes = qdb_.Read<int32_t>();
            
            lenResult = ntohl(fourbytes);
            break;
        }
            
        case kSpecial:
        {
            special = true;
            lenResult = byte & kLow6Bits;
            break;
        }
            
        default:
        {
            std::string s("Wrong length type:" + std::to_string(byte));
            throw std::runtime_error(std::move(s));
        }
    }

    return lenResult;
}


QObject QDBLoader::LoadSpecialStringObject(size_t  specialVal)
{
    bool isInt = true;
    long val;
    
    switch (specialVal)
    {
        case kEnc8Bits:
        {
            val = qdb_.Read<uint8_t>();
            break;
        }
            
        case kEnc16Bits:
        {
            val = qdb_.Read<uint16_t>();
            break;
        }
            
        case kEnc32Bits:
        {
            val = qdb_.Read<uint32_t>();
            break;
        }
            
        case kEncLZF:
        {
            isInt = false;
            break;
        }
            
        default:
            throw std::runtime_error("Wrong specialVal");
    }
    
    if (isInt)
        return QObject::CreateString(val);
    else
        return QObject::CreateString(LoadLZFString());
}

QString QDBLoader::LoadString(size_t strLen)
{
    const char* str = qdb_.Read(strLen);
    qdb_.Skip(strLen);
    
    return QString(str, strLen);
}


QString QDBLoader::LoadLZFString()
{
    bool special;
    size_t compressLen = LoadLength(special);
    if (special)
    {
        ERR << "Should not be special for compressLen";
        return QString();
    }
    
    unsigned rawLen = static_cast<unsigned>(LoadLength(special));
    if (special)
    {
        ERR << "Should not be special for rawLen";
        return QString();
    }
    
    const char* compressStr = qdb_.Read(compressLen);
    
    QString val;
    val.resize(rawLen);
    if (lzf_decompress(compressStr, static_cast<unsigned>(compressLen),
                       &val[0], rawLen) == 0)
    {
        ERR << "decompress error";
        return QString();
    }

    qdb_.Skip(compressLen);
    return val;
}

QString QDBLoader::LoadKey()
{
    return _LoadGenericString();
}


QObject QDBLoader::LoadObject(int8_t type)
{
    switch (type)
    {
        case kTypeString:
        {
            bool special;
            size_t len = LoadLength(special);

            if (special)
            {
                return LoadSpecialStringObject(len);
            }
            else
            {
                return QObject::CreateString(LoadString(len));
            }
        }
        case kTypeList:
        {
            return _LoadList();
        }
        case kTypeZipList:
        {
            return _LoadZipList(kTypeZipList);
        }
        case kTypeSet:
        {
            return _LoadSet();
        }
        case kTypeIntSet:
        {
            return _LoadIntset();
        }
        case kTypeHash:
        {
            return _LoadHash();
        }
        case kTypeHashZipList:
        {
            return _LoadZipList(kTypeHashZipList);
        }
        case kTypeZipMap:
        {
            assert(!!!"zipmap should be replaced with ziplist");
            break;
        }
        case kTypeZSet:
        {
            return _LoadSSet();
        }
        case kTypeZSetZipList:
        {
            return _LoadZipList(kTypeZSetZipList);
        }
        case kTypeQuickList:
        {
            return _LoadQuickList();
        }
            
        default:
            break;
    }
    
    return QObject(QType_invalid);
}


QString QDBLoader::_LoadGenericString()
{
    bool special;
    size_t len = LoadLength(special);
    
    if (special)
    {
        QObject obj = LoadSpecialStringObject(len);
        return *GetDecodedString(&obj);
    }
    else
    {
        return LoadString(len);
    }
}

QObject QDBLoader::_LoadList()
{
    bool special;
    const auto len = LoadLength(special);
    if (special)
        throw std::runtime_error("LoadList length should not be special");

    DBG << "list length = " << len;
    
    QObject obj(QObject::CreateList());
    PLIST list(obj.CastList());
    for (size_t i = 0; i < len; ++ i)
    {
        const auto elemLen = LoadLength(special);
        QString  elem;
        if (special)
        {
            QObject str = LoadSpecialStringObject(elemLen);
            elem = *GetDecodedString(&str);
        }
        else
        {
            elem = LoadString(elemLen);
        }
        
        list->push_back(elem);
        DBG << "list elem : " << elem.c_str();
    }
    
    return obj;
}

QObject QDBLoader::_LoadSet()
{
    bool special;
    const auto len = LoadLength(special);
    if (special)
        throw std::runtime_error("LoadSet length should not be special");

    DBG << "set length = " << len;
    
    QObject obj(QObject::CreateSet());
    PSET  set(obj.CastSet());
    for (size_t i = 0; i < len; ++ i)
    {
        const auto elemLen = LoadLength(special);
        QString  elem;
        if (special)
        {
            QObject str = LoadSpecialStringObject(elemLen);
            elem = *GetDecodedString(&str);
        }
        else
        {
            elem = LoadString(elemLen);
        }
        
        set->insert(elem);
        DBG << "set elem : " << elem.c_str();
    }
    
    return obj;
}

QObject QDBLoader::_LoadHash()
{
    bool special;
    const auto len = LoadLength(special);
    if (special)
        throw std::runtime_error("LoadHash length should not be special");

    DBG << "hash length = " << len;
    
    QObject obj(QObject::CreateHash());
    PHASH  hash(obj.CastHash());
    for (size_t i = 0; i < len; ++ i)
    {
        const auto keyLen = LoadLength(special);
        QString  key;
        if (special)
        {
            QObject str = LoadSpecialStringObject(keyLen);
            key = *GetDecodedString(&str);
        }
        else
        {
            key = LoadString(keyLen);
        }
        
        const auto valLen = LoadLength(special);
        QString  val;
        if (special)
        {
            QObject str = LoadSpecialStringObject(valLen);
            val = *GetDecodedString(&str);
        }
        else
        {
            val = LoadString(valLen);
        }
        
        hash->insert(QHash::value_type(key, val));
        DBG << "hash key : " << key.c_str() << " val : " << val.c_str();
    }
    
    return obj;
}


QObject QDBLoader::_LoadSSet()
{
    bool special;
    const auto len = LoadLength(special);
    if (special)
        throw std::runtime_error("LoadSortedSet length should not be special");

    DBG << "sset length = " << len;
    
    QObject obj(QObject::CreateSSet());
    PSSET  sset(obj.CastSortedSet());
    for (size_t i = 0; i < len; ++ i)
    {
        const auto memberLen = LoadLength(special);
        QString  member;
        if (special)
        {
            QObject str = LoadSpecialStringObject(memberLen);
            member = *GetDecodedString(&str);
        }
        else
        {
            member = LoadString(memberLen);
        }
        
        const auto score = _LoadDoubleValue();
        sset->AddMember(member, static_cast<long>(score));
        DBG << "sset member : " << member.c_str() << " score : " << score;
    }
    
    return obj;
}

double  QDBLoader::_LoadDoubleValue()
{
    const uint8_t byte1st = qdb_.Read<uint8_t>();

    double  dvalue;
    switch (byte1st)
    {
        case 253:
        {
            dvalue = NAN;
            break;
        }
            
        case 254:
        {
            dvalue = INFINITY;
            break;
        }
            
        case 255:
        {
            dvalue = INFINITY;
            break;
        }
            
        default:
        {
            size_t len = byte1st;
            const char* val = qdb_.Read(len);
            assert(len == byte1st);
            qdb_.Skip(len);
            
            std::istringstream is(std::string(val, len));
            is >> dvalue;
            
            break;
        }
    }
    
    DBG << "load double value " << dvalue;
    
    return  dvalue;
}

struct ZipListElement
{
    unsigned char* sval;
    unsigned int   slen;
    
    long long      lval;
    
    QString ToString() const
    {
        if (sval)
        {
            const QString str((const char* )sval, QString::size_type(slen));
            DBG << "string zip list element " << str;
            return str;
        }
        else
            return  LongToString();
    }
    
    QString LongToString() const
    {
        assert(!sval);

        QString str(16, 0);
        auto len = Number2Str(&str[0], 16, lval);
        str.resize(len);
        
        DBG << "long zip list element " << str;
        return str;
    }
};

QObject QDBLoader::_LoadZipList(int8_t type)
{
    QString zl = _LoadGenericString();
    return _LoadZipList(zl, type);
}

QObject QDBLoader::_LoadZipList(const QString& zl, int8_t type)
{
    unsigned char* zlist = (unsigned char* )&zl[0];
    unsigned       nElem = ziplistLen(zlist);
    
    std::vector<ZipListElement>  elements;
    elements.resize(nElem);
    
    for (unsigned i = 0; i < nElem; ++ i)
    {
        unsigned char* elem = ziplistIndex(zlist, (int)i);
        
        int succ = ziplistGet(elem, &elements[i].sval, &elements[i].slen,
                                    &elements[i].lval);
        assert (succ);
    }
    
    switch (type)
    {
        case kTypeZipList:
        {
            QObject  obj(QObject::CreateList());
            PLIST    list(obj.CastList());
            
            for (const auto& elem : elements)
            {
                list->push_back(elem.ToString());
            }
            
            return obj;
        }
            
        case kTypeHashZipList:
        {
            QObject  obj(QObject::CreateHash());
            PHASH    hash(obj.CastHash());
            
            assert(elements.size() % 2 == 0);
            
            for (auto it(elements.begin()); it != elements.end(); ++ it)
            {
                auto key = it;
                auto value = ++ it;

                hash->insert(QHash::value_type(key->ToString(),
                                               value->ToString()));
            }
            
            return obj;
        }
            
        case kTypeZSetZipList:
        {
            QObject  obj(QObject::CreateSSet());
            PSSET    sset(obj.CastSortedSet());

            assert(elements.size() % 2 == 0);
            
            for (auto it(elements.begin()); it != elements.end(); ++ it)
            {
                const QString& member = it->ToString();
                ++ it;
                
                double  score;
                if (it->sval)
                {
                    Strtod((const char* )it->sval, it->slen, &score);
                }
                else
                {
                    score = it->lval;
                }

                DBG << "sset member " << member << ", score " << score;
                sset->AddMember(member, score);
            }
            
            return obj;
        }
            
        default:
            assert(!!!"illegal data type");
            break;
    }
    
    return QObject(QType_invalid);
}


QObject QDBLoader::_LoadIntset()
{
    QString str = _LoadGenericString();
    
    intset* iset = (intset* )&str[0];
    unsigned nElem = intsetLen(iset);
    
    std::vector<int64_t>  elements;
    elements.resize(nElem);
    
    for (unsigned i = 0; i < nElem; ++ i)
    {
        intsetGet(iset, i, &elements[i]);
    }
    
    QObject  obj(QObject::CreateSet());
    PSET     set(obj.CastSet());
    
    for (auto v : elements)
    {
        char buf[64];
        auto bytes = Number2Str<int64_t>(buf, sizeof buf, v);
        set->insert(QString(buf, bytes));
    }

    return obj;
}

QObject QDBLoader::_LoadQuickList()
{
    bool special = true;
    auto nElem = LoadLength(special);

    QObject obj(QObject::CreateList());
    PLIST list(obj.CastList());
    while (nElem -- > 0)
    {
        QString zl = _LoadGenericString();
        if (zl.empty())
            continue;

        QObject l = _LoadZipList(zl, kTypeZipList);
        PLIST tmplist(l.CastList());
        if (!tmplist->empty())
            list->splice(list->end(), *tmplist);
    }

    return obj;
}

void QDBLoader::_LoadAux()
{
    /* AUX: generic string-string fields. Use to add state to RDB
     * which is backward compatible. Implementations of RDB loading
     * are required to skip AUX fields they don't understand.
     * 
     * An AUX field is composed of two strings: key and value. */
    QString auxkey = _LoadGenericString();
    QString auxvalue = _LoadGenericString();

    if (!auxkey.empty() && auxkey[0] == '%')
    {
        /* All the fields with a name staring with '%' are considered
        * information fields and are logged at startup with a log
        * level of NOTICE. */
        USR << "RDB '" << auxkey << "': " << auxvalue;
    }
    else
    {
        /* We ignore fields we don't understand, as by AUX field 
         * contract. */
        DBG << "Unrecognized RDB AUX field: '" << auxkey << "': " << auxvalue;
    }
}

void QDBLoader::_LoadResizeDB()
{
    bool special = true;
    auto dbsize = LoadLength(special);
    if (special)
        throw std::runtime_error("Should not be special when LoadLength");

    auto expiresize = LoadLength(special);
    if (special)
        throw std::runtime_error("Should not be special when LoadLength");

    // Qedis just ignore this
    (void)special;
    (void)dbsize;
    (void)expiresize;
}


std::string DumpObject(const QObject& val)
{
    const char* file = "qedisdump";
    {
        QDBSaver saver(file);
        saver.SaveType(val);
        saver.SaveObject(val);
    }

    // 2 bytes version
    char v[2];
    v[0] = kQDBVersion & 0xFF;
    v[1] = (kQDBVersion >> 8) & 0xFF;

    InputMemoryFile ifile;
    ifile.Open(file);

    std::size_t size = std::numeric_limits<std::size_t>::max();
    const char* data = ifile.Read(size);

    std::string result(data, size);
    result.append(v, 2);
    // 8 bytes crc
    const uint64_t crc = crc64(0, (const unsigned char* )result.data(), result.size());
    result.append((const char*)&crc, 8);

    unlink(file);
    return result;
}

QObject RestoreObject(const char* data, size_t len)
{
    try {
        QDBLoader loader(data, len);
        int8_t type = loader.LoadByte();
        QObject obj = loader.LoadObject(type);

        // check version
        char v[2];
        v[0] = loader.LoadByte();
        v[1] = loader.LoadByte();
        if (v[0] != kQDBVersion)
            return QObject();

        // check crc
        uint64_t crc = 0;
        unsigned char* p = (unsigned char*)&crc;
        for (size_t i = 0; i < sizeof(crc); ++ i)
        {
            p[i] = loader.LoadByte();
        }

        const uint64_t expectCrc = crc64(0, (const unsigned char* )data, len - 8);
        if (expectCrc != crc)
            return QObject();

        return obj;
    }
    catch (const std::runtime_error& e) {
        ERR << "RestoreObject with exception: " << e.what();
        return QObject();
    }
}

QError dump(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QObject* val;
    if (QSTORE.GetValue(params[1], val) != QError_ok)
    {
        ReplyError(QError_notExist, reply);
        return QError_notExist;
    }

    std::string str(DumpObject(*val));
    FormatBulk(str, reply);
    return QError_ok;
}

// restore key ttl ser-val replace
QError restore(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QObject obj(RestoreObject(params[3].data(), params[3].size()));
    if (obj.type == QType_invalid)
    {
        char err[] = "-ERR DUMP payload version or checksum are wrong\r\n";
        reply->PushData(err, sizeof err - 1);
        return QError_nop;
    }

    long ttl = 0;
    if (!Strtol(params[2].c_str(), params[2].size(), &ttl))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }

    bool replace = false;
    if (params.size() == 5 && params[4] == "replace")
        replace = true;

    const auto& key = params[1];
    if (!replace && QSTORE.GetObject(key))
    {
        USR << "Can not replace " << key;
        ReplyError(QError_busykey, reply);
        return QError_busykey;
    }

    QSTORE.SetValue(key, std::move(obj));
    if (ttl > 0)
        QSTORE.SetExpireAfter(key, ttl);

    FormatOK(reply);
    return QError_ok;
}

}

