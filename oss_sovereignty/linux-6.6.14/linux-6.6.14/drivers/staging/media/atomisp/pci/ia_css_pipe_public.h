#ifndef __IA_CSS_PIPE_PUBLIC_H
#define __IA_CSS_PIPE_PUBLIC_H
#include <type_support.h>
#include <ia_css_err.h>
#include <ia_css_types.h>
#include <ia_css_frame_public.h>
#include <ia_css_buffer.h>
#include <ia_css_acc_types.h>
enum {
	IA_CSS_PIPE_OUTPUT_STAGE_0 = 0,
	IA_CSS_PIPE_OUTPUT_STAGE_1,
	IA_CSS_PIPE_MAX_OUTPUT_STAGE,
};
enum ia_css_pipe_mode {
	IA_CSS_PIPE_MODE_PREVIEW,	 
	IA_CSS_PIPE_MODE_VIDEO,		 
	IA_CSS_PIPE_MODE_CAPTURE,	 
	IA_CSS_PIPE_MODE_COPY,		 
	IA_CSS_PIPE_MODE_YUVPP,		 
};
#define IA_CSS_PIPE_MODE_NUM (IA_CSS_PIPE_MODE_YUVPP + 1)
enum ia_css_pipe_version {
	IA_CSS_PIPE_VERSION_1 = 1,		 
	IA_CSS_PIPE_VERSION_2_2 = 2,		 
	IA_CSS_PIPE_VERSION_2_6_1 = 3,		 
	IA_CSS_PIPE_VERSION_2_7 = 4		 
};
struct ia_css_pipe_config {
	enum ia_css_pipe_mode mode;
	enum ia_css_pipe_version isp_pipe_version;
	struct ia_css_resolution input_effective_res;
	struct ia_css_resolution bayer_ds_out_res;
	struct ia_css_resolution capt_pp_in_res;
	struct ia_css_resolution vf_pp_in_res;
	struct ia_css_resolution output_system_in_res;
	struct ia_css_resolution dvs_crop_out_res;
	struct ia_css_frame_info output_info[IA_CSS_PIPE_MAX_OUTPUT_STAGE];
	struct ia_css_frame_info vf_output_info[IA_CSS_PIPE_MAX_OUTPUT_STAGE];
	struct ia_css_capture_config default_capture_config;
	struct ia_css_resolution dvs_envelope;  
	enum ia_css_frame_delay dvs_frame_delay;
	bool enable_dz;
	bool enable_dpc;
	bool enable_vfpp_bci;
	bool enable_tnr;
	struct ia_css_isp_config *p_isp_config;
	struct ia_css_resolution gdc_in_buffer_res;
	struct ia_css_point gdc_in_buffer_offset;
	struct ia_css_coordinate internal_frame_origin_bqs_on_sctbl;
};
#define DEFAULT_PIPE_CONFIG { \
	.mode			= IA_CSS_PIPE_MODE_PREVIEW, \
	.isp_pipe_version	= 1, \
	.output_info		= {IA_CSS_BINARY_DEFAULT_FRAME_INFO}, \
	.vf_output_info		= {IA_CSS_BINARY_DEFAULT_FRAME_INFO}, \
	.default_capture_config	= DEFAULT_CAPTURE_CONFIG, \
	.dvs_frame_delay	= IA_CSS_FRAME_DELAY_1, \
}
struct ia_css_pipe_info {
	struct ia_css_frame_info output_info[IA_CSS_PIPE_MAX_OUTPUT_STAGE];
	struct ia_css_frame_info vf_output_info[IA_CSS_PIPE_MAX_OUTPUT_STAGE];
	struct ia_css_frame_info raw_output_info;
	struct ia_css_resolution output_system_in_res_info;
	struct ia_css_shading_info shading_info;
	struct ia_css_grid_info  grid_info;
	int num_invalid_frames;
};
#define DEFAULT_PIPE_INFO {\
	.output_info		= {IA_CSS_BINARY_DEFAULT_FRAME_INFO}, \
	.vf_output_info		= {IA_CSS_BINARY_DEFAULT_FRAME_INFO}, \
	.raw_output_info	= IA_CSS_BINARY_DEFAULT_FRAME_INFO, \
	.shading_info		= DEFAULT_SHADING_INFO, \
	.grid_info		= DEFAULT_GRID_INFO, \
}
void ia_css_pipe_config_defaults(struct ia_css_pipe_config *pipe_config);
int
ia_css_pipe_create(const struct ia_css_pipe_config *config,
		   struct ia_css_pipe **pipe);
int
ia_css_pipe_destroy(struct ia_css_pipe *pipe);
int
ia_css_pipe_get_info(const struct ia_css_pipe *pipe,
		     struct ia_css_pipe_info *pipe_info);
int
ia_css_pipe_set_isp_config(struct ia_css_pipe *pipe,
			   struct ia_css_isp_config *config);
int
ia_css_pipe_set_irq_mask(struct ia_css_pipe *pipe,
			 unsigned int or_mask,
			 unsigned int and_mask);
int
ia_css_event_get_irq_mask(const struct ia_css_pipe *pipe,
			  unsigned int *or_mask,
			  unsigned int *and_mask);
int
ia_css_pipe_enqueue_buffer(struct ia_css_pipe *pipe,
			   const struct ia_css_buffer *buffer);
int
ia_css_pipe_dequeue_buffer(struct ia_css_pipe *pipe,
			   struct ia_css_buffer *buffer);
void
ia_css_pipe_get_isp_config(struct ia_css_pipe *pipe,
			   struct ia_css_isp_config *config);
int
ia_css_pipe_set_bci_scaler_lut(struct ia_css_pipe *pipe,
			       const void *lut);
bool ia_css_pipe_has_dvs_stats(struct ia_css_pipe_info *pipe_info);
int
ia_css_pipe_override_frame_format(struct ia_css_pipe *pipe,
				  int output_pin,
				  enum ia_css_frame_format format);
#endif  
