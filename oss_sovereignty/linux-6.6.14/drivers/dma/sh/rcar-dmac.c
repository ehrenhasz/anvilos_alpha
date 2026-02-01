
 

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_dma.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "../dmaengine.h"

 
struct rcar_dmac_xfer_chunk {
	struct list_head node;

	dma_addr_t src_addr;
	dma_addr_t dst_addr;
	u32 size;
};

 
struct rcar_dmac_hw_desc {
	u32 sar;
	u32 dar;
	u32 tcr;
	u32 reserved;
} __attribute__((__packed__));

 
struct rcar_dmac_desc {
	struct dma_async_tx_descriptor async_tx;
	enum dma_transfer_direction direction;
	unsigned int xfer_shift;
	u32 chcr;

	struct list_head node;
	struct list_head chunks;
	struct rcar_dmac_xfer_chunk *running;
	unsigned int nchunks;

	struct {
		bool use;
		struct rcar_dmac_hw_desc *mem;
		dma_addr_t dma;
		size_t size;
	} hwdescs;

	unsigned int size;
	bool cyclic;
};

#define to_rcar_dmac_desc(d)	container_of(d, struct rcar_dmac_desc, async_tx)

 
struct rcar_dmac_desc_page {
	struct list_head node;

	union {
		DECLARE_FLEX_ARRAY(struct rcar_dmac_desc, descs);
		DECLARE_FLEX_ARRAY(struct rcar_dmac_xfer_chunk, chunks);
	};
};

#define RCAR_DMAC_DESCS_PER_PAGE					\
	((PAGE_SIZE - offsetof(struct rcar_dmac_desc_page, descs)) /	\
	sizeof(struct rcar_dmac_desc))
#define RCAR_DMAC_XFER_CHUNKS_PER_PAGE					\
	((PAGE_SIZE - offsetof(struct rcar_dmac_desc_page, chunks)) /	\
	sizeof(struct rcar_dmac_xfer_chunk))

 
struct rcar_dmac_chan_slave {
	phys_addr_t slave_addr;
	unsigned int xfer_size;
};

 
struct rcar_dmac_chan_map {
	dma_addr_t addr;
	enum dma_data_direction dir;
	struct rcar_dmac_chan_slave slave;
};

 
struct rcar_dmac_chan {
	struct dma_chan chan;
	void __iomem *iomem;
	unsigned int index;
	int irq;

	struct rcar_dmac_chan_slave src;
	struct rcar_dmac_chan_slave dst;
	struct rcar_dmac_chan_map map;
	int mid_rid;

	spinlock_t lock;

	struct {
		struct list_head free;
		struct list_head pending;
		struct list_head active;
		struct list_head done;
		struct list_head wait;
		struct rcar_dmac_desc *running;

		struct list_head chunks_free;

		struct list_head pages;
	} desc;
};

#define to_rcar_dmac_chan(c)	container_of(c, struct rcar_dmac_chan, chan)

 
struct rcar_dmac {
	struct dma_device engine;
	struct device *dev;
	void __iomem *dmac_base;
	void __iomem *chan_base;

	unsigned int n_channels;
	struct rcar_dmac_chan *channels;
	u32 channels_mask;

	DECLARE_BITMAP(modules, 256);
};

#define to_rcar_dmac(d)		container_of(d, struct rcar_dmac, engine)

#define for_each_rcar_dmac_chan(i, dmac, chan)						\
	for (i = 0, chan = &(dmac)->channels[0]; i < (dmac)->n_channels; i++, chan++)	\
		if (!((dmac)->channels_mask & BIT(i))) continue; else

 
struct rcar_dmac_of_data {
	u32 chan_offset_base;
	u32 chan_offset_stride;
};

 

#define RCAR_DMAISTA			0x0020
#define RCAR_DMASEC			0x0030
#define RCAR_DMAOR			0x0060
#define RCAR_DMAOR_PRI_FIXED		(0 << 8)
#define RCAR_DMAOR_PRI_ROUND_ROBIN	(3 << 8)
#define RCAR_DMAOR_AE			(1 << 2)
#define RCAR_DMAOR_DME			(1 << 0)
#define RCAR_DMACHCLR			0x0080	 
#define RCAR_DMADPSEC			0x00a0

#define RCAR_DMASAR			0x0000
#define RCAR_DMADAR			0x0004
#define RCAR_DMATCR			0x0008
#define RCAR_DMATCR_MASK		0x00ffffff
#define RCAR_DMATSR			0x0028
#define RCAR_DMACHCR			0x000c
#define RCAR_DMACHCR_CAE		(1 << 31)
#define RCAR_DMACHCR_CAIE		(1 << 30)
#define RCAR_DMACHCR_DPM_DISABLED	(0 << 28)
#define RCAR_DMACHCR_DPM_ENABLED	(1 << 28)
#define RCAR_DMACHCR_DPM_REPEAT		(2 << 28)
#define RCAR_DMACHCR_DPM_INFINITE	(3 << 28)
#define RCAR_DMACHCR_RPT_SAR		(1 << 27)
#define RCAR_DMACHCR_RPT_DAR		(1 << 26)
#define RCAR_DMACHCR_RPT_TCR		(1 << 25)
#define RCAR_DMACHCR_DPB		(1 << 22)
#define RCAR_DMACHCR_DSE		(1 << 19)
#define RCAR_DMACHCR_DSIE		(1 << 18)
#define RCAR_DMACHCR_TS_1B		((0 << 20) | (0 << 3))
#define RCAR_DMACHCR_TS_2B		((0 << 20) | (1 << 3))
#define RCAR_DMACHCR_TS_4B		((0 << 20) | (2 << 3))
#define RCAR_DMACHCR_TS_16B		((0 << 20) | (3 << 3))
#define RCAR_DMACHCR_TS_32B		((1 << 20) | (0 << 3))
#define RCAR_DMACHCR_TS_64B		((1 << 20) | (1 << 3))
#define RCAR_DMACHCR_TS_8B		((1 << 20) | (3 << 3))
#define RCAR_DMACHCR_DM_FIXED		(0 << 14)
#define RCAR_DMACHCR_DM_INC		(1 << 14)
#define RCAR_DMACHCR_DM_DEC		(2 << 14)
#define RCAR_DMACHCR_SM_FIXED		(0 << 12)
#define RCAR_DMACHCR_SM_INC		(1 << 12)
#define RCAR_DMACHCR_SM_DEC		(2 << 12)
#define RCAR_DMACHCR_RS_AUTO		(4 << 8)
#define RCAR_DMACHCR_RS_DMARS		(8 << 8)
#define RCAR_DMACHCR_IE			(1 << 2)
#define RCAR_DMACHCR_TE			(1 << 1)
#define RCAR_DMACHCR_DE			(1 << 0)
#define RCAR_DMATCRB			0x0018
#define RCAR_DMATSRB			0x0038
#define RCAR_DMACHCRB			0x001c
#define RCAR_DMACHCRB_DCNT(n)		((n) << 24)
#define RCAR_DMACHCRB_DPTR_MASK		(0xff << 16)
#define RCAR_DMACHCRB_DPTR_SHIFT	16
#define RCAR_DMACHCRB_DRST		(1 << 15)
#define RCAR_DMACHCRB_DTS		(1 << 8)
#define RCAR_DMACHCRB_SLM_NORMAL	(0 << 4)
#define RCAR_DMACHCRB_SLM_CLK(n)	((8 | (n)) << 4)
#define RCAR_DMACHCRB_PRI(n)		((n) << 0)
#define RCAR_DMARS			0x0040
#define RCAR_DMABUFCR			0x0048
#define RCAR_DMABUFCR_MBU(n)		((n) << 16)
#define RCAR_DMABUFCR_ULB(n)		((n) << 0)
#define RCAR_DMADPBASE			0x0050
#define RCAR_DMADPBASE_MASK		0xfffffff0
#define RCAR_DMADPBASE_SEL		(1 << 0)
#define RCAR_DMADPCR			0x0054
#define RCAR_DMADPCR_DIPT(n)		((n) << 24)
#define RCAR_DMAFIXSAR			0x0010
#define RCAR_DMAFIXDAR			0x0014
#define RCAR_DMAFIXDPBASE		0x0060

 
#define RCAR_GEN4_DMACHCLR		0x0100

 
#define RCAR_DMAC_MEMCPY_XFER_SIZE	4

 

