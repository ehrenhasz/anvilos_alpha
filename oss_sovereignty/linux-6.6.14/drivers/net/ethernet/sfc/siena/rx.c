
 

#include <linux/socket.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/prefetch.h>
#include <linux/moduleparam.h>
#include <linux/iommu.h>
#include <net/ip.h>
#include <net/checksum.h>
#include <net/xdp.h>
#include <linux/bpf_trace.h>
#include "net_driver.h"
#include "efx.h"
#include "rx_common.h"
#include "filter.h"
#include "nic.h"
#include "selftest.h"
#include "workarounds.h"

 
#define EFX_RX_PREFERRED_BATCH 8U

 
#define EFX_MAX_RX_PREFIX_SIZE 16

 
#define EFX_SKB_HEADERS  128u

 
#define EFX_RX_MAX_FRAGS DIV_ROUND_UP(EFX_MAX_FRAME_LEN(EFX_MAX_MTU), \
				      EFX_RX_USR_BUF_SIZE)

static void efx_rx_packet__check_len(struct efx_rx_queue *rx_queue,
				     struct efx_rx_buffer *rx_buf,
				     int len)
{
	struct efx_nic *efx = rx_queue->efx;
	unsigned max_len = rx_buf->len - efx->type->rx_buffer_padding;

	if (likely(len <= max_len))
		return;

	 
	rx_buf->flags |= EFX_RX_PKT_DISCARD;

	if (net_ratelimit())
		netif_err(efx, rx_err, efx->net_dev,
			  "RX queue %d overlength RX event (%#x > %#x)\n",
			  efx_rx_queue_index(rx_queue), len, max_len);

	efx_rx_queue_channel(rx_queue)->n_rx_overlength++;
}

 
static struct sk_buff *efx_rx_mk_skb(struct efx_channel *channel,
				     struct efx_rx_buffer *rx_buf,
				     unsigned int n_frags,
				     u8 *eh, int hdr_len)
{
	struct efx_nic *efx = channel->efx;
	struct sk_buff *skb;

	 
	skb = netdev_alloc_skb(efx->net_dev,
			       efx->rx_ip_align + efx->rx_prefix_size +
			       hdr_len);
	if (unlikely(skb == NULL)) {
		atomic_inc(&efx->n_rx_noskb_drops);
		return NULL;
	}

	EFX_WARN_ON_ONCE_PARANOID(rx_buf->len < hdr_len);

	memcpy(skb->data + efx->rx_ip_align, eh - efx->rx_prefix_size,
	       efx->rx_prefix_size + hdr_len);
	skb_reserve(skb, efx->rx_ip_align + efx->rx_prefix_size);
	__skb_put(skb, hdr_len);

	 
	if (rx_buf->len > hdr_len) {
		rx_buf->page_offset += hdr_len;
		rx_buf->len -= hdr_len;

		for (;;) {
			skb_add_rx_frag(skb, skb_shinfo(skb)->nr_frags,
					rx_buf->page, rx_buf->page_offset,
					rx_buf->len, efx->rx_buffer_truesize);
			rx_buf->page = NULL;

			if (skb_shinfo(skb)->nr_frags == n_frags)
				break;

			rx_buf = efx_rx_buf_next(&channel->rx_queue, rx_buf);
		}
	} else {
		__free_pages(rx_buf->page, efx->rx_buffer_order);
		rx_buf->page = NULL;
		n_frags = 0;
	}

	 
	skb->protocol = eth_type_trans(skb, efx->net_dev);

	skb_mark_napi_id(skb, &channel->napi_str);

	return skb;
}

