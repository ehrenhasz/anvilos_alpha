#ifndef __PLAT_WAKEUP_MASK_H
#define __PLAT_WAKEUP_MASK_H __file__
#define NO_WAKEUP_IRQ (0x90000000)
struct samsung_wakeup_mask {
	unsigned int	irq;
	u32		bit;
};
extern void samsung_sync_wakemask(void __iomem *reg,
				  const struct samsung_wakeup_mask *masks,
				  int nr_masks);
#endif  
