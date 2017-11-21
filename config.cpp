#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/un.h>
#include <arpa/inet.h>
#endif

#include "util/log.h"
#include "config.h"
#include "connect_core.h"

static config_core  static_config;
static config_core * static_config_ptr;

config_core *getDefaultConfig()
{
	if(static_config_ptr == NULL )
	{
		configInit(&static_config);
		static_config_ptr = &static_config;
	}
	return static_config_ptr;
}

config_core *configCreate()
{
	config_core * core = (config_core*)malloc(sizeof(config_core));
	configInit(core);
	return core;
}

void configInit(config_core * config)
{
	if(config == NULL) return;
	memset(config,0,sizeof(config_core));
	config->max_listen = 1024*1024;

	if(config->socket_ptr != NULL)
	{
		free(config->socket_ptr);
		config->socket_ptr = NULL;
	}
	config->socket_ptr = createSocketAddr("0.0.0.0",9999);

	config->timeout = 1;
	config->concurrent = 65535;
	config->threadpool = 1;
}

void parseConfigFile(const char * file,config_core * config)
{
	if(config == NULL) return;
}

void configDump(config_core* config)
{
	if(config == NULL) return;
	VLOGI("listen数:%d",config->max_listen);
	VLOGI("epoll wait超时:%d",config->timeout);
	VLOGI("并发数:%d",config->concurrent);
	sa_family_t family = config->socket_ptr->sa_family;
	if(family == AF_INET){
		struct sockaddr_in * addr = (struct sockaddr_in*)config->socket_ptr;
		VLOGI("IP地址:%s",inet_ntoa(addr->sin_addr));
		VLOGI("IP端口:%d",ntohs(addr->sin_port));
	}else if(family == AF_INET6)
	{
		struct sockaddr_in6* addr = (struct sockaddr_in6*)config->socket_ptr;
		char ip[128] = {0};
		const char *ip_ptr = inet_ntop(AF_INET6, (void *)&addr->sin6_addr, ip, 128);
		VLOGI("IP地址:%s",ip_ptr);
		VLOGI("IP端口:%lld",ntohs(addr->sin6_port));
	}
#ifndef WIN32
	else if(family == AF_UNIX)
	{
		struct sockaddr_un * addr = (struct sockaddr_un*)config->socket_ptr;
		VLOGI("UNIX地址:%s",addr->sun_path);
	}
#endif
}

void configClear(config_core * config)
{
	if(config != NULL){
		if(config->socket_ptr != NULL)
		{
			free(config->socket_ptr);
			config->socket_ptr = NULL;
		}
		config->max_listen = 0;
		config->timeout = 0;
		config->concurrent = 0;
	}
}

void configFree(config_core ** config_ptr)
{
	if(config_ptr == NULL) return;
	config_core *config = *config_ptr;
	if(config != NULL){
		if(config->socket_ptr != NULL)
		{
			free(config->socket_ptr);
			config->socket_ptr = NULL;
		}
	}
	*config_ptr = NULL;
}
