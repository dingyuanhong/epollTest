
#include <WinSock2.h>
#include <stdlib.h>
#include <stdio.h>

#include "log.h"
#include "connect_core.h"
#pragma comment(lib,"ws2_32.lib")

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
		printf("¶Ë¿Ú´íÎó:%d",port);
		return 0;
	}

	printf("ip:%s\n",ip);
	printf("port:%d\n",port);

	WORD wVersionRequired = 2;
	WSADATA wsaData = {0};
	wsaData.wVersion = 2;
	wsaData.wHighVersion = 2;
	WSAStartup(wVersionRequired,&wsaData);

	SOCKET sock = socket(AF_INET,SOCK_STREAM,0);
	if (sock == -1)
	{
		VLOGE("socket error(%d)",errno);
		return -1;
	}
	struct sockaddr *addr = createSocketAddr(ip, port);
	VASSERTL("createSocketAddr:",addr != NULL);

	nonBlocking(sock);

	int ret = connect(sock,addr, GetSockaddrSize(addr));
	VASSERTL("connect:",ret != -1);
	if (ret == -1)
	{
		fd_set set;
		FD_ZERO(&set);
		FD_SET(sock,&set);
		//select ¼ì²â³¬Ê±2s
		struct timeval tm;
		tm.tv_sec = 2;
		tm.tv_usec = 0;

		if (select(-1, NULL, &set, NULL, &tm) <= 0)
		{
			// select´íÎó»òÕß³¬Ê±
			VLOGE("select error(%d)", errno);
			closesocket(sock);
			free(addr);
			return -1;
		}
		else
		{
			int error = -1;
			int optLen = sizeof(int);
			getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error, &optLen);

			if (0 != error)
			{
				VLOGE("getsockopt error(%d)", error);
				closesocket(sock);
				free(addr);
				return -1;
			}
		}
	}
	//×èÈûsocket
	int blockMode = 0;
	ioctlsocket(sock, FIONBIO, (u_long FAR*)&blockMode); //ÉèÖÃÎª×èÈûÄ£Ê½ 

	while (true) {
		char buffer[65535];
		ret = send(sock,buffer,65535,0);
		if (ret == SOCKET_ERROR)
		{
			VLOGE("send error(%d)(%d)",ret,errno);
			break;
		}
		else if (ret == 65535)
		{
		
		}
		else
		{
			VLOGW("send not all.(%d)",ret);
		}
	}

	VLOGI("socket colse.");
	shutdown(sock, SD_BOTH);

	closesocket(sock);
	free(addr);

	WSACleanup();
	VLOGI("process colse ok.");
	return 0;
}