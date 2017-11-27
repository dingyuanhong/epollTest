#ifndef LOG_H
#define LOG_H

#include <errno.h>

#define VLOG_VERBOSE 0x1  //
#define VLOG_DEBUG 0x2	//调试
#define VLOG_INFO 0x4	//信息
#define VLOG_NOTICE 0x8 //通知
#define VLOG_WARN 0x10	//警告
#define VLOG_ALERT 0x20  //警报
#define VLOG_ERROR 0x40  //错误
#define VLOG_ASSERT 0x80 //断言
#define VLOG_EMERG 0x100 //紧急
#define VLOG_CRIT 0x200	//非常严重

void VVprintf(const char * file,const char * func,int line,int level,const char* format,...);
void VVprintf(int level,const char* format,...);
void VLOG_SetLevel(int level);
int VLOG_GetLevel();

#define VLOG(L,A,...)  VVprintf(__FILE__,__func__,__LINE__,L,A"\n",##__VA_ARGS__)
// #define VLOG(L,A,...)  VVprintf(L,A"\n",##__VA_ARGS__)

#define VLOGV(A,...) VLOG(VLOG_VERBOSE,A,##__VA_ARGS__)
#define VLOGD(A,...) VLOG(VLOG_DEBUG,A,##__VA_ARGS__)
#define VLOGI(A,...) VLOG(VLOG_INFO,A,##__VA_ARGS__)
#define VLOGW(A,...) VLOG(VLOG_WARN,A,##__VA_ARGS__)
#define VLOGN(A,...) VLOG(VLOG_NOTICE,A,##__VA_ARGS__)
#define VLOGE(A,...) VLOG(VLOG_ERROR,A,##__VA_ARGS__)
#define VLOGA(A,...) VLOG(VLOG_ASSERT,A,##__VA_ARGS__)
#define VLOGC(A,...) VLOG(VLOG_CRIT,A,##__VA_ARGS__)

#define VALERT(A,...) VLOG(VLOG_ALERT,A,##__VA_ARGS__)

#define VASSERT(_exp) if((!(_exp))) VLOG(VLOG_ASSERT, #_exp);
#define VASSERTL(A,_exp) if((!(_exp))) VLOG(VLOG_ASSERT, A " " #_exp);
#define VASSERTR(_exp,A) if((!(_exp))) VLOG(VLOG_ASSERT, #_exp " " A);
#define VASSERTA(_exp,A,...) if((!(_exp))) VLOG(VLOG_ASSERT,"(" #_exp ") " A,##__VA_ARGS__);

#define VWARN(_exp) if((!(_exp))) VLOG(VLOG_WARN,#_exp);
#define VWARNL(A,_exp) if((!(_exp))) VLOG(VLOG_WARN,A " " #_exp);
#define VWARNR(_exp,A) if((!(_exp))) VLOG(VLOG_WARN,#_exp " " A);
#define VWARNA(_exp,A,...) if((!(_exp))) VLOG(VLOG_WARN,"(" #_exp ") " A,##__VA_ARGS__);

#define VABORT() VLOGE("abort");abort();

#endif
