 
 

#ifndef _VMWGFX_DEVCAPS_H_
#define _VMWGFX_DEVCAPS_H_

#include "vmwgfx_drv.h"

#include "device_include/svga_reg.h"

int vmw_devcaps_create(struct vmw_private *vmw);
void vmw_devcaps_destroy(struct vmw_private *vmw);
uint32_t vmw_devcaps_size(const struct vmw_private *vmw, bool gb_aware);
int vmw_devcaps_copy(struct vmw_private *vmw, bool gb_aware,
		     void *dst, uint32_t dst_size);

static inline uint32_t vmw_devcap_get(struct vmw_private *vmw,
				      uint32_t devcap)
{
	bool gb_objects = !!(vmw->capabilities & SVGA_CAP_GBOBJECTS);
	if (gb_objects)
		return vmw->devcaps[devcap];
	return 0;
}

#endif
