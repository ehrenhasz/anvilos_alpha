
 

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/soc/ti/ti-msgmgr.h>

#define Q_DATA_OFFSET(proxy, queue, reg)	\
		     ((0x10000 * (proxy)) + (0x80 * (queue)) + ((reg) * 4))
#define Q_STATE_OFFSET(queue)			((queue) * 0x4)
#define Q_STATE_ENTRY_COUNT_MASK		(0xFFF000)

#define SPROXY_THREAD_OFFSET(tid) (0x1000 * (tid))
#define SPROXY_THREAD_DATA_OFFSET(tid, reg) \
	(SPROXY_THREAD_OFFSET(tid) + ((reg) * 0x4) + 0x4)

#define SPROXY_THREAD_STATUS_OFFSET(tid) (SPROXY_THREAD_OFFSET(tid))

#define SPROXY_THREAD_STATUS_COUNT_MASK (0xFF)

#define SPROXY_THREAD_CTRL_OFFSET(tid) (0x1000 + SPROXY_THREAD_OFFSET(tid))
#define SPROXY_THREAD_CTRL_DIR_MASK (0x1 << 31)

 
struct ti_msgmgr_valid_queue_desc {
	u8 queue_id;
	u8 proxy_id;
	bool is_tx;
};

 
struct ti_msgmgr_desc {
	u8 queue_count;
	u8 max_message_size;
	u8 max_messages;
	u8 data_first_reg;
	u8 data_last_reg;
	u32 status_cnt_mask;
	u32 status_err_mask;
	bool tx_polled;
	int tx_poll_timeout_ms;
	const struct ti_msgmgr_valid_queue_desc *valid_queues;
	const char *data_region_name;
	const char *status_region_name;
	const char *ctrl_region_name;
	int num_valid_queues;
	bool is_sproxy;
};

 
struct ti_queue_inst {
	char name[30];
	u8 queue_id;
	u8 proxy_id;
	int irq;
	bool is_tx;
	void __iomem *queue_buff_start;
	void __iomem *queue_buff_end;
	void __iomem *queue_state;
	void __iomem *queue_ctrl;
	struct mbox_chan *chan;
	u32 *rx_buff;
	bool polled_rx_mode;
};

 
struct ti_msgmgr_inst {
	struct device *dev;
	const struct ti_msgmgr_desc *desc;
	void __iomem *queue_proxy_region;
	void __iomem *queue_state_debug_region;
	void __iomem *queue_ctrl_region;
	u8 num_valid_queues;
	struct ti_queue_inst *qinsts;
	struct mbox_controller mbox;
	struct mbox_chan *chans;
};

 
static inline int
ti_msgmgr_queue_get_num_messages(const struct ti_msgmgr_desc *d,
				 struct ti_queue_inst *qinst)
{
	u32 val;
	u32 status_cnt_mask = d->status_cnt_mask;

	 
	val = readl(qinst->queue_state) & status_cnt_mask;
	val >>= __ffs(status_cnt_mask);

	return val;
}

 
static inline bool ti_msgmgr_queue_is_error(const struct ti_msgmgr_desc *d,
					    struct ti_queue_inst *qinst)
{
	u32 val;

	 
	if (!d->is_sproxy)
		return false;

	 
	val = readl(qinst->queue_state) & d->status_err_mask;

	return val ? true : false;
}

static int ti_msgmgr_queue_rx_data(struct mbox_chan *chan, struct ti_queue_inst *qinst,
				   const struct ti_msgmgr_desc *desc)
{
	int num_words;
	struct ti_msgmgr_message message;
	void __iomem *data_reg;
	u32 *word_data;

	 
	message.len = desc->max_message_size;
	message.buf = (u8 *)qinst->rx_buff;

	 
	for (data_reg = qinst->queue_buff_start, word_data = qinst->rx_buff,
	     num_words = (desc->max_message_size / sizeof(u32));
	     num_words; num_words--, data_reg += sizeof(u32), word_data++)
		*word_data = readl(data_reg);

	 
	mbox_chan_received_data(chan, (void *)&message);

	return 0;
}

