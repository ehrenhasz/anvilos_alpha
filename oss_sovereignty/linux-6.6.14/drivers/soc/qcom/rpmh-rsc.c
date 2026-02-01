
 

#define pr_fmt(fmt) "%s " fmt, KBUILD_MODNAME

#include <linux/atomic.h>
#include <linux/cpu_pm.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#include <clocksource/arm_arch_timer.h>
#include <soc/qcom/cmd-db.h>
#include <soc/qcom/tcs.h>
#include <dt-bindings/soc/qcom,rpmh-rsc.h>

#include "rpmh-internal.h"

#define CREATE_TRACE_POINTS
#include "trace-rpmh.h"


#define RSC_DRV_ID			0

#define MAJOR_VER_MASK			0xFF
#define MAJOR_VER_SHIFT			16
#define MINOR_VER_MASK			0xFF
#define MINOR_VER_SHIFT			8

enum {
	RSC_DRV_TCS_OFFSET,
	RSC_DRV_CMD_OFFSET,
	DRV_SOLVER_CONFIG,
	DRV_PRNT_CHLD_CONFIG,
	RSC_DRV_IRQ_ENABLE,
	RSC_DRV_IRQ_STATUS,
	RSC_DRV_IRQ_CLEAR,
	RSC_DRV_CMD_WAIT_FOR_CMPL,
	RSC_DRV_CONTROL,
	RSC_DRV_STATUS,
	RSC_DRV_CMD_ENABLE,
	RSC_DRV_CMD_MSGID,
	RSC_DRV_CMD_ADDR,
	RSC_DRV_CMD_DATA,
	RSC_DRV_CMD_STATUS,
	RSC_DRV_CMD_RESP_DATA,
};

 
#define DRV_HW_SOLVER_MASK		1
#define DRV_HW_SOLVER_SHIFT		24

 
#define DRV_NUM_TCS_MASK		0x3F
#define DRV_NUM_TCS_SHIFT		6
#define DRV_NCPT_MASK			0x1F
#define DRV_NCPT_SHIFT			27

 
#define RSC_DRV_CTL_TCS_DATA_HI		0x38
#define RSC_DRV_CTL_TCS_DATA_HI_MASK	0xFFFFFF
#define RSC_DRV_CTL_TCS_DATA_HI_VALID	BIT(31)
#define RSC_DRV_CTL_TCS_DATA_LO		0x40
#define RSC_DRV_CTL_TCS_DATA_LO_MASK	0xFFFFFFFF
#define RSC_DRV_CTL_TCS_DATA_SIZE	32

#define TCS_AMC_MODE_ENABLE		BIT(16)
#define TCS_AMC_MODE_TRIGGER		BIT(24)

 
#define CMD_MSGID_LEN			8
#define CMD_MSGID_RESP_REQ		BIT(8)
#define CMD_MSGID_WRITE			BIT(16)
#define CMD_STATUS_ISSUED		BIT(8)
#define CMD_STATUS_COMPL		BIT(16)

 

#define USECS_TO_CYCLES(time_usecs)			\
	xloops_to_cycles((time_usecs) * 0x10C7UL)

static inline unsigned long xloops_to_cycles(u64 xloops)
{
	return (xloops * loops_per_jiffy * HZ) >> 32;
}

static u32 rpmh_rsc_reg_offset_ver_2_7[] = {
	[RSC_DRV_TCS_OFFSET]		= 672,
	[RSC_DRV_CMD_OFFSET]		= 20,
	[DRV_SOLVER_CONFIG]		= 0x04,
	[DRV_PRNT_CHLD_CONFIG]		= 0x0C,
	[RSC_DRV_IRQ_ENABLE]		= 0x00,
	[RSC_DRV_IRQ_STATUS]		= 0x04,
	[RSC_DRV_IRQ_CLEAR]		= 0x08,
	[RSC_DRV_CMD_WAIT_FOR_CMPL]	= 0x10,
	[RSC_DRV_CONTROL]		= 0x14,
	[RSC_DRV_STATUS]		= 0x18,
	[RSC_DRV_CMD_ENABLE]		= 0x1C,
	[RSC_DRV_CMD_MSGID]		= 0x30,
	[RSC_DRV_CMD_ADDR]		= 0x34,
	[RSC_DRV_CMD_DATA]		= 0x38,
	[RSC_DRV_CMD_STATUS]		= 0x3C,
	[RSC_DRV_CMD_RESP_DATA]		= 0x40,
};

