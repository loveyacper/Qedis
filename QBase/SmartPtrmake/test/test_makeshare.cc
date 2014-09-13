#include <cassert>
#include <iostream>
#include <iostream>
#include "./debug.h"
#include "../MakeShared.h" 
#include "./Timer.h"
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/smart_ptr/make_shared.hpp>

#if 0
void * operator new( std::size_t  size ) 
{ 
    std::cout << "new " << size << std::endl;
    return ::operator new(size); 
}
                                 
void operator delete( void * p )
{ 
    std::cout << "delete...\n";
    return ::operator delete(p); 
}
#endif

DECLARE_CLASS(Test)

class Bert
{
public:
    explicit Bert(const std::string& name) : m_age(21), m_name(name) 
    {
        std::cout << m_age << __PRETTY_FUNCTION__ << std::endl;
    }

    Bert(const Bert& other) : m_name(other.m_name)
    {
        std::cout << __PRETTY_FUNCTION__ << std::endl;
    }

    void Print() const
    {
        std::cout << m_name << std::endl;
    }
    ~Bert()
    {
        std::cout << __PRETTY_FUNCTION__ << std::endl;
    }
private:
    int m_age;
    std::string m_name;
};

const int N = 1000 * 10000;

int main()
{
    SharedPtr<Bert>  ptr(MakeShared<Bert>("bert young"));
    ptr->Print();
    return -1;

    Time  start;
    for (int i = 0; i < N; ++ i)
    {
        //Test* p = new Test(); delete  p;
        //boost::shared_ptr<Test>  ptr(boost::make_shared<Test>());
        SharedPtr<Test>  ptr(MakeShared<Test>());
    }
    Time  end;

    std::cout << "used " << (end - start) << std::endl;

    start.Now();
    for (int i = 0; i < N; ++ i)
    {
        //Test* p = new Test(); delete  p;
        //CounterBase* pb = new Counter<Test>(); delete pb; 
        SharedPtr<Test>  ptr(new Test());
        //boost::shared_ptr<Test>  ptr(new Test());
    }
    end.Now();

    std::cout << "used " << (end - start) << std::endl;
    return 0;
}

