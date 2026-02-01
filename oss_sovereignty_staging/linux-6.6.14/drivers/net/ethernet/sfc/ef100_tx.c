
 

#include <net/ip6_checksum.h>

#include "net_driver.h"
#include "tx_common.h"
#include "nic_common.h"
#include "mcdi_functions.h"
#include "ef100_regs.h"
#include "io.h"
#include "ef100_tx.h"
#include "ef100_nic.h"

int ef100_tx_probe(struct efx_tx_queue *tx_queue)
{
	 
	return efx_nic_alloc_buffer(tx_queue->efx, &tx_queue->txd,
				    (tx_queue->ptr_mask + 2) *
				    sizeof(efx_oword_t),
				    GFP_KERNEL);
}

void ef100_tx_init(struct efx_tx_queue *tx_queue)
{
	 
	tx_queue->core_txq =
		netdev_get_tx_queue(tx_queue->efx->net_dev,
				    tx_queue->channel->channel -
				    tx_queue->efx->tx_channel_offset);

	 
	tx_queue->tso_version = 3;
	if (efx_mcdi_tx_init(tx_queue))
		netdev_WARN(tx_queue->efx->net_dev,
			    "failed to initialise TXQ %d\n", tx_queue->queue);
}

static bool ef100_tx_can_tso(struct efx_tx_queue *tx_queue, struct sk_buff *skb)
{
	struct efx_nic *efx = tx_queue->efx;
	struct ef100_nic_data *nic_data;
	struct efx_tx_buffer *buffer;
	size_t header_len;
	u32 mss;

	nic_data = efx->nic_data;

	if (!skb_is_gso_tcp(skb))
		return false;
	if (!(efx->net_dev->features & NETIF_F_TSO))
		return false;

	mss = skb_shinfo(skb)->gso_size;
	if (unlikely(mss < 4)) {
		WARN_ONCE(1, "MSS of %u is too small for TSO\n", mss);
		return false;
	}

	header_len = efx_tx_tso_header_length(skb);
	if (header_len > nic_data->tso_max_hdr_len)
		return false;

	if (skb_shinfo(skb)->gso_segs > nic_data->tso_max_payload_num_segs) {
		 
		WARN_ON_ONCE(1);
		return false;
	}

	if (skb->data_len / mss > nic_data->tso_max_frames)
		return false;

	 
	if (WARN_ON_ONCE(skb->data_len > nic_data->tso_max_payload_len))
		return false;

	 
	buffer = efx_tx_queue_get_insert_buffer(tx_queue);
	buffer->flags = EFX_TX_BUF_TSO_V3 | EFX_TX_BUF_CONT;
	buffer->len = header_len;
	buffer->unmap_len = 0;
	buffer->skb = skb;
	++tx_queue->insert_count;
	return true;
}

static efx_oword_t *ef100_tx_desc(struct efx_tx_queue *tx_queue, unsigned int index)
{
	if (likely(tx_queue->txd.addr))
		return ((efx_oword_t *)tx_queue->txd.addr) + index;
	else
		return NULL;
}

static void ef100_notify_tx_desc(struct efx_tx_queue *tx_queue)
{
	unsigned int write_ptr;
	efx_dword_t reg;

	tx_queue->xmit_pending = false;

	if (unlikely(tx_queue->notify_count == tx_queue->write_count))
		return;

	write_ptr = tx_queue->write_count & tx_queue->ptr_mask;
	 
	EFX_POPULATE_DWORD_1(reg, ERF_GZ_TX_RING_PIDX, write_ptr);
	efx_writed_page(tx_queue->efx, &reg,
			ER_GZ_TX_RING_DOORBELL, tx_queue->queue);
	tx_queue->notify_count = tx_queue->write_count;
}

static void ef100_tx_push_buffers(struct efx_tx_queue *tx_queue)
{
	ef100_notify_tx_desc(tx_queue);
	++tx_queue->pushes;
}

static void ef100_set_tx_csum_partial(const struct sk_buff *skb,
				      struct efx_tx_buffer *buffer, efx_oword_t *txd)
{
	efx_oword_t csum;
	int csum_start;

	if (!skb || skb->ip_summed != CHECKSUM_PARTIAL)
		return;

	 
	csum_start = skb_checksum_start_offset(skb);
	EFX_POPULATE_OWORD_3(csum,
			     ESF_GZ_TX_SEND_CSO_PARTIAL_EN, 1,
			     ESF_GZ_TX_SEND_CSO_PARTIAL_START_W,
			     csum_start >> 1,
			     ESF_GZ_TX_SEND_CSO_PARTIAL_CSUM_W,
			     skb->csum_offset >> 1);
	EFX_OR_OWORD(*txd, *txd, csum);
}

