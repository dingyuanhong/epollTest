#include <unistd.h>
#include <sched.h>
#include <sys/sysinfo.h>
#include <pthread.h>

#include "util.h"
#include "log.h"

int getCPUCount()
{
	//系统总核数
	int cpu_num = sysconf(_SC_NPROCESSORS_CONF);
	VLOGI("CORE PROCESSORS_CONF=%d\n",cpu_num);
	cpu_num = get_nprocs_conf();
	//系统可使用核数
	cpu_num = get_nprocs();
	cpu_num = sysconf(_SC_NPROCESSORS_ONLN);

	return cpu_num;
}

int64_t getMemoryTotal()
{
	struct sysinfo si;
    sysinfo(&si);
	return si.totalram;
}

int64_t getMemoryUsage()
{
	struct sysinfo si;
    sysinfo(&si);
	return si.freeram;
}
