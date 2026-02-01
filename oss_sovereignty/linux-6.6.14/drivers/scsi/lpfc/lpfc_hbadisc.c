 

#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/lockdep.h>
#include <linux/utsname.h>

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_fc.h>
#include <scsi/fc/fc_fs.h>

#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_nl.h"
#include "lpfc_disc.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc.h"
#include "lpfc_scsi.h"
#include "lpfc_nvme.h"
#include "lpfc_logmsg.h"
#include "lpfc_crtn.h"
#include "lpfc_vport.h"
#include "lpfc_debugfs.h"

 
static uint8_t lpfcAlpaArray[] = {
	0xEF, 0xE8, 0xE4, 0xE2, 0xE1, 0xE0, 0xDC, 0xDA, 0xD9, 0xD6,
	0xD5, 0xD4, 0xD3, 0xD2, 0xD1, 0xCE, 0xCD, 0xCC, 0xCB, 0xCA,
	0xC9, 0xC7, 0xC6, 0xC5, 0xC3, 0xBC, 0xBA, 0xB9, 0xB6, 0xB5,
	0xB4, 0xB3, 0xB2, 0xB1, 0xAE, 0xAD, 0xAC, 0xAB, 0xAA, 0xA9,
	0xA7, 0xA6, 0xA5, 0xA3, 0x9F, 0x9E, 0x9D, 0x9B, 0x98, 0x97,
	0x90, 0x8F, 0x88, 0x84, 0x82, 0x81, 0x80, 0x7C, 0x7A, 0x79,
	0x76, 0x75, 0x74, 0x73, 0x72, 0x71, 0x6E, 0x6D, 0x6C, 0x6B,
	0x6A, 0x69, 0x67, 0x66, 0x65, 0x63, 0x5C, 0x5A, 0x59, 0x56,
	0x55, 0x54, 0x53, 0x52, 0x51, 0x4E, 0x4D, 0x4C, 0x4B, 0x4A,
	0x49, 0x47, 0x46, 0x45, 0x43, 0x3C, 0x3A, 0x39, 0x36, 0x35,
	0x34, 0x33, 0x32, 0x31, 0x2E, 0x2D, 0x2C, 0x2B, 0x2A, 0x29,
	0x27, 0x26, 0x25, 0x23, 0x1F, 0x1E, 0x1D, 0x1B, 0x18, 0x17,
	0x10, 0x0F, 0x08, 0x04, 0x02, 0x01
};

static void lpfc_disc_timeout_handler(struct lpfc_vport *);
static void lpfc_disc_flush_list(struct lpfc_vport *vport);
static void lpfc_unregister_fcfi_cmpl(struct lpfc_hba *, LPFC_MBOXQ_t *);
static int lpfc_fcf_inuse(struct lpfc_hba *);
static void lpfc_mbx_cmpl_read_sparam(struct lpfc_hba *, LPFC_MBOXQ_t *);
static void lpfc_check_inactive_vmid(struct lpfc_hba *phba);
static void lpfc_check_vmid_qfpa_issue(struct lpfc_hba *phba);

static int
lpfc_valid_xpt_node(struct lpfc_nodelist *ndlp)
{
	if (ndlp->nlp_fc4_type ||
	    ndlp->nlp_type & NLP_FABRIC)
		return 1;
	return 0;
}
 
static int
lpfc_rport_invalid(struct fc_rport *rport)
{
	struct lpfc_rport_data *rdata;
	struct lpfc_nodelist *ndlp;

	if (!rport) {
		pr_err("**** %s: NULL rport, exit.\n", __func__);
		return -EINVAL;
	}

	rdata = rport->dd_data;
	if (!rdata) {
		pr_err("**** %s: NULL dd_data on rport x%px SID x%x\n",
		       __func__, rport, rport->scsi_target_id);
		return -EINVAL;
	}

	ndlp = rdata->pnode;
	if (!rdata->pnode) {
		pr_info("**** %s: NULL ndlp on rport x%px SID x%x\n",
			__func__, rport, rport->scsi_target_id);
		return -EINVAL;
	}

	if (!ndlp->vport) {
		pr_err("**** %s: Null vport on ndlp x%px, DID x%x rport x%px "
		       "SID x%x\n", __func__, ndlp, ndlp->nlp_DID, rport,
		       rport->scsi_target_id);
		return -EINVAL;
	}
	return 0;
}

void
lpfc_terminate_rport_io(struct fc_rport *rport)
{
	struct lpfc_rport_data *rdata;
	struct lpfc_nodelist *ndlp;
	struct lpfc_vport *vport;

	if (lpfc_rport_invalid(rport))
		return;

	rdata = rport->dd_data;
	ndlp = rdata->pnode;
	vport = ndlp->vport;
	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_RPORT,
			      "rport terminate: sid:x%x did:x%x flg:x%x",
			      ndlp->nlp_sid, ndlp->nlp_DID, ndlp->nlp_flag);

	if (ndlp->nlp_sid != NLP_NO_SID)
		lpfc_sli_abort_iocb(vport, ndlp->nlp_sid, 0, LPFC_CTX_TGT);
}

 
void
lpfc_dev_loss_tmo_callbk(struct fc_rport *rport)
{
	struct lpfc_nodelist *ndlp;
	struct lpfc_vport *vport;
	struct lpfc_hba   *phba;
	struct lpfc_work_evt *evtp;
	unsigned long iflags;

	ndlp = ((struct lpfc_rport_data *)rport->dd_data)->pnode;
	if (!ndlp)
		return;

	vport = ndlp->vport;
	phba  = vport->phba;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_RPORT,
		"rport devlosscb: sid:x%x did:x%x flg:x%x",
		ndlp->nlp_sid, ndlp->nlp_DID, ndlp->nlp_flag);

	lpfc_printf_vlog(ndlp->vport, KERN_INFO, LOG_NODE,
			 "3181 dev_loss_callbk x%06x, rport x%px flg x%x "
			 "load_flag x%x refcnt %u state %d xpt x%x\n",
			 ndlp->nlp_DID, ndlp->rport, ndlp->nlp_flag,
			 vport->load_flag, kref_read(&ndlp->kref),
			 ndlp->nlp_state, ndlp->fc4_xpt_flags);

	 
	if (vport->load_flag & FC_UNLOADING) {
		spin_lock_irqsave(&ndlp->lock, iflags);
		ndlp->rport = NULL;

		 
		if (ndlp->fc4_xpt_flags & (NLP_XPT_REGD | SCSI_XPT_REGD)) {
			ndlp->fc4_xpt_flags &= ~SCSI_XPT_REGD;

			 
			if (!(ndlp->fc4_xpt_flags & NVME_XPT_REGD))
				ndlp->fc4_xpt_flags &= ~NLP_XPT_REGD;
			spin_unlock_irqrestore(&ndlp->lock, iflags);
			lpfc_nlp_put(ndlp);
			spin_lock_irqsave(&ndlp->lock, iflags);
		}

		 
		if (!(ndlp->fc4_xpt_flags & NVME_XPT_REGD) &&
		    !(ndlp->nlp_flag & NLP_DROPPED)) {
			ndlp->nlp_flag |= NLP_DROPPED;
			spin_unlock_irqrestore(&ndlp->lock, iflags);
			lpfc_nlp_put(ndlp);
			return;
		}

		spin_unlock_irqrestore(&ndlp->lock, iflags);
		return;
	}

	if (ndlp->nlp_state == NLP_STE_MAPPED_NODE)
		return;

	if (rport->port_name != wwn_to_u64(ndlp->nlp_portname.u.wwn))
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "6789 rport name %llx != node port name %llx",
				 rport->port_name,
				 wwn_to_u64(ndlp->nlp_portname.u.wwn));

	evtp = &ndlp->dev_loss_evt;

	if (!list_empty(&evtp->evt_listp)) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "6790 rport name %llx dev_loss_evt pending\n",
				 rport->port_name);
		return;
	}

	spin_lock_irqsave(&ndlp->lock, iflags);
	ndlp->nlp_flag |= NLP_IN_DEV_LOSS;

	 
	if (ndlp->nlp_state != NLP_STE_PLOGI_ISSUE)
		ndlp->nlp_flag &= ~NLP_NPR_2B_DISC;

	 
	ndlp->fc4_xpt_flags &= ~SCSI_XPT_REGD;
	((struct lpfc_rport_data *)rport->dd_data)->pnode = NULL;
	ndlp->rport = NULL;
	spin_unlock_irqrestore(&ndlp->lock, iflags);

	if (phba->worker_thread) {
		 
		evtp->evt_arg1 = lpfc_nlp_get(ndlp);

		spin_lock_irqsave(&phba->hbalock, iflags);
		if (evtp->evt_arg1) {
			evtp->evt = LPFC_EVT_DEV_LOSS;
			list_add_tail(&evtp->evt_listp, &phba->work_list);
			lpfc_worker_wake_up(phba);
		}
		spin_unlock_irqrestore(&phba->hbalock, iflags);
	} else {
		lpfc_printf_vlog(ndlp->vport, KERN_INFO, LOG_NODE,
				 "3188 worker thread is stopped %s x%06x, "
				 " rport x%px flg x%x load_flag x%x refcnt "
				 "%d\n", __func__, ndlp->nlp_DID,
				 ndlp->rport, ndlp->nlp_flag,
				 vport->load_flag, kref_read(&ndlp->kref));
		if (!(ndlp->fc4_xpt_flags & NVME_XPT_REGD)) {
			spin_lock_irqsave(&ndlp->lock, iflags);
			 
			ndlp->nlp_flag &= ~NLP_IN_DEV_LOSS;
			spin_unlock_irqrestore(&ndlp->lock, iflags);
			lpfc_disc_state_machine(vport, ndlp, NULL,
						NLP_EVT_DEVICE_RM);
		}

	}

	return;
}

 
static void lpfc_check_inactive_vmid_one(struct lpfc_vport *vport)
{
	u16 keep;
	u32 difftime = 0, r, bucket;
	u64 *lta;
	int cpu;
	struct lpfc_vmid *vmp;

	write_lock(&vport->vmid_lock);

	if (!vport->cur_vmid_cnt)
		goto out;

	 
	hash_for_each(vport->hash_table, bucket, vmp, hnode) {
		keep = 0;
		if (vmp->flag & LPFC_VMID_REGISTERED) {
			 
			 
			for_each_possible_cpu(cpu) {
				 
				lta = per_cpu_ptr(vmp->last_io_time, cpu);
				if (!lta)
					continue;
				difftime = (jiffies) - (*lta);
				if ((vport->vmid_inactivity_timeout *
				     JIFFIES_PER_HR) > difftime) {
					keep = 1;
					break;
				}
			}

			 
			 
			if (!keep) {
				 
				vmp->flag = LPFC_VMID_DE_REGISTER;
				write_unlock(&vport->vmid_lock);
				if (vport->vmid_priority_tagging)
					r = lpfc_vmid_uvem(vport, vmp, false);
				else
					r = lpfc_vmid_cmd(vport,
							  SLI_CTAS_DAPP_IDENT,
							  vmp);

				 
				 
				write_lock(&vport->vmid_lock);
				if (!r) {
					struct lpfc_vmid *ht = vmp;

					vport->cur_vmid_cnt--;
					ht->flag = LPFC_VMID_SLOT_FREE;
					free_percpu(ht->last_io_time);
					ht->last_io_time = NULL;
					hash_del(&ht->hnode);
				}
			}
		}
	}
 out:
	write_unlock(&vport->vmid_lock);
}

 

static void lpfc_check_inactive_vmid(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport;
	struct lpfc_vport **vports;
	int i;

	vports = lpfc_create_vport_work_array(phba);
	if (!vports)
		return;

	for (i = 0; i <= phba->max_vports; i++) {
		if ((!vports[i]) && (i == 0))
			vport = phba->pport;
		else
			vport = vports[i];
		if (!vport)
			break;

		lpfc_check_inactive_vmid_one(vport);
	}
	lpfc_destroy_vport_work_array(phba, vports);
}

 
void
lpfc_check_nlp_post_devloss(struct lpfc_vport *vport,
			    struct lpfc_nodelist *ndlp)
{
	unsigned long iflags;

	spin_lock_irqsave(&ndlp->lock, iflags);
	if (ndlp->save_flags & NLP_IN_RECOV_POST_DEV_LOSS) {
		ndlp->save_flags &= ~NLP_IN_RECOV_POST_DEV_LOSS;
		spin_unlock_irqrestore(&ndlp->lock, iflags);
		lpfc_nlp_get(ndlp);
		lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY | LOG_NODE,
				 "8438 Devloss timeout reversed on DID x%x "
				 "refcnt %d ndlp %p flag x%x "
				 "port_state = x%x\n",
				 ndlp->nlp_DID, kref_read(&ndlp->kref), ndlp,
				 ndlp->nlp_flag, vport->port_state);
		spin_lock_irqsave(&ndlp->lock, iflags);
	}
	spin_unlock_irqrestore(&ndlp->lock, iflags);
}

 
static int
lpfc_dev_loss_tmo_handler(struct lpfc_nodelist *ndlp)
{
	struct lpfc_vport *vport;
	struct lpfc_hba   *phba;
	uint8_t *name;
	int warn_on = 0;
	int fcf_inuse = 0;
	bool recovering = false;
	struct fc_vport *fc_vport = NULL;
	unsigned long iflags;

	vport = ndlp->vport;
	name = (uint8_t *)&ndlp->nlp_portname;
	phba = vport->phba;

	if (phba->sli_rev == LPFC_SLI_REV4)
		fcf_inuse = lpfc_fcf_inuse(phba);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_RPORT,
			      "rport devlosstmo:did:x%x type:x%x id:x%x",
			      ndlp->nlp_DID, ndlp->nlp_type, ndlp->nlp_sid);

	lpfc_printf_vlog(ndlp->vport, KERN_INFO, LOG_NODE,
			 "3182 %s x%06x, nflag x%x xflags x%x refcnt %d\n",
			 __func__, ndlp->nlp_DID, ndlp->nlp_flag,
			 ndlp->fc4_xpt_flags, kref_read(&ndlp->kref));

	 
	if (ndlp->nlp_state == NLP_STE_MAPPED_NODE) {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
				 "0284 Devloss timeout Ignored on "
				 "WWPN %x:%x:%x:%x:%x:%x:%x:%x "
				 "NPort x%x\n",
				 *name, *(name+1), *(name+2), *(name+3),
				 *(name+4), *(name+5), *(name+6), *(name+7),
				 ndlp->nlp_DID);

		spin_lock_irqsave(&ndlp->lock, iflags);
		ndlp->nlp_flag &= ~NLP_IN_DEV_LOSS;
		spin_unlock_irqrestore(&ndlp->lock, iflags);
		return fcf_inuse;
	}

	 
	if (ndlp->nlp_type & NLP_FABRIC) {
		spin_lock_irqsave(&ndlp->lock, iflags);

		 
		switch (ndlp->nlp_DID) {
		case Fabric_DID:
			fc_vport = vport->fc_vport;
			if (fc_vport) {
				 
				if (fc_vport->vport_state ==
				    FC_VPORT_INITIALIZING)
					recovering = true;
			} else {
				 
				if (phba->hba_flag & HBA_FLOGI_OUTSTANDING)
					recovering = true;
			}
			break;
		case Fabric_Cntl_DID:
			if (ndlp->nlp_flag & NLP_REG_LOGIN_SEND)
				recovering = true;
			break;
		case FDMI_DID:
			fallthrough;
		case NameServer_DID:
			if (ndlp->nlp_state >= NLP_STE_PLOGI_ISSUE &&
			    ndlp->nlp_state <= NLP_STE_REG_LOGIN_ISSUE)
				recovering = true;
			break;
		default:
			 
			if (ndlp->nlp_DID & Fabric_DID_MASK) {
				if (ndlp->nlp_state >= NLP_STE_PLOGI_ISSUE &&
				    ndlp->nlp_state <= NLP_STE_REG_LOGIN_ISSUE)
					recovering = true;
			}
			break;
		}
		spin_unlock_irqrestore(&ndlp->lock, iflags);

		 
		if (recovering) {
			lpfc_printf_vlog(vport, KERN_INFO,
					 LOG_DISCOVERY | LOG_NODE,
					 "8436 Devloss timeout marked on "
					 "DID x%x refcnt %d ndlp %p "
					 "flag x%x port_state = x%x\n",
					 ndlp->nlp_DID, kref_read(&ndlp->kref),
					 ndlp, ndlp->nlp_flag,
					 vport->port_state);
			spin_lock_irqsave(&ndlp->lock, iflags);
			ndlp->save_flags |= NLP_IN_RECOV_POST_DEV_LOSS;
			spin_unlock_irqrestore(&ndlp->lock, iflags);
		} else if (ndlp->nlp_state == NLP_STE_UNMAPPED_NODE) {
			 
			lpfc_printf_vlog(vport, KERN_INFO,
					 LOG_DISCOVERY | LOG_NODE,
					 "8437 Devloss timeout ignored on "
					 "DID x%x refcnt %d ndlp %p "
					 "flag x%x port_state = x%x\n",
					 ndlp->nlp_DID, kref_read(&ndlp->kref),
					 ndlp, ndlp->nlp_flag,
					 vport->port_state);
			return fcf_inuse;
		}

		spin_lock_irqsave(&ndlp->lock, iflags);
		ndlp->nlp_flag &= ~NLP_IN_DEV_LOSS;
		spin_unlock_irqrestore(&ndlp->lock, iflags);
		lpfc_nlp_put(ndlp);
		return fcf_inuse;
	}

	if (ndlp->nlp_sid != NLP_NO_SID) {
		warn_on = 1;
		lpfc_sli_abort_iocb(vport, ndlp->nlp_sid, 0, LPFC_CTX_TGT);
	}

	if (warn_on) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "0203 Devloss timeout on "
				 "WWPN %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x "
				 "NPort x%06x Data: x%x x%x x%x refcnt %d\n",
				 *name, *(name+1), *(name+2), *(name+3),
				 *(name+4), *(name+5), *(name+6), *(name+7),
				 ndlp->nlp_DID, ndlp->nlp_flag,
				 ndlp->nlp_state, ndlp->nlp_rpi,
				 kref_read(&ndlp->kref));
	} else {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_TRACE_EVENT,
				 "0204 Devloss timeout on "
				 "WWPN %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x "
				 "NPort x%06x Data: x%x x%x x%x\n",
				 *name, *(name+1), *(name+2), *(name+3),
				 *(name+4), *(name+5), *(name+6), *(name+7),
				 ndlp->nlp_DID, ndlp->nlp_flag,
				 ndlp->nlp_state, ndlp->nlp_rpi);
	}
	spin_lock_irqsave(&ndlp->lock, iflags);
	ndlp->nlp_flag &= ~NLP_IN_DEV_LOSS;
	spin_unlock_irqrestore(&ndlp->lock, iflags);

	 
	if (ndlp->nlp_state >= NLP_STE_PLOGI_ISSUE &&
	    ndlp->nlp_state <= NLP_STE_PRLI_ISSUE) {
		return fcf_inuse;
	}

	if (!(ndlp->fc4_xpt_flags & NVME_XPT_REGD))
		lpfc_disc_state_machine(vport, ndlp, NULL, NLP_EVT_DEVICE_RM);

	return fcf_inuse;
}

static void lpfc_check_vmid_qfpa_issue(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport;
	struct lpfc_vport **vports;
	int i;

	vports = lpfc_create_vport_work_array(phba);
	if (!vports)
		return;

	for (i = 0; i <= phba->max_vports; i++) {
		if ((!vports[i]) && (i == 0))
			vport = phba->pport;
		else
			vport = vports[i];
		if (!vport)
			break;

		if (vport->vmid_flag & LPFC_VMID_ISSUE_QFPA) {
			if (!lpfc_issue_els_qfpa(vport))
				vport->vmid_flag &= ~LPFC_VMID_ISSUE_QFPA;
		}
	}
	lpfc_destroy_vport_work_array(phba, vports);
}

 
static void
lpfc_sli4_post_dev_loss_tmo_handler(struct lpfc_hba *phba, int fcf_inuse,
				    uint32_t nlp_did)
{
	 
	if (!fcf_inuse)
		return;

	if ((phba->hba_flag & HBA_FIP_SUPPORT) && !lpfc_fcf_inuse(phba)) {
		spin_lock_irq(&phba->hbalock);
		if (phba->fcf.fcf_flag & FCF_DISCOVERY) {
			if (phba->hba_flag & HBA_DEVLOSS_TMO) {
				spin_unlock_irq(&phba->hbalock);
				return;
			}
			phba->hba_flag |= HBA_DEVLOSS_TMO;
			lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
					"2847 Last remote node (x%x) using "
					"FCF devloss tmo\n", nlp_did);
		}
		if (phba->fcf.fcf_flag & FCF_REDISC_PROG) {
			spin_unlock_irq(&phba->hbalock);
			lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
					"2868 Devloss tmo to FCF rediscovery "
					"in progress\n");
			return;
		}
		if (!(phba->hba_flag & (FCF_TS_INPROG | FCF_RR_INPROG))) {
			spin_unlock_irq(&phba->hbalock);
			lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
					"2869 Devloss tmo to idle FIP engine, "
					"unreg in-use FCF and rescan.\n");
			 
			lpfc_unregister_fcf_rescan(phba);
			return;
		}
		spin_unlock_irq(&phba->hbalock);
		if (phba->hba_flag & FCF_TS_INPROG)
			lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
					"2870 FCF table scan in progress\n");
		if (phba->hba_flag & FCF_RR_INPROG)
			lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
					"2871 FLOGI roundrobin FCF failover "
					"in progress\n");
	}
	lpfc_unregister_unused_fcf(phba);
}

 
struct lpfc_fast_path_event *
lpfc_alloc_fast_evt(struct lpfc_hba *phba) {
	struct lpfc_fast_path_event *ret;

	 
	if (atomic_read(&phba->fast_event_count) > LPFC_MAX_EVT_COUNT)
		return NULL;

	ret = kzalloc(sizeof(struct lpfc_fast_path_event),
			GFP_ATOMIC);
	if (ret) {
		atomic_inc(&phba->fast_event_count);
		INIT_LIST_HEAD(&ret->work_evt.evt_listp);
		ret->work_evt.evt = LPFC_EVT_FASTPATH_MGMT_EVT;
	}
	return ret;
}

 
void
lpfc_free_fast_evt(struct lpfc_hba *phba,
		struct lpfc_fast_path_event *evt) {

	atomic_dec(&phba->fast_event_count);
	kfree(evt);
}

 
static void
lpfc_send_fastpath_evt(struct lpfc_hba *phba,
		struct lpfc_work_evt *evtp)
{
	unsigned long evt_category, evt_sub_category;
	struct lpfc_fast_path_event *fast_evt_data;
	char *evt_data;
	uint32_t evt_data_size;
	struct Scsi_Host *shost;

	fast_evt_data = container_of(evtp, struct lpfc_fast_path_event,
		work_evt);

	evt_category = (unsigned long) fast_evt_data->un.fabric_evt.event_type;
	evt_sub_category = (unsigned long) fast_evt_data->un.
			fabric_evt.subcategory;
	shost = lpfc_shost_from_vport(fast_evt_data->vport);
	if (evt_category == FC_REG_FABRIC_EVENT) {
		if (evt_sub_category == LPFC_EVENT_FCPRDCHKERR) {
			evt_data = (char *) &fast_evt_data->un.read_check_error;
			evt_data_size = sizeof(fast_evt_data->un.
				read_check_error);
		} else if ((evt_sub_category == LPFC_EVENT_FABRIC_BUSY) ||
			(evt_sub_category == LPFC_EVENT_PORT_BUSY)) {
			evt_data = (char *) &fast_evt_data->un.fabric_evt;
			evt_data_size = sizeof(fast_evt_data->un.fabric_evt);
		} else {
			lpfc_free_fast_evt(phba, fast_evt_data);
			return;
		}
	} else if (evt_category == FC_REG_SCSI_EVENT) {
		switch (evt_sub_category) {
		case LPFC_EVENT_QFULL:
		case LPFC_EVENT_DEVBSY:
			evt_data = (char *) &fast_evt_data->un.scsi_evt;
			evt_data_size = sizeof(fast_evt_data->un.scsi_evt);
			break;
		case LPFC_EVENT_CHECK_COND:
			evt_data = (char *) &fast_evt_data->un.check_cond_evt;
			evt_data_size =  sizeof(fast_evt_data->un.
				check_cond_evt);
			break;
		case LPFC_EVENT_VARQUEDEPTH:
			evt_data = (char *) &fast_evt_data->un.queue_depth_evt;
			evt_data_size = sizeof(fast_evt_data->un.
				queue_depth_evt);
			break;
		default:
			lpfc_free_fast_evt(phba, fast_evt_data);
			return;
		}
	} else {
		lpfc_free_fast_evt(phba, fast_evt_data);
		return;
	}

	if (phba->cfg_enable_fc4_type != LPFC_ENABLE_NVME)
		fc_host_post_vendor_event(shost,
			fc_get_event_number(),
			evt_data_size,
			evt_data,
			LPFC_NL_VENDOR_ID);

	lpfc_free_fast_evt(phba, fast_evt_data);
	return;
}

static void
lpfc_work_list_done(struct lpfc_hba *phba)
{
	struct lpfc_work_evt  *evtp = NULL;
	struct lpfc_nodelist  *ndlp;
	int free_evt;
	int fcf_inuse;
	uint32_t nlp_did;
	bool hba_pci_err;

	spin_lock_irq(&phba->hbalock);
	while (!list_empty(&phba->work_list)) {
		list_remove_head((&phba->work_list), evtp, typeof(*evtp),
				 evt_listp);
		spin_unlock_irq(&phba->hbalock);
		hba_pci_err = test_bit(HBA_PCI_ERR, &phba->bit_flags);
		free_evt = 1;
		switch (evtp->evt) {
		case LPFC_EVT_ELS_RETRY:
			ndlp = (struct lpfc_nodelist *) (evtp->evt_arg1);
			if (!hba_pci_err) {
				lpfc_els_retry_delay_handler(ndlp);
				free_evt = 0;  
			}
			 
			lpfc_nlp_put(ndlp);
			break;
		case LPFC_EVT_DEV_LOSS:
			ndlp = (struct lpfc_nodelist *)(evtp->evt_arg1);
			fcf_inuse = lpfc_dev_loss_tmo_handler(ndlp);
			free_evt = 0;
			 
			nlp_did = ndlp->nlp_DID;
			lpfc_nlp_put(ndlp);
			if (phba->sli_rev == LPFC_SLI_REV4)
				lpfc_sli4_post_dev_loss_tmo_handler(phba,
								    fcf_inuse,
								    nlp_did);
			break;
		case LPFC_EVT_RECOVER_PORT:
			ndlp = (struct lpfc_nodelist *)(evtp->evt_arg1);
			if (!hba_pci_err) {
				lpfc_sli_abts_recover_port(ndlp->vport, ndlp);
				free_evt = 0;
			}
			 
			lpfc_nlp_put(ndlp);
			break;
		case LPFC_EVT_ONLINE:
			if (phba->link_state < LPFC_LINK_DOWN)
				*(int *) (evtp->evt_arg1) = lpfc_online(phba);
			else
				*(int *) (evtp->evt_arg1) = 0;
			complete((struct completion *)(evtp->evt_arg2));
			break;
		case LPFC_EVT_OFFLINE_PREP:
			if (phba->link_state >= LPFC_LINK_DOWN)
				lpfc_offline_prep(phba, LPFC_MBX_WAIT);
			*(int *)(evtp->evt_arg1) = 0;
			complete((struct completion *)(evtp->evt_arg2));
			break;
		case LPFC_EVT_OFFLINE:
			lpfc_offline(phba);
			lpfc_sli_brdrestart(phba);
			*(int *)(evtp->evt_arg1) =
				lpfc_sli_brdready(phba, HS_FFRDY | HS_MBRDY);
			lpfc_unblock_mgmt_io(phba);
			complete((struct completion *)(evtp->evt_arg2));
			break;
		case LPFC_EVT_WARM_START:
			lpfc_offline(phba);
			lpfc_reset_barrier(phba);
			lpfc_sli_brdreset(phba);
			lpfc_hba_down_post(phba);
			*(int *)(evtp->evt_arg1) =
				lpfc_sli_brdready(phba, HS_MBRDY);
			lpfc_unblock_mgmt_io(phba);
			complete((struct completion *)(evtp->evt_arg2));
			break;
		case LPFC_EVT_KILL:
			lpfc_offline(phba);
			*(int *)(evtp->evt_arg1)
				= (phba->pport->stopped)
				        ? 0 : lpfc_sli_brdkill(phba);
			lpfc_unblock_mgmt_io(phba);
			complete((struct completion *)(evtp->evt_arg2));
			break;
		case LPFC_EVT_FASTPATH_MGMT_EVT:
			lpfc_send_fastpath_evt(phba, evtp);
			free_evt = 0;
			break;
		case LPFC_EVT_RESET_HBA:
			if (!(phba->pport->load_flag & FC_UNLOADING))
				lpfc_reset_hba(phba);
			break;
		}
		if (free_evt)
			kfree(evtp);
		spin_lock_irq(&phba->hbalock);
	}
	spin_unlock_irq(&phba->hbalock);

}

