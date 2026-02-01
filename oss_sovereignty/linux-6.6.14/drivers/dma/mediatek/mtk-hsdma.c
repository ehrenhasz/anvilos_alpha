


 

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_dma.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/refcount.h>
#include <linux/slab.h>

#include "../virt-dma.h"

#define MTK_HSDMA_USEC_POLL		20
#define MTK_HSDMA_TIMEOUT_POLL		200000
#define MTK_HSDMA_DMA_BUSWIDTHS		BIT(DMA_SLAVE_BUSWIDTH_4_BYTES)

 
#define MTK_HSDMA_NR_VCHANS		3

 
#define MTK_HSDMA_NR_MAX_PCHANS		1

 
 
#define MTK_DMA_SIZE			64
#define MTK_HSDMA_NEXT_DESP_IDX(x, y)	(((x) + 1) & ((y) - 1))
#define MTK_HSDMA_LAST_DESP_IDX(x, y)	(((x) - 1) & ((y) - 1))
#define MTK_HSDMA_MAX_LEN		0x3f80
#define MTK_HSDMA_ALIGN_SIZE		4
#define MTK_HSDMA_PLEN_MASK		0x3fff
#define MTK_HSDMA_DESC_PLEN(x)		(((x) & MTK_HSDMA_PLEN_MASK) << 16)
#define MTK_HSDMA_DESC_PLEN_GET(x)	(((x) >> 16) & MTK_HSDMA_PLEN_MASK)

 
#define MTK_HSDMA_TX_BASE		0x0
#define MTK_HSDMA_TX_CNT		0x4
#define MTK_HSDMA_TX_CPU		0x8
#define MTK_HSDMA_TX_DMA		0xc
#define MTK_HSDMA_RX_BASE		0x100
#define MTK_HSDMA_RX_CNT		0x104
#define MTK_HSDMA_RX_CPU		0x108
#define MTK_HSDMA_RX_DMA		0x10c

 
#define MTK_HSDMA_GLO			0x204
#define MTK_HSDMA_GLO_MULTI_DMA		BIT(10)
#define MTK_HSDMA_TX_WB_DDONE		BIT(6)
#define MTK_HSDMA_BURST_64BYTES		(0x2 << 4)
#define MTK_HSDMA_GLO_RX_BUSY		BIT(3)
#define MTK_HSDMA_GLO_RX_DMA		BIT(2)
#define MTK_HSDMA_GLO_TX_BUSY		BIT(1)
#define MTK_HSDMA_GLO_TX_DMA		BIT(0)
#define MTK_HSDMA_GLO_DMA		(MTK_HSDMA_GLO_TX_DMA |	\
					 MTK_HSDMA_GLO_RX_DMA)
#define MTK_HSDMA_GLO_BUSY		(MTK_HSDMA_GLO_RX_BUSY | \
					 MTK_HSDMA_GLO_TX_BUSY)
#define MTK_HSDMA_GLO_DEFAULT		(MTK_HSDMA_GLO_TX_DMA | \
					 MTK_HSDMA_GLO_RX_DMA | \
					 MTK_HSDMA_TX_WB_DDONE | \
					 MTK_HSDMA_BURST_64BYTES | \
					 MTK_HSDMA_GLO_MULTI_DMA)

 
#define MTK_HSDMA_RESET			0x208
#define MTK_HSDMA_RST_TX		BIT(0)
#define MTK_HSDMA_RST_RX		BIT(16)

 
#define MTK_HSDMA_DLYINT		0x20c
#define MTK_HSDMA_RXDLY_INT_EN		BIT(15)

 
#define MTK_HSDMA_RXMAX_PINT(x)		(((x) & 0x7f) << 8)

 
#define MTK_HSDMA_RXMAX_PTIME(x)	((x) & 0x7f)
#define MTK_HSDMA_DLYINT_DEFAULT	(MTK_HSDMA_RXDLY_INT_EN | \
					 MTK_HSDMA_RXMAX_PINT(20) | \
					 MTK_HSDMA_RXMAX_PTIME(20))
