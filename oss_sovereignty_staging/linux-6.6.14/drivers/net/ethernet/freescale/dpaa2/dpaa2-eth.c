
 
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/etherdevice.h>
#include <linux/of_net.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/iommu.h>
#include <linux/fsl/mc.h>
#include <linux/bpf.h>
#include <linux/bpf_trace.h>
#include <linux/fsl/ptp_qoriq.h>
#include <linux/ptp_classify.h>
#include <net/pkt_cls.h>
#include <net/sock.h>
#include <net/tso.h>
#include <net/xdp_sock_drv.h>

#include "dpaa2-eth.h"

 
#define CREATE_TRACE_POINTS
#include "dpaa2-eth-trace.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Freescale Semiconductor, Inc");
MODULE_DESCRIPTION("Freescale DPAA2 Ethernet Driver");

struct ptp_qoriq *dpaa2_ptp;
EXPORT_SYMBOL(dpaa2_ptp);

static void dpaa2_eth_detect_features(struct dpaa2_eth_priv *priv)
{
	priv->features = 0;

	if (dpaa2_eth_cmp_dpni_ver(priv, DPNI_PTP_ONESTEP_VER_MAJOR,
				   DPNI_PTP_ONESTEP_VER_MINOR) >= 0)
		priv->features |= DPAA2_ETH_FEATURE_ONESTEP_CFG_DIRECT;
}

static void dpaa2_update_ptp_onestep_indirect(struct dpaa2_eth_priv *priv,
					      u32 offset, u8 udp)
{
	struct dpni_single_step_cfg cfg;

	cfg.en = 1;
	cfg.ch_update = udp;
	cfg.offset = offset;
	cfg.peer_delay = 0;

	if (dpni_set_single_step_cfg(priv->mc_io, 0, priv->mc_token, &cfg))
		WARN_ONCE(1, "Failed to set single step register");
}

static void dpaa2_update_ptp_onestep_direct(struct dpaa2_eth_priv *priv,
					    u32 offset, u8 udp)
{
	u32 val = 0;

	val = DPAA2_PTP_SINGLE_STEP_ENABLE |
	       DPAA2_PTP_SINGLE_CORRECTION_OFF(offset);

	if (udp)
		val |= DPAA2_PTP_SINGLE_STEP_CH;

	if (priv->onestep_reg_base)
		writel(val, priv->onestep_reg_base);
}

static void dpaa2_ptp_onestep_reg_update_method(struct dpaa2_eth_priv *priv)
{
	struct device *dev = priv->net_dev->dev.parent;
	struct dpni_single_step_cfg ptp_cfg;

	priv->dpaa2_set_onestep_params_cb = dpaa2_update_ptp_onestep_indirect;

	if (!(priv->features & DPAA2_ETH_FEATURE_ONESTEP_CFG_DIRECT))
		return;

	if (dpni_get_single_step_cfg(priv->mc_io, 0,
				     priv->mc_token, &ptp_cfg)) {
		dev_err(dev, "dpni_get_single_step_cfg cannot retrieve onestep reg, falling back to indirect update\n");
		return;
	}

	if (!ptp_cfg.ptp_onestep_reg_base) {
		dev_err(dev, "1588 onestep reg not available, falling back to indirect update\n");
		return;
	}

	priv->onestep_reg_base = ioremap(ptp_cfg.ptp_onestep_reg_base,
					 sizeof(u32));
	if (!priv->onestep_reg_base) {
		dev_err(dev, "1588 onestep reg cannot be mapped, falling back to indirect update\n");
		return;
	}

	priv->dpaa2_set_onestep_params_cb = dpaa2_update_ptp_onestep_direct;
}

void *dpaa2_iova_to_virt(struct iommu_domain *domain,
			 dma_addr_t iova_addr)
{
	phys_addr_t phys_addr;

	phys_addr = domain ? iommu_iova_to_phys(domain, iova_addr) : iova_addr;

	return phys_to_virt(phys_addr);
}

static void dpaa2_eth_validate_rx_csum(struct dpaa2_eth_priv *priv,
				       u32 fd_status,
				       struct sk_buff *skb)
{
	skb_checksum_none_assert(skb);

	 
	if (!(priv->net_dev->features & NETIF_F_RXCSUM))
		return;

	 
	if (!((fd_status & DPAA2_FAS_L3CV) &&
	      (fd_status & DPAA2_FAS_L4CV)))
		return;

	 
	skb->ip_summed = CHECKSUM_UNNECESSARY;
}

 
static void dpaa2_eth_free_rx_fd(struct dpaa2_eth_priv *priv,
				 const struct dpaa2_fd *fd,
				 void *vaddr)
{
	struct device *dev = priv->net_dev->dev.parent;
	dma_addr_t addr = dpaa2_fd_get_addr(fd);
	u8 fd_format = dpaa2_fd_get_format(fd);
	struct dpaa2_sg_entry *sgt;
	void *sg_vaddr;
	int i;

	 
	if (fd_format == dpaa2_fd_single)
		goto free_buf;
	else if (fd_format != dpaa2_fd_sg)
		 
		return;

	 
	sgt = vaddr + dpaa2_fd_get_offset(fd);
	for (i = 1; i < DPAA2_ETH_MAX_SG_ENTRIES; i++) {
		addr = dpaa2_sg_get_addr(&sgt[i]);
		sg_vaddr = dpaa2_iova_to_virt(priv->iommu_domain, addr);
		dma_unmap_page(dev, addr, priv->rx_buf_size,
			       DMA_BIDIRECTIONAL);

		free_pages((unsigned long)sg_vaddr, 0);
		if (dpaa2_sg_is_final(&sgt[i]))
			break;
	}

free_buf:
	free_pages((unsigned long)vaddr, 0);
}

 
static struct sk_buff *dpaa2_eth_build_linear_skb(struct dpaa2_eth_channel *ch,
						  const struct dpaa2_fd *fd,
						  void *fd_vaddr)
{
	struct sk_buff *skb = NULL;
	u16 fd_offset = dpaa2_fd_get_offset(fd);
	u32 fd_length = dpaa2_fd_get_len(fd);

	ch->buf_count--;

	skb = build_skb(fd_vaddr, DPAA2_ETH_RX_BUF_RAW_SIZE);
	if (unlikely(!skb))
		return NULL;

	skb_reserve(skb, fd_offset);
	skb_put(skb, fd_length);

	return skb;
}

 
static struct sk_buff *dpaa2_eth_build_frag_skb(struct dpaa2_eth_priv *priv,
						struct dpaa2_eth_channel *ch,
						struct dpaa2_sg_entry *sgt)
{
	struct sk_buff *skb = NULL;
	struct device *dev = priv->net_dev->dev.parent;
	void *sg_vaddr;
	dma_addr_t sg_addr;
	u16 sg_offset;
	u32 sg_length;
	struct page *page, *head_page;
	int page_offset;
	int i;

	for (i = 0; i < DPAA2_ETH_MAX_SG_ENTRIES; i++) {
		struct dpaa2_sg_entry *sge = &sgt[i];

		 

		 
		sg_addr = dpaa2_sg_get_addr(sge);
		sg_vaddr = dpaa2_iova_to_virt(priv->iommu_domain, sg_addr);
		dma_unmap_page(dev, sg_addr, priv->rx_buf_size,
			       DMA_BIDIRECTIONAL);

		sg_length = dpaa2_sg_get_len(sge);

		if (i == 0) {
			 
			skb = build_skb(sg_vaddr, DPAA2_ETH_RX_BUF_RAW_SIZE);
			if (unlikely(!skb)) {
				 
				free_pages((unsigned long)sg_vaddr, 0);

				 
				while (!dpaa2_sg_is_final(&sgt[i]) &&
				       i < DPAA2_ETH_MAX_SG_ENTRIES)
					i++;
				break;
			}

			sg_offset = dpaa2_sg_get_offset(sge);
			skb_reserve(skb, sg_offset);
			skb_put(skb, sg_length);
		} else {
			 
			page = virt_to_page(sg_vaddr);
			head_page = virt_to_head_page(sg_vaddr);

			 
			page_offset = ((unsigned long)sg_vaddr &
				(PAGE_SIZE - 1)) +
				(page_address(page) - page_address(head_page));

			skb_add_rx_frag(skb, i - 1, head_page, page_offset,
					sg_length, priv->rx_buf_size);
		}

		if (dpaa2_sg_is_final(sge))
			break;
	}

	WARN_ONCE(i == DPAA2_ETH_MAX_SG_ENTRIES, "Final bit not set in SGT");

	 
	ch->buf_count -= i + 2;

	return skb;
}

 
static void dpaa2_eth_free_bufs(struct dpaa2_eth_priv *priv, u64 *buf_array,
				int count, bool xsk_zc)
{
	struct device *dev = priv->net_dev->dev.parent;
	struct dpaa2_eth_swa *swa;
	struct xdp_buff *xdp_buff;
	void *vaddr;
	int i;

	for (i = 0; i < count; i++) {
		vaddr = dpaa2_iova_to_virt(priv->iommu_domain, buf_array[i]);

		if (!xsk_zc) {
			dma_unmap_page(dev, buf_array[i], priv->rx_buf_size,
				       DMA_BIDIRECTIONAL);
			free_pages((unsigned long)vaddr, 0);
		} else {
			swa = (struct dpaa2_eth_swa *)
				(vaddr + DPAA2_ETH_RX_HWA_SIZE);
			xdp_buff = swa->xsk.xdp_buff;
			xsk_buff_free(xdp_buff);
		}
	}
}

void dpaa2_eth_recycle_buf(struct dpaa2_eth_priv *priv,
			   struct dpaa2_eth_channel *ch,
			   dma_addr_t addr)
{
	int retries = 0;
	int err;

	ch->recycled_bufs[ch->recycled_bufs_cnt++] = addr;
	if (ch->recycled_bufs_cnt < DPAA2_ETH_BUFS_PER_CMD)
		return;

	while ((err = dpaa2_io_service_release(ch->dpio, ch->bp->bpid,
					       ch->recycled_bufs,
					       ch->recycled_bufs_cnt)) == -EBUSY) {
		if (retries++ >= DPAA2_ETH_SWP_BUSY_RETRIES)
			break;
		cpu_relax();
	}

	if (err) {
		dpaa2_eth_free_bufs(priv, ch->recycled_bufs,
				    ch->recycled_bufs_cnt, ch->xsk_zc);
		ch->buf_count -= ch->recycled_bufs_cnt;
	}

	ch->recycled_bufs_cnt = 0;
}

static int dpaa2_eth_xdp_flush(struct dpaa2_eth_priv *priv,
			       struct dpaa2_eth_fq *fq,
			       struct dpaa2_eth_xdp_fds *xdp_fds)
{
	int total_enqueued = 0, retries = 0, enqueued;
	struct dpaa2_eth_drv_stats *percpu_extras;
	int num_fds, err, max_retries;
	struct dpaa2_fd *fds;

	percpu_extras = this_cpu_ptr(priv->percpu_extras);

	 
	fds = xdp_fds->fds;
	num_fds = xdp_fds->num;
	max_retries = num_fds * DPAA2_ETH_ENQUEUE_RETRIES;
	while (total_enqueued < num_fds && retries < max_retries) {
		err = priv->enqueue(priv, fq, &fds[total_enqueued],
				    0, num_fds - total_enqueued, &enqueued);
		if (err == -EBUSY) {
			percpu_extras->tx_portal_busy += ++retries;
			continue;
		}
		total_enqueued += enqueued;
	}
	xdp_fds->num = 0;

	return total_enqueued;
}

static void dpaa2_eth_xdp_tx_flush(struct dpaa2_eth_priv *priv,
				   struct dpaa2_eth_channel *ch,
				   struct dpaa2_eth_fq *fq)
{
	struct rtnl_link_stats64 *percpu_stats;
	struct dpaa2_fd *fds;
	int enqueued, i;

	percpu_stats = this_cpu_ptr(priv->percpu_stats);

	 
	enqueued = dpaa2_eth_xdp_flush(priv, fq, &fq->xdp_tx_fds);

	 
	percpu_stats->tx_packets += enqueued;
	fds = fq->xdp_tx_fds.fds;
	for (i = 0; i < enqueued; i++) {
		percpu_stats->tx_bytes += dpaa2_fd_get_len(&fds[i]);
		ch->stats.xdp_tx++;
	}
	for (i = enqueued; i < fq->xdp_tx_fds.num; i++) {
		dpaa2_eth_recycle_buf(priv, ch, dpaa2_fd_get_addr(&fds[i]));
		percpu_stats->tx_errors++;
		ch->stats.xdp_tx_err++;
	}
	fq->xdp_tx_fds.num = 0;
}

void dpaa2_eth_xdp_enqueue(struct dpaa2_eth_priv *priv,
			   struct dpaa2_eth_channel *ch,
			   struct dpaa2_fd *fd,
			   void *buf_start, u16 queue_id)
{
	struct dpaa2_faead *faead;
	struct dpaa2_fd *dest_fd;
	struct dpaa2_eth_fq *fq;
	u32 ctrl, frc;

	 
	frc = dpaa2_fd_get_frc(fd);
	dpaa2_fd_set_frc(fd, frc | DPAA2_FD_FRC_FAEADV);
	dpaa2_fd_set_ctrl(fd, DPAA2_FD_CTRL_ASAL);

	 
	ctrl = DPAA2_FAEAD_A4V | DPAA2_FAEAD_A2V | DPAA2_FAEAD_EBDDV;
	faead = dpaa2_get_faead(buf_start, false);
	faead->ctrl = cpu_to_le32(ctrl);
	faead->conf_fqid = 0;

	fq = &priv->fq[queue_id];
	dest_fd = &fq->xdp_tx_fds.fds[fq->xdp_tx_fds.num++];
	memcpy(dest_fd, fd, sizeof(*dest_fd));

	if (fq->xdp_tx_fds.num < DEV_MAP_BULK_SIZE)
		return;

	dpaa2_eth_xdp_tx_flush(priv, ch, fq);
}

static u32 dpaa2_eth_run_xdp(struct dpaa2_eth_priv *priv,
			     struct dpaa2_eth_channel *ch,
			     struct dpaa2_eth_fq *rx_fq,
			     struct dpaa2_fd *fd, void *vaddr)
{
	dma_addr_t addr = dpaa2_fd_get_addr(fd);
	struct bpf_prog *xdp_prog;
	struct xdp_buff xdp;
	u32 xdp_act = XDP_PASS;
	int err, offset;

	xdp_prog = READ_ONCE(ch->xdp.prog);
	if (!xdp_prog)
		goto out;

	offset = dpaa2_fd_get_offset(fd) - XDP_PACKET_HEADROOM;
	xdp_init_buff(&xdp, DPAA2_ETH_RX_BUF_RAW_SIZE - offset, &ch->xdp_rxq);
	xdp_prepare_buff(&xdp, vaddr + offset, XDP_PACKET_HEADROOM,
			 dpaa2_fd_get_len(fd), false);

	xdp_act = bpf_prog_run_xdp(xdp_prog, &xdp);

	 
	dpaa2_fd_set_offset(fd, xdp.data - vaddr);
	dpaa2_fd_set_len(fd, xdp.data_end - xdp.data);

	switch (xdp_act) {
	case XDP_PASS:
		break;
	case XDP_TX:
		dpaa2_eth_xdp_enqueue(priv, ch, fd, vaddr, rx_fq->flowid);
		break;
	default:
		bpf_warn_invalid_xdp_action(priv->net_dev, xdp_prog, xdp_act);
		fallthrough;
	case XDP_ABORTED:
		trace_xdp_exception(priv->net_dev, xdp_prog, xdp_act);
		fallthrough;
	case XDP_DROP:
		dpaa2_eth_recycle_buf(priv, ch, addr);
		ch->stats.xdp_drop++;
		break;
	case XDP_REDIRECT:
		dma_unmap_page(priv->net_dev->dev.parent, addr,
			       priv->rx_buf_size, DMA_BIDIRECTIONAL);
		ch->buf_count--;

		 
		xdp.data_hard_start = vaddr;
		xdp.frame_sz = DPAA2_ETH_RX_BUF_RAW_SIZE;

		err = xdp_do_redirect(priv->net_dev, &xdp, xdp_prog);
		if (unlikely(err)) {
			addr = dma_map_page(priv->net_dev->dev.parent,
					    virt_to_page(vaddr), 0,
					    priv->rx_buf_size, DMA_BIDIRECTIONAL);
			if (unlikely(dma_mapping_error(priv->net_dev->dev.parent, addr))) {
				free_pages((unsigned long)vaddr, 0);
			} else {
				ch->buf_count++;
				dpaa2_eth_recycle_buf(priv, ch, addr);
			}
			ch->stats.xdp_drop++;
		} else {
			ch->stats.xdp_redirect++;
		}
		break;
	}

	ch->xdp.res |= xdp_act;
out:
	return xdp_act;
}

struct sk_buff *dpaa2_eth_alloc_skb(struct dpaa2_eth_priv *priv,
				    struct dpaa2_eth_channel *ch,
				    const struct dpaa2_fd *fd, u32 fd_length,
				    void *fd_vaddr)
{
	u16 fd_offset = dpaa2_fd_get_offset(fd);
	struct sk_buff *skb = NULL;
	unsigned int skb_len;

	skb_len = fd_length + dpaa2_eth_needed_headroom(NULL);

	skb = napi_alloc_skb(&ch->napi, skb_len);
	if (!skb)
		return NULL;

	skb_reserve(skb, dpaa2_eth_needed_headroom(NULL));
	skb_put(skb, fd_length);

	memcpy(skb->data, fd_vaddr + fd_offset, fd_length);

	return skb;
}

static struct sk_buff *dpaa2_eth_copybreak(struct dpaa2_eth_channel *ch,
					   const struct dpaa2_fd *fd,
					   void *fd_vaddr)
{
	struct dpaa2_eth_priv *priv = ch->priv;
	u32 fd_length = dpaa2_fd_get_len(fd);

	if (fd_length > priv->rx_copybreak)
		return NULL;

	return dpaa2_eth_alloc_skb(priv, ch, fd, fd_length, fd_vaddr);
}

void dpaa2_eth_receive_skb(struct dpaa2_eth_priv *priv,
			   struct dpaa2_eth_channel *ch,
			   const struct dpaa2_fd *fd, void *vaddr,
			   struct dpaa2_eth_fq *fq,
			   struct rtnl_link_stats64 *percpu_stats,
			   struct sk_buff *skb)
{
	struct dpaa2_fas *fas;
	u32 status = 0;

	fas = dpaa2_get_fas(vaddr, false);
	prefetch(fas);
	prefetch(skb->data);

	 
	if (priv->rx_tstamp) {
		struct skb_shared_hwtstamps *shhwtstamps = skb_hwtstamps(skb);
		__le64 *ts = dpaa2_get_ts(vaddr, false);
		u64 ns;

		memset(shhwtstamps, 0, sizeof(*shhwtstamps));

		ns = DPAA2_PTP_CLK_PERIOD_NS * le64_to_cpup(ts);
		shhwtstamps->hwtstamp = ns_to_ktime(ns);
	}

	 
	if (likely(dpaa2_fd_get_frc(fd) & DPAA2_FD_FRC_FASV)) {
		status = le32_to_cpu(fas->status);
		dpaa2_eth_validate_rx_csum(priv, status, skb);
	}

	skb->protocol = eth_type_trans(skb, priv->net_dev);
	skb_record_rx_queue(skb, fq->flowid);

	percpu_stats->rx_packets++;
	percpu_stats->rx_bytes += dpaa2_fd_get_len(fd);
	ch->stats.bytes_per_cdan += dpaa2_fd_get_len(fd);

	list_add_tail(&skb->list, ch->rx_list);
}

 
void dpaa2_eth_rx(struct dpaa2_eth_priv *priv,
		  struct dpaa2_eth_channel *ch,
		  const struct dpaa2_fd *fd,
		  struct dpaa2_eth_fq *fq)
{
	dma_addr_t addr = dpaa2_fd_get_addr(fd);
	u8 fd_format = dpaa2_fd_get_format(fd);
	void *vaddr;
	struct sk_buff *skb;
	struct rtnl_link_stats64 *percpu_stats;
	struct dpaa2_eth_drv_stats *percpu_extras;
	struct device *dev = priv->net_dev->dev.parent;
	bool recycle_rx_buf = false;
	void *buf_data;
	u32 xdp_act;

	 
	trace_dpaa2_rx_fd(priv->net_dev, fd);

	vaddr = dpaa2_iova_to_virt(priv->iommu_domain, addr);
	dma_sync_single_for_cpu(dev, addr, priv->rx_buf_size,
				DMA_BIDIRECTIONAL);

	buf_data = vaddr + dpaa2_fd_get_offset(fd);
	prefetch(buf_data);

	percpu_stats = this_cpu_ptr(priv->percpu_stats);
	percpu_extras = this_cpu_ptr(priv->percpu_extras);

