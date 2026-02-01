
 

#include <linux/bitops.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_dma.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/io-64-nonatomic-lo-hi.h>
#include <linux/pm_runtime.h>

#include "../dmaengine.h"

 
#define ZYNQMP_DMA_ISR			0x100
#define ZYNQMP_DMA_IMR			0x104
#define ZYNQMP_DMA_IER			0x108
#define ZYNQMP_DMA_IDS			0x10C
#define ZYNQMP_DMA_CTRL0		0x110
#define ZYNQMP_DMA_CTRL1		0x114
#define ZYNQMP_DMA_DATA_ATTR		0x120
#define ZYNQMP_DMA_DSCR_ATTR		0x124
#define ZYNQMP_DMA_SRC_DSCR_WRD0	0x128
#define ZYNQMP_DMA_SRC_DSCR_WRD1	0x12C
#define ZYNQMP_DMA_SRC_DSCR_WRD2	0x130
#define ZYNQMP_DMA_SRC_DSCR_WRD3	0x134
#define ZYNQMP_DMA_DST_DSCR_WRD0	0x138
#define ZYNQMP_DMA_DST_DSCR_WRD1	0x13C
#define ZYNQMP_DMA_DST_DSCR_WRD2	0x140
#define ZYNQMP_DMA_DST_DSCR_WRD3	0x144
#define ZYNQMP_DMA_SRC_START_LSB	0x158
#define ZYNQMP_DMA_SRC_START_MSB	0x15C
#define ZYNQMP_DMA_DST_START_LSB	0x160
#define ZYNQMP_DMA_DST_START_MSB	0x164
#define ZYNQMP_DMA_TOTAL_BYTE		0x188
#define ZYNQMP_DMA_RATE_CTRL		0x18C
#define ZYNQMP_DMA_IRQ_SRC_ACCT		0x190
#define ZYNQMP_DMA_IRQ_DST_ACCT		0x194
#define ZYNQMP_DMA_CTRL2		0x200

 
#define ZYNQMP_DMA_DONE			BIT(10)
#define ZYNQMP_DMA_AXI_WR_DATA		BIT(9)
#define ZYNQMP_DMA_AXI_RD_DATA		BIT(8)
#define ZYNQMP_DMA_AXI_RD_DST_DSCR	BIT(7)
#define ZYNQMP_DMA_AXI_RD_SRC_DSCR	BIT(6)
#define ZYNQMP_DMA_IRQ_DST_ACCT_ERR	BIT(5)
#define ZYNQMP_DMA_IRQ_SRC_ACCT_ERR	BIT(4)
#define ZYNQMP_DMA_BYTE_CNT_OVRFL	BIT(3)
#define ZYNQMP_DMA_DST_DSCR_DONE	BIT(2)
#define ZYNQMP_DMA_INV_APB		BIT(0)

 
#define ZYNQMP_DMA_OVR_FETCH		BIT(7)
#define ZYNQMP_DMA_POINT_TYPE_SG	BIT(6)
#define ZYNQMP_DMA_RATE_CTRL_EN		BIT(3)

 
#define ZYNQMP_DMA_SRC_ISSUE		GENMASK(4, 0)

 
#define ZYNQMP_DMA_ARBURST		GENMASK(27, 26)
#define ZYNQMP_DMA_ARCACHE		GENMASK(25, 22)
#define ZYNQMP_DMA_ARCACHE_OFST		22
#define ZYNQMP_DMA_ARQOS		GENMASK(21, 18)
#define ZYNQMP_DMA_ARQOS_OFST		18
#define ZYNQMP_DMA_ARLEN		GENMASK(17, 14)
#define ZYNQMP_DMA_ARLEN_OFST		14
#define ZYNQMP_DMA_AWBURST		GENMASK(13, 12)
#define ZYNQMP_DMA_AWCACHE		GENMASK(11, 8)
#define ZYNQMP_DMA_AWCACHE_OFST		8
#define ZYNQMP_DMA_AWQOS		GENMASK(7, 4)
#define ZYNQMP_DMA_AWQOS_OFST		4
#define ZYNQMP_DMA_AWLEN		GENMASK(3, 0)
#define ZYNQMP_DMA_AWLEN_OFST		0

 
#define ZYNQMP_DMA_AXCOHRNT		BIT(8)
#define ZYNQMP_DMA_AXCACHE		GENMASK(7, 4)
#define ZYNQMP_DMA_AXCACHE_OFST		4
#define ZYNQMP_DMA_AXQOS		GENMASK(3, 0)
#define ZYNQMP_DMA_AXQOS_OFST		0

 
#define ZYNQMP_DMA_ENABLE		BIT(0)

 
#define ZYNQMP_DMA_DESC_CTRL_STOP	0x10
#define ZYNQMP_DMA_DESC_CTRL_COMP_INT	0x4
#define ZYNQMP_DMA_DESC_CTRL_SIZE_256	0x2
#define ZYNQMP_DMA_DESC_CTRL_COHRNT	0x1

 
#define ZYNQMP_DMA_INT_ERR	(ZYNQMP_DMA_AXI_RD_DATA | \
				ZYNQMP_DMA_AXI_WR_DATA | \
				ZYNQMP_DMA_AXI_RD_DST_DSCR | \
				ZYNQMP_DMA_AXI_RD_SRC_DSCR | \
				ZYNQMP_DMA_INV_APB)