static void rcar_dmac_write(struct rcar_dmac *dmac, u32 reg, u32 data)
{
	if (reg == RCAR_DMAOR)
		writew(data, dmac->dmac_base + reg);
	else
		writel(data, dmac->dmac_base + reg);
}

static u32 rcar_dmac_read(struct rcar_dmac *dmac, u32 reg)
{
	if (reg == RCAR_DMAOR)
		return readw(dmac->dmac_base + reg);
	else
		return readl(dmac->dmac_base + reg);
}

static u32 rcar_dmac_chan_read(struct rcar_dmac_chan *chan, u32 reg)
{
	if (reg == RCAR_DMARS)
		return readw(chan->iomem + reg);
	else
		return readl(chan->iomem + reg);
}

static void rcar_dmac_chan_write(struct rcar_dmac_chan *chan, u32 reg, u32 data)
{
	if (reg == RCAR_DMARS)
		writew(data, chan->iomem + reg);
	else
		writel(data, chan->iomem + reg);
}

static void rcar_dmac_chan_clear(struct rcar_dmac *dmac,
				 struct rcar_dmac_chan *chan)
{
	if (dmac->chan_base)
		rcar_dmac_chan_write(chan, RCAR_GEN4_DMACHCLR, 1);
	else
		rcar_dmac_write(dmac, RCAR_DMACHCLR, BIT(chan->index));
}

static void rcar_dmac_chan_clear_all(struct rcar_dmac *dmac)
{
	struct rcar_dmac_chan *chan;
	unsigned int i;

	if (dmac->chan_base) {
		for_each_rcar_dmac_chan(i, dmac, chan)
			rcar_dmac_chan_write(chan, RCAR_GEN4_DMACHCLR, 1);
	} else {
		rcar_dmac_write(dmac, RCAR_DMACHCLR, dmac->channels_mask);
	}
}

 

static bool rcar_dmac_chan_is_busy(struct rcar_dmac_chan *chan)
{
	u32 chcr = rcar_dmac_chan_read(chan, RCAR_DMACHCR);

	return !!(chcr & (RCAR_DMACHCR_DE | RCAR_DMACHCR_TE));
}

static void rcar_dmac_chan_start_xfer(struct rcar_dmac_chan *chan)
{
	struct rcar_dmac_desc *desc = chan->desc.running;
	u32 chcr = desc->chcr;

	WARN_ON_ONCE(rcar_dmac_chan_is_busy(chan));

	if (chan->mid_rid >= 0)
		rcar_dmac_chan_write(chan, RCAR_DMARS, chan->mid_rid);

	if (desc->hwdescs.use) {
		struct rcar_dmac_xfer_chunk *chunk =
			list_first_entry(&desc->chunks,
					 struct rcar_dmac_xfer_chunk, node);

		dev_dbg(chan->chan.device->dev,
			"chan%u: queue desc %p: %u@%pad\n",
			chan->index, desc, desc->nchunks, &desc->hwdescs.dma);

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
		rcar_dmac_chan_write(chan, RCAR_DMAFIXSAR,
				     chunk->src_addr >> 32);
		rcar_dmac_chan_write(chan, RCAR_DMAFIXDAR,
				     chunk->dst_addr >> 32);
		rcar_dmac_chan_write(chan, RCAR_DMAFIXDPBASE,
				     desc->hwdescs.dma >> 32);
#endif
		rcar_dmac_chan_write(chan, RCAR_DMADPBASE,
				     (desc->hwdescs.dma & 0xfffffff0) |
				     RCAR_DMADPBASE_SEL);
		rcar_dmac_chan_write(chan, RCAR_DMACHCRB,
				     RCAR_DMACHCRB_DCNT(desc->nchunks - 1) |
				     RCAR_DMACHCRB_DRST);

		 
		rcar_dmac_chan_write(chan, RCAR_DMADAR,
				     chunk->dst_addr & 0xffffffff);

		 
		rcar_dmac_chan_write(chan, RCAR_DMADPCR, RCAR_DMADPCR_DIPT(1));

		chcr |= RCAR_DMACHCR_RPT_SAR | RCAR_DMACHCR_RPT_DAR
		     |  RCAR_DMACHCR_RPT_TCR | RCAR_DMACHCR_DPB;

		 
		if (!desc->cyclic)
			chcr |= RCAR_DMACHCR_DPM_ENABLED | RCAR_DMACHCR_IE;
		 
		else if (desc->async_tx.callback)
			chcr |= RCAR_DMACHCR_DPM_INFINITE | RCAR_DMACHCR_DSIE;
		 
		else
			chcr |= RCAR_DMACHCR_DPM_INFINITE;
	} else {
		struct rcar_dmac_xfer_chunk *chunk = desc->running;

		dev_dbg(chan->chan.device->dev,
			"chan%u: queue chunk %p: %u@%pad -> %pad\n",
			chan->index, chunk, chunk->size, &chunk->src_addr,
			&chunk->dst_addr);

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
		rcar_dmac_chan_write(chan, RCAR_DMAFIXSAR,
				     chunk->src_addr >> 32);
		rcar_dmac_chan_write(chan, RCAR_DMAFIXDAR,
				     chunk->dst_addr >> 32);
#endif
		rcar_dmac_chan_write(chan, RCAR_DMASAR,
				     chunk->src_addr & 0xffffffff);
		rcar_dmac_chan_write(chan, RCAR_DMADAR,
				     chunk->dst_addr & 0xffffffff);
		rcar_dmac_chan_write(chan, RCAR_DMATCR,
				     chunk->size >> desc->xfer_shift);

		chcr |= RCAR_DMACHCR_DPM_DISABLED | RCAR_DMACHCR_IE;
	}

	rcar_dmac_chan_write(chan, RCAR_DMACHCR,
			     chcr | RCAR_DMACHCR_DE | RCAR_DMACHCR_CAIE);
}

static int rcar_dmac_init(struct rcar_dmac *dmac)
{
	u16 dmaor;

	 
	rcar_dmac_chan_clear_all(dmac);
	rcar_dmac_write(dmac, RCAR_DMAOR,
			RCAR_DMAOR_PRI_FIXED | RCAR_DMAOR_DME);

	dmaor = rcar_dmac_read(dmac, RCAR_DMAOR);
	if ((dmaor & (RCAR_DMAOR_AE | RCAR_DMAOR_DME)) != RCAR_DMAOR_DME) {
		dev_warn(dmac->dev, "DMAOR initialization failed.\n");
		return -EIO;
	}

	return 0;
}

 

