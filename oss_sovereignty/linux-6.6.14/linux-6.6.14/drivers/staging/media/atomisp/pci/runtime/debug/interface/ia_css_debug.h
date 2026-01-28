#ifndef _IA_CSS_DEBUG_H_
#define _IA_CSS_DEBUG_H_
#include <type_support.h>
#include <linux/stdarg.h>
#include <linux/bits.h>
#include "ia_css_types.h"
#include "ia_css_binary.h"
#include "ia_css_frame_public.h"
#include "ia_css_pipe_public.h"
#include "ia_css_stream_public.h"
#include "ia_css_metadata.h"
#include "sh_css_internal.h"
#include "ia_css_pipe.h"
#define IA_CSS_DEBUG_ERROR   1
#define IA_CSS_DEBUG_WARNING 3
#define IA_CSS_DEBUG_VERBOSE   5
#define IA_CSS_DEBUG_TRACE   6
#define IA_CSS_DEBUG_TRACE_PRIVATE   7
#define IA_CSS_DEBUG_PARAM   8
#define IA_CSS_DEBUG_INFO    9
extern int dbg_level;
enum ia_css_debug_enable_param_dump {
	IA_CSS_DEBUG_DUMP_FPN = BIT(0),   
	IA_CSS_DEBUG_DUMP_OB  = BIT(1),   
	IA_CSS_DEBUG_DUMP_SC  = BIT(2),   
	IA_CSS_DEBUG_DUMP_WB  = BIT(3),   
	IA_CSS_DEBUG_DUMP_DP  = BIT(4),   
	IA_CSS_DEBUG_DUMP_BNR = BIT(5),   
	IA_CSS_DEBUG_DUMP_S3A = BIT(6),   
	IA_CSS_DEBUG_DUMP_DE  = BIT(7),   
	IA_CSS_DEBUG_DUMP_YNR = BIT(8),   
	IA_CSS_DEBUG_DUMP_CSC = BIT(9),   
	IA_CSS_DEBUG_DUMP_GC  = BIT(10),  
	IA_CSS_DEBUG_DUMP_TNR = BIT(11),  
	IA_CSS_DEBUG_DUMP_ANR = BIT(12),  
	IA_CSS_DEBUG_DUMP_CE  = BIT(13),  
	IA_CSS_DEBUG_DUMP_ALL = BIT(14),  
};
#define IA_CSS_ERROR(fmt, ...) \
	ia_css_debug_dtrace(IA_CSS_DEBUG_ERROR, \
		"%s() %d: error: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#define IA_CSS_WARNING(fmt, ...) \
	ia_css_debug_dtrace(IA_CSS_DEBUG_WARNING, \
		"%s() %d: warning: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#define IA_CSS_ENTER(fmt, ...) \
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, \
		"%s(): enter: " fmt "\n", __func__, ##__VA_ARGS__)
#define IA_CSS_ENTER_LEAVE(fmt, ...) \
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, \
		"%s(): enter: leave: " fmt "\n", __func__, ##__VA_ARGS__)
#define IA_CSS_LEAVE(fmt, ...) \
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, \
		"%s(): leave: " fmt "\n", __func__, ##__VA_ARGS__)
#define IA_CSS_LEAVE_ERR(__err) \
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, \
		"%s() %d: leave: return_err=%d\n", __func__, __LINE__, __err)
#define IA_CSS_LOG(fmt, ...) \
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, \
		"%s(): " fmt "\n", __func__, ##__VA_ARGS__)
#define IA_CSS_ENTER_PRIVATE(fmt, ...) \
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, \
		"%s(): enter: " fmt "\n", __func__, ##__VA_ARGS__)
#define IA_CSS_LEAVE_PRIVATE(fmt, ...) \
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, \
		"%s(): leave: " fmt "\n", __func__, ##__VA_ARGS__)
#define IA_CSS_LEAVE_ERR_PRIVATE(__err) \
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, \
		"%s() %d: leave: return_err=%d\n", __func__, __LINE__, __err)
#define IA_CSS_ENTER_LEAVE_PRIVATE(fmt, ...) \
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, \
		"%s(): enter: leave: " fmt "\n", __func__, ##__VA_ARGS__)
