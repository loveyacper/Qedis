
#ifndef BERT_THREADPOOL_H
#define BERT_THREADPOOL_H

#include <string>

#include <set>
#include <vector>
#include "IPC.h"

class Thread;
class Runnable;

class ThreadPool
{
public:
    static ThreadPool& Instance() {
        static ThreadPool  pool;
        return  pool;
    }

    bool ExecuteTask(const SharedPtr<Runnable>& );

    void StopAllThreads();

private:
    ThreadPool() : m_shutdown(false) { }

    typedef std::vector<Thread* > ThreadContainer;
    typedef ThreadContainer::iterator ThreadIterator;

    Mutex               m_threadsLock;
    ThreadContainer     m_threads;
    volatile bool       m_shutdown;


};

#endif

