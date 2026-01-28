#ifndef __IA_CSS_FRAME_H__
#define __IA_CSS_FRAME_H__
#include <ia_css_types.h>
#include <ia_css_frame_format.h>
#include <ia_css_frame_public.h>
#include "dma.h"
void ia_css_frame_info_set_width(struct ia_css_frame_info *info,
				 unsigned int width,
				 unsigned int min_padded_width);
void ia_css_frame_info_set_format(struct ia_css_frame_info *info,
				  enum ia_css_frame_format format);
void ia_css_frame_info_init(struct ia_css_frame_info *info,
			    unsigned int width,
			    unsigned int height,
			    enum ia_css_frame_format format,
			    unsigned int aligned);
bool ia_css_frame_info_is_same_resolution(
    const struct ia_css_frame_info *info_a,
    const struct ia_css_frame_info *info_b);
int ia_css_frame_check_info(const struct ia_css_frame_info *info);
int ia_css_frame_init_planes(struct ia_css_frame *frame);
void ia_css_frame_free_multiple(unsigned int num_frames,
				struct ia_css_frame **frames_array);
int ia_css_frame_allocate_with_buffer_size(struct ia_css_frame **frame,
					   const unsigned int size_bytes);
bool ia_css_frame_is_same_type(
    const struct ia_css_frame *frame_a,
    const struct ia_css_frame *frame_b);
int ia_css_dma_configure_from_info(struct dma_port_config *config,
				   const struct ia_css_frame_info *info);
unsigned int ia_css_frame_pad_width(unsigned int width, enum ia_css_frame_format format);
#endif  
