#ifndef _IA_CSS_BUFQ_H
#define _IA_CSS_BUFQ_H
#include <type_support.h>
#include "ia_css_bufq_comm.h"
#include "ia_css_buffer.h"
#include "ia_css_err.h"
#define BUFQ_EVENT_SIZE 4
bool ia_css_query_internal_queue_id(
    enum ia_css_buffer_type buf_type,
    unsigned int thread_id,
    enum sh_css_queue_id *val
);
void ia_css_queue_map(
    unsigned int thread_id,
    enum ia_css_buffer_type buf_type,
    bool map
);
void ia_css_queue_map_init(void);
void ia_css_bufq_init(void);
int ia_css_bufq_enqueue_buffer(
    int thread_index,
    int queue_id,
    uint32_t item);
int ia_css_bufq_dequeue_buffer(
    int queue_id,
    uint32_t *item);
int ia_css_bufq_enqueue_psys_event(
    u8 evt_id,
    u8 evt_payload_0,
    u8 evt_payload_1,
    uint8_t evt_payload_2
);
int ia_css_bufq_dequeue_psys_event(
    u8 item[BUFQ_EVENT_SIZE]
);
int ia_css_bufq_enqueue_isys_event(
    uint8_t evt_id);
int ia_css_bufq_dequeue_isys_event(
    u8 item[BUFQ_EVENT_SIZE]);
int ia_css_bufq_enqueue_tag_cmd(
    uint32_t item);
int ia_css_bufq_deinit(void);
void ia_css_bufq_dump_queue_info(void);
#endif	 
