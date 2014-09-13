
#include <cassert>
#include <iostream>
#include "./debug.h"
#include "../WeakPtr.h"

DECLARE_BASE_CLASS(Base)
DECLARE_SON_CLASS(Son, Base)

int main()
{
    SharedPtr<void>  p1(new Son);
    assert(p1 && p1.Unique());

    std::cerr << "Create weak begin :\n";
    {
        WeakPtr<void>  pWeak(p1);
        std::cerr << "weak count " << pWeak.WeakCount() << std::endl;
        std::cerr << "use count " << pWeak.UseCount() << std::endl;
        std::cerr << "expired ? " << pWeak.Expired() << std::endl;
        assert(pWeak.WeakCount() == 2);
        assert(pWeak.UseCount() == 1);
        assert(!pWeak.Expired());
    }

    std::cerr << "Destroy weak end:\n";
    std::cerr << "use count " << p1.UseCount() << std::endl;
    assert(p1.UseCount() == 1);

    std::cerr << "BYE\n";
}

