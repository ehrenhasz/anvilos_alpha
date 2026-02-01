
 

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include "dmaengine.h"

 
#define MV_XOR_V2_DMA_DESQ_BALR_OFF			0x000
#define MV_XOR_V2_DMA_DESQ_BAHR_OFF			0x004
#define MV_XOR_V2_DMA_DESQ_SIZE_OFF			0x008
#define MV_XOR_V2_DMA_DESQ_DONE_OFF			0x00C
#define   MV_XOR_V2_DMA_DESQ_DONE_PENDING_MASK		0x7FFF
#define   MV_XOR_V2_DMA_DESQ_DONE_PENDING_SHIFT		0
#define   MV_XOR_V2_DMA_DESQ_DONE_READ_PTR_MASK		0x1FFF
#define   MV_XOR_V2_DMA_DESQ_DONE_READ_PTR_SHIFT	16
#define MV_XOR_V2_DMA_DESQ_ARATTR_OFF			0x010
#define   MV_XOR_V2_DMA_DESQ_ATTR_CACHE_MASK		0x3F3F
#define   MV_XOR_V2_DMA_DESQ_ATTR_OUTER_SHAREABLE	0x202
#define   MV_XOR_V2_DMA_DESQ_ATTR_CACHEABLE		0x3C3C
#define MV_XOR_V2_DMA_IMSG_CDAT_OFF			0x014
#define MV_XOR_V2_DMA_IMSG_THRD_OFF			0x018
#define   MV_XOR_V2_DMA_IMSG_THRD_MASK			0x7FFF
#define   MV_XOR_V2_DMA_IMSG_TIMER_EN			BIT(18)
#define MV_XOR_V2_DMA_DESQ_AWATTR_OFF			0x01C
   
#define MV_XOR_V2_DMA_DESQ_ALLOC_OFF			0x04C
#define   MV_XOR_V2_DMA_DESQ_ALLOC_WRPTR_MASK		0xFFFF
#define   MV_XOR_V2_DMA_DESQ_ALLOC_WRPTR_SHIFT		16
#define MV_XOR_V2_DMA_IMSG_BALR_OFF			0x050
#define MV_XOR_V2_DMA_IMSG_BAHR_OFF			0x054
#define MV_XOR_V2_DMA_DESQ_CTRL_OFF			0x100
#define	  MV_XOR_V2_DMA_DESQ_CTRL_32B			1
#define   MV_XOR_V2_DMA_DESQ_CTRL_128B			7
#define MV_XOR_V2_DMA_DESQ_STOP_OFF			0x800
#define MV_XOR_V2_DMA_DESQ_DEALLOC_OFF			0x804
#define MV_XOR_V2_DMA_DESQ_ADD_OFF			0x808
#define MV_XOR_V2_DMA_IMSG_TMOT				0x810
#define   MV_XOR_V2_DMA_IMSG_TIMER_THRD_MASK		0x1FFF

 
#define MV_XOR_V2_GLOB_BW_CTRL				0x4
#define   MV_XOR_V2_GLOB_BW_CTRL_NUM_OSTD_RD_SHIFT	0
#define   MV_XOR_V2_GLOB_BW_CTRL_NUM_OSTD_RD_VAL	64
#define   MV_XOR_V2_GLOB_BW_CTRL_NUM_OSTD_WR_SHIFT	8
#define   MV_XOR_V2_GLOB_BW_CTRL_NUM_OSTD_WR_VAL	8
#define   MV_XOR_V2_GLOB_BW_CTRL_RD_BURST_LEN_SHIFT	12
#define   MV_XOR_V2_GLOB_BW_CTRL_RD_BURST_LEN_VAL	4
#define   MV_XOR_V2_GLOB_BW_CTRL_WR_BURST_LEN_SHIFT	16
#define	  MV_XOR_V2_GLOB_BW_CTRL_WR_BURST_LEN_VAL	4
#define MV_XOR_V2_GLOB_PAUSE				0x014
#define   MV_XOR_V2_GLOB_PAUSE_AXI_TIME_DIS_VAL		0x8
#define MV_XOR_V2_GLOB_SYS_INT_CAUSE			0x200
#define MV_XOR_V2_GLOB_SYS_INT_MASK			0x204
#define MV_XOR_V2_GLOB_MEM_INT_CAUSE			0x220
#define MV_XOR_V2_GLOB_MEM_INT_MASK			0x224

