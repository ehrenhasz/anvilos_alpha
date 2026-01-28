#ifndef __ARCH_ARM_MACH_OMAP2PLUS_COMMON_H
#define __ARCH_ARM_MACH_OMAP2PLUS_COMMON_H
#ifndef __ASSEMBLER__
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/mfd/twl.h>
#include <linux/platform_data/i2c-omap.h>
#include <linux/reboot.h>
#include <linux/irqchip/irq-omap-intc.h>
#include <asm/proc-fns.h>
#include <asm/hardware/cache-l2x0.h>
#include "i2c.h"
#define OMAP_INTC_START		NR_IRQS
extern int (*omap_pm_soc_init)(void);
int omap_pm_nop_init(void);
#if defined(CONFIG_PM) && defined(CONFIG_ARCH_OMAP3)
int omap3_pm_init(void);
#else
static inline int omap3_pm_init(void)
{
	return 0;
}
#endif
#if defined(CONFIG_PM) && (defined(CONFIG_ARCH_OMAP4) || defined(CONFIG_SOC_OMAP5) || defined(CONFIG_SOC_DRA7XX))
int omap4_pm_init(void);
int omap4_pm_init_early(void);
#else
static inline int omap4_pm_init(void)
{
	return 0;
}
static inline int omap4_pm_init_early(void)
{
	return 0;
}
#endif
#if defined(CONFIG_PM) && (defined(CONFIG_SOC_AM33XX) || \
	defined(CONFIG_SOC_AM43XX))
int amx3_common_pm_init(void);
#else
static inline int amx3_common_pm_init(void)
{
	return 0;
}
#endif
#ifdef CONFIG_CACHE_L2X0
int omap_l2_cache_init(void);
#define OMAP_L2C_AUX_CTRL	(L2C_AUX_CTRL_SHARED_OVERRIDE | \
				 L310_AUX_CTRL_DATA_PREFETCH | \
				 L310_AUX_CTRL_INSTR_PREFETCH)
void omap4_l2c310_write_sec(unsigned long val, unsigned reg);
#else
static inline int omap_l2_cache_init(void)
{
	return 0;
}
#define OMAP_L2C_AUX_CTRL	0
#define omap4_l2c310_write_sec	NULL
#endif
#ifdef CONFIG_SOC_HAS_REALTIME_COUNTER
extern void omap5_realtime_timer_init(void);
#else
static inline void omap5_realtime_timer_init(void)
{
}
#endif
void omap2420_init_early(void);
void omap2430_init_early(void);
void omap3430_init_early(void);
void omap3630_init_early(void);
void am33xx_init_early(void);
void am35xx_init_early(void);
void ti814x_init_early(void);
void ti816x_init_early(void);
void am43xx_init_early(void);
void am43xx_init_late(void);
void omap4430_init_early(void);
void omap5_init_early(void);
void omap3_init_late(void);
void omap4430_init_late(void);
void ti81xx_init_late(void);
void am33xx_init_late(void);
void omap5_init_late(void);
void dra7xx_init_early(void);
void dra7xx_init_late(void);
#ifdef CONFIG_SOC_BUS
void omap_soc_device_init(void);
#else
static inline void omap_soc_device_init(void)
{
}
#endif
#if defined(CONFIG_SOC_OMAP2420) || defined(CONFIG_SOC_OMAP2430)
void omap2xxx_restart(enum reboot_mode mode, const char *cmd);
#else
static inline void omap2xxx_restart(enum reboot_mode mode, const char *cmd)
{
}
#endif
#ifdef CONFIG_SOC_AM33XX
void am33xx_restart(enum reboot_mode mode, const char *cmd);
#else
static inline void am33xx_restart(enum reboot_mode mode, const char *cmd)
{
}
#endif
#ifdef CONFIG_ARCH_OMAP3
void omap3xxx_restart(enum reboot_mode mode, const char *cmd);
#else
static inline void omap3xxx_restart(enum reboot_mode mode, const char *cmd)
{
}
#endif
#ifdef CONFIG_SOC_TI81XX
void ti81xx_restart(enum reboot_mode mode, const char *cmd);
#else
static inline void ti81xx_restart(enum reboot_mode mode, const char *cmd)
{
}
#endif
#if defined(CONFIG_ARCH_OMAP4) || defined(CONFIG_SOC_OMAP5) || \
	defined(CONFIG_SOC_DRA7XX) || defined(CONFIG_SOC_AM43XX)
