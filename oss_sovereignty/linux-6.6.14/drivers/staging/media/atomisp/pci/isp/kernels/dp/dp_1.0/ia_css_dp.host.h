 
 

#ifndef __IA_CSS_DP_HOST_H
#define __IA_CSS_DP_HOST_H

#include "ia_css_dp_types.h"
#include "ia_css_dp_param.h"

extern const struct ia_css_dp_config default_dp_config;

 
extern const struct ia_css_dp_config default_dp_10bpp_config;

void
ia_css_dp_encode(
    struct sh_css_isp_dp_params *to,
    const struct ia_css_dp_config *from,
    unsigned int size);

void
ia_css_dp_dump(
    const struct sh_css_isp_dp_params *dp,
    unsigned int level);

void
ia_css_dp_debug_dtrace(
    const struct ia_css_dp_config *config,
    unsigned int level);

void
ia_css_init_dp_state(
    void  * state,
    size_t size);

#endif  
