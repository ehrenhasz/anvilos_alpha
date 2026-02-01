
 
#include "osdep.h"
#include "hmc.h"
#include "defs.h"
#include "type.h"
#include "protos.h"
#include "puda.h"
#include "ws.h"

static void irdma_ieq_receive(struct irdma_sc_vsi *vsi,
			      struct irdma_puda_buf *buf);
static void irdma_ieq_tx_compl(struct irdma_sc_vsi *vsi, void *sqwrid);
static void irdma_ilq_putback_rcvbuf(struct irdma_sc_qp *qp,
				     struct irdma_puda_buf *buf, u32 wqe_idx);
 
static struct irdma_puda_buf *irdma_puda_get_listbuf(struct list_head *list)
{
	struct irdma_puda_buf *buf = NULL;

	if (!list_empty(list)) {
		buf = (struct irdma_puda_buf *)list->next;
		list_del((struct list_head *)&buf->list);
	}

	return buf;
}

 
struct irdma_puda_buf *irdma_puda_get_bufpool(struct irdma_puda_rsrc *rsrc)
{
	struct irdma_puda_buf *buf = NULL;
	struct list_head *list = &rsrc->bufpool;
	unsigned long flags;

	spin_lock_irqsave(&rsrc->bufpool_lock, flags);
	buf = irdma_puda_get_listbuf(list);
	if (buf) {
		rsrc->avail_buf_count--;
		buf->vsi = rsrc->vsi;
	} else {
		rsrc->stats_buf_alloc_fail++;
	}
	spin_unlock_irqrestore(&rsrc->bufpool_lock, flags);

	return buf;
}

 
void irdma_puda_ret_bufpool(struct irdma_puda_rsrc *rsrc,
			    struct irdma_puda_buf *buf)
{
	unsigned long flags;

	buf->do_lpb = false;
	spin_lock_irqsave(&rsrc->bufpool_lock, flags);
	list_add(&buf->list, &rsrc->bufpool);
	spin_unlock_irqrestore(&rsrc->bufpool_lock, flags);
	rsrc->avail_buf_count++;
}

 
static void irdma_puda_post_recvbuf(struct irdma_puda_rsrc *rsrc, u32 wqe_idx,
				    struct irdma_puda_buf *buf, bool initial)
{
	__le64 *wqe;
	struct irdma_sc_qp *qp = &rsrc->qp;
	u64 offset24 = 0;

	 
	dma_sync_single_for_device(rsrc->dev->hw->device, buf->mem.pa,
				   buf->mem.size, DMA_BIDIRECTIONAL);
	qp->qp_uk.rq_wrid_array[wqe_idx] = (uintptr_t)buf;
	wqe = qp->qp_uk.rq_base[wqe_idx].elem;
	if (!initial)
		get_64bit_val(wqe, 24, &offset24);

	offset24 = (offset24) ? 0 : FIELD_PREP(IRDMAQPSQ_VALID, 1);

	set_64bit_val(wqe, 16, 0);
	set_64bit_val(wqe, 0, buf->mem.pa);
	if (qp->qp_uk.uk_attrs->hw_rev == IRDMA_GEN_1) {
		set_64bit_val(wqe, 8,
			      FIELD_PREP(IRDMAQPSQ_GEN1_FRAG_LEN, buf->mem.size));
	} else {
		set_64bit_val(wqe, 8,
			      FIELD_PREP(IRDMAQPSQ_FRAG_LEN, buf->mem.size) |
			      offset24);
	}
	dma_wmb();  

	set_64bit_val(wqe, 24, offset24);
}

 
static int irdma_puda_replenish_rq(struct irdma_puda_rsrc *rsrc, bool initial)
{
	u32 i;
	u32 invalid_cnt = rsrc->rxq_invalid_cnt;
	struct irdma_puda_buf *buf = NULL;

	for (i = 0; i < invalid_cnt; i++) {
		buf = irdma_puda_get_bufpool(rsrc);
		if (!buf)
			return -ENOBUFS;
		irdma_puda_post_recvbuf(rsrc, rsrc->rx_wqe_idx, buf, initial);
		rsrc->rx_wqe_idx = ((rsrc->rx_wqe_idx + 1) % rsrc->rq_size);
		rsrc->rxq_invalid_cnt--;
	}

	return 0;
}

 
static struct irdma_puda_buf *irdma_puda_alloc_buf(struct irdma_sc_dev *dev,
						   u32 len)
{
	struct irdma_puda_buf *buf;
	struct irdma_virt_mem buf_mem;

	buf_mem.size = sizeof(struct irdma_puda_buf);
	buf_mem.va = kzalloc(buf_mem.size, GFP_KERNEL);
	if (!buf_mem.va)
		return NULL;

	buf = buf_mem.va;
	buf->mem.size = len;
	buf->mem.va = kzalloc(buf->mem.size, GFP_KERNEL);
	if (!buf->mem.va)
		goto free_virt;
	buf->mem.pa = dma_map_single(dev->hw->device, buf->mem.va,
				     buf->mem.size, DMA_BIDIRECTIONAL);
	if (dma_mapping_error(dev->hw->device, buf->mem.pa)) {
		kfree(buf->mem.va);
		goto free_virt;
	}

	buf->buf_mem.va = buf_mem.va;
	buf->buf_mem.size = buf_mem.size;

	return buf;

free_virt:
	kfree(buf_mem.va);
	return NULL;
}

 
static void irdma_puda_dele_buf(struct irdma_sc_dev *dev,
				struct irdma_puda_buf *buf)
{
	dma_unmap_single(dev->hw->device, buf->mem.pa, buf->mem.size,
			 DMA_BIDIRECTIONAL);
	kfree(buf->mem.va);
	kfree(buf->buf_mem.va);
}

 
static __le64 *irdma_puda_get_next_send_wqe(struct irdma_qp_uk *qp,
					    u32 *wqe_idx)
{
	int ret_code = 0;

	*wqe_idx = IRDMA_RING_CURRENT_HEAD(qp->sq_ring);
	if (!*wqe_idx)
		qp->swqe_polarity = !qp->swqe_polarity;
	IRDMA_RING_MOVE_HEAD(qp->sq_ring, ret_code);
	if (ret_code)
		return NULL;

	return qp->sq_base[*wqe_idx].elem;
}

 
static int irdma_puda_poll_info(struct irdma_sc_cq *cq,
				struct irdma_puda_cmpl_info *info)
{
	struct irdma_cq_uk *cq_uk = &cq->cq_uk;
	u64 qword0, qword2, qword3, qword6;
	__le64 *cqe;
	__le64 *ext_cqe = NULL;
	u64 qword7 = 0;
	u64 comp_ctx;
	bool valid_bit;
	bool ext_valid = 0;
	u32 major_err, minor_err;
	u32 peek_head;
	bool error;
	u8 polarity;

	cqe = IRDMA_GET_CURRENT_CQ_ELEM(&cq->cq_uk);
	get_64bit_val(cqe, 24, &qword3);
	valid_bit = (bool)FIELD_GET(IRDMA_CQ_VALID, qword3);
	if (valid_bit != cq_uk->polarity)
		return -ENOENT;

	 
	dma_rmb();

	if (cq->dev->hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_2)
		ext_valid = (bool)FIELD_GET(IRDMA_CQ_EXTCQE, qword3);

	if (ext_valid) {
		peek_head = (cq_uk->cq_ring.head + 1) % cq_uk->cq_ring.size;
		ext_cqe = cq_uk->cq_base[peek_head].buf;
		get_64bit_val(ext_cqe, 24, &qword7);
		polarity = (u8)FIELD_GET(IRDMA_CQ_VALID, qword7);
		if (!peek_head)
			polarity ^= 1;
		if (polarity != cq_uk->polarity)
			return -ENOENT;

		 
		dma_rmb();

		IRDMA_RING_MOVE_HEAD_NOCHECK(cq_uk->cq_ring);
		if (!IRDMA_RING_CURRENT_HEAD(cq_uk->cq_ring))
			cq_uk->polarity = !cq_uk->polarity;
		 
		IRDMA_RING_MOVE_TAIL(cq_uk->cq_ring);
	}

