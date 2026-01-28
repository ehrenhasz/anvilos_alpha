#ifndef __IA_CSS_METADATA_H
#define __IA_CSS_METADATA_H
#include <type_support.h>
#include "ia_css_types.h"
#include "ia_css_stream_format.h"
struct ia_css_metadata_config {
	enum atomisp_input_format data_type;  
	struct ia_css_resolution  resolution;  
};
struct ia_css_metadata_info {
	struct ia_css_resolution resolution;  
	u32                 stride;      
	u32                 size;        
};
struct ia_css_metadata {
	struct ia_css_metadata_info info;     
	ia_css_ptr		    address;  
	u32		    exp_id;
};
#define SIZE_OF_IA_CSS_METADATA_STRUCT sizeof(struct ia_css_metadata)
struct ia_css_metadata *
ia_css_metadata_allocate(const struct ia_css_metadata_info *metadata_info);
void
ia_css_metadata_free(struct ia_css_metadata *metadata);
#endif  
