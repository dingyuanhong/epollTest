#include "util/sysdef.h"
#include "util/log.h"
#include "util/uv_memory.h"
#include "connect_core.h"
#include "kqueue_core.h"

#ifdef __MAC__
#include "util/minmax.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/event.h>
#include <sys/time.h>

#include "kqueue_core_threadpool.h"

struct kqueue_core * kqueueCreate()
{
	struct kqueue_core *core = (struct kqueue_core*)uv_malloc(sizeof(struct kqueue_core));
	kqueueInit(core);
	return core;
}

void kqueueInit(struct kqueue_core * core)
{
	if(core == NULL) return;
	core->pool = NULL;
	core->handle = -1;
	core->events_ptr = NULL;
	core->event_count = 0;
}

int kqueue_coreCreate(struct kqueue_core * core,int concurrent)
{
	core->handle = kqueue();
	VASSERTA(core->handle != -1,"kqueue error:%d",errno);
	return core->handle;
}

void kqueue_coreDelete(struct kqueue_core * core)
{
	VASSERT(core != NULL);
	if(core->handle != -1)
	{
		close(core->handle);
	}
	core->handle = -1;
}

void kqueueFree(struct kqueue_core ** core_ptr)
{
	if(core_ptr == NULL) return;
	struct kqueue_core * core = *core_ptr;
	if(core == NULL) return;
	kqueue_coreDelete(core);
	if(core->events_ptr != NULL)
	{
		uv_free(core->events_ptr);
		core->events_ptr = NULL;
	}
	uv_free(core);
	*core_ptr = NULL;
}

void kqueue_internel_read(struct kqueue_core * core,struct connect_core * conn)
{
	int ret = 0;
	do{
		ret = connectRead(conn);
		if((ret & CONNECT_STATUS_CONTINUE)){
			continue;
		}
		break;
	}while(1);
	kqueue_event_status(core,conn,ret);
}

void kqueue_internel_write(struct kqueue_core * core,struct connect_core * conn)
{
	int ret = 0;
	do{
		ret = connectWrite(conn);
		if((ret & CONNECT_STATUS_CONTINUE)){
			continue;
		}
		break;
	}while(1);
	kqueue_event_status(core,conn,ret);
}

static struct kqueue_func default_conn_func = {
	0,
	kqueue_internel_read,
	kqueue_internel_write,
	0,
};


static int kqueue_dispatch_accept(struct kqueue_core *core,struct interface_core * interface)
{
	if(core->conn_func.accept != NULL)
	{
		core->conn_func.accept(core,interface);
	}else
	{
		return kqueue_event_accept(core,interface);
	}
	return 0;
}

static void kqueue_dispatch_read(struct kqueue_core *core,struct connect_core* connect)
{
	if(core->conn_func.read != NULL)
	{
		core->conn_func.read(core,connect);
	}else
	{
		VLOGD("core->conn_func.read == NULL");
		kqueue_event_delete(core,connect);
	}
}

static void kqueue_dispatch_write(struct kqueue_core *core,struct connect_core* connect)
{
	if(core->conn_func.write != NULL)
	{
		core->conn_func.write(core,connect);
	}else
	{
		VLOGD("core->conn_func.write == NULL");
		kqueue_event_delete(core,connect);
	}
}

static void kqueue_dispatch_delete(struct kqueue_core *core,struct connect_core* connect)
{
	if(core->conn_func.close != NULL)
	{
		core->conn_func.close(core,connect);
	}else
	{
		// VLOGD("core->conn_func.close == NULL");
		kqueue_event_delete(core,connect);
	}
}

void kqueue_event_prepare(struct kqueue_core * core)
{
	VASSERT(core != NULL);
	if(core->pool != NULL)
	{
		core->conn_func = kqueue_func_threadpool;
	}else
	{
		core->conn_func = default_conn_func;
	}
}

int kqueue__accept(struct kqueue_core * core,struct interface_core *interface_ptr)
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
			VLOGE("accept error:%d",errno);
		}
		return -1;
	}
	nonBlocking(connect);
	cloexec(connect);
	keepalive(connect,1);

	struct connect_core * connect_ptr = connectCreate();
	connect_ptr->fd = connect;
	connect_ptr->ptr = (void*)core;

	int ret = kqueue_event_add(core,connect_ptr);
	if(ret == -1)
	{
		VLOGE("accept kqueue_event_add:%d",errno);
		connectFree(&connect_ptr);
		close(connect);
		return -1;
	}else{
		// VLOGD("accept new connect(%d) %p",connect,connect_ptr);
		return 1;
	}
	return 0;
}

