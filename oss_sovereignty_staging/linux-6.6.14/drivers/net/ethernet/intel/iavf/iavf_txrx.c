
 

#include <linux/prefetch.h>

#include "iavf.h"
#include "iavf_trace.h"
#include "iavf_prototype.h"

static inline __le64 build_ctob(u32 td_cmd, u32 td_offset, unsigned int size,
				u32 td_tag)
{
	return cpu_to_le64(IAVF_TX_DESC_DTYPE_DATA |
			   ((u64)td_cmd  << IAVF_TXD_QW1_CMD_SHIFT) |
			   ((u64)td_offset << IAVF_TXD_QW1_OFFSET_SHIFT) |
			   ((u64)size  << IAVF_TXD_QW1_TX_BUF_SZ_SHIFT) |
			   ((u64)td_tag  << IAVF_TXD_QW1_L2TAG1_SHIFT));
}

#define IAVF_TXD_CMD (IAVF_TX_DESC_CMD_EOP | IAVF_TX_DESC_CMD_RS)

 
static void iavf_unmap_and_free_tx_resource(struct iavf_ring *ring,
					    struct iavf_tx_buffer *tx_buffer)
{
	if (tx_buffer->skb) {
		if (tx_buffer->tx_flags & IAVF_TX_FLAGS_FD_SB)
			kfree(tx_buffer->raw_buf);
		else
			dev_kfree_skb_any(tx_buffer->skb);
		if (dma_unmap_len(tx_buffer, len))
			dma_unmap_single(ring->dev,
					 dma_unmap_addr(tx_buffer, dma),
					 dma_unmap_len(tx_buffer, len),
					 DMA_TO_DEVICE);
	} else if (dma_unmap_len(tx_buffer, len)) {
		dma_unmap_page(ring->dev,
			       dma_unmap_addr(tx_buffer, dma),
			       dma_unmap_len(tx_buffer, len),
			       DMA_TO_DEVICE);
	}

	tx_buffer->next_to_watch = NULL;
	tx_buffer->skb = NULL;
	dma_unmap_len_set(tx_buffer, len, 0);
	 
}

 
static void iavf_clean_tx_ring(struct iavf_ring *tx_ring)
{
	unsigned long bi_size;
	u16 i;

	 
	if (!tx_ring->tx_bi)
		return;

	 
	for (i = 0; i < tx_ring->count; i++)
		iavf_unmap_and_free_tx_resource(tx_ring, &tx_ring->tx_bi[i]);

	bi_size = sizeof(struct iavf_tx_buffer) * tx_ring->count;
	memset(tx_ring->tx_bi, 0, bi_size);

	 
	memset(tx_ring->desc, 0, tx_ring->size);

	tx_ring->next_to_use = 0;
	tx_ring->next_to_clean = 0;

	if (!tx_ring->netdev)
		return;

	 
	netdev_tx_reset_queue(txring_txq(tx_ring));
}

 
void iavf_free_tx_resources(struct iavf_ring *tx_ring)
{
	iavf_clean_tx_ring(tx_ring);
	kfree(tx_ring->tx_bi);
	tx_ring->tx_bi = NULL;

	if (tx_ring->desc) {
		dma_free_coherent(tx_ring->dev, tx_ring->size,
				  tx_ring->desc, tx_ring->dma);
		tx_ring->desc = NULL;
	}
}

 
static u32 iavf_get_tx_pending(struct iavf_ring *ring, bool in_sw)
{
	u32 head, tail;

	 
	head = ring->next_to_clean;
	tail = ring->next_to_use;

	if (head != tail)
		return (head < tail) ?
			tail - head : (tail + ring->count - head);

	return 0;
}

 
static void iavf_force_wb(struct iavf_vsi *vsi, struct iavf_q_vector *q_vector)
{
	u32 val = IAVF_VFINT_DYN_CTLN1_INTENA_MASK |
		  IAVF_VFINT_DYN_CTLN1_ITR_INDX_MASK |  
		  IAVF_VFINT_DYN_CTLN1_SWINT_TRIG_MASK |
		  IAVF_VFINT_DYN_CTLN1_SW_ITR_INDX_ENA_MASK
		   ;

	wr32(&vsi->back->hw,
	     IAVF_VFINT_DYN_CTLN1(q_vector->reg_idx),
	     val);
}

 
void iavf_detect_recover_hung(struct iavf_vsi *vsi)
{
	struct iavf_ring *tx_ring = NULL;
	struct net_device *netdev;
	unsigned int i;
	int packets;

	if (!vsi)
		return;

	if (test_bit(__IAVF_VSI_DOWN, vsi->state))
		return;

	netdev = vsi->netdev;
	if (!netdev)
		return;

	if (!netif_carrier_ok(netdev))
		return;

	for (i = 0; i < vsi->back->num_active_queues; i++) {
		tx_ring = &vsi->back->tx_rings[i];
		if (tx_ring && tx_ring->desc) {
			 
			packets = tx_ring->stats.packets & INT_MAX;
			if (tx_ring->tx_stats.prev_pkt_ctr == packets) {
				iavf_force_wb(vsi, tx_ring->q_vector);
				continue;
			}

			 
			smp_rmb();
			tx_ring->tx_stats.prev_pkt_ctr =
			  iavf_get_tx_pending(tx_ring, true) ? packets : -1;
		}
	}
}

#define WB_STRIDE 4

 
static bool iavf_clean_tx_irq(struct iavf_vsi *vsi,
			      struct iavf_ring *tx_ring, int napi_budget)
{
	int i = tx_ring->next_to_clean;
	struct iavf_tx_buffer *tx_buf;
	struct iavf_tx_desc *tx_desc;
	unsigned int total_bytes = 0, total_packets = 0;
	unsigned int budget = IAVF_DEFAULT_IRQ_WORK;

	tx_buf = &tx_ring->tx_bi[i];
	tx_desc = IAVF_TX_DESC(tx_ring, i);
	i -= tx_ring->count;

	do {
		struct iavf_tx_desc *eop_desc = tx_buf->next_to_watch;

		 
		if (!eop_desc)
			break;

		 
		smp_rmb();

		iavf_trace(clean_tx_irq, tx_ring, tx_desc, tx_buf);
		 
		if (!(eop_desc->cmd_type_offset_bsz &
		      cpu_to_le64(IAVF_TX_DESC_DTYPE_DESC_DONE)))
			break;

		 
		tx_buf->next_to_watch = NULL;

		 
		total_bytes += tx_buf->bytecount;
		total_packets += tx_buf->gso_segs;

		 
		napi_consume_skb(tx_buf->skb, napi_budget);

		 
		dma_unmap_single(tx_ring->dev,
				 dma_unmap_addr(tx_buf, dma),
				 dma_unmap_len(tx_buf, len),
				 DMA_TO_DEVICE);

		 
		tx_buf->skb = NULL;
		dma_unmap_len_set(tx_buf, len, 0);

		 
		while (tx_desc != eop_desc) {
			iavf_trace(clean_tx_irq_unmap,
				   tx_ring, tx_desc, tx_buf);

			tx_buf++;
			tx_desc++;
			i++;
			if (unlikely(!i)) {
				i -= tx_ring->count;
				tx_buf = tx_ring->tx_bi;
				tx_desc = IAVF_TX_DESC(tx_ring, 0);
			}

			 
			if (dma_unmap_len(tx_buf, len)) {
				dma_unmap_page(tx_ring->dev,
					       dma_unmap_addr(tx_buf, dma),
					       dma_unmap_len(tx_buf, len),
					       DMA_TO_DEVICE);
				dma_unmap_len_set(tx_buf, len, 0);
			}
		}

		 
		tx_buf++;
		tx_desc++;
		i++;
		if (unlikely(!i)) {
			i -= tx_ring->count;
			tx_buf = tx_ring->tx_bi;
			tx_desc = IAVF_TX_DESC(tx_ring, 0);
		}

		prefetch(tx_desc);

		 
		budget--;
	} while (likely(budget));

	i += tx_ring->count;
	tx_ring->next_to_clean = i;
	u64_stats_update_begin(&tx_ring->syncp);
	tx_ring->stats.bytes += total_bytes;
	tx_ring->stats.packets += total_packets;
	u64_stats_update_end(&tx_ring->syncp);
	tx_ring->q_vector->tx.total_bytes += total_bytes;
	tx_ring->q_vector->tx.total_packets += total_packets;

