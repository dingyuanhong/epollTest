#ifndef FATAL_H
#define FATAL_H
#ifdef _WIN32

#include "log.h"

#define uv_fatal_error(errorno, syscall) { \
	char* buf = NULL;						\
	const char* errmsg = NULL;				\
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |			\
		FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorno,										\
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, NULL);					\
	if (buf) errmsg = buf;		\
	else errmsg = "Unknown error"; \
	if (syscall) VLOGE("%s: (%d) %s", syscall, errorno, errmsg); \
	else VLOGE("(%d) %s", errorno, errmsg);  \
	if (buf) LocalFree(buf); \
	*((char*)NULL) = 0xff; \
	VABORT(); \
}	\


static int uv_translate_sys_error(int sys_errno) {
	if (sys_errno <= 0) {
		return sys_errno;  /* If < 0 then it's already a libuv error. */
	}
	return sys_errno;
}


#endif
#endif