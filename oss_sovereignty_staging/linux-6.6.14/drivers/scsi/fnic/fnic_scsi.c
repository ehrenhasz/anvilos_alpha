
 
#include <linux/mempool.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/pci.h>
#include <linux/scatterlist.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/delay.h>
#include <linux/gfp.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_tcq.h>
#include <scsi/fc/fc_els.h>
#include <scsi/fc/fc_fcoe.h>
#include <scsi/libfc.h>
#include <scsi/fc_frame.h>
#include "fnic_io.h"
#include "fnic.h"

const char *fnic_state_str[] = {
	[FNIC_IN_FC_MODE] =           "FNIC_IN_FC_MODE",
	[FNIC_IN_FC_TRANS_ETH_MODE] = "FNIC_IN_FC_TRANS_ETH_MODE",
	[FNIC_IN_ETH_MODE] =          "FNIC_IN_ETH_MODE",
	[FNIC_IN_ETH_TRANS_FC_MODE] = "FNIC_IN_ETH_TRANS_FC_MODE",
};

static const char *fnic_ioreq_state_str[] = {
	[FNIC_IOREQ_NOT_INITED] = "FNIC_IOREQ_NOT_INITED",
	[FNIC_IOREQ_CMD_PENDING] = "FNIC_IOREQ_CMD_PENDING",
	[FNIC_IOREQ_ABTS_PENDING] = "FNIC_IOREQ_ABTS_PENDING",
	[FNIC_IOREQ_ABTS_COMPLETE] = "FNIC_IOREQ_ABTS_COMPLETE",
	[FNIC_IOREQ_CMD_COMPLETE] = "FNIC_IOREQ_CMD_COMPLETE",
};

static const char *fcpio_status_str[] =  {
	[FCPIO_SUCCESS] = "FCPIO_SUCCESS",  
	[FCPIO_INVALID_HEADER] = "FCPIO_INVALID_HEADER",
	[FCPIO_OUT_OF_RESOURCE] = "FCPIO_OUT_OF_RESOURCE",
	[FCPIO_INVALID_PARAM] = "FCPIO_INVALID_PARAM]",
	[FCPIO_REQ_NOT_SUPPORTED] = "FCPIO_REQ_NOT_SUPPORTED",
	[FCPIO_IO_NOT_FOUND] = "FCPIO_IO_NOT_FOUND",
	[FCPIO_ABORTED] = "FCPIO_ABORTED",  
	[FCPIO_TIMEOUT] = "FCPIO_TIMEOUT",
	[FCPIO_SGL_INVALID] = "FCPIO_SGL_INVALID",
	[FCPIO_MSS_INVALID] = "FCPIO_MSS_INVALID",
	[FCPIO_DATA_CNT_MISMATCH] = "FCPIO_DATA_CNT_MISMATCH",
	[FCPIO_FW_ERR] = "FCPIO_FW_ERR",
	[FCPIO_ITMF_REJECTED] = "FCPIO_ITMF_REJECTED",
	[FCPIO_ITMF_FAILED] = "FCPIO_ITMF_FAILED",
	[FCPIO_ITMF_INCORRECT_LUN] = "FCPIO_ITMF_INCORRECT_LUN",
	[FCPIO_CMND_REJECTED] = "FCPIO_CMND_REJECTED",
	[FCPIO_NO_PATH_AVAIL] = "FCPIO_NO_PATH_AVAIL",
	[FCPIO_PATH_FAILED] = "FCPIO_PATH_FAILED",
	[FCPIO_LUNMAP_CHNG_PEND] = "FCPIO_LUNHMAP_CHNG_PEND",
};

const char *fnic_state_to_str(unsigned int state)
{
	if (state >= ARRAY_SIZE(fnic_state_str) || !fnic_state_str[state])
		return "unknown";

	return fnic_state_str[state];
}

static const char *fnic_ioreq_state_to_str(unsigned int state)
{
	if (state >= ARRAY_SIZE(fnic_ioreq_state_str) ||
	    !fnic_ioreq_state_str[state])
		return "unknown";

	return fnic_ioreq_state_str[state];
}

static const char *fnic_fcpio_status_to_str(unsigned int status)
{
	if (status >= ARRAY_SIZE(fcpio_status_str) || !fcpio_status_str[status])
		return "unknown";

	return fcpio_status_str[status];
}

static void fnic_cleanup_io(struct fnic *fnic);

static inline spinlock_t *fnic_io_lock_hash(struct fnic *fnic,
					    struct scsi_cmnd *sc)
{
	u32 hash = scsi_cmd_to_rq(sc)->tag & (FNIC_IO_LOCKS - 1);

	return &fnic->io_req_lock[hash];
}

static inline spinlock_t *fnic_io_lock_tag(struct fnic *fnic,
					    int tag)
{
	return &fnic->io_req_lock[tag & (FNIC_IO_LOCKS - 1)];
}

 
static void fnic_release_ioreq_buf(struct fnic *fnic,
				   struct fnic_io_req *io_req,
				   struct scsi_cmnd *sc)
{
	if (io_req->sgl_list_pa)
		dma_unmap_single(&fnic->pdev->dev, io_req->sgl_list_pa,
				 sizeof(io_req->sgl_list[0]) * io_req->sgl_cnt,
				 DMA_TO_DEVICE);
	scsi_dma_unmap(sc);

	if (io_req->sgl_cnt)
		mempool_free(io_req->sgl_list_alloc,
			     fnic->io_sgl_pool[io_req->sgl_type]);
	if (io_req->sense_buf_pa)
		dma_unmap_single(&fnic->pdev->dev, io_req->sense_buf_pa,
				 SCSI_SENSE_BUFFERSIZE, DMA_FROM_DEVICE);
}

 
static int free_wq_copy_descs(struct fnic *fnic, struct vnic_wq_copy *wq)
{
	 
	if (!fnic->fw_ack_recd[0])
		return 1;

	 
	if (wq->to_clean_index <= fnic->fw_ack_index[0])
		wq->ring.desc_avail += (fnic->fw_ack_index[0]
					- wq->to_clean_index + 1);
	else
		wq->ring.desc_avail += (wq->ring.desc_count
					- wq->to_clean_index
					+ fnic->fw_ack_index[0] + 1);

	 
	wq->to_clean_index =
		(fnic->fw_ack_index[0] + 1) % wq->ring.desc_count;

	 
	fnic->fw_ack_recd[0] = 0;
	return 0;
}


 
void
__fnic_set_state_flags(struct fnic *fnic, unsigned long st_flags,
			unsigned long clearbits)
{
	unsigned long flags = 0;
	unsigned long host_lock_flags = 0;

	spin_lock_irqsave(&fnic->fnic_lock, flags);
	spin_lock_irqsave(fnic->lport->host->host_lock, host_lock_flags);

	if (clearbits)
		fnic->state_flags &= ~st_flags;
	else
		fnic->state_flags |= st_flags;

	spin_unlock_irqrestore(fnic->lport->host->host_lock, host_lock_flags);
	spin_unlock_irqrestore(&fnic->fnic_lock, flags);

	return;
}


 
int fnic_fw_reset_handler(struct fnic *fnic)
{
	struct vnic_wq_copy *wq = &fnic->wq_copy[0];
	int ret = 0;
	unsigned long flags;

	 
	fnic_set_state_flags(fnic, FNIC_FLAGS_FWRESET);

	skb_queue_purge(&fnic->frame_queue);
	skb_queue_purge(&fnic->tx_queue);

	 
	while (atomic_read(&fnic->in_flight))
		schedule_timeout(msecs_to_jiffies(1));

	spin_lock_irqsave(&fnic->wq_copy_lock[0], flags);

	if (vnic_wq_copy_desc_avail(wq) <= fnic->wq_copy_desc_low[0])
		free_wq_copy_descs(fnic, wq);

	if (!vnic_wq_copy_desc_avail(wq))
		ret = -EAGAIN;
	else {
		fnic_queue_wq_copy_desc_fw_reset(wq, SCSI_NO_TAG);
		atomic64_inc(&fnic->fnic_stats.fw_stats.active_fw_reqs);
		if (atomic64_read(&fnic->fnic_stats.fw_stats.active_fw_reqs) >
			  atomic64_read(&fnic->fnic_stats.fw_stats.max_fw_reqs))
			atomic64_set(&fnic->fnic_stats.fw_stats.max_fw_reqs,
				atomic64_read(
				  &fnic->fnic_stats.fw_stats.active_fw_reqs));
	}

	spin_unlock_irqrestore(&fnic->wq_copy_lock[0], flags);

	if (!ret) {
		atomic64_inc(&fnic->fnic_stats.reset_stats.fw_resets);
		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
			      "Issued fw reset\n");
	} else {
		fnic_clear_state_flags(fnic, FNIC_FLAGS_FWRESET);
		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
			      "Failed to issue fw reset\n");
	}

	return ret;
}


 
int fnic_flogi_reg_handler(struct fnic *fnic, u32 fc_id)
{
	struct vnic_wq_copy *wq = &fnic->wq_copy[0];
	enum fcpio_flogi_reg_format_type format;
	struct fc_lport *lp = fnic->lport;
	u8 gw_mac[ETH_ALEN];
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&fnic->wq_copy_lock[0], flags);

	if (vnic_wq_copy_desc_avail(wq) <= fnic->wq_copy_desc_low[0])
		free_wq_copy_descs(fnic, wq);

	if (!vnic_wq_copy_desc_avail(wq)) {
		ret = -EAGAIN;
		goto flogi_reg_ioreq_end;
	}

	if (fnic->ctlr.map_dest) {
		eth_broadcast_addr(gw_mac);
		format = FCPIO_FLOGI_REG_DEF_DEST;
	} else {
		memcpy(gw_mac, fnic->ctlr.dest_addr, ETH_ALEN);
		format = FCPIO_FLOGI_REG_GW_DEST;
	}

	if ((fnic->config.flags & VFCF_FIP_CAPABLE) && !fnic->ctlr.map_dest) {
		fnic_queue_wq_copy_desc_fip_reg(wq, SCSI_NO_TAG,
						fc_id, gw_mac,
						fnic->data_src_addr,
						lp->r_a_tov, lp->e_d_tov);
		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
			      "FLOGI FIP reg issued fcid %x src %pM dest %pM\n",
			      fc_id, fnic->data_src_addr, gw_mac);
	} else {
		fnic_queue_wq_copy_desc_flogi_reg(wq, SCSI_NO_TAG,
						  format, fc_id, gw_mac);
		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
			      "FLOGI reg issued fcid %x map %d dest %pM\n",
			      fc_id, fnic->ctlr.map_dest, gw_mac);
	}

	atomic64_inc(&fnic->fnic_stats.fw_stats.active_fw_reqs);
	if (atomic64_read(&fnic->fnic_stats.fw_stats.active_fw_reqs) >
		  atomic64_read(&fnic->fnic_stats.fw_stats.max_fw_reqs))
		atomic64_set(&fnic->fnic_stats.fw_stats.max_fw_reqs,
		  atomic64_read(&fnic->fnic_stats.fw_stats.active_fw_reqs));

