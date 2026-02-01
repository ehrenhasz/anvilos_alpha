
 

#include <linux/bpf_trace.h>
#include <linux/netdevice.h>
#include <linux/overflow.h>
#include <linux/sizes.h>
#include <linux/bitfield.h>
#include <net/xfrm.h>

#include "../nfp_app.h"
#include "../nfp_net.h"
#include "../nfp_net_dp.h"
#include "../crypto/crypto.h"
#include "../crypto/fw.h"
#include "nfdk.h"

static int nfp_nfdk_tx_ring_should_wake(struct nfp_net_tx_ring *tx_ring)
{
	return !nfp_net_tx_full(tx_ring, NFDK_TX_DESC_STOP_CNT * 2);
}

static int nfp_nfdk_tx_ring_should_stop(struct nfp_net_tx_ring *tx_ring)
{
	return nfp_net_tx_full(tx_ring, NFDK_TX_DESC_STOP_CNT);
}

static void nfp_nfdk_tx_ring_stop(struct netdev_queue *nd_q,
				  struct nfp_net_tx_ring *tx_ring)
{
	netif_tx_stop_queue(nd_q);

	 
	smp_mb();
	if (unlikely(nfp_nfdk_tx_ring_should_wake(tx_ring)))
		netif_tx_start_queue(nd_q);
}

static __le64
nfp_nfdk_tx_tso(struct nfp_net_r_vector *r_vec, struct nfp_nfdk_tx_buf *txbuf,
		struct sk_buff *skb)
{
	u32 segs, hdrlen, l3_offset, l4_offset;
	struct nfp_nfdk_tx_desc txd;
	u16 mss;

	if (!skb->encapsulation) {
		l3_offset = skb_network_offset(skb);
		l4_offset = skb_transport_offset(skb);
		hdrlen = skb_tcp_all_headers(skb);
	} else {
		l3_offset = skb_inner_network_offset(skb);
		l4_offset = skb_inner_transport_offset(skb);
		hdrlen = skb_inner_tcp_all_headers(skb);
	}

	segs = skb_shinfo(skb)->gso_segs;
	mss = skb_shinfo(skb)->gso_size & NFDK_DESC_TX_MSS_MASK;

	txd.l3_offset = l3_offset;
	txd.l4_offset = l4_offset;
	txd.lso_meta_res = 0;
	txd.mss = cpu_to_le16(mss);
	txd.lso_hdrlen = hdrlen;
	txd.lso_totsegs = segs;

	txbuf->pkt_cnt = segs;
	txbuf->real_len = skb->len + hdrlen * (txbuf->pkt_cnt - 1);

	u64_stats_update_begin(&r_vec->tx_sync);
	r_vec->tx_lso++;
	u64_stats_update_end(&r_vec->tx_sync);

	return txd.raw;
}

static u8
nfp_nfdk_tx_csum(struct nfp_net_dp *dp, struct nfp_net_r_vector *r_vec,
		 unsigned int pkt_cnt, struct sk_buff *skb, u64 flags)
{
	struct ipv6hdr *ipv6h;
	struct iphdr *iph;

	if (!(dp->ctrl & NFP_NET_CFG_CTRL_TXCSUM))
		return flags;

	if (skb->ip_summed != CHECKSUM_PARTIAL)
		return flags;

	flags |= NFDK_DESC_TX_L4_CSUM;

	iph = skb->encapsulation ? inner_ip_hdr(skb) : ip_hdr(skb);
	ipv6h = skb->encapsulation ? inner_ipv6_hdr(skb) : ipv6_hdr(skb);

	 
	if (iph->version == 4) {
		flags |= NFDK_DESC_TX_L3_CSUM;
	} else if (ipv6h->version != 6) {
		nn_dp_warn(dp, "partial checksum but ipv=%x!\n", iph->version);
		return flags;
	}

	u64_stats_update_begin(&r_vec->tx_sync);
	if (!skb->encapsulation) {
		r_vec->hw_csum_tx += pkt_cnt;
	} else {
		flags |= NFDK_DESC_TX_ENCAP;
		r_vec->hw_csum_tx_inner += pkt_cnt;
	}
	u64_stats_update_end(&r_vec->tx_sync);

	return flags;
}

static int
nfp_nfdk_tx_maybe_close_block(struct nfp_net_tx_ring *tx_ring,
			      struct sk_buff *skb)
{
	unsigned int n_descs, wr_p, nop_slots;
	const skb_frag_t *frag, *fend;
	struct nfp_nfdk_tx_desc *txd;
	unsigned int nr_frags;
	unsigned int wr_idx;
	int err;

recount_descs:
	n_descs = nfp_nfdk_headlen_to_segs(skb_headlen(skb));
	nr_frags = skb_shinfo(skb)->nr_frags;
	frag = skb_shinfo(skb)->frags;
	fend = frag + nr_frags;
	for (; frag < fend; frag++)
		n_descs += DIV_ROUND_UP(skb_frag_size(frag),
					NFDK_TX_MAX_DATA_PER_DESC);

	if (unlikely(n_descs > NFDK_TX_DESC_GATHER_MAX)) {
		if (skb_is_nonlinear(skb)) {
			err = skb_linearize(skb);
			if (err)
				return err;
			goto recount_descs;
		}
		return -EINVAL;
	}

	 
	n_descs += !!skb_is_gso(skb);

	if (round_down(tx_ring->wr_p, NFDK_TX_DESC_BLOCK_CNT) !=
	    round_down(tx_ring->wr_p + n_descs, NFDK_TX_DESC_BLOCK_CNT))
		goto close_block;

	if ((u32)tx_ring->data_pending + skb->len > NFDK_TX_MAX_DATA_PER_BLOCK)
		goto close_block;

	return 0;

close_block:
	wr_p = tx_ring->wr_p;
	nop_slots = D_BLOCK_CPL(wr_p);

	wr_idx = D_IDX(tx_ring, wr_p);
	tx_ring->ktxbufs[wr_idx].skb = NULL;
	txd = &tx_ring->ktxds[wr_idx];

	memset(txd, 0, array_size(nop_slots, sizeof(struct nfp_nfdk_tx_desc)));

	tx_ring->data_pending = 0;
	tx_ring->wr_p += nop_slots;
	tx_ring->wr_ptr_add += nop_slots;

	return 0;
}

static int
nfp_nfdk_prep_tx_meta(struct nfp_net_dp *dp, struct nfp_app *app,
		      struct sk_buff *skb, bool *ipsec)
{
	struct metadata_dst *md_dst = skb_metadata_dst(skb);
	struct nfp_ipsec_offload offload_info;
	unsigned char *data;
	bool vlan_insert;
	u32 meta_id = 0;
	int md_bytes;

#ifdef CONFIG_NFP_NET_IPSEC
	if (xfrm_offload(skb))
		*ipsec = nfp_net_ipsec_tx_prep(dp, skb, &offload_info);
#endif

	if (unlikely(md_dst && md_dst->type != METADATA_HW_PORT_MUX))
		md_dst = NULL;

	vlan_insert = skb_vlan_tag_present(skb) && (dp->ctrl & NFP_NET_CFG_CTRL_TXVLAN_V2);

