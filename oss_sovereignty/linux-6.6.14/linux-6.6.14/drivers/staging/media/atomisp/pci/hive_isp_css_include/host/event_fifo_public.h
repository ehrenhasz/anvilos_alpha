#ifndef __EVENT_FIFO_PUBLIC_H
#define __EVENT_FIFO_PUBLIC_H
#include <type_support.h>
#include "system_local.h"
STORAGE_CLASS_EVENT_H void event_wait_for(
    const event_ID_t		ID);
STORAGE_CLASS_EVENT_H void cnd_event_wait_for(
    const event_ID_t		ID,
    const bool				cnd);
STORAGE_CLASS_EVENT_H hrt_data event_receive_token(
    const event_ID_t		ID);
STORAGE_CLASS_EVENT_H void event_send_token(
    const event_ID_t		ID,
    const hrt_data			token);
STORAGE_CLASS_EVENT_H bool is_event_pending(
    const event_ID_t		ID);
STORAGE_CLASS_EVENT_H bool can_event_send_token(
    const event_ID_t		ID);
#endif  
