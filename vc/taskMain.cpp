#include <WinSock2.h>
#include <stdio.h>
#include <windows.h>

#include "log.h"
#include "connect_core.h"
#include "taskConfig.h"

#pragma comment(lib,"ws2_32.lib")

DWORD WINAPI ThreadConnect(void* param)
{
	Config * cfg = (Config*)param;
	return Task(cfg);
}

int main(int argc, char* argv[])
{
	Config cfg;
	if (parseConfig(&cfg, argc, argv) != 0)
	{
		return -1;
	}
	dumpConfig(&cfg);

	WORD wVersionRequired = 2;
	WSADATA wsaData = { 0 };
	wsaData.wVersion = 2;
	wsaData.wHighVersion = 2;
	WSAStartup(wVersionRequired, &wsaData);

	if (cfg.threadCount == 1)
	{
		Task(&cfg);
	}
	else {
		VASSERT(cfg.threadCount > 1);
		HANDLE *handleTable = (HANDLE*)malloc(sizeof(HANDLE)*cfg.threadCount);
		memset(handleTable,0, sizeof(HANDLE)*cfg.threadCount);
		for (int i = 0; i < cfg.threadCount; i++)
		{
			HANDLE hThread = CreateThread(NULL, 0, ThreadConnect, &cfg, 0, NULL);
			handleTable[i] = hThread;
			VASSERT(hThread != NULL);
		}
		LONG lastActive = 0;
#define MAX_WAIT_COUNT 10
		int maxCount = MAX_WAIT_COUNT;
		while (maxCount > 0)
		{
			LONG active = InterlockedExchangeAdd(&cfg.active,0);
			if (lastActive != active)
			{
				printf("存活数:%d\n", cfg.active);
				lastActive = active;
				maxCount = MAX_WAIT_COUNT;
			}
			else {
				maxCount--;
			}
			Sleep(1000);
		}
		printf("稳定存活数:%d\n", lastActive);

		VWARNA(cfg.threadCount <= MAXIMUM_WAIT_OBJECTS,"线程数:%d 单次等待最大值:%d.", cfg.threadCount,MAXIMUM_WAIT_OBJECTS);

		int waitCount = cfg.threadCount;
		int waitedCount = 0;
		while (waitCount > 0)
		{
			if (waitCount == 1)
			{
				WaitForSingleObject(*(handleTable + waitedCount),INFINITE);
				waitedCount++;
				waitCount--;
			}
			else {
				int needWait = min(waitCount, MAXIMUM_WAIT_OBJECTS);
				WaitForMultipleObjects(needWait, handleTable + waitedCount, TRUE, INFINITE);
				waitCount -= needWait;
				waitedCount += needWait;
			}
		}

		for (int i = 0; i < cfg.threadCount; i++)
		{
			VASSERT(handleTable[i] != NULL);
			if (handleTable[i] != NULL)
			{
				CloseHandle(handleTable[i]);
			}
			handleTable[i] = NULL;
		}
		free(handleTable);
	}

	WSACleanup();
	VLOGI("process colse ok.");

	return 0;
}