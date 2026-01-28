#ifndef __IA_CSS_QUEUE_COMM_H
#define __IA_CSS_QUEUE_COMM_H
#include "type_support.h"
#include "ia_css_circbuf.h"
#define IA_CSS_QUEUE_LOC_HOST 0
#define IA_CSS_QUEUE_LOC_SP   1
#define IA_CSS_QUEUE_LOC_ISP  2
#define IA_CSS_QUEUE_TYPE_LOCAL  0
#define IA_CSS_QUEUE_TYPE_REMOTE 1
#define IA_CSS_MIN_ELEM_COUNT    8
#define IA_CSS_DMA_XFER_MASK (IA_CSS_MIN_ELEM_COUNT - 1)
struct ia_css_queue_remote {
	u32 cb_desc_addr;  
	u32 cb_elems_addr;  
	u8 location;     
	u8 proc_id;      
};
typedef struct ia_css_queue_remote ia_css_queue_remote_t;
#endif  