static void
lpfc_work_done(struct lpfc_hba *phba)
{
	struct lpfc_sli_ring *pring;
	uint32_t ha_copy, status, control, work_port_events;
	struct lpfc_vport **vports;
	struct lpfc_vport *vport;
	int i;
	bool hba_pci_err;

	hba_pci_err = test_bit(HBA_PCI_ERR, &phba->bit_flags);
	spin_lock_irq(&phba->hbalock);
	ha_copy = phba->work_ha;
	phba->work_ha = 0;
	spin_unlock_irq(&phba->hbalock);
	if (hba_pci_err)
		ha_copy = 0;

	 
	if (phba->pci_dev_grp == LPFC_PCI_DEV_OC && !hba_pci_err)
		lpfc_sli4_post_async_mbox(phba);

	if (ha_copy & HA_ERATT) {
		 
		lpfc_handle_eratt(phba);

		if (phba->fw_dump_cmpl) {
			complete(phba->fw_dump_cmpl);
			phba->fw_dump_cmpl = NULL;
		}
	}

	if (ha_copy & HA_MBATT)
		lpfc_sli_handle_mb_event(phba);

	if (ha_copy & HA_LATT)
		lpfc_handle_latt(phba);

	 
	if (lpfc_is_vmid_enabled(phba) && !hba_pci_err) {
		if (phba->pport->work_port_events &
		    WORKER_CHECK_VMID_ISSUE_QFPA) {
			lpfc_check_vmid_qfpa_issue(phba);
			phba->pport->work_port_events &=
				~WORKER_CHECK_VMID_ISSUE_QFPA;
		}
		if (phba->pport->work_port_events &
		    WORKER_CHECK_INACTIVE_VMID) {
			lpfc_check_inactive_vmid(phba);
			phba->pport->work_port_events &=
			    ~WORKER_CHECK_INACTIVE_VMID;
		}
	}

	 
	if (phba->pci_dev_grp == LPFC_PCI_DEV_OC) {
		if (phba->hba_flag & HBA_RRQ_ACTIVE)
			lpfc_handle_rrq_active(phba);
		if (phba->hba_flag & ELS_XRI_ABORT_EVENT)
			lpfc_sli4_els_xri_abort_event_proc(phba);
		if (phba->hba_flag & ASYNC_EVENT)
			lpfc_sli4_async_event_proc(phba);
		if (phba->hba_flag & HBA_POST_RECEIVE_BUFFER) {
			spin_lock_irq(&phba->hbalock);
			phba->hba_flag &= ~HBA_POST_RECEIVE_BUFFER;
			spin_unlock_irq(&phba->hbalock);
			lpfc_sli_hbqbuf_add_hbqs(phba, LPFC_ELS_HBQ);
		}
		if (phba->fcf.fcf_flag & FCF_REDISC_EVT)
			lpfc_sli4_fcf_redisc_event_proc(phba);
	}

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports; i++) {
			 
			if (vports[i] == NULL && i == 0)
				vport = phba->pport;
			else
				vport = vports[i];
			if (vport == NULL)
				break;
			spin_lock_irq(&vport->work_port_lock);
			work_port_events = vport->work_port_events;
			vport->work_port_events &= ~work_port_events;
			spin_unlock_irq(&vport->work_port_lock);
			if (hba_pci_err)
				continue;
			if (work_port_events & WORKER_DISC_TMO)
				lpfc_disc_timeout_handler(vport);
			if (work_port_events & WORKER_ELS_TMO)
				lpfc_els_timeout_handler(vport);
			if (work_port_events & WORKER_HB_TMO)
				lpfc_hb_timeout_handler(phba);
			if (work_port_events & WORKER_MBOX_TMO)
				lpfc_mbox_timeout_handler(phba);
			if (work_port_events & WORKER_FABRIC_BLOCK_TMO)
				lpfc_unblock_fabric_iocbs(phba);
			if (work_port_events & WORKER_RAMP_DOWN_QUEUE)
				lpfc_ramp_down_queue_handler(phba);
			if (work_port_events & WORKER_DELAYED_DISC_TMO)
				lpfc_delayed_disc_timeout_handler(vport);
		}
	lpfc_destroy_vport_work_array(phba, vports);

	pring = lpfc_phba_elsring(phba);
	status = (ha_copy & (HA_RXMASK  << (4*LPFC_ELS_RING)));
	status >>= (4*LPFC_ELS_RING);
	if (pring && (status & HA_RXMASK ||
		      pring->flag & LPFC_DEFERRED_RING_EVENT ||
		      phba->hba_flag & HBA_SP_QUEUE_EVT)) {
		if (pring->flag & LPFC_STOP_IOCB_EVENT) {
			pring->flag |= LPFC_DEFERRED_RING_EVENT;
			 
			if (!(phba->hba_flag & HBA_SP_QUEUE_EVT))
				set_bit(LPFC_DATA_READY, &phba->data_flags);
		} else {
			 
			if (phba->link_state >= LPFC_LINK_DOWN ||
			    phba->link_flag & LS_MDS_LOOPBACK) {
				pring->flag &= ~LPFC_DEFERRED_RING_EVENT;
				lpfc_sli_handle_slow_ring_event(phba, pring,
								(status &
								HA_RXMASK));
			}
		}
		if (phba->sli_rev == LPFC_SLI_REV4)
			lpfc_drain_txq(phba);
		 
		if (phba->sli_rev <= LPFC_SLI_REV3) {
			spin_lock_irq(&phba->hbalock);
			control = readl(phba->HCregaddr);
			if (!(control & (HC_R0INT_ENA << LPFC_ELS_RING))) {
				lpfc_debugfs_slow_ring_trc(phba,
					"WRK Enable ring: cntl:x%x hacopy:x%x",
					control, ha_copy, 0);

				control |= (HC_R0INT_ENA << LPFC_ELS_RING);
				writel(control, phba->HCregaddr);
				readl(phba->HCregaddr);  
			} else {
				lpfc_debugfs_slow_ring_trc(phba,
					"WRK Ring ok:     cntl:x%x hacopy:x%x",
					control, ha_copy, 0);
			}
			spin_unlock_irq(&phba->hbalock);
		}
	}
	lpfc_work_list_done(phba);
}

int
lpfc_do_work(void *p)
{
	struct lpfc_hba *phba = p;
	int rc;

	set_user_nice(current, MIN_NICE);
	current->flags |= PF_NOFREEZE;
	phba->data_flags = 0;

	while (!kthread_should_stop()) {
		 
		rc = wait_event_interruptible(phba->work_waitq,
					(test_and_clear_bit(LPFC_DATA_READY,
							    &phba->data_flags)
					 || kthread_should_stop()));
		 
		if (rc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0433 Wakeup on signal: rc=x%x\n", rc);
			break;
		}

		 
		lpfc_work_done(phba);
	}
	phba->worker_thread = NULL;
	lpfc_printf_log(phba, KERN_INFO, LOG_ELS,
			"0432 Worker thread stopped.\n");
	return 0;
}

 
int
lpfc_workq_post_event(struct lpfc_hba *phba, void *arg1, void *arg2,
		      uint32_t evt)
{
	struct lpfc_work_evt  *evtp;
	unsigned long flags;

	 
	evtp = kmalloc(sizeof(struct lpfc_work_evt), GFP_ATOMIC);
	if (!evtp)
		return 0;

	evtp->evt_arg1  = arg1;
	evtp->evt_arg2  = arg2;
	evtp->evt       = evt;

	spin_lock_irqsave(&phba->hbalock, flags);
	list_add_tail(&evtp->evt_listp, &phba->work_list);
	spin_unlock_irqrestore(&phba->hbalock, flags);

	lpfc_worker_wake_up(phba);

	return 1;
}

void
lpfc_cleanup_rpis(struct lpfc_vport *vport, int remove)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_nodelist *ndlp, *next_ndlp;

	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {
		if ((phba->sli3_options & LPFC_SLI3_VPORT_TEARDOWN) ||
		    ((vport->port_type == LPFC_NPIV_PORT) &&
		     ((ndlp->nlp_DID == NameServer_DID) ||
		      (ndlp->nlp_DID == FDMI_DID) ||
		      (ndlp->nlp_DID == Fabric_Cntl_DID))))
			lpfc_unreg_rpi(vport, ndlp);

		 
		if ((phba->sli_rev < LPFC_SLI_REV4) &&
		    (!remove && ndlp->nlp_type & NLP_FABRIC))
			continue;

		 
		if (phba->nvmet_support &&
		    ndlp->nlp_state == NLP_STE_UNMAPPED_NODE)
			lpfc_nvmet_invalidate_host(phba, ndlp);

		lpfc_disc_state_machine(vport, ndlp, NULL,
					remove
					? NLP_EVT_DEVICE_RM
					: NLP_EVT_DEVICE_RECOVERY);
	}
	if (phba->sli3_options & LPFC_SLI3_VPORT_TEARDOWN) {
		if (phba->sli_rev == LPFC_SLI_REV4)
			lpfc_sli4_unreg_all_rpis(vport);
		lpfc_mbx_unreg_vpi(vport);
		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
		spin_unlock_irq(shost->host_lock);
	}
}

void
lpfc_port_link_failure(struct lpfc_vport *vport)
{
	lpfc_vport_set_state(vport, FC_VPORT_LINKDOWN);

	 
	lpfc_cleanup_rcv_buffers(vport);

	 
	lpfc_els_flush_rscn(vport);

	 
	lpfc_els_flush_cmd(vport);

	lpfc_cleanup_rpis(vport, 0);

	 
	lpfc_can_disctmo(vport);
}

void
lpfc_linkdown_port(struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);

	if (vport->cfg_enable_fc4_type != LPFC_ENABLE_NVME)
		fc_host_post_event(shost, fc_get_event_number(),
				   FCH_EVT_LINKDOWN, 0);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Link Down:       state:x%x rtry:x%x flg:x%x",
		vport->port_state, vport->fc_ns_retry, vport->fc_flag);

	lpfc_port_link_failure(vport);

	 
	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~FC_DISC_DELAYED;
	spin_unlock_irq(shost->host_lock);
	del_timer_sync(&vport->delayed_disc_tmo);

	if (phba->sli_rev == LPFC_SLI_REV4 &&
	    vport->port_type == LPFC_PHYSICAL_PORT &&
	    phba->sli4_hba.fawwpn_flag & LPFC_FAWWPN_CONFIG) {
		 
		phba->sli4_hba.fawwpn_flag |= LPFC_FAWWPN_FABRIC;
	}
}

int
lpfc_linkdown(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	struct lpfc_vport **vports;
	LPFC_MBOXQ_t          *mb;
	int i;
	int offline;

	if (phba->link_state == LPFC_LINK_DOWN)
		return 0;

	 
	lpfc_scsi_dev_block(phba);
	offline = pci_channel_offline(phba->pcidev);

	phba->defer_flogi_acc_flag = false;

	 
	phba->link_flag &= ~LS_EXTERNAL_LOOPBACK;

	spin_lock_irq(&phba->hbalock);
	phba->fcf.fcf_flag &= ~(FCF_AVAILABLE | FCF_SCAN_DONE);
	spin_unlock_irq(&phba->hbalock);
	if (phba->link_state > LPFC_LINK_DOWN) {
		phba->link_state = LPFC_LINK_DOWN;
		if (phba->sli4_hba.conf_trunk) {
			phba->trunk_link.link0.state = 0;
			phba->trunk_link.link1.state = 0;
			phba->trunk_link.link2.state = 0;
			phba->trunk_link.link3.state = 0;
			phba->trunk_link.phy_lnk_speed =
						LPFC_LINK_SPEED_UNKNOWN;
			phba->sli4_hba.link_state.logical_speed =
						LPFC_LINK_SPEED_UNKNOWN;
		}
		spin_lock_irq(shost->host_lock);
		phba->pport->fc_flag &= ~FC_LBIT;
		spin_unlock_irq(shost->host_lock);
	}
	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL) {
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			 
			lpfc_linkdown_port(vports[i]);

			vports[i]->fc_myDID = 0;

			if ((vport->cfg_enable_fc4_type == LPFC_ENABLE_BOTH) ||
			    (vport->cfg_enable_fc4_type == LPFC_ENABLE_NVME)) {
				if (phba->nvmet_support)
					lpfc_nvmet_update_targetport(phba);
				else
					lpfc_nvme_update_localport(vports[i]);
			}
		}
	}
	lpfc_destroy_vport_work_array(phba, vports);

	 
	if (phba->sli_rev > LPFC_SLI_REV3 || offline)
		goto skip_unreg_did;

	mb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (mb) {
		lpfc_unreg_did(phba, 0xffff, LPFC_UNREG_ALL_DFLT_RPIS, mb);
		mb->vport = vport;
		mb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		if (lpfc_sli_issue_mbox(phba, mb, MBX_NOWAIT)
		    == MBX_NOT_FINISHED) {
			mempool_free(mb, phba->mbox_mem_pool);
		}
	}

 skip_unreg_did:
	 
	if (phba->pport->fc_flag & FC_PT2PT) {
		mb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
		if (mb) {
			lpfc_config_link(phba, mb);
			mb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
			mb->vport = vport;
			if (lpfc_sli_issue_mbox(phba, mb, MBX_NOWAIT)
			    == MBX_NOT_FINISHED) {
				mempool_free(mb, phba->mbox_mem_pool);
			}
		}
		spin_lock_irq(shost->host_lock);
		phba->pport->fc_flag &= ~(FC_PT2PT | FC_PT2PT_PLOGI);
		phba->pport->rcv_flogi_cnt = 0;
		spin_unlock_irq(shost->host_lock);
	}
	return 0;
}

static void
lpfc_linkup_cleanup_nodes(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp;

	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
		ndlp->nlp_fc4_type &= ~(NLP_FC4_FCP | NLP_FC4_NVME);

		if (ndlp->nlp_state == NLP_STE_UNUSED_NODE)
			continue;
		if (ndlp->nlp_type & NLP_FABRIC) {
			 
			if (ndlp->nlp_DID != Fabric_DID)
				lpfc_unreg_rpi(vport, ndlp);
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		} else if (!(ndlp->nlp_flag & NLP_NPR_ADISC)) {
			 
			lpfc_unreg_rpi(vport, ndlp);
		}
	}
}

static void
lpfc_linkup_port(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;

	if ((vport->load_flag & FC_UNLOADING) != 0)
		return;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Link Up:         top:x%x speed:x%x flg:x%x",
		phba->fc_topology, phba->fc_linkspeed, phba->link_flag);

	 
	if (!(phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
		(vport != phba->pport))
		return;

	if (vport->cfg_enable_fc4_type != LPFC_ENABLE_NVME)
		fc_host_post_event(shost, fc_get_event_number(),
				   FCH_EVT_LINKUP, 0);

	spin_lock_irq(shost->host_lock);
	if (phba->defer_flogi_acc_flag)
		vport->fc_flag &= ~(FC_ABORT_DISCOVERY | FC_RSCN_MODE |
				    FC_NLP_MORE | FC_RSCN_DISCOVERY);
	else
		vport->fc_flag &= ~(FC_PT2PT | FC_PT2PT_PLOGI |
				    FC_ABORT_DISCOVERY | FC_RSCN_MODE |
				    FC_NLP_MORE | FC_RSCN_DISCOVERY);
	vport->fc_flag |= FC_NDISC_ACTIVE;
	vport->fc_ns_retry = 0;
	spin_unlock_irq(shost->host_lock);
	lpfc_setup_fdmi_mask(vport);

	lpfc_linkup_cleanup_nodes(vport);
}

static int
lpfc_linkup(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	int i;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(phba->pport);

	phba->link_state = LPFC_LINK_UP;

	 
	clear_bit(FABRIC_COMANDS_BLOCKED, &phba->bit_flags);
	del_timer_sync(&phba->fabric_block_timer);

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++)
			lpfc_linkup_port(vports[i]);
	lpfc_destroy_vport_work_array(phba, vports);

	 
	spin_lock_irq(shost->host_lock);
	phba->pport->rcv_flogi_cnt = 0;
	spin_unlock_irq(shost->host_lock);

	 
	phba->hba_flag &= ~(HBA_FLOGI_ISSUED | HBA_RHBA_CMPL);

	return 0;
}

 
static void
lpfc_mbx_cmpl_clear_la(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport *vport = pmb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	struct lpfc_sli   *psli = &phba->sli;
	MAILBOX_t *mb = &pmb->u.mb;
	uint32_t control;

	 
	psli->sli3_ring[LPFC_EXTRA_RING].flag &= ~LPFC_STOP_IOCB_EVENT;
	psli->sli3_ring[LPFC_FCP_RING].flag &= ~LPFC_STOP_IOCB_EVENT;

	 
	if ((mb->mbxStatus) && (mb->mbxStatus != 0x1601)) {
		 
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "0320 CLEAR_LA mbxStatus error x%x hba "
				 "state x%x\n",
				 mb->mbxStatus, vport->port_state);
		phba->link_state = LPFC_HBA_ERROR;
		goto out;
	}

	if (vport->port_type == LPFC_PHYSICAL_PORT)
		phba->link_state = LPFC_HBA_READY;

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag |= LPFC_PROCESS_LA;
	control = readl(phba->HCregaddr);
	control |= HC_LAINT_ENA;
	writel(control, phba->HCregaddr);
	readl(phba->HCregaddr);  
	spin_unlock_irq(&phba->hbalock);
	mempool_free(pmb, phba->mbox_mem_pool);
	return;

out:
	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0225 Device Discovery completes\n");
	mempool_free(pmb, phba->mbox_mem_pool);

	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~FC_ABORT_DISCOVERY;
	spin_unlock_irq(shost->host_lock);

	lpfc_can_disctmo(vport);

	 

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag |= LPFC_PROCESS_LA;
	control = readl(phba->HCregaddr);
	control |= HC_LAINT_ENA;
	writel(control, phba->HCregaddr);
	readl(phba->HCregaddr);  
	spin_unlock_irq(&phba->hbalock);

	return;
}

void
lpfc_mbx_cmpl_local_config_link(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport *vport = pmb->vport;
	LPFC_MBOXQ_t *sparam_mb;
	u16 status = pmb->u.mb.mbxStatus;
	int rc;

	mempool_free(pmb, phba->mbox_mem_pool);

	if (status)
		goto out;

	 
	if ((phba->sli_rev == LPFC_SLI_REV4) &&
	    !(phba->hba_flag & HBA_FCOE_MODE) &&
	    (phba->link_flag & LS_LOOPBACK_MODE))
		return;

	if (phba->fc_topology == LPFC_TOPOLOGY_LOOP &&
	    vport->fc_flag & FC_PUBLIC_LOOP &&
	    !(vport->fc_flag & FC_LBIT)) {
			 
			lpfc_set_disctmo(vport);
			return;
	}

	 
	if (vport->port_state != LPFC_FLOGI) {
		 
		if (phba->bbcredit_support && phba->cfg_enable_bbcr &&
		    !(phba->link_flag & LS_LOOPBACK_MODE)) {
			sparam_mb = mempool_alloc(phba->mbox_mem_pool,
						  GFP_KERNEL);
			if (!sparam_mb)
				goto sparam_out;

			rc = lpfc_read_sparam(phba, sparam_mb, 0);
			if (rc) {
				mempool_free(sparam_mb, phba->mbox_mem_pool);
				goto sparam_out;
			}
			sparam_mb->vport = vport;
			sparam_mb->mbox_cmpl = lpfc_mbx_cmpl_read_sparam;
			rc = lpfc_sli_issue_mbox(phba, sparam_mb, MBX_NOWAIT);
			if (rc == MBX_NOT_FINISHED) {
				lpfc_mbox_rsrc_cleanup(phba, sparam_mb,
						       MBOX_THD_UNLOCKED);
				goto sparam_out;
			}

			phba->hba_flag |= HBA_DEFER_FLOGI;
		}  else {
			lpfc_initial_flogi(vport);
		}
	} else {
		if (vport->fc_flag & FC_PT2PT)
			lpfc_disc_start(vport);
	}
	return;

out:
	lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
			 "0306 CONFIG_LINK mbxStatus error x%x HBA state x%x\n",
			 status, vport->port_state);

sparam_out:
	lpfc_linkdown(phba);

	lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
			 "0200 CONFIG_LINK bad hba state x%x\n",
			 vport->port_state);

	lpfc_issue_clear_la(phba, vport);
	return;
}

 
void
lpfc_sli4_clear_fcf_rr_bmask(struct lpfc_hba *phba)
{
	struct lpfc_fcf_pri *fcf_pri;
	struct lpfc_fcf_pri *next_fcf_pri;
	memset(phba->fcf.fcf_rr_bmask, 0, sizeof(*phba->fcf.fcf_rr_bmask));
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(fcf_pri, next_fcf_pri,
				&phba->fcf.fcf_pri_list, list) {
		list_del_init(&fcf_pri->list);
		fcf_pri->fcf_rec.flag = 0;
	}
	spin_unlock_irq(&phba->hbalock);
}
static void
lpfc_mbx_cmpl_reg_fcfi(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	struct lpfc_vport *vport = mboxq->vport;

	if (mboxq->u.mb.mbxStatus) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "2017 REG_FCFI mbxStatus error x%x "
				 "HBA state x%x\n", mboxq->u.mb.mbxStatus,
				 vport->port_state);
		goto fail_out;
	}

	 
	phba->fcf.fcfi = bf_get(lpfc_reg_fcfi_fcfi, &mboxq->u.mqe.un.reg_fcfi);
	 
	spin_lock_irq(&phba->hbalock);
	phba->fcf.fcf_flag |= FCF_REGISTERED;
	spin_unlock_irq(&phba->hbalock);

	 
	if ((!(phba->hba_flag & FCF_RR_INPROG)) &&
		lpfc_check_pending_fcoe_event(phba, LPFC_UNREG_FCF))
		goto fail_out;

	 
	spin_lock_irq(&phba->hbalock);
	phba->fcf.fcf_flag |= (FCF_SCAN_DONE | FCF_IN_USE);
	phba->hba_flag &= ~FCF_TS_INPROG;
	if (vport->port_state != LPFC_FLOGI) {
		phba->hba_flag |= FCF_RR_INPROG;
		spin_unlock_irq(&phba->hbalock);
		lpfc_issue_init_vfi(vport);
		goto out;
	}
	spin_unlock_irq(&phba->hbalock);
	goto out;

fail_out:
	spin_lock_irq(&phba->hbalock);
	phba->hba_flag &= ~FCF_RR_INPROG;
	spin_unlock_irq(&phba->hbalock);
out:
	mempool_free(mboxq, phba->mbox_mem_pool);
}

 
static uint32_t
lpfc_fab_name_match(uint8_t *fab_name, struct fcf_record *new_fcf_record)
{
	if (fab_name[0] != bf_get(lpfc_fcf_record_fab_name_0, new_fcf_record))
		return 0;
	if (fab_name[1] != bf_get(lpfc_fcf_record_fab_name_1, new_fcf_record))
		return 0;
	if (fab_name[2] != bf_get(lpfc_fcf_record_fab_name_2, new_fcf_record))
		return 0;
	if (fab_name[3] != bf_get(lpfc_fcf_record_fab_name_3, new_fcf_record))
		return 0;
	if (fab_name[4] != bf_get(lpfc_fcf_record_fab_name_4, new_fcf_record))
		return 0;
	if (fab_name[5] != bf_get(lpfc_fcf_record_fab_name_5, new_fcf_record))
		return 0;
	if (fab_name[6] != bf_get(lpfc_fcf_record_fab_name_6, new_fcf_record))
		return 0;
	if (fab_name[7] != bf_get(lpfc_fcf_record_fab_name_7, new_fcf_record))
		return 0;
	return 1;
}

 
static uint32_t
lpfc_sw_name_match(uint8_t *sw_name, struct fcf_record *new_fcf_record)
{
	if (sw_name[0] != bf_get(lpfc_fcf_record_switch_name_0, new_fcf_record))
		return 0;
	if (sw_name[1] != bf_get(lpfc_fcf_record_switch_name_1, new_fcf_record))
		return 0;
	if (sw_name[2] != bf_get(lpfc_fcf_record_switch_name_2, new_fcf_record))
		return 0;
	if (sw_name[3] != bf_get(lpfc_fcf_record_switch_name_3, new_fcf_record))
		return 0;
	if (sw_name[4] != bf_get(lpfc_fcf_record_switch_name_4, new_fcf_record))
		return 0;
	if (sw_name[5] != bf_get(lpfc_fcf_record_switch_name_5, new_fcf_record))
		return 0;
	if (sw_name[6] != bf_get(lpfc_fcf_record_switch_name_6, new_fcf_record))
		return 0;
	if (sw_name[7] != bf_get(lpfc_fcf_record_switch_name_7, new_fcf_record))
		return 0;
	return 1;
}

 
static uint32_t
lpfc_mac_addr_match(uint8_t *mac_addr, struct fcf_record *new_fcf_record)
{
	if (mac_addr[0] != bf_get(lpfc_fcf_record_mac_0, new_fcf_record))
		return 0;
	if (mac_addr[1] != bf_get(lpfc_fcf_record_mac_1, new_fcf_record))
		return 0;
	if (mac_addr[2] != bf_get(lpfc_fcf_record_mac_2, new_fcf_record))
		return 0;
	if (mac_addr[3] != bf_get(lpfc_fcf_record_mac_3, new_fcf_record))
		return 0;
	if (mac_addr[4] != bf_get(lpfc_fcf_record_mac_4, new_fcf_record))
		return 0;
	if (mac_addr[5] != bf_get(lpfc_fcf_record_mac_5, new_fcf_record))
		return 0;
	return 1;
}

static bool
lpfc_vlan_id_match(uint16_t curr_vlan_id, uint16_t new_vlan_id)
{
	return (curr_vlan_id == new_vlan_id);
}

 
static void
__lpfc_update_fcf_record_pri(struct lpfc_hba *phba, uint16_t fcf_index,
				 struct fcf_record *new_fcf_record
				 )
{
	struct lpfc_fcf_pri *fcf_pri;

	fcf_pri = &phba->fcf.fcf_pri[fcf_index];
	fcf_pri->fcf_rec.fcf_index = fcf_index;
	 
	fcf_pri->fcf_rec.priority = new_fcf_record->fip_priority;

}

 
static void
lpfc_copy_fcf_record(struct lpfc_fcf_rec *fcf_rec,
		     struct fcf_record *new_fcf_record)
{
	 
