
 

#include "ia_css_properties.h"
#include <assert_support.h>
#include "ia_css_types.h"
#include "gdc_device.h"

void
ia_css_get_properties(struct ia_css_properties *properties)
{
	assert(properties);
	 
	properties->gdc_coord_one = gdc_get_unity(GDC0_ID) / HRT_GDC_COORD_SCALE;

	properties->l1_base_is_index = true;

	properties->vamem_type = IA_CSS_VAMEM_TYPE_2;
}
