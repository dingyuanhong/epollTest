#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "util/log.h"
#include "util/atomic.h"
#include "util/errno_name.h"
#include "connect_core.h"
#include "process_core.h"
#include "module/threadpool.h"

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

typedef struct Config {
	char * ip;
	int port;
	int nonblock;
	int async_timeout;
	int send_again_timeout;
	int threads;
	long active;
}Config;

int getArgumentInt(int pos,int argc, char* argv[],const char * name,int def)
{
	for(int i = pos ; i < argc;i+=2)
	{
		bool foundcmd = false;
		char * value = argv[i];
		if(strcmp(name,value) == 0)
		{
			foundcmd = true;
		}
		if(i + 1 >= argc) continue;
		if(foundcmd)
		{
			value = argv[i+1];
			int ret = atoi(value);
			return ret;
		}
	}
	return def;
}

char* getArgumentPChar(int pos,int argc, char* argv[],const char * name,char* def)
{
	for(int i = pos ; i < argc;i+=2)
	{
		bool foundcmd = false;
		char * value = argv[i];
		if(strcmp(name,value) == 0)
		{
			foundcmd = true;
		}
		if(i + 1 >= argc) continue;
		if(foundcmd)
		{
			return argv[i+1];
		}
	}
	return def;
}

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
		printf("port(%d) error", port);
		return -1;
	}

	config->ip = ip;
	config->port = port;
	config->nonblock = getArgumentInt(3,argc,argv,"-nonblock",0);
	config->threads = getArgumentInt(3,argc,argv,"-threads",1);
	config->async_timeout = getArgumentInt(3,argc,argv,"-async",0);
	config->send_again_timeout = getArgumentInt(3,argc,argv,"-send_again",0);
	config->active = 0;
	return 0;
}

int CheckConnect(int sock,Config *cfg)
{
	fd_set rset;
	fd_set wset;
	fd_set eset;
	FD_ZERO(&rset);
	FD_ZERO(&wset);
	FD_ZERO(&eset);
	FD_SET(sock, &rset);
	FD_SET(sock, &wset);
	FD_SET(sock, &eset);
	//select 检测超时2s
	struct timeval tm;
	tm.tv_sec = 2;
	tm.tv_usec = 0;
	int ret = select(sock+1, &rset,&wset,&eset,&tm);
	if (ret == 0)
	{
		// select错误或者超时
		VLOGD("select(%d) timeout",sock);
		return -1;
	}
	else if(ret < 0)
	{
		VLOGE("select(%d) errno:%d",sock,errno);
		return -1;
	}
	else
	{
		int error = -1;
		int optLen = sizeof(int);
		int ret = getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error, (socklen_t*)&optLen);
		VASSERT(ret == 0);
		VASSERTE(error);
		if (0 != error)
		{
			VLOGE("getsockopt(%d) errno(%d)",sock, error);
			VASSERTE(error);
			return -1;
		}
	}
	return 0;
}

int ConnectSocket(Config *cfg)
{
	int sock = socket(AF_INET,SOCK_STREAM,0);
	VASSERT(sock != -1);
	if(cfg->nonblock)
	{
		nonBlocking(sock);
	}
	struct sockaddr * addr = createSocketAddr(cfg->ip,cfg->port);
	int ret = connect(sock,addr,GetSockaddrSize(addr));
	VASSERTA(ret != -1,"sock:%d ip:%s port:%d errno:%d result:%d",sock,cfg->ip,cfg->port,errno,ret);
	if(ret != 0)
	{
		VASSERTE(errno);
		if(errno == EINPROGRESS){
			ret = CheckConnect(sock,cfg);
		}
	}
	if(ret != 0)
	{
		close(sock);
		free(addr);
		return -1;
	}
	free(addr);
	return sock;
}

int main(int argc, char* argv[])
{
	Config cfg = {0};
	if (parseConfig(&cfg, argc, argv) != 0)
	{
		return -1;
	}

	printf("ip:%s\n",cfg.ip);
	printf("port:%d\n",cfg.port);

	int handle = ConnectSocket(&cfg);
	if(handle != -1)
	{
		VLOGI("连接成功:%d",handle);
		close(handle);
	}else{
		VLOGI("连接失败");
	}
	return 0;
}