static dma_cookie_t rcar_dmac_tx_submit(struct dma_async_tx_descriptor *tx)
{
	struct rcar_dmac_chan *chan = to_rcar_dmac_chan(tx->chan);
	struct rcar_dmac_desc *desc = to_rcar_dmac_desc(tx);
	unsigned long flags;
	dma_cookie_t cookie;

	spin_lock_irqsave(&chan->lock, flags);

	cookie = dma_cookie_assign(tx);

	dev_dbg(chan->chan.device->dev, "chan%u: submit #%d@%p\n",
		chan->index, tx->cookie, desc);

	list_add_tail(&desc->node, &chan->desc.pending);
	desc->running = list_first_entry(&desc->chunks,
					 struct rcar_dmac_xfer_chunk, node);

	spin_unlock_irqrestore(&chan->lock, flags);

	return cookie;
}

 

 
static int rcar_dmac_desc_alloc(struct rcar_dmac_chan *chan, gfp_t gfp)
{
	struct rcar_dmac_desc_page *page;
	unsigned long flags;
	LIST_HEAD(list);
	unsigned int i;

	page = (void *)get_zeroed_page(gfp);
	if (!page)
		return -ENOMEM;

	for (i = 0; i < RCAR_DMAC_DESCS_PER_PAGE; ++i) {
		struct rcar_dmac_desc *desc = &page->descs[i];

		dma_async_tx_descriptor_init(&desc->async_tx, &chan->chan);
		desc->async_tx.tx_submit = rcar_dmac_tx_submit;
		INIT_LIST_HEAD(&desc->chunks);

		list_add_tail(&desc->node, &list);
	}

	spin_lock_irqsave(&chan->lock, flags);
	list_splice_tail(&list, &chan->desc.free);
	list_add_tail(&page->node, &chan->desc.pages);
	spin_unlock_irqrestore(&chan->lock, flags);

	return 0;
}

 
static void rcar_dmac_desc_put(struct rcar_dmac_chan *chan,
			       struct rcar_dmac_desc *desc)
{
	unsigned long flags;

	spin_lock_irqsave(&chan->lock, flags);
	list_splice_tail_init(&desc->chunks, &chan->desc.chunks_free);
	list_add(&desc->node, &chan->desc.free);
	spin_unlock_irqrestore(&chan->lock, flags);
}

static void rcar_dmac_desc_recycle_acked(struct rcar_dmac_chan *chan)
{
	struct rcar_dmac_desc *desc, *_desc;
	unsigned long flags;
	LIST_HEAD(list);

	 
	spin_lock_irqsave(&chan->lock, flags);
	list_splice_init(&chan->desc.wait, &list);
	spin_unlock_irqrestore(&chan->lock, flags);

	list_for_each_entry_safe(desc, _desc, &list, node) {
		if (async_tx_test_ack(&desc->async_tx)) {
			list_del(&desc->node);
			rcar_dmac_desc_put(chan, desc);
		}
	}

	if (list_empty(&list))
		return;

	 
	spin_lock_irqsave(&chan->lock, flags);
	list_splice(&list, &chan->desc.wait);
	spin_unlock_irqrestore(&chan->lock, flags);
}

 
static struct rcar_dmac_desc *rcar_dmac_desc_get(struct rcar_dmac_chan *chan)
{
	struct rcar_dmac_desc *desc;
	unsigned long flags;
	int ret;

	 
	rcar_dmac_desc_recycle_acked(chan);

	spin_lock_irqsave(&chan->lock, flags);

	while (list_empty(&chan->desc.free)) {
		 
		spin_unlock_irqrestore(&chan->lock, flags);
		ret = rcar_dmac_desc_alloc(chan, GFP_NOWAIT);
		if (ret < 0)
			return NULL;
		spin_lock_irqsave(&chan->lock, flags);
	}

	desc = list_first_entry(&chan->desc.free, struct rcar_dmac_desc, node);
	list_del(&desc->node);

	spin_unlock_irqrestore(&chan->lock, flags);

	return desc;
}

 
static int rcar_dmac_xfer_chunk_alloc(struct rcar_dmac_chan *chan, gfp_t gfp)
{
	struct rcar_dmac_desc_page *page;
	unsigned long flags;
	LIST_HEAD(list);
	unsigned int i;

	page = (void *)get_zeroed_page(gfp);
	if (!page)
		return -ENOMEM;

	for (i = 0; i < RCAR_DMAC_XFER_CHUNKS_PER_PAGE; ++i) {
		struct rcar_dmac_xfer_chunk *chunk = &page->chunks[i];

		list_add_tail(&chunk->node, &list);
	}

	spin_lock_irqsave(&chan->lock, flags);
	list_splice_tail(&list, &chan->desc.chunks_free);
	list_add_tail(&page->node, &chan->desc.pages);
	spin_unlock_irqrestore(&chan->lock, flags);

	return 0;
}

 
static struct rcar_dmac_xfer_chunk *
rcar_dmac_xfer_chunk_get(struct rcar_dmac_chan *chan)
{
	struct rcar_dmac_xfer_chunk *chunk;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&chan->lock, flags);

	while (list_empty(&chan->desc.chunks_free)) {
		 
		spin_unlock_irqrestore(&chan->lock, flags);
		ret = rcar_dmac_xfer_chunk_alloc(chan, GFP_NOWAIT);
		if (ret < 0)
			return NULL;
		spin_lock_irqsave(&chan->lock, flags);
	}

	chunk = list_first_entry(&chan->desc.chunks_free,
				 struct rcar_dmac_xfer_chunk, node);
	list_del(&chunk->node);

	spin_unlock_irqrestore(&chan->lock, flags);

	return chunk;
}

static void rcar_dmac_realloc_hwdesc(struct rcar_dmac_chan *chan,
				     struct rcar_dmac_desc *desc, size_t size)
{
	 
	size = PAGE_ALIGN(size);

	if (desc->hwdescs.size == size)
		return;

	if (desc->hwdescs.mem) {
		dma_free_coherent(chan->chan.device->dev, desc->hwdescs.size,
				  desc->hwdescs.mem, desc->hwdescs.dma);
		desc->hwdescs.mem = NULL;
		desc->hwdescs.size = 0;
	}

	if (!size)
		return;

	desc->hwdescs.mem = dma_alloc_coherent(chan->chan.device->dev, size,
					       &desc->hwdescs.dma, GFP_NOWAIT);
	if (!desc->hwdescs.mem)
		return;

	desc->hwdescs.size = size;
}

static int rcar_dmac_fill_hwdesc(struct rcar_dmac_chan *chan,
				 struct rcar_dmac_desc *desc)
{
	struct rcar_dmac_xfer_chunk *chunk;
	struct rcar_dmac_hw_desc *hwdesc;

	rcar_dmac_realloc_hwdesc(chan, desc, desc->nchunks * sizeof(*hwdesc));

	hwdesc = desc->hwdescs.mem;
	if (!hwdesc)
		return -ENOMEM;

	list_for_each_entry(chunk, &desc->chunks, node) {
		hwdesc->sar = chunk->src_addr;
		hwdesc->dar = chunk->dst_addr;
		hwdesc->tcr = chunk->size >> desc->xfer_shift;
		hwdesc++;
	}

