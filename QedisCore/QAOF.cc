
#include "QAOF.h"
#include "Log/Logger.h"
#include "Threads/ThreadPool.h"
#include "QStore.h"
#include "QConfig.h"
#include "QProtoParser.h"
#include <unistd.h>
#include <sstream>

namespace qedis
{

const char* const g_aofTmp = "qedis_appendonly.aof.tmp";

pid_t             g_rewritePid = -1;


/*****************************************************
 * when after fork(), the parent stop aof thread, which means
 * the coming aof data will be write to the tmp buffer, not to
 * aof file.
 ****************************************************/
void QAOFThreadController::RewriteDoneHandler(int exitRet, int whatSignal)
{
    g_rewritePid = -1;

    if (exitRet == 0 && whatSignal == 0)
    {
        INF << "save aof success";
        ::rename(g_aofTmp, g_config.appendfilename.c_str());
    }
    else
    {
        ERR << "save aof failed with exit result " << exitRet << ", signal " << whatSignal;
        ::unlink(g_aofTmp);
    }
        
    QAOFThreadController::Instance().Start();
}


QAOFThreadController& QAOFThreadController::Instance()
{
    static QAOFThreadController threadCtrl;
    return threadCtrl;
}

bool QAOFThreadController::ProcessTmpBuffer(BufferSequence& bf)
{
    aofBuffer_.ProcessBuffer(bf);
    return bf.count > 0;
}

void QAOFThreadController::SkipTmpBuffer(size_t n)
{
    aofBuffer_.Skip(n);
}

// main thread  call this
void QAOFThreadController::Start()
{
    DBG << "start aof thread";
    
    assert(!aofThread_ || !aofThread_->IsAlive());
    
    aofThread_ = std::make_shared<AOFThread>();
    aofThread_->SetAlive();
    
    ThreadPool::Instance().ExecuteTask(std::bind(&AOFThread::Run, aofThread_));
}

// when fork(), parent call stop;
void QAOFThreadController::Stop()
{
    if (!aofThread_)
        return;
    
    DBG << "stop aof thread";
    aofThread_->Stop();
    QAOFThreadController::Instance().Join();
    aofThread_ = nullptr;
}

// main thread call this
void QAOFThreadController::_WriteSelectDB(int db, AsyncBuffer& dst)
{
    if (db == lastDb_)
        return;

    lastDb_ = db;
    
    WriteMultiBulkLong(2, dst);
    WriteBulkString("select", 6, dst);
    WriteBulkLong(db, dst);
}

void QAOFThreadController::SaveCommand(const std::vector<QString>& params, int db)
{
    AsyncBuffer* dst;
    
    if (aofThread_ && aofThread_->IsAlive())
        dst = &aofThread_->buf_;
    else
        dst = &aofBuffer_;
    
    _WriteSelectDB(db, *dst);
    qedis::SaveCommand(params, *dst);
}

QAOFThreadController::AOFThread::~AOFThread()
{
    file_.Close();
}

bool QAOFThreadController::AOFThread::Flush()
{
    BufferSequence  data;
    buf_.ProcessBuffer(data);
    if (data.count == 0)
        return false;
    
    if (!file_.IsOpen())
        file_.Open(g_config.appendfilename.c_str());
    
    for (size_t i = 0; i < data.count; ++ i)
    {
        file_.Write(data.buffers[i].iov_base, data.buffers[i].iov_len);
    }
    
    buf_.Skip(data.TotalBytes());
    
    return data.count != 0;
}


void QAOFThreadController::AOFThread::SaveCommand(const std::vector<QString>& params)
{
    qedis::SaveCommand(params, buf_);
}

void QAOFThreadController::AOFThread::Run()
{
    assert (IsAlive());

    // CHECK aof temp buffer first!
    BufferSequence data;
    while (QAOFThreadController::Instance().ProcessTmpBuffer(data))
    {
        if (!file_.IsOpen())
            file_.Open(g_config.appendfilename.c_str());
        
        for (size_t i = 0; i < data.count; ++ i)
        {
            file_.Write(data.buffers[i].iov_base, data.buffers[i].iov_len);
        }
        
        QAOFThreadController::Instance().SkipTmpBuffer(data.TotalBytes());
    }
    
    while (IsAlive())
    {
        //sync incrementally, always, the redis sync policy is useless
        if (Flush())
            file_.Sync();
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    file_.Close();
    pro_.set_value();
}

void QAOFThreadController::Join()
{
    if (aofThread_)
        aofThread_->pro_.get_future().wait();
}

static void SaveExpire(const QString& key, uint64_t absMs, OutputMemoryFile& file)
{
    WriteBulkLong(3, file);
    WriteBulkString("expire", 6, file);
    WriteBulkString(key, file);
    WriteBulkLong(absMs, file);
}

// child  save the db to tmp file
static void SaveObject(const QString& key, const QObject& obj, OutputMemoryFile& file);
static void RewriteProcess()
{
    OutputMemoryFile  file;
    if (!file.Open(g_aofTmp, false))
    {
        perror("open tmp failed");
        _exit(-1);
    }

    for (int dbno = 0; true; ++ dbno)
    {
        if (QSTORE.SelectDB(dbno) == -1)
            break;
        
        if (QSTORE.DBSize() == 0)
            continue;

        // select db
        WriteMultiBulkLong(2, file);
        WriteBulkString("select", 6, file);
        WriteBulkLong(dbno, file);

        const auto now = ::Now();
        for (const auto& kv : QSTORE)
        {
            int64_t ttl = QSTORE.TTL(kv.first, now);
            if (ttl == QStore::ExpireResult::expired)
                continue;

            SaveObject(kv.first, kv.second, file);
            if (ttl > 0)
                SaveExpire(kv.first, ttl + now, file);
        }
    }
}

QError bgrewriteaof(const std::vector<QString>& , UnboundedBuffer* reply)
{
    if (g_rewritePid != -1)
    {
        reply->PushData("-ERR aof already in progress\r\n",
                 sizeof "-ERR aof already in progress\r\n" - 1);
        return QError_ok;
    }
    else
    {
        g_rewritePid = fork();
        switch (g_rewritePid)
        {
            case 0:
                RewriteProcess();
                _exit(0);
                
            case -1:
                ERR << "fork aof process failed, errno = " << errno;
                reply->PushData("-ERR aof rewrite failed\r\n",
                         sizeof "-ERR aof rewrite failed\r\n" - 1);
                return QError_ok;
                
            default:
                break;
        }
    }
    
    QAOFThreadController::Instance().Stop();
    FormatOK(reply);
    return QError_ok;
}

static void SaveStringObject(const QString& key, const QObject& obj, OutputMemoryFile& file)
{
    WriteMultiBulkLong(3, file);
    WriteBulkString("set", 3, file);
    WriteBulkString(key, file);

    auto str = GetDecodedString(&obj);
    WriteBulkString(*str, file);
}

static void SaveListObject(const QString& key, const QObject& obj, OutputMemoryFile& file)
{
    auto list = obj.CastList();
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

static void SaveSetObject(const QString& key, const QObject& obj, OutputMemoryFile& file)
{
    auto set = obj.CastSet();
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

static void  SaveZSetObject(const QString& key, const QObject& obj, OutputMemoryFile& file)
{
    auto zset = obj.CastSortedSet();
    if (zset->Size() == 0)
        return;

    WriteMultiBulkLong(2 * zset->Size() + 2, file); // zadd zset_name + (score + member)
    WriteBulkString("zadd", 4, file);
    WriteBulkString(key, file);

    for (const auto& pair : *zset)
    {
        const QString& member = pair.first;
        double score          = pair.second;

        char scoreStr[32];
        int  len = Double2Str(scoreStr, sizeof scoreStr, score);

        WriteBulkString(scoreStr, len, file);
        WriteBulkString(member, file);
    }
}

static void SaveHashObject(const QString& key, const QObject& obj, OutputMemoryFile& file)
{
    auto hash = obj.CastHash();
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


static void SaveObject(const QString& key, const QObject& obj, OutputMemoryFile& file)
{
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


QAOFLoader::QAOFLoader()
{
}

bool QAOFLoader::Load(const char* name)
{
    if (::access(name, F_OK) != 0)
        return false;

    {
        // truncate tail trash zeroes
        OutputMemoryFile file;
        file.Open(name);
        file.TruncateTailZero();
    }
    
    // load file to memory
    InputMemoryFile file;
    if (!file.Open(name))
        return  false;

    size_t maxLen = std::numeric_limits<size_t>::max();
    const char* content = file.Read(maxLen);
    
    if (maxLen == 0)
        return false;

    QProtoParser parser;
    // extract commands from file content
    const char* const end = content + maxLen;
    while (content < end)
    {
        parser.Reset();
        if (QParseResult::ok != parser.ParseRequest(content, end))
        {
            ERR << "Load aof failed";
            return false;
        }

        cmds_.push_back(parser.GetParams());
    }

    return true;
}
    
}
