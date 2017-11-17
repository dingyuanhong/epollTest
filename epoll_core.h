#ifndef EPOLL_EVENT_H
#define EPOLL_EVENT_H

#include "module/threadpool.h"

struct epoll_func
{
	void (*accept)(struct epoll_core * epoll,struct interface_core * conn);
	void (*read)(struct epoll_core * epoll,struct connect_core * conn);
	void (*write)(struct epoll_core * epoll,struct connect_core * conn);
	void (*close)(struct epoll_core * epoll,struct connect_core * conn);
};

struct epoll_core
{
	uv_threadpool_t *pool;
	int epoll;		//epoll句柄
	struct epoll_event *events_ptr;
	int event_count;
	struct epoll_func conn_func;
};

struct epoll_core * epollCreate();

void epollInit(struct epoll_core * core);

void epollFree(struct epoll_core ** core_ptr);
void epoll_event_prepare(struct epoll_core * core);

int epoll_event_process(struct epoll_core * core,long timeout);

int epoll__accept(struct epoll_core * core,struct interface_core *interface_ptr);
int epoll_event_accept(struct epoll_core * core,struct interface_core *interface_ptr);
int epoll_event_add(struct epoll_core * core,struct interface_core * connect);
int epoll_event_add(struct epoll_core * core,struct connect_core * connect);
void epoll_event_status(struct epoll_core * core,struct connect_core * connect,int status);
int epoll_event_close(struct epoll_core * core,struct connect_core *connect);
int epoll_event_delete(struct epoll_core * core,struct connect_core *connect);

void errnoDump(const char * name);

#endif
