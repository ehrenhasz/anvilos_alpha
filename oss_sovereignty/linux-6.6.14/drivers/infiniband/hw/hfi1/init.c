
 

#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/xarray.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/hrtimer.h>
#include <linux/bitmap.h>
#include <linux/numa.h>
#include <rdma/rdma_vt.h>

#include "hfi.h"
#include "device.h"
#include "common.h"
#include "trace.h"
#include "mad.h"
#include "sdma.h"
#include "debugfs.h"
#include "verbs.h"
#include "aspm.h"
#include "affinity.h"
#include "vnic.h"
#include "exp_rcv.h"
#include "netdev.h"

#undef pr_fmt
#define pr_fmt(fmt) DRIVER_NAME ": " fmt

 
#define HFI1_MIN_USER_CTXT_BUFCNT 7

#define HFI1_MIN_EAGER_BUFFER_SIZE (4 * 1024)  
#define HFI1_MAX_EAGER_BUFFER_SIZE (256 * 1024)  

#define NUM_IB_PORTS 1

 
int num_user_contexts = -1;
module_param_named(num_user_contexts, num_user_contexts, int, 0444);
MODULE_PARM_DESC(
	num_user_contexts, "Set max number of user contexts to use (default: -1 will use the real (non-HT) CPU count)");

uint krcvqs[RXE_NUM_DATA_VL];
int krcvqsset;
module_param_array(krcvqs, uint, &krcvqsset, S_IRUGO);
MODULE_PARM_DESC(krcvqs, "Array of the number of non-control kernel receive queues by VL");

 
unsigned long n_krcvqs;

static unsigned hfi1_rcvarr_split = 25;
module_param_named(rcvarr_split, hfi1_rcvarr_split, uint, S_IRUGO);
MODULE_PARM_DESC(rcvarr_split, "Percent of context's RcvArray entries used for Eager buffers");

static uint eager_buffer_size = (8 << 20);  
module_param(eager_buffer_size, uint, S_IRUGO);
MODULE_PARM_DESC(eager_buffer_size, "Size of the eager buffers, default: 8MB");

static uint rcvhdrcnt = 2048;  
module_param_named(rcvhdrcnt, rcvhdrcnt, uint, S_IRUGO);
MODULE_PARM_DESC(rcvhdrcnt, "Receive header queue count (default 2048)");

static uint hfi1_hdrq_entsize = 32;
module_param_named(hdrq_entsize, hfi1_hdrq_entsize, uint, 0444);
MODULE_PARM_DESC(hdrq_entsize, "Size of header queue entries: 2 - 8B, 16 - 64B, 32 - 128B (default)");

unsigned int user_credit_return_threshold = 33;	 
module_param(user_credit_return_threshold, uint, S_IRUGO);
MODULE_PARM_DESC(user_credit_return_threshold, "Credit return threshold for user send contexts, return when unreturned credits passes this many blocks (in percent of allocated blocks, 0 is off)");

DEFINE_XARRAY_FLAGS(hfi1_dev_table, XA_FLAGS_ALLOC | XA_FLAGS_LOCK_IRQ);

static int hfi1_create_kctxt(struct hfi1_devdata *dd,
			     struct hfi1_pportdata *ppd)
{
	struct hfi1_ctxtdata *rcd;
	int ret;

	 
	BUILD_BUG_ON(HFI1_CTRL_CTXT != 0);

	ret = hfi1_create_ctxtdata(ppd, dd->node, &rcd);
	if (ret < 0) {
		dd_dev_err(dd, "Kernel receive context allocation failed\n");
		return ret;
	}

	 
	rcd->flags = HFI1_CAP_KGET(MULTI_PKT_EGR) |
		HFI1_CAP_KGET(NODROP_RHQ_FULL) |
		HFI1_CAP_KGET(NODROP_EGR_FULL) |
		HFI1_CAP_KGET(DMA_RTAIL);

	 
	if (rcd->ctxt == HFI1_CTRL_CTXT)
		rcd->flags |= HFI1_CAP_DMA_RTAIL;
	rcd->fast_handler = get_dma_rtail_setting(rcd) ?
				handle_receive_interrupt_dma_rtail :
				handle_receive_interrupt_nodma_rtail;

	hfi1_set_seq_cnt(rcd, 1);

	rcd->sc = sc_alloc(dd, SC_ACK, rcd->rcvhdrqentsize, dd->node);
	if (!rcd->sc) {
		dd_dev_err(dd, "Kernel send context allocation failed\n");
		return -ENOMEM;
	}
	hfi1_init_ctxt(rcd->sc);

	return 0;
}

 
int hfi1_create_kctxts(struct hfi1_devdata *dd)
{
	u16 i;
	int ret;

	dd->rcd = kcalloc_node(dd->num_rcv_contexts, sizeof(*dd->rcd),
			       GFP_KERNEL, dd->node);
	if (!dd->rcd)
		return -ENOMEM;

	for (i = 0; i < dd->first_dyn_alloc_ctxt; ++i) {
		ret = hfi1_create_kctxt(dd, dd->pport);
		if (ret)
			goto bail;
	}

	return 0;
bail:
	for (i = 0; dd->rcd && i < dd->first_dyn_alloc_ctxt; ++i)
		hfi1_free_ctxt(dd->rcd[i]);

	 
	kfree(dd->rcd);
	dd->rcd = NULL;
	return ret;
}

 
static void hfi1_rcd_init(struct hfi1_ctxtdata *rcd)
{
	kref_init(&rcd->kref);
}

 
static void hfi1_rcd_free(struct kref *kref)
{
	unsigned long flags;
	struct hfi1_ctxtdata *rcd =
		container_of(kref, struct hfi1_ctxtdata, kref);

	spin_lock_irqsave(&rcd->dd->uctxt_lock, flags);
	rcd->dd->rcd[rcd->ctxt] = NULL;
	spin_unlock_irqrestore(&rcd->dd->uctxt_lock, flags);

	hfi1_free_ctxtdata(rcd->dd, rcd);

	kfree(rcd);
}

 
int hfi1_rcd_put(struct hfi1_ctxtdata *rcd)
{
	if (rcd)
		return kref_put(&rcd->kref, hfi1_rcd_free);

	return 0;
}

 
int hfi1_rcd_get(struct hfi1_ctxtdata *rcd)
{
	return kref_get_unless_zero(&rcd->kref);
}

 
static int allocate_rcd_index(struct hfi1_devdata *dd,
			      struct hfi1_ctxtdata *rcd, u16 *index)
{
	unsigned long flags;
	u16 ctxt;

	spin_lock_irqsave(&dd->uctxt_lock, flags);
	for (ctxt = 0; ctxt < dd->num_rcv_contexts; ctxt++)
		if (!dd->rcd[ctxt])
			break;

	if (ctxt < dd->num_rcv_contexts) {
		rcd->ctxt = ctxt;
		dd->rcd[ctxt] = rcd;
		hfi1_rcd_init(rcd);
	}
	spin_unlock_irqrestore(&dd->uctxt_lock, flags);

