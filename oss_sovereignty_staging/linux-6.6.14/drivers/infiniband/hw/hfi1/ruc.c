
 

#include <linux/spinlock.h>

#include "hfi.h"
#include "mad.h"
#include "qp.h"
#include "verbs_txreq.h"
#include "trace.h"

static int gid_ok(union ib_gid *gid, __be64 gid_prefix, __be64 id)
{
	return (gid->global.interface_id == id &&
		(gid->global.subnet_prefix == gid_prefix ||
		 gid->global.subnet_prefix == IB_DEFAULT_GID_PREFIX));
}

 
int hfi1_ruc_check_hdr(struct hfi1_ibport *ibp, struct hfi1_packet *packet)
{
	__be64 guid;
	unsigned long flags;
	struct rvt_qp *qp = packet->qp;
	u8 sc5 = ibp->sl_to_sc[rdma_ah_get_sl(&qp->remote_ah_attr)];
	u32 dlid = packet->dlid;
	u32 slid = packet->slid;
	u32 sl = packet->sl;
	bool migrated = packet->migrated;
	u16 pkey = packet->pkey;

	if (qp->s_mig_state == IB_MIG_ARMED && migrated) {
		if (!packet->grh) {
			if ((rdma_ah_get_ah_flags(&qp->alt_ah_attr) &
			     IB_AH_GRH) &&
			    (packet->etype != RHF_RCV_TYPE_BYPASS))
				return 1;
		} else {
			const struct ib_global_route *grh;

			if (!(rdma_ah_get_ah_flags(&qp->alt_ah_attr) &
			      IB_AH_GRH))
				return 1;
			grh = rdma_ah_read_grh(&qp->alt_ah_attr);
			guid = get_sguid(ibp, grh->sgid_index);
			if (!gid_ok(&packet->grh->dgid, ibp->rvp.gid_prefix,
				    guid))
				return 1;
			if (!gid_ok(
				&packet->grh->sgid,
				grh->dgid.global.subnet_prefix,
				grh->dgid.global.interface_id))
				return 1;
		}
		if (unlikely(rcv_pkey_check(ppd_from_ibp(ibp), pkey,
					    sc5, slid))) {
			hfi1_bad_pkey(ibp, pkey, sl, 0, qp->ibqp.qp_num,
				      slid, dlid);
			return 1;
		}
		 
		if (slid != rdma_ah_get_dlid(&qp->alt_ah_attr) ||
		    ppd_from_ibp(ibp)->port !=
			rdma_ah_get_port_num(&qp->alt_ah_attr))
			return 1;
		spin_lock_irqsave(&qp->s_lock, flags);
		hfi1_migrate_qp(qp);
		spin_unlock_irqrestore(&qp->s_lock, flags);
	} else {
		if (!packet->grh) {
			if ((rdma_ah_get_ah_flags(&qp->remote_ah_attr) &
			     IB_AH_GRH) &&
			    (packet->etype != RHF_RCV_TYPE_BYPASS))
				return 1;
		} else {
			const struct ib_global_route *grh;

			if (!(rdma_ah_get_ah_flags(&qp->remote_ah_attr) &
						   IB_AH_GRH))
				return 1;
			grh = rdma_ah_read_grh(&qp->remote_ah_attr);
			guid = get_sguid(ibp, grh->sgid_index);
			if (!gid_ok(&packet->grh->dgid, ibp->rvp.gid_prefix,
				    guid))
				return 1;
			if (!gid_ok(
			     &packet->grh->sgid,
			     grh->dgid.global.subnet_prefix,
			     grh->dgid.global.interface_id))
				return 1;
		}
		if (unlikely(rcv_pkey_check(ppd_from_ibp(ibp), pkey,
					    sc5, slid))) {
			hfi1_bad_pkey(ibp, pkey, sl, 0, qp->ibqp.qp_num,
				      slid, dlid);
			return 1;
		}
		 
		if ((slid != rdma_ah_get_dlid(&qp->remote_ah_attr)) ||
		    ppd_from_ibp(ibp)->port != qp->port_num)
			return 1;
		if (qp->s_mig_state == IB_MIG_REARM && !migrated)
			qp->s_mig_state = IB_MIG_ARMED;
	}

	return 0;
}

 
u32 hfi1_make_grh(struct hfi1_ibport *ibp, struct ib_grh *hdr,
		  const struct ib_global_route *grh, u32 hwords, u32 nwords)
{
	hdr->version_tclass_flow =
		cpu_to_be32((IB_GRH_VERSION << IB_GRH_VERSION_SHIFT) |
			    (grh->traffic_class << IB_GRH_TCLASS_SHIFT) |
			    (grh->flow_label << IB_GRH_FLOW_SHIFT));
	hdr->paylen = cpu_to_be16((hwords + nwords) << 2);
	 
	hdr->next_hdr = IB_GRH_NEXT_HDR;
	hdr->hop_limit = grh->hop_limit;
	 
	hdr->sgid.global.subnet_prefix = ibp->rvp.gid_prefix;
	hdr->sgid.global.interface_id =
		grh->sgid_index < HFI1_GUIDS_PER_PORT ?
		get_sguid(ibp, grh->sgid_index) :
		get_sguid(ibp, HFI1_PORT_GUID_INDEX);
	hdr->dgid = grh->dgid;

	 
	return sizeof(struct ib_grh) / sizeof(u32);
}

