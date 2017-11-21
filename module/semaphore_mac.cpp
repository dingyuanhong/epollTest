#include "../util/log.h"
#include "semaphore_mac.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef USED_MAC_SEM
#if defined(__APPLE__) && defined(__MACH__)
#include "TargetConditionals.h"
#ifdef TARGET_OS_MAC

int uv_sem_init( uv_sem_t* sem, unsigned int value )
{
	long ptr = (long)sem;
	sem->name = (char*)malloc(9);
	sprintf(sem->name,"%ld",ptr);
	sem->sem = sem_open( sem->name, O_CREAT, 0644, value );
    
	if( sem->sem == SEM_FAILED )
	{
		switch( errno )
		{
            case EEXIST:
                VLOGE("sem_open:EEXIST(%s)",sem->name);
                break;
                
            default:
                VLOGE("sem_open:%d",errno);
                break;
		}
		return -EINVAL;
	}
    
	return 0;
}


int uv_sem__delete(const char * name)
{
    int ret = sem_unlink(name);
    if (ret == -1) {
        VLOGE("sem_unlink:%d",errno);
        return -1;
    }
    return 0;
}

void uv_sem_destroy( uv_sem_t * sem )
{
	int ret = sem_close( sem->sem );
	VASSERT(sem->name != NULL);
    uv_sem__delete(sem->name);
    free(sem->name);
	if( ret == -1 )
	{
		switch( errno )
		{
            case EINVAL:
                VLOGE("sem_close:EINVAL");
                break;
                
            default:
                VLOGE("sem_close:%d",errno);
                break;
		}
	}
}

void uv_sem_post( uv_sem_t * sem )
{
	int ret = sem_post( sem->sem );
	if(ret != 0)
	{
		VLOGE("sem_post:%d",errno);
	}
}

void uv_sem_wait( uv_sem_t * sem )
{
	int ret = sem_wait( sem->sem );
	if(ret != 0)
	{
		VLOGE("sem_wait:%d",errno);
	}
}

int uv_sem_trywait( uv_sem_t * sem )
{
	int retErr = sem_trywait( sem->sem );
	if( retErr == -1 )
	{
		if( errno != EAGAIN )
		{
			VLOGE("sem_trywait:%d",errno);
		}
		return -1;
	}
	return 0;
}

#endif
#endif
#endif