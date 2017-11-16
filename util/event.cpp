#ifndef _WIN32
#include <sys/time.h>
#include <errno.h>
#include <new>
#include <iostream>

#include "event.h"
#include "uv_memory.h"

event_handle CreateEvent(bool bManualReset, bool bInitialState)
{
	event_handle event = (event_handle)uv_malloc(sizeof(uv_event_t));
    if (event == NULL)
    {
        return NULL;
    }
    event->state = bInitialState;
    event->manual_reset = bManualReset;
    if (pthread_mutex_init(&event->mutex, NULL))
    {
        delete event;
        return NULL;
    }
    if (pthread_cond_init(&event->cond, NULL))
    {
        pthread_mutex_destroy(&event->mutex);
        delete event;
        return NULL;
    }
	return event;
}

int WaitForEvent(event_handle event, long milliseconds)
{
	int rc = 0;
    struct timespec abstime;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    abstime.tv_sec  = tv.tv_sec + milliseconds / 1000;
    abstime.tv_nsec = tv.tv_usec*1000 + (milliseconds % 1000)*1000000;
    if (abstime.tv_nsec >= 1000000000)
    {
        abstime.tv_nsec -= 1000000000;
        abstime.tv_sec++;
    }

    if (pthread_mutex_lock(&event->mutex) != 0)
    {
        return WAIT_FAILED;
    }
    while (!event->state)
    {
		if(milliseconds == INFINITE)
		{
			rc = pthread_cond_wait(&event->cond, &event->mutex);
		}
		else
		{
			rc = pthread_cond_timedwait(&event->cond, &event->mutex, &abstime);
		}
        if (rc)
        {
            if (rc == ETIMEDOUT) break;
            pthread_mutex_unlock(&event->mutex);
            return WAIT_FAILED;
        }
    }
    if (rc == 0 && !event->manual_reset)
    {
        event->state = false;
    }
    if (pthread_mutex_unlock(&event->mutex) != 0)
    {
        return WAIT_FAILED;
    }
    if (rc == ETIMEDOUT)
    {
        //timeout return 1
        return WAIT_TIMEOUT;
    }
    //wait event success return 0
    return WAIT_OBJECT_0;
}

int ResetEvent(event_handle event)
{
	if(event == NULL) return -1;

	if (pthread_mutex_lock(&event->mutex) != 0)
    {
        return -1;
    }

    event->state = false;

    if (pthread_mutex_unlock(&event->mutex) != 0)
    {
        return -1;
    }
    return 0;
}

int SetEvent(event_handle event)
{
	if(event == NULL) return -1;
	return pthread_cond_signal(&event->cond);
}

void CloseEvent(event_handle event)
{
	if(event == NULL) return;

	pthread_cond_destroy(&event->cond);
    pthread_mutex_destroy(&event->mutex);
    uv_free(event);
}

#endif
