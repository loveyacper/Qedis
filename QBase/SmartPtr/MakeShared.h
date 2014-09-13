#include <cassert>
#include "SharedPtr.h"

namespace
{

template <typename T>
class  Deleter
{
public:
    Deleter( ) : m_init(false)
    {
    }

    Deleter(const Deleter& ) { }
    Deleter& operator= (const Deleter& ) { return *this; }

    T* Address() { return (T*)m_arena; }

    void operator() (T* dummy)
    {
        if (m_init)
        {
            m_init = false;
            reinterpret_cast<T*>(m_arena)->~T();
        }
    }

    void SetInit()     { m_init = true; }

private:
    union
    {
        unsigned long align;
        char  m_arena[sizeof(T)]; // TODO ALIAN
    };

    bool  m_init;
};

}

template <typename T>
SharedPtr<T>  MakeShared()
{
    SharedPtr<T>  ptr((T*)0, Deleter<T>() );

    Deleter<T>* deleter = (Deleter<T>*)ptr.GetDeleter();

    T*  addr = deleter->Address();
    new (addr)T();
    deleter->SetInit();

    InitShareMe(addr, &ptr);
    return SharedPtr<T>(ptr, addr);
}

template <typename T, typename ARG1>
SharedPtr<T>  MakeShared(const ARG1& arg1)
{
    SharedPtr<T>  ptr((T*)0, Deleter<T>() );

    Deleter<T>* deleter = (Deleter<T>*)ptr.GetDeleter();

    T*  addr = deleter->Address();
    new (addr)T(arg1);
    deleter->SetInit();

    InitShareMe(addr, &ptr);
    return SharedPtr<T>(ptr, addr);
}

template <typename T, typename ARG1, typename ARG2>
SharedPtr<T>  MakeShared(const ARG1& arg1, const ARG2& arg2)
{
    SharedPtr<T>  ptr((T*)0, Deleter<T>() );

    Deleter<T>* deleter = (Deleter<T>* )ptr.GetDeleter();

    T*  addr = deleter->Address();
    new (addr)T(arg1, arg2);
    deleter->SetInit();

    InitShareMe(addr, &ptr);
    return SharedPtr<T>(ptr, addr);
}

template <typename T, typename ARG1, typename ARG2, typename ARG3>
SharedPtr<T>  MakeShared(const ARG1& arg1, const ARG2& arg2, const ARG3& arg3)
{
    SharedPtr<T>  ptr((T*)0, Deleter<T>() );

    Deleter<T>* deleter = (Deleter<T>*)ptr.GetDeleter();
    T*  addr = deleter->Address();

    new (addr)T(arg1, arg2, arg3);
    deleter->SetInit();

    InitShareMe(addr, &ptr);
    return SharedPtr<T>(ptr, addr);
}