static u32 rpmh_rsc_reg_offset_ver_3_0[] = {
	[RSC_DRV_TCS_OFFSET]		= 672,
	[RSC_DRV_CMD_OFFSET]		= 24,
	[DRV_SOLVER_CONFIG]		= 0x04,
	[DRV_PRNT_CHLD_CONFIG]		= 0x0C,
	[RSC_DRV_IRQ_ENABLE]		= 0x00,
	[RSC_DRV_IRQ_STATUS]		= 0x04,
	[RSC_DRV_IRQ_CLEAR]		= 0x08,
	[RSC_DRV_CMD_WAIT_FOR_CMPL]	= 0x20,
	[RSC_DRV_CONTROL]		= 0x24,
	[RSC_DRV_STATUS]		= 0x28,
	[RSC_DRV_CMD_ENABLE]		= 0x2C,
	[RSC_DRV_CMD_MSGID]		= 0x34,
	[RSC_DRV_CMD_ADDR]		= 0x38,
	[RSC_DRV_CMD_DATA]		= 0x3C,
	[RSC_DRV_CMD_STATUS]		= 0x40,
	[RSC_DRV_CMD_RESP_DATA]		= 0x44,
};

static inline void __iomem *
tcs_reg_addr(const struct rsc_drv *drv, int reg, int tcs_id)
{
	return drv->tcs_base + drv->regs[RSC_DRV_TCS_OFFSET] * tcs_id + reg;
}

static inline void __iomem *
tcs_cmd_addr(const struct rsc_drv *drv, int reg, int tcs_id, int cmd_id)
{
	return tcs_reg_addr(drv, reg, tcs_id) + drv->regs[RSC_DRV_CMD_OFFSET] * cmd_id;
}

static u32 read_tcs_cmd(const struct rsc_drv *drv, int reg, int tcs_id,
			int cmd_id)
{
	return readl_relaxed(tcs_cmd_addr(drv, reg, tcs_id, cmd_id));
}

static u32 read_tcs_reg(const struct rsc_drv *drv, int reg, int tcs_id)
{
	return readl_relaxed(tcs_reg_addr(drv, reg, tcs_id));
}

static void write_tcs_cmd(const struct rsc_drv *drv, int reg, int tcs_id,
			  int cmd_id, u32 data)
{
	writel_relaxed(data, tcs_cmd_addr(drv, reg, tcs_id, cmd_id));
}

static void write_tcs_reg(const struct rsc_drv *drv, int reg, int tcs_id,
			  u32 data)
{
	writel_relaxed(data, tcs_reg_addr(drv, reg, tcs_id));
}