void efx_siena_rx_packet(struct efx_rx_queue *rx_queue, unsigned int index,
			 unsigned int n_frags, unsigned int len, u16 flags)
{
	struct efx_nic *efx = rx_queue->efx;
	struct efx_channel *channel = efx_rx_queue_channel(rx_queue);
	struct efx_rx_buffer *rx_buf;

	rx_queue->rx_packets++;

	rx_buf = efx_rx_buffer(rx_queue, index);
	rx_buf->flags |= flags;

	 
	if (n_frags == 1) {
		if (!(flags & EFX_RX_PKT_PREFIX_LEN))
			efx_rx_packet__check_len(rx_queue, rx_buf, len);
	} else if (unlikely(n_frags > EFX_RX_MAX_FRAGS) ||
		   unlikely(len <= (n_frags - 1) * efx->rx_dma_len) ||
		   unlikely(len > n_frags * efx->rx_dma_len) ||
		   unlikely(!efx->rx_scatter)) {
		 
		WARN_ON(!(len == 0 && rx_buf->flags & EFX_RX_PKT_DISCARD));
		rx_buf->flags |= EFX_RX_PKT_DISCARD;
	}

	netif_vdbg(efx, rx_status, efx->net_dev,
		   "RX queue %d received ids %x-%x len %d %s%s\n",
		   efx_rx_queue_index(rx_queue), index,
		   (index + n_frags - 1) & rx_queue->ptr_mask, len,
		   (rx_buf->flags & EFX_RX_PKT_CSUMMED) ? " [SUMMED]" : "",
		   (rx_buf->flags & EFX_RX_PKT_DISCARD) ? " [DISCARD]" : "");

	 
	if (unlikely(rx_buf->flags & EFX_RX_PKT_DISCARD)) {
		efx_rx_flush_packet(channel);
		efx_siena_discard_rx_packet(channel, rx_buf, n_frags);
		return;
	}

	if (n_frags == 1 && !(flags & EFX_RX_PKT_PREFIX_LEN))
		rx_buf->len = len;

	 
	efx_sync_rx_buffer(efx, rx_buf, rx_buf->len);

	 
	prefetch(efx_rx_buf_va(rx_buf));

	rx_buf->page_offset += efx->rx_prefix_size;
	rx_buf->len -= efx->rx_prefix_size;

	if (n_frags > 1) {
		 
		unsigned int tail_frags = n_frags - 1;

		for (;;) {
			rx_buf = efx_rx_buf_next(rx_queue, rx_buf);
			if (--tail_frags == 0)
				break;
			efx_sync_rx_buffer(efx, rx_buf, efx->rx_dma_len);
		}
		rx_buf->len = len - (n_frags - 1) * efx->rx_dma_len;
		efx_sync_rx_buffer(efx, rx_buf, rx_buf->len);
	}

	 
	rx_buf = efx_rx_buffer(rx_queue, index);
	efx_siena_recycle_rx_pages(channel, rx_buf, n_frags);

	 
	efx_rx_flush_packet(channel);
	channel->rx_pkt_n_frags = n_frags;
	channel->rx_pkt_index = index;
}

static void efx_rx_deliver(struct efx_channel *channel, u8 *eh,
			   struct efx_rx_buffer *rx_buf,
			   unsigned int n_frags)
{
	struct sk_buff *skb;
	u16 hdr_len = min_t(u16, rx_buf->len, EFX_SKB_HEADERS);

	skb = efx_rx_mk_skb(channel, rx_buf, n_frags, eh, hdr_len);
	if (unlikely(skb == NULL)) {
		struct efx_rx_queue *rx_queue;

		rx_queue = efx_channel_get_rx_queue(channel);
		efx_siena_free_rx_buffers(rx_queue, rx_buf, n_frags);
		return;
	}
	skb_record_rx_queue(skb, channel->rx_queue.core_index);

	 
	skb_checksum_none_assert(skb);
	if (likely(rx_buf->flags & EFX_RX_PKT_CSUMMED)) {
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		skb->csum_level = !!(rx_buf->flags & EFX_RX_PKT_CSUM_LEVEL);
	}

	efx_rx_skb_attach_timestamp(channel, skb);

	if (channel->type->receive_skb)
		if (channel->type->receive_skb(channel, skb))
			return;

	 
	if (channel->rx_list != NULL)
		 
		list_add_tail(&skb->list, channel->rx_list);
	else
		 
		netif_receive_skb(skb);
}

 
static bool efx_do_xdp(struct efx_nic *efx, struct efx_channel *channel,
		       struct efx_rx_buffer *rx_buf, u8 **ehp)
{
	u8 rx_prefix[EFX_MAX_RX_PREFIX_SIZE];
	struct efx_rx_queue *rx_queue;
	struct bpf_prog *xdp_prog;
	struct xdp_frame *xdpf;
	struct xdp_buff xdp;
	u32 xdp_act;
	s16 offset;
	int err;

	xdp_prog = rcu_dereference_bh(efx->xdp_prog);
	if (!xdp_prog)
		return true;

	rx_queue = efx_channel_get_rx_queue(channel);

	if (unlikely(channel->rx_pkt_n_frags > 1)) {
		 
		efx_siena_free_rx_buffers(rx_queue, rx_buf,
					  channel->rx_pkt_n_frags);
		if (net_ratelimit())
			netif_err(efx, rx_err, efx->net_dev,
				  "XDP is not possible with multiple receive fragments (%d)\n",
				  channel->rx_pkt_n_frags);
		channel->n_rx_xdp_bad_drops++;
		return false;
	}

