#ifndef _CORESIGHT_CORESIGHT_ETM_H
#define _CORESIGHT_CORESIGHT_ETM_H
#include <asm/local.h>
#include <linux/spinlock.h>
#include "coresight-priv.h"
#define ETMCR			0x000
#define ETMCCR			0x004
#define ETMTRIGGER		0x008
#define ETMSR			0x010
#define ETMSCR			0x014
#define ETMTSSCR		0x018
#define ETMTECR2		0x01c
#define ETMTEEVR		0x020
#define ETMTECR1		0x024
#define ETMFFLR			0x02c
#define ETMACVRn(n)		(0x040 + (n * 4))
#define ETMACTRn(n)		(0x080 + (n * 4))
#define ETMCNTRLDVRn(n)		(0x140 + (n * 4))
#define ETMCNTENRn(n)		(0x150 + (n * 4))
#define ETMCNTRLDEVRn(n)	(0x160 + (n * 4))
#define ETMCNTVRn(n)		(0x170 + (n * 4))
#define ETMSQ12EVR		0x180
#define ETMSQ21EVR		0x184
#define ETMSQ23EVR		0x188
#define ETMSQ31EVR		0x18c
#define ETMSQ32EVR		0x190
#define ETMSQ13EVR		0x194
#define ETMSQR			0x19c
#define ETMEXTOUTEVRn(n)	(0x1a0 + (n * 4))
#define ETMCIDCVRn(n)		(0x1b0 + (n * 4))
#define ETMCIDCMR		0x1bc
#define ETMIMPSPEC0		0x1c0
#define ETMIMPSPEC1		0x1c4
#define ETMIMPSPEC2		0x1c8
#define ETMIMPSPEC3		0x1cc
#define ETMIMPSPEC4		0x1d0
#define ETMIMPSPEC5		0x1d4
#define ETMIMPSPEC6		0x1d8
#define ETMIMPSPEC7		0x1dc
#define ETMSYNCFR		0x1e0
#define ETMIDR			0x1e4
#define ETMCCER			0x1e8
#define ETMEXTINSELR		0x1ec
#define ETMTESSEICR		0x1f0
#define ETMEIBCR		0x1f4
#define ETMTSEVR		0x1f8
#define ETMAUXCR		0x1fc
#define ETMTRACEIDR		0x200
#define ETMVMIDCVR		0x240
#define ETMOSLAR		0x300
#define ETMOSLSR		0x304
#define ETMOSSRR		0x308
#define ETMPDCR			0x310
#define ETMPDSR			0x314
#define ETM_MAX_ADDR_CMP	16
#define ETM_MAX_CNTR		4
#define ETM_MAX_CTXID_CMP	3
#define ETMCR_PWD_DWN		BIT(0)
#define ETMCR_STALL_MODE	BIT(7)
#define ETMCR_BRANCH_BROADCAST	BIT(8)
#define ETMCR_ETM_PRG		BIT(10)
#define ETMCR_ETM_EN		BIT(11)
#define ETMCR_CYC_ACC		BIT(12)
#define ETMCR_CTXID_SIZE	(BIT(14)|BIT(15))
#define ETMCR_TIMESTAMP_EN	BIT(28)
#define ETMCR_RETURN_STACK	BIT(29)
#define ETMCCR_FIFOFULL		BIT(23)
#define ETMPDCR_PWD_UP		BIT(3)
#define ETMTECR1_ADDR_COMP_1	BIT(0)
#define ETMTECR1_INC_EXC	BIT(24)
#define ETMTECR1_START_STOP	BIT(25)
#define ETMCCER_TIMESTAMP	BIT(22)
#define ETMCCER_RETSTACK	BIT(23)
#define ETM_MODE_EXCLUDE	BIT(0)
#define ETM_MODE_CYCACC		BIT(1)
#define ETM_MODE_STALL		BIT(2)
#define ETM_MODE_TIMESTAMP	BIT(3)
#define ETM_MODE_CTXID		BIT(4)
#define ETM_MODE_BBROAD		BIT(5)
#define ETM_MODE_RET_STACK	BIT(6)
#define ETM_MODE_ALL		(ETM_MODE_EXCLUDE | ETM_MODE_CYCACC | \
				 ETM_MODE_STALL | ETM_MODE_TIMESTAMP | \
				 ETM_MODE_BBROAD | ETM_MODE_RET_STACK | \
				 ETM_MODE_CTXID | ETM_MODE_EXCL_KERN | \
				 ETM_MODE_EXCL_USER)
