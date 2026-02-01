 
 

#ifndef _IA_CSS_CIRCBUF_DESC_H_
#define _IA_CSS_CIRCBUF_DESC_H_

#include <type_support.h>
#include <math_support.h>
#include <platform_support.h>
#include <sp.h>
#include "ia_css_circbuf_comm.h"
 
 
static inline bool ia_css_circbuf_desc_is_empty(
    ia_css_circbuf_desc_t *cb_desc)
{
	OP___assert(cb_desc);
	return (cb_desc->end == cb_desc->start);
}

 
static inline bool ia_css_circbuf_desc_is_full(
    ia_css_circbuf_desc_t *cb_desc)
{
	OP___assert(cb_desc);
	return (OP_std_modadd(cb_desc->end, 1, cb_desc->size) == cb_desc->start);
}

 
static inline void ia_css_circbuf_desc_init(
    ia_css_circbuf_desc_t *cb_desc,
    int8_t size)
{
	OP___assert(cb_desc);
	cb_desc->size = size;
}

 
static inline uint8_t ia_css_circbuf_desc_get_pos_at_offset(
    ia_css_circbuf_desc_t *cb_desc,
    u32 base,
    int offset)
{
	u8 dest;

	OP___assert(cb_desc);
	OP___assert(cb_desc->size > 0);

	 
	while (offset < 0) {
		offset += cb_desc->size;
	}

	 
	dest = OP_std_modadd(base, offset, cb_desc->size);

	return dest;
}

 
static inline int ia_css_circbuf_desc_get_offset(
    ia_css_circbuf_desc_t *cb_desc,
    u32 src_pos,
    uint32_t dest_pos)
{
	int offset;

	OP___assert(cb_desc);

	offset = (int)(dest_pos - src_pos);
	offset += (offset < 0) ? cb_desc->size : 0;

	return offset;
}

 
static inline uint32_t ia_css_circbuf_desc_get_num_elems(
    ia_css_circbuf_desc_t *cb_desc)
{
	int num;

	OP___assert(cb_desc);

	num = ia_css_circbuf_desc_get_offset(cb_desc,
					     cb_desc->start,
					     cb_desc->end);

	return (uint32_t)num;
}

 
static inline uint32_t ia_css_circbuf_desc_get_free_elems(
    ia_css_circbuf_desc_t *cb_desc)
{
	u32 num;

	OP___assert(cb_desc);

	num = ia_css_circbuf_desc_get_offset(cb_desc,
					     cb_desc->start,
					     cb_desc->end);

	return (cb_desc->size - num);
}
#endif  
