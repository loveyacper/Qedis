
#include <cassert>
#include <iostream>
#include "../WeakPtr.h"
#include "../EnableShareMe.h"

class Base : public EnableShareMe<Base>
{
public:
    int a;
    Base()
    {
        a = 0x12345678;
        std::cerr << __FUNCTION__ << "\n";
    }
   virtual ~Base()
    {
        a = 0;
        std::cerr << __FUNCTION__<<"\n";
    }
};

class Son : public Base 
{
public:
    Son()
    {
        std::cerr << __FUNCTION__ << "\n";
    }
   virtual ~Son()
    {
        std::cerr << __FUNCTION__<<"\n";
    }
};

class Test
{
public:
    int  a;
    Test()
    {
        a = 0xbadfcbad;
        std::cerr << __FUNCTION__ << "\n";
    }

   ~Test()
    {
        a = 0;
        std::cerr << __FUNCTION__<<"\n";
    }
};



int main()
{
    SharedPtr<Son>  p1(new Son);
    assert(p1.Unique());
    assert(p1);

    SharedPtr<Son>  p2(p1->ShareMe());
    assert(!p1.Unique());
    assert(!p2.Unique());
    assert(p1.UseCount() == 2);

    std::cerr << "0=======================================\n";
    {
        WeakPtr<Base>  pWeak(p1);
        assert(pWeak.WeakCount() == 3);
        assert(pWeak.UseCount() == 2);
        assert(!pWeak.Expired());
    }

    std::cerr << "1=======================================\n";
    SharedPtr<int>  pint(p1, &p1->a);
    assert(pint.UseCount() == 3);

    SharedPtr<void>  pvoid(pint);
    assert(pvoid.UseCount() == 4);
    SharedPtr<char>  pchar(pvoid);

    std::cerr << "2======================================= sizeof pchar \n" << sizeof(pchar);
    if (pint != NULL)
    {
        std::cerr << "output  pint " << *pint << std::endl;
        std::cerr << "output  pchar " << (int)(*pchar) << std::endl;
    }
    else
        std::cerr << "empty pint " << std::endl;
    std::cerr << "BYE BYE\n";
}

