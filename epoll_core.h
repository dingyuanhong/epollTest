#ifndef EPOLL_EVENT_H
#define EPOLL_EVENT_H

#include "module/threadpool.h"

struct epoll_core
{
	uv_threadpool_t *pool;
	int epoll;		//epoll句柄
	struct epoll_event *events_ptr;
	int event_count;
};

struct epoll_core * epollCreate();

void epollInit(struct epoll_core * core);

void epollFree(struct epoll_core ** core_ptr);

int epoll_event_process(struct epoll_core * core,long timeout);

int epoll_event_add(struct epoll_core * core,struct interface_core * connect);
int epoll_event_add(struct epoll_core * core,struct connect_core * connect);
void epoll_event_status(struct epoll_core * core,struct epoll_event *event_ptr,int status);


void errnoDump(const char * name);

#endif