	if (ctxt >= dd->num_rcv_contexts)
		return -EBUSY;

	*index = ctxt;

	return 0;
}

 
struct hfi1_ctxtdata *hfi1_rcd_get_by_index_safe(struct hfi1_devdata *dd,
						 u16 ctxt)
{
	if (ctxt < dd->num_rcv_contexts)
		return hfi1_rcd_get_by_index(dd, ctxt);

	return NULL;
}

 
struct hfi1_ctxtdata *hfi1_rcd_get_by_index(struct hfi1_devdata *dd, u16 ctxt)
{
	unsigned long flags;
	struct hfi1_ctxtdata *rcd = NULL;

	spin_lock_irqsave(&dd->uctxt_lock, flags);
	if (dd->rcd[ctxt]) {
		rcd = dd->rcd[ctxt];
		if (!hfi1_rcd_get(rcd))
			rcd = NULL;
	}
	spin_unlock_irqrestore(&dd->uctxt_lock, flags);

	return rcd;
}

 
int hfi1_create_ctxtdata(struct hfi1_pportdata *ppd, int numa,
			 struct hfi1_ctxtdata **context)
{
	struct hfi1_devdata *dd = ppd->dd;
	struct hfi1_ctxtdata *rcd;
	unsigned kctxt_ngroups = 0;
	u32 base;

	if (dd->rcv_entries.nctxt_extra >
	    dd->num_rcv_contexts - dd->first_dyn_alloc_ctxt)
		kctxt_ngroups = (dd->rcv_entries.nctxt_extra -
			 (dd->num_rcv_contexts - dd->first_dyn_alloc_ctxt));
	rcd = kzalloc_node(sizeof(*rcd), GFP_KERNEL, numa);
	if (rcd) {
		u32 rcvtids, max_entries;
		u16 ctxt;
		int ret;

		ret = allocate_rcd_index(dd, rcd, &ctxt);
		if (ret) {
			*context = NULL;
			kfree(rcd);
			return ret;
		}

		INIT_LIST_HEAD(&rcd->qp_wait_list);
		hfi1_exp_tid_group_init(rcd);
		rcd->ppd = ppd;
		rcd->dd = dd;
		rcd->numa_id = numa;
		rcd->rcv_array_groups = dd->rcv_entries.ngroups;
		rcd->rhf_rcv_function_map = normal_rhf_rcv_functions;
		rcd->slow_handler = handle_receive_interrupt;
		rcd->do_interrupt = rcd->slow_handler;
		rcd->msix_intr = CCE_NUM_MSIX_VECTORS;

		mutex_init(&rcd->exp_mutex);
		spin_lock_init(&rcd->exp_lock);
		INIT_LIST_HEAD(&rcd->flow_queue.queue_head);
		INIT_LIST_HEAD(&rcd->rarr_queue.queue_head);

		hfi1_cdbg(PROC, "setting up context %u", rcd->ctxt);

		 
		if (ctxt < dd->first_dyn_alloc_ctxt) {
			if (ctxt < kctxt_ngroups) {
				base = ctxt * (dd->rcv_entries.ngroups + 1);
				rcd->rcv_array_groups++;
			} else {
				base = kctxt_ngroups +
					(ctxt * dd->rcv_entries.ngroups);
			}
		} else {
			u16 ct = ctxt - dd->first_dyn_alloc_ctxt;

			base = ((dd->n_krcv_queues * dd->rcv_entries.ngroups) +
				kctxt_ngroups);
			if (ct < dd->rcv_entries.nctxt_extra) {
				base += ct * (dd->rcv_entries.ngroups + 1);
				rcd->rcv_array_groups++;
			} else {
				base += dd->rcv_entries.nctxt_extra +
					(ct * dd->rcv_entries.ngroups);
			}
		}
		rcd->eager_base = base * dd->rcv_entries.group_size;

		rcd->rcvhdrq_cnt = rcvhdrcnt;
		rcd->rcvhdrqentsize = hfi1_hdrq_entsize;
		rcd->rhf_offset =
			rcd->rcvhdrqentsize - sizeof(u64) / sizeof(u32);
		 
		max_entries = rcd->rcv_array_groups *
			dd->rcv_entries.group_size;
		rcvtids = ((max_entries * hfi1_rcvarr_split) / 100);
		rcd->egrbufs.count = round_down(rcvtids,
						dd->rcv_entries.group_size);
		if (rcd->egrbufs.count > MAX_EAGER_ENTRIES) {
			dd_dev_err(dd, "ctxt%u: requested too many RcvArray entries.\n",
				   rcd->ctxt);
			rcd->egrbufs.count = MAX_EAGER_ENTRIES;
		}
		hfi1_cdbg(PROC,
			  "ctxt%u: max Eager buffer RcvArray entries: %u",
			  rcd->ctxt, rcd->egrbufs.count);

		 
		rcd->egrbufs.buffers =
			kcalloc_node(rcd->egrbufs.count,
				     sizeof(*rcd->egrbufs.buffers),
				     GFP_KERNEL, numa);
		if (!rcd->egrbufs.buffers)
			goto bail;
		rcd->egrbufs.rcvtids =
			kcalloc_node(rcd->egrbufs.count,
				     sizeof(*rcd->egrbufs.rcvtids),
				     GFP_KERNEL, numa);
		if (!rcd->egrbufs.rcvtids)
			goto bail;
		rcd->egrbufs.size = eager_buffer_size;
		 
		if (rcd->egrbufs.size < hfi1_max_mtu) {
			rcd->egrbufs.size = __roundup_pow_of_two(hfi1_max_mtu);
			hfi1_cdbg(PROC,
				  "ctxt%u: eager bufs size too small. Adjusting to %u",
				    rcd->ctxt, rcd->egrbufs.size);
		}
		rcd->egrbufs.rcvtid_size = HFI1_MAX_EAGER_BUFFER_SIZE;

		 
		if (ctxt < dd->first_dyn_alloc_ctxt) {
			rcd->opstats = kzalloc_node(sizeof(*rcd->opstats),
						    GFP_KERNEL, numa);
			if (!rcd->opstats)
				goto bail;

			 
			hfi1_kern_init_ctxt_generations(rcd);
		}

		*context = rcd;
		return 0;
	}

bail:
	*context = NULL;
	hfi1_free_ctxt(rcd);
	return -ENOMEM;
}

 
void hfi1_free_ctxt(struct hfi1_ctxtdata *rcd)
{
	hfi1_rcd_put(rcd);
}

 
void set_link_ipg(struct hfi1_pportdata *ppd)
{
	struct hfi1_devdata *dd = ppd->dd;
	struct cc_state *cc_state;
	int i;
	u16 cce, ccti_limit, max_ccti = 0;
	u16 shift, mult;
	u64 src;
	u32 current_egress_rate;  
	u64 max_pkt_time;
	 

	cc_state = get_cc_state(ppd);

	if (!cc_state)
		 
		return;

	for (i = 0; i < OPA_MAX_SLS; i++) {
		u16 ccti = ppd->cca_timer[i].ccti;

		if (ccti > max_ccti)
			max_ccti = ccti;
	}

	ccti_limit = cc_state->cct.ccti_limit;
	if (max_ccti > ccti_limit)
		max_ccti = ccti_limit;

	cce = cc_state->cct.entries[max_ccti].entry;
	shift = (cce & 0xc000) >> 14;
	mult = (cce & 0x3fff);

	current_egress_rate = active_egress_rate(ppd);

	max_pkt_time = egress_cycles(ppd->ibmaxlen, current_egress_rate);

	src = (max_pkt_time >> shift) * mult;

	src &= SEND_STATIC_RATE_CONTROL_CSR_SRC_RELOAD_SMASK;
	src <<= SEND_STATIC_RATE_CONTROL_CSR_SRC_RELOAD_SHIFT;

	write_csr(dd, SEND_STATIC_RATE_CONTROL, src);
}

