#include "process_core.h"
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include "log.h"

static void handle_signal_term(int sig)
{
	VLOGI("signal exit:%d",sig);
	struct process_core *core = processGetDefault();
	core->signal_term_stop = true;
}

void processSignal(){
	signal(SIGTERM , handle_signal_term);
	signal(SIGINT , handle_signal_term);
	signal(SIGQUIT , handle_signal_term);
}

//进程绑定cpu
void processBindCPU(int cpuid)
{
	cpu_set_t mask;
	CPU_ZERO(&mask);
    CPU_SET(cpuid, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) < 0) {
        VLOGE("sched_setaffinity error(%d)",errno);
    }
}

//线程绑定cpu
void threadBindCPU(int cpuid)
{
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(cpuid, &mask);

	if (pthread_setaffinity_np(pthread_self(), sizeof(mask),&mask) < 0) {
		VLOGE("pthread_setaffinity_np error(%d)",errno);
	}
}

static struct process_core static_core;
static struct process_core * static_core_ptr = NULL;
struct process_core * processGetDefault()
{
	if(static_core_ptr == NULL)
	{
		processInit(&static_core);
		static_core_ptr = &static_core;
	}
	return static_core_ptr;
}

void processInit(struct process_core * core)
{
	if(core == NULL) return;
	core->epoll_ptr = epollCreate();
	core->signal_term_stop = false;
}

void processFree(struct process_core ** core_ptr)
{
	if(core_ptr == NULL) return;
	struct process_core * core = *core_ptr;
	if(core == NULL) return;

	epollFree(&core->epoll_ptr);

	*core_ptr = NULL;
}
