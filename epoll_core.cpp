#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>

#include "config.h"
#include "log.h"
#include "epoll_core.h"
#include "connect_core.h"
#include "process_core.h"

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

struct epoll_core * epollCreate()
{
	struct epoll_core *core = (struct epoll_core*)malloc(sizeof(struct epoll_core));
	epollInit(core);
	return core;
}

void epollInit(struct epoll_core * core)
{
	if(core == NULL) return;
	core->epoll = -1;
}

void epollFree(struct epoll_core ** core_ptr)
{
	if(core_ptr == NULL) return;
	struct epoll_core * core = *core_ptr;
	if(core == NULL) return;
	if(core->epoll != -1)
	{
		close(core->epoll);
	}
	core->epoll = -1;
	free(core);
	*core_ptr = NULL;
}

void * epoll_event_process(void* param)
{
	struct process_core * core = (struct process_core *)param;
	struct config_core * config = getDefaultConfig();
	long ret = (long)epoll_event_process(&core->epoll_ptr,config);
	return (void*)ret;
}

static const char * getEventName(int events)
{
	if(events & EPOLLIN)
	{
		return "EPOLLIN";
	}else if(events & EPOLLOUT)
	{
		return "EPOLLOUT";
	}else if(events & EPOLLERR)
	{
		return "EPOLLERR";
	}else if(events & EPOLLRDHUP)
	{
		return "EPOLLRDHUP";
	}else if(events & EPOLLPRI)
	{
		return "EPOLLPRI";
	}else if(events & EPOLLET)
	{
		return "EPOLLET";
	}else if(events & EPOLLONESHOT)
	{
		return "EPOLLONESHOT";
	}else {
		return "Unknown";
	}
}

static void epoll_event_dump(int event,struct epoll_event *event_ptr)
{
	const char * evnet_name = getEventName(event);
	struct connect_core *connect = (struct connect_core*)event_ptr->data.ptr;
	if(connect == NULL){
		VLOGE("%s connect_core == NULL",evnet_name);
		return;
	}
	VASSERT(connect != NULL);
	int fd = connect->fd;
	VLOGI("%s trigger(%d) events:(%x)",evnet_name,fd,event_ptr->events);
}

static void epoll_event_accept(struct epoll_core * core,struct epoll_event *event_ptr)
{
	struct interface_core *interface_ptr = (struct interface_core*)event_ptr->data.ptr;
	VASSERT(interface_ptr != NULL);
	int fd = interface_ptr->fd;

	struct sockaddr in_addr;
	socklen_t in_len = sizeof(struct sockaddr);
	int connect = accept(fd,&in_addr,&in_len);
	if(connect == -1)
	{
		VLOGE("accept error(%d)",errno);
		return;
	}
	nonBlocking(connect);

	struct connect_core * connect_ptr = connectCreate();
	connect_ptr->fd = connect;

	struct epoll_event event;
	event.data.ptr = (void*)connect_ptr;
	event.events = EPOLLIN;
	//边缘模式
	// event.events = EPOLLIN | EPOLLET;
	int ret = epoll_ctl(core->epoll,EPOLL_CTL_ADD,connect,&event);
	if(ret == -1)
	{
		VLOGE("accept new connect,epoll_ctl err(%d\n)",errno);
		connectFree(&connect_ptr);
		close(connect);
	}else{
		VLOGI("accept new connect(%d) %p",connect,event.data.ptr);
	}
}

static void epoll_event_close(int event,struct epoll_core * core,struct epoll_event *event_ptr)
{
	const char * evnet_name = getEventName(event);
	struct connect_core *connect = (struct connect_core*)event_ptr->data.ptr;
	if(connect == NULL){
		VLOGE("%s connect_core == NULL",evnet_name);
		return;
	}
	VASSERT(connect != NULL);
	int fd = connect->fd;
	VLOGI("%s trigger(%d)",evnet_name,fd);
	int ret = epoll_ctl(core->epoll,EPOLL_CTL_DEL,fd,NULL);
	if(ret != -1)
	{
		int error = connectGetErrno(connect);
		if(error != 0)
		{
			VLOGE("%s trigger(%d) error:(%d)",evnet_name,fd,error);
		}
		connectFree((struct connect_core**)(&event_ptr->data.ptr));
		close(fd);
		VLOGI("socket(%d) close success",fd);
	}else{
		VLOGI("epoll_ctl error(%d)",errno);
	}
}

