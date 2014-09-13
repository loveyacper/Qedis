#ifndef BERT_SHAREPTR_H
#define BERT_SHAREPTR_H

template <typename T>
class SharedPtr
{
public:
    explicit SharedPtr(T* ptr = 0) : m_ptr(ptr)
    {
        m_cnt = new int(1);
    }

    ~SharedPtr()
    {
        if (-- *m_cnt == 0)
        {
            delete  m_cnt;
            delete  m_ptr;
        }
    }

    SharedPtr(const SharedPtr& other) : m_cnt(other.m_cnt), m_ptr(other.m_ptr)
    {
        ++ *m_cnt;
    }
  
    SharedPtr& operator=(const SharedPtr& other)
    {
        if (this == &other)
            return *this;

        Reset();

        m_cnt = other.m_cnt;
        m_ptr = other.m_ptr;

        ++ *m_cnt;

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

    T& operator*() const
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
        return *m_cnt;
    }

    bool Unique() const
    {
        return UseCount() == 1;
    }

private:
    int* m_cnt;
    T*   m_ptr;
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

