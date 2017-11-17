#include <sys/epoll.h>
#include "epoll_core_intenel.h"
#include "util/log.h"
#include "connect_core.h"
#include "epoll_core.h"

const char * getEPOLLName(int events)
{
	if(events & EPOLLIN)
	{
		return "EPOLLIN";
	}else if(events & EPOLLOUT)
	{
		return "EPOLLOUT";
	}else if(events & EPOLLERR)
	{
		return "EPOLLERR";
	}else if(events & EPOLLRDHUP)
	{
		return "EPOLLRDHUP";
	}else if(events & EPOLLHUP)
	{
		return "EPOLLHUP";
	}else if(events & EPOLLPRI)
	{
		return "EPOLLPRI";
	}else if(events & EPOLLET)
	{
		return "EPOLLET";
	}else if(events & EPOLLONESHOT)
	{
		return "EPOLLONESHOT";
	}else {
		return "Unknown";
	}
}

void errnoDump(const char * name)
{
	if(errno == 0) return;
	if(EMFILE == errno || ENFILE == errno)
	{
		VLOGE("%s Upper limit of file descriptor(%d)",name,errno);
	}
	else if(ECONNABORTED == errno)
	{
		VLOGE("%s Connect abort(%d)",name,errno);
	}
	else if(ENOBUFS == errno || ENOMEM == errno)
	{
		VLOGE("%s Memeory not enough(%d)",name,errno);
	}
	else if(EPERM == errno)
	{
		VLOGE("%s Firewal prohibited(%d)",name,errno);
	}
	else if(ENOTSOCK == errno)
	{
		VLOGE("%s Descriptor not socket(%d)",name,errno);
	}
	else if(EBADF == errno)
	{
		VLOGE("%s Descriptor not invalid(%d)",name,errno);
	}
	else if(ENOTSOCK == errno)
	{
		VLOGE("%s Not socket(%d)",name,errno);
	}
	else{
		VLOGE("%s,error(%d)",name,errno);
	}
}


void epoll_internel_read(struct epoll_core * epoll,struct connect_core * conn)
{
	int ret = 0;
	do{
		ret = connectRead(conn);
		if((ret & CONNECT_STATUS_CONTINUE)){
			if(conn->events & EPOLLET)
			{
				continue;
			}
		}
		break;
	}while(1);
	epoll_event_status(epoll,conn,ret);
}

void epoll_internel_write(struct epoll_core * epoll,struct connect_core * conn)
{
	int ret = 0;
	do{
		ret = connectWrite(conn);
		if((ret & CONNECT_STATUS_CONTINUE)){
			if(conn->events & EPOLLET)
			{
				continue;
			}
		}
		break;
	}while(1);
	epoll_event_status(epoll,conn,ret);
}

void epoll_internel_close(struct epoll_core * epoll,struct connect_core * conn)
{
	epoll_event_close(epoll,conn);
}

struct epoll_func epoll_func_intenel = {
	epoll_internel_read,
	epoll_internel_write,
	epoll_internel_close
};