#define MV_XOR_V2_MIN_DESC_SIZE				32
#define MV_XOR_V2_EXT_DESC_SIZE				128

#define MV_XOR_V2_DESC_RESERVED_SIZE			12
#define MV_XOR_V2_DESC_BUFF_D_ADDR_SIZE			12

#define MV_XOR_V2_CMD_LINE_NUM_MAX_D_BUF		8

 
#define MV_XOR_V2_DESC_NUM				1024

 
#define MV_XOR_V2_DONE_IMSG_THRD  0x14
#define MV_XOR_V2_TIMER_THRD      0xB0

 
struct mv_xor_v2_descriptor {
	u16 desc_id;
	u16 flags;
	u32 crc32_result;
	u32 desc_ctrl;

	 
#define DESC_NUM_ACTIVE_D_BUF_SHIFT	22
#define DESC_OP_MODE_SHIFT		28
#define DESC_OP_MODE_NOP		0	 
#define DESC_OP_MODE_MEMCPY		1	 
#define DESC_OP_MODE_MEMSET		2	 
#define DESC_OP_MODE_MEMINIT		3	 
#define DESC_OP_MODE_MEM_COMPARE	4	 
#define DESC_OP_MODE_CRC32		5	 
#define DESC_OP_MODE_XOR		6	 
#define DESC_OP_MODE_RAID6		7	 
#define DESC_OP_MODE_RAID6_REC		8	 
#define DESC_Q_BUFFER_ENABLE		BIT(16)
#define DESC_P_BUFFER_ENABLE		BIT(17)
#define DESC_IOD			BIT(27)

