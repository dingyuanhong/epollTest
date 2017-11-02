#ifndef CONFIG_H
#define CONFIG_H

#include <sys/socket.h>

typedef struct config_core{
	int max_listen;					//监听数
	struct sockaddr * socket_ptr; 		//服务器端口
	int timeout;					//超时
	int concurrent;					//并发数
}config_core;

config_core *getDefaultConfig();

void parseConfigFile(const char * file,config_core * config);

void configDump(config_core* config);

void configInit(config_core * config);

void configClear(config_core * config);

void configFree(config_core ** config_ptr);

int GetSockaddrSize(struct sockaddr *sockaddr);

#endif
