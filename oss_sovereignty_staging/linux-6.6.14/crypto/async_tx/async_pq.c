
 
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/raid/pq.h>
#include <linux/async_tx.h>
#include <linux/gfp.h>

 
static struct page *pq_scribble_page;

 
#define P(b, d) (b[d-2])
#define Q(b, d) (b[d-1])

#define MAX_DISKS 255

 
static __async_inline struct dma_async_tx_descriptor *
do_async_gen_syndrome(struct dma_chan *chan,
		      const unsigned char *scfs, int disks,
		      struct dmaengine_unmap_data *unmap,
		      enum dma_ctrl_flags dma_flags,
		      struct async_submit_ctl *submit)
{
	struct dma_async_tx_descriptor *tx = NULL;
	struct dma_device *dma = chan->device;
	enum async_tx_flags flags_orig = submit->flags;
	dma_async_tx_callback cb_fn_orig = submit->cb_fn;
	dma_async_tx_callback cb_param_orig = submit->cb_param;
	int src_cnt = disks - 2;
	unsigned short pq_src_cnt;
	dma_addr_t dma_dest[2];
	int src_off = 0;

	while (src_cnt > 0) {
		submit->flags = flags_orig;
		pq_src_cnt = min(src_cnt, dma_maxpq(dma, dma_flags));
		 
		if (src_cnt > pq_src_cnt) {
			submit->flags &= ~ASYNC_TX_ACK;
			submit->flags |= ASYNC_TX_FENCE;
			submit->cb_fn = NULL;
			submit->cb_param = NULL;
		} else {
			submit->cb_fn = cb_fn_orig;
			submit->cb_param = cb_param_orig;
			if (cb_fn_orig)
				dma_flags |= DMA_PREP_INTERRUPT;
		}
		if (submit->flags & ASYNC_TX_FENCE)
			dma_flags |= DMA_PREP_FENCE;

		 
		for (;;) {
			dma_dest[0] = unmap->addr[disks - 2];
			dma_dest[1] = unmap->addr[disks - 1];
			tx = dma->device_prep_dma_pq(chan, dma_dest,
						     &unmap->addr[src_off],
						     pq_src_cnt,
						     &scfs[src_off], unmap->len,
						     dma_flags);
			if (likely(tx))
				break;
			async_tx_quiesce(&submit->depend_tx);
			dma_async_issue_pending(chan);
		}

		dma_set_unmap(tx, unmap);
		async_tx_submit(chan, tx, submit);
		submit->depend_tx = tx;

		 
		src_cnt -= pq_src_cnt;
		src_off += pq_src_cnt;

		dma_flags |= DMA_PREP_CONTINUE;
	}

	return tx;
}

 
static void
do_sync_gen_syndrome(struct page **blocks, unsigned int *offsets, int disks,
		     size_t len, struct async_submit_ctl *submit)
{
	void **srcs;
	int i;
	int start = -1, stop = disks - 3;

	if (submit->scribble)
		srcs = submit->scribble;
	else
		srcs = (void **) blocks;

	for (i = 0; i < disks; i++) {
		if (blocks[i] == NULL) {
			BUG_ON(i > disks - 3);  
			srcs[i] = (void*)raid6_empty_zero_page;
		} else {
			srcs[i] = page_address(blocks[i]) + offsets[i];

			if (i < disks - 2) {
				stop = i;
				if (start == -1)
					start = i;
			}
		}
	}
	if (submit->flags & ASYNC_TX_PQ_XOR_DST) {
		BUG_ON(!raid6_call.xor_syndrome);
		if (start >= 0)
			raid6_call.xor_syndrome(disks, start, stop, len, srcs);
	} else
		raid6_call.gen_syndrome(disks, len, srcs);
	async_tx_sync_epilog(submit);
}