	if (fd_format == dpaa2_fd_single) {
		xdp_act = dpaa2_eth_run_xdp(priv, ch, fq, (struct dpaa2_fd *)fd, vaddr);
		if (xdp_act != XDP_PASS) {
			percpu_stats->rx_packets++;
			percpu_stats->rx_bytes += dpaa2_fd_get_len(fd);
			return;
		}

		skb = dpaa2_eth_copybreak(ch, fd, vaddr);
		if (!skb) {
			dma_unmap_page(dev, addr, priv->rx_buf_size,
				       DMA_BIDIRECTIONAL);
			skb = dpaa2_eth_build_linear_skb(ch, fd, vaddr);
		} else {
			recycle_rx_buf = true;
		}
	} else if (fd_format == dpaa2_fd_sg) {
		WARN_ON(priv->xdp_prog);

		dma_unmap_page(dev, addr, priv->rx_buf_size,
			       DMA_BIDIRECTIONAL);
		skb = dpaa2_eth_build_frag_skb(priv, ch, buf_data);
		free_pages((unsigned long)vaddr, 0);
		percpu_extras->rx_sg_frames++;
		percpu_extras->rx_sg_bytes += dpaa2_fd_get_len(fd);
	} else {
		 
		goto err_frame_format;
	}

	if (unlikely(!skb))
		goto err_build_skb;

	dpaa2_eth_receive_skb(priv, ch, fd, vaddr, fq, percpu_stats, skb);

	if (recycle_rx_buf)
		dpaa2_eth_recycle_buf(priv, ch, dpaa2_fd_get_addr(fd));
	return;

err_build_skb:
	dpaa2_eth_free_rx_fd(priv, fd, vaddr);
err_frame_format:
	percpu_stats->rx_dropped++;
}

 
static void dpaa2_eth_rx_err(struct dpaa2_eth_priv *priv,
			     struct dpaa2_eth_channel *ch,
			     const struct dpaa2_fd *fd,
			     struct dpaa2_eth_fq *fq __always_unused)
{
	struct device *dev = priv->net_dev->dev.parent;
	dma_addr_t addr = dpaa2_fd_get_addr(fd);
	u8 fd_format = dpaa2_fd_get_format(fd);
	struct rtnl_link_stats64 *percpu_stats;
	struct dpaa2_eth_trap_item *trap_item;
	struct dpaa2_fapr *fapr;
	struct sk_buff *skb;
	void *buf_data;
	void *vaddr;

	vaddr = dpaa2_iova_to_virt(priv->iommu_domain, addr);
	dma_sync_single_for_cpu(dev, addr, priv->rx_buf_size,
				DMA_BIDIRECTIONAL);

	buf_data = vaddr + dpaa2_fd_get_offset(fd);

	if (fd_format == dpaa2_fd_single) {
		dma_unmap_page(dev, addr, priv->rx_buf_size,
			       DMA_BIDIRECTIONAL);
		skb = dpaa2_eth_build_linear_skb(ch, fd, vaddr);
	} else if (fd_format == dpaa2_fd_sg) {
		dma_unmap_page(dev, addr, priv->rx_buf_size,
			       DMA_BIDIRECTIONAL);
		skb = dpaa2_eth_build_frag_skb(priv, ch, buf_data);
		free_pages((unsigned long)vaddr, 0);
	} else {
		 
		dpaa2_eth_free_rx_fd(priv, fd, vaddr);
		goto err_frame_format;
	}

	fapr = dpaa2_get_fapr(vaddr, false);
	trap_item = dpaa2_eth_dl_get_trap(priv, fapr);
	if (trap_item)
		devlink_trap_report(priv->devlink, skb, trap_item->trap_ctx,
				    &priv->devlink_port, NULL);
	consume_skb(skb);

err_frame_format:
	percpu_stats = this_cpu_ptr(priv->percpu_stats);
	percpu_stats->rx_errors++;
	ch->buf_count--;
}

 
static int dpaa2_eth_consume_frames(struct dpaa2_eth_channel *ch,
				    struct dpaa2_eth_fq **src)
{
	struct dpaa2_eth_priv *priv = ch->priv;
	struct dpaa2_eth_fq *fq = NULL;
	struct dpaa2_dq *dq;
	const struct dpaa2_fd *fd;
	int cleaned = 0, retries = 0;
	int is_last;

	do {
		dq = dpaa2_io_store_next(ch->store, &is_last);
		if (unlikely(!dq)) {
			 
			if (retries++ >= DPAA2_ETH_SWP_BUSY_RETRIES) {
				netdev_err_once(priv->net_dev,
						"Unable to read a valid dequeue response\n");
				return -ETIMEDOUT;
			}
			continue;
		}

		fd = dpaa2_dq_fd(dq);
		fq = (struct dpaa2_eth_fq *)(uintptr_t)dpaa2_dq_fqd_ctx(dq);

		fq->consume(priv, ch, fd, fq);
		cleaned++;
		retries = 0;
	} while (!is_last);

	if (!cleaned)
		return 0;

	fq->stats.frames += cleaned;
	ch->stats.frames += cleaned;
	ch->stats.frames_per_cdan += cleaned;

	 
	if (src)
		*src = fq;

	return cleaned;
}

static int dpaa2_eth_ptp_parse(struct sk_buff *skb,
			       u8 *msgtype, u8 *twostep, u8 *udp,
			       u16 *correction_offset,
			       u16 *origintimestamp_offset)
{
	unsigned int ptp_class;
	struct ptp_header *hdr;
	unsigned int type;
	u8 *base;

	ptp_class = ptp_classify_raw(skb);
	if (ptp_class == PTP_CLASS_NONE)
		return -EINVAL;

	hdr = ptp_parse_header(skb, ptp_class);
	if (!hdr)
		return -EINVAL;

	*msgtype = ptp_get_msgtype(hdr, ptp_class);
	*twostep = hdr->flag_field[0] & 0x2;

	type = ptp_class & PTP_CLASS_PMASK;
	if (type == PTP_CLASS_IPV4 ||
	    type == PTP_CLASS_IPV6)
		*udp = 1;
	else
		*udp = 0;

	base = skb_mac_header(skb);
	*correction_offset = (u8 *)&hdr->correction - base;
	*origintimestamp_offset = (u8 *)hdr + sizeof(struct ptp_header) - base;

	return 0;
}

 
static void dpaa2_eth_enable_tx_tstamp(struct dpaa2_eth_priv *priv,
				       struct dpaa2_fd *fd,
				       void *buf_start,
				       struct sk_buff *skb)
{
	struct ptp_tstamp origin_timestamp;
	u8 msgtype, twostep, udp;
	struct dpaa2_faead *faead;
	struct dpaa2_fas *fas;
	struct timespec64 ts;
	u16 offset1, offset2;
	u32 ctrl, frc;
	__le64 *ns;
	u8 *data;

	 
	frc = dpaa2_fd_get_frc(fd);
	dpaa2_fd_set_frc(fd, frc | DPAA2_FD_FRC_FAEADV);

	 
	ctrl = dpaa2_fd_get_ctrl(fd);
	dpaa2_fd_set_ctrl(fd, ctrl | DPAA2_FD_CTRL_ASAL);

	 
	ctrl = DPAA2_FAEAD_A2V | DPAA2_FAEAD_UPDV | DPAA2_FAEAD_UPD;
	faead = dpaa2_get_faead(buf_start, true);
	faead->ctrl = cpu_to_le32(ctrl);

	if (skb->cb[0] == TX_TSTAMP_ONESTEP_SYNC) {
		if (dpaa2_eth_ptp_parse(skb, &msgtype, &twostep, &udp,
					&offset1, &offset2) ||
		    msgtype != PTP_MSGTYPE_SYNC || twostep) {
			WARN_ONCE(1, "Bad packet for one-step timestamping\n");
			return;
		}

		 
		frc = dpaa2_fd_get_frc(fd);
		dpaa2_fd_set_frc(fd, frc | DPAA2_FD_FRC_FASV);

		 
		fas = dpaa2_get_fas(buf_start, true);
		fas->status = cpu_to_le32(DPAA2_FAS_PTP);

		dpaa2_ptp->caps.gettime64(&dpaa2_ptp->caps, &ts);
		ns = dpaa2_get_ts(buf_start, true);
		*ns = cpu_to_le64(timespec64_to_ns(&ts) /
				  DPAA2_PTP_CLK_PERIOD_NS);

		 
		ns_to_ptp_tstamp(&origin_timestamp, le64_to_cpup(ns));
		data = skb_mac_header(skb);
		*(__be16 *)(data + offset2) = htons(origin_timestamp.sec_msb);
		*(__be32 *)(data + offset2 + 2) =
			htonl(origin_timestamp.sec_lsb);
		*(__be32 *)(data + offset2 + 6) = htonl(origin_timestamp.nsec);

		if (priv->ptp_correction_off == offset1)
			return;

		priv->dpaa2_set_onestep_params_cb(priv, offset1, udp);
		priv->ptp_correction_off = offset1;

	}
}

void *dpaa2_eth_sgt_get(struct dpaa2_eth_priv *priv)
{
	struct dpaa2_eth_sgt_cache *sgt_cache;
	void *sgt_buf = NULL;
	int sgt_buf_size;

	sgt_cache = this_cpu_ptr(priv->sgt_cache);
	sgt_buf_size = priv->tx_data_offset +
		DPAA2_ETH_SG_ENTRIES_MAX * sizeof(struct dpaa2_sg_entry);

	if (sgt_cache->count == 0)
		sgt_buf = napi_alloc_frag_align(sgt_buf_size, DPAA2_ETH_TX_BUF_ALIGN);
	else
		sgt_buf = sgt_cache->buf[--sgt_cache->count];
	if (!sgt_buf)
		return NULL;

	memset(sgt_buf, 0, sgt_buf_size);

	return sgt_buf;
}

void dpaa2_eth_sgt_recycle(struct dpaa2_eth_priv *priv, void *sgt_buf)
{
	struct dpaa2_eth_sgt_cache *sgt_cache;

	sgt_cache = this_cpu_ptr(priv->sgt_cache);
	if (sgt_cache->count >= DPAA2_ETH_SGT_CACHE_SIZE)
		skb_free_frag(sgt_buf);
	else
		sgt_cache->buf[sgt_cache->count++] = sgt_buf;
}

 
static int dpaa2_eth_build_sg_fd(struct dpaa2_eth_priv *priv,
				 struct sk_buff *skb,
				 struct dpaa2_fd *fd,
				 void **swa_addr)
{
	struct device *dev = priv->net_dev->dev.parent;
	void *sgt_buf = NULL;
	dma_addr_t addr;
	int nr_frags = skb_shinfo(skb)->nr_frags;
	struct dpaa2_sg_entry *sgt;
	int i, err;
	int sgt_buf_size;
	struct scatterlist *scl, *crt_scl;
	int num_sg;
	int num_dma_bufs;
	struct dpaa2_eth_swa *swa;

	 
	if (unlikely(PAGE_SIZE / sizeof(struct scatterlist) < nr_frags + 1))
		return -EINVAL;

	scl = kmalloc_array(nr_frags + 1, sizeof(struct scatterlist), GFP_ATOMIC);
	if (unlikely(!scl))
		return -ENOMEM;

	sg_init_table(scl, nr_frags + 1);
	num_sg = skb_to_sgvec(skb, scl, 0, skb->len);
	if (unlikely(num_sg < 0)) {
		err = -ENOMEM;
		goto dma_map_sg_failed;
	}
	num_dma_bufs = dma_map_sg(dev, scl, num_sg, DMA_BIDIRECTIONAL);
	if (unlikely(!num_dma_bufs)) {
		err = -ENOMEM;
		goto dma_map_sg_failed;
	}

	 
	sgt_buf_size = priv->tx_data_offset +
		       sizeof(struct dpaa2_sg_entry) *  num_dma_bufs;
	sgt_buf = dpaa2_eth_sgt_get(priv);
	if (unlikely(!sgt_buf)) {
		err = -ENOMEM;
		goto sgt_buf_alloc_failed;
	}

	sgt = (struct dpaa2_sg_entry *)(sgt_buf + priv->tx_data_offset);

	 
	for_each_sg(scl, crt_scl, num_dma_bufs, i) {
		dpaa2_sg_set_addr(&sgt[i], sg_dma_address(crt_scl));
		dpaa2_sg_set_len(&sgt[i], sg_dma_len(crt_scl));
	}
	dpaa2_sg_set_final(&sgt[i - 1], true);

	 
	*swa_addr = (void *)sgt_buf;
	swa = (struct dpaa2_eth_swa *)sgt_buf;
	swa->type = DPAA2_ETH_SWA_SG;
	swa->sg.skb = skb;
	swa->sg.scl = scl;
	swa->sg.num_sg = num_sg;
	swa->sg.sgt_size = sgt_buf_size;

	 
	addr = dma_map_single(dev, sgt_buf, sgt_buf_size, DMA_BIDIRECTIONAL);
	if (unlikely(dma_mapping_error(dev, addr))) {
		err = -ENOMEM;
		goto dma_map_single_failed;
	}
	memset(fd, 0, sizeof(struct dpaa2_fd));
	dpaa2_fd_set_offset(fd, priv->tx_data_offset);
	dpaa2_fd_set_format(fd, dpaa2_fd_sg);
	dpaa2_fd_set_addr(fd, addr);
	dpaa2_fd_set_len(fd, skb->len);
	dpaa2_fd_set_ctrl(fd, FD_CTRL_PTA);

	return 0;

dma_map_single_failed:
	dpaa2_eth_sgt_recycle(priv, sgt_buf);
sgt_buf_alloc_failed:
	dma_unmap_sg(dev, scl, num_sg, DMA_BIDIRECTIONAL);
dma_map_sg_failed:
	kfree(scl);
	return err;
}

 
static int dpaa2_eth_build_sg_fd_single_buf(struct dpaa2_eth_priv *priv,
					    struct sk_buff *skb,
					    struct dpaa2_fd *fd,
					    void **swa_addr)
{
	struct device *dev = priv->net_dev->dev.parent;
	struct dpaa2_sg_entry *sgt;
	struct dpaa2_eth_swa *swa;
	dma_addr_t addr, sgt_addr;
	void *sgt_buf = NULL;
	int sgt_buf_size;
	int err;

	 
	sgt_buf_size = priv->tx_data_offset + sizeof(struct dpaa2_sg_entry);
	sgt_buf = dpaa2_eth_sgt_get(priv);
	if (unlikely(!sgt_buf))
		return -ENOMEM;
	sgt = (struct dpaa2_sg_entry *)(sgt_buf + priv->tx_data_offset);

	addr = dma_map_single(dev, skb->data, skb->len, DMA_BIDIRECTIONAL);
	if (unlikely(dma_mapping_error(dev, addr))) {
		err = -ENOMEM;
		goto data_map_failed;
	}

	 
	dpaa2_sg_set_addr(sgt, addr);
	dpaa2_sg_set_len(sgt, skb->len);
	dpaa2_sg_set_final(sgt, true);

	 
	*swa_addr = (void *)sgt_buf;
	swa = (struct dpaa2_eth_swa *)sgt_buf;
	swa->type = DPAA2_ETH_SWA_SINGLE;
	swa->single.skb = skb;
	swa->single.sgt_size = sgt_buf_size;

	 
	sgt_addr = dma_map_single(dev, sgt_buf, sgt_buf_size, DMA_BIDIRECTIONAL);
	if (unlikely(dma_mapping_error(dev, sgt_addr))) {
		err = -ENOMEM;
		goto sgt_map_failed;
	}

	memset(fd, 0, sizeof(struct dpaa2_fd));
	dpaa2_fd_set_offset(fd, priv->tx_data_offset);
	dpaa2_fd_set_format(fd, dpaa2_fd_sg);
	dpaa2_fd_set_addr(fd, sgt_addr);
	dpaa2_fd_set_len(fd, skb->len);
	dpaa2_fd_set_ctrl(fd, FD_CTRL_PTA);

	return 0;

sgt_map_failed:
	dma_unmap_single(dev, addr, skb->len, DMA_BIDIRECTIONAL);
data_map_failed:
	dpaa2_eth_sgt_recycle(priv, sgt_buf);

	return err;
}

 
static int dpaa2_eth_build_single_fd(struct dpaa2_eth_priv *priv,
				     struct sk_buff *skb,
				     struct dpaa2_fd *fd,
				     void **swa_addr)
{
	struct device *dev = priv->net_dev->dev.parent;
	u8 *buffer_start, *aligned_start;
	struct dpaa2_eth_swa *swa;
	dma_addr_t addr;

	buffer_start = skb->data - dpaa2_eth_needed_headroom(skb);
	aligned_start = PTR_ALIGN(buffer_start - DPAA2_ETH_TX_BUF_ALIGN,
				  DPAA2_ETH_TX_BUF_ALIGN);
	if (aligned_start >= skb->head)
		buffer_start = aligned_start;
	else
		return -ENOMEM;

	 
	*swa_addr = (void *)buffer_start;
	swa = (struct dpaa2_eth_swa *)buffer_start;
	swa->type = DPAA2_ETH_SWA_SINGLE;
	swa->single.skb = skb;

	addr = dma_map_single(dev, buffer_start,
			      skb_tail_pointer(skb) - buffer_start,
			      DMA_BIDIRECTIONAL);
	if (unlikely(dma_mapping_error(dev, addr)))
		return -ENOMEM;

	memset(fd, 0, sizeof(struct dpaa2_fd));
	dpaa2_fd_set_addr(fd, addr);
	dpaa2_fd_set_offset(fd, (u16)(skb->data - buffer_start));
	dpaa2_fd_set_len(fd, skb->len);
	dpaa2_fd_set_format(fd, dpaa2_fd_single);
	dpaa2_fd_set_ctrl(fd, FD_CTRL_PTA);

	return 0;
}

 
void dpaa2_eth_free_tx_fd(struct dpaa2_eth_priv *priv,
			  struct dpaa2_eth_channel *ch,
			  struct dpaa2_eth_fq *fq,
			  const struct dpaa2_fd *fd, bool in_napi)
{
	struct device *dev = priv->net_dev->dev.parent;
	dma_addr_t fd_addr, sg_addr;
	struct sk_buff *skb = NULL;
	unsigned char *buffer_start;
	struct dpaa2_eth_swa *swa;
	u8 fd_format = dpaa2_fd_get_format(fd);
	u32 fd_len = dpaa2_fd_get_len(fd);
	struct dpaa2_sg_entry *sgt;
	int should_free_skb = 1;
	void *tso_hdr;
	int i;

	fd_addr = dpaa2_fd_get_addr(fd);
	buffer_start = dpaa2_iova_to_virt(priv->iommu_domain, fd_addr);
	swa = (struct dpaa2_eth_swa *)buffer_start;

	if (fd_format == dpaa2_fd_single) {
		if (swa->type == DPAA2_ETH_SWA_SINGLE) {
			skb = swa->single.skb;
			 
			dma_unmap_single(dev, fd_addr,
					 skb_tail_pointer(skb) - buffer_start,
					 DMA_BIDIRECTIONAL);
		} else {
			WARN_ONCE(swa->type != DPAA2_ETH_SWA_XDP, "Wrong SWA type");
			dma_unmap_single(dev, fd_addr, swa->xdp.dma_size,
					 DMA_BIDIRECTIONAL);
		}
	} else if (fd_format == dpaa2_fd_sg) {
		if (swa->type == DPAA2_ETH_SWA_SG) {
			skb = swa->sg.skb;

			 
			dma_unmap_sg(dev, swa->sg.scl, swa->sg.num_sg,
				     DMA_BIDIRECTIONAL);
			kfree(swa->sg.scl);

			 
			dma_unmap_single(dev, fd_addr, swa->sg.sgt_size,
					 DMA_BIDIRECTIONAL);
		} else if (swa->type == DPAA2_ETH_SWA_SW_TSO) {
			skb = swa->tso.skb;

			sgt = (struct dpaa2_sg_entry *)(buffer_start +
							priv->tx_data_offset);

			 
			dma_unmap_single(dev, fd_addr, swa->tso.sgt_size,
					 DMA_BIDIRECTIONAL);

			 
			tso_hdr = dpaa2_iova_to_virt(priv->iommu_domain, dpaa2_sg_get_addr(sgt));
			dma_unmap_single(dev, dpaa2_sg_get_addr(sgt), TSO_HEADER_SIZE,
					 DMA_TO_DEVICE);
			kfree(tso_hdr);

			 
			for (i = 1; i < swa->tso.num_sg; i++)
				dma_unmap_single(dev, dpaa2_sg_get_addr(&sgt[i]),
						 dpaa2_sg_get_len(&sgt[i]), DMA_TO_DEVICE);

			if (!swa->tso.is_last_fd)
				should_free_skb = 0;
		} else if (swa->type == DPAA2_ETH_SWA_XSK) {
			 
			dma_unmap_single(dev, fd_addr, swa->xsk.sgt_size,
					 DMA_BIDIRECTIONAL);
		} else {
			skb = swa->single.skb;

			 
			dma_unmap_single(dev, fd_addr, swa->single.sgt_size,
					 DMA_BIDIRECTIONAL);

			sgt = (struct dpaa2_sg_entry *)(buffer_start +
							priv->tx_data_offset);
			sg_addr = dpaa2_sg_get_addr(sgt);
			dma_unmap_single(dev, sg_addr, skb->len, DMA_BIDIRECTIONAL);
		}
	} else {
		netdev_dbg(priv->net_dev, "Invalid FD format\n");
		return;
	}

	if (swa->type == DPAA2_ETH_SWA_XSK) {
		ch->xsk_tx_pkts_sent++;
		dpaa2_eth_sgt_recycle(priv, buffer_start);
		return;
	}

	if (swa->type != DPAA2_ETH_SWA_XDP && in_napi) {
		fq->dq_frames++;
		fq->dq_bytes += fd_len;
	}

	if (swa->type == DPAA2_ETH_SWA_XDP) {
		xdp_return_frame(swa->xdp.xdpf);
		return;
	}

	 
	if (swa->type != DPAA2_ETH_SWA_SW_TSO) {
		if (skb->cb[0] == TX_TSTAMP) {
			struct skb_shared_hwtstamps shhwtstamps;
			__le64 *ts = dpaa2_get_ts(buffer_start, true);
			u64 ns;

			memset(&shhwtstamps, 0, sizeof(shhwtstamps));

			ns = DPAA2_PTP_CLK_PERIOD_NS * le64_to_cpup(ts);
			shhwtstamps.hwtstamp = ns_to_ktime(ns);
			skb_tstamp_tx(skb, &shhwtstamps);
		} else if (skb->cb[0] == TX_TSTAMP_ONESTEP_SYNC) {
			mutex_unlock(&priv->onestep_tstamp_lock);
		}
	}

	 
	if (fd_format != dpaa2_fd_single)
		dpaa2_eth_sgt_recycle(priv, buffer_start);

	 
	if (should_free_skb)
		napi_consume_skb(skb, in_napi);
}

