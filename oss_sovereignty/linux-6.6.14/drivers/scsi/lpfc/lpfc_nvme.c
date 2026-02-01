 
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/unaligned.h>
#include <linux/crc-t10dif.h>
#include <net/checksum.h>

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_transport_fc.h>
#include <scsi/fc/fc_fs.h>

#include "lpfc_version.h"
#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_nl.h"
#include "lpfc_disc.h"
#include "lpfc.h"
#include "lpfc_nvme.h"
#include "lpfc_scsi.h"
#include "lpfc_logmsg.h"
#include "lpfc_crtn.h"
#include "lpfc_vport.h"
#include "lpfc_debugfs.h"

 

static struct lpfc_io_buf *
lpfc_get_nvme_buf(struct lpfc_hba *phba, struct lpfc_nodelist *ndlp,
		  int idx, int expedite);

static void
lpfc_release_nvme_buf(struct lpfc_hba *, struct lpfc_io_buf *);

static struct nvme_fc_port_template lpfc_nvme_template;

 
static int
lpfc_nvme_create_queue(struct nvme_fc_local_port *pnvme_lport,
		       unsigned int qidx, u16 qsize,
		       void **handle)
{
	struct lpfc_nvme_lport *lport;
	struct lpfc_vport *vport;
	struct lpfc_nvme_qhandle *qhandle;
	char *str;

	if (!pnvme_lport->private)
		return -ENOMEM;

	lport = (struct lpfc_nvme_lport *)pnvme_lport->private;
	vport = lport->vport;

	if (!vport || vport->load_flag & FC_UNLOADING ||
	    vport->phba->hba_flag & HBA_IOQ_FLUSH)
		return -ENODEV;

	qhandle = kzalloc(sizeof(struct lpfc_nvme_qhandle), GFP_KERNEL);
	if (qhandle == NULL)
		return -ENOMEM;

	qhandle->cpu_id = raw_smp_processor_id();
	qhandle->qidx = qidx;
	 
	if (qidx) {
		str = "IO ";   
		qhandle->index = ((qidx - 1) %
			lpfc_nvme_template.max_hw_queues);
	} else {
		str = "ADM";   
		qhandle->index = qidx;
	}

	lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME,
			 "6073 Binding %s HdwQueue %d  (cpu %d) to "
			 "hdw_queue %d qhandle x%px\n", str,
			 qidx, qhandle->cpu_id, qhandle->index, qhandle);
	*handle = (void *)qhandle;
	return 0;
}

 
static void
lpfc_nvme_delete_queue(struct nvme_fc_local_port *pnvme_lport,
		       unsigned int qidx,
		       void *handle)
{
	struct lpfc_nvme_lport *lport;
	struct lpfc_vport *vport;

	if (!pnvme_lport->private)
		return;

	lport = (struct lpfc_nvme_lport *)pnvme_lport->private;
	vport = lport->vport;

	lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME,
			"6001 ENTER.  lpfc_pnvme x%px, qidx x%x qhandle x%px\n",
			lport, qidx, handle);
	kfree(handle);
}

static void
lpfc_nvme_localport_delete(struct nvme_fc_local_port *localport)
{
	struct lpfc_nvme_lport *lport = localport->private;

	lpfc_printf_vlog(lport->vport, KERN_INFO, LOG_NVME,
			 "6173 localport x%px delete complete\n",
			 lport);

	 
	if (lport->vport->localport)
		complete(lport->lport_unreg_cmp);
}

 
static void
lpfc_nvme_remoteport_delete(struct nvme_fc_remote_port *remoteport)
{
	struct lpfc_nvme_rport *rport = remoteport->private;
	struct lpfc_vport *vport;
	struct lpfc_nodelist *ndlp;
	u32 fc4_xpt_flags;

	ndlp = rport->ndlp;
	if (!ndlp) {
		pr_err("**** %s: NULL ndlp on rport x%px remoteport x%px\n",
		       __func__, rport, remoteport);
		goto rport_err;
	}

	vport = ndlp->vport;
	if (!vport) {
		pr_err("**** %s: Null vport on ndlp x%px, ste x%x rport x%px\n",
		       __func__, ndlp, ndlp->nlp_state, rport);
		goto rport_err;
	}

	fc4_xpt_flags = NVME_XPT_REGD | SCSI_XPT_REGD;

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_DISC,
			 "6146 remoteport delete of remoteport x%px, ndlp x%px "
			 "DID x%x xflags x%x\n",
			 remoteport, ndlp, ndlp->nlp_DID, ndlp->fc4_xpt_flags);
	spin_lock_irq(&ndlp->lock);

	 
	if (ndlp->fc4_xpt_flags & NVME_XPT_UNREG_WAIT)
		ndlp->fc4_xpt_flags &= ~(NVME_XPT_UNREG_WAIT | NVME_XPT_REGD);

	spin_unlock_irq(&ndlp->lock);

	 
	if (!(ndlp->fc4_xpt_flags & fc4_xpt_flags))
		lpfc_disc_state_machine(vport, ndlp, NULL, NLP_EVT_DEVICE_RM);

 rport_err:
	return;
}

 
int
lpfc_nvme_handle_lsreq(struct lpfc_hba *phba,
			struct lpfc_async_xchg_ctx *axchg)
{
#if (IS_ENABLED(CONFIG_NVME_FC))
	struct lpfc_vport *vport;
	struct lpfc_nvme_rport *lpfc_rport;
	struct nvme_fc_remote_port *remoteport;
	struct lpfc_nvme_lport *lport;
	uint32_t *payload = axchg->payload;
	int rc;

	vport = axchg->ndlp->vport;
	lpfc_rport = axchg->ndlp->nrport;
	if (!lpfc_rport)
		return -EINVAL;

	remoteport = lpfc_rport->remoteport;
	if (!vport->localport ||
	    vport->phba->hba_flag & HBA_IOQ_FLUSH)
		return -EINVAL;

	lport = vport->localport->private;
	if (!lport)
		return -EINVAL;

	rc = nvme_fc_rcv_ls_req(remoteport, &axchg->ls_rsp, axchg->payload,
				axchg->size);

	lpfc_printf_log(phba, KERN_INFO, LOG_NVME_DISC,
			"6205 NVME Unsol rcv: sz %d rc %d: %08x %08x %08x "
			"%08x %08x %08x\n",
			axchg->size, rc,
			*payload, *(payload+1), *(payload+2),
			*(payload+3), *(payload+4), *(payload+5));

	if (!rc)
		return 0;
#endif
	return 1;
}

 
void
__lpfc_nvme_ls_req_cmp(struct lpfc_hba *phba, struct lpfc_vport *vport,
			struct lpfc_iocbq *cmdwqe,
			struct lpfc_wcqe_complete *wcqe)
{
	struct nvmefc_ls_req *pnvme_lsreq;
	struct lpfc_dmabuf *buf_ptr;
	struct lpfc_nodelist *ndlp;
	int status;

	pnvme_lsreq = cmdwqe->context_un.nvme_lsreq;
	ndlp = cmdwqe->ndlp;
	buf_ptr = cmdwqe->bpl_dmabuf;

	status = bf_get(lpfc_wcqe_c_status, wcqe);

	lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_DISC,
			 "6047 NVMEx LS REQ x%px cmpl DID %x Xri: %x "
			 "status %x reason x%x cmd:x%px lsreg:x%px bmp:x%px "
			 "ndlp:x%px\n",
			 pnvme_lsreq, ndlp ? ndlp->nlp_DID : 0,
			 cmdwqe->sli4_xritag, status,
			 (wcqe->parameter & 0xffff),
			 cmdwqe, pnvme_lsreq, cmdwqe->bpl_dmabuf,
			 ndlp);

	lpfc_nvmeio_data(phba, "NVMEx LS CMPL: xri x%x stat x%x parm x%x\n",
			 cmdwqe->sli4_xritag, status, wcqe->parameter);

	if (buf_ptr) {
		lpfc_mbuf_free(phba, buf_ptr->virt, buf_ptr->phys);
		kfree(buf_ptr);
		cmdwqe->bpl_dmabuf = NULL;
	}
	if (pnvme_lsreq->done) {
		if (status != CQE_STATUS_SUCCESS)
			status = -ENXIO;
		pnvme_lsreq->done(pnvme_lsreq, status);
	} else {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "6046 NVMEx cmpl without done call back? "
				 "Data x%px DID %x Xri: %x status %x\n",
				pnvme_lsreq, ndlp ? ndlp->nlp_DID : 0,
				cmdwqe->sli4_xritag, status);
	}
	if (ndlp) {
		lpfc_nlp_put(ndlp);
		cmdwqe->ndlp = NULL;
	}
	lpfc_sli_release_iocbq(phba, cmdwqe);
}

static void
lpfc_nvme_ls_req_cmp(struct lpfc_hba *phba, struct lpfc_iocbq *cmdwqe,
		     struct lpfc_iocbq *rspwqe)
{
	struct lpfc_vport *vport = cmdwqe->vport;
	struct lpfc_nvme_lport *lport;
	uint32_t status;
	struct lpfc_wcqe_complete *wcqe = &rspwqe->wcqe_cmpl;

	status = bf_get(lpfc_wcqe_c_status, wcqe);

	if (vport->localport) {
		lport = (struct lpfc_nvme_lport *)vport->localport->private;
		if (lport) {
			atomic_inc(&lport->fc4NvmeLsCmpls);
			if (status) {
				if (bf_get(lpfc_wcqe_c_xb, wcqe))
					atomic_inc(&lport->cmpl_ls_xb);
				atomic_inc(&lport->cmpl_ls_err);
			}
		}
	}

	__lpfc_nvme_ls_req_cmp(phba, vport, cmdwqe, wcqe);
}

