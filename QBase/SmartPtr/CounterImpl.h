#ifndef BERT_COUNTERIMPL_H
#define BERT_COUNTERIMPL_H

#include "./CounterBase.h"

template <typename T>
class Counter : public CounterBase
{
public:
    explicit Counter(T* ptr = 0) : m_ptr(ptr)
    {
    }

    virtual void Dispose()
    {
        delete m_ptr;
    }

private:
    T*  m_ptr;
};

template <typename T, typename D>
class CounterD : public CounterBase
{
public:
    explicit CounterD(T* ptr, D d) : m_ptr(ptr), m_deleter(d)
    {
    }

    virtual void Dispose()
    {
        m_deleter(m_ptr);
    }

private:
    T*  m_ptr;
    D   m_deleter;
};

#endif

