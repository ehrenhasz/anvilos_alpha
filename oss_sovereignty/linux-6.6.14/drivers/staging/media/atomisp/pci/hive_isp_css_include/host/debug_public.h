 
 

#ifndef __DEBUG_PUBLIC_H_INCLUDED__
#define __DEBUG_PUBLIC_H_INCLUDED__

#include <type_support.h>
#include <ia_css_types.h>
#include "system_local.h"

 

typedef struct debug_data_s		debug_data_t;
typedef struct debug_data_ddr_s	debug_data_ddr_t;

extern debug_data_t				*debug_data_ptr;
extern hrt_address				debug_buffer_address;
extern ia_css_ptr				debug_buffer_ddr_address;

 
STORAGE_CLASS_DEBUG_H bool is_debug_buffer_empty(void);

 
STORAGE_CLASS_DEBUG_H hrt_data debug_dequeue(void);

 
STORAGE_CLASS_DEBUG_H void debug_synch_queue(void);

 
STORAGE_CLASS_DEBUG_H void debug_synch_queue_isp(void);

 
STORAGE_CLASS_DEBUG_H void debug_synch_queue_ddr(void);

 
void debug_buffer_init(
    const hrt_address		addr);

 
void debug_buffer_ddr_init(
    const ia_css_ptr		addr);

 
void debug_buffer_setmode(
    const debug_buf_mode_t	mode);

#endif  
