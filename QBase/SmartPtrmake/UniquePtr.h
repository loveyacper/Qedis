#ifndef BERT_UNIQUEPTR_H
#define BERT_UNIQUEPTR_H


template <typename T>
class DefaultDeleter
{
public:
    void operator() (T* p) const
    {
        delete p;
    }
};

template <typename T>
class ArrayDeleter
{
public:
    void operator() (T* p) const
    {
        delete [] p;
    }
};

template <typename T, typename D = DefaultDeleter<T> >
class UniquePtr
{
public:
    explicit UniquePtr(T* ptr = 0) : m_ptr(ptr) {}

    ~UniquePtr()
    {
        if (m_ptr)
            D()(m_ptr);
    }

    T* Get() const
    {
        return m_ptr;
    }

    operator T*() const
    {
        return m_ptr;
    }

    T* operator->() const
    {
        return m_ptr;
    }

    T& operator*() const
    {
        return  *m_ptr;
    }

    T* Release() 
    {
        T* ptr = m_ptr;
        m_ptr  = 0;
        return  ptr;
    }

    void Reset(T* ptr = 0) 
    {
        D()(m_ptr);
        m_ptr = ptr;
    }

    void swap(UniquePtr& other)
    {
        T* tmp = m_ptr;
        m_ptr  = other.m_ptr;
        other.m_ptr = tmp;
    }

private:
    T*        m_ptr;

    UniquePtr(const UniquePtr& );
    UniquePtr& operator=(const UniquePtr& );
};


template <typename T>
inline void swap(UniquePtr<T>& a, UniquePtr<T>& b)
{
    a.swap(b);
}

#endif