static int
lpfc_nvme_gen_req(struct lpfc_vport *vport, struct lpfc_dmabuf *bmp,
		  struct lpfc_dmabuf *inp,
		  struct nvmefc_ls_req *pnvme_lsreq,
		  void (*cmpl)(struct lpfc_hba *, struct lpfc_iocbq *,
			       struct lpfc_iocbq *),
		  struct lpfc_nodelist *ndlp, uint32_t num_entry,
		  uint32_t tmo, uint8_t retry)
{
	struct lpfc_hba *phba = vport->phba;
	union lpfc_wqe128 *wqe;
	struct lpfc_iocbq *genwqe;
	struct ulp_bde64 *bpl;
	struct ulp_bde64 bde;
	int i, rc, xmit_len, first_len;

	 
	genwqe = lpfc_sli_get_iocbq(phba);
	if (genwqe == NULL)
		return 1;

	wqe = &genwqe->wqe;
	 
	memset(wqe, 0, sizeof(union lpfc_wqe));

	genwqe->bpl_dmabuf = bmp;
	genwqe->cmd_flag |= LPFC_IO_NVME_LS;

	 
	genwqe->ndlp = lpfc_nlp_get(ndlp);
	if (!genwqe->ndlp) {
		dev_warn(&phba->pcidev->dev,
			 "Warning: Failed node ref, not sending LS_REQ\n");
		lpfc_sli_release_iocbq(phba, genwqe);
		return 1;
	}

	genwqe->context_un.nvme_lsreq = pnvme_lsreq;
	 

	if (!tmo)
		 
		tmo = (3 * phba->fc_ratov);

	 
	xmit_len = 0;
	first_len = 0;
	bpl = (struct ulp_bde64 *)bmp->virt;
	for (i = 0; i < num_entry; i++) {
		bde.tus.w = bpl[i].tus.w;
		if (bde.tus.f.bdeFlags != BUFF_TYPE_BDE_64)
			break;
		xmit_len += bde.tus.f.bdeSize;
		if (i == 0)
			first_len = xmit_len;
	}

	genwqe->num_bdes = num_entry;
	genwqe->hba_wqidx = 0;

	 
	wqe->generic.bde.tus.f.bdeFlags = BUFF_TYPE_BDE_64;
	wqe->generic.bde.tus.f.bdeSize = first_len;
	wqe->generic.bde.addrLow = bpl[0].addrLow;
	wqe->generic.bde.addrHigh = bpl[0].addrHigh;

	 
	wqe->gen_req.request_payload_len = first_len;

	 

	 
	bf_set(wqe_dfctl, &wqe->gen_req.wge_ctl, 0);
	bf_set(wqe_si, &wqe->gen_req.wge_ctl, 1);
	bf_set(wqe_la, &wqe->gen_req.wge_ctl, 1);
	bf_set(wqe_rctl, &wqe->gen_req.wge_ctl, FC_RCTL_ELS4_REQ);
	bf_set(wqe_type, &wqe->gen_req.wge_ctl, FC_TYPE_NVME);

	 
	bf_set(wqe_ctxt_tag, &wqe->gen_req.wqe_com,
	       phba->sli4_hba.rpi_ids[ndlp->nlp_rpi]);
	bf_set(wqe_xri_tag, &wqe->gen_req.wqe_com, genwqe->sli4_xritag);

	 
	bf_set(wqe_tmo, &wqe->gen_req.wqe_com, tmo);
	bf_set(wqe_class, &wqe->gen_req.wqe_com, CLASS3);
	bf_set(wqe_cmnd, &wqe->gen_req.wqe_com, CMD_GEN_REQUEST64_WQE);
	bf_set(wqe_ct, &wqe->gen_req.wqe_com, SLI4_CT_RPI);

	 
	wqe->gen_req.wqe_com.abort_tag = genwqe->iotag;

	 
	bf_set(wqe_reqtag, &wqe->gen_req.wqe_com, genwqe->iotag);

	 
	bf_set(wqe_dbde, &wqe->gen_req.wqe_com, 1);
	bf_set(wqe_iod, &wqe->gen_req.wqe_com, LPFC_WQE_IOD_READ);
	bf_set(wqe_qosd, &wqe->gen_req.wqe_com, 1);
	bf_set(wqe_lenloc, &wqe->gen_req.wqe_com, LPFC_WQE_LENLOC_NONE);
	bf_set(wqe_ebde_cnt, &wqe->gen_req.wqe_com, 0);

	 
	bf_set(wqe_cqid, &wqe->gen_req.wqe_com, LPFC_WQE_CQ_ID_DEFAULT);
	bf_set(wqe_cmd_type, &wqe->gen_req.wqe_com, OTHER_COMMAND);


	 
	genwqe->cmd_cmpl = cmpl;
	genwqe->drvrTimeout = tmo + LPFC_DRVR_TIMEOUT;
	genwqe->vport = vport;
	genwqe->retry = retry;

	lpfc_nvmeio_data(phba, "NVME LS  XMIT: xri x%x iotag x%x to x%06x\n",
			 genwqe->sli4_xritag, genwqe->iotag, ndlp->nlp_DID);

	rc = lpfc_sli4_issue_wqe(phba, &phba->sli4_hba.hdwq[0], genwqe);
	if (rc) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "6045 Issue GEN REQ WQE to NPORT x%x "
				 "Data: x%x x%x  rc x%x\n",
				 ndlp->nlp_DID, genwqe->iotag,
				 vport->port_state, rc);
		lpfc_nlp_put(ndlp);
		lpfc_sli_release_iocbq(phba, genwqe);
		return 1;
	}

	lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_DISC | LOG_ELS,
			 "6050 Issue GEN REQ WQE to NPORT x%x "
			 "Data: oxid: x%x state: x%x wq:x%px lsreq:x%px "
			 "bmp:x%px xmit:%d 1st:%d\n",
			 ndlp->nlp_DID, genwqe->sli4_xritag,
			 vport->port_state,
			 genwqe, pnvme_lsreq, bmp, xmit_len, first_len);
	return 0;
}


 
int
__lpfc_nvme_ls_req(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		      struct nvmefc_ls_req *pnvme_lsreq,
		      void (*gen_req_cmp)(struct lpfc_hba *phba,
				struct lpfc_iocbq *cmdwqe,
				struct lpfc_iocbq *rspwqe))
{
	struct lpfc_dmabuf *bmp;
	struct ulp_bde64 *bpl;
	int ret;
	uint16_t ntype, nstate;

	if (!ndlp) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "6051 NVMEx LS REQ: Bad NDLP x%px, Failing "
				 "LS Req\n",
				 ndlp);
		return -ENODEV;
	}

	ntype = ndlp->nlp_type;
	nstate = ndlp->nlp_state;
	if ((ntype & NLP_NVME_TARGET && nstate != NLP_STE_MAPPED_NODE) ||
	    (ntype & NLP_NVME_INITIATOR && nstate != NLP_STE_UNMAPPED_NODE)) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "6088 NVMEx LS REQ: Fail DID x%06x not "
				 "ready for IO. Type x%x, State x%x\n",
				 ndlp->nlp_DID, ntype, nstate);
		return -ENODEV;
	}
	if (vport->phba->hba_flag & HBA_IOQ_FLUSH)
		return -ENODEV;

	if (!vport->phba->sli4_hba.nvmels_wq)
		return -ENOMEM;

	 

	bmp = kmalloc(sizeof(*bmp), GFP_KERNEL);
	if (!bmp) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "6044 NVMEx LS REQ: Could not alloc LS buf "
				 "for DID %x\n",
				 ndlp->nlp_DID);
		return -ENOMEM;
	}

	bmp->virt = lpfc_mbuf_alloc(vport->phba, MEM_PRI, &(bmp->phys));
	if (!bmp->virt) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "6042 NVMEx LS REQ: Could not alloc mbuf "
				 "for DID %x\n",
				 ndlp->nlp_DID);
		kfree(bmp);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&bmp->list);

	bpl = (struct ulp_bde64 *)bmp->virt;
	bpl->addrHigh = le32_to_cpu(putPaddrHigh(pnvme_lsreq->rqstdma));
	bpl->addrLow = le32_to_cpu(putPaddrLow(pnvme_lsreq->rqstdma));
	bpl->tus.f.bdeFlags = 0;
	bpl->tus.f.bdeSize = pnvme_lsreq->rqstlen;
	bpl->tus.w = le32_to_cpu(bpl->tus.w);
	bpl++;

	bpl->addrHigh = le32_to_cpu(putPaddrHigh(pnvme_lsreq->rspdma));
	bpl->addrLow = le32_to_cpu(putPaddrLow(pnvme_lsreq->rspdma));
	bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_64I;
	bpl->tus.f.bdeSize = pnvme_lsreq->rsplen;
	bpl->tus.w = le32_to_cpu(bpl->tus.w);

	lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_DISC,
			"6149 NVMEx LS REQ: Issue to DID 0x%06x lsreq x%px, "
			"rqstlen:%d rsplen:%d %pad %pad\n",
			ndlp->nlp_DID, pnvme_lsreq, pnvme_lsreq->rqstlen,
			pnvme_lsreq->rsplen, &pnvme_lsreq->rqstdma,
			&pnvme_lsreq->rspdma);

	ret = lpfc_nvme_gen_req(vport, bmp, pnvme_lsreq->rqstaddr,
				pnvme_lsreq, gen_req_cmp, ndlp, 2,
				pnvme_lsreq->timeout, 0);
	if (ret != WQE_SUCCESS) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "6052 NVMEx REQ: EXIT. issue ls wqe failed "
				 "lsreq x%px Status %x DID %x\n",
				 pnvme_lsreq, ret, ndlp->nlp_DID);
		lpfc_mbuf_free(vport->phba, bmp->virt, bmp->phys);
		kfree(bmp);
		return -EIO;
	}

	return 0;
}

 
static int
lpfc_nvme_ls_req(struct nvme_fc_local_port *pnvme_lport,
		 struct nvme_fc_remote_port *pnvme_rport,
		 struct nvmefc_ls_req *pnvme_lsreq)
{
	struct lpfc_nvme_lport *lport;
	struct lpfc_nvme_rport *rport;
	struct lpfc_vport *vport;
	int ret;

	lport = (struct lpfc_nvme_lport *)pnvme_lport->private;
	rport = (struct lpfc_nvme_rport *)pnvme_rport->private;
	if (unlikely(!lport) || unlikely(!rport))
		return -EINVAL;

	vport = lport->vport;
	if (vport->load_flag & FC_UNLOADING ||
	    vport->phba->hba_flag & HBA_IOQ_FLUSH)
		return -ENODEV;

	atomic_inc(&lport->fc4NvmeLsRequests);

	ret = __lpfc_nvme_ls_req(vport, rport->ndlp, pnvme_lsreq,
				 lpfc_nvme_ls_req_cmp);
	if (ret)
		atomic_inc(&lport->xmt_ls_err);

	return ret;
}

 
int
__lpfc_nvme_ls_abort(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
			struct nvmefc_ls_req *pnvme_lsreq)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_sli_ring *pring;
	struct lpfc_iocbq *wqe, *next_wqe;
	bool foundit = false;

	if (!ndlp) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				"6049 NVMEx LS REQ Abort: Bad NDLP x%px DID "
				"x%06x, Failing LS Req\n",
				ndlp, ndlp ? ndlp->nlp_DID : 0);
		return -EINVAL;
	}

	lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_DISC | LOG_NVME_ABTS,
			 "6040 NVMEx LS REQ Abort: Issue LS_ABORT for lsreq "
			 "x%px rqstlen:%d rsplen:%d %pad %pad\n",
			 pnvme_lsreq, pnvme_lsreq->rqstlen,
			 pnvme_lsreq->rsplen, &pnvme_lsreq->rqstdma,
			 &pnvme_lsreq->rspdma);

	 
	pring = phba->sli4_hba.nvmels_wq->pring;
	spin_lock_irq(&phba->hbalock);
	spin_lock(&pring->ring_lock);
	list_for_each_entry_safe(wqe, next_wqe, &pring->txcmplq, list) {
		if (wqe->context_un.nvme_lsreq == pnvme_lsreq) {
			wqe->cmd_flag |= LPFC_DRIVER_ABORTED;
			foundit = true;
			break;
		}
	}
	spin_unlock(&pring->ring_lock);

	if (foundit)
		lpfc_sli_issue_abort_iotag(phba, pring, wqe, NULL);
	spin_unlock_irq(&phba->hbalock);

	if (foundit)
		return 0;

	lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_DISC | LOG_NVME_ABTS,
			 "6213 NVMEx LS REQ Abort: Unable to locate req x%px\n",
			 pnvme_lsreq);
	return -EINVAL;
}

