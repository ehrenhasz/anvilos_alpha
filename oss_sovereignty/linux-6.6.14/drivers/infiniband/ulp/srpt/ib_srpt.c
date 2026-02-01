 

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/kthread.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/inet.h>
#include <rdma/ib_cache.h>
#include <scsi/scsi_proto.h>
#include <scsi/scsi_tcq.h>
#include <target/target_core_base.h>
#include <target/target_core_fabric.h>
#include "ib_srpt.h"

 
#define DRV_NAME		"ib_srpt"

#define SRPT_ID_STRING	"Linux SRP target"

#undef pr_fmt
#define pr_fmt(fmt) DRV_NAME " " fmt

MODULE_AUTHOR("Vu Pham and Bart Van Assche");
MODULE_DESCRIPTION("SCSI RDMA Protocol target driver");
MODULE_LICENSE("Dual BSD/GPL");

 

static u64 srpt_service_guid;
static DEFINE_SPINLOCK(srpt_dev_lock);	 
static LIST_HEAD(srpt_dev_list);	 

static unsigned srp_max_req_size = DEFAULT_MAX_REQ_SIZE;
module_param(srp_max_req_size, int, 0444);
MODULE_PARM_DESC(srp_max_req_size,
		 "Maximum size of SRP request messages in bytes.");

static int srpt_srq_size = DEFAULT_SRPT_SRQ_SIZE;
module_param(srpt_srq_size, int, 0444);
MODULE_PARM_DESC(srpt_srq_size,
		 "Shared receive queue (SRQ) size.");

static int srpt_get_u64_x(char *buffer, const struct kernel_param *kp)
{
	return sprintf(buffer, "0x%016llx\n", *(u64 *)kp->arg);
}
module_param_call(srpt_service_guid, NULL, srpt_get_u64_x, &srpt_service_guid,
		  0444);
MODULE_PARM_DESC(srpt_service_guid,
		 "Using this value for ioc_guid, id_ext, and cm_listen_id instead of using the node_guid of the first HCA.");

static struct ib_client srpt_client;
 
static DEFINE_MUTEX(rdma_cm_mutex);
 
static u16 rdma_cm_port;
static struct rdma_cm_id *rdma_cm_id;
static void srpt_release_cmd(struct se_cmd *se_cmd);
static void srpt_free_ch(struct kref *kref);
static int srpt_queue_status(struct se_cmd *cmd);
static void srpt_recv_done(struct ib_cq *cq, struct ib_wc *wc);
static void srpt_send_done(struct ib_cq *cq, struct ib_wc *wc);
static void srpt_process_wait_list(struct srpt_rdma_ch *ch);

 
static bool srpt_set_ch_state(struct srpt_rdma_ch *ch, enum rdma_ch_state new)
{
	unsigned long flags;
	enum rdma_ch_state prev;
	bool changed = false;

	spin_lock_irqsave(&ch->spinlock, flags);
	prev = ch->state;
	if (new > prev) {
		ch->state = new;
		changed = true;
	}
	spin_unlock_irqrestore(&ch->spinlock, flags);

	return changed;
}

 
static void srpt_event_handler(struct ib_event_handler *handler,
			       struct ib_event *event)
{
	struct srpt_device *sdev =
		container_of(handler, struct srpt_device, event_handler);
	struct srpt_port *sport;
	u8 port_num;

	pr_debug("ASYNC event= %d on device= %s\n", event->event,
		 dev_name(&sdev->device->dev));

	switch (event->event) {
	case IB_EVENT_PORT_ERR:
		port_num = event->element.port_num - 1;
		if (port_num < sdev->device->phys_port_cnt) {
			sport = &sdev->port[port_num];
			sport->lid = 0;
			sport->sm_lid = 0;
		} else {
			WARN(true, "event %d: port_num %d out of range 1..%d\n",
			     event->event, port_num + 1,
			     sdev->device->phys_port_cnt);
		}
		break;
	case IB_EVENT_PORT_ACTIVE:
	case IB_EVENT_LID_CHANGE:
	case IB_EVENT_PKEY_CHANGE:
	case IB_EVENT_SM_CHANGE:
	case IB_EVENT_CLIENT_REREGISTER:
	case IB_EVENT_GID_CHANGE:
		 
		port_num = event->element.port_num - 1;
		if (port_num < sdev->device->phys_port_cnt) {
			sport = &sdev->port[port_num];
			if (!sport->lid && !sport->sm_lid)
				schedule_work(&sport->work);
		} else {
			WARN(true, "event %d: port_num %d out of range 1..%d\n",
			     event->event, port_num + 1,
			     sdev->device->phys_port_cnt);
		}
		break;
	default:
		pr_err("received unrecognized IB event %d\n", event->event);
		break;
	}
}

 
static void srpt_srq_event(struct ib_event *event, void *ctx)
{
	pr_debug("SRQ event %d\n", event->event);
}

static const char *get_ch_state_name(enum rdma_ch_state s)
{
	switch (s) {
	case CH_CONNECTING:
		return "connecting";
	case CH_LIVE:
		return "live";
	case CH_DISCONNECTING:
		return "disconnecting";
	case CH_DRAINING:
		return "draining";
	case CH_DISCONNECTED:
		return "disconnected";
	}
	return "???";
}

 
static void srpt_qp_event(struct ib_event *event, struct srpt_rdma_ch *ch)
{
	pr_debug("QP event %d on ch=%p sess_name=%s-%d state=%s\n",
		 event->event, ch, ch->sess_name, ch->qp->qp_num,
		 get_ch_state_name(ch->state));

	switch (event->event) {
	case IB_EVENT_COMM_EST:
		if (ch->using_rdma_cm)
			rdma_notify(ch->rdma_cm.cm_id, event->event);
		else
			ib_cm_notify(ch->ib_cm.cm_id, event->event);
		break;
	case IB_EVENT_QP_LAST_WQE_REACHED:
		pr_debug("%s-%d, state %s: received Last WQE event.\n",
			 ch->sess_name, ch->qp->qp_num,
			 get_ch_state_name(ch->state));
		break;
	default:
		pr_err("received unrecognized IB QP event %d\n", event->event);
		break;
	}
}

 
static void srpt_set_ioc(u8 *c_list, u32 slot, u8 value)
{
	u16 id;
	u8 tmp;

	id = (slot - 1) / 2;
	if (slot & 0x1) {
		tmp = c_list[id] & 0xf;
		c_list[id] = (value << 4) | tmp;
	} else {
		tmp = c_list[id] & 0xf0;
		c_list[id] = (value & 0xf) | tmp;
	}
}

 
static void srpt_get_class_port_info(struct ib_dm_mad *mad)
{
	struct ib_class_port_info *cif;

	cif = (struct ib_class_port_info *)mad->data;
	memset(cif, 0, sizeof(*cif));
	cif->base_version = 1;
	cif->class_version = 1;

	ib_set_cpi_resp_time(cif, 20);
	mad->mad_hdr.status = 0;
}

 
static void srpt_get_iou(struct ib_dm_mad *mad)
{
	struct ib_dm_iou_info *ioui;
	u8 slot;
	int i;

	ioui = (struct ib_dm_iou_info *)mad->data;
	ioui->change_id = cpu_to_be16(1);
	ioui->max_controllers = 16;

	 
	srpt_set_ioc(ioui->controller_list, 1, 1);
	for (i = 1, slot = 2; i < 16; i++, slot++)
		srpt_set_ioc(ioui->controller_list, slot, 0);

	mad->mad_hdr.status = 0;
}

 
static void srpt_get_ioc(struct srpt_port *sport, u32 slot,
			 struct ib_dm_mad *mad)
{
	struct srpt_device *sdev = sport->sdev;
	struct ib_dm_ioc_profile *iocp;
	int send_queue_depth;

	iocp = (struct ib_dm_ioc_profile *)mad->data;

	if (!slot || slot > 16) {
		mad->mad_hdr.status
			= cpu_to_be16(DM_MAD_STATUS_INVALID_FIELD);
		return;
	}

	if (slot > 2) {
		mad->mad_hdr.status
			= cpu_to_be16(DM_MAD_STATUS_NO_IOC);
		return;
	}

	if (sdev->use_srq)
		send_queue_depth = sdev->srq_size;
	else
		send_queue_depth = min(MAX_SRPT_RQ_SIZE,
				       sdev->device->attrs.max_qp_wr);

	memset(iocp, 0, sizeof(*iocp));
	strcpy(iocp->id_string, SRPT_ID_STRING);
	iocp->guid = cpu_to_be64(srpt_service_guid);
	iocp->vendor_id = cpu_to_be32(sdev->device->attrs.vendor_id);
	iocp->device_id = cpu_to_be32(sdev->device->attrs.vendor_part_id);
	iocp->device_version = cpu_to_be16(sdev->device->attrs.hw_ver);
	iocp->subsys_vendor_id = cpu_to_be32(sdev->device->attrs.vendor_id);
	iocp->subsys_device_id = 0x0;
	iocp->io_class = cpu_to_be16(SRP_REV16A_IB_IO_CLASS);
	iocp->io_subclass = cpu_to_be16(SRP_IO_SUBCLASS);
	iocp->protocol = cpu_to_be16(SRP_PROTOCOL);
	iocp->protocol_version = cpu_to_be16(SRP_PROTOCOL_VERSION);
	iocp->send_queue_depth = cpu_to_be16(send_queue_depth);
	iocp->rdma_read_depth = 4;
	iocp->send_size = cpu_to_be32(srp_max_req_size);
	iocp->rdma_size = cpu_to_be32(min(sport->port_attrib.srp_max_rdma_size,
					  1U << 24));
	iocp->num_svc_entries = 1;
	iocp->op_cap_mask = SRP_SEND_TO_IOC | SRP_SEND_FROM_IOC |
		SRP_RDMA_READ_FROM_IOC | SRP_RDMA_WRITE_FROM_IOC;

	mad->mad_hdr.status = 0;
}

 
static void srpt_get_svc_entries(u64 ioc_guid,
				 u16 slot, u8 hi, u8 lo, struct ib_dm_mad *mad)
{
	struct ib_dm_svc_entries *svc_entries;

	WARN_ON(!ioc_guid);

	if (!slot || slot > 16) {
		mad->mad_hdr.status
			= cpu_to_be16(DM_MAD_STATUS_INVALID_FIELD);
		return;
	}

	if (slot > 2 || lo > hi || hi > 1) {
		mad->mad_hdr.status
			= cpu_to_be16(DM_MAD_STATUS_NO_IOC);
		return;
	}

	svc_entries = (struct ib_dm_svc_entries *)mad->data;
	memset(svc_entries, 0, sizeof(*svc_entries));
	svc_entries->service_entries[0].id = cpu_to_be64(ioc_guid);
	snprintf(svc_entries->service_entries[0].name,
		 sizeof(svc_entries->service_entries[0].name),
		 "%s%016llx",
		 SRP_SERVICE_NAME_PREFIX,
		 ioc_guid);

	mad->mad_hdr.status = 0;
}

 
static void srpt_mgmt_method_get(struct srpt_port *sp, struct ib_mad *rq_mad,
				 struct ib_dm_mad *rsp_mad)
{
	u16 attr_id;
	u32 slot;
	u8 hi, lo;

	attr_id = be16_to_cpu(rq_mad->mad_hdr.attr_id);
	switch (attr_id) {
	case DM_ATTR_CLASS_PORT_INFO:
		srpt_get_class_port_info(rsp_mad);
		break;
	case DM_ATTR_IOU_INFO:
		srpt_get_iou(rsp_mad);
		break;
	case DM_ATTR_IOC_PROFILE:
		slot = be32_to_cpu(rq_mad->mad_hdr.attr_mod);
		srpt_get_ioc(sp, slot, rsp_mad);
		break;
	case DM_ATTR_SVC_ENTRIES:
		slot = be32_to_cpu(rq_mad->mad_hdr.attr_mod);
		hi = (u8) ((slot >> 8) & 0xff);
		lo = (u8) (slot & 0xff);
		slot = (u16) ((slot >> 16) & 0xffff);
		srpt_get_svc_entries(srpt_service_guid,
				     slot, hi, lo, rsp_mad);
		break;
	default:
		rsp_mad->mad_hdr.status =
		    cpu_to_be16(DM_MAD_STATUS_UNSUP_METHOD_ATTR);
		break;
	}
}

 
static void srpt_mad_send_handler(struct ib_mad_agent *mad_agent,
				  struct ib_mad_send_wc *mad_wc)
{
	rdma_destroy_ah(mad_wc->send_buf->ah, RDMA_DESTROY_AH_SLEEPABLE);
	ib_free_send_mad(mad_wc->send_buf);
}

 
static void srpt_mad_recv_handler(struct ib_mad_agent *mad_agent,
				  struct ib_mad_send_buf *send_buf,
				  struct ib_mad_recv_wc *mad_wc)
{
	struct srpt_port *sport = (struct srpt_port *)mad_agent->context;
	struct ib_ah *ah;
	struct ib_mad_send_buf *rsp;
	struct ib_dm_mad *dm_mad;

	if (!mad_wc || !mad_wc->recv_buf.mad)
		return;

	ah = ib_create_ah_from_wc(mad_agent->qp->pd, mad_wc->wc,
				  mad_wc->recv_buf.grh, mad_agent->port_num);
	if (IS_ERR(ah))
		goto err;

	BUILD_BUG_ON(offsetof(struct ib_dm_mad, data) != IB_MGMT_DEVICE_HDR);

	rsp = ib_create_send_mad(mad_agent, mad_wc->wc->src_qp,
				 mad_wc->wc->pkey_index, 0,
				 IB_MGMT_DEVICE_HDR, IB_MGMT_DEVICE_DATA,
				 GFP_KERNEL,
				 IB_MGMT_BASE_VERSION);
	if (IS_ERR(rsp))
		goto err_rsp;

	rsp->ah = ah;

	dm_mad = rsp->mad;
	memcpy(dm_mad, mad_wc->recv_buf.mad, sizeof(*dm_mad));
	dm_mad->mad_hdr.method = IB_MGMT_METHOD_GET_RESP;
	dm_mad->mad_hdr.status = 0;

	switch (mad_wc->recv_buf.mad->mad_hdr.method) {
	case IB_MGMT_METHOD_GET:
		srpt_mgmt_method_get(sport, mad_wc->recv_buf.mad, dm_mad);
		break;
	case IB_MGMT_METHOD_SET:
		dm_mad->mad_hdr.status =
		    cpu_to_be16(DM_MAD_STATUS_UNSUP_METHOD_ATTR);
		break;
	default:
		dm_mad->mad_hdr.status =
		    cpu_to_be16(DM_MAD_STATUS_UNSUP_METHOD);
		break;
	}

	if (!ib_post_send_mad(rsp, NULL)) {
		ib_free_recv_mad(mad_wc);
		 
		return;
	}

	ib_free_send_mad(rsp);

err_rsp:
	rdma_destroy_ah(ah, RDMA_DESTROY_AH_SLEEPABLE);
err:
	ib_free_recv_mad(mad_wc);
}

