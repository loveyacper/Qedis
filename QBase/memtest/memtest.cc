/*===============================================================
*   
*    File Name   : memtest.cc
*    Author      : Bert Young
*    Date        : 2014.04.06
*    Description : 
*    Tencent.co
================================================================*/
#include "Timer.h"
#include "MemoryPool.h"
#include <vector>
#include <iostream>

static std::vector<char* >  g_ptr;

static const int NUM = 50 * 10000;

static bool g_usePool = false;

int main(int ac, char* av[])
{
    if (ac > 1)
    {
        g_usePool = (av[1][0] != '0');
    }

    g_ptr.reserve(NUM);

    for (int size = 32; size <= 2048; size *= 2)
    {
        g_ptr.clear();

        Time  start;
        for (int i = 0; i < NUM; ++ i)
        {
            if (g_usePool)
                g_ptr.push_back((char*)MemoryPool::allocate(size));
            else
                g_ptr.push_back(new char[size]);

        }

        for (int i = 0; i < NUM; ++ i)
        {
            if (g_usePool)
                MemoryPool::deallocate(g_ptr[i], size);
                //MemoryPool::deallocate(reinterpret_cast<const void*>(g_ptr[i]), size);
            else
                delete [] g_ptr[i];
        }
        Time  end;

        std::cout << "Block " << size << ", used  " << (end - start) << std::endl;
    }
            
    if (g_usePool)
        MemoryPool::Destructor();

    return 0;
}
