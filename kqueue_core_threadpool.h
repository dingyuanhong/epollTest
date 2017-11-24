#ifndef KQUEUE_CORE_THREADPOOL_H
#define KQUEUE_CORE_THREADPOOL_H


void kqueue_threadpool_read(struct kqueue_core * core,struct connect_core * conn);
void kqueue_threadpool_write(struct kqueue_core * core,struct connect_core * conn);
void kqueue_threadpool_close(struct kqueue_core * core,struct connect_core * conn);

extern struct kqueue_func kqueue_func_threadpool;

#endif