flogi_reg_ioreq_end:
	spin_unlock_irqrestore(&fnic->wq_copy_lock[0], flags);
	return ret;
}

 
static inline int fnic_queue_wq_copy_desc(struct fnic *fnic,
					  struct vnic_wq_copy *wq,
					  struct fnic_io_req *io_req,
					  struct scsi_cmnd *sc,
					  int sg_count)
{
	struct scatterlist *sg;
	struct fc_rport *rport = starget_to_rport(scsi_target(sc->device));
	struct fc_rport_libfc_priv *rp = rport->dd_data;
	struct host_sg_desc *desc;
	struct misc_stats *misc_stats = &fnic->fnic_stats.misc_stats;
	unsigned int i;
	unsigned long intr_flags;
	int flags;
	u8 exch_flags;
	struct scsi_lun fc_lun;

	if (sg_count) {
		 
		desc = io_req->sgl_list;
		for_each_sg(scsi_sglist(sc), sg, sg_count, i) {
			desc->addr = cpu_to_le64(sg_dma_address(sg));
			desc->len = cpu_to_le32(sg_dma_len(sg));
			desc->_resvd = 0;
			desc++;
		}

		io_req->sgl_list_pa = dma_map_single(&fnic->pdev->dev,
				io_req->sgl_list,
				sizeof(io_req->sgl_list[0]) * sg_count,
				DMA_TO_DEVICE);
		if (dma_mapping_error(&fnic->pdev->dev, io_req->sgl_list_pa)) {
			printk(KERN_ERR "DMA mapping failed\n");
			return SCSI_MLQUEUE_HOST_BUSY;
		}
	}

	io_req->sense_buf_pa = dma_map_single(&fnic->pdev->dev,
					      sc->sense_buffer,
					      SCSI_SENSE_BUFFERSIZE,
					      DMA_FROM_DEVICE);
	if (dma_mapping_error(&fnic->pdev->dev, io_req->sense_buf_pa)) {
		dma_unmap_single(&fnic->pdev->dev, io_req->sgl_list_pa,
				sizeof(io_req->sgl_list[0]) * sg_count,
				DMA_TO_DEVICE);
		printk(KERN_ERR "DMA mapping failed\n");
		return SCSI_MLQUEUE_HOST_BUSY;
	}

	int_to_scsilun(sc->device->lun, &fc_lun);

	 
	spin_lock_irqsave(&fnic->wq_copy_lock[0], intr_flags);

	if (vnic_wq_copy_desc_avail(wq) <= fnic->wq_copy_desc_low[0])
		free_wq_copy_descs(fnic, wq);

	if (unlikely(!vnic_wq_copy_desc_avail(wq))) {
		spin_unlock_irqrestore(&fnic->wq_copy_lock[0], intr_flags);
		FNIC_SCSI_DBG(KERN_INFO, fnic->lport->host,
			  "fnic_queue_wq_copy_desc failure - no descriptors\n");
		atomic64_inc(&misc_stats->io_cpwq_alloc_failures);
		return SCSI_MLQUEUE_HOST_BUSY;
	}

	flags = 0;
	if (sc->sc_data_direction == DMA_FROM_DEVICE)
		flags = FCPIO_ICMND_RDDATA;
	else if (sc->sc_data_direction == DMA_TO_DEVICE)
		flags = FCPIO_ICMND_WRDATA;

	exch_flags = 0;
	if ((fnic->config.flags & VFCF_FCP_SEQ_LVL_ERR) &&
	    (rp->flags & FC_RP_FLAGS_RETRY))
		exch_flags |= FCPIO_ICMND_SRFLAG_RETRY;

	fnic_queue_wq_copy_desc_icmnd_16(wq, scsi_cmd_to_rq(sc)->tag,
					 0, exch_flags, io_req->sgl_cnt,
					 SCSI_SENSE_BUFFERSIZE,
					 io_req->sgl_list_pa,
					 io_req->sense_buf_pa,
					 0,  
					 FCPIO_ICMND_PTA_SIMPLE,
					 	 
					 flags,	 
					 sc->cmnd, sc->cmd_len,
					 scsi_bufflen(sc),
					 fc_lun.scsi_lun, io_req->port_id,
					 rport->maxframe_size, rp->r_a_tov,
					 rp->e_d_tov);

	atomic64_inc(&fnic->fnic_stats.fw_stats.active_fw_reqs);
	if (atomic64_read(&fnic->fnic_stats.fw_stats.active_fw_reqs) >
		  atomic64_read(&fnic->fnic_stats.fw_stats.max_fw_reqs))
		atomic64_set(&fnic->fnic_stats.fw_stats.max_fw_reqs,
		  atomic64_read(&fnic->fnic_stats.fw_stats.active_fw_reqs));

	spin_unlock_irqrestore(&fnic->wq_copy_lock[0], intr_flags);
	return 0;
}

 
static int fnic_queuecommand_lck(struct scsi_cmnd *sc)
{
	void (*done)(struct scsi_cmnd *) = scsi_done;
	const int tag = scsi_cmd_to_rq(sc)->tag;
	struct fc_lport *lp = shost_priv(sc->device->host);
	struct fc_rport *rport;
	struct fnic_io_req *io_req = NULL;
	struct fnic *fnic = lport_priv(lp);
	struct fnic_stats *fnic_stats = &fnic->fnic_stats;
	struct vnic_wq_copy *wq;
	int ret;
	u64 cmd_trace;
	int sg_count = 0;
	unsigned long flags = 0;
	unsigned long ptr;
	spinlock_t *io_lock = NULL;
	int io_lock_acquired = 0;
	struct fc_rport_libfc_priv *rp;

	if (unlikely(fnic_chk_state_flags_locked(fnic, FNIC_FLAGS_IO_BLOCKED)))
		return SCSI_MLQUEUE_HOST_BUSY;

	if (unlikely(fnic_chk_state_flags_locked(fnic, FNIC_FLAGS_FWRESET)))
		return SCSI_MLQUEUE_HOST_BUSY;

	rport = starget_to_rport(scsi_target(sc->device));
	if (!rport) {
		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
				"returning DID_NO_CONNECT for IO as rport is NULL\n");
		sc->result = DID_NO_CONNECT << 16;
		done(sc);
		return 0;
	}

	ret = fc_remote_port_chkready(rport);
	if (ret) {
		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
				"rport is not ready\n");
		atomic64_inc(&fnic_stats->misc_stats.rport_not_ready);
		sc->result = ret;
		done(sc);
		return 0;
	}

	rp = rport->dd_data;
	if (!rp || rp->rp_state == RPORT_ST_DELETE) {
		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
			"rport 0x%x removed, returning DID_NO_CONNECT\n",
			rport->port_id);

		atomic64_inc(&fnic_stats->misc_stats.rport_not_ready);
		sc->result = DID_NO_CONNECT<<16;
		done(sc);
		return 0;
	}

	if (rp->rp_state != RPORT_ST_READY) {
		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
			"rport 0x%x in state 0x%x, returning DID_IMM_RETRY\n",
			rport->port_id, rp->rp_state);

		sc->result = DID_IMM_RETRY << 16;
		done(sc);
		return 0;
	}

	if (lp->state != LPORT_ST_READY || !(lp->link_up))
		return SCSI_MLQUEUE_HOST_BUSY;

	atomic_inc(&fnic->in_flight);

	 
	spin_unlock(lp->host->host_lock);
	fnic_priv(sc)->state = FNIC_IOREQ_NOT_INITED;
	fnic_priv(sc)->flags = FNIC_NO_FLAGS;

	 
	io_req = mempool_alloc(fnic->io_req_pool, GFP_ATOMIC);
	if (!io_req) {
		atomic64_inc(&fnic_stats->io_stats.alloc_failures);
		ret = SCSI_MLQUEUE_HOST_BUSY;
		goto out;
	}
	memset(io_req, 0, sizeof(*io_req));

	 
	sg_count = scsi_dma_map(sc);
	if (sg_count < 0) {
		FNIC_TRACE(fnic_queuecommand, sc->device->host->host_no,
			  tag, sc, 0, sc->cmnd[0], sg_count, fnic_priv(sc)->state);
		mempool_free(io_req, fnic->io_req_pool);
		goto out;
	}

	 
	io_req->sgl_cnt = sg_count;
	io_req->sgl_type = FNIC_SGL_CACHE_DFLT;
	if (sg_count > FNIC_DFLT_SG_DESC_CNT)
		io_req->sgl_type = FNIC_SGL_CACHE_MAX;

	if (sg_count) {
		io_req->sgl_list =
			mempool_alloc(fnic->io_sgl_pool[io_req->sgl_type],
				      GFP_ATOMIC);
		if (!io_req->sgl_list) {
			atomic64_inc(&fnic_stats->io_stats.alloc_failures);
			ret = SCSI_MLQUEUE_HOST_BUSY;
			scsi_dma_unmap(sc);
			mempool_free(io_req, fnic->io_req_pool);
			goto out;
		}

		 
		io_req->sgl_list_alloc = io_req->sgl_list;
		ptr = (unsigned long) io_req->sgl_list;
		if (ptr % FNIC_SG_DESC_ALIGN) {
			io_req->sgl_list = (struct host_sg_desc *)
				(((unsigned long) ptr
				  + FNIC_SG_DESC_ALIGN - 1)
				 & ~(FNIC_SG_DESC_ALIGN - 1));
		}
	}

	 

	io_lock = fnic_io_lock_hash(fnic, sc);
	spin_lock_irqsave(io_lock, flags);

	 
	io_lock_acquired = 1;
	io_req->port_id = rport->port_id;
	io_req->start_time = jiffies;
	fnic_priv(sc)->state = FNIC_IOREQ_CMD_PENDING;
	fnic_priv(sc)->io_req = io_req;
	fnic_priv(sc)->flags |= FNIC_IO_INITIALIZED;

	 
	wq = &fnic->wq_copy[0];
	ret = fnic_queue_wq_copy_desc(fnic, wq, io_req, sc, sg_count);
	if (ret) {
		 
		FNIC_TRACE(fnic_queuecommand, sc->device->host->host_no,
			  tag, sc, 0, 0, 0, fnic_flags_and_state(sc));
		io_req = fnic_priv(sc)->io_req;
		fnic_priv(sc)->io_req = NULL;
		fnic_priv(sc)->state = FNIC_IOREQ_CMD_COMPLETE;
		spin_unlock_irqrestore(io_lock, flags);
		if (io_req) {
			fnic_release_ioreq_buf(fnic, io_req, sc);
			mempool_free(io_req, fnic->io_req_pool);
		}
		atomic_dec(&fnic->in_flight);
		 
		spin_lock(lp->host->host_lock);
		return ret;
	} else {
		atomic64_inc(&fnic_stats->io_stats.active_ios);
		atomic64_inc(&fnic_stats->io_stats.num_ios);
		if (atomic64_read(&fnic_stats->io_stats.active_ios) >
			  atomic64_read(&fnic_stats->io_stats.max_active_ios))
			atomic64_set(&fnic_stats->io_stats.max_active_ios,
			     atomic64_read(&fnic_stats->io_stats.active_ios));

		 
		fnic_priv(sc)->flags |= FNIC_IO_ISSUED;
	}
out:
	cmd_trace = ((u64)sc->cmnd[0] << 56 | (u64)sc->cmnd[7] << 40 |
			(u64)sc->cmnd[8] << 32 | (u64)sc->cmnd[2] << 24 |
			(u64)sc->cmnd[3] << 16 | (u64)sc->cmnd[4] << 8 |
			sc->cmnd[5]);

	FNIC_TRACE(fnic_queuecommand, sc->device->host->host_no,
		   tag, sc, io_req, sg_count, cmd_trace,
		   fnic_flags_and_state(sc));

	 
	if (io_lock_acquired)
		spin_unlock_irqrestore(io_lock, flags);

	atomic_dec(&fnic->in_flight);
	 
	spin_lock(lp->host->host_lock);
	return ret;
}

DEF_SCSI_QCMD(fnic_queuecommand)

 
static int fnic_fcpio_fw_reset_cmpl_handler(struct fnic *fnic,
					    struct fcpio_fw_req *desc)
{
	u8 type;
	u8 hdr_status;
	struct fcpio_tag tag;
	int ret = 0;
	unsigned long flags;
	struct reset_stats *reset_stats = &fnic->fnic_stats.reset_stats;

	fcpio_header_dec(&desc->hdr, &type, &hdr_status, &tag);

	atomic64_inc(&reset_stats->fw_reset_completions);

	 
	fnic_cleanup_io(fnic);

	atomic64_set(&fnic->fnic_stats.fw_stats.active_fw_reqs, 0);
	atomic64_set(&fnic->fnic_stats.io_stats.active_ios, 0);
	atomic64_set(&fnic->io_cmpl_skip, 0);

