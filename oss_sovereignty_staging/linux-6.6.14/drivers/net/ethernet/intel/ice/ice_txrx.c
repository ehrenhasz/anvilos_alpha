
 

 

#include <linux/mm.h>
#include <linux/netdevice.h>
#include <linux/prefetch.h>
#include <linux/bpf_trace.h>
#include <net/dsfield.h>
#include <net/mpls.h>
#include <net/xdp.h>
#include "ice_txrx_lib.h"
#include "ice_lib.h"
#include "ice.h"
#include "ice_trace.h"
#include "ice_dcb_lib.h"
#include "ice_xsk.h"
#include "ice_eswitch.h"

#define ICE_RX_HDR_SIZE		256

#define FDIR_DESC_RXDID 0x40
#define ICE_FDIR_CLEAN_DELAY 10

 
int
ice_prgm_fdir_fltr(struct ice_vsi *vsi, struct ice_fltr_desc *fdir_desc,
		   u8 *raw_packet)
{
	struct ice_tx_buf *tx_buf, *first;
	struct ice_fltr_desc *f_desc;
	struct ice_tx_desc *tx_desc;
	struct ice_tx_ring *tx_ring;
	struct device *dev;
	dma_addr_t dma;
	u32 td_cmd;
	u16 i;

	 
	if (!vsi)
		return -ENOENT;
	tx_ring = vsi->tx_rings[0];
	if (!tx_ring || !tx_ring->desc)
		return -ENOENT;
	dev = tx_ring->dev;

	 
	for (i = ICE_FDIR_CLEAN_DELAY; ICE_DESC_UNUSED(tx_ring) < 2; i--) {
		if (!i)
			return -EAGAIN;
		msleep_interruptible(1);
	}

	dma = dma_map_single(dev, raw_packet, ICE_FDIR_MAX_RAW_PKT_SIZE,
			     DMA_TO_DEVICE);

	if (dma_mapping_error(dev, dma))
		return -EINVAL;

	 
	i = tx_ring->next_to_use;
	first = &tx_ring->tx_buf[i];
	f_desc = ICE_TX_FDIRDESC(tx_ring, i);
	memcpy(f_desc, fdir_desc, sizeof(*f_desc));

	i++;
	i = (i < tx_ring->count) ? i : 0;
	tx_desc = ICE_TX_DESC(tx_ring, i);
	tx_buf = &tx_ring->tx_buf[i];

	i++;
	tx_ring->next_to_use = (i < tx_ring->count) ? i : 0;

	memset(tx_buf, 0, sizeof(*tx_buf));
	dma_unmap_len_set(tx_buf, len, ICE_FDIR_MAX_RAW_PKT_SIZE);
	dma_unmap_addr_set(tx_buf, dma, dma);

	tx_desc->buf_addr = cpu_to_le64(dma);
	td_cmd = ICE_TXD_LAST_DESC_CMD | ICE_TX_DESC_CMD_DUMMY |
		 ICE_TX_DESC_CMD_RE;

	tx_buf->type = ICE_TX_BUF_DUMMY;
	tx_buf->raw_buf = raw_packet;

	tx_desc->cmd_type_offset_bsz =
		ice_build_ctob(td_cmd, 0, ICE_FDIR_MAX_RAW_PKT_SIZE, 0);

	 
	wmb();

	 
	first->next_to_watch = tx_desc;

	writel(tx_ring->next_to_use, tx_ring->tail);

	return 0;
}

 
static void
ice_unmap_and_free_tx_buf(struct ice_tx_ring *ring, struct ice_tx_buf *tx_buf)
{
	if (dma_unmap_len(tx_buf, len))
		dma_unmap_page(ring->dev,
			       dma_unmap_addr(tx_buf, dma),
			       dma_unmap_len(tx_buf, len),
			       DMA_TO_DEVICE);

	switch (tx_buf->type) {
	case ICE_TX_BUF_DUMMY:
		devm_kfree(ring->dev, tx_buf->raw_buf);
		break;
	case ICE_TX_BUF_SKB:
		dev_kfree_skb_any(tx_buf->skb);
		break;
	case ICE_TX_BUF_XDP_TX:
		page_frag_free(tx_buf->raw_buf);
		break;
	case ICE_TX_BUF_XDP_XMIT:
		xdp_return_frame(tx_buf->xdpf);
		break;
	}

	tx_buf->next_to_watch = NULL;
	tx_buf->type = ICE_TX_BUF_EMPTY;
	dma_unmap_len_set(tx_buf, len, 0);
	 
}

