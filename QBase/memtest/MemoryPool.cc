#include <cassert>
#include <cstdlib>
#include <new>
#include <iostream>
#include "MemoryPool.h"


__thread MemoryPool::mem_node* MemoryPool::m_freelist[MemoryPool::BUCKETS] = { NULL };
__thread void*      MemoryPool::m_pool = NULL;
__thread unsigned   MemoryPool::m_poolSize = 0;

std::vector<void* >     MemoryPool::m_rawPtr;
Mutex       MemoryPool::m_mutex;

unsigned MemoryPool::_RoundUp( unsigned size )
{
    return ( size + ALIGN - 1 ) & ~( ALIGN - 1 ) ;
}

unsigned MemoryPool::_GetBucketIndex(unsigned size)
{
    assert (0 != size);
    assert (size <= MAX_SIZE && "size should not bigger than MAX_SIZE!!");
    unsigned index = (size - 1) / ALIGN;
    return index < BUCKETS ? index : BUCKETS;
}

void MemoryPool::Destructor()
{
    ScopeMutex   guard(m_mutex);

    for (unsigned int i = 0; i < m_rawPtr.size(); ++ i)
    {
        ::free(m_rawPtr[i]);
    }
    m_rawPtr.clear();
}

void* MemoryPool::allocate(unsigned size)
{
    if (0 == size)  return NULL;

    void* ret = NULL;
    size = _RoundUp(size);
    if (size > MAX_SIZE)
    {
        return ::malloc(size);
    }
    unsigned index = _GetBucketIndex(size);
    assert (index < BUCKETS);// must success

    if (m_freelist[index] != NULL)
    {
        ret = m_freelist[index];
        m_freelist[index] = m_freelist[index]->next_free;
        return ret;
    }
    else
    {
        if (m_poolSize > 0)
        {
            if (m_poolSize >= size)
            {
                ret  = m_pool;
                m_poolSize -= size;
                m_pool = (char *)m_pool + size;
                return ret;
            }
            else
            {
                //回收 pool;递归调用自己!
                unsigned idx = _GetBucketIndex(m_poolSize);
                assert(idx < BUCKETS);

                mem_node* temp = (mem_node *)m_pool;
                temp->next_free= m_freelist[idx];
                m_freelist[idx]= temp;
                
                m_poolSize = 0;
                m_pool     = NULL;

                return allocate(size);
            }
        }
        else
        {
            unsigned malloc_size = size * TRUNK_NUM;
            ret = ::malloc(malloc_size);
            if (NULL == ret)
            {
                std::cerr << "alloc failed " << malloc_size << std::endl;
                return NULL;
            }
            m_mutex.Lock();
            m_rawPtr.push_back(ret);
            m_mutex.Unlock();
            m_poolSize = size * (TRUNK_NUM >> 1);
            m_pool     = (char *)ret + malloc_size - m_poolSize;
            m_freelist[index] = (mem_node *)((char *)ret + size);

            mem_node* iterator = m_freelist[index];
            for (unsigned i = 1; i < (TRUNK_NUM+1) / 2 - 1; ++i)
            {
                iterator->next_free = (mem_node *)((char *)iterator + size);
                iterator = iterator->next_free;
            }
            iterator->next_free = NULL;// end
            return ret;
        }
    }

    assert (false);
    return ret; //never reach
}

void MemoryPool::deallocate(const void* p, unsigned size )
{
    size = _RoundUp(size); // !!! important
    if (size > MAX_SIZE)
    {
        ::free(const_cast<void* >(p));
    }
    else
    {
        unsigned index   = _GetBucketIndex(size);
        mem_node* temp   = (mem_node *)p;
        temp->next_free  = m_freelist[index];
        m_freelist[index]= temp;
    }
}

void * MemoryPool::operator new(std::size_t size)
{
    std::cerr << "Try new " << size << std::endl;
    return allocate(size);
}

void * MemoryPool::operator new[](std::size_t size)
{
    std::cerr << "Try new[] " << size << std::endl;
    return allocate(size);
}

void MemoryPool::operator delete(void* p , std::size_t size)
{
    std::cerr << "Try delete " << size << std::endl;
    deallocate(p, size);
}

void MemoryPool::operator delete[](void* p , std::size_t size)
{
    std::cerr << "Try delete[] " << size << std::endl;
     deallocate(p, size);
}