#define ZYNQMP_DMA_INT_OVRFL	(ZYNQMP_DMA_BYTE_CNT_OVRFL | \
				ZYNQMP_DMA_IRQ_SRC_ACCT_ERR | \
				ZYNQMP_DMA_IRQ_DST_ACCT_ERR)
#define ZYNQMP_DMA_INT_DONE	(ZYNQMP_DMA_DONE | ZYNQMP_DMA_DST_DSCR_DONE)
#define ZYNQMP_DMA_INT_EN_DEFAULT_MASK	(ZYNQMP_DMA_INT_DONE | \
					ZYNQMP_DMA_INT_ERR | \
					ZYNQMP_DMA_INT_OVRFL | \
					ZYNQMP_DMA_DST_DSCR_DONE)

 
#define ZYNQMP_DMA_NUM_DESCS	32

 
#define ZYNQMP_DMA_MAX_TRANS_LEN	0x40000000

 
#define ZYNQMP_DMA_MAX_DST_BURST_LEN    32768U
#define ZYNQMP_DMA_MAX_SRC_BURST_LEN    32768U

 
#define ZYNQMP_DMA_AXCACHE_VAL		0xF

#define ZYNQMP_DMA_SRC_ISSUE_RST_VAL	0x1F

#define ZYNQMP_DMA_IDS_DEFAULT_MASK	0xFFF

 
#define ZYNQMP_DMA_BUS_WIDTH_64		64
#define ZYNQMP_DMA_BUS_WIDTH_128	128

#define ZDMA_PM_TIMEOUT			100

#define ZYNQMP_DMA_DESC_SIZE(chan)	(chan->desc_size)

#define to_chan(chan)		container_of(chan, struct zynqmp_dma_chan, \
					     common)
#define tx_to_desc(tx)		container_of(tx, struct zynqmp_dma_desc_sw, \
					     async_tx)

 
struct zynqmp_dma_desc_ll {
	u64 addr;
	u32 size;
	u32 ctrl;
	u64 nxtdscraddr;
	u64 rsvd;
};

 
struct zynqmp_dma_desc_sw {
	u64 src;
	u64 dst;
	u32 len;
	struct list_head node;
	struct list_head tx_list;
	struct dma_async_tx_descriptor async_tx;
	struct zynqmp_dma_desc_ll *src_v;
	dma_addr_t src_p;
	struct zynqmp_dma_desc_ll *dst_v;
	dma_addr_t dst_p;
};

 
struct zynqmp_dma_chan {
	struct zynqmp_dma_device *zdev;
	void __iomem *regs;
	spinlock_t lock;
	struct list_head pending_list;
	struct list_head free_list;
	struct list_head active_list;
	struct zynqmp_dma_desc_sw *sw_desc_pool;
	struct list_head done_list;
	struct dma_chan common;
	void *desc_pool_v;
	dma_addr_t desc_pool_p;
	u32 desc_free_cnt;
	struct device *dev;
	int irq;
	bool is_dmacoherent;
	struct tasklet_struct tasklet;
	bool idle;
	size_t desc_size;
	bool err;
	u32 bus_width;
	u32 src_burst_len;
	u32 dst_burst_len;
};

 
struct zynqmp_dma_device {
	struct device *dev;
	struct dma_device common;
	struct zynqmp_dma_chan *chan;
	struct clk *clk_main;
	struct clk *clk_apb;
};

static inline void zynqmp_dma_writeq(struct zynqmp_dma_chan *chan, u32 reg,
				     u64 value)
{
	lo_hi_writeq(value, chan->regs + reg);
}

 
static void zynqmp_dma_update_desc_to_ctrlr(struct zynqmp_dma_chan *chan,
				      struct zynqmp_dma_desc_sw *desc)
{
	dma_addr_t addr;

