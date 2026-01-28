#ifndef __PLAT_MXC_IRQ_COMMON_H__
#define __PLAT_MXC_IRQ_COMMON_H__
#define FIQ_START	0
struct mxc_extra_irq
{
	int (*set_irq_fiq)(unsigned int irq, unsigned int type);
};
#endif
