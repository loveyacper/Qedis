#ifndef BERT_SHAREPTR_H
#define BERT_SHAREPTR_H

#include "./CounterImpl.h"

namespace {

// support SharedPtr<void>
template <typename T>
struct TypeTrait
{
    typedef T& reference;
};

template <>
struct TypeTrait<void>
{
    typedef void reference;
};

}

template <typename T>
class SharedPtr;

template <typename T>
class EnableShareMe;

template <typename T>
class WeakPtr;

inline void InitShareMe(...)
{
}

template <typename T, typename U> // assure U-->T is legal
inline void InitShareMe(EnableShareMe<T>* pMe, SharedPtr<U>* pShare)
{
    if (pMe)    pMe->AcceptOwner(pShare);
}

template <typename T>
class SharedPtr
{
    template <typename Y>
    friend class WeakPtr;

    template <typename U>
    friend class SharedPtr;

public:
    explicit SharedPtr(T* ptr = 0) : m_ptr(ptr)
    {
        m_cnt = new Counter<T>(ptr);
        InitShareMe(ptr, this);
    }

    ~SharedPtr()
    {
        m_cnt->DecShareCnt();
    }

    template<class U>
    explicit SharedPtr(U* ptr) : m_ptr(ptr)
    {
  //      if (ptr)  // must init
        {
            m_cnt = new Counter<U>(ptr);
            InitShareMe(ptr, this);
        }
    }

    template <typename U, typename D>
    SharedPtr(U* ptr, D del) : m_ptr(static_cast<T* >(ptr))
    {
  //      if (ptr)  // must init
        {
            m_cnt = new CounterD<U, D>(ptr, del);
            InitShareMe(ptr, this);
        }
    }

    // alias ctor
    template <typename U>
    SharedPtr(const SharedPtr<U>& other, T* ptr) : m_ptr(ptr)
    {
        other.m_cnt->AddShareCnt();
        m_cnt = other.m_cnt;
    }

    template <typename U>
    SharedPtr(const SharedPtr<U>& other) : m_cnt(other.m_cnt), m_ptr(static_cast<T* >(other.m_ptr))
    {
        m_cnt->AddShareCnt();
    }

    SharedPtr(const SharedPtr& other) : m_cnt(other.m_cnt), m_ptr(other.m_ptr)
    {
        m_cnt->AddShareCnt();
    }

    template <typename U>
    SharedPtr(const WeakPtr<U>& other)
    {
        if (other.m_cnt->AddShareCopy())
        {
            m_cnt = other.m_cnt;
            m_ptr = static_cast<T* >(other.m_ptr);
        }
        else
        {
            m_ptr = 0;
            m_cnt = new Counter<U>(m_ptr);
        }
    }
  
    SharedPtr& operator=(const SharedPtr& other)
    {
        if (this == &other)
            return *this;

        Reset();

        m_cnt = other.m_cnt;
        m_cnt->AddShareCnt();

        m_ptr = other.m_ptr;
        return *this;
    }

    template <typename U>
    SharedPtr& operator=(const SharedPtr<U>& other)
    {
        if (this == &other)
            return *this;

        Reset();

        m_cnt = other.m_cnt;
        m_cnt->AddShareCnt();

        m_ptr = static_cast<T* >(other.m_ptr);
        return *this;
    }

    void Reset()
    {
        SharedPtr().Swap(*this);
    }

    void Reset(T* ptr)
    {
        if (ptr == m_ptr)  return;

        SharedPtr(ptr).Swap(*this);
    }

    void Swap(SharedPtr& other)
    {
        if (this != &other)
        {
            std::swap(m_cnt, other.m_cnt);
            std::swap(m_ptr, other.m_ptr);
        }
    }

    typename TypeTrait<T>::reference operator*() const
    {
        return *m_ptr;
    }

    typedef T*  (SharedPtr<T>::* bool_type)() const;
    operator bool_type() const
    {
        return m_ptr == 0 ? 0 : &SharedPtr<T>::Get;
    }

    T* operator->() const
    {
        return m_ptr;
    }

    T* Get() const
    {
        return m_ptr;
    }

    int UseCount() const
    {
        return m_cnt->UseCount();
    }

    bool Unique() const
    {
        return UseCount() == 1;
    }

    void* GetDeleter() const {  return m_cnt->GetDeleter(); }

private:
    CounterBase* m_cnt;
    T*           m_ptr;
};

template <typename T>
inline bool operator==(SharedPtr<T> a, SharedPtr<T> b)
{
    return a.Get() == b.Get();
}

template <typename T>
inline bool operator<(SharedPtr<T> a, SharedPtr<T> b)
{
    return a.Get() < b.Get();
}

#endif