	spin_lock_irqsave(&fnic->fnic_lock, flags);

	 
	if (fnic->state == FNIC_IN_FC_TRANS_ETH_MODE) {
		 
		if (!hdr_status) {
			FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
				      "reset cmpl success\n");
			 
			fnic->state = FNIC_IN_ETH_MODE;
		} else {
			FNIC_SCSI_DBG(KERN_DEBUG,
				      fnic->lport->host,
				      "fnic fw_reset : failed %s\n",
				      fnic_fcpio_status_to_str(hdr_status));

			 
			fnic->state = FNIC_IN_FC_MODE;
			atomic64_inc(&reset_stats->fw_reset_failures);
			ret = -1;
		}
	} else {
		FNIC_SCSI_DBG(KERN_DEBUG,
			      fnic->lport->host,
			      "Unexpected state %s while processing"
			      " reset cmpl\n", fnic_state_to_str(fnic->state));
		atomic64_inc(&reset_stats->fw_reset_failures);
		ret = -1;
	}

	 
	if (fnic->remove_wait)
		complete(fnic->remove_wait);

	 
	if (fnic->remove_wait || ret) {
		spin_unlock_irqrestore(&fnic->fnic_lock, flags);
		skb_queue_purge(&fnic->tx_queue);
		goto reset_cmpl_handler_end;
	}

	spin_unlock_irqrestore(&fnic->fnic_lock, flags);

	fnic_flush_tx(fnic);

 reset_cmpl_handler_end:
	fnic_clear_state_flags(fnic, FNIC_FLAGS_FWRESET);

	return ret;
}

 
static int fnic_fcpio_flogi_reg_cmpl_handler(struct fnic *fnic,
					     struct fcpio_fw_req *desc)
{
	u8 type;
	u8 hdr_status;
	struct fcpio_tag tag;
	int ret = 0;
	unsigned long flags;

	fcpio_header_dec(&desc->hdr, &type, &hdr_status, &tag);

	 
	spin_lock_irqsave(&fnic->fnic_lock, flags);

	if (fnic->state == FNIC_IN_ETH_TRANS_FC_MODE) {

		 
		if (!hdr_status) {
			FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
				      "flog reg succeeded\n");
			fnic->state = FNIC_IN_FC_MODE;
		} else {
			FNIC_SCSI_DBG(KERN_DEBUG,
				      fnic->lport->host,
				      "fnic flogi reg :failed %s\n",
				      fnic_fcpio_status_to_str(hdr_status));
			fnic->state = FNIC_IN_ETH_MODE;
			ret = -1;
		}
	} else {
		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
			      "Unexpected fnic state %s while"
			      " processing flogi reg completion\n",
			      fnic_state_to_str(fnic->state));
		ret = -1;
	}

	if (!ret) {
		if (fnic->stop_rx_link_events) {
			spin_unlock_irqrestore(&fnic->fnic_lock, flags);
			goto reg_cmpl_handler_end;
		}
		spin_unlock_irqrestore(&fnic->fnic_lock, flags);

		fnic_flush_tx(fnic);
		queue_work(fnic_event_queue, &fnic->frame_work);
	} else {
		spin_unlock_irqrestore(&fnic->fnic_lock, flags);
	}

reg_cmpl_handler_end:
	return ret;
}

static inline int is_ack_index_in_range(struct vnic_wq_copy *wq,
					u16 request_out)
{
	if (wq->to_clean_index <= wq->to_use_index) {
		 
		if (request_out < wq->to_clean_index ||
		    request_out >= wq->to_use_index)
			return 0;
	} else {
		 
		if (request_out < wq->to_clean_index &&
		    request_out >= wq->to_use_index)
			return 0;
	}
	 
	return 1;
}


 
static inline void fnic_fcpio_ack_handler(struct fnic *fnic,
					  unsigned int cq_index,
					  struct fcpio_fw_req *desc)
{
	struct vnic_wq_copy *wq;
	u16 request_out = desc->u.ack.request_out;
	unsigned long flags;
	u64 *ox_id_tag = (u64 *)(void *)desc;

	 
	wq = &fnic->wq_copy[cq_index - fnic->raw_wq_count - fnic->rq_count];
	spin_lock_irqsave(&fnic->wq_copy_lock[0], flags);

	fnic->fnic_stats.misc_stats.last_ack_time = jiffies;
	if (is_ack_index_in_range(wq, request_out)) {
		fnic->fw_ack_index[0] = request_out;
		fnic->fw_ack_recd[0] = 1;
	} else
		atomic64_inc(
			&fnic->fnic_stats.misc_stats.ack_index_out_of_range);

	spin_unlock_irqrestore(&fnic->wq_copy_lock[0], flags);
	FNIC_TRACE(fnic_fcpio_ack_handler,
		  fnic->lport->host->host_no, 0, 0, ox_id_tag[2], ox_id_tag[3],
		  ox_id_tag[4], ox_id_tag[5]);
}

 
static void fnic_fcpio_icmnd_cmpl_handler(struct fnic *fnic,
					 struct fcpio_fw_req *desc)
{
	u8 type;
	u8 hdr_status;
	struct fcpio_tag tag;
	u32 id;
	u64 xfer_len = 0;
	struct fcpio_icmnd_cmpl *icmnd_cmpl;
	struct fnic_io_req *io_req;
	struct scsi_cmnd *sc;
	struct fnic_stats *fnic_stats = &fnic->fnic_stats;
	unsigned long flags;
	spinlock_t *io_lock;
	u64 cmd_trace;
	unsigned long start_time;
	unsigned long io_duration_time;

	 
	fcpio_header_dec(&desc->hdr, &type, &hdr_status, &tag);
	fcpio_tag_id_dec(&tag, &id);
	icmnd_cmpl = &desc->u.icmnd_cmpl;

	if (id >= fnic->fnic_max_tag_id) {
		shost_printk(KERN_ERR, fnic->lport->host,
			"Tag out of range tag %x hdr status = %s\n",
			     id, fnic_fcpio_status_to_str(hdr_status));
		return;
	}

	sc = scsi_host_find_tag(fnic->lport->host, id);
	WARN_ON_ONCE(!sc);
	if (!sc) {
		atomic64_inc(&fnic_stats->io_stats.sc_null);
		shost_printk(KERN_ERR, fnic->lport->host,
			  "icmnd_cmpl sc is null - "
			  "hdr status = %s tag = 0x%x desc = 0x%p\n",
			  fnic_fcpio_status_to_str(hdr_status), id, desc);
		FNIC_TRACE(fnic_fcpio_icmnd_cmpl_handler,
			  fnic->lport->host->host_no, id,
			  ((u64)icmnd_cmpl->_resvd0[1] << 16 |
			  (u64)icmnd_cmpl->_resvd0[0]),
			  ((u64)hdr_status << 16 |
			  (u64)icmnd_cmpl->scsi_status << 8 |
			  (u64)icmnd_cmpl->flags), desc,
			  (u64)icmnd_cmpl->residual, 0);
		return;
	}

	io_lock = fnic_io_lock_hash(fnic, sc);
	spin_lock_irqsave(io_lock, flags);
	io_req = fnic_priv(sc)->io_req;
	WARN_ON_ONCE(!io_req);
	if (!io_req) {
		atomic64_inc(&fnic_stats->io_stats.ioreq_null);
		fnic_priv(sc)->flags |= FNIC_IO_REQ_NULL;
		spin_unlock_irqrestore(io_lock, flags);
		shost_printk(KERN_ERR, fnic->lport->host,
			  "icmnd_cmpl io_req is null - "
			  "hdr status = %s tag = 0x%x sc 0x%p\n",
			  fnic_fcpio_status_to_str(hdr_status), id, sc);
		return;
	}
	start_time = io_req->start_time;

	 
	io_req->io_completed = 1;

	 
	if (fnic_priv(sc)->state == FNIC_IOREQ_ABTS_PENDING) {

		 
		fnic_priv(sc)->flags |= FNIC_IO_DONE;
		fnic_priv(sc)->flags |= FNIC_IO_ABTS_PENDING;
		spin_unlock_irqrestore(io_lock, flags);
		if(FCPIO_ABORTED == hdr_status)
			fnic_priv(sc)->flags |= FNIC_IO_ABORTED;

		FNIC_SCSI_DBG(KERN_INFO, fnic->lport->host,
			"icmnd_cmpl abts pending "
			  "hdr status = %s tag = 0x%x sc = 0x%p "
			  "scsi_status = %x residual = %d\n",
			  fnic_fcpio_status_to_str(hdr_status),
			  id, sc,
			  icmnd_cmpl->scsi_status,
			  icmnd_cmpl->residual);
		return;
	}

	 
	fnic_priv(sc)->state = FNIC_IOREQ_CMD_COMPLETE;

	icmnd_cmpl = &desc->u.icmnd_cmpl;

	switch (hdr_status) {
	case FCPIO_SUCCESS:
		sc->result = (DID_OK << 16) | icmnd_cmpl->scsi_status;
		xfer_len = scsi_bufflen(sc);

		if (icmnd_cmpl->flags & FCPIO_ICMND_CMPL_RESID_UNDER) {
			xfer_len -= icmnd_cmpl->residual;
			scsi_set_resid(sc, icmnd_cmpl->residual);
		}

		if (icmnd_cmpl->scsi_status == SAM_STAT_CHECK_CONDITION)
			atomic64_inc(&fnic_stats->misc_stats.check_condition);

		if (icmnd_cmpl->scsi_status == SAM_STAT_TASK_SET_FULL)
			atomic64_inc(&fnic_stats->misc_stats.queue_fulls);
		break;

	case FCPIO_TIMEOUT:           
		atomic64_inc(&fnic_stats->misc_stats.fcpio_timeout);
		sc->result = (DID_TIME_OUT << 16) | icmnd_cmpl->scsi_status;
		break;

	case FCPIO_ABORTED:           
		atomic64_inc(&fnic_stats->misc_stats.fcpio_aborted);
		sc->result = (DID_ERROR << 16) | icmnd_cmpl->scsi_status;
		break;

	case FCPIO_DATA_CNT_MISMATCH:  
		atomic64_inc(&fnic_stats->misc_stats.data_count_mismatch);
		scsi_set_resid(sc, icmnd_cmpl->residual);
		sc->result = (DID_ERROR << 16) | icmnd_cmpl->scsi_status;
		break;

	case FCPIO_OUT_OF_RESOURCE:   
		atomic64_inc(&fnic_stats->fw_stats.fw_out_of_resources);
		sc->result = (DID_REQUEUE << 16) | icmnd_cmpl->scsi_status;
		break;

	case FCPIO_IO_NOT_FOUND:      
		atomic64_inc(&fnic_stats->io_stats.io_not_found);
		sc->result = (DID_ERROR << 16) | icmnd_cmpl->scsi_status;
		break;

	case FCPIO_SGL_INVALID:       
		atomic64_inc(&fnic_stats->misc_stats.sgl_invalid);
		sc->result = (DID_ERROR << 16) | icmnd_cmpl->scsi_status;
		break;

	case FCPIO_FW_ERR:            
		atomic64_inc(&fnic_stats->fw_stats.io_fw_errs);
		sc->result = (DID_ERROR << 16) | icmnd_cmpl->scsi_status;
		break;

	case FCPIO_MSS_INVALID:       
		atomic64_inc(&fnic_stats->misc_stats.mss_invalid);
		sc->result = (DID_ERROR << 16) | icmnd_cmpl->scsi_status;
		break;

	case FCPIO_INVALID_HEADER:    
	case FCPIO_INVALID_PARAM:     
	case FCPIO_REQ_NOT_SUPPORTED: 
	default:
		sc->result = (DID_ERROR << 16) | icmnd_cmpl->scsi_status;
		break;
	}

	 
	fnic_priv(sc)->io_req = NULL;
	fnic_priv(sc)->flags |= FNIC_IO_DONE;

	if (hdr_status != FCPIO_SUCCESS) {
		atomic64_inc(&fnic_stats->io_stats.io_failures);
		shost_printk(KERN_ERR, fnic->lport->host, "hdr status = %s\n",
			     fnic_fcpio_status_to_str(hdr_status));
	}

	fnic_release_ioreq_buf(fnic, io_req, sc);

	cmd_trace = ((u64)hdr_status << 56) |
		  (u64)icmnd_cmpl->scsi_status << 48 |
		  (u64)icmnd_cmpl->flags << 40 | (u64)sc->cmnd[0] << 32 |
		  (u64)sc->cmnd[2] << 24 | (u64)sc->cmnd[3] << 16 |
		  (u64)sc->cmnd[4] << 8 | sc->cmnd[5];