static enum hrtimer_restart cca_timer_fn(struct hrtimer *t)
{
	struct cca_timer *cca_timer;
	struct hfi1_pportdata *ppd;
	int sl;
	u16 ccti_timer, ccti_min;
	struct cc_state *cc_state;
	unsigned long flags;
	enum hrtimer_restart ret = HRTIMER_NORESTART;

	cca_timer = container_of(t, struct cca_timer, hrtimer);
	ppd = cca_timer->ppd;
	sl = cca_timer->sl;

	rcu_read_lock();

	cc_state = get_cc_state(ppd);

	if (!cc_state) {
		rcu_read_unlock();
		return HRTIMER_NORESTART;
	}

	 

	ccti_min = cc_state->cong_setting.entries[sl].ccti_min;
	ccti_timer = cc_state->cong_setting.entries[sl].ccti_timer;

	spin_lock_irqsave(&ppd->cca_timer_lock, flags);

	if (cca_timer->ccti > ccti_min) {
		cca_timer->ccti--;
		set_link_ipg(ppd);
	}

	if (cca_timer->ccti > ccti_min) {
		unsigned long nsec = 1024 * ccti_timer;
		 
		hrtimer_forward_now(t, ns_to_ktime(nsec));
		ret = HRTIMER_RESTART;
	}

	spin_unlock_irqrestore(&ppd->cca_timer_lock, flags);
	rcu_read_unlock();
	return ret;
}

 
void hfi1_init_pportdata(struct pci_dev *pdev, struct hfi1_pportdata *ppd,
			 struct hfi1_devdata *dd, u8 hw_pidx, u32 port)
{
	int i;
	uint default_pkey_idx;
	struct cc_state *cc_state;

	ppd->dd = dd;
	ppd->hw_pidx = hw_pidx;
	ppd->port = port;  
	ppd->prev_link_width = LINK_WIDTH_DEFAULT;
	 
	for (i = 0; i < C_VL_COUNT + 1; i++) {
		ppd->port_vl_xmit_wait_last[i] = 0;
		ppd->vl_xmit_flit_cnt[i] = 0;
	}

	default_pkey_idx = 1;

	ppd->pkeys[default_pkey_idx] = DEFAULT_P_KEY;
	ppd->part_enforce |= HFI1_PART_ENFORCE_IN;
	ppd->pkeys[0] = 0x8001;

	INIT_WORK(&ppd->link_vc_work, handle_verify_cap);
	INIT_WORK(&ppd->link_up_work, handle_link_up);
	INIT_WORK(&ppd->link_down_work, handle_link_down);
	INIT_WORK(&ppd->freeze_work, handle_freeze);
	INIT_WORK(&ppd->link_downgrade_work, handle_link_downgrade);
	INIT_WORK(&ppd->sma_message_work, handle_sma_message);
	INIT_WORK(&ppd->link_bounce_work, handle_link_bounce);
	INIT_DELAYED_WORK(&ppd->start_link_work, handle_start_link);
	INIT_WORK(&ppd->linkstate_active_work, receive_interrupt_work);
	INIT_WORK(&ppd->qsfp_info.qsfp_work, qsfp_event);

	mutex_init(&ppd->hls_lock);
	spin_lock_init(&ppd->qsfp_info.qsfp_lock);

	ppd->qsfp_info.ppd = ppd;
	ppd->sm_trap_qp = 0x0;
	ppd->sa_qp = 0x1;

	ppd->hfi1_wq = NULL;

	spin_lock_init(&ppd->cca_timer_lock);

	for (i = 0; i < OPA_MAX_SLS; i++) {
		hrtimer_init(&ppd->cca_timer[i].hrtimer, CLOCK_MONOTONIC,
			     HRTIMER_MODE_REL);
		ppd->cca_timer[i].ppd = ppd;
		ppd->cca_timer[i].sl = i;
		ppd->cca_timer[i].ccti = 0;
		ppd->cca_timer[i].hrtimer.function = cca_timer_fn;
	}

	ppd->cc_max_table_entries = IB_CC_TABLE_CAP_DEFAULT;

	spin_lock_init(&ppd->cc_state_lock);
	spin_lock_init(&ppd->cc_log_lock);
	cc_state = kzalloc(sizeof(*cc_state), GFP_KERNEL);
	RCU_INIT_POINTER(ppd->cc_state, cc_state);
	if (!cc_state)
		goto bail;
	return;

bail:
	dd_dev_err(dd, "Congestion Control Agent disabled for port %d\n", port);
}

 
static int loadtime_init(struct hfi1_devdata *dd)
{
	return 0;
}

 
static int init_after_reset(struct hfi1_devdata *dd)
{
	int i;
	struct hfi1_ctxtdata *rcd;
	 
	for (i = 0; i < dd->num_rcv_contexts; i++) {
		rcd = hfi1_rcd_get_by_index(dd, i);
		hfi1_rcvctrl(dd, HFI1_RCVCTRL_CTXT_DIS |
			     HFI1_RCVCTRL_INTRAVAIL_DIS |
			     HFI1_RCVCTRL_TAILUPD_DIS, rcd);
		hfi1_rcd_put(rcd);
	}
	pio_send_control(dd, PSC_GLOBAL_DISABLE);
	for (i = 0; i < dd->num_send_contexts; i++)
		sc_disable(dd->send_contexts[i].sc);

	return 0;
}

