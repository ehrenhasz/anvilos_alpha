#ifndef __ARCH_ARM_MACH_OMAP2_DISPLAY_H
#define __ARCH_ARM_MACH_OMAP2_DISPLAY_H
#include <linux/kernel.h>
struct omap_dss_dispc_dev_attr {
	u8	manager_count;
	bool	has_framedonetv_irq;
};
int omap_init_vrfb(void);
int omap_init_fb(void);
int omap_init_vout(void);
#endif