	fcf_rec->fabric_name[0] =
		bf_get(lpfc_fcf_record_fab_name_0, new_fcf_record);
	fcf_rec->fabric_name[1] =
		bf_get(lpfc_fcf_record_fab_name_1, new_fcf_record);
	fcf_rec->fabric_name[2] =
		bf_get(lpfc_fcf_record_fab_name_2, new_fcf_record);
	fcf_rec->fabric_name[3] =
		bf_get(lpfc_fcf_record_fab_name_3, new_fcf_record);
	fcf_rec->fabric_name[4] =
		bf_get(lpfc_fcf_record_fab_name_4, new_fcf_record);
	fcf_rec->fabric_name[5] =
		bf_get(lpfc_fcf_record_fab_name_5, new_fcf_record);
	fcf_rec->fabric_name[6] =
		bf_get(lpfc_fcf_record_fab_name_6, new_fcf_record);
	fcf_rec->fabric_name[7] =
		bf_get(lpfc_fcf_record_fab_name_7, new_fcf_record);
	 
	fcf_rec->mac_addr[0] = bf_get(lpfc_fcf_record_mac_0, new_fcf_record);
	fcf_rec->mac_addr[1] = bf_get(lpfc_fcf_record_mac_1, new_fcf_record);
	fcf_rec->mac_addr[2] = bf_get(lpfc_fcf_record_mac_2, new_fcf_record);
	fcf_rec->mac_addr[3] = bf_get(lpfc_fcf_record_mac_3, new_fcf_record);
	fcf_rec->mac_addr[4] = bf_get(lpfc_fcf_record_mac_4, new_fcf_record);
	fcf_rec->mac_addr[5] = bf_get(lpfc_fcf_record_mac_5, new_fcf_record);
	 
	fcf_rec->fcf_indx = bf_get(lpfc_fcf_record_fcf_index, new_fcf_record);
	 
	fcf_rec->priority = new_fcf_record->fip_priority;
	 
	fcf_rec->switch_name[0] =
		bf_get(lpfc_fcf_record_switch_name_0, new_fcf_record);
	fcf_rec->switch_name[1] =
		bf_get(lpfc_fcf_record_switch_name_1, new_fcf_record);
	fcf_rec->switch_name[2] =
		bf_get(lpfc_fcf_record_switch_name_2, new_fcf_record);
	fcf_rec->switch_name[3] =
		bf_get(lpfc_fcf_record_switch_name_3, new_fcf_record);
	fcf_rec->switch_name[4] =
		bf_get(lpfc_fcf_record_switch_name_4, new_fcf_record);
	fcf_rec->switch_name[5] =
		bf_get(lpfc_fcf_record_switch_name_5, new_fcf_record);
	fcf_rec->switch_name[6] =
		bf_get(lpfc_fcf_record_switch_name_6, new_fcf_record);
	fcf_rec->switch_name[7] =
		bf_get(lpfc_fcf_record_switch_name_7, new_fcf_record);
}

 
static void
__lpfc_update_fcf_record(struct lpfc_hba *phba, struct lpfc_fcf_rec *fcf_rec,
		       struct fcf_record *new_fcf_record, uint32_t addr_mode,
		       uint16_t vlan_id, uint32_t flag)
{
	lockdep_assert_held(&phba->hbalock);

	 
	lpfc_copy_fcf_record(fcf_rec, new_fcf_record);
	 
	fcf_rec->addr_mode = addr_mode;
	fcf_rec->vlan_id = vlan_id;
	fcf_rec->flag |= (flag | RECORD_VALID);
	__lpfc_update_fcf_record_pri(phba,
		bf_get(lpfc_fcf_record_fcf_index, new_fcf_record),
				 new_fcf_record);
}

 
static void
lpfc_register_fcf(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *fcf_mbxq;
	int rc;

	spin_lock_irq(&phba->hbalock);
	 
	if (!(phba->fcf.fcf_flag & FCF_AVAILABLE)) {
		phba->hba_flag &= ~(FCF_TS_INPROG | FCF_RR_INPROG);
		spin_unlock_irq(&phba->hbalock);
		return;
	}

	 
	if (phba->fcf.fcf_flag & FCF_REGISTERED) {
		phba->fcf.fcf_flag |= (FCF_SCAN_DONE | FCF_IN_USE);
		phba->hba_flag &= ~FCF_TS_INPROG;
		if (phba->pport->port_state != LPFC_FLOGI &&
		    phba->pport->fc_flag & FC_FABRIC) {
			phba->hba_flag |= FCF_RR_INPROG;
			spin_unlock_irq(&phba->hbalock);
			lpfc_initial_flogi(phba->pport);
			return;
		}
		spin_unlock_irq(&phba->hbalock);
		return;
	}
	spin_unlock_irq(&phba->hbalock);

	fcf_mbxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!fcf_mbxq) {
		spin_lock_irq(&phba->hbalock);
		phba->hba_flag &= ~(FCF_TS_INPROG | FCF_RR_INPROG);
		spin_unlock_irq(&phba->hbalock);
		return;
	}

	lpfc_reg_fcfi(phba, fcf_mbxq);
	fcf_mbxq->vport = phba->pport;
	fcf_mbxq->mbox_cmpl = lpfc_mbx_cmpl_reg_fcfi;
	rc = lpfc_sli_issue_mbox(phba, fcf_mbxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		spin_lock_irq(&phba->hbalock);
		phba->hba_flag &= ~(FCF_TS_INPROG | FCF_RR_INPROG);
		spin_unlock_irq(&phba->hbalock);
		mempool_free(fcf_mbxq, phba->mbox_mem_pool);
	}

	return;
}

 
static int
lpfc_match_fcf_conn_list(struct lpfc_hba *phba,
			struct fcf_record *new_fcf_record,
			uint32_t *boot_flag, uint32_t *addr_mode,
			uint16_t *vlan_id)
{
	struct lpfc_fcf_conn_entry *conn_entry;
	int i, j, fcf_vlan_id = 0;

	 
	for (i = 0; i < 512; i++) {
		if (new_fcf_record->vlan_bitmap[i]) {
			fcf_vlan_id = i * 8;
			j = 0;
			while (!((new_fcf_record->vlan_bitmap[i] >> j) & 1)) {
				j++;
				fcf_vlan_id++;
			}
			break;
		}
	}

	 
	if (!bf_get(lpfc_fcf_record_fcf_avail, new_fcf_record) ||
	    !bf_get(lpfc_fcf_record_fcf_valid, new_fcf_record) ||
	    bf_get(lpfc_fcf_record_fcf_sol, new_fcf_record))
		return 0;

	if (!(phba->hba_flag & HBA_FIP_SUPPORT)) {
		*boot_flag = 0;
		*addr_mode = bf_get(lpfc_fcf_record_mac_addr_prov,
				new_fcf_record);
		if (phba->valid_vlan)
			*vlan_id = phba->vlan_id;
		else
			*vlan_id = LPFC_FCOE_NULL_VID;
		return 1;
	}

	 
	if (list_empty(&phba->fcf_conn_rec_list)) {
		*boot_flag = 0;
		*addr_mode = bf_get(lpfc_fcf_record_mac_addr_prov,
			new_fcf_record);

		 
		if (*addr_mode & LPFC_FCF_FPMA)
			*addr_mode = LPFC_FCF_FPMA;

		 
		if (fcf_vlan_id)
			*vlan_id = fcf_vlan_id;
		else
			*vlan_id = LPFC_FCOE_NULL_VID;
		return 1;
	}

	list_for_each_entry(conn_entry,
			    &phba->fcf_conn_rec_list, list) {
		if (!(conn_entry->conn_rec.flags & FCFCNCT_VALID))
			continue;

		if ((conn_entry->conn_rec.flags & FCFCNCT_FBNM_VALID) &&
			!lpfc_fab_name_match(conn_entry->conn_rec.fabric_name,
					     new_fcf_record))
			continue;
		if ((conn_entry->conn_rec.flags & FCFCNCT_SWNM_VALID) &&
			!lpfc_sw_name_match(conn_entry->conn_rec.switch_name,
					    new_fcf_record))
			continue;
		if (conn_entry->conn_rec.flags & FCFCNCT_VLAN_VALID) {
			 
			if (!(new_fcf_record->vlan_bitmap
				[conn_entry->conn_rec.vlan_tag / 8] &
				(1 << (conn_entry->conn_rec.vlan_tag % 8))))
				continue;
		}

		 
		if (!(bf_get(lpfc_fcf_record_mac_addr_prov, new_fcf_record)
			& (LPFC_FCF_FPMA | LPFC_FCF_SPMA)))
			continue;

		 
		if ((conn_entry->conn_rec.flags & FCFCNCT_AM_VALID) &&
			!(conn_entry->conn_rec.flags & FCFCNCT_AM_PREFERRED)) {

			 
			if ((conn_entry->conn_rec.flags & FCFCNCT_AM_SPMA) &&
				!(bf_get(lpfc_fcf_record_mac_addr_prov,
					new_fcf_record) & LPFC_FCF_SPMA))
				continue;

			 
			if (!(conn_entry->conn_rec.flags & FCFCNCT_AM_SPMA) &&
				!(bf_get(lpfc_fcf_record_mac_addr_prov,
				new_fcf_record) & LPFC_FCF_FPMA))
				continue;
		}

		 
		if (conn_entry->conn_rec.flags & FCFCNCT_BOOT)
			*boot_flag = 1;
		else
			*boot_flag = 0;

		 
		*addr_mode = bf_get(lpfc_fcf_record_mac_addr_prov,
				new_fcf_record);
		 
		if ((conn_entry->conn_rec.flags & FCFCNCT_AM_VALID) &&
			(!(conn_entry->conn_rec.flags & FCFCNCT_AM_PREFERRED)))
			*addr_mode = (conn_entry->conn_rec.flags &
				FCFCNCT_AM_SPMA) ?
				LPFC_FCF_SPMA : LPFC_FCF_FPMA;
		 
		else if ((conn_entry->conn_rec.flags & FCFCNCT_AM_VALID) &&
			(conn_entry->conn_rec.flags & FCFCNCT_AM_PREFERRED) &&
			(conn_entry->conn_rec.flags & FCFCNCT_AM_SPMA) &&
			(*addr_mode & LPFC_FCF_SPMA))
				*addr_mode = LPFC_FCF_SPMA;
		else if ((conn_entry->conn_rec.flags & FCFCNCT_AM_VALID) &&
			(conn_entry->conn_rec.flags & FCFCNCT_AM_PREFERRED) &&
			!(conn_entry->conn_rec.flags & FCFCNCT_AM_SPMA) &&
			(*addr_mode & LPFC_FCF_FPMA))
				*addr_mode = LPFC_FCF_FPMA;

		 
		if (conn_entry->conn_rec.flags & FCFCNCT_VLAN_VALID)
			*vlan_id = conn_entry->conn_rec.vlan_tag;
		 
		else if (fcf_vlan_id)
			*vlan_id = fcf_vlan_id;
		else
			*vlan_id = LPFC_FCOE_NULL_VID;

		return 1;
	}

	return 0;
}

 
int
lpfc_check_pending_fcoe_event(struct lpfc_hba *phba, uint8_t unreg_fcf)
{
	 
	if ((phba->link_state  >= LPFC_LINK_UP) &&
	    (phba->fcoe_eventtag == phba->fcoe_eventtag_at_fcf_scan))
		return 0;

	lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
			"2768 Pending link or FCF event during current "
			"handling of the previous event: link_state:x%x, "
			"evt_tag_at_scan:x%x, evt_tag_current:x%x\n",
			phba->link_state, phba->fcoe_eventtag_at_fcf_scan,
			phba->fcoe_eventtag);

	spin_lock_irq(&phba->hbalock);
	phba->fcf.fcf_flag &= ~FCF_AVAILABLE;
	spin_unlock_irq(&phba->hbalock);

	if (phba->link_state >= LPFC_LINK_UP) {
		lpfc_printf_log(phba, KERN_INFO, LOG_FIP | LOG_DISCOVERY,
				"2780 Restart FCF table scan due to "
				"pending FCF event:evt_tag_at_scan:x%x, "
				"evt_tag_current:x%x\n",
				phba->fcoe_eventtag_at_fcf_scan,
				phba->fcoe_eventtag);
		lpfc_sli4_fcf_scan_read_fcf_rec(phba, LPFC_FCOE_FCF_GET_FIRST);
	} else {
		 
		lpfc_printf_log(phba, KERN_INFO, LOG_FIP | LOG_DISCOVERY,
				"2833 Stop FCF discovery process due to link "
				"state change (x%x)\n", phba->link_state);
		spin_lock_irq(&phba->hbalock);
		phba->hba_flag &= ~(FCF_TS_INPROG | FCF_RR_INPROG);
		phba->fcf.fcf_flag &= ~(FCF_REDISC_FOV | FCF_DISCOVERY);
		spin_unlock_irq(&phba->hbalock);
	}

	 
	if (unreg_fcf) {
		spin_lock_irq(&phba->hbalock);
		phba->fcf.fcf_flag &= ~FCF_REGISTERED;
		spin_unlock_irq(&phba->hbalock);
		lpfc_sli4_unregister_fcf(phba);
	}
	return 1;
}

 
static bool
lpfc_sli4_new_fcf_random_select(struct lpfc_hba *phba, uint32_t fcf_cnt)
{
	uint32_t rand_num;

	 
	rand_num = get_random_u16();

	 
	if ((fcf_cnt * rand_num) < 0xFFFF)
		return true;
	else
		return false;
}

 
static struct fcf_record *
lpfc_sli4_fcf_rec_mbox_parse(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq,
			     uint16_t *next_fcf_index)
{
	void *virt_addr;
	struct lpfc_mbx_sge sge;
	struct lpfc_mbx_read_fcf_tbl *read_fcf;
	uint32_t shdr_status, shdr_add_status, if_type;
	union lpfc_sli4_cfg_shdr *shdr;
	struct fcf_record *new_fcf_record;

	 
	lpfc_sli4_mbx_sge_get(mboxq, 0, &sge);
	if (unlikely(!mboxq->sge_array)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2524 Failed to get the non-embedded SGE "
				"virtual address\n");
		return NULL;
	}
	virt_addr = mboxq->sge_array->addr[0];

	shdr = (union lpfc_sli4_cfg_shdr *)virt_addr;
	lpfc_sli_pcimem_bcopy(shdr, shdr,
			      sizeof(union lpfc_sli4_cfg_shdr));
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	if_type = bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status) {
		if (shdr_status == STATUS_FCF_TABLE_EMPTY ||
					if_type == LPFC_SLI_INTF_IF_TYPE_2)
			lpfc_printf_log(phba, KERN_ERR,
					LOG_TRACE_EVENT,
					"2726 READ_FCF_RECORD Indicates empty "
					"FCF table.\n");
		else
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"2521 READ_FCF_RECORD mailbox failed "
					"with status x%x add_status x%x, "
					"mbx\n", shdr_status, shdr_add_status);
		return NULL;
	}

	 
	read_fcf = (struct lpfc_mbx_read_fcf_tbl *)virt_addr;
	lpfc_sli_pcimem_bcopy(read_fcf, read_fcf,
			      sizeof(struct lpfc_mbx_read_fcf_tbl));
	*next_fcf_index = bf_get(lpfc_mbx_read_fcf_tbl_nxt_vindx, read_fcf);
	new_fcf_record = (struct fcf_record *)(virt_addr +
			  sizeof(struct lpfc_mbx_read_fcf_tbl));
	lpfc_sli_pcimem_bcopy(new_fcf_record, new_fcf_record,
				offsetof(struct fcf_record, vlan_bitmap));
	new_fcf_record->word137 = le32_to_cpu(new_fcf_record->word137);
	new_fcf_record->word138 = le32_to_cpu(new_fcf_record->word138);

	return new_fcf_record;
}

 
static void
lpfc_sli4_log_fcf_record_info(struct lpfc_hba *phba,
			      struct fcf_record *fcf_record,
			      uint16_t vlan_id,
			      uint16_t next_fcf_index)
{
	lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
			"2764 READ_FCF_RECORD:\n"
			"\tFCF_Index     : x%x\n"
			"\tFCF_Avail     : x%x\n"
			"\tFCF_Valid     : x%x\n"
			"\tFCF_SOL       : x%x\n"
			"\tFIP_Priority  : x%x\n"
			"\tMAC_Provider  : x%x\n"
			"\tLowest VLANID : x%x\n"
			"\tFCF_MAC Addr  : x%x:%x:%x:%x:%x:%x\n"
			"\tFabric_Name   : x%x:%x:%x:%x:%x:%x:%x:%x\n"
			"\tSwitch_Name   : x%x:%x:%x:%x:%x:%x:%x:%x\n"
			"\tNext_FCF_Index: x%x\n",
			bf_get(lpfc_fcf_record_fcf_index, fcf_record),
			bf_get(lpfc_fcf_record_fcf_avail, fcf_record),
			bf_get(lpfc_fcf_record_fcf_valid, fcf_record),
			bf_get(lpfc_fcf_record_fcf_sol, fcf_record),
			fcf_record->fip_priority,
			bf_get(lpfc_fcf_record_mac_addr_prov, fcf_record),
			vlan_id,
			bf_get(lpfc_fcf_record_mac_0, fcf_record),
			bf_get(lpfc_fcf_record_mac_1, fcf_record),
			bf_get(lpfc_fcf_record_mac_2, fcf_record),
			bf_get(lpfc_fcf_record_mac_3, fcf_record),
			bf_get(lpfc_fcf_record_mac_4, fcf_record),
			bf_get(lpfc_fcf_record_mac_5, fcf_record),
			bf_get(lpfc_fcf_record_fab_name_0, fcf_record),
			bf_get(lpfc_fcf_record_fab_name_1, fcf_record),
			bf_get(lpfc_fcf_record_fab_name_2, fcf_record),
			bf_get(lpfc_fcf_record_fab_name_3, fcf_record),
			bf_get(lpfc_fcf_record_fab_name_4, fcf_record),
			bf_get(lpfc_fcf_record_fab_name_5, fcf_record),
			bf_get(lpfc_fcf_record_fab_name_6, fcf_record),
			bf_get(lpfc_fcf_record_fab_name_7, fcf_record),
			bf_get(lpfc_fcf_record_switch_name_0, fcf_record),
			bf_get(lpfc_fcf_record_switch_name_1, fcf_record),
			bf_get(lpfc_fcf_record_switch_name_2, fcf_record),
			bf_get(lpfc_fcf_record_switch_name_3, fcf_record),
			bf_get(lpfc_fcf_record_switch_name_4, fcf_record),
			bf_get(lpfc_fcf_record_switch_name_5, fcf_record),
			bf_get(lpfc_fcf_record_switch_name_6, fcf_record),
			bf_get(lpfc_fcf_record_switch_name_7, fcf_record),
			next_fcf_index);
}

 
static bool
lpfc_sli4_fcf_record_match(struct lpfc_hba *phba,
			   struct lpfc_fcf_rec *fcf_rec,
			   struct fcf_record *new_fcf_record,
			   uint16_t new_vlan_id)
{
	if (new_vlan_id != LPFC_FCOE_IGNORE_VID)
		if (!lpfc_vlan_id_match(fcf_rec->vlan_id, new_vlan_id))
			return false;
	if (!lpfc_mac_addr_match(fcf_rec->mac_addr, new_fcf_record))
		return false;
	if (!lpfc_sw_name_match(fcf_rec->switch_name, new_fcf_record))
		return false;
	if (!lpfc_fab_name_match(fcf_rec->fabric_name, new_fcf_record))
		return false;
	if (fcf_rec->priority != new_fcf_record->fip_priority)
		return false;
	return true;
}

 
int lpfc_sli4_fcf_rr_next_proc(struct lpfc_vport *vport, uint16_t fcf_index)
{
	struct lpfc_hba *phba = vport->phba;
	int rc;

	if (fcf_index == LPFC_FCOE_FCF_NEXT_NONE) {
		spin_lock_irq(&phba->hbalock);
		if (phba->hba_flag & HBA_DEVLOSS_TMO) {
			spin_unlock_irq(&phba->hbalock);
			lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
					"2872 Devloss tmo with no eligible "
					"FCF, unregister in-use FCF (x%x) "
					"and rescan FCF table\n",
					phba->fcf.current_rec.fcf_indx);
			lpfc_unregister_fcf_rescan(phba);
			goto stop_flogi_current_fcf;
		}
		 
		phba->hba_flag &= ~FCF_RR_INPROG;
		 
		phba->fcf.fcf_flag &= ~(FCF_AVAILABLE | FCF_SCAN_DONE);
		spin_unlock_irq(&phba->hbalock);
		lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
				"2865 No FCF available, stop roundrobin FCF "
				"failover and change port state:x%x/x%x\n",
				phba->pport->port_state, LPFC_VPORT_UNKNOWN);
		phba->pport->port_state = LPFC_VPORT_UNKNOWN;

		if (!phba->fcf.fcf_redisc_attempted) {
			lpfc_unregister_fcf(phba);

			rc = lpfc_sli4_redisc_fcf_table(phba);
			if (!rc) {
				lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
						"3195 Rediscover FCF table\n");
				phba->fcf.fcf_redisc_attempted = 1;
				lpfc_sli4_clear_fcf_rr_bmask(phba);
			} else {
				lpfc_printf_log(phba, KERN_WARNING, LOG_FIP,
						"3196 Rediscover FCF table "
						"failed. Status:x%x\n", rc);
			}
		} else {
			lpfc_printf_log(phba, KERN_WARNING, LOG_FIP,
					"3197 Already rediscover FCF table "
					"attempted. No more retry\n");
		}
		goto stop_flogi_current_fcf;
	} else {
		lpfc_printf_log(phba, KERN_INFO, LOG_FIP | LOG_ELS,
				"2794 Try FLOGI roundrobin FCF failover to "
				"(x%x)\n", fcf_index);
		rc = lpfc_sli4_fcf_rr_read_fcf_rec(phba, fcf_index);
		if (rc)
			lpfc_printf_log(phba, KERN_WARNING, LOG_FIP | LOG_ELS,
					"2761 FLOGI roundrobin FCF failover "
					"failed (rc:x%x) to read FCF (x%x)\n",
					rc, phba->fcf.current_rec.fcf_indx);
		else
			goto stop_flogi_current_fcf;
	}
	return 0;

stop_flogi_current_fcf:
	lpfc_can_disctmo(vport);
	return 1;
}

 
static void lpfc_sli4_fcf_pri_list_del(struct lpfc_hba *phba,
			uint16_t fcf_index)
{
	struct lpfc_fcf_pri *new_fcf_pri;

	new_fcf_pri = &phba->fcf.fcf_pri[fcf_index];
	lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
		"3058 deleting idx x%x pri x%x flg x%x\n",
		fcf_index, new_fcf_pri->fcf_rec.priority,
		 new_fcf_pri->fcf_rec.flag);
	spin_lock_irq(&phba->hbalock);
	if (new_fcf_pri->fcf_rec.flag & LPFC_FCF_ON_PRI_LIST) {
		if (phba->fcf.current_rec.priority ==
				new_fcf_pri->fcf_rec.priority)
			phba->fcf.eligible_fcf_cnt--;
		list_del_init(&new_fcf_pri->list);
		new_fcf_pri->fcf_rec.flag &= ~LPFC_FCF_ON_PRI_LIST;
	}
	spin_unlock_irq(&phba->hbalock);
}

 
void
lpfc_sli4_set_fcf_flogi_fail(struct lpfc_hba *phba, uint16_t fcf_index)
{
	struct lpfc_fcf_pri *new_fcf_pri;
	new_fcf_pri = &phba->fcf.fcf_pri[fcf_index];
	spin_lock_irq(&phba->hbalock);
	new_fcf_pri->fcf_rec.flag |= LPFC_FCF_FLOGI_FAILED;
	spin_unlock_irq(&phba->hbalock);
}

 
static int lpfc_sli4_fcf_pri_list_add(struct lpfc_hba *phba,
	uint16_t fcf_index,
	struct fcf_record *new_fcf_record)
{
	uint16_t current_fcf_pri;
	uint16_t last_index;
	struct lpfc_fcf_pri *fcf_pri;
	struct lpfc_fcf_pri *next_fcf_pri;
	struct lpfc_fcf_pri *new_fcf_pri;
	int ret;

	new_fcf_pri = &phba->fcf.fcf_pri[fcf_index];
	lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
		"3059 adding idx x%x pri x%x flg x%x\n",
		fcf_index, new_fcf_record->fip_priority,
		 new_fcf_pri->fcf_rec.flag);
	spin_lock_irq(&phba->hbalock);
	if (new_fcf_pri->fcf_rec.flag & LPFC_FCF_ON_PRI_LIST)
		list_del_init(&new_fcf_pri->list);
	new_fcf_pri->fcf_rec.fcf_index = fcf_index;
	new_fcf_pri->fcf_rec.priority = new_fcf_record->fip_priority;
	if (list_empty(&phba->fcf.fcf_pri_list)) {
		list_add(&new_fcf_pri->list, &phba->fcf.fcf_pri_list);
		ret = lpfc_sli4_fcf_rr_index_set(phba,
				new_fcf_pri->fcf_rec.fcf_index);
		goto out;
	}

	last_index = find_first_bit(phba->fcf.fcf_rr_bmask,
				LPFC_SLI4_FCF_TBL_INDX_MAX);
	if (last_index >= LPFC_SLI4_FCF_TBL_INDX_MAX) {
		ret = 0;  
		goto out;
	}
	current_fcf_pri = phba->fcf.fcf_pri[last_index].fcf_rec.priority;
	if (new_fcf_pri->fcf_rec.priority <=  current_fcf_pri) {
		list_add(&new_fcf_pri->list, &phba->fcf.fcf_pri_list);
		if (new_fcf_pri->fcf_rec.priority <  current_fcf_pri) {
			memset(phba->fcf.fcf_rr_bmask, 0,
				sizeof(*phba->fcf.fcf_rr_bmask));
			 
			phba->fcf.eligible_fcf_cnt = 1;
		} else
			 
			phba->fcf.eligible_fcf_cnt++;
		ret = lpfc_sli4_fcf_rr_index_set(phba,
				new_fcf_pri->fcf_rec.fcf_index);
		goto out;
	}

	list_for_each_entry_safe(fcf_pri, next_fcf_pri,
				&phba->fcf.fcf_pri_list, list) {
		if (new_fcf_pri->fcf_rec.priority <=
				fcf_pri->fcf_rec.priority) {
			if (fcf_pri->list.prev == &phba->fcf.fcf_pri_list)
				list_add(&new_fcf_pri->list,
						&phba->fcf.fcf_pri_list);
			else
				list_add(&new_fcf_pri->list,
					 &((struct lpfc_fcf_pri *)
					fcf_pri->list.prev)->list);
			ret = 0;
			goto out;
		} else if (fcf_pri->list.next == &phba->fcf.fcf_pri_list
			|| new_fcf_pri->fcf_rec.priority <
				next_fcf_pri->fcf_rec.priority) {
			list_add(&new_fcf_pri->list, &fcf_pri->list);
			ret = 0;
			goto out;
		}
		if (new_fcf_pri->fcf_rec.priority > fcf_pri->fcf_rec.priority)
			continue;

	}
	ret = 1;