static int
lpfc_nvme_xmt_ls_rsp(struct nvme_fc_local_port *localport,
		     struct nvme_fc_remote_port *remoteport,
		     struct nvmefc_ls_rsp *ls_rsp)
{
	struct lpfc_async_xchg_ctx *axchg =
		container_of(ls_rsp, struct lpfc_async_xchg_ctx, ls_rsp);
	struct lpfc_nvme_lport *lport;
	int rc;

	if (axchg->phba->pport->load_flag & FC_UNLOADING)
		return -ENODEV;

	lport = (struct lpfc_nvme_lport *)localport->private;

	rc = __lpfc_nvme_xmt_ls_rsp(axchg, ls_rsp, __lpfc_nvme_xmt_ls_rsp_cmp);

	if (rc) {
		 
		if (rc != -EALREADY)
			atomic_inc(&lport->xmt_ls_abort);
		return rc;
	}

	return 0;
}

 
static void
lpfc_nvme_ls_abort(struct nvme_fc_local_port *pnvme_lport,
		   struct nvme_fc_remote_port *pnvme_rport,
		   struct nvmefc_ls_req *pnvme_lsreq)
{
	struct lpfc_nvme_lport *lport;
	struct lpfc_vport *vport;
	struct lpfc_nodelist *ndlp;
	int ret;

	lport = (struct lpfc_nvme_lport *)pnvme_lport->private;
	if (unlikely(!lport))
		return;
	vport = lport->vport;

	if (vport->load_flag & FC_UNLOADING)
		return;

	ndlp = lpfc_findnode_did(vport, pnvme_rport->port_id);

	ret = __lpfc_nvme_ls_abort(vport, ndlp, pnvme_lsreq);
	if (!ret)
		atomic_inc(&lport->xmt_ls_abort);
}

 
static inline void
lpfc_nvme_adj_fcp_sgls(struct lpfc_vport *vport,
		       struct lpfc_io_buf *lpfc_ncmd,
		       struct nvmefc_fcp_req *nCmd)
{
	struct lpfc_hba  *phba = vport->phba;
	struct sli4_sge *sgl;
	union lpfc_wqe128 *wqe;
	uint32_t *wptr, *dptr;

	 

	wqe = &lpfc_ncmd->cur_iocbq.wqe;

	 
	sgl = lpfc_ncmd->dma_sgl;
	sgl->sge_len = cpu_to_le32(nCmd->cmdlen);
	if (phba->cfg_nvme_embed_cmd) {
		sgl->addr_hi = 0;
		sgl->addr_lo = 0;

		 
		wqe->generic.bde.tus.f.bdeFlags = BUFF_TYPE_BDE_IMMED;
		wqe->generic.bde.tus.f.bdeSize = 56;
		wqe->generic.bde.addrHigh = 0;
		wqe->generic.bde.addrLow =  64;   

		 

		 
		wptr = &wqe->words[16];   
		dptr = (uint32_t *)nCmd->cmdaddr;   
		dptr++;			 

		*wptr++ = *dptr++;	 
		*wptr++ = *dptr++;	 
		*wptr++ = *dptr++;	 
		*wptr++ = *dptr++;	 
		dptr++;			 
		*wptr++ = *dptr++;	 
		*wptr++ = *dptr++;	 
		dptr += 8;		 
		*wptr++ = *dptr++;	 
		*wptr++ = *dptr++;	 
		*wptr++ = *dptr++;	 
		*wptr++ = *dptr++;	 
		*wptr++ = *dptr++;	 
		*wptr++ = *dptr++;	 
		*wptr++ = *dptr++;	 
		*wptr   = *dptr;	 
	} else {
		sgl->addr_hi = cpu_to_le32(putPaddrHigh(nCmd->cmddma));
		sgl->addr_lo = cpu_to_le32(putPaddrLow(nCmd->cmddma));

		 
		wqe->generic.bde.tus.f.bdeFlags =  BUFF_TYPE_BDE_64;
		wqe->generic.bde.tus.f.bdeSize = nCmd->cmdlen;
		wqe->generic.bde.addrHigh = sgl->addr_hi;
		wqe->generic.bde.addrLow =  sgl->addr_lo;

		 
		bf_set(wqe_dbde, &wqe->generic.wqe_com, 1);
		bf_set(wqe_wqes, &wqe->generic.wqe_com, 0);
	}

	sgl++;

	 
	sgl->addr_hi = cpu_to_le32(putPaddrHigh(nCmd->rspdma));
	sgl->addr_lo = cpu_to_le32(putPaddrLow(nCmd->rspdma));
	sgl->word2 = le32_to_cpu(sgl->word2);
	if (nCmd->sg_cnt)
		bf_set(lpfc_sli4_sge_last, sgl, 0);
	else
		bf_set(lpfc_sli4_sge_last, sgl, 1);
	sgl->word2 = cpu_to_le32(sgl->word2);
	sgl->sge_len = cpu_to_le32(nCmd->rsplen);
}


 
static void
lpfc_nvme_io_cmd_cmpl(struct lpfc_hba *phba, struct lpfc_iocbq *pwqeIn,
		      struct lpfc_iocbq *pwqeOut)
{
	struct lpfc_io_buf *lpfc_ncmd = pwqeIn->io_buf;
	struct lpfc_wcqe_complete *wcqe = &pwqeOut->wcqe_cmpl;
	struct lpfc_vport *vport = pwqeIn->vport;
	struct nvmefc_fcp_req *nCmd;
	struct nvme_fc_ersp_iu *ep;
	struct nvme_fc_cmd_iu *cp;
	struct lpfc_nodelist *ndlp;
	struct lpfc_nvme_fcpreq_priv *freqpriv;
	struct lpfc_nvme_lport *lport;
	uint32_t code, status, idx;
	uint16_t cid, sqhd, data;
	uint32_t *ptr;
	uint32_t lat;
	bool call_done = false;
#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	int cpu;
#endif
	int offline = 0;

	 
	if (!lpfc_ncmd) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "6071 Null lpfc_ncmd pointer. No "
				 "release, skip completion\n");
		return;
	}

	 
	spin_lock(&lpfc_ncmd->buf_lock);

	if (!lpfc_ncmd->nvmeCmd) {
		spin_unlock(&lpfc_ncmd->buf_lock);
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "6066 Missing cmpl ptrs: lpfc_ncmd x%px, "
				 "nvmeCmd x%px\n",
				 lpfc_ncmd, lpfc_ncmd->nvmeCmd);

		 
		lpfc_release_nvme_buf(phba, lpfc_ncmd);
		return;
	}
	nCmd = lpfc_ncmd->nvmeCmd;
	status = bf_get(lpfc_wcqe_c_status, wcqe);

	idx = lpfc_ncmd->cur_iocbq.hba_wqidx;
	phba->sli4_hba.hdwq[idx].nvme_cstat.io_cmpls++;

	if (unlikely(status && vport->localport)) {
		lport = (struct lpfc_nvme_lport *)vport->localport->private;
		if (lport) {
			if (bf_get(lpfc_wcqe_c_xb, wcqe))
				atomic_inc(&lport->cmpl_fcp_xb);
			atomic_inc(&lport->cmpl_fcp_err);
		}
	}

	lpfc_nvmeio_data(phba, "NVME FCP CMPL: xri x%x stat x%x parm x%x\n",
			 lpfc_ncmd->cur_iocbq.sli4_xritag,
			 status, wcqe->parameter);
	 
	ndlp = lpfc_ncmd->ndlp;
	if (!ndlp) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "6062 Ignoring NVME cmpl.  No ndlp\n");
		goto out_err;
	}

	code = bf_get(lpfc_wcqe_c_code, wcqe);
	if (code == CQE_CODE_NVME_ERSP) {
		 
		ep = (struct nvme_fc_ersp_iu *)nCmd->rspaddr;

		 
		cp = (struct nvme_fc_cmd_iu *)nCmd->cmdaddr;
		cid = cp->sqe.common.command_id;

		 
		sqhd = bf_get(lpfc_wcqe_c_sqhead, wcqe);

		 
		ep->iu_len = cpu_to_be16(8);
		ep->rsn = wcqe->parameter;
		ep->xfrd_len = cpu_to_be32(nCmd->payload_length);
		ep->rsvd12 = 0;
		ptr = (uint32_t *)&ep->cqe.result.u64;
		*ptr++ = wcqe->total_data_placed;
		data = bf_get(lpfc_wcqe_c_ersp0, wcqe);
		*ptr = (uint32_t)data;
		ep->cqe.sq_head = sqhd;
		ep->cqe.sq_id =  nCmd->sqid;
		ep->cqe.command_id = cid;
		ep->cqe.status = 0;

		lpfc_ncmd->status = IOSTAT_SUCCESS;
		lpfc_ncmd->result = 0;
		nCmd->rcv_rsplen = LPFC_NVME_ERSP_LEN;
		nCmd->transferred_length = nCmd->payload_length;
	} else {
		lpfc_ncmd->status = status;
		lpfc_ncmd->result = (wcqe->parameter & IOERR_PARAM_MASK);

		 
		switch (lpfc_ncmd->status) {
		case IOSTAT_SUCCESS:
			nCmd->transferred_length = wcqe->total_data_placed;
			nCmd->rcv_rsplen = 0;
			nCmd->status = 0;
			break;
		case IOSTAT_FCP_RSP_ERROR:
			nCmd->transferred_length = wcqe->total_data_placed;
			nCmd->rcv_rsplen = wcqe->parameter;
			nCmd->status = 0;

			 
			cp = (struct nvme_fc_cmd_iu *)nCmd->cmdaddr;
			ep = (struct nvme_fc_ersp_iu *)nCmd->rspaddr;

			 
			if (nCmd->rcv_rsplen == LPFC_NVME_ERSP_LEN) {
				lpfc_ncmd->status = IOSTAT_SUCCESS;
				lpfc_ncmd->result = 0;

				lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME,
					"6084 NVME FCP_ERR ERSP: "
					"xri %x placed x%x opcode x%x cmd_id "
					"x%x cqe_status x%x\n",
					lpfc_ncmd->cur_iocbq.sli4_xritag,
					wcqe->total_data_placed,
					cp->sqe.common.opcode,
					cp->sqe.common.command_id,
					ep->cqe.status);
				break;
			}
			lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
					 "6081 NVME Completion Protocol Error: "
					 "xri %x status x%x result x%x "
					 "placed x%x opcode x%x cmd_id x%x, "
					 "cqe_status x%x\n",
					 lpfc_ncmd->cur_iocbq.sli4_xritag,
					 lpfc_ncmd->status, lpfc_ncmd->result,
					 wcqe->total_data_placed,
					 cp->sqe.common.opcode,
					 cp->sqe.common.command_id,
					 ep->cqe.status);
			break;
		case IOSTAT_LOCAL_REJECT:
			 
			if (lpfc_ncmd->result == IOERR_ABORT_REQUESTED)
				lpfc_printf_vlog(vport, KERN_INFO,
					 LOG_NVME_IOERR,
					 "6032 Delay Aborted cmd x%px "
					 "nvme cmd x%px, xri x%x, "
					 "xb %d\n",
					 lpfc_ncmd, nCmd,
					 lpfc_ncmd->cur_iocbq.sli4_xritag,
					 bf_get(lpfc_wcqe_c_xb, wcqe));
			fallthrough;
		default:
