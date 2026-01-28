#ifndef __IA_CSS_STREAM_FORMAT_H
#define __IA_CSS_STREAM_FORMAT_H
#include <type_support.h>  
#include "../../../include/linux/atomisp_platform.h"
unsigned int ia_css_util_input_format_bpp(
    enum atomisp_input_format format,
    bool two_ppc);
#endif  
