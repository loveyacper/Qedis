#ifndef BERT_EPOLLER_H
#define BERT_EPOLLER_H

#if defined(__gnu_linux__)

#include <sys/epoll.h>
#include <vector>
#include "Poller.h"

class Epoller : public Poller
{
public:
    Epoller();
   ~Epoller();

    bool AddSocket(int sock, int events, void* userPtr);
    bool ModSocket(int sock, int events, void* userPtr);
    bool DelSocket(int sock, int events);

    int Poll(std::vector<FiredEvent>& events, std::size_t maxEvent, int timeoutMs);

private:
    std::vector<epoll_event> events_;    
};

#endif

#endif

