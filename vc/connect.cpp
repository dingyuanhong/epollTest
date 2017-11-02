#include <Windows.h>
//#include <WinSock2.h>

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
	struct sockaddr *addr = createSocketAddr("127.0.0.1",9999);
	VASSERTL("createSocketAddr:",addr != NULL);
	int ret = connect(sock,addr, GetSockaddrSize(addr));
	VASSERTL("connect:",ret != -1);

	while (true) {
		char buffer[65535];
		ret = send(sock,buffer,65535,0);
		if (ret == SOCKET_ERROR)
		{
			VLOGE("send error(%d)",ret);
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
	closesocket(sock);

	WSACleanup();
	VLOGI("process colse ok.");
	return 0;
}