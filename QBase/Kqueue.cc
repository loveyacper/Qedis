#if defined(__APPLE__)

#include "Kqueue.h"
#include "Log/Logger.h"

#include <sys/event.h>
#include <errno.h>
#include <unistd.h>


Kqueue::Kqueue()
{
    m_multiplexer = ::kqueue();
    INF << "create kqueue:  " << m_multiplexer;
}

Kqueue::~Kqueue()
{
    INF << "close kqueue: " << m_multiplexer;
    if (m_multiplexer != -1)  
        ::close(m_multiplexer);
}

bool Kqueue::AddSocket(int sock, int events, void* userPtr)
{
     struct kevent change[2];
         
     int  cnt = 0;
     
     if (events & EventTypeRead)
     {
         EV_SET(change + cnt, sock, EVFILT_READ, EV_ADD, 0, 0, userPtr);
         ++ cnt;
     }
                 
     if (events & EventTypeWrite)
     {
         EV_SET(change + cnt, sock, EVFILT_WRITE, EV_ADD, 0, 0, userPtr);
         ++ cnt;
     }
                     
     return kevent(m_multiplexer, change, cnt, NULL, 0, NULL) != -1;
}
    
bool Kqueue::DelSocket(int sock, int events)
{
    INF << "Delete socket " << sock << " with events " << events;

    struct kevent change[2];
    int cnt = 0;

    if (events & EventTypeRead)
    {
        EV_SET(change + cnt, sock, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        ++ cnt;
    }

    if (events & EventTypeWrite)
    {
        EV_SET(change + cnt, sock, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
        ++ cnt;
    }
                    
    if (cnt == 0)
        return false;
                        
    return -1 != kevent(m_multiplexer, change, cnt, NULL, 0, NULL);
}

   
bool Kqueue::ModSocket(int sock, int events, void* userPtr)
{
    bool ret = DelSocket(sock, EventTypeRead | EventTypeWrite);
    if (events == 0)
        return ret;

    return AddSocket(sock, events, userPtr);
}


int Kqueue::Poll(std::vector<FiredEvent>& events, int maxEvent, int timeoutMs)
{
    if (maxEvent <= 0)
        return 0;

    while (m_events.size() < maxEvent)
        m_events.resize(2 * m_events.size() + 1);

    struct timespec* pTimeout = NULL;  
    struct timespec  timeout;
    if (timeoutMs >= 0)
    {
        pTimeout = &timeout;
        timeout.tv_sec  = timeoutMs / 1000;
        timeout.tv_nsec = timeoutMs % 1000 * 1000000;
    }

    int nFired = ::kevent(m_multiplexer, NULL, 0, &m_events[0], maxEvent, pTimeout);
    if (nFired == -1)
        return -1;

    if (nFired > 0 && nFired > events.size())
        events.resize(nFired);

    for (int i = 0; i < nFired; ++ i)
    {
        FiredEvent& fired = events[i];
        fired.events   = 0;
        fired.userdata = m_events[i].udata;

        if (m_events[i].filter == EVFILT_READ)
            fired.events  |= EventTypeRead;

        if (m_events[i].filter == EVFILT_WRITE)
            fired.events  |= EventTypeWrite;

        if (m_events[i].flags & EV_ERROR)
            fired.events  |= EventTypeError;
    }

    return nFired;
}

#endif