	if (!(md_dst || vlan_insert || *ipsec))
		return 0;

	md_bytes = sizeof(meta_id) +
		   (!!md_dst ? NFP_NET_META_PORTID_SIZE : 0) +
		   (vlan_insert ? NFP_NET_META_VLAN_SIZE : 0) +
		   (*ipsec ? NFP_NET_META_IPSEC_FIELD_SIZE : 0);

	if (unlikely(skb_cow_head(skb, md_bytes)))
		return -ENOMEM;

	data = skb_push(skb, md_bytes) + md_bytes;
	if (md_dst) {
		data -= NFP_NET_META_PORTID_SIZE;
		put_unaligned_be32(md_dst->u.port_info.port_id, data);
		meta_id = NFP_NET_META_PORTID;
	}
	if (vlan_insert) {
		data -= NFP_NET_META_VLAN_SIZE;
		 
		memcpy(data, &skb->vlan_proto, sizeof(skb->vlan_proto));
		put_unaligned_be16(skb_vlan_tag_get(skb), data + sizeof(skb->vlan_proto));
		meta_id <<= NFP_NET_META_FIELD_SIZE;
		meta_id |= NFP_NET_META_VLAN;
	}

	if (*ipsec) {
		data -= NFP_NET_META_IPSEC_SIZE;
		put_unaligned_be32(offload_info.seq_hi, data);
		data -= NFP_NET_META_IPSEC_SIZE;
		put_unaligned_be32(offload_info.seq_low, data);
		data -= NFP_NET_META_IPSEC_SIZE;
		put_unaligned_be32(offload_info.handle - 1, data);
		meta_id <<= NFP_NET_META_IPSEC_FIELD_SIZE;
		meta_id |= NFP_NET_META_IPSEC << 8 | NFP_NET_META_IPSEC << 4 | NFP_NET_META_IPSEC;
	}

	meta_id = FIELD_PREP(NFDK_META_LEN, md_bytes) |
		  FIELD_PREP(NFDK_META_FIELDS, meta_id);

	data -= sizeof(meta_id);
	put_unaligned_be32(meta_id, data);