	u32 buff_size;
	u32 fill_pattern_src_addr[4];
	u32 data_buff_addr[MV_XOR_V2_DESC_BUFF_D_ADDR_SIZE];
	u32 reserved[MV_XOR_V2_DESC_RESERVED_SIZE];
};

 
struct mv_xor_v2_device {
	spinlock_t lock;
	void __iomem *dma_base;
	void __iomem *glob_base;
	struct clk *clk;
	struct clk *reg_clk;
	struct tasklet_struct irq_tasklet;
	struct list_head free_sw_desc;
	struct dma_device dmadev;
	struct dma_chan	dmachan;
	dma_addr_t hw_desq;
	struct mv_xor_v2_descriptor *hw_desq_virt;
	struct mv_xor_v2_sw_desc *sw_desq;
	int desc_size;
	unsigned int npendings;
	unsigned int hw_queue_idx;
	unsigned int irq;
};

 
struct mv_xor_v2_sw_desc {
	int idx;
	struct dma_async_tx_descriptor async_tx;
	struct mv_xor_v2_descriptor hw_desc;
	struct list_head free_list;
};

 
static void mv_xor_v2_set_data_buffers(struct mv_xor_v2_device *xor_dev,
					struct mv_xor_v2_descriptor *desc,
					dma_addr_t src, int index)
{
	int arr_index = ((index >> 1) * 3);

	 
	if ((index & 0x1) == 0) {
		desc->data_buff_addr[arr_index] = lower_32_bits(src);

		desc->data_buff_addr[arr_index + 2] &= ~0xFFFF;
		desc->data_buff_addr[arr_index + 2] |=
			upper_32_bits(src) & 0xFFFF;
	} else {
		desc->data_buff_addr[arr_index + 1] =
			lower_32_bits(src);

		desc->data_buff_addr[arr_index + 2] &= ~0xFFFF0000;
		desc->data_buff_addr[arr_index + 2] |=
			(upper_32_bits(src) & 0xFFFF) << 16;
	}
}

 
static void mv_xor_v2_add_desc_to_desq(struct mv_xor_v2_device *xor_dev,
				       int num_of_desc)
{
	 
	writel(num_of_desc, xor_dev->dma_base + MV_XOR_V2_DMA_DESQ_ADD_OFF);
}

 
static void mv_xor_v2_free_desc_from_desq(struct mv_xor_v2_device *xor_dev,
					  int num_of_desc)
{
	 
	writel(num_of_desc, xor_dev->dma_base + MV_XOR_V2_DMA_DESQ_DEALLOC_OFF);
}

 
static int mv_xor_v2_set_desc_size(struct mv_xor_v2_device *xor_dev)
{
	writel(MV_XOR_V2_DMA_DESQ_CTRL_128B,
	       xor_dev->dma_base + MV_XOR_V2_DMA_DESQ_CTRL_OFF);

	return MV_XOR_V2_EXT_DESC_SIZE;
}

 
static inline
void mv_xor_v2_enable_imsg_thrd(struct mv_xor_v2_device *xor_dev)
{
	u32 reg;

	 
	reg = readl(xor_dev->dma_base + MV_XOR_V2_DMA_IMSG_THRD_OFF);
	reg &= ~MV_XOR_V2_DMA_IMSG_THRD_MASK;
	reg |= MV_XOR_V2_DONE_IMSG_THRD;
	reg |= MV_XOR_V2_DMA_IMSG_TIMER_EN;
	writel(reg, xor_dev->dma_base + MV_XOR_V2_DMA_IMSG_THRD_OFF);

	 
	reg = readl(xor_dev->dma_base + MV_XOR_V2_DMA_IMSG_TMOT);
	reg &= ~MV_XOR_V2_DMA_IMSG_TIMER_THRD_MASK;
	reg |= MV_XOR_V2_TIMER_THRD;
	writel(reg, xor_dev->dma_base + MV_XOR_V2_DMA_IMSG_TMOT);
}