#define BTH2_OFFSET (offsetof(struct hfi1_sdma_header, \
			      hdr.ibh.u.oth.bth[2]) / 4)

 
static inline void build_ahg(struct rvt_qp *qp, u32 npsn)
{
	struct hfi1_qp_priv *priv = qp->priv;

	if (unlikely(qp->s_flags & HFI1_S_AHG_CLEAR))
		clear_ahg(qp);
	if (!(qp->s_flags & HFI1_S_AHG_VALID)) {
		 
		if (qp->s_ahgidx < 0)
			qp->s_ahgidx = sdma_ahg_alloc(priv->s_sde);
		if (qp->s_ahgidx >= 0) {
			qp->s_ahgpsn = npsn;
			priv->s_ahg->tx_flags |= SDMA_TXREQ_F_AHG_COPY;
			 
			priv->s_ahg->ahgidx = qp->s_ahgidx;
			qp->s_flags |= HFI1_S_AHG_VALID;
		}
	} else {
		 
		if (qp->s_ahgidx >= 0) {
			priv->s_ahg->tx_flags |= SDMA_TXREQ_F_USE_AHG;
			priv->s_ahg->ahgidx = qp->s_ahgidx;
			priv->s_ahg->ahgcount++;
			priv->s_ahg->ahgdesc[0] =
				sdma_build_ahg_descriptor(
					(__force u16)cpu_to_be16((u16)npsn),
					BTH2_OFFSET,
					16,
					16);
			if ((npsn & 0xffff0000) !=
					(qp->s_ahgpsn & 0xffff0000)) {
				priv->s_ahg->ahgcount++;
				priv->s_ahg->ahgdesc[1] =
					sdma_build_ahg_descriptor(
						(__force u16)cpu_to_be16(
							(u16)(npsn >> 16)),
						BTH2_OFFSET,
						0,
						16);
			}
		}
	}
}

