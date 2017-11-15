#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <pthread.h>
#include <stdlib.h>

#include "config.h"
#include "log.h"
#include "connect_core.h"
#include "process_core.h"

int main()
{
	processSignal();

	const char * defaultConfigFile = "/config.cfg";
	config_core * config = getDefaultConfig();
	parseConfigFile(defaultConfigFile,config);
	VASSERT(config != NULL);
	configDump(config);

	int epoll = epoll_create(config->max_listen);
	if(epoll == -1){
		VLOGE("epoll create error(%d)\n",errno);
		return -1;
	}

	int server = createServerConnect(config->socket_ptr,config->max_listen);
	if(server == -1)
	{
		return -1;
	}

	struct process_core * process = processGetDefault();
	VASSERT(process != NULL);

	uv_async_event_t async;
	uv_async_init(&async);

	struct epoll_core * epoll_core = process->epoll_ptr;
	epoll_core->epoll = epoll;
	epoll_core->event_count = config->concurrent;
	epoll_core->pool = threadpool_create(&async);
	struct interface_core * connect = interfaceCreate();
	connect->type |= INTERFACE_TYPE_SERVER;
	connect->fd = server;

	int ret = epoll_event_add(epoll_core,connect);
	if(ret != -1)
	{
		VLOGI("server:%d process:%p",connect->fd,process);
		VLOGI("epoll:%d pool:%p",epoll_core->epoll,epoll_core->pool);
		while(!process->signal_term_stop)
		{
			epoll_event_process(process->epoll_ptr,config->timeout);
			uv_async_done(&async);
		}
	}else{
		VLOGE("epill_ctl server error(%d)\n",errno);
	}
	interfaceFree(&connect);

	VLOGI("server stop.");
	processFree(&process);
	configFree(&config);
	return 1;
}