out_err:
			lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_IOERR,
					 "6072 NVME Completion Error: xri %x "
					 "status x%x result x%x [x%x] "
					 "placed x%x\n",
					 lpfc_ncmd->cur_iocbq.sli4_xritag,
					 lpfc_ncmd->status, lpfc_ncmd->result,
					 wcqe->parameter,
					 wcqe->total_data_placed);
			nCmd->transferred_length = 0;
			nCmd->rcv_rsplen = 0;
			nCmd->status = NVME_SC_INTERNAL;
			offline = pci_channel_offline(vport->phba->pcidev);
		}
	}

	 
	if (bf_get(lpfc_wcqe_c_xb, wcqe) && !offline)
		lpfc_ncmd->flags |= LPFC_SBUF_XBUSY;
	else
		lpfc_ncmd->flags &= ~LPFC_SBUF_XBUSY;

	 
#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	if (lpfc_ncmd->ts_cmd_start) {
		lpfc_ncmd->ts_isr_cmpl = pwqeIn->isr_timestamp;
		lpfc_ncmd->ts_data_io = ktime_get_ns();
		phba->ktime_last_cmd = lpfc_ncmd->ts_data_io;
		lpfc_io_ktime(phba, lpfc_ncmd);
	}
	if (unlikely(phba->hdwqstat_on & LPFC_CHECK_NVME_IO)) {
		cpu = raw_smp_processor_id();
		this_cpu_inc(phba->sli4_hba.c_stat->cmpl_io);
		if (lpfc_ncmd->cpu != cpu)
			lpfc_printf_vlog(vport,
					 KERN_INFO, LOG_NVME_IOERR,
					 "6701 CPU Check cmpl: "
					 "cpu %d expect %d\n",
					 cpu, lpfc_ncmd->cpu);
	}
#endif

	 

	if (!(lpfc_ncmd->flags & LPFC_SBUF_XBUSY)) {
		freqpriv = nCmd->private;
		freqpriv->nvme_buf = NULL;
		lpfc_ncmd->nvmeCmd = NULL;
		call_done = true;
	}
	spin_unlock(&lpfc_ncmd->buf_lock);

	 
	if (phba->cmf_active_mode != LPFC_CFG_OFF &&
	    nCmd->io_dir == NVMEFC_FCP_READ &&
	    nCmd->payload_length) {
		 
		lat = ktime_get_ns() - lpfc_ncmd->rx_cmd_start;
		lpfc_update_cmf_cmpl(phba, lat, nCmd->payload_length, NULL);
	}

	if (call_done)
		nCmd->done(nCmd);

	 
	lpfc_release_nvme_buf(phba, lpfc_ncmd);
}


 
static int
lpfc_nvme_prep_io_cmd(struct lpfc_vport *vport,
		      struct lpfc_io_buf *lpfc_ncmd,
		      struct lpfc_nodelist *pnode,
		      struct lpfc_fc4_ctrl_stat *cstat)
{
	struct lpfc_hba *phba = vport->phba;
	struct nvmefc_fcp_req *nCmd = lpfc_ncmd->nvmeCmd;
	struct nvme_common_command *sqe;
	struct lpfc_iocbq *pwqeq = &lpfc_ncmd->cur_iocbq;
	union lpfc_wqe128 *wqe = &pwqeq->wqe;
	uint32_t req_len;

	 
	if (nCmd->sg_cnt) {
		if (nCmd->io_dir == NVMEFC_FCP_WRITE) {
			 
			memcpy(&wqe->words[7],
			       &lpfc_iwrite_cmd_template.words[7],
			       sizeof(uint32_t) * 5);

			 
			wqe->fcp_iwrite.total_xfer_len = nCmd->payload_length;

			 
			if ((phba->cfg_nvme_enable_fb) &&
			    (pnode->nlp_flag & NLP_FIRSTBURST)) {
				req_len = lpfc_ncmd->nvmeCmd->payload_length;
				if (req_len < pnode->nvme_fb_size)
					wqe->fcp_iwrite.initial_xfer_len =
						req_len;
				else
					wqe->fcp_iwrite.initial_xfer_len =
						pnode->nvme_fb_size;
			} else {
				wqe->fcp_iwrite.initial_xfer_len = 0;
			}
			cstat->output_requests++;
		} else {
			 
			memcpy(&wqe->words[7],
			       &lpfc_iread_cmd_template.words[7],
			       sizeof(uint32_t) * 5);

			 
			wqe->fcp_iread.total_xfer_len = nCmd->payload_length;

			 
			wqe->fcp_iread.rsrvd5 = 0;

			 
			if (phba->cmf_active_mode == LPFC_CFG_MANAGED)
				bf_set(wqe_iod, &wqe->fcp_iread.wqe_com,
				       LPFC_WQE_IOD_NONE);
			cstat->input_requests++;
		}
	} else {
		 
		memcpy(&wqe->words[4], &lpfc_icmnd_cmd_template.words[4],
		       sizeof(uint32_t) * 8);
		cstat->control_requests++;
	}

	if (pnode->nlp_nvme_info & NLP_NVME_NSLER) {
		bf_set(wqe_erp, &wqe->generic.wqe_com, 1);
		sqe = &((struct nvme_fc_cmd_iu *)
			nCmd->cmdaddr)->sqe.common;
		if (sqe->opcode == nvme_admin_async_event)
			bf_set(wqe_ffrq, &wqe->generic.wqe_com, 1);
	}

	 

	 
	bf_set(payload_offset_len, &wqe->fcp_icmd,
	       (nCmd->rsplen + nCmd->cmdlen));

	 
	bf_set(wqe_ctxt_tag, &wqe->generic.wqe_com,
	       phba->sli4_hba.rpi_ids[pnode->nlp_rpi]);
	bf_set(wqe_xri_tag, &wqe->generic.wqe_com, pwqeq->sli4_xritag);

	 
	wqe->generic.wqe_com.abort_tag = pwqeq->iotag;

	 
	bf_set(wqe_reqtag, &wqe->generic.wqe_com, pwqeq->iotag);

	 
	bf_set(wqe_xchg, &wqe->fcp_iwrite.wqe_com, LPFC_NVME_XCHG);

	 

	 
	if (unlikely(lpfc_ncmd->cur_iocbq.cmd_flag & LPFC_IO_VMID)) {
		if (phba->pport->vmid_priority_tagging) {
			bf_set(wqe_ccpe, &wqe->fcp_iwrite.wqe_com, 1);
			bf_set(wqe_ccp, &wqe->fcp_iwrite.wqe_com,
			       lpfc_ncmd->cur_iocbq.vmid_tag.cs_ctl_vmid);
		} else {
			bf_set(wqe_appid, &wqe->fcp_iwrite.wqe_com, 1);
			bf_set(wqe_wqes, &wqe->fcp_iwrite.wqe_com, 1);
			wqe->words[31] = lpfc_ncmd->cur_iocbq.vmid_tag.app_id;
		}
	}

