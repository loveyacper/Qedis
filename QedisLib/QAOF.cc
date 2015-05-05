
#include "QAOF.h"
#include "Logger.h"
#include "ThreadPool.h"
#include "QStore.h"
#include <unistd.h>
#include <iostream>
#include <sstream>


pid_t   QAOFThreadController::sm_aofPid = -1;

const char* const g_aofFileName = "qedis_appendonly.aof";
const char* const g_aofTmp = "qedis_appendonly.aof.tmp";


/*****************************************************
 * when after fork(), the parent stop aof thread, which means
 * the coming aof data will be write to the tmp buffer, not to
 * aof file.
 ****************************************************/

template <typename DEST>
static void  WriteBulkString(const char* str, size_t strLen, DEST& dst)
{
    char    tmp[32];
    size_t  n = snprintf(tmp, sizeof tmp, "$%lu\n", strLen);
    
    dst.Write(tmp, n);
    dst.Write(str, strLen);
    dst.Write("\n", 1);
}


template <typename DEST>
static void  WriteBulkString(const QString& str, DEST& dst)
{
    WriteBulkString(str.data(), str.size(), dst);
}

template <typename DEST>
static void  WriteMultiBulkLong(long val, DEST& dst)
{
    char    tmp[32];
    size_t  n = snprintf(tmp, sizeof tmp, "*%lu\n", val);
    dst.Write(tmp, n);
}

template <typename DEST>
static void  WriteBulkLong(long val, DEST& dst)
{
    char    tmp[32];
    size_t  n = snprintf(tmp, sizeof tmp, "%lu", val);
    
    WriteBulkString(tmp, n, dst);
}

void   QAOFThreadController::SaveDoneHandler(int exitcode, int bysignal)
{
    if (exitcode == 0 && bysignal == 0)
    {
        INF << "save aof success";
        sm_aofPid = -1;
        
        QAOFThreadController::Instance().Join();
        ::rename(g_aofTmp, g_aofFileName);
        // rename file;
        QAOFThreadController::Instance().Start();
    }
    else
    {
        ERR << "save aof failed with exitcode " << exitcode << ", signal " << bysignal;
    }
}


QAOFThreadController& QAOFThreadController::Instance()
{
    static  QAOFThreadController  threadCtrl;
    return  threadCtrl;
}


bool  QAOFThreadController::ProcessTmpBuffer(BufferSequence& bf)
{
    m_aofBuffer.ProcessBuffer(bf);
    
    return bf.count > 0;
}

void  QAOFThreadController::SkipTmpBuffer(size_t n)
{
    m_aofBuffer.Skip(n);
}

// main thread  call this
void  QAOFThreadController::Start()
{
    std::cout << "start aof thread\n";
    
    assert(!m_aofThread || !m_aofThread->IsAlive());
    
    m_aofThread.reset(new AOFThread);
    m_aofThread->Open(g_aofFileName);
    m_aofThread->SetAlive();
    
    ThreadPool::Instance().ExecuteTask(m_aofThread);
}

// when fork(), parent call stop;
void   QAOFThreadController::Stop()
{
    assert (m_aofThread);
    
    std::cout << "stop aof thread\n";
    m_aofThread->Stop();
}

template <typename DEST>
static  void SaveCommand(const std::vector<QString>& params, DEST& dst)
{
    WriteMultiBulkLong(params.size(), dst);
    
    for (size_t i = 0; i < params.size(); ++ i)
    {
        WriteBulkString(params[i], dst);
    }
}
// main thread call this
void   QAOFThreadController::_WriteSelectDB(int db, OutputBuffer& dst)
{
    if (db == m_lastDb)
        return;

    m_lastDb = db;
    
    WriteBulkString("select", 6, dst);
    WriteBulkLong(db, dst);
}

void   QAOFThreadController::SaveCommand(const std::vector<QString>& params, int db)
{
    OutputBuffer* dst;
    
    if (m_aofThread && m_aofThread->IsAlive())
    {
        dst = &m_aofThread->m_buf;
    }
    else
    {
        dst = &m_aofBuffer;
    }
    
    _WriteSelectDB(db, *dst);
    ::SaveCommand(params, *dst);
}

QAOFThreadController::AOFThread::~AOFThread()
{
    m_file.Close();
}

bool   QAOFThreadController::AOFThread::Flush()
{
    BufferSequence  data;
    m_buf.ProcessBuffer(data);
    
    for (size_t i = 0; i < data.count; ++ i)
    {
        m_file.Write(data.buffers[i].iov_base, data.buffers[i].iov_len);
    }
    
    m_buf.Skip(data.TotalBytes());
    
    return  data.count != 0;
}


void   QAOFThreadController::AOFThread::SaveCommand(const std::vector<QString>& params)
{
    ::SaveCommand(params, m_buf);
}

void  QAOFThreadController::AOFThread::Run()
{
    assert (IsAlive());
   
    // CHECK aof temp buffer first!
    BufferSequence  data;
    while (QAOFThreadController::Instance().ProcessTmpBuffer(data))
    {
        for (size_t i = 0; i < data.count; ++ i)
        {
            m_file.Write(data.buffers[i].iov_base, data.buffers[i].iov_len);
        }
        
        QAOFThreadController::Instance().SkipTmpBuffer(data.TotalBytes());
    }
    
    while (IsAlive())
    {
        if (!Flush() && !m_file.Sync())  // TODO : fixme sync hz
            Thread::YieldCPU();
    }
    
    Close();
    m_sem.Post();
}

