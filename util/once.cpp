#include "log.h"
#include "atomic.h"
#include "once.h"
#include "event.h"

#define uv_once_t uv_cross_once_t

static void uv__cross_once_inner(uv_once_t* guard, void (*callback)(void)) {
	event_handle existing_event, created_event;

	created_event = CreateEvent( 1, 0);
	if (created_event == NULL) {
		VLOGE("CreateEvent Error.");
		return;
	}
#ifdef _WIN32
	existing_event = (event_handle)InterlockedCompareExchangePointer((PVOID volatile *)&guard->event,
	                                                 created_event,
	                                                 NULL);
#else
	existing_event = (event_handle)InterlockedCompareExchangePointer(&guard->event,
													 created_event,
													 NULL);
#endif

	if (existing_event == NULL) {
		/* We won the race */
		callback();
		int result = SetEvent(created_event);
#ifdef _WIN32
		VASSERTA(result == 1,"%d error:%d",result,GetLastError());
#else
		VASSERTA(result == 0,"%d",result);
#endif
		guard->ran = 1;
	} else {
		/* We lost the race. Destroy the event we created and wait for the */
		/* existing one to become signaled. */
		CloseEvent(created_event);
		int result = WaitForEvent(existing_event, INFINITE);
		VASSERT(result == WAIT_OBJECT_0);
	}
}

static void uv__cross_once_inner(uv_once_t* guard, void (*callback)(void* param),void* param) {
	event_handle existing_event, created_event;

	created_event = CreateEvent( 1, 0);
	if (created_event == NULL) {
		VLOGE("CreateEvent Error.");
		return;
	}

#ifdef _WIN32
	existing_event = (event_handle)InterlockedCompareExchangePointer((PVOID volatile *)&guard->event,
	                                                 created_event,
	                                                 NULL);
#else
	existing_event = (event_handle)InterlockedCompareExchangePointer(&guard->event,
													 created_event,
													 NULL);
#endif

	if (existing_event == NULL) {
		/* We won the race */
		callback(param);
		int result = SetEvent(created_event);
#ifdef _WIN32
		VASSERTA(result == 1,"%d error:%d",result,GetLastError());
#else
		VASSERTA(result == 0,"%d",result);
#endif
		guard->ran = 1;
	} else {
		/* We lost the race. Destroy the event we created and wait for the */
		/* existing one to become signaled. */
		CloseEvent(created_event);
		int result = WaitForEvent(existing_event, INFINITE);
		VASSERT(result == WAIT_OBJECT_0);
	}
}

void uv_cross_once(uv_once_t* guard, void (*callback)(void)) {
	/* Fast case - avoid WaitForSingleObject. */
	if (guard->ran) {
		return;
	}
	uv__cross_once_inner(guard, callback);
}

void uv_cross_once(uv_once_t* guard, void (*callback)(void* param),void * param){
	if (guard->ran) {
		return;
	}
	uv__cross_once_inner(guard, callback,param);
}