static struct netdev_queue *txring_txq(const struct ice_tx_ring *ring)
{
	return netdev_get_tx_queue(ring->netdev, ring->q_index);
}

 
void ice_clean_tx_ring(struct ice_tx_ring *tx_ring)
{
	u32 size;
	u16 i;

	if (ice_ring_is_xdp(tx_ring) && tx_ring->xsk_pool) {
		ice_xsk_clean_xdp_ring(tx_ring);
		goto tx_skip_free;
	}

	 
	if (!tx_ring->tx_buf)
		return;

	 
	for (i = 0; i < tx_ring->count; i++)
		ice_unmap_and_free_tx_buf(tx_ring, &tx_ring->tx_buf[i]);

tx_skip_free:
	memset(tx_ring->tx_buf, 0, sizeof(*tx_ring->tx_buf) * tx_ring->count);

	size = ALIGN(tx_ring->count * sizeof(struct ice_tx_desc),
		     PAGE_SIZE);
	 
	memset(tx_ring->desc, 0, size);

	tx_ring->next_to_use = 0;
	tx_ring->next_to_clean = 0;

	if (!tx_ring->netdev)
		return;

	 
	netdev_tx_reset_queue(txring_txq(tx_ring));
}

 
void ice_free_tx_ring(struct ice_tx_ring *tx_ring)
{
	u32 size;

	ice_clean_tx_ring(tx_ring);
	devm_kfree(tx_ring->dev, tx_ring->tx_buf);
	tx_ring->tx_buf = NULL;

	if (tx_ring->desc) {
		size = ALIGN(tx_ring->count * sizeof(struct ice_tx_desc),
			     PAGE_SIZE);
		dmam_free_coherent(tx_ring->dev, size,
				   tx_ring->desc, tx_ring->dma);
		tx_ring->desc = NULL;
	}
}

 
static bool ice_clean_tx_irq(struct ice_tx_ring *tx_ring, int napi_budget)
{
	unsigned int total_bytes = 0, total_pkts = 0;
	unsigned int budget = ICE_DFLT_IRQ_WORK;
	struct ice_vsi *vsi = tx_ring->vsi;
	s16 i = tx_ring->next_to_clean;
	struct ice_tx_desc *tx_desc;
	struct ice_tx_buf *tx_buf;

	 
	netdev_txq_bql_complete_prefetchw(txring_txq(tx_ring));

	tx_buf = &tx_ring->tx_buf[i];
	tx_desc = ICE_TX_DESC(tx_ring, i);
	i -= tx_ring->count;

	prefetch(&vsi->state);

	do {
		struct ice_tx_desc *eop_desc = tx_buf->next_to_watch;

		 
		if (!eop_desc)
			break;

		 
		prefetchw(&tx_buf->skb->users);

		smp_rmb();	 

		ice_trace(clean_tx_irq, tx_ring, tx_desc, tx_buf);
		 
		if (!(eop_desc->cmd_type_offset_bsz &
		      cpu_to_le64(ICE_TX_DESC_DTYPE_DESC_DONE)))
			break;

		 
		tx_buf->next_to_watch = NULL;

		 
		total_bytes += tx_buf->bytecount;
		total_pkts += tx_buf->gso_segs;

		 
		napi_consume_skb(tx_buf->skb, napi_budget);

		 
		dma_unmap_single(tx_ring->dev,
				 dma_unmap_addr(tx_buf, dma),
				 dma_unmap_len(tx_buf, len),
				 DMA_TO_DEVICE);

		 
		tx_buf->type = ICE_TX_BUF_EMPTY;
		dma_unmap_len_set(tx_buf, len, 0);

		 
		while (tx_desc != eop_desc) {
			ice_trace(clean_tx_irq_unmap, tx_ring, tx_desc, tx_buf);
			tx_buf++;
			tx_desc++;
			i++;
			if (unlikely(!i)) {
				i -= tx_ring->count;
				tx_buf = tx_ring->tx_buf;
				tx_desc = ICE_TX_DESC(tx_ring, 0);
			}

			 
			if (dma_unmap_len(tx_buf, len)) {
				dma_unmap_page(tx_ring->dev,
					       dma_unmap_addr(tx_buf, dma),
					       dma_unmap_len(tx_buf, len),
					       DMA_TO_DEVICE);
				dma_unmap_len_set(tx_buf, len, 0);
			}
		}
		ice_trace(clean_tx_irq_unmap_eop, tx_ring, tx_desc, tx_buf);

		 
		tx_buf++;
		tx_desc++;
		i++;
		if (unlikely(!i)) {
			i -= tx_ring->count;
			tx_buf = tx_ring->tx_buf;
			tx_desc = ICE_TX_DESC(tx_ring, 0);
		}

		prefetch(tx_desc);

		 
		budget--;
	} while (likely(budget));

	i += tx_ring->count;
	tx_ring->next_to_clean = i;

	ice_update_tx_ring_stats(tx_ring, total_pkts, total_bytes);
	netdev_tx_completed_queue(txring_txq(tx_ring), total_pkts, total_bytes);

#define TX_WAKE_THRESHOLD ((s16)(DESC_NEEDED * 2))
	if (unlikely(total_pkts && netif_carrier_ok(tx_ring->netdev) &&
		     (ICE_DESC_UNUSED(tx_ring) >= TX_WAKE_THRESHOLD))) {
		 
		smp_mb();
		if (netif_tx_queue_stopped(txring_txq(tx_ring)) &&
		    !test_bit(ICE_VSI_DOWN, vsi->state)) {
			netif_tx_wake_queue(txring_txq(tx_ring));
			++tx_ring->ring_stats->tx_stats.restart_q;
		}
	}

	return !!budget;
}

 
int ice_setup_tx_ring(struct ice_tx_ring *tx_ring)
{
	struct device *dev = tx_ring->dev;
	u32 size;

	if (!dev)
		return -ENOMEM;

	 
	WARN_ON(tx_ring->tx_buf);
	tx_ring->tx_buf =
		devm_kcalloc(dev, sizeof(*tx_ring->tx_buf), tx_ring->count,
			     GFP_KERNEL);
	if (!tx_ring->tx_buf)
		return -ENOMEM;

	 
	size = ALIGN(tx_ring->count * sizeof(struct ice_tx_desc),
		     PAGE_SIZE);
	tx_ring->desc = dmam_alloc_coherent(dev, size, &tx_ring->dma,
					    GFP_KERNEL);
	if (!tx_ring->desc) {
		dev_err(dev, "Unable to allocate memory for the Tx descriptor ring, size=%d\n",
			size);
		goto err;
	}

	tx_ring->next_to_use = 0;
	tx_ring->next_to_clean = 0;
	tx_ring->ring_stats->tx_stats.prev_pkt = -1;
	return 0;

err:
	devm_kfree(dev, tx_ring->tx_buf);
	tx_ring->tx_buf = NULL;
	return -ENOMEM;
}

 
void ice_clean_rx_ring(struct ice_rx_ring *rx_ring)
{
	struct xdp_buff *xdp = &rx_ring->xdp;
	struct device *dev = rx_ring->dev;
	u32 size;
	u16 i;

	 
	if (!rx_ring->rx_buf)
		return;

	if (rx_ring->xsk_pool) {
		ice_xsk_clean_rx_ring(rx_ring);
		goto rx_skip_free;
	}

	if (xdp->data) {
		xdp_return_buff(xdp);
		xdp->data = NULL;
	}

	 
	for (i = 0; i < rx_ring->count; i++) {
		struct ice_rx_buf *rx_buf = &rx_ring->rx_buf[i];

		if (!rx_buf->page)
			continue;

		 
		dma_sync_single_range_for_cpu(dev, rx_buf->dma,
					      rx_buf->page_offset,
					      rx_ring->rx_buf_len,
					      DMA_FROM_DEVICE);

		 
		dma_unmap_page_attrs(dev, rx_buf->dma, ice_rx_pg_size(rx_ring),
				     DMA_FROM_DEVICE, ICE_RX_DMA_ATTR);
		__page_frag_cache_drain(rx_buf->page, rx_buf->pagecnt_bias);

		rx_buf->page = NULL;
		rx_buf->page_offset = 0;
	}

rx_skip_free:
	if (rx_ring->xsk_pool)
		memset(rx_ring->xdp_buf, 0, array_size(rx_ring->count, sizeof(*rx_ring->xdp_buf)));
	else
		memset(rx_ring->rx_buf, 0, array_size(rx_ring->count, sizeof(*rx_ring->rx_buf)));

	 
	size = ALIGN(rx_ring->count * sizeof(union ice_32byte_rx_desc),
		     PAGE_SIZE);
	memset(rx_ring->desc, 0, size);

	rx_ring->next_to_alloc = 0;
	rx_ring->next_to_clean = 0;
	rx_ring->first_desc = 0;
	rx_ring->next_to_use = 0;
}

 
void ice_free_rx_ring(struct ice_rx_ring *rx_ring)
{
	u32 size;

	ice_clean_rx_ring(rx_ring);
	if (rx_ring->vsi->type == ICE_VSI_PF)
		if (xdp_rxq_info_is_reg(&rx_ring->xdp_rxq))
			xdp_rxq_info_unreg(&rx_ring->xdp_rxq);
	rx_ring->xdp_prog = NULL;
	if (rx_ring->xsk_pool) {
		kfree(rx_ring->xdp_buf);
		rx_ring->xdp_buf = NULL;
	} else {
		kfree(rx_ring->rx_buf);
		rx_ring->rx_buf = NULL;
	}

	if (rx_ring->desc) {
		size = ALIGN(rx_ring->count * sizeof(union ice_32byte_rx_desc),
			     PAGE_SIZE);
		dmam_free_coherent(rx_ring->dev, size,
				   rx_ring->desc, rx_ring->dma);
		rx_ring->desc = NULL;
	}
}

 
int ice_setup_rx_ring(struct ice_rx_ring *rx_ring)
{
	struct device *dev = rx_ring->dev;
	u32 size;

	if (!dev)
		return -ENOMEM;

	 
	WARN_ON(rx_ring->rx_buf);
	rx_ring->rx_buf =
		kcalloc(rx_ring->count, sizeof(*rx_ring->rx_buf), GFP_KERNEL);
	if (!rx_ring->rx_buf)
		return -ENOMEM;

	 
	size = ALIGN(rx_ring->count * sizeof(union ice_32byte_rx_desc),
		     PAGE_SIZE);
	rx_ring->desc = dmam_alloc_coherent(dev, size, &rx_ring->dma,
					    GFP_KERNEL);
	if (!rx_ring->desc) {
		dev_err(dev, "Unable to allocate memory for the Rx descriptor ring, size=%d\n",
			size);
		goto err;
	}

	rx_ring->next_to_use = 0;
	rx_ring->next_to_clean = 0;
	rx_ring->first_desc = 0;

	if (ice_is_xdp_ena_vsi(rx_ring->vsi))
		WRITE_ONCE(rx_ring->xdp_prog, rx_ring->vsi->xdp_prog);

	if (rx_ring->vsi->type == ICE_VSI_PF &&
	    !xdp_rxq_info_is_reg(&rx_ring->xdp_rxq))
		if (xdp_rxq_info_reg(&rx_ring->xdp_rxq, rx_ring->netdev,
				     rx_ring->q_index, rx_ring->q_vector->napi.napi_id))
			goto err;
	return 0;

err:
	kfree(rx_ring->rx_buf);
	rx_ring->rx_buf = NULL;
	return -ENOMEM;
}

 
static unsigned int
ice_rx_frame_truesize(struct ice_rx_ring *rx_ring, const unsigned int size)
{
	unsigned int truesize;

#if (PAGE_SIZE < 8192)
	truesize = ice_rx_pg_size(rx_ring) / 2;  
#else
	truesize = rx_ring->rx_offset ?
		SKB_DATA_ALIGN(rx_ring->rx_offset + size) +
		SKB_DATA_ALIGN(sizeof(struct skb_shared_info)) :
		SKB_DATA_ALIGN(size);
#endif
	return truesize;
}

 
static void
ice_run_xdp(struct ice_rx_ring *rx_ring, struct xdp_buff *xdp,
	    struct bpf_prog *xdp_prog, struct ice_tx_ring *xdp_ring,
	    struct ice_rx_buf *rx_buf)
{
	unsigned int ret = ICE_XDP_PASS;
	u32 act;

	if (!xdp_prog)
		goto exit;

