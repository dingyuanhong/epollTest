#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "util/log.h"
#include "connect_core.h"
#include "process_core.h"

int main(int argc, char* argv[])
{
	if(argc < 3)
	{
		printf("command: ip port.\n");
		return 0;
	}
	char * ip = argv[1];
	int port = atoi(argv[2]);
	if (port <= 0) {
		printf("端口错误:%d",port);
		return 0;
	}

	printf("ip:%s\n",ip);
	printf("port:%d\n",port);

	int sock = socket(AF_INET,SOCK_STREAM,0);
	VASSERT(sock != -1);
	struct sockaddr * addr = createSocketAddr(ip,port);
	int ret = connect(sock,addr,GetSockaddrSize(addr));
	VASSERT(ret != -1);
	VLOGI("connect success.");

	struct process_core * process = processGetDefault();
	processSignal();
	// int index = 0;
	while(!process->signal_term_stop)
	{
		char buffer[65535];
		ssize_t len = send(sock,buffer,65535,0);
		if(len != 65535)
		{
			VLOGE("send length:%d",len);
			VLOGE("send error(%d)",errno);
			break;
		}
		// VLOGI("%d 轮",index++);
		// usleep(1);
	}
	VLOGI("shutdown ...");
	ret = shutdown(sock,SHUT_RDWR);
	VASSERT(ret != -1);
	VLOGI("shutdown success.");

	VLOGI("stop success.");
	close(sock);
	VASSERT(sock != -1);
	sock = -1;
	processFree(&process);
	VLOGI("free success.");
	return 0;
}
