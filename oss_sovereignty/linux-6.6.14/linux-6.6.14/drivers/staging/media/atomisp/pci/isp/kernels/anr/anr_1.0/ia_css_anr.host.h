#ifndef __IA_CSS_ANR_HOST_H
#define __IA_CSS_ANR_HOST_H
#include "ia_css_anr_types.h"
#include "ia_css_anr_param.h"
extern const struct ia_css_anr_config default_anr_config;
void
ia_css_anr_encode(
    struct sh_css_isp_anr_params *to,
    const struct ia_css_anr_config *from,
    unsigned int size);
void
ia_css_anr_dump(
    const struct sh_css_isp_anr_params *anr,
    unsigned int level);
void
ia_css_anr_debug_dtrace(
    const struct ia_css_anr_config *config, unsigned int level)
;
#endif  
