
#include <cassert>
#include <iostream>
#include "./debug.h"
#include "../SharedPtr.h"

DECLARE_CLASS(Base)
DECLARE_CLASS(Test)

class NoDefinition;

int main()
{
    {
    SharedPtr<NoDefinition>  pppp; // No need definition if you don't use the ptr
    }

    SharedPtr<void>  pv1(new Test());
    SharedPtr<void>  pv2(new Base());

    pv2 = pv1; // destruct Base
    pv1 = pv2; // no effect

    return 0;
}