out:
	 
	new_fcf_pri->fcf_rec.flag = LPFC_FCF_ON_PRI_LIST;
	spin_unlock_irq(&phba->hbalock);
	return ret;
}

 
void
lpfc_mbx_cmpl_fcf_scan_read_fcf_rec(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	struct fcf_record *new_fcf_record;
	uint32_t boot_flag, addr_mode;
	uint16_t fcf_index, next_fcf_index;
	struct lpfc_fcf_rec *fcf_rec = NULL;
	uint16_t vlan_id = LPFC_FCOE_NULL_VID;
	bool select_new_fcf;
	int rc;

	 
	if (lpfc_check_pending_fcoe_event(phba, LPFC_SKIP_UNREG_FCF)) {
		lpfc_sli4_mbox_cmd_free(phba, mboxq);
		return;
	}

	 
	new_fcf_record = lpfc_sli4_fcf_rec_mbox_parse(phba, mboxq,
						      &next_fcf_index);
	if (!new_fcf_record) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2765 Mailbox command READ_FCF_RECORD "
				"failed to retrieve a FCF record.\n");
		 
		spin_lock_irq(&phba->hbalock);
		phba->hba_flag &= ~FCF_TS_INPROG;
		spin_unlock_irq(&phba->hbalock);
		lpfc_sli4_mbox_cmd_free(phba, mboxq);
		return;
	}

	 
	rc = lpfc_match_fcf_conn_list(phba, new_fcf_record, &boot_flag,
				      &addr_mode, &vlan_id);

	 
	lpfc_sli4_log_fcf_record_info(phba, new_fcf_record, vlan_id,
				      next_fcf_index);

	 
	if (!rc) {
		lpfc_sli4_fcf_pri_list_del(phba,
					bf_get(lpfc_fcf_record_fcf_index,
					       new_fcf_record));
		lpfc_printf_log(phba, KERN_WARNING, LOG_FIP,
				"2781 FCF (x%x) failed connection "
				"list check: (x%x/x%x/%x)\n",
				bf_get(lpfc_fcf_record_fcf_index,
				       new_fcf_record),
				bf_get(lpfc_fcf_record_fcf_avail,
				       new_fcf_record),
				bf_get(lpfc_fcf_record_fcf_valid,
				       new_fcf_record),
				bf_get(lpfc_fcf_record_fcf_sol,
				       new_fcf_record));
		if ((phba->fcf.fcf_flag & FCF_IN_USE) &&
		    lpfc_sli4_fcf_record_match(phba, &phba->fcf.current_rec,
		    new_fcf_record, LPFC_FCOE_IGNORE_VID)) {
			if (bf_get(lpfc_fcf_record_fcf_index, new_fcf_record) !=
			    phba->fcf.current_rec.fcf_indx) {
				lpfc_printf_log(phba, KERN_ERR,
						LOG_TRACE_EVENT,
					"2862 FCF (x%x) matches property "
					"of in-use FCF (x%x)\n",
					bf_get(lpfc_fcf_record_fcf_index,
					       new_fcf_record),
					phba->fcf.current_rec.fcf_indx);
				goto read_next_fcf;
			}
			 
			if (!(phba->fcf.fcf_flag & FCF_REDISC_PEND) &&
			    !(phba->fcf.fcf_flag & FCF_REDISC_FOV)) {
				lpfc_printf_log(phba, KERN_WARNING, LOG_FIP,
						"2835 Invalid in-use FCF "
						"(x%x), enter FCF failover "
						"table scan.\n",
						phba->fcf.current_rec.fcf_indx);
				spin_lock_irq(&phba->hbalock);
				phba->fcf.fcf_flag |= FCF_REDISC_FOV;
				spin_unlock_irq(&phba->hbalock);
				lpfc_sli4_mbox_cmd_free(phba, mboxq);
				lpfc_sli4_fcf_scan_read_fcf_rec(phba,
						LPFC_FCOE_FCF_GET_FIRST);
				return;
			}
		}
		goto read_next_fcf;
	} else {
		fcf_index = bf_get(lpfc_fcf_record_fcf_index, new_fcf_record);
		rc = lpfc_sli4_fcf_pri_list_add(phba, fcf_index,
							new_fcf_record);
		if (rc)
			goto read_next_fcf;
	}

	 
	spin_lock_irq(&phba->hbalock);
	if (phba->fcf.fcf_flag & FCF_IN_USE) {
		if (phba->cfg_fcf_failover_policy == LPFC_FCF_FOV &&
			lpfc_sli4_fcf_record_match(phba, &phba->fcf.current_rec,
		    new_fcf_record, vlan_id)) {
			if (bf_get(lpfc_fcf_record_fcf_index, new_fcf_record) ==
			    phba->fcf.current_rec.fcf_indx) {
				phba->fcf.fcf_flag |= FCF_AVAILABLE;
				if (phba->fcf.fcf_flag & FCF_REDISC_PEND)
					 
					__lpfc_sli4_stop_fcf_redisc_wait_timer(
									phba);
				else if (phba->fcf.fcf_flag & FCF_REDISC_FOV)
					 
					phba->fcf.fcf_flag &= ~FCF_REDISC_FOV;
				spin_unlock_irq(&phba->hbalock);
				lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
						"2836 New FCF matches in-use "
						"FCF (x%x), port_state:x%x, "
						"fc_flag:x%x\n",
						phba->fcf.current_rec.fcf_indx,
						phba->pport->port_state,
						phba->pport->fc_flag);
				goto out;
			} else
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"2863 New FCF (x%x) matches "
					"property of in-use FCF (x%x)\n",
					bf_get(lpfc_fcf_record_fcf_index,
					       new_fcf_record),
					phba->fcf.current_rec.fcf_indx);
		}
		 
		if (!(phba->fcf.fcf_flag & FCF_REDISC_FOV)) {
			spin_unlock_irq(&phba->hbalock);
			goto read_next_fcf;
		}
	}
	 
	if (phba->fcf.fcf_flag & FCF_REDISC_FOV)
		fcf_rec = &phba->fcf.failover_rec;
	else
		fcf_rec = &phba->fcf.current_rec;

	if (phba->fcf.fcf_flag & FCF_AVAILABLE) {
		 
		if (boot_flag && !(fcf_rec->flag & BOOT_ENABLE)) {
			 
			lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
					"2837 Update current FCF record "
					"(x%x) with new FCF record (x%x)\n",
					fcf_rec->fcf_indx,
					bf_get(lpfc_fcf_record_fcf_index,
					new_fcf_record));
			__lpfc_update_fcf_record(phba, fcf_rec, new_fcf_record,
					addr_mode, vlan_id, BOOT_ENABLE);
			spin_unlock_irq(&phba->hbalock);
			goto read_next_fcf;
		}
		 
		if (!boot_flag && (fcf_rec->flag & BOOT_ENABLE)) {
			spin_unlock_irq(&phba->hbalock);
			goto read_next_fcf;
		}
		 
		if (new_fcf_record->fip_priority < fcf_rec->priority) {
			 
			lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
					"2838 Update current FCF record "
					"(x%x) with new FCF record (x%x)\n",
					fcf_rec->fcf_indx,
					bf_get(lpfc_fcf_record_fcf_index,
					       new_fcf_record));
			__lpfc_update_fcf_record(phba, fcf_rec, new_fcf_record,
					addr_mode, vlan_id, 0);
			 
			phba->fcf.eligible_fcf_cnt = 1;
		} else if (new_fcf_record->fip_priority == fcf_rec->priority) {
			 
			phba->fcf.eligible_fcf_cnt++;
			select_new_fcf = lpfc_sli4_new_fcf_random_select(phba,
						phba->fcf.eligible_fcf_cnt);
			if (select_new_fcf) {
				lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
					"2839 Update current FCF record "
					"(x%x) with new FCF record (x%x)\n",
					fcf_rec->fcf_indx,
					bf_get(lpfc_fcf_record_fcf_index,
					       new_fcf_record));
				 
				__lpfc_update_fcf_record(phba, fcf_rec,
							 new_fcf_record,
							 addr_mode, vlan_id, 0);
			}
		}
		spin_unlock_irq(&phba->hbalock);
		goto read_next_fcf;
	}
	 
	if (fcf_rec) {
		lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
				"2840 Update initial FCF candidate "
				"with FCF (x%x)\n",
				bf_get(lpfc_fcf_record_fcf_index,
				       new_fcf_record));
		__lpfc_update_fcf_record(phba, fcf_rec, new_fcf_record,
					 addr_mode, vlan_id, (boot_flag ?
					 BOOT_ENABLE : 0));
		phba->fcf.fcf_flag |= FCF_AVAILABLE;
		 
		phba->fcf.eligible_fcf_cnt = 1;
	}
	spin_unlock_irq(&phba->hbalock);
	goto read_next_fcf;

read_next_fcf:
	lpfc_sli4_mbox_cmd_free(phba, mboxq);
	if (next_fcf_index == LPFC_FCOE_FCF_NEXT_NONE || next_fcf_index == 0) {
		if (phba->fcf.fcf_flag & FCF_REDISC_FOV) {
			 

			 
			if (!(phba->fcf.failover_rec.flag & RECORD_VALID)) {
				lpfc_printf_log(phba, KERN_WARNING, LOG_FIP,
					       "2782 No suitable FCF found: "
					       "(x%x/x%x)\n",
					       phba->fcoe_eventtag_at_fcf_scan,
					       bf_get(lpfc_fcf_record_fcf_index,
						      new_fcf_record));
				spin_lock_irq(&phba->hbalock);
				if (phba->hba_flag & HBA_DEVLOSS_TMO) {
					phba->hba_flag &= ~FCF_TS_INPROG;
					spin_unlock_irq(&phba->hbalock);
					 
					lpfc_printf_log(phba, KERN_INFO,
							LOG_FIP,
							"2864 On devloss tmo "
							"unreg in-use FCF and "
							"rescan FCF table\n");
					lpfc_unregister_fcf_rescan(phba);
					return;
				}
				 
				phba->hba_flag &= ~FCF_TS_INPROG;
				spin_unlock_irq(&phba->hbalock);
				return;
			}
			 

			 
			lpfc_unregister_fcf(phba);

			 
			lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
					"2842 Replace in-use FCF (x%x) "
					"with failover FCF (x%x)\n",
					phba->fcf.current_rec.fcf_indx,
					phba->fcf.failover_rec.fcf_indx);
			memcpy(&phba->fcf.current_rec,
			       &phba->fcf.failover_rec,
			       sizeof(struct lpfc_fcf_rec));
			 
			spin_lock_irq(&phba->hbalock);
			phba->fcf.fcf_flag &= ~FCF_REDISC_FOV;
			spin_unlock_irq(&phba->hbalock);
			 
			lpfc_register_fcf(phba);
		} else {
			 
			if ((phba->fcf.fcf_flag & FCF_REDISC_EVT) ||
			    (phba->fcf.fcf_flag & FCF_REDISC_PEND))
				return;

			if (phba->cfg_fcf_failover_policy == LPFC_FCF_FOV &&
				phba->fcf.fcf_flag & FCF_IN_USE) {
				 
				lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
						"2841 In-use FCF record (x%x) "
						"not reported, entering fast "
						"FCF failover mode scanning.\n",
						phba->fcf.current_rec.fcf_indx);
				spin_lock_irq(&phba->hbalock);
				phba->fcf.fcf_flag |= FCF_REDISC_FOV;
				spin_unlock_irq(&phba->hbalock);
				lpfc_sli4_fcf_scan_read_fcf_rec(phba,
						LPFC_FCOE_FCF_GET_FIRST);
				return;
			}
			 
			lpfc_register_fcf(phba);
		}
	} else
		lpfc_sli4_fcf_scan_read_fcf_rec(phba, next_fcf_index);
	return;

out:
	lpfc_sli4_mbox_cmd_free(phba, mboxq);
	lpfc_register_fcf(phba);

	return;
}

 
void
lpfc_mbx_cmpl_fcf_rr_read_fcf_rec(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	struct fcf_record *new_fcf_record;
	uint32_t boot_flag, addr_mode;
	uint16_t next_fcf_index, fcf_index;
	uint16_t current_fcf_index;
	uint16_t vlan_id = LPFC_FCOE_NULL_VID;
	int rc;

	 
	if (phba->link_state < LPFC_LINK_UP) {
		spin_lock_irq(&phba->hbalock);
		phba->fcf.fcf_flag &= ~FCF_DISCOVERY;
		phba->hba_flag &= ~FCF_RR_INPROG;
		spin_unlock_irq(&phba->hbalock);
		goto out;
	}

	 
	new_fcf_record = lpfc_sli4_fcf_rec_mbox_parse(phba, mboxq,
						      &next_fcf_index);
	if (!new_fcf_record) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_FIP,
				"2766 Mailbox command READ_FCF_RECORD "
				"failed to retrieve a FCF record. "
				"hba_flg x%x fcf_flg x%x\n", phba->hba_flag,
				phba->fcf.fcf_flag);
		lpfc_unregister_fcf_rescan(phba);
		goto out;
	}

	 
	rc = lpfc_match_fcf_conn_list(phba, new_fcf_record, &boot_flag,
				      &addr_mode, &vlan_id);

	 
	lpfc_sli4_log_fcf_record_info(phba, new_fcf_record, vlan_id,
				      next_fcf_index);

	fcf_index = bf_get(lpfc_fcf_record_fcf_index, new_fcf_record);
	if (!rc) {
		lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
				"2848 Remove ineligible FCF (x%x) from "
				"from roundrobin bmask\n", fcf_index);
		 
		lpfc_sli4_fcf_rr_index_clear(phba, fcf_index);
		 
		fcf_index = lpfc_sli4_fcf_rr_next_index_get(phba);
		rc = lpfc_sli4_fcf_rr_next_proc(phba->pport, fcf_index);
		if (rc)
			goto out;
		goto error_out;
	}

	if (fcf_index == phba->fcf.current_rec.fcf_indx) {
		lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
				"2760 Perform FLOGI roundrobin FCF failover: "
				"FCF (x%x) back to FCF (x%x)\n",
				phba->fcf.current_rec.fcf_indx, fcf_index);
		 
		msleep(500);
		lpfc_issue_init_vfi(phba->pport);
		goto out;
	}

	 
	lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
			"2834 Update current FCF (x%x) with new FCF (x%x)\n",
			phba->fcf.failover_rec.fcf_indx, fcf_index);
	spin_lock_irq(&phba->hbalock);
	__lpfc_update_fcf_record(phba, &phba->fcf.failover_rec,
				 new_fcf_record, addr_mode, vlan_id,
				 (boot_flag ? BOOT_ENABLE : 0));
	spin_unlock_irq(&phba->hbalock);

	current_fcf_index = phba->fcf.current_rec.fcf_indx;

	 
	lpfc_unregister_fcf(phba);

	 
	memcpy(&phba->fcf.current_rec, &phba->fcf.failover_rec,
	       sizeof(struct lpfc_fcf_rec));

	lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
			"2783 Perform FLOGI roundrobin FCF failover: FCF "
			"(x%x) to FCF (x%x)\n", current_fcf_index, fcf_index);

error_out:
	lpfc_register_fcf(phba);
out:
	lpfc_sli4_mbox_cmd_free(phba, mboxq);
}

 
void
lpfc_mbx_cmpl_read_fcf_rec(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	struct fcf_record *new_fcf_record;
	uint32_t boot_flag, addr_mode;
	uint16_t fcf_index, next_fcf_index;
	uint16_t vlan_id =  LPFC_FCOE_NULL_VID;
	int rc;

	 
	if (phba->link_state < LPFC_LINK_UP)
		goto out;

	 
	if (!(phba->fcf.fcf_flag & FCF_DISCOVERY))
		goto out;

	 
	new_fcf_record = lpfc_sli4_fcf_rec_mbox_parse(phba, mboxq,
						      &next_fcf_index);
	if (!new_fcf_record) {
		lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
				"2767 Mailbox command READ_FCF_RECORD "
				"failed to retrieve a FCF record.\n");
		goto out;
	}

	 
	rc = lpfc_match_fcf_conn_list(phba, new_fcf_record, &boot_flag,
				      &addr_mode, &vlan_id);

	 
	lpfc_sli4_log_fcf_record_info(phba, new_fcf_record, vlan_id,
				      next_fcf_index);

	if (!rc)
		goto out;

	 
	fcf_index = bf_get(lpfc_fcf_record_fcf_index, new_fcf_record);

	rc = lpfc_sli4_fcf_pri_list_add(phba, fcf_index, new_fcf_record);

out:
	lpfc_sli4_mbox_cmd_free(phba, mboxq);
}

 
static void
lpfc_init_vfi_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	struct lpfc_vport *vport = mboxq->vport;

	 
	if (mboxq->u.mb.mbxStatus &&
	    (bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) !=
			LPFC_SLI_INTF_IF_TYPE_0) &&
	    mboxq->u.mb.mbxStatus != MBX_VFI_IN_USE) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "2891 Init VFI mailbox failed 0x%x\n",
				 mboxq->u.mb.mbxStatus);
		mempool_free(mboxq, phba->mbox_mem_pool);
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
		return;
	}

	lpfc_initial_flogi(vport);
	mempool_free(mboxq, phba->mbox_mem_pool);
	return;
}

 
void
lpfc_issue_init_vfi(struct lpfc_vport *vport)
{
	LPFC_MBOXQ_t *mboxq;
	int rc;
	struct lpfc_hba *phba = vport->phba;

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_vlog(vport, KERN_ERR,
			LOG_TRACE_EVENT, "2892 Failed to allocate "
			"init_vfi mailbox\n");
		return;
	}
	lpfc_init_vfi(mboxq, vport);
	mboxq->mbox_cmpl = lpfc_init_vfi_cmpl;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "2893 Failed to issue init_vfi mailbox\n");
		mempool_free(mboxq, vport->phba->mbox_mem_pool);
	}
}

 
void
lpfc_init_vpi_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	struct lpfc_vport *vport = mboxq->vport;
	struct lpfc_nodelist *ndlp;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	if (mboxq->u.mb.mbxStatus) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "2609 Init VPI mailbox failed 0x%x\n",
				 mboxq->u.mb.mbxStatus);
		mempool_free(mboxq, phba->mbox_mem_pool);
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
		return;
	}
	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~FC_VPORT_NEEDS_INIT_VPI;
	spin_unlock_irq(shost->host_lock);

	 
	if ((phba->pport == vport) || (vport->port_state == LPFC_FDISC)) {
			ndlp = lpfc_findnode_did(vport, Fabric_DID);
			if (!ndlp)
				lpfc_printf_vlog(vport, KERN_ERR,
					LOG_TRACE_EVENT,
					"2731 Cannot find fabric "
					"controller node\n");
			else
				lpfc_register_new_vport(phba, vport, ndlp);
			mempool_free(mboxq, phba->mbox_mem_pool);
			return;
	}

	if (phba->link_flag & LS_NPIV_FAB_SUPPORTED)
		lpfc_initial_fdisc(vport);
	else {
		lpfc_vport_set_state(vport, FC_VPORT_NO_FABRIC_SUPP);
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "2606 No NPIV Fabric support\n");
	}
	mempool_free(mboxq, phba->mbox_mem_pool);
	return;
}

 
void
lpfc_issue_init_vpi(struct lpfc_vport *vport)
{
	LPFC_MBOXQ_t *mboxq;
	int rc, vpi;

	if ((vport->port_type != LPFC_PHYSICAL_PORT) && (!vport->vpi)) {
		vpi = lpfc_alloc_vpi(vport->phba);
		if (!vpi) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
					 "3303 Failed to obtain vport vpi\n");
			lpfc_vport_set_state(vport, FC_VPORT_FAILED);
			return;
		}
		vport->vpi = vpi;
	}

	mboxq = mempool_alloc(vport->phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_vlog(vport, KERN_ERR,
			LOG_TRACE_EVENT, "2607 Failed to allocate "
			"init_vpi mailbox\n");
		return;
	}
	lpfc_init_vpi(vport->phba, mboxq, vport->vpi);
	mboxq->vport = vport;
	mboxq->mbox_cmpl = lpfc_init_vpi_cmpl;
	rc = lpfc_sli_issue_mbox(vport->phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "2608 Failed to issue init_vpi mailbox\n");
		mempool_free(mboxq, vport->phba->mbox_mem_pool);
	}
}

 
void
lpfc_start_fdiscs(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	int i;

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL) {
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			if (vports[i]->port_type == LPFC_PHYSICAL_PORT)
				continue;
			 
			if (vports[i]->vpi > phba->max_vpi) {
				lpfc_vport_set_state(vports[i],
						     FC_VPORT_FAILED);
				continue;
			}
			if (phba->fc_topology == LPFC_TOPOLOGY_LOOP) {
				lpfc_vport_set_state(vports[i],
						     FC_VPORT_LINKDOWN);
				continue;
			}
			if (vports[i]->fc_flag & FC_VPORT_NEEDS_INIT_VPI) {
				lpfc_issue_init_vpi(vports[i]);
				continue;
			}
			if (phba->link_flag & LS_NPIV_FAB_SUPPORTED)
				lpfc_initial_fdisc(vports[i]);
			else {
				lpfc_vport_set_state(vports[i],
						     FC_VPORT_NO_FABRIC_SUPP);
				lpfc_printf_vlog(vports[i], KERN_ERR,
						 LOG_TRACE_EVENT,
						 "0259 No NPIV "
						 "Fabric support\n");
			}
		}
	}
	lpfc_destroy_vport_work_array(phba, vports);
}

void
lpfc_mbx_cmpl_reg_vfi(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	struct lpfc_vport *vport = mboxq->vport;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	 
	if (mboxq->u.mb.mbxStatus &&
	    (bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) !=
			LPFC_SLI_INTF_IF_TYPE_0) &&
	    mboxq->u.mb.mbxStatus != MBX_VFI_IN_USE) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "2018 REG_VFI mbxStatus error x%x "
				 "HBA state x%x\n",
				 mboxq->u.mb.mbxStatus, vport->port_state);
		if (phba->fc_topology == LPFC_TOPOLOGY_LOOP) {
			 
			lpfc_disc_list_loopmap(vport);
			 
			lpfc_disc_start(vport);
			goto out_free_mem;
		}
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
		goto out_free_mem;
	}

	 
	if (vport->fc_flag & FC_VFI_REGISTERED)
		if (!(phba->sli_rev == LPFC_SLI_REV4 &&
		      vport->fc_flag & FC_PT2PT))
			goto out_free_mem;

	 
	spin_lock_irq(shost->host_lock);
	vport->vpi_state |= LPFC_VPI_REGISTERED;
	vport->fc_flag |= FC_VFI_REGISTERED;
	vport->fc_flag &= ~FC_VPORT_NEEDS_REG_VPI;
	vport->fc_flag &= ~FC_VPORT_NEEDS_INIT_VPI;
	spin_unlock_irq(shost->host_lock);

	 
	if ((phba->sli_rev == LPFC_SLI_REV4) &&
	    (phba->link_flag & LS_LOOPBACK_MODE)) {
		phba->link_state = LPFC_HBA_READY;
		goto out_free_mem;
	}

	lpfc_printf_vlog(vport, KERN_INFO, LOG_SLI,
			 "3313 cmpl reg vfi  port_state:%x fc_flag:%x myDid:%x "
			 "alpacnt:%d LinkState:%x topology:%x\n",
			 vport->port_state, vport->fc_flag, vport->fc_myDID,
			 vport->phba->alpa_map[0],
			 phba->link_state, phba->fc_topology);

	if (vport->port_state == LPFC_FABRIC_CFG_LINK) {
		 
		if ((vport->fc_flag & FC_PT2PT) ||
		    ((phba->fc_topology == LPFC_TOPOLOGY_LOOP) &&
		    !(vport->fc_flag & FC_PUBLIC_LOOP))) {

			 
			lpfc_disc_list_loopmap(vport);
			 
			if (vport->fc_flag & FC_PT2PT)
				vport->port_state = LPFC_VPORT_READY;
			else
				lpfc_disc_start(vport);
		} else {
			lpfc_start_fdiscs(phba);
			lpfc_do_scr_ns_plogi(phba, vport);
		}
	}

out_free_mem:
	lpfc_mbox_rsrc_cleanup(phba, mboxq, MBOX_THD_UNLOCKED);
}

static void
lpfc_mbx_cmpl_read_sparam(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	MAILBOX_t *mb = &pmb->u.mb;
	struct lpfc_dmabuf *mp = (struct lpfc_dmabuf *)pmb->ctx_buf;
	struct lpfc_vport  *vport = pmb->vport;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct serv_parm *sp = &vport->fc_sparam;
	uint32_t ed_tov;

	 
	if (mb->mbxStatus) {
		 
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "0319 READ_SPARAM mbxStatus error x%x "
				 "hba state x%x>\n",
				 mb->mbxStatus, vport->port_state);
		lpfc_linkdown(phba);
		goto out;
	}

	memcpy((uint8_t *) &vport->fc_sparam, (uint8_t *) mp->virt,
	       sizeof (struct serv_parm));

	ed_tov = be32_to_cpu(sp->cmn.e_d_tov);
	if (sp->cmn.edtovResolution)	 
		ed_tov = (ed_tov + 999999) / 1000000;

	phba->fc_edtov = ed_tov;
	phba->fc_ratov = (2 * ed_tov) / 1000;
	if (phba->fc_ratov < FF_DEF_RATOV) {
		 
		phba->fc_ratov = FF_DEF_RATOV;
	}

	lpfc_update_vport_wwn(vport);
	fc_host_port_name(shost) = wwn_to_u64(vport->fc_portname.u.wwn);
	if (vport->port_type == LPFC_PHYSICAL_PORT) {
		memcpy(&phba->wwnn, &vport->fc_nodename, sizeof(phba->wwnn));
		memcpy(&phba->wwpn, &vport->fc_portname, sizeof(phba->wwnn));
	}

	lpfc_mbox_rsrc_cleanup(phba, pmb, MBOX_THD_UNLOCKED);

	 
	if (phba->hba_flag & HBA_DEFER_FLOGI) {
		lpfc_initial_flogi(vport);
		phba->hba_flag &= ~HBA_DEFER_FLOGI;
	}
	return;

out:
	lpfc_mbox_rsrc_cleanup(phba, pmb, MBOX_THD_UNLOCKED);
	lpfc_issue_clear_la(phba, vport);
}

