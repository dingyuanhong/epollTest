#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "thread.h"
#include "../util/queue.h"
#include "../util/once.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define container_of(ptr, type, member) \
  ((type *) ((char *) (ptr) - offsetof(type, member)))

typedef struct uv_async_event_s
{
	QUEUE wq;
	uv_mutex_t wq_mutex;
	void (*complited)(struct uv_async_event_s *w);
}uv_async_event_t;

void uv_async_init(uv_async_event_t *async);

int uv_async_done(uv_async_event_t* handle);

struct uv__work{
	void (*work)(struct uv__work *w);
	void (*done)(struct uv__work *w, int status);
	void* wq[2];	//双向链表
};

typedef struct uv_work_s uv_work_t;
typedef struct uv_threadpool_s uv_threadpool_t;

typedef void (*uv_work_cb)(uv_work_t* req);
typedef void (*uv_after_work_cb)(uv_work_t* req, int status);

typedef struct uv_work_s {
  //UV_REQ_FIELDS
  uv_threadpool_t* pool;
  uv_work_cb work_cb;
  uv_after_work_cb after_work_cb;
  struct uv__work work_req;
}uv_work_t;

typedef struct uv_threadpool_s{
	uv_cross_once_t once;
	uv_cond_t cond;
	uv_mutex_t mutex;
	QUEUE wq;
	uv_async_event_t * async_complete;

	unsigned int nthreads;
	unsigned int idle_threads;
	uv_thread_t* threads;
	uv_thread_t default_threads[4];
	volatile int initialized;
	int stop;
}uv_threadpool_t;

uv_threadpool_t *threadpool_create(uv_async_event_t *async);
int threadpool_stop(uv_threadpool_t *pool);

int uv_work_cancel(uv_threadpool_s* pool, struct uv__work* w);

//入队任务
int uv_queue_work(uv_threadpool_s* pool,
                  uv_work_t* req,
                  uv_work_cb work_cb,
                  uv_after_work_cb after_work_cb);


#endif
