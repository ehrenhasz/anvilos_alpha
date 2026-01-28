



#ifndef __RPM_INTERNAL_H__
#define __RPM_INTERNAL_H__

#include <linux/bitmap.h>
#include <linux/wait.h>
#include <soc/qcom/tcs.h>

#define TCS_TYPE_NR			4
#define MAX_CMDS_PER_TCS		16
#define MAX_TCS_PER_TYPE		3
#define MAX_TCS_NR			(MAX_TCS_PER_TYPE * TCS_TYPE_NR)
#define MAX_TCS_SLOTS			(MAX_CMDS_PER_TCS * MAX_TCS_PER_TYPE)

struct rsc_drv;


struct tcs_group {
	struct rsc_drv *drv;
	int type;
	u32 mask;
	u32 offset;
	int num_tcs;
	int ncpt;
	const struct tcs_request *req[MAX_TCS_PER_TYPE];
	DECLARE_BITMAP(slots, MAX_TCS_SLOTS);
};


struct rpmh_request {
	struct tcs_request msg;
	struct tcs_cmd cmd[MAX_RPMH_PAYLOAD];
	struct completion *completion;
	const struct device *dev;
	bool needs_free;
};


struct rpmh_ctrlr {
	struct list_head cache;
	spinlock_t cache_lock;
	bool dirty;
	struct list_head batch_cache;
};

struct rsc_ver {
	u32 major;
	u32 minor;
};


struct rsc_drv {
	const char *name;
	void __iomem *base;
	void __iomem *tcs_base;
	int id;
	int num_tcs;
	struct notifier_block rsc_pm;
	struct notifier_block genpd_nb;
	atomic_t cpus_in_pm;
	struct tcs_group tcs[TCS_TYPE_NR];
	DECLARE_BITMAP(tcs_in_use, MAX_TCS_NR);
	spinlock_t lock;
	wait_queue_head_t tcs_wait;
	struct rpmh_ctrlr client;
	struct device *dev;
	struct rsc_ver ver;
	u32 *regs;
};

int rpmh_rsc_send_data(struct rsc_drv *drv, const struct tcs_request *msg);
int rpmh_rsc_write_ctrl_data(struct rsc_drv *drv,
			     const struct tcs_request *msg);
void rpmh_rsc_invalidate(struct rsc_drv *drv);
void rpmh_rsc_write_next_wakeup(struct rsc_drv *drv);

void rpmh_tx_done(const struct tcs_request *msg);
int rpmh_flush(struct rpmh_ctrlr *ctrlr);

#endif 