static int dpaa2_eth_build_gso_fd(struct dpaa2_eth_priv *priv,
				  struct sk_buff *skb, struct dpaa2_fd *fd,
				  int *num_fds, u32 *total_fds_len)
{
	struct device *dev = priv->net_dev->dev.parent;
	int hdr_len, total_len, data_left, fd_len;
	int num_sge, err, i, sgt_buf_size;
	struct dpaa2_fd *fd_start = fd;
	struct dpaa2_sg_entry *sgt;
	struct dpaa2_eth_swa *swa;
	dma_addr_t sgt_addr, addr;
	dma_addr_t tso_hdr_dma;
	unsigned int index = 0;
	struct tso_t tso;
	char *tso_hdr;
	void *sgt_buf;

	 
	hdr_len = tso_start(skb, &tso);
	*total_fds_len = 0;

	total_len = skb->len - hdr_len;
	while (total_len > 0) {
		 
		sgt_buf = dpaa2_eth_sgt_get(priv);
		if (unlikely(!sgt_buf)) {
			netdev_err(priv->net_dev, "dpaa2_eth_sgt_get() failed\n");
			err = -ENOMEM;
			goto err_sgt_get;
		}
		sgt = (struct dpaa2_sg_entry *)(sgt_buf + priv->tx_data_offset);

		 
		data_left = min_t(int, skb_shinfo(skb)->gso_size, total_len);
		total_len -= data_left;
		fd_len = data_left + hdr_len;

		 
		tso_hdr = kmalloc(TSO_HEADER_SIZE, GFP_ATOMIC);
		if (!tso_hdr) {
			err =  -ENOMEM;
			goto err_alloc_tso_hdr;
		}

		tso_build_hdr(skb, tso_hdr, &tso, data_left, total_len == 0);
		tso_hdr_dma = dma_map_single(dev, tso_hdr, TSO_HEADER_SIZE, DMA_TO_DEVICE);
		if (dma_mapping_error(dev, tso_hdr_dma)) {
			netdev_err(priv->net_dev, "dma_map_single(tso_hdr) failed\n");
			err = -ENOMEM;
			goto err_map_tso_hdr;
		}

		 
		dpaa2_sg_set_addr(sgt, tso_hdr_dma);
		dpaa2_sg_set_len(sgt, hdr_len);
		dpaa2_sg_set_final(sgt, data_left <= 0);

		 
		num_sge = 1;
		while (data_left > 0) {
			int size;

			 
			sgt++;
			size = min_t(int, tso.size, data_left);

			addr = dma_map_single(dev, tso.data, size, DMA_TO_DEVICE);
			if (dma_mapping_error(dev, addr)) {
				netdev_err(priv->net_dev, "dma_map_single(tso.data) failed\n");
				err = -ENOMEM;
				goto err_map_data;
			}
			dpaa2_sg_set_addr(sgt, addr);
			dpaa2_sg_set_len(sgt, size);
			dpaa2_sg_set_final(sgt, size == data_left);

			num_sge++;

			 
			data_left -= size;
			tso_build_data(skb, &tso, size);
		}

		 
		sgt_buf_size = priv->tx_data_offset + num_sge * sizeof(struct dpaa2_sg_entry);
		swa = (struct dpaa2_eth_swa *)sgt_buf;
		swa->type = DPAA2_ETH_SWA_SW_TSO;
		swa->tso.skb = skb;
		swa->tso.num_sg = num_sge;
		swa->tso.sgt_size = sgt_buf_size;
		swa->tso.is_last_fd = total_len == 0 ? 1 : 0;

		 
		sgt_addr = dma_map_single(dev, sgt_buf, sgt_buf_size, DMA_BIDIRECTIONAL);
		if (unlikely(dma_mapping_error(dev, sgt_addr))) {
			netdev_err(priv->net_dev, "dma_map_single(sgt_buf) failed\n");
			err = -ENOMEM;
			goto err_map_sgt;
		}

		 
		memset(fd, 0, sizeof(struct dpaa2_fd));
		dpaa2_fd_set_offset(fd, priv->tx_data_offset);
		dpaa2_fd_set_format(fd, dpaa2_fd_sg);
		dpaa2_fd_set_addr(fd, sgt_addr);
		dpaa2_fd_set_len(fd, fd_len);
		dpaa2_fd_set_ctrl(fd, FD_CTRL_PTA);

		*total_fds_len += fd_len;
		 
		fd++;
		index++;
	}

	*num_fds = index;

	return 0;

err_map_sgt:
err_map_data:
	 
	sgt = (struct dpaa2_sg_entry *)(sgt_buf + priv->tx_data_offset);
	for (i = 1; i < num_sge; i++)
		dma_unmap_single(dev, dpaa2_sg_get_addr(&sgt[i]),
				 dpaa2_sg_get_len(&sgt[i]), DMA_TO_DEVICE);

	 
	dma_unmap_single(dev, tso_hdr_dma, TSO_HEADER_SIZE, DMA_TO_DEVICE);
err_map_tso_hdr:
	kfree(tso_hdr);
err_alloc_tso_hdr:
	dpaa2_eth_sgt_recycle(priv, sgt_buf);
err_sgt_get:
	 
	for (i = 0; i < index; i++)
		dpaa2_eth_free_tx_fd(priv, NULL, NULL, &fd_start[i], false);

	return err;
}

static netdev_tx_t __dpaa2_eth_tx(struct sk_buff *skb,
				  struct net_device *net_dev)
{
	struct dpaa2_eth_priv *priv = netdev_priv(net_dev);
	int total_enqueued = 0, retries = 0, enqueued;
	struct dpaa2_eth_drv_stats *percpu_extras;
	struct rtnl_link_stats64 *percpu_stats;
	unsigned int needed_headroom;
	int num_fds = 1, max_retries;
	struct dpaa2_eth_fq *fq;
	struct netdev_queue *nq;
	struct dpaa2_fd *fd;
	u16 queue_mapping;
	void *swa = NULL;
	u8 prio = 0;
	int err, i;
	u32 fd_len;

	percpu_stats = this_cpu_ptr(priv->percpu_stats);
	percpu_extras = this_cpu_ptr(priv->percpu_extras);
	fd = (this_cpu_ptr(priv->fd))->array;

	needed_headroom = dpaa2_eth_needed_headroom(skb);

	 
	skb = skb_unshare(skb, GFP_ATOMIC);
	if (unlikely(!skb)) {
		 
		percpu_stats->tx_dropped++;
		return NETDEV_TX_OK;
	}

	 

	if (skb_is_gso(skb)) {
		err = dpaa2_eth_build_gso_fd(priv, skb, fd, &num_fds, &fd_len);
		percpu_extras->tx_sg_frames += num_fds;
		percpu_extras->tx_sg_bytes += fd_len;
		percpu_extras->tx_tso_frames += num_fds;
		percpu_extras->tx_tso_bytes += fd_len;
	} else if (skb_is_nonlinear(skb)) {
		err = dpaa2_eth_build_sg_fd(priv, skb, fd, &swa);
		percpu_extras->tx_sg_frames++;
		percpu_extras->tx_sg_bytes += skb->len;
		fd_len = dpaa2_fd_get_len(fd);
	} else if (skb_headroom(skb) < needed_headroom) {
		err = dpaa2_eth_build_sg_fd_single_buf(priv, skb, fd, &swa);
		percpu_extras->tx_sg_frames++;
		percpu_extras->tx_sg_bytes += skb->len;
		percpu_extras->tx_converted_sg_frames++;
		percpu_extras->tx_converted_sg_bytes += skb->len;
		fd_len = dpaa2_fd_get_len(fd);
	} else {
		err = dpaa2_eth_build_single_fd(priv, skb, fd, &swa);
		fd_len = dpaa2_fd_get_len(fd);
	}

	if (unlikely(err)) {
		percpu_stats->tx_dropped++;
		goto err_build_fd;
	}

	if (swa && skb->cb[0])
		dpaa2_eth_enable_tx_tstamp(priv, fd, swa, skb);

	 
	for (i = 0; i < num_fds; i++)
		trace_dpaa2_tx_fd(net_dev, &fd[i]);

	 
	queue_mapping = skb_get_queue_mapping(skb);

	if (net_dev->num_tc) {
		prio = netdev_txq_to_tc(net_dev, queue_mapping);
		 
		prio = net_dev->num_tc - prio - 1;
		 
		queue_mapping %= dpaa2_eth_queue_count(priv);
	}
	fq = &priv->fq[queue_mapping];
	nq = netdev_get_tx_queue(net_dev, queue_mapping);
	netdev_tx_sent_queue(nq, fd_len);

	 
	max_retries = num_fds * DPAA2_ETH_ENQUEUE_RETRIES;
	while (total_enqueued < num_fds && retries < max_retries) {
		err = priv->enqueue(priv, fq, &fd[total_enqueued],
				    prio, num_fds - total_enqueued, &enqueued);
		if (err == -EBUSY) {
			retries++;
			continue;
		}

		total_enqueued += enqueued;
	}
	percpu_extras->tx_portal_busy += retries;

	if (unlikely(err < 0)) {
		percpu_stats->tx_errors++;
		 
		dpaa2_eth_free_tx_fd(priv, NULL, fq, fd, false);
		netdev_tx_completed_queue(nq, 1, fd_len);
	} else {
		percpu_stats->tx_packets += total_enqueued;
		percpu_stats->tx_bytes += fd_len;
	}

	return NETDEV_TX_OK;

err_build_fd:
	dev_kfree_skb(skb);

	return NETDEV_TX_OK;
}

static void dpaa2_eth_tx_onestep_tstamp(struct work_struct *work)
{
	struct dpaa2_eth_priv *priv = container_of(work, struct dpaa2_eth_priv,
						   tx_onestep_tstamp);
	struct sk_buff *skb;

	while (true) {
		skb = skb_dequeue(&priv->tx_skbs);
		if (!skb)
			return;

		 
		mutex_lock(&priv->onestep_tstamp_lock);
		__dpaa2_eth_tx(skb, priv->net_dev);
	}
}

static netdev_tx_t dpaa2_eth_tx(struct sk_buff *skb, struct net_device *net_dev)
{
	struct dpaa2_eth_priv *priv = netdev_priv(net_dev);
	u8 msgtype, twostep, udp;
	u16 offset1, offset2;

	 
	skb->cb[0] = 0;

	if ((skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) && dpaa2_ptp) {
		if (priv->tx_tstamp_type == HWTSTAMP_TX_ON)
			skb->cb[0] = TX_TSTAMP;
		else if (priv->tx_tstamp_type == HWTSTAMP_TX_ONESTEP_SYNC)
			skb->cb[0] = TX_TSTAMP_ONESTEP_SYNC;
	}

	 
	if (skb->cb[0] == TX_TSTAMP_ONESTEP_SYNC) {
		if (!dpaa2_eth_ptp_parse(skb, &msgtype, &twostep, &udp,
					 &offset1, &offset2))
			if (msgtype == PTP_MSGTYPE_SYNC && twostep == 0) {
				skb_queue_tail(&priv->tx_skbs, skb);
				queue_work(priv->dpaa2_ptp_wq,
					   &priv->tx_onestep_tstamp);
				return NETDEV_TX_OK;
			}
		 
		skb->cb[0] = TX_TSTAMP;
	}

	 
	return __dpaa2_eth_tx(skb, net_dev);
}

 
static void dpaa2_eth_tx_conf(struct dpaa2_eth_priv *priv,
			      struct dpaa2_eth_channel *ch,
			      const struct dpaa2_fd *fd,
			      struct dpaa2_eth_fq *fq)
{
	struct rtnl_link_stats64 *percpu_stats;
	struct dpaa2_eth_drv_stats *percpu_extras;
	u32 fd_len = dpaa2_fd_get_len(fd);
	u32 fd_errors;

	 
	trace_dpaa2_tx_conf_fd(priv->net_dev, fd);

	percpu_extras = this_cpu_ptr(priv->percpu_extras);
	percpu_extras->tx_conf_frames++;
	percpu_extras->tx_conf_bytes += fd_len;
	ch->stats.bytes_per_cdan += fd_len;

	 
	fd_errors = dpaa2_fd_get_ctrl(fd) & DPAA2_FD_TX_ERR_MASK;
	dpaa2_eth_free_tx_fd(priv, ch, fq, fd, true);

	if (likely(!fd_errors))
		return;

	if (net_ratelimit())
		netdev_dbg(priv->net_dev, "TX frame FD error: 0x%08x\n",
			   fd_errors);

	percpu_stats = this_cpu_ptr(priv->percpu_stats);
	 
	percpu_stats->tx_errors++;
}

static int dpaa2_eth_set_rx_vlan_filtering(struct dpaa2_eth_priv *priv,
					   bool enable)
{
	int err;

	err = dpni_enable_vlan_filter(priv->mc_io, 0, priv->mc_token, enable);

	if (err) {
		netdev_err(priv->net_dev,
			   "dpni_enable_vlan_filter failed\n");
		return err;
	}

	return 0;
}

static int dpaa2_eth_set_rx_csum(struct dpaa2_eth_priv *priv, bool enable)
{
	int err;

	err = dpni_set_offload(priv->mc_io, 0, priv->mc_token,
			       DPNI_OFF_RX_L3_CSUM, enable);
	if (err) {
		netdev_err(priv->net_dev,
			   "dpni_set_offload(RX_L3_CSUM) failed\n");
		return err;
	}

	err = dpni_set_offload(priv->mc_io, 0, priv->mc_token,
			       DPNI_OFF_RX_L4_CSUM, enable);
	if (err) {
		netdev_err(priv->net_dev,
			   "dpni_set_offload(RX_L4_CSUM) failed\n");
		return err;
	}

	return 0;
}

static int dpaa2_eth_set_tx_csum(struct dpaa2_eth_priv *priv, bool enable)
{
	int err;

	err = dpni_set_offload(priv->mc_io, 0, priv->mc_token,
			       DPNI_OFF_TX_L3_CSUM, enable);
	if (err) {
		netdev_err(priv->net_dev, "dpni_set_offload(TX_L3_CSUM) failed\n");
		return err;
	}

	err = dpni_set_offload(priv->mc_io, 0, priv->mc_token,
			       DPNI_OFF_TX_L4_CSUM, enable);
	if (err) {
		netdev_err(priv->net_dev, "dpni_set_offload(TX_L4_CSUM) failed\n");
		return err;
	}

	return 0;
}

 
static int dpaa2_eth_add_bufs(struct dpaa2_eth_priv *priv,
			      struct dpaa2_eth_channel *ch)
{
	struct xdp_buff *xdp_buffs[DPAA2_ETH_BUFS_PER_CMD];
	struct device *dev = priv->net_dev->dev.parent;
	u64 buf_array[DPAA2_ETH_BUFS_PER_CMD];
	struct dpaa2_eth_swa *swa;
	struct page *page;
	dma_addr_t addr;
	int retries = 0;
	int i = 0, err;
	u32 batch;

	 
	if (!ch->xsk_zc) {
		for (i = 0; i < DPAA2_ETH_BUFS_PER_CMD; i++) {
			 
			page = dev_alloc_pages(0);
			if (!page)
				goto err_alloc;

			addr = dma_map_page(dev, page, 0, priv->rx_buf_size,
					    DMA_BIDIRECTIONAL);
			if (unlikely(dma_mapping_error(dev, addr)))
				goto err_map;

			buf_array[i] = addr;

			 
			trace_dpaa2_eth_buf_seed(priv->net_dev,
						 page_address(page),
						 DPAA2_ETH_RX_BUF_RAW_SIZE,
						 addr, priv->rx_buf_size,
						 ch->bp->bpid);
		}
	} else if (xsk_buff_can_alloc(ch->xsk_pool, DPAA2_ETH_BUFS_PER_CMD)) {
		 
		batch = xsk_buff_alloc_batch(ch->xsk_pool, xdp_buffs,
					     DPAA2_ETH_BUFS_PER_CMD);
		if (!batch)
			goto err_alloc;

		for (i = 0; i < batch; i++) {
			swa = (struct dpaa2_eth_swa *)(xdp_buffs[i]->data_hard_start +
						       DPAA2_ETH_RX_HWA_SIZE);
			swa->xsk.xdp_buff = xdp_buffs[i];

			addr = xsk_buff_xdp_get_frame_dma(xdp_buffs[i]);
			if (unlikely(dma_mapping_error(dev, addr)))
				goto err_map;

			buf_array[i] = addr;

			trace_dpaa2_xsk_buf_seed(priv->net_dev,
						 xdp_buffs[i]->data_hard_start,
						 DPAA2_ETH_RX_BUF_RAW_SIZE,
						 addr, priv->rx_buf_size,
						 ch->bp->bpid);
		}
	}

release_bufs:
	 
	while ((err = dpaa2_io_service_release(ch->dpio, ch->bp->bpid,
					       buf_array, i)) == -EBUSY) {
		if (retries++ >= DPAA2_ETH_SWP_BUSY_RETRIES)
			break;
		cpu_relax();
	}

	 
	if (err) {
		dpaa2_eth_free_bufs(priv, buf_array, i, ch->xsk_zc);
		return 0;
	}

	return i;

err_map:
	if (!ch->xsk_zc) {
		__free_pages(page, 0);
	} else {
		for (; i < batch; i++)
			xsk_buff_free(xdp_buffs[i]);
	}
err_alloc:
	 
	if (i)
		goto release_bufs;

	return 0;
}

static int dpaa2_eth_seed_pool(struct dpaa2_eth_priv *priv,
			       struct dpaa2_eth_channel *ch)
{
	int i;
	int new_count;

	for (i = 0; i < DPAA2_ETH_NUM_BUFS; i += DPAA2_ETH_BUFS_PER_CMD) {
		new_count = dpaa2_eth_add_bufs(priv, ch);
		ch->buf_count += new_count;

		if (new_count < DPAA2_ETH_BUFS_PER_CMD)
			return -ENOMEM;
	}

	return 0;
}

static void dpaa2_eth_seed_pools(struct dpaa2_eth_priv *priv)
{
	struct net_device *net_dev = priv->net_dev;
	struct dpaa2_eth_channel *channel;
	int i, err = 0;

	for (i = 0; i < priv->num_channels; i++) {
		channel = priv->channel[i];

		err = dpaa2_eth_seed_pool(priv, channel);

		 
		if (err)
			netdev_err(net_dev, "Buffer seeding failed for DPBP %d (bpid=%d)\n",
				   channel->bp->dev->obj_desc.id,
				   channel->bp->bpid);
	}
}

 
static void dpaa2_eth_drain_bufs(struct dpaa2_eth_priv *priv, int bpid,
				 int count)
{
	u64 buf_array[DPAA2_ETH_BUFS_PER_CMD];
	bool xsk_zc = false;
	int retries = 0;
	int i, ret;

	for (i = 0; i < priv->num_channels; i++)
		if (priv->channel[i]->bp->bpid == bpid)
			xsk_zc = priv->channel[i]->xsk_zc;

	do {
		ret = dpaa2_io_service_acquire(NULL, bpid, buf_array, count);
		if (ret < 0) {
			if (ret == -EBUSY &&
			    retries++ < DPAA2_ETH_SWP_BUSY_RETRIES)
				continue;
			netdev_err(priv->net_dev, "dpaa2_io_service_acquire() failed\n");
			return;
		}
		dpaa2_eth_free_bufs(priv, buf_array, ret, xsk_zc);
		retries = 0;
	} while (ret);
}

