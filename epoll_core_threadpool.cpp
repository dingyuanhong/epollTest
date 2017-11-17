#include <sys/epoll.h>
#include "epoll_core_threadpool.h"
#include "util/log.h"
#include "util/atomic.h"
#include "connect_core.h"
#include "epoll_core_intenel.h"

static void work_read(uv_work_t* req)
{
	//uv_work_t* req = container_of(w, uv_work_t, work_req);
	struct connect_core *connect = container_of(req,struct connect_core,work);
	VASSERT(connect != NULL);
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

static void work_write(uv_work_t* req)
{
	//uv_work_t* req = container_of(w, uv_work_t, work_req);
	struct connect_core *connect = container_of(req,struct connect_core,work);
	VASSERT(connect != NULL);
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
}

static void work_close(uv_work_t* req)
{
	struct connect_core *connect = container_of(req,struct connect_core,work);
	VASSERT(connect != NULL);
	struct epoll_core * epoll = (struct epoll_core *)connect->ptr;
	VASSERT(epoll != NULL);
}

static void work_done(uv_work_t* req, int status)
{
	//uv_work_t* req = container_of(w, uv_work_t, work_req);
	struct connect_core *connect = container_of(req,struct connect_core,work);
	VASSERT(connect != NULL);
	struct epoll_core * epoll = (struct epoll_core *)connect->ptr;
	VASSERT(epoll != NULL);

	int ret = connect->ret;
	epoll_event_status(epoll,connect,ret);
	cmpxchgl(&connect->lock,1,0);
}

static void close_done(uv_work_t* req, int status)
{
	//uv_work_t* req = container_of(w, uv_work_t, work_req);
	struct connect_core *connect = container_of(req,struct connect_core,work);
	VASSERT(connect != NULL);
	struct epoll_core * epoll = (struct epoll_core *)connect->ptr;
	VASSERT(epoll != NULL);

	int ret = epoll_event_delete(epoll,connect);
	VASSERT(ret == 0);
	if(ret != 0)
	{
		cmpxchgl(&connect->lock,1,0);
		epoll_threadpool_close(epoll,connect);
	}
}

void epoll_threadpool_read(struct epoll_core * epoll,struct connect_core * conn)
{
	if(cmpxchgl(&conn->lock,0,1) == 0)
	{
		uv_queue_work(epoll->pool,&conn->work,work_read,work_done);
	}else{
		VLOGD("read task is running.");
	}
}

void epoll_threadpool_write(struct epoll_core * epoll,struct connect_core * conn)
{
	if(cmpxchgl(&conn->lock,0,1) == 0)
	{
		uv_queue_work(epoll->pool,&conn->work,work_write,work_done);
	}else{
		VLOGD("write task is running.");
	}
}

void epoll_threadpool_close(struct epoll_core * epoll,struct connect_core * conn)
{
	if(cmpxchgl(&conn->lock,0,1) == 0)
	{
		uv_queue_work(epoll->pool,&conn->work,work_close,close_done);
	}else{
		VLOGD("close task is running.");
	}
	//epoll_event_close(epoll,conn);
}

struct epoll_func epoll_func_threadpool = {
	epoll_threadpool_read,
	epoll_threadpool_write,
	epoll_threadpool_close
};