static int ti_msgmgr_queue_rx_poll_timeout(struct mbox_chan *chan, int timeout_us)
{
	struct device *dev = chan->mbox->dev;
	struct ti_msgmgr_inst *inst = dev_get_drvdata(dev);
	struct ti_queue_inst *qinst = chan->con_priv;
	const struct ti_msgmgr_desc *desc = inst->desc;
	int msg_count;
	int ret;

	ret = readl_poll_timeout_atomic(qinst->queue_state, msg_count,
					(msg_count & desc->status_cnt_mask),
					10, timeout_us);
	if (ret != 0)
		return ret;

	ti_msgmgr_queue_rx_data(chan, qinst, desc);

	return 0;
}

 
static irqreturn_t ti_msgmgr_queue_rx_interrupt(int irq, void *p)
{
	struct mbox_chan *chan = p;
	struct device *dev = chan->mbox->dev;
	struct ti_msgmgr_inst *inst = dev_get_drvdata(dev);
	struct ti_queue_inst *qinst = chan->con_priv;
	const struct ti_msgmgr_desc *desc;
	int msg_count;

	if (WARN_ON(!inst)) {
		dev_err(dev, "no platform drv data??\n");
		return -EINVAL;
	}

	 
	if (qinst->is_tx) {
		dev_err(dev, "Cannot handle rx interrupt on tx channel %s\n",
			qinst->name);
		return IRQ_NONE;
	}

	desc = inst->desc;
	if (ti_msgmgr_queue_is_error(desc, qinst)) {
		dev_err(dev, "Error on Rx channel %s\n", qinst->name);
		return IRQ_NONE;
	}

	 
	msg_count = ti_msgmgr_queue_get_num_messages(desc, qinst);
	if (!msg_count) {
		 
		dev_dbg(dev, "Spurious event - 0 pending data!\n");
		return IRQ_NONE;
	}

	ti_msgmgr_queue_rx_data(chan, qinst, desc);

	return IRQ_HANDLED;
}

 
static bool ti_msgmgr_queue_peek_data(struct mbox_chan *chan)
{
	struct ti_queue_inst *qinst = chan->con_priv;
	struct device *dev = chan->mbox->dev;
	struct ti_msgmgr_inst *inst = dev_get_drvdata(dev);
	const struct ti_msgmgr_desc *desc = inst->desc;
	int msg_count;

	if (qinst->is_tx)
		return false;

	if (ti_msgmgr_queue_is_error(desc, qinst)) {
		dev_err(dev, "Error on channel %s\n", qinst->name);
		return false;
	}

	msg_count = ti_msgmgr_queue_get_num_messages(desc, qinst);

	return msg_count ? true : false;
}

 
static bool ti_msgmgr_last_tx_done(struct mbox_chan *chan)
{
	struct ti_queue_inst *qinst = chan->con_priv;
	struct device *dev = chan->mbox->dev;
	struct ti_msgmgr_inst *inst = dev_get_drvdata(dev);
	const struct ti_msgmgr_desc *desc = inst->desc;
	int msg_count;

	if (!qinst->is_tx)
		return false;

	if (ti_msgmgr_queue_is_error(desc, qinst)) {
		dev_err(dev, "Error on channel %s\n", qinst->name);
		return false;
	}

	msg_count = ti_msgmgr_queue_get_num_messages(desc, qinst);

	if (desc->is_sproxy) {
		 
		return msg_count ? true : false;
	}

	 
	return msg_count ? false : true;
}

