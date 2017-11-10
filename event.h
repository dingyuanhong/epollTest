#ifndef EVENT_H
#define EVENT_H

#ifdef _MSC_VER
#include <Windows.h>
#define event_handle HANDLE

#ifdef CreateEvent
#undef CreateEvent
#endif
#define CreateEvent(A, B) CreateEventA(NULL,A,B,NULL)

#define WaitForEvent WaitForSingleObject
#define CloseEvent CloseHandle

#else

#include <pthread.h>

typedef struct uv_event_s
{
    bool state;
    bool manual_reset;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
}uv_event_t;
typedef uv_event_t *event_handle;

#define INFINITE            long(-1)

#define WAIT_FAILED -1
#define WAIT_OBJECT_0  0
#define WAIT_TIMEOUT 258L

event_handle CreateEvent(bool bManualReset, bool bInitialState);

int WaitForEvent(event_handle event, long milliseconds);

int ResetEvent(event_handle event);
int SetEvent(event_handle event);

void CloseEvent(event_handle event);

#endif

#endif
