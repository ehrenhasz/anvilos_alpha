#ifndef __IA_CSS_QUEUE_H
#define __IA_CSS_QUEUE_H
#include <platform_support.h>
#include <type_support.h>
#include "ia_css_queue_comm.h"
#include "../src/queue_access.h"
struct ia_css_queue_local {
	ia_css_circbuf_desc_t *cb_desc;  
	ia_css_circbuf_elem_t *cb_elems;  
};
typedef struct ia_css_queue_local ia_css_queue_local_t;
typedef struct ia_css_queue ia_css_queue_t;
int ia_css_queue_local_init(
    ia_css_queue_t *qhandle,
    ia_css_queue_local_t *desc);
int ia_css_queue_remote_init(
    ia_css_queue_t *qhandle,
    ia_css_queue_remote_t *desc);
int ia_css_queue_uninit(
    ia_css_queue_t *qhandle);
int ia_css_queue_enqueue(
    ia_css_queue_t *qhandle,
    uint32_t item);
int ia_css_queue_dequeue(
    ia_css_queue_t *qhandle,
    uint32_t *item);
int ia_css_queue_is_empty(
    ia_css_queue_t *qhandle,
    bool *is_empty);
int ia_css_queue_is_full(
    ia_css_queue_t *qhandle,
    bool *is_full);
int ia_css_queue_get_used_space(
    ia_css_queue_t *qhandle,
    uint32_t *size);
int ia_css_queue_get_free_space(
    ia_css_queue_t *qhandle,
    uint32_t *size);
int ia_css_queue_peek(
    ia_css_queue_t *qhandle,
    u32 offset,
    uint32_t *element);
int ia_css_queue_get_size(
    ia_css_queue_t *qhandle,
    uint32_t *size);
#endif  
