#include <errno.h>
#include <stdlib.h>
#ifdef _WIN32
#include <WinSock2.h>
//#include <Ws2def.h>
#include <Ws2ipdef.h>
#include <mstcpip.h>
#define sa_family_t ADDRESS_FAMILY
#define ssize_t int
#define MSG_DONTWAIT 0
#define close closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <error.h>
#endif

#include "util/log.h"
#include "connect_core.h"

void nonBlocking(SOCKET socket)
{
#ifdef _WIN32
	unsigned long flags = 1;
	ioctlsocket(socket, FIONBIO, &flags);
#else
	int flags = fcntl(socket, F_GETFL, 0);
	fcntl(socket, F_SETFL, flags | O_NONBLOCK);
#endif
}

void Reuse(SOCKET socket,int reuse)
{
#ifdef _WIN32
	unsigned long flags = reuse;
	ioctlsocket(socket, SO_REUSEADDR, &flags);
#else
	setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuse, sizeof(reuse));
#endif
}

void cloexec(SOCKET socket)
{
#ifndef _WIN32
	int flags = fcntl(socket, F_GETFD);
	flags |= FD_CLOEXEC;
	fcntl(socket, F_SETFD, flags);
#endif
}

void keepalive(SOCKET socket,int keep)
{
#ifdef _WIN32
	BOOL bSet= (keep != 0);
	int  ret = setsockopt(socket,SOL_SOCKET,SO_KEEPALIVE,(const char*)&bSet,sizeof(BOOL));
	if(ret == 0){
		tcp_keepalive live,liveout;
		live.keepaliveinterval=5000; //每次检测的间隔 （单位毫秒）
		live.keepalivetime=10000;  //第一次开始发送的时间（单位毫秒）
		live.onoff=TRUE;
		DWORD dw = 0;
		//此处显示了在ACE下获取套接字的方法，即句柄的(SOCKET)化就是句柄
		if(WSAIoctl(socket,SIO_KEEPALIVE_VALS,&live,sizeof(live),&liveout,sizeof(liveout),&dw,NULL,NULL) == SOCKET_ERROR)
		{
			VLOGE("WSAIoctl error.");
		}
	}
#else
	int keepAlive = keep;//设定KeepAlive
	int keepIdle = 3;//开始首次KeepAlive探测前的TCP空闭时间
	int keepInterval = 5;//两次KeepAlive探测间的时间间隔
	int keepCount = 3;//判定断开前的KeepAlive探测次数
	if (setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, (void*)&keepAlive, sizeof(keepAlive)) == -1)
	{
		VLOGE("setsockopt SO_KEEPALIVE error.(%d)", errno);
	}
	if (setsockopt(socket, SOL_TCP, TCP_KEEPIDLE, (void *)&keepIdle, sizeof(keepIdle)) == -1)
	{
		VLOGE("setsockopt TCP_KEEPIDLE error.(%d)", errno);
	}
	if (setsockopt(socket, SOL_TCP, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval)) == -1)
	{
		VLOGE("setsockopt TCP_KEEPINTVL error.(%d)", errno);
	}
	if (setsockopt(socket, SOL_TCP, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount)) == -1)
	{
		VLOGE("setsockopt TCP_KEEPCNT error.(%d)", errno);
	}
#endif
}

struct sockaddr *createSocketAddr(const char * ip,int port)
{
	struct sockaddr_in * addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	addr->sin_addr.s_addr = inet_addr(ip);
	return (struct sockaddr *)addr;
}

int GetSockaddrSize(const struct sockaddr *sockaddr)
{
	if(sockaddr == NULL) return 0;
	sa_family_t family = sockaddr->sa_family;
	if(family == AF_INET){
		return sizeof(struct sockaddr_in);
	}
	else if (family == AF_INET6)
	{
		return sizeof(struct sockaddr_in6);
	}
#ifndef WIN32
	else if(family == AF_UNIX)
	{
		return sizeof(struct sockaddr_un);
	}
#endif
	else {
		return 0;
	}
}

SOCKET createServerConnect(const sockaddr* socket_ptr,int max_listen)
{
	SOCKET server = socket(AF_INET,SOCK_STREAM,0);
	if(server == -1)
	{
		VLOGE("socket create error(%d)\n",errno);
		return -1;
	}
	nonBlocking(server);
	Reuse(server,1);

	int ret = bind(server, socket_ptr,GetSockaddrSize(socket_ptr));
	if(ret == -1)
	{
		VLOGE("socket bind error(%d)\n",errno);
		close(server);
		return -1;
	}

	ret = listen(server,max_listen);
	if(ret == -1)
	{
		VLOGE("socket listen error(%d)\n",errno);
		close(server);
		return -1;
	}

	return server;
}