	print_hex_dump_debug("PUDA: PUDA CQE", DUMP_PREFIX_OFFSET, 16, 8, cqe,
			     32, false);
	if (ext_valid)
		print_hex_dump_debug("PUDA: PUDA EXT-CQE", DUMP_PREFIX_OFFSET,
				     16, 8, ext_cqe, 32, false);

	error = (bool)FIELD_GET(IRDMA_CQ_ERROR, qword3);
	if (error) {
		ibdev_dbg(to_ibdev(cq->dev), "PUDA: receive error\n");
		major_err = (u32)(FIELD_GET(IRDMA_CQ_MAJERR, qword3));
		minor_err = (u32)(FIELD_GET(IRDMA_CQ_MINERR, qword3));
		info->compl_error = major_err << 16 | minor_err;
		return -EIO;
	}

	get_64bit_val(cqe, 0, &qword0);
	get_64bit_val(cqe, 16, &qword2);

	info->q_type = (u8)FIELD_GET(IRDMA_CQ_SQ, qword3);
	info->qp_id = (u32)FIELD_GET(IRDMACQ_QPID, qword2);
	if (cq->dev->hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_2)
		info->ipv4 = (bool)FIELD_GET(IRDMACQ_IPV4, qword3);

	get_64bit_val(cqe, 8, &comp_ctx);
	info->qp = (struct irdma_qp_uk *)(unsigned long)comp_ctx;
	info->wqe_idx = (u32)FIELD_GET(IRDMA_CQ_WQEIDX, qword3);

	if (info->q_type == IRDMA_CQE_QTYPE_RQ) {
		if (ext_valid) {
			info->vlan_valid = (bool)FIELD_GET(IRDMA_CQ_UDVLANVALID, qword7);
			if (info->vlan_valid) {
				get_64bit_val(ext_cqe, 16, &qword6);
				info->vlan = (u16)FIELD_GET(IRDMA_CQ_UDVLAN, qword6);
			}
			info->smac_valid = (bool)FIELD_GET(IRDMA_CQ_UDSMACVALID, qword7);
			if (info->smac_valid) {
				get_64bit_val(ext_cqe, 16, &qword6);
				info->smac[0] = (u8)((qword6 >> 40) & 0xFF);
				info->smac[1] = (u8)((qword6 >> 32) & 0xFF);
				info->smac[2] = (u8)((qword6 >> 24) & 0xFF);
				info->smac[3] = (u8)((qword6 >> 16) & 0xFF);
				info->smac[4] = (u8)((qword6 >> 8) & 0xFF);
				info->smac[5] = (u8)(qword6 & 0xFF);
			}
		}

		if (cq->dev->hw_attrs.uk_attrs.hw_rev == IRDMA_GEN_1) {
			info->vlan_valid = (bool)FIELD_GET(IRDMA_VLAN_TAG_VALID, qword3);
			info->l4proto = (u8)FIELD_GET(IRDMA_UDA_L4PROTO, qword2);
			info->l3proto = (u8)FIELD_GET(IRDMA_UDA_L3PROTO, qword2);
		}

		info->payload_len = (u32)FIELD_GET(IRDMACQ_PAYLDLEN, qword0);
	}