static void enable_chip(struct hfi1_devdata *dd)
{
	struct hfi1_ctxtdata *rcd;
	u32 rcvmask;
	u16 i;

	 
	pio_send_control(dd, PSC_GLOBAL_ENABLE);

	 
	for (i = 0; i < dd->first_dyn_alloc_ctxt; ++i) {
		rcd = hfi1_rcd_get_by_index(dd, i);
		if (!rcd)
			continue;
		rcvmask = HFI1_RCVCTRL_CTXT_ENB | HFI1_RCVCTRL_INTRAVAIL_ENB;
		rcvmask |= HFI1_CAP_KGET_MASK(rcd->flags, DMA_RTAIL) ?
			HFI1_RCVCTRL_TAILUPD_ENB : HFI1_RCVCTRL_TAILUPD_DIS;
		if (!HFI1_CAP_KGET_MASK(rcd->flags, MULTI_PKT_EGR))
			rcvmask |= HFI1_RCVCTRL_ONE_PKT_EGR_ENB;
		if (HFI1_CAP_KGET_MASK(rcd->flags, NODROP_RHQ_FULL))
			rcvmask |= HFI1_RCVCTRL_NO_RHQ_DROP_ENB;
		if (HFI1_CAP_KGET_MASK(rcd->flags, NODROP_EGR_FULL))
			rcvmask |= HFI1_RCVCTRL_NO_EGR_DROP_ENB;
		if (HFI1_CAP_IS_KSET(TID_RDMA))
			rcvmask |= HFI1_RCVCTRL_TIDFLOW_ENB;
		hfi1_rcvctrl(dd, rcvmask, rcd);
		sc_enable(rcd->sc);
		hfi1_rcd_put(rcd);
	}
}

 
static int create_workqueues(struct hfi1_devdata *dd)
{
	int pidx;
	struct hfi1_pportdata *ppd;

	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		ppd = dd->pport + pidx;
		if (!ppd->hfi1_wq) {
			ppd->hfi1_wq =
				alloc_workqueue(
				    "hfi%d_%d",
				    WQ_SYSFS | WQ_HIGHPRI | WQ_CPU_INTENSIVE |
				    WQ_MEM_RECLAIM,
				    HFI1_MAX_ACTIVE_WORKQUEUE_ENTRIES,
				    dd->unit, pidx);
			if (!ppd->hfi1_wq)
				goto wq_error;
		}
		if (!ppd->link_wq) {
			 
			ppd->link_wq =
				alloc_workqueue(
				    "hfi_link_%d_%d",
				    WQ_SYSFS | WQ_MEM_RECLAIM | WQ_UNBOUND,
				    1,  
				    dd->unit, pidx);
			if (!ppd->link_wq)
				goto wq_error;
		}
	}
	return 0;
wq_error:
	pr_err("alloc_workqueue failed for port %d\n", pidx + 1);
	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		ppd = dd->pport + pidx;
		if (ppd->hfi1_wq) {
			destroy_workqueue(ppd->hfi1_wq);
			ppd->hfi1_wq = NULL;
		}
		if (ppd->link_wq) {
			destroy_workqueue(ppd->link_wq);
			ppd->link_wq = NULL;
		}
	}
	return -ENOMEM;
}

 
static void destroy_workqueues(struct hfi1_devdata *dd)
{
	int pidx;
	struct hfi1_pportdata *ppd;

	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		ppd = dd->pport + pidx;

		if (ppd->hfi1_wq) {
			destroy_workqueue(ppd->hfi1_wq);
			ppd->hfi1_wq = NULL;
		}
		if (ppd->link_wq) {
			destroy_workqueue(ppd->link_wq);
			ppd->link_wq = NULL;
		}
	}
}

 
static void enable_general_intr(struct hfi1_devdata *dd)
{
	set_intr_bits(dd, CCE_ERR_INT, MISC_ERR_INT, true);
	set_intr_bits(dd, PIO_ERR_INT, TXE_ERR_INT, true);
	set_intr_bits(dd, IS_SENDCTXT_ERR_START, IS_SENDCTXT_ERR_END, true);
	set_intr_bits(dd, PBC_INT, GPIO_ASSERT_INT, true);
	set_intr_bits(dd, TCRIT_INT, TCRIT_INT, true);
	set_intr_bits(dd, IS_DC_START, IS_DC_END, true);
	set_intr_bits(dd, IS_SENDCREDIT_START, IS_SENDCREDIT_END, true);
}

 
int hfi1_init(struct hfi1_devdata *dd, int reinit)
{
	int ret = 0, pidx, lastfail = 0;
	unsigned long len;
	u16 i;
	struct hfi1_ctxtdata *rcd;
	struct hfi1_pportdata *ppd;

	 
	dd->process_pio_send = hfi1_verbs_send_pio;
	dd->process_dma_send = hfi1_verbs_send_dma;
	dd->pio_inline_send = pio_copy;
	dd->process_vnic_dma_send = hfi1_vnic_send_dma;

	if (is_ax(dd)) {
		atomic_set(&dd->drop_packet, DROP_PACKET_ON);
		dd->do_drop = true;
	} else {
		atomic_set(&dd->drop_packet, DROP_PACKET_OFF);
		dd->do_drop = false;
	}

	 
	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		ppd = dd->pport + pidx;
		ppd->linkup = 0;
	}

	if (reinit)
		ret = init_after_reset(dd);
	else
		ret = loadtime_init(dd);
	if (ret)
		goto done;

	 
	for (i = 0; dd->rcd && i < dd->first_dyn_alloc_ctxt; ++i) {
		 
		rcd = hfi1_rcd_get_by_index(dd, i);
		if (!rcd)
			continue;

		lastfail = hfi1_create_rcvhdrq(dd, rcd);
		if (!lastfail)
			lastfail = hfi1_setup_eagerbufs(rcd);
		if (!lastfail)
			lastfail = hfi1_kern_exp_rcv_init(rcd, reinit);
		if (lastfail) {
			dd_dev_err(dd,
				   "failed to allocate kernel ctxt's rcvhdrq and/or egr bufs\n");
			ret = lastfail;
		}
		 
		hfi1_rcd_put(rcd);
	}

	 
	len = PAGE_ALIGN(chip_rcv_contexts(dd) * HFI1_MAX_SHARED_CTXTS *
			 sizeof(*dd->events));
	dd->events = vmalloc_user(len);
	if (!dd->events)
		dd_dev_err(dd, "Failed to allocate user events page\n");
	 
	dd->status = vmalloc_user(PAGE_SIZE);
	if (!dd->status)
		dd_dev_err(dd, "Failed to allocate dev status page\n");
	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		ppd = dd->pport + pidx;
		if (dd->status)
			 
			ppd->statusp = &dd->status->port;

		set_mtu(ppd);
	}

	 
	enable_chip(dd);

done:
	 
	if (dd->status)
		dd->status->dev |= HFI1_STATUS_CHIP_PRESENT |
			HFI1_STATUS_INITTED;
	if (!ret) {
		 
		enable_general_intr(dd);
		init_qsfp_int(dd);

		 
		for (pidx = 0; pidx < dd->num_pports; ++pidx) {
			ppd = dd->pport + pidx;

			 
			lastfail = bringup_serdes(ppd);
			if (lastfail)
				dd_dev_info(dd,
					    "Failed to bring up port %u\n",
					    ppd->port);

			 
			if (ppd->statusp)
				*ppd->statusp |= HFI1_STATUS_CHIP_PRESENT |
							HFI1_STATUS_INITTED;
			if (!ppd->link_speed_enabled)
				continue;
		}
	}

	 
	return ret;
}