static inline void hfi1_make_ruc_bth(struct rvt_qp *qp,
				     struct ib_other_headers *ohdr,
				     u32 bth0, u32 bth1, u32 bth2)
{
	ohdr->bth[0] = cpu_to_be32(bth0);
	ohdr->bth[1] = cpu_to_be32(bth1);
	ohdr->bth[2] = cpu_to_be32(bth2);
}

 
static inline void hfi1_make_ruc_header_16B(struct rvt_qp *qp,
					    struct ib_other_headers *ohdr,
					    u32 bth0, u32 bth1, u32 bth2,
					    int middle,
					    struct hfi1_pkt_state *ps)
{
	struct hfi1_qp_priv *priv = qp->priv;
	struct hfi1_ibport *ibp = ps->ibp;
	struct hfi1_pportdata *ppd = ppd_from_ibp(ibp);
	u32 slid;
	u16 pkey = hfi1_get_pkey(ibp, qp->s_pkey_index);
	u8 l4 = OPA_16B_L4_IB_LOCAL;
	u8 extra_bytes = hfi1_get_16b_padding(
				(ps->s_txreq->hdr_dwords << 2),
				ps->s_txreq->s_cur_size);
	u32 nwords = SIZE_OF_CRC + ((ps->s_txreq->s_cur_size +
				 extra_bytes + SIZE_OF_LT) >> 2);
	bool becn = false;

	if (unlikely(rdma_ah_get_ah_flags(&qp->remote_ah_attr) & IB_AH_GRH) &&
	    hfi1_check_mcast(rdma_ah_get_dlid(&qp->remote_ah_attr))) {
		struct ib_grh *grh;
		struct ib_global_route *grd =
			rdma_ah_retrieve_grh(&qp->remote_ah_attr);
		 
		if (grd->sgid_index == OPA_GID_INDEX)
			grd->sgid_index = 0;
		grh = &ps->s_txreq->phdr.hdr.opah.u.l.grh;
		l4 = OPA_16B_L4_IB_GLOBAL;
		ps->s_txreq->hdr_dwords +=
			hfi1_make_grh(ibp, grh, grd,
				      ps->s_txreq->hdr_dwords - LRH_16B_DWORDS,
				      nwords);
		middle = 0;
	}

	if (qp->s_mig_state == IB_MIG_MIGRATED)
		bth1 |= OPA_BTH_MIG_REQ;
	else
		middle = 0;

	if (qp->s_flags & RVT_S_ECN) {
		qp->s_flags &= ~RVT_S_ECN;
		 
		becn = true;
		middle = 0;
	}
	if (middle)
		build_ahg(qp, bth2);
	else
		qp->s_flags &= ~HFI1_S_AHG_VALID;

	bth0 |= pkey;
	bth0 |= extra_bytes << 20;
	hfi1_make_ruc_bth(qp, ohdr, bth0, bth1, bth2);

	if (!ppd->lid)
		slid = be32_to_cpu(OPA_LID_PERMISSIVE);
	else
		slid = ppd->lid |
			(rdma_ah_get_path_bits(&qp->remote_ah_attr) &
			((1 << ppd->lmc) - 1));

	hfi1_make_16b_hdr(&ps->s_txreq->phdr.hdr.opah,
			  slid,
			  opa_get_lid(rdma_ah_get_dlid(&qp->remote_ah_attr),
				      16B),
			  (ps->s_txreq->hdr_dwords + nwords) >> 1,
			  pkey, becn, 0, l4, priv->s_sc);
}

 
static inline void hfi1_make_ruc_header_9B(struct rvt_qp *qp,
					   struct ib_other_headers *ohdr,
					   u32 bth0, u32 bth1, u32 bth2,
					   int middle,
					   struct hfi1_pkt_state *ps)
{
	struct hfi1_qp_priv *priv = qp->priv;
	struct hfi1_ibport *ibp = ps->ibp;
	u16 pkey = hfi1_get_pkey(ibp, qp->s_pkey_index);
	u16 lrh0 = HFI1_LRH_BTH;
	u8 extra_bytes = -ps->s_txreq->s_cur_size & 3;
	u32 nwords = SIZE_OF_CRC + ((ps->s_txreq->s_cur_size +
					 extra_bytes) >> 2);

