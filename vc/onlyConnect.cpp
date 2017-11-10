#include <WinSock2.h>
#include <stdio.h>
#include <windows.h>

#include "log.h"
#include "connect_core.h"
#include "taskConfig.h"

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
	//VASSERTL("connect:", ret != -1);
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
			//VLOGE("select error(%d)", errno);
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
				//VLOGE("getsockopt error(%d)", error);
				closesocket(sock);
				free(addr);
				return -1;
			}
		}
	}
	//阻塞socket
	int blockMode = 0;
	ioctlsocket(sock, FIONBIO, (u_long FAR*)&blockMode); //设置为阻塞模式 

#define  MAX_BUFFER_READ  1024

	while (true) {
		//Sleep(1000);
		char buffer[MAX_BUFFER_READ];
		ret = send(sock, buffer, MAX_BUFFER_READ, 0);
		if (ret == SOCKET_ERROR)
		{
			int errorno = WSAGetLastError();
			if (WSAECONNRESET == errorno)
			{
				break;
			}
			else
			{
				VLOGE("(%d)send error(%d)(%d)", sock, ret, errorno);
				break;
			}
		}
		else if (ret == MAX_BUFFER_READ)
		{

		}
		else
		{
			VLOGW("send not all.(%d)", ret);
		}
	}

	//VLOGI("socket colse.");
	shutdown(sock, SD_BOTH);

	closesocket(sock);
	free(addr);

	return 0;
}

#define RETRY_TIME 5

int Task(Config * cfg)
{
	InterlockedIncrement(&cfg->active);
	int retryCount = 0;
	int runCount = RETRY_TIME;
	while (runCount > 0)
	{
		int ret = ConnectTest(cfg->ip,cfg->port);
		if (ret != 0)
		{
			//Sleep(1);
			retryCount++;
		}
		else
		{
			break;
		}
		runCount--;
	}
	if (retryCount > 0 && retryCount != RETRY_TIME) {
		VLOGD("重试连接成功:%d", retryCount);
	}
	else if(retryCount == RETRY_TIME){
		//VLOGE("连接一直失败.");
	}
	InterlockedDecrement(&cfg->active);
	return 0;
}

