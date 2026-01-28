#ifndef __IPU3_CSS_FW_H
#define __IPU3_CSS_FW_H
#define IMGU_FW_NAME		"intel/ipu3-fw.bin"
#define IMGU_FW_NAME_20161208	\
	"intel/irci_irci_ecr-master_20161208_0213_20170112_1500.bin"
typedef u32 imgu_fw_ptr;
enum imgu_fw_type {
	IMGU_FW_SP_FIRMWARE,	 
	IMGU_FW_SP1_FIRMWARE,	 
	IMGU_FW_ISP_FIRMWARE,	 
	IMGU_FW_BOOTLOADER_FIRMWARE,	 
	IMGU_FW_ACC_FIRMWARE	 
};
enum imgu_fw_acc_type {
	IMGU_FW_ACC_NONE,	 
	IMGU_FW_ACC_OUTPUT,	 
	IMGU_FW_ACC_VIEWFINDER,	 
	IMGU_FW_ACC_STANDALONE,	 
};
struct imgu_fw_isp_parameter {
	u32 offset;		 
	u32 size;		 
};
struct imgu_fw_param_memory_offsets {
	struct {
		struct imgu_fw_isp_parameter lin;	 
		struct imgu_fw_isp_parameter tnr3;	 
		struct imgu_fw_isp_parameter xnr3;	 
	} vmem;
	struct {
		struct imgu_fw_isp_parameter tnr;
		struct imgu_fw_isp_parameter tnr3;	 
		struct imgu_fw_isp_parameter xnr3;	 
		struct imgu_fw_isp_parameter plane_io_config;	 
		struct imgu_fw_isp_parameter rgbir;	 
	} dmem;
};
struct imgu_fw_config_memory_offsets {
	struct {
		struct imgu_fw_isp_parameter iterator;
		struct imgu_fw_isp_parameter dvs;
		struct imgu_fw_isp_parameter output;
		struct imgu_fw_isp_parameter raw;
		struct imgu_fw_isp_parameter input_yuv;
		struct imgu_fw_isp_parameter tnr;
		struct imgu_fw_isp_parameter tnr3;
		struct imgu_fw_isp_parameter ref;
	} dmem;
};
struct imgu_fw_state_memory_offsets {
	struct {
		struct imgu_fw_isp_parameter tnr;
		struct imgu_fw_isp_parameter tnr3;
		struct imgu_fw_isp_parameter ref;
	} dmem;
};
union imgu_fw_all_memory_offsets {
	struct {
		u64 imgu_fw_mem_offsets[3];  
	} offsets;
	struct {
		u64 ptr;
	} array[IMGU_ABI_PARAM_CLASS_NUM];
};
struct imgu_fw_binary_xinfo {
	struct imgu_abi_binary_info sp;
	u32 type;	 
	u32 num_output_formats __aligned(8);
	u32 output_formats[IMGU_ABI_FRAME_FORMAT_NUM];	 
	u32 num_vf_formats __aligned(8);
	u32 vf_formats[IMGU_ABI_FRAME_FORMAT_NUM];	 
	u8 num_output_pins;
	imgu_fw_ptr xmem_addr;
	u64 imgu_fw_blob_descr_ptr __aligned(8);
	u32 blob_index __aligned(8);
	union imgu_fw_all_memory_offsets mem_offsets __aligned(8);
	struct imgu_fw_binary_xinfo *next __aligned(8);
};
struct imgu_fw_sp_info {
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
	u32 debug_buffer_ddr_address;	 
	u32 perf_counter_input_system_error;
	u32 threads_stack;	 
	u32 threads_stack_size;	 
	u32 curr_binary_id;	 
	u32 raw_copy_line_count;	 
	u32 ddr_parameter_address;	 
	u32 ddr_parameter_size;	 
	u32 sp_entry;		 
	u32 tagger_frames_addr;	 
};
struct imgu_fw_bl_info {
	u32 num_dma_cmds;	 
	u32 dma_cmd_list;	 
	u32 sw_state;		 
	u32 bl_entry;		 
};
struct imgu_fw_acc_info {
	u32 per_frame_data;	 
};
union imgu_fw_union {
	struct imgu_fw_binary_xinfo isp;	 
	struct imgu_fw_sp_info sp;	 
	struct imgu_fw_sp_info sp1;	 
	struct imgu_fw_bl_info bl;	 
	struct imgu_fw_acc_info acc;	 
};
struct imgu_fw_info {
	size_t header_size;	 
	u32 type __aligned(8);	 
	union imgu_fw_union info;	 
	struct imgu_abi_blob_info blob;	 
	u64 next;
	u32 loaded __aligned(8);	 
	const u64 isp_code __aligned(8);	 
	u32 handle __aligned(8);
	struct imgu_abi_isp_param_segments mem_initializers;
};
struct imgu_fw_bi_file_h {
	char version[64];	 
	int binary_nr;		 
	unsigned int h_size;	 
};
struct imgu_fw_header {
	struct imgu_fw_bi_file_h file_header;
	struct imgu_fw_info binary_header[];	 
};
int imgu_css_fw_init(struct imgu_css *css);
void imgu_css_fw_cleanup(struct imgu_css *css);
unsigned int imgu_css_fw_obgrid_size(const struct imgu_fw_info *bi);
void *imgu_css_fw_pipeline_params(struct imgu_css *css, unsigned int pipe,
				  enum imgu_abi_param_class cls,
				  enum imgu_abi_memories mem,
				  struct imgu_fw_isp_parameter *par,
				  size_t par_size, void *binary_params);
#endif
