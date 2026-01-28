#ifndef _IA_CSS_ACC_TYPES_H
#define _IA_CSS_ACC_TYPES_H
#include <system_local.h>	 
#include <type_support.h>
#include <platform_support.h>
#include <debug_global.h>
#include <linux/bits.h>
#include "ia_css_types.h"
#include "ia_css_frame_format.h"
#include "runtime/isp_param/interface/ia_css_isp_param_types.h"
enum ia_css_acc_type {
	IA_CSS_ACC_NONE,	 
	IA_CSS_ACC_OUTPUT,	 
	IA_CSS_ACC_VIEWFINDER,	 
	IA_CSS_ACC_STANDALONE,	 
};
enum ia_css_cell_type {
	IA_CSS_SP0 = 0,
	IA_CSS_SP1,
	IA_CSS_ISP,
	MAX_NUM_OF_CELLS
};
enum ia_css_fw_type {
	ia_css_sp_firmware,		 
	ia_css_isp_firmware,		 
	ia_css_bootloader_firmware,	 
	ia_css_acc_firmware		 
};
struct ia_css_blob_descr;
struct ia_css_blob_info {
	u32 offset;		 
	struct ia_css_isp_param_memory_offsets
		memory_offsets;   
	u32 prog_name_offset;   
	u32 size;			 
	u32 padding_size;	 
	u32 icache_source;	 
	u32 icache_size;	 
	u32 icache_padding; 
	u32 text_source;	 
	u32 text_size;		 
	u32 text_padding;	 
	u32 data_source;	 
	u32 data_target;	 
	u32 data_size;		 
	u32 data_padding;	 
	u32 bss_target;	 
	u32 bss_size;		 
	CSS_ALIGN(const void  *code,
		  8);		 
	CSS_ALIGN(const void  *data,
		  8);		 
};
struct ia_css_binary_input_info {
	u32		min_width;
	u32		min_height;
	u32		max_width;
	u32		max_height;
	u32		source;  
};
struct ia_css_binary_output_info {
	u32		min_width;
	u32		min_height;
	u32		max_width;
	u32		max_height;
	u32		num_chunks;
	u32		variable_format;
};
struct ia_css_binary_internal_info {
	u32		max_width;
	u32		max_height;
};
struct ia_css_binary_bds_info {
	u32		supported_bds_factors;
};
struct ia_css_binary_dvs_info {
	u32		max_envelope_width;
	u32		max_envelope_height;
};
struct ia_css_binary_vf_dec_info {
	u32		is_variable;
	u32		max_log_downscale;
};
struct ia_css_binary_s3a_info {
	u32		s3atbl_use_dmem;
	u32		fixed_s3a_deci_log;
};
struct ia_css_binary_dpc_info {
	u32		bnr_lite;  
};
struct ia_css_binary_iterator_info {
	u32		num_stripes;
	u32		row_stripes_height;
	u32		row_stripes_overlap_lines;
};
struct ia_css_binary_address_info {
	u32		isp_addresses;	 
	u32		main_entry;	 
	u32		in_frame;	 
	u32		out_frame;	 
	u32		in_data;	 
	u32		out_data;	 
	u32		sh_dma_cmd_ptr;      
};
struct ia_css_binary_uds_info {
	u16	bpp;
	u16	use_bci;
	u16	use_str;
	u16	woix;
	u16	woiy;
	u16	extra_out_vecs;
	u16	vectors_per_line_in;
	u16	vectors_per_line_out;
	u16	vectors_c_per_line_in;
	u16	vectors_c_per_line_out;
	u16	vmem_gdc_in_block_height_y;
	u16	vmem_gdc_in_block_height_c;
};
struct ia_css_binary_pipeline_info {
	u32	mode;
	u32	isp_pipe_version;
	u32	pipelining;
	u32	c_subsampling;
	u32	top_cropping;
	u32	left_cropping;
	u32	variable_resolution;
};
struct ia_css_binary_block_info {
	u32	block_width;
	u32	block_height;
	u32	output_block_height;
};
struct ia_css_binary_info {
	CSS_ALIGN(u32			id, 8);  
	struct ia_css_binary_pipeline_info	pipeline;
	struct ia_css_binary_input_info		input;
	struct ia_css_binary_output_info	output;
	struct ia_css_binary_internal_info	internal;
	struct ia_css_binary_bds_info		bds;
	struct ia_css_binary_dvs_info		dvs;
	struct ia_css_binary_vf_dec_info	vf_dec;
	struct ia_css_binary_s3a_info		s3a;
	struct ia_css_binary_dpc_info		dpc_bnr;  
	struct ia_css_binary_iterator_info	iterator;
	struct ia_css_binary_address_info	addresses;
	struct ia_css_binary_uds_info		uds;
	struct ia_css_binary_block_info		block;
	struct ia_css_isp_param_isp_segments	mem_initializers;
	struct {
		u8	reduced_pipe;
		u8	vf_veceven;
		u8	dis;
		u8	dvs_envelope;
		u8	uds;
		u8	dvs_6axis;
		u8	block_output;
		u8	streaming_dma;
		u8	ds;
		u8	bayer_fir_6db;
		u8	raw_binning;
		u8	continuous;
		u8	s3a;
		u8	fpnr;
		u8	sc;
		u8	macc;
		u8	output;
		u8	ref_frame;
		u8	tnr;
		u8	xnr;
		u8	params;
		u8	ca_gdc;
		u8	isp_addresses;
		u8	in_frame;
		u8	out_frame;
		u8	high_speed;
		u8	dpc;
		u8 padding[2];
	} enable;
	struct {
		u8	ref_y_channel;
		u8	ref_c_channel;
		u8	tnr_channel;
		u8	tnr_out_channel;
		u8	dvs_coords_channel;
		u8	output_channel;
		u8	c_channel;
		u8	vfout_channel;
		u8	vfout_c_channel;
		u8	vfdec_bits_per_pixel;
		u8	claimed_by_isp;
		u8 padding[2];
	} dma;
};
struct ia_css_binary_xinfo {
	struct ia_css_binary_info    sp;
	enum ia_css_acc_type	     type;
	CSS_ALIGN(s32	     num_output_formats, 8);
	enum ia_css_frame_format     output_formats[IA_CSS_FRAME_FORMAT_NUM];
	CSS_ALIGN(s32	     num_vf_formats, 8);  
	enum ia_css_frame_format
	vf_formats[IA_CSS_FRAME_FORMAT_NUM];  
	u8			     num_output_pins;
	ia_css_ptr		     xmem_addr;
	CSS_ALIGN(const struct ia_css_blob_descr *blob, 8);
	CSS_ALIGN(u32 blob_index, 8);
	CSS_ALIGN(union ia_css_all_memory_offsets mem_offsets, 8);
	CSS_ALIGN(struct ia_css_binary_xinfo *next, 8);
};
struct ia_css_bl_info {
	u32 num_dma_cmds;	 
	u32 dma_cmd_list;	 
	u32 sw_state;	 
	u32 bl_entry;	 
};
struct ia_css_sp_info {
	u32 init_dmem_data;  
	u32 per_frame_data;  
	u32 group;		 
	u32 output;		 
	u32 host_sp_queue;	 
	u32 host_sp_com; 
	u32 isp_started;	 
	u32 sw_state;	 
	u32 host_sp_queues_initialized;  
	u32 sleep_mode;   
	u32 invalidate_tlb;		 
	u32 stop_copy_preview;        
	u32 debug_buffer_ddr_address;	 
	u32 perf_counter_input_system_error;  
#ifdef HAS_WATCHDOG_SP_THREAD_DEBUG
	u32 debug_wait;  
	u32 debug_stage;  
	u32 debug_stripe;  
#endif
	u32 threads_stack;  
	u32 threads_stack_size;  
	u32 curr_binary_id;         
	u32 raw_copy_line_count;    
	u32 ddr_parameter_address;  
	u32 ddr_parameter_size;     
	u32 sp_entry;	 
	u32 tagger_frames_addr;    
};
struct ia_css_acc_info {
	u32 per_frame_data;  
};
union ia_css_fw_union {
	struct ia_css_binary_xinfo	isp;  
	struct ia_css_sp_info		sp;   
	struct ia_css_bl_info           bl;   
	struct ia_css_acc_info		acc;  
};
struct ia_css_fw_info {
	size_t			 header_size;  
	CSS_ALIGN(u32 type, 8);
	union ia_css_fw_union	 info;  
	struct ia_css_blob_info  blob;  
	struct ia_css_fw_info   *next;
	CSS_ALIGN(u32       loaded, 8);	 
	CSS_ALIGN(const u8 *isp_code, 8);   
	CSS_ALIGN(u32	handle, 8);
	struct ia_css_isp_param_css_segments mem_initializers;
};
struct ia_css_blob_descr {
	const unsigned char  *blob;
	struct ia_css_fw_info header;
	const char	     *name;
	union ia_css_all_memory_offsets mem_offsets;
};
struct ia_css_acc_fw;
struct ia_css_acc_sp {
	void (*init)(struct ia_css_acc_fw *);	 
	u32 sp_prog_name_offset;		 
	u32 sp_blob_offset;		 
	void	 *entry;			 
	u32 *css_abort;			 
	void	 *isp_code;			 
	struct ia_css_fw_info fw;		 
	const u8 *code;			 
};
struct ia_css_acc_fw_hdr {
	enum ia_css_acc_type type;	 
	u32	isp_prog_name_offset;  
	u32	isp_blob_offset;       
	u32	isp_size;	       
	const u8  *isp_code;	       
	struct ia_css_acc_sp  sp;   
	u32	handle;
	struct ia_css_data parameters;  
};
struct ia_css_acc_fw {
	struct ia_css_acc_fw_hdr header;  
};
#define IA_CSS_ACC_OFFSET(t, f, n) ((t)((uint8_t *)(f) + (f->header.n)))
#define IA_CSS_ACC_SP_PROG_NAME(f) IA_CSS_ACC_OFFSET(const char *, f, \
						 sp.sp_prog_name_offset)