struct hfi1_devdata *hfi1_lookup(int unit)
{
	return xa_load(&hfi1_dev_table, unit);
}

 
static void stop_timers(struct hfi1_devdata *dd)
{
	struct hfi1_pportdata *ppd;
	int pidx;

	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		ppd = dd->pport + pidx;
		if (ppd->led_override_timer.function) {
			del_timer_sync(&ppd->led_override_timer);
			atomic_set(&ppd->led_override_timer_active, 0);
		}
	}
}

 
static void shutdown_device(struct hfi1_devdata *dd)
{
	struct hfi1_pportdata *ppd;
	struct hfi1_ctxtdata *rcd;
	unsigned pidx;
	int i;

	if (dd->flags & HFI1_SHUTDOWN)
		return;
	dd->flags |= HFI1_SHUTDOWN;

	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		ppd = dd->pport + pidx;

		ppd->linkup = 0;
		if (ppd->statusp)
			*ppd->statusp &= ~(HFI1_STATUS_IB_CONF |
					   HFI1_STATUS_IB_READY);
	}
	dd->flags &= ~HFI1_INITTED;

	 
	set_intr_bits(dd, IS_FIRST_SOURCE, IS_LAST_SOURCE, false);
	msix_clean_up_interrupts(dd);

	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		ppd = dd->pport + pidx;
		for (i = 0; i < dd->num_rcv_contexts; i++) {
			rcd = hfi1_rcd_get_by_index(dd, i);
			hfi1_rcvctrl(dd, HFI1_RCVCTRL_TAILUPD_DIS |
				     HFI1_RCVCTRL_CTXT_DIS |
				     HFI1_RCVCTRL_INTRAVAIL_DIS |
				     HFI1_RCVCTRL_PKEY_DIS |
				     HFI1_RCVCTRL_ONE_PKT_EGR_DIS, rcd);
			hfi1_rcd_put(rcd);
		}
		 
		for (i = 0; i < dd->num_send_contexts; i++)
			sc_flush(dd->send_contexts[i].sc);
	}

	 
	udelay(20);

	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		ppd = dd->pport + pidx;

		 
		for (i = 0; i < dd->num_send_contexts; i++)
			sc_disable(dd->send_contexts[i].sc);
		 
		pio_send_control(dd, PSC_GLOBAL_DISABLE);

		shutdown_led_override(ppd);

		 
		hfi1_quiet_serdes(ppd);
		if (ppd->hfi1_wq)
			flush_workqueue(ppd->hfi1_wq);
		if (ppd->link_wq)
			flush_workqueue(ppd->link_wq);
	}
	sdma_exit(dd);
}

 
void hfi1_free_ctxtdata(struct hfi1_devdata *dd, struct hfi1_ctxtdata *rcd)
{
	u32 e;

	if (!rcd)
		return;

	if (rcd->rcvhdrq) {
		dma_free_coherent(&dd->pcidev->dev, rcvhdrq_size(rcd),
				  rcd->rcvhdrq, rcd->rcvhdrq_dma);
		rcd->rcvhdrq = NULL;
		if (hfi1_rcvhdrtail_kvaddr(rcd)) {
			dma_free_coherent(&dd->pcidev->dev, PAGE_SIZE,
					  (void *)hfi1_rcvhdrtail_kvaddr(rcd),
					  rcd->rcvhdrqtailaddr_dma);
			rcd->rcvhdrtail_kvaddr = NULL;
		}
	}

	 
	kfree(rcd->egrbufs.rcvtids);
	rcd->egrbufs.rcvtids = NULL;

	for (e = 0; e < rcd->egrbufs.alloced; e++) {
		if (rcd->egrbufs.buffers[e].addr)
			dma_free_coherent(&dd->pcidev->dev,
					  rcd->egrbufs.buffers[e].len,
					  rcd->egrbufs.buffers[e].addr,
					  rcd->egrbufs.buffers[e].dma);
	}
	kfree(rcd->egrbufs.buffers);
	rcd->egrbufs.alloced = 0;
	rcd->egrbufs.buffers = NULL;

	sc_free(rcd->sc);
	rcd->sc = NULL;

	vfree(rcd->subctxt_uregbase);
	vfree(rcd->subctxt_rcvegrbuf);
	vfree(rcd->subctxt_rcvhdr_base);
	kfree(rcd->opstats);

	rcd->subctxt_uregbase = NULL;
	rcd->subctxt_rcvegrbuf = NULL;
	rcd->subctxt_rcvhdr_base = NULL;
	rcd->opstats = NULL;
}

 
static struct hfi1_asic_data *release_asic_data(struct hfi1_devdata *dd)
{
	struct hfi1_asic_data *ad;
	int other;

	if (!dd->asic_data)
		return NULL;
	dd->asic_data->dds[dd->hfi1_id] = NULL;
	other = dd->hfi1_id ? 0 : 1;
	ad = dd->asic_data;
	dd->asic_data = NULL;
	 
	return ad->dds[other] ? NULL : ad;
}

