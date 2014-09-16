
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
    m_poller.Reset(new Epoller);
#else
    m_poller.Reset(new Kqueue);
#endif
}

NetThread::~NetThread()
{
//    DBG << "close epollfd " << m_epfd;
//    TEMP_FAILURE_RETRY(::close(m_epfd));
}

void NetThread::AddSocket(PSOCKET task, uint32_t events)
{
    ScopeMutex    guard(m_mutex);
    m_newTasks.push_back(std::make_pair(task, events)); 
    ++ m_newCnt;

    assert (m_newCnt == static_cast<int>(m_newTasks.size()));
}

void NetThread::ModSocket(PSOCKET task, uint32_t events)
{
    m_poller->ModSocket(task->GetSocket(), events, task.Get());
}

void NetThread::RemoveSocket(std::deque<PSOCKET >::iterator& iter)
{
    m_poller->DelSocket((*iter)->GetSocket(), 0);
    iter = m_tasks.erase(iter);
}

void NetThread::_TryAddNewTasks()
{
    if (m_newCnt > 0 && m_mutex.TryLock()) 
    { 
        NewTasks  tmp;
        m_newTasks.swap(tmp); 
        m_newCnt = 0; 
        m_mutex.Unlock();

        NewTasks::const_iterator iter(tmp.begin()),
                                 end (tmp.end());

        for (; iter != end; ++ iter) 
            _AddSocket(iter->first, iter->second); 
    }
}

void NetThread::_AddSocket(PSOCKET task, uint32_t events)
{
    if (m_poller->AddSocket(task->GetSocket(), events, task.Get()))
        m_tasks.push_back(task);
}

//////////////////////////////////
void RecvThread::Run()
{
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
                    LOCK_SDK_LOG; 
                    ERR << "on read error, socket " << pSock->GetSocket();
                    UNLOCK_SDK_LOG; 
                }
            }

            if (m_firedEvents[i].events & EventTypeError)
            {
                pSock->OnError();
                LOCK_SDK_LOG; 
                ERR << "recv thread, on error, socket " << pSock->GetSocket();
                UNLOCK_SDK_LOG; 
            }
        }
        
        if (nReady == 0)
            loopCount *= 2;
        
        if (++ loopCount < 10000)
            continue;

        loopCount = 0;

        for (std::deque<PSOCKET >::iterator  it(m_tasks.begin());
             it != m_tasks.end();
             )
        {
            if ((*it)->Invalid())
            {
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
    std::deque<PSOCKET >::iterator    it;
    
    int  loopCnt = 0;
    while (IsAlive())
    {
        _TryAddNewTasks();

        if (++ loopCnt > 10000)
        {
            loopCnt = 0;
            for (it = m_tasks.begin(); it != m_tasks.end(); )
            {
                Socket::SocketType  type  = (*it)->GetSocketType();
                Socket*  pSock = (*it).Get();
                
                bool hasDataToSend = false;
                if (type == Socket::SocketType_Stream)
                {
                    StreamSocket*  pTcpSock = static_cast<StreamSocket* >(pSock);
                    hasDataToSend = pTcpSock->HasDataToSend();
                }
                
                if (pSock->Invalid() && !hasDataToSend)
                {
                    RemoveSocket(it);
                }
                else
                {
                    ++ it;
                }
            }
        }

        if (m_tasks.empty())
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
                    pSock->OnError();
                LOCK_SDK_LOG; 
                    ERR << "on write error, socket " << pSock->GetSocket();
                UNLOCK_SDK_LOG; 
                }
            }
            
            if (m_firedEvents[i].events & EventTypeError)
            {
                pSock->OnError();
                LOCK_SDK_LOG; 
                ERR << "send thread, on error, socket " << pSock->GetSocket();
                UNLOCK_SDK_LOG; 
            }
        }
        
        if (nReady == 0)
            loopCnt *= 2;
    }
}


void NetThreadPool::StopAllThreads()
{
    m_recvThread->Stop();
    m_sendThread->Stop();

    LOCK_SDK_LOG; 
    INF << "Stop all recv and send threads";
    UNLOCK_SDK_LOG; 
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
    m_recvThread.Reset(new RecvThread);
    m_sendThread.Reset(new SendThread);

    if (!ThreadPool::Instance().ExecuteTask(m_recvThread) ||
        !ThreadPool::Instance().ExecuteTask(m_sendThread))
        return false;

    return  true;
}
    

void NetThreadPool::EnableRead(const SharedPtr<Socket>& sock)
{
    m_recvThread->ModSocket(sock, EventTypeRead);
}

void NetThreadPool::EnableWrite(const SharedPtr<Socket>& sock)
{
    m_sendThread->ModSocket(sock, EventTypeWrite);
}
   
void NetThreadPool::DisableRead(const SharedPtr<Socket>& sock)
{
    m_recvThread->ModSocket(sock, 0);
}

void NetThreadPool::DisableWrite(const SharedPtr<Socket>& sock)
{
    m_sendThread->ModSocket(sock, 0);
}

}

