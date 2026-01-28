#ifndef __IA_CSS_STREAM_PUBLIC_H
#define __IA_CSS_STREAM_PUBLIC_H
#include <type_support.h>
#include "ia_css_types.h"
#include "ia_css_pipe_public.h"
#include "ia_css_metadata.h"
#include "ia_css_tpg.h"
#include "ia_css_prbs.h"
#include "ia_css_input_port.h"
enum ia_css_input_mode {
	IA_CSS_INPUT_MODE_SENSOR,  
	IA_CSS_INPUT_MODE_FIFO,    
	IA_CSS_INPUT_MODE_TPG,     
	IA_CSS_INPUT_MODE_PRBS,    
	IA_CSS_INPUT_MODE_MEMORY,  
	IA_CSS_INPUT_MODE_BUFFERED_SENSOR  
};
struct ia_css_mipi_buffer_config {
	unsigned int size_mem_words;  
	bool contiguous;	      
	unsigned int nof_mipi_buffers;  
};
enum {
	IA_CSS_STREAM_ISYS_STREAM_0 = 0,
	IA_CSS_STREAM_DEFAULT_ISYS_STREAM_IDX = IA_CSS_STREAM_ISYS_STREAM_0,
	IA_CSS_STREAM_ISYS_STREAM_1,
	IA_CSS_STREAM_MAX_ISYS_STREAM_PER_CH
};
struct ia_css_stream_isys_stream_config {
	struct ia_css_resolution  input_res;  
	enum atomisp_input_format format;  
	int linked_isys_stream_id;  
	bool valid;  
};
struct ia_css_stream_input_config {
	struct ia_css_resolution  input_res;  
	struct ia_css_resolution  effective_res;  
	enum atomisp_input_format format;  
	enum ia_css_bayer_order bayer_order;  
};
struct ia_css_stream_config {
	enum ia_css_input_mode    mode;  
	union {
		struct ia_css_input_port  port;  
		struct ia_css_tpg_config  tpg;   
		struct ia_css_prbs_config prbs;  
	} source;  
	unsigned int	      channel_id;  
	struct ia_css_stream_isys_stream_config
		isys_config[IA_CSS_STREAM_MAX_ISYS_STREAM_PER_CH];
	struct ia_css_stream_input_config input_config;
	unsigned int sensor_binning_factor;  
	unsigned int pixels_per_clock;  
	bool online;  
	unsigned int init_num_cont_raw_buf;  
	unsigned int target_num_cont_raw_buf;  
	bool pack_raw_pixels;  
	bool continuous;  
	bool disable_cont_viewfinder;  
	s32 flash_gpio_pin;  
	int left_padding;  
	struct ia_css_mipi_buffer_config
		mipi_buffer_config;  
	struct ia_css_metadata_config
		metadata_config;      
	bool ia_css_enable_raw_buffer_locking;  
	bool lock_all;
};
struct ia_css_stream;
struct ia_css_stream_info {
	struct ia_css_metadata_info metadata_info;
};
void ia_css_stream_config_defaults(struct ia_css_stream_config *stream_config);
int
ia_css_stream_create(const struct ia_css_stream_config *stream_config,
		     int num_pipes,
		     struct ia_css_pipe *pipes[],
		     struct ia_css_stream **stream);
int
ia_css_stream_destroy(struct ia_css_stream *stream);
int
ia_css_stream_get_info(const struct ia_css_stream *stream,
		       struct ia_css_stream_info *stream_info);
int
ia_css_stream_start(struct ia_css_stream *stream);
int
ia_css_stream_stop(struct ia_css_stream *stream);
bool
ia_css_stream_has_stopped(struct ia_css_stream *stream);
int
ia_css_stream_unload(struct ia_css_stream *stream);
enum atomisp_input_format
ia_css_stream_get_format(const struct ia_css_stream *stream);
bool
ia_css_stream_get_two_pixels_per_clock(const struct ia_css_stream *stream);
int
ia_css_stream_set_output_padded_width(struct ia_css_stream *stream,
				      unsigned int output_padded_width);
int
ia_css_stream_get_max_buffer_depth(struct ia_css_stream *stream,
				   int *buffer_depth);
int
ia_css_stream_set_buffer_depth(struct ia_css_stream *stream, int buffer_depth);
int
ia_css_stream_get_buffer_depth(struct ia_css_stream *stream, int *buffer_depth);
int
ia_css_stream_capture(struct ia_css_stream *stream,
		      int num_captures,
		      unsigned int skip,
		      int offset);
int
ia_css_stream_capture_frame(struct ia_css_stream *stream,
			    unsigned int exp_id);
void
ia_css_stream_send_input_frame(const struct ia_css_stream *stream,
			       const unsigned short *data,
			       unsigned int width,
			       unsigned int height);
void
ia_css_stream_start_input_frame(const struct ia_css_stream *stream);
void
ia_css_stream_send_input_line(const struct ia_css_stream *stream,
			      const unsigned short *data,
			      unsigned int width,
			      const unsigned short *data2,
			      unsigned int width2);
void
ia_css_stream_send_input_embedded_line(const struct ia_css_stream *stream,
				       enum atomisp_input_format format,
				       const unsigned short *data,
				       unsigned int width);
void
ia_css_stream_end_input_frame(const struct ia_css_stream *stream);
void
ia_css_stream_request_flash(struct ia_css_stream *stream);
int
ia_css_stream_set_isp_config_on_pipe(struct ia_css_stream *stream,
				     const struct ia_css_isp_config *config,
				     struct ia_css_pipe *pipe);
int
ia_css_stream_set_isp_config(
    struct ia_css_stream *stream,
    const struct ia_css_isp_config *config);
void
ia_css_stream_get_isp_config(const struct ia_css_stream *stream,
			     struct ia_css_isp_config *config);
int
ia_css_alloc_continuous_frame_remain(struct ia_css_stream *stream);
int
ia_css_update_continuous_frames(struct ia_css_stream *stream);
int
ia_css_unlock_raw_frame(struct ia_css_stream *stream, uint32_t exp_id);
void
ia_css_en_dz_capt_pipe(struct ia_css_stream *stream, bool enable);
#endif  