static void finalize_asic_data(struct hfi1_devdata *dd,
			       struct hfi1_asic_data *ad)
{
	clean_up_i2c(dd, ad);
	kfree(ad);
}

 
void hfi1_free_devdata(struct hfi1_devdata *dd)
{
	struct hfi1_asic_data *ad;
	unsigned long flags;

	xa_lock_irqsave(&hfi1_dev_table, flags);
	__xa_erase(&hfi1_dev_table, dd->unit);
	ad = release_asic_data(dd);
	xa_unlock_irqrestore(&hfi1_dev_table, flags);

	finalize_asic_data(dd, ad);
	free_platform_config(dd);
	rcu_barrier();  
	free_percpu(dd->int_counter);
	free_percpu(dd->rcv_limit);
	free_percpu(dd->send_schedule);
	free_percpu(dd->tx_opstats);
	dd->int_counter   = NULL;
	dd->rcv_limit     = NULL;
	dd->send_schedule = NULL;
	dd->tx_opstats    = NULL;
	kfree(dd->comp_vect);
	dd->comp_vect = NULL;
	if (dd->rcvhdrtail_dummy_kvaddr)
		dma_free_coherent(&dd->pcidev->dev, sizeof(u64),
				  (void *)dd->rcvhdrtail_dummy_kvaddr,
				  dd->rcvhdrtail_dummy_dma);
	dd->rcvhdrtail_dummy_kvaddr = NULL;
	sdma_clean(dd, dd->num_sdma);
	rvt_dealloc_device(&dd->verbs_dev.rdi);
}

 
static struct hfi1_devdata *hfi1_alloc_devdata(struct pci_dev *pdev,
					       size_t extra)
{
	struct hfi1_devdata *dd;
	int ret, nports;

	 
	nports = extra / sizeof(struct hfi1_pportdata);

	dd = (struct hfi1_devdata *)rvt_alloc_device(sizeof(*dd) + extra,
						     nports);
	if (!dd)
		return ERR_PTR(-ENOMEM);
	dd->num_pports = nports;
	dd->pport = (struct hfi1_pportdata *)(dd + 1);
	dd->pcidev = pdev;
	pci_set_drvdata(pdev, dd);

	ret = xa_alloc_irq(&hfi1_dev_table, &dd->unit, dd, xa_limit_32b,
			GFP_KERNEL);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Could not allocate unit ID: error %d\n", -ret);
		goto bail;
	}
	rvt_set_ibdev_name(&dd->verbs_dev.rdi, "%s_%d", class_name(), dd->unit);
	 
	dd->node = pcibus_to_node(pdev->bus);
	if (dd->node == NUMA_NO_NODE) {
		dd_dev_err(dd, "Invalid PCI NUMA node. Performance may be affected\n");
		dd->node = 0;
	}

	 
	spin_lock_init(&dd->sc_lock);
	spin_lock_init(&dd->sendctrl_lock);
	spin_lock_init(&dd->rcvctrl_lock);
	spin_lock_init(&dd->uctxt_lock);
	spin_lock_init(&dd->hfi1_diag_trans_lock);
	spin_lock_init(&dd->sc_init_lock);
	spin_lock_init(&dd->dc8051_memlock);
	seqlock_init(&dd->sc2vl_lock);
	spin_lock_init(&dd->sde_map_lock);
	spin_lock_init(&dd->pio_map_lock);
	mutex_init(&dd->dc8051_lock);
	init_waitqueue_head(&dd->event_queue);
	spin_lock_init(&dd->irq_src_lock);

	dd->int_counter = alloc_percpu(u64);
	if (!dd->int_counter) {
		ret = -ENOMEM;
		goto bail;
	}

	dd->rcv_limit = alloc_percpu(u64);
	if (!dd->rcv_limit) {
		ret = -ENOMEM;
		goto bail;
	}

	dd->send_schedule = alloc_percpu(u64);
	if (!dd->send_schedule) {
		ret = -ENOMEM;
		goto bail;
	}

	dd->tx_opstats = alloc_percpu(struct hfi1_opcode_stats_perctx);
	if (!dd->tx_opstats) {
		ret = -ENOMEM;
		goto bail;
	}

	dd->comp_vect = kzalloc(sizeof(*dd->comp_vect), GFP_KERNEL);
	if (!dd->comp_vect) {
		ret = -ENOMEM;
		goto bail;
	}

	 
	dd->rcvhdrtail_dummy_kvaddr =
		dma_alloc_coherent(&dd->pcidev->dev, sizeof(u64),
				   &dd->rcvhdrtail_dummy_dma, GFP_KERNEL);
	if (!dd->rcvhdrtail_dummy_kvaddr) {
		ret = -ENOMEM;
		goto bail;
	}

	atomic_set(&dd->ipoib_rsm_usr_num, 0);
	return dd;

bail:
	hfi1_free_devdata(dd);
	return ERR_PTR(ret);
}

 
void hfi1_disable_after_error(struct hfi1_devdata *dd)
{
	if (dd->flags & HFI1_INITTED) {
		u32 pidx;

		dd->flags &= ~HFI1_INITTED;
		if (dd->pport)
			for (pidx = 0; pidx < dd->num_pports; ++pidx) {
				struct hfi1_pportdata *ppd;

				ppd = dd->pport + pidx;
				if (dd->flags & HFI1_PRESENT)
					set_link_state(ppd, HLS_DN_DISABLE);

				if (ppd->statusp)
					*ppd->statusp &= ~HFI1_STATUS_IB_READY;
			}
	}

	 
	if (dd->status)
		dd->status->dev |= HFI1_STATUS_HWERROR;
}

static void remove_one(struct pci_dev *);
static int init_one(struct pci_dev *, const struct pci_device_id *);
static void shutdown_one(struct pci_dev *);

#define DRIVER_LOAD_MSG "Cornelis " DRIVER_NAME " loaded: "
#define PFX DRIVER_NAME ": "

const struct pci_device_id hfi1_pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL0) },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL1) },
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, hfi1_pci_tbl);

static struct pci_driver hfi1_pci_driver = {
	.name = DRIVER_NAME,
	.probe = init_one,
	.remove = remove_one,
	.shutdown = shutdown_one,
	.id_table = hfi1_pci_tbl,
	.err_handler = &hfi1_pci_err_handler,
};

static void __init compute_krcvqs(void)
{
	int i;

	for (i = 0; i < krcvqsset; i++)
		n_krcvqs += krcvqs[i];
}

 
static int __init hfi1_mod_init(void)
{
	int ret;

	ret = dev_init();
	if (ret)
		goto bail;

	ret = node_affinity_init();
	if (ret)
		goto bail;

	 
	if (!valid_opa_max_mtu(hfi1_max_mtu)) {
		pr_err("Invalid max_mtu 0x%x, using 0x%x instead\n",
		       hfi1_max_mtu, HFI1_DEFAULT_MAX_MTU);
		hfi1_max_mtu = HFI1_DEFAULT_MAX_MTU;
	}
	 
	if (hfi1_cu > 128 || !is_power_of_2(hfi1_cu))
		hfi1_cu = 1;
	 
	if (user_credit_return_threshold > 100)
		user_credit_return_threshold = 100;

	compute_krcvqs();
	 
	if (rcv_intr_count > RCV_HDR_HEAD_COUNTER_MASK)
		rcv_intr_count = RCV_HDR_HEAD_COUNTER_MASK;
	 
	if (rcv_intr_count == 0 && rcv_intr_timeout == 0) {
		pr_err("Invalid mode: both receive interrupt count and available timeout are zero - setting interrupt count to 1\n");
		rcv_intr_count = 1;
	}
	if (rcv_intr_count > 1 && rcv_intr_timeout == 0) {
		 
		pr_err("Invalid mode: receive interrupt count greater than 1 and available timeout is zero - setting available timeout to 1\n");
		rcv_intr_timeout = 1;
	}
	if (rcv_intr_dynamic && !(rcv_intr_count > 1 && rcv_intr_timeout > 0)) {
		 
		pr_err("Invalid mode: dynamic receive interrupt mitigation with invalid count and timeout - turning dynamic off\n");
		rcv_intr_dynamic = 0;
	}

	 
	link_crc_mask &= SUPPORTED_CRCS;

	ret = opfn_init();
	if (ret < 0) {
		pr_err("Failed to allocate opfn_wq");
		goto bail_dev;
	}

	 
	hfi1_dbg_init();
	ret = pci_register_driver(&hfi1_pci_driver);
	if (ret < 0) {
		pr_err("Unable to register driver: error %d\n", -ret);
		goto bail_dev;
	}
	goto bail;  

bail_dev:
	hfi1_dbg_exit();
	dev_cleanup();
bail:
	return ret;
}

module_init(hfi1_mod_init);

 
static void __exit hfi1_mod_cleanup(void)
{
	pci_unregister_driver(&hfi1_pci_driver);
	opfn_exit();
	node_affinity_destroy_all();
	hfi1_dbg_exit();

	WARN_ON(!xa_empty(&hfi1_dev_table));
	dispose_firmware();	 
	dev_cleanup();
}