	return NFDK_DESC_TX_CHAIN_META;
}

 
netdev_tx_t nfp_nfdk_tx(struct sk_buff *skb, struct net_device *netdev)
{
	struct nfp_net *nn = netdev_priv(netdev);
	struct nfp_nfdk_tx_buf *txbuf, *etxbuf;
	u32 cnt, tmp_dlen, dlen_type = 0;
	struct nfp_net_tx_ring *tx_ring;
	struct nfp_net_r_vector *r_vec;
	const skb_frag_t *frag, *fend;
	struct nfp_nfdk_tx_desc *txd;
	unsigned int real_len, qidx;
	unsigned int dma_len, type;
	struct netdev_queue *nd_q;
	struct nfp_net_dp *dp;
	int nr_frags, wr_idx;
	dma_addr_t dma_addr;
	bool ipsec = false;
	u64 metadata;

	dp = &nn->dp;
	qidx = skb_get_queue_mapping(skb);
	tx_ring = &dp->tx_rings[qidx];
	r_vec = tx_ring->r_vec;
	nd_q = netdev_get_tx_queue(dp->netdev, qidx);

	 
	if (unlikely(nfp_net_tx_full(tx_ring, NFDK_TX_DESC_STOP_CNT))) {
		nn_dp_warn(dp, "TX ring %d busy. wrp=%u rdp=%u\n",
			   qidx, tx_ring->wr_p, tx_ring->rd_p);
		netif_tx_stop_queue(nd_q);
		nfp_net_tx_xmit_more_flush(tx_ring);
		u64_stats_update_begin(&r_vec->tx_sync);
		r_vec->tx_busy++;
		u64_stats_update_end(&r_vec->tx_sync);
		return NETDEV_TX_BUSY;
	}

	metadata = nfp_nfdk_prep_tx_meta(dp, nn->app, skb, &ipsec);
	if (unlikely((int)metadata < 0))
		goto err_flush;

	if (nfp_nfdk_tx_maybe_close_block(tx_ring, skb))
		goto err_flush;

	 
	nr_frags = skb_shinfo(skb)->nr_frags;
	 
	wr_idx = D_IDX(tx_ring, tx_ring->wr_p);
	txd = &tx_ring->ktxds[wr_idx];
	txbuf = &tx_ring->ktxbufs[wr_idx];

	dma_len = skb_headlen(skb);
	if (skb_is_gso(skb))
		type = NFDK_DESC_TX_TYPE_TSO;
	else if (!nr_frags && dma_len <= NFDK_TX_MAX_DATA_PER_HEAD)
		type = NFDK_DESC_TX_TYPE_SIMPLE;
	else
		type = NFDK_DESC_TX_TYPE_GATHER;

	dma_addr = dma_map_single(dp->dev, skb->data, dma_len, DMA_TO_DEVICE);
	if (dma_mapping_error(dp->dev, dma_addr))
		goto err_warn_dma;

	txbuf->skb = skb;
	txbuf++;

	txbuf->dma_addr = dma_addr;
	txbuf++;

	 
	dma_len -= 1;

	 
	dlen_type = FIELD_PREP(NFDK_DESC_TX_DMA_LEN_HEAD,
			       dma_len > NFDK_DESC_TX_DMA_LEN_HEAD ?
			       NFDK_DESC_TX_DMA_LEN_HEAD : dma_len) |
		    FIELD_PREP(NFDK_DESC_TX_TYPE_HEAD, type);

	txd->dma_len_type = cpu_to_le16(dlen_type);
	nfp_desc_set_dma_addr_48b(txd, dma_addr);

	 
	BUILD_BUG_ON(!(NFDK_DESC_TX_DMA_LEN_HEAD & 1));

	 
	tmp_dlen = dlen_type & NFDK_DESC_TX_DMA_LEN_HEAD;
	dma_len -= tmp_dlen;
	dma_addr += tmp_dlen + 1;
	txd++;

	 
	frag = skb_shinfo(skb)->frags;
	fend = frag + nr_frags;

	while (true) {
		while (dma_len > 0) {
			dma_len -= 1;
			dlen_type = FIELD_PREP(NFDK_DESC_TX_DMA_LEN, dma_len);

			txd->dma_len_type = cpu_to_le16(dlen_type);
			nfp_desc_set_dma_addr_48b(txd, dma_addr);

			dma_len -= dlen_type;
			dma_addr += dlen_type + 1;
			txd++;
		}

		if (frag >= fend)
			break;

		dma_len = skb_frag_size(frag);
		dma_addr = skb_frag_dma_map(dp->dev, frag, 0, dma_len,
					    DMA_TO_DEVICE);
		if (dma_mapping_error(dp->dev, dma_addr))
			goto err_unmap;

		txbuf->dma_addr = dma_addr;
		txbuf++;

		frag++;
	}

	(txd - 1)->dma_len_type = cpu_to_le16(dlen_type | NFDK_DESC_TX_EOP);

	if (ipsec)
		metadata = nfp_nfdk_ipsec_tx(metadata, skb);

	if (!skb_is_gso(skb)) {
		real_len = skb->len;
		 
		if (!ipsec)
			metadata = nfp_nfdk_tx_csum(dp, r_vec, 1, skb, metadata);
		txd->raw = cpu_to_le64(metadata);
		txd++;
	} else {
		 
		(txd + 1)->raw = nfp_nfdk_tx_tso(r_vec, txbuf, skb);
		real_len = txbuf->real_len;
		 
		if (!ipsec)
			metadata = nfp_nfdk_tx_csum(dp, r_vec, txbuf->pkt_cnt, skb, metadata);
		txd->raw = cpu_to_le64(metadata);
		txd += 2;
		txbuf++;
	}

	cnt = txd - tx_ring->ktxds - wr_idx;
	if (unlikely(round_down(wr_idx, NFDK_TX_DESC_BLOCK_CNT) !=
		     round_down(wr_idx + cnt - 1, NFDK_TX_DESC_BLOCK_CNT)))
		goto err_warn_overflow;

	skb_tx_timestamp(skb);

	tx_ring->wr_p += cnt;
	if (tx_ring->wr_p % NFDK_TX_DESC_BLOCK_CNT)
		tx_ring->data_pending += skb->len;
	else
		tx_ring->data_pending = 0;

	if (nfp_nfdk_tx_ring_should_stop(tx_ring))
		nfp_nfdk_tx_ring_stop(nd_q, tx_ring);

	tx_ring->wr_ptr_add += cnt;
	if (__netdev_tx_sent_queue(nd_q, real_len, netdev_xmit_more()))
		nfp_net_tx_xmit_more_flush(tx_ring);

	return NETDEV_TX_OK;

err_warn_overflow:
	WARN_ONCE(1, "unable to fit packet into a descriptor wr_idx:%d head:%d frags:%d cnt:%d",
		  wr_idx, skb_headlen(skb), nr_frags, cnt);
	if (skb_is_gso(skb))
		txbuf--;
err_unmap:
	 
	etxbuf = txbuf;
	 
	txbuf = &tx_ring->ktxbufs[wr_idx + 1];
	if (txbuf < etxbuf) {
		dma_unmap_single(dp->dev, txbuf->dma_addr,
				 skb_headlen(skb), DMA_TO_DEVICE);
		txbuf->raw = 0;
		txbuf++;
	}
	frag = skb_shinfo(skb)->frags;
	while (etxbuf < txbuf) {
		dma_unmap_page(dp->dev, txbuf->dma_addr,
			       skb_frag_size(frag), DMA_TO_DEVICE);
		txbuf->raw = 0;
		frag++;
		txbuf++;
	}
err_warn_dma:
	nn_dp_warn(dp, "Failed to map DMA TX buffer\n");
err_flush:
	nfp_net_tx_xmit_more_flush(tx_ring);
	u64_stats_update_begin(&r_vec->tx_sync);
	r_vec->tx_errors++;
	u64_stats_update_end(&r_vec->tx_sync);
	dev_kfree_skb_any(skb);
	return NETDEV_TX_OK;
}

 
static void nfp_nfdk_tx_complete(struct nfp_net_tx_ring *tx_ring, int budget)
{
	struct nfp_net_r_vector *r_vec = tx_ring->r_vec;
	struct nfp_net_dp *dp = &r_vec->nfp_net->dp;
	u32 done_pkts = 0, done_bytes = 0;
	struct nfp_nfdk_tx_buf *ktxbufs;
	struct device *dev = dp->dev;
	struct netdev_queue *nd_q;
	u32 rd_p, qcp_rd_p;
	int todo;

	rd_p = tx_ring->rd_p;
	if (tx_ring->wr_p == rd_p)
		return;

	 
	qcp_rd_p = nfp_net_read_tx_cmpl(tx_ring, dp);

	if (qcp_rd_p == tx_ring->qcp_rd_p)
		return;

	todo = D_IDX(tx_ring, qcp_rd_p - tx_ring->qcp_rd_p);
	ktxbufs = tx_ring->ktxbufs;

	while (todo > 0) {
		const skb_frag_t *frag, *fend;
		unsigned int size, n_descs = 1;
		struct nfp_nfdk_tx_buf *txbuf;
		struct sk_buff *skb;

		txbuf = &ktxbufs[D_IDX(tx_ring, rd_p)];
		skb = txbuf->skb;
		txbuf++;

		 
		if (!skb) {
			n_descs = D_BLOCK_CPL(rd_p);
			goto next;
		}

		 
		size = skb_headlen(skb);
		n_descs += nfp_nfdk_headlen_to_segs(size);
		dma_unmap_single(dev, txbuf->dma_addr, size, DMA_TO_DEVICE);
		txbuf++;

		 
		frag = skb_shinfo(skb)->frags;
		fend = frag + skb_shinfo(skb)->nr_frags;
		for (; frag < fend; frag++) {
			size = skb_frag_size(frag);
			n_descs += DIV_ROUND_UP(size,
						NFDK_TX_MAX_DATA_PER_DESC);
			dma_unmap_page(dev, txbuf->dma_addr,
				       skb_frag_size(frag), DMA_TO_DEVICE);
			txbuf++;
		}

		if (!skb_is_gso(skb)) {
			done_bytes += skb->len;
			done_pkts++;
		} else {
			done_bytes += txbuf->real_len;
			done_pkts += txbuf->pkt_cnt;
			n_descs++;
		}

		napi_consume_skb(skb, budget);
next:
		rd_p += n_descs;
		todo -= n_descs;
	}

	tx_ring->rd_p = rd_p;
	tx_ring->qcp_rd_p = qcp_rd_p;

	u64_stats_update_begin(&r_vec->tx_sync);
	r_vec->tx_bytes += done_bytes;
	r_vec->tx_pkts += done_pkts;
	u64_stats_update_end(&r_vec->tx_sync);

	if (!dp->netdev)
		return;

	nd_q = netdev_get_tx_queue(dp->netdev, tx_ring->idx);
	netdev_tx_completed_queue(nd_q, done_pkts, done_bytes);
	if (nfp_nfdk_tx_ring_should_wake(tx_ring)) {
		 
		smp_mb();

		if (unlikely(netif_tx_queue_stopped(nd_q)))
			netif_tx_wake_queue(nd_q);
	}

	WARN_ONCE(tx_ring->wr_p - tx_ring->rd_p > tx_ring->cnt,
		  "TX ring corruption rd_p=%u wr_p=%u cnt=%u\n",
		  tx_ring->rd_p, tx_ring->wr_p, tx_ring->cnt);
}

 
static void *
nfp_nfdk_napi_alloc_one(struct nfp_net_dp *dp, dma_addr_t *dma_addr)
{
	void *frag;

	if (!dp->xdp_prog) {
		frag = napi_alloc_frag(dp->fl_bufsz);
		if (unlikely(!frag))
			return NULL;
	} else {
		struct page *page;

		page = dev_alloc_page();
		if (unlikely(!page))
			return NULL;
		frag = page_address(page);
	}

	*dma_addr = nfp_net_dma_map_rx(dp, frag);
	if (dma_mapping_error(dp->dev, *dma_addr)) {
		nfp_net_free_frag(frag, dp->xdp_prog);
		nn_dp_warn(dp, "Failed to map DMA RX buffer\n");
		return NULL;
	}

	return frag;
}

 
static void
nfp_nfdk_rx_give_one(const struct nfp_net_dp *dp,
		     struct nfp_net_rx_ring *rx_ring,
		     void *frag, dma_addr_t dma_addr)
{
	unsigned int wr_idx;

	wr_idx = D_IDX(rx_ring, rx_ring->wr_p);

	nfp_net_dma_sync_dev_rx(dp, dma_addr);

	 
	rx_ring->rxbufs[wr_idx].frag = frag;
	rx_ring->rxbufs[wr_idx].dma_addr = dma_addr;

	 
	rx_ring->rxds[wr_idx].fld.reserved = 0;
	rx_ring->rxds[wr_idx].fld.meta_len_dd = 0;
	nfp_desc_set_dma_addr_48b(&rx_ring->rxds[wr_idx].fld,
				  dma_addr + dp->rx_dma_off);

	rx_ring->wr_p++;
	if (!(rx_ring->wr_p % NFP_NET_FL_BATCH)) {
		 
		wmb();
		nfp_qcp_wr_ptr_add(rx_ring->qcp_fl, NFP_NET_FL_BATCH);
	}
}

 
void nfp_nfdk_rx_ring_fill_freelist(struct nfp_net_dp *dp,
				    struct nfp_net_rx_ring *rx_ring)
{
	unsigned int i;

