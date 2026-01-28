#ifndef _RALINK_REGS_H_
#define _RALINK_REGS_H_
#include <linux/io.h>
enum ralink_soc_type {
	RALINK_UNKNOWN = 0,
	RT2880_SOC,
	RT3883_SOC,
	RT305X_SOC_RT3050,
	RT305X_SOC_RT3052,
	RT305X_SOC_RT3350,
	RT305X_SOC_RT3352,
	RT305X_SOC_RT5350,
	MT762X_SOC_MT7620A,
	MT762X_SOC_MT7620N,
	MT762X_SOC_MT7621AT,
	MT762X_SOC_MT7628AN,
	MT762X_SOC_MT7688,
};
extern enum ralink_soc_type ralink_soc;
extern __iomem void *rt_sysc_membase;
extern __iomem void *rt_memc_membase;
static inline void rt_sysc_w32(u32 val, unsigned reg)
{
	__raw_writel(val, rt_sysc_membase + reg);
}
static inline u32 rt_sysc_r32(unsigned reg)
{
	return __raw_readl(rt_sysc_membase + reg);
}
static inline void rt_sysc_m32(u32 clr, u32 set, unsigned reg)
{
	u32 val = rt_sysc_r32(reg) & ~clr;
	__raw_writel(val | set, rt_sysc_membase + reg);
}
static inline void rt_memc_w32(u32 val, unsigned reg)
{
	__raw_writel(val, rt_memc_membase + reg);
}
static inline u32 rt_memc_r32(unsigned reg)
{
	return __raw_readl(rt_memc_membase + reg);
}
#endif  
