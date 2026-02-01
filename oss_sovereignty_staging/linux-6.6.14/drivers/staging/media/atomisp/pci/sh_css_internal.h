 
 

#ifndef _SH_CSS_INTERNAL_H_
#define _SH_CSS_INTERNAL_H_

#include <system_global.h>
#include <math_support.h>
#include <type_support.h>
#include <platform_support.h>
#include <linux/stdarg.h>

#if !defined(ISP2401)
#include "input_formatter.h"
#endif
#include "input_system.h"

#include "ia_css_types.h"
#include "ia_css_acc_types.h"
#include "ia_css_buffer.h"

#include "ia_css_binary.h"
#include "sh_css_firmware.h"  
#include "sh_css_legacy.h"
#include "sh_css_defs.h"
#include "sh_css_uds.h"
#include "dma.h"	 
#include "ia_css_circbuf_comm.h"  
#include "ia_css_frame_comm.h"
#include "ia_css_3a.h"
#include "ia_css_dvs.h"
#include "ia_css_metadata.h"
#include "runtime/bufq/interface/ia_css_bufq.h"
#include "ia_css_timer.h"

 
#define IA_CSS_NUM_CB_SEM_READ_RESOURCE	2
#define IA_CSS_NUM_CB_SEM_WRITE_RESOURCE	1
#define IA_CSS_NUM_CBS						2
#define IA_CSS_CB_MAX_ELEMS					2

 
#define IA_CSS_COPYSINK_SEM_INDEX	0
#define IA_CSS_TAGGER_SEM_INDEX	1

 
#define IA_CSS_POST_OUT_EVENT_FORCE		2

#define SH_CSS_MAX_BINARY_NAME	64

#define SP_DEBUG_NONE	(0)
#define SP_DEBUG_DUMP	(1)
#define SP_DEBUG_COPY	(2)
#define SP_DEBUG_TRACE	(3)
#define SP_DEBUG_MINIMAL (4)

#define SP_DEBUG SP_DEBUG_NONE
#define SP_DEBUG_MINIMAL_OVERWRITE 1

#define SH_CSS_TNR_BIT_DEPTH 8
#define SH_CSS_REF_BIT_DEPTH 8

 
#define NUM_CONTINUOUS_FRAMES	15
#define NUM_MIPI_FRAMES_PER_STREAM		2

#define NUM_ONLINE_INIT_CONTINUOUS_FRAMES      2

#define NR_OF_PIPELINES			IA_CSS_PIPE_ID_NUM  

#define SH_CSS_MAX_IF_CONFIGS	3  
#define SH_CSS_IF_CONFIG_NOT_NEEDED	0xFF

 

#if !defined(ISP2401)
#define SH_CSS_SP_INTERNAL_METADATA_THREAD	1
#else
#define SH_CSS_SP_INTERNAL_METADATA_THREAD	0
#endif

#define SH_CSS_SP_INTERNAL_SERVICE_THREAD		1

#define SH_CSS_MAX_SP_THREADS		5

#define SH_CSS_MAX_SP_INTERNAL_THREADS	(\
	 SH_CSS_SP_INTERNAL_SERVICE_THREAD +\
	 SH_CSS_SP_INTERNAL_METADATA_THREAD)

#define SH_CSS_MAX_PIPELINES	SH_CSS_MAX_SP_THREADS

 
#define CALC_ALIGNMENT_MEMBER(x, y)	(CEIL_MUL(x, y) - x)
#define SIZE_OF_HRT_VADDRESS		sizeof(hive_uint32)
#define SIZE_OF_IA_CSS_PTR		sizeof(uint32_t)

 
#define NUM_OF_SPS 1

#define NUM_OF_BLS 0

 
enum sh_css_order_binaries {
	SP_FIRMWARE = 0,
	ISP_FIRMWARE
};

 
enum sh_css_pipe_config_override {
	SH_CSS_PIPE_CONFIG_OVRD_NONE     = 0,
	SH_CSS_PIPE_CONFIG_OVRD_NO_OVRD  = 0xffff
};