	if (tx_ring->flags & IAVF_TXR_FLAGS_WB_ON_ITR) {
		 
		unsigned int j = iavf_get_tx_pending(tx_ring, false);

		if (budget &&
		    ((j / WB_STRIDE) == 0) && (j > 0) &&
		    !test_bit(__IAVF_VSI_DOWN, vsi->state) &&
		    (IAVF_DESC_UNUSED(tx_ring) != tx_ring->count))
			tx_ring->arm_wb = true;
	}

	 
	netdev_tx_completed_queue(txring_txq(tx_ring),
				  total_packets, total_bytes);

#define TX_WAKE_THRESHOLD ((s16)(DESC_NEEDED * 2))
	if (unlikely(total_packets && netif_carrier_ok(tx_ring->netdev) &&
		     (IAVF_DESC_UNUSED(tx_ring) >= TX_WAKE_THRESHOLD))) {
		 
		smp_mb();
		if (__netif_subqueue_stopped(tx_ring->netdev,
					     tx_ring->queue_index) &&
		   !test_bit(__IAVF_VSI_DOWN, vsi->state)) {
			netif_wake_subqueue(tx_ring->netdev,
					    tx_ring->queue_index);
			++tx_ring->tx_stats.restart_queue;
		}
	}

	return !!budget;
}

 
static void iavf_enable_wb_on_itr(struct iavf_vsi *vsi,
				  struct iavf_q_vector *q_vector)
{
	u16 flags = q_vector->tx.ring[0].flags;
	u32 val;

	if (!(flags & IAVF_TXR_FLAGS_WB_ON_ITR))
		return;

	if (q_vector->arm_wb_state)
		return;

	val = IAVF_VFINT_DYN_CTLN1_WB_ON_ITR_MASK |
	      IAVF_VFINT_DYN_CTLN1_ITR_INDX_MASK;  

	wr32(&vsi->back->hw,
	     IAVF_VFINT_DYN_CTLN1(q_vector->reg_idx), val);
	q_vector->arm_wb_state = true;
}

static inline bool iavf_container_is_rx(struct iavf_q_vector *q_vector,
					struct iavf_ring_container *rc)
{
	return &q_vector->rx == rc;
}

#define IAVF_AIM_MULTIPLIER_100G	2560
#define IAVF_AIM_MULTIPLIER_50G		1280
#define IAVF_AIM_MULTIPLIER_40G		1024
#define IAVF_AIM_MULTIPLIER_20G		512
#define IAVF_AIM_MULTIPLIER_10G		256
#define IAVF_AIM_MULTIPLIER_1G		32

static unsigned int iavf_mbps_itr_multiplier(u32 speed_mbps)
{
	switch (speed_mbps) {
	case SPEED_100000:
		return IAVF_AIM_MULTIPLIER_100G;
	case SPEED_50000:
		return IAVF_AIM_MULTIPLIER_50G;
	case SPEED_40000:
		return IAVF_AIM_MULTIPLIER_40G;
	case SPEED_25000:
	case SPEED_20000:
		return IAVF_AIM_MULTIPLIER_20G;
	case SPEED_10000:
	default:
		return IAVF_AIM_MULTIPLIER_10G;
	case SPEED_1000:
	case SPEED_100:
		return IAVF_AIM_MULTIPLIER_1G;
	}
}

static unsigned int
iavf_virtchnl_itr_multiplier(enum virtchnl_link_speed speed_virtchnl)
{
	switch (speed_virtchnl) {
	case VIRTCHNL_LINK_SPEED_40GB:
		return IAVF_AIM_MULTIPLIER_40G;
	case VIRTCHNL_LINK_SPEED_25GB:
	case VIRTCHNL_LINK_SPEED_20GB:
		return IAVF_AIM_MULTIPLIER_20G;
	case VIRTCHNL_LINK_SPEED_10GB:
	default:
		return IAVF_AIM_MULTIPLIER_10G;
	case VIRTCHNL_LINK_SPEED_1GB:
	case VIRTCHNL_LINK_SPEED_100MB:
		return IAVF_AIM_MULTIPLIER_1G;
	}
}

static unsigned int iavf_itr_divisor(struct iavf_adapter *adapter)
{
	if (ADV_LINK_SUPPORT(adapter))
		return IAVF_ITR_ADAPTIVE_MIN_INC *
			iavf_mbps_itr_multiplier(adapter->link_speed_mbps);
	else
		return IAVF_ITR_ADAPTIVE_MIN_INC *
			iavf_virtchnl_itr_multiplier(adapter->link_speed);
}

 
static void iavf_update_itr(struct iavf_q_vector *q_vector,
			    struct iavf_ring_container *rc)
{
	unsigned int avg_wire_size, packets, bytes, itr;
	unsigned long next_update = jiffies;

	 
	if (!rc->ring || !ITR_IS_DYNAMIC(rc->ring->itr_setting))
		return;

	 
	itr = iavf_container_is_rx(q_vector, rc) ?
	      IAVF_ITR_ADAPTIVE_MIN_USECS | IAVF_ITR_ADAPTIVE_LATENCY :
	      IAVF_ITR_ADAPTIVE_MAX_USECS | IAVF_ITR_ADAPTIVE_LATENCY;

	 
	if (time_after(next_update, rc->next_update))
		goto clear_counts;

	 
	if (q_vector->itr_countdown) {
		itr = rc->target_itr;
		goto clear_counts;
	}

	packets = rc->total_packets;
	bytes = rc->total_bytes;

	if (iavf_container_is_rx(q_vector, rc)) {
		 
		if (packets && packets < 4 && bytes < 9000 &&
		    (q_vector->tx.target_itr & IAVF_ITR_ADAPTIVE_LATENCY)) {
			itr = IAVF_ITR_ADAPTIVE_LATENCY;
			goto adjust_by_size;
		}
	} else if (packets < 4) {
		 
		if (rc->target_itr == IAVF_ITR_ADAPTIVE_MAX_USECS &&
		    (q_vector->rx.target_itr & IAVF_ITR_MASK) ==
		     IAVF_ITR_ADAPTIVE_MAX_USECS)
			goto clear_counts;
	} else if (packets > 32) {
		 
		rc->target_itr &= ~IAVF_ITR_ADAPTIVE_LATENCY;
	}

	 
	if (packets < 56) {
		itr = rc->target_itr + IAVF_ITR_ADAPTIVE_MIN_INC;
		if ((itr & IAVF_ITR_MASK) > IAVF_ITR_ADAPTIVE_MAX_USECS) {
			itr &= IAVF_ITR_ADAPTIVE_LATENCY;
			itr += IAVF_ITR_ADAPTIVE_MAX_USECS;
		}
		goto clear_counts;
	}

	if (packets <= 256) {
		itr = min(q_vector->tx.current_itr, q_vector->rx.current_itr);
		itr &= IAVF_ITR_MASK;

		 
		if (packets <= 112)
			goto clear_counts;

		 
		itr /= 2;
		itr &= IAVF_ITR_MASK;
		if (itr < IAVF_ITR_ADAPTIVE_MIN_USECS)
			itr = IAVF_ITR_ADAPTIVE_MIN_USECS;

		goto clear_counts;
	}

	 
	itr = IAVF_ITR_ADAPTIVE_BULK;

adjust_by_size:
	 
	avg_wire_size = bytes / packets;

	 
	if (avg_wire_size <= 60) {
		 
		avg_wire_size = 4096;
	} else if (avg_wire_size <= 380) {
		 
		avg_wire_size *= 40;
		avg_wire_size += 1696;
	} else if (avg_wire_size <= 1084) {
		 
		avg_wire_size *= 15;
		avg_wire_size += 11452;
	} else if (avg_wire_size <= 1980) {
		 
		avg_wire_size *= 5;
		avg_wire_size += 22420;
	} else {
		 
		avg_wire_size = 32256;
	}

	 
	if (itr & IAVF_ITR_ADAPTIVE_LATENCY)
		avg_wire_size /= 2;

	 
	itr += DIV_ROUND_UP(avg_wire_size,
			    iavf_itr_divisor(q_vector->adapter)) *
		IAVF_ITR_ADAPTIVE_MIN_INC;