static irqreturn_t mv_xor_v2_interrupt_handler(int irq, void *data)
{
	struct mv_xor_v2_device *xor_dev = data;
	unsigned int ndescs;
	u32 reg;

	reg = readl(xor_dev->dma_base + MV_XOR_V2_DMA_DESQ_DONE_OFF);

	ndescs = ((reg >> MV_XOR_V2_DMA_DESQ_DONE_PENDING_SHIFT) &
		  MV_XOR_V2_DMA_DESQ_DONE_PENDING_MASK);

	 
	if (!ndescs)
		return IRQ_NONE;

	 
	tasklet_schedule(&xor_dev->irq_tasklet);

	return IRQ_HANDLED;
}

 
static dma_cookie_t
mv_xor_v2_tx_submit(struct dma_async_tx_descriptor *tx)
{
	void *dest_hw_desc;
	dma_cookie_t cookie;
	struct mv_xor_v2_sw_desc *sw_desc =
		container_of(tx, struct mv_xor_v2_sw_desc, async_tx);
	struct mv_xor_v2_device *xor_dev =
		container_of(tx->chan, struct mv_xor_v2_device, dmachan);

	dev_dbg(xor_dev->dmadev.dev,
		"%s sw_desc %p: async_tx %p\n",
		__func__, sw_desc, &sw_desc->async_tx);

	 
	spin_lock_bh(&xor_dev->lock);
	cookie = dma_cookie_assign(tx);

	 
	dest_hw_desc = xor_dev->hw_desq_virt + xor_dev->hw_queue_idx;

	memcpy(dest_hw_desc, &sw_desc->hw_desc, xor_dev->desc_size);

	xor_dev->npendings++;
	xor_dev->hw_queue_idx++;
	if (xor_dev->hw_queue_idx >= MV_XOR_V2_DESC_NUM)
		xor_dev->hw_queue_idx = 0;

	spin_unlock_bh(&xor_dev->lock);

	return cookie;
}

 
static struct mv_xor_v2_sw_desc	*
mv_xor_v2_prep_sw_desc(struct mv_xor_v2_device *xor_dev)
{
	struct mv_xor_v2_sw_desc *sw_desc;
	bool found = false;

	 
	spin_lock_bh(&xor_dev->lock);

	if (list_empty(&xor_dev->free_sw_desc)) {
		spin_unlock_bh(&xor_dev->lock);
		 
		tasklet_schedule(&xor_dev->irq_tasklet);
		return NULL;
	}

	list_for_each_entry(sw_desc, &xor_dev->free_sw_desc, free_list) {
		if (async_tx_test_ack(&sw_desc->async_tx)) {
			found = true;
			break;
		}
	}

	if (!found) {
		spin_unlock_bh(&xor_dev->lock);
		return NULL;
	}

	list_del(&sw_desc->free_list);

	 
	spin_unlock_bh(&xor_dev->lock);

	return sw_desc;
}

 
static struct dma_async_tx_descriptor *
mv_xor_v2_prep_dma_memcpy(struct dma_chan *chan, dma_addr_t dest,
			  dma_addr_t src, size_t len, unsigned long flags)
{
	struct mv_xor_v2_sw_desc *sw_desc;
	struct mv_xor_v2_descriptor *hw_descriptor;
	struct mv_xor_v2_device	*xor_dev;

	xor_dev = container_of(chan, struct mv_xor_v2_device, dmachan);

	dev_dbg(xor_dev->dmadev.dev,
		"%s len: %zu src %pad dest %pad flags: %ld\n",
		__func__, len, &src, &dest, flags);

	sw_desc = mv_xor_v2_prep_sw_desc(xor_dev);
	if (!sw_desc)
		return NULL;

	sw_desc->async_tx.flags = flags;

	 
	hw_descriptor = &sw_desc->hw_desc;

	 
	hw_descriptor->desc_id = sw_desc->idx;

	 
	hw_descriptor->desc_ctrl =
		DESC_OP_MODE_MEMCPY << DESC_OP_MODE_SHIFT;

	if (flags & DMA_PREP_INTERRUPT)
		hw_descriptor->desc_ctrl |= DESC_IOD;

	 
	hw_descriptor->fill_pattern_src_addr[0] = lower_32_bits(src);
	hw_descriptor->fill_pattern_src_addr[1] =
		upper_32_bits(src) & 0xFFFF;

	 
	hw_descriptor->fill_pattern_src_addr[2] = lower_32_bits(dest);
	hw_descriptor->fill_pattern_src_addr[3] =
		upper_32_bits(dest) & 0xFFFF;

	 
	hw_descriptor->buff_size = len;

	 
	return &sw_desc->async_tx;
}

 
static struct dma_async_tx_descriptor *
mv_xor_v2_prep_dma_xor(struct dma_chan *chan, dma_addr_t dest, dma_addr_t *src,
		       unsigned int src_cnt, size_t len, unsigned long flags)
{
	struct mv_xor_v2_sw_desc *sw_desc;
	struct mv_xor_v2_descriptor *hw_descriptor;
	struct mv_xor_v2_device	*xor_dev =
		container_of(chan, struct mv_xor_v2_device, dmachan);
	int i;

	if (src_cnt > MV_XOR_V2_CMD_LINE_NUM_MAX_D_BUF || src_cnt < 1)
		return NULL;

	dev_dbg(xor_dev->dmadev.dev,
		"%s src_cnt: %d len: %zu dest %pad flags: %ld\n",
		__func__, src_cnt, len, &dest, flags);

	sw_desc = mv_xor_v2_prep_sw_desc(xor_dev);
	if (!sw_desc)
		return NULL;

	sw_desc->async_tx.flags = flags;

	 
	hw_descriptor = &sw_desc->hw_desc;

	 
	hw_descriptor->desc_id = sw_desc->idx;

	 
	hw_descriptor->desc_ctrl =
		DESC_OP_MODE_XOR << DESC_OP_MODE_SHIFT;
	hw_descriptor->desc_ctrl |= DESC_P_BUFFER_ENABLE;

	if (flags & DMA_PREP_INTERRUPT)
		hw_descriptor->desc_ctrl |= DESC_IOD;

	 
	for (i = 0; i < src_cnt; i++)
		mv_xor_v2_set_data_buffers(xor_dev, hw_descriptor, src[i], i);

	hw_descriptor->desc_ctrl |=
		src_cnt << DESC_NUM_ACTIVE_D_BUF_SHIFT;

	 
	hw_descriptor->fill_pattern_src_addr[2] = lower_32_bits(dest);
	hw_descriptor->fill_pattern_src_addr[3] =
		upper_32_bits(dest) & 0xFFFF;

	 
	hw_descriptor->buff_size = len;

	 
	return &sw_desc->async_tx;
}

 
static struct dma_async_tx_descriptor *
mv_xor_v2_prep_dma_interrupt(struct dma_chan *chan, unsigned long flags)
{
	struct mv_xor_v2_sw_desc *sw_desc;
	struct mv_xor_v2_descriptor *hw_descriptor;
	struct mv_xor_v2_device	*xor_dev =
		container_of(chan, struct mv_xor_v2_device, dmachan);

	sw_desc = mv_xor_v2_prep_sw_desc(xor_dev);
	if (!sw_desc)
		return NULL;

	 
	hw_descriptor = &sw_desc->hw_desc;

	 
	hw_descriptor->desc_id = sw_desc->idx;

	 
	hw_descriptor->desc_ctrl =
		DESC_OP_MODE_NOP << DESC_OP_MODE_SHIFT;
	hw_descriptor->desc_ctrl |= DESC_IOD;

	 
	return &sw_desc->async_tx;
}

 
static void mv_xor_v2_issue_pending(struct dma_chan *chan)
{
	struct mv_xor_v2_device *xor_dev =
		container_of(chan, struct mv_xor_v2_device, dmachan);

	spin_lock_bh(&xor_dev->lock);

	 
	mv_xor_v2_add_desc_to_desq(xor_dev, xor_dev->npendings);
	xor_dev->npendings = 0;

	spin_unlock_bh(&xor_dev->lock);
}

