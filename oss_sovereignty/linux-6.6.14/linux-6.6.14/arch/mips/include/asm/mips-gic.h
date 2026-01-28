#ifndef __MIPS_ASM_MIPS_CPS_H__
# error Please include asm/mips-cps.h rather than asm/mips-gic.h
#endif
#ifndef __MIPS_ASM_MIPS_GIC_H__
#define __MIPS_ASM_MIPS_GIC_H__
#include <linux/bitops.h>
extern void __iomem *mips_gic_base;
#define MIPS_GIC_SHARED_OFS	0x00000
#define MIPS_GIC_SHARED_SZ	0x08000
#define MIPS_GIC_LOCAL_OFS	0x08000
#define MIPS_GIC_LOCAL_SZ	0x04000
#define MIPS_GIC_REDIR_OFS	0x0c000
#define MIPS_GIC_REDIR_SZ	0x04000
#define MIPS_GIC_USER_OFS	0x10000
#define MIPS_GIC_USER_SZ	0x10000
#define GIC_ACCESSOR_RO(sz, off, name)					\
	CPS_ACCESSOR_RO(gic, sz, MIPS_GIC_SHARED_OFS + off, name)
#define GIC_ACCESSOR_RW(sz, off, name)					\
	CPS_ACCESSOR_RW(gic, sz, MIPS_GIC_SHARED_OFS + off, name)
