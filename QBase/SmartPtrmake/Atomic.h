#ifndef BERT_ATOMIC_H
#define BERT_ATOMIC_H


#if defined(__gnu_linux__)

#define AtomicChange(plAddEnd, delta) __sync_fetch_and_add((plAddEnd), (delta))
#define AtomicChange64                AtomicChange

#define AtomicSet(plTarget, value)    __sync_lock_test_and_set ((plTarget), (value))
#define AtomicSet64                   AtomicSet

#define AtomicTestAndSet(plDest, lExchange, lComparand)\
    __sync_val_compare_and_swap((plDest), (lComparand), (lExchange))

#define AtomicTestAndSet64            AtomicTestAndSet


#else
#include <Windows.h>

#define AtomicChange(plAddEnd, delta)    InterlockedExchangeAdd((long*)(plAddEnd), (long)(delta))
#define AtomicChange64(pllAddEnd, delta) InterlockedExchangeAdd64((long long*)(pllAddEnd), (long long)(delta))

#define AtomicSet(plTarget, value)       InterlockedExchange((long*)(plTarget), (long)(value))
#define AtomicSet64(pllTarget, value)    InterlockedExchange64((long long*)(pllTarget), (long long)(value))

#define AtomicTestAndSet(plDest, lComparand, lExchange)\
    InterlockedCompareExchange((long*)(plDest), (long)(lExchange), (long)(lComparand))

#define AtomicTestAndSet64(pllDest, llComparand, llExchange)\
    InterlockedCompareExchange64((long long*)(pllDest), (long long)(llExchange), (long long)(llComparand))

#endif

#define AtomicGet(plTarget)      AtomicChange(plTarget, 0)
#define AtomicGet64(plTarget)    AtomicChange64(plTarget, 0)



#endif