static int srpt_format_guid(char *buf, unsigned int size, const __be64 *guid)
{
	const __be16 *g = (const __be16 *)guid;

	return snprintf(buf, size, "%04x:%04x:%04x:%04x",
			be16_to_cpu(g[0]), be16_to_cpu(g[1]),
			be16_to_cpu(g[2]), be16_to_cpu(g[3]));
}

 
static int srpt_refresh_port(struct srpt_port *sport)
{
	struct ib_mad_agent *mad_agent;
	struct ib_mad_reg_req reg_req;
	struct ib_port_modify port_modify;
	struct ib_port_attr port_attr;
	int ret;

	ret = ib_query_port(sport->sdev->device, sport->port, &port_attr);
	if (ret)
		return ret;

	sport->sm_lid = port_attr.sm_lid;
	sport->lid = port_attr.lid;

	ret = rdma_query_gid(sport->sdev->device, sport->port, 0, &sport->gid);
	if (ret)
		return ret;

	srpt_format_guid(sport->guid_name, ARRAY_SIZE(sport->guid_name),
			 &sport->gid.global.interface_id);
	snprintf(sport->gid_name, ARRAY_SIZE(sport->gid_name),
		 "0x%016llx%016llx",
		 be64_to_cpu(sport->gid.global.subnet_prefix),
		 be64_to_cpu(sport->gid.global.interface_id));

	if (rdma_protocol_iwarp(sport->sdev->device, sport->port))
		return 0;

	memset(&port_modify, 0, sizeof(port_modify));
	port_modify.set_port_cap_mask = IB_PORT_DEVICE_MGMT_SUP;
	port_modify.clr_port_cap_mask = 0;

	ret = ib_modify_port(sport->sdev->device, sport->port, 0, &port_modify);
	if (ret) {
		pr_warn("%s-%d: enabling device management failed (%d). Note: this is expected if SR-IOV is enabled.\n",
			dev_name(&sport->sdev->device->dev), sport->port, ret);
		return 0;
	}

	if (!sport->mad_agent) {
		memset(&reg_req, 0, sizeof(reg_req));
		reg_req.mgmt_class = IB_MGMT_CLASS_DEVICE_MGMT;
		reg_req.mgmt_class_version = IB_MGMT_BASE_VERSION;
		set_bit(IB_MGMT_METHOD_GET, reg_req.method_mask);
		set_bit(IB_MGMT_METHOD_SET, reg_req.method_mask);

		mad_agent = ib_register_mad_agent(sport->sdev->device,
						  sport->port,
						  IB_QPT_GSI,
						  &reg_req, 0,
						  srpt_mad_send_handler,
						  srpt_mad_recv_handler,
						  sport, 0);
		if (IS_ERR(mad_agent)) {
			pr_err("%s-%d: MAD agent registration failed (%ld). Note: this is expected if SR-IOV is enabled.\n",
			       dev_name(&sport->sdev->device->dev), sport->port,
			       PTR_ERR(mad_agent));
			sport->mad_agent = NULL;
			memset(&port_modify, 0, sizeof(port_modify));
			port_modify.clr_port_cap_mask = IB_PORT_DEVICE_MGMT_SUP;
			ib_modify_port(sport->sdev->device, sport->port, 0,
				       &port_modify);
			return 0;
		}

		sport->mad_agent = mad_agent;
	}

	return 0;
}

 
static void srpt_unregister_mad_agent(struct srpt_device *sdev, int port_cnt)
{
	struct ib_port_modify port_modify = {
		.clr_port_cap_mask = IB_PORT_DEVICE_MGMT_SUP,
	};
	struct srpt_port *sport;
	int i;

	for (i = 1; i <= port_cnt; i++) {
		sport = &sdev->port[i - 1];
		WARN_ON(sport->port != i);
		if (sport->mad_agent) {
			ib_modify_port(sdev->device, i, 0, &port_modify);
			ib_unregister_mad_agent(sport->mad_agent);
			sport->mad_agent = NULL;
		}
	}
}

 
static struct srpt_ioctx *srpt_alloc_ioctx(struct srpt_device *sdev,
					   int ioctx_size,
					   struct kmem_cache *buf_cache,
					   enum dma_data_direction dir)
{
	struct srpt_ioctx *ioctx;

	ioctx = kzalloc(ioctx_size, GFP_KERNEL);
	if (!ioctx)
		goto err;

	ioctx->buf = kmem_cache_alloc(buf_cache, GFP_KERNEL);
	if (!ioctx->buf)
		goto err_free_ioctx;

	ioctx->dma = ib_dma_map_single(sdev->device, ioctx->buf,
				       kmem_cache_size(buf_cache), dir);
	if (ib_dma_mapping_error(sdev->device, ioctx->dma))
		goto err_free_buf;

	return ioctx;

err_free_buf:
	kmem_cache_free(buf_cache, ioctx->buf);
err_free_ioctx:
	kfree(ioctx);
err:
	return NULL;
}

 
static void srpt_free_ioctx(struct srpt_device *sdev, struct srpt_ioctx *ioctx,
			    struct kmem_cache *buf_cache,
			    enum dma_data_direction dir)
{
	if (!ioctx)
		return;

	ib_dma_unmap_single(sdev->device, ioctx->dma,
			    kmem_cache_size(buf_cache), dir);
	kmem_cache_free(buf_cache, ioctx->buf);
	kfree(ioctx);
}

 
static struct srpt_ioctx **srpt_alloc_ioctx_ring(struct srpt_device *sdev,
				int ring_size, int ioctx_size,
				struct kmem_cache *buf_cache,
				int alignment_offset,
				enum dma_data_direction dir)
{
	struct srpt_ioctx **ring;
	int i;

	WARN_ON(ioctx_size != sizeof(struct srpt_recv_ioctx) &&
		ioctx_size != sizeof(struct srpt_send_ioctx));

	ring = kvmalloc_array(ring_size, sizeof(ring[0]), GFP_KERNEL);
	if (!ring)
		goto out;
	for (i = 0; i < ring_size; ++i) {
		ring[i] = srpt_alloc_ioctx(sdev, ioctx_size, buf_cache, dir);
		if (!ring[i])
			goto err;
		ring[i]->index = i;
		ring[i]->offset = alignment_offset;
	}
	goto out;

err:
	while (--i >= 0)
		srpt_free_ioctx(sdev, ring[i], buf_cache, dir);
	kvfree(ring);
	ring = NULL;
out:
	return ring;
}

 
static void srpt_free_ioctx_ring(struct srpt_ioctx **ioctx_ring,
				 struct srpt_device *sdev, int ring_size,
				 struct kmem_cache *buf_cache,
				 enum dma_data_direction dir)
{
	int i;

	if (!ioctx_ring)
		return;

	for (i = 0; i < ring_size; ++i)
		srpt_free_ioctx(sdev, ioctx_ring[i], buf_cache, dir);
	kvfree(ioctx_ring);
}

 
static enum srpt_command_state srpt_set_cmd_state(struct srpt_send_ioctx *ioctx,
						  enum srpt_command_state new)
{
	enum srpt_command_state previous;

	previous = ioctx->state;
	if (previous != SRPT_STATE_DONE)
		ioctx->state = new;

	return previous;
}

 
static bool srpt_test_and_set_cmd_state(struct srpt_send_ioctx *ioctx,
					enum srpt_command_state old,
					enum srpt_command_state new)
{
	enum srpt_command_state previous;

	WARN_ON(!ioctx);
	WARN_ON(old == SRPT_STATE_DONE);
	WARN_ON(new == SRPT_STATE_NEW);

	previous = ioctx->state;
	if (previous == old)
		ioctx->state = new;

	return previous == old;
}

 
static int srpt_post_recv(struct srpt_device *sdev, struct srpt_rdma_ch *ch,
			  struct srpt_recv_ioctx *ioctx)
{
	struct ib_sge list;
	struct ib_recv_wr wr;

	BUG_ON(!sdev);
	list.addr = ioctx->ioctx.dma + ioctx->ioctx.offset;
	list.length = srp_max_req_size;
	list.lkey = sdev->lkey;

	ioctx->ioctx.cqe.done = srpt_recv_done;
	wr.wr_cqe = &ioctx->ioctx.cqe;
	wr.next = NULL;
	wr.sg_list = &list;
	wr.num_sge = 1;

	if (sdev->use_srq)
		return ib_post_srq_recv(sdev->srq, &wr, NULL);
	else
		return ib_post_recv(ch->qp, &wr, NULL);
}

 
static int srpt_zerolength_write(struct srpt_rdma_ch *ch)
{
	struct ib_rdma_wr wr = {
		.wr = {
			.next		= NULL,
			{ .wr_cqe	= &ch->zw_cqe, },
			.opcode		= IB_WR_RDMA_WRITE,
			.send_flags	= IB_SEND_SIGNALED,
		}
	};

	pr_debug("%s-%d: queued zerolength write\n", ch->sess_name,
		 ch->qp->qp_num);

	return ib_post_send(ch->qp, &wr.wr, NULL);
}

static void srpt_zerolength_write_done(struct ib_cq *cq, struct ib_wc *wc)
{
	struct srpt_rdma_ch *ch = wc->qp->qp_context;

	pr_debug("%s-%d wc->status %d\n", ch->sess_name, ch->qp->qp_num,
		 wc->status);

	if (wc->status == IB_WC_SUCCESS) {
		srpt_process_wait_list(ch);
	} else {
		if (srpt_set_ch_state(ch, CH_DISCONNECTED))
			schedule_work(&ch->release_work);
		else
			pr_debug("%s-%d: already disconnected.\n",
				 ch->sess_name, ch->qp->qp_num);
	}
}

static int srpt_alloc_rw_ctxs(struct srpt_send_ioctx *ioctx,
		struct srp_direct_buf *db, int nbufs, struct scatterlist **sg,
		unsigned *sg_cnt)
{
	enum dma_data_direction dir = target_reverse_dma_direction(&ioctx->cmd);
	struct srpt_rdma_ch *ch = ioctx->ch;
	struct scatterlist *prev = NULL;
	unsigned prev_nents;
	int ret, i;

	if (nbufs == 1) {
		ioctx->rw_ctxs = &ioctx->s_rw_ctx;
	} else {
		ioctx->rw_ctxs = kmalloc_array(nbufs, sizeof(*ioctx->rw_ctxs),
			GFP_KERNEL);
		if (!ioctx->rw_ctxs)
			return -ENOMEM;
	}

	for (i = ioctx->n_rw_ctx; i < nbufs; i++, db++) {
		struct srpt_rw_ctx *ctx = &ioctx->rw_ctxs[i];
		u64 remote_addr = be64_to_cpu(db->va);
		u32 size = be32_to_cpu(db->len);
		u32 rkey = be32_to_cpu(db->key);

		ret = target_alloc_sgl(&ctx->sg, &ctx->nents, size, false,
				i < nbufs - 1);
		if (ret)
			goto unwind;

		ret = rdma_rw_ctx_init(&ctx->rw, ch->qp, ch->sport->port,
				ctx->sg, ctx->nents, 0, remote_addr, rkey, dir);
		if (ret < 0) {
			target_free_sgl(ctx->sg, ctx->nents);
			goto unwind;
		}

		ioctx->n_rdma += ret;
		ioctx->n_rw_ctx++;

		if (prev) {
			sg_unmark_end(&prev[prev_nents - 1]);
			sg_chain(prev, prev_nents + 1, ctx->sg);
		} else {
			*sg = ctx->sg;
		}

		prev = ctx->sg;
		prev_nents = ctx->nents;

		*sg_cnt += ctx->nents;
	}

	return 0;

unwind:
	while (--i >= 0) {
		struct srpt_rw_ctx *ctx = &ioctx->rw_ctxs[i];

		rdma_rw_ctx_destroy(&ctx->rw, ch->qp, ch->sport->port,
				ctx->sg, ctx->nents, dir);
		target_free_sgl(ctx->sg, ctx->nents);
	}
	if (ioctx->rw_ctxs != &ioctx->s_rw_ctx)
		kfree(ioctx->rw_ctxs);
	return ret;
}

static void srpt_free_rw_ctxs(struct srpt_rdma_ch *ch,
				    struct srpt_send_ioctx *ioctx)
{
	enum dma_data_direction dir = target_reverse_dma_direction(&ioctx->cmd);
	int i;

	for (i = 0; i < ioctx->n_rw_ctx; i++) {
		struct srpt_rw_ctx *ctx = &ioctx->rw_ctxs[i];

		rdma_rw_ctx_destroy(&ctx->rw, ch->qp, ch->sport->port,
				ctx->sg, ctx->nents, dir);
		target_free_sgl(ctx->sg, ctx->nents);
	}

	if (ioctx->rw_ctxs != &ioctx->s_rw_ctx)
		kfree(ioctx->rw_ctxs);
}

