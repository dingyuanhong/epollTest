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
#include "atomic.h"

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
	}else if(events & EPOLLHUP)
	{
		return "EPOLLHUP";
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
	VASSERT(connect != NULL);
	int fd = connect->fd;
	VLOGI("%s events:%x fd:%d  ptr:%p",evnet_name,event_ptr->events,fd,connect);
}

void errnoDump(const char * name)
{
	if(errno == 0) return;
	if(EMFILE == errno || ENFILE == errno)
	{
		VLOGE("%s Upper limit of file descriptor(%d)",name,errno);
	}
	else if(ECONNABORTED == errno)
	{
		VLOGE("%s Connect abort(%d)",name,errno);
	}
	else if(ENOBUFS == errno || ENOMEM == errno)
	{
		VLOGE("%s Memeory not enough(%d)",name,errno);
	}
	else if(EPERM == errno)
	{
		VLOGE("%s Firewal prohibited(%d)",name,errno);
	}
	else if(ENOTSOCK == errno)
	{
		VLOGE("%s Descriptor not socket(%d)",name,errno);
	}
	else if(EBADF == errno)
	{
		VLOGE("%s Descriptor not invalid(%d)",name,errno);
	}
	else if(ENOTSOCK == errno)
	{
		VLOGE("%s Not socket(%d)",name,errno);
	}
	else{
		VLOGE("%s,error(%d)",name,errno);
	}
}

static int epoll_event_accept(struct epoll_core * core,struct interface_core *interface_ptr)
{
	VASSERT(interface_ptr != NULL);
	int fd = interface_ptr->fd;

	struct sockaddr in_addr;
	socklen_t in_len = sizeof(struct sockaddr);
	int connect = accept(fd,&in_addr,&in_len);
	if(connect == -1)
	{
		if(EAGAIN == errno || EWOULDBLOCK == errno)
		{
			return 0;
		}else{
			errnoDump("accept");
		}
		return -1;
	}
	nonBlocking(connect);
	cloexec(connect);
	keepalive(connect,1);

	struct connect_core * connect_ptr = connectCreate();
	connect_ptr->fd = connect;
	connect_ptr->ptr = (void*)core;

	int ret = epoll_event_add(core,connect_ptr);
	if(ret == -1)
	{
		errnoDump("epoll_event_add");
		connectFree(&connect_ptr);
		close(connect);
		return -1;
	}else{
		VLOGI("accept new connect(%d) %p",connect,connect_ptr);
		return 1;
	}
	return 0;
}

static void epoll_event_close(struct epoll_core * core,struct epoll_event *event_ptr)
{
	struct connect_core *connect = (struct connect_core*)event_ptr->data.ptr;
	VASSERT(connect != NULL);
	int fd = connect->fd;

	int ret = epoll_ctl(core->epoll,EPOLL_CTL_DEL,fd,NULL);
	if(ret == 0)
	{
		int error = connectGetErrno(connect);
		VASSERTA(error == 0,"socket(%d) last error:(%d)",fd,error);
		int ret = close(fd);
		VASSERTA(ret == 0,"close(%d) error(%d)",fd,errno);
		if(ret == 0)
		{
			connectFree((struct connect_core**)(&event_ptr->data.ptr));
		}
	}else{
		VLOGE("epoll_ctl error(%d)",errno);
	}
}

static void epoll_event_shutdown(struct epoll_event *event_ptr,int howto)
{
	struct connect_core *connect = (struct connect_core*)event_ptr->data.ptr;
	VASSERT(connect != NULL);
	int fd = connect->fd;

	int ret = shutdown(fd,howto);
	if(ret != 0)
	{
		if(errno == ENOTCONN)
		{
		}else{
			errnoDump("shutdown");
			epoll_event_dump(0,event_ptr);
		}
	}else{
		VLOGI("(%d)shutdown success",fd);
	}
}

int epoll_event_add(struct epoll_core * core,struct interface_core * connect)
{
	epoll_event event = {0};
	event.events = EPOLLIN;
	// event.events |= EPOLLONESHOT;
	// event.events |= EPOLLET;
	event.data.ptr = (void*)connect;
	int ret = epoll_ctl(core->epoll,EPOLL_CTL_ADD,connect->fd,&event);
	return ret;
}

int epoll_event_add(struct epoll_core * core,struct connect_core * connect)
{
	epoll_event event = {0};
	event.events = EPOLLIN;
	event.events |= EPOLLONESHOT;
	// event.events |= EPOLLET;
	event.data.ptr = (void*)connect;
	int ret = epoll_ctl(core->epoll,EPOLL_CTL_ADD,connect->fd,&event);
	return ret;
}