void omap44xx_restart(enum reboot_mode mode, const char *cmd);
#else
static inline void omap44xx_restart(enum reboot_mode mode, const char *cmd)
{
}
#endif
#ifdef CONFIG_OMAP_INTERCONNECT_BARRIER
void omap_barrier_reserve_memblock(void);
void omap_barriers_init(void);
#else
static inline void omap_barrier_reserve_memblock(void)
{
}
#endif
void __init omap2_set_globals_tap(u32 class, void __iomem *tap);
void __init omap242x_map_io(void);
void __init omap243x_map_io(void);
void __init omap3_map_io(void);
void __init am33xx_map_io(void);
void __init omap4_map_io(void);
void __init omap5_map_io(void);
void __init dra7xx_map_io(void);
void __init ti81xx_map_io(void);
#define omap_test_timeout(cond, timeout, index)			\
({								\
	for (index = 0; index < timeout; index++) {		\
		if (cond)					\
			break;					\
		udelay(1);					\
	}							\
})
void omap_gic_of_init(void);
#ifdef CONFIG_CACHE_L2X0
extern void __iomem *omap4_get_l2cache_base(void);
#endif
struct device_node;
#ifdef CONFIG_SMP
extern void __iomem *omap4_get_scu_base(void);
#else
static inline void __iomem *omap4_get_scu_base(void)
{
	return NULL;
}
#endif
extern void gic_dist_disable(void);
extern void gic_dist_enable(void);
extern bool gic_dist_disabled(void);
extern void gic_timer_retrigger(void);
extern void _omap_smc1(u32 fn, u32 arg);
extern void omap4_sar_ram_init(void);
extern void __iomem *omap4_get_sar_ram_base(void);
extern void omap4_mpuss_early_init(void);
extern void omap_do_wfi(void);
extern void omap_interconnect_sync(void);
#ifdef CONFIG_SMP
extern u32 omap_modify_auxcoreboot0(u32 set_mask, u32 clear_mask);
extern void omap_auxcoreboot_addr(u32 cpu_addr);
extern u32 omap_read_auxcoreboot0(void);
extern void omap4_cpu_die(unsigned int cpu);
extern int omap4_cpu_kill(unsigned int cpu);
extern const struct smp_operations omap4_smp_ops;
#endif
extern u32 omap4_get_cpu1_ns_pa_addr(void);
#if defined(CONFIG_SMP) && defined(CONFIG_PM)
extern int omap4_mpuss_init(void);
extern int omap4_enter_lowpower(unsigned int cpu, unsigned int power_state,
				bool rcuidle);
extern int omap4_hotplug_cpu(unsigned int cpu, unsigned int power_state);
#else
static inline int omap4_enter_lowpower(unsigned int cpu,
					unsigned int power_state,
					bool rcuidle)
{
	cpu_do_idle();
	return 0;
}
static inline int omap4_hotplug_cpu(unsigned int cpu, unsigned int power_state)
{
	cpu_do_idle();
	return 0;
}
static inline int omap4_mpuss_init(void)
{
	return 0;
}
#endif
#ifdef CONFIG_ARCH_OMAP4
void omap4_secondary_startup(void);
void omap4460_secondary_startup(void);
int omap4_finish_suspend(unsigned long cpu_state);
void omap4_cpu_resume(void);
#else
static inline void omap4_secondary_startup(void)
{
}
static inline void omap4460_secondary_startup(void)
{
}
static inline int omap4_finish_suspend(unsigned long cpu_state)
{
	return 0;
}
static inline void omap4_cpu_resume(void)
{
}
#endif
#if defined(CONFIG_SOC_OMAP5) || defined(CONFIG_SOC_DRA7XX)
void omap5_secondary_startup(void);
void omap5_secondary_hyp_startup(void);
#else
static inline void omap5_secondary_startup(void)
{
}
static inline void omap5_secondary_hyp_startup(void)
{
}
#endif
struct omap_system_dma_plat_info;
void pdata_quirks_init(const struct of_device_id *);
void omap_auxdata_legacy_init(struct device *dev);
void omap_pcs_legacy_init(int irq, void (*rearm)(void));
extern struct omap_system_dma_plat_info dma_plat_info;
struct omap_sdrc_params;
extern void omap_sdrc_init(struct omap_sdrc_params *sdrc_cs0,
				      struct omap_sdrc_params *sdrc_cs1);
extern void omap_reserve(void);
struct omap_hwmod;
extern int omap_dss_reset(struct omap_hwmod *);
int omap_clk_init(void);
#if IS_ENABLED(CONFIG_OMAP_IOMMU)
int omap_iommu_set_pwrdm_constraint(struct platform_device *pdev, bool request,
				    u8 *pwrst);
#else
static inline int omap_iommu_set_pwrdm_constraint(struct platform_device *pdev,
						  bool request, u8 *pwrst)
{
	return 0;
}
#endif
#endif  
#endif  