static inline void *srpt_get_desc_buf(struct srp_cmd *srp_cmd)
{
	 
	BUILD_BUG_ON(!__same_type(srp_cmd->add_data[0], (s8)0) &&
		     !__same_type(srp_cmd->add_data[0], (u8)0));

	 
	return srp_cmd->add_data + (srp_cmd->add_cdb_len & ~3);
}

 
static int srpt_get_desc_tbl(struct srpt_recv_ioctx *recv_ioctx,
		struct srpt_send_ioctx *ioctx,
		struct srp_cmd *srp_cmd, enum dma_data_direction *dir,
		struct scatterlist **sg, unsigned int *sg_cnt, u64 *data_len,
		u16 imm_data_offset)
{
	BUG_ON(!dir);
	BUG_ON(!data_len);

	 
	if (srp_cmd->buf_fmt & 0xf)
		 
		*dir = DMA_FROM_DEVICE;
	else if (srp_cmd->buf_fmt >> 4)
		 
		*dir = DMA_TO_DEVICE;
	else
		*dir = DMA_NONE;

	 
	ioctx->cmd.data_direction = *dir;

	if (((srp_cmd->buf_fmt & 0xf) == SRP_DATA_DESC_DIRECT) ||
	    ((srp_cmd->buf_fmt >> 4) == SRP_DATA_DESC_DIRECT)) {
		struct srp_direct_buf *db = srpt_get_desc_buf(srp_cmd);

		*data_len = be32_to_cpu(db->len);
		return srpt_alloc_rw_ctxs(ioctx, db, 1, sg, sg_cnt);
	} else if (((srp_cmd->buf_fmt & 0xf) == SRP_DATA_DESC_INDIRECT) ||
		   ((srp_cmd->buf_fmt >> 4) == SRP_DATA_DESC_INDIRECT)) {
		struct srp_indirect_buf *idb = srpt_get_desc_buf(srp_cmd);
		int nbufs = be32_to_cpu(idb->table_desc.len) /
				sizeof(struct srp_direct_buf);

		if (nbufs >
		    (srp_cmd->data_out_desc_cnt + srp_cmd->data_in_desc_cnt)) {
			pr_err("received unsupported SRP_CMD request type (%u out + %u in != %u / %zu)\n",
			       srp_cmd->data_out_desc_cnt,
			       srp_cmd->data_in_desc_cnt,
			       be32_to_cpu(idb->table_desc.len),
			       sizeof(struct srp_direct_buf));
			return -EINVAL;
		}

		*data_len = be32_to_cpu(idb->len);
		return srpt_alloc_rw_ctxs(ioctx, idb->desc_list, nbufs,
				sg, sg_cnt);
	} else if ((srp_cmd->buf_fmt >> 4) == SRP_DATA_DESC_IMM) {
		struct srp_imm_buf *imm_buf = srpt_get_desc_buf(srp_cmd);
		void *data = (void *)srp_cmd + imm_data_offset;
		uint32_t len = be32_to_cpu(imm_buf->len);
		uint32_t req_size = imm_data_offset + len;

		if (req_size > srp_max_req_size) {
			pr_err("Immediate data (length %d + %d) exceeds request size %d\n",
			       imm_data_offset, len, srp_max_req_size);
			return -EINVAL;
		}
		if (recv_ioctx->byte_len < req_size) {
			pr_err("Received too few data - %d < %d\n",
			       recv_ioctx->byte_len, req_size);
			return -EIO;
		}
		 
		if ((void *)(imm_buf + 1) > (void *)data) {
			pr_err("Received invalid write request\n");
			return -EINVAL;
		}
		*data_len = len;
		ioctx->recv_ioctx = recv_ioctx;
		if ((uintptr_t)data & 511) {
			pr_warn_once("Internal error - the receive buffers are not aligned properly.\n");
			return -EINVAL;
		}
		sg_init_one(&ioctx->imm_sg, data, len);
		*sg = &ioctx->imm_sg;
		*sg_cnt = 1;
		return 0;
	} else {
		*data_len = 0;
		return 0;
	}
}

 
static int srpt_init_ch_qp(struct srpt_rdma_ch *ch, struct ib_qp *qp)
{
	struct ib_qp_attr *attr;
	int ret;

	WARN_ON_ONCE(ch->using_rdma_cm);

	attr = kzalloc(sizeof(*attr), GFP_KERNEL);
	if (!attr)
		return -ENOMEM;

	attr->qp_state = IB_QPS_INIT;
	attr->qp_access_flags = IB_ACCESS_LOCAL_WRITE;
	attr->port_num = ch->sport->port;

	ret = ib_find_cached_pkey(ch->sport->sdev->device, ch->sport->port,
				  ch->pkey, &attr->pkey_index);
	if (ret < 0)
		pr_err("Translating pkey %#x failed (%d) - using index 0\n",
		       ch->pkey, ret);

	ret = ib_modify_qp(qp, attr,
			   IB_QP_STATE | IB_QP_ACCESS_FLAGS | IB_QP_PORT |
			   IB_QP_PKEY_INDEX);

	kfree(attr);
	return ret;
}

 
static int srpt_ch_qp_rtr(struct srpt_rdma_ch *ch, struct ib_qp *qp)
{
	struct ib_qp_attr qp_attr;
	int attr_mask;
	int ret;

	WARN_ON_ONCE(ch->using_rdma_cm);

	qp_attr.qp_state = IB_QPS_RTR;
	ret = ib_cm_init_qp_attr(ch->ib_cm.cm_id, &qp_attr, &attr_mask);
	if (ret)
		goto out;

	qp_attr.max_dest_rd_atomic = 4;

	ret = ib_modify_qp(qp, &qp_attr, attr_mask);

out:
	return ret;
}

 
static int srpt_ch_qp_rts(struct srpt_rdma_ch *ch, struct ib_qp *qp)
{
	struct ib_qp_attr qp_attr;
	int attr_mask;
	int ret;

	qp_attr.qp_state = IB_QPS_RTS;
	ret = ib_cm_init_qp_attr(ch->ib_cm.cm_id, &qp_attr, &attr_mask);
	if (ret)
		goto out;

	qp_attr.max_rd_atomic = 4;

	ret = ib_modify_qp(qp, &qp_attr, attr_mask);

out:
	return ret;
}

 
static int srpt_ch_qp_err(struct srpt_rdma_ch *ch)
{
	struct ib_qp_attr qp_attr;

	qp_attr.qp_state = IB_QPS_ERR;
	return ib_modify_qp(ch->qp, &qp_attr, IB_QP_STATE);
}

 
static struct srpt_send_ioctx *srpt_get_send_ioctx(struct srpt_rdma_ch *ch)
{
	struct srpt_send_ioctx *ioctx;
	int tag, cpu;

	BUG_ON(!ch);

	tag = sbitmap_queue_get(&ch->sess->sess_tag_pool, &cpu);
	if (tag < 0)
		return NULL;

	ioctx = ch->ioctx_ring[tag];
	BUG_ON(ioctx->ch != ch);
	ioctx->state = SRPT_STATE_NEW;
	WARN_ON_ONCE(ioctx->recv_ioctx);
	ioctx->n_rdma = 0;
	ioctx->n_rw_ctx = 0;
	ioctx->queue_status_only = false;
	 
	memset(&ioctx->cmd, 0, sizeof(ioctx->cmd));
	memset(&ioctx->sense_data, 0, sizeof(ioctx->sense_data));
	ioctx->cmd.map_tag = tag;
	ioctx->cmd.map_cpu = cpu;

	return ioctx;
}

 
static int srpt_abort_cmd(struct srpt_send_ioctx *ioctx)
{
	enum srpt_command_state state;

	BUG_ON(!ioctx);

	 

	state = ioctx->state;
	switch (state) {
	case SRPT_STATE_NEED_DATA:
		ioctx->state = SRPT_STATE_DATA_IN;
		break;
	case SRPT_STATE_CMD_RSP_SENT:
	case SRPT_STATE_MGMT_RSP_SENT:
		ioctx->state = SRPT_STATE_DONE;
		break;
	default:
		WARN_ONCE(true, "%s: unexpected I/O context state %d\n",
			  __func__, state);
		break;
	}

	pr_debug("Aborting cmd with state %d -> %d and tag %lld\n", state,
		 ioctx->state, ioctx->cmd.tag);

	switch (state) {
	case SRPT_STATE_NEW:
	case SRPT_STATE_DATA_IN:
	case SRPT_STATE_MGMT:
	case SRPT_STATE_DONE:
		 
		break;
	case SRPT_STATE_NEED_DATA:
		pr_debug("tag %#llx: RDMA read error\n", ioctx->cmd.tag);
		transport_generic_request_failure(&ioctx->cmd,
					TCM_CHECK_CONDITION_ABORT_CMD);
		break;
	case SRPT_STATE_CMD_RSP_SENT:
		 
		transport_generic_free_cmd(&ioctx->cmd, 0);
		break;
	case SRPT_STATE_MGMT_RSP_SENT:
		transport_generic_free_cmd(&ioctx->cmd, 0);
		break;
	default:
		WARN(1, "Unexpected command state (%d)", state);
		break;
	}

	return state;
}

 
static void srpt_rdma_read_done(struct ib_cq *cq, struct ib_wc *wc)
{
	struct srpt_rdma_ch *ch = wc->qp->qp_context;
	struct srpt_send_ioctx *ioctx =
		container_of(wc->wr_cqe, struct srpt_send_ioctx, rdma_cqe);

	WARN_ON(ioctx->n_rdma <= 0);
	atomic_add(ioctx->n_rdma, &ch->sq_wr_avail);
	ioctx->n_rdma = 0;

	if (unlikely(wc->status != IB_WC_SUCCESS)) {
		pr_info("RDMA_READ for ioctx 0x%p failed with status %d\n",
			ioctx, wc->status);
		srpt_abort_cmd(ioctx);
		return;
	}

	if (srpt_test_and_set_cmd_state(ioctx, SRPT_STATE_NEED_DATA,
					SRPT_STATE_DATA_IN))
		target_execute_cmd(&ioctx->cmd);
	else
		pr_err("%s[%d]: wrong state = %d\n", __func__,
		       __LINE__, ioctx->state);
}

 
static int srpt_build_cmd_rsp(struct srpt_rdma_ch *ch,
			      struct srpt_send_ioctx *ioctx, u64 tag,
			      int status)
{
	struct se_cmd *cmd = &ioctx->cmd;
	struct srp_rsp *srp_rsp;
	const u8 *sense_data;
	int sense_data_len, max_sense_len;
	u32 resid = cmd->residual_count;

	 
	WARN_ON(status & 1);

	srp_rsp = ioctx->ioctx.buf;
	BUG_ON(!srp_rsp);

	sense_data = ioctx->sense_data;
	sense_data_len = ioctx->cmd.scsi_sense_length;
	WARN_ON(sense_data_len > sizeof(ioctx->sense_data));

	memset(srp_rsp, 0, sizeof(*srp_rsp));
	srp_rsp->opcode = SRP_RSP;
	srp_rsp->req_lim_delta =
		cpu_to_be32(1 + atomic_xchg(&ch->req_lim_delta, 0));
	srp_rsp->tag = tag;
	srp_rsp->status = status;

	if (cmd->se_cmd_flags & SCF_UNDERFLOW_BIT) {
		if (cmd->data_direction == DMA_TO_DEVICE) {
			 
			srp_rsp->flags = SRP_RSP_FLAG_DOUNDER;
			srp_rsp->data_out_res_cnt = cpu_to_be32(resid);
		} else if (cmd->data_direction == DMA_FROM_DEVICE) {
			 
			srp_rsp->flags = SRP_RSP_FLAG_DIUNDER;
			srp_rsp->data_in_res_cnt = cpu_to_be32(resid);
		}
	} else if (cmd->se_cmd_flags & SCF_OVERFLOW_BIT) {
		if (cmd->data_direction == DMA_TO_DEVICE) {
			 
			srp_rsp->flags = SRP_RSP_FLAG_DOOVER;
			srp_rsp->data_out_res_cnt = cpu_to_be32(resid);
		} else if (cmd->data_direction == DMA_FROM_DEVICE) {
			 
			srp_rsp->flags = SRP_RSP_FLAG_DIOVER;
			srp_rsp->data_in_res_cnt = cpu_to_be32(resid);
		}
	}

	if (sense_data_len) {
		BUILD_BUG_ON(MIN_MAX_RSP_SIZE <= sizeof(*srp_rsp));
		max_sense_len = ch->max_ti_iu_len - sizeof(*srp_rsp);
		if (sense_data_len > max_sense_len) {
			pr_warn("truncated sense data from %d to %d bytes\n",
				sense_data_len, max_sense_len);
			sense_data_len = max_sense_len;
		}

		srp_rsp->flags |= SRP_RSP_FLAG_SNSVALID;
		srp_rsp->sense_data_len = cpu_to_be32(sense_data_len);
		memcpy(srp_rsp->data, sense_data, sense_data_len);
	}

	return sizeof(*srp_rsp) + sense_data_len;
}

 
static int srpt_build_tskmgmt_rsp(struct srpt_rdma_ch *ch,
				  struct srpt_send_ioctx *ioctx,
				  u8 rsp_code, u64 tag)
{
	struct srp_rsp *srp_rsp;
	int resp_data_len;
	int resp_len;

	resp_data_len = 4;
	resp_len = sizeof(*srp_rsp) + resp_data_len;

	srp_rsp = ioctx->ioctx.buf;
	BUG_ON(!srp_rsp);
	memset(srp_rsp, 0, sizeof(*srp_rsp));

	srp_rsp->opcode = SRP_RSP;
	srp_rsp->req_lim_delta =
		cpu_to_be32(1 + atomic_xchg(&ch->req_lim_delta, 0));
	srp_rsp->tag = tag;

	srp_rsp->flags |= SRP_RSP_FLAG_RSPVALID;
	srp_rsp->resp_data_len = cpu_to_be32(resp_data_len);
	srp_rsp->data[3] = rsp_code;

	return resp_len;
}

static int srpt_check_stop_free(struct se_cmd *cmd)
{
	struct srpt_send_ioctx *ioctx = container_of(cmd,
				struct srpt_send_ioctx, cmd);

	return target_put_sess_cmd(&ioctx->cmd);
}

 
static void srpt_handle_cmd(struct srpt_rdma_ch *ch,
			    struct srpt_recv_ioctx *recv_ioctx,
			    struct srpt_send_ioctx *send_ioctx)
{
	struct se_cmd *cmd;
	struct srp_cmd *srp_cmd;
	struct scatterlist *sg = NULL;
	unsigned sg_cnt = 0;
	u64 data_len;
	enum dma_data_direction dir;
	int rc;

	BUG_ON(!send_ioctx);

	srp_cmd = recv_ioctx->ioctx.buf + recv_ioctx->ioctx.offset;
	cmd = &send_ioctx->cmd;
	cmd->tag = srp_cmd->tag;

	switch (srp_cmd->task_attr) {
	case SRP_CMD_SIMPLE_Q:
		cmd->sam_task_attr = TCM_SIMPLE_TAG;
		break;
	case SRP_CMD_ORDERED_Q:
	default:
		cmd->sam_task_attr = TCM_ORDERED_TAG;
		break;
	case SRP_CMD_HEAD_OF_Q:
		cmd->sam_task_attr = TCM_HEAD_TAG;
		break;
	case SRP_CMD_ACA:
		cmd->sam_task_attr = TCM_ACA_TAG;
		break;
	}

	rc = srpt_get_desc_tbl(recv_ioctx, send_ioctx, srp_cmd, &dir,
			       &sg, &sg_cnt, &data_len, ch->imm_data_offset);
	if (rc) {
		if (rc != -EAGAIN) {
			pr_err("0x%llx: parsing SRP descriptor table failed.\n",
			       srp_cmd->tag);
		}
		goto busy;
	}

	rc = target_init_cmd(cmd, ch->sess, &send_ioctx->sense_data[0],
			     scsilun_to_int(&srp_cmd->lun), data_len,
			     TCM_SIMPLE_TAG, dir, TARGET_SCF_ACK_KREF);
	if (rc != 0) {
		pr_debug("target_submit_cmd() returned %d for tag %#llx\n", rc,
			 srp_cmd->tag);
		goto busy;
	}

	if (target_submit_prep(cmd, srp_cmd->cdb, sg, sg_cnt, NULL, 0, NULL, 0,
			       GFP_KERNEL))
		return;

	target_submit(cmd);
	return;

busy:
	target_send_busy(cmd);
}

static int srp_tmr_to_tcm(int fn)
{
	switch (fn) {
	case SRP_TSK_ABORT_TASK:
		return TMR_ABORT_TASK;
	case SRP_TSK_ABORT_TASK_SET:
		return TMR_ABORT_TASK_SET;
	case SRP_TSK_CLEAR_TASK_SET:
		return TMR_CLEAR_TASK_SET;
	case SRP_TSK_LUN_RESET:
		return TMR_LUN_RESET;
	case SRP_TSK_CLEAR_ACA:
		return TMR_CLEAR_ACA;
	default:
		return -1;
	}
}

 
static void srpt_handle_tsk_mgmt(struct srpt_rdma_ch *ch,
				 struct srpt_recv_ioctx *recv_ioctx,
				 struct srpt_send_ioctx *send_ioctx)
{
	struct srp_tsk_mgmt *srp_tsk;
	struct se_cmd *cmd;
	struct se_session *sess = ch->sess;
	int tcm_tmr;
	int rc;

	BUG_ON(!send_ioctx);

	srp_tsk = recv_ioctx->ioctx.buf + recv_ioctx->ioctx.offset;
	cmd = &send_ioctx->cmd;

	pr_debug("recv tsk_mgmt fn %d for task_tag %lld and cmd tag %lld ch %p sess %p\n",
		 srp_tsk->tsk_mgmt_func, srp_tsk->task_tag, srp_tsk->tag, ch,
		 ch->sess);