	return 0;
}

 
int irdma_puda_poll_cmpl(struct irdma_sc_dev *dev, struct irdma_sc_cq *cq,
			 u32 *compl_err)
{
	struct irdma_qp_uk *qp;
	struct irdma_cq_uk *cq_uk = &cq->cq_uk;
	struct irdma_puda_cmpl_info info = {};
	int ret = 0;
	struct irdma_puda_buf *buf;
	struct irdma_puda_rsrc *rsrc;
	u8 cq_type = cq->cq_type;
	unsigned long flags;

	if (cq_type == IRDMA_CQ_TYPE_ILQ || cq_type == IRDMA_CQ_TYPE_IEQ) {
		rsrc = (cq_type == IRDMA_CQ_TYPE_ILQ) ? cq->vsi->ilq :
							cq->vsi->ieq;
	} else {
		ibdev_dbg(to_ibdev(dev), "PUDA: qp_type error\n");
		return -EINVAL;
	}

	ret = irdma_puda_poll_info(cq, &info);
	*compl_err = info.compl_error;
	if (ret == -ENOENT)
		return ret;
	if (ret)
		goto done;

	qp = info.qp;
	if (!qp || !rsrc) {
		ret = -EFAULT;
		goto done;
	}

	if (qp->qp_id != rsrc->qp_id) {
		ret = -EFAULT;
		goto done;
	}

	if (info.q_type == IRDMA_CQE_QTYPE_RQ) {
		buf = (struct irdma_puda_buf *)(uintptr_t)
			      qp->rq_wrid_array[info.wqe_idx];

		 
		dma_sync_single_for_cpu(dev->hw->device, buf->mem.pa,
					buf->mem.size, DMA_BIDIRECTIONAL);
		 
		ret = irdma_puda_get_tcpip_info(&info, buf);
		if (ret) {
			rsrc->stats_rcvd_pkt_err++;
			if (cq_type == IRDMA_CQ_TYPE_ILQ) {
				irdma_ilq_putback_rcvbuf(&rsrc->qp, buf,
							 info.wqe_idx);
			} else {
				irdma_puda_ret_bufpool(rsrc, buf);
				irdma_puda_replenish_rq(rsrc, false);
			}
			goto done;
		}

		rsrc->stats_pkt_rcvd++;
		rsrc->compl_rxwqe_idx = info.wqe_idx;
		ibdev_dbg(to_ibdev(dev), "PUDA: RQ completion\n");
		rsrc->receive(rsrc->vsi, buf);
		if (cq_type == IRDMA_CQ_TYPE_ILQ)
			irdma_ilq_putback_rcvbuf(&rsrc->qp, buf, info.wqe_idx);
		else
			irdma_puda_replenish_rq(rsrc, false);

	} else {
		ibdev_dbg(to_ibdev(dev), "PUDA: SQ completion\n");
		buf = (struct irdma_puda_buf *)(uintptr_t)
					qp->sq_wrtrk_array[info.wqe_idx].wrid;

		 
		dma_sync_single_for_cpu(dev->hw->device, buf->mem.pa,
					buf->mem.size, DMA_BIDIRECTIONAL);
		IRDMA_RING_SET_TAIL(qp->sq_ring, info.wqe_idx);
		rsrc->xmit_complete(rsrc->vsi, buf);
		spin_lock_irqsave(&rsrc->bufpool_lock, flags);
		rsrc->tx_wqe_avail_cnt++;
		spin_unlock_irqrestore(&rsrc->bufpool_lock, flags);
		if (!list_empty(&rsrc->txpend))
			irdma_puda_send_buf(rsrc, NULL);
	}

done:
	IRDMA_RING_MOVE_HEAD_NOCHECK(cq_uk->cq_ring);
	if (!IRDMA_RING_CURRENT_HEAD(cq_uk->cq_ring))
		cq_uk->polarity = !cq_uk->polarity;
	 
	IRDMA_RING_MOVE_TAIL(cq_uk->cq_ring);
	set_64bit_val(cq_uk->shadow_area, 0,
		      IRDMA_RING_CURRENT_HEAD(cq_uk->cq_ring));

	return ret;
}

 
int irdma_puda_send(struct irdma_sc_qp *qp, struct irdma_puda_send_info *info)
{
	__le64 *wqe;
	u32 iplen, l4len;
	u64 hdr[2];
	u32 wqe_idx;
	u8 iipt;

	 
	l4len = info->tcplen >> 2;
	if (info->ipv4) {
		iipt = 3;
		iplen = 5;
	} else {
		iipt = 1;
		iplen = 10;
	}

	wqe = irdma_puda_get_next_send_wqe(&qp->qp_uk, &wqe_idx);
	if (!wqe)
		return -ENOMEM;

	qp->qp_uk.sq_wrtrk_array[wqe_idx].wrid = (uintptr_t)info->scratch;
	 
	 

	if (qp->dev->hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_2) {
		hdr[0] = 0;  
		hdr[1] = FIELD_PREP(IRDMA_UDA_QPSQ_OPCODE, IRDMA_OP_TYPE_SEND) |
			 FIELD_PREP(IRDMA_UDA_QPSQ_L4LEN, l4len) |
			 FIELD_PREP(IRDMAQPSQ_AHID, info->ah_id) |
			 FIELD_PREP(IRDMA_UDA_QPSQ_SIGCOMPL, 1) |
			 FIELD_PREP(IRDMA_UDA_QPSQ_VALID,
				    qp->qp_uk.swqe_polarity);

		 

		set_64bit_val(wqe, 0, info->paddr);
		set_64bit_val(wqe, 8,
			      FIELD_PREP(IRDMAQPSQ_FRAG_LEN, info->len) |
			      FIELD_PREP(IRDMA_UDA_QPSQ_VALID, qp->qp_uk.swqe_polarity));
	} else {
		hdr[0] = FIELD_PREP(IRDMA_UDA_QPSQ_MACLEN, info->maclen >> 1) |
			 FIELD_PREP(IRDMA_UDA_QPSQ_IPLEN, iplen) |
			 FIELD_PREP(IRDMA_UDA_QPSQ_L4T, 1) |
			 FIELD_PREP(IRDMA_UDA_QPSQ_IIPT, iipt) |
			 FIELD_PREP(IRDMA_GEN1_UDA_QPSQ_L4LEN, l4len);

		hdr[1] = FIELD_PREP(IRDMA_UDA_QPSQ_OPCODE, IRDMA_OP_TYPE_SEND) |
			 FIELD_PREP(IRDMA_UDA_QPSQ_SIGCOMPL, 1) |
			 FIELD_PREP(IRDMA_UDA_QPSQ_DOLOOPBACK, info->do_lpb) |
			 FIELD_PREP(IRDMA_UDA_QPSQ_VALID, qp->qp_uk.swqe_polarity);

		 

		set_64bit_val(wqe, 0, info->paddr);
		set_64bit_val(wqe, 8,
			      FIELD_PREP(IRDMAQPSQ_GEN1_FRAG_LEN, info->len));
	}

	set_64bit_val(wqe, 16, hdr[0]);
	dma_wmb();  

	set_64bit_val(wqe, 24, hdr[1]);

	print_hex_dump_debug("PUDA: PUDA SEND WQE", DUMP_PREFIX_OFFSET, 16, 8,
			     wqe, 32, false);
	irdma_uk_qp_post_wr(&qp->qp_uk);
	return 0;
}

 
void irdma_puda_send_buf(struct irdma_puda_rsrc *rsrc,
			 struct irdma_puda_buf *buf)
{
	struct irdma_puda_send_info info;
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&rsrc->bufpool_lock, flags);
	 
	if (!rsrc->tx_wqe_avail_cnt || (buf && !list_empty(&rsrc->txpend))) {
		list_add_tail(&buf->list, &rsrc->txpend);
		spin_unlock_irqrestore(&rsrc->bufpool_lock, flags);
		rsrc->stats_sent_pkt_q++;
		if (rsrc->type == IRDMA_PUDA_RSRC_TYPE_ILQ)
			ibdev_dbg(to_ibdev(rsrc->dev),
				  "PUDA: adding to txpend\n");
		return;
	}
	rsrc->tx_wqe_avail_cnt--;
	 
	if (!buf) {
		buf = irdma_puda_get_listbuf(&rsrc->txpend);
		if (!buf)
			goto done;
	}

	info.scratch = buf;
	info.paddr = buf->mem.pa;
	info.len = buf->totallen;
	info.tcplen = buf->tcphlen;
	info.ipv4 = buf->ipv4;

	if (rsrc->dev->hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_2) {
		info.ah_id = buf->ah_id;
	} else {
		info.maclen = buf->maclen;
		info.do_lpb = buf->do_lpb;
	}

	 
	dma_sync_single_for_cpu(rsrc->dev->hw->device, buf->mem.pa,
				buf->mem.size, DMA_BIDIRECTIONAL);
	ret = irdma_puda_send(&rsrc->qp, &info);
	if (ret) {
		rsrc->tx_wqe_avail_cnt++;
		rsrc->stats_sent_pkt_q++;
		list_add(&buf->list, &rsrc->txpend);
		if (rsrc->type == IRDMA_PUDA_RSRC_TYPE_ILQ)
			ibdev_dbg(to_ibdev(rsrc->dev),
				  "PUDA: adding to puda_send\n");
	} else {
		rsrc->stats_pkt_sent++;
	}
