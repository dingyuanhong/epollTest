#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "log.h"

static int static_log_level = 0xFFFFFFFF;

void VLOG_SetLevel(int level)
{
	static_log_level = level;
}

int VLOG_GetLevel()
{
	return static_log_level;
}

static const char *LOG_LEVEL_NAME[]=
{
	"VERSOBE",
	"DEBUG",
	"INFO",
	"WARN",
	"ERROR",
	"ASSERT",
	"EMERG",
	"ALERT",
	"CRIT",
	"NOTICE"
};

static int LOG_LEVEL_NAME_[]=
{
	VLOG_VERBOSE,
	0,
	VLOG_DEBUG,
	1,
	VLOG_INFO,
	2,
	VLOG_WARN,
	3,
	VLOG_ERROR,
	4,
	VLOG_ASSERT,
	5,
	VLOG_EMERG,
	6,
	VLOG_ALERT,
#ifndef _WIN32
	7,
	VLOG_CRIT,
#endif
	8,
	VLOG_NOTICE,
	9
};

static const char * GetLevelString(int level)
{
	size_t length = sizeof(LOG_LEVEL_NAME_)/LOG_LEVEL_NAME_[0];
	for(size_t i = 0 ; i < length / 2;i++)
	{
		if(LOG_LEVEL_NAME_[2*i] & level)
		{
			return LOG_LEVEL_NAME[LOG_LEVEL_NAME_[2*i+1]];
		}
	}
	return "";
}

void VVprintf(const char * file,const char * func,int line,int level,const char* format,...)
{
	if((level&static_log_level) == 0) return;
	time_t now;
	time(&now);
	struct tm *time = localtime(&now);
	printf("%d%02d%02d-%02d:%02d:%02d ",time->tm_year + 1900,time->tm_mon + 1,time->tm_mday,time->tm_hour,time->tm_min,time->tm_sec);
	if(file != NULL && func != NULL){
		printf("(%s %s %d) ",file,func,line);
	}
	printf("%s:",GetLevelString(level));
	va_list list;
   	va_start(list,format);
	vprintf(format,list);
	va_end(list);
}

void VVprintf(int level,const char* format,...)
{
	if((level&static_log_level) == 0) return;
	time_t now;
	time(&now);
	struct tm *time = localtime(&now);
	printf("%d%02d%02d-%02d:%02d:%02d ",time->tm_year + 1900,time->tm_mon + 1,time->tm_mday,time->tm_hour,time->tm_min,time->tm_sec);
	printf("%s:",GetLevelString(level));
	va_list list;
   	va_start(list,format);
	vprintf(format,list);
	va_end(list);
}
