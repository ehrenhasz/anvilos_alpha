#ifndef __ARCH_ARM_MACH_OMAP1_COMMON_H
#define __ARCH_ARM_MACH_OMAP1_COMMON_H
#include <linux/platform_data/i2c-omap.h>
#include <linux/reboot.h>
#include <asm/exception.h>
#include "irqs.h"
#include "soc.h"
#include "i2c.h"
#ifdef CONFIG_OMAP_SERIAL_WAKE
int omap_serial_wakeup_init(void);
#else
static inline int omap_serial_wakeup_init(void)
{
	return 0;
}
#endif
void omap1_map_io(void);
void omap1_init_early(void);
void omap1_init_irq(void);
void __exception_irq_entry omap1_handle_irq(struct pt_regs *regs);
void omap1_init_late(void);
void omap1_restart(enum reboot_mode, const char *);
extern void __init omap_check_revision(void);
struct nand_chip;
extern void omap1_nand_cmd_ctl(struct nand_chip *this, int cmd,
			       unsigned int ctrl);
extern void omap1_timer_init(void);
#ifdef CONFIG_OMAP_32K_TIMER
extern int omap_32k_timer_init(void);
#else
static inline int __init omap_32k_timer_init(void)
{
	return -ENODEV;
}
#endif
#ifdef CONFIG_ARCH_OMAP16XX
extern int ocpi_enable(void);
#else
static inline int ocpi_enable(void) { return 0; }
#endif
extern u32 omap1_get_reset_sources(void);
#endif  