static bool ti_msgmgr_chan_has_polled_queue_rx(struct mbox_chan *chan)
{
	struct ti_queue_inst *qinst;

	if (!chan)
		return false;

	qinst = chan->con_priv;
	return qinst->polled_rx_mode;
}

 
static int ti_msgmgr_send_data(struct mbox_chan *chan, void *data)
{
	struct device *dev = chan->mbox->dev;
	struct ti_msgmgr_inst *inst = dev_get_drvdata(dev);
	const struct ti_msgmgr_desc *desc;
	struct ti_queue_inst *qinst = chan->con_priv;
	int num_words, trail_bytes;
	struct ti_msgmgr_message *message = data;
	void __iomem *data_reg;
	u32 *word_data;
	int ret = 0;

	if (WARN_ON(!inst)) {
		dev_err(dev, "no platform drv data??\n");
		return -EINVAL;
	}
	desc = inst->desc;

	if (ti_msgmgr_queue_is_error(desc, qinst)) {
		dev_err(dev, "Error on channel %s\n", qinst->name);
		return false;
	}

	if (desc->max_message_size < message->len) {
		dev_err(dev, "Queue %s message length %zu > max %d\n",
			qinst->name, message->len, desc->max_message_size);
		return -EINVAL;
	}

	 
	for (data_reg = qinst->queue_buff_start,
	     num_words = message->len / sizeof(u32),
	     word_data = (u32 *)message->buf;
	     num_words; num_words--, data_reg += sizeof(u32), word_data++)
		writel(*word_data, data_reg);

	trail_bytes = message->len % sizeof(u32);
	if (trail_bytes) {
		u32 data_trail = *word_data;

		 
		data_trail &= 0xFFFFFFFF >> (8 * (sizeof(u32) - trail_bytes));
		writel(data_trail, data_reg);
		data_reg += sizeof(u32);
	}

	 
	while (data_reg <= qinst->queue_buff_end) {
		writel(0, data_reg);
		data_reg += sizeof(u32);
	}

	 
	if (ti_msgmgr_chan_has_polled_queue_rx(message->chan_rx))
		ret = ti_msgmgr_queue_rx_poll_timeout(message->chan_rx,
						      message->timeout_rx_ms * 1000);

	return ret;
}

 
static int ti_msgmgr_queue_rx_irq_req(struct device *dev,
				      const struct ti_msgmgr_desc *d,
				      struct ti_queue_inst *qinst,
				      struct mbox_chan *chan)
{
	int ret = 0;
	char of_rx_irq_name[7];
	struct device_node *np;

	snprintf(of_rx_irq_name, sizeof(of_rx_irq_name),
		 "rx_%03d", d->is_sproxy ? qinst->proxy_id : qinst->queue_id);

	 
	if (qinst->irq < 0) {
		np = of_node_get(dev->of_node);
		if (!np)
			return -ENODATA;
		qinst->irq = of_irq_get_byname(np, of_rx_irq_name);
		of_node_put(np);

		if (qinst->irq < 0) {
			dev_err(dev,
				"QID %d PID %d:No IRQ[%s]: %d\n",
				qinst->queue_id, qinst->proxy_id,
				of_rx_irq_name, qinst->irq);
			return qinst->irq;
		}
	}

	 
	ret = request_irq(qinst->irq, ti_msgmgr_queue_rx_interrupt,
			  IRQF_SHARED, qinst->name, chan);
	if (ret) {
		dev_err(dev, "Unable to get IRQ %d on %s(res=%d)\n",
			qinst->irq, qinst->name, ret);
	}