void epoll_event_status(struct epoll_core * core,struct epoll_event *event_ptr,int status)
{
	VASSERT(core != NULL);
	struct connect_core *connect = (struct connect_core*)event_ptr->data.ptr;
	VASSERT(connect != NULL);
	if(connect == NULL) return;
	int fd = connect->fd;

	if(status & CONNECT_STATUS_CLOSE)
	{
		// VLOGD("CONNECT_STATUS_CLOSE(%d) events:%x",fd,event_ptr->events);
		epoll_event_shutdown(event_ptr,SHUT_RD);
		epoll_event_close(core,event_ptr);

		// event_ptr->events &= ~EPOLLIN;
		// event_ptr->events &= ~EPOLLOUT;
		// event_ptr->events |= EPOLLRDHUP;
		// int ret = epoll_ctl(core->epoll,EPOLL_CTL_MOD,fd,event_ptr);
		// if(ret == -1)
		// {
		// 	VLOGE("(%d) epoll_ctl error.",fd);
		// }
	}else if(status & CONNECT_STATUS_SEND)
	{
		// VLOGD("CONNECT_STATUS_SEND(%d) events:%x",fd,event_ptr->events);

		event_ptr->events &= ~EPOLLIN;
		event_ptr->events |= EPOLLOUT;
		event_ptr->events |= EPOLLONESHOT;
		int ret = epoll_ctl(core->epoll,EPOLL_CTL_MOD,fd,event_ptr);
		if(ret == -1)
		{
			VLOGE("(%d) epoll_ctl error(%d).",fd,errno);
			epoll_event_dump(EPOLLOUT,event_ptr);
		}
	}else if(status & CONNECT_STATUS_RECV)
	{
		// VLOGD("CONNECT_STATUS_RECV(%d) events:%x",fd,event_ptr->events);

		event_ptr->events &= ~EPOLLOUT;
		event_ptr->events |= EPOLLIN;
		event_ptr->events |= EPOLLONESHOT;
		int ret = epoll_ctl(core->epoll,EPOLL_CTL_MOD,fd,event_ptr);
		if(ret == -1)
		{
			VLOGE("(%d) epoll_ctl error(%d).",fd,errno);
			epoll_event_dump(EPOLLIN,event_ptr);
		}
	}else if(status & CONNECT_STATUS_CONTINUE)
	{
		VLOGD("CONNECT_STATUS_CONTINUE(%d) events:%x",fd,event_ptr->events);
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
				VLOGE("(%d) epoll_ctl error(%d).",fd,errno);
			}
		}
	}
	else{
		VLOGE("Unnown(%d) stasus:%d events:%d error",fd,status,event_ptr->events);
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
	epoll_event_status(core,&event,ret);
	cmpxchgl(&connect->lock,1,0);
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
	epoll_event_status(core,&event,ret);
	cmpxchgl(&connect->lock,1,0);
}

int  epoll_event_server(struct epoll_core * core,struct epoll_event* event_ptr)
{
	VASSERT(core != NULL);
	VASSERT(event_ptr != NULL);
	struct interface_core *interface_ptr = (struct interface_core*)event_ptr->data.ptr;
	VASSERT(interface_ptr != NULL);
	int events = event_ptr->events;
	if((events & EPOLLRDHUP) || (events & EPOLLHUP) || (events & EPOLLERR))
	{
		VLOGE("server is in error.");
	}
	else
	{
		while(1)
		{
			int ret = epoll_event_accept(core,interface_ptr);
			if(ret != 1)
			{
				break;
			}
		}

		if(events & EPOLLONESHOT)
		{
			event_ptr->data.ptr = (void*)interface_ptr;
			int ret = epoll_ctl(core->epoll,EPOLL_CTL_MOD,interface_ptr->fd,event_ptr);
			if(ret != 0)
			{
				errnoDump("epoll_event_server EPOLLONESHOT epoll_ctl");
			}
		}
	}
	return 0;
}

void epoll_event_connect(struct epoll_core * core,struct epoll_event* event_ptr)
{
	VASSERT(core != NULL);
	VASSERT(event_ptr != NULL);
	struct connect_core *connect = (struct connect_core*)event_ptr->data.ptr;
	VASSERT(connect != NULL);
	if(connect == NULL){
		return;
	}
	int events = event_ptr->events;
	// VLOGD("events:%x %p",events,event_ptr->data.ptr);

	connect->events = events;

	if(events & EPOLLRDHUP)
	{
		epoll_event_dump(EPOLLRDHUP,event_ptr);
		//主动请求关闭
		//对端关闭或者关闭写模式,本端关闭读模式
		// epoll_event_shutdown(event_ptr,SHUT_RD);
		epoll_event_close(core,event_ptr);
	}
	else if(events & EPOLLHUP)
	{
		epoll_event_dump(EPOLLHUP,event_ptr);
		//对端关闭或者关闭写模式,本端关闭读模式
		epoll_event_shutdown(event_ptr,SHUT_RD);
		epoll_event_close(core,event_ptr);
	}
	else if(events & EPOLLERR)
	{
		epoll_event_dump(EPOLLERR,event_ptr);
		epoll_event_shutdown(event_ptr,SHUT_RD);
		epoll_event_close(core,event_ptr);
	}
	else if(events & EPOLLIN)
	{
		if(core->pool != NULL){
			if(cmpxchgl(&connect->lock,0,1) == 0)
			{
				uv_queue_work(core->pool,&connect->work,work_read,done_read);
			}
		}else{
			int ret = connectRead(connect);
			epoll_event_status(core,event_ptr,ret);
		}
	}
	else if(events & EPOLLOUT)
	{
		if(core->pool != NULL)
		{
			if(cmpxchgl(&connect->lock,0,1) == 0)
			{
				uv_queue_work(core->pool,&connect->work,work_write,done_write);
			}
		}else{
			int ret = connectWrite(connect);
			epoll_event_status(core,event_ptr,ret);
		}
	}
	else if(events & EPOLLPRI)
	{
		//带外数据
		epoll_event_dump(EPOLLPRI,event_ptr);
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
		VLOGE("unknown events(%x)",events);
		epoll_event_dump(0,event_ptr);
	}
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
	if(n == 0)
	{
		return 0;
	}
	else if(n < 0)
	{
		VLOGE("epoll wait error:%d",errno);
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
			epoll_event_server(core,event_ptr);
		}
		else{
			epoll_event_connect(core,event_ptr);
		}
	}
	return 0;
}