enum host2sp_commands {
	host2sp_cmd_error = 0,
	 
	host2sp_cmd_ready = 1,
	 
	host2sp_cmd_dummy,		 
	host2sp_cmd_start_flash,	 
	host2sp_cmd_terminate,		 
	N_host2sp_cmd
};

 
enum sh_css_sp_event_type {
	SH_CSS_SP_EVENT_OUTPUT_FRAME_DONE,
	SH_CSS_SP_EVENT_SECOND_OUTPUT_FRAME_DONE,
	SH_CSS_SP_EVENT_VF_OUTPUT_FRAME_DONE,
	SH_CSS_SP_EVENT_SECOND_VF_OUTPUT_FRAME_DONE,
	SH_CSS_SP_EVENT_3A_STATISTICS_DONE,
	SH_CSS_SP_EVENT_DIS_STATISTICS_DONE,
	SH_CSS_SP_EVENT_PIPELINE_DONE,
	SH_CSS_SP_EVENT_FRAME_TAGGED,
	SH_CSS_SP_EVENT_INPUT_FRAME_DONE,
	SH_CSS_SP_EVENT_METADATA_DONE,
	SH_CSS_SP_EVENT_LACE_STATISTICS_DONE,
	SH_CSS_SP_EVENT_ACC_STAGE_COMPLETE,
	SH_CSS_SP_EVENT_TIMER,
	SH_CSS_SP_EVENT_PORT_EOF,
	SH_CSS_SP_EVENT_FW_WARNING,
	SH_CSS_SP_EVENT_FW_ASSERT,
	SH_CSS_SP_EVENT_NR_OF_TYPES		 
};

 
 
struct sh_css_ddr_address_map {
	ia_css_ptr isp_param;
	ia_css_ptr isp_mem_param[SH_CSS_MAX_STAGES][IA_CSS_NUM_MEMORIES];
	ia_css_ptr macc_tbl;
	ia_css_ptr fpn_tbl;
	ia_css_ptr sc_tbl;
	ia_css_ptr tetra_r_x;
	ia_css_ptr tetra_r_y;
	ia_css_ptr tetra_gr_x;
	ia_css_ptr tetra_gr_y;
	ia_css_ptr tetra_gb_x;
	ia_css_ptr tetra_gb_y;
	ia_css_ptr tetra_b_x;
	ia_css_ptr tetra_b_y;
	ia_css_ptr tetra_ratb_x;
	ia_css_ptr tetra_ratb_y;
	ia_css_ptr tetra_batr_x;
	ia_css_ptr tetra_batr_y;
	ia_css_ptr dvs_6axis_params_y;
};

#define SIZE_OF_SH_CSS_DDR_ADDRESS_MAP_STRUCT					\
	(SIZE_OF_HRT_VADDRESS +							\
	(SH_CSS_MAX_STAGES * IA_CSS_NUM_MEMORIES * SIZE_OF_HRT_VADDRESS) +	\
	(16 * SIZE_OF_HRT_VADDRESS))

 
struct sh_css_ddr_address_map_size {
	size_t isp_param;
	size_t isp_mem_param[SH_CSS_MAX_STAGES][IA_CSS_NUM_MEMORIES];
	size_t macc_tbl;
	size_t fpn_tbl;
	size_t sc_tbl;
	size_t tetra_r_x;
	size_t tetra_r_y;
	size_t tetra_gr_x;
	size_t tetra_gr_y;
	size_t tetra_gb_x;
	size_t tetra_gb_y;
	size_t tetra_b_x;
	size_t tetra_b_y;
	size_t tetra_ratb_x;
	size_t tetra_ratb_y;
	size_t tetra_batr_x;
	size_t tetra_batr_y;
	size_t dvs_6axis_params_y;
};

struct sh_css_ddr_address_map_compound {
	struct sh_css_ddr_address_map		map;
	struct sh_css_ddr_address_map_size	size;
};