done:
	spin_unlock_irqrestore(&rsrc->bufpool_lock, flags);
}

 
static void irdma_puda_qp_setctx(struct irdma_puda_rsrc *rsrc)
{
	struct irdma_sc_qp *qp = &rsrc->qp;
	__le64 *qp_ctx = qp->hw_host_ctx;

	set_64bit_val(qp_ctx, 8, qp->sq_pa);
	set_64bit_val(qp_ctx, 16, qp->rq_pa);
	set_64bit_val(qp_ctx, 24,
		      FIELD_PREP(IRDMAQPC_RQSIZE, qp->hw_rq_size) |
		      FIELD_PREP(IRDMAQPC_SQSIZE, qp->hw_sq_size));
	set_64bit_val(qp_ctx, 48,
		      FIELD_PREP(IRDMAQPC_SNDMSS, rsrc->buf_size));
	set_64bit_val(qp_ctx, 56, 0);
	if (qp->dev->hw_attrs.uk_attrs.hw_rev == IRDMA_GEN_1)
		set_64bit_val(qp_ctx, 64, 1);
	set_64bit_val(qp_ctx, 136,
		      FIELD_PREP(IRDMAQPC_TXCQNUM, rsrc->cq_id) |
		      FIELD_PREP(IRDMAQPC_RXCQNUM, rsrc->cq_id));
	set_64bit_val(qp_ctx, 144,
		      FIELD_PREP(IRDMAQPC_STAT_INDEX, rsrc->stats_idx));
	set_64bit_val(qp_ctx, 160,
		      FIELD_PREP(IRDMAQPC_PRIVEN, 1) |
		      FIELD_PREP(IRDMAQPC_USESTATSINSTANCE, rsrc->stats_idx_valid));
	set_64bit_val(qp_ctx, 168,
		      FIELD_PREP(IRDMAQPC_QPCOMPCTX, (uintptr_t)qp));
	set_64bit_val(qp_ctx, 176,
		      FIELD_PREP(IRDMAQPC_SQTPHVAL, qp->sq_tph_val) |
		      FIELD_PREP(IRDMAQPC_RQTPHVAL, qp->rq_tph_val) |
		      FIELD_PREP(IRDMAQPC_QSHANDLE, qp->qs_handle));

	print_hex_dump_debug("PUDA: PUDA QP CONTEXT", DUMP_PREFIX_OFFSET, 16,
			     8, qp_ctx, IRDMA_QP_CTX_SIZE, false);
}

 
static int irdma_puda_qp_wqe(struct irdma_sc_dev *dev, struct irdma_sc_qp *qp)
{
	struct irdma_sc_cqp *cqp;
	__le64 *wqe;
	u64 hdr;
	struct irdma_ccq_cqe_info compl_info;
	int status = 0;

	cqp = dev->cqp;
	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, 0);
	if (!wqe)
		return -ENOMEM;

	set_64bit_val(wqe, 16, qp->hw_host_ctx_pa);
	set_64bit_val(wqe, 40, qp->shadow_area_pa);

	hdr = qp->qp_uk.qp_id |
	      FIELD_PREP(IRDMA_CQPSQ_OPCODE, IRDMA_CQP_OP_CREATE_QP) |
	      FIELD_PREP(IRDMA_CQPSQ_QP_QPTYPE, IRDMA_QP_TYPE_UDA) |
	      FIELD_PREP(IRDMA_CQPSQ_QP_CQNUMVALID, 1) |
	      FIELD_PREP(IRDMA_CQPSQ_QP_NEXTIWSTATE, 2) |
	      FIELD_PREP(IRDMA_CQPSQ_WQEVALID, cqp->polarity);
	dma_wmb();  

	set_64bit_val(wqe, 24, hdr);

	print_hex_dump_debug("PUDA: PUDA QP CREATE", DUMP_PREFIX_OFFSET, 16,
			     8, wqe, 40, false);
	irdma_sc_cqp_post_sq(cqp);
	status = irdma_sc_poll_for_cqp_op_done(dev->cqp, IRDMA_CQP_OP_CREATE_QP,
					       &compl_info);

	return status;
}

 
static int irdma_puda_qp_create(struct irdma_puda_rsrc *rsrc)
{
	struct irdma_sc_qp *qp = &rsrc->qp;
	struct irdma_qp_uk *ukqp = &qp->qp_uk;
	int ret = 0;
	u32 sq_size, rq_size;
	struct irdma_dma_mem *mem;

	sq_size = rsrc->sq_size * IRDMA_QP_WQE_MIN_SIZE;
	rq_size = rsrc->rq_size * IRDMA_QP_WQE_MIN_SIZE;
	rsrc->qpmem.size = ALIGN((sq_size + rq_size + (IRDMA_SHADOW_AREA_SIZE << 3) + IRDMA_QP_CTX_SIZE),
				 IRDMA_HW_PAGE_SIZE);
	rsrc->qpmem.va = dma_alloc_coherent(rsrc->dev->hw->device,
					    rsrc->qpmem.size, &rsrc->qpmem.pa,
					    GFP_KERNEL);
	if (!rsrc->qpmem.va)
		return -ENOMEM;

	mem = &rsrc->qpmem;
	memset(mem->va, 0, rsrc->qpmem.size);
	qp->hw_sq_size = irdma_get_encoded_wqe_size(rsrc->sq_size, IRDMA_QUEUE_TYPE_SQ_RQ);
	qp->hw_rq_size = irdma_get_encoded_wqe_size(rsrc->rq_size, IRDMA_QUEUE_TYPE_SQ_RQ);
	qp->pd = &rsrc->sc_pd;
	qp->qp_uk.qp_type = IRDMA_QP_TYPE_UDA;
	qp->dev = rsrc->dev;
	qp->qp_uk.back_qp = rsrc;
	qp->sq_pa = mem->pa;
	qp->rq_pa = qp->sq_pa + sq_size;
	qp->vsi = rsrc->vsi;
	ukqp->sq_base = mem->va;
	ukqp->rq_base = &ukqp->sq_base[rsrc->sq_size];
	ukqp->shadow_area = ukqp->rq_base[rsrc->rq_size].elem;
	ukqp->uk_attrs = &qp->dev->hw_attrs.uk_attrs;
	qp->shadow_area_pa = qp->rq_pa + rq_size;
	qp->hw_host_ctx = ukqp->shadow_area + IRDMA_SHADOW_AREA_SIZE;
	qp->hw_host_ctx_pa = qp->shadow_area_pa + (IRDMA_SHADOW_AREA_SIZE << 3);
	qp->push_idx = IRDMA_INVALID_PUSH_PAGE_INDEX;
	ukqp->qp_id = rsrc->qp_id;
	ukqp->sq_wrtrk_array = rsrc->sq_wrtrk_array;
	ukqp->rq_wrid_array = rsrc->rq_wrid_array;
	ukqp->sq_size = rsrc->sq_size;
	ukqp->rq_size = rsrc->rq_size;

	IRDMA_RING_INIT(ukqp->sq_ring, ukqp->sq_size);
	IRDMA_RING_INIT(ukqp->initial_ring, ukqp->sq_size);
	IRDMA_RING_INIT(ukqp->rq_ring, ukqp->rq_size);
	ukqp->wqe_alloc_db = qp->pd->dev->wqe_alloc_db;

	ret = rsrc->dev->ws_add(qp->vsi, qp->user_pri);
	if (ret) {
		dma_free_coherent(rsrc->dev->hw->device, rsrc->qpmem.size,
				  rsrc->qpmem.va, rsrc->qpmem.pa);
		rsrc->qpmem.va = NULL;
		return ret;
	}

	irdma_qp_add_qos(qp);
	irdma_puda_qp_setctx(rsrc);

	if (rsrc->dev->ceq_valid)
		ret = irdma_cqp_qp_create_cmd(rsrc->dev, qp);
	else
		ret = irdma_puda_qp_wqe(rsrc->dev, qp);
	if (ret) {
		irdma_qp_rem_qos(qp);
		rsrc->dev->ws_remove(qp->vsi, qp->user_pri);
		dma_free_coherent(rsrc->dev->hw->device, rsrc->qpmem.size,
				  rsrc->qpmem.va, rsrc->qpmem.pa);
		rsrc->qpmem.va = NULL;
	}

	return ret;
}

 
static int irdma_puda_cq_wqe(struct irdma_sc_dev *dev, struct irdma_sc_cq *cq)
{
	__le64 *wqe;
	struct irdma_sc_cqp *cqp;
	u64 hdr;
	struct irdma_ccq_cqe_info compl_info;
	int status = 0;

	cqp = dev->cqp;
	wqe = irdma_sc_cqp_get_next_send_wqe(cqp, 0);
	if (!wqe)
		return -ENOMEM;

	set_64bit_val(wqe, 0, cq->cq_uk.cq_size);
	set_64bit_val(wqe, 8, (uintptr_t)cq >> 1);
	set_64bit_val(wqe, 16,
		      FIELD_PREP(IRDMA_CQPSQ_CQ_SHADOW_READ_THRESHOLD, cq->shadow_read_threshold));
	set_64bit_val(wqe, 32, cq->cq_pa);
	set_64bit_val(wqe, 40, cq->shadow_area_pa);
	set_64bit_val(wqe, 56,
		      FIELD_PREP(IRDMA_CQPSQ_TPHVAL, cq->tph_val) |
		      FIELD_PREP(IRDMA_CQPSQ_VSIIDX, cq->vsi->vsi_idx));

	hdr = cq->cq_uk.cq_id |
	      FIELD_PREP(IRDMA_CQPSQ_OPCODE, IRDMA_CQP_OP_CREATE_CQ) |
	      FIELD_PREP(IRDMA_CQPSQ_CQ_CHKOVERFLOW, 1) |
	      FIELD_PREP(IRDMA_CQPSQ_CQ_ENCEQEMASK, 1) |
	      FIELD_PREP(IRDMA_CQPSQ_CQ_CEQIDVALID, 1) |
	      FIELD_PREP(IRDMA_CQPSQ_WQEVALID, cqp->polarity);
	dma_wmb();  

	set_64bit_val(wqe, 24, hdr);

	print_hex_dump_debug("PUDA: PUDA CREATE CQ", DUMP_PREFIX_OFFSET, 16,
			     8, wqe, IRDMA_CQP_WQE_SIZE * 8, false);
	irdma_sc_cqp_post_sq(dev->cqp);
	status = irdma_sc_poll_for_cqp_op_done(dev->cqp, IRDMA_CQP_OP_CREATE_CQ,
					       &compl_info);
	if (!status) {
		struct irdma_sc_ceq *ceq = dev->ceq[0];

		if (ceq && ceq->reg_cq)
			status = irdma_sc_add_cq_ctx(ceq, cq);
	}

	return status;
}

 
static int irdma_puda_cq_create(struct irdma_puda_rsrc *rsrc)
{
	struct irdma_sc_dev *dev = rsrc->dev;
	struct irdma_sc_cq *cq = &rsrc->cq;
	int ret = 0;
	u32 cqsize;
	struct irdma_dma_mem *mem;
	struct irdma_cq_init_info info = {};
	struct irdma_cq_uk_init_info *init_info = &info.cq_uk_init_info;

	cq->vsi = rsrc->vsi;
	cqsize = rsrc->cq_size * (sizeof(struct irdma_cqe));
	rsrc->cqmem.size = ALIGN(cqsize + sizeof(struct irdma_cq_shadow_area),
				 IRDMA_CQ0_ALIGNMENT);
	rsrc->cqmem.va = dma_alloc_coherent(dev->hw->device, rsrc->cqmem.size,
					    &rsrc->cqmem.pa, GFP_KERNEL);
	if (!rsrc->cqmem.va)
		return -ENOMEM;

	mem = &rsrc->cqmem;
	info.dev = dev;
	info.type = (rsrc->type == IRDMA_PUDA_RSRC_TYPE_ILQ) ?
		    IRDMA_CQ_TYPE_ILQ : IRDMA_CQ_TYPE_IEQ;
	info.shadow_read_threshold = rsrc->cq_size >> 2;
	info.cq_base_pa = mem->pa;
	info.shadow_area_pa = mem->pa + cqsize;
	init_info->cq_base = mem->va;
	init_info->shadow_area = (__le64 *)((u8 *)mem->va + cqsize);
	init_info->cq_size = rsrc->cq_size;
	init_info->cq_id = rsrc->cq_id;
	info.ceqe_mask = true;
	info.ceq_id_valid = true;
	info.vsi = rsrc->vsi;

	ret = irdma_sc_cq_init(cq, &info);
	if (ret)
		goto error;

	if (rsrc->dev->ceq_valid)
		ret = irdma_cqp_cq_create_cmd(dev, cq);
	else
		ret = irdma_puda_cq_wqe(dev, cq);
error:
	if (ret) {
		dma_free_coherent(dev->hw->device, rsrc->cqmem.size,
				  rsrc->cqmem.va, rsrc->cqmem.pa);
		rsrc->cqmem.va = NULL;
	}

	return ret;
}

 
static void irdma_puda_free_qp(struct irdma_puda_rsrc *rsrc)
{
	int ret;
	struct irdma_ccq_cqe_info compl_info;
	struct irdma_sc_dev *dev = rsrc->dev;

	if (rsrc->dev->ceq_valid) {
		irdma_cqp_qp_destroy_cmd(dev, &rsrc->qp);
		rsrc->dev->ws_remove(rsrc->qp.vsi, rsrc->qp.user_pri);
		return;
	}

	ret = irdma_sc_qp_destroy(&rsrc->qp, 0, false, true, true);
	if (ret)
		ibdev_dbg(to_ibdev(dev),
			  "PUDA: error puda qp destroy wqe, status = %d\n",
			  ret);
	if (!ret) {
		ret = irdma_sc_poll_for_cqp_op_done(dev->cqp, IRDMA_CQP_OP_DESTROY_QP,
						    &compl_info);
		if (ret)
			ibdev_dbg(to_ibdev(dev),
				  "PUDA: error puda qp destroy failed, status = %d\n",
				  ret);
	}
	rsrc->dev->ws_remove(rsrc->qp.vsi, rsrc->qp.user_pri);
}

 
static void irdma_puda_free_cq(struct irdma_puda_rsrc *rsrc)
{
	int ret;
	struct irdma_ccq_cqe_info compl_info;
	struct irdma_sc_dev *dev = rsrc->dev;

	if (rsrc->dev->ceq_valid) {
		irdma_cqp_cq_destroy_cmd(dev, &rsrc->cq);
		return;
	}

	ret = irdma_sc_cq_destroy(&rsrc->cq, 0, true);
	if (ret)
		ibdev_dbg(to_ibdev(dev), "PUDA: error ieq cq destroy\n");
	if (!ret) {
		ret = irdma_sc_poll_for_cqp_op_done(dev->cqp, IRDMA_CQP_OP_DESTROY_CQ,
						    &compl_info);
		if (ret)
			ibdev_dbg(to_ibdev(dev),
				  "PUDA: error ieq qp destroy done\n");
	}
}

 
void irdma_puda_dele_rsrc(struct irdma_sc_vsi *vsi, enum puda_rsrc_type type,
			  bool reset)
{
	struct irdma_sc_dev *dev = vsi->dev;
	struct irdma_puda_rsrc *rsrc;
	struct irdma_puda_buf *buf = NULL;
	struct irdma_puda_buf *nextbuf = NULL;
	struct irdma_virt_mem *vmem;
	struct irdma_sc_ceq *ceq;

	ceq = vsi->dev->ceq[0];
	switch (type) {
	case IRDMA_PUDA_RSRC_TYPE_ILQ:
		rsrc = vsi->ilq;
		vmem = &vsi->ilq_mem;
		vsi->ilq = NULL;
		if (ceq && ceq->reg_cq)
			irdma_sc_remove_cq_ctx(ceq, &rsrc->cq);
		break;
	case IRDMA_PUDA_RSRC_TYPE_IEQ:
		rsrc = vsi->ieq;
		vmem = &vsi->ieq_mem;
		vsi->ieq = NULL;
		if (ceq && ceq->reg_cq)
			irdma_sc_remove_cq_ctx(ceq, &rsrc->cq);
		break;
	default:
		ibdev_dbg(to_ibdev(dev), "PUDA: error resource type = 0x%x\n",
			  type);
		return;
	}

	switch (rsrc->cmpl) {
	case PUDA_HASH_CRC_COMPLETE:
		irdma_free_hash_desc(rsrc->hash_desc);
		fallthrough;
	case PUDA_QP_CREATED:
		irdma_qp_rem_qos(&rsrc->qp);

		if (!reset)
			irdma_puda_free_qp(rsrc);

		dma_free_coherent(dev->hw->device, rsrc->qpmem.size,
				  rsrc->qpmem.va, rsrc->qpmem.pa);
		rsrc->qpmem.va = NULL;
		fallthrough;
	case PUDA_CQ_CREATED:
		if (!reset)
			irdma_puda_free_cq(rsrc);

		dma_free_coherent(dev->hw->device, rsrc->cqmem.size,
				  rsrc->cqmem.va, rsrc->cqmem.pa);
		rsrc->cqmem.va = NULL;
		break;
	default:
		ibdev_dbg(to_ibdev(rsrc->dev), "PUDA: error no resources\n");
		break;
	}
	 
	buf = rsrc->alloclist;
	while (buf) {
		nextbuf = buf->next;
		irdma_puda_dele_buf(dev, buf);
		buf = nextbuf;
		rsrc->alloc_buf_count--;
	}

	kfree(vmem->va);
}

 
static int irdma_puda_allocbufs(struct irdma_puda_rsrc *rsrc, u32 count)
{
	u32 i;
	struct irdma_puda_buf *buf;
	struct irdma_puda_buf *nextbuf;

	for (i = 0; i < count; i++) {
		buf = irdma_puda_alloc_buf(rsrc->dev, rsrc->buf_size);
		if (!buf) {
			rsrc->stats_buf_alloc_fail++;
			return -ENOMEM;
		}
		irdma_puda_ret_bufpool(rsrc, buf);
		rsrc->alloc_buf_count++;
		if (!rsrc->alloclist) {
			rsrc->alloclist = buf;
		} else {
			nextbuf = rsrc->alloclist;
			rsrc->alloclist = buf;
			buf->next = nextbuf;
		}
	}

	rsrc->avail_buf_count = rsrc->alloc_buf_count;

	return 0;
}

 
int irdma_puda_create_rsrc(struct irdma_sc_vsi *vsi,
			   struct irdma_puda_rsrc_info *info)
{
	struct irdma_sc_dev *dev = vsi->dev;
	int ret = 0;
	struct irdma_puda_rsrc *rsrc;
	u32 pudasize;
	u32 sqwridsize, rqwridsize;
	struct irdma_virt_mem *vmem;

