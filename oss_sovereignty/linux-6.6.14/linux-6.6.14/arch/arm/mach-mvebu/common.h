#ifndef __ARCH_MVEBU_COMMON_H
#define __ARCH_MVEBU_COMMON_H
#include <linux/reboot.h>
void mvebu_restart(enum reboot_mode mode, const char *cmd);
int mvebu_cpu_reset_deassert(int cpu);
void mvebu_pmsu_set_cpu_boot_addr(int hw_cpu, void *boot_addr);
void mvebu_system_controller_set_cpu_boot_addr(void *boot_addr);
int mvebu_system_controller_get_soc_id(u32 *dev, u32 *rev);
void __iomem *mvebu_get_scu_base(void);
int mvebu_pm_suspend_init(void (*board_pm_enter)(void __iomem *sdram_reg,
							u32 srcmd));
#endif
