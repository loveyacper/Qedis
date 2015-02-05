
#ifndef BERT_THREADPOOL_H
#define BERT_THREADPOOL_H

#include <string>

#include <set>
#include <vector>
#include <mutex>

class Thread;
class Runnable;

class ThreadPool
{
public:
    static ThreadPool& Instance() {
        static ThreadPool  pool;
        return  pool;
    }

    bool ExecuteTask(const std::shared_ptr<Runnable>& );

    void StopAllThreads();

private:
    ThreadPool() : m_shutdown(false) { }

    typedef std::vector<Thread* > ThreadContainer;
    typedef ThreadContainer::iterator ThreadIterator;

    std::mutex          m_threadsLock;
    ThreadContainer     m_threads;
    volatile bool       m_shutdown;


};

#endif