struct ia_css_isp_parameter_set_info {
	struct sh_css_ddr_address_map
		mem_map; 
	u32
	isp_parameters_id; 
	ia_css_ptr
	output_frame_ptr; 
};

 
struct sh_css_binary_args {
	struct ia_css_frame *in_frame;	      
	const struct ia_css_frame
		*delay_frames[MAX_NUM_VIDEO_DELAY_FRAMES];    
	const struct ia_css_frame *tnr_frames[NUM_VIDEO_TNR_FRAMES];    
	struct ia_css_frame
		*out_frame[IA_CSS_BINARY_MAX_OUTPUT_PORTS];       
	struct ia_css_frame *out_vf_frame;    
	bool                 copy_vf;
	bool                 copy_output;
	unsigned int vf_downscale_log2;
};

#if SP_DEBUG == SP_DEBUG_DUMP

#define SH_CSS_NUM_SP_DEBUG 48

struct sh_css_sp_debug_state {
	unsigned int error;
	unsigned int debug[SH_CSS_NUM_SP_DEBUG];
};

#elif SP_DEBUG == SP_DEBUG_COPY

#define SH_CSS_SP_DBG_TRACE_DEPTH	(40)

struct sh_css_sp_debug_trace {
	u16 frame;
	u16 line;
	u16 pixel_distance;
	u16 mipi_used_dword;
	u16 sp_index;
};

struct sh_css_sp_debug_state {
	u16 if_start_line;
	u16 if_start_column;
	u16 if_cropped_height;
	u16 if_cropped_width;
	unsigned int index;
	struct sh_css_sp_debug_trace
		trace[SH_CSS_SP_DBG_TRACE_DEPTH];
};

#elif SP_DEBUG == SP_DEBUG_TRACE

 
#define SH_CSS_SP_DBG_NR_OF_TRACES	(1)
#define SH_CSS_SP_DBG_TRACE_DEPTH	(40)

#define SH_CSS_SP_DBG_TRACE_FILE_ID_BIT_POS (13)

struct sh_css_sp_debug_trace {
	u16 time_stamp;
	u16 location;	 
	u32 data;
};

struct sh_css_sp_debug_state {
	struct sh_css_sp_debug_trace
		trace[SH_CSS_SP_DBG_NR_OF_TRACES][SH_CSS_SP_DBG_TRACE_DEPTH];
	u16 index_last[SH_CSS_SP_DBG_NR_OF_TRACES];
	u8 index[SH_CSS_SP_DBG_NR_OF_TRACES];
};

#elif SP_DEBUG == SP_DEBUG_MINIMAL

#define SH_CSS_NUM_SP_DEBUG 128

struct sh_css_sp_debug_state {
	unsigned int error;
	unsigned int debug[SH_CSS_NUM_SP_DEBUG];
};

#endif

struct sh_css_sp_debug_command {
	 
	u32 dma_sw_reg;
};

#if !defined(ISP2401)
 
struct sh_css_sp_input_formatter_set {
	u32				stream_format;
	input_formatter_cfg_t	config_a;
	input_formatter_cfg_t	config_b;
};
#endif

#define IA_CSS_MIPI_SIZE_CHECK_MAX_NOF_ENTRIES_PER_PORT (3)

 
struct sh_css_sp_config {
	u8			no_isp_sync;  
	u8			enable_raw_pool_locking;  
	u8			lock_all;
	 
#if !defined(ISP2401)
	struct {
		u8					a_changed;
		u8					b_changed;
		u8					isp_2ppc;
		struct sh_css_sp_input_formatter_set
			set[SH_CSS_MAX_IF_CONFIGS];  
	} input_formatter;
#endif
#if !defined(ISP2401)
	sync_generator_cfg_t	sync_gen;
	tpg_cfg_t		tpg;
	prbs_cfg_t		prbs;
	input_system_cfg_t	input_circuit;
	u8			input_circuit_cfg_changed;
	u32		mipi_sizes_for_check[N_CSI_PORTS][IA_CSS_MIPI_SIZE_CHECK_MAX_NOF_ENTRIES_PER_PORT];
#endif
	u8                 enable_isys_event_queue;
	u8			disable_cont_vf;
};

enum sh_css_stage_type {
	SH_CSS_SP_STAGE_TYPE  = 0,
	SH_CSS_ISP_STAGE_TYPE = 1
};

