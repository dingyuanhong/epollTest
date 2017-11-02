#include "process_core.h"
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

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
