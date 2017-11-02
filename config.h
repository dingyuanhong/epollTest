#ifndef CONFIG_H
#define CONFIG_H

#ifdef WIN32
#include <WinSock2.h>
//#include <Ws2def.h>
#include <Ws2ipdef.h>
#include <Ws2tcpip.h>
#define sa_family_t ADDRESS_FAMILY
#else
#include <sys/socket.h>
#endif

typedef struct config_core{
	int max_listen;					//监听数 
	struct sockaddr * socket_ptr;	//服务端口 
	int timeout;					//超时 
	int concurrent;					//并发数 
}config_core;

config_core *getDefaultConfig();

void parseConfigFile(const char * file,config_core * config);

void configDump(config_core* config);

void configInit(config_core * config);

void configClear(config_core * config);

void configFree(config_core ** config_ptr);

#endif
