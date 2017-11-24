#ifndef EVENT_LOOP_CORE_H
#define EVENT_LOOP_CORE_H

#if defined(__MAC__)
#include "kqueue_core.h"
typedef struct kqueue_core event_loop_core;

#define loopCreate kqueueCreate
#define loopInit kqueueInit
#define loop_coreCreate kqueue_coreCreate
#define loop_coreDelete kqueue_coreDelete
#define loopFree kqueueFree
#define loop_event_prepare kqueue_event_prepare
#define loop_event_process kqueue_event_process
#define loop_event_add kqueue_event_add
#define loop_event_close kqueue_event_close
#define loop_event_delete kqueue_event_delete

#elif defined(__linux__)
#include "epoll_core.h"
typedef struct epoll_core event_loop_core;

#define loopCreate epollCreate
#define loopInit epollInit
#define loop_coreCreate epoll_coreCreate
#define loop_coreDelete epoll_coreDelete
#define loopFree epollFree
#define loop_event_prepare epoll_event_prepare
#define loop_event_process epoll_event_process
#define loop_event_add epoll_event_add
#define loop_event_close epoll_event_close
#define loop_event_delete epoll_event_delete

#endif

#endif