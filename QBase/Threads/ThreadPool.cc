#include "ThreadPool.h"
#include <iostream>

using namespace std;

__thread bool ThreadPool::working_ = true;

ThreadPool::ThreadPool() : waiters_(0), shutdown_(false)
{
    maxIdleThread_ = std::max(1U, std::thread::hardware_concurrency());
    monitor_ = std::thread([this]() { this->_MonitorRoutine(); } );
}

ThreadPool::~ThreadPool()
{
    JoinAll();
}

ThreadPool& ThreadPool::Instance()
{
    static ThreadPool  pool;
    return pool;
}

void    ThreadPool::SetMaxIdleThread(unsigned int m)
{
    if (0 < m && m <= kMaxThreads)
        maxIdleThread_ = m;
}

void    ThreadPool::JoinAll()
{
    decltype(workers_)  tmp;
    
    {
        std::unique_lock<std::mutex>  guard(mutex_);
        if (shutdown_)
            return;
        
        cerr << std::this_thread::get_id() << " shutdown threadpool\n";
        shutdown_ = true;
        cond_.notify_all();
        
        tmp.swap(workers_);
        workers_.clear();
    }
    
    for (auto& t : tmp)
    {
        if (t.joinable())
        {
            cerr << "join thread " << t.get_id() << endl;
            t.join();
        }
        else
            cerr << "not joinable " << t.get_id() << endl;
    }
    
    if (monitor_.joinable())
        monitor_.join();
}

void   ThreadPool::_CreateWorker()
{
    std::thread  t([this]() { this->_WorkerRoutine(); } );
    workers_.push_back(std::move(t));
}

void   ThreadPool::_WorkerRoutine()
{
    working_ = true;
    
    cerr << "worker start " << std::this_thread::get_id() << endl;
    while (working_)
    {
        std::function<void ()>   task;
        
        {
            std::unique_lock<std::mutex>    guard(mutex_);
            
            ++ waiters_;
            cerr << waiters_ << ": incr waiter " << std::this_thread::get_id() << endl;
            cond_.wait(guard, [this]()->bool { return this->shutdown_ || !tasks_.empty(); } );
            -- waiters_;
            cerr << waiters_ << ": dec waiter " << std::this_thread::get_id() << endl;
            
            if (this->shutdown_ && tasks_.empty())
            {
                cerr << "exit because shutdown " << std::this_thread::get_id() << endl;
                return;
            }
            
            task = std::move(tasks_.front());
            tasks_.pop_front();
        }
        
        task();
    }
    
    cerr << "exit because not working " << std::this_thread::get_id() << endl;
}

void   ThreadPool::_MonitorRoutine()
{
    while (!shutdown_)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        std::unique_lock<std::mutex>   guard(mutex_);
        if (shutdown_)
            return;
        
        auto nw = waiters_;
        while (nw -- > maxIdleThread_)
        {
            cerr << "push exit item " << std::this_thread::get_id() << endl;
            tasks_.push_back([this]() { working_ = false; });
            cond_.notify_one();
        }
    }
}
