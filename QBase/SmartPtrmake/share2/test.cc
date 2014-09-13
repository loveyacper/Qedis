
#include <cassert>
#include <iostream>
#include "./SharedPtr.h"

class Test
{
public:
    Test()
    {
        a = 123456789;
        std::cerr << __FUNCTION__ << "\n";
    }

   ~Test()
    {
        a = 0;
        std::cerr << __FUNCTION__<<"\n";
    }

private:
    int  a;
};


int main()
{
    SharedPtr<Test>  p1(new Test);
    assert(p1.Unique());
    assert(p1);

    SharedPtr<Test>  p0((Test*)0);
    p0 = p1;
    p0.Reset(p1.Get());

    return 0;
}