	if ((itr & IAVF_ITR_MASK) > IAVF_ITR_ADAPTIVE_MAX_USECS) {
		itr &= IAVF_ITR_ADAPTIVE_LATENCY;
		itr += IAVF_ITR_ADAPTIVE_MAX_USECS;
	}

clear_counts:
	 
	rc->target_itr = itr;

	 
	rc->next_update = next_update + 1;

	rc->total_bytes = 0;
	rc->total_packets = 0;
}

 
int iavf_setup_tx_descriptors(struct iavf_ring *tx_ring)
{
	struct device *dev = tx_ring->dev;
	int bi_size;

	if (!dev)
		return -ENOMEM;

	 
	WARN_ON(tx_ring->tx_bi);
	bi_size = sizeof(struct iavf_tx_buffer) * tx_ring->count;
	tx_ring->tx_bi = kzalloc(bi_size, GFP_KERNEL);
	if (!tx_ring->tx_bi)
		goto err;

	 
	tx_ring->size = tx_ring->count * sizeof(struct iavf_tx_desc);
	tx_ring->size = ALIGN(tx_ring->size, 4096);
	tx_ring->desc = dma_alloc_coherent(dev, tx_ring->size,
					   &tx_ring->dma, GFP_KERNEL);
	if (!tx_ring->desc) {
		dev_info(dev, "Unable to allocate memory for the Tx descriptor ring, size=%d\n",
			 tx_ring->size);
		goto err;
	}

	tx_ring->next_to_use = 0;
	tx_ring->next_to_clean = 0;
	tx_ring->tx_stats.prev_pkt_ctr = -1;
	return 0;

err:
	kfree(tx_ring->tx_bi);
	tx_ring->tx_bi = NULL;
	return -ENOMEM;
}

 
static void iavf_clean_rx_ring(struct iavf_ring *rx_ring)
{
	unsigned long bi_size;
	u16 i;

	 
	if (!rx_ring->rx_bi)
		return;

	if (rx_ring->skb) {
		dev_kfree_skb(rx_ring->skb);
		rx_ring->skb = NULL;
	}

	 
	for (i = 0; i < rx_ring->count; i++) {
		struct iavf_rx_buffer *rx_bi = &rx_ring->rx_bi[i];

		if (!rx_bi->page)
			continue;

		 
		dma_sync_single_range_for_cpu(rx_ring->dev,
					      rx_bi->dma,
					      rx_bi->page_offset,
					      rx_ring->rx_buf_len,
					      DMA_FROM_DEVICE);

		 
		dma_unmap_page_attrs(rx_ring->dev, rx_bi->dma,
				     iavf_rx_pg_size(rx_ring),
				     DMA_FROM_DEVICE,
				     IAVF_RX_DMA_ATTR);

		__page_frag_cache_drain(rx_bi->page, rx_bi->pagecnt_bias);

		rx_bi->page = NULL;
		rx_bi->page_offset = 0;
	}

	bi_size = sizeof(struct iavf_rx_buffer) * rx_ring->count;
	memset(rx_ring->rx_bi, 0, bi_size);

	 
	memset(rx_ring->desc, 0, rx_ring->size);

	rx_ring->next_to_alloc = 0;
	rx_ring->next_to_clean = 0;
	rx_ring->next_to_use = 0;
}

 
void iavf_free_rx_resources(struct iavf_ring *rx_ring)
{
	iavf_clean_rx_ring(rx_ring);
	kfree(rx_ring->rx_bi);
	rx_ring->rx_bi = NULL;

	if (rx_ring->desc) {
		dma_free_coherent(rx_ring->dev, rx_ring->size,
				  rx_ring->desc, rx_ring->dma);
		rx_ring->desc = NULL;
	}
}

 
int iavf_setup_rx_descriptors(struct iavf_ring *rx_ring)
{
	struct device *dev = rx_ring->dev;
	int bi_size;

	 
	WARN_ON(rx_ring->rx_bi);
	bi_size = sizeof(struct iavf_rx_buffer) * rx_ring->count;
	rx_ring->rx_bi = kzalloc(bi_size, GFP_KERNEL);
	if (!rx_ring->rx_bi)
		goto err;

	u64_stats_init(&rx_ring->syncp);

	 
	rx_ring->size = rx_ring->count * sizeof(union iavf_32byte_rx_desc);
	rx_ring->size = ALIGN(rx_ring->size, 4096);
	rx_ring->desc = dma_alloc_coherent(dev, rx_ring->size,
					   &rx_ring->dma, GFP_KERNEL);

	if (!rx_ring->desc) {
		dev_info(dev, "Unable to allocate memory for the Rx descriptor ring, size=%d\n",
			 rx_ring->size);
		goto err;
	}

	rx_ring->next_to_alloc = 0;
	rx_ring->next_to_clean = 0;
	rx_ring->next_to_use = 0;

	return 0;
err:
	kfree(rx_ring->rx_bi);
	rx_ring->rx_bi = NULL;
	return -ENOMEM;
}

 
static inline void iavf_release_rx_desc(struct iavf_ring *rx_ring, u32 val)
{
	rx_ring->next_to_use = val;

	 
	rx_ring->next_to_alloc = val;

	 
	wmb();
	writel(val, rx_ring->tail);
}

 
static inline unsigned int iavf_rx_offset(struct iavf_ring *rx_ring)
{
	return ring_uses_build_skb(rx_ring) ? IAVF_SKB_PAD : 0;
}

 
static bool iavf_alloc_mapped_page(struct iavf_ring *rx_ring,
				   struct iavf_rx_buffer *bi)
{
	struct page *page = bi->page;
	dma_addr_t dma;

	 
	if (likely(page)) {
		rx_ring->rx_stats.page_reuse_count++;
		return true;
	}

	 
	page = dev_alloc_pages(iavf_rx_pg_order(rx_ring));
	if (unlikely(!page)) {
		rx_ring->rx_stats.alloc_page_failed++;
		return false;
	}

	 
	dma = dma_map_page_attrs(rx_ring->dev, page, 0,
				 iavf_rx_pg_size(rx_ring),
				 DMA_FROM_DEVICE,
				 IAVF_RX_DMA_ATTR);

	 
	if (dma_mapping_error(rx_ring->dev, dma)) {
		__free_pages(page, iavf_rx_pg_order(rx_ring));
		rx_ring->rx_stats.alloc_page_failed++;
		return false;
	}

	bi->dma = dma;
	bi->page = page;
	bi->page_offset = iavf_rx_offset(rx_ring);

	 
	bi->pagecnt_bias = 1;

	return true;
}

 
static void iavf_receive_skb(struct iavf_ring *rx_ring,
			     struct sk_buff *skb, u16 vlan_tag)
{
	struct iavf_q_vector *q_vector = rx_ring->q_vector;

	if ((rx_ring->netdev->features & NETIF_F_HW_VLAN_CTAG_RX) &&
	    (vlan_tag & VLAN_VID_MASK))
		__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), vlan_tag);
	else if ((rx_ring->netdev->features & NETIF_F_HW_VLAN_STAG_RX) &&
		 vlan_tag & VLAN_VID_MASK)
		__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021AD), vlan_tag);

	napi_gro_receive(&q_vector->napi, skb);
}

 
bool iavf_alloc_rx_buffers(struct iavf_ring *rx_ring, u16 cleaned_count)
{
	u16 ntu = rx_ring->next_to_use;
	union iavf_rx_desc *rx_desc;
	struct iavf_rx_buffer *bi;

	 
	if (!rx_ring->netdev || !cleaned_count)
		return false;

	rx_desc = IAVF_RX_DESC(rx_ring, ntu);
	bi = &rx_ring->rx_bi[ntu];

	do {
		if (!iavf_alloc_mapped_page(rx_ring, bi))
			goto no_buffers;

		 
		dma_sync_single_range_for_device(rx_ring->dev, bi->dma,
						 bi->page_offset,
						 rx_ring->rx_buf_len,
						 DMA_FROM_DEVICE);

		 
		rx_desc->read.pkt_addr = cpu_to_le64(bi->dma + bi->page_offset);

		rx_desc++;
		bi++;
		ntu++;
		if (unlikely(ntu == rx_ring->count)) {
			rx_desc = IAVF_RX_DESC(rx_ring, 0);
			bi = rx_ring->rx_bi;
			ntu = 0;
		}

		 
		rx_desc->wb.qword1.status_error_len = 0;

		cleaned_count--;
	} while (cleaned_count);

	if (rx_ring->next_to_use != ntu)
		iavf_release_rx_desc(rx_ring, ntu);

	return false;

no_buffers:
	if (rx_ring->next_to_use != ntu)
		iavf_release_rx_desc(rx_ring, ntu);

	 
	return true;
}

 
static inline void iavf_rx_checksum(struct iavf_vsi *vsi,
				    struct sk_buff *skb,
				    union iavf_rx_desc *rx_desc)
{
	struct iavf_rx_ptype_decoded decoded;
	u32 rx_error, rx_status;
	bool ipv4, ipv6;
	u8 ptype;
	u64 qword;

