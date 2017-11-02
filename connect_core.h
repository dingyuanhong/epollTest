#ifndef CONNECT_H
#define CONNECT_H

#ifndef _WIN32
#include <netinet/in.h>
#define SOCKET int
#endif

void nonBlocking(SOCKET socket);

struct interface_core
{
	int type;
	SOCKET fd;
};

struct connect_core
{
	int type;
	SOCKET fd;
	int status;
	int error;
};

#define INTERFACE_TYPE_SERVER 0x1

struct interface_core * interfaceCreate();
void interfaceFree(struct interface_core ** connect_ptr);

#define CONNECT_STATUS_SEND 0x1
#define CONNECT_STATUS_RECV 0x2
#define CONNECT_STATUS_CLOSE 0x4
#define CONNECT_STATUS_CONTINUE 0x8


struct sockaddr *createSocketAddr(const char * ip,int port);
int GetSockaddrSize(const struct sockaddr *sockaddr);

SOCKET createServerConnect(const sockaddr* socket_ptr,int max_listen);

struct connect_core * connectCreate();
void connectInit(struct connect_core * connect);
void connectClear(struct connect_core * connect);
void connectFree(struct connect_core ** connect_ptr);

int connectRead(int fd,struct connect_core * connect);
int connectWrite(int fd,struct connect_core * connect);
int connectGetErrno(struct connect_core * connect);

#endif