static void write_tcs_reg_sync(const struct rsc_drv *drv, int reg, int tcs_id,
			       u32 data)
{
	int i;

	writel(data, tcs_reg_addr(drv, reg, tcs_id));

	 
	for (i = 0; i < USEC_PER_SEC; i++) {
		if (readl(tcs_reg_addr(drv, reg, tcs_id)) == data)
			return;
		udelay(1);
	}
	pr_err("%s: error writing %#x to %d:%#x\n", drv->name,
	       data, tcs_id, reg);
}

 
static void tcs_invalidate(struct rsc_drv *drv, int type)
{
	int m;
	struct tcs_group *tcs = &drv->tcs[type];

	 
	if (bitmap_empty(tcs->slots, MAX_TCS_SLOTS))
		return;

	for (m = tcs->offset; m < tcs->offset + tcs->num_tcs; m++)
		write_tcs_reg_sync(drv, drv->regs[RSC_DRV_CMD_ENABLE], m, 0);

	bitmap_zero(tcs->slots, MAX_TCS_SLOTS);
}

 
void rpmh_rsc_invalidate(struct rsc_drv *drv)
{
	tcs_invalidate(drv, SLEEP_TCS);
	tcs_invalidate(drv, WAKE_TCS);
}

 
static struct tcs_group *get_tcs_for_msg(struct rsc_drv *drv,
					 const struct tcs_request *msg)
{
	int type;
	struct tcs_group *tcs;

	switch (msg->state) {
	case RPMH_ACTIVE_ONLY_STATE:
		type = ACTIVE_TCS;
		break;
	case RPMH_WAKE_ONLY_STATE:
		type = WAKE_TCS;
		break;
	case RPMH_SLEEP_STATE:
		type = SLEEP_TCS;
		break;
	default:
		return ERR_PTR(-EINVAL);
	}

	 
	tcs = &drv->tcs[type];
	if (msg->state == RPMH_ACTIVE_ONLY_STATE && !tcs->num_tcs)
		tcs = &drv->tcs[WAKE_TCS];

	return tcs;
}

 
static const struct tcs_request *get_req_from_tcs(struct rsc_drv *drv,
						  int tcs_id)
{
	struct tcs_group *tcs;
	int i;

	for (i = 0; i < TCS_TYPE_NR; i++) {
		tcs = &drv->tcs[i];
		if (tcs->mask & BIT(tcs_id))
			return tcs->req[tcs_id - tcs->offset];
	}

	return NULL;
}

 
static void __tcs_set_trigger(struct rsc_drv *drv, int tcs_id, bool trigger)
{
	u32 enable;
	u32 reg = drv->regs[RSC_DRV_CONTROL];

	 
	enable = read_tcs_reg(drv, reg, tcs_id);
	enable &= ~TCS_AMC_MODE_TRIGGER;
	write_tcs_reg_sync(drv, reg, tcs_id, enable);
	enable &= ~TCS_AMC_MODE_ENABLE;
	write_tcs_reg_sync(drv, reg, tcs_id, enable);

	if (trigger) {
		 
		enable = TCS_AMC_MODE_ENABLE;
		write_tcs_reg_sync(drv, reg, tcs_id, enable);
		enable |= TCS_AMC_MODE_TRIGGER;
		write_tcs_reg(drv, reg, tcs_id, enable);
	}
}

 
static void enable_tcs_irq(struct rsc_drv *drv, int tcs_id, bool enable)
{
	u32 data;
	u32 reg = drv->regs[RSC_DRV_IRQ_ENABLE];

	data = readl_relaxed(drv->tcs_base + reg);
	if (enable)
		data |= BIT(tcs_id);
	else
		data &= ~BIT(tcs_id);
	writel_relaxed(data, drv->tcs_base + reg);
}

 
static irqreturn_t tcs_tx_done(int irq, void *p)
{
	struct rsc_drv *drv = p;
	int i;
	unsigned long irq_status;
	const struct tcs_request *req;

	irq_status = readl_relaxed(drv->tcs_base + drv->regs[RSC_DRV_IRQ_STATUS]);

	for_each_set_bit(i, &irq_status, BITS_PER_TYPE(u32)) {
		req = get_req_from_tcs(drv, i);
		if (WARN_ON(!req))
			goto skip;

		trace_rpmh_tx_done(drv, i, req);

		 
		if (!drv->tcs[ACTIVE_TCS].num_tcs)
			__tcs_set_trigger(drv, i, false);
skip:
		 
		write_tcs_reg(drv, drv->regs[RSC_DRV_CMD_ENABLE], i, 0);
		writel_relaxed(BIT(i), drv->tcs_base + drv->regs[RSC_DRV_IRQ_CLEAR]);
		spin_lock(&drv->lock);
		clear_bit(i, drv->tcs_in_use);
		 
		if (!drv->tcs[ACTIVE_TCS].num_tcs)
			enable_tcs_irq(drv, i, false);
		spin_unlock(&drv->lock);
		wake_up(&drv->tcs_wait);
		if (req)
			rpmh_tx_done(req);
	}

	return IRQ_HANDLED;
}

 
static void __tcs_buffer_write(struct rsc_drv *drv, int tcs_id, int cmd_id,
			       const struct tcs_request *msg)
{
	u32 msgid;
	u32 cmd_msgid = CMD_MSGID_LEN | CMD_MSGID_WRITE;
	u32 cmd_enable = 0;
	struct tcs_cmd *cmd;
	int i, j;

	 
	cmd_msgid |= msg->wait_for_compl ? CMD_MSGID_RESP_REQ : 0;

	for (i = 0, j = cmd_id; i < msg->num_cmds; i++, j++) {
		cmd = &msg->cmds[i];
		cmd_enable |= BIT(j);
		msgid = cmd_msgid;
		 
		msgid |= cmd->wait ? CMD_MSGID_RESP_REQ : 0;

		write_tcs_cmd(drv, drv->regs[RSC_DRV_CMD_MSGID], tcs_id, j, msgid);
		write_tcs_cmd(drv, drv->regs[RSC_DRV_CMD_ADDR], tcs_id, j, cmd->addr);
		write_tcs_cmd(drv, drv->regs[RSC_DRV_CMD_DATA], tcs_id, j, cmd->data);
		trace_rpmh_send_msg(drv, tcs_id, msg->state, j, msgid, cmd);
	}

	cmd_enable |= read_tcs_reg(drv, drv->regs[RSC_DRV_CMD_ENABLE], tcs_id);
	write_tcs_reg(drv, drv->regs[RSC_DRV_CMD_ENABLE], tcs_id, cmd_enable);
}

 
static int check_for_req_inflight(struct rsc_drv *drv, struct tcs_group *tcs,
				  const struct tcs_request *msg)
{
	unsigned long curr_enabled;
	u32 addr;
	int j, k;
	int i = tcs->offset;

	for_each_set_bit_from(i, drv->tcs_in_use, tcs->offset + tcs->num_tcs) {
		curr_enabled = read_tcs_reg(drv, drv->regs[RSC_DRV_CMD_ENABLE], i);

		for_each_set_bit(j, &curr_enabled, MAX_CMDS_PER_TCS) {
			addr = read_tcs_cmd(drv, drv->regs[RSC_DRV_CMD_ADDR], i, j);
			for (k = 0; k < msg->num_cmds; k++) {
				if (addr == msg->cmds[k].addr)
					return -EBUSY;
			}
		}
	}

	return 0;
}

 
static int find_free_tcs(struct tcs_group *tcs)
{
	const struct rsc_drv *drv = tcs->drv;
	unsigned long i;
	unsigned long max = tcs->offset + tcs->num_tcs;

	i = find_next_zero_bit(drv->tcs_in_use, max, tcs->offset);
	if (i >= max)
		return -EBUSY;

	return i;
}

 
static int claim_tcs_for_req(struct rsc_drv *drv, struct tcs_group *tcs,
			     const struct tcs_request *msg)
{
	int ret;

	 
	ret = check_for_req_inflight(drv, tcs, msg);
	if (ret)
		return ret;

	return find_free_tcs(tcs);
}

 
int rpmh_rsc_send_data(struct rsc_drv *drv, const struct tcs_request *msg)
{
	struct tcs_group *tcs;
	int tcs_id;
	unsigned long flags;

	tcs = get_tcs_for_msg(drv, msg);
	if (IS_ERR(tcs))
		return PTR_ERR(tcs);

	spin_lock_irqsave(&drv->lock, flags);

	 
	wait_event_lock_irq(drv->tcs_wait,
			    (tcs_id = claim_tcs_for_req(drv, tcs, msg)) >= 0,
			    drv->lock);

	tcs->req[tcs_id - tcs->offset] = msg;
	set_bit(tcs_id, drv->tcs_in_use);
	if (msg->state == RPMH_ACTIVE_ONLY_STATE && tcs->type != ACTIVE_TCS) {
		 
		write_tcs_reg_sync(drv, drv->regs[RSC_DRV_CMD_ENABLE], tcs_id, 0);
		enable_tcs_irq(drv, tcs_id, true);
	}
	spin_unlock_irqrestore(&drv->lock, flags);

	 
	__tcs_buffer_write(drv, tcs_id, 0, msg);
	__tcs_set_trigger(drv, tcs_id, true);

	return 0;
}

 
static int find_slots(struct tcs_group *tcs, const struct tcs_request *msg,
		      int *tcs_id, int *cmd_id)
{
	int slot, offset;
	int i = 0;

	 
	do {
		slot = bitmap_find_next_zero_area(tcs->slots, MAX_TCS_SLOTS,
						  i, msg->num_cmds, 0);
		if (slot >= tcs->num_tcs * tcs->ncpt)
			return -ENOMEM;
		i += tcs->ncpt;
	} while (slot + msg->num_cmds - 1 >= i);

	bitmap_set(tcs->slots, slot, msg->num_cmds);

	offset = slot / tcs->ncpt;
	*tcs_id = offset + tcs->offset;
	*cmd_id = slot % tcs->ncpt;

	return 0;
}

 
int rpmh_rsc_write_ctrl_data(struct rsc_drv *drv, const struct tcs_request *msg)
{
	struct tcs_group *tcs;
	int tcs_id = 0, cmd_id = 0;
	int ret;

	tcs = get_tcs_for_msg(drv, msg);
	if (IS_ERR(tcs))
		return PTR_ERR(tcs);

	 
	ret = find_slots(tcs, msg, &tcs_id, &cmd_id);
	if (!ret)
		__tcs_buffer_write(drv, tcs_id, cmd_id, msg);

	return ret;
}

 
static bool rpmh_rsc_ctrlr_is_busy(struct rsc_drv *drv)
{
	unsigned long set;
	const struct tcs_group *tcs = &drv->tcs[ACTIVE_TCS];
	unsigned long max;

	 
	if (!tcs->num_tcs)
		tcs = &drv->tcs[WAKE_TCS];

	max = tcs->offset + tcs->num_tcs;
	set = find_next_bit(drv->tcs_in_use, max, tcs->offset);

	return set < max;
}

 
void rpmh_rsc_write_next_wakeup(struct rsc_drv *drv)
{
	ktime_t now, wakeup;
	u64 wakeup_us, wakeup_cycles = ~0;
	u32 lo, hi;

	if (!drv->tcs[CONTROL_TCS].num_tcs || !drv->genpd_nb.notifier_call)
		return;

	 
	if (system_state == SYSTEM_SUSPEND)
		goto exit;

	 
	wakeup = dev_pm_genpd_get_next_hrtimer(drv->dev);

	 
	now = ktime_get();
	wakeup = ktime_sub(wakeup, now);
	wakeup_us = ktime_to_us(wakeup);

	 
	wakeup_cycles = USECS_TO_CYCLES(wakeup_us);
	wakeup_cycles += arch_timer_read_counter();

exit:
	lo = wakeup_cycles & RSC_DRV_CTL_TCS_DATA_LO_MASK;
	hi = wakeup_cycles >> RSC_DRV_CTL_TCS_DATA_SIZE;
	hi &= RSC_DRV_CTL_TCS_DATA_HI_MASK;
	hi |= RSC_DRV_CTL_TCS_DATA_HI_VALID;

	writel_relaxed(lo, drv->base + RSC_DRV_CTL_TCS_DATA_LO);
	writel_relaxed(hi, drv->base + RSC_DRV_CTL_TCS_DATA_HI);
}

 
static int rpmh_rsc_cpu_pm_callback(struct notifier_block *nfb,
				    unsigned long action, void *v)
{
	struct rsc_drv *drv = container_of(nfb, struct rsc_drv, rsc_pm);
	int ret = NOTIFY_OK;
	int cpus_in_pm;

	switch (action) {
	case CPU_PM_ENTER:
		cpus_in_pm = atomic_inc_return(&drv->cpus_in_pm);
		 
		if (cpus_in_pm < num_online_cpus())
			return NOTIFY_OK;
		break;
	case CPU_PM_ENTER_FAILED:
	case CPU_PM_EXIT:
		atomic_dec(&drv->cpus_in_pm);
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}

	 
	if (spin_trylock(&drv->lock)) {
		if (rpmh_rsc_ctrlr_is_busy(drv) || rpmh_flush(&drv->client))
			ret = NOTIFY_BAD;
		spin_unlock(&drv->lock);
	} else {
		 
		return NOTIFY_OK;
	}

	if (ret == NOTIFY_BAD) {
		 
		if (cpus_in_pm < num_online_cpus())
			ret = NOTIFY_OK;
		else
			 
			atomic_dec(&drv->cpus_in_pm);
	}

	return ret;
}

 
static int rpmh_rsc_pd_callback(struct notifier_block *nfb,
				unsigned long action, void *v)
{
	struct rsc_drv *drv = container_of(nfb, struct rsc_drv, genpd_nb);

	 
	if ((action == GENPD_NOTIFY_PRE_OFF) &&
	    (rpmh_rsc_ctrlr_is_busy(drv) || rpmh_flush(&drv->client)))
		return NOTIFY_BAD;

	return NOTIFY_OK;
}