static void dpaa2_eth_drain_pool(struct dpaa2_eth_priv *priv, int bpid)
{
	int i;

	 
	dpaa2_eth_drain_bufs(priv, bpid, DPAA2_ETH_BUFS_PER_CMD);
	dpaa2_eth_drain_bufs(priv, bpid, 1);

	 
	for (i = 0; i < priv->num_channels; i++)
		if (priv->channel[i]->bp->bpid == bpid)
			priv->channel[i]->buf_count = 0;
}

static void dpaa2_eth_drain_pools(struct dpaa2_eth_priv *priv)
{
	int i;

	for (i = 0; i < priv->num_bps; i++)
		dpaa2_eth_drain_pool(priv, priv->bp[i]->bpid);
}

 
static int dpaa2_eth_refill_pool(struct dpaa2_eth_priv *priv,
				 struct dpaa2_eth_channel *ch)
{
	int new_count;

	if (likely(ch->buf_count >= DPAA2_ETH_REFILL_THRESH))
		return 0;

	do {
		new_count = dpaa2_eth_add_bufs(priv, ch);
		if (unlikely(!new_count)) {
			 
			break;
		}
		ch->buf_count += new_count;
	} while (ch->buf_count < DPAA2_ETH_NUM_BUFS);

	if (unlikely(ch->buf_count < DPAA2_ETH_NUM_BUFS))
		return -ENOMEM;

	return 0;
}

static void dpaa2_eth_sgt_cache_drain(struct dpaa2_eth_priv *priv)
{
	struct dpaa2_eth_sgt_cache *sgt_cache;
	u16 count;
	int k, i;

	for_each_possible_cpu(k) {
		sgt_cache = per_cpu_ptr(priv->sgt_cache, k);
		count = sgt_cache->count;

		for (i = 0; i < count; i++)
			skb_free_frag(sgt_cache->buf[i]);
		sgt_cache->count = 0;
	}
}

static int dpaa2_eth_pull_channel(struct dpaa2_eth_channel *ch)
{
	int err;
	int dequeues = -1;

	 
	do {
		err = dpaa2_io_service_pull_channel(ch->dpio, ch->ch_id,
						    ch->store);
		dequeues++;
		cpu_relax();
	} while (err == -EBUSY && dequeues < DPAA2_ETH_SWP_BUSY_RETRIES);

	ch->stats.dequeue_portal_busy += dequeues;
	if (unlikely(err))
		ch->stats.pull_err++;

	return err;
}

 
static int dpaa2_eth_poll(struct napi_struct *napi, int budget)
{
	struct dpaa2_eth_channel *ch;
	struct dpaa2_eth_priv *priv;
	int rx_cleaned = 0, txconf_cleaned = 0;
	struct dpaa2_eth_fq *fq, *txc_fq = NULL;
	struct netdev_queue *nq;
	int store_cleaned, work_done;
	bool work_done_zc = false;
	struct list_head rx_list;
	int retries = 0;
	u16 flowid;
	int err;

	ch = container_of(napi, struct dpaa2_eth_channel, napi);
	ch->xdp.res = 0;
	priv = ch->priv;

	INIT_LIST_HEAD(&rx_list);
	ch->rx_list = &rx_list;

	if (ch->xsk_zc) {
		work_done_zc = dpaa2_xsk_tx(priv, ch);
		 
		if (work_done_zc) {
			work_done = budget;
			goto out;
		}
	}

	do {
		err = dpaa2_eth_pull_channel(ch);
		if (unlikely(err))
			break;

		 
		dpaa2_eth_refill_pool(priv, ch);

		store_cleaned = dpaa2_eth_consume_frames(ch, &fq);
		if (store_cleaned <= 0)
			break;
		if (fq->type == DPAA2_RX_FQ) {
			rx_cleaned += store_cleaned;
			flowid = fq->flowid;
		} else {
			txconf_cleaned += store_cleaned;
			 
			txc_fq = fq;
		}

		 
		if (rx_cleaned >= budget ||
		    txconf_cleaned >= DPAA2_ETH_TXCONF_PER_NAPI) {
			work_done = budget;
			if (ch->xdp.res & XDP_REDIRECT)
				xdp_do_flush();
			goto out;
		}
	} while (store_cleaned);

	if (ch->xdp.res & XDP_REDIRECT)
		xdp_do_flush();

	 
	dpaa2_io_update_net_dim(ch->dpio, ch->stats.frames_per_cdan,
				ch->stats.bytes_per_cdan);
	ch->stats.frames_per_cdan = 0;
	ch->stats.bytes_per_cdan = 0;

	 
	napi_complete_done(napi, rx_cleaned);
	do {
		err = dpaa2_io_service_rearm(ch->dpio, &ch->nctx);
		cpu_relax();
	} while (err == -EBUSY && retries++ < DPAA2_ETH_SWP_BUSY_RETRIES);
	WARN_ONCE(err, "CDAN notifications rearm failed on core %d",
		  ch->nctx.desired_cpu);

	work_done = max(rx_cleaned, 1);

out:
	netif_receive_skb_list(ch->rx_list);

	if (ch->xsk_tx_pkts_sent) {
		xsk_tx_completed(ch->xsk_pool, ch->xsk_tx_pkts_sent);
		ch->xsk_tx_pkts_sent = 0;
	}

	if (txc_fq && txc_fq->dq_frames) {
		nq = netdev_get_tx_queue(priv->net_dev, txc_fq->flowid);
		netdev_tx_completed_queue(nq, txc_fq->dq_frames,
					  txc_fq->dq_bytes);
		txc_fq->dq_frames = 0;
		txc_fq->dq_bytes = 0;
	}

	if (rx_cleaned && ch->xdp.res & XDP_TX)
		dpaa2_eth_xdp_tx_flush(priv, ch, &priv->fq[flowid]);

	return work_done;
}

static void dpaa2_eth_enable_ch_napi(struct dpaa2_eth_priv *priv)
{
	struct dpaa2_eth_channel *ch;
	int i;

	for (i = 0; i < priv->num_channels; i++) {
		ch = priv->channel[i];
		napi_enable(&ch->napi);
	}
}

static void dpaa2_eth_disable_ch_napi(struct dpaa2_eth_priv *priv)
{
	struct dpaa2_eth_channel *ch;
	int i;

	for (i = 0; i < priv->num_channels; i++) {
		ch = priv->channel[i];
		napi_disable(&ch->napi);
	}
}

void dpaa2_eth_set_rx_taildrop(struct dpaa2_eth_priv *priv,
			       bool tx_pause, bool pfc)
{
	struct dpni_taildrop td = {0};
	struct dpaa2_eth_fq *fq;
	int i, err;

	 
	td.enable = !tx_pause;
	if (priv->rx_fqtd_enabled == td.enable)
		goto set_cgtd;

	td.threshold = DPAA2_ETH_FQ_TAILDROP_THRESH;
	td.units = DPNI_CONGESTION_UNIT_BYTES;

	for (i = 0; i < priv->num_fqs; i++) {
		fq = &priv->fq[i];
		if (fq->type != DPAA2_RX_FQ)
			continue;
		err = dpni_set_taildrop(priv->mc_io, 0, priv->mc_token,
					DPNI_CP_QUEUE, DPNI_QUEUE_RX,
					fq->tc, fq->flowid, &td);
		if (err) {
			netdev_err(priv->net_dev,
				   "dpni_set_taildrop(FQ) failed\n");
			return;
		}
	}

	priv->rx_fqtd_enabled = td.enable;

set_cgtd:
	 
	td.enable = !tx_pause || pfc;
	if (priv->rx_cgtd_enabled == td.enable)
		return;

	td.threshold = DPAA2_ETH_CG_TAILDROP_THRESH(priv);
	td.units = DPNI_CONGESTION_UNIT_FRAMES;
	for (i = 0; i < dpaa2_eth_tc_count(priv); i++) {
		err = dpni_set_taildrop(priv->mc_io, 0, priv->mc_token,
					DPNI_CP_GROUP, DPNI_QUEUE_RX,
					i, 0, &td);
		if (err) {
			netdev_err(priv->net_dev,
				   "dpni_set_taildrop(CG) failed\n");
			return;
		}
	}

	priv->rx_cgtd_enabled = td.enable;
}

static int dpaa2_eth_link_state_update(struct dpaa2_eth_priv *priv)
{
	struct dpni_link_state state = {0};
	bool tx_pause;
	int err;

	err = dpni_get_link_state(priv->mc_io, 0, priv->mc_token, &state);
	if (unlikely(err)) {
		netdev_err(priv->net_dev,
			   "dpni_get_link_state() failed\n");
		return err;
	}

	 
	tx_pause = dpaa2_eth_tx_pause_enabled(state.options);
	dpaa2_eth_set_rx_taildrop(priv, tx_pause, priv->pfc_enabled);

	 
	if (dpaa2_mac_is_type_phy(priv->mac))
		goto out;

	 
	if (priv->link_state.up == state.up)
		goto out;

	if (state.up) {
		netif_carrier_on(priv->net_dev);
		netif_tx_start_all_queues(priv->net_dev);
	} else {
		netif_tx_stop_all_queues(priv->net_dev);
		netif_carrier_off(priv->net_dev);
	}

	netdev_info(priv->net_dev, "Link Event: state %s\n",
		    state.up ? "up" : "down");

out:
	priv->link_state = state;

	return 0;
}

static int dpaa2_eth_open(struct net_device *net_dev)
{
	struct dpaa2_eth_priv *priv = netdev_priv(net_dev);
	int err;

	dpaa2_eth_seed_pools(priv);

	mutex_lock(&priv->mac_lock);

	if (!dpaa2_eth_is_type_phy(priv)) {
		 
		netif_tx_stop_all_queues(net_dev);

		 
		netif_carrier_off(net_dev);
	}
	dpaa2_eth_enable_ch_napi(priv);

	err = dpni_enable(priv->mc_io, 0, priv->mc_token);
	if (err < 0) {
		mutex_unlock(&priv->mac_lock);
		netdev_err(net_dev, "dpni_enable() failed\n");
		goto enable_err;
	}

	if (dpaa2_eth_is_type_phy(priv))
		dpaa2_mac_start(priv->mac);

	mutex_unlock(&priv->mac_lock);

	return 0;

enable_err:
	dpaa2_eth_disable_ch_napi(priv);
	dpaa2_eth_drain_pools(priv);
	return err;
}

 
static u32 dpaa2_eth_ingress_fq_count(struct dpaa2_eth_priv *priv)
{
	struct dpaa2_eth_fq *fq;
	u32 fcnt = 0, bcnt = 0, total = 0;
	int i, err;

	for (i = 0; i < priv->num_fqs; i++) {
		fq = &priv->fq[i];
		err = dpaa2_io_query_fq_count(NULL, fq->fqid, &fcnt, &bcnt);
		if (err) {
			netdev_warn(priv->net_dev, "query_fq_count failed");
			break;
		}
		total += fcnt;
	}

	return total;
}

static void dpaa2_eth_wait_for_ingress_fq_empty(struct dpaa2_eth_priv *priv)
{
	int retries = 10;
	u32 pending;

	do {
		pending = dpaa2_eth_ingress_fq_count(priv);
		if (pending)
			msleep(100);
	} while (pending && --retries);
}

#define DPNI_TX_PENDING_VER_MAJOR	7
#define DPNI_TX_PENDING_VER_MINOR	13
static void dpaa2_eth_wait_for_egress_fq_empty(struct dpaa2_eth_priv *priv)
{
	union dpni_statistics stats;
	int retries = 10;
	int err;

	if (dpaa2_eth_cmp_dpni_ver(priv, DPNI_TX_PENDING_VER_MAJOR,
				   DPNI_TX_PENDING_VER_MINOR) < 0)
		goto out;

	do {
		err = dpni_get_statistics(priv->mc_io, 0, priv->mc_token, 6,
					  &stats);
		if (err)
			goto out;
		if (stats.page_6.tx_pending_frames == 0)
			return;
	} while (--retries);

out:
	msleep(500);
}

static int dpaa2_eth_stop(struct net_device *net_dev)
{
	struct dpaa2_eth_priv *priv = netdev_priv(net_dev);
	int dpni_enabled = 0;
	int retries = 10;

	mutex_lock(&priv->mac_lock);

	if (dpaa2_eth_is_type_phy(priv)) {
		dpaa2_mac_stop(priv->mac);
	} else {
		netif_tx_stop_all_queues(net_dev);
		netif_carrier_off(net_dev);
	}

	mutex_unlock(&priv->mac_lock);

	 
	dpaa2_eth_wait_for_egress_fq_empty(priv);

	do {
		dpni_disable(priv->mc_io, 0, priv->mc_token);
		dpni_is_enabled(priv->mc_io, 0, priv->mc_token, &dpni_enabled);
		if (dpni_enabled)
			 
			msleep(100);
	} while (dpni_enabled && --retries);
	if (!retries) {
		netdev_warn(net_dev, "Retry count exceeded disabling DPNI\n");
		 
	}

	dpaa2_eth_wait_for_ingress_fq_empty(priv);
	dpaa2_eth_disable_ch_napi(priv);

	 
	dpaa2_eth_drain_pools(priv);

	 
	dpaa2_eth_sgt_cache_drain(priv);

	return 0;
}

static int dpaa2_eth_set_addr(struct net_device *net_dev, void *addr)
{
	struct dpaa2_eth_priv *priv = netdev_priv(net_dev);
	struct device *dev = net_dev->dev.parent;
	int err;

	err = eth_mac_addr(net_dev, addr);
	if (err < 0) {
		dev_err(dev, "eth_mac_addr() failed (%d)\n", err);
		return err;
	}

	err = dpni_set_primary_mac_addr(priv->mc_io, 0, priv->mc_token,
					net_dev->dev_addr);
	if (err) {
		dev_err(dev, "dpni_set_primary_mac_addr() failed (%d)\n", err);
		return err;
	}

	return 0;
}

 
static void dpaa2_eth_get_stats(struct net_device *net_dev,
				struct rtnl_link_stats64 *stats)
{
	struct dpaa2_eth_priv *priv = netdev_priv(net_dev);
	struct rtnl_link_stats64 *percpu_stats;
	u64 *cpustats;
	u64 *netstats = (u64 *)stats;
	int i, j;
	int num = sizeof(struct rtnl_link_stats64) / sizeof(u64);

	for_each_possible_cpu(i) {
		percpu_stats = per_cpu_ptr(priv->percpu_stats, i);
		cpustats = (u64 *)percpu_stats;
		for (j = 0; j < num; j++)
			netstats[j] += cpustats[j];
	}
}

 
static void dpaa2_eth_add_uc_hw_addr(const struct net_device *net_dev,
				     struct dpaa2_eth_priv *priv)
{
	struct netdev_hw_addr *ha;
	int err;

	netdev_for_each_uc_addr(ha, net_dev) {
		err = dpni_add_mac_addr(priv->mc_io, 0, priv->mc_token,
					ha->addr);
		if (err)
			netdev_warn(priv->net_dev,
				    "Could not add ucast MAC %pM to the filtering table (err %d)\n",
				    ha->addr, err);
	}
}

 
static void dpaa2_eth_add_mc_hw_addr(const struct net_device *net_dev,
				     struct dpaa2_eth_priv *priv)
{
	struct netdev_hw_addr *ha;
	int err;

	netdev_for_each_mc_addr(ha, net_dev) {
		err = dpni_add_mac_addr(priv->mc_io, 0, priv->mc_token,
					ha->addr);
		if (err)
			netdev_warn(priv->net_dev,
				    "Could not add mcast MAC %pM to the filtering table (err %d)\n",
				    ha->addr, err);
	}
}

static int dpaa2_eth_rx_add_vid(struct net_device *net_dev,
				__be16 vlan_proto, u16 vid)
{
	struct dpaa2_eth_priv *priv = netdev_priv(net_dev);
	int err;

	err = dpni_add_vlan_id(priv->mc_io, 0, priv->mc_token,
			       vid, 0, 0, 0);

	if (err) {
		netdev_warn(priv->net_dev,
			    "Could not add the vlan id %u\n",
			    vid);
		return err;
	}

	return 0;
}

static int dpaa2_eth_rx_kill_vid(struct net_device *net_dev,
				 __be16 vlan_proto, u16 vid)
{
	struct dpaa2_eth_priv *priv = netdev_priv(net_dev);
	int err;

	err = dpni_remove_vlan_id(priv->mc_io, 0, priv->mc_token, vid);

	if (err) {
		netdev_warn(priv->net_dev,
			    "Could not remove the vlan id %u\n",
			    vid);
		return err;
	}

	return 0;
}

static void dpaa2_eth_set_rx_mode(struct net_device *net_dev)
{
	struct dpaa2_eth_priv *priv = netdev_priv(net_dev);
	int uc_count = netdev_uc_count(net_dev);
	int mc_count = netdev_mc_count(net_dev);
	u8 max_mac = priv->dpni_attrs.mac_filter_entries;
	u32 options = priv->dpni_attrs.options;
	u16 mc_token = priv->mc_token;
	struct fsl_mc_io *mc_io = priv->mc_io;
	int err;

	 
	if (options & DPNI_OPT_NO_MAC_FILTER && max_mac != 0)
		netdev_info(net_dev,
			    "mac_filter_entries=%d, DPNI_OPT_NO_MAC_FILTER option must be disabled\n",
			    max_mac);

	 
	if (uc_count > max_mac) {
		netdev_info(net_dev,
			    "Unicast addr count reached %d, max allowed is %d; forcing promisc\n",
			    uc_count, max_mac);
		goto force_promisc;
	}
	if (mc_count + uc_count > max_mac) {
		netdev_info(net_dev,
			    "Unicast + multicast addr count reached %d, max allowed is %d; forcing promisc\n",
			    uc_count + mc_count, max_mac);
		goto force_mc_promisc;
	}

	 
	if (net_dev->flags & IFF_PROMISC)
		goto force_promisc;
	if (net_dev->flags & IFF_ALLMULTI) {
		 
		err = dpni_set_unicast_promisc(mc_io, 0, mc_token, 1);
		if (err)
			netdev_warn(net_dev, "Can't set uc promisc\n");

		 
		err = dpni_clear_mac_filters(mc_io, 0, mc_token, 1, 0);
		if (err)
			netdev_warn(net_dev, "Can't clear uc filters\n");
		dpaa2_eth_add_uc_hw_addr(net_dev, priv);

		 
		err = dpni_set_unicast_promisc(mc_io, 0, mc_token, 0);
		if (err)
			netdev_warn(net_dev, "Can't clear uc promisc\n");
		goto force_mc_promisc;
	}

	 
	err = dpni_set_unicast_promisc(mc_io, 0, mc_token, 1);
	if (err)
		netdev_warn(net_dev, "Can't set uc promisc (%d)\n", err);
	err = dpni_set_multicast_promisc(mc_io, 0, mc_token, 1);
	if (err)
		netdev_warn(net_dev, "Can't set mc promisc (%d)\n", err);

	 
	err = dpni_clear_mac_filters(mc_io, 0, mc_token, 1, 1);
	if (err)
		netdev_warn(net_dev, "Can't clear mac filters\n");
	dpaa2_eth_add_mc_hw_addr(net_dev, priv);
	dpaa2_eth_add_uc_hw_addr(net_dev, priv);

	 
	err = dpni_set_unicast_promisc(mc_io, 0, mc_token, 0);
	if (err)
		netdev_warn(net_dev, "Can't clear ucast promisc\n");
	err = dpni_set_multicast_promisc(mc_io, 0, mc_token, 0);
	if (err)
		netdev_warn(net_dev, "Can't clear mcast promisc\n");

	return;

force_promisc:
	err = dpni_set_unicast_promisc(mc_io, 0, mc_token, 1);
	if (err)
		netdev_warn(net_dev, "Can't set ucast promisc\n");
force_mc_promisc:
	err = dpni_set_multicast_promisc(mc_io, 0, mc_token, 1);
	if (err)
		netdev_warn(net_dev, "Can't set mcast promisc\n");
}