static inline
int mv_xor_v2_get_pending_params(struct mv_xor_v2_device *xor_dev,
				 int *pending_ptr)
{
	u32 reg;

	reg = readl(xor_dev->dma_base + MV_XOR_V2_DMA_DESQ_DONE_OFF);

	 
	*pending_ptr = ((reg >> MV_XOR_V2_DMA_DESQ_DONE_READ_PTR_SHIFT) &
			MV_XOR_V2_DMA_DESQ_DONE_READ_PTR_MASK);

	 
	return ((reg >> MV_XOR_V2_DMA_DESQ_DONE_PENDING_SHIFT) &
		MV_XOR_V2_DMA_DESQ_DONE_PENDING_MASK);
}

 
static void mv_xor_v2_tasklet(struct tasklet_struct *t)
{
	struct mv_xor_v2_device *xor_dev = from_tasklet(xor_dev, t,
							irq_tasklet);
	int pending_ptr, num_of_pending, i;
	struct mv_xor_v2_sw_desc *next_pending_sw_desc = NULL;

	dev_dbg(xor_dev->dmadev.dev, "%s %d\n", __func__, __LINE__);

	 
	num_of_pending = mv_xor_v2_get_pending_params(xor_dev, &pending_ptr);

	 
	for (i = 0; i < num_of_pending; i++) {
		struct mv_xor_v2_descriptor *next_pending_hw_desc =
			xor_dev->hw_desq_virt + pending_ptr;

		 
		next_pending_sw_desc =
			&xor_dev->sw_desq[next_pending_hw_desc->desc_id];

		 
		if (next_pending_sw_desc->async_tx.cookie > 0) {
			 
			dma_cookie_complete(&next_pending_sw_desc->async_tx);

			dma_descriptor_unmap(&next_pending_sw_desc->async_tx);
			dmaengine_desc_get_callback_invoke(
					&next_pending_sw_desc->async_tx, NULL);
		}

		dma_run_dependencies(&next_pending_sw_desc->async_tx);

		 
		spin_lock(&xor_dev->lock);

		 
		list_add(&next_pending_sw_desc->free_list,
			 &xor_dev->free_sw_desc);

		 
		spin_unlock(&xor_dev->lock);

		 
		pending_ptr++;
		if (pending_ptr >= MV_XOR_V2_DESC_NUM)
			pending_ptr = 0;
	}

	if (num_of_pending != 0) {
		 
		mv_xor_v2_free_desc_from_desq(xor_dev, num_of_pending);
	}
}

 
static void mv_xor_v2_set_msi_msg(struct msi_desc *desc, struct msi_msg *msg)
{
	struct mv_xor_v2_device *xor_dev = dev_get_drvdata(desc->dev);

	writel(msg->address_lo,
	       xor_dev->dma_base + MV_XOR_V2_DMA_IMSG_BALR_OFF);
	writel(msg->address_hi & 0xFFFF,
	       xor_dev->dma_base + MV_XOR_V2_DMA_IMSG_BAHR_OFF);
	writel(msg->data,
	       xor_dev->dma_base + MV_XOR_V2_DMA_IMSG_CDAT_OFF);
}

