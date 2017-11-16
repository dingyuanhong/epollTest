#ifndef CONNECT_H
#define CONNECT_H

#ifndef _WIN32
#include <netinet/in.h>
#define SOCKET int
#endif

#include "threadpool.h"

void nonBlocking(SOCKET socket);
void Reuse(SOCKET socket,int reuse);
void cloexec(SOCKET socket);
void keepalive(SOCKET socket,int keep);

struct sockaddr *createSocketAddr(const char * ip,int port);
int GetSockaddrSize(const struct sockaddr *sockaddr);
SOCKET createServerConnect(const sockaddr* socket_ptr,int max_listen);

struct interface_core
{
	int type;
	SOCKET fd;
};

struct connect_core
{
	int type;
	SOCKET fd;

	uv_work_t work;
	int events;
	void * ptr;
	long lock;

	int status;
	int error;
	int ret;
};

#define INTERFACE_TYPE_SERVER 0x1

struct interface_core * interfaceCreate();
void interfaceFree(struct interface_core ** connect_ptr);

#define CONNECT_STATUS_SEND 0x1
#define CONNECT_STATUS_RECV 0x2
#define CONNECT_STATUS_CLOSE 0x4
#define CONNECT_STATUS_CONTINUE 0x8

struct connect_core * connectCreate();
void connectInit(struct connect_core * connect);
void connectClear(struct connect_core * connect);
void connectFree(struct connect_core ** connect_ptr);

int connectRead(struct connect_core * connect);
int connectWrite(struct connect_core * connect);
int connectGetErrno(struct connect_core * connect);

#endif
