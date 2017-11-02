#ifndef CONNECT_H
#define CONNECT_H

#include <netinet/in.h>
#include "config.h"

struct interface_core
{
	int type;
	int fd;
};

struct connect_core
{
	int type;
	int fd;
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


struct sockaddr *createSocket(const char * ip,int port);
int GetSockaddrSize(struct sockaddr *sockaddr);

int createServerConnect(struct config_core *config);

struct connect_core * connectCreate();
void connectInit(struct connect_core * connect);
void connectClear(struct connect_core * connect);
void connectFree(struct connect_core ** connect_ptr);

int connectRead(int fd,struct connect_core * connect);
int connectWrite(int fd,struct connect_core * connect);
int connectGetErrno(struct connect_core * connect);

#endif