static void ef100_set_tx_hw_vlan(const struct sk_buff *skb, efx_oword_t *txd)
{
	u16 vlan_tci = skb_vlan_tag_get(skb);
	efx_oword_t vlan;

	EFX_POPULATE_OWORD_2(vlan,
			     ESF_GZ_TX_SEND_VLAN_INSERT_EN, 1,
			     ESF_GZ_TX_SEND_VLAN_INSERT_TCI, vlan_tci);
	EFX_OR_OWORD(*txd, *txd, vlan);
}

static void ef100_make_send_desc(struct efx_nic *efx,
				 const struct sk_buff *skb,
				 struct efx_tx_buffer *buffer, efx_oword_t *txd,
				 unsigned int segment_count)
{
	 
	EFX_POPULATE_OWORD_3(*txd,
			     ESF_GZ_TX_SEND_NUM_SEGS, segment_count,
			     ESF_GZ_TX_SEND_LEN, buffer->len,
			     ESF_GZ_TX_SEND_ADDR, buffer->dma_addr);

	if (likely(efx->net_dev->features & NETIF_F_HW_CSUM))
		ef100_set_tx_csum_partial(skb, buffer, txd);
	if (efx->net_dev->features & NETIF_F_HW_VLAN_CTAG_TX &&
	    skb && skb_vlan_tag_present(skb))
		ef100_set_tx_hw_vlan(skb, txd);
}

static void ef100_make_tso_desc(struct efx_nic *efx,
				const struct sk_buff *skb,
				struct efx_tx_buffer *buffer, efx_oword_t *txd,
				unsigned int segment_count)
{
	bool gso_partial = skb_shinfo(skb)->gso_type & SKB_GSO_PARTIAL;
	unsigned int len, ip_offset, tcp_offset, payload_segs;
	u32 mangleid = ESE_GZ_TX_DESC_IP4_ID_INC_MOD16;
	unsigned int outer_ip_offset, outer_l4_offset;
	u16 vlan_tci = skb_vlan_tag_get(skb);
	u32 mss = skb_shinfo(skb)->gso_size;
	bool encap = skb->encapsulation;
	bool udp_encap = false;
	u16 vlan_enable = 0;
	struct tcphdr *tcp;
	bool outer_csum;
	u32 paylen;

	if (skb_shinfo(skb)->gso_type & SKB_GSO_TCP_FIXEDID)
		mangleid = ESE_GZ_TX_DESC_IP4_ID_NO_OP;
	if (efx->net_dev->features & NETIF_F_HW_VLAN_CTAG_TX)
		vlan_enable = skb_vlan_tag_present(skb);

	len = skb->len - buffer->len;
	 
	payload_segs = segment_count - 2;
	if (encap) {
		outer_ip_offset = skb_network_offset(skb);
		outer_l4_offset = skb_transport_offset(skb);
		ip_offset = skb_inner_network_offset(skb);
		tcp_offset = skb_inner_transport_offset(skb);
		if (skb_shinfo(skb)->gso_type &
		    (SKB_GSO_UDP_TUNNEL | SKB_GSO_UDP_TUNNEL_CSUM))
			udp_encap = true;
	} else {
		ip_offset =  skb_network_offset(skb);
		tcp_offset = skb_transport_offset(skb);
		outer_ip_offset = outer_l4_offset = 0;
	}
	outer_csum = skb_shinfo(skb)->gso_type & SKB_GSO_UDP_TUNNEL_CSUM;

	 
	tcp = (void *)skb->data + tcp_offset;
	paylen = skb->len - tcp_offset;
	csum_replace_by_diff(&tcp->check, (__force __wsum)htonl(paylen));

	EFX_POPULATE_OWORD_19(*txd,
			      ESF_GZ_TX_DESC_TYPE, ESE_GZ_TX_DESC_TYPE_TSO,
			      ESF_GZ_TX_TSO_MSS, mss,
			      ESF_GZ_TX_TSO_HDR_NUM_SEGS, 1,
			      ESF_GZ_TX_TSO_PAYLOAD_NUM_SEGS, payload_segs,
			      ESF_GZ_TX_TSO_HDR_LEN_W, buffer->len >> 1,
			      ESF_GZ_TX_TSO_PAYLOAD_LEN, len,
			      ESF_GZ_TX_TSO_CSO_OUTER_L4, outer_csum,
			      ESF_GZ_TX_TSO_CSO_INNER_L4, 1,
			      ESF_GZ_TX_TSO_INNER_L3_OFF_W, ip_offset >> 1,
			      ESF_GZ_TX_TSO_INNER_L4_OFF_W, tcp_offset >> 1,
			      ESF_GZ_TX_TSO_ED_INNER_IP4_ID, mangleid,
			      ESF_GZ_TX_TSO_ED_INNER_IP_LEN, 1,
			      ESF_GZ_TX_TSO_OUTER_L3_OFF_W, outer_ip_offset >> 1,
			      ESF_GZ_TX_TSO_OUTER_L4_OFF_W, outer_l4_offset >> 1,
			      ESF_GZ_TX_TSO_ED_OUTER_UDP_LEN, udp_encap && !gso_partial,
			      ESF_GZ_TX_TSO_ED_OUTER_IP_LEN, encap && !gso_partial,
			      ESF_GZ_TX_TSO_ED_OUTER_IP4_ID, encap ? mangleid :
								     ESE_GZ_TX_DESC_IP4_ID_NO_OP,
			      ESF_GZ_TX_TSO_VLAN_INSERT_EN, vlan_enable,
			      ESF_GZ_TX_TSO_VLAN_INSERT_TCI, vlan_tci
		);
}

