#ifndef BERT_WEAKPTR_H
#define BERT_WEAKPTR_H

#include "./SharedPtr.h"

template <typename T>
class WeakPtr 
{
    template <typename Y>
    friend class SharedPtr;
public:
    WeakPtr() : m_cnt(0), m_ptr(0)
    {
    }

    ~WeakPtr()
    {
        if (m_cnt)
            m_cnt->DecWeakCnt();
    }

    template <typename U>
    WeakPtr(const SharedPtr<U>& sptr) : m_cnt(sptr.m_cnt), m_ptr(static_cast<T* >(sptr.m_ptr))
    {
        if (m_cnt)      m_cnt->AddWeakCnt();
    }

    WeakPtr(const WeakPtr& other) : m_cnt(other.m_cnt), m_ptr(other.m_ptr)
    {
        if (m_cnt)      m_cnt->AddWeakCnt();
    }
  
    WeakPtr& operator=(const WeakPtr& other)
    {
        if (this == &other)
            return *this;

        Reset();

        m_ptr = other.m_ptr;
        m_cnt = other.m_cnt;

        if (m_cnt)      m_cnt->AddWeakCnt();

        return *this;
    }

    void Reset()
    {
        WeakPtr().Swap(*this);
    }

    template <typename U>
    void Reset(SharedPtr<U>& sptr)
    {
        if (sptr.m_ptr == m_ptr)  return;

        WeakPtr(sptr).Swap(*this);
    }

    void Swap(WeakPtr& other)
    {
        if (this != &other)
        {
            std::swap(m_cnt, other.m_cnt);
            std::swap(m_ptr, other.m_ptr);
        }
    }

    int UseCount() const
    {
        if (m_cnt)  return m_cnt->UseCount();

        return 0;
    }

    int WeakCount() const
    {
        if (m_cnt)  return m_cnt->WeakCount();

        return 0;
    }

    bool Expired() const
    {
        return 0 == UseCount();
    }

    SharedPtr<T> Lock()
    {
        return SharedPtr<T>(*this);
    }

private:
    CounterBase* m_cnt;
    T*           m_ptr;
};

#endif

