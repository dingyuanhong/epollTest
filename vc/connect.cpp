
#include <WinSock2.h>

#include "log.h"
#include "connect_core.h"
#pragma comment(lib,"ws2_32.lib")

int main()
{
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
	struct sockaddr *addr = createSocketAddr("10.0.2.4",9999);
	VASSERTL("createSocketAddr:",addr != NULL);

	nonBlocking(sock);

	int ret = connect(sock,addr, GetSockaddrSize(addr));
	VASSERTL("connect:",ret != -1);
	if (ret == -1)
	{
		fd_set set;
		FD_ZERO(&set);
		FD_SET(sock,&set);
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