static int dpaa2_eth_set_features(struct net_device *net_dev,
				  netdev_features_t features)
{
	struct dpaa2_eth_priv *priv = netdev_priv(net_dev);
	netdev_features_t changed = features ^ net_dev->features;
	bool enable;
	int err;

	if (changed & NETIF_F_HW_VLAN_CTAG_FILTER) {
		enable = !!(features & NETIF_F_HW_VLAN_CTAG_FILTER);
		err = dpaa2_eth_set_rx_vlan_filtering(priv, enable);
		if (err)
			return err;
	}

	if (changed & NETIF_F_RXCSUM) {
		enable = !!(features & NETIF_F_RXCSUM);
		err = dpaa2_eth_set_rx_csum(priv, enable);
		if (err)
			return err;
	}

	if (changed & (NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM)) {
		enable = !!(features & (NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM));
		err = dpaa2_eth_set_tx_csum(priv, enable);
		if (err)
			return err;
	}

	return 0;
}

static int dpaa2_eth_ts_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct dpaa2_eth_priv *priv = netdev_priv(dev);
	struct hwtstamp_config config;

	if (!dpaa2_ptp)
		return -EINVAL;

	if (copy_from_user(&config, rq->ifr_data, sizeof(config)))
		return -EFAULT;

	switch (config.tx_type) {
	case HWTSTAMP_TX_OFF:
	case HWTSTAMP_TX_ON:
	case HWTSTAMP_TX_ONESTEP_SYNC:
		priv->tx_tstamp_type = config.tx_type;
		break;
	default:
		return -ERANGE;
	}

	if (config.rx_filter == HWTSTAMP_FILTER_NONE) {
		priv->rx_tstamp = false;
	} else {
		priv->rx_tstamp = true;
		 
		config.rx_filter = HWTSTAMP_FILTER_ALL;
	}

	if (priv->tx_tstamp_type == HWTSTAMP_TX_ONESTEP_SYNC)
		dpaa2_ptp_onestep_reg_update_method(priv);

	return copy_to_user(rq->ifr_data, &config, sizeof(config)) ?
			-EFAULT : 0;
}

static int dpaa2_eth_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct dpaa2_eth_priv *priv = netdev_priv(dev);
	int err;

	if (cmd == SIOCSHWTSTAMP)
		return dpaa2_eth_ts_ioctl(dev, rq, cmd);

	mutex_lock(&priv->mac_lock);

	if (dpaa2_eth_is_type_phy(priv)) {
		err = phylink_mii_ioctl(priv->mac->phylink, rq, cmd);
		mutex_unlock(&priv->mac_lock);
		return err;
	}

	mutex_unlock(&priv->mac_lock);

	return -EOPNOTSUPP;
}

static bool xdp_mtu_valid(struct dpaa2_eth_priv *priv, int mtu)
{
	int mfl, linear_mfl;

	mfl = DPAA2_ETH_L2_MAX_FRM(mtu);
	linear_mfl = priv->rx_buf_size - DPAA2_ETH_RX_HWA_SIZE -
		     dpaa2_eth_rx_head_room(priv) - XDP_PACKET_HEADROOM;

	if (mfl > linear_mfl) {
		netdev_warn(priv->net_dev, "Maximum MTU for XDP is %d\n",
			    linear_mfl - VLAN_ETH_HLEN);
		return false;
	}

	return true;
}

static int dpaa2_eth_set_rx_mfl(struct dpaa2_eth_priv *priv, int mtu, bool has_xdp)
{
	int mfl, err;

	 
	if (has_xdp)
		mfl = DPAA2_ETH_L2_MAX_FRM(mtu);
	else
		mfl = DPAA2_ETH_MFL;

	err = dpni_set_max_frame_length(priv->mc_io, 0, priv->mc_token, mfl);
	if (err) {
		netdev_err(priv->net_dev, "dpni_set_max_frame_length failed\n");
		return err;
	}

	return 0;
}

static int dpaa2_eth_change_mtu(struct net_device *dev, int new_mtu)
{
	struct dpaa2_eth_priv *priv = netdev_priv(dev);
	int err;

	if (!priv->xdp_prog)
		goto out;

	if (!xdp_mtu_valid(priv, new_mtu))
		return -EINVAL;

	err = dpaa2_eth_set_rx_mfl(priv, new_mtu, true);
	if (err)
		return err;

out:
	dev->mtu = new_mtu;
	return 0;
}

static int dpaa2_eth_update_rx_buffer_headroom(struct dpaa2_eth_priv *priv, bool has_xdp)
{
	struct dpni_buffer_layout buf_layout = {0};
	int err;

	err = dpni_get_buffer_layout(priv->mc_io, 0, priv->mc_token,
				     DPNI_QUEUE_RX, &buf_layout);
	if (err) {
		netdev_err(priv->net_dev, "dpni_get_buffer_layout failed\n");
		return err;
	}

	 
	buf_layout.data_head_room = dpaa2_eth_rx_head_room(priv) +
				    (has_xdp ? XDP_PACKET_HEADROOM : 0);
	buf_layout.options = DPNI_BUF_LAYOUT_OPT_DATA_HEAD_ROOM;
	err = dpni_set_buffer_layout(priv->mc_io, 0, priv->mc_token,
				     DPNI_QUEUE_RX, &buf_layout);
	if (err) {
		netdev_err(priv->net_dev, "dpni_set_buffer_layout failed\n");
		return err;
	}

	return 0;
}

static int dpaa2_eth_setup_xdp(struct net_device *dev, struct bpf_prog *prog)
{
	struct dpaa2_eth_priv *priv = netdev_priv(dev);
	struct dpaa2_eth_channel *ch;
	struct bpf_prog *old;
	bool up, need_update;
	int i, err;

	if (prog && !xdp_mtu_valid(priv, dev->mtu))
		return -EINVAL;

	if (prog)
		bpf_prog_add(prog, priv->num_channels);

	up = netif_running(dev);
	need_update = (!!priv->xdp_prog != !!prog);

	if (up)
		dev_close(dev);

	 
	if (need_update) {
		err = dpaa2_eth_set_rx_mfl(priv, dev->mtu, !!prog);
		if (err)
			goto out_err;
		err = dpaa2_eth_update_rx_buffer_headroom(priv, !!prog);
		if (err)
			goto out_err;
	}

	old = xchg(&priv->xdp_prog, prog);
	if (old)
		bpf_prog_put(old);

	for (i = 0; i < priv->num_channels; i++) {
		ch = priv->channel[i];
		old = xchg(&ch->xdp.prog, prog);
		if (old)
			bpf_prog_put(old);
	}

	if (up) {
		err = dev_open(dev, NULL);
		if (err)
			return err;
	}

	return 0;

out_err:
	if (prog)
		bpf_prog_sub(prog, priv->num_channels);
	if (up)
		dev_open(dev, NULL);

	return err;
}

static int dpaa2_eth_xdp(struct net_device *dev, struct netdev_bpf *xdp)
{
	switch (xdp->command) {
	case XDP_SETUP_PROG:
		return dpaa2_eth_setup_xdp(dev, xdp->prog);
	case XDP_SETUP_XSK_POOL:
		return dpaa2_xsk_setup_pool(dev, xdp->xsk.pool, xdp->xsk.queue_id);
	default:
		return -EINVAL;
	}

	return 0;
}

static int dpaa2_eth_xdp_create_fd(struct net_device *net_dev,
				   struct xdp_frame *xdpf,
				   struct dpaa2_fd *fd)
{
	struct device *dev = net_dev->dev.parent;
	unsigned int needed_headroom;
	struct dpaa2_eth_swa *swa;
	void *buffer_start, *aligned_start;
	dma_addr_t addr;

	 
	needed_headroom = dpaa2_eth_needed_headroom(NULL);
	if (xdpf->headroom < needed_headroom)
		return -EINVAL;

	 
	memset(fd, 0, sizeof(*fd));

	 
	buffer_start = xdpf->data - needed_headroom;
	aligned_start = PTR_ALIGN(buffer_start - DPAA2_ETH_TX_BUF_ALIGN,
				  DPAA2_ETH_TX_BUF_ALIGN);
	if (aligned_start >= xdpf->data - xdpf->headroom)
		buffer_start = aligned_start;

	swa = (struct dpaa2_eth_swa *)buffer_start;
	 
	swa->type = DPAA2_ETH_SWA_XDP;
	swa->xdp.dma_size = xdpf->data + xdpf->len - buffer_start;
	swa->xdp.xdpf = xdpf;

	addr = dma_map_single(dev, buffer_start,
			      swa->xdp.dma_size,
			      DMA_BIDIRECTIONAL);
	if (unlikely(dma_mapping_error(dev, addr)))
		return -ENOMEM;

	dpaa2_fd_set_addr(fd, addr);
	dpaa2_fd_set_offset(fd, xdpf->data - buffer_start);
	dpaa2_fd_set_len(fd, xdpf->len);
	dpaa2_fd_set_format(fd, dpaa2_fd_single);
	dpaa2_fd_set_ctrl(fd, FD_CTRL_PTA);

	return 0;
}

static int dpaa2_eth_xdp_xmit(struct net_device *net_dev, int n,
			      struct xdp_frame **frames, u32 flags)
{
	struct dpaa2_eth_priv *priv = netdev_priv(net_dev);
	struct dpaa2_eth_xdp_fds *xdp_redirect_fds;
	struct rtnl_link_stats64 *percpu_stats;
	struct dpaa2_eth_fq *fq;
	struct dpaa2_fd *fds;
	int enqueued, i, err;

	if (unlikely(flags & ~XDP_XMIT_FLAGS_MASK))
		return -EINVAL;

	if (!netif_running(net_dev))
		return -ENETDOWN;

	fq = &priv->fq[smp_processor_id()];
	xdp_redirect_fds = &fq->xdp_redirect_fds;
	fds = xdp_redirect_fds->fds;

	percpu_stats = this_cpu_ptr(priv->percpu_stats);

	 
	for (i = 0; i < n; i++) {
		err = dpaa2_eth_xdp_create_fd(net_dev, frames[i], &fds[i]);
		if (err)
			break;
	}
	xdp_redirect_fds->num = i;

	 
	enqueued = dpaa2_eth_xdp_flush(priv, fq, xdp_redirect_fds);

	 
	percpu_stats->tx_packets += enqueued;
	for (i = 0; i < enqueued; i++)
		percpu_stats->tx_bytes += dpaa2_fd_get_len(&fds[i]);

	return enqueued;
}

static int update_xps(struct dpaa2_eth_priv *priv)
{
	struct net_device *net_dev = priv->net_dev;
	struct cpumask xps_mask;
	struct dpaa2_eth_fq *fq;
	int i, num_queues, netdev_queues;
	int err = 0;

	num_queues = dpaa2_eth_queue_count(priv);
	netdev_queues = (net_dev->num_tc ? : 1) * num_queues;

	 
	for (i = 0; i < netdev_queues; i++) {
		fq = &priv->fq[i % num_queues];

		cpumask_clear(&xps_mask);
		cpumask_set_cpu(fq->target_cpu, &xps_mask);

		err = netif_set_xps_queue(net_dev, &xps_mask, i);
		if (err) {
			netdev_warn_once(net_dev, "Error setting XPS queue\n");
			break;
		}
	}

	return err;
}

static int dpaa2_eth_setup_mqprio(struct net_device *net_dev,
				  struct tc_mqprio_qopt *mqprio)
{
	struct dpaa2_eth_priv *priv = netdev_priv(net_dev);
	u8 num_tc, num_queues;
	int i;

	mqprio->hw = TC_MQPRIO_HW_OFFLOAD_TCS;
	num_queues = dpaa2_eth_queue_count(priv);
	num_tc = mqprio->num_tc;

	if (num_tc == net_dev->num_tc)
		return 0;

	if (num_tc  > dpaa2_eth_tc_count(priv)) {
		netdev_err(net_dev, "Max %d traffic classes supported\n",
			   dpaa2_eth_tc_count(priv));
		return -EOPNOTSUPP;
	}

	if (!num_tc) {
		netdev_reset_tc(net_dev);
		netif_set_real_num_tx_queues(net_dev, num_queues);
		goto out;
	}

	netdev_set_num_tc(net_dev, num_tc);
	netif_set_real_num_tx_queues(net_dev, num_tc * num_queues);

	for (i = 0; i < num_tc; i++)
		netdev_set_tc_queue(net_dev, i, num_queues, i * num_queues);

out:
	update_xps(priv);

	return 0;
}

#define bps_to_mbits(rate) (div_u64((rate), 1000000) * 8)

static int dpaa2_eth_setup_tbf(struct net_device *net_dev, struct tc_tbf_qopt_offload *p)
{
	struct tc_tbf_qopt_offload_replace_params *cfg = &p->replace_params;
	struct dpaa2_eth_priv *priv = netdev_priv(net_dev);
	struct dpni_tx_shaping_cfg tx_cr_shaper = { 0 };
	struct dpni_tx_shaping_cfg tx_er_shaper = { 0 };
	int err;

	if (p->command == TC_TBF_STATS)
		return -EOPNOTSUPP;

	 
	if (p->parent != TC_H_ROOT)
		return -EOPNOTSUPP;

	if (p->command == TC_TBF_REPLACE) {
		if (cfg->max_size > DPAA2_ETH_MAX_BURST_SIZE) {
			netdev_err(net_dev, "burst size cannot be greater than %d\n",
				   DPAA2_ETH_MAX_BURST_SIZE);
			return -EINVAL;
		}

		tx_cr_shaper.max_burst_size = cfg->max_size;
		 
		tx_cr_shaper.rate_limit = bps_to_mbits(cfg->rate.rate_bytes_ps);
	}

	err = dpni_set_tx_shaping(priv->mc_io, 0, priv->mc_token, &tx_cr_shaper,
				  &tx_er_shaper, 0);
	if (err) {
		netdev_err(net_dev, "dpni_set_tx_shaping() = %d\n", err);
		return err;
	}

	return 0;
}

static int dpaa2_eth_setup_tc(struct net_device *net_dev,
			      enum tc_setup_type type, void *type_data)
{
	switch (type) {
	case TC_SETUP_QDISC_MQPRIO:
		return dpaa2_eth_setup_mqprio(net_dev, type_data);
	case TC_SETUP_QDISC_TBF:
		return dpaa2_eth_setup_tbf(net_dev, type_data);
	default:
		return -EOPNOTSUPP;
	}
}

static const struct net_device_ops dpaa2_eth_ops = {
	.ndo_open = dpaa2_eth_open,
	.ndo_start_xmit = dpaa2_eth_tx,
	.ndo_stop = dpaa2_eth_stop,
	.ndo_set_mac_address = dpaa2_eth_set_addr,
	.ndo_get_stats64 = dpaa2_eth_get_stats,
	.ndo_set_rx_mode = dpaa2_eth_set_rx_mode,
	.ndo_set_features = dpaa2_eth_set_features,
	.ndo_eth_ioctl = dpaa2_eth_ioctl,
	.ndo_change_mtu = dpaa2_eth_change_mtu,
	.ndo_bpf = dpaa2_eth_xdp,
	.ndo_xdp_xmit = dpaa2_eth_xdp_xmit,
	.ndo_xsk_wakeup = dpaa2_xsk_wakeup,
	.ndo_setup_tc = dpaa2_eth_setup_tc,
	.ndo_vlan_rx_add_vid = dpaa2_eth_rx_add_vid,
	.ndo_vlan_rx_kill_vid = dpaa2_eth_rx_kill_vid
};

static void dpaa2_eth_cdan_cb(struct dpaa2_io_notification_ctx *ctx)
{
	struct dpaa2_eth_channel *ch;

	ch = container_of(ctx, struct dpaa2_eth_channel, nctx);

	 
	ch->stats.cdan++;

	 
	if (!napi_if_scheduled_mark_missed(&ch->napi))
		napi_schedule(&ch->napi);
}

 
static struct fsl_mc_device *dpaa2_eth_setup_dpcon(struct dpaa2_eth_priv *priv)
{
	struct fsl_mc_device *dpcon;
	struct device *dev = priv->net_dev->dev.parent;
	int err;

	err = fsl_mc_object_allocate(to_fsl_mc_device(dev),
				     FSL_MC_POOL_DPCON, &dpcon);
	if (err) {
		if (err == -ENXIO) {
			dev_dbg(dev, "Waiting for DPCON\n");
			err = -EPROBE_DEFER;
		} else {
			dev_info(dev, "Not enough DPCONs, will go on as-is\n");
		}
		return ERR_PTR(err);
	}

	err = dpcon_open(priv->mc_io, 0, dpcon->obj_desc.id, &dpcon->mc_handle);
	if (err) {
		dev_err(dev, "dpcon_open() failed\n");
		goto free;
	}

	err = dpcon_reset(priv->mc_io, 0, dpcon->mc_handle);
	if (err) {
		dev_err(dev, "dpcon_reset() failed\n");
		goto close;
	}

	err = dpcon_enable(priv->mc_io, 0, dpcon->mc_handle);
	if (err) {
		dev_err(dev, "dpcon_enable() failed\n");
		goto close;
	}

	return dpcon;

close:
	dpcon_close(priv->mc_io, 0, dpcon->mc_handle);
free:
	fsl_mc_object_free(dpcon);

	return ERR_PTR(err);
}

static void dpaa2_eth_free_dpcon(struct dpaa2_eth_priv *priv,
				 struct fsl_mc_device *dpcon)
{
	dpcon_disable(priv->mc_io, 0, dpcon->mc_handle);
	dpcon_close(priv->mc_io, 0, dpcon->mc_handle);
	fsl_mc_object_free(dpcon);
}

static struct dpaa2_eth_channel *dpaa2_eth_alloc_channel(struct dpaa2_eth_priv *priv)
{
	struct dpaa2_eth_channel *channel;
	struct dpcon_attr attr;
	struct device *dev = priv->net_dev->dev.parent;
	int err;

	channel = kzalloc(sizeof(*channel), GFP_KERNEL);
	if (!channel)
		return NULL;

	channel->dpcon = dpaa2_eth_setup_dpcon(priv);
	if (IS_ERR(channel->dpcon)) {
		err = PTR_ERR(channel->dpcon);
		goto err_setup;
	}

	err = dpcon_get_attributes(priv->mc_io, 0, channel->dpcon->mc_handle,
				   &attr);
	if (err) {
		dev_err(dev, "dpcon_get_attributes() failed\n");
		goto err_get_attr;
	}

	channel->dpcon_id = attr.id;
	channel->ch_id = attr.qbman_ch_id;
	channel->priv = priv;

	return channel;

err_get_attr:
	dpaa2_eth_free_dpcon(priv, channel->dpcon);
err_setup:
	kfree(channel);
	return ERR_PTR(err);
}

static void dpaa2_eth_free_channel(struct dpaa2_eth_priv *priv,
				   struct dpaa2_eth_channel *channel)
{
	dpaa2_eth_free_dpcon(priv, channel->dpcon);
	kfree(channel);
}

 
static int dpaa2_eth_setup_dpio(struct dpaa2_eth_priv *priv)
{
	struct dpaa2_io_notification_ctx *nctx;
	struct dpaa2_eth_channel *channel;
	struct dpcon_notification_cfg dpcon_notif_cfg;
	struct device *dev = priv->net_dev->dev.parent;
	int i, err;

	 
	cpumask_clear(&priv->dpio_cpumask);
	for_each_online_cpu(i) {
		 
		channel = dpaa2_eth_alloc_channel(priv);
		if (IS_ERR_OR_NULL(channel)) {
			err = PTR_ERR_OR_ZERO(channel);
			if (err == -EPROBE_DEFER)
				dev_dbg(dev, "waiting for affine channel\n");
			else
				dev_info(dev,
					 "No affine channel for cpu %d and above\n", i);
			goto err_alloc_ch;
		}

		priv->channel[priv->num_channels] = channel;

		nctx = &channel->nctx;
		nctx->is_cdan = 1;
		nctx->cb = dpaa2_eth_cdan_cb;
		nctx->id = channel->ch_id;
		nctx->desired_cpu = i;

		 
		channel->dpio = dpaa2_io_service_select(i);
		err = dpaa2_io_service_register(channel->dpio, nctx, dev);
		if (err) {
			dev_dbg(dev, "No affine DPIO for cpu %d\n", i);
			 
			err = -EPROBE_DEFER;
			goto err_service_reg;
		}

		 
		dpcon_notif_cfg.dpio_id = nctx->dpio_id;
		dpcon_notif_cfg.priority = 0;
		dpcon_notif_cfg.user_ctx = nctx->qman64;
		err = dpcon_set_notification(priv->mc_io, 0,
					     channel->dpcon->mc_handle,
					     &dpcon_notif_cfg);
		if (err) {
			dev_err(dev, "dpcon_set_notification failed()\n");
			goto err_set_cdan;
		}

		 
		cpumask_set_cpu(i, &priv->dpio_cpumask);
		priv->num_channels++;

		 
		if (priv->num_channels == priv->dpni_attrs.num_queues)
			break;
	}

	return 0;

err_set_cdan:
	dpaa2_io_service_deregister(channel->dpio, nctx, dev);
err_service_reg:
	dpaa2_eth_free_channel(priv, channel);
err_alloc_ch:
	if (err == -EPROBE_DEFER) {
		for (i = 0; i < priv->num_channels; i++) {
			channel = priv->channel[i];
			nctx = &channel->nctx;
			dpaa2_io_service_deregister(channel->dpio, nctx, dev);
			dpaa2_eth_free_channel(priv, channel);
		}
		priv->num_channels = 0;
		return err;
	}

	if (cpumask_empty(&priv->dpio_cpumask)) {
		dev_err(dev, "No cpu with an affine DPIO/DPCON\n");
		return -ENODEV;
	}

	dev_info(dev, "Cores %*pbl available for processing ingress traffic\n",
		 cpumask_pr_args(&priv->dpio_cpumask));

	return 0;
}

