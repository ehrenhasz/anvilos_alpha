#ifndef __IA_CSS_SHADING_H
#define __IA_CSS_SHADING_H
#include <ia_css_types.h>
struct ia_css_shading_table *
ia_css_shading_table_alloc(unsigned int width,
			   unsigned int height);
void
ia_css_shading_table_free(struct ia_css_shading_table *table);
#endif  
