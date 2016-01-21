
#include "QAOF.h"
#include "Log/Logger.h"
#include "Threads/ThreadPool.h"
#include "QStore.h"
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
void   QAOFThreadController::RewriteDoneHandler(int exitcode, int bysignal)
{
    if (exitcode == 0 && bysignal == 0)
    {
        INF << "save aof success";
        g_rewritePid = -1;
        
        QAOFThreadController::Instance().Join();
        ::rename(g_aofTmp, QAOFThreadController::Instance().aofFile_.c_str());
        QAOFThreadController::Instance().Start();
    }
    else
    {
        ERR << "save aof failed with exitcode " << exitcode << ", signal " << bysignal;
        g_rewritePid = -1;
        
        ::unlink(g_aofTmp);
        QAOFThreadController::Instance().Join();
        QAOFThreadController::Instance().Start();
    }
}


QAOFThreadController& QAOFThreadController::Instance()
{
    static  QAOFThreadController  threadCtrl;
    return  threadCtrl;
}

void  QAOFThreadController::SetAofFile(const QString& name)
{
    aofFile_ = name;
}

bool  QAOFThreadController::ProcessTmpBuffer(BufferSequence& bf)
{
    aofBuffer_.ProcessBuffer(bf);
    
    return bf.count > 0;
}

void  QAOFThreadController::SkipTmpBuffer(size_t n)
{
    aofBuffer_.Skip(n);
}

// main thread  call this
void  QAOFThreadController::Start()
{
    DBG << "start aof thread";
    
    assert(!aofThread_ || !aofThread_->IsAlive());
    
    aofThread_ = std::make_shared<AOFThread>();
    aofThread_->Open(aofFile_.c_str());
    aofThread_->SetAlive();
    
    ThreadPool::Instance().ExecuteTask(std::bind(&AOFThread::Run, aofThread_.get()));
}

// when fork(), parent call stop;
void   QAOFThreadController::Stop()
{
    if (!aofThread_)
        return;
    
    DBG << "stop aof thread";
    aofThread_->Stop();
    QAOFThreadController::Instance().Join();
    aofThread_ = nullptr;
}

// main thread call this
void   QAOFThreadController::_WriteSelectDB(int db, AsyncBuffer& dst)
{
    if (db == lastDb_)
        return;

    lastDb_ = db;
    
    WriteMultiBulkLong(2, dst);
    WriteBulkString("select", 6, dst);
    WriteBulkLong(db, dst);
}

void   QAOFThreadController::SaveCommand(const std::vector<QString>& params, int db)
{
    AsyncBuffer* dst;
    
    if (aofThread_ && aofThread_->IsAlive())
    {
        dst = &aofThread_->buf_;
    }
    else
    {
        dst = &aofBuffer_;
    }
    
    _WriteSelectDB(db, *dst);
    qedis::SaveCommand(params, *dst);
}

QAOFThreadController::AOFThread::~AOFThread()
{
    file_.Close();
}

bool   QAOFThreadController::AOFThread::Flush()
{
    BufferSequence  data;
    buf_.ProcessBuffer(data);
    
    for (size_t i = 0; i < data.count; ++ i)
    {
        file_.Write(data.buffers[i].iov_base, data.buffers[i].iov_len);
    }
    
    buf_.Skip(data.TotalBytes());
    
    return  data.count != 0;
}


void   QAOFThreadController::AOFThread::SaveCommand(const std::vector<QString>& params)
{
    qedis::SaveCommand(params, buf_);
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
            file_.Write(data.buffers[i].iov_base, data.buffers[i].iov_len);
        }
        
        QAOFThreadController::Instance().SkipTmpBuffer(data.TotalBytes());
    }
    
    while (IsAlive())
    {
        if (Flush())
            file_.Sync();
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    Close();
    pro_.set_value();
}

void  QAOFThreadController::Join()
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

QError bgrewriteaof(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    if (g_rewritePid != -1)
    {
        ReplyError(QError_exist, reply);
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
                break;
                
            default:
                break;
        }
    }
    
    QAOFThreadController::Instance().Stop();
    FormatOK(reply);
    return QError_ok;
}

static void  SaveStringObject(const QString& key, const QObject& obj, OutputMemoryFile& file)
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

static void  SaveListObject(const QString& key, const QObject& obj, OutputMemoryFile& file)
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

static void  SaveSetObject(const QString& key, const QObject& obj, OutputMemoryFile& file)
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

static void  SaveZSetObject(const QString& key, const QObject& obj, OutputMemoryFile& file)
{
    const PSSET& zset = obj.CastSortedSet();
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

        WriteBulkString(member, file);
        WriteBulkString(scoreStr, len, file);
    }
}

static void  SaveHashObject(const QString& key, const QObject& obj, OutputMemoryFile& file)
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
    _Reset();
}

bool  QAOFLoader::Load(const char* name)
{
    _Reset();
    
    const int kCRLFLen = 2;
    
    // load file to memory
    InputMemoryFile  file;
    if (!file.Open(name))
        return  false;

    size_t  maxLen = std::numeric_limits<size_t>::max();
    const char* content = file.Read(maxLen);

    // extract commands from file content
    const char* const end = content + maxLen;
    while (content < end)
    {
        switch (state_)
        {
            case State::Init:
                cmds_.resize(cmds_.size() + 1);

                state_ = State::Multi;
                break;

            case Multi:
                assert (*content == '*');
                ++ content;
                
                if (QParseInt::ok != GetIntUntilCRLF(content, end - content, multi_))
                {
                    ERR << "get multi failed";
                    return false;
                }
                else
                {
                    content += kCRLFLen;
                }

                state_ = State::Param;
                break;

            case Param:
                assert (*content == '$');
                ++ content;

                {
                    int   paramLen = 0;
                    if (QParseInt::ok != GetIntUntilCRLF(content, end - content, paramLen))
                    {
                        ERR << "get param len failed";
                        return false;
                    }
                    else
                    {
                        content += kCRLFLen;
                    }

                    if (content + paramLen > end)
                    {
                        ERR << "can not get param, len " << paramLen;
                        return false;
                    }

                    auto&  params = cmds_.back();
                    params.push_back(QString(content, paramLen));
                    content += paramLen + kCRLFLen;

                    if (static_cast<int>(params.size()) == multi_)
                        state_ = Ready;
                }

                break;

            case Ready:
                state_ = State::Init;
                break;
        }
    }

    if (state_ == State::Ready)
        state_ = State::AllReady;

    return state_ == State::AllReady;
}
    
void QAOFLoader::_Reset()
{
    state_ = State::Init;
    multi_ = 0;
    cmds_.clear();
}

}
