#ifndef ATOMIC_H
#define ATOMIC_H

#include "sysdef.h"
#ifdef _WIN32
#include <Windows.h>
#else
#include "atomic-ops.h"
#endif

#if defined(SYS_CC_GNUC)
typedef long ATOMIC_TYPE;

#ifndef atomic_inc
#   define atomic_inc(var)           __sync_fetch_and_add        (&(var), 1)
#endif
#ifndef atomic_dec
#   define atomic_dec(var)           __sync_fetch_and_sub        (&(var), 1)
#endif
#ifndef atomic_add
#   define atomic_add(var, val)      __sync_fetch_and_add        (&(var), (val))
#endif
#ifndef atomic_sub
#   define atomic_sub(var, val)      __sync_fetch_and_sub        (&(var), (val))
#endif
#ifndef atomic_set
#   define atomic_set(var, val)      __sync_lock_test_and_set    (&(var), (val))
#endif
#ifndef atomic_cas
#   define atomic_cas(var, cmp, val) ((cmp) == __sync_bool_compare_and_swap(&(var), (cmp), (val)))
#endif
#ifndef atomic_cas_ptr
#   define atomic_cas_ptr(var, cmp, val) __sync_bool_compare_and_swap(&(var), (cmp), (val))
#endif
#ifndef atomic_read
#	define atomic_read(var) 		 __sync_add_and_fetch		 (&(var),0)
#endif

#elif defined(SYS_OS_WIN)
typedef LONG volatile ATOMIC_TYPE;

#   define atomic_inc(var)           InterlockedExchangeAdd      (&(var), 1)
#   define atomic_dec(var)           InterlockedExchangeAdd      (&(var),-1)
#   define atomic_add(var, val)      InterlockedExchangeAdd      (&(var), (val))
#   define atomic_sub(var, val)      InterlockedExchangeAdd      (&(var),-(val))
#   define atomic_set(var, val)      InterlockedExchange         (&(var), (val))
#   define atomic_cas(var, cmp, val) ((cmp) == InterlockedCompareExchange(&(var), (val), (cmp)))
#   define atomic_cas_ptr(var, cmp, val) InterlockedCompareExchangePointer((PVOID volatile *)&(var), (PVOID)(val), (PVOID)(cmp))
#	define atomic_read(var) 		 InterlockedExchangeAdd      (&(var), 0)
#else
#   error "This platform is unsupported"
#endif

#if defined(SYS_OS_WIN64)
typedef LONGLONG volatile ATOMIC_TYPE64;

#   define atomic_inc64(var)           InterlockedExchangeAdd64    (&(var), 1)
#   define atomic_dec64(var)           InterlockedExchangeAdd64    (&(var),-1)
#   define atomic_add64(var, val)      InterlockedExchangeAdd64    (&(var), (val))
#   define atomic_sub64(var, val)      InterlockedExchangeAdd64    (&(var),-(val))
#   define atomic_set64(var, val)      InterlockedExchange64       (&(var), (val))
#   define atomic_cas64(var, cmp, val) ((cmp) == InterlockedCompareExchange64(&(var), (val), (cmp)))
#   define atomic_cas_ptr64(var, cmp, val) InterlockedCompareExchangePointer64(&(var), (val), (cmp))
#	define atomic_read64(var) 		 InterlockedExchangeAdd64    (&(var), 0)
#endif

#   if defined(SYS_OS_WIN)
#include <stdint.h>
static inline int usleep(int64_t microseconds)
{
	LARGE_INTEGER litmp;
	LONGLONG QPart1, QPart2;
	double diffMinus, diffFreq, diffTime;
	QueryPerformanceFrequency(&litmp);
	diffFreq = (double)litmp.QuadPart;// ���ü�������ʱ��Ƶ��
	QueryPerformanceCounter(&litmp);
	QPart1 = litmp.QuadPart;// ���ó�ʼֵ
	do {
		QueryPerformanceCounter(&litmp);
		QPart2 = litmp.QuadPart;//������ֵֹ
		diffMinus = (double)(QPart2 - QPart1);
		diffTime = diffMinus * 1000 / diffFreq;// ���ö�Ӧ��ʱ��ֵ����λΪ��
		diffTime = microseconds - diffTime;
		if (diffTime <= 0)
		{
			break;
		}
		else if (diffTime > 16)
		{
			Sleep(16);
		}
	} while (true);
	return 0;
}
#endif

#if defined(SYS_OS_MAC)
#include <unistd.h>
#elif defined(SYS_OS_LINUX)
#include <pthread.h>
#endif

static inline void yield(void)
{
#   if defined(SYS_OS_WINCE)
        Sleep(1);
#   elif defined(SYS_OS_WIN)
        SwitchToThread();
#	elif defined(SYS_OS_MAC)
        usleep(1);
#   else
        pthread_yield();
#   endif
}

#endif
