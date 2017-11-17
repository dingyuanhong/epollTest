#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>

#include "util/log.h"
#include "config.h"
#include "epoll_core.h"
#include "connect_core.h"
#include "process_core.h"
#include "epoll_core_intenel.h"
#include "epoll_core_threadpool.h"

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

void epoll_event_prepare(struct epoll_core * core)
{
	VASSERT(core != NULL);
	if(core->pool == NULL)
	{
		core->conn_func = epoll_func_intenel;
	}else{
		core->conn_func = epoll_func_threadpool;
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
	event.events |= EPOLLRDHUP; //检测对端正常关闭
	event.events |= EPOLLONESHOT;
	// event.events |= EPOLLET;
	event.data.ptr = (void*)connect;
	int ret = epoll_ctl(core->epoll,EPOLL_CTL_ADD,connect->fd,&event);
	return ret;
}

int epoll_event_accept(struct epoll_core * core,struct interface_core *interface_ptr)
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

void epoll_event_close(struct epoll_core * core,struct connect_core *connect)
{
	VASSERT(core != NULL);
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
			connectFree((struct connect_core**)(&connect));
		}
	}else{
		VLOGE("epoll_ctl error(%d)",errno);
	}
}

void epoll_event_status(struct epoll_core * core,struct connect_core *connect,int status)
{
	VASSERT(core != NULL);
	VASSERT(connect != NULL);
	if(connect == NULL) return;
	int fd = connect->fd;

	struct epoll_event event;
	event.data.ptr = (void*)connect;
	event.events = connect->events;
	struct epoll_event * event_ptr = &event;
	event_ptr->events |= EPOLLONESHOT;
	event_ptr->events |= EPOLLRDHUP; //检测对端正常关闭

	if(status & CONNECT_STATUS_CLOSE)
	{
		// VLOGD("CONNECT_STATUS_CLOSE(%d) events:%x",fd,event_ptr->events);
		connectShutdown(connect,SHUT_RD);
		epoll_event_close(core,connect);
	}else if(status & CONNECT_STATUS_SEND)
	{
		// VLOGD("CONNECT_STATUS_SEND(%d) events:%x",fd,event_ptr->events);

		event_ptr->events &= ~EPOLLIN;
		event_ptr->events |= EPOLLOUT;
		int ret = epoll_ctl(core->epoll,EPOLL_CTL_MOD,fd,event_ptr);
		if(ret == -1)
		{
			VLOGE("(%d) epoll_ctl error(%d).",fd,errno);
			connectDump(connect);
		}
	}else if(status & CONNECT_STATUS_RECV)
	{
		// VLOGD("CONNECT_STATUS_RECV(%d) events:%x",fd,event_ptr->events);

		event_ptr->events &= ~EPOLLOUT;
		event_ptr->events |= EPOLLIN;
		int ret = epoll_ctl(core->epoll,EPOLL_CTL_MOD,fd,event_ptr);
		if(ret == -1)
		{
			VLOGE("(%d) epoll_ctl error(%d).",fd,errno);
			connectDump(connect);
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
		VLOGD("EPOLLRDHUP trigger.");
		connectDump(connect);
		//主动请求关闭
		if(core->conn_func.close != NULL)
		{
			core->conn_func.close(core,connect);
		}else
		{
			VLOGE("core->conn_func.close == NULL");
			epoll_event_close(core,connect);
		}
	}
	else if(events & EPOLLHUP)
	{
		VLOGD("EPOLLHUP trigger.");
		connectDump(connect);
		//连接断开
		if(core->conn_func.close != NULL)
		{
			core->conn_func.close(core,connect);
		}else
		{
			VLOGE("core->conn_func.close == NULL");
			epoll_event_close(core,connect);
		}
	}
	else if(events & EPOLLERR)
	{
		VLOGD("EPOLLERR trigger.");
		//连接异常
		connectDump(connect);
		//连接断开
		if(core->conn_func.close != NULL)
		{
			core->conn_func.close(core,connect);
		}else
		{
			VLOGE("core->conn_func.close == NULL");
			epoll_event_close(core,connect);
		}
	}
	else if(events & EPOLLIN)
	{
		if(core->conn_func.read != NULL)
		{
			core->conn_func.read(core,connect);
		}else
		{
			VLOGE("core->conn_func.read == NULL");
			epoll_event_close(core,connect);
		}
	}
	else if(events & EPOLLOUT)
	{
		if(core->conn_func.write != NULL)
		{
			core->conn_func.write(core,connect);
		}else
		{
			VLOGE("core->conn_func.write == NULL");
			epoll_event_close(core,connect);
		}
	}
	else if(events & EPOLLPRI)
	{
		VLOGD("EPOLLET trigger.");
		//带外数据
		connectDump(connect);
	}
	else if(events & EPOLLET)
	{
		VLOGD("EPOLLET trigger.");
		//边缘触发
		connectDump(connect);
	}
	// else if(events & EPOLLNVAL)
	// {
	// 	VLOGD("EPOLLNVAL trigger.");
	// 	//文件描述符未打开
	// 	connectDump(EPOLLNVAL,connect);
	// }
	else{
		VLOGE("unknown events(%x) trigger",events);
		connectDump(connect);
		epoll_event_close(core,connect);
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
	int accept_event_count = min(n,event_count);
	for(int i = 0 ; i < accept_event_count; i++)
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
	return accept_event_count;
}