	info->count = 1;
	pudasize = sizeof(struct irdma_puda_rsrc);
	sqwridsize = info->sq_size * sizeof(struct irdma_sq_uk_wr_trk_info);
	rqwridsize = info->rq_size * 8;
	switch (info->type) {
	case IRDMA_PUDA_RSRC_TYPE_ILQ:
		vmem = &vsi->ilq_mem;
		break;
	case IRDMA_PUDA_RSRC_TYPE_IEQ:
		vmem = &vsi->ieq_mem;
		break;
	default:
		return -EOPNOTSUPP;
	}
	vmem->size = pudasize + sqwridsize + rqwridsize;
	vmem->va = kzalloc(vmem->size, GFP_KERNEL);
	if (!vmem->va)
		return -ENOMEM;

	rsrc = vmem->va;
	spin_lock_init(&rsrc->bufpool_lock);
	switch (info->type) {
	case IRDMA_PUDA_RSRC_TYPE_ILQ:
		vsi->ilq = vmem->va;
		vsi->ilq_count = info->count;
		rsrc->receive = info->receive;
		rsrc->xmit_complete = info->xmit_complete;
		break;
	case IRDMA_PUDA_RSRC_TYPE_IEQ:
		vsi->ieq_count = info->count;
		vsi->ieq = vmem->va;
		rsrc->receive = irdma_ieq_receive;
		rsrc->xmit_complete = irdma_ieq_tx_compl;
		break;
	default:
		return -EOPNOTSUPP;
	}