	FNIC_TRACE(fnic_fcpio_icmnd_cmpl_handler,
		  sc->device->host->host_no, id, sc,
		  ((u64)icmnd_cmpl->_resvd0[1] << 56 |
		  (u64)icmnd_cmpl->_resvd0[0] << 48 |
		  jiffies_to_msecs(jiffies - start_time)),
		  desc, cmd_trace, fnic_flags_and_state(sc));

	if (sc->sc_data_direction == DMA_FROM_DEVICE) {
		fnic->lport->host_stats.fcp_input_requests++;
		fnic->fcp_input_bytes += xfer_len;
	} else if (sc->sc_data_direction == DMA_TO_DEVICE) {
		fnic->lport->host_stats.fcp_output_requests++;
		fnic->fcp_output_bytes += xfer_len;
	} else
		fnic->lport->host_stats.fcp_control_requests++;

	 
	scsi_done(sc);
	spin_unlock_irqrestore(io_lock, flags);

	mempool_free(io_req, fnic->io_req_pool);

	atomic64_dec(&fnic_stats->io_stats.active_ios);
	if (atomic64_read(&fnic->io_cmpl_skip))
		atomic64_dec(&fnic->io_cmpl_skip);
	else
		atomic64_inc(&fnic_stats->io_stats.io_completions);


	io_duration_time = jiffies_to_msecs(jiffies) -
						jiffies_to_msecs(start_time);

	if(io_duration_time <= 10)
		atomic64_inc(&fnic_stats->io_stats.io_btw_0_to_10_msec);
	else if(io_duration_time <= 100)
		atomic64_inc(&fnic_stats->io_stats.io_btw_10_to_100_msec);
	else if(io_duration_time <= 500)
		atomic64_inc(&fnic_stats->io_stats.io_btw_100_to_500_msec);
	else if(io_duration_time <= 5000)
		atomic64_inc(&fnic_stats->io_stats.io_btw_500_to_5000_msec);
	else if(io_duration_time <= 10000)
		atomic64_inc(&fnic_stats->io_stats.io_btw_5000_to_10000_msec);
	else if(io_duration_time <= 30000)
		atomic64_inc(&fnic_stats->io_stats.io_btw_10000_to_30000_msec);
	else {
		atomic64_inc(&fnic_stats->io_stats.io_greater_than_30000_msec);

		if(io_duration_time > atomic64_read(&fnic_stats->io_stats.current_max_io_time))
			atomic64_set(&fnic_stats->io_stats.current_max_io_time, io_duration_time);
	}
}

 
static void fnic_fcpio_itmf_cmpl_handler(struct fnic *fnic,
					struct fcpio_fw_req *desc)
{
	u8 type;
	u8 hdr_status;
	struct fcpio_tag ftag;
	u32 id;
	struct scsi_cmnd *sc = NULL;
	struct fnic_io_req *io_req;
	struct fnic_stats *fnic_stats = &fnic->fnic_stats;
	struct abort_stats *abts_stats = &fnic->fnic_stats.abts_stats;
	struct terminate_stats *term_stats = &fnic->fnic_stats.term_stats;
	struct misc_stats *misc_stats = &fnic->fnic_stats.misc_stats;
	unsigned long flags;
	spinlock_t *io_lock;
	unsigned long start_time;
	unsigned int tag;

	fcpio_header_dec(&desc->hdr, &type, &hdr_status, &ftag);
	fcpio_tag_id_dec(&ftag, &id);

	tag = id & FNIC_TAG_MASK;
	if (tag == fnic->fnic_max_tag_id) {
		if (!(id & FNIC_TAG_DEV_RST)) {
			shost_printk(KERN_ERR, fnic->lport->host,
						"Tag out of range id 0x%x hdr status = %s\n",
						id, fnic_fcpio_status_to_str(hdr_status));
			return;
		}
	} else if (tag > fnic->fnic_max_tag_id) {
		shost_printk(KERN_ERR, fnic->lport->host,
					"Tag out of range tag 0x%x hdr status = %s\n",
					tag, fnic_fcpio_status_to_str(hdr_status));
		return;
	}

	if ((tag == fnic->fnic_max_tag_id) && (id & FNIC_TAG_DEV_RST)) {
		sc = fnic->sgreset_sc;
		io_lock = &fnic->sgreset_lock;
	} else {
		sc = scsi_host_find_tag(fnic->lport->host, id & FNIC_TAG_MASK);
		io_lock = fnic_io_lock_hash(fnic, sc);
	}

	WARN_ON_ONCE(!sc);
	if (!sc) {
		atomic64_inc(&fnic_stats->io_stats.sc_null);
		shost_printk(KERN_ERR, fnic->lport->host,
			  "itmf_cmpl sc is null - hdr status = %s tag = 0x%x\n",
			  fnic_fcpio_status_to_str(hdr_status), tag);
		return;
	}

	spin_lock_irqsave(io_lock, flags);
	io_req = fnic_priv(sc)->io_req;
	WARN_ON_ONCE(!io_req);
	if (!io_req) {
		atomic64_inc(&fnic_stats->io_stats.ioreq_null);
		spin_unlock_irqrestore(io_lock, flags);
		fnic_priv(sc)->flags |= FNIC_IO_ABT_TERM_REQ_NULL;
		shost_printk(KERN_ERR, fnic->lport->host,
			  "itmf_cmpl io_req is null - "
			  "hdr status = %s tag = 0x%x sc 0x%p\n",
			  fnic_fcpio_status_to_str(hdr_status), tag, sc);
		return;
	}
	start_time = io_req->start_time;

	if ((id & FNIC_TAG_ABORT) && (id & FNIC_TAG_DEV_RST)) {
		 
		 
		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
			      "dev reset abts cmpl recd. id %x status %s\n",
			      id, fnic_fcpio_status_to_str(hdr_status));
		fnic_priv(sc)->state = FNIC_IOREQ_ABTS_COMPLETE;
		fnic_priv(sc)->abts_status = hdr_status;
		fnic_priv(sc)->flags |= FNIC_DEV_RST_DONE;
		if (io_req->abts_done)
			complete(io_req->abts_done);
		spin_unlock_irqrestore(io_lock, flags);
	} else if (id & FNIC_TAG_ABORT) {
		 
		switch (hdr_status) {
		case FCPIO_SUCCESS:
			break;
		case FCPIO_TIMEOUT:
			if (fnic_priv(sc)->flags & FNIC_IO_ABTS_ISSUED)
				atomic64_inc(&abts_stats->abort_fw_timeouts);
			else
				atomic64_inc(
					&term_stats->terminate_fw_timeouts);
			break;
		case FCPIO_ITMF_REJECTED:
			FNIC_SCSI_DBG(KERN_INFO, fnic->lport->host,
				"abort reject recd. id %d\n",
				(int)(id & FNIC_TAG_MASK));
			break;
		case FCPIO_IO_NOT_FOUND:
			if (fnic_priv(sc)->flags & FNIC_IO_ABTS_ISSUED)
				atomic64_inc(&abts_stats->abort_io_not_found);
			else
				atomic64_inc(
					&term_stats->terminate_io_not_found);
			break;
		default:
			if (fnic_priv(sc)->flags & FNIC_IO_ABTS_ISSUED)
				atomic64_inc(&abts_stats->abort_failures);
			else
				atomic64_inc(
					&term_stats->terminate_failures);
			break;
		}
		if (fnic_priv(sc)->state != FNIC_IOREQ_ABTS_PENDING) {
			 
			spin_unlock_irqrestore(io_lock, flags);
			return;
		}

		fnic_priv(sc)->flags |= FNIC_IO_ABT_TERM_DONE;
		fnic_priv(sc)->abts_status = hdr_status;

		 
		if (hdr_status == FCPIO_IO_NOT_FOUND)
			fnic_priv(sc)->abts_status = FCPIO_SUCCESS;

		if (!(fnic_priv(sc)->flags & (FNIC_IO_ABORTED | FNIC_IO_DONE)))
			atomic64_inc(&misc_stats->no_icmnd_itmf_cmpls);

		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
			      "abts cmpl recd. id %d status %s\n",
			      (int)(id & FNIC_TAG_MASK),
			      fnic_fcpio_status_to_str(hdr_status));

		 
		if (io_req->abts_done) {
			complete(io_req->abts_done);
			spin_unlock_irqrestore(io_lock, flags);
		} else {
			FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
				      "abts cmpl, completing IO\n");
			fnic_priv(sc)->io_req = NULL;
			sc->result = (DID_ERROR << 16);

			spin_unlock_irqrestore(io_lock, flags);

			fnic_release_ioreq_buf(fnic, io_req, sc);
			mempool_free(io_req, fnic->io_req_pool);
			FNIC_TRACE(fnic_fcpio_itmf_cmpl_handler,
				   sc->device->host->host_no, id,
				   sc,
				   jiffies_to_msecs(jiffies - start_time),
				   desc,
				   (((u64)hdr_status << 40) |
				    (u64)sc->cmnd[0] << 32 |
				    (u64)sc->cmnd[2] << 24 |
				    (u64)sc->cmnd[3] << 16 |
				    (u64)sc->cmnd[4] << 8 | sc->cmnd[5]),
				   fnic_flags_and_state(sc));
			scsi_done(sc);
			atomic64_dec(&fnic_stats->io_stats.active_ios);
			if (atomic64_read(&fnic->io_cmpl_skip))
				atomic64_dec(&fnic->io_cmpl_skip);
			else
				atomic64_inc(&fnic_stats->io_stats.io_completions);
		}
	} else if (id & FNIC_TAG_DEV_RST) {
		 
		fnic_priv(sc)->lr_status = hdr_status;
		if (fnic_priv(sc)->state == FNIC_IOREQ_ABTS_PENDING) {
			spin_unlock_irqrestore(io_lock, flags);
			fnic_priv(sc)->flags |= FNIC_DEV_RST_ABTS_PENDING;
			FNIC_TRACE(fnic_fcpio_itmf_cmpl_handler,
				  sc->device->host->host_no, id, sc,
				  jiffies_to_msecs(jiffies - start_time),
				  desc, 0, fnic_flags_and_state(sc));
			FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
				"Terminate pending "
				"dev reset cmpl recd. id %d status %s\n",
				(int)(id & FNIC_TAG_MASK),
				fnic_fcpio_status_to_str(hdr_status));
			return;
		}
		if (fnic_priv(sc)->flags & FNIC_DEV_RST_TIMED_OUT) {
			 
			spin_unlock_irqrestore(io_lock, flags);
			FNIC_TRACE(fnic_fcpio_itmf_cmpl_handler,
				  sc->device->host->host_no, id, sc,
				  jiffies_to_msecs(jiffies - start_time),
				  desc, 0, fnic_flags_and_state(sc));
			FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
				"dev reset cmpl recd after time out. "
				"id %d status %s\n",
				(int)(id & FNIC_TAG_MASK),
				fnic_fcpio_status_to_str(hdr_status));
			return;
		}
		fnic_priv(sc)->state = FNIC_IOREQ_CMD_COMPLETE;
		fnic_priv(sc)->flags |= FNIC_DEV_RST_DONE;
		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
			      "dev reset cmpl recd. id %d status %s\n",
			      (int)(id & FNIC_TAG_MASK),
			      fnic_fcpio_status_to_str(hdr_status));
		if (io_req->dr_done)
			complete(io_req->dr_done);
		spin_unlock_irqrestore(io_lock, flags);

	} else {
		shost_printk(KERN_ERR, fnic->lport->host,
			     "Unexpected itmf io state %s tag %x\n",
			     fnic_ioreq_state_to_str(fnic_priv(sc)->state), id);
		spin_unlock_irqrestore(io_lock, flags);
	}

}

 
static int fnic_fcpio_cmpl_handler(struct vnic_dev *vdev,
				   unsigned int cq_index,
				   struct fcpio_fw_req *desc)
{
	struct fnic *fnic = vnic_dev_priv(vdev);

	switch (desc->hdr.type) {
	case FCPIO_ICMND_CMPL:  
	case FCPIO_ITMF_CMPL:  
	case FCPIO_FLOGI_REG_CMPL:  
	case FCPIO_FLOGI_FIP_REG_CMPL:  
	case FCPIO_RESET_CMPL:  
		atomic64_dec(&fnic->fnic_stats.fw_stats.active_fw_reqs);
		break;
	default:
		break;
	}

