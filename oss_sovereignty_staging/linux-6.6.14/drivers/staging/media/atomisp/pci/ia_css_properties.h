 
 

#ifndef __IA_CSS_PROPERTIES_H
#define __IA_CSS_PROPERTIES_H

 

#include <type_support.h>  
#include <ia_css_types.h>  

struct ia_css_properties {
	int  gdc_coord_one;
	bool l1_base_is_index;  
	enum ia_css_vamem_type vamem_type;
};

 
void
ia_css_get_properties(struct ia_css_properties *properties);

#endif  
