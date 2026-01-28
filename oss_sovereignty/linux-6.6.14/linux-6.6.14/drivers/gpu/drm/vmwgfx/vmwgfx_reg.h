#ifndef _VMWGFX_REG_H_
#define _VMWGFX_REG_H_
#include <linux/types.h>
struct svga_guest_mem_descriptor {
	u32 ppn;
	u32 num_pages;
};
struct svga_fifo_cmd_fence {
	u32 fence;
};
#define SVGA_SYNC_GENERIC         1
#define SVGA_SYNC_FIFOFULL        2
#include "device_include/svga3d_reg.h"
#endif