	switch (desc->hdr.type) {
	case FCPIO_ACK:  
		fnic_fcpio_ack_handler(fnic, cq_index, desc);
		break;

	case FCPIO_ICMND_CMPL:  
		fnic_fcpio_icmnd_cmpl_handler(fnic, desc);
		break;

	case FCPIO_ITMF_CMPL:  
		fnic_fcpio_itmf_cmpl_handler(fnic, desc);
		break;

	case FCPIO_FLOGI_REG_CMPL:  
	case FCPIO_FLOGI_FIP_REG_CMPL:  
		fnic_fcpio_flogi_reg_cmpl_handler(fnic, desc);
		break;

	case FCPIO_RESET_CMPL:  
		fnic_fcpio_fw_reset_cmpl_handler(fnic, desc);
		break;

	default:
		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
			      "firmware completion type %d\n",
			      desc->hdr.type);
		break;
	}

	return 0;
}

 
int fnic_wq_copy_cmpl_handler(struct fnic *fnic, int copy_work_to_do)
{
	unsigned int wq_work_done = 0;
	unsigned int i, cq_index;
	unsigned int cur_work_done;
	struct misc_stats *misc_stats = &fnic->fnic_stats.misc_stats;
	u64 start_jiffies = 0;
	u64 end_jiffies = 0;
	u64 delta_jiffies = 0;
	u64 delta_ms = 0;

	for (i = 0; i < fnic->wq_copy_count; i++) {
		cq_index = i + fnic->raw_wq_count + fnic->rq_count;

		start_jiffies = jiffies;
		cur_work_done = vnic_cq_copy_service(&fnic->cq[cq_index],
						     fnic_fcpio_cmpl_handler,
						     copy_work_to_do);
		end_jiffies = jiffies;

		wq_work_done += cur_work_done;
		delta_jiffies = end_jiffies - start_jiffies;
		if (delta_jiffies >
			(u64) atomic64_read(&misc_stats->max_isr_jiffies)) {
			atomic64_set(&misc_stats->max_isr_jiffies,
					delta_jiffies);
			delta_ms = jiffies_to_msecs(delta_jiffies);
			atomic64_set(&misc_stats->max_isr_time_ms, delta_ms);
			atomic64_set(&misc_stats->corr_work_done,
					cur_work_done);
		}
	}
	return wq_work_done;
}

static bool fnic_cleanup_io_iter(struct scsi_cmnd *sc, void *data)
{
	const int tag = scsi_cmd_to_rq(sc)->tag;
	struct fnic *fnic = data;
	struct fnic_io_req *io_req;
	unsigned long flags = 0;
	spinlock_t *io_lock;
	unsigned long start_time = 0;
	struct fnic_stats *fnic_stats = &fnic->fnic_stats;

	io_lock = fnic_io_lock_tag(fnic, tag);
	spin_lock_irqsave(io_lock, flags);

	io_req = fnic_priv(sc)->io_req;
	if ((fnic_priv(sc)->flags & FNIC_DEVICE_RESET) &&
	    !(fnic_priv(sc)->flags & FNIC_DEV_RST_DONE)) {
		 
		fnic_priv(sc)->flags |= FNIC_DEV_RST_DONE;
		if (io_req && io_req->dr_done)
			complete(io_req->dr_done);
		else if (io_req && io_req->abts_done)
			complete(io_req->abts_done);
		spin_unlock_irqrestore(io_lock, flags);
		return true;
	} else if (fnic_priv(sc)->flags & FNIC_DEVICE_RESET) {
		spin_unlock_irqrestore(io_lock, flags);
		return true;
	}
	if (!io_req) {
		spin_unlock_irqrestore(io_lock, flags);
		goto cleanup_scsi_cmd;
	}

	fnic_priv(sc)->io_req = NULL;

	spin_unlock_irqrestore(io_lock, flags);

	 
	start_time = io_req->start_time;
	fnic_release_ioreq_buf(fnic, io_req, sc);
	mempool_free(io_req, fnic->io_req_pool);

cleanup_scsi_cmd:
	sc->result = DID_TRANSPORT_DISRUPTED << 16;
	FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
		      "fnic_cleanup_io: tag:0x%x : sc:0x%p duration = %lu DID_TRANSPORT_DISRUPTED\n",
		      tag, sc, jiffies - start_time);

	if (atomic64_read(&fnic->io_cmpl_skip))
		atomic64_dec(&fnic->io_cmpl_skip);
	else
		atomic64_inc(&fnic_stats->io_stats.io_completions);

	 
	if (!(fnic_priv(sc)->flags & FNIC_IO_ISSUED))
		shost_printk(KERN_ERR, fnic->lport->host,
			     "Calling done for IO not issued to fw: tag:0x%x sc:0x%p\n",
			     tag, sc);

	FNIC_TRACE(fnic_cleanup_io,
		   sc->device->host->host_no, tag, sc,
		   jiffies_to_msecs(jiffies - start_time),
		   0, ((u64)sc->cmnd[0] << 32 |
		       (u64)sc->cmnd[2] << 24 |
		       (u64)sc->cmnd[3] << 16 |
		       (u64)sc->cmnd[4] << 8 | sc->cmnd[5]),
		   fnic_flags_and_state(sc));

	scsi_done(sc);

	return true;
}

static void fnic_cleanup_io(struct fnic *fnic)
{
	scsi_host_busy_iter(fnic->lport->host,
			    fnic_cleanup_io_iter, fnic);
}

void fnic_wq_copy_cleanup_handler(struct vnic_wq_copy *wq,
				  struct fcpio_host_req *desc)
{
	u32 id;
	struct fnic *fnic = vnic_dev_priv(wq->vdev);
	struct fnic_io_req *io_req;
	struct scsi_cmnd *sc;
	unsigned long flags;
	spinlock_t *io_lock;
	unsigned long start_time = 0;

	 
	fcpio_tag_id_dec(&desc->hdr.tag, &id);
	id &= FNIC_TAG_MASK;

	if (id >= fnic->fnic_max_tag_id)
		return;

	sc = scsi_host_find_tag(fnic->lport->host, id);
	if (!sc)
		return;

	io_lock = fnic_io_lock_hash(fnic, sc);
	spin_lock_irqsave(io_lock, flags);

	 
	io_req = fnic_priv(sc)->io_req;

	 

	if (!io_req) {
		spin_unlock_irqrestore(io_lock, flags);
		goto wq_copy_cleanup_scsi_cmd;
	}

	fnic_priv(sc)->io_req = NULL;

	spin_unlock_irqrestore(io_lock, flags);

	start_time = io_req->start_time;
	fnic_release_ioreq_buf(fnic, io_req, sc);
	mempool_free(io_req, fnic->io_req_pool);

wq_copy_cleanup_scsi_cmd:
	sc->result = DID_NO_CONNECT << 16;
	FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host, "wq_copy_cleanup_handler:"
		      " DID_NO_CONNECT\n");

	FNIC_TRACE(fnic_wq_copy_cleanup_handler,
		   sc->device->host->host_no, id, sc,
		   jiffies_to_msecs(jiffies - start_time),
		   0, ((u64)sc->cmnd[0] << 32 |
		       (u64)sc->cmnd[2] << 24 | (u64)sc->cmnd[3] << 16 |
		       (u64)sc->cmnd[4] << 8 | sc->cmnd[5]),
		   fnic_flags_and_state(sc));

	scsi_done(sc);
}

static inline int fnic_queue_abort_io_req(struct fnic *fnic, int tag,
					  u32 task_req, u8 *fc_lun,
					  struct fnic_io_req *io_req)
{
	struct vnic_wq_copy *wq = &fnic->wq_copy[0];
	struct Scsi_Host *host = fnic->lport->host;
	struct misc_stats *misc_stats = &fnic->fnic_stats.misc_stats;
	unsigned long flags;

	spin_lock_irqsave(host->host_lock, flags);
	if (unlikely(fnic_chk_state_flags_locked(fnic,
						FNIC_FLAGS_IO_BLOCKED))) {
		spin_unlock_irqrestore(host->host_lock, flags);
		return 1;
	} else
		atomic_inc(&fnic->in_flight);
	spin_unlock_irqrestore(host->host_lock, flags);

	spin_lock_irqsave(&fnic->wq_copy_lock[0], flags);

	if (vnic_wq_copy_desc_avail(wq) <= fnic->wq_copy_desc_low[0])
		free_wq_copy_descs(fnic, wq);

	if (!vnic_wq_copy_desc_avail(wq)) {
		spin_unlock_irqrestore(&fnic->wq_copy_lock[0], flags);
		atomic_dec(&fnic->in_flight);
		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
			"fnic_queue_abort_io_req: failure: no descriptors\n");
		atomic64_inc(&misc_stats->abts_cpwq_alloc_failures);
		return 1;
	}
	fnic_queue_wq_copy_desc_itmf(wq, tag | FNIC_TAG_ABORT,
				     0, task_req, tag, fc_lun, io_req->port_id,
				     fnic->config.ra_tov, fnic->config.ed_tov);

	atomic64_inc(&fnic->fnic_stats.fw_stats.active_fw_reqs);
	if (atomic64_read(&fnic->fnic_stats.fw_stats.active_fw_reqs) >
		  atomic64_read(&fnic->fnic_stats.fw_stats.max_fw_reqs))
		atomic64_set(&fnic->fnic_stats.fw_stats.max_fw_reqs,
		  atomic64_read(&fnic->fnic_stats.fw_stats.active_fw_reqs));

	spin_unlock_irqrestore(&fnic->wq_copy_lock[0], flags);
	atomic_dec(&fnic->in_flight);

	return 0;
}

struct fnic_rport_abort_io_iter_data {
	struct fnic *fnic;
	u32 port_id;
	int term_cnt;
};

static bool fnic_rport_abort_io_iter(struct scsi_cmnd *sc, void *data)
{
	struct fnic_rport_abort_io_iter_data *iter_data = data;
	struct fnic *fnic = iter_data->fnic;
	int abt_tag = scsi_cmd_to_rq(sc)->tag;
	struct fnic_io_req *io_req;
	spinlock_t *io_lock;
	unsigned long flags;
	struct reset_stats *reset_stats = &fnic->fnic_stats.reset_stats;
	struct terminate_stats *term_stats = &fnic->fnic_stats.term_stats;
	struct scsi_lun fc_lun;
	enum fnic_ioreq_state old_ioreq_state;

	io_lock = fnic_io_lock_tag(fnic, abt_tag);
	spin_lock_irqsave(io_lock, flags);

	io_req = fnic_priv(sc)->io_req;

	if (!io_req || io_req->port_id != iter_data->port_id) {
		spin_unlock_irqrestore(io_lock, flags);
		return true;
	}

	if ((fnic_priv(sc)->flags & FNIC_DEVICE_RESET) &&
	    !(fnic_priv(sc)->flags & FNIC_DEV_RST_ISSUED)) {
		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
			"fnic_rport_exch_reset dev rst not pending sc 0x%p\n",
			sc);
		spin_unlock_irqrestore(io_lock, flags);
		return true;
	}

	 
	if (fnic_priv(sc)->state == FNIC_IOREQ_ABTS_PENDING) {
		spin_unlock_irqrestore(io_lock, flags);
		return true;
	}
	if (io_req->abts_done) {
		shost_printk(KERN_ERR, fnic->lport->host,
			"fnic_rport_exch_reset: io_req->abts_done is set "
			"state is %s\n",
			fnic_ioreq_state_to_str(fnic_priv(sc)->state));
	}

	if (!(fnic_priv(sc)->flags & FNIC_IO_ISSUED)) {
		shost_printk(KERN_ERR, fnic->lport->host,
			     "rport_exch_reset "
			     "IO not yet issued %p tag 0x%x flags "
			     "%x state %d\n",
			     sc, abt_tag, fnic_priv(sc)->flags, fnic_priv(sc)->state);
	}
	old_ioreq_state = fnic_priv(sc)->state;
	fnic_priv(sc)->state = FNIC_IOREQ_ABTS_PENDING;
	fnic_priv(sc)->abts_status = FCPIO_INVALID_CODE;
	if (fnic_priv(sc)->flags & FNIC_DEVICE_RESET) {
		atomic64_inc(&reset_stats->device_reset_terminates);
		abt_tag |= FNIC_TAG_DEV_RST;
	}
	FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
		      "fnic_rport_exch_reset dev rst sc 0x%p\n", sc);
	BUG_ON(io_req->abts_done);

	FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
		      "fnic_rport_reset_exch: Issuing abts\n");

	spin_unlock_irqrestore(io_lock, flags);

	 
	int_to_scsilun(sc->device->lun, &fc_lun);

	if (fnic_queue_abort_io_req(fnic, abt_tag,
				    FCPIO_ITMF_ABT_TASK_TERM,
				    fc_lun.scsi_lun, io_req)) {
		 
		spin_lock_irqsave(io_lock, flags);
		if (fnic_priv(sc)->state == FNIC_IOREQ_ABTS_PENDING)
			fnic_priv(sc)->state = old_ioreq_state;
		spin_unlock_irqrestore(io_lock, flags);
	} else {
		spin_lock_irqsave(io_lock, flags);
		if (fnic_priv(sc)->flags & FNIC_DEVICE_RESET)
			fnic_priv(sc)->flags |= FNIC_DEV_RST_TERM_ISSUED;
		else
			fnic_priv(sc)->flags |= FNIC_IO_INTERNAL_TERM_ISSUED;
		spin_unlock_irqrestore(io_lock, flags);
		atomic64_inc(&term_stats->terminates);
		iter_data->term_cnt++;
	}
	return true;
}

