 

#include <linux/types.h>
#include <linux/list.h>
#include <linux/hardirq.h>
#include <linux/kfifo.h>
#include <linux/blkdev.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/in.h>
#include <linux/net.h>
#include <linux/scatterlist.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>

#include <net/sock.h>

#include <linux/uaccess.h>

#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi.h>
#include <scsi/scsi_transport_iscsi.h>

#include "iscsi_iser.h"

MODULE_DESCRIPTION("iSER (iSCSI Extensions for RDMA) Datamover");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Alex Nezhinsky, Dan Bar Dov, Or Gerlitz");

static const struct scsi_host_template iscsi_iser_sht;
static struct iscsi_transport iscsi_iser_transport;
static struct scsi_transport_template *iscsi_iser_scsi_transport;
static struct workqueue_struct *release_wq;
static DEFINE_MUTEX(unbind_iser_conn_mutex);
struct iser_global ig;

int iser_debug_level = 0;
module_param_named(debug_level, iser_debug_level, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug_level, "Enable debug tracing if > 0 (default:disabled)");

static int iscsi_iser_set(const char *val, const struct kernel_param *kp);
static const struct kernel_param_ops iscsi_iser_size_ops = {
	.set = iscsi_iser_set,
	.get = param_get_uint,
};

static unsigned int iscsi_max_lun = 512;
module_param_cb(max_lun, &iscsi_iser_size_ops, &iscsi_max_lun, S_IRUGO);
MODULE_PARM_DESC(max_lun, "Max LUNs to allow per session, should > 0 (default:512)");