#define MTK_HSDMA_INT_STATUS		0x220
#define MTK_HSDMA_INT_ENABLE		0x228
#define MTK_HSDMA_INT_RXDONE		BIT(16)

enum mtk_hsdma_vdesc_flag {
	MTK_HSDMA_VDESC_FINISHED	= 0x01,
};

#define IS_MTK_HSDMA_VDESC_FINISHED(x) ((x) == MTK_HSDMA_VDESC_FINISHED)

 
struct mtk_hsdma_pdesc {
	__le32 desc1;
	__le32 desc2;
	__le32 desc3;
	__le32 desc4;
} __packed __aligned(4);

 
struct mtk_hsdma_vdesc {
	struct virt_dma_desc vd;
	size_t len;
	size_t residue;
	dma_addr_t dest;
	dma_addr_t src;
};

 
struct mtk_hsdma_cb {
	struct virt_dma_desc *vd;
	enum mtk_hsdma_vdesc_flag flag;
};

 
struct mtk_hsdma_ring {
	struct mtk_hsdma_pdesc *txd;
	struct mtk_hsdma_pdesc *rxd;
	struct mtk_hsdma_cb *cb;
	dma_addr_t tphys;
	dma_addr_t rphys;
	u16 cur_tptr;
	u16 cur_rptr;
};

 
struct mtk_hsdma_pchan {
	struct mtk_hsdma_ring ring;
	size_t sz_ring;
	atomic_t nr_free;
};

 
struct mtk_hsdma_vchan {
	struct virt_dma_chan vc;
	struct completion issue_completion;
	bool issue_synchronize;
	struct list_head desc_hw_processing;
};

 
struct mtk_hsdma_soc {
	__le32 ddone;
	__le32 ls0;
};

 
struct mtk_hsdma_device {
	struct dma_device ddev;
	void __iomem *base;
	struct clk *clk;
	u32 irq;

	u32 dma_requests;
	struct mtk_hsdma_vchan *vc;
	struct mtk_hsdma_pchan *pc;
	refcount_t pc_refcnt;

	 
	spinlock_t lock;

	const struct mtk_hsdma_soc *soc;
};

static struct mtk_hsdma_device *to_hsdma_dev(struct dma_chan *chan)
{
	return container_of(chan->device, struct mtk_hsdma_device, ddev);
}

static inline struct mtk_hsdma_vchan *to_hsdma_vchan(struct dma_chan *chan)
{
	return container_of(chan, struct mtk_hsdma_vchan, vc.chan);
}

static struct mtk_hsdma_vdesc *to_hsdma_vdesc(struct virt_dma_desc *vd)
{
	return container_of(vd, struct mtk_hsdma_vdesc, vd);
}

static struct device *hsdma2dev(struct mtk_hsdma_device *hsdma)
{
	return hsdma->ddev.dev;
}

static u32 mtk_dma_read(struct mtk_hsdma_device *hsdma, u32 reg)
{
	return readl(hsdma->base + reg);
}

static void mtk_dma_write(struct mtk_hsdma_device *hsdma, u32 reg, u32 val)
{
	writel(val, hsdma->base + reg);
}

static void mtk_dma_rmw(struct mtk_hsdma_device *hsdma, u32 reg,
			u32 mask, u32 set)
{
	u32 val;

	val = mtk_dma_read(hsdma, reg);
	val &= ~mask;
	val |= set;
	mtk_dma_write(hsdma, reg, val);
}

static void mtk_dma_set(struct mtk_hsdma_device *hsdma, u32 reg, u32 val)
{
	mtk_dma_rmw(hsdma, reg, 0, val);
}

static void mtk_dma_clr(struct mtk_hsdma_device *hsdma, u32 reg, u32 val)
{
	mtk_dma_rmw(hsdma, reg, val, 0);
}

static void mtk_hsdma_vdesc_free(struct virt_dma_desc *vd)
{
	kfree(container_of(vd, struct mtk_hsdma_vdesc, vd));
}

