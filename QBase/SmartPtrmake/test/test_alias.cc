
#include <cassert>
#include <iostream>
#include "../WeakPtr.h"


class Test
{
public:
    int  val;

    Test()
    {
        val = 123;
        std::cerr << __FUNCTION__ << std::endl;
    }

    ~Test()
    {
        val = 0;
        std::cerr << __FUNCTION__ << std::endl;
    }
};

int main()
{
    SharedPtr<Test> pt (new Test());

    SharedPtr<int>  pint(pt, &pt->val);

    assert(pint.UseCount() == 2);
    assert (pint != NULL);

    std::cerr << "output  pint " << *pint << std::endl;

    std::cerr << "BYE BYE\n";
}