	srpt_set_cmd_state(send_ioctx, SRPT_STATE_MGMT);
	send_ioctx->cmd.tag = srp_tsk->tag;
	tcm_tmr = srp_tmr_to_tcm(srp_tsk->tsk_mgmt_func);
	rc = target_submit_tmr(&send_ioctx->cmd, sess, NULL,
			       scsilun_to_int(&srp_tsk->lun), srp_tsk, tcm_tmr,
			       GFP_KERNEL, srp_tsk->task_tag,
			       TARGET_SCF_ACK_KREF);
	if (rc != 0) {
		send_ioctx->cmd.se_tmr_req->response = TMR_FUNCTION_REJECTED;
		cmd->se_tfo->queue_tm_rsp(cmd);
	}
	return;
}

 
static bool
srpt_handle_new_iu(struct srpt_rdma_ch *ch, struct srpt_recv_ioctx *recv_ioctx)
{
	struct srpt_send_ioctx *send_ioctx = NULL;
	struct srp_cmd *srp_cmd;
	bool res = false;
	u8 opcode;

	BUG_ON(!ch);
	BUG_ON(!recv_ioctx);

	if (unlikely(ch->state == CH_CONNECTING))
		goto push;

	ib_dma_sync_single_for_cpu(ch->sport->sdev->device,
				   recv_ioctx->ioctx.dma,
				   recv_ioctx->ioctx.offset + srp_max_req_size,
				   DMA_FROM_DEVICE);

	srp_cmd = recv_ioctx->ioctx.buf + recv_ioctx->ioctx.offset;
	opcode = srp_cmd->opcode;
	if (opcode == SRP_CMD || opcode == SRP_TSK_MGMT) {
		send_ioctx = srpt_get_send_ioctx(ch);
		if (unlikely(!send_ioctx))
			goto push;
	}

	if (!list_empty(&recv_ioctx->wait_list)) {
		WARN_ON_ONCE(!ch->processing_wait_list);
		list_del_init(&recv_ioctx->wait_list);
	}

	switch (opcode) {
	case SRP_CMD:
		srpt_handle_cmd(ch, recv_ioctx, send_ioctx);
		break;
	case SRP_TSK_MGMT:
		srpt_handle_tsk_mgmt(ch, recv_ioctx, send_ioctx);
		break;
	case SRP_I_LOGOUT:
		pr_err("Not yet implemented: SRP_I_LOGOUT\n");
		break;
	case SRP_CRED_RSP:
		pr_debug("received SRP_CRED_RSP\n");
		break;
	case SRP_AER_RSP:
		pr_debug("received SRP_AER_RSP\n");
		break;
	case SRP_RSP:
		pr_err("Received SRP_RSP\n");
		break;
	default:
		pr_err("received IU with unknown opcode 0x%x\n", opcode);
		break;
	}

	if (!send_ioctx || !send_ioctx->recv_ioctx)
		srpt_post_recv(ch->sport->sdev, ch, recv_ioctx);
	res = true;

out:
	return res;

push:
	if (list_empty(&recv_ioctx->wait_list)) {
		WARN_ON_ONCE(ch->processing_wait_list);
		list_add_tail(&recv_ioctx->wait_list, &ch->cmd_wait_list);
	}
	goto out;
}

static void srpt_recv_done(struct ib_cq *cq, struct ib_wc *wc)
{
	struct srpt_rdma_ch *ch = wc->qp->qp_context;
	struct srpt_recv_ioctx *ioctx =
		container_of(wc->wr_cqe, struct srpt_recv_ioctx, ioctx.cqe);

	if (wc->status == IB_WC_SUCCESS) {
		int req_lim;

		req_lim = atomic_dec_return(&ch->req_lim);
		if (unlikely(req_lim < 0))
			pr_err("req_lim = %d < 0\n", req_lim);
		ioctx->byte_len = wc->byte_len;
		srpt_handle_new_iu(ch, ioctx);
	} else {
		pr_info_ratelimited("receiving failed for ioctx %p with status %d\n",
				    ioctx, wc->status);
	}
}

 
static void srpt_process_wait_list(struct srpt_rdma_ch *ch)
{
	struct srpt_recv_ioctx *recv_ioctx, *tmp;

	WARN_ON_ONCE(ch->state == CH_CONNECTING);

	if (list_empty(&ch->cmd_wait_list))
		return;

	WARN_ON_ONCE(ch->processing_wait_list);
	ch->processing_wait_list = true;
	list_for_each_entry_safe(recv_ioctx, tmp, &ch->cmd_wait_list,
				 wait_list) {
		if (!srpt_handle_new_iu(ch, recv_ioctx))
			break;
	}
	ch->processing_wait_list = false;
}

 
static void srpt_send_done(struct ib_cq *cq, struct ib_wc *wc)
{
	struct srpt_rdma_ch *ch = wc->qp->qp_context;
	struct srpt_send_ioctx *ioctx =
		container_of(wc->wr_cqe, struct srpt_send_ioctx, ioctx.cqe);
	enum srpt_command_state state;

	state = srpt_set_cmd_state(ioctx, SRPT_STATE_DONE);

	WARN_ON(state != SRPT_STATE_CMD_RSP_SENT &&
		state != SRPT_STATE_MGMT_RSP_SENT);

	atomic_add(1 + ioctx->n_rdma, &ch->sq_wr_avail);

	if (wc->status != IB_WC_SUCCESS)
		pr_info("sending response for ioctx 0x%p failed with status %d\n",
			ioctx, wc->status);

	if (state != SRPT_STATE_DONE) {
		transport_generic_free_cmd(&ioctx->cmd, 0);
	} else {
		pr_err("IB completion has been received too late for wr_id = %u.\n",
		       ioctx->ioctx.index);
	}

	srpt_process_wait_list(ch);
}

 
static int srpt_create_ch_ib(struct srpt_rdma_ch *ch)
{
	struct ib_qp_init_attr *qp_init;
	struct srpt_port *sport = ch->sport;
	struct srpt_device *sdev = sport->sdev;
	const struct ib_device_attr *attrs = &sdev->device->attrs;
	int sq_size = sport->port_attrib.srp_sq_size;
	int i, ret;

	WARN_ON(ch->rq_size < 1);

	ret = -ENOMEM;
	qp_init = kzalloc(sizeof(*qp_init), GFP_KERNEL);
	if (!qp_init)
		goto out;

retry:
	ch->cq = ib_cq_pool_get(sdev->device, ch->rq_size + sq_size, -1,
				 IB_POLL_WORKQUEUE);
	if (IS_ERR(ch->cq)) {
		ret = PTR_ERR(ch->cq);
		pr_err("failed to create CQ cqe= %d ret= %d\n",
		       ch->rq_size + sq_size, ret);
		goto out;
	}
	ch->cq_size = ch->rq_size + sq_size;

	qp_init->qp_context = (void *)ch;
	qp_init->event_handler
		= (void(*)(struct ib_event *, void*))srpt_qp_event;
	qp_init->send_cq = ch->cq;
	qp_init->recv_cq = ch->cq;
	qp_init->sq_sig_type = IB_SIGNAL_REQ_WR;
	qp_init->qp_type = IB_QPT_RC;
	 
	qp_init->cap.max_send_wr = min(sq_size / 2, attrs->max_qp_wr);
	qp_init->cap.max_rdma_ctxs = sq_size / 2;
	qp_init->cap.max_send_sge = attrs->max_send_sge;
	qp_init->cap.max_recv_sge = 1;
	qp_init->port_num = ch->sport->port;
	if (sdev->use_srq)
		qp_init->srq = sdev->srq;
	else
		qp_init->cap.max_recv_wr = ch->rq_size;

	if (ch->using_rdma_cm) {
		ret = rdma_create_qp(ch->rdma_cm.cm_id, sdev->pd, qp_init);
		ch->qp = ch->rdma_cm.cm_id->qp;
	} else {
		ch->qp = ib_create_qp(sdev->pd, qp_init);
		if (!IS_ERR(ch->qp)) {
			ret = srpt_init_ch_qp(ch, ch->qp);
			if (ret)
				ib_destroy_qp(ch->qp);
		} else {
			ret = PTR_ERR(ch->qp);
		}
	}
	if (ret) {
		bool retry = sq_size > MIN_SRPT_SQ_SIZE;

		if (retry) {
			pr_debug("failed to create queue pair with sq_size = %d (%d) - retrying\n",
				 sq_size, ret);
			ib_cq_pool_put(ch->cq, ch->cq_size);
			sq_size = max(sq_size / 2, MIN_SRPT_SQ_SIZE);
			goto retry;
		} else {
			pr_err("failed to create queue pair with sq_size = %d (%d)\n",
			       sq_size, ret);
			goto err_destroy_cq;
		}
	}

	atomic_set(&ch->sq_wr_avail, qp_init->cap.max_send_wr);

	pr_debug("%s: max_cqe= %d max_sge= %d sq_size = %d ch= %p\n",
		 __func__, ch->cq->cqe, qp_init->cap.max_send_sge,
		 qp_init->cap.max_send_wr, ch);

	if (!sdev->use_srq)
		for (i = 0; i < ch->rq_size; i++)
			srpt_post_recv(sdev, ch, ch->ioctx_recv_ring[i]);

out:
	kfree(qp_init);
	return ret;

err_destroy_cq:
	ch->qp = NULL;
	ib_cq_pool_put(ch->cq, ch->cq_size);
	goto out;
}

static void srpt_destroy_ch_ib(struct srpt_rdma_ch *ch)
{
	ib_destroy_qp(ch->qp);
	ib_cq_pool_put(ch->cq, ch->cq_size);
}

 
static bool srpt_close_ch(struct srpt_rdma_ch *ch)
{
	int ret;

	if (!srpt_set_ch_state(ch, CH_DRAINING)) {
		pr_debug("%s: already closed\n", ch->sess_name);
		return false;
	}

	kref_get(&ch->kref);

	ret = srpt_ch_qp_err(ch);
	if (ret < 0)
		pr_err("%s-%d: changing queue pair into error state failed: %d\n",
		       ch->sess_name, ch->qp->qp_num, ret);

	ret = srpt_zerolength_write(ch);
	if (ret < 0) {
		pr_err("%s-%d: queuing zero-length write failed: %d\n",
		       ch->sess_name, ch->qp->qp_num, ret);
		if (srpt_set_ch_state(ch, CH_DISCONNECTED))
			schedule_work(&ch->release_work);
		else
			WARN_ON_ONCE(true);
	}

	kref_put(&ch->kref, srpt_free_ch);

	return true;
}

 
static int srpt_disconnect_ch(struct srpt_rdma_ch *ch)
{
	int ret;

	if (!srpt_set_ch_state(ch, CH_DISCONNECTING))
		return -ENOTCONN;

	if (ch->using_rdma_cm) {
		ret = rdma_disconnect(ch->rdma_cm.cm_id);
	} else {
		ret = ib_send_cm_dreq(ch->ib_cm.cm_id, NULL, 0);
		if (ret < 0)
			ret = ib_send_cm_drep(ch->ib_cm.cm_id, NULL, 0);
	}

	if (ret < 0 && srpt_close_ch(ch))
		ret = 0;

	return ret;
}

 
static void srpt_disconnect_ch_sync(struct srpt_rdma_ch *ch)
{
	DECLARE_COMPLETION_ONSTACK(closed);
	struct srpt_port *sport = ch->sport;

	pr_debug("ch %s-%d state %d\n", ch->sess_name, ch->qp->qp_num,
		 ch->state);

	ch->closed = &closed;

	mutex_lock(&sport->mutex);
	srpt_disconnect_ch(ch);
	mutex_unlock(&sport->mutex);

	while (wait_for_completion_timeout(&closed, 5 * HZ) == 0)
		pr_info("%s(%s-%d state %d): still waiting ...\n", __func__,
			ch->sess_name, ch->qp->qp_num, ch->state);

}

static void __srpt_close_all_ch(struct srpt_port *sport)
{
	struct srpt_nexus *nexus;
	struct srpt_rdma_ch *ch;

	lockdep_assert_held(&sport->mutex);

	list_for_each_entry(nexus, &sport->nexus_list, entry) {
		list_for_each_entry(ch, &nexus->ch_list, list) {
			if (srpt_disconnect_ch(ch) >= 0)
				pr_info("Closing channel %s-%d because target %s_%d has been disabled\n",
					ch->sess_name, ch->qp->qp_num,
					dev_name(&sport->sdev->device->dev),
					sport->port);
			srpt_close_ch(ch);
		}
	}
}

 
static struct srpt_nexus *srpt_get_nexus(struct srpt_port *sport,
					 const u8 i_port_id[16],
					 const u8 t_port_id[16])
{
	struct srpt_nexus *nexus = NULL, *tmp_nexus = NULL, *n;

	for (;;) {
		mutex_lock(&sport->mutex);
		list_for_each_entry(n, &sport->nexus_list, entry) {
			if (memcmp(n->i_port_id, i_port_id, 16) == 0 &&
			    memcmp(n->t_port_id, t_port_id, 16) == 0) {
				nexus = n;
				break;
			}
		}
		if (!nexus && tmp_nexus) {
			list_add_tail_rcu(&tmp_nexus->entry,
					  &sport->nexus_list);
			swap(nexus, tmp_nexus);
		}
		mutex_unlock(&sport->mutex);

		if (nexus)
			break;
		tmp_nexus = kzalloc(sizeof(*nexus), GFP_KERNEL);
		if (!tmp_nexus) {
			nexus = ERR_PTR(-ENOMEM);
			break;
		}
		INIT_LIST_HEAD(&tmp_nexus->ch_list);
		memcpy(tmp_nexus->i_port_id, i_port_id, 16);
		memcpy(tmp_nexus->t_port_id, t_port_id, 16);
	}

	kfree(tmp_nexus);

	return nexus;
}

static void srpt_set_enabled(struct srpt_port *sport, bool enabled)
	__must_hold(&sport->mutex)
{
	lockdep_assert_held(&sport->mutex);

	if (sport->enabled == enabled)
		return;
	sport->enabled = enabled;
	if (!enabled)
		__srpt_close_all_ch(sport);
}

static void srpt_drop_sport_ref(struct srpt_port *sport)
{
	if (atomic_dec_return(&sport->refcount) == 0 && sport->freed_channels)
		complete(sport->freed_channels);
}