	if (unlikely(rdma_ah_get_ah_flags(&qp->remote_ah_attr) & IB_AH_GRH)) {
		struct ib_grh *grh = &ps->s_txreq->phdr.hdr.ibh.u.l.grh;

		lrh0 = HFI1_LRH_GRH;
		ps->s_txreq->hdr_dwords +=
			hfi1_make_grh(ibp, grh,
				      rdma_ah_read_grh(&qp->remote_ah_attr),
				      ps->s_txreq->hdr_dwords - LRH_9B_DWORDS,
				      nwords);
		middle = 0;
	}
	lrh0 |= (priv->s_sc & 0xf) << 12 |
		(rdma_ah_get_sl(&qp->remote_ah_attr) & 0xf) << 4;

	if (qp->s_mig_state == IB_MIG_MIGRATED)
		bth0 |= IB_BTH_MIG_REQ;
	else
		middle = 0;

	if (qp->s_flags & RVT_S_ECN) {
		qp->s_flags &= ~RVT_S_ECN;
		 
		bth1 |= (IB_BECN_MASK << IB_BECN_SHIFT);
		middle = 0;
	}
	if (middle)
		build_ahg(qp, bth2);
	else
		qp->s_flags &= ~HFI1_S_AHG_VALID;

	bth0 |= pkey;
	bth0 |= extra_bytes << 20;
	hfi1_make_ruc_bth(qp, ohdr, bth0, bth1, bth2);
	hfi1_make_ib_hdr(&ps->s_txreq->phdr.hdr.ibh,
			 lrh0,
			 ps->s_txreq->hdr_dwords + nwords,
			 opa_get_lid(rdma_ah_get_dlid(&qp->remote_ah_attr), 9B),
			 ppd_from_ibp(ibp)->lid |
				rdma_ah_get_path_bits(&qp->remote_ah_attr));
}

typedef void (*hfi1_make_ruc_hdr)(struct rvt_qp *qp,
				  struct ib_other_headers *ohdr,
				  u32 bth0, u32 bth1, u32 bth2, int middle,
				  struct hfi1_pkt_state *ps);

 
static const hfi1_make_ruc_hdr hfi1_ruc_header_tbl[2] = {
	[HFI1_PKT_TYPE_9B] = &hfi1_make_ruc_header_9B,
	[HFI1_PKT_TYPE_16B] = &hfi1_make_ruc_header_16B
};

void hfi1_make_ruc_header(struct rvt_qp *qp, struct ib_other_headers *ohdr,
			  u32 bth0, u32 bth1, u32 bth2, int middle,
			  struct hfi1_pkt_state *ps)
{
	struct hfi1_qp_priv *priv = qp->priv;

	 
	priv->s_ahg->tx_flags = 0;
	priv->s_ahg->ahgcount = 0;
	priv->s_ahg->ahgidx = 0;

	 
	hfi1_ruc_header_tbl[priv->hdr_type](qp, ohdr, bth0, bth1, bth2, middle,
					    ps);
}

 
#define SEND_RESCHED_TIMEOUT (5 * HZ)   

 
bool hfi1_schedule_send_yield(struct rvt_qp *qp, struct hfi1_pkt_state *ps,
			      bool tid)
{
	ps->pkts_sent = true;

	if (unlikely(time_after(jiffies, ps->timeout))) {
		if (!ps->in_thread ||
		    workqueue_congested(ps->cpu, ps->ppd->hfi1_wq)) {
			spin_lock_irqsave(&qp->s_lock, ps->flags);
			if (!tid) {
				qp->s_flags &= ~RVT_S_BUSY;
				hfi1_schedule_send(qp);
			} else {
				struct hfi1_qp_priv *priv = qp->priv;

				if (priv->s_flags &
				    HFI1_S_TID_BUSY_SET) {
					qp->s_flags &= ~RVT_S_BUSY;
					priv->s_flags &=
						~(HFI1_S_TID_BUSY_SET |
						  RVT_S_BUSY);
				} else {
					priv->s_flags &= ~RVT_S_BUSY;
				}
				hfi1_schedule_tid_send(qp);
			}

			spin_unlock_irqrestore(&qp->s_lock, ps->flags);
			this_cpu_inc(*ps->ppd->dd->send_schedule);
			trace_hfi1_rc_expired_time_slice(qp, true);
			return true;
		}

		cond_resched();
		this_cpu_inc(*ps->ppd->dd->send_schedule);
		ps->timeout = jiffies + ps->timeout_int;
	}

	trace_hfi1_rc_expired_time_slice(qp, false);
	return false;
}

