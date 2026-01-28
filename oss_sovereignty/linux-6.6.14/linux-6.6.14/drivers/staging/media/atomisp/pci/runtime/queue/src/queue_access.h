#ifndef __QUEUE_ACCESS_H
#define __QUEUE_ACCESS_H
#include <linux/errno.h>
#include <type_support.h>
#include <ia_css_queue_comm.h>
#include <ia_css_circbuf.h>
#define QUEUE_IGNORE_START_FLAG	0x0001
#define QUEUE_IGNORE_END_FLAG	0x0002
#define QUEUE_IGNORE_SIZE_FLAG	0x0004
#define QUEUE_IGNORE_STEP_FLAG	0x0008
#define QUEUE_IGNORE_DESC_FLAGS_MAX 0x000f
#define QUEUE_IGNORE_SIZE_START_STEP_FLAGS \
	(QUEUE_IGNORE_SIZE_FLAG | \
	QUEUE_IGNORE_START_FLAG | \
	QUEUE_IGNORE_STEP_FLAG)
#define QUEUE_IGNORE_SIZE_END_STEP_FLAGS \
	(QUEUE_IGNORE_SIZE_FLAG | \
	QUEUE_IGNORE_END_FLAG   | \
	QUEUE_IGNORE_STEP_FLAG)
#define QUEUE_IGNORE_START_END_STEP_FLAGS \
	(QUEUE_IGNORE_START_FLAG | \
	QUEUE_IGNORE_END_FLAG	  | \
	QUEUE_IGNORE_STEP_FLAG)
#define QUEUE_CB_DESC_INIT(cb_desc)	\
	do {				\
		(cb_desc)->size  = 0;	\
		(cb_desc)->step  = 0;	\
		(cb_desc)->start = 0;	\
		(cb_desc)->end   = 0;	\
	} while (0)
struct ia_css_queue {
	u8 type;         
	u8 location;     
	u8 proc_id;      
	union {
		ia_css_circbuf_t cb_local;
		struct {
			u32 cb_desc_addr;  
			u32 cb_elems_addr;  
		}	remote;
	} desc;
};
int ia_css_queue_load(
    struct ia_css_queue *rdesc,
    ia_css_circbuf_desc_t *cb_desc,
    uint32_t ignore_desc_flags);
int ia_css_queue_store(
    struct ia_css_queue *rdesc,
    ia_css_circbuf_desc_t *cb_desc,
    uint32_t ignore_desc_flags);
int ia_css_queue_item_load(
    struct ia_css_queue *rdesc,
    u8 position,
    ia_css_circbuf_elem_t *item);
int ia_css_queue_item_store(
    struct ia_css_queue *rdesc,
    u8 position,
    ia_css_circbuf_elem_t *item);
#endif  