	return ret;
}

 
static int ti_msgmgr_queue_startup(struct mbox_chan *chan)
{
	struct device *dev = chan->mbox->dev;
	struct ti_msgmgr_inst *inst = dev_get_drvdata(dev);
	struct ti_queue_inst *qinst = chan->con_priv;
	const struct ti_msgmgr_desc *d = inst->desc;
	int ret;
	int msg_count;

	 
	if (d->is_sproxy) {
		qinst->is_tx = (readl(qinst->queue_ctrl) &
				SPROXY_THREAD_CTRL_DIR_MASK) ? false : true;

		msg_count = ti_msgmgr_queue_get_num_messages(d, qinst);

		if (!msg_count && qinst->is_tx) {
			dev_err(dev, "%s: Cannot transmit with 0 credits!\n",
				qinst->name);
			return -EINVAL;
		}
	}

	if (!qinst->is_tx) {
		 
		qinst->rx_buff = kzalloc(d->max_message_size, GFP_KERNEL);
		if (!qinst->rx_buff)
			return -ENOMEM;
		 
		ret = ti_msgmgr_queue_rx_irq_req(dev, d, qinst, chan);
		if (ret) {
			kfree(qinst->rx_buff);
			return ret;
		}
	}

	return 0;
}

 
static void ti_msgmgr_queue_shutdown(struct mbox_chan *chan)
{
	struct ti_queue_inst *qinst = chan->con_priv;

	if (!qinst->is_tx) {
		free_irq(qinst->irq, chan);
		kfree(qinst->rx_buff);
	}
}

 
static struct mbox_chan *ti_msgmgr_of_xlate(struct mbox_controller *mbox,
					    const struct of_phandle_args *p)
{
	struct ti_msgmgr_inst *inst;
	int req_qid, req_pid;
	struct ti_queue_inst *qinst;
	const struct ti_msgmgr_desc *d;
	int i, ncells;

	inst = container_of(mbox, struct ti_msgmgr_inst, mbox);
	if (WARN_ON(!inst))
		return ERR_PTR(-EINVAL);

	d = inst->desc;

	if (d->is_sproxy)
		ncells = 1;
	else
		ncells = 2;
	if (p->args_count != ncells) {
		dev_err(inst->dev, "Invalid arguments in dt[%d]. Must be %d\n",
			p->args_count, ncells);
		return ERR_PTR(-EINVAL);
	}
	if (ncells == 1) {
		req_qid = 0;
		req_pid = p->args[0];
	} else {
		req_qid = p->args[0];
		req_pid = p->args[1];
	}

	if (d->is_sproxy) {
		if (req_pid >= d->num_valid_queues)
			goto err;
		qinst = &inst->qinsts[req_pid];
		return qinst->chan;
	}

	for (qinst = inst->qinsts, i = 0; i < inst->num_valid_queues;
	     i++, qinst++) {
		if (req_qid == qinst->queue_id && req_pid == qinst->proxy_id)
			return qinst->chan;
	}

err:
	dev_err(inst->dev, "Queue ID %d, Proxy ID %d is wrong on %pOFn\n",
		req_qid, req_pid, p->np);
	return ERR_PTR(-ENOENT);
}

 
static int ti_msgmgr_queue_setup(int idx, struct device *dev,
				 struct device_node *np,
				 struct ti_msgmgr_inst *inst,
				 const struct ti_msgmgr_desc *d,
				 const struct ti_msgmgr_valid_queue_desc *qd,
				 struct ti_queue_inst *qinst,
				 struct mbox_chan *chan)
{
	char *dir;

	qinst->proxy_id = qd->proxy_id;
	qinst->queue_id = qd->queue_id;

	if (qinst->queue_id > d->queue_count) {
		dev_err(dev, "Queue Data [idx=%d] queuid %d > %d\n",
			idx, qinst->queue_id, d->queue_count);
		return -ERANGE;
	}

	if (d->is_sproxy) {
		qinst->queue_buff_start = inst->queue_proxy_region +
		    SPROXY_THREAD_DATA_OFFSET(qinst->proxy_id,
					      d->data_first_reg);
		qinst->queue_buff_end = inst->queue_proxy_region +
		    SPROXY_THREAD_DATA_OFFSET(qinst->proxy_id,
					      d->data_last_reg);
		qinst->queue_state = inst->queue_state_debug_region +
		    SPROXY_THREAD_STATUS_OFFSET(qinst->proxy_id);
		qinst->queue_ctrl = inst->queue_ctrl_region +
		    SPROXY_THREAD_CTRL_OFFSET(qinst->proxy_id);

		 
		dir = "thr";
		snprintf(qinst->name, sizeof(qinst->name), "%s %s_%03d",
			 dev_name(dev), dir, qinst->proxy_id);
	} else {
		qinst->queue_buff_start = inst->queue_proxy_region +
		    Q_DATA_OFFSET(qinst->proxy_id, qinst->queue_id,
				  d->data_first_reg);
		qinst->queue_buff_end = inst->queue_proxy_region +
		    Q_DATA_OFFSET(qinst->proxy_id, qinst->queue_id,
				  d->data_last_reg);
		qinst->queue_state =
		    inst->queue_state_debug_region +
		    Q_STATE_OFFSET(qinst->queue_id);
		qinst->is_tx = qd->is_tx;
		dir = qinst->is_tx ? "tx" : "rx";
		snprintf(qinst->name, sizeof(qinst->name), "%s %s_%03d_%03d",
			 dev_name(dev), dir, qinst->queue_id, qinst->proxy_id);
	}

	qinst->chan = chan;

	 
	qinst->irq = -EINVAL;

	chan->con_priv = qinst;

	dev_dbg(dev, "[%d] qidx=%d pidx=%d irq=%d q_s=%p q_e = %p\n",
		idx, qinst->queue_id, qinst->proxy_id, qinst->irq,
		qinst->queue_buff_start, qinst->queue_buff_end);
	return 0;
}