#define IA_CSS_ACC_ISP_PROG_NAME(f) IA_CSS_ACC_OFFSET(const char *, f, \
						 isp_prog_name_offset)
#define IA_CSS_ACC_SP_CODE(f)      IA_CSS_ACC_OFFSET(uint8_t *, f, \
						 sp.sp_blob_offset)
#define IA_CSS_ACC_SP_DATA(f)      (IA_CSS_ACC_SP_CODE(f) + \
					(f)->header.sp.fw.blob.data_source)
#define IA_CSS_ACC_ISP_CODE(f)     IA_CSS_ACC_OFFSET(uint8_t*, f,\
						 isp_blob_offset)
#define IA_CSS_ACC_ISP_SIZE(f)     ((f)->header.isp_size)
#define IA_CSS_EXT_ISP_PROG_NAME(f)   ((const char *)(f) + (f)->blob.prog_name_offset)
#define IA_CSS_EXT_ISP_MEM_OFFSETS(f) \
	((const struct ia_css_memory_offsets *)((const char *)(f) + (f)->blob.mem_offsets))
enum ia_css_sp_sleep_mode {
	SP_DISABLE_SLEEP_MODE = 0,
	SP_SLEEP_AFTER_FRAME  = BIT(0),
	SP_SLEEP_AFTER_IRQ    = BIT(1),
};
#endif  
