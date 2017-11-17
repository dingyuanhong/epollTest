#include <WinSock2.h>
#include <stdio.h>
#include <windows.h>
#include "taskConfig.h"
#include "util/log.h"

int parseConfig(Config *config, int argc, char* argv[])
{
	if (argc < 3)
	{
		printf("command: ip port.\n");
		return -1;
	}
	char * ip = argv[1];
	int port = atoi(argv[2]);
	if (port <= 0) {
		printf("端口错误:%d", port);
		return -1;
	}

	int threadCount = 1;
	if (argc >= 4)
	{
		threadCount = atoi(argv[3]);
		if (threadCount <= 0)
		{
			printf("线程数错误:%d", threadCount);
			return -1;
		}
	}

	config->ip = ip;
	config->port = port;
	config->threadCount = threadCount;
	config->active = 0;
	return 0;
}

void dumpConfig(Config *config)
{
	if (config == NULL) return;
	printf("ip:%s\n", config->ip);
	printf("port:%d\n", config->port);
	printf("thread:%d\n", config->threadCount);
}