static int mv_xor_v2_descq_init(struct mv_xor_v2_device *xor_dev)
{
	u32 reg;

	 
	writel(MV_XOR_V2_DESC_NUM,
	       xor_dev->dma_base + MV_XOR_V2_DMA_DESQ_SIZE_OFF);

	 
	writel(lower_32_bits(xor_dev->hw_desq),
	       xor_dev->dma_base + MV_XOR_V2_DMA_DESQ_BALR_OFF);
	writel(upper_32_bits(xor_dev->hw_desq),
	       xor_dev->dma_base + MV_XOR_V2_DMA_DESQ_BAHR_OFF);

	 
	reg = readl(xor_dev->dma_base + MV_XOR_V2_DMA_DESQ_ARATTR_OFF);
	reg &= ~MV_XOR_V2_DMA_DESQ_ATTR_CACHE_MASK;
	reg |= MV_XOR_V2_DMA_DESQ_ATTR_OUTER_SHAREABLE |
		MV_XOR_V2_DMA_DESQ_ATTR_CACHEABLE;
	writel(reg, xor_dev->dma_base + MV_XOR_V2_DMA_DESQ_ARATTR_OFF);

	reg = readl(xor_dev->dma_base + MV_XOR_V2_DMA_DESQ_AWATTR_OFF);
	reg &= ~MV_XOR_V2_DMA_DESQ_ATTR_CACHE_MASK;
	reg |= MV_XOR_V2_DMA_DESQ_ATTR_OUTER_SHAREABLE |
		MV_XOR_V2_DMA_DESQ_ATTR_CACHEABLE;
	writel(reg, xor_dev->dma_base + MV_XOR_V2_DMA_DESQ_AWATTR_OFF);

	 
	reg = ((MV_XOR_V2_GLOB_BW_CTRL_NUM_OSTD_RD_VAL <<
		MV_XOR_V2_GLOB_BW_CTRL_NUM_OSTD_RD_SHIFT) |
	       (MV_XOR_V2_GLOB_BW_CTRL_NUM_OSTD_WR_VAL  <<
		MV_XOR_V2_GLOB_BW_CTRL_NUM_OSTD_WR_SHIFT) |
	       (MV_XOR_V2_GLOB_BW_CTRL_RD_BURST_LEN_VAL <<
		MV_XOR_V2_GLOB_BW_CTRL_RD_BURST_LEN_SHIFT) |
	       (MV_XOR_V2_GLOB_BW_CTRL_WR_BURST_LEN_VAL <<
		MV_XOR_V2_GLOB_BW_CTRL_WR_BURST_LEN_SHIFT));
	writel(reg, xor_dev->glob_base + MV_XOR_V2_GLOB_BW_CTRL);

	 
	reg = readl(xor_dev->glob_base + MV_XOR_V2_GLOB_PAUSE);
	reg |= MV_XOR_V2_GLOB_PAUSE_AXI_TIME_DIS_VAL;
	writel(reg, xor_dev->glob_base + MV_XOR_V2_GLOB_PAUSE);

	 
	writel(0, xor_dev->dma_base + MV_XOR_V2_DMA_DESQ_STOP_OFF);

	return 0;
}