static void dpaa2_eth_free_dpio(struct dpaa2_eth_priv *priv)
{
	struct device *dev = priv->net_dev->dev.parent;
	struct dpaa2_eth_channel *ch;
	int i;

	 
	for (i = 0; i < priv->num_channels; i++) {
		ch = priv->channel[i];
		dpaa2_io_service_deregister(ch->dpio, &ch->nctx, dev);
		dpaa2_eth_free_channel(priv, ch);
	}
}

static struct dpaa2_eth_channel *dpaa2_eth_get_affine_channel(struct dpaa2_eth_priv *priv,
							      int cpu)
{
	struct device *dev = priv->net_dev->dev.parent;
	int i;

	for (i = 0; i < priv->num_channels; i++)
		if (priv->channel[i]->nctx.desired_cpu == cpu)
			return priv->channel[i];

	 
	dev_warn(dev, "No affine channel found for cpu %d\n", cpu);

	return priv->channel[0];
}

static void dpaa2_eth_set_fq_affinity(struct dpaa2_eth_priv *priv)
{
	struct device *dev = priv->net_dev->dev.parent;
	struct dpaa2_eth_fq *fq;
	int rx_cpu, txc_cpu;
	int i;

	 
	rx_cpu = txc_cpu = cpumask_first(&priv->dpio_cpumask);

	for (i = 0; i < priv->num_fqs; i++) {
		fq = &priv->fq[i];
		switch (fq->type) {
		case DPAA2_RX_FQ:
		case DPAA2_RX_ERR_FQ:
			fq->target_cpu = rx_cpu;
			rx_cpu = cpumask_next(rx_cpu, &priv->dpio_cpumask);
			if (rx_cpu >= nr_cpu_ids)
				rx_cpu = cpumask_first(&priv->dpio_cpumask);
			break;
		case DPAA2_TX_CONF_FQ:
			fq->target_cpu = txc_cpu;
			txc_cpu = cpumask_next(txc_cpu, &priv->dpio_cpumask);
			if (txc_cpu >= nr_cpu_ids)
				txc_cpu = cpumask_first(&priv->dpio_cpumask);
			break;
		default:
			dev_err(dev, "Unknown FQ type: %d\n", fq->type);
		}
		fq->channel = dpaa2_eth_get_affine_channel(priv, fq->target_cpu);
	}

	update_xps(priv);
}

static void dpaa2_eth_setup_fqs(struct dpaa2_eth_priv *priv)
{
	int i, j;

	 
	for (i = 0; i < dpaa2_eth_queue_count(priv); i++) {
		priv->fq[priv->num_fqs].type = DPAA2_TX_CONF_FQ;
		priv->fq[priv->num_fqs].consume = dpaa2_eth_tx_conf;
		priv->fq[priv->num_fqs++].flowid = (u16)i;
	}

	for (j = 0; j < dpaa2_eth_tc_count(priv); j++) {
		for (i = 0; i < dpaa2_eth_queue_count(priv); i++) {
			priv->fq[priv->num_fqs].type = DPAA2_RX_FQ;
			priv->fq[priv->num_fqs].consume = dpaa2_eth_rx;
			priv->fq[priv->num_fqs].tc = (u8)j;
			priv->fq[priv->num_fqs++].flowid = (u16)i;
		}
	}

	 
	priv->fq[priv->num_fqs].type = DPAA2_RX_ERR_FQ;
	priv->fq[priv->num_fqs++].consume = dpaa2_eth_rx_err;

	 
	dpaa2_eth_set_fq_affinity(priv);
}

 
struct dpaa2_eth_bp *dpaa2_eth_allocate_dpbp(struct dpaa2_eth_priv *priv)
{
	struct device *dev = priv->net_dev->dev.parent;
	struct fsl_mc_device *dpbp_dev;
	struct dpbp_attr dpbp_attrs;
	struct dpaa2_eth_bp *bp;
	int err;

	err = fsl_mc_object_allocate(to_fsl_mc_device(dev), FSL_MC_POOL_DPBP,
				     &dpbp_dev);
	if (err) {
		if (err == -ENXIO)
			err = -EPROBE_DEFER;
		else
			dev_err(dev, "DPBP device allocation failed\n");
		return ERR_PTR(err);
	}

	bp = kzalloc(sizeof(*bp), GFP_KERNEL);
	if (!bp) {
		err = -ENOMEM;
		goto err_alloc;
	}

	err = dpbp_open(priv->mc_io, 0, dpbp_dev->obj_desc.id,
			&dpbp_dev->mc_handle);
	if (err) {
		dev_err(dev, "dpbp_open() failed\n");
		goto err_open;
	}

	err = dpbp_reset(priv->mc_io, 0, dpbp_dev->mc_handle);
	if (err) {
		dev_err(dev, "dpbp_reset() failed\n");
		goto err_reset;
	}

	err = dpbp_enable(priv->mc_io, 0, dpbp_dev->mc_handle);
	if (err) {
		dev_err(dev, "dpbp_enable() failed\n");
		goto err_enable;
	}

	err = dpbp_get_attributes(priv->mc_io, 0, dpbp_dev->mc_handle,
				  &dpbp_attrs);
	if (err) {
		dev_err(dev, "dpbp_get_attributes() failed\n");
		goto err_get_attr;
	}

	bp->dev = dpbp_dev;
	bp->bpid = dpbp_attrs.bpid;

	return bp;

err_get_attr:
	dpbp_disable(priv->mc_io, 0, dpbp_dev->mc_handle);
err_enable:
err_reset:
	dpbp_close(priv->mc_io, 0, dpbp_dev->mc_handle);
err_open:
	kfree(bp);
err_alloc:
	fsl_mc_object_free(dpbp_dev);

	return ERR_PTR(err);
}

static int dpaa2_eth_setup_default_dpbp(struct dpaa2_eth_priv *priv)
{
	struct dpaa2_eth_bp *bp;
	int i;

	bp = dpaa2_eth_allocate_dpbp(priv);
	if (IS_ERR(bp))
		return PTR_ERR(bp);

	priv->bp[DPAA2_ETH_DEFAULT_BP_IDX] = bp;
	priv->num_bps++;

	for (i = 0; i < priv->num_channels; i++)
		priv->channel[i]->bp = bp;

	return 0;
}

void dpaa2_eth_free_dpbp(struct dpaa2_eth_priv *priv, struct dpaa2_eth_bp *bp)
{
	int idx_bp;

	 
	for (idx_bp = 0; idx_bp < priv->num_bps; idx_bp++)
		if (priv->bp[idx_bp] == bp)
			break;

	 
	dpaa2_eth_drain_pool(priv, bp->bpid);
	dpbp_disable(priv->mc_io, 0, bp->dev->mc_handle);
	dpbp_close(priv->mc_io, 0, bp->dev->mc_handle);
	fsl_mc_object_free(bp->dev);
	kfree(bp);

	 
	priv->bp[idx_bp] = priv->bp[priv->num_bps - 1];
	priv->num_bps--;
}

static void dpaa2_eth_free_dpbps(struct dpaa2_eth_priv *priv)
{
	int i;

	for (i = 0; i < priv->num_bps; i++)
		dpaa2_eth_free_dpbp(priv, priv->bp[i]);
}

static int dpaa2_eth_set_buffer_layout(struct dpaa2_eth_priv *priv)
{
	struct device *dev = priv->net_dev->dev.parent;
	struct dpni_buffer_layout buf_layout = {0};
	u16 rx_buf_align;
	int err;

	 
	if (priv->dpni_attrs.wriop_version == DPAA2_WRIOP_VERSION(0, 0, 0) ||
	    priv->dpni_attrs.wriop_version == DPAA2_WRIOP_VERSION(1, 0, 0))
		rx_buf_align = DPAA2_ETH_RX_BUF_ALIGN_REV1;
	else
		rx_buf_align = DPAA2_ETH_RX_BUF_ALIGN;

	 
	priv->rx_buf_size = ALIGN_DOWN(DPAA2_ETH_RX_BUF_SIZE, rx_buf_align);

	 
	buf_layout.private_data_size = DPAA2_ETH_SWA_SIZE;
	buf_layout.pass_timestamp = true;
	buf_layout.pass_frame_status = true;
	buf_layout.options = DPNI_BUF_LAYOUT_OPT_PRIVATE_DATA_SIZE |
			     DPNI_BUF_LAYOUT_OPT_TIMESTAMP |
			     DPNI_BUF_LAYOUT_OPT_FRAME_STATUS;
	err = dpni_set_buffer_layout(priv->mc_io, 0, priv->mc_token,
				     DPNI_QUEUE_TX, &buf_layout);
	if (err) {
		dev_err(dev, "dpni_set_buffer_layout(TX) failed\n");
		return err;
	}

	 
	buf_layout.options = DPNI_BUF_LAYOUT_OPT_TIMESTAMP |
			     DPNI_BUF_LAYOUT_OPT_FRAME_STATUS;
	err = dpni_set_buffer_layout(priv->mc_io, 0, priv->mc_token,
				     DPNI_QUEUE_TX_CONFIRM, &buf_layout);
	if (err) {
		dev_err(dev, "dpni_set_buffer_layout(TX_CONF) failed\n");
		return err;
	}

	 
	err = dpni_get_tx_data_offset(priv->mc_io, 0, priv->mc_token,
				      &priv->tx_data_offset);
	if (err) {
		dev_err(dev, "dpni_get_tx_data_offset() failed\n");
		return err;
	}

	if ((priv->tx_data_offset % 64) != 0)
		dev_warn(dev, "Tx data offset (%d) not a multiple of 64B\n",
			 priv->tx_data_offset);

	 
	buf_layout.pass_frame_status = true;
	buf_layout.pass_parser_result = true;
	buf_layout.data_align = rx_buf_align;
	buf_layout.data_head_room = dpaa2_eth_rx_head_room(priv);
	buf_layout.private_data_size = 0;
	buf_layout.options = DPNI_BUF_LAYOUT_OPT_PARSER_RESULT |
			     DPNI_BUF_LAYOUT_OPT_FRAME_STATUS |
			     DPNI_BUF_LAYOUT_OPT_DATA_ALIGN |
			     DPNI_BUF_LAYOUT_OPT_DATA_HEAD_ROOM |
			     DPNI_BUF_LAYOUT_OPT_TIMESTAMP;
	err = dpni_set_buffer_layout(priv->mc_io, 0, priv->mc_token,
				     DPNI_QUEUE_RX, &buf_layout);
	if (err) {
		dev_err(dev, "dpni_set_buffer_layout(RX) failed\n");
		return err;
	}

	return 0;
}

#define DPNI_ENQUEUE_FQID_VER_MAJOR	7
#define DPNI_ENQUEUE_FQID_VER_MINOR	9

static inline int dpaa2_eth_enqueue_qd(struct dpaa2_eth_priv *priv,
				       struct dpaa2_eth_fq *fq,
				       struct dpaa2_fd *fd, u8 prio,
				       u32 num_frames __always_unused,
				       int *frames_enqueued)
{
	int err;

	err = dpaa2_io_service_enqueue_qd(fq->channel->dpio,
					  priv->tx_qdid, prio,
					  fq->tx_qdbin, fd);
	if (!err && frames_enqueued)
		*frames_enqueued = 1;
	return err;
}

static inline int dpaa2_eth_enqueue_fq_multiple(struct dpaa2_eth_priv *priv,
						struct dpaa2_eth_fq *fq,
						struct dpaa2_fd *fd,
						u8 prio, u32 num_frames,
						int *frames_enqueued)
{
	int err;

	err = dpaa2_io_service_enqueue_multiple_fq(fq->channel->dpio,
						   fq->tx_fqid[prio],
						   fd, num_frames);

	if (err == 0)
		return -EBUSY;

	if (frames_enqueued)
		*frames_enqueued = err;
	return 0;
}

static void dpaa2_eth_set_enqueue_mode(struct dpaa2_eth_priv *priv)
{
	if (dpaa2_eth_cmp_dpni_ver(priv, DPNI_ENQUEUE_FQID_VER_MAJOR,
				   DPNI_ENQUEUE_FQID_VER_MINOR) < 0)
		priv->enqueue = dpaa2_eth_enqueue_qd;
	else
		priv->enqueue = dpaa2_eth_enqueue_fq_multiple;
}

static int dpaa2_eth_set_pause(struct dpaa2_eth_priv *priv)
{
	struct device *dev = priv->net_dev->dev.parent;
	struct dpni_link_cfg link_cfg = {0};
	int err;

	 
	err = dpni_get_link_cfg(priv->mc_io, 0, priv->mc_token, &link_cfg);
	if (err) {
		dev_err(dev, "dpni_get_link_cfg() failed\n");
		return err;
	}

	 
	link_cfg.options |= DPNI_LINK_OPT_PAUSE;
	link_cfg.options &= ~DPNI_LINK_OPT_ASYM_PAUSE;
	err = dpni_set_link_cfg(priv->mc_io, 0, priv->mc_token, &link_cfg);
	if (err) {
		dev_err(dev, "dpni_set_link_cfg() failed\n");
		return err;
	}

	priv->link_state.options = link_cfg.options;

	return 0;
}

static void dpaa2_eth_update_tx_fqids(struct dpaa2_eth_priv *priv)
{
	struct dpni_queue_id qid = {0};
	struct dpaa2_eth_fq *fq;
	struct dpni_queue queue;
	int i, j, err;

	 
	if (dpaa2_eth_cmp_dpni_ver(priv, DPNI_ENQUEUE_FQID_VER_MAJOR,
				   DPNI_ENQUEUE_FQID_VER_MINOR) < 0)
		return;

	for (i = 0; i < priv->num_fqs; i++) {
		fq = &priv->fq[i];
		if (fq->type != DPAA2_TX_CONF_FQ)
			continue;
		for (j = 0; j < dpaa2_eth_tc_count(priv); j++) {
			err = dpni_get_queue(priv->mc_io, 0, priv->mc_token,
					     DPNI_QUEUE_TX, j, fq->flowid,
					     &queue, &qid);
			if (err)
				goto out_err;

			fq->tx_fqid[j] = qid.fqid;
			if (fq->tx_fqid[j] == 0)
				goto out_err;
		}
	}

	priv->enqueue = dpaa2_eth_enqueue_fq_multiple;

	return;

out_err:
	netdev_info(priv->net_dev,
		    "Error reading Tx FQID, fallback to QDID-based enqueue\n");
	priv->enqueue = dpaa2_eth_enqueue_qd;
}

 
static int dpaa2_eth_set_vlan_qos(struct dpaa2_eth_priv *priv)
{
	struct device *dev = priv->net_dev->dev.parent;
	struct dpkg_profile_cfg kg_cfg = {0};
	struct dpni_qos_tbl_cfg qos_cfg = {0};
	struct dpni_rule_cfg key_params;
	void *dma_mem, *key, *mask;
	u8 key_size = 2;	 
	int i, pcp, err;

	 
	if (dpaa2_eth_tc_count(priv) == 1 || !dpaa2_eth_fs_mask_enabled(priv)) {
		dev_dbg(dev, "VLAN-based QoS classification not supported\n");
		return -EOPNOTSUPP;
	}

	dma_mem = kzalloc(DPAA2_CLASSIFIER_DMA_SIZE, GFP_KERNEL);
	if (!dma_mem)
		return -ENOMEM;

	kg_cfg.num_extracts = 1;
	kg_cfg.extracts[0].type = DPKG_EXTRACT_FROM_HDR;
	kg_cfg.extracts[0].extract.from_hdr.prot = NET_PROT_VLAN;
	kg_cfg.extracts[0].extract.from_hdr.type = DPKG_FULL_FIELD;
	kg_cfg.extracts[0].extract.from_hdr.field = NH_FLD_VLAN_TCI;

	err = dpni_prepare_key_cfg(&kg_cfg, dma_mem);
	if (err) {
		dev_err(dev, "dpni_prepare_key_cfg failed\n");
		goto out_free_tbl;
	}

	 
	qos_cfg.default_tc = 0;
	qos_cfg.discard_on_miss = 0;
	qos_cfg.key_cfg_iova = dma_map_single(dev, dma_mem,
					      DPAA2_CLASSIFIER_DMA_SIZE,
					      DMA_TO_DEVICE);
	if (dma_mapping_error(dev, qos_cfg.key_cfg_iova)) {
		dev_err(dev, "QoS table DMA mapping failed\n");
		err = -ENOMEM;
		goto out_free_tbl;
	}

	err = dpni_set_qos_table(priv->mc_io, 0, priv->mc_token, &qos_cfg);
	if (err) {
		dev_err(dev, "dpni_set_qos_table failed\n");
		goto out_unmap_tbl;
	}

	 
	key = kzalloc(key_size * 2, GFP_KERNEL);
	if (!key) {
		err = -ENOMEM;
		goto out_unmap_tbl;
	}
	mask = key + key_size;
	*(__be16 *)mask = cpu_to_be16(VLAN_PRIO_MASK);

	key_params.key_iova = dma_map_single(dev, key, key_size * 2,
					     DMA_TO_DEVICE);
	if (dma_mapping_error(dev, key_params.key_iova)) {
		dev_err(dev, "Qos table entry DMA mapping failed\n");
		err = -ENOMEM;
		goto out_free_key;
	}

	key_params.mask_iova = key_params.key_iova + key_size;
	key_params.key_size = key_size;

	 
	for (i = dpaa2_eth_tc_count(priv) - 1, pcp = 7; i >= 0; i--, pcp--) {
		*(__be16 *)key = cpu_to_be16(pcp << VLAN_PRIO_SHIFT);
		dma_sync_single_for_device(dev, key_params.key_iova,
					   key_size * 2, DMA_TO_DEVICE);

		err = dpni_add_qos_entry(priv->mc_io, 0, priv->mc_token,
					 &key_params, i, i);
		if (err) {
			dev_err(dev, "dpni_add_qos_entry failed\n");
			dpni_clear_qos_table(priv->mc_io, 0, priv->mc_token);
			goto out_unmap_key;
		}
	}

	priv->vlan_cls_enabled = true;

	 
out_unmap_key:
	dma_unmap_single(dev, key_params.key_iova, key_size * 2, DMA_TO_DEVICE);
out_free_key:
	kfree(key);
out_unmap_tbl:
	dma_unmap_single(dev, qos_cfg.key_cfg_iova, DPAA2_CLASSIFIER_DMA_SIZE,
			 DMA_TO_DEVICE);
out_free_tbl:
	kfree(dma_mem);

	return err;
}

 
static int dpaa2_eth_setup_dpni(struct fsl_mc_device *ls_dev)
{
	struct device *dev = &ls_dev->dev;
	struct dpaa2_eth_priv *priv;
	struct net_device *net_dev;
	int err;

	net_dev = dev_get_drvdata(dev);
	priv = netdev_priv(net_dev);

	 
	err = dpni_open(priv->mc_io, 0, ls_dev->obj_desc.id, &priv->mc_token);
	if (err) {
		dev_err(dev, "dpni_open() failed\n");
		return err;
	}

	 
	err = dpni_get_api_version(priv->mc_io, 0, &priv->dpni_ver_major,
				   &priv->dpni_ver_minor);
	if (err) {
		dev_err(dev, "dpni_get_api_version() failed\n");
		goto close;
	}
	if (dpaa2_eth_cmp_dpni_ver(priv, DPNI_VER_MAJOR, DPNI_VER_MINOR) < 0) {
		dev_err(dev, "DPNI version %u.%u not supported, need >= %u.%u\n",
			priv->dpni_ver_major, priv->dpni_ver_minor,
			DPNI_VER_MAJOR, DPNI_VER_MINOR);
		err = -EOPNOTSUPP;
		goto close;
	}

	ls_dev->mc_io = priv->mc_io;
	ls_dev->mc_handle = priv->mc_token;

	err = dpni_reset(priv->mc_io, 0, priv->mc_token);
	if (err) {
		dev_err(dev, "dpni_reset() failed\n");
		goto close;
	}

	err = dpni_get_attributes(priv->mc_io, 0, priv->mc_token,
				  &priv->dpni_attrs);
	if (err) {
		dev_err(dev, "dpni_get_attributes() failed (err=%d)\n", err);
		goto close;
	}

	err = dpaa2_eth_set_buffer_layout(priv);
	if (err)
		goto close;

	dpaa2_eth_set_enqueue_mode(priv);

	 
	if (dpaa2_eth_has_pause_support(priv)) {
		err = dpaa2_eth_set_pause(priv);
		if (err)
			goto close;
	}

	err = dpaa2_eth_set_vlan_qos(priv);
	if (err && err != -EOPNOTSUPP)
		goto close;

	priv->cls_rules = devm_kcalloc(dev, dpaa2_eth_fs_count(priv),
				       sizeof(struct dpaa2_eth_cls_rule),
				       GFP_KERNEL);
	if (!priv->cls_rules) {
		err = -ENOMEM;
		goto close;
	}

	return 0;

close:
	dpni_close(priv->mc_io, 0, priv->mc_token);

	return err;
}