	addr = desc->src_p;
	zynqmp_dma_writeq(chan, ZYNQMP_DMA_SRC_START_LSB, addr);
	addr = desc->dst_p;
	zynqmp_dma_writeq(chan, ZYNQMP_DMA_DST_START_LSB, addr);
}

 
static void zynqmp_dma_desc_config_eod(struct zynqmp_dma_chan *chan,
				       void *desc)
{
	struct zynqmp_dma_desc_ll *hw = (struct zynqmp_dma_desc_ll *)desc;

	hw->ctrl |= ZYNQMP_DMA_DESC_CTRL_STOP;
	hw++;
	hw->ctrl |= ZYNQMP_DMA_DESC_CTRL_COMP_INT | ZYNQMP_DMA_DESC_CTRL_STOP;
}

 
static void zynqmp_dma_config_sg_ll_desc(struct zynqmp_dma_chan *chan,
				   struct zynqmp_dma_desc_ll *sdesc,
				   dma_addr_t src, dma_addr_t dst, size_t len,
				   struct zynqmp_dma_desc_ll *prev)
{
	struct zynqmp_dma_desc_ll *ddesc = sdesc + 1;

	sdesc->size = ddesc->size = len;
	sdesc->addr = src;
	ddesc->addr = dst;

	sdesc->ctrl = ddesc->ctrl = ZYNQMP_DMA_DESC_CTRL_SIZE_256;
	if (chan->is_dmacoherent) {
		sdesc->ctrl |= ZYNQMP_DMA_DESC_CTRL_COHRNT;
		ddesc->ctrl |= ZYNQMP_DMA_DESC_CTRL_COHRNT;
	}

	if (prev) {
		dma_addr_t addr = chan->desc_pool_p +
			    ((uintptr_t)sdesc - (uintptr_t)chan->desc_pool_v);
		ddesc = prev + 1;
		prev->nxtdscraddr = addr;
		ddesc->nxtdscraddr = addr + ZYNQMP_DMA_DESC_SIZE(chan);
	}
}

 
static void zynqmp_dma_init(struct zynqmp_dma_chan *chan)
{
	u32 val;

	writel(ZYNQMP_DMA_IDS_DEFAULT_MASK, chan->regs + ZYNQMP_DMA_IDS);
	val = readl(chan->regs + ZYNQMP_DMA_ISR);
	writel(val, chan->regs + ZYNQMP_DMA_ISR);

	if (chan->is_dmacoherent) {
		val = ZYNQMP_DMA_AXCOHRNT;
		val = (val & ~ZYNQMP_DMA_AXCACHE) |
			(ZYNQMP_DMA_AXCACHE_VAL << ZYNQMP_DMA_AXCACHE_OFST);
		writel(val, chan->regs + ZYNQMP_DMA_DSCR_ATTR);
	}

	val = readl(chan->regs + ZYNQMP_DMA_DATA_ATTR);
	if (chan->is_dmacoherent) {
		val = (val & ~ZYNQMP_DMA_ARCACHE) |
			(ZYNQMP_DMA_AXCACHE_VAL << ZYNQMP_DMA_ARCACHE_OFST);
		val = (val & ~ZYNQMP_DMA_AWCACHE) |
			(ZYNQMP_DMA_AXCACHE_VAL << ZYNQMP_DMA_AWCACHE_OFST);
	}
	writel(val, chan->regs + ZYNQMP_DMA_DATA_ATTR);

	 
	val = readl(chan->regs + ZYNQMP_DMA_IRQ_SRC_ACCT);
	val = readl(chan->regs + ZYNQMP_DMA_IRQ_DST_ACCT);

	chan->idle = true;
}

 
static dma_cookie_t zynqmp_dma_tx_submit(struct dma_async_tx_descriptor *tx)
{
	struct zynqmp_dma_chan *chan = to_chan(tx->chan);
	struct zynqmp_dma_desc_sw *desc, *new;
	dma_cookie_t cookie;
	unsigned long irqflags;

	new = tx_to_desc(tx);
	spin_lock_irqsave(&chan->lock, irqflags);
	cookie = dma_cookie_assign(tx);

	if (!list_empty(&chan->pending_list)) {
		desc = list_last_entry(&chan->pending_list,
				     struct zynqmp_dma_desc_sw, node);
		if (!list_empty(&desc->tx_list))
			desc = list_last_entry(&desc->tx_list,
					       struct zynqmp_dma_desc_sw, node);
		desc->src_v->nxtdscraddr = new->src_p;
		desc->src_v->ctrl &= ~ZYNQMP_DMA_DESC_CTRL_STOP;
		desc->dst_v->nxtdscraddr = new->dst_p;
		desc->dst_v->ctrl &= ~ZYNQMP_DMA_DESC_CTRL_STOP;
	}

	list_add_tail(&new->node, &chan->pending_list);
	spin_unlock_irqrestore(&chan->lock, irqflags);

	return cookie;
}

 
static struct zynqmp_dma_desc_sw *
zynqmp_dma_get_descriptor(struct zynqmp_dma_chan *chan)
{
	struct zynqmp_dma_desc_sw *desc;
	unsigned long irqflags;

	spin_lock_irqsave(&chan->lock, irqflags);
	desc = list_first_entry(&chan->free_list,
				struct zynqmp_dma_desc_sw, node);
	list_del(&desc->node);
	spin_unlock_irqrestore(&chan->lock, irqflags);

	INIT_LIST_HEAD(&desc->tx_list);
	 
	memset((void *)desc->src_v, 0, ZYNQMP_DMA_DESC_SIZE(chan));
	memset((void *)desc->dst_v, 0, ZYNQMP_DMA_DESC_SIZE(chan));

	return desc;
}

 
static void zynqmp_dma_free_descriptor(struct zynqmp_dma_chan *chan,
				 struct zynqmp_dma_desc_sw *sdesc)
{
	struct zynqmp_dma_desc_sw *child, *next;