	for (i = 0; i < rx_ring->cnt - 1; i++)
		nfp_nfdk_rx_give_one(dp, rx_ring, rx_ring->rxbufs[i].frag,
				     rx_ring->rxbufs[i].dma_addr);
}

 
static int nfp_nfdk_rx_csum_has_errors(u16 flags)
{
	u16 csum_all_checked, csum_all_ok;

	csum_all_checked = flags & __PCIE_DESC_RX_CSUM_ALL;
	csum_all_ok = flags & __PCIE_DESC_RX_CSUM_ALL_OK;

	return csum_all_checked != (csum_all_ok << PCIE_DESC_RX_CSUM_OK_SHIFT);
}

 
static void
nfp_nfdk_rx_csum(struct nfp_net_dp *dp, struct nfp_net_r_vector *r_vec,
		 struct nfp_net_rx_desc *rxd, struct nfp_meta_parsed *meta,
		 struct sk_buff *skb)
{
	skb_checksum_none_assert(skb);

	if (!(dp->netdev->features & NETIF_F_RXCSUM))
		return;

	if (meta->csum_type) {
		skb->ip_summed = meta->csum_type;
		skb->csum = meta->csum;
		u64_stats_update_begin(&r_vec->rx_sync);
		r_vec->hw_csum_rx_complete++;
		u64_stats_update_end(&r_vec->rx_sync);
		return;
	}

	if (nfp_nfdk_rx_csum_has_errors(le16_to_cpu(rxd->rxd.flags))) {
		u64_stats_update_begin(&r_vec->rx_sync);
		r_vec->hw_csum_rx_error++;
		u64_stats_update_end(&r_vec->rx_sync);
		return;
	}

	 
	if (rxd->rxd.flags & PCIE_DESC_RX_TCP_CSUM_OK ||
	    rxd->rxd.flags & PCIE_DESC_RX_UDP_CSUM_OK) {
		__skb_incr_checksum_unnecessary(skb);
		u64_stats_update_begin(&r_vec->rx_sync);
		r_vec->hw_csum_rx_ok++;
		u64_stats_update_end(&r_vec->rx_sync);
	}

	if (rxd->rxd.flags & PCIE_DESC_RX_I_TCP_CSUM_OK ||
	    rxd->rxd.flags & PCIE_DESC_RX_I_UDP_CSUM_OK) {
		__skb_incr_checksum_unnecessary(skb);
		u64_stats_update_begin(&r_vec->rx_sync);
		r_vec->hw_csum_rx_inner_ok++;
		u64_stats_update_end(&r_vec->rx_sync);
	}
}

static void
nfp_nfdk_set_hash(struct net_device *netdev, struct nfp_meta_parsed *meta,
		  unsigned int type, __be32 *hash)
{
	if (!(netdev->features & NETIF_F_RXHASH))
		return;

	switch (type) {
	case NFP_NET_RSS_IPV4:
	case NFP_NET_RSS_IPV6:
	case NFP_NET_RSS_IPV6_EX:
		meta->hash_type = PKT_HASH_TYPE_L3;
		break;
	default:
		meta->hash_type = PKT_HASH_TYPE_L4;
		break;
	}

	meta->hash = get_unaligned_be32(hash);
}

static bool
nfp_nfdk_parse_meta(struct net_device *netdev, struct nfp_meta_parsed *meta,
		    void *data, void *pkt, unsigned int pkt_len, int meta_len)
{
	u32 meta_info, vlan_info;

	meta_info = get_unaligned_be32(data);
	data += 4;

	while (meta_info) {
		switch (meta_info & NFP_NET_META_FIELD_MASK) {
		case NFP_NET_META_HASH:
			meta_info >>= NFP_NET_META_FIELD_SIZE;
			nfp_nfdk_set_hash(netdev, meta,
					  meta_info & NFP_NET_META_FIELD_MASK,
					  (__be32 *)data);
			data += 4;
			break;
		case NFP_NET_META_MARK:
			meta->mark = get_unaligned_be32(data);
			data += 4;
			break;
		case NFP_NET_META_VLAN:
			vlan_info = get_unaligned_be32(data);
			if (FIELD_GET(NFP_NET_META_VLAN_STRIP, vlan_info)) {
				meta->vlan.stripped = true;
				meta->vlan.tpid = FIELD_GET(NFP_NET_META_VLAN_TPID_MASK,
							    vlan_info);
				meta->vlan.tci = FIELD_GET(NFP_NET_META_VLAN_TCI_MASK,
							   vlan_info);
			}
			data += 4;
			break;
		case NFP_NET_META_PORTID:
			meta->portid = get_unaligned_be32(data);
			data += 4;
			break;
		case NFP_NET_META_CSUM:
			meta->csum_type = CHECKSUM_COMPLETE;
			meta->csum =
				(__force __wsum)__get_unaligned_cpu32(data);
			data += 4;
			break;
		case NFP_NET_META_RESYNC_INFO:
			if (nfp_net_tls_rx_resync_req(netdev, data, pkt,
						      pkt_len))
				return false;
			data += sizeof(struct nfp_net_tls_resync_req);
			break;
#ifdef CONFIG_NFP_NET_IPSEC
		case NFP_NET_META_IPSEC:
			 
			meta->ipsec_saidx = get_unaligned_be32(data) + 1;
			data += 4;
			break;
#endif
		default:
			return true;
		}

		meta_info >>= NFP_NET_META_FIELD_SIZE;
	}

	return data != pkt;
}

static void
nfp_nfdk_rx_drop(const struct nfp_net_dp *dp, struct nfp_net_r_vector *r_vec,
		 struct nfp_net_rx_ring *rx_ring, struct nfp_net_rx_buf *rxbuf,
		 struct sk_buff *skb)
{
	u64_stats_update_begin(&r_vec->rx_sync);
	r_vec->rx_drops++;
	 
