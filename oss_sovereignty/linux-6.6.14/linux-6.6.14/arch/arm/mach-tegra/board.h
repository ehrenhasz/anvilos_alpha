#ifndef __MACH_TEGRA_BOARD_H
#define __MACH_TEGRA_BOARD_H
#include <linux/types.h>
#include <linux/reboot.h>
void __init tegra_map_common_io(void);
void __init tegra_init_irq(void);
void __init tegra_paz00_wifikill_init(void);
#endif
