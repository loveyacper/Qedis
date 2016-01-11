#if defined(__APPLE__)

#include "Kqueue.h"
#include "Log/Logger.h"

#include <sys/event.h>
#include <errno.h>
#include <unistd.h>


Kqueue::Kqueue()
{
    multiplexer_ = ::kqueue();
    WITH_LOG(INF << "create kqueue:  " << multiplexer_);
}

Kqueue::~Kqueue()
{
    WITH_LOG(INF << "close kqueue: " << multiplexer_);
    if (multiplexer_ != -1)  
        ::close(multiplexer_);
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
                     
     return kevent(multiplexer_, change, cnt, NULL, 0, NULL) != -1;
}
    
bool Kqueue::DelSocket(int sock, int events)
{
    WITH_LOG(INF << "Delete socket " << sock << " with events " << events);

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
                        
    return -1 != kevent(multiplexer_, change, cnt, NULL, 0, NULL);
}

   
bool Kqueue::ModSocket(int sock, int events, void* userPtr)
{
    bool ret = DelSocket(sock, EventTypeRead | EventTypeWrite);
    if (events == 0)
        return ret;

    return AddSocket(sock, events, userPtr);
}


int Kqueue::Poll(std::vector<FiredEvent>& events, std::size_t maxEvent, int timeoutMs)
{
    if (maxEvent == 0)
        return 0;

    while (events_.size() < maxEvent)
        events_.resize(2 * events_.size() + 1);

    struct timespec* pTimeout = NULL;  
    struct timespec  timeout;
    if (timeoutMs >= 0)
    {
        pTimeout = &timeout;
        timeout.tv_sec  = timeoutMs / 1000;
        timeout.tv_nsec = timeoutMs % 1000 * 1000000;
    }

    int nFired = ::kevent(multiplexer_, NULL, 0, &events_[0], static_cast<int>(maxEvent), pTimeout);
    if (nFired == -1)
        return -1;

    if (nFired > 0 && static_cast<size_t>(nFired) > events.size())
        events.resize(nFired);

    for (int i = 0; i < nFired; ++ i)
    {
        FiredEvent& fired = events[i];
        fired.events   = 0;
        fired.userdata = events_[i].udata;

        if (events_[i].filter == EVFILT_READ)
            fired.events  |= EventTypeRead;

        if (events_[i].filter == EVFILT_WRITE)
            fired.events  |= EventTypeWrite;

        if (events_[i].flags & EV_ERROR)
            fired.events  |= EventTypeError;
    }

    return nFired;
}

#endif