static int mtk_hsdma_busy_wait(struct mtk_hsdma_device *hsdma)
{
	u32 status = 0;

	return readl_poll_timeout(hsdma->base + MTK_HSDMA_GLO, status,
				  !(status & MTK_HSDMA_GLO_BUSY),
				  MTK_HSDMA_USEC_POLL,
				  MTK_HSDMA_TIMEOUT_POLL);
}

static int mtk_hsdma_alloc_pchan(struct mtk_hsdma_device *hsdma,
				 struct mtk_hsdma_pchan *pc)
{
	struct mtk_hsdma_ring *ring = &pc->ring;
	int err;

	memset(pc, 0, sizeof(*pc));

	 
	pc->sz_ring = 2 * MTK_DMA_SIZE * sizeof(*ring->txd);
	ring->txd = dma_alloc_coherent(hsdma2dev(hsdma), pc->sz_ring,
				       &ring->tphys, GFP_NOWAIT);
	if (!ring->txd)
		return -ENOMEM;

	ring->rxd = &ring->txd[MTK_DMA_SIZE];
	ring->rphys = ring->tphys + MTK_DMA_SIZE * sizeof(*ring->txd);
	ring->cur_tptr = 0;
	ring->cur_rptr = MTK_DMA_SIZE - 1;

	ring->cb = kcalloc(MTK_DMA_SIZE, sizeof(*ring->cb), GFP_NOWAIT);
	if (!ring->cb) {
		err = -ENOMEM;
		goto err_free_dma;
	}

	atomic_set(&pc->nr_free, MTK_DMA_SIZE - 1);

	 
	mtk_dma_clr(hsdma, MTK_HSDMA_GLO, MTK_HSDMA_GLO_DMA);
	err = mtk_hsdma_busy_wait(hsdma);
	if (err)
		goto err_free_cb;

	 
	mtk_dma_set(hsdma, MTK_HSDMA_RESET,
		    MTK_HSDMA_RST_TX | MTK_HSDMA_RST_RX);
	mtk_dma_clr(hsdma, MTK_HSDMA_RESET,
		    MTK_HSDMA_RST_TX | MTK_HSDMA_RST_RX);

	 
	mtk_dma_write(hsdma, MTK_HSDMA_TX_BASE, ring->tphys);
	mtk_dma_write(hsdma, MTK_HSDMA_TX_CNT, MTK_DMA_SIZE);
	mtk_dma_write(hsdma, MTK_HSDMA_TX_CPU, ring->cur_tptr);
	mtk_dma_write(hsdma, MTK_HSDMA_TX_DMA, 0);
	mtk_dma_write(hsdma, MTK_HSDMA_RX_BASE, ring->rphys);
	mtk_dma_write(hsdma, MTK_HSDMA_RX_CNT, MTK_DMA_SIZE);
	mtk_dma_write(hsdma, MTK_HSDMA_RX_CPU, ring->cur_rptr);
	mtk_dma_write(hsdma, MTK_HSDMA_RX_DMA, 0);

	 
	mtk_dma_set(hsdma, MTK_HSDMA_GLO, MTK_HSDMA_GLO_DMA);

	 
	mtk_dma_write(hsdma, MTK_HSDMA_DLYINT, MTK_HSDMA_DLYINT_DEFAULT);

	 
	mtk_dma_set(hsdma, MTK_HSDMA_INT_ENABLE, MTK_HSDMA_INT_RXDONE);

	return 0;

err_free_cb:
	kfree(ring->cb);

err_free_dma:
	dma_free_coherent(hsdma2dev(hsdma),
			  pc->sz_ring, ring->txd, ring->tphys);
	return err;
}

