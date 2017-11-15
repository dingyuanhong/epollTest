#include "threadpool.h"
#include "log.h"
#include "uv_memory.h"

static QUEUE exit_message;
static void uv__cancelled(struct uv__work* w) {
	abort();
}

void uv_async_done(uv_async_event_t* handle) {
	struct uv__work* w;
	QUEUE* q;
	QUEUE wq;
	int err;

	uv_mutex_lock(&handle->wq_mutex);
	QUEUE_MOVE(&handle->wq, &wq);
	uv_mutex_unlock(&handle->wq_mutex);

	while (!QUEUE_EMPTY(&wq)) {
		q = QUEUE_HEAD(&wq);
		QUEUE_REMOVE(q);

		w = container_of(q, struct uv__work, wq);
		err = (w->work == uv__cancelled) ? UV_ECANCELED : 0;
		w->done(w, err);
	}
}

void uv_async_complited(uv_async_event_t * async,struct uv__work* w)
{
	if(async == NULL)
	{
		VLOGE("uv_threadpool_s->async_complete is NULL.");
		return;
	}
	uv_mutex_lock(&async->wq_mutex);
	if(w->work != uv__cancelled){
		w->work = NULL;  /* Signal uv_cancel() that the work req is done executing. */
	}
	QUEUE_INSERT_TAIL(&async->wq, &w->wq);
	uv_mutex_unlock(&async->wq_mutex);
	if(async->complited != NULL) async->complited(async);
}

int uv_work_cancel(uv_threadpool_s* pool, struct uv__work* w) {
	int cancelled;
	uv_async_event_t * async = pool->async_complete;
	uv_mutex_lock(&pool->mutex);
	uv_mutex_lock(&async->wq_mutex);

	cancelled = !QUEUE_EMPTY(&w->wq) && w->work != NULL && 	w->work != uv__cancelled;
	if (cancelled){
		QUEUE_REMOVE(&w->wq);
		w->work = uv__cancelled;
	}

	uv_mutex_unlock(&async->wq_mutex);
	uv_mutex_unlock(&pool->mutex);

	if (!cancelled)
		return UV_EBUSY;

	uv_async_complited(async,w);
	return 0;
}

void uv_async_init(uv_async_event_t *async)
{
	QUEUE_INIT(&async->wq);
	uv_mutex_init(&async->wq_mutex);
	async->complited = NULL;
}

static void worker(void* arg) {
	struct uv__work* w;
	QUEUE* q;

	uv_threadpool_t * pool = (uv_threadpool_t*) arg;
	if(pool == NULL) return;

	for (;;) {
		uv_mutex_lock(&pool->mutex);

		while (QUEUE_EMPTY(&pool->wq)) {
			pool->idle_threads += 1;
			uv_cond_wait(&pool->cond, &pool->mutex);
			pool->idle_threads -= 1;
		}

		q = QUEUE_HEAD(&pool->wq);

		if (q == &exit_message)
			uv_cond_signal(&pool->cond);
		else {
			QUEUE_REMOVE(q);
			QUEUE_INIT(q);  /* Signal uv_cancel() that the work req is executing. */
		}

		uv_mutex_unlock(&pool->mutex);

		if (q == &exit_message)
			break;

		w = QUEUE_DATA(q, struct uv__work, wq);
		w->work(w);

		uv_async_complited(pool->async_complete,w);
	}
}

static void uv__queue_work(struct uv__work* w) {
	uv_work_t* req = container_of(w, uv_work_t, work_req);
	req->work_cb(req);
}

static void uv__queue_done(struct uv__work* w, int err) {
	uv_work_t* req;
	req = container_of(w, uv_work_t, work_req);
	if (req->after_work_cb == NULL)
		return;
	req->after_work_cb(req, err);
}

static void post(uv_threadpool_s* pool,QUEUE* q) {
	uv_mutex_lock(&pool->mutex);
	QUEUE_INSERT_TAIL(&pool->wq, q);
	if (pool->idle_threads > 0)
		uv_cond_signal(&pool->cond);
	uv_mutex_unlock(&pool->mutex);
}

#define MAX_THREADPOOL_SIZE 128

static void init_once(void* param) {
	unsigned int i;
	const char* val;

	uv_threadpool_s* pool = (uv_threadpool_s*)param;
	VASSERT(pool != NULL);
	if(pool == NULL) return;

	if (uv_cond_init(&pool->cond))
	{
		VLOGE("cond init error.");
		return;
	}

	if (uv_mutex_init(&pool->mutex))
	{
		VLOGE("mutex init error.");
		return;
	}

	QUEUE_INIT(&pool->wq);

	if(pool->nthreads == 0)
	{
		pool->nthreads = ARRAY_SIZE(pool->default_threads);
		val = getenv("UV_THREADPOOL_SIZE");
		if (val != NULL)
			pool->nthreads = atoi(val);
	}
	if (pool->nthreads == 0)
		pool->nthreads = 1;
	if (pool->nthreads > MAX_THREADPOOL_SIZE)
		pool->nthreads = MAX_THREADPOOL_SIZE;

	pool->threads = pool->default_threads;
	if (pool->nthreads > ARRAY_SIZE(pool->default_threads)) {
		pool->threads = (uv_thread_t*)uv_malloc(pool->nthreads * sizeof(pool->threads[0]));
		if (pool->threads == NULL) {
			pool->nthreads = ARRAY_SIZE(pool->default_threads);
			pool->threads = pool->default_threads;
		}
	}

	for (i = 0; i < pool->nthreads; i++)
	if (uv_thread_create(pool->threads + i, worker, pool))
	{
		VLOGE("uv_thread_create error.");
		abort();
	}

	pool->initialized = 1;
}

void uv__work_submit(uv_threadpool_s* pool,
                     struct uv__work* w,
                     void (*work)(struct uv__work* w),
                     void (*done)(struct uv__work* w, int status)) {
	uv_cross_once(&pool->once, init_once,pool);
	w->work = work;
	w->done = done;
	post(pool,&w->wq);
}

int uv_queue_work(uv_threadpool_s* pool,
                  uv_work_t* req,
                  uv_work_cb work_cb,
                  uv_after_work_cb after_work_cb) {
	if (work_cb == 0)
		return UV_EINVAL;

	req->work_cb = work_cb;
	req->after_work_cb = after_work_cb;
	uv__work_submit(pool, &req->work_req, uv__queue_work, uv__queue_done);
	return 0;
}

uv_threadpool_t *threadpool_create(uv_async_event_t *async)
{
	uv_threadpool_t * pool = (uv_threadpool_t*)uv_malloc(sizeof(uv_threadpool_t));
	if(pool == NULL) return pool;
	uv_memset(pool,0,sizeof(uv_threadpool_t));

	pool->once = UV_CROSS_ONCE_INIT;
	pool->async_complete = async;
	return pool;
}

int threadpool_stop(uv_threadpool_t *pool)
{
	post(pool,&exit_message);
	return 0;
}
