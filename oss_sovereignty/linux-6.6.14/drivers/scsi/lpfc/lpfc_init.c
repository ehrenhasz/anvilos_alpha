 

#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/sched/clock.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/miscdevice.h>
#include <linux/percpu.h>
#include <linux/irq.h>
#include <linux/bitops.h>
#include <linux/crash_dump.h>
#include <linux/cpu.h>
#include <linux/cpuhotplug.h>

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_fc.h>
#include <scsi/scsi_tcq.h>
#include <scsi/fc/fc_fs.h>

#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_nl.h"
#include "lpfc_disc.h"
#include "lpfc.h"
#include "lpfc_scsi.h"
#include "lpfc_nvme.h"
#include "lpfc_logmsg.h"
#include "lpfc_crtn.h"
#include "lpfc_vport.h"
#include "lpfc_version.h"
#include "lpfc_ids.h"

static enum cpuhp_state lpfc_cpuhp_state;
 
static uint32_t lpfc_present_cpu;
static bool lpfc_pldv_detect;

static void __lpfc_cpuhp_remove(struct lpfc_hba *phba);
static void lpfc_cpuhp_remove(struct lpfc_hba *phba);
static void lpfc_cpuhp_add(struct lpfc_hba *phba);
static void lpfc_get_hba_model_desc(struct lpfc_hba *, uint8_t *, uint8_t *);
static int lpfc_post_rcv_buf(struct lpfc_hba *);
static int lpfc_sli4_queue_verify(struct lpfc_hba *);
static int lpfc_create_bootstrap_mbox(struct lpfc_hba *);
static int lpfc_setup_endian_order(struct lpfc_hba *);
static void lpfc_destroy_bootstrap_mbox(struct lpfc_hba *);
static void lpfc_free_els_sgl_list(struct lpfc_hba *);
static void lpfc_free_nvmet_sgl_list(struct lpfc_hba *);
static void lpfc_init_sgl_list(struct lpfc_hba *);
static int lpfc_init_active_sgl_array(struct lpfc_hba *);
static void lpfc_free_active_sgl(struct lpfc_hba *);
static int lpfc_hba_down_post_s3(struct lpfc_hba *phba);
static int lpfc_hba_down_post_s4(struct lpfc_hba *phba);
static int lpfc_sli4_cq_event_pool_create(struct lpfc_hba *);
static void lpfc_sli4_cq_event_pool_destroy(struct lpfc_hba *);
static void lpfc_sli4_cq_event_release_all(struct lpfc_hba *);
static void lpfc_sli4_disable_intr(struct lpfc_hba *);
static uint32_t lpfc_sli4_enable_intr(struct lpfc_hba *, uint32_t);
static void lpfc_sli4_oas_verify(struct lpfc_hba *phba);
static uint16_t lpfc_find_cpu_handle(struct lpfc_hba *, uint16_t, int);
static void lpfc_setup_bg(struct lpfc_hba *, struct Scsi_Host *);
static int lpfc_sli4_cgn_parm_chg_evt(struct lpfc_hba *);
static void lpfc_sli4_prep_dev_for_reset(struct lpfc_hba *phba);

static struct scsi_transport_template *lpfc_transport_template = NULL;
static struct scsi_transport_template *lpfc_vport_transport_template = NULL;
static DEFINE_IDR(lpfc_hba_index);
#define LPFC_NVMET_BUF_POST 254
static int lpfc_vmid_res_alloc(struct lpfc_hba *phba, struct lpfc_vport *vport);
static void lpfc_cgn_update_tstamp(struct lpfc_hba *phba, struct lpfc_cgn_ts *ts);

 
int
lpfc_config_port_prep(struct lpfc_hba *phba)
{
	lpfc_vpd_t *vp = &phba->vpd;
	int i = 0, rc;
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *mb;
	char *lpfc_vpd_data = NULL;
	uint16_t offset = 0;
	static char licensed[56] =
		    "key unlock for use with gnu public licensed code only\0";
	static int init_key = 1;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		phba->link_state = LPFC_HBA_ERROR;
		return -ENOMEM;
	}

	mb = &pmb->u.mb;
	phba->link_state = LPFC_INIT_MBX_CMDS;

	if (lpfc_is_LC_HBA(phba->pcidev->device)) {
		if (init_key) {
			uint32_t *ptext = (uint32_t *) licensed;

			for (i = 0; i < 56; i += sizeof (uint32_t), ptext++)
				*ptext = cpu_to_be32(*ptext);
			init_key = 0;
		}

		lpfc_read_nv(phba, pmb);
		memset((char*)mb->un.varRDnvp.rsvd3, 0,
			sizeof (mb->un.varRDnvp.rsvd3));
		memcpy((char*)mb->un.varRDnvp.rsvd3, licensed,
			 sizeof (licensed));

		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);

		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0324 Config Port initialization "
					"error, mbxCmd x%x READ_NVPARM, "
					"mbxStatus x%x\n",
					mb->mbxCommand, mb->mbxStatus);
			mempool_free(pmb, phba->mbox_mem_pool);
			return -ERESTART;
		}
		memcpy(phba->wwnn, (char *)mb->un.varRDnvp.nodename,
		       sizeof(phba->wwnn));
		memcpy(phba->wwpn, (char *)mb->un.varRDnvp.portname,
		       sizeof(phba->wwpn));
	}

	 
	phba->sli3_options &= (uint32_t)LPFC_SLI3_BG_ENABLED;

	 
	lpfc_read_rev(phba, pmb);
	rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);
	if (rc != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0439 Adapter failed to init, mbxCmd x%x "
				"READ_REV, mbxStatus x%x\n",
				mb->mbxCommand, mb->mbxStatus);
		mempool_free( pmb, phba->mbox_mem_pool);
		return -ERESTART;
	}


	 
	if (mb->un.varRdRev.rr == 0) {
		vp->rev.rBit = 0;
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0440 Adapter failed to init, READ_REV has "
				"missing revision information.\n");
		mempool_free(pmb, phba->mbox_mem_pool);
		return -ERESTART;
	}

	if (phba->sli_rev == 3 && !mb->un.varRdRev.v3rsp) {
		mempool_free(pmb, phba->mbox_mem_pool);
		return -EINVAL;
	}

	 
	vp->rev.rBit = 1;
	memcpy(&vp->sli3Feat, &mb->un.varRdRev.sli3Feat, sizeof(uint32_t));
	vp->rev.sli1FwRev = mb->un.varRdRev.sli1FwRev;
	memcpy(vp->rev.sli1FwName, (char*) mb->un.varRdRev.sli1FwName, 16);
	vp->rev.sli2FwRev = mb->un.varRdRev.sli2FwRev;
	memcpy(vp->rev.sli2FwName, (char *) mb->un.varRdRev.sli2FwName, 16);
	vp->rev.biuRev = mb->un.varRdRev.biuRev;
	vp->rev.smRev = mb->un.varRdRev.smRev;
	vp->rev.smFwRev = mb->un.varRdRev.un.smFwRev;
	vp->rev.endecRev = mb->un.varRdRev.endecRev;
	vp->rev.fcphHigh = mb->un.varRdRev.fcphHigh;
	vp->rev.fcphLow = mb->un.varRdRev.fcphLow;
	vp->rev.feaLevelHigh = mb->un.varRdRev.feaLevelHigh;
	vp->rev.feaLevelLow = mb->un.varRdRev.feaLevelLow;
	vp->rev.postKernRev = mb->un.varRdRev.postKernRev;
	vp->rev.opFwRev = mb->un.varRdRev.opFwRev;

	 
	if (vp->rev.feaLevelHigh < 9)
		phba->sli3_options |= LPFC_SLI3_VPORT_TEARDOWN;

	if (lpfc_is_LC_HBA(phba->pcidev->device))
		memcpy(phba->RandomData, (char *)&mb->un.varWords[24],
						sizeof (phba->RandomData));

	 
	lpfc_vpd_data = kmalloc(DMP_VPD_SIZE, GFP_KERNEL);
	if (!lpfc_vpd_data)
		goto out_free_mbox;
	do {
		lpfc_dump_mem(phba, pmb, offset, DMP_REGION_VPD);
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);

		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"0441 VPD not present on adapter, "
					"mbxCmd x%x DUMP VPD, mbxStatus x%x\n",
					mb->mbxCommand, mb->mbxStatus);
			mb->un.varDmp.word_cnt = 0;
		}
		 
		if (mb->un.varDmp.word_cnt == 0)
			break;

		if (mb->un.varDmp.word_cnt > DMP_VPD_SIZE - offset)
			mb->un.varDmp.word_cnt = DMP_VPD_SIZE - offset;
		lpfc_sli_pcimem_bcopy(((uint8_t *)mb) + DMP_RSP_OFFSET,
				      lpfc_vpd_data + offset,
				      mb->un.varDmp.word_cnt);
		offset += mb->un.varDmp.word_cnt;
	} while (mb->un.varDmp.word_cnt && offset < DMP_VPD_SIZE);

	lpfc_parse_vpd(phba, lpfc_vpd_data, offset);

	kfree(lpfc_vpd_data);
out_free_mbox:
	mempool_free(pmb, phba->mbox_mem_pool);
	return 0;
}

 
static void
lpfc_config_async_cmpl(struct lpfc_hba * phba, LPFC_MBOXQ_t * pmboxq)
{
	if (pmboxq->u.mb.mbxStatus == MBX_SUCCESS)
		phba->temp_sensor_support = 1;
	else
		phba->temp_sensor_support = 0;
	mempool_free(pmboxq, phba->mbox_mem_pool);
	return;
}

 
static void
lpfc_dump_wakeup_param_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmboxq)
{
	struct prog_id *prg;
	uint32_t prog_id_word;
	char dist = ' ';
	 
	char dist_char[] = "nabx";

	if (pmboxq->u.mb.mbxStatus != MBX_SUCCESS) {
		mempool_free(pmboxq, phba->mbox_mem_pool);
		return;
	}

	prg = (struct prog_id *) &prog_id_word;

	 
	prog_id_word = pmboxq->u.mb.un.varWords[7];

	 
	dist = dist_char[prg->dist];

	if ((prg->dist == 3) && (prg->num == 0))
		snprintf(phba->OptionROMVersion, 32, "%d.%d%d",
			prg->ver, prg->rev, prg->lev);
	else
		snprintf(phba->OptionROMVersion, 32, "%d.%d%d%c%d",
			prg->ver, prg->rev, prg->lev,
			dist, prg->num);
	mempool_free(pmboxq, phba->mbox_mem_pool);
	return;
}

 
void
lpfc_update_vport_wwn(struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;

	 
	if (vport->fc_nodename.u.wwn[0] == 0)
		memcpy(&vport->fc_nodename, &vport->fc_sparam.nodeName,
			sizeof(struct lpfc_name));
	else
		memcpy(&vport->fc_sparam.nodeName, &vport->fc_nodename,
			sizeof(struct lpfc_name));

	 
	if (vport->fc_portname.u.wwn[0] != 0 &&
		memcmp(&vport->fc_portname, &vport->fc_sparam.portName,
		       sizeof(struct lpfc_name))) {
		vport->vport_flag |= FAWWPN_PARAM_CHG;

		if (phba->sli_rev == LPFC_SLI_REV4 &&
		    vport->port_type == LPFC_PHYSICAL_PORT &&
		    phba->sli4_hba.fawwpn_flag & LPFC_FAWWPN_FABRIC) {
			if (!(phba->sli4_hba.fawwpn_flag & LPFC_FAWWPN_CONFIG))
				phba->sli4_hba.fawwpn_flag &=
						~LPFC_FAWWPN_FABRIC;
			lpfc_printf_log(phba, KERN_INFO,
					LOG_SLI | LOG_DISCOVERY | LOG_ELS,
					"2701 FA-PWWN change WWPN from %llx to "
					"%llx: vflag x%x fawwpn_flag x%x\n",
					wwn_to_u64(vport->fc_portname.u.wwn),
					wwn_to_u64
					   (vport->fc_sparam.portName.u.wwn),
					vport->vport_flag,
					phba->sli4_hba.fawwpn_flag);
			memcpy(&vport->fc_portname, &vport->fc_sparam.portName,
			       sizeof(struct lpfc_name));
		}
	}

	if (vport->fc_portname.u.wwn[0] == 0)
		memcpy(&vport->fc_portname, &vport->fc_sparam.portName,
		       sizeof(struct lpfc_name));
	else
		memcpy(&vport->fc_sparam.portName, &vport->fc_portname,
		       sizeof(struct lpfc_name));
}

 
int
lpfc_config_port_post(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pport;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *mb;
	struct lpfc_dmabuf *mp;
	struct lpfc_sli *psli = &phba->sli;
	uint32_t status, timeout;
	int i, j;
	int rc;

	spin_lock_irq(&phba->hbalock);
	 
	if (phba->over_temp_state == HBA_OVER_TEMP)
		phba->over_temp_state = HBA_NORMAL_TEMP;
	spin_unlock_irq(&phba->hbalock);

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		phba->link_state = LPFC_HBA_ERROR;
		return -ENOMEM;
	}
	mb = &pmb->u.mb;

	 
	rc = lpfc_read_sparam(phba, pmb, 0);
	if (rc) {
		mempool_free(pmb, phba->mbox_mem_pool);
		return -ENOMEM;
	}

	pmb->vport = vport;
	if (lpfc_sli_issue_mbox(phba, pmb, MBX_POLL) != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0448 Adapter failed init, mbxCmd x%x "
				"READ_SPARM mbxStatus x%x\n",
				mb->mbxCommand, mb->mbxStatus);
		phba->link_state = LPFC_HBA_ERROR;
		lpfc_mbox_rsrc_cleanup(phba, pmb, MBOX_THD_UNLOCKED);
		return -EIO;
	}

	mp = (struct lpfc_dmabuf *)pmb->ctx_buf;

	 
	memcpy(&vport->fc_sparam, mp->virt, sizeof (struct serv_parm));
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	pmb->ctx_buf = NULL;
	lpfc_update_vport_wwn(vport);

	 
	fc_host_node_name(shost) = wwn_to_u64(vport->fc_nodename.u.wwn);
	fc_host_port_name(shost) = wwn_to_u64(vport->fc_portname.u.wwn);
	fc_host_max_npiv_vports(shost) = phba->max_vpi;

	 
	 
	if (phba->SerialNumber[0] == 0) {
		uint8_t *outptr;

		outptr = &vport->fc_nodename.u.s.IEEE[0];
		for (i = 0; i < 12; i++) {
			status = *outptr++;
			j = ((status & 0xf0) >> 4);
			if (j <= 9)
				phba->SerialNumber[i] =
				    (char)((uint8_t) 0x30 + (uint8_t) j);
			else
				phba->SerialNumber[i] =
				    (char)((uint8_t) 0x61 + (uint8_t) (j - 10));
			i++;
			j = (status & 0xf);
			if (j <= 9)
				phba->SerialNumber[i] =
				    (char)((uint8_t) 0x30 + (uint8_t) j);
			else
				phba->SerialNumber[i] =
				    (char)((uint8_t) 0x61 + (uint8_t) (j - 10));
		}
	}

	lpfc_read_config(phba, pmb);
	pmb->vport = vport;
	if (lpfc_sli_issue_mbox(phba, pmb, MBX_POLL) != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0453 Adapter failed to init, mbxCmd x%x "
				"READ_CONFIG, mbxStatus x%x\n",
				mb->mbxCommand, mb->mbxStatus);
		phba->link_state = LPFC_HBA_ERROR;
		mempool_free( pmb, phba->mbox_mem_pool);
		return -EIO;
	}

	 
	lpfc_sli_read_link_ste(phba);

	 
	if (phba->cfg_hba_queue_depth > mb->un.varRdConfig.max_xri) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"3359 HBA queue depth changed from %d to %d\n",
				phba->cfg_hba_queue_depth,
				mb->un.varRdConfig.max_xri);
		phba->cfg_hba_queue_depth = mb->un.varRdConfig.max_xri;
	}

	phba->lmt = mb->un.varRdConfig.lmt;

	 
	lpfc_get_hba_model_desc(phba, phba->ModelName, phba->ModelDesc);

	phba->link_state = LPFC_LINK_DOWN;

	 
	if (psli->sli3_ring[LPFC_EXTRA_RING].sli.sli3.cmdringaddr)
		psli->sli3_ring[LPFC_EXTRA_RING].flag |= LPFC_STOP_IOCB_EVENT;
	if (psli->sli3_ring[LPFC_FCP_RING].sli.sli3.cmdringaddr)
		psli->sli3_ring[LPFC_FCP_RING].flag |= LPFC_STOP_IOCB_EVENT;

	 
	if (phba->sli_rev != 3)
		lpfc_post_rcv_buf(phba);

	 
	if (phba->intr_type == MSIX) {
		rc = lpfc_config_msi(phba, pmb);
		if (rc) {
			mempool_free(pmb, phba->mbox_mem_pool);
			return -EIO;
		}
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0352 Config MSI mailbox command "
					"failed, mbxCmd x%x, mbxStatus x%x\n",
					pmb->u.mb.mbxCommand,
					pmb->u.mb.mbxStatus);
			mempool_free(pmb, phba->mbox_mem_pool);
			return -EIO;
		}
	}

	spin_lock_irq(&phba->hbalock);
	 
	phba->hba_flag &= ~HBA_ERATT_HANDLED;

	 
	if (lpfc_readl(phba->HCregaddr, &status)) {
		spin_unlock_irq(&phba->hbalock);
		return -EIO;
	}
	status |= HC_MBINT_ENA | HC_ERINT_ENA | HC_LAINT_ENA;
	if (psli->num_rings > 0)
		status |= HC_R0INT_ENA;
	if (psli->num_rings > 1)
		status |= HC_R1INT_ENA;
	if (psli->num_rings > 2)
		status |= HC_R2INT_ENA;
	if (psli->num_rings > 3)
		status |= HC_R3INT_ENA;

	if ((phba->cfg_poll & ENABLE_FCP_RING_POLLING) &&
	    (phba->cfg_poll & DISABLE_FCP_RING_INT))
		status &= ~(HC_R0INT_ENA);

	writel(status, phba->HCregaddr);
	readl(phba->HCregaddr);  
	spin_unlock_irq(&phba->hbalock);

	 
	timeout = phba->fc_ratov * 2;
	mod_timer(&vport->els_tmofunc,
		  jiffies + msecs_to_jiffies(1000 * timeout));
	 
	mod_timer(&phba->hb_tmofunc,
		  jiffies + msecs_to_jiffies(1000 * LPFC_HB_MBOX_INTERVAL));
	phba->hba_flag &= ~(HBA_HBEAT_INP | HBA_HBEAT_TMO);
	phba->last_completion_time = jiffies;
	 
	mod_timer(&phba->eratt_poll,
		  jiffies + msecs_to_jiffies(1000 * phba->eratt_poll_interval));

	if (phba->hba_flag & LINK_DISABLED) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2598 Adapter Link is disabled.\n");
		lpfc_down_link(phba, pmb);
		pmb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);
		if ((rc != MBX_SUCCESS) && (rc != MBX_BUSY)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"2599 Adapter failed to issue DOWN_LINK"
					" mbox command rc 0x%x\n", rc);

			mempool_free(pmb, phba->mbox_mem_pool);
			return -EIO;
		}
	} else if (phba->cfg_suppress_link_up == LPFC_INITIALIZE_LINK) {
		mempool_free(pmb, phba->mbox_mem_pool);
		rc = phba->lpfc_hba_init_link(phba, MBX_NOWAIT);
		if (rc)
			return rc;
	}
	 
	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		phba->link_state = LPFC_HBA_ERROR;
		return -ENOMEM;
	}

	lpfc_config_async(phba, pmb, LPFC_ELS_RING);
	pmb->mbox_cmpl = lpfc_config_async_cmpl;
	pmb->vport = phba->pport;
	rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);

	if ((rc != MBX_BUSY) && (rc != MBX_SUCCESS)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0456 Adapter failed to issue "
				"ASYNCEVT_ENABLE mbox status x%x\n",
				rc);
		mempool_free(pmb, phba->mbox_mem_pool);
	}

	 
	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		phba->link_state = LPFC_HBA_ERROR;
		return -ENOMEM;
	}

	lpfc_dump_wakeup_param(phba, pmb);
	pmb->mbox_cmpl = lpfc_dump_wakeup_param_cmpl;
	pmb->vport = phba->pport;
	rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);

	if ((rc != MBX_BUSY) && (rc != MBX_SUCCESS)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0435 Adapter failed "
				"to get Option ROM version status x%x\n", rc);
		mempool_free(pmb, phba->mbox_mem_pool);
	}

	return 0;
}

 
int
lpfc_sli4_refresh_params(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_mqe *mqe;
	struct lpfc_sli4_parameters *mbx_sli4_parameters;
	int length, rc;

	mboxq = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		return -ENOMEM;

	mqe = &mboxq->u.mqe;
	 
	length = (sizeof(struct lpfc_mbx_get_sli4_parameters) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_GET_SLI4_PARAMETERS,
			 length, LPFC_SLI4_MBX_EMBED);

	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	if (unlikely(rc)) {
		mempool_free(mboxq, phba->mbox_mem_pool);
		return rc;
	}
	mbx_sli4_parameters = &mqe->un.get_sli4_parameters.sli4_parameters;
	phba->sli4_hba.pc_sli4_params.mi_cap =
		bf_get(cfg_mi_ver, mbx_sli4_parameters);

	 
	if (phba->cfg_enable_mi)
		phba->sli4_hba.pc_sli4_params.mi_ver =
			bf_get(cfg_mi_ver, mbx_sli4_parameters);
	else
		phba->sli4_hba.pc_sli4_params.mi_ver = 0;

	phba->sli4_hba.pc_sli4_params.cmf =
			bf_get(cfg_cmf, mbx_sli4_parameters);
	phba->sli4_hba.pc_sli4_params.pls =
			bf_get(cfg_pvl, mbx_sli4_parameters);

	mempool_free(mboxq, phba->mbox_mem_pool);
	return rc;
}

 
static int
lpfc_hba_init_link(struct lpfc_hba *phba, uint32_t flag)
{
	return lpfc_hba_init_link_fc_topology(phba, phba->cfg_topology, flag);
}

 
int
lpfc_hba_init_link_fc_topology(struct lpfc_hba *phba, uint32_t fc_topology,
			       uint32_t flag)
{
	struct lpfc_vport *vport = phba->pport;
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *mb;
	int rc;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		phba->link_state = LPFC_HBA_ERROR;
		return -ENOMEM;
	}
	mb = &pmb->u.mb;
	pmb->vport = vport;

	if ((phba->cfg_link_speed > LPFC_USER_LINK_SPEED_MAX) ||
	    ((phba->cfg_link_speed == LPFC_USER_LINK_SPEED_1G) &&
	     !(phba->lmt & LMT_1Gb)) ||
	    ((phba->cfg_link_speed == LPFC_USER_LINK_SPEED_2G) &&
	     !(phba->lmt & LMT_2Gb)) ||
	    ((phba->cfg_link_speed == LPFC_USER_LINK_SPEED_4G) &&
	     !(phba->lmt & LMT_4Gb)) ||
	    ((phba->cfg_link_speed == LPFC_USER_LINK_SPEED_8G) &&
	     !(phba->lmt & LMT_8Gb)) ||
	    ((phba->cfg_link_speed == LPFC_USER_LINK_SPEED_10G) &&
	     !(phba->lmt & LMT_10Gb)) ||
	    ((phba->cfg_link_speed == LPFC_USER_LINK_SPEED_16G) &&
	     !(phba->lmt & LMT_16Gb)) ||
	    ((phba->cfg_link_speed == LPFC_USER_LINK_SPEED_32G) &&
	     !(phba->lmt & LMT_32Gb)) ||
	    ((phba->cfg_link_speed == LPFC_USER_LINK_SPEED_64G) &&
	     !(phba->lmt & LMT_64Gb))) {
		 
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"1302 Invalid speed for this board:%d "
				"Reset link speed to auto.\n",
				phba->cfg_link_speed);
			phba->cfg_link_speed = LPFC_USER_LINK_SPEED_AUTO;
	}
	lpfc_init_link(phba, pmb, fc_topology, phba->cfg_link_speed);
	pmb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	if (phba->sli_rev < LPFC_SLI_REV4)
		lpfc_set_loopback_flag(phba);
	rc = lpfc_sli_issue_mbox(phba, pmb, flag);
	if ((rc != MBX_BUSY) && (rc != MBX_SUCCESS)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0498 Adapter failed to init, mbxCmd x%x "
				"INIT_LINK, mbxStatus x%x\n",
				mb->mbxCommand, mb->mbxStatus);
		if (phba->sli_rev <= LPFC_SLI_REV3) {
			 
			writel(0, phba->HCregaddr);
			readl(phba->HCregaddr);  
			 
			writel(0xffffffff, phba->HAregaddr);
			readl(phba->HAregaddr);  
		}
		phba->link_state = LPFC_HBA_ERROR;
		if (rc != MBX_BUSY || flag == MBX_POLL)
			mempool_free(pmb, phba->mbox_mem_pool);
		return -EIO;
	}
	phba->cfg_suppress_link_up = LPFC_INITIALIZE_LINK;
	if (flag == MBX_POLL)
		mempool_free(pmb, phba->mbox_mem_pool);

	return 0;
}

 
static int
lpfc_hba_down_link(struct lpfc_hba *phba, uint32_t flag)
{
	LPFC_MBOXQ_t *pmb;
	int rc;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		phba->link_state = LPFC_HBA_ERROR;
		return -ENOMEM;
	}

	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"0491 Adapter Link is disabled.\n");
	lpfc_down_link(phba, pmb);
	pmb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	rc = lpfc_sli_issue_mbox(phba, pmb, flag);
	if ((rc != MBX_SUCCESS) && (rc != MBX_BUSY)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2522 Adapter failed to issue DOWN_LINK"
				" mbox command rc 0x%x\n", rc);

		mempool_free(pmb, phba->mbox_mem_pool);
		return -EIO;
	}
	if (flag == MBX_POLL)
		mempool_free(pmb, phba->mbox_mem_pool);

	return 0;
}

 
int
lpfc_hba_down_prep(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	int i;

	if (phba->sli_rev <= LPFC_SLI_REV3) {
		 
		writel(0, phba->HCregaddr);
		readl(phba->HCregaddr);  
	}

	if (phba->pport->load_flag & FC_UNLOADING)
		lpfc_cleanup_discovery_resources(phba->pport);
	else {
		vports = lpfc_create_vport_work_array(phba);
		if (vports != NULL)
			for (i = 0; i <= phba->max_vports &&
				vports[i] != NULL; i++)
				lpfc_cleanup_discovery_resources(vports[i]);
		lpfc_destroy_vport_work_array(phba, vports);
	}
	return 0;
}

 
static void
lpfc_sli4_free_sp_events(struct lpfc_hba *phba)
{
	struct lpfc_iocbq *rspiocbq;
	struct hbq_dmabuf *dmabuf;
	struct lpfc_cq_event *cq_event;

	spin_lock_irq(&phba->hbalock);
	phba->hba_flag &= ~HBA_SP_QUEUE_EVT;
	spin_unlock_irq(&phba->hbalock);

	while (!list_empty(&phba->sli4_hba.sp_queue_event)) {
		 
		spin_lock_irq(&phba->hbalock);
		list_remove_head(&phba->sli4_hba.sp_queue_event,
				 cq_event, struct lpfc_cq_event, list);
		spin_unlock_irq(&phba->hbalock);

		switch (bf_get(lpfc_wcqe_c_code, &cq_event->cqe.wcqe_cmpl)) {
		case CQE_CODE_COMPL_WQE:
			rspiocbq = container_of(cq_event, struct lpfc_iocbq,
						 cq_event);
			lpfc_sli_release_iocbq(phba, rspiocbq);
			break;
		case CQE_CODE_RECEIVE:
		case CQE_CODE_RECEIVE_V1:
			dmabuf = container_of(cq_event, struct hbq_dmabuf,
					      cq_event);
			lpfc_in_buf_free(phba, &dmabuf->dbuf);
		}
	}
}

 
static void
lpfc_hba_free_post_buf(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring *pring;
	struct lpfc_dmabuf *mp, *next_mp;
	LIST_HEAD(buflist);
	int count;

	if (phba->sli3_options & LPFC_SLI3_HBQ_ENABLED)
		lpfc_sli_hbqbuf_free_all(phba);
	else {
		 
		pring = &psli->sli3_ring[LPFC_ELS_RING];
		spin_lock_irq(&phba->hbalock);
		list_splice_init(&pring->postbufq, &buflist);
		spin_unlock_irq(&phba->hbalock);

		count = 0;
		list_for_each_entry_safe(mp, next_mp, &buflist, list) {
			list_del(&mp->list);
			count++;
			lpfc_mbuf_free(phba, mp->virt, mp->phys);
			kfree(mp);
		}

		spin_lock_irq(&phba->hbalock);
		pring->postbufq_cnt -= count;
		spin_unlock_irq(&phba->hbalock);
	}
}

 
static void
lpfc_hba_clean_txcmplq(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_queue *qp = NULL;
	struct lpfc_sli_ring *pring;
	LIST_HEAD(completions);
	int i;
	struct lpfc_iocbq *piocb, *next_iocb;

	if (phba->sli_rev != LPFC_SLI_REV4) {
		for (i = 0; i < psli->num_rings; i++) {
			pring = &psli->sli3_ring[i];
			spin_lock_irq(&phba->hbalock);
			 
			list_splice_init(&pring->txcmplq, &completions);
			pring->txcmplq_cnt = 0;
			spin_unlock_irq(&phba->hbalock);

			lpfc_sli_abort_iocb_ring(phba, pring);
		}
		 
		lpfc_sli_cancel_iocbs(phba, &completions,
				      IOSTAT_LOCAL_REJECT, IOERR_SLI_ABORTED);
		return;
	}
	list_for_each_entry(qp, &phba->sli4_hba.lpfc_wq_list, wq_list) {
		pring = qp->pring;
		if (!pring)
			continue;
		spin_lock_irq(&pring->ring_lock);
		list_for_each_entry_safe(piocb, next_iocb,
					 &pring->txcmplq, list)
			piocb->cmd_flag &= ~LPFC_IO_ON_TXCMPLQ;
		list_splice_init(&pring->txcmplq, &completions);
		pring->txcmplq_cnt = 0;
		spin_unlock_irq(&pring->ring_lock);
		lpfc_sli_abort_iocb_ring(phba, pring);
	}
	 
	lpfc_sli_cancel_iocbs(phba, &completions,
			      IOSTAT_LOCAL_REJECT, IOERR_SLI_ABORTED);
}

 
static int
lpfc_hba_down_post_s3(struct lpfc_hba *phba)
{
	lpfc_hba_free_post_buf(phba);
	lpfc_hba_clean_txcmplq(phba);
	return 0;
}

 
static int
lpfc_hba_down_post_s4(struct lpfc_hba *phba)
{
	struct lpfc_io_buf *psb, *psb_next;
	struct lpfc_async_xchg_ctx *ctxp, *ctxp_next;
	struct lpfc_sli4_hdw_queue *qp;
	LIST_HEAD(aborts);
	LIST_HEAD(nvme_aborts);
	LIST_HEAD(nvmet_aborts);
	struct lpfc_sglq *sglq_entry = NULL;
	int cnt, idx;


	lpfc_sli_hbqbuf_free_all(phba);
	lpfc_hba_clean_txcmplq(phba);

	 

	 
	spin_lock_irq(&phba->sli4_hba.sgl_list_lock);
	list_for_each_entry(sglq_entry,
		&phba->sli4_hba.lpfc_abts_els_sgl_list, list)
		sglq_entry->state = SGL_FREED;

	list_splice_init(&phba->sli4_hba.lpfc_abts_els_sgl_list,
			&phba->sli4_hba.lpfc_els_sgl_list);


	spin_unlock_irq(&phba->sli4_hba.sgl_list_lock);

	 
	spin_lock_irq(&phba->hbalock);
	cnt = 0;
	for (idx = 0; idx < phba->cfg_hdw_queue; idx++) {
		qp = &phba->sli4_hba.hdwq[idx];

		spin_lock(&qp->abts_io_buf_list_lock);
		list_splice_init(&qp->lpfc_abts_io_buf_list,
				 &aborts);

		list_for_each_entry_safe(psb, psb_next, &aborts, list) {
			psb->pCmd = NULL;
			psb->status = IOSTAT_SUCCESS;
			cnt++;
		}
		spin_lock(&qp->io_buf_list_put_lock);
		list_splice_init(&aborts, &qp->lpfc_io_buf_list_put);
		qp->put_io_bufs += qp->abts_scsi_io_bufs;
		qp->put_io_bufs += qp->abts_nvme_io_bufs;
		qp->abts_scsi_io_bufs = 0;
		qp->abts_nvme_io_bufs = 0;
		spin_unlock(&qp->io_buf_list_put_lock);
		spin_unlock(&qp->abts_io_buf_list_lock);
	}
	spin_unlock_irq(&phba->hbalock);

	if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME) {
		spin_lock_irq(&phba->sli4_hba.abts_nvmet_buf_list_lock);
		list_splice_init(&phba->sli4_hba.lpfc_abts_nvmet_ctx_list,
				 &nvmet_aborts);
		spin_unlock_irq(&phba->sli4_hba.abts_nvmet_buf_list_lock);
		list_for_each_entry_safe(ctxp, ctxp_next, &nvmet_aborts, list) {
			ctxp->flag &= ~(LPFC_NVME_XBUSY | LPFC_NVME_ABORT_OP);
			lpfc_nvmet_ctxbuf_post(phba, ctxp->ctxbuf);
		}
	}

	lpfc_sli4_free_sp_events(phba);
	return cnt;
}

 
int
lpfc_hba_down_post(struct lpfc_hba *phba)
{
	return (*phba->lpfc_hba_down_post)(phba);
}

 
static void
lpfc_hb_timeout(struct timer_list *t)
{
	struct lpfc_hba *phba;
	uint32_t tmo_posted;
	unsigned long iflag;

	phba = from_timer(phba, t, hb_tmofunc);

	 
	spin_lock_irqsave(&phba->pport->work_port_lock, iflag);
	tmo_posted = phba->pport->work_port_events & WORKER_HB_TMO;
	if (!tmo_posted)
		phba->pport->work_port_events |= WORKER_HB_TMO;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, iflag);

	 
	if (!tmo_posted)
		lpfc_worker_wake_up(phba);
	return;
}

 
static void
lpfc_rrq_timeout(struct timer_list *t)
{
	struct lpfc_hba *phba;
	unsigned long iflag;

	phba = from_timer(phba, t, rrq_tmr);
	spin_lock_irqsave(&phba->pport->work_port_lock, iflag);
	if (!(phba->pport->load_flag & FC_UNLOADING))
		phba->hba_flag |= HBA_RRQ_ACTIVE;
	else
		phba->hba_flag &= ~HBA_RRQ_ACTIVE;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, iflag);

	if (!(phba->pport->load_flag & FC_UNLOADING))
		lpfc_worker_wake_up(phba);
}

 
static void
lpfc_hb_mbox_cmpl(struct lpfc_hba * phba, LPFC_MBOXQ_t * pmboxq)
{
	unsigned long drvr_flag;

	spin_lock_irqsave(&phba->hbalock, drvr_flag);
	phba->hba_flag &= ~(HBA_HBEAT_INP | HBA_HBEAT_TMO);
	spin_unlock_irqrestore(&phba->hbalock, drvr_flag);

	 
	mempool_free(pmboxq, phba->mbox_mem_pool);
	if (!(phba->pport->fc_flag & FC_OFFLINE_MODE) &&
		!(phba->link_state == LPFC_HBA_ERROR) &&
		!(phba->pport->load_flag & FC_UNLOADING))
		mod_timer(&phba->hb_tmofunc,
			  jiffies +
			  msecs_to_jiffies(1000 * LPFC_HB_MBOX_INTERVAL));
	return;
}

 
static void
lpfc_idle_stat_delay_work(struct work_struct *work)
{
	struct lpfc_hba *phba = container_of(to_delayed_work(work),
					     struct lpfc_hba,
					     idle_stat_delay_work);
	struct lpfc_queue *eq;
	struct lpfc_sli4_hdw_queue *hdwq;
	struct lpfc_idle_stat *idle_stat;
	u32 i, idle_percent;
	u64 wall, wall_idle, diff_wall, diff_idle, busy_time;

	if (phba->pport->load_flag & FC_UNLOADING)
		return;

	if (phba->link_state == LPFC_HBA_ERROR ||
	    phba->pport->fc_flag & FC_OFFLINE_MODE ||
	    phba->cmf_active_mode != LPFC_CFG_OFF)
		goto requeue;

	for_each_present_cpu(i) {
		hdwq = &phba->sli4_hba.hdwq[phba->sli4_hba.cpu_map[i].hdwq];
		eq = hdwq->hba_eq;

		 
		if (eq->chann != i)
			continue;

		idle_stat = &phba->sli4_hba.idle_stat[i];

		 
		wall_idle = get_cpu_idle_time(i, &wall, 1);
		diff_idle = wall_idle - idle_stat->prev_idle;
		diff_wall = wall - idle_stat->prev_wall;

		if (diff_wall <= diff_idle)
			busy_time = 0;
		else
			busy_time = diff_wall - diff_idle;

		idle_percent = div64_u64(100 * busy_time, diff_wall);
		idle_percent = 100 - idle_percent;

		if (idle_percent < 15)
			eq->poll_mode = LPFC_QUEUE_WORK;
		else
			eq->poll_mode = LPFC_THREADED_IRQ;

		idle_stat->prev_idle = wall_idle;
		idle_stat->prev_wall = wall;
	}

requeue:
	schedule_delayed_work(&phba->idle_stat_delay_work,
			      msecs_to_jiffies(LPFC_IDLE_STAT_DELAY));
}

static void
lpfc_hb_eq_delay_work(struct work_struct *work)
{
	struct lpfc_hba *phba = container_of(to_delayed_work(work),
					     struct lpfc_hba, eq_delay_work);
	struct lpfc_eq_intr_info *eqi, *eqi_new;
	struct lpfc_queue *eq, *eq_next;
	unsigned char *ena_delay = NULL;
	uint32_t usdelay;
	int i;

	if (!phba->cfg_auto_imax || phba->pport->load_flag & FC_UNLOADING)
		return;

	if (phba->link_state == LPFC_HBA_ERROR ||
	    phba->pport->fc_flag & FC_OFFLINE_MODE)
		goto requeue;

	ena_delay = kcalloc(phba->sli4_hba.num_possible_cpu, sizeof(*ena_delay),
			    GFP_KERNEL);
	if (!ena_delay)
		goto requeue;

	for (i = 0; i < phba->cfg_irq_chann; i++) {
		 
		eq = phba->sli4_hba.hba_eq_hdl[i].eq;
		if (!eq)
			continue;
		if (eq->q_mode || eq->q_flag & HBA_EQ_DELAY_CHK) {
			eq->q_flag &= ~HBA_EQ_DELAY_CHK;
			ena_delay[eq->last_cpu] = 1;
		}
	}

	for_each_present_cpu(i) {
		eqi = per_cpu_ptr(phba->sli4_hba.eq_info, i);
		if (ena_delay[i]) {
			usdelay = (eqi->icnt >> 10) * LPFC_EQ_DELAY_STEP;
			if (usdelay > LPFC_MAX_AUTO_EQ_DELAY)
				usdelay = LPFC_MAX_AUTO_EQ_DELAY;
		} else {
			usdelay = 0;
		}

		eqi->icnt = 0;

		list_for_each_entry_safe(eq, eq_next, &eqi->list, cpu_list) {
			if (unlikely(eq->last_cpu != i)) {
				eqi_new = per_cpu_ptr(phba->sli4_hba.eq_info,
						      eq->last_cpu);
				list_move_tail(&eq->cpu_list, &eqi_new->list);
				continue;
			}
			if (usdelay != eq->q_mode)
				lpfc_modify_hba_eq_delay(phba, eq->hdwq, 1,
							 usdelay);
		}
	}

	kfree(ena_delay);

requeue:
	queue_delayed_work(phba->wq, &phba->eq_delay_work,
			   msecs_to_jiffies(LPFC_EQ_DELAY_MSECS));
}

 
static void lpfc_hb_mxp_handler(struct lpfc_hba *phba)
{
	u32 i;
	u32 hwq_count;

	hwq_count = phba->cfg_hdw_queue;
	for (i = 0; i < hwq_count; i++) {
		 
		lpfc_adjust_pvt_pool_count(phba, i);

		 
		lpfc_adjust_high_watermark(phba, i);

#ifdef LPFC_MXP_STAT
		 
		lpfc_snapshot_mxp(phba, i);
#endif
	}
}

 
int
lpfc_issue_hb_mbox(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *pmboxq;
	int retval;

	 
	if (phba->hba_flag & HBA_HBEAT_INP)
		return 0;

	pmboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmboxq)
		return -ENOMEM;

	lpfc_heart_beat(phba, pmboxq);
	pmboxq->mbox_cmpl = lpfc_hb_mbox_cmpl;
	pmboxq->vport = phba->pport;
	retval = lpfc_sli_issue_mbox(phba, pmboxq, MBX_NOWAIT);

	if (retval != MBX_BUSY && retval != MBX_SUCCESS) {
		mempool_free(pmboxq, phba->mbox_mem_pool);
		return -ENXIO;
	}
	phba->hba_flag |= HBA_HBEAT_INP;

	return 0;
}

 
void
lpfc_issue_hb_tmo(struct lpfc_hba *phba)
{
	if (phba->cfg_enable_hba_heartbeat)
		return;
	phba->hba_flag |= HBA_HBEAT_TMO;
}

 
void
lpfc_hb_timeout_handler(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	struct lpfc_dmabuf *buf_ptr;
	int retval = 0;
	int i, tmo;
	struct lpfc_sli *psli = &phba->sli;
	LIST_HEAD(completions);

	if (phba->cfg_xri_rebalancing) {
		 
		lpfc_hb_mxp_handler(phba);
	}

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			lpfc_rcv_seq_check_edtov(vports[i]);
			lpfc_fdmi_change_check(vports[i]);
		}
	lpfc_destroy_vport_work_array(phba, vports);

	if ((phba->link_state == LPFC_HBA_ERROR) ||
		(phba->pport->load_flag & FC_UNLOADING) ||
		(phba->pport->fc_flag & FC_OFFLINE_MODE))
		return;

	if (phba->elsbuf_cnt &&
		(phba->elsbuf_cnt == phba->elsbuf_prev_cnt)) {
		spin_lock_irq(&phba->hbalock);
		list_splice_init(&phba->elsbuf, &completions);
		phba->elsbuf_cnt = 0;
		phba->elsbuf_prev_cnt = 0;
		spin_unlock_irq(&phba->hbalock);

		while (!list_empty(&completions)) {
			list_remove_head(&completions, buf_ptr,
				struct lpfc_dmabuf, list);
			lpfc_mbuf_free(phba, buf_ptr->virt, buf_ptr->phys);
			kfree(buf_ptr);
		}
	}
	phba->elsbuf_prev_cnt = phba->elsbuf_cnt;

	 
	if (phba->cfg_enable_hba_heartbeat) {
		 
		spin_lock_irq(&phba->pport->work_port_lock);
		if (time_after(phba->last_completion_time +
				msecs_to_jiffies(1000 * LPFC_HB_MBOX_INTERVAL),
				jiffies)) {
			spin_unlock_irq(&phba->pport->work_port_lock);
			if (phba->hba_flag & HBA_HBEAT_INP)
				tmo = (1000 * LPFC_HB_MBOX_TIMEOUT);
			else
				tmo = (1000 * LPFC_HB_MBOX_INTERVAL);
			goto out;
		}
		spin_unlock_irq(&phba->pport->work_port_lock);

		 
		if (phba->hba_flag & HBA_HBEAT_INP) {
			 
			lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"0459 Adapter heartbeat still outstanding: "
				"last compl time was %d ms.\n",
				jiffies_to_msecs(jiffies
					 - phba->last_completion_time));
			tmo = (1000 * LPFC_HB_MBOX_TIMEOUT);
		} else {
			if ((!(psli->sli_flag & LPFC_SLI_MBOX_ACTIVE)) &&
				(list_empty(&psli->mboxq))) {

				retval = lpfc_issue_hb_mbox(phba);
				if (retval) {
					tmo = (1000 * LPFC_HB_MBOX_INTERVAL);
					goto out;
				}
				phba->skipped_hb = 0;
			} else if (time_before_eq(phba->last_completion_time,
					phba->skipped_hb)) {
				lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"2857 Last completion time not "
					" updated in %d ms\n",
					jiffies_to_msecs(jiffies
						 - phba->last_completion_time));
			} else
				phba->skipped_hb = jiffies;

			tmo = (1000 * LPFC_HB_MBOX_TIMEOUT);
			goto out;
		}
	} else {
		 
		if (phba->hba_flag & HBA_HBEAT_TMO) {
			retval = lpfc_issue_hb_mbox(phba);
			if (retval)
				tmo = (1000 * LPFC_HB_MBOX_INTERVAL);
			else
				tmo = (1000 * LPFC_HB_MBOX_TIMEOUT);
			goto out;
		}
		tmo = (1000 * LPFC_HB_MBOX_INTERVAL);
	}
out:
	mod_timer(&phba->hb_tmofunc, jiffies + msecs_to_jiffies(tmo));
}

 
static void
lpfc_offline_eratt(struct lpfc_hba *phba)
{
	struct lpfc_sli   *psli = &phba->sli;

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag &= ~LPFC_SLI_ACTIVE;
	spin_unlock_irq(&phba->hbalock);
	lpfc_offline_prep(phba, LPFC_MBX_NO_WAIT);

	lpfc_offline(phba);
	lpfc_reset_barrier(phba);
	spin_lock_irq(&phba->hbalock);
	lpfc_sli_brdreset(phba);
	spin_unlock_irq(&phba->hbalock);
	lpfc_hba_down_post(phba);
	lpfc_sli_brdready(phba, HS_MBRDY);
	lpfc_unblock_mgmt_io(phba);
	phba->link_state = LPFC_HBA_ERROR;
	return;
}

 
void
lpfc_sli4_offline_eratt(struct lpfc_hba *phba)
{
	spin_lock_irq(&phba->hbalock);
	if (phba->link_state == LPFC_HBA_ERROR &&
		test_bit(HBA_PCI_ERR, &phba->bit_flags)) {
		spin_unlock_irq(&phba->hbalock);
		return;
	}
	phba->link_state = LPFC_HBA_ERROR;
	spin_unlock_irq(&phba->hbalock);

	lpfc_offline_prep(phba, LPFC_MBX_NO_WAIT);
	lpfc_sli_flush_io_rings(phba);
	lpfc_offline(phba);
	lpfc_hba_down_post(phba);
	lpfc_unblock_mgmt_io(phba);
}

 
static void
lpfc_handle_deferred_eratt(struct lpfc_hba *phba)
{
	uint32_t old_host_status = phba->work_hs;
	struct lpfc_sli *psli = &phba->sli;

	 
	if (pci_channel_offline(phba->pcidev)) {
		spin_lock_irq(&phba->hbalock);
		phba->hba_flag &= ~DEFER_ERATT;
		spin_unlock_irq(&phba->hbalock);
		return;
	}

	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"0479 Deferred Adapter Hardware Error "
			"Data: x%x x%x x%x\n",
			phba->work_hs, phba->work_status[0],
			phba->work_status[1]);

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag &= ~LPFC_SLI_ACTIVE;
	spin_unlock_irq(&phba->hbalock);


	 
	lpfc_sli_abort_fcp_rings(phba);

	 
	lpfc_offline_prep(phba, LPFC_MBX_WAIT);
	lpfc_offline(phba);

	 
	while (phba->work_hs & HS_FFER1) {
		msleep(100);
		if (lpfc_readl(phba->HSregaddr, &phba->work_hs)) {
			phba->work_hs = UNPLUG_ERR ;
			break;
		}
		 
		if (phba->pport->load_flag & FC_UNLOADING) {
			phba->work_hs = 0;
			break;
		}
	}

	 
	if ((!phba->work_hs) && (!(phba->pport->load_flag & FC_UNLOADING)))
		phba->work_hs = old_host_status & ~HS_FFER1;

	spin_lock_irq(&phba->hbalock);
	phba->hba_flag &= ~DEFER_ERATT;
	spin_unlock_irq(&phba->hbalock);
	phba->work_status[0] = readl(phba->MBslimaddr + 0xa8);
	phba->work_status[1] = readl(phba->MBslimaddr + 0xac);
}

static void
lpfc_board_errevt_to_mgmt(struct lpfc_hba *phba)
{
	struct lpfc_board_event_header board_event;
	struct Scsi_Host *shost;

	board_event.event_type = FC_REG_BOARD_EVENT;
	board_event.subcategory = LPFC_EVENT_PORTINTERR;
	shost = lpfc_shost_from_vport(phba->pport);
	fc_host_post_vendor_event(shost, fc_get_event_number(),
				  sizeof(board_event),
				  (char *) &board_event,
				  LPFC_NL_VENDOR_ID);
}

 
static void
lpfc_handle_eratt_s3(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pport;
	struct lpfc_sli   *psli = &phba->sli;
	uint32_t event_data;
	unsigned long temperature;
	struct temp_event temp_event_data;
	struct Scsi_Host  *shost;

	 
	if (pci_channel_offline(phba->pcidev)) {
		spin_lock_irq(&phba->hbalock);
		phba->hba_flag &= ~DEFER_ERATT;
		spin_unlock_irq(&phba->hbalock);
		return;
	}

	 
	if (!phba->cfg_enable_hba_reset)
		return;

	 
	lpfc_board_errevt_to_mgmt(phba);

	if (phba->hba_flag & DEFER_ERATT)
		lpfc_handle_deferred_eratt(phba);

	if ((phba->work_hs & HS_FFER6) || (phba->work_hs & HS_FFER8)) {
		if (phba->work_hs & HS_FFER6)
			 
			lpfc_printf_log(phba, KERN_INFO, LOG_LINK_EVENT,
					"1301 Re-establishing Link "
					"Data: x%x x%x x%x\n",
					phba->work_hs, phba->work_status[0],
					phba->work_status[1]);
		if (phba->work_hs & HS_FFER8)
			 
			lpfc_printf_log(phba, KERN_INFO, LOG_LINK_EVENT,
					"2861 Host Authentication device "
					"zeroization Data:x%x x%x x%x\n",
					phba->work_hs, phba->work_status[0],
					phba->work_status[1]);

		spin_lock_irq(&phba->hbalock);
		psli->sli_flag &= ~LPFC_SLI_ACTIVE;
		spin_unlock_irq(&phba->hbalock);

		 
		lpfc_sli_abort_fcp_rings(phba);

		 
		lpfc_offline_prep(phba, LPFC_MBX_NO_WAIT);
		lpfc_offline(phba);
		lpfc_sli_brdrestart(phba);
		if (lpfc_online(phba) == 0) {	 
			lpfc_unblock_mgmt_io(phba);
			return;
		}
		lpfc_unblock_mgmt_io(phba);
	} else if (phba->work_hs & HS_CRIT_TEMP) {
		temperature = readl(phba->MBslimaddr + TEMPERATURE_OFFSET);
		temp_event_data.event_type = FC_REG_TEMPERATURE_EVENT;
		temp_event_data.event_code = LPFC_CRIT_TEMP;
		temp_event_data.data = (uint32_t)temperature;

		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0406 Adapter maximum temperature exceeded "
				"(%ld), taking this port offline "
				"Data: x%x x%x x%x\n",
				temperature, phba->work_hs,
				phba->work_status[0], phba->work_status[1]);

		shost = lpfc_shost_from_vport(phba->pport);
		fc_host_post_vendor_event(shost, fc_get_event_number(),
					  sizeof(temp_event_data),
					  (char *) &temp_event_data,
					  SCSI_NL_VID_TYPE_PCI
					  | PCI_VENDOR_ID_EMULEX);

		spin_lock_irq(&phba->hbalock);
		phba->over_temp_state = HBA_OVER_TEMP;
		spin_unlock_irq(&phba->hbalock);
		lpfc_offline_eratt(phba);

	} else {
		 
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0457 Adapter Hardware Error "
				"Data: x%x x%x x%x\n",
				phba->work_hs,
				phba->work_status[0], phba->work_status[1]);

		event_data = FC_REG_DUMP_EVENT;
		shost = lpfc_shost_from_vport(vport);
		fc_host_post_vendor_event(shost, fc_get_event_number(),
				sizeof(event_data), (char *) &event_data,
				SCSI_NL_VID_TYPE_PCI | PCI_VENDOR_ID_EMULEX);

		lpfc_offline_eratt(phba);
	}
	return;
}

 
static int
lpfc_sli4_port_sta_fn_reset(struct lpfc_hba *phba, int mbx_action,
			    bool en_rn_msg)
{
	int rc;
	uint32_t intr_mode;
	LPFC_MBOXQ_t *mboxq;

	if (bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) >=
	    LPFC_SLI_INTF_IF_TYPE_2) {
		 
		rc = lpfc_sli4_pdev_status_reg_wait(phba);
		if (rc)
			return rc;
	}

	 
	if (en_rn_msg)
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2887 Reset Needed: Attempting Port "
				"Recovery...\n");

	 
	if (mbx_action == LPFC_MBX_NO_WAIT) {
		spin_lock_irq(&phba->hbalock);
		phba->sli.sli_flag &= ~LPFC_SLI_ACTIVE;
		if (phba->sli.mbox_active) {
			mboxq = phba->sli.mbox_active;
			mboxq->u.mb.mbxStatus = MBX_NOT_FINISHED;
			__lpfc_mbox_cmpl_put(phba, mboxq);
			phba->sli.sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
			phba->sli.mbox_active = NULL;
		}
		spin_unlock_irq(&phba->hbalock);
	}

	lpfc_offline_prep(phba, mbx_action);
	lpfc_sli_flush_io_rings(phba);
	lpfc_offline(phba);
	 
	lpfc_sli4_disable_intr(phba);
	rc = lpfc_sli_brdrestart(phba);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"6309 Failed to restart board\n");
		return rc;
	}
	 
	intr_mode = lpfc_sli4_enable_intr(phba, phba->intr_mode);
	if (intr_mode == LPFC_INTR_ERROR) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3175 Failed to enable interrupt\n");
		return -EIO;
	}
	phba->intr_mode = intr_mode;
	rc = lpfc_online(phba);
	if (rc == 0)
		lpfc_unblock_mgmt_io(phba);

	return rc;
}

 
static void
lpfc_handle_eratt_s4(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pport;
	uint32_t event_data;
	struct Scsi_Host *shost;
	uint32_t if_type;
	struct lpfc_register portstat_reg = {0};
	uint32_t reg_err1, reg_err2;
	uint32_t uerrlo_reg, uemasklo_reg;
	uint32_t smphr_port_status = 0, pci_rd_rc1, pci_rd_rc2;
	bool en_rn_msg = true;
	struct temp_event temp_event_data;
	struct lpfc_register portsmphr_reg;
	int rc, i;

	 
	if (pci_channel_offline(phba->pcidev)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3166 pci channel is offline\n");
		lpfc_sli_flush_io_rings(phba);
		return;
	}

	memset(&portsmphr_reg, 0, sizeof(portsmphr_reg));
	if_type = bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf);
	switch (if_type) {
	case LPFC_SLI_INTF_IF_TYPE_0:
		pci_rd_rc1 = lpfc_readl(
				phba->sli4_hba.u.if_type0.UERRLOregaddr,
				&uerrlo_reg);
		pci_rd_rc2 = lpfc_readl(
				phba->sli4_hba.u.if_type0.UEMASKLOregaddr,
				&uemasklo_reg);
		 
		if (pci_rd_rc1 == -EIO && pci_rd_rc2 == -EIO)
			return;
		if (!(phba->hba_flag & HBA_RECOVERABLE_UE)) {
			lpfc_sli4_offline_eratt(phba);
			return;
		}
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"7623 Checking UE recoverable");

		for (i = 0; i < phba->sli4_hba.ue_to_sr / 1000; i++) {
			if (lpfc_readl(phba->sli4_hba.PSMPHRregaddr,
				       &portsmphr_reg.word0))
				continue;

			smphr_port_status = bf_get(lpfc_port_smphr_port_status,
						   &portsmphr_reg);
			if ((smphr_port_status & LPFC_PORT_SEM_MASK) ==
			    LPFC_PORT_SEM_UE_RECOVERABLE)
				break;
			 
			msleep(1000);
		}

		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"4827 smphr_port_status x%x : Waited %dSec",
				smphr_port_status, i);

		 
		if ((smphr_port_status & LPFC_PORT_SEM_MASK) ==
		    LPFC_PORT_SEM_UE_RECOVERABLE) {
			for (i = 0; i < 20; i++) {
				msleep(1000);
				if (!lpfc_readl(phba->sli4_hba.PSMPHRregaddr,
				    &portsmphr_reg.word0) &&
				    (LPFC_POST_STAGE_PORT_READY ==
				     bf_get(lpfc_port_smphr_port_status,
				     &portsmphr_reg))) {
					rc = lpfc_sli4_port_sta_fn_reset(phba,
						LPFC_MBX_NO_WAIT, en_rn_msg);
					if (rc == 0)
						return;
					lpfc_printf_log(phba, KERN_ERR,
						LOG_TRACE_EVENT,
						"4215 Failed to recover UE");
					break;
				}
			}
		}
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"7624 Firmware not ready: Failing UE recovery,"
				" waited %dSec", i);
		phba->link_state = LPFC_HBA_ERROR;
		break;

	case LPFC_SLI_INTF_IF_TYPE_2:
	case LPFC_SLI_INTF_IF_TYPE_6:
		pci_rd_rc1 = lpfc_readl(
				phba->sli4_hba.u.if_type2.STATUSregaddr,
				&portstat_reg.word0);
		 
		if (pci_rd_rc1 == -EIO) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3151 PCI bus read access failure: x%x\n",
				readl(phba->sli4_hba.u.if_type2.STATUSregaddr));
			lpfc_sli4_offline_eratt(phba);
			return;
		}
		reg_err1 = readl(phba->sli4_hba.u.if_type2.ERR1regaddr);
		reg_err2 = readl(phba->sli4_hba.u.if_type2.ERR2regaddr);
		if (bf_get(lpfc_sliport_status_oti, &portstat_reg)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"2889 Port Overtemperature event, "
					"taking port offline Data: x%x x%x\n",
					reg_err1, reg_err2);

			phba->sfp_alarm |= LPFC_TRANSGRESSION_HIGH_TEMPERATURE;
			temp_event_data.event_type = FC_REG_TEMPERATURE_EVENT;
			temp_event_data.event_code = LPFC_CRIT_TEMP;
			temp_event_data.data = 0xFFFFFFFF;

			shost = lpfc_shost_from_vport(phba->pport);
			fc_host_post_vendor_event(shost, fc_get_event_number(),
						  sizeof(temp_event_data),
						  (char *)&temp_event_data,
						  SCSI_NL_VID_TYPE_PCI
						  | PCI_VENDOR_ID_EMULEX);

			spin_lock_irq(&phba->hbalock);
			phba->over_temp_state = HBA_OVER_TEMP;
			spin_unlock_irq(&phba->hbalock);
			lpfc_sli4_offline_eratt(phba);
			return;
		}
		if (reg_err1 == SLIPORT_ERR1_REG_ERR_CODE_2 &&
		    reg_err2 == SLIPORT_ERR2_REG_FW_RESTART) {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"3143 Port Down: Firmware Update "
					"Detected\n");
			en_rn_msg = false;
		} else if (reg_err1 == SLIPORT_ERR1_REG_ERR_CODE_2 &&
			 reg_err2 == SLIPORT_ERR2_REG_FORCED_DUMP)
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"3144 Port Down: Debug Dump\n");
		else if (reg_err1 == SLIPORT_ERR1_REG_ERR_CODE_2 &&
			 reg_err2 == SLIPORT_ERR2_REG_FUNC_PROVISON)
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"3145 Port Down: Provisioning\n");

		 
		if (!phba->cfg_enable_hba_reset)
			return;

		 
		rc = lpfc_sli4_port_sta_fn_reset(phba, LPFC_MBX_NO_WAIT,
				en_rn_msg);
		if (rc == 0) {
			 
			if (reg_err1 == SLIPORT_ERR1_REG_ERR_CODE_2 &&
			    reg_err2 == SLIPORT_ERR2_REG_FORCED_DUMP)
				return;
			else
				break;
		}
		 
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3152 Unrecoverable error\n");
		lpfc_sli4_offline_eratt(phba);
		break;
	case LPFC_SLI_INTF_IF_TYPE_1:
	default:
		break;
	}
	lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
			"3123 Report dump event to upper layer\n");
	 
	lpfc_board_errevt_to_mgmt(phba);

	event_data = FC_REG_DUMP_EVENT;
	shost = lpfc_shost_from_vport(vport);
	fc_host_post_vendor_event(shost, fc_get_event_number(),
				  sizeof(event_data), (char *) &event_data,
				  SCSI_NL_VID_TYPE_PCI | PCI_VENDOR_ID_EMULEX);
}

 
void
lpfc_handle_eratt(struct lpfc_hba *phba)
{
	(*phba->lpfc_handle_eratt)(phba);
}

 
void
lpfc_handle_latt(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pport;
	struct lpfc_sli   *psli = &phba->sli;
	LPFC_MBOXQ_t *pmb;
	volatile uint32_t control;
	int rc = 0;

	pmb = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		rc = 1;
		goto lpfc_handle_latt_err_exit;
	}

	rc = lpfc_mbox_rsrc_prep(phba, pmb);
	if (rc) {
		rc = 2;
		mempool_free(pmb, phba->mbox_mem_pool);
		goto lpfc_handle_latt_err_exit;
	}

	 
	lpfc_els_flush_all_cmd(phba);
	psli->slistat.link_event++;
	lpfc_read_topology(phba, pmb, (struct lpfc_dmabuf *)pmb->ctx_buf);
	pmb->mbox_cmpl = lpfc_mbx_cmpl_read_topology;
	pmb->vport = vport;
	 
	phba->sli.sli3_ring[LPFC_ELS_RING].flag |= LPFC_STOP_IOCB_EVENT;
	rc = lpfc_sli_issue_mbox (phba, pmb, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		rc = 4;
		goto lpfc_handle_latt_free_mbuf;
	}

	 
	spin_lock_irq(&phba->hbalock);
	writel(HA_LATT, phba->HAregaddr);
	readl(phba->HAregaddr);  
	spin_unlock_irq(&phba->hbalock);

	return;

lpfc_handle_latt_free_mbuf:
	phba->sli.sli3_ring[LPFC_ELS_RING].flag &= ~LPFC_STOP_IOCB_EVENT;
	lpfc_mbox_rsrc_cleanup(phba, pmb, MBOX_THD_UNLOCKED);
lpfc_handle_latt_err_exit:
	 
	spin_lock_irq(&phba->hbalock);
	psli->sli_flag |= LPFC_PROCESS_LA;
	control = readl(phba->HCregaddr);
	control |= HC_LAINT_ENA;
	writel(control, phba->HCregaddr);
	readl(phba->HCregaddr);  

	 
	writel(HA_LATT, phba->HAregaddr);
	readl(phba->HAregaddr);  
	spin_unlock_irq(&phba->hbalock);
	lpfc_linkdown(phba);
	phba->link_state = LPFC_HBA_ERROR;

	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"0300 LATT: Cannot issue READ_LA: Data:%d\n", rc);

	return;
}

static void
lpfc_fill_vpd(struct lpfc_hba *phba, uint8_t *vpd, int length, int *pindex)
{
	int i, j;

	while (length > 0) {
		 
		if ((vpd[*pindex] == 'S') && (vpd[*pindex + 1] == 'N')) {
			*pindex += 2;
			i = vpd[*pindex];
			*pindex += 1;
			j = 0;
			length -= (3+i);
			while (i--) {
				phba->SerialNumber[j++] = vpd[(*pindex)++];
				if (j == 31)
					break;
			}
			phba->SerialNumber[j] = 0;
			continue;
		} else if ((vpd[*pindex] == 'V') && (vpd[*pindex + 1] == '1')) {
			phba->vpd_flag |= VPD_MODEL_DESC;
			*pindex += 2;
			i = vpd[*pindex];
			*pindex += 1;
			j = 0;
			length -= (3+i);
			while (i--) {
				phba->ModelDesc[j++] = vpd[(*pindex)++];
				if (j == 255)
					break;
			}
			phba->ModelDesc[j] = 0;
			continue;
		} else if ((vpd[*pindex] == 'V') && (vpd[*pindex + 1] == '2')) {
			phba->vpd_flag |= VPD_MODEL_NAME;
			*pindex += 2;
			i = vpd[*pindex];
			*pindex += 1;
			j = 0;
			length -= (3+i);
			while (i--) {
				phba->ModelName[j++] = vpd[(*pindex)++];
				if (j == 79)
					break;
			}
			phba->ModelName[j] = 0;
			continue;
		} else if ((vpd[*pindex] == 'V') && (vpd[*pindex + 1] == '3')) {
			phba->vpd_flag |= VPD_PROGRAM_TYPE;
			*pindex += 2;
			i = vpd[*pindex];
			*pindex += 1;
			j = 0;
			length -= (3+i);
			while (i--) {
				phba->ProgramType[j++] = vpd[(*pindex)++];
				if (j == 255)
					break;
			}
			phba->ProgramType[j] = 0;
			continue;
		} else if ((vpd[*pindex] == 'V') && (vpd[*pindex + 1] == '4')) {
			phba->vpd_flag |= VPD_PORT;
			*pindex += 2;
			i = vpd[*pindex];
			*pindex += 1;
			j = 0;
			length -= (3 + i);
			while (i--) {
				if ((phba->sli_rev == LPFC_SLI_REV4) &&
				    (phba->sli4_hba.pport_name_sta ==
				     LPFC_SLI4_PPNAME_GET)) {
					j++;
					(*pindex)++;
				} else
					phba->Port[j++] = vpd[(*pindex)++];
				if (j == 19)
					break;
			}
			if ((phba->sli_rev != LPFC_SLI_REV4) ||
			    (phba->sli4_hba.pport_name_sta ==
			     LPFC_SLI4_PPNAME_NON))
				phba->Port[j] = 0;
			continue;
		} else {
			*pindex += 2;
			i = vpd[*pindex];
			*pindex += 1;
			*pindex += i;
			length -= (3 + i);
		}
	}
}

 
int
lpfc_parse_vpd(struct lpfc_hba *phba, uint8_t *vpd, int len)
{
	uint8_t lenlo, lenhi;
	int Length;
	int i;
	int finished = 0;
	int index = 0;

	if (!vpd)
		return 0;

	 
	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"0455 Vital Product Data: x%x x%x x%x x%x\n",
			(uint32_t) vpd[0], (uint32_t) vpd[1], (uint32_t) vpd[2],
			(uint32_t) vpd[3]);
	while (!finished && (index < (len - 4))) {
		switch (vpd[index]) {
		case 0x82:
		case 0x91:
			index += 1;
			lenlo = vpd[index];
			index += 1;
			lenhi = vpd[index];
			index += 1;
			i = ((((unsigned short)lenhi) << 8) + lenlo);
			index += i;
			break;
		case 0x90:
			index += 1;
			lenlo = vpd[index];
			index += 1;
			lenhi = vpd[index];
			index += 1;
			Length = ((((unsigned short)lenhi) << 8) + lenlo);
			if (Length > len - index)
				Length = len - index;

			lpfc_fill_vpd(phba, vpd, Length, &index);
			finished = 0;
			break;
		case 0x78:
			finished = 1;
			break;
		default:
			index ++;
			break;
		}
	}

	return(1);
}

 
static void
lpfc_get_atto_model_desc(struct lpfc_hba *phba, uint8_t *mdp, uint8_t *descp)
{
	uint16_t sub_dev_id = phba->pcidev->subsystem_device;
	char *model = "<Unknown>";
	int tbolt = 0;

	switch (sub_dev_id) {
	case PCI_DEVICE_ID_CLRY_161E:
		model = "161E";
		break;
	case PCI_DEVICE_ID_CLRY_162E:
		model = "162E";
		break;
	case PCI_DEVICE_ID_CLRY_164E:
		model = "164E";
		break;
	case PCI_DEVICE_ID_CLRY_161P:
		model = "161P";
		break;
	case PCI_DEVICE_ID_CLRY_162P:
		model = "162P";
		break;
	case PCI_DEVICE_ID_CLRY_164P:
		model = "164P";
		break;
	case PCI_DEVICE_ID_CLRY_321E:
		model = "321E";
		break;
	case PCI_DEVICE_ID_CLRY_322E:
		model = "322E";
		break;
	case PCI_DEVICE_ID_CLRY_324E:
		model = "324E";
		break;
	case PCI_DEVICE_ID_CLRY_321P:
		model = "321P";
		break;
	case PCI_DEVICE_ID_CLRY_322P:
		model = "322P";
		break;
	case PCI_DEVICE_ID_CLRY_324P:
		model = "324P";
		break;
	case PCI_DEVICE_ID_TLFC_2XX2:
		model = "2XX2";
		tbolt = 1;
		break;
	case PCI_DEVICE_ID_TLFC_3162:
		model = "3162";
		tbolt = 1;
		break;
	case PCI_DEVICE_ID_TLFC_3322:
		model = "3322";
		tbolt = 1;
		break;
	default:
		model = "Unknown";
		break;
	}

	if (mdp && mdp[0] == '\0')
		snprintf(mdp, 79, "%s", model);

	if (descp && descp[0] == '\0')
		snprintf(descp, 255,
			 "ATTO %s%s, Fibre Channel Adapter Initiator, Port %s",
			 (tbolt) ? "ThunderLink FC " : "Celerity FC-",
			 model,
			 phba->Port);
}

 
static void
lpfc_get_hba_model_desc(struct lpfc_hba *phba, uint8_t *mdp, uint8_t *descp)
{
	lpfc_vpd_t *vp;
	uint16_t dev_id = phba->pcidev->device;
	int max_speed;
	int GE = 0;
	int oneConnect = 0;  
	struct {
		char *name;
		char *bus;
		char *function;
	} m = {"<Unknown>", "", ""};

	if (mdp && mdp[0] != '\0'
		&& descp && descp[0] != '\0')
		return;

	if (phba->pcidev->vendor == PCI_VENDOR_ID_ATTO) {
		lpfc_get_atto_model_desc(phba, mdp, descp);
		return;
	}

	if (phba->lmt & LMT_64Gb)
		max_speed = 64;
	else if (phba->lmt & LMT_32Gb)
		max_speed = 32;
	else if (phba->lmt & LMT_16Gb)
		max_speed = 16;
	else if (phba->lmt & LMT_10Gb)
		max_speed = 10;
	else if (phba->lmt & LMT_8Gb)
		max_speed = 8;
	else if (phba->lmt & LMT_4Gb)
		max_speed = 4;
	else if (phba->lmt & LMT_2Gb)
		max_speed = 2;
	else if (phba->lmt & LMT_1Gb)
		max_speed = 1;
	else
		max_speed = 0;

	vp = &phba->vpd;

	switch (dev_id) {
	case PCI_DEVICE_ID_FIREFLY:
		m = (typeof(m)){"LP6000", "PCI",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_SUPERFLY:
		if (vp->rev.biuRev >= 1 && vp->rev.biuRev <= 3)
			m = (typeof(m)){"LP7000", "PCI", ""};
		else
			m = (typeof(m)){"LP7000E", "PCI", ""};
		m.function = "Obsolete, Unsupported Fibre Channel Adapter";
		break;
	case PCI_DEVICE_ID_DRAGONFLY:
		m = (typeof(m)){"LP8000", "PCI",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_CENTAUR:
		if (FC_JEDEC_ID(vp->rev.biuRev) == CENTAUR_2G_JEDEC_ID)
			m = (typeof(m)){"LP9002", "PCI", ""};
		else
			m = (typeof(m)){"LP9000", "PCI", ""};
		m.function = "Obsolete, Unsupported Fibre Channel Adapter";
		break;
	case PCI_DEVICE_ID_RFLY:
		m = (typeof(m)){"LP952", "PCI",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_PEGASUS:
		m = (typeof(m)){"LP9802", "PCI-X",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_THOR:
		m = (typeof(m)){"LP10000", "PCI-X",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_VIPER:
		m = (typeof(m)){"LPX1000",  "PCI-X",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_PFLY:
		m = (typeof(m)){"LP982", "PCI-X",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_TFLY:
		m = (typeof(m)){"LP1050", "PCI-X",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_HELIOS:
		m = (typeof(m)){"LP11000", "PCI-X2",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_HELIOS_SCSP:
		m = (typeof(m)){"LP11000-SP", "PCI-X2",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_HELIOS_DCSP:
		m = (typeof(m)){"LP11002-SP",  "PCI-X2",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_NEPTUNE:
		m = (typeof(m)){"LPe1000", "PCIe",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_NEPTUNE_SCSP:
		m = (typeof(m)){"LPe1000-SP", "PCIe",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_NEPTUNE_DCSP:
		m = (typeof(m)){"LPe1002-SP", "PCIe",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_BMID:
		m = (typeof(m)){"LP1150", "PCI-X2", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_BSMB:
		m = (typeof(m)){"LP111", "PCI-X2",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_ZEPHYR:
		m = (typeof(m)){"LPe11000", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_ZEPHYR_SCSP:
		m = (typeof(m)){"LPe11000", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_ZEPHYR_DCSP:
		m = (typeof(m)){"LP2105", "PCIe", "FCoE Adapter"};
		GE = 1;
		break;
	case PCI_DEVICE_ID_ZMID:
		m = (typeof(m)){"LPe1150", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_ZSMB:
		m = (typeof(m)){"LPe111", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_LP101:
		m = (typeof(m)){"LP101", "PCI-X",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_LP10000S:
		m = (typeof(m)){"LP10000-S", "PCI",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_LP11000S:
		m = (typeof(m)){"LP11000-S", "PCI-X2",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_LPE11000S:
		m = (typeof(m)){"LPe11000-S", "PCIe",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_SAT:
		m = (typeof(m)){"LPe12000", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_SAT_MID:
		m = (typeof(m)){"LPe1250", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_SAT_SMB:
		m = (typeof(m)){"LPe121", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_SAT_DCSP:
		m = (typeof(m)){"LPe12002-SP", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_SAT_SCSP:
		m = (typeof(m)){"LPe12000-SP", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_SAT_S:
		m = (typeof(m)){"LPe12000-S", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_PROTEUS_VF:
		m = (typeof(m)){"LPev12000", "PCIe IOV",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_PROTEUS_PF:
		m = (typeof(m)){"LPev12000", "PCIe IOV",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_PROTEUS_S:
		m = (typeof(m)){"LPemv12002-S", "PCIe IOV",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_TIGERSHARK:
		oneConnect = 1;
		m = (typeof(m)){"OCe10100", "PCIe", "FCoE"};
		break;
	case PCI_DEVICE_ID_TOMCAT:
		oneConnect = 1;
		m = (typeof(m)){"OCe11100", "PCIe", "FCoE"};
		break;
	case PCI_DEVICE_ID_FALCON:
		m = (typeof(m)){"LPSe12002-ML1-E", "PCIe",
				"EmulexSecure Fibre"};
		break;
	case PCI_DEVICE_ID_BALIUS:
		m = (typeof(m)){"LPVe12002", "PCIe Shared I/O",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_LANCER_FC:
		m = (typeof(m)){"LPe16000", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_LANCER_FC_VF:
		m = (typeof(m)){"LPe16000", "PCIe",
				"Obsolete, Unsupported Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_LANCER_FCOE:
		oneConnect = 1;
		m = (typeof(m)){"OCe15100", "PCIe", "FCoE"};
		break;
	case PCI_DEVICE_ID_LANCER_FCOE_VF:
		oneConnect = 1;
		m = (typeof(m)){"OCe15100", "PCIe",
				"Obsolete, Unsupported FCoE"};
		break;
	case PCI_DEVICE_ID_LANCER_G6_FC:
		m = (typeof(m)){"LPe32000", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_LANCER_G7_FC:
		m = (typeof(m)){"LPe36000", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_LANCER_G7P_FC:
		m = (typeof(m)){"LPe38000", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_SKYHAWK:
	case PCI_DEVICE_ID_SKYHAWK_VF:
		oneConnect = 1;
		m = (typeof(m)){"OCe14000", "PCIe", "FCoE"};
		break;
	default:
		m = (typeof(m)){"Unknown", "", ""};
		break;
	}

	if (mdp && mdp[0] == '\0')
		snprintf(mdp, 79,"%s", m.name);
	 
	if (descp && descp[0] == '\0') {
		if (oneConnect)
			snprintf(descp, 255,
				"Emulex OneConnect %s, %s Initiator %s",
				m.name, m.function,
				phba->Port);
		else if (max_speed == 0)
			snprintf(descp, 255,
				"Emulex %s %s %s",
				m.name, m.bus, m.function);
		else
			snprintf(descp, 255,
				"Emulex %s %d%s %s %s",
				m.name, max_speed, (GE) ? "GE" : "Gb",
				m.bus, m.function);
	}
}

 
int
lpfc_sli3_post_buffer(struct lpfc_hba *phba, struct lpfc_sli_ring *pring, int cnt)
{
	IOCB_t *icmd;
	struct lpfc_iocbq *iocb;
	struct lpfc_dmabuf *mp1, *mp2;

	cnt += pring->missbufcnt;

	 
	while (cnt > 0) {
		 
		iocb = lpfc_sli_get_iocbq(phba);
		if (iocb == NULL) {
			pring->missbufcnt = cnt;
			return cnt;
		}
		icmd = &iocb->iocb;

		 
		 
		mp1 = kmalloc(sizeof (struct lpfc_dmabuf), GFP_KERNEL);
		if (mp1)
		    mp1->virt = lpfc_mbuf_alloc(phba, MEM_PRI, &mp1->phys);
		if (!mp1 || !mp1->virt) {
			kfree(mp1);
			lpfc_sli_release_iocbq(phba, iocb);
			pring->missbufcnt = cnt;
			return cnt;
		}

		INIT_LIST_HEAD(&mp1->list);
		 
		if (cnt > 1) {
			mp2 = kmalloc(sizeof (struct lpfc_dmabuf), GFP_KERNEL);
			if (mp2)
				mp2->virt = lpfc_mbuf_alloc(phba, MEM_PRI,
							    &mp2->phys);
			if (!mp2 || !mp2->virt) {
				kfree(mp2);
				lpfc_mbuf_free(phba, mp1->virt, mp1->phys);
				kfree(mp1);
				lpfc_sli_release_iocbq(phba, iocb);
				pring->missbufcnt = cnt;
				return cnt;
			}

			INIT_LIST_HEAD(&mp2->list);
		} else {
			mp2 = NULL;
		}

		icmd->un.cont64[0].addrHigh = putPaddrHigh(mp1->phys);
		icmd->un.cont64[0].addrLow = putPaddrLow(mp1->phys);
		icmd->un.cont64[0].tus.f.bdeSize = FCELSSIZE;
		icmd->ulpBdeCount = 1;
		cnt--;
		if (mp2) {
			icmd->un.cont64[1].addrHigh = putPaddrHigh(mp2->phys);
			icmd->un.cont64[1].addrLow = putPaddrLow(mp2->phys);
			icmd->un.cont64[1].tus.f.bdeSize = FCELSSIZE;
			cnt--;
			icmd->ulpBdeCount = 2;
		}

		icmd->ulpCommand = CMD_QUE_RING_BUF64_CN;
		icmd->ulpLe = 1;

		if (lpfc_sli_issue_iocb(phba, pring->ringno, iocb, 0) ==
		    IOCB_ERROR) {
			lpfc_mbuf_free(phba, mp1->virt, mp1->phys);
			kfree(mp1);
			cnt++;
			if (mp2) {
				lpfc_mbuf_free(phba, mp2->virt, mp2->phys);
				kfree(mp2);
				cnt++;
			}
			lpfc_sli_release_iocbq(phba, iocb);
			pring->missbufcnt = cnt;
			return cnt;
		}
		lpfc_sli_ringpostbuf_put(phba, pring, mp1);
		if (mp2)
			lpfc_sli_ringpostbuf_put(phba, pring, mp2);
	}
	pring->missbufcnt = 0;
	return 0;
}

 
static int
lpfc_post_rcv_buf(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;

	 
	lpfc_sli3_post_buffer(phba, &psli->sli3_ring[LPFC_ELS_RING], LPFC_BUF_RING0);
	 

	return 0;
}

#define S(N,V) (((V)<<(N))|((V)>>(32-(N))))

 
static void
lpfc_sha_init(uint32_t * HashResultPointer)
{
	HashResultPointer[0] = 0x67452301;
	HashResultPointer[1] = 0xEFCDAB89;
	HashResultPointer[2] = 0x98BADCFE;
	HashResultPointer[3] = 0x10325476;
	HashResultPointer[4] = 0xC3D2E1F0;
}

 
static void
lpfc_sha_iterate(uint32_t * HashResultPointer, uint32_t * HashWorkingPointer)
{
	int t;
	uint32_t TEMP;
	uint32_t A, B, C, D, E;
	t = 16;
	do {
		HashWorkingPointer[t] =
		    S(1,
		      HashWorkingPointer[t - 3] ^ HashWorkingPointer[t -
								     8] ^
		      HashWorkingPointer[t - 14] ^ HashWorkingPointer[t - 16]);
	} while (++t <= 79);
	t = 0;
	A = HashResultPointer[0];
	B = HashResultPointer[1];
	C = HashResultPointer[2];
	D = HashResultPointer[3];
	E = HashResultPointer[4];

	do {
		if (t < 20) {
			TEMP = ((B & C) | ((~B) & D)) + 0x5A827999;
		} else if (t < 40) {
			TEMP = (B ^ C ^ D) + 0x6ED9EBA1;
		} else if (t < 60) {
			TEMP = ((B & C) | (B & D) | (C & D)) + 0x8F1BBCDC;
		} else {
			TEMP = (B ^ C ^ D) + 0xCA62C1D6;
		}
		TEMP += S(5, A) + E + HashWorkingPointer[t];
		E = D;
		D = C;
		C = S(30, B);
		B = A;
		A = TEMP;
	} while (++t <= 79);

	HashResultPointer[0] += A;
	HashResultPointer[1] += B;
	HashResultPointer[2] += C;
	HashResultPointer[3] += D;
	HashResultPointer[4] += E;

}

 
static void
lpfc_challenge_key(uint32_t * RandomChallenge, uint32_t * HashWorking)
{
	*HashWorking = (*RandomChallenge ^ *HashWorking);
}

 
void
lpfc_hba_init(struct lpfc_hba *phba, uint32_t *hbainit)
{
	int t;
	uint32_t *HashWorking;
	uint32_t *pwwnn = (uint32_t *) phba->wwnn;

	HashWorking = kcalloc(80, sizeof(uint32_t), GFP_KERNEL);
	if (!HashWorking)
		return;

	HashWorking[0] = HashWorking[78] = *pwwnn++;
	HashWorking[1] = HashWorking[79] = *pwwnn;

	for (t = 0; t < 7; t++)
		lpfc_challenge_key(phba->RandomData + t, HashWorking + t);

	lpfc_sha_init(hbainit);
	lpfc_sha_iterate(hbainit, HashWorking);
	kfree(HashWorking);
}

 
void
lpfc_cleanup(struct lpfc_vport *vport)
{
	struct lpfc_hba   *phba = vport->phba;
	struct lpfc_nodelist *ndlp, *next_ndlp;
	int i = 0;

	if (phba->link_state > LPFC_LINK_DOWN)
		lpfc_port_link_failure(vport);

	 
	if (lpfc_is_vmid_enabled(phba))
		lpfc_vmid_vport_cleanup(vport);

	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {
		if (vport->port_type != LPFC_PHYSICAL_PORT &&
		    ndlp->nlp_DID == Fabric_DID) {
			 
			lpfc_nlp_put(ndlp);
			continue;
		}

		if (ndlp->nlp_DID == Fabric_Cntl_DID &&
		    ndlp->nlp_state == NLP_STE_UNUSED_NODE) {
			lpfc_nlp_put(ndlp);
			continue;
		}

		 
		if (ndlp->nlp_type & NLP_FABRIC &&
		    ndlp->nlp_state == NLP_STE_UNMAPPED_NODE)
			lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RECOVERY);

		if (!(ndlp->fc4_xpt_flags & (NVME_XPT_REGD|SCSI_XPT_REGD)))
			lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RM);
	}

	 
	if (vport->load_flag & FC_UNLOADING &&
	    pci_channel_offline(phba->pcidev))
		lpfc_sli_flush_io_rings(vport->phba);

	 
	while (!list_empty(&vport->fc_nodes)) {
		if (i++ > 3000) {
			lpfc_printf_vlog(vport, KERN_ERR,
					 LOG_TRACE_EVENT,
				"0233 Nodelist not empty\n");
			list_for_each_entry_safe(ndlp, next_ndlp,
						&vport->fc_nodes, nlp_listp) {
				lpfc_printf_vlog(ndlp->vport, KERN_ERR,
						 LOG_DISCOVERY,
						 "0282 did:x%x ndlp:x%px "
						 "refcnt:%d xflags x%x nflag x%x\n",
						 ndlp->nlp_DID, (void *)ndlp,
						 kref_read(&ndlp->kref),
						 ndlp->fc4_xpt_flags,
						 ndlp->nlp_flag);
			}
			break;
		}

		 
		msleep(10);
	}
	lpfc_cleanup_vports_rrqs(vport, NULL);
}

 
void
lpfc_stop_vport_timers(struct lpfc_vport *vport)
{
	del_timer_sync(&vport->els_tmofunc);
	del_timer_sync(&vport->delayed_disc_tmo);
	lpfc_can_disctmo(vport);
	return;
}

 
void
__lpfc_sli4_stop_fcf_redisc_wait_timer(struct lpfc_hba *phba)
{
	 
	phba->fcf.fcf_flag &= ~FCF_REDISC_PEND;

	 
	del_timer(&phba->fcf.redisc_wait);
}

 
void
lpfc_sli4_stop_fcf_redisc_wait_timer(struct lpfc_hba *phba)
{
	spin_lock_irq(&phba->hbalock);
	if (!(phba->fcf.fcf_flag & FCF_REDISC_PEND)) {
		 
		spin_unlock_irq(&phba->hbalock);
		return;
	}
	__lpfc_sli4_stop_fcf_redisc_wait_timer(phba);
	 
	phba->fcf.fcf_flag &= ~(FCF_DEAD_DISC | FCF_ACVL_DISC);
	spin_unlock_irq(&phba->hbalock);
}

 
void
lpfc_cmf_stop(struct lpfc_hba *phba)
{
	int cpu;
	struct lpfc_cgn_stat *cgs;

	 
	if (!phba->sli4_hba.pc_sli4_params.cmf)
		return;

	lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
			"6221 Stop CMF / Cancel Timer\n");

	 
	hrtimer_cancel(&phba->cmf_stats_timer);
	hrtimer_cancel(&phba->cmf_timer);

	 
	atomic_set(&phba->cmf_busy, 0);
	for_each_present_cpu(cpu) {
		cgs = per_cpu_ptr(phba->cmf_stat, cpu);
		atomic64_set(&cgs->total_bytes, 0);
		atomic64_set(&cgs->rcv_bytes, 0);
		atomic_set(&cgs->rx_io_cnt, 0);
		atomic64_set(&cgs->rx_latency, 0);
	}
	atomic_set(&phba->cmf_bw_wait, 0);

	 
	queue_work(phba->wq, &phba->unblock_request_work);
}

static inline uint64_t
lpfc_get_max_line_rate(struct lpfc_hba *phba)
{
	uint64_t rate = lpfc_sli_port_speed_get(phba);

	return ((((unsigned long)rate) * 1024 * 1024) / 10);
}

void
lpfc_cmf_signal_init(struct lpfc_hba *phba)
{
	lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
			"6223 Signal CMF init\n");

	 
	phba->cmf_interval_rate = LPFC_CMF_INTERVAL;
	phba->cmf_max_line_rate = lpfc_get_max_line_rate(phba);
	phba->cmf_link_byte_count = div_u64(phba->cmf_max_line_rate *
					    phba->cmf_interval_rate, 1000);
	phba->cmf_max_bytes_per_interval = phba->cmf_link_byte_count;

	 
	lpfc_issue_cmf_sync_wqe(phba, 0, 0);
}

 
void
lpfc_cmf_start(struct lpfc_hba *phba)
{
	struct lpfc_cgn_stat *cgs;
	int cpu;

	 
	if (!phba->sli4_hba.pc_sli4_params.cmf ||
	    phba->cmf_active_mode == LPFC_CFG_OFF)
		return;

	 
	lpfc_init_congestion_buf(phba);

	atomic_set(&phba->cgn_fabric_warn_cnt, 0);
	atomic_set(&phba->cgn_fabric_alarm_cnt, 0);
	atomic_set(&phba->cgn_sync_alarm_cnt, 0);
	atomic_set(&phba->cgn_sync_warn_cnt, 0);

	atomic_set(&phba->cmf_busy, 0);
	for_each_present_cpu(cpu) {
		cgs = per_cpu_ptr(phba->cmf_stat, cpu);
		atomic64_set(&cgs->total_bytes, 0);
		atomic64_set(&cgs->rcv_bytes, 0);
		atomic_set(&cgs->rx_io_cnt, 0);
		atomic64_set(&cgs->rx_latency, 0);
	}
	phba->cmf_latency.tv_sec = 0;
	phba->cmf_latency.tv_nsec = 0;

	lpfc_cmf_signal_init(phba);

	lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
			"6222 Start CMF / Timer\n");

	phba->cmf_timer_cnt = 0;
	hrtimer_start(&phba->cmf_timer,
		      ktime_set(0, LPFC_CMF_INTERVAL * NSEC_PER_MSEC),
		      HRTIMER_MODE_REL);
	hrtimer_start(&phba->cmf_stats_timer,
		      ktime_set(0, LPFC_SEC_MIN * NSEC_PER_SEC),
		      HRTIMER_MODE_REL);
	 
	ktime_get_real_ts64(&phba->cmf_latency);

	atomic_set(&phba->cmf_bw_wait, 0);
	atomic_set(&phba->cmf_stop_io, 0);
}

 
void
lpfc_stop_hba_timers(struct lpfc_hba *phba)
{
	if (phba->pport)
		lpfc_stop_vport_timers(phba->pport);
	cancel_delayed_work_sync(&phba->eq_delay_work);
	cancel_delayed_work_sync(&phba->idle_stat_delay_work);
	del_timer_sync(&phba->sli.mbox_tmo);
	del_timer_sync(&phba->fabric_block_timer);
	del_timer_sync(&phba->eratt_poll);
	del_timer_sync(&phba->hb_tmofunc);
	if (phba->sli_rev == LPFC_SLI_REV4) {
		del_timer_sync(&phba->rrq_tmr);
		phba->hba_flag &= ~HBA_RRQ_ACTIVE;
	}
	phba->hba_flag &= ~(HBA_HBEAT_INP | HBA_HBEAT_TMO);

	switch (phba->pci_dev_grp) {
	case LPFC_PCI_DEV_LP:
		 
		del_timer_sync(&phba->fcp_poll_timer);
		break;
	case LPFC_PCI_DEV_OC:
		 
		lpfc_sli4_stop_fcf_redisc_wait_timer(phba);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0297 Invalid device group (x%x)\n",
				phba->pci_dev_grp);
		break;
	}
	return;
}

 
static void
lpfc_block_mgmt_io(struct lpfc_hba *phba, int mbx_action)
{
	unsigned long iflag;
	uint8_t actcmd = MBX_HEARTBEAT;
	unsigned long timeout;

	spin_lock_irqsave(&phba->hbalock, iflag);
	phba->sli.sli_flag |= LPFC_BLOCK_MGMT_IO;
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	if (mbx_action == LPFC_MBX_NO_WAIT)
		return;
	timeout = msecs_to_jiffies(LPFC_MBOX_TMO * 1000) + jiffies;
	spin_lock_irqsave(&phba->hbalock, iflag);
	if (phba->sli.mbox_active) {
		actcmd = phba->sli.mbox_active->u.mb.mbxCommand;
		 
		timeout = msecs_to_jiffies(lpfc_mbox_tmo_val(phba,
				phba->sli.mbox_active) * 1000) + jiffies;
	}
	spin_unlock_irqrestore(&phba->hbalock, iflag);

	 
	while (phba->sli.mbox_active) {
		 
		msleep(2);
		if (time_after(jiffies, timeout)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"2813 Mgmt IO is Blocked %x "
					"- mbox cmd %x still active\n",
					phba->sli.sli_flag, actcmd);
			break;
		}
	}
}

 
void
lpfc_sli4_node_prep(struct lpfc_hba *phba)
{
	struct lpfc_nodelist  *ndlp, *next_ndlp;
	struct lpfc_vport **vports;
	int i, rpi;

	if (phba->sli_rev != LPFC_SLI_REV4)
		return;

	vports = lpfc_create_vport_work_array(phba);
	if (vports == NULL)
		return;

	for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
		if (vports[i]->load_flag & FC_UNLOADING)
			continue;

		list_for_each_entry_safe(ndlp, next_ndlp,
					 &vports[i]->fc_nodes,
					 nlp_listp) {
			rpi = lpfc_sli4_alloc_rpi(phba);
			if (rpi == LPFC_RPI_ALLOC_ERROR) {
				 
				continue;
			}
			ndlp->nlp_rpi = rpi;
			lpfc_printf_vlog(ndlp->vport, KERN_INFO,
					 LOG_NODE | LOG_DISCOVERY,
					 "0009 Assign RPI x%x to ndlp x%px "
					 "DID:x%06x flg:x%x\n",
					 ndlp->nlp_rpi, ndlp, ndlp->nlp_DID,
					 ndlp->nlp_flag);
		}
	}
	lpfc_destroy_vport_work_array(phba, vports);
}

 
static void lpfc_create_expedite_pool(struct lpfc_hba *phba)
{
	struct lpfc_sli4_hdw_queue *qp;
	struct lpfc_io_buf *lpfc_ncmd;
	struct lpfc_io_buf *lpfc_ncmd_next;
	struct lpfc_epd_pool *epd_pool;
	unsigned long iflag;

	epd_pool = &phba->epd_pool;
	qp = &phba->sli4_hba.hdwq[0];

	spin_lock_init(&epd_pool->lock);
	spin_lock_irqsave(&qp->io_buf_list_put_lock, iflag);
	spin_lock(&epd_pool->lock);
	INIT_LIST_HEAD(&epd_pool->list);
	list_for_each_entry_safe(lpfc_ncmd, lpfc_ncmd_next,
				 &qp->lpfc_io_buf_list_put, list) {
		list_move_tail(&lpfc_ncmd->list, &epd_pool->list);
		lpfc_ncmd->expedite = true;
		qp->put_io_bufs--;
		epd_pool->count++;
		if (epd_pool->count >= XRI_BATCH)
			break;
	}
	spin_unlock(&epd_pool->lock);
	spin_unlock_irqrestore(&qp->io_buf_list_put_lock, iflag);
}

 
static void lpfc_destroy_expedite_pool(struct lpfc_hba *phba)
{
	struct lpfc_sli4_hdw_queue *qp;
	struct lpfc_io_buf *lpfc_ncmd;
	struct lpfc_io_buf *lpfc_ncmd_next;
	struct lpfc_epd_pool *epd_pool;
	unsigned long iflag;

	epd_pool = &phba->epd_pool;
	qp = &phba->sli4_hba.hdwq[0];

	spin_lock_irqsave(&qp->io_buf_list_put_lock, iflag);
	spin_lock(&epd_pool->lock);
	list_for_each_entry_safe(lpfc_ncmd, lpfc_ncmd_next,
				 &epd_pool->list, list) {
		list_move_tail(&lpfc_ncmd->list,
			       &qp->lpfc_io_buf_list_put);
		lpfc_ncmd->flags = false;
		qp->put_io_bufs++;
		epd_pool->count--;
	}
	spin_unlock(&epd_pool->lock);
	spin_unlock_irqrestore(&qp->io_buf_list_put_lock, iflag);
}

 
void lpfc_create_multixri_pools(struct lpfc_hba *phba)
{
	u32 i, j;
	u32 hwq_count;
	u32 count_per_hwq;
	struct lpfc_io_buf *lpfc_ncmd;
	struct lpfc_io_buf *lpfc_ncmd_next;
	unsigned long iflag;
	struct lpfc_sli4_hdw_queue *qp;
	struct lpfc_multixri_pool *multixri_pool;
	struct lpfc_pbl_pool *pbl_pool;
	struct lpfc_pvt_pool *pvt_pool;

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"1234 num_hdw_queue=%d num_present_cpu=%d common_xri_cnt=%d\n",
			phba->cfg_hdw_queue, phba->sli4_hba.num_present_cpu,
			phba->sli4_hba.io_xri_cnt);

	if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME)
		lpfc_create_expedite_pool(phba);

	hwq_count = phba->cfg_hdw_queue;
	count_per_hwq = phba->sli4_hba.io_xri_cnt / hwq_count;

	for (i = 0; i < hwq_count; i++) {
		multixri_pool = kzalloc(sizeof(*multixri_pool), GFP_KERNEL);

		if (!multixri_pool) {
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"1238 Failed to allocate memory for "
					"multixri_pool\n");

			if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME)
				lpfc_destroy_expedite_pool(phba);

			j = 0;
			while (j < i) {
				qp = &phba->sli4_hba.hdwq[j];
				kfree(qp->p_multixri_pool);
				j++;
			}
			phba->cfg_xri_rebalancing = 0;
			return;
		}

		qp = &phba->sli4_hba.hdwq[i];
		qp->p_multixri_pool = multixri_pool;

		multixri_pool->xri_limit = count_per_hwq;
		multixri_pool->rrb_next_hwqid = i;

		 
		pbl_pool = &multixri_pool->pbl_pool;
		spin_lock_init(&pbl_pool->lock);
		spin_lock_irqsave(&qp->io_buf_list_put_lock, iflag);
		spin_lock(&pbl_pool->lock);
		INIT_LIST_HEAD(&pbl_pool->list);
		list_for_each_entry_safe(lpfc_ncmd, lpfc_ncmd_next,
					 &qp->lpfc_io_buf_list_put, list) {
			list_move_tail(&lpfc_ncmd->list, &pbl_pool->list);
			qp->put_io_bufs--;
			pbl_pool->count++;
		}
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"1235 Moved %d buffers from PUT list over to pbl_pool[%d]\n",
				pbl_pool->count, i);
		spin_unlock(&pbl_pool->lock);
		spin_unlock_irqrestore(&qp->io_buf_list_put_lock, iflag);

		 
		pvt_pool = &multixri_pool->pvt_pool;
		pvt_pool->high_watermark = multixri_pool->xri_limit / 2;
		pvt_pool->low_watermark = XRI_BATCH;
		spin_lock_init(&pvt_pool->lock);
		spin_lock_irqsave(&pvt_pool->lock, iflag);
		INIT_LIST_HEAD(&pvt_pool->list);
		pvt_pool->count = 0;
		spin_unlock_irqrestore(&pvt_pool->lock, iflag);
	}
}

 
static void lpfc_destroy_multixri_pools(struct lpfc_hba *phba)
{
	u32 i;
	u32 hwq_count;
	struct lpfc_io_buf *lpfc_ncmd;
	struct lpfc_io_buf *lpfc_ncmd_next;
	unsigned long iflag;
	struct lpfc_sli4_hdw_queue *qp;
	struct lpfc_multixri_pool *multixri_pool;
	struct lpfc_pbl_pool *pbl_pool;
	struct lpfc_pvt_pool *pvt_pool;

	if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME)
		lpfc_destroy_expedite_pool(phba);

	if (!(phba->pport->load_flag & FC_UNLOADING))
		lpfc_sli_flush_io_rings(phba);

	hwq_count = phba->cfg_hdw_queue;

	for (i = 0; i < hwq_count; i++) {
		qp = &phba->sli4_hba.hdwq[i];
		multixri_pool = qp->p_multixri_pool;
		if (!multixri_pool)
			continue;

		qp->p_multixri_pool = NULL;

		spin_lock_irqsave(&qp->io_buf_list_put_lock, iflag);

		 
		pbl_pool = &multixri_pool->pbl_pool;
		spin_lock(&pbl_pool->lock);

		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"1236 Moving %d buffers from pbl_pool[%d] TO PUT list\n",
				pbl_pool->count, i);

		list_for_each_entry_safe(lpfc_ncmd, lpfc_ncmd_next,
					 &pbl_pool->list, list) {
			list_move_tail(&lpfc_ncmd->list,
				       &qp->lpfc_io_buf_list_put);
			qp->put_io_bufs++;
			pbl_pool->count--;
		}

		INIT_LIST_HEAD(&pbl_pool->list);
		pbl_pool->count = 0;

		spin_unlock(&pbl_pool->lock);

		 
		pvt_pool = &multixri_pool->pvt_pool;
		spin_lock(&pvt_pool->lock);

		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"1237 Moving %d buffers from pvt_pool[%d] TO PUT list\n",
				pvt_pool->count, i);

		list_for_each_entry_safe(lpfc_ncmd, lpfc_ncmd_next,
					 &pvt_pool->list, list) {
			list_move_tail(&lpfc_ncmd->list,
				       &qp->lpfc_io_buf_list_put);
			qp->put_io_bufs++;
			pvt_pool->count--;
		}

		INIT_LIST_HEAD(&pvt_pool->list);
		pvt_pool->count = 0;

		spin_unlock(&pvt_pool->lock);
		spin_unlock_irqrestore(&qp->io_buf_list_put_lock, iflag);

		kfree(multixri_pool);
	}
}

 
int
lpfc_online(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport;
	struct lpfc_vport **vports;
	int i, error = 0;
	bool vpis_cleared = false;

	if (!phba)
		return 0;
	vport = phba->pport;

	if (!(vport->fc_flag & FC_OFFLINE_MODE))
		return 0;

	lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
			"0458 Bring Adapter online\n");

	lpfc_block_mgmt_io(phba, LPFC_MBX_WAIT);

	if (phba->sli_rev == LPFC_SLI_REV4) {
		if (lpfc_sli4_hba_setup(phba)) {  
			lpfc_unblock_mgmt_io(phba);
			return 1;
		}
		spin_lock_irq(&phba->hbalock);
		if (!phba->sli4_hba.max_cfg_param.vpi_used)
			vpis_cleared = true;
		spin_unlock_irq(&phba->hbalock);

		 
		if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME &&
				!phba->nvmet_support) {
			error = lpfc_nvme_create_localport(phba->pport);
			if (error)
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"6132 NVME restore reg failed "
					"on nvmei error x%x\n", error);
		}
	} else {
		lpfc_sli_queue_init(phba);
		if (lpfc_sli_hba_setup(phba)) {	 
			lpfc_unblock_mgmt_io(phba);
			return 1;
		}
	}

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL) {
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			struct Scsi_Host *shost;
			shost = lpfc_shost_from_vport(vports[i]);
			spin_lock_irq(shost->host_lock);
			vports[i]->fc_flag &= ~FC_OFFLINE_MODE;
			if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED)
				vports[i]->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
			if (phba->sli_rev == LPFC_SLI_REV4) {
				vports[i]->fc_flag |= FC_VPORT_NEEDS_INIT_VPI;
				if ((vpis_cleared) &&
				    (vports[i]->port_type !=
					LPFC_PHYSICAL_PORT))
					vports[i]->vpi = 0;
			}
			spin_unlock_irq(shost->host_lock);
		}
	}
	lpfc_destroy_vport_work_array(phba, vports);

	if (phba->cfg_xri_rebalancing)
		lpfc_create_multixri_pools(phba);

	lpfc_cpuhp_add(phba);

	lpfc_unblock_mgmt_io(phba);
	return 0;
}

 
void
lpfc_unblock_mgmt_io(struct lpfc_hba * phba)
{
	unsigned long iflag;

	spin_lock_irqsave(&phba->hbalock, iflag);
	phba->sli.sli_flag &= ~LPFC_BLOCK_MGMT_IO;
	spin_unlock_irqrestore(&phba->hbalock, iflag);
}

 
void
lpfc_offline_prep(struct lpfc_hba *phba, int mbx_action)
{
	struct lpfc_vport *vport = phba->pport;
	struct lpfc_nodelist  *ndlp, *next_ndlp;
	struct lpfc_vport **vports;
	struct Scsi_Host *shost;
	int i;
	int offline;
	bool hba_pci_err;

	if (vport->fc_flag & FC_OFFLINE_MODE)
		return;

	lpfc_block_mgmt_io(phba, mbx_action);

	lpfc_linkdown(phba);

	offline =  pci_channel_offline(phba->pcidev);
	hba_pci_err = test_bit(HBA_PCI_ERR, &phba->bit_flags);

	 
	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL) {
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			if (vports[i]->load_flag & FC_UNLOADING)
				continue;
			shost = lpfc_shost_from_vport(vports[i]);
			spin_lock_irq(shost->host_lock);
			vports[i]->vpi_state &= ~LPFC_VPI_REGISTERED;
			vports[i]->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
			vports[i]->fc_flag &= ~FC_VFI_REGISTERED;
			spin_unlock_irq(shost->host_lock);

			shost =	lpfc_shost_from_vport(vports[i]);
			list_for_each_entry_safe(ndlp, next_ndlp,
						 &vports[i]->fc_nodes,
						 nlp_listp) {

				spin_lock_irq(&ndlp->lock);
				ndlp->nlp_flag &= ~NLP_NPR_ADISC;
				spin_unlock_irq(&ndlp->lock);

				if (offline || hba_pci_err) {
					spin_lock_irq(&ndlp->lock);
					ndlp->nlp_flag &= ~(NLP_UNREG_INP |
							    NLP_RPI_REGISTERED);
					spin_unlock_irq(&ndlp->lock);
					if (phba->sli_rev == LPFC_SLI_REV4)
						lpfc_sli_rpi_release(vports[i],
								     ndlp);
				} else {
					lpfc_unreg_rpi(vports[i], ndlp);
				}
				 
				if (phba->sli_rev == LPFC_SLI_REV4) {
					lpfc_printf_vlog(vports[i], KERN_INFO,
						 LOG_NODE | LOG_DISCOVERY,
						 "0011 Free RPI x%x on "
						 "ndlp: x%px did x%x\n",
						 ndlp->nlp_rpi, ndlp,
						 ndlp->nlp_DID);
					lpfc_sli4_free_rpi(phba, ndlp->nlp_rpi);
					ndlp->nlp_rpi = LPFC_RPI_ALLOC_ERROR;
				}

				if (ndlp->nlp_type & NLP_FABRIC) {
					lpfc_disc_state_machine(vports[i], ndlp,
						NULL, NLP_EVT_DEVICE_RECOVERY);

					 
					if (!(ndlp->save_flags &
					      NLP_IN_RECOV_POST_DEV_LOSS) &&
					    !(ndlp->fc4_xpt_flags &
					      (NVME_XPT_REGD | SCSI_XPT_REGD)))
						lpfc_disc_state_machine
							(vports[i], ndlp,
							 NULL,
							 NLP_EVT_DEVICE_RM);
				}
			}
		}
	}
	lpfc_destroy_vport_work_array(phba, vports);

	lpfc_sli_mbox_sys_shutdown(phba, mbx_action);

	if (phba->wq)
		flush_workqueue(phba->wq);
}

 
void
lpfc_offline(struct lpfc_hba *phba)
{
	struct Scsi_Host  *shost;
	struct lpfc_vport **vports;
	int i;

	if (phba->pport->fc_flag & FC_OFFLINE_MODE)
		return;

	 
	lpfc_stop_port(phba);

	 
	lpfc_nvmet_destroy_targetport(phba);
	lpfc_nvme_destroy_localport(phba->pport);

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++)
			lpfc_stop_vport_timers(vports[i]);
	lpfc_destroy_vport_work_array(phba, vports);
	lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
			"0460 Bring Adapter offline\n");
	 
	lpfc_sli_hba_down(phba);
	spin_lock_irq(&phba->hbalock);
	phba->work_ha = 0;
	spin_unlock_irq(&phba->hbalock);
	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			shost = lpfc_shost_from_vport(vports[i]);
			spin_lock_irq(shost->host_lock);
			vports[i]->work_port_events = 0;
			vports[i]->fc_flag |= FC_OFFLINE_MODE;
			spin_unlock_irq(shost->host_lock);
		}
	lpfc_destroy_vport_work_array(phba, vports);
	 
	if (phba->pport->fc_flag & FC_OFFLINE_MODE)
		__lpfc_cpuhp_remove(phba);

	if (phba->cfg_xri_rebalancing)
		lpfc_destroy_multixri_pools(phba);
}

 
static void
lpfc_scsi_free(struct lpfc_hba *phba)
{
	struct lpfc_io_buf *sb, *sb_next;

	if (!(phba->cfg_enable_fc4_type & LPFC_ENABLE_FCP))
		return;

	spin_lock_irq(&phba->hbalock);

	 

	spin_lock(&phba->scsi_buf_list_put_lock);
	list_for_each_entry_safe(sb, sb_next, &phba->lpfc_scsi_buf_list_put,
				 list) {
		list_del(&sb->list);
		dma_pool_free(phba->lpfc_sg_dma_buf_pool, sb->data,
			      sb->dma_handle);
		kfree(sb);
		phba->total_scsi_bufs--;
	}
	spin_unlock(&phba->scsi_buf_list_put_lock);

	spin_lock(&phba->scsi_buf_list_get_lock);
	list_for_each_entry_safe(sb, sb_next, &phba->lpfc_scsi_buf_list_get,
				 list) {
		list_del(&sb->list);
		dma_pool_free(phba->lpfc_sg_dma_buf_pool, sb->data,
			      sb->dma_handle);
		kfree(sb);
		phba->total_scsi_bufs--;
	}
	spin_unlock(&phba->scsi_buf_list_get_lock);
	spin_unlock_irq(&phba->hbalock);
}

 
void
lpfc_io_free(struct lpfc_hba *phba)
{
	struct lpfc_io_buf *lpfc_ncmd, *lpfc_ncmd_next;
	struct lpfc_sli4_hdw_queue *qp;
	int idx;

	for (idx = 0; idx < phba->cfg_hdw_queue; idx++) {
		qp = &phba->sli4_hba.hdwq[idx];
		 
		spin_lock(&qp->io_buf_list_put_lock);
		list_for_each_entry_safe(lpfc_ncmd, lpfc_ncmd_next,
					 &qp->lpfc_io_buf_list_put,
					 list) {
			list_del(&lpfc_ncmd->list);
			qp->put_io_bufs--;
			dma_pool_free(phba->lpfc_sg_dma_buf_pool,
				      lpfc_ncmd->data, lpfc_ncmd->dma_handle);
			if (phba->cfg_xpsgl && !phba->nvmet_support)
				lpfc_put_sgl_per_hdwq(phba, lpfc_ncmd);
			lpfc_put_cmd_rsp_buf_per_hdwq(phba, lpfc_ncmd);
			kfree(lpfc_ncmd);
			qp->total_io_bufs--;
		}
		spin_unlock(&qp->io_buf_list_put_lock);

		spin_lock(&qp->io_buf_list_get_lock);
		list_for_each_entry_safe(lpfc_ncmd, lpfc_ncmd_next,
					 &qp->lpfc_io_buf_list_get,
					 list) {
			list_del(&lpfc_ncmd->list);
			qp->get_io_bufs--;
			dma_pool_free(phba->lpfc_sg_dma_buf_pool,
				      lpfc_ncmd->data, lpfc_ncmd->dma_handle);
			if (phba->cfg_xpsgl && !phba->nvmet_support)
				lpfc_put_sgl_per_hdwq(phba, lpfc_ncmd);
			lpfc_put_cmd_rsp_buf_per_hdwq(phba, lpfc_ncmd);
			kfree(lpfc_ncmd);
			qp->total_io_bufs--;
		}
		spin_unlock(&qp->io_buf_list_get_lock);
	}
}

 
int
lpfc_sli4_els_sgl_update(struct lpfc_hba *phba)
{
	struct lpfc_sglq *sglq_entry = NULL, *sglq_entry_next = NULL;
	uint16_t i, lxri, xri_cnt, els_xri_cnt;
	LIST_HEAD(els_sgl_list);
	int rc;

	 
	els_xri_cnt = lpfc_sli4_get_els_iocb_cnt(phba);

	if (els_xri_cnt > phba->sli4_hba.els_xri_cnt) {
		 
		xri_cnt = els_xri_cnt - phba->sli4_hba.els_xri_cnt;
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"3157 ELS xri-sgl count increased from "
				"%d to %d\n", phba->sli4_hba.els_xri_cnt,
				els_xri_cnt);
		 
		for (i = 0; i < xri_cnt; i++) {
			sglq_entry = kzalloc(sizeof(struct lpfc_sglq),
					     GFP_KERNEL);
			if (sglq_entry == NULL) {
				lpfc_printf_log(phba, KERN_ERR,
						LOG_TRACE_EVENT,
						"2562 Failure to allocate an "
						"ELS sgl entry:%d\n", i);
				rc = -ENOMEM;
				goto out_free_mem;
			}
			sglq_entry->buff_type = GEN_BUFF_TYPE;
			sglq_entry->virt = lpfc_mbuf_alloc(phba, 0,
							   &sglq_entry->phys);
			if (sglq_entry->virt == NULL) {
				kfree(sglq_entry);
				lpfc_printf_log(phba, KERN_ERR,
						LOG_TRACE_EVENT,
						"2563 Failure to allocate an "
						"ELS mbuf:%d\n", i);
				rc = -ENOMEM;
				goto out_free_mem;
			}
			sglq_entry->sgl = sglq_entry->virt;
			memset(sglq_entry->sgl, 0, LPFC_BPL_SIZE);
			sglq_entry->state = SGL_FREED;
			list_add_tail(&sglq_entry->list, &els_sgl_list);
		}
		spin_lock_irq(&phba->sli4_hba.sgl_list_lock);
		list_splice_init(&els_sgl_list,
				 &phba->sli4_hba.lpfc_els_sgl_list);
		spin_unlock_irq(&phba->sli4_hba.sgl_list_lock);
	} else if (els_xri_cnt < phba->sli4_hba.els_xri_cnt) {
		 
		xri_cnt = phba->sli4_hba.els_xri_cnt - els_xri_cnt;
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"3158 ELS xri-sgl count decreased from "
				"%d to %d\n", phba->sli4_hba.els_xri_cnt,
				els_xri_cnt);
		spin_lock_irq(&phba->sli4_hba.sgl_list_lock);
		list_splice_init(&phba->sli4_hba.lpfc_els_sgl_list,
				 &els_sgl_list);
		 
		for (i = 0; i < xri_cnt; i++) {
			list_remove_head(&els_sgl_list,
					 sglq_entry, struct lpfc_sglq, list);
			if (sglq_entry) {
				__lpfc_mbuf_free(phba, sglq_entry->virt,
						 sglq_entry->phys);
				kfree(sglq_entry);
			}
		}
		list_splice_init(&els_sgl_list,
				 &phba->sli4_hba.lpfc_els_sgl_list);
		spin_unlock_irq(&phba->sli4_hba.sgl_list_lock);
	} else
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"3163 ELS xri-sgl count unchanged: %d\n",
				els_xri_cnt);
	phba->sli4_hba.els_xri_cnt = els_xri_cnt;

	 
	sglq_entry = NULL;
	sglq_entry_next = NULL;
	list_for_each_entry_safe(sglq_entry, sglq_entry_next,
				 &phba->sli4_hba.lpfc_els_sgl_list, list) {
		lxri = lpfc_sli4_next_xritag(phba);
		if (lxri == NO_XRI) {
			lpfc_printf_log(phba, KERN_ERR,
					LOG_TRACE_EVENT,
					"2400 Failed to allocate xri for "
					"ELS sgl\n");
			rc = -ENOMEM;
			goto out_free_mem;
		}
		sglq_entry->sli4_lxritag = lxri;
		sglq_entry->sli4_xritag = phba->sli4_hba.xri_ids[lxri];
	}
	return 0;

out_free_mem:
	lpfc_free_els_sgl_list(phba);
	return rc;
}

 
int
lpfc_sli4_nvmet_sgl_update(struct lpfc_hba *phba)
{
	struct lpfc_sglq *sglq_entry = NULL, *sglq_entry_next = NULL;
	uint16_t i, lxri, xri_cnt, els_xri_cnt;
	uint16_t nvmet_xri_cnt;
	LIST_HEAD(nvmet_sgl_list);
	int rc;

	 
	els_xri_cnt = lpfc_sli4_get_els_iocb_cnt(phba);

	 
	nvmet_xri_cnt = phba->sli4_hba.max_cfg_param.max_xri - els_xri_cnt;
	if (nvmet_xri_cnt > phba->sli4_hba.nvmet_xri_cnt) {
		 
		xri_cnt = nvmet_xri_cnt - phba->sli4_hba.nvmet_xri_cnt;
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"6302 NVMET xri-sgl cnt grew from %d to %d\n",
				phba->sli4_hba.nvmet_xri_cnt, nvmet_xri_cnt);
		 
		for (i = 0; i < xri_cnt; i++) {
			sglq_entry = kzalloc(sizeof(struct lpfc_sglq),
					     GFP_KERNEL);
			if (sglq_entry == NULL) {
				lpfc_printf_log(phba, KERN_ERR,
						LOG_TRACE_EVENT,
						"6303 Failure to allocate an "
						"NVMET sgl entry:%d\n", i);
				rc = -ENOMEM;
				goto out_free_mem;
			}
			sglq_entry->buff_type = NVMET_BUFF_TYPE;
			sglq_entry->virt = lpfc_nvmet_buf_alloc(phba, 0,
							   &sglq_entry->phys);
			if (sglq_entry->virt == NULL) {
				kfree(sglq_entry);
				lpfc_printf_log(phba, KERN_ERR,
						LOG_TRACE_EVENT,
						"6304 Failure to allocate an "
						"NVMET buf:%d\n", i);
				rc = -ENOMEM;
				goto out_free_mem;
			}
			sglq_entry->sgl = sglq_entry->virt;
			memset(sglq_entry->sgl, 0,
			       phba->cfg_sg_dma_buf_size);
			sglq_entry->state = SGL_FREED;
			list_add_tail(&sglq_entry->list, &nvmet_sgl_list);
		}
		spin_lock_irq(&phba->hbalock);
		spin_lock(&phba->sli4_hba.sgl_list_lock);
		list_splice_init(&nvmet_sgl_list,
				 &phba->sli4_hba.lpfc_nvmet_sgl_list);
		spin_unlock(&phba->sli4_hba.sgl_list_lock);
		spin_unlock_irq(&phba->hbalock);
	} else if (nvmet_xri_cnt < phba->sli4_hba.nvmet_xri_cnt) {
		 
		xri_cnt = phba->sli4_hba.nvmet_xri_cnt - nvmet_xri_cnt;
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"6305 NVMET xri-sgl count decreased from "
				"%d to %d\n", phba->sli4_hba.nvmet_xri_cnt,
				nvmet_xri_cnt);
		spin_lock_irq(&phba->hbalock);
		spin_lock(&phba->sli4_hba.sgl_list_lock);
		list_splice_init(&phba->sli4_hba.lpfc_nvmet_sgl_list,
				 &nvmet_sgl_list);
		 
		for (i = 0; i < xri_cnt; i++) {
			list_remove_head(&nvmet_sgl_list,
					 sglq_entry, struct lpfc_sglq, list);
			if (sglq_entry) {
				lpfc_nvmet_buf_free(phba, sglq_entry->virt,
						    sglq_entry->phys);
				kfree(sglq_entry);
			}
		}
		list_splice_init(&nvmet_sgl_list,
				 &phba->sli4_hba.lpfc_nvmet_sgl_list);
		spin_unlock(&phba->sli4_hba.sgl_list_lock);
		spin_unlock_irq(&phba->hbalock);
	} else
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"6306 NVMET xri-sgl count unchanged: %d\n",
				nvmet_xri_cnt);
	phba->sli4_hba.nvmet_xri_cnt = nvmet_xri_cnt;

	 
	sglq_entry = NULL;
	sglq_entry_next = NULL;
	list_for_each_entry_safe(sglq_entry, sglq_entry_next,
				 &phba->sli4_hba.lpfc_nvmet_sgl_list, list) {
		lxri = lpfc_sli4_next_xritag(phba);
		if (lxri == NO_XRI) {
			lpfc_printf_log(phba, KERN_ERR,
					LOG_TRACE_EVENT,
					"6307 Failed to allocate xri for "
					"NVMET sgl\n");
			rc = -ENOMEM;
			goto out_free_mem;
		}
		sglq_entry->sli4_lxritag = lxri;
		sglq_entry->sli4_xritag = phba->sli4_hba.xri_ids[lxri];
	}
	return 0;

out_free_mem:
	lpfc_free_nvmet_sgl_list(phba);
	return rc;
}

int
lpfc_io_buf_flush(struct lpfc_hba *phba, struct list_head *cbuf)
{
	LIST_HEAD(blist);
	struct lpfc_sli4_hdw_queue *qp;
	struct lpfc_io_buf *lpfc_cmd;
	struct lpfc_io_buf *iobufp, *prev_iobufp;
	int idx, cnt, xri, inserted;

	cnt = 0;
	for (idx = 0; idx < phba->cfg_hdw_queue; idx++) {
		qp = &phba->sli4_hba.hdwq[idx];
		spin_lock_irq(&qp->io_buf_list_get_lock);
		spin_lock(&qp->io_buf_list_put_lock);

		 
		list_splice_init(&qp->lpfc_io_buf_list_get, &blist);
		list_splice(&qp->lpfc_io_buf_list_put, &blist);
		INIT_LIST_HEAD(&qp->lpfc_io_buf_list_get);
		INIT_LIST_HEAD(&qp->lpfc_io_buf_list_put);
		cnt += qp->get_io_bufs + qp->put_io_bufs;
		qp->get_io_bufs = 0;
		qp->put_io_bufs = 0;
		qp->total_io_bufs = 0;
		spin_unlock(&qp->io_buf_list_put_lock);
		spin_unlock_irq(&qp->io_buf_list_get_lock);
	}

	 
	for (idx = 0; idx < cnt; idx++) {
		list_remove_head(&blist, lpfc_cmd, struct lpfc_io_buf, list);
		if (!lpfc_cmd)
			return cnt;
		if (idx == 0) {
			list_add_tail(&lpfc_cmd->list, cbuf);
			continue;
		}
		xri = lpfc_cmd->cur_iocbq.sli4_xritag;
		inserted = 0;
		prev_iobufp = NULL;
		list_for_each_entry(iobufp, cbuf, list) {
			if (xri < iobufp->cur_iocbq.sli4_xritag) {
				if (prev_iobufp)
					list_add(&lpfc_cmd->list,
						 &prev_iobufp->list);
				else
					list_add(&lpfc_cmd->list, cbuf);
				inserted = 1;
				break;
			}
			prev_iobufp = iobufp;
		}
		if (!inserted)
			list_add_tail(&lpfc_cmd->list, cbuf);
	}
	return cnt;
}

int
lpfc_io_buf_replenish(struct lpfc_hba *phba, struct list_head *cbuf)
{
	struct lpfc_sli4_hdw_queue *qp;
	struct lpfc_io_buf *lpfc_cmd;
	int idx, cnt;
	unsigned long iflags;

	qp = phba->sli4_hba.hdwq;
	cnt = 0;
	while (!list_empty(cbuf)) {
		for (idx = 0; idx < phba->cfg_hdw_queue; idx++) {
			list_remove_head(cbuf, lpfc_cmd,
					 struct lpfc_io_buf, list);
			if (!lpfc_cmd)
				return cnt;
			cnt++;
			qp = &phba->sli4_hba.hdwq[idx];
			lpfc_cmd->hdwq_no = idx;
			lpfc_cmd->hdwq = qp;
			lpfc_cmd->cur_iocbq.cmd_cmpl = NULL;
			spin_lock_irqsave(&qp->io_buf_list_put_lock, iflags);
			list_add_tail(&lpfc_cmd->list,
				      &qp->lpfc_io_buf_list_put);
			qp->put_io_bufs++;
			qp->total_io_bufs++;
			spin_unlock_irqrestore(&qp->io_buf_list_put_lock,
					       iflags);
		}
	}
	return cnt;
}

 
int
lpfc_sli4_io_sgl_update(struct lpfc_hba *phba)
{
	struct lpfc_io_buf *lpfc_ncmd = NULL, *lpfc_ncmd_next = NULL;
	uint16_t i, lxri, els_xri_cnt;
	uint16_t io_xri_cnt, io_xri_max;
	LIST_HEAD(io_sgl_list);
	int rc, cnt;

	 

	 
	els_xri_cnt = lpfc_sli4_get_els_iocb_cnt(phba);
	io_xri_max = phba->sli4_hba.max_cfg_param.max_xri - els_xri_cnt;
	phba->sli4_hba.io_xri_max = io_xri_max;

	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"6074 Current allocated XRI sgl count:%d, "
			"maximum XRI count:%d els_xri_cnt:%d\n\n",
			phba->sli4_hba.io_xri_cnt,
			phba->sli4_hba.io_xri_max,
			els_xri_cnt);

	cnt = lpfc_io_buf_flush(phba, &io_sgl_list);

	if (phba->sli4_hba.io_xri_cnt > phba->sli4_hba.io_xri_max) {
		 
		io_xri_cnt = phba->sli4_hba.io_xri_cnt -
					phba->sli4_hba.io_xri_max;
		 
		for (i = 0; i < io_xri_cnt; i++) {
			list_remove_head(&io_sgl_list, lpfc_ncmd,
					 struct lpfc_io_buf, list);
			if (lpfc_ncmd) {
				dma_pool_free(phba->lpfc_sg_dma_buf_pool,
					      lpfc_ncmd->data,
					      lpfc_ncmd->dma_handle);
				kfree(lpfc_ncmd);
			}
		}
		phba->sli4_hba.io_xri_cnt -= io_xri_cnt;
	}

	 
	lpfc_ncmd = NULL;
	lpfc_ncmd_next = NULL;
	phba->sli4_hba.io_xri_cnt = cnt;
	list_for_each_entry_safe(lpfc_ncmd, lpfc_ncmd_next,
				 &io_sgl_list, list) {
		lxri = lpfc_sli4_next_xritag(phba);
		if (lxri == NO_XRI) {
			lpfc_printf_log(phba, KERN_ERR,
					LOG_TRACE_EVENT,
					"6075 Failed to allocate xri for "
					"nvme buffer\n");
			rc = -ENOMEM;
			goto out_free_mem;
		}
		lpfc_ncmd->cur_iocbq.sli4_lxritag = lxri;
		lpfc_ncmd->cur_iocbq.sli4_xritag = phba->sli4_hba.xri_ids[lxri];
	}
	cnt = lpfc_io_buf_replenish(phba, &io_sgl_list);
	return 0;

out_free_mem:
	lpfc_io_free(phba);
	return rc;
}

 
int
lpfc_new_io_buf(struct lpfc_hba *phba, int num_to_alloc)
{
	struct lpfc_io_buf *lpfc_ncmd;
	struct lpfc_iocbq *pwqeq;
	uint16_t iotag, lxri = 0;
	int bcnt, num_posted;
	LIST_HEAD(prep_nblist);
	LIST_HEAD(post_nblist);
	LIST_HEAD(nvme_nblist);

	phba->sli4_hba.io_xri_cnt = 0;
	for (bcnt = 0; bcnt < num_to_alloc; bcnt++) {
		lpfc_ncmd = kzalloc(sizeof(*lpfc_ncmd), GFP_KERNEL);
		if (!lpfc_ncmd)
			break;
		 
		lpfc_ncmd->data = dma_pool_zalloc(phba->lpfc_sg_dma_buf_pool,
						  GFP_KERNEL,
						  &lpfc_ncmd->dma_handle);
		if (!lpfc_ncmd->data) {
			kfree(lpfc_ncmd);
			break;
		}

		if (phba->cfg_xpsgl && !phba->nvmet_support) {
			INIT_LIST_HEAD(&lpfc_ncmd->dma_sgl_xtra_list);
		} else {
			 
			if ((phba->sli3_options & LPFC_SLI3_BG_ENABLED) &&
			    (((unsigned long)(lpfc_ncmd->data) &
			    (unsigned long)(SLI4_PAGE_SIZE - 1)) != 0)) {
				lpfc_printf_log(phba, KERN_ERR,
						LOG_TRACE_EVENT,
						"3369 Memory alignment err: "
						"addr=%lx\n",
						(unsigned long)lpfc_ncmd->data);
				dma_pool_free(phba->lpfc_sg_dma_buf_pool,
					      lpfc_ncmd->data,
					      lpfc_ncmd->dma_handle);
				kfree(lpfc_ncmd);
				break;
			}
		}

		INIT_LIST_HEAD(&lpfc_ncmd->dma_cmd_rsp_list);

		lxri = lpfc_sli4_next_xritag(phba);
		if (lxri == NO_XRI) {
			dma_pool_free(phba->lpfc_sg_dma_buf_pool,
				      lpfc_ncmd->data, lpfc_ncmd->dma_handle);
			kfree(lpfc_ncmd);
			break;
		}
		pwqeq = &lpfc_ncmd->cur_iocbq;

		 
		iotag = lpfc_sli_next_iotag(phba, pwqeq);
		if (iotag == 0) {
			dma_pool_free(phba->lpfc_sg_dma_buf_pool,
				      lpfc_ncmd->data, lpfc_ncmd->dma_handle);
			kfree(lpfc_ncmd);
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"6121 Failed to allocate IOTAG for"
					" XRI:0x%x\n", lxri);
			lpfc_sli4_free_xri(phba, lxri);
			break;
		}
		pwqeq->sli4_lxritag = lxri;
		pwqeq->sli4_xritag = phba->sli4_hba.xri_ids[lxri];

		 
		lpfc_ncmd->dma_sgl = lpfc_ncmd->data;
		lpfc_ncmd->dma_phys_sgl = lpfc_ncmd->dma_handle;
		lpfc_ncmd->cur_iocbq.io_buf = lpfc_ncmd;
		spin_lock_init(&lpfc_ncmd->buf_lock);

		 
		list_add_tail(&lpfc_ncmd->list, &post_nblist);
		phba->sli4_hba.io_xri_cnt++;
	}
	lpfc_printf_log(phba, KERN_INFO, LOG_NVME,
			"6114 Allocate %d out of %d requested new NVME "
			"buffers of size x%zu bytes\n", bcnt, num_to_alloc,
			sizeof(*lpfc_ncmd));


	 
	if (!list_empty(&post_nblist))
		num_posted = lpfc_sli4_post_io_sgl_list(
				phba, &post_nblist, bcnt);
	else
		num_posted = 0;

	return num_posted;
}

static uint64_t
lpfc_get_wwpn(struct lpfc_hba *phba)
{
	uint64_t wwn;
	int rc;
	LPFC_MBOXQ_t *mboxq;
	MAILBOX_t *mb;

	mboxq = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool,
						GFP_KERNEL);
	if (!mboxq)
		return (uint64_t)-1;

	 
	lpfc_read_nv(phba, mboxq);
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	if (rc != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"6019 Mailbox failed , mbxCmd x%x "
				"READ_NV, mbxStatus x%x\n",
				bf_get(lpfc_mqe_command, &mboxq->u.mqe),
				bf_get(lpfc_mqe_status, &mboxq->u.mqe));
		mempool_free(mboxq, phba->mbox_mem_pool);
		return (uint64_t) -1;
	}
	mb = &mboxq->u.mb;
	memcpy(&wwn, (char *)mb->un.varRDnvp.portname, sizeof(uint64_t));
	 
	mempool_free(mboxq, phba->mbox_mem_pool);
	if (phba->sli_rev == LPFC_SLI_REV4)
		return be64_to_cpu(wwn);
	else
		return rol64(wwn, 32);
}

static unsigned short lpfc_get_sg_tablesize(struct lpfc_hba *phba)
{
	if (phba->sli_rev == LPFC_SLI_REV4)
		if (phba->cfg_xpsgl && !phba->nvmet_support)
			return LPFC_MAX_SG_TABLESIZE;
		else
			return phba->cfg_scsi_seg_cnt;
	else
		return phba->cfg_sg_seg_cnt;
}

 
static int
lpfc_vmid_res_alloc(struct lpfc_hba *phba, struct lpfc_vport *vport)
{
	 
	if (phba->sli_rev == LPFC_SLI_REV3) {
		phba->cfg_vmid_app_header = 0;
		phba->cfg_vmid_priority_tagging = 0;
	}

	if (lpfc_is_vmid_enabled(phba)) {
		vport->vmid =
		    kcalloc(phba->cfg_max_vmid, sizeof(struct lpfc_vmid),
			    GFP_KERNEL);
		if (!vport->vmid)
			return -ENOMEM;

		rwlock_init(&vport->vmid_lock);

		 
		vport->vmid_priority_tagging = phba->cfg_vmid_priority_tagging;
		vport->vmid_inactivity_timeout =
		    phba->cfg_vmid_inactivity_timeout;
		vport->max_vmid = phba->cfg_max_vmid;
		vport->cur_vmid_cnt = 0;

		vport->vmid_priority_range = bitmap_zalloc
			(LPFC_VMID_MAX_PRIORITY_RANGE, GFP_KERNEL);

		if (!vport->vmid_priority_range) {
			kfree(vport->vmid);
			return -ENOMEM;
		}

		hash_init(vport->hash_table);
	}
	return 0;
}

 
struct lpfc_vport *
lpfc_create_port(struct lpfc_hba *phba, int instance, struct device *dev)
{
	struct lpfc_vport *vport;
	struct Scsi_Host  *shost = NULL;
	struct scsi_host_template *template;
	int error = 0;
	int i;
	uint64_t wwn;
	bool use_no_reset_hba = false;
	int rc;

	if (lpfc_no_hba_reset_cnt) {
		if (phba->sli_rev < LPFC_SLI_REV4 &&
		    dev == &phba->pcidev->dev) {
			 
			lpfc_sli_brdrestart(phba);
			rc = lpfc_sli_chipset_init(phba);
			if (rc)
				return NULL;
		}
		wwn = lpfc_get_wwpn(phba);
	}

	for (i = 0; i < lpfc_no_hba_reset_cnt; i++) {
		if (wwn == lpfc_no_hba_reset[i]) {
			lpfc_printf_log(phba, KERN_ERR,
					LOG_TRACE_EVENT,
					"6020 Setting use_no_reset port=%llx\n",
					wwn);
			use_no_reset_hba = true;
			break;
		}
	}

	 
	if (dev == &phba->pcidev->dev) {
		if (phba->cfg_enable_fc4_type & LPFC_ENABLE_FCP) {
			 
			template = &lpfc_template;

			if (use_no_reset_hba)
				 
				template->eh_host_reset_handler = NULL;

			 
			template->sg_tablesize = lpfc_get_sg_tablesize(phba);
		} else {
			 
			template = &lpfc_template_nvme;
		}
	} else {
		 
		template = &lpfc_vport_template;

		 
		template->sg_tablesize = lpfc_get_sg_tablesize(phba);
	}

	shost = scsi_host_alloc(template, sizeof(struct lpfc_vport));
	if (!shost)
		goto out;

	vport = (struct lpfc_vport *) shost->hostdata;
	vport->phba = phba;
	vport->load_flag |= FC_LOADING;
	vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
	vport->fc_rscn_flush = 0;
	lpfc_get_vport_cfgparam(vport);

	 
	vport->cfg_enable_fc4_type = phba->cfg_enable_fc4_type;

	shost->unique_id = instance;
	shost->max_id = LPFC_MAX_TARGET;
	shost->max_lun = vport->cfg_max_luns;
	shost->this_id = -1;
	shost->max_cmd_len = 16;

	if (phba->sli_rev == LPFC_SLI_REV4) {
		if (!phba->cfg_fcp_mq_threshold ||
		    phba->cfg_fcp_mq_threshold > phba->cfg_hdw_queue)
			phba->cfg_fcp_mq_threshold = phba->cfg_hdw_queue;

		shost->nr_hw_queues = min_t(int, 2 * num_possible_nodes(),
					    phba->cfg_fcp_mq_threshold);

		shost->dma_boundary =
			phba->sli4_hba.pc_sli4_params.sge_supp_len-1;
	} else
		 
		shost->nr_hw_queues = 1;

	 
	shost->can_queue = phba->cfg_hba_queue_depth - 10;
	if (dev != &phba->pcidev->dev) {
		shost->transportt = lpfc_vport_transport_template;
		vport->port_type = LPFC_NPIV_PORT;
	} else {
		shost->transportt = lpfc_transport_template;
		vport->port_type = LPFC_PHYSICAL_PORT;
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT | LOG_FCP,
			"9081 CreatePort TMPLATE type %x TBLsize %d "
			"SEGcnt %d/%d\n",
			vport->port_type, shost->sg_tablesize,
			phba->cfg_scsi_seg_cnt, phba->cfg_sg_seg_cnt);

	 
	rc = lpfc_vmid_res_alloc(phba, vport);

	if (rc)
		goto out_put_shost;

	 
	INIT_LIST_HEAD(&vport->fc_nodes);
	INIT_LIST_HEAD(&vport->rcv_buffer_list);
	spin_lock_init(&vport->work_port_lock);

	timer_setup(&vport->fc_disctmo, lpfc_disc_timeout, 0);

	timer_setup(&vport->els_tmofunc, lpfc_els_timeout, 0);

	timer_setup(&vport->delayed_disc_tmo, lpfc_delayed_disc_tmo, 0);

	if (phba->sli3_options & LPFC_SLI3_BG_ENABLED)
		lpfc_setup_bg(phba, shost);

	error = scsi_add_host_with_dma(shost, dev, &phba->pcidev->dev);
	if (error)
		goto out_free_vmid;

	spin_lock_irq(&phba->port_list_lock);
	list_add_tail(&vport->listentry, &phba->port_list);
	spin_unlock_irq(&phba->port_list_lock);
	return vport;

out_free_vmid:
	kfree(vport->vmid);
	bitmap_free(vport->vmid_priority_range);
out_put_shost:
	scsi_host_put(shost);
out:
	return NULL;
}

 
void
destroy_port(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;

	lpfc_debugfs_terminate(vport);
	fc_remove_host(shost);
	scsi_remove_host(shost);

	spin_lock_irq(&phba->port_list_lock);
	list_del_init(&vport->listentry);
	spin_unlock_irq(&phba->port_list_lock);

	lpfc_cleanup(vport);
	return;
}

 
int
lpfc_get_instance(void)
{
	int ret;

	ret = idr_alloc(&lpfc_hba_index, NULL, 0, 0, GFP_KERNEL);
	return ret < 0 ? -1 : ret;
}

 
int lpfc_scan_finished(struct Scsi_Host *shost, unsigned long time)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	int stat = 0;

	spin_lock_irq(shost->host_lock);

	if (vport->load_flag & FC_UNLOADING) {
		stat = 1;
		goto finished;
	}
	if (time >= msecs_to_jiffies(30 * 1000)) {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0461 Scanning longer than 30 "
				"seconds.  Continuing initialization\n");
		stat = 1;
		goto finished;
	}
	if (time >= msecs_to_jiffies(15 * 1000) &&
	    phba->link_state <= LPFC_LINK_DOWN) {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0465 Link down longer than 15 "
				"seconds.  Continuing initialization\n");
		stat = 1;
		goto finished;
	}

	if (vport->port_state != LPFC_VPORT_READY)
		goto finished;
	if (vport->num_disc_nodes || vport->fc_prli_sent)
		goto finished;
	if (vport->fc_map_cnt == 0 && time < msecs_to_jiffies(2 * 1000))
		goto finished;
	if ((phba->sli.sli_flag & LPFC_SLI_MBOX_ACTIVE) != 0)
		goto finished;

	stat = 1;

finished:
	spin_unlock_irq(shost->host_lock);
	return stat;
}

static void lpfc_host_supported_speeds_set(struct Scsi_Host *shost)
{
	struct lpfc_vport *vport = (struct lpfc_vport *)shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	fc_host_supported_speeds(shost) = 0;
	 
	if (phba->hba_flag & HBA_FCOE_MODE)
		return;

	if (phba->lmt & LMT_256Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_256GBIT;
	if (phba->lmt & LMT_128Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_128GBIT;
	if (phba->lmt & LMT_64Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_64GBIT;
	if (phba->lmt & LMT_32Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_32GBIT;
	if (phba->lmt & LMT_16Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_16GBIT;
	if (phba->lmt & LMT_10Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_10GBIT;
	if (phba->lmt & LMT_8Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_8GBIT;
	if (phba->lmt & LMT_4Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_4GBIT;
	if (phba->lmt & LMT_2Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_2GBIT;
	if (phba->lmt & LMT_1Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_1GBIT;
}

 
void lpfc_host_attrib_init(struct Scsi_Host *shost)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	 

	fc_host_node_name(shost) = wwn_to_u64(vport->fc_nodename.u.wwn);
	fc_host_port_name(shost) = wwn_to_u64(vport->fc_portname.u.wwn);
	fc_host_supported_classes(shost) = FC_COS_CLASS3;

	memset(fc_host_supported_fc4s(shost), 0,
	       sizeof(fc_host_supported_fc4s(shost)));
	fc_host_supported_fc4s(shost)[2] = 1;
	fc_host_supported_fc4s(shost)[7] = 1;

	lpfc_vport_symbolic_node_name(vport, fc_host_symbolic_name(shost),
				 sizeof fc_host_symbolic_name(shost));

	lpfc_host_supported_speeds_set(shost);

	fc_host_maxframe_size(shost) =
		(((uint32_t) vport->fc_sparam.cmn.bbRcvSizeMsb & 0x0F) << 8) |
		(uint32_t) vport->fc_sparam.cmn.bbRcvSizeLsb;

	fc_host_dev_loss_tmo(shost) = vport->cfg_devloss_tmo;

	 
	memset(fc_host_active_fc4s(shost), 0,
	       sizeof(fc_host_active_fc4s(shost)));
	fc_host_active_fc4s(shost)[2] = 1;
	fc_host_active_fc4s(shost)[7] = 1;

	fc_host_max_npiv_vports(shost) = phba->max_vpi;
	spin_lock_irq(shost->host_lock);
	vport->load_flag &= ~FC_LOADING;
	spin_unlock_irq(shost->host_lock);
}

 
static void
lpfc_stop_port_s3(struct lpfc_hba *phba)
{
	 
	writel(0, phba->HCregaddr);
	readl(phba->HCregaddr);  
	 
	writel(0xffffffff, phba->HAregaddr);
	readl(phba->HAregaddr);  

	 
	lpfc_stop_hba_timers(phba);
	phba->pport->work_port_events = 0;
}

 
static void
lpfc_stop_port_s4(struct lpfc_hba *phba)
{
	 
	lpfc_stop_hba_timers(phba);
	if (phba->pport)
		phba->pport->work_port_events = 0;
	phba->sli4_hba.intr_enable = 0;
}

 
void
lpfc_stop_port(struct lpfc_hba *phba)
{
	phba->lpfc_stop_port(phba);

	if (phba->wq)
		flush_workqueue(phba->wq);
}

 
void
lpfc_fcf_redisc_wait_start_timer(struct lpfc_hba *phba)
{
	unsigned long fcf_redisc_wait_tmo =
		(jiffies + msecs_to_jiffies(LPFC_FCF_REDISCOVER_WAIT_TMO));
	 
	mod_timer(&phba->fcf.redisc_wait, fcf_redisc_wait_tmo);
	spin_lock_irq(&phba->hbalock);
	 
	phba->fcf.fcf_flag &= ~(FCF_AVAILABLE | FCF_SCAN_DONE);
	 
	phba->fcf.fcf_flag |= FCF_REDISC_PEND;
	spin_unlock_irq(&phba->hbalock);
}

 
static void
lpfc_sli4_fcf_redisc_wait_tmo(struct timer_list *t)
{
	struct lpfc_hba *phba = from_timer(phba, t, fcf.redisc_wait);

	 
	spin_lock_irq(&phba->hbalock);
	if (!(phba->fcf.fcf_flag & FCF_REDISC_PEND)) {
		spin_unlock_irq(&phba->hbalock);
		return;
	}
	 
	phba->fcf.fcf_flag &= ~FCF_REDISC_PEND;
	 
	phba->fcf.fcf_flag |= FCF_REDISC_EVT;
	spin_unlock_irq(&phba->hbalock);
	lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
			"2776 FCF rediscover quiescent timer expired\n");
	 
	lpfc_worker_wake_up(phba);
}

 
static void
lpfc_vmid_poll(struct timer_list *t)
{
	struct lpfc_hba *phba = from_timer(phba, t, inactive_vmid_poll);
	u32 wake_up = 0;

	 
	if (phba->pport->vmid_priority_tagging) {
		wake_up = 1;
		phba->pport->work_port_events |= WORKER_CHECK_VMID_ISSUE_QFPA;
	}

	 
	if (phba->pport->vmid_inactivity_timeout ||
	    phba->pport->load_flag & FC_DEREGISTER_ALL_APP_ID) {
		wake_up = 1;
		phba->pport->work_port_events |= WORKER_CHECK_INACTIVE_VMID;
	}

	if (wake_up)
		lpfc_worker_wake_up(phba);

	 
	mod_timer(&phba->inactive_vmid_poll, jiffies + msecs_to_jiffies(1000 *
							LPFC_VMID_TIMER));
}

 
static void
lpfc_sli4_parse_latt_fault(struct lpfc_hba *phba,
			   struct lpfc_acqe_link *acqe_link)
{
	switch (bf_get(lpfc_acqe_fc_la_att_type, acqe_link)) {
	case LPFC_FC_LA_TYPE_LINK_DOWN:
	case LPFC_FC_LA_TYPE_TRUNKING_EVENT:
	case LPFC_FC_LA_TYPE_ACTIVATE_FAIL:
	case LPFC_FC_LA_TYPE_LINK_RESET_PRTCL_EVT:
		break;
	default:
		switch (bf_get(lpfc_acqe_link_fault, acqe_link)) {
		case LPFC_ASYNC_LINK_FAULT_NONE:
		case LPFC_ASYNC_LINK_FAULT_LOCAL:
		case LPFC_ASYNC_LINK_FAULT_REMOTE:
		case LPFC_ASYNC_LINK_FAULT_LR_LRR:
			break;
		default:
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0398 Unknown link fault code: x%x\n",
					bf_get(lpfc_acqe_link_fault, acqe_link));
			break;
		}
		break;
	}
}

 
static uint8_t
lpfc_sli4_parse_latt_type(struct lpfc_hba *phba,
			  struct lpfc_acqe_link *acqe_link)
{
	uint8_t att_type;

	switch (bf_get(lpfc_acqe_link_status, acqe_link)) {
	case LPFC_ASYNC_LINK_STATUS_DOWN:
	case LPFC_ASYNC_LINK_STATUS_LOGICAL_DOWN:
		att_type = LPFC_ATT_LINK_DOWN;
		break;
	case LPFC_ASYNC_LINK_STATUS_UP:
		 
		att_type = LPFC_ATT_RESERVED;
		break;
	case LPFC_ASYNC_LINK_STATUS_LOGICAL_UP:
		att_type = LPFC_ATT_LINK_UP;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0399 Invalid link attention type: x%x\n",
				bf_get(lpfc_acqe_link_status, acqe_link));
		att_type = LPFC_ATT_RESERVED;
		break;
	}
	return att_type;
}

 
uint32_t
lpfc_sli_port_speed_get(struct lpfc_hba *phba)
{
	uint32_t link_speed;

	if (!lpfc_is_link_up(phba))
		return 0;

	if (phba->sli_rev <= LPFC_SLI_REV3) {
		switch (phba->fc_linkspeed) {
		case LPFC_LINK_SPEED_1GHZ:
			link_speed = 1000;
			break;
		case LPFC_LINK_SPEED_2GHZ:
			link_speed = 2000;
			break;
		case LPFC_LINK_SPEED_4GHZ:
			link_speed = 4000;
			break;
		case LPFC_LINK_SPEED_8GHZ:
			link_speed = 8000;
			break;
		case LPFC_LINK_SPEED_10GHZ:
			link_speed = 10000;
			break;
		case LPFC_LINK_SPEED_16GHZ:
			link_speed = 16000;
			break;
		default:
			link_speed = 0;
		}
	} else {
		if (phba->sli4_hba.link_state.logical_speed)
			link_speed =
			      phba->sli4_hba.link_state.logical_speed;
		else
			link_speed = phba->sli4_hba.link_state.speed;
	}
	return link_speed;
}

 
static uint32_t
lpfc_sli4_port_speed_parse(struct lpfc_hba *phba, uint32_t evt_code,
			   uint8_t speed_code)
{
	uint32_t port_speed;

	switch (evt_code) {
	case LPFC_TRAILER_CODE_LINK:
		switch (speed_code) {
		case LPFC_ASYNC_LINK_SPEED_ZERO:
			port_speed = 0;
			break;
		case LPFC_ASYNC_LINK_SPEED_10MBPS:
			port_speed = 10;
			break;
		case LPFC_ASYNC_LINK_SPEED_100MBPS:
			port_speed = 100;
			break;
		case LPFC_ASYNC_LINK_SPEED_1GBPS:
			port_speed = 1000;
			break;
		case LPFC_ASYNC_LINK_SPEED_10GBPS:
			port_speed = 10000;
			break;
		case LPFC_ASYNC_LINK_SPEED_20GBPS:
			port_speed = 20000;
			break;
		case LPFC_ASYNC_LINK_SPEED_25GBPS:
			port_speed = 25000;
			break;
		case LPFC_ASYNC_LINK_SPEED_40GBPS:
			port_speed = 40000;
			break;
		case LPFC_ASYNC_LINK_SPEED_100GBPS:
			port_speed = 100000;
			break;
		default:
			port_speed = 0;
		}
		break;
	case LPFC_TRAILER_CODE_FC:
		switch (speed_code) {
		case LPFC_FC_LA_SPEED_UNKNOWN:
			port_speed = 0;
			break;
		case LPFC_FC_LA_SPEED_1G:
			port_speed = 1000;
			break;
		case LPFC_FC_LA_SPEED_2G:
			port_speed = 2000;
			break;
		case LPFC_FC_LA_SPEED_4G:
			port_speed = 4000;
			break;
		case LPFC_FC_LA_SPEED_8G:
			port_speed = 8000;
			break;
		case LPFC_FC_LA_SPEED_10G:
			port_speed = 10000;
			break;
		case LPFC_FC_LA_SPEED_16G:
			port_speed = 16000;
			break;
		case LPFC_FC_LA_SPEED_32G:
			port_speed = 32000;
			break;
		case LPFC_FC_LA_SPEED_64G:
			port_speed = 64000;
			break;
		case LPFC_FC_LA_SPEED_128G:
			port_speed = 128000;
			break;
		case LPFC_FC_LA_SPEED_256G:
			port_speed = 256000;
			break;
		default:
			port_speed = 0;
		}
		break;
	default:
		port_speed = 0;
	}
	return port_speed;
}

 
static void
lpfc_sli4_async_link_evt(struct lpfc_hba *phba,
			 struct lpfc_acqe_link *acqe_link)
{
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *mb;
	struct lpfc_mbx_read_top *la;
	uint8_t att_type;
	int rc;

	att_type = lpfc_sli4_parse_latt_type(phba, acqe_link);
	if (att_type != LPFC_ATT_LINK_DOWN && att_type != LPFC_ATT_LINK_UP)
		return;
	phba->fcoe_eventtag = acqe_link->event_tag;
	pmb = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0395 The mboxq allocation failed\n");
		return;
	}

	rc = lpfc_mbox_rsrc_prep(phba, pmb);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0396 mailbox allocation failed\n");
		goto out_free_pmb;
	}

	 
	lpfc_els_flush_all_cmd(phba);

	 
	phba->sli4_hba.els_wq->pring->flag |= LPFC_STOP_IOCB_EVENT;

	 
	phba->sli.slistat.link_event++;

	 
	lpfc_read_topology(phba, pmb, (struct lpfc_dmabuf *)pmb->ctx_buf);
	pmb->mbox_cmpl = lpfc_mbx_cmpl_read_topology;
	pmb->vport = phba->pport;

	 
	phba->sli4_hba.link_state.speed =
			lpfc_sli4_port_speed_parse(phba, LPFC_TRAILER_CODE_LINK,
				bf_get(lpfc_acqe_link_speed, acqe_link));
	phba->sli4_hba.link_state.duplex =
				bf_get(lpfc_acqe_link_duplex, acqe_link);
	phba->sli4_hba.link_state.status =
				bf_get(lpfc_acqe_link_status, acqe_link);
	phba->sli4_hba.link_state.type =
				bf_get(lpfc_acqe_link_type, acqe_link);
	phba->sli4_hba.link_state.number =
				bf_get(lpfc_acqe_link_number, acqe_link);
	phba->sli4_hba.link_state.fault =
				bf_get(lpfc_acqe_link_fault, acqe_link);
	phba->sli4_hba.link_state.logical_speed =
			bf_get(lpfc_acqe_logical_link_speed, acqe_link) * 10;

	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"2900 Async FC/FCoE Link event - Speed:%dGBit "
			"duplex:x%x LA Type:x%x Port Type:%d Port Number:%d "
			"Logical speed:%dMbps Fault:%d\n",
			phba->sli4_hba.link_state.speed,
			phba->sli4_hba.link_state.topology,
			phba->sli4_hba.link_state.status,
			phba->sli4_hba.link_state.type,
			phba->sli4_hba.link_state.number,
			phba->sli4_hba.link_state.logical_speed,
			phba->sli4_hba.link_state.fault);
	 
	if (!(phba->hba_flag & HBA_FCOE_MODE)) {
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);
		if (rc == MBX_NOT_FINISHED)
			goto out_free_pmb;
		return;
	}
	 
	 
	mb = &pmb->u.mb;
	mb->mbxStatus = MBX_SUCCESS;

	 
	lpfc_sli4_parse_latt_fault(phba, acqe_link);

	 
	la = (struct lpfc_mbx_read_top *) &pmb->u.mb.un.varReadTop;
	la->eventTag = acqe_link->event_tag;
	bf_set(lpfc_mbx_read_top_att_type, la, att_type);
	bf_set(lpfc_mbx_read_top_link_spd, la,
	       (bf_get(lpfc_acqe_link_speed, acqe_link)));

	 
	bf_set(lpfc_mbx_read_top_topology, la, LPFC_TOPOLOGY_PT_PT);
	bf_set(lpfc_mbx_read_top_alpa_granted, la, 0);
	bf_set(lpfc_mbx_read_top_il, la, 0);
	bf_set(lpfc_mbx_read_top_pb, la, 0);
	bf_set(lpfc_mbx_read_top_fa, la, 0);
	bf_set(lpfc_mbx_read_top_mm, la, 0);

	 
	lpfc_mbx_cmpl_read_topology(phba, pmb);

	return;

out_free_pmb:
	lpfc_mbox_rsrc_cleanup(phba, pmb, MBOX_THD_UNLOCKED);
}

 
static uint8_t
lpfc_async_link_speed_to_read_top(struct lpfc_hba *phba, uint8_t speed_code)
{
	uint8_t port_speed;

	switch (speed_code) {
	case LPFC_FC_LA_SPEED_1G:
		port_speed = LPFC_LINK_SPEED_1GHZ;
		break;
	case LPFC_FC_LA_SPEED_2G:
		port_speed = LPFC_LINK_SPEED_2GHZ;
		break;
	case LPFC_FC_LA_SPEED_4G:
		port_speed = LPFC_LINK_SPEED_4GHZ;
		break;
	case LPFC_FC_LA_SPEED_8G:
		port_speed = LPFC_LINK_SPEED_8GHZ;
		break;
	case LPFC_FC_LA_SPEED_16G:
		port_speed = LPFC_LINK_SPEED_16GHZ;
		break;
	case LPFC_FC_LA_SPEED_32G:
		port_speed = LPFC_LINK_SPEED_32GHZ;
		break;
	case LPFC_FC_LA_SPEED_64G:
		port_speed = LPFC_LINK_SPEED_64GHZ;
		break;
	case LPFC_FC_LA_SPEED_128G:
		port_speed = LPFC_LINK_SPEED_128GHZ;
		break;
	case LPFC_FC_LA_SPEED_256G:
		port_speed = LPFC_LINK_SPEED_256GHZ;
		break;
	default:
		port_speed = 0;
		break;
	}

	return port_speed;
}

void
lpfc_cgn_dump_rxmonitor(struct lpfc_hba *phba)
{
	if (!phba->rx_monitor) {
		lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
				"4411 Rx Monitor Info is empty.\n");
	} else {
		lpfc_rx_monitor_report(phba, phba->rx_monitor, NULL, 0,
				       LPFC_MAX_RXMONITOR_DUMP);
	}
}

 
void
lpfc_cgn_update_stat(struct lpfc_hba *phba, uint32_t dtag)
{
	struct lpfc_cgn_info *cp;
	u32 value;

	 
	if (!phba->cgn_i)
		return;
	cp = (struct lpfc_cgn_info *)phba->cgn_i->virt;

	 
	switch (dtag) {
	case ELS_DTAG_LNK_INTEGRITY:
		le32_add_cpu(&cp->link_integ_notification, 1);
		lpfc_cgn_update_tstamp(phba, &cp->stat_lnk);
		break;
	case ELS_DTAG_DELIVERY:
		le32_add_cpu(&cp->delivery_notification, 1);
		lpfc_cgn_update_tstamp(phba, &cp->stat_delivery);
		break;
	case ELS_DTAG_PEER_CONGEST:
		le32_add_cpu(&cp->cgn_peer_notification, 1);
		lpfc_cgn_update_tstamp(phba, &cp->stat_peer);
		break;
	case ELS_DTAG_CONGESTION:
		le32_add_cpu(&cp->cgn_notification, 1);
		lpfc_cgn_update_tstamp(phba, &cp->stat_fpin);
	}
	if (phba->cgn_fpin_frequency &&
	    phba->cgn_fpin_frequency != LPFC_FPIN_INIT_FREQ) {
		value = LPFC_CGN_TIMER_TO_MIN / phba->cgn_fpin_frequency;
		cp->cgn_stat_npm = value;
	}

	value = lpfc_cgn_calc_crc32(cp, LPFC_CGN_INFO_SZ,
				    LPFC_CGN_CRC32_SEED);
	cp->cgn_info_crc = cpu_to_le32(value);
}

 
void
lpfc_cgn_update_tstamp(struct lpfc_hba *phba, struct lpfc_cgn_ts *ts)
{
	struct timespec64 cur_time;
	struct tm tm_val;

	ktime_get_real_ts64(&cur_time);
	time64_to_tm(cur_time.tv_sec, 0, &tm_val);

	ts->month = tm_val.tm_mon + 1;
	ts->day	= tm_val.tm_mday;
	ts->year = tm_val.tm_year - 100;
	ts->hour = tm_val.tm_hour;
	ts->minute = tm_val.tm_min;
	ts->second = tm_val.tm_sec;

	lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
			"2646 Updated CMF timestamp : "
			"%u/%u/%u %u:%u:%u\n",
			ts->day, ts->month,
			ts->year, ts->hour,
			ts->minute, ts->second);
}

 
static enum hrtimer_restart
lpfc_cmf_stats_timer(struct hrtimer *timer)
{
	struct lpfc_hba *phba;
	struct lpfc_cgn_info *cp;
	uint32_t i, index;
	uint16_t value, mvalue;
	uint64_t bps;
	uint32_t mbps;
	uint32_t dvalue, wvalue, lvalue, avalue;
	uint64_t latsum;
	__le16 *ptr;
	__le32 *lptr;
	__le16 *mptr;

	phba = container_of(timer, struct lpfc_hba, cmf_stats_timer);
	 
	if (!phba->cgn_i)
		return HRTIMER_NORESTART;
	cp = (struct lpfc_cgn_info *)phba->cgn_i->virt;

	phba->cgn_evt_timestamp = jiffies +
			msecs_to_jiffies(LPFC_CGN_TIMER_TO_MIN);
	phba->cgn_evt_minute++;

	 
	lpfc_cgn_update_tstamp(phba, &cp->base_time);

	if (phba->cgn_fpin_frequency &&
	    phba->cgn_fpin_frequency != LPFC_FPIN_INIT_FREQ) {
		value = LPFC_CGN_TIMER_TO_MIN / phba->cgn_fpin_frequency;
		cp->cgn_stat_npm = value;
	}

	 
	lvalue = atomic_read(&phba->cgn_latency_evt_cnt);
	latsum = atomic64_read(&phba->cgn_latency_evt);
	atomic_set(&phba->cgn_latency_evt_cnt, 0);
	atomic64_set(&phba->cgn_latency_evt, 0);

	 
	bps = div_u64(phba->rx_block_cnt, LPFC_SEC_MIN) * 512;
	phba->rx_block_cnt = 0;
	mvalue = bps / (1024 * 1024);  

	 
	 
	cp->cgn_info_mode = phba->cgn_p.cgn_param_mode;
	cp->cgn_info_level0 = phba->cgn_p.cgn_param_level0;
	cp->cgn_info_level1 = phba->cgn_p.cgn_param_level1;
	cp->cgn_info_level2 = phba->cgn_p.cgn_param_level2;

	 
	value = (uint16_t)(phba->pport->cfg_lun_queue_depth);
	cp->cgn_lunq = cpu_to_le16(value);

	 
	index = ++cp->cgn_index_minute;
	if (cp->cgn_index_minute == LPFC_MIN_HOUR) {
		cp->cgn_index_minute = 0;
		index = 0;
	}

	 
	dvalue = atomic_read(&phba->cgn_driver_evt_cnt);
	atomic_set(&phba->cgn_driver_evt_cnt, 0);

	 
	wvalue = 0;
	if ((phba->cgn_reg_fpin & LPFC_CGN_FPIN_WARN) ||
	    phba->cgn_reg_signal == EDC_CG_SIG_WARN_ONLY ||
	    phba->cgn_reg_signal == EDC_CG_SIG_WARN_ALARM)
		wvalue = atomic_read(&phba->cgn_fabric_warn_cnt);
	atomic_set(&phba->cgn_fabric_warn_cnt, 0);

	 
	avalue = 0;
	if ((phba->cgn_reg_fpin & LPFC_CGN_FPIN_ALARM) ||
	    phba->cgn_reg_signal == EDC_CG_SIG_WARN_ALARM)
		avalue = atomic_read(&phba->cgn_fabric_alarm_cnt);
	atomic_set(&phba->cgn_fabric_alarm_cnt, 0);

	 
	ptr = &cp->cgn_drvr_min[index];
	value = (uint16_t)dvalue;
	*ptr = cpu_to_le16(value);

	ptr = &cp->cgn_warn_min[index];
	value = (uint16_t)wvalue;
	*ptr = cpu_to_le16(value);

	ptr = &cp->cgn_alarm_min[index];
	value = (uint16_t)avalue;
	*ptr = cpu_to_le16(value);

	lptr = &cp->cgn_latency_min[index];
	if (lvalue) {
		lvalue = (uint32_t)div_u64(latsum, lvalue);
		*lptr = cpu_to_le32(lvalue);
	} else {
		*lptr = 0;
	}

	 
	mptr = &cp->cgn_bw_min[index];
	*mptr = cpu_to_le16(mvalue);

	lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
			"2418 Congestion Info - minute (%d): %d %d %d %d %d\n",
			index, dvalue, wvalue, *lptr, mvalue, avalue);

	 
	if ((phba->cgn_evt_minute % LPFC_MIN_HOUR) == 0) {
		 
		index = ++cp->cgn_index_hour;
		if (cp->cgn_index_hour == LPFC_HOUR_DAY) {
			cp->cgn_index_hour = 0;
			index = 0;
		}

		dvalue = 0;
		wvalue = 0;
		lvalue = 0;
		avalue = 0;
		mvalue = 0;
		mbps = 0;
		for (i = 0; i < LPFC_MIN_HOUR; i++) {
			dvalue += le16_to_cpu(cp->cgn_drvr_min[i]);
			wvalue += le16_to_cpu(cp->cgn_warn_min[i]);
			lvalue += le32_to_cpu(cp->cgn_latency_min[i]);
			mbps += le16_to_cpu(cp->cgn_bw_min[i]);
			avalue += le16_to_cpu(cp->cgn_alarm_min[i]);
		}
		if (lvalue)		 
			lvalue /= LPFC_MIN_HOUR;
		if (mbps)		 
			mvalue = mbps / LPFC_MIN_HOUR;

		lptr = &cp->cgn_drvr_hr[index];
		*lptr = cpu_to_le32(dvalue);
		lptr = &cp->cgn_warn_hr[index];
		*lptr = cpu_to_le32(wvalue);
		lptr = &cp->cgn_latency_hr[index];
		*lptr = cpu_to_le32(lvalue);
		mptr = &cp->cgn_bw_hr[index];
		*mptr = cpu_to_le16(mvalue);
		lptr = &cp->cgn_alarm_hr[index];
		*lptr = cpu_to_le32(avalue);

		lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
				"2419 Congestion Info - hour "
				"(%d): %d %d %d %d %d\n",
				index, dvalue, wvalue, lvalue, mvalue, avalue);
	}

	 
	if ((phba->cgn_evt_minute % LPFC_MIN_DAY) == 0) {
		 
		index = ++cp->cgn_index_day;
		if (cp->cgn_index_day == LPFC_MAX_CGN_DAYS) {
			cp->cgn_index_day = 0;
			index = 0;
		}

		dvalue = 0;
		wvalue = 0;
		lvalue = 0;
		mvalue = 0;
		mbps = 0;
		avalue = 0;
		for (i = 0; i < LPFC_HOUR_DAY; i++) {
			dvalue += le32_to_cpu(cp->cgn_drvr_hr[i]);
			wvalue += le32_to_cpu(cp->cgn_warn_hr[i]);
			lvalue += le32_to_cpu(cp->cgn_latency_hr[i]);
			mbps += le16_to_cpu(cp->cgn_bw_hr[i]);
			avalue += le32_to_cpu(cp->cgn_alarm_hr[i]);
		}
		if (lvalue)		 
			lvalue /= LPFC_HOUR_DAY;
		if (mbps)		 
			mvalue = mbps / LPFC_HOUR_DAY;

		lptr = &cp->cgn_drvr_day[index];
		*lptr = cpu_to_le32(dvalue);
		lptr = &cp->cgn_warn_day[index];
		*lptr = cpu_to_le32(wvalue);
		lptr = &cp->cgn_latency_day[index];
		*lptr = cpu_to_le32(lvalue);
		mptr = &cp->cgn_bw_day[index];
		*mptr = cpu_to_le16(mvalue);
		lptr = &cp->cgn_alarm_day[index];
		*lptr = cpu_to_le32(avalue);

		lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
				"2420 Congestion Info - daily (%d): "
				"%d %d %d %d %d\n",
				index, dvalue, wvalue, lvalue, mvalue, avalue);
	}

	 
	value = phba->cgn_fpin_frequency;
	cp->cgn_warn_freq = cpu_to_le16(value);
	cp->cgn_alarm_freq = cpu_to_le16(value);

	lvalue = lpfc_cgn_calc_crc32(cp, LPFC_CGN_INFO_SZ,
				     LPFC_CGN_CRC32_SEED);
	cp->cgn_info_crc = cpu_to_le32(lvalue);

	hrtimer_forward_now(timer, ktime_set(0, LPFC_SEC_MIN * NSEC_PER_SEC));

	return HRTIMER_RESTART;
}

 
uint32_t
lpfc_calc_cmf_latency(struct lpfc_hba *phba)
{
	struct timespec64 cmpl_time;
	uint32_t msec = 0;

	ktime_get_real_ts64(&cmpl_time);

	 
	if (cmpl_time.tv_sec == phba->cmf_latency.tv_sec) {
		msec = (cmpl_time.tv_nsec - phba->cmf_latency.tv_nsec) /
			NSEC_PER_MSEC;
	} else {
		if (cmpl_time.tv_nsec >= phba->cmf_latency.tv_nsec) {
			msec = (cmpl_time.tv_sec -
				phba->cmf_latency.tv_sec) * MSEC_PER_SEC;
			msec += ((cmpl_time.tv_nsec -
				  phba->cmf_latency.tv_nsec) / NSEC_PER_MSEC);
		} else {
			msec = (cmpl_time.tv_sec - phba->cmf_latency.tv_sec -
				1) * MSEC_PER_SEC;
			msec += (((NSEC_PER_SEC - phba->cmf_latency.tv_nsec) +
				 cmpl_time.tv_nsec) / NSEC_PER_MSEC);
		}
	}
	return msec;
}

 
static enum hrtimer_restart
lpfc_cmf_timer(struct hrtimer *timer)
{
	struct lpfc_hba *phba = container_of(timer, struct lpfc_hba,
					     cmf_timer);
	struct rx_info_entry entry;
	uint32_t io_cnt;
	uint32_t busy, max_read;
	uint64_t total, rcv, lat, mbpi, extra, cnt;
	int timer_interval = LPFC_CMF_INTERVAL;
	uint32_t ms;
	struct lpfc_cgn_stat *cgs;
	int cpu;

	 
	if (phba->cmf_active_mode == LPFC_CFG_OFF ||
	    !phba->cmf_latency.tv_sec) {
		lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
				"6224 CMF timer exit: %d %lld\n",
				phba->cmf_active_mode,
				(uint64_t)phba->cmf_latency.tv_sec);
		return HRTIMER_NORESTART;
	}

	 
	if (!phba->pport)
		goto skip;

	 
	atomic_set(&phba->cmf_stop_io, 1);

	 
	ms = lpfc_calc_cmf_latency(phba);


	 
	ktime_get_real_ts64(&phba->cmf_latency);

	phba->cmf_link_byte_count =
		div_u64(phba->cmf_max_line_rate * LPFC_CMF_INTERVAL, 1000);

	 
	total = 0;
	io_cnt = 0;
	lat = 0;
	rcv = 0;
	for_each_present_cpu(cpu) {
		cgs = per_cpu_ptr(phba->cmf_stat, cpu);
		total += atomic64_xchg(&cgs->total_bytes, 0);
		io_cnt += atomic_xchg(&cgs->rx_io_cnt, 0);
		lat += atomic64_xchg(&cgs->rx_latency, 0);
		rcv += atomic64_xchg(&cgs->rcv_bytes, 0);
	}

	 
	if (phba->cmf_active_mode == LPFC_CFG_MANAGED &&
	    phba->link_state != LPFC_LINK_DOWN &&
	    phba->hba_flag & HBA_SETUP) {
		mbpi = phba->cmf_last_sync_bw;
		phba->cmf_last_sync_bw = 0;
		extra = 0;

		 
		if (ms && ms < LPFC_CMF_INTERVAL) {
			cnt = div_u64(total, ms);  
			cnt *= LPFC_CMF_INTERVAL;  
			extra = cnt - total;
		}
		lpfc_issue_cmf_sync_wqe(phba, LPFC_CMF_INTERVAL, total + extra);
	} else {
		 
		mbpi = phba->cmf_link_byte_count;
		extra = 0;
	}
	phba->cmf_timer_cnt++;

	if (io_cnt) {
		 
		atomic_add(io_cnt, &phba->cgn_latency_evt_cnt);
		atomic64_add(lat, &phba->cgn_latency_evt);
	}
	busy = atomic_xchg(&phba->cmf_busy, 0);
	max_read = atomic_xchg(&phba->rx_max_read_cnt, 0);

	 
	if (mbpi) {
		if (mbpi > phba->cmf_link_byte_count ||
		    phba->cmf_active_mode == LPFC_CFG_MONITOR)
			mbpi = phba->cmf_link_byte_count;

		 
		if (mbpi != phba->cmf_max_bytes_per_interval)
			phba->cmf_max_bytes_per_interval = mbpi;
	}

	 
	if (phba->rx_monitor) {
		entry.total_bytes = total;
		entry.cmf_bytes = total + extra;
		entry.rcv_bytes = rcv;
		entry.cmf_busy = busy;
		entry.cmf_info = phba->cmf_active_info;
		if (io_cnt) {
			entry.avg_io_latency = div_u64(lat, io_cnt);
			entry.avg_io_size = div_u64(rcv, io_cnt);
		} else {
			entry.avg_io_latency = 0;
			entry.avg_io_size = 0;
		}
		entry.max_read_cnt = max_read;
		entry.io_cnt = io_cnt;
		entry.max_bytes_per_interval = mbpi;
		if (phba->cmf_active_mode == LPFC_CFG_MANAGED)
			entry.timer_utilization = phba->cmf_last_ts;
		else
			entry.timer_utilization = ms;
		entry.timer_interval = ms;
		phba->cmf_last_ts = 0;

		lpfc_rx_monitor_record(phba->rx_monitor, &entry);
	}

	if (phba->cmf_active_mode == LPFC_CFG_MONITOR) {
		 
		if (mbpi && total > mbpi)
			atomic_inc(&phba->cgn_driver_evt_cnt);
	}
	phba->rx_block_cnt += div_u64(rcv, 512);   

	 
	if (atomic_xchg(&phba->cmf_bw_wait, 0))
		queue_work(phba->wq, &phba->unblock_request_work);

	 
	atomic_set(&phba->cmf_stop_io, 0);

skip:
	hrtimer_forward_now(timer,
			    ktime_set(0, timer_interval * NSEC_PER_MSEC));
	return HRTIMER_RESTART;
}

#define trunk_link_status(__idx)\
	bf_get(lpfc_acqe_fc_la_trunk_config_port##__idx, acqe_fc) ?\
	       ((phba->trunk_link.link##__idx.state == LPFC_LINK_UP) ?\
		"Link up" : "Link down") : "NA"
 
#define trunk_port_fault(__idx)\
	bf_get(lpfc_acqe_fc_la_trunk_config_port##__idx, acqe_fc) ?\
	       (port_fault & (1 << __idx) ? "YES" : "NO") : "NA"

static void
lpfc_update_trunk_link_status(struct lpfc_hba *phba,
			      struct lpfc_acqe_fc_la *acqe_fc)
{
	uint8_t port_fault = bf_get(lpfc_acqe_fc_la_trunk_linkmask, acqe_fc);
	uint8_t err = bf_get(lpfc_acqe_fc_la_trunk_fault, acqe_fc);
	u8 cnt = 0;

	phba->sli4_hba.link_state.speed =
		lpfc_sli4_port_speed_parse(phba, LPFC_TRAILER_CODE_FC,
				bf_get(lpfc_acqe_fc_la_speed, acqe_fc));

	phba->sli4_hba.link_state.logical_speed =
				bf_get(lpfc_acqe_fc_la_llink_spd, acqe_fc) * 10;
	 
	phba->fc_linkspeed =
		 lpfc_async_link_speed_to_read_top(
				phba,
				bf_get(lpfc_acqe_fc_la_speed, acqe_fc));

	if (bf_get(lpfc_acqe_fc_la_trunk_config_port0, acqe_fc)) {
		phba->trunk_link.link0.state =
			bf_get(lpfc_acqe_fc_la_trunk_link_status_port0, acqe_fc)
			? LPFC_LINK_UP : LPFC_LINK_DOWN;
		phba->trunk_link.link0.fault = port_fault & 0x1 ? err : 0;
		cnt++;
	}
	if (bf_get(lpfc_acqe_fc_la_trunk_config_port1, acqe_fc)) {
		phba->trunk_link.link1.state =
			bf_get(lpfc_acqe_fc_la_trunk_link_status_port1, acqe_fc)
			? LPFC_LINK_UP : LPFC_LINK_DOWN;
		phba->trunk_link.link1.fault = port_fault & 0x2 ? err : 0;
		cnt++;
	}
	if (bf_get(lpfc_acqe_fc_la_trunk_config_port2, acqe_fc)) {
		phba->trunk_link.link2.state =
			bf_get(lpfc_acqe_fc_la_trunk_link_status_port2, acqe_fc)
			? LPFC_LINK_UP : LPFC_LINK_DOWN;
		phba->trunk_link.link2.fault = port_fault & 0x4 ? err : 0;
		cnt++;
	}
	if (bf_get(lpfc_acqe_fc_la_trunk_config_port3, acqe_fc)) {
		phba->trunk_link.link3.state =
			bf_get(lpfc_acqe_fc_la_trunk_link_status_port3, acqe_fc)
			? LPFC_LINK_UP : LPFC_LINK_DOWN;
		phba->trunk_link.link3.fault = port_fault & 0x8 ? err : 0;
		cnt++;
	}

	if (cnt)
		phba->trunk_link.phy_lnk_speed =
			phba->sli4_hba.link_state.logical_speed / (cnt * 1000);
	else
		phba->trunk_link.phy_lnk_speed = LPFC_LINK_SPEED_UNKNOWN;

	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"2910 Async FC Trunking Event - Speed:%d\n"
			"\tLogical speed:%d "
			"port0: %s port1: %s port2: %s port3: %s\n",
			phba->sli4_hba.link_state.speed,
			phba->sli4_hba.link_state.logical_speed,
			trunk_link_status(0), trunk_link_status(1),
			trunk_link_status(2), trunk_link_status(3));

	if (phba->cmf_active_mode != LPFC_CFG_OFF)
		lpfc_cmf_signal_init(phba);

	if (port_fault)
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3202 trunk error:0x%x (%s) seen on port0:%s "
				 
				"port1:%s port2:%s port3:%s\n", err, err > 0xA ?
				"UNDEFINED. update driver." : trunk_errmsg[err],
				trunk_port_fault(0), trunk_port_fault(1),
				trunk_port_fault(2), trunk_port_fault(3));
}


 
static void
lpfc_sli4_async_fc_evt(struct lpfc_hba *phba, struct lpfc_acqe_fc_la *acqe_fc)
{
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *mb;
	struct lpfc_mbx_read_top *la;
	char *log_level;
	int rc;

	if (bf_get(lpfc_trailer_type, acqe_fc) !=
	    LPFC_FC_LA_EVENT_TYPE_FC_LINK) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2895 Non FC link Event detected.(%d)\n",
				bf_get(lpfc_trailer_type, acqe_fc));
		return;
	}

	if (bf_get(lpfc_acqe_fc_la_att_type, acqe_fc) ==
	    LPFC_FC_LA_TYPE_TRUNKING_EVENT) {
		lpfc_update_trunk_link_status(phba, acqe_fc);
		return;
	}

	 
	phba->sli4_hba.link_state.speed =
			lpfc_sli4_port_speed_parse(phba, LPFC_TRAILER_CODE_FC,
				bf_get(lpfc_acqe_fc_la_speed, acqe_fc));
	phba->sli4_hba.link_state.duplex = LPFC_ASYNC_LINK_DUPLEX_FULL;
	phba->sli4_hba.link_state.topology =
				bf_get(lpfc_acqe_fc_la_topology, acqe_fc);
	phba->sli4_hba.link_state.status =
				bf_get(lpfc_acqe_fc_la_att_type, acqe_fc);
	phba->sli4_hba.link_state.type =
				bf_get(lpfc_acqe_fc_la_port_type, acqe_fc);
	phba->sli4_hba.link_state.number =
				bf_get(lpfc_acqe_fc_la_port_number, acqe_fc);
	phba->sli4_hba.link_state.fault =
				bf_get(lpfc_acqe_link_fault, acqe_fc);
	phba->sli4_hba.link_state.link_status =
				bf_get(lpfc_acqe_fc_la_link_status, acqe_fc);

	 
	if (phba->sli4_hba.link_state.status >= LPFC_FC_LA_TYPE_LINK_UP &&
	    phba->sli4_hba.link_state.status < LPFC_FC_LA_TYPE_ACTIVATE_FAIL) {
		if (bf_get(lpfc_acqe_fc_la_att_type, acqe_fc) ==
		    LPFC_FC_LA_TYPE_LINK_DOWN)
			phba->sli4_hba.link_state.logical_speed = 0;
		else if (!phba->sli4_hba.conf_trunk)
			phba->sli4_hba.link_state.logical_speed =
				bf_get(lpfc_acqe_fc_la_llink_spd, acqe_fc) * 10;
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"2896 Async FC event - Speed:%dGBaud Topology:x%x "
			"LA Type:x%x Port Type:%d Port Number:%d Logical speed:"
			"%dMbps Fault:x%x Link Status:x%x\n",
			phba->sli4_hba.link_state.speed,
			phba->sli4_hba.link_state.topology,
			phba->sli4_hba.link_state.status,
			phba->sli4_hba.link_state.type,
			phba->sli4_hba.link_state.number,
			phba->sli4_hba.link_state.logical_speed,
			phba->sli4_hba.link_state.fault,
			phba->sli4_hba.link_state.link_status);

	 
	if (phba->sli4_hba.link_state.status >= LPFC_FC_LA_TYPE_ACTIVATE_FAIL) {
		switch (phba->sli4_hba.link_state.status) {
		case LPFC_FC_LA_TYPE_ACTIVATE_FAIL:
			log_level = KERN_WARNING;
			phba->sli4_hba.link_state.status =
					LPFC_FC_LA_TYPE_LINK_DOWN;
			break;
		case LPFC_FC_LA_TYPE_LINK_RESET_PRTCL_EVT:
			 
			log_level = KERN_INFO;
			phba->sli4_hba.link_state.status =
					LPFC_FC_LA_TYPE_LINK_UP;
			break;
		default:
			log_level = KERN_INFO;
			break;
		}
		lpfc_log_msg(phba, log_level, LOG_SLI,
			     "2992 Async FC event - Informational Link "
			     "Attention Type x%x\n",
			     bf_get(lpfc_acqe_fc_la_att_type, acqe_fc));
		return;
	}

	pmb = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2897 The mboxq allocation failed\n");
		return;
	}
	rc = lpfc_mbox_rsrc_prep(phba, pmb);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2898 The mboxq prep failed\n");
		goto out_free_pmb;
	}

	 
	lpfc_els_flush_all_cmd(phba);

	 
	phba->sli4_hba.els_wq->pring->flag |= LPFC_STOP_IOCB_EVENT;

	 
	phba->sli.slistat.link_event++;

	 
	lpfc_read_topology(phba, pmb, (struct lpfc_dmabuf *)pmb->ctx_buf);
	pmb->mbox_cmpl = lpfc_mbx_cmpl_read_topology;
	pmb->vport = phba->pport;

	if (phba->sli4_hba.link_state.status != LPFC_FC_LA_TYPE_LINK_UP) {
		phba->link_flag &= ~(LS_MDS_LINK_DOWN | LS_MDS_LOOPBACK);

		switch (phba->sli4_hba.link_state.status) {
		case LPFC_FC_LA_TYPE_MDS_LINK_DOWN:
			phba->link_flag |= LS_MDS_LINK_DOWN;
			break;
		case LPFC_FC_LA_TYPE_MDS_LOOPBACK:
			phba->link_flag |= LS_MDS_LOOPBACK;
			break;
		default:
			break;
		}

		 
		mb = &pmb->u.mb;
		mb->mbxStatus = MBX_SUCCESS;

		 
		lpfc_sli4_parse_latt_fault(phba, (void *)acqe_fc);

		 
		la = (struct lpfc_mbx_read_top *)&pmb->u.mb.un.varReadTop;
		la->eventTag = acqe_fc->event_tag;

		if (phba->sli4_hba.link_state.status ==
		    LPFC_FC_LA_TYPE_UNEXP_WWPN) {
			bf_set(lpfc_mbx_read_top_att_type, la,
			       LPFC_FC_LA_TYPE_UNEXP_WWPN);
		} else {
			bf_set(lpfc_mbx_read_top_att_type, la,
			       LPFC_FC_LA_TYPE_LINK_DOWN);
		}
		 
		lpfc_mbx_cmpl_read_topology(phba, pmb);

		return;
	}

	rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED)
		goto out_free_pmb;
	return;

out_free_pmb:
	lpfc_mbox_rsrc_cleanup(phba, pmb, MBOX_THD_UNLOCKED);
}

 
static void
lpfc_sli4_async_sli_evt(struct lpfc_hba *phba, struct lpfc_acqe_sli *acqe_sli)
{
	char port_name;
	char message[128];
	uint8_t status;
	uint8_t evt_type;
	uint8_t operational = 0;
	struct temp_event temp_event_data;
	struct lpfc_acqe_misconfigured_event *misconfigured;
	struct lpfc_acqe_cgn_signal *cgn_signal;
	struct Scsi_Host  *shost;
	struct lpfc_vport **vports;
	int rc, i, cnt;

	evt_type = bf_get(lpfc_trailer_type, acqe_sli);

	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"2901 Async SLI event - Type:%d, Event Data: x%08x "
			"x%08x x%08x x%08x\n", evt_type,
			acqe_sli->event_data1, acqe_sli->event_data2,
			acqe_sli->event_data3, acqe_sli->trailer);

	port_name = phba->Port[0];
	if (port_name == 0x00)
		port_name = '?';  

	switch (evt_type) {
	case LPFC_SLI_EVENT_TYPE_OVER_TEMP:
		temp_event_data.event_type = FC_REG_TEMPERATURE_EVENT;
		temp_event_data.event_code = LPFC_THRESHOLD_TEMP;
		temp_event_data.data = (uint32_t)acqe_sli->event_data1;

		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"3190 Over Temperature:%d Celsius- Port Name %c\n",
				acqe_sli->event_data1, port_name);

		phba->sfp_warning |= LPFC_TRANSGRESSION_HIGH_TEMPERATURE;
		shost = lpfc_shost_from_vport(phba->pport);
		fc_host_post_vendor_event(shost, fc_get_event_number(),
					  sizeof(temp_event_data),
					  (char *)&temp_event_data,
					  SCSI_NL_VID_TYPE_PCI
					  | PCI_VENDOR_ID_EMULEX);
		break;
	case LPFC_SLI_EVENT_TYPE_NORM_TEMP:
		temp_event_data.event_type = FC_REG_TEMPERATURE_EVENT;
		temp_event_data.event_code = LPFC_NORMAL_TEMP;
		temp_event_data.data = (uint32_t)acqe_sli->event_data1;

		lpfc_printf_log(phba, KERN_INFO, LOG_SLI | LOG_LDS_EVENT,
				"3191 Normal Temperature:%d Celsius - Port Name %c\n",
				acqe_sli->event_data1, port_name);

		shost = lpfc_shost_from_vport(phba->pport);
		fc_host_post_vendor_event(shost, fc_get_event_number(),
					  sizeof(temp_event_data),
					  (char *)&temp_event_data,
					  SCSI_NL_VID_TYPE_PCI
					  | PCI_VENDOR_ID_EMULEX);
		break;
	case LPFC_SLI_EVENT_TYPE_MISCONFIGURED:
		misconfigured = (struct lpfc_acqe_misconfigured_event *)
					&acqe_sli->event_data1;

		 
		switch (phba->sli4_hba.lnk_info.lnk_no) {
		case LPFC_LINK_NUMBER_0:
			status = bf_get(lpfc_sli_misconfigured_port0_state,
					&misconfigured->theEvent);
			operational = bf_get(lpfc_sli_misconfigured_port0_op,
					&misconfigured->theEvent);
			break;
		case LPFC_LINK_NUMBER_1:
			status = bf_get(lpfc_sli_misconfigured_port1_state,
					&misconfigured->theEvent);
			operational = bf_get(lpfc_sli_misconfigured_port1_op,
					&misconfigured->theEvent);
			break;
		case LPFC_LINK_NUMBER_2:
			status = bf_get(lpfc_sli_misconfigured_port2_state,
					&misconfigured->theEvent);
			operational = bf_get(lpfc_sli_misconfigured_port2_op,
					&misconfigured->theEvent);
			break;
		case LPFC_LINK_NUMBER_3:
			status = bf_get(lpfc_sli_misconfigured_port3_state,
					&misconfigured->theEvent);
			operational = bf_get(lpfc_sli_misconfigured_port3_op,
					&misconfigured->theEvent);
			break;
		default:
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"3296 "
					"LPFC_SLI_EVENT_TYPE_MISCONFIGURED "
					"event: Invalid link %d",
					phba->sli4_hba.lnk_info.lnk_no);
			return;
		}

		 
		if (phba->sli4_hba.lnk_info.optic_state == status)
			return;

		switch (status) {
		case LPFC_SLI_EVENT_STATUS_VALID:
			sprintf(message, "Physical Link is functional");
			break;
		case LPFC_SLI_EVENT_STATUS_NOT_PRESENT:
			sprintf(message, "Optics faulted/incorrectly "
				"installed/not installed - Reseat optics, "
				"if issue not resolved, replace.");
			break;
		case LPFC_SLI_EVENT_STATUS_WRONG_TYPE:
			sprintf(message,
				"Optics of two types installed - Remove one "
				"optic or install matching pair of optics.");
			break;
		case LPFC_SLI_EVENT_STATUS_UNSUPPORTED:
			sprintf(message, "Incompatible optics - Replace with "
				"compatible optics for card to function.");
			break;
		case LPFC_SLI_EVENT_STATUS_UNQUALIFIED:
			sprintf(message, "Unqualified optics - Replace with "
				"Avago optics for Warranty and Technical "
				"Support - Link is%s operational",
				(operational) ? " not" : "");
			break;
		case LPFC_SLI_EVENT_STATUS_UNCERTIFIED:
			sprintf(message, "Uncertified optics - Replace with "
				"Avago-certified optics to enable link "
				"operation - Link is%s operational",
				(operational) ? " not" : "");
			break;
		default:
			 
			sprintf(message, "Unknown event status x%02x", status);
			break;
		}

		 
		rc = lpfc_sli4_read_config(phba);
		if (rc) {
			phba->lmt = 0;
			lpfc_printf_log(phba, KERN_ERR,
					LOG_TRACE_EVENT,
					"3194 Unable to retrieve supported "
					"speeds, rc = 0x%x\n", rc);
		}
		rc = lpfc_sli4_refresh_params(phba);
		if (rc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"3174 Unable to update pls support, "
					"rc x%x\n", rc);
		}
		vports = lpfc_create_vport_work_array(phba);
		if (vports != NULL) {
			for (i = 0; i <= phba->max_vports && vports[i] != NULL;
					i++) {
				shost = lpfc_shost_from_vport(vports[i]);
				lpfc_host_supported_speeds_set(shost);
			}
		}
		lpfc_destroy_vport_work_array(phba, vports);

		phba->sli4_hba.lnk_info.optic_state = status;
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"3176 Port Name %c %s\n", port_name, message);
		break;
	case LPFC_SLI_EVENT_TYPE_REMOTE_DPORT:
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"3192 Remote DPort Test Initiated - "
				"Event Data1:x%08x Event Data2: x%08x\n",
				acqe_sli->event_data1, acqe_sli->event_data2);
		break;
	case LPFC_SLI_EVENT_TYPE_PORT_PARAMS_CHG:
		 
		lpfc_sli4_cgn_parm_chg_evt(phba);
		break;
	case LPFC_SLI_EVENT_TYPE_MISCONF_FAWWN:
		 
		lpfc_log_msg(phba, KERN_WARNING, LOG_SLI | LOG_DISCOVERY,
			     "2699 Misconfigured FA-PWWN - Attached device "
			     "does not support FA-PWWN\n");
		phba->sli4_hba.fawwpn_flag &= ~LPFC_FAWWPN_FABRIC;
		memset(phba->pport->fc_portname.u.wwn, 0,
		       sizeof(struct lpfc_name));
		break;
	case LPFC_SLI_EVENT_TYPE_EEPROM_FAILURE:
		 
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
			     "2518 EEPROM failure - "
			     "Event Data1: x%08x Event Data2: x%08x\n",
			     acqe_sli->event_data1, acqe_sli->event_data2);
		break;
	case LPFC_SLI_EVENT_TYPE_CGN_SIGNAL:
		if (phba->cmf_active_mode == LPFC_CFG_OFF)
			break;
		cgn_signal = (struct lpfc_acqe_cgn_signal *)
					&acqe_sli->event_data1;
		phba->cgn_acqe_cnt++;

		cnt = bf_get(lpfc_warn_acqe, cgn_signal);
		atomic64_add(cnt, &phba->cgn_acqe_stat.warn);
		atomic64_add(cgn_signal->alarm_cnt, &phba->cgn_acqe_stat.alarm);

		 

		 
		if (cgn_signal->alarm_cnt) {
			if (phba->cgn_reg_signal == EDC_CG_SIG_WARN_ALARM) {
				 
				atomic_add(cgn_signal->alarm_cnt,
					   &phba->cgn_sync_alarm_cnt);
			}
		} else if (cnt) {
			 
			if (phba->cgn_reg_signal == EDC_CG_SIG_WARN_ONLY ||
			    phba->cgn_reg_signal == EDC_CG_SIG_WARN_ALARM) {
				 
				atomic_add(cnt, &phba->cgn_sync_warn_cnt);
			}
		}
		break;
	case LPFC_SLI_EVENT_TYPE_RD_SIGNAL:
		 
		lpfc_printf_log(phba, KERN_INFO,
				LOG_SLI | LOG_LINK_EVENT | LOG_LDS_EVENT,
				"2902 Remote Degrade Signaling: x%08x x%08x "
				"x%08x\n",
				acqe_sli->event_data1, acqe_sli->event_data2,
				acqe_sli->event_data3);
		break;
	default:
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"3193 Unrecognized SLI event, type: 0x%x",
				evt_type);
		break;
	}
}

 
static struct lpfc_nodelist *
lpfc_sli4_perform_vport_cvl(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp;
	struct Scsi_Host *shost;
	struct lpfc_hba *phba;

	if (!vport)
		return NULL;
	phba = vport->phba;
	if (!phba)
		return NULL;
	ndlp = lpfc_findnode_did(vport, Fabric_DID);
	if (!ndlp) {
		 
		ndlp = lpfc_nlp_init(vport, Fabric_DID);
		if (!ndlp)
			return NULL;
		 
		ndlp->nlp_type |= NLP_FABRIC;
		 
		lpfc_enqueue_node(vport, ndlp);
	}
	if ((phba->pport->port_state < LPFC_FLOGI) &&
		(phba->pport->port_state != LPFC_VPORT_FAILED))
		return NULL;
	 
	if ((vport != phba->pport) && (vport->port_state < LPFC_FDISC)
		&& (vport->port_state != LPFC_VPORT_FAILED))
		return NULL;
	shost = lpfc_shost_from_vport(vport);
	if (!shost)
		return NULL;
	lpfc_linkdown_port(vport);
	lpfc_cleanup_pending_mbox(vport);
	spin_lock_irq(shost->host_lock);
	vport->fc_flag |= FC_VPORT_CVL_RCVD;
	spin_unlock_irq(shost->host_lock);

	return ndlp;
}

 
static void
lpfc_sli4_perform_all_vport_cvl(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	int i;

	vports = lpfc_create_vport_work_array(phba);
	if (vports)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++)
			lpfc_sli4_perform_vport_cvl(vports[i]);
	lpfc_destroy_vport_work_array(phba, vports);
}

 
static void
lpfc_sli4_async_fip_evt(struct lpfc_hba *phba,
			struct lpfc_acqe_fip *acqe_fip)
{
	uint8_t event_type = bf_get(lpfc_trailer_type, acqe_fip);
	int rc;
	struct lpfc_vport *vport;
	struct lpfc_nodelist *ndlp;
	int active_vlink_present;
	struct lpfc_vport **vports;
	int i;

	phba->fc_eventTag = acqe_fip->event_tag;
	phba->fcoe_eventtag = acqe_fip->event_tag;
	switch (event_type) {
	case LPFC_FIP_EVENT_TYPE_NEW_FCF:
	case LPFC_FIP_EVENT_TYPE_FCF_PARAM_MOD:
		if (event_type == LPFC_FIP_EVENT_TYPE_NEW_FCF)
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"2546 New FCF event, evt_tag:x%x, "
					"index:x%x\n",
					acqe_fip->event_tag,
					acqe_fip->index);
		else
			lpfc_printf_log(phba, KERN_WARNING, LOG_FIP |
					LOG_DISCOVERY,
					"2788 FCF param modified event, "
					"evt_tag:x%x, index:x%x\n",
					acqe_fip->event_tag,
					acqe_fip->index);
		if (phba->fcf.fcf_flag & FCF_DISCOVERY) {
			 
			lpfc_printf_log(phba, KERN_INFO, LOG_FIP |
					LOG_DISCOVERY,
					"2779 Read FCF (x%x) for updating "
					"roundrobin FCF failover bmask\n",
					acqe_fip->index);
			rc = lpfc_sli4_read_fcf_rec(phba, acqe_fip->index);
		}

		 
		spin_lock_irq(&phba->hbalock);
		if (phba->hba_flag & FCF_TS_INPROG) {
			spin_unlock_irq(&phba->hbalock);
			break;
		}
		 
		if (phba->fcf.fcf_flag & (FCF_REDISC_EVT | FCF_REDISC_PEND)) {
			spin_unlock_irq(&phba->hbalock);
			break;
		}

		 
		if (phba->fcf.fcf_flag & FCF_SCAN_DONE) {
			spin_unlock_irq(&phba->hbalock);
			break;
		}
		spin_unlock_irq(&phba->hbalock);

		 
		lpfc_printf_log(phba, KERN_INFO, LOG_FIP | LOG_DISCOVERY,
				"2770 Start FCF table scan per async FCF "
				"event, evt_tag:x%x, index:x%x\n",
				acqe_fip->event_tag, acqe_fip->index);
		rc = lpfc_sli4_fcf_scan_read_fcf_rec(phba,
						     LPFC_FCOE_FCF_GET_FIRST);
		if (rc)
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"2547 Issue FCF scan read FCF mailbox "
					"command failed (x%x)\n", rc);
		break;

	case LPFC_FIP_EVENT_TYPE_FCF_TABLE_FULL:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2548 FCF Table full count 0x%x tag 0x%x\n",
				bf_get(lpfc_acqe_fip_fcf_count, acqe_fip),
				acqe_fip->event_tag);
		break;

	case LPFC_FIP_EVENT_TYPE_FCF_DEAD:
		phba->fcoe_cvl_eventtag = acqe_fip->event_tag;
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2549 FCF (x%x) disconnected from network, "
				 "tag:x%x\n", acqe_fip->index,
				 acqe_fip->event_tag);
		 
		spin_lock_irq(&phba->hbalock);
		if ((phba->fcf.fcf_flag & FCF_DISCOVERY) &&
		    (phba->fcf.current_rec.fcf_indx != acqe_fip->index)) {
			spin_unlock_irq(&phba->hbalock);
			 
			lpfc_sli4_fcf_rr_index_clear(phba, acqe_fip->index);
			break;
		}
		spin_unlock_irq(&phba->hbalock);

		 
		if (phba->fcf.current_rec.fcf_indx != acqe_fip->index)
			break;

		 
		spin_lock_irq(&phba->hbalock);
		 
		phba->fcf.fcf_flag |= FCF_DEAD_DISC;
		spin_unlock_irq(&phba->hbalock);

		lpfc_printf_log(phba, KERN_INFO, LOG_FIP | LOG_DISCOVERY,
				"2771 Start FCF fast failover process due to "
				"FCF DEAD event: evt_tag:x%x, fcf_index:x%x "
				"\n", acqe_fip->event_tag, acqe_fip->index);
		rc = lpfc_sli4_redisc_fcf_table(phba);
		if (rc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_FIP |
					LOG_TRACE_EVENT,
					"2772 Issue FCF rediscover mailbox "
					"command failed, fail through to FCF "
					"dead event\n");
			spin_lock_irq(&phba->hbalock);
			phba->fcf.fcf_flag &= ~FCF_DEAD_DISC;
			spin_unlock_irq(&phba->hbalock);
			 
			lpfc_sli4_fcf_dead_failthrough(phba);
		} else {
			 
			lpfc_sli4_clear_fcf_rr_bmask(phba);
			 
			lpfc_sli4_perform_all_vport_cvl(phba);
		}
		break;
	case LPFC_FIP_EVENT_TYPE_CVL:
		phba->fcoe_cvl_eventtag = acqe_fip->event_tag;
		lpfc_printf_log(phba, KERN_ERR,
				LOG_TRACE_EVENT,
			"2718 Clear Virtual Link Received for VPI 0x%x"
			" tag 0x%x\n", acqe_fip->index, acqe_fip->event_tag);

		vport = lpfc_find_vport_by_vpid(phba,
						acqe_fip->index);
		ndlp = lpfc_sli4_perform_vport_cvl(vport);
		if (!ndlp)
			break;
		active_vlink_present = 0;

		vports = lpfc_create_vport_work_array(phba);
		if (vports) {
			for (i = 0; i <= phba->max_vports && vports[i] != NULL;
					i++) {
				if ((!(vports[i]->fc_flag &
					FC_VPORT_CVL_RCVD)) &&
					(vports[i]->port_state > LPFC_FDISC)) {
					active_vlink_present = 1;
					break;
				}
			}
			lpfc_destroy_vport_work_array(phba, vports);
		}

		 
		if (!(vport->load_flag & FC_UNLOADING) &&
					active_vlink_present) {
			 
			mod_timer(&ndlp->nlp_delayfunc,
				  jiffies + msecs_to_jiffies(1000));
			spin_lock_irq(&ndlp->lock);
			ndlp->nlp_flag |= NLP_DELAY_TMO;
			spin_unlock_irq(&ndlp->lock);
			ndlp->nlp_last_elscmd = ELS_CMD_FDISC;
			vport->port_state = LPFC_FDISC;
		} else {
			 
			spin_lock_irq(&phba->hbalock);
			if (phba->fcf.fcf_flag & FCF_DISCOVERY) {
				spin_unlock_irq(&phba->hbalock);
				break;
			}
			 
			phba->fcf.fcf_flag |= FCF_ACVL_DISC;
			spin_unlock_irq(&phba->hbalock);
			lpfc_printf_log(phba, KERN_INFO, LOG_FIP |
					LOG_DISCOVERY,
					"2773 Start FCF failover per CVL, "
					"evt_tag:x%x\n", acqe_fip->event_tag);
			rc = lpfc_sli4_redisc_fcf_table(phba);
			if (rc) {
				lpfc_printf_log(phba, KERN_ERR, LOG_FIP |
						LOG_TRACE_EVENT,
						"2774 Issue FCF rediscover "
						"mailbox command failed, "
						"through to CVL event\n");
				spin_lock_irq(&phba->hbalock);
				phba->fcf.fcf_flag &= ~FCF_ACVL_DISC;
				spin_unlock_irq(&phba->hbalock);
				 
				lpfc_retry_pport_discovery(phba);
			} else
				 
				lpfc_sli4_clear_fcf_rr_bmask(phba);
		}
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0288 Unknown FCoE event type 0x%x event tag "
				"0x%x\n", event_type, acqe_fip->event_tag);
		break;
	}
}

 
static void
lpfc_sli4_async_dcbx_evt(struct lpfc_hba *phba,
			 struct lpfc_acqe_dcbx *acqe_dcbx)
{
	phba->fc_eventTag = acqe_dcbx->event_tag;
	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"0290 The SLI4 DCBX asynchronous event is not "
			"handled yet\n");
}

 
static void
lpfc_sli4_async_grp5_evt(struct lpfc_hba *phba,
			 struct lpfc_acqe_grp5 *acqe_grp5)
{
	uint16_t prev_ll_spd;

	phba->fc_eventTag = acqe_grp5->event_tag;
	phba->fcoe_eventtag = acqe_grp5->event_tag;
	prev_ll_spd = phba->sli4_hba.link_state.logical_speed;
	phba->sli4_hba.link_state.logical_speed =
		(bf_get(lpfc_acqe_grp5_llink_spd, acqe_grp5)) * 10;
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"2789 GRP5 Async Event: Updating logical link speed "
			"from %dMbps to %dMbps\n", prev_ll_spd,
			phba->sli4_hba.link_state.logical_speed);
}

 
static void
lpfc_sli4_async_cmstat_evt(struct lpfc_hba *phba)
{
	if (!phba->cgn_i)
		return;
	lpfc_init_congestion_stat(phba);
}

 
static void
lpfc_cgn_params_val(struct lpfc_hba *phba, struct lpfc_cgn_param *p_cfg_param)
{
	spin_lock_irq(&phba->hbalock);

	if (!lpfc_rangecheck(p_cfg_param->cgn_param_mode, LPFC_CFG_OFF,
			     LPFC_CFG_MONITOR)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_CGN_MGMT,
				"6225 CMF mode param out of range: %d\n",
				 p_cfg_param->cgn_param_mode);
		p_cfg_param->cgn_param_mode = LPFC_CFG_OFF;
	}

	spin_unlock_irq(&phba->hbalock);
}

static const char * const lpfc_cmf_mode_to_str[] = {
	"OFF",
	"MANAGED",
	"MONITOR",
};

 
static void
lpfc_cgn_params_parse(struct lpfc_hba *phba,
		      struct lpfc_cgn_param *p_cgn_param, uint32_t len)
{
	struct lpfc_cgn_info *cp;
	uint32_t crc, oldmode;
	char acr_string[4] = {0};

	 
	if (p_cgn_param->cgn_param_magic == LPFC_CFG_PARAM_MAGIC_NUM) {
		lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT | LOG_INIT,
				"4668 FW cgn parm buffer data: "
				"magic 0x%x version %d mode %d "
				"level0 %d level1 %d "
				"level2 %d byte13 %d "
				"byte14 %d byte15 %d "
				"byte11 %d byte12 %d activeMode %d\n",
				p_cgn_param->cgn_param_magic,
				p_cgn_param->cgn_param_version,
				p_cgn_param->cgn_param_mode,
				p_cgn_param->cgn_param_level0,
				p_cgn_param->cgn_param_level1,
				p_cgn_param->cgn_param_level2,
				p_cgn_param->byte13,
				p_cgn_param->byte14,
				p_cgn_param->byte15,
				p_cgn_param->byte11,
				p_cgn_param->byte12,
				phba->cmf_active_mode);

		oldmode = phba->cmf_active_mode;

		 
		lpfc_cgn_params_val(phba, p_cgn_param);

		 
		spin_lock_irq(&phba->hbalock);
		memcpy(&phba->cgn_p, p_cgn_param,
		       sizeof(struct lpfc_cgn_param));

		 
		if (phba->cgn_i) {
			cp = (struct lpfc_cgn_info *)phba->cgn_i->virt;
			cp->cgn_info_mode = phba->cgn_p.cgn_param_mode;
			cp->cgn_info_level0 = phba->cgn_p.cgn_param_level0;
			cp->cgn_info_level1 = phba->cgn_p.cgn_param_level1;
			cp->cgn_info_level2 = phba->cgn_p.cgn_param_level2;
			crc = lpfc_cgn_calc_crc32(cp, LPFC_CGN_INFO_SZ,
						  LPFC_CGN_CRC32_SEED);
			cp->cgn_info_crc = cpu_to_le32(crc);
		}
		spin_unlock_irq(&phba->hbalock);

		phba->cmf_active_mode = phba->cgn_p.cgn_param_mode;

		switch (oldmode) {
		case LPFC_CFG_OFF:
			if (phba->cgn_p.cgn_param_mode != LPFC_CFG_OFF) {
				 
				lpfc_cmf_start(phba);

				if (phba->link_state >= LPFC_LINK_UP) {
					phba->cgn_reg_fpin =
						phba->cgn_init_reg_fpin;
					phba->cgn_reg_signal =
						phba->cgn_init_reg_signal;
					lpfc_issue_els_edc(phba->pport, 0);
				}
			}
			break;
		case LPFC_CFG_MANAGED:
			switch (phba->cgn_p.cgn_param_mode) {
			case LPFC_CFG_OFF:
				 
				lpfc_cmf_stop(phba);
				if (phba->link_state >= LPFC_LINK_UP)
					lpfc_issue_els_edc(phba->pport, 0);
				break;
			case LPFC_CFG_MONITOR:
				phba->cmf_max_bytes_per_interval =
					phba->cmf_link_byte_count;

				 
				queue_work(phba->wq,
					   &phba->unblock_request_work);
				break;
			}
			break;
		case LPFC_CFG_MONITOR:
			switch (phba->cgn_p.cgn_param_mode) {
			case LPFC_CFG_OFF:
				 
				lpfc_cmf_stop(phba);
				if (phba->link_state >= LPFC_LINK_UP)
					lpfc_issue_els_edc(phba->pport, 0);
				break;
			case LPFC_CFG_MANAGED:
				lpfc_cmf_signal_init(phba);
				break;
			}
			break;
		}
		if (oldmode != LPFC_CFG_OFF ||
		    oldmode != phba->cgn_p.cgn_param_mode) {
			if (phba->cgn_p.cgn_param_mode == LPFC_CFG_MANAGED)
				scnprintf(acr_string, sizeof(acr_string), "%u",
					  phba->cgn_p.cgn_param_level0);
			else
				scnprintf(acr_string, sizeof(acr_string), "NA");

			dev_info(&phba->pcidev->dev, "%d: "
				 "4663 CMF: Mode %s acr %s\n",
				 phba->brd_no,
				 lpfc_cmf_mode_to_str
				 [phba->cgn_p.cgn_param_mode],
				 acr_string);
		}
	} else {
		lpfc_printf_log(phba, KERN_ERR, LOG_CGN_MGMT | LOG_INIT,
				"4669 FW cgn parm buf wrong magic 0x%x "
				"version %d\n", p_cgn_param->cgn_param_magic,
				p_cgn_param->cgn_param_version);
	}
}

 
int
lpfc_sli4_cgn_params_read(struct lpfc_hba *phba)
{
	int ret = 0;
	struct lpfc_cgn_param *p_cgn_param = NULL;
	u32 *pdata = NULL;
	u32 len = 0;

	 
	len = sizeof(struct lpfc_cgn_param);
	pdata = kzalloc(len, GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;
	ret = lpfc_read_object(phba, (char *)LPFC_PORT_CFG_NAME,
			       pdata, len);

	 
	if (!ret) {
		lpfc_printf_log(phba, KERN_ERR, LOG_CGN_MGMT | LOG_INIT,
				"4670 CGN RD OBJ returns no data\n");
		goto rd_obj_err;
	} else if (ret < 0) {
		 
		goto rd_obj_err;
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT | LOG_INIT,
			"6234 READ CGN PARAMS Successful %d\n", len);

	 
	p_cgn_param = (struct lpfc_cgn_param *)pdata;
	lpfc_cgn_params_parse(phba, p_cgn_param, len);

 rd_obj_err:
	kfree(pdata);
	return ret;
}

 
static int
lpfc_sli4_cgn_parm_chg_evt(struct lpfc_hba *phba)
{
	int ret = 0;

	if (!phba->sli4_hba.pc_sli4_params.cmf) {
		lpfc_printf_log(phba, KERN_ERR, LOG_CGN_MGMT | LOG_INIT,
				"4664 Cgn Evt when E2E off. Drop event\n");
		return -EACCES;
	}

	 
	ret = lpfc_sli4_cgn_params_read(phba);
	if (ret < 0) {
		lpfc_printf_log(phba, KERN_ERR, LOG_CGN_MGMT | LOG_INIT,
				"4667 Error reading Cgn Params (%d)\n",
				ret);
	} else if (!ret) {
		lpfc_printf_log(phba, KERN_ERR, LOG_CGN_MGMT | LOG_INIT,
				"4673 CGN Event empty object.\n");
	}
	return ret;
}

 
void lpfc_sli4_async_event_proc(struct lpfc_hba *phba)
{
	struct lpfc_cq_event *cq_event;
	unsigned long iflags;

	 
	spin_lock_irqsave(&phba->hbalock, iflags);
	phba->hba_flag &= ~ASYNC_EVENT;
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	 
	spin_lock_irqsave(&phba->sli4_hba.asynce_list_lock, iflags);
	while (!list_empty(&phba->sli4_hba.sp_asynce_work_queue)) {
		list_remove_head(&phba->sli4_hba.sp_asynce_work_queue,
				 cq_event, struct lpfc_cq_event, list);
		spin_unlock_irqrestore(&phba->sli4_hba.asynce_list_lock,
				       iflags);

		 
		switch (bf_get(lpfc_trailer_code, &cq_event->cqe.mcqe_cmpl)) {
		case LPFC_TRAILER_CODE_LINK:
			lpfc_sli4_async_link_evt(phba,
						 &cq_event->cqe.acqe_link);
			break;
		case LPFC_TRAILER_CODE_FCOE:
			lpfc_sli4_async_fip_evt(phba, &cq_event->cqe.acqe_fip);
			break;
		case LPFC_TRAILER_CODE_DCBX:
			lpfc_sli4_async_dcbx_evt(phba,
						 &cq_event->cqe.acqe_dcbx);
			break;
		case LPFC_TRAILER_CODE_GRP5:
			lpfc_sli4_async_grp5_evt(phba,
						 &cq_event->cqe.acqe_grp5);
			break;
		case LPFC_TRAILER_CODE_FC:
			lpfc_sli4_async_fc_evt(phba, &cq_event->cqe.acqe_fc);
			break;
		case LPFC_TRAILER_CODE_SLI:
			lpfc_sli4_async_sli_evt(phba, &cq_event->cqe.acqe_sli);
			break;
		case LPFC_TRAILER_CODE_CMSTAT:
			lpfc_sli4_async_cmstat_evt(phba);
			break;
		default:
			lpfc_printf_log(phba, KERN_ERR,
					LOG_TRACE_EVENT,
					"1804 Invalid asynchronous event code: "
					"x%x\n", bf_get(lpfc_trailer_code,
					&cq_event->cqe.mcqe_cmpl));
			break;
		}

		 
		lpfc_sli4_cq_event_release(phba, cq_event);
		spin_lock_irqsave(&phba->sli4_hba.asynce_list_lock, iflags);
	}
	spin_unlock_irqrestore(&phba->sli4_hba.asynce_list_lock, iflags);
}

 
void lpfc_sli4_fcf_redisc_event_proc(struct lpfc_hba *phba)
{
	int rc;

	spin_lock_irq(&phba->hbalock);
	 
	phba->fcf.fcf_flag &= ~FCF_REDISC_EVT;
	 
	phba->fcf.failover_rec.flag = 0;
	 
	phba->fcf.fcf_flag |= FCF_REDISC_FOV;
	spin_unlock_irq(&phba->hbalock);

	 
	lpfc_printf_log(phba, KERN_INFO, LOG_FIP | LOG_DISCOVERY,
			"2777 Start post-quiescent FCF table scan\n");
	rc = lpfc_sli4_fcf_scan_read_fcf_rec(phba, LPFC_FCOE_FCF_GET_FIRST);
	if (rc)
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2747 Issue FCF scan read FCF mailbox "
				"command failed 0x%x\n", rc);
}

 
int
lpfc_api_table_setup(struct lpfc_hba *phba, uint8_t dev_grp)
{
	int rc;

	 
	phba->pci_dev_grp = dev_grp;

	 
	if (dev_grp == LPFC_PCI_DEV_OC)
		phba->sli_rev = LPFC_SLI_REV4;

	 
	rc = lpfc_init_api_table_setup(phba, dev_grp);
	if (rc)
		return -ENODEV;
	 
	rc = lpfc_scsi_api_table_setup(phba, dev_grp);
	if (rc)
		return -ENODEV;
	 
	rc = lpfc_sli_api_table_setup(phba, dev_grp);
	if (rc)
		return -ENODEV;
	 
	rc = lpfc_mbox_api_table_setup(phba, dev_grp);
	if (rc)
		return -ENODEV;

	return 0;
}

 
static void lpfc_log_intr_mode(struct lpfc_hba *phba, uint32_t intr_mode)
{
	switch (intr_mode) {
	case 0:
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0470 Enable INTx interrupt mode.\n");
		break;
	case 1:
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0481 Enabled MSI interrupt mode.\n");
		break;
	case 2:
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0480 Enabled MSI-X interrupt mode.\n");
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0482 Illegal interrupt mode.\n");
		break;
	}
	return;
}

 
static int
lpfc_enable_pci_dev(struct lpfc_hba *phba)
{
	struct pci_dev *pdev;

	 
	if (!phba->pcidev)
		goto out_error;
	else
		pdev = phba->pcidev;
	 
	if (pci_enable_device_mem(pdev))
		goto out_error;
	 
	if (pci_request_mem_regions(pdev, LPFC_DRIVER_NAME))
		goto out_disable_device;
	 
	pci_set_master(pdev);
	pci_try_set_mwi(pdev);
	pci_save_state(pdev);

	 
	if (pci_is_pcie(pdev))
		pdev->needs_freset = 1;

	return 0;

out_disable_device:
	pci_disable_device(pdev);
out_error:
	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"1401 Failed to enable pci device\n");
	return -ENODEV;
}

 
static void
lpfc_disable_pci_dev(struct lpfc_hba *phba)
{
	struct pci_dev *pdev;

	 
	if (!phba->pcidev)
		return;
	else
		pdev = phba->pcidev;
	 
	pci_release_mem_regions(pdev);
	pci_disable_device(pdev);

	return;
}

 
void
lpfc_reset_hba(struct lpfc_hba *phba)
{
	int rc = 0;

	 
	if (!phba->cfg_enable_hba_reset) {
		phba->link_state = LPFC_HBA_ERROR;
		return;
	}

	 
	if (phba->sli.sli_flag & LPFC_SLI_ACTIVE) {
		lpfc_offline_prep(phba, LPFC_MBX_WAIT);
	} else {
		if (test_bit(MBX_TMO_ERR, &phba->bit_flags)) {
			 
			rc = lpfc_pci_function_reset(phba);
			lpfc_els_flush_all_cmd(phba);
		}
		lpfc_offline_prep(phba, LPFC_MBX_NO_WAIT);
		lpfc_sli_flush_io_rings(phba);
	}
	lpfc_offline(phba);
	clear_bit(MBX_TMO_ERR, &phba->bit_flags);
	if (unlikely(rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"8888 PCI function reset failed rc %x\n",
				rc);
	} else {
		lpfc_sli_brdrestart(phba);
		lpfc_online(phba);
		lpfc_unblock_mgmt_io(phba);
	}
}

 
uint16_t
lpfc_sli_sriov_nr_virtfn_get(struct lpfc_hba *phba)
{
	struct pci_dev *pdev = phba->pcidev;
	uint16_t nr_virtfn;
	int pos;

	pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_SRIOV);
	if (pos == 0)
		return 0;

	pci_read_config_word(pdev, pos + PCI_SRIOV_TOTAL_VF, &nr_virtfn);
	return nr_virtfn;
}

 
int
lpfc_sli_probe_sriov_nr_virtfn(struct lpfc_hba *phba, int nr_vfn)
{
	struct pci_dev *pdev = phba->pcidev;
	uint16_t max_nr_vfn;
	int rc;

	max_nr_vfn = lpfc_sli_sriov_nr_virtfn_get(phba);
	if (nr_vfn > max_nr_vfn) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3057 Requested vfs (%d) greater than "
				"supported vfs (%d)", nr_vfn, max_nr_vfn);
		return -EINVAL;
	}

	rc = pci_enable_sriov(pdev, nr_vfn);
	if (rc) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"2806 Failed to enable sriov on this device "
				"with vfn number nr_vf:%d, rc:%d\n",
				nr_vfn, rc);
	} else
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"2807 Successful enable sriov on this device "
				"with vfn number nr_vf:%d\n", nr_vfn);
	return rc;
}

static void
lpfc_unblock_requests_work(struct work_struct *work)
{
	struct lpfc_hba *phba = container_of(work, struct lpfc_hba,
					     unblock_request_work);

	lpfc_unblock_requests(phba);
}

 
static int
lpfc_setup_driver_resource_phase1(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;

	 
	atomic_set(&phba->fast_event_count, 0);
	atomic_set(&phba->dbg_log_idx, 0);
	atomic_set(&phba->dbg_log_cnt, 0);
	atomic_set(&phba->dbg_log_dmping, 0);
	spin_lock_init(&phba->hbalock);

	 
	spin_lock_init(&phba->port_list_lock);
	INIT_LIST_HEAD(&phba->port_list);

	INIT_LIST_HEAD(&phba->work_list);

	 
	init_waitqueue_head(&phba->work_waitq);

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"1403 Protocols supported %s %s %s\n",
			((phba->cfg_enable_fc4_type & LPFC_ENABLE_FCP) ?
				"SCSI" : " "),
			((phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME) ?
				"NVME" : " "),
			(phba->nvmet_support ? "NVMET" : " "));

	 
	spin_lock_init(&phba->scsi_buf_list_get_lock);
	INIT_LIST_HEAD(&phba->lpfc_scsi_buf_list_get);
	spin_lock_init(&phba->scsi_buf_list_put_lock);
	INIT_LIST_HEAD(&phba->lpfc_scsi_buf_list_put);

	 
	INIT_LIST_HEAD(&phba->fabric_iocb_list);

	 
	INIT_LIST_HEAD(&phba->elsbuf);

	 
	INIT_LIST_HEAD(&phba->fcf_conn_rec_list);

	 
	spin_lock_init(&phba->devicelock);
	INIT_LIST_HEAD(&phba->luns);

	 
	timer_setup(&psli->mbox_tmo, lpfc_mbox_timeout, 0);
	 
	timer_setup(&phba->fabric_block_timer, lpfc_fabric_block_timeout, 0);
	 
	timer_setup(&phba->eratt_poll, lpfc_poll_eratt, 0);
	 
	timer_setup(&phba->hb_tmofunc, lpfc_hb_timeout, 0);

	INIT_DELAYED_WORK(&phba->eq_delay_work, lpfc_hb_eq_delay_work);

	INIT_DELAYED_WORK(&phba->idle_stat_delay_work,
			  lpfc_idle_stat_delay_work);
	INIT_WORK(&phba->unblock_request_work, lpfc_unblock_requests_work);
	return 0;
}

 
static int
lpfc_sli_driver_resource_setup(struct lpfc_hba *phba)
{
	int rc, entry_sz;

	 

	 
	timer_setup(&phba->fcp_poll_timer, lpfc_poll_timeout, 0);

	 
	phba->work_ha_mask = (HA_ERATT | HA_MBATT | HA_LATT);
	phba->work_ha_mask |= (HA_RXMASK << (LPFC_ELS_RING * 4));

	 
	lpfc_get_cfgparam(phba);
	 

	rc = lpfc_setup_driver_resource_phase1(phba);
	if (rc)
		return -ENODEV;

	if (!phba->sli.sli3_ring)
		phba->sli.sli3_ring = kcalloc(LPFC_SLI3_MAX_RING,
					      sizeof(struct lpfc_sli_ring),
					      GFP_KERNEL);
	if (!phba->sli.sli3_ring)
		return -ENOMEM;

	 

	if (phba->sli_rev == LPFC_SLI_REV4)
		entry_sz = sizeof(struct sli4_sge);
	else
		entry_sz = sizeof(struct ulp_bde64);

	 
	if (phba->cfg_enable_bg) {
		 
		phba->cfg_sg_dma_buf_size = sizeof(struct fcp_cmnd) +
			sizeof(struct fcp_rsp) +
			(LPFC_MAX_SG_SEG_CNT * entry_sz);

		if (phba->cfg_sg_seg_cnt > LPFC_MAX_SG_SEG_CNT_DIF)
			phba->cfg_sg_seg_cnt = LPFC_MAX_SG_SEG_CNT_DIF;

		 
		phba->cfg_total_seg_cnt = LPFC_MAX_SG_SEG_CNT;
	} else {
		 
		phba->cfg_sg_dma_buf_size = sizeof(struct fcp_cmnd) +
			sizeof(struct fcp_rsp) +
			((phba->cfg_sg_seg_cnt + 2) * entry_sz);

		 
		phba->cfg_total_seg_cnt = phba->cfg_sg_seg_cnt + 2;
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT | LOG_FCP,
			"9088 INIT sg_tablesize:%d dmabuf_size:%d total_bde:%d\n",
			phba->cfg_sg_seg_cnt, phba->cfg_sg_dma_buf_size,
			phba->cfg_total_seg_cnt);

	phba->max_vpi = LPFC_MAX_VPI;
	 
	phba->max_vports = 0;

	 
	lpfc_sli_setup(phba);
	lpfc_sli_queue_init(phba);

	 
	if (lpfc_mem_alloc(phba, BPL_ALIGN_SZ))
		return -ENOMEM;

	phba->lpfc_sg_dma_buf_pool =
		dma_pool_create("lpfc_sg_dma_buf_pool",
				&phba->pcidev->dev, phba->cfg_sg_dma_buf_size,
				BPL_ALIGN_SZ, 0);

	if (!phba->lpfc_sg_dma_buf_pool)
		goto fail_free_mem;

	phba->lpfc_cmd_rsp_buf_pool =
			dma_pool_create("lpfc_cmd_rsp_buf_pool",
					&phba->pcidev->dev,
					sizeof(struct fcp_cmnd) +
					sizeof(struct fcp_rsp),
					BPL_ALIGN_SZ, 0);

	if (!phba->lpfc_cmd_rsp_buf_pool)
		goto fail_free_dma_buf_pool;

	 
	if (phba->cfg_sriov_nr_virtfn > 0) {
		rc = lpfc_sli_probe_sriov_nr_virtfn(phba,
						 phba->cfg_sriov_nr_virtfn);
		if (rc) {
			lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
					"2808 Requested number of SR-IOV "
					"virtual functions (%d) is not "
					"supported\n",
					phba->cfg_sriov_nr_virtfn);
			phba->cfg_sriov_nr_virtfn = 0;
		}
	}

	return 0;

fail_free_dma_buf_pool:
	dma_pool_destroy(phba->lpfc_sg_dma_buf_pool);
	phba->lpfc_sg_dma_buf_pool = NULL;
fail_free_mem:
	lpfc_mem_free(phba);
	return -ENOMEM;
}

 
static void
lpfc_sli_driver_resource_unset(struct lpfc_hba *phba)
{
	 
	lpfc_mem_free_all(phba);

	return;
}

 
static int
lpfc_sli4_driver_resource_setup(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mboxq;
	MAILBOX_t *mb;
	int rc, i, max_buf_size;
	int longs;
	int extra;
	uint64_t wwn;
	u32 if_type;
	u32 if_fam;

	phba->sli4_hba.num_present_cpu = lpfc_present_cpu;
	phba->sli4_hba.num_possible_cpu = cpumask_last(cpu_possible_mask) + 1;
	phba->sli4_hba.curr_disp_cpu = 0;

	 
	lpfc_get_cfgparam(phba);

	 
	rc = lpfc_setup_driver_resource_phase1(phba);
	if (rc)
		return -ENODEV;

	 
	rc = lpfc_sli4_post_status_check(phba);
	if (rc)
		return -ENODEV;

	 

	 
	phba->wq = alloc_workqueue("lpfc_wq", WQ_MEM_RECLAIM, 0);
	if (!phba->wq)
		return -ENOMEM;

	 

	timer_setup(&phba->rrq_tmr, lpfc_rrq_timeout, 0);

	 
	timer_setup(&phba->fcf.redisc_wait, lpfc_sli4_fcf_redisc_wait_tmo, 0);

	 
	hrtimer_init(&phba->cmf_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	phba->cmf_timer.function = lpfc_cmf_timer;
	 
	hrtimer_init(&phba->cmf_stats_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	phba->cmf_stats_timer.function = lpfc_cmf_stats_timer;

	 
	memset((uint8_t *)&phba->mbox_ext_buf_ctx, 0,
		sizeof(struct lpfc_mbox_ext_buf_ctx));
	INIT_LIST_HEAD(&phba->mbox_ext_buf_ctx.ext_dmabuf_list);

	phba->max_vpi = LPFC_MAX_VPI;

	 
	phba->max_vports = 0;

	 
	phba->valid_vlan = 0;
	phba->fc_map[0] = LPFC_FCOE_FCF_MAP0;
	phba->fc_map[1] = LPFC_FCOE_FCF_MAP1;
	phba->fc_map[2] = LPFC_FCOE_FCF_MAP2;

	 

	 
	INIT_LIST_HEAD(&phba->hbqs[LPFC_ELS_HBQ].hbq_buffer_list);
	phba->hbqs[LPFC_ELS_HBQ].hbq_alloc_buffer = lpfc_sli4_rb_alloc;
	phba->hbqs[LPFC_ELS_HBQ].hbq_free_buffer = lpfc_sli4_rb_free;

	 
	if (lpfc_is_vmid_enabled(phba))
		timer_setup(&phba->inactive_vmid_poll, lpfc_vmid_poll, 0);

	 
	 
	spin_lock_init(&phba->sli4_hba.abts_io_buf_list_lock);
	INIT_LIST_HEAD(&phba->sli4_hba.lpfc_abts_io_buf_list);

	if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME) {
		 
		spin_lock_init(&phba->sli4_hba.abts_nvmet_buf_list_lock);
		INIT_LIST_HEAD(&phba->sli4_hba.lpfc_abts_nvmet_ctx_list);
		INIT_LIST_HEAD(&phba->sli4_hba.lpfc_nvmet_io_wait_list);
		spin_lock_init(&phba->sli4_hba.t_active_list_lock);
		INIT_LIST_HEAD(&phba->sli4_hba.t_active_ctx_list);
	}

	 
	spin_lock_init(&phba->sli4_hba.sgl_list_lock);
	spin_lock_init(&phba->sli4_hba.nvmet_io_wait_lock);
	spin_lock_init(&phba->sli4_hba.asynce_list_lock);
	spin_lock_init(&phba->sli4_hba.els_xri_abrt_list_lock);

	 

	 
	INIT_LIST_HEAD(&phba->sli4_hba.sp_cqe_event_pool);
	 
	INIT_LIST_HEAD(&phba->sli4_hba.sp_queue_event);
	 
	INIT_LIST_HEAD(&phba->sli4_hba.sp_asynce_work_queue);
	 
	INIT_LIST_HEAD(&phba->sli4_hba.sp_els_xri_aborted_work_queue);
	 
	INIT_LIST_HEAD(&phba->sli4_hba.sp_unsol_work_queue);

	 
	INIT_LIST_HEAD(&phba->sli4_hba.lpfc_rpi_blk_list);
	INIT_LIST_HEAD(&phba->sli4_hba.lpfc_xri_blk_list);
	INIT_LIST_HEAD(&phba->sli4_hba.lpfc_vfi_blk_list);
	INIT_LIST_HEAD(&phba->lpfc_vpi_blk_list);

	 
	INIT_LIST_HEAD(&phba->sli.mboxq);
	INIT_LIST_HEAD(&phba->sli.mboxq_cmpl);

	 
	phba->sli4_hba.lnk_info.optic_state = 0xff;

	 
	rc = lpfc_mem_alloc(phba, SGL_ALIGN_SZ);
	if (rc)
		goto out_destroy_workqueue;

	 
	if (bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) >=
	    LPFC_SLI_INTF_IF_TYPE_2) {
		rc = lpfc_pci_function_reset(phba);
		if (unlikely(rc)) {
			rc = -ENODEV;
			goto out_free_mem;
		}
		phba->temp_sensor_support = 1;
	}

	 
	rc = lpfc_create_bootstrap_mbox(phba);
	if (unlikely(rc))
		goto out_free_mem;

	 
	rc = lpfc_setup_endian_order(phba);
	if (unlikely(rc))
		goto out_free_bsmbx;

	 
	rc = lpfc_sli4_read_config(phba);
	if (unlikely(rc))
		goto out_free_bsmbx;

	if (phba->sli4_hba.fawwpn_flag & LPFC_FAWWPN_CONFIG) {
		 
		phba->sli4_hba.fawwpn_flag |= LPFC_FAWWPN_FABRIC;
	}

	rc = lpfc_mem_alloc_active_rrq_pool_s4(phba);
	if (unlikely(rc))
		goto out_free_bsmbx;

	 
	if (bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) ==
	    LPFC_SLI_INTF_IF_TYPE_0) {
		rc = lpfc_pci_function_reset(phba);
		if (unlikely(rc))
			goto out_free_bsmbx;
	}

	mboxq = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool,
						       GFP_KERNEL);
	if (!mboxq) {
		rc = -ENOMEM;
		goto out_free_bsmbx;
	}

	 
	phba->nvmet_support = 0;
	if (lpfc_enable_nvmet_cnt) {

		 
		lpfc_read_nv(phba, mboxq);
		rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR,
					LOG_TRACE_EVENT,
					"6016 Mailbox failed , mbxCmd x%x "
					"READ_NV, mbxStatus x%x\n",
					bf_get(lpfc_mqe_command, &mboxq->u.mqe),
					bf_get(lpfc_mqe_status, &mboxq->u.mqe));
			mempool_free(mboxq, phba->mbox_mem_pool);
			rc = -EIO;
			goto out_free_bsmbx;
		}
		mb = &mboxq->u.mb;
		memcpy(&wwn, (char *)mb->un.varRDnvp.nodename,
		       sizeof(uint64_t));
		wwn = cpu_to_be64(wwn);
		phba->sli4_hba.wwnn.u.name = wwn;
		memcpy(&wwn, (char *)mb->un.varRDnvp.portname,
		       sizeof(uint64_t));
		 
		wwn = cpu_to_be64(wwn);
		phba->sli4_hba.wwpn.u.name = wwn;

		 
		for (i = 0; i < lpfc_enable_nvmet_cnt; i++) {
			if (wwn == lpfc_enable_nvmet[i]) {
#if (IS_ENABLED(CONFIG_NVME_TARGET_FC))
				if (lpfc_nvmet_mem_alloc(phba))
					break;

				phba->nvmet_support = 1;  

				lpfc_printf_log(phba, KERN_ERR,
						LOG_TRACE_EVENT,
						"6017 NVME Target %016llx\n",
						wwn);
#else
				lpfc_printf_log(phba, KERN_ERR,
						LOG_TRACE_EVENT,
						"6021 Can't enable NVME Target."
						" NVME_TARGET_FC infrastructure"
						" is not in kernel\n");
#endif
				 
				phba->cfg_xri_rebalancing = 0;
				if (phba->irq_chann_mode == NHT_MODE) {
					phba->cfg_irq_chann =
						phba->sli4_hba.num_present_cpu;
					phba->cfg_hdw_queue =
						phba->sli4_hba.num_present_cpu;
					phba->irq_chann_mode = NORMAL_MODE;
				}
				break;
			}
		}
	}

	lpfc_nvme_mod_param_dep(phba);

	 
	rc = lpfc_get_sli4_parameters(phba, mboxq);
	if (rc) {
		if_type = bf_get(lpfc_sli_intf_if_type,
				 &phba->sli4_hba.sli_intf);
		if_fam = bf_get(lpfc_sli_intf_sli_family,
				&phba->sli4_hba.sli_intf);
		if (phba->sli4_hba.extents_in_use &&
		    phba->sli4_hba.rpi_hdrs_in_use) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"2999 Unsupported SLI4 Parameters "
					"Extents and RPI headers enabled.\n");
			if (if_type == LPFC_SLI_INTF_IF_TYPE_0 &&
			    if_fam ==  LPFC_SLI_INTF_FAMILY_BE2) {
				mempool_free(mboxq, phba->mbox_mem_pool);
				rc = -EIO;
				goto out_free_bsmbx;
			}
		}
		if (!(if_type == LPFC_SLI_INTF_IF_TYPE_0 &&
		      if_fam == LPFC_SLI_INTF_FAMILY_BE2)) {
			mempool_free(mboxq, phba->mbox_mem_pool);
			rc = -EIO;
			goto out_free_bsmbx;
		}
	}

	 
	extra = 2;
	if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME)
		extra++;

	 
	max_buf_size = (2 * SLI4_PAGE_SIZE);

	 
	if (phba->sli3_options & LPFC_SLI3_BG_ENABLED) {
		 

		 
		phba->cfg_sg_dma_buf_size = sizeof(struct fcp_cmnd) +
				sizeof(struct fcp_rsp) + max_buf_size;

		 
		phba->cfg_total_seg_cnt = LPFC_MAX_SGL_SEG_CNT;

		 
		if (phba->cfg_enable_bg &&
		    phba->cfg_sg_seg_cnt > LPFC_MAX_BG_SLI4_SEG_CNT_DIF)
			phba->cfg_scsi_seg_cnt = LPFC_MAX_BG_SLI4_SEG_CNT_DIF;
		else
			phba->cfg_scsi_seg_cnt = phba->cfg_sg_seg_cnt;

	} else {
		 
		phba->cfg_sg_dma_buf_size = sizeof(struct fcp_cmnd) +
				sizeof(struct fcp_rsp) +
				((phba->cfg_sg_seg_cnt + extra) *
				sizeof(struct sli4_sge));

		 
		phba->cfg_total_seg_cnt = phba->cfg_sg_seg_cnt + extra;
		phba->cfg_scsi_seg_cnt = phba->cfg_sg_seg_cnt;

		 
	}

	if (phba->cfg_xpsgl && !phba->nvmet_support)
		phba->cfg_sg_dma_buf_size = LPFC_DEFAULT_XPSGL_SIZE;
	else if (phba->cfg_sg_dma_buf_size  <= LPFC_MIN_SG_SLI4_BUF_SZ)
		phba->cfg_sg_dma_buf_size = LPFC_MIN_SG_SLI4_BUF_SZ;
	else
		phba->cfg_sg_dma_buf_size =
				SLI4_PAGE_ALIGN(phba->cfg_sg_dma_buf_size);

	phba->border_sge_num = phba->cfg_sg_dma_buf_size /
			       sizeof(struct sli4_sge);

	 
	if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME) {
		if (phba->cfg_sg_seg_cnt > LPFC_MAX_NVME_SEG_CNT) {
			lpfc_printf_log(phba, KERN_INFO, LOG_NVME | LOG_INIT,
					"6300 Reducing NVME sg segment "
					"cnt to %d\n",
					LPFC_MAX_NVME_SEG_CNT);
			phba->cfg_nvme_seg_cnt = LPFC_MAX_NVME_SEG_CNT;
		} else
			phba->cfg_nvme_seg_cnt = phba->cfg_sg_seg_cnt;
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT | LOG_FCP,
			"9087 sg_seg_cnt:%d dmabuf_size:%d "
			"total:%d scsi:%d nvme:%d\n",
			phba->cfg_sg_seg_cnt, phba->cfg_sg_dma_buf_size,
			phba->cfg_total_seg_cnt,  phba->cfg_scsi_seg_cnt,
			phba->cfg_nvme_seg_cnt);

	if (phba->cfg_sg_dma_buf_size < SLI4_PAGE_SIZE)
		i = phba->cfg_sg_dma_buf_size;
	else
		i = SLI4_PAGE_SIZE;

	phba->lpfc_sg_dma_buf_pool =
			dma_pool_create("lpfc_sg_dma_buf_pool",
					&phba->pcidev->dev,
					phba->cfg_sg_dma_buf_size,
					i, 0);
	if (!phba->lpfc_sg_dma_buf_pool) {
		rc = -ENOMEM;
		goto out_free_bsmbx;
	}

	phba->lpfc_cmd_rsp_buf_pool =
			dma_pool_create("lpfc_cmd_rsp_buf_pool",
					&phba->pcidev->dev,
					sizeof(struct fcp_cmnd) +
					sizeof(struct fcp_rsp),
					i, 0);
	if (!phba->lpfc_cmd_rsp_buf_pool) {
		rc = -ENOMEM;
		goto out_free_sg_dma_buf;
	}

	mempool_free(mboxq, phba->mbox_mem_pool);

	 
	lpfc_sli4_oas_verify(phba);

	 
	lpfc_sli4_ras_init(phba);

	 
	rc = lpfc_sli4_queue_verify(phba);
	if (rc)
		goto out_free_cmd_rsp_buf;

	 
	rc = lpfc_sli4_cq_event_pool_create(phba);
	if (rc)
		goto out_free_cmd_rsp_buf;

	 
	lpfc_init_sgl_list(phba);

	 
	rc = lpfc_init_active_sgl_array(phba);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"1430 Failed to initialize sgl list.\n");
		goto out_destroy_cq_event_pool;
	}
	rc = lpfc_sli4_init_rpi_hdrs(phba);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"1432 Failed to initialize rpi headers.\n");
		goto out_free_active_sgl;
	}

	 
	longs = (LPFC_SLI4_FCF_TBL_INDX_MAX + BITS_PER_LONG - 1)/BITS_PER_LONG;
	phba->fcf.fcf_rr_bmask = kcalloc(longs, sizeof(unsigned long),
					 GFP_KERNEL);
	if (!phba->fcf.fcf_rr_bmask) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2759 Failed allocate memory for FCF round "
				"robin failover bmask\n");
		rc = -ENOMEM;
		goto out_remove_rpi_hdrs;
	}

	phba->sli4_hba.hba_eq_hdl = kcalloc(phba->cfg_irq_chann,
					    sizeof(struct lpfc_hba_eq_hdl),
					    GFP_KERNEL);
	if (!phba->sli4_hba.hba_eq_hdl) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2572 Failed allocate memory for "
				"fast-path per-EQ handle array\n");
		rc = -ENOMEM;
		goto out_free_fcf_rr_bmask;
	}

	phba->sli4_hba.cpu_map = kcalloc(phba->sli4_hba.num_possible_cpu,
					sizeof(struct lpfc_vector_map_info),
					GFP_KERNEL);
	if (!phba->sli4_hba.cpu_map) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3327 Failed allocate memory for msi-x "
				"interrupt vector mapping\n");
		rc = -ENOMEM;
		goto out_free_hba_eq_hdl;
	}

	phba->sli4_hba.eq_info = alloc_percpu(struct lpfc_eq_intr_info);
	if (!phba->sli4_hba.eq_info) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3321 Failed allocation for per_cpu stats\n");
		rc = -ENOMEM;
		goto out_free_hba_cpu_map;
	}

	phba->sli4_hba.idle_stat = kcalloc(phba->sli4_hba.num_possible_cpu,
					   sizeof(*phba->sli4_hba.idle_stat),
					   GFP_KERNEL);
	if (!phba->sli4_hba.idle_stat) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3390 Failed allocation for idle_stat\n");
		rc = -ENOMEM;
		goto out_free_hba_eq_info;
	}

#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	phba->sli4_hba.c_stat = alloc_percpu(struct lpfc_hdwq_stat);
	if (!phba->sli4_hba.c_stat) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3332 Failed allocating per cpu hdwq stats\n");
		rc = -ENOMEM;
		goto out_free_hba_idle_stat;
	}
#endif

	phba->cmf_stat = alloc_percpu(struct lpfc_cgn_stat);
	if (!phba->cmf_stat) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3331 Failed allocating per cpu cgn stats\n");
		rc = -ENOMEM;
		goto out_free_hba_hdwq_info;
	}

	 
	if (phba->cfg_sriov_nr_virtfn > 0) {
		rc = lpfc_sli_probe_sriov_nr_virtfn(phba,
						 phba->cfg_sriov_nr_virtfn);
		if (rc) {
			lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
					"3020 Requested number of SR-IOV "
					"virtual functions (%d) is not "
					"supported\n",
					phba->cfg_sriov_nr_virtfn);
			phba->cfg_sriov_nr_virtfn = 0;
		}
	}

	return 0;

out_free_hba_hdwq_info:
#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	free_percpu(phba->sli4_hba.c_stat);
out_free_hba_idle_stat:
#endif
	kfree(phba->sli4_hba.idle_stat);
out_free_hba_eq_info:
	free_percpu(phba->sli4_hba.eq_info);
out_free_hba_cpu_map:
	kfree(phba->sli4_hba.cpu_map);
out_free_hba_eq_hdl:
	kfree(phba->sli4_hba.hba_eq_hdl);
out_free_fcf_rr_bmask:
	kfree(phba->fcf.fcf_rr_bmask);
out_remove_rpi_hdrs:
	lpfc_sli4_remove_rpi_hdrs(phba);
out_free_active_sgl:
	lpfc_free_active_sgl(phba);
out_destroy_cq_event_pool:
	lpfc_sli4_cq_event_pool_destroy(phba);
out_free_cmd_rsp_buf:
	dma_pool_destroy(phba->lpfc_cmd_rsp_buf_pool);
	phba->lpfc_cmd_rsp_buf_pool = NULL;
out_free_sg_dma_buf:
	dma_pool_destroy(phba->lpfc_sg_dma_buf_pool);
	phba->lpfc_sg_dma_buf_pool = NULL;
out_free_bsmbx:
	lpfc_destroy_bootstrap_mbox(phba);
out_free_mem:
	lpfc_mem_free(phba);
out_destroy_workqueue:
	destroy_workqueue(phba->wq);
	phba->wq = NULL;
	return rc;
}

 
static void
lpfc_sli4_driver_resource_unset(struct lpfc_hba *phba)
{
	struct lpfc_fcf_conn_entry *conn_entry, *next_conn_entry;

	free_percpu(phba->sli4_hba.eq_info);
#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	free_percpu(phba->sli4_hba.c_stat);
#endif
	free_percpu(phba->cmf_stat);
	kfree(phba->sli4_hba.idle_stat);

	 
	kfree(phba->sli4_hba.cpu_map);
	phba->sli4_hba.num_possible_cpu = 0;
	phba->sli4_hba.num_present_cpu = 0;
	phba->sli4_hba.curr_disp_cpu = 0;
	cpumask_clear(&phba->sli4_hba.irq_aff_mask);

	 
	kfree(phba->sli4_hba.hba_eq_hdl);

	 
	lpfc_sli4_remove_rpi_hdrs(phba);
	lpfc_sli4_remove_rpis(phba);

	 
	kfree(phba->fcf.fcf_rr_bmask);

	 
	lpfc_free_active_sgl(phba);
	lpfc_free_els_sgl_list(phba);
	lpfc_free_nvmet_sgl_list(phba);

	 
	lpfc_sli4_cq_event_release_all(phba);
	lpfc_sli4_cq_event_pool_destroy(phba);

	 
	lpfc_sli4_dealloc_resource_identifiers(phba);

	 
	lpfc_destroy_bootstrap_mbox(phba);

	 
	lpfc_mem_free_all(phba);

	 
	list_for_each_entry_safe(conn_entry, next_conn_entry,
		&phba->fcf_conn_rec_list, list) {
		list_del_init(&conn_entry->list);
		kfree(conn_entry);
	}

	return;
}

 
int
lpfc_init_api_table_setup(struct lpfc_hba *phba, uint8_t dev_grp)
{
	phba->lpfc_hba_init_link = lpfc_hba_init_link;
	phba->lpfc_hba_down_link = lpfc_hba_down_link;
	phba->lpfc_selective_reset = lpfc_selective_reset;
	switch (dev_grp) {
	case LPFC_PCI_DEV_LP:
		phba->lpfc_hba_down_post = lpfc_hba_down_post_s3;
		phba->lpfc_handle_eratt = lpfc_handle_eratt_s3;
		phba->lpfc_stop_port = lpfc_stop_port_s3;
		break;
	case LPFC_PCI_DEV_OC:
		phba->lpfc_hba_down_post = lpfc_hba_down_post_s4;
		phba->lpfc_handle_eratt = lpfc_handle_eratt_s4;
		phba->lpfc_stop_port = lpfc_stop_port_s4;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1431 Invalid HBA PCI-device group: 0x%x\n",
				dev_grp);
		return -ENODEV;
	}
	return 0;
}

 
static int
lpfc_setup_driver_resource_phase2(struct lpfc_hba *phba)
{
	int error;

	 
	phba->worker_thread = kthread_run(lpfc_do_work, phba,
					  "lpfc_worker_%d", phba->brd_no);
	if (IS_ERR(phba->worker_thread)) {
		error = PTR_ERR(phba->worker_thread);
		return error;
	}

	return 0;
}

 
static void
lpfc_unset_driver_resource_phase2(struct lpfc_hba *phba)
{
	if (phba->wq) {
		destroy_workqueue(phba->wq);
		phba->wq = NULL;
	}

	 
	if (phba->worker_thread)
		kthread_stop(phba->worker_thread);
}

 
void
lpfc_free_iocb_list(struct lpfc_hba *phba)
{
	struct lpfc_iocbq *iocbq_entry = NULL, *iocbq_next = NULL;

	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(iocbq_entry, iocbq_next,
				 &phba->lpfc_iocb_list, list) {
		list_del(&iocbq_entry->list);
		kfree(iocbq_entry);
		phba->total_iocbq_bufs--;
	}
	spin_unlock_irq(&phba->hbalock);

	return;
}

 
int
lpfc_init_iocb_list(struct lpfc_hba *phba, int iocb_count)
{
	struct lpfc_iocbq *iocbq_entry = NULL;
	uint16_t iotag;
	int i;

	 
	INIT_LIST_HEAD(&phba->lpfc_iocb_list);
	for (i = 0; i < iocb_count; i++) {
		iocbq_entry = kzalloc(sizeof(struct lpfc_iocbq), GFP_KERNEL);
		if (iocbq_entry == NULL) {
			printk(KERN_ERR "%s: only allocated %d iocbs of "
				"expected %d count. Unloading driver.\n",
				__func__, i, iocb_count);
			goto out_free_iocbq;
		}

		iotag = lpfc_sli_next_iotag(phba, iocbq_entry);
		if (iotag == 0) {
			kfree(iocbq_entry);
			printk(KERN_ERR "%s: failed to allocate IOTAG. "
				"Unloading driver.\n", __func__);
			goto out_free_iocbq;
		}
		iocbq_entry->sli4_lxritag = NO_XRI;
		iocbq_entry->sli4_xritag = NO_XRI;

		spin_lock_irq(&phba->hbalock);
		list_add(&iocbq_entry->list, &phba->lpfc_iocb_list);
		phba->total_iocbq_bufs++;
		spin_unlock_irq(&phba->hbalock);
	}

	return 0;

out_free_iocbq:
	lpfc_free_iocb_list(phba);

	return -ENOMEM;
}

 
void
lpfc_free_sgl_list(struct lpfc_hba *phba, struct list_head *sglq_list)
{
	struct lpfc_sglq *sglq_entry = NULL, *sglq_next = NULL;

	list_for_each_entry_safe(sglq_entry, sglq_next, sglq_list, list) {
		list_del(&sglq_entry->list);
		lpfc_mbuf_free(phba, sglq_entry->virt, sglq_entry->phys);
		kfree(sglq_entry);
	}
}

 
static void
lpfc_free_els_sgl_list(struct lpfc_hba *phba)
{
	LIST_HEAD(sglq_list);

	 
	spin_lock_irq(&phba->sli4_hba.sgl_list_lock);
	list_splice_init(&phba->sli4_hba.lpfc_els_sgl_list, &sglq_list);
	spin_unlock_irq(&phba->sli4_hba.sgl_list_lock);

	 
	lpfc_free_sgl_list(phba, &sglq_list);
}

 
static void
lpfc_free_nvmet_sgl_list(struct lpfc_hba *phba)
{
	struct lpfc_sglq *sglq_entry = NULL, *sglq_next = NULL;
	LIST_HEAD(sglq_list);

	 
	spin_lock_irq(&phba->hbalock);
	spin_lock(&phba->sli4_hba.sgl_list_lock);
	list_splice_init(&phba->sli4_hba.lpfc_nvmet_sgl_list, &sglq_list);
	spin_unlock(&phba->sli4_hba.sgl_list_lock);
	spin_unlock_irq(&phba->hbalock);

	 
	list_for_each_entry_safe(sglq_entry, sglq_next, &sglq_list, list) {
		list_del(&sglq_entry->list);
		lpfc_nvmet_buf_free(phba, sglq_entry->virt, sglq_entry->phys);
		kfree(sglq_entry);
	}

	 
	phba->sli4_hba.nvmet_xri_cnt = 0;
}

 
static int
lpfc_init_active_sgl_array(struct lpfc_hba *phba)
{
	int size;
	size = sizeof(struct lpfc_sglq *);
	size *= phba->sli4_hba.max_cfg_param.max_xri;

	phba->sli4_hba.lpfc_sglq_active_list =
		kzalloc(size, GFP_KERNEL);
	if (!phba->sli4_hba.lpfc_sglq_active_list)
		return -ENOMEM;
	return 0;
}

 
static void
lpfc_free_active_sgl(struct lpfc_hba *phba)
{
	kfree(phba->sli4_hba.lpfc_sglq_active_list);
}

 
static void
lpfc_init_sgl_list(struct lpfc_hba *phba)
{
	 
	INIT_LIST_HEAD(&phba->sli4_hba.lpfc_els_sgl_list);
	INIT_LIST_HEAD(&phba->sli4_hba.lpfc_abts_els_sgl_list);
	INIT_LIST_HEAD(&phba->sli4_hba.lpfc_nvmet_sgl_list);
	INIT_LIST_HEAD(&phba->sli4_hba.lpfc_abts_nvmet_ctx_list);

	 
	phba->sli4_hba.els_xri_cnt = 0;

	 
	phba->sli4_hba.io_xri_cnt = 0;
}

 
int
lpfc_sli4_init_rpi_hdrs(struct lpfc_hba *phba)
{
	int rc = 0;
	struct lpfc_rpi_hdr *rpi_hdr;

	INIT_LIST_HEAD(&phba->sli4_hba.lpfc_rpi_hdr_list);
	if (!phba->sli4_hba.rpi_hdrs_in_use)
		return rc;
	if (phba->sli4_hba.extents_in_use)
		return -EIO;

	rpi_hdr = lpfc_sli4_create_rpi_hdr(phba);
	if (!rpi_hdr) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0391 Error during rpi post operation\n");
		lpfc_sli4_remove_rpis(phba);
		rc = -ENODEV;
	}

	return rc;
}

 
struct lpfc_rpi_hdr *
lpfc_sli4_create_rpi_hdr(struct lpfc_hba *phba)
{
	uint16_t rpi_limit, curr_rpi_range;
	struct lpfc_dmabuf *dmabuf;
	struct lpfc_rpi_hdr *rpi_hdr;

	 
	if (!phba->sli4_hba.rpi_hdrs_in_use)
		return NULL;
	if (phba->sli4_hba.extents_in_use)
		return NULL;

	 
	rpi_limit = phba->sli4_hba.max_cfg_param.max_rpi;

	spin_lock_irq(&phba->hbalock);
	 
	curr_rpi_range = phba->sli4_hba.next_rpi;
	spin_unlock_irq(&phba->hbalock);

	 
	if (curr_rpi_range == rpi_limit)
		return NULL;

	 
	dmabuf = kzalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (!dmabuf)
		return NULL;

	dmabuf->virt = dma_alloc_coherent(&phba->pcidev->dev,
					  LPFC_HDR_TEMPLATE_SIZE,
					  &dmabuf->phys, GFP_KERNEL);
	if (!dmabuf->virt) {
		rpi_hdr = NULL;
		goto err_free_dmabuf;
	}

	if (!IS_ALIGNED(dmabuf->phys, LPFC_HDR_TEMPLATE_SIZE)) {
		rpi_hdr = NULL;
		goto err_free_coherent;
	}

	 
	rpi_hdr = kzalloc(sizeof(struct lpfc_rpi_hdr), GFP_KERNEL);
	if (!rpi_hdr)
		goto err_free_coherent;

	rpi_hdr->dmabuf = dmabuf;
	rpi_hdr->len = LPFC_HDR_TEMPLATE_SIZE;
	rpi_hdr->page_count = 1;
	spin_lock_irq(&phba->hbalock);

	 
	rpi_hdr->start_rpi = curr_rpi_range;
	rpi_hdr->next_rpi = phba->sli4_hba.next_rpi + LPFC_RPI_HDR_COUNT;
	list_add_tail(&rpi_hdr->list, &phba->sli4_hba.lpfc_rpi_hdr_list);

	spin_unlock_irq(&phba->hbalock);
	return rpi_hdr;

 err_free_coherent:
	dma_free_coherent(&phba->pcidev->dev, LPFC_HDR_TEMPLATE_SIZE,
			  dmabuf->virt, dmabuf->phys);
 err_free_dmabuf:
	kfree(dmabuf);
	return NULL;
}

 
void
lpfc_sli4_remove_rpi_hdrs(struct lpfc_hba *phba)
{
	struct lpfc_rpi_hdr *rpi_hdr, *next_rpi_hdr;

	if (!phba->sli4_hba.rpi_hdrs_in_use)
		goto exit;

	list_for_each_entry_safe(rpi_hdr, next_rpi_hdr,
				 &phba->sli4_hba.lpfc_rpi_hdr_list, list) {
		list_del(&rpi_hdr->list);
		dma_free_coherent(&phba->pcidev->dev, rpi_hdr->len,
				  rpi_hdr->dmabuf->virt, rpi_hdr->dmabuf->phys);
		kfree(rpi_hdr->dmabuf);
		kfree(rpi_hdr);
	}
 exit:
	 
	phba->sli4_hba.next_rpi = 0;
}

 
static struct lpfc_hba *
lpfc_hba_alloc(struct pci_dev *pdev)
{
	struct lpfc_hba *phba;

	 
	phba = kzalloc(sizeof(struct lpfc_hba), GFP_KERNEL);
	if (!phba) {
		dev_err(&pdev->dev, "failed to allocate hba struct\n");
		return NULL;
	}

	 
	phba->pcidev = pdev;

	 
	phba->brd_no = lpfc_get_instance();
	if (phba->brd_no < 0) {
		kfree(phba);
		return NULL;
	}
	phba->eratt_poll_interval = LPFC_ERATT_POLL_INTERVAL;

	spin_lock_init(&phba->ct_ev_lock);
	INIT_LIST_HEAD(&phba->ct_ev_waiters);

	return phba;
}

 
static void
lpfc_hba_free(struct lpfc_hba *phba)
{
	if (phba->sli_rev == LPFC_SLI_REV4)
		kfree(phba->sli4_hba.hdwq);

	 
	idr_remove(&lpfc_hba_index, phba->brd_no);

	 
	kfree(phba->sli.sli3_ring);
	phba->sli.sli3_ring = NULL;

	kfree(phba);
	return;
}

 
void
lpfc_setup_fdmi_mask(struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;

	vport->load_flag |= FC_ALLOW_FDMI;
	if (phba->cfg_enable_SmartSAN ||
	    phba->cfg_fdmi_on == LPFC_FDMI_SUPPORT) {
		 
		vport->fdmi_hba_mask = LPFC_FDMI2_HBA_ATTR;
		if (phba->cfg_enable_SmartSAN)
			vport->fdmi_port_mask = LPFC_FDMI2_SMART_ATTR;
		else
			vport->fdmi_port_mask = LPFC_FDMI2_PORT_ATTR;
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_DISCOVERY,
			"6077 Setup FDMI mask: hba x%x port x%x\n",
			vport->fdmi_hba_mask, vport->fdmi_port_mask);
}

 
static int
lpfc_create_shost(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport;
	struct Scsi_Host  *shost;

	 
	phba->fc_edtov = FF_DEF_EDTOV;
	phba->fc_ratov = FF_DEF_RATOV;
	phba->fc_altov = FF_DEF_ALTOV;
	phba->fc_arbtov = FF_DEF_ARBTOV;

	atomic_set(&phba->sdev_cnt, 0);
	vport = lpfc_create_port(phba, phba->brd_no, &phba->pcidev->dev);
	if (!vport)
		return -ENODEV;

	shost = lpfc_shost_from_vport(vport);
	phba->pport = vport;

	if (phba->nvmet_support) {
		 
		phba->targetport = NULL;
		phba->cfg_enable_fc4_type = LPFC_ENABLE_NVME;
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT | LOG_NVME_DISC,
				"6076 NVME Target Found\n");
	}

	lpfc_debugfs_initialize(vport);
	 
	pci_set_drvdata(phba->pcidev, shost);

	lpfc_setup_fdmi_mask(vport);

	 
	return 0;
}

 
static void
lpfc_destroy_shost(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pport;

	 
	destroy_port(vport);

	return;
}

 
static void
lpfc_setup_bg(struct lpfc_hba *phba, struct Scsi_Host *shost)
{
	uint32_t old_mask;
	uint32_t old_guard;

	if (phba->cfg_prot_mask && phba->cfg_prot_guard) {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"1478 Registering BlockGuard with the "
				"SCSI layer\n");

		old_mask = phba->cfg_prot_mask;
		old_guard = phba->cfg_prot_guard;

		 
		phba->cfg_prot_mask &= (SHOST_DIF_TYPE1_PROTECTION |
			SHOST_DIX_TYPE0_PROTECTION |
			SHOST_DIX_TYPE1_PROTECTION);
		phba->cfg_prot_guard &= (SHOST_DIX_GUARD_IP |
					 SHOST_DIX_GUARD_CRC);

		 
		if (phba->cfg_prot_mask == SHOST_DIX_TYPE1_PROTECTION)
			phba->cfg_prot_mask |= SHOST_DIF_TYPE1_PROTECTION;

		if (phba->cfg_prot_mask && phba->cfg_prot_guard) {
			if ((old_mask != phba->cfg_prot_mask) ||
				(old_guard != phba->cfg_prot_guard))
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"1475 Registering BlockGuard with the "
					"SCSI layer: mask %d  guard %d\n",
					phba->cfg_prot_mask,
					phba->cfg_prot_guard);

			scsi_host_set_prot(shost, phba->cfg_prot_mask);
			scsi_host_set_guard(shost, phba->cfg_prot_guard);
		} else
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"1479 Not Registering BlockGuard with the SCSI "
				"layer, Bad protection parameters: %d %d\n",
				old_mask, old_guard);
	}
}

 
static void
lpfc_post_init_setup(struct lpfc_hba *phba)
{
	struct Scsi_Host  *shost;
	struct lpfc_adapter_event_header adapter_event;

	 
	lpfc_get_hba_model_desc(phba, phba->ModelName, phba->ModelDesc);

	 
	shost = pci_get_drvdata(phba->pcidev);
	shost->can_queue = phba->cfg_hba_queue_depth - 10;

	lpfc_host_attrib_init(shost);

	if (phba->cfg_poll & DISABLE_FCP_RING_INT) {
		spin_lock_irq(shost->host_lock);
		lpfc_poll_start_timer(phba);
		spin_unlock_irq(shost->host_lock);
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"0428 Perform SCSI scan\n");
	 
	adapter_event.event_type = FC_REG_ADAPTER_EVENT;
	adapter_event.subcategory = LPFC_EVENT_ARRIVAL;
	fc_host_post_vendor_event(shost, fc_get_event_number(),
				  sizeof(adapter_event),
				  (char *) &adapter_event,
				  LPFC_NL_VENDOR_ID);
	return;
}

 
static int
lpfc_sli_pci_mem_setup(struct lpfc_hba *phba)
{
	struct pci_dev *pdev = phba->pcidev;
	unsigned long bar0map_len, bar2map_len;
	int i, hbq_count;
	void *ptr;
	int error;

	if (!pdev)
		return -ENODEV;

	 
	error = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (error)
		error = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (error)
		return error;
	error = -ENODEV;

	 
	phba->pci_bar0_map = pci_resource_start(pdev, 0);
	bar0map_len = pci_resource_len(pdev, 0);

	phba->pci_bar2_map = pci_resource_start(pdev, 2);
	bar2map_len = pci_resource_len(pdev, 2);

	 
	phba->slim_memmap_p = ioremap(phba->pci_bar0_map, bar0map_len);
	if (!phba->slim_memmap_p) {
		dev_printk(KERN_ERR, &pdev->dev,
			   "ioremap failed for SLIM memory.\n");
		goto out;
	}

	 
	phba->ctrl_regs_memmap_p = ioremap(phba->pci_bar2_map, bar2map_len);
	if (!phba->ctrl_regs_memmap_p) {
		dev_printk(KERN_ERR, &pdev->dev,
			   "ioremap failed for HBA control registers.\n");
		goto out_iounmap_slim;
	}

	 
	phba->slim2p.virt = dma_alloc_coherent(&pdev->dev, SLI2_SLIM_SIZE,
					       &phba->slim2p.phys, GFP_KERNEL);
	if (!phba->slim2p.virt)
		goto out_iounmap;

	phba->mbox = phba->slim2p.virt + offsetof(struct lpfc_sli2_slim, mbx);
	phba->mbox_ext = (phba->slim2p.virt +
		offsetof(struct lpfc_sli2_slim, mbx_ext_words));
	phba->pcb = (phba->slim2p.virt + offsetof(struct lpfc_sli2_slim, pcb));
	phba->IOCBs = (phba->slim2p.virt +
		       offsetof(struct lpfc_sli2_slim, IOCBs));

	phba->hbqslimp.virt = dma_alloc_coherent(&pdev->dev,
						 lpfc_sli_hbq_size(),
						 &phba->hbqslimp.phys,
						 GFP_KERNEL);
	if (!phba->hbqslimp.virt)
		goto out_free_slim;

	hbq_count = lpfc_sli_hbq_count();
	ptr = phba->hbqslimp.virt;
	for (i = 0; i < hbq_count; ++i) {
		phba->hbqs[i].hbq_virt = ptr;
		INIT_LIST_HEAD(&phba->hbqs[i].hbq_buffer_list);
		ptr += (lpfc_hbq_defs[i]->entry_count *
			sizeof(struct lpfc_hbq_entry));
	}
	phba->hbqs[LPFC_ELS_HBQ].hbq_alloc_buffer = lpfc_els_hbq_alloc;
	phba->hbqs[LPFC_ELS_HBQ].hbq_free_buffer = lpfc_els_hbq_free;

	memset(phba->hbqslimp.virt, 0, lpfc_sli_hbq_size());

	phba->MBslimaddr = phba->slim_memmap_p;
	phba->HAregaddr = phba->ctrl_regs_memmap_p + HA_REG_OFFSET;
	phba->CAregaddr = phba->ctrl_regs_memmap_p + CA_REG_OFFSET;
	phba->HSregaddr = phba->ctrl_regs_memmap_p + HS_REG_OFFSET;
	phba->HCregaddr = phba->ctrl_regs_memmap_p + HC_REG_OFFSET;

	return 0;

out_free_slim:
	dma_free_coherent(&pdev->dev, SLI2_SLIM_SIZE,
			  phba->slim2p.virt, phba->slim2p.phys);
out_iounmap:
	iounmap(phba->ctrl_regs_memmap_p);
out_iounmap_slim:
	iounmap(phba->slim_memmap_p);
out:
	return error;
}

 
static void
lpfc_sli_pci_mem_unset(struct lpfc_hba *phba)
{
	struct pci_dev *pdev;

	 
	if (!phba->pcidev)
		return;
	else
		pdev = phba->pcidev;

	 
	dma_free_coherent(&pdev->dev, lpfc_sli_hbq_size(),
			  phba->hbqslimp.virt, phba->hbqslimp.phys);
	dma_free_coherent(&pdev->dev, SLI2_SLIM_SIZE,
			  phba->slim2p.virt, phba->slim2p.phys);

	 
	iounmap(phba->ctrl_regs_memmap_p);
	iounmap(phba->slim_memmap_p);

	return;
}

 
int
lpfc_sli4_post_status_check(struct lpfc_hba *phba)
{
	struct lpfc_register portsmphr_reg, uerrlo_reg, uerrhi_reg;
	struct lpfc_register reg_data;
	int i, port_error = 0;
	uint32_t if_type;

	memset(&portsmphr_reg, 0, sizeof(portsmphr_reg));
	memset(&reg_data, 0, sizeof(reg_data));
	if (!phba->sli4_hba.PSMPHRregaddr)
		return -ENODEV;

	 
	for (i = 0; i < 3000; i++) {
		if (lpfc_readl(phba->sli4_hba.PSMPHRregaddr,
			&portsmphr_reg.word0) ||
			(bf_get(lpfc_port_smphr_perr, &portsmphr_reg))) {
			 
			port_error = -ENODEV;
			break;
		}
		if (LPFC_POST_STAGE_PORT_READY ==
		    bf_get(lpfc_port_smphr_port_status, &portsmphr_reg))
			break;
		msleep(10);
	}

	 
	if (port_error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"1408 Port Failed POST - portsmphr=0x%x, "
			"perr=x%x, sfi=x%x, nip=x%x, ipc=x%x, scr1=x%x, "
			"scr2=x%x, hscratch=x%x, pstatus=x%x\n",
			portsmphr_reg.word0,
			bf_get(lpfc_port_smphr_perr, &portsmphr_reg),
			bf_get(lpfc_port_smphr_sfi, &portsmphr_reg),
			bf_get(lpfc_port_smphr_nip, &portsmphr_reg),
			bf_get(lpfc_port_smphr_ipc, &portsmphr_reg),
			bf_get(lpfc_port_smphr_scr1, &portsmphr_reg),
			bf_get(lpfc_port_smphr_scr2, &portsmphr_reg),
			bf_get(lpfc_port_smphr_host_scratch, &portsmphr_reg),
			bf_get(lpfc_port_smphr_port_status, &portsmphr_reg));
	} else {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"2534 Device Info: SLIFamily=0x%x, "
				"SLIRev=0x%x, IFType=0x%x, SLIHint_1=0x%x, "
				"SLIHint_2=0x%x, FT=0x%x\n",
				bf_get(lpfc_sli_intf_sli_family,
				       &phba->sli4_hba.sli_intf),
				bf_get(lpfc_sli_intf_slirev,
				       &phba->sli4_hba.sli_intf),
				bf_get(lpfc_sli_intf_if_type,
				       &phba->sli4_hba.sli_intf),
				bf_get(lpfc_sli_intf_sli_hint1,
				       &phba->sli4_hba.sli_intf),
				bf_get(lpfc_sli_intf_sli_hint2,
				       &phba->sli4_hba.sli_intf),
				bf_get(lpfc_sli_intf_func_type,
				       &phba->sli4_hba.sli_intf));
		 
		if_type = bf_get(lpfc_sli_intf_if_type,
				 &phba->sli4_hba.sli_intf);
		switch (if_type) {
		case LPFC_SLI_INTF_IF_TYPE_0:
			phba->sli4_hba.ue_mask_lo =
			      readl(phba->sli4_hba.u.if_type0.UEMASKLOregaddr);
			phba->sli4_hba.ue_mask_hi =
			      readl(phba->sli4_hba.u.if_type0.UEMASKHIregaddr);
			uerrlo_reg.word0 =
			      readl(phba->sli4_hba.u.if_type0.UERRLOregaddr);
			uerrhi_reg.word0 =
				readl(phba->sli4_hba.u.if_type0.UERRHIregaddr);
			if ((~phba->sli4_hba.ue_mask_lo & uerrlo_reg.word0) ||
			    (~phba->sli4_hba.ue_mask_hi & uerrhi_reg.word0)) {
				lpfc_printf_log(phba, KERN_ERR,
						LOG_TRACE_EVENT,
						"1422 Unrecoverable Error "
						"Detected during POST "
						"uerr_lo_reg=0x%x, "
						"uerr_hi_reg=0x%x, "
						"ue_mask_lo_reg=0x%x, "
						"ue_mask_hi_reg=0x%x\n",
						uerrlo_reg.word0,
						uerrhi_reg.word0,
						phba->sli4_hba.ue_mask_lo,
						phba->sli4_hba.ue_mask_hi);
				port_error = -ENODEV;
			}
			break;
		case LPFC_SLI_INTF_IF_TYPE_2:
		case LPFC_SLI_INTF_IF_TYPE_6:
			 
			if (lpfc_readl(phba->sli4_hba.u.if_type2.STATUSregaddr,
				&reg_data.word0) ||
				lpfc_sli4_unrecoverable_port(&reg_data)) {
				phba->work_status[0] =
					readl(phba->sli4_hba.u.if_type2.
					      ERR1regaddr);
				phba->work_status[1] =
					readl(phba->sli4_hba.u.if_type2.
					      ERR2regaddr);
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"2888 Unrecoverable port error "
					"following POST: port status reg "
					"0x%x, port_smphr reg 0x%x, "
					"error 1=0x%x, error 2=0x%x\n",
					reg_data.word0,
					portsmphr_reg.word0,
					phba->work_status[0],
					phba->work_status[1]);
				port_error = -ENODEV;
				break;
			}

			if (lpfc_pldv_detect &&
			    bf_get(lpfc_sli_intf_sli_family,
				   &phba->sli4_hba.sli_intf) ==
					LPFC_SLI_INTF_FAMILY_G6)
				pci_write_config_byte(phba->pcidev,
						      LPFC_SLI_INTF, CFG_PLD);
			break;
		case LPFC_SLI_INTF_IF_TYPE_1:
		default:
			break;
		}
	}
	return port_error;
}

 
static void
lpfc_sli4_bar0_register_memmap(struct lpfc_hba *phba, uint32_t if_type)
{
	switch (if_type) {
	case LPFC_SLI_INTF_IF_TYPE_0:
		phba->sli4_hba.u.if_type0.UERRLOregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_UERR_STATUS_LO;
		phba->sli4_hba.u.if_type0.UERRHIregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_UERR_STATUS_HI;
		phba->sli4_hba.u.if_type0.UEMASKLOregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_UE_MASK_LO;
		phba->sli4_hba.u.if_type0.UEMASKHIregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_UE_MASK_HI;
		phba->sli4_hba.SLIINTFregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_SLI_INTF;
		break;
	case LPFC_SLI_INTF_IF_TYPE_2:
		phba->sli4_hba.u.if_type2.EQDregaddr =
			phba->sli4_hba.conf_regs_memmap_p +
						LPFC_CTL_PORT_EQ_DELAY_OFFSET;
		phba->sli4_hba.u.if_type2.ERR1regaddr =
			phba->sli4_hba.conf_regs_memmap_p +
						LPFC_CTL_PORT_ER1_OFFSET;
		phba->sli4_hba.u.if_type2.ERR2regaddr =
			phba->sli4_hba.conf_regs_memmap_p +
						LPFC_CTL_PORT_ER2_OFFSET;
		phba->sli4_hba.u.if_type2.CTRLregaddr =
			phba->sli4_hba.conf_regs_memmap_p +
						LPFC_CTL_PORT_CTL_OFFSET;
		phba->sli4_hba.u.if_type2.STATUSregaddr =
			phba->sli4_hba.conf_regs_memmap_p +
						LPFC_CTL_PORT_STA_OFFSET;
		phba->sli4_hba.SLIINTFregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_SLI_INTF;
		phba->sli4_hba.PSMPHRregaddr =
			phba->sli4_hba.conf_regs_memmap_p +
						LPFC_CTL_PORT_SEM_OFFSET;
		phba->sli4_hba.RQDBregaddr =
			phba->sli4_hba.conf_regs_memmap_p +
						LPFC_ULP0_RQ_DOORBELL;
		phba->sli4_hba.WQDBregaddr =
			phba->sli4_hba.conf_regs_memmap_p +
						LPFC_ULP0_WQ_DOORBELL;
		phba->sli4_hba.CQDBregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_EQCQ_DOORBELL;
		phba->sli4_hba.EQDBregaddr = phba->sli4_hba.CQDBregaddr;
		phba->sli4_hba.MQDBregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_MQ_DOORBELL;
		phba->sli4_hba.BMBXregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_BMBX;
		break;
	case LPFC_SLI_INTF_IF_TYPE_6:
		phba->sli4_hba.u.if_type2.EQDregaddr =
			phba->sli4_hba.conf_regs_memmap_p +
						LPFC_CTL_PORT_EQ_DELAY_OFFSET;
		phba->sli4_hba.u.if_type2.ERR1regaddr =
			phba->sli4_hba.conf_regs_memmap_p +
						LPFC_CTL_PORT_ER1_OFFSET;
		phba->sli4_hba.u.if_type2.ERR2regaddr =
			phba->sli4_hba.conf_regs_memmap_p +
						LPFC_CTL_PORT_ER2_OFFSET;
		phba->sli4_hba.u.if_type2.CTRLregaddr =
			phba->sli4_hba.conf_regs_memmap_p +
						LPFC_CTL_PORT_CTL_OFFSET;
		phba->sli4_hba.u.if_type2.STATUSregaddr =
			phba->sli4_hba.conf_regs_memmap_p +
						LPFC_CTL_PORT_STA_OFFSET;
		phba->sli4_hba.PSMPHRregaddr =
			phba->sli4_hba.conf_regs_memmap_p +
						LPFC_CTL_PORT_SEM_OFFSET;
		phba->sli4_hba.BMBXregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_BMBX;
		break;
	case LPFC_SLI_INTF_IF_TYPE_1:
	default:
		dev_printk(KERN_ERR, &phba->pcidev->dev,
			   "FATAL - unsupported SLI4 interface type - %d\n",
			   if_type);
		break;
	}
}

 
static void
lpfc_sli4_bar1_register_memmap(struct lpfc_hba *phba, uint32_t if_type)
{
	switch (if_type) {
	case LPFC_SLI_INTF_IF_TYPE_0:
		phba->sli4_hba.PSMPHRregaddr =
			phba->sli4_hba.ctrl_regs_memmap_p +
			LPFC_SLIPORT_IF0_SMPHR;
		phba->sli4_hba.ISRregaddr = phba->sli4_hba.ctrl_regs_memmap_p +
			LPFC_HST_ISR0;
		phba->sli4_hba.IMRregaddr = phba->sli4_hba.ctrl_regs_memmap_p +
			LPFC_HST_IMR0;
		phba->sli4_hba.ISCRregaddr = phba->sli4_hba.ctrl_regs_memmap_p +
			LPFC_HST_ISCR0;
		break;
	case LPFC_SLI_INTF_IF_TYPE_6:
		phba->sli4_hba.RQDBregaddr = phba->sli4_hba.drbl_regs_memmap_p +
			LPFC_IF6_RQ_DOORBELL;
		phba->sli4_hba.WQDBregaddr = phba->sli4_hba.drbl_regs_memmap_p +
			LPFC_IF6_WQ_DOORBELL;
		phba->sli4_hba.CQDBregaddr = phba->sli4_hba.drbl_regs_memmap_p +
			LPFC_IF6_CQ_DOORBELL;
		phba->sli4_hba.EQDBregaddr = phba->sli4_hba.drbl_regs_memmap_p +
			LPFC_IF6_EQ_DOORBELL;
		phba->sli4_hba.MQDBregaddr = phba->sli4_hba.drbl_regs_memmap_p +
			LPFC_IF6_MQ_DOORBELL;
		break;
	case LPFC_SLI_INTF_IF_TYPE_2:
	case LPFC_SLI_INTF_IF_TYPE_1:
	default:
		dev_err(&phba->pcidev->dev,
			   "FATAL - unsupported SLI4 interface type - %d\n",
			   if_type);
		break;
	}
}

 
static int
lpfc_sli4_bar2_register_memmap(struct lpfc_hba *phba, uint32_t vf)
{
	if (vf > LPFC_VIR_FUNC_MAX)
		return -ENODEV;

	phba->sli4_hba.RQDBregaddr = (phba->sli4_hba.drbl_regs_memmap_p +
				vf * LPFC_VFR_PAGE_SIZE +
					LPFC_ULP0_RQ_DOORBELL);
	phba->sli4_hba.WQDBregaddr = (phba->sli4_hba.drbl_regs_memmap_p +
				vf * LPFC_VFR_PAGE_SIZE +
					LPFC_ULP0_WQ_DOORBELL);
	phba->sli4_hba.CQDBregaddr = (phba->sli4_hba.drbl_regs_memmap_p +
				vf * LPFC_VFR_PAGE_SIZE +
					LPFC_EQCQ_DOORBELL);
	phba->sli4_hba.EQDBregaddr = phba->sli4_hba.CQDBregaddr;
	phba->sli4_hba.MQDBregaddr = (phba->sli4_hba.drbl_regs_memmap_p +
				vf * LPFC_VFR_PAGE_SIZE + LPFC_MQ_DOORBELL);
	phba->sli4_hba.BMBXregaddr = (phba->sli4_hba.drbl_regs_memmap_p +
				vf * LPFC_VFR_PAGE_SIZE + LPFC_BMBX);
	return 0;
}

 
static int
lpfc_create_bootstrap_mbox(struct lpfc_hba *phba)
{
	uint32_t bmbx_size;
	struct lpfc_dmabuf *dmabuf;
	struct dma_address *dma_address;
	uint32_t pa_addr;
	uint64_t phys_addr;

	dmabuf = kzalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (!dmabuf)
		return -ENOMEM;

	 
	bmbx_size = sizeof(struct lpfc_bmbx_create) + (LPFC_ALIGN_16_BYTE - 1);
	dmabuf->virt = dma_alloc_coherent(&phba->pcidev->dev, bmbx_size,
					  &dmabuf->phys, GFP_KERNEL);
	if (!dmabuf->virt) {
		kfree(dmabuf);
		return -ENOMEM;
	}

	 
	phba->sli4_hba.bmbx.dmabuf = dmabuf;
	phba->sli4_hba.bmbx.bmbx_size = bmbx_size;

	phba->sli4_hba.bmbx.avirt = PTR_ALIGN(dmabuf->virt,
					      LPFC_ALIGN_16_BYTE);
	phba->sli4_hba.bmbx.aphys = ALIGN(dmabuf->phys,
					      LPFC_ALIGN_16_BYTE);

	 
	dma_address = &phba->sli4_hba.bmbx.dma_address;
	phys_addr = (uint64_t)phba->sli4_hba.bmbx.aphys;
	pa_addr = (uint32_t) ((phys_addr >> 34) & 0x3fffffff);
	dma_address->addr_hi = (uint32_t) ((pa_addr << 2) |
					   LPFC_BMBX_BIT1_ADDR_HI);

	pa_addr = (uint32_t) ((phba->sli4_hba.bmbx.aphys >> 4) & 0x3fffffff);
	dma_address->addr_lo = (uint32_t) ((pa_addr << 2) |
					   LPFC_BMBX_BIT1_ADDR_LO);
	return 0;
}

 
static void
lpfc_destroy_bootstrap_mbox(struct lpfc_hba *phba)
{
	dma_free_coherent(&phba->pcidev->dev,
			  phba->sli4_hba.bmbx.bmbx_size,
			  phba->sli4_hba.bmbx.dmabuf->virt,
			  phba->sli4_hba.bmbx.dmabuf->phys);

	kfree(phba->sli4_hba.bmbx.dmabuf);
	memset(&phba->sli4_hba.bmbx, 0, sizeof(struct lpfc_bmbx));
}

static const char * const lpfc_topo_to_str[] = {
	"Loop then P2P",
	"Loopback",
	"P2P Only",
	"Unsupported",
	"Loop Only",
	"Unsupported",
	"P2P then Loop",
};

#define	LINK_FLAGS_DEF	0x0
#define	LINK_FLAGS_P2P	0x1
#define	LINK_FLAGS_LOOP	0x2
 
static void
lpfc_map_topology(struct lpfc_hba *phba, struct lpfc_mbx_read_config *rd_config)
{
	u8 ptv, tf, pt;

	ptv = bf_get(lpfc_mbx_rd_conf_ptv, rd_config);
	tf = bf_get(lpfc_mbx_rd_conf_tf, rd_config);
	pt = bf_get(lpfc_mbx_rd_conf_pt, rd_config);

	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"2027 Read Config Data : ptv:0x%x, tf:0x%x pt:0x%x",
			 ptv, tf, pt);
	if (!ptv) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"2019 FW does not support persistent topology "
				"Using driver parameter defined value [%s]",
				lpfc_topo_to_str[phba->cfg_topology]);
		return;
	}
	 
	phba->hba_flag |= HBA_PERSISTENT_TOPO;

	 
	if ((bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) ==
		    LPFC_SLI_INTF_IF_TYPE_6) ||
	    (bf_get(lpfc_sli_intf_sli_family, &phba->sli4_hba.sli_intf) ==
		    LPFC_SLI_INTF_FAMILY_G6)) {
		if (!tf) {
			phba->cfg_topology = ((pt == LINK_FLAGS_LOOP)
					? FLAGS_TOPOLOGY_MODE_LOOP
					: FLAGS_TOPOLOGY_MODE_PT_PT);
		} else {
			phba->hba_flag &= ~HBA_PERSISTENT_TOPO;
		}
	} else {  
		if (tf) {
			 
			phba->cfg_topology = (pt ? FLAGS_TOPOLOGY_MODE_PT_LOOP :
					      FLAGS_TOPOLOGY_MODE_LOOP_PT);
		} else {
			phba->cfg_topology = ((pt == LINK_FLAGS_P2P)
					? FLAGS_TOPOLOGY_MODE_PT_PT
					: FLAGS_TOPOLOGY_MODE_LOOP);
		}
	}
	if (phba->hba_flag & HBA_PERSISTENT_TOPO) {
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"2020 Using persistent topology value [%s]",
				lpfc_topo_to_str[phba->cfg_topology]);
	} else {
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"2021 Invalid topology values from FW "
				"Using driver parameter defined value [%s]",
				lpfc_topo_to_str[phba->cfg_topology]);
	}
}

 
int
lpfc_sli4_read_config(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *pmb;
	struct lpfc_mbx_read_config *rd_config;
	union  lpfc_sli4_cfg_shdr *shdr;
	uint32_t shdr_status, shdr_add_status;
	struct lpfc_mbx_get_func_cfg *get_func_cfg;
	struct lpfc_rsrc_desc_fcfcoe *desc;
	char *pdesc_0;
	uint16_t forced_link_speed;
	uint32_t if_type, qmin, fawwpn;
	int length, i, rc = 0, rc2;

	pmb = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2011 Unable to allocate memory for issuing "
				"SLI_CONFIG_SPECIAL mailbox command\n");
		return -ENOMEM;
	}

	lpfc_read_config(phba, pmb);

	rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);
	if (rc != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2012 Mailbox failed , mbxCmd x%x "
				"READ_CONFIG, mbxStatus x%x\n",
				bf_get(lpfc_mqe_command, &pmb->u.mqe),
				bf_get(lpfc_mqe_status, &pmb->u.mqe));
		rc = -EIO;
	} else {
		rd_config = &pmb->u.mqe.un.rd_config;
		if (bf_get(lpfc_mbx_rd_conf_lnk_ldv, rd_config)) {
			phba->sli4_hba.lnk_info.lnk_dv = LPFC_LNK_DAT_VAL;
			phba->sli4_hba.lnk_info.lnk_tp =
				bf_get(lpfc_mbx_rd_conf_lnk_type, rd_config);
			phba->sli4_hba.lnk_info.lnk_no =
				bf_get(lpfc_mbx_rd_conf_lnk_numb, rd_config);
			lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
					"3081 lnk_type:%d, lnk_numb:%d\n",
					phba->sli4_hba.lnk_info.lnk_tp,
					phba->sli4_hba.lnk_info.lnk_no);
		} else
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					"3082 Mailbox (x%x) returned ldv:x0\n",
					bf_get(lpfc_mqe_command, &pmb->u.mqe));
		if (bf_get(lpfc_mbx_rd_conf_bbscn_def, rd_config)) {
			phba->bbcredit_support = 1;
			phba->sli4_hba.bbscn_params.word0 = rd_config->word8;
		}

		fawwpn = bf_get(lpfc_mbx_rd_conf_fawwpn, rd_config);

		if (fawwpn) {
			lpfc_printf_log(phba, KERN_INFO,
					LOG_INIT | LOG_DISCOVERY,
					"2702 READ_CONFIG: FA-PWWN is "
					"configured on\n");
			phba->sli4_hba.fawwpn_flag |= LPFC_FAWWPN_CONFIG;
		} else {
			 
			phba->sli4_hba.fawwpn_flag &= ~LPFC_FAWWPN_CONFIG;
		}

		phba->sli4_hba.conf_trunk =
			bf_get(lpfc_mbx_rd_conf_trunk, rd_config);
		phba->sli4_hba.extents_in_use =
			bf_get(lpfc_mbx_rd_conf_extnts_inuse, rd_config);

		phba->sli4_hba.max_cfg_param.max_xri =
			bf_get(lpfc_mbx_rd_conf_xri_count, rd_config);
		 
		if (is_kdump_kernel() &&
		    phba->sli4_hba.max_cfg_param.max_xri > 512)
			phba->sli4_hba.max_cfg_param.max_xri = 512;
		phba->sli4_hba.max_cfg_param.xri_base =
			bf_get(lpfc_mbx_rd_conf_xri_base, rd_config);
		phba->sli4_hba.max_cfg_param.max_vpi =
			bf_get(lpfc_mbx_rd_conf_vpi_count, rd_config);
		 
		if (phba->sli4_hba.max_cfg_param.max_vpi > LPFC_MAX_VPORTS)
			phba->sli4_hba.max_cfg_param.max_vpi = LPFC_MAX_VPORTS;
		phba->sli4_hba.max_cfg_param.vpi_base =
			bf_get(lpfc_mbx_rd_conf_vpi_base, rd_config);
		phba->sli4_hba.max_cfg_param.max_rpi =
			bf_get(lpfc_mbx_rd_conf_rpi_count, rd_config);
		phba->sli4_hba.max_cfg_param.rpi_base =
			bf_get(lpfc_mbx_rd_conf_rpi_base, rd_config);
		phba->sli4_hba.max_cfg_param.max_vfi =
			bf_get(lpfc_mbx_rd_conf_vfi_count, rd_config);
		phba->sli4_hba.max_cfg_param.vfi_base =
			bf_get(lpfc_mbx_rd_conf_vfi_base, rd_config);
		phba->sli4_hba.max_cfg_param.max_fcfi =
			bf_get(lpfc_mbx_rd_conf_fcfi_count, rd_config);
		phba->sli4_hba.max_cfg_param.max_eq =
			bf_get(lpfc_mbx_rd_conf_eq_count, rd_config);
		phba->sli4_hba.max_cfg_param.max_rq =
			bf_get(lpfc_mbx_rd_conf_rq_count, rd_config);
		phba->sli4_hba.max_cfg_param.max_wq =
			bf_get(lpfc_mbx_rd_conf_wq_count, rd_config);
		phba->sli4_hba.max_cfg_param.max_cq =
			bf_get(lpfc_mbx_rd_conf_cq_count, rd_config);
		phba->lmt = bf_get(lpfc_mbx_rd_conf_lmt, rd_config);
		phba->sli4_hba.next_xri = phba->sli4_hba.max_cfg_param.xri_base;
		phba->vpi_base = phba->sli4_hba.max_cfg_param.vpi_base;
		phba->vfi_base = phba->sli4_hba.max_cfg_param.vfi_base;
		phba->max_vpi = (phba->sli4_hba.max_cfg_param.max_vpi > 0) ?
				(phba->sli4_hba.max_cfg_param.max_vpi - 1) : 0;
		phba->max_vports = phba->max_vpi;

		 
		phba->cgn_reg_fpin = LPFC_CGN_FPIN_BOTH;
		phba->cgn_reg_signal = EDC_CG_SIG_NOTSUPPORTED;
		phba->cgn_sig_freq = lpfc_fabric_cgn_frequency;

		if (lpfc_use_cgn_signal) {
			if (bf_get(lpfc_mbx_rd_conf_wcs, rd_config)) {
				phba->cgn_reg_signal = EDC_CG_SIG_WARN_ONLY;
				phba->cgn_reg_fpin &= ~LPFC_CGN_FPIN_WARN;
			}
			if (bf_get(lpfc_mbx_rd_conf_acs, rd_config)) {
				 
				if (phba->cgn_reg_signal !=
				    EDC_CG_SIG_WARN_ONLY) {
					 
					phba->cgn_reg_fpin = LPFC_CGN_FPIN_BOTH;
					phba->cgn_reg_signal =
						EDC_CG_SIG_NOTSUPPORTED;
				} else {
					phba->cgn_reg_signal =
						EDC_CG_SIG_WARN_ALARM;
					phba->cgn_reg_fpin =
						LPFC_CGN_FPIN_NONE;
				}
			}
		}

		 
		phba->cgn_init_reg_fpin = phba->cgn_reg_fpin;
		phba->cgn_init_reg_signal = phba->cgn_reg_signal;

		lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
				"6446 READ_CONFIG reg_sig x%x reg_fpin:x%x\n",
				phba->cgn_reg_signal, phba->cgn_reg_fpin);

		lpfc_map_topology(phba, rd_config);
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"2003 cfg params Extents? %d "
				"XRI(B:%d M:%d), "
				"VPI(B:%d M:%d) "
				"VFI(B:%d M:%d) "
				"RPI(B:%d M:%d) "
				"FCFI:%d EQ:%d CQ:%d WQ:%d RQ:%d lmt:x%x\n",
				phba->sli4_hba.extents_in_use,
				phba->sli4_hba.max_cfg_param.xri_base,
				phba->sli4_hba.max_cfg_param.max_xri,
				phba->sli4_hba.max_cfg_param.vpi_base,
				phba->sli4_hba.max_cfg_param.max_vpi,
				phba->sli4_hba.max_cfg_param.vfi_base,
				phba->sli4_hba.max_cfg_param.max_vfi,
				phba->sli4_hba.max_cfg_param.rpi_base,
				phba->sli4_hba.max_cfg_param.max_rpi,
				phba->sli4_hba.max_cfg_param.max_fcfi,
				phba->sli4_hba.max_cfg_param.max_eq,
				phba->sli4_hba.max_cfg_param.max_cq,
				phba->sli4_hba.max_cfg_param.max_wq,
				phba->sli4_hba.max_cfg_param.max_rq,
				phba->lmt);

		 
		qmin = phba->sli4_hba.max_cfg_param.max_wq;
		if (phba->sli4_hba.max_cfg_param.max_cq < qmin)
			qmin = phba->sli4_hba.max_cfg_param.max_cq;
		 
		qmin -= 4;
		if (phba->sli4_hba.max_cfg_param.max_eq < qmin)
			qmin = phba->sli4_hba.max_cfg_param.max_eq;

		 
		if ((phba->cfg_irq_chann > qmin) ||
		    (phba->cfg_hdw_queue > qmin)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"2005 Reducing Queues - "
					"FW resource limitation: "
					"WQ %d CQ %d EQ %d: min %d: "
					"IRQ %d HDWQ %d\n",
					phba->sli4_hba.max_cfg_param.max_wq,
					phba->sli4_hba.max_cfg_param.max_cq,
					phba->sli4_hba.max_cfg_param.max_eq,
					qmin, phba->cfg_irq_chann,
					phba->cfg_hdw_queue);

			if (phba->cfg_irq_chann > qmin)
				phba->cfg_irq_chann = qmin;
			if (phba->cfg_hdw_queue > qmin)
				phba->cfg_hdw_queue = qmin;
		}
	}

	if (rc)
		goto read_cfg_out;

	 
	if_type = bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf);
	if (if_type >= LPFC_SLI_INTF_IF_TYPE_2) {
		forced_link_speed =
			bf_get(lpfc_mbx_rd_conf_link_speed, rd_config);
		if (forced_link_speed) {
			phba->hba_flag |= HBA_FORCED_LINK_SPEED;

			switch (forced_link_speed) {
			case LINK_SPEED_1G:
				phba->cfg_link_speed =
					LPFC_USER_LINK_SPEED_1G;
				break;
			case LINK_SPEED_2G:
				phba->cfg_link_speed =
					LPFC_USER_LINK_SPEED_2G;
				break;
			case LINK_SPEED_4G:
				phba->cfg_link_speed =
					LPFC_USER_LINK_SPEED_4G;
				break;
			case LINK_SPEED_8G:
				phba->cfg_link_speed =
					LPFC_USER_LINK_SPEED_8G;
				break;
			case LINK_SPEED_10G:
				phba->cfg_link_speed =
					LPFC_USER_LINK_SPEED_10G;
				break;
			case LINK_SPEED_16G:
				phba->cfg_link_speed =
					LPFC_USER_LINK_SPEED_16G;
				break;
			case LINK_SPEED_32G:
				phba->cfg_link_speed =
					LPFC_USER_LINK_SPEED_32G;
				break;
			case LINK_SPEED_64G:
				phba->cfg_link_speed =
					LPFC_USER_LINK_SPEED_64G;
				break;
			case 0xffff:
				phba->cfg_link_speed =
					LPFC_USER_LINK_SPEED_AUTO;
				break;
			default:
				lpfc_printf_log(phba, KERN_ERR,
						LOG_TRACE_EVENT,
						"0047 Unrecognized link "
						"speed : %d\n",
						forced_link_speed);
				phba->cfg_link_speed =
					LPFC_USER_LINK_SPEED_AUTO;
			}
		}
	}

	 
	length = phba->sli4_hba.max_cfg_param.max_xri -
			lpfc_sli4_get_els_iocb_cnt(phba);
	if (phba->cfg_hba_queue_depth > length) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"3361 HBA queue depth changed from %d to %d\n",
				phba->cfg_hba_queue_depth, length);
		phba->cfg_hba_queue_depth = length;
	}

	if (bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) <
	    LPFC_SLI_INTF_IF_TYPE_2)
		goto read_cfg_out;

	 
	length = (sizeof(struct lpfc_mbx_get_func_cfg) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, pmb, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_GET_FUNCTION_CONFIG,
			 length, LPFC_SLI4_MBX_EMBED);

	rc2 = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);
	shdr = (union lpfc_sli4_cfg_shdr *)
				&pmb->u.mqe.un.sli4_config.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (rc2 || shdr_status || shdr_add_status) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3026 Mailbox failed , mbxCmd x%x "
				"GET_FUNCTION_CONFIG, mbxStatus x%x\n",
				bf_get(lpfc_mqe_command, &pmb->u.mqe),
				bf_get(lpfc_mqe_status, &pmb->u.mqe));
		goto read_cfg_out;
	}

	 
	get_func_cfg = &pmb->u.mqe.un.get_func_cfg;

	pdesc_0 = (char *)&get_func_cfg->func_cfg.desc[0];
	desc = (struct lpfc_rsrc_desc_fcfcoe *)pdesc_0;
	length = bf_get(lpfc_rsrc_desc_fcfcoe_length, desc);
	if (length == LPFC_RSRC_DESC_TYPE_FCFCOE_V0_RSVD)
		length = LPFC_RSRC_DESC_TYPE_FCFCOE_V0_LENGTH;
	else if (length != LPFC_RSRC_DESC_TYPE_FCFCOE_V1_LENGTH)
		goto read_cfg_out;

	for (i = 0; i < LPFC_RSRC_DESC_MAX_NUM; i++) {
		desc = (struct lpfc_rsrc_desc_fcfcoe *)(pdesc_0 + length * i);
		if (LPFC_RSRC_DESC_TYPE_FCFCOE ==
		    bf_get(lpfc_rsrc_desc_fcfcoe_type, desc)) {
			phba->sli4_hba.iov.pf_number =
				bf_get(lpfc_rsrc_desc_fcfcoe_pfnum, desc);
			phba->sli4_hba.iov.vf_number =
				bf_get(lpfc_rsrc_desc_fcfcoe_vfnum, desc);
			break;
		}
	}

	if (i < LPFC_RSRC_DESC_MAX_NUM)
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"3027 GET_FUNCTION_CONFIG: pf_number:%d, "
				"vf_number:%d\n", phba->sli4_hba.iov.pf_number,
				phba->sli4_hba.iov.vf_number);
	else
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3028 GET_FUNCTION_CONFIG: failed to find "
				"Resource Descriptor:x%x\n",
				LPFC_RSRC_DESC_TYPE_FCFCOE);

read_cfg_out:
	mempool_free(pmb, phba->mbox_mem_pool);
	return rc;
}

 
static int
lpfc_setup_endian_order(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mboxq;
	uint32_t if_type, rc = 0;
	uint32_t endian_mb_data[2] = {HOST_ENDIAN_LOW_WORD0,
				      HOST_ENDIAN_HIGH_WORD1};

	if_type = bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf);
	switch (if_type) {
	case LPFC_SLI_INTF_IF_TYPE_0:
		mboxq = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool,
						       GFP_KERNEL);
		if (!mboxq) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0492 Unable to allocate memory for "
					"issuing SLI_CONFIG_SPECIAL mailbox "
					"command\n");
			return -ENOMEM;
		}

		 
		memset(mboxq, 0, sizeof(LPFC_MBOXQ_t));
		memcpy(&mboxq->u.mqe, &endian_mb_data, sizeof(endian_mb_data));
		rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0493 SLI_CONFIG_SPECIAL mailbox "
					"failed with status x%x\n",
					rc);
			rc = -EIO;
		}
		mempool_free(mboxq, phba->mbox_mem_pool);
		break;
	case LPFC_SLI_INTF_IF_TYPE_6:
	case LPFC_SLI_INTF_IF_TYPE_2:
	case LPFC_SLI_INTF_IF_TYPE_1:
	default:
		break;
	}
	return rc;
}

 
static int
lpfc_sli4_queue_verify(struct lpfc_hba *phba)
{
	 

	if (phba->nvmet_support) {
		if (phba->cfg_hdw_queue < phba->cfg_nvmet_mrq)
			phba->cfg_nvmet_mrq = phba->cfg_hdw_queue;
		if (phba->cfg_nvmet_mrq > LPFC_NVMET_MRQ_MAX)
			phba->cfg_nvmet_mrq = LPFC_NVMET_MRQ_MAX;
	}

	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2574 IO channels: hdwQ %d IRQ %d MRQ: %d\n",
			phba->cfg_hdw_queue, phba->cfg_irq_chann,
			phba->cfg_nvmet_mrq);

	 
	phba->sli4_hba.eq_esize = LPFC_EQE_SIZE_4B;
	phba->sli4_hba.eq_ecount = LPFC_EQE_DEF_COUNT;

	 
	phba->sli4_hba.cq_esize = LPFC_CQE_SIZE;
	phba->sli4_hba.cq_ecount = LPFC_CQE_DEF_COUNT;
	return 0;
}

static int
lpfc_alloc_io_wq_cq(struct lpfc_hba *phba, int idx)
{
	struct lpfc_queue *qdesc;
	u32 wqesize;
	int cpu;

	cpu = lpfc_find_cpu_handle(phba, idx, LPFC_FIND_BY_HDWQ);
	 
	if (phba->enab_exp_wqcq_pages)
		 
		qdesc = lpfc_sli4_queue_alloc(phba, LPFC_EXPANDED_PAGE_SIZE,
					      phba->sli4_hba.cq_esize,
					      LPFC_CQE_EXP_COUNT, cpu);

	else
		qdesc = lpfc_sli4_queue_alloc(phba, LPFC_DEFAULT_PAGE_SIZE,
					      phba->sli4_hba.cq_esize,
					      phba->sli4_hba.cq_ecount, cpu);
	if (!qdesc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0499 Failed allocate fast-path IO CQ (%d)\n",
				idx);
		return 1;
	}
	qdesc->qe_valid = 1;
	qdesc->hdwq = idx;
	qdesc->chann = cpu;
	phba->sli4_hba.hdwq[idx].io_cq = qdesc;

	 
	if (phba->enab_exp_wqcq_pages) {
		 
		wqesize = (phba->fcp_embed_io) ?
			LPFC_WQE128_SIZE : phba->sli4_hba.wq_esize;
		qdesc = lpfc_sli4_queue_alloc(phba, LPFC_EXPANDED_PAGE_SIZE,
					      wqesize,
					      LPFC_WQE_EXP_COUNT, cpu);
	} else
		qdesc = lpfc_sli4_queue_alloc(phba, LPFC_DEFAULT_PAGE_SIZE,
					      phba->sli4_hba.wq_esize,
					      phba->sli4_hba.wq_ecount, cpu);

	if (!qdesc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0503 Failed allocate fast-path IO WQ (%d)\n",
				idx);
		return 1;
	}
	qdesc->hdwq = idx;
	qdesc->chann = cpu;
	phba->sli4_hba.hdwq[idx].io_wq = qdesc;
	list_add_tail(&qdesc->wq_list, &phba->sli4_hba.lpfc_wq_list);
	return 0;
}

 
int
lpfc_sli4_queue_create(struct lpfc_hba *phba)
{
	struct lpfc_queue *qdesc;
	int idx, cpu, eqcpu;
	struct lpfc_sli4_hdw_queue *qp;
	struct lpfc_vector_map_info *cpup;
	struct lpfc_vector_map_info *eqcpup;
	struct lpfc_eq_intr_info *eqi;

	 
	phba->sli4_hba.mq_esize = LPFC_MQE_SIZE;
	phba->sli4_hba.mq_ecount = LPFC_MQE_DEF_COUNT;
	phba->sli4_hba.wq_esize = LPFC_WQE_SIZE;
	phba->sli4_hba.wq_ecount = LPFC_WQE_DEF_COUNT;
	phba->sli4_hba.rq_esize = LPFC_RQE_SIZE;
	phba->sli4_hba.rq_ecount = LPFC_RQE_DEF_COUNT;
	phba->sli4_hba.eq_esize = LPFC_EQE_SIZE_4B;
	phba->sli4_hba.eq_ecount = LPFC_EQE_DEF_COUNT;
	phba->sli4_hba.cq_esize = LPFC_CQE_SIZE;
	phba->sli4_hba.cq_ecount = LPFC_CQE_DEF_COUNT;

	if (!phba->sli4_hba.hdwq) {
		phba->sli4_hba.hdwq = kcalloc(
			phba->cfg_hdw_queue, sizeof(struct lpfc_sli4_hdw_queue),
			GFP_KERNEL);
		if (!phba->sli4_hba.hdwq) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"6427 Failed allocate memory for "
					"fast-path Hardware Queue array\n");
			goto out_error;
		}
		 
		for (idx = 0; idx < phba->cfg_hdw_queue; idx++) {
			qp = &phba->sli4_hba.hdwq[idx];
			spin_lock_init(&qp->io_buf_list_get_lock);
			spin_lock_init(&qp->io_buf_list_put_lock);
			INIT_LIST_HEAD(&qp->lpfc_io_buf_list_get);
			INIT_LIST_HEAD(&qp->lpfc_io_buf_list_put);
			qp->get_io_bufs = 0;
			qp->put_io_bufs = 0;
			qp->total_io_bufs = 0;
			spin_lock_init(&qp->abts_io_buf_list_lock);
			INIT_LIST_HEAD(&qp->lpfc_abts_io_buf_list);
			qp->abts_scsi_io_bufs = 0;
			qp->abts_nvme_io_bufs = 0;
			INIT_LIST_HEAD(&qp->sgl_list);
			INIT_LIST_HEAD(&qp->cmd_rsp_buf_list);
			spin_lock_init(&qp->hdwq_lock);
		}
	}

	if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME) {
		if (phba->nvmet_support) {
			phba->sli4_hba.nvmet_cqset = kcalloc(
					phba->cfg_nvmet_mrq,
					sizeof(struct lpfc_queue *),
					GFP_KERNEL);
			if (!phba->sli4_hba.nvmet_cqset) {
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"3121 Fail allocate memory for "
					"fast-path CQ set array\n");
				goto out_error;
			}
			phba->sli4_hba.nvmet_mrq_hdr = kcalloc(
					phba->cfg_nvmet_mrq,
					sizeof(struct lpfc_queue *),
					GFP_KERNEL);
			if (!phba->sli4_hba.nvmet_mrq_hdr) {
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"3122 Fail allocate memory for "
					"fast-path RQ set hdr array\n");
				goto out_error;
			}
			phba->sli4_hba.nvmet_mrq_data = kcalloc(
					phba->cfg_nvmet_mrq,
					sizeof(struct lpfc_queue *),
					GFP_KERNEL);
			if (!phba->sli4_hba.nvmet_mrq_data) {
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"3124 Fail allocate memory for "
					"fast-path RQ set data array\n");
				goto out_error;
			}
		}
	}

	INIT_LIST_HEAD(&phba->sli4_hba.lpfc_wq_list);

	 
	for_each_present_cpu(cpu) {
		 
		cpup = &phba->sli4_hba.cpu_map[cpu];
		if (!(cpup->flag & LPFC_CPU_FIRST_IRQ))
			continue;

		 
		qp = &phba->sli4_hba.hdwq[cpup->hdwq];

		 
		qdesc = lpfc_sli4_queue_alloc(phba, LPFC_DEFAULT_PAGE_SIZE,
					      phba->sli4_hba.eq_esize,
					      phba->sli4_hba.eq_ecount, cpu);
		if (!qdesc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0497 Failed allocate EQ (%d)\n",
					cpup->hdwq);
			goto out_error;
		}
		qdesc->qe_valid = 1;
		qdesc->hdwq = cpup->hdwq;
		qdesc->chann = cpu;  
		qdesc->last_cpu = qdesc->chann;

		 
		qp->hba_eq = qdesc;

		eqi = per_cpu_ptr(phba->sli4_hba.eq_info, qdesc->last_cpu);
		list_add(&qdesc->cpu_list, &eqi->list);
	}

	 
	for_each_present_cpu(cpu) {
		cpup = &phba->sli4_hba.cpu_map[cpu];

		 
		if (cpup->flag & LPFC_CPU_FIRST_IRQ)
			continue;

		 
		qp = &phba->sli4_hba.hdwq[cpup->hdwq];
		if (qp->hba_eq)
			continue;

		 
		eqcpu = lpfc_find_cpu_handle(phba, cpup->eq, LPFC_FIND_BY_EQ);
		eqcpup = &phba->sli4_hba.cpu_map[eqcpu];
		qp->hba_eq = phba->sli4_hba.hdwq[eqcpup->hdwq].hba_eq;
	}

	 
	for (idx = 0; idx < phba->cfg_hdw_queue; idx++) {
		if (lpfc_alloc_io_wq_cq(phba, idx))
			goto out_error;
	}

	if (phba->nvmet_support) {
		for (idx = 0; idx < phba->cfg_nvmet_mrq; idx++) {
			cpu = lpfc_find_cpu_handle(phba, idx,
						   LPFC_FIND_BY_HDWQ);
			qdesc = lpfc_sli4_queue_alloc(phba,
						      LPFC_DEFAULT_PAGE_SIZE,
						      phba->sli4_hba.cq_esize,
						      phba->sli4_hba.cq_ecount,
						      cpu);
			if (!qdesc) {
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"3142 Failed allocate NVME "
						"CQ Set (%d)\n", idx);
				goto out_error;
			}
			qdesc->qe_valid = 1;
			qdesc->hdwq = idx;
			qdesc->chann = cpu;
			phba->sli4_hba.nvmet_cqset[idx] = qdesc;
		}
	}

	 

	cpu = lpfc_find_cpu_handle(phba, 0, LPFC_FIND_BY_EQ);
	 
	qdesc = lpfc_sli4_queue_alloc(phba, LPFC_DEFAULT_PAGE_SIZE,
				      phba->sli4_hba.cq_esize,
				      phba->sli4_hba.cq_ecount, cpu);
	if (!qdesc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0500 Failed allocate slow-path mailbox CQ\n");
		goto out_error;
	}
	qdesc->qe_valid = 1;
	phba->sli4_hba.mbx_cq = qdesc;

	 
	qdesc = lpfc_sli4_queue_alloc(phba, LPFC_DEFAULT_PAGE_SIZE,
				      phba->sli4_hba.cq_esize,
				      phba->sli4_hba.cq_ecount, cpu);
	if (!qdesc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0501 Failed allocate slow-path ELS CQ\n");
		goto out_error;
	}
	qdesc->qe_valid = 1;
	qdesc->chann = cpu;
	phba->sli4_hba.els_cq = qdesc;


	 

	 

	qdesc = lpfc_sli4_queue_alloc(phba, LPFC_DEFAULT_PAGE_SIZE,
				      phba->sli4_hba.mq_esize,
				      phba->sli4_hba.mq_ecount, cpu);
	if (!qdesc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0505 Failed allocate slow-path MQ\n");
		goto out_error;
	}
	qdesc->chann = cpu;
	phba->sli4_hba.mbx_wq = qdesc;

	 

	 
	qdesc = lpfc_sli4_queue_alloc(phba, LPFC_DEFAULT_PAGE_SIZE,
				      phba->sli4_hba.wq_esize,
				      phba->sli4_hba.wq_ecount, cpu);
	if (!qdesc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0504 Failed allocate slow-path ELS WQ\n");
		goto out_error;
	}
	qdesc->chann = cpu;
	phba->sli4_hba.els_wq = qdesc;
	list_add_tail(&qdesc->wq_list, &phba->sli4_hba.lpfc_wq_list);

	if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME) {
		 
		qdesc = lpfc_sli4_queue_alloc(phba, LPFC_DEFAULT_PAGE_SIZE,
					      phba->sli4_hba.cq_esize,
					      phba->sli4_hba.cq_ecount, cpu);
		if (!qdesc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"6079 Failed allocate NVME LS CQ\n");
			goto out_error;
		}
		qdesc->chann = cpu;
		qdesc->qe_valid = 1;
		phba->sli4_hba.nvmels_cq = qdesc;

		 
		qdesc = lpfc_sli4_queue_alloc(phba, LPFC_DEFAULT_PAGE_SIZE,
					      phba->sli4_hba.wq_esize,
					      phba->sli4_hba.wq_ecount, cpu);
		if (!qdesc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"6080 Failed allocate NVME LS WQ\n");
			goto out_error;
		}
		qdesc->chann = cpu;
		phba->sli4_hba.nvmels_wq = qdesc;
		list_add_tail(&qdesc->wq_list, &phba->sli4_hba.lpfc_wq_list);
	}

	 

	 
	qdesc = lpfc_sli4_queue_alloc(phba, LPFC_DEFAULT_PAGE_SIZE,
				      phba->sli4_hba.rq_esize,
				      phba->sli4_hba.rq_ecount, cpu);
	if (!qdesc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0506 Failed allocate receive HRQ\n");
		goto out_error;
	}
	phba->sli4_hba.hdr_rq = qdesc;

	 
	qdesc = lpfc_sli4_queue_alloc(phba, LPFC_DEFAULT_PAGE_SIZE,
				      phba->sli4_hba.rq_esize,
				      phba->sli4_hba.rq_ecount, cpu);
	if (!qdesc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0507 Failed allocate receive DRQ\n");
		goto out_error;
	}
	phba->sli4_hba.dat_rq = qdesc;

	if ((phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME) &&
	    phba->nvmet_support) {
		for (idx = 0; idx < phba->cfg_nvmet_mrq; idx++) {
			cpu = lpfc_find_cpu_handle(phba, idx,
						   LPFC_FIND_BY_HDWQ);
			 
			qdesc = lpfc_sli4_queue_alloc(phba,
						      LPFC_DEFAULT_PAGE_SIZE,
						      phba->sli4_hba.rq_esize,
						      LPFC_NVMET_RQE_DEF_COUNT,
						      cpu);
			if (!qdesc) {
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"3146 Failed allocate "
						"receive HRQ\n");
				goto out_error;
			}
			qdesc->hdwq = idx;
			phba->sli4_hba.nvmet_mrq_hdr[idx] = qdesc;

			 
			qdesc->rqbp = kzalloc_node(sizeof(*qdesc->rqbp),
						   GFP_KERNEL,
						   cpu_to_node(cpu));
			if (qdesc->rqbp == NULL) {
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"6131 Failed allocate "
						"Header RQBP\n");
				goto out_error;
			}

			 
			INIT_LIST_HEAD(&qdesc->rqbp->rqb_buffer_list);

			 
			qdesc = lpfc_sli4_queue_alloc(phba,
						      LPFC_DEFAULT_PAGE_SIZE,
						      phba->sli4_hba.rq_esize,
						      LPFC_NVMET_RQE_DEF_COUNT,
						      cpu);
			if (!qdesc) {
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"3156 Failed allocate "
						"receive DRQ\n");
				goto out_error;
			}
			qdesc->hdwq = idx;
			phba->sli4_hba.nvmet_mrq_data[idx] = qdesc;
		}
	}

	 
	if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME) {
		for (idx = 0; idx < phba->cfg_hdw_queue; idx++) {
			memset(&phba->sli4_hba.hdwq[idx].nvme_cstat, 0,
			       sizeof(phba->sli4_hba.hdwq[idx].nvme_cstat));
		}
	}

	 
	if (phba->cfg_enable_fc4_type & LPFC_ENABLE_FCP) {
		for (idx = 0; idx < phba->cfg_hdw_queue; idx++) {
			memset(&phba->sli4_hba.hdwq[idx].scsi_cstat, 0,
			       sizeof(phba->sli4_hba.hdwq[idx].scsi_cstat));
		}
	}

	return 0;

out_error:
	lpfc_sli4_queue_destroy(phba);
	return -ENOMEM;
}

static inline void
__lpfc_sli4_release_queue(struct lpfc_queue **qp)
{
	if (*qp != NULL) {
		lpfc_sli4_queue_free(*qp);
		*qp = NULL;
	}
}

static inline void
lpfc_sli4_release_queues(struct lpfc_queue ***qs, int max)
{
	int idx;

	if (*qs == NULL)
		return;

	for (idx = 0; idx < max; idx++)
		__lpfc_sli4_release_queue(&(*qs)[idx]);

	kfree(*qs);
	*qs = NULL;
}

static inline void
lpfc_sli4_release_hdwq(struct lpfc_hba *phba)
{
	struct lpfc_sli4_hdw_queue *hdwq;
	struct lpfc_queue *eq;
	uint32_t idx;

	hdwq = phba->sli4_hba.hdwq;

	 
	for (idx = 0; idx < phba->cfg_hdw_queue; idx++) {
		 
		lpfc_sli4_queue_free(hdwq[idx].io_cq);
		lpfc_sli4_queue_free(hdwq[idx].io_wq);
		hdwq[idx].hba_eq = NULL;
		hdwq[idx].io_cq = NULL;
		hdwq[idx].io_wq = NULL;
		if (phba->cfg_xpsgl && !phba->nvmet_support)
			lpfc_free_sgl_per_hdwq(phba, &hdwq[idx]);
		lpfc_free_cmd_rsp_buf_per_hdwq(phba, &hdwq[idx]);
	}
	 
	for (idx = 0; idx < phba->cfg_irq_chann; idx++) {
		 
		eq = phba->sli4_hba.hba_eq_hdl[idx].eq;
		lpfc_sli4_queue_free(eq);
		phba->sli4_hba.hba_eq_hdl[idx].eq = NULL;
	}
}

 
void
lpfc_sli4_queue_destroy(struct lpfc_hba *phba)
{
	 
	spin_lock_irq(&phba->hbalock);
	phba->sli.sli_flag |= LPFC_QUEUE_FREE_INIT;
	while (phba->sli.sli_flag & LPFC_QUEUE_FREE_WAIT) {
		spin_unlock_irq(&phba->hbalock);
		msleep(20);
		spin_lock_irq(&phba->hbalock);
	}
	spin_unlock_irq(&phba->hbalock);

	lpfc_sli4_cleanup_poll_list(phba);

	 
	if (phba->sli4_hba.hdwq)
		lpfc_sli4_release_hdwq(phba);

	if (phba->nvmet_support) {
		lpfc_sli4_release_queues(&phba->sli4_hba.nvmet_cqset,
					 phba->cfg_nvmet_mrq);

		lpfc_sli4_release_queues(&phba->sli4_hba.nvmet_mrq_hdr,
					 phba->cfg_nvmet_mrq);
		lpfc_sli4_release_queues(&phba->sli4_hba.nvmet_mrq_data,
					 phba->cfg_nvmet_mrq);
	}

	 
	__lpfc_sli4_release_queue(&phba->sli4_hba.mbx_wq);

	 
	__lpfc_sli4_release_queue(&phba->sli4_hba.els_wq);

	 
	__lpfc_sli4_release_queue(&phba->sli4_hba.nvmels_wq);

	 
	__lpfc_sli4_release_queue(&phba->sli4_hba.hdr_rq);
	__lpfc_sli4_release_queue(&phba->sli4_hba.dat_rq);

	 
	__lpfc_sli4_release_queue(&phba->sli4_hba.els_cq);

	 
	__lpfc_sli4_release_queue(&phba->sli4_hba.nvmels_cq);

	 
	__lpfc_sli4_release_queue(&phba->sli4_hba.mbx_cq);

	 
	INIT_LIST_HEAD(&phba->sli4_hba.lpfc_wq_list);

	 
	spin_lock_irq(&phba->hbalock);
	phba->sli.sli_flag &= ~LPFC_QUEUE_FREE_INIT;
	spin_unlock_irq(&phba->hbalock);
}

int
lpfc_free_rq_buffer(struct lpfc_hba *phba, struct lpfc_queue *rq)
{
	struct lpfc_rqb *rqbp;
	struct lpfc_dmabuf *h_buf;
	struct rqb_dmabuf *rqb_buffer;

	rqbp = rq->rqbp;
	while (!list_empty(&rqbp->rqb_buffer_list)) {
		list_remove_head(&rqbp->rqb_buffer_list, h_buf,
				 struct lpfc_dmabuf, list);

		rqb_buffer = container_of(h_buf, struct rqb_dmabuf, hbuf);
		(rqbp->rqb_free_buffer)(phba, rqb_buffer);
		rqbp->buffer_count--;
	}
	return 1;
}

static int
lpfc_create_wq_cq(struct lpfc_hba *phba, struct lpfc_queue *eq,
	struct lpfc_queue *cq, struct lpfc_queue *wq, uint16_t *cq_map,
	int qidx, uint32_t qtype)
{
	struct lpfc_sli_ring *pring;
	int rc;

	if (!eq || !cq || !wq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"6085 Fast-path %s (%d) not allocated\n",
			((eq) ? ((cq) ? "WQ" : "CQ") : "EQ"), qidx);
		return -ENOMEM;
	}

	 
	rc = lpfc_cq_create(phba, cq, eq,
			(qtype == LPFC_MBOX) ? LPFC_MCQ : LPFC_WCQ, qtype);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"6086 Failed setup of CQ (%d), rc = 0x%x\n",
				qidx, (uint32_t)rc);
		return rc;
	}

	if (qtype != LPFC_MBOX) {
		 
		if (cq_map)
			*cq_map = cq->queue_id;

		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"6087 CQ setup: cq[%d]-id=%d, parent eq[%d]-id=%d\n",
			qidx, cq->queue_id, qidx, eq->queue_id);

		 
		rc = lpfc_wq_create(phba, wq, cq, qtype);
		if (rc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"4618 Fail setup fastpath WQ (%d), rc = 0x%x\n",
				qidx, (uint32_t)rc);
			 
			return rc;
		}

		 
		pring = wq->pring;
		pring->sli.sli4.wqp = (void *)wq;
		cq->pring = pring;

		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"2593 WQ setup: wq[%d]-id=%d assoc=%d, cq[%d]-id=%d\n",
			qidx, wq->queue_id, wq->assoc_qid, qidx, cq->queue_id);
	} else {
		rc = lpfc_mq_create(phba, wq, cq, LPFC_MBOX);
		if (rc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0539 Failed setup of slow-path MQ: "
					"rc = 0x%x\n", rc);
			 
			return rc;
		}

		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"2589 MBX MQ setup: wq-id=%d, parent cq-id=%d\n",
			phba->sli4_hba.mbx_wq->queue_id,
			phba->sli4_hba.mbx_cq->queue_id);
	}

	return 0;
}

 
static void
lpfc_setup_cq_lookup(struct lpfc_hba *phba)
{
	struct lpfc_queue *eq, *childq;
	int qidx;

	memset(phba->sli4_hba.cq_lookup, 0,
	       (sizeof(struct lpfc_queue *) * (phba->sli4_hba.cq_max + 1)));
	 
	for (qidx = 0; qidx < phba->cfg_irq_chann; qidx++) {
		 
		eq = phba->sli4_hba.hba_eq_hdl[qidx].eq;
		if (!eq)
			continue;
		 
		list_for_each_entry(childq, &eq->child_list, list) {
			if (childq->queue_id > phba->sli4_hba.cq_max)
				continue;
			if (childq->subtype == LPFC_IO)
				phba->sli4_hba.cq_lookup[childq->queue_id] =
					childq;
		}
	}
}

 
int
lpfc_sli4_queue_setup(struct lpfc_hba *phba)
{
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	struct lpfc_vector_map_info *cpup;
	struct lpfc_sli4_hdw_queue *qp;
	LPFC_MBOXQ_t *mboxq;
	int qidx, cpu;
	uint32_t length, usdelay;
	int rc = -ENOMEM;

	 
	mboxq = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3249 Unable to allocate memory for "
				"QUERY_FW_CFG mailbox command\n");
		return -ENOMEM;
	}
	length = (sizeof(struct lpfc_mbx_query_fw_config) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_QUERY_FW_CFG,
			 length, LPFC_SLI4_MBX_EMBED);

	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);

	shdr = (union lpfc_sli4_cfg_shdr *)
			&mboxq->u.mqe.un.sli4_config.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3250 QUERY_FW_CFG mailbox failed with status "
				"x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		mempool_free(mboxq, phba->mbox_mem_pool);
		rc = -ENXIO;
		goto out_error;
	}

	phba->sli4_hba.fw_func_mode =
			mboxq->u.mqe.un.query_fw_cfg.rsp.function_mode;
	phba->sli4_hba.ulp0_mode = mboxq->u.mqe.un.query_fw_cfg.rsp.ulp0_mode;
	phba->sli4_hba.ulp1_mode = mboxq->u.mqe.un.query_fw_cfg.rsp.ulp1_mode;
	phba->sli4_hba.physical_port =
			mboxq->u.mqe.un.query_fw_cfg.rsp.physical_port;
	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"3251 QUERY_FW_CFG: func_mode:x%x, ulp0_mode:x%x, "
			"ulp1_mode:x%x\n", phba->sli4_hba.fw_func_mode,
			phba->sli4_hba.ulp0_mode, phba->sli4_hba.ulp1_mode);

	mempool_free(mboxq, phba->mbox_mem_pool);

	 
	qp = phba->sli4_hba.hdwq;

	 
	if (!qp) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3147 Fast-path EQs not allocated\n");
		rc = -ENOMEM;
		goto out_error;
	}

	 
	for (qidx = 0; qidx < phba->cfg_irq_chann; qidx++) {
		 
		for_each_present_cpu(cpu) {
			cpup = &phba->sli4_hba.cpu_map[cpu];

			 
			if (!(cpup->flag & LPFC_CPU_FIRST_IRQ))
				continue;
			if (qidx != cpup->eq)
				continue;

			 
			rc = lpfc_eq_create(phba, qp[cpup->hdwq].hba_eq,
					    phba->cfg_fcp_imax);
			if (rc) {
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"0523 Failed setup of fast-path"
						" EQ (%d), rc = 0x%x\n",
						cpup->eq, (uint32_t)rc);
				goto out_destroy;
			}

			 
			phba->sli4_hba.hba_eq_hdl[cpup->eq].eq =
				qp[cpup->hdwq].hba_eq;

			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"2584 HBA EQ setup: queue[%d]-id=%d\n",
					cpup->eq,
					qp[cpup->hdwq].hba_eq->queue_id);
		}
	}

	 
	for (qidx = 0; qidx < phba->cfg_hdw_queue; qidx++) {
		cpu = lpfc_find_cpu_handle(phba, qidx, LPFC_FIND_BY_HDWQ);
		cpup = &phba->sli4_hba.cpu_map[cpu];

		 
		rc = lpfc_create_wq_cq(phba,
				       phba->sli4_hba.hdwq[cpup->hdwq].hba_eq,
				       qp[qidx].io_cq,
				       qp[qidx].io_wq,
				       &phba->sli4_hba.hdwq[qidx].io_cq_map,
				       qidx,
				       LPFC_IO);
		if (rc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0535 Failed to setup fastpath "
					"IO WQ/CQ (%d), rc = 0x%x\n",
					qidx, (uint32_t)rc);
			goto out_destroy;
		}
	}

	 

	 

	if (!phba->sli4_hba.mbx_cq || !phba->sli4_hba.mbx_wq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0528 %s not allocated\n",
				phba->sli4_hba.mbx_cq ?
				"Mailbox WQ" : "Mailbox CQ");
		rc = -ENOMEM;
		goto out_destroy;
	}

	rc = lpfc_create_wq_cq(phba, qp[0].hba_eq,
			       phba->sli4_hba.mbx_cq,
			       phba->sli4_hba.mbx_wq,
			       NULL, 0, LPFC_MBOX);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"0529 Failed setup of mailbox WQ/CQ: rc = 0x%x\n",
			(uint32_t)rc);
		goto out_destroy;
	}
	if (phba->nvmet_support) {
		if (!phba->sli4_hba.nvmet_cqset) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"3165 Fast-path NVME CQ Set "
					"array not allocated\n");
			rc = -ENOMEM;
			goto out_destroy;
		}
		if (phba->cfg_nvmet_mrq > 1) {
			rc = lpfc_cq_create_set(phba,
					phba->sli4_hba.nvmet_cqset,
					qp,
					LPFC_WCQ, LPFC_NVMET);
			if (rc) {
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"3164 Failed setup of NVME CQ "
						"Set, rc = 0x%x\n",
						(uint32_t)rc);
				goto out_destroy;
			}
		} else {
			 
			rc = lpfc_cq_create(phba, phba->sli4_hba.nvmet_cqset[0],
					    qp[0].hba_eq,
					    LPFC_WCQ, LPFC_NVMET);
			if (rc) {
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"6089 Failed setup NVMET CQ: "
						"rc = 0x%x\n", (uint32_t)rc);
				goto out_destroy;
			}
			phba->sli4_hba.nvmet_cqset[0]->chann = 0;

			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"6090 NVMET CQ setup: cq-id=%d, "
					"parent eq-id=%d\n",
					phba->sli4_hba.nvmet_cqset[0]->queue_id,
					qp[0].hba_eq->queue_id);
		}
	}

	 
	if (!phba->sli4_hba.els_cq || !phba->sli4_hba.els_wq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0530 ELS %s not allocated\n",
				phba->sli4_hba.els_cq ? "WQ" : "CQ");
		rc = -ENOMEM;
		goto out_destroy;
	}
	rc = lpfc_create_wq_cq(phba, qp[0].hba_eq,
			       phba->sli4_hba.els_cq,
			       phba->sli4_hba.els_wq,
			       NULL, 0, LPFC_ELS);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0525 Failed setup of ELS WQ/CQ: rc = 0x%x\n",
				(uint32_t)rc);
		goto out_destroy;
	}
	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"2590 ELS WQ setup: wq-id=%d, parent cq-id=%d\n",
			phba->sli4_hba.els_wq->queue_id,
			phba->sli4_hba.els_cq->queue_id);

	if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME) {
		 
		if (!phba->sli4_hba.nvmels_cq || !phba->sli4_hba.nvmels_wq) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"6091 LS %s not allocated\n",
					phba->sli4_hba.nvmels_cq ? "WQ" : "CQ");
			rc = -ENOMEM;
			goto out_destroy;
		}
		rc = lpfc_create_wq_cq(phba, qp[0].hba_eq,
				       phba->sli4_hba.nvmels_cq,
				       phba->sli4_hba.nvmels_wq,
				       NULL, 0, LPFC_NVME_LS);
		if (rc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0526 Failed setup of NVVME LS WQ/CQ: "
					"rc = 0x%x\n", (uint32_t)rc);
			goto out_destroy;
		}

		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"6096 ELS WQ setup: wq-id=%d, "
				"parent cq-id=%d\n",
				phba->sli4_hba.nvmels_wq->queue_id,
				phba->sli4_hba.nvmels_cq->queue_id);
	}

	 
	if (phba->nvmet_support) {
		if ((!phba->sli4_hba.nvmet_cqset) ||
		    (!phba->sli4_hba.nvmet_mrq_hdr) ||
		    (!phba->sli4_hba.nvmet_mrq_data)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"6130 MRQ CQ Queues not "
					"allocated\n");
			rc = -ENOMEM;
			goto out_destroy;
		}
		if (phba->cfg_nvmet_mrq > 1) {
			rc = lpfc_mrq_create(phba,
					     phba->sli4_hba.nvmet_mrq_hdr,
					     phba->sli4_hba.nvmet_mrq_data,
					     phba->sli4_hba.nvmet_cqset,
					     LPFC_NVMET);
			if (rc) {
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"6098 Failed setup of NVMET "
						"MRQ: rc = 0x%x\n",
						(uint32_t)rc);
				goto out_destroy;
			}

		} else {
			rc = lpfc_rq_create(phba,
					    phba->sli4_hba.nvmet_mrq_hdr[0],
					    phba->sli4_hba.nvmet_mrq_data[0],
					    phba->sli4_hba.nvmet_cqset[0],
					    LPFC_NVMET);
			if (rc) {
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"6057 Failed setup of NVMET "
						"Receive Queue: rc = 0x%x\n",
						(uint32_t)rc);
				goto out_destroy;
			}

			lpfc_printf_log(
				phba, KERN_INFO, LOG_INIT,
				"6099 NVMET RQ setup: hdr-rq-id=%d, "
				"dat-rq-id=%d parent cq-id=%d\n",
				phba->sli4_hba.nvmet_mrq_hdr[0]->queue_id,
				phba->sli4_hba.nvmet_mrq_data[0]->queue_id,
				phba->sli4_hba.nvmet_cqset[0]->queue_id);

		}
	}

	if (!phba->sli4_hba.hdr_rq || !phba->sli4_hba.dat_rq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0540 Receive Queue not allocated\n");
		rc = -ENOMEM;
		goto out_destroy;
	}

	rc = lpfc_rq_create(phba, phba->sli4_hba.hdr_rq, phba->sli4_hba.dat_rq,
			    phba->sli4_hba.els_cq, LPFC_USOL);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0541 Failed setup of Receive Queue: "
				"rc = 0x%x\n", (uint32_t)rc);
		goto out_destroy;
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"2592 USL RQ setup: hdr-rq-id=%d, dat-rq-id=%d "
			"parent cq-id=%d\n",
			phba->sli4_hba.hdr_rq->queue_id,
			phba->sli4_hba.dat_rq->queue_id,
			phba->sli4_hba.els_cq->queue_id);

	if (phba->cfg_fcp_imax)
		usdelay = LPFC_SEC_TO_USEC / phba->cfg_fcp_imax;
	else
		usdelay = 0;

	for (qidx = 0; qidx < phba->cfg_irq_chann;
	     qidx += LPFC_MAX_EQ_DELAY_EQID_CNT)
		lpfc_modify_hba_eq_delay(phba, qidx, LPFC_MAX_EQ_DELAY_EQID_CNT,
					 usdelay);

	if (phba->sli4_hba.cq_max) {
		kfree(phba->sli4_hba.cq_lookup);
		phba->sli4_hba.cq_lookup = kcalloc((phba->sli4_hba.cq_max + 1),
			sizeof(struct lpfc_queue *), GFP_KERNEL);
		if (!phba->sli4_hba.cq_lookup) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0549 Failed setup of CQ Lookup table: "
					"size 0x%x\n", phba->sli4_hba.cq_max);
			rc = -ENOMEM;
			goto out_destroy;
		}
		lpfc_setup_cq_lookup(phba);
	}
	return 0;

out_destroy:
	lpfc_sli4_queue_unset(phba);
out_error:
	return rc;
}

 
void
lpfc_sli4_queue_unset(struct lpfc_hba *phba)
{
	struct lpfc_sli4_hdw_queue *qp;
	struct lpfc_queue *eq;
	int qidx;

	 
	if (phba->sli4_hba.mbx_wq)
		lpfc_mq_destroy(phba, phba->sli4_hba.mbx_wq);

	 
	if (phba->sli4_hba.nvmels_wq)
		lpfc_wq_destroy(phba, phba->sli4_hba.nvmels_wq);

	 
	if (phba->sli4_hba.els_wq)
		lpfc_wq_destroy(phba, phba->sli4_hba.els_wq);

	 
	if (phba->sli4_hba.hdr_rq)
		lpfc_rq_destroy(phba, phba->sli4_hba.hdr_rq,
				phba->sli4_hba.dat_rq);

	 
	if (phba->sli4_hba.mbx_cq)
		lpfc_cq_destroy(phba, phba->sli4_hba.mbx_cq);

	 
	if (phba->sli4_hba.els_cq)
		lpfc_cq_destroy(phba, phba->sli4_hba.els_cq);

	 
	if (phba->sli4_hba.nvmels_cq)
		lpfc_cq_destroy(phba, phba->sli4_hba.nvmels_cq);

	if (phba->nvmet_support) {
		 
		if (phba->sli4_hba.nvmet_mrq_hdr) {
			for (qidx = 0; qidx < phba->cfg_nvmet_mrq; qidx++)
				lpfc_rq_destroy(
					phba,
					phba->sli4_hba.nvmet_mrq_hdr[qidx],
					phba->sli4_hba.nvmet_mrq_data[qidx]);
		}

		 
		if (phba->sli4_hba.nvmet_cqset) {
			for (qidx = 0; qidx < phba->cfg_nvmet_mrq; qidx++)
				lpfc_cq_destroy(
					phba, phba->sli4_hba.nvmet_cqset[qidx]);
		}
	}

	 
	if (phba->sli4_hba.hdwq) {
		 
		for (qidx = 0; qidx < phba->cfg_hdw_queue; qidx++) {
			 
			qp = &phba->sli4_hba.hdwq[qidx];
			lpfc_wq_destroy(phba, qp->io_wq);
			lpfc_cq_destroy(phba, qp->io_cq);
		}
		 
		for (qidx = 0; qidx < phba->cfg_irq_chann; qidx++) {
			 
			eq = phba->sli4_hba.hba_eq_hdl[qidx].eq;
			lpfc_eq_destroy(phba, eq);
		}
	}

	kfree(phba->sli4_hba.cq_lookup);
	phba->sli4_hba.cq_lookup = NULL;
	phba->sli4_hba.cq_max = 0;
}

 
static int
lpfc_sli4_cq_event_pool_create(struct lpfc_hba *phba)
{
	struct lpfc_cq_event *cq_event;
	int i;

	for (i = 0; i < (4 * phba->sli4_hba.cq_ecount); i++) {
		cq_event = kmalloc(sizeof(struct lpfc_cq_event), GFP_KERNEL);
		if (!cq_event)
			goto out_pool_create_fail;
		list_add_tail(&cq_event->list,
			      &phba->sli4_hba.sp_cqe_event_pool);
	}
	return 0;

out_pool_create_fail:
	lpfc_sli4_cq_event_pool_destroy(phba);
	return -ENOMEM;
}

 
static void
lpfc_sli4_cq_event_pool_destroy(struct lpfc_hba *phba)
{
	struct lpfc_cq_event *cq_event, *next_cq_event;

	list_for_each_entry_safe(cq_event, next_cq_event,
				 &phba->sli4_hba.sp_cqe_event_pool, list) {
		list_del(&cq_event->list);
		kfree(cq_event);
	}
}

 
struct lpfc_cq_event *
__lpfc_sli4_cq_event_alloc(struct lpfc_hba *phba)
{
	struct lpfc_cq_event *cq_event = NULL;

	list_remove_head(&phba->sli4_hba.sp_cqe_event_pool, cq_event,
			 struct lpfc_cq_event, list);
	return cq_event;
}

 
struct lpfc_cq_event *
lpfc_sli4_cq_event_alloc(struct lpfc_hba *phba)
{
	struct lpfc_cq_event *cq_event;
	unsigned long iflags;

	spin_lock_irqsave(&phba->hbalock, iflags);
	cq_event = __lpfc_sli4_cq_event_alloc(phba);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return cq_event;
}

 
void
__lpfc_sli4_cq_event_release(struct lpfc_hba *phba,
			     struct lpfc_cq_event *cq_event)
{
	list_add_tail(&cq_event->list, &phba->sli4_hba.sp_cqe_event_pool);
}

 
void
lpfc_sli4_cq_event_release(struct lpfc_hba *phba,
			   struct lpfc_cq_event *cq_event)
{
	unsigned long iflags;
	spin_lock_irqsave(&phba->hbalock, iflags);
	__lpfc_sli4_cq_event_release(phba, cq_event);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
}

 
static void
lpfc_sli4_cq_event_release_all(struct lpfc_hba *phba)
{
	LIST_HEAD(cq_event_list);
	struct lpfc_cq_event *cq_event;
	unsigned long iflags;

	 

	 
	spin_lock_irqsave(&phba->sli4_hba.els_xri_abrt_list_lock, iflags);
	list_splice_init(&phba->sli4_hba.sp_els_xri_aborted_work_queue,
			 &cq_event_list);
	spin_unlock_irqrestore(&phba->sli4_hba.els_xri_abrt_list_lock, iflags);

	 
	spin_lock_irqsave(&phba->sli4_hba.asynce_list_lock, iflags);
	list_splice_init(&phba->sli4_hba.sp_asynce_work_queue,
			 &cq_event_list);
	spin_unlock_irqrestore(&phba->sli4_hba.asynce_list_lock, iflags);

	while (!list_empty(&cq_event_list)) {
		list_remove_head(&cq_event_list, cq_event,
				 struct lpfc_cq_event, list);
		lpfc_sli4_cq_event_release(phba, cq_event);
	}
}

 
int
lpfc_pci_function_reset(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mboxq;
	uint32_t rc = 0, if_type;
	uint32_t shdr_status, shdr_add_status;
	uint32_t rdy_chk;
	uint32_t port_reset = 0;
	union lpfc_sli4_cfg_shdr *shdr;
	struct lpfc_register reg_data;
	uint16_t devid;

	if_type = bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf);
	switch (if_type) {
	case LPFC_SLI_INTF_IF_TYPE_0:
		mboxq = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool,
						       GFP_KERNEL);
		if (!mboxq) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0494 Unable to allocate memory for "
					"issuing SLI_FUNCTION_RESET mailbox "
					"command\n");
			return -ENOMEM;
		}

		 
		lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_COMMON,
				 LPFC_MBOX_OPCODE_FUNCTION_RESET, 0,
				 LPFC_SLI4_MBX_EMBED);
		rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
		shdr = (union lpfc_sli4_cfg_shdr *)
			&mboxq->u.mqe.un.sli4_config.header.cfg_shdr;
		shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
		shdr_add_status = bf_get(lpfc_mbox_hdr_add_status,
					 &shdr->response);
		mempool_free(mboxq, phba->mbox_mem_pool);
		if (shdr_status || shdr_add_status || rc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0495 SLI_FUNCTION_RESET mailbox "
					"failed with status x%x add_status x%x,"
					" mbx status x%x\n",
					shdr_status, shdr_add_status, rc);
			rc = -ENXIO;
		}
		break;
	case LPFC_SLI_INTF_IF_TYPE_2:
	case LPFC_SLI_INTF_IF_TYPE_6:
wait:
		 
		for (rdy_chk = 0; rdy_chk < 1500; rdy_chk++) {
			if (lpfc_readl(phba->sli4_hba.u.if_type2.
				STATUSregaddr, &reg_data.word0)) {
				rc = -ENODEV;
				goto out;
			}
			if (bf_get(lpfc_sliport_status_rdy, &reg_data))
				break;
			msleep(20);
		}

		if (!bf_get(lpfc_sliport_status_rdy, &reg_data)) {
			phba->work_status[0] = readl(
				phba->sli4_hba.u.if_type2.ERR1regaddr);
			phba->work_status[1] = readl(
				phba->sli4_hba.u.if_type2.ERR2regaddr);
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"2890 Port not ready, port status reg "
					"0x%x error 1=0x%x, error 2=0x%x\n",
					reg_data.word0,
					phba->work_status[0],
					phba->work_status[1]);
			rc = -ENODEV;
			goto out;
		}

		if (bf_get(lpfc_sliport_status_pldv, &reg_data))
			lpfc_pldv_detect = true;

		if (!port_reset) {
			 
			reg_data.word0 = 0;
			bf_set(lpfc_sliport_ctrl_end, &reg_data,
			       LPFC_SLIPORT_LITTLE_ENDIAN);
			bf_set(lpfc_sliport_ctrl_ip, &reg_data,
			       LPFC_SLIPORT_INIT_PORT);
			writel(reg_data.word0, phba->sli4_hba.u.if_type2.
			       CTRLregaddr);
			 
			pci_read_config_word(phba->pcidev,
					     PCI_DEVICE_ID, &devid);

			port_reset = 1;
			msleep(20);
			goto wait;
		} else if (bf_get(lpfc_sliport_status_rn, &reg_data)) {
			rc = -ENODEV;
			goto out;
		}
		break;

	case LPFC_SLI_INTF_IF_TYPE_1:
	default:
		break;
	}

out:
	 
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3317 HBA not functional: IP Reset Failed "
				"try: echo fw_reset > board_mode\n");
		rc = -ENODEV;
	}

	return rc;
}

 
static int
lpfc_sli4_pci_mem_setup(struct lpfc_hba *phba)
{
	struct pci_dev *pdev = phba->pcidev;
	unsigned long bar0map_len, bar1map_len, bar2map_len;
	int error;
	uint32_t if_type;

	if (!pdev)
		return -ENODEV;

	 
	error = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (error)
		error = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (error)
		return error;

	 
	if (pci_read_config_dword(pdev, LPFC_SLI_INTF,
				  &phba->sli4_hba.sli_intf.word0)) {
		return -ENODEV;
	}

	 
	if (bf_get(lpfc_sli_intf_valid, &phba->sli4_hba.sli_intf) !=
	    LPFC_SLI_INTF_VALID) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2894 SLI_INTF reg contents invalid "
				"sli_intf reg 0x%x\n",
				phba->sli4_hba.sli_intf.word0);
		return -ENODEV;
	}

	if_type = bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf);
	 
	if (pci_resource_start(pdev, PCI_64BIT_BAR0)) {
		phba->pci_bar0_map = pci_resource_start(pdev, PCI_64BIT_BAR0);
		bar0map_len = pci_resource_len(pdev, PCI_64BIT_BAR0);

		 
		phba->sli4_hba.conf_regs_memmap_p =
			ioremap(phba->pci_bar0_map, bar0map_len);
		if (!phba->sli4_hba.conf_regs_memmap_p) {
			dev_printk(KERN_ERR, &pdev->dev,
				   "ioremap failed for SLI4 PCI config "
				   "registers.\n");
			return -ENODEV;
		}
		phba->pci_bar0_memmap_p = phba->sli4_hba.conf_regs_memmap_p;
		 
		lpfc_sli4_bar0_register_memmap(phba, if_type);
	} else {
		phba->pci_bar0_map = pci_resource_start(pdev, 1);
		bar0map_len = pci_resource_len(pdev, 1);
		if (if_type >= LPFC_SLI_INTF_IF_TYPE_2) {
			dev_printk(KERN_ERR, &pdev->dev,
			   "FATAL - No BAR0 mapping for SLI4, if_type 2\n");
			return -ENODEV;
		}
		phba->sli4_hba.conf_regs_memmap_p =
				ioremap(phba->pci_bar0_map, bar0map_len);
		if (!phba->sli4_hba.conf_regs_memmap_p) {
			dev_printk(KERN_ERR, &pdev->dev,
				"ioremap failed for SLI4 PCI config "
				"registers.\n");
			return -ENODEV;
		}
		lpfc_sli4_bar0_register_memmap(phba, if_type);
	}

	if (if_type == LPFC_SLI_INTF_IF_TYPE_0) {
		if (pci_resource_start(pdev, PCI_64BIT_BAR2)) {
			 
			phba->pci_bar1_map = pci_resource_start(pdev,
								PCI_64BIT_BAR2);
			bar1map_len = pci_resource_len(pdev, PCI_64BIT_BAR2);
			phba->sli4_hba.ctrl_regs_memmap_p =
					ioremap(phba->pci_bar1_map,
						bar1map_len);
			if (!phba->sli4_hba.ctrl_regs_memmap_p) {
				dev_err(&pdev->dev,
					   "ioremap failed for SLI4 HBA "
					    "control registers.\n");
				error = -ENOMEM;
				goto out_iounmap_conf;
			}
			phba->pci_bar2_memmap_p =
					 phba->sli4_hba.ctrl_regs_memmap_p;
			lpfc_sli4_bar1_register_memmap(phba, if_type);
		} else {
			error = -ENOMEM;
			goto out_iounmap_conf;
		}
	}

	if ((if_type == LPFC_SLI_INTF_IF_TYPE_6) &&
	    (pci_resource_start(pdev, PCI_64BIT_BAR2))) {
		 
		phba->pci_bar1_map = pci_resource_start(pdev, PCI_64BIT_BAR2);
		bar1map_len = pci_resource_len(pdev, PCI_64BIT_BAR2);
		phba->sli4_hba.drbl_regs_memmap_p =
				ioremap(phba->pci_bar1_map, bar1map_len);
		if (!phba->sli4_hba.drbl_regs_memmap_p) {
			dev_err(&pdev->dev,
			   "ioremap failed for SLI4 HBA doorbell registers.\n");
			error = -ENOMEM;
			goto out_iounmap_conf;
		}
		phba->pci_bar2_memmap_p = phba->sli4_hba.drbl_regs_memmap_p;
		lpfc_sli4_bar1_register_memmap(phba, if_type);
	}

	if (if_type == LPFC_SLI_INTF_IF_TYPE_0) {
		if (pci_resource_start(pdev, PCI_64BIT_BAR4)) {
			 
			phba->pci_bar2_map = pci_resource_start(pdev,
								PCI_64BIT_BAR4);
			bar2map_len = pci_resource_len(pdev, PCI_64BIT_BAR4);
			phba->sli4_hba.drbl_regs_memmap_p =
					ioremap(phba->pci_bar2_map,
						bar2map_len);
			if (!phba->sli4_hba.drbl_regs_memmap_p) {
				dev_err(&pdev->dev,
					   "ioremap failed for SLI4 HBA"
					   " doorbell registers.\n");
				error = -ENOMEM;
				goto out_iounmap_ctrl;
			}
			phba->pci_bar4_memmap_p =
					phba->sli4_hba.drbl_regs_memmap_p;
			error = lpfc_sli4_bar2_register_memmap(phba, LPFC_VF0);
			if (error)
				goto out_iounmap_all;
		} else {
			error = -ENOMEM;
			goto out_iounmap_ctrl;
		}
	}

	if (if_type == LPFC_SLI_INTF_IF_TYPE_6 &&
	    pci_resource_start(pdev, PCI_64BIT_BAR4)) {
		 
		phba->pci_bar2_map = pci_resource_start(pdev, PCI_64BIT_BAR4);
		bar2map_len = pci_resource_len(pdev, PCI_64BIT_BAR4);
		phba->sli4_hba.dpp_regs_memmap_p =
				ioremap(phba->pci_bar2_map, bar2map_len);
		if (!phba->sli4_hba.dpp_regs_memmap_p) {
			dev_err(&pdev->dev,
			   "ioremap failed for SLI4 HBA dpp registers.\n");
			error = -ENOMEM;
			goto out_iounmap_all;
		}
		phba->pci_bar4_memmap_p = phba->sli4_hba.dpp_regs_memmap_p;
	}

	 
	switch (if_type) {
	case LPFC_SLI_INTF_IF_TYPE_0:
	case LPFC_SLI_INTF_IF_TYPE_2:
		phba->sli4_hba.sli4_eq_clr_intr = lpfc_sli4_eq_clr_intr;
		phba->sli4_hba.sli4_write_eq_db = lpfc_sli4_write_eq_db;
		phba->sli4_hba.sli4_write_cq_db = lpfc_sli4_write_cq_db;
		break;
	case LPFC_SLI_INTF_IF_TYPE_6:
		phba->sli4_hba.sli4_eq_clr_intr = lpfc_sli4_if6_eq_clr_intr;
		phba->sli4_hba.sli4_write_eq_db = lpfc_sli4_if6_write_eq_db;
		phba->sli4_hba.sli4_write_cq_db = lpfc_sli4_if6_write_cq_db;
		break;
	default:
		break;
	}

	return 0;

out_iounmap_all:
	if (phba->sli4_hba.drbl_regs_memmap_p)
		iounmap(phba->sli4_hba.drbl_regs_memmap_p);
out_iounmap_ctrl:
	if (phba->sli4_hba.ctrl_regs_memmap_p)
		iounmap(phba->sli4_hba.ctrl_regs_memmap_p);
out_iounmap_conf:
	iounmap(phba->sli4_hba.conf_regs_memmap_p);

	return error;
}

 
static void
lpfc_sli4_pci_mem_unset(struct lpfc_hba *phba)
{
	uint32_t if_type;
	if_type = bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf);

	switch (if_type) {
	case LPFC_SLI_INTF_IF_TYPE_0:
		iounmap(phba->sli4_hba.drbl_regs_memmap_p);
		iounmap(phba->sli4_hba.ctrl_regs_memmap_p);
		iounmap(phba->sli4_hba.conf_regs_memmap_p);
		break;
	case LPFC_SLI_INTF_IF_TYPE_2:
		iounmap(phba->sli4_hba.conf_regs_memmap_p);
		break;
	case LPFC_SLI_INTF_IF_TYPE_6:
		iounmap(phba->sli4_hba.drbl_regs_memmap_p);
		iounmap(phba->sli4_hba.conf_regs_memmap_p);
		if (phba->sli4_hba.dpp_regs_memmap_p)
			iounmap(phba->sli4_hba.dpp_regs_memmap_p);
		break;
	case LPFC_SLI_INTF_IF_TYPE_1:
		break;
	default:
		dev_printk(KERN_ERR, &phba->pcidev->dev,
			   "FATAL - unsupported SLI4 interface type - %d\n",
			   if_type);
		break;
	}
}

 
static int
lpfc_sli_enable_msix(struct lpfc_hba *phba)
{
	int rc;
	LPFC_MBOXQ_t *pmb;

	 
	rc = pci_alloc_irq_vectors(phba->pcidev,
			LPFC_MSIX_VECTORS, LPFC_MSIX_VECTORS, PCI_IRQ_MSIX);
	if (rc < 0) {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0420 PCI enable MSI-X failed (%d)\n", rc);
		goto vec_fail_out;
	}

	 

	 
	rc = request_irq(pci_irq_vector(phba->pcidev, 0),
			 &lpfc_sli_sp_intr_handler, 0,
			 LPFC_SP_DRIVER_HANDLER_NAME, phba);
	if (rc) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"0421 MSI-X slow-path request_irq failed "
				"(%d)\n", rc);
		goto msi_fail_out;
	}

	 
	rc = request_irq(pci_irq_vector(phba->pcidev, 1),
			 &lpfc_sli_fp_intr_handler, 0,
			 LPFC_FP_DRIVER_HANDLER_NAME, phba);

	if (rc) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"0429 MSI-X fast-path request_irq failed "
				"(%d)\n", rc);
		goto irq_fail_out;
	}

	 
	pmb = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);

	if (!pmb) {
		rc = -ENOMEM;
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0474 Unable to allocate memory for issuing "
				"MBOX_CONFIG_MSI command\n");
		goto mem_fail_out;
	}
	rc = lpfc_config_msi(phba, pmb);
	if (rc)
		goto mbx_fail_out;
	rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);
	if (rc != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_MBOX,
				"0351 Config MSI mailbox command failed, "
				"mbxCmd x%x, mbxStatus x%x\n",
				pmb->u.mb.mbxCommand, pmb->u.mb.mbxStatus);
		goto mbx_fail_out;
	}

	 
	mempool_free(pmb, phba->mbox_mem_pool);
	return rc;

mbx_fail_out:
	 
	mempool_free(pmb, phba->mbox_mem_pool);

mem_fail_out:
	 
	free_irq(pci_irq_vector(phba->pcidev, 1), phba);

irq_fail_out:
	 
	free_irq(pci_irq_vector(phba->pcidev, 0), phba);

msi_fail_out:
	 
	pci_free_irq_vectors(phba->pcidev);

vec_fail_out:
	return rc;
}

 
static int
lpfc_sli_enable_msi(struct lpfc_hba *phba)
{
	int rc;

	rc = pci_enable_msi(phba->pcidev);
	if (!rc)
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0012 PCI enable MSI mode success.\n");
	else {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0471 PCI enable MSI mode failed (%d)\n", rc);
		return rc;
	}

	rc = request_irq(phba->pcidev->irq, lpfc_sli_intr_handler,
			 0, LPFC_DRIVER_NAME, phba);
	if (rc) {
		pci_disable_msi(phba->pcidev);
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"0478 MSI request_irq failed (%d)\n", rc);
	}
	return rc;
}

 
static uint32_t
lpfc_sli_enable_intr(struct lpfc_hba *phba, uint32_t cfg_mode)
{
	uint32_t intr_mode = LPFC_INTR_ERROR;
	int retval;

	 
	retval = lpfc_sli_config_port(phba, LPFC_SLI_REV3);
	if (retval)
		return intr_mode;
	phba->hba_flag &= ~HBA_NEEDS_CFG_PORT;

	if (cfg_mode == 2) {
		 
		retval = lpfc_sli_enable_msix(phba);
		if (!retval) {
			 
			phba->intr_type = MSIX;
			intr_mode = 2;
		}
	}

	 
	if (cfg_mode >= 1 && phba->intr_type == NONE) {
		retval = lpfc_sli_enable_msi(phba);
		if (!retval) {
			 
			phba->intr_type = MSI;
			intr_mode = 1;
		}
	}

	 
	if (phba->intr_type == NONE) {
		retval = request_irq(phba->pcidev->irq, lpfc_sli_intr_handler,
				     IRQF_SHARED, LPFC_DRIVER_NAME, phba);
		if (!retval) {
			 
			phba->intr_type = INTx;
			intr_mode = 0;
		}
	}
	return intr_mode;
}

 
static void
lpfc_sli_disable_intr(struct lpfc_hba *phba)
{
	int nr_irqs, i;

	if (phba->intr_type == MSIX)
		nr_irqs = LPFC_MSIX_VECTORS;
	else
		nr_irqs = 1;

	for (i = 0; i < nr_irqs; i++)
		free_irq(pci_irq_vector(phba->pcidev, i), phba);
	pci_free_irq_vectors(phba->pcidev);

	 
	phba->intr_type = NONE;
	phba->sli.slistat.sli_intr = 0;
}

 
static uint16_t
lpfc_find_cpu_handle(struct lpfc_hba *phba, uint16_t id, int match)
{
	struct lpfc_vector_map_info *cpup;
	int cpu;

	 
	for_each_present_cpu(cpu) {
		cpup = &phba->sli4_hba.cpu_map[cpu];

		 
		if ((match == LPFC_FIND_BY_EQ) &&
		    (cpup->flag & LPFC_CPU_FIRST_IRQ) &&
		    (cpup->eq == id))
			return cpu;

		 
		if ((match == LPFC_FIND_BY_HDWQ) && (cpup->hdwq == id))
			return cpu;
	}
	return 0;
}

#ifdef CONFIG_X86
 
static int
lpfc_find_hyper(struct lpfc_hba *phba, int cpu,
		uint16_t phys_id, uint16_t core_id)
{
	struct lpfc_vector_map_info *cpup;
	int idx;

	for_each_present_cpu(idx) {
		cpup = &phba->sli4_hba.cpu_map[idx];
		 
		if ((cpup->phys_id == phys_id) &&
		    (cpup->core_id == core_id) &&
		    (cpu != idx))
			return 1;
	}
	return 0;
}
#endif

 
static inline void
lpfc_assign_eq_map_info(struct lpfc_hba *phba, uint16_t eqidx, uint16_t flag,
			unsigned int cpu)
{
	struct lpfc_vector_map_info *cpup = &phba->sli4_hba.cpu_map[cpu];
	struct lpfc_hba_eq_hdl *eqhdl = lpfc_get_eq_hdl(eqidx);

	cpup->eq = eqidx;
	cpup->flag |= flag;

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"3336 Set Affinity: CPU %d irq %d eq %d flag x%x\n",
			cpu, eqhdl->irq, cpup->eq, cpup->flag);
}

 
static void
lpfc_cpu_map_array_init(struct lpfc_hba *phba)
{
	struct lpfc_vector_map_info *cpup;
	struct lpfc_eq_intr_info *eqi;
	int cpu;

	for_each_possible_cpu(cpu) {
		cpup = &phba->sli4_hba.cpu_map[cpu];
		cpup->phys_id = LPFC_VECTOR_MAP_EMPTY;
		cpup->core_id = LPFC_VECTOR_MAP_EMPTY;
		cpup->hdwq = LPFC_VECTOR_MAP_EMPTY;
		cpup->eq = LPFC_VECTOR_MAP_EMPTY;
		cpup->flag = 0;
		eqi = per_cpu_ptr(phba->sli4_hba.eq_info, cpu);
		INIT_LIST_HEAD(&eqi->list);
		eqi->icnt = 0;
	}
}

 
static void
lpfc_hba_eq_hdl_array_init(struct lpfc_hba *phba)
{
	struct lpfc_hba_eq_hdl *eqhdl;
	int i;

	for (i = 0; i < phba->cfg_irq_chann; i++) {
		eqhdl = lpfc_get_eq_hdl(i);
		eqhdl->irq = LPFC_IRQ_EMPTY;
		eqhdl->phba = phba;
	}
}

 
static void
lpfc_cpu_affinity_check(struct lpfc_hba *phba, int vectors)
{
	int i, cpu, idx, next_idx, new_cpu, start_cpu, first_cpu;
	int max_phys_id, min_phys_id;
	int max_core_id, min_core_id;
	struct lpfc_vector_map_info *cpup;
	struct lpfc_vector_map_info *new_cpup;
#ifdef CONFIG_X86
	struct cpuinfo_x86 *cpuinfo;
#endif
#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	struct lpfc_hdwq_stat *c_stat;
#endif

	max_phys_id = 0;
	min_phys_id = LPFC_VECTOR_MAP_EMPTY;
	max_core_id = 0;
	min_core_id = LPFC_VECTOR_MAP_EMPTY;

	 
	for_each_present_cpu(cpu) {
		cpup = &phba->sli4_hba.cpu_map[cpu];
#ifdef CONFIG_X86
		cpuinfo = &cpu_data(cpu);
		cpup->phys_id = cpuinfo->phys_proc_id;
		cpup->core_id = cpuinfo->cpu_core_id;
		if (lpfc_find_hyper(phba, cpu, cpup->phys_id, cpup->core_id))
			cpup->flag |= LPFC_CPU_MAP_HYPER;
#else
		 
		cpup->phys_id = 0;
		cpup->core_id = cpu;
#endif

		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"3328 CPU %d physid %d coreid %d flag x%x\n",
				cpu, cpup->phys_id, cpup->core_id, cpup->flag);

		if (cpup->phys_id > max_phys_id)
			max_phys_id = cpup->phys_id;
		if (cpup->phys_id < min_phys_id)
			min_phys_id = cpup->phys_id;

		if (cpup->core_id > max_core_id)
			max_core_id = cpup->core_id;
		if (cpup->core_id < min_core_id)
			min_core_id = cpup->core_id;
	}

	 
	first_cpu = cpumask_first(cpu_present_mask);
	start_cpu = first_cpu;

	for_each_present_cpu(cpu) {
		cpup = &phba->sli4_hba.cpu_map[cpu];

		 
		if (cpup->eq == LPFC_VECTOR_MAP_EMPTY) {
			 
			cpup->flag |= LPFC_CPU_MAP_UNASSIGN;

			 
			new_cpu = start_cpu;
			for (i = 0; i < phba->sli4_hba.num_present_cpu; i++) {
				new_cpup = &phba->sli4_hba.cpu_map[new_cpu];
				if (!(new_cpup->flag & LPFC_CPU_MAP_UNASSIGN) &&
				    (new_cpup->eq != LPFC_VECTOR_MAP_EMPTY) &&
				    (new_cpup->phys_id == cpup->phys_id))
					goto found_same;
				new_cpu = lpfc_next_present_cpu(new_cpu);
			}
			 
			continue;
found_same:
			 
			cpup->eq = new_cpup->eq;

			 
			start_cpu = lpfc_next_present_cpu(new_cpu);

			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"3337 Set Affinity: CPU %d "
					"eq %d from peer cpu %d same "
					"phys_id (%d)\n",
					cpu, cpup->eq, new_cpu,
					cpup->phys_id);
		}
	}

	 
	start_cpu = first_cpu;

	for_each_present_cpu(cpu) {
		cpup = &phba->sli4_hba.cpu_map[cpu];

		 
		if (cpup->eq == LPFC_VECTOR_MAP_EMPTY) {
			 
			cpup->flag |= LPFC_CPU_MAP_UNASSIGN;

			 
			new_cpu = start_cpu;
			for (i = 0; i < phba->sli4_hba.num_present_cpu; i++) {
				new_cpup = &phba->sli4_hba.cpu_map[new_cpu];
				if (!(new_cpup->flag & LPFC_CPU_MAP_UNASSIGN) &&
				    (new_cpup->eq != LPFC_VECTOR_MAP_EMPTY))
					goto found_any;
				new_cpu = lpfc_next_present_cpu(new_cpu);
			}
			 
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"3339 Set Affinity: CPU %d "
					"eq %d UNASSIGNED\n",
					cpup->hdwq, cpup->eq);
			continue;
found_any:
			 
			cpup->eq = new_cpup->eq;

			 
			start_cpu = lpfc_next_present_cpu(new_cpu);

			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"3338 Set Affinity: CPU %d "
					"eq %d from peer cpu %d (%d/%d)\n",
					cpu, cpup->eq, new_cpu,
					new_cpup->phys_id, new_cpup->core_id);
		}
	}

	 
	idx = 0;
	for_each_present_cpu(cpu) {
		cpup = &phba->sli4_hba.cpu_map[cpu];

		 
		if (!(cpup->flag & LPFC_CPU_FIRST_IRQ))
			continue;

		 
		cpup->hdwq = idx;
		idx++;
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"3333 Set Affinity: CPU %d (phys %d core %d): "
				"hdwq %d eq %d flg x%x\n",
				cpu, cpup->phys_id, cpup->core_id,
				cpup->hdwq, cpup->eq, cpup->flag);
	}
	 
	next_idx = idx;
	start_cpu = 0;
	idx = 0;
	for_each_present_cpu(cpu) {
		cpup = &phba->sli4_hba.cpu_map[cpu];

		 
		if (cpup->flag & LPFC_CPU_FIRST_IRQ)
			continue;

		 
		if (next_idx < phba->cfg_hdw_queue) {
			cpup->hdwq = next_idx;
			next_idx++;
			continue;
		}

		 
		new_cpu = start_cpu;
		for (i = 0; i < phba->sli4_hba.num_present_cpu; i++) {
			new_cpup = &phba->sli4_hba.cpu_map[new_cpu];
			if (new_cpup->hdwq != LPFC_VECTOR_MAP_EMPTY &&
			    new_cpup->phys_id == cpup->phys_id &&
			    new_cpup->core_id == cpup->core_id) {
				goto found_hdwq;
			}
			new_cpu = lpfc_next_present_cpu(new_cpu);
		}

		 
		new_cpu = start_cpu;
		for (i = 0; i < phba->sli4_hba.num_present_cpu; i++) {
			new_cpup = &phba->sli4_hba.cpu_map[new_cpu];
			if (new_cpup->hdwq != LPFC_VECTOR_MAP_EMPTY &&
			    new_cpup->phys_id == cpup->phys_id)
				goto found_hdwq;
			new_cpu = lpfc_next_present_cpu(new_cpu);
		}

		 
		cpup->hdwq = idx % phba->cfg_hdw_queue;
		idx++;
		goto logit;
 found_hdwq:
		 
		start_cpu = lpfc_next_present_cpu(new_cpu);
		cpup->hdwq = new_cpup->hdwq;
 logit:
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"3335 Set Affinity: CPU %d (phys %d core %d): "
				"hdwq %d eq %d flg x%x\n",
				cpu, cpup->phys_id, cpup->core_id,
				cpup->hdwq, cpup->eq, cpup->flag);
	}

	 
	idx = 0;
	for_each_possible_cpu(cpu) {
		cpup = &phba->sli4_hba.cpu_map[cpu];
#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
		c_stat = per_cpu_ptr(phba->sli4_hba.c_stat, cpu);
		c_stat->hdwq_no = cpup->hdwq;
#endif
		if (cpup->hdwq != LPFC_VECTOR_MAP_EMPTY)
			continue;

		cpup->hdwq = idx++ % phba->cfg_hdw_queue;
#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
		c_stat->hdwq_no = cpup->hdwq;
#endif
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"3340 Set Affinity: not present "
				"CPU %d hdwq %d\n",
				cpu, cpup->hdwq);
	}

	 
	return;
}

 
static int
lpfc_cpuhp_get_eq(struct lpfc_hba *phba, unsigned int cpu,
		  struct list_head *eqlist)
{
	const struct cpumask *maskp;
	struct lpfc_queue *eq;
	struct cpumask *tmp;
	u16 idx;

	tmp = kzalloc(cpumask_size(), GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	for (idx = 0; idx < phba->cfg_irq_chann; idx++) {
		maskp = pci_irq_get_affinity(phba->pcidev, idx);
		if (!maskp)
			continue;
		 
		if (!cpumask_and(tmp, maskp, cpumask_of(cpu)))
			continue;
		 
		cpumask_and(tmp, maskp, cpu_online_mask);
		if (cpumask_weight(tmp) > 1)
			continue;

		 
		eq = phba->sli4_hba.hba_eq_hdl[idx].eq;
		list_add(&eq->_poll_list, eqlist);
	}
	kfree(tmp);
	return 0;
}

static void __lpfc_cpuhp_remove(struct lpfc_hba *phba)
{
	if (phba->sli_rev != LPFC_SLI_REV4)
		return;

	cpuhp_state_remove_instance_nocalls(lpfc_cpuhp_state,
					    &phba->cpuhp);
	 
	synchronize_rcu();
	del_timer_sync(&phba->cpuhp_poll_timer);
}

static void lpfc_cpuhp_remove(struct lpfc_hba *phba)
{
	if (phba->pport && (phba->pport->fc_flag & FC_OFFLINE_MODE))
		return;

	__lpfc_cpuhp_remove(phba);
}

static void lpfc_cpuhp_add(struct lpfc_hba *phba)
{
	if (phba->sli_rev != LPFC_SLI_REV4)
		return;

	rcu_read_lock();

	if (!list_empty(&phba->poll_list))
		mod_timer(&phba->cpuhp_poll_timer,
			  jiffies + msecs_to_jiffies(LPFC_POLL_HB));

	rcu_read_unlock();

	cpuhp_state_add_instance_nocalls(lpfc_cpuhp_state,
					 &phba->cpuhp);
}

static int __lpfc_cpuhp_checks(struct lpfc_hba *phba, int *retval)
{
	if (phba->pport->load_flag & FC_UNLOADING) {
		*retval = -EAGAIN;
		return true;
	}

	if (phba->sli_rev != LPFC_SLI_REV4) {
		*retval = 0;
		return true;
	}

	 
	return false;
}

 
static inline void
lpfc_irq_set_aff(struct lpfc_hba_eq_hdl *eqhdl, unsigned int cpu)
{
	cpumask_clear(&eqhdl->aff_mask);
	cpumask_set_cpu(cpu, &eqhdl->aff_mask);
	irq_set_status_flags(eqhdl->irq, IRQ_NO_BALANCING);
	irq_set_affinity(eqhdl->irq, &eqhdl->aff_mask);
}

 
static inline void
lpfc_irq_clear_aff(struct lpfc_hba_eq_hdl *eqhdl)
{
	cpumask_clear(&eqhdl->aff_mask);
	irq_clear_status_flags(eqhdl->irq, IRQ_NO_BALANCING);
}

 
static void
lpfc_irq_rebalance(struct lpfc_hba *phba, unsigned int cpu, bool offline)
{
	struct lpfc_vector_map_info *cpup;
	struct cpumask *aff_mask;
	unsigned int cpu_select, cpu_next, idx;
	const struct cpumask *orig_mask;

	if (phba->irq_chann_mode == NORMAL_MODE)
		return;

	orig_mask = &phba->sli4_hba.irq_aff_mask;

	if (!cpumask_test_cpu(cpu, orig_mask))
		return;

	cpup = &phba->sli4_hba.cpu_map[cpu];

	if (!(cpup->flag & LPFC_CPU_FIRST_IRQ))
		return;

	if (offline) {
		 
		cpu_next = cpumask_next_wrap(cpu, orig_mask, cpu, true);
		cpu_select = lpfc_next_online_cpu(orig_mask, cpu_next);

		 
		if ((cpu_select < nr_cpu_ids) && (cpu_select != cpu)) {
			 
			for (idx = 0; idx < phba->cfg_irq_chann; idx++) {
				aff_mask = lpfc_get_aff_mask(idx);

				 
				if (cpumask_test_cpu(cpu, aff_mask))
					lpfc_irq_set_aff(lpfc_get_eq_hdl(idx),
							 cpu_select);
			}
		} else {
			 
			for (idx = 0; idx < phba->cfg_irq_chann; idx++)
				lpfc_irq_clear_aff(lpfc_get_eq_hdl(idx));
		}
	} else {
		 
		lpfc_irq_set_aff(lpfc_get_eq_hdl(cpup->eq), cpu);
	}
}

static int lpfc_cpu_offline(unsigned int cpu, struct hlist_node *node)
{
	struct lpfc_hba *phba = hlist_entry_safe(node, struct lpfc_hba, cpuhp);
	struct lpfc_queue *eq, *next;
	LIST_HEAD(eqlist);
	int retval;

	if (!phba) {
		WARN_ONCE(!phba, "cpu: %u. phba:NULL", raw_smp_processor_id());
		return 0;
	}

	if (__lpfc_cpuhp_checks(phba, &retval))
		return retval;

	lpfc_irq_rebalance(phba, cpu, true);

	retval = lpfc_cpuhp_get_eq(phba, cpu, &eqlist);
	if (retval)
		return retval;

	 
	list_for_each_entry_safe(eq, next, &eqlist, _poll_list) {
		list_del_init(&eq->_poll_list);
		lpfc_sli4_start_polling(eq);
	}

	return 0;
}

static int lpfc_cpu_online(unsigned int cpu, struct hlist_node *node)
{
	struct lpfc_hba *phba = hlist_entry_safe(node, struct lpfc_hba, cpuhp);
	struct lpfc_queue *eq, *next;
	unsigned int n;
	int retval;

	if (!phba) {
		WARN_ONCE(!phba, "cpu: %u. phba:NULL", raw_smp_processor_id());
		return 0;
	}

	if (__lpfc_cpuhp_checks(phba, &retval))
		return retval;

	lpfc_irq_rebalance(phba, cpu, false);

	list_for_each_entry_safe(eq, next, &phba->poll_list, _poll_list) {
		n = lpfc_find_cpu_handle(phba, eq->hdwq, LPFC_FIND_BY_HDWQ);
		if (n == cpu)
			lpfc_sli4_stop_polling(eq);
	}

	return 0;
}

 
static int
lpfc_sli4_enable_msix(struct lpfc_hba *phba)
{
	int vectors, rc, index;
	char *name;
	const struct cpumask *aff_mask = NULL;
	unsigned int cpu = 0, cpu_cnt = 0, cpu_select = nr_cpu_ids;
	struct lpfc_vector_map_info *cpup;
	struct lpfc_hba_eq_hdl *eqhdl;
	const struct cpumask *maskp;
	unsigned int flags = PCI_IRQ_MSIX;

	 
	vectors = phba->cfg_irq_chann;

	if (phba->irq_chann_mode != NORMAL_MODE)
		aff_mask = &phba->sli4_hba.irq_aff_mask;

	if (aff_mask) {
		cpu_cnt = cpumask_weight(aff_mask);
		vectors = min(phba->cfg_irq_chann, cpu_cnt);

		 
		cpu = cpumask_first(aff_mask);
		cpu_select = lpfc_next_online_cpu(aff_mask, cpu);
	} else {
		flags |= PCI_IRQ_AFFINITY;
	}

	rc = pci_alloc_irq_vectors(phba->pcidev, 1, vectors, flags);
	if (rc < 0) {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0484 PCI enable MSI-X failed (%d)\n", rc);
		goto vec_fail_out;
	}
	vectors = rc;

	 
	for (index = 0; index < vectors; index++) {
		eqhdl = lpfc_get_eq_hdl(index);
		name = eqhdl->handler_name;
		memset(name, 0, LPFC_SLI4_HANDLER_NAME_SZ);
		snprintf(name, LPFC_SLI4_HANDLER_NAME_SZ,
			 LPFC_DRIVER_HANDLER_NAME"%d", index);

		eqhdl->idx = index;
		rc = pci_irq_vector(phba->pcidev, index);
		if (rc < 0) {
			lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
					"0489 MSI-X fast-path (%d) "
					"pci_irq_vec failed (%d)\n", index, rc);
			goto cfg_fail_out;
		}
		eqhdl->irq = rc;

		rc = request_threaded_irq(eqhdl->irq,
					  &lpfc_sli4_hba_intr_handler,
					  &lpfc_sli4_hba_intr_handler_th,
					  IRQF_ONESHOT, name, eqhdl);
		if (rc) {
			lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
					"0486 MSI-X fast-path (%d) "
					"request_irq failed (%d)\n", index, rc);
			goto cfg_fail_out;
		}

		if (aff_mask) {
			 
			if (cpu_select < nr_cpu_ids)
				lpfc_irq_set_aff(eqhdl, cpu_select);

			 
			lpfc_assign_eq_map_info(phba, index,
						LPFC_CPU_FIRST_IRQ,
						cpu);

			 
			cpu = cpumask_next(cpu, aff_mask);

			 
			cpu_select = lpfc_next_online_cpu(aff_mask, cpu);
		} else if (vectors == 1) {
			cpu = cpumask_first(cpu_present_mask);
			lpfc_assign_eq_map_info(phba, index, LPFC_CPU_FIRST_IRQ,
						cpu);
		} else {
			maskp = pci_irq_get_affinity(phba->pcidev, index);

			 
			for_each_cpu_and(cpu, maskp, cpu_present_mask) {
				cpup = &phba->sli4_hba.cpu_map[cpu];

				 
				if (cpup->eq != LPFC_VECTOR_MAP_EMPTY)
					continue;
				lpfc_assign_eq_map_info(phba, index,
							LPFC_CPU_FIRST_IRQ,
							cpu);
				break;
			}
		}
	}

	if (vectors != phba->cfg_irq_chann) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3238 Reducing IO channels to match number of "
				"MSI-X vectors, requested %d got %d\n",
				phba->cfg_irq_chann, vectors);
		if (phba->cfg_irq_chann > vectors)
			phba->cfg_irq_chann = vectors;
	}

	return rc;

cfg_fail_out:
	 
	for (--index; index >= 0; index--) {
		eqhdl = lpfc_get_eq_hdl(index);
		lpfc_irq_clear_aff(eqhdl);
		free_irq(eqhdl->irq, eqhdl);
	}

	 
	pci_free_irq_vectors(phba->pcidev);

vec_fail_out:
	return rc;
}

 
static int
lpfc_sli4_enable_msi(struct lpfc_hba *phba)
{
	int rc, index;
	unsigned int cpu;
	struct lpfc_hba_eq_hdl *eqhdl;

	rc = pci_alloc_irq_vectors(phba->pcidev, 1, 1,
				   PCI_IRQ_MSI | PCI_IRQ_AFFINITY);
	if (rc > 0)
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0487 PCI enable MSI mode success.\n");
	else {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0488 PCI enable MSI mode failed (%d)\n", rc);
		return rc ? rc : -1;
	}

	rc = request_irq(phba->pcidev->irq, lpfc_sli4_intr_handler,
			 0, LPFC_DRIVER_NAME, phba);
	if (rc) {
		pci_free_irq_vectors(phba->pcidev);
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"0490 MSI request_irq failed (%d)\n", rc);
		return rc;
	}

	eqhdl = lpfc_get_eq_hdl(0);
	rc = pci_irq_vector(phba->pcidev, 0);
	if (rc < 0) {
		pci_free_irq_vectors(phba->pcidev);
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"0496 MSI pci_irq_vec failed (%d)\n", rc);
		return rc;
	}
	eqhdl->irq = rc;

	cpu = cpumask_first(cpu_present_mask);
	lpfc_assign_eq_map_info(phba, 0, LPFC_CPU_FIRST_IRQ, cpu);

	for (index = 0; index < phba->cfg_irq_chann; index++) {
		eqhdl = lpfc_get_eq_hdl(index);
		eqhdl->idx = index;
	}

	return 0;
}

 
static uint32_t
lpfc_sli4_enable_intr(struct lpfc_hba *phba, uint32_t cfg_mode)
{
	uint32_t intr_mode = LPFC_INTR_ERROR;
	int retval, idx;

	if (cfg_mode == 2) {
		 
		retval = 0;
		if (!retval) {
			 
			retval = lpfc_sli4_enable_msix(phba);
			if (!retval) {
				 
				phba->intr_type = MSIX;
				intr_mode = 2;
			}
		}
	}

	 
	if (cfg_mode >= 1 && phba->intr_type == NONE) {
		retval = lpfc_sli4_enable_msi(phba);
		if (!retval) {
			 
			phba->intr_type = MSI;
			intr_mode = 1;
		}
	}

	 
	if (phba->intr_type == NONE) {
		retval = request_irq(phba->pcidev->irq, lpfc_sli4_intr_handler,
				     IRQF_SHARED, LPFC_DRIVER_NAME, phba);
		if (!retval) {
			struct lpfc_hba_eq_hdl *eqhdl;
			unsigned int cpu;

			 
			phba->intr_type = INTx;
			intr_mode = 0;

			eqhdl = lpfc_get_eq_hdl(0);
			retval = pci_irq_vector(phba->pcidev, 0);
			if (retval < 0) {
				lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
					"0502 INTR pci_irq_vec failed (%d)\n",
					 retval);
				return LPFC_INTR_ERROR;
			}
			eqhdl->irq = retval;

			cpu = cpumask_first(cpu_present_mask);
			lpfc_assign_eq_map_info(phba, 0, LPFC_CPU_FIRST_IRQ,
						cpu);
			for (idx = 0; idx < phba->cfg_irq_chann; idx++) {
				eqhdl = lpfc_get_eq_hdl(idx);
				eqhdl->idx = idx;
			}
		}
	}
	return intr_mode;
}

 
static void
lpfc_sli4_disable_intr(struct lpfc_hba *phba)
{
	 
	if (phba->intr_type == MSIX) {
		int index;
		struct lpfc_hba_eq_hdl *eqhdl;

		 
		for (index = 0; index < phba->cfg_irq_chann; index++) {
			eqhdl = lpfc_get_eq_hdl(index);
			lpfc_irq_clear_aff(eqhdl);
			free_irq(eqhdl->irq, eqhdl);
		}
	} else {
		free_irq(phba->pcidev->irq, phba);
	}

	pci_free_irq_vectors(phba->pcidev);

	 
	phba->intr_type = NONE;
	phba->sli.slistat.sli_intr = 0;
}

 
static void
lpfc_unset_hba(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);

	spin_lock_irq(shost->host_lock);
	vport->load_flag |= FC_UNLOADING;
	spin_unlock_irq(shost->host_lock);

	kfree(phba->vpi_bmask);
	kfree(phba->vpi_ids);

	lpfc_stop_hba_timers(phba);

	phba->pport->work_port_events = 0;

	lpfc_sli_hba_down(phba);

	lpfc_sli_brdrestart(phba);

	lpfc_sli_disable_intr(phba);

	return;
}

 
static void
lpfc_sli4_xri_exchange_busy_wait(struct lpfc_hba *phba)
{
	struct lpfc_sli4_hdw_queue *qp;
	int idx, ccnt;
	int wait_time = 0;
	int io_xri_cmpl = 1;
	int nvmet_xri_cmpl = 1;
	int els_xri_cmpl = list_empty(&phba->sli4_hba.lpfc_abts_els_sgl_list);

	 
	msleep(LPFC_XRI_EXCH_BUSY_WAIT_T1 * 5);

	 
	if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME)
		lpfc_nvme_wait_for_io_drain(phba);

	ccnt = 0;
	for (idx = 0; idx < phba->cfg_hdw_queue; idx++) {
		qp = &phba->sli4_hba.hdwq[idx];
		io_xri_cmpl = list_empty(&qp->lpfc_abts_io_buf_list);
		if (!io_xri_cmpl)  
			ccnt++;
	}
	if (ccnt)
		io_xri_cmpl = 0;

	if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME) {
		nvmet_xri_cmpl =
			list_empty(&phba->sli4_hba.lpfc_abts_nvmet_ctx_list);
	}

	while (!els_xri_cmpl || !io_xri_cmpl || !nvmet_xri_cmpl) {
		if (wait_time > LPFC_XRI_EXCH_BUSY_WAIT_TMO) {
			if (!nvmet_xri_cmpl)
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"6424 NVMET XRI exchange busy "
						"wait time: %d seconds.\n",
						wait_time/1000);
			if (!io_xri_cmpl)
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"6100 IO XRI exchange busy "
						"wait time: %d seconds.\n",
						wait_time/1000);
			if (!els_xri_cmpl)
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"2878 ELS XRI exchange busy "
						"wait time: %d seconds.\n",
						wait_time/1000);
			msleep(LPFC_XRI_EXCH_BUSY_WAIT_T2);
			wait_time += LPFC_XRI_EXCH_BUSY_WAIT_T2;
		} else {
			msleep(LPFC_XRI_EXCH_BUSY_WAIT_T1);
			wait_time += LPFC_XRI_EXCH_BUSY_WAIT_T1;
		}

		ccnt = 0;
		for (idx = 0; idx < phba->cfg_hdw_queue; idx++) {
			qp = &phba->sli4_hba.hdwq[idx];
			io_xri_cmpl = list_empty(
			    &qp->lpfc_abts_io_buf_list);
			if (!io_xri_cmpl)  
				ccnt++;
		}
		if (ccnt)
			io_xri_cmpl = 0;

		if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME) {
			nvmet_xri_cmpl = list_empty(
				&phba->sli4_hba.lpfc_abts_nvmet_ctx_list);
		}
		els_xri_cmpl =
			list_empty(&phba->sli4_hba.lpfc_abts_els_sgl_list);

	}
}

 
static void
lpfc_sli4_hba_unset(struct lpfc_hba *phba)
{
	int wait_cnt = 0;
	LPFC_MBOXQ_t *mboxq;
	struct pci_dev *pdev = phba->pcidev;

	lpfc_stop_hba_timers(phba);
	hrtimer_cancel(&phba->cmf_stats_timer);
	hrtimer_cancel(&phba->cmf_timer);

	if (phba->pport)
		phba->sli4_hba.intr_enable = 0;

	 

	 
	spin_lock_irq(&phba->hbalock);
	phba->sli.sli_flag |= LPFC_SLI_ASYNC_MBX_BLK;
	spin_unlock_irq(&phba->hbalock);
	 
	while (phba->sli.sli_flag & LPFC_SLI_MBOX_ACTIVE) {
		msleep(10);
		if (++wait_cnt > LPFC_ACTIVE_MBOX_WAIT_CNT)
			break;
	}
	 
	if (phba->sli.sli_flag & LPFC_SLI_MBOX_ACTIVE) {
		spin_lock_irq(&phba->hbalock);
		mboxq = phba->sli.mbox_active;
		mboxq->u.mb.mbxStatus = MBX_NOT_FINISHED;
		__lpfc_mbox_cmpl_put(phba, mboxq);
		phba->sli.sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
		phba->sli.mbox_active = NULL;
		spin_unlock_irq(&phba->hbalock);
	}

	 
	lpfc_sli_hba_iocb_abort(phba);

	if (!pci_channel_offline(phba->pcidev))
		 
		lpfc_sli4_xri_exchange_busy_wait(phba);

	 
	if (phba->pport)
		lpfc_cpuhp_remove(phba);

	 
	lpfc_sli4_disable_intr(phba);

	 
	if (phba->cfg_sriov_nr_virtfn)
		pci_disable_sriov(pdev);

	 
	kthread_stop(phba->worker_thread);

	 
	lpfc_ras_stop_fwlog(phba);

	 
	lpfc_pci_function_reset(phba);

	 
	lpfc_sli4_queue_destroy(phba);

	 
	if (phba->ras_fwlog.ras_enabled)
		lpfc_sli4_ras_dma_free(phba);

	 
	if (phba->pport)
		phba->pport->work_port_events = 0;
}

static uint32_t
lpfc_cgn_crc32(uint32_t crc, u8 byte)
{
	uint32_t msb = 0;
	uint32_t bit;

	for (bit = 0; bit < 8; bit++) {
		msb = (crc >> 31) & 1;
		crc <<= 1;

		if (msb ^ (byte & 1)) {
			crc ^= LPFC_CGN_CRC32_MAGIC_NUMBER;
			crc |= 1;
		}
		byte >>= 1;
	}
	return crc;
}

static uint32_t
lpfc_cgn_reverse_bits(uint32_t wd)
{
	uint32_t result = 0;
	uint32_t i;

	for (i = 0; i < 32; i++) {
		result <<= 1;
		result |= (1 & (wd >> i));
	}
	return result;
}

 
uint32_t
lpfc_cgn_calc_crc32(void *ptr, uint32_t byteLen, uint32_t crc)
{
	uint32_t  i;
	uint32_t result;
	uint8_t  *data = (uint8_t *)ptr;

	for (i = 0; i < byteLen; ++i)
		crc = lpfc_cgn_crc32(crc, data[i]);

	result = ~lpfc_cgn_reverse_bits(crc);
	return result;
}

void
lpfc_init_congestion_buf(struct lpfc_hba *phba)
{
	struct lpfc_cgn_info *cp;
	uint16_t size;
	uint32_t crc;

	lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
			"6235 INIT Congestion Buffer %p\n", phba->cgn_i);

	if (!phba->cgn_i)
		return;
	cp = (struct lpfc_cgn_info *)phba->cgn_i->virt;

	atomic_set(&phba->cgn_fabric_warn_cnt, 0);
	atomic_set(&phba->cgn_fabric_alarm_cnt, 0);
	atomic_set(&phba->cgn_sync_alarm_cnt, 0);
	atomic_set(&phba->cgn_sync_warn_cnt, 0);

	atomic_set(&phba->cgn_driver_evt_cnt, 0);
	atomic_set(&phba->cgn_latency_evt_cnt, 0);
	atomic64_set(&phba->cgn_latency_evt, 0);
	phba->cgn_evt_minute = 0;

	memset(cp, 0xff, offsetof(struct lpfc_cgn_info, cgn_stat));
	cp->cgn_info_size = cpu_to_le16(LPFC_CGN_INFO_SZ);
	cp->cgn_info_version = LPFC_CGN_INFO_V4;

	 
	cp->cgn_info_mode = phba->cgn_p.cgn_param_mode;
	cp->cgn_info_level0 = phba->cgn_p.cgn_param_level0;
	cp->cgn_info_level1 = phba->cgn_p.cgn_param_level1;
	cp->cgn_info_level2 = phba->cgn_p.cgn_param_level2;

	lpfc_cgn_update_tstamp(phba, &cp->base_time);

	 
	if (phba->pport) {
		size = (uint16_t)(phba->pport->cfg_lun_queue_depth);
		cp->cgn_lunq = cpu_to_le16(size);
	}

	 

	cp->cgn_warn_freq = cpu_to_le16(LPFC_FPIN_INIT_FREQ);
	cp->cgn_alarm_freq = cpu_to_le16(LPFC_FPIN_INIT_FREQ);
	crc = lpfc_cgn_calc_crc32(cp, LPFC_CGN_INFO_SZ, LPFC_CGN_CRC32_SEED);
	cp->cgn_info_crc = cpu_to_le32(crc);

	phba->cgn_evt_timestamp = jiffies +
		msecs_to_jiffies(LPFC_CGN_TIMER_TO_MIN);
}

void
lpfc_init_congestion_stat(struct lpfc_hba *phba)
{
	struct lpfc_cgn_info *cp;
	uint32_t crc;

	lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
			"6236 INIT Congestion Stat %p\n", phba->cgn_i);

	if (!phba->cgn_i)
		return;

	cp = (struct lpfc_cgn_info *)phba->cgn_i->virt;
	memset(&cp->cgn_stat, 0, sizeof(cp->cgn_stat));

	lpfc_cgn_update_tstamp(phba, &cp->stat_start);
	crc = lpfc_cgn_calc_crc32(cp, LPFC_CGN_INFO_SZ, LPFC_CGN_CRC32_SEED);
	cp->cgn_info_crc = cpu_to_le32(crc);
}

 
static int
__lpfc_reg_congestion_buf(struct lpfc_hba *phba, int reg)
{
	struct lpfc_mbx_reg_congestion_buf *reg_congestion_buf;
	union  lpfc_sli4_cfg_shdr *shdr;
	uint32_t shdr_status, shdr_add_status;
	LPFC_MBOXQ_t *mboxq;
	int length, rc;

	if (!phba->cgn_i)
		return -ENXIO;

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX,
				"2641 REG_CONGESTION_BUF mbox allocation fail: "
				"HBA state x%x reg %d\n",
				phba->pport->port_state, reg);
		return -ENOMEM;
	}

	length = (sizeof(struct lpfc_mbx_reg_congestion_buf) -
		sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_REG_CONGESTION_BUF, length,
			 LPFC_SLI4_MBX_EMBED);
	reg_congestion_buf = &mboxq->u.mqe.un.reg_congestion_buf;
	bf_set(lpfc_mbx_reg_cgn_buf_type, reg_congestion_buf, 1);
	if (reg > 0)
		bf_set(lpfc_mbx_reg_cgn_buf_cnt, reg_congestion_buf, 1);
	else
		bf_set(lpfc_mbx_reg_cgn_buf_cnt, reg_congestion_buf, 0);
	reg_congestion_buf->length = sizeof(struct lpfc_cgn_info);
	reg_congestion_buf->addr_lo =
		putPaddrLow(phba->cgn_i->phys);
	reg_congestion_buf->addr_hi =
		putPaddrHigh(phba->cgn_i->phys);

	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	shdr = (union lpfc_sli4_cfg_shdr *)
		&mboxq->u.mqe.un.sli4_config.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status,
				 &shdr->response);
	mempool_free(mboxq, phba->mbox_mem_pool);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2642 REG_CONGESTION_BUF mailbox "
				"failed with status x%x add_status x%x,"
				" mbx status x%x reg %d\n",
				shdr_status, shdr_add_status, rc, reg);
		return -ENXIO;
	}
	return 0;
}

int
lpfc_unreg_congestion_buf(struct lpfc_hba *phba)
{
	lpfc_cmf_stop(phba);
	return __lpfc_reg_congestion_buf(phba, 0);
}

int
lpfc_reg_congestion_buf(struct lpfc_hba *phba)
{
	return __lpfc_reg_congestion_buf(phba, 1);
}

 
int
lpfc_get_sli4_parameters(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	int rc;
	struct lpfc_mqe *mqe = &mboxq->u.mqe;
	struct lpfc_pc_sli4_params *sli4_params;
	uint32_t mbox_tmo;
	int length;
	bool exp_wqcq_pages = true;
	struct lpfc_sli4_parameters *mbx_sli4_parameters;

	 
	phba->sli4_hba.rpi_hdrs_in_use = 1;

	 
	length = (sizeof(struct lpfc_mbx_get_sli4_parameters) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_GET_SLI4_PARAMETERS,
			 length, LPFC_SLI4_MBX_EMBED);
	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mboxq);
		rc = lpfc_sli_issue_mbox_wait(phba, mboxq, mbox_tmo);
	}
	if (unlikely(rc))
		return rc;
	sli4_params = &phba->sli4_hba.pc_sli4_params;
	mbx_sli4_parameters = &mqe->un.get_sli4_parameters.sli4_parameters;
	sli4_params->if_type = bf_get(cfg_if_type, mbx_sli4_parameters);
	sli4_params->sli_rev = bf_get(cfg_sli_rev, mbx_sli4_parameters);
	sli4_params->sli_family = bf_get(cfg_sli_family, mbx_sli4_parameters);
	sli4_params->featurelevel_1 = bf_get(cfg_sli_hint_1,
					     mbx_sli4_parameters);
	sli4_params->featurelevel_2 = bf_get(cfg_sli_hint_2,
					     mbx_sli4_parameters);
	if (bf_get(cfg_phwq, mbx_sli4_parameters))
		phba->sli3_options |= LPFC_SLI4_PHWQ_ENABLED;
	else
		phba->sli3_options &= ~LPFC_SLI4_PHWQ_ENABLED;
	sli4_params->sge_supp_len = mbx_sli4_parameters->sge_supp_len;
	sli4_params->loopbk_scope = bf_get(cfg_loopbk_scope,
					   mbx_sli4_parameters);
	sli4_params->oas_supported = bf_get(cfg_oas, mbx_sli4_parameters);
	sli4_params->cqv = bf_get(cfg_cqv, mbx_sli4_parameters);
	sli4_params->mqv = bf_get(cfg_mqv, mbx_sli4_parameters);
	sli4_params->wqv = bf_get(cfg_wqv, mbx_sli4_parameters);
	sli4_params->rqv = bf_get(cfg_rqv, mbx_sli4_parameters);
	sli4_params->eqav = bf_get(cfg_eqav, mbx_sli4_parameters);
	sli4_params->cqav = bf_get(cfg_cqav, mbx_sli4_parameters);
	sli4_params->wqsize = bf_get(cfg_wqsize, mbx_sli4_parameters);
	sli4_params->bv1s = bf_get(cfg_bv1s, mbx_sli4_parameters);
	sli4_params->pls = bf_get(cfg_pvl, mbx_sli4_parameters);
	sli4_params->sgl_pages_max = bf_get(cfg_sgl_page_cnt,
					    mbx_sli4_parameters);
	sli4_params->wqpcnt = bf_get(cfg_wqpcnt, mbx_sli4_parameters);
	sli4_params->sgl_pp_align = bf_get(cfg_sgl_pp_align,
					   mbx_sli4_parameters);
	phba->sli4_hba.extents_in_use = bf_get(cfg_ext, mbx_sli4_parameters);
	phba->sli4_hba.rpi_hdrs_in_use = bf_get(cfg_hdrr, mbx_sli4_parameters);
	sli4_params->mi_cap = bf_get(cfg_mi_ver, mbx_sli4_parameters);

	 
	phba->cfg_xpsgl = bf_get(cfg_xpsgl, mbx_sli4_parameters);

	 
	rc = (bf_get(cfg_nvme, mbx_sli4_parameters) &&
		     bf_get(cfg_xib, mbx_sli4_parameters));

	if (rc) {
		 
		sli4_params->nvme = 1;

		 
		if (phba->cfg_enable_fc4_type == LPFC_ENABLE_FCP) {
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT | LOG_NVME,
					"6133 Disabling NVME support: "
					"FC4 type not supported: x%x\n",
					phba->cfg_enable_fc4_type);
			goto fcponly;
		}
	} else {
		 
		sli4_params->nvme = 0;
		if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT | LOG_NVME,
					"6101 Disabling NVME support: Not "
					"supported by firmware (%d %d) x%x\n",
					bf_get(cfg_nvme, mbx_sli4_parameters),
					bf_get(cfg_xib, mbx_sli4_parameters),
					phba->cfg_enable_fc4_type);
fcponly:
			phba->nvmet_support = 0;
			phba->cfg_nvmet_mrq = 0;
			phba->cfg_nvme_seg_cnt = 0;

			 
			if (!(phba->cfg_enable_fc4_type & LPFC_ENABLE_FCP))
				return -ENODEV;
			phba->cfg_enable_fc4_type = LPFC_ENABLE_FCP;
		}
	}

	 
	if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME)
		phba->cfg_sg_seg_cnt = LPFC_MAX_NVME_SEG_CNT;

	 
	if (bf_get(cfg_pbde, mbx_sli4_parameters))
		phba->cfg_enable_pbde = 1;
	else
		phba->cfg_enable_pbde = 0;

	 
	if (phba->cfg_suppress_rsp && bf_get(cfg_xib, mbx_sli4_parameters) &&
	    !(bf_get(cfg_nosr, mbx_sli4_parameters)))
		phba->sli.sli_flag |= LPFC_SLI_SUPPRESS_RSP;
	else
		phba->cfg_suppress_rsp = 0;

	if (bf_get(cfg_eqdr, mbx_sli4_parameters))
		phba->sli.sli_flag |= LPFC_SLI_USE_EQDR;

	 
	if (sli4_params->sge_supp_len > LPFC_MAX_SGE_SIZE)
		sli4_params->sge_supp_len = LPFC_MAX_SGE_SIZE;

	rc = dma_set_max_seg_size(&phba->pcidev->dev, sli4_params->sge_supp_len);
	if (unlikely(rc)) {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"6400 Can't set dma maximum segment size\n");
		return rc;
	}

	 
	if (bf_get(cfg_ext_embed_cb, mbx_sli4_parameters))
		phba->fcp_embed_io = 1;
	else
		phba->fcp_embed_io = 0;

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT | LOG_NVME,
			"6422 XIB %d PBDE %d: FCP %d NVME %d %d %d\n",
			bf_get(cfg_xib, mbx_sli4_parameters),
			phba->cfg_enable_pbde,
			phba->fcp_embed_io, sli4_params->nvme,
			phba->cfg_nvme_embed_cmd, phba->cfg_suppress_rsp);

	if ((bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) ==
	    LPFC_SLI_INTF_IF_TYPE_2) &&
	    (bf_get(lpfc_sli_intf_sli_family, &phba->sli4_hba.sli_intf) ==
		 LPFC_SLI_INTF_FAMILY_LNCR_A0))
		exp_wqcq_pages = false;

	if ((bf_get(cfg_cqpsize, mbx_sli4_parameters) & LPFC_CQ_16K_PAGE_SZ) &&
	    (bf_get(cfg_wqpsize, mbx_sli4_parameters) & LPFC_WQ_16K_PAGE_SZ) &&
	    exp_wqcq_pages &&
	    (sli4_params->wqsize & LPFC_WQ_SZ128_SUPPORT))
		phba->enab_exp_wqcq_pages = 1;
	else
		phba->enab_exp_wqcq_pages = 0;
	 
	if (bf_get(cfg_mds_diags, mbx_sli4_parameters))
		phba->mds_diags_support = 1;
	else
		phba->mds_diags_support = 0;

	 
	if (bf_get(cfg_nsler, mbx_sli4_parameters))
		phba->nsler = 1;
	else
		phba->nsler = 0;

	return 0;
}

 
static int
lpfc_pci_probe_one_s3(struct pci_dev *pdev, const struct pci_device_id *pid)
{
	struct lpfc_hba   *phba;
	struct lpfc_vport *vport = NULL;
	struct Scsi_Host  *shost = NULL;
	int error;
	uint32_t cfg_mode, intr_mode;

	 
	phba = lpfc_hba_alloc(pdev);
	if (!phba)
		return -ENOMEM;

	 
	error = lpfc_enable_pci_dev(phba);
	if (error)
		goto out_free_phba;

	 
	error = lpfc_api_table_setup(phba, LPFC_PCI_DEV_LP);
	if (error)
		goto out_disable_pci_dev;

	 
	error = lpfc_sli_pci_mem_setup(phba);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1402 Failed to set up pci memory space.\n");
		goto out_disable_pci_dev;
	}

	 
	error = lpfc_sli_driver_resource_setup(phba);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1404 Failed to set up driver resource.\n");
		goto out_unset_pci_mem_s3;
	}

	 

	error = lpfc_init_iocb_list(phba, LPFC_IOCB_LIST_CNT);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1405 Failed to initialize iocb list.\n");
		goto out_unset_driver_resource_s3;
	}

	 
	error = lpfc_setup_driver_resource_phase2(phba);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1406 Failed to set up driver resource.\n");
		goto out_free_iocb_list;
	}

	 
	lpfc_get_hba_model_desc(phba, phba->ModelName, phba->ModelDesc);

	 
	error = lpfc_create_shost(phba);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1407 Failed to create scsi host.\n");
		goto out_unset_driver_resource;
	}

	 
	vport = phba->pport;
	error = lpfc_alloc_sysfs_attr(vport);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1476 Failed to allocate sysfs attr\n");
		goto out_destroy_shost;
	}

	shost = lpfc_shost_from_vport(vport);  
	 
	cfg_mode = phba->cfg_use_msi;
	while (true) {
		 
		lpfc_stop_port(phba);
		 
		intr_mode = lpfc_sli_enable_intr(phba, cfg_mode);
		if (intr_mode == LPFC_INTR_ERROR) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0431 Failed to enable interrupt.\n");
			error = -ENODEV;
			goto out_free_sysfs_attr;
		}
		 
		if (lpfc_sli_hba_setup(phba)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"1477 Failed to set up hba\n");
			error = -ENODEV;
			goto out_remove_device;
		}

		 
		msleep(50);
		 
		if (intr_mode == 0 ||
		    phba->sli.slistat.sli_intr > LPFC_MSIX_VECTORS) {
			 
			phba->intr_mode = intr_mode;
			lpfc_log_intr_mode(phba, intr_mode);
			break;
		} else {
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"0447 Configure interrupt mode (%d) "
					"failed active interrupt test.\n",
					intr_mode);
			 
			lpfc_sli_disable_intr(phba);
			 
			cfg_mode = --intr_mode;
		}
	}

	 
	lpfc_post_init_setup(phba);

	 
	lpfc_create_static_vport(phba);

	return 0;

out_remove_device:
	lpfc_unset_hba(phba);
out_free_sysfs_attr:
	lpfc_free_sysfs_attr(vport);
out_destroy_shost:
	lpfc_destroy_shost(phba);
out_unset_driver_resource:
	lpfc_unset_driver_resource_phase2(phba);
out_free_iocb_list:
	lpfc_free_iocb_list(phba);
out_unset_driver_resource_s3:
	lpfc_sli_driver_resource_unset(phba);
out_unset_pci_mem_s3:
	lpfc_sli_pci_mem_unset(phba);
out_disable_pci_dev:
	lpfc_disable_pci_dev(phba);
	if (shost)
		scsi_host_put(shost);
out_free_phba:
	lpfc_hba_free(phba);
	return error;
}

 
static void
lpfc_pci_remove_one_s3(struct pci_dev *pdev)
{
	struct Scsi_Host  *shost = pci_get_drvdata(pdev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_vport **vports;
	struct lpfc_hba   *phba = vport->phba;
	int i;

	spin_lock_irq(&phba->hbalock);
	vport->load_flag |= FC_UNLOADING;
	spin_unlock_irq(&phba->hbalock);

	lpfc_free_sysfs_attr(vport);

	 
	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			if (vports[i]->port_type == LPFC_PHYSICAL_PORT)
				continue;
			fc_vport_terminate(vports[i]->fc_vport);
		}
	lpfc_destroy_vport_work_array(phba, vports);

	 
	fc_remove_host(shost);
	scsi_remove_host(shost);

	 
	lpfc_cleanup(vport);

	 

	 
	lpfc_sli_hba_down(phba);
	 
	kthread_stop(phba->worker_thread);
	 
	lpfc_sli_brdrestart(phba);

	kfree(phba->vpi_bmask);
	kfree(phba->vpi_ids);

	lpfc_stop_hba_timers(phba);
	spin_lock_irq(&phba->port_list_lock);
	list_del_init(&vport->listentry);
	spin_unlock_irq(&phba->port_list_lock);

	lpfc_debugfs_terminate(vport);

	 
	if (phba->cfg_sriov_nr_virtfn)
		pci_disable_sriov(pdev);

	 
	lpfc_sli_disable_intr(phba);

	scsi_host_put(shost);

	 
	lpfc_scsi_free(phba);
	lpfc_free_iocb_list(phba);

	lpfc_mem_free_all(phba);

	dma_free_coherent(&pdev->dev, lpfc_sli_hbq_size(),
			  phba->hbqslimp.virt, phba->hbqslimp.phys);

	 
	dma_free_coherent(&pdev->dev, SLI2_SLIM_SIZE,
			  phba->slim2p.virt, phba->slim2p.phys);

	 
	iounmap(phba->ctrl_regs_memmap_p);
	iounmap(phba->slim_memmap_p);

	lpfc_hba_free(phba);

	pci_release_mem_regions(pdev);
	pci_disable_device(pdev);
}

 
static int __maybe_unused
lpfc_pci_suspend_one_s3(struct device *dev_d)
{
	struct Scsi_Host *shost = dev_get_drvdata(dev_d);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"0473 PCI device Power Management suspend.\n");

	 
	lpfc_offline_prep(phba, LPFC_MBX_WAIT);
	lpfc_offline(phba);
	kthread_stop(phba->worker_thread);

	 
	lpfc_sli_disable_intr(phba);

	return 0;
}

 
static int __maybe_unused
lpfc_pci_resume_one_s3(struct device *dev_d)
{
	struct Scsi_Host *shost = dev_get_drvdata(dev_d);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;
	uint32_t intr_mode;
	int error;

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"0452 PCI device Power Management resume.\n");

	 
	phba->worker_thread = kthread_run(lpfc_do_work, phba,
					"lpfc_worker_%d", phba->brd_no);
	if (IS_ERR(phba->worker_thread)) {
		error = PTR_ERR(phba->worker_thread);
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0434 PM resume failed to start worker "
				"thread: error=x%x.\n", error);
		return error;
	}

	 
	lpfc_cpu_map_array_init(phba);
	 
	lpfc_hba_eq_hdl_array_init(phba);
	 
	intr_mode = lpfc_sli_enable_intr(phba, phba->intr_mode);
	if (intr_mode == LPFC_INTR_ERROR) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0430 PM resume Failed to enable interrupt\n");
		return -EIO;
	} else
		phba->intr_mode = intr_mode;

	 
	lpfc_sli_brdrestart(phba);
	lpfc_online(phba);

	 
	lpfc_log_intr_mode(phba, phba->intr_mode);

	return 0;
}

 
static void
lpfc_sli_prep_dev_for_recover(struct lpfc_hba *phba)
{
	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"2723 PCI channel I/O abort preparing for recovery\n");

	 
	lpfc_sli_abort_fcp_rings(phba);
}

 
static void
lpfc_sli_prep_dev_for_reset(struct lpfc_hba *phba)
{
	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"2710 PCI channel disable preparing for reset\n");

	 
	lpfc_block_mgmt_io(phba, LPFC_MBX_WAIT);

	 
	lpfc_scsi_dev_block(phba);

	 
	lpfc_sli_flush_io_rings(phba);

	 
	lpfc_stop_hba_timers(phba);

	 
	lpfc_sli_disable_intr(phba);
	pci_disable_device(phba->pcidev);
}

 
static void
lpfc_sli_prep_dev_for_perm_failure(struct lpfc_hba *phba)
{
	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"2711 PCI channel permanent disable for failure\n");
	 
	lpfc_scsi_dev_block(phba);
	lpfc_sli4_prep_dev_for_reset(phba);

	 
	lpfc_stop_hba_timers(phba);

	 
	lpfc_sli_flush_io_rings(phba);
}

 
static pci_ers_result_t
lpfc_io_error_detected_s3(struct pci_dev *pdev, pci_channel_state_t state)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;

	switch (state) {
	case pci_channel_io_normal:
		 
		lpfc_sli_prep_dev_for_recover(phba);
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_frozen:
		 
		lpfc_sli_prep_dev_for_reset(phba);
		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		 
		lpfc_sli_prep_dev_for_perm_failure(phba);
		return PCI_ERS_RESULT_DISCONNECT;
	default:
		 
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0472 Unknown PCI error state: x%x\n", state);
		lpfc_sli_prep_dev_for_reset(phba);
		return PCI_ERS_RESULT_NEED_RESET;
	}
}

 
static pci_ers_result_t
lpfc_io_slot_reset_s3(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;
	struct lpfc_sli *psli = &phba->sli;
	uint32_t intr_mode;

	dev_printk(KERN_INFO, &pdev->dev, "recovering from a slot reset.\n");
	if (pci_enable_device_mem(pdev)) {
		printk(KERN_ERR "lpfc: Cannot re-enable "
			"PCI device after reset.\n");
		return PCI_ERS_RESULT_DISCONNECT;
	}

	pci_restore_state(pdev);

	 
	pci_save_state(pdev);

	if (pdev->is_busmaster)
		pci_set_master(pdev);

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag &= ~LPFC_SLI_ACTIVE;
	spin_unlock_irq(&phba->hbalock);

	 
	intr_mode = lpfc_sli_enable_intr(phba, phba->intr_mode);
	if (intr_mode == LPFC_INTR_ERROR) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0427 Cannot re-enable interrupt after "
				"slot reset.\n");
		return PCI_ERS_RESULT_DISCONNECT;
	} else
		phba->intr_mode = intr_mode;

	 
	lpfc_offline_prep(phba, LPFC_MBX_WAIT);
	lpfc_offline(phba);
	lpfc_sli_brdrestart(phba);

	 
	lpfc_log_intr_mode(phba, phba->intr_mode);

	return PCI_ERS_RESULT_RECOVERED;
}

 
static void
lpfc_io_resume_s3(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;

	 
	lpfc_online(phba);
}

 
int
lpfc_sli4_get_els_iocb_cnt(struct lpfc_hba *phba)
{
	int max_xri = phba->sli4_hba.max_cfg_param.max_xri;

	if (phba->sli_rev == LPFC_SLI_REV4) {
		if (max_xri <= 100)
			return 10;
		else if (max_xri <= 256)
			return 25;
		else if (max_xri <= 512)
			return 50;
		else if (max_xri <= 1024)
			return 100;
		else if (max_xri <= 1536)
			return 150;
		else if (max_xri <= 2048)
			return 200;
		else
			return 250;
	} else
		return 0;
}

 
int
lpfc_sli4_get_iocb_cnt(struct lpfc_hba *phba)
{
	int max_xri = lpfc_sli4_get_els_iocb_cnt(phba);

	if (phba->nvmet_support)
		max_xri += LPFC_NVMET_BUF_POST;
	return max_xri;
}


static int
lpfc_log_write_firmware_error(struct lpfc_hba *phba, uint32_t offset,
	uint32_t magic_number, uint32_t ftype, uint32_t fid, uint32_t fsize,
	const struct firmware *fw)
{
	int rc;
	u8 sli_family;

	sli_family = bf_get(lpfc_sli_intf_sli_family, &phba->sli4_hba.sli_intf);
	 
	if (offset == ADD_STATUS_FW_NOT_SUPPORTED ||
	    (sli_family == LPFC_SLI_INTF_FAMILY_G6 &&
	     magic_number != MAGIC_NUMBER_G6) ||
	    (sli_family == LPFC_SLI_INTF_FAMILY_G7 &&
	     magic_number != MAGIC_NUMBER_G7) ||
	    (sli_family == LPFC_SLI_INTF_FAMILY_G7P &&
	     magic_number != MAGIC_NUMBER_G7P)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3030 This firmware version is not supported on"
				" this HBA model. Device:%x Magic:%x Type:%x "
				"ID:%x Size %d %zd\n",
				phba->pcidev->device, magic_number, ftype, fid,
				fsize, fw->size);
		rc = -EINVAL;
	} else if (offset == ADD_STATUS_FW_DOWNLOAD_HW_DISABLED) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3021 Firmware downloads have been prohibited "
				"by a system configuration setting on "
				"Device:%x Magic:%x Type:%x ID:%x Size %d "
				"%zd\n",
				phba->pcidev->device, magic_number, ftype, fid,
				fsize, fw->size);
		rc = -EACCES;
	} else {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3022 FW Download failed. Add Status x%x "
				"Device:%x Magic:%x Type:%x ID:%x Size %d "
				"%zd\n",
				offset, phba->pcidev->device, magic_number,
				ftype, fid, fsize, fw->size);
		rc = -EIO;
	}
	return rc;
}

 
static void
lpfc_write_firmware(const struct firmware *fw, void *context)
{
	struct lpfc_hba *phba = (struct lpfc_hba *)context;
	char fwrev[FW_REV_STR_SIZE];
	struct lpfc_grp_hdr *image;
	struct list_head dma_buffer_list;
	int i, rc = 0;
	struct lpfc_dmabuf *dmabuf, *next;
	uint32_t offset = 0, temp_offset = 0;
	uint32_t magic_number, ftype, fid, fsize;

	 
	if (!fw) {
		rc = -ENXIO;
		goto out;
	}
	image = (struct lpfc_grp_hdr *)fw->data;

	magic_number = be32_to_cpu(image->magic_number);
	ftype = bf_get_be32(lpfc_grp_hdr_file_type, image);
	fid = bf_get_be32(lpfc_grp_hdr_id, image);
	fsize = be32_to_cpu(image->size);

	INIT_LIST_HEAD(&dma_buffer_list);
	lpfc_decode_firmware_rev(phba, fwrev, 1);
	if (strncmp(fwrev, image->revision, strnlen(image->revision, 16))) {
		lpfc_log_msg(phba, KERN_NOTICE, LOG_INIT | LOG_SLI,
			     "3023 Updating Firmware, Current Version:%s "
			     "New Version:%s\n",
			     fwrev, image->revision);
		for (i = 0; i < LPFC_MBX_WR_CONFIG_MAX_BDE; i++) {
			dmabuf = kzalloc(sizeof(struct lpfc_dmabuf),
					 GFP_KERNEL);
			if (!dmabuf) {
				rc = -ENOMEM;
				goto release_out;
			}
			dmabuf->virt = dma_alloc_coherent(&phba->pcidev->dev,
							  SLI4_PAGE_SIZE,
							  &dmabuf->phys,
							  GFP_KERNEL);
			if (!dmabuf->virt) {
				kfree(dmabuf);
				rc = -ENOMEM;
				goto release_out;
			}
			list_add_tail(&dmabuf->list, &dma_buffer_list);
		}
		while (offset < fw->size) {
			temp_offset = offset;
			list_for_each_entry(dmabuf, &dma_buffer_list, list) {
				if (temp_offset + SLI4_PAGE_SIZE > fw->size) {
					memcpy(dmabuf->virt,
					       fw->data + temp_offset,
					       fw->size - temp_offset);
					temp_offset = fw->size;
					break;
				}
				memcpy(dmabuf->virt, fw->data + temp_offset,
				       SLI4_PAGE_SIZE);
				temp_offset += SLI4_PAGE_SIZE;
			}
			rc = lpfc_wr_object(phba, &dma_buffer_list,
				    (fw->size - offset), &offset);
			if (rc) {
				rc = lpfc_log_write_firmware_error(phba, offset,
								   magic_number,
								   ftype,
								   fid,
								   fsize,
								   fw);
				goto release_out;
			}
		}
		rc = offset;
	} else
		lpfc_log_msg(phba, KERN_NOTICE, LOG_INIT | LOG_SLI,
			     "3029 Skipped Firmware update, Current "
			     "Version:%s New Version:%s\n",
			     fwrev, image->revision);

release_out:
	list_for_each_entry_safe(dmabuf, next, &dma_buffer_list, list) {
		list_del(&dmabuf->list);
		dma_free_coherent(&phba->pcidev->dev, SLI4_PAGE_SIZE,
				  dmabuf->virt, dmabuf->phys);
		kfree(dmabuf);
	}
	release_firmware(fw);
out:
	if (rc < 0)
		lpfc_log_msg(phba, KERN_ERR, LOG_INIT | LOG_SLI,
			     "3062 Firmware update error, status %d.\n", rc);
	else
		lpfc_log_msg(phba, KERN_NOTICE, LOG_INIT | LOG_SLI,
			     "3024 Firmware update success: size %d.\n", rc);
}

 
int
lpfc_sli4_request_firmware_update(struct lpfc_hba *phba, uint8_t fw_upgrade)
{
	uint8_t file_name[ELX_MODEL_NAME_SIZE];
	int ret;
	const struct firmware *fw;

	 
	if (bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) <
	    LPFC_SLI_INTF_IF_TYPE_2)
		return -EPERM;

	snprintf(file_name, ELX_MODEL_NAME_SIZE, "%s.grp", phba->ModelName);

	if (fw_upgrade == INT_FW_UPGRADE) {
		ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_UEVENT,
					file_name, &phba->pcidev->dev,
					GFP_KERNEL, (void *)phba,
					lpfc_write_firmware);
	} else if (fw_upgrade == RUN_FW_UPGRADE) {
		ret = request_firmware(&fw, file_name, &phba->pcidev->dev);
		if (!ret)
			lpfc_write_firmware(fw, (void *)phba);
	} else {
		ret = -EINVAL;
	}

	return ret;
}

 
static int
lpfc_pci_probe_one_s4(struct pci_dev *pdev, const struct pci_device_id *pid)
{
	struct lpfc_hba   *phba;
	struct lpfc_vport *vport = NULL;
	struct Scsi_Host  *shost = NULL;
	int error;
	uint32_t cfg_mode, intr_mode;

	 
	phba = lpfc_hba_alloc(pdev);
	if (!phba)
		return -ENOMEM;

	INIT_LIST_HEAD(&phba->poll_list);

	 
	error = lpfc_enable_pci_dev(phba);
	if (error)
		goto out_free_phba;

	 
	error = lpfc_api_table_setup(phba, LPFC_PCI_DEV_OC);
	if (error)
		goto out_disable_pci_dev;

	 
	error = lpfc_sli4_pci_mem_setup(phba);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1410 Failed to set up pci memory space.\n");
		goto out_disable_pci_dev;
	}

	 
	error = lpfc_sli4_driver_resource_setup(phba);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1412 Failed to set up driver resource.\n");
		goto out_unset_pci_mem_s4;
	}

	INIT_LIST_HEAD(&phba->active_rrq_list);
	INIT_LIST_HEAD(&phba->fcf.fcf_pri_list);

	 
	error = lpfc_setup_driver_resource_phase2(phba);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1414 Failed to set up driver resource.\n");
		goto out_unset_driver_resource_s4;
	}

	 
	lpfc_get_hba_model_desc(phba, phba->ModelName, phba->ModelDesc);

	 
	cfg_mode = phba->cfg_use_msi;

	 
	phba->pport = NULL;
	lpfc_stop_port(phba);

	 
	lpfc_cpu_map_array_init(phba);

	 
	lpfc_hba_eq_hdl_array_init(phba);

	 
	intr_mode = lpfc_sli4_enable_intr(phba, cfg_mode);
	if (intr_mode == LPFC_INTR_ERROR) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0426 Failed to enable interrupt.\n");
		error = -ENODEV;
		goto out_unset_driver_resource;
	}
	 
	if (phba->intr_type != MSIX) {
		phba->cfg_irq_chann = 1;
		if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME) {
			if (phba->nvmet_support)
				phba->cfg_nvmet_mrq = 1;
		}
	}
	lpfc_cpu_affinity_check(phba, phba->cfg_irq_chann);

	 
	error = lpfc_create_shost(phba);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1415 Failed to create scsi host.\n");
		goto out_disable_intr;
	}
	vport = phba->pport;
	shost = lpfc_shost_from_vport(vport);  

	 
	error = lpfc_alloc_sysfs_attr(vport);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1416 Failed to allocate sysfs attr\n");
		goto out_destroy_shost;
	}

	 
	if (lpfc_sli4_hba_setup(phba)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"1421 Failed to set up hba\n");
		error = -ENODEV;
		goto out_free_sysfs_attr;
	}

	 
	phba->intr_mode = intr_mode;
	lpfc_log_intr_mode(phba, intr_mode);

	 
	lpfc_post_init_setup(phba);

	 
	if (phba->nvmet_support == 0) {
		if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME) {
			 
			error = lpfc_nvme_create_localport(vport);
			if (error) {
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"6004 NVME registration "
						"failed, error x%x\n",
						error);
			}
		}
	}

	 
	if (phba->cfg_request_firmware_upgrade)
		lpfc_sli4_request_firmware_update(phba, INT_FW_UPGRADE);

	 
	lpfc_create_static_vport(phba);

	timer_setup(&phba->cpuhp_poll_timer, lpfc_sli4_poll_hbtimer, 0);
	cpuhp_state_add_instance_nocalls(lpfc_cpuhp_state, &phba->cpuhp);

	return 0;

out_free_sysfs_attr:
	lpfc_free_sysfs_attr(vport);
out_destroy_shost:
	lpfc_destroy_shost(phba);
out_disable_intr:
	lpfc_sli4_disable_intr(phba);
out_unset_driver_resource:
	lpfc_unset_driver_resource_phase2(phba);
out_unset_driver_resource_s4:
	lpfc_sli4_driver_resource_unset(phba);
out_unset_pci_mem_s4:
	lpfc_sli4_pci_mem_unset(phba);
out_disable_pci_dev:
	lpfc_disable_pci_dev(phba);
	if (shost)
		scsi_host_put(shost);
out_free_phba:
	lpfc_hba_free(phba);
	return error;
}

 
static void
lpfc_pci_remove_one_s4(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_vport **vports;
	struct lpfc_hba *phba = vport->phba;
	int i;

	 
	spin_lock_irq(&phba->hbalock);
	vport->load_flag |= FC_UNLOADING;
	spin_unlock_irq(&phba->hbalock);
	if (phba->cgn_i)
		lpfc_unreg_congestion_buf(phba);

	lpfc_free_sysfs_attr(vport);

	 
	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			if (vports[i]->port_type == LPFC_PHYSICAL_PORT)
				continue;
			fc_vport_terminate(vports[i]->fc_vport);
		}
	lpfc_destroy_vport_work_array(phba, vports);

	 
	fc_remove_host(shost);
	scsi_remove_host(shost);

	 
	lpfc_cleanup(vport);
	lpfc_nvmet_destroy_targetport(phba);
	lpfc_nvme_destroy_localport(vport);

	 
	if (phba->cfg_xri_rebalancing)
		lpfc_destroy_multixri_pools(phba);

	 
	lpfc_debugfs_terminate(vport);

	lpfc_stop_hba_timers(phba);
	spin_lock_irq(&phba->port_list_lock);
	list_del_init(&vport->listentry);
	spin_unlock_irq(&phba->port_list_lock);

	 
	lpfc_io_free(phba);
	lpfc_free_iocb_list(phba);
	lpfc_sli4_hba_unset(phba);

	lpfc_unset_driver_resource_phase2(phba);
	lpfc_sli4_driver_resource_unset(phba);

	 
	lpfc_sli4_pci_mem_unset(phba);

	 
	scsi_host_put(shost);
	lpfc_disable_pci_dev(phba);

	 
	lpfc_hba_free(phba);

	return;
}

 
static int __maybe_unused
lpfc_pci_suspend_one_s4(struct device *dev_d)
{
	struct Scsi_Host *shost = dev_get_drvdata(dev_d);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"2843 PCI device Power Management suspend.\n");

	 
	lpfc_offline_prep(phba, LPFC_MBX_WAIT);
	lpfc_offline(phba);
	kthread_stop(phba->worker_thread);

	 
	lpfc_sli4_disable_intr(phba);
	lpfc_sli4_queue_destroy(phba);

	return 0;
}

 
static int __maybe_unused
lpfc_pci_resume_one_s4(struct device *dev_d)
{
	struct Scsi_Host *shost = dev_get_drvdata(dev_d);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;
	uint32_t intr_mode;
	int error;

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"0292 PCI device Power Management resume.\n");

	  
	phba->worker_thread = kthread_run(lpfc_do_work, phba,
					"lpfc_worker_%d", phba->brd_no);
	if (IS_ERR(phba->worker_thread)) {
		error = PTR_ERR(phba->worker_thread);
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0293 PM resume failed to start worker "
				"thread: error=x%x.\n", error);
		return error;
	}

	 
	intr_mode = lpfc_sli4_enable_intr(phba, phba->intr_mode);
	if (intr_mode == LPFC_INTR_ERROR) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0294 PM resume Failed to enable interrupt\n");
		return -EIO;
	} else
		phba->intr_mode = intr_mode;

	 
	lpfc_sli_brdrestart(phba);
	lpfc_online(phba);

	 
	lpfc_log_intr_mode(phba, phba->intr_mode);

	return 0;
}

 
static void
lpfc_sli4_prep_dev_for_recover(struct lpfc_hba *phba)
{
	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"2828 PCI channel I/O abort preparing for recovery\n");
	 
	lpfc_sli_abort_fcp_rings(phba);
}

 
static void
lpfc_sli4_prep_dev_for_reset(struct lpfc_hba *phba)
{
	int offline =  pci_channel_offline(phba->pcidev);

	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2826 PCI channel disable preparing for reset offline"
			" %d\n", offline);

	 
	lpfc_block_mgmt_io(phba, LPFC_MBX_NO_WAIT);


	 
	lpfc_offline_prep(phba, LPFC_MBX_NO_WAIT);
	 
	lpfc_sli_flush_io_rings(phba);
	lpfc_offline(phba);

	 
	lpfc_stop_hba_timers(phba);

	lpfc_sli4_queue_destroy(phba);
	 
	lpfc_sli4_disable_intr(phba);
	pci_disable_device(phba->pcidev);
}

 
static void
lpfc_sli4_prep_dev_for_perm_failure(struct lpfc_hba *phba)
{
	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"2827 PCI channel permanent disable for failure\n");

	 
	lpfc_scsi_dev_block(phba);

	 
	lpfc_stop_hba_timers(phba);

	 
	lpfc_sli_flush_io_rings(phba);
}

 
static pci_ers_result_t
lpfc_io_error_detected_s4(struct pci_dev *pdev, pci_channel_state_t state)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;
	bool hba_pci_err;

	switch (state) {
	case pci_channel_io_normal:
		 
		lpfc_sli4_prep_dev_for_recover(phba);
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_frozen:
		hba_pci_err = test_and_set_bit(HBA_PCI_ERR, &phba->bit_flags);
		 
		if (!hba_pci_err)
			lpfc_sli4_prep_dev_for_reset(phba);
		else
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"2832  Already handling PCI error "
					"state: x%x\n", state);
		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		set_bit(HBA_PCI_ERR, &phba->bit_flags);
		 
		lpfc_sli4_prep_dev_for_perm_failure(phba);
		return PCI_ERS_RESULT_DISCONNECT;
	default:
		hba_pci_err = test_and_set_bit(HBA_PCI_ERR, &phba->bit_flags);
		if (!hba_pci_err)
			lpfc_sli4_prep_dev_for_reset(phba);
		 
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2825 Unknown PCI error state: x%x\n", state);
		lpfc_sli4_prep_dev_for_reset(phba);
		return PCI_ERS_RESULT_NEED_RESET;
	}
}

 
static pci_ers_result_t
lpfc_io_slot_reset_s4(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;
	struct lpfc_sli *psli = &phba->sli;
	uint32_t intr_mode;
	bool hba_pci_err;

	dev_printk(KERN_INFO, &pdev->dev, "recovering from a slot reset.\n");
	if (pci_enable_device_mem(pdev)) {
		printk(KERN_ERR "lpfc: Cannot re-enable "
		       "PCI device after reset.\n");
		return PCI_ERS_RESULT_DISCONNECT;
	}

	pci_restore_state(pdev);

	hba_pci_err = test_and_clear_bit(HBA_PCI_ERR, &phba->bit_flags);
	if (!hba_pci_err)
		dev_info(&pdev->dev,
			 "hba_pci_err was not set, recovering slot reset.\n");
	 
	pci_save_state(pdev);

	if (pdev->is_busmaster)
		pci_set_master(pdev);

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag &= ~LPFC_SLI_ACTIVE;
	spin_unlock_irq(&phba->hbalock);

	 
	lpfc_cpu_map_array_init(phba);
	 
	intr_mode = lpfc_sli4_enable_intr(phba, phba->intr_mode);
	if (intr_mode == LPFC_INTR_ERROR) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2824 Cannot re-enable interrupt after "
				"slot reset.\n");
		return PCI_ERS_RESULT_DISCONNECT;
	} else
		phba->intr_mode = intr_mode;
	lpfc_cpu_affinity_check(phba, phba->cfg_irq_chann);

	 
	lpfc_log_intr_mode(phba, phba->intr_mode);

	return PCI_ERS_RESULT_RECOVERED;
}

 
static void
lpfc_io_resume_s4(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;

	 
	if (!(phba->sli.sli_flag & LPFC_SLI_ACTIVE)) {
		 
		lpfc_sli_brdrestart(phba);
		 
		lpfc_online(phba);
	}
}

 
static int
lpfc_pci_probe_one(struct pci_dev *pdev, const struct pci_device_id *pid)
{
	int rc;
	struct lpfc_sli_intf intf;

	if (pci_read_config_dword(pdev, LPFC_SLI_INTF, &intf.word0))
		return -ENODEV;

	if ((bf_get(lpfc_sli_intf_valid, &intf) == LPFC_SLI_INTF_VALID) &&
	    (bf_get(lpfc_sli_intf_slirev, &intf) == LPFC_SLI_INTF_REV_SLI4))
		rc = lpfc_pci_probe_one_s4(pdev, pid);
	else
		rc = lpfc_pci_probe_one_s3(pdev, pid);

	return rc;
}

 
static void
lpfc_pci_remove_one(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;

	switch (phba->pci_dev_grp) {
	case LPFC_PCI_DEV_LP:
		lpfc_pci_remove_one_s3(pdev);
		break;
	case LPFC_PCI_DEV_OC:
		lpfc_pci_remove_one_s4(pdev);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"1424 Invalid PCI device group: 0x%x\n",
				phba->pci_dev_grp);
		break;
	}
	return;
}

 
static int __maybe_unused
lpfc_pci_suspend_one(struct device *dev)
{
	struct Scsi_Host *shost = dev_get_drvdata(dev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;
	int rc = -ENODEV;

	switch (phba->pci_dev_grp) {
	case LPFC_PCI_DEV_LP:
		rc = lpfc_pci_suspend_one_s3(dev);
		break;
	case LPFC_PCI_DEV_OC:
		rc = lpfc_pci_suspend_one_s4(dev);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"1425 Invalid PCI device group: 0x%x\n",
				phba->pci_dev_grp);
		break;
	}
	return rc;
}

 
static int __maybe_unused
lpfc_pci_resume_one(struct device *dev)
{
	struct Scsi_Host *shost = dev_get_drvdata(dev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;
	int rc = -ENODEV;

	switch (phba->pci_dev_grp) {
	case LPFC_PCI_DEV_LP:
		rc = lpfc_pci_resume_one_s3(dev);
		break;
	case LPFC_PCI_DEV_OC:
		rc = lpfc_pci_resume_one_s4(dev);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"1426 Invalid PCI device group: 0x%x\n",
				phba->pci_dev_grp);
		break;
	}
	return rc;
}

 
static pci_ers_result_t
lpfc_io_error_detected(struct pci_dev *pdev, pci_channel_state_t state)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;
	pci_ers_result_t rc = PCI_ERS_RESULT_DISCONNECT;

	if (phba->link_state == LPFC_HBA_ERROR &&
	    phba->hba_flag & HBA_IOQ_FLUSH)
		return PCI_ERS_RESULT_NEED_RESET;

	switch (phba->pci_dev_grp) {
	case LPFC_PCI_DEV_LP:
		rc = lpfc_io_error_detected_s3(pdev, state);
		break;
	case LPFC_PCI_DEV_OC:
		rc = lpfc_io_error_detected_s4(pdev, state);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"1427 Invalid PCI device group: 0x%x\n",
				phba->pci_dev_grp);
		break;
	}
	return rc;
}

 
static pci_ers_result_t
lpfc_io_slot_reset(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;
	pci_ers_result_t rc = PCI_ERS_RESULT_DISCONNECT;

	switch (phba->pci_dev_grp) {
	case LPFC_PCI_DEV_LP:
		rc = lpfc_io_slot_reset_s3(pdev);
		break;
	case LPFC_PCI_DEV_OC:
		rc = lpfc_io_slot_reset_s4(pdev);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"1428 Invalid PCI device group: 0x%x\n",
				phba->pci_dev_grp);
		break;
	}
	return rc;
}

 
static void
lpfc_io_resume(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;

	switch (phba->pci_dev_grp) {
	case LPFC_PCI_DEV_LP:
		lpfc_io_resume_s3(pdev);
		break;
	case LPFC_PCI_DEV_OC:
		lpfc_io_resume_s4(pdev);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"1429 Invalid PCI device group: 0x%x\n",
				phba->pci_dev_grp);
		break;
	}
	return;
}

 
static void
lpfc_sli4_oas_verify(struct lpfc_hba *phba)
{

	if (!phba->cfg_EnableXLane)
		return;

	if (phba->sli4_hba.pc_sli4_params.oas_supported) {
		phba->cfg_fof = 1;
	} else {
		phba->cfg_fof = 0;
		mempool_destroy(phba->device_data_mem_pool);
		phba->device_data_mem_pool = NULL;
	}

	return;
}

 
void
lpfc_sli4_ras_init(struct lpfc_hba *phba)
{
	 
	if ((bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) ==
		    LPFC_SLI_INTF_IF_TYPE_6) ||
	    (bf_get(lpfc_sli_intf_sli_family, &phba->sli4_hba.sli_intf) ==
		    LPFC_SLI_INTF_FAMILY_G6)) {
		phba->ras_fwlog.ras_hwsupport = true;
		if (phba->cfg_ras_fwlog_func == PCI_FUNC(phba->pcidev->devfn) &&
		    phba->cfg_ras_fwlog_buffsize)
			phba->ras_fwlog.ras_enabled = true;
		else
			phba->ras_fwlog.ras_enabled = false;
	} else {
		phba->ras_fwlog.ras_hwsupport = false;
	}
}


MODULE_DEVICE_TABLE(pci, lpfc_id_table);

static const struct pci_error_handlers lpfc_err_handler = {
	.error_detected = lpfc_io_error_detected,
	.slot_reset = lpfc_io_slot_reset,
	.resume = lpfc_io_resume,
};

static SIMPLE_DEV_PM_OPS(lpfc_pci_pm_ops_one,
			 lpfc_pci_suspend_one,
			 lpfc_pci_resume_one);

static struct pci_driver lpfc_driver = {
	.name		= LPFC_DRIVER_NAME,
	.id_table	= lpfc_id_table,
	.probe		= lpfc_pci_probe_one,
	.remove		= lpfc_pci_remove_one,
	.shutdown	= lpfc_pci_remove_one,
	.driver.pm	= &lpfc_pci_pm_ops_one,
	.err_handler    = &lpfc_err_handler,
};

static const struct file_operations lpfc_mgmt_fop = {
	.owner = THIS_MODULE,
};

static struct miscdevice lpfc_mgmt_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "lpfcmgmt",
	.fops = &lpfc_mgmt_fop,
};

 
static int __init
lpfc_init(void)
{
	int error = 0;

	pr_info(LPFC_MODULE_DESC "\n");
	pr_info(LPFC_COPYRIGHT "\n");

	error = misc_register(&lpfc_mgmt_dev);
	if (error)
		printk(KERN_ERR "Could not register lpfcmgmt device, "
			"misc_register returned with status %d", error);

	error = -ENOMEM;
	lpfc_transport_functions.vport_create = lpfc_vport_create;
	lpfc_transport_functions.vport_delete = lpfc_vport_delete;
	lpfc_transport_template =
				fc_attach_transport(&lpfc_transport_functions);
	if (lpfc_transport_template == NULL)
		goto unregister;
	lpfc_vport_transport_template =
		fc_attach_transport(&lpfc_vport_transport_functions);
	if (lpfc_vport_transport_template == NULL) {
		fc_release_transport(lpfc_transport_template);
		goto unregister;
	}
	lpfc_wqe_cmd_template();
	lpfc_nvmet_cmd_template();

	 
	lpfc_present_cpu = num_present_cpus();

	lpfc_pldv_detect = false;

	error = cpuhp_setup_state_multi(CPUHP_AP_ONLINE_DYN,
					"lpfc/sli4:online",
					lpfc_cpu_online, lpfc_cpu_offline);
	if (error < 0)
		goto cpuhp_failure;
	lpfc_cpuhp_state = error;

	error = pci_register_driver(&lpfc_driver);
	if (error)
		goto unwind;

	return error;

unwind:
	cpuhp_remove_multi_state(lpfc_cpuhp_state);
cpuhp_failure:
	fc_release_transport(lpfc_transport_template);
	fc_release_transport(lpfc_vport_transport_template);
unregister:
	misc_deregister(&lpfc_mgmt_dev);

	return error;
}

void lpfc_dmp_dbg(struct lpfc_hba *phba)
{
	unsigned int start_idx;
	unsigned int dbg_cnt;
	unsigned int temp_idx;
	int i;
	int j = 0;
	unsigned long rem_nsec;

	if (atomic_cmpxchg(&phba->dbg_log_dmping, 0, 1) != 0)
		return;

	start_idx = (unsigned int)atomic_read(&phba->dbg_log_idx) % DBG_LOG_SZ;
	dbg_cnt = (unsigned int)atomic_read(&phba->dbg_log_cnt);
	if (!dbg_cnt)
		goto out;
	temp_idx = start_idx;
	if (dbg_cnt >= DBG_LOG_SZ) {
		dbg_cnt = DBG_LOG_SZ;
		temp_idx -= 1;
	} else {
		if ((start_idx + dbg_cnt) > (DBG_LOG_SZ - 1)) {
			temp_idx = (start_idx + dbg_cnt) % DBG_LOG_SZ;
		} else {
			if (start_idx < dbg_cnt)
				start_idx = DBG_LOG_SZ - (dbg_cnt - start_idx);
			else
				start_idx -= dbg_cnt;
		}
	}
	dev_info(&phba->pcidev->dev, "start %d end %d cnt %d\n",
		 start_idx, temp_idx, dbg_cnt);

	for (i = 0; i < dbg_cnt; i++) {
		if ((start_idx + i) < DBG_LOG_SZ)
			temp_idx = (start_idx + i) % DBG_LOG_SZ;
		else
			temp_idx = j++;
		rem_nsec = do_div(phba->dbg_log[temp_idx].t_ns, NSEC_PER_SEC);
		dev_info(&phba->pcidev->dev, "%d: [%5lu.%06lu] %s",
			 temp_idx,
			 (unsigned long)phba->dbg_log[temp_idx].t_ns,
			 rem_nsec / 1000,
			 phba->dbg_log[temp_idx].log);
	}
out:
	atomic_set(&phba->dbg_log_cnt, 0);
	atomic_set(&phba->dbg_log_dmping, 0);
}

__printf(2, 3)
void lpfc_dbg_print(struct lpfc_hba *phba, const char *fmt, ...)
{
	unsigned int idx;
	va_list args;
	int dbg_dmping = atomic_read(&phba->dbg_log_dmping);
	struct va_format vaf;


	va_start(args, fmt);
	if (unlikely(dbg_dmping)) {
		vaf.fmt = fmt;
		vaf.va = &args;
		dev_info(&phba->pcidev->dev, "%pV", &vaf);
		va_end(args);
		return;
	}
	idx = (unsigned int)atomic_fetch_add(1, &phba->dbg_log_idx) %
		DBG_LOG_SZ;

	atomic_inc(&phba->dbg_log_cnt);

	vscnprintf(phba->dbg_log[idx].log,
		   sizeof(phba->dbg_log[idx].log), fmt, args);
	va_end(args);

	phba->dbg_log[idx].t_ns = local_clock();
}

 
static void __exit
lpfc_exit(void)
{
	misc_deregister(&lpfc_mgmt_dev);
	pci_unregister_driver(&lpfc_driver);
	cpuhp_remove_multi_state(lpfc_cpuhp_state);
	fc_release_transport(lpfc_transport_template);
	fc_release_transport(lpfc_vport_transport_template);
	idr_destroy(&lpfc_hba_index);
}

module_init(lpfc_init);
module_exit(lpfc_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(LPFC_MODULE_DESC);
MODULE_AUTHOR("Broadcom");
MODULE_VERSION("0:" LPFC_DRIVER_VERSION);