	qword = le64_to_cpu(rx_desc->wb.qword1.status_error_len);
	ptype = (qword & IAVF_RXD_QW1_PTYPE_MASK) >> IAVF_RXD_QW1_PTYPE_SHIFT;
	rx_error = (qword & IAVF_RXD_QW1_ERROR_MASK) >>
		   IAVF_RXD_QW1_ERROR_SHIFT;
	rx_status = (qword & IAVF_RXD_QW1_STATUS_MASK) >>
		    IAVF_RXD_QW1_STATUS_SHIFT;
	decoded = decode_rx_desc_ptype(ptype);

	skb->ip_summed = CHECKSUM_NONE;

	skb_checksum_none_assert(skb);

	 
	if (!(vsi->netdev->features & NETIF_F_RXCSUM))
		return;

	 
	if (!(rx_status & BIT(IAVF_RX_DESC_STATUS_L3L4P_SHIFT)))
		return;

	 
	if (!(decoded.known && decoded.outer_ip))
		return;

	ipv4 = (decoded.outer_ip == IAVF_RX_PTYPE_OUTER_IP) &&
	       (decoded.outer_ip_ver == IAVF_RX_PTYPE_OUTER_IPV4);
	ipv6 = (decoded.outer_ip == IAVF_RX_PTYPE_OUTER_IP) &&
	       (decoded.outer_ip_ver == IAVF_RX_PTYPE_OUTER_IPV6);

	if (ipv4 &&
	    (rx_error & (BIT(IAVF_RX_DESC_ERROR_IPE_SHIFT) |
			 BIT(IAVF_RX_DESC_ERROR_EIPE_SHIFT))))
		goto checksum_fail;

	 
	if (ipv6 &&
	    rx_status & BIT(IAVF_RX_DESC_STATUS_IPV6EXADD_SHIFT))
		 
		return;

	 
	if (rx_error & BIT(IAVF_RX_DESC_ERROR_L4E_SHIFT))
		goto checksum_fail;

	 
	if (rx_error & BIT(IAVF_RX_DESC_ERROR_PPRS_SHIFT))
		return;

	 
	switch (decoded.inner_prot) {
	case IAVF_RX_PTYPE_INNER_PROT_TCP:
	case IAVF_RX_PTYPE_INNER_PROT_UDP:
	case IAVF_RX_PTYPE_INNER_PROT_SCTP:
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		fallthrough;
	default:
		break;
	}

	return;

checksum_fail:
	vsi->back->hw_csum_rx_error++;
}

 
static inline int iavf_ptype_to_htype(u8 ptype)
{
	struct iavf_rx_ptype_decoded decoded = decode_rx_desc_ptype(ptype);

	if (!decoded.known)
		return PKT_HASH_TYPE_NONE;

	if (decoded.outer_ip == IAVF_RX_PTYPE_OUTER_IP &&
	    decoded.payload_layer == IAVF_RX_PTYPE_PAYLOAD_LAYER_PAY4)
		return PKT_HASH_TYPE_L4;
	else if (decoded.outer_ip == IAVF_RX_PTYPE_OUTER_IP &&
		 decoded.payload_layer == IAVF_RX_PTYPE_PAYLOAD_LAYER_PAY3)
		return PKT_HASH_TYPE_L3;
	else
		return PKT_HASH_TYPE_L2;
}

 
static inline void iavf_rx_hash(struct iavf_ring *ring,
				union iavf_rx_desc *rx_desc,
				struct sk_buff *skb,
				u8 rx_ptype)
{
	u32 hash;
	const __le64 rss_mask =
		cpu_to_le64((u64)IAVF_RX_DESC_FLTSTAT_RSS_HASH <<
			    IAVF_RX_DESC_STATUS_FLTSTAT_SHIFT);

	if (!(ring->netdev->features & NETIF_F_RXHASH))
		return;

	if ((rx_desc->wb.qword1.status_error_len & rss_mask) == rss_mask) {
		hash = le32_to_cpu(rx_desc->wb.qword0.hi_dword.rss);
		skb_set_hash(skb, hash, iavf_ptype_to_htype(rx_ptype));
	}
}

 
static inline
void iavf_process_skb_fields(struct iavf_ring *rx_ring,
			     union iavf_rx_desc *rx_desc, struct sk_buff *skb,
			     u8 rx_ptype)
{
	iavf_rx_hash(rx_ring, rx_desc, skb, rx_ptype);

	iavf_rx_checksum(rx_ring->vsi, skb, rx_desc);

	skb_record_rx_queue(skb, rx_ring->queue_index);

	 
	skb->protocol = eth_type_trans(skb, rx_ring->netdev);
}

 
static bool iavf_cleanup_headers(struct iavf_ring *rx_ring, struct sk_buff *skb)
{
	 
	if (eth_skb_pad(skb))
		return true;

	return false;
}

 
static void iavf_reuse_rx_page(struct iavf_ring *rx_ring,
			       struct iavf_rx_buffer *old_buff)
{
	struct iavf_rx_buffer *new_buff;
	u16 nta = rx_ring->next_to_alloc;