	act = bpf_prog_run_xdp(xdp_prog, xdp);
	switch (act) {
	case XDP_PASS:
		break;
	case XDP_TX:
		if (static_branch_unlikely(&ice_xdp_locking_key))
			spin_lock(&xdp_ring->tx_lock);
		ret = __ice_xmit_xdp_ring(xdp, xdp_ring, false);
		if (static_branch_unlikely(&ice_xdp_locking_key))
			spin_unlock(&xdp_ring->tx_lock);
		if (ret == ICE_XDP_CONSUMED)
			goto out_failure;
		break;
	case XDP_REDIRECT:
		if (xdp_do_redirect(rx_ring->netdev, xdp, xdp_prog))
			goto out_failure;
		ret = ICE_XDP_REDIR;
		break;
	default:
		bpf_warn_invalid_xdp_action(rx_ring->netdev, xdp_prog, act);
		fallthrough;
	case XDP_ABORTED:
out_failure:
		trace_xdp_exception(rx_ring->netdev, xdp_prog, act);
		fallthrough;
	case XDP_DROP:
		ret = ICE_XDP_CONSUMED;
	}
exit:
	rx_buf->act = ret;
	if (unlikely(xdp_buff_has_frags(xdp)))
		ice_set_rx_bufs_act(xdp, rx_ring, ret);
}

 
static int ice_xmit_xdp_ring(const struct xdp_frame *xdpf,
			     struct ice_tx_ring *xdp_ring)
{
	struct xdp_buff xdp;

	xdp.data_hard_start = (void *)xdpf;
	xdp.data = xdpf->data;
	xdp.data_end = xdp.data + xdpf->len;
	xdp.frame_sz = xdpf->frame_sz;
	xdp.flags = xdpf->flags;

	return __ice_xmit_xdp_ring(&xdp, xdp_ring, true);
}

 
int
ice_xdp_xmit(struct net_device *dev, int n, struct xdp_frame **frames,
	     u32 flags)
{
	struct ice_netdev_priv *np = netdev_priv(dev);
	unsigned int queue_index = smp_processor_id();
	struct ice_vsi *vsi = np->vsi;
	struct ice_tx_ring *xdp_ring;
	struct ice_tx_buf *tx_buf;
	int nxmit = 0, i;

	if (test_bit(ICE_VSI_DOWN, vsi->state))
		return -ENETDOWN;

	if (!ice_is_xdp_ena_vsi(vsi))
		return -ENXIO;

	if (unlikely(flags & ~XDP_XMIT_FLAGS_MASK))
		return -EINVAL;

	if (static_branch_unlikely(&ice_xdp_locking_key)) {
		queue_index %= vsi->num_xdp_txq;
		xdp_ring = vsi->xdp_rings[queue_index];
		spin_lock(&xdp_ring->tx_lock);
	} else {
		 
		if (unlikely(queue_index >= vsi->num_xdp_txq))
			return -ENXIO;
		xdp_ring = vsi->xdp_rings[queue_index];
	}

	tx_buf = &xdp_ring->tx_buf[xdp_ring->next_to_use];
	for (i = 0; i < n; i++) {
		const struct xdp_frame *xdpf = frames[i];
		int err;

		err = ice_xmit_xdp_ring(xdpf, xdp_ring);
		if (err != ICE_XDP_TX)
			break;
		nxmit++;
	}

	tx_buf->rs_idx = ice_set_rs_bit(xdp_ring);
	if (unlikely(flags & XDP_XMIT_FLUSH))
		ice_xdp_ring_update_tail(xdp_ring);

	if (static_branch_unlikely(&ice_xdp_locking_key))
		spin_unlock(&xdp_ring->tx_lock);

	return nxmit;
}

 
static bool
ice_alloc_mapped_page(struct ice_rx_ring *rx_ring, struct ice_rx_buf *bi)
{
	struct page *page = bi->page;
	dma_addr_t dma;

	 
	if (likely(page))
		return true;

	 
	page = dev_alloc_pages(ice_rx_pg_order(rx_ring));
	if (unlikely(!page)) {
		rx_ring->ring_stats->rx_stats.alloc_page_failed++;
		return false;
	}

	 
	dma = dma_map_page_attrs(rx_ring->dev, page, 0, ice_rx_pg_size(rx_ring),
				 DMA_FROM_DEVICE, ICE_RX_DMA_ATTR);

	 
	if (dma_mapping_error(rx_ring->dev, dma)) {
		__free_pages(page, ice_rx_pg_order(rx_ring));
		rx_ring->ring_stats->rx_stats.alloc_page_failed++;
		return false;
	}

	bi->dma = dma;
	bi->page = page;
	bi->page_offset = rx_ring->rx_offset;
	page_ref_add(page, USHRT_MAX - 1);
	bi->pagecnt_bias = USHRT_MAX;

	return true;
}

 
bool ice_alloc_rx_bufs(struct ice_rx_ring *rx_ring, unsigned int cleaned_count)
{
	union ice_32b_rx_flex_desc *rx_desc;
	u16 ntu = rx_ring->next_to_use;
	struct ice_rx_buf *bi;

	 
	if ((!rx_ring->netdev && rx_ring->vsi->type != ICE_VSI_CTRL) ||
	    !cleaned_count)
		return false;

	 
	rx_desc = ICE_RX_DESC(rx_ring, ntu);
	bi = &rx_ring->rx_buf[ntu];

	do {
		 
		if (!ice_alloc_mapped_page(rx_ring, bi))
			break;

		 
		dma_sync_single_range_for_device(rx_ring->dev, bi->dma,
						 bi->page_offset,
						 rx_ring->rx_buf_len,
						 DMA_FROM_DEVICE);

		 
		rx_desc->read.pkt_addr = cpu_to_le64(bi->dma + bi->page_offset);

		rx_desc++;
		bi++;
		ntu++;
		if (unlikely(ntu == rx_ring->count)) {
			rx_desc = ICE_RX_DESC(rx_ring, 0);
			bi = rx_ring->rx_buf;
			ntu = 0;
		}

		 
		rx_desc->wb.status_error0 = 0;

		cleaned_count--;
	} while (cleaned_count);

	if (rx_ring->next_to_use != ntu)
		ice_release_rx_desc(rx_ring, ntu);

	return !!cleaned_count;
}

 
static void
ice_rx_buf_adjust_pg_offset(struct ice_rx_buf *rx_buf, unsigned int size)
{
#if (PAGE_SIZE < 8192)
	 
	rx_buf->page_offset ^= size;
#else
	 
	rx_buf->page_offset += size;
#endif
}

 
static bool
ice_can_reuse_rx_page(struct ice_rx_buf *rx_buf)
{
	unsigned int pagecnt_bias = rx_buf->pagecnt_bias;
	struct page *page = rx_buf->page;

	 
	if (!dev_page_is_reusable(page))
		return false;

#if (PAGE_SIZE < 8192)
	 
	if (unlikely(rx_buf->pgcnt - pagecnt_bias > 1))
		return false;
#else
#define ICE_LAST_OFFSET \
	(SKB_WITH_OVERHEAD(PAGE_SIZE) - ICE_RXBUF_2048)
	if (rx_buf->page_offset > ICE_LAST_OFFSET)
		return false;
#endif  

	 
	if (unlikely(pagecnt_bias == 1)) {
		page_ref_add(page, USHRT_MAX - 1);
		rx_buf->pagecnt_bias = USHRT_MAX;
	}

	return true;
}

 
static int
ice_add_xdp_frag(struct ice_rx_ring *rx_ring, struct xdp_buff *xdp,
		 struct ice_rx_buf *rx_buf, const unsigned int size)
{
	struct skb_shared_info *sinfo = xdp_get_shared_info_from_buff(xdp);

	if (!size)
		return 0;

	if (!xdp_buff_has_frags(xdp)) {
		sinfo->nr_frags = 0;
		sinfo->xdp_frags_size = 0;
		xdp_buff_set_frags_flag(xdp);
	}

	if (unlikely(sinfo->nr_frags == MAX_SKB_FRAGS)) {
		if (unlikely(xdp_buff_has_frags(xdp)))
			ice_set_rx_bufs_act(xdp, rx_ring, ICE_XDP_CONSUMED);
		return -ENOMEM;
	}

	__skb_fill_page_desc_noacc(sinfo, sinfo->nr_frags++, rx_buf->page,
				   rx_buf->page_offset, size);
	sinfo->xdp_frags_size += size;

	if (page_is_pfmemalloc(rx_buf->page))
		xdp_buff_set_frag_pfmemalloc(xdp);