static void srpt_free_ch(struct kref *kref)
{
	struct srpt_rdma_ch *ch = container_of(kref, struct srpt_rdma_ch, kref);

	srpt_drop_sport_ref(ch->sport);
	kfree_rcu(ch, rcu);
}

 
static void srpt_release_channel_work(struct work_struct *w)
{
	struct srpt_rdma_ch *ch;
	struct srpt_device *sdev;
	struct srpt_port *sport;
	struct se_session *se_sess;

	ch = container_of(w, struct srpt_rdma_ch, release_work);
	pr_debug("%s-%d\n", ch->sess_name, ch->qp->qp_num);

	sdev = ch->sport->sdev;
	BUG_ON(!sdev);

	se_sess = ch->sess;
	BUG_ON(!se_sess);

	target_stop_session(se_sess);
	target_wait_for_sess_cmds(se_sess);

	target_remove_session(se_sess);
	ch->sess = NULL;

	if (ch->using_rdma_cm)
		rdma_destroy_id(ch->rdma_cm.cm_id);
	else
		ib_destroy_cm_id(ch->ib_cm.cm_id);

	sport = ch->sport;
	mutex_lock(&sport->mutex);
	list_del_rcu(&ch->list);
	mutex_unlock(&sport->mutex);

	if (ch->closed)
		complete(ch->closed);

	srpt_destroy_ch_ib(ch);

	srpt_free_ioctx_ring((struct srpt_ioctx **)ch->ioctx_ring,
			     ch->sport->sdev, ch->rq_size,
			     ch->rsp_buf_cache, DMA_TO_DEVICE);

	kmem_cache_destroy(ch->rsp_buf_cache);

	srpt_free_ioctx_ring((struct srpt_ioctx **)ch->ioctx_recv_ring,
			     sdev, ch->rq_size,
			     ch->req_buf_cache, DMA_FROM_DEVICE);

	kmem_cache_destroy(ch->req_buf_cache);

	kref_put(&ch->kref, srpt_free_ch);
}

 
static int srpt_cm_req_recv(struct srpt_device *const sdev,
			    struct ib_cm_id *ib_cm_id,
			    struct rdma_cm_id *rdma_cm_id,
			    u8 port_num, __be16 pkey,
			    const struct srp_login_req *req,
			    const char *src_addr)
{
	struct srpt_port *sport = &sdev->port[port_num - 1];
	struct srpt_nexus *nexus;
	struct srp_login_rsp *rsp = NULL;
	struct srp_login_rej *rej = NULL;
	union {
		struct rdma_conn_param rdma_cm;
		struct ib_cm_rep_param ib_cm;
	} *rep_param = NULL;
	struct srpt_rdma_ch *ch = NULL;
	char i_port_id[36];
	u32 it_iu_len;
	int i, tag_num, tag_size, ret;
	struct srpt_tpg *stpg;

	WARN_ON_ONCE(irqs_disabled());

	it_iu_len = be32_to_cpu(req->req_it_iu_len);

	pr_info("Received SRP_LOGIN_REQ with i_port_id %pI6, t_port_id %pI6 and it_iu_len %d on port %d (guid=%pI6); pkey %#04x\n",
		req->initiator_port_id, req->target_port_id, it_iu_len,
		port_num, &sport->gid, be16_to_cpu(pkey));

	nexus = srpt_get_nexus(sport, req->initiator_port_id,
			       req->target_port_id);
	if (IS_ERR(nexus)) {
		ret = PTR_ERR(nexus);
		goto out;
	}

	ret = -ENOMEM;
	rsp = kzalloc(sizeof(*rsp), GFP_KERNEL);
	rej = kzalloc(sizeof(*rej), GFP_KERNEL);
	rep_param = kzalloc(sizeof(*rep_param), GFP_KERNEL);
	if (!rsp || !rej || !rep_param)
		goto out;

	ret = -EINVAL;
	if (it_iu_len > srp_max_req_size || it_iu_len < 64) {
		rej->reason = cpu_to_be32(
				SRP_LOGIN_REJ_REQ_IT_IU_LENGTH_TOO_LARGE);
		pr_err("rejected SRP_LOGIN_REQ because its length (%d bytes) is out of range (%d .. %d)\n",
		       it_iu_len, 64, srp_max_req_size);
		goto reject;
	}

	if (!sport->enabled) {
		rej->reason = cpu_to_be32(SRP_LOGIN_REJ_INSUFFICIENT_RESOURCES);
		pr_info("rejected SRP_LOGIN_REQ because target port %s_%d has not yet been enabled\n",
			dev_name(&sport->sdev->device->dev), port_num);
		goto reject;
	}

	if (*(__be64 *)req->target_port_id != cpu_to_be64(srpt_service_guid)
	    || *(__be64 *)(req->target_port_id + 8) !=
	       cpu_to_be64(srpt_service_guid)) {
		rej->reason = cpu_to_be32(
				SRP_LOGIN_REJ_UNABLE_ASSOCIATE_CHANNEL);
		pr_err("rejected SRP_LOGIN_REQ because it has an invalid target port identifier.\n");
		goto reject;
	}

	ret = -ENOMEM;
	ch = kzalloc(sizeof(*ch), GFP_KERNEL);
	if (!ch) {
		rej->reason = cpu_to_be32(SRP_LOGIN_REJ_INSUFFICIENT_RESOURCES);
		pr_err("rejected SRP_LOGIN_REQ because out of memory.\n");
		goto reject;
	}

	kref_init(&ch->kref);
	ch->pkey = be16_to_cpu(pkey);
	ch->nexus = nexus;
	ch->zw_cqe.done = srpt_zerolength_write_done;
	INIT_WORK(&ch->release_work, srpt_release_channel_work);
	ch->sport = sport;
	if (rdma_cm_id) {
		ch->using_rdma_cm = true;
		ch->rdma_cm.cm_id = rdma_cm_id;
		rdma_cm_id->context = ch;
	} else {
		ch->ib_cm.cm_id = ib_cm_id;
		ib_cm_id->context = ch;
	}
	 
	ch->rq_size = min(MAX_SRPT_RQ_SIZE, sdev->device->attrs.max_qp_wr);
	spin_lock_init(&ch->spinlock);
	ch->state = CH_CONNECTING;
	INIT_LIST_HEAD(&ch->cmd_wait_list);
	ch->max_rsp_size = ch->sport->port_attrib.srp_max_rsp_size;

	ch->rsp_buf_cache = kmem_cache_create("srpt-rsp-buf", ch->max_rsp_size,
					      512, 0, NULL);
	if (!ch->rsp_buf_cache)
		goto free_ch;

	ch->ioctx_ring = (struct srpt_send_ioctx **)
		srpt_alloc_ioctx_ring(ch->sport->sdev, ch->rq_size,
				      sizeof(*ch->ioctx_ring[0]),
				      ch->rsp_buf_cache, 0, DMA_TO_DEVICE);
	if (!ch->ioctx_ring) {
		pr_err("rejected SRP_LOGIN_REQ because creating a new QP SQ ring failed.\n");
		rej->reason = cpu_to_be32(SRP_LOGIN_REJ_INSUFFICIENT_RESOURCES);
		goto free_rsp_cache;
	}

	for (i = 0; i < ch->rq_size; i++)
		ch->ioctx_ring[i]->ch = ch;
	if (!sdev->use_srq) {
		u16 imm_data_offset = req->req_flags & SRP_IMMED_REQUESTED ?
			be16_to_cpu(req->imm_data_offset) : 0;
		u16 alignment_offset;
		u32 req_sz;

		if (req->req_flags & SRP_IMMED_REQUESTED)
			pr_debug("imm_data_offset = %d\n",
				 be16_to_cpu(req->imm_data_offset));
		if (imm_data_offset >= sizeof(struct srp_cmd)) {
			ch->imm_data_offset = imm_data_offset;
			rsp->rsp_flags |= SRP_LOGIN_RSP_IMMED_SUPP;
		} else {
			ch->imm_data_offset = 0;
		}
		alignment_offset = round_up(imm_data_offset, 512) -
			imm_data_offset;
		req_sz = alignment_offset + imm_data_offset + srp_max_req_size;
		ch->req_buf_cache = kmem_cache_create("srpt-req-buf", req_sz,
						      512, 0, NULL);
		if (!ch->req_buf_cache)
			goto free_rsp_ring;

		ch->ioctx_recv_ring = (struct srpt_recv_ioctx **)
			srpt_alloc_ioctx_ring(ch->sport->sdev, ch->rq_size,
					      sizeof(*ch->ioctx_recv_ring[0]),
					      ch->req_buf_cache,
					      alignment_offset,
					      DMA_FROM_DEVICE);
		if (!ch->ioctx_recv_ring) {
			pr_err("rejected SRP_LOGIN_REQ because creating a new QP RQ ring failed.\n");
			rej->reason =
			    cpu_to_be32(SRP_LOGIN_REJ_INSUFFICIENT_RESOURCES);
			goto free_recv_cache;
		}
		for (i = 0; i < ch->rq_size; i++)
			INIT_LIST_HEAD(&ch->ioctx_recv_ring[i]->wait_list);
	}

	ret = srpt_create_ch_ib(ch);
	if (ret) {
		rej->reason = cpu_to_be32(SRP_LOGIN_REJ_INSUFFICIENT_RESOURCES);
		pr_err("rejected SRP_LOGIN_REQ because creating a new RDMA channel failed.\n");
		goto free_recv_ring;
	}

	strscpy(ch->sess_name, src_addr, sizeof(ch->sess_name));
	snprintf(i_port_id, sizeof(i_port_id), "0x%016llx%016llx",
			be64_to_cpu(*(__be64 *)nexus->i_port_id),
			be64_to_cpu(*(__be64 *)(nexus->i_port_id + 8)));

	pr_debug("registering src addr %s or i_port_id %s\n", ch->sess_name,
		 i_port_id);

	tag_num = ch->rq_size;
	tag_size = 1;  

	if (sport->guid_id) {
		mutex_lock(&sport->guid_id->mutex);
		list_for_each_entry(stpg, &sport->guid_id->tpg_list, entry) {
			if (!IS_ERR_OR_NULL(ch->sess))
				break;
			ch->sess = target_setup_session(&stpg->tpg, tag_num,
						tag_size, TARGET_PROT_NORMAL,
						ch->sess_name, ch, NULL);
		}
		mutex_unlock(&sport->guid_id->mutex);
	}

	if (sport->gid_id) {
		mutex_lock(&sport->gid_id->mutex);
		list_for_each_entry(stpg, &sport->gid_id->tpg_list, entry) {
			if (!IS_ERR_OR_NULL(ch->sess))
				break;
			ch->sess = target_setup_session(&stpg->tpg, tag_num,
					tag_size, TARGET_PROT_NORMAL, i_port_id,
					ch, NULL);
			if (!IS_ERR_OR_NULL(ch->sess))
				break;
			 
			ch->sess = target_setup_session(&stpg->tpg, tag_num,
						tag_size, TARGET_PROT_NORMAL,
						i_port_id + 2, ch, NULL);
		}
		mutex_unlock(&sport->gid_id->mutex);
	}

	if (IS_ERR_OR_NULL(ch->sess)) {
		WARN_ON_ONCE(ch->sess == NULL);
		ret = PTR_ERR(ch->sess);
		ch->sess = NULL;
		pr_info("Rejected login for initiator %s: ret = %d.\n",
			ch->sess_name, ret);
		rej->reason = cpu_to_be32(ret == -ENOMEM ?
				SRP_LOGIN_REJ_INSUFFICIENT_RESOURCES :
				SRP_LOGIN_REJ_CHANNEL_LIMIT_REACHED);
		goto destroy_ib;
	}

	 
	atomic_inc(&sport->refcount);

	mutex_lock(&sport->mutex);

	if ((req->req_flags & SRP_MTCH_ACTION) == SRP_MULTICHAN_SINGLE) {
		struct srpt_rdma_ch *ch2;

		list_for_each_entry(ch2, &nexus->ch_list, list) {
			if (srpt_disconnect_ch(ch2) < 0)
				continue;
			pr_info("Relogin - closed existing channel %s\n",
				ch2->sess_name);
			rsp->rsp_flags |= SRP_LOGIN_RSP_MULTICHAN_TERMINATED;
		}
	} else {
		rsp->rsp_flags |= SRP_LOGIN_RSP_MULTICHAN_MAINTAINED;
	}

	list_add_tail_rcu(&ch->list, &nexus->ch_list);

	if (!sport->enabled) {
		rej->reason = cpu_to_be32(
				SRP_LOGIN_REJ_INSUFFICIENT_RESOURCES);
		pr_info("rejected SRP_LOGIN_REQ because target %s_%d is not enabled\n",
			dev_name(&sdev->device->dev), port_num);
		mutex_unlock(&sport->mutex);
		ret = -EINVAL;
		goto reject;
	}

	mutex_unlock(&sport->mutex);

	ret = ch->using_rdma_cm ? 0 : srpt_ch_qp_rtr(ch, ch->qp);
	if (ret) {
		rej->reason = cpu_to_be32(SRP_LOGIN_REJ_INSUFFICIENT_RESOURCES);
		pr_err("rejected SRP_LOGIN_REQ because enabling RTR failed (error code = %d)\n",
		       ret);
		goto reject;
	}

	pr_debug("Establish connection sess=%p name=%s ch=%p\n", ch->sess,
		 ch->sess_name, ch);

	 
	rsp->opcode = SRP_LOGIN_RSP;
	rsp->tag = req->tag;
	rsp->max_it_iu_len = cpu_to_be32(srp_max_req_size);
	rsp->max_ti_iu_len = req->req_it_iu_len;
	ch->max_ti_iu_len = it_iu_len;
	rsp->buf_fmt = cpu_to_be16(SRP_BUF_FORMAT_DIRECT |
				   SRP_BUF_FORMAT_INDIRECT);
	rsp->req_lim_delta = cpu_to_be32(ch->rq_size);
	atomic_set(&ch->req_lim, ch->rq_size);
	atomic_set(&ch->req_lim_delta, 0);

	 
	if (ch->using_rdma_cm) {
		rep_param->rdma_cm.private_data = (void *)rsp;
		rep_param->rdma_cm.private_data_len = sizeof(*rsp);
		rep_param->rdma_cm.rnr_retry_count = 7;
		rep_param->rdma_cm.flow_control = 1;
		rep_param->rdma_cm.responder_resources = 4;
		rep_param->rdma_cm.initiator_depth = 4;
	} else {
		rep_param->ib_cm.qp_num = ch->qp->qp_num;
		rep_param->ib_cm.private_data = (void *)rsp;
		rep_param->ib_cm.private_data_len = sizeof(*rsp);
		rep_param->ib_cm.rnr_retry_count = 7;
		rep_param->ib_cm.flow_control = 1;
		rep_param->ib_cm.failover_accepted = 0;
		rep_param->ib_cm.srq = 1;
		rep_param->ib_cm.responder_resources = 4;
		rep_param->ib_cm.initiator_depth = 4;
	}

	 
	mutex_lock(&sport->mutex);
	if (sport->enabled && ch->state == CH_CONNECTING) {
		if (ch->using_rdma_cm)
			ret = rdma_accept(rdma_cm_id, &rep_param->rdma_cm);
		else
			ret = ib_send_cm_rep(ib_cm_id, &rep_param->ib_cm);
	} else {
		ret = -EINVAL;
	}
	mutex_unlock(&sport->mutex);

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		goto reject;
	default:
		rej->reason = cpu_to_be32(SRP_LOGIN_REJ_INSUFFICIENT_RESOURCES);
		pr_err("sending SRP_LOGIN_REQ response failed (error code = %d)\n",
		       ret);
		goto reject;
	}

	goto out;

destroy_ib:
	srpt_destroy_ch_ib(ch);

free_recv_ring:
	srpt_free_ioctx_ring((struct srpt_ioctx **)ch->ioctx_recv_ring,
			     ch->sport->sdev, ch->rq_size,
			     ch->req_buf_cache, DMA_FROM_DEVICE);

free_recv_cache:
	kmem_cache_destroy(ch->req_buf_cache);

