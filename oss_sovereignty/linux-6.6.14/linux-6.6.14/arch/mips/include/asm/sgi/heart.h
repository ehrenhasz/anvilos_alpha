#ifndef __ASM_SGI_HEART_H
#define __ASM_SGI_HEART_H
#include <linux/types.h>
#include <linux/time.h>
#define HEART_MEMORY_BANKS	4
#define HEART_MAX_CPUS		4
#define HEART_XKPHYS_BASE	((void *)(IO_BASE | 0x000000000ff00000ULL))
struct ip30_heart_regs {		 
	u64 mode;			 
	u64 sdram_mode;			 
	u64 mem_refresh;		 
	u64 mem_req_arb;		 
	union {
		u64 q[HEART_MEMORY_BANKS];	 
		u32 l[HEART_MEMORY_BANKS * 2];	 
	} mem_cfg;			 
	u64 fc_mode;			 
	u64 fc_timer_limit;		 
	u64 fc_addr[2];			 
	u64 fc_credit_cnt[2];		 
	u64 fc_timer[2];		 
	u64 status;			 
	u64 bus_err_addr;		 
	u64 bus_err_misc;		 
	u64 mem_err_addr;		 
	u64 mem_err_data;		 
	u64 piur_acc_err;		 
	u64 mlan_clock_div;		 
	u64 mlan_ctrl;			 
	u64 __pad0[0x01e8];		 
	u64 undefined;			 
	u64 __pad1[0x1dff];		 
	u64 imr[HEART_MAX_CPUS];	 
	u64 set_isr;			 
	u64 clear_isr;			 
	u64 isr;			 
	u64 imsr;			 
	u64 cause;			 
	u64 __pad2[0x1ff7];		 
	u64 count;			 
	u64 __pad3[0x1fff];		 
	u64 compare;			 
	u64 __pad4[0x1fff];		 
	u64 trigger;			 
	u64 __pad5[0x1fff];		 
	u64 cpuid;			 
	u64 __pad6[0x1fff];		 
	u64 sync;			 
};
#define HEART_NS_PER_CYCLE	80
#define HEART_CYCLES_PER_SEC	(NSEC_PER_SEC / HEART_NS_PER_CYCLE)
#define HEART_ATK_MASK		0x0007ffffffffffff	 
#define HEART_ACK_ALL_MASK	0xffffffffffffffff	 
#define HEART_CLR_ALL_MASK	0x0000000000000000	 
#define HEART_BR_ERR_MASK	0x7ff8000000000000	 
#define HEART_CPU0_ERR_MASK	0x8ff8000000000000	 
#define HEART_CPU1_ERR_MASK	0x97f8000000000000	 
#define HEART_CPU2_ERR_MASK	0xa7f8000000000000	 
#define HEART_CPU3_ERR_MASK	0xc7f8000000000000	 
#define HEART_ERR_MASK		0x1ff			 
#define HEART_ERR_MASK_START	51			 
#define HEART_ERR_MASK_END	63			 
#define HM_PROC_DISABLE_SHFT		60
#define HM_PROC_DISABLE_MSK		(0xfUL << HM_PROC_DISABLE_SHFT)
#define HM_PROC_DISABLE(x)		(0x1UL << (x) + HM_PROC_DISABLE_SHFT)
#define HM_MAX_PSR			(0x7UL << 57)
#define HM_MAX_IOSR			(0x7UL << 54)
#define HM_MAX_PEND_IOSR		(0x7UL << 51)
#define HM_TRIG_SRC_SEL_MSK		(0x7UL << 48)
#define HM_TRIG_HEART_EXC		(0x0UL << 48)
#define HM_TRIG_REG_BIT			(0x1UL << 48)
#define HM_TRIG_SYSCLK			(0x2UL << 48)
#define HM_TRIG_MEMCLK_2X		(0x3UL << 48)
#define HM_TRIG_MEMCLK			(0x4UL << 48)
#define HM_TRIG_IOCLK			(0x5UL << 48)
#define HM_PIU_TEST_MODE		(0xfUL << 40)
#define HM_GP_FLAG_MSK			(0xfUL << 36)
#define HM_GP_FLAG(x)			BIT((x) + 36)
#define HM_MAX_PROC_HYST		(0xfUL << 32)
#define HM_LLP_WRST_AFTER_RST		BIT(28)
#define HM_LLP_LINK_RST			BIT(27)
#define HM_LLP_WARM_RST			BIT(26)
#define HM_COR_ECC_LCK			BIT(25)
#define HM_REDUCED_PWR			BIT(24)
#define HM_COLD_RST			BIT(23)
#define HM_SW_RST			BIT(22)
#define HM_MEM_FORCE_WR			BIT(21)
#define HM_DB_ERR_GEN			BIT(20)
#define HM_SB_ERR_GEN			BIT(19)
#define HM_CACHED_PIO_EN		BIT(18)
#define HM_CACHED_PROM_EN		BIT(17)
#define HM_PE_SYS_COR_ERE		BIT(16)
#define HM_GLOBAL_ECC_EN		BIT(15)
#define HM_IO_COH_EN			BIT(14)
#define HM_INT_EN			BIT(13)
#define HM_DATA_CHK_EN			BIT(12)
#define HM_REF_EN			BIT(11)
#define HM_BAD_SYSWR_ERE		BIT(10)
#define HM_BAD_SYSRD_ERE		BIT(9)
#define HM_SYSSTATE_ERE			BIT(8)
#define HM_SYSCMD_ERE			BIT(7)
#define HM_NCOR_SYS_ERE			BIT(6)
#define HM_COR_SYS_ERE			BIT(5)
#define HM_DATA_ELMNT_ERE		BIT(4)
#define HM_MEM_ADDR_PROC_ERE		BIT(3)
#define HM_MEM_ADDR_IO_ERE		BIT(2)
#define HM_NCOR_MEM_ERE			BIT(1)
#define HM_COR_MEM_ERE			BIT(0)
#define HEART_MEMREF_REFS(x)		((0xfUL & (x)) << 16)
#define HEART_MEMREF_PERIOD(x)		((0xffffUL & (x)))
#define HEART_MEMREF_REFS_VAL		HEART_MEMREF_REFS(8)
#define HEART_MEMREF_PERIOD_VAL		HEART_MEMREF_PERIOD(0x4000)
#define HEART_MEMREF_VAL		(HEART_MEMREF_REFS_VAL | \
					 HEART_MEMREF_PERIOD_VAL)
