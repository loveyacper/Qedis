
#ifndef BERT_IPC_H
#define BERT_IPC_H

#if defined(__gnu_linux__) || defined(__APPLE__)
    #include <pthread.h>
    #include <semaphore.h>
    #include <atomic>

#else
    #include <Windows.h>

#endif

class Semaphore
{
#if defined(__gnu_linux__)
    sem_t  m_sem;
#elif defined(__APPLE__)
    char   m_name[32];
    sem_t* m_sem;
    static std::atomic<std::size_t> m_id;
#else
    HANDLE m_sem;
#endif

public:
    Semaphore(long lInit = 0, long lMaxForWindows = 65536);
    ~Semaphore();

    void Wait();
    void Post(long lReleaseCntForWin = 1);
    int  Value();

private:
    // Avoid copy
    Semaphore(const Semaphore& );

    // Avoid assignment
    Semaphore& operator= (const Semaphore& );
};

#endif