free_rsp_ring:
	srpt_free_ioctx_ring((struct srpt_ioctx **)ch->ioctx_ring,
			     ch->sport->sdev, ch->rq_size,
			     ch->rsp_buf_cache, DMA_TO_DEVICE);

free_rsp_cache:
	kmem_cache_destroy(ch->rsp_buf_cache);

free_ch:
	if (rdma_cm_id)
		rdma_cm_id->context = NULL;
	else
		ib_cm_id->context = NULL;
	kfree(ch);
	ch = NULL;

	WARN_ON_ONCE(ret == 0);

reject:
	pr_info("Rejecting login with reason %#x\n", be32_to_cpu(rej->reason));
	rej->opcode = SRP_LOGIN_REJ;
	rej->tag = req->tag;
	rej->buf_fmt = cpu_to_be16(SRP_BUF_FORMAT_DIRECT |
				   SRP_BUF_FORMAT_INDIRECT);

	if (rdma_cm_id)
		rdma_reject(rdma_cm_id, rej, sizeof(*rej),
			    IB_CM_REJ_CONSUMER_DEFINED);
	else
		ib_send_cm_rej(ib_cm_id, IB_CM_REJ_CONSUMER_DEFINED, NULL, 0,
			       rej, sizeof(*rej));

	if (ch && ch->sess) {
		srpt_close_ch(ch);
		 
		ret = 0;
	}

out:
	kfree(rep_param);
	kfree(rsp);
	kfree(rej);

	return ret;
}

static int srpt_ib_cm_req_recv(struct ib_cm_id *cm_id,
			       const struct ib_cm_req_event_param *param,
			       void *private_data)
{
	char sguid[40];

	srpt_format_guid(sguid, sizeof(sguid),
			 &param->primary_path->dgid.global.interface_id);

	return srpt_cm_req_recv(cm_id->context, cm_id, NULL, param->port,
				param->primary_path->pkey,
				private_data, sguid);
}

static int srpt_rdma_cm_req_recv(struct rdma_cm_id *cm_id,
				 struct rdma_cm_event *event)
{
	struct srpt_device *sdev;
	struct srp_login_req req;
	const struct srp_login_req_rdma *req_rdma;
	struct sa_path_rec *path_rec = cm_id->route.path_rec;
	char src_addr[40];

	sdev = ib_get_client_data(cm_id->device, &srpt_client);
	if (!sdev)
		return -ECONNREFUSED;

	if (event->param.conn.private_data_len < sizeof(*req_rdma))
		return -EINVAL;

	 
	req_rdma = event->param.conn.private_data;
	memset(&req, 0, sizeof(req));
	req.opcode		= req_rdma->opcode;
	req.tag			= req_rdma->tag;
	req.req_it_iu_len	= req_rdma->req_it_iu_len;
	req.req_buf_fmt		= req_rdma->req_buf_fmt;
	req.req_flags		= req_rdma->req_flags;
	memcpy(req.initiator_port_id, req_rdma->initiator_port_id, 16);
	memcpy(req.target_port_id, req_rdma->target_port_id, 16);
	req.imm_data_offset	= req_rdma->imm_data_offset;

	snprintf(src_addr, sizeof(src_addr), "%pIS",
		 &cm_id->route.addr.src_addr);

	return srpt_cm_req_recv(sdev, NULL, cm_id, cm_id->port_num,
				path_rec ? path_rec->pkey : 0, &req, src_addr);
}

static void srpt_cm_rej_recv(struct srpt_rdma_ch *ch,
			     enum ib_cm_rej_reason reason,
			     const u8 *private_data,
			     u8 private_data_len)
{
	char *priv = NULL;
	int i;

	if (private_data_len && (priv = kmalloc(private_data_len * 3 + 1,
						GFP_KERNEL))) {
		for (i = 0; i < private_data_len; i++)
			sprintf(priv + 3 * i, " %02x", private_data[i]);
	}
	pr_info("Received CM REJ for ch %s-%d; reason %d%s%s.\n",
		ch->sess_name, ch->qp->qp_num, reason, private_data_len ?
		"; private data" : "", priv ? priv : " (?)");
	kfree(priv);
}

 
static void srpt_cm_rtu_recv(struct srpt_rdma_ch *ch)
{
	int ret;

	ret = ch->using_rdma_cm ? 0 : srpt_ch_qp_rts(ch, ch->qp);
	if (ret < 0) {
		pr_err("%s-%d: QP transition to RTS failed\n", ch->sess_name,
		       ch->qp->qp_num);
		srpt_close_ch(ch);
		return;
	}

	 
	if (!srpt_set_ch_state(ch, CH_LIVE)) {
		pr_err("%s-%d: channel transition to LIVE state failed\n",
		       ch->sess_name, ch->qp->qp_num);
		return;
	}

	 
	ret = srpt_zerolength_write(ch);
	WARN_ONCE(ret < 0, "%d\n", ret);
}

 
static int srpt_cm_handler(struct ib_cm_id *cm_id,
			   const struct ib_cm_event *event)
{
	struct srpt_rdma_ch *ch = cm_id->context;
	int ret;

	ret = 0;
	switch (event->event) {
	case IB_CM_REQ_RECEIVED:
		ret = srpt_ib_cm_req_recv(cm_id, &event->param.req_rcvd,
					  event->private_data);
		break;
	case IB_CM_REJ_RECEIVED:
		srpt_cm_rej_recv(ch, event->param.rej_rcvd.reason,
				 event->private_data,
				 IB_CM_REJ_PRIVATE_DATA_SIZE);
		break;
	case IB_CM_RTU_RECEIVED:
	case IB_CM_USER_ESTABLISHED:
		srpt_cm_rtu_recv(ch);
		break;
	case IB_CM_DREQ_RECEIVED:
		srpt_disconnect_ch(ch);
		break;
	case IB_CM_DREP_RECEIVED:
		pr_info("Received CM DREP message for ch %s-%d.\n",
			ch->sess_name, ch->qp->qp_num);
		srpt_close_ch(ch);
		break;
	case IB_CM_TIMEWAIT_EXIT:
		pr_info("Received CM TimeWait exit for ch %s-%d.\n",
			ch->sess_name, ch->qp->qp_num);
		srpt_close_ch(ch);
		break;
	case IB_CM_REP_ERROR:
		pr_info("Received CM REP error for ch %s-%d.\n", ch->sess_name,
			ch->qp->qp_num);
		break;
	case IB_CM_DREQ_ERROR:
		pr_info("Received CM DREQ ERROR event.\n");
		break;
	case IB_CM_MRA_RECEIVED:
		pr_info("Received CM MRA event\n");
		break;
	default:
		pr_err("received unrecognized CM event %d\n", event->event);
		break;
	}

	return ret;
}

static int srpt_rdma_cm_handler(struct rdma_cm_id *cm_id,
				struct rdma_cm_event *event)
{
	struct srpt_rdma_ch *ch = cm_id->context;
	int ret = 0;

	switch (event->event) {
	case RDMA_CM_EVENT_CONNECT_REQUEST:
		ret = srpt_rdma_cm_req_recv(cm_id, event);
		break;
	case RDMA_CM_EVENT_REJECTED:
		srpt_cm_rej_recv(ch, event->status,
				 event->param.conn.private_data,
				 event->param.conn.private_data_len);
		break;
	case RDMA_CM_EVENT_ESTABLISHED:
		srpt_cm_rtu_recv(ch);
		break;
	case RDMA_CM_EVENT_DISCONNECTED:
		if (ch->state < CH_DISCONNECTING)
			srpt_disconnect_ch(ch);
		else
			srpt_close_ch(ch);
		break;
	case RDMA_CM_EVENT_TIMEWAIT_EXIT:
		srpt_close_ch(ch);
		break;
	case RDMA_CM_EVENT_UNREACHABLE:
		pr_info("Received CM REP error for ch %s-%d.\n", ch->sess_name,
			ch->qp->qp_num);
		break;
	case RDMA_CM_EVENT_DEVICE_REMOVAL:
	case RDMA_CM_EVENT_ADDR_CHANGE:
		break;
	default:
		pr_err("received unrecognized RDMA CM event %d\n",
		       event->event);
		break;
	}

	return ret;
}

 
static int srpt_write_pending(struct se_cmd *se_cmd)
{
	struct srpt_send_ioctx *ioctx =
		container_of(se_cmd, struct srpt_send_ioctx, cmd);
	struct srpt_rdma_ch *ch = ioctx->ch;
	struct ib_send_wr *first_wr = NULL;
	struct ib_cqe *cqe = &ioctx->rdma_cqe;
	enum srpt_command_state new_state;
	int ret, i;

	if (ioctx->recv_ioctx) {
		srpt_set_cmd_state(ioctx, SRPT_STATE_DATA_IN);
		target_execute_cmd(&ioctx->cmd);
		return 0;
	}

	new_state = srpt_set_cmd_state(ioctx, SRPT_STATE_NEED_DATA);
	WARN_ON(new_state == SRPT_STATE_DONE);

	if (atomic_sub_return(ioctx->n_rdma, &ch->sq_wr_avail) < 0) {
		pr_warn("%s: IB send queue full (needed %d)\n",
				__func__, ioctx->n_rdma);
		ret = -ENOMEM;
		goto out_undo;
	}

	cqe->done = srpt_rdma_read_done;
	for (i = ioctx->n_rw_ctx - 1; i >= 0; i--) {
		struct srpt_rw_ctx *ctx = &ioctx->rw_ctxs[i];

		first_wr = rdma_rw_ctx_wrs(&ctx->rw, ch->qp, ch->sport->port,
				cqe, first_wr);
		cqe = NULL;
	}

	ret = ib_post_send(ch->qp, first_wr, NULL);
	if (ret) {
		pr_err("%s: ib_post_send() returned %d for %d (avail: %d)\n",
			 __func__, ret, ioctx->n_rdma,
			 atomic_read(&ch->sq_wr_avail));
		goto out_undo;
	}

	return 0;
out_undo:
	atomic_add(ioctx->n_rdma, &ch->sq_wr_avail);
	return ret;
}

static u8 tcm_to_srp_tsk_mgmt_status(const int tcm_mgmt_status)
{
	switch (tcm_mgmt_status) {
	case TMR_FUNCTION_COMPLETE:
		return SRP_TSK_MGMT_SUCCESS;
	case TMR_FUNCTION_REJECTED:
		return SRP_TSK_MGMT_FUNC_NOT_SUPP;
	}
	return SRP_TSK_MGMT_FAILED;
}

 
static void srpt_queue_response(struct se_cmd *cmd)
{
	struct srpt_send_ioctx *ioctx =
		container_of(cmd, struct srpt_send_ioctx, cmd);
	struct srpt_rdma_ch *ch = ioctx->ch;
	struct srpt_device *sdev = ch->sport->sdev;
	struct ib_send_wr send_wr, *first_wr = &send_wr;
	struct ib_sge sge;
	enum srpt_command_state state;
	int resp_len, ret, i;
	u8 srp_tm_status;

	state = ioctx->state;
	switch (state) {
	case SRPT_STATE_NEW:
	case SRPT_STATE_DATA_IN:
		ioctx->state = SRPT_STATE_CMD_RSP_SENT;
		break;
	case SRPT_STATE_MGMT:
		ioctx->state = SRPT_STATE_MGMT_RSP_SENT;
		break;
	default:
		WARN(true, "ch %p; cmd %d: unexpected command state %d\n",
			ch, ioctx->ioctx.index, ioctx->state);
		break;
	}

	if (WARN_ON_ONCE(state == SRPT_STATE_CMD_RSP_SENT))
		return;

	 
	if (ioctx->cmd.data_direction == DMA_FROM_DEVICE &&
	    ioctx->cmd.data_length &&
	    !ioctx->queue_status_only) {
		for (i = ioctx->n_rw_ctx - 1; i >= 0; i--) {
			struct srpt_rw_ctx *ctx = &ioctx->rw_ctxs[i];

			first_wr = rdma_rw_ctx_wrs(&ctx->rw, ch->qp,
					ch->sport->port, NULL, first_wr);
		}
	}

	if (state != SRPT_STATE_MGMT)
		resp_len = srpt_build_cmd_rsp(ch, ioctx, ioctx->cmd.tag,
					      cmd->scsi_status);
	else {
		srp_tm_status
			= tcm_to_srp_tsk_mgmt_status(cmd->se_tmr_req->response);
		resp_len = srpt_build_tskmgmt_rsp(ch, ioctx, srp_tm_status,
						 ioctx->cmd.tag);
	}

	atomic_inc(&ch->req_lim);

	if (unlikely(atomic_sub_return(1 + ioctx->n_rdma,
			&ch->sq_wr_avail) < 0)) {
		pr_warn("%s: IB send queue full (needed %d)\n",
				__func__, ioctx->n_rdma);
		goto out;
	}

	ib_dma_sync_single_for_device(sdev->device, ioctx->ioctx.dma, resp_len,
				      DMA_TO_DEVICE);

	sge.addr = ioctx->ioctx.dma;
	sge.length = resp_len;
	sge.lkey = sdev->lkey;

	ioctx->ioctx.cqe.done = srpt_send_done;
	send_wr.next = NULL;
	send_wr.wr_cqe = &ioctx->ioctx.cqe;
	send_wr.sg_list = &sge;
	send_wr.num_sge = 1;
	send_wr.opcode = IB_WR_SEND;
	send_wr.send_flags = IB_SEND_SIGNALED;

	ret = ib_post_send(ch->qp, first_wr, NULL);
	if (ret < 0) {
		pr_err("%s: sending cmd response failed for tag %llu (%d)\n",
			__func__, ioctx->cmd.tag, ret);
		goto out;
	}

	return;

out:
	atomic_add(1 + ioctx->n_rdma, &ch->sq_wr_avail);
	atomic_dec(&ch->req_lim);
	srpt_set_cmd_state(ioctx, SRPT_STATE_DONE);
	target_put_sess_cmd(&ioctx->cmd);
}

static int srpt_queue_data_in(struct se_cmd *cmd)
{
	srpt_queue_response(cmd);
	return 0;
}

static void srpt_queue_tm_rsp(struct se_cmd *cmd)
{
	srpt_queue_response(cmd);
}

 
static void srpt_aborted_task(struct se_cmd *cmd)
{
	struct srpt_send_ioctx *ioctx = container_of(cmd,
				struct srpt_send_ioctx, cmd);
	struct srpt_rdma_ch *ch = ioctx->ch;

	atomic_inc(&ch->req_lim_delta);
}

static int srpt_queue_status(struct se_cmd *cmd)
{
	struct srpt_send_ioctx *ioctx;

	ioctx = container_of(cmd, struct srpt_send_ioctx, cmd);
	BUG_ON(ioctx->sense_data != cmd->sense_buffer);
	if (cmd->se_cmd_flags &
	    (SCF_TRANSPORT_TASK_SENSE | SCF_EMULATED_TASK_SENSE))
		WARN_ON(cmd->scsi_status != SAM_STAT_CHECK_CONDITION);
	ioctx->queue_status_only = true;
	srpt_queue_response(cmd);
	return 0;
}