#define SH_CSS_NUM_STAGE_TYPES 2

#define SH_CSS_PIPE_CONFIG_SAMPLE_PARAMS	BIT(0)
#define SH_CSS_PIPE_CONFIG_SAMPLE_PARAMS_MASK \
	((SH_CSS_PIPE_CONFIG_SAMPLE_PARAMS << SH_CSS_MAX_SP_THREADS) - 1)

#if defined(ISP2401)
struct sh_css_sp_pipeline_terminal {
	union {
		 
		virtual_input_system_stream_t
		virtual_input_system_stream[IA_CSS_STREAM_MAX_ISYS_STREAM_PER_CH];
	} context;
	 
	union {
		 
		virtual_input_system_stream_cfg_t
		virtual_input_system_stream_cfg[IA_CSS_STREAM_MAX_ISYS_STREAM_PER_CH];
	} ctrl;
};

struct sh_css_sp_pipeline_io {
	struct sh_css_sp_pipeline_terminal	input;
	 
	 
};

 
struct sh_css_sp_pipeline_io_status {
	u32	active[N_INPUT_SYSTEM_CSI_PORT];	 
	u32	running[N_INPUT_SYSTEM_CSI_PORT];	 
};

#endif
enum sh_css_port_dir {
	SH_CSS_PORT_INPUT  = 0,
	SH_CSS_PORT_OUTPUT  = 1
};

enum sh_css_port_type {
	SH_CSS_HOST_TYPE  = 0,
	SH_CSS_COPYSINK_TYPE  = 1,
	SH_CSS_TAGGERSINK_TYPE  = 2
};

 
#define SH_CSS_PORT_FLD_WIDTH_IN_BITS (4)
#define SH_CSS_PORT_TYPE_BIT_FLD(pt) (0x1 << (pt))
#define SH_CSS_PORT_FLD(pd) ((pd) ? SH_CSS_PORT_FLD_WIDTH_IN_BITS : 0)
#define SH_CSS_PIPE_PORT_CONFIG_ON(p, pd, pt) ((p) |= (SH_CSS_PORT_TYPE_BIT_FLD(pt) << SH_CSS_PORT_FLD(pd)))
#define SH_CSS_PIPE_PORT_CONFIG_OFF(p, pd, pt) ((p) &= ~(SH_CSS_PORT_TYPE_BIT_FLD(pt) << SH_CSS_PORT_FLD(pd)))
#define SH_CSS_PIPE_PORT_CONFIG_SET(p, pd, pt, val) ((val) ? \
		SH_CSS_PIPE_PORT_CONFIG_ON(p, pd, pt) : SH_CSS_PIPE_PORT_CONFIG_OFF(p, pd, pt))
#define SH_CSS_PIPE_PORT_CONFIG_GET(p, pd, pt) ((p) & (SH_CSS_PORT_TYPE_BIT_FLD(pt) << SH_CSS_PORT_FLD(pd)))
#define SH_CSS_PIPE_PORT_CONFIG_IS_CONTINUOUS(p) \
	(!(SH_CSS_PIPE_PORT_CONFIG_GET(p, SH_CSS_PORT_INPUT, SH_CSS_HOST_TYPE) && \
	   SH_CSS_PIPE_PORT_CONFIG_GET(p, SH_CSS_PORT_OUTPUT, SH_CSS_HOST_TYPE)))

#define IA_CSS_ACQUIRE_ISP_POS	31

 
#define SH_CSS_METADATA_ENABLED        0x01
#define SH_CSS_METADATA_PROCESSED      0x02
#define SH_CSS_METADATA_OFFLINE_MODE   0x04
#define SH_CSS_METADATA_WAIT_INPUT     0x08

 
void
ia_css_metadata_free_multiple(unsigned int num_bufs,
			      struct ia_css_metadata **bufs);

 
