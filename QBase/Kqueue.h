#ifndef BERT_KQUEUE_H
#define BERT_KQUEUE_H

#if defined(__APPLE__)

#include "Poller.h"
#include <vector>

class Kqueue : public Poller
{
public:
    Kqueue();
   ~Kqueue();

    bool AddSocket(int sock, int events, void* userPtr);
    bool ModSocket(int sock, int events, void* userPtr);
    bool DelSocket(int sock, int events);

    int Poll(std::vector<FiredEvent>& events, std::size_t maxEvent, int timeoutMs);

private:
    std::vector<struct kevent> m_events;    
};

#endif

#endif

