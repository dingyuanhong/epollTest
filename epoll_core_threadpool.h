#ifndef EPOLL_CORE_THREADPOOL_H
#define EPOLL_CORE_THREADPOOL_H


void epoll_threadpool_read(struct epoll_core * epoll,struct connect_core * conn);
void epoll_threadpool_write(struct epoll_core * epoll,struct connect_core * conn);
void epoll_threadpool_close(struct epoll_core * epoll,struct connect_core * conn);

extern struct epoll_func epoll_func_threadpool;

#endif