#define QOS_INVALID                  (~0U)

 
struct sh_css_sp_pipeline {
	u32	pipe_id;	 
	u32	pipe_num;	 
	u32	thread_id;	 
	u32	pipe_config;	 
	u32	pipe_qos_config;	 
	u32	inout_port_config;
	u32	required_bds_factor;
	u32	dvs_frame_delay;
	u32	input_system_mode;	 
	u32	port_id;	 
	u32	num_stages;		 
	u32	running;	 
	ia_css_ptr	sp_stage_addr[SH_CSS_MAX_STAGES];
	ia_css_ptr	scaler_pp_lut;  
	u32	dummy;  
	s32 num_execs;  
	struct {
		u32        format;    
		u32        width;     
		u32        height;    
		u32        stride;    
		u32        size;      
		ia_css_ptr    cont_buf;  
	} metadata;
	u32	output_frame_queue_id;
	union {
		struct {
			u32	bytes_available;
		} bin;
		struct {
			u32	height;
			u32	width;
			u32	padded_width;
			u32	max_input_width;
			u32	raw_bit_depth;
		} raw;
	} copy;
};

 
#define SH_CSS_NUM_DYNAMIC_FRAME_IDS (3)

struct ia_css_frames_sp {
	struct ia_css_frame_sp	in;
	struct ia_css_frame_sp	out[IA_CSS_BINARY_MAX_OUTPUT_PORTS];
	struct ia_css_resolution effective_in_res;
	struct ia_css_frame_sp	out_vf;
	struct ia_css_frame_sp_info internal_frame_info;
	struct ia_css_buffer_sp s3a_buf;
	struct ia_css_buffer_sp dvs_buf;
	struct ia_css_buffer_sp metadata_buf;
};

 
struct sh_css_isp_stage {
	 
	struct ia_css_blob_info	  blob_info;
	struct ia_css_binary_info binary_info;
	char			  binary_name[SH_CSS_MAX_BINARY_NAME];
	struct ia_css_isp_param_css_segments mem_initializers;
};

 
struct sh_css_sp_stage {
	 
	u8			num;  
	u8			isp_online;
	u8			isp_copy_vf;
	u8			isp_copy_output;
	u8			sp_enable_xnr;
	u8			isp_deci_log_factor;
	u8			isp_vf_downscale_bits;
	u8			deinterleaved;
	 
	u8			program_input_circuit;
	 
	u8			func;
	 
	 
	u8			stage_type;
	u8			num_stripes;
	u8			isp_pipe_version;
	struct {
		u8		vf_output;
		u8		s3a;
		u8		sdis;
		u8		dvs_stats;
		u8		lace_stats;
	} enable;
	 
	 

	struct sh_css_crop_pos		sp_out_crop_pos;
	struct ia_css_frames_sp		frames;
	struct ia_css_resolution	dvs_envelope;
	struct sh_css_uds_info		uds;
	ia_css_ptr			isp_stage_addr;
	ia_css_ptr			xmem_bin_addr;
	ia_css_ptr			xmem_map_addr;

	u16		top_cropping;
	u16		row_stripes_height;
	u16		row_stripes_overlap_lines;
	u8			if_config_index;  
};

 
struct sh_css_sp_group {
	struct sh_css_sp_config		config;
	struct sh_css_sp_pipeline	pipe[SH_CSS_MAX_SP_THREADS];
#if defined(ISP2401)
	struct sh_css_sp_pipeline_io	pipe_io[SH_CSS_MAX_SP_THREADS];
	struct sh_css_sp_pipeline_io_status	pipe_io_status;
#endif
	struct sh_css_sp_debug_command	debug;
};

 
struct sh_css_sp_per_frame_data {
	 
	ia_css_ptr			sp_group_addr;
};

#define SH_CSS_NUM_SDW_IRQS 3

 
struct sh_css_sp_output {
	unsigned int			bin_copy_bytes_copied;
#if SP_DEBUG != SP_DEBUG_NONE
	struct sh_css_sp_debug_state	debug;
#endif
	unsigned int		sw_interrupt_value[SH_CSS_NUM_SDW_IRQS];
};

 
 