static inline void __printf(2, 0) ia_css_debug_vdtrace(unsigned int level,
						       const char *fmt,
						       va_list args)
{
	if (dbg_level >= level)
		sh_css_vprint(fmt, args);
}
__printf(2, 3) void ia_css_debug_dtrace(unsigned int level,
					const char *fmt, ...);
void ia_css_debug_dump_sp_stack_info(void);
void ia_css_debug_set_dtrace_level(
    const unsigned int	trace_level);
unsigned int ia_css_debug_get_dtrace_level(void);
void ia_css_debug_dump_isp_state(void);
void ia_css_debug_dump_sp_state(void);
void ia_css_debug_dump_gac_state(void);
void ia_css_debug_dump_dma_state(void);
void ia_css_debug_dump_sp_sw_debug_info(void);
void ia_css_debug_dump_debug_info(
    const char	*context);
#if SP_DEBUG != SP_DEBUG_NONE
void ia_css_debug_print_sp_debug_state(
    const struct sh_css_sp_debug_state *state);
#endif
void ia_css_debug_binary_print(
    const struct ia_css_binary *bi);
void ia_css_debug_sp_dump_mipi_fifo_high_water(void);
void ia_css_debug_dump_isp_gdc_fifo_state(void);
void ia_css_debug_dump_dma_isp_fifo_state(void);
void ia_css_debug_dump_dma_sp_fifo_state(void);
void ia_css_debug_dump_pif_a_isp_fifo_state(void);
void ia_css_debug_dump_pif_b_isp_fifo_state(void);
void ia_css_debug_dump_str2mem_sp_fifo_state(void);
void ia_css_debug_dump_isp_sp_fifo_state(void);
void ia_css_debug_dump_all_fifo_state(void);
void ia_css_debug_dump_rx_state(void);
void ia_css_debug_dump_isys_state(void);
void ia_css_debug_frame_print(
    const struct ia_css_frame	*frame,
    const char	*descr);
void ia_css_debug_enable_sp_sleep_mode(enum ia_css_sp_sleep_mode mode);
void ia_css_debug_wake_up_sp(void);
void ia_css_debug_dump_isp_params(struct ia_css_stream *stream,
				  unsigned int enable);
void ia_css_debug_dump_perf_counters(void);
#ifdef HAS_WATCHDOG_SP_THREAD_DEBUG
void sh_css_dump_thread_wait_info(void);
void sh_css_dump_pipe_stage_info(void);
void sh_css_dump_pipe_stripe_info(void);
#endif
void ia_css_debug_dump_isp_binary(void);
void sh_css_dump_sp_raw_copy_linecount(bool reduced);
void ia_css_debug_dump_resolution(
    const struct ia_css_resolution *res,
    const char *label);
void ia_css_debug_dump_frame_info(
    const struct ia_css_frame_info *info,
    const char *label);
void ia_css_debug_dump_capture_config(
    const struct ia_css_capture_config *config);
void ia_css_debug_dump_pipe_extra_config(
    const struct ia_css_pipe_extra_config *extra_config);
void ia_css_debug_dump_pipe_config(
    const struct ia_css_pipe_config *config);
void ia_css_debug_dump_stream_config_source(
    const struct ia_css_stream_config *config);
void ia_css_debug_dump_mipi_buffer_config(
    const struct ia_css_mipi_buffer_config *config);
void ia_css_debug_dump_metadata_config(
    const struct ia_css_metadata_config *config);
void ia_css_debug_dump_stream_config(
    const struct ia_css_stream_config *config,
    int num_pipes);
void ia_css_debug_tagger_state(void);
bool ia_css_debug_mode_init(void);
bool ia_css_debug_mode_disable_dma_channel(
    int dma_ID,
    int channel_id,
    int request_type);
bool ia_css_debug_mode_enable_dma_channel(
    int dma_ID,
    int channel_id,
    int request_type);
void ia_css_debug_dump_trace(void);
void ia_css_debug_pc_dump(sp_ID_t id, unsigned int num_of_dumps);
void ia_css_debug_dump_hang_status(
    struct ia_css_pipe *pipe);
void ia_css_debug_ext_command_handler(void);
#endif  