static void
lpfc_mbx_process_link_up(struct lpfc_hba *phba, struct lpfc_mbx_read_top *la)
{
	struct lpfc_vport *vport = phba->pport;
	LPFC_MBOXQ_t *sparam_mbox, *cfglink_mbox = NULL;
	struct Scsi_Host *shost;
	int i;
	int rc;
	struct fcf_record *fcf_record;
	uint32_t fc_flags = 0;
	unsigned long iflags;

	spin_lock_irqsave(&phba->hbalock, iflags);
	phba->fc_linkspeed = bf_get(lpfc_mbx_read_top_link_spd, la);

	if (!(phba->hba_flag & HBA_FCOE_MODE)) {
		switch (bf_get(lpfc_mbx_read_top_link_spd, la)) {
		case LPFC_LINK_SPEED_1GHZ:
		case LPFC_LINK_SPEED_2GHZ:
		case LPFC_LINK_SPEED_4GHZ:
		case LPFC_LINK_SPEED_8GHZ:
		case LPFC_LINK_SPEED_10GHZ:
		case LPFC_LINK_SPEED_16GHZ:
		case LPFC_LINK_SPEED_32GHZ:
		case LPFC_LINK_SPEED_64GHZ:
		case LPFC_LINK_SPEED_128GHZ:
		case LPFC_LINK_SPEED_256GHZ:
			break;
		default:
			phba->fc_linkspeed = LPFC_LINK_SPEED_UNKNOWN;
			break;
		}
	}

	if (phba->fc_topology &&
	    phba->fc_topology != bf_get(lpfc_mbx_read_top_topology, la)) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"3314 Toplogy changed was 0x%x is 0x%x\n",
				phba->fc_topology,
				bf_get(lpfc_mbx_read_top_topology, la));
		phba->fc_topology_changed = 1;
	}

	phba->fc_topology = bf_get(lpfc_mbx_read_top_topology, la);
	phba->link_flag &= ~(LS_NPIV_FAB_SUPPORTED | LS_CT_VEN_RPA);

	shost = lpfc_shost_from_vport(vport);
	if (phba->fc_topology == LPFC_TOPOLOGY_LOOP) {
		phba->sli3_options &= ~LPFC_SLI3_NPIV_ENABLED;

		 
		if (phba->cfg_enable_npiv && phba->max_vpi)
			lpfc_printf_log(phba, KERN_ERR, LOG_LINK_EVENT,
				"1309 Link Up Event npiv not supported in loop "
				"topology\n");
				 
		if (bf_get(lpfc_mbx_read_top_il, la))
			fc_flags |= FC_LBIT;

		vport->fc_myDID = bf_get(lpfc_mbx_read_top_alpa_granted, la);
		i = la->lilpBde64.tus.f.bdeSize;

		if (i == 0) {
			phba->alpa_map[0] = 0;
		} else {
			if (vport->cfg_log_verbose & LOG_LINK_EVENT) {
				int numalpa, j, k;
				union {
					uint8_t pamap[16];
					struct {
						uint32_t wd1;
						uint32_t wd2;
						uint32_t wd3;
						uint32_t wd4;
					} pa;
				} un;
				numalpa = phba->alpa_map[0];
				j = 0;
				while (j < numalpa) {
					memset(un.pamap, 0, 16);
					for (k = 1; j < numalpa; k++) {
						un.pamap[k - 1] =
							phba->alpa_map[j + 1];
						j++;
						if (k == 16)
							break;
					}
					 
					lpfc_printf_log(phba,
							KERN_WARNING,
							LOG_LINK_EVENT,
							"1304 Link Up Event "
							"ALPA map Data: x%x "
							"x%x x%x x%x\n",
							un.pa.wd1, un.pa.wd2,
							un.pa.wd3, un.pa.wd4);
				}
			}
		}
	} else {
		if (!(phba->sli3_options & LPFC_SLI3_NPIV_ENABLED)) {
			if (phba->max_vpi && phba->cfg_enable_npiv &&
			   (phba->sli_rev >= LPFC_SLI_REV3))
				phba->sli3_options |= LPFC_SLI3_NPIV_ENABLED;
		}
		vport->fc_myDID = phba->fc_pref_DID;
		fc_flags |= FC_LBIT;
	}
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	if (fc_flags) {
		spin_lock_irqsave(shost->host_lock, iflags);
		vport->fc_flag |= fc_flags;
		spin_unlock_irqrestore(shost->host_lock, iflags);
	}

	lpfc_linkup(phba);
	sparam_mbox = NULL;

	sparam_mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!sparam_mbox)
		goto out;

	rc = lpfc_read_sparam(phba, sparam_mbox, 0);
	if (rc) {
		mempool_free(sparam_mbox, phba->mbox_mem_pool);
		goto out;
	}
	sparam_mbox->vport = vport;
	sparam_mbox->mbox_cmpl = lpfc_mbx_cmpl_read_sparam;
	rc = lpfc_sli_issue_mbox(phba, sparam_mbox, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		lpfc_mbox_rsrc_cleanup(phba, sparam_mbox, MBOX_THD_UNLOCKED);
		goto out;
	}

	if (!(phba->hba_flag & HBA_FCOE_MODE)) {
		cfglink_mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
		if (!cfglink_mbox)
			goto out;
		vport->port_state = LPFC_LOCAL_CFG_LINK;
		lpfc_config_link(phba, cfglink_mbox);
		cfglink_mbox->vport = vport;
		cfglink_mbox->mbox_cmpl = lpfc_mbx_cmpl_local_config_link;
		rc = lpfc_sli_issue_mbox(phba, cfglink_mbox, MBX_NOWAIT);
		if (rc == MBX_NOT_FINISHED) {
			mempool_free(cfglink_mbox, phba->mbox_mem_pool);
			goto out;
		}
	} else {
		vport->port_state = LPFC_VPORT_UNKNOWN;
		 
		if (!(phba->hba_flag & HBA_FIP_SUPPORT)) {
			fcf_record = kzalloc(sizeof(struct fcf_record),
					GFP_KERNEL);
			if (unlikely(!fcf_record)) {
				lpfc_printf_log(phba, KERN_ERR,
					LOG_TRACE_EVENT,
					"2554 Could not allocate memory for "
					"fcf record\n");
				rc = -ENODEV;
				goto out;
			}

			lpfc_sli4_build_dflt_fcf_record(phba, fcf_record,
						LPFC_FCOE_FCF_DEF_INDEX);
			rc = lpfc_sli4_add_fcf_record(phba, fcf_record);
			if (unlikely(rc)) {
				lpfc_printf_log(phba, KERN_ERR,
					LOG_TRACE_EVENT,
					"2013 Could not manually add FCF "
					"record 0, status %d\n", rc);
				rc = -ENODEV;
				kfree(fcf_record);
				goto out;
			}
			kfree(fcf_record);
		}
		 
		spin_lock_irqsave(&phba->hbalock, iflags);
		if (phba->hba_flag & FCF_TS_INPROG) {
			spin_unlock_irqrestore(&phba->hbalock, iflags);
			return;
		}
		 
		phba->fcf.fcf_flag |= FCF_INIT_DISC;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		lpfc_printf_log(phba, KERN_INFO, LOG_FIP | LOG_DISCOVERY,
				"2778 Start FCF table scan at linkup\n");
		rc = lpfc_sli4_fcf_scan_read_fcf_rec(phba,
						     LPFC_FCOE_FCF_GET_FIRST);
		if (rc) {
			spin_lock_irqsave(&phba->hbalock, iflags);
			phba->fcf.fcf_flag &= ~FCF_INIT_DISC;
			spin_unlock_irqrestore(&phba->hbalock, iflags);
			goto out;
		}
		 
		lpfc_sli4_clear_fcf_rr_bmask(phba);
	}

	 
	memset(phba->os_host_name, 0, sizeof(phba->os_host_name));
	scnprintf(phba->os_host_name, sizeof(phba->os_host_name), "%s",
		  init_utsname()->nodename);
	return;
out:
	lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
			 "0263 Discovery Mailbox error: state: 0x%x : x%px x%px\n",
			 vport->port_state, sparam_mbox, cfglink_mbox);
	lpfc_issue_clear_la(phba, vport);
	return;
}

static void
lpfc_enable_la(struct lpfc_hba *phba)
{
	uint32_t control;
	struct lpfc_sli *psli = &phba->sli;
	spin_lock_irq(&phba->hbalock);
	psli->sli_flag |= LPFC_PROCESS_LA;
	if (phba->sli_rev <= LPFC_SLI_REV3) {
		control = readl(phba->HCregaddr);
		control |= HC_LAINT_ENA;
		writel(control, phba->HCregaddr);
		readl(phba->HCregaddr);  
	}
	spin_unlock_irq(&phba->hbalock);
}

static void
lpfc_mbx_issue_link_down(struct lpfc_hba *phba)
{
	lpfc_linkdown(phba);
	lpfc_enable_la(phba);
	lpfc_unregister_unused_fcf(phba);
	 
}


 
void
lpfc_mbx_cmpl_read_topology(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport *vport = pmb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	struct lpfc_mbx_read_top *la;
	struct lpfc_sli_ring *pring;
	MAILBOX_t *mb = &pmb->u.mb;
	struct lpfc_dmabuf *mp = (struct lpfc_dmabuf *)(pmb->ctx_buf);
	uint8_t attn_type;
	unsigned long iflags;

	 
	pring = lpfc_phba_elsring(phba);
	if (pring)
		pring->flag &= ~LPFC_STOP_IOCB_EVENT;

	 
	if (mb->mbxStatus) {
		lpfc_printf_log(phba, KERN_INFO, LOG_LINK_EVENT,
				"1307 READ_LA mbox error x%x state x%x\n",
				mb->mbxStatus, vport->port_state);
		lpfc_mbx_issue_link_down(phba);
		phba->link_state = LPFC_HBA_ERROR;
		goto lpfc_mbx_cmpl_read_topology_free_mbuf;
	}

	la = (struct lpfc_mbx_read_top *) &pmb->u.mb.un.varReadTop;
	attn_type = bf_get(lpfc_mbx_read_top_att_type, la);

	memcpy(&phba->alpa_map[0], mp->virt, 128);

	spin_lock_irqsave(shost->host_lock, iflags);
	if (bf_get(lpfc_mbx_read_top_pb, la))
		vport->fc_flag |= FC_BYPASSED_MODE;
	else
		vport->fc_flag &= ~FC_BYPASSED_MODE;
	spin_unlock_irqrestore(shost->host_lock, iflags);

	if (phba->fc_eventTag <= la->eventTag) {
		phba->fc_stat.LinkMultiEvent++;
		if (attn_type == LPFC_ATT_LINK_UP)
			if (phba->fc_eventTag != 0)
				lpfc_linkdown(phba);
	}

	phba->fc_eventTag = la->eventTag;
	phba->link_events++;
	if (attn_type == LPFC_ATT_LINK_UP) {
		phba->fc_stat.LinkUp++;
		if (phba->link_flag & LS_LOOPBACK_MODE) {
			lpfc_printf_log(phba, KERN_ERR, LOG_LINK_EVENT,
					"1306 Link Up Event in loop back mode "
					"x%x received Data: x%x x%x x%x x%x\n",
					la->eventTag, phba->fc_eventTag,
					bf_get(lpfc_mbx_read_top_alpa_granted,
					       la),
					bf_get(lpfc_mbx_read_top_link_spd, la),
					phba->alpa_map[0]);
		} else {
			lpfc_printf_log(phba, KERN_ERR, LOG_LINK_EVENT,
					"1303 Link Up Event x%x received "
					"Data: x%x x%x x%x x%x x%x\n",
					la->eventTag, phba->fc_eventTag,
					bf_get(lpfc_mbx_read_top_alpa_granted,
					       la),
					bf_get(lpfc_mbx_read_top_link_spd, la),
					phba->alpa_map[0],
					bf_get(lpfc_mbx_read_top_fa, la));
		}
		lpfc_mbx_process_link_up(phba, la);

		if (phba->cmf_active_mode != LPFC_CFG_OFF)
			lpfc_cmf_signal_init(phba);

		if (phba->lmt & LMT_64Gb)
			lpfc_read_lds_params(phba);

	} else if (attn_type == LPFC_ATT_LINK_DOWN ||
		   attn_type == LPFC_ATT_UNEXP_WWPN) {
		phba->fc_stat.LinkDown++;
		if (phba->link_flag & LS_LOOPBACK_MODE)
			lpfc_printf_log(phba, KERN_ERR, LOG_LINK_EVENT,
				"1308 Link Down Event in loop back mode "
				"x%x received "
				"Data: x%x x%x x%x\n",
				la->eventTag, phba->fc_eventTag,
				phba->pport->port_state, vport->fc_flag);
		else if (attn_type == LPFC_ATT_UNEXP_WWPN)
			lpfc_printf_log(phba, KERN_ERR, LOG_LINK_EVENT,
				"1313 Link Down Unexpected FA WWPN Event x%x "
				"received Data: x%x x%x x%x x%x\n",
				la->eventTag, phba->fc_eventTag,
				phba->pport->port_state, vport->fc_flag,
				bf_get(lpfc_mbx_read_top_fa, la));
		else
			lpfc_printf_log(phba, KERN_ERR, LOG_LINK_EVENT,
				"1305 Link Down Event x%x received "
				"Data: x%x x%x x%x x%x\n",
				la->eventTag, phba->fc_eventTag,
				phba->pport->port_state, vport->fc_flag,
				bf_get(lpfc_mbx_read_top_fa, la));
		lpfc_mbx_issue_link_down(phba);
	}

	if ((phba->sli_rev < LPFC_SLI_REV4) &&
	    bf_get(lpfc_mbx_read_top_fa, la))
		lpfc_printf_log(phba, KERN_INFO, LOG_LINK_EVENT,
				"1311 fa %d\n",
				bf_get(lpfc_mbx_read_top_fa, la));

lpfc_mbx_cmpl_read_topology_free_mbuf:
	lpfc_mbox_rsrc_cleanup(phba, pmb, MBOX_THD_UNLOCKED);
}

 
void
lpfc_mbx_cmpl_reg_login(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport  *vport = pmb->vport;
	struct lpfc_dmabuf *mp = (struct lpfc_dmabuf *)pmb->ctx_buf;
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *)pmb->ctx_ndlp;

	 
	pmb->ctx_buf = NULL;
	pmb->ctx_ndlp = NULL;

	lpfc_printf_vlog(vport, KERN_INFO, LOG_SLI | LOG_NODE | LOG_DISCOVERY,
			 "0002 rpi:%x DID:%x flg:%x %d x%px\n",
			 ndlp->nlp_rpi, ndlp->nlp_DID, ndlp->nlp_flag,
			 kref_read(&ndlp->kref),
			 ndlp);
	if (ndlp->nlp_flag & NLP_REG_LOGIN_SEND)
		ndlp->nlp_flag &= ~NLP_REG_LOGIN_SEND;

	if (ndlp->nlp_flag & NLP_IGNR_REG_CMPL ||
	    ndlp->nlp_state != NLP_STE_REG_LOGIN_ISSUE) {
		 
		spin_lock_irq(&ndlp->lock);
		ndlp->nlp_flag &= ~NLP_IGNR_REG_CMPL;
		spin_unlock_irq(&ndlp->lock);

		 
		ndlp->nlp_flag |= NLP_RPI_REGISTERED;
		lpfc_unreg_rpi(vport, ndlp);
	}

	 
	lpfc_disc_state_machine(vport, ndlp, pmb, NLP_EVT_CMPL_REG_LOGIN);
	pmb->ctx_buf = mp;
	lpfc_mbox_rsrc_cleanup(phba, pmb, MBOX_THD_UNLOCKED);

	 
	lpfc_nlp_put(ndlp);

	return;
}

static void
lpfc_mbx_cmpl_unreg_vpi(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	MAILBOX_t *mb = &pmb->u.mb;
	struct lpfc_vport *vport = pmb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);

	switch (mb->mbxStatus) {
	case 0x0011:
	case 0x0020:
		lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE,
				 "0911 cmpl_unreg_vpi, mb status = 0x%x\n",
				 mb->mbxStatus);
		break;
	 
	case 0x9700:
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
			"2798 Unreg_vpi failed vpi 0x%x, mb status = 0x%x\n",
			vport->vpi, mb->mbxStatus);
		if (!(phba->pport->load_flag & FC_UNLOADING))
			lpfc_workq_post_event(phba, NULL, NULL,
				LPFC_EVT_RESET_HBA);
	}
	spin_lock_irq(shost->host_lock);
	vport->vpi_state &= ~LPFC_VPI_REGISTERED;
	vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
	spin_unlock_irq(shost->host_lock);
	mempool_free(pmb, phba->mbox_mem_pool);
	lpfc_cleanup_vports_rrqs(vport, NULL);
	 
	if ((vport->load_flag & FC_UNLOADING) && (vport != phba->pport))
		scsi_host_put(shost);
}

int
lpfc_mbx_unreg_vpi(struct lpfc_vport *vport)
{
	struct lpfc_hba  *phba = vport->phba;
	LPFC_MBOXQ_t *mbox;
	int rc;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return 1;

	lpfc_unreg_vpi(phba, vport->vpi, mbox);
	mbox->vport = vport;
	mbox->mbox_cmpl = lpfc_mbx_cmpl_unreg_vpi;
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "1800 Could not issue unreg_vpi\n");
		mempool_free(mbox, phba->mbox_mem_pool);
		return rc;
	}
	return 0;
}

static void
lpfc_mbx_cmpl_reg_vpi(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport *vport = pmb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	MAILBOX_t *mb = &pmb->u.mb;

	switch (mb->mbxStatus) {
	case 0x0011:
	case 0x9601:
	case 0x9602:
		lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE,
				 "0912 cmpl_reg_vpi, mb status = 0x%x\n",
				 mb->mbxStatus);
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
		spin_lock_irq(shost->host_lock);
		vport->fc_flag &= ~(FC_FABRIC | FC_PUBLIC_LOOP);
		spin_unlock_irq(shost->host_lock);
		vport->fc_myDID = 0;

		if ((vport->cfg_enable_fc4_type == LPFC_ENABLE_BOTH) ||
		    (vport->cfg_enable_fc4_type == LPFC_ENABLE_NVME)) {
			if (phba->nvmet_support)
				lpfc_nvmet_update_targetport(phba);
			else
				lpfc_nvme_update_localport(vport);
		}
		goto out;
	}

	spin_lock_irq(shost->host_lock);
	vport->vpi_state |= LPFC_VPI_REGISTERED;
	vport->fc_flag &= ~FC_VPORT_NEEDS_REG_VPI;
	spin_unlock_irq(shost->host_lock);
	vport->num_disc_nodes = 0;
	 
	if (vport->fc_npr_cnt)
		lpfc_els_disc_plogi(vport);

	if (!vport->num_disc_nodes) {
		spin_lock_irq(shost->host_lock);
		vport->fc_flag &= ~FC_NDISC_ACTIVE;
		spin_unlock_irq(shost->host_lock);
		lpfc_can_disctmo(vport);
	}
	vport->port_state = LPFC_VPORT_READY;

out:
	mempool_free(pmb, phba->mbox_mem_pool);
	return;
}

 
void
lpfc_create_static_vport(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *pmb = NULL;
	MAILBOX_t *mb;
	struct static_vport_info *vport_info;
	int mbx_wait_rc = 0, i;
	struct fc_vport_identifiers vport_id;
	struct fc_vport *new_fc_vport;
	struct Scsi_Host *shost;
	struct lpfc_vport *vport;
	uint16_t offset = 0;
	uint8_t *vport_buff;
	struct lpfc_dmabuf *mp;
	uint32_t byte_count = 0;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0542 lpfc_create_static_vport failed to"
				" allocate mailbox memory\n");
		return;
	}
	memset(pmb, 0, sizeof(LPFC_MBOXQ_t));
	mb = &pmb->u.mb;

	vport_info = kzalloc(sizeof(struct static_vport_info), GFP_KERNEL);
	if (!vport_info) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0543 lpfc_create_static_vport failed to"
				" allocate vport_info\n");
		mempool_free(pmb, phba->mbox_mem_pool);
		return;
	}

	vport_buff = (uint8_t *) vport_info;
	do {
		 
		if (pmb->ctx_buf) {
			mp = (struct lpfc_dmabuf *)pmb->ctx_buf;
			lpfc_mbuf_free(phba, mp->virt, mp->phys);
			kfree(mp);
			pmb->ctx_buf = NULL;
		}
		if (lpfc_dump_static_vport(phba, pmb, offset))
			goto out;

		pmb->vport = phba->pport;
		mbx_wait_rc = lpfc_sli_issue_mbox_wait(phba, pmb,
							LPFC_MBOX_TMO);

		if ((mbx_wait_rc != MBX_SUCCESS) || mb->mbxStatus) {
			lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"0544 lpfc_create_static_vport failed to"
				" issue dump mailbox command ret 0x%x "
				"status 0x%x\n",
				mbx_wait_rc, mb->mbxStatus);
			goto out;
		}

		if (phba->sli_rev == LPFC_SLI_REV4) {
			byte_count = pmb->u.mqe.un.mb_words[5];
			mp = (struct lpfc_dmabuf *)pmb->ctx_buf;
			if (byte_count > sizeof(struct static_vport_info) -
					offset)
				byte_count = sizeof(struct static_vport_info)
					- offset;
			memcpy(vport_buff + offset, mp->virt, byte_count);
			offset += byte_count;
		} else {
			if (mb->un.varDmp.word_cnt >
				sizeof(struct static_vport_info) - offset)
				mb->un.varDmp.word_cnt =
					sizeof(struct static_vport_info)
						- offset;
			byte_count = mb->un.varDmp.word_cnt;
			lpfc_sli_pcimem_bcopy(((uint8_t *)mb) + DMP_RSP_OFFSET,
				vport_buff + offset,
				byte_count);

			offset += byte_count;
		}

	} while (byte_count &&
		offset < sizeof(struct static_vport_info));


	if ((le32_to_cpu(vport_info->signature) != VPORT_INFO_SIG) ||
		((le32_to_cpu(vport_info->rev) & VPORT_INFO_REV_MASK)
			!= VPORT_INFO_REV)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0545 lpfc_create_static_vport bad"
				" information header 0x%x 0x%x\n",
				le32_to_cpu(vport_info->signature),
				le32_to_cpu(vport_info->rev) &
				VPORT_INFO_REV_MASK);

		goto out;
	}

	shost = lpfc_shost_from_vport(phba->pport);

	for (i = 0; i < MAX_STATIC_VPORT_COUNT; i++) {
		memset(&vport_id, 0, sizeof(vport_id));
		vport_id.port_name = wwn_to_u64(vport_info->vport_list[i].wwpn);
		vport_id.node_name = wwn_to_u64(vport_info->vport_list[i].wwnn);
		if (!vport_id.port_name || !vport_id.node_name)
			continue;

		vport_id.roles = FC_PORT_ROLE_FCP_INITIATOR;
		vport_id.vport_type = FC_PORTTYPE_NPIV;
		vport_id.disable = false;
		new_fc_vport = fc_vport_create(shost, 0, &vport_id);

		if (!new_fc_vport) {
			lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"0546 lpfc_create_static_vport failed to"
				" create vport\n");
			continue;
		}

		vport = *(struct lpfc_vport **)new_fc_vport->dd_data;
		vport->vport_flag |= STATIC_VPORT;
	}

out:
	kfree(vport_info);
	if (mbx_wait_rc != MBX_TIMEOUT)
		lpfc_mbox_rsrc_cleanup(phba, pmb, MBOX_THD_UNLOCKED);
}

 
void
lpfc_mbx_cmpl_fabric_reg_login(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport *vport = pmb->vport;
	MAILBOX_t *mb = &pmb->u.mb;
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *)pmb->ctx_ndlp;
	struct Scsi_Host *shost;

	pmb->ctx_ndlp = NULL;

	if (mb->mbxStatus) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "0258 Register Fabric login error: 0x%x\n",
				 mb->mbxStatus);
		lpfc_mbox_rsrc_cleanup(phba, pmb, MBOX_THD_UNLOCKED);
		if (phba->fc_topology == LPFC_TOPOLOGY_LOOP) {
			 
			lpfc_disc_list_loopmap(vport);

			 
			lpfc_disc_start(vport);
			 
			lpfc_nlp_put(ndlp);
			return;
		}

		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
		 
		lpfc_nlp_put(ndlp);
		return;
	}

	if (phba->sli_rev < LPFC_SLI_REV4)
		ndlp->nlp_rpi = mb->un.varWords[0];
	ndlp->nlp_flag |= NLP_RPI_REGISTERED;
	ndlp->nlp_type |= NLP_FABRIC;
	lpfc_nlp_set_state(vport, ndlp, NLP_STE_UNMAPPED_NODE);

	if (vport->port_state == LPFC_FABRIC_CFG_LINK) {
		 
		if (!(vport->fc_flag & FC_LOGO_RCVD_DID_CHNG))
			lpfc_start_fdiscs(phba);
		else {
			shost = lpfc_shost_from_vport(vport);
			spin_lock_irq(shost->host_lock);
			vport->fc_flag &= ~FC_LOGO_RCVD_DID_CHNG ;
			spin_unlock_irq(shost->host_lock);
		}
		lpfc_do_scr_ns_plogi(phba, vport);
	}

	lpfc_mbox_rsrc_cleanup(phba, pmb, MBOX_THD_UNLOCKED);

	 
	lpfc_nlp_put(ndlp);
	return;
}

  
int
lpfc_issue_gidft(struct lpfc_vport *vport)
{
	 
	if ((vport->cfg_enable_fc4_type == LPFC_ENABLE_BOTH) ||
	    (vport->cfg_enable_fc4_type == LPFC_ENABLE_FCP)) {
		if (lpfc_ns_cmd(vport, SLI_CTNS_GID_FT, 0, SLI_CTPT_FCP)) {
			 
			lpfc_printf_vlog(vport, KERN_ERR,
					 LOG_TRACE_EVENT,
					 "0604 %s FC TYPE %x %s\n",
					 "Failed to issue GID_FT to ",
					 FC_TYPE_FCP,
					 "Finishing discovery.");
			return 0;
		}
		vport->gidft_inp++;
	}

	if ((vport->cfg_enable_fc4_type == LPFC_ENABLE_BOTH) ||
	    (vport->cfg_enable_fc4_type == LPFC_ENABLE_NVME)) {
		if (lpfc_ns_cmd(vport, SLI_CTNS_GID_FT, 0, SLI_CTPT_NVME)) {
			 
			lpfc_printf_vlog(vport, KERN_ERR,
					 LOG_TRACE_EVENT,
					 "0605 %s FC_TYPE %x %s %d\n",
					 "Failed to issue GID_FT to ",
					 FC_TYPE_NVME,
					 "Finishing discovery: gidftinp ",
					 vport->gidft_inp);
			if (vport->gidft_inp == 0)
				return 0;
		} else
			vport->gidft_inp++;
	}
	return vport->gidft_inp;
}

 
int
lpfc_issue_gidpt(struct lpfc_vport *vport)
{
	 
	if (lpfc_ns_cmd(vport, SLI_CTNS_GID_PT, 0, GID_PT_N_PORT)) {
		 
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "0606 %s Port TYPE %x %s\n",
				 "Failed to issue GID_PT to ",
				 GID_PT_N_PORT,
				 "Finishing discovery.");
		return 0;
	}
	vport->gidft_inp++;
	return 1;
}

 
void
lpfc_mbx_cmpl_ns_reg_login(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	MAILBOX_t *mb = &pmb->u.mb;
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *)pmb->ctx_ndlp;
	struct lpfc_vport *vport = pmb->vport;
	int rc;

	pmb->ctx_ndlp = NULL;
	vport->gidft_inp = 0;

	if (mb->mbxStatus) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "0260 Register NameServer error: 0x%x\n",
				 mb->mbxStatus);

