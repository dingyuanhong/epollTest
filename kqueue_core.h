#ifndef KQUEUE_CORE_H
#define KQUEUE_CORE_H

#include "module/threadpool.h"

struct kqueue_func
{
	void (*accept)(struct kqueue_core * core,struct interface_core * conn);
	void (*read)(struct kqueue_core * core,struct connect_core * conn);
	void (*write)(struct kqueue_core * core,struct connect_core * conn);
	void (*close)(struct kqueue_core * core,struct connect_core * conn);
};

struct kqueue_core
{
	uv_threadpool_t *pool;
	int handle;		//epoll句柄
	struct kevent *events_ptr;
	int event_count;
	struct kqueue_func conn_func;
};

struct kqueue_core * kqueueCreate();

void kqueueInit(struct kqueue_core * core);

int kqueue_coreCreate(struct kqueue_core * core,int concurrent);
void kqueue_coreDelete(struct kqueue_core * core);

void kqueueFree(struct kqueue_core ** core_ptr);
void kqueue_event_prepare(struct kqueue_core * core);

int kqueue_event_process(struct kqueue_core * core,long timeout);

int kqueue__accept(struct kqueue_core * core,struct interface_core *conn);
int kqueue_event_accept(struct kqueue_core * core,struct interface_core *conn);
int kqueue_event_add(struct kqueue_core * core,struct interface_core * conn);
int kqueue_event_add(struct kqueue_core * core,struct connect_core * conn);
void kqueue_event_status(struct kqueue_core * core,struct connect_core * conn,int status);
int kqueue_event_close(struct kqueue_core * core,struct connect_core *conn);
int kqueue_event_delete(struct kqueue_core * core,struct connect_core *conn);


#endif