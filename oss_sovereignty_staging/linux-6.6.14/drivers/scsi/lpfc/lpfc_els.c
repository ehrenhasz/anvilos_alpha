 
 
#include <linux/blkdev.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_fc.h>
#include <uapi/scsi/fc/fc_fs.h>
#include <uapi/scsi/fc/fc_els.h>

#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_nl.h"
#include "lpfc_disc.h"
#include "lpfc_scsi.h"
#include "lpfc.h"
#include "lpfc_logmsg.h"
#include "lpfc_crtn.h"
#include "lpfc_vport.h"
#include "lpfc_debugfs.h"

static int lpfc_els_retry(struct lpfc_hba *, struct lpfc_iocbq *,
			  struct lpfc_iocbq *);
static void lpfc_cmpl_fabric_iocb(struct lpfc_hba *, struct lpfc_iocbq *,
			struct lpfc_iocbq *);
static void lpfc_fabric_abort_vport(struct lpfc_vport *vport);
static int lpfc_issue_els_fdisc(struct lpfc_vport *vport,
				struct lpfc_nodelist *ndlp, uint8_t retry);
static int lpfc_issue_fabric_iocb(struct lpfc_hba *phba,
				  struct lpfc_iocbq *iocb);
static void lpfc_cmpl_els_edc(struct lpfc_hba *phba,
			      struct lpfc_iocbq *cmdiocb,
			      struct lpfc_iocbq *rspiocb);
static void lpfc_cmpl_els_uvem(struct lpfc_hba *, struct lpfc_iocbq *,
			       struct lpfc_iocbq *);

static int lpfc_max_els_tries = 3;

static void lpfc_init_cs_ctl_bitmap(struct lpfc_vport *vport);
static void lpfc_vmid_set_cs_ctl_range(struct lpfc_vport *vport, u32 min, u32 max);
static void lpfc_vmid_put_cs_ctl(struct lpfc_vport *vport, u32 ctcl_vmid);

 
int
lpfc_els_chk_latt(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	uint32_t ha_copy;

	if (vport->port_state >= LPFC_VPORT_READY ||
	    phba->link_state == LPFC_LINK_DOWN ||
	    phba->sli_rev > LPFC_SLI_REV3)
		return 0;

	 
	if (lpfc_readl(phba->HAregaddr, &ha_copy))
		return 1;

	if (!(ha_copy & HA_LATT))
		return 0;

	 
	lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
			 "0237 Pending Link Event during "
			 "Discovery: State x%x\n",
			 phba->pport->port_state);

	 
	spin_lock_irq(shost->host_lock);
	vport->fc_flag |= FC_ABORT_DISCOVERY;
	spin_unlock_irq(shost->host_lock);

	if (phba->link_state != LPFC_CLEAR_LA)
		lpfc_issue_clear_la(phba, vport);

	return 1;
}

 
struct lpfc_iocbq *
lpfc_prep_els_iocb(struct lpfc_vport *vport, u8 expect_rsp,
		   u16 cmd_size, u8 retry,
		   struct lpfc_nodelist *ndlp, u32 did,
		   u32 elscmd)
{
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_dmabuf *pcmd, *prsp, *pbuflist, *bmp;
	struct ulp_bde64_le *bpl;
	u32 timeout = 0;

	if (!lpfc_is_link_up(phba))
		return NULL;

	 
	elsiocb = lpfc_sli_get_iocbq(phba);
	if (!elsiocb)
		return NULL;

	 
	if ((did == Fabric_DID) &&
	    (phba->hba_flag & HBA_FIP_SUPPORT) &&
	    ((elscmd == ELS_CMD_FLOGI) ||
	     (elscmd == ELS_CMD_FDISC) ||
	     (elscmd == ELS_CMD_LOGO)))
		switch (elscmd) {
		case ELS_CMD_FLOGI:
			elsiocb->cmd_flag |=
				((LPFC_ELS_ID_FLOGI << LPFC_FIP_ELS_ID_SHIFT)
				 & LPFC_FIP_ELS_ID_MASK);
			break;
		case ELS_CMD_FDISC:
			elsiocb->cmd_flag |=
				((LPFC_ELS_ID_FDISC << LPFC_FIP_ELS_ID_SHIFT)
				 & LPFC_FIP_ELS_ID_MASK);
			break;
		case ELS_CMD_LOGO:
			elsiocb->cmd_flag |=
				((LPFC_ELS_ID_LOGO << LPFC_FIP_ELS_ID_SHIFT)
				 & LPFC_FIP_ELS_ID_MASK);
			break;
		}
	else
		elsiocb->cmd_flag &= ~LPFC_FIP_ELS_ID_MASK;

	 
	 
	pcmd = kmalloc(sizeof(*pcmd), GFP_KERNEL);
	if (pcmd)
		pcmd->virt = lpfc_mbuf_alloc(phba, MEM_PRI, &pcmd->phys);
	if (!pcmd || !pcmd->virt)
		goto els_iocb_free_pcmb_exit;

	INIT_LIST_HEAD(&pcmd->list);

	 
	if (expect_rsp) {
		prsp = kmalloc(sizeof(*prsp), GFP_KERNEL);
		if (prsp)
			prsp->virt = lpfc_mbuf_alloc(phba, MEM_PRI,
						     &prsp->phys);
		if (!prsp || !prsp->virt)
			goto els_iocb_free_prsp_exit;
		INIT_LIST_HEAD(&prsp->list);
	} else {
		prsp = NULL;
	}

	 
	pbuflist = kmalloc(sizeof(*pbuflist), GFP_KERNEL);
	if (pbuflist)
		pbuflist->virt = lpfc_mbuf_alloc(phba, MEM_PRI,
						 &pbuflist->phys);
	if (!pbuflist || !pbuflist->virt)
		goto els_iocb_free_pbuf_exit;

	INIT_LIST_HEAD(&pbuflist->list);

	if (expect_rsp) {
		switch (elscmd) {
		case ELS_CMD_FLOGI:
			timeout = FF_DEF_RATOV * 2;
			break;
		case ELS_CMD_LOGO:
			timeout = phba->fc_ratov;
			break;
		default:
			timeout = phba->fc_ratov * 2;
		}

		 
		elsiocb->num_bdes = 2;
	}

	if (phba->sli_rev == LPFC_SLI_REV4)
		bmp = pcmd;
	else
		bmp = pbuflist;

	lpfc_sli_prep_els_req_rsp(phba, elsiocb, vport, bmp, cmd_size, did,
				  elscmd, timeout, expect_rsp);

	bpl = (struct ulp_bde64_le *)pbuflist->virt;
	bpl->addr_low = cpu_to_le32(putPaddrLow(pcmd->phys));
	bpl->addr_high = cpu_to_le32(putPaddrHigh(pcmd->phys));
	bpl->type_size = cpu_to_le32(cmd_size);
	bpl->type_size |= cpu_to_le32(ULP_BDE64_TYPE_BDE_64);

	if (expect_rsp) {
		bpl++;
		bpl->addr_low = cpu_to_le32(putPaddrLow(prsp->phys));
		bpl->addr_high = cpu_to_le32(putPaddrHigh(prsp->phys));
		bpl->type_size = cpu_to_le32(FCELSSIZE);
		bpl->type_size |= cpu_to_le32(ULP_BDE64_TYPE_BDE_64);
	}

	elsiocb->cmd_dmabuf = pcmd;
	elsiocb->bpl_dmabuf = pbuflist;
	elsiocb->retry = retry;
	elsiocb->vport = vport;
	elsiocb->drvrTimeout = (phba->fc_ratov << 1) + LPFC_DRVR_TIMEOUT;

	if (prsp)
		list_add(&prsp->list, &pcmd->list);
	if (expect_rsp) {
		 
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
				 "0116 Xmit ELS command x%x to remote "
				 "NPORT x%x I/O tag: x%x, port state:x%x "
				 "rpi x%x fc_flag:x%x\n",
				 elscmd, did, elsiocb->iotag,
				 vport->port_state, ndlp->nlp_rpi,
				 vport->fc_flag);
	} else {
		 
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
				 "0117 Xmit ELS response x%x to remote "
				 "NPORT x%x I/O tag: x%x, size: x%x "
				 "port_state x%x  rpi x%x fc_flag x%x\n",
				 elscmd, ndlp->nlp_DID, elsiocb->iotag,
				 cmd_size, vport->port_state,
				 ndlp->nlp_rpi, vport->fc_flag);
	}

	return elsiocb;

els_iocb_free_pbuf_exit:
	if (expect_rsp)
		lpfc_mbuf_free(phba, prsp->virt, prsp->phys);
	kfree(pbuflist);

els_iocb_free_prsp_exit:
	lpfc_mbuf_free(phba, pcmd->virt, pcmd->phys);
	kfree(prsp);

els_iocb_free_pcmb_exit:
	kfree(pcmd);
	lpfc_sli_release_iocbq(phba, elsiocb);
	return NULL;
}

 
int
lpfc_issue_fabric_reglogin(struct lpfc_vport *vport)
{
	struct lpfc_hba  *phba = vport->phba;
	LPFC_MBOXQ_t *mbox;
	struct lpfc_nodelist *ndlp;
	struct serv_parm *sp;
	int rc;
	int err = 0;

	sp = &phba->fc_fabparam;
	ndlp = lpfc_findnode_did(vport, Fabric_DID);
	if (!ndlp) {
		err = 1;
		goto fail;
	}

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		err = 2;
		goto fail;
	}

	vport->port_state = LPFC_FABRIC_CFG_LINK;
	lpfc_config_link(phba, mbox);
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	mbox->vport = vport;

	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		err = 3;
		goto fail_free_mbox;
	}

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		err = 4;
		goto fail;
	}
	rc = lpfc_reg_rpi(phba, vport->vpi, Fabric_DID, (uint8_t *)sp, mbox,
			  ndlp->nlp_rpi);
	if (rc) {
		err = 5;
		goto fail_free_mbox;
	}

	mbox->mbox_cmpl = lpfc_mbx_cmpl_fabric_reg_login;
	mbox->vport = vport;
	 
	mbox->ctx_ndlp = lpfc_nlp_get(ndlp);
	if (!mbox->ctx_ndlp) {
		err = 6;
		goto fail_free_mbox;
	}

	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		err = 7;
		goto fail_issue_reg_login;
	}

	return 0;

fail_issue_reg_login:
	 
	lpfc_nlp_put(ndlp);
fail_free_mbox:
	lpfc_mbox_rsrc_cleanup(phba, mbox, MBOX_THD_UNLOCKED);
fail:
	lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
			 "0249 Cannot issue Register Fabric login: Err %d\n",
			 err);
	return -ENXIO;
}

 
int
lpfc_issue_reg_vfi(struct lpfc_vport *vport)
{
	struct lpfc_hba  *phba = vport->phba;
	LPFC_MBOXQ_t *mboxq = NULL;
	struct lpfc_nodelist *ndlp;
	struct lpfc_dmabuf *dmabuf = NULL;
	int rc = 0;

	 
	if ((phba->sli_rev == LPFC_SLI_REV4) &&
	    !(phba->link_flag & LS_LOOPBACK_MODE) &&
	    !(vport->fc_flag & FC_PT2PT)) {
		ndlp = lpfc_findnode_did(vport, Fabric_DID);
		if (!ndlp) {
			rc = -ENODEV;
			goto fail;
		}
	}

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		rc = -ENOMEM;
		goto fail;
	}

	 
	if ((vport->fc_flag & FC_FABRIC) || (vport->fc_flag & FC_PT2PT)) {
		rc = lpfc_mbox_rsrc_prep(phba, mboxq);
		if (rc) {
			rc = -ENOMEM;
			goto fail_mbox;
		}
		dmabuf = mboxq->ctx_buf;
		memcpy(dmabuf->virt, &phba->fc_fabparam,
		       sizeof(struct serv_parm));
	}

	vport->port_state = LPFC_FABRIC_CFG_LINK;
	if (dmabuf) {
		lpfc_reg_vfi(mboxq, vport, dmabuf->phys);
		 
		mboxq->ctx_buf = dmabuf;
	} else {
		lpfc_reg_vfi(mboxq, vport, 0);
	}

	mboxq->mbox_cmpl = lpfc_mbx_cmpl_reg_vfi;
	mboxq->vport = vport;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		rc = -ENXIO;
		goto fail_mbox;
	}
	return 0;

fail_mbox:
	lpfc_mbox_rsrc_cleanup(phba, mboxq, MBOX_THD_UNLOCKED);
fail:
	lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
			 "0289 Issue Register VFI failed: Err %d\n", rc);
	return rc;
}

 
int
lpfc_issue_unreg_vfi(struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;
	struct Scsi_Host *shost;
	LPFC_MBOXQ_t *mboxq;
	int rc;

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2556 UNREG_VFI mbox allocation failed"
				"HBA state x%x\n", phba->pport->port_state);
		return -ENOMEM;
	}

	lpfc_unreg_vfi(mboxq, vport);
	mboxq->vport = vport;
	mboxq->mbox_cmpl = lpfc_unregister_vfi_cmpl;

	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2557 UNREG_VFI issue mbox failed rc x%x "
				"HBA state x%x\n",
				rc, phba->pport->port_state);
		mempool_free(mboxq, phba->mbox_mem_pool);
		return -EIO;
	}

	shost = lpfc_shost_from_vport(vport);
	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~FC_VFI_REGISTERED;
	spin_unlock_irq(shost->host_lock);
	return 0;
}

 
static uint8_t
lpfc_check_clean_addr_bit(struct lpfc_vport *vport,
		struct serv_parm *sp)
{
	struct lpfc_hba *phba = vport->phba;
	uint8_t fabric_param_changed = 0;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	if ((vport->fc_prevDID != vport->fc_myDID) ||
		memcmp(&vport->fabric_portname, &sp->portName,
			sizeof(struct lpfc_name)) ||
		memcmp(&vport->fabric_nodename, &sp->nodeName,
			sizeof(struct lpfc_name)) ||
		(vport->vport_flag & FAWWPN_PARAM_CHG)) {
		fabric_param_changed = 1;
		vport->vport_flag &= ~FAWWPN_PARAM_CHG;
	}
	 
	if (fabric_param_changed && !sp->cmn.clean_address_bit &&
	    (vport->fc_prevDID || phba->cfg_delay_discovery)) {
		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_DISC_DELAYED;
		spin_unlock_irq(shost->host_lock);
	}

	return fabric_param_changed;
}


 
static int
lpfc_cmpl_els_flogi_fabric(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
			   struct serv_parm *sp, uint32_t ulp_word4)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_nodelist *np;
	struct lpfc_nodelist *next_np;
	uint8_t fabric_param_changed;

	spin_lock_irq(shost->host_lock);
	vport->fc_flag |= FC_FABRIC;
	spin_unlock_irq(shost->host_lock);

	phba->fc_edtov = be32_to_cpu(sp->cmn.e_d_tov);
	if (sp->cmn.edtovResolution)	 
		phba->fc_edtov = (phba->fc_edtov + 999999) / 1000000;

	phba->fc_edtovResol = sp->cmn.edtovResolution;
	phba->fc_ratov = (be32_to_cpu(sp->cmn.w2.r_a_tov) + 999) / 1000;

	if (phba->fc_topology == LPFC_TOPOLOGY_LOOP) {
		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_PUBLIC_LOOP;
		spin_unlock_irq(shost->host_lock);
	}

	vport->fc_myDID = ulp_word4 & Mask_DID;
	memcpy(&ndlp->nlp_portname, &sp->portName, sizeof(struct lpfc_name));
	memcpy(&ndlp->nlp_nodename, &sp->nodeName, sizeof(struct lpfc_name));
	ndlp->nlp_class_sup = 0;
	if (sp->cls1.classValid)
		ndlp->nlp_class_sup |= FC_COS_CLASS1;
	if (sp->cls2.classValid)
		ndlp->nlp_class_sup |= FC_COS_CLASS2;
	if (sp->cls3.classValid)
		ndlp->nlp_class_sup |= FC_COS_CLASS3;
	if (sp->cls4.classValid)
		ndlp->nlp_class_sup |= FC_COS_CLASS4;
	ndlp->nlp_maxframe = ((sp->cmn.bbRcvSizeMsb & 0x0F) << 8) |
				sp->cmn.bbRcvSizeLsb;

	fabric_param_changed = lpfc_check_clean_addr_bit(vport, sp);
	if (fabric_param_changed) {
		 
		if (phba->cfg_enable_SmartSAN ||
		    (phba->cfg_fdmi_on == LPFC_FDMI_SUPPORT)) {
			 
			vport->fdmi_hba_mask = LPFC_FDMI2_HBA_ATTR;
			if (phba->cfg_enable_SmartSAN)
				vport->fdmi_port_mask = LPFC_FDMI2_SMART_ATTR;
			else
				vport->fdmi_port_mask = LPFC_FDMI2_PORT_ATTR;
		} else {
			vport->fdmi_hba_mask = 0;
			vport->fdmi_port_mask = 0;
		}

	}
	memcpy(&vport->fabric_portname, &sp->portName,
			sizeof(struct lpfc_name));
	memcpy(&vport->fabric_nodename, &sp->nodeName,
			sizeof(struct lpfc_name));
	memcpy(&phba->fc_fabparam, sp, sizeof(struct serv_parm));

	if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) {
		if (sp->cmn.response_multiple_NPort) {
			lpfc_printf_vlog(vport, KERN_WARNING,
					 LOG_ELS | LOG_VPORT,
					 "1816 FLOGI NPIV supported, "
					 "response data 0x%x\n",
					 sp->cmn.response_multiple_NPort);
			spin_lock_irq(&phba->hbalock);
			phba->link_flag |= LS_NPIV_FAB_SUPPORTED;
			spin_unlock_irq(&phba->hbalock);
		} else {
			 
			lpfc_printf_vlog(vport, KERN_WARNING,
					 LOG_ELS | LOG_VPORT,
					 "1817 Fabric does not support NPIV "
					 "- configuring single port mode.\n");
			spin_lock_irq(&phba->hbalock);
			phba->link_flag &= ~LS_NPIV_FAB_SUPPORTED;
			spin_unlock_irq(&phba->hbalock);
		}
	}

	 
	if ((phba->sli_rev == LPFC_SLI_REV4) &&
	    (phba->sli4_hba.lnk_info.lnk_tp == LPFC_LNK_TYPE_FC)) {
		 
		if (fabric_param_changed)
			lpfc_unregister_fcf_prep(phba);

		 
		if (vport->fc_flag & FC_VFI_REGISTERED)
			lpfc_issue_reg_vfi(vport);
	}

	if (fabric_param_changed &&
		!(vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)) {

		 
		list_for_each_entry_safe(np, next_np,
					&vport->fc_nodes, nlp_listp) {
			if ((np->nlp_state != NLP_STE_NPR_NODE) ||
				   !(np->nlp_flag & NLP_NPR_ADISC))
				continue;
			spin_lock_irq(&np->lock);
			np->nlp_flag &= ~NLP_NPR_ADISC;
			spin_unlock_irq(&np->lock);
			lpfc_unreg_rpi(vport, np);
		}
		lpfc_cleanup_pending_mbox(vport);

		if (phba->sli_rev == LPFC_SLI_REV4) {
			lpfc_sli4_unreg_all_rpis(vport);
			lpfc_mbx_unreg_vpi(vport);
			spin_lock_irq(shost->host_lock);
			vport->fc_flag |= FC_VPORT_NEEDS_INIT_VPI;
			spin_unlock_irq(shost->host_lock);
		}

		 
		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
		spin_unlock_irq(shost->host_lock);
	} else if ((phba->sli_rev == LPFC_SLI_REV4) &&
		!(vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)) {
			 
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_UNMAPPED_NODE);
			lpfc_register_new_vport(phba, vport, ndlp);
			return 0;
	}

	if (phba->sli_rev < LPFC_SLI_REV4) {
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_REG_LOGIN_ISSUE);
		if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED &&
		    vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)
			lpfc_register_new_vport(phba, vport, ndlp);
		else
			lpfc_issue_fabric_reglogin(vport);
	} else {
		ndlp->nlp_type |= NLP_FABRIC;
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_UNMAPPED_NODE);
		if ((!(vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)) &&
			(vport->vpi_state & LPFC_VPI_REGISTERED)) {
			lpfc_start_fdiscs(phba);
			lpfc_do_scr_ns_plogi(phba, vport);
		} else if (vport->fc_flag & FC_VFI_REGISTERED)
			lpfc_issue_init_vpi(vport);
		else {
			lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
					"3135 Need register VFI: (x%x/%x)\n",
					vport->fc_prevDID, vport->fc_myDID);
			lpfc_issue_reg_vfi(vport);
		}
	}
	return 0;
}

 
static int
lpfc_cmpl_els_flogi_nport(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
			  struct serv_parm *sp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	LPFC_MBOXQ_t *mbox;
	int rc;

	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~(FC_FABRIC | FC_PUBLIC_LOOP);
	vport->fc_flag |= FC_PT2PT;
	spin_unlock_irq(shost->host_lock);

	 
	phba->sli3_options &= ~LPFC_SLI3_NPIV_ENABLED;

	 
	if ((phba->sli_rev == LPFC_SLI_REV4) && phba->fc_topology_changed) {
		lpfc_unregister_fcf_prep(phba);

		spin_lock_irq(shost->host_lock);
		vport->fc_flag &= ~FC_VFI_REGISTERED;
		spin_unlock_irq(shost->host_lock);
		phba->fc_topology_changed = 0;
	}

	rc = memcmp(&vport->fc_portname, &sp->portName,
		    sizeof(vport->fc_portname));

	if (rc >= 0) {
		 
		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_PT2PT_PLOGI;
		spin_unlock_irq(shost->host_lock);

		 

		 
		if (rc)
			vport->fc_myDID = PT2PT_LocalID;

		 
		if (!(ndlp->fc4_xpt_flags & (SCSI_XPT_REGD | NVME_XPT_REGD)))
			lpfc_nlp_put(ndlp);

		ndlp = lpfc_findnode_did(vport, PT2PT_RemoteID);
		if (!ndlp) {
			 
			ndlp = lpfc_nlp_init(vport, PT2PT_RemoteID);
			if (!ndlp)
				goto fail;
		}

		memcpy(&ndlp->nlp_portname, &sp->portName,
		       sizeof(struct lpfc_name));
		memcpy(&ndlp->nlp_nodename, &sp->nodeName,
		       sizeof(struct lpfc_name));
		 
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		spin_lock_irq(&ndlp->lock);
		ndlp->nlp_flag |= NLP_NPR_2B_DISC;
		spin_unlock_irq(&ndlp->lock);

		mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
		if (!mbox)
			goto fail;

		lpfc_config_link(phba, mbox);

		mbox->mbox_cmpl = lpfc_mbx_cmpl_local_config_link;
		mbox->vport = vport;
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
		if (rc == MBX_NOT_FINISHED) {
			mempool_free(mbox, phba->mbox_mem_pool);
			goto fail;
		}
	} else {
		 
		if (!(ndlp->fc4_xpt_flags & (SCSI_XPT_REGD | NVME_XPT_REGD)))
			lpfc_nlp_put(ndlp);

		 
		lpfc_disc_start(vport);
	}

	return 0;
fail:
	return -ENXIO;
}

 
static void
lpfc_cmpl_els_flogi(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		    struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp = cmdiocb->ndlp;
	IOCB_t *irsp;
	struct lpfc_dmabuf *pcmd = cmdiocb->cmd_dmabuf, *prsp;
	struct serv_parm *sp;
	uint16_t fcf_index;
	int rc;
	u32 ulp_status, ulp_word4, tmo;
	bool flogi_in_retry = false;

	 
	if (lpfc_els_chk_latt(vport)) {
		 
		if (!(ndlp->fc4_xpt_flags & SCSI_XPT_REGD))
			lpfc_nlp_put(ndlp);
		goto out;
	}

	ulp_status = get_job_ulpstatus(phba, rspiocb);
	ulp_word4 = get_job_word4(phba, rspiocb);

	if (phba->sli_rev == LPFC_SLI_REV4) {
		tmo = get_wqe_tmo(cmdiocb);
	} else {
		irsp = &rspiocb->iocb;
		tmo = irsp->ulpTimeout;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"FLOGI cmpl:      status:x%x/x%x state:x%x",
		ulp_status, ulp_word4,
		vport->port_state);

	if (ulp_status) {
		 
		if ((phba->hba_flag & HBA_FIP_SUPPORT) &&
		    (phba->fcf.fcf_flag & FCF_DISCOVERY)) {
			if (phba->link_state < LPFC_LINK_UP)
				goto stop_rr_fcf_flogi;
			if ((phba->fcoe_cvl_eventtag_attn ==
			     phba->fcoe_cvl_eventtag) &&
			    (ulp_status == IOSTAT_LOCAL_REJECT) &&
			    ((ulp_word4 & IOERR_PARAM_MASK) ==
			    IOERR_SLI_ABORTED))
				goto stop_rr_fcf_flogi;
			else
				phba->fcoe_cvl_eventtag_attn =
					phba->fcoe_cvl_eventtag;
			lpfc_printf_log(phba, KERN_WARNING, LOG_FIP | LOG_ELS,
					"2611 FLOGI failed on FCF (x%x), "
					"status:x%x/x%x, tmo:x%x, perform "
					"roundrobin FCF failover\n",
					phba->fcf.current_rec.fcf_indx,
					ulp_status, ulp_word4, tmo);
			lpfc_sli4_set_fcf_flogi_fail(phba,
					phba->fcf.current_rec.fcf_indx);
			fcf_index = lpfc_sli4_fcf_rr_next_index_get(phba);
			rc = lpfc_sli4_fcf_rr_next_proc(vport, fcf_index);
			if (rc)
				goto out;
		}

stop_rr_fcf_flogi:
		 
		if (!(ulp_status == IOSTAT_LOCAL_REJECT &&
		      ((ulp_word4 & IOERR_PARAM_MASK) ==
					IOERR_LOOP_OPEN_FAILURE)))
			lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
					 "2858 FLOGI failure Status:x%x/x%x TMO"
					 ":x%x Data x%x x%x\n",
					 ulp_status, ulp_word4, tmo,
					 phba->hba_flag, phba->fcf.fcf_flag);

		 
		if (lpfc_els_retry(phba, cmdiocb, rspiocb)) {
			 
			lpfc_check_nlp_post_devloss(vport, ndlp);
			flogi_in_retry = true;
			goto out;
		}

		 
		if (!(ndlp->nlp_flag & NLP_IN_DEV_LOSS) &&
		    !(ndlp->fc4_xpt_flags & SCSI_XPT_REGD))
			lpfc_nlp_put(ndlp);

		lpfc_printf_vlog(vport, KERN_WARNING, LOG_ELS,
				 "0150 FLOGI failure Status:x%x/x%x "
				 "xri x%x TMO:x%x refcnt %d\n",
				 ulp_status, ulp_word4, cmdiocb->sli4_xritag,
				 tmo, kref_read(&ndlp->kref));

		 
		if (!(ulp_status == IOSTAT_LOCAL_REJECT &&
		      ((ulp_word4 & IOERR_PARAM_MASK) ==
					IOERR_LOOP_OPEN_FAILURE))) {
			 
			lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
					 "0100 FLOGI failure Status:x%x/x%x "
					 "TMO:x%x\n",
					 ulp_status, ulp_word4, tmo);
			goto flogifail;
		}

		 
		spin_lock_irq(shost->host_lock);
		vport->fc_flag &= ~(FC_FABRIC | FC_PUBLIC_LOOP |
				    FC_PT2PT_NO_NVME);
		spin_unlock_irq(shost->host_lock);

		 
		if (phba->alpa_map[0] == 0)
			vport->cfg_discovery_threads = LPFC_MAX_DISC_THREADS;
		if ((phba->sli_rev == LPFC_SLI_REV4) &&
		    (!(vport->fc_flag & FC_VFI_REGISTERED) ||
		     (vport->fc_prevDID != vport->fc_myDID) ||
			phba->fc_topology_changed)) {
			if (vport->fc_flag & FC_VFI_REGISTERED) {
				if (phba->fc_topology_changed) {
					lpfc_unregister_fcf_prep(phba);
					spin_lock_irq(shost->host_lock);
					vport->fc_flag &= ~FC_VFI_REGISTERED;
					spin_unlock_irq(shost->host_lock);
					phba->fc_topology_changed = 0;
				} else {
					lpfc_sli4_unreg_all_rpis(vport);
				}
			}

			 
			if (!lpfc_error_lost_link(vport, ulp_status, ulp_word4))
				lpfc_issue_reg_vfi(vport);

			goto out;
		}
		goto flogifail;
	}
	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~FC_VPORT_CVL_RCVD;
	vport->fc_flag &= ~FC_VPORT_LOGO_RCVD;
	spin_unlock_irq(shost->host_lock);

	 
	prsp = list_get_first(&pcmd->list, struct lpfc_dmabuf, list);
	if (!prsp)
		goto out;
	sp = prsp->virt + sizeof(uint32_t);

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0101 FLOGI completes successfully, I/O tag:x%x "
			 "xri x%x Data: x%x x%x x%x x%x x%x x%x x%x %d\n",
			 cmdiocb->iotag, cmdiocb->sli4_xritag,
			 ulp_word4, sp->cmn.e_d_tov,
			 sp->cmn.w2.r_a_tov, sp->cmn.edtovResolution,
			 vport->port_state, vport->fc_flag,
			 sp->cmn.priority_tagging, kref_read(&ndlp->kref));

	if (sp->cmn.priority_tagging)
		vport->phba->pport->vmid_flag |= (LPFC_VMID_ISSUE_QFPA |
						  LPFC_VMID_TYPE_PRIO);
	 
	if (lpfc_is_vmid_enabled(phba))
		lpfc_reinit_vmid(vport);

	 
	lpfc_check_nlp_post_devloss(vport, ndlp);
	if (vport->port_state == LPFC_FLOGI) {
		 
		if (sp->cmn.fPort)
			rc = lpfc_cmpl_els_flogi_fabric(vport, ndlp, sp,
							ulp_word4);
		else if (!(phba->hba_flag & HBA_FCOE_MODE))
			rc = lpfc_cmpl_els_flogi_nport(vport, ndlp, sp);
		else {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				"2831 FLOGI response with cleared Fabric "
				"bit fcf_index 0x%x "
				"Switch Name %02x%02x%02x%02x%02x%02x%02x%02x "
				"Fabric Name "
				"%02x%02x%02x%02x%02x%02x%02x%02x\n",
				phba->fcf.current_rec.fcf_indx,
				phba->fcf.current_rec.switch_name[0],
				phba->fcf.current_rec.switch_name[1],
				phba->fcf.current_rec.switch_name[2],
				phba->fcf.current_rec.switch_name[3],
				phba->fcf.current_rec.switch_name[4],
				phba->fcf.current_rec.switch_name[5],
				phba->fcf.current_rec.switch_name[6],
				phba->fcf.current_rec.switch_name[7],
				phba->fcf.current_rec.fabric_name[0],
				phba->fcf.current_rec.fabric_name[1],
				phba->fcf.current_rec.fabric_name[2],
				phba->fcf.current_rec.fabric_name[3],
				phba->fcf.current_rec.fabric_name[4],
				phba->fcf.current_rec.fabric_name[5],
				phba->fcf.current_rec.fabric_name[6],
				phba->fcf.current_rec.fabric_name[7]);

			lpfc_nlp_put(ndlp);
			spin_lock_irq(&phba->hbalock);
			phba->fcf.fcf_flag &= ~FCF_DISCOVERY;
			phba->hba_flag &= ~(FCF_RR_INPROG | HBA_DEVLOSS_TMO);
			spin_unlock_irq(&phba->hbalock);
			phba->fcf.fcf_redisc_attempted = 0;  
			goto out;
		}
		if (!rc) {
			 
			if (phba->hba_flag & HBA_FIP_SUPPORT)
				lpfc_printf_vlog(vport, KERN_INFO, LOG_FIP |
						LOG_ELS,
						"2769 FLOGI to FCF (x%x) "
						"completed successfully\n",
						phba->fcf.current_rec.fcf_indx);
			spin_lock_irq(&phba->hbalock);
			phba->fcf.fcf_flag &= ~FCF_DISCOVERY;
			phba->hba_flag &= ~(FCF_RR_INPROG | HBA_DEVLOSS_TMO);
			spin_unlock_irq(&phba->hbalock);
			phba->fcf.fcf_redisc_attempted = 0;  
			goto out;
		}
	} else if (vport->port_state > LPFC_FLOGI &&
		   vport->fc_flag & FC_PT2PT) {
		 
		if (!sp->cmn.fPort)
			goto out;
	}

flogifail:
	spin_lock_irq(&phba->hbalock);
	phba->fcf.fcf_flag &= ~FCF_DISCOVERY;
	spin_unlock_irq(&phba->hbalock);

	if (!lpfc_error_lost_link(vport, ulp_status, ulp_word4)) {
		 
		lpfc_disc_list_loopmap(vport);

		 
		lpfc_disc_start(vport);
	} else if (((ulp_status != IOSTAT_LOCAL_REJECT) ||
			(((ulp_word4 & IOERR_PARAM_MASK) !=
			 IOERR_SLI_ABORTED) &&
			((ulp_word4 & IOERR_PARAM_MASK) !=
			 IOERR_SLI_DOWN))) &&
			(phba->link_state != LPFC_CLEAR_LA)) {
		 
		lpfc_issue_clear_la(phba, vport);
	}
out:
	if (!flogi_in_retry)
		phba->hba_flag &= ~HBA_FLOGI_OUTSTANDING;

	lpfc_els_free_iocb(phba, cmdiocb);
	lpfc_nlp_put(ndlp);
}

 
static void
lpfc_cmpl_els_link_down(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
			struct lpfc_iocbq *rspiocb)
{
	uint32_t *pcmd;
	uint32_t cmd;
	u32 ulp_status, ulp_word4;

	pcmd = (uint32_t *)cmdiocb->cmd_dmabuf->virt;
	cmd = *pcmd;

	ulp_status = get_job_ulpstatus(phba, rspiocb);
	ulp_word4 = get_job_word4(phba, rspiocb);

	lpfc_printf_log(phba, KERN_INFO, LOG_ELS,
			"6445 ELS completes after LINK_DOWN: "
			" Status %x/%x cmd x%x flg x%x\n",
			ulp_status, ulp_word4, cmd,
			cmdiocb->cmd_flag);

	if (cmdiocb->cmd_flag & LPFC_IO_FABRIC) {
		cmdiocb->cmd_flag &= ~LPFC_IO_FABRIC;
		atomic_dec(&phba->fabric_iocb_count);
	}
	lpfc_els_free_iocb(phba, cmdiocb);
}

 
static int
lpfc_issue_els_flogi(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		     uint8_t retry)
{
	struct lpfc_hba  *phba = vport->phba;
	struct serv_parm *sp;
	union lpfc_wqe128 *wqe = NULL;
	IOCB_t *icmd = NULL;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_iocbq defer_flogi_acc;
	u8 *pcmd, ct;
	uint16_t cmdsize;
	uint32_t tmo, did;
	int rc;

	cmdsize = (sizeof(uint32_t) + sizeof(struct serv_parm));
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_FLOGI);

	if (!elsiocb)
		return 1;

	wqe = &elsiocb->wqe;
	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;
	icmd = &elsiocb->iocb;

	 
	*((uint32_t *) (pcmd)) = ELS_CMD_FLOGI;
	pcmd += sizeof(uint32_t);
	memcpy(pcmd, &vport->fc_sparam, sizeof(struct serv_parm));
	sp = (struct serv_parm *) pcmd;

	 
	sp->cmn.e_d_tov = 0;
	sp->cmn.w2.r_a_tov = 0;
	sp->cmn.virtual_fabric_support = 0;
	sp->cls1.classValid = 0;
	if (sp->cmn.fcphLow < FC_PH3)
		sp->cmn.fcphLow = FC_PH3;
	if (sp->cmn.fcphHigh < FC_PH3)
		sp->cmn.fcphHigh = FC_PH3;

	 
	if (phba->cfg_vmid_priority_tagging) {
		sp->cmn.priority_tagging = 1;
		 
		if (!memchr_inv(vport->lpfc_vmid_host_uuid, 0,
				sizeof(vport->lpfc_vmid_host_uuid))) {
			memcpy(vport->lpfc_vmid_host_uuid, phba->wwpn,
			       sizeof(phba->wwpn));
			memcpy(&vport->lpfc_vmid_host_uuid[8], phba->wwnn,
			       sizeof(phba->wwnn));
		}
	}

	if  (phba->sli_rev == LPFC_SLI_REV4) {
		if (bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) ==
		    LPFC_SLI_INTF_IF_TYPE_0) {
			 
			ct = SLI4_CT_FCFI;
			bf_set(wqe_ct, &wqe->els_req.wqe_com, ct);

			 
			bf_set(wqe_ctxt_tag, &wqe->els_req.wqe_com,
			       phba->fcf.fcfi);
		}

		 
		sp->cls2.classValid = 0;
		sp->cls2.seqDelivery = 0;
	} else {
		 
		sp->cls2.seqDelivery = (sp->cls2.classValid) ? 1 : 0;
		sp->cls3.seqDelivery = (sp->cls3.classValid) ? 1 : 0;
		if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) {
			sp->cmn.request_multiple_Nport = 1;
			 
			icmd->ulpCt_h = 1;
			icmd->ulpCt_l = 0;
		} else {
			sp->cmn.request_multiple_Nport = 0;
		}

		if (phba->fc_topology != LPFC_TOPOLOGY_LOOP) {
			icmd->un.elsreq64.myID = 0;
			icmd->un.elsreq64.fl = 1;
		}
	}

	tmo = phba->fc_ratov;
	phba->fc_ratov = LPFC_DISC_FLOGI_TMO;
	lpfc_set_disctmo(vport);
	phba->fc_ratov = tmo;

	phba->fc_stat.elsXmitFLOGI++;
	elsiocb->cmd_cmpl = lpfc_cmpl_els_flogi;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue FLOGI:     opt:x%x",
		phba->sli3_options, 0, 0);

	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}

	 
	phba->hba_flag |= (HBA_FLOGI_ISSUED | HBA_FLOGI_OUTSTANDING);

	rc = lpfc_issue_fabric_iocb(phba, elsiocb);
	if (rc == IOCB_ERROR) {
		phba->hba_flag &= ~(HBA_FLOGI_ISSUED | HBA_FLOGI_OUTSTANDING);
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
		return 1;
	}

	 
	phba->link_flag &= ~LS_EXTERNAL_LOOPBACK;

	 
	if (phba->defer_flogi_acc_flag) {
		 
		ndlp = lpfc_findnode_did(vport, 0);
		if (!ndlp)
			return 0;

		did = vport->fc_myDID;
		vport->fc_myDID = Fabric_DID;

		memset(&defer_flogi_acc, 0, sizeof(struct lpfc_iocbq));

		if (phba->sli_rev == LPFC_SLI_REV4) {
			bf_set(wqe_ctxt_tag,
			       &defer_flogi_acc.wqe.xmit_els_rsp.wqe_com,
			       phba->defer_flogi_acc_rx_id);
			bf_set(wqe_rcvoxid,
			       &defer_flogi_acc.wqe.xmit_els_rsp.wqe_com,
			       phba->defer_flogi_acc_ox_id);
		} else {
			icmd = &defer_flogi_acc.iocb;
			icmd->ulpContext = phba->defer_flogi_acc_rx_id;
			icmd->unsli3.rcvsli3.ox_id =
				phba->defer_flogi_acc_ox_id;
		}

		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
				 "3354 Xmit deferred FLOGI ACC: rx_id: x%x,"
				 " ox_id: x%x, hba_flag x%x\n",
				 phba->defer_flogi_acc_rx_id,
				 phba->defer_flogi_acc_ox_id, phba->hba_flag);

		 
		lpfc_els_rsp_acc(vport, ELS_CMD_FLOGI, &defer_flogi_acc,
				 ndlp, NULL);

		phba->defer_flogi_acc_flag = false;
		vport->fc_myDID = did;

		 
		lpfc_nlp_put(ndlp);
	}

	return 0;
}

 
int
lpfc_els_abort_flogi(struct lpfc_hba *phba)
{
	struct lpfc_sli_ring *pring;
	struct lpfc_iocbq *iocb, *next_iocb;
	struct lpfc_nodelist *ndlp;
	u32 ulp_command;

	 
	lpfc_printf_log(phba, KERN_INFO, LOG_DISCOVERY,
			"0201 Abort outstanding I/O on NPort x%x\n",
			Fabric_DID);

	pring = lpfc_phba_elsring(phba);
	if (unlikely(!pring))
		return -EIO;

	 
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(iocb, next_iocb, &pring->txcmplq, list) {
		ulp_command = get_job_cmnd(phba, iocb);
		if (ulp_command == CMD_ELS_REQUEST64_CR) {
			ndlp = iocb->ndlp;
			if (ndlp && ndlp->nlp_DID == Fabric_DID) {
				if ((phba->pport->fc_flag & FC_PT2PT) &&
				    !(phba->pport->fc_flag & FC_PT2PT_PLOGI))
					iocb->fabric_cmd_cmpl =
						lpfc_ignore_els_cmpl;
				lpfc_sli_issue_abort_iotag(phba, pring, iocb,
							   NULL);
			}
		}
	}
	 
	lpfc_issue_hb_tmo(phba);

	spin_unlock_irq(&phba->hbalock);

	return 0;
}

 
int
lpfc_initial_flogi(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp;

	vport->port_state = LPFC_FLOGI;
	lpfc_set_disctmo(vport);

	 
	ndlp = lpfc_findnode_did(vport, Fabric_DID);
	if (!ndlp) {
		 
		ndlp = lpfc_nlp_init(vport, Fabric_DID);
		if (!ndlp)
			return 0;
		 
		ndlp->nlp_type |= NLP_FABRIC;

		 
		lpfc_enqueue_node(vport, ndlp);
	}

	 
	vport->fc_flag &= ~FC_FABRIC;
	if (lpfc_issue_els_flogi(vport, ndlp, 0)) {
		 
		if (!(ndlp->fc4_xpt_flags & (SCSI_XPT_REGD | NVME_XPT_REGD)) &&
		    !(ndlp->nlp_flag & NLP_IN_DEV_LOSS))
			lpfc_nlp_put(ndlp);
		return 0;
	}
	return 1;
}

 
int
lpfc_initial_fdisc(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp;

	 
	ndlp = lpfc_findnode_did(vport, Fabric_DID);
	if (!ndlp) {
		 
		ndlp = lpfc_nlp_init(vport, Fabric_DID);
		if (!ndlp)
			return 0;

		 
		ndlp->nlp_type |= NLP_FABRIC;

		 
		lpfc_enqueue_node(vport, ndlp);
	}

	if (lpfc_issue_els_fdisc(vport, ndlp, 0)) {
		 
		if (!(ndlp->fc4_xpt_flags & (SCSI_XPT_REGD | NVME_XPT_REGD)) &&
		    !(ndlp->nlp_flag & NLP_IN_DEV_LOSS))
			lpfc_nlp_put(ndlp);
		return 0;
	}
	return 1;
}

 
void
lpfc_more_plogi(struct lpfc_vport *vport)
{
	if (vport->num_disc_nodes)
		vport->num_disc_nodes--;

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0232 Continue discovery with %d PLOGIs to go "
			 "Data: x%x x%x x%x\n",
			 vport->num_disc_nodes, vport->fc_plogi_cnt,
			 vport->fc_flag, vport->port_state);
	 
	if (vport->fc_flag & FC_NLP_MORE)
		 
		lpfc_els_disc_plogi(vport);

	return;
}

 
static struct lpfc_nodelist *
lpfc_plogi_confirm_nport(struct lpfc_hba *phba, uint32_t *prsp,
			 struct lpfc_nodelist *ndlp)
{
	struct lpfc_vport *vport = ndlp->vport;
	struct lpfc_nodelist *new_ndlp;
	struct serv_parm *sp;
	uint8_t  name[sizeof(struct lpfc_name)];
	uint32_t keepDID = 0, keep_nlp_flag = 0;
	uint32_t keep_new_nlp_flag = 0;
	uint16_t keep_nlp_state;
	u32 keep_nlp_fc4_type = 0;
	struct lpfc_nvme_rport *keep_nrport = NULL;
	unsigned long *active_rrqs_xri_bitmap = NULL;

	 
	if (ndlp->nlp_type & NLP_FABRIC)
		return ndlp;

	sp = (struct serv_parm *) ((uint8_t *) prsp + sizeof(uint32_t));
	memset(name, 0, sizeof(struct lpfc_name));

	 
	new_ndlp = lpfc_findnode_wwpn(vport, &sp->portName);

	 
	if (!new_ndlp || (new_ndlp == ndlp))
		return ndlp;

	 
	lpfc_nlp_unreg_node(vport, new_ndlp);

	if (phba->sli_rev == LPFC_SLI_REV4) {
		active_rrqs_xri_bitmap = mempool_alloc(phba->active_rrq_pool,
						       GFP_KERNEL);
		if (active_rrqs_xri_bitmap)
			memset(active_rrqs_xri_bitmap, 0,
			       phba->cfg_rrq_xri_bitmap_sz);
	}

	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS | LOG_NODE,
			 "3178 PLOGI confirm: ndlp x%x x%x x%x: "
			 "new_ndlp x%x x%x x%x\n",
			 ndlp->nlp_DID, ndlp->nlp_flag,  ndlp->nlp_fc4_type,
			 (new_ndlp ? new_ndlp->nlp_DID : 0),
			 (new_ndlp ? new_ndlp->nlp_flag : 0),
			 (new_ndlp ? new_ndlp->nlp_fc4_type : 0));

	keepDID = new_ndlp->nlp_DID;

	if (phba->sli_rev == LPFC_SLI_REV4 && active_rrqs_xri_bitmap)
		memcpy(active_rrqs_xri_bitmap, new_ndlp->active_rrqs_xri_bitmap,
		       phba->cfg_rrq_xri_bitmap_sz);

	 
	if (vport->fc_flag & FC_FABRIC) {
		keep_nlp_fc4_type = new_ndlp->nlp_fc4_type;
		new_ndlp->nlp_fc4_type = ndlp->nlp_fc4_type;
	}

	lpfc_unreg_rpi(vport, new_ndlp);
	new_ndlp->nlp_DID = ndlp->nlp_DID;
	new_ndlp->nlp_prev_state = ndlp->nlp_prev_state;
	if (phba->sli_rev == LPFC_SLI_REV4)
		memcpy(new_ndlp->active_rrqs_xri_bitmap,
		       ndlp->active_rrqs_xri_bitmap,
		       phba->cfg_rrq_xri_bitmap_sz);

	 
	spin_lock_irq(&ndlp->lock);
	spin_lock_irq(&new_ndlp->lock);
	keep_new_nlp_flag = new_ndlp->nlp_flag;
	keep_nlp_flag = ndlp->nlp_flag;
	new_ndlp->nlp_flag = ndlp->nlp_flag;

	 
	if (keep_new_nlp_flag & NLP_UNREG_INP)
		new_ndlp->nlp_flag |= NLP_UNREG_INP;
	else
		new_ndlp->nlp_flag &= ~NLP_UNREG_INP;

	 
	if (keep_new_nlp_flag & NLP_RPI_REGISTERED)
		new_ndlp->nlp_flag |= NLP_RPI_REGISTERED;
	else
		new_ndlp->nlp_flag &= ~NLP_RPI_REGISTERED;

	 
	if (keep_new_nlp_flag & NLP_DROPPED)
		new_ndlp->nlp_flag |= NLP_DROPPED;
	else
		new_ndlp->nlp_flag &= ~NLP_DROPPED;

	ndlp->nlp_flag = keep_new_nlp_flag;

	 
	if (keep_nlp_flag & NLP_UNREG_INP)
		ndlp->nlp_flag |= NLP_UNREG_INP;
	else
		ndlp->nlp_flag &= ~NLP_UNREG_INP;

	 
	if (keep_nlp_flag & NLP_RPI_REGISTERED)
		ndlp->nlp_flag |= NLP_RPI_REGISTERED;
	else
		ndlp->nlp_flag &= ~NLP_RPI_REGISTERED;

	 
	if (keep_nlp_flag & NLP_DROPPED)
		ndlp->nlp_flag |= NLP_DROPPED;
	else
		ndlp->nlp_flag &= ~NLP_DROPPED;

	spin_unlock_irq(&new_ndlp->lock);
	spin_unlock_irq(&ndlp->lock);

	 
	keep_nlp_state = new_ndlp->nlp_state;
	lpfc_nlp_set_state(vport, new_ndlp, ndlp->nlp_state);

	 
	keep_nrport = new_ndlp->nrport;
	new_ndlp->nrport = ndlp->nrport;

	 
	if (memcmp(&ndlp->nlp_portname, name, sizeof(struct lpfc_name)) == 0) {
		 
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "3179 PLOGI confirm NEW: %x %x\n",
			 new_ndlp->nlp_DID, keepDID);

		 
		ndlp->nlp_DID = keepDID;
		ndlp->nlp_fc4_type = keep_nlp_fc4_type;
		lpfc_nlp_set_state(vport, ndlp, keep_nlp_state);
		if (phba->sli_rev == LPFC_SLI_REV4 &&
		    active_rrqs_xri_bitmap)
			memcpy(ndlp->active_rrqs_xri_bitmap,
			       active_rrqs_xri_bitmap,
			       phba->cfg_rrq_xri_bitmap_sz);

	} else {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "3180 PLOGI confirm SWAP: %x %x\n",
			 new_ndlp->nlp_DID, keepDID);

		lpfc_unreg_rpi(vport, ndlp);

		 
		ndlp->nlp_DID = keepDID;
		ndlp->nlp_fc4_type = keep_nlp_fc4_type;

		if (phba->sli_rev == LPFC_SLI_REV4 &&
		    active_rrqs_xri_bitmap)
			memcpy(ndlp->active_rrqs_xri_bitmap,
			       active_rrqs_xri_bitmap,
			       phba->cfg_rrq_xri_bitmap_sz);

		 
		if ((ndlp->nlp_state == NLP_STE_UNMAPPED_NODE) ||
		    (ndlp->nlp_state == NLP_STE_MAPPED_NODE))
			keep_nlp_state = NLP_STE_NPR_NODE;
		lpfc_nlp_set_state(vport, ndlp, keep_nlp_state);
		ndlp->nrport = keep_nrport;
	}

	 
	if (!ndlp->rport && (ndlp->nlp_state == NLP_STE_NPR_NODE))
		lpfc_disc_state_machine(vport, ndlp, NULL, NLP_EVT_DEVICE_RM);

	if (phba->sli_rev == LPFC_SLI_REV4 &&
	    active_rrqs_xri_bitmap)
		mempool_free(active_rrqs_xri_bitmap,
			     phba->active_rrq_pool);

	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS | LOG_NODE,
			 "3173 PLOGI confirm exit: new_ndlp x%x x%x x%x\n",
			 new_ndlp->nlp_DID, new_ndlp->nlp_flag,
			 new_ndlp->nlp_fc4_type);

	return new_ndlp;
}

 
void
lpfc_end_rscn(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	if (vport->fc_flag & FC_RSCN_MODE) {
		 
		if (vport->fc_rscn_id_cnt ||
		    (vport->fc_flag & FC_RSCN_DISCOVERY) != 0)
			lpfc_els_handle_rscn(vport);
		else {
			spin_lock_irq(shost->host_lock);
			vport->fc_flag &= ~FC_RSCN_MODE;
			spin_unlock_irq(shost->host_lock);
		}
	}
}

 

static void
lpfc_cmpl_els_rrq(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct lpfc_nodelist *ndlp = cmdiocb->ndlp;
	struct lpfc_node_rrq *rrq;
	u32 ulp_status = get_job_ulpstatus(phba, rspiocb);
	u32 ulp_word4 = get_job_word4(phba, rspiocb);

	 
	rrq = cmdiocb->context_un.rrq;
	cmdiocb->rsp_iocb = rspiocb;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"RRQ cmpl:      status:x%x/x%x did:x%x",
		ulp_status, ulp_word4,
		get_job_els_rsp64_did(phba, cmdiocb));


	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "2880 RRQ completes to DID x%x "
			 "Data: x%x x%x x%x x%x x%x\n",
			 ndlp->nlp_DID, ulp_status, ulp_word4,
			 get_wqe_tmo(cmdiocb), rrq->xritag, rrq->rxid);

	if (ulp_status) {
		 
		 
		if (ulp_status != IOSTAT_LS_RJT ||
		    (((ulp_word4) >> 16 != LSRJT_INVALID_CMD) &&
		     ((ulp_word4) >> 16 != LSRJT_UNABLE_TPC)) ||
		    (phba)->pport->cfg_log_verbose & LOG_ELS)
			lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
					 "2881 RRQ failure DID:%06X Status:"
					 "x%x/x%x\n",
					 ndlp->nlp_DID, ulp_status,
					 ulp_word4);
	}

	lpfc_clr_rrq_active(phba, rrq->xritag, rrq);
	lpfc_els_free_iocb(phba, cmdiocb);
	lpfc_nlp_put(ndlp);
	return;
}
 
static void
lpfc_cmpl_els_plogi(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		    struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	IOCB_t *irsp;
	struct lpfc_nodelist *ndlp, *free_ndlp;
	struct lpfc_dmabuf *prsp;
	int disc;
	struct serv_parm *sp = NULL;
	u32 ulp_status, ulp_word4, did, iotag;
	bool release_node = false;

	 
	cmdiocb->rsp_iocb = rspiocb;

	ulp_status = get_job_ulpstatus(phba, rspiocb);
	ulp_word4 = get_job_word4(phba, rspiocb);
	did = get_job_els_rsp64_did(phba, cmdiocb);

	if (phba->sli_rev == LPFC_SLI_REV4) {
		iotag = get_wqe_reqtag(cmdiocb);
	} else {
		irsp = &rspiocb->iocb;
		iotag = irsp->ulpIoTag;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"PLOGI cmpl:      status:x%x/x%x did:x%x",
		ulp_status, ulp_word4, did);

	ndlp = lpfc_findnode_did(vport, did);
	if (!ndlp) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "0136 PLOGI completes to NPort x%x "
				 "with no ndlp. Data: x%x x%x x%x\n",
				 did, ulp_status, ulp_word4, iotag);
		goto out_freeiocb;
	}

	 
	spin_lock_irq(&ndlp->lock);
	disc = (ndlp->nlp_flag & NLP_NPR_2B_DISC);
	ndlp->nlp_flag &= ~NLP_NPR_2B_DISC;
	spin_unlock_irq(&ndlp->lock);

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0102 PLOGI completes to NPort x%06x "
			 "Data: x%x x%x x%x x%x x%x\n",
			 ndlp->nlp_DID, ndlp->nlp_fc4_type,
			 ulp_status, ulp_word4,
			 disc, vport->num_disc_nodes);

	 
	if (lpfc_els_chk_latt(vport)) {
		spin_lock_irq(&ndlp->lock);
		ndlp->nlp_flag |= NLP_NPR_2B_DISC;
		spin_unlock_irq(&ndlp->lock);
		goto out;
	}

	if (ulp_status) {
		 
		if (lpfc_els_retry(phba, cmdiocb, rspiocb)) {
			 
			if (disc) {
				spin_lock_irq(&ndlp->lock);
				ndlp->nlp_flag |= NLP_NPR_2B_DISC;
				spin_unlock_irq(&ndlp->lock);
			}
			goto out;
		}
		 
		if (ulp_status != IOSTAT_LS_RJT ||
		    (((ulp_word4) >> 16 != LSRJT_INVALID_CMD) &&
		     ((ulp_word4) >> 16 != LSRJT_UNABLE_TPC)) ||
		    (phba)->pport->cfg_log_verbose & LOG_ELS)
			lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
					 "2753 PLOGI failure DID:%06X "
					 "Status:x%x/x%x\n",
					 ndlp->nlp_DID, ulp_status,
					 ulp_word4);

		 
		if (!lpfc_error_lost_link(vport, ulp_status, ulp_word4))
			lpfc_disc_state_machine(vport, ndlp, cmdiocb,
						NLP_EVT_CMPL_PLOGI);

		 
		spin_lock_irq(&ndlp->lock);
		if ((ndlp->nlp_flag & (NLP_ACC_REGLOGIN | NLP_RCV_PLOGI)) &&
		    ndlp->nlp_state == NLP_STE_REG_LOGIN_ISSUE) {
			spin_unlock_irq(&ndlp->lock);
			goto out;
		}

		 
		if (!(ndlp->fc4_xpt_flags & (SCSI_XPT_REGD | NVME_XPT_REGD))) {
			ndlp->nlp_flag &= ~NLP_NPR_2B_DISC;
			if (!(ndlp->nlp_flag & NLP_IN_DEV_LOSS))
				release_node = true;
		}
		spin_unlock_irq(&ndlp->lock);

		if (release_node)
			lpfc_disc_state_machine(vport, ndlp, cmdiocb,
						NLP_EVT_DEVICE_RM);
	} else {
		 
		prsp = list_entry(cmdiocb->cmd_dmabuf->list.next,
				  struct lpfc_dmabuf, list);
		ndlp = lpfc_plogi_confirm_nport(phba, prsp->virt, ndlp);

		sp = (struct serv_parm *)((u8 *)prsp->virt +
					  sizeof(u32));

		ndlp->vmid_support = 0;
		if ((phba->cfg_vmid_app_header && sp->cmn.app_hdr_support) ||
		    (phba->cfg_vmid_priority_tagging &&
		     sp->cmn.priority_tagging)) {
			lpfc_printf_log(phba, KERN_DEBUG, LOG_ELS,
					"4018 app_hdr_support %d tagging %d DID x%x\n",
					sp->cmn.app_hdr_support,
					sp->cmn.priority_tagging,
					ndlp->nlp_DID);
			 
			ndlp->vmid_support = 1;
		}

		lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					NLP_EVT_CMPL_PLOGI);
	}

	if (disc && vport->num_disc_nodes) {
		 
		lpfc_more_plogi(vport);

		if (vport->num_disc_nodes == 0) {
			spin_lock_irq(shost->host_lock);
			vport->fc_flag &= ~FC_NDISC_ACTIVE;
			spin_unlock_irq(shost->host_lock);

			lpfc_can_disctmo(vport);
			lpfc_end_rscn(vport);
		}
	}

out:
	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_NODE,
			      "PLOGI Cmpl PUT:     did:x%x refcnt %d",
			      ndlp->nlp_DID, kref_read(&ndlp->kref), 0);

out_freeiocb:
	 
	free_ndlp = cmdiocb->ndlp;

	lpfc_els_free_iocb(phba, cmdiocb);
	lpfc_nlp_put(free_ndlp);
	return;
}

 
int
lpfc_issue_els_plogi(struct lpfc_vport *vport, uint32_t did, uint8_t retry)
{
	struct lpfc_hba  *phba = vport->phba;
	struct serv_parm *sp;
	struct lpfc_nodelist *ndlp;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int ret;

	ndlp = lpfc_findnode_did(vport, did);
	if (!ndlp)
		return 1;

	 
	if ((ndlp->nlp_flag & (NLP_IGNR_REG_CMPL | NLP_UNREG_INP)) &&
	    ((ndlp->nlp_DID & Fabric_DID_MASK) != Fabric_DID_MASK) &&
	    !(vport->fc_flag & FC_OFFLINE_MODE)) {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
				 "4110 Issue PLOGI x%x deferred "
				 "on NPort x%x rpi x%x flg x%x Data:"
				 " x%px\n",
				 ndlp->nlp_defer_did, ndlp->nlp_DID,
				 ndlp->nlp_rpi, ndlp->nlp_flag, ndlp);

		 
		if (ndlp->nlp_defer_did == NLP_EVT_NOTHING_PENDING)
			ndlp->nlp_defer_did = did;
		return 0;
	}

	cmdsize = (sizeof(uint32_t) + sizeof(struct serv_parm));
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp, did,
				     ELS_CMD_PLOGI);
	if (!elsiocb)
		return 1;

	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;

	 
	*((uint32_t *) (pcmd)) = ELS_CMD_PLOGI;
	pcmd += sizeof(uint32_t);
	memcpy(pcmd, &vport->fc_sparam, sizeof(struct serv_parm));
	sp = (struct serv_parm *) pcmd;

	 
	if ((vport->fc_flag & FC_FABRIC) && !(vport->fc_flag & FC_PUBLIC_LOOP))
		sp->cmn.altBbCredit = 1;

	if (sp->cmn.fcphLow < FC_PH_4_3)
		sp->cmn.fcphLow = FC_PH_4_3;

	if (sp->cmn.fcphHigh < FC_PH3)
		sp->cmn.fcphHigh = FC_PH3;

	sp->cmn.valid_vendor_ver_level = 0;
	memset(sp->un.vendorVersion, 0, sizeof(sp->un.vendorVersion));
	sp->cmn.bbRcvSizeMsb &= 0xF;

	 
	ndlp->vmid_support = 0;
	if (vport->vmid_priority_tagging)
		sp->cmn.priority_tagging = 1;
	else if (phba->cfg_vmid_app_header &&
		 bf_get(lpfc_ftr_ashdr, &phba->sli4_hba.sli4_flags))
		sp->cmn.app_hdr_support = 1;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue PLOGI:     did:x%x",
		did, 0, 0);

	 
	if (phba->sli.sli_flag & LPFC_SLI_SUPPRESS_RSP) {
		sp->cmn.valid_vendor_ver_level = 1;
		sp->un.vv.vid = cpu_to_be32(LPFC_VV_EMLX_ID);
		sp->un.vv.flags = cpu_to_be32(LPFC_VV_SUPPRESS_RSP);
	}

	phba->fc_stat.elsXmitPLOGI++;
	elsiocb->cmd_cmpl = lpfc_cmpl_els_plogi;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
			      "Issue PLOGI:     did:x%x refcnt %d",
			      did, kref_read(&ndlp->kref), 0);
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}

	ret = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (ret) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
		return 1;
	}

	return 0;
}

 
static void
lpfc_cmpl_els_prli(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		   struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct lpfc_nodelist *ndlp;
	char *mode;
	u32 loglevel;
	u32 ulp_status;
	u32 ulp_word4;
	bool release_node = false;

	 
	cmdiocb->rsp_iocb = rspiocb;

	ndlp = cmdiocb->ndlp;

	ulp_status = get_job_ulpstatus(phba, rspiocb);
	ulp_word4 = get_job_word4(phba, rspiocb);

	spin_lock_irq(&ndlp->lock);
	ndlp->nlp_flag &= ~NLP_PRLI_SND;

	 
	vport->fc_prli_sent--;
	ndlp->fc4_prli_sent--;
	spin_unlock_irq(&ndlp->lock);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"PRLI cmpl:       status:x%x/x%x did:x%x",
		ulp_status, ulp_word4,
		ndlp->nlp_DID);

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0103 PRLI completes to NPort x%06x "
			 "Data: x%x x%x x%x x%x\n",
			 ndlp->nlp_DID, ulp_status, ulp_word4,
			 vport->num_disc_nodes, ndlp->fc4_prli_sent);

	 
	if (lpfc_els_chk_latt(vport))
		goto out;

	if (ulp_status) {
		 
		if (lpfc_els_retry(phba, cmdiocb, rspiocb)) {
			 
			goto out;
		}

		 
		if ((vport->fc_flag & FC_FABRIC) ||
		    (vport->cfg_enable_fc4_type != LPFC_ENABLE_BOTH)) {
			mode = KERN_ERR;
			loglevel =  LOG_TRACE_EVENT;
		} else {
			mode = KERN_INFO;
			loglevel =  LOG_ELS;
		}

		 
		lpfc_printf_vlog(vport, mode, loglevel,
				 "2754 PRLI failure DID:%06X Status:x%x/x%x, "
				 "data: x%x x%x x%x\n",
				 ndlp->nlp_DID, ulp_status,
				 ulp_word4, ndlp->nlp_state,
				 ndlp->fc4_prli_sent, ndlp->nlp_flag);

		 
		if (!lpfc_error_lost_link(vport, ulp_status, ulp_word4))
			lpfc_disc_state_machine(vport, ndlp, cmdiocb,
						NLP_EVT_CMPL_PRLI);

		 
		if ((ndlp->nlp_state >= NLP_STE_PLOGI_ISSUE &&
		     ndlp->nlp_state <= NLP_STE_REG_LOGIN_ISSUE) ||
		    (ndlp->nlp_state == NLP_STE_NPR_NODE &&
		     ndlp->nlp_flag & NLP_DELAY_TMO)) {
			lpfc_printf_vlog(vport, KERN_WARNING, LOG_NODE,
					 "2784 PRLI cmpl: Allow Node recovery "
					 "DID x%06x nstate x%x nflag x%x\n",
					 ndlp->nlp_DID, ndlp->nlp_state,
					 ndlp->nlp_flag);
			goto out;
		}

		 
		if (vport->fc_flag & FC_PT2PT)
			goto out;

		 
		spin_lock_irq(&ndlp->lock);
		if (!(ndlp->fc4_xpt_flags & (SCSI_XPT_REGD | NVME_XPT_REGD)) &&
		    !ndlp->fc4_prli_sent) {
			ndlp->nlp_flag &= ~NLP_NPR_2B_DISC;
			if (!(ndlp->nlp_flag & NLP_IN_DEV_LOSS))
				release_node = true;
		}
		spin_unlock_irq(&ndlp->lock);

		if (release_node)
			lpfc_disc_state_machine(vport, ndlp, cmdiocb,
						NLP_EVT_DEVICE_RM);
	} else {
		 
		lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					NLP_EVT_CMPL_PRLI);
	}

out:
	lpfc_els_free_iocb(phba, cmdiocb);
	lpfc_nlp_put(ndlp);
	return;
}

 
int
lpfc_issue_els_prli(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		    uint8_t retry)
{
	int rc = 0;
	struct lpfc_hba *phba = vport->phba;
	PRLI *npr;
	struct lpfc_nvme_prli *npr_nvme;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;
	u32 local_nlp_type, elscmd;

	 
	if (phba->sli_rev == LPFC_SLI_REV4 &&
	    vport->fc_flag & FC_RSCN_MODE &&
	    vport->nvmei_support)
		ndlp->nlp_fc4_type |= NLP_FC4_NVME;
	local_nlp_type = ndlp->nlp_fc4_type;

	 
	ndlp->nlp_type &= ~(NLP_FCP_TARGET | NLP_FCP_INITIATOR);
	ndlp->nlp_type &= ~(NLP_NVME_TARGET | NLP_NVME_INITIATOR);
	ndlp->nlp_fcp_info &= ~NLP_FCP_2_DEVICE;
	ndlp->nlp_flag &= ~(NLP_FIRSTBURST | NLP_NPR_2B_DISC);
	ndlp->nvme_fb_size = 0;

 send_next_prli:
	if (local_nlp_type & NLP_FC4_FCP) {
		 
		cmdsize = (sizeof(uint32_t) + sizeof(PRLI));
		elscmd = ELS_CMD_PRLI;
	} else if (local_nlp_type & NLP_FC4_NVME) {
		 
		cmdsize = (sizeof(uint32_t) + sizeof(struct lpfc_nvme_prli));
		elscmd = ELS_CMD_NVMEPRLI;
	} else {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
				 "3083 Unknown FC_TYPE x%x ndlp x%06x\n",
				 ndlp->nlp_fc4_type, ndlp->nlp_DID);
		return 1;
	}

	 
	if (phba->sli_rev == LPFC_SLI_REV3 &&
	    ndlp->nlp_fc4_type == NLP_FC4_NVME) {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
				 "3088 Rport fc4 type 0x%x not supported by SLI3 adapter\n",
				 ndlp->nlp_type);
		lpfc_disc_state_machine(vport, ndlp, NULL, NLP_EVT_DEVICE_RM);
		return 1;
	}

	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, elscmd);
	if (!elsiocb)
		return 1;

	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;

	 
	memset(pcmd, 0, cmdsize);

	if (local_nlp_type & NLP_FC4_FCP) {
		 
		*((uint32_t *)(pcmd)) = ELS_CMD_PRLI;
		pcmd += sizeof(uint32_t);
		npr = (PRLI *)pcmd;

		 
		if (phba->vpd.rev.feaLevelHigh >= 0x02) {
			npr->ConfmComplAllowed = 1;
			npr->Retry = 1;
			npr->TaskRetryIdReq = 1;
		}
		npr->estabImagePair = 1;
		npr->readXferRdyDis = 1;
		if (vport->cfg_first_burst_size)
			npr->writeXferRdyDis = 1;

		 
		npr->prliType = PRLI_FCP_TYPE;
		npr->initiatorFunc = 1;
		elsiocb->cmd_flag |= LPFC_PRLI_FCP_REQ;

		 
		local_nlp_type &= ~NLP_FC4_FCP;
	} else if (local_nlp_type & NLP_FC4_NVME) {
		 
		*((uint32_t *)(pcmd)) = ELS_CMD_NVMEPRLI;
		pcmd += sizeof(uint32_t);
		npr_nvme = (struct lpfc_nvme_prli *)pcmd;
		bf_set(prli_type_code, npr_nvme, PRLI_NVME_TYPE);
		bf_set(prli_estabImagePair, npr_nvme, 0);   
		if (phba->nsler) {
			bf_set(prli_nsler, npr_nvme, 1);
			bf_set(prli_conf, npr_nvme, 1);
		}

		 
		if ((phba->cfg_nvme_enable_fb) &&
		    !phba->nvmet_support)
			bf_set(prli_fba, npr_nvme, 1);

		if (phba->nvmet_support) {
			bf_set(prli_tgt, npr_nvme, 1);
			bf_set(prli_disc, npr_nvme, 1);
		} else {
			bf_set(prli_init, npr_nvme, 1);
			bf_set(prli_conf, npr_nvme, 1);
		}

		npr_nvme->word1 = cpu_to_be32(npr_nvme->word1);
		npr_nvme->word4 = cpu_to_be32(npr_nvme->word4);
		elsiocb->cmd_flag |= LPFC_PRLI_NVME_REQ;

		 
		local_nlp_type &= ~NLP_FC4_NVME;
	}

	phba->fc_stat.elsXmitPRLI++;
	elsiocb->cmd_cmpl = lpfc_cmpl_els_prli;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
			      "Issue PRLI:  did:x%x refcnt %d",
			      ndlp->nlp_DID, kref_read(&ndlp->kref), 0);
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
		return 1;
	}

	 
	spin_lock_irq(&ndlp->lock);
	ndlp->nlp_flag |= NLP_PRLI_SND;
	vport->fc_prli_sent++;
	ndlp->fc4_prli_sent++;
	spin_unlock_irq(&ndlp->lock);

	 
	if (phba->sli_rev == LPFC_SLI_REV4 &&
	    local_nlp_type & (NLP_FC4_FCP | NLP_FC4_NVME))
		goto send_next_prli;
	else
		return 0;
}

 
static void
lpfc_rscn_disc(struct lpfc_vport *vport)
{
	lpfc_can_disctmo(vport);

	 
	 
	if (vport->fc_npr_cnt)
		if (lpfc_els_disc_plogi(vport))
			return;

	lpfc_end_rscn(vport);
}

 
static void
lpfc_adisc_done(struct lpfc_vport *vport)
{
	struct Scsi_Host   *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba   *phba = vport->phba;

	 
	if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
	    !(vport->fc_flag & FC_RSCN_MODE) &&
	    (phba->sli_rev < LPFC_SLI_REV4)) {

		 
		if (!lpfc_is_link_up(phba))
			return;

		 
		lpfc_issue_clear_la(phba, vport);
		lpfc_issue_reg_vpi(phba, vport);
		return;
	}
	 
	if (vport->port_state < LPFC_VPORT_READY) {
		 
		lpfc_issue_clear_la(phba, vport);
		if (!(vport->fc_flag & FC_ABORT_DISCOVERY)) {
			vport->num_disc_nodes = 0;
			 
			if (vport->fc_npr_cnt)
				lpfc_els_disc_plogi(vport);
			if (!vport->num_disc_nodes) {
				spin_lock_irq(shost->host_lock);
				vport->fc_flag &= ~FC_NDISC_ACTIVE;
				spin_unlock_irq(shost->host_lock);
				lpfc_can_disctmo(vport);
				lpfc_end_rscn(vport);
			}
		}
		vport->port_state = LPFC_VPORT_READY;
	} else
		lpfc_rscn_disc(vport);
}

 
void
lpfc_more_adisc(struct lpfc_vport *vport)
{
	if (vport->num_disc_nodes)
		vport->num_disc_nodes--;
	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0210 Continue discovery with %d ADISCs to go "
			 "Data: x%x x%x x%x\n",
			 vport->num_disc_nodes, vport->fc_adisc_cnt,
			 vport->fc_flag, vport->port_state);
	 
	if (vport->fc_flag & FC_NLP_MORE) {
		lpfc_set_disctmo(vport);
		 
		lpfc_els_disc_adisc(vport);
	}
	if (!vport->num_disc_nodes)
		lpfc_adisc_done(vport);
	return;
}

 
static void
lpfc_cmpl_els_adisc(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		    struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	IOCB_t *irsp;
	struct lpfc_nodelist *ndlp;
	int  disc;
	u32 ulp_status, ulp_word4, tmo;
	bool release_node = false;

	 
	cmdiocb->rsp_iocb = rspiocb;

	ndlp = cmdiocb->ndlp;

	ulp_status = get_job_ulpstatus(phba, rspiocb);
	ulp_word4 = get_job_word4(phba, rspiocb);

	if (phba->sli_rev == LPFC_SLI_REV4) {
		tmo = get_wqe_tmo(cmdiocb);
	} else {
		irsp = &rspiocb->iocb;
		tmo = irsp->ulpTimeout;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"ADISC cmpl:      status:x%x/x%x did:x%x",
		ulp_status, ulp_word4,
		ndlp->nlp_DID);

	 
	spin_lock_irq(&ndlp->lock);
	disc = (ndlp->nlp_flag & NLP_NPR_2B_DISC);
	ndlp->nlp_flag &= ~(NLP_ADISC_SND | NLP_NPR_2B_DISC);
	spin_unlock_irq(&ndlp->lock);
	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0104 ADISC completes to NPort x%x "
			 "Data: x%x x%x x%x x%x x%x\n",
			 ndlp->nlp_DID, ulp_status, ulp_word4,
			 tmo, disc, vport->num_disc_nodes);
	 
	if (lpfc_els_chk_latt(vport)) {
		spin_lock_irq(&ndlp->lock);
		ndlp->nlp_flag |= NLP_NPR_2B_DISC;
		spin_unlock_irq(&ndlp->lock);
		goto out;
	}

	if (ulp_status) {
		 
		if (lpfc_els_retry(phba, cmdiocb, rspiocb)) {
			 
			if (disc) {
				spin_lock_irq(&ndlp->lock);
				ndlp->nlp_flag |= NLP_NPR_2B_DISC;
				spin_unlock_irq(&ndlp->lock);
				lpfc_set_disctmo(vport);
			}
			goto out;
		}
		 
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "2755 ADISC failure DID:%06X Status:x%x/x%x\n",
				 ndlp->nlp_DID, ulp_status,
				 ulp_word4);
		lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					NLP_EVT_CMPL_ADISC);

		 
		spin_lock_irq(&ndlp->lock);
		if (!(ndlp->fc4_xpt_flags & (SCSI_XPT_REGD | NVME_XPT_REGD))) {
			ndlp->nlp_flag &= ~NLP_NPR_2B_DISC;
			if (!(ndlp->nlp_flag & NLP_IN_DEV_LOSS))
				release_node = true;
		}
		spin_unlock_irq(&ndlp->lock);

		if (release_node)
			lpfc_disc_state_machine(vport, ndlp, cmdiocb,
						NLP_EVT_DEVICE_RM);
	} else
		 
		lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					NLP_EVT_CMPL_ADISC);

	 
	if (disc && vport->num_disc_nodes)
		lpfc_more_adisc(vport);
out:
	lpfc_els_free_iocb(phba, cmdiocb);
	lpfc_nlp_put(ndlp);
	return;
}

 
int
lpfc_issue_els_adisc(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		     uint8_t retry)
{
	int rc = 0;
	struct lpfc_hba  *phba = vport->phba;
	ADISC *ap;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;

	cmdsize = (sizeof(uint32_t) + sizeof(ADISC));
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ADISC);
	if (!elsiocb)
		return 1;

	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;

	 
	*((uint32_t *) (pcmd)) = ELS_CMD_ADISC;
	pcmd += sizeof(uint32_t);

	 
	ap = (ADISC *) pcmd;
	ap->hardAL_PA = phba->fc_pref_ALPA;
	memcpy(&ap->portName, &vport->fc_portname, sizeof(struct lpfc_name));
	memcpy(&ap->nodeName, &vport->fc_nodename, sizeof(struct lpfc_name));
	ap->DID = be32_to_cpu(vport->fc_myDID);

	phba->fc_stat.elsXmitADISC++;
	elsiocb->cmd_cmpl = lpfc_cmpl_els_adisc;
	spin_lock_irq(&ndlp->lock);
	ndlp->nlp_flag |= NLP_ADISC_SND;
	spin_unlock_irq(&ndlp->lock);
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		goto err;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
			      "Issue ADISC:   did:x%x refcnt %d",
			      ndlp->nlp_DID, kref_read(&ndlp->kref), 0);

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
		goto err;
	}

	return 0;

err:
	spin_lock_irq(&ndlp->lock);
	ndlp->nlp_flag &= ~NLP_ADISC_SND;
	spin_unlock_irq(&ndlp->lock);
	return 1;
}

 
static void
lpfc_cmpl_els_logo(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		   struct lpfc_iocbq *rspiocb)
{
	struct lpfc_nodelist *ndlp = cmdiocb->ndlp;
	struct lpfc_vport *vport = ndlp->vport;
	IOCB_t *irsp;
	unsigned long flags;
	uint32_t skip_recovery = 0;
	int wake_up_waiter = 0;
	u32 ulp_status;
	u32 ulp_word4;
	u32 tmo;

	 
	cmdiocb->rsp_iocb = rspiocb;

	ulp_status = get_job_ulpstatus(phba, rspiocb);
	ulp_word4 = get_job_word4(phba, rspiocb);

	if (phba->sli_rev == LPFC_SLI_REV4) {
		tmo = get_wqe_tmo(cmdiocb);
	} else {
		irsp = &rspiocb->iocb;
		tmo = irsp->ulpTimeout;
	}

	spin_lock_irq(&ndlp->lock);
	ndlp->nlp_flag &= ~NLP_LOGO_SND;
	if (ndlp->save_flags & NLP_WAIT_FOR_LOGO) {
		wake_up_waiter = 1;
		ndlp->save_flags &= ~NLP_WAIT_FOR_LOGO;
	}
	spin_unlock_irq(&ndlp->lock);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"LOGO cmpl:       status:x%x/x%x did:x%x",
		ulp_status, ulp_word4,
		ndlp->nlp_DID);

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0105 LOGO completes to NPort x%x "
			 "refcnt %d nflags x%x Data: x%x x%x x%x x%x\n",
			 ndlp->nlp_DID, kref_read(&ndlp->kref), ndlp->nlp_flag,
			 ulp_status, ulp_word4,
			 tmo, vport->num_disc_nodes);

	if (lpfc_els_chk_latt(vport)) {
		skip_recovery = 1;
		goto out;
	}

	 
	if (ulp_status) {
		 
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "2756 LOGO failure, No Retry DID:%06X "
				 "Status:x%x/x%x\n",
				 ndlp->nlp_DID, ulp_status,
				 ulp_word4);

		if (lpfc_error_lost_link(vport, ulp_status, ulp_word4))
			skip_recovery = 1;
	}

	 
	lpfc_disc_state_machine(vport, ndlp, cmdiocb, NLP_EVT_CMPL_LOGO);

	if (skip_recovery)
		goto out;

	 
	if (ndlp->nlp_flag & NLP_TARGET_REMOVE) {
		spin_lock_irq(&ndlp->lock);
		if (phba->sli_rev == LPFC_SLI_REV4)
			ndlp->nlp_flag |= NLP_RELEASE_RPI;
		ndlp->nlp_flag &= ~NLP_NPR_2B_DISC;
		spin_unlock_irq(&ndlp->lock);
		lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					NLP_EVT_DEVICE_RM);
		goto out_rsrc_free;
	}

out:
	 

	if (wake_up_waiter && ndlp->logo_waitq)
		wake_up(ndlp->logo_waitq);
	 
	if (ndlp->nlp_type & (NLP_FCP_TARGET | NLP_NVME_TARGET) &&
	    skip_recovery == 0) {
		lpfc_cancel_retry_delay_tmo(vport, ndlp);
		spin_lock_irqsave(&ndlp->lock, flags);
		ndlp->nlp_flag |= NLP_NPR_2B_DISC;
		spin_unlock_irqrestore(&ndlp->lock, flags);

		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
				 "3187 LOGO completes to NPort x%x: Start "
				 "Recovery Data: x%x x%x x%x x%x\n",
				 ndlp->nlp_DID, ulp_status,
				 ulp_word4, tmo,
				 vport->num_disc_nodes);

		lpfc_els_free_iocb(phba, cmdiocb);
		lpfc_nlp_put(ndlp);

		lpfc_disc_start(vport);
		return;
	}

	 
	if (!(ndlp->fc4_xpt_flags & (SCSI_XPT_REGD | NVME_XPT_REGD))) {
		spin_lock_irq(&ndlp->lock);
		ndlp->nlp_flag &= ~NLP_NPR_2B_DISC;
		spin_unlock_irq(&ndlp->lock);
		lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					NLP_EVT_DEVICE_RM);
	}
out_rsrc_free:
	 
	lpfc_els_free_iocb(phba, cmdiocb);
	lpfc_nlp_put(ndlp);
}

 
int
lpfc_issue_els_logo(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		    uint8_t retry)
{
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int rc;

	spin_lock_irq(&ndlp->lock);
	if (ndlp->nlp_flag & NLP_LOGO_SND) {
		spin_unlock_irq(&ndlp->lock);
		return 0;
	}
	spin_unlock_irq(&ndlp->lock);

	cmdsize = (2 * sizeof(uint32_t)) + sizeof(struct lpfc_name);
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_LOGO);
	if (!elsiocb)
		return 1;

	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;
	*((uint32_t *) (pcmd)) = ELS_CMD_LOGO;
	pcmd += sizeof(uint32_t);

	 
	*((uint32_t *) (pcmd)) = be32_to_cpu(vport->fc_myDID);
	pcmd += sizeof(uint32_t);
	memcpy(pcmd, &vport->fc_portname, sizeof(struct lpfc_name));

	phba->fc_stat.elsXmitLOGO++;
	elsiocb->cmd_cmpl = lpfc_cmpl_els_logo;
	spin_lock_irq(&ndlp->lock);
	ndlp->nlp_flag |= NLP_LOGO_SND;
	ndlp->nlp_flag &= ~NLP_ISSUE_LOGO;
	spin_unlock_irq(&ndlp->lock);
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		goto err;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
			      "Issue LOGO:      did:x%x refcnt %d",
			      ndlp->nlp_DID, kref_read(&ndlp->kref), 0);

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
		goto err;
	}

	spin_lock_irq(&ndlp->lock);
	ndlp->nlp_prev_state = ndlp->nlp_state;
	spin_unlock_irq(&ndlp->lock);
	lpfc_nlp_set_state(vport, ndlp, NLP_STE_LOGO_ISSUE);
	return 0;

err:
	spin_lock_irq(&ndlp->lock);
	ndlp->nlp_flag &= ~NLP_LOGO_SND;
	spin_unlock_irq(&ndlp->lock);
	return 1;
}

 
static void
lpfc_cmpl_els_cmd(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct lpfc_nodelist *free_ndlp;
	IOCB_t *irsp;
	u32 ulp_status, ulp_word4, tmo, did, iotag;

	ulp_status = get_job_ulpstatus(phba, rspiocb);
	ulp_word4 = get_job_word4(phba, rspiocb);
	did = get_job_els_rsp64_did(phba, cmdiocb);

	if (phba->sli_rev == LPFC_SLI_REV4) {
		tmo = get_wqe_tmo(cmdiocb);
		iotag = get_wqe_reqtag(cmdiocb);
	} else {
		irsp = &rspiocb->iocb;
		tmo = irsp->ulpTimeout;
		iotag = irsp->ulpIoTag;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
			      "ELS cmd cmpl:    status:x%x/x%x did:x%x",
			      ulp_status, ulp_word4, did);

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0106 ELS cmd tag x%x completes Data: x%x x%x x%x\n",
			 iotag, ulp_status, ulp_word4, tmo);

	 
	lpfc_els_chk_latt(vport);

	free_ndlp = cmdiocb->ndlp;

	lpfc_els_free_iocb(phba, cmdiocb);
	lpfc_nlp_put(free_ndlp);
}

 
static int
lpfc_reg_fab_ctrl_node(struct lpfc_vport *vport, struct lpfc_nodelist *fc_ndlp)
{
	int rc = 0;
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_nodelist *ns_ndlp;
	LPFC_MBOXQ_t *mbox;

	if (fc_ndlp->nlp_flag & NLP_RPI_REGISTERED)
		return rc;

	ns_ndlp = lpfc_findnode_did(vport, NameServer_DID);
	if (!ns_ndlp)
		return -ENODEV;

	lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE,
			 "0935 %s: Reg FC RPI x%x on FC DID x%x NSSte: x%x\n",
			 __func__, fc_ndlp->nlp_rpi, fc_ndlp->nlp_DID,
			 ns_ndlp->nlp_state);
	if (ns_ndlp->nlp_state != NLP_STE_UNMAPPED_NODE)
		return -ENODEV;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_NODE,
				 "0936 %s: no memory for reg_login "
				 "Data: x%x x%x x%x x%x\n", __func__,
				 fc_ndlp->nlp_DID, fc_ndlp->nlp_state,
				 fc_ndlp->nlp_flag, fc_ndlp->nlp_rpi);
		return -ENOMEM;
	}
	rc = lpfc_reg_rpi(phba, vport->vpi, fc_ndlp->nlp_DID,
			  (u8 *)&vport->fc_sparam, mbox, fc_ndlp->nlp_rpi);
	if (rc) {
		rc = -EACCES;
		goto out;
	}

	fc_ndlp->nlp_flag |= NLP_REG_LOGIN_SEND;
	mbox->mbox_cmpl = lpfc_mbx_cmpl_fc_reg_login;
	mbox->ctx_ndlp = lpfc_nlp_get(fc_ndlp);
	if (!mbox->ctx_ndlp) {
		rc = -ENOMEM;
		goto out;
	}

	mbox->vport = vport;
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		rc = -ENODEV;
		lpfc_nlp_put(fc_ndlp);
		goto out;
	}
	 
	lpfc_nlp_set_state(vport, fc_ndlp,
			   NLP_STE_REG_LOGIN_ISSUE);
	return 0;

 out:
	lpfc_mbox_rsrc_cleanup(phba, mbox, MBOX_THD_UNLOCKED);
	lpfc_printf_vlog(vport, KERN_ERR, LOG_NODE,
			 "0938 %s: failed to format reg_login "
			 "Data: x%x x%x x%x x%x\n", __func__,
			 fc_ndlp->nlp_DID, fc_ndlp->nlp_state,
			 fc_ndlp->nlp_flag, fc_ndlp->nlp_rpi);
	return rc;
}

 
static void
lpfc_cmpl_els_disc_cmd(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		       struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	IOCB_t *irsp;
	struct lpfc_els_rdf_rsp *prdf;
	struct lpfc_dmabuf *pcmd, *prsp;
	u32 *pdata;
	u32 cmd;
	struct lpfc_nodelist *ndlp = cmdiocb->ndlp;
	u32 ulp_status, ulp_word4, tmo, did, iotag;

	ulp_status = get_job_ulpstatus(phba, rspiocb);
	ulp_word4 = get_job_word4(phba, rspiocb);
	did = get_job_els_rsp64_did(phba, cmdiocb);

	if (phba->sli_rev == LPFC_SLI_REV4) {
		tmo = get_wqe_tmo(cmdiocb);
		iotag = get_wqe_reqtag(cmdiocb);
	} else {
		irsp = &rspiocb->iocb;
		tmo = irsp->ulpTimeout;
		iotag = irsp->ulpIoTag;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"ELS cmd cmpl:    status:x%x/x%x did:x%x",
		ulp_status, ulp_word4, did);

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS | LOG_CGN_MGMT,
			 "0217 ELS cmd tag x%x completes Data: x%x x%x x%x x%x\n",
			 iotag, ulp_status, ulp_word4, tmo, cmdiocb->retry);

	pcmd = cmdiocb->cmd_dmabuf;
	if (!pcmd)
		goto out;

	pdata = (u32 *)pcmd->virt;
	if (!pdata)
		goto out;
	cmd = *pdata;

	 
	if (ulp_status == IOSTAT_LOCAL_REJECT &&
	    ((ulp_word4 & IOERR_PARAM_MASK) ==
	    IOERR_SEQUENCE_TIMEOUT)) {
		cmdiocb->retry++;
		if (cmdiocb->retry <= 1) {
			switch (cmd) {
			case ELS_CMD_SCR:
				lpfc_issue_els_scr(vport, cmdiocb->retry);
				break;
			case ELS_CMD_EDC:
				lpfc_issue_els_edc(vport, cmdiocb->retry);
				break;
			case ELS_CMD_RDF:
				lpfc_issue_els_rdf(vport, cmdiocb->retry);
				break;
			}
			goto out;
		}
		phba->fc_stat.elsRetryExceeded++;
	}
	if (cmd == ELS_CMD_EDC) {
		 
		lpfc_cmpl_els_edc(phba, cmdiocb, rspiocb);
		return;
	}
	if (ulp_status) {
		 
		lpfc_printf_vlog(vport, KERN_WARNING, LOG_ELS | LOG_CGN_MGMT,
				 "4203 ELS cmd x%x error: x%x x%X\n", cmd,
				 ulp_status, ulp_word4);
		goto out;
	}

	 
	if (cmd == ELS_CMD_RDF) {
		int i;

		prsp = list_get_first(&pcmd->list, struct lpfc_dmabuf, list);
		if (!prsp)
			goto out;

		prdf = (struct lpfc_els_rdf_rsp *)prsp->virt;
		if (!prdf)
			goto out;

		for (i = 0; i < ELS_RDF_REG_TAG_CNT &&
			    i < be32_to_cpu(prdf->reg_d1.reg_desc.count); i++)
			lpfc_printf_vlog(vport, KERN_INFO,
					 LOG_ELS | LOG_CGN_MGMT,
					 "4677 Fabric RDF Notification Grant "
					 "Data: 0x%08x Reg: %x %x\n",
					 be32_to_cpu(
						 prdf->reg_d1.desc_tags[i]),
					 phba->cgn_reg_signal,
					 phba->cgn_reg_fpin);
	}

out:
	 
	lpfc_els_chk_latt(vport);
	lpfc_els_free_iocb(phba, cmdiocb);
	lpfc_nlp_put(ndlp);
	return;
}

 
int
lpfc_issue_els_scr(struct lpfc_vport *vport, uint8_t retry)
{
	int rc = 0;
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;
	struct lpfc_nodelist *ndlp;

	cmdsize = (sizeof(uint32_t) + sizeof(SCR));

	ndlp = lpfc_findnode_did(vport, Fabric_Cntl_DID);
	if (!ndlp) {
		ndlp = lpfc_nlp_init(vport, Fabric_Cntl_DID);
		if (!ndlp)
			return 1;
		lpfc_enqueue_node(vport, ndlp);
	}

	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_SCR);
	if (!elsiocb)
		return 1;

	if (phba->sli_rev == LPFC_SLI_REV4) {
		rc = lpfc_reg_fab_ctrl_node(vport, ndlp);
		if (rc) {
			lpfc_els_free_iocb(phba, elsiocb);
			lpfc_printf_vlog(vport, KERN_ERR, LOG_NODE,
					 "0937 %s: Failed to reg fc node, rc %d\n",
					 __func__, rc);
			return 1;
		}
	}
	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;

	*((uint32_t *) (pcmd)) = ELS_CMD_SCR;
	pcmd += sizeof(uint32_t);

	 
	memset(pcmd, 0, sizeof(SCR));
	((SCR *) pcmd)->Function = SCR_FUNC_FULL;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue SCR:       did:x%x",
		ndlp->nlp_DID, 0, 0);

	phba->fc_stat.elsXmitSCR++;
	elsiocb->cmd_cmpl = lpfc_cmpl_els_disc_cmd;
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
			      "Issue SCR:     did:x%x refcnt %d",
			      ndlp->nlp_DID, kref_read(&ndlp->kref), 0);

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
		return 1;
	}

	return 0;
}

 
int
lpfc_issue_els_rscn(struct lpfc_vport *vport, uint8_t retry)
{
	int rc = 0;
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_nodelist *ndlp;
	struct {
		struct fc_els_rscn rscn;
		struct fc_els_rscn_page portid;
	} *event;
	uint32_t nportid;
	uint16_t cmdsize = sizeof(*event);

	 
	if (phba->fc_topology == LPFC_TOPOLOGY_LOOP &&
	    !(vport->fc_flag & FC_PUBLIC_LOOP))
		return 1;

	if (vport->fc_flag & FC_PT2PT) {
		 
		ndlp = lpfc_findnode_mapped(vport);
		if (!ndlp)
			return 1;
	} else {
		nportid = FC_FID_FCTRL;
		 
		ndlp = lpfc_findnode_did(vport, nportid);
		if (!ndlp) {
			 
			ndlp = lpfc_nlp_init(vport, nportid);
			if (!ndlp)
				return 1;
			lpfc_enqueue_node(vport, ndlp);
		}
	}

	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_RSCN_XMT);

	if (!elsiocb)
		return 1;

	event = elsiocb->cmd_dmabuf->virt;

	event->rscn.rscn_cmd = ELS_RSCN;
	event->rscn.rscn_page_len = sizeof(struct fc_els_rscn_page);
	event->rscn.rscn_plen = cpu_to_be16(cmdsize);

	nportid = vport->fc_myDID;
	 
	event->portid.rscn_page_flags = 0;
	event->portid.rscn_fid[0] = (nportid & 0x00FF0000) >> 16;
	event->portid.rscn_fid[1] = (nportid & 0x0000FF00) >> 8;
	event->portid.rscn_fid[2] = nportid & 0x000000FF;

	phba->fc_stat.elsXmitRSCN++;
	elsiocb->cmd_cmpl = lpfc_cmpl_els_cmd;
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
			      "Issue RSCN:       did:x%x",
			      ndlp->nlp_DID, 0, 0);

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
		return 1;
	}

	return 0;
}

 
static int
lpfc_issue_els_farpr(struct lpfc_vport *vport, uint32_t nportid, uint8_t retry)
{
	int rc = 0;
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_iocbq *elsiocb;
	FARP *fp;
	uint8_t *pcmd;
	uint32_t *lp;
	uint16_t cmdsize;
	struct lpfc_nodelist *ondlp;
	struct lpfc_nodelist *ndlp;

	cmdsize = (sizeof(uint32_t) + sizeof(FARP));

	ndlp = lpfc_findnode_did(vport, nportid);
	if (!ndlp) {
		ndlp = lpfc_nlp_init(vport, nportid);
		if (!ndlp)
			return 1;
		lpfc_enqueue_node(vport, ndlp);
	}

	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_FARPR);
	if (!elsiocb)
		return 1;

	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;

	*((uint32_t *) (pcmd)) = ELS_CMD_FARPR;
	pcmd += sizeof(uint32_t);

	 
	fp = (FARP *) (pcmd);
	memset(fp, 0, sizeof(FARP));
	lp = (uint32_t *) pcmd;
	*lp++ = be32_to_cpu(nportid);
	*lp++ = be32_to_cpu(vport->fc_myDID);
	fp->Rflags = 0;
	fp->Mflags = (FARP_MATCH_PORT | FARP_MATCH_NODE);

	memcpy(&fp->RportName, &vport->fc_portname, sizeof(struct lpfc_name));
	memcpy(&fp->RnodeName, &vport->fc_nodename, sizeof(struct lpfc_name));
	ondlp = lpfc_findnode_did(vport, nportid);
	if (ondlp) {
		memcpy(&fp->OportName, &ondlp->nlp_portname,
		       sizeof(struct lpfc_name));
		memcpy(&fp->OnodeName, &ondlp->nlp_nodename,
		       sizeof(struct lpfc_name));
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue FARPR:     did:x%x",
		ndlp->nlp_DID, 0, 0);

	phba->fc_stat.elsXmitFARPR++;
	elsiocb->cmd_cmpl = lpfc_cmpl_els_cmd;
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		 
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
		return 1;
	}
	 
	 
	return 0;
}

 
int
lpfc_issue_els_rdf(struct lpfc_vport *vport, uint8_t retry)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_els_rdf_req *prdf;
	struct lpfc_nodelist *ndlp;
	uint16_t cmdsize;
	int rc;

	cmdsize = sizeof(*prdf);

	ndlp = lpfc_findnode_did(vport, Fabric_Cntl_DID);
	if (!ndlp) {
		ndlp = lpfc_nlp_init(vport, Fabric_Cntl_DID);
		if (!ndlp)
			return -ENODEV;
		lpfc_enqueue_node(vport, ndlp);
	}

	 
	if (vport->port_type == LPFC_NPIV_PORT)
		return -EACCES;

	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_RDF);
	if (!elsiocb)
		return -ENOMEM;

	 
	prdf = (struct lpfc_els_rdf_req *)elsiocb->cmd_dmabuf->virt;
	memset(prdf, 0, cmdsize);
	prdf->rdf.fpin_cmd = ELS_RDF;
	prdf->rdf.desc_len = cpu_to_be32(sizeof(struct lpfc_els_rdf_req) -
					 sizeof(struct fc_els_rdf));
	prdf->reg_d1.reg_desc.desc_tag = cpu_to_be32(ELS_DTAG_FPIN_REGISTER);
	prdf->reg_d1.reg_desc.desc_len = cpu_to_be32(
				FC_TLV_DESC_LENGTH_FROM_SZ(prdf->reg_d1));
	prdf->reg_d1.reg_desc.count = cpu_to_be32(ELS_RDF_REG_TAG_CNT);
	prdf->reg_d1.desc_tags[0] = cpu_to_be32(ELS_DTAG_LNK_INTEGRITY);
	prdf->reg_d1.desc_tags[1] = cpu_to_be32(ELS_DTAG_DELIVERY);
	prdf->reg_d1.desc_tags[2] = cpu_to_be32(ELS_DTAG_PEER_CONGEST);
	prdf->reg_d1.desc_tags[3] = cpu_to_be32(ELS_DTAG_CONGESTION);

	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS | LOG_CGN_MGMT,
			 "6444 Xmit RDF to remote NPORT x%x Reg: %x %x\n",
			 ndlp->nlp_DID, phba->cgn_reg_signal,
			 phba->cgn_reg_fpin);

	phba->cgn_fpin_frequency = LPFC_FPIN_INIT_FREQ;
	elsiocb->cmd_cmpl = lpfc_cmpl_els_disc_cmd;
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		return -EIO;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
			      "Issue RDF:     did:x%x refcnt %d",
			      ndlp->nlp_DID, kref_read(&ndlp->kref), 0);

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
		return -EIO;
	}
	return 0;
}

  
static int
lpfc_els_rcv_rdf(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		 struct lpfc_nodelist *ndlp)
{
	 
	if (lpfc_els_rsp_acc(vport, ELS_CMD_RDF, cmdiocb, ndlp, NULL)) {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS | LOG_CGN_MGMT,
				 "1623 Failed to RDF_ACC from x%x for x%x\n",
				 ndlp->nlp_DID, vport->fc_myDID);
		return -EIO;
	}

	 
	if (lpfc_issue_els_rdf(vport, 0)) {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS | LOG_CGN_MGMT,
				 "2623 Failed to re register RDF for x%x\n",
				 vport->fc_myDID);
		return -EIO;
	}

	return 0;
}

 
static void
lpfc_least_capable_settings(struct lpfc_hba *phba,
			    struct fc_diag_cg_sig_desc *pcgd)
{
	u32 rsp_sig_cap = 0, drv_sig_cap = 0;
	u32 rsp_sig_freq_cyc = 0, rsp_sig_freq_scale = 0;

	 
	rsp_sig_cap = be32_to_cpu(pcgd->xmt_signal_capability);
	rsp_sig_freq_cyc = be16_to_cpu(pcgd->xmt_signal_frequency.count);
	rsp_sig_freq_scale = be16_to_cpu(pcgd->xmt_signal_frequency.units);

	 
	if (rsp_sig_cap == EDC_CG_SIG_NOTSUPPORTED)
		goto out_no_support;

	 
	switch (rsp_sig_freq_scale) {
	case EDC_CG_SIGFREQ_SEC:
		rsp_sig_freq_cyc *= MSEC_PER_SEC;
		break;
	case EDC_CG_SIGFREQ_MSEC:
		rsp_sig_freq_cyc = 1;
		break;
	default:
		goto out_no_support;
	}

	 
	drv_sig_cap = phba->cgn_reg_signal;

	 
	if (rsp_sig_freq_cyc > phba->cgn_sig_freq)
		phba->cgn_sig_freq = rsp_sig_freq_cyc;

	 
	phba->cgn_reg_fpin = LPFC_CGN_FPIN_WARN | LPFC_CGN_FPIN_ALARM;
	phba->cgn_reg_signal = EDC_CG_SIG_NOTSUPPORTED;
	if (rsp_sig_cap == EDC_CG_SIG_WARN_ONLY &&
	    (drv_sig_cap == EDC_CG_SIG_WARN_ONLY ||
	     drv_sig_cap == EDC_CG_SIG_WARN_ALARM)) {
		phba->cgn_reg_signal = EDC_CG_SIG_WARN_ONLY;
		phba->cgn_reg_fpin &= ~LPFC_CGN_FPIN_WARN;
	}
	if (rsp_sig_cap == EDC_CG_SIG_WARN_ALARM) {
		if (drv_sig_cap == EDC_CG_SIG_WARN_ALARM) {
			phba->cgn_reg_signal = EDC_CG_SIG_WARN_ALARM;
			phba->cgn_reg_fpin = LPFC_CGN_FPIN_NONE;
		}
		if (drv_sig_cap == EDC_CG_SIG_WARN_ONLY) {
			phba->cgn_reg_signal = EDC_CG_SIG_WARN_ONLY;
			phba->cgn_reg_fpin &= ~LPFC_CGN_FPIN_WARN;
		}
	}

	 
	return;

out_no_support:
	phba->cgn_reg_signal = EDC_CG_SIG_NOTSUPPORTED;
	phba->cgn_sig_freq = 0;
	phba->cgn_reg_fpin = LPFC_CGN_FPIN_ALARM | LPFC_CGN_FPIN_WARN;
}

DECLARE_ENUM2STR_LOOKUP(lpfc_get_tlv_dtag_nm, fc_ls_tlv_dtag,
			FC_LS_TLV_DTAG_INIT);

 
static void
lpfc_cmpl_els_edc(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_iocbq *rspiocb)
{
	IOCB_t *irsp_iocb;
	struct fc_els_edc_resp *edc_rsp;
	struct fc_tlv_desc *tlv;
	struct fc_diag_cg_sig_desc *pcgd;
	struct fc_diag_lnkflt_desc *plnkflt;
	struct lpfc_dmabuf *pcmd, *prsp;
	const char *dtag_nm;
	u32 *pdata, dtag;
	int desc_cnt = 0, bytes_remain;
	bool rcv_cap_desc = false;
	struct lpfc_nodelist *ndlp;
	u32 ulp_status, ulp_word4, tmo, did, iotag;

	ndlp = cmdiocb->ndlp;

	ulp_status = get_job_ulpstatus(phba, rspiocb);
	ulp_word4 = get_job_word4(phba, rspiocb);
	did = get_job_els_rsp64_did(phba, rspiocb);

	if (phba->sli_rev == LPFC_SLI_REV4) {
		tmo = get_wqe_tmo(rspiocb);
		iotag = get_wqe_reqtag(rspiocb);
	} else {
		irsp_iocb = &rspiocb->iocb;
		tmo = irsp_iocb->ulpTimeout;
		iotag = irsp_iocb->ulpIoTag;
	}

	lpfc_debugfs_disc_trc(phba->pport, LPFC_DISC_TRC_ELS_CMD,
			      "EDC cmpl:    status:x%x/x%x did:x%x",
			      ulp_status, ulp_word4, did);

	 
	lpfc_printf_log(phba, KERN_INFO, LOG_ELS | LOG_CGN_MGMT,
			"4201 EDC cmd tag x%x completes Data: x%x x%x x%x\n",
			iotag, ulp_status, ulp_word4, tmo);

	pcmd = cmdiocb->cmd_dmabuf;
	if (!pcmd)
		goto out;

	pdata = (u32 *)pcmd->virt;
	if (!pdata)
		goto out;

	 
	if (ulp_status)
		goto out;

	prsp = list_get_first(&pcmd->list, struct lpfc_dmabuf, list);
	if (!prsp)
		goto out;

	edc_rsp = prsp->virt;
	if (!edc_rsp)
		goto out;

	 
	lpfc_printf_log(phba, KERN_INFO,
			LOG_ELS | LOG_CGN_MGMT | LOG_LDS_EVENT,
			"4676 Fabric EDC Rsp: "
			"0x%02x, 0x%08x\n",
			edc_rsp->acc_hdr.la_cmd,
			be32_to_cpu(edc_rsp->desc_list_len));

	 
	bytes_remain = be32_to_cpu(edc_rsp->desc_list_len) -
				   sizeof(struct fc_els_lsri_desc);
	if (bytes_remain <= 0)
		goto out;

	tlv = edc_rsp->desc;

	 
	while (bytes_remain) {
		if (bytes_remain < FC_TLV_DESC_HDR_SZ) {
			lpfc_printf_log(phba, KERN_WARNING, LOG_CGN_MGMT,
					"6461 Truncated TLV hdr on "
					"Diagnostic descriptor[%d]\n",
					desc_cnt);
			goto out;
		}

		dtag = be32_to_cpu(tlv->desc_tag);
		switch (dtag) {
		case ELS_DTAG_LNK_FAULT_CAP:
			if (bytes_remain < FC_TLV_DESC_SZ_FROM_LENGTH(tlv) ||
			    FC_TLV_DESC_SZ_FROM_LENGTH(tlv) !=
					sizeof(struct fc_diag_lnkflt_desc)) {
				lpfc_printf_log(phba, KERN_WARNING,
					LOG_ELS | LOG_CGN_MGMT | LOG_LDS_EVENT,
					"6462 Truncated Link Fault Diagnostic "
					"descriptor[%d]: %d vs 0x%zx 0x%zx\n",
					desc_cnt, bytes_remain,
					FC_TLV_DESC_SZ_FROM_LENGTH(tlv),
					sizeof(struct fc_diag_lnkflt_desc));
				goto out;
			}
			plnkflt = (struct fc_diag_lnkflt_desc *)tlv;
			lpfc_printf_log(phba, KERN_INFO,
				LOG_ELS | LOG_LDS_EVENT,
				"4617 Link Fault Desc Data: 0x%08x 0x%08x "
				"0x%08x 0x%08x 0x%08x\n",
				be32_to_cpu(plnkflt->desc_tag),
				be32_to_cpu(plnkflt->desc_len),
				be32_to_cpu(
					plnkflt->degrade_activate_threshold),
				be32_to_cpu(
					plnkflt->degrade_deactivate_threshold),
				be32_to_cpu(plnkflt->fec_degrade_interval));
			break;
		case ELS_DTAG_CG_SIGNAL_CAP:
			if (bytes_remain < FC_TLV_DESC_SZ_FROM_LENGTH(tlv) ||
			    FC_TLV_DESC_SZ_FROM_LENGTH(tlv) !=
					sizeof(struct fc_diag_cg_sig_desc)) {
				lpfc_printf_log(
					phba, KERN_WARNING, LOG_CGN_MGMT,
					"6463 Truncated Cgn Signal Diagnostic "
					"descriptor[%d]: %d vs 0x%zx 0x%zx\n",
					desc_cnt, bytes_remain,
					FC_TLV_DESC_SZ_FROM_LENGTH(tlv),
					sizeof(struct fc_diag_cg_sig_desc));
				goto out;
			}

			pcgd = (struct fc_diag_cg_sig_desc *)tlv;
			lpfc_printf_log(
				phba, KERN_INFO, LOG_ELS | LOG_CGN_MGMT,
				"4616 CGN Desc Data: 0x%08x 0x%08x "
				"0x%08x 0x%04x 0x%04x 0x%08x 0x%04x 0x%04x\n",
				be32_to_cpu(pcgd->desc_tag),
				be32_to_cpu(pcgd->desc_len),
				be32_to_cpu(pcgd->xmt_signal_capability),
				be16_to_cpu(pcgd->xmt_signal_frequency.count),
				be16_to_cpu(pcgd->xmt_signal_frequency.units),
				be32_to_cpu(pcgd->rcv_signal_capability),
				be16_to_cpu(pcgd->rcv_signal_frequency.count),
				be16_to_cpu(pcgd->rcv_signal_frequency.units));

			 
			lpfc_least_capable_settings(phba, pcgd);
			rcv_cap_desc = true;
			break;
		default:
			dtag_nm = lpfc_get_tlv_dtag_nm(dtag);
			lpfc_printf_log(phba, KERN_WARNING, LOG_CGN_MGMT,
					"4919 unknown Diagnostic "
					"Descriptor[%d]: tag x%x (%s)\n",
					desc_cnt, dtag, dtag_nm);
		}

		bytes_remain -= FC_TLV_DESC_SZ_FROM_LENGTH(tlv);
		tlv = fc_tlv_next_desc(tlv);
		desc_cnt++;
	}

out:
	if (!rcv_cap_desc) {
		phba->cgn_reg_fpin = LPFC_CGN_FPIN_ALARM | LPFC_CGN_FPIN_WARN;
		phba->cgn_reg_signal = EDC_CG_SIG_NOTSUPPORTED;
		phba->cgn_sig_freq = 0;
		lpfc_printf_log(phba, KERN_WARNING, LOG_ELS | LOG_CGN_MGMT,
				"4202 EDC rsp error - sending RDF "
				"for FPIN only.\n");
	}

	lpfc_config_cgn_signal(phba);

	 
	lpfc_els_chk_latt(phba->pport);
	lpfc_debugfs_disc_trc(phba->pport, LPFC_DISC_TRC_ELS_CMD,
			      "EDC Cmpl:     did:x%x refcnt %d",
			      ndlp->nlp_DID, kref_read(&ndlp->kref), 0);
	lpfc_els_free_iocb(phba, cmdiocb);
	lpfc_nlp_put(ndlp);
}

static void
lpfc_format_edc_lft_desc(struct lpfc_hba *phba, struct fc_tlv_desc *tlv)
{
	struct fc_diag_lnkflt_desc *lft = (struct fc_diag_lnkflt_desc *)tlv;

	lft->desc_tag = cpu_to_be32(ELS_DTAG_LNK_FAULT_CAP);
	lft->desc_len = cpu_to_be32(
		FC_TLV_DESC_LENGTH_FROM_SZ(struct fc_diag_lnkflt_desc));

	lft->degrade_activate_threshold =
		cpu_to_be32(phba->degrade_activate_threshold);
	lft->degrade_deactivate_threshold =
		cpu_to_be32(phba->degrade_deactivate_threshold);
	lft->fec_degrade_interval = cpu_to_be32(phba->fec_degrade_interval);
}

static void
lpfc_format_edc_cgn_desc(struct lpfc_hba *phba, struct fc_tlv_desc *tlv)
{
	struct fc_diag_cg_sig_desc *cgd = (struct fc_diag_cg_sig_desc *)tlv;

	 

	 
	cgd->desc_tag = cpu_to_be32(ELS_DTAG_CG_SIGNAL_CAP);

	 
	cgd->desc_len = cpu_to_be32(
		FC_TLV_DESC_LENGTH_FROM_SZ(struct fc_diag_cg_sig_desc));

	 

	if (phba->cmf_active_mode == LPFC_CFG_OFF) {
		 
		phba->cgn_sig_freq = 0;
		return;
	}
	switch (phba->cgn_reg_signal) {
	case EDC_CG_SIG_WARN_ONLY:
		cgd->rcv_signal_capability = cpu_to_be32(EDC_CG_SIG_WARN_ONLY);
		break;
	case EDC_CG_SIG_WARN_ALARM:
		cgd->rcv_signal_capability = cpu_to_be32(EDC_CG_SIG_WARN_ALARM);
		break;
	default:
		 
		break;
	}

	 
	cgd->rcv_signal_frequency.count =
		cpu_to_be16(lpfc_fabric_cgn_frequency);
	cgd->rcv_signal_frequency.units =
		cpu_to_be16(EDC_CG_SIGFREQ_MSEC);
}

static bool
lpfc_link_is_lds_capable(struct lpfc_hba *phba)
{
	if (!(phba->lmt & LMT_64Gb))
		return false;
	if (phba->sli_rev != LPFC_SLI_REV4)
		return false;

	if (phba->sli4_hba.conf_trunk) {
		if (phba->trunk_link.phy_lnk_speed == LPFC_USER_LINK_SPEED_64G)
			return true;
	} else if (phba->fc_linkspeed == LPFC_LINK_SPEED_64GHZ) {
		return true;
	}
	return false;
}

  
int
lpfc_issue_els_edc(struct lpfc_vport *vport, uint8_t retry)
{
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_iocbq *elsiocb;
	struct fc_els_edc *edc_req;
	struct fc_tlv_desc *tlv;
	u16 cmdsize;
	struct lpfc_nodelist *ndlp;
	u8 *pcmd = NULL;
	u32 cgn_desc_size, lft_desc_size;
	int rc;

	if (vport->port_type == LPFC_NPIV_PORT)
		return -EACCES;

	ndlp = lpfc_findnode_did(vport, Fabric_DID);
	if (!ndlp || ndlp->nlp_state != NLP_STE_UNMAPPED_NODE)
		return -ENODEV;

	cgn_desc_size = (phba->cgn_init_reg_signal) ?
				sizeof(struct fc_diag_cg_sig_desc) : 0;
	lft_desc_size = (lpfc_link_is_lds_capable(phba)) ?
				sizeof(struct fc_diag_lnkflt_desc) : 0;
	cmdsize = cgn_desc_size + lft_desc_size;

	 
	if (!cmdsize)
		goto try_rdf;

	cmdsize += sizeof(struct fc_els_edc);
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_EDC);
	if (!elsiocb)
		goto try_rdf;

	 
	pcmd = (u8 *)elsiocb->cmd_dmabuf->virt;
	memset(pcmd, 0, cmdsize);
	edc_req = (struct fc_els_edc *)pcmd;
	edc_req->desc_len = cpu_to_be32(cgn_desc_size + lft_desc_size);
	edc_req->edc_cmd = ELS_EDC;
	tlv = edc_req->desc;

	if (cgn_desc_size) {
		lpfc_format_edc_cgn_desc(phba, tlv);
		phba->cgn_sig_freq = lpfc_fabric_cgn_frequency;
		tlv = fc_tlv_next_desc(tlv);
	}

	if (lft_desc_size)
		lpfc_format_edc_lft_desc(phba, tlv);

	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS | LOG_CGN_MGMT,
			 "4623 Xmit EDC to remote "
			 "NPORT x%x reg_sig x%x reg_fpin:x%x\n",
			 ndlp->nlp_DID, phba->cgn_reg_signal,
			 phba->cgn_reg_fpin);

	elsiocb->cmd_cmpl = lpfc_cmpl_els_disc_cmd;
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		return -EIO;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
			      "Issue EDC:     did:x%x refcnt %d",
			      ndlp->nlp_DID, kref_read(&ndlp->kref), 0);
	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		 
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
		goto try_rdf;
	}
	return 0;
try_rdf:
	phba->cgn_reg_fpin = LPFC_CGN_FPIN_WARN | LPFC_CGN_FPIN_ALARM;
	phba->cgn_reg_signal = EDC_CG_SIG_NOTSUPPORTED;
	rc = lpfc_issue_els_rdf(vport, 0);
	return rc;
}

 
void
lpfc_cancel_retry_delay_tmo(struct lpfc_vport *vport, struct lpfc_nodelist *nlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_work_evt *evtp;

	if (!(nlp->nlp_flag & NLP_DELAY_TMO))
		return;
	spin_lock_irq(&nlp->lock);
	nlp->nlp_flag &= ~NLP_DELAY_TMO;
	spin_unlock_irq(&nlp->lock);
	del_timer_sync(&nlp->nlp_delayfunc);
	nlp->nlp_last_elscmd = 0;
	if (!list_empty(&nlp->els_retry_evt.evt_listp)) {
		list_del_init(&nlp->els_retry_evt.evt_listp);
		 
		evtp = &nlp->els_retry_evt;
		lpfc_nlp_put((struct lpfc_nodelist *)evtp->evt_arg1);
	}
	if (nlp->nlp_flag & NLP_NPR_2B_DISC) {
		spin_lock_irq(&nlp->lock);
		nlp->nlp_flag &= ~NLP_NPR_2B_DISC;
		spin_unlock_irq(&nlp->lock);
		if (vport->num_disc_nodes) {
			if (vport->port_state < LPFC_VPORT_READY) {
				 
				lpfc_more_adisc(vport);
			} else {
				 
				lpfc_more_plogi(vport);
				if (vport->num_disc_nodes == 0) {
					spin_lock_irq(shost->host_lock);
					vport->fc_flag &= ~FC_NDISC_ACTIVE;
					spin_unlock_irq(shost->host_lock);
					lpfc_can_disctmo(vport);
					lpfc_end_rscn(vport);
				}
			}
		}
	}
	return;
}

 
void
lpfc_els_retry_delay(struct timer_list *t)
{
	struct lpfc_nodelist *ndlp = from_timer(ndlp, t, nlp_delayfunc);
	struct lpfc_vport *vport = ndlp->vport;
	struct lpfc_hba   *phba = vport->phba;
	unsigned long flags;
	struct lpfc_work_evt  *evtp = &ndlp->els_retry_evt;

	spin_lock_irqsave(&phba->hbalock, flags);
	if (!list_empty(&evtp->evt_listp)) {
		spin_unlock_irqrestore(&phba->hbalock, flags);
		return;
	}

	 
	evtp->evt_arg1  = lpfc_nlp_get(ndlp);
	if (evtp->evt_arg1) {
		evtp->evt = LPFC_EVT_ELS_RETRY;
		list_add_tail(&evtp->evt_listp, &phba->work_list);
		lpfc_worker_wake_up(phba);
	}
	spin_unlock_irqrestore(&phba->hbalock, flags);
	return;
}

 
void
lpfc_els_retry_delay_handler(struct lpfc_nodelist *ndlp)
{
	struct lpfc_vport *vport = ndlp->vport;
	uint32_t cmd, retry;

	spin_lock_irq(&ndlp->lock);
	cmd = ndlp->nlp_last_elscmd;
	ndlp->nlp_last_elscmd = 0;

	if (!(ndlp->nlp_flag & NLP_DELAY_TMO)) {
		spin_unlock_irq(&ndlp->lock);
		return;
	}

	ndlp->nlp_flag &= ~NLP_DELAY_TMO;
	spin_unlock_irq(&ndlp->lock);
	 
	del_timer_sync(&ndlp->nlp_delayfunc);
	retry = ndlp->nlp_retry;
	ndlp->nlp_retry = 0;

	switch (cmd) {
	case ELS_CMD_FLOGI:
		lpfc_issue_els_flogi(vport, ndlp, retry);
		break;
	case ELS_CMD_PLOGI:
		if (!lpfc_issue_els_plogi(vport, ndlp->nlp_DID, retry)) {
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_PLOGI_ISSUE);
		}
		break;
	case ELS_CMD_ADISC:
		if (!lpfc_issue_els_adisc(vport, ndlp, retry)) {
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_ADISC_ISSUE);
		}
		break;
	case ELS_CMD_PRLI:
	case ELS_CMD_NVMEPRLI:
		if (!lpfc_issue_els_prli(vport, ndlp, retry)) {
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_PRLI_ISSUE);
		}
		break;
	case ELS_CMD_LOGO:
		if (!lpfc_issue_els_logo(vport, ndlp, retry)) {
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_LOGO_ISSUE);
		}
		break;
	case ELS_CMD_FDISC:
		if (!(vport->fc_flag & FC_VPORT_NEEDS_INIT_VPI))
			lpfc_issue_els_fdisc(vport, ndlp, retry);
		break;
	}
	return;
}

 
int
lpfc_link_reset(struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;
	LPFC_MBOXQ_t *mbox;
	uint32_t control;
	int rc;

	lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
			 "2851 Attempt link reset\n");
	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2852 Failed to allocate mbox memory");
		return 1;
	}

	 
	if (phba->sli_rev <= LPFC_SLI_REV3) {
		spin_lock_irq(&phba->hbalock);
		phba->sli.sli_flag |= LPFC_PROCESS_LA;
		control = readl(phba->HCregaddr);
		control |= HC_LAINT_ENA;
		writel(control, phba->HCregaddr);
		readl(phba->HCregaddr);  
		spin_unlock_irq(&phba->hbalock);
	}

	lpfc_init_link(phba, mbox, phba->cfg_topology,
		       phba->cfg_link_speed);
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	mbox->vport = vport;
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
	if ((rc != MBX_BUSY) && (rc != MBX_SUCCESS)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2853 Failed to issue INIT_LINK "
				"mbox command, rc:x%x\n", rc);
		mempool_free(mbox, phba->mbox_mem_pool);
		return 1;
	}

	return 0;
}

 
static int
lpfc_els_retry(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
	       struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	union lpfc_wqe128 *irsp = &rspiocb->wqe;
	struct lpfc_nodelist *ndlp = cmdiocb->ndlp;
	struct lpfc_dmabuf *pcmd = cmdiocb->cmd_dmabuf;
	uint32_t *elscmd;
	struct ls_rjt stat;
	int retry = 0, maxretry = lpfc_max_els_tries, delay = 0;
	int logerr = 0;
	uint32_t cmd = 0;
	uint32_t did;
	int link_reset = 0, rc;
	u32 ulp_status = get_job_ulpstatus(phba, rspiocb);
	u32 ulp_word4 = get_job_word4(phba, rspiocb);


	 

	if (pcmd && pcmd->virt) {
		elscmd = (uint32_t *) (pcmd->virt);
		cmd = *elscmd++;
	}

	if (ndlp)
		did = ndlp->nlp_DID;
	else {
		 
		did = get_job_els_rsp64_did(phba, rspiocb);
		ndlp = lpfc_findnode_did(vport, did);
		if (!ndlp && (cmd != ELS_CMD_PLOGI))
			return 0;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Retry ELS:       wd7:x%x wd4:x%x did:x%x",
		*(((uint32_t *)irsp) + 7), ulp_word4, did);

	switch (ulp_status) {
	case IOSTAT_FCP_RSP_ERROR:
		break;
	case IOSTAT_REMOTE_STOP:
		if (phba->sli_rev == LPFC_SLI_REV4) {
			 
			lpfc_set_rrq_active(phba, ndlp,
					 cmdiocb->sli4_lxritag, 0, 0);
		}
		break;
	case IOSTAT_LOCAL_REJECT:
		switch ((ulp_word4 & IOERR_PARAM_MASK)) {
		case IOERR_LOOP_OPEN_FAILURE:
			if (cmd == ELS_CMD_PLOGI && cmdiocb->retry == 0)
				delay = 1000;
			retry = 1;
			break;

		case IOERR_ILLEGAL_COMMAND:
			lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
					 "0124 Retry illegal cmd x%x "
					 "retry:x%x delay:x%x\n",
					 cmd, cmdiocb->retry, delay);
			retry = 1;
			 
			maxretry = 8;
			if (cmdiocb->retry > 2)
				delay = 1000;
			break;

		case IOERR_NO_RESOURCES:
			logerr = 1;  
			retry = 1;
			if (cmdiocb->retry > 100)
				delay = 100;
			maxretry = 250;
			break;

		case IOERR_ILLEGAL_FRAME:
			delay = 100;
			retry = 1;
			break;

		case IOERR_INVALID_RPI:
			if (cmd == ELS_CMD_PLOGI &&
			    did == NameServer_DID) {
				 
				 
				maxretry = 0;
				delay = 100;
			} else if (cmd == ELS_CMD_PRLI &&
				   ndlp->nlp_state != NLP_STE_PRLI_ISSUE) {
				 
				maxretry = 0;
				retry = 0;
				break;
			}
			retry = 1;
			break;

		case IOERR_SEQUENCE_TIMEOUT:
			if (cmd == ELS_CMD_PLOGI &&
			    did == NameServer_DID &&
			    (cmdiocb->retry + 1) == maxretry) {
				 
				link_reset = 1;
				break;
			}
			retry = 1;
			delay = 100;
			break;
		case IOERR_SLI_ABORTED:
			 
			if (cmd == ELS_CMD_PLOGI) {
				 
				if (ndlp &&
				    ndlp->nlp_state != NLP_STE_PLOGI_ISSUE)
					goto out_retry;
				retry = 1;
				maxretry = 2;
			}
			break;
		}
		break;

	case IOSTAT_NPORT_RJT:
	case IOSTAT_FABRIC_RJT:
		if (ulp_word4 & RJT_UNAVAIL_TEMP) {
			retry = 1;
			break;
		}
		break;

	case IOSTAT_NPORT_BSY:
	case IOSTAT_FABRIC_BSY:
		logerr = 1;  
		retry = 1;
		break;

	case IOSTAT_LS_RJT:
		stat.un.ls_rjt_error_be = cpu_to_be32(ulp_word4);
		 
		if ((vport->fc_flag & FC_PT2PT) &&
		    cmd == ELS_CMD_NVMEPRLI) {
			switch (stat.un.b.lsRjtRsnCode) {
			case LSRJT_UNABLE_TPC:
			case LSRJT_INVALID_CMD:
			case LSRJT_LOGICAL_ERR:
			case LSRJT_CMD_UNSUPPORTED:
				lpfc_printf_vlog(vport, KERN_WARNING, LOG_ELS,
						 "0168 NVME PRLI LS_RJT "
						 "reason %x port doesn't "
						 "support NVME, disabling NVME\n",
						 stat.un.b.lsRjtRsnCode);
				retry = 0;
				vport->fc_flag |= FC_PT2PT_NO_NVME;
				goto out_retry;
			}
		}
		switch (stat.un.b.lsRjtRsnCode) {
		case LSRJT_UNABLE_TPC:
			 
			lpfc_printf_vlog(vport, KERN_INFO,
					 LOG_DISCOVERY | LOG_ELS | LOG_NODE,
					 "0153 ELS cmd x%x LS_RJT by x%x. "
					 "RsnCode x%x RsnCodeExp x%x\n",
					 cmd, did, stat.un.b.lsRjtRsnCode,
					 stat.un.b.lsRjtRsnCodeExp);

			switch (stat.un.b.lsRjtRsnCodeExp) {
			case LSEXP_CANT_GIVE_DATA:
			case LSEXP_CMD_IN_PROGRESS:
				if (cmd == ELS_CMD_PLOGI) {
					delay = 1000;
					maxretry = 48;
				}
				retry = 1;
				break;
			case LSEXP_REQ_UNSUPPORTED:
			case LSEXP_NO_RSRC_ASSIGN:
				 
				if (cmd == ELS_CMD_PRLI ||
				    cmd == ELS_CMD_NVMEPRLI)
					break;
				fallthrough;
			default:
				 
				if (cmd == ELS_CMD_PLOGI ||
				    cmd == ELS_CMD_PRLI ||
				    cmd == ELS_CMD_NVMEPRLI) {
					delay = 1000;
					maxretry = lpfc_max_els_tries + 1;
					retry = 1;
				}
				break;
			}

			if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
			  (cmd == ELS_CMD_FDISC) &&
			  (stat.un.b.lsRjtRsnCodeExp == LSEXP_OUT_OF_RESOURCE)){
				lpfc_printf_vlog(vport, KERN_ERR,
						 LOG_TRACE_EVENT,
						 "0125 FDISC Failed (x%x). "
						 "Fabric out of resources\n",
						 stat.un.lsRjtError);
				lpfc_vport_set_state(vport,
						     FC_VPORT_NO_FABRIC_RSCS);
			}
			break;

		case LSRJT_LOGICAL_BSY:
			if ((cmd == ELS_CMD_PLOGI) ||
			    (cmd == ELS_CMD_PRLI) ||
			    (cmd == ELS_CMD_NVMEPRLI)) {
				delay = 1000;
				maxretry = 48;
			} else if (cmd == ELS_CMD_FDISC) {
				 
				maxretry = 48;
				if (cmdiocb->retry >= 32)
					delay = 1000;
			}
			retry = 1;
			break;

		case LSRJT_LOGICAL_ERR:
			 
			if (cmd == ELS_CMD_FDISC &&
			    stat.un.b.lsRjtRsnCodeExp == LSEXP_PORT_LOGIN_REQ) {
				maxretry = 3;
				delay = 1000;
				retry = 1;
			} else if (cmd == ELS_CMD_FLOGI &&
				   stat.un.b.lsRjtRsnCodeExp ==
						LSEXP_NOTHING_MORE) {
				vport->fc_sparam.cmn.bbRcvSizeMsb &= 0xf;
				retry = 1;
				lpfc_printf_vlog(vport, KERN_ERR,
						 LOG_TRACE_EVENT,
						 "0820 FLOGI Failed (x%x). "
						 "BBCredit Not Supported\n",
						 stat.un.lsRjtError);
			}
			break;

		case LSRJT_PROTOCOL_ERR:
			if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
			  (cmd == ELS_CMD_FDISC) &&
			  ((stat.un.b.lsRjtRsnCodeExp == LSEXP_INVALID_PNAME) ||
			  (stat.un.b.lsRjtRsnCodeExp == LSEXP_INVALID_NPORT_ID))
			  ) {
				lpfc_printf_vlog(vport, KERN_ERR,
						 LOG_TRACE_EVENT,
						 "0122 FDISC Failed (x%x). "
						 "Fabric Detected Bad WWN\n",
						 stat.un.lsRjtError);
				lpfc_vport_set_state(vport,
						     FC_VPORT_FABRIC_REJ_WWN);
			}
			break;
		case LSRJT_VENDOR_UNIQUE:
			if ((stat.un.b.vendorUnique == 0x45) &&
			    (cmd == ELS_CMD_FLOGI)) {
				goto out_retry;
			}
			break;
		case LSRJT_CMD_UNSUPPORTED:
			 
			if (stat.un.b.lsRjtRsnCodeExp ==
			    LSEXP_REQ_UNSUPPORTED) {
				if (cmd == ELS_CMD_PRLI)
					goto out_retry;
			}
			break;
		}
		break;

	case IOSTAT_INTERMED_RSP:
	case IOSTAT_BA_RJT:
		break;

	default:
		break;
	}

	if (link_reset) {
		rc = lpfc_link_reset(vport);
		if (rc) {
			 
			retry = 1;
			delay = 100;
			goto out_retry;
		}
		return 1;
	}

	if (did == FDMI_DID)
		retry = 1;

	if ((cmd == ELS_CMD_FLOGI) &&
	    (phba->fc_topology != LPFC_TOPOLOGY_LOOP) &&
	    !lpfc_error_lost_link(vport, ulp_status, ulp_word4)) {
		 
		retry = 1;
		 
		if (phba->link_flag != LS_LOOPBACK_MODE)
			maxretry = 0;
		else
			maxretry = 2;

		if (cmdiocb->retry >= 100)
			delay = 5000;
		else if (cmdiocb->retry >= 32)
			delay = 1000;
	} else if ((cmd == ELS_CMD_FDISC) &&
	    !lpfc_error_lost_link(vport, ulp_status, ulp_word4)) {
		 
		retry = 1;
		maxretry = vport->cfg_devloss_tmo;
		delay = 1000;
	}

	cmdiocb->retry++;
	if (maxretry && (cmdiocb->retry >= maxretry)) {
		phba->fc_stat.elsRetryExceeded++;
		retry = 0;
	}

	if ((vport->load_flag & FC_UNLOADING) != 0)
		retry = 0;

out_retry:
	if (retry) {
		if ((cmd == ELS_CMD_PLOGI) || (cmd == ELS_CMD_FDISC)) {
			 
			if (phba->fcf.fcf_flag & FCF_DISCOVERY) {
				lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
						 "2849 Stop retry ELS command "
						 "x%x to remote NPORT x%x, "
						 "Data: x%x x%x\n", cmd, did,
						 cmdiocb->retry, delay);
				return 0;
			}
		}

		 
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
				 "0107 Retry ELS command x%x to remote "
				 "NPORT x%x Data: x%x x%x\n",
				 cmd, did, cmdiocb->retry, delay);

		if (((cmd == ELS_CMD_PLOGI) || (cmd == ELS_CMD_ADISC)) &&
			((ulp_status != IOSTAT_LOCAL_REJECT) ||
			((ulp_word4 & IOERR_PARAM_MASK) !=
			IOERR_NO_RESOURCES))) {
			 

			 
			if (timer_pending(&vport->fc_disctmo) ||
			    (vport->fc_flag & FC_RSCN_MODE))
				lpfc_set_disctmo(vport);
		}

		phba->fc_stat.elsXmitRetry++;
		if (ndlp && delay) {
			phba->fc_stat.elsDelayRetry++;
			ndlp->nlp_retry = cmdiocb->retry;

			 
			mod_timer(&ndlp->nlp_delayfunc,
				jiffies + msecs_to_jiffies(delay));
			spin_lock_irq(&ndlp->lock);
			ndlp->nlp_flag |= NLP_DELAY_TMO;
			spin_unlock_irq(&ndlp->lock);

			ndlp->nlp_prev_state = ndlp->nlp_state;
			if ((cmd == ELS_CMD_PRLI) ||
			    (cmd == ELS_CMD_NVMEPRLI))
				lpfc_nlp_set_state(vport, ndlp,
					NLP_STE_PRLI_ISSUE);
			else if (cmd != ELS_CMD_ADISC)
				lpfc_nlp_set_state(vport, ndlp,
					NLP_STE_NPR_NODE);
			ndlp->nlp_last_elscmd = cmd;

			return 1;
		}
		switch (cmd) {
		case ELS_CMD_FLOGI:
			lpfc_issue_els_flogi(vport, ndlp, cmdiocb->retry);
			return 1;
		case ELS_CMD_FDISC:
			lpfc_issue_els_fdisc(vport, ndlp, cmdiocb->retry);
			return 1;
		case ELS_CMD_PLOGI:
			if (ndlp) {
				ndlp->nlp_prev_state = ndlp->nlp_state;
				lpfc_nlp_set_state(vport, ndlp,
						   NLP_STE_PLOGI_ISSUE);
			}
			lpfc_issue_els_plogi(vport, did, cmdiocb->retry);
			return 1;
		case ELS_CMD_ADISC:
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_ADISC_ISSUE);
			lpfc_issue_els_adisc(vport, ndlp, cmdiocb->retry);
			return 1;
		case ELS_CMD_PRLI:
		case ELS_CMD_NVMEPRLI:
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_PRLI_ISSUE);
			lpfc_issue_els_prli(vport, ndlp, cmdiocb->retry);
			return 1;
		case ELS_CMD_LOGO:
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_LOGO_ISSUE);
			lpfc_issue_els_logo(vport, ndlp, cmdiocb->retry);
			return 1;
		}
	}
	 
	if (logerr) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
			 "0137 No retry ELS command x%x to remote "
			 "NPORT x%x: Out of Resources: Error:x%x/%x\n",
			 cmd, did, ulp_status,
			 ulp_word4);
	}
	else {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0108 No retry ELS command x%x to remote "
			 "NPORT x%x Retried:%d Error:x%x/%x\n",
			 cmd, did, cmdiocb->retry, ulp_status,
			 ulp_word4);
	}
	return 0;
}

 
static int
lpfc_els_free_data(struct lpfc_hba *phba, struct lpfc_dmabuf *buf_ptr1)
{
	struct lpfc_dmabuf *buf_ptr;

	 
	if (!list_empty(&buf_ptr1->list)) {
		list_remove_head(&buf_ptr1->list, buf_ptr,
				 struct lpfc_dmabuf,
				 list);
		lpfc_mbuf_free(phba, buf_ptr->virt, buf_ptr->phys);
		kfree(buf_ptr);
	}
	lpfc_mbuf_free(phba, buf_ptr1->virt, buf_ptr1->phys);
	kfree(buf_ptr1);
	return 0;
}

 
static int
lpfc_els_free_bpl(struct lpfc_hba *phba, struct lpfc_dmabuf *buf_ptr)
{
	lpfc_mbuf_free(phba, buf_ptr->virt, buf_ptr->phys);
	kfree(buf_ptr);
	return 0;
}

 
int
lpfc_els_free_iocb(struct lpfc_hba *phba, struct lpfc_iocbq *elsiocb)
{
	struct lpfc_dmabuf *buf_ptr, *buf_ptr1;

	 
	elsiocb->ndlp = NULL;

	 
	if (elsiocb->cmd_dmabuf) {
		if (elsiocb->cmd_flag & LPFC_DELAY_MEM_FREE) {
			 
			elsiocb->cmd_flag &= ~LPFC_DELAY_MEM_FREE;
			buf_ptr = elsiocb->cmd_dmabuf;
			elsiocb->cmd_dmabuf = NULL;
			if (buf_ptr) {
				buf_ptr1 = NULL;
				spin_lock_irq(&phba->hbalock);
				if (!list_empty(&buf_ptr->list)) {
					list_remove_head(&buf_ptr->list,
						buf_ptr1, struct lpfc_dmabuf,
						list);
					INIT_LIST_HEAD(&buf_ptr1->list);
					list_add_tail(&buf_ptr1->list,
						&phba->elsbuf);
					phba->elsbuf_cnt++;
				}
				INIT_LIST_HEAD(&buf_ptr->list);
				list_add_tail(&buf_ptr->list, &phba->elsbuf);
				phba->elsbuf_cnt++;
				spin_unlock_irq(&phba->hbalock);
			}
		} else {
			buf_ptr1 = elsiocb->cmd_dmabuf;
			lpfc_els_free_data(phba, buf_ptr1);
			elsiocb->cmd_dmabuf = NULL;
		}
	}

	if (elsiocb->bpl_dmabuf) {
		buf_ptr = elsiocb->bpl_dmabuf;
		lpfc_els_free_bpl(phba, buf_ptr);
		elsiocb->bpl_dmabuf = NULL;
	}
	lpfc_sli_release_iocbq(phba, elsiocb);
	return 0;
}

 
static void
lpfc_cmpl_els_logo_acc(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		       struct lpfc_iocbq *rspiocb)
{
	struct lpfc_nodelist *ndlp = cmdiocb->ndlp;
	struct lpfc_vport *vport = cmdiocb->vport;
	u32 ulp_status, ulp_word4;

	ulp_status = get_job_ulpstatus(phba, rspiocb);
	ulp_word4 = get_job_word4(phba, rspiocb);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"ACC LOGO cmpl:   status:x%x/x%x did:x%x",
		ulp_status, ulp_word4, ndlp->nlp_DID);
	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0109 ACC to LOGO completes to NPort x%x refcnt %d "
			 "Data: x%x x%x x%x\n",
			 ndlp->nlp_DID, kref_read(&ndlp->kref), ndlp->nlp_flag,
			 ndlp->nlp_state, ndlp->nlp_rpi);

	 
	if (ndlp->nlp_type & NLP_FABRIC &&
	    ((ndlp->nlp_DID & WELL_KNOWN_DID_MASK) != WELL_KNOWN_DID_MASK))
		goto out;

	if (ndlp->nlp_state == NLP_STE_NPR_NODE) {
		 
		if ((ndlp->nlp_flag & NLP_DELAY_TMO) &&
		    (ndlp->nlp_last_elscmd == ELS_CMD_PLOGI))
			goto out;

		if (ndlp->nlp_flag & NLP_RPI_REGISTERED)
			lpfc_unreg_rpi(vport, ndlp);

	}
 out:
	 
	lpfc_els_free_iocb(phba, cmdiocb);
	lpfc_nlp_put(ndlp);
}

 
void
lpfc_mbx_cmpl_dflt_rpi(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_nodelist *ndlp = pmb->ctx_ndlp;
	u32 mbx_flag = pmb->mbox_flag;
	u32 mbx_cmd = pmb->u.mb.mbxCommand;

	if (ndlp) {
		lpfc_printf_vlog(ndlp->vport, KERN_INFO, LOG_NODE,
				 "0006 rpi x%x DID:%x flg:%x %d x%px "
				 "mbx_cmd x%x mbx_flag x%x x%px\n",
				 ndlp->nlp_rpi, ndlp->nlp_DID, ndlp->nlp_flag,
				 kref_read(&ndlp->kref), ndlp, mbx_cmd,
				 mbx_flag, pmb);

		 
		spin_lock_irq(&ndlp->lock);
		ndlp->nlp_flag &= ~NLP_REG_LOGIN_SEND;
		if (mbx_cmd == MBX_UNREG_LOGIN)
			ndlp->nlp_flag &= ~NLP_UNREG_INP;
		spin_unlock_irq(&ndlp->lock);
		lpfc_nlp_put(ndlp);
		lpfc_drop_node(ndlp->vport, ndlp);
	}

	lpfc_mbox_rsrc_cleanup(phba, pmb, MBOX_THD_UNLOCKED);
}

 
static void
lpfc_cmpl_els_rsp(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_iocbq *rspiocb)
{
	struct lpfc_nodelist *ndlp = cmdiocb->ndlp;
	struct lpfc_vport *vport = ndlp ? ndlp->vport : NULL;
	struct Scsi_Host  *shost = vport ? lpfc_shost_from_vport(vport) : NULL;
	IOCB_t  *irsp;
	LPFC_MBOXQ_t *mbox = NULL;
	u32 ulp_status, ulp_word4, tmo, did, iotag;

	if (!vport) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3177 ELS response failed\n");
		goto out;
	}
	if (cmdiocb->context_un.mbox)
		mbox = cmdiocb->context_un.mbox;

	ulp_status = get_job_ulpstatus(phba, rspiocb);
	ulp_word4 = get_job_word4(phba, rspiocb);
	did = get_job_els_rsp64_did(phba, cmdiocb);

	if (phba->sli_rev == LPFC_SLI_REV4) {
		tmo = get_wqe_tmo(cmdiocb);
		iotag = get_wqe_reqtag(cmdiocb);
	} else {
		irsp = &rspiocb->iocb;
		tmo = irsp->ulpTimeout;
		iotag = irsp->ulpIoTag;
	}

	 
	if (!ndlp || lpfc_els_chk_latt(vport)) {
		if (mbox)
			lpfc_mbox_rsrc_cleanup(phba, mbox, MBOX_THD_UNLOCKED);
		goto out;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"ELS rsp cmpl:    status:x%x/x%x did:x%x",
		ulp_status, ulp_word4, did);
	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0110 ELS response tag x%x completes "
			 "Data: x%x x%x x%x x%x x%x x%x x%x x%x %p %p\n",
			 iotag, ulp_status, ulp_word4, tmo,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi, kref_read(&ndlp->kref), mbox, ndlp);
	if (mbox) {
		if (ulp_status == 0
		    && (ndlp->nlp_flag & NLP_ACC_REGLOGIN)) {
			if (!lpfc_unreg_rpi(vport, ndlp) &&
			    (!(vport->fc_flag & FC_PT2PT))) {
				if (ndlp->nlp_state ==  NLP_STE_PLOGI_ISSUE ||
				    ndlp->nlp_state ==
				     NLP_STE_REG_LOGIN_ISSUE) {
					lpfc_printf_vlog(vport, KERN_INFO,
							 LOG_DISCOVERY,
							 "0314 PLOGI recov "
							 "DID x%x "
							 "Data: x%x x%x x%x\n",
							 ndlp->nlp_DID,
							 ndlp->nlp_state,
							 ndlp->nlp_rpi,
							 ndlp->nlp_flag);
					goto out_free_mbox;
				}
			}

			 
			mbox->ctx_ndlp = lpfc_nlp_get(ndlp);
			if (!mbox->ctx_ndlp)
				goto out_free_mbox;

			mbox->vport = vport;
			if (ndlp->nlp_flag & NLP_RM_DFLT_RPI) {
				mbox->mbox_flag |= LPFC_MBX_IMED_UNREG;
				mbox->mbox_cmpl = lpfc_mbx_cmpl_dflt_rpi;
			}
			else {
				mbox->mbox_cmpl = lpfc_mbx_cmpl_reg_login;
				ndlp->nlp_prev_state = ndlp->nlp_state;
				lpfc_nlp_set_state(vport, ndlp,
					   NLP_STE_REG_LOGIN_ISSUE);
			}

			ndlp->nlp_flag |= NLP_REG_LOGIN_SEND;
			if (lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT)
			    != MBX_NOT_FINISHED)
				goto out;

			 
			lpfc_nlp_put(ndlp);
			ndlp->nlp_flag &= ~NLP_REG_LOGIN_SEND;

			 
			lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				"0138 ELS rsp: Cannot issue reg_login for x%x "
				"Data: x%x x%x x%x\n",
				ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
				ndlp->nlp_rpi);
		}
out_free_mbox:
		lpfc_mbox_rsrc_cleanup(phba, mbox, MBOX_THD_UNLOCKED);
	}
out:
	if (ndlp && shost) {
		spin_lock_irq(&ndlp->lock);
		if (mbox)
			ndlp->nlp_flag &= ~NLP_ACC_REGLOGIN;
		ndlp->nlp_flag &= ~NLP_RM_DFLT_RPI;
		spin_unlock_irq(&ndlp->lock);
	}

	 
	if (phba->sli_rev == LPFC_SLI_REV4 &&
	    vport && vport->port_type == LPFC_NPIV_PORT &&
	    !(ndlp->fc4_xpt_flags & SCSI_XPT_REGD)) {
		if (ndlp->nlp_flag & NLP_RELEASE_RPI) {
			if (ndlp->nlp_state != NLP_STE_PLOGI_ISSUE &&
			    ndlp->nlp_state != NLP_STE_REG_LOGIN_ISSUE) {
				lpfc_sli4_free_rpi(phba, ndlp->nlp_rpi);
				spin_lock_irq(&ndlp->lock);
				ndlp->nlp_rpi = LPFC_RPI_ALLOC_ERROR;
				ndlp->nlp_flag &= ~NLP_RELEASE_RPI;
				spin_unlock_irq(&ndlp->lock);
			}
			lpfc_drop_node(vport, ndlp);
		} else if (ndlp->nlp_state != NLP_STE_PLOGI_ISSUE &&
			   ndlp->nlp_state != NLP_STE_REG_LOGIN_ISSUE &&
			   ndlp->nlp_state != NLP_STE_PRLI_ISSUE) {
			 
			lpfc_drop_node(vport, ndlp);
		}
	}

	 
	lpfc_els_free_iocb(phba, cmdiocb);
	lpfc_nlp_put(ndlp);
	return;
}

 
int
lpfc_els_rsp_acc(struct lpfc_vport *vport, uint32_t flag,
		 struct lpfc_iocbq *oldiocb, struct lpfc_nodelist *ndlp,
		 LPFC_MBOXQ_t *mbox)
{
	struct lpfc_hba  *phba = vport->phba;
	IOCB_t *icmd;
	IOCB_t *oldcmd;
	union lpfc_wqe128 *wqe;
	union lpfc_wqe128 *oldwqe = &oldiocb->wqe;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	struct serv_parm *sp;
	uint16_t cmdsize;
	int rc;
	ELS_PKT *els_pkt_ptr;
	struct fc_els_rdf_resp *rdf_resp;

	switch (flag) {
	case ELS_CMD_ACC:
		cmdsize = sizeof(uint32_t);
		elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry,
					     ndlp, ndlp->nlp_DID, ELS_CMD_ACC);
		if (!elsiocb) {
			spin_lock_irq(&ndlp->lock);
			ndlp->nlp_flag &= ~NLP_LOGO_ACC;
			spin_unlock_irq(&ndlp->lock);
			return 1;
		}

		if (phba->sli_rev == LPFC_SLI_REV4) {
			wqe = &elsiocb->wqe;
			 
			bf_set(wqe_ctxt_tag, &wqe->xmit_els_rsp.wqe_com,
			       bf_get(wqe_ctxt_tag,
				      &oldwqe->xmit_els_rsp.wqe_com));

			 
			bf_set(wqe_rcvoxid, &wqe->xmit_els_rsp.wqe_com,
			       bf_get(wqe_rcvoxid,
				      &oldwqe->xmit_els_rsp.wqe_com));
		} else {
			icmd = &elsiocb->iocb;
			oldcmd = &oldiocb->iocb;
			icmd->ulpContext = oldcmd->ulpContext;  
			icmd->unsli3.rcvsli3.ox_id =
				oldcmd->unsli3.rcvsli3.ox_id;
		}

		pcmd = elsiocb->cmd_dmabuf->virt;
		*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
		pcmd += sizeof(uint32_t);

		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
			"Issue ACC:       did:x%x flg:x%x",
			ndlp->nlp_DID, ndlp->nlp_flag, 0);
		break;
	case ELS_CMD_FLOGI:
	case ELS_CMD_PLOGI:
		cmdsize = (sizeof(struct serv_parm) + sizeof(uint32_t));
		elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry,
					     ndlp, ndlp->nlp_DID, ELS_CMD_ACC);
		if (!elsiocb)
			return 1;

		if (phba->sli_rev == LPFC_SLI_REV4) {
			wqe = &elsiocb->wqe;
			 
			bf_set(wqe_ctxt_tag, &wqe->xmit_els_rsp.wqe_com,
			       bf_get(wqe_ctxt_tag,
				      &oldwqe->xmit_els_rsp.wqe_com));

			 
			bf_set(wqe_rcvoxid, &wqe->xmit_els_rsp.wqe_com,
			       bf_get(wqe_rcvoxid,
				      &oldwqe->xmit_els_rsp.wqe_com));
		} else {
			icmd = &elsiocb->iocb;
			oldcmd = &oldiocb->iocb;
			icmd->ulpContext = oldcmd->ulpContext;  
			icmd->unsli3.rcvsli3.ox_id =
				oldcmd->unsli3.rcvsli3.ox_id;
		}

		pcmd = (u8 *)elsiocb->cmd_dmabuf->virt;

		if (mbox)
			elsiocb->context_un.mbox = mbox;

		*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
		pcmd += sizeof(uint32_t);
		sp = (struct serv_parm *)pcmd;

		if (flag == ELS_CMD_FLOGI) {
			 
			memcpy(sp, &phba->fc_fabparam,
			       sizeof(struct serv_parm));

			 
			sp->cmn.fPort = 0;

			 
			sp->cls1.classValid = 0;
			sp->cls2.classValid = 0;
			sp->cls3.classValid = 0;
			sp->cls4.classValid = 0;

			 
			memcpy(&sp->portName, &vport->fc_sparam.portName,
			       sizeof(struct lpfc_name));
			memcpy(&sp->nodeName, &vport->fc_sparam.nodeName,
			       sizeof(struct lpfc_name));
		} else {
			memcpy(pcmd, &vport->fc_sparam,
			       sizeof(struct serv_parm));

			sp->cmn.valid_vendor_ver_level = 0;
			memset(sp->un.vendorVersion, 0,
			       sizeof(sp->un.vendorVersion));
			sp->cmn.bbRcvSizeMsb &= 0xF;

			 
			if (phba->sli.sli_flag & LPFC_SLI_SUPPRESS_RSP) {
				sp->cmn.valid_vendor_ver_level = 1;
				sp->un.vv.vid = cpu_to_be32(LPFC_VV_EMLX_ID);
				sp->un.vv.flags =
					cpu_to_be32(LPFC_VV_SUPPRESS_RSP);
			}
		}

		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
			"Issue ACC FLOGI/PLOGI: did:x%x flg:x%x",
			ndlp->nlp_DID, ndlp->nlp_flag, 0);
		break;
	case ELS_CMD_PRLO:
		cmdsize = sizeof(uint32_t) + sizeof(PRLO);
		elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry,
					     ndlp, ndlp->nlp_DID, ELS_CMD_PRLO);
		if (!elsiocb)
			return 1;

		if (phba->sli_rev == LPFC_SLI_REV4) {
			wqe = &elsiocb->wqe;
			 
			bf_set(wqe_ctxt_tag, &wqe->xmit_els_rsp.wqe_com,
			       bf_get(wqe_ctxt_tag,
				      &oldwqe->xmit_els_rsp.wqe_com));

			 
			bf_set(wqe_rcvoxid, &wqe->xmit_els_rsp.wqe_com,
			       bf_get(wqe_rcvoxid,
				      &oldwqe->xmit_els_rsp.wqe_com));
		} else {
			icmd = &elsiocb->iocb;
			oldcmd = &oldiocb->iocb;
			icmd->ulpContext = oldcmd->ulpContext;  
			icmd->unsli3.rcvsli3.ox_id =
				oldcmd->unsli3.rcvsli3.ox_id;
		}

		pcmd = (u8 *) elsiocb->cmd_dmabuf->virt;

		memcpy(pcmd, oldiocb->cmd_dmabuf->virt,
		       sizeof(uint32_t) + sizeof(PRLO));
		*((uint32_t *) (pcmd)) = ELS_CMD_PRLO_ACC;
		els_pkt_ptr = (ELS_PKT *) pcmd;
		els_pkt_ptr->un.prlo.acceptRspCode = PRLO_REQ_EXECUTED;

		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
			"Issue ACC PRLO:  did:x%x flg:x%x",
			ndlp->nlp_DID, ndlp->nlp_flag, 0);
		break;
	case ELS_CMD_RDF:
		cmdsize = sizeof(*rdf_resp);
		elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry,
					     ndlp, ndlp->nlp_DID, ELS_CMD_ACC);
		if (!elsiocb)
			return 1;

		if (phba->sli_rev == LPFC_SLI_REV4) {
			wqe = &elsiocb->wqe;
			 
			bf_set(wqe_ctxt_tag, &wqe->xmit_els_rsp.wqe_com,
			       bf_get(wqe_ctxt_tag,
				      &oldwqe->xmit_els_rsp.wqe_com));

			 
			bf_set(wqe_rcvoxid, &wqe->xmit_els_rsp.wqe_com,
			       bf_get(wqe_rcvoxid,
				      &oldwqe->xmit_els_rsp.wqe_com));
		} else {
			icmd = &elsiocb->iocb;
			oldcmd = &oldiocb->iocb;
			icmd->ulpContext = oldcmd->ulpContext;  
			icmd->unsli3.rcvsli3.ox_id =
				oldcmd->unsli3.rcvsli3.ox_id;
		}

		pcmd = (u8 *)elsiocb->cmd_dmabuf->virt;
		rdf_resp = (struct fc_els_rdf_resp *)pcmd;
		memset(rdf_resp, 0, sizeof(*rdf_resp));
		rdf_resp->acc_hdr.la_cmd = ELS_LS_ACC;

		 
		rdf_resp->desc_list_len = cpu_to_be32(12);

		 
		rdf_resp->lsri.desc_tag = cpu_to_be32(1);
		rdf_resp->lsri.desc_len = cpu_to_be32(sizeof(u32));
		rdf_resp->lsri.rqst_w0.cmd = ELS_RDF;
		break;
	default:
		return 1;
	}
	if (ndlp->nlp_flag & NLP_LOGO_ACC) {
		spin_lock_irq(&ndlp->lock);
		if (!(ndlp->nlp_flag & NLP_RPI_REGISTERED ||
			ndlp->nlp_flag & NLP_REG_LOGIN_SEND))
			ndlp->nlp_flag &= ~NLP_LOGO_ACC;
		spin_unlock_irq(&ndlp->lock);
		elsiocb->cmd_cmpl = lpfc_cmpl_els_logo_acc;
	} else {
		elsiocb->cmd_cmpl = lpfc_cmpl_els_rsp;
	}

	phba->fc_stat.elsXmitACC++;
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
		return 1;
	}

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0128 Xmit ELS ACC response Status: x%x, IoTag: x%x, "
			 "XRI: x%x, DID: x%x, nlp_flag: x%x nlp_state: x%x "
			 "RPI: x%x, fc_flag x%x refcnt %d\n",
			 rc, elsiocb->iotag, elsiocb->sli4_xritag,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi, vport->fc_flag, kref_read(&ndlp->kref));
	return 0;
}

 
int
lpfc_els_rsp_reject(struct lpfc_vport *vport, uint32_t rejectError,
		    struct lpfc_iocbq *oldiocb, struct lpfc_nodelist *ndlp,
		    LPFC_MBOXQ_t *mbox)
{
	int rc;
	struct lpfc_hba  *phba = vport->phba;
	IOCB_t *icmd;
	IOCB_t *oldcmd;
	union lpfc_wqe128 *wqe;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;

	cmdsize = 2 * sizeof(uint32_t);
	elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_LS_RJT);
	if (!elsiocb)
		return 1;

	if (phba->sli_rev == LPFC_SLI_REV4) {
		wqe = &elsiocb->wqe;
		bf_set(wqe_ctxt_tag, &wqe->generic.wqe_com,
		       get_job_ulpcontext(phba, oldiocb));  
		bf_set(wqe_rcvoxid, &wqe->xmit_els_rsp.wqe_com,
		       get_job_rcvoxid(phba, oldiocb));
	} else {
		icmd = &elsiocb->iocb;
		oldcmd = &oldiocb->iocb;
		icmd->ulpContext = oldcmd->ulpContext;  
		icmd->unsli3.rcvsli3.ox_id = oldcmd->unsli3.rcvsli3.ox_id;
	}

	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;

	*((uint32_t *) (pcmd)) = ELS_CMD_LS_RJT;
	pcmd += sizeof(uint32_t);
	*((uint32_t *) (pcmd)) = rejectError;

	if (mbox)
		elsiocb->context_un.mbox = mbox;

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0129 Xmit ELS RJT x%x response tag x%x "
			 "xri x%x, did x%x, nlp_flag x%x, nlp_state x%x, "
			 "rpi x%x\n",
			 rejectError, elsiocb->iotag,
			 get_job_ulpcontext(phba, elsiocb), ndlp->nlp_DID,
			 ndlp->nlp_flag, ndlp->nlp_state, ndlp->nlp_rpi);
	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"Issue LS_RJT:    did:x%x flg:x%x err:x%x",
		ndlp->nlp_DID, ndlp->nlp_flag, rejectError);

	phba->fc_stat.elsXmitLSRJT++;
	elsiocb->cmd_cmpl = lpfc_cmpl_els_rsp;
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}

	 
	if (phba->sli_rev == LPFC_SLI_REV4 &&
	    vport->port_type == LPFC_NPIV_PORT &&
	    !(ndlp->fc4_xpt_flags & SCSI_XPT_REGD)) {
		spin_lock_irq(&ndlp->lock);
		ndlp->nlp_flag |= NLP_RELEASE_RPI;
		spin_unlock_irq(&ndlp->lock);
	}

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
		return 1;
	}

	return 0;
}

  
static int
lpfc_issue_els_edc_rsp(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		       struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba  *phba = vport->phba;
	struct fc_els_edc_resp *edc_rsp;
	struct fc_tlv_desc *tlv;
	struct lpfc_iocbq *elsiocb;
	IOCB_t *icmd, *cmd;
	union lpfc_wqe128 *wqe;
	u32 cgn_desc_size, lft_desc_size;
	u16 cmdsize;
	uint8_t *pcmd;
	int rc;

	cmdsize = sizeof(struct fc_els_edc_resp);
	cgn_desc_size = sizeof(struct fc_diag_cg_sig_desc);
	lft_desc_size = (lpfc_link_is_lds_capable(phba)) ?
				sizeof(struct fc_diag_lnkflt_desc) : 0;
	cmdsize += cgn_desc_size + lft_desc_size;
	elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, cmdiocb->retry,
				     ndlp, ndlp->nlp_DID, ELS_CMD_ACC);
	if (!elsiocb)
		return 1;

	if (phba->sli_rev == LPFC_SLI_REV4) {
		wqe = &elsiocb->wqe;
		bf_set(wqe_ctxt_tag, &wqe->generic.wqe_com,
		       get_job_ulpcontext(phba, cmdiocb));  
		bf_set(wqe_rcvoxid, &wqe->xmit_els_rsp.wqe_com,
		       get_job_rcvoxid(phba, cmdiocb));
	} else {
		icmd = &elsiocb->iocb;
		cmd = &cmdiocb->iocb;
		icmd->ulpContext = cmd->ulpContext;  
		icmd->unsli3.rcvsli3.ox_id = cmd->unsli3.rcvsli3.ox_id;
	}

	pcmd = elsiocb->cmd_dmabuf->virt;
	memset(pcmd, 0, cmdsize);

	edc_rsp = (struct fc_els_edc_resp *)pcmd;
	edc_rsp->acc_hdr.la_cmd = ELS_LS_ACC;
	edc_rsp->desc_list_len = cpu_to_be32(sizeof(struct fc_els_lsri_desc) +
						cgn_desc_size + lft_desc_size);
	edc_rsp->lsri.desc_tag = cpu_to_be32(ELS_DTAG_LS_REQ_INFO);
	edc_rsp->lsri.desc_len = cpu_to_be32(
		FC_TLV_DESC_LENGTH_FROM_SZ(struct fc_els_lsri_desc));
	edc_rsp->lsri.rqst_w0.cmd = ELS_EDC;
	tlv = edc_rsp->desc;
	lpfc_format_edc_cgn_desc(phba, tlv);
	tlv = fc_tlv_next_desc(tlv);
	if (lft_desc_size)
		lpfc_format_edc_lft_desc(phba, tlv);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
			      "Issue EDC ACC:      did:x%x flg:x%x refcnt %d",
			      ndlp->nlp_DID, ndlp->nlp_flag,
			      kref_read(&ndlp->kref));
	elsiocb->cmd_cmpl = lpfc_cmpl_els_rsp;

	phba->fc_stat.elsXmitACC++;
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
		return 1;
	}

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0152 Xmit EDC ACC response Status: x%x, IoTag: x%x, "
			 "XRI: x%x, DID: x%x, nlp_flag: x%x nlp_state: x%x "
			 "RPI: x%x, fc_flag x%x\n",
			 rc, elsiocb->iotag, elsiocb->sli4_xritag,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi, vport->fc_flag);

	return 0;
}

 
int
lpfc_els_rsp_adisc_acc(struct lpfc_vport *vport, struct lpfc_iocbq *oldiocb,
		       struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba  *phba = vport->phba;
	ADISC *ap;
	IOCB_t *icmd, *oldcmd;
	union lpfc_wqe128 *wqe;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int rc;
	u32 ulp_context;

	cmdsize = sizeof(uint32_t) + sizeof(ADISC);
	elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ACC);
	if (!elsiocb)
		return 1;

	if (phba->sli_rev == LPFC_SLI_REV4) {
		wqe = &elsiocb->wqe;
		 
		bf_set(wqe_ctxt_tag, &wqe->generic.wqe_com,
		       get_job_ulpcontext(phba, oldiocb));
		ulp_context = get_job_ulpcontext(phba, elsiocb);
		 
		bf_set(wqe_rcvoxid, &wqe->xmit_els_rsp.wqe_com,
		       get_job_rcvoxid(phba, oldiocb));
	} else {
		icmd = &elsiocb->iocb;
		oldcmd = &oldiocb->iocb;
		icmd->ulpContext = oldcmd->ulpContext;  
		ulp_context = elsiocb->iocb.ulpContext;
		icmd->unsli3.rcvsli3.ox_id =
			oldcmd->unsli3.rcvsli3.ox_id;
	}

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0130 Xmit ADISC ACC response iotag x%x xri: "
			 "x%x, did x%x, nlp_flag x%x, nlp_state x%x rpi x%x\n",
			 elsiocb->iotag, ulp_context,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);
	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;

	*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
	pcmd += sizeof(uint32_t);

	ap = (ADISC *) (pcmd);
	ap->hardAL_PA = phba->fc_pref_ALPA;
	memcpy(&ap->portName, &vport->fc_portname, sizeof(struct lpfc_name));
	memcpy(&ap->nodeName, &vport->fc_nodename, sizeof(struct lpfc_name));
	ap->DID = be32_to_cpu(vport->fc_myDID);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		      "Issue ACC ADISC: did:x%x flg:x%x refcnt %d",
		      ndlp->nlp_DID, ndlp->nlp_flag, kref_read(&ndlp->kref));

	phba->fc_stat.elsXmitACC++;
	elsiocb->cmd_cmpl = lpfc_cmpl_els_rsp;
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
		return 1;
	}

	return 0;
}

 
int
lpfc_els_rsp_prli_acc(struct lpfc_vport *vport, struct lpfc_iocbq *oldiocb,
		      struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba  *phba = vport->phba;
	PRLI *npr;
	struct lpfc_nvme_prli *npr_nvme;
	lpfc_vpd_t *vpd;
	IOCB_t *icmd;
	IOCB_t *oldcmd;
	union lpfc_wqe128 *wqe;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;
	uint32_t prli_fc4_req, *req_payload;
	struct lpfc_dmabuf *req_buf;
	int rc;
	u32 elsrspcmd, ulp_context;

	 
	req_buf = oldiocb->cmd_dmabuf;
	req_payload = (((uint32_t *)req_buf->virt) + 1);

	 
	prli_fc4_req = be32_to_cpu(*req_payload);
	prli_fc4_req = (prli_fc4_req >> 24) & 0xff;
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "6127 PRLI_ACC:  Req Type x%x, Word1 x%08x\n",
			 prli_fc4_req, *((uint32_t *)req_payload));

	if (prli_fc4_req == PRLI_FCP_TYPE) {
		cmdsize = sizeof(uint32_t) + sizeof(PRLI);
		elsrspcmd = (ELS_CMD_ACC | (ELS_CMD_PRLI & ~ELS_RSP_MASK));
	} else if (prli_fc4_req == PRLI_NVME_TYPE) {
		cmdsize = sizeof(uint32_t) + sizeof(struct lpfc_nvme_prli);
		elsrspcmd = (ELS_CMD_ACC | (ELS_CMD_NVMEPRLI & ~ELS_RSP_MASK));
	} else {
		return 1;
	}

	elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry, ndlp,
				     ndlp->nlp_DID, elsrspcmd);
	if (!elsiocb)
		return 1;

	if (phba->sli_rev == LPFC_SLI_REV4) {
		wqe = &elsiocb->wqe;
		bf_set(wqe_ctxt_tag, &wqe->generic.wqe_com,
		       get_job_ulpcontext(phba, oldiocb));  
		ulp_context = get_job_ulpcontext(phba, elsiocb);
		bf_set(wqe_rcvoxid, &wqe->xmit_els_rsp.wqe_com,
		       get_job_rcvoxid(phba, oldiocb));
	} else {
		icmd = &elsiocb->iocb;
		oldcmd = &oldiocb->iocb;
		icmd->ulpContext = oldcmd->ulpContext;  
		ulp_context = elsiocb->iocb.ulpContext;
		icmd->unsli3.rcvsli3.ox_id =
			oldcmd->unsli3.rcvsli3.ox_id;
	}

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0131 Xmit PRLI ACC response tag x%x xri x%x, "
			 "did x%x, nlp_flag x%x, nlp_state x%x, rpi x%x\n",
			 elsiocb->iotag, ulp_context,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);
	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;
	memset(pcmd, 0, cmdsize);

	*((uint32_t *)(pcmd)) = elsrspcmd;
	pcmd += sizeof(uint32_t);

	 
	vpd = &phba->vpd;

	if (prli_fc4_req == PRLI_FCP_TYPE) {
		 
		npr = (PRLI *) pcmd;
		if ((ndlp->nlp_type & NLP_FCP_TARGET) &&
		    (vpd->rev.feaLevelHigh >= 0x02)) {
			npr->ConfmComplAllowed = 1;
			npr->Retry = 1;
			npr->TaskRetryIdReq = 1;
		}
		npr->acceptRspCode = PRLI_REQ_EXECUTED;

		 
		if (ndlp->nlp_type & NLP_FCP_TARGET)
			npr->estabImagePair = 1;
		else
			npr->estabImagePair = 0;
		npr->readXferRdyDis = 1;
		npr->ConfmComplAllowed = 1;
		npr->prliType = PRLI_FCP_TYPE;
		npr->initiatorFunc = 1;

		 
		lpfc_printf_vlog(vport, KERN_INFO,
				 LOG_ELS | LOG_NODE | LOG_DISCOVERY,
				 "6014 FCP issue PRLI ACC imgpair %d "
				 "retry %d task %d\n",
				 npr->estabImagePair,
				 npr->Retry, npr->TaskRetryIdReq);

	} else if (prli_fc4_req == PRLI_NVME_TYPE) {
		 
		npr_nvme = (struct lpfc_nvme_prli *) pcmd;
		bf_set(prli_type_code, npr_nvme, PRLI_NVME_TYPE);
		bf_set(prli_estabImagePair, npr_nvme, 0);   
		bf_set(prli_acc_rsp_code, npr_nvme, PRLI_REQ_EXECUTED);
		if (phba->nvmet_support) {
			bf_set(prli_tgt, npr_nvme, 1);
			bf_set(prli_disc, npr_nvme, 1);
			if (phba->cfg_nvme_enable_fb) {
				bf_set(prli_fba, npr_nvme, 1);

				 
				bf_set(prli_fb_sz, npr_nvme,
				       phba->cfg_nvmet_fb_size);
			}
		} else {
			bf_set(prli_init, npr_nvme, 1);
		}

		lpfc_printf_vlog(vport, KERN_INFO, LOG_NVME_DISC,
				 "6015 NVME issue PRLI ACC word1 x%08x "
				 "word4 x%08x word5 x%08x flag x%x, "
				 "fcp_info x%x nlp_type x%x\n",
				 npr_nvme->word1, npr_nvme->word4,
				 npr_nvme->word5, ndlp->nlp_flag,
				 ndlp->nlp_fcp_info, ndlp->nlp_type);
		npr_nvme->word1 = cpu_to_be32(npr_nvme->word1);
		npr_nvme->word4 = cpu_to_be32(npr_nvme->word4);
		npr_nvme->word5 = cpu_to_be32(npr_nvme->word5);
	} else
		lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
				 "6128 Unknown FC_TYPE x%x x%x ndlp x%06x\n",
				 prli_fc4_req, ndlp->nlp_fc4_type,
				 ndlp->nlp_DID);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		      "Issue ACC PRLI:  did:x%x flg:x%x",
		      ndlp->nlp_DID, ndlp->nlp_flag, kref_read(&ndlp->kref));

	phba->fc_stat.elsXmitACC++;
	elsiocb->cmd_cmpl = lpfc_cmpl_els_rsp;
	elsiocb->ndlp =  lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
		return 1;
	}

	return 0;
}

 
static int
lpfc_els_rsp_rnid_acc(struct lpfc_vport *vport, uint8_t format,
		      struct lpfc_iocbq *oldiocb, struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba  *phba = vport->phba;
	RNID *rn;
	IOCB_t *icmd, *oldcmd;
	union lpfc_wqe128 *wqe;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int rc;
	u32 ulp_context;

	cmdsize = sizeof(uint32_t) + sizeof(uint32_t)
					+ (2 * sizeof(struct lpfc_name));
	if (format)
		cmdsize += sizeof(RNID_TOP_DISC);

	elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ACC);
	if (!elsiocb)
		return 1;

	if (phba->sli_rev == LPFC_SLI_REV4) {
		wqe = &elsiocb->wqe;
		bf_set(wqe_ctxt_tag, &wqe->generic.wqe_com,
		       get_job_ulpcontext(phba, oldiocb));  
		ulp_context = get_job_ulpcontext(phba, elsiocb);
		bf_set(wqe_rcvoxid, &wqe->xmit_els_rsp.wqe_com,
		       get_job_rcvoxid(phba, oldiocb));
	} else {
		icmd = &elsiocb->iocb;
		oldcmd = &oldiocb->iocb;
		icmd->ulpContext = oldcmd->ulpContext;  
		ulp_context = elsiocb->iocb.ulpContext;
		icmd->unsli3.rcvsli3.ox_id =
			oldcmd->unsli3.rcvsli3.ox_id;
	}

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0132 Xmit RNID ACC response tag x%x xri x%x\n",
			 elsiocb->iotag, ulp_context);
	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;
	*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
	pcmd += sizeof(uint32_t);

	memset(pcmd, 0, sizeof(RNID));
	rn = (RNID *) (pcmd);
	rn->Format = format;
	rn->CommonLen = (2 * sizeof(struct lpfc_name));
	memcpy(&rn->portName, &vport->fc_portname, sizeof(struct lpfc_name));
	memcpy(&rn->nodeName, &vport->fc_nodename, sizeof(struct lpfc_name));
	switch (format) {
	case 0:
		rn->SpecificLen = 0;
		break;
	case RNID_TOPOLOGY_DISC:
		rn->SpecificLen = sizeof(RNID_TOP_DISC);
		memcpy(&rn->un.topologyDisc.portName,
		       &vport->fc_portname, sizeof(struct lpfc_name));
		rn->un.topologyDisc.unitType = RNID_HBA;
		rn->un.topologyDisc.physPort = 0;
		rn->un.topologyDisc.attachedNodes = 0;
		break;
	default:
		rn->CommonLen = 0;
		rn->SpecificLen = 0;
		break;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		      "Issue ACC RNID:  did:x%x flg:x%x refcnt %d",
		      ndlp->nlp_DID, ndlp->nlp_flag, kref_read(&ndlp->kref));

	phba->fc_stat.elsXmitACC++;
	elsiocb->cmd_cmpl = lpfc_cmpl_els_rsp;
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
		return 1;
	}

	return 0;
}

 
static void
lpfc_els_clear_rrq(struct lpfc_vport *vport,
		   struct lpfc_iocbq *iocb, struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba  *phba = vport->phba;
	uint8_t *pcmd;
	struct RRQ *rrq;
	uint16_t rxid;
	uint16_t xri;
	struct lpfc_node_rrq *prrq;


	pcmd = (uint8_t *)iocb->cmd_dmabuf->virt;
	pcmd += sizeof(uint32_t);
	rrq = (struct RRQ *)pcmd;
	rrq->rrq_exchg = be32_to_cpu(rrq->rrq_exchg);
	rxid = bf_get(rrq_rxid, rrq);

	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			"2883 Clear RRQ for SID:x%x OXID:x%x RXID:x%x"
			" x%x x%x\n",
			be32_to_cpu(bf_get(rrq_did, rrq)),
			bf_get(rrq_oxid, rrq),
			rxid,
			get_wqe_reqtag(iocb),
			get_job_ulpcontext(phba, iocb));

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"Clear RRQ:  did:x%x flg:x%x exchg:x%.08x",
		ndlp->nlp_DID, ndlp->nlp_flag, rrq->rrq_exchg);
	if (vport->fc_myDID == be32_to_cpu(bf_get(rrq_did, rrq)))
		xri = bf_get(rrq_oxid, rrq);
	else
		xri = rxid;
	prrq = lpfc_get_active_rrq(vport, xri, ndlp->nlp_DID);
	if (prrq)
		lpfc_clr_rrq_active(phba, xri, prrq);
	return;
}

 
static int
lpfc_els_rsp_echo_acc(struct lpfc_vport *vport, uint8_t *data,
		      struct lpfc_iocbq *oldiocb, struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba  *phba = vport->phba;
	IOCB_t *icmd, *oldcmd;
	union lpfc_wqe128 *wqe;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int rc;
	u32 ulp_context;

	if (phba->sli_rev == LPFC_SLI_REV4)
		cmdsize = oldiocb->wcqe_cmpl.total_data_placed;
	else
		cmdsize = oldiocb->iocb.unsli3.rcvsli3.acc_len;

	 
	if (cmdsize > LPFC_BPL_SIZE)
		cmdsize = LPFC_BPL_SIZE;
	elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ACC);
	if (!elsiocb)
		return 1;

	if (phba->sli_rev == LPFC_SLI_REV4) {
		wqe = &elsiocb->wqe;
		bf_set(wqe_ctxt_tag, &wqe->generic.wqe_com,
		       get_job_ulpcontext(phba, oldiocb));  
		ulp_context = get_job_ulpcontext(phba, elsiocb);
		bf_set(wqe_rcvoxid, &wqe->xmit_els_rsp.wqe_com,
		       get_job_rcvoxid(phba, oldiocb));
	} else {
		icmd = &elsiocb->iocb;
		oldcmd = &oldiocb->iocb;
		icmd->ulpContext = oldcmd->ulpContext;  
		ulp_context = elsiocb->iocb.ulpContext;
		icmd->unsli3.rcvsli3.ox_id =
			oldcmd->unsli3.rcvsli3.ox_id;
	}

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "2876 Xmit ECHO ACC response tag x%x xri x%x\n",
			 elsiocb->iotag, ulp_context);
	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;
	*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
	pcmd += sizeof(uint32_t);
	memcpy(pcmd, data, cmdsize - sizeof(uint32_t));

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		      "Issue ACC ECHO:  did:x%x flg:x%x refcnt %d",
		      ndlp->nlp_DID, ndlp->nlp_flag, kref_read(&ndlp->kref));

	phba->fc_stat.elsXmitACC++;
	elsiocb->cmd_cmpl = lpfc_cmpl_els_rsp;
	elsiocb->ndlp =  lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
		return 1;
	}

	return 0;
}

 
int
lpfc_els_disc_adisc(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp, *next_ndlp;
	int sentadisc = 0;

	 
	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {

		if (ndlp->nlp_state != NLP_STE_NPR_NODE ||
		    !(ndlp->nlp_flag & NLP_NPR_ADISC))
			continue;

		spin_lock_irq(&ndlp->lock);
		ndlp->nlp_flag &= ~NLP_NPR_ADISC;
		spin_unlock_irq(&ndlp->lock);

		if (!(ndlp->nlp_flag & NLP_NPR_2B_DISC)) {
			 
			lpfc_nlp_unreg_node(vport, ndlp);
			lpfc_unreg_rpi(vport, ndlp);
			continue;
		}

		ndlp->nlp_prev_state = ndlp->nlp_state;
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_ADISC_ISSUE);
		lpfc_issue_els_adisc(vport, ndlp, 0);
		sentadisc++;
		vport->num_disc_nodes++;
		if (vport->num_disc_nodes >=
				vport->cfg_discovery_threads) {
			spin_lock_irq(shost->host_lock);
			vport->fc_flag |= FC_NLP_MORE;
			spin_unlock_irq(shost->host_lock);
			break;
		}

	}
	if (sentadisc == 0) {
		spin_lock_irq(shost->host_lock);
		vport->fc_flag &= ~FC_NLP_MORE;
		spin_unlock_irq(shost->host_lock);
	}
	return sentadisc;
}

 
int
lpfc_els_disc_plogi(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp, *next_ndlp;
	int sentplogi = 0;

	 
	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {
		if (ndlp->nlp_state == NLP_STE_NPR_NODE &&
				(ndlp->nlp_flag & NLP_NPR_2B_DISC) != 0 &&
				(ndlp->nlp_flag & NLP_DELAY_TMO) == 0 &&
				(ndlp->nlp_flag & NLP_NPR_ADISC) == 0) {
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_PLOGI_ISSUE);
			lpfc_issue_els_plogi(vport, ndlp->nlp_DID, 0);
			sentplogi++;
			vport->num_disc_nodes++;
			if (vport->num_disc_nodes >=
					vport->cfg_discovery_threads) {
				spin_lock_irq(shost->host_lock);
				vport->fc_flag |= FC_NLP_MORE;
				spin_unlock_irq(shost->host_lock);
				break;
			}
		}
	}

	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "6452 Discover PLOGI %d flag x%x\n",
			 sentplogi, vport->fc_flag);

	if (sentplogi) {
		lpfc_set_disctmo(vport);
	}
	else {
		spin_lock_irq(shost->host_lock);
		vport->fc_flag &= ~FC_NLP_MORE;
		spin_unlock_irq(shost->host_lock);
	}
	return sentplogi;
}

static uint32_t
lpfc_rdp_res_link_service(struct fc_rdp_link_service_desc *desc,
		uint32_t word0)
{

	desc->tag = cpu_to_be32(RDP_LINK_SERVICE_DESC_TAG);
	desc->payload.els_req = word0;
	desc->length = cpu_to_be32(sizeof(desc->payload));

	return sizeof(struct fc_rdp_link_service_desc);
}

static uint32_t
lpfc_rdp_res_sfp_desc(struct fc_rdp_sfp_desc *desc,
		uint8_t *page_a0, uint8_t *page_a2)
{
	uint16_t wavelength;
	uint16_t temperature;
	uint16_t rx_power;
	uint16_t tx_bias;
	uint16_t tx_power;
	uint16_t vcc;
	uint16_t flag = 0;
	struct sff_trasnceiver_codes_byte4 *trasn_code_byte4;
	struct sff_trasnceiver_codes_byte5 *trasn_code_byte5;

	desc->tag = cpu_to_be32(RDP_SFP_DESC_TAG);

	trasn_code_byte4 = (struct sff_trasnceiver_codes_byte4 *)
			&page_a0[SSF_TRANSCEIVER_CODE_B4];
	trasn_code_byte5 = (struct sff_trasnceiver_codes_byte5 *)
			&page_a0[SSF_TRANSCEIVER_CODE_B5];

	if ((trasn_code_byte4->fc_sw_laser) ||
	    (trasn_code_byte5->fc_sw_laser_sl) ||
	    (trasn_code_byte5->fc_sw_laser_sn)) {   
		flag |= (SFP_FLAG_PT_SWLASER << SFP_FLAG_PT_SHIFT);
	} else if (trasn_code_byte4->fc_lw_laser) {
		wavelength = (page_a0[SSF_WAVELENGTH_B1] << 8) |
			page_a0[SSF_WAVELENGTH_B0];
		if (wavelength == SFP_WAVELENGTH_LC1310)
			flag |= SFP_FLAG_PT_LWLASER_LC1310 << SFP_FLAG_PT_SHIFT;
		if (wavelength == SFP_WAVELENGTH_LL1550)
			flag |= SFP_FLAG_PT_LWLASER_LL1550 << SFP_FLAG_PT_SHIFT;
	}
	 
	flag |= ((page_a0[SSF_IDENTIFIER] == SFF_PG0_IDENT_SFP) ?
			SFP_FLAG_CT_SFP_PLUS : SFP_FLAG_CT_UNKNOWN)
					<< SFP_FLAG_CT_SHIFT;

	 
	flag |= ((page_a0[SSF_CONNECTOR] == SFF_PG0_CONNECTOR_LC) ?
			SFP_FLAG_IS_OPTICAL_PORT : 0)
					<< SFP_FLAG_IS_OPTICAL_SHIFT;

	temperature = (page_a2[SFF_TEMPERATURE_B1] << 8 |
		page_a2[SFF_TEMPERATURE_B0]);
	vcc = (page_a2[SFF_VCC_B1] << 8 |
		page_a2[SFF_VCC_B0]);
	tx_power = (page_a2[SFF_TXPOWER_B1] << 8 |
		page_a2[SFF_TXPOWER_B0]);
	tx_bias = (page_a2[SFF_TX_BIAS_CURRENT_B1] << 8 |
		page_a2[SFF_TX_BIAS_CURRENT_B0]);
	rx_power = (page_a2[SFF_RXPOWER_B1] << 8 |
		page_a2[SFF_RXPOWER_B0]);
	desc->sfp_info.temperature = cpu_to_be16(temperature);
	desc->sfp_info.rx_power = cpu_to_be16(rx_power);
	desc->sfp_info.tx_bias = cpu_to_be16(tx_bias);
	desc->sfp_info.tx_power = cpu_to_be16(tx_power);
	desc->sfp_info.vcc = cpu_to_be16(vcc);

	desc->sfp_info.flags = cpu_to_be16(flag);
	desc->length = cpu_to_be32(sizeof(desc->sfp_info));

	return sizeof(struct fc_rdp_sfp_desc);
}

static uint32_t
lpfc_rdp_res_link_error(struct fc_rdp_link_error_status_desc *desc,
		READ_LNK_VAR *stat)
{
	uint32_t type;

	desc->tag = cpu_to_be32(RDP_LINK_ERROR_STATUS_DESC_TAG);

	type = VN_PT_PHY_PF_PORT << VN_PT_PHY_SHIFT;

	desc->info.port_type = cpu_to_be32(type);

	desc->info.link_status.link_failure_cnt =
		cpu_to_be32(stat->linkFailureCnt);
	desc->info.link_status.loss_of_synch_cnt =
		cpu_to_be32(stat->lossSyncCnt);
	desc->info.link_status.loss_of_signal_cnt =
		cpu_to_be32(stat->lossSignalCnt);
	desc->info.link_status.primitive_seq_proto_err =
		cpu_to_be32(stat->primSeqErrCnt);
	desc->info.link_status.invalid_trans_word =
		cpu_to_be32(stat->invalidXmitWord);
	desc->info.link_status.invalid_crc_cnt = cpu_to_be32(stat->crcCnt);

	desc->length = cpu_to_be32(sizeof(desc->info));

	return sizeof(struct fc_rdp_link_error_status_desc);
}

static uint32_t
lpfc_rdp_res_bbc_desc(struct fc_rdp_bbc_desc *desc, READ_LNK_VAR *stat,
		      struct lpfc_vport *vport)
{
	uint32_t bbCredit;

	desc->tag = cpu_to_be32(RDP_BBC_DESC_TAG);

	bbCredit = vport->fc_sparam.cmn.bbCreditLsb |
			(vport->fc_sparam.cmn.bbCreditMsb << 8);
	desc->bbc_info.port_bbc = cpu_to_be32(bbCredit);
	if (vport->phba->fc_topology != LPFC_TOPOLOGY_LOOP) {
		bbCredit = vport->phba->fc_fabparam.cmn.bbCreditLsb |
			(vport->phba->fc_fabparam.cmn.bbCreditMsb << 8);
		desc->bbc_info.attached_port_bbc = cpu_to_be32(bbCredit);
	} else {
		desc->bbc_info.attached_port_bbc = 0;
	}

	desc->bbc_info.rtt = 0;
	desc->length = cpu_to_be32(sizeof(desc->bbc_info));

	return sizeof(struct fc_rdp_bbc_desc);
}

static uint32_t
lpfc_rdp_res_oed_temp_desc(struct lpfc_hba *phba,
			   struct fc_rdp_oed_sfp_desc *desc, uint8_t *page_a2)
{
	uint32_t flags = 0;

	desc->tag = cpu_to_be32(RDP_OED_DESC_TAG);

	desc->oed_info.hi_alarm = page_a2[SSF_TEMP_HIGH_ALARM];
	desc->oed_info.lo_alarm = page_a2[SSF_TEMP_LOW_ALARM];
	desc->oed_info.hi_warning = page_a2[SSF_TEMP_HIGH_WARNING];
	desc->oed_info.lo_warning = page_a2[SSF_TEMP_LOW_WARNING];

	if (phba->sfp_alarm & LPFC_TRANSGRESSION_HIGH_TEMPERATURE)
		flags |= RDP_OET_HIGH_ALARM;
	if (phba->sfp_alarm & LPFC_TRANSGRESSION_LOW_TEMPERATURE)
		flags |= RDP_OET_LOW_ALARM;
	if (phba->sfp_warning & LPFC_TRANSGRESSION_HIGH_TEMPERATURE)
		flags |= RDP_OET_HIGH_WARNING;
	if (phba->sfp_warning & LPFC_TRANSGRESSION_LOW_TEMPERATURE)
		flags |= RDP_OET_LOW_WARNING;

	flags |= ((0xf & RDP_OED_TEMPERATURE) << RDP_OED_TYPE_SHIFT);
	desc->oed_info.function_flags = cpu_to_be32(flags);
	desc->length = cpu_to_be32(sizeof(desc->oed_info));
	return sizeof(struct fc_rdp_oed_sfp_desc);
}

static uint32_t
lpfc_rdp_res_oed_voltage_desc(struct lpfc_hba *phba,
			      struct fc_rdp_oed_sfp_desc *desc,
			      uint8_t *page_a2)
{
	uint32_t flags = 0;

	desc->tag = cpu_to_be32(RDP_OED_DESC_TAG);

	desc->oed_info.hi_alarm = page_a2[SSF_VOLTAGE_HIGH_ALARM];
	desc->oed_info.lo_alarm = page_a2[SSF_VOLTAGE_LOW_ALARM];
	desc->oed_info.hi_warning = page_a2[SSF_VOLTAGE_HIGH_WARNING];
	desc->oed_info.lo_warning = page_a2[SSF_VOLTAGE_LOW_WARNING];

	if (phba->sfp_alarm & LPFC_TRANSGRESSION_HIGH_VOLTAGE)
		flags |= RDP_OET_HIGH_ALARM;
	if (phba->sfp_alarm & LPFC_TRANSGRESSION_LOW_VOLTAGE)
		flags |= RDP_OET_LOW_ALARM;
	if (phba->sfp_warning & LPFC_TRANSGRESSION_HIGH_VOLTAGE)
		flags |= RDP_OET_HIGH_WARNING;
	if (phba->sfp_warning & LPFC_TRANSGRESSION_LOW_VOLTAGE)
		flags |= RDP_OET_LOW_WARNING;

	flags |= ((0xf & RDP_OED_VOLTAGE) << RDP_OED_TYPE_SHIFT);
	desc->oed_info.function_flags = cpu_to_be32(flags);
	desc->length = cpu_to_be32(sizeof(desc->oed_info));
	return sizeof(struct fc_rdp_oed_sfp_desc);
}

static uint32_t
lpfc_rdp_res_oed_txbias_desc(struct lpfc_hba *phba,
			     struct fc_rdp_oed_sfp_desc *desc,
			     uint8_t *page_a2)
{
	uint32_t flags = 0;

	desc->tag = cpu_to_be32(RDP_OED_DESC_TAG);

	desc->oed_info.hi_alarm = page_a2[SSF_BIAS_HIGH_ALARM];
	desc->oed_info.lo_alarm = page_a2[SSF_BIAS_LOW_ALARM];
	desc->oed_info.hi_warning = page_a2[SSF_BIAS_HIGH_WARNING];
	desc->oed_info.lo_warning = page_a2[SSF_BIAS_LOW_WARNING];

	if (phba->sfp_alarm & LPFC_TRANSGRESSION_HIGH_TXBIAS)
		flags |= RDP_OET_HIGH_ALARM;
	if (phba->sfp_alarm & LPFC_TRANSGRESSION_LOW_TXBIAS)
		flags |= RDP_OET_LOW_ALARM;
	if (phba->sfp_warning & LPFC_TRANSGRESSION_HIGH_TXBIAS)
		flags |= RDP_OET_HIGH_WARNING;
	if (phba->sfp_warning & LPFC_TRANSGRESSION_LOW_TXBIAS)
		flags |= RDP_OET_LOW_WARNING;

	flags |= ((0xf & RDP_OED_TXBIAS) << RDP_OED_TYPE_SHIFT);
	desc->oed_info.function_flags = cpu_to_be32(flags);
	desc->length = cpu_to_be32(sizeof(desc->oed_info));
	return sizeof(struct fc_rdp_oed_sfp_desc);
}

static uint32_t
lpfc_rdp_res_oed_txpower_desc(struct lpfc_hba *phba,
			      struct fc_rdp_oed_sfp_desc *desc,
			      uint8_t *page_a2)
{
	uint32_t flags = 0;

	desc->tag = cpu_to_be32(RDP_OED_DESC_TAG);

	desc->oed_info.hi_alarm = page_a2[SSF_TXPOWER_HIGH_ALARM];
	desc->oed_info.lo_alarm = page_a2[SSF_TXPOWER_LOW_ALARM];
	desc->oed_info.hi_warning = page_a2[SSF_TXPOWER_HIGH_WARNING];
	desc->oed_info.lo_warning = page_a2[SSF_TXPOWER_LOW_WARNING];

	if (phba->sfp_alarm & LPFC_TRANSGRESSION_HIGH_TXPOWER)
		flags |= RDP_OET_HIGH_ALARM;
	if (phba->sfp_alarm & LPFC_TRANSGRESSION_LOW_TXPOWER)
		flags |= RDP_OET_LOW_ALARM;
	if (phba->sfp_warning & LPFC_TRANSGRESSION_HIGH_TXPOWER)
		flags |= RDP_OET_HIGH_WARNING;
	if (phba->sfp_warning & LPFC_TRANSGRESSION_LOW_TXPOWER)
		flags |= RDP_OET_LOW_WARNING;

	flags |= ((0xf & RDP_OED_TXPOWER) << RDP_OED_TYPE_SHIFT);
	desc->oed_info.function_flags = cpu_to_be32(flags);
	desc->length = cpu_to_be32(sizeof(desc->oed_info));
	return sizeof(struct fc_rdp_oed_sfp_desc);
}


static uint32_t
lpfc_rdp_res_oed_rxpower_desc(struct lpfc_hba *phba,
			      struct fc_rdp_oed_sfp_desc *desc,
			      uint8_t *page_a2)
{
	uint32_t flags = 0;

	desc->tag = cpu_to_be32(RDP_OED_DESC_TAG);

	desc->oed_info.hi_alarm = page_a2[SSF_RXPOWER_HIGH_ALARM];
	desc->oed_info.lo_alarm = page_a2[SSF_RXPOWER_LOW_ALARM];
	desc->oed_info.hi_warning = page_a2[SSF_RXPOWER_HIGH_WARNING];
	desc->oed_info.lo_warning = page_a2[SSF_RXPOWER_LOW_WARNING];

	if (phba->sfp_alarm & LPFC_TRANSGRESSION_HIGH_RXPOWER)
		flags |= RDP_OET_HIGH_ALARM;
	if (phba->sfp_alarm & LPFC_TRANSGRESSION_LOW_RXPOWER)
		flags |= RDP_OET_LOW_ALARM;
	if (phba->sfp_warning & LPFC_TRANSGRESSION_HIGH_RXPOWER)
		flags |= RDP_OET_HIGH_WARNING;
	if (phba->sfp_warning & LPFC_TRANSGRESSION_LOW_RXPOWER)
		flags |= RDP_OET_LOW_WARNING;

	flags |= ((0xf & RDP_OED_RXPOWER) << RDP_OED_TYPE_SHIFT);
	desc->oed_info.function_flags = cpu_to_be32(flags);
	desc->length = cpu_to_be32(sizeof(desc->oed_info));
	return sizeof(struct fc_rdp_oed_sfp_desc);
}

static uint32_t
lpfc_rdp_res_opd_desc(struct fc_rdp_opd_sfp_desc *desc,
		      uint8_t *page_a0, struct lpfc_vport *vport)
{
	desc->tag = cpu_to_be32(RDP_OPD_DESC_TAG);
	memcpy(desc->opd_info.vendor_name, &page_a0[SSF_VENDOR_NAME], 16);
	memcpy(desc->opd_info.model_number, &page_a0[SSF_VENDOR_PN], 16);
	memcpy(desc->opd_info.serial_number, &page_a0[SSF_VENDOR_SN], 16);
	memcpy(desc->opd_info.revision, &page_a0[SSF_VENDOR_REV], 4);
	memcpy(desc->opd_info.date, &page_a0[SSF_DATE_CODE], 8);
	desc->length = cpu_to_be32(sizeof(desc->opd_info));
	return sizeof(struct fc_rdp_opd_sfp_desc);
}

static uint32_t
lpfc_rdp_res_fec_desc(struct fc_fec_rdp_desc *desc, READ_LNK_VAR *stat)
{
	if (bf_get(lpfc_read_link_stat_gec2, stat) == 0)
		return 0;
	desc->tag = cpu_to_be32(RDP_FEC_DESC_TAG);

	desc->info.CorrectedBlocks =
		cpu_to_be32(stat->fecCorrBlkCount);
	desc->info.UncorrectableBlocks =
		cpu_to_be32(stat->fecUncorrBlkCount);

	desc->length = cpu_to_be32(sizeof(desc->info));

	return sizeof(struct fc_fec_rdp_desc);
}

static uint32_t
lpfc_rdp_res_speed(struct fc_rdp_port_speed_desc *desc, struct lpfc_hba *phba)
{
	uint16_t rdp_cap = 0;
	uint16_t rdp_speed;

	desc->tag = cpu_to_be32(RDP_PORT_SPEED_DESC_TAG);

	switch (phba->fc_linkspeed) {
	case LPFC_LINK_SPEED_1GHZ:
		rdp_speed = RDP_PS_1GB;
		break;
	case LPFC_LINK_SPEED_2GHZ:
		rdp_speed = RDP_PS_2GB;
		break;
	case LPFC_LINK_SPEED_4GHZ:
		rdp_speed = RDP_PS_4GB;
		break;
	case LPFC_LINK_SPEED_8GHZ:
		rdp_speed = RDP_PS_8GB;
		break;
	case LPFC_LINK_SPEED_10GHZ:
		rdp_speed = RDP_PS_10GB;
		break;
	case LPFC_LINK_SPEED_16GHZ:
		rdp_speed = RDP_PS_16GB;
		break;
	case LPFC_LINK_SPEED_32GHZ:
		rdp_speed = RDP_PS_32GB;
		break;
	case LPFC_LINK_SPEED_64GHZ:
		rdp_speed = RDP_PS_64GB;
		break;
	case LPFC_LINK_SPEED_128GHZ:
		rdp_speed = RDP_PS_128GB;
		break;
	case LPFC_LINK_SPEED_256GHZ:
		rdp_speed = RDP_PS_256GB;
		break;
	default:
		rdp_speed = RDP_PS_UNKNOWN;
		break;
	}

	desc->info.port_speed.speed = cpu_to_be16(rdp_speed);

	if (phba->lmt & LMT_256Gb)
		rdp_cap |= RDP_PS_256GB;
	if (phba->lmt & LMT_128Gb)
		rdp_cap |= RDP_PS_128GB;
	if (phba->lmt & LMT_64Gb)
		rdp_cap |= RDP_PS_64GB;
	if (phba->lmt & LMT_32Gb)
		rdp_cap |= RDP_PS_32GB;
	if (phba->lmt & LMT_16Gb)
		rdp_cap |= RDP_PS_16GB;
	if (phba->lmt & LMT_10Gb)
		rdp_cap |= RDP_PS_10GB;
	if (phba->lmt & LMT_8Gb)
		rdp_cap |= RDP_PS_8GB;
	if (phba->lmt & LMT_4Gb)
		rdp_cap |= RDP_PS_4GB;
	if (phba->lmt & LMT_2Gb)
		rdp_cap |= RDP_PS_2GB;
	if (phba->lmt & LMT_1Gb)
		rdp_cap |= RDP_PS_1GB;

	if (rdp_cap == 0)
		rdp_cap = RDP_CAP_UNKNOWN;
	if (phba->cfg_link_speed != LPFC_USER_LINK_SPEED_AUTO)
		rdp_cap |= RDP_CAP_USER_CONFIGURED;

	desc->info.port_speed.capabilities = cpu_to_be16(rdp_cap);
	desc->length = cpu_to_be32(sizeof(desc->info));
	return sizeof(struct fc_rdp_port_speed_desc);
}

static uint32_t
lpfc_rdp_res_diag_port_names(struct fc_rdp_port_name_desc *desc,
		struct lpfc_vport *vport)
{

	desc->tag = cpu_to_be32(RDP_PORT_NAMES_DESC_TAG);

	memcpy(desc->port_names.wwnn, &vport->fc_nodename,
			sizeof(desc->port_names.wwnn));

	memcpy(desc->port_names.wwpn, &vport->fc_portname,
			sizeof(desc->port_names.wwpn));

	desc->length = cpu_to_be32(sizeof(desc->port_names));
	return sizeof(struct fc_rdp_port_name_desc);
}

static uint32_t
lpfc_rdp_res_attach_port_names(struct fc_rdp_port_name_desc *desc,
		struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{

	desc->tag = cpu_to_be32(RDP_PORT_NAMES_DESC_TAG);
	if (vport->fc_flag & FC_FABRIC) {
		memcpy(desc->port_names.wwnn, &vport->fabric_nodename,
		       sizeof(desc->port_names.wwnn));

		memcpy(desc->port_names.wwpn, &vport->fabric_portname,
		       sizeof(desc->port_names.wwpn));
	} else {   
		memcpy(desc->port_names.wwnn, &ndlp->nlp_nodename,
		       sizeof(desc->port_names.wwnn));

		memcpy(desc->port_names.wwpn, &ndlp->nlp_portname,
		       sizeof(desc->port_names.wwpn));
	}

	desc->length = cpu_to_be32(sizeof(desc->port_names));
	return sizeof(struct fc_rdp_port_name_desc);
}

static void
lpfc_els_rdp_cmpl(struct lpfc_hba *phba, struct lpfc_rdp_context *rdp_context,
		int status)
{
	struct lpfc_nodelist *ndlp = rdp_context->ndlp;
	struct lpfc_vport *vport = ndlp->vport;
	struct lpfc_iocbq *elsiocb;
	struct ulp_bde64 *bpl;
	IOCB_t *icmd;
	union lpfc_wqe128 *wqe;
	uint8_t *pcmd;
	struct ls_rjt *stat;
	struct fc_rdp_res_frame *rdp_res;
	uint32_t cmdsize, len;
	uint16_t *flag_ptr;
	int rc;
	u32 ulp_context;

	if (status != SUCCESS)
		goto error;

	 
	cmdsize = sizeof(struct fc_rdp_res_frame);

	elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize,
				lpfc_max_els_tries, rdp_context->ndlp,
				rdp_context->ndlp->nlp_DID, ELS_CMD_ACC);
	if (!elsiocb)
		goto free_rdp_context;

	ulp_context = get_job_ulpcontext(phba, elsiocb);
	if (phba->sli_rev == LPFC_SLI_REV4) {
		wqe = &elsiocb->wqe;
		 
		bf_set(wqe_rcvoxid, &wqe->xmit_els_rsp.wqe_com,
		       rdp_context->ox_id);
		bf_set(wqe_ctxt_tag, &wqe->xmit_els_rsp.wqe_com,
		       rdp_context->rx_id);
	} else {
		icmd = &elsiocb->iocb;
		icmd->ulpContext = rdp_context->rx_id;
		icmd->unsli3.rcvsli3.ox_id = rdp_context->ox_id;
	}

	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			"2171 Xmit RDP response tag x%x xri x%x, "
			"did x%x, nlp_flag x%x, nlp_state x%x, rpi x%x",
			elsiocb->iotag, ulp_context,
			ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			ndlp->nlp_rpi);
	rdp_res = (struct fc_rdp_res_frame *)elsiocb->cmd_dmabuf->virt;
	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;
	memset(pcmd, 0, sizeof(struct fc_rdp_res_frame));
	*((uint32_t *) (pcmd)) = ELS_CMD_ACC;

	 
	flag_ptr = (uint16_t *)(rdp_context->page_a2 + SSF_ALARM_FLAGS);
	phba->sfp_alarm |= *flag_ptr;
	flag_ptr = (uint16_t *)(rdp_context->page_a2 + SSF_WARNING_FLAGS);
	phba->sfp_warning |= *flag_ptr;

	 
	len = 8;
	len += lpfc_rdp_res_link_service((struct fc_rdp_link_service_desc *)
					 (len + pcmd), ELS_CMD_RDP);

	len += lpfc_rdp_res_sfp_desc((struct fc_rdp_sfp_desc *)(len + pcmd),
			rdp_context->page_a0, rdp_context->page_a2);
	len += lpfc_rdp_res_speed((struct fc_rdp_port_speed_desc *)(len + pcmd),
				  phba);
	len += lpfc_rdp_res_link_error((struct fc_rdp_link_error_status_desc *)
				       (len + pcmd), &rdp_context->link_stat);
	len += lpfc_rdp_res_diag_port_names((struct fc_rdp_port_name_desc *)
					     (len + pcmd), vport);
	len += lpfc_rdp_res_attach_port_names((struct fc_rdp_port_name_desc *)
					(len + pcmd), vport, ndlp);
	len += lpfc_rdp_res_fec_desc((struct fc_fec_rdp_desc *)(len + pcmd),
			&rdp_context->link_stat);
	len += lpfc_rdp_res_bbc_desc((struct fc_rdp_bbc_desc *)(len + pcmd),
				     &rdp_context->link_stat, vport);
	len += lpfc_rdp_res_oed_temp_desc(phba,
				(struct fc_rdp_oed_sfp_desc *)(len + pcmd),
				rdp_context->page_a2);
	len += lpfc_rdp_res_oed_voltage_desc(phba,
				(struct fc_rdp_oed_sfp_desc *)(len + pcmd),
				rdp_context->page_a2);
	len += lpfc_rdp_res_oed_txbias_desc(phba,
				(struct fc_rdp_oed_sfp_desc *)(len + pcmd),
				rdp_context->page_a2);
	len += lpfc_rdp_res_oed_txpower_desc(phba,
				(struct fc_rdp_oed_sfp_desc *)(len + pcmd),
				rdp_context->page_a2);
	len += lpfc_rdp_res_oed_rxpower_desc(phba,
				(struct fc_rdp_oed_sfp_desc *)(len + pcmd),
				rdp_context->page_a2);
	len += lpfc_rdp_res_opd_desc((struct fc_rdp_opd_sfp_desc *)(len + pcmd),
				     rdp_context->page_a0, vport);

	rdp_res->length = cpu_to_be32(len - 8);
	elsiocb->cmd_cmpl = lpfc_cmpl_els_rsp;

	 
	bpl = (struct ulp_bde64 *)elsiocb->bpl_dmabuf->virt;
	bpl->tus.f.bdeSize = len;
	bpl->tus.f.bdeFlags = 0;
	bpl->tus.w = le32_to_cpu(bpl->tus.w);

	phba->fc_stat.elsXmitACC++;
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		goto free_rdp_context;
	}

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
	}

	goto free_rdp_context;

error:
	cmdsize = 2 * sizeof(uint32_t);
	elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, lpfc_max_els_tries,
			ndlp, ndlp->nlp_DID, ELS_CMD_LS_RJT);
	if (!elsiocb)
		goto free_rdp_context;

	if (phba->sli_rev == LPFC_SLI_REV4) {
		wqe = &elsiocb->wqe;
		 
		bf_set(wqe_rcvoxid, &wqe->xmit_els_rsp.wqe_com,
		       rdp_context->ox_id);
		bf_set(wqe_ctxt_tag,
		       &wqe->xmit_els_rsp.wqe_com,
		       rdp_context->rx_id);
	} else {
		icmd = &elsiocb->iocb;
		icmd->ulpContext = rdp_context->rx_id;
		icmd->unsli3.rcvsli3.ox_id = rdp_context->ox_id;
	}

	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;

	*((uint32_t *) (pcmd)) = ELS_CMD_LS_RJT;
	stat = (struct ls_rjt *)(pcmd + sizeof(uint32_t));
	stat->un.b.lsRjtRsnCode = LSRJT_UNABLE_TPC;

	phba->fc_stat.elsXmitLSRJT++;
	elsiocb->cmd_cmpl = lpfc_cmpl_els_rsp;
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		goto free_rdp_context;
	}

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
	}

free_rdp_context:
	 
	lpfc_nlp_put(ndlp);
	kfree(rdp_context);
}

static int
lpfc_get_rdp_info(struct lpfc_hba *phba, struct lpfc_rdp_context *rdp_context)
{
	LPFC_MBOXQ_t *mbox = NULL;
	int rc;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_MBOX | LOG_ELS,
				"7105 failed to allocate mailbox memory");
		return 1;
	}

	if (lpfc_sli4_dump_page_a0(phba, mbox))
		goto rdp_fail;
	mbox->vport = rdp_context->ndlp->vport;
	mbox->mbox_cmpl = lpfc_mbx_cmpl_rdp_page_a0;
	mbox->ctx_ndlp = (struct lpfc_rdp_context *)rdp_context;
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		lpfc_mbox_rsrc_cleanup(phba, mbox, MBOX_THD_UNLOCKED);
		return 1;
	}

	return 0;

rdp_fail:
	mempool_free(mbox, phba->mbox_mem_pool);
	return 1;
}

int lpfc_get_sfp_info_wait(struct lpfc_hba *phba,
			   struct lpfc_rdp_context *rdp_context)
{
	LPFC_MBOXQ_t *mbox = NULL;
	int rc;
	struct lpfc_dmabuf *mp;
	struct lpfc_dmabuf *mpsave;
	void *virt;
	MAILBOX_t *mb;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_MBOX | LOG_ELS,
				"7205 failed to allocate mailbox memory");
		return 1;
	}

	if (lpfc_sli4_dump_page_a0(phba, mbox))
		goto sfp_fail;
	mp = mbox->ctx_buf;
	mpsave = mp;
	virt = mp->virt;
	if (phba->sli_rev < LPFC_SLI_REV4) {
		mb = &mbox->u.mb;
		mb->un.varDmp.cv = 1;
		mb->un.varDmp.co = 1;
		mb->un.varWords[2] = 0;
		mb->un.varWords[3] = DMP_SFF_PAGE_A0_SIZE / 4;
		mb->un.varWords[4] = 0;
		mb->un.varWords[5] = 0;
		mb->un.varWords[6] = 0;
		mb->un.varWords[7] = 0;
		mb->un.varWords[8] = 0;
		mb->un.varWords[9] = 0;
		mb->un.varWords[10] = 0;
		mbox->in_ext_byte_len = DMP_SFF_PAGE_A0_SIZE;
		mbox->out_ext_byte_len = DMP_SFF_PAGE_A0_SIZE;
		mbox->mbox_offset_word = 5;
		mbox->ctx_buf = virt;
	} else {
		bf_set(lpfc_mbx_memory_dump_type3_length,
		       &mbox->u.mqe.un.mem_dump_type3, DMP_SFF_PAGE_A0_SIZE);
		mbox->u.mqe.un.mem_dump_type3.addr_lo = putPaddrLow(mp->phys);
		mbox->u.mqe.un.mem_dump_type3.addr_hi = putPaddrHigh(mp->phys);
	}
	mbox->vport = phba->pport;
	mbox->ctx_ndlp = (struct lpfc_rdp_context *)rdp_context;

	rc = lpfc_sli_issue_mbox_wait(phba, mbox, 30);
	if (rc == MBX_NOT_FINISHED) {
		rc = 1;
		goto error;
	}

	if (phba->sli_rev == LPFC_SLI_REV4)
		mp = (struct lpfc_dmabuf *)(mbox->ctx_buf);
	else
		mp = mpsave;

	if (bf_get(lpfc_mqe_status, &mbox->u.mqe)) {
		rc = 1;
		goto error;
	}

	lpfc_sli_bemem_bcopy(mp->virt, &rdp_context->page_a0,
			     DMP_SFF_PAGE_A0_SIZE);

	memset(mbox, 0, sizeof(*mbox));
	memset(mp->virt, 0, DMP_SFF_PAGE_A2_SIZE);
	INIT_LIST_HEAD(&mp->list);

	 
	mbox->ctx_buf = mp;
	mbox->vport = phba->pport;

	bf_set(lpfc_mqe_command, &mbox->u.mqe, MBX_DUMP_MEMORY);
	bf_set(lpfc_mbx_memory_dump_type3_type,
	       &mbox->u.mqe.un.mem_dump_type3, DMP_LMSD);
	bf_set(lpfc_mbx_memory_dump_type3_link,
	       &mbox->u.mqe.un.mem_dump_type3, phba->sli4_hba.physical_port);
	bf_set(lpfc_mbx_memory_dump_type3_page_no,
	       &mbox->u.mqe.un.mem_dump_type3, DMP_PAGE_A2);
	if (phba->sli_rev < LPFC_SLI_REV4) {
		mb = &mbox->u.mb;
		mb->un.varDmp.cv = 1;
		mb->un.varDmp.co = 1;
		mb->un.varWords[2] = 0;
		mb->un.varWords[3] = DMP_SFF_PAGE_A2_SIZE / 4;
		mb->un.varWords[4] = 0;
		mb->un.varWords[5] = 0;
		mb->un.varWords[6] = 0;
		mb->un.varWords[7] = 0;
		mb->un.varWords[8] = 0;
		mb->un.varWords[9] = 0;
		mb->un.varWords[10] = 0;
		mbox->in_ext_byte_len = DMP_SFF_PAGE_A2_SIZE;
		mbox->out_ext_byte_len = DMP_SFF_PAGE_A2_SIZE;
		mbox->mbox_offset_word = 5;
		mbox->ctx_buf = virt;
	} else {
		bf_set(lpfc_mbx_memory_dump_type3_length,
		       &mbox->u.mqe.un.mem_dump_type3, DMP_SFF_PAGE_A2_SIZE);
		mbox->u.mqe.un.mem_dump_type3.addr_lo = putPaddrLow(mp->phys);
		mbox->u.mqe.un.mem_dump_type3.addr_hi = putPaddrHigh(mp->phys);
	}

	mbox->ctx_ndlp = (struct lpfc_rdp_context *)rdp_context;
	rc = lpfc_sli_issue_mbox_wait(phba, mbox, 30);
	if (bf_get(lpfc_mqe_status, &mbox->u.mqe)) {
		rc = 1;
		goto error;
	}
	rc = 0;

	lpfc_sli_bemem_bcopy(mp->virt, &rdp_context->page_a2,
			     DMP_SFF_PAGE_A2_SIZE);

error:
	mbox->ctx_buf = mpsave;
	lpfc_mbox_rsrc_cleanup(phba, mbox, MBOX_THD_UNLOCKED);

	return rc;

sfp_fail:
	mempool_free(mbox, phba->mbox_mem_pool);
	return 1;
}

 
static int
lpfc_els_rcv_rdp(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_dmabuf *pcmd;
	uint8_t rjt_err, rjt_expl = LSEXP_NOTHING_MORE;
	struct fc_rdp_req_frame *rdp_req;
	struct lpfc_rdp_context *rdp_context;
	union lpfc_wqe128 *cmd = NULL;
	struct ls_rjt stat;

	if (phba->sli_rev < LPFC_SLI_REV4 ||
	    bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) <
						LPFC_SLI_INTF_IF_TYPE_2) {
		rjt_err = LSRJT_UNABLE_TPC;
		rjt_expl = LSEXP_REQ_UNSUPPORTED;
		goto error;
	}

	if (phba->sli_rev < LPFC_SLI_REV4 || (phba->hba_flag & HBA_FCOE_MODE)) {
		rjt_err = LSRJT_UNABLE_TPC;
		rjt_expl = LSEXP_REQ_UNSUPPORTED;
		goto error;
	}

	pcmd = cmdiocb->cmd_dmabuf;
	rdp_req = (struct fc_rdp_req_frame *) pcmd->virt;

	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "2422 ELS RDP Request "
			 "dec len %d tag x%x port_id %d len %d\n",
			 be32_to_cpu(rdp_req->rdp_des_length),
			 be32_to_cpu(rdp_req->nport_id_desc.tag),
			 be32_to_cpu(rdp_req->nport_id_desc.nport_id),
			 be32_to_cpu(rdp_req->nport_id_desc.length));

	if (sizeof(struct fc_rdp_nport_desc) !=
			be32_to_cpu(rdp_req->rdp_des_length))
		goto rjt_logerr;
	if (RDP_N_PORT_DESC_TAG != be32_to_cpu(rdp_req->nport_id_desc.tag))
		goto rjt_logerr;
	if (RDP_NPORT_ID_SIZE !=
			be32_to_cpu(rdp_req->nport_id_desc.length))
		goto rjt_logerr;
	rdp_context = kzalloc(sizeof(struct lpfc_rdp_context), GFP_KERNEL);
	if (!rdp_context) {
		rjt_err = LSRJT_UNABLE_TPC;
		goto error;
	}

	cmd = &cmdiocb->wqe;
	rdp_context->ndlp = lpfc_nlp_get(ndlp);
	if (!rdp_context->ndlp) {
		kfree(rdp_context);
		rjt_err = LSRJT_UNABLE_TPC;
		goto error;
	}
	rdp_context->ox_id = bf_get(wqe_rcvoxid,
				    &cmd->xmit_els_rsp.wqe_com);
	rdp_context->rx_id = bf_get(wqe_ctxt_tag,
				    &cmd->xmit_els_rsp.wqe_com);
	rdp_context->cmpl = lpfc_els_rdp_cmpl;
	if (lpfc_get_rdp_info(phba, rdp_context)) {
		lpfc_printf_vlog(ndlp->vport, KERN_WARNING, LOG_ELS,
				 "2423 Unable to send mailbox");
		kfree(rdp_context);
		rjt_err = LSRJT_UNABLE_TPC;
		lpfc_nlp_put(ndlp);
		goto error;
	}

	return 0;

rjt_logerr:
	rjt_err = LSRJT_LOGICAL_ERR;

error:
	memset(&stat, 0, sizeof(stat));
	stat.un.b.lsRjtRsnCode = rjt_err;
	stat.un.b.lsRjtRsnCodeExp = rjt_expl;
	lpfc_els_rsp_reject(vport, stat.un.lsRjtError, cmdiocb, ndlp, NULL);
	return 1;
}


static void
lpfc_els_lcb_rsp(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	MAILBOX_t *mb;
	IOCB_t *icmd;
	union lpfc_wqe128 *wqe;
	uint8_t *pcmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_nodelist *ndlp;
	struct ls_rjt *stat;
	union lpfc_sli4_cfg_shdr *shdr;
	struct lpfc_lcb_context *lcb_context;
	struct fc_lcb_res_frame *lcb_res;
	uint32_t cmdsize, shdr_status, shdr_add_status;
	int rc;

	mb = &pmb->u.mb;
	lcb_context = (struct lpfc_lcb_context *)pmb->ctx_ndlp;
	ndlp = lcb_context->ndlp;
	pmb->ctx_ndlp = NULL;
	pmb->ctx_buf = NULL;

	shdr = (union lpfc_sli4_cfg_shdr *)
			&pmb->u.mqe.un.beacon_config.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);

	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX,
				"0194 SET_BEACON_CONFIG mailbox "
				"completed with status x%x add_status x%x,"
				" mbx status x%x\n",
				shdr_status, shdr_add_status, mb->mbxStatus);

	if ((mb->mbxStatus != MBX_SUCCESS) || shdr_status ||
	    (shdr_add_status == ADD_STATUS_OPERATION_ALREADY_ACTIVE) ||
	    (shdr_add_status == ADD_STATUS_INVALID_REQUEST)) {
		mempool_free(pmb, phba->mbox_mem_pool);
		goto error;
	}

	mempool_free(pmb, phba->mbox_mem_pool);
	cmdsize = sizeof(struct fc_lcb_res_frame);
	elsiocb = lpfc_prep_els_iocb(phba->pport, 0, cmdsize,
			lpfc_max_els_tries, ndlp,
			ndlp->nlp_DID, ELS_CMD_ACC);

	 
	lpfc_nlp_put(ndlp);

	if (!elsiocb)
		goto free_lcb_context;

	lcb_res = (struct fc_lcb_res_frame *)elsiocb->cmd_dmabuf->virt;

	memset(lcb_res, 0, sizeof(struct fc_lcb_res_frame));

	if (phba->sli_rev == LPFC_SLI_REV4) {
		wqe = &elsiocb->wqe;
		bf_set(wqe_ctxt_tag, &wqe->generic.wqe_com, lcb_context->rx_id);
		bf_set(wqe_rcvoxid, &wqe->xmit_els_rsp.wqe_com,
		       lcb_context->ox_id);
	} else {
		icmd = &elsiocb->iocb;
		icmd->ulpContext = lcb_context->rx_id;
		icmd->unsli3.rcvsli3.ox_id = lcb_context->ox_id;
	}

	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;
	*((uint32_t *)(pcmd)) = ELS_CMD_ACC;
	lcb_res->lcb_sub_command = lcb_context->sub_command;
	lcb_res->lcb_type = lcb_context->type;
	lcb_res->capability = lcb_context->capability;
	lcb_res->lcb_frequency = lcb_context->frequency;
	lcb_res->lcb_duration = lcb_context->duration;
	elsiocb->cmd_cmpl = lpfc_cmpl_els_rsp;
	phba->fc_stat.elsXmitACC++;

	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		goto out;
	}

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
	}
 out:
	kfree(lcb_context);
	return;

error:
	cmdsize = sizeof(struct fc_lcb_res_frame);
	elsiocb = lpfc_prep_els_iocb(phba->pport, 0, cmdsize,
				     lpfc_max_els_tries, ndlp,
				     ndlp->nlp_DID, ELS_CMD_LS_RJT);
	lpfc_nlp_put(ndlp);
	if (!elsiocb)
		goto free_lcb_context;

	if (phba->sli_rev == LPFC_SLI_REV4) {
		wqe = &elsiocb->wqe;
		bf_set(wqe_ctxt_tag, &wqe->generic.wqe_com, lcb_context->rx_id);
		bf_set(wqe_rcvoxid, &wqe->xmit_els_rsp.wqe_com,
		       lcb_context->ox_id);
	} else {
		icmd = &elsiocb->iocb;
		icmd->ulpContext = lcb_context->rx_id;
		icmd->unsli3.rcvsli3.ox_id = lcb_context->ox_id;
	}

	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;

	*((uint32_t *)(pcmd)) = ELS_CMD_LS_RJT;
	stat = (struct ls_rjt *)(pcmd + sizeof(uint32_t));
	stat->un.b.lsRjtRsnCode = LSRJT_UNABLE_TPC;

	if (shdr_add_status == ADD_STATUS_OPERATION_ALREADY_ACTIVE)
		stat->un.b.lsRjtRsnCodeExp = LSEXP_CMD_IN_PROGRESS;

	elsiocb->cmd_cmpl = lpfc_cmpl_els_rsp;
	phba->fc_stat.elsXmitLSRJT++;
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		goto free_lcb_context;
	}

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
	}
free_lcb_context:
	kfree(lcb_context);
}

static int
lpfc_sli4_set_beacon(struct lpfc_vport *vport,
		     struct lpfc_lcb_context *lcb_context,
		     uint32_t beacon_state)
{
	struct lpfc_hba *phba = vport->phba;
	union lpfc_sli4_cfg_shdr *cfg_shdr;
	LPFC_MBOXQ_t *mbox = NULL;
	uint32_t len;
	int rc;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return 1;

	cfg_shdr = &mbox->u.mqe.un.sli4_config.header.cfg_shdr;
	len = sizeof(struct lpfc_mbx_set_beacon_config) -
		sizeof(struct lpfc_sli4_cfg_mhdr);
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_SET_BEACON_CONFIG, len,
			 LPFC_SLI4_MBX_EMBED);
	mbox->ctx_ndlp = (void *)lcb_context;
	mbox->vport = phba->pport;
	mbox->mbox_cmpl = lpfc_els_lcb_rsp;
	bf_set(lpfc_mbx_set_beacon_port_num, &mbox->u.mqe.un.beacon_config,
	       phba->sli4_hba.physical_port);
	bf_set(lpfc_mbx_set_beacon_state, &mbox->u.mqe.un.beacon_config,
	       beacon_state);
	mbox->u.mqe.un.beacon_config.word5 = 0;		 

	 

	if (phba->sli4_hba.pc_sli4_params.bv1s) {
		 
		cfg_shdr->request.word9 = BEACON_VERSION_V1;
		lcb_context->capability |= LCB_CAPABILITY_DURATION;
		bf_set(lpfc_mbx_set_beacon_port_type,
		       &mbox->u.mqe.un.beacon_config, 0);
		bf_set(lpfc_mbx_set_beacon_duration_v1,
		       &mbox->u.mqe.un.beacon_config,
		       be16_to_cpu(lcb_context->duration));
	} else {
		 
		if (be16_to_cpu(lcb_context->duration) != 0) {
			mempool_free(mbox, phba->mbox_mem_pool);
			return 1;
		}
		cfg_shdr->request.word9 = BEACON_VERSION_V0;
		lcb_context->capability &=  ~(LCB_CAPABILITY_DURATION);
		bf_set(lpfc_mbx_set_beacon_state,
		       &mbox->u.mqe.un.beacon_config, beacon_state);
		bf_set(lpfc_mbx_set_beacon_port_type,
		       &mbox->u.mqe.un.beacon_config, 1);
		bf_set(lpfc_mbx_set_beacon_duration,
		       &mbox->u.mqe.un.beacon_config,
		       be16_to_cpu(lcb_context->duration));
	}

	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		mempool_free(mbox, phba->mbox_mem_pool);
		return 1;
	}

	return 0;
}


 
static int
lpfc_els_rcv_lcb(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		 struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_dmabuf *pcmd;
	uint8_t *lp;
	struct fc_lcb_request_frame *beacon;
	struct lpfc_lcb_context *lcb_context;
	u8 state, rjt_err = 0;
	struct ls_rjt stat;

	pcmd = cmdiocb->cmd_dmabuf;
	lp = (uint8_t *)pcmd->virt;
	beacon = (struct fc_lcb_request_frame *)pcmd->virt;

	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			"0192 ELS LCB Data x%x x%x x%x x%x sub x%x "
			"type x%x frequency %x duration x%x\n",
			lp[0], lp[1], lp[2],
			beacon->lcb_command,
			beacon->lcb_sub_command,
			beacon->lcb_type,
			beacon->lcb_frequency,
			be16_to_cpu(beacon->lcb_duration));

	if (beacon->lcb_sub_command != LPFC_LCB_ON &&
	    beacon->lcb_sub_command != LPFC_LCB_OFF) {
		rjt_err = LSRJT_CMD_UNSUPPORTED;
		goto rjt;
	}

	if (phba->sli_rev < LPFC_SLI_REV4  ||
	    phba->hba_flag & HBA_FCOE_MODE ||
	    (bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) <
	    LPFC_SLI_INTF_IF_TYPE_2)) {
		rjt_err = LSRJT_CMD_UNSUPPORTED;
		goto rjt;
	}

	lcb_context = kmalloc(sizeof(*lcb_context), GFP_KERNEL);
	if (!lcb_context) {
		rjt_err = LSRJT_UNABLE_TPC;
		goto rjt;
	}

	state = (beacon->lcb_sub_command == LPFC_LCB_ON) ? 1 : 0;
	lcb_context->sub_command = beacon->lcb_sub_command;
	lcb_context->capability	= 0;
	lcb_context->type = beacon->lcb_type;
	lcb_context->frequency = beacon->lcb_frequency;
	lcb_context->duration = beacon->lcb_duration;
	lcb_context->ox_id = get_job_rcvoxid(phba, cmdiocb);
	lcb_context->rx_id = get_job_ulpcontext(phba, cmdiocb);
	lcb_context->ndlp = lpfc_nlp_get(ndlp);
	if (!lcb_context->ndlp) {
		rjt_err = LSRJT_UNABLE_TPC;
		goto rjt_free;
	}

	if (lpfc_sli4_set_beacon(vport, lcb_context, state)) {
		lpfc_printf_vlog(ndlp->vport, KERN_ERR, LOG_TRACE_EVENT,
				 "0193 failed to send mail box");
		lpfc_nlp_put(ndlp);
		rjt_err = LSRJT_UNABLE_TPC;
		goto rjt_free;
	}
	return 0;

rjt_free:
	kfree(lcb_context);
rjt:
	memset(&stat, 0, sizeof(stat));
	stat.un.b.lsRjtRsnCode = rjt_err;
	lpfc_els_rsp_reject(vport, stat.un.lsRjtError, cmdiocb, ndlp, NULL);
	return 1;
}


 
void
lpfc_els_flush_rscn(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	int i;

	spin_lock_irq(shost->host_lock);
	if (vport->fc_rscn_flush) {
		 
		spin_unlock_irq(shost->host_lock);
		return;
	}
	 
	vport->fc_rscn_flush = 1;
	spin_unlock_irq(shost->host_lock);

	for (i = 0; i < vport->fc_rscn_id_cnt; i++) {
		lpfc_in_buf_free(phba, vport->fc_rscn_id_list[i]);
		vport->fc_rscn_id_list[i] = NULL;
	}
	spin_lock_irq(shost->host_lock);
	vport->fc_rscn_id_cnt = 0;
	vport->fc_flag &= ~(FC_RSCN_MODE | FC_RSCN_DISCOVERY);
	spin_unlock_irq(shost->host_lock);
	lpfc_can_disctmo(vport);
	 
	vport->fc_rscn_flush = 0;
}

 
int
lpfc_rscn_payload_check(struct lpfc_vport *vport, uint32_t did)
{
	D_ID ns_did;
	D_ID rscn_did;
	uint32_t *lp;
	uint32_t payload_len, i;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	ns_did.un.word = did;

	 
	if ((did & Fabric_DID_MASK) == Fabric_DID_MASK)
		return 0;

	 
	if (vport->fc_flag & FC_RSCN_DISCOVERY)
		return did;

	spin_lock_irq(shost->host_lock);
	if (vport->fc_rscn_flush) {
		 
		spin_unlock_irq(shost->host_lock);
		return 0;
	}
	 
	vport->fc_rscn_flush = 1;
	spin_unlock_irq(shost->host_lock);
	for (i = 0; i < vport->fc_rscn_id_cnt; i++) {
		lp = vport->fc_rscn_id_list[i]->virt;
		payload_len = be32_to_cpu(*lp++ & ~ELS_CMD_MASK);
		payload_len -= sizeof(uint32_t);	 
		while (payload_len) {
			rscn_did.un.word = be32_to_cpu(*lp++);
			payload_len -= sizeof(uint32_t);
			switch (rscn_did.un.b.resv & RSCN_ADDRESS_FORMAT_MASK) {
			case RSCN_ADDRESS_FORMAT_PORT:
				if ((ns_did.un.b.domain == rscn_did.un.b.domain)
				    && (ns_did.un.b.area == rscn_did.un.b.area)
				    && (ns_did.un.b.id == rscn_did.un.b.id))
					goto return_did_out;
				break;
			case RSCN_ADDRESS_FORMAT_AREA:
				if ((ns_did.un.b.domain == rscn_did.un.b.domain)
				    && (ns_did.un.b.area == rscn_did.un.b.area))
					goto return_did_out;
				break;
			case RSCN_ADDRESS_FORMAT_DOMAIN:
				if (ns_did.un.b.domain == rscn_did.un.b.domain)
					goto return_did_out;
				break;
			case RSCN_ADDRESS_FORMAT_FABRIC:
				goto return_did_out;
			}
		}
	}
	 
	vport->fc_rscn_flush = 0;
	return 0;
return_did_out:
	 
	vport->fc_rscn_flush = 0;
	return did;
}

 
static int
lpfc_rscn_recovery_check(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp = NULL, *n;

	 
	list_for_each_entry_safe(ndlp, n, &vport->fc_nodes, nlp_listp) {
		if ((ndlp->nlp_state == NLP_STE_UNUSED_NODE) ||
		    !lpfc_rscn_payload_check(vport, ndlp->nlp_DID))
			continue;

		 
		if (vport->phba->nvmet_support)
			continue;

		 
		switch (ndlp->nlp_state) {
		case  NLP_STE_PLOGI_ISSUE:
		case  NLP_STE_ADISC_ISSUE:
		case  NLP_STE_REG_LOGIN_ISSUE:
		case  NLP_STE_PRLI_ISSUE:
		case  NLP_STE_LOGO_ISSUE:
			continue;
		}

		lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RECOVERY);
		lpfc_cancel_retry_delay_tmo(vport, ndlp);
	}
	return 0;
}

 
static void
lpfc_send_rscn_event(struct lpfc_vport *vport,
		struct lpfc_iocbq *cmdiocb)
{
	struct lpfc_dmabuf *pcmd;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	uint32_t *payload_ptr;
	uint32_t payload_len;
	struct lpfc_rscn_event_header *rscn_event_data;

	pcmd = cmdiocb->cmd_dmabuf;
	payload_ptr = (uint32_t *) pcmd->virt;
	payload_len = be32_to_cpu(*payload_ptr & ~ELS_CMD_MASK);

	rscn_event_data = kmalloc(sizeof(struct lpfc_rscn_event_header) +
		payload_len, GFP_KERNEL);
	if (!rscn_event_data) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
			"0147 Failed to allocate memory for RSCN event\n");
		return;
	}
	rscn_event_data->event_type = FC_REG_RSCN_EVENT;
	rscn_event_data->payload_length = payload_len;
	memcpy(rscn_event_data->rscn_payload, payload_ptr,
		payload_len);

	fc_host_post_vendor_event(shost,
		fc_get_event_number(),
		sizeof(struct lpfc_rscn_event_header) + payload_len,
		(char *)rscn_event_data,
		LPFC_NL_VENDOR_ID);

	kfree(rscn_event_data);
}

 
static int
lpfc_els_rcv_rscn(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_nodelist *ndlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_dmabuf *pcmd;
	uint32_t *lp, *datap;
	uint32_t payload_len, length, nportid, *cmd;
	int rscn_cnt;
	int rscn_id = 0, hba_id = 0;
	int i, tmo;

	pcmd = cmdiocb->cmd_dmabuf;
	lp = (uint32_t *) pcmd->virt;

	payload_len = be32_to_cpu(*lp++ & ~ELS_CMD_MASK);
	payload_len -= sizeof(uint32_t);	 
	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0214 RSCN received Data: x%x x%x x%x x%x\n",
			 vport->fc_flag, payload_len, *lp,
			 vport->fc_rscn_id_cnt);

	 
	lpfc_send_rscn_event(vport, cmdiocb);

	for (i = 0; i < payload_len/sizeof(uint32_t); i++)
		fc_host_post_event(shost, fc_get_event_number(),
			FCH_EVT_RSCN, lp[i]);

	 
	if (vport->fc_flag & FC_PT2PT) {
		 
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
				 "2024 pt2pt RSCN %08x Data: x%x x%x\n",
				 *lp, vport->fc_flag, payload_len);
		lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb, ndlp, NULL);

		 
		if (ndlp->nlp_fc4_type & NLP_FC4_NVME &&
		    ndlp->nlp_type & (NLP_NVME_TARGET | NLP_NVME_DISCOVERY))
			lpfc_nvme_rescan_port(vport, ndlp);
		return 0;
	}

	 
	if (vport->port_state <= LPFC_NS_QRY) {
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RSCN ignore: did:x%x/ste:x%x flg:x%x",
			ndlp->nlp_DID, vport->port_state, ndlp->nlp_flag);

		lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb, ndlp, NULL);
		return 0;
	}

	 
	if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
		!(vport->cfg_peer_port_login)) {
		i = payload_len;
		datap = lp;
		while (i > 0) {
			nportid = *datap++;
			nportid = ((be32_to_cpu(nportid)) & Mask_DID);
			i -= sizeof(uint32_t);
			rscn_id++;
			if (lpfc_find_vport_by_did(phba, nportid))
				hba_id++;
		}
		if (rscn_id == hba_id) {
			 
			lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
					 "0219 Ignore RSCN "
					 "Data: x%x x%x x%x x%x\n",
					 vport->fc_flag, payload_len,
					 *lp, vport->fc_rscn_id_cnt);
			lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
				"RCV RSCN vport:  did:x%x/ste:x%x flg:x%x",
				ndlp->nlp_DID, vport->port_state,
				ndlp->nlp_flag);

			lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb,
				ndlp, NULL);
			 
			if (vport->fc_flag & FC_DISC_TMO) {
				tmo = ((phba->fc_ratov * 3) + 3);
				mod_timer(&vport->fc_disctmo,
					  jiffies +
					  msecs_to_jiffies(1000 * tmo));
			}
			return 0;
		}
	}

	spin_lock_irq(shost->host_lock);
	if (vport->fc_rscn_flush) {
		 
		vport->fc_flag |= FC_RSCN_DISCOVERY;
		spin_unlock_irq(shost->host_lock);
		 
		lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb, ndlp, NULL);
		return 0;
	}
	 
	vport->fc_rscn_flush = 1;
	spin_unlock_irq(shost->host_lock);
	 
	rscn_cnt = vport->fc_rscn_id_cnt;
	 
	if (vport->fc_flag & (FC_RSCN_MODE | FC_NDISC_ACTIVE)) {
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RSCN defer:  did:x%x/ste:x%x flg:x%x",
			ndlp->nlp_DID, vport->port_state, ndlp->nlp_flag);

		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_RSCN_DEFERRED;

		 
		if (vport->fc_flag & FC_DISC_TMO) {
			tmo = ((phba->fc_ratov * 3) + 3);
			mod_timer(&vport->fc_disctmo,
				  jiffies + msecs_to_jiffies(1000 * tmo));
		}
		if ((rscn_cnt < FC_MAX_HOLD_RSCN) &&
		    !(vport->fc_flag & FC_RSCN_DISCOVERY)) {
			vport->fc_flag |= FC_RSCN_MODE;
			spin_unlock_irq(shost->host_lock);
			if (rscn_cnt) {
				cmd = vport->fc_rscn_id_list[rscn_cnt-1]->virt;
				length = be32_to_cpu(*cmd & ~ELS_CMD_MASK);
			}
			if ((rscn_cnt) &&
			    (payload_len + length <= LPFC_BPL_SIZE)) {
				*cmd &= ELS_CMD_MASK;
				*cmd |= cpu_to_be32(payload_len + length);
				memcpy(((uint8_t *)cmd) + length, lp,
				       payload_len);
			} else {
				vport->fc_rscn_id_list[rscn_cnt] = pcmd;
				vport->fc_rscn_id_cnt++;
				 
				cmdiocb->cmd_dmabuf = NULL;
			}
			 
			lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
					 "0235 Deferred RSCN "
					 "Data: x%x x%x x%x\n",
					 vport->fc_rscn_id_cnt, vport->fc_flag,
					 vport->port_state);
		} else {
			vport->fc_flag |= FC_RSCN_DISCOVERY;
			spin_unlock_irq(shost->host_lock);
			 
			lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
					 "0234 ReDiscovery RSCN "
					 "Data: x%x x%x x%x\n",
					 vport->fc_rscn_id_cnt, vport->fc_flag,
					 vport->port_state);
		}
		 
		vport->fc_rscn_flush = 0;
		 
		lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb, ndlp, NULL);
		 
		lpfc_rscn_recovery_check(vport);
		return 0;
	}
	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
		"RCV RSCN:        did:x%x/ste:x%x flg:x%x",
		ndlp->nlp_DID, vport->port_state, ndlp->nlp_flag);

	spin_lock_irq(shost->host_lock);
	vport->fc_flag |= FC_RSCN_MODE;
	spin_unlock_irq(shost->host_lock);
	vport->fc_rscn_id_list[vport->fc_rscn_id_cnt++] = pcmd;
	 
	vport->fc_rscn_flush = 0;
	 
	cmdiocb->cmd_dmabuf = NULL;
	lpfc_set_disctmo(vport);
	 
	lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb, ndlp, NULL);
	 
	lpfc_rscn_recovery_check(vport);
	return lpfc_els_handle_rscn(vport);
}

 
int
lpfc_els_handle_rscn(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp;
	struct lpfc_hba  *phba = vport->phba;

	 
	if (vport->load_flag & FC_UNLOADING) {
		lpfc_els_flush_rscn(vport);
		return 0;
	}

	 
	lpfc_set_disctmo(vport);

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0215 RSCN processed Data: x%x x%x x%x x%x x%x x%x\n",
			 vport->fc_flag, 0, vport->fc_rscn_id_cnt,
			 vport->port_state, vport->num_disc_nodes,
			 vport->gidft_inp);

	 
	vport->fc_ns_retry = 0;
	vport->num_disc_nodes = 0;

	ndlp = lpfc_findnode_did(vport, NameServer_DID);
	if (ndlp && ndlp->nlp_state == NLP_STE_UNMAPPED_NODE) {
		 
		if (phba->cfg_ns_query == LPFC_NS_QUERY_GID_FT) {
			if (lpfc_issue_gidft(vport) > 0)
				return 1;
		} else if (phba->cfg_ns_query == LPFC_NS_QUERY_GID_PT) {
			if (lpfc_issue_gidpt(vport) > 0)
				return 1;
		} else {
			return 1;
		}
	} else {
		 
		if (ndlp) {
			ndlp->nlp_prev_state = NLP_STE_UNUSED_NODE;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_PLOGI_ISSUE);
		} else {
			ndlp = lpfc_nlp_init(vport, NameServer_DID);
			if (!ndlp) {
				lpfc_els_flush_rscn(vport);
				return 0;
			}
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_PLOGI_ISSUE);
		}
		ndlp->nlp_type |= NLP_FABRIC;
		lpfc_issue_els_plogi(vport, NameServer_DID, 0);
		 
		return 1;
	}

	lpfc_els_flush_rscn(vport);
	return 0;
}

 
static int
lpfc_els_rcv_flogi(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		   struct lpfc_nodelist *ndlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_dmabuf *pcmd = cmdiocb->cmd_dmabuf;
	uint32_t *lp = (uint32_t *) pcmd->virt;
	union lpfc_wqe128 *wqe = &cmdiocb->wqe;
	struct serv_parm *sp;
	LPFC_MBOXQ_t *mbox;
	uint32_t cmd, did;
	int rc;
	uint32_t fc_flag = 0;
	uint32_t port_state = 0;

	 
	phba->link_flag &= ~LS_EXTERNAL_LOOPBACK;

	cmd = *lp++;
	sp = (struct serv_parm *) lp;

	 

	lpfc_set_disctmo(vport);

	if (phba->fc_topology == LPFC_TOPOLOGY_LOOP) {
		 
		did =  bf_get(wqe_els_did, &wqe->xmit_els_rsp.wqe_dest);

		 
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "0113 An FLOGI ELS command x%x was "
				 "received from DID x%x in Loop Mode\n",
				 cmd, did);
		return 1;
	}

	(void) lpfc_check_sparm(vport, ndlp, sp, CLASS3, 1);

	 

	rc = memcmp(&vport->fc_portname, &sp->portName,
		    sizeof(struct lpfc_name));

	if (!rc) {
		if (phba->sli_rev < LPFC_SLI_REV4) {
			mbox = mempool_alloc(phba->mbox_mem_pool,
					     GFP_KERNEL);
			if (!mbox)
				return 1;
			lpfc_linkdown(phba);
			lpfc_init_link(phba, mbox,
				       phba->cfg_topology,
				       phba->cfg_link_speed);
			mbox->u.mb.un.varInitLnk.lipsr_AL_PA = 0;
			mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
			mbox->vport = vport;
			rc = lpfc_sli_issue_mbox(phba, mbox,
						 MBX_NOWAIT);
			lpfc_set_loopback_flag(phba);
			if (rc == MBX_NOT_FINISHED)
				mempool_free(mbox, phba->mbox_mem_pool);
			return 1;
		}

		 
		phba->link_flag |= LS_EXTERNAL_LOOPBACK;

		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS | LOG_LIBDFC,
				 "1119 External Loopback plug detected\n");

		 
		lpfc_els_abort_flogi(phba);
		return 0;

	} else if (rc > 0) {	 
		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_PT2PT_PLOGI;
		spin_unlock_irq(shost->host_lock);

		 
		vport->fc_myDID = PT2PT_LocalID;
	} else {
		vport->fc_myDID = PT2PT_RemoteID;
	}

	 
	spin_lock_irq(shost->host_lock);
	fc_flag = vport->fc_flag;
	port_state = vport->port_state;
	vport->fc_flag |= FC_PT2PT;
	vport->fc_flag &= ~(FC_FABRIC | FC_PUBLIC_LOOP);

	 
	vport->rcv_flogi_cnt++;
	spin_unlock_irq(shost->host_lock);
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "3311 Rcv Flogi PS x%x new PS x%x "
			 "fc_flag x%x new fc_flag x%x\n",
			 port_state, vport->port_state,
			 fc_flag, vport->fc_flag);

	 
	did = vport->fc_myDID;
	vport->fc_myDID = Fabric_DID;

	memcpy(&phba->fc_fabparam, sp, sizeof(struct serv_parm));

	 
	if (!(phba->hba_flag & HBA_FLOGI_ISSUED)) {
		phba->defer_flogi_acc_rx_id = bf_get(wqe_ctxt_tag,
						     &wqe->xmit_els_rsp.wqe_com);
		phba->defer_flogi_acc_ox_id = bf_get(wqe_rcvoxid,
						     &wqe->xmit_els_rsp.wqe_com);

		vport->fc_myDID = did;

		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
				 "3344 Deferring FLOGI ACC: rx_id: x%x,"
				 " ox_id: x%x, hba_flag x%x\n",
				 phba->defer_flogi_acc_rx_id,
				 phba->defer_flogi_acc_ox_id, phba->hba_flag);

		phba->defer_flogi_acc_flag = true;

		return 0;
	}

	 
	lpfc_els_rsp_acc(vport, ELS_CMD_FLOGI, cmdiocb, ndlp, NULL);

	 
	vport->fc_myDID = did;

	return 0;
}

 
static int
lpfc_els_rcv_rnid(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_nodelist *ndlp)
{
	struct lpfc_dmabuf *pcmd;
	uint32_t *lp;
	RNID *rn;
	struct ls_rjt stat;

	pcmd = cmdiocb->cmd_dmabuf;
	lp = (uint32_t *) pcmd->virt;

	lp++;
	rn = (RNID *) lp;

	 

	switch (rn->Format) {
	case 0:
	case RNID_TOPOLOGY_DISC:
		 
		lpfc_els_rsp_rnid_acc(vport, rn->Format, cmdiocb, ndlp);
		break;
	default:
		 
		stat.un.b.lsRjtRsvd0 = 0;
		stat.un.b.lsRjtRsnCode = LSRJT_UNABLE_TPC;
		stat.un.b.lsRjtRsnCodeExp = LSEXP_CANT_GIVE_DATA;
		stat.un.b.vendorUnique = 0;
		lpfc_els_rsp_reject(vport, stat.un.lsRjtError, cmdiocb, ndlp,
			NULL);
	}
	return 0;
}

 
static int
lpfc_els_rcv_echo(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_nodelist *ndlp)
{
	uint8_t *pcmd;

	pcmd = (uint8_t *)cmdiocb->cmd_dmabuf->virt;

	 
	pcmd += sizeof(uint32_t);

	lpfc_els_rsp_echo_acc(vport, pcmd, cmdiocb, ndlp);
	return 0;
}

 
static int
lpfc_els_rcv_lirr(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_nodelist *ndlp)
{
	struct ls_rjt stat;

	 
	stat.un.b.lsRjtRsvd0 = 0;
	stat.un.b.lsRjtRsnCode = LSRJT_UNABLE_TPC;
	stat.un.b.lsRjtRsnCodeExp = LSEXP_CANT_GIVE_DATA;
	stat.un.b.vendorUnique = 0;
	lpfc_els_rsp_reject(vport, stat.un.lsRjtError, cmdiocb, ndlp, NULL);
	return 0;
}

 
static void
lpfc_els_rcv_rrq(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		 struct lpfc_nodelist *ndlp)
{
	lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb, ndlp, NULL);
	if (vport->phba->sli_rev == LPFC_SLI_REV4)
		lpfc_els_clear_rrq(vport, cmdiocb, ndlp);
}

 
static void
lpfc_els_rsp_rls_acc(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	int rc = 0;
	MAILBOX_t *mb;
	IOCB_t *icmd;
	union lpfc_wqe128 *wqe;
	struct RLS_RSP *rls_rsp;
	uint8_t *pcmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_nodelist *ndlp;
	uint16_t oxid;
	uint16_t rxid;
	uint32_t cmdsize;
	u32 ulp_context;

	mb = &pmb->u.mb;

	ndlp = pmb->ctx_ndlp;
	rxid = (uint16_t)((unsigned long)(pmb->ctx_buf) & 0xffff);
	oxid = (uint16_t)(((unsigned long)(pmb->ctx_buf) >> 16) & 0xffff);
	pmb->ctx_buf = NULL;
	pmb->ctx_ndlp = NULL;

	if (mb->mbxStatus) {
		mempool_free(pmb, phba->mbox_mem_pool);
		return;
	}

	cmdsize = sizeof(struct RLS_RSP) + sizeof(uint32_t);
	elsiocb = lpfc_prep_els_iocb(phba->pport, 0, cmdsize,
				     lpfc_max_els_tries, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ACC);

	 
	lpfc_nlp_put(ndlp);

	if (!elsiocb) {
		mempool_free(pmb, phba->mbox_mem_pool);
		return;
	}

	ulp_context = get_job_ulpcontext(phba, elsiocb);
	if (phba->sli_rev == LPFC_SLI_REV4) {
		wqe = &elsiocb->wqe;
		 
		bf_set(wqe_ctxt_tag, &wqe->generic.wqe_com, rxid);
		bf_set(wqe_rcvoxid, &wqe->xmit_els_rsp.wqe_com, oxid);
	} else {
		icmd = &elsiocb->iocb;
		icmd->ulpContext = rxid;
		icmd->unsli3.rcvsli3.ox_id = oxid;
	}

	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;
	*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
	pcmd += sizeof(uint32_t);  
	rls_rsp = (struct RLS_RSP *)pcmd;

	rls_rsp->linkFailureCnt = cpu_to_be32(mb->un.varRdLnk.linkFailureCnt);
	rls_rsp->lossSyncCnt = cpu_to_be32(mb->un.varRdLnk.lossSyncCnt);
	rls_rsp->lossSignalCnt = cpu_to_be32(mb->un.varRdLnk.lossSignalCnt);
	rls_rsp->primSeqErrCnt = cpu_to_be32(mb->un.varRdLnk.primSeqErrCnt);
	rls_rsp->invalidXmitWord = cpu_to_be32(mb->un.varRdLnk.invalidXmitWord);
	rls_rsp->crcCnt = cpu_to_be32(mb->un.varRdLnk.crcCnt);
	mempool_free(pmb, phba->mbox_mem_pool);
	 
	lpfc_printf_vlog(ndlp->vport, KERN_INFO, LOG_ELS,
			 "2874 Xmit ELS RLS ACC response tag x%x xri x%x, "
			 "did x%x, nlp_flag x%x, nlp_state x%x, rpi x%x\n",
			 elsiocb->iotag, ulp_context,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);
	elsiocb->cmd_cmpl = lpfc_cmpl_els_rsp;
	phba->fc_stat.elsXmitACC++;
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		return;
	}

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
	}
	return;
}

 
static int
lpfc_els_rcv_rls(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		 struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba *phba = vport->phba;
	LPFC_MBOXQ_t *mbox;
	struct ls_rjt stat;
	u32 ctx = get_job_ulpcontext(phba, cmdiocb);
	u32 ox_id = get_job_rcvoxid(phba, cmdiocb);

	if ((ndlp->nlp_state != NLP_STE_UNMAPPED_NODE) &&
	    (ndlp->nlp_state != NLP_STE_MAPPED_NODE))
		 
		goto reject_out;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_ATOMIC);
	if (mbox) {
		lpfc_read_lnk_stat(phba, mbox);
		mbox->ctx_buf = (void *)((unsigned long)
					 (ox_id << 16 | ctx));
		mbox->ctx_ndlp = lpfc_nlp_get(ndlp);
		if (!mbox->ctx_ndlp)
			goto node_err;
		mbox->vport = vport;
		mbox->mbox_cmpl = lpfc_els_rsp_rls_acc;
		if (lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT)
			!= MBX_NOT_FINISHED)
			 
			return 0;
		 
		lpfc_nlp_put(ndlp);
node_err:
		mempool_free(mbox, phba->mbox_mem_pool);
	}
reject_out:
	 
	stat.un.b.lsRjtRsvd0 = 0;
	stat.un.b.lsRjtRsnCode = LSRJT_UNABLE_TPC;
	stat.un.b.lsRjtRsnCodeExp = LSEXP_CANT_GIVE_DATA;
	stat.un.b.vendorUnique = 0;
	lpfc_els_rsp_reject(vport, stat.un.lsRjtError, cmdiocb, ndlp, NULL);
	return 0;
}

 
static int
lpfc_els_rcv_rtv(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		 struct lpfc_nodelist *ndlp)
{
	int rc = 0;
	IOCB_t *icmd;
	union lpfc_wqe128 *wqe;
	struct lpfc_hba *phba = vport->phba;
	struct ls_rjt stat;
	struct RTV_RSP *rtv_rsp;
	uint8_t *pcmd;
	struct lpfc_iocbq *elsiocb;
	uint32_t cmdsize;
	u32 ulp_context;

	if ((ndlp->nlp_state != NLP_STE_UNMAPPED_NODE) &&
	    (ndlp->nlp_state != NLP_STE_MAPPED_NODE))
		 
		goto reject_out;

	cmdsize = sizeof(struct RTV_RSP) + sizeof(uint32_t);
	elsiocb = lpfc_prep_els_iocb(phba->pport, 0, cmdsize,
				     lpfc_max_els_tries, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ACC);

	if (!elsiocb)
		return 1;

	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;
	*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
	pcmd += sizeof(uint32_t);  

	ulp_context = get_job_ulpcontext(phba, elsiocb);
	 
	if (phba->sli_rev == LPFC_SLI_REV4) {
		wqe = &elsiocb->wqe;
		bf_set(wqe_ctxt_tag, &wqe->generic.wqe_com,
		       get_job_ulpcontext(phba, cmdiocb));
		bf_set(wqe_rcvoxid, &wqe->xmit_els_rsp.wqe_com,
		       get_job_rcvoxid(phba, cmdiocb));
	} else {
		icmd = &elsiocb->iocb;
		icmd->ulpContext = get_job_ulpcontext(phba, cmdiocb);
		icmd->unsli3.rcvsli3.ox_id = get_job_rcvoxid(phba, cmdiocb);
	}

	rtv_rsp = (struct RTV_RSP *)pcmd;

	 
	rtv_rsp->ratov = cpu_to_be32(phba->fc_ratov * 1000);  
	rtv_rsp->edtov = cpu_to_be32(phba->fc_edtov);
	bf_set(qtov_edtovres, rtv_rsp, phba->fc_edtovResol ? 1 : 0);
	bf_set(qtov_rttov, rtv_rsp, 0);  
	rtv_rsp->qtov = cpu_to_be32(rtv_rsp->qtov);

	 
	lpfc_printf_vlog(ndlp->vport, KERN_INFO, LOG_ELS,
			 "2875 Xmit ELS RTV ACC response tag x%x xri x%x, "
			 "did x%x, nlp_flag x%x, nlp_state x%x, rpi x%x, "
			 "Data: x%x x%x x%x\n",
			 elsiocb->iotag, ulp_context,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi,
			rtv_rsp->ratov, rtv_rsp->edtov, rtv_rsp->qtov);
	elsiocb->cmd_cmpl = lpfc_cmpl_els_rsp;
	phba->fc_stat.elsXmitACC++;
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 0;
	}

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
	}
	return 0;

reject_out:
	 
	stat.un.b.lsRjtRsvd0 = 0;
	stat.un.b.lsRjtRsnCode = LSRJT_UNABLE_TPC;
	stat.un.b.lsRjtRsnCodeExp = LSEXP_CANT_GIVE_DATA;
	stat.un.b.vendorUnique = 0;
	lpfc_els_rsp_reject(vport, stat.un.lsRjtError, cmdiocb, ndlp, NULL);
	return 0;
}

 
static int
lpfc_issue_els_rrq(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
			uint32_t did, struct lpfc_node_rrq *rrq)
{
	struct lpfc_hba  *phba = vport->phba;
	struct RRQ *els_rrq;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int ret;

	if (!ndlp)
		return 1;

	 
	cmdsize = (sizeof(uint32_t) + sizeof(struct RRQ));
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, 0, ndlp, did,
				     ELS_CMD_RRQ);
	if (!elsiocb)
		return 1;

	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;

	 
	*((uint32_t *) (pcmd)) = ELS_CMD_RRQ;
	pcmd += sizeof(uint32_t);
	els_rrq = (struct RRQ *) pcmd;

	bf_set(rrq_oxid, els_rrq, phba->sli4_hba.xri_ids[rrq->xritag]);
	bf_set(rrq_rxid, els_rrq, rrq->rxid);
	bf_set(rrq_did, els_rrq, vport->fc_myDID);
	els_rrq->rrq = cpu_to_be32(els_rrq->rrq);
	els_rrq->rrq_exchg = cpu_to_be32(els_rrq->rrq_exchg);


	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue RRQ:     did:x%x",
		did, rrq->xritag, rrq->rxid);
	elsiocb->context_un.rrq = rrq;
	elsiocb->cmd_cmpl = lpfc_cmpl_els_rrq;

	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp)
		goto io_err;

	ret = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (ret == IOCB_ERROR) {
		lpfc_nlp_put(ndlp);
		goto io_err;
	}
	return 0;

 io_err:
	lpfc_els_free_iocb(phba, elsiocb);
	return 1;
}

 
int
lpfc_send_rrq(struct lpfc_hba *phba, struct lpfc_node_rrq *rrq)
{
	struct lpfc_nodelist *ndlp = lpfc_findnode_did(rrq->vport,
						       rrq->nlp_DID);
	if (!ndlp)
		return 1;

	if (lpfc_test_rrq_active(phba, ndlp, rrq->xritag))
		return lpfc_issue_els_rrq(rrq->vport, ndlp,
					 rrq->nlp_DID, rrq);
	else
		return 1;
}

 
static int
lpfc_els_rsp_rpl_acc(struct lpfc_vport *vport, uint16_t cmdsize,
		     struct lpfc_iocbq *oldiocb, struct lpfc_nodelist *ndlp)
{
	int rc = 0;
	struct lpfc_hba *phba = vport->phba;
	IOCB_t *icmd;
	union lpfc_wqe128 *wqe;
	RPL_RSP rpl_rsp;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	u32 ulp_context;

	elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ACC);

	if (!elsiocb)
		return 1;

	ulp_context = get_job_ulpcontext(phba, elsiocb);
	if (phba->sli_rev == LPFC_SLI_REV4) {
		wqe = &elsiocb->wqe;
		 
		bf_set(wqe_ctxt_tag, &wqe->generic.wqe_com,
		       get_job_ulpcontext(phba, oldiocb));
		bf_set(wqe_rcvoxid, &wqe->xmit_els_rsp.wqe_com,
		       get_job_rcvoxid(phba, oldiocb));
	} else {
		icmd = &elsiocb->iocb;
		icmd->ulpContext = get_job_ulpcontext(phba, oldiocb);
		icmd->unsli3.rcvsli3.ox_id = get_job_rcvoxid(phba, oldiocb);
	}

	pcmd = elsiocb->cmd_dmabuf->virt;
	*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
	pcmd += sizeof(uint16_t);
	*((uint16_t *)(pcmd)) = be16_to_cpu(cmdsize);
	pcmd += sizeof(uint16_t);

	 
	rpl_rsp.listLen = be32_to_cpu(1);
	rpl_rsp.index = 0;
	rpl_rsp.port_num_blk.portNum = 0;
	rpl_rsp.port_num_blk.portID = be32_to_cpu(vport->fc_myDID);
	memcpy(&rpl_rsp.port_num_blk.portName, &vport->fc_portname,
	    sizeof(struct lpfc_name));
	memcpy(pcmd, &rpl_rsp, cmdsize - sizeof(uint32_t));
	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0120 Xmit ELS RPL ACC response tag x%x "
			 "xri x%x, did x%x, nlp_flag x%x, nlp_state x%x, "
			 "rpi x%x\n",
			 elsiocb->iotag, ulp_context,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);
	elsiocb->cmd_cmpl = lpfc_cmpl_els_rsp;
	phba->fc_stat.elsXmitACC++;
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
		return 1;
	}

	return 0;
}

 
static int
lpfc_els_rcv_rpl(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		 struct lpfc_nodelist *ndlp)
{
	struct lpfc_dmabuf *pcmd;
	uint32_t *lp;
	uint32_t maxsize;
	uint16_t cmdsize;
	RPL *rpl;
	struct ls_rjt stat;

	if ((ndlp->nlp_state != NLP_STE_UNMAPPED_NODE) &&
	    (ndlp->nlp_state != NLP_STE_MAPPED_NODE)) {
		 
		stat.un.b.lsRjtRsvd0 = 0;
		stat.un.b.lsRjtRsnCode = LSRJT_UNABLE_TPC;
		stat.un.b.lsRjtRsnCodeExp = LSEXP_CANT_GIVE_DATA;
		stat.un.b.vendorUnique = 0;
		lpfc_els_rsp_reject(vport, stat.un.lsRjtError, cmdiocb, ndlp,
			NULL);
		 
		return 0;
	}

	pcmd = cmdiocb->cmd_dmabuf;
	lp = (uint32_t *) pcmd->virt;
	rpl = (RPL *) (lp + 1);
	maxsize = be32_to_cpu(rpl->maxsize);

	 
	if ((rpl->index == 0) &&
	    ((maxsize == 0) ||
	     ((maxsize * sizeof(uint32_t)) >= sizeof(RPL_RSP)))) {
		cmdsize = sizeof(uint32_t) + sizeof(RPL_RSP);
	} else {
		cmdsize = sizeof(uint32_t) + maxsize * sizeof(uint32_t);
	}
	lpfc_els_rsp_rpl_acc(vport, cmdsize, cmdiocb, ndlp);

	return 0;
}

 
static int
lpfc_els_rcv_farp(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_nodelist *ndlp)
{
	struct lpfc_dmabuf *pcmd;
	uint32_t *lp;
	FARP *fp;
	uint32_t cnt, did;

	did = get_job_els_rsp64_did(vport->phba, cmdiocb);
	pcmd = cmdiocb->cmd_dmabuf;
	lp = (uint32_t *) pcmd->virt;

	lp++;
	fp = (FARP *) lp;
	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0601 FARP-REQ received from DID x%x\n", did);
	 
	if (fp->Mflags & ~(FARP_MATCH_NODE | FARP_MATCH_PORT)) {
		return 0;
	}

	cnt = 0;
	 
	if (fp->Mflags & FARP_MATCH_PORT) {
		if (memcmp(&fp->RportName, &vport->fc_portname,
			   sizeof(struct lpfc_name)) == 0)
			cnt = 1;
	}

	 
	if (fp->Mflags & FARP_MATCH_NODE) {
		if (memcmp(&fp->RnodeName, &vport->fc_nodename,
			   sizeof(struct lpfc_name)) == 0)
			cnt = 1;
	}

	if (cnt) {
		if ((ndlp->nlp_state == NLP_STE_UNMAPPED_NODE) ||
		   (ndlp->nlp_state == NLP_STE_MAPPED_NODE)) {
			 
			if (fp->Rflags & FARP_REQUEST_PLOGI) {
				ndlp->nlp_prev_state = ndlp->nlp_state;
				lpfc_nlp_set_state(vport, ndlp,
						   NLP_STE_PLOGI_ISSUE);
				lpfc_issue_els_plogi(vport, ndlp->nlp_DID, 0);
			}

			 
			if (fp->Rflags & FARP_REQUEST_FARPR)
				lpfc_issue_els_farpr(vport, did, 0);
		}
	}
	return 0;
}

 
static int
lpfc_els_rcv_farpr(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		   struct lpfc_nodelist  *ndlp)
{
	uint32_t did;

	did = get_job_els_rsp64_did(vport->phba, cmdiocb);

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0600 FARP-RSP received from DID x%x\n", did);
	 
	lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb, ndlp, NULL);

	return 0;
}

 
static int
lpfc_els_rcv_fan(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		 struct lpfc_nodelist *fan_ndlp)
{
	struct lpfc_hba *phba = vport->phba;
	uint32_t *lp;
	FAN *fp;

	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS, "0265 FAN received\n");
	lp = (uint32_t *)cmdiocb->cmd_dmabuf->virt;
	fp = (FAN *) ++lp;
	 
	if ((vport == phba->pport) &&
	    (vport->port_state == LPFC_LOCAL_CFG_LINK)) {
		if ((memcmp(&phba->fc_fabparam.nodeName, &fp->FnodeName,
			    sizeof(struct lpfc_name))) ||
		    (memcmp(&phba->fc_fabparam.portName, &fp->FportName,
			    sizeof(struct lpfc_name)))) {
			 
			lpfc_issue_init_vfi(vport);
		} else {
			 
			vport->fc_myDID = vport->fc_prevDID;
			if (phba->sli_rev < LPFC_SLI_REV4)
				lpfc_issue_fabric_reglogin(vport);
			else {
				lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
					"3138 Need register VFI: (x%x/%x)\n",
					vport->fc_prevDID, vport->fc_myDID);
				lpfc_issue_reg_vfi(vport);
			}
		}
	}
	return 0;
}

 
static int
lpfc_els_rcv_edc(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		 struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba  *phba = vport->phba;
	struct fc_els_edc *edc_req;
	struct fc_tlv_desc *tlv;
	uint8_t *payload;
	uint32_t *ptr, dtag;
	const char *dtag_nm;
	int desc_cnt = 0, bytes_remain;
	struct fc_diag_lnkflt_desc *plnkflt;

	payload = cmdiocb->cmd_dmabuf->virt;

	edc_req = (struct fc_els_edc *)payload;
	bytes_remain = be32_to_cpu(edc_req->desc_len);

	ptr = (uint32_t *)payload;
	lpfc_printf_vlog(vport, KERN_INFO,
			 LOG_ELS | LOG_CGN_MGMT | LOG_LDS_EVENT,
			 "3319 Rcv EDC payload len %d: x%x x%x x%x\n",
			 bytes_remain, be32_to_cpu(*ptr),
			 be32_to_cpu(*(ptr + 1)), be32_to_cpu(*(ptr + 2)));

	 
	phba->cgn_reg_signal = EDC_CG_SIG_NOTSUPPORTED;
	phba->cgn_sig_freq = 0;
	phba->cgn_reg_fpin = LPFC_CGN_FPIN_ALARM | LPFC_CGN_FPIN_WARN;

	if (bytes_remain <= 0)
		goto out;

	tlv = edc_req->desc;

	 
	while (bytes_remain) {
		if (bytes_remain < FC_TLV_DESC_HDR_SZ) {
			lpfc_printf_log(phba, KERN_WARNING,
					LOG_ELS | LOG_CGN_MGMT | LOG_LDS_EVENT,
					"6464 Truncated TLV hdr on "
					"Diagnostic descriptor[%d]\n",
					desc_cnt);
			goto out;
		}

		dtag = be32_to_cpu(tlv->desc_tag);
		switch (dtag) {
		case ELS_DTAG_LNK_FAULT_CAP:
			if (bytes_remain < FC_TLV_DESC_SZ_FROM_LENGTH(tlv) ||
			    FC_TLV_DESC_SZ_FROM_LENGTH(tlv) !=
				sizeof(struct fc_diag_lnkflt_desc)) {
				lpfc_printf_log(phba, KERN_WARNING,
					LOG_ELS | LOG_CGN_MGMT | LOG_LDS_EVENT,
					"6465 Truncated Link Fault Diagnostic "
					"descriptor[%d]: %d vs 0x%zx 0x%zx\n",
					desc_cnt, bytes_remain,
					FC_TLV_DESC_SZ_FROM_LENGTH(tlv),
					sizeof(struct fc_diag_lnkflt_desc));
				goto out;
			}
			plnkflt = (struct fc_diag_lnkflt_desc *)tlv;
			lpfc_printf_log(phba, KERN_INFO,
				LOG_ELS | LOG_LDS_EVENT,
				"4626 Link Fault Desc Data: x%08x len x%x "
				"da x%x dd x%x interval x%x\n",
				be32_to_cpu(plnkflt->desc_tag),
				be32_to_cpu(plnkflt->desc_len),
				be32_to_cpu(
					plnkflt->degrade_activate_threshold),
				be32_to_cpu(
					plnkflt->degrade_deactivate_threshold),
				be32_to_cpu(plnkflt->fec_degrade_interval));
			break;
		case ELS_DTAG_CG_SIGNAL_CAP:
			if (bytes_remain < FC_TLV_DESC_SZ_FROM_LENGTH(tlv) ||
			    FC_TLV_DESC_SZ_FROM_LENGTH(tlv) !=
				sizeof(struct fc_diag_cg_sig_desc)) {
				lpfc_printf_log(
					phba, KERN_WARNING, LOG_CGN_MGMT,
					"6466 Truncated cgn signal Diagnostic "
					"descriptor[%d]: %d vs 0x%zx 0x%zx\n",
					desc_cnt, bytes_remain,
					FC_TLV_DESC_SZ_FROM_LENGTH(tlv),
					sizeof(struct fc_diag_cg_sig_desc));
				goto out;
			}

			phba->cgn_reg_fpin = phba->cgn_init_reg_fpin;
			phba->cgn_reg_signal = phba->cgn_init_reg_signal;

			 
			phba->cgn_sig_freq = lpfc_fabric_cgn_frequency;

			lpfc_least_capable_settings(
				phba, (struct fc_diag_cg_sig_desc *)tlv);
			break;
		default:
			dtag_nm = lpfc_get_tlv_dtag_nm(dtag);
			lpfc_printf_log(phba, KERN_WARNING,
					LOG_ELS | LOG_CGN_MGMT | LOG_LDS_EVENT,
					"6467 unknown Diagnostic "
					"Descriptor[%d]: tag x%x (%s)\n",
					desc_cnt, dtag, dtag_nm);
		}
		bytes_remain -= FC_TLV_DESC_SZ_FROM_LENGTH(tlv);
		tlv = fc_tlv_next_desc(tlv);
		desc_cnt++;
	}
out:
	 
	lpfc_issue_els_edc_rsp(vport, cmdiocb, ndlp);

	lpfc_config_cgn_signal(phba);
	return 0;
}

 
void
lpfc_els_timeout(struct timer_list *t)
{
	struct lpfc_vport *vport = from_timer(vport, t, els_tmofunc);
	struct lpfc_hba   *phba = vport->phba;
	uint32_t tmo_posted;
	unsigned long iflag;

	spin_lock_irqsave(&vport->work_port_lock, iflag);
	tmo_posted = vport->work_port_events & WORKER_ELS_TMO;
	if ((!tmo_posted) && (!(vport->load_flag & FC_UNLOADING)))
		vport->work_port_events |= WORKER_ELS_TMO;
	spin_unlock_irqrestore(&vport->work_port_lock, iflag);

	if ((!tmo_posted) && (!(vport->load_flag & FC_UNLOADING)))
		lpfc_worker_wake_up(phba);
	return;
}


 
void
lpfc_els_timeout_handler(struct lpfc_vport *vport)
{
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_sli_ring *pring;
	struct lpfc_iocbq *tmp_iocb, *piocb;
	IOCB_t *cmd = NULL;
	struct lpfc_dmabuf *pcmd;
	uint32_t els_command = 0;
	uint32_t timeout;
	uint32_t remote_ID = 0xffffffff;
	LIST_HEAD(abort_list);
	u32 ulp_command = 0, ulp_context = 0, did = 0, iotag = 0;


	timeout = (uint32_t)(phba->fc_ratov << 1);

	pring = lpfc_phba_elsring(phba);
	if (unlikely(!pring))
		return;

	if (phba->pport->load_flag & FC_UNLOADING)
		return;

	spin_lock_irq(&phba->hbalock);
	if (phba->sli_rev == LPFC_SLI_REV4)
		spin_lock(&pring->ring_lock);

	list_for_each_entry_safe(piocb, tmp_iocb, &pring->txcmplq, list) {
		ulp_command = get_job_cmnd(phba, piocb);
		ulp_context = get_job_ulpcontext(phba, piocb);
		did = get_job_els_rsp64_did(phba, piocb);

		if (phba->sli_rev == LPFC_SLI_REV4) {
			iotag = get_wqe_reqtag(piocb);
		} else {
			cmd = &piocb->iocb;
			iotag = cmd->ulpIoTag;
		}

		if ((piocb->cmd_flag & LPFC_IO_LIBDFC) != 0 ||
		    ulp_command == CMD_ABORT_XRI_CX ||
		    ulp_command == CMD_ABORT_XRI_CN ||
		    ulp_command == CMD_CLOSE_XRI_CN)
			continue;

		if (piocb->vport != vport)
			continue;

		pcmd = piocb->cmd_dmabuf;
		if (pcmd)
			els_command = *(uint32_t *) (pcmd->virt);

		if (els_command == ELS_CMD_FARP ||
		    els_command == ELS_CMD_FARPR ||
		    els_command == ELS_CMD_FDISC)
			continue;

		if (piocb->drvrTimeout > 0) {
			if (piocb->drvrTimeout >= timeout)
				piocb->drvrTimeout -= timeout;
			else
				piocb->drvrTimeout = 0;
			continue;
		}

		remote_ID = 0xffffffff;
		if (ulp_command != CMD_GEN_REQUEST64_CR) {
			remote_ID = did;
		} else {
			struct lpfc_nodelist *ndlp;
			ndlp = __lpfc_findnode_rpi(vport, ulp_context);
			if (ndlp)
				remote_ID = ndlp->nlp_DID;
		}
		list_add_tail(&piocb->dlist, &abort_list);
	}
	if (phba->sli_rev == LPFC_SLI_REV4)
		spin_unlock(&pring->ring_lock);
	spin_unlock_irq(&phba->hbalock);

	list_for_each_entry_safe(piocb, tmp_iocb, &abort_list, dlist) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
			 "0127 ELS timeout Data: x%x x%x x%x "
			 "x%x\n", els_command,
			 remote_ID, ulp_command, iotag);

		spin_lock_irq(&phba->hbalock);
		list_del_init(&piocb->dlist);
		lpfc_sli_issue_abort_iotag(phba, pring, piocb, NULL);
		spin_unlock_irq(&phba->hbalock);
	}

	 
	lpfc_issue_hb_tmo(phba);

	if (!list_empty(&pring->txcmplq))
		if (!(phba->pport->load_flag & FC_UNLOADING))
			mod_timer(&vport->els_tmofunc,
				  jiffies + msecs_to_jiffies(1000 * timeout));
}

 
void
lpfc_els_flush_cmd(struct lpfc_vport *vport)
{
	LIST_HEAD(abort_list);
	LIST_HEAD(cancel_list);
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_sli_ring *pring;
	struct lpfc_iocbq *tmp_iocb, *piocb;
	u32 ulp_command;
	unsigned long iflags = 0;
	bool mbx_tmo_err;

	lpfc_fabric_abort_vport(vport);

	 
	spin_lock_irqsave(&phba->hbalock, iflags);
	pring = lpfc_phba_elsring(phba);

	 
	if (unlikely(!pring)) {
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		return;
	}

	if (phba->sli_rev == LPFC_SLI_REV4)
		spin_lock(&pring->ring_lock);

	mbx_tmo_err = test_bit(MBX_TMO_ERR, &phba->bit_flags);
	 
	list_for_each_entry_safe(piocb, tmp_iocb, &pring->txcmplq, list) {
		if (piocb->cmd_flag & LPFC_IO_LIBDFC && !mbx_tmo_err)
			continue;

		if (piocb->vport != vport)
			continue;

		if (piocb->cmd_flag & LPFC_DRIVER_ABORTED && !mbx_tmo_err)
			continue;

		 
		ulp_command = get_job_cmnd(phba, piocb);
		if (ulp_command == CMD_ELS_REQUEST64_CR) {
			list_add_tail(&piocb->dlist, &abort_list);

			 
			if (phba->link_state == LPFC_LINK_DOWN)
				piocb->cmd_cmpl = lpfc_cmpl_els_link_down;
		} else if (ulp_command == CMD_GEN_REQUEST64_CR ||
			   mbx_tmo_err)
			list_add_tail(&piocb->dlist, &abort_list);
	}

	if (phba->sli_rev == LPFC_SLI_REV4)
		spin_unlock(&pring->ring_lock);
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	 
	list_for_each_entry_safe(piocb, tmp_iocb, &abort_list, dlist) {
		spin_lock_irqsave(&phba->hbalock, iflags);
		list_del_init(&piocb->dlist);
		if (mbx_tmo_err)
			list_move_tail(&piocb->list, &cancel_list);
		else
			lpfc_sli_issue_abort_iotag(phba, pring, piocb, NULL);

		spin_unlock_irqrestore(&phba->hbalock, iflags);
	}
	if (!list_empty(&cancel_list))
		lpfc_sli_cancel_iocbs(phba, &cancel_list, IOSTAT_LOCAL_REJECT,
				      IOERR_SLI_ABORTED);
	else
		 
		lpfc_issue_hb_tmo(phba);

	if (!list_empty(&abort_list))
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "3387 abort list for txq not empty\n");
	INIT_LIST_HEAD(&abort_list);

	spin_lock_irqsave(&phba->hbalock, iflags);
	if (phba->sli_rev == LPFC_SLI_REV4)
		spin_lock(&pring->ring_lock);

	 
	list_for_each_entry_safe(piocb, tmp_iocb, &pring->txq, list) {
		ulp_command = get_job_cmnd(phba, piocb);

		if (piocb->cmd_flag & LPFC_IO_LIBDFC)
			continue;

		 
		if (ulp_command == CMD_QUE_RING_BUF_CN ||
		    ulp_command == CMD_QUE_RING_BUF64_CN ||
		    ulp_command == CMD_CLOSE_XRI_CN ||
		    ulp_command == CMD_ABORT_XRI_CN ||
		    ulp_command == CMD_ABORT_XRI_CX)
			continue;

		if (piocb->vport != vport)
			continue;

		list_del_init(&piocb->list);
		list_add_tail(&piocb->list, &abort_list);
	}

	 
	if (vport == phba->pport) {
		list_for_each_entry_safe(piocb, tmp_iocb,
					 &phba->fabric_iocb_list, list) {
			list_del_init(&piocb->list);
			list_add_tail(&piocb->list, &abort_list);
		}
	}

	if (phba->sli_rev == LPFC_SLI_REV4)
		spin_unlock(&pring->ring_lock);
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	 
	lpfc_sli_cancel_iocbs(phba, &abort_list,
			      IOSTAT_LOCAL_REJECT, IOERR_SLI_ABORTED);

	return;
}

 
void
lpfc_els_flush_all_cmd(struct lpfc_hba  *phba)
{
	struct lpfc_vport *vport;

	spin_lock_irq(&phba->port_list_lock);
	list_for_each_entry(vport, &phba->port_list, listentry)
		lpfc_els_flush_cmd(vport);
	spin_unlock_irq(&phba->port_list_lock);

	return;
}

 
void
lpfc_send_els_failure_event(struct lpfc_hba *phba,
			struct lpfc_iocbq *cmdiocbp,
			struct lpfc_iocbq *rspiocbp)
{
	struct lpfc_vport *vport = cmdiocbp->vport;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_lsrjt_event lsrjt_event;
	struct lpfc_fabric_event_header fabric_event;
	struct ls_rjt stat;
	struct lpfc_nodelist *ndlp;
	uint32_t *pcmd;
	u32 ulp_status, ulp_word4;

	ndlp = cmdiocbp->ndlp;
	if (!ndlp)
		return;

	ulp_status = get_job_ulpstatus(phba, rspiocbp);
	ulp_word4 = get_job_word4(phba, rspiocbp);

	if (ulp_status == IOSTAT_LS_RJT) {
		lsrjt_event.header.event_type = FC_REG_ELS_EVENT;
		lsrjt_event.header.subcategory = LPFC_EVENT_LSRJT_RCV;
		memcpy(lsrjt_event.header.wwpn, &ndlp->nlp_portname,
			sizeof(struct lpfc_name));
		memcpy(lsrjt_event.header.wwnn, &ndlp->nlp_nodename,
			sizeof(struct lpfc_name));
		pcmd = (uint32_t *)cmdiocbp->cmd_dmabuf->virt;
		lsrjt_event.command = (pcmd != NULL) ? *pcmd : 0;
		stat.un.ls_rjt_error_be = cpu_to_be32(ulp_word4);
		lsrjt_event.reason_code = stat.un.b.lsRjtRsnCode;
		lsrjt_event.explanation = stat.un.b.lsRjtRsnCodeExp;
		fc_host_post_vendor_event(shost,
			fc_get_event_number(),
			sizeof(lsrjt_event),
			(char *)&lsrjt_event,
			LPFC_NL_VENDOR_ID);
		return;
	}
	if (ulp_status == IOSTAT_NPORT_BSY ||
	    ulp_status == IOSTAT_FABRIC_BSY) {
		fabric_event.event_type = FC_REG_FABRIC_EVENT;
		if (ulp_status == IOSTAT_NPORT_BSY)
			fabric_event.subcategory = LPFC_EVENT_PORT_BUSY;
		else
			fabric_event.subcategory = LPFC_EVENT_FABRIC_BUSY;
		memcpy(fabric_event.wwpn, &ndlp->nlp_portname,
			sizeof(struct lpfc_name));
		memcpy(fabric_event.wwnn, &ndlp->nlp_nodename,
			sizeof(struct lpfc_name));
		fc_host_post_vendor_event(shost,
			fc_get_event_number(),
			sizeof(fabric_event),
			(char *)&fabric_event,
			LPFC_NL_VENDOR_ID);
		return;
	}

}

 
static void
lpfc_send_els_event(struct lpfc_vport *vport,
		    struct lpfc_nodelist *ndlp,
		    uint32_t *payload)
{
	struct lpfc_els_event_header *els_data = NULL;
	struct lpfc_logo_event *logo_data = NULL;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	if (*payload == ELS_CMD_LOGO) {
		logo_data = kmalloc(sizeof(struct lpfc_logo_event), GFP_KERNEL);
		if (!logo_data) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				"0148 Failed to allocate memory "
				"for LOGO event\n");
			return;
		}
		els_data = &logo_data->header;
	} else {
		els_data = kmalloc(sizeof(struct lpfc_els_event_header),
			GFP_KERNEL);
		if (!els_data) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				"0149 Failed to allocate memory "
				"for ELS event\n");
			return;
		}
	}
	els_data->event_type = FC_REG_ELS_EVENT;
	switch (*payload) {
	case ELS_CMD_PLOGI:
		els_data->subcategory = LPFC_EVENT_PLOGI_RCV;
		break;
	case ELS_CMD_PRLO:
		els_data->subcategory = LPFC_EVENT_PRLO_RCV;
		break;
	case ELS_CMD_ADISC:
		els_data->subcategory = LPFC_EVENT_ADISC_RCV;
		break;
	case ELS_CMD_LOGO:
		els_data->subcategory = LPFC_EVENT_LOGO_RCV;
		 
		memcpy(logo_data->logo_wwpn, &payload[2],
			sizeof(struct lpfc_name));
		break;
	default:
		kfree(els_data);
		return;
	}
	memcpy(els_data->wwpn, &ndlp->nlp_portname, sizeof(struct lpfc_name));
	memcpy(els_data->wwnn, &ndlp->nlp_nodename, sizeof(struct lpfc_name));
	if (*payload == ELS_CMD_LOGO) {
		fc_host_post_vendor_event(shost,
			fc_get_event_number(),
			sizeof(struct lpfc_logo_event),
			(char *)logo_data,
			LPFC_NL_VENDOR_ID);
		kfree(logo_data);
	} else {
		fc_host_post_vendor_event(shost,
			fc_get_event_number(),
			sizeof(struct lpfc_els_event_header),
			(char *)els_data,
			LPFC_NL_VENDOR_ID);
		kfree(els_data);
	}

	return;
}


DECLARE_ENUM2STR_LOOKUP(lpfc_get_fpin_li_event_nm, fc_fpin_li_event_types,
			FC_FPIN_LI_EVT_TYPES_INIT);

DECLARE_ENUM2STR_LOOKUP(lpfc_get_fpin_deli_event_nm, fc_fpin_deli_event_types,
			FC_FPIN_DELI_EVT_TYPES_INIT);

DECLARE_ENUM2STR_LOOKUP(lpfc_get_fpin_congn_event_nm, fc_fpin_congn_event_types,
			FC_FPIN_CONGN_EVT_TYPES_INIT);

DECLARE_ENUM2STR_LOOKUP(lpfc_get_fpin_congn_severity_nm,
			fc_fpin_congn_severity_types,
			FC_FPIN_CONGN_SEVERITY_INIT);


 
static void
lpfc_display_fpin_wwpn(struct lpfc_hba *phba, __be64 *wwnlist, u32 cnt)
{
	char buf[LPFC_FPIN_WWPN_LINE_SZ];
	__be64 wwn;
	u64 wwpn;
	int i, len;
	int line = 0;
	int wcnt = 0;
	bool endit = false;

	len = scnprintf(buf, LPFC_FPIN_WWPN_LINE_SZ, "Accessible WWPNs:");
	for (i = 0; i < cnt; i++) {
		 
		if (i == (cnt - 1))
			endit = true;

		 
		wwn = *wwnlist++;
		wwpn = be64_to_cpu(wwn);
		len += scnprintf(buf + len, LPFC_FPIN_WWPN_LINE_SZ - len,
				 " %016llx", wwpn);

		 
		wcnt++;
		if (wcnt == LPFC_FPIN_WWPN_LINE_CNT || endit) {
			buf[len] = 0;
			lpfc_printf_log(phba, KERN_INFO, LOG_ELS,
					"4686 %s\n", buf);

			 
			if (endit)
				return;

			 
			line++;
			if (line == LPFC_FPIN_WWPN_NUM_LINE) {
				lpfc_printf_log(phba, KERN_INFO, LOG_ELS,
						"4687 %d WWPNs Truncated\n",
						cnt - i - 1);
				return;
			}

			 
			wcnt = 0;
			len = scnprintf(buf, LPFC_FPIN_WWPN_LINE_SZ,
					"Additional WWPNs:");
		}
	}
}

 
static void
lpfc_els_rcv_fpin_li(struct lpfc_hba *phba, struct fc_tlv_desc *tlv)
{
	struct fc_fn_li_desc *li = (struct fc_fn_li_desc *)tlv;
	const char *li_evt_str;
	u32 li_evt, cnt;

	li_evt = be16_to_cpu(li->event_type);
	li_evt_str = lpfc_get_fpin_li_event_nm(li_evt);
	cnt = be32_to_cpu(li->pname_count);

	lpfc_printf_log(phba, KERN_INFO, LOG_ELS,
			"4680 FPIN Link Integrity %s (x%x) "
			"Detecting PN x%016llx Attached PN x%016llx "
			"Duration %d mSecs Count %d Port Cnt %d\n",
			li_evt_str, li_evt,
			be64_to_cpu(li->detecting_wwpn),
			be64_to_cpu(li->attached_wwpn),
			be32_to_cpu(li->event_threshold),
			be32_to_cpu(li->event_count), cnt);

	lpfc_display_fpin_wwpn(phba, (__be64 *)&li->pname_list, cnt);
}

 
static void
lpfc_els_rcv_fpin_del(struct lpfc_hba *phba, struct fc_tlv_desc *tlv)
{
	struct fc_fn_deli_desc *del = (struct fc_fn_deli_desc *)tlv;
	const char *del_rsn_str;
	u32 del_rsn;
	__be32 *frame;

	del_rsn = be16_to_cpu(del->deli_reason_code);
	del_rsn_str = lpfc_get_fpin_deli_event_nm(del_rsn);

	 
	frame = (__be32 *)(del + 1);

	lpfc_printf_log(phba, KERN_INFO, LOG_ELS,
			"4681 FPIN Delivery %s (x%x) "
			"Detecting PN x%016llx Attached PN x%016llx "
			"DiscHdr0  x%08x "
			"DiscHdr1 x%08x DiscHdr2 x%08x DiscHdr3 x%08x "
			"DiscHdr4 x%08x DiscHdr5 x%08x\n",
			del_rsn_str, del_rsn,
			be64_to_cpu(del->detecting_wwpn),
			be64_to_cpu(del->attached_wwpn),
			be32_to_cpu(frame[0]),
			be32_to_cpu(frame[1]),
			be32_to_cpu(frame[2]),
			be32_to_cpu(frame[3]),
			be32_to_cpu(frame[4]),
			be32_to_cpu(frame[5]));
}

 
static void
lpfc_els_rcv_fpin_peer_cgn(struct lpfc_hba *phba, struct fc_tlv_desc *tlv)
{
	struct fc_fn_peer_congn_desc *pc = (struct fc_fn_peer_congn_desc *)tlv;
	const char *pc_evt_str;
	u32 pc_evt, cnt;

	pc_evt = be16_to_cpu(pc->event_type);
	pc_evt_str = lpfc_get_fpin_congn_event_nm(pc_evt);
	cnt = be32_to_cpu(pc->pname_count);

	lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT | LOG_ELS,
			"4684 FPIN Peer Congestion %s (x%x) "
			"Duration %d mSecs "
			"Detecting PN x%016llx Attached PN x%016llx "
			"Impacted Port Cnt %d\n",
			pc_evt_str, pc_evt,
			be32_to_cpu(pc->event_period),
			be64_to_cpu(pc->detecting_wwpn),
			be64_to_cpu(pc->attached_wwpn),
			cnt);

	lpfc_display_fpin_wwpn(phba, (__be64 *)&pc->pname_list, cnt);
}

 
static int
lpfc_els_rcv_fpin_cgn(struct lpfc_hba *phba, struct fc_tlv_desc *tlv)
{
	struct lpfc_cgn_info *cp;
	struct fc_fn_congn_desc *cgn = (struct fc_fn_congn_desc *)tlv;
	const char *cgn_evt_str;
	u32 cgn_evt;
	const char *cgn_sev_str;
	u32 cgn_sev;
	uint16_t value;
	u32 crc;
	bool nm_log = false;
	int rc = 1;

	cgn_evt = be16_to_cpu(cgn->event_type);
	cgn_evt_str = lpfc_get_fpin_congn_event_nm(cgn_evt);
	cgn_sev = cgn->severity;
	cgn_sev_str = lpfc_get_fpin_congn_severity_nm(cgn_sev);

	 
	switch (cgn_evt) {
	case FPIN_CONGN_LOST_CREDIT:
		nm_log = true;
		break;
	case FPIN_CONGN_CREDIT_STALL:
		nm_log = true;
		fallthrough;
	case FPIN_CONGN_OVERSUBSCRIPTION:
		if (cgn_evt == FPIN_CONGN_OVERSUBSCRIPTION)
			nm_log = false;
		switch (cgn_sev) {
		case FPIN_CONGN_SEVERITY_ERROR:
			 
			if (phba->cmf_active_mode != LPFC_CFG_OFF) {
				if (phba->cgn_reg_fpin & LPFC_CGN_FPIN_ALARM) {
					 
					atomic_inc(&phba->cgn_sync_alarm_cnt);
				}
				 
				atomic_inc(&phba->cgn_fabric_alarm_cnt);
				goto cleanup;
			}
			break;
		case FPIN_CONGN_SEVERITY_WARNING:
			 
			if (phba->cmf_active_mode != LPFC_CFG_OFF) {
				if (phba->cgn_reg_fpin & LPFC_CGN_FPIN_WARN) {
					 
					atomic_inc(&phba->cgn_sync_warn_cnt);
				}
				 
				atomic_inc(&phba->cgn_fabric_warn_cnt);
cleanup:
				 
				phba->cgn_fpin_frequency =
					be32_to_cpu(cgn->event_period);
				value = phba->cgn_fpin_frequency;
				if (phba->cgn_i) {
					cp = (struct lpfc_cgn_info *)
						phba->cgn_i->virt;
					cp->cgn_alarm_freq =
						cpu_to_le16(value);
					cp->cgn_warn_freq =
						cpu_to_le16(value);
					crc = lpfc_cgn_calc_crc32
						(cp,
						LPFC_CGN_INFO_SZ,
						LPFC_CGN_CRC32_SEED);
					cp->cgn_info_crc = cpu_to_le32(crc);
				}

				 
				rc = 0;
			}
			break;
		}
		break;
	}

	 
	lpfc_printf_log(phba, (nm_log ? KERN_WARNING : KERN_INFO),
			LOG_CGN_MGMT | LOG_ELS,
			"4683 FPIN CONGESTION %s type %s (x%x) Event "
			"Duration %d mSecs\n",
			cgn_sev_str, cgn_evt_str, cgn_evt,
			be32_to_cpu(cgn->event_period));
	return rc;
}

void
lpfc_els_rcv_fpin(struct lpfc_vport *vport, void *p, u32 fpin_length)
{
	struct lpfc_hba *phba = vport->phba;
	struct fc_els_fpin *fpin = (struct fc_els_fpin *)p;
	struct fc_tlv_desc *tlv, *first_tlv, *current_tlv;
	const char *dtag_nm;
	int desc_cnt = 0, bytes_remain, cnt;
	u32 dtag, deliver = 0;
	int len;

	 
	if (vport->port_state < LPFC_DISC_AUTH)
		return;

	 
	if (fpin_length < sizeof(struct fc_els_fpin))
		return;

	 
	len = be32_to_cpu(fpin->desc_len);
	if (fpin_length < len + sizeof(struct fc_els_fpin)) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_CGN_MGMT,
				"4671 Bad ELS FPIN length %d: %d\n",
				len, fpin_length);
		return;
	}

	tlv = (struct fc_tlv_desc *)&fpin->fpin_desc[0];
	first_tlv = tlv;
	bytes_remain = fpin_length - offsetof(struct fc_els_fpin, fpin_desc);
	bytes_remain = min_t(u32, bytes_remain, be32_to_cpu(fpin->desc_len));

	 
	while (bytes_remain >= FC_TLV_DESC_HDR_SZ &&
	       bytes_remain >= FC_TLV_DESC_SZ_FROM_LENGTH(tlv)) {
		dtag = be32_to_cpu(tlv->desc_tag);
		switch (dtag) {
		case ELS_DTAG_LNK_INTEGRITY:
			lpfc_els_rcv_fpin_li(phba, tlv);
			deliver = 1;
			break;
		case ELS_DTAG_DELIVERY:
			lpfc_els_rcv_fpin_del(phba, tlv);
			deliver = 1;
			break;
		case ELS_DTAG_PEER_CONGEST:
			lpfc_els_rcv_fpin_peer_cgn(phba, tlv);
			deliver = 1;
			break;
		case ELS_DTAG_CONGESTION:
			deliver = lpfc_els_rcv_fpin_cgn(phba, tlv);
			break;
		default:
			dtag_nm = lpfc_get_tlv_dtag_nm(dtag);
			lpfc_printf_log(phba, KERN_WARNING, LOG_CGN_MGMT,
					"4678 unknown FPIN descriptor[%d]: "
					"tag x%x (%s)\n",
					desc_cnt, dtag, dtag_nm);

			 
			return;
		}
		lpfc_cgn_update_stat(phba, dtag);
		cnt = be32_to_cpu(tlv->desc_len);

		 
		len -= (cnt + sizeof(struct fc_tlv_desc));
		if (len < 0) {
			dtag_nm = lpfc_get_tlv_dtag_nm(dtag);
			lpfc_printf_log(phba, KERN_WARNING, LOG_CGN_MGMT,
					"4672 Bad FPIN descriptor TLV length "
					"%d: %d %d %s\n",
					cnt, len, fpin_length, dtag_nm);
			return;
		}

		current_tlv = tlv;
		bytes_remain -= FC_TLV_DESC_SZ_FROM_LENGTH(tlv);
		tlv = fc_tlv_next_desc(tlv);

		 
		if (desc_cnt)
			memcpy(first_tlv, current_tlv,
			       (cnt + sizeof(struct fc_els_fpin)));

		 
		fpin_length = cnt + sizeof(struct fc_els_fpin);
		fpin->desc_len = cpu_to_be32(fpin_length);
		fpin_length += sizeof(struct fc_els_fpin);  

		 
		if (deliver)
			fc_host_fpin_rcv(lpfc_shost_from_vport(vport),
					 fpin_length, (char *)fpin, 0);
		desc_cnt++;
	}
}

 
static void
lpfc_els_unsol_buffer(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
		      struct lpfc_vport *vport, struct lpfc_iocbq *elsiocb)
{
	struct lpfc_nodelist *ndlp;
	struct ls_rjt stat;
	u32 *payload, payload_len;
	u32 cmd = 0, did = 0, newnode, status = 0;
	uint8_t rjt_exp, rjt_err = 0, init_link = 0;
	struct lpfc_wcqe_complete *wcqe_cmpl = NULL;
	LPFC_MBOXQ_t *mbox;

	if (!vport || !elsiocb->cmd_dmabuf)
		goto dropit;

	newnode = 0;
	wcqe_cmpl = &elsiocb->wcqe_cmpl;
	payload = elsiocb->cmd_dmabuf->virt;
	if (phba->sli_rev == LPFC_SLI_REV4)
		payload_len = wcqe_cmpl->total_data_placed;
	else
		payload_len = elsiocb->iocb.unsli3.rcvsli3.acc_len;
	status = get_job_ulpstatus(phba, elsiocb);
	cmd = *payload;
	if ((phba->sli3_options & LPFC_SLI3_HBQ_ENABLED) == 0)
		lpfc_sli3_post_buffer(phba, pring, 1);

	did = get_job_els_rsp64_did(phba, elsiocb);
	if (status) {
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV Unsol ELS:  status:x%x/x%x did:x%x",
			status, get_job_word4(phba, elsiocb), did);
		goto dropit;
	}

	 
	if (lpfc_els_chk_latt(vport))
		goto dropit;

	 
	if (vport->load_flag & FC_UNLOADING)
		goto dropit;

	 
	if ((vport->fc_flag & FC_DISC_DELAYED) &&
			(cmd != ELS_CMD_PLOGI))
		goto dropit;

	ndlp = lpfc_findnode_did(vport, did);
	if (!ndlp) {
		 
		ndlp = lpfc_nlp_init(vport, did);
		if (!ndlp)
			goto dropit;
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		newnode = 1;
		if ((did & Fabric_DID_MASK) == Fabric_DID_MASK)
			ndlp->nlp_type |= NLP_FABRIC;
	} else if (ndlp->nlp_state == NLP_STE_UNUSED_NODE) {
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		newnode = 1;
	}

	phba->fc_stat.elsRcvFrame++;

	 
	spin_lock_irq(&ndlp->lock);
	if (ndlp->nlp_flag & NLP_IN_DEV_LOSS) {
		spin_unlock_irq(&ndlp->lock);
		if (newnode)
			lpfc_nlp_put(ndlp);
		goto dropit;
	}
	spin_unlock_irq(&ndlp->lock);

	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp)
		goto dropit;
	elsiocb->vport = vport;

	if ((cmd & ELS_CMD_MASK) == ELS_CMD_RSCN) {
		cmd &= ELS_CMD_MASK;
	}
	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0112 ELS command x%x received from NPORT x%x "
			 "refcnt %d Data: x%x x%x x%x x%x\n",
			 cmd, did, kref_read(&ndlp->kref), vport->port_state,
			 vport->fc_flag, vport->fc_myDID, vport->fc_prevDID);

	 
	if ((vport->port_state < LPFC_FABRIC_CFG_LINK) &&
	    (cmd != ELS_CMD_FLOGI) &&
	    !((cmd == ELS_CMD_PLOGI) && (vport->fc_flag & FC_PT2PT))) {
		rjt_err = LSRJT_LOGICAL_BSY;
		rjt_exp = LSEXP_NOTHING_MORE;
		goto lsrjt;
	}

	switch (cmd) {
	case ELS_CMD_PLOGI:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV PLOGI:       did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvPLOGI++;
		ndlp = lpfc_plogi_confirm_nport(phba, payload, ndlp);
		if (phba->sli_rev == LPFC_SLI_REV4 &&
		    (phba->pport->fc_flag & FC_PT2PT)) {
			vport->fc_prevDID = vport->fc_myDID;
			 
			vport->fc_myDID =
				bf_get(els_rsp64_sid,
				       &elsiocb->wqe.xmit_els_rsp);
			lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
					 "3312 Remote port assigned DID x%x "
					 "%x\n", vport->fc_myDID,
					 vport->fc_prevDID);
		}

		lpfc_send_els_event(vport, ndlp, payload);

		 
		if (vport->fc_flag & FC_DISC_DELAYED) {
			rjt_err = LSRJT_UNABLE_TPC;
			rjt_exp = LSEXP_NOTHING_MORE;
			break;
		}

		if (vport->port_state < LPFC_DISC_AUTH) {
			if (!(phba->pport->fc_flag & FC_PT2PT) ||
				(phba->pport->fc_flag & FC_PT2PT_PLOGI)) {
				rjt_err = LSRJT_UNABLE_TPC;
				rjt_exp = LSEXP_NOTHING_MORE;
				break;
			}
		}

		spin_lock_irq(&ndlp->lock);
		ndlp->nlp_flag &= ~NLP_TARGET_REMOVE;
		spin_unlock_irq(&ndlp->lock);

		lpfc_disc_state_machine(vport, ndlp, elsiocb,
					NLP_EVT_RCV_PLOGI);

		break;
	case ELS_CMD_FLOGI:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV FLOGI:       did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvFLOGI++;

		 
		if (vport->port_state >= LPFC_LOCAL_CFG_LINK &&
		    vport->fc_flag & FC_PT2PT &&
		    vport->rcv_flogi_cnt >= 1) {
			rjt_err = LSRJT_LOGICAL_BSY;
			rjt_exp = LSEXP_NOTHING_MORE;
			init_link++;
			goto lsrjt;
		}

		lpfc_els_rcv_flogi(vport, elsiocb, ndlp);
		 
		if (phba->defer_flogi_acc_flag)
			break;
		if (newnode)
			lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RM);
		break;
	case ELS_CMD_LOGO:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV LOGO:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvLOGO++;
		lpfc_send_els_event(vport, ndlp, payload);
		if (vport->port_state < LPFC_DISC_AUTH) {
			rjt_err = LSRJT_UNABLE_TPC;
			rjt_exp = LSEXP_NOTHING_MORE;
			break;
		}
		lpfc_disc_state_machine(vport, ndlp, elsiocb, NLP_EVT_RCV_LOGO);
		if (newnode)
			lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RM);
		break;
	case ELS_CMD_PRLO:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV PRLO:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvPRLO++;
		lpfc_send_els_event(vport, ndlp, payload);
		if (vport->port_state < LPFC_DISC_AUTH) {
			rjt_err = LSRJT_UNABLE_TPC;
			rjt_exp = LSEXP_NOTHING_MORE;
			break;
		}
		lpfc_disc_state_machine(vport, ndlp, elsiocb, NLP_EVT_RCV_PRLO);
		break;
	case ELS_CMD_LCB:
		phba->fc_stat.elsRcvLCB++;
		lpfc_els_rcv_lcb(vport, elsiocb, ndlp);
		break;
	case ELS_CMD_RDP:
		phba->fc_stat.elsRcvRDP++;
		lpfc_els_rcv_rdp(vport, elsiocb, ndlp);
		break;
	case ELS_CMD_RSCN:
		phba->fc_stat.elsRcvRSCN++;
		lpfc_els_rcv_rscn(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RM);
		break;
	case ELS_CMD_ADISC:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV ADISC:       did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		lpfc_send_els_event(vport, ndlp, payload);
		phba->fc_stat.elsRcvADISC++;
		if (vport->port_state < LPFC_DISC_AUTH) {
			rjt_err = LSRJT_UNABLE_TPC;
			rjt_exp = LSEXP_NOTHING_MORE;
			break;
		}
		lpfc_disc_state_machine(vport, ndlp, elsiocb,
					NLP_EVT_RCV_ADISC);
		break;
	case ELS_CMD_PDISC:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV PDISC:       did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvPDISC++;
		if (vport->port_state < LPFC_DISC_AUTH) {
			rjt_err = LSRJT_UNABLE_TPC;
			rjt_exp = LSEXP_NOTHING_MORE;
			break;
		}
		lpfc_disc_state_machine(vport, ndlp, elsiocb,
					NLP_EVT_RCV_PDISC);
		break;
	case ELS_CMD_FARPR:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV FARPR:       did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvFARPR++;
		lpfc_els_rcv_farpr(vport, elsiocb, ndlp);
		break;
	case ELS_CMD_FARP:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV FARP:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvFARP++;
		lpfc_els_rcv_farp(vport, elsiocb, ndlp);
		break;
	case ELS_CMD_FAN:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV FAN:         did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvFAN++;
		lpfc_els_rcv_fan(vport, elsiocb, ndlp);
		break;
	case ELS_CMD_PRLI:
	case ELS_CMD_NVMEPRLI:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV PRLI:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvPRLI++;
		if ((vport->port_state < LPFC_DISC_AUTH) &&
		    (vport->fc_flag & FC_FABRIC)) {
			rjt_err = LSRJT_UNABLE_TPC;
			rjt_exp = LSEXP_NOTHING_MORE;
			break;
		}
		lpfc_disc_state_machine(vport, ndlp, elsiocb, NLP_EVT_RCV_PRLI);
		break;
	case ELS_CMD_LIRR:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV LIRR:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvLIRR++;
		lpfc_els_rcv_lirr(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RM);
		break;
	case ELS_CMD_RLS:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RLS:         did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvRLS++;
		lpfc_els_rcv_rls(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RM);
		break;
	case ELS_CMD_RPL:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RPL:         did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvRPL++;
		lpfc_els_rcv_rpl(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RM);
		break;
	case ELS_CMD_RNID:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RNID:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvRNID++;
		lpfc_els_rcv_rnid(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RM);
		break;
	case ELS_CMD_RTV:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RTV:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);
		phba->fc_stat.elsRcvRTV++;
		lpfc_els_rcv_rtv(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RM);
		break;
	case ELS_CMD_RRQ:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RRQ:         did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvRRQ++;
		lpfc_els_rcv_rrq(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RM);
		break;
	case ELS_CMD_ECHO:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV ECHO:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvECHO++;
		lpfc_els_rcv_echo(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RM);
		break;
	case ELS_CMD_REC:
		 
		rjt_err = LSRJT_UNABLE_TPC;
		rjt_exp = LSEXP_INVALID_OX_RX;
		break;
	case ELS_CMD_FPIN:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
				      "RCV FPIN:       did:x%x/ste:x%x flg:x%x",
				      did, vport->port_state, ndlp->nlp_flag);

		lpfc_els_rcv_fpin(vport, (struct fc_els_fpin *)payload,
				  payload_len);

		 
		break;
	case ELS_CMD_EDC:
		lpfc_els_rcv_edc(vport, elsiocb, ndlp);
		break;
	case ELS_CMD_RDF:
		phba->fc_stat.elsRcvRDF++;
		 
		if (did != Fabric_Cntl_DID) {
			lpfc_printf_vlog(vport, KERN_WARNING, LOG_ELS,
					 "1115 Received RDF from invalid DID "
					 "x%x\n", did);
			rjt_err = LSRJT_PROTOCOL_ERR;
			rjt_exp = LSEXP_NOTHING_MORE;
			goto lsrjt;
		}

		lpfc_els_rcv_rdf(vport, elsiocb, ndlp);
		break;
	default:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV ELS cmd:     cmd:x%x did:x%x/ste:x%x",
			cmd, did, vport->port_state);

		 
		rjt_err = LSRJT_CMD_UNSUPPORTED;
		rjt_exp = LSEXP_NOTHING_MORE;

		 
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "0115 Unknown ELS command x%x "
				 "received from NPORT x%x\n", cmd, did);
		if (newnode)
			lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RM);
		break;
	}

lsrjt:
	 
	if (rjt_err) {
		memset(&stat, 0, sizeof(stat));
		stat.un.b.lsRjtRsnCode = rjt_err;
		stat.un.b.lsRjtRsnCodeExp = rjt_exp;
		lpfc_els_rsp_reject(vport, stat.un.lsRjtError, elsiocb, ndlp,
				    NULL);
		 
		if (newnode)
			lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RM);
	}

	 
	lpfc_nlp_put(elsiocb->ndlp);
	elsiocb->ndlp = NULL;

	 
	if (init_link) {
		mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
		if (!mbox)
			return;
		lpfc_linkdown(phba);
		lpfc_init_link(phba, mbox,
			       phba->cfg_topology,
			       phba->cfg_link_speed);
		mbox->u.mb.un.varInitLnk.lipsr_AL_PA = 0;
		mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		mbox->vport = vport;
		if (lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT) ==
		    MBX_NOT_FINISHED)
			mempool_free(mbox, phba->mbox_mem_pool);
	}

	return;

dropit:
	if (vport && !(vport->load_flag & FC_UNLOADING))
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
			"0111 Dropping received ELS cmd "
			"Data: x%x x%x x%x x%x\n",
			cmd, status, get_job_word4(phba, elsiocb), did);

	phba->fc_stat.elsRcvDrop++;
}

 
void
lpfc_els_unsol_event(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
		     struct lpfc_iocbq *elsiocb)
{
	struct lpfc_vport *vport = elsiocb->vport;
	u32 ulp_command, status, parameter, bde_count = 0;
	IOCB_t *icmd;
	struct lpfc_wcqe_complete *wcqe_cmpl = NULL;
	struct lpfc_dmabuf *bdeBuf1 = elsiocb->cmd_dmabuf;
	struct lpfc_dmabuf *bdeBuf2 = elsiocb->bpl_dmabuf;
	dma_addr_t paddr;

	elsiocb->cmd_dmabuf = NULL;
	elsiocb->rsp_dmabuf = NULL;
	elsiocb->bpl_dmabuf = NULL;

	wcqe_cmpl = &elsiocb->wcqe_cmpl;
	ulp_command = get_job_cmnd(phba, elsiocb);
	status = get_job_ulpstatus(phba, elsiocb);
	parameter = get_job_word4(phba, elsiocb);
	if (phba->sli_rev == LPFC_SLI_REV4)
		bde_count = wcqe_cmpl->word3;
	else
		bde_count = elsiocb->iocb.ulpBdeCount;

	if (status == IOSTAT_NEED_BUFFER) {
		lpfc_sli_hbqbuf_add_hbqs(phba, LPFC_ELS_HBQ);
	} else if (status == IOSTAT_LOCAL_REJECT &&
		   (parameter & IOERR_PARAM_MASK) ==
		   IOERR_RCV_BUFFER_WAITING) {
		phba->fc_stat.NoRcvBuf++;
		 
		if (!(phba->sli3_options & LPFC_SLI3_HBQ_ENABLED))
			lpfc_sli3_post_buffer(phba, pring, 0);
		return;
	}

	if (phba->sli_rev == LPFC_SLI_REV3) {
		icmd = &elsiocb->iocb;
		if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
		    (ulp_command == CMD_IOCB_RCV_ELS64_CX ||
		     ulp_command == CMD_IOCB_RCV_SEQ64_CX)) {
			if (icmd->unsli3.rcvsli3.vpi == 0xffff)
				vport = phba->pport;
			else
				vport = lpfc_find_vport_by_vpid(phba,
						icmd->unsli3.rcvsli3.vpi);
		}
	}

	 
	if (bde_count == 0)
		return;

	 
	if (phba->sli3_options & LPFC_SLI3_HBQ_ENABLED) {
		elsiocb->cmd_dmabuf = bdeBuf1;
		if (bde_count == 2)
			elsiocb->bpl_dmabuf = bdeBuf2;
	} else {
		icmd = &elsiocb->iocb;
		paddr = getPaddr(icmd->un.cont64[0].addrHigh,
				 icmd->un.cont64[0].addrLow);
		elsiocb->cmd_dmabuf = lpfc_sli_ringpostbuf_get(phba, pring,
							       paddr);
		if (bde_count == 2) {
			paddr = getPaddr(icmd->un.cont64[1].addrHigh,
					 icmd->un.cont64[1].addrLow);
			elsiocb->bpl_dmabuf = lpfc_sli_ringpostbuf_get(phba,
									pring,
									paddr);
		}
	}

	lpfc_els_unsol_buffer(phba, pring, vport, elsiocb);
	 
	if (elsiocb->cmd_dmabuf) {
		lpfc_in_buf_free(phba, elsiocb->cmd_dmabuf);
		elsiocb->cmd_dmabuf = NULL;
	}

	if (elsiocb->bpl_dmabuf) {
		lpfc_in_buf_free(phba, elsiocb->bpl_dmabuf);
		elsiocb->bpl_dmabuf = NULL;
	}

}

static void
lpfc_start_fdmi(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp;

	 
	ndlp = lpfc_findnode_did(vport, FDMI_DID);
	if (!ndlp) {
		ndlp = lpfc_nlp_init(vport, FDMI_DID);
		if (ndlp) {
			ndlp->nlp_type |= NLP_FABRIC;
		} else {
			return;
		}
	}

	lpfc_nlp_set_state(vport, ndlp, NLP_STE_PLOGI_ISSUE);
	lpfc_issue_els_plogi(vport, ndlp->nlp_DID, 0);
}

 
void
lpfc_do_scr_ns_plogi(struct lpfc_hba *phba, struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	 
	spin_lock_irq(shost->host_lock);
	if (vport->fc_flag & FC_DISC_DELAYED) {
		spin_unlock_irq(shost->host_lock);
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "3334 Delay fc port discovery for %d secs\n",
				 phba->fc_ratov);
		mod_timer(&vport->delayed_disc_tmo,
			jiffies + msecs_to_jiffies(1000 * phba->fc_ratov));
		return;
	}
	spin_unlock_irq(shost->host_lock);

	ndlp = lpfc_findnode_did(vport, NameServer_DID);
	if (!ndlp) {
		ndlp = lpfc_nlp_init(vport, NameServer_DID);
		if (!ndlp) {
			if (phba->fc_topology == LPFC_TOPOLOGY_LOOP) {
				lpfc_disc_start(vport);
				return;
			}
			lpfc_vport_set_state(vport, FC_VPORT_FAILED);
			lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
					 "0251 NameServer login: no memory\n");
			return;
		}
	}

	ndlp->nlp_type |= NLP_FABRIC;

	lpfc_nlp_set_state(vport, ndlp, NLP_STE_PLOGI_ISSUE);

	if (lpfc_issue_els_plogi(vport, ndlp->nlp_DID, 0)) {
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "0252 Cannot issue NameServer login\n");
		return;
	}

	if ((phba->cfg_enable_SmartSAN ||
	     (phba->cfg_fdmi_on == LPFC_FDMI_SUPPORT)) &&
	     (vport->load_flag & FC_ALLOW_FDMI))
		lpfc_start_fdmi(vport);
}

 
static void
lpfc_cmpl_reg_new_vport(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport *vport = pmb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp = pmb->ctx_ndlp;
	MAILBOX_t *mb = &pmb->u.mb;
	int rc;

	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~FC_VPORT_NEEDS_REG_VPI;
	spin_unlock_irq(shost->host_lock);

	if (mb->mbxStatus) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				"0915 Register VPI failed : Status: x%x"
				" upd bit: x%x \n", mb->mbxStatus,
				 mb->un.varRegVpi.upd);
		if (phba->sli_rev == LPFC_SLI_REV4 &&
			mb->un.varRegVpi.upd)
			goto mbox_err_exit ;

		switch (mb->mbxStatus) {
		case 0x11:	 
		case 0x9603:	 
		case 0x9602:	 
			 
			lpfc_vport_set_state(vport, FC_VPORT_FAILED);
			spin_lock_irq(shost->host_lock);
			vport->fc_flag &= ~(FC_FABRIC | FC_PUBLIC_LOOP);
			spin_unlock_irq(shost->host_lock);
			lpfc_can_disctmo(vport);
			break;
		 
		case 0x20:
			spin_lock_irq(shost->host_lock);
			vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
			spin_unlock_irq(shost->host_lock);
			lpfc_init_vpi(phba, pmb, vport->vpi);
			pmb->vport = vport;
			pmb->mbox_cmpl = lpfc_init_vpi_cmpl;
			rc = lpfc_sli_issue_mbox(phba, pmb,
				MBX_NOWAIT);
			if (rc == MBX_NOT_FINISHED) {
				lpfc_printf_vlog(vport, KERN_ERR,
						 LOG_TRACE_EVENT,
					"2732 Failed to issue INIT_VPI"
					" mailbox command\n");
			} else {
				lpfc_nlp_put(ndlp);
				return;
			}
			fallthrough;
		default:
			 
			if (phba->sli_rev == LPFC_SLI_REV4)
				lpfc_sli4_unreg_all_rpis(vport);
			lpfc_mbx_unreg_vpi(vport);
			spin_lock_irq(shost->host_lock);
			vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
			spin_unlock_irq(shost->host_lock);
			if (mb->mbxStatus == MBX_NOT_FINISHED)
				break;
			if ((vport->port_type == LPFC_PHYSICAL_PORT) &&
			    !(vport->fc_flag & FC_LOGO_RCVD_DID_CHNG)) {
				if (phba->sli_rev == LPFC_SLI_REV4)
					lpfc_issue_init_vfi(vport);
				else
					lpfc_initial_flogi(vport);
			} else {
				lpfc_initial_fdisc(vport);
			}
			break;
		}
	} else {
		spin_lock_irq(shost->host_lock);
		vport->vpi_state |= LPFC_VPI_REGISTERED;
		spin_unlock_irq(shost->host_lock);
		if (vport == phba->pport) {
			if (phba->sli_rev < LPFC_SLI_REV4)
				lpfc_issue_fabric_reglogin(vport);
			else {
				 
				if (vport->port_state != LPFC_FDISC)
					lpfc_start_fdiscs(phba);
				lpfc_do_scr_ns_plogi(phba, vport);
			}
		} else {
			lpfc_do_scr_ns_plogi(phba, vport);
		}
	}
mbox_err_exit:
	 
	lpfc_nlp_put(ndlp);

	mempool_free(pmb, phba->mbox_mem_pool);
	return;
}

 
void
lpfc_register_new_vport(struct lpfc_hba *phba, struct lpfc_vport *vport,
			struct lpfc_nodelist *ndlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	LPFC_MBOXQ_t *mbox;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (mbox) {
		lpfc_reg_vpi(vport, mbox);
		mbox->vport = vport;
		mbox->ctx_ndlp = lpfc_nlp_get(ndlp);
		if (!mbox->ctx_ndlp) {
			mempool_free(mbox, phba->mbox_mem_pool);
			goto mbox_err_exit;
		}

		mbox->mbox_cmpl = lpfc_cmpl_reg_new_vport;
		if (lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT)
		    == MBX_NOT_FINISHED) {
			 
			lpfc_nlp_put(ndlp);
			mempool_free(mbox, phba->mbox_mem_pool);

			lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				"0253 Register VPI: Can't send mbox\n");
			goto mbox_err_exit;
		}
	} else {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "0254 Register VPI: no memory\n");
		goto mbox_err_exit;
	}
	return;

mbox_err_exit:
	lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~FC_VPORT_NEEDS_REG_VPI;
	spin_unlock_irq(shost->host_lock);
	return;
}

 
void
lpfc_cancel_all_vport_retry_delay_timer(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	struct lpfc_nodelist *ndlp;
	uint32_t link_state;
	int i;

	 
	link_state = phba->link_state;
	lpfc_linkdown(phba);
	phba->link_state = link_state;

	vports = lpfc_create_vport_work_array(phba);

	if (vports) {
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			ndlp = lpfc_findnode_did(vports[i], Fabric_DID);
			if (ndlp)
				lpfc_cancel_retry_delay_tmo(vports[i], ndlp);
			lpfc_els_flush_cmd(vports[i]);
		}
		lpfc_destroy_vport_work_array(phba, vports);
	}
}

 
void
lpfc_retry_pport_discovery(struct lpfc_hba *phba)
{
	struct lpfc_nodelist *ndlp;

	 
	lpfc_cancel_all_vport_retry_delay_timer(phba);

	 
	ndlp = lpfc_findnode_did(phba->pport, Fabric_DID);
	if (!ndlp)
		return;

	mod_timer(&ndlp->nlp_delayfunc, jiffies + msecs_to_jiffies(1000));
	spin_lock_irq(&ndlp->lock);
	ndlp->nlp_flag |= NLP_DELAY_TMO;
	spin_unlock_irq(&ndlp->lock);
	ndlp->nlp_last_elscmd = ELS_CMD_FLOGI;
	phba->pport->port_state = LPFC_FLOGI;
	return;
}

 
static int
lpfc_fabric_login_reqd(struct lpfc_hba *phba,
		struct lpfc_iocbq *cmdiocb,
		struct lpfc_iocbq *rspiocb)
{
	u32 ulp_status = get_job_ulpstatus(phba, rspiocb);
	u32 ulp_word4 = get_job_word4(phba, rspiocb);

	if (ulp_status != IOSTAT_FABRIC_RJT ||
	    ulp_word4 != RJT_LOGIN_REQUIRED)
		return 0;
	else
		return 1;
}

 
static void
lpfc_cmpl_els_fdisc(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		    struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp = cmdiocb->ndlp;
	struct lpfc_nodelist *np;
	struct lpfc_nodelist *next_np;
	struct lpfc_iocbq *piocb;
	struct lpfc_dmabuf *pcmd = cmdiocb->cmd_dmabuf, *prsp;
	struct serv_parm *sp;
	uint8_t fabric_param_changed;
	u32 ulp_status, ulp_word4;

	ulp_status = get_job_ulpstatus(phba, rspiocb);
	ulp_word4 = get_job_word4(phba, rspiocb);

	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0123 FDISC completes. x%x/x%x prevDID: x%x\n",
			 ulp_status, ulp_word4,
			 vport->fc_prevDID);
	 
	list_for_each_entry(piocb, &phba->fabric_iocb_list, list) {
		lpfc_set_disctmo(piocb->vport);
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"FDISC cmpl:      status:x%x/x%x prevdid:x%x",
		ulp_status, ulp_word4, vport->fc_prevDID);

	if (ulp_status) {

		if (lpfc_fabric_login_reqd(phba, cmdiocb, rspiocb)) {
			lpfc_retry_pport_discovery(phba);
			goto out;
		}

		 
		if (lpfc_els_retry(phba, cmdiocb, rspiocb))
			goto out;
		 
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "0126 FDISC failed. (x%x/x%x)\n",
				 ulp_status, ulp_word4);
		goto fdisc_failed;
	}

	lpfc_check_nlp_post_devloss(vport, ndlp);

	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~FC_VPORT_CVL_RCVD;
	vport->fc_flag &= ~FC_VPORT_LOGO_RCVD;
	vport->fc_flag |= FC_FABRIC;
	if (vport->phba->fc_topology == LPFC_TOPOLOGY_LOOP)
		vport->fc_flag |=  FC_PUBLIC_LOOP;
	spin_unlock_irq(shost->host_lock);

	vport->fc_myDID = ulp_word4 & Mask_DID;
	lpfc_vport_set_state(vport, FC_VPORT_ACTIVE);
	prsp = list_get_first(&pcmd->list, struct lpfc_dmabuf, list);
	if (!prsp)
		goto out;
	sp = prsp->virt + sizeof(uint32_t);
	fabric_param_changed = lpfc_check_clean_addr_bit(vport, sp);
	memcpy(&vport->fabric_portname, &sp->portName,
		sizeof(struct lpfc_name));
	memcpy(&vport->fabric_nodename, &sp->nodeName,
		sizeof(struct lpfc_name));
	if (fabric_param_changed &&
		!(vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)) {
		 
		list_for_each_entry_safe(np, next_np,
			&vport->fc_nodes, nlp_listp) {
			if ((np->nlp_state != NLP_STE_NPR_NODE) ||
			    !(np->nlp_flag & NLP_NPR_ADISC))
				continue;
			spin_lock_irq(&ndlp->lock);
			np->nlp_flag &= ~NLP_NPR_ADISC;
			spin_unlock_irq(&ndlp->lock);
			lpfc_unreg_rpi(vport, np);
		}
		lpfc_cleanup_pending_mbox(vport);

		if (phba->sli_rev == LPFC_SLI_REV4)
			lpfc_sli4_unreg_all_rpis(vport);

		lpfc_mbx_unreg_vpi(vport);
		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
		if (phba->sli_rev == LPFC_SLI_REV4)
			vport->fc_flag |= FC_VPORT_NEEDS_INIT_VPI;
		else
			vport->fc_flag |= FC_LOGO_RCVD_DID_CHNG;
		spin_unlock_irq(shost->host_lock);
	} else if ((phba->sli_rev == LPFC_SLI_REV4) &&
		!(vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)) {
		 
		lpfc_register_new_vport(phba, vport, ndlp);
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_UNMAPPED_NODE);
		goto out;
	}

	if (vport->fc_flag & FC_VPORT_NEEDS_INIT_VPI)
		lpfc_issue_init_vpi(vport);
	else if (vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)
		lpfc_register_new_vport(phba, vport, ndlp);
	else
		lpfc_do_scr_ns_plogi(phba, vport);

	 
	lpfc_nlp_set_state(vport, ndlp, NLP_STE_UNMAPPED_NODE);
	goto out;

fdisc_failed:
	if (vport->fc_vport &&
	    (vport->fc_vport->vport_state != FC_VPORT_NO_FABRIC_RSCS))
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	 
	lpfc_can_disctmo(vport);
out:
	lpfc_els_free_iocb(phba, cmdiocb);
	lpfc_nlp_put(ndlp);
}

 
static int
lpfc_issue_els_fdisc(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		     uint8_t retry)
{
	struct lpfc_hba *phba = vport->phba;
	IOCB_t *icmd;
	union lpfc_wqe128 *wqe = NULL;
	struct lpfc_iocbq *elsiocb;
	struct serv_parm *sp;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int did = ndlp->nlp_DID;
	int rc;

	vport->port_state = LPFC_FDISC;
	vport->fc_myDID = 0;
	cmdsize = (sizeof(uint32_t) + sizeof(struct serv_parm));
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp, did,
				     ELS_CMD_FDISC);
	if (!elsiocb) {
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "0255 Issue FDISC: no IOCB\n");
		return 1;
	}

	if (phba->sli_rev == LPFC_SLI_REV4) {
		wqe = &elsiocb->wqe;
		bf_set(els_req64_sid, &wqe->els_req, 0);
		bf_set(els_req64_sp, &wqe->els_req, 1);
	} else {
		icmd = &elsiocb->iocb;
		icmd->un.elsreq64.myID = 0;
		icmd->un.elsreq64.fl = 1;
		icmd->ulpCt_h = 1;
		icmd->ulpCt_l = 0;
	}

	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;
	*((uint32_t *) (pcmd)) = ELS_CMD_FDISC;
	pcmd += sizeof(uint32_t);  
	memcpy(pcmd, &vport->phba->pport->fc_sparam, sizeof(struct serv_parm));
	sp = (struct serv_parm *) pcmd;
	 
	sp->cmn.e_d_tov = 0;
	sp->cmn.w2.r_a_tov = 0;
	sp->cmn.virtual_fabric_support = 0;
	sp->cls1.classValid = 0;
	sp->cls2.seqDelivery = 1;
	sp->cls3.seqDelivery = 1;

	pcmd += sizeof(uint32_t);  
	pcmd += sizeof(uint32_t);  
	pcmd += sizeof(uint32_t);  
	pcmd += sizeof(uint32_t);  
	memcpy(pcmd, &vport->fc_portname, 8);
	pcmd += sizeof(uint32_t);  
	pcmd += sizeof(uint32_t);  
	memcpy(pcmd, &vport->fc_nodename, 8);
	sp->cmn.valid_vendor_ver_level = 0;
	memset(sp->un.vendorVersion, 0, sizeof(sp->un.vendorVersion));
	lpfc_set_disctmo(vport);

	phba->fc_stat.elsXmitFDISC++;
	elsiocb->cmd_cmpl = lpfc_cmpl_els_fdisc;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue FDISC:     did:x%x",
		did, 0, 0);

	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp)
		goto err_out;

	rc = lpfc_issue_fabric_iocb(phba, elsiocb);
	if (rc == IOCB_ERROR) {
		lpfc_nlp_put(ndlp);
		goto err_out;
	}

	lpfc_vport_set_state(vport, FC_VPORT_INITIALIZING);
	return 0;

 err_out:
	lpfc_els_free_iocb(phba, elsiocb);
	lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
			 "0256 Issue FDISC: Cannot send IOCB\n");
	return 1;
}

 
static void
lpfc_cmpl_els_npiv_logo(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
			struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	IOCB_t *irsp;
	struct lpfc_nodelist *ndlp;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	u32 ulp_status, ulp_word4, did, tmo;

	ndlp = cmdiocb->ndlp;

	ulp_status = get_job_ulpstatus(phba, rspiocb);
	ulp_word4 = get_job_word4(phba, rspiocb);

	if (phba->sli_rev == LPFC_SLI_REV4) {
		did = get_job_els_rsp64_did(phba, cmdiocb);
		tmo = get_wqe_tmo(cmdiocb);
	} else {
		irsp = &rspiocb->iocb;
		did = get_job_els_rsp64_did(phba, rspiocb);
		tmo = irsp->ulpTimeout;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"LOGO npiv cmpl:  status:x%x/x%x did:x%x",
		ulp_status, ulp_word4, did);

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "2928 NPIV LOGO completes to NPort x%x "
			 "Data: x%x x%x x%x x%x x%x x%x x%x\n",
			 ndlp->nlp_DID, ulp_status, ulp_word4,
			 tmo, vport->num_disc_nodes,
			 kref_read(&ndlp->kref), ndlp->nlp_flag,
			 ndlp->fc4_xpt_flags);

	if (ulp_status == IOSTAT_SUCCESS) {
		spin_lock_irq(shost->host_lock);
		vport->fc_flag &= ~FC_NDISC_ACTIVE;
		vport->fc_flag &= ~FC_FABRIC;
		spin_unlock_irq(shost->host_lock);
		lpfc_can_disctmo(vport);
	}

	if (ndlp->save_flags & NLP_WAIT_FOR_LOGO) {
		 
		if (ndlp->logo_waitq)
			wake_up(ndlp->logo_waitq);
		spin_lock_irq(&ndlp->lock);
		ndlp->nlp_flag &= ~(NLP_ISSUE_LOGO | NLP_LOGO_SND);
		ndlp->save_flags &= ~NLP_WAIT_FOR_LOGO;
		spin_unlock_irq(&ndlp->lock);
	}

	 
	lpfc_els_free_iocb(phba, cmdiocb);
	lpfc_nlp_put(ndlp);
}

 
int
lpfc_issue_els_npiv_logo(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	int rc = 0;
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;

	cmdsize = 2 * sizeof(uint32_t) + sizeof(struct lpfc_name);
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, 0, ndlp, ndlp->nlp_DID,
				     ELS_CMD_LOGO);
	if (!elsiocb)
		return 1;

	pcmd = (uint8_t *)elsiocb->cmd_dmabuf->virt;
	*((uint32_t *) (pcmd)) = ELS_CMD_LOGO;
	pcmd += sizeof(uint32_t);

	 
	*((uint32_t *) (pcmd)) = be32_to_cpu(vport->fc_myDID);
	pcmd += sizeof(uint32_t);
	memcpy(pcmd, &vport->fc_portname, sizeof(struct lpfc_name));

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue LOGO npiv  did:x%x flg:x%x",
		ndlp->nlp_DID, ndlp->nlp_flag, 0);

	elsiocb->cmd_cmpl = lpfc_cmpl_els_npiv_logo;
	spin_lock_irq(&ndlp->lock);
	ndlp->nlp_flag |= NLP_LOGO_SND;
	spin_unlock_irq(&ndlp->lock);
	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(phba, elsiocb);
		goto err;
	}

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
		goto err;
	}
	return 0;

err:
	spin_lock_irq(&ndlp->lock);
	ndlp->nlp_flag &= ~NLP_LOGO_SND;
	spin_unlock_irq(&ndlp->lock);
	return 1;
}

 
void
lpfc_fabric_block_timeout(struct timer_list *t)
{
	struct lpfc_hba  *phba = from_timer(phba, t, fabric_block_timer);
	unsigned long iflags;
	uint32_t tmo_posted;

	spin_lock_irqsave(&phba->pport->work_port_lock, iflags);
	tmo_posted = phba->pport->work_port_events & WORKER_FABRIC_BLOCK_TMO;
	if (!tmo_posted)
		phba->pport->work_port_events |= WORKER_FABRIC_BLOCK_TMO;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, iflags);

	if (!tmo_posted)
		lpfc_worker_wake_up(phba);
	return;
}

 
static void
lpfc_resume_fabric_iocbs(struct lpfc_hba *phba)
{
	struct lpfc_iocbq *iocb;
	unsigned long iflags;
	int ret;

repeat:
	iocb = NULL;
	spin_lock_irqsave(&phba->hbalock, iflags);
	 
	if (atomic_read(&phba->fabric_iocb_count) == 0) {
		list_remove_head(&phba->fabric_iocb_list, iocb, typeof(*iocb),
				 list);
		if (iocb)
			 
			atomic_inc(&phba->fabric_iocb_count);
	}
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	if (iocb) {
		iocb->fabric_cmd_cmpl = iocb->cmd_cmpl;
		iocb->cmd_cmpl = lpfc_cmpl_fabric_iocb;
		iocb->cmd_flag |= LPFC_IO_FABRIC;

		lpfc_debugfs_disc_trc(iocb->vport, LPFC_DISC_TRC_ELS_CMD,
				      "Fabric sched1:   ste:x%x",
				      iocb->vport->port_state, 0, 0);

		ret = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, iocb, 0);

		if (ret == IOCB_ERROR) {
			iocb->cmd_cmpl = iocb->fabric_cmd_cmpl;
			iocb->fabric_cmd_cmpl = NULL;
			iocb->cmd_flag &= ~LPFC_IO_FABRIC;
			set_job_ulpstatus(iocb, IOSTAT_LOCAL_REJECT);
			iocb->wcqe_cmpl.parameter = IOERR_SLI_ABORTED;
			iocb->cmd_cmpl(phba, iocb, iocb);

			atomic_dec(&phba->fabric_iocb_count);
			goto repeat;
		}
	}
}

 
void
lpfc_unblock_fabric_iocbs(struct lpfc_hba *phba)
{
	clear_bit(FABRIC_COMANDS_BLOCKED, &phba->bit_flags);

	lpfc_resume_fabric_iocbs(phba);
	return;
}

 
static void
lpfc_block_fabric_iocbs(struct lpfc_hba *phba)
{
	int blocked;

	blocked = test_and_set_bit(FABRIC_COMANDS_BLOCKED, &phba->bit_flags);
	 
	if (!blocked)
		mod_timer(&phba->fabric_block_timer,
			  jiffies + msecs_to_jiffies(100));

	return;
}

 
static void
lpfc_cmpl_fabric_iocb(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		      struct lpfc_iocbq *rspiocb)
{
	struct ls_rjt stat;
	u32 ulp_status = get_job_ulpstatus(phba, rspiocb);
	u32 ulp_word4 = get_job_word4(phba, rspiocb);

	WARN_ON((cmdiocb->cmd_flag & LPFC_IO_FABRIC) != LPFC_IO_FABRIC);

	switch (ulp_status) {
		case IOSTAT_NPORT_RJT:
		case IOSTAT_FABRIC_RJT:
			if (ulp_word4 & RJT_UNAVAIL_TEMP)
				lpfc_block_fabric_iocbs(phba);
			break;

		case IOSTAT_NPORT_BSY:
		case IOSTAT_FABRIC_BSY:
			lpfc_block_fabric_iocbs(phba);
			break;

		case IOSTAT_LS_RJT:
			stat.un.ls_rjt_error_be =
				cpu_to_be32(ulp_word4);
			if ((stat.un.b.lsRjtRsnCode == LSRJT_UNABLE_TPC) ||
				(stat.un.b.lsRjtRsnCode == LSRJT_LOGICAL_BSY))
				lpfc_block_fabric_iocbs(phba);
			break;
	}

	BUG_ON(atomic_read(&phba->fabric_iocb_count) == 0);

	cmdiocb->cmd_cmpl = cmdiocb->fabric_cmd_cmpl;
	cmdiocb->fabric_cmd_cmpl = NULL;
	cmdiocb->cmd_flag &= ~LPFC_IO_FABRIC;
	cmdiocb->cmd_cmpl(phba, cmdiocb, rspiocb);

	atomic_dec(&phba->fabric_iocb_count);
	if (!test_bit(FABRIC_COMANDS_BLOCKED, &phba->bit_flags)) {
		 
		lpfc_resume_fabric_iocbs(phba);
	}
}

 
static int
lpfc_issue_fabric_iocb(struct lpfc_hba *phba, struct lpfc_iocbq *iocb)
{
	unsigned long iflags;
	int ready;
	int ret;

	BUG_ON(atomic_read(&phba->fabric_iocb_count) > 1);

	spin_lock_irqsave(&phba->hbalock, iflags);
	ready = atomic_read(&phba->fabric_iocb_count) == 0 &&
		!test_bit(FABRIC_COMANDS_BLOCKED, &phba->bit_flags);

	if (ready)
		 
		atomic_inc(&phba->fabric_iocb_count);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	if (ready) {
		iocb->fabric_cmd_cmpl = iocb->cmd_cmpl;
		iocb->cmd_cmpl = lpfc_cmpl_fabric_iocb;
		iocb->cmd_flag |= LPFC_IO_FABRIC;

		lpfc_debugfs_disc_trc(iocb->vport, LPFC_DISC_TRC_ELS_CMD,
				      "Fabric sched2:   ste:x%x",
				      iocb->vport->port_state, 0, 0);

		ret = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, iocb, 0);

		if (ret == IOCB_ERROR) {
			iocb->cmd_cmpl = iocb->fabric_cmd_cmpl;
			iocb->fabric_cmd_cmpl = NULL;
			iocb->cmd_flag &= ~LPFC_IO_FABRIC;
			atomic_dec(&phba->fabric_iocb_count);
		}
	} else {
		spin_lock_irqsave(&phba->hbalock, iflags);
		list_add_tail(&iocb->list, &phba->fabric_iocb_list);
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		ret = IOCB_SUCCESS;
	}
	return ret;
}

 
static void lpfc_fabric_abort_vport(struct lpfc_vport *vport)
{
	LIST_HEAD(completions);
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_iocbq *tmp_iocb, *piocb;

	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(piocb, tmp_iocb, &phba->fabric_iocb_list,
				 list) {

		if (piocb->vport != vport)
			continue;

		list_move_tail(&piocb->list, &completions);
	}
	spin_unlock_irq(&phba->hbalock);

	 
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);
}

 
void lpfc_fabric_abort_nport(struct lpfc_nodelist *ndlp)
{
	LIST_HEAD(completions);
	struct lpfc_hba  *phba = ndlp->phba;
	struct lpfc_iocbq *tmp_iocb, *piocb;
	struct lpfc_sli_ring *pring;

	pring = lpfc_phba_elsring(phba);

	if (unlikely(!pring))
		return;

	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(piocb, tmp_iocb, &phba->fabric_iocb_list,
				 list) {
		if ((lpfc_check_sli_ndlp(phba, pring, piocb, ndlp))) {

			list_move_tail(&piocb->list, &completions);
		}
	}
	spin_unlock_irq(&phba->hbalock);

	 
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);
}

 
void lpfc_fabric_abort_hba(struct lpfc_hba *phba)
{
	LIST_HEAD(completions);

	spin_lock_irq(&phba->hbalock);
	list_splice_init(&phba->fabric_iocb_list, &completions);
	spin_unlock_irq(&phba->hbalock);

	 
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);
}

 
void
lpfc_sli4_vport_delete_els_xri_aborted(struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_sglq *sglq_entry = NULL, *sglq_next = NULL;
	struct lpfc_nodelist *ndlp = NULL;
	unsigned long iflag = 0;

	spin_lock_irqsave(&phba->sli4_hba.sgl_list_lock, iflag);
	list_for_each_entry_safe(sglq_entry, sglq_next,
			&phba->sli4_hba.lpfc_abts_els_sgl_list, list) {
		if (sglq_entry->ndlp && sglq_entry->ndlp->vport == vport) {
			lpfc_nlp_put(sglq_entry->ndlp);
			ndlp = sglq_entry->ndlp;
			sglq_entry->ndlp = NULL;

			 
			if ((vport->load_flag & FC_UNLOADING) &&
			    ndlp->nlp_DID == Fabric_DID) {
				list_del(&sglq_entry->list);
				sglq_entry->state = SGL_FREED;
				list_add_tail(&sglq_entry->list,
					&phba->sli4_hba.lpfc_els_sgl_list);
			}
		}
	}
	spin_unlock_irqrestore(&phba->sli4_hba.sgl_list_lock, iflag);
	return;
}

 
void
lpfc_sli4_els_xri_aborted(struct lpfc_hba *phba,
			  struct sli4_wcqe_xri_aborted *axri)
{
	uint16_t xri = bf_get(lpfc_wcqe_xa_xri, axri);
	uint16_t rxid = bf_get(lpfc_wcqe_xa_remote_xid, axri);
	uint16_t lxri = 0;

	struct lpfc_sglq *sglq_entry = NULL, *sglq_next = NULL;
	unsigned long iflag = 0;
	struct lpfc_nodelist *ndlp;
	struct lpfc_sli_ring *pring;

	pring = lpfc_phba_elsring(phba);

	spin_lock_irqsave(&phba->sli4_hba.sgl_list_lock, iflag);
	list_for_each_entry_safe(sglq_entry, sglq_next,
			&phba->sli4_hba.lpfc_abts_els_sgl_list, list) {
		if (sglq_entry->sli4_xritag == xri) {
			list_del(&sglq_entry->list);
			ndlp = sglq_entry->ndlp;
			sglq_entry->ndlp = NULL;
			list_add_tail(&sglq_entry->list,
				&phba->sli4_hba.lpfc_els_sgl_list);
			sglq_entry->state = SGL_FREED;
			spin_unlock_irqrestore(&phba->sli4_hba.sgl_list_lock,
					       iflag);

			if (ndlp) {
				lpfc_set_rrq_active(phba, ndlp,
					sglq_entry->sli4_lxritag,
					rxid, 1);
				lpfc_nlp_put(ndlp);
			}

			 
			if (pring && !list_empty(&pring->txq))
				lpfc_worker_wake_up(phba);
			return;
		}
	}
	spin_unlock_irqrestore(&phba->sli4_hba.sgl_list_lock, iflag);
	lxri = lpfc_sli4_xri_inrange(phba, xri);
	if (lxri == NO_XRI)
		return;

	spin_lock_irqsave(&phba->hbalock, iflag);
	sglq_entry = __lpfc_get_active_sglq(phba, lxri);
	if (!sglq_entry || (sglq_entry->sli4_xritag != xri)) {
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		return;
	}
	sglq_entry->state = SGL_XRI_ABORTED;
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return;
}

 
void
lpfc_sli_abts_recover_port(struct lpfc_vport *vport,
			   struct lpfc_nodelist *ndlp)
{
	struct Scsi_Host *shost;
	struct lpfc_hba *phba;
	unsigned long flags = 0;

	shost = lpfc_shost_from_vport(vport);
	phba = vport->phba;
	if (ndlp->nlp_state != NLP_STE_MAPPED_NODE) {
		lpfc_printf_log(phba, KERN_INFO,
				LOG_SLI, "3093 No rport recovery needed. "
				"rport in state 0x%x\n", ndlp->nlp_state);
		return;
	}
	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"3094 Start rport recovery on shost id 0x%x "
			"fc_id 0x%06x vpi 0x%x rpi 0x%x state 0x%x "
			"flags 0x%x\n",
			shost->host_no, ndlp->nlp_DID,
			vport->vpi, ndlp->nlp_rpi, ndlp->nlp_state,
			ndlp->nlp_flag);
	 
	spin_lock_irqsave(&ndlp->lock, flags);
	ndlp->nlp_fcp_info &= ~NLP_FCP_2_DEVICE;
	ndlp->nlp_flag |= NLP_ISSUE_LOGO;
	spin_unlock_irqrestore(&ndlp->lock, flags);
	lpfc_unreg_rpi(vport, ndlp);
}

static void lpfc_init_cs_ctl_bitmap(struct lpfc_vport *vport)
{
	bitmap_zero(vport->vmid_priority_range, LPFC_VMID_MAX_PRIORITY_RANGE);
}

static void
lpfc_vmid_set_cs_ctl_range(struct lpfc_vport *vport, u32 min, u32 max)
{
	u32 i;

	if ((min > max) || (max > LPFC_VMID_MAX_PRIORITY_RANGE))
		return;

	for (i = min; i <= max; i++)
		set_bit(i, vport->vmid_priority_range);
}

static void lpfc_vmid_put_cs_ctl(struct lpfc_vport *vport, u32 ctcl_vmid)
{
	set_bit(ctcl_vmid, vport->vmid_priority_range);
}

u32 lpfc_vmid_get_cs_ctl(struct lpfc_vport *vport)
{
	u32 i;

	i = find_first_bit(vport->vmid_priority_range,
			   LPFC_VMID_MAX_PRIORITY_RANGE);

	if (i == LPFC_VMID_MAX_PRIORITY_RANGE)
		return 0;

	clear_bit(i, vport->vmid_priority_range);
	return i;
}

#define MAX_PRIORITY_DESC	255

static void
lpfc_cmpl_els_qfpa(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		   struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct priority_range_desc *desc;
	struct lpfc_dmabuf *prsp = NULL;
	struct lpfc_vmid_priority_range *vmid_range = NULL;
	u32 *data;
	struct lpfc_dmabuf *dmabuf = cmdiocb->cmd_dmabuf;
	u32 ulp_status = get_job_ulpstatus(phba, rspiocb);
	u32 ulp_word4 = get_job_word4(phba, rspiocb);
	u8 *pcmd, max_desc;
	u32 len, i;
	struct lpfc_nodelist *ndlp = cmdiocb->ndlp;

	prsp = list_get_first(&dmabuf->list, struct lpfc_dmabuf, list);
	if (!prsp)
		goto out;

	pcmd = prsp->virt;
	data = (u32 *)pcmd;
	if (data[0] == ELS_CMD_LS_RJT) {
		lpfc_printf_vlog(vport, KERN_WARNING, LOG_SLI,
				 "3277 QFPA LS_RJT x%x  x%x\n",
				 data[0], data[1]);
		goto out;
	}
	if (ulp_status) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_SLI,
				 "6529 QFPA failed with status x%x  x%x\n",
				 ulp_status, ulp_word4);
		goto out;
	}

	if (!vport->qfpa_res) {
		max_desc = FCELSSIZE / sizeof(*vport->qfpa_res);
		vport->qfpa_res = kcalloc(max_desc, sizeof(*vport->qfpa_res),
					  GFP_KERNEL);
		if (!vport->qfpa_res)
			goto out;
	}

	len = *((u32 *)(pcmd + 4));
	len = be32_to_cpu(len);
	memcpy(vport->qfpa_res, pcmd, len + 8);
	len = len / LPFC_PRIORITY_RANGE_DESC_SIZE;

	desc = (struct priority_range_desc *)(pcmd + 8);
	vmid_range = vport->vmid_priority.vmid_range;
	if (!vmid_range) {
		vmid_range = kcalloc(MAX_PRIORITY_DESC, sizeof(*vmid_range),
				     GFP_KERNEL);
		if (!vmid_range) {
			kfree(vport->qfpa_res);
			goto out;
		}
		vport->vmid_priority.vmid_range = vmid_range;
	}
	vport->vmid_priority.num_descriptors = len;

	for (i = 0; i < len; i++, vmid_range++, desc++) {
		lpfc_printf_vlog(vport, KERN_DEBUG, LOG_ELS,
				 "6539 vmid values low=%d, high=%d, qos=%d, "
				 "local ve id=%d\n", desc->lo_range,
				 desc->hi_range, desc->qos_priority,
				 desc->local_ve_id);

		vmid_range->low = desc->lo_range << 1;
		if (desc->local_ve_id == QFPA_ODD_ONLY)
			vmid_range->low++;
		if (desc->qos_priority)
			vport->vmid_flag |= LPFC_VMID_QOS_ENABLED;
		vmid_range->qos = desc->qos_priority;

		vmid_range->high = desc->hi_range << 1;
		if ((desc->local_ve_id == QFPA_ODD_ONLY) ||
		    (desc->local_ve_id == QFPA_EVEN_ODD))
			vmid_range->high++;
	}
	lpfc_init_cs_ctl_bitmap(vport);
	for (i = 0; i < vport->vmid_priority.num_descriptors; i++) {
		lpfc_vmid_set_cs_ctl_range(vport,
				vport->vmid_priority.vmid_range[i].low,
				vport->vmid_priority.vmid_range[i].high);
	}

	vport->vmid_flag |= LPFC_VMID_QFPA_CMPL;
 out:
	lpfc_els_free_iocb(phba, cmdiocb);
	lpfc_nlp_put(ndlp);
}

int lpfc_issue_els_qfpa(struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_nodelist *ndlp;
	struct lpfc_iocbq *elsiocb;
	u8 *pcmd;
	int ret;

	ndlp = lpfc_findnode_did(phba->pport, Fabric_DID);
	if (!ndlp || ndlp->nlp_state != NLP_STE_UNMAPPED_NODE)
		return -ENXIO;

	elsiocb = lpfc_prep_els_iocb(vport, 1, LPFC_QFPA_SIZE, 2, ndlp,
				     ndlp->nlp_DID, ELS_CMD_QFPA);
	if (!elsiocb)
		return -ENOMEM;

	pcmd = (u8 *)elsiocb->cmd_dmabuf->virt;

	*((u32 *)(pcmd)) = ELS_CMD_QFPA;
	pcmd += 4;

	elsiocb->cmd_cmpl = lpfc_cmpl_els_qfpa;

	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(vport->phba, elsiocb);
		return -ENXIO;
	}

	ret = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 2);
	if (ret != IOCB_SUCCESS) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_nlp_put(ndlp);
		return -EIO;
	}
	vport->vmid_flag &= ~LPFC_VMID_QOS_ENABLED;
	return 0;
}

int
lpfc_vmid_uvem(struct lpfc_vport *vport,
	       struct lpfc_vmid *vmid, bool instantiated)
{
	struct lpfc_vem_id_desc *vem_id_desc;
	struct lpfc_nodelist *ndlp;
	struct lpfc_iocbq *elsiocb;
	struct instantiated_ve_desc *inst_desc;
	struct lpfc_vmid_context *vmid_context;
	u8 *pcmd;
	u32 *len;
	int ret = 0;

	ndlp = lpfc_findnode_did(vport, Fabric_DID);
	if (!ndlp || ndlp->nlp_state != NLP_STE_UNMAPPED_NODE)
		return -ENXIO;

	vmid_context = kmalloc(sizeof(*vmid_context), GFP_KERNEL);
	if (!vmid_context)
		return -ENOMEM;
	elsiocb = lpfc_prep_els_iocb(vport, 1, LPFC_UVEM_SIZE, 2,
				     ndlp, Fabric_DID, ELS_CMD_UVEM);
	if (!elsiocb)
		goto out;

	lpfc_printf_vlog(vport, KERN_DEBUG, LOG_ELS,
			 "3427 Host vmid %s %d\n",
			 vmid->host_vmid, instantiated);
	vmid_context->vmp = vmid;
	vmid_context->nlp = ndlp;
	vmid_context->instantiated = instantiated;
	elsiocb->vmid_tag.vmid_context = vmid_context;
	pcmd = (u8 *)elsiocb->cmd_dmabuf->virt;

	if (!memchr_inv(vport->lpfc_vmid_host_uuid, 0,
			sizeof(vport->lpfc_vmid_host_uuid)))
		memcpy(vport->lpfc_vmid_host_uuid, vmid->host_vmid,
		       sizeof(vport->lpfc_vmid_host_uuid));

	*((u32 *)(pcmd)) = ELS_CMD_UVEM;
	len = (u32 *)(pcmd + 4);
	*len = cpu_to_be32(LPFC_UVEM_SIZE - 8);

	vem_id_desc = (struct lpfc_vem_id_desc *)(pcmd + 8);
	vem_id_desc->tag = be32_to_cpu(VEM_ID_DESC_TAG);
	vem_id_desc->length = be32_to_cpu(LPFC_UVEM_VEM_ID_DESC_SIZE);
	memcpy(vem_id_desc->vem_id, vport->lpfc_vmid_host_uuid,
	       sizeof(vem_id_desc->vem_id));

	inst_desc = (struct instantiated_ve_desc *)(pcmd + 32);
	inst_desc->tag = be32_to_cpu(INSTANTIATED_VE_DESC_TAG);
	inst_desc->length = be32_to_cpu(LPFC_UVEM_VE_MAP_DESC_SIZE);
	memcpy(inst_desc->global_vem_id, vmid->host_vmid,
	       sizeof(inst_desc->global_vem_id));

	bf_set(lpfc_instantiated_nport_id, inst_desc, vport->fc_myDID);
	bf_set(lpfc_instantiated_local_id, inst_desc,
	       vmid->un.cs_ctl_vmid);
	if (instantiated) {
		inst_desc->tag = be32_to_cpu(INSTANTIATED_VE_DESC_TAG);
	} else {
		inst_desc->tag = be32_to_cpu(DEINSTANTIATED_VE_DESC_TAG);
		lpfc_vmid_put_cs_ctl(vport, vmid->un.cs_ctl_vmid);
	}
	inst_desc->word6 = cpu_to_be32(inst_desc->word6);

	elsiocb->cmd_cmpl = lpfc_cmpl_els_uvem;

	elsiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!elsiocb->ndlp) {
		lpfc_els_free_iocb(vport->phba, elsiocb);
		goto out;
	}

	ret = lpfc_sli_issue_iocb(vport->phba, LPFC_ELS_RING, elsiocb, 0);
	if (ret != IOCB_SUCCESS) {
		lpfc_els_free_iocb(vport->phba, elsiocb);
		lpfc_nlp_put(ndlp);
		goto out;
	}

	return 0;
 out:
	kfree(vmid_context);
	return -EIO;
}

static void
lpfc_cmpl_els_uvem(struct lpfc_hba *phba, struct lpfc_iocbq *icmdiocb,
		   struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = icmdiocb->vport;
	struct lpfc_dmabuf *prsp = NULL;
	struct lpfc_vmid_context *vmid_context =
	    icmdiocb->vmid_tag.vmid_context;
	struct lpfc_nodelist *ndlp = icmdiocb->ndlp;
	u8 *pcmd;
	u32 *data;
	u32 ulp_status = get_job_ulpstatus(phba, rspiocb);
	u32 ulp_word4 = get_job_word4(phba, rspiocb);
	struct lpfc_dmabuf *dmabuf = icmdiocb->cmd_dmabuf;
	struct lpfc_vmid *vmid;

	vmid = vmid_context->vmp;
	if (!ndlp || ndlp->nlp_state != NLP_STE_UNMAPPED_NODE)
		ndlp = NULL;

	prsp = list_get_first(&dmabuf->list, struct lpfc_dmabuf, list);
	if (!prsp)
		goto out;
	pcmd = prsp->virt;
	data = (u32 *)pcmd;
	if (data[0] == ELS_CMD_LS_RJT) {
		lpfc_printf_vlog(vport, KERN_WARNING, LOG_SLI,
				 "4532 UVEM LS_RJT %x %x\n", data[0], data[1]);
		goto out;
	}
	if (ulp_status) {
		lpfc_printf_vlog(vport, KERN_WARNING, LOG_SLI,
				 "4533 UVEM error status %x: %x\n",
				 ulp_status, ulp_word4);
		goto out;
	}
	spin_lock(&phba->hbalock);
	 
	vport->vmid_flag |= LPFC_VMID_IN_USE;
	phba->pport->vmid_flag |= LPFC_VMID_IN_USE;
	spin_unlock(&phba->hbalock);

	if (vmid_context->instantiated) {
		write_lock(&vport->vmid_lock);
		vmid->flag |= LPFC_VMID_REGISTERED;
		vmid->flag &= ~LPFC_VMID_REQ_REGISTER;
		write_unlock(&vport->vmid_lock);
	}

 out:
	kfree(vmid_context);
	lpfc_els_free_iocb(phba, icmdiocb);
	lpfc_nlp_put(ndlp);
}
