#ifndef DMAENGINE_H
#define DMAENGINE_H
#include <linux/bug.h>
#include <linux/dmaengine.h>
static inline void dma_cookie_init(struct dma_chan *chan)
{
	chan->cookie = DMA_MIN_COOKIE;
	chan->completed_cookie = DMA_MIN_COOKIE;
}
static inline dma_cookie_t dma_cookie_assign(struct dma_async_tx_descriptor *tx)
{
	struct dma_chan *chan = tx->chan;
	dma_cookie_t cookie;
	cookie = chan->cookie + 1;
	if (cookie < DMA_MIN_COOKIE)
		cookie = DMA_MIN_COOKIE;
	tx->cookie = chan->cookie = cookie;
	return cookie;
}
static inline void dma_cookie_complete(struct dma_async_tx_descriptor *tx)
{
	BUG_ON(tx->cookie < DMA_MIN_COOKIE);
	tx->chan->completed_cookie = tx->cookie;
	tx->cookie = 0;
}
static inline enum dma_status dma_cookie_status(struct dma_chan *chan,
	dma_cookie_t cookie, struct dma_tx_state *state)
{
	dma_cookie_t used, complete;
	used = chan->cookie;
	complete = chan->completed_cookie;
	barrier();
	if (state) {
		state->last = complete;
		state->used = used;
		state->residue = 0;
		state->in_flight_bytes = 0;
	}
	return dma_async_is_complete(cookie, complete, used);
}
static inline void dma_set_residue(struct dma_tx_state *state, u32 residue)
{
	if (state)
		state->residue = residue;
}
static inline void dma_set_in_flight_bytes(struct dma_tx_state *state,
					   u32 in_flight_bytes)
{
	if (state)
		state->in_flight_bytes = in_flight_bytes;
}
struct dmaengine_desc_callback {
	dma_async_tx_callback callback;
	dma_async_tx_callback_result callback_result;
	void *callback_param;
};
static inline void
dmaengine_desc_get_callback(struct dma_async_tx_descriptor *tx,
			    struct dmaengine_desc_callback *cb)
{
	cb->callback = tx->callback;
	cb->callback_result = tx->callback_result;
	cb->callback_param = tx->callback_param;
}
static inline void
dmaengine_desc_callback_invoke(struct dmaengine_desc_callback *cb,
			       const struct dmaengine_result *result)
{
	struct dmaengine_result dummy_result = {
		.result = DMA_TRANS_NOERROR,
		.residue = 0
	};
	if (cb->callback_result) {
		if (!result)
			result = &dummy_result;
		cb->callback_result(cb->callback_param, result);
	} else if (cb->callback) {
		cb->callback(cb->callback_param);
	}
}
static inline void
dmaengine_desc_get_callback_invoke(struct dma_async_tx_descriptor *tx,
				   const struct dmaengine_result *result)
{
	struct dmaengine_desc_callback cb;
	dmaengine_desc_get_callback(tx, &cb);
	dmaengine_desc_callback_invoke(&cb, result);
}
static inline bool
dmaengine_desc_callback_valid(struct dmaengine_desc_callback *cb)
{
	return cb->callback || cb->callback_result;
}
struct dma_chan *dma_get_slave_channel(struct dma_chan *chan);
struct dma_chan *dma_get_any_slave_channel(struct dma_device *device);
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
static inline struct dentry *
dmaengine_get_debugfs_root(struct dma_device *dma_dev) {
	return dma_dev->dbg_dev_root;
}
#else
struct dentry;
static inline struct dentry *
dmaengine_get_debugfs_root(struct dma_device *dma_dev)
{
	return NULL;
}
#endif  
#endif