	new_buff = &rx_ring->rx_bi[nta];

	 
	nta++;
	rx_ring->next_to_alloc = (nta < rx_ring->count) ? nta : 0;

	 
	new_buff->dma		= old_buff->dma;
	new_buff->page		= old_buff->page;
	new_buff->page_offset	= old_buff->page_offset;
	new_buff->pagecnt_bias	= old_buff->pagecnt_bias;
}

 
static bool iavf_can_reuse_rx_page(struct iavf_rx_buffer *rx_buffer)
{
	unsigned int pagecnt_bias = rx_buffer->pagecnt_bias;
	struct page *page = rx_buffer->page;

	 
	if (!dev_page_is_reusable(page))
		return false;

#if (PAGE_SIZE < 8192)
	 
	if (unlikely((page_count(page) - pagecnt_bias) > 1))
		return false;
#else
#define IAVF_LAST_OFFSET \
	(SKB_WITH_OVERHEAD(PAGE_SIZE) - IAVF_RXBUFFER_2048)
	if (rx_buffer->page_offset > IAVF_LAST_OFFSET)
		return false;
#endif

	 
	if (unlikely(!pagecnt_bias)) {
		page_ref_add(page, USHRT_MAX);
		rx_buffer->pagecnt_bias = USHRT_MAX;
	}

	return true;
}

 
static void iavf_add_rx_frag(struct iavf_ring *rx_ring,
			     struct iavf_rx_buffer *rx_buffer,
			     struct sk_buff *skb,
			     unsigned int size)
{
#if (PAGE_SIZE < 8192)
	unsigned int truesize = iavf_rx_pg_size(rx_ring) / 2;
#else
	unsigned int truesize = SKB_DATA_ALIGN(size + iavf_rx_offset(rx_ring));
#endif

	if (!size)
		return;

	skb_add_rx_frag(skb, skb_shinfo(skb)->nr_frags, rx_buffer->page,
			rx_buffer->page_offset, size, truesize);

	 
#if (PAGE_SIZE < 8192)
	rx_buffer->page_offset ^= truesize;
#else
	rx_buffer->page_offset += truesize;
#endif
}

 
static struct iavf_rx_buffer *iavf_get_rx_buffer(struct iavf_ring *rx_ring,
						 const unsigned int size)
{
	struct iavf_rx_buffer *rx_buffer;

	rx_buffer = &rx_ring->rx_bi[rx_ring->next_to_clean];
	prefetchw(rx_buffer->page);
	if (!size)
		return rx_buffer;

	 
	dma_sync_single_range_for_cpu(rx_ring->dev,
				      rx_buffer->dma,
				      rx_buffer->page_offset,
				      size,
				      DMA_FROM_DEVICE);

	 
	rx_buffer->pagecnt_bias--;

	return rx_buffer;
}

 
static struct sk_buff *iavf_construct_skb(struct iavf_ring *rx_ring,
					  struct iavf_rx_buffer *rx_buffer,
					  unsigned int size)
{
	void *va;
#if (PAGE_SIZE < 8192)
	unsigned int truesize = iavf_rx_pg_size(rx_ring) / 2;
#else
	unsigned int truesize = SKB_DATA_ALIGN(size);
#endif
	unsigned int headlen;
	struct sk_buff *skb;

	if (!rx_buffer)
		return NULL;
	 
	va = page_address(rx_buffer->page) + rx_buffer->page_offset;
	net_prefetch(va);

	 
	skb = __napi_alloc_skb(&rx_ring->q_vector->napi,
			       IAVF_RX_HDR_SIZE,
			       GFP_ATOMIC | __GFP_NOWARN);
	if (unlikely(!skb))
		return NULL;

	 
	headlen = size;
	if (headlen > IAVF_RX_HDR_SIZE)
		headlen = eth_get_headlen(skb->dev, va, IAVF_RX_HDR_SIZE);

	 
	memcpy(__skb_put(skb, headlen), va, ALIGN(headlen, sizeof(long)));

	 
	size -= headlen;
	if (size) {
		skb_add_rx_frag(skb, 0, rx_buffer->page,
				rx_buffer->page_offset + headlen,
				size, truesize);

		 
#if (PAGE_SIZE < 8192)
		rx_buffer->page_offset ^= truesize;
#else
		rx_buffer->page_offset += truesize;
#endif
	} else {
		 
		rx_buffer->pagecnt_bias++;
	}

	return skb;
}

 
static struct sk_buff *iavf_build_skb(struct iavf_ring *rx_ring,
				      struct iavf_rx_buffer *rx_buffer,
				      unsigned int size)
{
	void *va;
#if (PAGE_SIZE < 8192)
	unsigned int truesize = iavf_rx_pg_size(rx_ring) / 2;
#else
	unsigned int truesize = SKB_DATA_ALIGN(sizeof(struct skb_shared_info)) +
				SKB_DATA_ALIGN(IAVF_SKB_PAD + size);
#endif
	struct sk_buff *skb;

	if (!rx_buffer || !size)
		return NULL;
	 
	va = page_address(rx_buffer->page) + rx_buffer->page_offset;
	net_prefetch(va);

	 
	skb = napi_build_skb(va - IAVF_SKB_PAD, truesize);
	if (unlikely(!skb))
		return NULL;

	 
	skb_reserve(skb, IAVF_SKB_PAD);
	__skb_put(skb, size);

	 
#if (PAGE_SIZE < 8192)
	rx_buffer->page_offset ^= truesize;
#else
	rx_buffer->page_offset += truesize;
#endif

	return skb;
}

 
static void iavf_put_rx_buffer(struct iavf_ring *rx_ring,
			       struct iavf_rx_buffer *rx_buffer)
{
	if (!rx_buffer)
		return;

	if (iavf_can_reuse_rx_page(rx_buffer)) {
		 
		iavf_reuse_rx_page(rx_ring, rx_buffer);
		rx_ring->rx_stats.page_reuse_count++;
	} else {
		 
		dma_unmap_page_attrs(rx_ring->dev, rx_buffer->dma,
				     iavf_rx_pg_size(rx_ring),
				     DMA_FROM_DEVICE, IAVF_RX_DMA_ATTR);
		__page_frag_cache_drain(rx_buffer->page,
					rx_buffer->pagecnt_bias);
	}

	 
	rx_buffer->page = NULL;
}

 
static bool iavf_is_non_eop(struct iavf_ring *rx_ring,
			    union iavf_rx_desc *rx_desc,
			    struct sk_buff *skb)
{
	u32 ntc = rx_ring->next_to_clean + 1;

	 
	ntc = (ntc < rx_ring->count) ? ntc : 0;
	rx_ring->next_to_clean = ntc;

	prefetch(IAVF_RX_DESC(rx_ring, ntc));

	 
#define IAVF_RXD_EOF BIT(IAVF_RX_DESC_STATUS_EOF_SHIFT)
	if (likely(iavf_test_staterr(rx_desc, IAVF_RXD_EOF)))
		return false;

	rx_ring->rx_stats.non_eop_descs++;

	return true;
}

 
static int iavf_clean_rx_irq(struct iavf_ring *rx_ring, int budget)
{
	unsigned int total_rx_bytes = 0, total_rx_packets = 0;
	struct sk_buff *skb = rx_ring->skb;
	u16 cleaned_count = IAVF_DESC_UNUSED(rx_ring);
	bool failure = false;

	while (likely(total_rx_packets < (unsigned int)budget)) {
		struct iavf_rx_buffer *rx_buffer;
		union iavf_rx_desc *rx_desc;
		unsigned int size;
		u16 vlan_tag = 0;
		u8 rx_ptype;
		u64 qword;

		 
		if (cleaned_count >= IAVF_RX_BUFFER_WRITE) {
			failure = failure ||
				  iavf_alloc_rx_buffers(rx_ring, cleaned_count);
			cleaned_count = 0;
		}

		rx_desc = IAVF_RX_DESC(rx_ring, rx_ring->next_to_clean);

		 
		qword = le64_to_cpu(rx_desc->wb.qword1.status_error_len);

		 
		dma_rmb();
#define IAVF_RXD_DD BIT(IAVF_RX_DESC_STATUS_DD_SHIFT)
		if (!iavf_test_staterr(rx_desc, IAVF_RXD_DD))
			break;

		size = (qword & IAVF_RXD_QW1_LENGTH_PBUF_MASK) >>
		       IAVF_RXD_QW1_LENGTH_PBUF_SHIFT;

		iavf_trace(clean_rx_irq, rx_ring, rx_desc, skb);
		rx_buffer = iavf_get_rx_buffer(rx_ring, size);

		 
		if (skb)
			iavf_add_rx_frag(rx_ring, rx_buffer, skb, size);
		else if (ring_uses_build_skb(rx_ring))
			skb = iavf_build_skb(rx_ring, rx_buffer, size);
		else
			skb = iavf_construct_skb(rx_ring, rx_buffer, size);

		 
		if (!skb) {
			rx_ring->rx_stats.alloc_buff_failed++;
			if (rx_buffer && size)
				rx_buffer->pagecnt_bias++;
			break;
		}

		iavf_put_rx_buffer(rx_ring, rx_buffer);
		cleaned_count++;

		if (iavf_is_non_eop(rx_ring, rx_desc, skb))
			continue;

		 
		if (unlikely(iavf_test_staterr(rx_desc, BIT(IAVF_RXD_QW1_ERROR_SHIFT)))) {
			dev_kfree_skb_any(skb);
			skb = NULL;
			continue;
		}

		if (iavf_cleanup_headers(rx_ring, skb)) {
			skb = NULL;
			continue;
		}

		 
		total_rx_bytes += skb->len;

		qword = le64_to_cpu(rx_desc->wb.qword1.status_error_len);
		rx_ptype = (qword & IAVF_RXD_QW1_PTYPE_MASK) >>
			   IAVF_RXD_QW1_PTYPE_SHIFT;

		 
		iavf_process_skb_fields(rx_ring, rx_desc, skb, rx_ptype);

		if (qword & BIT(IAVF_RX_DESC_STATUS_L2TAG1P_SHIFT) &&
		    rx_ring->flags & IAVF_TXRX_FLAGS_VLAN_TAG_LOC_L2TAG1)
			vlan_tag = le16_to_cpu(rx_desc->wb.qword0.lo_dword.l2tag1);
		if (rx_desc->wb.qword2.ext_status &
		    cpu_to_le16(BIT(IAVF_RX_DESC_EXT_STATUS_L2TAG2P_SHIFT)) &&
		    rx_ring->flags & IAVF_RXR_FLAGS_VLAN_TAG_LOC_L2TAG2_2)
			vlan_tag = le16_to_cpu(rx_desc->wb.qword2.l2tag2_2);

		iavf_trace(clean_rx_irq_rx, rx_ring, rx_desc, skb);
		iavf_receive_skb(rx_ring, skb, vlan_tag);
		skb = NULL;

		 
		total_rx_packets++;
	}

	rx_ring->skb = skb;

	u64_stats_update_begin(&rx_ring->syncp);
	rx_ring->stats.packets += total_rx_packets;
	rx_ring->stats.bytes += total_rx_bytes;
	u64_stats_update_end(&rx_ring->syncp);
	rx_ring->q_vector->rx.total_packets += total_rx_packets;
	rx_ring->q_vector->rx.total_bytes += total_rx_bytes;

	 
	return failure ? budget : (int)total_rx_packets;
}

