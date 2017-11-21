#ifndef PROCESS_CORE_H
#define PROCESS_CORE_H

struct process_core
{
	bool signal_term_stop;			//终止信号
	void  *epoll_ptr;				//epoll句柄
};

#define VOFFSET(TYPE,MEMBER) ((long)&(((TYPE*)0)->MEMBER))

#define PROCESS(PTR,MEMBER) (struct process_core*)( ((char*)PTR) - VOFFSET(struct process_core,MEMBER) )

void processSignal();

//进程绑定cpu
void processBindCPU(int cpuid);
//线程绑定cpu
void threadBindCPU(int cpuid);

struct process_core * processGetDefault();

void processInit(struct process_core * core);
void processSet(struct process_core * core,void * ptr);
void processFree(struct process_core ** core_ptr);

#endif