static void mtk_hsdma_free_pchan(struct mtk_hsdma_device *hsdma,
				 struct mtk_hsdma_pchan *pc)
{
	struct mtk_hsdma_ring *ring = &pc->ring;

	 
	mtk_dma_clr(hsdma, MTK_HSDMA_GLO, MTK_HSDMA_GLO_DMA);
	mtk_hsdma_busy_wait(hsdma);

	 
	mtk_dma_clr(hsdma, MTK_HSDMA_INT_ENABLE, MTK_HSDMA_INT_RXDONE);
	mtk_dma_write(hsdma, MTK_HSDMA_TX_BASE, 0);
	mtk_dma_write(hsdma, MTK_HSDMA_TX_CNT, 0);
	mtk_dma_write(hsdma, MTK_HSDMA_TX_CPU, 0);
	mtk_dma_write(hsdma, MTK_HSDMA_RX_BASE, 0);
	mtk_dma_write(hsdma, MTK_HSDMA_RX_CNT, 0);
	mtk_dma_write(hsdma, MTK_HSDMA_RX_CPU, MTK_DMA_SIZE - 1);

	kfree(ring->cb);

	dma_free_coherent(hsdma2dev(hsdma),
			  pc->sz_ring, ring->txd, ring->tphys);
}

static int mtk_hsdma_issue_pending_vdesc(struct mtk_hsdma_device *hsdma,
					 struct mtk_hsdma_pchan *pc,
					 struct mtk_hsdma_vdesc *hvd)
{
	struct mtk_hsdma_ring *ring = &pc->ring;
	struct mtk_hsdma_pdesc *txd, *rxd;
	u16 reserved, prev, tlen, num_sgs;
	unsigned long flags;

	 
	spin_lock_irqsave(&hsdma->lock, flags);

	 
	num_sgs = DIV_ROUND_UP(hvd->len, MTK_HSDMA_MAX_LEN);
	reserved = min_t(u16, num_sgs, atomic_read(&pc->nr_free));

	if (!reserved) {
		spin_unlock_irqrestore(&hsdma->lock, flags);
		return -ENOSPC;
	}

	atomic_sub(reserved, &pc->nr_free);

	while (reserved--) {
		 
		tlen = (hvd->len > MTK_HSDMA_MAX_LEN) ?
		       MTK_HSDMA_MAX_LEN : hvd->len;

		 
		txd = &ring->txd[ring->cur_tptr];
		WRITE_ONCE(txd->desc1, hvd->src);
		WRITE_ONCE(txd->desc2,
			   hsdma->soc->ls0 | MTK_HSDMA_DESC_PLEN(tlen));

		rxd = &ring->rxd[ring->cur_tptr];
		WRITE_ONCE(rxd->desc1, hvd->dest);
		WRITE_ONCE(rxd->desc2, MTK_HSDMA_DESC_PLEN(tlen));

		 
		ring->cb[ring->cur_tptr].vd = &hvd->vd;

		 
		ring->cur_tptr = MTK_HSDMA_NEXT_DESP_IDX(ring->cur_tptr,
							 MTK_DMA_SIZE);

		 
		hvd->src  += tlen;
		hvd->dest += tlen;
		hvd->len  -= tlen;
	}

	 
	if (!hvd->len) {
		prev = MTK_HSDMA_LAST_DESP_IDX(ring->cur_tptr, MTK_DMA_SIZE);
		ring->cb[prev].flag = MTK_HSDMA_VDESC_FINISHED;
	}

	 
	wmb();

	 
	mtk_dma_write(hsdma, MTK_HSDMA_TX_CPU, ring->cur_tptr);

	spin_unlock_irqrestore(&hsdma->lock, flags);

	return 0;
}

static void mtk_hsdma_issue_vchan_pending(struct mtk_hsdma_device *hsdma,
					  struct mtk_hsdma_vchan *hvc)
{
	struct virt_dma_desc *vd, *vd2;
	int err;

	lockdep_assert_held(&hvc->vc.lock);

	list_for_each_entry_safe(vd, vd2, &hvc->vc.desc_issued, node) {
		struct mtk_hsdma_vdesc *hvd;

		hvd = to_hsdma_vdesc(vd);

		 
		err = mtk_hsdma_issue_pending_vdesc(hsdma, hsdma->pc, hvd);

		 
		if (err == -ENOSPC || hvd->len > 0)
			break;

		 
		list_move_tail(&vd->node, &hvc->desc_hw_processing);
	}
}