int kqueue_event_accept(struct kqueue_core * core,struct interface_core *conn)
{
	VASSERT(core != NULL);
	VASSERT(conn != NULL);
	while(1)
	{
		int ret = kqueue__accept(core,conn);
		if(ret != 1)
		{
			break;
		}
	}
	int events = conn->events;
	int flags = conn->flags;
	if(flags & EV_ONESHOT)
	{
		struct kevent event;
    	EV_SET(&event, conn->fd, EVFILT_READ, EV_ONESHOT|EV_ENABLE, 0, 0, (void*)(intptr_t)conn);
		int ret = kevent(core->handle, &event, 1, NULL, 0, NULL);
		if(ret != 0)
		{
			VLOGE("kevent error:%d",errno);
			return -1;
		}
	}
	return 0;
}

int kqueue_event_add(struct kqueue_core * core,struct interface_core * conn)
{
	struct kevent ev;
    EV_SET(&ev, conn->fd, EVFILT_READ, EV_ADD|EV_ENABLE, 0, 0, (void*)(intptr_t)conn);
	int r = kevent(core->handle, &ev, 1, NULL, 0, NULL);
	return r;
}

int kqueue_event_add(struct kqueue_core * core,struct connect_core * conn)
{
	struct kevent ev;
    EV_SET(&ev, conn->fd, EVFILT_READ, EV_ADD|EV_ENABLE, 0, 0, (void*)(intptr_t)conn);
	int r = kevent(core->handle, &ev, 1, NULL, 0, NULL);
	return r;
}

void kqueue_event_status(struct kqueue_core * core,struct connect_core * conn,int status)
{
	VASSERT(core != NULL);
	VASSERT(conn != NULL);
	if(conn == NULL) return;
	int fd = conn->fd;

	struct kevent event;
	int filter = conn->events;
	int flags = conn->flags;
	int events = filter;

	flags |= EV_ONESHOT;

	if(status & CONNECT_STATUS_CLOSE)
	{
		VLOGD("CONNECT_STATUS_CLOSE(%d) events:%x",fd,events);

		kqueue_dispatch_delete(core,conn);
	}else if(status & CONNECT_STATUS_SEND)
	{
		// VLOGD("CONNECT_STATUS_SEND(%d) events:%x",fd,events);
		events &= ~EVFILT_READ;
		events |= EVFILT_WRITE;

    	EV_SET(&event, conn->fd, events, flags, 0, 0, (void*)(intptr_t)conn);
		int ret = kevent(core->handle, &event, 1, NULL, 0, NULL);
		if(ret == -1)
		{
			VLOGE("(%d) epoll_ctl error(%d).",fd,errno);
			connectDump(conn);
		}
	}else if(status & CONNECT_STATUS_RECV)
	{
		// VLOGD("CONNECT_STATUS_RECV(%d) events:%x",fd,events);

		events &= ~EVFILT_WRITE;
		events |= EVFILT_READ;

		EV_SET(&event, conn->fd, events, flags, 0, 0, (void*)(intptr_t)conn);
		int ret = kevent(core->handle, &event, 1, NULL, 0, NULL);
		if(ret == -1)
		{
			VLOGE("(%d) epoll_ctl error(%d).",fd,errno);
			connectDump(conn);
		}
	}else if(status & CONNECT_STATUS_CONTINUE)
	{
		VLOGD("CONNECT_STATUS_CONTINUE(%d) events:%x",fd,events);
	}else if(status == 0)
	{
		// VLOGD("status == 0 (%d) events:%x",fd,event_ptr->events);
		if(flags & EV_ONESHOT)
		{
			events &= ~EVFILT_WRITE;
			events |= EVFILT_READ;

			EV_SET(&event, conn->fd, events, flags, 0, 0, (void*)(intptr_t)conn);
			int ret = kevent(core->handle, &event, 1, NULL, 0, NULL);
			if(ret == -1)
			{
				VLOGE("(%d) epoll_ctl error(%d).",fd,errno);
			}
		}
	}
	else{
		VLOGE("Unnown(%d) stasus:%d events:%d error",fd,status,events);
	}
}

