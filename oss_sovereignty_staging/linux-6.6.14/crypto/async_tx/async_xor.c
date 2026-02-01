
 
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/raid/xor.h>
#include <linux/async_tx.h>

 
static __async_inline struct dma_async_tx_descriptor *
do_async_xor(struct dma_chan *chan, struct dmaengine_unmap_data *unmap,
	     struct async_submit_ctl *submit)
{
	struct dma_device *dma = chan->device;
	struct dma_async_tx_descriptor *tx = NULL;
	dma_async_tx_callback cb_fn_orig = submit->cb_fn;
	void *cb_param_orig = submit->cb_param;
	enum async_tx_flags flags_orig = submit->flags;
	enum dma_ctrl_flags dma_flags = 0;
	int src_cnt = unmap->to_cnt;
	int xor_src_cnt;
	dma_addr_t dma_dest = unmap->addr[unmap->to_cnt];
	dma_addr_t *src_list = unmap->addr;

	while (src_cnt) {
		dma_addr_t tmp;

		submit->flags = flags_orig;
		xor_src_cnt = min(src_cnt, (int)dma->max_xor);
		 
		if (src_cnt > xor_src_cnt) {
			submit->flags &= ~ASYNC_TX_ACK;
			submit->flags |= ASYNC_TX_FENCE;
			submit->cb_fn = NULL;
			submit->cb_param = NULL;
		} else {
			submit->cb_fn = cb_fn_orig;
			submit->cb_param = cb_param_orig;
		}
		if (submit->cb_fn)
			dma_flags |= DMA_PREP_INTERRUPT;
		if (submit->flags & ASYNC_TX_FENCE)
			dma_flags |= DMA_PREP_FENCE;

		 
		tmp = src_list[0];
		if (src_list > unmap->addr)
			src_list[0] = dma_dest;
		tx = dma->device_prep_dma_xor(chan, dma_dest, src_list,
					      xor_src_cnt, unmap->len,
					      dma_flags);

		if (unlikely(!tx))
			async_tx_quiesce(&submit->depend_tx);

		 
		while (unlikely(!tx)) {
			dma_async_issue_pending(chan);
			tx = dma->device_prep_dma_xor(chan, dma_dest,
						      src_list,
						      xor_src_cnt, unmap->len,
						      dma_flags);
		}
		src_list[0] = tmp;

		dma_set_unmap(tx, unmap);
		async_tx_submit(chan, tx, submit);
		submit->depend_tx = tx;

		if (src_cnt > xor_src_cnt) {
			 
			src_cnt -= xor_src_cnt;
			 
			src_cnt++;
			src_list += xor_src_cnt - 1;
		} else
			break;
	}

	return tx;
}

static void
do_sync_xor_offs(struct page *dest, unsigned int offset,
		struct page **src_list, unsigned int *src_offs,
	    int src_cnt, size_t len, struct async_submit_ctl *submit)
{
	int i;
	int xor_src_cnt = 0;
	int src_off = 0;
	void *dest_buf;
	void **srcs;

	if (submit->scribble)
		srcs = submit->scribble;
	else
		srcs = (void **) src_list;

	 
	for (i = 0; i < src_cnt; i++)
		if (src_list[i])
			srcs[xor_src_cnt++] = page_address(src_list[i]) +
				(src_offs ? src_offs[i] : offset);
	src_cnt = xor_src_cnt;
	 
	dest_buf = page_address(dest) + offset;

	if (submit->flags & ASYNC_TX_XOR_ZERO_DST)
		memset(dest_buf, 0, len);

	while (src_cnt > 0) {
		 
		xor_src_cnt = min(src_cnt, MAX_XOR_BLOCKS);
		xor_blocks(xor_src_cnt, len, dest_buf, &srcs[src_off]);

		 
		src_cnt -= xor_src_cnt;
		src_off += xor_src_cnt;
	}

	async_tx_sync_epilog(submit);
}

static inline bool
dma_xor_aligned_offsets(struct dma_device *device, unsigned int offset,
		unsigned int *src_offs, int src_cnt, int len)
{
	int i;

	if (!is_dma_xor_aligned(device, offset, 0, len))
		return false;

	if (!src_offs)
		return true;

	for (i = 0; i < src_cnt; i++) {
		if (!is_dma_xor_aligned(device, src_offs[i], 0, len))
			return false;
	}
	return true;
}

 
struct dma_async_tx_descriptor *
async_xor_offs(struct page *dest, unsigned int offset,
		struct page **src_list, unsigned int *src_offs,
		int src_cnt, size_t len, struct async_submit_ctl *submit)
{
	struct dma_chan *chan = async_tx_find_channel(submit, DMA_XOR,
						      &dest, 1, src_list,
						      src_cnt, len);
	struct dma_device *device = chan ? chan->device : NULL;
	struct dmaengine_unmap_data *unmap = NULL;

	BUG_ON(src_cnt <= 1);

	if (device)
		unmap = dmaengine_get_unmap_data(device->dev, src_cnt+1, GFP_NOWAIT);

	if (unmap && dma_xor_aligned_offsets(device, offset,
				src_offs, src_cnt, len)) {
		struct dma_async_tx_descriptor *tx;
		int i, j;

		 
		pr_debug("%s (async): len: %zu\n", __func__, len);

		unmap->len = len;
		for (i = 0, j = 0; i < src_cnt; i++) {
			if (!src_list[i])
				continue;
			unmap->to_cnt++;
			unmap->addr[j++] = dma_map_page(device->dev, src_list[i],
					src_offs ? src_offs[i] : offset,
					len, DMA_TO_DEVICE);
		}

		 
		unmap->addr[j] = dma_map_page(device->dev, dest, offset, len,
					      DMA_BIDIRECTIONAL);
		unmap->bidi_cnt = 1;

		tx = do_async_xor(chan, unmap, submit);
		dmaengine_unmap_put(unmap);
		return tx;
	} else {
		dmaengine_unmap_put(unmap);
		 
		pr_debug("%s (sync): len: %zu\n", __func__, len);
		WARN_ONCE(chan, "%s: no space for dma address conversion\n",
			  __func__);

		 
		if (submit->flags & ASYNC_TX_XOR_DROP_DST) {
			src_cnt--;
			src_list++;
			if (src_offs)
				src_offs++;
		}

		 
		async_tx_quiesce(&submit->depend_tx);

		do_sync_xor_offs(dest, offset, src_list, src_offs,
				src_cnt, len, submit);

		return NULL;
	}
}
EXPORT_SYMBOL_GPL(async_xor_offs);

 
struct dma_async_tx_descriptor *
async_xor(struct page *dest, struct page **src_list, unsigned int offset,
	  int src_cnt, size_t len, struct async_submit_ctl *submit)
{
	return async_xor_offs(dest, offset, src_list, NULL,
			src_cnt, len, submit);
}
EXPORT_SYMBOL_GPL(async_xor);

