#ifndef ONCE_H
#define ONCE_H

#include "atomic.h"

typedef struct uv_cross_once_s {
  unsigned char ran;
  ATOMIC_TYPE event;
} uv_cross_once_t;

#define UV_CROSS_ONCE_INIT {0,0}

void uv_cross_once(uv_cross_once_t* guard, void (*callback)(void));

void uv_cross_once(uv_cross_once_t* guard, void (*callback)(void* param),void * param);

#endif