	return 0;
}

 
static void
ice_reuse_rx_page(struct ice_rx_ring *rx_ring, struct ice_rx_buf *old_buf)
{
	u16 nta = rx_ring->next_to_alloc;
	struct ice_rx_buf *new_buf;

	new_buf = &rx_ring->rx_buf[nta];

	 
	nta++;
	rx_ring->next_to_alloc = (nta < rx_ring->count) ? nta : 0;

	 
	new_buf->dma = old_buf->dma;
	new_buf->page = old_buf->page;
	new_buf->page_offset = old_buf->page_offset;
	new_buf->pagecnt_bias = old_buf->pagecnt_bias;
}

 
static struct ice_rx_buf *
ice_get_rx_buf(struct ice_rx_ring *rx_ring, const unsigned int size,
	       const unsigned int ntc)
{
	struct ice_rx_buf *rx_buf;

	rx_buf = &rx_ring->rx_buf[ntc];
	rx_buf->pgcnt =
#if (PAGE_SIZE < 8192)
		page_count(rx_buf->page);
#else
		0;
#endif
	prefetchw(rx_buf->page);

	if (!size)
		return rx_buf;
	 
	dma_sync_single_range_for_cpu(rx_ring->dev, rx_buf->dma,
				      rx_buf->page_offset, size,
				      DMA_FROM_DEVICE);

	 
	rx_buf->pagecnt_bias--;

	return rx_buf;
}

 
static struct sk_buff *
ice_build_skb(struct ice_rx_ring *rx_ring, struct xdp_buff *xdp)
{
	u8 metasize = xdp->data - xdp->data_meta;
	struct skb_shared_info *sinfo = NULL;
	unsigned int nr_frags;
	struct sk_buff *skb;

	if (unlikely(xdp_buff_has_frags(xdp))) {
		sinfo = xdp_get_shared_info_from_buff(xdp);
		nr_frags = sinfo->nr_frags;
	}

	 
	net_prefetch(xdp->data_meta);
	 
	skb = napi_build_skb(xdp->data_hard_start, xdp->frame_sz);
	if (unlikely(!skb))
		return NULL;

	 
	skb_record_rx_queue(skb, rx_ring->q_index);

	 
	skb_reserve(skb, xdp->data - xdp->data_hard_start);
	__skb_put(skb, xdp->data_end - xdp->data);
	if (metasize)
		skb_metadata_set(skb, metasize);

	if (unlikely(xdp_buff_has_frags(xdp)))
		xdp_update_skb_shared_info(skb, nr_frags,
					   sinfo->xdp_frags_size,
					   nr_frags * xdp->frame_sz,
					   xdp_buff_is_frag_pfmemalloc(xdp));

	return skb;
}

 
static struct sk_buff *
ice_construct_skb(struct ice_rx_ring *rx_ring, struct xdp_buff *xdp)
{
	unsigned int size = xdp->data_end - xdp->data;
	struct skb_shared_info *sinfo = NULL;
	struct ice_rx_buf *rx_buf;
	unsigned int nr_frags = 0;
	unsigned int headlen;
	struct sk_buff *skb;

	 
	net_prefetch(xdp->data);

	if (unlikely(xdp_buff_has_frags(xdp))) {
		sinfo = xdp_get_shared_info_from_buff(xdp);
		nr_frags = sinfo->nr_frags;
	}

	 
	skb = __napi_alloc_skb(&rx_ring->q_vector->napi, ICE_RX_HDR_SIZE,
			       GFP_ATOMIC | __GFP_NOWARN);
	if (unlikely(!skb))
		return NULL;

	rx_buf = &rx_ring->rx_buf[rx_ring->first_desc];
	skb_record_rx_queue(skb, rx_ring->q_index);
	 
	headlen = size;
	if (headlen > ICE_RX_HDR_SIZE)
		headlen = eth_get_headlen(skb->dev, xdp->data, ICE_RX_HDR_SIZE);

	 
	memcpy(__skb_put(skb, headlen), xdp->data, ALIGN(headlen,
							 sizeof(long)));

	 
	size -= headlen;
	if (size) {
		 
		if (unlikely(nr_frags >= MAX_SKB_FRAGS - 1)) {
			dev_kfree_skb(skb);
			return NULL;
		}
		skb_add_rx_frag(skb, 0, rx_buf->page,
				rx_buf->page_offset + headlen, size,
				xdp->frame_sz);
	} else {
		 
		rx_buf->act = ICE_SKB_CONSUMED;
	}

	if (unlikely(xdp_buff_has_frags(xdp))) {
		struct skb_shared_info *skinfo = skb_shinfo(skb);

		memcpy(&skinfo->frags[skinfo->nr_frags], &sinfo->frags[0],
		       sizeof(skb_frag_t) * nr_frags);

		xdp_update_skb_shared_info(skb, skinfo->nr_frags + nr_frags,
					   sinfo->xdp_frags_size,
					   nr_frags * xdp->frame_sz,
					   xdp_buff_is_frag_pfmemalloc(xdp));
	}

	return skb;
}

 
static void
ice_put_rx_buf(struct ice_rx_ring *rx_ring, struct ice_rx_buf *rx_buf)
{
	if (!rx_buf)
		return;

	if (ice_can_reuse_rx_page(rx_buf)) {
		 
		ice_reuse_rx_page(rx_ring, rx_buf);
	} else {
		 
		dma_unmap_page_attrs(rx_ring->dev, rx_buf->dma,
				     ice_rx_pg_size(rx_ring), DMA_FROM_DEVICE,
				     ICE_RX_DMA_ATTR);
		__page_frag_cache_drain(rx_buf->page, rx_buf->pagecnt_bias);
	}

	 
	rx_buf->page = NULL;
}

 
int ice_clean_rx_irq(struct ice_rx_ring *rx_ring, int budget)
{
	unsigned int total_rx_bytes = 0, total_rx_pkts = 0;
	unsigned int offset = rx_ring->rx_offset;
	struct xdp_buff *xdp = &rx_ring->xdp;
	u32 cached_ntc = rx_ring->first_desc;
	struct ice_tx_ring *xdp_ring = NULL;
	struct bpf_prog *xdp_prog = NULL;
	u32 ntc = rx_ring->next_to_clean;
	u32 cnt = rx_ring->count;
	u32 xdp_xmit = 0;
	u32 cached_ntu;
	bool failure;
	u32 first;

	 
#if (PAGE_SIZE < 8192)
	xdp->frame_sz = ice_rx_frame_truesize(rx_ring, 0);
#endif

	xdp_prog = READ_ONCE(rx_ring->xdp_prog);
	if (xdp_prog) {
		xdp_ring = rx_ring->xdp_ring;
		cached_ntu = xdp_ring->next_to_use;
	}

	 
	while (likely(total_rx_pkts < (unsigned int)budget)) {
		union ice_32b_rx_flex_desc *rx_desc;
		struct ice_rx_buf *rx_buf;
		struct sk_buff *skb;
		unsigned int size;
		u16 stat_err_bits;
		u16 vlan_tag = 0;
		u16 rx_ptype;

		 
		rx_desc = ICE_RX_DESC(rx_ring, ntc);

		 
		stat_err_bits = BIT(ICE_RX_FLEX_DESC_STATUS0_DD_S);
		if (!ice_test_staterr(rx_desc->wb.status_error0, stat_err_bits))
			break;

		 
		dma_rmb();

		ice_trace(clean_rx_irq, rx_ring, rx_desc);
		if (rx_desc->wb.rxdid == FDIR_DESC_RXDID || !rx_ring->netdev) {
			struct ice_vsi *ctrl_vsi = rx_ring->vsi;

			if (rx_desc->wb.rxdid == FDIR_DESC_RXDID &&
			    ctrl_vsi->vf)
				ice_vc_fdir_irq_handler(ctrl_vsi, rx_desc);
			if (++ntc == cnt)
				ntc = 0;
			rx_ring->first_desc = ntc;
			continue;
		}

		size = le16_to_cpu(rx_desc->wb.pkt_len) &
			ICE_RX_FLX_DESC_PKT_LEN_M;

		 
		rx_buf = ice_get_rx_buf(rx_ring, size, ntc);

		if (!xdp->data) {
			void *hard_start;

			hard_start = page_address(rx_buf->page) + rx_buf->page_offset -
				     offset;
			xdp_prepare_buff(xdp, hard_start, offset, size, !!offset);
#if (PAGE_SIZE > 4096)
			 
			xdp->frame_sz = ice_rx_frame_truesize(rx_ring, size);
#endif
			xdp_buff_clear_frags_flag(xdp);
		} else if (ice_add_xdp_frag(rx_ring, xdp, rx_buf, size)) {
			break;
		}
		if (++ntc == cnt)
			ntc = 0;

		 
		if (ice_is_non_eop(rx_ring, rx_desc))
			continue;

		ice_run_xdp(rx_ring, xdp, xdp_prog, xdp_ring, rx_buf);
		if (rx_buf->act == ICE_XDP_PASS)
			goto construct_skb;
		total_rx_bytes += xdp_get_buff_len(xdp);
		total_rx_pkts++;

		xdp->data = NULL;
		rx_ring->first_desc = ntc;
		continue;
construct_skb:
		if (likely(ice_ring_uses_build_skb(rx_ring)))
			skb = ice_build_skb(rx_ring, xdp);
		else
			skb = ice_construct_skb(rx_ring, xdp);
		 
		if (!skb) {
			rx_ring->ring_stats->rx_stats.alloc_page_failed++;
			rx_buf->act = ICE_XDP_CONSUMED;
			if (unlikely(xdp_buff_has_frags(xdp)))
				ice_set_rx_bufs_act(xdp, rx_ring,
						    ICE_XDP_CONSUMED);
			xdp->data = NULL;
			rx_ring->first_desc = ntc;
			break;
		}
		xdp->data = NULL;
		rx_ring->first_desc = ntc;

		stat_err_bits = BIT(ICE_RX_FLEX_DESC_STATUS0_RXE_S);
		if (unlikely(ice_test_staterr(rx_desc->wb.status_error0,
					      stat_err_bits))) {
			dev_kfree_skb_any(skb);
			continue;
		}

		vlan_tag = ice_get_vlan_tag_from_rx_desc(rx_desc);

		 
		if (eth_skb_pad(skb))
			continue;

		 
		total_rx_bytes += skb->len;

		 
		rx_ptype = le16_to_cpu(rx_desc->wb.ptype_flex_flags0) &
			ICE_RX_FLEX_DESC_PTYPE_M;

		ice_process_skb_fields(rx_ring, rx_desc, skb, rx_ptype);

		ice_trace(clean_rx_irq_indicate, rx_ring, rx_desc, skb);
		 
		ice_receive_skb(rx_ring, skb, vlan_tag);

		 
		total_rx_pkts++;
	}

	first = rx_ring->first_desc;
	while (cached_ntc != first) {
		struct ice_rx_buf *buf = &rx_ring->rx_buf[cached_ntc];

		if (buf->act & (ICE_XDP_TX | ICE_XDP_REDIR)) {
			ice_rx_buf_adjust_pg_offset(buf, xdp->frame_sz);
			xdp_xmit |= buf->act;
		} else if (buf->act & ICE_XDP_CONSUMED) {
			buf->pagecnt_bias++;
		} else if (buf->act == ICE_XDP_PASS) {
			ice_rx_buf_adjust_pg_offset(buf, xdp->frame_sz);
		}

		ice_put_rx_buf(rx_ring, buf);
		if (++cached_ntc >= cnt)
			cached_ntc = 0;
	}
	rx_ring->next_to_clean = ntc;
	 
	failure = ice_alloc_rx_bufs(rx_ring, ICE_RX_DESC_UNUSED(rx_ring));

	if (xdp_xmit)
		ice_finalize_xdp_rx(xdp_ring, xdp_xmit, cached_ntu);

	if (rx_ring->ring_stats)
		ice_update_rx_ring_stats(rx_ring, total_rx_pkts,
					 total_rx_bytes);

	 
	return failure ? budget : (int)total_rx_pkts;
}