#define GIC_VX_ACCESSOR_RO(sz, off, name)				\
	CPS_ACCESSOR_RO(gic, sz, MIPS_GIC_LOCAL_OFS + off, vl_##name)	\
	CPS_ACCESSOR_RO(gic, sz, MIPS_GIC_REDIR_OFS + off, vo_##name)
#define GIC_VX_ACCESSOR_RW(sz, off, name)				\
	CPS_ACCESSOR_RW(gic, sz, MIPS_GIC_LOCAL_OFS + off, vl_##name)	\
	CPS_ACCESSOR_RW(gic, sz, MIPS_GIC_REDIR_OFS + off, vo_##name)
#define GIC_ACCESSOR_RO_INTR_REG(sz, off, stride, name)			\
static inline void __iomem *addr_gic_##name(unsigned int intr)		\
{									\
	return mips_gic_base + (off) + (intr * (stride));		\
}									\
									\
static inline unsigned int read_gic_##name(unsigned int intr)		\
{									\
	BUILD_BUG_ON(sz != 32);						\
	return __raw_readl(addr_gic_##name(intr));			\
}
#define GIC_ACCESSOR_RW_INTR_REG(sz, off, stride, name)			\
	GIC_ACCESSOR_RO_INTR_REG(sz, off, stride, name)			\
									\
static inline void write_gic_##name(unsigned int intr,			\
				    unsigned int val)			\
{									\
	BUILD_BUG_ON(sz != 32);						\
	__raw_writel(val, addr_gic_##name(intr));			\
}
#define GIC_VX_ACCESSOR_RO_INTR_REG(sz, off, stride, name)		\
	GIC_ACCESSOR_RO_INTR_REG(sz, MIPS_GIC_LOCAL_OFS + off,		\
				 stride, vl_##name)			\
	GIC_ACCESSOR_RO_INTR_REG(sz, MIPS_GIC_REDIR_OFS + off,		\
				 stride, vo_##name)
#define GIC_VX_ACCESSOR_RW_INTR_REG(sz, off, stride, name)		\
	GIC_ACCESSOR_RW_INTR_REG(sz, MIPS_GIC_LOCAL_OFS + off,		\
				 stride, vl_##name)			\
	GIC_ACCESSOR_RW_INTR_REG(sz, MIPS_GIC_REDIR_OFS + off,		\
				 stride, vo_##name)
#define GIC_ACCESSOR_RO_INTR_BIT(off, name)				\
static inline void __iomem *addr_gic_##name(void)			\
{									\
	return mips_gic_base + (off);					\
}									\
									\
static inline unsigned int read_gic_##name(unsigned int intr)		\
{									\
	void __iomem *addr = addr_gic_##name();				\
	unsigned int val;						\
									\
	if (mips_cm_is64) {						\
		addr += (intr / 64) * sizeof(uint64_t);			\
		val = __raw_readq(addr) >> intr % 64;			\
	} else {							\
		addr += (intr / 32) * sizeof(uint32_t);			\
		val = __raw_readl(addr) >> intr % 32;			\
	}								\
									\
	return val & 0x1;						\
}
#define GIC_ACCESSOR_RW_INTR_BIT(off, name)				\
	GIC_ACCESSOR_RO_INTR_BIT(off, name)				\
									\
static inline void write_gic_##name(unsigned int intr)			\
{									\
	void __iomem *addr = addr_gic_##name();				\
									\
	if (mips_cm_is64) {						\
		addr += (intr / 64) * sizeof(uint64_t);			\
		__raw_writeq(BIT(intr % 64), addr);			\
	} else {							\
		addr += (intr / 32) * sizeof(uint32_t);			\
		__raw_writel(BIT(intr % 32), addr);			\
	}								\
}									\
									\
static inline void change_gic_##name(unsigned int intr,			\
				     unsigned int val)			\
{									\
	void __iomem *addr = addr_gic_##name();				\
									\
	if (mips_cm_is64) {						\
		uint64_t _val;						\
									\
		addr += (intr / 64) * sizeof(uint64_t);			\
		_val = __raw_readq(addr);				\
		_val &= ~BIT_ULL(intr % 64);				\
		_val |= (uint64_t)val << (intr % 64);			\
		__raw_writeq(_val, addr);				\
	} else {							\
		uint32_t _val;						\
									\
		addr += (intr / 32) * sizeof(uint32_t);			\
		_val = __raw_readl(addr);				\
		_val &= ~BIT(intr % 32);				\
		_val |= val << (intr % 32);				\
		__raw_writel(_val, addr);				\
	}								\
}
#define GIC_VX_ACCESSOR_RO_INTR_BIT(sz, off, name)			\
	GIC_ACCESSOR_RO_INTR_BIT(sz, MIPS_GIC_LOCAL_OFS + off,		\
				 vl_##name)				\
	GIC_ACCESSOR_RO_INTR_BIT(sz, MIPS_GIC_REDIR_OFS + off,		\
				 vo_##name)
#define GIC_VX_ACCESSOR_RW_INTR_BIT(sz, off, name)			\
	GIC_ACCESSOR_RW_INTR_BIT(sz, MIPS_GIC_LOCAL_OFS + off,		\
				 vl_##name)				\
	GIC_ACCESSOR_RW_INTR_BIT(sz, MIPS_GIC_REDIR_OFS + off,		\
				 vo_##name)
GIC_ACCESSOR_RW(32, 0x000, config)
#define GIC_CONFIG_COUNTSTOP		BIT(28)
#define GIC_CONFIG_COUNTBITS		GENMASK(27, 24)
#define GIC_CONFIG_NUMINTERRUPTS	GENMASK(23, 16)
#define GIC_CONFIG_PVPS			GENMASK(6, 0)
GIC_ACCESSOR_RW(64, 0x010, counter)
GIC_ACCESSOR_RW(32, 0x010, counter_32l)
GIC_ACCESSOR_RW(32, 0x014, counter_32h)
GIC_ACCESSOR_RW_INTR_BIT(0x100, pol)
#define GIC_POL_ACTIVE_LOW		0	 
#define GIC_POL_ACTIVE_HIGH		1	 
#define GIC_POL_FALLING_EDGE		0	 
#define GIC_POL_RISING_EDGE		1	 
GIC_ACCESSOR_RW_INTR_BIT(0x180, trig)
#define GIC_TRIG_LEVEL			0
#define GIC_TRIG_EDGE			1
GIC_ACCESSOR_RW_INTR_BIT(0x200, dual)
#define GIC_DUAL_SINGLE			0	 
#define GIC_DUAL_DUAL			1	 
GIC_ACCESSOR_RW(32, 0x280, wedge)
#define GIC_WEDGE_RW			BIT(31)
#define GIC_WEDGE_INTR			GENMASK(7, 0)
GIC_ACCESSOR_RW_INTR_BIT(0x300, rmask)
GIC_ACCESSOR_RW_INTR_BIT(0x380, smask)
GIC_ACCESSOR_RO_INTR_BIT(0x400, mask)
GIC_ACCESSOR_RO_INTR_BIT(0x480, pend)
GIC_ACCESSOR_RW_INTR_REG(32, 0x500, 0x4, map_pin)
#define GIC_MAP_PIN_MAP_TO_PIN		BIT(31)
#define GIC_MAP_PIN_MAP_TO_NMI		BIT(30)
#define GIC_MAP_PIN_MAP			GENMASK(5, 0)
GIC_ACCESSOR_RW_INTR_REG(32, 0x2000, 0x20, map_vp)
GIC_VX_ACCESSOR_RW(32, 0x000, ctl)
#define GIC_VX_CTL_FDC_ROUTABLE		BIT(4)
#define GIC_VX_CTL_SWINT_ROUTABLE	BIT(3)
#define GIC_VX_CTL_PERFCNT_ROUTABLE	BIT(2)
#define GIC_VX_CTL_TIMER_ROUTABLE	BIT(1)
#define GIC_VX_CTL_EIC			BIT(0)
GIC_VX_ACCESSOR_RO(32, 0x004, pend)
GIC_VX_ACCESSOR_RO(32, 0x008, mask)
GIC_VX_ACCESSOR_RW(32, 0x00c, rmask)
GIC_VX_ACCESSOR_RW(32, 0x010, smask)
GIC_VX_ACCESSOR_RW_INTR_REG(32, 0x040, 0x4, map)
GIC_VX_ACCESSOR_RW(32, 0x040, wd_map)
GIC_VX_ACCESSOR_RW(32, 0x044, compare_map)
GIC_VX_ACCESSOR_RW(32, 0x048, timer_map)
GIC_VX_ACCESSOR_RW(32, 0x04c, fdc_map)
GIC_VX_ACCESSOR_RW(32, 0x050, perfctr_map)
GIC_VX_ACCESSOR_RW(32, 0x054, swint0_map)
GIC_VX_ACCESSOR_RW(32, 0x058, swint1_map)
GIC_VX_ACCESSOR_RW(32, 0x080, other)
#define GIC_VX_OTHER_VPNUM		GENMASK(5, 0)
GIC_VX_ACCESSOR_RO(32, 0x088, ident)
#define GIC_VX_IDENT_VPNUM		GENMASK(5, 0)
GIC_VX_ACCESSOR_RW(64, 0x0a0, compare)
GIC_VX_ACCESSOR_RW_INTR_REG(32, 0x100, 0x4, eic_shadow_set)
enum mips_gic_local_interrupt {
	GIC_LOCAL_INT_WD,
	GIC_LOCAL_INT_COMPARE,
	GIC_LOCAL_INT_TIMER,
	GIC_LOCAL_INT_PERFCTR,
	GIC_LOCAL_INT_SWINT0,
	GIC_LOCAL_INT_SWINT1,
	GIC_LOCAL_INT_FDC,
	GIC_NUM_LOCAL_INTRS
};
static inline bool mips_gic_present(void)
{
	return IS_ENABLED(CONFIG_MIPS_GIC) && mips_gic_base;
}
static inline unsigned int
mips_gic_vx_map_reg(enum mips_gic_local_interrupt intr)
{
	if (intr <= GIC_LOCAL_INT_TIMER)
		return intr;
	if (intr == GIC_LOCAL_INT_FDC)
		return GIC_LOCAL_INT_TIMER + 1;
	return intr + 1;
}
extern int gic_get_c0_compare_int(void);
extern int gic_get_c0_perfcount_int(void);
extern int gic_get_c0_fdc_int(void);
#endif  
