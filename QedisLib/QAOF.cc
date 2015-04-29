
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


/*
 when after fork(), the parent stop aof thread, which means
 the coming aof data will be write to the tmp buffer, not to
 aof file.
 */

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
    char    buf[32];
    size_t  n = snprintf(buf, sizeof buf, "*%lu\n", params.size());
    
    dst.Write(buf, n);
    
    for (size_t i = 0; i < params.size(); ++ i)
    {
        WriteBulkString(params[i], dst);
    }
}
// main thread call this
void   QAOFThreadController::_WriteSelectDB(int db,
                                            const QString& cmd,
                                            OutputBuffer& dst)
{
    if (db == m_lastDb)
        return;

    m_lastDb = db;

    bool  isSelect = (cmd.size() == 6 &&
                      memcmp(cmd.data(), "select", 6) == 0);
    if (!isSelect)
    {
        WriteBulkString("select", 6, dst);
        WriteBulkLong(db, dst);
    }
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
    
    _WriteSelectDB(db, params[0], *dst);
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
            {
                // child  save the db to tmp file
                MemoryFile  file;
                if (file.Open(g_aofTmp, false))
                {
                    std::cerr << "open aof tmp failed\n";
                    exit(-1);
                }
            }
                break;
                
            case -1:
                ERR << "fork aof process failed, errno = " << errno;
                break;
                
            default:
                break;
        }
    }
    
    return QError_ok;
}

static void  SaveStringObject(const QString& key, const QObject& obj, MemoryFile& file)
{
    WriteBulkString("set", 3, file);
    WriteBulkString(key, file);
    
    const PSTRING& pstr = obj.CastString();
    WriteBulkString(*pstr, file);
}


void SaveObject(const QString& key, const QObject& obj, MemoryFile& file)
{
    // set key val
    switch (obj.type)
    {
        case QType_string:
            SaveStringObject(key, obj, file);
            break;
            
        case QType_list:
            break;
            
        case QType_set:
            break;
            
        case QType_sortedSet:
            break;
            
        case QType_hash:
            break;
            
        default:
            assert (false);
            break;
    }
}

