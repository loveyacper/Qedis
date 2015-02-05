
#if !defined(__gnu_linux__)
#define TEMP_FAILURE_RETRY(x)  x
#endif

#if defined(__APPLE__)
#define  PTHREAD_MUTEX_RECURSIVE_NP PTHREAD_MUTEX_RECURSIVE
#include <cstdio>
#endif

#if defined(__gnu_linux__) || defined(__APPLE__)
#include <errno.h> // TEMP_FAILURE_RETRY
#include <unistd.h> // TEMP_FAILURE_RETRY
#include <cassert>

#endif

#include "IPC.h"


#if defined(__APPLE__)
std::atomic<std::size_t> Semaphore::m_id;
#endif

Semaphore::Semaphore(long lInit, long lMaxForWindows)
{
#if defined(__gnu_linux__)
    ::sem_init(&m_sem, PTHREAD_PROCESS_PRIVATE, lInit);
#elif defined(__APPLE__)

    ++ m_id;
    snprintf(m_name, sizeof(m_name), "defaultSem%d", m_id.load());

    m_sem = ::sem_open(m_name, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, lInit);
    sem_unlink(m_name);

    assert(m_sem != SEM_FAILED);
#else
    m_sem = ::CreateSemaphore(0, lInit, lMaxForWindows, 0);
#endif
}

Semaphore::~Semaphore()
{
#if defined(__gnu_linux__)
    ::sem_destroy(&m_sem);
#elif defined(__APPLE__)
    ::sem_close(m_sem);
#else
    ::CloseHandle(m_sem);
#endif
}

void Semaphore::Wait()
{
#if defined(__gnu_linux__)
    TEMP_FAILURE_RETRY(::sem_wait(&m_sem));
#elif defined(__APPLE__)
    TEMP_FAILURE_RETRY(::sem_wait(m_sem));
#else
    ::WaitForSingleObject(m_sem, INFINITE);
#endif
}

void Semaphore::Post(long lReleaseCntForWin)
{
#if defined(__gnu_linux__)
    ::sem_post(&m_sem);
#elif defined(__APPLE__)
    ::sem_post(m_sem);
#else
    ::ReleaseSemaphore(m_sem, lReleaseCntForWin, 0);
#endif
}

int  Semaphore::Value()
{
    int  val = 0;
#if defined(__gnu_linux__)
    ::sem_getvalue(&m_sem, &val);
#elif defined(__APPLE__)
    ::sem_getvalue(m_sem, &val);
#elif defined(__WIN32__)
#endif

    return val;
}