static void dpaa2_eth_free_dpni(struct dpaa2_eth_priv *priv)
{
	int err;

	err = dpni_reset(priv->mc_io, 0, priv->mc_token);
	if (err)
		netdev_warn(priv->net_dev, "dpni_reset() failed (err %d)\n",
			    err);

	dpni_close(priv->mc_io, 0, priv->mc_token);
}

static int dpaa2_eth_setup_rx_flow(struct dpaa2_eth_priv *priv,
				   struct dpaa2_eth_fq *fq)
{
	struct device *dev = priv->net_dev->dev.parent;
	struct dpni_queue queue;
	struct dpni_queue_id qid;
	int err;

	err = dpni_get_queue(priv->mc_io, 0, priv->mc_token,
			     DPNI_QUEUE_RX, fq->tc, fq->flowid, &queue, &qid);
	if (err) {
		dev_err(dev, "dpni_get_queue(RX) failed\n");
		return err;
	}

	fq->fqid = qid.fqid;

	queue.destination.id = fq->channel->dpcon_id;
	queue.destination.type = DPNI_DEST_DPCON;
	queue.destination.priority = 1;
	queue.user_context = (u64)(uintptr_t)fq;
	err = dpni_set_queue(priv->mc_io, 0, priv->mc_token,
			     DPNI_QUEUE_RX, fq->tc, fq->flowid,
			     DPNI_QUEUE_OPT_USER_CTX | DPNI_QUEUE_OPT_DEST,
			     &queue);
	if (err) {
		dev_err(dev, "dpni_set_queue(RX) failed\n");
		return err;
	}

	 
	 
	if (fq->tc > 0)
		return 0;

	err = xdp_rxq_info_reg(&fq->channel->xdp_rxq, priv->net_dev,
			       fq->flowid, 0);
	if (err) {
		dev_err(dev, "xdp_rxq_info_reg failed\n");
		return err;
	}

	err = xdp_rxq_info_reg_mem_model(&fq->channel->xdp_rxq,
					 MEM_TYPE_PAGE_ORDER0, NULL);
	if (err) {
		dev_err(dev, "xdp_rxq_info_reg_mem_model failed\n");
		return err;
	}

	return 0;
}

static int dpaa2_eth_setup_tx_flow(struct dpaa2_eth_priv *priv,
				   struct dpaa2_eth_fq *fq)
{
	struct device *dev = priv->net_dev->dev.parent;
	struct dpni_queue queue;
	struct dpni_queue_id qid;
	int i, err;

	for (i = 0; i < dpaa2_eth_tc_count(priv); i++) {
		err = dpni_get_queue(priv->mc_io, 0, priv->mc_token,
				     DPNI_QUEUE_TX, i, fq->flowid,
				     &queue, &qid);
		if (err) {
			dev_err(dev, "dpni_get_queue(TX) failed\n");
			return err;
		}
		fq->tx_fqid[i] = qid.fqid;
	}

	 
	fq->tx_qdbin = qid.qdbin;

	err = dpni_get_queue(priv->mc_io, 0, priv->mc_token,
			     DPNI_QUEUE_TX_CONFIRM, 0, fq->flowid,
			     &queue, &qid);
	if (err) {
		dev_err(dev, "dpni_get_queue(TX_CONF) failed\n");
		return err;
	}

	fq->fqid = qid.fqid;

	queue.destination.id = fq->channel->dpcon_id;
	queue.destination.type = DPNI_DEST_DPCON;
	queue.destination.priority = 0;
	queue.user_context = (u64)(uintptr_t)fq;
	err = dpni_set_queue(priv->mc_io, 0, priv->mc_token,
			     DPNI_QUEUE_TX_CONFIRM, 0, fq->flowid,
			     DPNI_QUEUE_OPT_USER_CTX | DPNI_QUEUE_OPT_DEST,
			     &queue);
	if (err) {
		dev_err(dev, "dpni_set_queue(TX_CONF) failed\n");
		return err;
	}

	return 0;
}

static int setup_rx_err_flow(struct dpaa2_eth_priv *priv,
			     struct dpaa2_eth_fq *fq)
{
	struct device *dev = priv->net_dev->dev.parent;
	struct dpni_queue q = { { 0 } };
	struct dpni_queue_id qid;
	u8 q_opt = DPNI_QUEUE_OPT_USER_CTX | DPNI_QUEUE_OPT_DEST;
	int err;

	err = dpni_get_queue(priv->mc_io, 0, priv->mc_token,
			     DPNI_QUEUE_RX_ERR, 0, 0, &q, &qid);
	if (err) {
		dev_err(dev, "dpni_get_queue() failed (%d)\n", err);
		return err;
	}

	fq->fqid = qid.fqid;

	q.destination.id = fq->channel->dpcon_id;
	q.destination.type = DPNI_DEST_DPCON;
	q.destination.priority = 1;
	q.user_context = (u64)(uintptr_t)fq;
	err = dpni_set_queue(priv->mc_io, 0, priv->mc_token,
			     DPNI_QUEUE_RX_ERR, 0, 0, q_opt, &q);
	if (err) {
		dev_err(dev, "dpni_set_queue() failed (%d)\n", err);
		return err;
	}

	return 0;
}

 
static const struct dpaa2_eth_dist_fields dist_fields[] = {
	{
		 
		.rxnfc_field = RXH_L2DA,
		.cls_prot = NET_PROT_ETH,
		.cls_field = NH_FLD_ETH_DA,
		.id = DPAA2_ETH_DIST_ETHDST,
		.size = 6,
	}, {
		.cls_prot = NET_PROT_ETH,
		.cls_field = NH_FLD_ETH_SA,
		.id = DPAA2_ETH_DIST_ETHSRC,
		.size = 6,
	}, {
		 
		.cls_prot = NET_PROT_ETH,
		.cls_field = NH_FLD_ETH_TYPE,
		.id = DPAA2_ETH_DIST_ETHTYPE,
		.size = 2,
	}, {
		 
		.rxnfc_field = RXH_VLAN,
		.cls_prot = NET_PROT_VLAN,
		.cls_field = NH_FLD_VLAN_TCI,
		.id = DPAA2_ETH_DIST_VLAN,
		.size = 2,
	}, {
		 
		.rxnfc_field = RXH_IP_SRC,
		.cls_prot = NET_PROT_IP,
		.cls_field = NH_FLD_IP_SRC,
		.id = DPAA2_ETH_DIST_IPSRC,
		.size = 4,
	}, {
		.rxnfc_field = RXH_IP_DST,
		.cls_prot = NET_PROT_IP,
		.cls_field = NH_FLD_IP_DST,
		.id = DPAA2_ETH_DIST_IPDST,
		.size = 4,
	}, {
		.rxnfc_field = RXH_L3_PROTO,
		.cls_prot = NET_PROT_IP,
		.cls_field = NH_FLD_IP_PROTO,
		.id = DPAA2_ETH_DIST_IPPROTO,
		.size = 1,
	}, {
		 
		.rxnfc_field = RXH_L4_B_0_1,
		.cls_prot = NET_PROT_UDP,
		.cls_field = NH_FLD_UDP_PORT_SRC,
		.id = DPAA2_ETH_DIST_L4SRC,
		.size = 2,
	}, {
		.rxnfc_field = RXH_L4_B_2_3,
		.cls_prot = NET_PROT_UDP,
		.cls_field = NH_FLD_UDP_PORT_DST,
		.id = DPAA2_ETH_DIST_L4DST,
		.size = 2,
	},
};

 
static int dpaa2_eth_config_legacy_hash_key(struct dpaa2_eth_priv *priv, dma_addr_t key)
{
	struct device *dev = priv->net_dev->dev.parent;
	struct dpni_rx_tc_dist_cfg dist_cfg;
	int i, err = 0;

	memset(&dist_cfg, 0, sizeof(dist_cfg));

	dist_cfg.key_cfg_iova = key;
	dist_cfg.dist_size = dpaa2_eth_queue_count(priv);
	dist_cfg.dist_mode = DPNI_DIST_MODE_HASH;

	for (i = 0; i < dpaa2_eth_tc_count(priv); i++) {
		err = dpni_set_rx_tc_dist(priv->mc_io, 0, priv->mc_token,
					  i, &dist_cfg);
		if (err) {
			dev_err(dev, "dpni_set_rx_tc_dist failed\n");
			break;
		}
	}

	return err;
}

 
static int dpaa2_eth_config_hash_key(struct dpaa2_eth_priv *priv, dma_addr_t key)
{
	struct device *dev = priv->net_dev->dev.parent;
	struct dpni_rx_dist_cfg dist_cfg;
	int i, err = 0;

	memset(&dist_cfg, 0, sizeof(dist_cfg));

	dist_cfg.key_cfg_iova = key;
	dist_cfg.dist_size = dpaa2_eth_queue_count(priv);
	dist_cfg.enable = 1;

	for (i = 0; i < dpaa2_eth_tc_count(priv); i++) {
		dist_cfg.tc = i;
		err = dpni_set_rx_hash_dist(priv->mc_io, 0, priv->mc_token,
					    &dist_cfg);
		if (err) {
			dev_err(dev, "dpni_set_rx_hash_dist failed\n");
			break;
		}

		 
		if (priv->dpni_attrs.options & DPNI_OPT_SHARED_FS)
			break;
	}

	return err;
}

 
static int dpaa2_eth_config_cls_key(struct dpaa2_eth_priv *priv, dma_addr_t key)
{
	struct device *dev = priv->net_dev->dev.parent;
	struct dpni_rx_dist_cfg dist_cfg;
	int i, err = 0;

	memset(&dist_cfg, 0, sizeof(dist_cfg));

	dist_cfg.key_cfg_iova = key;
	dist_cfg.dist_size = dpaa2_eth_queue_count(priv);
	dist_cfg.enable = 1;

	for (i = 0; i < dpaa2_eth_tc_count(priv); i++) {
		dist_cfg.tc = i;
		err = dpni_set_rx_fs_dist(priv->mc_io, 0, priv->mc_token,
					  &dist_cfg);
		if (err) {
			dev_err(dev, "dpni_set_rx_fs_dist failed\n");
			break;
		}

		 
		if (priv->dpni_attrs.options & DPNI_OPT_SHARED_FS)
			break;
	}

	return err;
}

 
int dpaa2_eth_cls_key_size(u64 fields)
{
	int i, size = 0;

	for (i = 0; i < ARRAY_SIZE(dist_fields); i++) {
		if (!(fields & dist_fields[i].id))
			continue;
		size += dist_fields[i].size;
	}

	return size;
}

 
int dpaa2_eth_cls_fld_off(int prot, int field)
{
	int i, off = 0;

	for (i = 0; i < ARRAY_SIZE(dist_fields); i++) {
		if (dist_fields[i].cls_prot == prot &&
		    dist_fields[i].cls_field == field)
			return off;
		off += dist_fields[i].size;
	}

	WARN_ONCE(1, "Unsupported header field used for Rx flow cls\n");
	return 0;
}

 
void dpaa2_eth_cls_trim_rule(void *key_mem, u64 fields)
{
	int off = 0, new_off = 0;
	int i, size;

	for (i = 0; i < ARRAY_SIZE(dist_fields); i++) {
		size = dist_fields[i].size;
		if (dist_fields[i].id & fields) {
			memcpy(key_mem + new_off, key_mem + off, size);
			new_off += size;
		}
		off += size;
	}
}

 
static int dpaa2_eth_set_dist_key(struct net_device *net_dev,
				  enum dpaa2_eth_rx_dist type, u64 flags)
{
	struct device *dev = net_dev->dev.parent;
	struct dpaa2_eth_priv *priv = netdev_priv(net_dev);
	struct dpkg_profile_cfg cls_cfg;
	u32 rx_hash_fields = 0;
	dma_addr_t key_iova;
	u8 *dma_mem;
	int i;
	int err = 0;

	memset(&cls_cfg, 0, sizeof(cls_cfg));

	for (i = 0; i < ARRAY_SIZE(dist_fields); i++) {
		struct dpkg_extract *key =
			&cls_cfg.extracts[cls_cfg.num_extracts];

		 
		if (!(flags & dist_fields[i].id))
			continue;
		if (type == DPAA2_ETH_RX_DIST_HASH)
			rx_hash_fields |= dist_fields[i].rxnfc_field;

		if (cls_cfg.num_extracts >= DPKG_MAX_NUM_OF_EXTRACTS) {
			dev_err(dev, "error adding key extraction rule, too many rules?\n");
			return -E2BIG;
		}

		key->type = DPKG_EXTRACT_FROM_HDR;
		key->extract.from_hdr.prot = dist_fields[i].cls_prot;
		key->extract.from_hdr.type = DPKG_FULL_FIELD;
		key->extract.from_hdr.field = dist_fields[i].cls_field;
		cls_cfg.num_extracts++;
	}

	dma_mem = kzalloc(DPAA2_CLASSIFIER_DMA_SIZE, GFP_KERNEL);
	if (!dma_mem)
		return -ENOMEM;

	err = dpni_prepare_key_cfg(&cls_cfg, dma_mem);
	if (err) {
		dev_err(dev, "dpni_prepare_key_cfg error %d\n", err);
		goto free_key;
	}

	 
	key_iova = dma_map_single(dev, dma_mem, DPAA2_CLASSIFIER_DMA_SIZE,
				  DMA_TO_DEVICE);
	if (dma_mapping_error(dev, key_iova)) {
		dev_err(dev, "DMA mapping failed\n");
		err = -ENOMEM;
		goto free_key;
	}

	if (type == DPAA2_ETH_RX_DIST_HASH) {
		if (dpaa2_eth_has_legacy_dist(priv))
			err = dpaa2_eth_config_legacy_hash_key(priv, key_iova);
		else
			err = dpaa2_eth_config_hash_key(priv, key_iova);
	} else {
		err = dpaa2_eth_config_cls_key(priv, key_iova);
	}

	dma_unmap_single(dev, key_iova, DPAA2_CLASSIFIER_DMA_SIZE,
			 DMA_TO_DEVICE);
	if (!err && type == DPAA2_ETH_RX_DIST_HASH)
		priv->rx_hash_fields = rx_hash_fields;

free_key:
	kfree(dma_mem);
	return err;
}

int dpaa2_eth_set_hash(struct net_device *net_dev, u64 flags)
{
	struct dpaa2_eth_priv *priv = netdev_priv(net_dev);
	u64 key = 0;
	int i;

	if (!dpaa2_eth_hash_enabled(priv))
		return -EOPNOTSUPP;

	for (i = 0; i < ARRAY_SIZE(dist_fields); i++)
		if (dist_fields[i].rxnfc_field & flags)
			key |= dist_fields[i].id;

	return dpaa2_eth_set_dist_key(net_dev, DPAA2_ETH_RX_DIST_HASH, key);
}

int dpaa2_eth_set_cls(struct net_device *net_dev, u64 flags)
{
	return dpaa2_eth_set_dist_key(net_dev, DPAA2_ETH_RX_DIST_CLS, flags);
}

static int dpaa2_eth_set_default_cls(struct dpaa2_eth_priv *priv)
{
	struct device *dev = priv->net_dev->dev.parent;
	int err;

	 
	if (dpaa2_eth_has_legacy_dist(priv)) {
		dev_dbg(dev, "Rx cls not supported by current MC version\n");
		return -EOPNOTSUPP;
	}

	if (!dpaa2_eth_fs_enabled(priv)) {
		dev_dbg(dev, "Rx cls disabled in DPNI options\n");
		return -EOPNOTSUPP;
	}

	if (!dpaa2_eth_hash_enabled(priv)) {
		dev_dbg(dev, "Rx cls disabled for single queue DPNIs\n");
		return -EOPNOTSUPP;
	}

	 
	if (!dpaa2_eth_fs_mask_enabled(priv))
		goto out;

	err = dpaa2_eth_set_cls(priv->net_dev, DPAA2_ETH_DIST_ALL);
	if (err)
		return err;

out:
	priv->rx_cls_enabled = 1;

	return 0;
}

 
static int dpaa2_eth_bind_dpni(struct dpaa2_eth_priv *priv)
{
	struct dpaa2_eth_bp *bp = priv->bp[DPAA2_ETH_DEFAULT_BP_IDX];
	struct net_device *net_dev = priv->net_dev;
	struct dpni_pools_cfg pools_params = { 0 };
	struct device *dev = net_dev->dev.parent;
	struct dpni_error_cfg err_cfg;
	int err = 0;
	int i;

	pools_params.num_dpbp = 1;
	pools_params.pools[0].dpbp_id = bp->dev->obj_desc.id;
	pools_params.pools[0].backup_pool = 0;
	pools_params.pools[0].buffer_size = priv->rx_buf_size;
	err = dpni_set_pools(priv->mc_io, 0, priv->mc_token, &pools_params);
	if (err) {
		dev_err(dev, "dpni_set_pools() failed\n");
		return err;
	}

	 
	err = dpaa2_eth_set_hash(net_dev, DPAA2_RXH_DEFAULT);
	if (err && err != -EOPNOTSUPP)
		dev_err(dev, "Failed to configure hashing\n");

	 
	err = dpaa2_eth_set_default_cls(priv);
	if (err && err != -EOPNOTSUPP)
		dev_err(dev, "Failed to configure Rx classification key\n");

	 
	err_cfg.errors = DPAA2_FAS_RX_ERR_MASK;
	err_cfg.set_frame_annotation = 1;
	err_cfg.error_action = DPNI_ERROR_ACTION_DISCARD;
	err = dpni_set_errors_behavior(priv->mc_io, 0, priv->mc_token,
				       &err_cfg);
	if (err) {
		dev_err(dev, "dpni_set_errors_behavior failed\n");
		return err;
	}

	 
	for (i = 0; i < priv->num_fqs; i++) {
		switch (priv->fq[i].type) {
		case DPAA2_RX_FQ:
			err = dpaa2_eth_setup_rx_flow(priv, &priv->fq[i]);
			break;
		case DPAA2_TX_CONF_FQ:
			err = dpaa2_eth_setup_tx_flow(priv, &priv->fq[i]);
			break;
		case DPAA2_RX_ERR_FQ:
			err = setup_rx_err_flow(priv, &priv->fq[i]);
			break;
		default:
			dev_err(dev, "Invalid FQ type %d\n", priv->fq[i].type);
			return -EINVAL;
		}
		if (err)
			return err;
	}

	err = dpni_get_qdid(priv->mc_io, 0, priv->mc_token,
			    DPNI_QUEUE_TX, &priv->tx_qdid);
	if (err) {
		dev_err(dev, "dpni_get_qdid() failed\n");
		return err;
	}

	return 0;
}

 
static int dpaa2_eth_alloc_rings(struct dpaa2_eth_priv *priv)
{
	struct net_device *net_dev = priv->net_dev;
	struct device *dev = net_dev->dev.parent;
	int i;

	for (i = 0; i < priv->num_channels; i++) {
		priv->channel[i]->store =
			dpaa2_io_store_create(DPAA2_ETH_STORE_SIZE, dev);
		if (!priv->channel[i]->store) {
			netdev_err(net_dev, "dpaa2_io_store_create() failed\n");
			goto err_ring;
		}
	}

	return 0;

err_ring:
	for (i = 0; i < priv->num_channels; i++) {
		if (!priv->channel[i]->store)
			break;
		dpaa2_io_store_destroy(priv->channel[i]->store);
	}

	return -ENOMEM;
}

static void dpaa2_eth_free_rings(struct dpaa2_eth_priv *priv)
{
	int i;

	for (i = 0; i < priv->num_channels; i++)
		dpaa2_io_store_destroy(priv->channel[i]->store);
}