static inline u32 iavf_buildreg_itr(const int type, u16 itr)
{
	u32 val;

	 
	itr &= IAVF_ITR_MASK;

	val = IAVF_VFINT_DYN_CTLN1_INTENA_MASK |
	      (type << IAVF_VFINT_DYN_CTLN1_ITR_INDX_SHIFT) |
	      (itr << (IAVF_VFINT_DYN_CTLN1_INTERVAL_SHIFT - 1));

	return val;
}

 
#define INTREG IAVF_VFINT_DYN_CTLN1

 
#define ITR_COUNTDOWN_START 3

 
static inline void iavf_update_enable_itr(struct iavf_vsi *vsi,
					  struct iavf_q_vector *q_vector)
{
	struct iavf_hw *hw = &vsi->back->hw;
	u32 intval;

	 
	iavf_update_itr(q_vector, &q_vector->tx);
	iavf_update_itr(q_vector, &q_vector->rx);

	 
	if (q_vector->rx.target_itr < q_vector->rx.current_itr) {
		 
		intval = iavf_buildreg_itr(IAVF_RX_ITR,
					   q_vector->rx.target_itr);
		q_vector->rx.current_itr = q_vector->rx.target_itr;
		q_vector->itr_countdown = ITR_COUNTDOWN_START;
	} else if ((q_vector->tx.target_itr < q_vector->tx.current_itr) ||
		   ((q_vector->rx.target_itr - q_vector->rx.current_itr) <
		    (q_vector->tx.target_itr - q_vector->tx.current_itr))) {
		 
		intval = iavf_buildreg_itr(IAVF_TX_ITR,
					   q_vector->tx.target_itr);
		q_vector->tx.current_itr = q_vector->tx.target_itr;
		q_vector->itr_countdown = ITR_COUNTDOWN_START;
	} else if (q_vector->rx.current_itr != q_vector->rx.target_itr) {
		 
		intval = iavf_buildreg_itr(IAVF_RX_ITR,
					   q_vector->rx.target_itr);
		q_vector->rx.current_itr = q_vector->rx.target_itr;
		q_vector->itr_countdown = ITR_COUNTDOWN_START;
	} else {
		 
		intval = iavf_buildreg_itr(IAVF_ITR_NONE, 0);
		if (q_vector->itr_countdown)
			q_vector->itr_countdown--;
	}

	if (!test_bit(__IAVF_VSI_DOWN, vsi->state))
		wr32(hw, INTREG(q_vector->reg_idx), intval);
}

 
int iavf_napi_poll(struct napi_struct *napi, int budget)
{
	struct iavf_q_vector *q_vector =
			       container_of(napi, struct iavf_q_vector, napi);
	struct iavf_vsi *vsi = q_vector->vsi;
	struct iavf_ring *ring;
	bool clean_complete = true;
	bool arm_wb = false;
	int budget_per_ring;
	int work_done = 0;

	if (test_bit(__IAVF_VSI_DOWN, vsi->state)) {
		napi_complete(napi);
		return 0;
	}

	 
	iavf_for_each_ring(ring, q_vector->tx) {
		if (!iavf_clean_tx_irq(vsi, ring, budget)) {
			clean_complete = false;
			continue;
		}
		arm_wb |= ring->arm_wb;
		ring->arm_wb = false;
	}

	 
	if (budget <= 0)
		goto tx_only;

	 
	budget_per_ring = max(budget/q_vector->num_ringpairs, 1);

	iavf_for_each_ring(ring, q_vector->rx) {
		int cleaned = iavf_clean_rx_irq(ring, budget_per_ring);

		work_done += cleaned;
		 
		if (cleaned >= budget_per_ring)
			clean_complete = false;
	}

	 
	if (!clean_complete) {
		int cpu_id = smp_processor_id();

		 
		if (!cpumask_test_cpu(cpu_id, &q_vector->affinity_mask)) {
			 
			napi_complete_done(napi, work_done);

			 
			iavf_force_wb(vsi, q_vector);

			 
			return budget - 1;
		}
tx_only:
		if (arm_wb) {
			q_vector->tx.ring[0].tx_stats.tx_force_wb++;
			iavf_enable_wb_on_itr(vsi, q_vector);
		}
		return budget;
	}

	if (vsi->back->flags & IAVF_TXR_FLAGS_WB_ON_ITR)
		q_vector->arm_wb_state = false;

	 
	if (likely(napi_complete_done(napi, work_done)))
		iavf_update_enable_itr(vsi, q_vector);

	return min_t(int, work_done, budget - 1);
}

 
static void iavf_tx_prepare_vlan_flags(struct sk_buff *skb,
				       struct iavf_ring *tx_ring, u32 *flags)
{
	u32  tx_flags = 0;


	 
	if (!skb_vlan_tag_present(skb))
		return;

	tx_flags |= skb_vlan_tag_get(skb) << IAVF_TX_FLAGS_VLAN_SHIFT;
	if (tx_ring->flags & IAVF_TXR_FLAGS_VLAN_TAG_LOC_L2TAG2) {
		tx_flags |= IAVF_TX_FLAGS_HW_OUTER_SINGLE_VLAN;
	} else if (tx_ring->flags & IAVF_TXRX_FLAGS_VLAN_TAG_LOC_L2TAG1) {
		tx_flags |= IAVF_TX_FLAGS_HW_VLAN;
	} else {
		dev_dbg(tx_ring->dev, "Unsupported Tx VLAN tag location requested\n");
		return;
	}

