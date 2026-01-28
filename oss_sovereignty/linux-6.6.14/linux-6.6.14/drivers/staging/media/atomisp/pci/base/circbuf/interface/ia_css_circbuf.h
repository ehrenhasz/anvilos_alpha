#ifndef _IA_CSS_CIRCBUF_H
#define _IA_CSS_CIRCBUF_H
#include <sp.h>
#include <type_support.h>
#include <math_support.h>
#include <assert_support.h>
#include <platform_support.h>
#include "ia_css_circbuf_comm.h"
#include "ia_css_circbuf_desc.h"
typedef struct ia_css_circbuf_s ia_css_circbuf_t;
struct ia_css_circbuf_s {
	ia_css_circbuf_desc_t *desc;     
	ia_css_circbuf_elem_t *elems;	 
};
void ia_css_circbuf_create(
    ia_css_circbuf_t *cb,
    ia_css_circbuf_elem_t *elems,
    ia_css_circbuf_desc_t *desc);
void ia_css_circbuf_destroy(
    ia_css_circbuf_t *cb);
uint32_t ia_css_circbuf_pop(
    ia_css_circbuf_t *cb);
uint32_t ia_css_circbuf_extract(
    ia_css_circbuf_t *cb,
    int offset);
static inline void ia_css_circbuf_elem_set_val(
    ia_css_circbuf_elem_t *elem,
    uint32_t val)
{
	OP___assert(elem);
	elem->val = val;
}
static inline void ia_css_circbuf_elem_init(
    ia_css_circbuf_elem_t *elem)
{
	OP___assert(elem);
	ia_css_circbuf_elem_set_val(elem, 0);
}
static inline void ia_css_circbuf_elem_cpy(
    ia_css_circbuf_elem_t *src,
    ia_css_circbuf_elem_t *dest)
{
	OP___assert(src);
	OP___assert(dest);
	ia_css_circbuf_elem_set_val(dest, src->val);
}
static inline uint8_t ia_css_circbuf_get_pos_at_offset(
    ia_css_circbuf_t *cb,
    u32 base,
    int offset)
{
	u8 dest;
	OP___assert(cb);
	OP___assert(cb->desc);
	OP___assert(cb->desc->size > 0);
	while (offset < 0) {
		offset += cb->desc->size;
	}
	dest = OP_std_modadd(base, offset, cb->desc->size);
	return dest;
}
static inline int ia_css_circbuf_get_offset(
    ia_css_circbuf_t *cb,
    u32 src_pos,
    uint32_t dest_pos)
{
	int offset;
	OP___assert(cb);
	OP___assert(cb->desc);
	offset = (int)(dest_pos - src_pos);
	offset += (offset < 0) ? cb->desc->size : 0;
	return offset;
}
static inline uint32_t ia_css_circbuf_get_size(
    ia_css_circbuf_t *cb)
{
	OP___assert(cb);
	OP___assert(cb->desc);
	return cb->desc->size;
}
static inline uint32_t ia_css_circbuf_get_num_elems(
    ia_css_circbuf_t *cb)
{
	int num;
	OP___assert(cb);
	OP___assert(cb->desc);
	num = ia_css_circbuf_get_offset(cb, cb->desc->start, cb->desc->end);
	return (uint32_t)num;
}
static inline bool ia_css_circbuf_is_empty(
    ia_css_circbuf_t *cb)
{
	OP___assert(cb);
	OP___assert(cb->desc);
	return ia_css_circbuf_desc_is_empty(cb->desc);
}
static inline bool ia_css_circbuf_is_full(ia_css_circbuf_t *cb)
{
	OP___assert(cb);
	OP___assert(cb->desc);
	return ia_css_circbuf_desc_is_full(cb->desc);
}
static inline void ia_css_circbuf_write(
    ia_css_circbuf_t *cb,
    ia_css_circbuf_elem_t elem)
{
	OP___assert(cb);
	OP___assert(cb->desc);
	assert(!ia_css_circbuf_is_full(cb));
	ia_css_circbuf_elem_cpy(&elem, &cb->elems[cb->desc->end]);
	cb->desc->end = ia_css_circbuf_get_pos_at_offset(cb, cb->desc->end, 1);
}
static inline void ia_css_circbuf_push(
    ia_css_circbuf_t *cb,
    uint32_t val)
{
	ia_css_circbuf_elem_t elem;
	OP___assert(cb);
	ia_css_circbuf_elem_init(&elem);
	ia_css_circbuf_elem_set_val(&elem, val);
	ia_css_circbuf_write(cb, elem);
}
static inline uint32_t ia_css_circbuf_get_free_elems(
    ia_css_circbuf_t *cb)
{
	OP___assert(cb);
	OP___assert(cb->desc);
	return ia_css_circbuf_desc_get_free_elems(cb->desc);
}
uint32_t ia_css_circbuf_peek(
    ia_css_circbuf_t *cb,
    int offset);
uint32_t ia_css_circbuf_peek_from_start(
    ia_css_circbuf_t *cb,
    int offset);
bool ia_css_circbuf_increase_size(
    ia_css_circbuf_t *cb,
    unsigned int sz_delta,
    ia_css_circbuf_elem_t *elems);
#endif  
