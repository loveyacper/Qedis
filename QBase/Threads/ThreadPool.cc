#include "Thread.h"
#include "ThreadPool.h"
#include "../Log/Logger.h"

bool ThreadPool::ExecuteTask(const std::shared_ptr<Runnable>& toRun)
{
    if (!toRun || m_shutdown)
        return false;

    Thread* thread = NULL;
    bool newThread = false;

    std::lock_guard<std::mutex>  guard(m_threadsLock);

    if (m_shutdown)
        return false;

    ThreadIterator  it(m_threads.begin());
    while (it != m_threads.end())
    {
        if ((*it)->Idle())
        {
            thread = *it;
            break;
        }
        else
        {
            ++ it;
        }
    }

    if (NULL == thread)
    {
        thread   = new Thread(toRun);
        newThread= true;
        m_threads.push_back(thread);
        INF << "new thread " << thread;
    }
    else
    {
        thread->SetTask(toRun);
        INF << "old thread " << thread;
    }

    INF << "Thread size " << m_threads.size();

    if (newThread)
        return thread->Start();
    else
        thread->Resume();

    return true;
}

void ThreadPool::StopAllThreads()
{
    m_threadsLock.lock();
    m_shutdown = true;
    ThreadContainer  tmp(m_threads);
    m_threads.clear();
    m_threadsLock.unlock();

    for (ThreadIterator it(tmp.begin()); it != tmp.end(); ++ it)
    {
        (*it)->Stop();
        (*it)->Resume();
        (*it)->Join();
        delete *it;
    }
}