	if (skb && rxbuf)
		r_vec->rx_replace_buf_alloc_fail++;
	u64_stats_update_end(&r_vec->rx_sync);

	 
	if (skb && rxbuf && skb->head == rxbuf->frag)
		page_ref_inc(virt_to_head_page(rxbuf->frag));
	if (rxbuf)
		nfp_nfdk_rx_give_one(dp, rx_ring, rxbuf->frag, rxbuf->dma_addr);
	if (skb)
		dev_kfree_skb_any(skb);
}

static bool nfp_nfdk_xdp_complete(struct nfp_net_tx_ring *tx_ring)
{
	struct nfp_net_r_vector *r_vec = tx_ring->r_vec;
	struct nfp_net_dp *dp = &r_vec->nfp_net->dp;
	struct nfp_net_rx_ring *rx_ring;
	u32 qcp_rd_p, done = 0;
	bool done_all;
	int todo;

	 
	qcp_rd_p = nfp_net_read_tx_cmpl(tx_ring, dp);
	if (qcp_rd_p == tx_ring->qcp_rd_p)
		return true;

	todo = D_IDX(tx_ring, qcp_rd_p - tx_ring->qcp_rd_p);

	done_all = todo <= NFP_NET_XDP_MAX_COMPLETE;
	todo = min(todo, NFP_NET_XDP_MAX_COMPLETE);

	rx_ring = r_vec->rx_ring;
	while (todo > 0) {
		int idx = D_IDX(tx_ring, tx_ring->rd_p + done);
		struct nfp_nfdk_tx_buf *txbuf;
		unsigned int step = 1;

		txbuf = &tx_ring->ktxbufs[idx];
		if (!txbuf->raw)
			goto next;

		if (NFDK_TX_BUF_INFO(txbuf->val) != NFDK_TX_BUF_INFO_SOP) {
			WARN_ONCE(1, "Unexpected TX buffer in XDP TX ring\n");
			goto next;
		}

		 
		nfp_nfdk_rx_give_one(dp, rx_ring,
				     (void *)NFDK_TX_BUF_PTR(txbuf[0].val),
				     txbuf[1].dma_addr);
		txbuf[0].raw = 0;
		txbuf[1].raw = 0;
		step = 2;

		u64_stats_update_begin(&r_vec->tx_sync);
		 
		r_vec->tx_pkts++;
		u64_stats_update_end(&r_vec->tx_sync);
next:
		todo -= step;
		done += step;
	}

	tx_ring->qcp_rd_p = D_IDX(tx_ring, tx_ring->qcp_rd_p + done);
	tx_ring->rd_p += done;

	WARN_ONCE(tx_ring->wr_p - tx_ring->rd_p > tx_ring->cnt,
		  "XDP TX ring corruption rd_p=%u wr_p=%u cnt=%u\n",
		  tx_ring->rd_p, tx_ring->wr_p, tx_ring->cnt);

	return done_all;
}

