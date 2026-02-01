 
 

#ifndef __IA_CSS_RAA_HOST_H
#define __IA_CSS_RAA_HOST_H

#include "aa/aa_2/ia_css_aa2_types.h"
#include "aa/aa_2/ia_css_aa2_param.h"

void
ia_css_raa_encode(
    struct sh_css_isp_aa_params *to,
    const struct ia_css_aa_config *from,
    unsigned int size);

#endif  
