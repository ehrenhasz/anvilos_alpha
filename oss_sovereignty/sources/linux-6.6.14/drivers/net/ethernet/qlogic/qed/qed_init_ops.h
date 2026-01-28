


#ifndef _QED_INIT_OPS_H
#define _QED_INIT_OPS_H

#include <linux/types.h>
#include <linux/slab.h>
#include "qed.h"


void qed_init_iro_array(struct qed_dev *cdev);


int qed_init_run(struct qed_hwfn *p_hwfn,
		 struct qed_ptt *p_ptt,
		 int phase,
		 int phase_id,
		 int modes);


int qed_init_alloc(struct qed_hwfn *p_hwfn);


void qed_init_free(struct qed_hwfn *p_hwfn);


void qed_init_store_rt_reg(struct qed_hwfn *p_hwfn,
			   u32 rt_offset,
			   u32 val);

#define STORE_RT_REG(hwfn, offset, val)	\
	qed_init_store_rt_reg(hwfn, offset, val)

#define OVERWRITE_RT_REG(hwfn, offset, val) \
	qed_init_store_rt_reg(hwfn, offset, val)

void qed_init_store_rt_agg(struct qed_hwfn *p_hwfn,
			   u32 rt_offset,
			   u32 *val,
			   size_t size);

#define STORE_RT_REG_AGG(hwfn, offset, val) \
	qed_init_store_rt_agg(hwfn, offset, (u32 *)&(val), sizeof(val))


void qed_gtt_init(struct qed_hwfn *p_hwfn);
#endif