	chan->desc_free_cnt++;
	list_move_tail(&sdesc->node, &chan->free_list);
	list_for_each_entry_safe(child, next, &sdesc->tx_list, node) {
		chan->desc_free_cnt++;
		list_move_tail(&child->node, &chan->free_list);
	}
}

 
static void zynqmp_dma_free_desc_list(struct zynqmp_dma_chan *chan,
				      struct list_head *list)
{
	struct zynqmp_dma_desc_sw *desc, *next;

	list_for_each_entry_safe(desc, next, list, node)
		zynqmp_dma_free_descriptor(chan, desc);
}

 
static int zynqmp_dma_alloc_chan_resources(struct dma_chan *dchan)
{
	struct zynqmp_dma_chan *chan = to_chan(dchan);
	struct zynqmp_dma_desc_sw *desc;
	int i, ret;

	ret = pm_runtime_resume_and_get(chan->dev);
	if (ret < 0)
		return ret;

	chan->sw_desc_pool = kcalloc(ZYNQMP_DMA_NUM_DESCS, sizeof(*desc),
				     GFP_KERNEL);
	if (!chan->sw_desc_pool)
		return -ENOMEM;

	chan->idle = true;
	chan->desc_free_cnt = ZYNQMP_DMA_NUM_DESCS;

	INIT_LIST_HEAD(&chan->free_list);

	for (i = 0; i < ZYNQMP_DMA_NUM_DESCS; i++) {
		desc = chan->sw_desc_pool + i;
		dma_async_tx_descriptor_init(&desc->async_tx, &chan->common);
		desc->async_tx.tx_submit = zynqmp_dma_tx_submit;
		list_add_tail(&desc->node, &chan->free_list);
	}

	chan->desc_pool_v = dma_alloc_coherent(chan->dev,
					       (2 * ZYNQMP_DMA_DESC_SIZE(chan) *
					       ZYNQMP_DMA_NUM_DESCS),
					       &chan->desc_pool_p, GFP_KERNEL);
	if (!chan->desc_pool_v)
		return -ENOMEM;

	for (i = 0; i < ZYNQMP_DMA_NUM_DESCS; i++) {
		desc = chan->sw_desc_pool + i;
		desc->src_v = (struct zynqmp_dma_desc_ll *) (chan->desc_pool_v +
					(i * ZYNQMP_DMA_DESC_SIZE(chan) * 2));
		desc->dst_v = (struct zynqmp_dma_desc_ll *) (desc->src_v + 1);
		desc->src_p = chan->desc_pool_p +
				(i * ZYNQMP_DMA_DESC_SIZE(chan) * 2);
		desc->dst_p = desc->src_p + ZYNQMP_DMA_DESC_SIZE(chan);
	}

	return ZYNQMP_DMA_NUM_DESCS;
}

 
static void zynqmp_dma_start(struct zynqmp_dma_chan *chan)
{
	writel(ZYNQMP_DMA_INT_EN_DEFAULT_MASK, chan->regs + ZYNQMP_DMA_IER);
	writel(0, chan->regs + ZYNQMP_DMA_TOTAL_BYTE);
	chan->idle = false;
	writel(ZYNQMP_DMA_ENABLE, chan->regs + ZYNQMP_DMA_CTRL2);
}

 
static void zynqmp_dma_handle_ovfl_int(struct zynqmp_dma_chan *chan, u32 status)
{
	if (status & ZYNQMP_DMA_BYTE_CNT_OVRFL)
		writel(0, chan->regs + ZYNQMP_DMA_TOTAL_BYTE);
	if (status & ZYNQMP_DMA_IRQ_DST_ACCT_ERR)
		readl(chan->regs + ZYNQMP_DMA_IRQ_DST_ACCT);
	if (status & ZYNQMP_DMA_IRQ_SRC_ACCT_ERR)
		readl(chan->regs + ZYNQMP_DMA_IRQ_SRC_ACCT);
}