	dma_sync_single_for_cpu(&efx->pci_dev->dev, rx_buf->dma_addr,
				rx_buf->len, DMA_FROM_DEVICE);

	 
	EFX_WARN_ON_PARANOID(efx->rx_prefix_size > EFX_MAX_RX_PREFIX_SIZE);
	memcpy(rx_prefix, *ehp - efx->rx_prefix_size,
	       efx->rx_prefix_size);

	xdp_init_buff(&xdp, efx->rx_page_buf_step, &rx_queue->xdp_rxq_info);
	 
	xdp_prepare_buff(&xdp, *ehp - EFX_XDP_HEADROOM, EFX_XDP_HEADROOM,
			 rx_buf->len, false);

	xdp_act = bpf_prog_run_xdp(xdp_prog, &xdp);

	offset = (u8 *)xdp.data - *ehp;

	switch (xdp_act) {
	case XDP_PASS:
		 
		if (offset) {
			*ehp += offset;
			rx_buf->page_offset += offset;
			rx_buf->len -= offset;
			memcpy(*ehp - efx->rx_prefix_size, rx_prefix,
			       efx->rx_prefix_size);
		}
		break;

	case XDP_TX:
		 
		xdpf = xdp_convert_buff_to_frame(&xdp);
		err = efx_siena_xdp_tx_buffers(efx, 1, &xdpf, true);
		if (unlikely(err != 1)) {
			efx_siena_free_rx_buffers(rx_queue, rx_buf, 1);
			if (net_ratelimit())
				netif_err(efx, rx_err, efx->net_dev,
					  "XDP TX failed (%d)\n", err);
			channel->n_rx_xdp_bad_drops++;
			trace_xdp_exception(efx->net_dev, xdp_prog, xdp_act);
		} else {
			channel->n_rx_xdp_tx++;
		}
		break;

	case XDP_REDIRECT:
		err = xdp_do_redirect(efx->net_dev, &xdp, xdp_prog);
		if (unlikely(err)) {
			efx_siena_free_rx_buffers(rx_queue, rx_buf, 1);
			if (net_ratelimit())
				netif_err(efx, rx_err, efx->net_dev,
					  "XDP redirect failed (%d)\n", err);
			channel->n_rx_xdp_bad_drops++;
			trace_xdp_exception(efx->net_dev, xdp_prog, xdp_act);
		} else {
			channel->n_rx_xdp_redirect++;
		}
		break;

	default:
		bpf_warn_invalid_xdp_action(efx->net_dev, xdp_prog, xdp_act);
		efx_siena_free_rx_buffers(rx_queue, rx_buf, 1);
		channel->n_rx_xdp_bad_drops++;
		trace_xdp_exception(efx->net_dev, xdp_prog, xdp_act);
		break;

	case XDP_ABORTED:
		trace_xdp_exception(efx->net_dev, xdp_prog, xdp_act);
		fallthrough;
	case XDP_DROP:
		efx_siena_free_rx_buffers(rx_queue, rx_buf, 1);
		channel->n_rx_xdp_drops++;
		break;
	}

	return xdp_act == XDP_PASS;
}

 
void __efx_siena_rx_packet(struct efx_channel *channel)
{
	struct efx_nic *efx = channel->efx;
	struct efx_rx_buffer *rx_buf =
		efx_rx_buffer(&channel->rx_queue, channel->rx_pkt_index);
	u8 *eh = efx_rx_buf_va(rx_buf);

	 
	if (rx_buf->flags & EFX_RX_PKT_PREFIX_LEN)
		rx_buf->len = le16_to_cpup((__le16 *)
					   (eh + efx->rx_packet_len_offset));

	 
	if (unlikely(efx->loopback_selftest)) {
		struct efx_rx_queue *rx_queue;

		efx_siena_loopback_rx_packet(efx, eh, rx_buf->len);
		rx_queue = efx_channel_get_rx_queue(channel);
		efx_siena_free_rx_buffers(rx_queue, rx_buf,
					  channel->rx_pkt_n_frags);
		goto out;
	}

	if (!efx_do_xdp(efx, channel, rx_buf, &eh))
		goto out;

	if (unlikely(!(efx->net_dev->features & NETIF_F_RXCSUM)))
		rx_buf->flags &= ~EFX_RX_PKT_CSUMMED;

	if ((rx_buf->flags & EFX_RX_PKT_TCP) && !channel->type->receive_skb)
		efx_siena_rx_packet_gro(channel, rx_buf,
					channel->rx_pkt_n_frags, eh, 0);
	else
		efx_rx_deliver(channel, eh, rx_buf, channel->rx_pkt_n_frags);
out:
	channel->rx_pkt_n_frags = 0;
}