static inline bool
is_dma_pq_aligned_offs(struct dma_device *dev, unsigned int *offs,
				     int src_cnt, size_t len)
{
	int i;

	for (i = 0; i < src_cnt; i++) {
		if (!is_dma_pq_aligned(dev, offs[i], 0, len))
			return false;
	}
	return true;
}

 
struct dma_async_tx_descriptor *
async_gen_syndrome(struct page **blocks, unsigned int *offsets, int disks,
		   size_t len, struct async_submit_ctl *submit)
{
	int src_cnt = disks - 2;
	struct dma_chan *chan = async_tx_find_channel(submit, DMA_PQ,
						      &P(blocks, disks), 2,
						      blocks, src_cnt, len);
	struct dma_device *device = chan ? chan->device : NULL;
	struct dmaengine_unmap_data *unmap = NULL;

	BUG_ON(disks > MAX_DISKS || !(P(blocks, disks) || Q(blocks, disks)));

	if (device)
		unmap = dmaengine_get_unmap_data(device->dev, disks, GFP_NOWAIT);

	 
	if (unmap && !(submit->flags & ASYNC_TX_PQ_XOR_DST) &&
	    (src_cnt <= dma_maxpq(device, 0) ||
	     dma_maxpq(device, DMA_PREP_CONTINUE) > 0) &&
	    is_dma_pq_aligned_offs(device, offsets, disks, len)) {
		struct dma_async_tx_descriptor *tx;
		enum dma_ctrl_flags dma_flags = 0;
		unsigned char coefs[MAX_DISKS];
		int i, j;

		 
		pr_debug("%s: (async) disks: %d len: %zu\n",
			 __func__, disks, len);

		 
		unmap->len = len;
		for (i = 0, j = 0; i < src_cnt; i++) {
			if (blocks[i] == NULL)
				continue;
			unmap->addr[j] = dma_map_page(device->dev, blocks[i],
						offsets[i], len, DMA_TO_DEVICE);
			coefs[j] = raid6_gfexp[i];
			unmap->to_cnt++;
			j++;
		}

		 
		unmap->bidi_cnt++;
		if (P(blocks, disks))
			unmap->addr[j++] = dma_map_page(device->dev, P(blocks, disks),
							P(offsets, disks),
							len, DMA_BIDIRECTIONAL);
		else {
			unmap->addr[j++] = 0;
			dma_flags |= DMA_PREP_PQ_DISABLE_P;
		}

		unmap->bidi_cnt++;
		if (Q(blocks, disks))
			unmap->addr[j++] = dma_map_page(device->dev, Q(blocks, disks),
							Q(offsets, disks),
							len, DMA_BIDIRECTIONAL);
		else {
			unmap->addr[j++] = 0;
			dma_flags |= DMA_PREP_PQ_DISABLE_Q;
		}

		tx = do_async_gen_syndrome(chan, coefs, j, unmap, dma_flags, submit);
		dmaengine_unmap_put(unmap);
		return tx;
	}

	dmaengine_unmap_put(unmap);

	 
	pr_debug("%s: (sync) disks: %d len: %zu\n", __func__, disks, len);

	 
	async_tx_quiesce(&submit->depend_tx);

	if (!P(blocks, disks)) {
		P(blocks, disks) = pq_scribble_page;
		P(offsets, disks) = 0;
	}
	if (!Q(blocks, disks)) {
		Q(blocks, disks) = pq_scribble_page;
		Q(offsets, disks) = 0;
	}
	do_sync_gen_syndrome(blocks, offsets, disks, len, submit);

	return NULL;
}
EXPORT_SYMBOL_GPL(async_gen_syndrome);

