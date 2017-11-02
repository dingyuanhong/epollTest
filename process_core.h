#ifndef PROCESS_CORE_H
#define PROCESS_CORE_H

#include "epoll_core.h"

struct process_core
{
	bool signal_term_stop;			//终止信号
	struct epoll_core  *epoll_ptr;	//epoll句柄
};

#define VOFFSET(TYPE,MEMBER) ((long)&(((TYPE*)0)->MEMBER))

#define PROCESS(PTR,MEMBER) (struct process_core*)( ((char*)PTR) - VOFFSET(struct process_core,MEMBER) )

void processSignal();

struct process_core * processGetDefault();

void processInit(struct process_core * core);
void processFree(struct process_core ** core_ptr);

#endif
