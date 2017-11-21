#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "util/log.h"
#include "util/atomic.h"
#include "connect_core.h"
#include "process_core.h"
#include "module/threadpool.h"

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

typedef struct Config {
	char * ip;
	int port;
	int async_timeout;
	int send_again_timeout;
	int threads;
	long active;
}Config;

int getArgumentInt(int pos,int argc, char* argv[],char * name,int def)
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

char* getArgumentPChar(int pos,int argc, char* argv[],char * name,char* def)
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
		printf("端口错误:%d", port);
		return -1;
	}

	config->ip = ip;
	config->port = port;
	config->threads = getArgumentInt(3,argc,argv,"-threads",1);
	config->async_timeout = getArgumentInt(3,argc,argv,"-async",0);
	config->send_again_timeout = getArgumentInt(3,argc,argv,"-send_again",0);
	config->active = 0;
	return 0;
}

typedef struct TaskManage{
	Config *cfg;
	uv_work_t req;
	uv_threadpool_t * pool;
}TaskManage;

typedef struct Task{
	int sock;
	int ret;
	int error;
	uv_work_t req;
	TaskManage * manage;
}Task;

int CheckConnect(int sock,Config *cfg)
{
	fd_set set;
	FD_ZERO(&set);
	FD_SET(sock, &set);
	//select 检测超时2s
	struct timeval tm;
	tm.tv_sec = 1;
	tm.tv_usec = 0;
	int ret = select(-1, NULL, &set, NULL, &tm);
	if (ret == 0)
	{
		// select错误或者超时
		return -1;
	}
	else if(ret < 0)
	{
		VLOGE("select error:%d",errno);
		return -1;
	}
	else
	{
		int error = -1;
		int optLen = sizeof(int);
		getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error, (socklen_t*)&optLen);

		if (0 != error)
		{
			VLOGE("getsockopt error(%d)", error);
			return -1;
		}
	}
	return 0;
}

int ConnectSocket(Config *cfg)
{
	int sock = socket(AF_INET,SOCK_STREAM,0);
	VASSERT(sock != -1);
	struct sockaddr * addr = createSocketAddr(cfg->ip,cfg->port);
	int ret = connect(sock,addr,GetSockaddrSize(addr));
	VASSERT(ret != -1);
	if(ret != 0)
	{
		ret = CheckConnect(sock,cfg);
	}
	if(ret != 0)
	{
		close(sock);
		free(addr);
		return -1;
	}
	return sock;
}

#define MAX_BUFFER_READ 1024

void read_work(uv_work_t* req)
{
	Task *task = container_of(req,struct Task,req);
	VASSERT(task != NULL);

	char buffer[MAX_BUFFER_READ];
	int ret = send(task->sock, buffer, MAX_BUFFER_READ, 0);
	if(ret == -1)
	{
		if(EAGAIN == ret )
		{
			if(task->manage->cfg->send_again_timeout > 0)
			{
				usleep(task->manage->cfg->send_again_timeout);
			}
		}else{
			task->error = errno;
			task->ret = ret;
		}
	}
}

void read_done(uv_work_t* req, int status)
{
	Task *task = container_of(req,struct Task,req);
	VASSERT(task != NULL);
	VASSERT(task->manage != NULL);

	if(task->ret == 0)
	{
		int ret = uv_queue_work(task->manage->pool,&task->req,read_work,read_done);
		VASSERT(ret == 0);
	}else{
		VASSERT(task->manage != NULL);
		atomic_dec(&task->manage->cfg->active);
		VLOGE("read error.%d",task->error);
		close(task->sock);
		free(task);
	}
}

void create_work(uv_work_t* req)
{
	TaskManage *task = container_of(req,struct TaskManage,req);
	VASSERT(task != NULL);
	VASSERT(task->cfg != NULL);
	VASSERT(task->pool != NULL);

	int sock = ConnectSocket(task->cfg);
	if(sock != -1)
	{
		Task * t = (Task*)malloc(sizeof(Task));
		t->sock = sock;
		t->ret = 0;
		t->error = 0;
		t->manage = task;
		int ret = uv_queue_work(task->pool,&t->req,read_work,read_done);
		VASSERT(ret == 0);
		atomic_inc(&task->cfg->active);
	}else{
		VLOGE("connect error.(%d)",errno);
	}
}

void create_done(uv_work_t* req, int status)
{
	TaskManage *task = container_of(req,struct TaskManage,req);
	VASSERT(task != NULL);
	VASSERT(task->cfg != NULL);
	VASSERT(task->pool != NULL);
	long active = atomic_read(&task->cfg->active);
	if(active < task->cfg->threads)
	{
		int ret = uv_queue_work(task->pool,&task->req,create_work,create_done);
		VASSERT(ret == 0);
	}else{
		VLOGD("task over.%d",atomic_read(&task->cfg->active));
	}
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
	printf("threads:%d\n",cfg.threads);
	printf("async:%d\n",cfg.async_timeout);
	printf("send_again:%d\n",cfg.send_again_timeout);

	uv_async_event_t async;
	uv_async_init(&async);
	uv_threadpool_t * pool = threadpool_create(&async);
	pool->nthreads = 8;

	struct process_core * process = processGetDefault();
	processSignal();

	TaskManage tasks[4] = {0};
	int count = sizeof(tasks)/sizeof(tasks[0]);
	count = min(cfg.threads,count);
	for(int i = 0 ; i < count;i++)
	{
		TaskManage * task = &tasks[i];
		task->cfg = &cfg;
		task->pool = pool;
		uv_queue_work(task->pool,&task->req,create_work,create_done);
	}
	while(!process->signal_term_stop)
	{
		VASSERT(pool != NULL);
		int ret = uv_async_done(&async);
		if(ret <= 0)
		{
			if(cfg.async_timeout > 0)
			{
				usleep(cfg.async_timeout);
			}
		}
	}

	processFree(&process);
	VLOGI("free success.");
	return 0;
}
