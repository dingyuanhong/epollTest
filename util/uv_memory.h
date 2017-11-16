#ifndef UV_MEMORY_H
#define UV_MEMORY_H

#include <stdlib.h>
#include <string.h>

#define uv_malloc malloc
#define uv_free free
#define uv_calloc calloc
#define uv_realloc realloc
#define uv_memset memset

#define UV_EINVAL -1
#define UV_EBUSY -2
#define UV_ECANCELED -3

#define UV_ENOMEM -4
#define UV_EACCES -5
#define UV_EAGAIN -6
#define UV_EIO -7

#define UV_ETIMEDOUT -8

#endif