static int rpmh_rsc_pd_attach(struct rsc_drv *drv, struct device *dev)
{
	int ret;

	pm_runtime_enable(dev);
	drv->genpd_nb.notifier_call = rpmh_rsc_pd_callback;
	ret = dev_pm_genpd_add_notifier(dev, &drv->genpd_nb);
	if (ret)
		pm_runtime_disable(dev);

	return ret;
}

static int rpmh_probe_tcs_config(struct platform_device *pdev, struct rsc_drv *drv)
{
	struct tcs_type_config {
		u32 type;
		u32 n;
	} tcs_cfg[TCS_TYPE_NR] = { { 0 } };
	struct device_node *dn = pdev->dev.of_node;
	u32 config, max_tcs, ncpt, offset;
	int i, ret, n, st = 0;
	struct tcs_group *tcs;

	ret = of_property_read_u32(dn, "qcom,tcs-offset", &offset);
	if (ret)
		return ret;
	drv->tcs_base = drv->base + offset;

	config = readl_relaxed(drv->base + drv->regs[DRV_PRNT_CHLD_CONFIG]);

	max_tcs = config;
	max_tcs &= DRV_NUM_TCS_MASK << (DRV_NUM_TCS_SHIFT * drv->id);
	max_tcs = max_tcs >> (DRV_NUM_TCS_SHIFT * drv->id);

	ncpt = config & (DRV_NCPT_MASK << DRV_NCPT_SHIFT);
	ncpt = ncpt >> DRV_NCPT_SHIFT;

	n = of_property_count_u32_elems(dn, "qcom,tcs-config");
	if (n != 2 * TCS_TYPE_NR)
		return -EINVAL;

	for (i = 0; i < TCS_TYPE_NR; i++) {
		ret = of_property_read_u32_index(dn, "qcom,tcs-config",
						 i * 2, &tcs_cfg[i].type);
		if (ret)
			return ret;
		if (tcs_cfg[i].type >= TCS_TYPE_NR)
			return -EINVAL;

		ret = of_property_read_u32_index(dn, "qcom,tcs-config",
						 i * 2 + 1, &tcs_cfg[i].n);
		if (ret)
			return ret;
		if (tcs_cfg[i].n > MAX_TCS_PER_TYPE)
			return -EINVAL;
	}

	for (i = 0; i < TCS_TYPE_NR; i++) {
		tcs = &drv->tcs[tcs_cfg[i].type];
		if (tcs->drv)
			return -EINVAL;
		tcs->drv = drv;
		tcs->type = tcs_cfg[i].type;
		tcs->num_tcs = tcs_cfg[i].n;
		tcs->ncpt = ncpt;

		if (!tcs->num_tcs || tcs->type == CONTROL_TCS)
			continue;

		if (st + tcs->num_tcs > max_tcs ||
		    st + tcs->num_tcs >= BITS_PER_BYTE * sizeof(tcs->mask))
			return -EINVAL;

		tcs->mask = ((1 << tcs->num_tcs) - 1) << st;
		tcs->offset = st;
		st += tcs->num_tcs;
	}

	drv->num_tcs = st;

	return 0;
}

