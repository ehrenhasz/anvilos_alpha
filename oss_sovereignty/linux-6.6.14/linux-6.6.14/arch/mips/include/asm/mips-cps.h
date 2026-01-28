#ifndef __MIPS_ASM_MIPS_CPS_H__
#define __MIPS_ASM_MIPS_CPS_H__
#include <linux/bitfield.h>
#include <linux/io.h>
#include <linux/types.h>
extern unsigned long __cps_access_bad_size(void)
	__compiletime_error("Bad size for CPS accessor");
#define CPS_ACCESSOR_A(unit, off, name)					\
static inline void *addr_##unit##_##name(void)				\
{									\
	return mips_##unit##_base + (off);				\
}
#define CPS_ACCESSOR_R(unit, sz, name)					\
static inline uint##sz##_t read_##unit##_##name(void)			\
{									\
	uint64_t val64;							\
									\
	switch (sz) {							\
	case 32:							\
		return __raw_readl(addr_##unit##_##name());		\
									\
	case 64:							\
		if (mips_cm_is64)					\
			return __raw_readq(addr_##unit##_##name());	\
									\
		val64 = __raw_readl(addr_##unit##_##name() + 4);	\
		val64 <<= 32;						\
		val64 |= __raw_readl(addr_##unit##_##name());		\
		return val64;						\
									\
	default:							\
		return __cps_access_bad_size();				\
	}								\
}
#define CPS_ACCESSOR_W(unit, sz, name)					\
static inline void write_##unit##_##name(uint##sz##_t val)		\
{									\
	switch (sz) {							\
	case 32:							\
		__raw_writel(val, addr_##unit##_##name());		\
		break;							\
									\
	case 64:							\
		if (mips_cm_is64) {					\
			__raw_writeq(val, addr_##unit##_##name());	\
			break;						\
		}							\
									\
		__raw_writel((uint64_t)val >> 32,			\
			     addr_##unit##_##name() + 4);		\
		__raw_writel(val, addr_##unit##_##name());		\
		break;							\
									\
	default:							\
		__cps_access_bad_size();				\
		break;							\
	}								\
}
#define CPS_ACCESSOR_M(unit, sz, name)					\
static inline void change_##unit##_##name(uint##sz##_t mask,		\
					  uint##sz##_t val)		\
{									\
	uint##sz##_t reg_val = read_##unit##_##name();			\
	reg_val &= ~mask;						\
	reg_val |= val;							\
	write_##unit##_##name(reg_val);					\
}									\
									\
static inline void set_##unit##_##name(uint##sz##_t val)		\
{									\
	change_##unit##_##name(val, val);				\
}									\
									\
static inline void clear_##unit##_##name(uint##sz##_t val)		\
{									\
	change_##unit##_##name(val, 0);					\
}
#define CPS_ACCESSOR_RO(unit, sz, off, name)				\
	CPS_ACCESSOR_A(unit, off, name)					\
	CPS_ACCESSOR_R(unit, sz, name)
#define CPS_ACCESSOR_WO(unit, sz, off, name)				\
	CPS_ACCESSOR_A(unit, off, name)					\
	CPS_ACCESSOR_W(unit, sz, name)
#define CPS_ACCESSOR_RW(unit, sz, off, name)				\
	CPS_ACCESSOR_A(unit, off, name)					\
	CPS_ACCESSOR_R(unit, sz, name)					\
	CPS_ACCESSOR_W(unit, sz, name)					\
	CPS_ACCESSOR_M(unit, sz, name)
#include <asm/mips-cm.h>
#include <asm/mips-cpc.h>
#include <asm/mips-gic.h>
static inline unsigned int mips_cps_numclusters(void)
{
	if (mips_cm_revision() < CM_REV_CM3_5)
		return 1;
	return FIELD_GET(CM_GCR_CONFIG_NUM_CLUSTERS, read_gcr_config());
}
static inline uint64_t mips_cps_cluster_config(unsigned int cluster)
{
	uint64_t config;
	if (mips_cm_revision() < CM_REV_CM3_5) {
		WARN_ON(cluster != 0);
		config = read_gcr_config();
	} else {
		mips_cm_lock_other(cluster, 0, 0, CM_GCR_Cx_OTHER_BLOCK_GLOBAL);
		config = read_cpc_redir_config();
		mips_cm_unlock_other();
	}
	return config;
}
static inline unsigned int mips_cps_numcores(unsigned int cluster)
{
	if (!mips_cm_present())
		return 0;
	return FIELD_GET(CM_GCR_CONFIG_PCORES,
			 mips_cps_cluster_config(cluster) + 1);
}
static inline unsigned int mips_cps_numiocu(unsigned int cluster)
{
	if (!mips_cm_present())
		return 0;
	return FIELD_GET(CM_GCR_CONFIG_NUMIOCU,
			 mips_cps_cluster_config(cluster));
}
static inline unsigned int mips_cps_numvps(unsigned int cluster, unsigned int core)
{
	unsigned int cfg;
	if (!mips_cm_present())
		return 1;
	if ((!IS_ENABLED(CONFIG_MIPS_MT_SMP) || !cpu_has_mipsmt)
		&& (!IS_ENABLED(CONFIG_CPU_MIPSR6) || !cpu_has_vp))
		return 1;
	mips_cm_lock_other(cluster, core, 0, CM_GCR_Cx_OTHER_BLOCK_LOCAL);
	if (mips_cm_revision() < CM_REV_CM3_5) {
		cfg = read_gcr_co_config();
	} else {
		cfg = read_cpc_co_config();
	}
	mips_cm_unlock_other();
	return FIELD_GET(CM_GCR_Cx_CONFIG_PVPE, cfg + 1);
}
#endif  