static int mv_xor_v2_suspend(struct platform_device *dev, pm_message_t state)
{
	struct mv_xor_v2_device *xor_dev = platform_get_drvdata(dev);

	 
	writel(0x1, xor_dev->dma_base + MV_XOR_V2_DMA_DESQ_STOP_OFF);

	return 0;
}

static int mv_xor_v2_resume(struct platform_device *dev)
{
	struct mv_xor_v2_device *xor_dev = platform_get_drvdata(dev);

	mv_xor_v2_set_desc_size(xor_dev);
	mv_xor_v2_enable_imsg_thrd(xor_dev);
	mv_xor_v2_descq_init(xor_dev);

	return 0;
}

static int mv_xor_v2_probe(struct platform_device *pdev)
{
	struct mv_xor_v2_device *xor_dev;
	int i, ret = 0;
	struct dma_device *dma_dev;
	struct mv_xor_v2_sw_desc *sw_desc;

	BUILD_BUG_ON(sizeof(struct mv_xor_v2_descriptor) !=
		     MV_XOR_V2_EXT_DESC_SIZE);

	xor_dev = devm_kzalloc(&pdev->dev, sizeof(*xor_dev), GFP_KERNEL);
	if (!xor_dev)
		return -ENOMEM;

	xor_dev->dma_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(xor_dev->dma_base))
		return PTR_ERR(xor_dev->dma_base);

	xor_dev->glob_base = devm_platform_ioremap_resource(pdev, 1);
	if (IS_ERR(xor_dev->glob_base))
		return PTR_ERR(xor_dev->glob_base);

	platform_set_drvdata(pdev, xor_dev);

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(40));
	if (ret)
		return ret;

	xor_dev->reg_clk = devm_clk_get_optional_enabled(&pdev->dev, "reg");
	if (IS_ERR(xor_dev->reg_clk))
		return PTR_ERR(xor_dev->reg_clk);

	xor_dev->clk = devm_clk_get_enabled(&pdev->dev, NULL);
	if (IS_ERR(xor_dev->clk))
		return PTR_ERR(xor_dev->clk);

	ret = platform_msi_domain_alloc_irqs(&pdev->dev, 1,
					     mv_xor_v2_set_msi_msg);
	if (ret)
		return ret;

	xor_dev->irq = msi_get_virq(&pdev->dev, 0);

	ret = devm_request_irq(&pdev->dev, xor_dev->irq,
			       mv_xor_v2_interrupt_handler, 0,
			       dev_name(&pdev->dev), xor_dev);
	if (ret)
		goto free_msi_irqs;

	tasklet_setup(&xor_dev->irq_tasklet, mv_xor_v2_tasklet);

	xor_dev->desc_size = mv_xor_v2_set_desc_size(xor_dev);

	dma_cookie_init(&xor_dev->dmachan);

	 
	xor_dev->hw_desq_virt =
		dma_alloc_coherent(&pdev->dev,
				   xor_dev->desc_size * MV_XOR_V2_DESC_NUM,
				   &xor_dev->hw_desq, GFP_KERNEL);
	if (!xor_dev->hw_desq_virt) {
		ret = -ENOMEM;
		goto free_msi_irqs;
	}

	 
	xor_dev->sw_desq = devm_kcalloc(&pdev->dev,
					MV_XOR_V2_DESC_NUM, sizeof(*sw_desc),
					GFP_KERNEL);
	if (!xor_dev->sw_desq) {
		ret = -ENOMEM;
		goto free_hw_desq;
	}

	spin_lock_init(&xor_dev->lock);

	 
	INIT_LIST_HEAD(&xor_dev->free_sw_desc);

	 
	for (i = 0; i < MV_XOR_V2_DESC_NUM; i++) {
		struct mv_xor_v2_sw_desc *sw_desc =
			xor_dev->sw_desq + i;
		sw_desc->idx = i;
		dma_async_tx_descriptor_init(&sw_desc->async_tx,
					     &xor_dev->dmachan);
		sw_desc->async_tx.tx_submit = mv_xor_v2_tx_submit;
		async_tx_ack(&sw_desc->async_tx);

		list_add(&sw_desc->free_list,
			 &xor_dev->free_sw_desc);
	}

	dma_dev = &xor_dev->dmadev;

	 
	dma_cap_zero(dma_dev->cap_mask);
	dma_cap_set(DMA_MEMCPY, dma_dev->cap_mask);
	dma_cap_set(DMA_XOR, dma_dev->cap_mask);
	dma_cap_set(DMA_INTERRUPT, dma_dev->cap_mask);

	 
	INIT_LIST_HEAD(&dma_dev->channels);

	 
	dma_dev->device_tx_status = dma_cookie_status;
	dma_dev->device_issue_pending = mv_xor_v2_issue_pending;
	dma_dev->dev = &pdev->dev;

	dma_dev->device_prep_dma_memcpy = mv_xor_v2_prep_dma_memcpy;
	dma_dev->device_prep_dma_interrupt = mv_xor_v2_prep_dma_interrupt;
	dma_dev->max_xor = 8;
	dma_dev->device_prep_dma_xor = mv_xor_v2_prep_dma_xor;

	xor_dev->dmachan.device = dma_dev;

	list_add_tail(&xor_dev->dmachan.device_node,
		      &dma_dev->channels);

	mv_xor_v2_enable_imsg_thrd(xor_dev);

	mv_xor_v2_descq_init(xor_dev);

	ret = dma_async_device_register(dma_dev);
	if (ret)
		goto free_hw_desq;

	dev_notice(&pdev->dev, "Marvell Version 2 XOR driver\n");

	return 0;