static void zynqmp_dma_config(struct zynqmp_dma_chan *chan)
{
	u32 val, burst_val;

	val = readl(chan->regs + ZYNQMP_DMA_CTRL0);
	val |= ZYNQMP_DMA_POINT_TYPE_SG;
	writel(val, chan->regs + ZYNQMP_DMA_CTRL0);

	val = readl(chan->regs + ZYNQMP_DMA_DATA_ATTR);
	burst_val = __ilog2_u32(chan->src_burst_len);
	val = (val & ~ZYNQMP_DMA_ARLEN) |
		((burst_val << ZYNQMP_DMA_ARLEN_OFST) & ZYNQMP_DMA_ARLEN);
	burst_val = __ilog2_u32(chan->dst_burst_len);
	val = (val & ~ZYNQMP_DMA_AWLEN) |
		((burst_val << ZYNQMP_DMA_AWLEN_OFST) & ZYNQMP_DMA_AWLEN);
	writel(val, chan->regs + ZYNQMP_DMA_DATA_ATTR);
}

 
static int zynqmp_dma_device_config(struct dma_chan *dchan,
				    struct dma_slave_config *config)
{
	struct zynqmp_dma_chan *chan = to_chan(dchan);

	chan->src_burst_len = clamp(config->src_maxburst, 1U,
		ZYNQMP_DMA_MAX_SRC_BURST_LEN);
	chan->dst_burst_len = clamp(config->dst_maxburst, 1U,
		ZYNQMP_DMA_MAX_DST_BURST_LEN);

