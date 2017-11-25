
#ifndef ERRORNAME_MAC_H
#define ERRORNAME_MAC_H

#include "log.h"
#include <errno.h>

#define ERRNO_LIST \
N(EPERM)	\
N(ENOENT)	\
N(ESRCH)	\
N(EINTR)	\
N(EIO)	\
N(ENXIO)	\
N(E2BIG)	\
N(ENOEXEC)	\
N(EBADF)	\
N(ECHILD)	\
N(EDEADLK)	\
N(ENOMEM)	\
N(EACCES)	\
N(EFAULT)	\
N(ENOTBLK)	\
N(EBUSY)	\
N(EEXIST)	\
N(EXDEV)	\
N(ENODEV)	\
N(ENOTDIR)	\
N(EISDIR)	\
N(EINVAL)	\
N(ENFILE)	\
N(EMFILE)	\
N(ENOTTY)	\
N(ETXTBSY)	\
N(EFBIG)	\
N(ENOSPC)	\
N(ESPIPE)	\
N(EROFS)	\
N(EMLINK)	\
N(EPIPE)	\
N(EDOM)		\
N(ERANGE)	\
N(EAGAIN)	\
N(EWOULDBLOCK)	\
N(EINPROGRESS)	\
N(EALREADY)	\
N(ENOTSOCK)	\
N(EDESTADDRREQ)	\
N(EMSGSIZE)	\
N(EPROTOTYPE)	\
N(ENOPROTOOPT)	\
N(EPROTONOSUPPORT) \
N(ESOCKTNOSUPPORT) \
N(ENOTSUP)	\
N(EOPNOTSUPP)\
N(EPFNOSUPPORT)	\
N(EAFNOSUPPORT)\
N(EADDRINUSE)\
N(EADDRNOTAVAIL)	\
N(ENETDOWN)	\
N(ENETUNREACH)	\
N(ENETRESET)	\
N(ECONNABORTED)	\
N(ECONNRESET)	\
N(ENOBUFS)	\
N(EISCONN)	\
N(ENOTCONN)	\
N(ESHUTDOWN)	\
N(ETOOMANYREFS)	\
N(ECONNREFUSED)	\
N(ELOOP)	\
N(ENAMETOOLONG)	\
N(EHOSTDOWN) 	\
N(EHOSTUNREACH)	\
N(ENOTEMPTY)	\
N(EPROCLIM)	\
N(EUSERS)	\
N(EDQUOT)	\
N(ESTALE)	\
N(EREMOTE)	\
N(EBADRPC)	\
N(ERPCMISMATCH)	\
N(EPROGUNAVAIL)	\
N(EPROGMISMATCH)	\
N(EPROCUNAVAIL)	\
N(ENOLCK)	\
N(ENOSYS)	\
N(EFTYPE)	\
N(EAUTH)	\
N(ENEEDAUTH)	\
N(EPWROFF)	\
N(EDEVERR)	\
N(EOVERFLOW)	\
N(EBADEXEC)	\
N(EBADARCH)	\
N(ESHLIBVERS)	\
N(EBADMACHO)	\
N(ECANCELED)	\
N(EIDRM)	\
N(ENOMSG)	\
N(EILSEQ)	\
N(EBADMSG)	\
N(EMULTIHOP)	\
N(ENODATA)	\
N(ENOLINK)	\
N(ENOSR)	\
N(ENOSTR)	\
N(EPROTO)	\
N(ETIME)	\
N(EOPNOTSUPP)	\
N(ENOPOLICY)	\
N(ENOTRECOVERABLE)	\
N(EOWNERDEAD)	\
N(EQFULL)	\
N(ELAST)

#define N(A) #A,
static char* static_errno_name_list[] = {
	"",
	ERRNO_LIST
};
#undef N

#define errnoName(A) static_errno_name_list[(A)]

#define ASSERTE(A) if((A) != 0)VLOGA("errno==%d %s",(A),errnoName((A)));

#undef ERRNO_LIST
#endif