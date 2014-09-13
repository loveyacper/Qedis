
#include <cassert>
#include <iostream>
#include "./debug.h"
#include "../EnableShareMe.h"

class Base : public EnableShareMe<Base>
{
public:
    Base()
    {
        std::cerr << __FUNCTION__ << "\n";
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
    assert(!p1.Unique());
    assert(p1.UseCount() == 2);

    std::cerr << "BYE BYE\n";
}

