#ifndef __IA_CSS_PIPELINE_H__
#define __IA_CSS_PIPELINE_H__
#include "sh_css_internal.h"
#include "ia_css_pipe_public.h"
#include "ia_css_pipeline_common.h"
#define IA_CSS_PIPELINE_NUM_MAX		(20)
struct ia_css_pipeline_stage {
	unsigned int stage_num;
	struct ia_css_binary *binary;	 
	struct ia_css_binary_info *binary_info;
	const struct ia_css_fw_info *firmware;	 
	enum ia_css_pipeline_stage_sp_func sp_func;
	unsigned int max_input_width;	 
	struct sh_css_binary_args args;
	int mode;
	bool out_frame_allocated[IA_CSS_BINARY_MAX_OUTPUT_PORTS];
	bool vf_frame_allocated;
	struct ia_css_pipeline_stage *next;
	bool enable_zoom;
};
struct ia_css_pipeline {
	enum ia_css_pipe_id pipe_id;
	u8 pipe_num;
	bool stop_requested;
	struct ia_css_pipeline_stage *stages;
	struct ia_css_pipeline_stage *current_stage;
	unsigned int num_stages;
	struct ia_css_frame in_frame;
	struct ia_css_frame out_frame[IA_CSS_PIPE_MAX_OUTPUT_STAGE];
	struct ia_css_frame vf_frame[IA_CSS_PIPE_MAX_OUTPUT_STAGE];
	unsigned int dvs_frame_delay;
	unsigned int inout_port_config;
	int num_execs;
	bool acquire_isp_each_stage;
};
#define DEFAULT_PIPELINE { \
	.pipe_id		= IA_CSS_PIPE_ID_PREVIEW, \
	.in_frame		= DEFAULT_FRAME, \
	.out_frame		= {DEFAULT_FRAME}, \
	.vf_frame		= {DEFAULT_FRAME}, \
	.dvs_frame_delay	= IA_CSS_FRAME_DELAY_1, \
	.num_execs		= -1, \
	.acquire_isp_each_stage	= true, \
}
struct ia_css_pipeline_stage_desc {
	struct ia_css_binary *binary;
	const struct ia_css_fw_info *firmware;
	enum ia_css_pipeline_stage_sp_func sp_func;
	unsigned int max_input_width;
	unsigned int mode;
	struct ia_css_frame *in_frame;
	struct ia_css_frame *out_frame[IA_CSS_BINARY_MAX_OUTPUT_PORTS];
	struct ia_css_frame *vf_frame;
};
void ia_css_pipeline_init(void);
int ia_css_pipeline_create(
    struct ia_css_pipeline *pipeline,
    enum ia_css_pipe_id pipe_id,
    unsigned int pipe_num,
    unsigned int dvs_frame_delay);
void ia_css_pipeline_destroy(struct ia_css_pipeline *pipeline);
void ia_css_pipeline_start(enum ia_css_pipe_id pipe_id,
			   struct ia_css_pipeline *pipeline);
int ia_css_pipeline_request_stop(struct ia_css_pipeline *pipeline);
bool ia_css_pipeline_has_stopped(struct ia_css_pipeline *pipe);
void ia_css_pipeline_clean(struct ia_css_pipeline *pipeline);
int ia_css_pipeline_create_and_add_stage(
    struct ia_css_pipeline *pipeline,
    struct ia_css_pipeline_stage_desc *stage_desc,
    struct ia_css_pipeline_stage **stage);
void ia_css_pipeline_finalize_stages(struct ia_css_pipeline *pipeline,
				     bool continuous);
int ia_css_pipeline_get_stage(struct ia_css_pipeline *pipeline,
	int mode,
	struct ia_css_pipeline_stage **stage);
int ia_css_pipeline_get_stage_from_fw(struct ia_css_pipeline
	*pipeline,
	u32 fw_handle,
	struct ia_css_pipeline_stage **stage);
int ia_css_pipeline_get_fw_from_stage(struct ia_css_pipeline
	*pipeline,
	u32 stage_num,
	uint32_t *fw_handle);
int ia_css_pipeline_get_output_stage(
    struct ia_css_pipeline *pipeline,
    int mode,
    struct ia_css_pipeline_stage **stage);
bool ia_css_pipeline_uses_params(struct ia_css_pipeline *pipeline);
bool ia_css_pipeline_get_sp_thread_id(unsigned int key, unsigned int *val);
#if defined(ISP2401)
struct sh_css_sp_pipeline_io_status *ia_css_pipeline_get_pipe_io_status(void);
#endif
void ia_css_pipeline_map(unsigned int pipe_num, bool map);
bool ia_css_pipeline_is_mapped(unsigned int key);
void ia_css_pipeline_dump_thread_map_info(void);
#endif  