static void __ice_update_sample(struct ice_q_vector *q_vector,
				struct ice_ring_container *rc,
				struct dim_sample *sample,
				bool is_tx)
{
	u64 packets = 0, bytes = 0;

	if (is_tx) {
		struct ice_tx_ring *tx_ring;

		ice_for_each_tx_ring(tx_ring, *rc) {
			struct ice_ring_stats *ring_stats;

			ring_stats = tx_ring->ring_stats;
			if (!ring_stats)
				continue;
			packets += ring_stats->stats.pkts;
			bytes += ring_stats->stats.bytes;
		}
	} else {
		struct ice_rx_ring *rx_ring;

		ice_for_each_rx_ring(rx_ring, *rc) {
			struct ice_ring_stats *ring_stats;

			ring_stats = rx_ring->ring_stats;
			if (!ring_stats)
				continue;
			packets += ring_stats->stats.pkts;
			bytes += ring_stats->stats.bytes;
		}
	}

	dim_update_sample(q_vector->total_events, packets, bytes, sample);
	sample->comp_ctr = 0;

	 
	if (ktime_ms_delta(sample->time, rc->dim.start_sample.time) >= 1000)
		rc->dim.state = DIM_START_MEASURE;
}

 
static void ice_net_dim(struct ice_q_vector *q_vector)
{
	struct ice_ring_container *tx = &q_vector->tx;
	struct ice_ring_container *rx = &q_vector->rx;

	if (ITR_IS_DYNAMIC(tx)) {
		struct dim_sample dim_sample;

		__ice_update_sample(q_vector, tx, &dim_sample, true);
		net_dim(&tx->dim, dim_sample);
	}

	if (ITR_IS_DYNAMIC(rx)) {
		struct dim_sample dim_sample;

		__ice_update_sample(q_vector, rx, &dim_sample, false);
		net_dim(&rx->dim, dim_sample);
	}
}

 
static u32 ice_buildreg_itr(u16 itr_idx, u16 itr)
{
	 
	itr &= ICE_ITR_MASK;

	return GLINT_DYN_CTL_INTENA_M | GLINT_DYN_CTL_CLEARPBA_M |
		(itr_idx << GLINT_DYN_CTL_ITR_INDX_S) |
		(itr << (GLINT_DYN_CTL_INTERVAL_S - ICE_ITR_GRAN_S));
}

 
static void ice_enable_interrupt(struct ice_q_vector *q_vector)
{
	struct ice_vsi *vsi = q_vector->vsi;
	bool wb_en = q_vector->wb_on_itr;
	u32 itr_val;

	if (test_bit(ICE_DOWN, vsi->state))
		return;

	 
	if (!wb_en) {
		itr_val = ice_buildreg_itr(ICE_ITR_NONE, 0);
	} else {
		q_vector->wb_on_itr = false;

		 
		itr_val = ice_buildreg_itr(ICE_IDX_ITR2, ICE_ITR_20K);
		itr_val |= GLINT_DYN_CTL_SWINT_TRIG_M |
			   ICE_IDX_ITR2 << GLINT_DYN_CTL_SW_ITR_INDX_S |
			   GLINT_DYN_CTL_SW_ITR_INDX_ENA_M;
	}
	wr32(&vsi->back->hw, GLINT_DYN_CTL(q_vector->reg_idx), itr_val);
}

 
static void ice_set_wb_on_itr(struct ice_q_vector *q_vector)
{
	struct ice_vsi *vsi = q_vector->vsi;

	 
	if (q_vector->wb_on_itr)
		return;

	 
	wr32(&vsi->back->hw, GLINT_DYN_CTL(q_vector->reg_idx),
	     ((ICE_ITR_NONE << GLINT_DYN_CTL_ITR_INDX_S) &
	      GLINT_DYN_CTL_ITR_INDX_M) | GLINT_DYN_CTL_INTENA_MSK_M |
	     GLINT_DYN_CTL_WB_ON_ITR_M);

	q_vector->wb_on_itr = true;
}

 
int ice_napi_poll(struct napi_struct *napi, int budget)
{
	struct ice_q_vector *q_vector =
				container_of(napi, struct ice_q_vector, napi);
	struct ice_tx_ring *tx_ring;
	struct ice_rx_ring *rx_ring;
	bool clean_complete = true;
	int budget_per_ring;
	int work_done = 0;

	 
	ice_for_each_tx_ring(tx_ring, q_vector->tx) {
		bool wd;

		if (tx_ring->xsk_pool)
			wd = ice_xmit_zc(tx_ring);
		else if (ice_ring_is_xdp(tx_ring))
			wd = true;
		else
			wd = ice_clean_tx_irq(tx_ring, budget);

		if (!wd)
			clean_complete = false;
	}

	 
	if (unlikely(budget <= 0))
		return budget;

	 
	if (unlikely(q_vector->num_ring_rx > 1))
		 
		budget_per_ring = max_t(int, budget / q_vector->num_ring_rx, 1);
	else
		 
		budget_per_ring = budget;

	ice_for_each_rx_ring(rx_ring, q_vector->rx) {
		int cleaned;

		 
		cleaned = rx_ring->xsk_pool ?
			  ice_clean_rx_irq_zc(rx_ring, budget_per_ring) :
			  ice_clean_rx_irq(rx_ring, budget_per_ring);
		work_done += cleaned;
		 
		if (cleaned >= budget_per_ring)
			clean_complete = false;
	}

	 
	if (!clean_complete) {
		 
		ice_set_wb_on_itr(q_vector);
		return budget;
	}

	 
	if (napi_complete_done(napi, work_done)) {
		ice_net_dim(q_vector);
		ice_enable_interrupt(q_vector);
	} else {
		ice_set_wb_on_itr(q_vector);
	}

	return min_t(int, work_done, budget - 1);
}

 
static int __ice_maybe_stop_tx(struct ice_tx_ring *tx_ring, unsigned int size)
{
	netif_tx_stop_queue(txring_txq(tx_ring));
	 
	smp_mb();

	 
	if (likely(ICE_DESC_UNUSED(tx_ring) < size))
		return -EBUSY;

	 
	netif_tx_start_queue(txring_txq(tx_ring));
	++tx_ring->ring_stats->tx_stats.restart_q;
	return 0;
}

 
static int ice_maybe_stop_tx(struct ice_tx_ring *tx_ring, unsigned int size)
{
	if (likely(ICE_DESC_UNUSED(tx_ring) >= size))
		return 0;

	return __ice_maybe_stop_tx(tx_ring, size);
}

 
static void
ice_tx_map(struct ice_tx_ring *tx_ring, struct ice_tx_buf *first,
	   struct ice_tx_offload_params *off)
{
	u64 td_offset, td_tag, td_cmd;
	u16 i = tx_ring->next_to_use;
	unsigned int data_len, size;
	struct ice_tx_desc *tx_desc;
	struct ice_tx_buf *tx_buf;
	struct sk_buff *skb;
	skb_frag_t *frag;
	dma_addr_t dma;
	bool kick;

