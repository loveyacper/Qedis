
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

NetThread::NetThread() : running_(true), newCnt_(0)
{
#if defined(__gnu_linux__)
    poller_.reset(new Epoller);
#else
    poller_.reset(new Kqueue);
#endif
}

NetThread::~NetThread()
{
//    DBG << "close epollfd " << epfd_;
//    TEMP_FAILURE_RETRY(::close(epfd_));
}

void NetThread::AddSocket(PSOCKET task, uint32_t events)
{
    std::lock_guard<std::mutex>    guard(mutex_);
    newTasks_.push_back(std::make_pair(task, events)); 
    ++ newCnt_;

    assert (newCnt_ == static_cast<int>(newTasks_.size()));
}

void NetThread::ModSocket(PSOCKET task, uint32_t events)
{
    poller_->ModSocket(task->GetSocket(), events, task.get());
}

void NetThread::RemoveSocket(std::deque<PSOCKET >::iterator& iter)
{
    poller_->DelSocket((*iter)->GetSocket(), 0);
    iter = tasks_.erase(iter);
}

void NetThread::_TryAddNewTasks()
{
    if (newCnt_ > 0 && mutex_.try_lock())
    { 
        NewTasks  tmp;
        newTasks_.swap(tmp); 
        newCnt_ = 0; 
        mutex_.unlock();

        NewTasks::const_iterator iter(tmp.begin()),
                                 end (tmp.end());

        for (; iter != end; ++ iter) 
            _AddSocket(iter->first, iter->second); 
    }
}

void NetThread::_AddSocket(PSOCKET task, uint32_t events)
{
    if (poller_->AddSocket(task->GetSocket(), events, task.get()))
        tasks_.push_back(task);
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

        if (tasks_.empty())
        {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
        }

        const int nReady = poller_->Poll(firedEvents_, static_cast<int>(tasks_.size()), 1);
        for (int i = 0; i < nReady; ++ i)
        {
            assert (!(firedEvents_[i].events & EventTypeWrite));

            Socket* pSock = (Socket* )firedEvents_[i].userdata;

            if (firedEvents_[i].events & EventTypeRead)
            {
                if (!pSock->OnReadable())
                {
                    pSock->OnError();
                }
            }

            if (firedEvents_[i].events & EventTypeError)
            {
                WITH_LOG(ERR << "recv thread, on error, socket " << pSock->GetSocket());
                pSock->OnError();
            }
        }
        
        if (nReady == 0)
            loopCount *= 2;
        
        if (++ loopCount < 100000)
            continue;

        loopCount = 0;

        for (std::deque<PSOCKET >::iterator  it(tasks_.begin());
             it != tasks_.end();
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
        for (it = tasks_.begin(); it != tasks_.end(); )
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
                if (pSock->epollOut_)
                    ++ nOut;

                ++ it;
            }
        }
        
        if (nOut == 0)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
        }

        const int nReady = poller_->Poll(firedEvents_, static_cast<int>(tasks_.size()), 1);
        for (int i = 0; i < nReady; ++ i)
        {
            Socket* pSock = (Socket* )firedEvents_[i].userdata;
            
            assert (!(firedEvents_[i].events & EventTypeRead));
            if (firedEvents_[i].events & EventTypeWrite)
            {
                if (!pSock->OnWritable())
                {
                    WITH_LOG(ERR << "on write error, socket " << pSock->GetSocket());
                    pSock->OnError();
                }
            }
            
            if (firedEvents_[i].events & EventTypeError)
            {
                WITH_LOG(ERR << "send thread, on error, socket " << pSock->GetSocket());
                pSock->OnError();
            }
        }
    }
}


void NetThreadPool::StopAllThreads()
{
    recvThread_->Stop();
    recvThread_.reset();
    sendThread_->Stop();
    sendThread_.reset();

    WITH_LOG(INF << "Stop all recv and send threads");
}

bool NetThreadPool::AddSocket(PSOCKET sock, uint32_t  events)
{
    if (events & EventTypeRead)
    {
        recvThread_->AddSocket(sock, EventTypeRead);
    }

    if (events & EventTypeWrite)
    {
        sendThread_->AddSocket(sock, EventTypeWrite);
    }

    return true;
}

bool NetThreadPool::StartAllThreads()
{
    recvThread_.reset(new RecvThread);
    sendThread_.reset(new SendThread);
    
    ThreadPool::Instance().ExecuteTask(std::bind(&RecvThread::Run, recvThread_.get()));
    ThreadPool::Instance().ExecuteTask(std::bind(&SendThread::Run, sendThread_.get()));

    return  true;
}
    

void NetThreadPool::EnableRead(const std::shared_ptr<Socket>& sock)
{
    recvThread_->ModSocket(sock, EventTypeRead);
}

void NetThreadPool::EnableWrite(const std::shared_ptr<Socket>& sock)
{
    sendThread_->ModSocket(sock, EventTypeWrite);
}
   
void NetThreadPool::DisableRead(const std::shared_ptr<Socket>& sock)
{
    recvThread_->ModSocket(sock, 0);
}

void NetThreadPool::DisableWrite(const std::shared_ptr<Socket>& sock)
{
    sendThread_->ModSocket(sock, 0);
}

}

