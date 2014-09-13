#include <cassert>
#include "SharedPtr.h"

namespace
{

template <typename T>
class  Deleter
{
public:
    Deleter( )
    {
    }

    Deleter(const Deleter& ) { }
    Deleter& operator= (const Deleter& ) { return *this; }

    T* Address() { return (T*)m_arena; }

    void operator() (T* dummy)
    {
        reinterpret_cast<T*>(m_arena)->~T();
    }

private:
    union
    {
        unsigned long align;
        char  m_arena[sizeof(T)]; // TODO ALIAN
    };
};

}

template <typename T>
SharedPtr<T>  MakeShared()
{
    SharedPtr<T>  ptr((T*)0, Deleter<T>() );

    Deleter<T>* deleter = (Deleter<T>*)ptr.GetDeleter();

    T*  addr = deleter->Address();
    new (addr)T();

    return SharedPtr<T>(ptr, addr);
}

template <typename T, typename ARG1>
SharedPtr<T>  MakeShared(const ARG1& arg1)
{
    SharedPtr<T>  ptr((T*)0, Deleter<T>() );

    Deleter<T>* deleter = (Deleter<T>*)ptr.GetDeleter();
    T*  addr = deleter->Address();

    new (addr)T(arg1);

    return SharedPtr<T>(ptr, addr);
}

template <typename T, typename ARG1, typename ARG2>
SharedPtr<T>  MakeShared(const ARG1& arg1, const ARG2& arg2)
{
    SharedPtr<T>  ptr((T*)0, Deleter<T>() );

    Deleter<T>* deleter = (Deleter<T>*)ptr.GetDeleter();
    T*  addr = deleter->Address();

    new (addr)T(arg1, arg2);

    return SharedPtr<T>(ptr, addr);
}

template <typename T, typename ARG1, typename ARG2, typename ARG3>
SharedPtr<T>  MakeShared(const ARG1& arg1, const ARG2& arg2, const ARG3& arg3)
{
    SharedPtr<T>  ptr((T*)0, Deleter<T>() );

    Deleter<T>* deleter = (Deleter<T>*)ptr.GetDeleter();
    T*  addr = deleter->Address();

    new (addr)T(arg1, arg2, arg3);

    return SharedPtr<T>(ptr, addr);
}