	td_tag = off->td_l2tag1;
	td_cmd = off->td_cmd;
	td_offset = off->td_offset;
	skb = first->skb;

	data_len = skb->data_len;
	size = skb_headlen(skb);

	tx_desc = ICE_TX_DESC(tx_ring, i);

	if (first->tx_flags & ICE_TX_FLAGS_HW_VLAN) {
		td_cmd |= (u64)ICE_TX_DESC_CMD_IL2TAG1;
		td_tag = first->vid;
	}

	dma = dma_map_single(tx_ring->dev, skb->data, size, DMA_TO_DEVICE);

	tx_buf = first;

	for (frag = &skb_shinfo(skb)->frags[0];; frag++) {
		unsigned int max_data = ICE_MAX_DATA_PER_TXD_ALIGNED;

		if (dma_mapping_error(tx_ring->dev, dma))
			goto dma_error;

		 
		dma_unmap_len_set(tx_buf, len, size);
		dma_unmap_addr_set(tx_buf, dma, dma);

		 
		max_data += -dma & (ICE_MAX_READ_REQ_SIZE - 1);
		tx_desc->buf_addr = cpu_to_le64(dma);

		 
		while (unlikely(size > ICE_MAX_DATA_PER_TXD)) {
			tx_desc->cmd_type_offset_bsz =
				ice_build_ctob(td_cmd, td_offset, max_data,
					       td_tag);

			tx_desc++;
			i++;

			if (i == tx_ring->count) {
				tx_desc = ICE_TX_DESC(tx_ring, 0);
				i = 0;
			}

			dma += max_data;
			size -= max_data;

			max_data = ICE_MAX_DATA_PER_TXD_ALIGNED;
			tx_desc->buf_addr = cpu_to_le64(dma);
		}

		if (likely(!data_len))
			break;

		tx_desc->cmd_type_offset_bsz = ice_build_ctob(td_cmd, td_offset,
							      size, td_tag);

		tx_desc++;
		i++;

		if (i == tx_ring->count) {
			tx_desc = ICE_TX_DESC(tx_ring, 0);
			i = 0;
		}

		size = skb_frag_size(frag);
		data_len -= size;

		dma = skb_frag_dma_map(tx_ring->dev, frag, 0, size,
				       DMA_TO_DEVICE);

		tx_buf = &tx_ring->tx_buf[i];
		tx_buf->type = ICE_TX_BUF_FRAG;
	}

	 
	skb_tx_timestamp(first->skb);

	i++;
	if (i == tx_ring->count)
		i = 0;

	 
	td_cmd |= (u64)ICE_TXD_LAST_DESC_CMD;
	tx_desc->cmd_type_offset_bsz =
			ice_build_ctob(td_cmd, td_offset, size, td_tag);

	 
	wmb();

	 
	first->next_to_watch = tx_desc;

	tx_ring->next_to_use = i;

	ice_maybe_stop_tx(tx_ring, DESC_NEEDED);

	 
	kick = __netdev_tx_sent_queue(txring_txq(tx_ring), first->bytecount,
				      netdev_xmit_more());
	if (kick)
		 
		writel(i, tx_ring->tail);

	return;

dma_error:
	 
	for (;;) {
		tx_buf = &tx_ring->tx_buf[i];
		ice_unmap_and_free_tx_buf(tx_ring, tx_buf);
		if (tx_buf == first)
			break;
		if (i == 0)
			i = tx_ring->count;
		i--;
	}

