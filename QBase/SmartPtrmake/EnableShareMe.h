#ifndef BERT_ENABLESHAREME_H
#define BERT_ENABLESHAREME_H

#include "WeakPtr.h"

template <typename T>
class EnableShareMe
{
protected:
    EnableShareMe() {}
    EnableShareMe(const EnableShareMe& ) { }

public:
    EnableShareMe& operator= (const EnableShareMe& ) { return *this; }

    template <typename U>
    void AcceptOwner(SharedPtr<U>* sptr)
    {
        if (m_weak.Expired() && sptr)
        {
            m_weak.Reset(*sptr);
        }
    }

    SharedPtr<T>     ShareMe()
    {
        return SharedPtr<T>(m_weak);
    }

private:
    WeakPtr<T>  m_weak;
};

#endif

