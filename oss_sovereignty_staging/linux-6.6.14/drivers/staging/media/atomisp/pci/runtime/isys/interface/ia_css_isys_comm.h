 
 

#ifndef __IA_CSS_ISYS_COMM_H
#define __IA_CSS_ISYS_COMM_H

#include <type_support.h>
#include <input_system.h>

#ifdef ISP2401
#include <platform_support.h>		 
#include <input_system_global.h>
#include <ia_css_stream_public.h>	 

#define SH_CSS_NODES_PER_THREAD		2
#define SH_CSS_MAX_ISYS_CHANNEL_NODES	(SH_CSS_MAX_SP_THREADS * SH_CSS_NODES_PER_THREAD)

 
typedef virtual_input_system_stream_t		*ia_css_isys_stream_h;
typedef virtual_input_system_stream_cfg_t	ia_css_isys_stream_cfg_t;

 
typedef bool ia_css_isys_error_t;

static inline uint32_t ia_css_isys_generate_stream_id(
    u32	sp_thread_id,
    uint32_t	stream_id)
{
	return sp_thread_id * IA_CSS_STREAM_MAX_ISYS_STREAM_PER_CH + stream_id;
}

#endif   
#endif   