static void epoll_event_shutdown(int event,struct epoll_event *event_ptr,int howto)
{
	const char * evnet_name = getEventName(event);
	struct connect_core *connect = (struct connect_core*)event_ptr->data.ptr;
	if(connect == NULL){
		VLOGE("%s connect_core == NULL",evnet_name);
		return;
	}
	VASSERT(connect != NULL);
	int fd = connect->fd;
	VLOGI("%s trigger(%d)",fd,evnet_name);
	int ret = shutdown(fd,howto);
	if(ret != -1)
	{
		VLOGW("%s shutdown(%d) error(%d)",evnet_name,fd,errno);
	}else{
		VLOGI("%s shutdown(%d) success",evnet_name,fd);
	}
}

void epoll_event_status(int event,int status,struct epoll_core * core,struct epoll_event *event_ptr)
{
	const char * evnet_name = getEventName(event);
	struct connect_core *connect = (struct connect_core*)event_ptr->data.ptr;
	if(connect == NULL) return;
	VASSERT(connect != NULL);
	int fd = connect->fd;

	if(status & CONNECT_STATUS_CLOSE)
	{
		event_ptr->events &= ~EPOLLIN;
		event_ptr->events &= ~EPOLLOUT;
		event_ptr->events |= EPOLLRDHUP;
		VLOGI("CONNECT_STATUS_CLOSE(%d) events:%x",fd,event_ptr->events);
		int ret = epoll_ctl(core->epoll,EPOLL_CTL_MOD,fd,event_ptr);
		if(ret == -1)
		{
			VLOGE("%s(%d) epoll_ctl close error.",evnet_name,fd);
		}
	}else if(status & CONNECT_STATUS_SEND)
	{
		event_ptr->events &= ~EPOLLIN;
		event_ptr->events |= EPOLLOUT;
		VLOGI("CONNECT_STATUS_SEND(%d) events:%x",fd,event_ptr->events);
		int ret = epoll_ctl(core->epoll,EPOLL_CTL_MOD,fd,event_ptr);
		if(ret == -1)
		{
			VLOGE("%s(%d) epoll_ctl send error.",evnet_name,fd);
		}
	}else if(status & CONNECT_STATUS_RECV)
	{
		event_ptr->events &= ~EPOLLOUT;
		event_ptr->events |= EPOLLIN;
		VLOGI("CONNECT_STATUS_RECV(%d) events:%x",fd,event_ptr->events);
		int ret = epoll_ctl(core->epoll,EPOLL_CTL_MOD,fd,event_ptr);
		if(ret == -1)
		{
			VLOGE("%s(%d) epoll_ctl recv error.",evnet_name,fd);
		}
	}else if(status & CONNECT_STATUS_CONTINUE)
	{
		// VLOGI("CONNECT_STATUS_CONTINUE(%d) events:%x",fd,event_ptr->events);
		// int ret = epoll_ctl(core->epoll,EPOLL_CTL_MOD,fd,event_ptr);
		// if(ret == -1)
		// {
		// 	VLOGE("%s(%d) epoll_ctl recv error.",evnet_name,fd);
		// }
	}else if(status == 0)
	{

	}
	else{
		VLOGE("Unnown(%d) events:%d stasus:%d error",fd,event,status);
	}
}