static bool
nfp_nfdk_tx_xdp_buf(struct nfp_net_dp *dp, struct nfp_net_rx_ring *rx_ring,
		    struct nfp_net_tx_ring *tx_ring,
		    struct nfp_net_rx_buf *rxbuf, unsigned int dma_off,
		    unsigned int pkt_len, bool *completed)
{
	unsigned int dma_map_sz = dp->fl_bufsz - NFP_NET_RX_BUF_NON_DATA;
	unsigned int dma_len, type, cnt, dlen_type, tmp_dlen;
	struct nfp_nfdk_tx_buf *txbuf;
	struct nfp_nfdk_tx_desc *txd;
	unsigned int n_descs;
	dma_addr_t dma_addr;
	int wr_idx;

	 
	if (pkt_len + dma_off > dma_map_sz)
		return false;

	 
	if (unlikely(nfp_net_tx_full(tx_ring, NFDK_TX_DESC_STOP_CNT))) {
		if (!*completed) {
			nfp_nfdk_xdp_complete(tx_ring);
			*completed = true;
		}

		if (unlikely(nfp_net_tx_full(tx_ring, NFDK_TX_DESC_STOP_CNT))) {
			nfp_nfdk_rx_drop(dp, rx_ring->r_vec, rx_ring, rxbuf,
					 NULL);
			return false;
		}
	}

	 
	n_descs = nfp_nfdk_headlen_to_segs(pkt_len);
	if ((round_down(tx_ring->wr_p, NFDK_TX_DESC_BLOCK_CNT) !=
	     round_down(tx_ring->wr_p + n_descs, NFDK_TX_DESC_BLOCK_CNT)) ||
	    ((u32)tx_ring->data_pending + pkt_len >
	     NFDK_TX_MAX_DATA_PER_BLOCK)) {
		unsigned int nop_slots = D_BLOCK_CPL(tx_ring->wr_p);

		wr_idx = D_IDX(tx_ring, tx_ring->wr_p);
		txd = &tx_ring->ktxds[wr_idx];
		memset(txd, 0,
		       array_size(nop_slots, sizeof(struct nfp_nfdk_tx_desc)));

		tx_ring->data_pending = 0;
		tx_ring->wr_p += nop_slots;
		tx_ring->wr_ptr_add += nop_slots;
	}

	wr_idx = D_IDX(tx_ring, tx_ring->wr_p);

	txbuf = &tx_ring->ktxbufs[wr_idx];

	txbuf[0].val = (unsigned long)rxbuf->frag | NFDK_TX_BUF_INFO_SOP;
	txbuf[1].dma_addr = rxbuf->dma_addr;
	 

	dma_sync_single_for_device(dp->dev, rxbuf->dma_addr + dma_off,
				   pkt_len, DMA_BIDIRECTIONAL);

	 
	txd = &tx_ring->ktxds[wr_idx];
	dma_len = pkt_len;
	dma_addr = rxbuf->dma_addr + dma_off;

	if (dma_len <= NFDK_TX_MAX_DATA_PER_HEAD)
		type = NFDK_DESC_TX_TYPE_SIMPLE;
	else
		type = NFDK_DESC_TX_TYPE_GATHER;

	 
	dma_len -= 1;
	dlen_type = FIELD_PREP(NFDK_DESC_TX_DMA_LEN_HEAD,
			       dma_len > NFDK_DESC_TX_DMA_LEN_HEAD ?
			       NFDK_DESC_TX_DMA_LEN_HEAD : dma_len) |
		    FIELD_PREP(NFDK_DESC_TX_TYPE_HEAD, type);

	txd->dma_len_type = cpu_to_le16(dlen_type);
	nfp_desc_set_dma_addr_48b(txd, dma_addr);

	tmp_dlen = dlen_type & NFDK_DESC_TX_DMA_LEN_HEAD;
	dma_len -= tmp_dlen;
	dma_addr += tmp_dlen + 1;
	txd++;

	while (dma_len > 0) {
		dma_len -= 1;
		dlen_type = FIELD_PREP(NFDK_DESC_TX_DMA_LEN, dma_len);
		txd->dma_len_type = cpu_to_le16(dlen_type);
		nfp_desc_set_dma_addr_48b(txd, dma_addr);

		dlen_type &= NFDK_DESC_TX_DMA_LEN;
		dma_len -= dlen_type;
		dma_addr += dlen_type + 1;
		txd++;
	}

	(txd - 1)->dma_len_type = cpu_to_le16(dlen_type | NFDK_DESC_TX_EOP);

	 
	txd->raw = 0;
	txd++;

	cnt = txd - tx_ring->ktxds - wr_idx;
	tx_ring->wr_p += cnt;
	if (tx_ring->wr_p % NFDK_TX_DESC_BLOCK_CNT)
		tx_ring->data_pending += pkt_len;
	else
		tx_ring->data_pending = 0;

	tx_ring->wr_ptr_add += cnt;
	return true;
}

 
static int nfp_nfdk_rx(struct nfp_net_rx_ring *rx_ring, int budget)
{
	struct nfp_net_r_vector *r_vec = rx_ring->r_vec;
	struct nfp_net_dp *dp = &r_vec->nfp_net->dp;
	struct nfp_net_tx_ring *tx_ring;
	struct bpf_prog *xdp_prog;
	bool xdp_tx_cmpl = false;
	unsigned int true_bufsz;
	struct sk_buff *skb;
	int pkts_polled = 0;
	struct xdp_buff xdp;
	int idx;

	xdp_prog = READ_ONCE(dp->xdp_prog);
	true_bufsz = xdp_prog ? PAGE_SIZE : dp->fl_bufsz;
	xdp_init_buff(&xdp, PAGE_SIZE - NFP_NET_RX_BUF_HEADROOM,
		      &rx_ring->xdp_rxq);
	tx_ring = r_vec->xdp_ring;

	while (pkts_polled < budget) {
		unsigned int meta_len, data_len, meta_off, pkt_len, pkt_off;
		struct nfp_net_rx_buf *rxbuf;
		struct nfp_net_rx_desc *rxd;
		struct nfp_meta_parsed meta;
		bool redir_egress = false;
		struct net_device *netdev;
		dma_addr_t new_dma_addr;
		u32 meta_len_xdp = 0;
		void *new_frag;

		idx = D_IDX(rx_ring, rx_ring->rd_p);

		rxd = &rx_ring->rxds[idx];
		if (!(rxd->rxd.meta_len_dd & PCIE_DESC_RX_DD))
			break;

		 
		dma_rmb();

		memset(&meta, 0, sizeof(meta));

		rx_ring->rd_p++;
		pkts_polled++;

		rxbuf =	&rx_ring->rxbufs[idx];
		 
		meta_len = rxd->rxd.meta_len_dd & PCIE_DESC_RX_META_LEN_MASK;
		data_len = le16_to_cpu(rxd->rxd.data_len);
		pkt_len = data_len - meta_len;

		pkt_off = NFP_NET_RX_BUF_HEADROOM + dp->rx_dma_off;
		if (dp->rx_offset == NFP_NET_CFG_RX_OFFSET_DYNAMIC)
			pkt_off += meta_len;
		else
			pkt_off += dp->rx_offset;
		meta_off = pkt_off - meta_len;

		 
		u64_stats_update_begin(&r_vec->rx_sync);
		r_vec->rx_pkts++;
		r_vec->rx_bytes += pkt_len;
		u64_stats_update_end(&r_vec->rx_sync);

		if (unlikely(meta_len > NFP_NET_MAX_PREPEND ||
			     (dp->rx_offset && meta_len > dp->rx_offset))) {
			nn_dp_warn(dp, "oversized RX packet metadata %u\n",
				   meta_len);
			nfp_nfdk_rx_drop(dp, r_vec, rx_ring, rxbuf, NULL);
			continue;
		}

		nfp_net_dma_sync_cpu_rx(dp, rxbuf->dma_addr + meta_off,
					data_len);

		if (meta_len) {
			if (unlikely(nfp_nfdk_parse_meta(dp->netdev, &meta,
							 rxbuf->frag + meta_off,
							 rxbuf->frag + pkt_off,
							 pkt_len, meta_len))) {
				nn_dp_warn(dp, "invalid RX packet metadata\n");
				nfp_nfdk_rx_drop(dp, r_vec, rx_ring, rxbuf,
						 NULL);
				continue;
			}
		}

		if (xdp_prog && !meta.portid) {
			void *orig_data = rxbuf->frag + pkt_off;
			unsigned int dma_off;
			int act;

			xdp_prepare_buff(&xdp,
					 rxbuf->frag + NFP_NET_RX_BUF_HEADROOM,
					 pkt_off - NFP_NET_RX_BUF_HEADROOM,
					 pkt_len, true);

			act = bpf_prog_run_xdp(xdp_prog, &xdp);

			pkt_len = xdp.data_end - xdp.data;
			pkt_off += xdp.data - orig_data;

			switch (act) {
			case XDP_PASS:
				meta_len_xdp = xdp.data - xdp.data_meta;
				break;
			case XDP_TX:
				dma_off = pkt_off - NFP_NET_RX_BUF_HEADROOM;
				if (unlikely(!nfp_nfdk_tx_xdp_buf(dp, rx_ring,
								  tx_ring,
								  rxbuf,
								  dma_off,
								  pkt_len,
								  &xdp_tx_cmpl)))
					trace_xdp_exception(dp->netdev,
							    xdp_prog, act);
				continue;
			default:
				bpf_warn_invalid_xdp_action(dp->netdev, xdp_prog, act);
				fallthrough;
			case XDP_ABORTED:
				trace_xdp_exception(dp->netdev, xdp_prog, act);
				fallthrough;
			case XDP_DROP:
				nfp_nfdk_rx_give_one(dp, rx_ring, rxbuf->frag,
						     rxbuf->dma_addr);
				continue;
			}
		}

		if (likely(!meta.portid)) {
			netdev = dp->netdev;
		} else if (meta.portid == NFP_META_PORT_ID_CTRL) {
			struct nfp_net *nn = netdev_priv(dp->netdev);

			nfp_app_ctrl_rx_raw(nn->app, rxbuf->frag + pkt_off,
					    pkt_len);
			nfp_nfdk_rx_give_one(dp, rx_ring, rxbuf->frag,
					     rxbuf->dma_addr);
			continue;
		} else {
			struct nfp_net *nn;

			nn = netdev_priv(dp->netdev);
			netdev = nfp_app_dev_get(nn->app, meta.portid,
						 &redir_egress);
			if (unlikely(!netdev)) {
				nfp_nfdk_rx_drop(dp, r_vec, rx_ring, rxbuf,
						 NULL);
				continue;
			}

			if (nfp_netdev_is_nfp_repr(netdev))
				nfp_repr_inc_rx_stats(netdev, pkt_len);
		}

		skb = build_skb(rxbuf->frag, true_bufsz);
		if (unlikely(!skb)) {
			nfp_nfdk_rx_drop(dp, r_vec, rx_ring, rxbuf, NULL);
			continue;
		}
		new_frag = nfp_nfdk_napi_alloc_one(dp, &new_dma_addr);
		if (unlikely(!new_frag)) {
			nfp_nfdk_rx_drop(dp, r_vec, rx_ring, rxbuf, skb);
			continue;
		}

		nfp_net_dma_unmap_rx(dp, rxbuf->dma_addr);

		nfp_nfdk_rx_give_one(dp, rx_ring, new_frag, new_dma_addr);

		skb_reserve(skb, pkt_off);
		skb_put(skb, pkt_len);

		skb->mark = meta.mark;
		skb_set_hash(skb, meta.hash, meta.hash_type);

		skb_record_rx_queue(skb, rx_ring->idx);
		skb->protocol = eth_type_trans(skb, netdev);

		nfp_nfdk_rx_csum(dp, r_vec, rxd, &meta, skb);

		if (unlikely(!nfp_net_vlan_strip(skb, rxd, &meta))) {
			nfp_nfdk_rx_drop(dp, r_vec, rx_ring, NULL, skb);
			continue;
		}

#ifdef CONFIG_NFP_NET_IPSEC
		if (meta.ipsec_saidx != 0 && unlikely(nfp_net_ipsec_rx(&meta, skb))) {
			nfp_nfdk_rx_drop(dp, r_vec, rx_ring, NULL, skb);
			continue;
		}
#endif

		if (meta_len_xdp)
			skb_metadata_set(skb, meta_len_xdp);

		if (likely(!redir_egress)) {
			napi_gro_receive(&rx_ring->r_vec->napi, skb);
		} else {
			skb->dev = netdev;
			skb_reset_network_header(skb);
			__skb_push(skb, ETH_HLEN);
			dev_queue_xmit(skb);
		}
	}

	if (xdp_prog) {
		if (tx_ring->wr_ptr_add)
			nfp_net_tx_xmit_more_flush(tx_ring);
		else if (unlikely(tx_ring->wr_p != tx_ring->rd_p) &&
			 !xdp_tx_cmpl)
			if (!nfp_nfdk_xdp_complete(tx_ring))
				pkts_polled = budget;
	}

	return pkts_polled;
}

 
int nfp_nfdk_poll(struct napi_struct *napi, int budget)
{
	struct nfp_net_r_vector *r_vec =
		container_of(napi, struct nfp_net_r_vector, napi);
	unsigned int pkts_polled = 0;

	if (r_vec->tx_ring)
		nfp_nfdk_tx_complete(r_vec->tx_ring, budget);
	if (r_vec->rx_ring)
		pkts_polled = nfp_nfdk_rx(r_vec->rx_ring, budget);

	if (pkts_polled < budget)
		if (napi_complete_done(napi, pkts_polled))
			nfp_net_irq_unmask(r_vec->nfp_net, r_vec->irq_entry);

	if (r_vec->nfp_net->rx_coalesce_adapt_on && r_vec->rx_ring) {
		struct dim_sample dim_sample = {};
		unsigned int start;
		u64 pkts, bytes;

		do {
			start = u64_stats_fetch_begin(&r_vec->rx_sync);
			pkts = r_vec->rx_pkts;
			bytes = r_vec->rx_bytes;
		} while (u64_stats_fetch_retry(&r_vec->rx_sync, start));

		dim_update_sample(r_vec->event_ctr, pkts, bytes, &dim_sample);
		net_dim(&r_vec->rx_dim, dim_sample);
	}

	if (r_vec->nfp_net->tx_coalesce_adapt_on && r_vec->tx_ring) {
		struct dim_sample dim_sample = {};
		unsigned int start;
		u64 pkts, bytes;

		do {
			start = u64_stats_fetch_begin(&r_vec->tx_sync);
			pkts = r_vec->tx_pkts;
			bytes = r_vec->tx_bytes;
		} while (u64_stats_fetch_retry(&r_vec->tx_sync, start));

		dim_update_sample(r_vec->event_ctr, pkts, bytes, &dim_sample);
		net_dim(&r_vec->tx_dim, dim_sample);
	}

	return pkts_polled;
}

 

