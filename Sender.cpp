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
	config->nonblock = getArgumentInt(3,argc,argv,"-nonblock",1);
	config->threads = getArgumentInt(3,argc,argv,"-threads",1);
	config->async_timeout = getArgumentInt(3,argc,argv,"-async",1);
	config->send_again_timeout = getArgumentInt(3,argc,argv,"-send_again",0);
	config->active = 0;
	return 0;
}

typedef struct TaskManage{
	Config *cfg;
	uv_work_t req;
	uv_threadpool_t * pool;
	int count;
	int id;
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
		VASSERTE(errno);
		return -1;
	}
	else
	{
		int error = -1;
		int optLen = sizeof(int);
		getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error, (socklen_t*)&optLen);

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
	if(ret != 0)
	{
		if(errno == EINPROGRESS){
			ret = CheckConnect(sock,cfg);
		}else
		{
			VASSERTA(ret != -1,"sock:%d ip:%s port:%d errno:%d",sock,cfg->ip,cfg->port,errno);
			VASSERTE(errno);
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

#define MAX_BUFFER_WRITE 1024

void write_work(uv_work_t* req)
{
	Task *task = container_of(req,struct Task,req);
	VASSERT(task != NULL);

	char buffer[MAX_BUFFER_WRITE];
	int ret = send(task->sock, buffer, MAX_BUFFER_WRITE, 0);
	if(ret == -1)
	{
		if(EAGAIN == errno )
		{
			if(task->manage->cfg->send_again_timeout > 0)
			{
				usleep(task->manage->cfg->send_again_timeout);
			}
		}else{
			task->error = errno;
			task->ret = ret;
		}
	}else
	{
		if(ret == 0)
		{
			VLOGW("%d send result:%d errno:%d",task->sock,ret,errno);
		}else
		{
			//VLOGI("%d send result:%d",task->sock,ret);
		}
	}
}

void write_done(uv_work_t* req, int status)
{
	Task *task = container_of(req,struct Task,req);
	VASSERT(task != NULL);
	VASSERT(task->manage != NULL);

	if(task->ret == 0 && status == 0)
	{
		int ret = uv_queue_work(task->manage->pool,&task->req,write_work,write_done);
		VASSERT(ret == 0);
		if(ret == 0) return;
	}
	{
		VASSERT(task->manage != NULL);
		atomic_dec(task->manage->cfg->active);
		VLOGE("write result:%d errno.%d",task->ret,task->error);
		VASSERTE(task->error);
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
		int ret = uv_queue_work(task->pool,&t->req,write_work,write_done);
		VASSERT(ret == 0);
		atomic_inc(task->cfg->active);
	}else{
		VLOGE("ConnectSocket result:%d",sock);
		task->count++;
	}
	
}

void create_done(uv_work_t* req, int status)
{
	TaskManage *task = container_of(req,struct TaskManage,req);
	VASSERT(task != NULL);
	VASSERT(task->cfg != NULL);
	VASSERT(task->pool != NULL);
	long active = atomic_read(task->cfg->active);
	if(active < task->cfg->threads && status == 0)
	{
		int ret = uv_queue_work(task->pool,&task->req,create_work,create_done);
		VASSERT(ret == 0);
	}else{
		VLOGD("task over.%d",atomic_read(task->cfg->active));
	}
	if(task->count > 1000)
	{
		VLOGD("%d current active:%d",task->id,atomic_read(task->cfg->active));
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

	TaskManage tasks[4];
	int count = sizeof(tasks)/sizeof(tasks[0]);
	count = min(cfg.threads,count);
	for(int i = 0 ; i < count;i++)
	{
		TaskManage * task = &tasks[i];
		task->count = 0;
		task->id = i;
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
	VLOGD("thread pool stop...");
	//清理最后资源
	threadpool_stop(pool);
	VLOGD("thread pool stop.");
	uv_async_done(&async);
	VLOGD("thread async stop.");
	processFree(&process);
	VLOGI("free success.");
	return 0;
}