	return 0;
}

 
static void zynqmp_dma_start_transfer(struct zynqmp_dma_chan *chan)
{
	struct zynqmp_dma_desc_sw *desc;

	if (!chan->idle)
		return;

	zynqmp_dma_config(chan);

	desc = list_first_entry_or_null(&chan->pending_list,
					struct zynqmp_dma_desc_sw, node);
	if (!desc)
		return;

	list_splice_tail_init(&chan->pending_list, &chan->active_list);
	zynqmp_dma_update_desc_to_ctrlr(chan, desc);
	zynqmp_dma_start(chan);
}


 
static void zynqmp_dma_chan_desc_cleanup(struct zynqmp_dma_chan *chan)
{
	struct zynqmp_dma_desc_sw *desc, *next;
	unsigned long irqflags;

	spin_lock_irqsave(&chan->lock, irqflags);

	list_for_each_entry_safe(desc, next, &chan->done_list, node) {
		struct dmaengine_desc_callback cb;

		dmaengine_desc_get_callback(&desc->async_tx, &cb);
		if (dmaengine_desc_callback_valid(&cb)) {
			spin_unlock_irqrestore(&chan->lock, irqflags);
			dmaengine_desc_callback_invoke(&cb, NULL);
			spin_lock_irqsave(&chan->lock, irqflags);
		}

		 
		zynqmp_dma_free_descriptor(chan, desc);
	}

	spin_unlock_irqrestore(&chan->lock, irqflags);
}

 
static void zynqmp_dma_complete_descriptor(struct zynqmp_dma_chan *chan)
{
	struct zynqmp_dma_desc_sw *desc;

	desc = list_first_entry_or_null(&chan->active_list,
					struct zynqmp_dma_desc_sw, node);
	if (!desc)
		return;
	list_del(&desc->node);
	dma_cookie_complete(&desc->async_tx);
	list_add_tail(&desc->node, &chan->done_list);
}

 
static void zynqmp_dma_issue_pending(struct dma_chan *dchan)
{
	struct zynqmp_dma_chan *chan = to_chan(dchan);
	unsigned long irqflags;

	spin_lock_irqsave(&chan->lock, irqflags);
	zynqmp_dma_start_transfer(chan);
	spin_unlock_irqrestore(&chan->lock, irqflags);
}

 
static void zynqmp_dma_free_descriptors(struct zynqmp_dma_chan *chan)
{
	unsigned long irqflags;

	spin_lock_irqsave(&chan->lock, irqflags);
	zynqmp_dma_free_desc_list(chan, &chan->active_list);
	zynqmp_dma_free_desc_list(chan, &chan->pending_list);
	zynqmp_dma_free_desc_list(chan, &chan->done_list);
	spin_unlock_irqrestore(&chan->lock, irqflags);
}

 
static void zynqmp_dma_free_chan_resources(struct dma_chan *dchan)
{
	struct zynqmp_dma_chan *chan = to_chan(dchan);

	zynqmp_dma_free_descriptors(chan);
	dma_free_coherent(chan->dev,
		(2 * ZYNQMP_DMA_DESC_SIZE(chan) * ZYNQMP_DMA_NUM_DESCS),
		chan->desc_pool_v, chan->desc_pool_p);
	kfree(chan->sw_desc_pool);
	pm_runtime_mark_last_busy(chan->dev);
	pm_runtime_put_autosuspend(chan->dev);
}

 
static void zynqmp_dma_reset(struct zynqmp_dma_chan *chan)
{
	unsigned long irqflags;

	writel(ZYNQMP_DMA_IDS_DEFAULT_MASK, chan->regs + ZYNQMP_DMA_IDS);

	spin_lock_irqsave(&chan->lock, irqflags);
	zynqmp_dma_complete_descriptor(chan);
	spin_unlock_irqrestore(&chan->lock, irqflags);
	zynqmp_dma_chan_desc_cleanup(chan);
	zynqmp_dma_free_descriptors(chan);

	zynqmp_dma_init(chan);
}

 
static irqreturn_t zynqmp_dma_irq_handler(int irq, void *data)
{
	struct zynqmp_dma_chan *chan = (struct zynqmp_dma_chan *)data;
	u32 isr, imr, status;
	irqreturn_t ret = IRQ_NONE;

	isr = readl(chan->regs + ZYNQMP_DMA_ISR);
	imr = readl(chan->regs + ZYNQMP_DMA_IMR);
	status = isr & ~imr;

	writel(isr, chan->regs + ZYNQMP_DMA_ISR);
	if (status & ZYNQMP_DMA_INT_DONE) {
		tasklet_schedule(&chan->tasklet);
		ret = IRQ_HANDLED;
	}

	if (status & ZYNQMP_DMA_DONE)
		chan->idle = true;

	if (status & ZYNQMP_DMA_INT_ERR) {
		chan->err = true;
		tasklet_schedule(&chan->tasklet);
		dev_err(chan->dev, "Channel %p has errors\n", chan);
		ret = IRQ_HANDLED;
	}

	if (status & ZYNQMP_DMA_INT_OVRFL) {
		zynqmp_dma_handle_ovfl_int(chan, status);
		dev_dbg(chan->dev, "Channel %p overflow interrupt\n", chan);
		ret = IRQ_HANDLED;
	}

	return ret;
}

 
static void zynqmp_dma_do_tasklet(struct tasklet_struct *t)
{
	struct zynqmp_dma_chan *chan = from_tasklet(chan, t, tasklet);
	u32 count;
	unsigned long irqflags;

	if (chan->err) {
		zynqmp_dma_reset(chan);
		chan->err = false;
		return;
	}

	spin_lock_irqsave(&chan->lock, irqflags);
	count = readl(chan->regs + ZYNQMP_DMA_IRQ_DST_ACCT);
	while (count) {
		zynqmp_dma_complete_descriptor(chan);
		count--;
	}
	spin_unlock_irqrestore(&chan->lock, irqflags);

	zynqmp_dma_chan_desc_cleanup(chan);

	if (chan->idle) {
		spin_lock_irqsave(&chan->lock, irqflags);
		zynqmp_dma_start_transfer(chan);
		spin_unlock_irqrestore(&chan->lock, irqflags);
	}
}

 
static int zynqmp_dma_device_terminate_all(struct dma_chan *dchan)
{
	struct zynqmp_dma_chan *chan = to_chan(dchan);

	writel(ZYNQMP_DMA_IDS_DEFAULT_MASK, chan->regs + ZYNQMP_DMA_IDS);
	zynqmp_dma_free_descriptors(chan);

	return 0;
}

 
static void zynqmp_dma_synchronize(struct dma_chan *dchan)
{
	struct zynqmp_dma_chan *chan = to_chan(dchan);

	tasklet_kill(&chan->tasklet);
}

 
static struct dma_async_tx_descriptor *zynqmp_dma_prep_memcpy(
				struct dma_chan *dchan, dma_addr_t dma_dst,
				dma_addr_t dma_src, size_t len, ulong flags)
{
	struct zynqmp_dma_chan *chan;
	struct zynqmp_dma_desc_sw *new, *first = NULL;
	void *desc = NULL, *prev = NULL;
	size_t copy;
	u32 desc_cnt;
	unsigned long irqflags;

	chan = to_chan(dchan);

	desc_cnt = DIV_ROUND_UP(len, ZYNQMP_DMA_MAX_TRANS_LEN);

	spin_lock_irqsave(&chan->lock, irqflags);
	if (desc_cnt > chan->desc_free_cnt) {
		spin_unlock_irqrestore(&chan->lock, irqflags);
		dev_dbg(chan->dev, "chan %p descs are not available\n", chan);
		return NULL;
	}
	chan->desc_free_cnt = chan->desc_free_cnt - desc_cnt;
	spin_unlock_irqrestore(&chan->lock, irqflags);

	do {
		 
		new = zynqmp_dma_get_descriptor(chan);

		copy = min_t(size_t, len, ZYNQMP_DMA_MAX_TRANS_LEN);
		desc = (struct zynqmp_dma_desc_ll *)new->src_v;
		zynqmp_dma_config_sg_ll_desc(chan, desc, dma_src,
					     dma_dst, copy, prev);
		prev = desc;
		len -= copy;
		dma_src += copy;
		dma_dst += copy;
		if (!first)
			first = new;
		else
			list_add_tail(&new->node, &first->tx_list);
	} while (len);

	zynqmp_dma_desc_config_eod(chan, desc);
	async_tx_ack(&first->async_tx);
	first->async_tx.flags = (enum dma_ctrl_flags)flags;
	return &first->async_tx;
}

 
static void zynqmp_dma_chan_remove(struct zynqmp_dma_chan *chan)
{
	if (!chan)
		return;

	if (chan->irq)
		devm_free_irq(chan->zdev->dev, chan->irq, chan);
	tasklet_kill(&chan->tasklet);
	list_del(&chan->common.device_node);
}

 
static int zynqmp_dma_chan_probe(struct zynqmp_dma_device *zdev,
			   struct platform_device *pdev)
{
	struct zynqmp_dma_chan *chan;
	struct device_node *node = pdev->dev.of_node;
	int err;