unsigned int iser_max_sectors = ISER_DEF_MAX_SECTORS;
module_param_cb(max_sectors, &iscsi_iser_size_ops, &iser_max_sectors,
		S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(max_sectors, "Max number of sectors in a single scsi command, should > 0 (default:1024)");

bool iser_always_reg = true;
module_param_named(always_register, iser_always_reg, bool, S_IRUGO);
MODULE_PARM_DESC(always_register,
		 "Always register memory, even for continuous memory regions (default:true)");

bool iser_pi_enable = false;
module_param_named(pi_enable, iser_pi_enable, bool, S_IRUGO);
MODULE_PARM_DESC(pi_enable, "Enable T10-PI offload support (default:disabled)");

static int iscsi_iser_set(const char *val, const struct kernel_param *kp)
{
	int ret;
	unsigned int n = 0;

	ret = kstrtouint(val, 10, &n);
	if (ret != 0 || n == 0)
		return -EINVAL;

	return param_set_uint(val, kp);
}

 
void iscsi_iser_recv(struct iscsi_conn *conn, struct iscsi_hdr *hdr,
		     char *rx_data, int rx_data_len)
{
	int rc = 0;
	int datalen;

	 
	datalen = ntoh24(hdr->dlength);
	if (datalen > rx_data_len || (datalen + 4) < rx_data_len) {
		iser_err("wrong datalen %d (hdr), %d (IB)\n",
			datalen, rx_data_len);
		rc = ISCSI_ERR_DATALEN;
		goto error;
	}

	if (datalen != rx_data_len)
		iser_dbg("aligned datalen (%d) hdr, %d (IB)\n",
			datalen, rx_data_len);

	rc = iscsi_complete_pdu(conn, hdr, rx_data, rx_data_len);
	if (rc && rc != ISCSI_ERR_NO_SCSI_CMD)
		goto error;

	return;
error:
	iscsi_conn_failure(conn, rc);
}

 
static int iscsi_iser_pdu_alloc(struct iscsi_task *task, uint8_t opcode)
{
	struct iscsi_iser_task *iser_task = task->dd_data;

	task->hdr = (struct iscsi_hdr *)&iser_task->desc.iscsi_header;
	task->hdr_max = sizeof(iser_task->desc.iscsi_header);

	return 0;
}

 
int iser_initialize_task_headers(struct iscsi_task *task,
				 struct iser_tx_desc *tx_desc)
{
	struct iser_conn *iser_conn = task->conn->dd_data;
	struct iser_device *device = iser_conn->ib_conn.device;
	struct iscsi_iser_task *iser_task = task->dd_data;
	u64 dma_addr;

	if (unlikely(iser_conn->state != ISER_CONN_UP))
		return -ENODEV;

	dma_addr = ib_dma_map_single(device->ib_device, (void *)tx_desc,
				ISER_HEADERS_LEN, DMA_TO_DEVICE);
	if (ib_dma_mapping_error(device->ib_device, dma_addr))
		return -ENOMEM;

	tx_desc->inv_wr.next = NULL;
	tx_desc->reg_wr.wr.next = NULL;
	tx_desc->mapped = true;
	tx_desc->dma_addr = dma_addr;
	tx_desc->tx_sg[0].addr   = tx_desc->dma_addr;
	tx_desc->tx_sg[0].length = ISER_HEADERS_LEN;
	tx_desc->tx_sg[0].lkey   = device->pd->local_dma_lkey;

	iser_task->iser_conn = iser_conn;

	return 0;
}

 
static int iscsi_iser_task_init(struct iscsi_task *task)
{
	struct iscsi_iser_task *iser_task = task->dd_data;
	int ret;

	ret = iser_initialize_task_headers(task, &iser_task->desc);
	if (ret) {
		iser_err("Failed to init task %p, err = %d\n",
			 iser_task, ret);
		return ret;
	}

	 
	if (!task->sc)
		return 0;

	iser_task->command_sent = 0;
	iser_task_rdma_init(iser_task);
	iser_task->sc = task->sc;

	return 0;
}

 
static int iscsi_iser_mtask_xmit(struct iscsi_conn *conn,
				 struct iscsi_task *task)
{
	int error = 0;

	iser_dbg("mtask xmit [cid %d itt 0x%x]\n", conn->id, task->itt);

	error = iser_send_control(conn, task);

	 
	return error;
}

static int iscsi_iser_task_xmit_unsol_data(struct iscsi_conn *conn,
					   struct iscsi_task *task)
{
	struct iscsi_r2t_info *r2t = &task->unsol_r2t;
	struct iscsi_data hdr;
	int error = 0;

	 
	while (iscsi_task_has_unsol_data(task)) {
		iscsi_prep_data_out_pdu(task, r2t, &hdr);
		iser_dbg("Sending data-out: itt 0x%x, data count %d\n",
			   hdr.itt, r2t->data_count);

		 
		 
		error = iser_send_data_out(conn, task, &hdr);
		if (error) {
			r2t->datasn--;
			goto iscsi_iser_task_xmit_unsol_data_exit;
		}
		r2t->sent += r2t->data_count;
		iser_dbg("Need to send %d more as data-out PDUs\n",
			   r2t->data_length - r2t->sent);
	}

iscsi_iser_task_xmit_unsol_data_exit:
	return error;
}

 
static int iscsi_iser_task_xmit(struct iscsi_task *task)
{
	struct iscsi_conn *conn = task->conn;
	struct iscsi_iser_task *iser_task = task->dd_data;
	int error = 0;

	if (!task->sc)
		return iscsi_iser_mtask_xmit(conn, task);

	if (task->sc->sc_data_direction == DMA_TO_DEVICE) {
		BUG_ON(scsi_bufflen(task->sc) == 0);

		iser_dbg("cmd [itt %x total %d imm %d unsol_data %d\n",
			   task->itt, scsi_bufflen(task->sc),
			   task->imm_count, task->unsol_r2t.data_length);
	}

	iser_dbg("ctask xmit [cid %d itt 0x%x]\n",
		   conn->id, task->itt);

	 
	if (!iser_task->command_sent) {
		error = iser_send_command(conn, task);
		if (error)
			goto iscsi_iser_task_xmit_exit;
		iser_task->command_sent = 1;
	}

	 
	if (iscsi_task_has_unsol_data(task))
		error = iscsi_iser_task_xmit_unsol_data(conn, task);

 iscsi_iser_task_xmit_exit:
	return error;
}

 
static void iscsi_iser_cleanup_task(struct iscsi_task *task)
{
	struct iscsi_iser_task *iser_task = task->dd_data;
	struct iser_tx_desc *tx_desc = &iser_task->desc;
	struct iser_conn *iser_conn = task->conn->dd_data;
	struct iser_device *device = iser_conn->ib_conn.device;

	 
	if (!device)
		return;

	if (likely(tx_desc->mapped)) {
		ib_dma_unmap_single(device->ib_device, tx_desc->dma_addr,
				    ISER_HEADERS_LEN, DMA_TO_DEVICE);
		tx_desc->mapped = false;
	}

	 
	if (!task->sc)
		return;

	if (iser_task->status == ISER_TASK_STATUS_STARTED) {
		iser_task->status = ISER_TASK_STATUS_COMPLETED;
		iser_task_rdma_finalize(iser_task);
	}
}

 
static u8 iscsi_iser_check_protection(struct iscsi_task *task, sector_t *sector)
{
	struct iscsi_iser_task *iser_task = task->dd_data;
	enum iser_data_dir dir = iser_task->dir[ISER_DIR_IN] ?
					ISER_DIR_IN : ISER_DIR_OUT;

	return iser_check_task_pi_status(iser_task, dir, sector);
}

 
static struct iscsi_cls_conn *
iscsi_iser_conn_create(struct iscsi_cls_session *cls_session,
		       uint32_t conn_idx)
{
	struct iscsi_conn *conn;
	struct iscsi_cls_conn *cls_conn;

	cls_conn = iscsi_conn_setup(cls_session, 0, conn_idx);
	if (!cls_conn)
		return NULL;
	conn = cls_conn->dd_data;

	 
	conn->max_recv_dlength = ISER_RECV_DATA_SEG_LEN;

	return cls_conn;
}

 
static int iscsi_iser_conn_bind(struct iscsi_cls_session *cls_session,
				struct iscsi_cls_conn *cls_conn,
				uint64_t transport_eph, int is_leading)
{
	struct iscsi_conn *conn = cls_conn->dd_data;
	struct iser_conn *iser_conn;
	struct iscsi_endpoint *ep;
	int error;

	error = iscsi_conn_bind(cls_session, cls_conn, is_leading);
	if (error)
		return error;

	 
	ep = iscsi_lookup_endpoint(transport_eph);
	if (!ep) {
		iser_err("can't bind eph %llx\n",
			 (unsigned long long)transport_eph);
		return -EINVAL;
	}
	iser_conn = ep->dd_data;

	mutex_lock(&iser_conn->state_mutex);
	if (iser_conn->state != ISER_CONN_UP) {
		error = -EINVAL;
		iser_err("iser_conn %p state is %d, teardown started\n",
			 iser_conn, iser_conn->state);
		goto out;
	}

	error = iser_alloc_rx_descriptors(iser_conn, conn->session);
	if (error)
		goto out;

	 
	iser_info("binding iscsi conn %p to iser_conn %p\n", conn, iser_conn);

	conn->dd_data = iser_conn;
	iser_conn->iscsi_conn = conn;

out:
	iscsi_put_endpoint(ep);
	mutex_unlock(&iser_conn->state_mutex);
	return error;
}

 
static int iscsi_iser_conn_start(struct iscsi_cls_conn *cls_conn)
{
	struct iscsi_conn *iscsi_conn;
	struct iser_conn *iser_conn;

	iscsi_conn = cls_conn->dd_data;
	iser_conn = iscsi_conn->dd_data;
	reinit_completion(&iser_conn->stop_completion);

	return iscsi_conn_start(cls_conn);
}

 
static void iscsi_iser_conn_stop(struct iscsi_cls_conn *cls_conn, int flag)
{
	struct iscsi_conn *conn = cls_conn->dd_data;
	struct iser_conn *iser_conn = conn->dd_data;

	iser_info("stopping iscsi_conn: %p, iser_conn: %p\n", conn, iser_conn);

	 
	if (iser_conn) {
		mutex_lock(&iser_conn->state_mutex);
		mutex_lock(&unbind_iser_conn_mutex);
		iser_conn_terminate(iser_conn);
		iscsi_conn_stop(cls_conn, flag);

		 
		iser_conn->iscsi_conn = NULL;
		conn->dd_data = NULL;
		mutex_unlock(&unbind_iser_conn_mutex);

		complete(&iser_conn->stop_completion);
		mutex_unlock(&iser_conn->state_mutex);
	} else {
		iscsi_conn_stop(cls_conn, flag);
	}
}

 
static void iscsi_iser_session_destroy(struct iscsi_cls_session *cls_session)
{
	struct Scsi_Host *shost = iscsi_session_to_shost(cls_session);

	iscsi_session_teardown(cls_session);
	iscsi_host_remove(shost, false);
	iscsi_host_free(shost);
}

static inline unsigned int iser_dif_prot_caps(int prot_caps)
{
	int ret = 0;

	if (prot_caps & IB_PROT_T10DIF_TYPE_1)
		ret |= SHOST_DIF_TYPE1_PROTECTION |
		       SHOST_DIX_TYPE0_PROTECTION |
		       SHOST_DIX_TYPE1_PROTECTION;
	if (prot_caps & IB_PROT_T10DIF_TYPE_2)
		ret |= SHOST_DIF_TYPE2_PROTECTION |
		       SHOST_DIX_TYPE2_PROTECTION;
	if (prot_caps & IB_PROT_T10DIF_TYPE_3)
		ret |= SHOST_DIF_TYPE3_PROTECTION |
		       SHOST_DIX_TYPE3_PROTECTION;

	return ret;
}

 
static struct iscsi_cls_session *
iscsi_iser_session_create(struct iscsi_endpoint *ep,
			  uint16_t cmds_max, uint16_t qdepth,
			  uint32_t initial_cmdsn)
{
	struct iscsi_cls_session *cls_session;
	struct Scsi_Host *shost;
	struct iser_conn *iser_conn = NULL;
	struct ib_conn *ib_conn;
	struct ib_device *ib_dev;
	u32 max_fr_sectors;

	shost = iscsi_host_alloc(&iscsi_iser_sht, 0, 0);
	if (!shost)
		return NULL;
	shost->transportt = iscsi_iser_scsi_transport;
	shost->cmd_per_lun = qdepth;
	shost->max_lun = iscsi_max_lun;
	shost->max_id = 0;
	shost->max_channel = 0;
	shost->max_cmd_len = 16;

	 
	if (ep) {
		iser_conn = ep->dd_data;
		shost->sg_tablesize = iser_conn->scsi_sg_tablesize;
		shost->can_queue = min_t(u16, cmds_max, iser_conn->max_cmds);

		mutex_lock(&iser_conn->state_mutex);
		if (iser_conn->state != ISER_CONN_UP) {
			iser_err("iser conn %p already started teardown\n",
				 iser_conn);
			mutex_unlock(&iser_conn->state_mutex);
			goto free_host;
		}

		ib_conn = &iser_conn->ib_conn;
		ib_dev = ib_conn->device->ib_device;
		if (ib_conn->pi_support) {
			u32 sig_caps = ib_dev->attrs.sig_prot_cap;

			shost->sg_prot_tablesize = shost->sg_tablesize;
			scsi_host_set_prot(shost, iser_dif_prot_caps(sig_caps));
			scsi_host_set_guard(shost, SHOST_DIX_GUARD_IP |
						   SHOST_DIX_GUARD_CRC);
		}

		if (!(ib_dev->attrs.kernel_cap_flags & IBK_SG_GAPS_REG))
			shost->virt_boundary_mask = SZ_4K - 1;

		if (iscsi_host_add(shost, ib_dev->dev.parent)) {
			mutex_unlock(&iser_conn->state_mutex);
			goto free_host;
		}
		mutex_unlock(&iser_conn->state_mutex);
	} else {
		shost->can_queue = min_t(u16, cmds_max, ISER_DEF_XMIT_CMDS_MAX);
		if (iscsi_host_add(shost, NULL))
			goto free_host;
	}

	max_fr_sectors = (shost->sg_tablesize * PAGE_SIZE) >> 9;
	shost->max_sectors = min(iser_max_sectors, max_fr_sectors);

	iser_dbg("iser_conn %p, sg_tablesize %u, max_sectors %u\n",
		 iser_conn, shost->sg_tablesize,
		 shost->max_sectors);

	if (shost->max_sectors < iser_max_sectors)
		iser_warn("max_sectors was reduced from %u to %u\n",
			  iser_max_sectors, shost->max_sectors);

	cls_session = iscsi_session_setup(&iscsi_iser_transport, shost,
					  shost->can_queue, 0,
					  sizeof(struct iscsi_iser_task),
					  initial_cmdsn, 0);
	if (!cls_session)
		goto remove_host;

	return cls_session;

remove_host:
	iscsi_host_remove(shost, false);
free_host:
	iscsi_host_free(shost);
	return NULL;
}

static int iscsi_iser_set_param(struct iscsi_cls_conn *cls_conn,
				enum iscsi_param param, char *buf, int buflen)
{
	int value;

	switch (param) {
	case ISCSI_PARAM_MAX_RECV_DLENGTH:
		 
		break;
	case ISCSI_PARAM_HDRDGST_EN:
		sscanf(buf, "%d", &value);
		if (value) {
			iser_err("DataDigest wasn't negotiated to None\n");
			return -EPROTO;
		}
		break;
	case ISCSI_PARAM_DATADGST_EN:
		sscanf(buf, "%d", &value);
		if (value) {
			iser_err("DataDigest wasn't negotiated to None\n");
			return -EPROTO;
		}
		break;
	case ISCSI_PARAM_IFMARKER_EN:
		sscanf(buf, "%d", &value);
		if (value) {
			iser_err("IFMarker wasn't negotiated to No\n");
			return -EPROTO;
		}
		break;
	case ISCSI_PARAM_OFMARKER_EN:
		sscanf(buf, "%d", &value);
		if (value) {
			iser_err("OFMarker wasn't negotiated to No\n");
			return -EPROTO;
		}
		break;
	default:
		return iscsi_set_param(cls_conn, param, buf, buflen);
	}

	return 0;
}

 
static void iscsi_iser_conn_get_stats(struct iscsi_cls_conn *cls_conn,
				      struct iscsi_stats *stats)
{
	struct iscsi_conn *conn = cls_conn->dd_data;

	stats->txdata_octets = conn->txdata_octets;
	stats->rxdata_octets = conn->rxdata_octets;
	stats->scsicmd_pdus = conn->scsicmd_pdus_cnt;
	stats->dataout_pdus = conn->dataout_pdus_cnt;
	stats->scsirsp_pdus = conn->scsirsp_pdus_cnt;
	stats->datain_pdus = conn->datain_pdus_cnt;  
	stats->r2t_pdus = conn->r2t_pdus_cnt;  
	stats->tmfcmd_pdus = conn->tmfcmd_pdus_cnt;
	stats->tmfrsp_pdus = conn->tmfrsp_pdus_cnt;
	stats->custom_length = 0;
}

static int iscsi_iser_get_ep_param(struct iscsi_endpoint *ep,
				   enum iscsi_param param, char *buf)
{
	struct iser_conn *iser_conn = ep->dd_data;

	switch (param) {
	case ISCSI_PARAM_CONN_PORT:
	case ISCSI_PARAM_CONN_ADDRESS:
		if (!iser_conn || !iser_conn->ib_conn.cma_id)
			return -ENOTCONN;

		return iscsi_conn_get_addr_param((struct sockaddr_storage *)
				&iser_conn->ib_conn.cma_id->route.addr.dst_addr,
				param, buf);
	default:
		break;
	}
	return -ENOSYS;
}

 
static struct iscsi_endpoint *iscsi_iser_ep_connect(struct Scsi_Host *shost,
						    struct sockaddr *dst_addr,
						    int non_blocking)
{
	int err;
	struct iser_conn *iser_conn;
	struct iscsi_endpoint *ep;

	ep = iscsi_create_endpoint(0);
	if (!ep)
		return ERR_PTR(-ENOMEM);

	iser_conn = kzalloc(sizeof(*iser_conn), GFP_KERNEL);
	if (!iser_conn) {
		err = -ENOMEM;
		goto failure;
	}

	ep->dd_data = iser_conn;
	iser_conn->ep = ep;
	iser_conn_init(iser_conn);

	err = iser_connect(iser_conn, NULL, dst_addr, non_blocking);
	if (err)
		goto failure;

	return ep;
failure:
	iscsi_destroy_endpoint(ep);
	return ERR_PTR(err);
}

 
static int iscsi_iser_ep_poll(struct iscsi_endpoint *ep, int timeout_ms)
{
	struct iser_conn *iser_conn = ep->dd_data;
	int rc;

	rc = wait_for_completion_interruptible_timeout(&iser_conn->up_completion,
						       msecs_to_jiffies(timeout_ms));
	 
	if (rc == 0) {
		mutex_lock(&iser_conn->state_mutex);
		if (iser_conn->state == ISER_CONN_TERMINATING ||
		    iser_conn->state == ISER_CONN_DOWN)
			rc = -1;
		mutex_unlock(&iser_conn->state_mutex);
	}

	iser_info("iser conn %p rc = %d\n", iser_conn, rc);

	if (rc > 0)
		return 1;  
	else if (!rc)
		return 0;  
	else
		return rc;  
}

 
static void iscsi_iser_ep_disconnect(struct iscsi_endpoint *ep)
{
	struct iser_conn *iser_conn = ep->dd_data;

	iser_info("ep %p iser conn %p\n", ep, iser_conn);

	mutex_lock(&iser_conn->state_mutex);
	iser_conn_terminate(iser_conn);

	 
	if (iser_conn->iscsi_conn) {
		INIT_WORK(&iser_conn->release_work, iser_release_work);
		queue_work(release_wq, &iser_conn->release_work);
		mutex_unlock(&iser_conn->state_mutex);
	} else {
		iser_conn->state = ISER_CONN_DOWN;
		mutex_unlock(&iser_conn->state_mutex);
		iser_conn_release(iser_conn);
	}

	iscsi_destroy_endpoint(ep);
}

static umode_t iser_attr_is_visible(int param_type, int param)
{
	switch (param_type) {
	case ISCSI_HOST_PARAM:
		switch (param) {
		case ISCSI_HOST_PARAM_NETDEV_NAME:
		case ISCSI_HOST_PARAM_HWADDRESS:
		case ISCSI_HOST_PARAM_INITIATOR_NAME:
			return S_IRUGO;
		default:
			return 0;
		}
	case ISCSI_PARAM:
		switch (param) {
		case ISCSI_PARAM_MAX_RECV_DLENGTH:
		case ISCSI_PARAM_MAX_XMIT_DLENGTH:
		case ISCSI_PARAM_HDRDGST_EN:
		case ISCSI_PARAM_DATADGST_EN:
		case ISCSI_PARAM_CONN_ADDRESS:
		case ISCSI_PARAM_CONN_PORT:
		case ISCSI_PARAM_EXP_STATSN:
		case ISCSI_PARAM_PERSISTENT_ADDRESS:
		case ISCSI_PARAM_PERSISTENT_PORT:
		case ISCSI_PARAM_PING_TMO:
		case ISCSI_PARAM_RECV_TMO:
		case ISCSI_PARAM_INITIAL_R2T_EN:
		case ISCSI_PARAM_MAX_R2T:
		case ISCSI_PARAM_IMM_DATA_EN:
		case ISCSI_PARAM_FIRST_BURST:
		case ISCSI_PARAM_MAX_BURST:
		case ISCSI_PARAM_PDU_INORDER_EN:
		case ISCSI_PARAM_DATASEQ_INORDER_EN:
		case ISCSI_PARAM_TARGET_NAME:
		case ISCSI_PARAM_TPGT:
		case ISCSI_PARAM_USERNAME:
		case ISCSI_PARAM_PASSWORD:
		case ISCSI_PARAM_USERNAME_IN:
		case ISCSI_PARAM_PASSWORD_IN:
		case ISCSI_PARAM_FAST_ABORT:
		case ISCSI_PARAM_ABORT_TMO:
		case ISCSI_PARAM_LU_RESET_TMO:
		case ISCSI_PARAM_TGT_RESET_TMO:
		case ISCSI_PARAM_IFACE_NAME:
		case ISCSI_PARAM_INITIATOR_NAME:
		case ISCSI_PARAM_DISCOVERY_SESS:
			return S_IRUGO;
		default:
			return 0;
		}
	}

	return 0;
}

static const struct scsi_host_template iscsi_iser_sht = {
	.module                 = THIS_MODULE,
	.name                   = "iSCSI Initiator over iSER",
	.queuecommand           = iscsi_queuecommand,
	.change_queue_depth	= scsi_change_queue_depth,
	.sg_tablesize           = ISCSI_ISER_DEF_SG_TABLESIZE,
	.cmd_per_lun            = ISER_DEF_CMD_PER_LUN,
	.eh_timed_out		= iscsi_eh_cmd_timed_out,
	.eh_abort_handler       = iscsi_eh_abort,
	.eh_device_reset_handler= iscsi_eh_device_reset,
	.eh_target_reset_handler = iscsi_eh_recover_target,
	.target_alloc		= iscsi_target_alloc,
	.proc_name              = "iscsi_iser",
	.this_id                = -1,
	.track_queue_depth	= 1,
	.cmd_size		= sizeof(struct iscsi_cmd),
};

static struct iscsi_transport iscsi_iser_transport = {
	.owner                  = THIS_MODULE,
	.name                   = "iser",
	.caps                   = CAP_RECOVERY_L0 | CAP_MULTI_R2T | CAP_TEXT_NEGO,
	 
	.create_session         = iscsi_iser_session_create,
	.destroy_session        = iscsi_iser_session_destroy,
	 
	.create_conn            = iscsi_iser_conn_create,
	.bind_conn              = iscsi_iser_conn_bind,
	.unbind_conn		= iscsi_conn_unbind,
	.destroy_conn           = iscsi_conn_teardown,
	.attr_is_visible	= iser_attr_is_visible,
	.set_param              = iscsi_iser_set_param,
	.get_conn_param		= iscsi_conn_get_param,
	.get_ep_param		= iscsi_iser_get_ep_param,
	.get_session_param	= iscsi_session_get_param,
	.start_conn             = iscsi_iser_conn_start,
	.stop_conn              = iscsi_iser_conn_stop,
	 
	.get_host_param		= iscsi_host_get_param,
	.set_host_param		= iscsi_host_set_param,
	 
	.send_pdu		= iscsi_conn_send_pdu,
	.get_stats		= iscsi_iser_conn_get_stats,
	.init_task		= iscsi_iser_task_init,
	.xmit_task		= iscsi_iser_task_xmit,
	.cleanup_task		= iscsi_iser_cleanup_task,
	.alloc_pdu		= iscsi_iser_pdu_alloc,
	.check_protection	= iscsi_iser_check_protection,
	 
	.session_recovery_timedout = iscsi_session_recovery_timedout,

	.ep_connect             = iscsi_iser_ep_connect,
	.ep_poll                = iscsi_iser_ep_poll,
	.ep_disconnect          = iscsi_iser_ep_disconnect
};

static int __init iser_init(void)
{
	int err;

	iser_dbg("Starting iSER datamover...\n");

	memset(&ig, 0, sizeof(struct iser_global));

	ig.desc_cache = kmem_cache_create("iser_descriptors",
					  sizeof(struct iser_tx_desc),
					  0, SLAB_HWCACHE_ALIGN,
					  NULL);
	if (ig.desc_cache == NULL)
		return -ENOMEM;

	 
	mutex_init(&ig.device_list_mutex);
	INIT_LIST_HEAD(&ig.device_list);
	mutex_init(&ig.connlist_mutex);
	INIT_LIST_HEAD(&ig.connlist);

	release_wq = alloc_workqueue("release workqueue", 0, 0);
	if (!release_wq) {
		iser_err("failed to allocate release workqueue\n");
		err = -ENOMEM;
		goto err_alloc_wq;
	}

	iscsi_iser_scsi_transport = iscsi_register_transport(
							&iscsi_iser_transport);
	if (!iscsi_iser_scsi_transport) {
		iser_err("iscsi_register_transport failed\n");
		err = -EINVAL;
		goto err_reg;
	}

	return 0;

err_reg:
	destroy_workqueue(release_wq);
err_alloc_wq:
	kmem_cache_destroy(ig.desc_cache);

	return err;
}

static void __exit iser_exit(void)
{
	struct iser_conn *iser_conn, *n;
	int connlist_empty;

	iser_dbg("Removing iSER datamover...\n");
	destroy_workqueue(release_wq);

	mutex_lock(&ig.connlist_mutex);
	connlist_empty = list_empty(&ig.connlist);
	mutex_unlock(&ig.connlist_mutex);

	if (!connlist_empty) {
		iser_err("Error cleanup stage completed but we still have iser "
			 "connections, destroying them anyway\n");
		list_for_each_entry_safe(iser_conn, n, &ig.connlist,
					 conn_list) {
			iser_conn_release(iser_conn);
		}
	}

	iscsi_unregister_transport(&iscsi_iser_transport);
	kmem_cache_destroy(ig.desc_cache);
}

module_init(iser_init);
module_exit(iser_exit);
