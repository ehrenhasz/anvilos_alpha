 
 

#ifndef _IA_CSS_EVENTQ_H
#define _IA_CSS_EVENTQ_H

#include "ia_css_queue.h"	 

 
int ia_css_eventq_recv(
    ia_css_queue_t *eventq_handle,
    uint8_t *payload);

 
int ia_css_eventq_send(
    ia_css_queue_t *eventq_handle,
    u8 evt_id,
    u8 evt_payload_0,
    u8 evt_payload_1,
    uint8_t evt_payload_2);
#endif  