	return 0;
}

 
static void rcar_dmac_chcr_de_barrier(struct rcar_dmac_chan *chan)
{
	u32 chcr;
	unsigned int i;

	 
	for (i = 0; i < 1024; i++) {
		chcr = rcar_dmac_chan_read(chan, RCAR_DMACHCR);
		if (!(chcr & RCAR_DMACHCR_DE))
			return;
		udelay(1);
	}

	dev_err(chan->chan.device->dev, "CHCR DE check error\n");
}

static void rcar_dmac_clear_chcr_de(struct rcar_dmac_chan *chan)
{
	u32 chcr = rcar_dmac_chan_read(chan, RCAR_DMACHCR);

	 
	rcar_dmac_chan_write(chan, RCAR_DMACHCR, (chcr & ~RCAR_DMACHCR_DE));

	 
	rcar_dmac_chcr_de_barrier(chan);
}

static void rcar_dmac_chan_halt(struct rcar_dmac_chan *chan)
{
	u32 chcr = rcar_dmac_chan_read(chan, RCAR_DMACHCR);

	chcr &= ~(RCAR_DMACHCR_DSE | RCAR_DMACHCR_DSIE | RCAR_DMACHCR_IE |
		  RCAR_DMACHCR_TE | RCAR_DMACHCR_DE |
		  RCAR_DMACHCR_CAE | RCAR_DMACHCR_CAIE);
	rcar_dmac_chan_write(chan, RCAR_DMACHCR, chcr);
	rcar_dmac_chcr_de_barrier(chan);
}

static void rcar_dmac_chan_reinit(struct rcar_dmac_chan *chan)
{
	struct rcar_dmac_desc *desc, *_desc;
	unsigned long flags;
	LIST_HEAD(descs);

	spin_lock_irqsave(&chan->lock, flags);

	 
	list_splice_init(&chan->desc.pending, &descs);
	list_splice_init(&chan->desc.active, &descs);
	list_splice_init(&chan->desc.done, &descs);
	list_splice_init(&chan->desc.wait, &descs);

	chan->desc.running = NULL;

	spin_unlock_irqrestore(&chan->lock, flags);

	list_for_each_entry_safe(desc, _desc, &descs, node) {
		list_del(&desc->node);
		rcar_dmac_desc_put(chan, desc);
	}
}

static void rcar_dmac_stop_all_chan(struct rcar_dmac *dmac)
{
	struct rcar_dmac_chan *chan;
	unsigned int i;

	 
	for_each_rcar_dmac_chan(i, dmac, chan) {
		 
		spin_lock_irq(&chan->lock);
		rcar_dmac_chan_halt(chan);
		spin_unlock_irq(&chan->lock);
	}
}

static int rcar_dmac_chan_pause(struct dma_chan *chan)
{
	unsigned long flags;
	struct rcar_dmac_chan *rchan = to_rcar_dmac_chan(chan);

	spin_lock_irqsave(&rchan->lock, flags);
	rcar_dmac_clear_chcr_de(rchan);
	spin_unlock_irqrestore(&rchan->lock, flags);

	return 0;
}

 

static void rcar_dmac_chan_configure_desc(struct rcar_dmac_chan *chan,
					  struct rcar_dmac_desc *desc)
{
	static const u32 chcr_ts[] = {
		RCAR_DMACHCR_TS_1B, RCAR_DMACHCR_TS_2B,
		RCAR_DMACHCR_TS_4B, RCAR_DMACHCR_TS_8B,
		RCAR_DMACHCR_TS_16B, RCAR_DMACHCR_TS_32B,
		RCAR_DMACHCR_TS_64B,
	};

	unsigned int xfer_size;
	u32 chcr;

	switch (desc->direction) {
	case DMA_DEV_TO_MEM:
		chcr = RCAR_DMACHCR_DM_INC | RCAR_DMACHCR_SM_FIXED
		     | RCAR_DMACHCR_RS_DMARS;
		xfer_size = chan->src.xfer_size;
		break;

	case DMA_MEM_TO_DEV:
		chcr = RCAR_DMACHCR_DM_FIXED | RCAR_DMACHCR_SM_INC
		     | RCAR_DMACHCR_RS_DMARS;
		xfer_size = chan->dst.xfer_size;
		break;

	case DMA_MEM_TO_MEM:
	default:
		chcr = RCAR_DMACHCR_DM_INC | RCAR_DMACHCR_SM_INC
		     | RCAR_DMACHCR_RS_AUTO;
		xfer_size = RCAR_DMAC_MEMCPY_XFER_SIZE;
		break;
	}

	desc->xfer_shift = ilog2(xfer_size);
	desc->chcr = chcr | chcr_ts[desc->xfer_shift];
}

 
static struct dma_async_tx_descriptor *
rcar_dmac_chan_prep_sg(struct rcar_dmac_chan *chan, struct scatterlist *sgl,
		       unsigned int sg_len, dma_addr_t dev_addr,
		       enum dma_transfer_direction dir, unsigned long dma_flags,
		       bool cyclic)
{
	struct rcar_dmac_xfer_chunk *chunk;
	struct rcar_dmac_desc *desc;
	struct scatterlist *sg;
	unsigned int nchunks = 0;
	unsigned int max_chunk_size;
	unsigned int full_size = 0;
	bool cross_boundary = false;
	unsigned int i;
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	u32 high_dev_addr;
	u32 high_mem_addr;
#endif

	desc = rcar_dmac_desc_get(chan);
	if (!desc)
		return NULL;

	desc->async_tx.flags = dma_flags;
	desc->async_tx.cookie = -EBUSY;

	desc->cyclic = cyclic;
	desc->direction = dir;

	rcar_dmac_chan_configure_desc(chan, desc);

	max_chunk_size = RCAR_DMATCR_MASK << desc->xfer_shift;

	 
	for_each_sg(sgl, sg, sg_len, i) {
		dma_addr_t mem_addr = sg_dma_address(sg);
		unsigned int len = sg_dma_len(sg);

		full_size += len;

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
		if (i == 0) {
			high_dev_addr = dev_addr >> 32;
			high_mem_addr = mem_addr >> 32;
		}

		if ((dev_addr >> 32 != high_dev_addr) ||
		    (mem_addr >> 32 != high_mem_addr))
			cross_boundary = true;
#endif
		while (len) {
			unsigned int size = min(len, max_chunk_size);

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
			 
			if (dev_addr >> 32 != (dev_addr + size - 1) >> 32) {
				size = ALIGN(dev_addr, 1ULL << 32) - dev_addr;
				cross_boundary = true;
			}
			if (mem_addr >> 32 != (mem_addr + size - 1) >> 32) {
				size = ALIGN(mem_addr, 1ULL << 32) - mem_addr;
				cross_boundary = true;
			}
#endif

			chunk = rcar_dmac_xfer_chunk_get(chan);
			if (!chunk) {
				rcar_dmac_desc_put(chan, desc);
				return NULL;
			}

			if (dir == DMA_DEV_TO_MEM) {
				chunk->src_addr = dev_addr;
				chunk->dst_addr = mem_addr;
			} else {
				chunk->src_addr = mem_addr;
				chunk->dst_addr = dev_addr;
			}

			chunk->size = size;

			dev_dbg(chan->chan.device->dev,
				"chan%u: chunk %p/%p sgl %u@%p, %u/%u %pad -> %pad\n",
				chan->index, chunk, desc, i, sg, size, len,
				&chunk->src_addr, &chunk->dst_addr);

			mem_addr += size;
			if (dir == DMA_MEM_TO_MEM)
				dev_addr += size;

			len -= size;

			list_add_tail(&chunk->node, &desc->chunks);
			nchunks++;
		}
	}

	desc->nchunks = nchunks;
	desc->size = full_size;

	 
	desc->hwdescs.use = !cross_boundary && nchunks > 1;
	if (desc->hwdescs.use) {
		if (rcar_dmac_fill_hwdesc(chan, desc) < 0)
			desc->hwdescs.use = false;
	}

	return &desc->async_tx;
}

 

