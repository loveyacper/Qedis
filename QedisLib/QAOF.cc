
#include "QAOF.h"
#include "Logger.h"
#include "ThreadPool.h"
#include <iostream>
#include <sstream>


pid_t       g_aofPid = -1;

const char* const g_aofFileName = "qedis_appendonly.aof";

QAOFFile::QAOFFile()
{
    Open(g_aofFileName);
}

QAOFFile::~QAOFFile()
{
    Sync();
}

QAOFFile&  QAOFFile::Instance()
{
    static QAOFFile  aof;
    return aof;
}

bool   QAOFFile::Open(const char* file)
{
    if (!m_file.Open(file, false))
    {
        return false;
    }
    
    return true;
}

void   QAOFFile::SaveCommand(const std::vector<QString>& params)
{
    char    buf[128];
    size_t  n = snprintf(buf, sizeof buf, "*%lu\n", params.size());
    
    m_buf.AsyncWrite(buf, n);
    
    for (size_t i = 0; i < params.size(); ++ i)
    {
        n = snprintf(buf, sizeof buf, "$%lu\n", params[i].size());
        m_buf.AsyncWrite(buf, n);
        m_buf.AsyncWrite(params[i].data(), params[i].size());
        m_buf.AsyncWrite("\n", 1);
    }
}

bool   QAOFFile::Loop()
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

bool   QAOFFile::Sync()
{
    return  m_file.Sync();
}

void QAOFFile::SaveDoneHandler(int exitcode, int bysignal)
{
    if (exitcode == 0 && bysignal == 0)
    {
        INF << "save aof success";
        g_aofPid = -1;
    }
    else
    {
        ERR << "save aof failed with exitcode " << exitcode << ", signal " << bysignal;
    }
}


QAOFThread& QAOFThread::Instance()
{
    static QAOFThread  thr;
    return             thr;
}

void  QAOFThread::Start()
{
    std::cout << "start aof thread\n";
    
    m_aofThread.reset(new AOFThread);
    m_aofThread->SetAlive();
    
    ThreadPool::Instance().ExecuteTask(m_aofThread);
}

void   QAOFThread::Stop()
{
    std::cout << "stop aof thread\n";
    
    m_aofThread->Stop();
}

bool   QAOFThread::Update()
{
    return QAOFFile::Instance().Loop();
}

void  QAOFThread::AOFThread::Run()
{
    while (IsAlive())
    {
        if (!QAOFThread::Instance().Update())
            Thread::YieldCPU();
    }
}