static void mtk_hsdma_free_rooms_in_ring(struct mtk_hsdma_device *hsdma)
{
	struct mtk_hsdma_vchan *hvc;
	struct mtk_hsdma_pdesc *rxd;
	struct mtk_hsdma_vdesc *hvd;
	struct mtk_hsdma_pchan *pc;
	struct mtk_hsdma_cb *cb;
	int i = MTK_DMA_SIZE;
	__le32 desc2;
	u32 status;
	u16 next;

	 
	status = mtk_dma_read(hsdma, MTK_HSDMA_INT_STATUS);
	if (unlikely(!(status & MTK_HSDMA_INT_RXDONE)))
		goto rx_done;

	pc = hsdma->pc;

	 
	while (i--) {
		next = MTK_HSDMA_NEXT_DESP_IDX(pc->ring.cur_rptr,
					       MTK_DMA_SIZE);
		rxd = &pc->ring.rxd[next];

		 
		desc2 = READ_ONCE(rxd->desc2);
		if (!(desc2 & hsdma->soc->ddone))
			break;

		cb = &pc->ring.cb[next];
		if (unlikely(!cb->vd)) {
			dev_err(hsdma2dev(hsdma), "cb->vd cannot be null\n");
			break;
		}

		 
		hvd = to_hsdma_vdesc(cb->vd);
		hvd->residue -= MTK_HSDMA_DESC_PLEN_GET(rxd->desc2);

		 
		if (IS_MTK_HSDMA_VDESC_FINISHED(cb->flag)) {
			hvc = to_hsdma_vchan(cb->vd->tx.chan);

			spin_lock(&hvc->vc.lock);

			 
			list_del(&cb->vd->node);

			 
			vchan_cookie_complete(cb->vd);

			if (hvc->issue_synchronize &&
			    list_empty(&hvc->desc_hw_processing)) {
				complete(&hvc->issue_completion);
				hvc->issue_synchronize = false;
			}
			spin_unlock(&hvc->vc.lock);

			cb->flag = 0;
		}

		cb->vd = NULL;

		 
		WRITE_ONCE(rxd->desc1, 0);
		WRITE_ONCE(rxd->desc2, 0);
		pc->ring.cur_rptr = next;

		 
		atomic_inc(&pc->nr_free);
	}

	 
	wmb();

	 
	mtk_dma_write(hsdma, MTK_HSDMA_RX_CPU, pc->ring.cur_rptr);

	 
	if (atomic_read(&pc->nr_free) >= MTK_DMA_SIZE - 1)
		mtk_dma_write(hsdma, MTK_HSDMA_INT_STATUS, status);

	 
	for (i = 0; i < hsdma->dma_requests; i++) {
		hvc = &hsdma->vc[i];
		spin_lock(&hvc->vc.lock);
		mtk_hsdma_issue_vchan_pending(hsdma, hvc);
		spin_unlock(&hvc->vc.lock);
	}

rx_done:
	 
	mtk_dma_set(hsdma, MTK_HSDMA_INT_ENABLE, MTK_HSDMA_INT_RXDONE);
}

static irqreturn_t mtk_hsdma_irq(int irq, void *devid)
{
	struct mtk_hsdma_device *hsdma = devid;

	 
	mtk_dma_clr(hsdma, MTK_HSDMA_INT_ENABLE, MTK_HSDMA_INT_RXDONE);

	mtk_hsdma_free_rooms_in_ring(hsdma);

	return IRQ_HANDLED;
}

static struct virt_dma_desc *mtk_hsdma_find_active_desc(struct dma_chan *c,
							dma_cookie_t cookie)
{
	struct mtk_hsdma_vchan *hvc = to_hsdma_vchan(c);
	struct virt_dma_desc *vd;

	list_for_each_entry(vd, &hvc->desc_hw_processing, node)
		if (vd->tx.cookie == cookie)
			return vd;

	list_for_each_entry(vd, &hvc->vc.desc_issued, node)
		if (vd->tx.cookie == cookie)
			return vd;

	return NULL;
}