static void srpt_refresh_port_work(struct work_struct *work)
{
	struct srpt_port *sport = container_of(work, struct srpt_port, work);

	srpt_refresh_port(sport);
}

 
static int srpt_release_sport(struct srpt_port *sport)
{
	DECLARE_COMPLETION_ONSTACK(c);
	struct srpt_nexus *nexus, *next_n;
	struct srpt_rdma_ch *ch;

	WARN_ON_ONCE(irqs_disabled());

	sport->freed_channels = &c;

	mutex_lock(&sport->mutex);
	srpt_set_enabled(sport, false);
	mutex_unlock(&sport->mutex);

	while (atomic_read(&sport->refcount) > 0 &&
	       wait_for_completion_timeout(&c, 5 * HZ) <= 0) {
		pr_info("%s_%d: waiting for unregistration of %d sessions ...\n",
			dev_name(&sport->sdev->device->dev), sport->port,
			atomic_read(&sport->refcount));
		rcu_read_lock();
		list_for_each_entry(nexus, &sport->nexus_list, entry) {
			list_for_each_entry(ch, &nexus->ch_list, list) {
				pr_info("%s-%d: state %s\n",
					ch->sess_name, ch->qp->qp_num,
					get_ch_state_name(ch->state));
			}
		}
		rcu_read_unlock();
	}

	mutex_lock(&sport->mutex);
	list_for_each_entry_safe(nexus, next_n, &sport->nexus_list, entry) {
		list_del(&nexus->entry);
		kfree_rcu(nexus, rcu);
	}
	mutex_unlock(&sport->mutex);

	return 0;
}

struct port_and_port_id {
	struct srpt_port *sport;
	struct srpt_port_id **port_id;
};

static struct port_and_port_id __srpt_lookup_port(const char *name)
{
	struct ib_device *dev;
	struct srpt_device *sdev;
	struct srpt_port *sport;
	int i;

	list_for_each_entry(sdev, &srpt_dev_list, list) {
		dev = sdev->device;
		if (!dev)
			continue;

		for (i = 0; i < dev->phys_port_cnt; i++) {
			sport = &sdev->port[i];

			if (strcmp(sport->guid_name, name) == 0) {
				kref_get(&sdev->refcnt);
				return (struct port_and_port_id){
					sport, &sport->guid_id};
			}
			if (strcmp(sport->gid_name, name) == 0) {
				kref_get(&sdev->refcnt);
				return (struct port_and_port_id){
					sport, &sport->gid_id};
			}
		}
	}

	return (struct port_and_port_id){};
}

 
static struct port_and_port_id srpt_lookup_port(const char *name)
{
	struct port_and_port_id papi;

	spin_lock(&srpt_dev_lock);
	papi = __srpt_lookup_port(name);
	spin_unlock(&srpt_dev_lock);

	return papi;
}

static void srpt_free_srq(struct srpt_device *sdev)
{
	if (!sdev->srq)
		return;

	ib_destroy_srq(sdev->srq);
	srpt_free_ioctx_ring((struct srpt_ioctx **)sdev->ioctx_ring, sdev,
			     sdev->srq_size, sdev->req_buf_cache,
			     DMA_FROM_DEVICE);
	kmem_cache_destroy(sdev->req_buf_cache);
	sdev->srq = NULL;
}

static int srpt_alloc_srq(struct srpt_device *sdev)
{
	struct ib_srq_init_attr srq_attr = {
		.event_handler = srpt_srq_event,
		.srq_context = (void *)sdev,
		.attr.max_wr = sdev->srq_size,
		.attr.max_sge = 1,
		.srq_type = IB_SRQT_BASIC,
	};
	struct ib_device *device = sdev->device;
	struct ib_srq *srq;
	int i;

	WARN_ON_ONCE(sdev->srq);
	srq = ib_create_srq(sdev->pd, &srq_attr);
	if (IS_ERR(srq)) {
		pr_debug("ib_create_srq() failed: %ld\n", PTR_ERR(srq));
		return PTR_ERR(srq);
	}

	pr_debug("create SRQ #wr= %d max_allow=%d dev= %s\n", sdev->srq_size,
		 sdev->device->attrs.max_srq_wr, dev_name(&device->dev));

	sdev->req_buf_cache = kmem_cache_create("srpt-srq-req-buf",
						srp_max_req_size, 0, 0, NULL);
	if (!sdev->req_buf_cache)
		goto free_srq;

	sdev->ioctx_ring = (struct srpt_recv_ioctx **)
		srpt_alloc_ioctx_ring(sdev, sdev->srq_size,
				      sizeof(*sdev->ioctx_ring[0]),
				      sdev->req_buf_cache, 0, DMA_FROM_DEVICE);
	if (!sdev->ioctx_ring)
		goto free_cache;

	sdev->use_srq = true;
	sdev->srq = srq;

	for (i = 0; i < sdev->srq_size; ++i) {
		INIT_LIST_HEAD(&sdev->ioctx_ring[i]->wait_list);
		srpt_post_recv(sdev, NULL, sdev->ioctx_ring[i]);
	}

	return 0;

free_cache:
	kmem_cache_destroy(sdev->req_buf_cache);

free_srq:
	ib_destroy_srq(srq);
	return -ENOMEM;
}

static int srpt_use_srq(struct srpt_device *sdev, bool use_srq)
{
	struct ib_device *device = sdev->device;
	int ret = 0;

	if (!use_srq) {
		srpt_free_srq(sdev);
		sdev->use_srq = false;
	} else if (use_srq && !sdev->srq) {
		ret = srpt_alloc_srq(sdev);
	}
	pr_debug("%s(%s): use_srq = %d; ret = %d\n", __func__,
		 dev_name(&device->dev), sdev->use_srq, ret);
	return ret;
}

static void srpt_free_sdev(struct kref *refcnt)
{
	struct srpt_device *sdev = container_of(refcnt, typeof(*sdev), refcnt);

	kfree(sdev);
}

static void srpt_sdev_put(struct srpt_device *sdev)
{
	kref_put(&sdev->refcnt, srpt_free_sdev);
}

 
static int srpt_add_one(struct ib_device *device)
{
	struct srpt_device *sdev;
	struct srpt_port *sport;
	int ret;
	u32 i;

	pr_debug("device = %p\n", device);

	sdev = kzalloc(struct_size(sdev, port, device->phys_port_cnt),
		       GFP_KERNEL);
	if (!sdev)
		return -ENOMEM;

	kref_init(&sdev->refcnt);
	sdev->device = device;
	mutex_init(&sdev->sdev_mutex);

	sdev->pd = ib_alloc_pd(device, 0);
	if (IS_ERR(sdev->pd)) {
		ret = PTR_ERR(sdev->pd);
		goto free_dev;
	}

	sdev->lkey = sdev->pd->local_dma_lkey;

	sdev->srq_size = min(srpt_srq_size, sdev->device->attrs.max_srq_wr);

	srpt_use_srq(sdev, sdev->port[0].port_attrib.use_srq);

	if (!srpt_service_guid)
		srpt_service_guid = be64_to_cpu(device->node_guid);

	if (rdma_port_get_link_layer(device, 1) == IB_LINK_LAYER_INFINIBAND)
		sdev->cm_id = ib_create_cm_id(device, srpt_cm_handler, sdev);
	if (IS_ERR(sdev->cm_id)) {
		pr_info("ib_create_cm_id() failed: %ld\n",
			PTR_ERR(sdev->cm_id));
		ret = PTR_ERR(sdev->cm_id);
		sdev->cm_id = NULL;
		if (!rdma_cm_id)
			goto err_ring;
	}

	 
	pr_debug("Target login info: id_ext=%016llx,ioc_guid=%016llx,pkey=ffff,service_id=%016llx\n",
		 srpt_service_guid, srpt_service_guid, srpt_service_guid);

	 
	ret = sdev->cm_id ?
		ib_cm_listen(sdev->cm_id, cpu_to_be64(srpt_service_guid)) :
		0;
	if (ret < 0) {
		pr_err("ib_cm_listen() failed: %d (cm_id state = %d)\n", ret,
		       sdev->cm_id->state);
		goto err_cm;
	}

	INIT_IB_EVENT_HANDLER(&sdev->event_handler, sdev->device,
			      srpt_event_handler);
	ib_register_event_handler(&sdev->event_handler);

	for (i = 1; i <= sdev->device->phys_port_cnt; i++) {
		sport = &sdev->port[i - 1];
		INIT_LIST_HEAD(&sport->nexus_list);
		mutex_init(&sport->mutex);
		sport->sdev = sdev;
		sport->port = i;
		sport->port_attrib.srp_max_rdma_size = DEFAULT_MAX_RDMA_SIZE;
		sport->port_attrib.srp_max_rsp_size = DEFAULT_MAX_RSP_SIZE;
		sport->port_attrib.srp_sq_size = DEF_SRPT_SQ_SIZE;
		sport->port_attrib.use_srq = false;
		INIT_WORK(&sport->work, srpt_refresh_port_work);

		ret = srpt_refresh_port(sport);
		if (ret) {
			pr_err("MAD registration failed for %s-%d.\n",
			       dev_name(&sdev->device->dev), i);
			i--;
			goto err_port;
		}
	}

	spin_lock(&srpt_dev_lock);
	list_add_tail(&sdev->list, &srpt_dev_list);
	spin_unlock(&srpt_dev_lock);

	ib_set_client_data(device, &srpt_client, sdev);
	pr_debug("added %s.\n", dev_name(&device->dev));
	return 0;

err_port:
	srpt_unregister_mad_agent(sdev, i);
	ib_unregister_event_handler(&sdev->event_handler);
err_cm:
	if (sdev->cm_id)
		ib_destroy_cm_id(sdev->cm_id);
err_ring:
	srpt_free_srq(sdev);
	ib_dealloc_pd(sdev->pd);
free_dev:
	srpt_sdev_put(sdev);
	pr_info("%s(%s) failed.\n", __func__, dev_name(&device->dev));
	return ret;
}

 
static void srpt_remove_one(struct ib_device *device, void *client_data)
{
	struct srpt_device *sdev = client_data;
	int i;

	srpt_unregister_mad_agent(sdev, sdev->device->phys_port_cnt);

	ib_unregister_event_handler(&sdev->event_handler);

	 
	for (i = 0; i < sdev->device->phys_port_cnt; i++)
		cancel_work_sync(&sdev->port[i].work);

	if (sdev->cm_id)
		ib_destroy_cm_id(sdev->cm_id);

	ib_set_client_data(device, &srpt_client, NULL);

	 
	spin_lock(&srpt_dev_lock);
	list_del(&sdev->list);
	spin_unlock(&srpt_dev_lock);

	for (i = 0; i < sdev->device->phys_port_cnt; i++)
		srpt_release_sport(&sdev->port[i]);

	srpt_free_srq(sdev);

	ib_dealloc_pd(sdev->pd);

	srpt_sdev_put(sdev);
}

static struct ib_client srpt_client = {
	.name = DRV_NAME,
	.add = srpt_add_one,
	.remove = srpt_remove_one
};

static int srpt_check_true(struct se_portal_group *se_tpg)
{
	return 1;
}

static struct srpt_port *srpt_tpg_to_sport(struct se_portal_group *tpg)
{
	return tpg->se_tpg_wwn->priv;
}

static struct srpt_port_id *srpt_wwn_to_sport_id(struct se_wwn *wwn)
{
	struct srpt_port *sport = wwn->priv;

	if (sport->guid_id && &sport->guid_id->wwn == wwn)
		return sport->guid_id;
	if (sport->gid_id && &sport->gid_id->wwn == wwn)
		return sport->gid_id;
	WARN_ON_ONCE(true);
	return NULL;
}

static char *srpt_get_fabric_wwn(struct se_portal_group *tpg)
{
	struct srpt_tpg *stpg = container_of(tpg, typeof(*stpg), tpg);

	return stpg->sport_id->name;
}

static u16 srpt_get_tag(struct se_portal_group *tpg)
{
	return 1;
}

static void srpt_release_cmd(struct se_cmd *se_cmd)
{
	struct srpt_send_ioctx *ioctx = container_of(se_cmd,
				struct srpt_send_ioctx, cmd);
	struct srpt_rdma_ch *ch = ioctx->ch;
	struct srpt_recv_ioctx *recv_ioctx = ioctx->recv_ioctx;

	WARN_ON_ONCE(ioctx->state != SRPT_STATE_DONE &&
		     !(ioctx->cmd.transport_state & CMD_T_ABORTED));

	if (recv_ioctx) {
		WARN_ON_ONCE(!list_empty(&recv_ioctx->wait_list));
		ioctx->recv_ioctx = NULL;
		srpt_post_recv(ch->sport->sdev, ch, recv_ioctx);
	}

	if (ioctx->n_rw_ctx) {
		srpt_free_rw_ctxs(ch, ioctx);
		ioctx->n_rw_ctx = 0;
	}

	target_free_tag(se_cmd->se_sess, se_cmd);
}

 
static void srpt_close_session(struct se_session *se_sess)
{
	struct srpt_rdma_ch *ch = se_sess->fabric_sess_ptr;

	srpt_disconnect_ch_sync(ch);
}

 
static int srpt_get_tcm_cmd_state(struct se_cmd *se_cmd)
{
	struct srpt_send_ioctx *ioctx;

	ioctx = container_of(se_cmd, struct srpt_send_ioctx, cmd);
	return ioctx->state;
}

static int srpt_parse_guid(u64 *guid, const char *name)
{
	u16 w[4];
	int ret = -EINVAL;

	if (sscanf(name, "%hx:%hx:%hx:%hx", &w[0], &w[1], &w[2], &w[3]) != 4)
		goto out;
	*guid = get_unaligned_be64(w);
	ret = 0;
out:
	return ret;
}

 
static int srpt_parse_i_port_id(u8 i_port_id[16], const char *name)
{
	const char *p;
	unsigned len, count, leading_zero_bytes;
	int ret;

	p = name;
	if (strncasecmp(p, "0x", 2) == 0)
		p += 2;
	ret = -EINVAL;
	len = strlen(p);
	if (len % 2)
		goto out;
	count = min(len / 2, 16U);
	leading_zero_bytes = 16 - count;
	memset(i_port_id, 0, leading_zero_bytes);
	ret = hex2bin(i_port_id + leading_zero_bytes, p, count);

out:
	return ret;
}

 
static int srpt_init_nodeacl(struct se_node_acl *se_nacl, const char *name)
{
	struct sockaddr_storage sa;
	u64 guid;
	u8 i_port_id[16];
	int ret;

	ret = srpt_parse_guid(&guid, name);
	if (ret < 0)
		ret = srpt_parse_i_port_id(i_port_id, name);
	if (ret < 0)
		ret = inet_pton_with_scope(&init_net, AF_UNSPEC, name, NULL,
					   &sa);
	if (ret < 0)
		pr_err("invalid initiator port ID %s\n", name);
	return ret;
}

static ssize_t srpt_tpg_attrib_srp_max_rdma_size_show(struct config_item *item,
		char *page)
{
	struct se_portal_group *se_tpg = attrib_to_tpg(item);
	struct srpt_port *sport = srpt_tpg_to_sport(se_tpg);

	return sysfs_emit(page, "%u\n", sport->port_attrib.srp_max_rdma_size);
}

