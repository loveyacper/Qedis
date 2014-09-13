#ifndef BERT_COUNTERBASE_H
#define BERT_COUNTERBASE_H

#include "../Threads/Atomic.h"


class CounterBase
{
public:
    CounterBase() : m_shareCnt(1), m_weakCnt(1)
    {
    }

    virtual ~CounterBase()
    {
    }

    virtual void Dispose() = 0;

    void AddShareCnt()
    {
        AtomicChange(&m_shareCnt, 1);
    }

    void DecShareCnt()
    {
        if (1 == AtomicChange(&m_shareCnt, -1))
        {
            Dispose();
            DecWeakCnt();
        }
    }

    void AddWeakCnt()
    {
        AtomicChange(&m_weakCnt, 1);
    }

    void DecWeakCnt()
    {
        if (1 == AtomicChange(&m_weakCnt, -1))
            Destroy();
    }

    bool  AddShareCopy()
    {
        // When you constructing shared_ptr from weak_ptr,
        // The weak_ptr may be expired(imagine the last shared_ptr out of scope)
        // so I use a infinite loop, in case the weak ptr is invalid(then return false);
        while (true)
        {
            const int oldCnt = m_shareCnt;
            if (oldCnt == 0)
                return false;
        
            if (oldCnt == AtomicTestAndSet(&m_shareCnt, oldCnt, oldCnt + 1))
                return  true;
        }

        return false;
    }

    void Destroy()
    {        
        delete this;
    }

    int UseCount() const 
    {
        return m_shareCnt;
    }

    int WeakCount() const 
    {
        return m_weakCnt;
    }

private:
    volatile int m_shareCnt;
    volatile int m_weakCnt;

    CounterBase(const CounterBase& );
    CounterBase& operator= (const CounterBase& );
};

#endif

