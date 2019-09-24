#ifndef LFDEF_H
#define LFDEF_H

#ifdef _WIN32
#if defined(NTDDI_VERSION) && WINNT == 1
#include <wdm.h>
#ifndef BOOL
#define BOOL BOOLEAN
#endif
#ifdef _WIN64 // [
typedef signed __int64    intptr_t;
typedef unsigned __int64  uintptr_t;
#else // _WIN64 ][
typedef _W64 signed int   intptr_t;
typedef _W64 unsigned int uintptr_t;
#endif // _WIN64 ]
#else
#include <Windows.h>
#include <stdint.h>
#include <stdio.h>
#endif
#endif

#ifdef __cplusplus
extern  "C"
{
#endif

#ifdef _WIN32

static __inline unsigned int InterlockedExchangeUINT(unsigned int volatile * Target, unsigned int  Value)
{
	return (unsigned int)InterlockedExchange((long volatile *)Target, Value);
}

static __inline unsigned int InterlockedCompareExchangeUINT(unsigned int volatile * Destination,
	unsigned int Exchange, unsigned int Comparand)
{
	return (unsigned int)InterlockedCompareExchange((long volatile  *)Destination, (long)Exchange, (long)Comparand);
}

static __inline unsigned int InterlockedIncrementUINT(unsigned int volatile *Addend)
{
	return (unsigned int)InterlockedIncrement((long volatile *)Addend);
}

static __inline unsigned int InterlockedDecrementUINT(unsigned int volatile *Addend)
{
	return (unsigned int)InterlockedDecrement((long volatile *)Addend);
}

#elif defined(__linux__)

#define InterlockedCompareExchange(d, e, c)         __sync_val_compare_and_swap(d, c, e)
#define InterlockedCompareExchange64(d, e, c)       __sync_val_compare_and_swap(d, c, e)
#define InterlockedCompareExchangeUINT(d, e, c)     __sync_val_compare_and_swap(d, c, e)
#define InterlockedCompareExchangePointer(d, e, c)  __sync_val_compare_and_swap(d, c, e)
#define InterlockedExchange                         __sync_lock_test_and_set
#define InterlockedExchangeUINT                     __sync_lock_test_and_set
#define InterlockedExchangePointer                  __sync_lock_test_and_set
#define InterlockedIncrement(x)                     __sync_add_and_fetch(x, 1)
#define InterlockedIncrementUINT(x)                 __sync_add_and_fetch(x, 1)
#define InterlockedDecrement(x)                     __sync_sub_and_fetch(x, 1)
#define InterlockedDecrementUINT(x)                 __sync_sub_and_fetch(x, 1)

#define InterlockedExchangeAdd(x, v)                __sync_fetch_and_add(x, v)
#define InterlockedExchangeAdd64(x, v)              __sync_fetch_and_add(x, v)
#define InterlockedExchange64                       __sync_lock_test_and_set

#endif

#ifdef USE_STRICT_INTERLOCKED
#define InterlockedRead(x) InterlockedExchangeAdd(x, 0)
#define InterlockedRead64(x) InterlockedExchangeAdd64(x, 0)
#else
static __inline long InterlockedRead(long volatile *x)
{
    return *x;
}
static __inline long long InterlockedRead64(long long volatile *x)
{
    return *x;
}
#endif

#ifdef _DEBUG
#ifdef _NTDDK_
#define log_debugf(x) DbgPrint x
#else
#define log_debugf(x) printf x
#endif
#else
#define log_debugf(x)
#endif

void revolve(int yield);
void dead_loop();
long get_revolve_cycles();

#ifdef __cplusplus
}
#endif

#endif