#define ETM_SQR_MASK		0x3
#define ETM_TRACEID_MASK	0x3f
#define ETM_EVENT_MASK		0x1ffff
#define ETM_SYNC_MASK		0xfff
#define ETM_ALL_MASK		0xffffffff
#define ETMSR_PROG_BIT		1
#define ETM_SEQ_STATE_MAX_VAL	(0x2)
#define PORT_SIZE_MASK		(GENMASK(21, 21) | GENMASK(6, 4))
#define ETM_HARD_WIRE_RES_A	 	\
				((0x0f << 0)	|		\
				 		\
				(0x06 << 4))
#define ETM_ADD_COMP_0		 	\
				((0x00 << 7)	|		\
				 		\
				(0x00 << 11))
#define ETM_EVENT_NOT_A		BIT(14)  
#define ETM_DEFAULT_EVENT_VAL	(ETM_HARD_WIRE_RES_A	|	\
				 ETM_ADD_COMP_0		|	\
				 ETM_EVENT_NOT_A)
struct etm_config {
	u32				mode;
	u32				ctrl;
	u32				trigger_event;
	u32				startstop_ctrl;
	u32				enable_event;
	u32				enable_ctrl1;
	u32				enable_ctrl2;
	u32				fifofull_level;
	u8				addr_idx;
	u32				addr_val[ETM_MAX_ADDR_CMP];
	u32				addr_acctype[ETM_MAX_ADDR_CMP];
	u32				addr_type[ETM_MAX_ADDR_CMP];
	u8				cntr_idx;
	u32				cntr_rld_val[ETM_MAX_CNTR];
	u32				cntr_event[ETM_MAX_CNTR];
	u32				cntr_rld_event[ETM_MAX_CNTR];
	u32				cntr_val[ETM_MAX_CNTR];
	u32				seq_12_event;
	u32				seq_21_event;
	u32				seq_23_event;
	u32				seq_31_event;
	u32				seq_32_event;
	u32				seq_13_event;
	u32				seq_curr_state;
	u8				ctxid_idx;
	u32				ctxid_pid[ETM_MAX_CTXID_CMP];
	u32				ctxid_mask;
	u32				sync_freq;
	u32				timestamp_event;
};
struct etm_drvdata {
	void __iomem			*base;
	struct clk			*atclk;
	struct coresight_device		*csdev;
	spinlock_t			spinlock;
	int				cpu;
	int				port_size;
	u8				arch;
	bool				use_cp14;
	local_t				mode;
	bool				sticky_enable;
	bool				boot_enable;
	bool				os_unlock;
	u8				nr_addr_cmp;
	u8				nr_cntr;
	u8				nr_ext_inp;
	u8				nr_ext_out;
	u8				nr_ctxid_cmp;
	u32				etmccr;
	u32				etmccer;
	u32				traceid;
	struct etm_config		config;
};
static inline void etm_writel(struct etm_drvdata *drvdata,
			      u32 val, u32 off)
{
	if (drvdata->use_cp14) {
		if (etm_writel_cp14(off, val)) {
			dev_err(&drvdata->csdev->dev,
				"invalid CP14 access to ETM reg: %#x", off);
		}
	} else {
		writel_relaxed(val, drvdata->base + off);
	}
}
static inline unsigned int etm_readl(struct etm_drvdata *drvdata, u32 off)
{
	u32 val;
	if (drvdata->use_cp14) {
		if (etm_readl_cp14(off, &val)) {
			dev_err(&drvdata->csdev->dev,
				"invalid CP14 access to ETM reg: %#x", off);
		}
	} else {
		val = readl_relaxed(drvdata->base + off);
	}
	return val;
}
extern const struct attribute_group *coresight_etm_groups[];
void etm_set_default(struct etm_config *config);
void etm_config_trace_mode(struct etm_config *config);
struct etm_config *get_etm_config(struct etm_drvdata *drvdata);
int etm_read_alloc_trace_id(struct etm_drvdata *drvdata);
void etm_release_trace_id(struct etm_drvdata *drvdata);
#endif