bool
nfp_nfdk_ctrl_tx_one(struct nfp_net *nn, struct nfp_net_r_vector *r_vec,
		     struct sk_buff *skb, bool old)
{
	u32 cnt, tmp_dlen, dlen_type = 0;
	struct nfp_net_tx_ring *tx_ring;
	struct nfp_nfdk_tx_buf *txbuf;
	struct nfp_nfdk_tx_desc *txd;
	unsigned int dma_len, type;
	struct nfp_net_dp *dp;
	dma_addr_t dma_addr;
	u64 metadata = 0;
	int wr_idx;

	dp = &r_vec->nfp_net->dp;
	tx_ring = r_vec->tx_ring;

	if (WARN_ON_ONCE(skb_shinfo(skb)->nr_frags)) {
		nn_dp_warn(dp, "Driver's CTRL TX does not implement gather\n");
		goto err_free;
	}

	 
	if (unlikely(nfp_net_tx_full(tx_ring, NFDK_TX_DESC_STOP_CNT))) {
		u64_stats_update_begin(&r_vec->tx_sync);
		r_vec->tx_busy++;
		u64_stats_update_end(&r_vec->tx_sync);
		if (!old)
			__skb_queue_tail(&r_vec->queue, skb);
		else
			__skb_queue_head(&r_vec->queue, skb);
		return NETDEV_TX_BUSY;
	}

	if (nfp_app_ctrl_has_meta(nn->app)) {
		if (unlikely(skb_headroom(skb) < 8)) {
			nn_dp_warn(dp, "CTRL TX on skb without headroom\n");
			goto err_free;
		}
		metadata = NFDK_DESC_TX_CHAIN_META;
		put_unaligned_be32(NFP_META_PORT_ID_CTRL, skb_push(skb, 4));
		put_unaligned_be32(FIELD_PREP(NFDK_META_LEN, 8) |
				   FIELD_PREP(NFDK_META_FIELDS,
					      NFP_NET_META_PORTID),
				   skb_push(skb, 4));
	}

	if (nfp_nfdk_tx_maybe_close_block(tx_ring, skb))
		goto err_free;

	 
	wr_idx = D_IDX(tx_ring, tx_ring->wr_p);
	txd = &tx_ring->ktxds[wr_idx];
	txbuf = &tx_ring->ktxbufs[wr_idx];

	dma_len = skb_headlen(skb);
	if (dma_len <= NFDK_TX_MAX_DATA_PER_HEAD)
		type = NFDK_DESC_TX_TYPE_SIMPLE;
	else
		type = NFDK_DESC_TX_TYPE_GATHER;

	dma_addr = dma_map_single(dp->dev, skb->data, dma_len, DMA_TO_DEVICE);
	if (dma_mapping_error(dp->dev, dma_addr))
		goto err_warn_dma;

	txbuf->skb = skb;
	txbuf++;

	txbuf->dma_addr = dma_addr;
	txbuf++;

	dma_len -= 1;
	dlen_type = FIELD_PREP(NFDK_DESC_TX_DMA_LEN_HEAD,
			       dma_len > NFDK_DESC_TX_DMA_LEN_HEAD ?
			       NFDK_DESC_TX_DMA_LEN_HEAD : dma_len) |
		    FIELD_PREP(NFDK_DESC_TX_TYPE_HEAD, type);

	txd->dma_len_type = cpu_to_le16(dlen_type);
	nfp_desc_set_dma_addr_48b(txd, dma_addr);

	tmp_dlen = dlen_type & NFDK_DESC_TX_DMA_LEN_HEAD;
	dma_len -= tmp_dlen;
	dma_addr += tmp_dlen + 1;
	txd++;