	pwqeq->vport = vport;
	return 0;
}


 
static int
lpfc_nvme_prep_io_dma(struct lpfc_vport *vport,
		      struct lpfc_io_buf *lpfc_ncmd)
{
	struct lpfc_hba *phba = vport->phba;
	struct nvmefc_fcp_req *nCmd = lpfc_ncmd->nvmeCmd;
	union lpfc_wqe128 *wqe = &lpfc_ncmd->cur_iocbq.wqe;
	struct sli4_sge *sgl = lpfc_ncmd->dma_sgl;
	struct sli4_hybrid_sgl *sgl_xtra = NULL;
	struct scatterlist *data_sg;
	struct sli4_sge *first_data_sgl;
	struct ulp_bde64 *bde;
	dma_addr_t physaddr = 0;
	uint32_t dma_len = 0;
	uint32_t dma_offset = 0;
	int nseg, i, j;
	bool lsp_just_set = false;

	 
	lpfc_nvme_adj_fcp_sgls(vport, lpfc_ncmd, nCmd);

	 
	if (nCmd->sg_cnt) {
		 
		sgl += 2;

		first_data_sgl = sgl;
		lpfc_ncmd->seg_cnt = nCmd->sg_cnt;
		if (lpfc_ncmd->seg_cnt > lpfc_nvme_template.max_sgl_segments) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"6058 Too many sg segments from "
					"NVME Transport.  Max %d, "
					"nvmeIO sg_cnt %d\n",
					phba->cfg_nvme_seg_cnt + 1,
					lpfc_ncmd->seg_cnt);
			lpfc_ncmd->seg_cnt = 0;
			return 1;
		}

		 
		nseg = nCmd->sg_cnt;
		data_sg = nCmd->first_sgl;

		 
		j = 2;
		for (i = 0; i < nseg; i++) {
			if (data_sg == NULL) {
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"6059 dptr err %d, nseg %d\n",
						i, nseg);
				lpfc_ncmd->seg_cnt = 0;
				return 1;
			}

			sgl->word2 = 0;
			if (nseg == 1) {
				bf_set(lpfc_sli4_sge_last, sgl, 1);
				bf_set(lpfc_sli4_sge_type, sgl,
				       LPFC_SGE_TYPE_DATA);
			} else {
				bf_set(lpfc_sli4_sge_last, sgl, 0);

				 
				if (!lsp_just_set &&
				    !((j + 1) % phba->border_sge_num) &&
				    ((nseg - 1) != i)) {
					 
					bf_set(lpfc_sli4_sge_type, sgl,
					       LPFC_SGE_TYPE_LSP);

					sgl_xtra = lpfc_get_sgl_per_hdwq(
							phba, lpfc_ncmd);

					if (unlikely(!sgl_xtra)) {
						lpfc_ncmd->seg_cnt = 0;
						return 1;
					}
					sgl->addr_lo = cpu_to_le32(putPaddrLow(
						       sgl_xtra->dma_phys_sgl));
					sgl->addr_hi = cpu_to_le32(putPaddrHigh(
						       sgl_xtra->dma_phys_sgl));

				} else {
					bf_set(lpfc_sli4_sge_type, sgl,
					       LPFC_SGE_TYPE_DATA);
				}
			}

			if (!(bf_get(lpfc_sli4_sge_type, sgl) &
				     LPFC_SGE_TYPE_LSP)) {
				if ((nseg - 1) == i)
					bf_set(lpfc_sli4_sge_last, sgl, 1);

				physaddr = sg_dma_address(data_sg);
				dma_len = sg_dma_len(data_sg);
				sgl->addr_lo = cpu_to_le32(
							 putPaddrLow(physaddr));
				sgl->addr_hi = cpu_to_le32(
							putPaddrHigh(physaddr));

				bf_set(lpfc_sli4_sge_offset, sgl, dma_offset);
				sgl->word2 = cpu_to_le32(sgl->word2);
				sgl->sge_len = cpu_to_le32(dma_len);

				dma_offset += dma_len;
				data_sg = sg_next(data_sg);

				sgl++;

				lsp_just_set = false;
			} else {
				sgl->word2 = cpu_to_le32(sgl->word2);

				sgl->sge_len = cpu_to_le32(
						     phba->cfg_sg_dma_buf_size);

				sgl = (struct sli4_sge *)sgl_xtra->dma_sgl;
				i = i - 1;

				lsp_just_set = true;
			}

			j++;
		}

		 
		if (nseg == 1 && phba->cfg_enable_pbde) {
			 
			bde = (struct ulp_bde64 *)
				&wqe->words[13];
			bde->addrLow = first_data_sgl->addr_lo;
			bde->addrHigh = first_data_sgl->addr_hi;
			bde->tus.f.bdeSize =
				le32_to_cpu(first_data_sgl->sge_len);
			bde->tus.f.bdeFlags = BUFF_TYPE_BDE_64;
			bde->tus.w = cpu_to_le32(bde->tus.w);

			 
			bf_set(wqe_pbde, &wqe->generic.wqe_com, 1);
		} else {
			memset(&wqe->words[13], 0, (sizeof(uint32_t) * 3));
			 
		}

	} else {
		lpfc_ncmd->seg_cnt = 0;

		 
		if (nCmd->payload_length != 0) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"6063 NVME DMA Prep Err: sg_cnt %d "
					"payload_length x%x\n",
					nCmd->sg_cnt, nCmd->payload_length);
			return 1;
		}
	}
	return 0;
}

 
static int
lpfc_nvme_fcp_io_submit(struct nvme_fc_local_port *pnvme_lport,
			struct nvme_fc_remote_port *pnvme_rport,
			void *hw_queue_handle,
			struct nvmefc_fcp_req *pnvme_fcreq)
{
	int ret = 0;
	int expedite = 0;
	int idx, cpu;
	struct lpfc_nvme_lport *lport;
	struct lpfc_fc4_ctrl_stat *cstat;
	struct lpfc_vport *vport;
	struct lpfc_hba *phba;
	struct lpfc_nodelist *ndlp;
	struct lpfc_io_buf *lpfc_ncmd;
	struct lpfc_nvme_rport *rport;
	struct lpfc_nvme_qhandle *lpfc_queue_info;
	struct lpfc_nvme_fcpreq_priv *freqpriv;
	struct nvme_common_command *sqe;
	uint64_t start = 0;
#if (IS_ENABLED(CONFIG_NVME_FC))
	u8 *uuid = NULL;
	int err;
	enum dma_data_direction iodir;
#endif

	 
	lport = (struct lpfc_nvme_lport *)pnvme_lport->private;
	if (unlikely(!lport)) {
		ret = -EINVAL;
		goto out_fail;
	}

	vport = lport->vport;

	if (unlikely(!hw_queue_handle)) {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_IOERR,
				 "6117 Fail IO, NULL hw_queue_handle\n");
		atomic_inc(&lport->xmt_fcp_err);
		ret = -EBUSY;
		goto out_fail;
	}

	phba = vport->phba;

	if ((unlikely(vport->load_flag & FC_UNLOADING)) ||
	    phba->hba_flag & HBA_IOQ_FLUSH) {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_IOERR,
				 "6124 Fail IO, Driver unload\n");
		atomic_inc(&lport->xmt_fcp_err);
		ret = -ENODEV;
		goto out_fail;
	}

	freqpriv = pnvme_fcreq->private;
	if (unlikely(!freqpriv)) {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_IOERR,
				 "6158 Fail IO, NULL request data\n");
		atomic_inc(&lport->xmt_fcp_err);
		ret = -EINVAL;
		goto out_fail;
	}

#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	if (phba->ktime_on)
		start = ktime_get_ns();
#endif
	rport = (struct lpfc_nvme_rport *)pnvme_rport->private;
	lpfc_queue_info = (struct lpfc_nvme_qhandle *)hw_queue_handle;

	 
	ndlp = rport->ndlp;
	if (!ndlp) {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE | LOG_NVME_IOERR,
				 "6053 Busy IO, ndlp not ready: rport x%px "
				  "ndlp x%px, DID x%06x\n",
				 rport, ndlp, pnvme_rport->port_id);
		atomic_inc(&lport->xmt_fcp_err);
		ret = -EBUSY;
		goto out_fail;
	}

	 
	if ((ndlp->nlp_type & NLP_NVME_TARGET) &&
	    (ndlp->nlp_state != NLP_STE_MAPPED_NODE)) {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE | LOG_NVME_IOERR,
				 "6036 Fail IO, DID x%06x not ready for "
				 "IO. State x%x, Type x%x Flg x%x\n",
				 pnvme_rport->port_id,
				 ndlp->nlp_state, ndlp->nlp_type,
				 ndlp->fc4_xpt_flags);
		atomic_inc(&lport->xmt_fcp_bad_ndlp);
		ret = -EBUSY;
		goto out_fail;

	}

	 
	if (!lpfc_queue_info->qidx && !pnvme_fcreq->sg_cnt) {
		sqe = &((struct nvme_fc_cmd_iu *)
			pnvme_fcreq->cmdaddr)->sqe.common;
		if (sqe->opcode == nvme_admin_keep_alive)
			expedite = 1;
	}

	 
	if (phba->cmf_active_mode != LPFC_CFG_OFF &&
	    pnvme_fcreq->io_dir == NVMEFC_FCP_READ &&
	    pnvme_fcreq->payload_length) {
		ret = lpfc_update_cmf_cmd(phba, pnvme_fcreq->payload_length);
		if (ret) {
			ret = -EBUSY;
			goto out_fail;
		}
		 
		start = ktime_get_ns();
	}

	 
	if (lpfc_ndlp_check_qdepth(phba, ndlp)) {
		if ((atomic_read(&ndlp->cmd_pending) >= ndlp->cmd_qdepth) &&
		    !expedite) {
			lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_IOERR,
					 "6174 Fail IO, ndlp qdepth exceeded: "
					 "idx %d DID %x pend %d qdepth %d\n",
					 lpfc_queue_info->index, ndlp->nlp_DID,
					 atomic_read(&ndlp->cmd_pending),
					 ndlp->cmd_qdepth);
			atomic_inc(&lport->xmt_fcp_qdepth);
			ret = -EBUSY;
			goto out_fail1;
		}
	}

	 
	if (phba->cfg_fcp_io_sched == LPFC_FCP_SCHED_BY_HDWQ) {
		idx = lpfc_queue_info->index;
	} else {
		cpu = raw_smp_processor_id();
		idx = phba->sli4_hba.cpu_map[cpu].hdwq;
	}

	lpfc_ncmd = lpfc_get_nvme_buf(phba, ndlp, idx, expedite);
	if (lpfc_ncmd == NULL) {
		atomic_inc(&lport->xmt_fcp_noxri);
		lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_IOERR,
				 "6065 Fail IO, driver buffer pool is empty: "
				 "idx %d DID %x\n",
				 lpfc_queue_info->index, ndlp->nlp_DID);
		ret = -EBUSY;
		goto out_fail1;
	}
#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	if (start) {
		lpfc_ncmd->ts_cmd_start = start;
		lpfc_ncmd->ts_last_cmd = phba->ktime_last_cmd;
	} else {
		lpfc_ncmd->ts_cmd_start = 0;
	}
#endif
	lpfc_ncmd->rx_cmd_start = start;

	 
	freqpriv->nvme_buf = lpfc_ncmd;
	lpfc_ncmd->nvmeCmd = pnvme_fcreq;
	lpfc_ncmd->ndlp = ndlp;
	lpfc_ncmd->qidx = lpfc_queue_info->qidx;

#if (IS_ENABLED(CONFIG_NVME_FC))
	 
	if (lpfc_is_vmid_enabled(phba) &&
	    (ndlp->vmid_support ||
	     phba->pport->vmid_priority_tagging ==
	     LPFC_VMID_PRIO_TAG_ALL_TARGETS)) {
		 
		 
		uuid = nvme_fc_io_getuuid(pnvme_fcreq);

		if (uuid) {
			if (pnvme_fcreq->io_dir == NVMEFC_FCP_WRITE)
				iodir = DMA_TO_DEVICE;
			else if (pnvme_fcreq->io_dir == NVMEFC_FCP_READ)
				iodir = DMA_FROM_DEVICE;
			else
				iodir = DMA_NONE;

			err = lpfc_vmid_get_appid(vport, uuid, iodir,
					(union lpfc_vmid_io_tag *)
						&lpfc_ncmd->cur_iocbq.vmid_tag);
			if (!err)
				lpfc_ncmd->cur_iocbq.cmd_flag |= LPFC_IO_VMID;
		}
	}
#endif

	 
	lpfc_ncmd->cur_iocbq.hba_wqidx = idx;
	cstat = &phba->sli4_hba.hdwq[idx].nvme_cstat;

	lpfc_nvme_prep_io_cmd(vport, lpfc_ncmd, ndlp, cstat);
	ret = lpfc_nvme_prep_io_dma(vport, lpfc_ncmd);
	if (ret) {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_IOERR,
				 "6175 Fail IO, Prep DMA: "
				 "idx %d DID %x\n",
				 lpfc_queue_info->index, ndlp->nlp_DID);
		atomic_inc(&lport->xmt_fcp_err);
		ret = -ENOMEM;
		goto out_free_nvme_buf;
	}

	lpfc_nvmeio_data(phba, "NVME FCP XMIT: xri x%x idx %d to %06x\n",
			 lpfc_ncmd->cur_iocbq.sli4_xritag,
			 lpfc_queue_info->index, ndlp->nlp_DID);

	ret = lpfc_sli4_issue_wqe(phba, lpfc_ncmd->hdwq, &lpfc_ncmd->cur_iocbq);
	if (ret) {
		atomic_inc(&lport->xmt_fcp_wqerr);
		lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_IOERR,
				 "6113 Fail IO, Could not issue WQE err %x "
				 "sid: x%x did: x%x oxid: x%x\n",
				 ret, vport->fc_myDID, ndlp->nlp_DID,
				 lpfc_ncmd->cur_iocbq.sli4_xritag);
		goto out_free_nvme_buf;
	}

	if (phba->cfg_xri_rebalancing)
		lpfc_keep_pvt_pool_above_lowwm(phba, lpfc_ncmd->hdwq_no);