	chan = devm_kzalloc(zdev->dev, sizeof(*chan), GFP_KERNEL);
	if (!chan)
		return -ENOMEM;
	chan->dev = zdev->dev;
	chan->zdev = zdev;

	chan->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(chan->regs))
		return PTR_ERR(chan->regs);

	chan->bus_width = ZYNQMP_DMA_BUS_WIDTH_64;
	chan->dst_burst_len = ZYNQMP_DMA_MAX_DST_BURST_LEN;
	chan->src_burst_len = ZYNQMP_DMA_MAX_SRC_BURST_LEN;
	err = of_property_read_u32(node, "xlnx,bus-width", &chan->bus_width);
	if (err < 0) {
		dev_err(&pdev->dev, "missing xlnx,bus-width property\n");
		return err;
	}

	if (chan->bus_width != ZYNQMP_DMA_BUS_WIDTH_64 &&
	    chan->bus_width != ZYNQMP_DMA_BUS_WIDTH_128) {
		dev_err(zdev->dev, "invalid bus-width value");
		return -EINVAL;
	}

	chan->is_dmacoherent =  of_property_read_bool(node, "dma-coherent");
	zdev->chan = chan;
	tasklet_setup(&chan->tasklet, zynqmp_dma_do_tasklet);
	spin_lock_init(&chan->lock);
	INIT_LIST_HEAD(&chan->active_list);
	INIT_LIST_HEAD(&chan->pending_list);
	INIT_LIST_HEAD(&chan->done_list);
	INIT_LIST_HEAD(&chan->free_list);

	dma_cookie_init(&chan->common);
	chan->common.device = &zdev->common;
	list_add_tail(&chan->common.device_node, &zdev->common.channels);

	zynqmp_dma_init(chan);
	chan->irq = platform_get_irq(pdev, 0);
	if (chan->irq < 0)
		return -ENXIO;
	err = devm_request_irq(&pdev->dev, chan->irq, zynqmp_dma_irq_handler, 0,
			       "zynqmp-dma", chan);
	if (err)
		return err;

	chan->desc_size = sizeof(struct zynqmp_dma_desc_ll);
	chan->idle = true;
	return 0;
}

 
static struct dma_chan *of_zynqmp_dma_xlate(struct of_phandle_args *dma_spec,
					    struct of_dma *ofdma)
{
	struct zynqmp_dma_device *zdev = ofdma->of_dma_data;

	return dma_get_slave_channel(&zdev->chan->common);
}

 
static int __maybe_unused zynqmp_dma_suspend(struct device *dev)
{
	if (!device_may_wakeup(dev))
		return pm_runtime_force_suspend(dev);

	return 0;
}

 
static int __maybe_unused zynqmp_dma_resume(struct device *dev)
{
	if (!device_may_wakeup(dev))
		return pm_runtime_force_resume(dev);

	return 0;
}

 
static int __maybe_unused zynqmp_dma_runtime_suspend(struct device *dev)
{
	struct zynqmp_dma_device *zdev = dev_get_drvdata(dev);

	clk_disable_unprepare(zdev->clk_main);
	clk_disable_unprepare(zdev->clk_apb);

	return 0;
}

 
static int __maybe_unused zynqmp_dma_runtime_resume(struct device *dev)
{
	struct zynqmp_dma_device *zdev = dev_get_drvdata(dev);
	int err;

	err = clk_prepare_enable(zdev->clk_main);
	if (err) {
		dev_err(dev, "Unable to enable main clock.\n");
		return err;
	}

	err = clk_prepare_enable(zdev->clk_apb);
	if (err) {
		dev_err(dev, "Unable to enable apb clock.\n");
		clk_disable_unprepare(zdev->clk_main);
		return err;
	}

	return 0;
}

