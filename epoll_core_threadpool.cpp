#include "util/sysdef.h"
#include "util/log.h"
#include "util/atomic.h"

#include "connect_core.h"
#include "epoll_core_intenel.h"
#include "epoll_core_threadpool.h"

#ifdef __linux__
#include <sys/epoll.h>
static void work_accept(uv_work_t* req)
{
	//uv_work_t* req = container_of(w, uv_work_t, work_req);
	struct interface_core *connect = container_of(req,struct interface_core,work);
	VASSERT(connect != NULL);
	struct epoll_core * core = (struct epoll_core *)connect->ptr;
	VASSERT(core != NULL);
	while(1)
	{
		int ret = epoll__accept(core,connect);
		if(ret != 1)
		{
			break;
		}
	}
	int events = connect->events;
	if(events & EPOLLONESHOT)
	{
		struct epoll_event event;
		event.data.ptr = (void*)connect;
		event.events = connect->events;
		cmpxchgl(&connect->lock,1,0);
		int ret = epoll_ctl(core->handle,EPOLL_CTL_MOD,connect->fd,&event);
		if(ret != 0)
		{
			VLOGE("epoll_ctl error.(%d)",errno);
		}
	}else{
		cmpxchgl(&connect->lock,1,0);
	}
}

static void work_read(uv_work_t* req)
{
	//uv_work_t* req = container_of(w, uv_work_t, work_req);
	struct connect_core *connect = container_of(req,struct connect_core,work);
	VASSERT(connect != NULL);
	struct epoll_core * core = (struct epoll_core *)connect->ptr;
	VASSERT(core != NULL);
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
	cmpxchgl(&connect->lock,1,0);
	epoll_event_status(core,connect,ret);
}

static void work_write(uv_work_t* req)
{
	//uv_work_t* req = container_of(w, uv_work_t, work_req);
	struct connect_core *connect = container_of(req,struct connect_core,work);
	VASSERT(connect != NULL);
	struct epoll_core * core = (struct epoll_core *)connect->ptr;
	VASSERT(core != NULL);
	int ret = 0;
	while(true)
	{
		ret = connectWrite(connect);
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
	cmpxchgl(&connect->lock,1,0);
	epoll_event_status(core,connect,ret);
}

static void work_close(uv_work_t* req)
{
	struct connect_core *connect = container_of(req,struct connect_core,work);
	VASSERT(connect != NULL);
	struct epoll_core * core = (struct epoll_core *)connect->ptr;
	VASSERT(core != NULL);
}

static void work_done(uv_work_t* req, int status)
{
	//uv_work_t* req = container_of(w, uv_work_t, work_req);
	struct connect_core *connect = container_of(req,struct connect_core,work);
	VASSERT(connect != NULL);
	struct epoll_core * core = (struct epoll_core *)connect->ptr;
	VASSERT(core != NULL);

	int ret = connect->ret;
	cmpxchgl(&connect->lock,1,0);
	epoll_event_status(core,connect,ret);
}

static void close_done(uv_work_t* req, int status)
{
	//uv_work_t* req = container_of(w, uv_work_t, work_req);
	struct connect_core *connect = container_of(req,struct connect_core,work);
	VASSERT(connect != NULL);
	struct epoll_core * core = (struct epoll_core *)connect->ptr;
	VASSERT(core != NULL);

	int ret = epoll_event_delete(core,connect);
	VASSERT(ret == 0);
	if(ret != 0)
	{
		cmpxchgl(&connect->lock,1,0);
		epoll_threadpool_close(core,connect);
	}
}

void epoll_threadpool_accept(struct epoll_core * core,struct interface_core * conn)
{
	if(cmpxchgl(&conn->lock,0,1) == 0)
	{
		uv_queue_work(core->pool,&conn->work,work_accept,NULL);
	}else{
		// VLOGD("accept task is running.");
	}
}

void epoll_threadpool_read(struct epoll_core * core,struct connect_core * conn)
{
	if(cmpxchgl(&conn->lock,0,1) == 0)
	{
		uv_queue_work(core->pool,&conn->work,work_read,NULL);
	}else{
		VLOGD("read task is running.");
	}
}

void epoll_threadpool_write(struct epoll_core * core,struct connect_core * conn)
{
	if(cmpxchgl(&conn->lock,0,1) == 0)
	{
		uv_queue_work(core->pool,&conn->work,work_write,NULL);
	}else{
		VLOGD("write task is running.");
	}
}

void epoll_threadpool_close(struct epoll_core * core,struct connect_core * conn)
{
	if(cmpxchgl(&conn->lock,0,1) == 0)
	{
		uv_queue_work(core->pool,&conn->work,work_close,close_done);
	}else{
		VLOGD("close task is running.");
	}
	//epoll_event_delete(core,conn);
}

struct epoll_func epoll_func_threadpool = {
	epoll_threadpool_accept,
	epoll_threadpool_read,
	epoll_threadpool_write,
	NULL
};
#endif