#define  IA_CSS_NUM_ELEMS_HOST2SP_BUFFER_QUEUE    6
#define  IA_CSS_NUM_ELEMS_HOST2SP_PARAM_QUEUE    3
#define  IA_CSS_NUM_ELEMS_HOST2SP_TAG_CMD_QUEUE  6

 
#define  IA_CSS_NUM_ELEMS_SP2HOST_ISYS_EVENT_QUEUE (2 * N_CSI_PORTS)
 
#define  IA_CSS_NUM_ELEMS_HOST2SP_ISYS_EVENT_QUEUE (2 * N_CSI_PORTS)

#define  IA_CSS_NUM_ELEMS_HOST2SP_PSYS_EVENT_QUEUE    13
#define  IA_CSS_NUM_ELEMS_SP2HOST_BUFFER_QUEUE        19
#define  IA_CSS_NUM_ELEMS_SP2HOST_PSYS_EVENT_QUEUE    26  

struct sh_css_hmm_buffer {
	union {
		struct ia_css_isp_3a_statistics  s3a;
		struct ia_css_isp_dvs_statistics dis;
		ia_css_ptr skc_dvs_statistics;
		ia_css_ptr lace_stat;
		struct ia_css_metadata	metadata;
		struct frame_data_wrapper {
			ia_css_ptr	frame_data;
			u32	flashed;
			u32	exp_id;
			u32	isp_parameters_id;  
		} frame;
		ia_css_ptr ddr_ptrs;
	} payload;
	 
	CSS_ALIGN(u64 cookie_ptr, 8);  
	u64 kernel_ptr;
	struct ia_css_time_meas timing_data;
	clock_value_t isys_eof_clock_tick;
};

#define SIZE_OF_FRAME_STRUCT						\
	(SIZE_OF_HRT_VADDRESS +						\
	(3 * sizeof(uint32_t)))

#define SIZE_OF_PAYLOAD_UNION						\
	(MAX(MAX(MAX(MAX(						\
	SIZE_OF_IA_CSS_ISP_3A_STATISTICS_STRUCT,			\
	SIZE_OF_IA_CSS_ISP_DVS_STATISTICS_STRUCT),			\
	SIZE_OF_IA_CSS_METADATA_STRUCT),				\
	SIZE_OF_FRAME_STRUCT),						\
	SIZE_OF_HRT_VADDRESS))

 
#define SIZE_OF_SH_CSS_HMM_BUFFER_STRUCT				\
	(SIZE_OF_PAYLOAD_UNION +					\
	CALC_ALIGNMENT_MEMBER(SIZE_OF_PAYLOAD_UNION, 8) +		\
	8 +						\
	8 +						\
	SIZE_OF_IA_CSS_TIME_MEAS_STRUCT +				\
	SIZE_OF_IA_CSS_CLOCK_TICK_STRUCT +			\
	CALC_ALIGNMENT_MEMBER(SIZE_OF_IA_CSS_CLOCK_TICK_STRUCT, 8))

enum sh_css_queue_type {
	sh_css_invalid_queue_type = -1,
	sh_css_host2sp_buffer_queue,
	sh_css_sp2host_buffer_queue,
	sh_css_host2sp_psys_event_queue,
	sh_css_sp2host_psys_event_queue,
	sh_css_sp2host_isys_event_queue,
	sh_css_host2sp_isys_event_queue,
	sh_css_host2sp_tag_cmd_queue,
};

struct sh_css_event_irq_mask {
	u16 or_mask;
	u16 and_mask;
};

#define SIZE_OF_SH_CSS_EVENT_IRQ_MASK_STRUCT				\
	(2 * sizeof(uint16_t))

struct host_sp_communication {
	 
	u32 host2sp_command;

	 
	ia_css_ptr host2sp_offline_frames[NUM_CONTINUOUS_FRAMES];
	ia_css_ptr host2sp_offline_metadata[NUM_CONTINUOUS_FRAMES];