static const struct dev_pm_ops zynqmp_dma_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(zynqmp_dma_suspend, zynqmp_dma_resume)
	SET_RUNTIME_PM_OPS(zynqmp_dma_runtime_suspend,
			   zynqmp_dma_runtime_resume, NULL)
};

 
static int zynqmp_dma_probe(struct platform_device *pdev)
{
	struct zynqmp_dma_device *zdev;
	struct dma_device *p;
	int ret;

	zdev = devm_kzalloc(&pdev->dev, sizeof(*zdev), GFP_KERNEL);
	if (!zdev)
		return -ENOMEM;

	zdev->dev = &pdev->dev;
	INIT_LIST_HEAD(&zdev->common.channels);

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(44));
	if (ret) {
		dev_err(&pdev->dev, "DMA not available for address range\n");
		return ret;
	}
	dma_cap_set(DMA_MEMCPY, zdev->common.cap_mask);

	p = &zdev->common;
	p->device_prep_dma_memcpy = zynqmp_dma_prep_memcpy;
	p->device_terminate_all = zynqmp_dma_device_terminate_all;
	p->device_synchronize = zynqmp_dma_synchronize;
	p->device_issue_pending = zynqmp_dma_issue_pending;
	p->device_alloc_chan_resources = zynqmp_dma_alloc_chan_resources;
	p->device_free_chan_resources = zynqmp_dma_free_chan_resources;
	p->device_tx_status = dma_cookie_status;
	p->device_config = zynqmp_dma_device_config;
	p->dev = &pdev->dev;

	zdev->clk_main = devm_clk_get(&pdev->dev, "clk_main");
	if (IS_ERR(zdev->clk_main))
		return dev_err_probe(&pdev->dev, PTR_ERR(zdev->clk_main),
				     "main clock not found.\n");

	zdev->clk_apb = devm_clk_get(&pdev->dev, "clk_apb");
	if (IS_ERR(zdev->clk_apb))
		return dev_err_probe(&pdev->dev, PTR_ERR(zdev->clk_apb),
				     "apb clock not found.\n");

	platform_set_drvdata(pdev, zdev);
	pm_runtime_set_autosuspend_delay(zdev->dev, ZDMA_PM_TIMEOUT);
	pm_runtime_use_autosuspend(zdev->dev);
	pm_runtime_enable(zdev->dev);
	ret = pm_runtime_resume_and_get(zdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "device wakeup failed.\n");
		pm_runtime_disable(zdev->dev);
	}
	if (!pm_runtime_enabled(zdev->dev)) {
		ret = zynqmp_dma_runtime_resume(zdev->dev);
		if (ret)
			return ret;
	}

	ret = zynqmp_dma_chan_probe(zdev, pdev);
	if (ret) {
		dev_err_probe(&pdev->dev, ret, "Probing channel failed\n");
		goto err_disable_pm;
	}

	p->dst_addr_widths = BIT(zdev->chan->bus_width / 8);
	p->src_addr_widths = BIT(zdev->chan->bus_width / 8);

	ret = dma_async_device_register(&zdev->common);
	if (ret) {
		dev_err(zdev->dev, "failed to register the dma device\n");
		goto free_chan_resources;
	}

	ret = of_dma_controller_register(pdev->dev.of_node,
					 of_zynqmp_dma_xlate, zdev);
	if (ret) {
		dev_err_probe(&pdev->dev, ret, "Unable to register DMA to DT\n");
		dma_async_device_unregister(&zdev->common);
		goto free_chan_resources;
	}

	pm_runtime_mark_last_busy(zdev->dev);
	pm_runtime_put_sync_autosuspend(zdev->dev);

	return 0;

free_chan_resources:
	zynqmp_dma_chan_remove(zdev->chan);
err_disable_pm:
	if (!pm_runtime_enabled(zdev->dev))
		zynqmp_dma_runtime_suspend(zdev->dev);
	pm_runtime_disable(zdev->dev);
	return ret;
}

 
static int zynqmp_dma_remove(struct platform_device *pdev)
{
	struct zynqmp_dma_device *zdev = platform_get_drvdata(pdev);

	of_dma_controller_free(pdev->dev.of_node);
	dma_async_device_unregister(&zdev->common);

	zynqmp_dma_chan_remove(zdev->chan);
	pm_runtime_disable(zdev->dev);
	if (!pm_runtime_enabled(zdev->dev))
		zynqmp_dma_runtime_suspend(zdev->dev);

	return 0;
}

static const struct of_device_id zynqmp_dma_of_match[] = {
	{ .compatible = "xlnx,zynqmp-dma-1.0", },
	{}
};
MODULE_DEVICE_TABLE(of, zynqmp_dma_of_match);

static struct platform_driver zynqmp_dma_driver = {
	.driver = {
		.name = "xilinx-zynqmp-dma",
		.of_match_table = zynqmp_dma_of_match,
		.pm = &zynqmp_dma_dev_pm_ops,
	},
	.probe = zynqmp_dma_probe,
	.remove = zynqmp_dma_remove,
};

module_platform_driver(zynqmp_dma_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xilinx, Inc.");
MODULE_DESCRIPTION("Xilinx ZynqMP DMA driver");
