 
 

#ifndef __IA_CSS_ANR2_HOST_H
#define __IA_CSS_ANR2_HOST_H

#include "sh_css_params.h"

#include "ia_css_anr2_types.h"
#include "ia_css_anr2_param.h"
#include "ia_css_anr2_table.host.h"

void
ia_css_anr2_vmem_encode(
    struct ia_css_isp_anr2_params *to,
    const struct ia_css_anr_thres *from,
    size_t size);

void
ia_css_anr2_debug_dtrace(
    const struct ia_css_anr_thres *config, unsigned int level)
;

#endif  