static int page_is_zero(struct page *p, unsigned int offset, size_t len)
{
	return !memchr_inv(page_address(p) + offset, 0, len);
}

static inline struct dma_chan *
xor_val_chan(struct async_submit_ctl *submit, struct page *dest,
		 struct page **src_list, int src_cnt, size_t len)
{
	#ifdef CONFIG_ASYNC_TX_DISABLE_XOR_VAL_DMA
	return NULL;
	#endif
	return async_tx_find_channel(submit, DMA_XOR_VAL, &dest, 1, src_list,
				     src_cnt, len);
}

 
struct dma_async_tx_descriptor *
async_xor_val_offs(struct page *dest, unsigned int offset,
		struct page **src_list, unsigned int *src_offs,
		int src_cnt, size_t len, enum sum_check_flags *result,
		struct async_submit_ctl *submit)
{
	struct dma_chan *chan = xor_val_chan(submit, dest, src_list, src_cnt, len);
	struct dma_device *device = chan ? chan->device : NULL;
	struct dma_async_tx_descriptor *tx = NULL;
	struct dmaengine_unmap_data *unmap = NULL;

	BUG_ON(src_cnt <= 1);

	if (device)
		unmap = dmaengine_get_unmap_data(device->dev, src_cnt, GFP_NOWAIT);

	if (unmap && src_cnt <= device->max_xor &&
	    dma_xor_aligned_offsets(device, offset, src_offs, src_cnt, len)) {
		unsigned long dma_prep_flags = 0;
		int i;

		pr_debug("%s: (async) len: %zu\n", __func__, len);

		if (submit->cb_fn)
			dma_prep_flags |= DMA_PREP_INTERRUPT;
		if (submit->flags & ASYNC_TX_FENCE)
			dma_prep_flags |= DMA_PREP_FENCE;

		for (i = 0; i < src_cnt; i++) {
			unmap->addr[i] = dma_map_page(device->dev, src_list[i],
					src_offs ? src_offs[i] : offset,
					len, DMA_TO_DEVICE);
			unmap->to_cnt++;
		}
		unmap->len = len;

		tx = device->device_prep_dma_xor_val(chan, unmap->addr, src_cnt,
						     len, result,
						     dma_prep_flags);
		if (unlikely(!tx)) {
			async_tx_quiesce(&submit->depend_tx);

			while (!tx) {
				dma_async_issue_pending(chan);
				tx = device->device_prep_dma_xor_val(chan,
					unmap->addr, src_cnt, len, result,
					dma_prep_flags);
			}
		}
		dma_set_unmap(tx, unmap);
		async_tx_submit(chan, tx, submit);
	} else {
		enum async_tx_flags flags_orig = submit->flags;

		pr_debug("%s: (sync) len: %zu\n", __func__, len);
		WARN_ONCE(device && src_cnt <= device->max_xor,
			  "%s: no space for dma address conversion\n",
			  __func__);

		submit->flags |= ASYNC_TX_XOR_DROP_DST;
		submit->flags &= ~ASYNC_TX_ACK;

		tx = async_xor_offs(dest, offset, src_list, src_offs,
				src_cnt, len, submit);

		async_tx_quiesce(&tx);

		*result = !page_is_zero(dest, offset, len) << SUM_CHECK_P;

		async_tx_sync_epilog(submit);
		submit->flags = flags_orig;
	}
	dmaengine_unmap_put(unmap);

	return tx;
}
EXPORT_SYMBOL_GPL(async_xor_val_offs);

 
struct dma_async_tx_descriptor *
async_xor_val(struct page *dest, struct page **src_list, unsigned int offset,
	      int src_cnt, size_t len, enum sum_check_flags *result,
	      struct async_submit_ctl *submit)
{
	return async_xor_val_offs(dest, offset, src_list, NULL, src_cnt,
			len, result, submit);
}
EXPORT_SYMBOL_GPL(async_xor_val);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("asynchronous xor/xor-zero-sum api");
MODULE_LICENSE("GPL");