static enum dma_status mtk_hsdma_tx_status(struct dma_chan *c,
					   dma_cookie_t cookie,
					   struct dma_tx_state *txstate)
{
	struct mtk_hsdma_vchan *hvc = to_hsdma_vchan(c);
	struct mtk_hsdma_vdesc *hvd;
	struct virt_dma_desc *vd;
	enum dma_status ret;
	unsigned long flags;
	size_t bytes = 0;

	ret = dma_cookie_status(c, cookie, txstate);
	if (ret == DMA_COMPLETE || !txstate)
		return ret;

	spin_lock_irqsave(&hvc->vc.lock, flags);
	vd = mtk_hsdma_find_active_desc(c, cookie);
	spin_unlock_irqrestore(&hvc->vc.lock, flags);

	if (vd) {
		hvd = to_hsdma_vdesc(vd);
		bytes = hvd->residue;
	}

	dma_set_residue(txstate, bytes);

	return ret;
}

static void mtk_hsdma_issue_pending(struct dma_chan *c)
{
	struct mtk_hsdma_device *hsdma = to_hsdma_dev(c);
	struct mtk_hsdma_vchan *hvc = to_hsdma_vchan(c);
	unsigned long flags;

	spin_lock_irqsave(&hvc->vc.lock, flags);

	if (vchan_issue_pending(&hvc->vc))
		mtk_hsdma_issue_vchan_pending(hsdma, hvc);

	spin_unlock_irqrestore(&hvc->vc.lock, flags);
}

static struct dma_async_tx_descriptor *
mtk_hsdma_prep_dma_memcpy(struct dma_chan *c, dma_addr_t dest,
			  dma_addr_t src, size_t len, unsigned long flags)
{
	struct mtk_hsdma_vdesc *hvd;

	hvd = kzalloc(sizeof(*hvd), GFP_NOWAIT);
	if (!hvd)
		return NULL;

	hvd->len = len;
	hvd->residue = len;
	hvd->src = src;
	hvd->dest = dest;

	return vchan_tx_prep(to_virt_chan(c), &hvd->vd, flags);
}

static int mtk_hsdma_free_inactive_desc(struct dma_chan *c)
{
	struct virt_dma_chan *vc = to_virt_chan(c);
	unsigned long flags;
	LIST_HEAD(head);

	spin_lock_irqsave(&vc->lock, flags);
	list_splice_tail_init(&vc->desc_allocated, &head);
	list_splice_tail_init(&vc->desc_submitted, &head);
	list_splice_tail_init(&vc->desc_issued, &head);
	spin_unlock_irqrestore(&vc->lock, flags);

	 
	vchan_dma_desc_free_list(vc, &head);

	return 0;
}

static void mtk_hsdma_free_active_desc(struct dma_chan *c)
{
	struct mtk_hsdma_vchan *hvc = to_hsdma_vchan(c);
	bool sync_needed = false;

	 
	spin_lock(&hvc->vc.lock);
	if (!list_empty(&hvc->desc_hw_processing)) {
		hvc->issue_synchronize = true;
		sync_needed = true;
	}
	spin_unlock(&hvc->vc.lock);

	if (sync_needed)
		wait_for_completion(&hvc->issue_completion);
	 
	WARN_ONCE(!list_empty(&hvc->desc_hw_processing),
		  "Desc pending still in list desc_hw_processing\n");

	 
	vchan_synchronize(&hvc->vc);

	WARN_ONCE(!list_empty(&hvc->vc.desc_completed),
		  "Desc pending still in list desc_completed\n");
}

static int mtk_hsdma_terminate_all(struct dma_chan *c)
{
	 
	mtk_hsdma_free_inactive_desc(c);

	 
	mtk_hsdma_free_active_desc(c);

	return 0;
}

static int mtk_hsdma_alloc_chan_resources(struct dma_chan *c)
{
	struct mtk_hsdma_device *hsdma = to_hsdma_dev(c);
	int err;

	 
	if (!refcount_read(&hsdma->pc_refcnt)) {
		err = mtk_hsdma_alloc_pchan(hsdma, hsdma->pc);
		if (err)
			return err;
		 
		refcount_set(&hsdma->pc_refcnt, 1);
	} else {
		refcount_inc(&hsdma->pc_refcnt);
	}

	return 0;
}

