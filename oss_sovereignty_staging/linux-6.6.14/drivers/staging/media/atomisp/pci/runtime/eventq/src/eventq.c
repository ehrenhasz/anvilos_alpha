
 

#include "ia_css_types.h"
#include "assert_support.h"
#include "ia_css_queue.h"  
#include "ia_css_eventq.h"
#include "ia_css_event.h"	 
int ia_css_eventq_recv(
    ia_css_queue_t *eventq_handle,
    uint8_t *payload)
{
	u32 sp_event;
	int error;

	 
	error = ia_css_queue_dequeue(eventq_handle, &sp_event);

	 
	if (!error)
		ia_css_event_decode(sp_event, payload);
	return error;
}

 
int ia_css_eventq_send(
    ia_css_queue_t *eventq_handle,
    u8 evt_id,
    u8 evt_payload_0,
    u8 evt_payload_1,
    uint8_t evt_payload_2)
{
	u8 tmp[4];
	u32 sw_event;
	int error = -ENOSYS;

	 
	tmp[0] = evt_id;
	tmp[1] = evt_payload_0;
	tmp[2] = evt_payload_1;
	tmp[3] = evt_payload_2;
	ia_css_event_encode(tmp, 4, &sw_event);

	 
	for ( ; ; ) {
		error = ia_css_queue_enqueue(eventq_handle, sw_event);
		if (error != -ENOBUFS) {
			 
			break;
		}
		 
		udelay(1);
	}
	return error;
}
