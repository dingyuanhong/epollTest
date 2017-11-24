#ifndef EVENT_H
#define EVENT_H

#ifdef _MSC_VER
#include <Windows.h>
#define event_handle HANDLE

#ifdef createEvent
#undef createEvent
#endif
#define createEvent(A, B) CreateEventA(NULL,A,B,NULL)

#define waitForEvent WaitForSingleObject
#define closeEvent CloseHandle
//返回值:0 成功 其他:错误码
#define resetEvent(event) (ResetEvent(event) == 1?0:-1)
//返回值:0 成功 其他:错误码
#define setEvent(event) (SetEvent(event) == 1?0:-1)

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

event_handle createEvent(bool bManualReset, bool bInitialState);

int waitForEvent(event_handle event, long milliseconds);

//返回值:0 成功 其他:错误码
int resetEvent(event_handle event);
//返回值:0 成功 其他:错误码
int setEvent(event_handle event);

void closeEvent(event_handle event);

#endif

#endif