out:
		 
		lpfc_nlp_put(ndlp);
		lpfc_mbox_rsrc_cleanup(phba, pmb, MBOX_THD_UNLOCKED);

		 
		if (!(ndlp->fc4_xpt_flags & (SCSI_XPT_REGD | NVME_XPT_REGD))) {
			spin_lock_irq(&ndlp->lock);
			ndlp->nlp_flag &= ~NLP_NPR_2B_DISC;
			spin_unlock_irq(&ndlp->lock);
			lpfc_nlp_put(ndlp);
		}

		if (phba->fc_topology == LPFC_TOPOLOGY_LOOP) {
			 
			lpfc_disc_list_loopmap(vport);

			 
			lpfc_disc_start(vport);
			return;
		}
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
		return;
	}

	if (phba->sli_rev < LPFC_SLI_REV4)
		ndlp->nlp_rpi = mb->un.varWords[0];
	ndlp->nlp_flag |= NLP_RPI_REGISTERED;
	ndlp->nlp_type |= NLP_FABRIC;
	lpfc_nlp_set_state(vport, ndlp, NLP_STE_UNMAPPED_NODE);
	lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE | LOG_DISCOVERY,
			 "0003 rpi:%x DID:%x flg:%x %d x%px\n",
			 ndlp->nlp_rpi, ndlp->nlp_DID, ndlp->nlp_flag,
			 kref_read(&ndlp->kref),
			 ndlp);

	if (vport->port_state < LPFC_VPORT_READY) {
		 
		lpfc_ns_cmd(vport, SLI_CTNS_RNN_ID, 0, 0);
		lpfc_ns_cmd(vport, SLI_CTNS_RSNN_NN, 0, 0);
		lpfc_ns_cmd(vport, SLI_CTNS_RSPN_ID, 0, 0);
		lpfc_ns_cmd(vport, SLI_CTNS_RFT_ID, 0, 0);

		if ((vport->cfg_enable_fc4_type == LPFC_ENABLE_BOTH) ||
		    (vport->cfg_enable_fc4_type == LPFC_ENABLE_FCP))
			lpfc_ns_cmd(vport, SLI_CTNS_RFF_ID, 0, FC_TYPE_FCP);

		if ((vport->cfg_enable_fc4_type == LPFC_ENABLE_BOTH) ||
		    (vport->cfg_enable_fc4_type == LPFC_ENABLE_NVME))
			lpfc_ns_cmd(vport, SLI_CTNS_RFF_ID, 0,
				    FC_TYPE_NVME);

		 
		lpfc_issue_els_scr(vport, 0);

		 
		if (phba->cmf_active_mode != LPFC_CFG_OFF) {
			phba->cgn_reg_fpin = phba->cgn_init_reg_fpin;
			phba->cgn_reg_signal = phba->cgn_init_reg_signal;
			rc = lpfc_issue_els_edc(vport, 0);
			lpfc_printf_log(phba, KERN_INFO,
					LOG_INIT | LOG_ELS | LOG_DISCOVERY,
					"4220 Issue EDC status x%x Data x%x\n",
					rc, phba->cgn_init_reg_signal);
		} else if (phba->lmt & LMT_64Gb) {
			 
			lpfc_issue_els_edc(vport, 0);
		} else {
			lpfc_issue_els_rdf(vport, 0);
		}
	}

	vport->fc_ns_retry = 0;
	if (lpfc_issue_gidft(vport) == 0)
		goto out;

	 
	lpfc_nlp_put(ndlp);
	lpfc_mbox_rsrc_cleanup(phba, pmb, MBOX_THD_UNLOCKED);
	return;
}

 
void
lpfc_mbx_cmpl_fc_reg_login(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport *vport = pmb->vport;
	MAILBOX_t *mb = &pmb->u.mb;
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *)pmb->ctx_ndlp;

	pmb->ctx_ndlp = NULL;
	if (mb->mbxStatus) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "0933 %s: Register FC login error: 0x%x\n",
				 __func__, mb->mbxStatus);
		goto out;
	}

	lpfc_check_nlp_post_devloss(vport, ndlp);

	if (phba->sli_rev < LPFC_SLI_REV4)
		ndlp->nlp_rpi = mb->un.varWords[0];

	lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE,
			 "0934 %s: Complete FC x%x RegLogin rpi x%x ste x%x\n",
			 __func__, ndlp->nlp_DID, ndlp->nlp_rpi,
			 ndlp->nlp_state);

	ndlp->nlp_flag |= NLP_RPI_REGISTERED;
	ndlp->nlp_flag &= ~NLP_REG_LOGIN_SEND;
	ndlp->nlp_type |= NLP_FABRIC;
	lpfc_nlp_set_state(vport, ndlp, NLP_STE_UNMAPPED_NODE);

 out:
	lpfc_mbox_rsrc_cleanup(phba, pmb, MBOX_THD_UNLOCKED);

	 
	lpfc_nlp_put(ndlp);
}

static void
lpfc_register_remote_port(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct fc_rport  *rport;
	struct lpfc_rport_data *rdata;
	struct fc_rport_identifiers rport_ids;
	struct lpfc_hba  *phba = vport->phba;
	unsigned long flags;

	if (vport->cfg_enable_fc4_type == LPFC_ENABLE_NVME)
		return;

	 
	rport_ids.node_name = wwn_to_u64(ndlp->nlp_nodename.u.wwn);
	rport_ids.port_name = wwn_to_u64(ndlp->nlp_portname.u.wwn);
	rport_ids.port_id = ndlp->nlp_DID;
	rport_ids.roles = FC_RPORT_ROLE_UNKNOWN;


	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_RPORT,
			      "rport add:       did:x%x flg:x%x type x%x",
			      ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_type);

	 
	if (vport->load_flag & FC_UNLOADING)
		return;

	ndlp->rport = rport = fc_remote_port_add(shost, 0, &rport_ids);
	if (!rport) {
		dev_printk(KERN_WARNING, &phba->pcidev->dev,
			   "Warning: fc_remote_port_add failed\n");
		return;
	}

	 
	rport->maxframe_size = ndlp->nlp_maxframe;
	rport->supported_classes = ndlp->nlp_class_sup;
	rdata = rport->dd_data;
	rdata->pnode = lpfc_nlp_get(ndlp);
	if (!rdata->pnode) {
		dev_warn(&phba->pcidev->dev,
			 "Warning - node ref failed. Unreg rport\n");
		fc_remote_port_delete(rport);
		ndlp->rport = NULL;
		return;
	}

	spin_lock_irqsave(&ndlp->lock, flags);
	ndlp->fc4_xpt_flags |= SCSI_XPT_REGD;
	spin_unlock_irqrestore(&ndlp->lock, flags);

	if (ndlp->nlp_type & NLP_FCP_TARGET)
		rport_ids.roles |= FC_PORT_ROLE_FCP_TARGET;
	if (ndlp->nlp_type & NLP_FCP_INITIATOR)
		rport_ids.roles |= FC_PORT_ROLE_FCP_INITIATOR;
	if (ndlp->nlp_type & NLP_NVME_INITIATOR)
		rport_ids.roles |= FC_PORT_ROLE_NVME_INITIATOR;
	if (ndlp->nlp_type & NLP_NVME_TARGET)
		rport_ids.roles |= FC_PORT_ROLE_NVME_TARGET;
	if (ndlp->nlp_type & NLP_NVME_DISCOVERY)
		rport_ids.roles |= FC_PORT_ROLE_NVME_DISCOVERY;

	if (rport_ids.roles !=  FC_RPORT_ROLE_UNKNOWN)
		fc_remote_port_rolechg(rport, rport_ids.roles);

	lpfc_printf_vlog(ndlp->vport, KERN_INFO, LOG_NODE,
			 "3183 %s rport x%px DID x%x, role x%x refcnt %d\n",
			 __func__, rport, rport->port_id, rport->roles,
			 kref_read(&ndlp->kref));

	if ((rport->scsi_target_id != -1) &&
	    (rport->scsi_target_id < LPFC_MAX_TARGET)) {
		ndlp->nlp_sid = rport->scsi_target_id;
	}

	return;
}

static void
lpfc_unregister_remote_port(struct lpfc_nodelist *ndlp)
{
	struct fc_rport *rport = ndlp->rport;
	struct lpfc_vport *vport = ndlp->vport;

	if (vport->cfg_enable_fc4_type == LPFC_ENABLE_NVME)
		return;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_RPORT,
		"rport delete:    did:x%x flg:x%x type x%x",
		ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_type);

	lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE,
			 "3184 rport unregister x%06x, rport x%px "
			 "xptflg x%x refcnt %d\n",
			 ndlp->nlp_DID, rport, ndlp->fc4_xpt_flags,
			 kref_read(&ndlp->kref));

	fc_remote_port_delete(rport);
	lpfc_nlp_put(ndlp);
}

static void
lpfc_nlp_counters(struct lpfc_vport *vport, int state, int count)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	unsigned long iflags;

	spin_lock_irqsave(shost->host_lock, iflags);
	switch (state) {
	case NLP_STE_UNUSED_NODE:
		vport->fc_unused_cnt += count;
		break;
	case NLP_STE_PLOGI_ISSUE:
		vport->fc_plogi_cnt += count;
		break;
	case NLP_STE_ADISC_ISSUE:
		vport->fc_adisc_cnt += count;
		break;
	case NLP_STE_REG_LOGIN_ISSUE:
		vport->fc_reglogin_cnt += count;
		break;
	case NLP_STE_PRLI_ISSUE:
		vport->fc_prli_cnt += count;
		break;
	case NLP_STE_UNMAPPED_NODE:
		vport->fc_unmap_cnt += count;
		break;
	case NLP_STE_MAPPED_NODE:
		vport->fc_map_cnt += count;
		break;
	case NLP_STE_NPR_NODE:
		if (vport->fc_npr_cnt == 0 && count == -1)
			vport->fc_npr_cnt = 0;
		else
			vport->fc_npr_cnt += count;
		break;
	}
	spin_unlock_irqrestore(shost->host_lock, iflags);
}

 
void
lpfc_nlp_reg_node(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	unsigned long iflags;

	lpfc_check_nlp_post_devloss(vport, ndlp);

	spin_lock_irqsave(&ndlp->lock, iflags);
	if (ndlp->fc4_xpt_flags & NLP_XPT_REGD) {
		 
		spin_unlock_irqrestore(&ndlp->lock, iflags);

		if (ndlp->fc4_xpt_flags & NVME_XPT_REGD &&
		    ndlp->nlp_type & (NLP_NVME_TARGET | NLP_NVME_DISCOVERY)) {
			lpfc_nvme_rescan_port(vport, ndlp);
		}
		return;
	}

	ndlp->fc4_xpt_flags |= NLP_XPT_REGD;
	spin_unlock_irqrestore(&ndlp->lock, iflags);

	if (lpfc_valid_xpt_node(ndlp)) {
		vport->phba->nport_event_cnt++;
		 
		lpfc_register_remote_port(vport, ndlp);
	}

	 
	if (!(ndlp->nlp_fc4_type & NLP_FC4_NVME))
		return;

	 
	if (vport->phba->sli_rev >= LPFC_SLI_REV4 &&
			ndlp->nlp_fc4_type & NLP_FC4_NVME) {
		if (vport->phba->nvmet_support == 0) {
			 
			if (ndlp->nlp_type & NLP_NVME_TARGET) {
				vport->phba->nport_event_cnt++;
				lpfc_nvme_register_port(vport, ndlp);
			}
		} else {
			 
			lpfc_nlp_get(ndlp);
		}
	}
}

 
void
lpfc_nlp_unreg_node(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	unsigned long iflags;

	spin_lock_irqsave(&ndlp->lock, iflags);
	if (!(ndlp->fc4_xpt_flags & NLP_XPT_REGD)) {
		spin_unlock_irqrestore(&ndlp->lock, iflags);
		lpfc_printf_vlog(vport, KERN_INFO,
				 LOG_ELS | LOG_NODE | LOG_DISCOVERY,
				 "0999 %s Not regd: ndlp x%px rport x%px DID "
				 "x%x FLG x%x XPT x%x\n",
				  __func__, ndlp, ndlp->rport, ndlp->nlp_DID,
				  ndlp->nlp_flag, ndlp->fc4_xpt_flags);
		return;
	}

	ndlp->fc4_xpt_flags &= ~NLP_XPT_REGD;
	spin_unlock_irqrestore(&ndlp->lock, iflags);

	if (ndlp->rport &&
	    ndlp->fc4_xpt_flags & SCSI_XPT_REGD) {
		vport->phba->nport_event_cnt++;
		lpfc_unregister_remote_port(ndlp);
	} else if (!ndlp->rport) {
		lpfc_printf_vlog(vport, KERN_INFO,
				 LOG_ELS | LOG_NODE | LOG_DISCOVERY,
				 "1999 %s NDLP in devloss x%px DID x%x FLG x%x"
				 " XPT x%x refcnt %u\n",
				 __func__, ndlp, ndlp->nlp_DID, ndlp->nlp_flag,
				 ndlp->fc4_xpt_flags,
				 kref_read(&ndlp->kref));
	}

	if (ndlp->fc4_xpt_flags & NVME_XPT_REGD) {
		vport->phba->nport_event_cnt++;
		if (vport->phba->nvmet_support == 0) {
			 
			if (ndlp->nlp_type & NLP_NVME_TARGET)
				lpfc_nvme_unregister_port(vport, ndlp);
		} else {
			 
			lpfc_nlp_put(ndlp);
		}
	}

}

 
static void
lpfc_handle_adisc_state(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		int new_state)
{
	switch (new_state) {
	 
	case NLP_STE_ADISC_ISSUE:
		break;

	 
	case NLP_STE_UNMAPPED_NODE:
		ndlp->nlp_type |= NLP_FC_NODE;
		fallthrough;
	case NLP_STE_MAPPED_NODE:
		ndlp->nlp_flag &= ~NLP_NODEV_REMOVE;
		lpfc_nlp_reg_node(vport, ndlp);
		break;

	 
	case NLP_STE_NPR_NODE:
		ndlp->nlp_flag &= ~NLP_RCV_PLOGI;
		fallthrough;
	default:
		lpfc_nlp_unreg_node(vport, ndlp);
		break;
	}

}

static void
lpfc_nlp_state_cleanup(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		       int old_state, int new_state)
{
	 
	if (new_state == NLP_STE_ADISC_ISSUE ||
	    old_state == NLP_STE_ADISC_ISSUE) {
		lpfc_handle_adisc_state(vport, ndlp, new_state);
		return;
	}

	if (new_state == NLP_STE_UNMAPPED_NODE) {
		ndlp->nlp_flag &= ~NLP_NODEV_REMOVE;
		ndlp->nlp_type |= NLP_FC_NODE;
	}
	if (new_state == NLP_STE_MAPPED_NODE)
		ndlp->nlp_flag &= ~NLP_NODEV_REMOVE;
	if (new_state == NLP_STE_NPR_NODE)
		ndlp->nlp_flag &= ~NLP_RCV_PLOGI;

	 
	if ((old_state == NLP_STE_MAPPED_NODE ||
	     old_state == NLP_STE_UNMAPPED_NODE)) {
		 
		if (!(ndlp->nlp_flag & NLP_NPR_ADISC) ||
		    !lpfc_is_link_up(vport->phba))
			lpfc_nlp_unreg_node(vport, ndlp);
	}

	if (new_state ==  NLP_STE_MAPPED_NODE ||
	    new_state == NLP_STE_UNMAPPED_NODE)
		lpfc_nlp_reg_node(vport, ndlp);

	 
	if ((new_state == NLP_STE_MAPPED_NODE) &&
	    (ndlp->nlp_type & NLP_FCP_TARGET) &&
	    (!ndlp->rport ||
	     ndlp->rport->scsi_target_id == -1 ||
	     ndlp->rport->scsi_target_id >= LPFC_MAX_TARGET)) {
		spin_lock_irq(&ndlp->lock);
		ndlp->nlp_flag |= NLP_TGT_NO_SCSIID;
		spin_unlock_irq(&ndlp->lock);
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_UNMAPPED_NODE);
	}
}

static char *
lpfc_nlp_state_name(char *buffer, size_t size, int state)
{
	static char *states[] = {
		[NLP_STE_UNUSED_NODE] = "UNUSED",
		[NLP_STE_PLOGI_ISSUE] = "PLOGI",
		[NLP_STE_ADISC_ISSUE] = "ADISC",
		[NLP_STE_REG_LOGIN_ISSUE] = "REGLOGIN",
		[NLP_STE_PRLI_ISSUE] = "PRLI",
		[NLP_STE_LOGO_ISSUE] = "LOGO",
		[NLP_STE_UNMAPPED_NODE] = "UNMAPPED",
		[NLP_STE_MAPPED_NODE] = "MAPPED",
		[NLP_STE_NPR_NODE] = "NPR",
	};

	if (state < NLP_STE_MAX_STATE && states[state])
		strscpy(buffer, states[state], size);
	else
		snprintf(buffer, size, "unknown (%d)", state);
	return buffer;
}

void
lpfc_nlp_set_state(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		   int state)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	int  old_state = ndlp->nlp_state;
	int node_dropped = ndlp->nlp_flag & NLP_DROPPED;
	char name1[16], name2[16];

	lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE,
			 "0904 NPort state transition x%06x, %s -> %s\n",
			 ndlp->nlp_DID,
			 lpfc_nlp_state_name(name1, sizeof(name1), old_state),
			 lpfc_nlp_state_name(name2, sizeof(name2), state));

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_NODE,
		"node statechg    did:x%x old:%d ste:%d",
		ndlp->nlp_DID, old_state, state);

	if (node_dropped && old_state == NLP_STE_UNUSED_NODE &&
	    state != NLP_STE_UNUSED_NODE) {
		ndlp->nlp_flag &= ~NLP_DROPPED;
		lpfc_nlp_get(ndlp);
	}

	if (old_state == NLP_STE_NPR_NODE &&
	    state != NLP_STE_NPR_NODE)
		lpfc_cancel_retry_delay_tmo(vport, ndlp);
	if (old_state == NLP_STE_UNMAPPED_NODE) {
		ndlp->nlp_flag &= ~NLP_TGT_NO_SCSIID;
		ndlp->nlp_type &= ~NLP_FC_NODE;
	}

	if (list_empty(&ndlp->nlp_listp)) {
		spin_lock_irq(shost->host_lock);
		list_add_tail(&ndlp->nlp_listp, &vport->fc_nodes);
		spin_unlock_irq(shost->host_lock);
	} else if (old_state)
		lpfc_nlp_counters(vport, old_state, -1);

	ndlp->nlp_state = state;
	lpfc_nlp_counters(vport, state, 1);
	lpfc_nlp_state_cleanup(vport, ndlp, old_state, state);
}

void
lpfc_enqueue_node(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	if (list_empty(&ndlp->nlp_listp)) {
		spin_lock_irq(shost->host_lock);
		list_add_tail(&ndlp->nlp_listp, &vport->fc_nodes);
		spin_unlock_irq(shost->host_lock);
	}
}

void
lpfc_dequeue_node(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	lpfc_cancel_retry_delay_tmo(vport, ndlp);
	if (ndlp->nlp_state && !list_empty(&ndlp->nlp_listp))
		lpfc_nlp_counters(vport, ndlp->nlp_state, -1);
	spin_lock_irq(shost->host_lock);
	list_del_init(&ndlp->nlp_listp);
	spin_unlock_irq(shost->host_lock);
	lpfc_nlp_state_cleanup(vport, ndlp, ndlp->nlp_state,
				NLP_STE_UNUSED_NODE);
}

 
static inline void
lpfc_initialize_node(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
	uint32_t did)
{
	INIT_LIST_HEAD(&ndlp->els_retry_evt.evt_listp);
	INIT_LIST_HEAD(&ndlp->dev_loss_evt.evt_listp);
	timer_setup(&ndlp->nlp_delayfunc, lpfc_els_retry_delay, 0);
	INIT_LIST_HEAD(&ndlp->recovery_evt.evt_listp);

	ndlp->nlp_DID = did;
	ndlp->vport = vport;
	ndlp->phba = vport->phba;
	ndlp->nlp_sid = NLP_NO_SID;
	ndlp->nlp_fc4_type = NLP_FC4_NONE;
	kref_init(&ndlp->kref);
	atomic_set(&ndlp->cmd_pending, 0);
	ndlp->cmd_qdepth = vport->cfg_tgt_queue_depth;
	ndlp->nlp_defer_did = NLP_EVT_NOTHING_PENDING;
}

void
lpfc_drop_node(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	 
	if (ndlp->nlp_state == NLP_STE_UNUSED_NODE)
		return;
	lpfc_nlp_set_state(vport, ndlp, NLP_STE_UNUSED_NODE);
	if (vport->phba->sli_rev == LPFC_SLI_REV4) {
		lpfc_cleanup_vports_rrqs(vport, ndlp);
		lpfc_unreg_rpi(vport, ndlp);
	}

	 
	spin_lock_irq(&ndlp->lock);
	if (!(ndlp->nlp_flag & NLP_DROPPED)) {
		ndlp->nlp_flag |= NLP_DROPPED;
		spin_unlock_irq(&ndlp->lock);
		lpfc_nlp_put(ndlp);
		return;
	}
	spin_unlock_irq(&ndlp->lock);
}

 
void
lpfc_set_disctmo(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	uint32_t tmo;

	if (vport->port_state == LPFC_LOCAL_CFG_LINK) {
		 
		tmo = (((phba->fc_edtov + 999) / 1000) + 1);
	} else {
		 
		tmo = ((phba->fc_ratov * 3) + 3);
	}


	if (!timer_pending(&vport->fc_disctmo)) {
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
			"set disc timer:  tmo:x%x state:x%x flg:x%x",
			tmo, vport->port_state, vport->fc_flag);
	}

	mod_timer(&vport->fc_disctmo, jiffies + msecs_to_jiffies(1000 * tmo));
	spin_lock_irq(shost->host_lock);
	vport->fc_flag |= FC_DISC_TMO;
	spin_unlock_irq(shost->host_lock);

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0247 Start Discovery Timer state x%x "
			 "Data: x%x x%lx x%x x%x\n",
			 vport->port_state, tmo,
			 (unsigned long)&vport->fc_disctmo, vport->fc_plogi_cnt,
			 vport->fc_adisc_cnt);

	return;
}

 
int
lpfc_can_disctmo(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	unsigned long iflags;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"can disc timer:  state:x%x rtry:x%x flg:x%x",
		vport->port_state, vport->fc_ns_retry, vport->fc_flag);

	 
	if (vport->fc_flag & FC_DISC_TMO ||
	    timer_pending(&vport->fc_disctmo)) {
		spin_lock_irqsave(shost->host_lock, iflags);
		vport->fc_flag &= ~FC_DISC_TMO;
		spin_unlock_irqrestore(shost->host_lock, iflags);
		del_timer_sync(&vport->fc_disctmo);
		spin_lock_irqsave(&vport->work_port_lock, iflags);
		vport->work_port_events &= ~WORKER_DISC_TMO;
		spin_unlock_irqrestore(&vport->work_port_lock, iflags);
	}

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0248 Cancel Discovery Timer state x%x "
			 "Data: x%x x%x x%x\n",
			 vport->port_state, vport->fc_flag,
			 vport->fc_plogi_cnt, vport->fc_adisc_cnt);
	return 0;
}

 
int
lpfc_check_sli_ndlp(struct lpfc_hba *phba,
		    struct lpfc_sli_ring *pring,
		    struct lpfc_iocbq *iocb,
		    struct lpfc_nodelist *ndlp)
{
	struct lpfc_vport *vport = ndlp->vport;
	u8 ulp_command;
	u16 ulp_context;
	u32 remote_id;

	if (iocb->vport != vport)
		return 0;

	ulp_command = get_job_cmnd(phba, iocb);
	ulp_context = get_job_ulpcontext(phba, iocb);
	remote_id = get_job_els_rsp64_did(phba, iocb);

	if (pring->ringno == LPFC_ELS_RING) {
		switch (ulp_command) {
		case CMD_GEN_REQUEST64_CR:
			if (iocb->ndlp == ndlp)
				return 1;
			fallthrough;
		case CMD_ELS_REQUEST64_CR:
			if (remote_id == ndlp->nlp_DID)
				return 1;
			fallthrough;
		case CMD_XMIT_ELS_RSP64_CX:
			if (iocb->ndlp == ndlp)
				return 1;
		}
	} else if (pring->ringno == LPFC_FCP_RING) {
		 
		if ((ndlp->nlp_type & NLP_FCP_TARGET) &&
		    (ndlp->nlp_flag & NLP_DELAY_TMO)) {
			return 0;
		}
		if (ulp_context == ndlp->nlp_rpi)
			return 1;
	}
	return 0;
}

static void
__lpfc_dequeue_nport_iocbs(struct lpfc_hba *phba,
		struct lpfc_nodelist *ndlp, struct lpfc_sli_ring *pring,
		struct list_head *dequeue_list)
{
	struct lpfc_iocbq *iocb, *next_iocb;

	list_for_each_entry_safe(iocb, next_iocb, &pring->txq, list) {
		 
		if (lpfc_check_sli_ndlp(phba, pring, iocb, ndlp))
			 
			list_move_tail(&iocb->list, dequeue_list);
	}
}

static void
lpfc_sli3_dequeue_nport_iocbs(struct lpfc_hba *phba,
		struct lpfc_nodelist *ndlp, struct list_head *dequeue_list)
{
	struct lpfc_sli *psli = &phba->sli;
	uint32_t i;

	spin_lock_irq(&phba->hbalock);
	for (i = 0; i < psli->num_rings; i++)
		__lpfc_dequeue_nport_iocbs(phba, ndlp, &psli->sli3_ring[i],
						dequeue_list);
	spin_unlock_irq(&phba->hbalock);
}

static void
lpfc_sli4_dequeue_nport_iocbs(struct lpfc_hba *phba,
		struct lpfc_nodelist *ndlp, struct list_head *dequeue_list)
{
	struct lpfc_sli_ring *pring;
	struct lpfc_queue *qp = NULL;

	spin_lock_irq(&phba->hbalock);
	list_for_each_entry(qp, &phba->sli4_hba.lpfc_wq_list, wq_list) {
		pring = qp->pring;
		if (!pring)
			continue;
		spin_lock(&pring->ring_lock);
		__lpfc_dequeue_nport_iocbs(phba, ndlp, pring, dequeue_list);
		spin_unlock(&pring->ring_lock);
	}
	spin_unlock_irq(&phba->hbalock);
}

 
static int
lpfc_no_rpi(struct lpfc_hba *phba, struct lpfc_nodelist *ndlp)
{
	LIST_HEAD(completions);

	lpfc_fabric_abort_nport(ndlp);

	 
	if (ndlp->nlp_flag & NLP_RPI_REGISTERED) {
		if (phba->sli_rev != LPFC_SLI_REV4)
			lpfc_sli3_dequeue_nport_iocbs(phba, ndlp, &completions);
		else
			lpfc_sli4_dequeue_nport_iocbs(phba, ndlp, &completions);
	}

	 
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);

	return 0;
}

 
static void
lpfc_nlp_logo_unreg(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport  *vport = pmb->vport;
	struct lpfc_nodelist *ndlp;

	ndlp = (struct lpfc_nodelist *)(pmb->ctx_ndlp);
	if (!ndlp)
		return;
	lpfc_issue_els_logo(vport, ndlp, 0);

	 
	if ((ndlp->nlp_flag & NLP_UNREG_INP) &&
	    (ndlp->nlp_defer_did != NLP_EVT_NOTHING_PENDING)) {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
				 "1434 UNREG cmpl deferred logo x%x "
				 "on NPort x%x Data: x%x x%px\n",
				 ndlp->nlp_rpi, ndlp->nlp_DID,
				 ndlp->nlp_defer_did, ndlp);

		ndlp->nlp_flag &= ~NLP_UNREG_INP;
		ndlp->nlp_defer_did = NLP_EVT_NOTHING_PENDING;
		lpfc_issue_els_plogi(vport, ndlp->nlp_DID, 0);
	} else {
		 
		if (ndlp->nlp_flag & NLP_RELEASE_RPI) {
			lpfc_sli4_free_rpi(vport->phba, ndlp->nlp_rpi);
			spin_lock_irq(&ndlp->lock);
			ndlp->nlp_flag &= ~NLP_RELEASE_RPI;
			ndlp->nlp_rpi = LPFC_RPI_ALLOC_ERROR;
			spin_unlock_irq(&ndlp->lock);
		}
		spin_lock_irq(&ndlp->lock);
		ndlp->nlp_flag &= ~NLP_UNREG_INP;
		spin_unlock_irq(&ndlp->lock);
	}

	 
	lpfc_nlp_put(ndlp);
	mempool_free(pmb, phba->mbox_mem_pool);
}

 
static void
lpfc_set_unreg_login_mbx_cmpl(struct lpfc_hba *phba, struct lpfc_vport *vport,
	struct lpfc_nodelist *ndlp, LPFC_MBOXQ_t *mbox)
{
	unsigned long iflags;

	 
	mbox->ctx_ndlp = lpfc_nlp_get(ndlp);
	if (!mbox->ctx_ndlp)
		return;

