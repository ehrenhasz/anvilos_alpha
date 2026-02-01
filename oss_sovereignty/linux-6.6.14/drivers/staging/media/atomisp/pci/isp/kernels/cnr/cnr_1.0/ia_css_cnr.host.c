
 

#include "ia_css_types.h"
#include "sh_css_defs.h"
#include "ia_css_debug.h"

#include "ia_css_cnr.host.h"

 
void
ia_css_init_cnr_state(
    void  * state,
    size_t size)
{
	memset(state, 0, size);
}
