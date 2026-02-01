 
 

#ifndef _DPU_HW_INTERRUPTS_H
#define _DPU_HW_INTERRUPTS_H

#include <linux/types.h>

#include "dpu_hwio.h"
#include "dpu_hw_catalog.h"
#include "dpu_hw_util.h"
#include "dpu_hw_mdss.h"

 
enum dpu_hw_intr_reg {
	MDP_SSPP_TOP0_INTR,
	MDP_SSPP_TOP0_INTR2,
	MDP_SSPP_TOP0_HIST_INTR,
	 
	MDP_INTF0_INTR,
	MDP_INTF1_INTR,
	MDP_INTF2_INTR,
	MDP_INTF3_INTR,
	MDP_INTF4_INTR,
	MDP_INTF5_INTR,
	MDP_INTF6_INTR,
	MDP_INTF7_INTR,
	MDP_INTF8_INTR,
	MDP_INTF1_TEAR_INTR,
	MDP_INTF2_TEAR_INTR,
	MDP_AD4_0_INTR,
	MDP_AD4_1_INTR,
	MDP_INTR_MAX,
};

#define MDP_INTFn_INTR(intf)	(MDP_INTF0_INTR + (intf - INTF_0))

#define DPU_IRQ_IDX(reg_idx, offset)	(reg_idx * 32 + offset)

 
struct dpu_hw_intr {
	struct dpu_hw_blk_reg_map hw;
	u32 cache_irq_mask[MDP_INTR_MAX];
	u32 *save_irq_status;
	u32 total_irqs;
	spinlock_t irq_lock;
	unsigned long irq_mask;
	const struct dpu_intr_reg *intr_set;

	struct {
		void (*cb)(void *arg, int irq_idx);
		void *arg;
		atomic_t count;
	} irq_tbl[];
};

 
struct dpu_hw_intr *dpu_hw_intr_init(void __iomem *addr,
		const struct dpu_mdss_cfg *m);

 
void dpu_hw_intr_destroy(struct dpu_hw_intr *intr);
#endif