#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	if (lpfc_ncmd->ts_cmd_start)
		lpfc_ncmd->ts_cmd_wqput = ktime_get_ns();

	if (phba->hdwqstat_on & LPFC_CHECK_NVME_IO) {
		cpu = raw_smp_processor_id();
		this_cpu_inc(phba->sli4_hba.c_stat->xmt_io);
		lpfc_ncmd->cpu = cpu;
		if (idx != cpu)
			lpfc_printf_vlog(vport,
					 KERN_INFO, LOG_NVME_IOERR,
					"6702 CPU Check cmd: "
					"cpu %d wq %d\n",
					lpfc_ncmd->cpu,
					lpfc_queue_info->index);
	}
#endif
	return 0;

 out_free_nvme_buf:
	if (lpfc_ncmd->nvmeCmd->sg_cnt) {
		if (lpfc_ncmd->nvmeCmd->io_dir == NVMEFC_FCP_WRITE)
			cstat->output_requests--;
		else
			cstat->input_requests--;
	} else
		cstat->control_requests--;
	lpfc_release_nvme_buf(phba, lpfc_ncmd);
 out_fail1:
	lpfc_update_cmf_cmpl(phba, LPFC_CGN_NOT_SENT,
			     pnvme_fcreq->payload_length, NULL);
 out_fail:
	return ret;
}

 
void
lpfc_nvme_abort_fcreq_cmpl(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
			   struct lpfc_iocbq *rspiocb)
{
	struct lpfc_wcqe_complete *abts_cmpl = &rspiocb->wcqe_cmpl;

	lpfc_printf_log(phba, KERN_INFO, LOG_NVME,
			"6145 ABORT_XRI_CN completing on rpi x%x "
			"original iotag x%x, abort cmd iotag x%x "
			"req_tag x%x, status x%x, hwstatus x%x\n",
			bf_get(wqe_ctxt_tag, &cmdiocb->wqe.generic.wqe_com),
			get_job_abtsiotag(phba, cmdiocb), cmdiocb->iotag,
			bf_get(lpfc_wcqe_c_request_tag, abts_cmpl),
			bf_get(lpfc_wcqe_c_status, abts_cmpl),
			bf_get(lpfc_wcqe_c_hw_status, abts_cmpl));
	lpfc_sli_release_iocbq(phba, cmdiocb);
}

 
static void
lpfc_nvme_fcp_abort(struct nvme_fc_local_port *pnvme_lport,
		    struct nvme_fc_remote_port *pnvme_rport,
		    void *hw_queue_handle,
		    struct nvmefc_fcp_req *pnvme_fcreq)
{
	struct lpfc_nvme_lport *lport;
	struct lpfc_vport *vport;
	struct lpfc_hba *phba;
	struct lpfc_io_buf *lpfc_nbuf;
	struct lpfc_iocbq *nvmereq_wqe;
	struct lpfc_nvme_fcpreq_priv *freqpriv;
	unsigned long flags;
	int ret_val;

	 
	lport = (struct lpfc_nvme_lport *)pnvme_lport->private;
	if (unlikely(!lport))
		return;

	vport = lport->vport;

	if (unlikely(!hw_queue_handle)) {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_ABTS,
				 "6129 Fail Abort, HW Queue Handle NULL.\n");
		return;
	}

	phba = vport->phba;
	freqpriv = pnvme_fcreq->private;

	if (unlikely(!freqpriv))
		return;
	if (vport->load_flag & FC_UNLOADING)
		return;

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_ABTS,
			 "6002 Abort Request to rport DID x%06x "
			 "for nvme_fc_req x%px\n",
			 pnvme_rport->port_id,
			 pnvme_fcreq);

	lpfc_nbuf = freqpriv->nvme_buf;
	if (!lpfc_nbuf) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "6140 NVME IO req has no matching lpfc nvme "
				 "io buffer.  Skipping abort req.\n");
		return;
	} else if (!lpfc_nbuf->nvmeCmd) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "6141 lpfc NVME IO req has no nvme_fcreq "
				 "io buffer.  Skipping abort req.\n");
		return;
	}

	 
	spin_lock_irqsave(&lpfc_nbuf->buf_lock, flags);

	 
	spin_lock(&phba->hbalock);
	 
	if (phba->hba_flag & HBA_IOQ_FLUSH) {
		spin_unlock(&phba->hbalock);
		spin_unlock_irqrestore(&lpfc_nbuf->buf_lock, flags);
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "6139 Driver in reset cleanup - flushing "
				 "NVME Req now.  hba_flag x%x\n",
				 phba->hba_flag);
		return;
	}

	nvmereq_wqe = &lpfc_nbuf->cur_iocbq;

	 
	if (lpfc_nbuf->nvmeCmd != pnvme_fcreq) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "6143 NVME req mismatch: "
				 "lpfc_nbuf x%px nvmeCmd x%px, "
				 "pnvme_fcreq x%px.  Skipping Abort xri x%x\n",
				 lpfc_nbuf, lpfc_nbuf->nvmeCmd,
				 pnvme_fcreq, nvmereq_wqe->sli4_xritag);
		goto out_unlock;
	}

	 
	if (!(nvmereq_wqe->cmd_flag & LPFC_IO_ON_TXCMPLQ)) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "6142 NVME IO req x%px not queued - skipping "
				 "abort req xri x%x\n",
				 pnvme_fcreq, nvmereq_wqe->sli4_xritag);
		goto out_unlock;
	}

	atomic_inc(&lport->xmt_fcp_abort);
	lpfc_nvmeio_data(phba, "NVME FCP ABORT: xri x%x idx %d to %06x\n",
			 nvmereq_wqe->sli4_xritag,
			 nvmereq_wqe->hba_wqidx, pnvme_rport->port_id);

	 
	if (nvmereq_wqe->cmd_flag & LPFC_DRIVER_ABORTED) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "6144 Outstanding NVME I/O Abort Request "
				 "still pending on nvme_fcreq x%px, "
				 "lpfc_ncmd x%px xri x%x\n",
				 pnvme_fcreq, lpfc_nbuf,
				 nvmereq_wqe->sli4_xritag);
		goto out_unlock;
	}

	ret_val = lpfc_sli4_issue_abort_iotag(phba, nvmereq_wqe,
					      lpfc_nvme_abort_fcreq_cmpl);

	spin_unlock(&phba->hbalock);
	spin_unlock_irqrestore(&lpfc_nbuf->buf_lock, flags);

	 
	lpfc_issue_hb_tmo(phba);

	if (ret_val != WQE_SUCCESS) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "6137 Failed abts issue_wqe with status x%x "
				 "for nvme_fcreq x%px.\n",
				 ret_val, pnvme_fcreq);
		return;
	}

	lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_ABTS,
			 "6138 Transport Abort NVME Request Issued for "
			 "ox_id x%x\n",
			 nvmereq_wqe->sli4_xritag);
	return;

out_unlock:
	spin_unlock(&phba->hbalock);
	spin_unlock_irqrestore(&lpfc_nbuf->buf_lock, flags);
	return;
}

 
static struct nvme_fc_port_template lpfc_nvme_template = {
	 
	.localport_delete  = lpfc_nvme_localport_delete,
	.remoteport_delete = lpfc_nvme_remoteport_delete,
	.create_queue = lpfc_nvme_create_queue,
	.delete_queue = lpfc_nvme_delete_queue,
	.ls_req       = lpfc_nvme_ls_req,
	.fcp_io       = lpfc_nvme_fcp_io_submit,
	.ls_abort     = lpfc_nvme_ls_abort,
	.fcp_abort    = lpfc_nvme_fcp_abort,
	.xmt_ls_rsp   = lpfc_nvme_xmt_ls_rsp,

