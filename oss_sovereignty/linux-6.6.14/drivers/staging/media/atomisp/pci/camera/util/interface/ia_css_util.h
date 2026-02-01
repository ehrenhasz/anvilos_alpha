 
 

#ifndef __IA_CSS_UTIL_H__
#define __IA_CSS_UTIL_H__

#include <linux/errno.h>

#include <ia_css_err.h>
#include <type_support.h>
#include <ia_css_frame_public.h>
#include <ia_css_stream_public.h>
#include <ia_css_stream_format.h>

 
int ia_css_convert_errno(
    int in_err);

 
int ia_css_util_check_vf_info(
    const struct ia_css_frame_info *const info);

 
int ia_css_util_check_input(
    const struct ia_css_stream_config *const stream_config,
    bool must_be_raw,
    bool must_be_yuv);

 
int ia_css_util_check_vf_out_info(
    const struct ia_css_frame_info *const out_info,
    const struct ia_css_frame_info *const vf_info);

 
int ia_css_util_check_res(
    unsigned int width,
    unsigned int height);

 
 
bool ia_css_util_res_leq(
    struct ia_css_resolution a,
    struct ia_css_resolution b);

 
 
bool ia_css_util_resolution_is_zero(
    const struct ia_css_resolution resolution);

 
 
bool ia_css_util_resolution_is_even(
    const struct ia_css_resolution resolution);

 
unsigned int ia_css_util_input_format_bpp(
    enum atomisp_input_format stream_format,
    bool two_ppc);

 
bool ia_css_util_is_input_format_raw(
    enum atomisp_input_format stream_format);

 
bool ia_css_util_is_input_format_yuv(
    enum atomisp_input_format stream_format);

#endif  