	if (ndlp->nlp_flag & NLP_ISSUE_LOGO) {
		mbox->mbox_cmpl = lpfc_nlp_logo_unreg;

	} else if (phba->sli_rev == LPFC_SLI_REV4 &&
		   (!(vport->load_flag & FC_UNLOADING)) &&
		    (bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) >=
				      LPFC_SLI_INTF_IF_TYPE_2) &&
		    (kref_read(&ndlp->kref) > 0)) {
		mbox->mbox_cmpl = lpfc_sli4_unreg_rpi_cmpl_clr;
	} else {
		if (vport->load_flag & FC_UNLOADING) {
			if (phba->sli_rev == LPFC_SLI_REV4) {
				spin_lock_irqsave(&ndlp->lock, iflags);
				ndlp->nlp_flag |= NLP_RELEASE_RPI;
				spin_unlock_irqrestore(&ndlp->lock, iflags);
			}
		}
		mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	}
}

 
int
lpfc_unreg_rpi(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba *phba = vport->phba;
	LPFC_MBOXQ_t    *mbox;
	int rc, acc_plogi = 1;
	uint16_t rpi;

	if (ndlp->nlp_flag & NLP_RPI_REGISTERED ||
	    ndlp->nlp_flag & NLP_REG_LOGIN_SEND) {
		if (ndlp->nlp_flag & NLP_REG_LOGIN_SEND)
			lpfc_printf_vlog(vport, KERN_INFO,
					 LOG_NODE | LOG_DISCOVERY,
					 "3366 RPI x%x needs to be "
					 "unregistered nlp_flag x%x "
					 "did x%x\n",
					 ndlp->nlp_rpi, ndlp->nlp_flag,
					 ndlp->nlp_DID);

		 
		if (ndlp->nlp_flag & NLP_UNREG_INP) {
			lpfc_printf_vlog(vport, KERN_INFO,
					 LOG_NODE | LOG_DISCOVERY,
					 "1436 unreg_rpi SKIP UNREG x%x on "
					 "NPort x%x deferred x%x  flg x%x "
					 "Data: x%px\n",
					 ndlp->nlp_rpi, ndlp->nlp_DID,
					 ndlp->nlp_defer_did,
					 ndlp->nlp_flag, ndlp);
			goto out;
		}

		mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
		if (mbox) {
			 
			rpi = ndlp->nlp_rpi;
			if (phba->sli_rev == LPFC_SLI_REV4)
				rpi = phba->sli4_hba.rpi_ids[ndlp->nlp_rpi];

			lpfc_unreg_login(phba, vport->vpi, rpi, mbox);
			mbox->vport = vport;
			lpfc_set_unreg_login_mbx_cmpl(phba, vport, ndlp, mbox);
			if (!mbox->ctx_ndlp) {
				mempool_free(mbox, phba->mbox_mem_pool);
				return 1;
			}

			if (mbox->mbox_cmpl == lpfc_sli4_unreg_rpi_cmpl_clr)
				 
				acc_plogi = 0;
			if (((ndlp->nlp_DID & Fabric_DID_MASK) !=
			    Fabric_DID_MASK) &&
			    (!(vport->fc_flag & FC_OFFLINE_MODE)))
				ndlp->nlp_flag |= NLP_UNREG_INP;

			lpfc_printf_vlog(vport, KERN_INFO,
					 LOG_NODE | LOG_DISCOVERY,
					 "1433 unreg_rpi UNREG x%x on "
					 "NPort x%x deferred flg x%x "
					 "Data:x%px\n",
					 ndlp->nlp_rpi, ndlp->nlp_DID,
					 ndlp->nlp_flag, ndlp);

			rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
			if (rc == MBX_NOT_FINISHED) {
				ndlp->nlp_flag &= ~NLP_UNREG_INP;
				mempool_free(mbox, phba->mbox_mem_pool);
				acc_plogi = 1;
				lpfc_nlp_put(ndlp);
			}
		} else {
			lpfc_printf_vlog(vport, KERN_INFO,
					 LOG_NODE | LOG_DISCOVERY,
					 "1444 Failed to allocate mempool "
					 "unreg_rpi UNREG x%x, "
					 "DID x%x, flag x%x, "
					 "ndlp x%px\n",
					 ndlp->nlp_rpi, ndlp->nlp_DID,
					 ndlp->nlp_flag, ndlp);

			 
			if (!(vport->load_flag & FC_UNLOADING)) {
				ndlp->nlp_flag &= ~NLP_UNREG_INP;
				lpfc_issue_els_logo(vport, ndlp, 0);
				ndlp->nlp_prev_state = ndlp->nlp_state;
				lpfc_nlp_set_state(vport, ndlp,
						   NLP_STE_NPR_NODE);
			}

			return 1;
		}
		lpfc_no_rpi(phba, ndlp);
out:
		if (phba->sli_rev != LPFC_SLI_REV4)
			ndlp->nlp_rpi = 0;
		ndlp->nlp_flag &= ~NLP_RPI_REGISTERED;
		ndlp->nlp_flag &= ~NLP_NPR_ADISC;
		if (acc_plogi)
			ndlp->nlp_flag &= ~NLP_LOGO_ACC;
		return 1;
	}
	ndlp->nlp_flag &= ~NLP_LOGO_ACC;
	return 0;
}

 
void
lpfc_unreg_hba_rpis(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	struct lpfc_nodelist *ndlp;
	struct Scsi_Host *shost;
	int i;

	vports = lpfc_create_vport_work_array(phba);
	if (!vports) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2884 Vport array allocation failed \n");
		return;
	}
	for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
		shost = lpfc_shost_from_vport(vports[i]);
		spin_lock_irq(shost->host_lock);
		list_for_each_entry(ndlp, &vports[i]->fc_nodes, nlp_listp) {
			if (ndlp->nlp_flag & NLP_RPI_REGISTERED) {
				 
				spin_unlock_irq(shost->host_lock);
				lpfc_unreg_rpi(vports[i], ndlp);
				spin_lock_irq(shost->host_lock);
			}
		}
		spin_unlock_irq(shost->host_lock);
	}
	lpfc_destroy_vport_work_array(phba, vports);
}

void
lpfc_unreg_all_rpis(struct lpfc_vport *vport)
{
	struct lpfc_hba  *phba  = vport->phba;
	LPFC_MBOXQ_t     *mbox;
	int rc;

	if (phba->sli_rev == LPFC_SLI_REV4) {
		lpfc_sli4_unreg_all_rpis(vport);
		return;
	}

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (mbox) {
		lpfc_unreg_login(phba, vport->vpi, LPFC_UNREG_ALL_RPIS_VPORT,
				 mbox);
		mbox->vport = vport;
		mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		mbox->ctx_ndlp = NULL;
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, LPFC_MBOX_TMO);
		if (rc != MBX_TIMEOUT)
			mempool_free(mbox, phba->mbox_mem_pool);

		if ((rc == MBX_TIMEOUT) || (rc == MBX_NOT_FINISHED))
			lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
					 "1836 Could not issue "
					 "unreg_login(all_rpis) status %d\n",
					 rc);
	}
}

void
lpfc_unreg_default_rpis(struct lpfc_vport *vport)
{
	struct lpfc_hba  *phba  = vport->phba;
	LPFC_MBOXQ_t     *mbox;
	int rc;

	 
	if (phba->sli_rev > LPFC_SLI_REV3)
		return;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (mbox) {
		lpfc_unreg_did(phba, vport->vpi, LPFC_UNREG_ALL_DFLT_RPIS,
			       mbox);
		mbox->vport = vport;
		mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		mbox->ctx_ndlp = NULL;
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, LPFC_MBOX_TMO);
		if (rc != MBX_TIMEOUT)
			mempool_free(mbox, phba->mbox_mem_pool);

		if ((rc == MBX_TIMEOUT) || (rc == MBX_NOT_FINISHED))
			lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
					 "1815 Could not issue "
					 "unreg_did (default rpis) status %d\n",
					 rc);
	}
}

 
static int
lpfc_cleanup_node(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba  *phba = vport->phba;
	LPFC_MBOXQ_t *mb, *nextmb;

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE,
			 "0900 Cleanup node for NPort x%x "
			 "Data: x%x x%x x%x\n",
			 ndlp->nlp_DID, ndlp->nlp_flag,
			 ndlp->nlp_state, ndlp->nlp_rpi);
	lpfc_dequeue_node(vport, ndlp);

	 

	 
	if ((mb = phba->sli.mbox_active)) {
		if ((mb->u.mb.mbxCommand == MBX_REG_LOGIN64) &&
		   !(mb->mbox_flag & LPFC_MBX_IMED_UNREG) &&
		   (ndlp == (struct lpfc_nodelist *)mb->ctx_ndlp)) {
			mb->ctx_ndlp = NULL;
			mb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		}
	}

	spin_lock_irq(&phba->hbalock);
	 
	list_for_each_entry(mb, &phba->sli.mboxq_cmpl, list) {
		if ((mb->u.mb.mbxCommand != MBX_REG_LOGIN64) ||
			(mb->mbox_flag & LPFC_MBX_IMED_UNREG) ||
			(ndlp != (struct lpfc_nodelist *)mb->ctx_ndlp))
			continue;

		mb->ctx_ndlp = NULL;
		mb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	}

	list_for_each_entry_safe(mb, nextmb, &phba->sli.mboxq, list) {
		if ((mb->u.mb.mbxCommand == MBX_REG_LOGIN64) &&
		   !(mb->mbox_flag & LPFC_MBX_IMED_UNREG) &&
		    (ndlp == (struct lpfc_nodelist *)mb->ctx_ndlp)) {
			list_del(&mb->list);
			lpfc_mbox_rsrc_cleanup(phba, mb, MBOX_THD_LOCKED);

			 
		}
	}
	spin_unlock_irq(&phba->hbalock);

	lpfc_els_abort(phba, ndlp);

	spin_lock_irq(&ndlp->lock);
	ndlp->nlp_flag &= ~NLP_DELAY_TMO;
	spin_unlock_irq(&ndlp->lock);

	ndlp->nlp_last_elscmd = 0;
	del_timer_sync(&ndlp->nlp_delayfunc);

	list_del_init(&ndlp->els_retry_evt.evt_listp);
	list_del_init(&ndlp->dev_loss_evt.evt_listp);
	list_del_init(&ndlp->recovery_evt.evt_listp);
	lpfc_cleanup_vports_rrqs(vport, ndlp);

	if (phba->sli_rev == LPFC_SLI_REV4)
		ndlp->nlp_flag |= NLP_RELEASE_RPI;

	return 0;
}

static int
lpfc_matchdid(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
	      uint32_t did)
{
	D_ID mydid, ndlpdid, matchdid;

	if (did == Bcast_DID)
		return 0;

	 
	if (ndlp->nlp_DID == did)
		return 1;

	 
	mydid.un.word = vport->fc_myDID;
	if ((mydid.un.b.domain == 0) && (mydid.un.b.area == 0)) {
		return 0;
	}

	matchdid.un.word = did;
	ndlpdid.un.word = ndlp->nlp_DID;
	if (matchdid.un.b.id == ndlpdid.un.b.id) {
		if ((mydid.un.b.domain == matchdid.un.b.domain) &&
		    (mydid.un.b.area == matchdid.un.b.area)) {
			 
			if ((ndlpdid.un.b.domain == 0) &&
			    (ndlpdid.un.b.area == 0)) {
				if (ndlpdid.un.b.id &&
				    vport->phba->fc_topology ==
				    LPFC_TOPOLOGY_LOOP)
					return 1;
			}
			return 0;
		}

		matchdid.un.word = ndlp->nlp_DID;
		if ((mydid.un.b.domain == ndlpdid.un.b.domain) &&
		    (mydid.un.b.area == ndlpdid.un.b.area)) {
			if ((matchdid.un.b.domain == 0) &&
			    (matchdid.un.b.area == 0)) {
				if (matchdid.un.b.id)
					return 1;
			}
		}
	}
	return 0;
}

 
static struct lpfc_nodelist *
__lpfc_findnode_did(struct lpfc_vport *vport, uint32_t did)
{
	struct lpfc_nodelist *ndlp;
	uint32_t data1;

	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
		if (lpfc_matchdid(vport, ndlp, did)) {
			data1 = (((uint32_t)ndlp->nlp_state << 24) |
				 ((uint32_t)ndlp->nlp_xri << 16) |
				 ((uint32_t)ndlp->nlp_type << 8)
				 );
			lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE,
					 "0929 FIND node DID "
					 "Data: x%px x%x x%x x%x x%x x%px\n",
					 ndlp, ndlp->nlp_DID,
					 ndlp->nlp_flag, data1, ndlp->nlp_rpi,
					 ndlp->active_rrqs_xri_bitmap);
			return ndlp;
		}
	}

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE,
			 "0932 FIND node did x%x NOT FOUND.\n", did);
	return NULL;
}

struct lpfc_nodelist *
lpfc_findnode_did(struct lpfc_vport *vport, uint32_t did)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp;
	unsigned long iflags;

	spin_lock_irqsave(shost->host_lock, iflags);
	ndlp = __lpfc_findnode_did(vport, did);
	spin_unlock_irqrestore(shost->host_lock, iflags);
	return ndlp;
}

struct lpfc_nodelist *
lpfc_findnode_mapped(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp;
	uint32_t data1;
	unsigned long iflags;

	spin_lock_irqsave(shost->host_lock, iflags);

	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
		if (ndlp->nlp_state == NLP_STE_UNMAPPED_NODE ||
		    ndlp->nlp_state == NLP_STE_MAPPED_NODE) {
			data1 = (((uint32_t)ndlp->nlp_state << 24) |
				 ((uint32_t)ndlp->nlp_xri << 16) |
				 ((uint32_t)ndlp->nlp_type << 8) |
				 ((uint32_t)ndlp->nlp_rpi & 0xff));
			spin_unlock_irqrestore(shost->host_lock, iflags);
			lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE,
					 "2025 FIND node DID "
					 "Data: x%px x%x x%x x%x x%px\n",
					 ndlp, ndlp->nlp_DID,
					 ndlp->nlp_flag, data1,
					 ndlp->active_rrqs_xri_bitmap);
			return ndlp;
		}
	}
	spin_unlock_irqrestore(shost->host_lock, iflags);

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE,
			 "2026 FIND mapped did NOT FOUND.\n");
	return NULL;
}

struct lpfc_nodelist *
lpfc_setup_disc_node(struct lpfc_vport *vport, uint32_t did)
{
	struct lpfc_nodelist *ndlp;

	ndlp = lpfc_findnode_did(vport, did);
	if (!ndlp) {
		if (vport->phba->nvmet_support)
			return NULL;
		if ((vport->fc_flag & FC_RSCN_MODE) != 0 &&
		    lpfc_rscn_payload_check(vport, did) == 0)
			return NULL;
		ndlp = lpfc_nlp_init(vport, did);
		if (!ndlp)
			return NULL;
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);

		lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
				 "6453 Setup New Node 2B_DISC x%x "
				 "Data:x%x x%x x%x\n",
				 ndlp->nlp_DID, ndlp->nlp_flag,
				 ndlp->nlp_state, vport->fc_flag);

		spin_lock_irq(&ndlp->lock);
		ndlp->nlp_flag |= NLP_NPR_2B_DISC;
		spin_unlock_irq(&ndlp->lock);
		return ndlp;
	}

	 
	if ((vport->fc_flag & FC_RSCN_MODE) &&
	    !(vport->fc_flag & FC_NDISC_ACTIVE)) {
		if (lpfc_rscn_payload_check(vport, did)) {

			 
			lpfc_cancel_retry_delay_tmo(vport, ndlp);

			lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
					 "6455 Setup RSCN Node 2B_DISC x%x "
					 "Data:x%x x%x x%x\n",
					 ndlp->nlp_DID, ndlp->nlp_flag,
					 ndlp->nlp_state, vport->fc_flag);

			 
			if (vport->phba->nvmet_support)
				return ndlp;

			 
			if (ndlp->nlp_flag & NLP_RCV_PLOGI &&
			    !(ndlp->nlp_type &
			     (NLP_FCP_TARGET | NLP_NVME_TARGET)))
				return NULL;

			if (ndlp->nlp_state > NLP_STE_UNUSED_NODE &&
			    ndlp->nlp_state < NLP_STE_PRLI_ISSUE) {
				lpfc_disc_state_machine(vport, ndlp, NULL,
							NLP_EVT_DEVICE_RECOVERY);
			}

			spin_lock_irq(&ndlp->lock);
			ndlp->nlp_flag |= NLP_NPR_2B_DISC;
			spin_unlock_irq(&ndlp->lock);
		} else {
			lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
					 "6456 Skip Setup RSCN Node x%x "
					 "Data:x%x x%x x%x\n",
					 ndlp->nlp_DID, ndlp->nlp_flag,
					 ndlp->nlp_state, vport->fc_flag);
			ndlp = NULL;
		}
	} else {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
				 "6457 Setup Active Node 2B_DISC x%x "
				 "Data:x%x x%x x%x\n",
				 ndlp->nlp_DID, ndlp->nlp_flag,
				 ndlp->nlp_state, vport->fc_flag);

		 
		if (ndlp->nlp_state == NLP_STE_ADISC_ISSUE ||
		    ndlp->nlp_state == NLP_STE_PLOGI_ISSUE ||
		    (!vport->phba->nvmet_support &&
		     ndlp->nlp_flag & NLP_RCV_PLOGI))
			return NULL;

		if (vport->phba->nvmet_support)
			return ndlp;

		 
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);

		spin_lock_irq(&ndlp->lock);
		ndlp->nlp_flag |= NLP_NPR_2B_DISC;
		spin_unlock_irq(&ndlp->lock);
	}
	return ndlp;
}

 
void
lpfc_disc_list_loopmap(struct lpfc_vport *vport)
{
	struct lpfc_hba  *phba = vport->phba;
	int j;
	uint32_t alpa, index;

	if (!lpfc_is_link_up(phba))
		return;

	if (phba->fc_topology != LPFC_TOPOLOGY_LOOP)
		return;

	 
	if (phba->alpa_map[0]) {
		for (j = 1; j <= phba->alpa_map[0]; j++) {
			alpa = phba->alpa_map[j];
			if (((vport->fc_myDID & 0xff) == alpa) || (alpa == 0))
				continue;
			lpfc_setup_disc_node(vport, alpa);
		}
	} else {
		 
		for (j = 0; j < FC_MAXLOOP; j++) {
			 
			if (vport->cfg_scan_down)
				index = j;
			else
				index = FC_MAXLOOP - j - 1;
			alpa = lpfcAlpaArray[index];
			if ((vport->fc_myDID & 0xff) == alpa)
				continue;
			lpfc_setup_disc_node(vport, alpa);
		}
	}
	return;
}

 
void
lpfc_issue_clear_la(struct lpfc_hba *phba, struct lpfc_vport *vport)
{
	LPFC_MBOXQ_t *mbox;
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring *extra_ring = &psli->sli3_ring[LPFC_EXTRA_RING];
	struct lpfc_sli_ring *fcp_ring   = &psli->sli3_ring[LPFC_FCP_RING];
	int  rc;

	 
	if ((phba->link_state >= LPFC_CLEAR_LA) ||
	    (vport->port_type != LPFC_PHYSICAL_PORT) ||
		(phba->sli_rev == LPFC_SLI_REV4))
		return;

			 
	if ((mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL)) != NULL) {
		phba->link_state = LPFC_CLEAR_LA;
		lpfc_clear_la(phba, mbox);
		mbox->mbox_cmpl = lpfc_mbx_cmpl_clear_la;
		mbox->vport = vport;
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
		if (rc == MBX_NOT_FINISHED) {
			mempool_free(mbox, phba->mbox_mem_pool);
			lpfc_disc_flush_list(vport);
			extra_ring->flag &= ~LPFC_STOP_IOCB_EVENT;
			fcp_ring->flag &= ~LPFC_STOP_IOCB_EVENT;
			phba->link_state = LPFC_HBA_ERROR;
		}
	}
}

 
void
lpfc_issue_reg_vpi(struct lpfc_hba *phba, struct lpfc_vport *vport)
{
	LPFC_MBOXQ_t *regvpimbox;

	regvpimbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (regvpimbox) {
		lpfc_reg_vpi(vport, regvpimbox);
		regvpimbox->mbox_cmpl = lpfc_mbx_cmpl_reg_vpi;
		regvpimbox->vport = vport;
		if (lpfc_sli_issue_mbox(phba, regvpimbox, MBX_NOWAIT)
					== MBX_NOT_FINISHED) {
			mempool_free(regvpimbox, phba->mbox_mem_pool);
		}
	}
}

 
void
lpfc_disc_start(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	uint32_t num_sent;
	uint32_t clear_la_pending;

	if (!lpfc_is_link_up(phba)) {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_SLI,
				 "3315 Link is not up %x\n",
				 phba->link_state);
		return;
	}

	if (phba->link_state == LPFC_CLEAR_LA)
		clear_la_pending = 1;
	else
		clear_la_pending = 0;

	if (vport->port_state < LPFC_VPORT_READY)
		vport->port_state = LPFC_DISC_AUTH;

	lpfc_set_disctmo(vport);

	vport->fc_prevDID = vport->fc_myDID;
	vport->num_disc_nodes = 0;

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0202 Start Discovery port state x%x "
			 "flg x%x Data: x%x x%x x%x\n",
			 vport->port_state, vport->fc_flag, vport->fc_plogi_cnt,
			 vport->fc_adisc_cnt, vport->fc_npr_cnt);

	 
	num_sent = lpfc_els_disc_adisc(vport);

	if (num_sent)
		return;

	 
	if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
	    !(vport->fc_flag & FC_PT2PT) &&
	    !(vport->fc_flag & FC_RSCN_MODE) &&
	    (phba->sli_rev < LPFC_SLI_REV4)) {
		lpfc_issue_clear_la(phba, vport);
		lpfc_issue_reg_vpi(phba, vport);
		return;
	}

	 
	if (vport->port_state < LPFC_VPORT_READY && !clear_la_pending) {
		 
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
			}
		}
		vport->port_state = LPFC_VPORT_READY;
	} else {
		 
		num_sent = lpfc_els_disc_plogi(vport);

		if (num_sent)
			return;

		if (vport->fc_flag & FC_RSCN_MODE) {
			 
			if ((vport->fc_rscn_id_cnt == 0) &&
			    (!(vport->fc_flag & FC_RSCN_DISCOVERY))) {
				spin_lock_irq(shost->host_lock);
				vport->fc_flag &= ~FC_RSCN_MODE;
				spin_unlock_irq(shost->host_lock);
				lpfc_can_disctmo(vport);
			} else
				lpfc_els_handle_rscn(vport);
		}
	}
	return;
}

 
static void
lpfc_free_tx(struct lpfc_hba *phba, struct lpfc_nodelist *ndlp)
{
	LIST_HEAD(completions);
	struct lpfc_iocbq    *iocb, *next_iocb;
	struct lpfc_sli_ring *pring;
	u32 ulp_command;

	pring = lpfc_phba_elsring(phba);
	if (unlikely(!pring))
		return;

	 
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(iocb, next_iocb, &pring->txq, list) {
		if (iocb->ndlp != ndlp)
			continue;

		ulp_command = get_job_cmnd(phba, iocb);

		if (ulp_command == CMD_ELS_REQUEST64_CR ||
		    ulp_command == CMD_XMIT_ELS_RSP64_CX) {

			list_move_tail(&iocb->list, &completions);
		}
	}

	 
	list_for_each_entry_safe(iocb, next_iocb, &pring->txcmplq, list) {
		if (iocb->ndlp != ndlp)
			continue;

		ulp_command = get_job_cmnd(phba, iocb);

		if (ulp_command == CMD_ELS_REQUEST64_CR ||
		    ulp_command == CMD_XMIT_ELS_RSP64_CX) {
			lpfc_sli_issue_abort_iotag(phba, pring, iocb, NULL);
		}
	}
	spin_unlock_irq(&phba->hbalock);

	 
	lpfc_issue_hb_tmo(phba);

	 
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);
}

static void
lpfc_disc_flush_list(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp, *next_ndlp;
	struct lpfc_hba *phba = vport->phba;

	if (vport->fc_plogi_cnt || vport->fc_adisc_cnt) {
		list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes,
					 nlp_listp) {
			if (ndlp->nlp_state == NLP_STE_PLOGI_ISSUE ||
			    ndlp->nlp_state == NLP_STE_ADISC_ISSUE) {
				lpfc_free_tx(phba, ndlp);
			}
		}
	}
}

 
static void
lpfc_notify_xport_npr(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp, *next_ndlp;

	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes,
				 nlp_listp) {
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
	}
}
void
lpfc_cleanup_discovery_resources(struct lpfc_vport *vport)
{
	lpfc_els_flush_rscn(vport);
	lpfc_els_flush_cmd(vport);
	lpfc_disc_flush_list(vport);
	if (pci_channel_offline(vport->phba->pcidev))
		lpfc_notify_xport_npr(vport);
}

 
 
 
void
lpfc_disc_timeout(struct timer_list *t)
{
	struct lpfc_vport *vport = from_timer(vport, t, fc_disctmo);
	struct lpfc_hba   *phba = vport->phba;
	uint32_t tmo_posted;
	unsigned long flags = 0;

	if (unlikely(!phba))
		return;

	spin_lock_irqsave(&vport->work_port_lock, flags);
	tmo_posted = vport->work_port_events & WORKER_DISC_TMO;
	if (!tmo_posted)
		vport->work_port_events |= WORKER_DISC_TMO;
	spin_unlock_irqrestore(&vport->work_port_lock, flags);

	if (!tmo_posted)
		lpfc_worker_wake_up(phba);
	return;
}

static void
lpfc_disc_timeout_handler(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_sli  *psli = &phba->sli;
	struct lpfc_nodelist *ndlp, *next_ndlp;
	LPFC_MBOXQ_t *initlinkmbox;
	int rc, clrlaerr = 0;

	if (!(vport->fc_flag & FC_DISC_TMO))
		return;

	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~FC_DISC_TMO;
	spin_unlock_irq(shost->host_lock);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"disc timeout:    state:x%x rtry:x%x flg:x%x",
		vport->port_state, vport->fc_ns_retry, vport->fc_flag);

	switch (vport->port_state) {

	case LPFC_LOCAL_CFG_LINK:
		 
		lpfc_printf_vlog(vport, KERN_WARNING, LOG_DISCOVERY,
				 "0221 FAN timeout\n");

		 
		list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes,
					 nlp_listp) {
			if (ndlp->nlp_state != NLP_STE_NPR_NODE)
				continue;
			if (ndlp->nlp_type & NLP_FABRIC) {
				 
				lpfc_drop_node(vport, ndlp);

			} else if (!(ndlp->nlp_flag & NLP_NPR_ADISC)) {
				 
				lpfc_unreg_rpi(vport, ndlp);
			}
		}
		if (vport->port_state != LPFC_FLOGI) {
			if (phba->sli_rev <= LPFC_SLI_REV3)
				lpfc_initial_flogi(vport);
			else
				lpfc_issue_init_vfi(vport);
			return;
		}
		break;

	case LPFC_FDISC:
	case LPFC_FLOGI:
	 
		 
		lpfc_printf_vlog(vport, KERN_ERR,
				 LOG_TRACE_EVENT,
				 "0222 Initial %s timeout\n",
				 vport->vpi ? "FDISC" : "FLOGI");

		 

		 
		lpfc_disc_list_loopmap(vport);

		 
		lpfc_disc_start(vport);
		break;

	case LPFC_FABRIC_CFG_LINK:
	 
		lpfc_printf_vlog(vport, KERN_ERR,
				 LOG_TRACE_EVENT,
				 "0223 Timeout while waiting for "
				 "NameServer login\n");
		 
		ndlp = lpfc_findnode_did(vport, NameServer_DID);
		if (ndlp)
			lpfc_els_abort(phba, ndlp);

		 
		goto restart_disc;

	case LPFC_NS_QRY:
	 
		lpfc_printf_vlog(vport, KERN_ERR,
				 LOG_TRACE_EVENT,
				 "0224 NameServer Query timeout "
				 "Data: x%x x%x\n",
				 vport->fc_ns_retry, LPFC_MAX_NS_RETRY);

		if (vport->fc_ns_retry < LPFC_MAX_NS_RETRY) {
			 
			vport->fc_ns_retry++;
			vport->gidft_inp = 0;
			rc = lpfc_issue_gidft(vport);
			if (rc == 0)
				break;
		}
		vport->fc_ns_retry = 0;