	*flags = tx_flags;
}

 
static int iavf_tso(struct iavf_tx_buffer *first, u8 *hdr_len,
		    u64 *cd_type_cmd_tso_mss)
{
	struct sk_buff *skb = first->skb;
	u64 cd_cmd, cd_tso_len, cd_mss;
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
	u32 paylen, l4_offset;
	u16 gso_segs, gso_size;
	int err;

	if (skb->ip_summed != CHECKSUM_PARTIAL)
		return 0;

	if (!skb_is_gso(skb))
		return 0;

	err = skb_cow_head(skb, 0);
	if (err < 0)
		return err;

	ip.hdr = skb_network_header(skb);
	l4.hdr = skb_transport_header(skb);

	 
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

			 
			l4_offset = l4.hdr - skb->data;

			 
			paylen = skb->len - l4_offset;
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

	 
	l4_offset = l4.hdr - skb->data;
	 
	paylen = skb->len - l4_offset;

	if (skb_shinfo(skb)->gso_type & SKB_GSO_UDP_L4) {
		csum_replace_by_diff(&l4.udp->check,
				     (__force __wsum)htonl(paylen));
		 
		*hdr_len = (u8)sizeof(l4.udp) + l4_offset;
	} else {
		csum_replace_by_diff(&l4.tcp->check,
				     (__force __wsum)htonl(paylen));
		 
		*hdr_len = (u8)((l4.tcp->doff * 4) + l4_offset);
	}

	 
	gso_size = skb_shinfo(skb)->gso_size;
	gso_segs = skb_shinfo(skb)->gso_segs;

	 
	first->gso_segs = gso_segs;
	first->bytecount += (first->gso_segs - 1) * *hdr_len;

	 
	cd_cmd = IAVF_TX_CTX_DESC_TSO;
	cd_tso_len = skb->len - *hdr_len;
	cd_mss = gso_size;
	*cd_type_cmd_tso_mss |= (cd_cmd << IAVF_TXD_CTX_QW1_CMD_SHIFT) |
				(cd_tso_len << IAVF_TXD_CTX_QW1_TSO_LEN_SHIFT) |
				(cd_mss << IAVF_TXD_CTX_QW1_MSS_SHIFT);
	return 1;
}

 
static int iavf_tx_enable_csum(struct sk_buff *skb, u32 *tx_flags,
			       u32 *td_cmd, u32 *td_offset,
			       struct iavf_ring *tx_ring,
			       u32 *cd_tunneling)
{
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
	unsigned char *exthdr;
	u32 offset, cmd = 0;
	__be16 frag_off;
	u8 l4_proto = 0;

	if (skb->ip_summed != CHECKSUM_PARTIAL)
		return 0;

	ip.hdr = skb_network_header(skb);
	l4.hdr = skb_transport_header(skb);

	 
	offset = ((ip.hdr - skb->data) / 2) << IAVF_TX_DESC_LENGTH_MACLEN_SHIFT;

	if (skb->encapsulation) {
		u32 tunnel = 0;
		 
		if (*tx_flags & IAVF_TX_FLAGS_IPV4) {
			tunnel |= (*tx_flags & IAVF_TX_FLAGS_TSO) ?
				  IAVF_TX_CTX_EXT_IP_IPV4 :
				  IAVF_TX_CTX_EXT_IP_IPV4_NO_CSUM;

			l4_proto = ip.v4->protocol;
		} else if (*tx_flags & IAVF_TX_FLAGS_IPV6) {
			tunnel |= IAVF_TX_CTX_EXT_IP_IPV6;

			exthdr = ip.hdr + sizeof(*ip.v6);
			l4_proto = ip.v6->nexthdr;
			if (l4.hdr != exthdr)
				ipv6_skip_exthdr(skb, exthdr - skb->data,
						 &l4_proto, &frag_off);
		}

		 
		switch (l4_proto) {
		case IPPROTO_UDP:
			tunnel |= IAVF_TXD_CTX_UDP_TUNNELING;
			*tx_flags |= IAVF_TX_FLAGS_VXLAN_TUNNEL;
			break;
		case IPPROTO_GRE:
			tunnel |= IAVF_TXD_CTX_GRE_TUNNELING;
			*tx_flags |= IAVF_TX_FLAGS_VXLAN_TUNNEL;
			break;
		case IPPROTO_IPIP:
		case IPPROTO_IPV6:
			*tx_flags |= IAVF_TX_FLAGS_VXLAN_TUNNEL;
			l4.hdr = skb_inner_network_header(skb);
			break;
		default:
			if (*tx_flags & IAVF_TX_FLAGS_TSO)
				return -1;

			skb_checksum_help(skb);
			return 0;
		}

		 
		tunnel |= ((l4.hdr - ip.hdr) / 4) <<
			  IAVF_TXD_CTX_QW0_EXT_IPLEN_SHIFT;

		 
		ip.hdr = skb_inner_network_header(skb);

		 
		tunnel |= ((ip.hdr - l4.hdr) / 2) <<
			  IAVF_TXD_CTX_QW0_NATLEN_SHIFT;

		 
		if ((*tx_flags & IAVF_TX_FLAGS_TSO) &&
		    !(skb_shinfo(skb)->gso_type & SKB_GSO_PARTIAL) &&
		    (skb_shinfo(skb)->gso_type & SKB_GSO_UDP_TUNNEL_CSUM))
			tunnel |= IAVF_TXD_CTX_QW0_L4T_CS_MASK;

		 
		*cd_tunneling |= tunnel;

		 
		l4.hdr = skb_inner_transport_header(skb);
		l4_proto = 0;

		 
		*tx_flags &= ~(IAVF_TX_FLAGS_IPV4 | IAVF_TX_FLAGS_IPV6);
		if (ip.v4->version == 4)
			*tx_flags |= IAVF_TX_FLAGS_IPV4;
		if (ip.v6->version == 6)
			*tx_flags |= IAVF_TX_FLAGS_IPV6;
	}

	 
	if (*tx_flags & IAVF_TX_FLAGS_IPV4) {
		l4_proto = ip.v4->protocol;
		 
		cmd |= (*tx_flags & IAVF_TX_FLAGS_TSO) ?
		       IAVF_TX_DESC_CMD_IIPT_IPV4_CSUM :
		       IAVF_TX_DESC_CMD_IIPT_IPV4;
	} else if (*tx_flags & IAVF_TX_FLAGS_IPV6) {
		cmd |= IAVF_TX_DESC_CMD_IIPT_IPV6;

		exthdr = ip.hdr + sizeof(*ip.v6);
		l4_proto = ip.v6->nexthdr;
		if (l4.hdr != exthdr)
			ipv6_skip_exthdr(skb, exthdr - skb->data,
					 &l4_proto, &frag_off);
	}

	 
	offset |= ((l4.hdr - ip.hdr) / 4) << IAVF_TX_DESC_LENGTH_IPLEN_SHIFT;

	 
	switch (l4_proto) {
	case IPPROTO_TCP:
		 
		cmd |= IAVF_TX_DESC_CMD_L4T_EOFT_TCP;
		offset |= l4.tcp->doff << IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	case IPPROTO_SCTP:
		 
		cmd |= IAVF_TX_DESC_CMD_L4T_EOFT_SCTP;
		offset |= (sizeof(struct sctphdr) >> 2) <<
			  IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	case IPPROTO_UDP:
		 
		cmd |= IAVF_TX_DESC_CMD_L4T_EOFT_UDP;
		offset |= (sizeof(struct udphdr) >> 2) <<
			  IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	default:
		if (*tx_flags & IAVF_TX_FLAGS_TSO)
			return -1;
		skb_checksum_help(skb);
		return 0;
	}

	*td_cmd |= cmd;
	*td_offset |= offset;

	return 1;
}

 
static void iavf_create_tx_ctx(struct iavf_ring *tx_ring,
			       const u64 cd_type_cmd_tso_mss,
			       const u32 cd_tunneling, const u32 cd_l2tag2)
{
	struct iavf_tx_context_desc *context_desc;
	int i = tx_ring->next_to_use;

	if ((cd_type_cmd_tso_mss == IAVF_TX_DESC_DTYPE_CONTEXT) &&
	    !cd_tunneling && !cd_l2tag2)
		return;

	 
	context_desc = IAVF_TX_CTXTDESC(tx_ring, i);

	i++;
	tx_ring->next_to_use = (i < tx_ring->count) ? i : 0;

	 
	context_desc->tunneling_params = cpu_to_le32(cd_tunneling);
	context_desc->l2tag2 = cpu_to_le16(cd_l2tag2);
	context_desc->rsvd = cpu_to_le16(0);
	context_desc->type_cmd_tso_mss = cpu_to_le64(cd_type_cmd_tso_mss);
}

 
bool __iavf_chk_linearize(struct sk_buff *skb)
{
	const skb_frag_t *frag, *stale;
	int nr_frags, sum;

	 
	nr_frags = skb_shinfo(skb)->nr_frags;
	if (nr_frags < (IAVF_MAX_BUFFER_TXD - 1))
		return false;

	 
	nr_frags -= IAVF_MAX_BUFFER_TXD - 2;
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

		 
		if (stale_size > IAVF_MAX_DATA_PER_TXD) {
			int align_pad = -(skb_frag_off(stale)) &
					(IAVF_MAX_READ_REQ_SIZE - 1);

			sum -= align_pad;
			stale_size -= align_pad;

			do {
				sum -= IAVF_MAX_DATA_PER_TXD_ALIGNED;
				stale_size -= IAVF_MAX_DATA_PER_TXD_ALIGNED;
			} while (stale_size > IAVF_MAX_DATA_PER_TXD);
		}

		 
		if (sum < 0)
			return true;

		if (!nr_frags--)
			break;

		sum -= stale_size;
	}

	return false;
}

 
int __iavf_maybe_stop_tx(struct iavf_ring *tx_ring, int size)
{
	netif_stop_subqueue(tx_ring->netdev, tx_ring->queue_index);
	 
	smp_mb();

	 
	if (likely(IAVF_DESC_UNUSED(tx_ring) < size))
		return -EBUSY;

	 
	netif_start_subqueue(tx_ring->netdev, tx_ring->queue_index);
	++tx_ring->tx_stats.restart_queue;
	return 0;
}

 
static inline void iavf_tx_map(struct iavf_ring *tx_ring, struct sk_buff *skb,
			       struct iavf_tx_buffer *first, u32 tx_flags,
			       const u8 hdr_len, u32 td_cmd, u32 td_offset)
{
	unsigned int data_len = skb->data_len;
	unsigned int size = skb_headlen(skb);
	skb_frag_t *frag;
	struct iavf_tx_buffer *tx_bi;
	struct iavf_tx_desc *tx_desc;
	u16 i = tx_ring->next_to_use;
	u32 td_tag = 0;
	dma_addr_t dma;

	if (tx_flags & IAVF_TX_FLAGS_HW_VLAN) {
		td_cmd |= IAVF_TX_DESC_CMD_IL2TAG1;
		td_tag = (tx_flags & IAVF_TX_FLAGS_VLAN_MASK) >>
			 IAVF_TX_FLAGS_VLAN_SHIFT;
	}

	first->tx_flags = tx_flags;

	dma = dma_map_single(tx_ring->dev, skb->data, size, DMA_TO_DEVICE);

	tx_desc = IAVF_TX_DESC(tx_ring, i);
	tx_bi = first;

	for (frag = &skb_shinfo(skb)->frags[0];; frag++) {
		unsigned int max_data = IAVF_MAX_DATA_PER_TXD_ALIGNED;

		if (dma_mapping_error(tx_ring->dev, dma))
			goto dma_error;

		 
		dma_unmap_len_set(tx_bi, len, size);
		dma_unmap_addr_set(tx_bi, dma, dma);

		 
		max_data += -dma & (IAVF_MAX_READ_REQ_SIZE - 1);
		tx_desc->buffer_addr = cpu_to_le64(dma);

		while (unlikely(size > IAVF_MAX_DATA_PER_TXD)) {
			tx_desc->cmd_type_offset_bsz =
				build_ctob(td_cmd, td_offset,
					   max_data, td_tag);

			tx_desc++;
			i++;

			if (i == tx_ring->count) {
				tx_desc = IAVF_TX_DESC(tx_ring, 0);
				i = 0;
			}

			dma += max_data;
			size -= max_data;

			max_data = IAVF_MAX_DATA_PER_TXD_ALIGNED;
			tx_desc->buffer_addr = cpu_to_le64(dma);
		}

		if (likely(!data_len))
			break;

		tx_desc->cmd_type_offset_bsz = build_ctob(td_cmd, td_offset,
							  size, td_tag);

		tx_desc++;
		i++;

		if (i == tx_ring->count) {
			tx_desc = IAVF_TX_DESC(tx_ring, 0);
			i = 0;
		}

		size = skb_frag_size(frag);
		data_len -= size;

		dma = skb_frag_dma_map(tx_ring->dev, frag, 0, size,
				       DMA_TO_DEVICE);

		tx_bi = &tx_ring->tx_bi[i];
	}

	netdev_tx_sent_queue(txring_txq(tx_ring), first->bytecount);

	i++;
	if (i == tx_ring->count)
		i = 0;

	tx_ring->next_to_use = i;

	iavf_maybe_stop_tx(tx_ring, DESC_NEEDED);

	 
	td_cmd |= IAVF_TXD_CMD;
	tx_desc->cmd_type_offset_bsz =
			build_ctob(td_cmd, td_offset, size, td_tag);

	skb_tx_timestamp(skb);

	 
	wmb();

	 
	first->next_to_watch = tx_desc;

	 
	if (netif_xmit_stopped(txring_txq(tx_ring)) || !netdev_xmit_more()) {
		writel(i, tx_ring->tail);
	}

	return;

dma_error:
	dev_info(tx_ring->dev, "TX DMA map failed\n");

	 
	for (;;) {
		tx_bi = &tx_ring->tx_bi[i];
		iavf_unmap_and_free_tx_resource(tx_ring, tx_bi);
		if (tx_bi == first)
			break;
		if (i == 0)
			i = tx_ring->count;
		i--;
	}

	tx_ring->next_to_use = i;
}

 
static netdev_tx_t iavf_xmit_frame_ring(struct sk_buff *skb,
					struct iavf_ring *tx_ring)
{
	u64 cd_type_cmd_tso_mss = IAVF_TX_DESC_DTYPE_CONTEXT;
	u32 cd_tunneling = 0, cd_l2tag2 = 0;
	struct iavf_tx_buffer *first;
	u32 td_offset = 0;
	u32 tx_flags = 0;
	__be16 protocol;
	u32 td_cmd = 0;
	u8 hdr_len = 0;
	int tso, count;

	 
	prefetch(skb->data);