static void fnic_rport_exch_reset(struct fnic *fnic, u32 port_id)
{
	struct terminate_stats *term_stats = &fnic->fnic_stats.term_stats;
	struct fnic_rport_abort_io_iter_data iter_data = {
		.fnic = fnic,
		.port_id = port_id,
		.term_cnt = 0,
	};

	FNIC_SCSI_DBG(KERN_DEBUG,
		      fnic->lport->host,
		      "fnic_rport_exch_reset called portid 0x%06x\n",
		      port_id);

	if (fnic->in_remove)
		return;

	scsi_host_busy_iter(fnic->lport->host, fnic_rport_abort_io_iter,
			    &iter_data);
	if (iter_data.term_cnt > atomic64_read(&term_stats->max_terminates))
		atomic64_set(&term_stats->max_terminates, iter_data.term_cnt);

}

void fnic_terminate_rport_io(struct fc_rport *rport)
{
	struct fc_rport_libfc_priv *rdata;
	struct fc_lport *lport;
	struct fnic *fnic;

	if (!rport) {
		printk(KERN_ERR "fnic_terminate_rport_io: rport is NULL\n");
		return;
	}
	rdata = rport->dd_data;

	if (!rdata) {
		printk(KERN_ERR "fnic_terminate_rport_io: rdata is NULL\n");
		return;
	}
	lport = rdata->local_port;

	if (!lport) {
		printk(KERN_ERR "fnic_terminate_rport_io: lport is NULL\n");
		return;
	}
	fnic = lport_priv(lport);
	FNIC_SCSI_DBG(KERN_DEBUG,
		      fnic->lport->host, "fnic_terminate_rport_io called"
		      " wwpn 0x%llx, wwnn0x%llx, rport 0x%p, portid 0x%06x\n",
		      rport->port_name, rport->node_name, rport,
		      rport->port_id);

	if (fnic->in_remove)
		return;

	fnic_rport_exch_reset(fnic, rport->port_id);
}

 
int fnic_abort_cmd(struct scsi_cmnd *sc)
{
	struct request *const rq = scsi_cmd_to_rq(sc);
	struct fc_lport *lp;
	struct fnic *fnic;
	struct fnic_io_req *io_req = NULL;
	struct fc_rport *rport;
	spinlock_t *io_lock;
	unsigned long flags;
	unsigned long start_time = 0;
	int ret = SUCCESS;
	u32 task_req = 0;
	struct scsi_lun fc_lun;
	struct fnic_stats *fnic_stats;
	struct abort_stats *abts_stats;
	struct terminate_stats *term_stats;
	enum fnic_ioreq_state old_ioreq_state;
	const int tag = rq->tag;
	unsigned long abt_issued_time;
	DECLARE_COMPLETION_ONSTACK(tm_done);

	 
	fc_block_scsi_eh(sc);

	 
	lp = shost_priv(sc->device->host);

	fnic = lport_priv(lp);
	fnic_stats = &fnic->fnic_stats;
	abts_stats = &fnic->fnic_stats.abts_stats;
	term_stats = &fnic->fnic_stats.term_stats;

	rport = starget_to_rport(scsi_target(sc->device));
	FNIC_SCSI_DBG(KERN_DEBUG,
		fnic->lport->host,
		"Abort Cmd called FCID 0x%x, LUN 0x%llx TAG %x flags %x\n",
		rport->port_id, sc->device->lun, tag, fnic_priv(sc)->flags);

	fnic_priv(sc)->flags = FNIC_NO_FLAGS;

	if (lp->state != LPORT_ST_READY || !(lp->link_up)) {
		ret = FAILED;
		goto fnic_abort_cmd_end;
	}

	 
	io_lock = fnic_io_lock_hash(fnic, sc);
	spin_lock_irqsave(io_lock, flags);
	io_req = fnic_priv(sc)->io_req;
	if (!io_req) {
		spin_unlock_irqrestore(io_lock, flags);
		goto fnic_abort_cmd_end;
	}

	io_req->abts_done = &tm_done;

	if (fnic_priv(sc)->state == FNIC_IOREQ_ABTS_PENDING) {
		spin_unlock_irqrestore(io_lock, flags);
		goto wait_pending;
	}

	abt_issued_time = jiffies_to_msecs(jiffies) - jiffies_to_msecs(io_req->start_time);
	if (abt_issued_time <= 6000)
		atomic64_inc(&abts_stats->abort_issued_btw_0_to_6_sec);
	else if (abt_issued_time > 6000 && abt_issued_time <= 20000)
		atomic64_inc(&abts_stats->abort_issued_btw_6_to_20_sec);
	else if (abt_issued_time > 20000 && abt_issued_time <= 30000)
		atomic64_inc(&abts_stats->abort_issued_btw_20_to_30_sec);
	else if (abt_issued_time > 30000 && abt_issued_time <= 40000)
		atomic64_inc(&abts_stats->abort_issued_btw_30_to_40_sec);
	else if (abt_issued_time > 40000 && abt_issued_time <= 50000)
		atomic64_inc(&abts_stats->abort_issued_btw_40_to_50_sec);
	else if (abt_issued_time > 50000 && abt_issued_time <= 60000)
		atomic64_inc(&abts_stats->abort_issued_btw_50_to_60_sec);
	else
		atomic64_inc(&abts_stats->abort_issued_greater_than_60_sec);

	FNIC_SCSI_DBG(KERN_INFO, fnic->lport->host,
		"CBD Opcode: %02x Abort issued time: %lu msec\n", sc->cmnd[0], abt_issued_time);
	 
	old_ioreq_state = fnic_priv(sc)->state;
	fnic_priv(sc)->state = FNIC_IOREQ_ABTS_PENDING;
	fnic_priv(sc)->abts_status = FCPIO_INVALID_CODE;

	spin_unlock_irqrestore(io_lock, flags);

	 
	if (fc_remote_port_chkready(rport) == 0)
		task_req = FCPIO_ITMF_ABT_TASK;
	else {
		atomic64_inc(&fnic_stats->misc_stats.rport_not_ready);
		task_req = FCPIO_ITMF_ABT_TASK_TERM;
	}

	 
	int_to_scsilun(sc->device->lun, &fc_lun);

	if (fnic_queue_abort_io_req(fnic, tag, task_req, fc_lun.scsi_lun,
				    io_req)) {
		spin_lock_irqsave(io_lock, flags);
		if (fnic_priv(sc)->state == FNIC_IOREQ_ABTS_PENDING)
			fnic_priv(sc)->state = old_ioreq_state;
		io_req = fnic_priv(sc)->io_req;
		if (io_req)
			io_req->abts_done = NULL;
		spin_unlock_irqrestore(io_lock, flags);
		ret = FAILED;
		goto fnic_abort_cmd_end;
	}
	if (task_req == FCPIO_ITMF_ABT_TASK) {
		fnic_priv(sc)->flags |= FNIC_IO_ABTS_ISSUED;
		atomic64_inc(&fnic_stats->abts_stats.aborts);
	} else {
		fnic_priv(sc)->flags |= FNIC_IO_TERM_ISSUED;
		atomic64_inc(&fnic_stats->term_stats.terminates);
	}

	 
 wait_pending:
	wait_for_completion_timeout(&tm_done,
				    msecs_to_jiffies
				    (2 * fnic->config.ra_tov +
				     fnic->config.ed_tov));

	 
	spin_lock_irqsave(io_lock, flags);

	io_req = fnic_priv(sc)->io_req;
	if (!io_req) {
		atomic64_inc(&fnic_stats->io_stats.ioreq_null);
		spin_unlock_irqrestore(io_lock, flags);
		fnic_priv(sc)->flags |= FNIC_IO_ABT_TERM_REQ_NULL;
		ret = FAILED;
		goto fnic_abort_cmd_end;
	}
	io_req->abts_done = NULL;

	 
	if (fnic_priv(sc)->abts_status == FCPIO_INVALID_CODE) {
		spin_unlock_irqrestore(io_lock, flags);
		if (task_req == FCPIO_ITMF_ABT_TASK) {
			atomic64_inc(&abts_stats->abort_drv_timeouts);
		} else {
			atomic64_inc(&term_stats->terminate_drv_timeouts);
		}
		fnic_priv(sc)->flags |= FNIC_IO_ABT_TERM_TIMED_OUT;
		ret = FAILED;
		goto fnic_abort_cmd_end;
	}

	 

	if (!(fnic_priv(sc)->flags & (FNIC_IO_ABORTED | FNIC_IO_DONE))) {
		spin_unlock_irqrestore(io_lock, flags);
		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
			"Issuing Host reset due to out of order IO\n");

		ret = FAILED;
		goto fnic_abort_cmd_end;
	}

	fnic_priv(sc)->state = FNIC_IOREQ_ABTS_COMPLETE;

	start_time = io_req->start_time;
	 
	if (fnic_priv(sc)->abts_status == FCPIO_SUCCESS) {
		fnic_priv(sc)->io_req = NULL;
	} else {
		ret = FAILED;
		spin_unlock_irqrestore(io_lock, flags);
		goto fnic_abort_cmd_end;
	}

	spin_unlock_irqrestore(io_lock, flags);

	fnic_release_ioreq_buf(fnic, io_req, sc);
	mempool_free(io_req, fnic->io_req_pool);

	 
	sc->result = DID_ABORT << 16;
	scsi_done(sc);
	atomic64_dec(&fnic_stats->io_stats.active_ios);
	if (atomic64_read(&fnic->io_cmpl_skip))
		atomic64_dec(&fnic->io_cmpl_skip);
	else
		atomic64_inc(&fnic_stats->io_stats.io_completions);

fnic_abort_cmd_end:
	FNIC_TRACE(fnic_abort_cmd, sc->device->host->host_no, tag, sc,
		  jiffies_to_msecs(jiffies - start_time),
		  0, ((u64)sc->cmnd[0] << 32 |
		  (u64)sc->cmnd[2] << 24 | (u64)sc->cmnd[3] << 16 |
		  (u64)sc->cmnd[4] << 8 | sc->cmnd[5]),
		  fnic_flags_and_state(sc));

	FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
		      "Returning from abort cmd type %x %s\n", task_req,
		      (ret == SUCCESS) ?
		      "SUCCESS" : "FAILED");
	return ret;
}

static inline int fnic_queue_dr_io_req(struct fnic *fnic,
				       struct scsi_cmnd *sc,
				       struct fnic_io_req *io_req)
{
	struct vnic_wq_copy *wq = &fnic->wq_copy[0];
	struct Scsi_Host *host = fnic->lport->host;
	struct misc_stats *misc_stats = &fnic->fnic_stats.misc_stats;
	struct scsi_lun fc_lun;
	int ret = 0;
	unsigned long intr_flags;
	unsigned int tag = scsi_cmd_to_rq(sc)->tag;

