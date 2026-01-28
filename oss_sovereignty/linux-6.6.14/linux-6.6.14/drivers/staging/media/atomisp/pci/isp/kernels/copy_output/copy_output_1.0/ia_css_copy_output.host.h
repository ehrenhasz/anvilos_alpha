#ifndef __IA_CSS_COPY_OUTPUT_HOST_H
#define __IA_CSS_COPY_OUTPUT_HOST_H
#include "type_support.h"
#include "ia_css_binary.h"
#include "ia_css_copy_output_param.h"
void
ia_css_copy_output_config(
    struct sh_css_isp_copy_output_isp_config      *to,
    const struct ia_css_copy_output_configuration *from,
    unsigned int size);
int ia_css_copy_output_configure(const struct ia_css_binary     *binary,
				 bool enable);
#endif  