	while (dma_len > 0) {
		dma_len -= 1;
		dlen_type = FIELD_PREP(NFDK_DESC_TX_DMA_LEN, dma_len);
		txd->dma_len_type = cpu_to_le16(dlen_type);
		nfp_desc_set_dma_addr_48b(txd, dma_addr);

		dlen_type &= NFDK_DESC_TX_DMA_LEN;
		dma_len -= dlen_type;
		dma_addr += dlen_type + 1;
		txd++;
	}

	(txd - 1)->dma_len_type = cpu_to_le16(dlen_type | NFDK_DESC_TX_EOP);

	 
	txd->raw = cpu_to_le64(metadata);
	txd++;

	cnt = txd - tx_ring->ktxds - wr_idx;
	if (unlikely(round_down(wr_idx, NFDK_TX_DESC_BLOCK_CNT) !=
		     round_down(wr_idx + cnt - 1, NFDK_TX_DESC_BLOCK_CNT)))
		goto err_warn_overflow;

	tx_ring->wr_p += cnt;
	if (tx_ring->wr_p % NFDK_TX_DESC_BLOCK_CNT)
		tx_ring->data_pending += skb->len;
	else
		tx_ring->data_pending = 0;

	tx_ring->wr_ptr_add += cnt;
	nfp_net_tx_xmit_more_flush(tx_ring);

	return NETDEV_TX_OK;

err_warn_overflow:
	WARN_ONCE(1, "unable to fit packet into a descriptor wr_idx:%d head:%d frags:%d cnt:%d",
		  wr_idx, skb_headlen(skb), 0, cnt);
	txbuf--;
	dma_unmap_single(dp->dev, txbuf->dma_addr,
			 skb_headlen(skb), DMA_TO_DEVICE);
	txbuf->raw = 0;
err_warn_dma:
	nn_dp_warn(dp, "Failed to map DMA TX buffer\n");
err_free:
	u64_stats_update_begin(&r_vec->tx_sync);
	r_vec->tx_errors++;
	u64_stats_update_end(&r_vec->tx_sync);
	dev_kfree_skb_any(skb);
	return NETDEV_TX_OK;
}

static void __nfp_ctrl_tx_queued(struct nfp_net_r_vector *r_vec)
{
	struct sk_buff *skb;

	while ((skb = __skb_dequeue(&r_vec->queue)))
		if (nfp_nfdk_ctrl_tx_one(r_vec->nfp_net, r_vec, skb, true))
			return;
}

static bool
nfp_ctrl_meta_ok(struct nfp_net *nn, void *data, unsigned int meta_len)
{
	u32 meta_type, meta_tag;

	if (!nfp_app_ctrl_has_meta(nn->app))
		return !meta_len;

	if (meta_len != 8)
		return false;

	meta_type = get_unaligned_be32(data);
	meta_tag = get_unaligned_be32(data + 4);

	return (meta_type == NFP_NET_META_PORTID &&
		meta_tag == NFP_META_PORT_ID_CTRL);
}

static bool
nfp_ctrl_rx_one(struct nfp_net *nn, struct nfp_net_dp *dp,
		struct nfp_net_r_vector *r_vec, struct nfp_net_rx_ring *rx_ring)
{
	unsigned int meta_len, data_len, meta_off, pkt_len, pkt_off;
	struct nfp_net_rx_buf *rxbuf;
	struct nfp_net_rx_desc *rxd;
	dma_addr_t new_dma_addr;
	struct sk_buff *skb;
	void *new_frag;
	int idx;

	idx = D_IDX(rx_ring, rx_ring->rd_p);

	rxd = &rx_ring->rxds[idx];
	if (!(rxd->rxd.meta_len_dd & PCIE_DESC_RX_DD))
		return false;

	 
	dma_rmb();

	rx_ring->rd_p++;

	rxbuf =	&rx_ring->rxbufs[idx];
	meta_len = rxd->rxd.meta_len_dd & PCIE_DESC_RX_META_LEN_MASK;
	data_len = le16_to_cpu(rxd->rxd.data_len);
	pkt_len = data_len - meta_len;

	pkt_off = NFP_NET_RX_BUF_HEADROOM + dp->rx_dma_off;
	if (dp->rx_offset == NFP_NET_CFG_RX_OFFSET_DYNAMIC)
		pkt_off += meta_len;
	else
		pkt_off += dp->rx_offset;
	meta_off = pkt_off - meta_len;

	 
	u64_stats_update_begin(&r_vec->rx_sync);
	r_vec->rx_pkts++;
	r_vec->rx_bytes += pkt_len;
	u64_stats_update_end(&r_vec->rx_sync);

	nfp_net_dma_sync_cpu_rx(dp, rxbuf->dma_addr + meta_off,	data_len);

	if (unlikely(!nfp_ctrl_meta_ok(nn, rxbuf->frag + meta_off, meta_len))) {
		nn_dp_warn(dp, "incorrect metadata for ctrl packet (%d)\n",
			   meta_len);
		nfp_nfdk_rx_drop(dp, r_vec, rx_ring, rxbuf, NULL);
		return true;
	}

	skb = build_skb(rxbuf->frag, dp->fl_bufsz);
	if (unlikely(!skb)) {
		nfp_nfdk_rx_drop(dp, r_vec, rx_ring, rxbuf, NULL);
		return true;
	}
	new_frag = nfp_nfdk_napi_alloc_one(dp, &new_dma_addr);
	if (unlikely(!new_frag)) {
		nfp_nfdk_rx_drop(dp, r_vec, rx_ring, rxbuf, skb);
		return true;
	}

	nfp_net_dma_unmap_rx(dp, rxbuf->dma_addr);

	nfp_nfdk_rx_give_one(dp, rx_ring, new_frag, new_dma_addr);

	skb_reserve(skb, pkt_off);
	skb_put(skb, pkt_len);

	nfp_app_ctrl_rx(nn->app, skb);

	return true;
}

static bool nfp_ctrl_rx(struct nfp_net_r_vector *r_vec)
{
	struct nfp_net_rx_ring *rx_ring = r_vec->rx_ring;
	struct nfp_net *nn = r_vec->nfp_net;
	struct nfp_net_dp *dp = &nn->dp;
	unsigned int budget = 512;

	while (nfp_ctrl_rx_one(nn, dp, r_vec, rx_ring) && budget--)
		continue;

	return budget;
}

void nfp_nfdk_ctrl_poll(struct tasklet_struct *t)
{
	struct nfp_net_r_vector *r_vec = from_tasklet(r_vec, t, tasklet);

	spin_lock(&r_vec->lock);
	nfp_nfdk_tx_complete(r_vec->tx_ring, 0);
	__nfp_ctrl_tx_queued(r_vec);
	spin_unlock(&r_vec->lock);

	if (nfp_ctrl_rx(r_vec)) {
		nfp_net_irq_unmask(r_vec->nfp_net, r_vec->irq_entry);
	} else {
		tasklet_schedule(&r_vec->tasklet);
		nn_dp_warn(&r_vec->nfp_net->dp,
			   "control message budget exceeded!\n");
	}
}
