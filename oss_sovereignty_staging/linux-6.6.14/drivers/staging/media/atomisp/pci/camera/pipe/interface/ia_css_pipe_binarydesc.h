 
 

#ifndef __IA_CSS_PIPE_BINARYDESC_H__
#define __IA_CSS_PIPE_BINARYDESC_H__

#include <linux/math.h>

#include <ia_css_types.h>		 
#include <ia_css_frame_public.h>	 
#include <ia_css_binary.h>		 

 
void ia_css_pipe_get_copy_binarydesc(
    struct ia_css_pipe const *const pipe,
    struct ia_css_binary_descr *copy_descr,
    struct ia_css_frame_info *in_info,
    struct ia_css_frame_info *out_info,
    struct ia_css_frame_info *vf_info);

 
void ia_css_pipe_get_vfpp_binarydesc(
    struct ia_css_pipe const *const pipe,
    struct ia_css_binary_descr *vf_pp_descr,
    struct ia_css_frame_info *in_info,
    struct ia_css_frame_info *out_info);

 
int sh_css_bds_factor_get_fract(unsigned int bds_factor, struct u32_fract *bds);

 
int ia_css_pipe_get_preview_binarydesc(
    struct ia_css_pipe *const pipe,
    struct ia_css_binary_descr *preview_descr,
    struct ia_css_frame_info *in_info,
    struct ia_css_frame_info *bds_out_info,
    struct ia_css_frame_info *out_info,
    struct ia_css_frame_info *vf_info);

 
int ia_css_pipe_get_video_binarydesc(
    struct ia_css_pipe *const pipe,
    struct ia_css_binary_descr *video_descr,
    struct ia_css_frame_info *in_info,
    struct ia_css_frame_info *bds_out_info,
    struct ia_css_frame_info *out_info,
    struct ia_css_frame_info *vf_info,
    int stream_config_left_padding);

 
void ia_css_pipe_get_yuvscaler_binarydesc(
    struct ia_css_pipe const *const pipe,
    struct ia_css_binary_descr *yuv_scaler_descr,
    struct ia_css_frame_info *in_info,
    struct ia_css_frame_info *out_info,
    struct ia_css_frame_info *internal_out_info,
    struct ia_css_frame_info *vf_info);

 
void ia_css_pipe_get_capturepp_binarydesc(
    struct ia_css_pipe *const pipe,
    struct ia_css_binary_descr *capture_pp_descr,
    struct ia_css_frame_info *in_info,
    struct ia_css_frame_info *out_info,
    struct ia_css_frame_info *vf_info);

 
void ia_css_pipe_get_primary_binarydesc(
    struct ia_css_pipe const *const pipe,
    struct ia_css_binary_descr *prim_descr,
    struct ia_css_frame_info *in_info,
    struct ia_css_frame_info *out_info,
    struct ia_css_frame_info *vf_info,
    unsigned int stage_idx);

 
void ia_css_pipe_get_pre_gdc_binarydesc(
    struct ia_css_pipe const *const pipe,
    struct ia_css_binary_descr *gdc_descr,
    struct ia_css_frame_info *in_info,
    struct ia_css_frame_info *out_info);

 
void ia_css_pipe_get_gdc_binarydesc(
    struct ia_css_pipe const *const pipe,
    struct ia_css_binary_descr *gdc_descr,
    struct ia_css_frame_info *in_info,
    struct ia_css_frame_info *out_info);

 
void ia_css_pipe_get_post_gdc_binarydesc(
    struct ia_css_pipe const *const pipe,
    struct ia_css_binary_descr *post_gdc_descr,
    struct ia_css_frame_info *in_info,
    struct ia_css_frame_info *out_info,
    struct ia_css_frame_info *vf_info);

 
void ia_css_pipe_get_pre_de_binarydesc(
    struct ia_css_pipe const *const pipe,
    struct ia_css_binary_descr *pre_de_descr,
    struct ia_css_frame_info *in_info,
    struct ia_css_frame_info *out_info);

 
void ia_css_pipe_get_pre_anr_binarydesc(
    struct ia_css_pipe const *const pipe,
    struct ia_css_binary_descr *pre_anr_descr,
    struct ia_css_frame_info *in_info,
    struct ia_css_frame_info *out_info);

 
void ia_css_pipe_get_anr_binarydesc(
    struct ia_css_pipe const *const pipe,
    struct ia_css_binary_descr *anr_descr,
    struct ia_css_frame_info *in_info,
    struct ia_css_frame_info *out_info);

 
void ia_css_pipe_get_post_anr_binarydesc(
    struct ia_css_pipe const *const pipe,
    struct ia_css_binary_descr *post_anr_descr,
    struct ia_css_frame_info *in_info,
    struct ia_css_frame_info *out_info,
    struct ia_css_frame_info *vf_info);

 
void ia_css_pipe_get_ldc_binarydesc(
    struct ia_css_pipe const *const pipe,
    struct ia_css_binary_descr *ldc_descr,
    struct ia_css_frame_info *in_info,
    struct ia_css_frame_info *out_info);

 
int binarydesc_calculate_bds_factor(
    struct ia_css_resolution input_res,
    struct ia_css_resolution output_res,
    unsigned int *bds_factor);

#endif  
