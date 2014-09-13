
#include <cassert>
#include <iostream>
#include "./debug.h"
#include "../EnableShareMe.h"

class Base : public EnableShareMe<Base>
{
public:
    int  a;
    Base()
    {
        std::cerr << __FUNCTION__ << "\n";
        a = 111;
    }
   virtual ~Base()
    {
        std::cerr << __FUNCTION__<<"\n";
    }
};

DECLARE_SON_CLASS(Son, Base)

int main()
{
    SharedPtr<Base>  p1(new Son);

    SharedPtr<Son>  p2(p1->ShareMe());

    SharedPtr<int>  p3(p1, &p1->a);
    std::cerr << *p3 << std::endl;

    assert(!p1.Unique());
    assert(p1.UseCount() == 3);

    std::cerr << "BYE BYE\n";
}