	tx_ring->next_to_use = i;
}

 
static
int ice_tx_csum(struct ice_tx_buf *first, struct ice_tx_offload_params *off)
{
	u32 l4_len = 0, l3_len = 0, l2_len = 0;
	struct sk_buff *skb = first->skb;
	union {
		struct iphdr *v4;
		struct ipv6hdr *v6;
		unsigned char *hdr;
	} ip;
	union {
		struct tcphdr *tcp;
		unsigned char *hdr;
	} l4;
	__be16 frag_off, protocol;
	unsigned char *exthdr;
	u32 offset, cmd = 0;
	u8 l4_proto = 0;

	if (skb->ip_summed != CHECKSUM_PARTIAL)
		return 0;

	protocol = vlan_get_protocol(skb);

	if (eth_p_mpls(protocol)) {
		ip.hdr = skb_inner_network_header(skb);
		l4.hdr = skb_checksum_start(skb);
	} else {
		ip.hdr = skb_network_header(skb);
		l4.hdr = skb_transport_header(skb);
	}

	 
	l2_len = ip.hdr - skb->data;
	offset = (l2_len / 2) << ICE_TX_DESC_LEN_MACLEN_S;

	 
	if (ip.v4->version == 4)
		first->tx_flags |= ICE_TX_FLAGS_IPV4;
	else if (ip.v6->version == 6)
		first->tx_flags |= ICE_TX_FLAGS_IPV6;

	if (skb->encapsulation) {
		bool gso_ena = false;
		u32 tunnel = 0;

		 
		if (first->tx_flags & ICE_TX_FLAGS_IPV4) {
			tunnel |= (first->tx_flags & ICE_TX_FLAGS_TSO) ?
				  ICE_TX_CTX_EIPT_IPV4 :
				  ICE_TX_CTX_EIPT_IPV4_NO_CSUM;
			l4_proto = ip.v4->protocol;
		} else if (first->tx_flags & ICE_TX_FLAGS_IPV6) {
			int ret;

			tunnel |= ICE_TX_CTX_EIPT_IPV6;
			exthdr = ip.hdr + sizeof(*ip.v6);
			l4_proto = ip.v6->nexthdr;
			ret = ipv6_skip_exthdr(skb, exthdr - skb->data,
					       &l4_proto, &frag_off);
			if (ret < 0)
				return -1;
		}

		 
		switch (l4_proto) {
		case IPPROTO_UDP:
			tunnel |= ICE_TXD_CTX_UDP_TUNNELING;
			first->tx_flags |= ICE_TX_FLAGS_TUNNEL;
			break;
		case IPPROTO_GRE:
			tunnel |= ICE_TXD_CTX_GRE_TUNNELING;
			first->tx_flags |= ICE_TX_FLAGS_TUNNEL;
			break;
		case IPPROTO_IPIP:
		case IPPROTO_IPV6:
			first->tx_flags |= ICE_TX_FLAGS_TUNNEL;
			l4.hdr = skb_inner_network_header(skb);
			break;
		default:
			if (first->tx_flags & ICE_TX_FLAGS_TSO)
				return -1;

			skb_checksum_help(skb);
			return 0;
		}

		 
		tunnel |= ((l4.hdr - ip.hdr) / 4) <<
			  ICE_TXD_CTX_QW0_EIPLEN_S;

		 
		ip.hdr = skb_inner_network_header(skb);

		 
		tunnel |= ((ip.hdr - l4.hdr) / 2) <<
			   ICE_TXD_CTX_QW0_NATLEN_S;

		gso_ena = skb_shinfo(skb)->gso_type & SKB_GSO_PARTIAL;
		 
		if ((first->tx_flags & ICE_TX_FLAGS_TSO) && !gso_ena &&
		    (skb_shinfo(skb)->gso_type & SKB_GSO_UDP_TUNNEL_CSUM))
			tunnel |= ICE_TXD_CTX_QW0_L4T_CS_M;

		 
		off->cd_tunnel_params |= tunnel;

		 
		off->cd_qw1 |= (u64)ICE_TX_DESC_DTYPE_CTX;

		 
		l4.hdr = skb_inner_transport_header(skb);
		l4_proto = 0;

		 
		first->tx_flags &= ~(ICE_TX_FLAGS_IPV4 | ICE_TX_FLAGS_IPV6);
		if (ip.v4->version == 4)
			first->tx_flags |= ICE_TX_FLAGS_IPV4;
		if (ip.v6->version == 6)
			first->tx_flags |= ICE_TX_FLAGS_IPV6;
	}

	 
	if (first->tx_flags & ICE_TX_FLAGS_IPV4) {
		l4_proto = ip.v4->protocol;
		 
		if (first->tx_flags & ICE_TX_FLAGS_TSO)
			cmd |= ICE_TX_DESC_CMD_IIPT_IPV4_CSUM;
		else
			cmd |= ICE_TX_DESC_CMD_IIPT_IPV4;

	} else if (first->tx_flags & ICE_TX_FLAGS_IPV6) {
		cmd |= ICE_TX_DESC_CMD_IIPT_IPV6;
		exthdr = ip.hdr + sizeof(*ip.v6);
		l4_proto = ip.v6->nexthdr;
		if (l4.hdr != exthdr)
			ipv6_skip_exthdr(skb, exthdr - skb->data, &l4_proto,
					 &frag_off);
	} else {
		return -1;
	}

	 
	l3_len = l4.hdr - ip.hdr;
	offset |= (l3_len / 4) << ICE_TX_DESC_LEN_IPLEN_S;

	 
	switch (l4_proto) {
	case IPPROTO_TCP:
		 
		cmd |= ICE_TX_DESC_CMD_L4T_EOFT_TCP;
		l4_len = l4.tcp->doff;
		offset |= l4_len << ICE_TX_DESC_LEN_L4_LEN_S;
		break;
	case IPPROTO_UDP:
		 
		cmd |= ICE_TX_DESC_CMD_L4T_EOFT_UDP;
		l4_len = (sizeof(struct udphdr) >> 2);
		offset |= l4_len << ICE_TX_DESC_LEN_L4_LEN_S;
		break;
	case IPPROTO_SCTP:
		 
		cmd |= ICE_TX_DESC_CMD_L4T_EOFT_SCTP;
		l4_len = sizeof(struct sctphdr) >> 2;
		offset |= l4_len << ICE_TX_DESC_LEN_L4_LEN_S;
		break;

	default:
		if (first->tx_flags & ICE_TX_FLAGS_TSO)
			return -1;
		skb_checksum_help(skb);
		return 0;
	}

	off->td_cmd |= cmd;
	off->td_offset |= offset;
	return 1;
}

 
static void
ice_tx_prepare_vlan_flags(struct ice_tx_ring *tx_ring, struct ice_tx_buf *first)
{
	struct sk_buff *skb = first->skb;

	 
	if (!skb_vlan_tag_present(skb) && eth_type_vlan(skb->protocol))
		return;

	 
	if (skb_vlan_tag_present(skb)) {
		first->vid = skb_vlan_tag_get(skb);
		if (tx_ring->flags & ICE_TX_FLAGS_RING_VLAN_L2TAG2)
			first->tx_flags |= ICE_TX_FLAGS_HW_OUTER_SINGLE_VLAN;
		else
			first->tx_flags |= ICE_TX_FLAGS_HW_VLAN;
	}

	ice_tx_prepare_vlan_flags_dcb(tx_ring, first);
}

 
static
int ice_tso(struct ice_tx_buf *first, struct ice_tx_offload_params *off)
{
	struct sk_buff *skb = first->skb;
	union {
		struct iphdr *v4;
		struct ipv6hdr *v6;
		unsigned char *hdr;
	} ip;
	union {
		struct tcphdr *tcp;
		struct udphdr *udp;
		unsigned char *hdr;
	} l4;
	u64 cd_mss, cd_tso_len;
	__be16 protocol;
	u32 paylen;
	u8 l4_start;
	int err;

	if (skb->ip_summed != CHECKSUM_PARTIAL)
		return 0;

	if (!skb_is_gso(skb))
		return 0;

	err = skb_cow_head(skb, 0);
	if (err < 0)
		return err;

	protocol = vlan_get_protocol(skb);

	if (eth_p_mpls(protocol))
		ip.hdr = skb_inner_network_header(skb);
	else
		ip.hdr = skb_network_header(skb);
	l4.hdr = skb_checksum_start(skb);

	 
	if (ip.v4->version == 4) {
		ip.v4->tot_len = 0;
		ip.v4->check = 0;
	} else {
		ip.v6->payload_len = 0;
	}

	if (skb_shinfo(skb)->gso_type & (SKB_GSO_GRE |
					 SKB_GSO_GRE_CSUM |
					 SKB_GSO_IPXIP4 |
					 SKB_GSO_IPXIP6 |
					 SKB_GSO_UDP_TUNNEL |
					 SKB_GSO_UDP_TUNNEL_CSUM)) {
		if (!(skb_shinfo(skb)->gso_type & SKB_GSO_PARTIAL) &&
		    (skb_shinfo(skb)->gso_type & SKB_GSO_UDP_TUNNEL_CSUM)) {
			l4.udp->len = 0;

			 
			l4_start = (u8)(l4.hdr - skb->data);

			 
			paylen = skb->len - l4_start;
			csum_replace_by_diff(&l4.udp->check,
					     (__force __wsum)htonl(paylen));
		}

		 
		ip.hdr = skb_inner_network_header(skb);
		l4.hdr = skb_inner_transport_header(skb);

		 
		if (ip.v4->version == 4) {
			ip.v4->tot_len = 0;
			ip.v4->check = 0;
		} else {
			ip.v6->payload_len = 0;
		}
	}

	 
	l4_start = (u8)(l4.hdr - skb->data);

	 
	paylen = skb->len - l4_start;

	if (skb_shinfo(skb)->gso_type & SKB_GSO_UDP_L4) {
		csum_replace_by_diff(&l4.udp->check,
				     (__force __wsum)htonl(paylen));
		 
		off->header_len = (u8)sizeof(l4.udp) + l4_start;
	} else {
		csum_replace_by_diff(&l4.tcp->check,
				     (__force __wsum)htonl(paylen));
		 
		off->header_len = (u8)((l4.tcp->doff * 4) + l4_start);
	}

	 
	first->gso_segs = skb_shinfo(skb)->gso_segs;
	first->bytecount += (first->gso_segs - 1) * off->header_len;

	cd_tso_len = skb->len - off->header_len;
	cd_mss = skb_shinfo(skb)->gso_size;

	 
	off->cd_qw1 |= (u64)(ICE_TX_DESC_DTYPE_CTX |
			     (ICE_TX_CTX_DESC_TSO << ICE_TXD_CTX_QW1_CMD_S) |
			     (cd_tso_len << ICE_TXD_CTX_QW1_TSO_LEN_S) |
			     (cd_mss << ICE_TXD_CTX_QW1_MSS_S));
	first->tx_flags |= ICE_TX_FLAGS_TSO;
	return 1;
}

 
static unsigned int ice_txd_use_count(unsigned int size)
{
	return ((size * 85) >> 20) + ICE_DESCS_FOR_SKB_DATA_PTR;
}

 
static unsigned int ice_xmit_desc_count(struct sk_buff *skb)
{
	const skb_frag_t *frag = &skb_shinfo(skb)->frags[0];
	unsigned int nr_frags = skb_shinfo(skb)->nr_frags;
	unsigned int count = 0, size = skb_headlen(skb);

	for (;;) {
		count += ice_txd_use_count(size);

		if (!nr_frags--)
			break;

		size = skb_frag_size(frag++);
	}

	return count;
}

 
static bool __ice_chk_linearize(struct sk_buff *skb)
{
	const skb_frag_t *frag, *stale;
	int nr_frags, sum;

	 
	nr_frags = skb_shinfo(skb)->nr_frags;
	if (nr_frags < (ICE_MAX_BUF_TXD - 1))
		return false;

	 
	nr_frags -= ICE_MAX_BUF_TXD - 2;
	frag = &skb_shinfo(skb)->frags[0];

	 
	sum = 1 - skb_shinfo(skb)->gso_size;

	 
	sum += skb_frag_size(frag++);
	sum += skb_frag_size(frag++);
	sum += skb_frag_size(frag++);
	sum += skb_frag_size(frag++);
	sum += skb_frag_size(frag++);

	 
	for (stale = &skb_shinfo(skb)->frags[0];; stale++) {
		int stale_size = skb_frag_size(stale);

		sum += skb_frag_size(frag++);

		 
		if (stale_size > ICE_MAX_DATA_PER_TXD) {
			int align_pad = -(skb_frag_off(stale)) &
					(ICE_MAX_READ_REQ_SIZE - 1);

			sum -= align_pad;
			stale_size -= align_pad;

			do {
				sum -= ICE_MAX_DATA_PER_TXD_ALIGNED;
				stale_size -= ICE_MAX_DATA_PER_TXD_ALIGNED;
			} while (stale_size > ICE_MAX_DATA_PER_TXD);
		}

		 
		if (sum < 0)
			return true;

		if (!nr_frags--)
			break;

		sum -= stale_size;
	}

	return false;
}

 
static bool ice_chk_linearize(struct sk_buff *skb, unsigned int count)
{
	 
	if (likely(count < ICE_MAX_BUF_TXD))
		return false;

	if (skb_is_gso(skb))
		return __ice_chk_linearize(skb);

	 
	return count != ICE_MAX_BUF_TXD;
}

 
static void
ice_tstamp(struct ice_tx_ring *tx_ring, struct sk_buff *skb,
	   struct ice_tx_buf *first, struct ice_tx_offload_params *off)
{
	s8 idx;

	 
	if (likely(!(skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP)))
		return;

