#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

int getCPUCount();
int64_t getMemoryTotal();
int64_t getMemoryUsage();

void threadAffinityCPU(int cpuid);

#endif