void  QAOFThreadController::Join()
{
    m_aofThread->m_sem.Wait();
}

static void SaveExpire(const QString& key, uint64_t absMs, MemoryFile& file)
{
    WriteBulkLong(3, file);
    WriteBulkString("expire", 6, file);
    WriteBulkString(key, file);
    WriteBulkLong(absMs, file);
}

// child  save the db to tmp file
static void SaveObject(const QString& key, const QObject& obj, MemoryFile& file);
static void RewriteProcess()
{
    MemoryFile  file;
    if (!file.Open(g_aofTmp, false))
    {
        perror("open tmp failed");
        std::cerr << "open aof tmp failed\n";
        exit(-1);
    }

    for (int dbno = 0; dbno < 16; ++ dbno)
    {
        QSTORE.SelectDB(dbno);
        if (QSTORE.DBSize() == 0)
            continue;
        // select db;
        WriteMultiBulkLong(2, file);
        WriteBulkString("select", 6, file);
        WriteBulkLong(dbno, file);

        uint64_t  now = ::Now();
        for (auto kv(QSTORE.begin()); kv != QSTORE.end(); ++ kv)
        {
            int64_t ttl = QSTORE.TTL(kv->first, now);
            if (ttl == -2)
                continue;

            SaveObject(kv->first, kv->second, file);
            if (ttl > 0)
                SaveExpire(kv->first, ttl + now, file);
        }
    }
}

QError bgrewriteaof(const std::vector<QString>& params, UnboundedBuffer& reply)
{
    if (QAOFThreadController::sm_aofPid != -1)
    {
        ReplyError(QError_exist, reply);
    }
    else
    {
        QAOFThreadController::sm_aofPid = fork();
        switch (QAOFThreadController::sm_aofPid)
        {
            case 0:
                RewriteProcess();
                exit(0);
                
            case -1:
                ERR << "fork aof process failed, errno = " << errno;
                break;
                
            default:
                break;
        }
    }
    
    QAOFThreadController::Instance().Stop();
    FormatOK(reply);
    return QError_ok;
}

static void  SaveStringObject(const QString& key, const QObject& obj, MemoryFile& file)
{
    WriteMultiBulkLong(3, file);
    WriteBulkString("set", 3, file);
    WriteBulkString(key, file);
    
    const PSTRING& str = obj.CastString();
    if (obj.encoding == QEncode_raw)
    {
        WriteBulkString(*str, file);
    }
    else
    {
        intptr_t val = (intptr_t)str.get();
        WriteBulkLong(val, file);
    }
}

static void  SaveListObject(const QString& key, const QObject& obj, MemoryFile& file)
{
    const PLIST&  list = obj.CastList();
    if (list->empty())
        return;

    WriteMultiBulkLong(list->size() + 2, file); // rpush listname + elems
    WriteBulkString("rpush", 5, file);
    WriteBulkString(key, file);

    for (const auto& elem : *list)
    {
        WriteBulkString(elem, file);
    }
}

static void  SaveSetObject(const QString& key, const QObject& obj, MemoryFile& file)
{
    const PSET& set = obj.CastSet();
    if (set->empty())
        return;

    WriteMultiBulkLong(set->size() + 2, file); // sadd set_name + elems
    WriteBulkString("sadd", 4, file);
    WriteBulkString(key, file);

    for (const auto& elem : *set)
    {
        WriteBulkString(elem, file);
    }
}

static void  SaveZSetObject(const QString& key, const QObject& obj, MemoryFile& file)
{
    const PSSET& zset = obj.CastSortedSet();
    if (zset->Size() == 0)
        return;

    WriteMultiBulkLong(2 * zset->Size() + 2, file); // zadd zset_name + (score + member)
    WriteBulkString("zadd", 4, file);
    WriteBulkString(key, file);

    for (auto it(zset->begin()); it != zset->end(); ++ it)
    {
        const QString& member = it->first;
        double score          = it->second;

        char scoreStr[32];
        int  len = Double2Str(scoreStr, sizeof scoreStr, score);

        WriteBulkString(member, file);
        WriteBulkString(scoreStr, len, file);
    }
}

static void  SaveHashObject(const QString& key, const QObject& obj, MemoryFile& file)
{
    const PHASH& hash = obj.CastHash();
    if (hash->empty())
        return;

    WriteMultiBulkLong(2* hash->size() + 2, file); // hmset hash_name + (key + value)
    WriteBulkString("hmset", 5, file);
    WriteBulkString(key, file);

    for (const auto& pair : *hash)
    {
        WriteBulkString(pair.first, file);
        WriteBulkString(pair.second, file);
    }
}


static void SaveObject(const QString& key, const QObject& obj, MemoryFile& file)
{
    // set key val
    switch (obj.type)
    {
        case QType_string:
            SaveStringObject(key, obj, file);
            break;
            
        case QType_list:
            SaveListObject(key, obj, file);
            break;
            
        case QType_set:
            SaveSetObject(key, obj, file);
            break;
            
        case QType_sortedSet:
            SaveZSetObject(key, obj, file);
            break;
            
        case QType_hash:
            SaveHashObject(key, obj, file);
            break;
            
        default:
            assert (false);
            break;
    }
}

