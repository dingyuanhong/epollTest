#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#endif
#include "staticUtil.h"
#include "util.h"
#include "log.h"

int getCPUCount()
{
#ifdef _WIN32
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	VLOGD("Number of processors: %d.\n", info.dwNumberOfProcessors);
	int cpu_num = info.dwNumberOfProcessors;
	return cpu_num;
#elif __MAC__
	return 0;
#else
	//系统总核数
	int cpu_num = sysconf(_SC_NPROCESSORS_CONF);
	VLOGI("CORE PROCESSORS_CONF=%d\n",cpu_num);
	cpu_num = get_nprocs_conf();
	//系统可使用核数
	cpu_num = get_nprocs();
	cpu_num = sysconf(_SC_NPROCESSORS_ONLN);
	return cpu_num;
#endif
}

int64_t getMemoryTotal()
{
#ifdef _WIN32
	MEMORYSTATUS memStatus;
    GlobalMemoryStatus(&memStatus);
	return memStatus.dwTotalPhys;
#elif __MAC__
	return 0;
#else
	struct sysinfo si;
    sysinfo(&si);
	return si.totalram;
#endif
}

int64_t getMemoryUsage()
{
#ifdef _WIN32
	MEMORYSTATUS memStatus;
    GlobalMemoryStatus(&memStatus);
	return memStatus.dwAvailPhys;
#elif __MAC__
	return 0;
#else
	struct sysinfo si;
    sysinfo(&si);
	return si.freeram;
#endif
}

//线程绑定cpu
void threadAffinityCPU(int cpuid)
{
#ifdef _WIN32
	DWORD_PTR ret = SetThreadAffinityMask(GetCurrentThread(),cpuid);
	if(ret == 0)
	{
		VLOGE("SetThreadAffinityMask GetLastError:%d",GetLastError());
	}
#elif __MAC__
#else
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(cpuid, &mask);

	if (pthread_setaffinity_np(pthread_self(), sizeof(mask),&mask) < 0) {
		VLOGE("pthread_setaffinity_np error(%d)",errno);
	}
#endif
}

//进程绑定cpu
void processAffinityCPU(int cpuid)
{
#ifdef _WIN32
#elif __MAC__
#else
	cpu_set_t mask;
	CPU_ZERO(&mask);
    CPU_SET(cpuid, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) < 0) {
        VLOGE("sched_setaffinity error(%d)",errno);
    }
#endif
}