static void ef100_tx_make_descriptors(struct efx_tx_queue *tx_queue,
				      const struct sk_buff *skb,
				      unsigned int segment_count,
				      struct efx_rep *efv)
{
	unsigned int old_write_count = tx_queue->write_count;
	unsigned int new_write_count = old_write_count;
	struct efx_tx_buffer *buffer;
	unsigned int next_desc_type;
	unsigned int write_ptr;
	efx_oword_t *txd;
	unsigned int nr_descs = tx_queue->insert_count - old_write_count;

	if (unlikely(nr_descs == 0))
		return;

	if (segment_count)
		next_desc_type = ESE_GZ_TX_DESC_TYPE_TSO;
	else
		next_desc_type = ESE_GZ_TX_DESC_TYPE_SEND;

	if (unlikely(efv)) {
		 
		write_ptr = new_write_count & tx_queue->ptr_mask;
		txd = ef100_tx_desc(tx_queue, write_ptr);
		++new_write_count;

		tx_queue->packet_write_count = new_write_count;
		EFX_POPULATE_OWORD_3(*txd,
				     ESF_GZ_TX_DESC_TYPE, ESE_GZ_TX_DESC_TYPE_PREFIX,
				     ESF_GZ_TX_PREFIX_EGRESS_MPORT, efv->mport,
				     ESF_GZ_TX_PREFIX_EGRESS_MPORT_EN, 1);
		nr_descs--;
	}

	 
	if (!skb)
		nr_descs = 1;

	do {
		write_ptr = new_write_count & tx_queue->ptr_mask;
		buffer = &tx_queue->buffer[write_ptr];
		txd = ef100_tx_desc(tx_queue, write_ptr);
		++new_write_count;

		 
		tx_queue->packet_write_count = new_write_count;

		switch (next_desc_type) {
		case ESE_GZ_TX_DESC_TYPE_SEND:
			ef100_make_send_desc(tx_queue->efx, skb,
					     buffer, txd, nr_descs);
			break;
		case ESE_GZ_TX_DESC_TYPE_TSO:
			 
			WARN_ON_ONCE(!(buffer->flags & EFX_TX_BUF_TSO_V3));
			ef100_make_tso_desc(tx_queue->efx, skb,
					    buffer, txd, nr_descs);
			break;
		default:
			 
			EFX_POPULATE_OWORD_3(*txd,
					     ESF_GZ_TX_DESC_TYPE, ESE_GZ_TX_DESC_TYPE_SEG,
					     ESF_GZ_TX_SEG_LEN, buffer->len,
					     ESF_GZ_TX_SEG_ADDR, buffer->dma_addr);
		}
		 
		next_desc_type = skb ? ESE_GZ_TX_DESC_TYPE_SEG :
				       ESE_GZ_TX_DESC_TYPE_SEND;
		 
		if (unlikely(efv))
			buffer->flags |= EFX_TX_BUF_EFV;

	} while (new_write_count != tx_queue->insert_count);

	wmb();  

	tx_queue->write_count = new_write_count;

	 
	smp_mb();
}

void ef100_tx_write(struct efx_tx_queue *tx_queue)
{
	ef100_tx_make_descriptors(tx_queue, NULL, 0, NULL);
	ef100_tx_push_buffers(tx_queue);
}