#define HEART_MEMARB_IODIS		(1  << 20)
#define HEART_MEMARB_MAXPMWRQS		(15 << 16)
#define HEART_MEMARB_MAXPMRRQS		(15 << 12)
#define HEART_MEMARB_MAXPMRQS		(15 << 8)
#define HEART_MEMARB_MAXRRRQS		(15 << 4)
#define HEART_MEMARB_MAXGBRRQS		(15)
#define HEART_MEMCFG_VALID		0x80000000	 
#define HEART_MEMCFG_DENSITY		0x01c00000	 
#define HEART_MEMCFG_SIZE_MASK		0x003f0000	 
#define HEART_MEMCFG_ADDR_MASK		0x000001ff	 
#define HEART_MEMCFG_SIZE_SHIFT		16		 
#define HEART_MEMCFG_DENSITY_SHIFT	22		 
#define HEART_MEMCFG_UNIT_SHIFT		25		 
#define HEART_STAT_HSTL_SDRV		BIT(14)
#define HEART_STAT_FC_CR_OUT(x)		BIT((x) + 12)
#define HEART_STAT_DIR_CNNCT		BIT(11)
#define HEART_STAT_TRITON		BIT(10)
#define HEART_STAT_R4K			BIT(9)
#define HEART_STAT_BIG_ENDIAN		BIT(8)
#define HEART_STAT_PROC_SHFT		4
#define HEART_STAT_PROC_MSK		(0xfUL << HEART_STAT_PROC_SHFT)
#define HEART_STAT_PROC_ACTIVE(x)	(0x1UL << ((x) + HEART_STAT_PROC_SHFT))
#define HEART_STAT_WIDGET_ID		0xf
#define HC_PE_SYS_COR_ERR_MSK		(0xfUL << 60)
#define HC_PE_SYS_COR_ERR(x)		BIT((x) + 60)
#define HC_PIOWDB_OFLOW			BIT(44)
#define HC_PIORWRB_OFLOW		BIT(43)
#define HC_PIUR_ACC_ERR			BIT(42)
#define HC_BAD_SYSWR_ERR		BIT(41)
#define HC_BAD_SYSRD_ERR		BIT(40)
#define HC_SYSSTATE_ERR_MSK		(0xfUL << 36)
#define HC_SYSSTATE_ERR(x)		BIT((x) + 36)
#define HC_SYSCMD_ERR_MSK		(0xfUL << 32)
#define HC_SYSCMD_ERR(x)		BIT((x) + 32)
#define HC_NCOR_SYSAD_ERR_MSK		(0xfUL << 28)
#define HC_NCOR_SYSAD_ERR(x)		BIT((x) + 28)
#define HC_COR_SYSAD_ERR_MSK		(0xfUL << 24)
#define HC_COR_SYSAD_ERR(x)		BIT((x) + 24)
#define HC_DATA_ELMNT_ERR_MSK		(0xfUL << 20)
#define HC_DATA_ELMNT_ERR(x)		BIT((x) + 20)
#define HC_WIDGET_ERR			BIT(16)
#define HC_MEM_ADDR_ERR_PROC_MSK	(0xfUL << 4)
#define HC_MEM_ADDR_ERR_PROC(x)	BIT((x) + 4)
#define HC_MEM_ADDR_ERR_IO		BIT(2)
#define HC_NCOR_MEM_ERR			BIT(1)
#define HC_COR_MEM_ERR			BIT(0)
#define HEART_NUM_IRQS			64
#define HEART_L4_INT_MASK		0xfff8000000000000ULL	 
#define HEART_L3_INT_MASK		0x0004000000000000ULL	 
#define HEART_L2_INT_MASK		0x0003ffff00000000ULL	 
#define HEART_L1_INT_MASK		0x00000000ffff0000ULL	 
#define HEART_L0_INT_MASK		0x000000000000ffffULL	 
#define HEART_L0_INT_GENERIC		 0
#define HEART_L0_INT_FLOW_CTRL_HWTR_0	 1
#define HEART_L0_INT_FLOW_CTRL_HWTR_1	 2
#define HEART_L2_INT_RESCHED_CPU_0	46
#define HEART_L2_INT_RESCHED_CPU_1	47
#define HEART_L2_INT_CALL_CPU_0		48
#define HEART_L2_INT_CALL_CPU_1		49
#define HEART_L3_INT_TIMER		50
#define HEART_L4_INT_XWID_ERR_9		51
#define HEART_L4_INT_XWID_ERR_A		52
#define HEART_L4_INT_XWID_ERR_B		53
#define HEART_L4_INT_XWID_ERR_C		54
#define HEART_L4_INT_XWID_ERR_D		55
#define HEART_L4_INT_XWID_ERR_E		56
#define HEART_L4_INT_XWID_ERR_F		57
#define HEART_L4_INT_XWID_ERR_XBOW	58
#define HEART_L4_INT_CPU_BUS_ERR_0	59
#define HEART_L4_INT_CPU_BUS_ERR_1	60
#define HEART_L4_INT_CPU_BUS_ERR_2	61
#define HEART_L4_INT_CPU_BUS_ERR_3	62
#define HEART_L4_INT_HEART_EXCP		63
extern struct ip30_heart_regs __iomem *heart_regs;
#define heart_read	____raw_readq
#define heart_write	____raw_writeq
#endif  