	ia_css_ptr host2sp_mipi_frames[N_CSI_PORTS][NUM_MIPI_FRAMES_PER_STREAM];
	ia_css_ptr host2sp_mipi_metadata[N_CSI_PORTS][NUM_MIPI_FRAMES_PER_STREAM];
	u32 host2sp_num_mipi_frames[N_CSI_PORTS];
	u32 host2sp_cont_avail_num_raw_frames;
	u32 host2sp_cont_extra_num_raw_frames;
	u32 host2sp_cont_target_num_raw_frames;
	struct sh_css_event_irq_mask host2sp_event_irq_mask[NR_OF_PIPELINES];

};

#define SIZE_OF_HOST_SP_COMMUNICATION_STRUCT				\
	(sizeof(uint32_t) +						\
	(NUM_CONTINUOUS_FRAMES * SIZE_OF_HRT_VADDRESS * 2) +		\
	(N_CSI_PORTS * NUM_MIPI_FRAMES_PER_STREAM * SIZE_OF_HRT_VADDRESS * 2) +			\
	((3 + N_CSI_PORTS) * sizeof(uint32_t)) +						\
	(NR_OF_PIPELINES * SIZE_OF_SH_CSS_EVENT_IRQ_MASK_STRUCT))

struct host_sp_queues {
	 
	ia_css_circbuf_desc_t host2sp_buffer_queues_desc
	[SH_CSS_MAX_SP_THREADS][SH_CSS_MAX_NUM_QUEUES];
	ia_css_circbuf_elem_t host2sp_buffer_queues_elems
	[SH_CSS_MAX_SP_THREADS][SH_CSS_MAX_NUM_QUEUES]
	[IA_CSS_NUM_ELEMS_HOST2SP_BUFFER_QUEUE];
	ia_css_circbuf_desc_t sp2host_buffer_queues_desc
	[SH_CSS_MAX_NUM_QUEUES];
	ia_css_circbuf_elem_t sp2host_buffer_queues_elems
	[SH_CSS_MAX_NUM_QUEUES][IA_CSS_NUM_ELEMS_SP2HOST_BUFFER_QUEUE];

	 
	ia_css_circbuf_desc_t host2sp_psys_event_queue_desc;

	ia_css_circbuf_elem_t host2sp_psys_event_queue_elems
	[IA_CSS_NUM_ELEMS_HOST2SP_PSYS_EVENT_QUEUE];
	ia_css_circbuf_desc_t sp2host_psys_event_queue_desc;

	ia_css_circbuf_elem_t sp2host_psys_event_queue_elems
	[IA_CSS_NUM_ELEMS_SP2HOST_PSYS_EVENT_QUEUE];

	 
	ia_css_circbuf_desc_t host2sp_isys_event_queue_desc;

	ia_css_circbuf_elem_t host2sp_isys_event_queue_elems
	[IA_CSS_NUM_ELEMS_HOST2SP_ISYS_EVENT_QUEUE];
	ia_css_circbuf_desc_t sp2host_isys_event_queue_desc;

	ia_css_circbuf_elem_t sp2host_isys_event_queue_elems
	[IA_CSS_NUM_ELEMS_SP2HOST_ISYS_EVENT_QUEUE];
	 
	ia_css_circbuf_desc_t host2sp_tag_cmd_queue_desc;

	ia_css_circbuf_elem_t host2sp_tag_cmd_queue_elems
	[IA_CSS_NUM_ELEMS_HOST2SP_TAG_CMD_QUEUE];
};

#define SIZE_OF_QUEUES_ELEMS							\
	(SIZE_OF_IA_CSS_CIRCBUF_ELEM_S_STRUCT *				\
	((SH_CSS_MAX_SP_THREADS * SH_CSS_MAX_NUM_QUEUES * IA_CSS_NUM_ELEMS_HOST2SP_BUFFER_QUEUE) + \
	(SH_CSS_MAX_NUM_QUEUES * IA_CSS_NUM_ELEMS_SP2HOST_BUFFER_QUEUE) +	\
	(IA_CSS_NUM_ELEMS_HOST2SP_PSYS_EVENT_QUEUE) +				\
	(IA_CSS_NUM_ELEMS_SP2HOST_PSYS_EVENT_QUEUE) +				\
	(IA_CSS_NUM_ELEMS_HOST2SP_ISYS_EVENT_QUEUE) +				\
	(IA_CSS_NUM_ELEMS_SP2HOST_ISYS_EVENT_QUEUE) +				\
	(IA_CSS_NUM_ELEMS_HOST2SP_TAG_CMD_QUEUE)))

