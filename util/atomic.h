#ifndef ATOMIC_H
#define ATOMIC_H

#ifdef _WIN32
#include <Windows.h>
#else
#include "atomic-ops.h"

//long InterlockedCompareExchangePointer(long volatile * des,long exchange,long comparand);
#define InterlockedCompareExchangePointer(A,E,C) cmpxchgl((long*)A,(long)C,(long)E)

#endif
#endif