static int dpaa2_eth_set_mac_addr(struct dpaa2_eth_priv *priv)
{
	struct net_device *net_dev = priv->net_dev;
	struct device *dev = net_dev->dev.parent;
	u8 mac_addr[ETH_ALEN], dpni_mac_addr[ETH_ALEN];
	int err;

	 
	err = dpni_get_port_mac_addr(priv->mc_io, 0, priv->mc_token, mac_addr);
	if (err) {
		dev_err(dev, "dpni_get_port_mac_addr() failed\n");
		return err;
	}

	 
	err = dpni_get_primary_mac_addr(priv->mc_io, 0, priv->mc_token,
					dpni_mac_addr);
	if (err) {
		dev_err(dev, "dpni_get_primary_mac_addr() failed\n");
		return err;
	}

	 
	if (!is_zero_ether_addr(mac_addr)) {
		 
		if (!ether_addr_equal(mac_addr, dpni_mac_addr)) {
			err = dpni_set_primary_mac_addr(priv->mc_io, 0,
							priv->mc_token,
							mac_addr);
			if (err) {
				dev_err(dev, "dpni_set_primary_mac_addr() failed\n");
				return err;
			}
		}
		eth_hw_addr_set(net_dev, mac_addr);
	} else if (is_zero_ether_addr(dpni_mac_addr)) {
		 
		eth_hw_addr_random(net_dev);
		dev_dbg_once(dev, "device(s) have all-zero hwaddr, replaced with random\n");

		err = dpni_set_primary_mac_addr(priv->mc_io, 0, priv->mc_token,
						net_dev->dev_addr);
		if (err) {
			dev_err(dev, "dpni_set_primary_mac_addr() failed\n");
			return err;
		}

		 
		net_dev->addr_assign_type = NET_ADDR_PERM;
	} else {
		 
		eth_hw_addr_set(net_dev, dpni_mac_addr);
	}

	return 0;
}

static int dpaa2_eth_netdev_init(struct net_device *net_dev)
{
	struct device *dev = net_dev->dev.parent;
	struct dpaa2_eth_priv *priv = netdev_priv(net_dev);
	u32 options = priv->dpni_attrs.options;
	u64 supported = 0, not_supported = 0;
	u8 bcast_addr[ETH_ALEN];
	u8 num_queues;
	int err;

	net_dev->netdev_ops = &dpaa2_eth_ops;
	net_dev->ethtool_ops = &dpaa2_ethtool_ops;

	err = dpaa2_eth_set_mac_addr(priv);
	if (err)
		return err;

	 
	eth_broadcast_addr(bcast_addr);
	err = dpni_add_mac_addr(priv->mc_io, 0, priv->mc_token, bcast_addr);
	if (err) {
		dev_err(dev, "dpni_add_mac_addr() failed\n");
		return err;
	}

	 
	net_dev->max_mtu = DPAA2_ETH_MAX_MTU;
	err = dpni_set_max_frame_length(priv->mc_io, 0, priv->mc_token,
					DPAA2_ETH_MFL);
	if (err) {
		dev_err(dev, "dpni_set_max_frame_length() failed\n");
		return err;
	}

	 
	num_queues = dpaa2_eth_queue_count(priv);
	err = netif_set_real_num_tx_queues(net_dev, num_queues);
	if (err) {
		dev_err(dev, "netif_set_real_num_tx_queues() failed\n");
		return err;
	}
	err = netif_set_real_num_rx_queues(net_dev, num_queues);
	if (err) {
		dev_err(dev, "netif_set_real_num_rx_queues() failed\n");
		return err;
	}

	dpaa2_eth_detect_features(priv);

	 
	supported |= IFF_LIVE_ADDR_CHANGE;

	if (options & DPNI_OPT_NO_MAC_FILTER)
		not_supported |= IFF_UNICAST_FLT;
	else
		supported |= IFF_UNICAST_FLT;

	net_dev->priv_flags |= supported;
	net_dev->priv_flags &= ~not_supported;

	 
	net_dev->features = NETIF_F_RXCSUM |
			    NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM |
			    NETIF_F_SG | NETIF_F_HIGHDMA |
			    NETIF_F_LLTX | NETIF_F_HW_TC | NETIF_F_TSO;
	net_dev->gso_max_segs = DPAA2_ETH_ENQUEUE_MAX_FDS;
	net_dev->hw_features = net_dev->features;
	net_dev->xdp_features = NETDEV_XDP_ACT_BASIC |
				NETDEV_XDP_ACT_REDIRECT |
				NETDEV_XDP_ACT_NDO_XMIT;
	if (priv->dpni_attrs.wriop_version >= DPAA2_WRIOP_VERSION(3, 0, 0) &&
	    priv->dpni_attrs.num_queues <= 8)
		net_dev->xdp_features |= NETDEV_XDP_ACT_XSK_ZEROCOPY;

	if (priv->dpni_attrs.vlan_filter_entries)
		net_dev->hw_features |= NETIF_F_HW_VLAN_CTAG_FILTER;

	return 0;
}

static int dpaa2_eth_poll_link_state(void *arg)
{
	struct dpaa2_eth_priv *priv = (struct dpaa2_eth_priv *)arg;
	int err;

	while (!kthread_should_stop()) {
		err = dpaa2_eth_link_state_update(priv);
		if (unlikely(err))
			return err;

		msleep(DPAA2_ETH_LINK_STATE_REFRESH);
	}

	return 0;
}

static int dpaa2_eth_connect_mac(struct dpaa2_eth_priv *priv)
{
	struct fsl_mc_device *dpni_dev, *dpmac_dev;
	struct dpaa2_mac *mac;
	int err;

	dpni_dev = to_fsl_mc_device(priv->net_dev->dev.parent);
	dpmac_dev = fsl_mc_get_endpoint(dpni_dev, 0);

	if (PTR_ERR(dpmac_dev) == -EPROBE_DEFER) {
		netdev_dbg(priv->net_dev, "waiting for mac\n");
		return PTR_ERR(dpmac_dev);
	}

	if (IS_ERR(dpmac_dev) || dpmac_dev->dev.type != &fsl_mc_bus_dpmac_type)
		return 0;

	mac = kzalloc(sizeof(struct dpaa2_mac), GFP_KERNEL);
	if (!mac)
		return -ENOMEM;

	mac->mc_dev = dpmac_dev;
	mac->mc_io = priv->mc_io;
	mac->net_dev = priv->net_dev;

	err = dpaa2_mac_open(mac);
	if (err)
		goto err_free_mac;

	if (dpaa2_mac_is_type_phy(mac)) {
		err = dpaa2_mac_connect(mac);
		if (err) {
			if (err == -EPROBE_DEFER)
				netdev_dbg(priv->net_dev,
					   "could not connect to MAC\n");
			else
				netdev_err(priv->net_dev,
					   "Error connecting to the MAC endpoint: %pe",
					   ERR_PTR(err));
			goto err_close_mac;
		}
	}

	mutex_lock(&priv->mac_lock);
	priv->mac = mac;
	mutex_unlock(&priv->mac_lock);

	return 0;

err_close_mac:
	dpaa2_mac_close(mac);
err_free_mac:
	kfree(mac);
	return err;
}

static void dpaa2_eth_disconnect_mac(struct dpaa2_eth_priv *priv)
{
	struct dpaa2_mac *mac;

	mutex_lock(&priv->mac_lock);
	mac = priv->mac;
	priv->mac = NULL;
	mutex_unlock(&priv->mac_lock);

	if (!mac)
		return;

	if (dpaa2_mac_is_type_phy(mac))
		dpaa2_mac_disconnect(mac);

	dpaa2_mac_close(mac);
	kfree(mac);
}

static irqreturn_t dpni_irq0_handler_thread(int irq_num, void *arg)
{
	u32 status = ~0;
	struct device *dev = (struct device *)arg;
	struct fsl_mc_device *dpni_dev = to_fsl_mc_device(dev);
	struct net_device *net_dev = dev_get_drvdata(dev);
	struct dpaa2_eth_priv *priv = netdev_priv(net_dev);
	bool had_mac;
	int err;

	err = dpni_get_irq_status(dpni_dev->mc_io, 0, dpni_dev->mc_handle,
				  DPNI_IRQ_INDEX, &status);
	if (unlikely(err)) {
		netdev_err(net_dev, "Can't get irq status (err %d)\n", err);
		return IRQ_HANDLED;
	}

	if (status & DPNI_IRQ_EVENT_LINK_CHANGED)
		dpaa2_eth_link_state_update(netdev_priv(net_dev));

	if (status & DPNI_IRQ_EVENT_ENDPOINT_CHANGED) {
		dpaa2_eth_set_mac_addr(netdev_priv(net_dev));
		dpaa2_eth_update_tx_fqids(priv);

		 
		had_mac = !!priv->mac;
		if (had_mac)
			dpaa2_eth_disconnect_mac(priv);
		else
			dpaa2_eth_connect_mac(priv);
	}

	return IRQ_HANDLED;
}

static int dpaa2_eth_setup_irqs(struct fsl_mc_device *ls_dev)
{
	int err = 0;
	struct fsl_mc_device_irq *irq;

	err = fsl_mc_allocate_irqs(ls_dev);
	if (err) {
		dev_err(&ls_dev->dev, "MC irqs allocation failed\n");
		return err;
	}

	irq = ls_dev->irqs[0];
	err = devm_request_threaded_irq(&ls_dev->dev, irq->virq,
					NULL, dpni_irq0_handler_thread,
					IRQF_NO_SUSPEND | IRQF_ONESHOT,
					dev_name(&ls_dev->dev), &ls_dev->dev);
	if (err < 0) {
		dev_err(&ls_dev->dev, "devm_request_threaded_irq(): %d\n", err);
		goto free_mc_irq;
	}

	err = dpni_set_irq_mask(ls_dev->mc_io, 0, ls_dev->mc_handle,
				DPNI_IRQ_INDEX, DPNI_IRQ_EVENT_LINK_CHANGED |
				DPNI_IRQ_EVENT_ENDPOINT_CHANGED);
	if (err < 0) {
		dev_err(&ls_dev->dev, "dpni_set_irq_mask(): %d\n", err);
		goto free_irq;
	}

	err = dpni_set_irq_enable(ls_dev->mc_io, 0, ls_dev->mc_handle,
				  DPNI_IRQ_INDEX, 1);
	if (err < 0) {
		dev_err(&ls_dev->dev, "dpni_set_irq_enable(): %d\n", err);
		goto free_irq;
	}

	return 0;

free_irq:
	devm_free_irq(&ls_dev->dev, irq->virq, &ls_dev->dev);
free_mc_irq:
	fsl_mc_free_irqs(ls_dev);

	return err;
}

static void dpaa2_eth_add_ch_napi(struct dpaa2_eth_priv *priv)
{
	int i;
	struct dpaa2_eth_channel *ch;

	for (i = 0; i < priv->num_channels; i++) {
		ch = priv->channel[i];
		 
		netif_napi_add(priv->net_dev, &ch->napi, dpaa2_eth_poll);
	}
}

static void dpaa2_eth_del_ch_napi(struct dpaa2_eth_priv *priv)
{
	int i;
	struct dpaa2_eth_channel *ch;

	for (i = 0; i < priv->num_channels; i++) {
		ch = priv->channel[i];
		netif_napi_del(&ch->napi);
	}
}

static int dpaa2_eth_probe(struct fsl_mc_device *dpni_dev)
{
	struct device *dev;
	struct net_device *net_dev = NULL;
	struct dpaa2_eth_priv *priv = NULL;
	int err = 0;

	dev = &dpni_dev->dev;

	 
	net_dev = alloc_etherdev_mq(sizeof(*priv), DPAA2_ETH_MAX_NETDEV_QUEUES);
	if (!net_dev) {
		dev_err(dev, "alloc_etherdev_mq() failed\n");
		return -ENOMEM;
	}

	SET_NETDEV_DEV(net_dev, dev);
	dev_set_drvdata(dev, net_dev);

	priv = netdev_priv(net_dev);
	priv->net_dev = net_dev;
	SET_NETDEV_DEVLINK_PORT(net_dev, &priv->devlink_port);

	mutex_init(&priv->mac_lock);

	priv->iommu_domain = iommu_get_domain_for_dev(dev);

	priv->tx_tstamp_type = HWTSTAMP_TX_OFF;
	priv->rx_tstamp = false;

	priv->dpaa2_ptp_wq = alloc_workqueue("dpaa2_ptp_wq", 0, 0);
	if (!priv->dpaa2_ptp_wq) {
		err = -ENOMEM;
		goto err_wq_alloc;
	}

	INIT_WORK(&priv->tx_onestep_tstamp, dpaa2_eth_tx_onestep_tstamp);
	mutex_init(&priv->onestep_tstamp_lock);
	skb_queue_head_init(&priv->tx_skbs);

	priv->rx_copybreak = DPAA2_ETH_DEFAULT_COPYBREAK;

	 
	err = fsl_mc_portal_allocate(dpni_dev, FSL_MC_IO_ATOMIC_CONTEXT_PORTAL,
				     &priv->mc_io);
	if (err) {
		if (err == -ENXIO) {
			dev_dbg(dev, "waiting for MC portal\n");
			err = -EPROBE_DEFER;
		} else {
			dev_err(dev, "MC portal allocation failed\n");
		}
		goto err_portal_alloc;
	}

	 
	err = dpaa2_eth_setup_dpni(dpni_dev);
	if (err)
		goto err_dpni_setup;

	err = dpaa2_eth_setup_dpio(priv);
	if (err)
		goto err_dpio_setup;

	dpaa2_eth_setup_fqs(priv);

	err = dpaa2_eth_setup_default_dpbp(priv);
	if (err)
		goto err_dpbp_setup;

	err = dpaa2_eth_bind_dpni(priv);
	if (err)
		goto err_bind;

	 
	dpaa2_eth_add_ch_napi(priv);

	 
	priv->percpu_stats = alloc_percpu(*priv->percpu_stats);
	if (!priv->percpu_stats) {
		dev_err(dev, "alloc_percpu(percpu_stats) failed\n");
		err = -ENOMEM;
		goto err_alloc_percpu_stats;
	}
	priv->percpu_extras = alloc_percpu(*priv->percpu_extras);
	if (!priv->percpu_extras) {
		dev_err(dev, "alloc_percpu(percpu_extras) failed\n");
		err = -ENOMEM;
		goto err_alloc_percpu_extras;
	}

	priv->sgt_cache = alloc_percpu(*priv->sgt_cache);
	if (!priv->sgt_cache) {
		dev_err(dev, "alloc_percpu(sgt_cache) failed\n");
		err = -ENOMEM;
		goto err_alloc_sgt_cache;
	}

	priv->fd = alloc_percpu(*priv->fd);
	if (!priv->fd) {
		dev_err(dev, "alloc_percpu(fds) failed\n");
		err = -ENOMEM;
		goto err_alloc_fds;
	}

	err = dpaa2_eth_netdev_init(net_dev);
	if (err)
		goto err_netdev_init;

	 
	err = dpaa2_eth_set_rx_csum(priv, !!(net_dev->features & NETIF_F_RXCSUM));
	if (err)
		goto err_csum;

	err = dpaa2_eth_set_tx_csum(priv,
				    !!(net_dev->features & (NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM)));
	if (err)
		goto err_csum;

	err = dpaa2_eth_alloc_rings(priv);
	if (err)
		goto err_alloc_rings;

#ifdef CONFIG_FSL_DPAA2_ETH_DCB
	if (dpaa2_eth_has_pause_support(priv) && priv->vlan_cls_enabled) {
		priv->dcbx_mode = DCB_CAP_DCBX_HOST | DCB_CAP_DCBX_VER_IEEE;
		net_dev->dcbnl_ops = &dpaa2_eth_dcbnl_ops;
	} else {
		dev_dbg(dev, "PFC not supported\n");
	}
#endif

	err = dpaa2_eth_connect_mac(priv);
	if (err)
		goto err_connect_mac;

	err = dpaa2_eth_setup_irqs(dpni_dev);
	if (err) {
		netdev_warn(net_dev, "Failed to set link interrupt, fall back to polling\n");
		priv->poll_thread = kthread_run(dpaa2_eth_poll_link_state, priv,
						"%s_poll_link", net_dev->name);
		if (IS_ERR(priv->poll_thread)) {
			dev_err(dev, "Error starting polling thread\n");
			goto err_poll_thread;
		}
		priv->do_link_poll = true;
	}

	err = dpaa2_eth_dl_alloc(priv);
	if (err)
		goto err_dl_register;

	err = dpaa2_eth_dl_traps_register(priv);
	if (err)
		goto err_dl_trap_register;

	err = dpaa2_eth_dl_port_add(priv);
	if (err)
		goto err_dl_port_add;

	net_dev->needed_headroom = DPAA2_ETH_SWA_SIZE + DPAA2_ETH_TX_BUF_ALIGN;

	err = register_netdev(net_dev);
	if (err < 0) {
		dev_err(dev, "register_netdev() failed\n");
		goto err_netdev_reg;
	}

#ifdef CONFIG_DEBUG_FS
	dpaa2_dbg_add(priv);
#endif

	dpaa2_eth_dl_register(priv);
	dev_info(dev, "Probed interface %s\n", net_dev->name);
	return 0;

err_netdev_reg:
	dpaa2_eth_dl_port_del(priv);
err_dl_port_add:
	dpaa2_eth_dl_traps_unregister(priv);
err_dl_trap_register:
	dpaa2_eth_dl_free(priv);
err_dl_register:
	if (priv->do_link_poll)
		kthread_stop(priv->poll_thread);
	else
		fsl_mc_free_irqs(dpni_dev);
err_poll_thread:
	dpaa2_eth_disconnect_mac(priv);
err_connect_mac:
	dpaa2_eth_free_rings(priv);
err_alloc_rings:
err_csum:
err_netdev_init:
	free_percpu(priv->fd);
err_alloc_fds:
	free_percpu(priv->sgt_cache);
err_alloc_sgt_cache:
	free_percpu(priv->percpu_extras);
err_alloc_percpu_extras:
	free_percpu(priv->percpu_stats);
err_alloc_percpu_stats:
	dpaa2_eth_del_ch_napi(priv);
err_bind:
	dpaa2_eth_free_dpbps(priv);
err_dpbp_setup:
	dpaa2_eth_free_dpio(priv);
err_dpio_setup:
	dpaa2_eth_free_dpni(priv);
err_dpni_setup:
	fsl_mc_portal_free(priv->mc_io);
err_portal_alloc:
	destroy_workqueue(priv->dpaa2_ptp_wq);
err_wq_alloc:
	dev_set_drvdata(dev, NULL);
	free_netdev(net_dev);

	return err;
}

static void dpaa2_eth_remove(struct fsl_mc_device *ls_dev)
{
	struct device *dev;
	struct net_device *net_dev;
	struct dpaa2_eth_priv *priv;

	dev = &ls_dev->dev;
	net_dev = dev_get_drvdata(dev);
	priv = netdev_priv(net_dev);

	dpaa2_eth_dl_unregister(priv);

#ifdef CONFIG_DEBUG_FS
	dpaa2_dbg_remove(priv);
#endif

	unregister_netdev(net_dev);

	dpaa2_eth_dl_port_del(priv);
	dpaa2_eth_dl_traps_unregister(priv);
	dpaa2_eth_dl_free(priv);

	if (priv->do_link_poll)
		kthread_stop(priv->poll_thread);
	else
		fsl_mc_free_irqs(ls_dev);

	dpaa2_eth_disconnect_mac(priv);
	dpaa2_eth_free_rings(priv);
	free_percpu(priv->fd);
	free_percpu(priv->sgt_cache);
	free_percpu(priv->percpu_stats);
	free_percpu(priv->percpu_extras);

	dpaa2_eth_del_ch_napi(priv);
	dpaa2_eth_free_dpbps(priv);
	dpaa2_eth_free_dpio(priv);
	dpaa2_eth_free_dpni(priv);
	if (priv->onestep_reg_base)
		iounmap(priv->onestep_reg_base);

	fsl_mc_portal_free(priv->mc_io);

	destroy_workqueue(priv->dpaa2_ptp_wq);

	dev_dbg(net_dev->dev.parent, "Removed interface %s\n", net_dev->name);

	free_netdev(net_dev);
}

static const struct fsl_mc_device_id dpaa2_eth_match_id_table[] = {
	{
		.vendor = FSL_MC_VENDOR_FREESCALE,
		.obj_type = "dpni",
	},
	{ .vendor = 0x0 }
};
MODULE_DEVICE_TABLE(fslmc, dpaa2_eth_match_id_table);

static struct fsl_mc_driver dpaa2_eth_driver = {
	.driver = {
		.name = KBUILD_MODNAME,
	},
	.probe = dpaa2_eth_probe,
	.remove = dpaa2_eth_remove,
	.match_id_table = dpaa2_eth_match_id_table
};

static int __init dpaa2_eth_driver_init(void)
{
	int err;

	dpaa2_eth_dbg_init();
	err = fsl_mc_driver_register(&dpaa2_eth_driver);
	if (err) {
		dpaa2_eth_dbg_exit();
		return err;
	}

	return 0;
}

static void __exit dpaa2_eth_driver_exit(void)
{
	dpaa2_eth_dbg_exit();
	fsl_mc_driver_unregister(&dpaa2_eth_driver);
}

module_init(dpaa2_eth_driver_init);
module_exit(dpaa2_eth_driver_exit);