#define IA_CSS_NUM_CIRCBUF_DESCS 5

#define SIZE_OF_QUEUES_DESC \
	((SH_CSS_MAX_SP_THREADS * SH_CSS_MAX_NUM_QUEUES * \
	  SIZE_OF_IA_CSS_CIRCBUF_DESC_S_STRUCT) + \
	 (SH_CSS_MAX_NUM_QUEUES * SIZE_OF_IA_CSS_CIRCBUF_DESC_S_STRUCT) + \
	 (IA_CSS_NUM_CIRCBUF_DESCS * SIZE_OF_IA_CSS_CIRCBUF_DESC_S_STRUCT))

#define SIZE_OF_HOST_SP_QUEUES_STRUCT		\
	(SIZE_OF_QUEUES_ELEMS + SIZE_OF_QUEUES_DESC)

extern int  __printf(1, 0) (*sh_css_printf)(const char *fmt, va_list args);

static inline void  __printf(1, 2) sh_css_print(const char *fmt, ...)
{
	va_list ap;

	if (sh_css_printf) {
		va_start(ap, fmt);
		sh_css_printf(fmt, ap);
		va_end(ap);
	}
}

static inline void  __printf(1, 0) sh_css_vprint(const char *fmt, va_list args)
{
	if (sh_css_printf)
		sh_css_printf(fmt, args);
}

 
ia_css_ptr sh_css_params_ddr_address_map(void);

int
sh_css_params_init(void);

void
sh_css_params_uninit(void);

void
sh_css_binary_args_reset(struct sh_css_binary_args *args);

 
bool
sh_css_frame_equal_types(const struct ia_css_frame *frame_a,
			 const struct ia_css_frame *frame_b);

bool
sh_css_frame_info_equal_resolution(const struct ia_css_frame_info *info_a,
				   const struct ia_css_frame_info *info_b);

void
sh_css_capture_enable_bayer_downscaling(bool enable);

void
sh_css_binary_print(const struct ia_css_binary *binary);

 
void
sh_css_frame_info_set_width(struct ia_css_frame_info *info,
			    unsigned int width,
			    unsigned int aligned);

#if !defined(ISP2401)

unsigned int
sh_css_get_mipi_sizes_for_check(const unsigned int port,
				const unsigned int idx);

#endif

ia_css_ptr
sh_css_store_sp_group_to_ddr(void);

ia_css_ptr
sh_css_store_sp_stage_to_ddr(unsigned int pipe, unsigned int stage);

ia_css_ptr
sh_css_store_isp_stage_to_ddr(unsigned int pipe, unsigned int stage);

void
sh_css_update_uds_and_crop_info(
    const struct ia_css_binary_info *info,
    const struct ia_css_frame_info *in_frame_info,
    const struct ia_css_frame_info *out_frame_info,
    const struct ia_css_resolution *dvs_env,
    const struct ia_css_dz_config *zoom,
    const struct ia_css_vector *motion_vector,
    struct sh_css_uds_info *uds,		 
    struct sh_css_crop_pos *sp_out_crop_pos,	 

    bool enable_zoom
);

void
sh_css_invalidate_shading_tables(struct ia_css_stream *stream);

struct ia_css_pipeline *
ia_css_pipe_get_pipeline(const struct ia_css_pipe *pipe);

unsigned int
ia_css_pipe_get_pipe_num(const struct ia_css_pipe *pipe);

unsigned int
ia_css_pipe_get_isp_pipe_version(const struct ia_css_pipe *pipe);

bool
sh_css_continuous_is_enabled(uint8_t pipe_num);

struct ia_css_pipe *
find_pipe_by_num(uint32_t pipe_num);

#ifdef ISP2401
void
ia_css_get_crop_offsets(
    struct ia_css_pipe *pipe,
    struct ia_css_frame_info *in_frame);
#endif

#endif  
