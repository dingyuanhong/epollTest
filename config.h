#ifndef CONFIG_H
#define CONFIG_H

#ifdef _WIN32
#include <WinSock2.h>
//#include <Ws2def.h>
#include <Ws2ipdef.h>
#include <Ws2tcpip.h>
#define sa_family_t ADDRESS_FAMILY
#else
#include <sys/socket.h>
#endif

typedef struct config_core_s
{
	int max_listen;
	struct sockaddr * socket_ptr;
	int timeout;
	int concurrent;
	int threadpool;
}config_core;

config_core *getDefaultConfig();

void parseConfigFile(const char * file,config_core * config);

void configDump(config_core* config);

void configInit(config_core * config);

void configClear(config_core * config);

void configFree(config_core ** config_ptr);

#endif