	.max_hw_queues = 1,
	.max_sgl_segments = LPFC_NVME_DEFAULT_SEGS,
	.max_dif_sgl_segments = LPFC_NVME_DEFAULT_SEGS,
	.dma_boundary = 0xFFFFFFFF,

	 
	.local_priv_sz = sizeof(struct lpfc_nvme_lport),
	.remote_priv_sz = sizeof(struct lpfc_nvme_rport),
	.lsrqst_priv_sz = 0,
	.fcprqst_priv_sz = sizeof(struct lpfc_nvme_fcpreq_priv),
};

 
static struct lpfc_io_buf *
lpfc_get_nvme_buf(struct lpfc_hba *phba, struct lpfc_nodelist *ndlp,
		  int idx, int expedite)
{
	struct lpfc_io_buf *lpfc_ncmd;
	struct lpfc_sli4_hdw_queue *qp;
	struct sli4_sge *sgl;
	struct lpfc_iocbq *pwqeq;
	union lpfc_wqe128 *wqe;

	lpfc_ncmd = lpfc_get_io_buf(phba, NULL, idx, expedite);

	if (lpfc_ncmd) {
		pwqeq = &(lpfc_ncmd->cur_iocbq);
		wqe = &pwqeq->wqe;

		 
		pwqeq->cmd_flag = LPFC_IO_NVME;
		pwqeq->cmd_cmpl = lpfc_nvme_io_cmd_cmpl;
		lpfc_ncmd->start_time = jiffies;
		lpfc_ncmd->flags = 0;

		 
		sgl = lpfc_ncmd->dma_sgl;
		bf_set(lpfc_sli4_sge_type, sgl, LPFC_SGE_TYPE_SKIP);
		bf_set(lpfc_sli4_sge_last, sgl, 0);
		sgl->word2 = cpu_to_le32(sgl->word2);
		 

		 
		memset(wqe, 0, sizeof(union lpfc_wqe));

		if (lpfc_ndlp_check_qdepth(phba, ndlp)) {
			atomic_inc(&ndlp->cmd_pending);
			lpfc_ncmd->flags |= LPFC_SBUF_BUMP_QDEPTH;
		}

	} else {
		qp = &phba->sli4_hba.hdwq[idx];
		qp->empty_io_bufs++;
	}

	return  lpfc_ncmd;
}

 
static void
lpfc_release_nvme_buf(struct lpfc_hba *phba, struct lpfc_io_buf *lpfc_ncmd)
{
	struct lpfc_sli4_hdw_queue *qp;
	unsigned long iflag = 0;

	if ((lpfc_ncmd->flags & LPFC_SBUF_BUMP_QDEPTH) && lpfc_ncmd->ndlp)
		atomic_dec(&lpfc_ncmd->ndlp->cmd_pending);

	lpfc_ncmd->ndlp = NULL;
	lpfc_ncmd->flags &= ~LPFC_SBUF_BUMP_QDEPTH;

	qp = lpfc_ncmd->hdwq;
	if (unlikely(lpfc_ncmd->flags & LPFC_SBUF_XBUSY)) {
		lpfc_printf_log(phba, KERN_INFO, LOG_NVME_ABTS,
				"6310 XB release deferred for "
				"ox_id x%x on reqtag x%x\n",
				lpfc_ncmd->cur_iocbq.sli4_xritag,
				lpfc_ncmd->cur_iocbq.iotag);

		spin_lock_irqsave(&qp->abts_io_buf_list_lock, iflag);
		list_add_tail(&lpfc_ncmd->list,
			&qp->lpfc_abts_io_buf_list);
		qp->abts_nvme_io_bufs++;
		spin_unlock_irqrestore(&qp->abts_io_buf_list_lock, iflag);
	} else
		lpfc_release_io_buf(phba, (struct lpfc_io_buf *)lpfc_ncmd, qp);
}

 
int
lpfc_nvme_create_localport(struct lpfc_vport *vport)
{
	int ret = 0;
	struct lpfc_hba  *phba = vport->phba;
	struct nvme_fc_port_info nfcp_info;
	struct nvme_fc_local_port *localport;
	struct lpfc_nvme_lport *lport;

	 
	memset(&nfcp_info, 0, sizeof(struct nvme_fc_port_info));
	nfcp_info.port_role = FC_PORT_ROLE_NVME_INITIATOR;
	nfcp_info.node_name = wwn_to_u64(vport->fc_nodename.u.wwn);
	nfcp_info.port_name = wwn_to_u64(vport->fc_portname.u.wwn);

	 
	lpfc_nvme_template.max_sgl_segments = phba->cfg_nvme_seg_cnt + 1;

	 
	lpfc_nvme_template.max_hw_queues = phba->cfg_hdw_queue;

	if (!IS_ENABLED(CONFIG_NVME_FC))
		return ret;

	 

	ret = nvme_fc_register_localport(&nfcp_info, &lpfc_nvme_template,
					 &vport->phba->pcidev->dev, &localport);
	if (!ret) {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME | LOG_NVME_DISC,
				 "6005 Successfully registered local "
				 "NVME port num %d, localP x%px, private "
				 "x%px, sg_seg %d\n",
				 localport->port_num, localport,
				 localport->private,
				 lpfc_nvme_template.max_sgl_segments);

		 
		lport = (struct lpfc_nvme_lport *)localport->private;
		vport->localport = localport;
		lport->vport = vport;
		vport->nvmei_support = 1;

		atomic_set(&lport->xmt_fcp_noxri, 0);
		atomic_set(&lport->xmt_fcp_bad_ndlp, 0);
		atomic_set(&lport->xmt_fcp_qdepth, 0);
		atomic_set(&lport->xmt_fcp_err, 0);
		atomic_set(&lport->xmt_fcp_wqerr, 0);
		atomic_set(&lport->xmt_fcp_abort, 0);
		atomic_set(&lport->xmt_ls_abort, 0);
		atomic_set(&lport->xmt_ls_err, 0);
		atomic_set(&lport->cmpl_fcp_xb, 0);
		atomic_set(&lport->cmpl_fcp_err, 0);
		atomic_set(&lport->cmpl_ls_xb, 0);
		atomic_set(&lport->cmpl_ls_err, 0);

		atomic_set(&lport->fc4NvmeLsRequests, 0);
		atomic_set(&lport->fc4NvmeLsCmpls, 0);
	}

	return ret;
}

#if (IS_ENABLED(CONFIG_NVME_FC))
 
static void
lpfc_nvme_lport_unreg_wait(struct lpfc_vport *vport,
			   struct lpfc_nvme_lport *lport,
			   struct completion *lport_unreg_cmp)
{
	u32 wait_tmo;
	int ret, i, pending = 0;
	struct lpfc_sli_ring  *pring;
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_sli4_hdw_queue *qp;
	int abts_scsi, abts_nvme;

	 
	wait_tmo = msecs_to_jiffies(LPFC_NVME_WAIT_TMO * 1000);
	while (true) {
		ret = wait_for_completion_timeout(lport_unreg_cmp, wait_tmo);
		if (unlikely(!ret)) {
			pending = 0;
			abts_scsi = 0;
			abts_nvme = 0;
			for (i = 0; i < phba->cfg_hdw_queue; i++) {
				qp = &phba->sli4_hba.hdwq[i];
				if (!vport->localport || !qp || !qp->io_wq)
					return;

				pring = qp->io_wq->pring;
				if (!pring)
					continue;
				pending += pring->txcmplq_cnt;
				abts_scsi += qp->abts_scsi_io_bufs;
				abts_nvme += qp->abts_nvme_io_bufs;
			}
			if (!vport->localport ||
			    test_bit(HBA_PCI_ERR, &vport->phba->bit_flags) ||
			    phba->link_state == LPFC_HBA_ERROR ||
			    vport->load_flag & FC_UNLOADING)
				return;

			lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
					 "6176 Lport x%px Localport x%px wait "
					 "timed out. Pending %d [%d:%d]. "
					 "Renewing.\n",
					 lport, vport->localport, pending,
					 abts_scsi, abts_nvme);
			continue;
		}
		break;
	}
	lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_IOERR,
			 "6177 Lport x%px Localport x%px Complete Success\n",
			 lport, vport->localport);
}
#endif

 
void
lpfc_nvme_destroy_localport(struct lpfc_vport *vport)
{
#if (IS_ENABLED(CONFIG_NVME_FC))
	struct nvme_fc_local_port *localport;
	struct lpfc_nvme_lport *lport;
	int ret;
	DECLARE_COMPLETION_ONSTACK(lport_unreg_cmp);

	if (vport->nvmei_support == 0)
		return;

	localport = vport->localport;
	if (!localport)
		return;
	lport = (struct lpfc_nvme_lport *)localport->private;

	lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME,
			 "6011 Destroying NVME localport x%px\n",
			 localport);

	 
	lport->lport_unreg_cmp = &lport_unreg_cmp;
	ret = nvme_fc_unregister_localport(localport);

	 
	lpfc_nvme_lport_unreg_wait(vport, lport, &lport_unreg_cmp);
	vport->localport = NULL;

	 
	vport->nvmei_support = 0;
	if (ret == 0) {
		lpfc_printf_vlog(vport,
				 KERN_INFO, LOG_NVME_DISC,
				 "6009 Unregistered lport Success\n");
	} else {
		lpfc_printf_vlog(vport,
				 KERN_INFO, LOG_NVME_DISC,
				 "6010 Unregistered lport "
				 "Failed, status x%x\n",
				 ret);
	}
#endif
}

void
lpfc_nvme_update_localport(struct lpfc_vport *vport)
{
#if (IS_ENABLED(CONFIG_NVME_FC))
	struct nvme_fc_local_port *localport;
	struct lpfc_nvme_lport *lport;

	localport = vport->localport;
	if (!localport) {
		lpfc_printf_vlog(vport, KERN_WARNING, LOG_NVME,
				 "6710 Update NVME fail. No localport\n");
		return;
	}
	lport = (struct lpfc_nvme_lport *)localport->private;
	if (!lport) {
		lpfc_printf_vlog(vport, KERN_WARNING, LOG_NVME,
				 "6171 Update NVME fail. localP x%px, No lport\n",
				 localport);
		return;
	}
	lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME,
			 "6012 Update NVME lport x%px did x%x\n",
			 localport, vport->fc_myDID);

	localport->port_id = vport->fc_myDID;
	if (localport->port_id == 0)
		localport->port_role = FC_PORT_ROLE_NVME_DISCOVERY;
	else
		localport->port_role = FC_PORT_ROLE_NVME_INITIATOR;

	lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_DISC,
			 "6030 bound lport x%px to DID x%06x\n",
			 lport, localport->port_id);
#endif
}