static ssize_t srpt_tpg_attrib_srp_max_rdma_size_store(struct config_item *item,
		const char *page, size_t count)
{
	struct se_portal_group *se_tpg = attrib_to_tpg(item);
	struct srpt_port *sport = srpt_tpg_to_sport(se_tpg);
	unsigned long val;
	int ret;

	ret = kstrtoul(page, 0, &val);
	if (ret < 0) {
		pr_err("kstrtoul() failed with ret: %d\n", ret);
		return -EINVAL;
	}
	if (val > MAX_SRPT_RDMA_SIZE) {
		pr_err("val: %lu exceeds MAX_SRPT_RDMA_SIZE: %d\n", val,
			MAX_SRPT_RDMA_SIZE);
		return -EINVAL;
	}
	if (val < DEFAULT_MAX_RDMA_SIZE) {
		pr_err("val: %lu smaller than DEFAULT_MAX_RDMA_SIZE: %d\n",
			val, DEFAULT_MAX_RDMA_SIZE);
		return -EINVAL;
	}
	sport->port_attrib.srp_max_rdma_size = val;

	return count;
}

static ssize_t srpt_tpg_attrib_srp_max_rsp_size_show(struct config_item *item,
		char *page)
{
	struct se_portal_group *se_tpg = attrib_to_tpg(item);
	struct srpt_port *sport = srpt_tpg_to_sport(se_tpg);

	return sysfs_emit(page, "%u\n", sport->port_attrib.srp_max_rsp_size);
}

static ssize_t srpt_tpg_attrib_srp_max_rsp_size_store(struct config_item *item,
		const char *page, size_t count)
{
	struct se_portal_group *se_tpg = attrib_to_tpg(item);
	struct srpt_port *sport = srpt_tpg_to_sport(se_tpg);
	unsigned long val;
	int ret;

	ret = kstrtoul(page, 0, &val);
	if (ret < 0) {
		pr_err("kstrtoul() failed with ret: %d\n", ret);
		return -EINVAL;
	}
	if (val > MAX_SRPT_RSP_SIZE) {
		pr_err("val: %lu exceeds MAX_SRPT_RSP_SIZE: %d\n", val,
			MAX_SRPT_RSP_SIZE);
		return -EINVAL;
	}
	if (val < MIN_MAX_RSP_SIZE) {
		pr_err("val: %lu smaller than MIN_MAX_RSP_SIZE: %d\n", val,
			MIN_MAX_RSP_SIZE);
		return -EINVAL;
	}
	sport->port_attrib.srp_max_rsp_size = val;

	return count;
}

static ssize_t srpt_tpg_attrib_srp_sq_size_show(struct config_item *item,
		char *page)
{
	struct se_portal_group *se_tpg = attrib_to_tpg(item);
	struct srpt_port *sport = srpt_tpg_to_sport(se_tpg);

	return sysfs_emit(page, "%u\n", sport->port_attrib.srp_sq_size);
}

static ssize_t srpt_tpg_attrib_srp_sq_size_store(struct config_item *item,
		const char *page, size_t count)
{
	struct se_portal_group *se_tpg = attrib_to_tpg(item);
	struct srpt_port *sport = srpt_tpg_to_sport(se_tpg);
	unsigned long val;
	int ret;

	ret = kstrtoul(page, 0, &val);
	if (ret < 0) {
		pr_err("kstrtoul() failed with ret: %d\n", ret);
		return -EINVAL;
	}
	if (val > MAX_SRPT_SRQ_SIZE) {
		pr_err("val: %lu exceeds MAX_SRPT_SRQ_SIZE: %d\n", val,
			MAX_SRPT_SRQ_SIZE);
		return -EINVAL;
	}
	if (val < MIN_SRPT_SRQ_SIZE) {
		pr_err("val: %lu smaller than MIN_SRPT_SRQ_SIZE: %d\n", val,
			MIN_SRPT_SRQ_SIZE);
		return -EINVAL;
	}
	sport->port_attrib.srp_sq_size = val;

	return count;
}

static ssize_t srpt_tpg_attrib_use_srq_show(struct config_item *item,
					    char *page)
{
	struct se_portal_group *se_tpg = attrib_to_tpg(item);
	struct srpt_port *sport = srpt_tpg_to_sport(se_tpg);

	return sysfs_emit(page, "%d\n", sport->port_attrib.use_srq);
}

static ssize_t srpt_tpg_attrib_use_srq_store(struct config_item *item,
					     const char *page, size_t count)
{
	struct se_portal_group *se_tpg = attrib_to_tpg(item);
	struct srpt_port *sport = srpt_tpg_to_sport(se_tpg);
	struct srpt_device *sdev = sport->sdev;
	unsigned long val;
	bool enabled;
	int ret;

	ret = kstrtoul(page, 0, &val);
	if (ret < 0)
		return ret;
	if (val != !!val)
		return -EINVAL;

	ret = mutex_lock_interruptible(&sdev->sdev_mutex);
	if (ret < 0)
		return ret;
	ret = mutex_lock_interruptible(&sport->mutex);
	if (ret < 0)
		goto unlock_sdev;
	enabled = sport->enabled;
	 
	srpt_set_enabled(sport, false);
	sport->port_attrib.use_srq = val;
	srpt_use_srq(sdev, sport->port_attrib.use_srq);
	srpt_set_enabled(sport, enabled);
	ret = count;
	mutex_unlock(&sport->mutex);
unlock_sdev:
	mutex_unlock(&sdev->sdev_mutex);

	return ret;
}

CONFIGFS_ATTR(srpt_tpg_attrib_,  srp_max_rdma_size);
CONFIGFS_ATTR(srpt_tpg_attrib_,  srp_max_rsp_size);
CONFIGFS_ATTR(srpt_tpg_attrib_,  srp_sq_size);
CONFIGFS_ATTR(srpt_tpg_attrib_,  use_srq);

static struct configfs_attribute *srpt_tpg_attrib_attrs[] = {
	&srpt_tpg_attrib_attr_srp_max_rdma_size,
	&srpt_tpg_attrib_attr_srp_max_rsp_size,
	&srpt_tpg_attrib_attr_srp_sq_size,
	&srpt_tpg_attrib_attr_use_srq,
	NULL,
};

static struct rdma_cm_id *srpt_create_rdma_id(struct sockaddr *listen_addr)
{
	struct rdma_cm_id *rdma_cm_id;
	int ret;

	rdma_cm_id = rdma_create_id(&init_net, srpt_rdma_cm_handler,
				    NULL, RDMA_PS_TCP, IB_QPT_RC);
	if (IS_ERR(rdma_cm_id)) {
		pr_err("RDMA/CM ID creation failed: %ld\n",
		       PTR_ERR(rdma_cm_id));
		goto out;
	}

	ret = rdma_bind_addr(rdma_cm_id, listen_addr);
	if (ret) {
		char addr_str[64];

		snprintf(addr_str, sizeof(addr_str), "%pISp", listen_addr);
		pr_err("Binding RDMA/CM ID to address %s failed: %d\n",
		       addr_str, ret);
		rdma_destroy_id(rdma_cm_id);
		rdma_cm_id = ERR_PTR(ret);
		goto out;
	}

	ret = rdma_listen(rdma_cm_id, 128);
	if (ret) {
		pr_err("rdma_listen() failed: %d\n", ret);
		rdma_destroy_id(rdma_cm_id);
		rdma_cm_id = ERR_PTR(ret);
	}

out:
	return rdma_cm_id;
}

static ssize_t srpt_rdma_cm_port_show(struct config_item *item, char *page)
{
	return sysfs_emit(page, "%d\n", rdma_cm_port);
}

static ssize_t srpt_rdma_cm_port_store(struct config_item *item,
				       const char *page, size_t count)
{
	struct sockaddr_in  addr4 = { .sin_family  = AF_INET  };
	struct sockaddr_in6 addr6 = { .sin6_family = AF_INET6 };
	struct rdma_cm_id *new_id = NULL;
	u16 val;
	int ret;

	ret = kstrtou16(page, 0, &val);
	if (ret < 0)
		return ret;
	ret = count;
	if (rdma_cm_port == val)
		goto out;

	if (val) {
		addr6.sin6_port = cpu_to_be16(val);
		new_id = srpt_create_rdma_id((struct sockaddr *)&addr6);
		if (IS_ERR(new_id)) {
			addr4.sin_port = cpu_to_be16(val);
			new_id = srpt_create_rdma_id((struct sockaddr *)&addr4);
			if (IS_ERR(new_id)) {
				ret = PTR_ERR(new_id);
				goto out;
			}
		}
	}

	mutex_lock(&rdma_cm_mutex);
	rdma_cm_port = val;
	swap(rdma_cm_id, new_id);
	mutex_unlock(&rdma_cm_mutex);

	if (new_id)
		rdma_destroy_id(new_id);
	ret = count;
out:
	return ret;
}

CONFIGFS_ATTR(srpt_, rdma_cm_port);

static struct configfs_attribute *srpt_da_attrs[] = {
	&srpt_attr_rdma_cm_port,
	NULL,
};

static int srpt_enable_tpg(struct se_portal_group *se_tpg, bool enable)
{
	struct srpt_port *sport = srpt_tpg_to_sport(se_tpg);

	mutex_lock(&sport->mutex);
	srpt_set_enabled(sport, enable);
	mutex_unlock(&sport->mutex);

	return 0;
}

 
static struct se_portal_group *srpt_make_tpg(struct se_wwn *wwn,
					     const char *name)
{
	struct srpt_port_id *sport_id = srpt_wwn_to_sport_id(wwn);
	struct srpt_tpg *stpg;
	int res = -ENOMEM;

	stpg = kzalloc(sizeof(*stpg), GFP_KERNEL);
	if (!stpg)
		return ERR_PTR(res);
	stpg->sport_id = sport_id;
	res = core_tpg_register(wwn, &stpg->tpg, SCSI_PROTOCOL_SRP);
	if (res) {
		kfree(stpg);
		return ERR_PTR(res);
	}

	mutex_lock(&sport_id->mutex);
	list_add_tail(&stpg->entry, &sport_id->tpg_list);
	mutex_unlock(&sport_id->mutex);

	return &stpg->tpg;
}

 
static void srpt_drop_tpg(struct se_portal_group *tpg)
{
	struct srpt_tpg *stpg = container_of(tpg, typeof(*stpg), tpg);
	struct srpt_port_id *sport_id = stpg->sport_id;
	struct srpt_port *sport = srpt_tpg_to_sport(tpg);

	mutex_lock(&sport_id->mutex);
	list_del(&stpg->entry);
	mutex_unlock(&sport_id->mutex);

	sport->enabled = false;
	core_tpg_deregister(tpg);
	kfree(stpg);
}

 
static struct se_wwn *srpt_make_tport(struct target_fabric_configfs *tf,
				      struct config_group *group,
				      const char *name)
{
	struct port_and_port_id papi = srpt_lookup_port(name);
	struct srpt_port *sport = papi.sport;
	struct srpt_port_id *port_id;

	if (!papi.port_id)
		return ERR_PTR(-EINVAL);
	if (*papi.port_id) {
		 
		WARN_ON_ONCE(true);
		return &(*papi.port_id)->wwn;
	}
	port_id = kzalloc(sizeof(*port_id), GFP_KERNEL);
	if (!port_id) {
		srpt_sdev_put(sport->sdev);
		return ERR_PTR(-ENOMEM);
	}
	mutex_init(&port_id->mutex);
	INIT_LIST_HEAD(&port_id->tpg_list);
	port_id->wwn.priv = sport;
	memcpy(port_id->name, port_id == sport->guid_id ? sport->guid_name :
	       sport->gid_name, ARRAY_SIZE(port_id->name));

	*papi.port_id = port_id;

	return &port_id->wwn;
}

 
static void srpt_drop_tport(struct se_wwn *wwn)
{
	struct srpt_port_id *port_id = container_of(wwn, typeof(*port_id), wwn);
	struct srpt_port *sport = wwn->priv;

	if (sport->guid_id == port_id)
		sport->guid_id = NULL;
	else if (sport->gid_id == port_id)
		sport->gid_id = NULL;
	else
		WARN_ON_ONCE(true);

	srpt_sdev_put(sport->sdev);
	kfree(port_id);
}

static ssize_t srpt_wwn_version_show(struct config_item *item, char *buf)
{
	return sysfs_emit(buf, "\n");
}

CONFIGFS_ATTR_RO(srpt_wwn_, version);

static struct configfs_attribute *srpt_wwn_attrs[] = {
	&srpt_wwn_attr_version,
	NULL,
};

static const struct target_core_fabric_ops srpt_template = {
	.module				= THIS_MODULE,
	.fabric_name			= "srpt",
	.tpg_get_wwn			= srpt_get_fabric_wwn,
	.tpg_get_tag			= srpt_get_tag,
	.tpg_check_demo_mode_cache	= srpt_check_true,
	.tpg_check_demo_mode_write_protect = srpt_check_true,
	.release_cmd			= srpt_release_cmd,
	.check_stop_free		= srpt_check_stop_free,
	.close_session			= srpt_close_session,
	.sess_get_initiator_sid		= NULL,
	.write_pending			= srpt_write_pending,
	.get_cmd_state			= srpt_get_tcm_cmd_state,
	.queue_data_in			= srpt_queue_data_in,
	.queue_status			= srpt_queue_status,
	.queue_tm_rsp			= srpt_queue_tm_rsp,
	.aborted_task			= srpt_aborted_task,
	 
	.fabric_make_wwn		= srpt_make_tport,
	.fabric_drop_wwn		= srpt_drop_tport,
	.fabric_make_tpg		= srpt_make_tpg,
	.fabric_enable_tpg		= srpt_enable_tpg,
	.fabric_drop_tpg		= srpt_drop_tpg,
	.fabric_init_nodeacl		= srpt_init_nodeacl,

	.tfc_discovery_attrs		= srpt_da_attrs,
	.tfc_wwn_attrs			= srpt_wwn_attrs,
	.tfc_tpg_attrib_attrs		= srpt_tpg_attrib_attrs,
};

 
static int __init srpt_init_module(void)
{
	int ret;

	ret = -EINVAL;
	if (srp_max_req_size < MIN_MAX_REQ_SIZE) {
		pr_err("invalid value %d for kernel module parameter srp_max_req_size -- must be at least %d.\n",
		       srp_max_req_size, MIN_MAX_REQ_SIZE);
		goto out;
	}

	if (srpt_srq_size < MIN_SRPT_SRQ_SIZE
	    || srpt_srq_size > MAX_SRPT_SRQ_SIZE) {
		pr_err("invalid value %d for kernel module parameter srpt_srq_size -- must be in the range [%d..%d].\n",
		       srpt_srq_size, MIN_SRPT_SRQ_SIZE, MAX_SRPT_SRQ_SIZE);
		goto out;
	}

	ret = target_register_template(&srpt_template);
	if (ret)
		goto out;

	ret = ib_register_client(&srpt_client);
	if (ret) {
		pr_err("couldn't register IB client\n");
		goto out_unregister_target;
	}

	return 0;

out_unregister_target:
	target_unregister_template(&srpt_template);
out:
	return ret;
}

static void __exit srpt_cleanup_module(void)
{
	if (rdma_cm_id)
		rdma_destroy_id(rdma_cm_id);
	ib_unregister_client(&srpt_client);
	target_unregister_template(&srpt_template);
}

module_init(srpt_init_module);
module_exit(srpt_cleanup_module);