int ef100_ev_tx(struct efx_channel *channel, const efx_qword_t *p_event)
{
	unsigned int tx_done =
		EFX_QWORD_FIELD(*p_event, ESF_GZ_EV_TXCMPL_NUM_DESC);
	unsigned int qlabel =
		EFX_QWORD_FIELD(*p_event, ESF_GZ_EV_TXCMPL_Q_LABEL);
	struct efx_tx_queue *tx_queue =
		efx_channel_get_tx_queue(channel, qlabel);
	unsigned int tx_index = (tx_queue->read_count + tx_done - 1) &
				tx_queue->ptr_mask;

	return efx_xmit_done(tx_queue, tx_index);
}

 
netdev_tx_t ef100_enqueue_skb(struct efx_tx_queue *tx_queue,
			      struct sk_buff *skb)
{
	return __ef100_enqueue_skb(tx_queue, skb, NULL);
}

int __ef100_enqueue_skb(struct efx_tx_queue *tx_queue, struct sk_buff *skb,
			struct efx_rep *efv)
{
	unsigned int old_insert_count = tx_queue->insert_count;
	struct efx_nic *efx = tx_queue->efx;
	bool xmit_more = netdev_xmit_more();
	unsigned int fill_level;
	unsigned int segments;
	int rc;

	if (!tx_queue->buffer || !tx_queue->ptr_mask) {
		netif_stop_queue(efx->net_dev);
		dev_kfree_skb_any(skb);
		return -ENODEV;
	}

	segments = skb_is_gso(skb) ? skb_shinfo(skb)->gso_segs : 0;
	if (segments == 1)
		segments = 0;	 
	if (segments && !ef100_tx_can_tso(tx_queue, skb)) {
		rc = efx_tx_tso_fallback(tx_queue, skb);
		tx_queue->tso_fallbacks++;
		if (rc)
			goto err;
		else
			return 0;
	}

	if (unlikely(efv)) {
		struct efx_tx_buffer *buffer = __efx_tx_queue_get_insert_buffer(tx_queue);

		 
		if (netif_tx_queue_stopped(tx_queue->core_txq) ||
		    unlikely(efx_tx_buffer_in_use(buffer))) {
			atomic64_inc(&efv->stats.tx_errors);
			rc = -ENOSPC;
			goto err;
		}

		 
		fill_level = efx_channel_tx_old_fill_level(tx_queue->channel);
		fill_level += efx_tx_max_skb_descs(efx);
		if (fill_level > efx->txq_stop_thresh) {
			struct efx_tx_queue *txq2;

			 
			efx_for_each_channel_tx_queue(txq2, tx_queue->channel)
				txq2->old_read_count = READ_ONCE(txq2->read_count);

			fill_level = efx_channel_tx_old_fill_level(tx_queue->channel);
			fill_level += efx_tx_max_skb_descs(efx);
			if (fill_level > efx->txq_stop_thresh) {
				atomic64_inc(&efv->stats.tx_errors);
				rc = -ENOSPC;
				goto err;
			}
		}

		buffer->flags = EFX_TX_BUF_OPTION | EFX_TX_BUF_EFV;
		tx_queue->insert_count++;
	}

	 
	rc = efx_tx_map_data(tx_queue, skb, segments);
	if (rc)
		goto err;
	ef100_tx_make_descriptors(tx_queue, skb, segments, efv);

	fill_level = efx_channel_tx_old_fill_level(tx_queue->channel);
	if (fill_level > efx->txq_stop_thresh) {
		struct efx_tx_queue *txq2;

		 
		WARN_ON(efv);

		netif_tx_stop_queue(tx_queue->core_txq);
		 
		smp_mb();
		efx_for_each_channel_tx_queue(txq2, tx_queue->channel)
			txq2->old_read_count = READ_ONCE(txq2->read_count);
		fill_level = efx_channel_tx_old_fill_level(tx_queue->channel);
		if (fill_level < efx->txq_stop_thresh)
			netif_tx_start_queue(tx_queue->core_txq);
	}

	tx_queue->xmit_pending = true;

	 
	if (unlikely(efv) ||
	    __netdev_tx_sent_queue(tx_queue->core_txq, skb->len, xmit_more) ||
	    tx_queue->write_count - tx_queue->notify_count > 255)
		ef100_tx_push_buffers(tx_queue);

	if (segments) {
		tx_queue->tso_bursts++;
		tx_queue->tso_packets += segments;
		tx_queue->tx_packets  += segments;
	} else {
		tx_queue->tx_packets++;
	}
	return 0;

err:
	efx_enqueue_unwind(tx_queue, old_insert_count);
	if (!IS_ERR_OR_NULL(skb))
		dev_kfree_skb_any(skb);

	 
	if (tx_queue->xmit_pending && !xmit_more)
		ef100_tx_push_buffers(tx_queue);
	return rc;
}
