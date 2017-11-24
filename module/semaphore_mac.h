#ifndef SEMAPHORE_MAC_H
#define SEMAPHORE_MAC_H
#ifndef  _WIN32
#include <semaphore.h>
#endif

#if defined(__APPLE__) && defined(__MACH__)

#include "TargetConditionals.h"
#ifdef TARGET_OS_MAC

typedef struct sem_mac_s{
	sem_t *sem;
	char * name; 
}sem_mac_t;

#define UV_PLATFORM_SEM_T sem_mac_t

typedef UV_PLATFORM_SEM_T uv_sem_t;

#define UV_EXTERN 

UV_EXTERN int uv_sem_init(uv_sem_t* sem, unsigned int value);
UV_EXTERN void uv_sem_destroy(uv_sem_t* sem);
UV_EXTERN void uv_sem_post(uv_sem_t* sem);
UV_EXTERN void uv_sem_wait(uv_sem_t* sem);
UV_EXTERN int uv_sem_trywait(uv_sem_t* sem);

#endif
#endif

#endif