static int rcar_dmac_alloc_chan_resources(struct dma_chan *chan)
{
	struct rcar_dmac_chan *rchan = to_rcar_dmac_chan(chan);
	int ret;

	INIT_LIST_HEAD(&rchan->desc.chunks_free);
	INIT_LIST_HEAD(&rchan->desc.pages);

	 
	ret = rcar_dmac_xfer_chunk_alloc(rchan, GFP_KERNEL);
	if (ret < 0)
		return -ENOMEM;

	ret = rcar_dmac_desc_alloc(rchan, GFP_KERNEL);
	if (ret < 0)
		return -ENOMEM;

	return pm_runtime_get_sync(chan->device->dev);
}

static void rcar_dmac_free_chan_resources(struct dma_chan *chan)
{
	struct rcar_dmac_chan *rchan = to_rcar_dmac_chan(chan);
	struct rcar_dmac *dmac = to_rcar_dmac(chan->device);
	struct rcar_dmac_chan_map *map = &rchan->map;
	struct rcar_dmac_desc_page *page, *_page;
	struct rcar_dmac_desc *desc;
	LIST_HEAD(list);

	 
	spin_lock_irq(&rchan->lock);
	rcar_dmac_chan_halt(rchan);
	spin_unlock_irq(&rchan->lock);

	 
	synchronize_irq(rchan->irq);

	if (rchan->mid_rid >= 0) {
		 
		clear_bit(rchan->mid_rid, dmac->modules);
		rchan->mid_rid = -EINVAL;
	}

	list_splice_init(&rchan->desc.free, &list);
	list_splice_init(&rchan->desc.pending, &list);
	list_splice_init(&rchan->desc.active, &list);
	list_splice_init(&rchan->desc.done, &list);
	list_splice_init(&rchan->desc.wait, &list);

	rchan->desc.running = NULL;

	list_for_each_entry(desc, &list, node)
		rcar_dmac_realloc_hwdesc(rchan, desc, 0);

	list_for_each_entry_safe(page, _page, &rchan->desc.pages, node) {
		list_del(&page->node);
		free_page((unsigned long)page);
	}

	 
	if (map->slave.xfer_size) {
		dma_unmap_resource(chan->device->dev, map->addr,
				   map->slave.xfer_size, map->dir, 0);
		map->slave.xfer_size = 0;
	}

	pm_runtime_put(chan->device->dev);
}

static struct dma_async_tx_descriptor *
rcar_dmac_prep_dma_memcpy(struct dma_chan *chan, dma_addr_t dma_dest,
			  dma_addr_t dma_src, size_t len, unsigned long flags)
{
	struct rcar_dmac_chan *rchan = to_rcar_dmac_chan(chan);
	struct scatterlist sgl;

	if (!len)
		return NULL;

	sg_init_table(&sgl, 1);
	sg_set_page(&sgl, pfn_to_page(PFN_DOWN(dma_src)), len,
		    offset_in_page(dma_src));
	sg_dma_address(&sgl) = dma_src;
	sg_dma_len(&sgl) = len;

	return rcar_dmac_chan_prep_sg(rchan, &sgl, 1, dma_dest,
				      DMA_MEM_TO_MEM, flags, false);
}

static int rcar_dmac_map_slave_addr(struct dma_chan *chan,
				    enum dma_transfer_direction dir)
{
	struct rcar_dmac_chan *rchan = to_rcar_dmac_chan(chan);
	struct rcar_dmac_chan_map *map = &rchan->map;
	phys_addr_t dev_addr;
	size_t dev_size;
	enum dma_data_direction dev_dir;

	if (dir == DMA_DEV_TO_MEM) {
		dev_addr = rchan->src.slave_addr;
		dev_size = rchan->src.xfer_size;
		dev_dir = DMA_TO_DEVICE;
	} else {
		dev_addr = rchan->dst.slave_addr;
		dev_size = rchan->dst.xfer_size;
		dev_dir = DMA_FROM_DEVICE;
	}

	 
	if (dev_addr == map->slave.slave_addr &&
	    dev_size == map->slave.xfer_size &&
	    dev_dir == map->dir)
		return 0;

	 
	if (map->slave.xfer_size)
		dma_unmap_resource(chan->device->dev, map->addr,
				   map->slave.xfer_size, map->dir, 0);
	map->slave.xfer_size = 0;

	 
	map->addr = dma_map_resource(chan->device->dev, dev_addr, dev_size,
				     dev_dir, 0);

	if (dma_mapping_error(chan->device->dev, map->addr)) {
		dev_err(chan->device->dev,
			"chan%u: failed to map %zx@%pap", rchan->index,
			dev_size, &dev_addr);
		return -EIO;
	}

	dev_dbg(chan->device->dev, "chan%u: map %zx@%pap to %pad dir: %s\n",
		rchan->index, dev_size, &dev_addr, &map->addr,
		dev_dir == DMA_TO_DEVICE ? "DMA_TO_DEVICE" : "DMA_FROM_DEVICE");

	map->slave.slave_addr = dev_addr;
	map->slave.xfer_size = dev_size;
	map->dir = dev_dir;

	return 0;
}

static struct dma_async_tx_descriptor *
rcar_dmac_prep_slave_sg(struct dma_chan *chan, struct scatterlist *sgl,
			unsigned int sg_len, enum dma_transfer_direction dir,
			unsigned long flags, void *context)
{
	struct rcar_dmac_chan *rchan = to_rcar_dmac_chan(chan);

	 
	if (rchan->mid_rid < 0 || !sg_len || !sg_dma_len(sgl)) {
		dev_warn(chan->device->dev,
			 "%s: bad parameter: len=%d, id=%d\n",
			 __func__, sg_len, rchan->mid_rid);
		return NULL;
	}

	if (rcar_dmac_map_slave_addr(chan, dir))
		return NULL;

	return rcar_dmac_chan_prep_sg(rchan, sgl, sg_len, rchan->map.addr,
				      dir, flags, false);
}

#define RCAR_DMAC_MAX_SG_LEN	32

static struct dma_async_tx_descriptor *
rcar_dmac_prep_dma_cyclic(struct dma_chan *chan, dma_addr_t buf_addr,
			  size_t buf_len, size_t period_len,
			  enum dma_transfer_direction dir, unsigned long flags)
{
	struct rcar_dmac_chan *rchan = to_rcar_dmac_chan(chan);
	struct dma_async_tx_descriptor *desc;
	struct scatterlist *sgl;
	unsigned int sg_len;
	unsigned int i;

	 
	if (rchan->mid_rid < 0 || buf_len < period_len) {
		dev_warn(chan->device->dev,
			"%s: bad parameter: buf_len=%zu, period_len=%zu, id=%d\n",
			__func__, buf_len, period_len, rchan->mid_rid);
		return NULL;
	}

	if (rcar_dmac_map_slave_addr(chan, dir))
		return NULL;

