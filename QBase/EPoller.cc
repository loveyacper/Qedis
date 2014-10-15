
#if defined(__gnu_linux__)

#include "EPoller.h"
#include "Log/Logger.h"

#include <errno.h>
#include <unistd.h>

namespace Epoll
{
    bool ModSocket(int epfd, int socket, uint32_t events, void* ptr);

    bool AddSocket(int epfd, int socket, uint32_t events, void* ptr)
    {
        if (socket < 0)
            return false;

        USR << "add socket: fd " << socket << " with events " << events;

        epoll_event  ev;
        ev.data.ptr= ptr;

        ev.events  = 0;
        if (events & EventTypeRead)
            ev.events |= EPOLLIN;
        if (events & EventTypeWrite)
            ev.events |= EPOLLOUT;

        return 0 == epoll_ctl(epfd, EPOLL_CTL_ADD, socket, &ev);
    }

    bool DelSocket(int epfd, int socket)
    {
        if (socket < 0)
            return false;

        USR << "del events socket " << socket;

        epoll_event dummy;

        return 0 == epoll_ctl(epfd, EPOLL_CTL_DEL, socket, &dummy) ;
    }

    bool ModSocket(int epfd, int socket, uint32_t events, void* ptr)
    {
        if (socket < 0)
            return false;

        USR << "mod socket: fd " << socket << ", new events " << events;

        epoll_event  ev;
        ev.data.ptr= ptr;

        ev.events  = 0;
        if (events & EventTypeRead)
            ev.events |= EPOLLIN;
        if (events & EventTypeWrite)
            ev.events |= EPOLLOUT;

        return 0 == epoll_ctl(epfd, EPOLL_CTL_MOD, socket, &ev);
    }
}


Epoller::Epoller()
{
    m_multiplexer = ::epoll_create(512);
    INF << "create epoll:  " << m_multiplexer;
}

Epoller::~Epoller()
{
    INF << "close epoll:  " << m_multiplexer;
    if (m_multiplexer != -1)  
        ::close(m_multiplexer);
}

bool Epoller::AddSocket(int sock, int events, void* userPtr)
{
    if (Epoll::AddSocket(m_multiplexer, sock, events, userPtr))
        return  true;

    return (errno == EEXIST) && ModSocket(sock, events, userPtr);
}
    
bool Epoller::DelSocket(int sock, int events)
{
    return Epoll::DelSocket(m_multiplexer, sock);
}

   
bool Epoller::ModSocket(int sock, int events, void* userPtr)
{
    if (events == 0)
        return DelSocket(sock, 0);

    if (Epoll::ModSocket(m_multiplexer, sock, events, userPtr))
        return  true;

    return  errno == ENOENT && AddSocket(sock, events, userPtr);
}


int Epoller::Poll(std::vector<FiredEvent>& events, size_t  maxEvent, int timeoutMs)
{
    if (maxEvent == 0)
        return 0;

    while (m_events.size() < maxEvent)
        m_events.resize(2 * m_events.size() + 1);

    int nFired = TEMP_FAILURE_RETRY(::epoll_wait(m_multiplexer, &m_events[0], maxEvent, timeoutMs));
    if (nFired == -1 && errno != EINTR && errno != EWOULDBLOCK)
        return -1;

    if (nFired > 0 && static_cast<size_t>(nFired) > events.size())
        events.resize(nFired);

    for (int i = 0; i < nFired; ++ i)
    {
        FiredEvent& fired = events[i];
        fired.events   = 0;
        fired.userdata = m_events[i].data.ptr;

        if (m_events[i].events & EPOLLIN)
            fired.events  |= EventTypeRead;

        if (m_events[i].events & EPOLLOUT)
            fired.events  |= EventTypeWrite;

        if (m_events[i].events & (EPOLLERR | EPOLLHUP))
            fired.events  |= EventTypeError;
    }

    return nFired;
}

#endif