static int ti_msgmgr_queue_rx_set_polled_mode(struct ti_queue_inst *qinst, bool enable)
{
	if (enable) {
		disable_irq(qinst->irq);
		qinst->polled_rx_mode = true;
	} else {
		enable_irq(qinst->irq);
		qinst->polled_rx_mode = false;
	}

	return 0;
}

static int ti_msgmgr_suspend(struct device *dev)
{
	struct ti_msgmgr_inst *inst = dev_get_drvdata(dev);
	struct ti_queue_inst *qinst;
	int i;

	 
	for (qinst = inst->qinsts, i = 0; i < inst->num_valid_queues; qinst++, i++) {
		if (!qinst->is_tx)
			ti_msgmgr_queue_rx_set_polled_mode(qinst, true);
	}

	return 0;
}

static int ti_msgmgr_resume(struct device *dev)
{
	struct ti_msgmgr_inst *inst = dev_get_drvdata(dev);
	struct ti_queue_inst *qinst;
	int i;

	for (qinst = inst->qinsts, i = 0; i < inst->num_valid_queues; qinst++, i++) {
		if (!qinst->is_tx)
			ti_msgmgr_queue_rx_set_polled_mode(qinst, false);
	}

	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(ti_msgmgr_pm_ops, ti_msgmgr_suspend, ti_msgmgr_resume);

 
static const struct mbox_chan_ops ti_msgmgr_chan_ops = {
	.startup = ti_msgmgr_queue_startup,
	.shutdown = ti_msgmgr_queue_shutdown,
	.peek_data = ti_msgmgr_queue_peek_data,
	.last_tx_done = ti_msgmgr_last_tx_done,
	.send_data = ti_msgmgr_send_data,
};

 
static const struct ti_msgmgr_valid_queue_desc k2g_valid_queues[] = {
	{.queue_id = 0, .proxy_id = 0, .is_tx = true,},
	{.queue_id = 1, .proxy_id = 0, .is_tx = true,},
	{.queue_id = 2, .proxy_id = 0, .is_tx = true,},
	{.queue_id = 3, .proxy_id = 0, .is_tx = true,},
	{.queue_id = 5, .proxy_id = 2, .is_tx = false,},
	{.queue_id = 56, .proxy_id = 1, .is_tx = true,},
	{.queue_id = 57, .proxy_id = 2, .is_tx = false,},
	{.queue_id = 58, .proxy_id = 3, .is_tx = true,},
	{.queue_id = 59, .proxy_id = 4, .is_tx = true,},
	{.queue_id = 60, .proxy_id = 5, .is_tx = true,},
	{.queue_id = 61, .proxy_id = 6, .is_tx = true,},
};

static const struct ti_msgmgr_desc k2g_desc = {
	.queue_count = 64,
	.max_message_size = 64,
	.max_messages = 128,
	.data_region_name = "queue_proxy_region",
	.status_region_name = "queue_state_debug_region",
	.data_first_reg = 16,
	.data_last_reg = 31,
	.status_cnt_mask = Q_STATE_ENTRY_COUNT_MASK,
	.tx_polled = false,
	.valid_queues = k2g_valid_queues,
	.num_valid_queues = ARRAY_SIZE(k2g_valid_queues),
	.is_sproxy = false,
};

static const struct ti_msgmgr_desc am654_desc = {
	.queue_count = 190,
	.num_valid_queues = 190,
	.max_message_size = 60,
	.data_region_name = "target_data",
	.status_region_name = "rt",
	.ctrl_region_name = "scfg",
	.data_first_reg = 0,
	.data_last_reg = 14,
	.status_cnt_mask = SPROXY_THREAD_STATUS_COUNT_MASK,
	.tx_polled = false,
	.is_sproxy = true,
};

static const struct of_device_id ti_msgmgr_of_match[] = {
	{.compatible = "ti,k2g-message-manager", .data = &k2g_desc},
	{.compatible = "ti,am654-secure-proxy", .data = &am654_desc},
	{   }
};

MODULE_DEVICE_TABLE(of, ti_msgmgr_of_match);

static int ti_msgmgr_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *of_id;
	struct device_node *np;
	const struct ti_msgmgr_desc *desc;
	struct ti_msgmgr_inst *inst;
	struct ti_queue_inst *qinst;
	struct mbox_controller *mbox;
	struct mbox_chan *chans;
	int queue_count;
	int i;
	int ret = -EINVAL;
	const struct ti_msgmgr_valid_queue_desc *queue_desc;

	if (!dev->of_node) {
		dev_err(dev, "no OF information\n");
		return -EINVAL;
	}
	np = dev->of_node;

	of_id = of_match_device(ti_msgmgr_of_match, dev);
	if (!of_id) {
		dev_err(dev, "OF data missing\n");
		return -EINVAL;
	}
	desc = of_id->data;

	inst = devm_kzalloc(dev, sizeof(*inst), GFP_KERNEL);
	if (!inst)
		return -ENOMEM;

	inst->dev = dev;
	inst->desc = desc;

	inst->queue_proxy_region =
		devm_platform_ioremap_resource_byname(pdev, desc->data_region_name);
	if (IS_ERR(inst->queue_proxy_region))
		return PTR_ERR(inst->queue_proxy_region);

	inst->queue_state_debug_region =
		devm_platform_ioremap_resource_byname(pdev, desc->status_region_name);
	if (IS_ERR(inst->queue_state_debug_region))
		return PTR_ERR(inst->queue_state_debug_region);

	if (desc->is_sproxy) {
		inst->queue_ctrl_region =
			devm_platform_ioremap_resource_byname(pdev, desc->ctrl_region_name);
		if (IS_ERR(inst->queue_ctrl_region))
			return PTR_ERR(inst->queue_ctrl_region);
	}

	dev_dbg(dev, "proxy region=%p, queue_state=%p\n",
		inst->queue_proxy_region, inst->queue_state_debug_region);

	queue_count = desc->num_valid_queues;
	if (!queue_count || queue_count > desc->queue_count) {
		dev_crit(dev, "Invalid Number of queues %d. Max %d\n",
			 queue_count, desc->queue_count);
		return -ERANGE;
	}
	inst->num_valid_queues = queue_count;

	qinst = devm_kcalloc(dev, queue_count, sizeof(*qinst), GFP_KERNEL);
	if (!qinst)
		return -ENOMEM;
	inst->qinsts = qinst;

	chans = devm_kcalloc(dev, queue_count, sizeof(*chans), GFP_KERNEL);
	if (!chans)
		return -ENOMEM;
	inst->chans = chans;

	if (desc->is_sproxy) {
		struct ti_msgmgr_valid_queue_desc sproxy_desc;

		 
		for (i = 0; i < queue_count; i++, qinst++, chans++) {
			sproxy_desc.queue_id = 0;
			sproxy_desc.proxy_id = i;
			ret = ti_msgmgr_queue_setup(i, dev, np, inst,
						    desc, &sproxy_desc, qinst,
						    chans);
			if (ret)
				return ret;
		}
	} else {
		 
		for (i = 0, queue_desc = desc->valid_queues;
		     i < queue_count; i++, qinst++, chans++, queue_desc++) {
			ret = ti_msgmgr_queue_setup(i, dev, np, inst,
						    desc, queue_desc, qinst,
						    chans);
			if (ret)
				return ret;
		}
	}

	mbox = &inst->mbox;
	mbox->dev = dev;
	mbox->ops = &ti_msgmgr_chan_ops;
	mbox->chans = inst->chans;
	mbox->num_chans = inst->num_valid_queues;
	mbox->txdone_irq = false;
	mbox->txdone_poll = desc->tx_polled;
	if (desc->tx_polled)
		mbox->txpoll_period = desc->tx_poll_timeout_ms;
	mbox->of_xlate = ti_msgmgr_of_xlate;

	platform_set_drvdata(pdev, inst);
	ret = devm_mbox_controller_register(dev, mbox);
	if (ret)
		dev_err(dev, "Failed to register mbox_controller(%d)\n", ret);

	return ret;
}

static struct platform_driver ti_msgmgr_driver = {
	.probe = ti_msgmgr_probe,
	.driver = {
		   .name = "ti-msgmgr",
		   .of_match_table = of_match_ptr(ti_msgmgr_of_match),
		   .pm = &ti_msgmgr_pm_ops,
	},
};
module_platform_driver(ti_msgmgr_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TI message manager driver");
MODULE_AUTHOR("Nishanth Menon");
MODULE_ALIAS("platform:ti-msgmgr");
