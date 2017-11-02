#ifndef EPOLL_EVENT_H
#define EPOLL_EVENT_H

struct epoll_core
{
	int epoll;		//epoll句柄
};

struct epoll_core * epollCreate();

void epollInit(struct epoll_core * core);

void epollFree(struct epoll_core ** core_ptr);

void * epoll_event_process(void*);

int epoll_event_process(struct epoll_core ** core_ptr,struct config_core * config);

#endif