void hfi1_do_send_from_rvt(struct rvt_qp *qp)
{
	hfi1_do_send(qp, false);
}

void _hfi1_do_send(struct work_struct *work)
{
	struct iowait_work *w = container_of(work, struct iowait_work, iowork);
	struct rvt_qp *qp = iowait_to_qp(w->iow);

	hfi1_do_send(qp, true);
}

 
void hfi1_do_send(struct rvt_qp *qp, bool in_thread)
{
	struct hfi1_pkt_state ps;
	struct hfi1_qp_priv *priv = qp->priv;
	int (*make_req)(struct rvt_qp *qp, struct hfi1_pkt_state *ps);

	ps.dev = to_idev(qp->ibqp.device);
	ps.ibp = to_iport(qp->ibqp.device, qp->port_num);
	ps.ppd = ppd_from_ibp(ps.ibp);
	ps.in_thread = in_thread;
	ps.wait = iowait_get_ib_work(&priv->s_iowait);

	trace_hfi1_rc_do_send(qp, in_thread);

	switch (qp->ibqp.qp_type) {
	case IB_QPT_RC:
		if (!loopback && ((rdma_ah_get_dlid(&qp->remote_ah_attr) &
				   ~((1 << ps.ppd->lmc) - 1)) ==
				  ps.ppd->lid)) {
			rvt_ruc_loopback(qp);
			return;
		}
		make_req = hfi1_make_rc_req;
		ps.timeout_int = qp->timeout_jiffies;
		break;
	case IB_QPT_UC:
		if (!loopback && ((rdma_ah_get_dlid(&qp->remote_ah_attr) &
				   ~((1 << ps.ppd->lmc) - 1)) ==
				  ps.ppd->lid)) {
			rvt_ruc_loopback(qp);
			return;
		}
		make_req = hfi1_make_uc_req;
		ps.timeout_int = SEND_RESCHED_TIMEOUT;
		break;
	default:
		make_req = hfi1_make_ud_req;
		ps.timeout_int = SEND_RESCHED_TIMEOUT;
	}

	spin_lock_irqsave(&qp->s_lock, ps.flags);

	 
	if (!hfi1_send_ok(qp)) {
		if (qp->s_flags & HFI1_S_ANY_WAIT_IO)
			iowait_set_flag(&priv->s_iowait, IOWAIT_PENDING_IB);
		spin_unlock_irqrestore(&qp->s_lock, ps.flags);
		return;
	}

	qp->s_flags |= RVT_S_BUSY;

	ps.timeout_int = ps.timeout_int / 8;
	ps.timeout = jiffies + ps.timeout_int;
	ps.cpu = priv->s_sde ? priv->s_sde->cpu :
			cpumask_first(cpumask_of_node(ps.ppd->dd->node));
	ps.pkts_sent = false;

	 
	ps.s_txreq = get_waiting_verbs_txreq(ps.wait);
	do {
		 
		if (ps.s_txreq) {
			if (priv->s_flags & HFI1_S_TID_BUSY_SET)
				qp->s_flags |= RVT_S_BUSY;
			spin_unlock_irqrestore(&qp->s_lock, ps.flags);
			 
			if (hfi1_verbs_send(qp, &ps))
				return;

			 
			if (hfi1_schedule_send_yield(qp, &ps, false))
				return;

			spin_lock_irqsave(&qp->s_lock, ps.flags);
		}
	} while (make_req(qp, &ps));
	iowait_starve_clear(ps.pkts_sent, &priv->s_iowait);
	spin_unlock_irqrestore(&qp->s_lock, ps.flags);
}