	if (tag == SCSI_NO_TAG)
		tag = io_req->tag;

	spin_lock_irqsave(host->host_lock, intr_flags);
	if (unlikely(fnic_chk_state_flags_locked(fnic,
						FNIC_FLAGS_IO_BLOCKED))) {
		spin_unlock_irqrestore(host->host_lock, intr_flags);
		return FAILED;
	} else
		atomic_inc(&fnic->in_flight);
	spin_unlock_irqrestore(host->host_lock, intr_flags);

	spin_lock_irqsave(&fnic->wq_copy_lock[0], intr_flags);

	if (vnic_wq_copy_desc_avail(wq) <= fnic->wq_copy_desc_low[0])
		free_wq_copy_descs(fnic, wq);

	if (!vnic_wq_copy_desc_avail(wq)) {
		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
			  "queue_dr_io_req failure - no descriptors\n");
		atomic64_inc(&misc_stats->devrst_cpwq_alloc_failures);
		ret = -EAGAIN;
		goto lr_io_req_end;
	}

	 
	int_to_scsilun(sc->device->lun, &fc_lun);

	tag |= FNIC_TAG_DEV_RST;
	fnic_queue_wq_copy_desc_itmf(wq, tag,
				     0, FCPIO_ITMF_LUN_RESET, SCSI_NO_TAG,
				     fc_lun.scsi_lun, io_req->port_id,
				     fnic->config.ra_tov, fnic->config.ed_tov);

	atomic64_inc(&fnic->fnic_stats.fw_stats.active_fw_reqs);
	if (atomic64_read(&fnic->fnic_stats.fw_stats.active_fw_reqs) >
		  atomic64_read(&fnic->fnic_stats.fw_stats.max_fw_reqs))
		atomic64_set(&fnic->fnic_stats.fw_stats.max_fw_reqs,
		  atomic64_read(&fnic->fnic_stats.fw_stats.active_fw_reqs));

lr_io_req_end:
	spin_unlock_irqrestore(&fnic->wq_copy_lock[0], intr_flags);
	atomic_dec(&fnic->in_flight);

	return ret;
}

struct fnic_pending_aborts_iter_data {
	struct fnic *fnic;
	struct scsi_cmnd *lr_sc;
	struct scsi_device *lun_dev;
	int ret;
};

static bool fnic_pending_aborts_iter(struct scsi_cmnd *sc, void *data)
{
	struct fnic_pending_aborts_iter_data *iter_data = data;
	struct fnic *fnic = iter_data->fnic;
	struct scsi_device *lun_dev = iter_data->lun_dev;
	int abt_tag = scsi_cmd_to_rq(sc)->tag;
	struct fnic_io_req *io_req;
	spinlock_t *io_lock;
	unsigned long flags;
	struct scsi_lun fc_lun;
	DECLARE_COMPLETION_ONSTACK(tm_done);
	enum fnic_ioreq_state old_ioreq_state;

	if (sc == iter_data->lr_sc || sc->device != lun_dev)
		return true;

	io_lock = fnic_io_lock_tag(fnic, abt_tag);
	spin_lock_irqsave(io_lock, flags);
	io_req = fnic_priv(sc)->io_req;
	if (!io_req) {
		spin_unlock_irqrestore(io_lock, flags);
		return true;
	}

	 
	FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
		      "Found IO in %s on lun\n",
		      fnic_ioreq_state_to_str(fnic_priv(sc)->state));

	if (fnic_priv(sc)->state == FNIC_IOREQ_ABTS_PENDING) {
		spin_unlock_irqrestore(io_lock, flags);
		return true;
	}
	if ((fnic_priv(sc)->flags & FNIC_DEVICE_RESET) &&
	    (!(fnic_priv(sc)->flags & FNIC_DEV_RST_ISSUED))) {
		FNIC_SCSI_DBG(KERN_INFO, fnic->lport->host,
			      "%s dev rst not pending sc 0x%p\n", __func__,
			      sc);
		spin_unlock_irqrestore(io_lock, flags);
		return true;
	}

	if (io_req->abts_done)
		shost_printk(KERN_ERR, fnic->lport->host,
			     "%s: io_req->abts_done is set state is %s\n",
			     __func__, fnic_ioreq_state_to_str(fnic_priv(sc)->state));
	old_ioreq_state = fnic_priv(sc)->state;
	 
	fnic_priv(sc)->state = FNIC_IOREQ_ABTS_PENDING;

	BUG_ON(io_req->abts_done);

	if (fnic_priv(sc)->flags & FNIC_DEVICE_RESET) {
		abt_tag |= FNIC_TAG_DEV_RST;
		FNIC_SCSI_DBG(KERN_INFO, fnic->lport->host,
			      "%s: dev rst sc 0x%p\n", __func__, sc);
	}

	fnic_priv(sc)->abts_status = FCPIO_INVALID_CODE;
	io_req->abts_done = &tm_done;
	spin_unlock_irqrestore(io_lock, flags);

	 
	int_to_scsilun(sc->device->lun, &fc_lun);

	if (fnic_queue_abort_io_req(fnic, abt_tag,
				    FCPIO_ITMF_ABT_TASK_TERM,
				    fc_lun.scsi_lun, io_req)) {
		spin_lock_irqsave(io_lock, flags);
		io_req = fnic_priv(sc)->io_req;
		if (io_req)
			io_req->abts_done = NULL;
		if (fnic_priv(sc)->state == FNIC_IOREQ_ABTS_PENDING)
			fnic_priv(sc)->state = old_ioreq_state;
		spin_unlock_irqrestore(io_lock, flags);
		iter_data->ret = FAILED;
		return false;
	} else {
		spin_lock_irqsave(io_lock, flags);
		if (fnic_priv(sc)->flags & FNIC_DEVICE_RESET)
			fnic_priv(sc)->flags |= FNIC_DEV_RST_TERM_ISSUED;
		spin_unlock_irqrestore(io_lock, flags);
	}
	fnic_priv(sc)->flags |= FNIC_IO_INTERNAL_TERM_ISSUED;

	wait_for_completion_timeout(&tm_done, msecs_to_jiffies
				    (fnic->config.ed_tov));

	 
	spin_lock_irqsave(io_lock, flags);
	io_req = fnic_priv(sc)->io_req;
	if (!io_req) {
		spin_unlock_irqrestore(io_lock, flags);
		fnic_priv(sc)->flags |= FNIC_IO_ABT_TERM_REQ_NULL;
		return true;
	}

	io_req->abts_done = NULL;

	 
	if (fnic_priv(sc)->abts_status == FCPIO_INVALID_CODE) {
		spin_unlock_irqrestore(io_lock, flags);
		fnic_priv(sc)->flags |= FNIC_IO_ABT_TERM_DONE;
		iter_data->ret = FAILED;
		return false;
	}
	fnic_priv(sc)->state = FNIC_IOREQ_ABTS_COMPLETE;

	 
	if (sc != iter_data->lr_sc)
		fnic_priv(sc)->io_req = NULL;
	spin_unlock_irqrestore(io_lock, flags);

	 
	if (sc != iter_data->lr_sc) {
		fnic_release_ioreq_buf(fnic, io_req, sc);
		mempool_free(io_req, fnic->io_req_pool);
	}

	 
	 
	sc->result = DID_RESET << 16;
	scsi_done(sc);

	return true;
}

 
static int fnic_clean_pending_aborts(struct fnic *fnic,
				     struct scsi_cmnd *lr_sc,
				     bool new_sc)

{
	int ret = 0;
	struct fnic_pending_aborts_iter_data iter_data = {
		.fnic = fnic,
		.lun_dev = lr_sc->device,
		.ret = SUCCESS,
	};

	iter_data.lr_sc = lr_sc;

	scsi_host_busy_iter(fnic->lport->host,
			    fnic_pending_aborts_iter, &iter_data);
	if (iter_data.ret == FAILED) {
		ret = iter_data.ret;
		goto clean_pending_aborts_end;
	}
	schedule_timeout(msecs_to_jiffies(2 * fnic->config.ed_tov));

	 
	if (fnic_is_abts_pending(fnic, lr_sc))
		ret = 1;

clean_pending_aborts_end:
	FNIC_SCSI_DBG(KERN_INFO, fnic->lport->host,
			"%s: exit status: %d\n", __func__, ret);
	return ret;
}

 
int fnic_device_reset(struct scsi_cmnd *sc)
{
	struct request *rq = scsi_cmd_to_rq(sc);
	struct fc_lport *lp;
	struct fnic *fnic;
	struct fnic_io_req *io_req = NULL;
	struct fc_rport *rport;
	int status;
	int ret = FAILED;
	spinlock_t *io_lock;
	unsigned long flags;
	unsigned long start_time = 0;
	struct scsi_lun fc_lun;
	struct fnic_stats *fnic_stats;
	struct reset_stats *reset_stats;
	int tag = rq->tag;
	DECLARE_COMPLETION_ONSTACK(tm_done);
	bool new_sc = 0;

	 
	fc_block_scsi_eh(sc);

	 
	lp = shost_priv(sc->device->host);

	fnic = lport_priv(lp);
	fnic_stats = &fnic->fnic_stats;
	reset_stats = &fnic->fnic_stats.reset_stats;

	atomic64_inc(&reset_stats->device_resets);

	rport = starget_to_rport(scsi_target(sc->device));
	FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
		      "Device reset called FCID 0x%x, LUN 0x%llx sc 0x%p\n",
		      rport->port_id, sc->device->lun, sc);

	if (lp->state != LPORT_ST_READY || !(lp->link_up))
		goto fnic_device_reset_end;

	 
	if (fc_remote_port_chkready(rport)) {
		atomic64_inc(&fnic_stats->misc_stats.rport_not_ready);
		goto fnic_device_reset_end;
	}

	fnic_priv(sc)->flags = FNIC_DEVICE_RESET;

	if (unlikely(tag < 0)) {
		 
		mutex_lock(&fnic->sgreset_mutex);
		tag = fnic->fnic_max_tag_id;
		new_sc = 1;
		fnic->sgreset_sc = sc;
		io_lock = &fnic->sgreset_lock;
		FNIC_SCSI_DBG(KERN_INFO, fnic->lport->host,
			"fcid: 0x%x lun: 0x%llx flags: 0x%x tag: 0x%x Issuing sgreset\n",
			rport->port_id, sc->device->lun, fnic_priv(sc)->flags, tag);
	} else
		io_lock = fnic_io_lock_hash(fnic, sc);

	spin_lock_irqsave(io_lock, flags);
	io_req = fnic_priv(sc)->io_req;

	 
	if (!io_req) {
		io_req = mempool_alloc(fnic->io_req_pool, GFP_ATOMIC);
		if (!io_req) {
			spin_unlock_irqrestore(io_lock, flags);
			goto fnic_device_reset_end;
		}
		memset(io_req, 0, sizeof(*io_req));
		io_req->port_id = rport->port_id;
		io_req->tag = tag;
		io_req->sc = sc;
		fnic_priv(sc)->io_req = io_req;
	}
	io_req->dr_done = &tm_done;
	fnic_priv(sc)->state = FNIC_IOREQ_CMD_PENDING;
	fnic_priv(sc)->lr_status = FCPIO_INVALID_CODE;
	spin_unlock_irqrestore(io_lock, flags);

	FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host, "TAG %x\n", tag);

	 
	if (fnic_queue_dr_io_req(fnic, sc, io_req)) {
		spin_lock_irqsave(io_lock, flags);
		io_req = fnic_priv(sc)->io_req;
		if (io_req)
			io_req->dr_done = NULL;
		goto fnic_device_reset_clean;
	}
	spin_lock_irqsave(io_lock, flags);
	fnic_priv(sc)->flags |= FNIC_DEV_RST_ISSUED;
	spin_unlock_irqrestore(io_lock, flags);

	 
	wait_for_completion_timeout(&tm_done,
				    msecs_to_jiffies(FNIC_LUN_RESET_TIMEOUT));

	spin_lock_irqsave(io_lock, flags);
	io_req = fnic_priv(sc)->io_req;
	if (!io_req) {
		spin_unlock_irqrestore(io_lock, flags);
		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
				"io_req is null tag 0x%x sc 0x%p\n", tag, sc);
		goto fnic_device_reset_end;
	}
	io_req->dr_done = NULL;

	status = fnic_priv(sc)->lr_status;

	 
	if (status == FCPIO_INVALID_CODE) {
		atomic64_inc(&reset_stats->device_reset_timeouts);
		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
			      "Device reset timed out\n");
		fnic_priv(sc)->flags |= FNIC_DEV_RST_TIMED_OUT;
		spin_unlock_irqrestore(io_lock, flags);
		int_to_scsilun(sc->device->lun, &fc_lun);
		 
		while (1) {
			spin_lock_irqsave(io_lock, flags);
			if (fnic_priv(sc)->flags & FNIC_DEV_RST_TERM_ISSUED) {
				spin_unlock_irqrestore(io_lock, flags);
				break;
			}
			spin_unlock_irqrestore(io_lock, flags);
			if (fnic_queue_abort_io_req(fnic,
				tag | FNIC_TAG_DEV_RST,
				FCPIO_ITMF_ABT_TASK_TERM,
				fc_lun.scsi_lun, io_req)) {
				wait_for_completion_timeout(&tm_done,
				msecs_to_jiffies(FNIC_ABT_TERM_DELAY_TIMEOUT));
			} else {
				spin_lock_irqsave(io_lock, flags);
				fnic_priv(sc)->flags |= FNIC_DEV_RST_TERM_ISSUED;
				fnic_priv(sc)->state = FNIC_IOREQ_ABTS_PENDING;
				io_req->abts_done = &tm_done;
				spin_unlock_irqrestore(io_lock, flags);
				FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
				"Abort and terminate issued on Device reset "
				"tag 0x%x sc 0x%p\n", tag, sc);
				break;
			}
		}
		while (1) {
			spin_lock_irqsave(io_lock, flags);
			if (!(fnic_priv(sc)->flags & FNIC_DEV_RST_DONE)) {
				spin_unlock_irqrestore(io_lock, flags);
				wait_for_completion_timeout(&tm_done,
				msecs_to_jiffies(FNIC_LUN_RESET_TIMEOUT));
				break;
			} else {
				io_req = fnic_priv(sc)->io_req;
				io_req->abts_done = NULL;
				goto fnic_device_reset_clean;
			}
		}
	} else {
		spin_unlock_irqrestore(io_lock, flags);
	}

	 
	if (status != FCPIO_SUCCESS) {
		spin_lock_irqsave(io_lock, flags);
		FNIC_SCSI_DBG(KERN_DEBUG,
			      fnic->lport->host,
			      "Device reset completed - failed\n");
		io_req = fnic_priv(sc)->io_req;
		goto fnic_device_reset_clean;
	}

	 
	if (fnic_clean_pending_aborts(fnic, sc, new_sc)) {
		spin_lock_irqsave(io_lock, flags);
		io_req = fnic_priv(sc)->io_req;
		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
			      "Device reset failed"
			      " since could not abort all IOs\n");
		goto fnic_device_reset_clean;
	}

	 
	spin_lock_irqsave(io_lock, flags);
	io_req = fnic_priv(sc)->io_req;
	if (io_req)
		 
		ret = SUCCESS;

