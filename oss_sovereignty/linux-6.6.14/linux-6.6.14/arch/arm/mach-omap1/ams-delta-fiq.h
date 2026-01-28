#ifndef __AMS_DELTA_FIQ_H
#define __AMS_DELTA_FIQ_H
#include "irqs.h"
#define INT_DEFERRED_FIQ	INT_1510_RES12
#if (INT_DEFERRED_FIQ < IH2_BASE)
#define DEFERRED_FIQ_IH_BASE	OMAP_IH1_BASE
#else
#define DEFERRED_FIQ_IH_BASE	OMAP_IH2_BASE
#endif
#ifndef __ASSEMBLER__
extern unsigned char qwerty_fiqin_start, qwerty_fiqin_end;
extern void __init ams_delta_init_fiq(struct gpio_chip *chip,
				      struct platform_device *pdev);
#endif
#endif