	sg_len = buf_len / period_len;
	if (sg_len > RCAR_DMAC_MAX_SG_LEN) {
		dev_err(chan->device->dev,
			"chan%u: sg length %d exceeds limit %d",
			rchan->index, sg_len, RCAR_DMAC_MAX_SG_LEN);
		return NULL;
	}

	 
	sgl = kmalloc_array(sg_len, sizeof(*sgl), GFP_NOWAIT);
	if (!sgl)
		return NULL;

	sg_init_table(sgl, sg_len);

	for (i = 0; i < sg_len; ++i) {
		dma_addr_t src = buf_addr + (period_len * i);

		sg_set_page(&sgl[i], pfn_to_page(PFN_DOWN(src)), period_len,
			    offset_in_page(src));
		sg_dma_address(&sgl[i]) = src;
		sg_dma_len(&sgl[i]) = period_len;
	}

	desc = rcar_dmac_chan_prep_sg(rchan, sgl, sg_len, rchan->map.addr,
				      dir, flags, true);

	kfree(sgl);
	return desc;
}

static int rcar_dmac_device_config(struct dma_chan *chan,
				   struct dma_slave_config *cfg)
{
	struct rcar_dmac_chan *rchan = to_rcar_dmac_chan(chan);

	 
	rchan->src.slave_addr = cfg->src_addr;
	rchan->dst.slave_addr = cfg->dst_addr;
	rchan->src.xfer_size = cfg->src_addr_width;
	rchan->dst.xfer_size = cfg->dst_addr_width;

	return 0;
}

static int rcar_dmac_chan_terminate_all(struct dma_chan *chan)
{
	struct rcar_dmac_chan *rchan = to_rcar_dmac_chan(chan);
	unsigned long flags;

	spin_lock_irqsave(&rchan->lock, flags);
	rcar_dmac_chan_halt(rchan);
	spin_unlock_irqrestore(&rchan->lock, flags);

	 

	rcar_dmac_chan_reinit(rchan);

	return 0;
}

static unsigned int rcar_dmac_chan_get_residue(struct rcar_dmac_chan *chan,
					       dma_cookie_t cookie)
{
	struct rcar_dmac_desc *desc = chan->desc.running;
	struct rcar_dmac_xfer_chunk *running = NULL;
	struct rcar_dmac_xfer_chunk *chunk;
	enum dma_status status;
	unsigned int residue = 0;
	unsigned int dptr = 0;
	unsigned int chcrb;
	unsigned int tcrb;
	unsigned int i;

	if (!desc)
		return 0;

	 
	status = dma_cookie_status(&chan->chan, cookie, NULL);
	if (status == DMA_COMPLETE)
		return 0;

	 
	if (cookie != desc->async_tx.cookie) {
		list_for_each_entry(desc, &chan->desc.done, node) {
			if (cookie == desc->async_tx.cookie)
				return 0;
		}
		list_for_each_entry(desc, &chan->desc.pending, node) {
			if (cookie == desc->async_tx.cookie)
				return desc->size;
		}
		list_for_each_entry(desc, &chan->desc.active, node) {
			if (cookie == desc->async_tx.cookie)
				return desc->size;
		}

		 
		WARN(1, "No descriptor for cookie!");
		return 0;
	}

	 
	for (i = 0; i < 3; i++) {
		chcrb = rcar_dmac_chan_read(chan, RCAR_DMACHCRB) &
					    RCAR_DMACHCRB_DPTR_MASK;
		tcrb = rcar_dmac_chan_read(chan, RCAR_DMATCRB);
		 
		if (chcrb == (rcar_dmac_chan_read(chan, RCAR_DMACHCRB) &
			      RCAR_DMACHCRB_DPTR_MASK))
			break;
	}
	WARN_ONCE(i >= 3, "residue might be not continuous!");

	 
	if (desc->hwdescs.use) {
		dptr = chcrb >> RCAR_DMACHCRB_DPTR_SHIFT;
		if (dptr == 0)
			dptr = desc->nchunks;
		dptr--;
		WARN_ON(dptr >= desc->nchunks);
	} else {
		running = desc->running;
	}

	 
	list_for_each_entry_reverse(chunk, &desc->chunks, node) {
		if (chunk == running || ++dptr == desc->nchunks)
			break;

		residue += chunk->size;
	}

	 
	residue += tcrb << desc->xfer_shift;

	return residue;
}

static enum dma_status rcar_dmac_tx_status(struct dma_chan *chan,
					   dma_cookie_t cookie,
					   struct dma_tx_state *txstate)
{
	struct rcar_dmac_chan *rchan = to_rcar_dmac_chan(chan);
	enum dma_status status;
	unsigned long flags;
	unsigned int residue;
	bool cyclic;

	status = dma_cookie_status(chan, cookie, txstate);
	if (status == DMA_COMPLETE || !txstate)
		return status;

	spin_lock_irqsave(&rchan->lock, flags);
	residue = rcar_dmac_chan_get_residue(rchan, cookie);
	cyclic = rchan->desc.running ? rchan->desc.running->cyclic : false;
	spin_unlock_irqrestore(&rchan->lock, flags);

	 
	if (!residue && !cyclic)
		return DMA_COMPLETE;

	dma_set_residue(txstate, residue);

	return status;
}

static void rcar_dmac_issue_pending(struct dma_chan *chan)
{
	struct rcar_dmac_chan *rchan = to_rcar_dmac_chan(chan);
	unsigned long flags;

	spin_lock_irqsave(&rchan->lock, flags);

	if (list_empty(&rchan->desc.pending))
		goto done;

	 
	list_splice_tail_init(&rchan->desc.pending, &rchan->desc.active);

	 
	if (!rchan->desc.running) {
		struct rcar_dmac_desc *desc;

		desc = list_first_entry(&rchan->desc.active,
					struct rcar_dmac_desc, node);
		rchan->desc.running = desc;

		rcar_dmac_chan_start_xfer(rchan);
	}

done:
	spin_unlock_irqrestore(&rchan->lock, flags);
}

static void rcar_dmac_device_synchronize(struct dma_chan *chan)
{
	struct rcar_dmac_chan *rchan = to_rcar_dmac_chan(chan);

	synchronize_irq(rchan->irq);
}

 

static irqreturn_t rcar_dmac_isr_desc_stage_end(struct rcar_dmac_chan *chan)
{
	struct rcar_dmac_desc *desc = chan->desc.running;
	unsigned int stage;

	if (WARN_ON(!desc || !desc->cyclic)) {
		 
		return IRQ_NONE;
	}

	 
	stage = (rcar_dmac_chan_read(chan, RCAR_DMACHCRB) &
		 RCAR_DMACHCRB_DPTR_MASK) >> RCAR_DMACHCRB_DPTR_SHIFT;
	rcar_dmac_chan_write(chan, RCAR_DMADPCR, RCAR_DMADPCR_DIPT(stage));

	return IRQ_WAKE_THREAD;
}

static irqreturn_t rcar_dmac_isr_transfer_end(struct rcar_dmac_chan *chan)
{
	struct rcar_dmac_desc *desc = chan->desc.running;
	irqreturn_t ret = IRQ_WAKE_THREAD;

	if (WARN_ON_ONCE(!desc)) {
		 
		return IRQ_NONE;
	}

	 
	if (!desc->hwdescs.use) {
		 
		if (!list_is_last(&desc->running->node, &desc->chunks)) {
			desc->running = list_next_entry(desc->running, node);
			if (!desc->cyclic)
				ret = IRQ_HANDLED;
			goto done;
		}

		 
		if (desc->cyclic) {
			desc->running =
				list_first_entry(&desc->chunks,
						 struct rcar_dmac_xfer_chunk,
						 node);
			goto done;
		}
	}

	 
	list_move_tail(&desc->node, &chan->desc.done);

	 
	if (!list_empty(&chan->desc.active))
		chan->desc.running = list_first_entry(&chan->desc.active,
						      struct rcar_dmac_desc,
						      node);
	else
		chan->desc.running = NULL;

done:
	if (chan->desc.running)
		rcar_dmac_chan_start_xfer(chan);

	return ret;
}

