#ifndef __SH_CSS_HOST_DATA_H
#define __SH_CSS_HOST_DATA_H
#include <ia_css_types.h>	 
struct ia_css_host_data *
ia_css_host_data_allocate(size_t size);
void ia_css_host_data_free(struct ia_css_host_data *me);
#endif  
