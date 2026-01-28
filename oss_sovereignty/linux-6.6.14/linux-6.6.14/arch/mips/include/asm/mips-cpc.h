#ifndef __MIPS_ASM_MIPS_CPS_H__
# error Please include asm/mips-cps.h rather than asm/mips-cpc.h
#endif
#ifndef __MIPS_ASM_MIPS_CPC_H__
#define __MIPS_ASM_MIPS_CPC_H__
#include <linux/bitops.h>
#include <linux/errno.h>
extern void __iomem *mips_cpc_base;
extern phys_addr_t mips_cpc_default_phys_base(void);
#ifdef CONFIG_MIPS_CPC
extern int mips_cpc_probe(void);
#else
static inline int mips_cpc_probe(void)
{
	return -ENODEV;
}
#endif
static inline bool mips_cpc_present(void)
{
#ifdef CONFIG_MIPS_CPC
	return mips_cpc_base != NULL;
#else
	return false;
#endif
}
#define MIPS_CPC_GCB_OFS	0x0000
#define MIPS_CPC_CLCB_OFS	0x2000
#define MIPS_CPC_COCB_OFS	0x4000
#define CPC_ACCESSOR_RO(sz, off, name)					\
	CPS_ACCESSOR_RO(cpc, sz, MIPS_CPC_GCB_OFS + off, name)		\
	CPS_ACCESSOR_RO(cpc, sz, MIPS_CPC_COCB_OFS + off, redir_##name)
#define CPC_ACCESSOR_RW(sz, off, name)					\
	CPS_ACCESSOR_RW(cpc, sz, MIPS_CPC_GCB_OFS + off, name)		\
	CPS_ACCESSOR_RW(cpc, sz, MIPS_CPC_COCB_OFS + off, redir_##name)
#define CPC_CX_ACCESSOR_RO(sz, off, name)				\
	CPS_ACCESSOR_RO(cpc, sz, MIPS_CPC_CLCB_OFS + off, cl_##name)	\
	CPS_ACCESSOR_RO(cpc, sz, MIPS_CPC_COCB_OFS + off, co_##name)
#define CPC_CX_ACCESSOR_RW(sz, off, name)				\
	CPS_ACCESSOR_RW(cpc, sz, MIPS_CPC_CLCB_OFS + off, cl_##name)	\
	CPS_ACCESSOR_RW(cpc, sz, MIPS_CPC_COCB_OFS + off, co_##name)
CPC_ACCESSOR_RW(32, 0x000, access)
CPC_ACCESSOR_RW(32, 0x008, seqdel)
CPC_ACCESSOR_RW(32, 0x010, rail)
CPC_ACCESSOR_RW(32, 0x018, resetlen)
CPC_ACCESSOR_RO(32, 0x020, revision)
CPC_ACCESSOR_RW(32, 0x030, pwrup_ctl)
#define CPC_PWRUP_CTL_CM_PWRUP			BIT(0)
CPC_ACCESSOR_RW(64, 0x138, config)
CPC_ACCESSOR_RW(32, 0x140, sys_config)
#define CPC_SYS_CONFIG_BE_IMMEDIATE		BIT(2)
#define CPC_SYS_CONFIG_BE_STATUS		BIT(1)
#define CPC_SYS_CONFIG_BE			BIT(0)
CPC_CX_ACCESSOR_RW(32, 0x000, cmd)
#define CPC_Cx_CMD				GENMASK(3, 0)
#define  CPC_Cx_CMD_CLOCKOFF			0x1
#define  CPC_Cx_CMD_PWRDOWN			0x2
#define  CPC_Cx_CMD_PWRUP			0x3
#define  CPC_Cx_CMD_RESET			0x4
CPC_CX_ACCESSOR_RW(32, 0x008, stat_conf)
#define CPC_Cx_STAT_CONF_PWRUPE			BIT(23)
#define CPC_Cx_STAT_CONF_SEQSTATE		GENMASK(22, 19)
#define  CPC_Cx_STAT_CONF_SEQSTATE_D0		0x0
#define  CPC_Cx_STAT_CONF_SEQSTATE_U0		0x1
#define  CPC_Cx_STAT_CONF_SEQSTATE_U1		0x2
#define  CPC_Cx_STAT_CONF_SEQSTATE_U2		0x3
#define  CPC_Cx_STAT_CONF_SEQSTATE_U3		0x4
#define  CPC_Cx_STAT_CONF_SEQSTATE_U4		0x5
#define  CPC_Cx_STAT_CONF_SEQSTATE_U5		0x6
#define  CPC_Cx_STAT_CONF_SEQSTATE_U6		0x7
#define  CPC_Cx_STAT_CONF_SEQSTATE_D1		0x8
#define  CPC_Cx_STAT_CONF_SEQSTATE_D3		0x9
#define  CPC_Cx_STAT_CONF_SEQSTATE_D2		0xa
#define CPC_Cx_STAT_CONF_CLKGAT_IMPL		BIT(17)
#define CPC_Cx_STAT_CONF_PWRDN_IMPL		BIT(16)
#define CPC_Cx_STAT_CONF_EJTAG_PROBE		BIT(15)
CPC_CX_ACCESSOR_RW(32, 0x010, other)
#define CPC_Cx_OTHER_CORENUM			GENMASK(23, 16)
CPC_CX_ACCESSOR_RW(32, 0x020, vp_stop)
CPC_CX_ACCESSOR_RW(32, 0x028, vp_run)
CPC_CX_ACCESSOR_RW(32, 0x030, vp_running)
CPC_CX_ACCESSOR_RW(32, 0x090, config)
#ifdef CONFIG_MIPS_CPC
extern void mips_cpc_lock_other(unsigned int core);
extern void mips_cpc_unlock_other(void);
#else  
static inline void mips_cpc_lock_other(unsigned int core) { }
static inline void mips_cpc_unlock_other(void) { }
#endif  
#endif  
