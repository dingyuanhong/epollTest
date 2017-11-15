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
	core->pool = NULL;
	core->epoll = -1;
	core->events_ptr = NULL;
	core->event_count = 0;
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
	if(core->events_ptr != NULL)
	{
		free(core->events_ptr);
		core->events_ptr = NULL;
	}
	core->event_count = 0;
	core->pool = NULL;
	free(core);
	*core_ptr = NULL;
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
	connect_ptr->ptr = (void*)core;

	struct epoll_event event;
	event.data.ptr = (void*)connect_ptr;
	event.events = EPOLLIN | EPOLLONESHOT;
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
	VASSERT(core != NULL);

	if(status & CONNECT_STATUS_CLOSE)
	{
		VLOGD("CONNECT_STATUS_CLOSE(%d) events:%x",fd,event_ptr->events);

		event_ptr->events &= ~EPOLLIN;
		event_ptr->events &= ~EPOLLOUT;
		event_ptr->events |= EPOLLRDHUP;
		int ret = epoll_ctl(core->epoll,EPOLL_CTL_MOD,fd,event_ptr);
		if(ret == -1)
		{
			VLOGE("%s(%d) epoll_ctl error.",evnet_name,fd);
		}
	}else if(status & CONNECT_STATUS_SEND)
	{
		VLOGD("CONNECT_STATUS_SEND(%d) events:%x",fd,event_ptr->events);

		event_ptr->events &= ~EPOLLIN;
		event_ptr->events |= EPOLLOUT;
		int ret = epoll_ctl(core->epoll,EPOLL_CTL_MOD,fd,event_ptr);
		if(ret == -1)
		{
			VLOGE("%s(%d) epoll_ctl error(%d).",evnet_name,fd,errno);
		}
	}else if(status & CONNECT_STATUS_RECV)
	{
		// VLOGD("CONNECT_STATUS_RECV(%d) events:%x",fd,event_ptr->events);

		event_ptr->events &= ~EPOLLOUT;
		event_ptr->events |= EPOLLIN;
		int ret = epoll_ctl(core->epoll,EPOLL_CTL_MOD,fd,event_ptr);
		if(ret == -1)
		{
			VLOGE("%s(%d) epoll_ctl error(%d).",evnet_name,fd,errno);
		}
	}else if(status & CONNECT_STATUS_CONTINUE)
	{
		VLOGD("CONNECT_STATUS_CONTINUE(%d) events:%x",fd,event_ptr->events);
		if(event_ptr->events & EPOLLONESHOT)
		{
			int ret = epoll_ctl(core->epoll,EPOLL_CTL_MOD,fd,event_ptr);
			if(ret == -1)
			{
				VLOGE("%s(%d) epoll_ctl error(%d).",evnet_name,fd,errno);
			}
		}
	}else if(status == 0)
	{
		VLOGD("status == 0 (%d) events:%x",fd,event_ptr->events);
		if(event_ptr->events & EPOLLONESHOT)
		{
			event_ptr->events &= ~EPOLLOUT;
			event_ptr->events |= EPOLLIN;
			int ret = epoll_ctl(core->epoll,EPOLL_CTL_MOD,fd,event_ptr);
			if(ret == -1)
			{
				VLOGE("%s(%d) epoll_ctl error(%d).",evnet_name,fd,errno);
			}
		}
	}
	else{
		VLOGE("Unnown(%d) events:%d stasus:%d error",fd,event,status);
	}
}

static void work_read(uv_work_t* req)
{
	//uv_work_t* req = container_of(w, uv_work_t, work_req);
	struct connect_core *connect = container_of(req,struct connect_core,work);
	int ret = 0;
	while(true)
	{
		ret = connectRead(connect);
		if(ret == CONNECT_STATUS_CONTINUE)
		{
			if(connect->events & EPOLLET)
			{
				continue;
			}
		}
		break;
	}
	connect->ret = ret;
}

static void done_read(uv_work_t* req, int status)
{
	//uv_work_t* req = container_of(w, uv_work_t, work_req);
	struct connect_core *connect = container_of(req,struct connect_core,work);
	VASSERT(connect != NULL);
	struct epoll_core * core = (struct epoll_core *)connect->ptr;
	VASSERT(core != NULL);

	int ret = connect->ret;
	struct epoll_event event;
	event.data.ptr = (void*)connect;
	event.events = connect->events;
	event.events |= EPOLLONESHOT;
	epoll_event_status(EPOLLIN,ret,core,&event);
}

static void work_write(uv_work_t* req)
{
	//uv_work_t* req = container_of(w, uv_work_t, work_req);
	struct connect_core *connect = container_of(req,struct connect_core,work);
	int ret = connectWrite(connect);
	connect->ret = ret;
}

static void done_write(uv_work_t* req, int status)
{
	//uv_work_t* req = container_of(w, uv_work_t, work_req);
	struct connect_core *connect = container_of(req,struct connect_core,work);
	VASSERT(connect != NULL);
	struct epoll_core * core = (struct epoll_core *)connect->ptr;
	VASSERT(core != NULL);

	int ret = connect->ret;
	struct epoll_event event;
	event.data.ptr = (void*)connect;
	event.events = connect->events;
	event.events |= EPOLLONESHOT;
	epoll_event_status(EPOLLOUT,ret,core,&event);
}

int epoll_event_process(struct epoll_core * core,long timeout)
{
	int event_count = core->event_count;
	struct epoll_event *events_ptr = core->events_ptr;

	if(events_ptr == NULL)
	{
		event_count = max(1,event_count);
		events_ptr = (struct epoll_event*)malloc(sizeof(struct epoll_event)*event_count);
		if(events_ptr == NULL)
		{
			VLOGE("memory not enough.");
			return -1;
		}
		VASSERT(events_ptr != NULL);
		VASSERT(event_count > 0);

		core->events_ptr = events_ptr;
		core->event_count = event_count;
	}

	int n = epoll_wait(core->epoll,events_ptr,event_count,timeout);
	if(n <= 0)
	{
		return -1;
	}
	// VLOGI("接收到消息:%d",n);
	for(int i = 0 ; i < min(n,event_count); i++)
	{
		struct epoll_event *event_ptr = &events_ptr[i];
		VASSERT(event_ptr != NULL);

		struct interface_core *interface_ptr = (struct interface_core*)event_ptr->data.ptr;
		VASSERT(interface_ptr != NULL);

		if(interface_ptr->type & INTERFACE_TYPE_SERVER)
		{
			epoll_event_accept(core,event_ptr);
		}
		else{
			int events = event_ptr->events;
			// VLOGD("events:%x %p",events,event_ptr->data.ptr);
			struct connect_core *connect = (struct connect_core*)event_ptr->data.ptr;
			if(connect == NULL){
				VLOGE("EPOLLOUT connect_core == NULL");
				continue;
			}
			connect->events = events;
			if(events & EPOLLIN)
			{
				uv_queue_work(core->pool,&connect->work,work_read,done_read);
			}
			else if(events & EPOLLOUT)
			{
				uv_queue_work(core->pool,&connect->work,work_write,done_write);
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