static irqreturn_t rcar_dmac_isr_channel(int irq, void *dev)
{
	u32 mask = RCAR_DMACHCR_DSE | RCAR_DMACHCR_TE;
	struct rcar_dmac_chan *chan = dev;
	irqreturn_t ret = IRQ_NONE;
	bool reinit = false;
	u32 chcr;

	spin_lock(&chan->lock);

	chcr = rcar_dmac_chan_read(chan, RCAR_DMACHCR);
	if (chcr & RCAR_DMACHCR_CAE) {
		struct rcar_dmac *dmac = to_rcar_dmac(chan->chan.device);

		 
		rcar_dmac_chan_clear(dmac, chan);
		rcar_dmac_chcr_de_barrier(chan);
		reinit = true;
		goto spin_lock_end;
	}

	if (chcr & RCAR_DMACHCR_TE)
		mask |= RCAR_DMACHCR_DE;
	rcar_dmac_chan_write(chan, RCAR_DMACHCR, chcr & ~mask);
	if (mask & RCAR_DMACHCR_DE)
		rcar_dmac_chcr_de_barrier(chan);

	if (chcr & RCAR_DMACHCR_DSE)
		ret |= rcar_dmac_isr_desc_stage_end(chan);

	if (chcr & RCAR_DMACHCR_TE)
		ret |= rcar_dmac_isr_transfer_end(chan);

spin_lock_end:
	spin_unlock(&chan->lock);

	if (reinit) {
		dev_err(chan->chan.device->dev, "Channel Address Error\n");

		rcar_dmac_chan_reinit(chan);
		ret = IRQ_HANDLED;
	}

	return ret;
}

static irqreturn_t rcar_dmac_isr_channel_thread(int irq, void *dev)
{
	struct rcar_dmac_chan *chan = dev;
	struct rcar_dmac_desc *desc;
	struct dmaengine_desc_callback cb;

	spin_lock_irq(&chan->lock);

	 
	if (chan->desc.running && chan->desc.running->cyclic) {
		desc = chan->desc.running;
		dmaengine_desc_get_callback(&desc->async_tx, &cb);

		if (dmaengine_desc_callback_valid(&cb)) {
			spin_unlock_irq(&chan->lock);
			dmaengine_desc_callback_invoke(&cb, NULL);
			spin_lock_irq(&chan->lock);
		}
	}

	 
	while (!list_empty(&chan->desc.done)) {
		desc = list_first_entry(&chan->desc.done, struct rcar_dmac_desc,
					node);
		dma_cookie_complete(&desc->async_tx);
		list_del(&desc->node);

		dmaengine_desc_get_callback(&desc->async_tx, &cb);
		if (dmaengine_desc_callback_valid(&cb)) {
			spin_unlock_irq(&chan->lock);
			 
			dmaengine_desc_callback_invoke(&cb, NULL);
			spin_lock_irq(&chan->lock);
		}

		list_add_tail(&desc->node, &chan->desc.wait);
	}

	spin_unlock_irq(&chan->lock);

	 
	rcar_dmac_desc_recycle_acked(chan);

	return IRQ_HANDLED;
}

 

static bool rcar_dmac_chan_filter(struct dma_chan *chan, void *arg)
{
	struct rcar_dmac *dmac = to_rcar_dmac(chan->device);
	struct of_phandle_args *dma_spec = arg;

	 
	if (chan->device->device_config != rcar_dmac_device_config)
		return false;

	return !test_and_set_bit(dma_spec->args[0], dmac->modules);
}

static struct dma_chan *rcar_dmac_of_xlate(struct of_phandle_args *dma_spec,
					   struct of_dma *ofdma)
{
	struct rcar_dmac_chan *rchan;
	struct dma_chan *chan;
	dma_cap_mask_t mask;

	if (dma_spec->args_count != 1)
		return NULL;

	 
	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	chan = __dma_request_channel(&mask, rcar_dmac_chan_filter, dma_spec,
				     ofdma->of_node);
	if (!chan)
		return NULL;

	rchan = to_rcar_dmac_chan(chan);
	rchan->mid_rid = dma_spec->args[0];

	return chan;
}

 

#ifdef CONFIG_PM
static int rcar_dmac_runtime_suspend(struct device *dev)
{
	return 0;
}

static int rcar_dmac_runtime_resume(struct device *dev)
{
	struct rcar_dmac *dmac = dev_get_drvdata(dev);

	return rcar_dmac_init(dmac);
}
#endif

static const struct dev_pm_ops rcar_dmac_pm = {
	 
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				      pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(rcar_dmac_runtime_suspend, rcar_dmac_runtime_resume,
			   NULL)
};

 

static int rcar_dmac_chan_probe(struct rcar_dmac *dmac,
				struct rcar_dmac_chan *rchan)
{
	struct platform_device *pdev = to_platform_device(dmac->dev);
	struct dma_chan *chan = &rchan->chan;
	char pdev_irqname[5];
	char *irqname;
	int ret;

	rchan->mid_rid = -EINVAL;

	spin_lock_init(&rchan->lock);

	INIT_LIST_HEAD(&rchan->desc.free);
	INIT_LIST_HEAD(&rchan->desc.pending);
	INIT_LIST_HEAD(&rchan->desc.active);
	INIT_LIST_HEAD(&rchan->desc.done);
	INIT_LIST_HEAD(&rchan->desc.wait);

	 
	sprintf(pdev_irqname, "ch%u", rchan->index);
	rchan->irq = platform_get_irq_byname(pdev, pdev_irqname);
	if (rchan->irq < 0)
		return -ENODEV;

	irqname = devm_kasprintf(dmac->dev, GFP_KERNEL, "%s:%u",
				 dev_name(dmac->dev), rchan->index);
	if (!irqname)
		return -ENOMEM;

	 
	chan->device = &dmac->engine;
	dma_cookie_init(chan);

	list_add_tail(&chan->device_node, &dmac->engine.channels);

	ret = devm_request_threaded_irq(dmac->dev, rchan->irq,
					rcar_dmac_isr_channel,
					rcar_dmac_isr_channel_thread, 0,
					irqname, rchan);
	if (ret) {
		dev_err(dmac->dev, "failed to request IRQ %u (%d)\n",
			rchan->irq, ret);
		return ret;
	}

	return 0;
}

#define RCAR_DMAC_MAX_CHANNELS	32

static int rcar_dmac_parse_of(struct device *dev, struct rcar_dmac *dmac)
{
	struct device_node *np = dev->of_node;
	int ret;

	ret = of_property_read_u32(np, "dma-channels", &dmac->n_channels);
	if (ret < 0) {
		dev_err(dev, "unable to read dma-channels property\n");
		return ret;
	}

	 
	if (dmac->n_channels <= 0 ||
	    dmac->n_channels >= RCAR_DMAC_MAX_CHANNELS) {
		dev_err(dev, "invalid number of channels %u\n",
			dmac->n_channels);
		return -EINVAL;
	}

	 
	dmac->channels_mask = GENMASK(dmac->n_channels - 1, 0);
	of_property_read_u32(np, "dma-channel-mask", &dmac->channels_mask);

	 
	dmac->channels_mask &= GENMASK(dmac->n_channels - 1, 0);

	return 0;
}