static void mtk_hsdma_free_chan_resources(struct dma_chan *c)
{
	struct mtk_hsdma_device *hsdma = to_hsdma_dev(c);

	 
	mtk_hsdma_terminate_all(c);

	 
	if (!refcount_dec_and_test(&hsdma->pc_refcnt))
		return;

	mtk_hsdma_free_pchan(hsdma, hsdma->pc);
}

static int mtk_hsdma_hw_init(struct mtk_hsdma_device *hsdma)
{
	int err;

	pm_runtime_enable(hsdma2dev(hsdma));
	pm_runtime_get_sync(hsdma2dev(hsdma));

	err = clk_prepare_enable(hsdma->clk);
	if (err)
		return err;

	mtk_dma_write(hsdma, MTK_HSDMA_INT_ENABLE, 0);
	mtk_dma_write(hsdma, MTK_HSDMA_GLO, MTK_HSDMA_GLO_DEFAULT);

	return 0;
}

static int mtk_hsdma_hw_deinit(struct mtk_hsdma_device *hsdma)
{
	mtk_dma_write(hsdma, MTK_HSDMA_GLO, 0);

	clk_disable_unprepare(hsdma->clk);

	pm_runtime_put_sync(hsdma2dev(hsdma));
	pm_runtime_disable(hsdma2dev(hsdma));

	return 0;
}

static const struct mtk_hsdma_soc mt7623_soc = {
	.ddone = BIT(31),
	.ls0 = BIT(30),
};

static const struct mtk_hsdma_soc mt7622_soc = {
	.ddone = BIT(15),
	.ls0 = BIT(14),
};

static const struct of_device_id mtk_hsdma_match[] = {
	{ .compatible = "mediatek,mt7623-hsdma", .data = &mt7623_soc},
	{ .compatible = "mediatek,mt7622-hsdma", .data = &mt7622_soc},
	{   }
};
MODULE_DEVICE_TABLE(of, mtk_hsdma_match);

