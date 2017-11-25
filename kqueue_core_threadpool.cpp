#include "util/sysdef.h"
#include "util/log.h"
#include "util/atomic.h"
#include "connect_core.h"
#include "module/threadpool.h"
#include "kqueue_core.h"
#include "kqueue_core_threadpool.h"

#ifdef __MAC__
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/event.h>
#include <sys/time.h>

static void work_accept(uv_work_t* req)
{
	//uv_work_t* req = container_of(w, uv_work_t, work_req);
	struct interface_core *conn = container_of(req,struct interface_core,work);
	VASSERT(conn != NULL);
	struct kqueue_core * core = (struct kqueue_core *)conn->ptr;
	VASSERT(core != NULL);
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
    	cmpxchgl(&conn->lock,1,0);
		int ret = kevent(core->handle, &event, 1, NULL, 0, NULL);
		if(ret != 0)
		{
			VLOGE("kevent errno:%d",errno);
		}
	}else{
		cmpxchgl(&conn->lock,1,0);
	}
}

static void work_read(uv_work_t* req)
{
	//uv_work_t* req = container_of(w, uv_work_t, work_req);
	struct connect_core *connect = container_of(req,struct connect_core,work);
	VASSERT(connect != NULL);
	struct kqueue_core * core = (struct kqueue_core *)connect->ptr;
	VASSERT(core != NULL);
	int ret = 0;
	while(true)
	{
		ret = connectRead(connect);
		if(ret == CONNECT_STATUS_CONTINUE)
		{
			continue;
		}
		break;
	}
	connect->ret = ret;
	cmpxchgl(&connect->lock,1,0);
	kqueue_event_status(core,connect,ret);
}

static void work_write(uv_work_t* req)
{
	//uv_work_t* req = container_of(w, uv_work_t, work_req);
	struct connect_core *connect = container_of(req,struct connect_core,work);
	VASSERT(connect != NULL);
	struct kqueue_core * core = (struct kqueue_core *)connect->ptr;
	VASSERT(core != NULL);
	int ret = 0;
	while(true)
	{
		ret = connectWrite(connect);
		if(ret == CONNECT_STATUS_CONTINUE)
		{
			continue;
		}
		break;
	}
	connect->ret = ret;
	cmpxchgl(&connect->lock,1,0);
	kqueue_event_status(core,connect,ret);
}

static void work_close(uv_work_t* req)
{
	struct connect_core *connect = container_of(req,struct connect_core,work);
	VASSERT(connect != NULL);
	struct kqueue_core * core = (struct kqueue_core *)connect->ptr;
	VASSERT(core != NULL);
}

static void work_done(uv_work_t* req, int status)
{
	//uv_work_t* req = container_of(w, uv_work_t, work_req);
	struct connect_core *connect = container_of(req,struct connect_core,work);
	VASSERT(connect != NULL);
	struct kqueue_core * core = (struct kqueue_core *)connect->ptr;
	VASSERT(core != NULL);

	int ret = connect->ret;
	cmpxchgl(&connect->lock,1,0);
	kqueue_event_status(core,connect,ret);
}

static void close_done(uv_work_t* req, int status)
{
	//uv_work_t* req = container_of(w, uv_work_t, work_req);
	struct connect_core *connect = container_of(req,struct connect_core,work);
	VASSERT(connect != NULL);
	struct kqueue_core * core = (struct kqueue_core *)connect->ptr;
	VASSERT(core != NULL);

	int ret = kqueue_event_delete(core,connect);
	VASSERT(ret == 0);
	if(ret != 0)
	{
		cmpxchgl(&connect->lock,1,0);
		kqueue_threadpool_close(core,connect);
	}
}

void kqueue_threadpool_accept(struct kqueue_core * core,struct interface_core * conn)
{
	if(cmpxchgl(&conn->lock,0,1) == 0)
	{
		uv_queue_work(core->pool,&conn->work,work_accept,NULL);
	}else{
		// VLOGD("accept task is running.");
	}
}

void kqueue_threadpool_read(struct kqueue_core * core,struct connect_core * conn)
{
	if(cmpxchgl(&conn->lock,0,1) == 0)
	{
		uv_queue_work(core->pool,&conn->work,work_read,NULL);
	}else{
		VLOGD("read task is running.");
	}
}

void kqueue_threadpool_write(struct kqueue_core * core,struct connect_core * conn)
{
	if(cmpxchgl(&conn->lock,0,1) == 0)
	{
		uv_queue_work(core->pool,&conn->work,work_write,NULL);
	}else{
		VLOGD("write task is running.");
	}
}

void kqueue_threadpool_close(struct kqueue_core * core,struct connect_core * conn)
{
	if(cmpxchgl(&conn->lock,0,1) == 0)
	{
		uv_queue_work(core->pool,&conn->work,work_close,close_done);
	}else{
		VLOGD("close task is running.");
	}
	//kqueue_event_delete(core,conn);
}

struct kqueue_func kqueue_func_threadpool = {
	kqueue_threadpool_accept,
	kqueue_threadpool_read,
	kqueue_threadpool_write,
	NULL
};
#endif
