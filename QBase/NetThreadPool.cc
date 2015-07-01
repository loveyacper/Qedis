
#include "NetThreadPool.h"
#include "StreamSocket.h"
#include "Log/Logger.h"
#include "Timer.h"

#if defined(__gnu_linux__)
#include "EPoller.h"
#elif defined(__APPLE__)
#include "Kqueue.h"
#else
#error "Only support osx and linux"
#endif

#include <cassert>
#include <errno.h>


namespace Internal
{

NetThread::NetThread() : m_running(true), m_newCnt(0)
{
#if defined(__gnu_linux__)
    m_poller.reset(new Epoller);
#else
    m_poller.reset(new Kqueue);
#endif
}

NetThread::~NetThread()
{
//    DBG << "close epollfd " << m_epfd;
//    TEMP_FAILURE_RETRY(::close(m_epfd));
}

void NetThread::AddSocket(PSOCKET task, uint32_t events)
{
    std::lock_guard<std::mutex>    guard(m_mutex);
    m_newTasks.push_back(std::make_pair(task, events)); 
    ++ m_newCnt;

    assert (m_newCnt == static_cast<int>(m_newTasks.size()));
}

void NetThread::ModSocket(PSOCKET task, uint32_t events)
{
    m_poller->ModSocket(task->GetSocket(), events, task.get());
}

void NetThread::RemoveSocket(std::deque<PSOCKET >::iterator& iter)
{
    m_poller->DelSocket((*iter)->GetSocket(), 0);
    iter = m_tasks.erase(iter);
}

void NetThread::_TryAddNewTasks()
{
    if (m_newCnt > 0 && m_mutex.try_lock())
    { 
        NewTasks  tmp;
        m_newTasks.swap(tmp); 
        m_newCnt = 0; 
        m_mutex.unlock();

        NewTasks::const_iterator iter(tmp.begin()),
                                 end (tmp.end());

        for (; iter != end; ++ iter) 
            _AddSocket(iter->first, iter->second); 
    }
}

void NetThread::_AddSocket(PSOCKET task, uint32_t events)
{
    if (m_poller->AddSocket(task->GetSocket(), events, task.get()))
        m_tasks.push_back(task);
}

//////////////////////////////////
void RecvThread::Run()
{
    // init log;
    g_logLevel = logALL;
    g_logDest  = logFILE;
    if (g_logLevel && g_logDest)
    {
        g_log = LogManager::Instance().CreateLog(g_logLevel, g_logDest, "recvthread_log");
    }

    std::deque<PSOCKET >::iterator    it;

    int    loopCount = 0;
    while (IsAlive())
    {
        _TryAddNewTasks();

        if (m_tasks.empty())
        {
            Thread::YieldCPU();
            continue;
        }

        const int nReady = m_poller->Poll(m_firedEvents, static_cast<int>(m_tasks.size()), 1);
        for (int i = 0; i < nReady; ++ i)
        {
            assert (!(m_firedEvents[i].events & EventTypeWrite));

            Socket* pSock = (Socket* )m_firedEvents[i].userdata;

            if (m_firedEvents[i].events & EventTypeRead)
            {
                if (!pSock->OnReadable())
                {
                    pSock->OnError();
                }
            }

            if (m_firedEvents[i].events & EventTypeError)
            {
                ERR << "recv thread, on error, socket " << pSock->GetSocket();
                pSock->OnError();
            }
        }
        
        if (nReady == 0)
            loopCount *= 2;
        
        if (++ loopCount < 100000)
            continue;

        loopCount = 0;

        for (std::deque<PSOCKET >::iterator  it(m_tasks.begin());
             it != m_tasks.end();
             )
        {
            if ((*it)->Invalid())
            {
                NetThreadPool::Instance().DisableRead(*it);
                RemoveSocket(it);
            }
            else
            {
                ++ it;
            }
        }
    }
}

void SendThread::Run( )
{
    // init log;
    g_logLevel = logALL;
    g_logDest  = logFILE;
    if (g_logLevel && g_logDest)
    {
        g_log = LogManager::Instance().CreateLog(g_logLevel, g_logDest, "sendthread_log");
    }
    
    std::deque<PSOCKET >::iterator    it;
    
    while (IsAlive())
    {
        _TryAddNewTasks();

        size_t  nOut = 0;
        for (it = m_tasks.begin(); it != m_tasks.end(); )
        {
            Socket::SocketType  type  = (*it)->GetSocketType();
            Socket*  pSock = (*it).get();
            
            if (type == Socket::SocketType_Stream)
            {
                StreamSocket*  pTcpSock = static_cast<StreamSocket* >(pSock);
                if (!pTcpSock->Send())
                    pTcpSock->OnError();
            }
            
            if (pSock->Invalid())
            {
                NetThreadPool::Instance().DisableWrite(*it);
                RemoveSocket(it);
            }
            else
            {
                if (pSock->m_epollOut)
                    ++ nOut;

                ++ it;
            }
        }
        
        if (nOut == 0)
        {
            Thread::YieldCPU();
            continue;
        }

        const int nReady = m_poller->Poll(m_firedEvents, static_cast<int>(m_tasks.size()), 1);
        for (int i = 0; i < nReady; ++ i)
        {
            Socket* pSock = (Socket* )m_firedEvents[i].userdata;
            
            assert (!(m_firedEvents[i].events & EventTypeRead));
            if (m_firedEvents[i].events & EventTypeWrite)
            {
                if (!pSock->OnWritable())
                {
                    ERR << "on write error, socket " << pSock->GetSocket();
                    pSock->OnError();
                }
            }
            
            if (m_firedEvents[i].events & EventTypeError)
            {
                ERR << "send thread, on error, socket " << pSock->GetSocket();
                pSock->OnError();
            }
        }
    }
}


void NetThreadPool::StopAllThreads()
{
    m_recvThread->Stop();
    m_sendThread->Stop();

    INF << "Stop all recv and send threads";
}

bool NetThreadPool::AddSocket(PSOCKET sock, uint32_t  events)
{
    if (events & EventTypeRead)
    {
        m_recvThread->AddSocket(sock, EventTypeRead);
    }

    if (events & EventTypeWrite)
    {
        m_sendThread->AddSocket(sock, EventTypeWrite);
    }

    return true;
}

bool NetThreadPool::StartAllThreads()
{
    m_recvThread.reset(new RecvThread);
    m_sendThread.reset(new SendThread);

    if (!ThreadPool::Instance().ExecuteTask(m_recvThread) ||
        !ThreadPool::Instance().ExecuteTask(m_sendThread))
        return false;

    return  true;
}
    

void NetThreadPool::EnableRead(const std::shared_ptr<Socket>& sock)
{
    m_recvThread->ModSocket(sock, EventTypeRead);
}

void NetThreadPool::EnableWrite(const std::shared_ptr<Socket>& sock)
{
    m_sendThread->ModSocket(sock, EventTypeWrite);
}
   
void NetThreadPool::DisableRead(const std::shared_ptr<Socket>& sock)
{
    m_recvThread->ModSocket(sock, 0);
}

void NetThreadPool::DisableWrite(const std::shared_ptr<Socket>& sock)
{
    m_sendThread->ModSocket(sock, 0);
}

}