int
lpfc_nvme_register_port(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
#if (IS_ENABLED(CONFIG_NVME_FC))
	int ret = 0;
	struct nvme_fc_local_port *localport;
	struct lpfc_nvme_lport *lport;
	struct lpfc_nvme_rport *rport;
	struct lpfc_nvme_rport *oldrport;
	struct nvme_fc_remote_port *remote_port;
	struct nvme_fc_port_info rpinfo;
	struct lpfc_nodelist *prev_ndlp = NULL;
	struct fc_rport *srport = ndlp->rport;

	lpfc_printf_vlog(ndlp->vport, KERN_INFO, LOG_NVME_DISC,
			 "6006 Register NVME PORT. DID x%06x nlptype x%x\n",
			 ndlp->nlp_DID, ndlp->nlp_type);

	localport = vport->localport;
	if (!localport)
		return 0;

	lport = (struct lpfc_nvme_lport *)localport->private;

	 
	memset(&rpinfo, 0, sizeof(struct nvme_fc_port_info));
	rpinfo.port_id = ndlp->nlp_DID;
	if (ndlp->nlp_type & NLP_NVME_TARGET)
		rpinfo.port_role |= FC_PORT_ROLE_NVME_TARGET;
	if (ndlp->nlp_type & NLP_NVME_INITIATOR)
		rpinfo.port_role |= FC_PORT_ROLE_NVME_INITIATOR;

	if (ndlp->nlp_type & NLP_NVME_DISCOVERY)
		rpinfo.port_role |= FC_PORT_ROLE_NVME_DISCOVERY;

	rpinfo.port_name = wwn_to_u64(ndlp->nlp_portname.u.wwn);
	rpinfo.node_name = wwn_to_u64(ndlp->nlp_nodename.u.wwn);
	if (srport)
		rpinfo.dev_loss_tmo = srport->dev_loss_tmo;
	else
		rpinfo.dev_loss_tmo = vport->cfg_devloss_tmo;

	spin_lock_irq(&ndlp->lock);

	 
	oldrport = lpfc_ndlp_get_nrport(ndlp);
	if (oldrport) {
		prev_ndlp = oldrport->ndlp;
		spin_unlock_irq(&ndlp->lock);
	} else {
		spin_unlock_irq(&ndlp->lock);
		if (!lpfc_nlp_get(ndlp)) {
			dev_warn(&vport->phba->pcidev->dev,
				 "Warning - No node ref - exit register\n");
			return 0;
		}
	}

	ret = nvme_fc_register_remoteport(localport, &rpinfo, &remote_port);
	if (!ret) {
		 
		 
		spin_lock_irq(&ndlp->lock);
		ndlp->fc4_xpt_flags &= ~NVME_XPT_UNREG_WAIT;
		ndlp->fc4_xpt_flags |= NVME_XPT_REGD;
		spin_unlock_irq(&ndlp->lock);
		rport = remote_port->private;
		if (oldrport) {

			 
			spin_lock_irq(&ndlp->lock);
			ndlp->nrport = NULL;
			ndlp->fc4_xpt_flags &= ~NVME_XPT_UNREG_WAIT;
			spin_unlock_irq(&ndlp->lock);
			rport->ndlp = NULL;
			rport->remoteport = NULL;

			 
			if (prev_ndlp && prev_ndlp != ndlp) {
				if (!prev_ndlp->nrport)
					lpfc_nlp_put(prev_ndlp);
			}
		}

		 
		rport->remoteport = remote_port;
		rport->lport = lport;
		rport->ndlp = ndlp;
		spin_lock_irq(&ndlp->lock);
		ndlp->nrport = rport;
		spin_unlock_irq(&ndlp->lock);
		lpfc_printf_vlog(vport, KERN_INFO,
				 LOG_NVME_DISC | LOG_NODE,
				 "6022 Bind lport x%px to remoteport x%px "
				 "rport x%px WWNN 0x%llx, "
				 "Rport WWPN 0x%llx DID "
				 "x%06x Role x%x, ndlp %p prev_ndlp x%px\n",
				 lport, remote_port, rport,
				 rpinfo.node_name, rpinfo.port_name,
				 rpinfo.port_id, rpinfo.port_role,
				 ndlp, prev_ndlp);
	} else {
		lpfc_printf_vlog(vport, KERN_ERR,
				 LOG_TRACE_EVENT,
				 "6031 RemotePort Registration failed "
				 "err: %d, DID x%06x ref %u\n",
				 ret, ndlp->nlp_DID, kref_read(&ndlp->kref));
		lpfc_nlp_put(ndlp);
	}

	return ret;
#else
	return 0;
#endif
}

 
void
lpfc_nvme_rescan_port(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
#if (IS_ENABLED(CONFIG_NVME_FC))
	struct lpfc_nvme_rport *nrport;
	struct nvme_fc_remote_port *remoteport = NULL;

	spin_lock_irq(&ndlp->lock);
	nrport = lpfc_ndlp_get_nrport(ndlp);
	if (nrport)
		remoteport = nrport->remoteport;
	spin_unlock_irq(&ndlp->lock);

	lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_DISC,
			 "6170 Rescan NPort DID x%06x type x%x "
			 "state x%x nrport x%px remoteport x%px\n",
			 ndlp->nlp_DID, ndlp->nlp_type, ndlp->nlp_state,
			 nrport, remoteport);

	if (!nrport || !remoteport)
		goto rescan_exit;

	 
	if (remoteport->port_role & FC_PORT_ROLE_NVME_DISCOVERY &&
	    ndlp->nlp_state == NLP_STE_MAPPED_NODE) {
		nvme_fc_rescan_remoteport(remoteport);

		lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_DISC,
				 "6172 NVME rescanned DID x%06x "
				 "port_state x%x\n",
				 ndlp->nlp_DID, remoteport->port_state);
	}
	return;
 rescan_exit:
	lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_DISC,
			 "6169 Skip NVME Rport Rescan, NVME remoteport "
			 "unregistered\n");
#endif
}

 
void
lpfc_nvme_unregister_port(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
#if (IS_ENABLED(CONFIG_NVME_FC))
	int ret;
	struct nvme_fc_local_port *localport;
	struct lpfc_nvme_lport *lport;
	struct lpfc_nvme_rport *rport;
	struct nvme_fc_remote_port *remoteport = NULL;

	localport = vport->localport;

	 
	if (!localport)
		return;

	lport = (struct lpfc_nvme_lport *)localport->private;
	if (!lport)
		goto input_err;

	spin_lock_irq(&ndlp->lock);
	rport = lpfc_ndlp_get_nrport(ndlp);
	if (rport)
		remoteport = rport->remoteport;
	spin_unlock_irq(&ndlp->lock);
	if (!remoteport)
		goto input_err;

	lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_DISC,
			 "6033 Unreg nvme remoteport x%px, portname x%llx, "
			 "port_id x%06x, portstate x%x port type x%x "
			 "refcnt %d\n",
			 remoteport, remoteport->port_name,
			 remoteport->port_id, remoteport->port_state,
			 ndlp->nlp_type, kref_read(&ndlp->kref));

	 

	if (ndlp->nlp_type & NLP_NVME_TARGET) {
		 
		spin_lock_irq(&vport->phba->hbalock);
		ndlp->fc4_xpt_flags |= NVME_XPT_UNREG_WAIT;
		spin_unlock_irq(&vport->phba->hbalock);

		 
		if (vport->load_flag & FC_UNLOADING ||
		    unlikely(vport->phba->link_state == LPFC_HBA_ERROR))
			(void)nvme_fc_set_remoteport_devloss(remoteport, 0);

		ret = nvme_fc_unregister_remoteport(remoteport);

		 
		ndlp->nrport = NULL;
		lpfc_nlp_put(ndlp);
		if (ret != 0) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
					 "6167 NVME unregister failed %d "
					 "port_state x%x\n",
					 ret, remoteport->port_state);

			if (vport->load_flag & FC_UNLOADING) {
				 
				spin_lock_irq(&ndlp->lock);
				if (!(ndlp->nlp_flag & NLP_DROPPED)) {
					ndlp->nlp_flag |= NLP_DROPPED;
					spin_unlock_irq(&ndlp->lock);
					lpfc_nlp_put(ndlp);
					return;
				}
				spin_unlock_irq(&ndlp->lock);
			}
		}
	}
	return;

 input_err:
#endif
	lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
			 "6168 State error: lport x%px, rport x%px FCID x%06x\n",
			 vport->localport, ndlp->rport, ndlp->nlp_DID);
}

 
void
lpfc_sli4_nvme_pci_offline_aborted(struct lpfc_hba *phba,
				   struct lpfc_io_buf *lpfc_ncmd)
{
	struct nvmefc_fcp_req *nvme_cmd = NULL;

	lpfc_printf_log(phba, KERN_INFO, LOG_NVME_ABTS,
			"6533 %s nvme_cmd %p tag x%x abort complete and "
			"xri released\n", __func__,
			lpfc_ncmd->nvmeCmd,
			lpfc_ncmd->cur_iocbq.iotag);

	 
	if (lpfc_ncmd->nvmeCmd) {
		nvme_cmd = lpfc_ncmd->nvmeCmd;
		nvme_cmd->transferred_length = 0;
		nvme_cmd->rcv_rsplen = 0;
		nvme_cmd->status = NVME_SC_INTERNAL;
		nvme_cmd->done(nvme_cmd);
		lpfc_ncmd->nvmeCmd = NULL;
	}
	lpfc_release_nvme_buf(phba, lpfc_ncmd);
}

 
void
lpfc_sli4_nvme_xri_aborted(struct lpfc_hba *phba,
			   struct sli4_wcqe_xri_aborted *axri,
			   struct lpfc_io_buf *lpfc_ncmd)
{
	uint16_t xri = bf_get(lpfc_wcqe_xa_xri, axri);
	struct nvmefc_fcp_req *nvme_cmd = NULL;
	struct lpfc_nodelist *ndlp = lpfc_ncmd->ndlp;


	if (ndlp)
		lpfc_sli4_abts_err_handler(phba, ndlp, axri);

	lpfc_printf_log(phba, KERN_INFO, LOG_NVME_ABTS,
			"6311 nvme_cmd %p xri x%x tag x%x abort complete and "
			"xri released\n",
			lpfc_ncmd->nvmeCmd, xri,
			lpfc_ncmd->cur_iocbq.iotag);

	 
	if (lpfc_ncmd->nvmeCmd) {
		nvme_cmd = lpfc_ncmd->nvmeCmd;
		nvme_cmd->done(nvme_cmd);
		lpfc_ncmd->nvmeCmd = NULL;
	}
	lpfc_release_nvme_buf(phba, lpfc_ncmd);
}

 
void
lpfc_nvme_wait_for_io_drain(struct lpfc_hba *phba)
{
	struct lpfc_sli_ring  *pring;
	u32 i, wait_cnt = 0;

	if (phba->sli_rev < LPFC_SLI_REV4 || !phba->sli4_hba.hdwq)
		return;

	 
	for (i = 0; i < phba->cfg_hdw_queue; i++) {
		if (!phba->sli4_hba.hdwq[i].io_wq)
			continue;
		pring = phba->sli4_hba.hdwq[i].io_wq->pring;

		if (!pring)
			continue;

		 
		while (!list_empty(&pring->txcmplq)) {
			msleep(LPFC_XRI_EXCH_BUSY_WAIT_T1);
			wait_cnt++;

			 
			if ((wait_cnt % 1000) == 0) {
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"6178 NVME IO not empty, "
						"cnt %d\n", wait_cnt);
			}
		}
	}

	 
	lpfc_issue_hb_tmo(phba);

}

void
lpfc_nvme_cancel_iocb(struct lpfc_hba *phba, struct lpfc_iocbq *pwqeIn,
		      uint32_t stat, uint32_t param)
{
#if (IS_ENABLED(CONFIG_NVME_FC))
	struct lpfc_io_buf *lpfc_ncmd;
	struct nvmefc_fcp_req *nCmd;
	struct lpfc_wcqe_complete wcqe;
	struct lpfc_wcqe_complete *wcqep = &wcqe;

	lpfc_ncmd = pwqeIn->io_buf;
	if (!lpfc_ncmd) {
		lpfc_sli_release_iocbq(phba, pwqeIn);
		return;
	}
	 
	if (bf_get(wqe_cmnd, &pwqeIn->wqe.gen_req.wqe_com) ==
	    CMD_ABORT_XRI_CX) {
		lpfc_sli_release_iocbq(phba, pwqeIn);
		return;
	}

	spin_lock(&lpfc_ncmd->buf_lock);
	nCmd = lpfc_ncmd->nvmeCmd;
	if (!nCmd) {
		spin_unlock(&lpfc_ncmd->buf_lock);
		lpfc_release_nvme_buf(phba, lpfc_ncmd);
		return;
	}
	spin_unlock(&lpfc_ncmd->buf_lock);

	lpfc_printf_log(phba, KERN_INFO, LOG_NVME_IOERR,
			"6194 NVME Cancel xri %x\n",
			lpfc_ncmd->cur_iocbq.sli4_xritag);

	wcqep->word0 = 0;
	bf_set(lpfc_wcqe_c_status, wcqep, stat);
	wcqep->parameter = param;
	wcqep->total_data_placed = 0;
	wcqep->word3 = 0;  

	 
	if (phba->sli.sli_flag & LPFC_SLI_ACTIVE)
		bf_set(lpfc_wcqe_c_xb, wcqep, 1);

	memcpy(&pwqeIn->wcqe_cmpl, wcqep, sizeof(*wcqep));
	(pwqeIn->cmd_cmpl)(phba, pwqeIn, pwqeIn);
#endif
}