free_hw_desq:
	dma_free_coherent(&pdev->dev,
			  xor_dev->desc_size * MV_XOR_V2_DESC_NUM,
			  xor_dev->hw_desq_virt, xor_dev->hw_desq);
free_msi_irqs:
	platform_msi_domain_free_irqs(&pdev->dev);
	return ret;
}

static int mv_xor_v2_remove(struct platform_device *pdev)
{
	struct mv_xor_v2_device *xor_dev = platform_get_drvdata(pdev);

	dma_async_device_unregister(&xor_dev->dmadev);

	dma_free_coherent(&pdev->dev,
			  xor_dev->desc_size * MV_XOR_V2_DESC_NUM,
			  xor_dev->hw_desq_virt, xor_dev->hw_desq);

	devm_free_irq(&pdev->dev, xor_dev->irq, xor_dev);

	platform_msi_domain_free_irqs(&pdev->dev);

	tasklet_kill(&xor_dev->irq_tasklet);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mv_xor_v2_dt_ids[] = {
	{ .compatible = "marvell,xor-v2", },
	{},
};
MODULE_DEVICE_TABLE(of, mv_xor_v2_dt_ids);
#endif

static struct platform_driver mv_xor_v2_driver = {
	.probe		= mv_xor_v2_probe,
	.suspend	= mv_xor_v2_suspend,
	.resume		= mv_xor_v2_resume,
	.remove		= mv_xor_v2_remove,
	.driver		= {
		.name	= "mv_xor_v2",
		.of_match_table = of_match_ptr(mv_xor_v2_dt_ids),
	},
};

module_platform_driver(mv_xor_v2_driver);

MODULE_DESCRIPTION("DMA engine driver for Marvell's Version 2 of XOR engine");