static int rpmh_rsc_probe(struct platform_device *pdev)
{
	struct device_node *dn = pdev->dev.of_node;
	struct rsc_drv *drv;
	char drv_id[10] = {0};
	int ret, irq;
	u32 solver_config;
	u32 rsc_id;

	 
	ret = cmd_db_ready();
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Command DB not available (%d)\n",
									ret);
		return ret;
	}

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	ret = of_property_read_u32(dn, "qcom,drv-id", &drv->id);
	if (ret)
		return ret;

	drv->name = of_get_property(dn, "label", NULL);
	if (!drv->name)
		drv->name = dev_name(&pdev->dev);

	snprintf(drv_id, ARRAY_SIZE(drv_id), "drv-%d", drv->id);
	drv->base = devm_platform_ioremap_resource_byname(pdev, drv_id);
	if (IS_ERR(drv->base))
		return PTR_ERR(drv->base);

	rsc_id = readl_relaxed(drv->base + RSC_DRV_ID);
	drv->ver.major = rsc_id & (MAJOR_VER_MASK << MAJOR_VER_SHIFT);
	drv->ver.major >>= MAJOR_VER_SHIFT;
	drv->ver.minor = rsc_id & (MINOR_VER_MASK << MINOR_VER_SHIFT);
	drv->ver.minor >>= MINOR_VER_SHIFT;

	if (drv->ver.major == 3)
		drv->regs = rpmh_rsc_reg_offset_ver_3_0;
	else
		drv->regs = rpmh_rsc_reg_offset_ver_2_7;

	ret = rpmh_probe_tcs_config(pdev, drv);
	if (ret)
		return ret;

	spin_lock_init(&drv->lock);
	init_waitqueue_head(&drv->tcs_wait);
	bitmap_zero(drv->tcs_in_use, MAX_TCS_NR);

	irq = platform_get_irq(pdev, drv->id);
	if (irq < 0)
		return irq;

	ret = devm_request_irq(&pdev->dev, irq, tcs_tx_done,
			       IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND,
			       drv->name, drv);
	if (ret)
		return ret;

	 
	solver_config = readl_relaxed(drv->base + drv->regs[DRV_SOLVER_CONFIG]);
	solver_config &= DRV_HW_SOLVER_MASK << DRV_HW_SOLVER_SHIFT;
	solver_config = solver_config >> DRV_HW_SOLVER_SHIFT;
	if (!solver_config) {
		if (pdev->dev.pm_domain) {
			ret = rpmh_rsc_pd_attach(drv, &pdev->dev);
			if (ret)
				return ret;
		} else {
			drv->rsc_pm.notifier_call = rpmh_rsc_cpu_pm_callback;
			cpu_pm_register_notifier(&drv->rsc_pm);
		}
	}

	 
	writel_relaxed(drv->tcs[ACTIVE_TCS].mask,
		       drv->tcs_base + drv->regs[RSC_DRV_IRQ_ENABLE]);

	spin_lock_init(&drv->client.cache_lock);
	INIT_LIST_HEAD(&drv->client.cache);
	INIT_LIST_HEAD(&drv->client.batch_cache);

	dev_set_drvdata(&pdev->dev, drv);
	drv->dev = &pdev->dev;

	ret = devm_of_platform_populate(&pdev->dev);
	if (ret && pdev->dev.pm_domain) {
		dev_pm_genpd_remove_notifier(&pdev->dev);
		pm_runtime_disable(&pdev->dev);
	}

	return ret;
}

static const struct of_device_id rpmh_drv_match[] = {
	{ .compatible = "qcom,rpmh-rsc", },
	{ }
};
MODULE_DEVICE_TABLE(of, rpmh_drv_match);

static struct platform_driver rpmh_driver = {
	.probe = rpmh_rsc_probe,
	.driver = {
		  .name = "rpmh",
		  .of_match_table = rpmh_drv_match,
		  .suppress_bind_attrs = true,
	},
};

static int __init rpmh_driver_init(void)
{
	return platform_driver_register(&rpmh_driver);
}
arch_initcall(rpmh_driver_init);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. RPMh Driver");
MODULE_LICENSE("GPL v2");