static int mtk_hsdma_probe(struct platform_device *pdev)
{
	struct mtk_hsdma_device *hsdma;
	struct mtk_hsdma_vchan *vc;
	struct dma_device *dd;
	int i, err;

	hsdma = devm_kzalloc(&pdev->dev, sizeof(*hsdma), GFP_KERNEL);
	if (!hsdma)
		return -ENOMEM;

	dd = &hsdma->ddev;

	hsdma->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(hsdma->base))
		return PTR_ERR(hsdma->base);

	hsdma->soc = of_device_get_match_data(&pdev->dev);
	if (!hsdma->soc) {
		dev_err(&pdev->dev, "No device match found\n");
		return -ENODEV;
	}

	hsdma->clk = devm_clk_get(&pdev->dev, "hsdma");
	if (IS_ERR(hsdma->clk)) {
		dev_err(&pdev->dev, "No clock for %s\n",
			dev_name(&pdev->dev));
		return PTR_ERR(hsdma->clk);
	}

	err = platform_get_irq(pdev, 0);
	if (err < 0)
		return err;
	hsdma->irq = err;

	refcount_set(&hsdma->pc_refcnt, 0);
	spin_lock_init(&hsdma->lock);

	dma_cap_set(DMA_MEMCPY, dd->cap_mask);

	dd->copy_align = MTK_HSDMA_ALIGN_SIZE;
	dd->device_alloc_chan_resources = mtk_hsdma_alloc_chan_resources;
	dd->device_free_chan_resources = mtk_hsdma_free_chan_resources;
	dd->device_tx_status = mtk_hsdma_tx_status;
	dd->device_issue_pending = mtk_hsdma_issue_pending;
	dd->device_prep_dma_memcpy = mtk_hsdma_prep_dma_memcpy;
	dd->device_terminate_all = mtk_hsdma_terminate_all;
	dd->src_addr_widths = MTK_HSDMA_DMA_BUSWIDTHS;
	dd->dst_addr_widths = MTK_HSDMA_DMA_BUSWIDTHS;
	dd->directions = BIT(DMA_MEM_TO_MEM);
	dd->residue_granularity = DMA_RESIDUE_GRANULARITY_SEGMENT;
	dd->dev = &pdev->dev;
	INIT_LIST_HEAD(&dd->channels);

	hsdma->dma_requests = MTK_HSDMA_NR_VCHANS;
	if (pdev->dev.of_node && of_property_read_u32(pdev->dev.of_node,
						      "dma-requests",
						      &hsdma->dma_requests)) {
		dev_info(&pdev->dev,
			 "Using %u as missing dma-requests property\n",
			 MTK_HSDMA_NR_VCHANS);
	}

	hsdma->pc = devm_kcalloc(&pdev->dev, MTK_HSDMA_NR_MAX_PCHANS,
				 sizeof(*hsdma->pc), GFP_KERNEL);
	if (!hsdma->pc)
		return -ENOMEM;

	hsdma->vc = devm_kcalloc(&pdev->dev, hsdma->dma_requests,
				 sizeof(*hsdma->vc), GFP_KERNEL);
	if (!hsdma->vc)
		return -ENOMEM;

	for (i = 0; i < hsdma->dma_requests; i++) {
		vc = &hsdma->vc[i];
		vc->vc.desc_free = mtk_hsdma_vdesc_free;
		vchan_init(&vc->vc, dd);
		init_completion(&vc->issue_completion);
		INIT_LIST_HEAD(&vc->desc_hw_processing);
	}

	err = dma_async_device_register(dd);
	if (err)
		return err;

	err = of_dma_controller_register(pdev->dev.of_node,
					 of_dma_xlate_by_chan_id, hsdma);
	if (err) {
		dev_err(&pdev->dev,
			"MediaTek HSDMA OF registration failed %d\n", err);
		goto err_unregister;
	}

	mtk_hsdma_hw_init(hsdma);

	err = devm_request_irq(&pdev->dev, hsdma->irq,
			       mtk_hsdma_irq, 0,
			       dev_name(&pdev->dev), hsdma);
	if (err) {
		dev_err(&pdev->dev,
			"request_irq failed with err %d\n", err);
		goto err_free;
	}

	platform_set_drvdata(pdev, hsdma);

	dev_info(&pdev->dev, "MediaTek HSDMA driver registered\n");

	return 0;

err_free:
	mtk_hsdma_hw_deinit(hsdma);
	of_dma_controller_free(pdev->dev.of_node);
err_unregister:
	dma_async_device_unregister(dd);

	return err;
}

static int mtk_hsdma_remove(struct platform_device *pdev)
{
	struct mtk_hsdma_device *hsdma = platform_get_drvdata(pdev);
	struct mtk_hsdma_vchan *vc;
	int i;

	 
	for (i = 0; i < hsdma->dma_requests; i++) {
		vc = &hsdma->vc[i];

		list_del(&vc->vc.chan.device_node);
		tasklet_kill(&vc->vc.task);
	}

	 
	mtk_dma_write(hsdma, MTK_HSDMA_INT_ENABLE, 0);

	 
	synchronize_irq(hsdma->irq);

	 
	mtk_hsdma_hw_deinit(hsdma);

	dma_async_device_unregister(&hsdma->ddev);
	of_dma_controller_free(pdev->dev.of_node);

	return 0;
}

static struct platform_driver mtk_hsdma_driver = {
	.probe		= mtk_hsdma_probe,
	.remove		= mtk_hsdma_remove,
	.driver = {
		.name		= KBUILD_MODNAME,
		.of_match_table	= mtk_hsdma_match,
	},
};
module_platform_driver(mtk_hsdma_driver);

MODULE_DESCRIPTION("MediaTek High-Speed DMA Controller Driver");
MODULE_AUTHOR("Sean Wang <sean.wang@mediatek.com>");
MODULE_LICENSE("GPL v2");
