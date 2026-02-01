 
 

#ifndef __BAYER_IO_HOST_H
#define __BAYER_IO_HOST_H

#include "ia_css_bayer_io_param.h"
#include "ia_css_bayer_io_types.h"
#include "ia_css_binary.h"
#include "sh_css_internal.h"

int ia_css_bayer_io_config(const struct ia_css_binary     *binary,
			   const struct sh_css_binary_args *args);

#endif  
