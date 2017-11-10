
#include <WinSock2.h>
#include <stdlib.h>
#include <stdio.h>

#include "log.h"
#include "connect_core.h"
#include "taskConfig.h"
#include <errors.h>

#pragma comment(lib,"ws2_32.lib")

int ConnectTest(const char * ip, int port)
{
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1)
	{
		VLOGE("socket error(%d)", errno);
		return -1;
	}
	struct sockaddr *addr = createSocketAddr(ip, port);
	VASSERTL("createSocketAddr:", addr != NULL);

	nonBlocking(sock);
	int ret = connect(sock, addr, GetSockaddrSize(addr));
	int errorno = WSAGetLastError();
	VASSERT(WSAETIMEDOUT == errorno);
	VASSERTL("connect:", ret != -1);
	if (ret == -1)
	{
		fd_set set;
		FD_ZERO(&set);
		FD_SET(sock, &set);
		//select 检测超时2s
		struct timeval tm;
		tm.tv_sec = 2;
		tm.tv_usec = 0;

		if (select(-1, NULL, &set, NULL, &tm) <= 0)
		{
			// select错误或者超时
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
	//阻塞socket
	int blockMode = 0;
	ioctlsocket(sock, FIONBIO, (u_long FAR*)&blockMode); //设置为阻塞模式 

#define  MAX_BUFFER_READ  65535

	while (true) {
		char buffer[MAX_BUFFER_READ];
		ret = send(sock, buffer, MAX_BUFFER_READ, 0);
		if (ret == SOCKET_ERROR)
		{
			int errorno = WSAGetLastError();
			VLOGE("send error(%d)(%d)", ret, errorno);
			break;
		}
		else if (ret == MAX_BUFFER_READ)
		{

		}
		else
		{
			VLOGW("send not all.(%d)", ret);
		}
	}

	VLOGI("socket colse.");
	shutdown(sock, SD_BOTH);

	closesocket(sock);
	free(addr);

	return 0;
}


int Task(Config * cfg)
{
	while (TRUE)
	{
		int ret = ConnectTest(cfg->ip, cfg->port);
		if (ret != 0)
		{
			VLOGE("任务执行失败.");
			Sleep(1);
		}
	}
	return 0;
}