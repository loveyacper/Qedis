#include <unistd.h>
#include <signal.h>

#include <cstring>
#include <cassert>
#include "Thread.h"
#include "../Log/Logger.h"


__thread  Logger*  g_log;
__thread  int      g_logLevel;
__thread  int      g_logDest;
    
Thread::Thread(const std::shared_ptr<Runnable>& runnable) :
m_runnable(runnable),
m_handle(INVALID_HANDLE_VALUE),
m_tid(0),
m_suspendCount(0)
{
    m_working = true;
}

Thread::~Thread()
{    
    DBG << "delete thread " << this;
}


void*  Thread::ThreadFunc(void* arg) 
{
    Thread* pThread       = (Thread* ) arg;
    pThread->m_tid        = Thread::GetCurrentThreadId();

    sigset_t mask;
    sigfillset(&mask);
    ::pthread_sigmask(SIG_SETMASK, &mask, NULL);

    while (pThread->m_working)
    {
        pThread->_Run();
        pThread->Suspend();
    }

    DBG << "Exit run thread id " << pThread->m_tid;

    ::pthread_exit(0);
    return 0;
}

void Thread::Sleep(unsigned int seconds)
{
    ::sleep(seconds);
}

void Thread::MSleep(unsigned int mSeconds)
{
    ::usleep(mSeconds * 1000U);
}

bool Thread::Start()
{
    return Thread::_LaunchThread(m_handle, ThreadFunc, this);
}

bool Thread::_LaunchThread(THREAD_HANDLE& handle, PTHREADFUNC func, void* arg)
{
    return 0 == ::pthread_create(&handle, NULL, func, arg);
}

void Thread::_Run()
{
    if (m_runnable)
    {
        m_runnable->Run();
    }
}

void Thread::Stop()
{
    INF << "Set work false " << this;
    m_working = false;
}

void Thread::Join()
{
    if (INVALID_HANDLE_VALUE == m_handle)
        return;

    int ret = ::pthread_join(m_handle, NULL);
    INF << this << " Join result " << ret;
    m_handle = INVALID_HANDLE_VALUE;
}

void Thread::Suspend()
{    
    assert(m_tid == Thread::GetCurrentThreadId());
    INF << "Suspend " << this << ", sem value " << m_sem.Value();
    ++ m_suspendCount;
    m_sem.Wait();
}

void Thread::Resume()
{    
    assert(m_tid != Thread::GetCurrentThreadId());
    INF << "Resume " << this << ", sem value " << m_sem.Value();
    m_sem.Post();
    -- m_suspendCount;
}

THREAD_ID Thread::GetCurrentThreadId()
{
    return ::pthread_self();
}

void Thread::YieldCPU()
{
    ::usleep(100);
}