restart_disc:
		 
		if (phba->sli_rev < LPFC_SLI_REV4) {
			if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED)
				lpfc_issue_reg_vpi(phba, vport);
			else  {
				lpfc_issue_clear_la(phba, vport);
				vport->port_state = LPFC_VPORT_READY;
			}
		}

		 
		initlinkmbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
		if (!initlinkmbox) {
			lpfc_printf_vlog(vport, KERN_ERR,
					 LOG_TRACE_EVENT,
					 "0206 Device Discovery "
					 "completion error\n");
			phba->link_state = LPFC_HBA_ERROR;
			break;
		}

		lpfc_linkdown(phba);
		lpfc_init_link(phba, initlinkmbox, phba->cfg_topology,
			       phba->cfg_link_speed);
		initlinkmbox->u.mb.un.varInitLnk.lipsr_AL_PA = 0;
		initlinkmbox->vport = vport;
		initlinkmbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		rc = lpfc_sli_issue_mbox(phba, initlinkmbox, MBX_NOWAIT);
		lpfc_set_loopback_flag(phba);
		if (rc == MBX_NOT_FINISHED)
			mempool_free(initlinkmbox, phba->mbox_mem_pool);

		break;

	case LPFC_DISC_AUTH:
	 
		lpfc_printf_vlog(vport, KERN_ERR,
				 LOG_TRACE_EVENT,
				 "0227 Node Authentication timeout\n");
		lpfc_disc_flush_list(vport);

		 
		if (phba->sli_rev < LPFC_SLI_REV4) {
			if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED)
				lpfc_issue_reg_vpi(phba, vport);
			else  {	 
				lpfc_issue_clear_la(phba, vport);
				vport->port_state = LPFC_VPORT_READY;
			}
		}
		break;

	case LPFC_VPORT_READY:
		if (vport->fc_flag & FC_RSCN_MODE) {
			lpfc_printf_vlog(vport, KERN_ERR,
					 LOG_TRACE_EVENT,
					 "0231 RSCN timeout Data: x%x "
					 "x%x x%x x%x\n",
					 vport->fc_ns_retry, LPFC_MAX_NS_RETRY,
					 vport->port_state, vport->gidft_inp);

			 
			lpfc_els_flush_cmd(vport);

			lpfc_els_flush_rscn(vport);
			lpfc_disc_flush_list(vport);
		}
		break;

	default:
		lpfc_printf_vlog(vport, KERN_ERR,
				 LOG_TRACE_EVENT,
				 "0273 Unexpected discovery timeout, "
				 "vport State x%x\n", vport->port_state);
		break;
	}

	switch (phba->link_state) {
	case LPFC_CLEAR_LA:
				 
		lpfc_printf_vlog(vport, KERN_ERR,
				 LOG_TRACE_EVENT,
				 "0228 CLEAR LA timeout\n");
		clrlaerr = 1;
		break;

	case LPFC_LINK_UP:
		lpfc_issue_clear_la(phba, vport);
		fallthrough;
	case LPFC_LINK_UNKNOWN:
	case LPFC_WARM_START:
	case LPFC_INIT_START:
	case LPFC_INIT_MBX_CMDS:
	case LPFC_LINK_DOWN:
	case LPFC_HBA_ERROR:
		lpfc_printf_vlog(vport, KERN_ERR,
				 LOG_TRACE_EVENT,
				 "0230 Unexpected timeout, hba link "
				 "state x%x\n", phba->link_state);
		clrlaerr = 1;
		break;

	case LPFC_HBA_READY:
		break;
	}

	if (clrlaerr) {
		lpfc_disc_flush_list(vport);
		if (phba->sli_rev != LPFC_SLI_REV4) {
			psli->sli3_ring[(LPFC_EXTRA_RING)].flag &=
				~LPFC_STOP_IOCB_EVENT;
			psli->sli3_ring[LPFC_FCP_RING].flag &=
				~LPFC_STOP_IOCB_EVENT;
		}
		vport->port_state = LPFC_VPORT_READY;
	}
	return;
}

 
void
lpfc_mbx_cmpl_fdmi_reg_login(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	MAILBOX_t *mb = &pmb->u.mb;
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *)pmb->ctx_ndlp;
	struct lpfc_vport    *vport = pmb->vport;

	pmb->ctx_ndlp = NULL;

	if (phba->sli_rev < LPFC_SLI_REV4)
		ndlp->nlp_rpi = mb->un.varWords[0];
	ndlp->nlp_flag |= NLP_RPI_REGISTERED;
	ndlp->nlp_type |= NLP_FABRIC;
	lpfc_nlp_set_state(vport, ndlp, NLP_STE_UNMAPPED_NODE);
	lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE | LOG_DISCOVERY,
			 "0004 rpi:%x DID:%x flg:%x %d x%px\n",
			 ndlp->nlp_rpi, ndlp->nlp_DID, ndlp->nlp_flag,
			 kref_read(&ndlp->kref),
			 ndlp);
	 
	if (vport->port_type == LPFC_PHYSICAL_PORT) {
		phba->link_flag &= ~LS_CT_VEN_RPA;  
		lpfc_fdmi_cmd(vport, ndlp, SLI_MGMT_DHBA, 0);
	} else {
		lpfc_fdmi_cmd(vport, ndlp, SLI_MGMT_DPRT, 0);
	}


	 
	lpfc_nlp_put(ndlp);
	lpfc_mbox_rsrc_cleanup(phba, pmb, MBOX_THD_UNLOCKED);
	return;
}

static int
lpfc_filter_by_rpi(struct lpfc_nodelist *ndlp, void *param)
{
	uint16_t *rpi = param;

	return ndlp->nlp_rpi == *rpi;
}

static int
lpfc_filter_by_wwpn(struct lpfc_nodelist *ndlp, void *param)
{
	return memcmp(&ndlp->nlp_portname, param,
		      sizeof(ndlp->nlp_portname)) == 0;
}

static struct lpfc_nodelist *
__lpfc_find_node(struct lpfc_vport *vport, node_filter filter, void *param)
{
	struct lpfc_nodelist *ndlp;

	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
		if (filter(ndlp, param)) {
			lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE,
					 "3185 FIND node filter %ps DID "
					 "ndlp x%px did x%x flg x%x st x%x "
					 "xri x%x type x%x rpi x%x\n",
					 filter, ndlp, ndlp->nlp_DID,
					 ndlp->nlp_flag, ndlp->nlp_state,
					 ndlp->nlp_xri, ndlp->nlp_type,
					 ndlp->nlp_rpi);
			return ndlp;
		}
	}
	lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE,
			 "3186 FIND node filter %ps NOT FOUND.\n", filter);
	return NULL;
}

 
struct lpfc_nodelist *
__lpfc_findnode_rpi(struct lpfc_vport *vport, uint16_t rpi)
{
	return __lpfc_find_node(vport, lpfc_filter_by_rpi, &rpi);
}

 
struct lpfc_nodelist *
lpfc_findnode_wwpn(struct lpfc_vport *vport, struct lpfc_name *wwpn)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp;

	spin_lock_irq(shost->host_lock);
	ndlp = __lpfc_find_node(vport, lpfc_filter_by_wwpn, wwpn);
	spin_unlock_irq(shost->host_lock);
	return ndlp;
}

 
struct lpfc_nodelist *
lpfc_findnode_rpi(struct lpfc_vport *vport, uint16_t rpi)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp;
	unsigned long flags;

	spin_lock_irqsave(shost->host_lock, flags);
	ndlp = __lpfc_findnode_rpi(vport, rpi);
	spin_unlock_irqrestore(shost->host_lock, flags);
	return ndlp;
}

 
struct lpfc_vport *
lpfc_find_vport_by_vpid(struct lpfc_hba *phba, uint16_t vpi)
{
	struct lpfc_vport *vport;
	unsigned long flags;
	int i = 0;

	 
	if (vpi > 0) {
		 
		for (i = 0; i <= phba->max_vpi; i++) {
			if (vpi == phba->vpi_ids[i])
				break;
		}

		if (i > phba->max_vpi) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"2936 Could not find Vport mapped "
					"to vpi %d\n", vpi);
			return NULL;
		}
	}

	spin_lock_irqsave(&phba->port_list_lock, flags);
	list_for_each_entry(vport, &phba->port_list, listentry) {
		if (vport->vpi == i) {
			spin_unlock_irqrestore(&phba->port_list_lock, flags);
			return vport;
		}
	}
	spin_unlock_irqrestore(&phba->port_list_lock, flags);
	return NULL;
}

struct lpfc_nodelist *
lpfc_nlp_init(struct lpfc_vport *vport, uint32_t did)
{
	struct lpfc_nodelist *ndlp;
	int rpi = LPFC_RPI_ALLOC_ERROR;

	if (vport->phba->sli_rev == LPFC_SLI_REV4) {
		rpi = lpfc_sli4_alloc_rpi(vport->phba);
		if (rpi == LPFC_RPI_ALLOC_ERROR)
			return NULL;
	}

	ndlp = mempool_alloc(vport->phba->nlp_mem_pool, GFP_KERNEL);
	if (!ndlp) {
		if (vport->phba->sli_rev == LPFC_SLI_REV4)
			lpfc_sli4_free_rpi(vport->phba, rpi);
		return NULL;
	}

	memset(ndlp, 0, sizeof (struct lpfc_nodelist));

	spin_lock_init(&ndlp->lock);

	lpfc_initialize_node(vport, ndlp, did);
	INIT_LIST_HEAD(&ndlp->nlp_listp);
	if (vport->phba->sli_rev == LPFC_SLI_REV4) {
		ndlp->nlp_rpi = rpi;
		lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE | LOG_DISCOVERY,
				 "0007 Init New ndlp x%px, rpi:x%x DID:%x "
				 "flg:x%x refcnt:%d\n",
				 ndlp, ndlp->nlp_rpi, ndlp->nlp_DID,
				 ndlp->nlp_flag, kref_read(&ndlp->kref));

		ndlp->active_rrqs_xri_bitmap =
				mempool_alloc(vport->phba->active_rrq_pool,
					      GFP_KERNEL);
		if (ndlp->active_rrqs_xri_bitmap)
			memset(ndlp->active_rrqs_xri_bitmap, 0,
			       ndlp->phba->cfg_rrq_xri_bitmap_sz);
	}



	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_NODE,
		"node init:       did:x%x",
		ndlp->nlp_DID, 0, 0);

	return ndlp;
}

 
static void
lpfc_nlp_release(struct kref *kref)
{
	struct lpfc_nodelist *ndlp = container_of(kref, struct lpfc_nodelist,
						  kref);
	struct lpfc_vport *vport = ndlp->vport;

	lpfc_debugfs_disc_trc(ndlp->vport, LPFC_DISC_TRC_NODE,
		"node release:    did:x%x flg:x%x type:x%x",
		ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_type);

	lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE,
			 "0279 %s: ndlp: x%px did %x refcnt:%d rpi:%x\n",
			 __func__, ndlp, ndlp->nlp_DID,
			 kref_read(&ndlp->kref), ndlp->nlp_rpi);

	 
	lpfc_cancel_retry_delay_tmo(vport, ndlp);
	lpfc_cleanup_node(vport, ndlp);

	 
	if (ndlp->nlp_flag & NLP_RELEASE_RPI) {
		if (ndlp->nlp_rpi != LPFC_RPI_ALLOC_ERROR &&
		    !(ndlp->nlp_flag & (NLP_RPI_REGISTERED | NLP_UNREG_INP))) {
			lpfc_sli4_free_rpi(vport->phba, ndlp->nlp_rpi);
			ndlp->nlp_rpi = LPFC_RPI_ALLOC_ERROR;
		}
	}

	 
	ndlp->vport = NULL;
	ndlp->nlp_state = NLP_STE_FREED_NODE;
	ndlp->nlp_flag = 0;
	ndlp->fc4_xpt_flags = 0;

	 
	if (ndlp->phba->sli_rev == LPFC_SLI_REV4)
		mempool_free(ndlp->active_rrqs_xri_bitmap,
				ndlp->phba->active_rrq_pool);
	mempool_free(ndlp, ndlp->phba->nlp_mem_pool);
}

 
struct lpfc_nodelist *
lpfc_nlp_get(struct lpfc_nodelist *ndlp)
{
	unsigned long flags;

	if (ndlp) {
		lpfc_debugfs_disc_trc(ndlp->vport, LPFC_DISC_TRC_NODE,
			"node get:        did:x%x flg:x%x refcnt:x%x",
			ndlp->nlp_DID, ndlp->nlp_flag,
			kref_read(&ndlp->kref));

		 
		spin_lock_irqsave(&ndlp->lock, flags);
		if (!kref_get_unless_zero(&ndlp->kref)) {
			spin_unlock_irqrestore(&ndlp->lock, flags);
			lpfc_printf_vlog(ndlp->vport, KERN_WARNING, LOG_NODE,
				"0276 %s: ndlp:x%px refcnt:%d\n",
				__func__, (void *)ndlp, kref_read(&ndlp->kref));
			return NULL;
		}
		spin_unlock_irqrestore(&ndlp->lock, flags);
	} else {
		WARN_ONCE(!ndlp, "**** %s, get ref on NULL ndlp!", __func__);
	}

	return ndlp;
}

 
int
lpfc_nlp_put(struct lpfc_nodelist *ndlp)
{
	if (ndlp) {
		lpfc_debugfs_disc_trc(ndlp->vport, LPFC_DISC_TRC_NODE,
				"node put:        did:x%x flg:x%x refcnt:x%x",
				ndlp->nlp_DID, ndlp->nlp_flag,
				kref_read(&ndlp->kref));
	} else {
		WARN_ONCE(!ndlp, "**** %s, put ref on NULL ndlp!", __func__);
	}

	return ndlp ? kref_put(&ndlp->kref, lpfc_nlp_release) : 0;
}

 
static int
lpfc_fcf_inuse(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	int i, ret = 0;
	struct lpfc_nodelist *ndlp;
	struct Scsi_Host  *shost;

	vports = lpfc_create_vport_work_array(phba);

	 
	if (!vports)
		return 1;

	for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
		shost = lpfc_shost_from_vport(vports[i]);
		spin_lock_irq(shost->host_lock);
		 
		if (!(vports[i]->fc_flag & FC_VPORT_CVL_RCVD)) {
			spin_unlock_irq(shost->host_lock);
			ret =  1;
			goto out;
		}
		list_for_each_entry(ndlp, &vports[i]->fc_nodes, nlp_listp) {
			if (ndlp->rport &&
			  (ndlp->rport->roles & FC_RPORT_ROLE_FCP_TARGET)) {
				ret = 1;
				spin_unlock_irq(shost->host_lock);
				goto out;
			} else if (ndlp->nlp_flag & NLP_RPI_REGISTERED) {
				ret = 1;
				lpfc_printf_log(phba, KERN_INFO,
						LOG_NODE | LOG_DISCOVERY,
						"2624 RPI %x DID %x flag %x "
						"still logged in\n",
						ndlp->nlp_rpi, ndlp->nlp_DID,
						ndlp->nlp_flag);
			}
		}
		spin_unlock_irq(shost->host_lock);
	}
out:
	lpfc_destroy_vport_work_array(phba, vports);
	return ret;
}

 
void
lpfc_unregister_vfi_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	struct lpfc_vport *vport = mboxq->vport;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	if (mboxq->u.mb.mbxStatus) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2555 UNREG_VFI mbxStatus error x%x "
				"HBA state x%x\n",
				mboxq->u.mb.mbxStatus, vport->port_state);
	}
	spin_lock_irq(shost->host_lock);
	phba->pport->fc_flag &= ~FC_VFI_REGISTERED;
	spin_unlock_irq(shost->host_lock);
	mempool_free(mboxq, phba->mbox_mem_pool);
	return;
}

 
static void
lpfc_unregister_fcfi_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	struct lpfc_vport *vport = mboxq->vport;

	if (mboxq->u.mb.mbxStatus) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2550 UNREG_FCFI mbxStatus error x%x "
				"HBA state x%x\n",
				mboxq->u.mb.mbxStatus, vport->port_state);
	}
	mempool_free(mboxq, phba->mbox_mem_pool);
	return;
}

 
int
lpfc_unregister_fcf_prep(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	struct lpfc_nodelist *ndlp;
	struct Scsi_Host *shost;
	int i = 0, rc;

	 
	if (lpfc_fcf_inuse(phba))
		lpfc_unreg_hba_rpis(phba);

	 
	phba->pport->port_state = LPFC_VPORT_UNKNOWN;

	 
	vports = lpfc_create_vport_work_array(phba);
	if (vports && (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED))
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			 
			ndlp = lpfc_findnode_did(vports[i], Fabric_DID);
			if (ndlp)
				lpfc_cancel_retry_delay_tmo(vports[i], ndlp);
			lpfc_cleanup_pending_mbox(vports[i]);
			if (phba->sli_rev == LPFC_SLI_REV4)
				lpfc_sli4_unreg_all_rpis(vports[i]);
			lpfc_mbx_unreg_vpi(vports[i]);
			shost = lpfc_shost_from_vport(vports[i]);
			spin_lock_irq(shost->host_lock);
			vports[i]->fc_flag |= FC_VPORT_NEEDS_INIT_VPI;
			vports[i]->vpi_state &= ~LPFC_VPI_REGISTERED;
			spin_unlock_irq(shost->host_lock);
		}
	lpfc_destroy_vport_work_array(phba, vports);
	if (i == 0 && (!(phba->sli3_options & LPFC_SLI3_NPIV_ENABLED))) {
		ndlp = lpfc_findnode_did(phba->pport, Fabric_DID);
		if (ndlp)
			lpfc_cancel_retry_delay_tmo(phba->pport, ndlp);
		lpfc_cleanup_pending_mbox(phba->pport);
		if (phba->sli_rev == LPFC_SLI_REV4)
			lpfc_sli4_unreg_all_rpis(phba->pport);
		lpfc_mbx_unreg_vpi(phba->pport);
		shost = lpfc_shost_from_vport(phba->pport);
		spin_lock_irq(shost->host_lock);
		phba->pport->fc_flag |= FC_VPORT_NEEDS_INIT_VPI;
		phba->pport->vpi_state &= ~LPFC_VPI_REGISTERED;
		spin_unlock_irq(shost->host_lock);
	}

	 
	lpfc_els_flush_all_cmd(phba);

	 
	rc = lpfc_issue_unreg_vfi(phba->pport);
	return rc;
}

 
int
lpfc_sli4_unregister_fcf(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mbox;
	int rc;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2551 UNREG_FCFI mbox allocation failed"
				"HBA state x%x\n", phba->pport->port_state);
		return -ENOMEM;
	}
	lpfc_unreg_fcfi(mbox, phba->fcf.fcfi);
	mbox->vport = phba->pport;
	mbox->mbox_cmpl = lpfc_unregister_fcfi_cmpl;
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);

	if (rc == MBX_NOT_FINISHED) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2552 Unregister FCFI command failed rc x%x "
				"HBA state x%x\n",
				rc, phba->pport->port_state);
		return -EINVAL;
	}
	return 0;
}

 
void
lpfc_unregister_fcf_rescan(struct lpfc_hba *phba)
{
	int rc;

	 
	rc = lpfc_unregister_fcf_prep(phba);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2748 Failed to prepare for unregistering "
				"HBA's FCF record: rc=%d\n", rc);
		return;
	}

	 
	rc = lpfc_sli4_unregister_fcf(phba);
	if (rc)
		return;
	 
	spin_lock_irq(&phba->hbalock);
	phba->fcf.fcf_flag = 0;
	spin_unlock_irq(&phba->hbalock);
	phba->fcf.current_rec.flag = 0;

	 
	if ((phba->pport->load_flag & FC_UNLOADING) ||
	    (phba->link_state < LPFC_LINK_UP))
		return;

	 
	spin_lock_irq(&phba->hbalock);
	phba->fcf.fcf_flag |= FCF_INIT_DISC;
	spin_unlock_irq(&phba->hbalock);

	 
	lpfc_sli4_clear_fcf_rr_bmask(phba);

	rc = lpfc_sli4_fcf_scan_read_fcf_rec(phba, LPFC_FCOE_FCF_GET_FIRST);

	if (rc) {
		spin_lock_irq(&phba->hbalock);
		phba->fcf.fcf_flag &= ~FCF_INIT_DISC;
		spin_unlock_irq(&phba->hbalock);
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2553 lpfc_unregister_unused_fcf failed "
				"to read FCF record HBA state x%x\n",
				phba->pport->port_state);
	}
}

 
void
lpfc_unregister_fcf(struct lpfc_hba *phba)
{
	int rc;

	 
	rc = lpfc_unregister_fcf_prep(phba);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2749 Failed to prepare for unregistering "
				"HBA's FCF record: rc=%d\n", rc);
		return;
	}

	 
	rc = lpfc_sli4_unregister_fcf(phba);
	if (rc)
		return;
	 
	spin_lock_irq(&phba->hbalock);
	phba->fcf.fcf_flag &= ~FCF_REGISTERED;
	spin_unlock_irq(&phba->hbalock);
}

 
void
lpfc_unregister_unused_fcf(struct lpfc_hba *phba)
{
	 
	spin_lock_irq(&phba->hbalock);
	if (!(phba->hba_flag & HBA_FCOE_MODE) ||
	    !(phba->fcf.fcf_flag & FCF_REGISTERED) ||
	    !(phba->hba_flag & HBA_FIP_SUPPORT) ||
	    (phba->fcf.fcf_flag & FCF_DISCOVERY) ||
	    (phba->pport->port_state == LPFC_FLOGI)) {
		spin_unlock_irq(&phba->hbalock);
		return;
	}
	spin_unlock_irq(&phba->hbalock);

	if (lpfc_fcf_inuse(phba))
		return;

	lpfc_unregister_fcf_rescan(phba);
}

 
static void
lpfc_read_fcf_conn_tbl(struct lpfc_hba *phba,
	uint8_t *buff)
{
	struct lpfc_fcf_conn_entry *conn_entry, *next_conn_entry;
	struct lpfc_fcf_conn_hdr *conn_hdr;
	struct lpfc_fcf_conn_rec *conn_rec;
	uint32_t record_count;
	int i;

	 
	list_for_each_entry_safe(conn_entry, next_conn_entry,
		&phba->fcf_conn_rec_list, list) {
		list_del_init(&conn_entry->list);
		kfree(conn_entry);
	}

	conn_hdr = (struct lpfc_fcf_conn_hdr *) buff;
	record_count = conn_hdr->length * sizeof(uint32_t)/
		sizeof(struct lpfc_fcf_conn_rec);

	conn_rec = (struct lpfc_fcf_conn_rec *)
		(buff + sizeof(struct lpfc_fcf_conn_hdr));

	for (i = 0; i < record_count; i++) {
		if (!(conn_rec[i].flags & FCFCNCT_VALID))
			continue;
		conn_entry = kzalloc(sizeof(struct lpfc_fcf_conn_entry),
			GFP_KERNEL);
		if (!conn_entry) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"2566 Failed to allocate connection"
					" table entry\n");
			return;
		}

		memcpy(&conn_entry->conn_rec, &conn_rec[i],
			sizeof(struct lpfc_fcf_conn_rec));
		list_add_tail(&conn_entry->list,
			&phba->fcf_conn_rec_list);
	}

	if (!list_empty(&phba->fcf_conn_rec_list)) {
		i = 0;
		list_for_each_entry(conn_entry, &phba->fcf_conn_rec_list,
				    list) {
			conn_rec = &conn_entry->conn_rec;
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"3345 FCF connection list rec[%02d]: "
					"flags:x%04x, vtag:x%04x, "
					"fabric_name:x%02x:%02x:%02x:%02x:"
					"%02x:%02x:%02x:%02x, "
					"switch_name:x%02x:%02x:%02x:%02x:"
					"%02x:%02x:%02x:%02x\n", i++,
					conn_rec->flags, conn_rec->vlan_tag,
					conn_rec->fabric_name[0],
					conn_rec->fabric_name[1],
					conn_rec->fabric_name[2],
					conn_rec->fabric_name[3],
					conn_rec->fabric_name[4],
					conn_rec->fabric_name[5],
					conn_rec->fabric_name[6],
					conn_rec->fabric_name[7],
					conn_rec->switch_name[0],
					conn_rec->switch_name[1],
					conn_rec->switch_name[2],
					conn_rec->switch_name[3],
					conn_rec->switch_name[4],
					conn_rec->switch_name[5],
					conn_rec->switch_name[6],
					conn_rec->switch_name[7]);
		}
	}
}

 
static void
lpfc_read_fcoe_param(struct lpfc_hba *phba,
			uint8_t *buff)
{
	struct lpfc_fip_param_hdr *fcoe_param_hdr;
	struct lpfc_fcoe_params *fcoe_param;

	fcoe_param_hdr = (struct lpfc_fip_param_hdr *)
		buff;
	fcoe_param = (struct lpfc_fcoe_params *)
		(buff + sizeof(struct lpfc_fip_param_hdr));

	if ((fcoe_param_hdr->parm_version != FIPP_VERSION) ||
		(fcoe_param_hdr->length != FCOE_PARAM_LENGTH))
		return;

	if (fcoe_param_hdr->parm_flags & FIPP_VLAN_VALID) {
		phba->valid_vlan = 1;
		phba->vlan_id = le16_to_cpu(fcoe_param->vlan_tag) &
			0xFFF;
	}

	phba->fc_map[0] = fcoe_param->fc_map[0];
	phba->fc_map[1] = fcoe_param->fc_map[1];
	phba->fc_map[2] = fcoe_param->fc_map[2];
	return;
}

 
static uint8_t *
lpfc_get_rec_conf23(uint8_t *buff, uint32_t size, uint8_t rec_type)
{
	uint32_t offset = 0, rec_length;

	if ((buff[0] == LPFC_REGION23_LAST_REC) ||
		(size < sizeof(uint32_t)))
		return NULL;

	rec_length = buff[offset + 1];

	 
	while ((offset + rec_length * sizeof(uint32_t) + sizeof(uint32_t))
		<= size) {
		if (buff[offset] == rec_type)
			return &buff[offset];

		if (buff[offset] == LPFC_REGION23_LAST_REC)
			return NULL;

		offset += rec_length * sizeof(uint32_t) + sizeof(uint32_t);
		rec_length = buff[offset + 1];
	}
	return NULL;
}

 
void
lpfc_parse_fcoe_conf(struct lpfc_hba *phba,
		uint8_t *buff,
		uint32_t size)
{
	uint32_t offset = 0;
	uint8_t *rec_ptr;

	 
	if (size < 2*sizeof(uint32_t))
		return;

	 
	if (memcmp(buff, LPFC_REGION23_SIGNATURE, 4)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"2567 Config region 23 has bad signature\n");
		return;
	}

	offset += 4;

	 
	if (buff[offset] != LPFC_REGION23_VERSION) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2568 Config region 23 has bad version\n");
		return;
	}
	offset += 4;

	 
	rec_ptr = lpfc_get_rec_conf23(&buff[offset],
			size - offset, FCOE_PARAM_TYPE);
	if (rec_ptr)
		lpfc_read_fcoe_param(phba, rec_ptr);

	 
	rec_ptr = lpfc_get_rec_conf23(&buff[offset],
		size - offset, FCOE_CONN_TBL_TYPE);
	if (rec_ptr)
		lpfc_read_fcf_conn_tbl(phba, rec_ptr);

}

 
bool
lpfc_error_lost_link(struct lpfc_vport *vport, u32 ulp_status, u32 ulp_word4)
{
	 
	u32 rsn_code = IOERR_PARAM_MASK & ulp_word4;

	if (ulp_status == IOSTAT_LOCAL_REJECT &&
	    (rsn_code == IOERR_SLI_ABORTED ||
	     rsn_code == IOERR_LINK_DOWN ||
	     rsn_code == IOERR_SLI_DOWN)) {
		lpfc_printf_vlog(vport, KERN_WARNING, LOG_SLI | LOG_ELS,
				 "0408 Report link error true: <x%x:x%x>\n",
				 ulp_status, ulp_word4);
		return true;
	}

	return false;
}