int kqueue_event_close(struct kqueue_core * core,struct connect_core *conn)
{
	VASSERT(core != NULL);
	VASSERT(conn != NULL);
	int fd = conn->fd;

	struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_DELETE|EV_DISABLE, 0, 0, NULL);
	int ret = kevent(core->handle, &ev, 1, NULL, 0, NULL);
	if(ret == 0)
	{
		int error = connectGetErrno(conn);
		VASSERTA(error == 0,"socket(%d) last error:(%d)",fd,error);
		int ret = close(fd);
		VASSERTA(ret == 0,"close(%d) error(%d)",fd,errno);
		if(ret == 0)
		{
			conn->fd = -1;
		}
		return ret;
	}else
	{
		VLOGE("epoll_ctl error(%d)",errno);
	}
	return -1;
}

int kqueue_event_delete(struct kqueue_core * core,struct connect_core *conn)
{
	VASSERT(conn != NULL);
	int ret = 0;
	if(conn->fd != -1)
	{
		ret = kqueue_event_close(core,conn);
	}
	if(ret == 0)
	{
		connectFree(&conn);
	}
	return ret;
}


static int kqueue_event_server(struct kqueue_core * core,struct kevent *event_ptr)
{
	VASSERT(core != NULL);
	VASSERT(event_ptr != NULL);
	struct interface_core *conn = (struct interface_core*)event_ptr->udata;
	VASSERT(conn != NULL);
	int events = event_ptr->filter;
	int flags = event_ptr->flags;

	if(flags & EV_ERROR)
	{
		VLOGE("server is in error.");
		return -1;
	}
	else
	{
		conn->events = events;
		conn->flags = flags;
		return kqueue_dispatch_accept(core,conn);
	}
	return 0;
}


static void kqueue_event_connect(struct kqueue_core * core,struct kevent *event_ptr)
{
	VASSERT(core != NULL);
	VASSERT(event_ptr != NULL);
	struct connect_core *conn = (struct connect_core*)event_ptr->udata;
	VASSERT(conn != NULL);
	if(conn == NULL){
		return;
	}
	int events = event_ptr->filter;
	int flags = event_ptr->flags;
	// VLOGD("events:%x %p",events,event_ptr->data.ptr);

	conn->events = events;
	conn->flags = flags;

	if(flags & EV_ERROR)
	{
		// VLOGD("EV_ERROR trigger.");
		//连接异常
		// connectDump(conn);
		//连接断开
		kqueue_dispatch_delete(core,conn);
	}
	else if(events & EVFILT_READ)
	{
		kqueue_dispatch_read(core,conn);
	}
	else if(events & EVFILT_WRITE)
	{
		kqueue_dispatch_write(core,conn);
	}
	else{
		VLOGE("unknown events(%x) flags(%d) trigger",events,flags);
		connectDump(conn);
		kqueue_dispatch_delete(core,conn);
	}
}

int kqueue_event_process(struct kqueue_core * core,long timeout)
{
	int event_count = core->event_count;
	struct kevent *events_ptr = core->events_ptr;

	if(events_ptr == NULL)
	{
		event_count = max(1,event_count);
		events_ptr = (struct kevent*)uv_malloc(sizeof(struct kevent)*event_count);
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

	struct timespec timeout_spec;
    timeout_spec.tv_sec = timeout / 1000;
    timeout_spec.tv_nsec = (timeout % 1000) * 1000 * 1000;
	int n = kevent(core->handle, NULL, 0, core->events_ptr, core->event_count, &timeout_spec);
	if(n == 0)
	{
		return 0;
	}
	else if(n < 0)
	{
		VLOGE("kevent wait error:%d",errno);
		return -1;
	}
	// VLOGI("接收到消息:%d",n);
	int accept_event_count = min(n,event_count);
	for(int i = 0 ; i < accept_event_count;i++)
	{
		struct kevent *event_ptr = &events_ptr[i];
		VASSERT(event_ptr != NULL);

		struct interface_core *interface_ptr = (struct interface_core*)event_ptr->udata;
		VASSERT(interface_ptr != NULL);

		if(interface_ptr->type & INTERFACE_TYPE_SERVER)
		{
			kqueue_event_server(core,event_ptr);
		}
		else{
			kqueue_event_connect(core,event_ptr);
		}
	}
	return accept_event_count;
}



#endif