	if (!tx_ring->ptp_tx)
		return;

	 
	if (first->tx_flags & ICE_TX_FLAGS_TSO)
		return;

	 
	idx = ice_ptp_request_ts(tx_ring->tx_tstamps, skb);
	if (idx < 0) {
		tx_ring->vsi->back->ptp.tx_hwtstamp_skipped++;
		return;
	}

	off->cd_qw1 |= (u64)(ICE_TX_DESC_DTYPE_CTX |
			     (ICE_TX_CTX_DESC_TSYN << ICE_TXD_CTX_QW1_CMD_S) |
			     ((u64)idx << ICE_TXD_CTX_QW1_TSO_LEN_S));
	first->tx_flags |= ICE_TX_FLAGS_TSYN;
}

 
static netdev_tx_t
ice_xmit_frame_ring(struct sk_buff *skb, struct ice_tx_ring *tx_ring)
{
	struct ice_tx_offload_params offload = { 0 };
	struct ice_vsi *vsi = tx_ring->vsi;
	struct ice_tx_buf *first;
	struct ethhdr *eth;
	unsigned int count;
	int tso, csum;

	ice_trace(xmit_frame_ring, tx_ring, skb);

	if (unlikely(ipv6_hopopt_jumbo_remove(skb)))
		goto out_drop;

	count = ice_xmit_desc_count(skb);
	if (ice_chk_linearize(skb, count)) {
		if (__skb_linearize(skb))
			goto out_drop;
		count = ice_txd_use_count(skb->len);
		tx_ring->ring_stats->tx_stats.tx_linearize++;
	}

	 
	if (ice_maybe_stop_tx(tx_ring, count + ICE_DESCS_PER_CACHE_LINE +
			      ICE_DESCS_FOR_CTX_DESC)) {
		tx_ring->ring_stats->tx_stats.tx_busy++;
		return NETDEV_TX_BUSY;
	}

	 
	netdev_txq_bql_enqueue_prefetchw(txring_txq(tx_ring));

	offload.tx_ring = tx_ring;

	 
	first = &tx_ring->tx_buf[tx_ring->next_to_use];
	first->skb = skb;
	first->type = ICE_TX_BUF_SKB;
	first->bytecount = max_t(unsigned int, skb->len, ETH_ZLEN);
	first->gso_segs = 1;
	first->tx_flags = 0;

	 
	ice_tx_prepare_vlan_flags(tx_ring, first);
	if (first->tx_flags & ICE_TX_FLAGS_HW_OUTER_SINGLE_VLAN) {
		offload.cd_qw1 |= (u64)(ICE_TX_DESC_DTYPE_CTX |
					(ICE_TX_CTX_DESC_IL2TAG2 <<
					ICE_TXD_CTX_QW1_CMD_S));
		offload.cd_l2tag2 = first->vid;
	}

	 
	tso = ice_tso(first, &offload);
	if (tso < 0)
		goto out_drop;

	 
	csum = ice_tx_csum(first, &offload);
	if (csum < 0)
		goto out_drop;

	 
	eth = (struct ethhdr *)skb_mac_header(skb);
	if (unlikely((skb->priority == TC_PRIO_CONTROL ||
		      eth->h_proto == htons(ETH_P_LLDP)) &&
		     vsi->type == ICE_VSI_PF &&
		     vsi->port_info->qos_cfg.is_sw_lldp))
		offload.cd_qw1 |= (u64)(ICE_TX_DESC_DTYPE_CTX |
					ICE_TX_CTX_DESC_SWTCH_UPLINK <<
					ICE_TXD_CTX_QW1_CMD_S);

	ice_tstamp(tx_ring, skb, first, &offload);
	if (ice_is_switchdev_running(vsi->back))
		ice_eswitch_set_target_vsi(skb, &offload);

	if (offload.cd_qw1 & ICE_TX_DESC_DTYPE_CTX) {
		struct ice_tx_ctx_desc *cdesc;
		u16 i = tx_ring->next_to_use;

		 
		cdesc = ICE_TX_CTX_DESC(tx_ring, i);
		i++;
		tx_ring->next_to_use = (i < tx_ring->count) ? i : 0;

		 
		cdesc->tunneling_params = cpu_to_le32(offload.cd_tunnel_params);
		cdesc->l2tag2 = cpu_to_le16(offload.cd_l2tag2);
		cdesc->rsvd = cpu_to_le16(0);
		cdesc->qw1 = cpu_to_le64(offload.cd_qw1);
	}

	ice_tx_map(tx_ring, first, &offload);
	return NETDEV_TX_OK;

out_drop:
	ice_trace(xmit_frame_ring_drop, tx_ring, skb);
	dev_kfree_skb_any(skb);
	return NETDEV_TX_OK;
}

 
netdev_tx_t ice_start_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_vsi *vsi = np->vsi;
	struct ice_tx_ring *tx_ring;

	tx_ring = vsi->tx_rings[skb->queue_mapping];

	 
	if (skb_put_padto(skb, ICE_MIN_TX_LEN))
		return NETDEV_TX_OK;

	return ice_xmit_frame_ring(skb, tx_ring);
}

 
static u8 ice_get_dscp_up(struct ice_dcbx_cfg *dcbcfg, struct sk_buff *skb)
{
	u8 dscp = 0;

	if (skb->protocol == htons(ETH_P_IP))
		dscp = ipv4_get_dsfield(ip_hdr(skb)) >> 2;
	else if (skb->protocol == htons(ETH_P_IPV6))
		dscp = ipv6_get_dsfield(ipv6_hdr(skb)) >> 2;

	return dcbcfg->dscp_map[dscp];
}

u16
ice_select_queue(struct net_device *netdev, struct sk_buff *skb,
		 struct net_device *sb_dev)
{
	struct ice_pf *pf = ice_netdev_to_pf(netdev);
	struct ice_dcbx_cfg *dcbcfg;

	dcbcfg = &pf->hw.port_info->qos_cfg.local_dcbx_cfg;
	if (dcbcfg->pfc_mode == ICE_QOS_MODE_DSCP)
		skb->priority = ice_get_dscp_up(dcbcfg, skb);

	return netdev_pick_tx(netdev, skb, sb_dev);
}

 
void ice_clean_ctrl_tx_irq(struct ice_tx_ring *tx_ring)
{
	struct ice_vsi *vsi = tx_ring->vsi;
	s16 i = tx_ring->next_to_clean;
	int budget = ICE_DFLT_IRQ_WORK;
	struct ice_tx_desc *tx_desc;
	struct ice_tx_buf *tx_buf;

	tx_buf = &tx_ring->tx_buf[i];
	tx_desc = ICE_TX_DESC(tx_ring, i);
	i -= tx_ring->count;

	do {
		struct ice_tx_desc *eop_desc = tx_buf->next_to_watch;

		 
		if (!eop_desc)
			break;

		 
		smp_rmb();

		 
		if (!(eop_desc->cmd_type_offset_bsz &
		      cpu_to_le64(ICE_TX_DESC_DTYPE_DESC_DONE)))
			break;

		 
		tx_buf->next_to_watch = NULL;
		tx_desc->buf_addr = 0;
		tx_desc->cmd_type_offset_bsz = 0;

		 
		tx_buf++;
		tx_desc++;
		i++;
		if (unlikely(!i)) {
			i -= tx_ring->count;
			tx_buf = tx_ring->tx_buf;
			tx_desc = ICE_TX_DESC(tx_ring, 0);
		}

		 
		if (dma_unmap_len(tx_buf, len))
			dma_unmap_single(tx_ring->dev,
					 dma_unmap_addr(tx_buf, dma),
					 dma_unmap_len(tx_buf, len),
					 DMA_TO_DEVICE);
		if (tx_buf->type == ICE_TX_BUF_DUMMY)
			devm_kfree(tx_ring->dev, tx_buf->raw_buf);

		 
		tx_buf->type = ICE_TX_BUF_EMPTY;
		tx_buf->tx_flags = 0;
		tx_buf->next_to_watch = NULL;
		dma_unmap_len_set(tx_buf, len, 0);
		tx_desc->buf_addr = 0;
		tx_desc->cmd_type_offset_bsz = 0;

		 
		tx_buf++;
		tx_desc++;
		i++;
		if (unlikely(!i)) {
			i -= tx_ring->count;
			tx_buf = tx_ring->tx_buf;
			tx_desc = ICE_TX_DESC(tx_ring, 0);
		}

		budget--;
	} while (likely(budget));

	i += tx_ring->count;
	tx_ring->next_to_clean = i;

	 
	ice_irq_dynamic_ena(&vsi->back->hw, vsi, vsi->q_vectors[0]);
}