static int rcar_dmac_probe(struct platform_device *pdev)
{
	const enum dma_slave_buswidth widths = DMA_SLAVE_BUSWIDTH_1_BYTE |
		DMA_SLAVE_BUSWIDTH_2_BYTES | DMA_SLAVE_BUSWIDTH_4_BYTES |
		DMA_SLAVE_BUSWIDTH_8_BYTES | DMA_SLAVE_BUSWIDTH_16_BYTES |
		DMA_SLAVE_BUSWIDTH_32_BYTES | DMA_SLAVE_BUSWIDTH_64_BYTES;
	const struct rcar_dmac_of_data *data;
	struct rcar_dmac_chan *chan;
	struct dma_device *engine;
	void __iomem *chan_base;
	struct rcar_dmac *dmac;
	unsigned int i;
	int ret;

	data = of_device_get_match_data(&pdev->dev);
	if (!data)
		return -EINVAL;

	dmac = devm_kzalloc(&pdev->dev, sizeof(*dmac), GFP_KERNEL);
	if (!dmac)
		return -ENOMEM;

	dmac->dev = &pdev->dev;
	platform_set_drvdata(pdev, dmac);
	ret = dma_set_max_seg_size(dmac->dev, RCAR_DMATCR_MASK);
	if (ret)
		return ret;

	ret = dma_set_mask_and_coherent(dmac->dev, DMA_BIT_MASK(40));
	if (ret)
		return ret;

	ret = rcar_dmac_parse_of(&pdev->dev, dmac);
	if (ret < 0)
		return ret;

	 
	if (device_iommu_mapped(&pdev->dev))
		dmac->channels_mask &= ~BIT(0);

	dmac->channels = devm_kcalloc(&pdev->dev, dmac->n_channels,
				      sizeof(*dmac->channels), GFP_KERNEL);
	if (!dmac->channels)
		return -ENOMEM;

	 
	dmac->dmac_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(dmac->dmac_base))
		return PTR_ERR(dmac->dmac_base);

	if (!data->chan_offset_base) {
		dmac->chan_base = devm_platform_ioremap_resource(pdev, 1);
		if (IS_ERR(dmac->chan_base))
			return PTR_ERR(dmac->chan_base);

		chan_base = dmac->chan_base;
	} else {
		chan_base = dmac->dmac_base + data->chan_offset_base;
	}

	for_each_rcar_dmac_chan(i, dmac, chan) {
		chan->index = i;
		chan->iomem = chan_base + i * data->chan_offset_stride;
	}

	 
	pm_runtime_enable(&pdev->dev);
	ret = pm_runtime_resume_and_get(&pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "runtime PM get sync failed (%d)\n", ret);
		goto err_pm_disable;
	}

	ret = rcar_dmac_init(dmac);
	pm_runtime_put(&pdev->dev);

	if (ret) {
		dev_err(&pdev->dev, "failed to reset device\n");
		goto err_pm_disable;
	}

	 
	engine = &dmac->engine;

	dma_cap_set(DMA_MEMCPY, engine->cap_mask);
	dma_cap_set(DMA_SLAVE, engine->cap_mask);

	engine->dev		= &pdev->dev;
	engine->copy_align	= ilog2(RCAR_DMAC_MEMCPY_XFER_SIZE);

	engine->src_addr_widths	= widths;
	engine->dst_addr_widths	= widths;
	engine->directions	= BIT(DMA_MEM_TO_DEV) | BIT(DMA_DEV_TO_MEM);
	engine->residue_granularity = DMA_RESIDUE_GRANULARITY_BURST;

	engine->device_alloc_chan_resources	= rcar_dmac_alloc_chan_resources;
	engine->device_free_chan_resources	= rcar_dmac_free_chan_resources;
	engine->device_prep_dma_memcpy		= rcar_dmac_prep_dma_memcpy;
	engine->device_prep_slave_sg		= rcar_dmac_prep_slave_sg;
	engine->device_prep_dma_cyclic		= rcar_dmac_prep_dma_cyclic;
	engine->device_config			= rcar_dmac_device_config;
	engine->device_pause			= rcar_dmac_chan_pause;
	engine->device_terminate_all		= rcar_dmac_chan_terminate_all;
	engine->device_tx_status		= rcar_dmac_tx_status;
	engine->device_issue_pending		= rcar_dmac_issue_pending;
	engine->device_synchronize		= rcar_dmac_device_synchronize;

	INIT_LIST_HEAD(&engine->channels);

	for_each_rcar_dmac_chan(i, dmac, chan) {
		ret = rcar_dmac_chan_probe(dmac, chan);
		if (ret < 0)
			goto err_pm_disable;
	}

	 
	ret = of_dma_controller_register(pdev->dev.of_node, rcar_dmac_of_xlate,
					 NULL);
	if (ret < 0)
		goto err_pm_disable;

	 
	ret = dma_async_device_register(engine);
	if (ret < 0)
		goto err_dma_free;

	return 0;

err_dma_free:
	of_dma_controller_free(pdev->dev.of_node);
err_pm_disable:
	pm_runtime_disable(&pdev->dev);
	return ret;
}

static int rcar_dmac_remove(struct platform_device *pdev)
{
	struct rcar_dmac *dmac = platform_get_drvdata(pdev);

	of_dma_controller_free(pdev->dev.of_node);
	dma_async_device_unregister(&dmac->engine);

	pm_runtime_disable(&pdev->dev);

	return 0;
}

static void rcar_dmac_shutdown(struct platform_device *pdev)
{
	struct rcar_dmac *dmac = platform_get_drvdata(pdev);

	rcar_dmac_stop_all_chan(dmac);
}

static const struct rcar_dmac_of_data rcar_dmac_data = {
	.chan_offset_base	= 0x8000,
	.chan_offset_stride	= 0x80,
};

static const struct rcar_dmac_of_data rcar_gen4_dmac_data = {
	.chan_offset_base	= 0x0,
	.chan_offset_stride	= 0x1000,
};

static const struct of_device_id rcar_dmac_of_ids[] = {
	{
		.compatible = "renesas,rcar-dmac",
		.data = &rcar_dmac_data,
	}, {
		.compatible = "renesas,rcar-gen4-dmac",
		.data = &rcar_gen4_dmac_data,
	}, {
		.compatible = "renesas,dmac-r8a779a0",
		.data = &rcar_gen4_dmac_data,
	},
	{   }
};
MODULE_DEVICE_TABLE(of, rcar_dmac_of_ids);

static struct platform_driver rcar_dmac_driver = {
	.driver		= {
		.pm	= &rcar_dmac_pm,
		.name	= "rcar-dmac",
		.of_match_table = rcar_dmac_of_ids,
	},
	.probe		= rcar_dmac_probe,
	.remove		= rcar_dmac_remove,
	.shutdown	= rcar_dmac_shutdown,
};

module_platform_driver(rcar_dmac_driver);

MODULE_DESCRIPTION("R-Car Gen2 DMA Controller Driver");
MODULE_AUTHOR("Laurent Pinchart <laurent.pinchart@ideasonboard.com>");
MODULE_LICENSE("GPL v2");