struct interface_core * interfaceCreate()
{
	struct interface_core *core = (struct interface_core*)malloc(sizeof(struct interface_core));
	if(core == NULL) return NULL;
	core->type = 0;
	core->fd = -1;
	return core;
}

void interfaceFree(struct interface_core ** connect_ptr)
{
	if(connect_ptr == NULL) return;
	struct interface_core *core = *connect_ptr;
	if(core == NULL) return;
	if(core->fd != -1)
	{
		close(core->fd);
	}
	free(core);
	*connect_ptr = NULL;
}

struct connect_core * connectCreate()
{
	struct connect_core *core = (struct connect_core*)malloc(sizeof(struct connect_core));
	if(core == NULL) return NULL;
	connectInit(core);
	return core;
}

void connectInit(struct connect_core * connect)
{
	if(connect == NULL) return;
	connect->type = 0;
	connect->fd = -1;
	connect->status = 0;
	connect->error = 0;
	connect->ret = 0;
	connect->lock = 0;
}

void connectClear(struct connect_core * connect)
{
	if(connect == NULL) return;
	if(connect->fd != -1)
	{
		close(connect->fd);
	}
	connect->type = 0;
	connect->fd = -1;
	connect->status = 0;
	connect->error = 0;
	connect->ret = 0;
	connect->ptr = NULL;
}

void connectFree(struct connect_core ** connect_ptr)
{
	if(connect_ptr == NULL) return;
	struct connect_core * core = *connect_ptr;
	if(core != NULL)
	{
		free(core);
	}
	*connect_ptr = NULL;
}

int connectRead(struct connect_core * connect)
{
	if(connect == NULL) return CONNECT_STATUS_CLOSE;
	if(connect->status & CONNECT_STATUS_CLOSE)
	{
		return CONNECT_STATUS_CLOSE;
	}
	int fd = connect->fd;

	char buffer[65535];
	ssize_t ret = recv(fd,buffer,65535,MSG_DONTWAIT);
	if(ret == 0)
	{
		VLOGI("recv close.");
		connect->status |= CONNECT_STATUS_CLOSE;
		return CONNECT_STATUS_CLOSE;
	}else if(ret == -1){
		if(errno == EAGAIN)
		{
			return 0;
		}
		else if(ECONNRESET == errno)
		{
			// VLOGD("recv ECONNRESET socket.");
		}
		else
		if(ENOTSOCK == errno)
		{
			// VLOGD("recv ENOTSOCK socket.");
		}
		else{
			VLOGE("recv(%d) error.(%d)",fd,errno);
		}
		connect->error = errno;
		connect->status |= CONNECT_STATUS_CLOSE;
		return CONNECT_STATUS_CLOSE;
	}else if(ret != 65535)
	{
		// VLOGI("recv data 接受完成.(%d)",ret);
		return CONNECT_STATUS_CONTINUE | CONNECT_STATUS_RECV;
	}
	return CONNECT_STATUS_CONTINUE | CONNECT_STATUS_RECV;
}

int connectWrite(struct connect_core * connect)
{
	if(connect == NULL) return CONNECT_STATUS_CLOSE;
	if(connect->status & CONNECT_STATUS_CLOSE)
	{
		return CONNECT_STATUS_CLOSE;
	}
	int fd = connect->fd;
	char buffer[65535];
	ssize_t ret = send(fd,buffer,65535,MSG_DONTWAIT);
	if(ret <= 0){
		if(EAGAIN == errno)
		{
			return CONNECT_STATUS_SEND;
		}
		else if(ECONNRESET == errno)
		{
			// VLOGD("send ECONNRESET socket.");
		}
		else
		{
			VLOGE("send error.(%d)",errno);
		}
		connect->error = errno;
		connect->status |= CONNECT_STATUS_CLOSE;
		return CONNECT_STATUS_CLOSE;
	}else if(ret != 65535)
	{
		VLOGD("send data 未完全发送.(%d)",ret);
		return CONNECT_STATUS_CONTINUE | CONNECT_STATUS_SEND;
	}
	return 0;
}

int connectGetErrno(struct connect_core * connect)
{
	if(connect == NULL) return 0;
	return connect->error;
}

int connectShutdown(struct connect_core *connect,int howto)
{
	VASSERT(connect != NULL);
	int fd = connect->fd;

	int ret = shutdown(fd,howto);
	if(ret != 0)
	{
		if(errno == ENOTCONN)
		{
			return 0;
		}else{
			VLOGE("shutdown error:%d",errno);
		}
		return -1;
	}else{
		VLOGI("(%d)shutdown success",fd);
	}
	return 0;
}

void connectDump(struct connect_core *connect)
{
	VASSERT(connect != NULL);
	int fd = connect->fd;
	VLOGI("events:%x fd:%d  ptr:%p status:%d",connect->events,fd,connect,connect->status);
}
