#ifndef BERT_THREAD_H
#define BERT_THREAD_H

#include  <string>
#include  "Atomic.h"
#include  "IPC.h"
#include  "../SmartPtr/SharedPtr.h"

#if defined(__gnu_linux__) || defined(__APPLE__)
    #include <pthread.h>
    typedef pthread_t   THREAD_HANDLE;
    typedef pthread_t   THREAD_ID;
    #define INVALID_HANDLE_VALUE   (pthread_t)(-1)
#else
    #include  <WinBase.h>
    #include  <process.h>
    typedef HANDLE   THREAD_HANDLE;
    typedef DWORD    THREAD_ID;
#endif

class Runnable
{
public:
    virtual  ~Runnable()  { }
    virtual void  Run() = 0;
};

class Thread
{
    SharedPtr<Runnable> m_runnable;

    THREAD_HANDLE m_handle;
    THREAD_ID     m_tid;

    // Control suspend and resume
    Semaphore     m_sem;

    // Has called pthread_create?
    volatile bool m_working;
    volatile int m_suspendCount; // sorry, we can't get the number of suspend threads that waiting for a posix semphore.

    typedef void * (*PTHREADFUNC)(void* ); 
    static  void * ThreadFunc(void* );

public:
    explicit Thread(const SharedPtr<Runnable>& = SharedPtr<Runnable>());
    ~Thread();

    void   SetTask(const SharedPtr<Runnable>& r)  { m_runnable = r; }

    THREAD_ID     GetThreadID()     { return m_tid; }
    THREAD_HANDLE GetThreadHandle() { return m_handle; }
    
    static void   Sleep(unsigned int seconds);
    static void   MSleep(unsigned int mSeconds);
    
    bool   Start();
    bool   Idle() const  { return m_suspendCount > 0; }

    void   Stop();
    void   Join();
    
    void   Suspend();
    void   Resume();

    static THREAD_ID   GetCurrentThreadId();
    static void        YieldCPU();

private:
    void   _Run();
    static bool  _LaunchThread(THREAD_HANDLE& handle, PTHREADFUNC func, void* arg);

private:
    Thread(const Thread& );
    Thread& operator= (const Thread& );
};

#endif