module_exit(hfi1_mod_cleanup);

 
static void cleanup_device_data(struct hfi1_devdata *dd)
{
	int ctxt;
	int pidx;

	 
	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		struct hfi1_pportdata *ppd = &dd->pport[pidx];
		struct cc_state *cc_state;
		int i;

		if (ppd->statusp)
			*ppd->statusp &= ~HFI1_STATUS_CHIP_PRESENT;

		for (i = 0; i < OPA_MAX_SLS; i++)
			hrtimer_cancel(&ppd->cca_timer[i].hrtimer);

		spin_lock(&ppd->cc_state_lock);
		cc_state = get_cc_state_protected(ppd);
		RCU_INIT_POINTER(ppd->cc_state, NULL);
		spin_unlock(&ppd->cc_state_lock);

		if (cc_state)
			kfree_rcu(cc_state, rcu);
	}

	free_credit_return(dd);

	 
	for (ctxt = 0; dd->rcd && ctxt < dd->num_rcv_contexts; ctxt++) {
		struct hfi1_ctxtdata *rcd = dd->rcd[ctxt];

		if (rcd) {
			hfi1_free_ctxt_rcv_groups(rcd);
			hfi1_free_ctxt(rcd);
		}
	}

	kfree(dd->rcd);
	dd->rcd = NULL;

	free_pio_map(dd);
	 
	for (ctxt = 0; ctxt < dd->num_send_contexts; ctxt++)
		sc_free(dd->send_contexts[ctxt].sc);
	dd->num_send_contexts = 0;
	kfree(dd->send_contexts);
	dd->send_contexts = NULL;
	kfree(dd->hw_to_sw);
	dd->hw_to_sw = NULL;
	kfree(dd->boardname);
	vfree(dd->events);
	vfree(dd->status);
}

 
static void postinit_cleanup(struct hfi1_devdata *dd)
{
	hfi1_start_cleanup(dd);
	hfi1_comp_vectors_clean_up(dd);
	hfi1_dev_affinity_clean_up(dd);

	hfi1_pcie_ddcleanup(dd);
	hfi1_pcie_cleanup(dd->pcidev);

	cleanup_device_data(dd);

	hfi1_free_devdata(dd);
}

static int init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int ret = 0, j, pidx, initfail;
	struct hfi1_devdata *dd;
	struct hfi1_pportdata *ppd;

	 
	HFI1_CAP_LOCK();

	 
	if (!(ent->device == PCI_DEVICE_ID_INTEL0 ||
	      ent->device == PCI_DEVICE_ID_INTEL1)) {
		dev_err(&pdev->dev, "Failing on unknown Intel deviceid 0x%x\n",
			ent->device);
		ret = -ENODEV;
		goto bail;
	}

	 
	dd = hfi1_alloc_devdata(pdev, NUM_IB_PORTS *
				sizeof(struct hfi1_pportdata));
	if (IS_ERR(dd)) {
		ret = PTR_ERR(dd);
		goto bail;
	}

	 
	ret = hfi1_validate_rcvhdrcnt(dd, rcvhdrcnt);
	if (ret)
		goto bail;

	 
	if (!encode_rcv_header_entry_size(hfi1_hdrq_entsize)) {
		dd_dev_err(dd, "Invalid HdrQ Entry size %u\n",
			   hfi1_hdrq_entsize);
		ret = -EINVAL;
		goto bail;
	}

	 
	if (eager_buffer_size) {
		if (!is_power_of_2(eager_buffer_size))
			eager_buffer_size =
				roundup_pow_of_two(eager_buffer_size);
		eager_buffer_size =
			clamp_val(eager_buffer_size,
				  MIN_EAGER_BUFFER * 8,
				  MAX_EAGER_BUFFER_TOTAL);
		dd_dev_info(dd, "Eager buffer size %u\n",
			    eager_buffer_size);
	} else {
		dd_dev_err(dd, "Invalid Eager buffer size of 0\n");
		ret = -EINVAL;
		goto bail;
	}

	 
	hfi1_rcvarr_split = clamp_val(hfi1_rcvarr_split, 0, 100);

	ret = hfi1_pcie_init(dd);
	if (ret)
		goto bail;

	 
	ret = hfi1_init_dd(dd);
	if (ret)
		goto clean_bail;  

	ret = create_workqueues(dd);
	if (ret)
		goto clean_bail;

	 
	initfail = hfi1_init(dd, 0);

	ret = hfi1_register_ib_device(dd);

	 
	if (!initfail && !ret) {
		dd->flags |= HFI1_INITTED;
		 
		hfi1_dbg_ibdev_init(&dd->verbs_dev);
	}

	j = hfi1_device_create(dd);
	if (j)
		dd_dev_err(dd, "Failed to create /dev devices: %d\n", -j);

	if (initfail || ret) {
		msix_clean_up_interrupts(dd);
		stop_timers(dd);
		flush_workqueue(ib_wq);
		for (pidx = 0; pidx < dd->num_pports; ++pidx) {
			hfi1_quiet_serdes(dd->pport + pidx);
			ppd = dd->pport + pidx;
			if (ppd->hfi1_wq) {
				destroy_workqueue(ppd->hfi1_wq);
				ppd->hfi1_wq = NULL;
			}
			if (ppd->link_wq) {
				destroy_workqueue(ppd->link_wq);
				ppd->link_wq = NULL;
			}
		}
		if (!j)
			hfi1_device_remove(dd);
		if (!ret)
			hfi1_unregister_ib_device(dd);
		postinit_cleanup(dd);
		if (initfail)
			ret = initfail;
		goto bail;	 
	}

	sdma_start(dd);

	return 0;

clean_bail:
	hfi1_pcie_cleanup(pdev);
bail:
	return ret;
}

static void wait_for_clients(struct hfi1_devdata *dd)
{
	 
	if (refcount_dec_and_test(&dd->user_refcount))
		complete(&dd->user_comp);

	wait_for_completion(&dd->user_comp);
}

static void remove_one(struct pci_dev *pdev)
{
	struct hfi1_devdata *dd = pci_get_drvdata(pdev);

	 
	hfi1_dbg_ibdev_exit(&dd->verbs_dev);

	 
	hfi1_device_remove(dd);

	 
	wait_for_clients(dd);

	 
	hfi1_unregister_ib_device(dd);

	 
	hfi1_free_rx(dd);

	 
	shutdown_device(dd);
	destroy_workqueues(dd);

	stop_timers(dd);

	 
	flush_workqueue(ib_wq);

	postinit_cleanup(dd);
}

