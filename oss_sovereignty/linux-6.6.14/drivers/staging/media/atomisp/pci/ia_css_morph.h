 
 

#ifndef __IA_CSS_MORPH_H
#define __IA_CSS_MORPH_H

 

#include <ia_css_types.h>

 
struct ia_css_morph_table *
ia_css_morph_table_allocate(unsigned int width, unsigned int height);

 
void
ia_css_morph_table_free(struct ia_css_morph_table *me);

#endif  
