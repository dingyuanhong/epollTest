#ifndef EPOLL_EVENT_H
#define EPOLL_EVENT_H

struct epoll_core
{
	int epoll;		//epoll句柄
	struct epoll_event *events_ptr;
	int event_count;
};

struct epoll_core * epollCreate();

void epollInit(struct epoll_core * core);

void epollFree(struct epoll_core ** core_ptr);

int epoll_event_process(struct epoll_core * core,long timeout);

int epoll_event_add(struct epoll_core * core,struct interface_core * connect);

#endif
