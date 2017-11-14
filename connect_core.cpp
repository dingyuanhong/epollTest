#include <errno.h>
#include <stdlib.h>
#ifdef _WIN32
#include <WinSock2.h>
//#include <Ws2def.h>
#include <Ws2ipdef.h>
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
#include <error.h>
#endif

#include "log.h"
#include "connect_core.h"

void nonBlocking(SOCKET socket)
{
#ifdef WIN32
	unsigned long flags = 1;
	ioctlsocket(socket, FIONBIO, &flags);
#else
	int flags = fcntl(socket, F_GETFL, 0);
	fcntl(socket, F_SETFL, flags | O_NONBLOCK);
#endif
}

void Reuse(SOCKET socket,int reuse)
{
#ifdef WIN32
	unsigned long flags = reuse;
	ioctlsocket(socket, SO_REUSEADDR, &flags);
#else
	setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuse, sizeof(reuse));
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

int connectRead(int fd,struct connect_core * connect)
{
	if(connect == NULL) return CONNECT_STATUS_CLOSE;
	if(connect->status & CONNECT_STATUS_CLOSE)
	{
		return CONNECT_STATUS_CLOSE;
	}
	char buffer[65535];
	ssize_t ret = recv(fd,buffer,65535,MSG_DONTWAIT);
	if(ret == 0)
	{
		VLOGI("recv close.");
		connect->status |= CONNECT_STATUS_CLOSE;
		return CONNECT_STATUS_CLOSE;
	}else if(ret == -1){
		if(ECONNRESET == errno)
		{
			return CONNECT_STATUS_CLOSE;
		}
		else
		if(errno == EAGAIN)
		{
			VLOGD("recv EAGAIN 缓冲区已无数据可读.");
			return 0;
		}else{
			VLOGE("recv error.(%d)",errno);
			connect->error = errno;
			connect->status |= CONNECT_STATUS_CLOSE;
			return CONNECT_STATUS_CLOSE;
		}
	}else if(ret != 65535)
	{
		// VLOGI("recv data 接受完成.(%d)",ret);
		return 0;
	}
	return CONNECT_STATUS_CONTINUE;
}

int connectWrite(int fd,struct connect_core * connect)
{
	if(connect == NULL) return CONNECT_STATUS_CLOSE;
	if(connect->status & CONNECT_STATUS_CLOSE)
	{
		return CONNECT_STATUS_CLOSE;
	}

	char buffer[65535];
	ssize_t ret = send(fd,buffer,65535,MSG_DONTWAIT);
	if(ret <= 0){
		if(ECONNRESET == errno)
		{
			return CONNECT_STATUS_CLOSE;
		}
		else
		{
			VLOGE("send error.(%d)",errno);
			connect->error = errno;
			connect->status |= CONNECT_STATUS_CLOSE;
			return CONNECT_STATUS_CLOSE;
		}
	}else if(ret != 65535)
	{
		VLOGD("send data 未完全发送.(%d)",ret);
	}
	return CONNECT_STATUS_CONTINUE;
}

int connectGetErrno(struct connect_core * connect)
{
	if(connect == NULL) return 0;
	return connect->error;
}