int epoll_event_process(struct epoll_core ** core_ptr,struct config_core * config)
{
	struct process_core * process = PROCESS(core_ptr,epoll_ptr);
	VASSERT(process != NULL);
	VASSERT(process->signal_term_stop == false);

	struct epoll_core * core = *core_ptr;
	int event_count = config->concurrent;
	event_count = max(1,event_count);
	struct epoll_event *events_ptr = (struct epoll_event*)malloc(sizeof(struct epoll_event)*event_count);
	if(events_ptr == NULL)
	{
		VLOGE("memory not enough.");
		return -1;
	}
	VASSERT(events_ptr != NULL);

	VASSERT(event_count > 0);
	while(!process->signal_term_stop)
	{
		int n = epoll_wait(core->epoll,events_ptr,event_count,config->timeout);
		if(n <= 0)
		{
			continue;
		}
		// VLOGI("接收到消息:%d",n);
		for(int i = 0 ; i < min(n,event_count); i++)
		{
			struct epoll_event *event_ptr = &events_ptr[i];
			VASSERT(event_ptr != NULL);
			int events = event_ptr->events;
			// VLOGI("events:%x %p",events,event_ptr->data.ptr);

			struct interface_core *interface_ptr = (struct interface_core*)event_ptr->data.ptr;
			VASSERT(interface_ptr != NULL);

			if(interface_ptr->type & INTERFACE_TYPE_SERVER)
			{
				epoll_event_accept(core,event_ptr);
			}
			else if(events & EPOLLIN)
			{
				struct connect_core *connect = (struct connect_core*)event_ptr->data.ptr;
				if(connect == NULL){
					VLOGE("EPOLLOUT connect_core == NULL");
					continue;
				}
				int fd = connect->fd;
				// VLOGI("EPOLLIN(%d) data ...",fd);
				int ret = connectRead(fd,connect);
				// VLOGI("EPOLLIN(%d) data .",fd);
				epoll_event_status(EPOLLIN,ret,core,event_ptr);
			}
			else if(events & EPOLLOUT)
			{
				struct connect_core *connect = (struct connect_core*)event_ptr->data.ptr;
				if(connect == NULL){
					VLOGE("EPOLLOUT connect_core == NULL");
					continue;
				}
				int fd = connect->fd;
				// VLOGI("EPOLLOUT(%d) data ...",fd);
				int ret = connectWrite(fd,connect);
				// VLOGI("EPOLLOUT(%d) data .",fd);
				epoll_event_status(EPOLLOUT,ret,core,event_ptr);

			}
			else if(events & EPOLLRDHUP)
			{
				//主动请求关闭
				//对端关闭或者关闭写模式,本端关闭读模式
				// epoll_event_shutdown(EPOLLRDHUP,event_ptr,SHUT_RD);
				epoll_event_close(EPOLLRDHUP,core,event_ptr);
			}
			else if(events & EPOLLHUP)
			{
				//对端关闭或者关闭写模式,本端关闭读模式
				epoll_event_shutdown(EPOLLRDHUP,event_ptr,SHUT_RD);
				// epoll_event_close(EPOLLRDHUP,core,event_ptr);
			}
			else if(events & EPOLLERR)
			{
				epoll_event_close(EPOLLERR,core,event_ptr);
			}else if(events & EPOLLPRI)
			{
				//带外数据
				epoll_event_dump(EPOLLET,event_ptr);
			}
			else if(events & EPOLLET)
			{
				//边缘触发
				epoll_event_dump(EPOLLET,event_ptr);
			}
			// else if(events & EPOLLNVAL)
			// {
			// 	//文件描述符未打开
			// 	epoll_event_dump(EPOLLNVAL,event_ptr);
			// }
			else{
				epoll_event_dump(0,event_ptr);
			}
		}
	}
	free(events_ptr);
	return 0;
}

int epoll_event_add(struct epoll_core * core,struct interface_core * connect)
{
	epoll_event event = {0};
	event.events = EPOLLIN;
	// event.events = EPOLLIN | EPOLLET;
	event.data.ptr = (void*)connect;
	int ret = epoll_ctl(core->epoll,EPOLL_CTL_ADD,connect->fd,&event);
	return ret;
}
