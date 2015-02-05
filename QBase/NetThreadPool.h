#ifndef BERT_NETTHREADPOOL_H
#define BERT_NETTHREADPOOL_H

#include <deque>
#include <vector>
#include <unistd.h>
#include <memory>
#include <mutex>
#include "Poller.h"
#include "Threads/Thread.h"
#include "Threads/ThreadPool.h"

inline long GetCpuNum()
{
    return  sysconf(_SC_NPROCESSORS_ONLN);
}

class   Socket;
typedef std::shared_ptr<Socket> PSOCKET;

namespace Internal
{


class NetThread : public Runnable
{
public:
    NetThread();
   ~NetThread();

    bool IsAlive() const  {  return m_running; }
    void Stop()           {  m_running = false;}

    void AddSocket(PSOCKET , uint32_t event);
    void ModSocket(PSOCKET , uint32_t event);
    void RemoveSocket(std::deque<PSOCKET>::iterator & iter);

protected:
    std::unique_ptr<Poller>        m_poller;
    std::vector<FiredEvent > m_firedEvents;    
    std::deque<PSOCKET>      m_tasks;
    void  _TryAddNewTasks();

private:
    volatile bool m_running;

    std::mutex   m_mutex;
    typedef std::vector<std::pair<std::shared_ptr<Socket>, uint32_t> >    NewTasks; 
    NewTasks     m_newTasks; 
    volatile int m_newCnt;
    void _AddSocket(PSOCKET , uint32_t  event);
};

class RecvThread : public NetThread
{
public:
    void Run();
};

class SendThread : public NetThread
{
public:
    void Run();
};


///////////////////////////////////////////////
class NetThreadPool
{
    std::shared_ptr<RecvThread> m_recvThread;
    std::shared_ptr<SendThread> m_sendThread;

public:
    bool AddSocket(PSOCKET , uint32_t event);
    bool StartAllThreads();
    void StopAllThreads();
    
    void EnableRead(const std::shared_ptr<Socket>& sock);
    void EnableWrite(const std::shared_ptr<Socket>& sock);
    void DisableRead(const std::shared_ptr<Socket>& sock);
    void DisableWrite(const std::shared_ptr<Socket>& sock);

    static NetThreadPool& Instance()
    {
        static  NetThreadPool    pool;
        return  pool;
    }
};

}

#endif