fnic_device_reset_clean:
	if (io_req)
		fnic_priv(sc)->io_req = NULL;

	spin_unlock_irqrestore(io_lock, flags);

	if (io_req) {
		start_time = io_req->start_time;
		fnic_release_ioreq_buf(fnic, io_req, sc);
		mempool_free(io_req, fnic->io_req_pool);
	}

fnic_device_reset_end:
	FNIC_TRACE(fnic_device_reset, sc->device->host->host_no, rq->tag, sc,
		  jiffies_to_msecs(jiffies - start_time),
		  0, ((u64)sc->cmnd[0] << 32 |
		  (u64)sc->cmnd[2] << 24 | (u64)sc->cmnd[3] << 16 |
		  (u64)sc->cmnd[4] << 8 | sc->cmnd[5]),
		  fnic_flags_and_state(sc));

	if (new_sc) {
		fnic->sgreset_sc = NULL;
		mutex_unlock(&fnic->sgreset_mutex);
	}

	FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
		      "Returning from device reset %s\n",
		      (ret == SUCCESS) ?
		      "SUCCESS" : "FAILED");

	if (ret == FAILED)
		atomic64_inc(&reset_stats->device_reset_failures);

	return ret;
}

 
int fnic_reset(struct Scsi_Host *shost)
{
	struct fc_lport *lp;
	struct fnic *fnic;
	int ret = 0;
	struct reset_stats *reset_stats;

	lp = shost_priv(shost);
	fnic = lport_priv(lp);
	reset_stats = &fnic->fnic_stats.reset_stats;

	FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
		      "fnic_reset called\n");

	atomic64_inc(&reset_stats->fnic_resets);

	 
	ret = fc_lport_reset(lp);

	FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
		      "Returning from fnic reset %s\n",
		      (ret == 0) ?
		      "SUCCESS" : "FAILED");

	if (ret == 0)
		atomic64_inc(&reset_stats->fnic_reset_completions);
	else
		atomic64_inc(&reset_stats->fnic_reset_failures);

	return ret;
}

 
int fnic_host_reset(struct scsi_cmnd *sc)
{
	int ret;
	unsigned long wait_host_tmo;
	struct Scsi_Host *shost = sc->device->host;
	struct fc_lport *lp = shost_priv(shost);
	struct fnic *fnic = lport_priv(lp);
	unsigned long flags;

	spin_lock_irqsave(&fnic->fnic_lock, flags);
	if (!fnic->internal_reset_inprogress) {
		fnic->internal_reset_inprogress = true;
	} else {
		spin_unlock_irqrestore(&fnic->fnic_lock, flags);
		FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
			"host reset in progress skipping another host reset\n");
		return SUCCESS;
	}
	spin_unlock_irqrestore(&fnic->fnic_lock, flags);

	 
	ret = (fnic_reset(shost) == 0) ? SUCCESS : FAILED;
	if (ret == SUCCESS) {
		wait_host_tmo = jiffies + FNIC_HOST_RESET_SETTLE_TIME * HZ;
		ret = FAILED;
		while (time_before(jiffies, wait_host_tmo)) {
			if ((lp->state == LPORT_ST_READY) &&
			    (lp->link_up)) {
				ret = SUCCESS;
				break;
			}
			ssleep(1);
		}
	}

	spin_lock_irqsave(&fnic->fnic_lock, flags);
	fnic->internal_reset_inprogress = false;
	spin_unlock_irqrestore(&fnic->fnic_lock, flags);
	return ret;
}

 
void fnic_scsi_abort_io(struct fc_lport *lp)
{
	int err = 0;
	unsigned long flags;
	enum fnic_state old_state;
	struct fnic *fnic = lport_priv(lp);
	DECLARE_COMPLETION_ONSTACK(remove_wait);

	 
retry_fw_reset:
	spin_lock_irqsave(&fnic->fnic_lock, flags);
	if (unlikely(fnic->state == FNIC_IN_FC_TRANS_ETH_MODE) &&
		     fnic->link_events) {
		 
		spin_unlock_irqrestore(&fnic->fnic_lock, flags);
		schedule_timeout(msecs_to_jiffies(100));
		goto retry_fw_reset;
	}

	fnic->remove_wait = &remove_wait;
	old_state = fnic->state;
	fnic->state = FNIC_IN_FC_TRANS_ETH_MODE;
	fnic_update_mac_locked(fnic, fnic->ctlr.ctl_src_addr);
	spin_unlock_irqrestore(&fnic->fnic_lock, flags);

	err = fnic_fw_reset_handler(fnic);
	if (err) {
		spin_lock_irqsave(&fnic->fnic_lock, flags);
		if (fnic->state == FNIC_IN_FC_TRANS_ETH_MODE)
			fnic->state = old_state;
		fnic->remove_wait = NULL;
		spin_unlock_irqrestore(&fnic->fnic_lock, flags);
		return;
	}

	 
	wait_for_completion_timeout(&remove_wait,
				    msecs_to_jiffies(FNIC_RMDEVICE_TIMEOUT));

	spin_lock_irqsave(&fnic->fnic_lock, flags);
	fnic->remove_wait = NULL;
	FNIC_SCSI_DBG(KERN_DEBUG, fnic->lport->host,
		      "fnic_scsi_abort_io %s\n",
		      (fnic->state == FNIC_IN_ETH_MODE) ?
		      "SUCCESS" : "FAILED");
	spin_unlock_irqrestore(&fnic->fnic_lock, flags);

}

 
void fnic_scsi_cleanup(struct fc_lport *lp)
{
	unsigned long flags;
	enum fnic_state old_state;
	struct fnic *fnic = lport_priv(lp);

	 
retry_fw_reset:
	spin_lock_irqsave(&fnic->fnic_lock, flags);
	if (unlikely(fnic->state == FNIC_IN_FC_TRANS_ETH_MODE)) {
		 
		spin_unlock_irqrestore(&fnic->fnic_lock, flags);
		schedule_timeout(msecs_to_jiffies(100));
		goto retry_fw_reset;
	}
	old_state = fnic->state;
	fnic->state = FNIC_IN_FC_TRANS_ETH_MODE;
	fnic_update_mac_locked(fnic, fnic->ctlr.ctl_src_addr);
	spin_unlock_irqrestore(&fnic->fnic_lock, flags);

	if (fnic_fw_reset_handler(fnic)) {
		spin_lock_irqsave(&fnic->fnic_lock, flags);
		if (fnic->state == FNIC_IN_FC_TRANS_ETH_MODE)
			fnic->state = old_state;
		spin_unlock_irqrestore(&fnic->fnic_lock, flags);
	}

}

void fnic_empty_scsi_cleanup(struct fc_lport *lp)
{
}

void fnic_exch_mgr_reset(struct fc_lport *lp, u32 sid, u32 did)
{
	struct fnic *fnic = lport_priv(lp);

	 
	if (sid)
		goto call_fc_exch_mgr_reset;

	if (did) {
		fnic_rport_exch_reset(fnic, did);
		goto call_fc_exch_mgr_reset;
	}

	 
	if (!fnic->in_remove)
		fnic_scsi_cleanup(lp);
	else
		fnic_scsi_abort_io(lp);

	 
call_fc_exch_mgr_reset:
	fc_exch_mgr_reset(lp, sid, did);

}

static bool fnic_abts_pending_iter(struct scsi_cmnd *sc, void *data)
{
	struct fnic_pending_aborts_iter_data *iter_data = data;
	struct fnic *fnic = iter_data->fnic;
	int cmd_state;
	struct fnic_io_req *io_req;
	spinlock_t *io_lock;
	unsigned long flags;

	 
	if (iter_data->lr_sc && sc == iter_data->lr_sc)
		return true;
	if (iter_data->lun_dev && sc->device != iter_data->lun_dev)
		return true;

	io_lock = fnic_io_lock_hash(fnic, sc);
	spin_lock_irqsave(io_lock, flags);

	io_req = fnic_priv(sc)->io_req;
	if (!io_req) {
		spin_unlock_irqrestore(io_lock, flags);
		return true;
	}

	 
	FNIC_SCSI_DBG(KERN_INFO, fnic->lport->host,
		      "Found IO in %s on lun\n",
		      fnic_ioreq_state_to_str(fnic_priv(sc)->state));
	cmd_state = fnic_priv(sc)->state;
	spin_unlock_irqrestore(io_lock, flags);
	if (cmd_state == FNIC_IOREQ_ABTS_PENDING)
		iter_data->ret = 1;

	return iter_data->ret ? false : true;
}

 
int fnic_is_abts_pending(struct fnic *fnic, struct scsi_cmnd *lr_sc)
{
	struct fnic_pending_aborts_iter_data iter_data = {
		.fnic = fnic,
		.lun_dev = NULL,
		.ret = 0,
	};

	if (lr_sc) {
		iter_data.lun_dev = lr_sc->device;
		iter_data.lr_sc = lr_sc;
	}

	 
	scsi_host_busy_iter(fnic->lport->host,
			    fnic_abts_pending_iter, &iter_data);

	return iter_data.ret;
}
