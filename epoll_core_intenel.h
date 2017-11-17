#ifndef EPOLL_CORE_INTENEL_H
#define EPOLL_CORE_INTENEL_H

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

const char * getEPOLLName(int events);
void errnoDump(const char * name);

#include "epoll_core.h"

extern struct epoll_func epoll_func_intenel;


#endif