static inline struct dma_chan *
pq_val_chan(struct async_submit_ctl *submit, struct page **blocks, int disks, size_t len)
{
	#ifdef CONFIG_ASYNC_TX_DISABLE_PQ_VAL_DMA
	return NULL;
	#endif
	return async_tx_find_channel(submit, DMA_PQ_VAL, NULL, 0,  blocks,
				     disks, len);
}

 
struct dma_async_tx_descriptor *
async_syndrome_val(struct page **blocks, unsigned int *offsets, int disks,
		   size_t len, enum sum_check_flags *pqres, struct page *spare,
		   unsigned int s_off, struct async_submit_ctl *submit)
{
	struct dma_chan *chan = pq_val_chan(submit, blocks, disks, len);
	struct dma_device *device = chan ? chan->device : NULL;
	struct dma_async_tx_descriptor *tx;
	unsigned char coefs[MAX_DISKS];
	enum dma_ctrl_flags dma_flags = submit->cb_fn ? DMA_PREP_INTERRUPT : 0;
	struct dmaengine_unmap_data *unmap = NULL;

	BUG_ON(disks < 4 || disks > MAX_DISKS);

	if (device)
		unmap = dmaengine_get_unmap_data(device->dev, disks, GFP_NOWAIT);

	if (unmap && disks <= dma_maxpq(device, 0) &&
	    is_dma_pq_aligned_offs(device, offsets, disks, len)) {
		struct device *dev = device->dev;
		dma_addr_t pq[2];
		int i, j = 0, src_cnt = 0;

		pr_debug("%s: (async) disks: %d len: %zu\n",
			 __func__, disks, len);

		unmap->len = len;
		for (i = 0; i < disks-2; i++)
			if (likely(blocks[i])) {
				unmap->addr[j] = dma_map_page(dev, blocks[i],
							      offsets[i], len,
							      DMA_TO_DEVICE);
				coefs[j] = raid6_gfexp[i];
				unmap->to_cnt++;
				src_cnt++;
				j++;
			}

		if (!P(blocks, disks)) {
			pq[0] = 0;
			dma_flags |= DMA_PREP_PQ_DISABLE_P;
		} else {
			pq[0] = dma_map_page(dev, P(blocks, disks),
					     P(offsets, disks), len,
					     DMA_TO_DEVICE);
			unmap->addr[j++] = pq[0];
			unmap->to_cnt++;
		}
		if (!Q(blocks, disks)) {
			pq[1] = 0;
			dma_flags |= DMA_PREP_PQ_DISABLE_Q;
		} else {
			pq[1] = dma_map_page(dev, Q(blocks, disks),
					     Q(offsets, disks), len,
					     DMA_TO_DEVICE);
			unmap->addr[j++] = pq[1];
			unmap->to_cnt++;
		}

		if (submit->flags & ASYNC_TX_FENCE)
			dma_flags |= DMA_PREP_FENCE;
		for (;;) {
			tx = device->device_prep_dma_pq_val(chan, pq,
							    unmap->addr,
							    src_cnt,
							    coefs,
							    len, pqres,
							    dma_flags);
			if (likely(tx))
				break;
			async_tx_quiesce(&submit->depend_tx);
			dma_async_issue_pending(chan);
		}

		dma_set_unmap(tx, unmap);
		async_tx_submit(chan, tx, submit);
	} else {
		struct page *p_src = P(blocks, disks);
		unsigned int p_off = P(offsets, disks);
		struct page *q_src = Q(blocks, disks);
		unsigned int q_off = Q(offsets, disks);
		enum async_tx_flags flags_orig = submit->flags;
		dma_async_tx_callback cb_fn_orig = submit->cb_fn;
		void *scribble = submit->scribble;
		void *cb_param_orig = submit->cb_param;
		void *p, *q, *s;

		pr_debug("%s: (sync) disks: %d len: %zu\n",
			 __func__, disks, len);

		 
		BUG_ON(!spare || !scribble);

		 
		async_tx_quiesce(&submit->depend_tx);

		 
		tx = NULL;
		*pqres = 0;
		if (p_src) {
			init_async_submit(submit, ASYNC_TX_XOR_ZERO_DST, NULL,
					  NULL, NULL, scribble);
			tx = async_xor_offs(spare, s_off,
					blocks, offsets, disks-2, len, submit);
			async_tx_quiesce(&tx);
			p = page_address(p_src) + p_off;
			s = page_address(spare) + s_off;
			*pqres |= !!memcmp(p, s, len) << SUM_CHECK_P;
		}

		if (q_src) {
			P(blocks, disks) = NULL;
			Q(blocks, disks) = spare;
			Q(offsets, disks) = s_off;
			init_async_submit(submit, 0, NULL, NULL, NULL, scribble);
			tx = async_gen_syndrome(blocks, offsets, disks,
					len, submit);
			async_tx_quiesce(&tx);
			q = page_address(q_src) + q_off;
			s = page_address(spare) + s_off;
			*pqres |= !!memcmp(q, s, len) << SUM_CHECK_Q;
		}

		 
		P(blocks, disks) = p_src;
		P(offsets, disks) = p_off;
		Q(blocks, disks) = q_src;
		Q(offsets, disks) = q_off;

		submit->cb_fn = cb_fn_orig;
		submit->cb_param = cb_param_orig;
		submit->flags = flags_orig;
		async_tx_sync_epilog(submit);
		tx = NULL;
	}
	dmaengine_unmap_put(unmap);

	return tx;
}
EXPORT_SYMBOL_GPL(async_syndrome_val);

static int __init async_pq_init(void)
{
	pq_scribble_page = alloc_page(GFP_KERNEL);

	if (pq_scribble_page)
		return 0;

	pr_err("%s: failed to allocate required spare page\n", __func__);

	return -ENOMEM;
}

static void __exit async_pq_exit(void)
{
	__free_page(pq_scribble_page);
}

module_init(async_pq_init);
module_exit(async_pq_exit);

MODULE_DESCRIPTION("asynchronous raid6 syndrome generation/validation");
MODULE_LICENSE("GPL");