	iavf_trace(xmit_frame_ring, skb, tx_ring);

	count = iavf_xmit_descriptor_count(skb);
	if (iavf_chk_linearize(skb, count)) {
		if (__skb_linearize(skb)) {
			dev_kfree_skb_any(skb);
			return NETDEV_TX_OK;
		}
		count = iavf_txd_use_count(skb->len);
		tx_ring->tx_stats.tx_linearize++;
	}

	 
	if (iavf_maybe_stop_tx(tx_ring, count + 4 + 1)) {
		tx_ring->tx_stats.tx_busy++;
		return NETDEV_TX_BUSY;
	}

	 
	first = &tx_ring->tx_bi[tx_ring->next_to_use];
	first->skb = skb;
	first->bytecount = skb->len;
	first->gso_segs = 1;

	 
	iavf_tx_prepare_vlan_flags(skb, tx_ring, &tx_flags);
	if (tx_flags & IAVF_TX_FLAGS_HW_OUTER_SINGLE_VLAN) {
		cd_type_cmd_tso_mss |= IAVF_TX_CTX_DESC_IL2TAG2 <<
			IAVF_TXD_CTX_QW1_CMD_SHIFT;
		cd_l2tag2 = (tx_flags & IAVF_TX_FLAGS_VLAN_MASK) >>
			IAVF_TX_FLAGS_VLAN_SHIFT;
	}

	 
	protocol = vlan_get_protocol(skb);

	 
	if (protocol == htons(ETH_P_IP))
		tx_flags |= IAVF_TX_FLAGS_IPV4;
	else if (protocol == htons(ETH_P_IPV6))
		tx_flags |= IAVF_TX_FLAGS_IPV6;

	tso = iavf_tso(first, &hdr_len, &cd_type_cmd_tso_mss);

	if (tso < 0)
		goto out_drop;
	else if (tso)
		tx_flags |= IAVF_TX_FLAGS_TSO;

	 
	tso = iavf_tx_enable_csum(skb, &tx_flags, &td_cmd, &td_offset,
				  tx_ring, &cd_tunneling);
	if (tso < 0)
		goto out_drop;

	 
	td_cmd |= IAVF_TX_DESC_CMD_ICRC;

	iavf_create_tx_ctx(tx_ring, cd_type_cmd_tso_mss,
			   cd_tunneling, cd_l2tag2);

	iavf_tx_map(tx_ring, skb, first, tx_flags, hdr_len,
		    td_cmd, td_offset);

	return NETDEV_TX_OK;

out_drop:
	iavf_trace(xmit_frame_ring_drop, first->skb, tx_ring);
	dev_kfree_skb_any(first->skb);
	first->skb = NULL;
	return NETDEV_TX_OK;
}

 
netdev_tx_t iavf_xmit_frame(struct sk_buff *skb, struct net_device *netdev)
{
	struct iavf_adapter *adapter = netdev_priv(netdev);
	struct iavf_ring *tx_ring = &adapter->tx_rings[skb->queue_mapping];

	 
	if (unlikely(skb->len < IAVF_MIN_TX_LEN)) {
		if (skb_pad(skb, IAVF_MIN_TX_LEN - skb->len))
			return NETDEV_TX_OK;
		skb->len = IAVF_MIN_TX_LEN;
		skb_set_tail_pointer(skb, IAVF_MIN_TX_LEN);
	}

	return iavf_xmit_frame_ring(skb, tx_ring);
}