static void shutdown_one(struct pci_dev *pdev)
{
	struct hfi1_devdata *dd = pci_get_drvdata(pdev);

	shutdown_device(dd);
}

 
int hfi1_create_rcvhdrq(struct hfi1_devdata *dd, struct hfi1_ctxtdata *rcd)
{
	unsigned amt;

	if (!rcd->rcvhdrq) {
		amt = rcvhdrq_size(rcd);

		rcd->rcvhdrq = dma_alloc_coherent(&dd->pcidev->dev, amt,
						  &rcd->rcvhdrq_dma,
						  GFP_KERNEL);

		if (!rcd->rcvhdrq) {
			dd_dev_err(dd,
				   "attempt to allocate %d bytes for ctxt %u rcvhdrq failed\n",
				   amt, rcd->ctxt);
			goto bail;
		}

		if (HFI1_CAP_KGET_MASK(rcd->flags, DMA_RTAIL) ||
		    HFI1_CAP_UGET_MASK(rcd->flags, DMA_RTAIL)) {
			rcd->rcvhdrtail_kvaddr = dma_alloc_coherent(&dd->pcidev->dev,
								    PAGE_SIZE,
								    &rcd->rcvhdrqtailaddr_dma,
								    GFP_KERNEL);
			if (!rcd->rcvhdrtail_kvaddr)
				goto bail_free;
		}
	}

	set_hdrq_regs(rcd->dd, rcd->ctxt, rcd->rcvhdrqentsize,
		      rcd->rcvhdrq_cnt);

	return 0;

bail_free:
	dd_dev_err(dd,
		   "attempt to allocate 1 page for ctxt %u rcvhdrqtailaddr failed\n",
		   rcd->ctxt);
	dma_free_coherent(&dd->pcidev->dev, amt, rcd->rcvhdrq,
			  rcd->rcvhdrq_dma);
	rcd->rcvhdrq = NULL;
bail:
	return -ENOMEM;
}

 
int hfi1_setup_eagerbufs(struct hfi1_ctxtdata *rcd)
{
	struct hfi1_devdata *dd = rcd->dd;
	u32 max_entries, egrtop, alloced_bytes = 0;
	u16 order, idx = 0;
	int ret = 0;
	u16 round_mtu = roundup_pow_of_two(hfi1_max_mtu);

	 
	if (rcd->egrbufs.size < (round_mtu * dd->rcv_entries.group_size))
		rcd->egrbufs.size = round_mtu * dd->rcv_entries.group_size;
	 
	if (!HFI1_CAP_KGET_MASK(rcd->flags, MULTI_PKT_EGR))
		rcd->egrbufs.rcvtid_size = round_mtu;

	 
	if (rcd->egrbufs.size <= (1 << 20))
		rcd->egrbufs.rcvtid_size = max((unsigned long)round_mtu,
			rounddown_pow_of_two(rcd->egrbufs.size / 8));

	while (alloced_bytes < rcd->egrbufs.size &&
	       rcd->egrbufs.alloced < rcd->egrbufs.count) {
		rcd->egrbufs.buffers[idx].addr =
			dma_alloc_coherent(&dd->pcidev->dev,
					   rcd->egrbufs.rcvtid_size,
					   &rcd->egrbufs.buffers[idx].dma,
					   GFP_KERNEL);
		if (rcd->egrbufs.buffers[idx].addr) {
			rcd->egrbufs.buffers[idx].len =
				rcd->egrbufs.rcvtid_size;
			rcd->egrbufs.rcvtids[rcd->egrbufs.alloced].addr =
				rcd->egrbufs.buffers[idx].addr;
			rcd->egrbufs.rcvtids[rcd->egrbufs.alloced].dma =
				rcd->egrbufs.buffers[idx].dma;
			rcd->egrbufs.alloced++;
			alloced_bytes += rcd->egrbufs.rcvtid_size;
			idx++;
		} else {
			u32 new_size, i, j;
			u64 offset = 0;

			 
			if (rcd->egrbufs.rcvtid_size == round_mtu ||
			    !HFI1_CAP_KGET_MASK(rcd->flags, MULTI_PKT_EGR)) {
				dd_dev_err(dd, "ctxt%u: Failed to allocate eager buffers\n",
					   rcd->ctxt);
				ret = -ENOMEM;
				goto bail_rcvegrbuf_phys;
			}

			new_size = rcd->egrbufs.rcvtid_size / 2;

			 
			if (idx == 0) {
				rcd->egrbufs.rcvtid_size = new_size;
				continue;
			}

			 
			rcd->egrbufs.alloced = 0;
			for (i = 0, j = 0, offset = 0; j < idx; i++) {
				if (i >= rcd->egrbufs.count)
					break;
				rcd->egrbufs.rcvtids[i].dma =
					rcd->egrbufs.buffers[j].dma + offset;
				rcd->egrbufs.rcvtids[i].addr =
					rcd->egrbufs.buffers[j].addr + offset;
				rcd->egrbufs.alloced++;
				if ((rcd->egrbufs.buffers[j].dma + offset +
				     new_size) ==
				    (rcd->egrbufs.buffers[j].dma +
				     rcd->egrbufs.buffers[j].len)) {
					j++;
					offset = 0;
				} else {
					offset += new_size;
				}
			}
			rcd->egrbufs.rcvtid_size = new_size;
		}
	}
	rcd->egrbufs.numbufs = idx;
	rcd->egrbufs.size = alloced_bytes;

	hfi1_cdbg(PROC,
		  "ctxt%u: Alloced %u rcv tid entries @ %uKB, total %uKB",
		  rcd->ctxt, rcd->egrbufs.alloced,
		  rcd->egrbufs.rcvtid_size / 1024, rcd->egrbufs.size / 1024);

	 
	rcd->egrbufs.threshold =
		rounddown_pow_of_two(rcd->egrbufs.alloced / 2);
	 
	max_entries = rcd->rcv_array_groups * dd->rcv_entries.group_size;
	egrtop = roundup(rcd->egrbufs.alloced, dd->rcv_entries.group_size);
	rcd->expected_count = max_entries - egrtop;
	if (rcd->expected_count > MAX_TID_PAIR_ENTRIES * 2)
		rcd->expected_count = MAX_TID_PAIR_ENTRIES * 2;

	rcd->expected_base = rcd->eager_base + egrtop;
	hfi1_cdbg(PROC, "ctxt%u: eager:%u, exp:%u, egrbase:%u, expbase:%u",
		  rcd->ctxt, rcd->egrbufs.alloced, rcd->expected_count,
		  rcd->eager_base, rcd->expected_base);

	if (!hfi1_rcvbuf_validate(rcd->egrbufs.rcvtid_size, PT_EAGER, &order)) {
		hfi1_cdbg(PROC,
			  "ctxt%u: current Eager buffer size is invalid %u",
			  rcd->ctxt, rcd->egrbufs.rcvtid_size);
		ret = -EINVAL;
		goto bail_rcvegrbuf_phys;
	}

	for (idx = 0; idx < rcd->egrbufs.alloced; idx++) {
		hfi1_put_tid(dd, rcd->eager_base + idx, PT_EAGER,
			     rcd->egrbufs.rcvtids[idx].dma, order);
		cond_resched();
	}

	return 0;

bail_rcvegrbuf_phys:
	for (idx = 0; idx < rcd->egrbufs.alloced &&
	     rcd->egrbufs.buffers[idx].addr;
	     idx++) {
		dma_free_coherent(&dd->pcidev->dev,
				  rcd->egrbufs.buffers[idx].len,
				  rcd->egrbufs.buffers[idx].addr,
				  rcd->egrbufs.buffers[idx].dma);
		rcd->egrbufs.buffers[idx].addr = NULL;
		rcd->egrbufs.buffers[idx].dma = 0;
		rcd->egrbufs.buffers[idx].len = 0;
	}

	return ret;
}
