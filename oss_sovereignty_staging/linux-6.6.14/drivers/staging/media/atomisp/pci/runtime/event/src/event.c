
 

#include "sh_css_sp.h"

#include "dma.h"	 

#include <type_support.h>
#include "ia_css_binary.h"
#include "sh_css_hrt.h"
#include "sh_css_defs.h"
#include "sh_css_internal.h"
#include "ia_css_debug.h"
#include "ia_css_debug_internal.h"
#include "sh_css_legacy.h"

#include "gdc_device.h"				 

 	 

#include "assert_support.h"

#include "ia_css_queue.h"	 
#include "ia_css_event.h"	 
 
bool ia_css_event_encode(
    u8	*in,
    u8	nr,
    uint32_t	*out)
{
	bool ret;
	u32 nr_of_bits;
	u32 i;

	assert(in);
	assert(out);
	OP___assert(nr > 0 && nr <= MAX_NR_OF_PAYLOADS_PER_SW_EVENT);

	 
	*out = 0;

	 
	nr_of_bits = sizeof(uint32_t) * 8 / nr;

	 
	for (i = 0; i < nr; i++) {
		*out <<= nr_of_bits;
		*out |= in[i];
	}

	 
	ret = (nr > 0 && nr <= MAX_NR_OF_PAYLOADS_PER_SW_EVENT);

	return ret;
}

void ia_css_event_decode(
    u32 event,
    uint8_t *payload)
{
	assert(payload[1] == 0);
	assert(payload[2] == 0);
	assert(payload[3] == 0);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
			    "ia_css_event_decode() enter:\n");

	 
	payload[0] = event & 0xff;   
	payload[1] = (event >> 8) & 0xff;
	payload[2] = (event >> 16) & 0xff;
	payload[3] = 0;

	switch (payload[0]) {
	case SH_CSS_SP_EVENT_PORT_EOF:
		payload[2] = 0;
		payload[3] = (event >> 24) & 0xff;
		break;

	case SH_CSS_SP_EVENT_ACC_STAGE_COMPLETE:
	case SH_CSS_SP_EVENT_TIMER:
	case SH_CSS_SP_EVENT_FRAME_TAGGED:
	case SH_CSS_SP_EVENT_FW_WARNING:
	case SH_CSS_SP_EVENT_FW_ASSERT:
		payload[3] = (event >> 24) & 0xff;
		break;
	default:
		break;
	}
}