	rsrc->type = info->type;
	rsrc->sq_wrtrk_array = (struct irdma_sq_uk_wr_trk_info *)
			       ((u8 *)vmem->va + pudasize);
	rsrc->rq_wrid_array = (u64 *)((u8 *)vmem->va + pudasize + sqwridsize);
	 
	INIT_LIST_HEAD(&rsrc->bufpool);
	INIT_LIST_HEAD(&rsrc->txpend);

	rsrc->tx_wqe_avail_cnt = info->sq_size - 1;
	irdma_sc_pd_init(dev, &rsrc->sc_pd, info->pd_id, info->abi_ver);
	rsrc->qp_id = info->qp_id;
	rsrc->cq_id = info->cq_id;
	rsrc->sq_size = info->sq_size;
	rsrc->rq_size = info->rq_size;
	rsrc->cq_size = info->rq_size + info->sq_size;
	if (dev->hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_2) {
		if (rsrc->type == IRDMA_PUDA_RSRC_TYPE_ILQ)
			rsrc->cq_size += info->rq_size;
	}
	rsrc->buf_size = info->buf_size;
	rsrc->dev = dev;
	rsrc->vsi = vsi;
	rsrc->stats_idx = info->stats_idx;
	rsrc->stats_idx_valid = info->stats_idx_valid;

	ret = irdma_puda_cq_create(rsrc);
	if (!ret) {
		rsrc->cmpl = PUDA_CQ_CREATED;
		ret = irdma_puda_qp_create(rsrc);
	}
	if (ret) {
		ibdev_dbg(to_ibdev(dev),
			  "PUDA: error qp_create type=%d, status=%d\n",
			  rsrc->type, ret);
		goto error;
	}
	rsrc->cmpl = PUDA_QP_CREATED;

	ret = irdma_puda_allocbufs(rsrc, info->tx_buf_cnt + info->rq_size);
	if (ret) {
		ibdev_dbg(to_ibdev(dev), "PUDA: error alloc_buf\n");
		goto error;
	}

	rsrc->rxq_invalid_cnt = info->rq_size;
	ret = irdma_puda_replenish_rq(rsrc, true);
	if (ret)
		goto error;

	if (info->type == IRDMA_PUDA_RSRC_TYPE_IEQ) {
		if (!irdma_init_hash_desc(&rsrc->hash_desc)) {
			rsrc->check_crc = true;
			rsrc->cmpl = PUDA_HASH_CRC_COMPLETE;
			ret = 0;
		}
	}

	irdma_sc_ccq_arm(&rsrc->cq);
	return ret;

error:
	irdma_puda_dele_rsrc(vsi, info->type, false);

	return ret;
}

 
static void irdma_ilq_putback_rcvbuf(struct irdma_sc_qp *qp,
				     struct irdma_puda_buf *buf, u32 wqe_idx)
{
	__le64 *wqe;
	u64 offset8, offset24;

	 
	dma_sync_single_for_device(qp->dev->hw->device, buf->mem.pa,
				   buf->mem.size, DMA_BIDIRECTIONAL);
	wqe = qp->qp_uk.rq_base[wqe_idx].elem;
	get_64bit_val(wqe, 24, &offset24);
	if (qp->dev->hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_2) {
		get_64bit_val(wqe, 8, &offset8);
		if (offset24)
			offset8 &= ~FIELD_PREP(IRDMAQPSQ_VALID, 1);
		else
			offset8 |= FIELD_PREP(IRDMAQPSQ_VALID, 1);
		set_64bit_val(wqe, 8, offset8);
		dma_wmb();  
	}
	if (offset24)
		offset24 = 0;
	else
		offset24 = FIELD_PREP(IRDMAQPSQ_VALID, 1);

	set_64bit_val(wqe, 24, offset24);
}

 
static u16 irdma_ieq_get_fpdu_len(struct irdma_pfpdu *pfpdu, u8 *datap,
				  u32 rcv_seq)
{
	u32 marker_seq, end_seq, blk_start;
	u8 marker_len = pfpdu->marker_len;
	u16 total_len = 0;
	u16 fpdu_len;

	blk_start = (pfpdu->rcv_start_seq - rcv_seq) & (IRDMA_MRK_BLK_SZ - 1);
	if (!blk_start) {
		total_len = marker_len;
		marker_seq = rcv_seq + IRDMA_MRK_BLK_SZ;
		if (marker_len && *(u32 *)datap)
			return 0;
	} else {
		marker_seq = rcv_seq + blk_start;
	}

	datap += total_len;
	fpdu_len = ntohs(*(__be16 *)datap);
	fpdu_len += IRDMA_IEQ_MPA_FRAMING;
	fpdu_len = (fpdu_len + 3) & 0xfffc;

	if (fpdu_len > pfpdu->max_fpdu_data)
		return 0;

	total_len += fpdu_len;
	end_seq = rcv_seq + total_len;
	while ((int)(marker_seq - end_seq) < 0) {
		total_len += marker_len;
		end_seq += marker_len;
		marker_seq += IRDMA_MRK_BLK_SZ;
	}

	return total_len;
}

 
static void irdma_ieq_copy_to_txbuf(struct irdma_puda_buf *buf,
				    struct irdma_puda_buf *txbuf,
				    u16 buf_offset, u32 txbuf_offset, u32 len)
{
	void *mem1 = (u8 *)buf->mem.va + buf_offset;
	void *mem2 = (u8 *)txbuf->mem.va + txbuf_offset;

	memcpy(mem2, mem1, len);
}

 
static void irdma_ieq_setup_tx_buf(struct irdma_puda_buf *buf,
				   struct irdma_puda_buf *txbuf)
{
	txbuf->tcphlen = buf->tcphlen;
	txbuf->ipv4 = buf->ipv4;

	if (buf->vsi->dev->hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_2) {
		txbuf->hdrlen = txbuf->tcphlen;
		irdma_ieq_copy_to_txbuf(buf, txbuf, IRDMA_TCP_OFFSET, 0,
					txbuf->hdrlen);
	} else {
		txbuf->maclen = buf->maclen;
		txbuf->hdrlen = buf->hdrlen;
		irdma_ieq_copy_to_txbuf(buf, txbuf, 0, 0, buf->hdrlen);
	}
}

 
static void irdma_ieq_check_first_buf(struct irdma_puda_buf *buf, u32 fps)
{
	u32 offset;

	if (buf->seqnum < fps) {
		offset = fps - buf->seqnum;
		if (offset > buf->datalen)
			return;
		buf->data += offset;
		buf->datalen -= (u16)offset;
		buf->seqnum = fps;
	}
}

 
static void irdma_ieq_compl_pfpdu(struct irdma_puda_rsrc *ieq,
				  struct list_head *rxlist,
				  struct list_head *pbufl,
				  struct irdma_puda_buf *txbuf, u16 fpdu_len)
{
	struct irdma_puda_buf *buf;
	u32 nextseqnum;
	u16 txoffset, bufoffset;

