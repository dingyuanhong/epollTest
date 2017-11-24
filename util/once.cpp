#include "log.h"
#include "atomic.h"
#include "once.h"
#include "event.h"

#define uv_once_t uv_cross_once_t

static void uv__cross_once_inner(uv_once_t* guard, void (*callback)(void)) {
	event_handle existing_event, created_event;

	created_event = createEvent( 1, 0);
	if (created_event == NULL) {
		VLOGE("CreateEvent Error.");
		return;
	}

	existing_event = (event_handle)atomic_cas_ptr(guard->event,
													 (ATOMIC_TYPE)created_event,
													 NULL);

	if (existing_event == NULL) {
		/* We won the race */
		callback();
		int result = setEvent(created_event);
		VASSERTA(result == 0,"%d",result);
		guard->ran = 1;
	} else {
		/* We lost the race. Destroy the event we created and wait for the */
		/* existing one to become signaled. */
		closeEvent(created_event);
		int result = waitForEvent(existing_event, INFINITE);
		VASSERT(result == WAIT_OBJECT_0);
	}
}

static void uv__cross_once_inner(uv_once_t* guard, void (*callback)(void* param),void* param) {
	event_handle existing_event, created_event;

	created_event = createEvent( 1, 0);
	if (created_event == NULL) {
		VLOGE("CreateEvent Error.");
		return;
	}

	existing_event = (event_handle)atomic_cas_ptr(guard->event,
													 (ATOMIC_TYPE)created_event,
													 NULL);

	if (existing_event == NULL) {
		/* We won the race */
		callback(param);
		int result = setEvent(created_event);
		VASSERTA(result == 0,"%d",result);
		guard->ran = 1;
	} else {
		/* We lost the race. Destroy the event we created and wait for the */
		/* existing one to become signaled. */
		closeEvent(created_event);
		int result = waitForEvent(existing_event, INFINITE);
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
