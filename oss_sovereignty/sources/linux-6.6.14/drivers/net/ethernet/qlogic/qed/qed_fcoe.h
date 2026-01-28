


#ifndef _QED_FCOE_H
#define _QED_FCOE_H
#include <linux/types.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/qed/qed_fcoe_if.h>
#include <linux/qed/qed_chain.h>
#include "qed.h"
#include "qed_hsi.h"
#include "qed_mcp.h"
#include "qed_sp.h"

struct qed_fcoe_info {
	spinlock_t lock; 
	struct list_head free_list;
};

#if IS_ENABLED(CONFIG_QED_FCOE)
int qed_fcoe_alloc(struct qed_hwfn *p_hwfn);

void qed_fcoe_setup(struct qed_hwfn *p_hwfn);

void qed_fcoe_free(struct qed_hwfn *p_hwfn);

void qed_get_protocol_stats_fcoe(struct qed_dev *cdev,
				 struct qed_mcp_fcoe_stats *stats,
				 bool is_atomic);
#else 
static inline int qed_fcoe_alloc(struct qed_hwfn *p_hwfn)
{
	return -EINVAL;
}

static inline void qed_fcoe_setup(struct qed_hwfn *p_hwfn) {}
static inline void qed_fcoe_free(struct qed_hwfn *p_hwfn) {}

static inline void qed_get_protocol_stats_fcoe(struct qed_dev *cdev,
					       struct qed_mcp_fcoe_stats *stats,
					       bool is_atomic)
{
}
#endif 

#endif 
