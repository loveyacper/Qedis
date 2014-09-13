#ifndef BERT_SHAREPTR_H
#define BERT_SHAREPTR_H

#include "./CounterImpl.h"
#include <algorithm>

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

struct static_cast_tag
{
};

struct const_cast_tag
{
};

struct dynamic_cast_tag
{
};

template <typename T>
class SharedPtr
{
    template <typename Y>
    friend class WeakPtr;

    template <typename U>
    friend class SharedPtr;

public:
    // for cast
    template <typename U>
    explicit SharedPtr(const SharedPtr<U>& ptr, static_cast_tag ) : m_cnt(ptr.m_cnt), m_ptr(static_cast<T* >(ptr.m_ptr))
    {
        if (m_cnt)  m_cnt->AddShareCnt();
    }

    template <typename U>
    explicit SharedPtr(const SharedPtr<U>& ptr, const_cast_tag) : m_cnt(ptr.m_cnt), m_ptr(const_cast<T* >(ptr.m_ptr))
    {
        if (m_cnt)  m_cnt->AddShareCnt();
    }


    SharedPtr() : m_cnt(0), m_ptr(0)
    {
    }

    explicit SharedPtr(T* ptr) : m_ptr(ptr)
    {
        m_cnt = new Counter<T>(ptr);
        InitShareMe(ptr, this);
    }

    ~SharedPtr()
    {
        if (m_cnt) m_cnt->DecShareCnt();
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
        m_cnt = other.m_cnt;
        if (m_cnt) m_cnt->AddShareCnt();
            
        // !! because ptr maybe shareme, the shareme information must be in the T object
    //    InitShareMe(ptr, this);
    }

    template <typename U>
    SharedPtr(const SharedPtr<U>& other) : m_cnt(other.m_cnt), m_ptr(static_cast<T* >(other.m_ptr))
    {
        if (m_cnt) m_cnt->AddShareCnt();
    }

    SharedPtr(const SharedPtr& other) : m_cnt(other.m_cnt), m_ptr(other.m_ptr)
    {
        if (m_cnt) m_cnt->AddShareCnt();
    }

    template <typename U>
    SharedPtr(const WeakPtr<U>& other)
    {
        CounterBase* tmp = other.m_cnt;
        if (tmp && tmp->AddShareCopy())
        {
            m_cnt = tmp;
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
        m_ptr = other.m_ptr;

        if (m_cnt) m_cnt->AddShareCnt();

        return *this;
    }

    template <typename U>
    SharedPtr& operator=(const SharedPtr<U>& other)
    {
        if (this == &other)
            return *this;

        Reset();

        m_cnt = other.m_cnt;
        m_ptr = static_cast<T* >(other.m_ptr);

        if (m_cnt) m_cnt->AddShareCnt();

        return *this;
    }

    void Reset()
    {
        SharedPtr().Swap(*this);
    }

    template <typename U> // bug: if not U, delete void*
    void Reset(U* ptr)
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
        return m_cnt ? m_cnt->UseCount() : 0;
    }

    bool Unique() const
    {
        return UseCount() == 1;
    }

    // ugly, but for c pointer...
    void SetIntValue(intptr_t  value) 
    { 
        Reset();
        m_ptr = reinterpret_cast<T*>(value);
    }

    intptr_t GetIntValue() const
    {
        return (intptr_t)m_ptr;
    }

private:
    CounterBase* m_cnt;
    T*           m_ptr;
};

template <typename T>
inline bool operator==(const SharedPtr<T>& a, const SharedPtr<T>& b)
{
    return a.Get() == b.Get();
}

template <typename T>
inline bool operator<(const SharedPtr<T>& a, const SharedPtr<T>& b)
{
    return a.Get() < b.Get();
}

template <typename U, typename T>
SharedPtr<U>  StaticPointerCast(const SharedPtr<T>&  ptr)
{
    return SharedPtr<U>(ptr, static_cast_tag());
}

template <typename U, typename T>
SharedPtr<U>  ConstPointerCast(const SharedPtr<T>&  ptr)
{
    return SharedPtr<U>(ptr, const_cast_tag());
}


#endif