	buf = irdma_puda_get_listbuf(pbufl);
	if (!buf)
		return;

	nextseqnum = buf->seqnum + fpdu_len;
	irdma_ieq_setup_tx_buf(buf, txbuf);
	if (buf->vsi->dev->hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_2) {
		txoffset = txbuf->hdrlen;
		txbuf->totallen = txbuf->hdrlen + fpdu_len;
		txbuf->data = (u8 *)txbuf->mem.va + txoffset;
	} else {
		txoffset = buf->hdrlen;
		txbuf->totallen = buf->hdrlen + fpdu_len;
		txbuf->data = (u8 *)txbuf->mem.va + buf->hdrlen;
	}
	bufoffset = (u16)(buf->data - (u8 *)buf->mem.va);

	do {
		if (buf->datalen >= fpdu_len) {
			 
			irdma_ieq_copy_to_txbuf(buf, txbuf, bufoffset, txoffset,
						fpdu_len);
			buf->datalen -= fpdu_len;
			buf->data += fpdu_len;
			buf->seqnum = nextseqnum;
			break;
		}
		 
		irdma_ieq_copy_to_txbuf(buf, txbuf, bufoffset, txoffset,
					buf->datalen);
		txoffset += buf->datalen;
		fpdu_len -= buf->datalen;
		irdma_puda_ret_bufpool(ieq, buf);
		buf = irdma_puda_get_listbuf(pbufl);
		if (!buf)
			return;

		bufoffset = (u16)(buf->data - (u8 *)buf->mem.va);
	} while (1);

	 
	if (buf->datalen)
		list_add(&buf->list, rxlist);
	else
		irdma_puda_ret_bufpool(ieq, buf);
}

 
static int irdma_ieq_create_pbufl(struct irdma_pfpdu *pfpdu,
				  struct list_head *rxlist,
				  struct list_head *pbufl,
				  struct irdma_puda_buf *buf, u16 fpdu_len)
{
	int status = 0;
	struct irdma_puda_buf *nextbuf;
	u32 nextseqnum;
	u16 plen = fpdu_len - buf->datalen;
	bool done = false;

	nextseqnum = buf->seqnum + buf->datalen;
	do {
		nextbuf = irdma_puda_get_listbuf(rxlist);
		if (!nextbuf) {
			status = -ENOBUFS;
			break;
		}
		list_add_tail(&nextbuf->list, pbufl);
		if (nextbuf->seqnum != nextseqnum) {
			pfpdu->bad_seq_num++;
			status = -ERANGE;
			break;
		}
		if (nextbuf->datalen >= plen) {
			done = true;
		} else {
			plen -= nextbuf->datalen;
			nextseqnum = nextbuf->seqnum + nextbuf->datalen;
		}

	} while (!done);

	return status;
}

 
static int irdma_ieq_handle_partial(struct irdma_puda_rsrc *ieq,
				    struct irdma_pfpdu *pfpdu,
				    struct irdma_puda_buf *buf, u16 fpdu_len)
{
	int status = 0;
	u8 *crcptr;
	u32 mpacrc;
	u32 seqnum = buf->seqnum;
	struct list_head pbufl;  
	struct irdma_puda_buf *txbuf = NULL;
	struct list_head *rxlist = &pfpdu->rxlist;

	ieq->partials_handled++;

	INIT_LIST_HEAD(&pbufl);
	list_add(&buf->list, &pbufl);

	status = irdma_ieq_create_pbufl(pfpdu, rxlist, &pbufl, buf, fpdu_len);
	if (status)
		goto error;

	txbuf = irdma_puda_get_bufpool(ieq);
	if (!txbuf) {
		pfpdu->no_tx_bufs++;
		status = -ENOBUFS;
		goto error;
	}

	irdma_ieq_compl_pfpdu(ieq, rxlist, &pbufl, txbuf, fpdu_len);
	irdma_ieq_update_tcpip_info(txbuf, fpdu_len, seqnum);

	crcptr = txbuf->data + fpdu_len - 4;
	mpacrc = *(u32 *)crcptr;
	if (ieq->check_crc) {
		status = irdma_ieq_check_mpacrc(ieq->hash_desc, txbuf->data,
						(fpdu_len - 4), mpacrc);
		if (status) {
			ibdev_dbg(to_ibdev(ieq->dev), "IEQ: error bad crc\n");
			goto error;
		}
	}

	print_hex_dump_debug("IEQ: IEQ TX BUFFER", DUMP_PREFIX_OFFSET, 16, 8,
			     txbuf->mem.va, txbuf->totallen, false);
	if (ieq->dev->hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_2)
		txbuf->ah_id = pfpdu->ah->ah_info.ah_idx;
	txbuf->do_lpb = true;
	irdma_puda_send_buf(ieq, txbuf);
	pfpdu->rcv_nxt = seqnum + fpdu_len;
	return status;

error:
	while (!list_empty(&pbufl)) {
		buf = list_last_entry(&pbufl, struct irdma_puda_buf, list);
		list_move(&buf->list, rxlist);
	}
	if (txbuf)
		irdma_puda_ret_bufpool(ieq, txbuf);

	return status;
}

 
static int irdma_ieq_process_buf(struct irdma_puda_rsrc *ieq,
				 struct irdma_pfpdu *pfpdu,
				 struct irdma_puda_buf *buf)
{
	u16 fpdu_len = 0;
	u16 datalen = buf->datalen;
	u8 *datap = buf->data;
	u8 *crcptr;
	u16 ioffset = 0;
	u32 mpacrc;
	u32 seqnum = buf->seqnum;
	u16 len = 0;
	u16 full = 0;
	bool partial = false;
	struct irdma_puda_buf *txbuf;
	struct list_head *rxlist = &pfpdu->rxlist;
	int ret = 0;

	ioffset = (u16)(buf->data - (u8 *)buf->mem.va);
	while (datalen) {
		fpdu_len = irdma_ieq_get_fpdu_len(pfpdu, datap, buf->seqnum);
		if (!fpdu_len) {
			ibdev_dbg(to_ibdev(ieq->dev),
				  "IEQ: error bad fpdu len\n");
			list_add(&buf->list, rxlist);
			return -EINVAL;
		}

		if (datalen < fpdu_len) {
			partial = true;
			break;
		}
		crcptr = datap + fpdu_len - 4;
		mpacrc = *(u32 *)crcptr;
		if (ieq->check_crc)
			ret = irdma_ieq_check_mpacrc(ieq->hash_desc, datap,
						     fpdu_len - 4, mpacrc);
		if (ret) {
			list_add(&buf->list, rxlist);
			ibdev_dbg(to_ibdev(ieq->dev),
				  "ERR: IRDMA_ERR_MPA_CRC\n");
			return -EINVAL;
		}
		full++;
		pfpdu->fpdu_processed++;
		ieq->fpdu_processed++;
		datap += fpdu_len;
		len += fpdu_len;
		datalen -= fpdu_len;
	}
	if (full) {
		 
		txbuf = irdma_puda_get_bufpool(ieq);
		if (!txbuf) {
			pfpdu->no_tx_bufs++;
			list_add(&buf->list, rxlist);
			return -ENOBUFS;
		}
		 
		irdma_ieq_setup_tx_buf(buf, txbuf);
		 
		if (ieq->dev->hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_2) {
			irdma_ieq_copy_to_txbuf(buf, txbuf, ioffset,
						txbuf->hdrlen, len);
			txbuf->totallen = txbuf->hdrlen + len;
			txbuf->ah_id = pfpdu->ah->ah_info.ah_idx;
		} else {
			irdma_ieq_copy_to_txbuf(buf, txbuf, ioffset,
						buf->hdrlen, len);
			txbuf->totallen = buf->hdrlen + len;
		}
		irdma_ieq_update_tcpip_info(txbuf, len, buf->seqnum);
		print_hex_dump_debug("IEQ: IEQ TX BUFFER", DUMP_PREFIX_OFFSET,
				     16, 8, txbuf->mem.va, txbuf->totallen,
				     false);
		txbuf->do_lpb = true;
		irdma_puda_send_buf(ieq, txbuf);

		if (!datalen) {
			pfpdu->rcv_nxt = buf->seqnum + len;
			irdma_puda_ret_bufpool(ieq, buf);
			return 0;
		}
		buf->data = datap;
		buf->seqnum = seqnum + len;
		buf->datalen = datalen;
		pfpdu->rcv_nxt = buf->seqnum;
	}
	if (partial)
		return irdma_ieq_handle_partial(ieq, pfpdu, buf, fpdu_len);

