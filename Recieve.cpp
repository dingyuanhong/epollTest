#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <pthread.h>
#include <stdlib.h>

#include "util/log.h"
#include "config.h"
#include "connect_core.h"
#include "process_core.h"
#include "event_loop_core.h"

int main()
{
	processSignal();

	const char * defaultConfigFile = "/config.cfg";
	config_core * config = getDefaultConfig();
	parseConfigFile(defaultConfigFile,config);
	VASSERT(config != NULL);
	configDump(config);

	int server = createServerConnect(config->socket_ptr,config->max_listen);
	if(server == -1)
	{
		return -1;
	}

	struct process_core * process = processGetDefault();
	VASSERT(process != NULL);

	uv_async_event_t async;
	uv_async_init(&async);

	event_loop_core * loop_core = loopCreate();;
	loop_coreCreate(loop_core,config->max_listen);
	loop_core->event_count = config->concurrent;
	if(config->threadpool)
	{
		loop_core->pool = threadpool_create(&async);
	}

	processSet(process,loop_core);

	struct interface_core * connect = interfaceCreate();
	connect->type |= INTERFACE_TYPE_SERVER;
	connect->fd = server;
	connect->ptr = (void*)loop_core;
	loop_event_prepare(loop_core);

	int ret = loop_event_add(loop_core,connect);
	if(ret != -1)
	{
		VLOGI("server:%d process:%p",connect->fd,process);
		VLOGI("loop:%d pool:%p",loop_core->handle,loop_core->pool);
		while(!process->signal_term_stop)
		{
			loop_event_process(loop_core,config->timeout);
			uv_async_done(&async);
		}
	}else{
		VLOGE("epill_ctl server error(%d)\n",errno);
	}
	interfaceFree(&connect);

	VLOGI("server stop.");
	loop_coreDelete(loop_core);
	loopFree(&loop_core);
	processFree(&process);
	configFree(&config);
	return 1;
}
