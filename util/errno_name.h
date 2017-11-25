#ifndef ERRNO_NAME_H
#define ERRNO_NAME_H

#include "sysdef.h"

#ifdef __MAC__
#include "errno_name_mac.h"
#elif defined __linux__
#include "errno_name_linux.h"
#elif defined _WIN32
#include "errno_name_win32.h"
#endif

struct errno_name {
	int err;
	const char * name;
};

inline const char* errnoName(int error) {
#define N(A) {A,#A},
	static struct errno_name static_errno_name_list[] = {
		ERRNO_LIST
	};
#undef N

	for (int i = 0; i < sizeof(static_errno_name_list) / sizeof(static_errno_name_list[0]); i++)
	{
		if (error == static_errno_name_list[i].err)
		{
			return static_errno_name_list[i].name;
		}
	}
	return "";
}

#define VASSERTE(A) if((A) != 0)VLOGA("errno==%d %s",(A),errnoName((A)));

inline void VASSERTEA(int error)
{
#define N(A) if(A==error)VLOGA("errno:%d %s",(A),#A);
	ERRNO_LIST;
#undef N
}

inline void print_errnos() {
#define N(A) VLOGA("errno:%d %s",(A),#A);
	ERRNO_LIST;
#undef N
}

#undef ERRNO_LIST
#endif