	return 0;
}

 
void irdma_ieq_process_fpdus(struct irdma_sc_qp *qp,
			     struct irdma_puda_rsrc *ieq)
{
	struct irdma_pfpdu *pfpdu = &qp->pfpdu;
	struct list_head *rxlist = &pfpdu->rxlist;
	struct irdma_puda_buf *buf;
	int status;

	do {
		if (list_empty(rxlist))
			break;
		buf = irdma_puda_get_listbuf(rxlist);
		if (!buf) {
			ibdev_dbg(to_ibdev(ieq->dev), "IEQ: error no buf\n");
			break;
		}
		if (buf->seqnum != pfpdu->rcv_nxt) {
			 
			pfpdu->out_of_order++;
			list_add(&buf->list, rxlist);
			break;
		}
		 
		status = irdma_ieq_process_buf(ieq, pfpdu, buf);
		if (status == -EINVAL) {
			pfpdu->mpa_crc_err = true;
			while (!list_empty(rxlist)) {
				buf = irdma_puda_get_listbuf(rxlist);
				irdma_puda_ret_bufpool(ieq, buf);
				pfpdu->crc_err++;
				ieq->crc_err++;
			}
			 
			irdma_ieq_mpa_crc_ae(ieq->dev, qp);
		}
	} while (!status);
}

 
static int irdma_ieq_create_ah(struct irdma_sc_qp *qp, struct irdma_puda_buf *buf)
{
	struct irdma_ah_info ah_info = {};

	qp->pfpdu.ah_buf = buf;
	irdma_puda_ieq_get_ah_info(qp, &ah_info);
	return irdma_puda_create_ah(qp->vsi->dev, &ah_info, false,
				    IRDMA_PUDA_RSRC_TYPE_IEQ, qp,
				    &qp->pfpdu.ah);
}

 
static void irdma_ieq_handle_exception(struct irdma_puda_rsrc *ieq,
				       struct irdma_sc_qp *qp,
				       struct irdma_puda_buf *buf)
{
	struct irdma_pfpdu *pfpdu = &qp->pfpdu;
	u32 *hw_host_ctx = (u32 *)qp->hw_host_ctx;
	u32 rcv_wnd = hw_host_ctx[23];
	 
	u32 fps = *(u32 *)(qp->q2_buf + Q2_FPSN_OFFSET);
	struct list_head *rxlist = &pfpdu->rxlist;
	unsigned long flags = 0;
	u8 hw_rev = qp->dev->hw_attrs.uk_attrs.hw_rev;

	print_hex_dump_debug("IEQ: IEQ RX BUFFER", DUMP_PREFIX_OFFSET, 16, 8,
			     buf->mem.va, buf->totallen, false);

	spin_lock_irqsave(&pfpdu->lock, flags);
	pfpdu->total_ieq_bufs++;
	if (pfpdu->mpa_crc_err) {
		pfpdu->crc_err++;
		goto error;
	}
	if (pfpdu->mode && fps != pfpdu->fps) {
		 
		irdma_ieq_cleanup_qp(ieq, qp);
		ibdev_dbg(to_ibdev(ieq->dev), "IEQ: restarting new partial\n");
		pfpdu->mode = false;
	}

	if (!pfpdu->mode) {
		print_hex_dump_debug("IEQ: Q2 BUFFER", DUMP_PREFIX_OFFSET, 16,
				     8, (u64 *)qp->q2_buf, 128, false);
		 
		pfpdu->rcv_nxt = fps;
		pfpdu->fps = fps;
		pfpdu->mode = true;
		pfpdu->max_fpdu_data = (buf->ipv4) ?
				       (ieq->vsi->mtu - IRDMA_MTU_TO_MSS_IPV4) :
				       (ieq->vsi->mtu - IRDMA_MTU_TO_MSS_IPV6);
		pfpdu->pmode_count++;
		ieq->pmode_count++;
		INIT_LIST_HEAD(rxlist);
		irdma_ieq_check_first_buf(buf, fps);
	}

	if (!(rcv_wnd >= (buf->seqnum - pfpdu->rcv_nxt))) {
		pfpdu->bad_seq_num++;
		ieq->bad_seq_num++;
		goto error;
	}

	if (!list_empty(rxlist)) {
		if (buf->seqnum != pfpdu->nextseqnum) {
			irdma_send_ieq_ack(qp);
			 
			goto error;
		}
	}
	 
	list_add_tail(&buf->list, rxlist);
	pfpdu->nextseqnum = buf->seqnum + buf->datalen;
	pfpdu->lastrcv_buf = buf;
	if (hw_rev >= IRDMA_GEN_2 && !pfpdu->ah) {
		irdma_ieq_create_ah(qp, buf);
		if (!pfpdu->ah)
			goto error;
		goto exit;
	}
	if (hw_rev == IRDMA_GEN_1)
		irdma_ieq_process_fpdus(qp, ieq);
	else if (pfpdu->ah && pfpdu->ah->ah_info.ah_valid)
		irdma_ieq_process_fpdus(qp, ieq);
exit:
	spin_unlock_irqrestore(&pfpdu->lock, flags);

	return;

error:
	irdma_puda_ret_bufpool(ieq, buf);
	spin_unlock_irqrestore(&pfpdu->lock, flags);
}

 
static void irdma_ieq_receive(struct irdma_sc_vsi *vsi,
			      struct irdma_puda_buf *buf)
{
	struct irdma_puda_rsrc *ieq = vsi->ieq;
	struct irdma_sc_qp *qp = NULL;
	u32 wqe_idx = ieq->compl_rxwqe_idx;

	qp = irdma_ieq_get_qp(vsi->dev, buf);
	if (!qp) {
		ieq->stats_bad_qp_id++;
		irdma_puda_ret_bufpool(ieq, buf);
	} else {
		irdma_ieq_handle_exception(ieq, qp, buf);
	}
	 
	if (!ieq->rxq_invalid_cnt)
		ieq->rx_wqe_idx = wqe_idx;
	ieq->rxq_invalid_cnt++;
}

 
static void irdma_ieq_tx_compl(struct irdma_sc_vsi *vsi, void *sqwrid)
{
	struct irdma_puda_rsrc *ieq = vsi->ieq;
	struct irdma_puda_buf *buf = sqwrid;

	irdma_puda_ret_bufpool(ieq, buf);
}

 
void irdma_ieq_cleanup_qp(struct irdma_puda_rsrc *ieq, struct irdma_sc_qp *qp)
{
	struct irdma_puda_buf *buf;
	struct irdma_pfpdu *pfpdu = &qp->pfpdu;
	struct list_head *rxlist = &pfpdu->rxlist;

	if (qp->pfpdu.ah) {
		irdma_puda_free_ah(ieq->dev, qp->pfpdu.ah);
		qp->pfpdu.ah = NULL;
		qp->pfpdu.ah_buf = NULL;
	}

	if (!pfpdu->mode)
		return;

	while (!list_empty(rxlist)) {
		buf = irdma_puda_get_listbuf(rxlist);
		irdma_puda_ret_bufpool(ieq, buf);
	}
}
