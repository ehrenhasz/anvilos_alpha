 

#include <linux/blkdev.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/lockdep.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_fc.h>
#include <scsi/fc/fc_fs.h>
#include <linux/crash_dump.h>
#ifdef CONFIG_X86
#include <asm/set_memory.h>
#endif

#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_nl.h"
#include "lpfc_disc.h"
#include "lpfc.h"
#include "lpfc_scsi.h"
#include "lpfc_nvme.h"
#include "lpfc_crtn.h"
#include "lpfc_logmsg.h"
#include "lpfc_compat.h"
#include "lpfc_debugfs.h"
#include "lpfc_vport.h"
#include "lpfc_version.h"

 
typedef enum _lpfc_iocb_type {
	LPFC_UNKNOWN_IOCB,
	LPFC_UNSOL_IOCB,
	LPFC_SOL_IOCB,
	LPFC_ABORT_IOCB
} lpfc_iocb_type;


 
static int lpfc_sli_issue_mbox_s4(struct lpfc_hba *, LPFC_MBOXQ_t *,
				  uint32_t);
static int lpfc_sli4_read_rev(struct lpfc_hba *, LPFC_MBOXQ_t *,
			      uint8_t *, uint32_t *);
static struct lpfc_iocbq *
lpfc_sli4_els_preprocess_rspiocbq(struct lpfc_hba *phba,
				  struct lpfc_iocbq *rspiocbq);
static void lpfc_sli4_send_seq_to_ulp(struct lpfc_vport *,
				      struct hbq_dmabuf *);
static void lpfc_sli4_handle_mds_loopback(struct lpfc_vport *vport,
					  struct hbq_dmabuf *dmabuf);
static bool lpfc_sli4_fp_handle_cqe(struct lpfc_hba *phba,
				   struct lpfc_queue *cq, struct lpfc_cqe *cqe);
static int lpfc_sli4_post_sgl_list(struct lpfc_hba *, struct list_head *,
				       int);
static void lpfc_sli4_hba_handle_eqe(struct lpfc_hba *phba,
				     struct lpfc_queue *eq,
				     struct lpfc_eqe *eqe,
				     enum lpfc_poll_mode poll_mode);
static bool lpfc_sli4_mbox_completions_pending(struct lpfc_hba *phba);
static bool lpfc_sli4_process_missed_mbox_completions(struct lpfc_hba *phba);
static struct lpfc_cqe *lpfc_sli4_cq_get(struct lpfc_queue *q);
static void __lpfc_sli4_consume_cqe(struct lpfc_hba *phba,
				    struct lpfc_queue *cq,
				    struct lpfc_cqe *cqe);
static uint16_t lpfc_wqe_bpl2sgl(struct lpfc_hba *phba,
				 struct lpfc_iocbq *pwqeq,
				 struct lpfc_sglq *sglq);

union lpfc_wqe128 lpfc_iread_cmd_template;
union lpfc_wqe128 lpfc_iwrite_cmd_template;
union lpfc_wqe128 lpfc_icmnd_cmd_template;

 
void lpfc_wqe_cmd_template(void)
{
	union lpfc_wqe128 *wqe;

	 
	wqe = &lpfc_iread_cmd_template;
	memset(wqe, 0, sizeof(union lpfc_wqe128));

	 

	 

	 

	 

	 

	 
	bf_set(wqe_cmnd, &wqe->fcp_iread.wqe_com, CMD_FCP_IREAD64_WQE);
	bf_set(wqe_pu, &wqe->fcp_iread.wqe_com, PARM_READ_CHECK);
	bf_set(wqe_class, &wqe->fcp_iread.wqe_com, CLASS3);
	bf_set(wqe_ct, &wqe->fcp_iread.wqe_com, SLI4_CT_RPI);

	 

	 

	 
	bf_set(wqe_qosd, &wqe->fcp_iread.wqe_com, 0);
	bf_set(wqe_iod, &wqe->fcp_iread.wqe_com, LPFC_WQE_IOD_READ);
	bf_set(wqe_lenloc, &wqe->fcp_iread.wqe_com, LPFC_WQE_LENLOC_WORD4);
	bf_set(wqe_dbde, &wqe->fcp_iread.wqe_com, 0);
	bf_set(wqe_wqes, &wqe->fcp_iread.wqe_com, 1);

	 
	bf_set(wqe_cmd_type, &wqe->fcp_iread.wqe_com, COMMAND_DATA_IN);
	bf_set(wqe_cqid, &wqe->fcp_iread.wqe_com, LPFC_WQE_CQ_ID_DEFAULT);
	bf_set(wqe_pbde, &wqe->fcp_iread.wqe_com, 0);

	 

	 

	 
	wqe = &lpfc_iwrite_cmd_template;
	memset(wqe, 0, sizeof(union lpfc_wqe128));

	 

	 

	 

	 

	 

	 
	bf_set(wqe_cmnd, &wqe->fcp_iwrite.wqe_com, CMD_FCP_IWRITE64_WQE);
	bf_set(wqe_pu, &wqe->fcp_iwrite.wqe_com, PARM_READ_CHECK);
	bf_set(wqe_class, &wqe->fcp_iwrite.wqe_com, CLASS3);
	bf_set(wqe_ct, &wqe->fcp_iwrite.wqe_com, SLI4_CT_RPI);

	 

	 

	 
	bf_set(wqe_qosd, &wqe->fcp_iwrite.wqe_com, 0);
	bf_set(wqe_iod, &wqe->fcp_iwrite.wqe_com, LPFC_WQE_IOD_WRITE);
	bf_set(wqe_lenloc, &wqe->fcp_iwrite.wqe_com, LPFC_WQE_LENLOC_WORD4);
	bf_set(wqe_dbde, &wqe->fcp_iwrite.wqe_com, 0);
	bf_set(wqe_wqes, &wqe->fcp_iwrite.wqe_com, 1);

	 
	bf_set(wqe_cmd_type, &wqe->fcp_iwrite.wqe_com, COMMAND_DATA_OUT);
	bf_set(wqe_cqid, &wqe->fcp_iwrite.wqe_com, LPFC_WQE_CQ_ID_DEFAULT);
	bf_set(wqe_pbde, &wqe->fcp_iwrite.wqe_com, 0);

	 

	 

	 
	wqe = &lpfc_icmnd_cmd_template;
	memset(wqe, 0, sizeof(union lpfc_wqe128));

	 

	 

	 

	 

	 
	bf_set(wqe_cmnd, &wqe->fcp_icmd.wqe_com, CMD_FCP_ICMND64_WQE);
	bf_set(wqe_pu, &wqe->fcp_icmd.wqe_com, 0);
	bf_set(wqe_class, &wqe->fcp_icmd.wqe_com, CLASS3);
	bf_set(wqe_ct, &wqe->fcp_icmd.wqe_com, SLI4_CT_RPI);

	 

	 

	 
	bf_set(wqe_qosd, &wqe->fcp_icmd.wqe_com, 1);
	bf_set(wqe_iod, &wqe->fcp_icmd.wqe_com, LPFC_WQE_IOD_NONE);
	bf_set(wqe_lenloc, &wqe->fcp_icmd.wqe_com, LPFC_WQE_LENLOC_NONE);
	bf_set(wqe_dbde, &wqe->fcp_icmd.wqe_com, 0);
	bf_set(wqe_wqes, &wqe->fcp_icmd.wqe_com, 1);

	 
	bf_set(wqe_cmd_type, &wqe->fcp_icmd.wqe_com, COMMAND_DATA_IN);
	bf_set(wqe_cqid, &wqe->fcp_icmd.wqe_com, LPFC_WQE_CQ_ID_DEFAULT);
	bf_set(wqe_pbde, &wqe->fcp_icmd.wqe_com, 0);

	 
}

#if defined(CONFIG_64BIT) && defined(__LITTLE_ENDIAN)
 
static void
lpfc_sli4_pcimem_bcopy(void *srcp, void *destp, uint32_t cnt)
{
	uint64_t *src = srcp;
	uint64_t *dest = destp;
	int i;

	for (i = 0; i < (int)cnt; i += sizeof(uint64_t))
		*dest++ = *src++;
}
#else
#define lpfc_sli4_pcimem_bcopy(a, b, c) lpfc_sli_pcimem_bcopy(a, b, c)
#endif

 
static int
lpfc_sli4_wq_put(struct lpfc_queue *q, union lpfc_wqe128 *wqe)
{
	union lpfc_wqe *temp_wqe;
	struct lpfc_register doorbell;
	uint32_t host_index;
	uint32_t idx;
	uint32_t i = 0;
	uint8_t *tmp;
	u32 if_type;

	 
	if (unlikely(!q))
		return -ENOMEM;

	temp_wqe = lpfc_sli4_qe(q, q->host_index);

	 
	idx = ((q->host_index + 1) % q->entry_count);
	if (idx == q->hba_index) {
		q->WQ_overflow++;
		return -EBUSY;
	}
	q->WQ_posted++;
	 
	if (!((q->host_index + 1) % q->notify_interval))
		bf_set(wqe_wqec, &wqe->generic.wqe_com, 1);
	else
		bf_set(wqe_wqec, &wqe->generic.wqe_com, 0);
	if (q->phba->sli3_options & LPFC_SLI4_PHWQ_ENABLED)
		bf_set(wqe_wqid, &wqe->generic.wqe_com, q->queue_id);
	lpfc_sli4_pcimem_bcopy(wqe, temp_wqe, q->entry_size);
	if (q->dpp_enable && q->phba->cfg_enable_dpp) {
		 
		tmp = (uint8_t *)temp_wqe;
#ifdef __raw_writeq
		for (i = 0; i < q->entry_size; i += sizeof(uint64_t))
			__raw_writeq(*((uint64_t *)(tmp + i)),
					q->dpp_regaddr + i);
#else
		for (i = 0; i < q->entry_size; i += sizeof(uint32_t))
			__raw_writel(*((uint32_t *)(tmp + i)),
					q->dpp_regaddr + i);
#endif
	}
	 
	wmb();

	 
	host_index = q->host_index;

	q->host_index = idx;

	 
	doorbell.word0 = 0;
	if (q->db_format == LPFC_DB_LIST_FORMAT) {
		if (q->dpp_enable && q->phba->cfg_enable_dpp) {
			bf_set(lpfc_if6_wq_db_list_fm_num_posted, &doorbell, 1);
			bf_set(lpfc_if6_wq_db_list_fm_dpp, &doorbell, 1);
			bf_set(lpfc_if6_wq_db_list_fm_dpp_id, &doorbell,
			    q->dpp_id);
			bf_set(lpfc_if6_wq_db_list_fm_id, &doorbell,
			    q->queue_id);
		} else {
			bf_set(lpfc_wq_db_list_fm_num_posted, &doorbell, 1);
			bf_set(lpfc_wq_db_list_fm_id, &doorbell, q->queue_id);

			 
			if_type = bf_get(lpfc_sli_intf_if_type,
					 &q->phba->sli4_hba.sli_intf);
			if (if_type != LPFC_SLI_INTF_IF_TYPE_6)
				bf_set(lpfc_wq_db_list_fm_index, &doorbell,
				       host_index);
		}
	} else if (q->db_format == LPFC_DB_RING_FORMAT) {
		bf_set(lpfc_wq_db_ring_fm_num_posted, &doorbell, 1);
		bf_set(lpfc_wq_db_ring_fm_id, &doorbell, q->queue_id);
	} else {
		return -EINVAL;
	}
	writel(doorbell.word0, q->db_regaddr);

	return 0;
}

 
static void
lpfc_sli4_wq_release(struct lpfc_queue *q, uint32_t index)
{
	 
	if (unlikely(!q))
		return;

	q->hba_index = index;
}

 
static uint32_t
lpfc_sli4_mq_put(struct lpfc_queue *q, struct lpfc_mqe *mqe)
{
	struct lpfc_mqe *temp_mqe;
	struct lpfc_register doorbell;

	 
	if (unlikely(!q))
		return -ENOMEM;
	temp_mqe = lpfc_sli4_qe(q, q->host_index);

	 
	if (((q->host_index + 1) % q->entry_count) == q->hba_index)
		return -ENOMEM;
	lpfc_sli4_pcimem_bcopy(mqe, temp_mqe, q->entry_size);
	 
	q->phba->mbox = (MAILBOX_t *)temp_mqe;

	 
	q->host_index = ((q->host_index + 1) % q->entry_count);

	 
	doorbell.word0 = 0;
	bf_set(lpfc_mq_doorbell_num_posted, &doorbell, 1);
	bf_set(lpfc_mq_doorbell_id, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.MQDBregaddr);
	return 0;
}

 
static uint32_t
lpfc_sli4_mq_release(struct lpfc_queue *q)
{
	 
	if (unlikely(!q))
		return 0;

	 
	q->phba->mbox = NULL;
	q->hba_index = ((q->hba_index + 1) % q->entry_count);
	return 1;
}

 
static struct lpfc_eqe *
lpfc_sli4_eq_get(struct lpfc_queue *q)
{
	struct lpfc_eqe *eqe;

	 
	if (unlikely(!q))
		return NULL;
	eqe = lpfc_sli4_qe(q, q->host_index);

	 
	if (bf_get_le32(lpfc_eqe_valid, eqe) != q->qe_valid)
		return NULL;

	 
	mb();
	return eqe;
}

 
void
lpfc_sli4_eq_clr_intr(struct lpfc_queue *q)
{
	struct lpfc_register doorbell;

	doorbell.word0 = 0;
	bf_set(lpfc_eqcq_doorbell_eqci, &doorbell, 1);
	bf_set(lpfc_eqcq_doorbell_qt, &doorbell, LPFC_QUEUE_TYPE_EVENT);
	bf_set(lpfc_eqcq_doorbell_eqid_hi, &doorbell,
		(q->queue_id >> LPFC_EQID_HI_FIELD_SHIFT));
	bf_set(lpfc_eqcq_doorbell_eqid_lo, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.EQDBregaddr);
}

 
void
lpfc_sli4_if6_eq_clr_intr(struct lpfc_queue *q)
{
	struct lpfc_register doorbell;

	doorbell.word0 = 0;
	bf_set(lpfc_if6_eq_doorbell_eqid, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.EQDBregaddr);
}

 
void
lpfc_sli4_write_eq_db(struct lpfc_hba *phba, struct lpfc_queue *q,
		     uint32_t count, bool arm)
{
	struct lpfc_register doorbell;

	 
	if (unlikely(!q || (count == 0 && !arm)))
		return;

	 
	doorbell.word0 = 0;
	if (arm) {
		bf_set(lpfc_eqcq_doorbell_arm, &doorbell, 1);
		bf_set(lpfc_eqcq_doorbell_eqci, &doorbell, 1);
	}
	bf_set(lpfc_eqcq_doorbell_num_released, &doorbell, count);
	bf_set(lpfc_eqcq_doorbell_qt, &doorbell, LPFC_QUEUE_TYPE_EVENT);
	bf_set(lpfc_eqcq_doorbell_eqid_hi, &doorbell,
			(q->queue_id >> LPFC_EQID_HI_FIELD_SHIFT));
	bf_set(lpfc_eqcq_doorbell_eqid_lo, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.EQDBregaddr);
	 
	if ((q->phba->intr_type == INTx) && (arm == LPFC_QUEUE_REARM))
		readl(q->phba->sli4_hba.EQDBregaddr);
}

 
void
lpfc_sli4_if6_write_eq_db(struct lpfc_hba *phba, struct lpfc_queue *q,
			  uint32_t count, bool arm)
{
	struct lpfc_register doorbell;

	 
	if (unlikely(!q || (count == 0 && !arm)))
		return;

	 
	doorbell.word0 = 0;
	if (arm)
		bf_set(lpfc_if6_eq_doorbell_arm, &doorbell, 1);
	bf_set(lpfc_if6_eq_doorbell_num_released, &doorbell, count);
	bf_set(lpfc_if6_eq_doorbell_eqid, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.EQDBregaddr);
	 
	if ((q->phba->intr_type == INTx) && (arm == LPFC_QUEUE_REARM))
		readl(q->phba->sli4_hba.EQDBregaddr);
}

static void
__lpfc_sli4_consume_eqe(struct lpfc_hba *phba, struct lpfc_queue *eq,
			struct lpfc_eqe *eqe)
{
	if (!phba->sli4_hba.pc_sli4_params.eqav)
		bf_set_le32(lpfc_eqe_valid, eqe, 0);

	eq->host_index = ((eq->host_index + 1) % eq->entry_count);

	 
	if (phba->sli4_hba.pc_sli4_params.eqav && !eq->host_index)
		eq->qe_valid = (eq->qe_valid) ? 0 : 1;
}

static void
lpfc_sli4_eqcq_flush(struct lpfc_hba *phba, struct lpfc_queue *eq)
{
	struct lpfc_eqe *eqe = NULL;
	u32 eq_count = 0, cq_count = 0;
	struct lpfc_cqe *cqe = NULL;
	struct lpfc_queue *cq = NULL, *childq = NULL;
	int cqid = 0;

	 
	eqe = lpfc_sli4_eq_get(eq);
	while (eqe) {
		 
		cqid = bf_get_le32(lpfc_eqe_resource_id, eqe);
		cq = NULL;

		list_for_each_entry(childq, &eq->child_list, list) {
			if (childq->queue_id == cqid) {
				cq = childq;
				break;
			}
		}
		 
		if (cq) {
			cqe = lpfc_sli4_cq_get(cq);
			while (cqe) {
				__lpfc_sli4_consume_cqe(phba, cq, cqe);
				cq_count++;
				cqe = lpfc_sli4_cq_get(cq);
			}
			 
			phba->sli4_hba.sli4_write_cq_db(phba, cq, cq_count,
			    LPFC_QUEUE_REARM);
			cq_count = 0;
		}
		__lpfc_sli4_consume_eqe(phba, eq, eqe);
		eq_count++;
		eqe = lpfc_sli4_eq_get(eq);
	}

	 
	phba->sli4_hba.sli4_write_eq_db(phba, eq, eq_count, LPFC_QUEUE_REARM);
}

static int
lpfc_sli4_process_eq(struct lpfc_hba *phba, struct lpfc_queue *eq,
		     u8 rearm, enum lpfc_poll_mode poll_mode)
{
	struct lpfc_eqe *eqe;
	int count = 0, consumed = 0;

	if (cmpxchg(&eq->queue_claimed, 0, 1) != 0)
		goto rearm_and_exit;

	eqe = lpfc_sli4_eq_get(eq);
	while (eqe) {
		lpfc_sli4_hba_handle_eqe(phba, eq, eqe, poll_mode);
		__lpfc_sli4_consume_eqe(phba, eq, eqe);

		consumed++;
		if (!(++count % eq->max_proc_limit))
			break;

		if (!(count % eq->notify_interval)) {
			phba->sli4_hba.sli4_write_eq_db(phba, eq, consumed,
							LPFC_QUEUE_NOARM);
			consumed = 0;
		}

		eqe = lpfc_sli4_eq_get(eq);
	}
	eq->EQ_processed += count;

	 
	if (count > eq->EQ_max_eqe)
		eq->EQ_max_eqe = count;

	xchg(&eq->queue_claimed, 0);

rearm_and_exit:
	 
	phba->sli4_hba.sli4_write_eq_db(phba, eq, consumed, rearm);

	return count;
}

 
static struct lpfc_cqe *
lpfc_sli4_cq_get(struct lpfc_queue *q)
{
	struct lpfc_cqe *cqe;

	 
	if (unlikely(!q))
		return NULL;
	cqe = lpfc_sli4_qe(q, q->host_index);

	 
	if (bf_get_le32(lpfc_cqe_valid, cqe) != q->qe_valid)
		return NULL;

	 
	mb();
	return cqe;
}

static void
__lpfc_sli4_consume_cqe(struct lpfc_hba *phba, struct lpfc_queue *cq,
			struct lpfc_cqe *cqe)
{
	if (!phba->sli4_hba.pc_sli4_params.cqav)
		bf_set_le32(lpfc_cqe_valid, cqe, 0);

	cq->host_index = ((cq->host_index + 1) % cq->entry_count);

	 
	if (phba->sli4_hba.pc_sli4_params.cqav && !cq->host_index)
		cq->qe_valid = (cq->qe_valid) ? 0 : 1;
}

 
void
lpfc_sli4_write_cq_db(struct lpfc_hba *phba, struct lpfc_queue *q,
		     uint32_t count, bool arm)
{
	struct lpfc_register doorbell;

	 
	if (unlikely(!q || (count == 0 && !arm)))
		return;

	 
	doorbell.word0 = 0;
	if (arm)
		bf_set(lpfc_eqcq_doorbell_arm, &doorbell, 1);
	bf_set(lpfc_eqcq_doorbell_num_released, &doorbell, count);
	bf_set(lpfc_eqcq_doorbell_qt, &doorbell, LPFC_QUEUE_TYPE_COMPLETION);
	bf_set(lpfc_eqcq_doorbell_cqid_hi, &doorbell,
			(q->queue_id >> LPFC_CQID_HI_FIELD_SHIFT));
	bf_set(lpfc_eqcq_doorbell_cqid_lo, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.CQDBregaddr);
}

 
void
lpfc_sli4_if6_write_cq_db(struct lpfc_hba *phba, struct lpfc_queue *q,
			 uint32_t count, bool arm)
{
	struct lpfc_register doorbell;

	 
	if (unlikely(!q || (count == 0 && !arm)))
		return;

	 
	doorbell.word0 = 0;
	if (arm)
		bf_set(lpfc_if6_cq_doorbell_arm, &doorbell, 1);
	bf_set(lpfc_if6_cq_doorbell_num_released, &doorbell, count);
	bf_set(lpfc_if6_cq_doorbell_cqid, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.CQDBregaddr);
}

 
int
lpfc_sli4_rq_put(struct lpfc_queue *hq, struct lpfc_queue *dq,
		 struct lpfc_rqe *hrqe, struct lpfc_rqe *drqe)
{
	struct lpfc_rqe *temp_hrqe;
	struct lpfc_rqe *temp_drqe;
	struct lpfc_register doorbell;
	int hq_put_index;
	int dq_put_index;

	 
	if (unlikely(!hq) || unlikely(!dq))
		return -ENOMEM;
	hq_put_index = hq->host_index;
	dq_put_index = dq->host_index;
	temp_hrqe = lpfc_sli4_qe(hq, hq_put_index);
	temp_drqe = lpfc_sli4_qe(dq, dq_put_index);

	if (hq->type != LPFC_HRQ || dq->type != LPFC_DRQ)
		return -EINVAL;
	if (hq_put_index != dq_put_index)
		return -EINVAL;
	 
	if (((hq_put_index + 1) % hq->entry_count) == hq->hba_index)
		return -EBUSY;
	lpfc_sli4_pcimem_bcopy(hrqe, temp_hrqe, hq->entry_size);
	lpfc_sli4_pcimem_bcopy(drqe, temp_drqe, dq->entry_size);

	 
	hq->host_index = ((hq_put_index + 1) % hq->entry_count);
	dq->host_index = ((dq_put_index + 1) % dq->entry_count);
	hq->RQ_buf_posted++;

	 
	if (!(hq->host_index % hq->notify_interval)) {
		doorbell.word0 = 0;
		if (hq->db_format == LPFC_DB_RING_FORMAT) {
			bf_set(lpfc_rq_db_ring_fm_num_posted, &doorbell,
			       hq->notify_interval);
			bf_set(lpfc_rq_db_ring_fm_id, &doorbell, hq->queue_id);
		} else if (hq->db_format == LPFC_DB_LIST_FORMAT) {
			bf_set(lpfc_rq_db_list_fm_num_posted, &doorbell,
			       hq->notify_interval);
			bf_set(lpfc_rq_db_list_fm_index, &doorbell,
			       hq->host_index);
			bf_set(lpfc_rq_db_list_fm_id, &doorbell, hq->queue_id);
		} else {
			return -EINVAL;
		}
		writel(doorbell.word0, hq->db_regaddr);
	}
	return hq_put_index;
}

 
static uint32_t
lpfc_sli4_rq_release(struct lpfc_queue *hq, struct lpfc_queue *dq)
{
	 
	if (unlikely(!hq) || unlikely(!dq))
		return 0;

	if ((hq->type != LPFC_HRQ) || (dq->type != LPFC_DRQ))
		return 0;
	hq->hba_index = ((hq->hba_index + 1) % hq->entry_count);
	dq->hba_index = ((dq->hba_index + 1) % dq->entry_count);
	return 1;
}

 
static inline IOCB_t *
lpfc_cmd_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	return (IOCB_t *) (((char *) pring->sli.sli3.cmdringaddr) +
			   pring->sli.sli3.cmdidx * phba->iocb_cmd_size);
}

 
static inline IOCB_t *
lpfc_resp_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	return (IOCB_t *) (((char *) pring->sli.sli3.rspringaddr) +
			   pring->sli.sli3.rspidx * phba->iocb_rsp_size);
}

 
struct lpfc_iocbq *
__lpfc_sli_get_iocbq(struct lpfc_hba *phba)
{
	struct list_head *lpfc_iocb_list = &phba->lpfc_iocb_list;
	struct lpfc_iocbq * iocbq = NULL;

	lockdep_assert_held(&phba->hbalock);

	list_remove_head(lpfc_iocb_list, iocbq, struct lpfc_iocbq, list);
	if (iocbq)
		phba->iocb_cnt++;
	if (phba->iocb_cnt > phba->iocb_max)
		phba->iocb_max = phba->iocb_cnt;
	return iocbq;
}

 
struct lpfc_sglq *
__lpfc_clear_active_sglq(struct lpfc_hba *phba, uint16_t xritag)
{
	struct lpfc_sglq *sglq;

	sglq = phba->sli4_hba.lpfc_sglq_active_list[xritag];
	phba->sli4_hba.lpfc_sglq_active_list[xritag] = NULL;
	return sglq;
}

 
struct lpfc_sglq *
__lpfc_get_active_sglq(struct lpfc_hba *phba, uint16_t xritag)
{
	struct lpfc_sglq *sglq;

	sglq =  phba->sli4_hba.lpfc_sglq_active_list[xritag];
	return sglq;
}

 
void
lpfc_clr_rrq_active(struct lpfc_hba *phba,
		    uint16_t xritag,
		    struct lpfc_node_rrq *rrq)
{
	struct lpfc_nodelist *ndlp = NULL;

	 
	if (rrq->vport)
		ndlp = lpfc_findnode_did(rrq->vport, rrq->nlp_DID);

	if (!ndlp)
		goto out;

	if (test_and_clear_bit(xritag, ndlp->active_rrqs_xri_bitmap)) {
		rrq->send_rrq = 0;
		rrq->xritag = 0;
		rrq->rrq_stop_time = 0;
	}
out:
	mempool_free(rrq, phba->rrq_pool);
}

 
void
lpfc_handle_rrq_active(struct lpfc_hba *phba)
{
	struct lpfc_node_rrq *rrq;
	struct lpfc_node_rrq *nextrrq;
	unsigned long next_time;
	unsigned long iflags;
	LIST_HEAD(send_rrq);

	spin_lock_irqsave(&phba->hbalock, iflags);
	phba->hba_flag &= ~HBA_RRQ_ACTIVE;
	next_time = jiffies + msecs_to_jiffies(1000 * (phba->fc_ratov + 1));
	list_for_each_entry_safe(rrq, nextrrq,
				 &phba->active_rrq_list, list) {
		if (time_after(jiffies, rrq->rrq_stop_time))
			list_move(&rrq->list, &send_rrq);
		else if (time_before(rrq->rrq_stop_time, next_time))
			next_time = rrq->rrq_stop_time;
	}
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	if ((!list_empty(&phba->active_rrq_list)) &&
	    (!(phba->pport->load_flag & FC_UNLOADING)))
		mod_timer(&phba->rrq_tmr, next_time);
	list_for_each_entry_safe(rrq, nextrrq, &send_rrq, list) {
		list_del(&rrq->list);
		if (!rrq->send_rrq) {
			 
			lpfc_clr_rrq_active(phba, rrq->xritag, rrq);
		} else if (lpfc_send_rrq(phba, rrq)) {
			 
			lpfc_clr_rrq_active(phba, rrq->xritag,
					    rrq);
		}
	}
}

 
struct lpfc_node_rrq *
lpfc_get_active_rrq(struct lpfc_vport *vport, uint16_t xri, uint32_t did)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_node_rrq *rrq;
	struct lpfc_node_rrq *nextrrq;
	unsigned long iflags;

	if (phba->sli_rev != LPFC_SLI_REV4)
		return NULL;
	spin_lock_irqsave(&phba->hbalock, iflags);
	list_for_each_entry_safe(rrq, nextrrq, &phba->active_rrq_list, list) {
		if (rrq->vport == vport && rrq->xritag == xri &&
				rrq->nlp_DID == did){
			list_del(&rrq->list);
			spin_unlock_irqrestore(&phba->hbalock, iflags);
			return rrq;
		}
	}
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return NULL;
}

 
void
lpfc_cleanup_vports_rrqs(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)

{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_node_rrq *rrq;
	struct lpfc_node_rrq *nextrrq;
	unsigned long iflags;
	LIST_HEAD(rrq_list);

	if (phba->sli_rev != LPFC_SLI_REV4)
		return;
	if (!ndlp) {
		lpfc_sli4_vport_delete_els_xri_aborted(vport);
		lpfc_sli4_vport_delete_fcp_xri_aborted(vport);
	}
	spin_lock_irqsave(&phba->hbalock, iflags);
	list_for_each_entry_safe(rrq, nextrrq, &phba->active_rrq_list, list) {
		if (rrq->vport != vport)
			continue;

		if (!ndlp || ndlp == lpfc_findnode_did(vport, rrq->nlp_DID))
			list_move(&rrq->list, &rrq_list);

	}
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	list_for_each_entry_safe(rrq, nextrrq, &rrq_list, list) {
		list_del(&rrq->list);
		lpfc_clr_rrq_active(phba, rrq->xritag, rrq);
	}
}

 
int
lpfc_test_rrq_active(struct lpfc_hba *phba, struct lpfc_nodelist *ndlp,
			uint16_t  xritag)
{
	if (!ndlp)
		return 0;
	if (!ndlp->active_rrqs_xri_bitmap)
		return 0;
	if (test_bit(xritag, ndlp->active_rrqs_xri_bitmap))
		return 1;
	else
		return 0;
}

 
int
lpfc_set_rrq_active(struct lpfc_hba *phba, struct lpfc_nodelist *ndlp,
		    uint16_t xritag, uint16_t rxid, uint16_t send_rrq)
{
	unsigned long iflags;
	struct lpfc_node_rrq *rrq;
	int empty;

	if (!ndlp)
		return -EINVAL;

	if (!phba->cfg_enable_rrq)
		return -EINVAL;

	spin_lock_irqsave(&phba->hbalock, iflags);
	if (phba->pport->load_flag & FC_UNLOADING) {
		phba->hba_flag &= ~HBA_RRQ_ACTIVE;
		goto out;
	}

	if (ndlp->vport && (ndlp->vport->load_flag & FC_UNLOADING))
		goto out;

	if (!ndlp->active_rrqs_xri_bitmap)
		goto out;

	if (test_and_set_bit(xritag, ndlp->active_rrqs_xri_bitmap))
		goto out;

	spin_unlock_irqrestore(&phba->hbalock, iflags);
	rrq = mempool_alloc(phba->rrq_pool, GFP_ATOMIC);
	if (!rrq) {
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"3155 Unable to allocate RRQ xri:0x%x rxid:0x%x"
				" DID:0x%x Send:%d\n",
				xritag, rxid, ndlp->nlp_DID, send_rrq);
		return -EINVAL;
	}
	if (phba->cfg_enable_rrq == 1)
		rrq->send_rrq = send_rrq;
	else
		rrq->send_rrq = 0;
	rrq->xritag = xritag;
	rrq->rrq_stop_time = jiffies +
				msecs_to_jiffies(1000 * (phba->fc_ratov + 1));
	rrq->nlp_DID = ndlp->nlp_DID;
	rrq->vport = ndlp->vport;
	rrq->rxid = rxid;
	spin_lock_irqsave(&phba->hbalock, iflags);
	empty = list_empty(&phba->active_rrq_list);
	list_add_tail(&rrq->list, &phba->active_rrq_list);
	phba->hba_flag |= HBA_RRQ_ACTIVE;
	if (empty)
		lpfc_worker_wake_up(phba);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return 0;
out:
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"2921 Can't set rrq active xri:0x%x rxid:0x%x"
			" DID:0x%x Send:%d\n",
			xritag, rxid, ndlp->nlp_DID, send_rrq);
	return -EINVAL;
}

 
static struct lpfc_sglq *
__lpfc_sli_get_els_sglq(struct lpfc_hba *phba, struct lpfc_iocbq *piocbq)
{
	struct list_head *lpfc_els_sgl_list = &phba->sli4_hba.lpfc_els_sgl_list;
	struct lpfc_sglq *sglq = NULL;
	struct lpfc_sglq *start_sglq = NULL;
	struct lpfc_io_buf *lpfc_cmd;
	struct lpfc_nodelist *ndlp;
	int found = 0;
	u8 cmnd;

	cmnd = get_job_cmnd(phba, piocbq);

	if (piocbq->cmd_flag & LPFC_IO_FCP) {
		lpfc_cmd = piocbq->io_buf;
		ndlp = lpfc_cmd->rdata->pnode;
	} else  if ((cmnd == CMD_GEN_REQUEST64_CR) &&
			!(piocbq->cmd_flag & LPFC_IO_LIBDFC)) {
		ndlp = piocbq->ndlp;
	} else  if (piocbq->cmd_flag & LPFC_IO_LIBDFC) {
		if (piocbq->cmd_flag & LPFC_IO_LOOPBACK)
			ndlp = NULL;
		else
			ndlp = piocbq->ndlp;
	} else {
		ndlp = piocbq->ndlp;
	}

	spin_lock(&phba->sli4_hba.sgl_list_lock);
	list_remove_head(lpfc_els_sgl_list, sglq, struct lpfc_sglq, list);
	start_sglq = sglq;
	while (!found) {
		if (!sglq)
			break;
		if (ndlp && ndlp->active_rrqs_xri_bitmap &&
		    test_bit(sglq->sli4_lxritag,
		    ndlp->active_rrqs_xri_bitmap)) {
			 
			list_add_tail(&sglq->list, lpfc_els_sgl_list);
			sglq = NULL;
			list_remove_head(lpfc_els_sgl_list, sglq,
						struct lpfc_sglq, list);
			if (sglq == start_sglq) {
				list_add_tail(&sglq->list, lpfc_els_sgl_list);
				sglq = NULL;
				break;
			} else
				continue;
		}
		sglq->ndlp = ndlp;
		found = 1;
		phba->sli4_hba.lpfc_sglq_active_list[sglq->sli4_lxritag] = sglq;
		sglq->state = SGL_ALLOCATED;
	}
	spin_unlock(&phba->sli4_hba.sgl_list_lock);
	return sglq;
}

 
struct lpfc_sglq *
__lpfc_sli_get_nvmet_sglq(struct lpfc_hba *phba, struct lpfc_iocbq *piocbq)
{
	struct list_head *lpfc_nvmet_sgl_list;
	struct lpfc_sglq *sglq = NULL;

	lpfc_nvmet_sgl_list = &phba->sli4_hba.lpfc_nvmet_sgl_list;

	lockdep_assert_held(&phba->sli4_hba.sgl_list_lock);

	list_remove_head(lpfc_nvmet_sgl_list, sglq, struct lpfc_sglq, list);
	if (!sglq)
		return NULL;
	phba->sli4_hba.lpfc_sglq_active_list[sglq->sli4_lxritag] = sglq;
	sglq->state = SGL_ALLOCATED;
	return sglq;
}

 
struct lpfc_iocbq *
lpfc_sli_get_iocbq(struct lpfc_hba *phba)
{
	struct lpfc_iocbq * iocbq = NULL;
	unsigned long iflags;

	spin_lock_irqsave(&phba->hbalock, iflags);
	iocbq = __lpfc_sli_get_iocbq(phba);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return iocbq;
}

 
static void
__lpfc_sli_release_iocbq_s4(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	struct lpfc_sglq *sglq;
	unsigned long iflag = 0;
	struct lpfc_sli_ring *pring;

	if (iocbq->sli4_xritag == NO_XRI)
		sglq = NULL;
	else
		sglq = __lpfc_clear_active_sglq(phba, iocbq->sli4_lxritag);


	if (sglq)  {
		if (iocbq->cmd_flag & LPFC_IO_NVMET) {
			spin_lock_irqsave(&phba->sli4_hba.sgl_list_lock,
					  iflag);
			sglq->state = SGL_FREED;
			sglq->ndlp = NULL;
			list_add_tail(&sglq->list,
				      &phba->sli4_hba.lpfc_nvmet_sgl_list);
			spin_unlock_irqrestore(
				&phba->sli4_hba.sgl_list_lock, iflag);
			goto out;
		}

		if ((iocbq->cmd_flag & LPFC_EXCHANGE_BUSY) &&
		    (!(unlikely(pci_channel_offline(phba->pcidev)))) &&
		    sglq->state != SGL_XRI_ABORTED) {
			spin_lock_irqsave(&phba->sli4_hba.sgl_list_lock,
					  iflag);

			 
			if (sglq->ndlp && !lpfc_nlp_get(sglq->ndlp))
				sglq->ndlp = NULL;

			list_add(&sglq->list,
				 &phba->sli4_hba.lpfc_abts_els_sgl_list);
			spin_unlock_irqrestore(
				&phba->sli4_hba.sgl_list_lock, iflag);
		} else {
			spin_lock_irqsave(&phba->sli4_hba.sgl_list_lock,
					  iflag);
			sglq->state = SGL_FREED;
			sglq->ndlp = NULL;
			list_add_tail(&sglq->list,
				      &phba->sli4_hba.lpfc_els_sgl_list);
			spin_unlock_irqrestore(
				&phba->sli4_hba.sgl_list_lock, iflag);
			pring = lpfc_phba_elsring(phba);
			 
			if (pring && (!list_empty(&pring->txq)))
				lpfc_worker_wake_up(phba);
		}
	}

out:
	 
	memset_startat(iocbq, 0, wqe);
	iocbq->sli4_lxritag = NO_XRI;
	iocbq->sli4_xritag = NO_XRI;
	iocbq->cmd_flag &= ~(LPFC_IO_NVME | LPFC_IO_NVMET | LPFC_IO_CMF |
			      LPFC_IO_NVME_LS);
	list_add_tail(&iocbq->list, &phba->lpfc_iocb_list);
}


 
static void
__lpfc_sli_release_iocbq_s3(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{

	 
	memset_startat(iocbq, 0, iocb);
	iocbq->sli4_xritag = NO_XRI;
	list_add_tail(&iocbq->list, &phba->lpfc_iocb_list);
}

 
static void
__lpfc_sli_release_iocbq(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	lockdep_assert_held(&phba->hbalock);

	phba->__lpfc_sli_release_iocbq(phba, iocbq);
	phba->iocb_cnt--;
}

 
void
lpfc_sli_release_iocbq(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	unsigned long iflags;

	 
	spin_lock_irqsave(&phba->hbalock, iflags);
	__lpfc_sli_release_iocbq(phba, iocbq);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
}

 
void
lpfc_sli_cancel_iocbs(struct lpfc_hba *phba, struct list_head *iocblist,
		      uint32_t ulpstatus, uint32_t ulpWord4)
{
	struct lpfc_iocbq *piocb;

	while (!list_empty(iocblist)) {
		list_remove_head(iocblist, piocb, struct lpfc_iocbq, list);
		if (piocb->cmd_cmpl) {
			if (piocb->cmd_flag & LPFC_IO_NVME) {
				lpfc_nvme_cancel_iocb(phba, piocb,
						      ulpstatus, ulpWord4);
			} else {
				if (phba->sli_rev == LPFC_SLI_REV4) {
					bf_set(lpfc_wcqe_c_status,
					       &piocb->wcqe_cmpl, ulpstatus);
					piocb->wcqe_cmpl.parameter = ulpWord4;
				} else {
					piocb->iocb.ulpStatus = ulpstatus;
					piocb->iocb.un.ulpWord[4] = ulpWord4;
				}
				(piocb->cmd_cmpl) (phba, piocb, piocb);
			}
		} else {
			lpfc_sli_release_iocbq(phba, piocb);
		}
	}
	return;
}

 
static lpfc_iocb_type
lpfc_sli_iocb_cmd_type(uint8_t iocb_cmnd)
{
	lpfc_iocb_type type = LPFC_UNKNOWN_IOCB;

	if (iocb_cmnd > CMD_MAX_IOCB_CMD)
		return 0;

	switch (iocb_cmnd) {
	case CMD_XMIT_SEQUENCE_CR:
	case CMD_XMIT_SEQUENCE_CX:
	case CMD_XMIT_BCAST_CN:
	case CMD_XMIT_BCAST_CX:
	case CMD_ELS_REQUEST_CR:
	case CMD_ELS_REQUEST_CX:
	case CMD_CREATE_XRI_CR:
	case CMD_CREATE_XRI_CX:
	case CMD_GET_RPI_CN:
	case CMD_XMIT_ELS_RSP_CX:
	case CMD_GET_RPI_CR:
	case CMD_FCP_IWRITE_CR:
	case CMD_FCP_IWRITE_CX:
	case CMD_FCP_IREAD_CR:
	case CMD_FCP_IREAD_CX:
	case CMD_FCP_ICMND_CR:
	case CMD_FCP_ICMND_CX:
	case CMD_FCP_TSEND_CX:
	case CMD_FCP_TRSP_CX:
	case CMD_FCP_TRECEIVE_CX:
	case CMD_FCP_AUTO_TRSP_CX:
	case CMD_ADAPTER_MSG:
	case CMD_ADAPTER_DUMP:
	case CMD_XMIT_SEQUENCE64_CR:
	case CMD_XMIT_SEQUENCE64_CX:
	case CMD_XMIT_BCAST64_CN:
	case CMD_XMIT_BCAST64_CX:
	case CMD_ELS_REQUEST64_CR:
	case CMD_ELS_REQUEST64_CX:
	case CMD_FCP_IWRITE64_CR:
	case CMD_FCP_IWRITE64_CX:
	case CMD_FCP_IREAD64_CR:
	case CMD_FCP_IREAD64_CX:
	case CMD_FCP_ICMND64_CR:
	case CMD_FCP_ICMND64_CX:
	case CMD_FCP_TSEND64_CX:
	case CMD_FCP_TRSP64_CX:
	case CMD_FCP_TRECEIVE64_CX:
	case CMD_GEN_REQUEST64_CR:
	case CMD_GEN_REQUEST64_CX:
	case CMD_XMIT_ELS_RSP64_CX:
	case DSSCMD_IWRITE64_CR:
	case DSSCMD_IWRITE64_CX:
	case DSSCMD_IREAD64_CR:
	case DSSCMD_IREAD64_CX:
	case CMD_SEND_FRAME:
		type = LPFC_SOL_IOCB;
		break;
	case CMD_ABORT_XRI_CN:
	case CMD_ABORT_XRI_CX:
	case CMD_CLOSE_XRI_CN:
	case CMD_CLOSE_XRI_CX:
	case CMD_XRI_ABORTED_CX:
	case CMD_ABORT_MXRI64_CN:
	case CMD_XMIT_BLS_RSP64_CX:
		type = LPFC_ABORT_IOCB;
		break;
	case CMD_RCV_SEQUENCE_CX:
	case CMD_RCV_ELS_REQ_CX:
	case CMD_RCV_SEQUENCE64_CX:
	case CMD_RCV_ELS_REQ64_CX:
	case CMD_ASYNC_STATUS:
	case CMD_IOCB_RCV_SEQ64_CX:
	case CMD_IOCB_RCV_ELS64_CX:
	case CMD_IOCB_RCV_CONT64_CX:
	case CMD_IOCB_RET_XRI64_CX:
		type = LPFC_UNSOL_IOCB;
		break;
	case CMD_IOCB_XMIT_MSEQ64_CR:
	case CMD_IOCB_XMIT_MSEQ64_CX:
	case CMD_IOCB_RCV_SEQ_LIST64_CX:
	case CMD_IOCB_RCV_ELS_LIST64_CX:
	case CMD_IOCB_CLOSE_EXTENDED_CN:
	case CMD_IOCB_ABORT_EXTENDED_CN:
	case CMD_IOCB_RET_HBQE64_CN:
	case CMD_IOCB_FCP_IBIDIR64_CR:
	case CMD_IOCB_FCP_IBIDIR64_CX:
	case CMD_IOCB_FCP_ITASKMGT64_CX:
	case CMD_IOCB_LOGENTRY_CN:
	case CMD_IOCB_LOGENTRY_ASYNC_CN:
		printk("%s - Unhandled SLI-3 Command x%x\n",
				__func__, iocb_cmnd);
		type = LPFC_UNKNOWN_IOCB;
		break;
	default:
		type = LPFC_UNKNOWN_IOCB;
		break;
	}

	return type;
}

 
static int
lpfc_sli_ring_map(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *pmbox;
	int i, rc, ret = 0;

	pmb = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb)
		return -ENOMEM;
	pmbox = &pmb->u.mb;
	phba->link_state = LPFC_INIT_MBX_CMDS;
	for (i = 0; i < psli->num_rings; i++) {
		lpfc_config_ring(phba, i, pmb);
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0446 Adapter failed to init (%d), "
					"mbxCmd x%x CFG_RING, mbxStatus x%x, "
					"ring %d\n",
					rc, pmbox->mbxCommand,
					pmbox->mbxStatus, i);
			phba->link_state = LPFC_HBA_ERROR;
			ret = -ENXIO;
			break;
		}
	}
	mempool_free(pmb, phba->mbox_mem_pool);
	return ret;
}

 
static int
lpfc_sli_ringtxcmpl_put(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			struct lpfc_iocbq *piocb)
{
	u32 ulp_command = 0;

	BUG_ON(!piocb);
	ulp_command = get_job_cmnd(phba, piocb);

	list_add_tail(&piocb->list, &pring->txcmplq);
	piocb->cmd_flag |= LPFC_IO_ON_TXCMPLQ;
	pring->txcmplq_cnt++;
	if ((unlikely(pring->ringno == LPFC_ELS_RING)) &&
	   (ulp_command != CMD_ABORT_XRI_WQE) &&
	   (ulp_command != CMD_ABORT_XRI_CN) &&
	   (ulp_command != CMD_CLOSE_XRI_CN)) {
		BUG_ON(!piocb->vport);
		if (!(piocb->vport->load_flag & FC_UNLOADING))
			mod_timer(&piocb->vport->els_tmofunc,
				  jiffies +
				  msecs_to_jiffies(1000 * (phba->fc_ratov << 1)));
	}

	return 0;
}

 
struct lpfc_iocbq *
lpfc_sli_ringtx_get(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	struct lpfc_iocbq *cmd_iocb;

	lockdep_assert_held(&phba->hbalock);

	list_remove_head((&pring->txq), cmd_iocb, struct lpfc_iocbq, list);
	return cmd_iocb;
}

 
static void
lpfc_cmf_sync_cmpl(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		   struct lpfc_iocbq *rspiocb)
{
	union lpfc_wqe128 *wqe;
	uint32_t status, info;
	struct lpfc_wcqe_complete *wcqe = &rspiocb->wcqe_cmpl;
	uint64_t bw, bwdif, slop;
	uint64_t pcent, bwpcent;
	int asig, afpin, sigcnt, fpincnt;
	int wsigmax, wfpinmax, cg, tdp;
	char *s;

	 
	status = bf_get(lpfc_wcqe_c_status, wcqe);
	if (status) {
		lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
				"6211 CMF_SYNC_WQE Error "
				"req_tag x%x status x%x hwstatus x%x "
				"tdatap x%x parm x%x\n",
				bf_get(lpfc_wcqe_c_request_tag, wcqe),
				bf_get(lpfc_wcqe_c_status, wcqe),
				bf_get(lpfc_wcqe_c_hw_status, wcqe),
				wcqe->total_data_placed,
				wcqe->parameter);
		goto out;
	}

	 
	info = wcqe->parameter;
	phba->cmf_active_info = info;

	 
	if (info > LPFC_MAX_CMF_INFO || phba->cmf_info_per_interval == info)
		info = 0;
	else
		phba->cmf_info_per_interval = info;

	tdp = bf_get(lpfc_wcqe_c_cmf_bw, wcqe);
	cg = bf_get(lpfc_wcqe_c_cmf_cg, wcqe);

	 
	bw = (uint64_t)tdp * LPFC_CMF_BLK_SIZE;
	if (!bw) {
		lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
				"6212 CMF_SYNC_WQE x%x: NULL bw\n",
				bf_get(lpfc_wcqe_c_request_tag, wcqe));
		goto out;
	}

	 
	wqe = &cmdiocb->wqe;
	asig = bf_get(cmf_sync_asig, &wqe->cmf_sync);
	afpin = bf_get(cmf_sync_afpin, &wqe->cmf_sync);
	fpincnt = bf_get(cmf_sync_wfpincnt, &wqe->cmf_sync);
	sigcnt = bf_get(cmf_sync_wsigcnt, &wqe->cmf_sync);
	if (phba->cmf_max_bytes_per_interval != bw ||
	    (asig || afpin || sigcnt || fpincnt)) {
		 
		if (phba->cmf_max_bytes_per_interval <  bw) {
			bwdif = bw - phba->cmf_max_bytes_per_interval;
			s = "Increase";
		} else {
			bwdif = phba->cmf_max_bytes_per_interval - bw;
			s = "Decrease";
		}

		 
		slop = div_u64(phba->cmf_link_byte_count, 200);  
		pcent = div64_u64(bwdif * 100 + slop,
				  phba->cmf_link_byte_count);
		bwpcent = div64_u64(bw * 100 + slop,
				    phba->cmf_link_byte_count);
		 
		if (bwpcent > 100)
			bwpcent = 100;

		if (phba->cmf_max_bytes_per_interval < bw &&
		    bwpcent > 95)
			lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
					"6208 Congestion bandwidth "
					"limits removed\n");
		else if ((phba->cmf_max_bytes_per_interval > bw) &&
			 ((bwpcent + pcent) <= 100) && ((bwpcent + pcent) > 95))
			lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
					"6209 Congestion bandwidth "
					"limits in effect\n");

		if (asig) {
			lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
					"6237 BW Threshold %lld%% (%lld): "
					"%lld%% %s: Signal Alarm: cg:%d "
					"Info:%u\n",
					bwpcent, bw, pcent, s, cg,
					phba->cmf_active_info);
		} else if (afpin) {
			lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
					"6238 BW Threshold %lld%% (%lld): "
					"%lld%% %s: FPIN Alarm: cg:%d "
					"Info:%u\n",
					bwpcent, bw, pcent, s, cg,
					phba->cmf_active_info);
		} else if (sigcnt) {
			wsigmax = bf_get(cmf_sync_wsigmax, &wqe->cmf_sync);
			lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
					"6239 BW Threshold %lld%% (%lld): "
					"%lld%% %s: Signal Warning: "
					"Cnt %d Max %d: cg:%d Info:%u\n",
					bwpcent, bw, pcent, s, sigcnt,
					wsigmax, cg, phba->cmf_active_info);
		} else if (fpincnt) {
			wfpinmax = bf_get(cmf_sync_wfpinmax, &wqe->cmf_sync);
			lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
					"6240 BW Threshold %lld%% (%lld): "
					"%lld%% %s: FPIN Warning: "
					"Cnt %d Max %d: cg:%d Info:%u\n",
					bwpcent, bw, pcent, s, fpincnt,
					wfpinmax, cg, phba->cmf_active_info);
		} else {
			lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
					"6241 BW Threshold %lld%% (%lld): "
					"CMF %lld%% %s: cg:%d Info:%u\n",
					bwpcent, bw, pcent, s, cg,
					phba->cmf_active_info);
		}
	} else if (info) {
		lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
				"6246 Info Threshold %u\n", info);
	}

	 
	phba->cmf_last_sync_bw = bw;
out:
	lpfc_sli_release_iocbq(phba, cmdiocb);
}

 
int
lpfc_issue_cmf_sync_wqe(struct lpfc_hba *phba, u32 ms, u64 total)
{
	union lpfc_wqe128 *wqe;
	struct lpfc_iocbq *sync_buf;
	unsigned long iflags;
	u32 ret_val;
	u32 atot, wtot, max;
	u8 warn_sync_period = 0;

	 
	atot = atomic_xchg(&phba->cgn_sync_alarm_cnt, 0);
	wtot = atomic_xchg(&phba->cgn_sync_warn_cnt, 0);

	 
	if (phba->cmf_active_mode != LPFC_CFG_MANAGED ||
	    phba->link_state == LPFC_LINK_DOWN)
		return 0;

	spin_lock_irqsave(&phba->hbalock, iflags);
	sync_buf = __lpfc_sli_get_iocbq(phba);
	if (!sync_buf) {
		lpfc_printf_log(phba, KERN_ERR, LOG_CGN_MGMT,
				"6244 No available WQEs for CMF_SYNC_WQE\n");
		ret_val = ENOMEM;
		goto out_unlock;
	}

	wqe = &sync_buf->wqe;

	 
	memset(wqe, 0, sizeof(*wqe));

	 
	if (!ms) {
		lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
				"6441 CMF Init %d - CMF_SYNC_WQE\n",
				phba->fc_eventTag);
		bf_set(cmf_sync_op, &wqe->cmf_sync, 1);  
		bf_set(cmf_sync_interval, &wqe->cmf_sync, LPFC_CMF_INTERVAL);
		goto initpath;
	}

	bf_set(cmf_sync_op, &wqe->cmf_sync, 0);  
	bf_set(cmf_sync_interval, &wqe->cmf_sync, ms);

	 
	if (atot) {
		if (phba->cgn_reg_signal == EDC_CG_SIG_WARN_ALARM) {
			 
			bf_set(cmf_sync_asig, &wqe->cmf_sync, 1);
		} else {
			 
			bf_set(cmf_sync_afpin, &wqe->cmf_sync, 1);
		}
	} else if (wtot) {
		if (phba->cgn_reg_signal == EDC_CG_SIG_WARN_ONLY ||
		    phba->cgn_reg_signal == EDC_CG_SIG_WARN_ALARM) {
			 
			max = LPFC_SEC_TO_MSEC / lpfc_fabric_cgn_frequency *
				lpfc_acqe_cgn_frequency;
			bf_set(cmf_sync_wsigmax, &wqe->cmf_sync, max);
			bf_set(cmf_sync_wsigcnt, &wqe->cmf_sync, wtot);
			warn_sync_period = lpfc_acqe_cgn_frequency;
		} else {
			 
			bf_set(cmf_sync_wfpinmax, &wqe->cmf_sync, 1);
			bf_set(cmf_sync_wfpincnt, &wqe->cmf_sync, 1);
			if (phba->cgn_fpin_frequency != LPFC_FPIN_INIT_FREQ)
				warn_sync_period =
				LPFC_MSECS_TO_SECS(phba->cgn_fpin_frequency);
		}
	}

	 
	wqe->cmf_sync.read_bytes = (u32)(total / LPFC_CMF_BLK_SIZE);

initpath:
	bf_set(cmf_sync_ver, &wqe->cmf_sync, LPFC_CMF_SYNC_VER);
	wqe->cmf_sync.event_tag = phba->fc_eventTag;
	bf_set(cmf_sync_cmnd, &wqe->cmf_sync, CMD_CMF_SYNC_WQE);

	 
	bf_set(cmf_sync_reqtag, &wqe->cmf_sync, sync_buf->iotag);

	bf_set(cmf_sync_qosd, &wqe->cmf_sync, 1);
	bf_set(cmf_sync_period, &wqe->cmf_sync, warn_sync_period);

	bf_set(cmf_sync_cmd_type, &wqe->cmf_sync, CMF_SYNC_COMMAND);
	bf_set(cmf_sync_wqec, &wqe->cmf_sync, 1);
	bf_set(cmf_sync_cqid, &wqe->cmf_sync, LPFC_WQE_CQ_ID_DEFAULT);

	sync_buf->vport = phba->pport;
	sync_buf->cmd_cmpl = lpfc_cmf_sync_cmpl;
	sync_buf->cmd_dmabuf = NULL;
	sync_buf->rsp_dmabuf = NULL;
	sync_buf->bpl_dmabuf = NULL;
	sync_buf->sli4_xritag = NO_XRI;

	sync_buf->cmd_flag |= LPFC_IO_CMF;
	ret_val = lpfc_sli4_issue_wqe(phba, &phba->sli4_hba.hdwq[0], sync_buf);
	if (ret_val) {
		lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
				"6214 Cannot issue CMF_SYNC_WQE: x%x\n",
				ret_val);
		__lpfc_sli_release_iocbq(phba, sync_buf);
	}
out_unlock:
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return ret_val;
}

 
static IOCB_t *
lpfc_sli_next_iocb_slot (struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	struct lpfc_pgp *pgp = &phba->port_gp[pring->ringno];
	uint32_t  max_cmd_idx = pring->sli.sli3.numCiocb;

	lockdep_assert_held(&phba->hbalock);

	if ((pring->sli.sli3.next_cmdidx == pring->sli.sli3.cmdidx) &&
	   (++pring->sli.sli3.next_cmdidx >= max_cmd_idx))
		pring->sli.sli3.next_cmdidx = 0;

	if (unlikely(pring->sli.sli3.local_getidx ==
		pring->sli.sli3.next_cmdidx)) {

		pring->sli.sli3.local_getidx = le32_to_cpu(pgp->cmdGetInx);

		if (unlikely(pring->sli.sli3.local_getidx >= max_cmd_idx)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0315 Ring %d issue: portCmdGet %d "
					"is bigger than cmd ring %d\n",
					pring->ringno,
					pring->sli.sli3.local_getidx,
					max_cmd_idx);

			phba->link_state = LPFC_HBA_ERROR;
			 
			phba->work_ha |= HA_ERATT;
			phba->work_hs = HS_FFER3;

			lpfc_worker_wake_up(phba);

			return NULL;
		}

		if (pring->sli.sli3.local_getidx == pring->sli.sli3.next_cmdidx)
			return NULL;
	}

	return lpfc_cmd_iocb(phba, pring);
}

 
uint16_t
lpfc_sli_next_iotag(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	struct lpfc_iocbq **new_arr;
	struct lpfc_iocbq **old_arr;
	size_t new_len;
	struct lpfc_sli *psli = &phba->sli;
	uint16_t iotag;

	spin_lock_irq(&phba->hbalock);
	iotag = psli->last_iotag;
	if(++iotag < psli->iocbq_lookup_len) {
		psli->last_iotag = iotag;
		psli->iocbq_lookup[iotag] = iocbq;
		spin_unlock_irq(&phba->hbalock);
		iocbq->iotag = iotag;
		return iotag;
	} else if (psli->iocbq_lookup_len < (0xffff
					   - LPFC_IOCBQ_LOOKUP_INCREMENT)) {
		new_len = psli->iocbq_lookup_len + LPFC_IOCBQ_LOOKUP_INCREMENT;
		spin_unlock_irq(&phba->hbalock);
		new_arr = kcalloc(new_len, sizeof(struct lpfc_iocbq *),
				  GFP_KERNEL);
		if (new_arr) {
			spin_lock_irq(&phba->hbalock);
			old_arr = psli->iocbq_lookup;
			if (new_len <= psli->iocbq_lookup_len) {
				 
				kfree(new_arr);
				iotag = psli->last_iotag;
				if(++iotag < psli->iocbq_lookup_len) {
					psli->last_iotag = iotag;
					psli->iocbq_lookup[iotag] = iocbq;
					spin_unlock_irq(&phba->hbalock);
					iocbq->iotag = iotag;
					return iotag;
				}
				spin_unlock_irq(&phba->hbalock);
				return 0;
			}
			if (psli->iocbq_lookup)
				memcpy(new_arr, old_arr,
				       ((psli->last_iotag  + 1) *
					sizeof (struct lpfc_iocbq *)));
			psli->iocbq_lookup = new_arr;
			psli->iocbq_lookup_len = new_len;
			psli->last_iotag = iotag;
			psli->iocbq_lookup[iotag] = iocbq;
			spin_unlock_irq(&phba->hbalock);
			iocbq->iotag = iotag;
			kfree(old_arr);
			return iotag;
		}
	} else
		spin_unlock_irq(&phba->hbalock);

	lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
			"0318 Failed to allocate IOTAG.last IOTAG is %d\n",
			psli->last_iotag);

	return 0;
}

 
static void
lpfc_sli_submit_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
		IOCB_t *iocb, struct lpfc_iocbq *nextiocb)
{
	 
	nextiocb->iocb.ulpIoTag = (nextiocb->cmd_cmpl) ? nextiocb->iotag : 0;


	if (pring->ringno == LPFC_ELS_RING) {
		lpfc_debugfs_slow_ring_trc(phba,
			"IOCB cmd ring:   wd4:x%08x wd6:x%08x wd7:x%08x",
			*(((uint32_t *) &nextiocb->iocb) + 4),
			*(((uint32_t *) &nextiocb->iocb) + 6),
			*(((uint32_t *) &nextiocb->iocb) + 7));
	}

	 
	lpfc_sli_pcimem_bcopy(&nextiocb->iocb, iocb, phba->iocb_cmd_size);
	wmb();
	pring->stats.iocb_cmd++;

	 
	if (nextiocb->cmd_cmpl)
		lpfc_sli_ringtxcmpl_put(phba, pring, nextiocb);
	else
		__lpfc_sli_release_iocbq(phba, nextiocb);

	 
	pring->sli.sli3.cmdidx = pring->sli.sli3.next_cmdidx;
	writel(pring->sli.sli3.cmdidx, &phba->host_gp[pring->ringno].cmdPutInx);
}

 
static void
lpfc_sli_update_full_ring(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	int ringno = pring->ringno;

	pring->flag |= LPFC_CALL_RING_AVAILABLE;

	wmb();

	 
	writel((CA_R0ATT|CA_R0CE_REQ) << (ringno*4), phba->CAregaddr);
	readl(phba->CAregaddr);  

	pring->stats.iocb_cmd_full++;
}

 
static void
lpfc_sli_update_ring(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	int ringno = pring->ringno;

	 
	if (!(phba->sli3_options & LPFC_SLI3_CRP_ENABLED)) {
		wmb();
		writel(CA_R0ATT << (ringno * 4), phba->CAregaddr);
		readl(phba->CAregaddr);  
	}
}

 
static void
lpfc_sli_resume_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	IOCB_t *iocb;
	struct lpfc_iocbq *nextiocb;

	lockdep_assert_held(&phba->hbalock);

	 

	if (lpfc_is_link_up(phba) &&
	    (!list_empty(&pring->txq)) &&
	    (pring->ringno != LPFC_FCP_RING ||
	     phba->sli.sli_flag & LPFC_PROCESS_LA)) {

		while ((iocb = lpfc_sli_next_iocb_slot(phba, pring)) &&
		       (nextiocb = lpfc_sli_ringtx_get(phba, pring)))
			lpfc_sli_submit_iocb(phba, pring, iocb, nextiocb);

		if (iocb)
			lpfc_sli_update_ring(phba, pring);
		else
			lpfc_sli_update_full_ring(phba, pring);
	}

	return;
}

 
static struct lpfc_hbq_entry *
lpfc_sli_next_hbq_slot(struct lpfc_hba *phba, uint32_t hbqno)
{
	struct hbq_s *hbqp = &phba->hbqs[hbqno];

	lockdep_assert_held(&phba->hbalock);

	if (hbqp->next_hbqPutIdx == hbqp->hbqPutIdx &&
	    ++hbqp->next_hbqPutIdx >= hbqp->entry_count)
		hbqp->next_hbqPutIdx = 0;

	if (unlikely(hbqp->local_hbqGetIdx == hbqp->next_hbqPutIdx)) {
		uint32_t raw_index = phba->hbq_get[hbqno];
		uint32_t getidx = le32_to_cpu(raw_index);

		hbqp->local_hbqGetIdx = getidx;

		if (unlikely(hbqp->local_hbqGetIdx >= hbqp->entry_count)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"1802 HBQ %d: local_hbqGetIdx "
					"%u is > than hbqp->entry_count %u\n",
					hbqno, hbqp->local_hbqGetIdx,
					hbqp->entry_count);

			phba->link_state = LPFC_HBA_ERROR;
			return NULL;
		}

		if (hbqp->local_hbqGetIdx == hbqp->next_hbqPutIdx)
			return NULL;
	}

	return (struct lpfc_hbq_entry *) phba->hbqs[hbqno].hbq_virt +
			hbqp->hbqPutIdx;
}

 
void
lpfc_sli_hbqbuf_free_all(struct lpfc_hba *phba)
{
	struct lpfc_dmabuf *dmabuf, *next_dmabuf;
	struct hbq_dmabuf *hbq_buf;
	unsigned long flags;
	int i, hbq_count;

	hbq_count = lpfc_sli_hbq_count();
	 
	spin_lock_irqsave(&phba->hbalock, flags);
	for (i = 0; i < hbq_count; ++i) {
		list_for_each_entry_safe(dmabuf, next_dmabuf,
				&phba->hbqs[i].hbq_buffer_list, list) {
			hbq_buf = container_of(dmabuf, struct hbq_dmabuf, dbuf);
			list_del(&hbq_buf->dbuf.list);
			(phba->hbqs[i].hbq_free_buffer)(phba, hbq_buf);
		}
		phba->hbqs[i].buffer_count = 0;
	}

	 
	phba->hbq_in_use = 0;
	spin_unlock_irqrestore(&phba->hbalock, flags);
}

 
static int
lpfc_sli_hbq_to_firmware(struct lpfc_hba *phba, uint32_t hbqno,
			 struct hbq_dmabuf *hbq_buf)
{
	lockdep_assert_held(&phba->hbalock);
	return phba->lpfc_sli_hbq_to_firmware(phba, hbqno, hbq_buf);
}

 
static int
lpfc_sli_hbq_to_firmware_s3(struct lpfc_hba *phba, uint32_t hbqno,
			    struct hbq_dmabuf *hbq_buf)
{
	struct lpfc_hbq_entry *hbqe;
	dma_addr_t physaddr = hbq_buf->dbuf.phys;

	lockdep_assert_held(&phba->hbalock);
	 
	hbqe = lpfc_sli_next_hbq_slot(phba, hbqno);
	if (hbqe) {
		struct hbq_s *hbqp = &phba->hbqs[hbqno];

		hbqe->bde.addrHigh = le32_to_cpu(putPaddrHigh(physaddr));
		hbqe->bde.addrLow  = le32_to_cpu(putPaddrLow(physaddr));
		hbqe->bde.tus.f.bdeSize = hbq_buf->total_size;
		hbqe->bde.tus.f.bdeFlags = 0;
		hbqe->bde.tus.w = le32_to_cpu(hbqe->bde.tus.w);
		hbqe->buffer_tag = le32_to_cpu(hbq_buf->tag);
				 
		hbqp->hbqPutIdx = hbqp->next_hbqPutIdx;
		writel(hbqp->hbqPutIdx, phba->hbq_put + hbqno);
				 
		readl(phba->hbq_put + hbqno);
		list_add_tail(&hbq_buf->dbuf.list, &hbqp->hbq_buffer_list);
		return 0;
	} else
		return -ENOMEM;
}

 
static int
lpfc_sli_hbq_to_firmware_s4(struct lpfc_hba *phba, uint32_t hbqno,
			    struct hbq_dmabuf *hbq_buf)
{
	int rc;
	struct lpfc_rqe hrqe;
	struct lpfc_rqe drqe;
	struct lpfc_queue *hrq;
	struct lpfc_queue *drq;

	if (hbqno != LPFC_ELS_HBQ)
		return 1;
	hrq = phba->sli4_hba.hdr_rq;
	drq = phba->sli4_hba.dat_rq;

	lockdep_assert_held(&phba->hbalock);
	hrqe.address_lo = putPaddrLow(hbq_buf->hbuf.phys);
	hrqe.address_hi = putPaddrHigh(hbq_buf->hbuf.phys);
	drqe.address_lo = putPaddrLow(hbq_buf->dbuf.phys);
	drqe.address_hi = putPaddrHigh(hbq_buf->dbuf.phys);
	rc = lpfc_sli4_rq_put(hrq, drq, &hrqe, &drqe);
	if (rc < 0)
		return rc;
	hbq_buf->tag = (rc | (hbqno << 16));
	list_add_tail(&hbq_buf->dbuf.list, &phba->hbqs[hbqno].hbq_buffer_list);
	return 0;
}

 
static struct lpfc_hbq_init lpfc_els_hbq = {
	.rn = 1,
	.entry_count = 256,
	.mask_count = 0,
	.profile = 0,
	.ring_mask = (1 << LPFC_ELS_RING),
	.buffer_count = 0,
	.init_count = 40,
	.add_count = 40,
};

 
struct lpfc_hbq_init *lpfc_hbq_defs[] = {
	&lpfc_els_hbq,
};

 
static int
lpfc_sli_hbqbuf_fill_hbqs(struct lpfc_hba *phba, uint32_t hbqno, uint32_t count)
{
	uint32_t i, posted = 0;
	unsigned long flags;
	struct hbq_dmabuf *hbq_buffer;
	LIST_HEAD(hbq_buf_list);
	if (!phba->hbqs[hbqno].hbq_alloc_buffer)
		return 0;

	if ((phba->hbqs[hbqno].buffer_count + count) >
	    lpfc_hbq_defs[hbqno]->entry_count)
		count = lpfc_hbq_defs[hbqno]->entry_count -
					phba->hbqs[hbqno].buffer_count;
	if (!count)
		return 0;
	 
	for (i = 0; i < count; i++) {
		hbq_buffer = (phba->hbqs[hbqno].hbq_alloc_buffer)(phba);
		if (!hbq_buffer)
			break;
		list_add_tail(&hbq_buffer->dbuf.list, &hbq_buf_list);
	}
	 
	spin_lock_irqsave(&phba->hbalock, flags);
	if (!phba->hbq_in_use)
		goto err;
	while (!list_empty(&hbq_buf_list)) {
		list_remove_head(&hbq_buf_list, hbq_buffer, struct hbq_dmabuf,
				 dbuf.list);
		hbq_buffer->tag = (phba->hbqs[hbqno].buffer_count |
				      (hbqno << 16));
		if (!lpfc_sli_hbq_to_firmware(phba, hbqno, hbq_buffer)) {
			phba->hbqs[hbqno].buffer_count++;
			posted++;
		} else
			(phba->hbqs[hbqno].hbq_free_buffer)(phba, hbq_buffer);
	}
	spin_unlock_irqrestore(&phba->hbalock, flags);
	return posted;
err:
	spin_unlock_irqrestore(&phba->hbalock, flags);
	while (!list_empty(&hbq_buf_list)) {
		list_remove_head(&hbq_buf_list, hbq_buffer, struct hbq_dmabuf,
				 dbuf.list);
		(phba->hbqs[hbqno].hbq_free_buffer)(phba, hbq_buffer);
	}
	return 0;
}

 
int
lpfc_sli_hbqbuf_add_hbqs(struct lpfc_hba *phba, uint32_t qno)
{
	if (phba->sli_rev == LPFC_SLI_REV4)
		return 0;
	else
		return lpfc_sli_hbqbuf_fill_hbqs(phba, qno,
					 lpfc_hbq_defs[qno]->add_count);
}

 
static int
lpfc_sli_hbqbuf_init_hbqs(struct lpfc_hba *phba, uint32_t qno)
{
	if (phba->sli_rev == LPFC_SLI_REV4)
		return lpfc_sli_hbqbuf_fill_hbqs(phba, qno,
					lpfc_hbq_defs[qno]->entry_count);
	else
		return lpfc_sli_hbqbuf_fill_hbqs(phba, qno,
					 lpfc_hbq_defs[qno]->init_count);
}

 
static struct hbq_dmabuf *
lpfc_sli_hbqbuf_get(struct list_head *rb_list)
{
	struct lpfc_dmabuf *d_buf;

	list_remove_head(rb_list, d_buf, struct lpfc_dmabuf, list);
	if (!d_buf)
		return NULL;
	return container_of(d_buf, struct hbq_dmabuf, dbuf);
}

 
static struct rqb_dmabuf *
lpfc_sli_rqbuf_get(struct lpfc_hba *phba, struct lpfc_queue *hrq)
{
	struct lpfc_dmabuf *h_buf;
	struct lpfc_rqb *rqbp;

	rqbp = hrq->rqbp;
	list_remove_head(&rqbp->rqb_buffer_list, h_buf,
			 struct lpfc_dmabuf, list);
	if (!h_buf)
		return NULL;
	rqbp->buffer_count--;
	return container_of(h_buf, struct rqb_dmabuf, hbuf);
}

 
static struct hbq_dmabuf *
lpfc_sli_hbqbuf_find(struct lpfc_hba *phba, uint32_t tag)
{
	struct lpfc_dmabuf *d_buf;
	struct hbq_dmabuf *hbq_buf;
	uint32_t hbqno;

	hbqno = tag >> 16;
	if (hbqno >= LPFC_MAX_HBQS)
		return NULL;

	spin_lock_irq(&phba->hbalock);
	list_for_each_entry(d_buf, &phba->hbqs[hbqno].hbq_buffer_list, list) {
		hbq_buf = container_of(d_buf, struct hbq_dmabuf, dbuf);
		if (hbq_buf->tag == tag) {
			spin_unlock_irq(&phba->hbalock);
			return hbq_buf;
		}
	}
	spin_unlock_irq(&phba->hbalock);
	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"1803 Bad hbq tag. Data: x%x x%x\n",
			tag, phba->hbqs[tag >> 16].buffer_count);
	return NULL;
}

 
void
lpfc_sli_free_hbq(struct lpfc_hba *phba, struct hbq_dmabuf *hbq_buffer)
{
	uint32_t hbqno;

	if (hbq_buffer) {
		hbqno = hbq_buffer->tag >> 16;
		if (lpfc_sli_hbq_to_firmware(phba, hbqno, hbq_buffer))
			(phba->hbqs[hbqno].hbq_free_buffer)(phba, hbq_buffer);
	}
}

 
static int
lpfc_sli_chk_mbx_command(uint8_t mbxCommand)
{
	uint8_t ret;

	switch (mbxCommand) {
	case MBX_LOAD_SM:
	case MBX_READ_NV:
	case MBX_WRITE_NV:
	case MBX_WRITE_VPARMS:
	case MBX_RUN_BIU_DIAG:
	case MBX_INIT_LINK:
	case MBX_DOWN_LINK:
	case MBX_CONFIG_LINK:
	case MBX_CONFIG_RING:
	case MBX_RESET_RING:
	case MBX_READ_CONFIG:
	case MBX_READ_RCONFIG:
	case MBX_READ_SPARM:
	case MBX_READ_STATUS:
	case MBX_READ_RPI:
	case MBX_READ_XRI:
	case MBX_READ_REV:
	case MBX_READ_LNK_STAT:
	case MBX_REG_LOGIN:
	case MBX_UNREG_LOGIN:
	case MBX_CLEAR_LA:
	case MBX_DUMP_MEMORY:
	case MBX_DUMP_CONTEXT:
	case MBX_RUN_DIAGS:
	case MBX_RESTART:
	case MBX_UPDATE_CFG:
	case MBX_DOWN_LOAD:
	case MBX_DEL_LD_ENTRY:
	case MBX_RUN_PROGRAM:
	case MBX_SET_MASK:
	case MBX_SET_VARIABLE:
	case MBX_UNREG_D_ID:
	case MBX_KILL_BOARD:
	case MBX_CONFIG_FARP:
	case MBX_BEACON:
	case MBX_LOAD_AREA:
	case MBX_RUN_BIU_DIAG64:
	case MBX_CONFIG_PORT:
	case MBX_READ_SPARM64:
	case MBX_READ_RPI64:
	case MBX_REG_LOGIN64:
	case MBX_READ_TOPOLOGY:
	case MBX_WRITE_WWN:
	case MBX_SET_DEBUG:
	case MBX_LOAD_EXP_ROM:
	case MBX_ASYNCEVT_ENABLE:
	case MBX_REG_VPI:
	case MBX_UNREG_VPI:
	case MBX_HEARTBEAT:
	case MBX_PORT_CAPABILITIES:
	case MBX_PORT_IOV_CONTROL:
	case MBX_SLI4_CONFIG:
	case MBX_SLI4_REQ_FTRS:
	case MBX_REG_FCFI:
	case MBX_UNREG_FCFI:
	case MBX_REG_VFI:
	case MBX_UNREG_VFI:
	case MBX_INIT_VPI:
	case MBX_INIT_VFI:
	case MBX_RESUME_RPI:
	case MBX_READ_EVENT_LOG_STATUS:
	case MBX_READ_EVENT_LOG:
	case MBX_SECURITY_MGMT:
	case MBX_AUTH_PORT:
	case MBX_ACCESS_VDATA:
		ret = mbxCommand;
		break;
	default:
		ret = MBX_SHUTDOWN;
		break;
	}
	return ret;
}

 
void
lpfc_sli_wake_mbox_wait(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmboxq)
{
	unsigned long drvr_flag;
	struct completion *pmbox_done;

	 
	pmboxq->mbox_flag |= LPFC_MBX_WAKE;
	spin_lock_irqsave(&phba->hbalock, drvr_flag);
	pmbox_done = (struct completion *)pmboxq->context3;
	if (pmbox_done)
		complete(pmbox_done);
	spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
	return;
}

static void
__lpfc_sli_rpi_release(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	unsigned long iflags;

	if (ndlp->nlp_flag & NLP_RELEASE_RPI) {
		lpfc_sli4_free_rpi(vport->phba, ndlp->nlp_rpi);
		spin_lock_irqsave(&ndlp->lock, iflags);
		ndlp->nlp_flag &= ~NLP_RELEASE_RPI;
		ndlp->nlp_rpi = LPFC_RPI_ALLOC_ERROR;
		spin_unlock_irqrestore(&ndlp->lock, iflags);
	}
	ndlp->nlp_flag &= ~NLP_UNREG_INP;
}

void
lpfc_sli_rpi_release(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	__lpfc_sli_rpi_release(vport, ndlp);
}

 
void
lpfc_sli_def_mbox_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport  *vport = pmb->vport;
	struct lpfc_dmabuf *mp;
	struct lpfc_nodelist *ndlp;
	struct Scsi_Host *shost;
	uint16_t rpi, vpi;
	int rc;

	 
	if (!(phba->pport->load_flag & FC_UNLOADING) &&
	    pmb->u.mb.mbxCommand == MBX_REG_LOGIN64 &&
	    !pmb->u.mb.mbxStatus) {
		mp = (struct lpfc_dmabuf *)pmb->ctx_buf;
		if (mp) {
			pmb->ctx_buf = NULL;
			lpfc_mbuf_free(phba, mp->virt, mp->phys);
			kfree(mp);
		}
		rpi = pmb->u.mb.un.varWords[0];
		vpi = pmb->u.mb.un.varRegLogin.vpi;
		if (phba->sli_rev == LPFC_SLI_REV4)
			vpi -= phba->sli4_hba.max_cfg_param.vpi_base;
		lpfc_unreg_login(phba, vpi, rpi, pmb);
		pmb->vport = vport;
		pmb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);
		if (rc != MBX_NOT_FINISHED)
			return;
	}

	if ((pmb->u.mb.mbxCommand == MBX_REG_VPI) &&
		!(phba->pport->load_flag & FC_UNLOADING) &&
		!pmb->u.mb.mbxStatus) {
		shost = lpfc_shost_from_vport(vport);
		spin_lock_irq(shost->host_lock);
		vport->vpi_state |= LPFC_VPI_REGISTERED;
		vport->fc_flag &= ~FC_VPORT_NEEDS_REG_VPI;
		spin_unlock_irq(shost->host_lock);
	}

	if (pmb->u.mb.mbxCommand == MBX_REG_LOGIN64) {
		ndlp = (struct lpfc_nodelist *)pmb->ctx_ndlp;
		lpfc_nlp_put(ndlp);
	}

	if (pmb->u.mb.mbxCommand == MBX_UNREG_LOGIN) {
		ndlp = (struct lpfc_nodelist *)pmb->ctx_ndlp;

		 
		if (ndlp) {
			lpfc_printf_vlog(
				vport,
				KERN_INFO, LOG_MBOX | LOG_DISCOVERY,
				"1438 UNREG cmpl deferred mbox x%x "
				"on NPort x%x Data: x%x x%x x%px x%x x%x\n",
				ndlp->nlp_rpi, ndlp->nlp_DID,
				ndlp->nlp_flag, ndlp->nlp_defer_did,
				ndlp, vport->load_flag, kref_read(&ndlp->kref));

			if ((ndlp->nlp_flag & NLP_UNREG_INP) &&
			    (ndlp->nlp_defer_did != NLP_EVT_NOTHING_PENDING)) {
				ndlp->nlp_flag &= ~NLP_UNREG_INP;
				ndlp->nlp_defer_did = NLP_EVT_NOTHING_PENDING;
				lpfc_issue_els_plogi(vport, ndlp->nlp_DID, 0);
			} else {
				__lpfc_sli_rpi_release(vport, ndlp);
			}

			 
			lpfc_nlp_put(ndlp);
			pmb->ctx_ndlp = NULL;
		}
	}

	 
	if (pmb->u.mb.mbxCommand == MBX_RESUME_RPI) {
		ndlp = (struct lpfc_nodelist *)pmb->ctx_ndlp;
		lpfc_nlp_put(ndlp);
	}

	 
	if ((pmb->u.mb.mbxCommand == MBX_INIT_LINK) &&
	    (pmb->u.mb.mbxStatus == MBXERR_SEC_NO_PERMISSION))
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2860 SLI authentication is required "
				"for INIT_LINK but has not done yet\n");

	if (bf_get(lpfc_mqe_command, &pmb->u.mqe) == MBX_SLI4_CONFIG)
		lpfc_sli4_mbox_cmd_free(phba, pmb);
	else
		lpfc_mbox_rsrc_cleanup(phba, pmb, MBOX_THD_UNLOCKED);
}
  
void
lpfc_sli4_unreg_rpi_cmpl_clr(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport  *vport = pmb->vport;
	struct lpfc_nodelist *ndlp;

	ndlp = pmb->ctx_ndlp;
	if (pmb->u.mb.mbxCommand == MBX_UNREG_LOGIN) {
		if (phba->sli_rev == LPFC_SLI_REV4 &&
		    (bf_get(lpfc_sli_intf_if_type,
		     &phba->sli4_hba.sli_intf) >=
		     LPFC_SLI_INTF_IF_TYPE_2)) {
			if (ndlp) {
				lpfc_printf_vlog(
					 vport, KERN_INFO, LOG_MBOX | LOG_SLI,
					 "0010 UNREG_LOGIN vpi:%x "
					 "rpi:%x DID:%x defer x%x flg x%x "
					 "x%px\n",
					 vport->vpi, ndlp->nlp_rpi,
					 ndlp->nlp_DID, ndlp->nlp_defer_did,
					 ndlp->nlp_flag,
					 ndlp);
				ndlp->nlp_flag &= ~NLP_LOGO_ACC;

				 
				if ((ndlp->nlp_flag & NLP_UNREG_INP) &&
				    (ndlp->nlp_defer_did !=
				    NLP_EVT_NOTHING_PENDING)) {
					lpfc_printf_vlog(
						vport, KERN_INFO, LOG_DISCOVERY,
						"4111 UNREG cmpl deferred "
						"clr x%x on "
						"NPort x%x Data: x%x x%px\n",
						ndlp->nlp_rpi, ndlp->nlp_DID,
						ndlp->nlp_defer_did, ndlp);
					ndlp->nlp_flag &= ~NLP_UNREG_INP;
					ndlp->nlp_defer_did =
						NLP_EVT_NOTHING_PENDING;
					lpfc_issue_els_plogi(
						vport, ndlp->nlp_DID, 0);
				} else {
					__lpfc_sli_rpi_release(vport, ndlp);
				}
				lpfc_nlp_put(ndlp);
			}
		}
	}

	mempool_free(pmb, phba->mbox_mem_pool);
}

 
int
lpfc_sli_handle_mb_event(struct lpfc_hba *phba)
{
	MAILBOX_t *pmbox;
	LPFC_MBOXQ_t *pmb;
	int rc;
	LIST_HEAD(cmplq);

	phba->sli.slistat.mbox_event++;

	 
	spin_lock_irq(&phba->hbalock);
	list_splice_init(&phba->sli.mboxq_cmpl, &cmplq);
	spin_unlock_irq(&phba->hbalock);

	 
	do {
		list_remove_head(&cmplq, pmb, LPFC_MBOXQ_t, list);
		if (pmb == NULL)
			break;

		pmbox = &pmb->u.mb;

		if (pmbox->mbxCommand != MBX_HEARTBEAT) {
			if (pmb->vport) {
				lpfc_debugfs_disc_trc(pmb->vport,
					LPFC_DISC_TRC_MBOX_VPORT,
					"MBOX cmpl vport: cmd:x%x mb:x%x x%x",
					(uint32_t)pmbox->mbxCommand,
					pmbox->un.varWords[0],
					pmbox->un.varWords[1]);
			}
			else {
				lpfc_debugfs_disc_trc(phba->pport,
					LPFC_DISC_TRC_MBOX,
					"MBOX cmpl:       cmd:x%x mb:x%x x%x",
					(uint32_t)pmbox->mbxCommand,
					pmbox->un.varWords[0],
					pmbox->un.varWords[1]);
			}
		}

		 
		if (lpfc_sli_chk_mbx_command(pmbox->mbxCommand) ==
		    MBX_SHUTDOWN) {
			 
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"(%d):0323 Unknown Mailbox command "
					"x%x (x%x/x%x) Cmpl\n",
					pmb->vport ? pmb->vport->vpi :
					LPFC_VPORT_UNKNOWN,
					pmbox->mbxCommand,
					lpfc_sli_config_mbox_subsys_get(phba,
									pmb),
					lpfc_sli_config_mbox_opcode_get(phba,
									pmb));
			phba->link_state = LPFC_HBA_ERROR;
			phba->work_hs = HS_FFER3;
			lpfc_handle_eratt(phba);
			continue;
		}

		if (pmbox->mbxStatus) {
			phba->sli.slistat.mbox_stat_err++;
			if (pmbox->mbxStatus == MBXERR_NO_RESOURCES) {
				 
				lpfc_printf_log(phba, KERN_INFO,
					LOG_MBOX | LOG_SLI,
					"(%d):0305 Mbox cmd cmpl "
					"error - RETRYing Data: x%x "
					"(x%x/x%x) x%x x%x x%x\n",
					pmb->vport ? pmb->vport->vpi :
					LPFC_VPORT_UNKNOWN,
					pmbox->mbxCommand,
					lpfc_sli_config_mbox_subsys_get(phba,
									pmb),
					lpfc_sli_config_mbox_opcode_get(phba,
									pmb),
					pmbox->mbxStatus,
					pmbox->un.varWords[0],
					pmb->vport ? pmb->vport->port_state :
					LPFC_VPORT_UNKNOWN);
				pmbox->mbxStatus = 0;
				pmbox->mbxOwner = OWN_HOST;
				rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);
				if (rc != MBX_NOT_FINISHED)
					continue;
			}
		}

		 
		lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
				"(%d):0307 Mailbox cmd x%x (x%x/x%x) Cmpl %ps "
				"Data: x%x x%x x%x x%x x%x x%x x%x x%x x%x "
				"x%x x%x x%x\n",
				pmb->vport ? pmb->vport->vpi : 0,
				pmbox->mbxCommand,
				lpfc_sli_config_mbox_subsys_get(phba, pmb),
				lpfc_sli_config_mbox_opcode_get(phba, pmb),
				pmb->mbox_cmpl,
				*((uint32_t *) pmbox),
				pmbox->un.varWords[0],
				pmbox->un.varWords[1],
				pmbox->un.varWords[2],
				pmbox->un.varWords[3],
				pmbox->un.varWords[4],
				pmbox->un.varWords[5],
				pmbox->un.varWords[6],
				pmbox->un.varWords[7],
				pmbox->un.varWords[8],
				pmbox->un.varWords[9],
				pmbox->un.varWords[10]);

		if (pmb->mbox_cmpl)
			pmb->mbox_cmpl(phba,pmb);
	} while (1);
	return 0;
}

 
static struct lpfc_dmabuf *
lpfc_sli_get_buff(struct lpfc_hba *phba,
		  struct lpfc_sli_ring *pring,
		  uint32_t tag)
{
	struct hbq_dmabuf *hbq_entry;

	if (tag & QUE_BUFTAG_BIT)
		return lpfc_sli_ring_taggedbuf_get(phba, pring, tag);
	hbq_entry = lpfc_sli_hbqbuf_find(phba, tag);
	if (!hbq_entry)
		return NULL;
	return &hbq_entry->dbuf;
}

 
static void
lpfc_nvme_unsol_ls_handler(struct lpfc_hba *phba, struct lpfc_iocbq *piocb)
{
	struct lpfc_nodelist *ndlp;
	struct lpfc_dmabuf *d_buf;
	struct hbq_dmabuf *nvmebuf;
	struct fc_frame_header *fc_hdr;
	struct lpfc_async_xchg_ctx *axchg = NULL;
	char *failwhy = NULL;
	uint32_t oxid, sid, did, fctl, size;
	int ret = 1;

	d_buf = piocb->cmd_dmabuf;

	nvmebuf = container_of(d_buf, struct hbq_dmabuf, dbuf);
	fc_hdr = nvmebuf->hbuf.virt;
	oxid = be16_to_cpu(fc_hdr->fh_ox_id);
	sid = sli4_sid_from_fc_hdr(fc_hdr);
	did = sli4_did_from_fc_hdr(fc_hdr);
	fctl = (fc_hdr->fh_f_ctl[0] << 16 |
		fc_hdr->fh_f_ctl[1] << 8 |
		fc_hdr->fh_f_ctl[2]);
	size = bf_get(lpfc_rcqe_length, &nvmebuf->cq_event.cqe.rcqe_cmpl);

	lpfc_nvmeio_data(phba, "NVME LS    RCV: xri x%x sz %d from %06x\n",
			 oxid, size, sid);

	if (phba->pport->load_flag & FC_UNLOADING) {
		failwhy = "Driver Unloading";
	} else if (!(phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME)) {
		failwhy = "NVME FC4 Disabled";
	} else if (!phba->nvmet_support && !phba->pport->localport) {
		failwhy = "No Localport";
	} else if (phba->nvmet_support && !phba->targetport) {
		failwhy = "No Targetport";
	} else if (unlikely(fc_hdr->fh_r_ctl != FC_RCTL_ELS4_REQ)) {
		failwhy = "Bad NVME LS R_CTL";
	} else if (unlikely((fctl & 0x00FF0000) !=
			(FC_FC_FIRST_SEQ | FC_FC_END_SEQ | FC_FC_SEQ_INIT))) {
		failwhy = "Bad NVME LS F_CTL";
	} else {
		axchg = kzalloc(sizeof(*axchg), GFP_ATOMIC);
		if (!axchg)
			failwhy = "No CTX memory";
	}

	if (unlikely(failwhy)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"6154 Drop NVME LS: SID %06X OXID x%X: %s\n",
				sid, oxid, failwhy);
		goto out_fail;
	}

	 
	ndlp = lpfc_findnode_did(phba->pport, sid);
	if (!ndlp ||
	    ((ndlp->nlp_state != NLP_STE_UNMAPPED_NODE) &&
	     (ndlp->nlp_state != NLP_STE_MAPPED_NODE))) {
		lpfc_printf_log(phba, KERN_ERR, LOG_NVME_DISC,
				"6216 NVME Unsol rcv: No ndlp: "
				"NPort_ID x%x oxid x%x\n",
				sid, oxid);
		goto out_fail;
	}

	axchg->phba = phba;
	axchg->ndlp = ndlp;
	axchg->size = size;
	axchg->oxid = oxid;
	axchg->sid = sid;
	axchg->wqeq = NULL;
	axchg->state = LPFC_NVME_STE_LS_RCV;
	axchg->entry_cnt = 1;
	axchg->rqb_buffer = (void *)nvmebuf;
	axchg->hdwq = &phba->sli4_hba.hdwq[0];
	axchg->payload = nvmebuf->dbuf.virt;
	INIT_LIST_HEAD(&axchg->list);

	if (phba->nvmet_support) {
		ret = lpfc_nvmet_handle_lsreq(phba, axchg);
		spin_lock_irq(&ndlp->lock);
		if (!ret && !(ndlp->fc4_xpt_flags & NLP_XPT_HAS_HH)) {
			ndlp->fc4_xpt_flags |= NLP_XPT_HAS_HH;
			spin_unlock_irq(&ndlp->lock);

			 
			if (!lpfc_nlp_get(ndlp))
				goto out_fail;

			lpfc_printf_log(phba, KERN_ERR, LOG_NODE,
					"6206 NVMET unsol ls_req ndlp x%px "
					"DID x%x xflags x%x refcnt %d\n",
					ndlp, ndlp->nlp_DID,
					ndlp->fc4_xpt_flags,
					kref_read(&ndlp->kref));
		} else {
			spin_unlock_irq(&ndlp->lock);
		}
	} else {
		ret = lpfc_nvme_handle_lsreq(phba, axchg);
	}

	 
	if (!ret)
		return;

out_fail:
	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"6155 Drop NVME LS from DID %06X: SID %06X OXID x%X "
			"NVMe%s handler failed %d\n",
			did, sid, oxid,
			(phba->nvmet_support) ? "T" : "I", ret);

	 
	lpfc_in_buf_free(phba, &nvmebuf->dbuf);

	 
	if (axchg && (fctl & FC_FC_FIRST_SEQ && !(fctl & FC_FC_EX_CTX)))
		ret = lpfc_nvme_unsol_ls_issue_abort(phba, axchg, sid, oxid);

	if (ret)
		kfree(axchg);
}

 
static int
lpfc_complete_unsol_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			 struct lpfc_iocbq *saveq, uint32_t fch_r_ctl,
			 uint32_t fch_type)
{
	int i;

	switch (fch_type) {
	case FC_TYPE_NVME:
		lpfc_nvme_unsol_ls_handler(phba, saveq);
		return 1;
	default:
		break;
	}

	 
	if (pring->prt[0].profile) {
		if (pring->prt[0].lpfc_sli_rcv_unsol_event)
			(pring->prt[0].lpfc_sli_rcv_unsol_event) (phba, pring,
									saveq);
		return 1;
	}
	 
	for (i = 0; i < pring->num_mask; i++) {
		if ((pring->prt[i].rctl == fch_r_ctl) &&
		    (pring->prt[i].type == fch_type)) {
			if (pring->prt[i].lpfc_sli_rcv_unsol_event)
				(pring->prt[i].lpfc_sli_rcv_unsol_event)
						(phba, pring, saveq);
			return 1;
		}
	}
	return 0;
}

static void
lpfc_sli_prep_unsol_wqe(struct lpfc_hba *phba,
			struct lpfc_iocbq *saveq)
{
	IOCB_t *irsp;
	union lpfc_wqe128 *wqe;
	u16 i = 0;

	irsp = &saveq->iocb;
	wqe = &saveq->wqe;

	 
	bf_set(lpfc_wcqe_c_status, &saveq->wcqe_cmpl, irsp->ulpStatus);
	saveq->wcqe_cmpl.word3 = irsp->ulpBdeCount;
	saveq->wcqe_cmpl.parameter = irsp->un.ulpWord[4];
	saveq->wcqe_cmpl.total_data_placed = irsp->unsli3.rcvsli3.acc_len;

	 
	bf_set(els_rsp64_sid, &wqe->xmit_els_rsp, irsp->un.rcvels.parmRo);

	 
	bf_set(wqe_ctxt_tag, &wqe->xmit_els_rsp.wqe_com, irsp->ulpContext);

	 
	bf_set(wqe_rcvoxid, &wqe->xmit_els_rsp.wqe_com,
	       irsp->unsli3.rcvsli3.ox_id);

	 
	bf_set(wqe_els_did, &wqe->xmit_els_rsp.wqe_dest,
	       irsp->un.rcvels.remoteID);

	 
	for (i = 0; i < irsp->ulpBdeCount; i++) {
		struct lpfc_hbq_entry *hbqe = NULL;

		if (phba->sli3_options & LPFC_SLI3_HBQ_ENABLED) {
			if (i == 0) {
				hbqe = (struct lpfc_hbq_entry *)
					&irsp->un.ulpWord[0];
				saveq->wqe.gen_req.bde.tus.f.bdeSize =
					hbqe->bde.tus.f.bdeSize;
			} else if (i == 1) {
				hbqe = (struct lpfc_hbq_entry *)
					&irsp->unsli3.sli3Words[4];
				saveq->unsol_rcv_len = hbqe->bde.tus.f.bdeSize;
			}
		}
	}
}

 
static int
lpfc_sli_process_unsol_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			    struct lpfc_iocbq *saveq)
{
	IOCB_t           * irsp;
	WORD5            * w5p;
	dma_addr_t	 paddr;
	uint32_t           Rctl, Type;
	struct lpfc_iocbq *iocbq;
	struct lpfc_dmabuf *dmzbuf;

	irsp = &saveq->iocb;
	saveq->vport = phba->pport;

	if (irsp->ulpCommand == CMD_ASYNC_STATUS) {
		if (pring->lpfc_sli_rcv_async_status)
			pring->lpfc_sli_rcv_async_status(phba, pring, saveq);
		else
			lpfc_printf_log(phba,
					KERN_WARNING,
					LOG_SLI,
					"0316 Ring %d handler: unexpected "
					"ASYNC_STATUS iocb received evt_code "
					"0x%x\n",
					pring->ringno,
					irsp->un.asyncstat.evt_code);
		return 1;
	}

	if ((irsp->ulpCommand == CMD_IOCB_RET_XRI64_CX) &&
	    (phba->sli3_options & LPFC_SLI3_HBQ_ENABLED)) {
		if (irsp->ulpBdeCount > 0) {
			dmzbuf = lpfc_sli_get_buff(phba, pring,
						   irsp->un.ulpWord[3]);
			lpfc_in_buf_free(phba, dmzbuf);
		}

		if (irsp->ulpBdeCount > 1) {
			dmzbuf = lpfc_sli_get_buff(phba, pring,
						   irsp->unsli3.sli3Words[3]);
			lpfc_in_buf_free(phba, dmzbuf);
		}

		if (irsp->ulpBdeCount > 2) {
			dmzbuf = lpfc_sli_get_buff(phba, pring,
						   irsp->unsli3.sli3Words[7]);
			lpfc_in_buf_free(phba, dmzbuf);
		}

		return 1;
	}

	if (phba->sli3_options & LPFC_SLI3_HBQ_ENABLED) {
		if (irsp->ulpBdeCount != 0) {
			saveq->cmd_dmabuf = lpfc_sli_get_buff(phba, pring,
						irsp->un.ulpWord[3]);
			if (!saveq->cmd_dmabuf)
				lpfc_printf_log(phba,
					KERN_ERR,
					LOG_SLI,
					"0341 Ring %d Cannot find buffer for "
					"an unsolicited iocb. tag 0x%x\n",
					pring->ringno,
					irsp->un.ulpWord[3]);
		}
		if (irsp->ulpBdeCount == 2) {
			saveq->bpl_dmabuf = lpfc_sli_get_buff(phba, pring,
						irsp->unsli3.sli3Words[7]);
			if (!saveq->bpl_dmabuf)
				lpfc_printf_log(phba,
					KERN_ERR,
					LOG_SLI,
					"0342 Ring %d Cannot find buffer for an"
					" unsolicited iocb. tag 0x%x\n",
					pring->ringno,
					irsp->unsli3.sli3Words[7]);
		}
		list_for_each_entry(iocbq, &saveq->list, list) {
			irsp = &iocbq->iocb;
			if (irsp->ulpBdeCount != 0) {
				iocbq->cmd_dmabuf = lpfc_sli_get_buff(phba,
							pring,
							irsp->un.ulpWord[3]);
				if (!iocbq->cmd_dmabuf)
					lpfc_printf_log(phba,
						KERN_ERR,
						LOG_SLI,
						"0343 Ring %d Cannot find "
						"buffer for an unsolicited iocb"
						". tag 0x%x\n", pring->ringno,
						irsp->un.ulpWord[3]);
			}
			if (irsp->ulpBdeCount == 2) {
				iocbq->bpl_dmabuf = lpfc_sli_get_buff(phba,
						pring,
						irsp->unsli3.sli3Words[7]);
				if (!iocbq->bpl_dmabuf)
					lpfc_printf_log(phba,
						KERN_ERR,
						LOG_SLI,
						"0344 Ring %d Cannot find "
						"buffer for an unsolicited "
						"iocb. tag 0x%x\n",
						pring->ringno,
						irsp->unsli3.sli3Words[7]);
			}
		}
	} else {
		paddr = getPaddr(irsp->un.cont64[0].addrHigh,
				 irsp->un.cont64[0].addrLow);
		saveq->cmd_dmabuf = lpfc_sli_ringpostbuf_get(phba, pring,
							     paddr);
		if (irsp->ulpBdeCount == 2) {
			paddr = getPaddr(irsp->un.cont64[1].addrHigh,
					 irsp->un.cont64[1].addrLow);
			saveq->bpl_dmabuf = lpfc_sli_ringpostbuf_get(phba,
								   pring,
								   paddr);
		}
	}

	if (irsp->ulpBdeCount != 0 &&
	    (irsp->ulpCommand == CMD_IOCB_RCV_CONT64_CX ||
	     irsp->ulpStatus == IOSTAT_INTERMED_RSP)) {
		int found = 0;

		 
		list_for_each_entry(iocbq, &pring->iocb_continue_saveq, clist) {
			if (iocbq->iocb.unsli3.rcvsli3.ox_id ==
				saveq->iocb.unsli3.rcvsli3.ox_id) {
				list_add_tail(&saveq->list, &iocbq->list);
				found = 1;
				break;
			}
		}
		if (!found)
			list_add_tail(&saveq->clist,
				      &pring->iocb_continue_saveq);

		if (saveq->iocb.ulpStatus != IOSTAT_INTERMED_RSP) {
			list_del_init(&iocbq->clist);
			saveq = iocbq;
			irsp = &saveq->iocb;
		} else {
			return 0;
		}
	}
	if ((irsp->ulpCommand == CMD_RCV_ELS_REQ64_CX) ||
	    (irsp->ulpCommand == CMD_RCV_ELS_REQ_CX) ||
	    (irsp->ulpCommand == CMD_IOCB_RCV_ELS64_CX)) {
		Rctl = FC_RCTL_ELS_REQ;
		Type = FC_TYPE_ELS;
	} else {
		w5p = (WORD5 *)&(saveq->iocb.un.ulpWord[5]);
		Rctl = w5p->hcsw.Rctl;
		Type = w5p->hcsw.Type;

		 
		if ((Rctl == 0) && (pring->ringno == LPFC_ELS_RING) &&
			(irsp->ulpCommand == CMD_RCV_SEQUENCE64_CX ||
			 irsp->ulpCommand == CMD_IOCB_RCV_SEQ64_CX)) {
			Rctl = FC_RCTL_ELS_REQ;
			Type = FC_TYPE_ELS;
			w5p->hcsw.Rctl = Rctl;
			w5p->hcsw.Type = Type;
		}
	}

	if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
	    (irsp->ulpCommand == CMD_IOCB_RCV_ELS64_CX ||
	    irsp->ulpCommand == CMD_IOCB_RCV_SEQ64_CX)) {
		if (irsp->unsli3.rcvsli3.vpi == 0xffff)
			saveq->vport = phba->pport;
		else
			saveq->vport = lpfc_find_vport_by_vpid(phba,
					       irsp->unsli3.rcvsli3.vpi);
	}

	 
	lpfc_sli_prep_unsol_wqe(phba, saveq);

	if (!lpfc_complete_unsol_iocb(phba, pring, saveq, Rctl, Type))
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"0313 Ring %d handler: unexpected Rctl x%x "
				"Type x%x received\n",
				pring->ringno, Rctl, Type);

	return 1;
}

 
static struct lpfc_iocbq *
lpfc_sli_iocbq_lookup(struct lpfc_hba *phba,
		      struct lpfc_sli_ring *pring,
		      struct lpfc_iocbq *prspiocb)
{
	struct lpfc_iocbq *cmd_iocb = NULL;
	u16 iotag;

	if (phba->sli_rev == LPFC_SLI_REV4)
		iotag = get_wqe_reqtag(prspiocb);
	else
		iotag = prspiocb->iocb.ulpIoTag;

	if (iotag != 0 && iotag <= phba->sli.last_iotag) {
		cmd_iocb = phba->sli.iocbq_lookup[iotag];
		if (cmd_iocb->cmd_flag & LPFC_IO_ON_TXCMPLQ) {
			 
			list_del_init(&cmd_iocb->list);
			cmd_iocb->cmd_flag &= ~LPFC_IO_ON_TXCMPLQ;
			pring->txcmplq_cnt--;
			return cmd_iocb;
		}
	}

	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"0317 iotag x%x is out of "
			"range: max iotag x%x\n",
			iotag, phba->sli.last_iotag);
	return NULL;
}

 
static struct lpfc_iocbq *
lpfc_sli_iocbq_lookup_by_tag(struct lpfc_hba *phba,
			     struct lpfc_sli_ring *pring, uint16_t iotag)
{
	struct lpfc_iocbq *cmd_iocb = NULL;

	if (iotag != 0 && iotag <= phba->sli.last_iotag) {
		cmd_iocb = phba->sli.iocbq_lookup[iotag];
		if (cmd_iocb->cmd_flag & LPFC_IO_ON_TXCMPLQ) {
			 
			list_del_init(&cmd_iocb->list);
			cmd_iocb->cmd_flag &= ~LPFC_IO_ON_TXCMPLQ;
			pring->txcmplq_cnt--;
			return cmd_iocb;
		}
	}

	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"0372 iotag x%x lookup error: max iotag (x%x) "
			"cmd_flag x%x\n",
			iotag, phba->sli.last_iotag,
			cmd_iocb ? cmd_iocb->cmd_flag : 0xffff);
	return NULL;
}

 
static int
lpfc_sli_process_sol_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			  struct lpfc_iocbq *saveq)
{
	struct lpfc_iocbq *cmdiocbp;
	unsigned long iflag;
	u32 ulp_command, ulp_status, ulp_word4, ulp_context, iotag;

	if (phba->sli_rev == LPFC_SLI_REV4)
		spin_lock_irqsave(&pring->ring_lock, iflag);
	else
		spin_lock_irqsave(&phba->hbalock, iflag);
	cmdiocbp = lpfc_sli_iocbq_lookup(phba, pring, saveq);
	if (phba->sli_rev == LPFC_SLI_REV4)
		spin_unlock_irqrestore(&pring->ring_lock, iflag);
	else
		spin_unlock_irqrestore(&phba->hbalock, iflag);

	ulp_command = get_job_cmnd(phba, saveq);
	ulp_status = get_job_ulpstatus(phba, saveq);
	ulp_word4 = get_job_word4(phba, saveq);
	ulp_context = get_job_ulpcontext(phba, saveq);
	if (phba->sli_rev == LPFC_SLI_REV4)
		iotag = get_wqe_reqtag(saveq);
	else
		iotag = saveq->iocb.ulpIoTag;

	if (cmdiocbp) {
		ulp_command = get_job_cmnd(phba, cmdiocbp);
		if (cmdiocbp->cmd_cmpl) {
			 
			if (ulp_status &&
			     (pring->ringno == LPFC_ELS_RING) &&
			     (ulp_command == CMD_ELS_REQUEST64_CR))
				lpfc_send_els_failure_event(phba,
					cmdiocbp, saveq);

			 
			if (pring->ringno == LPFC_ELS_RING) {
				if ((phba->sli_rev < LPFC_SLI_REV4) &&
				    (cmdiocbp->cmd_flag &
							LPFC_DRIVER_ABORTED)) {
					spin_lock_irqsave(&phba->hbalock,
							  iflag);
					cmdiocbp->cmd_flag &=
						~LPFC_DRIVER_ABORTED;
					spin_unlock_irqrestore(&phba->hbalock,
							       iflag);
					saveq->iocb.ulpStatus =
						IOSTAT_LOCAL_REJECT;
					saveq->iocb.un.ulpWord[4] =
						IOERR_SLI_ABORTED;

					 
					spin_lock_irqsave(&phba->hbalock,
							  iflag);
					saveq->cmd_flag |= LPFC_DELAY_MEM_FREE;
					spin_unlock_irqrestore(&phba->hbalock,
							       iflag);
				}
				if (phba->sli_rev == LPFC_SLI_REV4) {
					if (saveq->cmd_flag &
					    LPFC_EXCHANGE_BUSY) {
						 
						spin_lock_irqsave(
							&phba->hbalock, iflag);
						cmdiocbp->cmd_flag |=
							LPFC_EXCHANGE_BUSY;
						spin_unlock_irqrestore(
							&phba->hbalock, iflag);
					}
					if (cmdiocbp->cmd_flag &
					    LPFC_DRIVER_ABORTED) {
						 
						spin_lock_irqsave(
							&phba->hbalock, iflag);
						cmdiocbp->cmd_flag &=
							~LPFC_DRIVER_ABORTED;
						spin_unlock_irqrestore(
							&phba->hbalock, iflag);
						set_job_ulpstatus(cmdiocbp,
								  IOSTAT_LOCAL_REJECT);
						set_job_ulpword4(cmdiocbp,
								 IOERR_ABORT_REQUESTED);
						 
						set_job_ulpstatus(saveq,
								  IOSTAT_LOCAL_REJECT);
						set_job_ulpword4(saveq,
								 IOERR_SLI_ABORTED);
						spin_lock_irqsave(
							&phba->hbalock, iflag);
						saveq->cmd_flag |=
							LPFC_DELAY_MEM_FREE;
						spin_unlock_irqrestore(
							&phba->hbalock, iflag);
					}
				}
			}
			cmdiocbp->cmd_cmpl(phba, cmdiocbp, saveq);
		} else
			lpfc_sli_release_iocbq(phba, cmdiocbp);
	} else {
		 
		if (pring->ringno != LPFC_ELS_RING) {
			 
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					 "0322 Ring %d handler: "
					 "unexpected completion IoTag x%x "
					 "Data: x%x x%x x%x x%x\n",
					 pring->ringno, iotag, ulp_status,
					 ulp_word4, ulp_command, ulp_context);
		}
	}

	return 1;
}

 
static void
lpfc_sli_rsp_pointers_error(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	struct lpfc_pgp *pgp = &phba->port_gp[pring->ringno];
	 
	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"0312 Ring %d handler: portRspPut %d "
			"is bigger than rsp ring %d\n",
			pring->ringno, le32_to_cpu(pgp->rspPutInx),
			pring->sli.sli3.numRiocb);

	phba->link_state = LPFC_HBA_ERROR;

	 
	phba->work_ha |= HA_ERATT;
	phba->work_hs = HS_FFER3;

	lpfc_worker_wake_up(phba);

	return;
}

 
void lpfc_poll_eratt(struct timer_list *t)
{
	struct lpfc_hba *phba;
	uint32_t eratt = 0;
	uint64_t sli_intr, cnt;

	phba = from_timer(phba, t, eratt_poll);
	if (!(phba->hba_flag & HBA_SETUP))
		return;

	 
	sli_intr = phba->sli.slistat.sli_intr;

	if (phba->sli.slistat.sli_prev_intr > sli_intr)
		cnt = (((uint64_t)(-1) - phba->sli.slistat.sli_prev_intr) +
			sli_intr);
	else
		cnt = (sli_intr - phba->sli.slistat.sli_prev_intr);

	 
	do_div(cnt, phba->eratt_poll_interval);
	phba->sli.slistat.sli_ips = cnt;

	phba->sli.slistat.sli_prev_intr = sli_intr;

	 
	eratt = lpfc_sli_check_eratt(phba);

	if (eratt)
		 
		lpfc_worker_wake_up(phba);
	else
		 
		mod_timer(&phba->eratt_poll,
			  jiffies +
			  msecs_to_jiffies(1000 * phba->eratt_poll_interval));
	return;
}


 
int
lpfc_sli_handle_fast_ring_event(struct lpfc_hba *phba,
				struct lpfc_sli_ring *pring, uint32_t mask)
{
	struct lpfc_pgp *pgp = &phba->port_gp[pring->ringno];
	IOCB_t *irsp = NULL;
	IOCB_t *entry = NULL;
	struct lpfc_iocbq *cmdiocbq = NULL;
	struct lpfc_iocbq rspiocbq;
	uint32_t status;
	uint32_t portRspPut, portRspMax;
	int rc = 1;
	lpfc_iocb_type type;
	unsigned long iflag;
	uint32_t rsp_cmpl = 0;

	spin_lock_irqsave(&phba->hbalock, iflag);
	pring->stats.iocb_event++;

	 
	portRspMax = pring->sli.sli3.numRiocb;
	portRspPut = le32_to_cpu(pgp->rspPutInx);
	if (unlikely(portRspPut >= portRspMax)) {
		lpfc_sli_rsp_pointers_error(phba, pring);
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		return 1;
	}
	if (phba->fcp_ring_in_use) {
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		return 1;
	} else
		phba->fcp_ring_in_use = 1;

	rmb();
	while (pring->sli.sli3.rspidx != portRspPut) {
		 
		entry = lpfc_resp_iocb(phba, pring);
		phba->last_completion_time = jiffies;

		if (++pring->sli.sli3.rspidx >= portRspMax)
			pring->sli.sli3.rspidx = 0;

		lpfc_sli_pcimem_bcopy((uint32_t *) entry,
				      (uint32_t *) &rspiocbq.iocb,
				      phba->iocb_rsp_size);
		INIT_LIST_HEAD(&(rspiocbq.list));
		irsp = &rspiocbq.iocb;

		type = lpfc_sli_iocb_cmd_type(irsp->ulpCommand & CMD_IOCB_MASK);
		pring->stats.iocb_rsp++;
		rsp_cmpl++;

		if (unlikely(irsp->ulpStatus)) {
			 
			if ((irsp->ulpStatus == IOSTAT_LOCAL_REJECT) &&
			    ((irsp->un.ulpWord[4] & IOERR_PARAM_MASK) ==
			     IOERR_NO_RESOURCES)) {
				spin_unlock_irqrestore(&phba->hbalock, iflag);
				phba->lpfc_rampdown_queue_depth(phba);
				spin_lock_irqsave(&phba->hbalock, iflag);
			}

			 
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					"0336 Rsp Ring %d error: IOCB Data: "
					"x%x x%x x%x x%x x%x x%x x%x x%x\n",
					pring->ringno,
					irsp->un.ulpWord[0],
					irsp->un.ulpWord[1],
					irsp->un.ulpWord[2],
					irsp->un.ulpWord[3],
					irsp->un.ulpWord[4],
					irsp->un.ulpWord[5],
					*(uint32_t *)&irsp->un1,
					*((uint32_t *)&irsp->un1 + 1));
		}

		switch (type) {
		case LPFC_ABORT_IOCB:
		case LPFC_SOL_IOCB:
			 
			if (unlikely(irsp->ulpCommand == CMD_XRI_ABORTED_CX)) {
				lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
						"0333 IOCB cmd 0x%x"
						" processed. Skipping"
						" completion\n",
						irsp->ulpCommand);
				break;
			}

			cmdiocbq = lpfc_sli_iocbq_lookup(phba, pring,
							 &rspiocbq);
			if (unlikely(!cmdiocbq))
				break;
			if (cmdiocbq->cmd_flag & LPFC_DRIVER_ABORTED)
				cmdiocbq->cmd_flag &= ~LPFC_DRIVER_ABORTED;
			if (cmdiocbq->cmd_cmpl) {
				spin_unlock_irqrestore(&phba->hbalock, iflag);
				cmdiocbq->cmd_cmpl(phba, cmdiocbq, &rspiocbq);
				spin_lock_irqsave(&phba->hbalock, iflag);
			}
			break;
		case LPFC_UNSOL_IOCB:
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			lpfc_sli_process_unsol_iocb(phba, pring, &rspiocbq);
			spin_lock_irqsave(&phba->hbalock, iflag);
			break;
		default:
			if (irsp->ulpCommand == CMD_ADAPTER_MSG) {
				char adaptermsg[LPFC_MAX_ADPTMSG];
				memset(adaptermsg, 0, LPFC_MAX_ADPTMSG);
				memcpy(&adaptermsg[0], (uint8_t *) irsp,
				       MAX_MSG_DATA);
				dev_warn(&((phba->pcidev)->dev),
					 "lpfc%d: %s\n",
					 phba->brd_no, adaptermsg);
			} else {
				 
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"0334 Unknown IOCB command "
						"Data: x%x, x%x x%x x%x x%x\n",
						type, irsp->ulpCommand,
						irsp->ulpStatus,
						irsp->ulpIoTag,
						irsp->ulpContext);
			}
			break;
		}

		 
		writel(pring->sli.sli3.rspidx,
			&phba->host_gp[pring->ringno].rspGetInx);

		if (pring->sli.sli3.rspidx == portRspPut)
			portRspPut = le32_to_cpu(pgp->rspPutInx);
	}

	if ((rsp_cmpl > 0) && (mask & HA_R0RE_REQ)) {
		pring->stats.iocb_rsp_full++;
		status = ((CA_R0ATT | CA_R0RE_RSP) << (pring->ringno * 4));
		writel(status, phba->CAregaddr);
		readl(phba->CAregaddr);
	}
	if ((mask & HA_R0CE_RSP) && (pring->flag & LPFC_CALL_RING_AVAILABLE)) {
		pring->flag &= ~LPFC_CALL_RING_AVAILABLE;
		pring->stats.iocb_cmd_empty++;

		 
		pring->sli.sli3.local_getidx = le32_to_cpu(pgp->cmdGetInx);
		lpfc_sli_resume_iocb(phba, pring);

		if ((pring->lpfc_sli_cmd_available))
			(pring->lpfc_sli_cmd_available) (phba, pring);

	}

	phba->fcp_ring_in_use = 0;
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return rc;
}

 
static struct lpfc_iocbq *
lpfc_sli_sp_handle_rspiocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			struct lpfc_iocbq *rspiocbp)
{
	struct lpfc_iocbq *saveq;
	struct lpfc_iocbq *cmdiocb;
	struct lpfc_iocbq *next_iocb;
	IOCB_t *irsp;
	uint32_t free_saveq;
	u8 cmd_type;
	lpfc_iocb_type type;
	unsigned long iflag;
	u32 ulp_status = get_job_ulpstatus(phba, rspiocbp);
	u32 ulp_word4 = get_job_word4(phba, rspiocbp);
	u32 ulp_command = get_job_cmnd(phba, rspiocbp);
	int rc;

	spin_lock_irqsave(&phba->hbalock, iflag);
	 
	list_add_tail(&rspiocbp->list, &pring->iocb_continueq);
	pring->iocb_continueq_cnt++;

	 
	free_saveq = 1;
	saveq = list_get_first(&pring->iocb_continueq,
			       struct lpfc_iocbq, list);
	list_del_init(&pring->iocb_continueq);
	pring->iocb_continueq_cnt = 0;

	pring->stats.iocb_rsp++;

	 
	if (ulp_status == IOSTAT_LOCAL_REJECT &&
	    ((ulp_word4 & IOERR_PARAM_MASK) ==
	     IOERR_NO_RESOURCES)) {
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		phba->lpfc_rampdown_queue_depth(phba);
		spin_lock_irqsave(&phba->hbalock, iflag);
	}

	if (ulp_status) {
		 
		if (phba->sli_rev < LPFC_SLI_REV4) {
			irsp = &rspiocbp->iocb;
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					"0328 Rsp Ring %d error: ulp_status x%x "
					"IOCB Data: "
					"x%08x x%08x x%08x x%08x "
					"x%08x x%08x x%08x x%08x "
					"x%08x x%08x x%08x x%08x "
					"x%08x x%08x x%08x x%08x\n",
					pring->ringno, ulp_status,
					get_job_ulpword(rspiocbp, 0),
					get_job_ulpword(rspiocbp, 1),
					get_job_ulpword(rspiocbp, 2),
					get_job_ulpword(rspiocbp, 3),
					get_job_ulpword(rspiocbp, 4),
					get_job_ulpword(rspiocbp, 5),
					*(((uint32_t *)irsp) + 6),
					*(((uint32_t *)irsp) + 7),
					*(((uint32_t *)irsp) + 8),
					*(((uint32_t *)irsp) + 9),
					*(((uint32_t *)irsp) + 10),
					*(((uint32_t *)irsp) + 11),
					*(((uint32_t *)irsp) + 12),
					*(((uint32_t *)irsp) + 13),
					*(((uint32_t *)irsp) + 14),
					*(((uint32_t *)irsp) + 15));
		} else {
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					"0321 Rsp Ring %d error: "
					"IOCB Data: "
					"x%x x%x x%x x%x\n",
					pring->ringno,
					rspiocbp->wcqe_cmpl.word0,
					rspiocbp->wcqe_cmpl.total_data_placed,
					rspiocbp->wcqe_cmpl.parameter,
					rspiocbp->wcqe_cmpl.word3);
		}
	}


	 
	cmd_type = ulp_command & CMD_IOCB_MASK;
	type = lpfc_sli_iocb_cmd_type(cmd_type);
	switch (type) {
	case LPFC_SOL_IOCB:
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		rc = lpfc_sli_process_sol_iocb(phba, pring, saveq);
		spin_lock_irqsave(&phba->hbalock, iflag);
		break;
	case LPFC_UNSOL_IOCB:
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		rc = lpfc_sli_process_unsol_iocb(phba, pring, saveq);
		spin_lock_irqsave(&phba->hbalock, iflag);
		if (!rc)
			free_saveq = 0;
		break;
	case LPFC_ABORT_IOCB:
		cmdiocb = NULL;
		if (ulp_command != CMD_XRI_ABORTED_CX)
			cmdiocb = lpfc_sli_iocbq_lookup(phba, pring,
							saveq);
		if (cmdiocb) {
			 
			if (cmdiocb->cmd_cmpl) {
				spin_unlock_irqrestore(&phba->hbalock, iflag);
				cmdiocb->cmd_cmpl(phba, cmdiocb, saveq);
				spin_lock_irqsave(&phba->hbalock, iflag);
			} else {
				__lpfc_sli_release_iocbq(phba, cmdiocb);
			}
		}
		break;
	case LPFC_UNKNOWN_IOCB:
		if (ulp_command == CMD_ADAPTER_MSG) {
			char adaptermsg[LPFC_MAX_ADPTMSG];

			memset(adaptermsg, 0, LPFC_MAX_ADPTMSG);
			memcpy(&adaptermsg[0], (uint8_t *)&rspiocbp->wqe,
			       MAX_MSG_DATA);
			dev_warn(&((phba->pcidev)->dev),
				 "lpfc%d: %s\n",
				 phba->brd_no, adaptermsg);
		} else {
			 
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0335 Unknown IOCB "
					"command Data: x%x "
					"x%x x%x x%x\n",
					ulp_command,
					ulp_status,
					get_wqe_reqtag(rspiocbp),
					get_job_ulpcontext(phba, rspiocbp));
		}
		break;
	}

	if (free_saveq) {
		list_for_each_entry_safe(rspiocbp, next_iocb,
					 &saveq->list, list) {
			list_del_init(&rspiocbp->list);
			__lpfc_sli_release_iocbq(phba, rspiocbp);
		}
		__lpfc_sli_release_iocbq(phba, saveq);
	}
	rspiocbp = NULL;
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return rspiocbp;
}

 
void
lpfc_sli_handle_slow_ring_event(struct lpfc_hba *phba,
				struct lpfc_sli_ring *pring, uint32_t mask)
{
	phba->lpfc_sli_handle_slow_ring_event(phba, pring, mask);
}

 
static void
lpfc_sli_handle_slow_ring_event_s3(struct lpfc_hba *phba,
				   struct lpfc_sli_ring *pring, uint32_t mask)
{
	struct lpfc_pgp *pgp;
	IOCB_t *entry;
	IOCB_t *irsp = NULL;
	struct lpfc_iocbq *rspiocbp = NULL;
	uint32_t portRspPut, portRspMax;
	unsigned long iflag;
	uint32_t status;

	pgp = &phba->port_gp[pring->ringno];
	spin_lock_irqsave(&phba->hbalock, iflag);
	pring->stats.iocb_event++;

	 
	portRspMax = pring->sli.sli3.numRiocb;
	portRspPut = le32_to_cpu(pgp->rspPutInx);
	if (portRspPut >= portRspMax) {
		 
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0303 Ring %d handler: portRspPut %d "
				"is bigger than rsp ring %d\n",
				pring->ringno, portRspPut, portRspMax);

		phba->link_state = LPFC_HBA_ERROR;
		spin_unlock_irqrestore(&phba->hbalock, iflag);

		phba->work_hs = HS_FFER3;
		lpfc_handle_eratt(phba);

		return;
	}

	rmb();
	while (pring->sli.sli3.rspidx != portRspPut) {
		 
		entry = lpfc_resp_iocb(phba, pring);

		phba->last_completion_time = jiffies;
		rspiocbp = __lpfc_sli_get_iocbq(phba);
		if (rspiocbp == NULL) {
			printk(KERN_ERR "%s: out of buffers! Failing "
			       "completion.\n", __func__);
			break;
		}

		lpfc_sli_pcimem_bcopy(entry, &rspiocbp->iocb,
				      phba->iocb_rsp_size);
		irsp = &rspiocbp->iocb;

		if (++pring->sli.sli3.rspidx >= portRspMax)
			pring->sli.sli3.rspidx = 0;

		if (pring->ringno == LPFC_ELS_RING) {
			lpfc_debugfs_slow_ring_trc(phba,
			"IOCB rsp ring:   wd4:x%08x wd6:x%08x wd7:x%08x",
				*(((uint32_t *) irsp) + 4),
				*(((uint32_t *) irsp) + 6),
				*(((uint32_t *) irsp) + 7));
		}

		writel(pring->sli.sli3.rspidx,
			&phba->host_gp[pring->ringno].rspGetInx);

		spin_unlock_irqrestore(&phba->hbalock, iflag);
		 
		rspiocbp = lpfc_sli_sp_handle_rspiocb(phba, pring, rspiocbp);
		spin_lock_irqsave(&phba->hbalock, iflag);

		 
		if (pring->sli.sli3.rspidx == portRspPut) {
			portRspPut = le32_to_cpu(pgp->rspPutInx);
		}
	}  

	if ((rspiocbp != NULL) && (mask & HA_R0RE_REQ)) {
		 
		pring->stats.iocb_rsp_full++;
		 
		status = ((CA_R0ATT | CA_R0RE_RSP) << (pring->ringno * 4));
		writel(status, phba->CAregaddr);
		readl(phba->CAregaddr);  
	}
	if ((mask & HA_R0CE_RSP) && (pring->flag & LPFC_CALL_RING_AVAILABLE)) {
		pring->flag &= ~LPFC_CALL_RING_AVAILABLE;
		pring->stats.iocb_cmd_empty++;

		 
		pring->sli.sli3.local_getidx = le32_to_cpu(pgp->cmdGetInx);
		lpfc_sli_resume_iocb(phba, pring);

		if ((pring->lpfc_sli_cmd_available))
			(pring->lpfc_sli_cmd_available) (phba, pring);

	}

	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return;
}

 
static void
lpfc_sli_handle_slow_ring_event_s4(struct lpfc_hba *phba,
				   struct lpfc_sli_ring *pring, uint32_t mask)
{
	struct lpfc_iocbq *irspiocbq;
	struct hbq_dmabuf *dmabuf;
	struct lpfc_cq_event *cq_event;
	unsigned long iflag;
	int count = 0;

	spin_lock_irqsave(&phba->hbalock, iflag);
	phba->hba_flag &= ~HBA_SP_QUEUE_EVT;
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	while (!list_empty(&phba->sli4_hba.sp_queue_event)) {
		 
		spin_lock_irqsave(&phba->hbalock, iflag);
		list_remove_head(&phba->sli4_hba.sp_queue_event,
				 cq_event, struct lpfc_cq_event, list);
		spin_unlock_irqrestore(&phba->hbalock, iflag);

		switch (bf_get(lpfc_wcqe_c_code, &cq_event->cqe.wcqe_cmpl)) {
		case CQE_CODE_COMPL_WQE:
			irspiocbq = container_of(cq_event, struct lpfc_iocbq,
						 cq_event);
			 
			irspiocbq = lpfc_sli4_els_preprocess_rspiocbq(phba,
								      irspiocbq);
			if (irspiocbq)
				lpfc_sli_sp_handle_rspiocb(phba, pring,
							   irspiocbq);
			count++;
			break;
		case CQE_CODE_RECEIVE:
		case CQE_CODE_RECEIVE_V1:
			dmabuf = container_of(cq_event, struct hbq_dmabuf,
					      cq_event);
			lpfc_sli4_handle_received_buffer(phba, dmabuf);
			count++;
			break;
		default:
			break;
		}

		 
		if (count == 64)
			break;
	}
}

 
void
lpfc_sli_abort_iocb_ring(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	LIST_HEAD(tx_completions);
	LIST_HEAD(txcmplq_completions);
	struct lpfc_iocbq *iocb, *next_iocb;
	int offline;

	if (pring->ringno == LPFC_ELS_RING) {
		lpfc_fabric_abort_hba(phba);
	}
	offline = pci_channel_offline(phba->pcidev);

	 
	if (phba->sli_rev >= LPFC_SLI_REV4) {
		spin_lock_irq(&pring->ring_lock);
		list_splice_init(&pring->txq, &tx_completions);
		pring->txq_cnt = 0;

		if (offline) {
			list_splice_init(&pring->txcmplq,
					 &txcmplq_completions);
		} else {
			 
			list_for_each_entry_safe(iocb, next_iocb,
						 &pring->txcmplq, list)
				lpfc_sli_issue_abort_iotag(phba, pring,
							   iocb, NULL);
		}
		spin_unlock_irq(&pring->ring_lock);
	} else {
		spin_lock_irq(&phba->hbalock);
		list_splice_init(&pring->txq, &tx_completions);
		pring->txq_cnt = 0;

		if (offline) {
			list_splice_init(&pring->txcmplq, &txcmplq_completions);
		} else {
			 
			list_for_each_entry_safe(iocb, next_iocb,
						 &pring->txcmplq, list)
				lpfc_sli_issue_abort_iotag(phba, pring,
							   iocb, NULL);
		}
		spin_unlock_irq(&phba->hbalock);
	}

	if (offline) {
		 
		lpfc_sli_cancel_iocbs(phba, &txcmplq_completions,
				      IOSTAT_LOCAL_REJECT, IOERR_SLI_ABORTED);
	} else {
		 
		lpfc_issue_hb_tmo(phba);
	}
	 
	lpfc_sli_cancel_iocbs(phba, &tx_completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);
}

 
void
lpfc_sli_abort_fcp_rings(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring  *pring;
	uint32_t i;

	 
	if (phba->sli_rev >= LPFC_SLI_REV4) {
		for (i = 0; i < phba->cfg_hdw_queue; i++) {
			pring = phba->sli4_hba.hdwq[i].io_wq->pring;
			lpfc_sli_abort_iocb_ring(phba, pring);
		}
	} else {
		pring = &psli->sli3_ring[LPFC_FCP_RING];
		lpfc_sli_abort_iocb_ring(phba, pring);
	}
}

 
void
lpfc_sli_flush_io_rings(struct lpfc_hba *phba)
{
	LIST_HEAD(txq);
	LIST_HEAD(txcmplq);
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring  *pring;
	uint32_t i;
	struct lpfc_iocbq *piocb, *next_iocb;

	spin_lock_irq(&phba->hbalock);
	 
	phba->hba_flag |= HBA_IOQ_FLUSH;
	spin_unlock_irq(&phba->hbalock);

	 
	if (phba->sli_rev >= LPFC_SLI_REV4) {
		for (i = 0; i < phba->cfg_hdw_queue; i++) {
			pring = phba->sli4_hba.hdwq[i].io_wq->pring;

			spin_lock_irq(&pring->ring_lock);
			 
			list_splice_init(&pring->txq, &txq);
			list_for_each_entry_safe(piocb, next_iocb,
						 &pring->txcmplq, list)
				piocb->cmd_flag &= ~LPFC_IO_ON_TXCMPLQ;
			 
			list_splice_init(&pring->txcmplq, &txcmplq);
			pring->txq_cnt = 0;
			pring->txcmplq_cnt = 0;
			spin_unlock_irq(&pring->ring_lock);

			 
			lpfc_sli_cancel_iocbs(phba, &txq,
					      IOSTAT_LOCAL_REJECT,
					      IOERR_SLI_DOWN);
			 
			lpfc_sli_cancel_iocbs(phba, &txcmplq,
					      IOSTAT_LOCAL_REJECT,
					      IOERR_SLI_DOWN);
			if (unlikely(pci_channel_offline(phba->pcidev)))
				lpfc_sli4_io_xri_aborted(phba, NULL, 0);
		}
	} else {
		pring = &psli->sli3_ring[LPFC_FCP_RING];

		spin_lock_irq(&phba->hbalock);
		 
		list_splice_init(&pring->txq, &txq);
		list_for_each_entry_safe(piocb, next_iocb,
					 &pring->txcmplq, list)
			piocb->cmd_flag &= ~LPFC_IO_ON_TXCMPLQ;
		 
		list_splice_init(&pring->txcmplq, &txcmplq);
		pring->txq_cnt = 0;
		pring->txcmplq_cnt = 0;
		spin_unlock_irq(&phba->hbalock);

		 
		lpfc_sli_cancel_iocbs(phba, &txq, IOSTAT_LOCAL_REJECT,
				      IOERR_SLI_DOWN);
		 
		lpfc_sli_cancel_iocbs(phba, &txcmplq, IOSTAT_LOCAL_REJECT,
				      IOERR_SLI_DOWN);
	}
}

 
static int
lpfc_sli_brdready_s3(struct lpfc_hba *phba, uint32_t mask)
{
	uint32_t status;
	int i = 0;
	int retval = 0;

	 
	if (lpfc_readl(phba->HSregaddr, &status))
		return 1;

	phba->hba_flag |= HBA_NEEDS_CFG_PORT;

	 
	while (((status & mask) != mask) &&
	       !(status & HS_FFERM) &&
	       i++ < 20) {

		if (i <= 5)
			msleep(10);
		else if (i <= 10)
			msleep(500);
		else
			msleep(2500);

		if (i == 15) {
				 
			phba->pport->port_state = LPFC_VPORT_UNKNOWN;
			lpfc_sli_brdrestart(phba);
		}
		 
		if (lpfc_readl(phba->HSregaddr, &status)) {
			retval = 1;
			break;
		}
	}

	 
	if ((status & HS_FFERM) || (i >= 20)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2751 Adapter failed to restart, "
				"status reg x%x, FW Data: A8 x%x AC x%x\n",
				status,
				readl(phba->MBslimaddr + 0xa8),
				readl(phba->MBslimaddr + 0xac));
		phba->link_state = LPFC_HBA_ERROR;
		retval = 1;
	}

	return retval;
}

 
static int
lpfc_sli_brdready_s4(struct lpfc_hba *phba, uint32_t mask)
{
	uint32_t status;
	int retval = 0;

	 
	status = lpfc_sli4_post_status_check(phba);

	if (status) {
		phba->pport->port_state = LPFC_VPORT_UNKNOWN;
		lpfc_sli_brdrestart(phba);
		status = lpfc_sli4_post_status_check(phba);
	}

	 
	if (status) {
		phba->link_state = LPFC_HBA_ERROR;
		retval = 1;
	} else
		phba->sli4_hba.intr_enable = 0;

	phba->hba_flag &= ~HBA_SETUP;
	return retval;
}

 
int
lpfc_sli_brdready(struct lpfc_hba *phba, uint32_t mask)
{
	return phba->lpfc_sli_brdready(phba, mask);
}

#define BARRIER_TEST_PATTERN (0xdeadbeef)

 
void lpfc_reset_barrier(struct lpfc_hba *phba)
{
	uint32_t __iomem *resp_buf;
	uint32_t __iomem *mbox_buf;
	volatile struct MAILBOX_word0 mbox;
	uint32_t hc_copy, ha_copy, resp_data;
	int  i;
	uint8_t hdrtype;

	lockdep_assert_held(&phba->hbalock);

	pci_read_config_byte(phba->pcidev, PCI_HEADER_TYPE, &hdrtype);
	if (hdrtype != 0x80 ||
	    (FC_JEDEC_ID(phba->vpd.rev.biuRev) != HELIOS_JEDEC_ID &&
	     FC_JEDEC_ID(phba->vpd.rev.biuRev) != THOR_JEDEC_ID))
		return;

	 
	resp_buf = phba->MBslimaddr;

	 
	if (lpfc_readl(phba->HCregaddr, &hc_copy))
		return;
	writel((hc_copy & ~HC_ERINT_ENA), phba->HCregaddr);
	readl(phba->HCregaddr);  
	phba->link_flag |= LS_IGNORE_ERATT;

	if (lpfc_readl(phba->HAregaddr, &ha_copy))
		return;
	if (ha_copy & HA_ERATT) {
		 
		writel(HA_ERATT, phba->HAregaddr);
		phba->pport->stopped = 1;
	}

	mbox.word0 = 0;
	mbox.mbxCommand = MBX_KILL_BOARD;
	mbox.mbxOwner = OWN_CHIP;

	writel(BARRIER_TEST_PATTERN, (resp_buf + 1));
	mbox_buf = phba->MBslimaddr;
	writel(mbox.word0, mbox_buf);

	for (i = 0; i < 50; i++) {
		if (lpfc_readl((resp_buf + 1), &resp_data))
			return;
		if (resp_data != ~(BARRIER_TEST_PATTERN))
			mdelay(1);
		else
			break;
	}
	resp_data = 0;
	if (lpfc_readl((resp_buf + 1), &resp_data))
		return;
	if (resp_data  != ~(BARRIER_TEST_PATTERN)) {
		if (phba->sli.sli_flag & LPFC_SLI_ACTIVE ||
		    phba->pport->stopped)
			goto restore_hc;
		else
			goto clear_errat;
	}

	mbox.mbxOwner = OWN_HOST;
	resp_data = 0;
	for (i = 0; i < 500; i++) {
		if (lpfc_readl(resp_buf, &resp_data))
			return;
		if (resp_data != mbox.word0)
			mdelay(1);
		else
			break;
	}

clear_errat:

	while (++i < 500) {
		if (lpfc_readl(phba->HAregaddr, &ha_copy))
			return;
		if (!(ha_copy & HA_ERATT))
			mdelay(1);
		else
			break;
	}

	if (readl(phba->HAregaddr) & HA_ERATT) {
		writel(HA_ERATT, phba->HAregaddr);
		phba->pport->stopped = 1;
	}

restore_hc:
	phba->link_flag &= ~LS_IGNORE_ERATT;
	writel(hc_copy, phba->HCregaddr);
	readl(phba->HCregaddr);  
}

 
int
lpfc_sli_brdkill(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli;
	LPFC_MBOXQ_t *pmb;
	uint32_t status;
	uint32_t ha_copy;
	int retval;
	int i = 0;

	psli = &phba->sli;

	 
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"0329 Kill HBA Data: x%x x%x\n",
			phba->pport->port_state, psli->sli_flag);

	pmb = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb)
		return 1;

	 
	spin_lock_irq(&phba->hbalock);
	if (lpfc_readl(phba->HCregaddr, &status)) {
		spin_unlock_irq(&phba->hbalock);
		mempool_free(pmb, phba->mbox_mem_pool);
		return 1;
	}
	status &= ~HC_ERINT_ENA;
	writel(status, phba->HCregaddr);
	readl(phba->HCregaddr);  
	phba->link_flag |= LS_IGNORE_ERATT;
	spin_unlock_irq(&phba->hbalock);

	lpfc_kill_board(phba, pmb);
	pmb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	retval = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);

	if (retval != MBX_SUCCESS) {
		if (retval != MBX_BUSY)
			mempool_free(pmb, phba->mbox_mem_pool);
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2752 KILL_BOARD command failed retval %d\n",
				retval);
		spin_lock_irq(&phba->hbalock);
		phba->link_flag &= ~LS_IGNORE_ERATT;
		spin_unlock_irq(&phba->hbalock);
		return 1;
	}

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag &= ~LPFC_SLI_ACTIVE;
	spin_unlock_irq(&phba->hbalock);

	mempool_free(pmb, phba->mbox_mem_pool);

	 
	if (lpfc_readl(phba->HAregaddr, &ha_copy))
		return 1;
	while ((i++ < 30) && !(ha_copy & HA_ERATT)) {
		mdelay(100);
		if (lpfc_readl(phba->HAregaddr, &ha_copy))
			return 1;
	}

	del_timer_sync(&psli->mbox_tmo);
	if (ha_copy & HA_ERATT) {
		writel(HA_ERATT, phba->HAregaddr);
		phba->pport->stopped = 1;
	}
	spin_lock_irq(&phba->hbalock);
	psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
	psli->mbox_active = NULL;
	phba->link_flag &= ~LS_IGNORE_ERATT;
	spin_unlock_irq(&phba->hbalock);

	lpfc_hba_down_post(phba);
	phba->link_state = LPFC_HBA_ERROR;

	return ha_copy & HA_ERATT ? 0 : 1;
}

 
int
lpfc_sli_brdreset(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli;
	struct lpfc_sli_ring *pring;
	uint16_t cfg_value;
	int i;

	psli = &phba->sli;

	 
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"0325 Reset HBA Data: x%x x%x\n",
			(phba->pport) ? phba->pport->port_state : 0,
			psli->sli_flag);

	 
	phba->fc_eventTag = 0;
	phba->link_events = 0;
	phba->hba_flag |= HBA_NEEDS_CFG_PORT;
	if (phba->pport) {
		phba->pport->fc_myDID = 0;
		phba->pport->fc_prevDID = 0;
	}

	 
	if (pci_read_config_word(phba->pcidev, PCI_COMMAND, &cfg_value))
		return -EIO;

	pci_write_config_word(phba->pcidev, PCI_COMMAND,
			      (cfg_value &
			       ~(PCI_COMMAND_PARITY | PCI_COMMAND_SERR)));

	psli->sli_flag &= ~(LPFC_SLI_ACTIVE | LPFC_PROCESS_LA);

	 
	writel(HC_INITFF, phba->HCregaddr);
	mdelay(1);
	readl(phba->HCregaddr);  
	writel(0, phba->HCregaddr);
	readl(phba->HCregaddr);  

	 
	pci_write_config_word(phba->pcidev, PCI_COMMAND, cfg_value);

	 
	for (i = 0; i < psli->num_rings; i++) {
		pring = &psli->sli3_ring[i];
		pring->flag = 0;
		pring->sli.sli3.rspidx = 0;
		pring->sli.sli3.next_cmdidx  = 0;
		pring->sli.sli3.local_getidx = 0;
		pring->sli.sli3.cmdidx = 0;
		pring->missbufcnt = 0;
	}

	phba->link_state = LPFC_WARM_START;
	return 0;
}

 
int
lpfc_sli4_brdreset(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	uint16_t cfg_value;
	int rc = 0;

	 
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"0295 Reset HBA Data: x%x x%x x%x\n",
			phba->pport->port_state, psli->sli_flag,
			phba->hba_flag);

	 
	phba->fc_eventTag = 0;
	phba->link_events = 0;
	phba->pport->fc_myDID = 0;
	phba->pport->fc_prevDID = 0;
	phba->hba_flag &= ~HBA_SETUP;

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag &= ~(LPFC_PROCESS_LA);
	phba->fcf.fcf_flag = 0;
	spin_unlock_irq(&phba->hbalock);

	 
	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"0389 Performing PCI function reset!\n");

	 
	if (pci_read_config_word(phba->pcidev, PCI_COMMAND, &cfg_value)) {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"3205 PCI read Config failed\n");
		return -EIO;
	}

	pci_write_config_word(phba->pcidev, PCI_COMMAND, (cfg_value &
			      ~(PCI_COMMAND_PARITY | PCI_COMMAND_SERR)));

	 
	rc = lpfc_pci_function_reset(phba);

	 
	pci_write_config_word(phba->pcidev, PCI_COMMAND, cfg_value);

	return rc;
}

 
static int
lpfc_sli_brdrestart_s3(struct lpfc_hba *phba)
{
	volatile struct MAILBOX_word0 mb;
	struct lpfc_sli *psli;
	void __iomem *to_slim;

	spin_lock_irq(&phba->hbalock);

	psli = &phba->sli;

	 
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"0337 Restart HBA Data: x%x x%x\n",
			(phba->pport) ? phba->pport->port_state : 0,
			psli->sli_flag);

	mb.word0 = 0;
	mb.mbxCommand = MBX_RESTART;
	mb.mbxHc = 1;

	lpfc_reset_barrier(phba);

	to_slim = phba->MBslimaddr;
	writel(mb.word0, to_slim);
	readl(to_slim);  

	 
	if (phba->pport && phba->pport->port_state)
		mb.word0 = 1;	 
	else
		mb.word0 = 0;	 
	to_slim = phba->MBslimaddr + sizeof (uint32_t);
	writel(mb.word0, to_slim);
	readl(to_slim);  

	lpfc_sli_brdreset(phba);
	if (phba->pport)
		phba->pport->stopped = 0;
	phba->link_state = LPFC_INIT_START;
	phba->hba_flag = 0;
	spin_unlock_irq(&phba->hbalock);

	memset(&psli->lnk_stat_offsets, 0, sizeof(psli->lnk_stat_offsets));
	psli->stats_start = ktime_get_seconds();

	 
	mdelay(100);

	lpfc_hba_down_post(phba);

	return 0;
}

 
static int
lpfc_sli_brdrestart_s4(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	int rc;

	 
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"0296 Restart HBA Data: x%x x%x\n",
			phba->pport->port_state, psli->sli_flag);

	rc = lpfc_sli4_brdreset(phba);
	if (rc) {
		phba->link_state = LPFC_HBA_ERROR;
		goto hba_down_queue;
	}

	spin_lock_irq(&phba->hbalock);
	phba->pport->stopped = 0;
	phba->link_state = LPFC_INIT_START;
	phba->hba_flag = 0;
	 
	phba->sli4_hba.fawwpn_flag &= LPFC_FAWWPN_FABRIC;
	spin_unlock_irq(&phba->hbalock);

	memset(&psli->lnk_stat_offsets, 0, sizeof(psli->lnk_stat_offsets));
	psli->stats_start = ktime_get_seconds();

hba_down_queue:
	lpfc_hba_down_post(phba);
	lpfc_sli4_queue_destroy(phba);

	return rc;
}

 
int
lpfc_sli_brdrestart(struct lpfc_hba *phba)
{
	return phba->lpfc_sli_brdrestart(phba);
}

 
int
lpfc_sli_chipset_init(struct lpfc_hba *phba)
{
	uint32_t status, i = 0;

	 
	if (lpfc_readl(phba->HSregaddr, &status))
		return -EIO;

	 
	i = 0;
	while ((status & (HS_FFRDY | HS_MBRDY)) != (HS_FFRDY | HS_MBRDY)) {

		 
		if (i++ >= 200) {
			 
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0436 Adapter failed to init, "
					"timeout, status reg x%x, "
					"FW Data: A8 x%x AC x%x\n", status,
					readl(phba->MBslimaddr + 0xa8),
					readl(phba->MBslimaddr + 0xac));
			phba->link_state = LPFC_HBA_ERROR;
			return -ETIMEDOUT;
		}

		 
		if (status & HS_FFERM) {
			 
			 
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0437 Adapter failed to init, "
					"chipset, status reg x%x, "
					"FW Data: A8 x%x AC x%x\n", status,
					readl(phba->MBslimaddr + 0xa8),
					readl(phba->MBslimaddr + 0xac));
			phba->link_state = LPFC_HBA_ERROR;
			return -EIO;
		}

		if (i <= 10)
			msleep(10);
		else if (i <= 100)
			msleep(100);
		else
			msleep(1000);

		if (i == 150) {
			 
			phba->pport->port_state = LPFC_VPORT_UNKNOWN;
			lpfc_sli_brdrestart(phba);
		}
		 
		if (lpfc_readl(phba->HSregaddr, &status))
			return -EIO;
	}

	 
	if (status & HS_FFERM) {
		 
		 
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0438 Adapter failed to init, chipset, "
				"status reg x%x, "
				"FW Data: A8 x%x AC x%x\n", status,
				readl(phba->MBslimaddr + 0xa8),
				readl(phba->MBslimaddr + 0xac));
		phba->link_state = LPFC_HBA_ERROR;
		return -EIO;
	}

	phba->hba_flag |= HBA_NEEDS_CFG_PORT;

	 
	writel(0, phba->HCregaddr);
	readl(phba->HCregaddr);  

	 
	writel(0xffffffff, phba->HAregaddr);
	readl(phba->HAregaddr);  
	return 0;
}

 
int
lpfc_sli_hbq_count(void)
{
	return ARRAY_SIZE(lpfc_hbq_defs);
}

 
static int
lpfc_sli_hbq_entry_count(void)
{
	int  hbq_count = lpfc_sli_hbq_count();
	int  count = 0;
	int  i;

	for (i = 0; i < hbq_count; ++i)
		count += lpfc_hbq_defs[i]->entry_count;
	return count;
}

 
int
lpfc_sli_hbq_size(void)
{
	return lpfc_sli_hbq_entry_count() * sizeof(struct lpfc_hbq_entry);
}

 
static int
lpfc_sli_hbq_setup(struct lpfc_hba *phba)
{
	int  hbq_count = lpfc_sli_hbq_count();
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *pmbox;
	uint32_t hbqno;
	uint32_t hbq_entry_index;

				 
	pmb = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);

	if (!pmb)
		return -ENOMEM;

	pmbox = &pmb->u.mb;

	 
	phba->link_state = LPFC_INIT_MBX_CMDS;
	phba->hbq_in_use = 1;

	hbq_entry_index = 0;
	for (hbqno = 0; hbqno < hbq_count; ++hbqno) {
		phba->hbqs[hbqno].next_hbqPutIdx = 0;
		phba->hbqs[hbqno].hbqPutIdx      = 0;
		phba->hbqs[hbqno].local_hbqGetIdx   = 0;
		phba->hbqs[hbqno].entry_count =
			lpfc_hbq_defs[hbqno]->entry_count;
		lpfc_config_hbq(phba, hbqno, lpfc_hbq_defs[hbqno],
			hbq_entry_index, pmb);
		hbq_entry_index += phba->hbqs[hbqno].entry_count;

		if (lpfc_sli_issue_mbox(phba, pmb, MBX_POLL) != MBX_SUCCESS) {
			 

			lpfc_printf_log(phba, KERN_ERR,
					LOG_SLI | LOG_VPORT,
					"1805 Adapter failed to init. "
					"Data: x%x x%x x%x\n",
					pmbox->mbxCommand,
					pmbox->mbxStatus, hbqno);

			phba->link_state = LPFC_HBA_ERROR;
			mempool_free(pmb, phba->mbox_mem_pool);
			return -ENXIO;
		}
	}
	phba->hbq_count = hbq_count;

	mempool_free(pmb, phba->mbox_mem_pool);

	 
	for (hbqno = 0; hbqno < hbq_count; ++hbqno)
		lpfc_sli_hbqbuf_init_hbqs(phba, hbqno);
	return 0;
}

 
static int
lpfc_sli4_rb_setup(struct lpfc_hba *phba)
{
	phba->hbq_in_use = 1;
	 
	if (phba->cfg_enable_mds_diags && phba->mds_diags_support)
		phba->hbqs[LPFC_ELS_HBQ].entry_count =
			lpfc_hbq_defs[LPFC_ELS_HBQ]->entry_count >> 1;
	else
		phba->hbqs[LPFC_ELS_HBQ].entry_count =
			lpfc_hbq_defs[LPFC_ELS_HBQ]->entry_count;
	phba->hbq_count = 1;
	lpfc_sli_hbqbuf_init_hbqs(phba, LPFC_ELS_HBQ);
	 
	return 0;
}

 
int
lpfc_sli_config_port(struct lpfc_hba *phba, int sli_mode)
{
	LPFC_MBOXQ_t *pmb;
	uint32_t resetcount = 0, rc = 0, done = 0;

	pmb = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		phba->link_state = LPFC_HBA_ERROR;
		return -ENOMEM;
	}

	phba->sli_rev = sli_mode;
	while (resetcount < 2 && !done) {
		spin_lock_irq(&phba->hbalock);
		phba->sli.sli_flag |= LPFC_SLI_MBOX_ACTIVE;
		spin_unlock_irq(&phba->hbalock);
		phba->pport->port_state = LPFC_VPORT_UNKNOWN;
		lpfc_sli_brdrestart(phba);
		rc = lpfc_sli_chipset_init(phba);
		if (rc)
			break;

		spin_lock_irq(&phba->hbalock);
		phba->sli.sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
		spin_unlock_irq(&phba->hbalock);
		resetcount++;

		 
		rc = lpfc_config_port_prep(phba);
		if (rc == -ERESTART) {
			phba->link_state = LPFC_LINK_UNKNOWN;
			continue;
		} else if (rc)
			break;

		phba->link_state = LPFC_INIT_MBX_CMDS;
		lpfc_config_port(phba, pmb);
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);
		phba->sli3_options &= ~(LPFC_SLI3_NPIV_ENABLED |
					LPFC_SLI3_HBQ_ENABLED |
					LPFC_SLI3_CRP_ENABLED |
					LPFC_SLI3_DSS_ENABLED);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0442 Adapter failed to init, mbxCmd x%x "
				"CONFIG_PORT, mbxStatus x%x Data: x%x\n",
				pmb->u.mb.mbxCommand, pmb->u.mb.mbxStatus, 0);
			spin_lock_irq(&phba->hbalock);
			phba->sli.sli_flag &= ~LPFC_SLI_ACTIVE;
			spin_unlock_irq(&phba->hbalock);
			rc = -ENXIO;
		} else {
			 
			spin_lock_irq(&phba->hbalock);
			phba->sli.sli_flag &= ~LPFC_SLI_ASYNC_MBX_BLK;
			spin_unlock_irq(&phba->hbalock);
			done = 1;

			if ((pmb->u.mb.un.varCfgPort.casabt == 1) &&
			    (pmb->u.mb.un.varCfgPort.gasabt == 0))
				lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
					"3110 Port did not grant ASABT\n");
		}
	}
	if (!done) {
		rc = -EINVAL;
		goto do_prep_failed;
	}
	if (pmb->u.mb.un.varCfgPort.sli_mode == 3) {
		if (!pmb->u.mb.un.varCfgPort.cMA) {
			rc = -ENXIO;
			goto do_prep_failed;
		}
		if (phba->max_vpi && pmb->u.mb.un.varCfgPort.gmv) {
			phba->sli3_options |= LPFC_SLI3_NPIV_ENABLED;
			phba->max_vpi = pmb->u.mb.un.varCfgPort.max_vpi;
			phba->max_vports = (phba->max_vpi > phba->max_vports) ?
				phba->max_vpi : phba->max_vports;

		} else
			phba->max_vpi = 0;
		if (pmb->u.mb.un.varCfgPort.gerbm)
			phba->sli3_options |= LPFC_SLI3_HBQ_ENABLED;
		if (pmb->u.mb.un.varCfgPort.gcrp)
			phba->sli3_options |= LPFC_SLI3_CRP_ENABLED;

		phba->hbq_get = phba->mbox->us.s3_pgp.hbq_get;
		phba->port_gp = phba->mbox->us.s3_pgp.port;

		if (phba->sli3_options & LPFC_SLI3_BG_ENABLED) {
			if (pmb->u.mb.un.varCfgPort.gbg == 0) {
				phba->cfg_enable_bg = 0;
				phba->sli3_options &= ~LPFC_SLI3_BG_ENABLED;
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"0443 Adapter did not grant "
						"BlockGuard\n");
			}
		}
	} else {
		phba->hbq_get = NULL;
		phba->port_gp = phba->mbox->us.s2.port;
		phba->max_vpi = 0;
	}
do_prep_failed:
	mempool_free(pmb, phba->mbox_mem_pool);
	return rc;
}


 
int
lpfc_sli_hba_setup(struct lpfc_hba *phba)
{
	uint32_t rc;
	int  i;
	int longs;

	 
	if (phba->hba_flag & HBA_NEEDS_CFG_PORT) {
		rc = lpfc_sli_config_port(phba, LPFC_SLI_REV3);
		if (rc)
			return -EIO;
		phba->hba_flag &= ~HBA_NEEDS_CFG_PORT;
	}
	phba->fcp_embed_io = 0;	 

	if (phba->sli_rev == 3) {
		phba->iocb_cmd_size = SLI3_IOCB_CMD_SIZE;
		phba->iocb_rsp_size = SLI3_IOCB_RSP_SIZE;
	} else {
		phba->iocb_cmd_size = SLI2_IOCB_CMD_SIZE;
		phba->iocb_rsp_size = SLI2_IOCB_RSP_SIZE;
		phba->sli3_options = 0;
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"0444 Firmware in SLI %x mode. Max_vpi %d\n",
			phba->sli_rev, phba->max_vpi);
	rc = lpfc_sli_ring_map(phba);

	if (rc)
		goto lpfc_sli_hba_setup_error;

	 
	if (phba->sli_rev == LPFC_SLI_REV3) {
		 
		if ((phba->vpi_bmask == NULL) && (phba->vpi_ids == NULL)) {
			longs = (phba->max_vpi + BITS_PER_LONG) / BITS_PER_LONG;
			phba->vpi_bmask = kcalloc(longs,
						  sizeof(unsigned long),
						  GFP_KERNEL);
			if (!phba->vpi_bmask) {
				rc = -ENOMEM;
				goto lpfc_sli_hba_setup_error;
			}

			phba->vpi_ids = kcalloc(phba->max_vpi + 1,
						sizeof(uint16_t),
						GFP_KERNEL);
			if (!phba->vpi_ids) {
				kfree(phba->vpi_bmask);
				rc = -ENOMEM;
				goto lpfc_sli_hba_setup_error;
			}
			for (i = 0; i < phba->max_vpi; i++)
				phba->vpi_ids[i] = i;
		}
	}

	 
	if (phba->sli3_options & LPFC_SLI3_HBQ_ENABLED) {
		rc = lpfc_sli_hbq_setup(phba);
		if (rc)
			goto lpfc_sli_hba_setup_error;
	}
	spin_lock_irq(&phba->hbalock);
	phba->sli.sli_flag |= LPFC_PROCESS_LA;
	spin_unlock_irq(&phba->hbalock);

	rc = lpfc_config_port_post(phba);
	if (rc)
		goto lpfc_sli_hba_setup_error;

	return rc;

lpfc_sli_hba_setup_error:
	phba->link_state = LPFC_HBA_ERROR;
	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"0445 Firmware initialization failed\n");
	return rc;
}

 
static int
lpfc_sli4_read_fcoe_params(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_dmabuf *mp;
	struct lpfc_mqe *mqe;
	uint32_t data_length;
	int rc;

	 
	phba->valid_vlan = 0;
	phba->fc_map[0] = LPFC_FCOE_FCF_MAP0;
	phba->fc_map[1] = LPFC_FCOE_FCF_MAP1;
	phba->fc_map[2] = LPFC_FCOE_FCF_MAP2;

	mboxq = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		return -ENOMEM;

	mqe = &mboxq->u.mqe;
	if (lpfc_sli4_dump_cfg_rg23(phba, mboxq)) {
		rc = -ENOMEM;
		goto out_free_mboxq;
	}

	mp = (struct lpfc_dmabuf *)mboxq->ctx_buf;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);

	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
			"(%d):2571 Mailbox cmd x%x Status x%x "
			"Data: x%x x%x x%x x%x x%x x%x x%x x%x x%x "
			"x%x x%x x%x x%x x%x x%x x%x x%x x%x "
			"CQ: x%x x%x x%x x%x\n",
			mboxq->vport ? mboxq->vport->vpi : 0,
			bf_get(lpfc_mqe_command, mqe),
			bf_get(lpfc_mqe_status, mqe),
			mqe->un.mb_words[0], mqe->un.mb_words[1],
			mqe->un.mb_words[2], mqe->un.mb_words[3],
			mqe->un.mb_words[4], mqe->un.mb_words[5],
			mqe->un.mb_words[6], mqe->un.mb_words[7],
			mqe->un.mb_words[8], mqe->un.mb_words[9],
			mqe->un.mb_words[10], mqe->un.mb_words[11],
			mqe->un.mb_words[12], mqe->un.mb_words[13],
			mqe->un.mb_words[14], mqe->un.mb_words[15],
			mqe->un.mb_words[16], mqe->un.mb_words[50],
			mboxq->mcqe.word0,
			mboxq->mcqe.mcqe_tag0, 	mboxq->mcqe.mcqe_tag1,
			mboxq->mcqe.trailer);

	if (rc) {
		rc = -EIO;
		goto out_free_mboxq;
	}
	data_length = mqe->un.mb_words[5];
	if (data_length > DMP_RGN23_SIZE) {
		rc = -EIO;
		goto out_free_mboxq;
	}

	lpfc_parse_fcoe_conf(phba, mp->virt, data_length);
	rc = 0;

out_free_mboxq:
	lpfc_mbox_rsrc_cleanup(phba, mboxq, MBOX_THD_UNLOCKED);
	return rc;
}

 
static int
lpfc_sli4_read_rev(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq,
		    uint8_t *vpd, uint32_t *vpd_size)
{
	int rc = 0;
	uint32_t dma_size;
	struct lpfc_dmabuf *dmabuf;
	struct lpfc_mqe *mqe;

	dmabuf = kzalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (!dmabuf)
		return -ENOMEM;

	 
	dma_size = *vpd_size;
	dmabuf->virt = dma_alloc_coherent(&phba->pcidev->dev, dma_size,
					  &dmabuf->phys, GFP_KERNEL);
	if (!dmabuf->virt) {
		kfree(dmabuf);
		return -ENOMEM;
	}

	 
	lpfc_read_rev(phba, mboxq);
	mqe = &mboxq->u.mqe;
	mqe->un.read_rev.vpd_paddr_high = putPaddrHigh(dmabuf->phys);
	mqe->un.read_rev.vpd_paddr_low = putPaddrLow(dmabuf->phys);
	mqe->un.read_rev.word1 &= 0x0000FFFF;
	bf_set(lpfc_mbx_rd_rev_vpd, &mqe->un.read_rev, 1);
	bf_set(lpfc_mbx_rd_rev_avail_len, &mqe->un.read_rev, dma_size);

	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	if (rc) {
		dma_free_coherent(&phba->pcidev->dev, dma_size,
				  dmabuf->virt, dmabuf->phys);
		kfree(dmabuf);
		return -EIO;
	}

	 
	if (mqe->un.read_rev.avail_vpd_len < *vpd_size)
		*vpd_size = mqe->un.read_rev.avail_vpd_len;

	memcpy(vpd, dmabuf->virt, *vpd_size);

	dma_free_coherent(&phba->pcidev->dev, dma_size,
			  dmabuf->virt, dmabuf->phys);
	kfree(dmabuf);
	return 0;
}

 
static int
lpfc_sli4_get_ctl_attr(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_mbx_get_cntl_attributes *mbx_cntl_attr;
	struct lpfc_controller_attribute *cntl_attr;
	void *virtaddr = NULL;
	uint32_t alloclen, reqlen;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	int rc;

	mboxq = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		return -ENOMEM;

	 
	reqlen = sizeof(struct lpfc_mbx_get_cntl_attributes);
	alloclen = lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_COMMON,
			LPFC_MBOX_OPCODE_GET_CNTL_ATTRIBUTES, reqlen,
			LPFC_SLI4_MBX_NEMBED);

	if (alloclen < reqlen) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3084 Allocated DMA memory size (%d) is "
				"less than the requested DMA memory size "
				"(%d)\n", alloclen, reqlen);
		rc = -ENOMEM;
		goto out_free_mboxq;
	}
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	virtaddr = mboxq->sge_array->addr[0];
	mbx_cntl_attr = (struct lpfc_mbx_get_cntl_attributes *)virtaddr;
	shdr = &mbx_cntl_attr->cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"3085 Mailbox x%x (x%x/x%x) failed, "
				"rc:x%x, status:x%x, add_status:x%x\n",
				bf_get(lpfc_mqe_command, &mboxq->u.mqe),
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				rc, shdr_status, shdr_add_status);
		rc = -ENXIO;
		goto out_free_mboxq;
	}

	cntl_attr = &mbx_cntl_attr->cntl_attr;
	phba->sli4_hba.lnk_info.lnk_dv = LPFC_LNK_DAT_VAL;
	phba->sli4_hba.lnk_info.lnk_tp =
		bf_get(lpfc_cntl_attr_lnk_type, cntl_attr);
	phba->sli4_hba.lnk_info.lnk_no =
		bf_get(lpfc_cntl_attr_lnk_numb, cntl_attr);
	phba->sli4_hba.flash_id = bf_get(lpfc_cntl_attr_flash_id, cntl_attr);
	phba->sli4_hba.asic_rev = bf_get(lpfc_cntl_attr_asic_rev, cntl_attr);

	memset(phba->BIOSVersion, 0, sizeof(phba->BIOSVersion));
	strlcat(phba->BIOSVersion, (char *)cntl_attr->bios_ver_str,
		sizeof(phba->BIOSVersion));

	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"3086 lnk_type:%d, lnk_numb:%d, bios_ver:%s, "
			"flash_id: x%02x, asic_rev: x%02x\n",
			phba->sli4_hba.lnk_info.lnk_tp,
			phba->sli4_hba.lnk_info.lnk_no,
			phba->BIOSVersion, phba->sli4_hba.flash_id,
			phba->sli4_hba.asic_rev);
out_free_mboxq:
	if (bf_get(lpfc_mqe_command, &mboxq->u.mqe) == MBX_SLI4_CONFIG)
		lpfc_sli4_mbox_cmd_free(phba, mboxq);
	else
		mempool_free(mboxq, phba->mbox_mem_pool);
	return rc;
}

 
static int
lpfc_sli4_retrieve_pport_name(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_mbx_get_port_name *get_port_name;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	char cport_name = 0;
	int rc;

	 
	phba->sli4_hba.lnk_info.lnk_dv = LPFC_LNK_DAT_INVAL;
	phba->sli4_hba.pport_name_sta = LPFC_SLI4_PPNAME_NON;

	mboxq = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		return -ENOMEM;
	 
	phba->sli4_hba.lnk_info.lnk_dv = LPFC_LNK_DAT_INVAL;
	lpfc_sli4_read_config(phba);

	if (phba->sli4_hba.fawwpn_flag & LPFC_FAWWPN_CONFIG)
		phba->sli4_hba.fawwpn_flag |= LPFC_FAWWPN_FABRIC;

	if (phba->sli4_hba.lnk_info.lnk_dv == LPFC_LNK_DAT_VAL)
		goto retrieve_ppname;

	 
	rc = lpfc_sli4_get_ctl_attr(phba);
	if (rc)
		goto out_free_mboxq;

retrieve_ppname:
	lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_COMMON,
		LPFC_MBOX_OPCODE_GET_PORT_NAME,
		sizeof(struct lpfc_mbx_get_port_name) -
		sizeof(struct lpfc_sli4_cfg_mhdr),
		LPFC_SLI4_MBX_EMBED);
	get_port_name = &mboxq->u.mqe.un.get_port_name;
	shdr = (union lpfc_sli4_cfg_shdr *)&get_port_name->header.cfg_shdr;
	bf_set(lpfc_mbox_hdr_version, &shdr->request, LPFC_OPCODE_VERSION_1);
	bf_set(lpfc_mbx_get_port_name_lnk_type, &get_port_name->u.request,
		phba->sli4_hba.lnk_info.lnk_tp);
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"3087 Mailbox x%x (x%x/x%x) failed: "
				"rc:x%x, status:x%x, add_status:x%x\n",
				bf_get(lpfc_mqe_command, &mboxq->u.mqe),
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				rc, shdr_status, shdr_add_status);
		rc = -ENXIO;
		goto out_free_mboxq;
	}
	switch (phba->sli4_hba.lnk_info.lnk_no) {
	case LPFC_LINK_NUMBER_0:
		cport_name = bf_get(lpfc_mbx_get_port_name_name0,
				&get_port_name->u.response);
		phba->sli4_hba.pport_name_sta = LPFC_SLI4_PPNAME_GET;
		break;
	case LPFC_LINK_NUMBER_1:
		cport_name = bf_get(lpfc_mbx_get_port_name_name1,
				&get_port_name->u.response);
		phba->sli4_hba.pport_name_sta = LPFC_SLI4_PPNAME_GET;
		break;
	case LPFC_LINK_NUMBER_2:
		cport_name = bf_get(lpfc_mbx_get_port_name_name2,
				&get_port_name->u.response);
		phba->sli4_hba.pport_name_sta = LPFC_SLI4_PPNAME_GET;
		break;
	case LPFC_LINK_NUMBER_3:
		cport_name = bf_get(lpfc_mbx_get_port_name_name3,
				&get_port_name->u.response);
		phba->sli4_hba.pport_name_sta = LPFC_SLI4_PPNAME_GET;
		break;
	default:
		break;
	}

	if (phba->sli4_hba.pport_name_sta == LPFC_SLI4_PPNAME_GET) {
		phba->Port[0] = cport_name;
		phba->Port[1] = '\0';
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"3091 SLI get port name: %s\n", phba->Port);
	}

out_free_mboxq:
	if (bf_get(lpfc_mqe_command, &mboxq->u.mqe) == MBX_SLI4_CONFIG)
		lpfc_sli4_mbox_cmd_free(phba, mboxq);
	else
		mempool_free(mboxq, phba->mbox_mem_pool);
	return rc;
}

 
static void
lpfc_sli4_arm_cqeq_intr(struct lpfc_hba *phba)
{
	int qidx;
	struct lpfc_sli4_hba *sli4_hba = &phba->sli4_hba;
	struct lpfc_sli4_hdw_queue *qp;
	struct lpfc_queue *eq;

	sli4_hba->sli4_write_cq_db(phba, sli4_hba->mbx_cq, 0, LPFC_QUEUE_REARM);
	sli4_hba->sli4_write_cq_db(phba, sli4_hba->els_cq, 0, LPFC_QUEUE_REARM);
	if (sli4_hba->nvmels_cq)
		sli4_hba->sli4_write_cq_db(phba, sli4_hba->nvmels_cq, 0,
					   LPFC_QUEUE_REARM);

	if (sli4_hba->hdwq) {
		 
		for (qidx = 0; qidx < phba->cfg_hdw_queue; qidx++) {
			qp = &sli4_hba->hdwq[qidx];
			 
			sli4_hba->sli4_write_cq_db(phba, qp->io_cq, 0,
						LPFC_QUEUE_REARM);
		}

		 
		for (qidx = 0; qidx < phba->cfg_irq_chann; qidx++) {
			eq = sli4_hba->hba_eq_hdl[qidx].eq;
			 
			sli4_hba->sli4_write_eq_db(phba, eq,
						   0, LPFC_QUEUE_REARM);
		}
	}

	if (phba->nvmet_support) {
		for (qidx = 0; qidx < phba->cfg_nvmet_mrq; qidx++) {
			sli4_hba->sli4_write_cq_db(phba,
				sli4_hba->nvmet_cqset[qidx], 0,
				LPFC_QUEUE_REARM);
		}
	}
}

 
int
lpfc_sli4_get_avail_extnt_rsrc(struct lpfc_hba *phba, uint16_t type,
			       uint16_t *extnt_count, uint16_t *extnt_size)
{
	int rc = 0;
	uint32_t length;
	uint32_t mbox_tmo;
	struct lpfc_mbx_get_rsrc_extent_info *rsrc_info;
	LPFC_MBOXQ_t *mbox;

	*extnt_count = 0;
	*extnt_size = 0;

	mbox = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	 
	length = (sizeof(struct lpfc_mbx_get_rsrc_extent_info) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_GET_RSRC_EXTENT_INFO,
			 length, LPFC_SLI4_MBX_EMBED);

	 
	rc = lpfc_sli4_mbox_rsrc_extent(phba, mbox, 0, type,
					LPFC_SLI4_MBX_EMBED);
	if (unlikely(rc)) {
		rc = -EIO;
		goto err_exit;
	}

	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}
	if (unlikely(rc)) {
		rc = -EIO;
		goto err_exit;
	}

	rsrc_info = &mbox->u.mqe.un.rsrc_extent_info;
	if (bf_get(lpfc_mbox_hdr_status,
		   &rsrc_info->header.cfg_shdr.response)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2930 Failed to get resource extents "
				"Status 0x%x Add'l Status 0x%x\n",
				bf_get(lpfc_mbox_hdr_status,
				       &rsrc_info->header.cfg_shdr.response),
				bf_get(lpfc_mbox_hdr_add_status,
				       &rsrc_info->header.cfg_shdr.response));
		rc = -EIO;
		goto err_exit;
	}

	*extnt_count = bf_get(lpfc_mbx_get_rsrc_extent_info_cnt,
			      &rsrc_info->u.rsp);
	*extnt_size = bf_get(lpfc_mbx_get_rsrc_extent_info_size,
			     &rsrc_info->u.rsp);

	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"3162 Retrieved extents type-%d from port: count:%d, "
			"size:%d\n", type, *extnt_count, *extnt_size);

err_exit:
	mempool_free(mbox, phba->mbox_mem_pool);
	return rc;
}

 
static int
lpfc_sli4_chk_avail_extnt_rsrc(struct lpfc_hba *phba, uint16_t type)
{
	uint16_t curr_ext_cnt, rsrc_ext_cnt;
	uint16_t size_diff, rsrc_ext_size;
	int rc = 0;
	struct lpfc_rsrc_blks *rsrc_entry;
	struct list_head *rsrc_blk_list = NULL;

	size_diff = 0;
	curr_ext_cnt = 0;
	rc = lpfc_sli4_get_avail_extnt_rsrc(phba, type,
					    &rsrc_ext_cnt,
					    &rsrc_ext_size);
	if (unlikely(rc))
		return -EIO;

	switch (type) {
	case LPFC_RSC_TYPE_FCOE_RPI:
		rsrc_blk_list = &phba->sli4_hba.lpfc_rpi_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_VPI:
		rsrc_blk_list = &phba->lpfc_vpi_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_XRI:
		rsrc_blk_list = &phba->sli4_hba.lpfc_xri_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_VFI:
		rsrc_blk_list = &phba->sli4_hba.lpfc_vfi_blk_list;
		break;
	default:
		break;
	}

	list_for_each_entry(rsrc_entry, rsrc_blk_list, list) {
		curr_ext_cnt++;
		if (rsrc_entry->rsrc_size != rsrc_ext_size)
			size_diff++;
	}

	if (curr_ext_cnt != rsrc_ext_cnt || size_diff != 0)
		rc = 1;

	return rc;
}

 
static int
lpfc_sli4_cfg_post_extnts(struct lpfc_hba *phba, uint16_t extnt_cnt,
			  uint16_t type, bool *emb, LPFC_MBOXQ_t *mbox)
{
	int rc = 0;
	uint32_t req_len;
	uint32_t emb_len;
	uint32_t alloc_len, mbox_tmo;

	 
	req_len = extnt_cnt * sizeof(uint16_t);

	 
	emb_len = sizeof(MAILBOX_t) - sizeof(struct mbox_header) -
		sizeof(uint32_t);

	 
	*emb = LPFC_SLI4_MBX_EMBED;
	if (req_len > emb_len) {
		req_len = extnt_cnt * sizeof(uint16_t) +
			sizeof(union lpfc_sli4_cfg_shdr) +
			sizeof(uint32_t);
		*emb = LPFC_SLI4_MBX_NEMBED;
	}

	alloc_len = lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
				     LPFC_MBOX_OPCODE_ALLOC_RSRC_EXTENT,
				     req_len, *emb);
	if (alloc_len < req_len) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"2982 Allocated DMA memory size (x%x) is "
			"less than the requested DMA memory "
			"size (x%x)\n", alloc_len, req_len);
		return -ENOMEM;
	}
	rc = lpfc_sli4_mbox_rsrc_extent(phba, mbox, extnt_cnt, type, *emb);
	if (unlikely(rc))
		return -EIO;

	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}

	if (unlikely(rc))
		rc = -EIO;
	return rc;
}

 
static int
lpfc_sli4_alloc_extent(struct lpfc_hba *phba, uint16_t type)
{
	bool emb = false;
	uint16_t rsrc_id_cnt, rsrc_cnt, rsrc_size;
	uint16_t rsrc_id, rsrc_start, j, k;
	uint16_t *ids;
	int i, rc;
	unsigned long longs;
	unsigned long *bmask;
	struct lpfc_rsrc_blks *rsrc_blks;
	LPFC_MBOXQ_t *mbox;
	uint32_t length;
	struct lpfc_id_range *id_array = NULL;
	void *virtaddr = NULL;
	struct lpfc_mbx_nembed_rsrc_extent *n_rsrc;
	struct lpfc_mbx_alloc_rsrc_extents *rsrc_ext;
	struct list_head *ext_blk_list;

	rc = lpfc_sli4_get_avail_extnt_rsrc(phba, type,
					    &rsrc_cnt,
					    &rsrc_size);
	if (unlikely(rc))
		return -EIO;

	if ((rsrc_cnt == 0) || (rsrc_size == 0)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"3009 No available Resource Extents "
			"for resource type 0x%x: Count: 0x%x, "
			"Size 0x%x\n", type, rsrc_cnt,
			rsrc_size);
		return -ENOMEM;
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_INIT | LOG_SLI,
			"2903 Post resource extents type-0x%x: "
			"count:%d, size %d\n", type, rsrc_cnt, rsrc_size);

	mbox = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	rc = lpfc_sli4_cfg_post_extnts(phba, rsrc_cnt, type, &emb, mbox);
	if (unlikely(rc)) {
		rc = -EIO;
		goto err_exit;
	}

	 
	if (emb == LPFC_SLI4_MBX_EMBED) {
		rsrc_ext = &mbox->u.mqe.un.alloc_rsrc_extents;
		id_array = &rsrc_ext->u.rsp.id[0];
		rsrc_cnt = bf_get(lpfc_mbx_rsrc_cnt, &rsrc_ext->u.rsp);
	} else {
		virtaddr = mbox->sge_array->addr[0];
		n_rsrc = (struct lpfc_mbx_nembed_rsrc_extent *) virtaddr;
		rsrc_cnt = bf_get(lpfc_mbx_rsrc_cnt, n_rsrc);
		id_array = &n_rsrc->id;
	}

	longs = ((rsrc_cnt * rsrc_size) + BITS_PER_LONG - 1) / BITS_PER_LONG;
	rsrc_id_cnt = rsrc_cnt * rsrc_size;

	 
	length = sizeof(struct lpfc_rsrc_blks);
	switch (type) {
	case LPFC_RSC_TYPE_FCOE_RPI:
		phba->sli4_hba.rpi_bmask = kcalloc(longs,
						   sizeof(unsigned long),
						   GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.rpi_bmask)) {
			rc = -ENOMEM;
			goto err_exit;
		}
		phba->sli4_hba.rpi_ids = kcalloc(rsrc_id_cnt,
						 sizeof(uint16_t),
						 GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.rpi_ids)) {
			kfree(phba->sli4_hba.rpi_bmask);
			rc = -ENOMEM;
			goto err_exit;
		}

		 
		phba->sli4_hba.next_rpi = rsrc_id_cnt;

		 
		bmask = phba->sli4_hba.rpi_bmask;
		ids = phba->sli4_hba.rpi_ids;
		ext_blk_list = &phba->sli4_hba.lpfc_rpi_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_VPI:
		phba->vpi_bmask = kcalloc(longs, sizeof(unsigned long),
					  GFP_KERNEL);
		if (unlikely(!phba->vpi_bmask)) {
			rc = -ENOMEM;
			goto err_exit;
		}
		phba->vpi_ids = kcalloc(rsrc_id_cnt, sizeof(uint16_t),
					 GFP_KERNEL);
		if (unlikely(!phba->vpi_ids)) {
			kfree(phba->vpi_bmask);
			rc = -ENOMEM;
			goto err_exit;
		}

		 
		bmask = phba->vpi_bmask;
		ids = phba->vpi_ids;
		ext_blk_list = &phba->lpfc_vpi_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_XRI:
		phba->sli4_hba.xri_bmask = kcalloc(longs,
						   sizeof(unsigned long),
						   GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.xri_bmask)) {
			rc = -ENOMEM;
			goto err_exit;
		}
		phba->sli4_hba.max_cfg_param.xri_used = 0;
		phba->sli4_hba.xri_ids = kcalloc(rsrc_id_cnt,
						 sizeof(uint16_t),
						 GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.xri_ids)) {
			kfree(phba->sli4_hba.xri_bmask);
			rc = -ENOMEM;
			goto err_exit;
		}

		 
		bmask = phba->sli4_hba.xri_bmask;
		ids = phba->sli4_hba.xri_ids;
		ext_blk_list = &phba->sli4_hba.lpfc_xri_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_VFI:
		phba->sli4_hba.vfi_bmask = kcalloc(longs,
						   sizeof(unsigned long),
						   GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.vfi_bmask)) {
			rc = -ENOMEM;
			goto err_exit;
		}
		phba->sli4_hba.vfi_ids = kcalloc(rsrc_id_cnt,
						 sizeof(uint16_t),
						 GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.vfi_ids)) {
			kfree(phba->sli4_hba.vfi_bmask);
			rc = -ENOMEM;
			goto err_exit;
		}

		 
		bmask = phba->sli4_hba.vfi_bmask;
		ids = phba->sli4_hba.vfi_ids;
		ext_blk_list = &phba->sli4_hba.lpfc_vfi_blk_list;
		break;
	default:
		 
		id_array = NULL;
		bmask = NULL;
		ids = NULL;
		ext_blk_list = NULL;
		goto err_exit;
	}

	 
	for (i = 0, j = 0, k = 0; i < rsrc_cnt; i++) {
		if ((i % 2) == 0)
			rsrc_id = bf_get(lpfc_mbx_rsrc_id_word4_0,
					 &id_array[k]);
		else
			rsrc_id = bf_get(lpfc_mbx_rsrc_id_word4_1,
					 &id_array[k]);

		rsrc_blks = kzalloc(length, GFP_KERNEL);
		if (unlikely(!rsrc_blks)) {
			rc = -ENOMEM;
			kfree(bmask);
			kfree(ids);
			goto err_exit;
		}
		rsrc_blks->rsrc_start = rsrc_id;
		rsrc_blks->rsrc_size = rsrc_size;
		list_add_tail(&rsrc_blks->list, ext_blk_list);
		rsrc_start = rsrc_id;
		if ((type == LPFC_RSC_TYPE_FCOE_XRI) && (j == 0)) {
			phba->sli4_hba.io_xri_start = rsrc_start +
				lpfc_sli4_get_iocb_cnt(phba);
		}

		while (rsrc_id < (rsrc_start + rsrc_size)) {
			ids[j] = rsrc_id;
			rsrc_id++;
			j++;
		}
		 
		if ((i % 2) == 1)
			k++;
	}
 err_exit:
	lpfc_sli4_mbox_cmd_free(phba, mbox);
	return rc;
}



 
static int
lpfc_sli4_dealloc_extent(struct lpfc_hba *phba, uint16_t type)
{
	int rc;
	uint32_t length, mbox_tmo = 0;
	LPFC_MBOXQ_t *mbox;
	struct lpfc_mbx_dealloc_rsrc_extents *dealloc_rsrc;
	struct lpfc_rsrc_blks *rsrc_blk, *rsrc_blk_next;

	mbox = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	 
	length = (sizeof(struct lpfc_mbx_dealloc_rsrc_extents) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_DEALLOC_RSRC_EXTENT,
			 length, LPFC_SLI4_MBX_EMBED);

	 
	rc = lpfc_sli4_mbox_rsrc_extent(phba, mbox, 0, type,
					LPFC_SLI4_MBX_EMBED);
	if (unlikely(rc)) {
		rc = -EIO;
		goto out_free_mbox;
	}
	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}
	if (unlikely(rc)) {
		rc = -EIO;
		goto out_free_mbox;
	}

	dealloc_rsrc = &mbox->u.mqe.un.dealloc_rsrc_extents;
	if (bf_get(lpfc_mbox_hdr_status,
		   &dealloc_rsrc->header.cfg_shdr.response)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2919 Failed to release resource extents "
				"for type %d - Status 0x%x Add'l Status 0x%x. "
				"Resource memory not released.\n",
				type,
				bf_get(lpfc_mbox_hdr_status,
				    &dealloc_rsrc->header.cfg_shdr.response),
				bf_get(lpfc_mbox_hdr_add_status,
				    &dealloc_rsrc->header.cfg_shdr.response));
		rc = -EIO;
		goto out_free_mbox;
	}

	 
	switch (type) {
	case LPFC_RSC_TYPE_FCOE_VPI:
		kfree(phba->vpi_bmask);
		kfree(phba->vpi_ids);
		bf_set(lpfc_vpi_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);
		list_for_each_entry_safe(rsrc_blk, rsrc_blk_next,
				    &phba->lpfc_vpi_blk_list, list) {
			list_del_init(&rsrc_blk->list);
			kfree(rsrc_blk);
		}
		phba->sli4_hba.max_cfg_param.vpi_used = 0;
		break;
	case LPFC_RSC_TYPE_FCOE_XRI:
		kfree(phba->sli4_hba.xri_bmask);
		kfree(phba->sli4_hba.xri_ids);
		list_for_each_entry_safe(rsrc_blk, rsrc_blk_next,
				    &phba->sli4_hba.lpfc_xri_blk_list, list) {
			list_del_init(&rsrc_blk->list);
			kfree(rsrc_blk);
		}
		break;
	case LPFC_RSC_TYPE_FCOE_VFI:
		kfree(phba->sli4_hba.vfi_bmask);
		kfree(phba->sli4_hba.vfi_ids);
		bf_set(lpfc_vfi_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);
		list_for_each_entry_safe(rsrc_blk, rsrc_blk_next,
				    &phba->sli4_hba.lpfc_vfi_blk_list, list) {
			list_del_init(&rsrc_blk->list);
			kfree(rsrc_blk);
		}
		break;
	case LPFC_RSC_TYPE_FCOE_RPI:
		 
		list_for_each_entry_safe(rsrc_blk, rsrc_blk_next,
				    &phba->sli4_hba.lpfc_rpi_blk_list, list) {
			list_del_init(&rsrc_blk->list);
			kfree(rsrc_blk);
		}
		break;
	default:
		break;
	}

	bf_set(lpfc_idx_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);

 out_free_mbox:
	mempool_free(mbox, phba->mbox_mem_pool);
	return rc;
}

static void
lpfc_set_features(struct lpfc_hba *phba, LPFC_MBOXQ_t *mbox,
		  uint32_t feature)
{
	uint32_t len;
	u32 sig_freq = 0;

	len = sizeof(struct lpfc_mbx_set_feature) -
		sizeof(struct lpfc_sli4_cfg_mhdr);
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_SET_FEATURES, len,
			 LPFC_SLI4_MBX_EMBED);

	switch (feature) {
	case LPFC_SET_UE_RECOVERY:
		bf_set(lpfc_mbx_set_feature_UER,
		       &mbox->u.mqe.un.set_feature, 1);
		mbox->u.mqe.un.set_feature.feature = LPFC_SET_UE_RECOVERY;
		mbox->u.mqe.un.set_feature.param_len = 8;
		break;
	case LPFC_SET_MDS_DIAGS:
		bf_set(lpfc_mbx_set_feature_mds,
		       &mbox->u.mqe.un.set_feature, 1);
		bf_set(lpfc_mbx_set_feature_mds_deep_loopbk,
		       &mbox->u.mqe.un.set_feature, 1);
		mbox->u.mqe.un.set_feature.feature = LPFC_SET_MDS_DIAGS;
		mbox->u.mqe.un.set_feature.param_len = 8;
		break;
	case LPFC_SET_CGN_SIGNAL:
		if (phba->cmf_active_mode == LPFC_CFG_OFF)
			sig_freq = 0;
		else
			sig_freq = phba->cgn_sig_freq;

		if (phba->cgn_reg_signal == EDC_CG_SIG_WARN_ALARM) {
			bf_set(lpfc_mbx_set_feature_CGN_alarm_freq,
			       &mbox->u.mqe.un.set_feature, sig_freq);
			bf_set(lpfc_mbx_set_feature_CGN_warn_freq,
			       &mbox->u.mqe.un.set_feature, sig_freq);
		}

		if (phba->cgn_reg_signal == EDC_CG_SIG_WARN_ONLY)
			bf_set(lpfc_mbx_set_feature_CGN_warn_freq,
			       &mbox->u.mqe.un.set_feature, sig_freq);

		if (phba->cmf_active_mode == LPFC_CFG_OFF ||
		    phba->cgn_reg_signal == EDC_CG_SIG_NOTSUPPORTED)
			sig_freq = 0;
		else
			sig_freq = lpfc_acqe_cgn_frequency;

		bf_set(lpfc_mbx_set_feature_CGN_acqe_freq,
		       &mbox->u.mqe.un.set_feature, sig_freq);

		mbox->u.mqe.un.set_feature.feature = LPFC_SET_CGN_SIGNAL;
		mbox->u.mqe.un.set_feature.param_len = 12;
		break;
	case LPFC_SET_DUAL_DUMP:
		bf_set(lpfc_mbx_set_feature_dd,
		       &mbox->u.mqe.un.set_feature, LPFC_ENABLE_DUAL_DUMP);
		bf_set(lpfc_mbx_set_feature_ddquery,
		       &mbox->u.mqe.un.set_feature, 0);
		mbox->u.mqe.un.set_feature.feature = LPFC_SET_DUAL_DUMP;
		mbox->u.mqe.un.set_feature.param_len = 4;
		break;
	case LPFC_SET_ENABLE_MI:
		mbox->u.mqe.un.set_feature.feature = LPFC_SET_ENABLE_MI;
		mbox->u.mqe.un.set_feature.param_len = 4;
		bf_set(lpfc_mbx_set_feature_milunq, &mbox->u.mqe.un.set_feature,
		       phba->pport->cfg_lun_queue_depth);
		bf_set(lpfc_mbx_set_feature_mi, &mbox->u.mqe.un.set_feature,
		       phba->sli4_hba.pc_sli4_params.mi_ver);
		break;
	case LPFC_SET_LD_SIGNAL:
		mbox->u.mqe.un.set_feature.feature = LPFC_SET_LD_SIGNAL;
		mbox->u.mqe.un.set_feature.param_len = 16;
		bf_set(lpfc_mbx_set_feature_lds_qry,
		       &mbox->u.mqe.un.set_feature, LPFC_QUERY_LDS_OP);
		break;
	case LPFC_SET_ENABLE_CMF:
		mbox->u.mqe.un.set_feature.feature = LPFC_SET_ENABLE_CMF;
		mbox->u.mqe.un.set_feature.param_len = 4;
		bf_set(lpfc_mbx_set_feature_cmf,
		       &mbox->u.mqe.un.set_feature, 1);
		break;
	}
	return;
}

 
void
lpfc_ras_stop_fwlog(struct lpfc_hba *phba)
{
	struct lpfc_ras_fwlog *ras_fwlog = &phba->ras_fwlog;

	spin_lock_irq(&phba->hbalock);
	ras_fwlog->state = INACTIVE;
	spin_unlock_irq(&phba->hbalock);

	 
	writel(LPFC_CTL_PDEV_CTL_DDL_RAS,
	       phba->sli4_hba.conf_regs_memmap_p + LPFC_CTL_PDEV_CTL_OFFSET);

	 
	usleep_range(10 * 1000, 20 * 1000);
}

 
void
lpfc_sli4_ras_dma_free(struct lpfc_hba *phba)
{
	struct lpfc_ras_fwlog *ras_fwlog = &phba->ras_fwlog;
	struct lpfc_dmabuf *dmabuf, *next;

	if (!list_empty(&ras_fwlog->fwlog_buff_list)) {
		list_for_each_entry_safe(dmabuf, next,
				    &ras_fwlog->fwlog_buff_list,
				    list) {
			list_del(&dmabuf->list);
			dma_free_coherent(&phba->pcidev->dev,
					  LPFC_RAS_MAX_ENTRY_SIZE,
					  dmabuf->virt, dmabuf->phys);
			kfree(dmabuf);
		}
	}

	if (ras_fwlog->lwpd.virt) {
		dma_free_coherent(&phba->pcidev->dev,
				  sizeof(uint32_t) * 2,
				  ras_fwlog->lwpd.virt,
				  ras_fwlog->lwpd.phys);
		ras_fwlog->lwpd.virt = NULL;
	}

	spin_lock_irq(&phba->hbalock);
	ras_fwlog->state = INACTIVE;
	spin_unlock_irq(&phba->hbalock);
}

 

static int
lpfc_sli4_ras_dma_alloc(struct lpfc_hba *phba,
			uint32_t fwlog_buff_count)
{
	struct lpfc_ras_fwlog *ras_fwlog = &phba->ras_fwlog;
	struct lpfc_dmabuf *dmabuf;
	int rc = 0, i = 0;

	 
	INIT_LIST_HEAD(&ras_fwlog->fwlog_buff_list);

	 
	ras_fwlog->lwpd.virt = dma_alloc_coherent(&phba->pcidev->dev,
					    sizeof(uint32_t) * 2,
					    &ras_fwlog->lwpd.phys,
					    GFP_KERNEL);
	if (!ras_fwlog->lwpd.virt) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"6185 LWPD Memory Alloc Failed\n");

		return -ENOMEM;
	}

	ras_fwlog->fw_buffcount = fwlog_buff_count;
	for (i = 0; i < ras_fwlog->fw_buffcount; i++) {
		dmabuf = kzalloc(sizeof(struct lpfc_dmabuf),
				 GFP_KERNEL);
		if (!dmabuf) {
			rc = -ENOMEM;
			lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
					"6186 Memory Alloc failed FW logging");
			goto free_mem;
		}

		dmabuf->virt = dma_alloc_coherent(&phba->pcidev->dev,
						  LPFC_RAS_MAX_ENTRY_SIZE,
						  &dmabuf->phys, GFP_KERNEL);
		if (!dmabuf->virt) {
			kfree(dmabuf);
			rc = -ENOMEM;
			lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
					"6187 DMA Alloc Failed FW logging");
			goto free_mem;
		}
		dmabuf->buffer_tag = i;
		list_add_tail(&dmabuf->list, &ras_fwlog->fwlog_buff_list);
	}

free_mem:
	if (rc)
		lpfc_sli4_ras_dma_free(phba);

	return rc;
}

 
static void
lpfc_sli4_ras_mbox_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	MAILBOX_t *mb;
	union lpfc_sli4_cfg_shdr *shdr;
	uint32_t shdr_status, shdr_add_status;
	struct lpfc_ras_fwlog *ras_fwlog = &phba->ras_fwlog;

	mb = &pmb->u.mb;

	shdr = (union lpfc_sli4_cfg_shdr *)
		&pmb->u.mqe.un.ras_fwlog.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);

	if (mb->mbxStatus != MBX_SUCCESS || shdr_status) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"6188 FW LOG mailbox "
				"completed with status x%x add_status x%x,"
				" mbx status x%x\n",
				shdr_status, shdr_add_status, mb->mbxStatus);

		ras_fwlog->ras_hwsupport = false;
		goto disable_ras;
	}

	spin_lock_irq(&phba->hbalock);
	ras_fwlog->state = ACTIVE;
	spin_unlock_irq(&phba->hbalock);
	mempool_free(pmb, phba->mbox_mem_pool);

	return;

disable_ras:
	 
	lpfc_sli4_ras_dma_free(phba);
	mempool_free(pmb, phba->mbox_mem_pool);
}

 
int
lpfc_sli4_ras_fwlog_init(struct lpfc_hba *phba,
			 uint32_t fwlog_level,
			 uint32_t fwlog_enable)
{
	struct lpfc_ras_fwlog *ras_fwlog = &phba->ras_fwlog;
	struct lpfc_mbx_set_ras_fwlog *mbx_fwlog = NULL;
	struct lpfc_dmabuf *dmabuf;
	LPFC_MBOXQ_t *mbox;
	uint32_t len = 0, fwlog_buffsize, fwlog_entry_count;
	int rc = 0;

	spin_lock_irq(&phba->hbalock);
	ras_fwlog->state = INACTIVE;
	spin_unlock_irq(&phba->hbalock);

	fwlog_buffsize = (LPFC_RAS_MIN_BUFF_POST_SIZE *
			  phba->cfg_ras_fwlog_buffsize);
	fwlog_entry_count = (fwlog_buffsize/LPFC_RAS_MAX_ENTRY_SIZE);

	 
	if (!ras_fwlog->lwpd.virt) {
		rc = lpfc_sli4_ras_dma_alloc(phba, fwlog_entry_count);
		if (rc) {
			lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
					"6189 FW Log Memory Allocation Failed");
			return rc;
		}
	}

	 
	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"6190 RAS MBX Alloc Failed");
		rc = -ENOMEM;
		goto mem_free;
	}

	ras_fwlog->fw_loglevel = fwlog_level;
	len = (sizeof(struct lpfc_mbx_set_ras_fwlog) -
		sizeof(struct lpfc_sli4_cfg_mhdr));

	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_LOWLEVEL,
			 LPFC_MBOX_OPCODE_SET_DIAG_LOG_OPTION,
			 len, LPFC_SLI4_MBX_EMBED);

	mbx_fwlog = (struct lpfc_mbx_set_ras_fwlog *)&mbox->u.mqe.un.ras_fwlog;
	bf_set(lpfc_fwlog_enable, &mbx_fwlog->u.request,
	       fwlog_enable);
	bf_set(lpfc_fwlog_loglvl, &mbx_fwlog->u.request,
	       ras_fwlog->fw_loglevel);
	bf_set(lpfc_fwlog_buffcnt, &mbx_fwlog->u.request,
	       ras_fwlog->fw_buffcount);
	bf_set(lpfc_fwlog_buffsz, &mbx_fwlog->u.request,
	       LPFC_RAS_MAX_ENTRY_SIZE/SLI4_PAGE_SIZE);

	 
	list_for_each_entry(dmabuf, &ras_fwlog->fwlog_buff_list, list) {
		memset(dmabuf->virt, 0, LPFC_RAS_MAX_ENTRY_SIZE);

		mbx_fwlog->u.request.buff_fwlog[dmabuf->buffer_tag].addr_lo =
			putPaddrLow(dmabuf->phys);

		mbx_fwlog->u.request.buff_fwlog[dmabuf->buffer_tag].addr_hi =
			putPaddrHigh(dmabuf->phys);
	}

	 
	mbx_fwlog->u.request.lwpd.addr_lo = putPaddrLow(ras_fwlog->lwpd.phys);
	mbx_fwlog->u.request.lwpd.addr_hi = putPaddrHigh(ras_fwlog->lwpd.phys);

	spin_lock_irq(&phba->hbalock);
	ras_fwlog->state = REG_INPROGRESS;
	spin_unlock_irq(&phba->hbalock);
	mbox->vport = phba->pport;
	mbox->mbox_cmpl = lpfc_sli4_ras_mbox_cmpl;

	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);

	if (rc == MBX_NOT_FINISHED) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"6191 FW-Log Mailbox failed. "
				"status %d mbxStatus : x%x", rc,
				bf_get(lpfc_mqe_status, &mbox->u.mqe));
		mempool_free(mbox, phba->mbox_mem_pool);
		rc = -EIO;
		goto mem_free;
	} else
		rc = 0;
mem_free:
	if (rc)
		lpfc_sli4_ras_dma_free(phba);

	return rc;
}

 
void
lpfc_sli4_ras_setup(struct lpfc_hba *phba)
{
	 
	if (lpfc_check_fwlog_support(phba))
		return;

	lpfc_sli4_ras_fwlog_init(phba, phba->cfg_ras_fwlog_level,
				 LPFC_RAS_ENABLE_LOGGING);
}

 
int
lpfc_sli4_alloc_resource_identifiers(struct lpfc_hba *phba)
{
	int i, rc, error = 0;
	uint16_t count, base;
	unsigned long longs;

	if (!phba->sli4_hba.rpi_hdrs_in_use)
		phba->sli4_hba.next_rpi = phba->sli4_hba.max_cfg_param.max_rpi;
	if (phba->sli4_hba.extents_in_use) {
		 
		if (bf_get(lpfc_idx_rsrc_rdy, &phba->sli4_hba.sli4_flags) ==
		    LPFC_IDX_RSRC_RDY) {
			 
			rc = lpfc_sli4_chk_avail_extnt_rsrc(phba,
						 LPFC_RSC_TYPE_FCOE_VFI);
			if (rc != 0)
				error++;
			rc = lpfc_sli4_chk_avail_extnt_rsrc(phba,
						 LPFC_RSC_TYPE_FCOE_VPI);
			if (rc != 0)
				error++;
			rc = lpfc_sli4_chk_avail_extnt_rsrc(phba,
						 LPFC_RSC_TYPE_FCOE_XRI);
			if (rc != 0)
				error++;
			rc = lpfc_sli4_chk_avail_extnt_rsrc(phba,
						 LPFC_RSC_TYPE_FCOE_RPI);
			if (rc != 0)
				error++;

			 
			if (error) {
				lpfc_printf_log(phba, KERN_INFO,
						LOG_MBOX | LOG_INIT,
						"2931 Detected extent resource "
						"change.  Reallocating all "
						"extents.\n");
				rc = lpfc_sli4_dealloc_extent(phba,
						 LPFC_RSC_TYPE_FCOE_VFI);
				rc = lpfc_sli4_dealloc_extent(phba,
						 LPFC_RSC_TYPE_FCOE_VPI);
				rc = lpfc_sli4_dealloc_extent(phba,
						 LPFC_RSC_TYPE_FCOE_XRI);
				rc = lpfc_sli4_dealloc_extent(phba,
						 LPFC_RSC_TYPE_FCOE_RPI);
			} else
				return 0;
		}

		rc = lpfc_sli4_alloc_extent(phba, LPFC_RSC_TYPE_FCOE_VFI);
		if (unlikely(rc))
			goto err_exit;

		rc = lpfc_sli4_alloc_extent(phba, LPFC_RSC_TYPE_FCOE_VPI);
		if (unlikely(rc))
			goto err_exit;

		rc = lpfc_sli4_alloc_extent(phba, LPFC_RSC_TYPE_FCOE_RPI);
		if (unlikely(rc))
			goto err_exit;

		rc = lpfc_sli4_alloc_extent(phba, LPFC_RSC_TYPE_FCOE_XRI);
		if (unlikely(rc))
			goto err_exit;
		bf_set(lpfc_idx_rsrc_rdy, &phba->sli4_hba.sli4_flags,
		       LPFC_IDX_RSRC_RDY);
		return rc;
	} else {
		 
		if (bf_get(lpfc_idx_rsrc_rdy, &phba->sli4_hba.sli4_flags) ==
		    LPFC_IDX_RSRC_RDY) {
			lpfc_sli4_dealloc_resource_identifiers(phba);
			lpfc_sli4_remove_rpis(phba);
		}
		 
		count = phba->sli4_hba.max_cfg_param.max_rpi;
		if (count <= 0) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"3279 Invalid provisioning of "
					"rpi:%d\n", count);
			rc = -EINVAL;
			goto err_exit;
		}
		base = phba->sli4_hba.max_cfg_param.rpi_base;
		longs = (count + BITS_PER_LONG - 1) / BITS_PER_LONG;
		phba->sli4_hba.rpi_bmask = kcalloc(longs,
						   sizeof(unsigned long),
						   GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.rpi_bmask)) {
			rc = -ENOMEM;
			goto err_exit;
		}
		phba->sli4_hba.rpi_ids = kcalloc(count, sizeof(uint16_t),
						 GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.rpi_ids)) {
			rc = -ENOMEM;
			goto free_rpi_bmask;
		}

		for (i = 0; i < count; i++)
			phba->sli4_hba.rpi_ids[i] = base + i;

		 
		count = phba->sli4_hba.max_cfg_param.max_vpi;
		if (count <= 0) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"3280 Invalid provisioning of "
					"vpi:%d\n", count);
			rc = -EINVAL;
			goto free_rpi_ids;
		}
		base = phba->sli4_hba.max_cfg_param.vpi_base;
		longs = (count + BITS_PER_LONG - 1) / BITS_PER_LONG;
		phba->vpi_bmask = kcalloc(longs, sizeof(unsigned long),
					  GFP_KERNEL);
		if (unlikely(!phba->vpi_bmask)) {
			rc = -ENOMEM;
			goto free_rpi_ids;
		}
		phba->vpi_ids = kcalloc(count, sizeof(uint16_t),
					GFP_KERNEL);
		if (unlikely(!phba->vpi_ids)) {
			rc = -ENOMEM;
			goto free_vpi_bmask;
		}

		for (i = 0; i < count; i++)
			phba->vpi_ids[i] = base + i;

		 
		count = phba->sli4_hba.max_cfg_param.max_xri;
		if (count <= 0) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"3281 Invalid provisioning of "
					"xri:%d\n", count);
			rc = -EINVAL;
			goto free_vpi_ids;
		}
		base = phba->sli4_hba.max_cfg_param.xri_base;
		longs = (count + BITS_PER_LONG - 1) / BITS_PER_LONG;
		phba->sli4_hba.xri_bmask = kcalloc(longs,
						   sizeof(unsigned long),
						   GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.xri_bmask)) {
			rc = -ENOMEM;
			goto free_vpi_ids;
		}
		phba->sli4_hba.max_cfg_param.xri_used = 0;
		phba->sli4_hba.xri_ids = kcalloc(count, sizeof(uint16_t),
						 GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.xri_ids)) {
			rc = -ENOMEM;
			goto free_xri_bmask;
		}

		for (i = 0; i < count; i++)
			phba->sli4_hba.xri_ids[i] = base + i;

		 
		count = phba->sli4_hba.max_cfg_param.max_vfi;
		if (count <= 0) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"3282 Invalid provisioning of "
					"vfi:%d\n", count);
			rc = -EINVAL;
			goto free_xri_ids;
		}
		base = phba->sli4_hba.max_cfg_param.vfi_base;
		longs = (count + BITS_PER_LONG - 1) / BITS_PER_LONG;
		phba->sli4_hba.vfi_bmask = kcalloc(longs,
						   sizeof(unsigned long),
						   GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.vfi_bmask)) {
			rc = -ENOMEM;
			goto free_xri_ids;
		}
		phba->sli4_hba.vfi_ids = kcalloc(count, sizeof(uint16_t),
						 GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.vfi_ids)) {
			rc = -ENOMEM;
			goto free_vfi_bmask;
		}

		for (i = 0; i < count; i++)
			phba->sli4_hba.vfi_ids[i] = base + i;

		 
		bf_set(lpfc_idx_rsrc_rdy, &phba->sli4_hba.sli4_flags,
		       LPFC_IDX_RSRC_RDY);
		return 0;
	}

 free_vfi_bmask:
	kfree(phba->sli4_hba.vfi_bmask);
	phba->sli4_hba.vfi_bmask = NULL;
 free_xri_ids:
	kfree(phba->sli4_hba.xri_ids);
	phba->sli4_hba.xri_ids = NULL;
 free_xri_bmask:
	kfree(phba->sli4_hba.xri_bmask);
	phba->sli4_hba.xri_bmask = NULL;
 free_vpi_ids:
	kfree(phba->vpi_ids);
	phba->vpi_ids = NULL;
 free_vpi_bmask:
	kfree(phba->vpi_bmask);
	phba->vpi_bmask = NULL;
 free_rpi_ids:
	kfree(phba->sli4_hba.rpi_ids);
	phba->sli4_hba.rpi_ids = NULL;
 free_rpi_bmask:
	kfree(phba->sli4_hba.rpi_bmask);
	phba->sli4_hba.rpi_bmask = NULL;
 err_exit:
	return rc;
}

 
int
lpfc_sli4_dealloc_resource_identifiers(struct lpfc_hba *phba)
{
	if (phba->sli4_hba.extents_in_use) {
		lpfc_sli4_dealloc_extent(phba, LPFC_RSC_TYPE_FCOE_VPI);
		lpfc_sli4_dealloc_extent(phba, LPFC_RSC_TYPE_FCOE_RPI);
		lpfc_sli4_dealloc_extent(phba, LPFC_RSC_TYPE_FCOE_XRI);
		lpfc_sli4_dealloc_extent(phba, LPFC_RSC_TYPE_FCOE_VFI);
	} else {
		kfree(phba->vpi_bmask);
		phba->sli4_hba.max_cfg_param.vpi_used = 0;
		kfree(phba->vpi_ids);
		bf_set(lpfc_vpi_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);
		kfree(phba->sli4_hba.xri_bmask);
		kfree(phba->sli4_hba.xri_ids);
		kfree(phba->sli4_hba.vfi_bmask);
		kfree(phba->sli4_hba.vfi_ids);
		bf_set(lpfc_vfi_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);
		bf_set(lpfc_idx_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);
	}

	return 0;
}

 
int
lpfc_sli4_get_allocated_extnts(struct lpfc_hba *phba, uint16_t type,
			       uint16_t *extnt_cnt, uint16_t *extnt_size)
{
	bool emb;
	int rc = 0;
	uint16_t curr_blks = 0;
	uint32_t req_len, emb_len;
	uint32_t alloc_len, mbox_tmo;
	struct list_head *blk_list_head;
	struct lpfc_rsrc_blks *rsrc_blk;
	LPFC_MBOXQ_t *mbox;
	void *virtaddr = NULL;
	struct lpfc_mbx_nembed_rsrc_extent *n_rsrc;
	struct lpfc_mbx_alloc_rsrc_extents *rsrc_ext;
	union  lpfc_sli4_cfg_shdr *shdr;

	switch (type) {
	case LPFC_RSC_TYPE_FCOE_VPI:
		blk_list_head = &phba->lpfc_vpi_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_XRI:
		blk_list_head = &phba->sli4_hba.lpfc_xri_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_VFI:
		blk_list_head = &phba->sli4_hba.lpfc_vfi_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_RPI:
		blk_list_head = &phba->sli4_hba.lpfc_rpi_blk_list;
		break;
	default:
		return -EIO;
	}

	 
	list_for_each_entry(rsrc_blk, blk_list_head, list) {
		if (curr_blks == 0) {
			 
			*extnt_size = rsrc_blk->rsrc_size;
		}
		curr_blks++;
	}

	 
	emb_len = sizeof(MAILBOX_t) - sizeof(struct mbox_header) -
		sizeof(uint32_t);

	 
	emb = LPFC_SLI4_MBX_EMBED;
	req_len = emb_len;
	if (req_len > emb_len) {
		req_len = curr_blks * sizeof(uint16_t) +
			sizeof(union lpfc_sli4_cfg_shdr) +
			sizeof(uint32_t);
		emb = LPFC_SLI4_MBX_NEMBED;
	}

	mbox = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	memset(mbox, 0, sizeof(LPFC_MBOXQ_t));

	alloc_len = lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
				     LPFC_MBOX_OPCODE_GET_ALLOC_RSRC_EXTENT,
				     req_len, emb);
	if (alloc_len < req_len) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"2983 Allocated DMA memory size (x%x) is "
			"less than the requested DMA memory "
			"size (x%x)\n", alloc_len, req_len);
		rc = -ENOMEM;
		goto err_exit;
	}
	rc = lpfc_sli4_mbox_rsrc_extent(phba, mbox, curr_blks, type, emb);
	if (unlikely(rc)) {
		rc = -EIO;
		goto err_exit;
	}

	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}

	if (unlikely(rc)) {
		rc = -EIO;
		goto err_exit;
	}

	 
	if (emb == LPFC_SLI4_MBX_EMBED) {
		rsrc_ext = &mbox->u.mqe.un.alloc_rsrc_extents;
		shdr = &rsrc_ext->header.cfg_shdr;
		*extnt_cnt = bf_get(lpfc_mbx_rsrc_cnt, &rsrc_ext->u.rsp);
	} else {
		virtaddr = mbox->sge_array->addr[0];
		n_rsrc = (struct lpfc_mbx_nembed_rsrc_extent *) virtaddr;
		shdr = &n_rsrc->cfg_shdr;
		*extnt_cnt = bf_get(lpfc_mbx_rsrc_cnt, n_rsrc);
	}

	if (bf_get(lpfc_mbox_hdr_status, &shdr->response)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"2984 Failed to read allocated resources "
			"for type %d - Status 0x%x Add'l Status 0x%x.\n",
			type,
			bf_get(lpfc_mbox_hdr_status, &shdr->response),
			bf_get(lpfc_mbox_hdr_add_status, &shdr->response));
		rc = -EIO;
		goto err_exit;
	}
 err_exit:
	lpfc_sli4_mbox_cmd_free(phba, mbox);
	return rc;
}

 
static int
lpfc_sli4_repost_sgl_list(struct lpfc_hba *phba,
			  struct list_head *sgl_list, int cnt)
{
	struct lpfc_sglq *sglq_entry = NULL;
	struct lpfc_sglq *sglq_entry_next = NULL;
	struct lpfc_sglq *sglq_entry_first = NULL;
	int status, total_cnt;
	int post_cnt = 0, num_posted = 0, block_cnt = 0;
	int last_xritag = NO_XRI;
	LIST_HEAD(prep_sgl_list);
	LIST_HEAD(blck_sgl_list);
	LIST_HEAD(allc_sgl_list);
	LIST_HEAD(post_sgl_list);
	LIST_HEAD(free_sgl_list);

	spin_lock_irq(&phba->hbalock);
	spin_lock(&phba->sli4_hba.sgl_list_lock);
	list_splice_init(sgl_list, &allc_sgl_list);
	spin_unlock(&phba->sli4_hba.sgl_list_lock);
	spin_unlock_irq(&phba->hbalock);

	total_cnt = cnt;
	list_for_each_entry_safe(sglq_entry, sglq_entry_next,
				 &allc_sgl_list, list) {
		list_del_init(&sglq_entry->list);
		block_cnt++;
		if ((last_xritag != NO_XRI) &&
		    (sglq_entry->sli4_xritag != last_xritag + 1)) {
			 
			list_splice_init(&prep_sgl_list, &blck_sgl_list);
			post_cnt = block_cnt - 1;
			 
			list_add_tail(&sglq_entry->list, &prep_sgl_list);
			block_cnt = 1;
		} else {
			 
			list_add_tail(&sglq_entry->list, &prep_sgl_list);
			 
			if (block_cnt == LPFC_NEMBED_MBOX_SGL_CNT) {
				list_splice_init(&prep_sgl_list,
						 &blck_sgl_list);
				post_cnt = block_cnt;
				block_cnt = 0;
			}
		}
		num_posted++;

		 
		last_xritag = sglq_entry->sli4_xritag;

		 
		if (num_posted == total_cnt) {
			if (post_cnt == 0) {
				list_splice_init(&prep_sgl_list,
						 &blck_sgl_list);
				post_cnt = block_cnt;
			} else if (block_cnt == 1) {
				status = lpfc_sli4_post_sgl(phba,
						sglq_entry->phys, 0,
						sglq_entry->sli4_xritag);
				if (!status) {
					 
					list_add_tail(&sglq_entry->list,
						      &post_sgl_list);
				} else {
					 
					lpfc_printf_log(phba, KERN_WARNING,
						LOG_SLI,
						"3159 Failed to post "
						"sgl, xritag:x%x\n",
						sglq_entry->sli4_xritag);
					list_add_tail(&sglq_entry->list,
						      &free_sgl_list);
					total_cnt--;
				}
			}
		}

		 
		if (post_cnt == 0)
			continue;

		 
		status = lpfc_sli4_post_sgl_list(phba, &blck_sgl_list,
						 post_cnt);

		if (!status) {
			 
			list_splice_init(&blck_sgl_list, &post_sgl_list);
		} else {
			 
			sglq_entry_first = list_first_entry(&blck_sgl_list,
							    struct lpfc_sglq,
							    list);
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					"3160 Failed to post sgl-list, "
					"xritag:x%x-x%x\n",
					sglq_entry_first->sli4_xritag,
					(sglq_entry_first->sli4_xritag +
					 post_cnt - 1));
			list_splice_init(&blck_sgl_list, &free_sgl_list);
			total_cnt -= post_cnt;
		}

		 
		if (block_cnt == 0)
			last_xritag = NO_XRI;

		 
		post_cnt = 0;
	}

	 
	lpfc_free_sgl_list(phba, &free_sgl_list);

	 
	if (!list_empty(&post_sgl_list)) {
		spin_lock_irq(&phba->hbalock);
		spin_lock(&phba->sli4_hba.sgl_list_lock);
		list_splice_init(&post_sgl_list, sgl_list);
		spin_unlock(&phba->sli4_hba.sgl_list_lock);
		spin_unlock_irq(&phba->hbalock);
	} else {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3161 Failure to post sgl to port,status %x "
				"blkcnt %d totalcnt %d postcnt %d\n",
				status, block_cnt, total_cnt, post_cnt);
		return -EIO;
	}

	 
	return total_cnt;
}

 
static int
lpfc_sli4_repost_io_sgl_list(struct lpfc_hba *phba)
{
	LIST_HEAD(post_nblist);
	int num_posted, rc = 0;

	 
	lpfc_io_buf_flush(phba, &post_nblist);

	 
	if (!list_empty(&post_nblist)) {
		num_posted = lpfc_sli4_post_io_sgl_list(
			phba, &post_nblist, phba->sli4_hba.io_xri_cnt);
		 
		if (num_posted == 0)
			rc = -EIO;
	}
	return rc;
}

static void
lpfc_set_host_data(struct lpfc_hba *phba, LPFC_MBOXQ_t *mbox)
{
	uint32_t len;

	len = sizeof(struct lpfc_mbx_set_host_data) -
		sizeof(struct lpfc_sli4_cfg_mhdr);
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_SET_HOST_DATA, len,
			 LPFC_SLI4_MBX_EMBED);

	mbox->u.mqe.un.set_host_data.param_id = LPFC_SET_HOST_OS_DRIVER_VERSION;
	mbox->u.mqe.un.set_host_data.param_len =
					LPFC_HOST_OS_DRIVER_VERSION_SIZE;
	snprintf(mbox->u.mqe.un.set_host_data.un.data,
		 LPFC_HOST_OS_DRIVER_VERSION_SIZE,
		 "Linux %s v"LPFC_DRIVER_VERSION,
		 (phba->hba_flag & HBA_FCOE_MODE) ? "FCoE" : "FC");
}

int
lpfc_post_rq_buffer(struct lpfc_hba *phba, struct lpfc_queue *hrq,
		    struct lpfc_queue *drq, int count, int idx)
{
	int rc, i;
	struct lpfc_rqe hrqe;
	struct lpfc_rqe drqe;
	struct lpfc_rqb *rqbp;
	unsigned long flags;
	struct rqb_dmabuf *rqb_buffer;
	LIST_HEAD(rqb_buf_list);

	rqbp = hrq->rqbp;
	for (i = 0; i < count; i++) {
		spin_lock_irqsave(&phba->hbalock, flags);
		 
		if (rqbp->buffer_count + i >= rqbp->entry_count - 1) {
			spin_unlock_irqrestore(&phba->hbalock, flags);
			break;
		}
		spin_unlock_irqrestore(&phba->hbalock, flags);

		rqb_buffer = rqbp->rqb_alloc_buffer(phba);
		if (!rqb_buffer)
			break;
		rqb_buffer->hrq = hrq;
		rqb_buffer->drq = drq;
		rqb_buffer->idx = idx;
		list_add_tail(&rqb_buffer->hbuf.list, &rqb_buf_list);
	}

	spin_lock_irqsave(&phba->hbalock, flags);
	while (!list_empty(&rqb_buf_list)) {
		list_remove_head(&rqb_buf_list, rqb_buffer, struct rqb_dmabuf,
				 hbuf.list);

		hrqe.address_lo = putPaddrLow(rqb_buffer->hbuf.phys);
		hrqe.address_hi = putPaddrHigh(rqb_buffer->hbuf.phys);
		drqe.address_lo = putPaddrLow(rqb_buffer->dbuf.phys);
		drqe.address_hi = putPaddrHigh(rqb_buffer->dbuf.phys);
		rc = lpfc_sli4_rq_put(hrq, drq, &hrqe, &drqe);
		if (rc < 0) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"6421 Cannot post to HRQ %d: %x %x %x "
					"DRQ %x %x\n",
					hrq->queue_id,
					hrq->host_index,
					hrq->hba_index,
					hrq->entry_count,
					drq->host_index,
					drq->hba_index);
			rqbp->rqb_free_buffer(phba, rqb_buffer);
		} else {
			list_add_tail(&rqb_buffer->hbuf.list,
				      &rqbp->rqb_buffer_list);
			rqbp->buffer_count++;
		}
	}
	spin_unlock_irqrestore(&phba->hbalock, flags);
	return 1;
}

static void
lpfc_mbx_cmpl_read_lds_params(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	union lpfc_sli4_cfg_shdr *shdr;
	u32 shdr_status, shdr_add_status;

	shdr = (union lpfc_sli4_cfg_shdr *)
		&pmb->u.mqe.un.sli4_config.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || pmb->u.mb.mbxStatus) {
		lpfc_printf_log(phba, KERN_INFO, LOG_LDS_EVENT | LOG_MBOX,
				"4622 SET_FEATURE (x%x) mbox failed, "
				"status x%x add_status x%x, mbx status x%x\n",
				LPFC_SET_LD_SIGNAL, shdr_status,
				shdr_add_status, pmb->u.mb.mbxStatus);
		phba->degrade_activate_threshold = 0;
		phba->degrade_deactivate_threshold = 0;
		phba->fec_degrade_interval = 0;
		goto out;
	}

	phba->degrade_activate_threshold = pmb->u.mqe.un.set_feature.word7;
	phba->degrade_deactivate_threshold = pmb->u.mqe.un.set_feature.word8;
	phba->fec_degrade_interval = pmb->u.mqe.un.set_feature.word10;

	lpfc_printf_log(phba, KERN_INFO, LOG_LDS_EVENT,
			"4624 Success: da x%x dd x%x interval x%x\n",
			phba->degrade_activate_threshold,
			phba->degrade_deactivate_threshold,
			phba->fec_degrade_interval);
out:
	mempool_free(pmb, phba->mbox_mem_pool);
}

int
lpfc_read_lds_params(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mboxq;
	int rc;

	mboxq = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		return -ENOMEM;

	lpfc_set_features(phba, mboxq, LPFC_SET_LD_SIGNAL);
	mboxq->vport = phba->pport;
	mboxq->mbox_cmpl = lpfc_mbx_cmpl_read_lds_params;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		mempool_free(mboxq, phba->mbox_mem_pool);
		return -EIO;
	}
	return 0;
}

static void
lpfc_mbx_cmpl_cgn_set_ftrs(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport *vport = pmb->vport;
	union lpfc_sli4_cfg_shdr *shdr;
	u32 shdr_status, shdr_add_status;
	u32 sig, acqe;

	 
	shdr = (union lpfc_sli4_cfg_shdr *)
		&pmb->u.mqe.un.sli4_config.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || pmb->u.mb.mbxStatus) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT | LOG_CGN_MGMT,
				"2516 CGN SET_FEATURE mbox failed with "
				"status x%x add_status x%x, mbx status x%x "
				"Reset Congestion to FPINs only\n",
				shdr_status, shdr_add_status,
				pmb->u.mb.mbxStatus);
		 
		phba->cgn_reg_signal = EDC_CG_SIG_NOTSUPPORTED;
		phba->cgn_reg_fpin = LPFC_CGN_FPIN_WARN | LPFC_CGN_FPIN_ALARM;
		goto out;
	}

	 
	phba->cgn_acqe_cnt = 0;

	acqe = bf_get(lpfc_mbx_set_feature_CGN_acqe_freq,
		      &pmb->u.mqe.un.set_feature);
	sig = bf_get(lpfc_mbx_set_feature_CGN_warn_freq,
		     &pmb->u.mqe.un.set_feature);
	lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
			"4620 SET_FEATURES Success: Freq: %ds %dms "
			" Reg: x%x x%x\n", acqe, sig,
			phba->cgn_reg_signal, phba->cgn_reg_fpin);
out:
	mempool_free(pmb, phba->mbox_mem_pool);

	 
	lpfc_issue_els_rdf(vport, 0);
}

int
lpfc_config_cgn_signal(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mboxq;
	u32 rc;

	mboxq = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		goto out_rdf;

	lpfc_set_features(phba, mboxq, LPFC_SET_CGN_SIGNAL);
	mboxq->vport = phba->pport;
	mboxq->mbox_cmpl = lpfc_mbx_cmpl_cgn_set_ftrs;

	lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
			"4621 SET_FEATURES: FREQ sig x%x acqe x%x: "
			"Reg: x%x x%x\n",
			phba->cgn_sig_freq, lpfc_acqe_cgn_frequency,
			phba->cgn_reg_signal, phba->cgn_reg_fpin);

	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED)
		goto out;
	return 0;

out:
	mempool_free(mboxq, phba->mbox_mem_pool);
out_rdf:
	 
	phba->cgn_reg_fpin = LPFC_CGN_FPIN_WARN | LPFC_CGN_FPIN_ALARM;
	phba->cgn_reg_signal = EDC_CG_SIG_NOTSUPPORTED;
	lpfc_issue_els_rdf(phba->pport, 0);
	return -EIO;
}

 
static void lpfc_init_idle_stat_hb(struct lpfc_hba *phba)
{
	int i;
	struct lpfc_sli4_hdw_queue *hdwq;
	struct lpfc_queue *eq;
	struct lpfc_idle_stat *idle_stat;
	u64 wall;

	for_each_present_cpu(i) {
		hdwq = &phba->sli4_hba.hdwq[phba->sli4_hba.cpu_map[i].hdwq];
		eq = hdwq->hba_eq;

		 
		if (eq->chann != i)
			continue;

		idle_stat = &phba->sli4_hba.idle_stat[i];

		idle_stat->prev_idle = get_cpu_idle_time(i, &wall, 1);
		idle_stat->prev_wall = wall;

		if (phba->nvmet_support ||
		    phba->cmf_active_mode != LPFC_CFG_OFF ||
		    phba->intr_type != MSIX)
			eq->poll_mode = LPFC_QUEUE_WORK;
		else
			eq->poll_mode = LPFC_THREADED_IRQ;
	}

	if (!phba->nvmet_support && phba->intr_type == MSIX)
		schedule_delayed_work(&phba->idle_stat_delay_work,
				      msecs_to_jiffies(LPFC_IDLE_STAT_DELAY));
}

static void lpfc_sli4_dip(struct lpfc_hba *phba)
{
	uint32_t if_type;

	if_type = bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf);
	if (if_type == LPFC_SLI_INTF_IF_TYPE_2 ||
	    if_type == LPFC_SLI_INTF_IF_TYPE_6) {
		struct lpfc_register reg_data;

		if (lpfc_readl(phba->sli4_hba.u.if_type2.STATUSregaddr,
			       &reg_data.word0))
			return;

		if (bf_get(lpfc_sliport_status_dip, &reg_data))
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"2904 Firmware Dump Image Present"
					" on Adapter");
	}
}

 
int lpfc_rx_monitor_create_ring(struct lpfc_rx_info_monitor *rx_monitor,
				u32 entries)
{
	rx_monitor->ring = kmalloc_array(entries, sizeof(struct rx_info_entry),
					 GFP_KERNEL);
	if (!rx_monitor->ring)
		return -ENOMEM;

	rx_monitor->head_idx = 0;
	rx_monitor->tail_idx = 0;
	spin_lock_init(&rx_monitor->lock);
	rx_monitor->entries = entries;

	return 0;
}

 
void lpfc_rx_monitor_destroy_ring(struct lpfc_rx_info_monitor *rx_monitor)
{
	kfree(rx_monitor->ring);
	rx_monitor->ring = NULL;
	rx_monitor->entries = 0;
	rx_monitor->head_idx = 0;
	rx_monitor->tail_idx = 0;
}

 
void lpfc_rx_monitor_record(struct lpfc_rx_info_monitor *rx_monitor,
			    struct rx_info_entry *entry)
{
	struct rx_info_entry *ring = rx_monitor->ring;
	u32 *head_idx = &rx_monitor->head_idx;
	u32 *tail_idx = &rx_monitor->tail_idx;
	spinlock_t *ring_lock = &rx_monitor->lock;
	u32 ring_size = rx_monitor->entries;

	spin_lock(ring_lock);
	memcpy(&ring[*tail_idx], entry, sizeof(*entry));
	*tail_idx = (*tail_idx + 1) % ring_size;

	 
	if (*tail_idx == *head_idx)
		*head_idx = (*head_idx + 1) % ring_size;

	spin_unlock(ring_lock);
}

 
u32 lpfc_rx_monitor_report(struct lpfc_hba *phba,
			   struct lpfc_rx_info_monitor *rx_monitor, char *buf,
			   u32 buf_len, u32 max_read_entries)
{
	struct rx_info_entry *ring = rx_monitor->ring;
	struct rx_info_entry *entry;
	u32 *head_idx = &rx_monitor->head_idx;
	u32 *tail_idx = &rx_monitor->tail_idx;
	spinlock_t *ring_lock = &rx_monitor->lock;
	u32 ring_size = rx_monitor->entries;
	u32 cnt = 0;
	char tmp[DBG_LOG_STR_SZ] = {0};
	bool log_to_kmsg = (!buf || !buf_len) ? true : false;

	if (!log_to_kmsg) {
		 
		memset(buf, 0, buf_len);

		scnprintf(buf, buf_len, "\t%-16s%-16s%-16s%-16s%-8s%-8s%-8s"
					"%-8s%-8s%-8s%-16s\n",
					"MaxBPI", "Tot_Data_CMF",
					"Tot_Data_Cmd", "Tot_Data_Cmpl",
					"Lat(us)", "Avg_IO", "Max_IO", "Bsy",
					"IO_cnt", "Info", "BWutil(ms)");
	}

	 
	spin_lock_irq(ring_lock);
	while (*head_idx != *tail_idx) {
		entry = &ring[*head_idx];

		 
		if (!log_to_kmsg) {
			 
			scnprintf(tmp, sizeof(tmp),
				  "%03d:\t%-16llu%-16llu%-16llu%-16llu%-8llu"
				  "%-8llu%-8llu%-8u%-8u%-8u%u(%u)\n",
				  *head_idx, entry->max_bytes_per_interval,
				  entry->cmf_bytes, entry->total_bytes,
				  entry->rcv_bytes, entry->avg_io_latency,
				  entry->avg_io_size, entry->max_read_cnt,
				  entry->cmf_busy, entry->io_cnt,
				  entry->cmf_info, entry->timer_utilization,
				  entry->timer_interval);

			 
			if ((strlen(buf) + strlen(tmp)) >= buf_len)
				break;

			 
			strlcat(buf, tmp, buf_len);
		} else {
			lpfc_printf_log(phba, KERN_INFO, LOG_CGN_MGMT,
					"4410 %02u: MBPI %llu Xmit %llu "
					"Cmpl %llu Lat %llu ASz %llu Info %02u "
					"BWUtil %u Int %u slot %u\n",
					cnt, entry->max_bytes_per_interval,
					entry->total_bytes, entry->rcv_bytes,
					entry->avg_io_latency,
					entry->avg_io_size, entry->cmf_info,
					entry->timer_utilization,
					entry->timer_interval, *head_idx);
		}

		*head_idx = (*head_idx + 1) % ring_size;

		 
		cnt++;
		if (cnt >= max_read_entries)
			break;
	}
	spin_unlock_irq(ring_lock);

	return cnt;
}

 
static int
lpfc_cmf_setup(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_dmabuf *mp;
	struct lpfc_pc_sli4_params *sli4_params;
	int rc, cmf, mi_ver;

	rc = lpfc_sli4_refresh_params(phba);
	if (unlikely(rc))
		return rc;

	mboxq = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		return -ENOMEM;

	sli4_params = &phba->sli4_hba.pc_sli4_params;

	 
	if (sli4_params->mi_ver) {
		lpfc_set_features(phba, mboxq, LPFC_SET_ENABLE_MI);
		rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
		mi_ver = bf_get(lpfc_mbx_set_feature_mi,
				 &mboxq->u.mqe.un.set_feature);

		if (rc == MBX_SUCCESS) {
			if (mi_ver) {
				lpfc_printf_log(phba,
						KERN_WARNING, LOG_CGN_MGMT,
						"6215 MI is enabled\n");
				sli4_params->mi_ver = mi_ver;
			} else {
				lpfc_printf_log(phba,
						KERN_WARNING, LOG_CGN_MGMT,
						"6338 MI is disabled\n");
				sli4_params->mi_ver = 0;
			}
		} else {
			 
			lpfc_printf_log(phba, KERN_INFO,
					LOG_CGN_MGMT | LOG_INIT,
					"6245 Enable MI Mailbox x%x (x%x/x%x) "
					"failed, rc:x%x mi:x%x\n",
					bf_get(lpfc_mqe_command, &mboxq->u.mqe),
					lpfc_sli_config_mbox_subsys_get
						(phba, mboxq),
					lpfc_sli_config_mbox_opcode_get
						(phba, mboxq),
					rc, sli4_params->mi_ver);
		}
	} else {
		lpfc_printf_log(phba, KERN_WARNING, LOG_CGN_MGMT,
				"6217 MI is disabled\n");
	}

	 
	if (sli4_params->mi_ver)
		phba->cfg_fdmi_on = LPFC_FDMI_SUPPORT;

	 
	if (sli4_params->cmf) {
		lpfc_set_features(phba, mboxq, LPFC_SET_ENABLE_CMF);
		rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
		cmf = bf_get(lpfc_mbx_set_feature_cmf,
			     &mboxq->u.mqe.un.set_feature);
		if (rc == MBX_SUCCESS && cmf) {
			lpfc_printf_log(phba, KERN_WARNING, LOG_CGN_MGMT,
					"6218 CMF is enabled: mode %d\n",
					phba->cmf_active_mode);
		} else {
			lpfc_printf_log(phba, KERN_WARNING,
					LOG_CGN_MGMT | LOG_INIT,
					"6219 Enable CMF Mailbox x%x (x%x/x%x) "
					"failed, rc:x%x dd:x%x\n",
					bf_get(lpfc_mqe_command, &mboxq->u.mqe),
					lpfc_sli_config_mbox_subsys_get
						(phba, mboxq),
					lpfc_sli_config_mbox_opcode_get
						(phba, mboxq),
					rc, cmf);
			sli4_params->cmf = 0;
			phba->cmf_active_mode = LPFC_CFG_OFF;
			goto no_cmf;
		}

		 
		if (!phba->cgn_i) {
			mp = kmalloc(sizeof(*mp), GFP_KERNEL);
			if (mp)
				mp->virt = dma_alloc_coherent
						(&phba->pcidev->dev,
						sizeof(struct lpfc_cgn_info),
						&mp->phys, GFP_KERNEL);
			if (!mp || !mp->virt) {
				lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
						"2640 Failed to alloc memory "
						"for Congestion Info\n");
				kfree(mp);
				sli4_params->cmf = 0;
				phba->cmf_active_mode = LPFC_CFG_OFF;
				goto no_cmf;
			}
			phba->cgn_i = mp;

			 
			lpfc_init_congestion_buf(phba);
			lpfc_init_congestion_stat(phba);

			 
			atomic64_set(&phba->cgn_acqe_stat.alarm, 0);
			atomic64_set(&phba->cgn_acqe_stat.warn, 0);
		}

		rc = lpfc_sli4_cgn_params_read(phba);
		if (rc < 0) {
			lpfc_printf_log(phba, KERN_ERR, LOG_CGN_MGMT | LOG_INIT,
					"6242 Error reading Cgn Params (%d)\n",
					rc);
			 
			sli4_params->cmf = 0;
		} else if (!rc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_CGN_MGMT | LOG_INIT,
					"6243 CGN Event empty object.\n");
			 
			sli4_params->cmf = 0;
		}
	} else {
no_cmf:
		lpfc_printf_log(phba, KERN_WARNING, LOG_CGN_MGMT,
				"6220 CMF is disabled\n");
	}

	 
	if (sli4_params->cmf && sli4_params->mi_ver) {
		rc = lpfc_reg_congestion_buf(phba);
		if (rc) {
			dma_free_coherent(&phba->pcidev->dev,
					  sizeof(struct lpfc_cgn_info),
					  phba->cgn_i->virt, phba->cgn_i->phys);
			kfree(phba->cgn_i);
			phba->cgn_i = NULL;
			 
			phba->cmf_active_mode = LPFC_CFG_OFF;
			sli4_params->cmf = 0;
			return 0;
		}
	}
	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"6470 Setup MI version %d CMF %d mode %d\n",
			sli4_params->mi_ver, sli4_params->cmf,
			phba->cmf_active_mode);

	mempool_free(mboxq, phba->mbox_mem_pool);

	 
	atomic_set(&phba->cgn_fabric_warn_cnt, 0);
	atomic_set(&phba->cgn_fabric_alarm_cnt, 0);
	atomic_set(&phba->cgn_sync_alarm_cnt, 0);
	atomic_set(&phba->cgn_sync_warn_cnt, 0);
	atomic_set(&phba->cgn_driver_evt_cnt, 0);
	atomic_set(&phba->cgn_latency_evt_cnt, 0);
	atomic64_set(&phba->cgn_latency_evt, 0);

	phba->cmf_interval_rate = LPFC_CMF_INTERVAL;

	 
	if (!phba->rx_monitor) {
		phba->rx_monitor = kzalloc(sizeof(*phba->rx_monitor),
					   GFP_KERNEL);

		if (!phba->rx_monitor) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"2644 Failed to alloc memory "
					"for RX Monitor Buffer\n");
			return -ENOMEM;
		}

		 
		if (lpfc_rx_monitor_create_ring(phba->rx_monitor,
						LPFC_MAX_RXMONITOR_ENTRY)) {
			kfree(phba->rx_monitor);
			phba->rx_monitor = NULL;
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"2645 Failed to alloc memory "
					"for RX Monitor's Ring\n");
			return -ENOMEM;
		}
	}

	return 0;
}

static int
lpfc_set_host_tm(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mboxq;
	uint32_t len, rc;
	struct timespec64 cur_time;
	struct tm broken;
	uint32_t month, day, year;
	uint32_t hour, minute, second;
	struct lpfc_mbx_set_host_date_time *tm;

	mboxq = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		return -ENOMEM;

	len = sizeof(struct lpfc_mbx_set_host_data) -
		sizeof(struct lpfc_sli4_cfg_mhdr);
	lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_SET_HOST_DATA, len,
			 LPFC_SLI4_MBX_EMBED);

	mboxq->u.mqe.un.set_host_data.param_id = LPFC_SET_HOST_DATE_TIME;
	mboxq->u.mqe.un.set_host_data.param_len =
			sizeof(struct lpfc_mbx_set_host_date_time);
	tm = &mboxq->u.mqe.un.set_host_data.un.tm;
	ktime_get_real_ts64(&cur_time);
	time64_to_tm(cur_time.tv_sec, 0, &broken);
	month = broken.tm_mon + 1;
	day = broken.tm_mday;
	year = broken.tm_year - 100;
	hour = broken.tm_hour;
	minute = broken.tm_min;
	second = broken.tm_sec;
	bf_set(lpfc_mbx_set_host_month, tm, month);
	bf_set(lpfc_mbx_set_host_day, tm, day);
	bf_set(lpfc_mbx_set_host_year, tm, year);
	bf_set(lpfc_mbx_set_host_hour, tm, hour);
	bf_set(lpfc_mbx_set_host_min, tm, minute);
	bf_set(lpfc_mbx_set_host_sec, tm, second);

	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	mempool_free(mboxq, phba->mbox_mem_pool);
	return rc;
}

 
int
lpfc_sli4_hba_setup(struct lpfc_hba *phba)
{
	int rc, i, cnt, len, dd;
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_mqe *mqe;
	uint8_t *vpd;
	uint32_t vpd_size;
	uint32_t ftr_rsp = 0;
	struct Scsi_Host *shost = lpfc_shost_from_vport(phba->pport);
	struct lpfc_vport *vport = phba->pport;
	struct lpfc_dmabuf *mp;
	struct lpfc_rqb *rqbp;
	u32 flg;

	 
	rc = lpfc_pci_function_reset(phba);
	if (unlikely(rc))
		return -ENODEV;

	 
	rc = lpfc_sli4_post_status_check(phba);
	if (unlikely(rc))
		return -ENODEV;
	else {
		spin_lock_irq(&phba->hbalock);
		phba->sli.sli_flag |= LPFC_SLI_ACTIVE;
		flg = phba->sli.sli_flag;
		spin_unlock_irq(&phba->hbalock);
		 
		for (i = 0; i < 50 && (flg & LPFC_SLI_MBOX_ACTIVE); i++) {
			msleep(20);
			spin_lock_irq(&phba->hbalock);
			flg = phba->sli.sli_flag;
			spin_unlock_irq(&phba->hbalock);
		}
	}
	phba->hba_flag &= ~HBA_SETUP;

	lpfc_sli4_dip(phba);

	 
	mboxq = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		return -ENOMEM;

	 
	vpd_size = SLI4_PAGE_SIZE;
	vpd = kzalloc(vpd_size, GFP_KERNEL);
	if (!vpd) {
		rc = -ENOMEM;
		goto out_free_mbox;
	}

	rc = lpfc_sli4_read_rev(phba, mboxq, vpd, &vpd_size);
	if (unlikely(rc)) {
		kfree(vpd);
		goto out_free_mbox;
	}

	mqe = &mboxq->u.mqe;
	phba->sli_rev = bf_get(lpfc_mbx_rd_rev_sli_lvl, &mqe->un.read_rev);
	if (bf_get(lpfc_mbx_rd_rev_fcoe, &mqe->un.read_rev)) {
		phba->hba_flag |= HBA_FCOE_MODE;
		phba->fcp_embed_io = 0;	 
	} else {
		phba->hba_flag &= ~HBA_FCOE_MODE;
	}

	if (bf_get(lpfc_mbx_rd_rev_cee_ver, &mqe->un.read_rev) ==
		LPFC_DCBX_CEE_MODE)
		phba->hba_flag |= HBA_FIP_SUPPORT;
	else
		phba->hba_flag &= ~HBA_FIP_SUPPORT;

	phba->hba_flag &= ~HBA_IOQ_FLUSH;

	if (phba->sli_rev != LPFC_SLI_REV4) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"0376 READ_REV Error. SLI Level %d "
			"FCoE enabled %d\n",
			phba->sli_rev, phba->hba_flag & HBA_FCOE_MODE);
		rc = -EIO;
		kfree(vpd);
		goto out_free_mbox;
	}

	rc = lpfc_set_host_tm(phba);
	lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_INIT,
			"6468 Set host date / time: Status x%x:\n", rc);

	 
	if (phba->hba_flag & HBA_FCOE_MODE &&
	    lpfc_sli4_read_fcoe_params(phba))
		lpfc_printf_log(phba, KERN_WARNING, LOG_MBOX | LOG_INIT,
			"2570 Failed to read FCoE parameters\n");

	 
	rc = lpfc_sli4_retrieve_pport_name(phba);
	if (!rc)
		lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
				"3080 Successful retrieving SLI4 device "
				"physical port name: %s.\n", phba->Port);

	rc = lpfc_sli4_get_ctl_attr(phba);
	if (!rc)
		lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
				"8351 Successful retrieving SLI4 device "
				"CTL ATTR\n");

	 
	rc = lpfc_parse_vpd(phba, vpd, vpd_size);
	if (unlikely(!rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0377 Error %d parsing vpd. "
				"Using defaults.\n", rc);
		rc = 0;
	}
	kfree(vpd);

	 
	phba->vpd.rev.biuRev = mqe->un.read_rev.first_hw_rev;
	phba->vpd.rev.smRev = mqe->un.read_rev.second_hw_rev;

	 
	if ((bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) ==
			LPFC_SLI_INTF_IF_TYPE_6) &&
	    (phba->vpd.rev.biuRev == LPFC_G7_ASIC_1) &&
	    (phba->vpd.rev.smRev == 0) &&
	    (phba->cfg_nvme_embed_cmd == 1))
		phba->cfg_nvme_embed_cmd = 0;

	phba->vpd.rev.endecRev = mqe->un.read_rev.third_hw_rev;
	phba->vpd.rev.fcphHigh = bf_get(lpfc_mbx_rd_rev_fcph_high,
					 &mqe->un.read_rev);
	phba->vpd.rev.fcphLow = bf_get(lpfc_mbx_rd_rev_fcph_low,
				       &mqe->un.read_rev);
	phba->vpd.rev.feaLevelHigh = bf_get(lpfc_mbx_rd_rev_ftr_lvl_high,
					    &mqe->un.read_rev);
	phba->vpd.rev.feaLevelLow = bf_get(lpfc_mbx_rd_rev_ftr_lvl_low,
					   &mqe->un.read_rev);
	phba->vpd.rev.sli1FwRev = mqe->un.read_rev.fw_id_rev;
	memcpy(phba->vpd.rev.sli1FwName, mqe->un.read_rev.fw_name, 16);
	phba->vpd.rev.sli2FwRev = mqe->un.read_rev.ulp_fw_id_rev;
	memcpy(phba->vpd.rev.sli2FwName, mqe->un.read_rev.ulp_fw_name, 16);
	phba->vpd.rev.opFwRev = mqe->un.read_rev.fw_id_rev;
	memcpy(phba->vpd.rev.opFwName, mqe->un.read_rev.fw_name, 16);
	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
			"(%d):0380 READ_REV Status x%x "
			"fw_rev:%s fcphHi:%x fcphLo:%x flHi:%x flLo:%x\n",
			mboxq->vport ? mboxq->vport->vpi : 0,
			bf_get(lpfc_mqe_status, mqe),
			phba->vpd.rev.opFwName,
			phba->vpd.rev.fcphHigh, phba->vpd.rev.fcphLow,
			phba->vpd.rev.feaLevelHigh, phba->vpd.rev.feaLevelLow);

	if (bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) ==
	    LPFC_SLI_INTF_IF_TYPE_0) {
		lpfc_set_features(phba, mboxq, LPFC_SET_UE_RECOVERY);
		rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
		if (rc == MBX_SUCCESS) {
			phba->hba_flag |= HBA_RECOVERABLE_UE;
			 
			phba->eratt_poll_interval = 1;
			phba->sli4_hba.ue_to_sr = bf_get(
					lpfc_mbx_set_feature_UESR,
					&mboxq->u.mqe.un.set_feature);
			phba->sli4_hba.ue_to_rp = bf_get(
					lpfc_mbx_set_feature_UERP,
					&mboxq->u.mqe.un.set_feature);
		}
	}

	if (phba->cfg_enable_mds_diags && phba->mds_diags_support) {
		 
		lpfc_set_features(phba, mboxq, LPFC_SET_MDS_DIAGS);
		rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
		if (rc != MBX_SUCCESS)
			phba->mds_diags_support = 0;
	}

	 
	lpfc_request_features(phba, mboxq);
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	if (unlikely(rc)) {
		rc = -EIO;
		goto out_free_mbox;
	}

	 
	if (phba->cfg_vmid_app_header && !(bf_get(lpfc_mbx_rq_ftr_rsp_ashdr,
						  &mqe->un.req_ftrs))) {
		bf_set(lpfc_ftr_ashdr, &phba->sli4_hba.sli4_flags, 0);
		phba->cfg_vmid_app_header = 0;
		lpfc_printf_log(phba, KERN_DEBUG, LOG_SLI,
				"1242 vmid feature not supported\n");
	}

	 
	if (!(bf_get(lpfc_mbx_rq_ftr_rsp_fcpi, &mqe->un.req_ftrs))) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_MBOX | LOG_SLI,
				"0378 No support for fcpi mode.\n");
		ftr_rsp++;
	}

	 
	if (phba->hba_flag & HBA_FCOE_MODE) {
		if (bf_get(lpfc_mbx_rq_ftr_rsp_perfh, &mqe->un.req_ftrs))
			phba->sli3_options |= LPFC_SLI4_PERFH_ENABLED;
		else
			phba->sli3_options &= ~LPFC_SLI4_PERFH_ENABLED;
	}

	 
	if (phba->sli3_options & LPFC_SLI3_BG_ENABLED) {
		if (!(bf_get(lpfc_mbx_rq_ftr_rsp_dif, &mqe->un.req_ftrs))) {
			phba->cfg_enable_bg = 0;
			phba->sli3_options &= ~LPFC_SLI3_BG_ENABLED;
			ftr_rsp++;
		}
	}

	if (phba->max_vpi && phba->cfg_enable_npiv &&
	    !(bf_get(lpfc_mbx_rq_ftr_rsp_npiv, &mqe->un.req_ftrs)))
		ftr_rsp++;

	if (ftr_rsp) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_MBOX | LOG_SLI,
				"0379 Feature Mismatch Data: x%08x %08x "
				"x%x x%x x%x\n", mqe->un.req_ftrs.word2,
				mqe->un.req_ftrs.word3, phba->cfg_enable_bg,
				phba->cfg_enable_npiv, phba->max_vpi);
		if (!(bf_get(lpfc_mbx_rq_ftr_rsp_dif, &mqe->un.req_ftrs)))
			phba->cfg_enable_bg = 0;
		if (!(bf_get(lpfc_mbx_rq_ftr_rsp_npiv, &mqe->un.req_ftrs)))
			phba->cfg_enable_npiv = 0;
	}

	 
	spin_lock_irq(&phba->hbalock);
	phba->sli3_options |= (LPFC_SLI3_NPIV_ENABLED | LPFC_SLI3_HBQ_ENABLED);
	spin_unlock_irq(&phba->hbalock);

	 
	lpfc_set_features(phba, mboxq, LPFC_SET_DUAL_DUMP);
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	dd = bf_get(lpfc_mbx_set_feature_dd, &mboxq->u.mqe.un.set_feature);
	if ((rc == MBX_SUCCESS) && (dd == LPFC_ENABLE_DUAL_DUMP))
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"6448 Dual Dump is enabled\n");
	else
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI | LOG_INIT,
				"6447 Dual Dump Mailbox x%x (x%x/x%x) failed, "
				"rc:x%x dd:x%x\n",
				bf_get(lpfc_mqe_command, &mboxq->u.mqe),
				lpfc_sli_config_mbox_subsys_get(
					phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(
					phba, mboxq),
				rc, dd);
	 
	rc = lpfc_sli4_alloc_resource_identifiers(phba);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2920 Failed to alloc Resource IDs "
				"rc = x%x\n", rc);
		goto out_free_mbox;
	}

	lpfc_set_host_data(phba, mboxq);

	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	if (rc) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_MBOX | LOG_SLI,
				"2134 Failed to set host os driver version %x",
				rc);
	}

	 
	rc = lpfc_read_sparam(phba, mboxq, vport->vpi);
	if (rc) {
		phba->link_state = LPFC_HBA_ERROR;
		rc = -ENOMEM;
		goto out_free_mbox;
	}

	mboxq->vport = vport;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	mp = (struct lpfc_dmabuf *)mboxq->ctx_buf;
	if (rc == MBX_SUCCESS) {
		memcpy(&vport->fc_sparam, mp->virt, sizeof(struct serv_parm));
		rc = 0;
	}

	 
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	mboxq->ctx_buf = NULL;
	if (unlikely(rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0382 READ_SPARAM command failed "
				"status %d, mbxStatus x%x\n",
				rc, bf_get(lpfc_mqe_status, mqe));
		phba->link_state = LPFC_HBA_ERROR;
		rc = -EIO;
		goto out_free_mbox;
	}

	lpfc_update_vport_wwn(vport);

	 
	fc_host_node_name(shost) = wwn_to_u64(vport->fc_nodename.u.wwn);
	fc_host_port_name(shost) = wwn_to_u64(vport->fc_portname.u.wwn);

	 
	rc = lpfc_sli4_queue_create(phba);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3089 Failed to allocate queues\n");
		rc = -ENODEV;
		goto out_free_mbox;
	}
	 
	rc = lpfc_sli4_queue_setup(phba);
	if (unlikely(rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0381 Error %d during queue setup.\n ", rc);
		goto out_stop_timers;
	}
	 
	lpfc_sli4_setup(phba);
	lpfc_sli4_queue_init(phba);

	 
	rc = lpfc_sli4_els_sgl_update(phba);
	if (unlikely(rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"1400 Failed to update xri-sgl size and "
				"mapping: %d\n", rc);
		goto out_destroy_queue;
	}

	 
	rc = lpfc_sli4_repost_sgl_list(phba, &phba->sli4_hba.lpfc_els_sgl_list,
				       phba->sli4_hba.els_xri_cnt);
	if (unlikely(rc < 0)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0582 Error %d during els sgl post "
				"operation\n", rc);
		rc = -ENODEV;
		goto out_destroy_queue;
	}
	phba->sli4_hba.els_xri_cnt = rc;

	if (phba->nvmet_support) {
		 
		rc = lpfc_sli4_nvmet_sgl_update(phba);
		if (unlikely(rc)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"6308 Failed to update nvmet-sgl size "
					"and mapping: %d\n", rc);
			goto out_destroy_queue;
		}

		 
		rc = lpfc_sli4_repost_sgl_list(
			phba,
			&phba->sli4_hba.lpfc_nvmet_sgl_list,
			phba->sli4_hba.nvmet_xri_cnt);
		if (unlikely(rc < 0)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"3117 Error %d during nvmet "
					"sgl post\n", rc);
			rc = -ENODEV;
			goto out_destroy_queue;
		}
		phba->sli4_hba.nvmet_xri_cnt = rc;

		 
		cnt = phba->sli4_hba.nvmet_xri_cnt +
			phba->sli4_hba.max_cfg_param.max_xri;
	} else {
		 
		rc = lpfc_sli4_io_sgl_update(phba);
		if (unlikely(rc)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"6082 Failed to update nvme-sgl size "
					"and mapping: %d\n", rc);
			goto out_destroy_queue;
		}

		 
		rc = lpfc_sli4_repost_io_sgl_list(phba);
		if (unlikely(rc)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"6116 Error %d during nvme sgl post "
					"operation\n", rc);
			 
			 
			rc = -ENODEV;
			goto out_destroy_queue;
		}
		 
		cnt = phba->sli4_hba.max_cfg_param.max_xri;
	}

	if (!phba->sli.iocbq_lookup) {
		 
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"2821 initialize iocb list with %d entries\n",
				cnt);
		rc = lpfc_init_iocb_list(phba, cnt);
		if (rc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"1413 Failed to init iocb list.\n");
			goto out_destroy_queue;
		}
	}

	if (phba->nvmet_support)
		lpfc_nvmet_create_targetport(phba);

	if (phba->nvmet_support && phba->cfg_nvmet_mrq) {
		 
		for (i = 0; i < phba->cfg_nvmet_mrq; i++) {
			rqbp = phba->sli4_hba.nvmet_mrq_hdr[i]->rqbp;
			INIT_LIST_HEAD(&rqbp->rqb_buffer_list);
			rqbp->rqb_alloc_buffer = lpfc_sli4_nvmet_alloc;
			rqbp->rqb_free_buffer = lpfc_sli4_nvmet_free;
			rqbp->entry_count = LPFC_NVMET_RQE_DEF_COUNT;
			rqbp->buffer_count = 0;

			lpfc_post_rq_buffer(
				phba, phba->sli4_hba.nvmet_mrq_hdr[i],
				phba->sli4_hba.nvmet_mrq_data[i],
				phba->cfg_nvmet_mrq_post, i);
		}
	}

	 
	rc = lpfc_sli4_post_all_rpi_hdrs(phba);
	if (unlikely(rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0393 Error %d during rpi post operation\n",
				rc);
		rc = -ENODEV;
		goto out_free_iocblist;
	}
	lpfc_sli4_node_prep(phba);

	if (!(phba->hba_flag & HBA_FCOE_MODE)) {
		if ((phba->nvmet_support == 0) || (phba->cfg_nvmet_mrq == 1)) {
			 
			lpfc_reg_fcfi(phba, mboxq);
			mboxq->vport = phba->pport;
			rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
			if (rc != MBX_SUCCESS)
				goto out_unset_queue;
			rc = 0;
			phba->fcf.fcfi = bf_get(lpfc_reg_fcfi_fcfi,
						&mboxq->u.mqe.un.reg_fcfi);
		} else {
			 

			 
			lpfc_reg_fcfi_mrq(phba, mboxq, 0);
			mboxq->vport = phba->pport;
			rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
			if (rc != MBX_SUCCESS)
				goto out_unset_queue;
			rc = 0;
			phba->fcf.fcfi = bf_get(lpfc_reg_fcfi_mrq_fcfi,
						&mboxq->u.mqe.un.reg_fcfi_mrq);

			 
			lpfc_reg_fcfi_mrq(phba, mboxq, 1);
			mboxq->vport = phba->pport;
			rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
			if (rc != MBX_SUCCESS)
				goto out_unset_queue;
			rc = 0;
		}
		 
		lpfc_sli_read_link_ste(phba);
	}

	 
	if (phba->nvmet_support == 0) {
		if (phba->sli4_hba.io_xri_cnt == 0) {
			len = lpfc_new_io_buf(
					      phba, phba->sli4_hba.io_xri_max);
			if (len == 0) {
				rc = -ENOMEM;
				goto out_unset_queue;
			}

			if (phba->cfg_xri_rebalancing)
				lpfc_create_multixri_pools(phba);
		}
	} else {
		phba->cfg_xri_rebalancing = 0;
	}

	 
	spin_lock_irq(&phba->hbalock);
	phba->sli.sli_flag &= ~LPFC_SLI_ASYNC_MBX_BLK;
	spin_unlock_irq(&phba->hbalock);

	 
	lpfc_sli4_rb_setup(phba);

	 
	phba->fcf.fcf_flag = 0;
	phba->fcf.current_rec.flag = 0;

	 
	mod_timer(&vport->els_tmofunc,
		  jiffies + msecs_to_jiffies(1000 * (phba->fc_ratov * 2)));

	 
	mod_timer(&phba->hb_tmofunc,
		  jiffies + msecs_to_jiffies(1000 * LPFC_HB_MBOX_INTERVAL));
	phba->hba_flag &= ~(HBA_HBEAT_INP | HBA_HBEAT_TMO);
	phba->last_completion_time = jiffies;

	 
	if (phba->cfg_auto_imax)
		queue_delayed_work(phba->wq, &phba->eq_delay_work,
				   msecs_to_jiffies(LPFC_EQ_DELAY_MSECS));

	 
	lpfc_init_idle_stat_hb(phba);

	 
	mod_timer(&phba->eratt_poll,
		  jiffies + msecs_to_jiffies(1000 * phba->eratt_poll_interval));

	 
	spin_lock_irq(&phba->hbalock);
	phba->link_state = LPFC_LINK_DOWN;

	 
	if (bf_get(lpfc_conf_trunk_port0, &phba->sli4_hba))
		phba->trunk_link.link0.state = LPFC_LINK_DOWN;
	if (bf_get(lpfc_conf_trunk_port1, &phba->sli4_hba))
		phba->trunk_link.link1.state = LPFC_LINK_DOWN;
	if (bf_get(lpfc_conf_trunk_port2, &phba->sli4_hba))
		phba->trunk_link.link2.state = LPFC_LINK_DOWN;
	if (bf_get(lpfc_conf_trunk_port3, &phba->sli4_hba))
		phba->trunk_link.link3.state = LPFC_LINK_DOWN;
	spin_unlock_irq(&phba->hbalock);

	 
	lpfc_sli4_arm_cqeq_intr(phba);

	 
	phba->sli4_hba.intr_enable = 1;

	 
	lpfc_cmf_setup(phba);

	if (!(phba->hba_flag & HBA_FCOE_MODE) &&
	    (phba->hba_flag & LINK_DISABLED)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3103 Adapter Link is disabled.\n");
		lpfc_down_link(phba, mboxq);
		rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"3104 Adapter failed to issue "
					"DOWN_LINK mbox cmd, rc:x%x\n", rc);
			goto out_io_buff_free;
		}
	} else if (phba->cfg_suppress_link_up == LPFC_INITIALIZE_LINK) {
		 
		if (!(phba->link_flag & LS_LOOPBACK_MODE)) {
			rc = phba->lpfc_hba_init_link(phba, MBX_NOWAIT);
			if (rc)
				goto out_io_buff_free;
		}
	}
	mempool_free(mboxq, phba->mbox_mem_pool);

	 
	lpfc_sli4_ras_setup(phba);

	phba->hba_flag |= HBA_SETUP;
	return rc;

out_io_buff_free:
	 
	lpfc_io_free(phba);
out_unset_queue:
	 
	lpfc_sli4_queue_unset(phba);
out_free_iocblist:
	lpfc_free_iocb_list(phba);
out_destroy_queue:
	lpfc_sli4_queue_destroy(phba);
out_stop_timers:
	lpfc_stop_hba_timers(phba);
out_free_mbox:
	mempool_free(mboxq, phba->mbox_mem_pool);
	return rc;
}

 
void
lpfc_mbox_timeout(struct timer_list *t)
{
	struct lpfc_hba  *phba = from_timer(phba, t, sli.mbox_tmo);
	unsigned long iflag;
	uint32_t tmo_posted;

	spin_lock_irqsave(&phba->pport->work_port_lock, iflag);
	tmo_posted = phba->pport->work_port_events & WORKER_MBOX_TMO;
	if (!tmo_posted)
		phba->pport->work_port_events |= WORKER_MBOX_TMO;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, iflag);

	if (!tmo_posted)
		lpfc_worker_wake_up(phba);
	return;
}

 
static bool
lpfc_sli4_mbox_completions_pending(struct lpfc_hba *phba)
{

	uint32_t idx;
	struct lpfc_queue *mcq;
	struct lpfc_mcqe *mcqe;
	bool pending_completions = false;
	uint8_t	qe_valid;

	if (unlikely(!phba) || (phba->sli_rev != LPFC_SLI_REV4))
		return false;

	 

	mcq = phba->sli4_hba.mbx_cq;
	idx = mcq->hba_index;
	qe_valid = mcq->qe_valid;
	while (bf_get_le32(lpfc_cqe_valid,
	       (struct lpfc_cqe *)lpfc_sli4_qe(mcq, idx)) == qe_valid) {
		mcqe = (struct lpfc_mcqe *)(lpfc_sli4_qe(mcq, idx));
		if (bf_get_le32(lpfc_trailer_completed, mcqe) &&
		    (!bf_get_le32(lpfc_trailer_async, mcqe))) {
			pending_completions = true;
			break;
		}
		idx = (idx + 1) % mcq->entry_count;
		if (mcq->hba_index == idx)
			break;

		 
		if (phba->sli4_hba.pc_sli4_params.cqav && !idx)
			qe_valid = (qe_valid) ? 0 : 1;
	}
	return pending_completions;

}

 
static bool
lpfc_sli4_process_missed_mbox_completions(struct lpfc_hba *phba)
{
	struct lpfc_sli4_hba *sli4_hba = &phba->sli4_hba;
	uint32_t eqidx;
	struct lpfc_queue *fpeq = NULL;
	struct lpfc_queue *eq;
	bool mbox_pending;

	if (unlikely(!phba) || (phba->sli_rev != LPFC_SLI_REV4))
		return false;

	 
	if (sli4_hba->hdwq) {
		for (eqidx = 0; eqidx < phba->cfg_irq_chann; eqidx++) {
			eq = phba->sli4_hba.hba_eq_hdl[eqidx].eq;
			if (eq && eq->queue_id == sli4_hba->mbx_cq->assoc_qid) {
				fpeq = eq;
				break;
			}
		}
	}
	if (!fpeq)
		return false;

	 

	sli4_hba->sli4_eq_clr_intr(fpeq);

	 

	mbox_pending = lpfc_sli4_mbox_completions_pending(phba);

	 

	if (mbox_pending)
		 
		lpfc_sli4_process_eq(phba, fpeq, LPFC_QUEUE_REARM,
				     LPFC_QUEUE_WORK);
	else
		 
		sli4_hba->sli4_write_eq_db(phba, fpeq, 0, LPFC_QUEUE_REARM);

	return mbox_pending;

}

 
void
lpfc_mbox_timeout_handler(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *pmbox = phba->sli.mbox_active;
	MAILBOX_t *mb = NULL;

	struct lpfc_sli *psli = &phba->sli;

	 
	lpfc_sli4_process_missed_mbox_completions(phba);

	if (!(psli->sli_flag & LPFC_SLI_ACTIVE))
		return;

	if (pmbox != NULL)
		mb = &pmbox->u.mb;
	 
	spin_lock_irq(&phba->hbalock);
	if (pmbox == NULL) {
		lpfc_printf_log(phba, KERN_WARNING,
				LOG_MBOX | LOG_SLI,
				"0353 Active Mailbox cleared - mailbox timeout "
				"exiting\n");
		spin_unlock_irq(&phba->hbalock);
		return;
	}

	 
	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"0310 Mailbox command x%x timeout Data: x%x x%x x%px\n",
			mb->mbxCommand,
			phba->pport->port_state,
			phba->sli.sli_flag,
			phba->sli.mbox_active);
	spin_unlock_irq(&phba->hbalock);

	 
	set_bit(MBX_TMO_ERR, &phba->bit_flags);
	spin_lock_irq(&phba->pport->work_port_lock);
	phba->pport->work_port_events &= ~WORKER_MBOX_TMO;
	spin_unlock_irq(&phba->pport->work_port_lock);
	spin_lock_irq(&phba->hbalock);
	phba->link_state = LPFC_LINK_UNKNOWN;
	psli->sli_flag &= ~LPFC_SLI_ACTIVE;
	spin_unlock_irq(&phba->hbalock);

	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"0345 Resetting board due to mailbox timeout\n");

	 
	lpfc_reset_hba(phba);
}

 
static int
lpfc_sli_issue_mbox_s3(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmbox,
		       uint32_t flag)
{
	MAILBOX_t *mbx;
	struct lpfc_sli *psli = &phba->sli;
	uint32_t status, evtctr;
	uint32_t ha_copy, hc_copy;
	int i;
	unsigned long timeout;
	unsigned long drvr_flag = 0;
	uint32_t word0, ldata;
	void __iomem *to_slim;
	int processing_queue = 0;

	spin_lock_irqsave(&phba->hbalock, drvr_flag);
	if (!pmbox) {
		phba->sli.sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
		 
		if (unlikely(psli->sli_flag & LPFC_SLI_ASYNC_MBX_BLK)) {
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
			return MBX_SUCCESS;
		}
		processing_queue = 1;
		pmbox = lpfc_mbox_get(phba);
		if (!pmbox) {
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
			return MBX_SUCCESS;
		}
	}

	if (pmbox->mbox_cmpl && pmbox->mbox_cmpl != lpfc_sli_def_mbox_cmpl &&
		pmbox->mbox_cmpl != lpfc_sli_wake_mbox_wait) {
		if(!pmbox->vport) {
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
			lpfc_printf_log(phba, KERN_ERR,
					LOG_MBOX | LOG_VPORT,
					"1806 Mbox x%x failed. No vport\n",
					pmbox->u.mb.mbxCommand);
			dump_stack();
			goto out_not_finished;
		}
	}

	 
	if (unlikely(pci_channel_offline(phba->pcidev))) {
		spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
		goto out_not_finished;
	}

	 
	if (unlikely(phba->hba_flag & DEFER_ERATT)) {
		spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
		goto out_not_finished;
	}

	psli = &phba->sli;

	mbx = &pmbox->u.mb;
	status = MBX_SUCCESS;

	if (phba->link_state == LPFC_HBA_ERROR) {
		spin_unlock_irqrestore(&phba->hbalock, drvr_flag);

		 
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"(%d):0311 Mailbox command x%x cannot "
				"issue Data: x%x x%x\n",
				pmbox->vport ? pmbox->vport->vpi : 0,
				pmbox->u.mb.mbxCommand, psli->sli_flag, flag);
		goto out_not_finished;
	}

	if (mbx->mbxCommand != MBX_KILL_BOARD && flag & MBX_NOWAIT) {
		if (lpfc_readl(phba->HCregaddr, &hc_copy) ||
			!(hc_copy & HC_MBINT_ENA)) {
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"(%d):2528 Mailbox command x%x cannot "
				"issue Data: x%x x%x\n",
				pmbox->vport ? pmbox->vport->vpi : 0,
				pmbox->u.mb.mbxCommand, psli->sli_flag, flag);
			goto out_not_finished;
		}
	}

	if (psli->sli_flag & LPFC_SLI_MBOX_ACTIVE) {
		 

		if (flag & MBX_POLL) {
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);

			 
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"(%d):2529 Mailbox command x%x "
					"cannot issue Data: x%x x%x\n",
					pmbox->vport ? pmbox->vport->vpi : 0,
					pmbox->u.mb.mbxCommand,
					psli->sli_flag, flag);
			goto out_not_finished;
		}

		if (!(psli->sli_flag & LPFC_SLI_ACTIVE)) {
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
			 
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"(%d):2530 Mailbox command x%x "
					"cannot issue Data: x%x x%x\n",
					pmbox->vport ? pmbox->vport->vpi : 0,
					pmbox->u.mb.mbxCommand,
					psli->sli_flag, flag);
			goto out_not_finished;
		}

		 
		lpfc_mbox_put(phba, pmbox);

		 
		lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
				"(%d):0308 Mbox cmd issue - BUSY Data: "
				"x%x x%x x%x x%x\n",
				pmbox->vport ? pmbox->vport->vpi : 0xffffff,
				mbx->mbxCommand,
				phba->pport ? phba->pport->port_state : 0xff,
				psli->sli_flag, flag);

		psli->slistat.mbox_busy++;
		spin_unlock_irqrestore(&phba->hbalock, drvr_flag);

		if (pmbox->vport) {
			lpfc_debugfs_disc_trc(pmbox->vport,
				LPFC_DISC_TRC_MBOX_VPORT,
				"MBOX Bsy vport:  cmd:x%x mb:x%x x%x",
				(uint32_t)mbx->mbxCommand,
				mbx->un.varWords[0], mbx->un.varWords[1]);
		}
		else {
			lpfc_debugfs_disc_trc(phba->pport,
				LPFC_DISC_TRC_MBOX,
				"MBOX Bsy:        cmd:x%x mb:x%x x%x",
				(uint32_t)mbx->mbxCommand,
				mbx->un.varWords[0], mbx->un.varWords[1]);
		}

		return MBX_BUSY;
	}

	psli->sli_flag |= LPFC_SLI_MBOX_ACTIVE;

	 
	if (flag != MBX_POLL) {
		if (!(psli->sli_flag & LPFC_SLI_ACTIVE) &&
		    (mbx->mbxCommand != MBX_KILL_BOARD)) {
			psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
			 
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"(%d):2531 Mailbox command x%x "
					"cannot issue Data: x%x x%x\n",
					pmbox->vport ? pmbox->vport->vpi : 0,
					pmbox->u.mb.mbxCommand,
					psli->sli_flag, flag);
			goto out_not_finished;
		}
		 
		timeout = msecs_to_jiffies(lpfc_mbox_tmo_val(phba, pmbox) *
					   1000);
		mod_timer(&psli->mbox_tmo, jiffies + timeout);
	}

	 
	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
			"(%d):0309 Mailbox cmd x%x issue Data: x%x x%x "
			"x%x\n",
			pmbox->vport ? pmbox->vport->vpi : 0,
			mbx->mbxCommand,
			phba->pport ? phba->pport->port_state : 0xff,
			psli->sli_flag, flag);

	if (mbx->mbxCommand != MBX_HEARTBEAT) {
		if (pmbox->vport) {
			lpfc_debugfs_disc_trc(pmbox->vport,
				LPFC_DISC_TRC_MBOX_VPORT,
				"MBOX Send vport: cmd:x%x mb:x%x x%x",
				(uint32_t)mbx->mbxCommand,
				mbx->un.varWords[0], mbx->un.varWords[1]);
		}
		else {
			lpfc_debugfs_disc_trc(phba->pport,
				LPFC_DISC_TRC_MBOX,
				"MBOX Send:       cmd:x%x mb:x%x x%x",
				(uint32_t)mbx->mbxCommand,
				mbx->un.varWords[0], mbx->un.varWords[1]);
		}
	}

	psli->slistat.mbox_cmd++;
	evtctr = psli->slistat.mbox_event;

	 
	mbx->mbxOwner = OWN_CHIP;

	if (psli->sli_flag & LPFC_SLI_ACTIVE) {
		 
		if (pmbox->in_ext_byte_len || pmbox->out_ext_byte_len) {
			*(((uint32_t *)mbx) + pmbox->mbox_offset_word)
				= (uint8_t *)phba->mbox_ext
				  - (uint8_t *)phba->mbox;
		}

		 
		if (pmbox->in_ext_byte_len && pmbox->ctx_buf) {
			lpfc_sli_pcimem_bcopy(pmbox->ctx_buf,
					      (uint8_t *)phba->mbox_ext,
					      pmbox->in_ext_byte_len);
		}
		 
		lpfc_sli_pcimem_bcopy(mbx, phba->mbox, MAILBOX_CMD_SIZE);
	} else {
		 
		if (pmbox->in_ext_byte_len || pmbox->out_ext_byte_len)
			*(((uint32_t *)mbx) + pmbox->mbox_offset_word)
				= MAILBOX_HBA_EXT_OFFSET;

		 
		if (pmbox->in_ext_byte_len && pmbox->ctx_buf)
			lpfc_memcpy_to_slim(phba->MBslimaddr +
				MAILBOX_HBA_EXT_OFFSET,
				pmbox->ctx_buf, pmbox->in_ext_byte_len);

		if (mbx->mbxCommand == MBX_CONFIG_PORT)
			 
			lpfc_sli_pcimem_bcopy(mbx, phba->mbox,
					      MAILBOX_CMD_SIZE);

		 
		to_slim = phba->MBslimaddr + sizeof (uint32_t);
		lpfc_memcpy_to_slim(to_slim, &mbx->un.varWords[0],
			    MAILBOX_CMD_SIZE - sizeof (uint32_t));

		 
		ldata = *((uint32_t *)mbx);
		to_slim = phba->MBslimaddr;
		writel(ldata, to_slim);
		readl(to_slim);  

		if (mbx->mbxCommand == MBX_CONFIG_PORT)
			 
			psli->sli_flag |= LPFC_SLI_ACTIVE;
	}

	wmb();

	switch (flag) {
	case MBX_NOWAIT:
		 
		psli->mbox_active = pmbox;
		 
		writel(CA_MBATT, phba->CAregaddr);
		readl(phba->CAregaddr);  
		 
		break;

	case MBX_POLL:
		 
		psli->mbox_active = NULL;
		 
		writel(CA_MBATT, phba->CAregaddr);
		readl(phba->CAregaddr);  

		if (psli->sli_flag & LPFC_SLI_ACTIVE) {
			 
			word0 = *((uint32_t *)phba->mbox);
			word0 = le32_to_cpu(word0);
		} else {
			 
			if (lpfc_readl(phba->MBslimaddr, &word0)) {
				spin_unlock_irqrestore(&phba->hbalock,
						       drvr_flag);
				goto out_not_finished;
			}
		}

		 
		if (lpfc_readl(phba->HAregaddr, &ha_copy)) {
			spin_unlock_irqrestore(&phba->hbalock,
						       drvr_flag);
			goto out_not_finished;
		}
		timeout = msecs_to_jiffies(lpfc_mbox_tmo_val(phba, pmbox) *
							1000) + jiffies;
		i = 0;
		 
		while (((word0 & OWN_CHIP) == OWN_CHIP) ||
		       (!(ha_copy & HA_MBATT) &&
			(phba->link_state > LPFC_WARM_START))) {
			if (time_after(jiffies, timeout)) {
				psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
				spin_unlock_irqrestore(&phba->hbalock,
						       drvr_flag);
				goto out_not_finished;
			}

			 
			if (((word0 & OWN_CHIP) != OWN_CHIP)
			    && (evtctr != psli->slistat.mbox_event))
				break;

			if (i++ > 10) {
				spin_unlock_irqrestore(&phba->hbalock,
						       drvr_flag);
				msleep(1);
				spin_lock_irqsave(&phba->hbalock, drvr_flag);
			}

			if (psli->sli_flag & LPFC_SLI_ACTIVE) {
				 
				word0 = *((uint32_t *)phba->mbox);
				word0 = le32_to_cpu(word0);
				if (mbx->mbxCommand == MBX_CONFIG_PORT) {
					MAILBOX_t *slimmb;
					uint32_t slimword0;
					 
					slimword0 = readl(phba->MBslimaddr);
					slimmb = (MAILBOX_t *) & slimword0;
					if (((slimword0 & OWN_CHIP) != OWN_CHIP)
					    && slimmb->mbxStatus) {
						psli->sli_flag &=
						    ~LPFC_SLI_ACTIVE;
						word0 = slimword0;
					}
				}
			} else {
				 
				word0 = readl(phba->MBslimaddr);
			}
			 
			if (lpfc_readl(phba->HAregaddr, &ha_copy)) {
				spin_unlock_irqrestore(&phba->hbalock,
						       drvr_flag);
				goto out_not_finished;
			}
		}

		if (psli->sli_flag & LPFC_SLI_ACTIVE) {
			 
			lpfc_sli_pcimem_bcopy(phba->mbox, mbx,
						MAILBOX_CMD_SIZE);
			 
			if (pmbox->out_ext_byte_len && pmbox->ctx_buf) {
				lpfc_sli_pcimem_bcopy(phba->mbox_ext,
						      pmbox->ctx_buf,
						      pmbox->out_ext_byte_len);
			}
		} else {
			 
			lpfc_memcpy_from_slim(mbx, phba->MBslimaddr,
						MAILBOX_CMD_SIZE);
			 
			if (pmbox->out_ext_byte_len && pmbox->ctx_buf) {
				lpfc_memcpy_from_slim(
					pmbox->ctx_buf,
					phba->MBslimaddr +
					MAILBOX_HBA_EXT_OFFSET,
					pmbox->out_ext_byte_len);
			}
		}

		writel(HA_MBATT, phba->HAregaddr);
		readl(phba->HAregaddr);  

		psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
		status = mbx->mbxStatus;
	}

	spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
	return status;

out_not_finished:
	if (processing_queue) {
		pmbox->u.mb.mbxStatus = MBX_NOT_FINISHED;
		lpfc_mbox_cmpl_put(phba, pmbox);
	}
	return MBX_NOT_FINISHED;
}

 
static int
lpfc_sli4_async_mbox_block(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	LPFC_MBOXQ_t *mboxq;
	int rc = 0;
	unsigned long timeout = 0;
	u32 sli_flag;
	u8 cmd, subsys, opcode;

	 
	spin_lock_irq(&phba->hbalock);
	psli->sli_flag |= LPFC_SLI_ASYNC_MBX_BLK;
	 
	if (phba->sli.mbox_active)
		timeout = msecs_to_jiffies(lpfc_mbox_tmo_val(phba,
						phba->sli.mbox_active) *
						1000) + jiffies;
	spin_unlock_irq(&phba->hbalock);

	 
	if (timeout)
		lpfc_sli4_process_missed_mbox_completions(phba);

	 
	while (phba->sli.mbox_active) {
		 
		msleep(2);
		if (time_after(jiffies, timeout)) {
			 

			 
			spin_lock_irq(&phba->hbalock);
			if (phba->sli.mbox_active) {
				mboxq = phba->sli.mbox_active;
				cmd = mboxq->u.mb.mbxCommand;
				subsys = lpfc_sli_config_mbox_subsys_get(phba,
									 mboxq);
				opcode = lpfc_sli_config_mbox_opcode_get(phba,
									 mboxq);
				sli_flag = psli->sli_flag;
				spin_unlock_irq(&phba->hbalock);
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"2352 Mailbox command x%x "
						"(x%x/x%x) sli_flag x%x could "
						"not complete\n",
						cmd, subsys, opcode,
						sli_flag);
			} else {
				spin_unlock_irq(&phba->hbalock);
			}

			rc = 1;
			break;
		}
	}

	 
	if (rc) {
		spin_lock_irq(&phba->hbalock);
		psli->sli_flag &= ~LPFC_SLI_ASYNC_MBX_BLK;
		spin_unlock_irq(&phba->hbalock);
	}
	return rc;
}

 
static void
lpfc_sli4_async_mbox_unblock(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;

	spin_lock_irq(&phba->hbalock);
	if (!(psli->sli_flag & LPFC_SLI_ASYNC_MBX_BLK)) {
		 
		spin_unlock_irq(&phba->hbalock);
		return;
	}

	 
	psli->sli_flag &= ~LPFC_SLI_ASYNC_MBX_BLK;
	spin_unlock_irq(&phba->hbalock);

	 
	lpfc_worker_wake_up(phba);
}

 
static int
lpfc_sli4_wait_bmbx_ready(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	uint32_t db_ready;
	unsigned long timeout;
	struct lpfc_register bmbx_reg;
	struct lpfc_register portstat_reg = {-1};

	 
	if (bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) >=
	    LPFC_SLI_INTF_IF_TYPE_2) {
		if (lpfc_readl(phba->sli4_hba.u.if_type2.STATUSregaddr,
			       &portstat_reg.word0) ||
		    lpfc_sli4_unrecoverable_port(&portstat_reg)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"3858 Skipping bmbx ready because "
					"Port Status x%x\n",
					portstat_reg.word0);
			return MBXERR_ERROR;
		}
	}

	timeout = msecs_to_jiffies(lpfc_mbox_tmo_val(phba, mboxq)
				   * 1000) + jiffies;

	do {
		bmbx_reg.word0 = readl(phba->sli4_hba.BMBXregaddr);
		db_ready = bf_get(lpfc_bmbx_rdy, &bmbx_reg);
		if (!db_ready)
			mdelay(2);

		if (time_after(jiffies, timeout))
			return MBXERR_ERROR;
	} while (!db_ready);

	return 0;
}

 
static int
lpfc_sli4_post_sync_mbox(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	int rc = MBX_SUCCESS;
	unsigned long iflag;
	uint32_t mcqe_status;
	uint32_t mbx_cmnd;
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_mqe *mb = &mboxq->u.mqe;
	struct lpfc_bmbx_create *mbox_rgn;
	struct dma_address *dma_address;

	 
	spin_lock_irqsave(&phba->hbalock, iflag);
	if (psli->sli_flag & LPFC_SLI_MBOX_ACTIVE) {
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"(%d):2532 Mailbox command x%x (x%x/x%x) "
				"cannot issue Data: x%x x%x\n",
				mboxq->vport ? mboxq->vport->vpi : 0,
				mboxq->u.mb.mbxCommand,
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				psli->sli_flag, MBX_POLL);
		return MBXERR_ERROR;
	}
	 
	psli->sli_flag |= LPFC_SLI_MBOX_ACTIVE;
	phba->sli.mbox_active = mboxq;
	spin_unlock_irqrestore(&phba->hbalock, iflag);

	 
	rc = lpfc_sli4_wait_bmbx_ready(phba, mboxq);
	if (rc)
		goto exit;
	 
	mbx_cmnd = bf_get(lpfc_mqe_command, mb);
	memset(phba->sli4_hba.bmbx.avirt, 0, sizeof(struct lpfc_bmbx_create));
	lpfc_sli4_pcimem_bcopy(mb, phba->sli4_hba.bmbx.avirt,
			       sizeof(struct lpfc_mqe));

	 
	dma_address = &phba->sli4_hba.bmbx.dma_address;
	writel(dma_address->addr_hi, phba->sli4_hba.BMBXregaddr);

	 
	rc = lpfc_sli4_wait_bmbx_ready(phba, mboxq);
	if (rc)
		goto exit;

	 
	writel(dma_address->addr_lo, phba->sli4_hba.BMBXregaddr);

	 
	rc = lpfc_sli4_wait_bmbx_ready(phba, mboxq);
	if (rc)
		goto exit;

	 
	lpfc_sli4_pcimem_bcopy(phba->sli4_hba.bmbx.avirt, mb,
			       sizeof(struct lpfc_mqe));
	mbox_rgn = (struct lpfc_bmbx_create *) phba->sli4_hba.bmbx.avirt;
	lpfc_sli4_pcimem_bcopy(&mbox_rgn->mcqe, &mboxq->mcqe,
			       sizeof(struct lpfc_mcqe));
	mcqe_status = bf_get(lpfc_mcqe_status, &mbox_rgn->mcqe);
	 
	if (mcqe_status != MB_CQE_STATUS_SUCCESS) {
		if (bf_get(lpfc_mqe_status, mb) == MBX_SUCCESS)
			bf_set(lpfc_mqe_status, mb,
			       (LPFC_MBX_ERROR_RANGE | mcqe_status));
		rc = MBXERR_ERROR;
	} else
		lpfc_sli4_swap_str(phba, mboxq);

	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
			"(%d):0356 Mailbox cmd x%x (x%x/x%x) Status x%x "
			"Data: x%x x%x x%x x%x x%x x%x x%x x%x x%x x%x x%x"
			" x%x x%x CQ: x%x x%x x%x x%x\n",
			mboxq->vport ? mboxq->vport->vpi : 0, mbx_cmnd,
			lpfc_sli_config_mbox_subsys_get(phba, mboxq),
			lpfc_sli_config_mbox_opcode_get(phba, mboxq),
			bf_get(lpfc_mqe_status, mb),
			mb->un.mb_words[0], mb->un.mb_words[1],
			mb->un.mb_words[2], mb->un.mb_words[3],
			mb->un.mb_words[4], mb->un.mb_words[5],
			mb->un.mb_words[6], mb->un.mb_words[7],
			mb->un.mb_words[8], mb->un.mb_words[9],
			mb->un.mb_words[10], mb->un.mb_words[11],
			mb->un.mb_words[12], mboxq->mcqe.word0,
			mboxq->mcqe.mcqe_tag0, 	mboxq->mcqe.mcqe_tag1,
			mboxq->mcqe.trailer);
exit:
	 
	spin_lock_irqsave(&phba->hbalock, iflag);
	psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
	phba->sli.mbox_active = NULL;
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return rc;
}

 
static int
lpfc_sli_issue_mbox_s4(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq,
		       uint32_t flag)
{
	struct lpfc_sli *psli = &phba->sli;
	unsigned long iflags;
	int rc;

	 
	lpfc_idiag_mbxacc_dump_issue_mbox(phba, &mboxq->u.mb);

	rc = lpfc_mbox_dev_check(phba);
	if (unlikely(rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"(%d):2544 Mailbox command x%x (x%x/x%x) "
				"cannot issue Data: x%x x%x\n",
				mboxq->vport ? mboxq->vport->vpi : 0,
				mboxq->u.mb.mbxCommand,
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				psli->sli_flag, flag);
		goto out_not_finished;
	}

	 
	if (!phba->sli4_hba.intr_enable) {
		if (flag == MBX_POLL)
			rc = lpfc_sli4_post_sync_mbox(phba, mboxq);
		else
			rc = -EIO;
		if (rc != MBX_SUCCESS)
			lpfc_printf_log(phba, KERN_WARNING, LOG_MBOX | LOG_SLI,
					"(%d):2541 Mailbox command x%x "
					"(x%x/x%x) failure: "
					"mqe_sta: x%x mcqe_sta: x%x/x%x "
					"Data: x%x x%x\n",
					mboxq->vport ? mboxq->vport->vpi : 0,
					mboxq->u.mb.mbxCommand,
					lpfc_sli_config_mbox_subsys_get(phba,
									mboxq),
					lpfc_sli_config_mbox_opcode_get(phba,
									mboxq),
					bf_get(lpfc_mqe_status, &mboxq->u.mqe),
					bf_get(lpfc_mcqe_status, &mboxq->mcqe),
					bf_get(lpfc_mcqe_ext_status,
					       &mboxq->mcqe),
					psli->sli_flag, flag);
		return rc;
	} else if (flag == MBX_POLL) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_MBOX | LOG_SLI,
				"(%d):2542 Try to issue mailbox command "
				"x%x (x%x/x%x) synchronously ahead of async "
				"mailbox command queue: x%x x%x\n",
				mboxq->vport ? mboxq->vport->vpi : 0,
				mboxq->u.mb.mbxCommand,
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				psli->sli_flag, flag);
		 
		rc = lpfc_sli4_async_mbox_block(phba);
		if (!rc) {
			 
			rc = lpfc_sli4_post_sync_mbox(phba, mboxq);
			if (rc != MBX_SUCCESS)
				lpfc_printf_log(phba, KERN_WARNING,
					LOG_MBOX | LOG_SLI,
					"(%d):2597 Sync Mailbox command "
					"x%x (x%x/x%x) failure: "
					"mqe_sta: x%x mcqe_sta: x%x/x%x "
					"Data: x%x x%x\n",
					mboxq->vport ? mboxq->vport->vpi : 0,
					mboxq->u.mb.mbxCommand,
					lpfc_sli_config_mbox_subsys_get(phba,
									mboxq),
					lpfc_sli_config_mbox_opcode_get(phba,
									mboxq),
					bf_get(lpfc_mqe_status, &mboxq->u.mqe),
					bf_get(lpfc_mcqe_status, &mboxq->mcqe),
					bf_get(lpfc_mcqe_ext_status,
					       &mboxq->mcqe),
					psli->sli_flag, flag);
			 
			lpfc_sli4_async_mbox_unblock(phba);
		}
		return rc;
	}

	 
	rc = lpfc_mbox_cmd_check(phba, mboxq);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"(%d):2543 Mailbox command x%x (x%x/x%x) "
				"cannot issue Data: x%x x%x\n",
				mboxq->vport ? mboxq->vport->vpi : 0,
				mboxq->u.mb.mbxCommand,
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				psli->sli_flag, flag);
		goto out_not_finished;
	}

	 
	psli->slistat.mbox_busy++;
	spin_lock_irqsave(&phba->hbalock, iflags);
	lpfc_mbox_put(phba, mboxq);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
			"(%d):0354 Mbox cmd issue - Enqueue Data: "
			"x%x (x%x/x%x) x%x x%x x%x\n",
			mboxq->vport ? mboxq->vport->vpi : 0xffffff,
			bf_get(lpfc_mqe_command, &mboxq->u.mqe),
			lpfc_sli_config_mbox_subsys_get(phba, mboxq),
			lpfc_sli_config_mbox_opcode_get(phba, mboxq),
			phba->pport->port_state,
			psli->sli_flag, MBX_NOWAIT);
	 
	lpfc_worker_wake_up(phba);

	return MBX_BUSY;

out_not_finished:
	return MBX_NOT_FINISHED;
}

 
int
lpfc_sli4_post_async_mbox(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	LPFC_MBOXQ_t *mboxq;
	int rc = MBX_SUCCESS;
	unsigned long iflags;
	struct lpfc_mqe *mqe;
	uint32_t mbx_cmnd;

	 
	if (unlikely(!phba->sli4_hba.intr_enable))
		return MBX_NOT_FINISHED;

	 
	spin_lock_irqsave(&phba->hbalock, iflags);
	if (unlikely(psli->sli_flag & LPFC_SLI_ASYNC_MBX_BLK)) {
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		return MBX_NOT_FINISHED;
	}
	if (psli->sli_flag & LPFC_SLI_MBOX_ACTIVE) {
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		return MBX_NOT_FINISHED;
	}
	if (unlikely(phba->sli.mbox_active)) {
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0384 There is pending active mailbox cmd\n");
		return MBX_NOT_FINISHED;
	}
	 
	psli->sli_flag |= LPFC_SLI_MBOX_ACTIVE;

	 
	mboxq = lpfc_mbox_get(phba);

	 
	if (!mboxq) {
		psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		return MBX_SUCCESS;
	}
	phba->sli.mbox_active = mboxq;
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	 
	rc = lpfc_mbox_dev_check(phba);
	if (unlikely(rc))
		 
		goto out_not_finished;

	 
	mqe = &mboxq->u.mqe;
	mbx_cmnd = bf_get(lpfc_mqe_command, mqe);

	 
	mod_timer(&psli->mbox_tmo, (jiffies +
		  msecs_to_jiffies(1000 * lpfc_mbox_tmo_val(phba, mboxq))));

	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
			"(%d):0355 Mailbox cmd x%x (x%x/x%x) issue Data: "
			"x%x x%x\n",
			mboxq->vport ? mboxq->vport->vpi : 0, mbx_cmnd,
			lpfc_sli_config_mbox_subsys_get(phba, mboxq),
			lpfc_sli_config_mbox_opcode_get(phba, mboxq),
			phba->pport->port_state, psli->sli_flag);

	if (mbx_cmnd != MBX_HEARTBEAT) {
		if (mboxq->vport) {
			lpfc_debugfs_disc_trc(mboxq->vport,
				LPFC_DISC_TRC_MBOX_VPORT,
				"MBOX Send vport: cmd:x%x mb:x%x x%x",
				mbx_cmnd, mqe->un.mb_words[0],
				mqe->un.mb_words[1]);
		} else {
			lpfc_debugfs_disc_trc(phba->pport,
				LPFC_DISC_TRC_MBOX,
				"MBOX Send: cmd:x%x mb:x%x x%x",
				mbx_cmnd, mqe->un.mb_words[0],
				mqe->un.mb_words[1]);
		}
	}
	psli->slistat.mbox_cmd++;

	 
	rc = lpfc_sli4_mq_put(phba->sli4_hba.mbx_wq, mqe);
	if (rc != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"(%d):2533 Mailbox command x%x (x%x/x%x) "
				"cannot issue Data: x%x x%x\n",
				mboxq->vport ? mboxq->vport->vpi : 0,
				mboxq->u.mb.mbxCommand,
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				psli->sli_flag, MBX_NOWAIT);
		goto out_not_finished;
	}

	return rc;

out_not_finished:
	spin_lock_irqsave(&phba->hbalock, iflags);
	if (phba->sli.mbox_active) {
		mboxq->u.mb.mbxStatus = MBX_NOT_FINISHED;
		__lpfc_mbox_cmpl_put(phba, mboxq);
		 
		psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
		phba->sli.mbox_active = NULL;
	}
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	return MBX_NOT_FINISHED;
}

 
int
lpfc_sli_issue_mbox(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmbox, uint32_t flag)
{
	return phba->lpfc_sli_issue_mbox(phba, pmbox, flag);
}

 
int
lpfc_mbox_api_table_setup(struct lpfc_hba *phba, uint8_t dev_grp)
{

	switch (dev_grp) {
	case LPFC_PCI_DEV_LP:
		phba->lpfc_sli_issue_mbox = lpfc_sli_issue_mbox_s3;
		phba->lpfc_sli_handle_slow_ring_event =
				lpfc_sli_handle_slow_ring_event_s3;
		phba->lpfc_sli_hbq_to_firmware = lpfc_sli_hbq_to_firmware_s3;
		phba->lpfc_sli_brdrestart = lpfc_sli_brdrestart_s3;
		phba->lpfc_sli_brdready = lpfc_sli_brdready_s3;
		break;
	case LPFC_PCI_DEV_OC:
		phba->lpfc_sli_issue_mbox = lpfc_sli_issue_mbox_s4;
		phba->lpfc_sli_handle_slow_ring_event =
				lpfc_sli_handle_slow_ring_event_s4;
		phba->lpfc_sli_hbq_to_firmware = lpfc_sli_hbq_to_firmware_s4;
		phba->lpfc_sli_brdrestart = lpfc_sli_brdrestart_s4;
		phba->lpfc_sli_brdready = lpfc_sli_brdready_s4;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1420 Invalid HBA PCI-device group: 0x%x\n",
				dev_grp);
		return -ENODEV;
	}
	return 0;
}

 
void
__lpfc_sli_ringtx_put(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
		    struct lpfc_iocbq *piocb)
{
	if (phba->sli_rev == LPFC_SLI_REV4)
		lockdep_assert_held(&pring->ring_lock);
	else
		lockdep_assert_held(&phba->hbalock);
	 
	list_add_tail(&piocb->list, &pring->txq);
}

 
static struct lpfc_iocbq *
lpfc_sli_next_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
		   struct lpfc_iocbq **piocb)
{
	struct lpfc_iocbq * nextiocb;

	lockdep_assert_held(&phba->hbalock);

	nextiocb = lpfc_sli_ringtx_get(phba, pring);
	if (!nextiocb) {
		nextiocb = *piocb;
		*piocb = NULL;
	}

	return nextiocb;
}

 
static int
__lpfc_sli_issue_iocb_s3(struct lpfc_hba *phba, uint32_t ring_number,
		    struct lpfc_iocbq *piocb, uint32_t flag)
{
	struct lpfc_iocbq *nextiocb;
	IOCB_t *iocb;
	struct lpfc_sli_ring *pring = &phba->sli.sli3_ring[ring_number];

	lockdep_assert_held(&phba->hbalock);

	if (piocb->cmd_cmpl && (!piocb->vport) &&
	   (piocb->iocb.ulpCommand != CMD_ABORT_XRI_CN) &&
	   (piocb->iocb.ulpCommand != CMD_CLOSE_XRI_CN)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"1807 IOCB x%x failed. No vport\n",
				piocb->iocb.ulpCommand);
		dump_stack();
		return IOCB_ERROR;
	}


	 
	if (unlikely(pci_channel_offline(phba->pcidev)))
		return IOCB_ERROR;

	 
	if (unlikely(phba->hba_flag & DEFER_ERATT))
		return IOCB_ERROR;

	 
	if (unlikely(phba->link_state < LPFC_LINK_DOWN))
		return IOCB_ERROR;

	 
	if (unlikely(pring->flag & LPFC_STOP_IOCB_EVENT))
		goto iocb_busy;

	if (unlikely(phba->link_state == LPFC_LINK_DOWN)) {
		 
		switch (piocb->iocb.ulpCommand) {
		case CMD_QUE_RING_BUF_CN:
		case CMD_QUE_RING_BUF64_CN:
			 
			if (piocb->cmd_cmpl)
				piocb->cmd_cmpl = NULL;
			fallthrough;
		case CMD_CREATE_XRI_CR:
		case CMD_CLOSE_XRI_CN:
		case CMD_CLOSE_XRI_CX:
			break;
		default:
			goto iocb_busy;
		}

	 
	} else if (unlikely(pring->ringno == LPFC_FCP_RING &&
			    !(phba->sli.sli_flag & LPFC_PROCESS_LA))) {
		goto iocb_busy;
	}

	while ((iocb = lpfc_sli_next_iocb_slot(phba, pring)) &&
	       (nextiocb = lpfc_sli_next_iocb(phba, pring, &piocb)))
		lpfc_sli_submit_iocb(phba, pring, iocb, nextiocb);

	if (iocb)
		lpfc_sli_update_ring(phba, pring);
	else
		lpfc_sli_update_full_ring(phba, pring);

	if (!piocb)
		return IOCB_SUCCESS;

	goto out_busy;

 iocb_busy:
	pring->stats.iocb_cmd_delay++;

 out_busy:

	if (!(flag & SLI_IOCB_RET_IOCB)) {
		__lpfc_sli_ringtx_put(phba, pring, piocb);
		return IOCB_SUCCESS;
	}

	return IOCB_BUSY;
}

 
static int
__lpfc_sli_issue_fcp_io_s3(struct lpfc_hba *phba, uint32_t ring_number,
			   struct lpfc_iocbq *piocb, uint32_t flag)
{
	unsigned long iflags;
	int rc;

	spin_lock_irqsave(&phba->hbalock, iflags);
	rc = __lpfc_sli_issue_iocb_s3(phba, ring_number, piocb, flag);
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	return rc;
}

 
static int
__lpfc_sli_issue_fcp_io_s4(struct lpfc_hba *phba, uint32_t ring_number,
			   struct lpfc_iocbq *piocb, uint32_t flag)
{
	struct lpfc_io_buf *lpfc_cmd = piocb->io_buf;

	lpfc_prep_embed_io(phba, lpfc_cmd);
	return lpfc_sli4_issue_wqe(phba, lpfc_cmd->hdwq, piocb);
}

void
lpfc_prep_embed_io(struct lpfc_hba *phba, struct lpfc_io_buf *lpfc_cmd)
{
	struct lpfc_iocbq *piocb = &lpfc_cmd->cur_iocbq;
	union lpfc_wqe128 *wqe = &lpfc_cmd->cur_iocbq.wqe;
	struct sli4_sge *sgl;

	 
	sgl = (struct sli4_sge *)lpfc_cmd->dma_sgl;

	if (phba->fcp_embed_io) {
		struct fcp_cmnd *fcp_cmnd;
		u32 *ptr;

		fcp_cmnd = lpfc_cmd->fcp_cmnd;

		 
		wqe->generic.bde.tus.f.bdeFlags =
			BUFF_TYPE_BDE_IMMED;
		wqe->generic.bde.tus.f.bdeSize = sgl->sge_len;
		wqe->generic.bde.addrHigh = 0;
		wqe->generic.bde.addrLow =  88;   

		bf_set(wqe_wqes, &wqe->fcp_iwrite.wqe_com, 1);
		bf_set(wqe_dbde, &wqe->fcp_iwrite.wqe_com, 0);

		 
		ptr = &wqe->words[22];
		memcpy(ptr, fcp_cmnd, sizeof(struct fcp_cmnd));
	} else {
		 
		wqe->generic.bde.tus.f.bdeFlags =  BUFF_TYPE_BDE_64;
		wqe->generic.bde.tus.f.bdeSize = sizeof(struct fcp_cmnd);
		wqe->generic.bde.addrHigh = sgl->addr_hi;
		wqe->generic.bde.addrLow =  sgl->addr_lo;

		 
		bf_set(wqe_dbde, &wqe->generic.wqe_com, 1);
		bf_set(wqe_wqes, &wqe->generic.wqe_com, 0);
	}

	 
	if (unlikely(piocb->cmd_flag & LPFC_IO_VMID)) {
		if (phba->pport->vmid_flag & LPFC_VMID_TYPE_PRIO) {
			bf_set(wqe_ccpe, &wqe->fcp_iwrite.wqe_com, 1);
			bf_set(wqe_ccp, &wqe->fcp_iwrite.wqe_com,
					(piocb->vmid_tag.cs_ctl_vmid));
		} else if (phba->cfg_vmid_app_header) {
			bf_set(wqe_appid, &wqe->fcp_iwrite.wqe_com, 1);
			bf_set(wqe_wqes, &wqe->fcp_iwrite.wqe_com, 1);
			wqe->words[31] = piocb->vmid_tag.app_id;
		}
	}
}

 
static int
__lpfc_sli_issue_iocb_s4(struct lpfc_hba *phba, uint32_t ring_number,
			 struct lpfc_iocbq *piocb, uint32_t flag)
{
	struct lpfc_sglq *sglq;
	union lpfc_wqe128 *wqe;
	struct lpfc_queue *wq;
	struct lpfc_sli_ring *pring;
	u32 ulp_command = get_job_cmnd(phba, piocb);

	 
	if ((piocb->cmd_flag & LPFC_IO_FCP) ||
	    (piocb->cmd_flag & LPFC_USE_FCPWQIDX)) {
		wq = phba->sli4_hba.hdwq[piocb->hba_wqidx].io_wq;
	} else {
		wq = phba->sli4_hba.els_wq;
	}

	 
	pring = wq->pring;

	 

	lockdep_assert_held(&pring->ring_lock);
	wqe = &piocb->wqe;
	if (piocb->sli4_xritag == NO_XRI) {
		if (ulp_command == CMD_ABORT_XRI_CX)
			sglq = NULL;
		else {
			sglq = __lpfc_sli_get_els_sglq(phba, piocb);
			if (!sglq) {
				if (!(flag & SLI_IOCB_RET_IOCB)) {
					__lpfc_sli_ringtx_put(phba,
							pring,
							piocb);
					return IOCB_SUCCESS;
				} else {
					return IOCB_BUSY;
				}
			}
		}
	} else if (piocb->cmd_flag &  LPFC_IO_FCP) {
		 
		sglq = NULL;
	}
	else {
		 
		sglq = __lpfc_get_active_sglq(phba, piocb->sli4_lxritag);
		if (!sglq)
			return IOCB_ERROR;
	}

	if (sglq) {
		piocb->sli4_lxritag = sglq->sli4_lxritag;
		piocb->sli4_xritag = sglq->sli4_xritag;

		 
		if (ulp_command == CMD_XMIT_BLS_RSP64_CX &&
		    piocb->abort_bls == LPFC_ABTS_UNSOL_INT)
			bf_set(xmit_bls_rsp64_rxid, &wqe->xmit_bls_rsp,
			       piocb->sli4_xritag);

		bf_set(wqe_xri_tag, &wqe->generic.wqe_com,
		       piocb->sli4_xritag);

		if (lpfc_wqe_bpl2sgl(phba, piocb, sglq) == NO_XRI)
			return IOCB_ERROR;
	}

	if (lpfc_sli4_wq_put(wq, wqe))
		return IOCB_ERROR;

	lpfc_sli_ringtxcmpl_put(phba, pring, piocb);

	return 0;
}

 
int
lpfc_sli_issue_fcp_io(struct lpfc_hba *phba, uint32_t ring_number,
		      struct lpfc_iocbq *piocb, uint32_t flag)
{
	return phba->__lpfc_sli_issue_fcp_io(phba, ring_number, piocb, flag);
}

 
int
__lpfc_sli_issue_iocb(struct lpfc_hba *phba, uint32_t ring_number,
		struct lpfc_iocbq *piocb, uint32_t flag)
{
	return phba->__lpfc_sli_issue_iocb(phba, ring_number, piocb, flag);
}

static void
__lpfc_sli_prep_els_req_rsp_s3(struct lpfc_iocbq *cmdiocbq,
			       struct lpfc_vport *vport,
			       struct lpfc_dmabuf *bmp, u16 cmd_size, u32 did,
			       u32 elscmd, u8 tmo, u8 expect_rsp)
{
	struct lpfc_hba *phba = vport->phba;
	IOCB_t *cmd;

	cmd = &cmdiocbq->iocb;
	memset(cmd, 0, sizeof(*cmd));

	cmd->un.elsreq64.bdl.addrHigh = putPaddrHigh(bmp->phys);
	cmd->un.elsreq64.bdl.addrLow = putPaddrLow(bmp->phys);
	cmd->un.elsreq64.bdl.bdeFlags = BUFF_TYPE_BLP_64;

	if (expect_rsp) {
		cmd->un.elsreq64.bdl.bdeSize = (2 * sizeof(struct ulp_bde64));
		cmd->un.elsreq64.remoteID = did;  
		cmd->ulpCommand = CMD_ELS_REQUEST64_CR;
		cmd->ulpTimeout = tmo;
	} else {
		cmd->un.elsreq64.bdl.bdeSize = sizeof(struct ulp_bde64);
		cmd->un.genreq64.xmit_els_remoteID = did;  
		cmd->ulpCommand = CMD_XMIT_ELS_RSP64_CX;
		cmd->ulpPU = PARM_NPIV_DID;
	}
	cmd->ulpBdeCount = 1;
	cmd->ulpLe = 1;
	cmd->ulpClass = CLASS3;

	 
	if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) {
		if (expect_rsp) {
			cmd->un.elsreq64.myID = vport->fc_myDID;

			 
			cmd->ulpContext = phba->vpi_ids[vport->vpi];
		}

		cmd->ulpCt_h = 0;
		 
		if (elscmd == ELS_CMD_ECHO)
			cmd->ulpCt_l = 0;  
		else
			cmd->ulpCt_l = 1;  
	}
}

static void
__lpfc_sli_prep_els_req_rsp_s4(struct lpfc_iocbq *cmdiocbq,
			       struct lpfc_vport *vport,
			       struct lpfc_dmabuf *bmp, u16 cmd_size, u32 did,
			       u32 elscmd, u8 tmo, u8 expect_rsp)
{
	struct lpfc_hba  *phba = vport->phba;
	union lpfc_wqe128 *wqe;
	struct ulp_bde64_le *bde;
	u8 els_id;

	wqe = &cmdiocbq->wqe;
	memset(wqe, 0, sizeof(*wqe));

	 
	bde = (struct ulp_bde64_le *)&wqe->generic.bde;
	bde->addr_low = cpu_to_le32(putPaddrLow(bmp->phys));
	bde->addr_high = cpu_to_le32(putPaddrHigh(bmp->phys));
	bde->type_size = cpu_to_le32(cmd_size);
	bde->type_size |= cpu_to_le32(ULP_BDE64_TYPE_BDE_64);

	if (expect_rsp) {
		bf_set(wqe_cmnd, &wqe->els_req.wqe_com, CMD_ELS_REQUEST64_WQE);

		 
		wqe->els_req.payload_len = cmd_size;
		wqe->els_req.max_response_payload_len = FCELSSIZE;

		 
		bf_set(wqe_els_did, &wqe->els_req.wqe_dest, did);

		 
		switch (elscmd) {
		case ELS_CMD_PLOGI:
			els_id = LPFC_ELS_ID_PLOGI;
			break;
		case ELS_CMD_FLOGI:
			els_id = LPFC_ELS_ID_FLOGI;
			break;
		case ELS_CMD_LOGO:
			els_id = LPFC_ELS_ID_LOGO;
			break;
		case ELS_CMD_FDISC:
			if (!vport->fc_myDID) {
				els_id = LPFC_ELS_ID_FDISC;
				break;
			}
			fallthrough;
		default:
			els_id = LPFC_ELS_ID_DEFAULT;
			break;
		}

		bf_set(wqe_els_id, &wqe->els_req.wqe_com, els_id);
	} else {
		 
		bf_set(wqe_els_did, &wqe->xmit_els_rsp.wqe_dest, did);

		 
		wqe->xmit_els_rsp.response_payload_len = cmd_size;

		bf_set(wqe_cmnd, &wqe->xmit_els_rsp.wqe_com,
		       CMD_XMIT_ELS_RSP64_WQE);
	}

	bf_set(wqe_tmo, &wqe->generic.wqe_com, tmo);
	bf_set(wqe_reqtag, &wqe->generic.wqe_com, cmdiocbq->iotag);
	bf_set(wqe_class, &wqe->generic.wqe_com, CLASS3);

	 
	if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) ||
	    (vport->fc_flag & FC_PT2PT)) {
		if (expect_rsp) {
			bf_set(els_req64_sid, &wqe->els_req, vport->fc_myDID);

			 
			bf_set(wqe_ctxt_tag, &wqe->els_req.wqe_com,
			       phba->vpi_ids[vport->vpi]);
		}

		 
		if (elscmd == ELS_CMD_ECHO)
			bf_set(wqe_ct, &wqe->generic.wqe_com, 0);
		else
			bf_set(wqe_ct, &wqe->generic.wqe_com, 1);
	}
}

void
lpfc_sli_prep_els_req_rsp(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocbq,
			  struct lpfc_vport *vport, struct lpfc_dmabuf *bmp,
			  u16 cmd_size, u32 did, u32 elscmd, u8 tmo,
			  u8 expect_rsp)
{
	phba->__lpfc_sli_prep_els_req_rsp(cmdiocbq, vport, bmp, cmd_size, did,
					  elscmd, tmo, expect_rsp);
}

static void
__lpfc_sli_prep_gen_req_s3(struct lpfc_iocbq *cmdiocbq, struct lpfc_dmabuf *bmp,
			   u16 rpi, u32 num_entry, u8 tmo)
{
	IOCB_t *cmd;

	cmd = &cmdiocbq->iocb;
	memset(cmd, 0, sizeof(*cmd));

	cmd->un.genreq64.bdl.addrHigh = putPaddrHigh(bmp->phys);
	cmd->un.genreq64.bdl.addrLow = putPaddrLow(bmp->phys);
	cmd->un.genreq64.bdl.bdeFlags = BUFF_TYPE_BLP_64;
	cmd->un.genreq64.bdl.bdeSize = num_entry * sizeof(struct ulp_bde64);

	cmd->un.genreq64.w5.hcsw.Rctl = FC_RCTL_DD_UNSOL_CTL;
	cmd->un.genreq64.w5.hcsw.Type = FC_TYPE_CT;
	cmd->un.genreq64.w5.hcsw.Fctl = (SI | LA);

	cmd->ulpContext = rpi;
	cmd->ulpClass = CLASS3;
	cmd->ulpCommand = CMD_GEN_REQUEST64_CR;
	cmd->ulpBdeCount = 1;
	cmd->ulpLe = 1;
	cmd->ulpOwner = OWN_CHIP;
	cmd->ulpTimeout = tmo;
}

static void
__lpfc_sli_prep_gen_req_s4(struct lpfc_iocbq *cmdiocbq, struct lpfc_dmabuf *bmp,
			   u16 rpi, u32 num_entry, u8 tmo)
{
	union lpfc_wqe128 *cmdwqe;
	struct ulp_bde64_le *bde, *bpl;
	u32 xmit_len = 0, total_len = 0, size, type, i;

	cmdwqe = &cmdiocbq->wqe;
	memset(cmdwqe, 0, sizeof(*cmdwqe));

	 
	bpl = (struct ulp_bde64_le *)bmp->virt;
	for (i = 0; i < num_entry; i++) {
		size = le32_to_cpu(bpl[i].type_size) & ULP_BDE64_SIZE_MASK;
		total_len += size;
	}
	for (i = 0; i < num_entry; i++) {
		size = le32_to_cpu(bpl[i].type_size) & ULP_BDE64_SIZE_MASK;
		type = le32_to_cpu(bpl[i].type_size) & ULP_BDE64_TYPE_MASK;
		if (type != ULP_BDE64_TYPE_BDE_64)
			break;
		xmit_len += size;
	}

	 
	bde = (struct ulp_bde64_le *)&cmdwqe->generic.bde;
	bde->addr_low = bpl->addr_low;
	bde->addr_high = bpl->addr_high;
	bde->type_size = cpu_to_le32(xmit_len);
	bde->type_size |= cpu_to_le32(ULP_BDE64_TYPE_BDE_64);

	 
	cmdwqe->gen_req.request_payload_len = xmit_len;

	 
	bf_set(wqe_type, &cmdwqe->gen_req.wge_ctl, FC_TYPE_CT);
	bf_set(wqe_rctl, &cmdwqe->gen_req.wge_ctl, FC_RCTL_DD_UNSOL_CTL);
	bf_set(wqe_si, &cmdwqe->gen_req.wge_ctl, 1);
	bf_set(wqe_la, &cmdwqe->gen_req.wge_ctl, 1);

	 
	bf_set(wqe_ctxt_tag, &cmdwqe->gen_req.wqe_com, rpi);

	 
	bf_set(wqe_tmo, &cmdwqe->gen_req.wqe_com, tmo);
	bf_set(wqe_class, &cmdwqe->gen_req.wqe_com, CLASS3);
	bf_set(wqe_cmnd, &cmdwqe->gen_req.wqe_com, CMD_GEN_REQUEST64_CR);
	bf_set(wqe_ct, &cmdwqe->gen_req.wqe_com, SLI4_CT_RPI);

	 
	cmdwqe->gen_req.max_response_payload_len = total_len - xmit_len;
}

void
lpfc_sli_prep_gen_req(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocbq,
		      struct lpfc_dmabuf *bmp, u16 rpi, u32 num_entry, u8 tmo)
{
	phba->__lpfc_sli_prep_gen_req(cmdiocbq, bmp, rpi, num_entry, tmo);
}

static void
__lpfc_sli_prep_xmit_seq64_s3(struct lpfc_iocbq *cmdiocbq,
			      struct lpfc_dmabuf *bmp, u16 rpi, u16 ox_id,
			      u32 num_entry, u8 rctl, u8 last_seq, u8 cr_cx_cmd)
{
	IOCB_t *icmd;

	icmd = &cmdiocbq->iocb;
	memset(icmd, 0, sizeof(*icmd));

	icmd->un.xseq64.bdl.addrHigh = putPaddrHigh(bmp->phys);
	icmd->un.xseq64.bdl.addrLow = putPaddrLow(bmp->phys);
	icmd->un.xseq64.bdl.bdeFlags = BUFF_TYPE_BLP_64;
	icmd->un.xseq64.bdl.bdeSize = (num_entry * sizeof(struct ulp_bde64));
	icmd->un.xseq64.w5.hcsw.Fctl = LA;
	if (last_seq)
		icmd->un.xseq64.w5.hcsw.Fctl |= LS;
	icmd->un.xseq64.w5.hcsw.Dfctl = 0;
	icmd->un.xseq64.w5.hcsw.Rctl = rctl;
	icmd->un.xseq64.w5.hcsw.Type = FC_TYPE_CT;

	icmd->ulpBdeCount = 1;
	icmd->ulpLe = 1;
	icmd->ulpClass = CLASS3;

	switch (cr_cx_cmd) {
	case CMD_XMIT_SEQUENCE64_CR:
		icmd->ulpContext = rpi;
		icmd->ulpCommand = CMD_XMIT_SEQUENCE64_CR;
		break;
	case CMD_XMIT_SEQUENCE64_CX:
		icmd->ulpContext = ox_id;
		icmd->ulpCommand = CMD_XMIT_SEQUENCE64_CX;
		break;
	default:
		break;
	}
}

static void
__lpfc_sli_prep_xmit_seq64_s4(struct lpfc_iocbq *cmdiocbq,
			      struct lpfc_dmabuf *bmp, u16 rpi, u16 ox_id,
			      u32 full_size, u8 rctl, u8 last_seq, u8 cr_cx_cmd)
{
	union lpfc_wqe128 *wqe;
	struct ulp_bde64 *bpl;

	wqe = &cmdiocbq->wqe;
	memset(wqe, 0, sizeof(*wqe));

	 
	bpl = (struct ulp_bde64 *)bmp->virt;
	wqe->xmit_sequence.bde.addrHigh = bpl->addrHigh;
	wqe->xmit_sequence.bde.addrLow = bpl->addrLow;
	wqe->xmit_sequence.bde.tus.w = bpl->tus.w;

	 
	bf_set(wqe_ls, &wqe->xmit_sequence.wge_ctl, last_seq);
	bf_set(wqe_la, &wqe->xmit_sequence.wge_ctl, 1);
	bf_set(wqe_dfctl, &wqe->xmit_sequence.wge_ctl, 0);
	bf_set(wqe_rctl, &wqe->xmit_sequence.wge_ctl, rctl);
	bf_set(wqe_type, &wqe->xmit_sequence.wge_ctl, FC_TYPE_CT);

	 
	bf_set(wqe_ctxt_tag, &wqe->xmit_sequence.wqe_com, rpi);

	bf_set(wqe_cmnd, &wqe->xmit_sequence.wqe_com,
	       CMD_XMIT_SEQUENCE64_WQE);

	 
	bf_set(wqe_class, &wqe->xmit_sequence.wqe_com, CLASS3);

	 
	bf_set(wqe_rcvoxid, &wqe->xmit_sequence.wqe_com, ox_id);

	 
	if (cmdiocbq->cmd_flag & (LPFC_IO_LIBDFC | LPFC_IO_LOOPBACK))
		wqe->xmit_sequence.xmit_len = full_size;
	else
		wqe->xmit_sequence.xmit_len =
			wqe->xmit_sequence.bde.tus.f.bdeSize;
}

void
lpfc_sli_prep_xmit_seq64(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocbq,
			 struct lpfc_dmabuf *bmp, u16 rpi, u16 ox_id,
			 u32 num_entry, u8 rctl, u8 last_seq, u8 cr_cx_cmd)
{
	phba->__lpfc_sli_prep_xmit_seq64(cmdiocbq, bmp, rpi, ox_id, num_entry,
					 rctl, last_seq, cr_cx_cmd);
}

static void
__lpfc_sli_prep_abort_xri_s3(struct lpfc_iocbq *cmdiocbq, u16 ulp_context,
			     u16 iotag, u8 ulp_class, u16 cqid, bool ia,
			     bool wqec)
{
	IOCB_t *icmd = NULL;

	icmd = &cmdiocbq->iocb;
	memset(icmd, 0, sizeof(*icmd));

	 
	icmd->un.acxri.abortContextTag = ulp_context;
	icmd->un.acxri.abortIoTag = iotag;

	if (ia) {
		 
		icmd->ulpCommand = CMD_CLOSE_XRI_CN;
	} else {
		 
		icmd->un.acxri.abortType = ABORT_TYPE_ABTS;

		 
		icmd->ulpClass = ulp_class;
		icmd->ulpCommand = CMD_ABORT_XRI_CN;
	}

	 
	icmd->ulpLe = 1;
}

static void
__lpfc_sli_prep_abort_xri_s4(struct lpfc_iocbq *cmdiocbq, u16 ulp_context,
			     u16 iotag, u8 ulp_class, u16 cqid, bool ia,
			     bool wqec)
{
	union lpfc_wqe128 *wqe;

	wqe = &cmdiocbq->wqe;
	memset(wqe, 0, sizeof(*wqe));

	 
	bf_set(abort_cmd_criteria, &wqe->abort_cmd, T_XRI_TAG);
	if (ia)
		bf_set(abort_cmd_ia, &wqe->abort_cmd, 1);
	else
		bf_set(abort_cmd_ia, &wqe->abort_cmd, 0);

	 
	bf_set(wqe_cmnd, &wqe->abort_cmd.wqe_com, CMD_ABORT_XRI_WQE);

	 
	wqe->abort_cmd.wqe_com.abort_tag = ulp_context;

	 
	bf_set(wqe_reqtag, &wqe->abort_cmd.wqe_com, iotag);

	 
	bf_set(wqe_qosd, &wqe->abort_cmd.wqe_com, 1);

	 
	if (wqec)
		bf_set(wqe_wqec, &wqe->abort_cmd.wqe_com, 1);
	bf_set(wqe_cqid, &wqe->abort_cmd.wqe_com, cqid);
	bf_set(wqe_cmd_type, &wqe->abort_cmd.wqe_com, OTHER_COMMAND);
}

void
lpfc_sli_prep_abort_xri(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocbq,
			u16 ulp_context, u16 iotag, u8 ulp_class, u16 cqid,
			bool ia, bool wqec)
{
	phba->__lpfc_sli_prep_abort_xri(cmdiocbq, ulp_context, iotag, ulp_class,
					cqid, ia, wqec);
}

 
int
lpfc_sli_api_table_setup(struct lpfc_hba *phba, uint8_t dev_grp)
{

	switch (dev_grp) {
	case LPFC_PCI_DEV_LP:
		phba->__lpfc_sli_issue_iocb = __lpfc_sli_issue_iocb_s3;
		phba->__lpfc_sli_release_iocbq = __lpfc_sli_release_iocbq_s3;
		phba->__lpfc_sli_issue_fcp_io = __lpfc_sli_issue_fcp_io_s3;
		phba->__lpfc_sli_prep_els_req_rsp = __lpfc_sli_prep_els_req_rsp_s3;
		phba->__lpfc_sli_prep_gen_req = __lpfc_sli_prep_gen_req_s3;
		phba->__lpfc_sli_prep_xmit_seq64 = __lpfc_sli_prep_xmit_seq64_s3;
		phba->__lpfc_sli_prep_abort_xri = __lpfc_sli_prep_abort_xri_s3;
		break;
	case LPFC_PCI_DEV_OC:
		phba->__lpfc_sli_issue_iocb = __lpfc_sli_issue_iocb_s4;
		phba->__lpfc_sli_release_iocbq = __lpfc_sli_release_iocbq_s4;
		phba->__lpfc_sli_issue_fcp_io = __lpfc_sli_issue_fcp_io_s4;
		phba->__lpfc_sli_prep_els_req_rsp = __lpfc_sli_prep_els_req_rsp_s4;
		phba->__lpfc_sli_prep_gen_req = __lpfc_sli_prep_gen_req_s4;
		phba->__lpfc_sli_prep_xmit_seq64 = __lpfc_sli_prep_xmit_seq64_s4;
		phba->__lpfc_sli_prep_abort_xri = __lpfc_sli_prep_abort_xri_s4;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1419 Invalid HBA PCI-device group: 0x%x\n",
				dev_grp);
		return -ENODEV;
	}
	return 0;
}

 
struct lpfc_sli_ring *
lpfc_sli4_calc_ring(struct lpfc_hba *phba, struct lpfc_iocbq *piocb)
{
	struct lpfc_io_buf *lpfc_cmd;

	if (piocb->cmd_flag & (LPFC_IO_FCP | LPFC_USE_FCPWQIDX)) {
		if (unlikely(!phba->sli4_hba.hdwq))
			return NULL;
		 
		if (!(piocb->cmd_flag & LPFC_USE_FCPWQIDX)) {
			lpfc_cmd = piocb->io_buf;
			piocb->hba_wqidx = lpfc_cmd->hdwq_no;
		}
		return phba->sli4_hba.hdwq[piocb->hba_wqidx].io_wq->pring;
	} else {
		if (unlikely(!phba->sli4_hba.els_wq))
			return NULL;
		piocb->hba_wqidx = 0;
		return phba->sli4_hba.els_wq->pring;
	}
}

inline void lpfc_sli4_poll_eq(struct lpfc_queue *eq)
{
	struct lpfc_hba *phba = eq->phba;

	 
	smp_rmb();

	if (READ_ONCE(eq->mode) == LPFC_EQ_POLL)
		 
		lpfc_sli4_process_eq(phba, eq, LPFC_QUEUE_NOARM,
				     LPFC_QUEUE_WORK);
}

 
int
lpfc_sli_issue_iocb(struct lpfc_hba *phba, uint32_t ring_number,
		    struct lpfc_iocbq *piocb, uint32_t flag)
{
	struct lpfc_sli_ring *pring;
	struct lpfc_queue *eq;
	unsigned long iflags;
	int rc;

	 
	if (unlikely(pci_channel_offline(phba->pcidev)))
		return IOCB_ERROR;

	if (phba->sli_rev == LPFC_SLI_REV4) {
		lpfc_sli_prep_wqe(phba, piocb);

		eq = phba->sli4_hba.hdwq[piocb->hba_wqidx].hba_eq;

		pring = lpfc_sli4_calc_ring(phba, piocb);
		if (unlikely(pring == NULL))
			return IOCB_ERROR;

		spin_lock_irqsave(&pring->ring_lock, iflags);
		rc = __lpfc_sli_issue_iocb(phba, ring_number, piocb, flag);
		spin_unlock_irqrestore(&pring->ring_lock, iflags);

		lpfc_sli4_poll_eq(eq);
	} else {
		 
		spin_lock_irqsave(&phba->hbalock, iflags);
		rc = __lpfc_sli_issue_iocb(phba, ring_number, piocb, flag);
		spin_unlock_irqrestore(&phba->hbalock, iflags);
	}
	return rc;
}

 
static int
lpfc_extra_ring_setup( struct lpfc_hba *phba)
{
	struct lpfc_sli *psli;
	struct lpfc_sli_ring *pring;

	psli = &phba->sli;

	 

	 
	pring = &psli->sli3_ring[LPFC_FCP_RING];
	pring->sli.sli3.numCiocb -= SLI2_IOCB_CMD_R1XTRA_ENTRIES;
	pring->sli.sli3.numRiocb -= SLI2_IOCB_RSP_R1XTRA_ENTRIES;
	pring->sli.sli3.numCiocb -= SLI2_IOCB_CMD_R3XTRA_ENTRIES;
	pring->sli.sli3.numRiocb -= SLI2_IOCB_RSP_R3XTRA_ENTRIES;

	 
	pring = &psli->sli3_ring[LPFC_EXTRA_RING];

	pring->sli.sli3.numCiocb += SLI2_IOCB_CMD_R1XTRA_ENTRIES;
	pring->sli.sli3.numRiocb += SLI2_IOCB_RSP_R1XTRA_ENTRIES;
	pring->sli.sli3.numCiocb += SLI2_IOCB_CMD_R3XTRA_ENTRIES;
	pring->sli.sli3.numRiocb += SLI2_IOCB_RSP_R3XTRA_ENTRIES;

	 
	pring->iotag_max = 4096;
	pring->num_mask = 1;
	pring->prt[0].profile = 0;       
	pring->prt[0].rctl = phba->cfg_multi_ring_rctl;
	pring->prt[0].type = phba->cfg_multi_ring_type;
	pring->prt[0].lpfc_sli_rcv_unsol_event = NULL;
	return 0;
}

static void
lpfc_sli_post_recovery_event(struct lpfc_hba *phba,
			     struct lpfc_nodelist *ndlp)
{
	unsigned long iflags;
	struct lpfc_work_evt  *evtp = &ndlp->recovery_evt;

	spin_lock_irqsave(&phba->hbalock, iflags);
	if (!list_empty(&evtp->evt_listp)) {
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		return;
	}

	 
	evtp->evt_arg1  = lpfc_nlp_get(ndlp);
	if (!evtp->evt_arg1) {
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		return;
	}
	evtp->evt = LPFC_EVT_RECOVER_PORT;
	list_add_tail(&evtp->evt_listp, &phba->work_list);
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	lpfc_worker_wake_up(phba);
}

 
static void
lpfc_sli_abts_err_handler(struct lpfc_hba *phba,
			  struct lpfc_iocbq *iocbq)
{
	struct lpfc_nodelist *ndlp = NULL;
	uint16_t rpi = 0, vpi = 0;
	struct lpfc_vport *vport = NULL;

	 
	vpi = iocbq->iocb.un.asyncstat.sub_ctxt_tag;
	rpi = iocbq->iocb.ulpContext;

	lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
			"3092 Port generated ABTS async event "
			"on vpi %d rpi %d status 0x%x\n",
			vpi, rpi, iocbq->iocb.ulpStatus);

	vport = lpfc_find_vport_by_vpid(phba, vpi);
	if (!vport)
		goto err_exit;
	ndlp = lpfc_findnode_rpi(vport, rpi);
	if (!ndlp)
		goto err_exit;

	if (iocbq->iocb.ulpStatus == IOSTAT_LOCAL_REJECT)
		lpfc_sli_abts_recover_port(vport, ndlp);
	return;

 err_exit:
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"3095 Event Context not found, no "
			"action on vpi %d rpi %d status 0x%x, reason 0x%x\n",
			vpi, rpi, iocbq->iocb.ulpStatus,
			iocbq->iocb.ulpContext);
}

 
void
lpfc_sli4_abts_err_handler(struct lpfc_hba *phba,
			   struct lpfc_nodelist *ndlp,
			   struct sli4_wcqe_xri_aborted *axri)
{
	uint32_t ext_status = 0;

	if (!ndlp) {
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"3115 Node Context not found, driver "
				"ignoring abts err event\n");
		return;
	}

	lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
			"3116 Port generated FCP XRI ABORT event on "
			"vpi %d rpi %d xri x%x status 0x%x parameter x%x\n",
			ndlp->vport->vpi, phba->sli4_hba.rpi_ids[ndlp->nlp_rpi],
			bf_get(lpfc_wcqe_xa_xri, axri),
			bf_get(lpfc_wcqe_xa_status, axri),
			axri->parameter);

	 
	ext_status = axri->parameter & IOERR_PARAM_MASK;
	if ((bf_get(lpfc_wcqe_xa_status, axri) == IOSTAT_LOCAL_REJECT) &&
	    ((ext_status == IOERR_SEQUENCE_TIMEOUT) || (ext_status == 0)))
		lpfc_sli_post_recovery_event(phba, ndlp);
}

 
static void
lpfc_sli_async_event_handler(struct lpfc_hba * phba,
	struct lpfc_sli_ring * pring, struct lpfc_iocbq * iocbq)
{
	IOCB_t *icmd;
	uint16_t evt_code;
	struct temp_event temp_event_data;
	struct Scsi_Host *shost;
	uint32_t *iocb_w;

	icmd = &iocbq->iocb;
	evt_code = icmd->un.asyncstat.evt_code;

	switch (evt_code) {
	case ASYNC_TEMP_WARN:
	case ASYNC_TEMP_SAFE:
		temp_event_data.data = (uint32_t) icmd->ulpContext;
		temp_event_data.event_type = FC_REG_TEMPERATURE_EVENT;
		if (evt_code == ASYNC_TEMP_WARN) {
			temp_event_data.event_code = LPFC_THRESHOLD_TEMP;
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0347 Adapter is very hot, please take "
				"corrective action. temperature : %d Celsius\n",
				(uint32_t) icmd->ulpContext);
		} else {
			temp_event_data.event_code = LPFC_NORMAL_TEMP;
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0340 Adapter temperature is OK now. "
				"temperature : %d Celsius\n",
				(uint32_t) icmd->ulpContext);
		}

		 
		shost = lpfc_shost_from_vport(phba->pport);
		fc_host_post_vendor_event(shost, fc_get_event_number(),
			sizeof(temp_event_data), (char *) &temp_event_data,
			LPFC_NL_VENDOR_ID);
		break;
	case ASYNC_STATUS_CN:
		lpfc_sli_abts_err_handler(phba, iocbq);
		break;
	default:
		iocb_w = (uint32_t *) icmd;
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"0346 Ring %d handler: unexpected ASYNC_STATUS"
			" evt_code 0x%x\n"
			"W0  0x%08x W1  0x%08x W2  0x%08x W3  0x%08x\n"
			"W4  0x%08x W5  0x%08x W6  0x%08x W7  0x%08x\n"
			"W8  0x%08x W9  0x%08x W10 0x%08x W11 0x%08x\n"
			"W12 0x%08x W13 0x%08x W14 0x%08x W15 0x%08x\n",
			pring->ringno, icmd->un.asyncstat.evt_code,
			iocb_w[0], iocb_w[1], iocb_w[2], iocb_w[3],
			iocb_w[4], iocb_w[5], iocb_w[6], iocb_w[7],
			iocb_w[8], iocb_w[9], iocb_w[10], iocb_w[11],
			iocb_w[12], iocb_w[13], iocb_w[14], iocb_w[15]);

		break;
	}
}


 
int
lpfc_sli4_setup(struct lpfc_hba *phba)
{
	struct lpfc_sli_ring *pring;

	pring = phba->sli4_hba.els_wq->pring;
	pring->num_mask = LPFC_MAX_RING_MASK;
	pring->prt[0].profile = 0;	 
	pring->prt[0].rctl = FC_RCTL_ELS_REQ;
	pring->prt[0].type = FC_TYPE_ELS;
	pring->prt[0].lpfc_sli_rcv_unsol_event =
	    lpfc_els_unsol_event;
	pring->prt[1].profile = 0;	 
	pring->prt[1].rctl = FC_RCTL_ELS_REP;
	pring->prt[1].type = FC_TYPE_ELS;
	pring->prt[1].lpfc_sli_rcv_unsol_event =
	    lpfc_els_unsol_event;
	pring->prt[2].profile = 0;	 
	 
	pring->prt[2].rctl = FC_RCTL_DD_UNSOL_CTL;
	 
	pring->prt[2].type = FC_TYPE_CT;
	pring->prt[2].lpfc_sli_rcv_unsol_event =
	    lpfc_ct_unsol_event;
	pring->prt[3].profile = 0;	 
	 
	pring->prt[3].rctl = FC_RCTL_DD_SOL_CTL;
	 
	pring->prt[3].type = FC_TYPE_CT;
	pring->prt[3].lpfc_sli_rcv_unsol_event =
	    lpfc_ct_unsol_event;
	return 0;
}

 
int
lpfc_sli_setup(struct lpfc_hba *phba)
{
	int i, totiocbsize = 0;
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring *pring;

	psli->num_rings = MAX_SLI3_CONFIGURED_RINGS;
	psli->sli_flag = 0;

	psli->iocbq_lookup = NULL;
	psli->iocbq_lookup_len = 0;
	psli->last_iotag = 0;

	for (i = 0; i < psli->num_rings; i++) {
		pring = &psli->sli3_ring[i];
		switch (i) {
		case LPFC_FCP_RING:	 
			 
			pring->sli.sli3.numCiocb = SLI2_IOCB_CMD_R0_ENTRIES;
			pring->sli.sli3.numRiocb = SLI2_IOCB_RSP_R0_ENTRIES;
			pring->sli.sli3.numCiocb +=
				SLI2_IOCB_CMD_R1XTRA_ENTRIES;
			pring->sli.sli3.numRiocb +=
				SLI2_IOCB_RSP_R1XTRA_ENTRIES;
			pring->sli.sli3.numCiocb +=
				SLI2_IOCB_CMD_R3XTRA_ENTRIES;
			pring->sli.sli3.numRiocb +=
				SLI2_IOCB_RSP_R3XTRA_ENTRIES;
			pring->sli.sli3.sizeCiocb = (phba->sli_rev == 3) ?
							SLI3_IOCB_CMD_SIZE :
							SLI2_IOCB_CMD_SIZE;
			pring->sli.sli3.sizeRiocb = (phba->sli_rev == 3) ?
							SLI3_IOCB_RSP_SIZE :
							SLI2_IOCB_RSP_SIZE;
			pring->iotag_ctr = 0;
			pring->iotag_max =
			    (phba->cfg_hba_queue_depth * 2);
			pring->fast_iotag = pring->iotag_max;
			pring->num_mask = 0;
			break;
		case LPFC_EXTRA_RING:	 
			 
			pring->sli.sli3.numCiocb = SLI2_IOCB_CMD_R1_ENTRIES;
			pring->sli.sli3.numRiocb = SLI2_IOCB_RSP_R1_ENTRIES;
			pring->sli.sli3.sizeCiocb = (phba->sli_rev == 3) ?
							SLI3_IOCB_CMD_SIZE :
							SLI2_IOCB_CMD_SIZE;
			pring->sli.sli3.sizeRiocb = (phba->sli_rev == 3) ?
							SLI3_IOCB_RSP_SIZE :
							SLI2_IOCB_RSP_SIZE;
			pring->iotag_max = phba->cfg_hba_queue_depth;
			pring->num_mask = 0;
			break;
		case LPFC_ELS_RING:	 
			 
			pring->sli.sli3.numCiocb = SLI2_IOCB_CMD_R2_ENTRIES;
			pring->sli.sli3.numRiocb = SLI2_IOCB_RSP_R2_ENTRIES;
			pring->sli.sli3.sizeCiocb = (phba->sli_rev == 3) ?
							SLI3_IOCB_CMD_SIZE :
							SLI2_IOCB_CMD_SIZE;
			pring->sli.sli3.sizeRiocb = (phba->sli_rev == 3) ?
							SLI3_IOCB_RSP_SIZE :
							SLI2_IOCB_RSP_SIZE;
			pring->fast_iotag = 0;
			pring->iotag_ctr = 0;
			pring->iotag_max = 4096;
			pring->lpfc_sli_rcv_async_status =
				lpfc_sli_async_event_handler;
			pring->num_mask = LPFC_MAX_RING_MASK;
			pring->prt[0].profile = 0;	 
			pring->prt[0].rctl = FC_RCTL_ELS_REQ;
			pring->prt[0].type = FC_TYPE_ELS;
			pring->prt[0].lpfc_sli_rcv_unsol_event =
			    lpfc_els_unsol_event;
			pring->prt[1].profile = 0;	 
			pring->prt[1].rctl = FC_RCTL_ELS_REP;
			pring->prt[1].type = FC_TYPE_ELS;
			pring->prt[1].lpfc_sli_rcv_unsol_event =
			    lpfc_els_unsol_event;
			pring->prt[2].profile = 0;	 
			 
			pring->prt[2].rctl = FC_RCTL_DD_UNSOL_CTL;
			 
			pring->prt[2].type = FC_TYPE_CT;
			pring->prt[2].lpfc_sli_rcv_unsol_event =
			    lpfc_ct_unsol_event;
			pring->prt[3].profile = 0;	 
			 
			pring->prt[3].rctl = FC_RCTL_DD_SOL_CTL;
			 
			pring->prt[3].type = FC_TYPE_CT;
			pring->prt[3].lpfc_sli_rcv_unsol_event =
			    lpfc_ct_unsol_event;
			break;
		}
		totiocbsize += (pring->sli.sli3.numCiocb *
			pring->sli.sli3.sizeCiocb) +
			(pring->sli.sli3.numRiocb * pring->sli.sli3.sizeRiocb);
	}
	if (totiocbsize > MAX_SLIM_IOCB_SIZE) {
		 
		printk(KERN_ERR "%d:0462 Too many cmd / rsp ring entries in "
		       "SLI2 SLIM Data: x%x x%lx\n",
		       phba->brd_no, totiocbsize,
		       (unsigned long) MAX_SLIM_IOCB_SIZE);
	}
	if (phba->cfg_multi_ring_support == 2)
		lpfc_extra_ring_setup(phba);

	return 0;
}

 
void
lpfc_sli4_queue_init(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli;
	struct lpfc_sli_ring *pring;
	int i;

	psli = &phba->sli;
	spin_lock_irq(&phba->hbalock);
	INIT_LIST_HEAD(&psli->mboxq);
	INIT_LIST_HEAD(&psli->mboxq_cmpl);
	 
	for (i = 0; i < phba->cfg_hdw_queue; i++) {
		pring = phba->sli4_hba.hdwq[i].io_wq->pring;
		pring->flag = 0;
		pring->ringno = LPFC_FCP_RING;
		pring->txcmplq_cnt = 0;
		INIT_LIST_HEAD(&pring->txq);
		INIT_LIST_HEAD(&pring->txcmplq);
		INIT_LIST_HEAD(&pring->iocb_continueq);
		spin_lock_init(&pring->ring_lock);
	}
	pring = phba->sli4_hba.els_wq->pring;
	pring->flag = 0;
	pring->ringno = LPFC_ELS_RING;
	pring->txcmplq_cnt = 0;
	INIT_LIST_HEAD(&pring->txq);
	INIT_LIST_HEAD(&pring->txcmplq);
	INIT_LIST_HEAD(&pring->iocb_continueq);
	spin_lock_init(&pring->ring_lock);

	if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME) {
		pring = phba->sli4_hba.nvmels_wq->pring;
		pring->flag = 0;
		pring->ringno = LPFC_ELS_RING;
		pring->txcmplq_cnt = 0;
		INIT_LIST_HEAD(&pring->txq);
		INIT_LIST_HEAD(&pring->txcmplq);
		INIT_LIST_HEAD(&pring->iocb_continueq);
		spin_lock_init(&pring->ring_lock);
	}

	spin_unlock_irq(&phba->hbalock);
}

 
void
lpfc_sli_queue_init(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli;
	struct lpfc_sli_ring *pring;
	int i;

	psli = &phba->sli;
	spin_lock_irq(&phba->hbalock);
	INIT_LIST_HEAD(&psli->mboxq);
	INIT_LIST_HEAD(&psli->mboxq_cmpl);
	 
	for (i = 0; i < psli->num_rings; i++) {
		pring = &psli->sli3_ring[i];
		pring->ringno = i;
		pring->sli.sli3.next_cmdidx  = 0;
		pring->sli.sli3.local_getidx = 0;
		pring->sli.sli3.cmdidx = 0;
		INIT_LIST_HEAD(&pring->iocb_continueq);
		INIT_LIST_HEAD(&pring->iocb_continue_saveq);
		INIT_LIST_HEAD(&pring->postbufq);
		pring->flag = 0;
		INIT_LIST_HEAD(&pring->txq);
		INIT_LIST_HEAD(&pring->txcmplq);
		spin_lock_init(&pring->ring_lock);
	}
	spin_unlock_irq(&phba->hbalock);
}

 
static void
lpfc_sli_mbox_sys_flush(struct lpfc_hba *phba)
{
	LIST_HEAD(completions);
	struct lpfc_sli *psli = &phba->sli;
	LPFC_MBOXQ_t *pmb;
	unsigned long iflag;

	 
	local_bh_disable();

	 
	spin_lock_irqsave(&phba->hbalock, iflag);

	 
	list_splice_init(&phba->sli.mboxq, &completions);
	 
	if (psli->mbox_active) {
		list_add_tail(&psli->mbox_active->list, &completions);
		psli->mbox_active = NULL;
		psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
	}
	 
	list_splice_init(&phba->sli.mboxq_cmpl, &completions);
	spin_unlock_irqrestore(&phba->hbalock, iflag);

	 
	local_bh_enable();

	 
	while (!list_empty(&completions)) {
		list_remove_head(&completions, pmb, LPFC_MBOXQ_t, list);
		pmb->u.mb.mbxStatus = MBX_NOT_FINISHED;
		if (pmb->mbox_cmpl)
			pmb->mbox_cmpl(phba, pmb);
	}
}

 
int
lpfc_sli_host_down(struct lpfc_vport *vport)
{
	LIST_HEAD(completions);
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_queue *qp = NULL;
	struct lpfc_sli_ring *pring;
	struct lpfc_iocbq *iocb, *next_iocb;
	int i;
	unsigned long flags = 0;
	uint16_t prev_pring_flag;

	lpfc_cleanup_discovery_resources(vport);

	spin_lock_irqsave(&phba->hbalock, flags);

	 
	if (phba->sli_rev != LPFC_SLI_REV4) {
		for (i = 0; i < psli->num_rings; i++) {
			pring = &psli->sli3_ring[i];
			prev_pring_flag = pring->flag;
			 
			if (pring->ringno == LPFC_ELS_RING) {
				pring->flag |= LPFC_DEFERRED_RING_EVENT;
				 
				set_bit(LPFC_DATA_READY, &phba->data_flags);
			}
			list_for_each_entry_safe(iocb, next_iocb,
						 &pring->txq, list) {
				if (iocb->vport != vport)
					continue;
				list_move_tail(&iocb->list, &completions);
			}
			list_for_each_entry_safe(iocb, next_iocb,
						 &pring->txcmplq, list) {
				if (iocb->vport != vport)
					continue;
				lpfc_sli_issue_abort_iotag(phba, pring, iocb,
							   NULL);
			}
			pring->flag = prev_pring_flag;
		}
	} else {
		list_for_each_entry(qp, &phba->sli4_hba.lpfc_wq_list, wq_list) {
			pring = qp->pring;
			if (!pring)
				continue;
			if (pring == phba->sli4_hba.els_wq->pring) {
				pring->flag |= LPFC_DEFERRED_RING_EVENT;
				 
				set_bit(LPFC_DATA_READY, &phba->data_flags);
			}
			prev_pring_flag = pring->flag;
			spin_lock(&pring->ring_lock);
			list_for_each_entry_safe(iocb, next_iocb,
						 &pring->txq, list) {
				if (iocb->vport != vport)
					continue;
				list_move_tail(&iocb->list, &completions);
			}
			spin_unlock(&pring->ring_lock);
			list_for_each_entry_safe(iocb, next_iocb,
						 &pring->txcmplq, list) {
				if (iocb->vport != vport)
					continue;
				lpfc_sli_issue_abort_iotag(phba, pring, iocb,
							   NULL);
			}
			pring->flag = prev_pring_flag;
		}
	}
	spin_unlock_irqrestore(&phba->hbalock, flags);

	 
	lpfc_issue_hb_tmo(phba);

	 
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_DOWN);
	return 1;
}

 
int
lpfc_sli_hba_down(struct lpfc_hba *phba)
{
	LIST_HEAD(completions);
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_queue *qp = NULL;
	struct lpfc_sli_ring *pring;
	struct lpfc_dmabuf *buf_ptr;
	unsigned long flags = 0;
	int i;

	 
	lpfc_sli_mbox_sys_shutdown(phba, LPFC_MBX_WAIT);

	lpfc_hba_down_prep(phba);

	 
	local_bh_disable();

	lpfc_fabric_abort_hba(phba);

	spin_lock_irqsave(&phba->hbalock, flags);

	 
	if (phba->sli_rev != LPFC_SLI_REV4) {
		for (i = 0; i < psli->num_rings; i++) {
			pring = &psli->sli3_ring[i];
			 
			if (pring->ringno == LPFC_ELS_RING) {
				pring->flag |= LPFC_DEFERRED_RING_EVENT;
				 
				set_bit(LPFC_DATA_READY, &phba->data_flags);
			}
			list_splice_init(&pring->txq, &completions);
		}
	} else {
		list_for_each_entry(qp, &phba->sli4_hba.lpfc_wq_list, wq_list) {
			pring = qp->pring;
			if (!pring)
				continue;
			spin_lock(&pring->ring_lock);
			list_splice_init(&pring->txq, &completions);
			spin_unlock(&pring->ring_lock);
			if (pring == phba->sli4_hba.els_wq->pring) {
				pring->flag |= LPFC_DEFERRED_RING_EVENT;
				 
				set_bit(LPFC_DATA_READY, &phba->data_flags);
			}
		}
	}
	spin_unlock_irqrestore(&phba->hbalock, flags);

	 
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_DOWN);

	spin_lock_irqsave(&phba->hbalock, flags);
	list_splice_init(&phba->elsbuf, &completions);
	phba->elsbuf_cnt = 0;
	phba->elsbuf_prev_cnt = 0;
	spin_unlock_irqrestore(&phba->hbalock, flags);

	while (!list_empty(&completions)) {
		list_remove_head(&completions, buf_ptr,
			struct lpfc_dmabuf, list);
		lpfc_mbuf_free(phba, buf_ptr->virt, buf_ptr->phys);
		kfree(buf_ptr);
	}

	 
	local_bh_enable();

	 
	del_timer_sync(&psli->mbox_tmo);

	spin_lock_irqsave(&phba->pport->work_port_lock, flags);
	phba->pport->work_port_events &= ~WORKER_MBOX_TMO;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, flags);

	return 1;
}

 
void
lpfc_sli_pcimem_bcopy(void *srcp, void *destp, uint32_t cnt)
{
	uint32_t *src = srcp;
	uint32_t *dest = destp;
	uint32_t ldata;
	int i;

	for (i = 0; i < (int)cnt; i += sizeof (uint32_t)) {
		ldata = *src;
		ldata = le32_to_cpu(ldata);
		*dest = ldata;
		src++;
		dest++;
	}
}


 
void
lpfc_sli_bemem_bcopy(void *srcp, void *destp, uint32_t cnt)
{
	uint32_t *src = srcp;
	uint32_t *dest = destp;
	uint32_t ldata;
	int i;

	for (i = 0; i < (int)cnt; i += sizeof(uint32_t)) {
		ldata = *src;
		ldata = be32_to_cpu(ldata);
		*dest = ldata;
		src++;
		dest++;
	}
}

 
int
lpfc_sli_ringpostbuf_put(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			 struct lpfc_dmabuf *mp)
{
	 
	spin_lock_irq(&phba->hbalock);
	list_add_tail(&mp->list, &pring->postbufq);
	pring->postbufq_cnt++;
	spin_unlock_irq(&phba->hbalock);
	return 0;
}

 
uint32_t
lpfc_sli_get_buffer_tag(struct lpfc_hba *phba)
{
	spin_lock_irq(&phba->hbalock);
	phba->buffer_tag_count++;
	 
	phba->buffer_tag_count |= QUE_BUFTAG_BIT;
	spin_unlock_irq(&phba->hbalock);
	return phba->buffer_tag_count;
}

 
struct lpfc_dmabuf *
lpfc_sli_ring_taggedbuf_get(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			uint32_t tag)
{
	struct lpfc_dmabuf *mp, *next_mp;
	struct list_head *slp = &pring->postbufq;

	 
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(mp, next_mp, &pring->postbufq, list) {
		if (mp->buffer_tag == tag) {
			list_del_init(&mp->list);
			pring->postbufq_cnt--;
			spin_unlock_irq(&phba->hbalock);
			return mp;
		}
	}

	spin_unlock_irq(&phba->hbalock);
	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"0402 Cannot find virtual addr for buffer tag on "
			"ring %d Data x%lx x%px x%px x%x\n",
			pring->ringno, (unsigned long) tag,
			slp->next, slp->prev, pring->postbufq_cnt);

	return NULL;
}

 
struct lpfc_dmabuf *
lpfc_sli_ringpostbuf_get(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			 dma_addr_t phys)
{
	struct lpfc_dmabuf *mp, *next_mp;
	struct list_head *slp = &pring->postbufq;

	 
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(mp, next_mp, &pring->postbufq, list) {
		if (mp->phys == phys) {
			list_del_init(&mp->list);
			pring->postbufq_cnt--;
			spin_unlock_irq(&phba->hbalock);
			return mp;
		}
	}

	spin_unlock_irq(&phba->hbalock);
	lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"0410 Cannot find virtual addr for mapped buf on "
			"ring %d Data x%llx x%px x%px x%x\n",
			pring->ringno, (unsigned long long)phys,
			slp->next, slp->prev, pring->postbufq_cnt);
	return NULL;
}

 
static void
lpfc_sli_abort_els_cmpl(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
			struct lpfc_iocbq *rspiocb)
{
	u32 ulp_status = get_job_ulpstatus(phba, rspiocb);
	u32 ulp_word4 = get_job_word4(phba, rspiocb);
	u8 cmnd = get_job_cmnd(phba, cmdiocb);

	if (ulp_status) {
		 
		if (phba->sli_rev < LPFC_SLI_REV4) {
			if (cmnd == CMD_ABORT_XRI_CX &&
			    ulp_status == IOSTAT_LOCAL_REJECT &&
			    ulp_word4 == IOERR_ABORT_REQUESTED) {
				goto release_iocb;
			}
		}

		lpfc_printf_log(phba, KERN_WARNING, LOG_ELS | LOG_SLI,
				"0327 Cannot abort els iocb x%px "
				"with io cmd xri %x abort tag : x%x, "
				"abort status %x abort code %x\n",
				cmdiocb, get_job_abtsiotag(phba, cmdiocb),
				(phba->sli_rev == LPFC_SLI_REV4) ?
				get_wqe_reqtag(cmdiocb) :
				cmdiocb->iocb.un.acxri.abortContextTag,
				ulp_status, ulp_word4);

	}
release_iocb:
	lpfc_sli_release_iocbq(phba, cmdiocb);
	return;
}

 
void
lpfc_ignore_els_cmpl(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		     struct lpfc_iocbq *rspiocb)
{
	struct lpfc_nodelist *ndlp = cmdiocb->ndlp;
	IOCB_t *irsp;
	LPFC_MBOXQ_t *mbox;
	u32 ulp_command, ulp_status, ulp_word4, iotag;

	ulp_command = get_job_cmnd(phba, cmdiocb);
	ulp_status = get_job_ulpstatus(phba, rspiocb);
	ulp_word4 = get_job_word4(phba, rspiocb);

	if (phba->sli_rev == LPFC_SLI_REV4) {
		iotag = get_wqe_reqtag(cmdiocb);
	} else {
		irsp = &rspiocb->iocb;
		iotag = irsp->ulpIoTag;

		 
		if (cmdiocb->context_un.mbox) {
			mbox = cmdiocb->context_un.mbox;
			lpfc_mbox_rsrc_cleanup(phba, mbox, MBOX_THD_UNLOCKED);
			cmdiocb->context_un.mbox = NULL;
		}
	}

	 
	lpfc_printf_log(phba, KERN_INFO, LOG_ELS,
			"0139 Ignoring ELS cmd code x%x completion Data: "
			"x%x x%x x%x x%px\n",
			ulp_command, ulp_status, ulp_word4, iotag,
			cmdiocb->ndlp);
	 
	if (ulp_command == CMD_GEN_REQUEST64_CR)
		lpfc_ct_free_iocb(phba, cmdiocb);
	else
		lpfc_els_free_iocb(phba, cmdiocb);

	lpfc_nlp_put(ndlp);
}

 
int
lpfc_sli_issue_abort_iotag(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			   struct lpfc_iocbq *cmdiocb, void *cmpl)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct lpfc_iocbq *abtsiocbp;
	int retval = IOCB_ERROR;
	unsigned long iflags;
	struct lpfc_nodelist *ndlp = NULL;
	u32 ulp_command = get_job_cmnd(phba, cmdiocb);
	u16 ulp_context, iotag;
	bool ia;

	 
	if (ulp_command == CMD_ABORT_XRI_WQE ||
	    ulp_command == CMD_ABORT_XRI_CN ||
	    ulp_command == CMD_CLOSE_XRI_CN ||
	    cmdiocb->cmd_flag & LPFC_DRIVER_ABORTED)
		return IOCB_ABORTING;

	if (!pring) {
		if (cmdiocb->cmd_flag & LPFC_IO_FABRIC)
			cmdiocb->fabric_cmd_cmpl = lpfc_ignore_els_cmpl;
		else
			cmdiocb->cmd_cmpl = lpfc_ignore_els_cmpl;
		return retval;
	}

	 
	if ((vport->load_flag & FC_UNLOADING) &&
	    pring->ringno == LPFC_ELS_RING) {
		if (cmdiocb->cmd_flag & LPFC_IO_FABRIC)
			cmdiocb->fabric_cmd_cmpl = lpfc_ignore_els_cmpl;
		else
			cmdiocb->cmd_cmpl = lpfc_ignore_els_cmpl;
		return retval;
	}

	 
	abtsiocbp = __lpfc_sli_get_iocbq(phba);
	if (abtsiocbp == NULL)
		return IOCB_NORESOURCE;

	 
	cmdiocb->cmd_flag |= LPFC_DRIVER_ABORTED;

	if (phba->sli_rev == LPFC_SLI_REV4) {
		ulp_context = cmdiocb->sli4_xritag;
		iotag = abtsiocbp->iotag;
	} else {
		iotag = cmdiocb->iocb.ulpIoTag;
		if (pring->ringno == LPFC_ELS_RING) {
			ndlp = cmdiocb->ndlp;
			ulp_context = ndlp->nlp_rpi;
		} else {
			ulp_context = cmdiocb->iocb.ulpContext;
		}
	}

	if (phba->link_state < LPFC_LINK_UP ||
	    (phba->sli_rev == LPFC_SLI_REV4 &&
	     phba->sli4_hba.link_state.status == LPFC_FC_LA_TYPE_LINK_DOWN) ||
	    (phba->link_flag & LS_EXTERNAL_LOOPBACK))
		ia = true;
	else
		ia = false;

	lpfc_sli_prep_abort_xri(phba, abtsiocbp, ulp_context, iotag,
				cmdiocb->iocb.ulpClass,
				LPFC_WQE_CQ_ID_DEFAULT, ia, false);

	abtsiocbp->vport = vport;

	 
	abtsiocbp->hba_wqidx = cmdiocb->hba_wqidx;
	if (cmdiocb->cmd_flag & LPFC_IO_FCP)
		abtsiocbp->cmd_flag |= (LPFC_IO_FCP | LPFC_USE_FCPWQIDX);

	if (cmdiocb->cmd_flag & LPFC_IO_FOF)
		abtsiocbp->cmd_flag |= LPFC_IO_FOF;

	if (cmpl)
		abtsiocbp->cmd_cmpl = cmpl;
	else
		abtsiocbp->cmd_cmpl = lpfc_sli_abort_els_cmpl;
	abtsiocbp->vport = vport;

	if (phba->sli_rev == LPFC_SLI_REV4) {
		pring = lpfc_sli4_calc_ring(phba, abtsiocbp);
		if (unlikely(pring == NULL))
			goto abort_iotag_exit;
		 
		spin_lock_irqsave(&pring->ring_lock, iflags);
		retval = __lpfc_sli_issue_iocb(phba, pring->ringno,
			abtsiocbp, 0);
		spin_unlock_irqrestore(&pring->ring_lock, iflags);
	} else {
		retval = __lpfc_sli_issue_iocb(phba, pring->ringno,
			abtsiocbp, 0);
	}

abort_iotag_exit:

	lpfc_printf_vlog(vport, KERN_INFO, LOG_SLI,
			 "0339 Abort IO XRI x%x, Original iotag x%x, "
			 "abort tag x%x Cmdjob : x%px Abortjob : x%px "
			 "retval x%x\n",
			 ulp_context, (phba->sli_rev == LPFC_SLI_REV4) ?
			 cmdiocb->iotag : iotag, iotag, cmdiocb, abtsiocbp,
			 retval);
	if (retval) {
		cmdiocb->cmd_flag &= ~LPFC_DRIVER_ABORTED;
		__lpfc_sli_release_iocbq(phba, abtsiocbp);
	}

	 
	return retval;
}

 
void
lpfc_sli_hba_iocb_abort(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring *pring;
	struct lpfc_queue *qp = NULL;
	int i;

	if (phba->sli_rev != LPFC_SLI_REV4) {
		for (i = 0; i < psli->num_rings; i++) {
			pring = &psli->sli3_ring[i];
			lpfc_sli_abort_iocb_ring(phba, pring);
		}
		return;
	}
	list_for_each_entry(qp, &phba->sli4_hba.lpfc_wq_list, wq_list) {
		pring = qp->pring;
		if (!pring)
			continue;
		lpfc_sli_abort_iocb_ring(phba, pring);
	}
}

 
static int
lpfc_sli_validate_fcp_iocb_for_abort(struct lpfc_iocbq *iocbq,
				     struct lpfc_vport *vport)
{
	u8 ulp_command;

	 
	if (!iocbq || iocbq->vport != vport)
		return -ENODEV;

	 
	ulp_command = get_job_cmnd(vport->phba, iocbq);
	if (!(iocbq->cmd_flag & LPFC_IO_FCP) ||
	    !(iocbq->cmd_flag & LPFC_IO_ON_TXCMPLQ) ||
	    (iocbq->cmd_flag & LPFC_DRIVER_ABORTED) ||
	    (ulp_command == CMD_ABORT_XRI_CN ||
	     ulp_command == CMD_CLOSE_XRI_CN ||
	     ulp_command == CMD_ABORT_XRI_WQE))
		return -EINVAL;

	return 0;
}

 
static int
lpfc_sli_validate_fcp_iocb(struct lpfc_iocbq *iocbq, struct lpfc_vport *vport,
			   uint16_t tgt_id, uint64_t lun_id,
			   lpfc_ctx_cmd ctx_cmd)
{
	struct lpfc_io_buf *lpfc_cmd;
	int rc = 1;

	lpfc_cmd = container_of(iocbq, struct lpfc_io_buf, cur_iocbq);

	if (lpfc_cmd->pCmd == NULL)
		return rc;

	switch (ctx_cmd) {
	case LPFC_CTX_LUN:
		if ((lpfc_cmd->rdata) && (lpfc_cmd->rdata->pnode) &&
		    (lpfc_cmd->rdata->pnode->nlp_sid == tgt_id) &&
		    (scsilun_to_int(&lpfc_cmd->fcp_cmnd->fcp_lun) == lun_id))
			rc = 0;
		break;
	case LPFC_CTX_TGT:
		if ((lpfc_cmd->rdata) && (lpfc_cmd->rdata->pnode) &&
		    (lpfc_cmd->rdata->pnode->nlp_sid == tgt_id))
			rc = 0;
		break;
	case LPFC_CTX_HOST:
		rc = 0;
		break;
	default:
		printk(KERN_ERR "%s: Unknown context cmd type, value %d\n",
			__func__, ctx_cmd);
		break;
	}

	return rc;
}

 
int
lpfc_sli_sum_iocb(struct lpfc_vport *vport, uint16_t tgt_id, uint64_t lun_id,
		  lpfc_ctx_cmd ctx_cmd)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_iocbq *iocbq;
	int sum, i;
	unsigned long iflags;
	u8 ulp_command;

	spin_lock_irqsave(&phba->hbalock, iflags);
	for (i = 1, sum = 0; i <= phba->sli.last_iotag; i++) {
		iocbq = phba->sli.iocbq_lookup[i];

		if (!iocbq || iocbq->vport != vport)
			continue;
		if (!(iocbq->cmd_flag & LPFC_IO_FCP) ||
		    !(iocbq->cmd_flag & LPFC_IO_ON_TXCMPLQ))
			continue;

		 
		ulp_command = get_job_cmnd(phba, iocbq);
		if (ulp_command == CMD_ABORT_XRI_CN ||
		    ulp_command == CMD_CLOSE_XRI_CN ||
		    ulp_command == CMD_ABORT_XRI_WQE) {
			sum++;
			continue;
		}

		if (lpfc_sli_validate_fcp_iocb(iocbq, vport, tgt_id, lun_id,
					       ctx_cmd) == 0)
			sum++;
	}
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	return sum;
}

 
void
lpfc_sli_abort_fcp_cmpl(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
			struct lpfc_iocbq *rspiocb)
{
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"3096 ABORT_XRI_CX completing on rpi x%x "
			"original iotag x%x, abort cmd iotag x%x "
			"status 0x%x, reason 0x%x\n",
			(phba->sli_rev == LPFC_SLI_REV4) ?
			cmdiocb->sli4_xritag :
			cmdiocb->iocb.un.acxri.abortContextTag,
			get_job_abtsiotag(phba, cmdiocb),
			cmdiocb->iotag, get_job_ulpstatus(phba, rspiocb),
			get_job_word4(phba, rspiocb));
	lpfc_sli_release_iocbq(phba, cmdiocb);
	return;
}

 
int
lpfc_sli_abort_iocb(struct lpfc_vport *vport, u16 tgt_id, u64 lun_id,
		    lpfc_ctx_cmd abort_cmd)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_sli_ring *pring = NULL;
	struct lpfc_iocbq *iocbq;
	int errcnt = 0, ret_val = 0;
	unsigned long iflags;
	int i;

	 
	if (phba->hba_flag & HBA_IOQ_FLUSH)
		return errcnt;

	for (i = 1; i <= phba->sli.last_iotag; i++) {
		iocbq = phba->sli.iocbq_lookup[i];

		if (lpfc_sli_validate_fcp_iocb_for_abort(iocbq, vport))
			continue;

		if (lpfc_sli_validate_fcp_iocb(iocbq, vport, tgt_id, lun_id,
					       abort_cmd) != 0)
			continue;

		spin_lock_irqsave(&phba->hbalock, iflags);
		if (phba->sli_rev == LPFC_SLI_REV3) {
			pring = &phba->sli.sli3_ring[LPFC_FCP_RING];
		} else if (phba->sli_rev == LPFC_SLI_REV4) {
			pring = lpfc_sli4_calc_ring(phba, iocbq);
		}
		ret_val = lpfc_sli_issue_abort_iotag(phba, pring, iocbq,
						     lpfc_sli_abort_fcp_cmpl);
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		if (ret_val != IOCB_SUCCESS)
			errcnt++;
	}

	return errcnt;
}

 
int
lpfc_sli_abort_taskmgmt(struct lpfc_vport *vport, struct lpfc_sli_ring *pring,
			uint16_t tgt_id, uint64_t lun_id, lpfc_ctx_cmd cmd)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_io_buf *lpfc_cmd;
	struct lpfc_iocbq *abtsiocbq;
	struct lpfc_nodelist *ndlp = NULL;
	struct lpfc_iocbq *iocbq;
	int sum, i, ret_val;
	unsigned long iflags;
	struct lpfc_sli_ring *pring_s4 = NULL;
	u16 ulp_context, iotag, cqid = LPFC_WQE_CQ_ID_DEFAULT;
	bool ia;

	spin_lock_irqsave(&phba->hbalock, iflags);

	 
	if (phba->hba_flag & HBA_IOQ_FLUSH) {
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		return 0;
	}
	sum = 0;

	for (i = 1; i <= phba->sli.last_iotag; i++) {
		iocbq = phba->sli.iocbq_lookup[i];

		if (lpfc_sli_validate_fcp_iocb_for_abort(iocbq, vport))
			continue;

		if (lpfc_sli_validate_fcp_iocb(iocbq, vport, tgt_id, lun_id,
					       cmd) != 0)
			continue;

		 
		lpfc_cmd = container_of(iocbq, struct lpfc_io_buf, cur_iocbq);
		spin_lock(&lpfc_cmd->buf_lock);

		if (!lpfc_cmd->pCmd) {
			spin_unlock(&lpfc_cmd->buf_lock);
			continue;
		}

		if (phba->sli_rev == LPFC_SLI_REV4) {
			pring_s4 =
			    phba->sli4_hba.hdwq[iocbq->hba_wqidx].io_wq->pring;
			if (!pring_s4) {
				spin_unlock(&lpfc_cmd->buf_lock);
				continue;
			}
			 
			spin_lock(&pring_s4->ring_lock);
		}

		 
		if ((iocbq->cmd_flag & LPFC_DRIVER_ABORTED) ||
		    !(iocbq->cmd_flag & LPFC_IO_ON_TXCMPLQ)) {
			if (phba->sli_rev == LPFC_SLI_REV4)
				spin_unlock(&pring_s4->ring_lock);
			spin_unlock(&lpfc_cmd->buf_lock);
			continue;
		}

		 
		abtsiocbq = __lpfc_sli_get_iocbq(phba);
		if (!abtsiocbq) {
			if (phba->sli_rev == LPFC_SLI_REV4)
				spin_unlock(&pring_s4->ring_lock);
			spin_unlock(&lpfc_cmd->buf_lock);
			continue;
		}

		if (phba->sli_rev == LPFC_SLI_REV4) {
			iotag = abtsiocbq->iotag;
			ulp_context = iocbq->sli4_xritag;
			cqid = lpfc_cmd->hdwq->io_cq_map;
		} else {
			iotag = iocbq->iocb.ulpIoTag;
			if (pring->ringno == LPFC_ELS_RING) {
				ndlp = iocbq->ndlp;
				ulp_context = ndlp->nlp_rpi;
			} else {
				ulp_context = iocbq->iocb.ulpContext;
			}
		}

		ndlp = lpfc_cmd->rdata->pnode;

		if (lpfc_is_link_up(phba) &&
		    (ndlp && ndlp->nlp_state == NLP_STE_MAPPED_NODE) &&
		    !(phba->link_flag & LS_EXTERNAL_LOOPBACK))
			ia = false;
		else
			ia = true;

		lpfc_sli_prep_abort_xri(phba, abtsiocbq, ulp_context, iotag,
					iocbq->iocb.ulpClass, cqid,
					ia, false);

		abtsiocbq->vport = vport;

		 
		abtsiocbq->hba_wqidx = iocbq->hba_wqidx;
		if (iocbq->cmd_flag & LPFC_IO_FCP)
			abtsiocbq->cmd_flag |= LPFC_USE_FCPWQIDX;
		if (iocbq->cmd_flag & LPFC_IO_FOF)
			abtsiocbq->cmd_flag |= LPFC_IO_FOF;

		 
		abtsiocbq->cmd_cmpl = lpfc_sli_abort_fcp_cmpl;

		 
		iocbq->cmd_flag |= LPFC_DRIVER_ABORTED;

		if (phba->sli_rev == LPFC_SLI_REV4) {
			ret_val = __lpfc_sli_issue_iocb(phba, pring_s4->ringno,
							abtsiocbq, 0);
			spin_unlock(&pring_s4->ring_lock);
		} else {
			ret_val = __lpfc_sli_issue_iocb(phba, pring->ringno,
							abtsiocbq, 0);
		}

		spin_unlock(&lpfc_cmd->buf_lock);

		if (ret_val == IOCB_ERROR)
			__lpfc_sli_release_iocbq(phba, abtsiocbq);
		else
			sum++;
	}
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return sum;
}

 
static void
lpfc_sli_wake_iocb_wait(struct lpfc_hba *phba,
			struct lpfc_iocbq *cmdiocbq,
			struct lpfc_iocbq *rspiocbq)
{
	wait_queue_head_t *pdone_q;
	unsigned long iflags;
	struct lpfc_io_buf *lpfc_cmd;
	size_t offset = offsetof(struct lpfc_iocbq, wqe);

	spin_lock_irqsave(&phba->hbalock, iflags);
	if (cmdiocbq->cmd_flag & LPFC_IO_WAKE_TMO) {

		 

		spin_unlock_irqrestore(&phba->hbalock, iflags);
		cmdiocbq->cmd_cmpl = cmdiocbq->wait_cmd_cmpl;
		cmdiocbq->wait_cmd_cmpl = NULL;
		if (cmdiocbq->cmd_cmpl)
			cmdiocbq->cmd_cmpl(phba, cmdiocbq, NULL);
		else
			lpfc_sli_release_iocbq(phba, cmdiocbq);
		return;
	}

	 
	cmdiocbq->cmd_flag |= LPFC_IO_WAKE;
	if (cmdiocbq->rsp_iocb && rspiocbq)
		memcpy((char *)cmdiocbq->rsp_iocb + offset,
		       (char *)rspiocbq + offset, sizeof(*rspiocbq) - offset);

	 
	if ((cmdiocbq->cmd_flag & LPFC_IO_FCP) &&
	    !(cmdiocbq->cmd_flag & LPFC_IO_LIBDFC)) {
		lpfc_cmd = container_of(cmdiocbq, struct lpfc_io_buf,
					cur_iocbq);
		if (rspiocbq && (rspiocbq->cmd_flag & LPFC_EXCHANGE_BUSY))
			lpfc_cmd->flags |= LPFC_SBUF_XBUSY;
		else
			lpfc_cmd->flags &= ~LPFC_SBUF_XBUSY;
	}

	pdone_q = cmdiocbq->context_un.wait_queue;
	if (pdone_q)
		wake_up(pdone_q);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return;
}

 
static int
lpfc_chk_iocb_flg(struct lpfc_hba *phba,
		 struct lpfc_iocbq *piocbq, uint32_t flag)
{
	unsigned long iflags;
	int ret;

	spin_lock_irqsave(&phba->hbalock, iflags);
	ret = piocbq->cmd_flag & flag;
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return ret;

}

 
int
lpfc_sli_issue_iocb_wait(struct lpfc_hba *phba,
			 uint32_t ring_number,
			 struct lpfc_iocbq *piocb,
			 struct lpfc_iocbq *prspiocbq,
			 uint32_t timeout)
{
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(done_q);
	long timeleft, timeout_req = 0;
	int retval = IOCB_SUCCESS;
	uint32_t creg_val;
	struct lpfc_iocbq *iocb;
	int txq_cnt = 0;
	int txcmplq_cnt = 0;
	struct lpfc_sli_ring *pring;
	unsigned long iflags;
	bool iocb_completed = true;

	if (phba->sli_rev >= LPFC_SLI_REV4) {
		lpfc_sli_prep_wqe(phba, piocb);

		pring = lpfc_sli4_calc_ring(phba, piocb);
	} else
		pring = &phba->sli.sli3_ring[ring_number];
	 
	if (prspiocbq) {
		if (piocb->rsp_iocb)
			return IOCB_ERROR;
		piocb->rsp_iocb = prspiocbq;
	}

	piocb->wait_cmd_cmpl = piocb->cmd_cmpl;
	piocb->cmd_cmpl = lpfc_sli_wake_iocb_wait;
	piocb->context_un.wait_queue = &done_q;
	piocb->cmd_flag &= ~(LPFC_IO_WAKE | LPFC_IO_WAKE_TMO);

	if (phba->cfg_poll & DISABLE_FCP_RING_INT) {
		if (lpfc_readl(phba->HCregaddr, &creg_val))
			return IOCB_ERROR;
		creg_val |= (HC_R0INT_ENA << LPFC_FCP_RING);
		writel(creg_val, phba->HCregaddr);
		readl(phba->HCregaddr);  
	}

	retval = lpfc_sli_issue_iocb(phba, ring_number, piocb,
				     SLI_IOCB_RET_IOCB);
	if (retval == IOCB_SUCCESS) {
		timeout_req = msecs_to_jiffies(timeout * 1000);
		timeleft = wait_event_timeout(done_q,
				lpfc_chk_iocb_flg(phba, piocb, LPFC_IO_WAKE),
				timeout_req);
		spin_lock_irqsave(&phba->hbalock, iflags);
		if (!(piocb->cmd_flag & LPFC_IO_WAKE)) {

			 

			iocb_completed = false;
			piocb->cmd_flag |= LPFC_IO_WAKE_TMO;
		}
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		if (iocb_completed) {
			lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
					"0331 IOCB wake signaled\n");
			 
		} else if (timeleft == 0) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0338 IOCB wait timeout error - no "
					"wake response Data x%x\n", timeout);
			retval = IOCB_TIMEDOUT;
		} else {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0330 IOCB wake NOT set, "
					"Data x%x x%lx\n",
					timeout, (timeleft / jiffies));
			retval = IOCB_TIMEDOUT;
		}
	} else if (retval == IOCB_BUSY) {
		if (phba->cfg_log_verbose & LOG_SLI) {
			list_for_each_entry(iocb, &pring->txq, list) {
				txq_cnt++;
			}
			list_for_each_entry(iocb, &pring->txcmplq, list) {
				txcmplq_cnt++;
			}
			lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"2818 Max IOCBs %d txq cnt %d txcmplq cnt %d\n",
				phba->iocb_cnt, txq_cnt, txcmplq_cnt);
		}
		return retval;
	} else {
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"0332 IOCB wait issue failed, Data x%x\n",
				retval);
		retval = IOCB_ERROR;
	}

	if (phba->cfg_poll & DISABLE_FCP_RING_INT) {
		if (lpfc_readl(phba->HCregaddr, &creg_val))
			return IOCB_ERROR;
		creg_val &= ~(HC_R0INT_ENA << LPFC_FCP_RING);
		writel(creg_val, phba->HCregaddr);
		readl(phba->HCregaddr);  
	}

	if (prspiocbq)
		piocb->rsp_iocb = NULL;

	piocb->context_un.wait_queue = NULL;
	piocb->cmd_cmpl = NULL;
	return retval;
}

 
int
lpfc_sli_issue_mbox_wait(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmboxq,
			 uint32_t timeout)
{
	struct completion mbox_done;
	int retval;
	unsigned long flag;

	pmboxq->mbox_flag &= ~LPFC_MBX_WAKE;
	 
	pmboxq->mbox_cmpl = lpfc_sli_wake_mbox_wait;

	 
	init_completion(&mbox_done);
	pmboxq->context3 = &mbox_done;
	 
	retval = lpfc_sli_issue_mbox(phba, pmboxq, MBX_NOWAIT);
	if (retval == MBX_BUSY || retval == MBX_SUCCESS) {
		wait_for_completion_timeout(&mbox_done,
					    msecs_to_jiffies(timeout * 1000));

		spin_lock_irqsave(&phba->hbalock, flag);
		pmboxq->context3 = NULL;
		 
		if (pmboxq->mbox_flag & LPFC_MBX_WAKE) {
			retval = MBX_SUCCESS;
		} else {
			retval = MBX_TIMEOUT;
			pmboxq->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		}
		spin_unlock_irqrestore(&phba->hbalock, flag);
	}
	return retval;
}

 
void
lpfc_sli_mbox_sys_shutdown(struct lpfc_hba *phba, int mbx_action)
{
	struct lpfc_sli *psli = &phba->sli;
	unsigned long timeout;

	if (mbx_action == LPFC_MBX_NO_WAIT) {
		 
		msleep(100);
		lpfc_sli_mbox_sys_flush(phba);
		return;
	}
	timeout = msecs_to_jiffies(LPFC_MBOX_TMO * 1000) + jiffies;

	 
	local_bh_disable();

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag |= LPFC_SLI_ASYNC_MBX_BLK;

	if (psli->sli_flag & LPFC_SLI_ACTIVE) {
		 
		if (phba->sli.mbox_active)
			timeout = msecs_to_jiffies(lpfc_mbox_tmo_val(phba,
						phba->sli.mbox_active) *
						1000) + jiffies;
		spin_unlock_irq(&phba->hbalock);

		 
		local_bh_enable();

		while (phba->sli.mbox_active) {
			 
			msleep(2);
			if (time_after(jiffies, timeout))
				 
				break;
		}
	} else {
		spin_unlock_irq(&phba->hbalock);

		 
		local_bh_enable();
	}

	lpfc_sli_mbox_sys_flush(phba);
}

 
static int
lpfc_sli_eratt_read(struct lpfc_hba *phba)
{
	uint32_t ha_copy;

	 
	if (lpfc_readl(phba->HAregaddr, &ha_copy))
		goto unplug_err;

	if (ha_copy & HA_ERATT) {
		 
		if (lpfc_sli_read_hs(phba))
			goto unplug_err;

		 
		if ((HS_FFER1 & phba->work_hs) &&
		    ((HS_FFER2 | HS_FFER3 | HS_FFER4 | HS_FFER5 |
		      HS_FFER6 | HS_FFER7 | HS_FFER8) & phba->work_hs)) {
			phba->hba_flag |= DEFER_ERATT;
			 
			writel(0, phba->HCregaddr);
			readl(phba->HCregaddr);
		}

		 
		phba->work_ha |= HA_ERATT;
		 
		phba->hba_flag |= HBA_ERATT_HANDLED;
		return 1;
	}
	return 0;

unplug_err:
	 
	phba->work_hs |= UNPLUG_ERR;
	 
	phba->work_ha |= HA_ERATT;
	 
	phba->hba_flag |= HBA_ERATT_HANDLED;
	return 1;
}

 
static int
lpfc_sli4_eratt_read(struct lpfc_hba *phba)
{
	uint32_t uerr_sta_hi, uerr_sta_lo;
	uint32_t if_type, portsmphr;
	struct lpfc_register portstat_reg;
	u32 logmask;

	 
	if_type = bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf);
	switch (if_type) {
	case LPFC_SLI_INTF_IF_TYPE_0:
		if (lpfc_readl(phba->sli4_hba.u.if_type0.UERRLOregaddr,
			&uerr_sta_lo) ||
			lpfc_readl(phba->sli4_hba.u.if_type0.UERRHIregaddr,
			&uerr_sta_hi)) {
			phba->work_hs |= UNPLUG_ERR;
			phba->work_ha |= HA_ERATT;
			phba->hba_flag |= HBA_ERATT_HANDLED;
			return 1;
		}
		if ((~phba->sli4_hba.ue_mask_lo & uerr_sta_lo) ||
		    (~phba->sli4_hba.ue_mask_hi & uerr_sta_hi)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"1423 HBA Unrecoverable error: "
					"uerr_lo_reg=0x%x, uerr_hi_reg=0x%x, "
					"ue_mask_lo_reg=0x%x, "
					"ue_mask_hi_reg=0x%x\n",
					uerr_sta_lo, uerr_sta_hi,
					phba->sli4_hba.ue_mask_lo,
					phba->sli4_hba.ue_mask_hi);
			phba->work_status[0] = uerr_sta_lo;
			phba->work_status[1] = uerr_sta_hi;
			phba->work_ha |= HA_ERATT;
			phba->hba_flag |= HBA_ERATT_HANDLED;
			return 1;
		}
		break;
	case LPFC_SLI_INTF_IF_TYPE_2:
	case LPFC_SLI_INTF_IF_TYPE_6:
		if (lpfc_readl(phba->sli4_hba.u.if_type2.STATUSregaddr,
			&portstat_reg.word0) ||
			lpfc_readl(phba->sli4_hba.PSMPHRregaddr,
			&portsmphr)){
			phba->work_hs |= UNPLUG_ERR;
			phba->work_ha |= HA_ERATT;
			phba->hba_flag |= HBA_ERATT_HANDLED;
			return 1;
		}
		if (bf_get(lpfc_sliport_status_err, &portstat_reg)) {
			phba->work_status[0] =
				readl(phba->sli4_hba.u.if_type2.ERR1regaddr);
			phba->work_status[1] =
				readl(phba->sli4_hba.u.if_type2.ERR2regaddr);
			logmask = LOG_TRACE_EVENT;
			if (phba->work_status[0] ==
				SLIPORT_ERR1_REG_ERR_CODE_2 &&
			    phba->work_status[1] == SLIPORT_ERR2_REG_FW_RESTART)
				logmask = LOG_SLI;
			lpfc_printf_log(phba, KERN_ERR, logmask,
					"2885 Port Status Event: "
					"port status reg 0x%x, "
					"port smphr reg 0x%x, "
					"error 1=0x%x, error 2=0x%x\n",
					portstat_reg.word0,
					portsmphr,
					phba->work_status[0],
					phba->work_status[1]);
			phba->work_ha |= HA_ERATT;
			phba->hba_flag |= HBA_ERATT_HANDLED;
			return 1;
		}
		break;
	case LPFC_SLI_INTF_IF_TYPE_1:
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2886 HBA Error Attention on unsupported "
				"if type %d.", if_type);
		return 1;
	}

	return 0;
}

 
int
lpfc_sli_check_eratt(struct lpfc_hba *phba)
{
	uint32_t ha_copy;

	 
	if (phba->link_flag & LS_IGNORE_ERATT)
		return 0;

	 
	spin_lock_irq(&phba->hbalock);
	if (phba->hba_flag & HBA_ERATT_HANDLED) {
		 
		spin_unlock_irq(&phba->hbalock);
		return 0;
	}

	 
	if (unlikely(phba->hba_flag & DEFER_ERATT)) {
		spin_unlock_irq(&phba->hbalock);
		return 0;
	}

	 
	if (unlikely(pci_channel_offline(phba->pcidev))) {
		spin_unlock_irq(&phba->hbalock);
		return 0;
	}

	switch (phba->sli_rev) {
	case LPFC_SLI_REV2:
	case LPFC_SLI_REV3:
		 
		ha_copy = lpfc_sli_eratt_read(phba);
		break;
	case LPFC_SLI_REV4:
		 
		ha_copy = lpfc_sli4_eratt_read(phba);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0299 Invalid SLI revision (%d)\n",
				phba->sli_rev);
		ha_copy = 0;
		break;
	}
	spin_unlock_irq(&phba->hbalock);

	return ha_copy;
}

 
static inline int
lpfc_intr_state_check(struct lpfc_hba *phba)
{
	 
	if (unlikely(pci_channel_offline(phba->pcidev)))
		return -EIO;

	 
	phba->sli.slistat.sli_intr++;

	 
	if (unlikely(phba->link_state < LPFC_LINK_DOWN))
		return -EIO;

	return 0;
}

 
irqreturn_t
lpfc_sli_sp_intr_handler(int irq, void *dev_id)
{
	struct lpfc_hba  *phba;
	uint32_t ha_copy, hc_copy;
	uint32_t work_ha_copy;
	unsigned long status;
	unsigned long iflag;
	uint32_t control;

	MAILBOX_t *mbox, *pmbox;
	struct lpfc_vport *vport;
	struct lpfc_nodelist *ndlp;
	struct lpfc_dmabuf *mp;
	LPFC_MBOXQ_t *pmb;
	int rc;

	 
	phba = (struct lpfc_hba *)dev_id;

	if (unlikely(!phba))
		return IRQ_NONE;

	 
	if (phba->intr_type == MSIX) {
		 
		if (lpfc_intr_state_check(phba))
			return IRQ_NONE;
		 
		spin_lock_irqsave(&phba->hbalock, iflag);
		if (lpfc_readl(phba->HAregaddr, &ha_copy))
			goto unplug_error;
		 
		if (phba->link_flag & LS_IGNORE_ERATT)
			ha_copy &= ~HA_ERATT;
		 
		if (ha_copy & HA_ERATT) {
			if (phba->hba_flag & HBA_ERATT_HANDLED)
				 
				ha_copy &= ~HA_ERATT;
			else
				 
				phba->hba_flag |= HBA_ERATT_HANDLED;
		}

		 
		if (unlikely(phba->hba_flag & DEFER_ERATT)) {
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			return IRQ_NONE;
		}

		 
		if (lpfc_readl(phba->HCregaddr, &hc_copy))
			goto unplug_error;

		writel(hc_copy & ~(HC_MBINT_ENA | HC_R2INT_ENA |
			HC_LAINT_ENA | HC_ERINT_ENA),
			phba->HCregaddr);
		writel((ha_copy & (HA_MBATT | HA_R2_CLR_MSK)),
			phba->HAregaddr);
		writel(hc_copy, phba->HCregaddr);
		readl(phba->HAregaddr);  
		spin_unlock_irqrestore(&phba->hbalock, iflag);
	} else
		ha_copy = phba->ha_copy;

	work_ha_copy = ha_copy & phba->work_ha_mask;

	if (work_ha_copy) {
		if (work_ha_copy & HA_LATT) {
			if (phba->sli.sli_flag & LPFC_PROCESS_LA) {
				 
				spin_lock_irqsave(&phba->hbalock, iflag);
				phba->sli.sli_flag &= ~LPFC_PROCESS_LA;
				if (lpfc_readl(phba->HCregaddr, &control))
					goto unplug_error;
				control &= ~HC_LAINT_ENA;
				writel(control, phba->HCregaddr);
				readl(phba->HCregaddr);  
				spin_unlock_irqrestore(&phba->hbalock, iflag);
			}
			else
				work_ha_copy &= ~HA_LATT;
		}

		if (work_ha_copy & ~(HA_ERATT | HA_MBATT | HA_LATT)) {
			 
			status = (work_ha_copy &
				(HA_RXMASK  << (4*LPFC_ELS_RING)));
			status >>= (4*LPFC_ELS_RING);
			if (status & HA_RXMASK) {
				spin_lock_irqsave(&phba->hbalock, iflag);
				if (lpfc_readl(phba->HCregaddr, &control))
					goto unplug_error;

				lpfc_debugfs_slow_ring_trc(phba,
				"ISR slow ring:   ctl:x%x stat:x%x isrcnt:x%x",
				control, status,
				(uint32_t)phba->sli.slistat.sli_intr);

				if (control & (HC_R0INT_ENA << LPFC_ELS_RING)) {
					lpfc_debugfs_slow_ring_trc(phba,
						"ISR Disable ring:"
						"pwork:x%x hawork:x%x wait:x%x",
						phba->work_ha, work_ha_copy,
						(uint32_t)((unsigned long)
						&phba->work_waitq));

					control &=
					    ~(HC_R0INT_ENA << LPFC_ELS_RING);
					writel(control, phba->HCregaddr);
					readl(phba->HCregaddr);  
				}
				else {
					lpfc_debugfs_slow_ring_trc(phba,
						"ISR slow ring:   pwork:"
						"x%x hawork:x%x wait:x%x",
						phba->work_ha, work_ha_copy,
						(uint32_t)((unsigned long)
						&phba->work_waitq));
				}
				spin_unlock_irqrestore(&phba->hbalock, iflag);
			}
		}
		spin_lock_irqsave(&phba->hbalock, iflag);
		if (work_ha_copy & HA_ERATT) {
			if (lpfc_sli_read_hs(phba))
				goto unplug_error;
			 
			if ((HS_FFER1 & phba->work_hs) &&
				((HS_FFER2 | HS_FFER3 | HS_FFER4 | HS_FFER5 |
				  HS_FFER6 | HS_FFER7 | HS_FFER8) &
				  phba->work_hs)) {
				phba->hba_flag |= DEFER_ERATT;
				 
				writel(0, phba->HCregaddr);
				readl(phba->HCregaddr);
			}
		}

		if ((work_ha_copy & HA_MBATT) && (phba->sli.mbox_active)) {
			pmb = phba->sli.mbox_active;
			pmbox = &pmb->u.mb;
			mbox = phba->mbox;
			vport = pmb->vport;

			 
			lpfc_sli_pcimem_bcopy(mbox, pmbox, sizeof(uint32_t));
			if (pmbox->mbxOwner != OWN_HOST) {
				spin_unlock_irqrestore(&phba->hbalock, iflag);
				 
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"(%d):0304 Stray Mailbox "
						"Interrupt mbxCommand x%x "
						"mbxStatus x%x\n",
						(vport ? vport->vpi : 0),
						pmbox->mbxCommand,
						pmbox->mbxStatus);
				 
				work_ha_copy &= ~HA_MBATT;
			} else {
				phba->sli.mbox_active = NULL;
				spin_unlock_irqrestore(&phba->hbalock, iflag);
				phba->last_completion_time = jiffies;
				del_timer(&phba->sli.mbox_tmo);
				if (pmb->mbox_cmpl) {
					lpfc_sli_pcimem_bcopy(mbox, pmbox,
							MAILBOX_CMD_SIZE);
					if (pmb->out_ext_byte_len &&
						pmb->ctx_buf)
						lpfc_sli_pcimem_bcopy(
						phba->mbox_ext,
						pmb->ctx_buf,
						pmb->out_ext_byte_len);
				}
				if (pmb->mbox_flag & LPFC_MBX_IMED_UNREG) {
					pmb->mbox_flag &= ~LPFC_MBX_IMED_UNREG;

					lpfc_debugfs_disc_trc(vport,
						LPFC_DISC_TRC_MBOX_VPORT,
						"MBOX dflt rpi: : "
						"status:x%x rpi:x%x",
						(uint32_t)pmbox->mbxStatus,
						pmbox->un.varWords[0], 0);

					if (!pmbox->mbxStatus) {
						mp = (struct lpfc_dmabuf *)
							(pmb->ctx_buf);
						ndlp = (struct lpfc_nodelist *)
							pmb->ctx_ndlp;

						 
						lpfc_unreg_login(phba,
							vport->vpi,
							pmbox->un.varWords[0],
							pmb);
						pmb->mbox_cmpl =
							lpfc_mbx_cmpl_dflt_rpi;
						pmb->ctx_buf = mp;
						pmb->ctx_ndlp = ndlp;
						pmb->vport = vport;
						rc = lpfc_sli_issue_mbox(phba,
								pmb,
								MBX_NOWAIT);
						if (rc != MBX_BUSY)
							lpfc_printf_log(phba,
							KERN_ERR,
							LOG_TRACE_EVENT,
							"0350 rc should have"
							"been MBX_BUSY\n");
						if (rc != MBX_NOT_FINISHED)
							goto send_current_mbox;
					}
				}
				spin_lock_irqsave(
						&phba->pport->work_port_lock,
						iflag);
				phba->pport->work_port_events &=
					~WORKER_MBOX_TMO;
				spin_unlock_irqrestore(
						&phba->pport->work_port_lock,
						iflag);

				 
				if (pmbox->mbxCommand == MBX_HEARTBEAT) {
					 
					phba->sli.mbox_active = NULL;
					phba->sli.sli_flag &=
						~LPFC_SLI_MBOX_ACTIVE;
					if (pmb->mbox_cmpl)
						pmb->mbox_cmpl(phba, pmb);
				} else {
					 
					lpfc_mbox_cmpl_put(phba, pmb);
				}
			}
		} else
			spin_unlock_irqrestore(&phba->hbalock, iflag);

		if ((work_ha_copy & HA_MBATT) &&
		    (phba->sli.mbox_active == NULL)) {
send_current_mbox:
			 
			do {
				rc = lpfc_sli_issue_mbox(phba, NULL,
							 MBX_NOWAIT);
			} while (rc == MBX_NOT_FINISHED);
			if (rc != MBX_SUCCESS)
				lpfc_printf_log(phba, KERN_ERR,
						LOG_TRACE_EVENT,
						"0349 rc should be "
						"MBX_SUCCESS\n");
		}

		spin_lock_irqsave(&phba->hbalock, iflag);
		phba->work_ha |= work_ha_copy;
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		lpfc_worker_wake_up(phba);
	}
	return IRQ_HANDLED;
unplug_error:
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return IRQ_HANDLED;

}  

 
irqreturn_t
lpfc_sli_fp_intr_handler(int irq, void *dev_id)
{
	struct lpfc_hba  *phba;
	uint32_t ha_copy;
	unsigned long status;
	unsigned long iflag;
	struct lpfc_sli_ring *pring;

	 
	phba = (struct lpfc_hba *) dev_id;

	if (unlikely(!phba))
		return IRQ_NONE;

	 
	if (phba->intr_type == MSIX) {
		 
		if (lpfc_intr_state_check(phba))
			return IRQ_NONE;
		 
		if (lpfc_readl(phba->HAregaddr, &ha_copy))
			return IRQ_HANDLED;
		 
		spin_lock_irqsave(&phba->hbalock, iflag);
		 
		if (unlikely(phba->hba_flag & DEFER_ERATT)) {
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			return IRQ_NONE;
		}
		writel((ha_copy & (HA_R0_CLR_MSK | HA_R1_CLR_MSK)),
			phba->HAregaddr);
		readl(phba->HAregaddr);  
		spin_unlock_irqrestore(&phba->hbalock, iflag);
	} else
		ha_copy = phba->ha_copy;

	 
	ha_copy &= ~(phba->work_ha_mask);

	status = (ha_copy & (HA_RXMASK << (4*LPFC_FCP_RING)));
	status >>= (4*LPFC_FCP_RING);
	pring = &phba->sli.sli3_ring[LPFC_FCP_RING];
	if (status & HA_RXMASK)
		lpfc_sli_handle_fast_ring_event(phba, pring, status);

	if (phba->cfg_multi_ring_support == 2) {
		 
		status = (ha_copy & (HA_RXMASK << (4*LPFC_EXTRA_RING)));
		status >>= (4*LPFC_EXTRA_RING);
		if (status & HA_RXMASK) {
			lpfc_sli_handle_fast_ring_event(phba,
					&phba->sli.sli3_ring[LPFC_EXTRA_RING],
					status);
		}
	}
	return IRQ_HANDLED;
}   

 
irqreturn_t
lpfc_sli_intr_handler(int irq, void *dev_id)
{
	struct lpfc_hba  *phba;
	irqreturn_t sp_irq_rc, fp_irq_rc;
	unsigned long status1, status2;
	uint32_t hc_copy;

	 
	phba = (struct lpfc_hba *) dev_id;

	if (unlikely(!phba))
		return IRQ_NONE;

	 
	if (lpfc_intr_state_check(phba))
		return IRQ_NONE;

	spin_lock(&phba->hbalock);
	if (lpfc_readl(phba->HAregaddr, &phba->ha_copy)) {
		spin_unlock(&phba->hbalock);
		return IRQ_HANDLED;
	}

	if (unlikely(!phba->ha_copy)) {
		spin_unlock(&phba->hbalock);
		return IRQ_NONE;
	} else if (phba->ha_copy & HA_ERATT) {
		if (phba->hba_flag & HBA_ERATT_HANDLED)
			 
			phba->ha_copy &= ~HA_ERATT;
		else
			 
			phba->hba_flag |= HBA_ERATT_HANDLED;
	}

	 
	if (unlikely(phba->hba_flag & DEFER_ERATT)) {
		spin_unlock(&phba->hbalock);
		return IRQ_NONE;
	}

	 
	if (lpfc_readl(phba->HCregaddr, &hc_copy)) {
		spin_unlock(&phba->hbalock);
		return IRQ_HANDLED;
	}
	writel(hc_copy & ~(HC_MBINT_ENA | HC_R0INT_ENA | HC_R1INT_ENA
		| HC_R2INT_ENA | HC_LAINT_ENA | HC_ERINT_ENA),
		phba->HCregaddr);
	writel((phba->ha_copy & ~(HA_LATT | HA_ERATT)), phba->HAregaddr);
	writel(hc_copy, phba->HCregaddr);
	readl(phba->HAregaddr);  
	spin_unlock(&phba->hbalock);

	 

	 
	status1 = phba->ha_copy & (HA_MBATT | HA_LATT | HA_ERATT);

	 
	status2 = (phba->ha_copy & (HA_RXMASK  << (4*LPFC_ELS_RING)));
	status2 >>= (4*LPFC_ELS_RING);

	if (status1 || (status2 & HA_RXMASK))
		sp_irq_rc = lpfc_sli_sp_intr_handler(irq, dev_id);
	else
		sp_irq_rc = IRQ_NONE;

	 

	 
	status1 = (phba->ha_copy & (HA_RXMASK << (4*LPFC_FCP_RING)));
	status1 >>= (4*LPFC_FCP_RING);

	 
	if (phba->cfg_multi_ring_support == 2) {
		status2 = (phba->ha_copy & (HA_RXMASK << (4*LPFC_EXTRA_RING)));
		status2 >>= (4*LPFC_EXTRA_RING);
	} else
		status2 = 0;

	if ((status1 & HA_RXMASK) || (status2 & HA_RXMASK))
		fp_irq_rc = lpfc_sli_fp_intr_handler(irq, dev_id);
	else
		fp_irq_rc = IRQ_NONE;

	 
	return (sp_irq_rc == IRQ_HANDLED) ? sp_irq_rc : fp_irq_rc;
}   

 
void lpfc_sli4_els_xri_abort_event_proc(struct lpfc_hba *phba)
{
	struct lpfc_cq_event *cq_event;
	unsigned long iflags;

	 
	spin_lock_irqsave(&phba->hbalock, iflags);
	phba->hba_flag &= ~ELS_XRI_ABORT_EVENT;
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	 
	spin_lock_irqsave(&phba->sli4_hba.els_xri_abrt_list_lock, iflags);
	while (!list_empty(&phba->sli4_hba.sp_els_xri_aborted_work_queue)) {
		 
		list_remove_head(&phba->sli4_hba.sp_els_xri_aborted_work_queue,
				 cq_event, struct lpfc_cq_event, list);
		spin_unlock_irqrestore(&phba->sli4_hba.els_xri_abrt_list_lock,
				       iflags);
		 
		lpfc_sli4_els_xri_aborted(phba, &cq_event->cqe.wcqe_axri);

		 
		lpfc_sli4_cq_event_release(phba, cq_event);
		spin_lock_irqsave(&phba->sli4_hba.els_xri_abrt_list_lock,
				  iflags);
	}
	spin_unlock_irqrestore(&phba->sli4_hba.els_xri_abrt_list_lock, iflags);
}

 
static struct lpfc_iocbq *
lpfc_sli4_els_preprocess_rspiocbq(struct lpfc_hba *phba,
				  struct lpfc_iocbq *irspiocbq)
{
	struct lpfc_sli_ring *pring;
	struct lpfc_iocbq *cmdiocbq;
	struct lpfc_wcqe_complete *wcqe;
	unsigned long iflags;

	pring = lpfc_phba_elsring(phba);
	if (unlikely(!pring))
		return NULL;

	wcqe = &irspiocbq->cq_event.cqe.wcqe_cmpl;
	spin_lock_irqsave(&pring->ring_lock, iflags);
	pring->stats.iocb_event++;
	 
	cmdiocbq = lpfc_sli_iocbq_lookup_by_tag(phba, pring,
				bf_get(lpfc_wcqe_c_request_tag, wcqe));
	if (unlikely(!cmdiocbq)) {
		spin_unlock_irqrestore(&pring->ring_lock, iflags);
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"0386 ELS complete with no corresponding "
				"cmdiocb: 0x%x 0x%x 0x%x 0x%x\n",
				wcqe->word0, wcqe->total_data_placed,
				wcqe->parameter, wcqe->word3);
		lpfc_sli_release_iocbq(phba, irspiocbq);
		return NULL;
	}

	memcpy(&irspiocbq->wqe, &cmdiocbq->wqe, sizeof(union lpfc_wqe128));
	memcpy(&irspiocbq->wcqe_cmpl, wcqe, sizeof(*wcqe));

	 
	lpfc_sli_ringtxcmpl_put(phba, pring, cmdiocbq);
	spin_unlock_irqrestore(&pring->ring_lock, iflags);

	if (bf_get(lpfc_wcqe_c_xb, wcqe)) {
		spin_lock_irqsave(&phba->hbalock, iflags);
		irspiocbq->cmd_flag |= LPFC_EXCHANGE_BUSY;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
	}

	return irspiocbq;
}

inline struct lpfc_cq_event *
lpfc_cq_event_setup(struct lpfc_hba *phba, void *entry, int size)
{
	struct lpfc_cq_event *cq_event;

	 
	cq_event = lpfc_sli4_cq_event_alloc(phba);
	if (!cq_event) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0602 Failed to alloc CQ_EVENT entry\n");
		return NULL;
	}

	 
	memcpy(&cq_event->cqe, entry, size);
	return cq_event;
}

 
static bool
lpfc_sli4_sp_handle_async_event(struct lpfc_hba *phba, struct lpfc_mcqe *mcqe)
{
	struct lpfc_cq_event *cq_event;
	unsigned long iflags;

	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"0392 Async Event: word0:x%x, word1:x%x, "
			"word2:x%x, word3:x%x\n", mcqe->word0,
			mcqe->mcqe_tag0, mcqe->mcqe_tag1, mcqe->trailer);

	cq_event = lpfc_cq_event_setup(phba, mcqe, sizeof(struct lpfc_mcqe));
	if (!cq_event)
		return false;

	spin_lock_irqsave(&phba->sli4_hba.asynce_list_lock, iflags);
	list_add_tail(&cq_event->list, &phba->sli4_hba.sp_asynce_work_queue);
	spin_unlock_irqrestore(&phba->sli4_hba.asynce_list_lock, iflags);

	 
	spin_lock_irqsave(&phba->hbalock, iflags);
	phba->hba_flag |= ASYNC_EVENT;
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	return true;
}

 
static bool
lpfc_sli4_sp_handle_mbox_event(struct lpfc_hba *phba, struct lpfc_mcqe *mcqe)
{
	uint32_t mcqe_status;
	MAILBOX_t *mbox, *pmbox;
	struct lpfc_mqe *mqe;
	struct lpfc_vport *vport;
	struct lpfc_nodelist *ndlp;
	struct lpfc_dmabuf *mp;
	unsigned long iflags;
	LPFC_MBOXQ_t *pmb;
	bool workposted = false;
	int rc;

	 
	if (!bf_get(lpfc_trailer_completed, mcqe))
		goto out_no_mqe_complete;

	 
	spin_lock_irqsave(&phba->hbalock, iflags);
	pmb = phba->sli.mbox_active;
	if (unlikely(!pmb)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"1832 No pending MBOX command to handle\n");
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		goto out_no_mqe_complete;
	}
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	mqe = &pmb->u.mqe;
	pmbox = (MAILBOX_t *)&pmb->u.mqe;
	mbox = phba->mbox;
	vport = pmb->vport;

	 
	phba->last_completion_time = jiffies;
	del_timer(&phba->sli.mbox_tmo);

	 
	if (pmb->mbox_cmpl && mbox)
		lpfc_sli4_pcimem_bcopy(mbox, mqe, sizeof(struct lpfc_mqe));

	 
	mcqe_status = bf_get(lpfc_mcqe_status, mcqe);
	if (mcqe_status != MB_CQE_STATUS_SUCCESS) {
		if (bf_get(lpfc_mqe_status, mqe) == MBX_SUCCESS)
			bf_set(lpfc_mqe_status, mqe,
			       (LPFC_MBX_ERROR_RANGE | mcqe_status));
	}
	if (pmb->mbox_flag & LPFC_MBX_IMED_UNREG) {
		pmb->mbox_flag &= ~LPFC_MBX_IMED_UNREG;
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_MBOX_VPORT,
				      "MBOX dflt rpi: status:x%x rpi:x%x",
				      mcqe_status,
				      pmbox->un.varWords[0], 0);
		if (mcqe_status == MB_CQE_STATUS_SUCCESS) {
			mp = (struct lpfc_dmabuf *)(pmb->ctx_buf);
			ndlp = (struct lpfc_nodelist *)pmb->ctx_ndlp;

			 
			spin_lock_irqsave(&ndlp->lock, iflags);
			ndlp->nlp_flag |= NLP_UNREG_INP;
			spin_unlock_irqrestore(&ndlp->lock, iflags);
			lpfc_unreg_login(phba, vport->vpi,
					 pmbox->un.varWords[0], pmb);
			pmb->mbox_cmpl = lpfc_mbx_cmpl_dflt_rpi;
			pmb->ctx_buf = mp;

			 
			pmb->ctx_ndlp = ndlp;
			pmb->vport = vport;
			rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);
			if (rc != MBX_BUSY)
				lpfc_printf_log(phba, KERN_ERR,
						LOG_TRACE_EVENT,
						"0385 rc should "
						"have been MBX_BUSY\n");
			if (rc != MBX_NOT_FINISHED)
				goto send_current_mbox;
		}
	}
	spin_lock_irqsave(&phba->pport->work_port_lock, iflags);
	phba->pport->work_port_events &= ~WORKER_MBOX_TMO;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, iflags);

	 
	if (pmbox->mbxCommand == MBX_HEARTBEAT) {
		spin_lock_irqsave(&phba->hbalock, iflags);
		 
		phba->sli.sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
		phba->sli.mbox_active = NULL;
		if (bf_get(lpfc_trailer_consumed, mcqe))
			lpfc_sli4_mq_release(phba->sli4_hba.mbx_wq);
		spin_unlock_irqrestore(&phba->hbalock, iflags);

		 
		lpfc_sli4_post_async_mbox(phba);

		 
		if (pmb->mbox_cmpl)
			pmb->mbox_cmpl(phba, pmb);
		return false;
	}

	 
	spin_lock_irqsave(&phba->hbalock, iflags);
	__lpfc_mbox_cmpl_put(phba, pmb);
	phba->work_ha |= HA_MBATT;
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	workposted = true;

send_current_mbox:
	spin_lock_irqsave(&phba->hbalock, iflags);
	 
	phba->sli.sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
	 
	phba->sli.mbox_active = NULL;
	if (bf_get(lpfc_trailer_consumed, mcqe))
		lpfc_sli4_mq_release(phba->sli4_hba.mbx_wq);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	 
	lpfc_worker_wake_up(phba);
	return workposted;

out_no_mqe_complete:
	spin_lock_irqsave(&phba->hbalock, iflags);
	if (bf_get(lpfc_trailer_consumed, mcqe))
		lpfc_sli4_mq_release(phba->sli4_hba.mbx_wq);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return false;
}

 
static bool
lpfc_sli4_sp_handle_mcqe(struct lpfc_hba *phba, struct lpfc_queue *cq,
			 struct lpfc_cqe *cqe)
{
	struct lpfc_mcqe mcqe;
	bool workposted;

	cq->CQ_mbox++;

	 
	lpfc_sli4_pcimem_bcopy(cqe, &mcqe, sizeof(struct lpfc_mcqe));

	 
	if (!bf_get(lpfc_trailer_async, &mcqe))
		workposted = lpfc_sli4_sp_handle_mbox_event(phba, &mcqe);
	else
		workposted = lpfc_sli4_sp_handle_async_event(phba, &mcqe);
	return workposted;
}

 
static bool
lpfc_sli4_sp_handle_els_wcqe(struct lpfc_hba *phba, struct lpfc_queue *cq,
			     struct lpfc_wcqe_complete *wcqe)
{
	struct lpfc_iocbq *irspiocbq;
	unsigned long iflags;
	struct lpfc_sli_ring *pring = cq->pring;
	int txq_cnt = 0;
	int txcmplq_cnt = 0;

	 
	if (unlikely(bf_get(lpfc_wcqe_c_status, wcqe))) {
		 
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"0357 ELS CQE error: status=x%x: "
				"CQE: %08x %08x %08x %08x\n",
				bf_get(lpfc_wcqe_c_status, wcqe),
				wcqe->word0, wcqe->total_data_placed,
				wcqe->parameter, wcqe->word3);
	}

	 
	irspiocbq = lpfc_sli_get_iocbq(phba);
	if (!irspiocbq) {
		if (!list_empty(&pring->txq))
			txq_cnt++;
		if (!list_empty(&pring->txcmplq))
			txcmplq_cnt++;
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"0387 NO IOCBQ data: txq_cnt=%d iocb_cnt=%d "
			"els_txcmplq_cnt=%d\n",
			txq_cnt, phba->iocb_cnt,
			txcmplq_cnt);
		return false;
	}

	 
	memcpy(&irspiocbq->cq_event.cqe.wcqe_cmpl, wcqe, sizeof(*wcqe));
	spin_lock_irqsave(&phba->hbalock, iflags);
	list_add_tail(&irspiocbq->cq_event.list,
		      &phba->sli4_hba.sp_queue_event);
	phba->hba_flag |= HBA_SP_QUEUE_EVT;
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	return true;
}

 
static void
lpfc_sli4_sp_handle_rel_wcqe(struct lpfc_hba *phba,
			     struct lpfc_wcqe_release *wcqe)
{
	 
	if (unlikely(!phba->sli4_hba.els_wq))
		return;
	 
	if (bf_get(lpfc_wcqe_r_wq_id, wcqe) == phba->sli4_hba.els_wq->queue_id)
		lpfc_sli4_wq_release(phba->sli4_hba.els_wq,
				     bf_get(lpfc_wcqe_r_wqe_index, wcqe));
	else
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"2579 Slow-path wqe consume event carries "
				"miss-matched qid: wcqe-qid=x%x, sp-qid=x%x\n",
				bf_get(lpfc_wcqe_r_wqe_index, wcqe),
				phba->sli4_hba.els_wq->queue_id);
}

 
static bool
lpfc_sli4_sp_handle_abort_xri_wcqe(struct lpfc_hba *phba,
				   struct lpfc_queue *cq,
				   struct sli4_wcqe_xri_aborted *wcqe)
{
	bool workposted = false;
	struct lpfc_cq_event *cq_event;
	unsigned long iflags;

	switch (cq->subtype) {
	case LPFC_IO:
		lpfc_sli4_io_xri_aborted(phba, wcqe, cq->hdwq);
		if (phba->cfg_enable_fc4_type & LPFC_ENABLE_NVME) {
			 
			if (phba->nvmet_support)
				lpfc_sli4_nvmet_xri_aborted(phba, wcqe);
		}
		workposted = false;
		break;
	case LPFC_NVME_LS:  
	case LPFC_ELS:
		cq_event = lpfc_cq_event_setup(phba, wcqe, sizeof(*wcqe));
		if (!cq_event) {
			workposted = false;
			break;
		}
		cq_event->hdwq = cq->hdwq;
		spin_lock_irqsave(&phba->sli4_hba.els_xri_abrt_list_lock,
				  iflags);
		list_add_tail(&cq_event->list,
			      &phba->sli4_hba.sp_els_xri_aborted_work_queue);
		 
		phba->hba_flag |= ELS_XRI_ABORT_EVENT;
		spin_unlock_irqrestore(&phba->sli4_hba.els_xri_abrt_list_lock,
				       iflags);
		workposted = true;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0603 Invalid CQ subtype %d: "
				"%08x %08x %08x %08x\n",
				cq->subtype, wcqe->word0, wcqe->parameter,
				wcqe->word2, wcqe->word3);
		workposted = false;
		break;
	}
	return workposted;
}

#define FC_RCTL_MDS_DIAGS	0xF4

 
static bool
lpfc_sli4_sp_handle_rcqe(struct lpfc_hba *phba, struct lpfc_rcqe *rcqe)
{
	bool workposted = false;
	struct fc_frame_header *fc_hdr;
	struct lpfc_queue *hrq = phba->sli4_hba.hdr_rq;
	struct lpfc_queue *drq = phba->sli4_hba.dat_rq;
	struct lpfc_nvmet_tgtport *tgtp;
	struct hbq_dmabuf *dma_buf;
	uint32_t status, rq_id;
	unsigned long iflags;

	 
	if (unlikely(!hrq) || unlikely(!drq))
		return workposted;

	if (bf_get(lpfc_cqe_code, rcqe) == CQE_CODE_RECEIVE_V1)
		rq_id = bf_get(lpfc_rcqe_rq_id_v1, rcqe);
	else
		rq_id = bf_get(lpfc_rcqe_rq_id, rcqe);
	if (rq_id != hrq->queue_id)
		goto out;

	status = bf_get(lpfc_rcqe_status, rcqe);
	switch (status) {
	case FC_STATUS_RQ_BUF_LEN_EXCEEDED:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2537 Receive Frame Truncated!!\n");
		fallthrough;
	case FC_STATUS_RQ_SUCCESS:
		spin_lock_irqsave(&phba->hbalock, iflags);
		lpfc_sli4_rq_release(hrq, drq);
		dma_buf = lpfc_sli_hbqbuf_get(&phba->hbqs[0].hbq_buffer_list);
		if (!dma_buf) {
			hrq->RQ_no_buf_found++;
			spin_unlock_irqrestore(&phba->hbalock, iflags);
			goto out;
		}
		hrq->RQ_rcv_buf++;
		hrq->RQ_buf_posted--;
		memcpy(&dma_buf->cq_event.cqe.rcqe_cmpl, rcqe, sizeof(*rcqe));

		fc_hdr = (struct fc_frame_header *)dma_buf->hbuf.virt;

		if (fc_hdr->fh_r_ctl == FC_RCTL_MDS_DIAGS ||
		    fc_hdr->fh_r_ctl == FC_RCTL_DD_UNSOL_DATA) {
			spin_unlock_irqrestore(&phba->hbalock, iflags);
			 
			if  (!(phba->pport->load_flag & FC_UNLOADING))
				lpfc_sli4_handle_mds_loopback(phba->pport,
							      dma_buf);
			else
				lpfc_in_buf_free(phba, &dma_buf->dbuf);
			break;
		}

		 
		list_add_tail(&dma_buf->cq_event.list,
			      &phba->sli4_hba.sp_queue_event);
		 
		phba->hba_flag |= HBA_SP_QUEUE_EVT;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		workposted = true;
		break;
	case FC_STATUS_INSUFF_BUF_FRM_DISC:
		if (phba->nvmet_support) {
			tgtp = phba->targetport->private;
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"6402 RQE Error x%x, posted %d err_cnt "
					"%d: %x %x %x\n",
					status, hrq->RQ_buf_posted,
					hrq->RQ_no_posted_buf,
					atomic_read(&tgtp->rcv_fcp_cmd_in),
					atomic_read(&tgtp->rcv_fcp_cmd_out),
					atomic_read(&tgtp->xmt_fcp_release));
		}
		fallthrough;

	case FC_STATUS_INSUFF_BUF_NEED_BUF:
		hrq->RQ_no_posted_buf++;
		 
		spin_lock_irqsave(&phba->hbalock, iflags);
		phba->hba_flag |= HBA_POST_RECEIVE_BUFFER;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		workposted = true;
		break;
	case FC_STATUS_RQ_DMA_FAILURE:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2564 RQE DMA Error x%x, x%08x x%08x x%08x "
				"x%08x\n",
				status, rcqe->word0, rcqe->word1,
				rcqe->word2, rcqe->word3);

		 
		if (bf_get(lpfc_rcqe_iv, rcqe))
			break;

		 
		spin_lock_irqsave(&phba->hbalock, iflags);
		lpfc_sli4_rq_release(hrq, drq);
		dma_buf = lpfc_sli_hbqbuf_get(&phba->hbqs[0].hbq_buffer_list);
		if (!dma_buf) {
			hrq->RQ_no_buf_found++;
			spin_unlock_irqrestore(&phba->hbalock, iflags);
			break;
		}
		hrq->RQ_rcv_buf++;
		hrq->RQ_buf_posted--;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		lpfc_in_buf_free(phba, &dma_buf->dbuf);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2565 Unexpected RQE Status x%x, w0-3 x%08x "
				"x%08x x%08x x%08x\n",
				status, rcqe->word0, rcqe->word1,
				rcqe->word2, rcqe->word3);
		break;
	}
out:
	return workposted;
}

 
static bool
lpfc_sli4_sp_handle_cqe(struct lpfc_hba *phba, struct lpfc_queue *cq,
			 struct lpfc_cqe *cqe)
{
	struct lpfc_cqe cqevt;
	bool workposted = false;

	 
	lpfc_sli4_pcimem_bcopy(cqe, &cqevt, sizeof(struct lpfc_cqe));

	 
	switch (bf_get(lpfc_cqe_code, &cqevt)) {
	case CQE_CODE_COMPL_WQE:
		 
		phba->last_completion_time = jiffies;
		workposted = lpfc_sli4_sp_handle_els_wcqe(phba, cq,
				(struct lpfc_wcqe_complete *)&cqevt);
		break;
	case CQE_CODE_RELEASE_WQE:
		 
		lpfc_sli4_sp_handle_rel_wcqe(phba,
				(struct lpfc_wcqe_release *)&cqevt);
		break;
	case CQE_CODE_XRI_ABORTED:
		 
		phba->last_completion_time = jiffies;
		workposted = lpfc_sli4_sp_handle_abort_xri_wcqe(phba, cq,
				(struct sli4_wcqe_xri_aborted *)&cqevt);
		break;
	case CQE_CODE_RECEIVE:
	case CQE_CODE_RECEIVE_V1:
		 
		phba->last_completion_time = jiffies;
		workposted = lpfc_sli4_sp_handle_rcqe(phba,
				(struct lpfc_rcqe *)&cqevt);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0388 Not a valid WCQE code: x%x\n",
				bf_get(lpfc_cqe_code, &cqevt));
		break;
	}
	return workposted;
}

 
static void
lpfc_sli4_sp_handle_eqe(struct lpfc_hba *phba, struct lpfc_eqe *eqe,
	struct lpfc_queue *speq)
{
	struct lpfc_queue *cq = NULL, *childq;
	uint16_t cqid;
	int ret = 0;

	 
	cqid = bf_get_le32(lpfc_eqe_resource_id, eqe);

	list_for_each_entry(childq, &speq->child_list, list) {
		if (childq->queue_id == cqid) {
			cq = childq;
			break;
		}
	}
	if (unlikely(!cq)) {
		if (phba->sli.sli_flag & LPFC_SLI_ACTIVE)
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0365 Slow-path CQ identifier "
					"(%d) does not exist\n", cqid);
		return;
	}

	 
	cq->assoc_qp = speq;

	if (is_kdump_kernel())
		ret = queue_work(phba->wq, &cq->spwork);
	else
		ret = queue_work_on(cq->chann, phba->wq, &cq->spwork);

	if (!ret)
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0390 Cannot schedule queue work "
				"for CQ eqcqid=%d, cqid=%d on CPU %d\n",
				cqid, cq->queue_id, raw_smp_processor_id());
}

 
static bool
__lpfc_sli4_process_cq(struct lpfc_hba *phba, struct lpfc_queue *cq,
	bool (*handler)(struct lpfc_hba *, struct lpfc_queue *,
			struct lpfc_cqe *), unsigned long *delay)
{
	struct lpfc_cqe *cqe;
	bool workposted = false;
	int count = 0, consumed = 0;
	bool arm = true;

	 
	*delay = 0;

	if (cmpxchg(&cq->queue_claimed, 0, 1) != 0)
		goto rearm_and_exit;

	 
	cq->q_flag = 0;
	cqe = lpfc_sli4_cq_get(cq);
	while (cqe) {
		workposted |= handler(phba, cq, cqe);
		__lpfc_sli4_consume_cqe(phba, cq, cqe);

		consumed++;
		if (!(++count % cq->max_proc_limit))
			break;

		if (!(count % cq->notify_interval)) {
			phba->sli4_hba.sli4_write_cq_db(phba, cq, consumed,
						LPFC_QUEUE_NOARM);
			consumed = 0;
			cq->assoc_qp->q_flag |= HBA_EQ_DELAY_CHK;
		}

		if (count == LPFC_NVMET_CQ_NOTIFY)
			cq->q_flag |= HBA_NVMET_CQ_NOTIFY;

		cqe = lpfc_sli4_cq_get(cq);
	}
	if (count >= phba->cfg_cq_poll_threshold) {
		*delay = 1;
		arm = false;
	}

	 
	if (count > cq->CQ_max_cqe)
		cq->CQ_max_cqe = count;

	cq->assoc_qp->EQ_cqe_cnt += count;

	 
	if (unlikely(count == 0))
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"0369 No entry from completion queue "
				"qid=%d\n", cq->queue_id);

	xchg(&cq->queue_claimed, 0);

rearm_and_exit:
	phba->sli4_hba.sli4_write_cq_db(phba, cq, consumed,
			arm ?  LPFC_QUEUE_REARM : LPFC_QUEUE_NOARM);

	return workposted;
}

 
static void
__lpfc_sli4_sp_process_cq(struct lpfc_queue *cq)
{
	struct lpfc_hba *phba = cq->phba;
	unsigned long delay;
	bool workposted = false;
	int ret = 0;

	 
	switch (cq->type) {
	case LPFC_MCQ:
		workposted |= __lpfc_sli4_process_cq(phba, cq,
						lpfc_sli4_sp_handle_mcqe,
						&delay);
		break;
	case LPFC_WCQ:
		if (cq->subtype == LPFC_IO)
			workposted |= __lpfc_sli4_process_cq(phba, cq,
						lpfc_sli4_fp_handle_cqe,
						&delay);
		else
			workposted |= __lpfc_sli4_process_cq(phba, cq,
						lpfc_sli4_sp_handle_cqe,
						&delay);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0370 Invalid completion queue type (%d)\n",
				cq->type);
		return;
	}

	if (delay) {
		if (is_kdump_kernel())
			ret = queue_delayed_work(phba->wq, &cq->sched_spwork,
						delay);
		else
			ret = queue_delayed_work_on(cq->chann, phba->wq,
						&cq->sched_spwork, delay);
		if (!ret)
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0394 Cannot schedule queue work "
				"for cqid=%d on CPU %d\n",
				cq->queue_id, cq->chann);
	}

	 
	if (workposted)
		lpfc_worker_wake_up(phba);
}

 
static void
lpfc_sli4_sp_process_cq(struct work_struct *work)
{
	struct lpfc_queue *cq = container_of(work, struct lpfc_queue, spwork);

	__lpfc_sli4_sp_process_cq(cq);
}

 
static void
lpfc_sli4_dly_sp_process_cq(struct work_struct *work)
{
	struct lpfc_queue *cq = container_of(to_delayed_work(work),
					struct lpfc_queue, sched_spwork);

	__lpfc_sli4_sp_process_cq(cq);
}

 
static void
lpfc_sli4_fp_handle_fcp_wcqe(struct lpfc_hba *phba, struct lpfc_queue *cq,
			     struct lpfc_wcqe_complete *wcqe)
{
	struct lpfc_sli_ring *pring = cq->pring;
	struct lpfc_iocbq *cmdiocbq;
	unsigned long iflags;

	 
	if (unlikely(bf_get(lpfc_wcqe_c_status, wcqe))) {
		 
		if (((bf_get(lpfc_wcqe_c_status, wcqe) ==
		     IOSTAT_LOCAL_REJECT)) &&
		    ((wcqe->parameter & IOERR_PARAM_MASK) ==
		     IOERR_NO_RESOURCES))
			phba->lpfc_rampdown_queue_depth(phba);

		 
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"0373 FCP CQE cmpl: status=x%x: "
				"CQE: %08x %08x %08x %08x\n",
				bf_get(lpfc_wcqe_c_status, wcqe),
				wcqe->word0, wcqe->total_data_placed,
				wcqe->parameter, wcqe->word3);
	}

	 
	spin_lock_irqsave(&pring->ring_lock, iflags);
	pring->stats.iocb_event++;
	cmdiocbq = lpfc_sli_iocbq_lookup_by_tag(phba, pring,
				bf_get(lpfc_wcqe_c_request_tag, wcqe));
	spin_unlock_irqrestore(&pring->ring_lock, iflags);
	if (unlikely(!cmdiocbq)) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"0374 FCP complete with no corresponding "
				"cmdiocb: iotag (%d)\n",
				bf_get(lpfc_wcqe_c_request_tag, wcqe));
		return;
	}
#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	cmdiocbq->isr_timestamp = cq->isr_timestamp;
#endif
	if (bf_get(lpfc_wcqe_c_xb, wcqe)) {
		spin_lock_irqsave(&phba->hbalock, iflags);
		cmdiocbq->cmd_flag |= LPFC_EXCHANGE_BUSY;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
	}

	if (cmdiocbq->cmd_cmpl) {
		 
		if (!(cmdiocbq->cmd_flag & LPFC_IO_FCP) &&
		    cmdiocbq->cmd_flag & LPFC_DRIVER_ABORTED) {
			spin_lock_irqsave(&phba->hbalock, iflags);
			cmdiocbq->cmd_flag &= ~LPFC_DRIVER_ABORTED;
			spin_unlock_irqrestore(&phba->hbalock, iflags);
		}

		 
		memcpy(&cmdiocbq->wcqe_cmpl, wcqe,
		       sizeof(struct lpfc_wcqe_complete));
		cmdiocbq->cmd_cmpl(phba, cmdiocbq, cmdiocbq);
	} else {
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"0375 FCP cmdiocb not callback function "
				"iotag: (%d)\n",
				bf_get(lpfc_wcqe_c_request_tag, wcqe));
	}
}

 
static void
lpfc_sli4_fp_handle_rel_wcqe(struct lpfc_hba *phba, struct lpfc_queue *cq,
			     struct lpfc_wcqe_release *wcqe)
{
	struct lpfc_queue *childwq;
	bool wqid_matched = false;
	uint16_t hba_wqid;

	 
	hba_wqid = bf_get(lpfc_wcqe_r_wq_id, wcqe);
	list_for_each_entry(childwq, &cq->child_list, list) {
		if (childwq->queue_id == hba_wqid) {
			lpfc_sli4_wq_release(childwq,
					bf_get(lpfc_wcqe_r_wqe_index, wcqe));
			if (childwq->q_flag & HBA_NVMET_WQFULL)
				lpfc_nvmet_wqfull_process(phba, childwq);
			wqid_matched = true;
			break;
		}
	}
	 
	if (wqid_matched != true)
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"2580 Fast-path wqe consume event carries "
				"miss-matched qid: wcqe-qid=x%x\n", hba_wqid);
}

 
static bool
lpfc_sli4_nvmet_handle_rcqe(struct lpfc_hba *phba, struct lpfc_queue *cq,
			    struct lpfc_rcqe *rcqe)
{
	bool workposted = false;
	struct lpfc_queue *hrq;
	struct lpfc_queue *drq;
	struct rqb_dmabuf *dma_buf;
	struct fc_frame_header *fc_hdr;
	struct lpfc_nvmet_tgtport *tgtp;
	uint32_t status, rq_id;
	unsigned long iflags;
	uint32_t fctl, idx;

	if ((phba->nvmet_support == 0) ||
	    (phba->sli4_hba.nvmet_cqset == NULL))
		return workposted;

	idx = cq->queue_id - phba->sli4_hba.nvmet_cqset[0]->queue_id;
	hrq = phba->sli4_hba.nvmet_mrq_hdr[idx];
	drq = phba->sli4_hba.nvmet_mrq_data[idx];

	 
	if (unlikely(!hrq) || unlikely(!drq))
		return workposted;

	if (bf_get(lpfc_cqe_code, rcqe) == CQE_CODE_RECEIVE_V1)
		rq_id = bf_get(lpfc_rcqe_rq_id_v1, rcqe);
	else
		rq_id = bf_get(lpfc_rcqe_rq_id, rcqe);

	if ((phba->nvmet_support == 0) ||
	    (rq_id != hrq->queue_id))
		return workposted;

	status = bf_get(lpfc_rcqe_status, rcqe);
	switch (status) {
	case FC_STATUS_RQ_BUF_LEN_EXCEEDED:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"6126 Receive Frame Truncated!!\n");
		fallthrough;
	case FC_STATUS_RQ_SUCCESS:
		spin_lock_irqsave(&phba->hbalock, iflags);
		lpfc_sli4_rq_release(hrq, drq);
		dma_buf = lpfc_sli_rqbuf_get(phba, hrq);
		if (!dma_buf) {
			hrq->RQ_no_buf_found++;
			spin_unlock_irqrestore(&phba->hbalock, iflags);
			goto out;
		}
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		hrq->RQ_rcv_buf++;
		hrq->RQ_buf_posted--;
		fc_hdr = (struct fc_frame_header *)dma_buf->hbuf.virt;

		 
		fctl = (fc_hdr->fh_f_ctl[0] << 16 |
			fc_hdr->fh_f_ctl[1] << 8 |
			fc_hdr->fh_f_ctl[2]);
		if (((fctl &
		    (FC_FC_FIRST_SEQ | FC_FC_END_SEQ | FC_FC_SEQ_INIT)) !=
		    (FC_FC_FIRST_SEQ | FC_FC_END_SEQ | FC_FC_SEQ_INIT)) ||
		    (fc_hdr->fh_seq_cnt != 0))  
			goto drop;

		if (fc_hdr->fh_type == FC_TYPE_FCP) {
			dma_buf->bytes_recv = bf_get(lpfc_rcqe_length, rcqe);
			lpfc_nvmet_unsol_fcp_event(
				phba, idx, dma_buf, cq->isr_timestamp,
				cq->q_flag & HBA_NVMET_CQ_NOTIFY);
			return false;
		}
drop:
		lpfc_rq_buf_free(phba, &dma_buf->hbuf);
		break;
	case FC_STATUS_INSUFF_BUF_FRM_DISC:
		if (phba->nvmet_support) {
			tgtp = phba->targetport->private;
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"6401 RQE Error x%x, posted %d err_cnt "
					"%d: %x %x %x\n",
					status, hrq->RQ_buf_posted,
					hrq->RQ_no_posted_buf,
					atomic_read(&tgtp->rcv_fcp_cmd_in),
					atomic_read(&tgtp->rcv_fcp_cmd_out),
					atomic_read(&tgtp->xmt_fcp_release));
		}
		fallthrough;

	case FC_STATUS_INSUFF_BUF_NEED_BUF:
		hrq->RQ_no_posted_buf++;
		 
		break;
	case FC_STATUS_RQ_DMA_FAILURE:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2575 RQE DMA Error x%x, x%08x x%08x x%08x "
				"x%08x\n",
				status, rcqe->word0, rcqe->word1,
				rcqe->word2, rcqe->word3);

		 
		if (bf_get(lpfc_rcqe_iv, rcqe))
			break;

		 
		spin_lock_irqsave(&phba->hbalock, iflags);
		lpfc_sli4_rq_release(hrq, drq);
		dma_buf = lpfc_sli_rqbuf_get(phba, hrq);
		if (!dma_buf) {
			hrq->RQ_no_buf_found++;
			spin_unlock_irqrestore(&phba->hbalock, iflags);
			break;
		}
		hrq->RQ_rcv_buf++;
		hrq->RQ_buf_posted--;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		lpfc_rq_buf_free(phba, &dma_buf->hbuf);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2576 Unexpected RQE Status x%x, w0-3 x%08x "
				"x%08x x%08x x%08x\n",
				status, rcqe->word0, rcqe->word1,
				rcqe->word2, rcqe->word3);
		break;
	}
out:
	return workposted;
}

 
static bool
lpfc_sli4_fp_handle_cqe(struct lpfc_hba *phba, struct lpfc_queue *cq,
			 struct lpfc_cqe *cqe)
{
	struct lpfc_wcqe_release wcqe;
	bool workposted = false;

	 
	lpfc_sli4_pcimem_bcopy(cqe, &wcqe, sizeof(struct lpfc_cqe));

	 
	switch (bf_get(lpfc_wcqe_c_code, &wcqe)) {
	case CQE_CODE_COMPL_WQE:
	case CQE_CODE_NVME_ERSP:
		cq->CQ_wq++;
		 
		phba->last_completion_time = jiffies;
		if (cq->subtype == LPFC_IO || cq->subtype == LPFC_NVME_LS)
			lpfc_sli4_fp_handle_fcp_wcqe(phba, cq,
				(struct lpfc_wcqe_complete *)&wcqe);
		break;
	case CQE_CODE_RELEASE_WQE:
		cq->CQ_release_wqe++;
		 
		lpfc_sli4_fp_handle_rel_wcqe(phba, cq,
				(struct lpfc_wcqe_release *)&wcqe);
		break;
	case CQE_CODE_XRI_ABORTED:
		cq->CQ_xri_aborted++;
		 
		phba->last_completion_time = jiffies;
		workposted = lpfc_sli4_sp_handle_abort_xri_wcqe(phba, cq,
				(struct sli4_wcqe_xri_aborted *)&wcqe);
		break;
	case CQE_CODE_RECEIVE_V1:
	case CQE_CODE_RECEIVE:
		phba->last_completion_time = jiffies;
		if (cq->subtype == LPFC_NVMET) {
			workposted = lpfc_sli4_nvmet_handle_rcqe(
				phba, cq, (struct lpfc_rcqe *)&wcqe);
		}
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0144 Not a valid CQE code: x%x\n",
				bf_get(lpfc_wcqe_c_code, &wcqe));
		break;
	}
	return workposted;
}

 
static void
__lpfc_sli4_hba_process_cq(struct lpfc_queue *cq)
{
	struct lpfc_hba *phba = cq->phba;
	unsigned long delay;
	bool workposted = false;
	int ret;

	 
	workposted |= __lpfc_sli4_process_cq(phba, cq, lpfc_sli4_fp_handle_cqe,
					     &delay);

	if (delay) {
		if (is_kdump_kernel())
			ret = queue_delayed_work(phba->wq, &cq->sched_irqwork,
						delay);
		else
			ret = queue_delayed_work_on(cq->chann, phba->wq,
						&cq->sched_irqwork, delay);
		if (!ret)
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0367 Cannot schedule queue work "
					"for cqid=%d on CPU %d\n",
					cq->queue_id, cq->chann);
	}

	 
	if (workposted)
		lpfc_worker_wake_up(phba);
}

 
static void
lpfc_sli4_hba_process_cq(struct work_struct *work)
{
	struct lpfc_queue *cq = container_of(work, struct lpfc_queue, irqwork);

	__lpfc_sli4_hba_process_cq(cq);
}

 
static void
lpfc_sli4_hba_handle_eqe(struct lpfc_hba *phba, struct lpfc_queue *eq,
			 struct lpfc_eqe *eqe, enum lpfc_poll_mode poll_mode)
{
	struct lpfc_queue *cq = NULL;
	uint32_t qidx = eq->hdwq;
	uint16_t cqid, id;
	int ret;

	if (unlikely(bf_get_le32(lpfc_eqe_major_code, eqe) != 0)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0366 Not a valid completion "
				"event: majorcode=x%x, minorcode=x%x\n",
				bf_get_le32(lpfc_eqe_major_code, eqe),
				bf_get_le32(lpfc_eqe_minor_code, eqe));
		return;
	}

	 
	cqid = bf_get_le32(lpfc_eqe_resource_id, eqe);

	 
	if (cqid <= phba->sli4_hba.cq_max) {
		cq = phba->sli4_hba.cq_lookup[cqid];
		if (cq)
			goto  work_cq;
	}

	 
	if (phba->cfg_nvmet_mrq && phba->sli4_hba.nvmet_cqset) {
		id = phba->sli4_hba.nvmet_cqset[0]->queue_id;
		if ((cqid >= id) && (cqid < (id + phba->cfg_nvmet_mrq))) {
			 
			cq = phba->sli4_hba.nvmet_cqset[cqid - id];
			goto  process_cq;
		}
	}

	if (phba->sli4_hba.nvmels_cq &&
	    (cqid == phba->sli4_hba.nvmels_cq->queue_id)) {
		 
		cq = phba->sli4_hba.nvmels_cq;
	}

	 
	if (cq == NULL) {
		lpfc_sli4_sp_handle_eqe(phba, eqe,
					phba->sli4_hba.hdwq[qidx].hba_eq);
		return;
	}

process_cq:
	if (unlikely(cqid != cq->queue_id)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0368 Miss-matched fast-path completion "
				"queue identifier: eqcqid=%d, fcpcqid=%d\n",
				cqid, cq->queue_id);
		return;
	}

work_cq:
#if defined(CONFIG_SCSI_LPFC_DEBUG_FS)
	if (phba->ktime_on)
		cq->isr_timestamp = ktime_get_ns();
	else
		cq->isr_timestamp = 0;
#endif

	switch (poll_mode) {
	case LPFC_THREADED_IRQ:
		__lpfc_sli4_hba_process_cq(cq);
		break;
	case LPFC_QUEUE_WORK:
	default:
		if (is_kdump_kernel())
			ret = queue_work(phba->wq, &cq->irqwork);
		else
			ret = queue_work_on(cq->chann, phba->wq, &cq->irqwork);
		if (!ret)
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"0383 Cannot schedule queue work "
					"for CQ eqcqid=%d, cqid=%d on CPU %d\n",
					cqid, cq->queue_id,
					raw_smp_processor_id());
		break;
	}
}

 
static void
lpfc_sli4_dly_hba_process_cq(struct work_struct *work)
{
	struct lpfc_queue *cq = container_of(to_delayed_work(work),
					struct lpfc_queue, sched_irqwork);

	__lpfc_sli4_hba_process_cq(cq);
}

 
irqreturn_t
lpfc_sli4_hba_intr_handler(int irq, void *dev_id)
{
	struct lpfc_hba *phba;
	struct lpfc_hba_eq_hdl *hba_eq_hdl;
	struct lpfc_queue *fpeq;
	unsigned long iflag;
	int hba_eqidx;
	int ecount = 0;
	struct lpfc_eq_intr_info *eqi;

	 
	hba_eq_hdl = (struct lpfc_hba_eq_hdl *)dev_id;
	phba = hba_eq_hdl->phba;
	hba_eqidx = hba_eq_hdl->idx;

	if (unlikely(!phba))
		return IRQ_NONE;
	if (unlikely(!phba->sli4_hba.hdwq))
		return IRQ_NONE;

	 
	fpeq = phba->sli4_hba.hba_eq_hdl[hba_eqidx].eq;
	if (unlikely(!fpeq))
		return IRQ_NONE;

	 
	if (unlikely(lpfc_intr_state_check(phba))) {
		 
		spin_lock_irqsave(&phba->hbalock, iflag);
		if (phba->link_state < LPFC_LINK_DOWN)
			 
			lpfc_sli4_eqcq_flush(phba, fpeq);
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		return IRQ_NONE;
	}

	switch (fpeq->poll_mode) {
	case LPFC_THREADED_IRQ:
		 
		if (phba->cmf_active_mode == LPFC_CFG_OFF)
			return IRQ_WAKE_THREAD;
		fallthrough;
	case LPFC_QUEUE_WORK:
	default:
		eqi = this_cpu_ptr(phba->sli4_hba.eq_info);
		eqi->icnt++;

		fpeq->last_cpu = raw_smp_processor_id();

		if (eqi->icnt > LPFC_EQD_ISR_TRIGGER &&
		    fpeq->q_flag & HBA_EQ_DELAY_CHK &&
		    phba->cfg_auto_imax &&
		    fpeq->q_mode != LPFC_MAX_AUTO_EQ_DELAY &&
		    phba->sli.sli_flag & LPFC_SLI_USE_EQDR)
			lpfc_sli4_mod_hba_eq_delay(phba, fpeq,
						   LPFC_MAX_AUTO_EQ_DELAY);

		 
		ecount = lpfc_sli4_process_eq(phba, fpeq, LPFC_QUEUE_REARM,
					      LPFC_QUEUE_WORK);

		if (unlikely(ecount == 0)) {
			fpeq->EQ_no_entry++;
			if (phba->intr_type == MSIX)
				 
				lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
						"0358 MSI-X interrupt with no EQE\n");
			else
				 
				return IRQ_NONE;
		}
	}

	return IRQ_HANDLED;
}  

 
irqreturn_t
lpfc_sli4_intr_handler(int irq, void *dev_id)
{
	struct lpfc_hba  *phba;
	irqreturn_t hba_irq_rc;
	bool hba_handled = false;
	int qidx;

	 
	phba = (struct lpfc_hba *)dev_id;

	if (unlikely(!phba))
		return IRQ_NONE;

	 
	for (qidx = 0; qidx < phba->cfg_irq_chann; qidx++) {
		hba_irq_rc = lpfc_sli4_hba_intr_handler(irq,
					&phba->sli4_hba.hba_eq_hdl[qidx]);
		if (hba_irq_rc == IRQ_HANDLED)
			hba_handled |= true;
	}

	return (hba_handled == true) ? IRQ_HANDLED : IRQ_NONE;
}  

void lpfc_sli4_poll_hbtimer(struct timer_list *t)
{
	struct lpfc_hba *phba = from_timer(phba, t, cpuhp_poll_timer);
	struct lpfc_queue *eq;

	rcu_read_lock();

	list_for_each_entry_rcu(eq, &phba->poll_list, _poll_list)
		lpfc_sli4_poll_eq(eq);
	if (!list_empty(&phba->poll_list))
		mod_timer(&phba->cpuhp_poll_timer,
			  jiffies + msecs_to_jiffies(LPFC_POLL_HB));

	rcu_read_unlock();
}

static inline void lpfc_sli4_add_to_poll_list(struct lpfc_queue *eq)
{
	struct lpfc_hba *phba = eq->phba;

	 
	if (list_empty(&phba->poll_list))
		mod_timer(&phba->cpuhp_poll_timer,
			  jiffies + msecs_to_jiffies(LPFC_POLL_HB));

	list_add_rcu(&eq->_poll_list, &phba->poll_list);
	synchronize_rcu();
}

static inline void lpfc_sli4_remove_from_poll_list(struct lpfc_queue *eq)
{
	struct lpfc_hba *phba = eq->phba;

	 
	list_del_rcu(&eq->_poll_list);
	synchronize_rcu();

	if (list_empty(&phba->poll_list))
		del_timer_sync(&phba->cpuhp_poll_timer);
}

void lpfc_sli4_cleanup_poll_list(struct lpfc_hba *phba)
{
	struct lpfc_queue *eq, *next;

	list_for_each_entry_safe(eq, next, &phba->poll_list, _poll_list)
		list_del(&eq->_poll_list);

	INIT_LIST_HEAD(&phba->poll_list);
	synchronize_rcu();
}

static inline void
__lpfc_sli4_switch_eqmode(struct lpfc_queue *eq, uint8_t mode)
{
	if (mode == eq->mode)
		return;
	 

	 
	WRITE_ONCE(eq->mode, mode);
	 
	smp_wmb();

	 
	mode ? lpfc_sli4_add_to_poll_list(eq) :
	       lpfc_sli4_remove_from_poll_list(eq);
}

void lpfc_sli4_start_polling(struct lpfc_queue *eq)
{
	__lpfc_sli4_switch_eqmode(eq, LPFC_EQ_POLL);
}

void lpfc_sli4_stop_polling(struct lpfc_queue *eq)
{
	struct lpfc_hba *phba = eq->phba;

	__lpfc_sli4_switch_eqmode(eq, LPFC_EQ_INTERRUPT);

	 
	phba->sli4_hba.sli4_write_eq_db(phba, eq, 0, LPFC_QUEUE_REARM);
}

 
void
lpfc_sli4_queue_free(struct lpfc_queue *queue)
{
	struct lpfc_dmabuf *dmabuf;

	if (!queue)
		return;

	if (!list_empty(&queue->wq_list))
		list_del(&queue->wq_list);

	while (!list_empty(&queue->page_list)) {
		list_remove_head(&queue->page_list, dmabuf, struct lpfc_dmabuf,
				 list);
		dma_free_coherent(&queue->phba->pcidev->dev, queue->page_size,
				  dmabuf->virt, dmabuf->phys);
		kfree(dmabuf);
	}
	if (queue->rqbp) {
		lpfc_free_rq_buffer(queue->phba, queue);
		kfree(queue->rqbp);
	}

	if (!list_empty(&queue->cpu_list))
		list_del(&queue->cpu_list);

	kfree(queue);
	return;
}

 
struct lpfc_queue *
lpfc_sli4_queue_alloc(struct lpfc_hba *phba, uint32_t page_size,
		      uint32_t entry_size, uint32_t entry_count, int cpu)
{
	struct lpfc_queue *queue;
	struct lpfc_dmabuf *dmabuf;
	uint32_t hw_page_size = phba->sli4_hba.pc_sli4_params.if_page_sz;
	uint16_t x, pgcnt;

	if (!phba->sli4_hba.pc_sli4_params.supported)
		hw_page_size = page_size;

	pgcnt = ALIGN(entry_size * entry_count, hw_page_size) / hw_page_size;

	 
	if (pgcnt > phba->sli4_hba.pc_sli4_params.wqpcnt)
		pgcnt = phba->sli4_hba.pc_sli4_params.wqpcnt;

	queue = kzalloc_node(sizeof(*queue) + (sizeof(void *) * pgcnt),
			     GFP_KERNEL, cpu_to_node(cpu));
	if (!queue)
		return NULL;

	INIT_LIST_HEAD(&queue->list);
	INIT_LIST_HEAD(&queue->_poll_list);
	INIT_LIST_HEAD(&queue->wq_list);
	INIT_LIST_HEAD(&queue->wqfull_list);
	INIT_LIST_HEAD(&queue->page_list);
	INIT_LIST_HEAD(&queue->child_list);
	INIT_LIST_HEAD(&queue->cpu_list);

	 
	queue->page_count = pgcnt;
	queue->q_pgs = (void **)&queue[1];
	queue->entry_cnt_per_pg = hw_page_size / entry_size;
	queue->entry_size = entry_size;
	queue->entry_count = entry_count;
	queue->page_size = hw_page_size;
	queue->phba = phba;

	for (x = 0; x < queue->page_count; x++) {
		dmabuf = kzalloc_node(sizeof(*dmabuf), GFP_KERNEL,
				      dev_to_node(&phba->pcidev->dev));
		if (!dmabuf)
			goto out_fail;
		dmabuf->virt = dma_alloc_coherent(&phba->pcidev->dev,
						  hw_page_size, &dmabuf->phys,
						  GFP_KERNEL);
		if (!dmabuf->virt) {
			kfree(dmabuf);
			goto out_fail;
		}
		dmabuf->buffer_tag = x;
		list_add_tail(&dmabuf->list, &queue->page_list);
		 
		queue->q_pgs[x] = dmabuf->virt;
	}
	INIT_WORK(&queue->irqwork, lpfc_sli4_hba_process_cq);
	INIT_WORK(&queue->spwork, lpfc_sli4_sp_process_cq);
	INIT_DELAYED_WORK(&queue->sched_irqwork, lpfc_sli4_dly_hba_process_cq);
	INIT_DELAYED_WORK(&queue->sched_spwork, lpfc_sli4_dly_sp_process_cq);

	 

	return queue;
out_fail:
	lpfc_sli4_queue_free(queue);
	return NULL;
}

 
static void __iomem *
lpfc_dual_chute_pci_bar_map(struct lpfc_hba *phba, uint16_t pci_barset)
{
	if (!phba->pcidev)
		return NULL;

	switch (pci_barset) {
	case WQ_PCI_BAR_0_AND_1:
		return phba->pci_bar0_memmap_p;
	case WQ_PCI_BAR_2_AND_3:
		return phba->pci_bar2_memmap_p;
	case WQ_PCI_BAR_4_AND_5:
		return phba->pci_bar4_memmap_p;
	default:
		break;
	}
	return NULL;
}

 
void
lpfc_modify_hba_eq_delay(struct lpfc_hba *phba, uint32_t startq,
			 uint32_t numq, uint32_t usdelay)
{
	struct lpfc_mbx_modify_eq_delay *eq_delay;
	LPFC_MBOXQ_t *mbox;
	struct lpfc_queue *eq;
	int cnt = 0, rc, length;
	uint32_t shdr_status, shdr_add_status;
	uint32_t dmult;
	int qidx;
	union lpfc_sli4_cfg_shdr *shdr;

	if (startq >= phba->cfg_irq_chann)
		return;

	if (usdelay > 0xFFFF) {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT | LOG_FCP | LOG_NVME,
				"6429 usdelay %d too large. Scaled down to "
				"0xFFFF.\n", usdelay);
		usdelay = 0xFFFF;
	}

	 
	if (phba->sli.sli_flag & LPFC_SLI_USE_EQDR) {
		for (qidx = startq; qidx < phba->cfg_irq_chann; qidx++) {
			eq = phba->sli4_hba.hba_eq_hdl[qidx].eq;
			if (!eq)
				continue;

			lpfc_sli4_mod_hba_eq_delay(phba, eq, usdelay);

			if (++cnt >= numq)
				break;
		}
		return;
	}

	 

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"6428 Failed allocating mailbox cmd buffer."
				" EQ delay was not set.\n");
		return;
	}
	length = (sizeof(struct lpfc_mbx_modify_eq_delay) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_MODIFY_EQ_DELAY,
			 length, LPFC_SLI4_MBX_EMBED);
	eq_delay = &mbox->u.mqe.un.eq_delay;

	 
	dmult = (usdelay * LPFC_DMULT_CONST) / LPFC_SEC_TO_USEC;
	if (dmult)
		dmult--;
	if (dmult > LPFC_DMULT_MAX)
		dmult = LPFC_DMULT_MAX;

	for (qidx = startq; qidx < phba->cfg_irq_chann; qidx++) {
		eq = phba->sli4_hba.hba_eq_hdl[qidx].eq;
		if (!eq)
			continue;
		eq->q_mode = usdelay;
		eq_delay->u.request.eq[cnt].eq_id = eq->queue_id;
		eq_delay->u.request.eq[cnt].phase = 0;
		eq_delay->u.request.eq[cnt].delay_multi = dmult;

		if (++cnt >= numq)
			break;
	}
	eq_delay->u.request.num_eq = cnt;

	mbox->vport = phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	mbox->ctx_ndlp = NULL;
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	shdr = (union lpfc_sli4_cfg_shdr *) &eq_delay->header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2512 MODIFY_EQ_DELAY mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
	}
	mempool_free(mbox, phba->mbox_mem_pool);
	return;
}

 
int
lpfc_eq_create(struct lpfc_hba *phba, struct lpfc_queue *eq, uint32_t imax)
{
	struct lpfc_mbx_eq_create *eq_create;
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	struct lpfc_dmabuf *dmabuf;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	uint16_t dmult;
	uint32_t hw_page_size = phba->sli4_hba.pc_sli4_params.if_page_sz;

	 
	if (!eq)
		return -ENODEV;
	if (!phba->sli4_hba.pc_sli4_params.supported)
		hw_page_size = SLI4_PAGE_SIZE;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_eq_create) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_EQ_CREATE,
			 length, LPFC_SLI4_MBX_EMBED);
	eq_create = &mbox->u.mqe.un.eq_create;
	shdr = (union lpfc_sli4_cfg_shdr *) &eq_create->header.cfg_shdr;
	bf_set(lpfc_mbx_eq_create_num_pages, &eq_create->u.request,
	       eq->page_count);
	bf_set(lpfc_eq_context_size, &eq_create->u.request.context,
	       LPFC_EQE_SIZE);
	bf_set(lpfc_eq_context_valid, &eq_create->u.request.context, 1);

	 
	if (phba->sli4_hba.pc_sli4_params.eqav) {
		bf_set(lpfc_mbox_hdr_version, &shdr->request,
		       LPFC_Q_CREATE_VERSION_2);
		bf_set(lpfc_eq_context_autovalid, &eq_create->u.request.context,
		       phba->sli4_hba.pc_sli4_params.eqav);
	}

	 
	dmult = 0;
	bf_set(lpfc_eq_context_delay_multi, &eq_create->u.request.context,
	       dmult);
	switch (eq->entry_count) {
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0360 Unsupported EQ count. (%d)\n",
				eq->entry_count);
		if (eq->entry_count < 256) {
			status = -EINVAL;
			goto out;
		}
		fallthrough;	 
	case 256:
		bf_set(lpfc_eq_context_count, &eq_create->u.request.context,
		       LPFC_EQ_CNT_256);
		break;
	case 512:
		bf_set(lpfc_eq_context_count, &eq_create->u.request.context,
		       LPFC_EQ_CNT_512);
		break;
	case 1024:
		bf_set(lpfc_eq_context_count, &eq_create->u.request.context,
		       LPFC_EQ_CNT_1024);
		break;
	case 2048:
		bf_set(lpfc_eq_context_count, &eq_create->u.request.context,
		       LPFC_EQ_CNT_2048);
		break;
	case 4096:
		bf_set(lpfc_eq_context_count, &eq_create->u.request.context,
		       LPFC_EQ_CNT_4096);
		break;
	}
	list_for_each_entry(dmabuf, &eq->page_list, list) {
		memset(dmabuf->virt, 0, hw_page_size);
		eq_create->u.request.page[dmabuf->buffer_tag].addr_lo =
					putPaddrLow(dmabuf->phys);
		eq_create->u.request.page[dmabuf->buffer_tag].addr_hi =
					putPaddrHigh(dmabuf->phys);
	}
	mbox->vport = phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	mbox->ctx_buf = NULL;
	mbox->ctx_ndlp = NULL;
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2500 EQ_CREATE mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
	}
	eq->type = LPFC_EQ;
	eq->subtype = LPFC_NONE;
	eq->queue_id = bf_get(lpfc_mbx_eq_create_q_id, &eq_create->u.response);
	if (eq->queue_id == 0xFFFF)
		status = -ENXIO;
	eq->host_index = 0;
	eq->notify_interval = LPFC_EQ_NOTIFY_INTRVL;
	eq->max_proc_limit = LPFC_EQ_MAX_PROC_LIMIT;
out:
	mempool_free(mbox, phba->mbox_mem_pool);
	return status;
}

 
irqreturn_t lpfc_sli4_hba_intr_handler_th(int irq, void *dev_id)
{
	struct lpfc_hba *phba;
	struct lpfc_hba_eq_hdl *hba_eq_hdl;
	struct lpfc_queue *fpeq;
	int ecount = 0;
	int hba_eqidx;
	struct lpfc_eq_intr_info *eqi;

	 
	hba_eq_hdl = (struct lpfc_hba_eq_hdl *)dev_id;
	phba = hba_eq_hdl->phba;
	hba_eqidx = hba_eq_hdl->idx;

	if (unlikely(!phba))
		return IRQ_NONE;
	if (unlikely(!phba->sli4_hba.hdwq))
		return IRQ_NONE;

	 
	fpeq = phba->sli4_hba.hba_eq_hdl[hba_eqidx].eq;
	if (unlikely(!fpeq))
		return IRQ_NONE;

	eqi = per_cpu_ptr(phba->sli4_hba.eq_info, raw_smp_processor_id());
	eqi->icnt++;

	fpeq->last_cpu = raw_smp_processor_id();

	if (eqi->icnt > LPFC_EQD_ISR_TRIGGER &&
	    fpeq->q_flag & HBA_EQ_DELAY_CHK &&
	    phba->cfg_auto_imax &&
	    fpeq->q_mode != LPFC_MAX_AUTO_EQ_DELAY &&
	    phba->sli.sli_flag & LPFC_SLI_USE_EQDR)
		lpfc_sli4_mod_hba_eq_delay(phba, fpeq, LPFC_MAX_AUTO_EQ_DELAY);

	 
	ecount = lpfc_sli4_process_eq(phba, fpeq, LPFC_QUEUE_REARM,
				      LPFC_THREADED_IRQ);

	if (unlikely(ecount == 0)) {
		fpeq->EQ_no_entry++;
		if (phba->intr_type == MSIX)
			 
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					"3358 MSI-X interrupt with no EQE\n");
		else
			 
			return IRQ_NONE;
	}
	return IRQ_HANDLED;
}

 
int
lpfc_cq_create(struct lpfc_hba *phba, struct lpfc_queue *cq,
	       struct lpfc_queue *eq, uint32_t type, uint32_t subtype)
{
	struct lpfc_mbx_cq_create *cq_create;
	struct lpfc_dmabuf *dmabuf;
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	 
	if (!cq || !eq)
		return -ENODEV;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_cq_create) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_CQ_CREATE,
			 length, LPFC_SLI4_MBX_EMBED);
	cq_create = &mbox->u.mqe.un.cq_create;
	shdr = (union lpfc_sli4_cfg_shdr *) &cq_create->header.cfg_shdr;
	bf_set(lpfc_mbx_cq_create_num_pages, &cq_create->u.request,
		    cq->page_count);
	bf_set(lpfc_cq_context_event, &cq_create->u.request.context, 1);
	bf_set(lpfc_cq_context_valid, &cq_create->u.request.context, 1);
	bf_set(lpfc_mbox_hdr_version, &shdr->request,
	       phba->sli4_hba.pc_sli4_params.cqv);
	if (phba->sli4_hba.pc_sli4_params.cqv == LPFC_Q_CREATE_VERSION_2) {
		bf_set(lpfc_mbx_cq_create_page_size, &cq_create->u.request,
		       (cq->page_size / SLI4_PAGE_SIZE));
		bf_set(lpfc_cq_eq_id_2, &cq_create->u.request.context,
		       eq->queue_id);
		bf_set(lpfc_cq_context_autovalid, &cq_create->u.request.context,
		       phba->sli4_hba.pc_sli4_params.cqav);
	} else {
		bf_set(lpfc_cq_eq_id, &cq_create->u.request.context,
		       eq->queue_id);
	}
	switch (cq->entry_count) {
	case 2048:
	case 4096:
		if (phba->sli4_hba.pc_sli4_params.cqv ==
		    LPFC_Q_CREATE_VERSION_2) {
			cq_create->u.request.context.lpfc_cq_context_count =
				cq->entry_count;
			bf_set(lpfc_cq_context_count,
			       &cq_create->u.request.context,
			       LPFC_CQ_CNT_WORD7);
			break;
		}
		fallthrough;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0361 Unsupported CQ count: "
				"entry cnt %d sz %d pg cnt %d\n",
				cq->entry_count, cq->entry_size,
				cq->page_count);
		if (cq->entry_count < 256) {
			status = -EINVAL;
			goto out;
		}
		fallthrough;	 
	case 256:
		bf_set(lpfc_cq_context_count, &cq_create->u.request.context,
		       LPFC_CQ_CNT_256);
		break;
	case 512:
		bf_set(lpfc_cq_context_count, &cq_create->u.request.context,
		       LPFC_CQ_CNT_512);
		break;
	case 1024:
		bf_set(lpfc_cq_context_count, &cq_create->u.request.context,
		       LPFC_CQ_CNT_1024);
		break;
	}
	list_for_each_entry(dmabuf, &cq->page_list, list) {
		memset(dmabuf->virt, 0, cq->page_size);
		cq_create->u.request.page[dmabuf->buffer_tag].addr_lo =
					putPaddrLow(dmabuf->phys);
		cq_create->u.request.page[dmabuf->buffer_tag].addr_hi =
					putPaddrHigh(dmabuf->phys);
	}
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);

	 
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2501 CQ_CREATE mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
		goto out;
	}
	cq->queue_id = bf_get(lpfc_mbx_cq_create_q_id, &cq_create->u.response);
	if (cq->queue_id == 0xFFFF) {
		status = -ENXIO;
		goto out;
	}
	 
	list_add_tail(&cq->list, &eq->child_list);
	 
	cq->type = type;
	cq->subtype = subtype;
	cq->queue_id = bf_get(lpfc_mbx_cq_create_q_id, &cq_create->u.response);
	cq->assoc_qid = eq->queue_id;
	cq->assoc_qp = eq;
	cq->host_index = 0;
	cq->notify_interval = LPFC_CQ_NOTIFY_INTRVL;
	cq->max_proc_limit = min(phba->cfg_cq_max_proc_limit, cq->entry_count);

	if (cq->queue_id > phba->sli4_hba.cq_max)
		phba->sli4_hba.cq_max = cq->queue_id;
out:
	mempool_free(mbox, phba->mbox_mem_pool);
	return status;
}

 
int
lpfc_cq_create_set(struct lpfc_hba *phba, struct lpfc_queue **cqp,
		   struct lpfc_sli4_hdw_queue *hdwq, uint32_t type,
		   uint32_t subtype)
{
	struct lpfc_queue *cq;
	struct lpfc_queue *eq;
	struct lpfc_mbx_cq_create_set *cq_set;
	struct lpfc_dmabuf *dmabuf;
	LPFC_MBOXQ_t *mbox;
	int rc, length, alloclen, status = 0;
	int cnt, idx, numcq, page_idx = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	uint32_t hw_page_size = phba->sli4_hba.pc_sli4_params.if_page_sz;

	 
	numcq = phba->cfg_nvmet_mrq;
	if (!cqp || !hdwq || !numcq)
		return -ENODEV;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	length = sizeof(struct lpfc_mbx_cq_create_set);
	length += ((numcq * cqp[0]->page_count) *
		   sizeof(struct dma_address));
	alloclen = lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			LPFC_MBOX_OPCODE_FCOE_CQ_CREATE_SET, length,
			LPFC_SLI4_MBX_NEMBED);
	if (alloclen < length) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3098 Allocated DMA memory size (%d) is "
				"less than the requested DMA memory size "
				"(%d)\n", alloclen, length);
		status = -ENOMEM;
		goto out;
	}
	cq_set = mbox->sge_array->addr[0];
	shdr = (union lpfc_sli4_cfg_shdr *)&cq_set->cfg_shdr;
	bf_set(lpfc_mbox_hdr_version, &shdr->request, 0);

	for (idx = 0; idx < numcq; idx++) {
		cq = cqp[idx];
		eq = hdwq[idx].hba_eq;
		if (!cq || !eq) {
			status = -ENOMEM;
			goto out;
		}
		if (!phba->sli4_hba.pc_sli4_params.supported)
			hw_page_size = cq->page_size;

		switch (idx) {
		case 0:
			bf_set(lpfc_mbx_cq_create_set_page_size,
			       &cq_set->u.request,
			       (hw_page_size / SLI4_PAGE_SIZE));
			bf_set(lpfc_mbx_cq_create_set_num_pages,
			       &cq_set->u.request, cq->page_count);
			bf_set(lpfc_mbx_cq_create_set_evt,
			       &cq_set->u.request, 1);
			bf_set(lpfc_mbx_cq_create_set_valid,
			       &cq_set->u.request, 1);
			bf_set(lpfc_mbx_cq_create_set_cqe_size,
			       &cq_set->u.request, 0);
			bf_set(lpfc_mbx_cq_create_set_num_cq,
			       &cq_set->u.request, numcq);
			bf_set(lpfc_mbx_cq_create_set_autovalid,
			       &cq_set->u.request,
			       phba->sli4_hba.pc_sli4_params.cqav);
			switch (cq->entry_count) {
			case 2048:
			case 4096:
				if (phba->sli4_hba.pc_sli4_params.cqv ==
				    LPFC_Q_CREATE_VERSION_2) {
					bf_set(lpfc_mbx_cq_create_set_cqe_cnt,
					       &cq_set->u.request,
						cq->entry_count);
					bf_set(lpfc_mbx_cq_create_set_cqe_cnt,
					       &cq_set->u.request,
					       LPFC_CQ_CNT_WORD7);
					break;
				}
				fallthrough;
			default:
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"3118 Bad CQ count. (%d)\n",
						cq->entry_count);
				if (cq->entry_count < 256) {
					status = -EINVAL;
					goto out;
				}
				fallthrough;	 
			case 256:
				bf_set(lpfc_mbx_cq_create_set_cqe_cnt,
				       &cq_set->u.request, LPFC_CQ_CNT_256);
				break;
			case 512:
				bf_set(lpfc_mbx_cq_create_set_cqe_cnt,
				       &cq_set->u.request, LPFC_CQ_CNT_512);
				break;
			case 1024:
				bf_set(lpfc_mbx_cq_create_set_cqe_cnt,
				       &cq_set->u.request, LPFC_CQ_CNT_1024);
				break;
			}
			bf_set(lpfc_mbx_cq_create_set_eq_id0,
			       &cq_set->u.request, eq->queue_id);
			break;
		case 1:
			bf_set(lpfc_mbx_cq_create_set_eq_id1,
			       &cq_set->u.request, eq->queue_id);
			break;
		case 2:
			bf_set(lpfc_mbx_cq_create_set_eq_id2,
			       &cq_set->u.request, eq->queue_id);
			break;
		case 3:
			bf_set(lpfc_mbx_cq_create_set_eq_id3,
			       &cq_set->u.request, eq->queue_id);
			break;
		case 4:
			bf_set(lpfc_mbx_cq_create_set_eq_id4,
			       &cq_set->u.request, eq->queue_id);
			break;
		case 5:
			bf_set(lpfc_mbx_cq_create_set_eq_id5,
			       &cq_set->u.request, eq->queue_id);
			break;
		case 6:
			bf_set(lpfc_mbx_cq_create_set_eq_id6,
			       &cq_set->u.request, eq->queue_id);
			break;
		case 7:
			bf_set(lpfc_mbx_cq_create_set_eq_id7,
			       &cq_set->u.request, eq->queue_id);
			break;
		case 8:
			bf_set(lpfc_mbx_cq_create_set_eq_id8,
			       &cq_set->u.request, eq->queue_id);
			break;
		case 9:
			bf_set(lpfc_mbx_cq_create_set_eq_id9,
			       &cq_set->u.request, eq->queue_id);
			break;
		case 10:
			bf_set(lpfc_mbx_cq_create_set_eq_id10,
			       &cq_set->u.request, eq->queue_id);
			break;
		case 11:
			bf_set(lpfc_mbx_cq_create_set_eq_id11,
			       &cq_set->u.request, eq->queue_id);
			break;
		case 12:
			bf_set(lpfc_mbx_cq_create_set_eq_id12,
			       &cq_set->u.request, eq->queue_id);
			break;
		case 13:
			bf_set(lpfc_mbx_cq_create_set_eq_id13,
			       &cq_set->u.request, eq->queue_id);
			break;
		case 14:
			bf_set(lpfc_mbx_cq_create_set_eq_id14,
			       &cq_set->u.request, eq->queue_id);
			break;
		case 15:
			bf_set(lpfc_mbx_cq_create_set_eq_id15,
			       &cq_set->u.request, eq->queue_id);
			break;
		}

		 
		list_add_tail(&cq->list, &eq->child_list);
		 
		cq->type = type;
		cq->subtype = subtype;
		cq->assoc_qid = eq->queue_id;
		cq->assoc_qp = eq;
		cq->host_index = 0;
		cq->notify_interval = LPFC_CQ_NOTIFY_INTRVL;
		cq->max_proc_limit = min(phba->cfg_cq_max_proc_limit,
					 cq->entry_count);
		cq->chann = idx;

		rc = 0;
		list_for_each_entry(dmabuf, &cq->page_list, list) {
			memset(dmabuf->virt, 0, hw_page_size);
			cnt = page_idx + dmabuf->buffer_tag;
			cq_set->u.request.page[cnt].addr_lo =
					putPaddrLow(dmabuf->phys);
			cq_set->u.request.page[cnt].addr_hi =
					putPaddrHigh(dmabuf->phys);
			rc++;
		}
		page_idx += rc;
	}

	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);

	 
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3119 CQ_CREATE_SET mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
		goto out;
	}
	rc = bf_get(lpfc_mbx_cq_create_set_base_id, &cq_set->u.response);
	if (rc == 0xFFFF) {
		status = -ENXIO;
		goto out;
	}

	for (idx = 0; idx < numcq; idx++) {
		cq = cqp[idx];
		cq->queue_id = rc + idx;
		if (cq->queue_id > phba->sli4_hba.cq_max)
			phba->sli4_hba.cq_max = cq->queue_id;
	}

out:
	lpfc_sli4_mbox_cmd_free(phba, mbox);
	return status;
}

 
static void
lpfc_mq_create_fb_init(struct lpfc_hba *phba, struct lpfc_queue *mq,
		       LPFC_MBOXQ_t *mbox, struct lpfc_queue *cq)
{
	struct lpfc_mbx_mq_create *mq_create;
	struct lpfc_dmabuf *dmabuf;
	int length;

	length = (sizeof(struct lpfc_mbx_mq_create) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_MQ_CREATE,
			 length, LPFC_SLI4_MBX_EMBED);
	mq_create = &mbox->u.mqe.un.mq_create;
	bf_set(lpfc_mbx_mq_create_num_pages, &mq_create->u.request,
	       mq->page_count);
	bf_set(lpfc_mq_context_cq_id, &mq_create->u.request.context,
	       cq->queue_id);
	bf_set(lpfc_mq_context_valid, &mq_create->u.request.context, 1);
	switch (mq->entry_count) {
	case 16:
		bf_set(lpfc_mq_context_ring_size, &mq_create->u.request.context,
		       LPFC_MQ_RING_SIZE_16);
		break;
	case 32:
		bf_set(lpfc_mq_context_ring_size, &mq_create->u.request.context,
		       LPFC_MQ_RING_SIZE_32);
		break;
	case 64:
		bf_set(lpfc_mq_context_ring_size, &mq_create->u.request.context,
		       LPFC_MQ_RING_SIZE_64);
		break;
	case 128:
		bf_set(lpfc_mq_context_ring_size, &mq_create->u.request.context,
		       LPFC_MQ_RING_SIZE_128);
		break;
	}
	list_for_each_entry(dmabuf, &mq->page_list, list) {
		mq_create->u.request.page[dmabuf->buffer_tag].addr_lo =
			putPaddrLow(dmabuf->phys);
		mq_create->u.request.page[dmabuf->buffer_tag].addr_hi =
			putPaddrHigh(dmabuf->phys);
	}
}

 
int32_t
lpfc_mq_create(struct lpfc_hba *phba, struct lpfc_queue *mq,
	       struct lpfc_queue *cq, uint32_t subtype)
{
	struct lpfc_mbx_mq_create *mq_create;
	struct lpfc_mbx_mq_create_ext *mq_create_ext;
	struct lpfc_dmabuf *dmabuf;
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	uint32_t hw_page_size = phba->sli4_hba.pc_sli4_params.if_page_sz;

	 
	if (!mq || !cq)
		return -ENODEV;
	if (!phba->sli4_hba.pc_sli4_params.supported)
		hw_page_size = SLI4_PAGE_SIZE;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_mq_create_ext) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_MQ_CREATE_EXT,
			 length, LPFC_SLI4_MBX_EMBED);

	mq_create_ext = &mbox->u.mqe.un.mq_create_ext;
	shdr = (union lpfc_sli4_cfg_shdr *) &mq_create_ext->header.cfg_shdr;
	bf_set(lpfc_mbx_mq_create_ext_num_pages,
	       &mq_create_ext->u.request, mq->page_count);
	bf_set(lpfc_mbx_mq_create_ext_async_evt_link,
	       &mq_create_ext->u.request, 1);
	bf_set(lpfc_mbx_mq_create_ext_async_evt_fip,
	       &mq_create_ext->u.request, 1);
	bf_set(lpfc_mbx_mq_create_ext_async_evt_group5,
	       &mq_create_ext->u.request, 1);
	bf_set(lpfc_mbx_mq_create_ext_async_evt_fc,
	       &mq_create_ext->u.request, 1);
	bf_set(lpfc_mbx_mq_create_ext_async_evt_sli,
	       &mq_create_ext->u.request, 1);
	bf_set(lpfc_mq_context_valid, &mq_create_ext->u.request.context, 1);
	bf_set(lpfc_mbox_hdr_version, &shdr->request,
	       phba->sli4_hba.pc_sli4_params.mqv);
	if (phba->sli4_hba.pc_sli4_params.mqv == LPFC_Q_CREATE_VERSION_1)
		bf_set(lpfc_mbx_mq_create_ext_cq_id, &mq_create_ext->u.request,
		       cq->queue_id);
	else
		bf_set(lpfc_mq_context_cq_id, &mq_create_ext->u.request.context,
		       cq->queue_id);
	switch (mq->entry_count) {
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0362 Unsupported MQ count. (%d)\n",
				mq->entry_count);
		if (mq->entry_count < 16) {
			status = -EINVAL;
			goto out;
		}
		fallthrough;	 
	case 16:
		bf_set(lpfc_mq_context_ring_size,
		       &mq_create_ext->u.request.context,
		       LPFC_MQ_RING_SIZE_16);
		break;
	case 32:
		bf_set(lpfc_mq_context_ring_size,
		       &mq_create_ext->u.request.context,
		       LPFC_MQ_RING_SIZE_32);
		break;
	case 64:
		bf_set(lpfc_mq_context_ring_size,
		       &mq_create_ext->u.request.context,
		       LPFC_MQ_RING_SIZE_64);
		break;
	case 128:
		bf_set(lpfc_mq_context_ring_size,
		       &mq_create_ext->u.request.context,
		       LPFC_MQ_RING_SIZE_128);
		break;
	}
	list_for_each_entry(dmabuf, &mq->page_list, list) {
		memset(dmabuf->virt, 0, hw_page_size);
		mq_create_ext->u.request.page[dmabuf->buffer_tag].addr_lo =
					putPaddrLow(dmabuf->phys);
		mq_create_ext->u.request.page[dmabuf->buffer_tag].addr_hi =
					putPaddrHigh(dmabuf->phys);
	}
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	mq->queue_id = bf_get(lpfc_mbx_mq_create_q_id,
			      &mq_create_ext->u.response);
	if (rc != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"2795 MQ_CREATE_EXT failed with "
				"status x%x. Failback to MQ_CREATE.\n",
				rc);
		lpfc_mq_create_fb_init(phba, mq, mbox, cq);
		mq_create = &mbox->u.mqe.un.mq_create;
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
		shdr = (union lpfc_sli4_cfg_shdr *) &mq_create->header.cfg_shdr;
		mq->queue_id = bf_get(lpfc_mbx_mq_create_q_id,
				      &mq_create->u.response);
	}

	 
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2502 MQ_CREATE mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
		goto out;
	}
	if (mq->queue_id == 0xFFFF) {
		status = -ENXIO;
		goto out;
	}
	mq->type = LPFC_MQ;
	mq->assoc_qid = cq->queue_id;
	mq->subtype = subtype;
	mq->host_index = 0;
	mq->hba_index = 0;

	 
	list_add_tail(&mq->list, &cq->child_list);
out:
	mempool_free(mbox, phba->mbox_mem_pool);
	return status;
}

 
int
lpfc_wq_create(struct lpfc_hba *phba, struct lpfc_queue *wq,
	       struct lpfc_queue *cq, uint32_t subtype)
{
	struct lpfc_mbx_wq_create *wq_create;
	struct lpfc_dmabuf *dmabuf;
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	uint32_t hw_page_size = phba->sli4_hba.pc_sli4_params.if_page_sz;
	struct dma_address *page;
	void __iomem *bar_memmap_p;
	uint32_t db_offset;
	uint16_t pci_barset;
	uint8_t dpp_barset;
	uint32_t dpp_offset;
	uint8_t wq_create_version;
#ifdef CONFIG_X86
	unsigned long pg_addr;
#endif

	 
	if (!wq || !cq)
		return -ENODEV;
	if (!phba->sli4_hba.pc_sli4_params.supported)
		hw_page_size = wq->page_size;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_wq_create) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_WQ_CREATE,
			 length, LPFC_SLI4_MBX_EMBED);
	wq_create = &mbox->u.mqe.un.wq_create;
	shdr = (union lpfc_sli4_cfg_shdr *) &wq_create->header.cfg_shdr;
	bf_set(lpfc_mbx_wq_create_num_pages, &wq_create->u.request,
		    wq->page_count);
	bf_set(lpfc_mbx_wq_create_cq_id, &wq_create->u.request,
		    cq->queue_id);

	 
	bf_set(lpfc_mbox_hdr_version, &shdr->request,
	       phba->sli4_hba.pc_sli4_params.wqv);

	if ((phba->sli4_hba.pc_sli4_params.wqsize & LPFC_WQ_SZ128_SUPPORT) ||
	    (wq->page_size > SLI4_PAGE_SIZE))
		wq_create_version = LPFC_Q_CREATE_VERSION_1;
	else
		wq_create_version = LPFC_Q_CREATE_VERSION_0;

	switch (wq_create_version) {
	case LPFC_Q_CREATE_VERSION_1:
		bf_set(lpfc_mbx_wq_create_wqe_count, &wq_create->u.request_1,
		       wq->entry_count);
		bf_set(lpfc_mbox_hdr_version, &shdr->request,
		       LPFC_Q_CREATE_VERSION_1);

		switch (wq->entry_size) {
		default:
		case 64:
			bf_set(lpfc_mbx_wq_create_wqe_size,
			       &wq_create->u.request_1,
			       LPFC_WQ_WQE_SIZE_64);
			break;
		case 128:
			bf_set(lpfc_mbx_wq_create_wqe_size,
			       &wq_create->u.request_1,
			       LPFC_WQ_WQE_SIZE_128);
			break;
		}
		 
		bf_set(lpfc_mbx_wq_create_dpp_req, &wq_create->u.request_1, 1);
		bf_set(lpfc_mbx_wq_create_page_size,
		       &wq_create->u.request_1,
		       (wq->page_size / SLI4_PAGE_SIZE));
		page = wq_create->u.request_1.page;
		break;
	default:
		page = wq_create->u.request.page;
		break;
	}

	list_for_each_entry(dmabuf, &wq->page_list, list) {
		memset(dmabuf->virt, 0, hw_page_size);
		page[dmabuf->buffer_tag].addr_lo = putPaddrLow(dmabuf->phys);
		page[dmabuf->buffer_tag].addr_hi = putPaddrHigh(dmabuf->phys);
	}

	if (phba->sli4_hba.fw_func_mode & LPFC_DUA_MODE)
		bf_set(lpfc_mbx_wq_create_dua, &wq_create->u.request, 1);

	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	 
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2503 WQ_CREATE mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
		goto out;
	}

	if (wq_create_version == LPFC_Q_CREATE_VERSION_0)
		wq->queue_id = bf_get(lpfc_mbx_wq_create_q_id,
					&wq_create->u.response);
	else
		wq->queue_id = bf_get(lpfc_mbx_wq_create_v1_q_id,
					&wq_create->u.response_1);

	if (wq->queue_id == 0xFFFF) {
		status = -ENXIO;
		goto out;
	}

	wq->db_format = LPFC_DB_LIST_FORMAT;
	if (wq_create_version == LPFC_Q_CREATE_VERSION_0) {
		if (phba->sli4_hba.fw_func_mode & LPFC_DUA_MODE) {
			wq->db_format = bf_get(lpfc_mbx_wq_create_db_format,
					       &wq_create->u.response);
			if ((wq->db_format != LPFC_DB_LIST_FORMAT) &&
			    (wq->db_format != LPFC_DB_RING_FORMAT)) {
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"3265 WQ[%d] doorbell format "
						"not supported: x%x\n",
						wq->queue_id, wq->db_format);
				status = -EINVAL;
				goto out;
			}
			pci_barset = bf_get(lpfc_mbx_wq_create_bar_set,
					    &wq_create->u.response);
			bar_memmap_p = lpfc_dual_chute_pci_bar_map(phba,
								   pci_barset);
			if (!bar_memmap_p) {
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"3263 WQ[%d] failed to memmap "
						"pci barset:x%x\n",
						wq->queue_id, pci_barset);
				status = -ENOMEM;
				goto out;
			}
			db_offset = wq_create->u.response.doorbell_offset;
			if ((db_offset != LPFC_ULP0_WQ_DOORBELL) &&
			    (db_offset != LPFC_ULP1_WQ_DOORBELL)) {
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"3252 WQ[%d] doorbell offset "
						"not supported: x%x\n",
						wq->queue_id, db_offset);
				status = -EINVAL;
				goto out;
			}
			wq->db_regaddr = bar_memmap_p + db_offset;
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"3264 WQ[%d]: barset:x%x, offset:x%x, "
					"format:x%x\n", wq->queue_id,
					pci_barset, db_offset, wq->db_format);
		} else
			wq->db_regaddr = phba->sli4_hba.WQDBregaddr;
	} else {
		 
		wq->dpp_enable = bf_get(lpfc_mbx_wq_create_dpp_rsp,
				    &wq_create->u.response_1);
		if (wq->dpp_enable) {
			pci_barset = bf_get(lpfc_mbx_wq_create_v1_bar_set,
					    &wq_create->u.response_1);
			bar_memmap_p = lpfc_dual_chute_pci_bar_map(phba,
								   pci_barset);
			if (!bar_memmap_p) {
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"3267 WQ[%d] failed to memmap "
						"pci barset:x%x\n",
						wq->queue_id, pci_barset);
				status = -ENOMEM;
				goto out;
			}
			db_offset = wq_create->u.response_1.doorbell_offset;
			wq->db_regaddr = bar_memmap_p + db_offset;
			wq->dpp_id = bf_get(lpfc_mbx_wq_create_dpp_id,
					    &wq_create->u.response_1);
			dpp_barset = bf_get(lpfc_mbx_wq_create_dpp_bar,
					    &wq_create->u.response_1);
			bar_memmap_p = lpfc_dual_chute_pci_bar_map(phba,
								   dpp_barset);
			if (!bar_memmap_p) {
				lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
						"3268 WQ[%d] failed to memmap "
						"pci barset:x%x\n",
						wq->queue_id, dpp_barset);
				status = -ENOMEM;
				goto out;
			}
			dpp_offset = wq_create->u.response_1.dpp_offset;
			wq->dpp_regaddr = bar_memmap_p + dpp_offset;
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"3271 WQ[%d]: barset:x%x, offset:x%x, "
					"dpp_id:x%x dpp_barset:x%x "
					"dpp_offset:x%x\n",
					wq->queue_id, pci_barset, db_offset,
					wq->dpp_id, dpp_barset, dpp_offset);

#ifdef CONFIG_X86
			 
			pg_addr = (unsigned long)(wq->dpp_regaddr) & PAGE_MASK;
			rc = set_memory_wc(pg_addr, 1);
			if (rc) {
				lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"3272 Cannot setup Combined "
					"Write on WQ[%d] - disable DPP\n",
					wq->queue_id);
				phba->cfg_enable_dpp = 0;
			}
#else
			phba->cfg_enable_dpp = 0;
#endif
		} else
			wq->db_regaddr = phba->sli4_hba.WQDBregaddr;
	}
	wq->pring = kzalloc(sizeof(struct lpfc_sli_ring), GFP_KERNEL);
	if (wq->pring == NULL) {
		status = -ENOMEM;
		goto out;
	}
	wq->type = LPFC_WQ;
	wq->assoc_qid = cq->queue_id;
	wq->subtype = subtype;
	wq->host_index = 0;
	wq->hba_index = 0;
	wq->notify_interval = LPFC_WQ_NOTIFY_INTRVL;

	 
	list_add_tail(&wq->list, &cq->child_list);
out:
	mempool_free(mbox, phba->mbox_mem_pool);
	return status;
}

 
int
lpfc_rq_create(struct lpfc_hba *phba, struct lpfc_queue *hrq,
	       struct lpfc_queue *drq, struct lpfc_queue *cq, uint32_t subtype)
{
	struct lpfc_mbx_rq_create *rq_create;
	struct lpfc_dmabuf *dmabuf;
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	uint32_t hw_page_size = phba->sli4_hba.pc_sli4_params.if_page_sz;
	void __iomem *bar_memmap_p;
	uint32_t db_offset;
	uint16_t pci_barset;

	 
	if (!hrq || !drq || !cq)
		return -ENODEV;
	if (!phba->sli4_hba.pc_sli4_params.supported)
		hw_page_size = SLI4_PAGE_SIZE;

	if (hrq->entry_count != drq->entry_count)
		return -EINVAL;
	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_rq_create) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_RQ_CREATE,
			 length, LPFC_SLI4_MBX_EMBED);
	rq_create = &mbox->u.mqe.un.rq_create;
	shdr = (union lpfc_sli4_cfg_shdr *) &rq_create->header.cfg_shdr;
	bf_set(lpfc_mbox_hdr_version, &shdr->request,
	       phba->sli4_hba.pc_sli4_params.rqv);
	if (phba->sli4_hba.pc_sli4_params.rqv == LPFC_Q_CREATE_VERSION_1) {
		bf_set(lpfc_rq_context_rqe_count_1,
		       &rq_create->u.request.context,
		       hrq->entry_count);
		rq_create->u.request.context.buffer_size = LPFC_HDR_BUF_SIZE;
		bf_set(lpfc_rq_context_rqe_size,
		       &rq_create->u.request.context,
		       LPFC_RQE_SIZE_8);
		bf_set(lpfc_rq_context_page_size,
		       &rq_create->u.request.context,
		       LPFC_RQ_PAGE_SIZE_4096);
	} else {
		switch (hrq->entry_count) {
		default:
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"2535 Unsupported RQ count. (%d)\n",
					hrq->entry_count);
			if (hrq->entry_count < 512) {
				status = -EINVAL;
				goto out;
			}
			fallthrough;	 
		case 512:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_512);
			break;
		case 1024:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_1024);
			break;
		case 2048:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_2048);
			break;
		case 4096:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_4096);
			break;
		}
		bf_set(lpfc_rq_context_buf_size, &rq_create->u.request.context,
		       LPFC_HDR_BUF_SIZE);
	}
	bf_set(lpfc_rq_context_cq_id, &rq_create->u.request.context,
	       cq->queue_id);
	bf_set(lpfc_mbx_rq_create_num_pages, &rq_create->u.request,
	       hrq->page_count);
	list_for_each_entry(dmabuf, &hrq->page_list, list) {
		memset(dmabuf->virt, 0, hw_page_size);
		rq_create->u.request.page[dmabuf->buffer_tag].addr_lo =
					putPaddrLow(dmabuf->phys);
		rq_create->u.request.page[dmabuf->buffer_tag].addr_hi =
					putPaddrHigh(dmabuf->phys);
	}
	if (phba->sli4_hba.fw_func_mode & LPFC_DUA_MODE)
		bf_set(lpfc_mbx_rq_create_dua, &rq_create->u.request, 1);

	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	 
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2504 RQ_CREATE mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
		goto out;
	}
	hrq->queue_id = bf_get(lpfc_mbx_rq_create_q_id, &rq_create->u.response);
	if (hrq->queue_id == 0xFFFF) {
		status = -ENXIO;
		goto out;
	}

	if (phba->sli4_hba.fw_func_mode & LPFC_DUA_MODE) {
		hrq->db_format = bf_get(lpfc_mbx_rq_create_db_format,
					&rq_create->u.response);
		if ((hrq->db_format != LPFC_DB_LIST_FORMAT) &&
		    (hrq->db_format != LPFC_DB_RING_FORMAT)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"3262 RQ [%d] doorbell format not "
					"supported: x%x\n", hrq->queue_id,
					hrq->db_format);
			status = -EINVAL;
			goto out;
		}

		pci_barset = bf_get(lpfc_mbx_rq_create_bar_set,
				    &rq_create->u.response);
		bar_memmap_p = lpfc_dual_chute_pci_bar_map(phba, pci_barset);
		if (!bar_memmap_p) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"3269 RQ[%d] failed to memmap pci "
					"barset:x%x\n", hrq->queue_id,
					pci_barset);
			status = -ENOMEM;
			goto out;
		}

		db_offset = rq_create->u.response.doorbell_offset;
		if ((db_offset != LPFC_ULP0_RQ_DOORBELL) &&
		    (db_offset != LPFC_ULP1_RQ_DOORBELL)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"3270 RQ[%d] doorbell offset not "
					"supported: x%x\n", hrq->queue_id,
					db_offset);
			status = -EINVAL;
			goto out;
		}
		hrq->db_regaddr = bar_memmap_p + db_offset;
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"3266 RQ[qid:%d]: barset:x%x, offset:x%x, "
				"format:x%x\n", hrq->queue_id, pci_barset,
				db_offset, hrq->db_format);
	} else {
		hrq->db_format = LPFC_DB_RING_FORMAT;
		hrq->db_regaddr = phba->sli4_hba.RQDBregaddr;
	}
	hrq->type = LPFC_HRQ;
	hrq->assoc_qid = cq->queue_id;
	hrq->subtype = subtype;
	hrq->host_index = 0;
	hrq->hba_index = 0;
	hrq->notify_interval = LPFC_RQ_NOTIFY_INTRVL;

	 
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_RQ_CREATE,
			 length, LPFC_SLI4_MBX_EMBED);
	bf_set(lpfc_mbox_hdr_version, &shdr->request,
	       phba->sli4_hba.pc_sli4_params.rqv);
	if (phba->sli4_hba.pc_sli4_params.rqv == LPFC_Q_CREATE_VERSION_1) {
		bf_set(lpfc_rq_context_rqe_count_1,
		       &rq_create->u.request.context, hrq->entry_count);
		if (subtype == LPFC_NVMET)
			rq_create->u.request.context.buffer_size =
				LPFC_NVMET_DATA_BUF_SIZE;
		else
			rq_create->u.request.context.buffer_size =
				LPFC_DATA_BUF_SIZE;
		bf_set(lpfc_rq_context_rqe_size, &rq_create->u.request.context,
		       LPFC_RQE_SIZE_8);
		bf_set(lpfc_rq_context_page_size, &rq_create->u.request.context,
		       (PAGE_SIZE/SLI4_PAGE_SIZE));
	} else {
		switch (drq->entry_count) {
		default:
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"2536 Unsupported RQ count. (%d)\n",
					drq->entry_count);
			if (drq->entry_count < 512) {
				status = -EINVAL;
				goto out;
			}
			fallthrough;	 
		case 512:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_512);
			break;
		case 1024:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_1024);
			break;
		case 2048:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_2048);
			break;
		case 4096:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_4096);
			break;
		}
		if (subtype == LPFC_NVMET)
			bf_set(lpfc_rq_context_buf_size,
			       &rq_create->u.request.context,
			       LPFC_NVMET_DATA_BUF_SIZE);
		else
			bf_set(lpfc_rq_context_buf_size,
			       &rq_create->u.request.context,
			       LPFC_DATA_BUF_SIZE);
	}
	bf_set(lpfc_rq_context_cq_id, &rq_create->u.request.context,
	       cq->queue_id);
	bf_set(lpfc_mbx_rq_create_num_pages, &rq_create->u.request,
	       drq->page_count);
	list_for_each_entry(dmabuf, &drq->page_list, list) {
		rq_create->u.request.page[dmabuf->buffer_tag].addr_lo =
					putPaddrLow(dmabuf->phys);
		rq_create->u.request.page[dmabuf->buffer_tag].addr_hi =
					putPaddrHigh(dmabuf->phys);
	}
	if (phba->sli4_hba.fw_func_mode & LPFC_DUA_MODE)
		bf_set(lpfc_mbx_rq_create_dua, &rq_create->u.request, 1);
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	 
	shdr = (union lpfc_sli4_cfg_shdr *) &rq_create->header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		status = -ENXIO;
		goto out;
	}
	drq->queue_id = bf_get(lpfc_mbx_rq_create_q_id, &rq_create->u.response);
	if (drq->queue_id == 0xFFFF) {
		status = -ENXIO;
		goto out;
	}
	drq->type = LPFC_DRQ;
	drq->assoc_qid = cq->queue_id;
	drq->subtype = subtype;
	drq->host_index = 0;
	drq->hba_index = 0;
	drq->notify_interval = LPFC_RQ_NOTIFY_INTRVL;

	 
	list_add_tail(&hrq->list, &cq->child_list);
	list_add_tail(&drq->list, &cq->child_list);

out:
	mempool_free(mbox, phba->mbox_mem_pool);
	return status;
}

 
int
lpfc_mrq_create(struct lpfc_hba *phba, struct lpfc_queue **hrqp,
		struct lpfc_queue **drqp, struct lpfc_queue **cqp,
		uint32_t subtype)
{
	struct lpfc_queue *hrq, *drq, *cq;
	struct lpfc_mbx_rq_create_v2 *rq_create;
	struct lpfc_dmabuf *dmabuf;
	LPFC_MBOXQ_t *mbox;
	int rc, length, alloclen, status = 0;
	int cnt, idx, numrq, page_idx = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	uint32_t hw_page_size = phba->sli4_hba.pc_sli4_params.if_page_sz;

	numrq = phba->cfg_nvmet_mrq;
	 
	if (!hrqp || !drqp || !cqp || !numrq)
		return -ENODEV;
	if (!phba->sli4_hba.pc_sli4_params.supported)
		hw_page_size = SLI4_PAGE_SIZE;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	length = sizeof(struct lpfc_mbx_rq_create_v2);
	length += ((2 * numrq * hrqp[0]->page_count) *
		   sizeof(struct dma_address));

	alloclen = lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
				    LPFC_MBOX_OPCODE_FCOE_RQ_CREATE, length,
				    LPFC_SLI4_MBX_NEMBED);
	if (alloclen < length) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3099 Allocated DMA memory size (%d) is "
				"less than the requested DMA memory size "
				"(%d)\n", alloclen, length);
		status = -ENOMEM;
		goto out;
	}



	rq_create = mbox->sge_array->addr[0];
	shdr = (union lpfc_sli4_cfg_shdr *)&rq_create->cfg_shdr;

	bf_set(lpfc_mbox_hdr_version, &shdr->request, LPFC_Q_CREATE_VERSION_2);
	cnt = 0;

	for (idx = 0; idx < numrq; idx++) {
		hrq = hrqp[idx];
		drq = drqp[idx];
		cq  = cqp[idx];

		 
		if (!hrq || !drq || !cq) {
			status = -ENODEV;
			goto out;
		}

		if (hrq->entry_count != drq->entry_count) {
			status = -EINVAL;
			goto out;
		}

		if (idx == 0) {
			bf_set(lpfc_mbx_rq_create_num_pages,
			       &rq_create->u.request,
			       hrq->page_count);
			bf_set(lpfc_mbx_rq_create_rq_cnt,
			       &rq_create->u.request, (numrq * 2));
			bf_set(lpfc_mbx_rq_create_dnb, &rq_create->u.request,
			       1);
			bf_set(lpfc_rq_context_base_cq,
			       &rq_create->u.request.context,
			       cq->queue_id);
			bf_set(lpfc_rq_context_data_size,
			       &rq_create->u.request.context,
			       LPFC_NVMET_DATA_BUF_SIZE);
			bf_set(lpfc_rq_context_hdr_size,
			       &rq_create->u.request.context,
			       LPFC_HDR_BUF_SIZE);
			bf_set(lpfc_rq_context_rqe_count_1,
			       &rq_create->u.request.context,
			       hrq->entry_count);
			bf_set(lpfc_rq_context_rqe_size,
			       &rq_create->u.request.context,
			       LPFC_RQE_SIZE_8);
			bf_set(lpfc_rq_context_page_size,
			       &rq_create->u.request.context,
			       (PAGE_SIZE/SLI4_PAGE_SIZE));
		}
		rc = 0;
		list_for_each_entry(dmabuf, &hrq->page_list, list) {
			memset(dmabuf->virt, 0, hw_page_size);
			cnt = page_idx + dmabuf->buffer_tag;
			rq_create->u.request.page[cnt].addr_lo =
					putPaddrLow(dmabuf->phys);
			rq_create->u.request.page[cnt].addr_hi =
					putPaddrHigh(dmabuf->phys);
			rc++;
		}
		page_idx += rc;

		rc = 0;
		list_for_each_entry(dmabuf, &drq->page_list, list) {
			memset(dmabuf->virt, 0, hw_page_size);
			cnt = page_idx + dmabuf->buffer_tag;
			rq_create->u.request.page[cnt].addr_lo =
					putPaddrLow(dmabuf->phys);
			rq_create->u.request.page[cnt].addr_hi =
					putPaddrHigh(dmabuf->phys);
			rc++;
		}
		page_idx += rc;

		hrq->db_format = LPFC_DB_RING_FORMAT;
		hrq->db_regaddr = phba->sli4_hba.RQDBregaddr;
		hrq->type = LPFC_HRQ;
		hrq->assoc_qid = cq->queue_id;
		hrq->subtype = subtype;
		hrq->host_index = 0;
		hrq->hba_index = 0;
		hrq->notify_interval = LPFC_RQ_NOTIFY_INTRVL;

		drq->db_format = LPFC_DB_RING_FORMAT;
		drq->db_regaddr = phba->sli4_hba.RQDBregaddr;
		drq->type = LPFC_DRQ;
		drq->assoc_qid = cq->queue_id;
		drq->subtype = subtype;
		drq->host_index = 0;
		drq->hba_index = 0;
		drq->notify_interval = LPFC_RQ_NOTIFY_INTRVL;

		list_add_tail(&hrq->list, &cq->child_list);
		list_add_tail(&drq->list, &cq->child_list);
	}

	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	 
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3120 RQ_CREATE mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
		goto out;
	}
	rc = bf_get(lpfc_mbx_rq_create_q_id, &rq_create->u.response);
	if (rc == 0xFFFF) {
		status = -ENXIO;
		goto out;
	}

	 
	for (idx = 0; idx < numrq; idx++) {
		hrq = hrqp[idx];
		hrq->queue_id = rc + (2 * idx);
		drq = drqp[idx];
		drq->queue_id = rc + (2 * idx) + 1;
	}

out:
	lpfc_sli4_mbox_cmd_free(phba, mbox);
	return status;
}

 
int
lpfc_eq_destroy(struct lpfc_hba *phba, struct lpfc_queue *eq)
{
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	 
	if (!eq)
		return -ENODEV;

	mbox = mempool_alloc(eq->phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_eq_destroy) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_EQ_DESTROY,
			 length, LPFC_SLI4_MBX_EMBED);
	bf_set(lpfc_mbx_eq_destroy_q_id, &mbox->u.mqe.un.eq_destroy.u.request,
	       eq->queue_id);
	mbox->vport = eq->phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;

	rc = lpfc_sli_issue_mbox(eq->phba, mbox, MBX_POLL);
	 
	shdr = (union lpfc_sli4_cfg_shdr *)
		&mbox->u.mqe.un.eq_destroy.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2505 EQ_DESTROY mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
	}

	 
	list_del_init(&eq->list);
	mempool_free(mbox, eq->phba->mbox_mem_pool);
	return status;
}

 
int
lpfc_cq_destroy(struct lpfc_hba *phba, struct lpfc_queue *cq)
{
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	 
	if (!cq)
		return -ENODEV;
	mbox = mempool_alloc(cq->phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_cq_destroy) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_CQ_DESTROY,
			 length, LPFC_SLI4_MBX_EMBED);
	bf_set(lpfc_mbx_cq_destroy_q_id, &mbox->u.mqe.un.cq_destroy.u.request,
	       cq->queue_id);
	mbox->vport = cq->phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	rc = lpfc_sli_issue_mbox(cq->phba, mbox, MBX_POLL);
	 
	shdr = (union lpfc_sli4_cfg_shdr *)
		&mbox->u.mqe.un.wq_create.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2506 CQ_DESTROY mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
	}
	 
	list_del_init(&cq->list);
	mempool_free(mbox, cq->phba->mbox_mem_pool);
	return status;
}

 
int
lpfc_mq_destroy(struct lpfc_hba *phba, struct lpfc_queue *mq)
{
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	 
	if (!mq)
		return -ENODEV;
	mbox = mempool_alloc(mq->phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_mq_destroy) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_MQ_DESTROY,
			 length, LPFC_SLI4_MBX_EMBED);
	bf_set(lpfc_mbx_mq_destroy_q_id, &mbox->u.mqe.un.mq_destroy.u.request,
	       mq->queue_id);
	mbox->vport = mq->phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	rc = lpfc_sli_issue_mbox(mq->phba, mbox, MBX_POLL);
	 
	shdr = (union lpfc_sli4_cfg_shdr *)
		&mbox->u.mqe.un.mq_destroy.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2507 MQ_DESTROY mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
	}
	 
	list_del_init(&mq->list);
	mempool_free(mbox, mq->phba->mbox_mem_pool);
	return status;
}

 
int
lpfc_wq_destroy(struct lpfc_hba *phba, struct lpfc_queue *wq)
{
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	 
	if (!wq)
		return -ENODEV;
	mbox = mempool_alloc(wq->phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_wq_destroy) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_WQ_DESTROY,
			 length, LPFC_SLI4_MBX_EMBED);
	bf_set(lpfc_mbx_wq_destroy_q_id, &mbox->u.mqe.un.wq_destroy.u.request,
	       wq->queue_id);
	mbox->vport = wq->phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	rc = lpfc_sli_issue_mbox(wq->phba, mbox, MBX_POLL);
	shdr = (union lpfc_sli4_cfg_shdr *)
		&mbox->u.mqe.un.wq_destroy.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2508 WQ_DESTROY mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
	}
	 
	list_del_init(&wq->list);
	kfree(wq->pring);
	wq->pring = NULL;
	mempool_free(mbox, wq->phba->mbox_mem_pool);
	return status;
}

 
int
lpfc_rq_destroy(struct lpfc_hba *phba, struct lpfc_queue *hrq,
		struct lpfc_queue *drq)
{
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	 
	if (!hrq || !drq)
		return -ENODEV;
	mbox = mempool_alloc(hrq->phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_rq_destroy) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_RQ_DESTROY,
			 length, LPFC_SLI4_MBX_EMBED);
	bf_set(lpfc_mbx_rq_destroy_q_id, &mbox->u.mqe.un.rq_destroy.u.request,
	       hrq->queue_id);
	mbox->vport = hrq->phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	rc = lpfc_sli_issue_mbox(hrq->phba, mbox, MBX_POLL);
	 
	shdr = (union lpfc_sli4_cfg_shdr *)
		&mbox->u.mqe.un.rq_destroy.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2509 RQ_DESTROY mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		mempool_free(mbox, hrq->phba->mbox_mem_pool);
		return -ENXIO;
	}
	bf_set(lpfc_mbx_rq_destroy_q_id, &mbox->u.mqe.un.rq_destroy.u.request,
	       drq->queue_id);
	rc = lpfc_sli_issue_mbox(drq->phba, mbox, MBX_POLL);
	shdr = (union lpfc_sli4_cfg_shdr *)
		&mbox->u.mqe.un.rq_destroy.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2510 RQ_DESTROY mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
	}
	list_del_init(&hrq->list);
	list_del_init(&drq->list);
	mempool_free(mbox, hrq->phba->mbox_mem_pool);
	return status;
}

 
int
lpfc_sli4_post_sgl(struct lpfc_hba *phba,
		dma_addr_t pdma_phys_addr0,
		dma_addr_t pdma_phys_addr1,
		uint16_t xritag)
{
	struct lpfc_mbx_post_sgl_pages *post_sgl_pages;
	LPFC_MBOXQ_t *mbox;
	int rc;
	uint32_t shdr_status, shdr_add_status;
	uint32_t mbox_tmo;
	union lpfc_sli4_cfg_shdr *shdr;

	if (xritag == NO_XRI) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0364 Invalid param:\n");
		return -EINVAL;
	}

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			LPFC_MBOX_OPCODE_FCOE_POST_SGL_PAGES,
			sizeof(struct lpfc_mbx_post_sgl_pages) -
			sizeof(struct lpfc_sli4_cfg_mhdr), LPFC_SLI4_MBX_EMBED);

	post_sgl_pages = (struct lpfc_mbx_post_sgl_pages *)
				&mbox->u.mqe.un.post_sgl_pages;
	bf_set(lpfc_post_sgl_pages_xri, post_sgl_pages, xritag);
	bf_set(lpfc_post_sgl_pages_xricnt, post_sgl_pages, 1);

	post_sgl_pages->sgl_pg_pairs[0].sgl_pg0_addr_lo	=
				cpu_to_le32(putPaddrLow(pdma_phys_addr0));
	post_sgl_pages->sgl_pg_pairs[0].sgl_pg0_addr_hi =
				cpu_to_le32(putPaddrHigh(pdma_phys_addr0));

	post_sgl_pages->sgl_pg_pairs[0].sgl_pg1_addr_lo	=
				cpu_to_le32(putPaddrLow(pdma_phys_addr1));
	post_sgl_pages->sgl_pg_pairs[0].sgl_pg1_addr_hi =
				cpu_to_le32(putPaddrHigh(pdma_phys_addr1));
	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}
	 
	shdr = (union lpfc_sli4_cfg_shdr *) &post_sgl_pages->header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (!phba->sli4_hba.intr_enable)
		mempool_free(mbox, phba->mbox_mem_pool);
	else if (rc != MBX_TIMEOUT)
		mempool_free(mbox, phba->mbox_mem_pool);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2511 POST_SGL mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
	}
	return 0;
}

 
static uint16_t
lpfc_sli4_alloc_xri(struct lpfc_hba *phba)
{
	unsigned long xri;

	 
	spin_lock_irq(&phba->hbalock);
	xri = find_first_zero_bit(phba->sli4_hba.xri_bmask,
				 phba->sli4_hba.max_cfg_param.max_xri);
	if (xri >= phba->sli4_hba.max_cfg_param.max_xri) {
		spin_unlock_irq(&phba->hbalock);
		return NO_XRI;
	} else {
		set_bit(xri, phba->sli4_hba.xri_bmask);
		phba->sli4_hba.max_cfg_param.xri_used++;
	}
	spin_unlock_irq(&phba->hbalock);
	return xri;
}

 
static void
__lpfc_sli4_free_xri(struct lpfc_hba *phba, int xri)
{
	if (test_and_clear_bit(xri, phba->sli4_hba.xri_bmask)) {
		phba->sli4_hba.max_cfg_param.xri_used--;
	}
}

 
void
lpfc_sli4_free_xri(struct lpfc_hba *phba, int xri)
{
	spin_lock_irq(&phba->hbalock);
	__lpfc_sli4_free_xri(phba, xri);
	spin_unlock_irq(&phba->hbalock);
}

 
uint16_t
lpfc_sli4_next_xritag(struct lpfc_hba *phba)
{
	uint16_t xri_index;

	xri_index = lpfc_sli4_alloc_xri(phba);
	if (xri_index == NO_XRI)
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"2004 Failed to allocate XRI.last XRITAG is %d"
				" Max XRI is %d, Used XRI is %d\n",
				xri_index,
				phba->sli4_hba.max_cfg_param.max_xri,
				phba->sli4_hba.max_cfg_param.xri_used);
	return xri_index;
}

 
static int
lpfc_sli4_post_sgl_list(struct lpfc_hba *phba,
			    struct list_head *post_sgl_list,
			    int post_cnt)
{
	struct lpfc_sglq *sglq_entry = NULL, *sglq_next = NULL;
	struct lpfc_mbx_post_uembed_sgl_page1 *sgl;
	struct sgl_page_pairs *sgl_pg_pairs;
	void *viraddr;
	LPFC_MBOXQ_t *mbox;
	uint32_t reqlen, alloclen, pg_pairs;
	uint32_t mbox_tmo;
	uint16_t xritag_start = 0;
	int rc = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	reqlen = post_cnt * sizeof(struct sgl_page_pairs) +
		 sizeof(union lpfc_sli4_cfg_shdr) + sizeof(uint32_t);
	if (reqlen > SLI4_PAGE_SIZE) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2559 Block sgl registration required DMA "
				"size (%d) great than a page\n", reqlen);
		return -ENOMEM;
	}

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	 
	alloclen = lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_POST_SGL_PAGES, reqlen,
			 LPFC_SLI4_MBX_NEMBED);

	if (alloclen < reqlen) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"0285 Allocated DMA memory size (%d) is "
				"less than the requested DMA memory "
				"size (%d)\n", alloclen, reqlen);
		lpfc_sli4_mbox_cmd_free(phba, mbox);
		return -ENOMEM;
	}
	 
	viraddr = mbox->sge_array->addr[0];
	sgl = (struct lpfc_mbx_post_uembed_sgl_page1 *)viraddr;
	sgl_pg_pairs = &sgl->sgl_pg_pairs;

	pg_pairs = 0;
	list_for_each_entry_safe(sglq_entry, sglq_next, post_sgl_list, list) {
		 
		sgl_pg_pairs->sgl_pg0_addr_lo =
				cpu_to_le32(putPaddrLow(sglq_entry->phys));
		sgl_pg_pairs->sgl_pg0_addr_hi =
				cpu_to_le32(putPaddrHigh(sglq_entry->phys));
		sgl_pg_pairs->sgl_pg1_addr_lo =
				cpu_to_le32(putPaddrLow(0));
		sgl_pg_pairs->sgl_pg1_addr_hi =
				cpu_to_le32(putPaddrHigh(0));

		 
		if (pg_pairs == 0)
			xritag_start = sglq_entry->sli4_xritag;
		sgl_pg_pairs++;
		pg_pairs++;
	}

	 
	bf_set(lpfc_post_sgl_pages_xri, sgl, xritag_start);
	bf_set(lpfc_post_sgl_pages_xricnt, sgl, post_cnt);
	sgl->word0 = cpu_to_le32(sgl->word0);

	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}
	shdr = (union lpfc_sli4_cfg_shdr *) &sgl->cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (!phba->sli4_hba.intr_enable)
		lpfc_sli4_mbox_cmd_free(phba, mbox);
	else if (rc != MBX_TIMEOUT)
		lpfc_sli4_mbox_cmd_free(phba, mbox);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2513 POST_SGL_BLOCK mailbox command failed "
				"status x%x add_status x%x mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		rc = -ENXIO;
	}
	return rc;
}

 
static int
lpfc_sli4_post_io_sgl_block(struct lpfc_hba *phba, struct list_head *nblist,
			    int count)
{
	struct lpfc_io_buf *lpfc_ncmd;
	struct lpfc_mbx_post_uembed_sgl_page1 *sgl;
	struct sgl_page_pairs *sgl_pg_pairs;
	void *viraddr;
	LPFC_MBOXQ_t *mbox;
	uint32_t reqlen, alloclen, pg_pairs;
	uint32_t mbox_tmo;
	uint16_t xritag_start = 0;
	int rc = 0;
	uint32_t shdr_status, shdr_add_status;
	dma_addr_t pdma_phys_bpl1;
	union lpfc_sli4_cfg_shdr *shdr;

	 
	reqlen = count * sizeof(struct sgl_page_pairs) +
		 sizeof(union lpfc_sli4_cfg_shdr) + sizeof(uint32_t);
	if (reqlen > SLI4_PAGE_SIZE) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"6118 Block sgl registration required DMA "
				"size (%d) great than a page\n", reqlen);
		return -ENOMEM;
	}
	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"6119 Failed to allocate mbox cmd memory\n");
		return -ENOMEM;
	}

	 
	alloclen = lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
				    LPFC_MBOX_OPCODE_FCOE_POST_SGL_PAGES,
				    reqlen, LPFC_SLI4_MBX_NEMBED);

	if (alloclen < reqlen) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"6120 Allocated DMA memory size (%d) is "
				"less than the requested DMA memory "
				"size (%d)\n", alloclen, reqlen);
		lpfc_sli4_mbox_cmd_free(phba, mbox);
		return -ENOMEM;
	}

	 
	viraddr = mbox->sge_array->addr[0];

	 
	sgl = (struct lpfc_mbx_post_uembed_sgl_page1 *)viraddr;
	sgl_pg_pairs = &sgl->sgl_pg_pairs;

	pg_pairs = 0;
	list_for_each_entry(lpfc_ncmd, nblist, list) {
		 
		sgl_pg_pairs->sgl_pg0_addr_lo =
			cpu_to_le32(putPaddrLow(lpfc_ncmd->dma_phys_sgl));
		sgl_pg_pairs->sgl_pg0_addr_hi =
			cpu_to_le32(putPaddrHigh(lpfc_ncmd->dma_phys_sgl));
		if (phba->cfg_sg_dma_buf_size > SGL_PAGE_SIZE)
			pdma_phys_bpl1 = lpfc_ncmd->dma_phys_sgl +
						SGL_PAGE_SIZE;
		else
			pdma_phys_bpl1 = 0;
		sgl_pg_pairs->sgl_pg1_addr_lo =
			cpu_to_le32(putPaddrLow(pdma_phys_bpl1));
		sgl_pg_pairs->sgl_pg1_addr_hi =
			cpu_to_le32(putPaddrHigh(pdma_phys_bpl1));
		 
		if (pg_pairs == 0)
			xritag_start = lpfc_ncmd->cur_iocbq.sli4_xritag;
		sgl_pg_pairs++;
		pg_pairs++;
	}
	bf_set(lpfc_post_sgl_pages_xri, sgl, xritag_start);
	bf_set(lpfc_post_sgl_pages_xricnt, sgl, pg_pairs);
	 
	sgl->word0 = cpu_to_le32(sgl->word0);

	if (!phba->sli4_hba.intr_enable) {
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	} else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}
	shdr = (union lpfc_sli4_cfg_shdr *)&sgl->cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (!phba->sli4_hba.intr_enable)
		lpfc_sli4_mbox_cmd_free(phba, mbox);
	else if (rc != MBX_TIMEOUT)
		lpfc_sli4_mbox_cmd_free(phba, mbox);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"6125 POST_SGL_BLOCK mailbox command failed "
				"status x%x add_status x%x mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		rc = -ENXIO;
	}
	return rc;
}

 
int
lpfc_sli4_post_io_sgl_list(struct lpfc_hba *phba,
			   struct list_head *post_nblist, int sb_count)
{
	struct lpfc_io_buf *lpfc_ncmd, *lpfc_ncmd_next;
	int status, sgl_size;
	int post_cnt = 0, block_cnt = 0, num_posting = 0, num_posted = 0;
	dma_addr_t pdma_phys_sgl1;
	int last_xritag = NO_XRI;
	int cur_xritag;
	LIST_HEAD(prep_nblist);
	LIST_HEAD(blck_nblist);
	LIST_HEAD(nvme_nblist);

	 
	if (sb_count <= 0)
		return -EINVAL;

	sgl_size = phba->cfg_sg_dma_buf_size;
	list_for_each_entry_safe(lpfc_ncmd, lpfc_ncmd_next, post_nblist, list) {
		list_del_init(&lpfc_ncmd->list);
		block_cnt++;
		if ((last_xritag != NO_XRI) &&
		    (lpfc_ncmd->cur_iocbq.sli4_xritag != last_xritag + 1)) {
			 
			list_splice_init(&prep_nblist, &blck_nblist);
			post_cnt = block_cnt - 1;
			 
			list_add_tail(&lpfc_ncmd->list, &prep_nblist);
			block_cnt = 1;
		} else {
			 
			list_add_tail(&lpfc_ncmd->list, &prep_nblist);
			 
			if (block_cnt == LPFC_NEMBED_MBOX_SGL_CNT) {
				list_splice_init(&prep_nblist, &blck_nblist);
				post_cnt = block_cnt;
				block_cnt = 0;
			}
		}
		num_posting++;
		last_xritag = lpfc_ncmd->cur_iocbq.sli4_xritag;

		 
		if (num_posting == sb_count) {
			if (post_cnt == 0) {
				 
				list_splice_init(&prep_nblist, &blck_nblist);
				post_cnt = block_cnt;
			} else if (block_cnt == 1) {
				 
				if (sgl_size > SGL_PAGE_SIZE)
					pdma_phys_sgl1 =
						lpfc_ncmd->dma_phys_sgl +
						SGL_PAGE_SIZE;
				else
					pdma_phys_sgl1 = 0;
				cur_xritag = lpfc_ncmd->cur_iocbq.sli4_xritag;
				status = lpfc_sli4_post_sgl(
						phba, lpfc_ncmd->dma_phys_sgl,
						pdma_phys_sgl1, cur_xritag);
				if (status) {
					 
					lpfc_ncmd->flags |=
						LPFC_SBUF_NOT_POSTED;
				} else {
					 
					lpfc_ncmd->flags &=
						~LPFC_SBUF_NOT_POSTED;
					lpfc_ncmd->status = IOSTAT_SUCCESS;
					num_posted++;
				}
				 
				list_add_tail(&lpfc_ncmd->list, &nvme_nblist);
			}
		}

		 
		if (post_cnt == 0)
			continue;

		 
		status = lpfc_sli4_post_io_sgl_block(phba, &blck_nblist,
						     post_cnt);

		 
		if (block_cnt == 0)
			last_xritag = NO_XRI;

		 
		post_cnt = 0;

		 
		while (!list_empty(&blck_nblist)) {
			list_remove_head(&blck_nblist, lpfc_ncmd,
					 struct lpfc_io_buf, list);
			if (status) {
				 
				lpfc_ncmd->flags |= LPFC_SBUF_NOT_POSTED;
			} else {
				 
				lpfc_ncmd->flags &= ~LPFC_SBUF_NOT_POSTED;
				lpfc_ncmd->status = IOSTAT_SUCCESS;
				num_posted++;
			}
			list_add_tail(&lpfc_ncmd->list, &nvme_nblist);
		}
	}
	 
	lpfc_io_buf_replenish(phba, &nvme_nblist);

	return num_posted;
}

 
static int
lpfc_fc_frame_check(struct lpfc_hba *phba, struct fc_frame_header *fc_hdr)
{
	 
	struct fc_vft_header *fc_vft_hdr;
	uint32_t *header = (uint32_t *) fc_hdr;

#define FC_RCTL_MDS_DIAGS	0xF4

	switch (fc_hdr->fh_r_ctl) {
	case FC_RCTL_DD_UNCAT:		 
	case FC_RCTL_DD_SOL_DATA:	 
	case FC_RCTL_DD_UNSOL_CTL:	 
	case FC_RCTL_DD_SOL_CTL:	 
	case FC_RCTL_DD_UNSOL_DATA:	 
	case FC_RCTL_DD_DATA_DESC:	 
	case FC_RCTL_DD_UNSOL_CMD:	 
	case FC_RCTL_DD_CMD_STATUS:	 
	case FC_RCTL_ELS_REQ:	 
	case FC_RCTL_ELS_REP:	 
	case FC_RCTL_ELS4_REQ:	 
	case FC_RCTL_ELS4_REP:	 
	case FC_RCTL_BA_ABTS: 	 
	case FC_RCTL_BA_RMC: 	 
	case FC_RCTL_BA_ACC:	 
	case FC_RCTL_BA_RJT:	 
	case FC_RCTL_BA_PRMT:
	case FC_RCTL_ACK_1:	 
	case FC_RCTL_ACK_0:	 
	case FC_RCTL_P_RJT:	 
	case FC_RCTL_F_RJT:	 
	case FC_RCTL_P_BSY:	 
	case FC_RCTL_F_BSY:	 
	case FC_RCTL_F_BSYL:	 
	case FC_RCTL_LCR:	 
	case FC_RCTL_MDS_DIAGS:  
	case FC_RCTL_END:	 
		break;
	case FC_RCTL_VFTH:	 
		fc_vft_hdr = (struct fc_vft_header *)fc_hdr;
		fc_hdr = &((struct fc_frame_header *)fc_vft_hdr)[1];
		return lpfc_fc_frame_check(phba, fc_hdr);
	case FC_RCTL_BA_NOP:	 
	default:
		goto drop;
	}

	switch (fc_hdr->fh_type) {
	case FC_TYPE_BLS:
	case FC_TYPE_ELS:
	case FC_TYPE_FCP:
	case FC_TYPE_CT:
	case FC_TYPE_NVME:
		break;
	case FC_TYPE_IP:
	case FC_TYPE_ILS:
	default:
		goto drop;
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_ELS,
			"2538 Received frame rctl:x%x, type:x%x, "
			"frame Data:%08x %08x %08x %08x %08x %08x %08x\n",
			fc_hdr->fh_r_ctl, fc_hdr->fh_type,
			be32_to_cpu(header[0]), be32_to_cpu(header[1]),
			be32_to_cpu(header[2]), be32_to_cpu(header[3]),
			be32_to_cpu(header[4]), be32_to_cpu(header[5]),
			be32_to_cpu(header[6]));
	return 0;
drop:
	lpfc_printf_log(phba, KERN_WARNING, LOG_ELS,
			"2539 Dropped frame rctl:x%x type:x%x\n",
			fc_hdr->fh_r_ctl, fc_hdr->fh_type);
	return 1;
}

 
static uint32_t
lpfc_fc_hdr_get_vfi(struct fc_frame_header *fc_hdr)
{
	struct fc_vft_header *fc_vft_hdr = (struct fc_vft_header *)fc_hdr;

	if (fc_hdr->fh_r_ctl != FC_RCTL_VFTH)
		return 0;
	return bf_get(fc_vft_hdr_vf_id, fc_vft_hdr);
}

 
static struct lpfc_vport *
lpfc_fc_frame_to_vport(struct lpfc_hba *phba, struct fc_frame_header *fc_hdr,
		       uint16_t fcfi, uint32_t did)
{
	struct lpfc_vport **vports;
	struct lpfc_vport *vport = NULL;
	int i;

	if (did == Fabric_DID)
		return phba->pport;
	if ((phba->pport->fc_flag & FC_PT2PT) &&
		!(phba->link_state == LPFC_HBA_READY))
		return phba->pport;

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL) {
		for (i = 0; i <= phba->max_vpi && vports[i] != NULL; i++) {
			if (phba->fcf.fcfi == fcfi &&
			    vports[i]->vfi == lpfc_fc_hdr_get_vfi(fc_hdr) &&
			    vports[i]->fc_myDID == did) {
				vport = vports[i];
				break;
			}
		}
	}
	lpfc_destroy_vport_work_array(phba, vports);
	return vport;
}

 
static void
lpfc_update_rcv_time_stamp(struct lpfc_vport *vport)
{
	struct lpfc_dmabuf *h_buf;
	struct hbq_dmabuf *dmabuf = NULL;

	 
	h_buf = list_get_first(&vport->rcv_buffer_list,
			       struct lpfc_dmabuf, list);
	if (!h_buf)
		return;
	dmabuf = container_of(h_buf, struct hbq_dmabuf, hbuf);
	vport->rcv_buffer_time_stamp = dmabuf->time_stamp;
}

 
void
lpfc_cleanup_rcv_buffers(struct lpfc_vport *vport)
{
	struct lpfc_dmabuf *h_buf, *hnext;
	struct lpfc_dmabuf *d_buf, *dnext;
	struct hbq_dmabuf *dmabuf = NULL;

	 
	list_for_each_entry_safe(h_buf, hnext, &vport->rcv_buffer_list, list) {
		dmabuf = container_of(h_buf, struct hbq_dmabuf, hbuf);
		list_del_init(&dmabuf->hbuf.list);
		list_for_each_entry_safe(d_buf, dnext,
					 &dmabuf->dbuf.list, list) {
			list_del_init(&d_buf->list);
			lpfc_in_buf_free(vport->phba, d_buf);
		}
		lpfc_in_buf_free(vport->phba, &dmabuf->dbuf);
	}
}

 
void
lpfc_rcv_seq_check_edtov(struct lpfc_vport *vport)
{
	struct lpfc_dmabuf *h_buf, *hnext;
	struct lpfc_dmabuf *d_buf, *dnext;
	struct hbq_dmabuf *dmabuf = NULL;
	unsigned long timeout;
	int abort_count = 0;

	timeout = (msecs_to_jiffies(vport->phba->fc_edtov) +
		   vport->rcv_buffer_time_stamp);
	if (list_empty(&vport->rcv_buffer_list) ||
	    time_before(jiffies, timeout))
		return;
	 
	list_for_each_entry_safe(h_buf, hnext, &vport->rcv_buffer_list, list) {
		dmabuf = container_of(h_buf, struct hbq_dmabuf, hbuf);
		timeout = (msecs_to_jiffies(vport->phba->fc_edtov) +
			   dmabuf->time_stamp);
		if (time_before(jiffies, timeout))
			break;
		abort_count++;
		list_del_init(&dmabuf->hbuf.list);
		list_for_each_entry_safe(d_buf, dnext,
					 &dmabuf->dbuf.list, list) {
			list_del_init(&d_buf->list);
			lpfc_in_buf_free(vport->phba, d_buf);
		}
		lpfc_in_buf_free(vport->phba, &dmabuf->dbuf);
	}
	if (abort_count)
		lpfc_update_rcv_time_stamp(vport);
}

 
static struct hbq_dmabuf *
lpfc_fc_frame_add(struct lpfc_vport *vport, struct hbq_dmabuf *dmabuf)
{
	struct fc_frame_header *new_hdr;
	struct fc_frame_header *temp_hdr;
	struct lpfc_dmabuf *d_buf;
	struct lpfc_dmabuf *h_buf;
	struct hbq_dmabuf *seq_dmabuf = NULL;
	struct hbq_dmabuf *temp_dmabuf = NULL;
	uint8_t	found = 0;

	INIT_LIST_HEAD(&dmabuf->dbuf.list);
	dmabuf->time_stamp = jiffies;
	new_hdr = (struct fc_frame_header *)dmabuf->hbuf.virt;

	 
	list_for_each_entry(h_buf, &vport->rcv_buffer_list, list) {
		temp_hdr = (struct fc_frame_header *)h_buf->virt;
		if ((temp_hdr->fh_seq_id != new_hdr->fh_seq_id) ||
		    (temp_hdr->fh_ox_id != new_hdr->fh_ox_id) ||
		    (memcmp(&temp_hdr->fh_s_id, &new_hdr->fh_s_id, 3)))
			continue;
		 
		seq_dmabuf = container_of(h_buf, struct hbq_dmabuf, hbuf);
		break;
	}
	if (!seq_dmabuf) {
		 
		list_add_tail(&dmabuf->hbuf.list, &vport->rcv_buffer_list);
		lpfc_update_rcv_time_stamp(vport);
		return dmabuf;
	}
	temp_hdr = seq_dmabuf->hbuf.virt;
	if (be16_to_cpu(new_hdr->fh_seq_cnt) <
		be16_to_cpu(temp_hdr->fh_seq_cnt)) {
		list_del_init(&seq_dmabuf->hbuf.list);
		list_add_tail(&dmabuf->hbuf.list, &vport->rcv_buffer_list);
		list_add_tail(&dmabuf->dbuf.list, &seq_dmabuf->dbuf.list);
		lpfc_update_rcv_time_stamp(vport);
		return dmabuf;
	}
	 
	list_move_tail(&seq_dmabuf->hbuf.list, &vport->rcv_buffer_list);
	seq_dmabuf->time_stamp = jiffies;
	lpfc_update_rcv_time_stamp(vport);
	if (list_empty(&seq_dmabuf->dbuf.list)) {
		list_add_tail(&dmabuf->dbuf.list, &seq_dmabuf->dbuf.list);
		return seq_dmabuf;
	}
	 
	d_buf = list_entry(seq_dmabuf->dbuf.list.prev, typeof(*d_buf), list);
	while (!found) {
		temp_dmabuf = container_of(d_buf, struct hbq_dmabuf, dbuf);
		temp_hdr = (struct fc_frame_header *)temp_dmabuf->hbuf.virt;
		 
		if (be16_to_cpu(new_hdr->fh_seq_cnt) >
			be16_to_cpu(temp_hdr->fh_seq_cnt)) {
			list_add(&dmabuf->dbuf.list, &temp_dmabuf->dbuf.list);
			found = 1;
			break;
		}

		if (&d_buf->list == &seq_dmabuf->dbuf.list)
			break;
		d_buf = list_entry(d_buf->list.prev, typeof(*d_buf), list);
	}

	if (found)
		return seq_dmabuf;
	return NULL;
}

 
static bool
lpfc_sli4_abort_partial_seq(struct lpfc_vport *vport,
			    struct hbq_dmabuf *dmabuf)
{
	struct fc_frame_header *new_hdr;
	struct fc_frame_header *temp_hdr;
	struct lpfc_dmabuf *d_buf, *n_buf, *h_buf;
	struct hbq_dmabuf *seq_dmabuf = NULL;

	 
	INIT_LIST_HEAD(&dmabuf->dbuf.list);
	INIT_LIST_HEAD(&dmabuf->hbuf.list);
	new_hdr = (struct fc_frame_header *)dmabuf->hbuf.virt;
	list_for_each_entry(h_buf, &vport->rcv_buffer_list, list) {
		temp_hdr = (struct fc_frame_header *)h_buf->virt;
		if ((temp_hdr->fh_seq_id != new_hdr->fh_seq_id) ||
		    (temp_hdr->fh_ox_id != new_hdr->fh_ox_id) ||
		    (memcmp(&temp_hdr->fh_s_id, &new_hdr->fh_s_id, 3)))
			continue;
		 
		seq_dmabuf = container_of(h_buf, struct hbq_dmabuf, hbuf);
		break;
	}

	 
	if (seq_dmabuf) {
		list_for_each_entry_safe(d_buf, n_buf,
					 &seq_dmabuf->dbuf.list, list) {
			list_del_init(&d_buf->list);
			lpfc_in_buf_free(vport->phba, d_buf);
		}
		return true;
	}
	return false;
}

 
static bool
lpfc_sli4_abort_ulp_seq(struct lpfc_vport *vport, struct hbq_dmabuf *dmabuf)
{
	struct lpfc_hba *phba = vport->phba;
	int handled;

	 
	if (phba->sli_rev < LPFC_SLI_REV4)
		return false;

	 
	handled = lpfc_ct_handle_unsol_abort(phba, dmabuf);
	if (handled)
		return true;

	return false;
}

 
static void
lpfc_sli4_seq_abort_rsp_cmpl(struct lpfc_hba *phba,
			     struct lpfc_iocbq *cmd_iocbq,
			     struct lpfc_iocbq *rsp_iocbq)
{
	if (cmd_iocbq) {
		lpfc_nlp_put(cmd_iocbq->ndlp);
		lpfc_sli_release_iocbq(phba, cmd_iocbq);
	}

	 
	if (rsp_iocbq && rsp_iocbq->iocb.ulpStatus)
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"3154 BLS ABORT RSP failed, data:  x%x/x%x\n",
			get_job_ulpstatus(phba, rsp_iocbq),
			get_job_word4(phba, rsp_iocbq));
}

 
uint16_t
lpfc_sli4_xri_inrange(struct lpfc_hba *phba,
		      uint16_t xri)
{
	uint16_t i;

	for (i = 0; i < phba->sli4_hba.max_cfg_param.max_xri; i++) {
		if (xri == phba->sli4_hba.xri_ids[i])
			return i;
	}
	return NO_XRI;
}

 
void
lpfc_sli4_seq_abort_rsp(struct lpfc_vport *vport,
			struct fc_frame_header *fc_hdr, bool aborted)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_iocbq *ctiocb = NULL;
	struct lpfc_nodelist *ndlp;
	uint16_t oxid, rxid, xri, lxri;
	uint32_t sid, fctl;
	union lpfc_wqe128 *icmd;
	int rc;

	if (!lpfc_is_link_up(phba))
		return;

	sid = sli4_sid_from_fc_hdr(fc_hdr);
	oxid = be16_to_cpu(fc_hdr->fh_ox_id);
	rxid = be16_to_cpu(fc_hdr->fh_rx_id);

	ndlp = lpfc_findnode_did(vport, sid);
	if (!ndlp) {
		ndlp = lpfc_nlp_init(vport, sid);
		if (!ndlp) {
			lpfc_printf_vlog(vport, KERN_WARNING, LOG_ELS,
					 "1268 Failed to allocate ndlp for "
					 "oxid:x%x SID:x%x\n", oxid, sid);
			return;
		}
		 
		lpfc_enqueue_node(vport, ndlp);
	}

	 
	ctiocb = lpfc_sli_get_iocbq(phba);
	if (!ctiocb)
		return;

	icmd = &ctiocb->wqe;

	 
	fctl = sli4_fctl_from_fc_hdr(fc_hdr);

	ctiocb->ndlp = lpfc_nlp_get(ndlp);
	if (!ctiocb->ndlp) {
		lpfc_sli_release_iocbq(phba, ctiocb);
		return;
	}

	ctiocb->vport = phba->pport;
	ctiocb->cmd_cmpl = lpfc_sli4_seq_abort_rsp_cmpl;
	ctiocb->sli4_lxritag = NO_XRI;
	ctiocb->sli4_xritag = NO_XRI;
	ctiocb->abort_rctl = FC_RCTL_BA_ACC;

	if (fctl & FC_FC_EX_CTX)
		 
		xri = oxid;
	else
		xri = rxid;
	lxri = lpfc_sli4_xri_inrange(phba, xri);
	if (lxri != NO_XRI)
		lpfc_set_rrq_active(phba, ndlp, lxri,
			(xri == oxid) ? rxid : oxid, 0);
	 
	if ((fctl & FC_FC_EX_CTX) &&
	    (lxri > lpfc_sli4_get_iocb_cnt(phba))) {
		ctiocb->abort_rctl = FC_RCTL_BA_RJT;
		bf_set(xmit_bls_rsp64_rjt_vspec, &icmd->xmit_bls_rsp, 0);
		bf_set(xmit_bls_rsp64_rjt_expc, &icmd->xmit_bls_rsp,
		       FC_BA_RJT_INV_XID);
		bf_set(xmit_bls_rsp64_rjt_rsnc, &icmd->xmit_bls_rsp,
		       FC_BA_RJT_UNABLE);
	}

	 
	if (aborted == false) {
		ctiocb->abort_rctl = FC_RCTL_BA_RJT;
		bf_set(xmit_bls_rsp64_rjt_vspec, &icmd->xmit_bls_rsp, 0);
		bf_set(xmit_bls_rsp64_rjt_expc, &icmd->xmit_bls_rsp,
		       FC_BA_RJT_INV_XID);
		bf_set(xmit_bls_rsp64_rjt_rsnc, &icmd->xmit_bls_rsp,
		       FC_BA_RJT_UNABLE);
	}

	if (fctl & FC_FC_EX_CTX) {
		 
		ctiocb->abort_bls = LPFC_ABTS_UNSOL_RSP;
		bf_set(xmit_bls_rsp64_rxid, &icmd->xmit_bls_rsp, rxid);
	} else {
		 
		ctiocb->abort_bls = LPFC_ABTS_UNSOL_INT;
	}

	 
	bf_set(xmit_bls_rsp64_oxid, &icmd->xmit_bls_rsp, oxid);
	bf_set(xmit_bls_rsp64_oxid, &icmd->xmit_bls_rsp, rxid);

	 
	bf_set(wqe_els_did, &icmd->xmit_bls_rsp.wqe_dest,
	       ndlp->nlp_DID);
	bf_set(xmit_bls_rsp64_temprpi, &icmd->xmit_bls_rsp,
	       phba->sli4_hba.rpi_ids[ndlp->nlp_rpi]);
	bf_set(wqe_cmnd, &icmd->generic.wqe_com, CMD_XMIT_BLS_RSP64_CX);

	 
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "1200 Send BLS cmd x%x on oxid x%x Data: x%x\n",
			 ctiocb->abort_rctl, oxid, phba->link_state);

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, ctiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				 "2925 Failed to issue CT ABTS RSP x%x on "
				 "xri x%x, Data x%x\n",
				 ctiocb->abort_rctl, oxid,
				 phba->link_state);
		lpfc_nlp_put(ndlp);
		ctiocb->ndlp = NULL;
		lpfc_sli_release_iocbq(phba, ctiocb);
	}
}

 
static void
lpfc_sli4_handle_unsol_abort(struct lpfc_vport *vport,
			     struct hbq_dmabuf *dmabuf)
{
	struct lpfc_hba *phba = vport->phba;
	struct fc_frame_header fc_hdr;
	uint32_t fctl;
	bool aborted;

	 
	memcpy(&fc_hdr, dmabuf->hbuf.virt, sizeof(struct fc_frame_header));
	fctl = sli4_fctl_from_fc_hdr(&fc_hdr);

	if (fctl & FC_FC_EX_CTX) {
		 
		aborted = true;
	} else {
		 
		aborted = lpfc_sli4_abort_partial_seq(vport, dmabuf);
		if (aborted == false)
			aborted = lpfc_sli4_abort_ulp_seq(vport, dmabuf);
	}
	lpfc_in_buf_free(phba, &dmabuf->dbuf);

	if (phba->nvmet_support) {
		lpfc_nvmet_rcv_unsol_abort(vport, &fc_hdr);
		return;
	}

	 
	lpfc_sli4_seq_abort_rsp(vport, &fc_hdr, aborted);
}

 
static int
lpfc_seq_complete(struct hbq_dmabuf *dmabuf)
{
	struct fc_frame_header *hdr;
	struct lpfc_dmabuf *d_buf;
	struct hbq_dmabuf *seq_dmabuf;
	uint32_t fctl;
	int seq_count = 0;

	hdr = (struct fc_frame_header *)dmabuf->hbuf.virt;
	 
	if (hdr->fh_seq_cnt != seq_count)
		return 0;
	fctl = (hdr->fh_f_ctl[0] << 16 |
		hdr->fh_f_ctl[1] << 8 |
		hdr->fh_f_ctl[2]);
	 
	if (fctl & FC_FC_END_SEQ)
		return 1;
	list_for_each_entry(d_buf, &dmabuf->dbuf.list, list) {
		seq_dmabuf = container_of(d_buf, struct hbq_dmabuf, dbuf);
		hdr = (struct fc_frame_header *)seq_dmabuf->hbuf.virt;
		 
		if (++seq_count != be16_to_cpu(hdr->fh_seq_cnt))
			return 0;
		fctl = (hdr->fh_f_ctl[0] << 16 |
			hdr->fh_f_ctl[1] << 8 |
			hdr->fh_f_ctl[2]);
		 
		if (fctl & FC_FC_END_SEQ)
			return 1;
	}
	return 0;
}

 
static struct lpfc_iocbq *
lpfc_prep_seq(struct lpfc_vport *vport, struct hbq_dmabuf *seq_dmabuf)
{
	struct hbq_dmabuf *hbq_buf;
	struct lpfc_dmabuf *d_buf, *n_buf;
	struct lpfc_iocbq *first_iocbq, *iocbq;
	struct fc_frame_header *fc_hdr;
	uint32_t sid;
	uint32_t len, tot_len;

	fc_hdr = (struct fc_frame_header *)seq_dmabuf->hbuf.virt;
	 
	list_del_init(&seq_dmabuf->hbuf.list);
	lpfc_update_rcv_time_stamp(vport);
	 
	sid = sli4_sid_from_fc_hdr(fc_hdr);
	tot_len = 0;
	 
	first_iocbq = lpfc_sli_get_iocbq(vport->phba);
	if (first_iocbq) {
		 
		first_iocbq->wcqe_cmpl.total_data_placed = 0;
		bf_set(lpfc_wcqe_c_status, &first_iocbq->wcqe_cmpl,
		       IOSTAT_SUCCESS);
		first_iocbq->vport = vport;

		 
		if (sli4_type_from_fc_hdr(fc_hdr) == FC_TYPE_ELS) {
			bf_set(els_rsp64_sid, &first_iocbq->wqe.xmit_els_rsp,
			       sli4_did_from_fc_hdr(fc_hdr));
		}

		bf_set(wqe_ctxt_tag, &first_iocbq->wqe.xmit_els_rsp.wqe_com,
		       NO_XRI);
		bf_set(wqe_rcvoxid, &first_iocbq->wqe.xmit_els_rsp.wqe_com,
		       be16_to_cpu(fc_hdr->fh_ox_id));

		 
		tot_len = bf_get(lpfc_rcqe_length,
				 &seq_dmabuf->cq_event.cqe.rcqe_cmpl);

		first_iocbq->cmd_dmabuf = &seq_dmabuf->dbuf;
		first_iocbq->bpl_dmabuf = NULL;
		 
		first_iocbq->wcqe_cmpl.word3 = 1;

		if (tot_len > LPFC_DATA_BUF_SIZE)
			first_iocbq->wqe.gen_req.bde.tus.f.bdeSize =
				LPFC_DATA_BUF_SIZE;
		else
			first_iocbq->wqe.gen_req.bde.tus.f.bdeSize = tot_len;

		first_iocbq->wcqe_cmpl.total_data_placed = tot_len;
		bf_set(wqe_els_did, &first_iocbq->wqe.xmit_els_rsp.wqe_dest,
		       sid);
	}
	iocbq = first_iocbq;
	 
	list_for_each_entry_safe(d_buf, n_buf, &seq_dmabuf->dbuf.list, list) {
		if (!iocbq) {
			lpfc_in_buf_free(vport->phba, d_buf);
			continue;
		}
		if (!iocbq->bpl_dmabuf) {
			iocbq->bpl_dmabuf = d_buf;
			iocbq->wcqe_cmpl.word3++;
			 
			hbq_buf = container_of(d_buf, struct hbq_dmabuf, dbuf);
			len = bf_get(lpfc_rcqe_length,
				       &hbq_buf->cq_event.cqe.rcqe_cmpl);
			iocbq->unsol_rcv_len = len;
			iocbq->wcqe_cmpl.total_data_placed += len;
			tot_len += len;
		} else {
			iocbq = lpfc_sli_get_iocbq(vport->phba);
			if (!iocbq) {
				if (first_iocbq) {
					bf_set(lpfc_wcqe_c_status,
					       &first_iocbq->wcqe_cmpl,
					       IOSTAT_SUCCESS);
					first_iocbq->wcqe_cmpl.parameter =
						IOERR_NO_RESOURCES;
				}
				lpfc_in_buf_free(vport->phba, d_buf);
				continue;
			}
			 
			hbq_buf = container_of(d_buf, struct hbq_dmabuf, dbuf);
			len = bf_get(lpfc_rcqe_length,
				       &hbq_buf->cq_event.cqe.rcqe_cmpl);
			iocbq->cmd_dmabuf = d_buf;
			iocbq->bpl_dmabuf = NULL;
			iocbq->wcqe_cmpl.word3 = 1;

			if (len > LPFC_DATA_BUF_SIZE)
				iocbq->wqe.xmit_els_rsp.bde.tus.f.bdeSize =
					LPFC_DATA_BUF_SIZE;
			else
				iocbq->wqe.xmit_els_rsp.bde.tus.f.bdeSize =
					len;

			tot_len += len;
			iocbq->wcqe_cmpl.total_data_placed = tot_len;
			bf_set(wqe_els_did, &iocbq->wqe.xmit_els_rsp.wqe_dest,
			       sid);
			list_add_tail(&iocbq->list, &first_iocbq->list);
		}
	}
	 
	if (!first_iocbq)
		lpfc_in_buf_free(vport->phba, &seq_dmabuf->dbuf);

	return first_iocbq;
}

static void
lpfc_sli4_send_seq_to_ulp(struct lpfc_vport *vport,
			  struct hbq_dmabuf *seq_dmabuf)
{
	struct fc_frame_header *fc_hdr;
	struct lpfc_iocbq *iocbq, *curr_iocb, *next_iocb;
	struct lpfc_hba *phba = vport->phba;

	fc_hdr = (struct fc_frame_header *)seq_dmabuf->hbuf.virt;
	iocbq = lpfc_prep_seq(vport, seq_dmabuf);
	if (!iocbq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2707 Ring %d handler: Failed to allocate "
				"iocb Rctl x%x Type x%x received\n",
				LPFC_ELS_RING,
				fc_hdr->fh_r_ctl, fc_hdr->fh_type);
		return;
	}
	if (!lpfc_complete_unsol_iocb(phba,
				      phba->sli4_hba.els_wq->pring,
				      iocbq, fc_hdr->fh_r_ctl,
				      fc_hdr->fh_type)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2540 Ring %d handler: unexpected Rctl "
				"x%x Type x%x received\n",
				LPFC_ELS_RING,
				fc_hdr->fh_r_ctl, fc_hdr->fh_type);
		lpfc_in_buf_free(phba, &seq_dmabuf->dbuf);
	}

	 
	list_for_each_entry_safe(curr_iocb, next_iocb,
				 &iocbq->list, list) {
		list_del_init(&curr_iocb->list);
		lpfc_sli_release_iocbq(phba, curr_iocb);
	}
	lpfc_sli_release_iocbq(phba, iocbq);
}

static void
lpfc_sli4_mds_loopback_cmpl(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
			    struct lpfc_iocbq *rspiocb)
{
	struct lpfc_dmabuf *pcmd = cmdiocb->cmd_dmabuf;

	if (pcmd && pcmd->virt)
		dma_pool_free(phba->lpfc_drb_pool, pcmd->virt, pcmd->phys);
	kfree(pcmd);
	lpfc_sli_release_iocbq(phba, cmdiocb);
	lpfc_drain_txq(phba);
}

static void
lpfc_sli4_handle_mds_loopback(struct lpfc_vport *vport,
			      struct hbq_dmabuf *dmabuf)
{
	struct fc_frame_header *fc_hdr;
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_iocbq *iocbq = NULL;
	union  lpfc_wqe128 *pwqe;
	struct lpfc_dmabuf *pcmd = NULL;
	uint32_t frame_len;
	int rc;
	unsigned long iflags;

	fc_hdr = (struct fc_frame_header *)dmabuf->hbuf.virt;
	frame_len = bf_get(lpfc_rcqe_length, &dmabuf->cq_event.cqe.rcqe_cmpl);

	 
	iocbq = lpfc_sli_get_iocbq(phba);
	if (!iocbq) {
		 
		spin_lock_irqsave(&phba->hbalock, iflags);
		list_add_tail(&dmabuf->cq_event.list,
			      &phba->sli4_hba.sp_queue_event);
		phba->hba_flag |= HBA_SP_QUEUE_EVT;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		lpfc_worker_wake_up(phba);
		return;
	}

	 
	pcmd = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (pcmd)
		pcmd->virt = dma_pool_alloc(phba->lpfc_drb_pool, GFP_KERNEL,
					    &pcmd->phys);
	if (!pcmd || !pcmd->virt)
		goto exit;

	INIT_LIST_HEAD(&pcmd->list);

	 
	memcpy(pcmd->virt, dmabuf->dbuf.virt, frame_len);

	iocbq->cmd_dmabuf = pcmd;
	iocbq->vport = vport;
	iocbq->cmd_flag &= ~LPFC_FIP_ELS_ID_MASK;
	iocbq->cmd_flag |= LPFC_USE_FCPWQIDX;
	iocbq->num_bdes = 0;

	pwqe = &iocbq->wqe;
	 
	pwqe->gen_req.bde.addrHigh = putPaddrHigh(pcmd->phys);
	pwqe->gen_req.bde.addrLow = putPaddrLow(pcmd->phys);
	pwqe->gen_req.bde.tus.f.bdeSize = frame_len;
	pwqe->gen_req.bde.tus.f.bdeFlags = BUFF_TYPE_BDE_64;

	pwqe->send_frame.frame_len = frame_len;
	pwqe->send_frame.fc_hdr_wd0 = be32_to_cpu(*((__be32 *)fc_hdr));
	pwqe->send_frame.fc_hdr_wd1 = be32_to_cpu(*((__be32 *)fc_hdr + 1));
	pwqe->send_frame.fc_hdr_wd2 = be32_to_cpu(*((__be32 *)fc_hdr + 2));
	pwqe->send_frame.fc_hdr_wd3 = be32_to_cpu(*((__be32 *)fc_hdr + 3));
	pwqe->send_frame.fc_hdr_wd4 = be32_to_cpu(*((__be32 *)fc_hdr + 4));
	pwqe->send_frame.fc_hdr_wd5 = be32_to_cpu(*((__be32 *)fc_hdr + 5));

	pwqe->generic.wqe_com.word7 = 0;
	pwqe->generic.wqe_com.word10 = 0;

	bf_set(wqe_cmnd, &pwqe->generic.wqe_com, CMD_SEND_FRAME);
	bf_set(wqe_sof, &pwqe->generic.wqe_com, 0x2E);  
	bf_set(wqe_eof, &pwqe->generic.wqe_com, 0x41);  
	bf_set(wqe_lenloc, &pwqe->generic.wqe_com, 1);
	bf_set(wqe_xbl, &pwqe->generic.wqe_com, 1);
	bf_set(wqe_dbde, &pwqe->generic.wqe_com, 1);
	bf_set(wqe_xc, &pwqe->generic.wqe_com, 1);
	bf_set(wqe_cmd_type, &pwqe->generic.wqe_com, 0xA);
	bf_set(wqe_cqid, &pwqe->generic.wqe_com, LPFC_WQE_CQ_ID_DEFAULT);
	bf_set(wqe_xri_tag, &pwqe->generic.wqe_com, iocbq->sli4_xritag);
	bf_set(wqe_reqtag, &pwqe->generic.wqe_com, iocbq->iotag);
	bf_set(wqe_class, &pwqe->generic.wqe_com, CLASS3);
	pwqe->generic.wqe_com.abort_tag = iocbq->iotag;

	iocbq->cmd_cmpl = lpfc_sli4_mds_loopback_cmpl;

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, iocbq, 0);
	if (rc == IOCB_ERROR)
		goto exit;

	lpfc_in_buf_free(phba, &dmabuf->dbuf);
	return;

exit:
	lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
			"2023 Unable to process MDS loopback frame\n");
	if (pcmd && pcmd->virt)
		dma_pool_free(phba->lpfc_drb_pool, pcmd->virt, pcmd->phys);
	kfree(pcmd);
	if (iocbq)
		lpfc_sli_release_iocbq(phba, iocbq);
	lpfc_in_buf_free(phba, &dmabuf->dbuf);
}

 
void
lpfc_sli4_handle_received_buffer(struct lpfc_hba *phba,
				 struct hbq_dmabuf *dmabuf)
{
	struct hbq_dmabuf *seq_dmabuf;
	struct fc_frame_header *fc_hdr;
	struct lpfc_vport *vport;
	uint32_t fcfi;
	uint32_t did;

	 
	fc_hdr = (struct fc_frame_header *)dmabuf->hbuf.virt;

	if (fc_hdr->fh_r_ctl == FC_RCTL_MDS_DIAGS ||
	    fc_hdr->fh_r_ctl == FC_RCTL_DD_UNSOL_DATA) {
		vport = phba->pport;
		 
		if  (!(phba->pport->load_flag & FC_UNLOADING))
			lpfc_sli4_handle_mds_loopback(vport, dmabuf);
		else
			lpfc_in_buf_free(phba, &dmabuf->dbuf);
		return;
	}

	 
	if (lpfc_fc_frame_check(phba, fc_hdr)) {
		lpfc_in_buf_free(phba, &dmabuf->dbuf);
		return;
	}

	if ((bf_get(lpfc_cqe_code,
		    &dmabuf->cq_event.cqe.rcqe_cmpl) == CQE_CODE_RECEIVE_V1))
		fcfi = bf_get(lpfc_rcqe_fcf_id_v1,
			      &dmabuf->cq_event.cqe.rcqe_cmpl);
	else
		fcfi = bf_get(lpfc_rcqe_fcf_id,
			      &dmabuf->cq_event.cqe.rcqe_cmpl);

	if (fc_hdr->fh_r_ctl == 0xF4 && fc_hdr->fh_type == 0xFF) {
		vport = phba->pport;
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"2023 MDS Loopback %d bytes\n",
				bf_get(lpfc_rcqe_length,
				       &dmabuf->cq_event.cqe.rcqe_cmpl));
		 
		lpfc_sli4_handle_mds_loopback(vport, dmabuf);
		return;
	}

	 
	did = sli4_did_from_fc_hdr(fc_hdr);

	vport = lpfc_fc_frame_to_vport(phba, fc_hdr, fcfi, did);
	if (!vport) {
		 
		lpfc_in_buf_free(phba, &dmabuf->dbuf);
		return;
	}

	 
	if (!(vport->vpi_state & LPFC_VPI_REGISTERED) &&
		(did != Fabric_DID)) {
		 
		if (!(vport->fc_flag & FC_PT2PT) ||
			(phba->link_state == LPFC_HBA_READY)) {
			lpfc_in_buf_free(phba, &dmabuf->dbuf);
			return;
		}
	}

	 
	if (fc_hdr->fh_r_ctl == FC_RCTL_BA_ABTS) {
		lpfc_sli4_handle_unsol_abort(vport, dmabuf);
		return;
	}

	 
	seq_dmabuf = lpfc_fc_frame_add(vport, dmabuf);
	if (!seq_dmabuf) {
		 
		lpfc_in_buf_free(phba, &dmabuf->dbuf);
		return;
	}
	 
	if (!lpfc_seq_complete(seq_dmabuf))
		return;

	 
	lpfc_sli4_send_seq_to_ulp(vport, seq_dmabuf);
}

 
int
lpfc_sli4_post_all_rpi_hdrs(struct lpfc_hba *phba)
{
	struct lpfc_rpi_hdr *rpi_page;
	uint32_t rc = 0;
	uint16_t lrpi = 0;

	 
	if (!phba->sli4_hba.rpi_hdrs_in_use)
		goto exit;
	if (phba->sli4_hba.extents_in_use)
		return -EIO;

	list_for_each_entry(rpi_page, &phba->sli4_hba.lpfc_rpi_hdr_list, list) {
		 
		if (bf_get(lpfc_rpi_rsrc_rdy, &phba->sli4_hba.sli4_flags) !=
		    LPFC_RPI_RSRC_RDY)
			rpi_page->start_rpi = phba->sli4_hba.rpi_ids[lrpi];

		rc = lpfc_sli4_post_rpi_hdr(phba, rpi_page);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"2008 Error %d posting all rpi "
					"headers\n", rc);
			rc = -EIO;
			break;
		}
	}

 exit:
	bf_set(lpfc_rpi_rsrc_rdy, &phba->sli4_hba.sli4_flags,
	       LPFC_RPI_RSRC_RDY);
	return rc;
}

 
int
lpfc_sli4_post_rpi_hdr(struct lpfc_hba *phba, struct lpfc_rpi_hdr *rpi_page)
{
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_mbx_post_hdr_tmpl *hdr_tmpl;
	uint32_t rc = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	 
	if (!phba->sli4_hba.rpi_hdrs_in_use)
		return rc;
	if (phba->sli4_hba.extents_in_use)
		return -EIO;

	 
	mboxq = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2001 Unable to allocate memory for issuing "
				"SLI_CONFIG_SPECIAL mailbox command\n");
		return -ENOMEM;
	}

	 
	hdr_tmpl = &mboxq->u.mqe.un.hdr_tmpl;
	lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_POST_HDR_TEMPLATE,
			 sizeof(struct lpfc_mbx_post_hdr_tmpl) -
			 sizeof(struct lpfc_sli4_cfg_mhdr),
			 LPFC_SLI4_MBX_EMBED);


	 
	bf_set(lpfc_mbx_post_hdr_tmpl_rpi_offset, hdr_tmpl,
	       rpi_page->start_rpi);
	bf_set(lpfc_mbx_post_hdr_tmpl_page_cnt,
	       hdr_tmpl, rpi_page->page_count);

	hdr_tmpl->rpi_paddr_lo = putPaddrLow(rpi_page->dmabuf->phys);
	hdr_tmpl->rpi_paddr_hi = putPaddrHigh(rpi_page->dmabuf->phys);
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	shdr = (union lpfc_sli4_cfg_shdr *) &hdr_tmpl->header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	mempool_free(mboxq, phba->mbox_mem_pool);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2514 POST_RPI_HDR mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		rc = -ENXIO;
	} else {
		 
		spin_lock_irq(&phba->hbalock);
		phba->sli4_hba.next_rpi = rpi_page->next_rpi;
		spin_unlock_irq(&phba->hbalock);
	}
	return rc;
}

 
int
lpfc_sli4_alloc_rpi(struct lpfc_hba *phba)
{
	unsigned long rpi;
	uint16_t max_rpi, rpi_limit;
	uint16_t rpi_remaining, lrpi = 0;
	struct lpfc_rpi_hdr *rpi_hdr;
	unsigned long iflag;

	 
	spin_lock_irqsave(&phba->hbalock, iflag);
	max_rpi = phba->sli4_hba.max_cfg_param.max_rpi;
	rpi_limit = phba->sli4_hba.next_rpi;

	rpi = find_first_zero_bit(phba->sli4_hba.rpi_bmask, rpi_limit);
	if (rpi >= rpi_limit)
		rpi = LPFC_RPI_ALLOC_ERROR;
	else {
		set_bit(rpi, phba->sli4_hba.rpi_bmask);
		phba->sli4_hba.max_cfg_param.rpi_used++;
		phba->sli4_hba.rpi_count++;
	}
	lpfc_printf_log(phba, KERN_INFO,
			LOG_NODE | LOG_DISCOVERY,
			"0001 Allocated rpi:x%x max:x%x lim:x%x\n",
			(int) rpi, max_rpi, rpi_limit);

	 
	if ((rpi == LPFC_RPI_ALLOC_ERROR) &&
	    (phba->sli4_hba.rpi_count >= max_rpi)) {
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		return rpi;
	}

	 
	if (!phba->sli4_hba.rpi_hdrs_in_use) {
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		return rpi;
	}

	 
	rpi_remaining = phba->sli4_hba.next_rpi - phba->sli4_hba.rpi_count;
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	if (rpi_remaining < LPFC_RPI_LOW_WATER_MARK) {
		rpi_hdr = lpfc_sli4_create_rpi_hdr(phba);
		if (!rpi_hdr) {
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"2002 Error Could not grow rpi "
					"count\n");
		} else {
			lrpi = rpi_hdr->start_rpi;
			rpi_hdr->start_rpi = phba->sli4_hba.rpi_ids[lrpi];
			lpfc_sli4_post_rpi_hdr(phba, rpi_hdr);
		}
	}

	return rpi;
}

 
static void
__lpfc_sli4_free_rpi(struct lpfc_hba *phba, int rpi)
{
	 
	if (rpi == LPFC_RPI_ALLOC_ERROR)
		return;

	if (test_and_clear_bit(rpi, phba->sli4_hba.rpi_bmask)) {
		phba->sli4_hba.rpi_count--;
		phba->sli4_hba.max_cfg_param.rpi_used--;
	} else {
		lpfc_printf_log(phba, KERN_INFO,
				LOG_NODE | LOG_DISCOVERY,
				"2016 rpi %x not inuse\n",
				rpi);
	}
}

 
void
lpfc_sli4_free_rpi(struct lpfc_hba *phba, int rpi)
{
	spin_lock_irq(&phba->hbalock);
	__lpfc_sli4_free_rpi(phba, rpi);
	spin_unlock_irq(&phba->hbalock);
}

 
void
lpfc_sli4_remove_rpis(struct lpfc_hba *phba)
{
	kfree(phba->sli4_hba.rpi_bmask);
	kfree(phba->sli4_hba.rpi_ids);
	bf_set(lpfc_rpi_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);
}

 
int
lpfc_sli4_resume_rpi(struct lpfc_nodelist *ndlp,
	void (*cmpl)(struct lpfc_hba *, LPFC_MBOXQ_t *), void *arg)
{
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_hba *phba = ndlp->phba;
	int rc;

	 
	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		return -ENOMEM;

	 
	if (!lpfc_nlp_get(ndlp)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2122 %s: Failed to get nlp ref\n",
				__func__);
		mempool_free(mboxq, phba->mbox_mem_pool);
		return -EIO;
	}

	 
	lpfc_resume_rpi(mboxq, ndlp);
	if (cmpl) {
		mboxq->mbox_cmpl = cmpl;
		mboxq->ctx_buf = arg;
	} else
		mboxq->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	mboxq->ctx_ndlp = ndlp;
	mboxq->vport = ndlp->vport;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2010 Resume RPI Mailbox failed "
				"status %d, mbxStatus x%x\n", rc,
				bf_get(lpfc_mqe_status, &mboxq->u.mqe));
		lpfc_nlp_put(ndlp);
		mempool_free(mboxq, phba->mbox_mem_pool);
		return -EIO;
	}
	return 0;
}

 
int
lpfc_sli4_init_vpi(struct lpfc_vport *vport)
{
	LPFC_MBOXQ_t *mboxq;
	int rc = 0;
	int retval = MBX_SUCCESS;
	uint32_t mbox_tmo;
	struct lpfc_hba *phba = vport->phba;
	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		return -ENOMEM;
	lpfc_init_vpi(phba, mboxq, vport->vpi);
	mbox_tmo = lpfc_mbox_tmo_val(phba, mboxq);
	rc = lpfc_sli_issue_mbox_wait(phba, mboxq, mbox_tmo);
	if (rc != MBX_SUCCESS) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_TRACE_EVENT,
				"2022 INIT VPI Mailbox failed "
				"status %d, mbxStatus x%x\n", rc,
				bf_get(lpfc_mqe_status, &mboxq->u.mqe));
		retval = -EIO;
	}
	if (rc != MBX_TIMEOUT)
		mempool_free(mboxq, vport->phba->mbox_mem_pool);

	return retval;
}

 
static void
lpfc_mbx_cmpl_add_fcf_record(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	void *virt_addr;
	union lpfc_sli4_cfg_shdr *shdr;
	uint32_t shdr_status, shdr_add_status;

	virt_addr = mboxq->sge_array->addr[0];
	 
	shdr = (union lpfc_sli4_cfg_shdr *) virt_addr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);

	if ((shdr_status || shdr_add_status) &&
		(shdr_status != STATUS_FCF_IN_USE))
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"2558 ADD_FCF_RECORD mailbox failed with "
			"status x%x add_status x%x\n",
			shdr_status, shdr_add_status);

	lpfc_sli4_mbox_cmd_free(phba, mboxq);
}

 
int
lpfc_sli4_add_fcf_record(struct lpfc_hba *phba, struct fcf_record *fcf_record)
{
	int rc = 0;
	LPFC_MBOXQ_t *mboxq;
	uint8_t *bytep;
	void *virt_addr;
	struct lpfc_mbx_sge sge;
	uint32_t alloc_len, req_len;
	uint32_t fcfindex;

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"2009 Failed to allocate mbox for ADD_FCF cmd\n");
		return -ENOMEM;
	}

	req_len = sizeof(struct fcf_record) + sizeof(union lpfc_sli4_cfg_shdr) +
		  sizeof(uint32_t);

	 
	alloc_len = lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_FCOE,
				     LPFC_MBOX_OPCODE_FCOE_ADD_FCF,
				     req_len, LPFC_SLI4_MBX_NEMBED);
	if (alloc_len < req_len) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"2523 Allocated DMA memory size (x%x) is "
			"less than the requested DMA memory "
			"size (x%x)\n", alloc_len, req_len);
		lpfc_sli4_mbox_cmd_free(phba, mboxq);
		return -ENOMEM;
	}

	 
	lpfc_sli4_mbx_sge_get(mboxq, 0, &sge);
	virt_addr = mboxq->sge_array->addr[0];
	 
	fcfindex = bf_get(lpfc_fcf_record_fcf_index, fcf_record);
	bytep = virt_addr + sizeof(union lpfc_sli4_cfg_shdr);
	lpfc_sli_pcimem_bcopy(&fcfindex, bytep, sizeof(uint32_t));

	 
	bytep += sizeof(uint32_t);
	lpfc_sli_pcimem_bcopy(fcf_record, bytep, sizeof(struct fcf_record));
	mboxq->vport = phba->pport;
	mboxq->mbox_cmpl = lpfc_mbx_cmpl_add_fcf_record;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"2515 ADD_FCF_RECORD mailbox failed with "
			"status 0x%x\n", rc);
		lpfc_sli4_mbox_cmd_free(phba, mboxq);
		rc = -EIO;
	} else
		rc = 0;

	return rc;
}

 
void
lpfc_sli4_build_dflt_fcf_record(struct lpfc_hba *phba,
				struct fcf_record *fcf_record,
				uint16_t fcf_index)
{
	memset(fcf_record, 0, sizeof(struct fcf_record));
	fcf_record->max_rcv_size = LPFC_FCOE_MAX_RCV_SIZE;
	fcf_record->fka_adv_period = LPFC_FCOE_FKA_ADV_PER;
	fcf_record->fip_priority = LPFC_FCOE_FIP_PRIORITY;
	bf_set(lpfc_fcf_record_mac_0, fcf_record, phba->fc_map[0]);
	bf_set(lpfc_fcf_record_mac_1, fcf_record, phba->fc_map[1]);
	bf_set(lpfc_fcf_record_mac_2, fcf_record, phba->fc_map[2]);
	bf_set(lpfc_fcf_record_mac_3, fcf_record, LPFC_FCOE_FCF_MAC3);
	bf_set(lpfc_fcf_record_mac_4, fcf_record, LPFC_FCOE_FCF_MAC4);
	bf_set(lpfc_fcf_record_mac_5, fcf_record, LPFC_FCOE_FCF_MAC5);
	bf_set(lpfc_fcf_record_fc_map_0, fcf_record, phba->fc_map[0]);
	bf_set(lpfc_fcf_record_fc_map_1, fcf_record, phba->fc_map[1]);
	bf_set(lpfc_fcf_record_fc_map_2, fcf_record, phba->fc_map[2]);
	bf_set(lpfc_fcf_record_fcf_valid, fcf_record, 1);
	bf_set(lpfc_fcf_record_fcf_avail, fcf_record, 1);
	bf_set(lpfc_fcf_record_fcf_index, fcf_record, fcf_index);
	bf_set(lpfc_fcf_record_mac_addr_prov, fcf_record,
		LPFC_FCF_FPMA | LPFC_FCF_SPMA);
	 
	if (phba->valid_vlan) {
		fcf_record->vlan_bitmap[phba->vlan_id / 8]
			= 1 << (phba->vlan_id % 8);
	}
}

 
int
lpfc_sli4_fcf_scan_read_fcf_rec(struct lpfc_hba *phba, uint16_t fcf_index)
{
	int rc = 0, error;
	LPFC_MBOXQ_t *mboxq;

	phba->fcoe_eventtag_at_fcf_scan = phba->fcoe_eventtag;
	phba->fcoe_cvl_eventtag_attn = phba->fcoe_cvl_eventtag;
	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2000 Failed to allocate mbox for "
				"READ_FCF cmd\n");
		error = -ENOMEM;
		goto fail_fcf_scan;
	}
	 
	rc = lpfc_sli4_mbx_read_fcf_rec(phba, mboxq, fcf_index);
	if (rc) {
		error = -EINVAL;
		goto fail_fcf_scan;
	}
	 
	mboxq->vport = phba->pport;
	mboxq->mbox_cmpl = lpfc_mbx_cmpl_fcf_scan_read_fcf_rec;

	spin_lock_irq(&phba->hbalock);
	phba->hba_flag |= FCF_TS_INPROG;
	spin_unlock_irq(&phba->hbalock);

	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED)
		error = -EIO;
	else {
		 
		if (fcf_index == LPFC_FCOE_FCF_GET_FIRST)
			phba->fcf.eligible_fcf_cnt = 0;
		error = 0;
	}
fail_fcf_scan:
	if (error) {
		if (mboxq)
			lpfc_sli4_mbox_cmd_free(phba, mboxq);
		 
		spin_lock_irq(&phba->hbalock);
		phba->hba_flag &= ~FCF_TS_INPROG;
		spin_unlock_irq(&phba->hbalock);
	}
	return error;
}

 
int
lpfc_sli4_fcf_rr_read_fcf_rec(struct lpfc_hba *phba, uint16_t fcf_index)
{
	int rc = 0, error;
	LPFC_MBOXQ_t *mboxq;

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP | LOG_INIT,
				"2763 Failed to allocate mbox for "
				"READ_FCF cmd\n");
		error = -ENOMEM;
		goto fail_fcf_read;
	}
	 
	rc = lpfc_sli4_mbx_read_fcf_rec(phba, mboxq, fcf_index);
	if (rc) {
		error = -EINVAL;
		goto fail_fcf_read;
	}
	 
	mboxq->vport = phba->pport;
	mboxq->mbox_cmpl = lpfc_mbx_cmpl_fcf_rr_read_fcf_rec;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED)
		error = -EIO;
	else
		error = 0;

fail_fcf_read:
	if (error && mboxq)
		lpfc_sli4_mbox_cmd_free(phba, mboxq);
	return error;
}

 
int
lpfc_sli4_read_fcf_rec(struct lpfc_hba *phba, uint16_t fcf_index)
{
	int rc = 0, error;
	LPFC_MBOXQ_t *mboxq;

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP | LOG_INIT,
				"2758 Failed to allocate mbox for "
				"READ_FCF cmd\n");
				error = -ENOMEM;
				goto fail_fcf_read;
	}
	 
	rc = lpfc_sli4_mbx_read_fcf_rec(phba, mboxq, fcf_index);
	if (rc) {
		error = -EINVAL;
		goto fail_fcf_read;
	}
	 
	mboxq->vport = phba->pport;
	mboxq->mbox_cmpl = lpfc_mbx_cmpl_read_fcf_rec;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED)
		error = -EIO;
	else
		error = 0;

fail_fcf_read:
	if (error && mboxq)
		lpfc_sli4_mbox_cmd_free(phba, mboxq);
	return error;
}

 
static int
lpfc_check_next_fcf_pri_level(struct lpfc_hba *phba)
{
	uint16_t next_fcf_pri;
	uint16_t last_index;
	struct lpfc_fcf_pri *fcf_pri;
	int rc;
	int ret = 0;

	last_index = find_first_bit(phba->fcf.fcf_rr_bmask,
			LPFC_SLI4_FCF_TBL_INDX_MAX);
	lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
			"3060 Last IDX %d\n", last_index);

	 
	spin_lock_irq(&phba->hbalock);
	if (list_empty(&phba->fcf.fcf_pri_list) ||
	    list_is_singular(&phba->fcf.fcf_pri_list)) {
		spin_unlock_irq(&phba->hbalock);
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP,
			"3061 Last IDX %d\n", last_index);
		return 0;  
	}
	spin_unlock_irq(&phba->hbalock);

	next_fcf_pri = 0;
	 
	memset(phba->fcf.fcf_rr_bmask, 0,
			sizeof(*phba->fcf.fcf_rr_bmask));
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry(fcf_pri, &phba->fcf.fcf_pri_list, list) {
		if (fcf_pri->fcf_rec.flag & LPFC_FCF_FLOGI_FAILED)
			continue;
		 
		if (!next_fcf_pri)
			next_fcf_pri = fcf_pri->fcf_rec.priority;
		spin_unlock_irq(&phba->hbalock);
		if (fcf_pri->fcf_rec.priority == next_fcf_pri) {
			rc = lpfc_sli4_fcf_rr_index_set(phba,
						fcf_pri->fcf_rec.fcf_index);
			if (rc)
				return 0;
		}
		spin_lock_irq(&phba->hbalock);
	}
	 
	if (!next_fcf_pri && !list_empty(&phba->fcf.fcf_pri_list)) {
		list_for_each_entry(fcf_pri, &phba->fcf.fcf_pri_list, list) {
			fcf_pri->fcf_rec.flag &= ~LPFC_FCF_FLOGI_FAILED;
			 
			if (!next_fcf_pri)
				next_fcf_pri = fcf_pri->fcf_rec.priority;
			spin_unlock_irq(&phba->hbalock);
			if (fcf_pri->fcf_rec.priority == next_fcf_pri) {
				rc = lpfc_sli4_fcf_rr_index_set(phba,
						fcf_pri->fcf_rec.fcf_index);
				if (rc)
					return 0;
			}
			spin_lock_irq(&phba->hbalock);
		}
	} else
		ret = 1;
	spin_unlock_irq(&phba->hbalock);

	return ret;
}
 
uint16_t
lpfc_sli4_fcf_rr_next_index_get(struct lpfc_hba *phba)
{
	uint16_t next_fcf_index;

initial_priority:
	 
	next_fcf_index = phba->fcf.current_rec.fcf_indx;

next_priority:
	 
	next_fcf_index = (next_fcf_index + 1) % LPFC_SLI4_FCF_TBL_INDX_MAX;
	next_fcf_index = find_next_bit(phba->fcf.fcf_rr_bmask,
				       LPFC_SLI4_FCF_TBL_INDX_MAX,
				       next_fcf_index);

	 
	if (next_fcf_index >= LPFC_SLI4_FCF_TBL_INDX_MAX) {
		 
		next_fcf_index = find_first_bit(phba->fcf.fcf_rr_bmask,
					       LPFC_SLI4_FCF_TBL_INDX_MAX);
	}


	 
	if (next_fcf_index >= LPFC_SLI4_FCF_TBL_INDX_MAX ||
		next_fcf_index == phba->fcf.current_rec.fcf_indx) {
		 
		if (lpfc_check_next_fcf_pri_level(phba))
			goto initial_priority;
		lpfc_printf_log(phba, KERN_WARNING, LOG_FIP,
				"2844 No roundrobin failover FCF available\n");

		return LPFC_FCOE_FCF_NEXT_NONE;
	}

	if (next_fcf_index < LPFC_SLI4_FCF_TBL_INDX_MAX &&
		phba->fcf.fcf_pri[next_fcf_index].fcf_rec.flag &
		LPFC_FCF_FLOGI_FAILED) {
		if (list_is_singular(&phba->fcf.fcf_pri_list))
			return LPFC_FCOE_FCF_NEXT_NONE;

		goto next_priority;
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
			"2845 Get next roundrobin failover FCF (x%x)\n",
			next_fcf_index);

	return next_fcf_index;
}

 
int
lpfc_sli4_fcf_rr_index_set(struct lpfc_hba *phba, uint16_t fcf_index)
{
	if (fcf_index >= LPFC_SLI4_FCF_TBL_INDX_MAX) {
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP,
				"2610 FCF (x%x) reached driver's book "
				"keeping dimension:x%x\n",
				fcf_index, LPFC_SLI4_FCF_TBL_INDX_MAX);
		return -EINVAL;
	}
	 
	set_bit(fcf_index, phba->fcf.fcf_rr_bmask);

	lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
			"2790 Set FCF (x%x) to roundrobin FCF failover "
			"bmask\n", fcf_index);

	return 0;
}

 
void
lpfc_sli4_fcf_rr_index_clear(struct lpfc_hba *phba, uint16_t fcf_index)
{
	struct lpfc_fcf_pri *fcf_pri, *fcf_pri_next;
	if (fcf_index >= LPFC_SLI4_FCF_TBL_INDX_MAX) {
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP,
				"2762 FCF (x%x) reached driver's book "
				"keeping dimension:x%x\n",
				fcf_index, LPFC_SLI4_FCF_TBL_INDX_MAX);
		return;
	}
	 
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(fcf_pri, fcf_pri_next, &phba->fcf.fcf_pri_list,
				 list) {
		if (fcf_pri->fcf_rec.fcf_index == fcf_index) {
			list_del_init(&fcf_pri->list);
			break;
		}
	}
	spin_unlock_irq(&phba->hbalock);
	clear_bit(fcf_index, phba->fcf.fcf_rr_bmask);

	lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
			"2791 Clear FCF (x%x) from roundrobin failover "
			"bmask\n", fcf_index);
}

 
static void
lpfc_mbx_cmpl_redisc_fcf_table(struct lpfc_hba *phba, LPFC_MBOXQ_t *mbox)
{
	struct lpfc_mbx_redisc_fcf_tbl *redisc_fcf;
	uint32_t shdr_status, shdr_add_status;

	redisc_fcf = &mbox->u.mqe.un.redisc_fcf_tbl;

	shdr_status = bf_get(lpfc_mbox_hdr_status,
			     &redisc_fcf->header.cfg_shdr.response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status,
			     &redisc_fcf->header.cfg_shdr.response);
	if (shdr_status || shdr_add_status) {
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP,
				"2746 Requesting for FCF rediscovery failed "
				"status x%x add_status x%x\n",
				shdr_status, shdr_add_status);
		if (phba->fcf.fcf_flag & FCF_ACVL_DISC) {
			spin_lock_irq(&phba->hbalock);
			phba->fcf.fcf_flag &= ~FCF_ACVL_DISC;
			spin_unlock_irq(&phba->hbalock);
			 
			lpfc_retry_pport_discovery(phba);
		} else {
			spin_lock_irq(&phba->hbalock);
			phba->fcf.fcf_flag &= ~FCF_DEAD_DISC;
			spin_unlock_irq(&phba->hbalock);
			 
			lpfc_sli4_fcf_dead_failthrough(phba);
		}
	} else {
		lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
				"2775 Start FCF rediscover quiescent timer\n");
		 
		lpfc_fcf_redisc_wait_start_timer(phba);
	}

	mempool_free(mbox, phba->mbox_mem_pool);
}

 
int
lpfc_sli4_redisc_fcf_table(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mbox;
	struct lpfc_mbx_redisc_fcf_tbl *redisc_fcf;
	int rc, length;

	 
	lpfc_cancel_all_vport_retry_delay_timer(phba);

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2745 Failed to allocate mbox for "
				"requesting FCF rediscover.\n");
		return -ENOMEM;
	}

	length = (sizeof(struct lpfc_mbx_redisc_fcf_tbl) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_REDISCOVER_FCF,
			 length, LPFC_SLI4_MBX_EMBED);

	redisc_fcf = &mbox->u.mqe.un.redisc_fcf_tbl;
	 
	bf_set(lpfc_mbx_redisc_fcf_count, redisc_fcf, 0);

	 
	mbox->vport = phba->pport;
	mbox->mbox_cmpl = lpfc_mbx_cmpl_redisc_fcf_table;
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);

	if (rc == MBX_NOT_FINISHED) {
		mempool_free(mbox, phba->mbox_mem_pool);
		return -EIO;
	}
	return 0;
}

 
void
lpfc_sli4_fcf_dead_failthrough(struct lpfc_hba *phba)
{
	uint32_t link_state;

	 
	link_state = phba->link_state;
	lpfc_linkdown(phba);
	phba->link_state = link_state;

	 
	lpfc_unregister_unused_fcf(phba);
}

 
static uint32_t
lpfc_sli_get_config_region23(struct lpfc_hba *phba, char *rgn23_data)
{
	LPFC_MBOXQ_t *pmb = NULL;
	MAILBOX_t *mb;
	uint32_t offset = 0;
	int rc;

	if (!rgn23_data)
		return 0;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2600 failed to allocate mailbox memory\n");
		return 0;
	}
	mb = &pmb->u.mb;

	do {
		lpfc_dump_mem(phba, pmb, offset, DMP_REGION_23);
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);

		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"2601 failed to read config "
					"region 23, rc 0x%x Status 0x%x\n",
					rc, mb->mbxStatus);
			mb->un.varDmp.word_cnt = 0;
		}
		 
		if (mb->un.varDmp.word_cnt == 0)
			break;

		if (mb->un.varDmp.word_cnt > DMP_RGN23_SIZE - offset)
			mb->un.varDmp.word_cnt = DMP_RGN23_SIZE - offset;

		lpfc_sli_pcimem_bcopy(((uint8_t *)mb) + DMP_RSP_OFFSET,
				       rgn23_data + offset,
				       mb->un.varDmp.word_cnt);
		offset += mb->un.varDmp.word_cnt;
	} while (mb->un.varDmp.word_cnt && offset < DMP_RGN23_SIZE);

	mempool_free(pmb, phba->mbox_mem_pool);
	return offset;
}

 
static uint32_t
lpfc_sli4_get_config_region23(struct lpfc_hba *phba, char *rgn23_data)
{
	LPFC_MBOXQ_t *mboxq = NULL;
	struct lpfc_dmabuf *mp = NULL;
	struct lpfc_mqe *mqe;
	uint32_t data_length = 0;
	int rc;

	if (!rgn23_data)
		return 0;

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3105 failed to allocate mailbox memory\n");
		return 0;
	}

	if (lpfc_sli4_dump_cfg_rg23(phba, mboxq))
		goto out;
	mqe = &mboxq->u.mqe;
	mp = (struct lpfc_dmabuf *)mboxq->ctx_buf;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	if (rc)
		goto out;
	data_length = mqe->un.mb_words[5];
	if (data_length == 0)
		goto out;
	if (data_length > DMP_RGN23_SIZE) {
		data_length = 0;
		goto out;
	}
	lpfc_sli_pcimem_bcopy((char *)mp->virt, rgn23_data, data_length);
out:
	lpfc_mbox_rsrc_cleanup(phba, mboxq, MBOX_THD_UNLOCKED);
	return data_length;
}

 
void
lpfc_sli_read_link_ste(struct lpfc_hba *phba)
{
	uint8_t *rgn23_data = NULL;
	uint32_t if_type, data_size, sub_tlv_len, tlv_offset;
	uint32_t offset = 0;

	 
	rgn23_data = kzalloc(DMP_RGN23_SIZE, GFP_KERNEL);
	if (!rgn23_data)
		goto out;

	if (phba->sli_rev < LPFC_SLI_REV4)
		data_size = lpfc_sli_get_config_region23(phba, rgn23_data);
	else {
		if_type = bf_get(lpfc_sli_intf_if_type,
				 &phba->sli4_hba.sli_intf);
		if (if_type == LPFC_SLI_INTF_IF_TYPE_0)
			goto out;
		data_size = lpfc_sli4_get_config_region23(phba, rgn23_data);
	}

	if (!data_size)
		goto out;

	 
	if (memcmp(&rgn23_data[offset], LPFC_REGION23_SIGNATURE, 4)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"2619 Config region 23 has bad signature\n");
			goto out;
	}
	offset += 4;

	 
	if (rgn23_data[offset] != LPFC_REGION23_VERSION) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
			"2620 Config region 23 has bad version\n");
		goto out;
	}
	offset += 4;

	 
	while (offset < data_size) {
		if (rgn23_data[offset] == LPFC_REGION23_LAST_REC)
			break;
		 
		if ((rgn23_data[offset] != DRIVER_SPECIFIC_TYPE) ||
		    (rgn23_data[offset + 2] != LINUX_DRIVER_ID) ||
		    (rgn23_data[offset + 3] != 0)) {
			offset += rgn23_data[offset + 1] * 4 + 4;
			continue;
		}

		 
		sub_tlv_len = rgn23_data[offset + 1] * 4;
		offset += 4;
		tlv_offset = 0;

		 
		while ((offset < data_size) &&
			(tlv_offset < sub_tlv_len)) {
			if (rgn23_data[offset] == LPFC_REGION23_LAST_REC) {
				offset += 4;
				tlv_offset += 4;
				break;
			}
			if (rgn23_data[offset] != PORT_STE_TYPE) {
				offset += rgn23_data[offset + 1] * 4 + 4;
				tlv_offset += rgn23_data[offset + 1] * 4 + 4;
				continue;
			}

			 
			if (!rgn23_data[offset + 2])
				phba->hba_flag |= LINK_DISABLED;

			goto out;
		}
	}

out:
	kfree(rgn23_data);
	return;
}

 
static void
lpfc_log_fw_write_cmpl(struct lpfc_hba *phba, u32 shdr_status,
		       u32 shdr_add_status, u32 shdr_add_status_2,
		       u32 shdr_change_status, u32 shdr_csf)
{
	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
			"4198 %s: flash_id x%02x, asic_rev x%02x, "
			"status x%02x, add_status x%02x, add_status_2 x%02x, "
			"change_status x%02x, csf %01x\n", __func__,
			phba->sli4_hba.flash_id, phba->sli4_hba.asic_rev,
			shdr_status, shdr_add_status, shdr_add_status_2,
			shdr_change_status, shdr_csf);

	if (shdr_add_status == LPFC_ADD_STATUS_INCOMPAT_OBJ) {
		switch (shdr_add_status_2) {
		case LPFC_ADD_STATUS_2_INCOMPAT_FLASH:
			lpfc_log_msg(phba, KERN_WARNING, LOG_MBOX | LOG_SLI,
				     "4199 Firmware write failed: "
				     "image incompatible with flash x%02x\n",
				     phba->sli4_hba.flash_id);
			break;
		case LPFC_ADD_STATUS_2_INCORRECT_ASIC:
			lpfc_log_msg(phba, KERN_WARNING, LOG_MBOX | LOG_SLI,
				     "4200 Firmware write failed: "
				     "image incompatible with ASIC "
				     "architecture x%02x\n",
				     phba->sli4_hba.asic_rev);
			break;
		default:
			lpfc_log_msg(phba, KERN_WARNING, LOG_MBOX | LOG_SLI,
				     "4210 Firmware write failed: "
				     "add_status_2 x%02x\n",
				     shdr_add_status_2);
			break;
		}
	} else if (!shdr_status && !shdr_add_status) {
		if (shdr_change_status == LPFC_CHANGE_STATUS_FW_RESET ||
		    shdr_change_status == LPFC_CHANGE_STATUS_PORT_MIGRATION) {
			if (shdr_csf)
				shdr_change_status =
						   LPFC_CHANGE_STATUS_PCI_RESET;
		}

		switch (shdr_change_status) {
		case (LPFC_CHANGE_STATUS_PHYS_DEV_RESET):
			lpfc_log_msg(phba, KERN_NOTICE, LOG_MBOX | LOG_SLI,
				     "3198 Firmware write complete: System "
				     "reboot required to instantiate\n");
			break;
		case (LPFC_CHANGE_STATUS_FW_RESET):
			lpfc_log_msg(phba, KERN_NOTICE, LOG_MBOX | LOG_SLI,
				     "3199 Firmware write complete: "
				     "Firmware reset required to "
				     "instantiate\n");
			break;
		case (LPFC_CHANGE_STATUS_PORT_MIGRATION):
			lpfc_log_msg(phba, KERN_NOTICE, LOG_MBOX | LOG_SLI,
				     "3200 Firmware write complete: Port "
				     "Migration or PCI Reset required to "
				     "instantiate\n");
			break;
		case (LPFC_CHANGE_STATUS_PCI_RESET):
			lpfc_log_msg(phba, KERN_NOTICE, LOG_MBOX | LOG_SLI,
				     "3201 Firmware write complete: PCI "
				     "Reset required to instantiate\n");
			break;
		default:
			break;
		}
	}
}

 
int
lpfc_wr_object(struct lpfc_hba *phba, struct list_head *dmabuf_list,
	       uint32_t size, uint32_t *offset)
{
	struct lpfc_mbx_wr_object *wr_object;
	LPFC_MBOXQ_t *mbox;
	int rc = 0, i = 0;
	int mbox_status = 0;
	uint32_t shdr_status, shdr_add_status, shdr_add_status_2;
	uint32_t shdr_change_status = 0, shdr_csf = 0;
	uint32_t mbox_tmo;
	struct lpfc_dmabuf *dmabuf;
	uint32_t written = 0;
	bool check_change_status = false;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			LPFC_MBOX_OPCODE_WRITE_OBJECT,
			sizeof(struct lpfc_mbx_wr_object) -
			sizeof(struct lpfc_sli4_cfg_mhdr), LPFC_SLI4_MBX_EMBED);

	wr_object = (struct lpfc_mbx_wr_object *)&mbox->u.mqe.un.wr_object;
	wr_object->u.request.write_offset = *offset;
	sprintf((uint8_t *)wr_object->u.request.object_name, "/");
	wr_object->u.request.object_name[0] =
		cpu_to_le32(wr_object->u.request.object_name[0]);
	bf_set(lpfc_wr_object_eof, &wr_object->u.request, 0);
	list_for_each_entry(dmabuf, dmabuf_list, list) {
		if (i >= LPFC_MBX_WR_CONFIG_MAX_BDE || written >= size)
			break;
		wr_object->u.request.bde[i].addrLow = putPaddrLow(dmabuf->phys);
		wr_object->u.request.bde[i].addrHigh =
			putPaddrHigh(dmabuf->phys);
		if (written + SLI4_PAGE_SIZE >= size) {
			wr_object->u.request.bde[i].tus.f.bdeSize =
				(size - written);
			written += (size - written);
			bf_set(lpfc_wr_object_eof, &wr_object->u.request, 1);
			bf_set(lpfc_wr_object_eas, &wr_object->u.request, 1);
			check_change_status = true;
		} else {
			wr_object->u.request.bde[i].tus.f.bdeSize =
				SLI4_PAGE_SIZE;
			written += SLI4_PAGE_SIZE;
		}
		i++;
	}
	wr_object->u.request.bde_count = i;
	bf_set(lpfc_wr_object_write_length, &wr_object->u.request, written);
	if (!phba->sli4_hba.intr_enable)
		mbox_status = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		mbox_status = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}

	 
	rc = mbox_status;

	 
	shdr_status = bf_get(lpfc_mbox_hdr_status,
			     &wr_object->header.cfg_shdr.response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status,
				 &wr_object->header.cfg_shdr.response);
	shdr_add_status_2 = bf_get(lpfc_mbox_hdr_add_status_2,
				   &wr_object->header.cfg_shdr.response);
	if (check_change_status) {
		shdr_change_status = bf_get(lpfc_wr_object_change_status,
					    &wr_object->u.response);
		shdr_csf = bf_get(lpfc_wr_object_csf,
				  &wr_object->u.response);
	}

	if (shdr_status || shdr_add_status || shdr_add_status_2 || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"3025 Write Object mailbox failed with "
				"status x%x add_status x%x, add_status_2 x%x, "
				"mbx status x%x\n",
				shdr_status, shdr_add_status, shdr_add_status_2,
				rc);
		rc = -ENXIO;
		*offset = shdr_add_status;
	} else {
		*offset += wr_object->u.response.actual_write_length;
	}

	if (rc || check_change_status)
		lpfc_log_fw_write_cmpl(phba, shdr_status, shdr_add_status,
				       shdr_add_status_2, shdr_change_status,
				       shdr_csf);

	if (!phba->sli4_hba.intr_enable)
		mempool_free(mbox, phba->mbox_mem_pool);
	else if (mbox_status != MBX_TIMEOUT)
		mempool_free(mbox, phba->mbox_mem_pool);

	return rc;
}

 
void
lpfc_cleanup_pending_mbox(struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;
	LPFC_MBOXQ_t *mb, *nextmb;
	struct lpfc_nodelist *ndlp;
	struct lpfc_nodelist *act_mbx_ndlp = NULL;
	LIST_HEAD(mbox_cmd_list);
	uint8_t restart_loop;

	 
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(mb, nextmb, &phba->sli.mboxq, list) {
		if (mb->vport != vport)
			continue;

		if ((mb->u.mb.mbxCommand != MBX_REG_LOGIN64) &&
			(mb->u.mb.mbxCommand != MBX_REG_VPI))
			continue;

		list_move_tail(&mb->list, &mbox_cmd_list);
	}
	 
	mb = phba->sli.mbox_active;
	if (mb && (mb->vport == vport)) {
		if ((mb->u.mb.mbxCommand == MBX_REG_LOGIN64) ||
			(mb->u.mb.mbxCommand == MBX_REG_VPI))
			mb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		if (mb->u.mb.mbxCommand == MBX_REG_LOGIN64) {
			act_mbx_ndlp = (struct lpfc_nodelist *)mb->ctx_ndlp;

			 
			act_mbx_ndlp = lpfc_nlp_get(act_mbx_ndlp);

			 
			mb->mbox_flag |= LPFC_MBX_IMED_UNREG;
		}
	}
	 
	do {
		restart_loop = 0;
		list_for_each_entry(mb, &phba->sli.mboxq_cmpl, list) {
			 
			if ((mb->vport != vport) ||
				(mb->mbox_flag & LPFC_MBX_IMED_UNREG))
				continue;

			if ((mb->u.mb.mbxCommand != MBX_REG_LOGIN64) &&
				(mb->u.mb.mbxCommand != MBX_REG_VPI))
				continue;

			mb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
			if (mb->u.mb.mbxCommand == MBX_REG_LOGIN64) {
				ndlp = (struct lpfc_nodelist *)mb->ctx_ndlp;
				 
				mb->mbox_flag |= LPFC_MBX_IMED_UNREG;
				restart_loop = 1;
				spin_unlock_irq(&phba->hbalock);
				spin_lock(&ndlp->lock);
				ndlp->nlp_flag &= ~NLP_IGNR_REG_CMPL;
				spin_unlock(&ndlp->lock);
				spin_lock_irq(&phba->hbalock);
				break;
			}
		}
	} while (restart_loop);

	spin_unlock_irq(&phba->hbalock);

	 
	while (!list_empty(&mbox_cmd_list)) {
		list_remove_head(&mbox_cmd_list, mb, LPFC_MBOXQ_t, list);
		if (mb->u.mb.mbxCommand == MBX_REG_LOGIN64) {
			ndlp = (struct lpfc_nodelist *)mb->ctx_ndlp;
			mb->ctx_ndlp = NULL;
			if (ndlp) {
				spin_lock(&ndlp->lock);
				ndlp->nlp_flag &= ~NLP_IGNR_REG_CMPL;
				spin_unlock(&ndlp->lock);
				lpfc_nlp_put(ndlp);
			}
		}
		lpfc_mbox_rsrc_cleanup(phba, mb, MBOX_THD_UNLOCKED);
	}

	 
	if (act_mbx_ndlp) {
		spin_lock(&act_mbx_ndlp->lock);
		act_mbx_ndlp->nlp_flag &= ~NLP_IGNR_REG_CMPL;
		spin_unlock(&act_mbx_ndlp->lock);
		lpfc_nlp_put(act_mbx_ndlp);
	}
}

 

uint32_t
lpfc_drain_txq(struct lpfc_hba *phba)
{
	LIST_HEAD(completions);
	struct lpfc_sli_ring *pring;
	struct lpfc_iocbq *piocbq = NULL;
	unsigned long iflags = 0;
	char *fail_msg = NULL;
	uint32_t txq_cnt = 0;
	struct lpfc_queue *wq;
	int ret = 0;

	if (phba->link_flag & LS_MDS_LOOPBACK) {
		 
		wq = phba->sli4_hba.hdwq[0].io_wq;
		if (unlikely(!wq))
			return 0;
		pring = wq->pring;
	} else {
		wq = phba->sli4_hba.els_wq;
		if (unlikely(!wq))
			return 0;
		pring = lpfc_phba_elsring(phba);
	}

	if (unlikely(!pring) || list_empty(&pring->txq))
		return 0;

	spin_lock_irqsave(&pring->ring_lock, iflags);
	list_for_each_entry(piocbq, &pring->txq, list) {
		txq_cnt++;
	}

	if (txq_cnt > pring->txq_max)
		pring->txq_max = txq_cnt;

	spin_unlock_irqrestore(&pring->ring_lock, iflags);

	while (!list_empty(&pring->txq)) {
		spin_lock_irqsave(&pring->ring_lock, iflags);

		piocbq = lpfc_sli_ringtx_get(phba, pring);
		if (!piocbq) {
			spin_unlock_irqrestore(&pring->ring_lock, iflags);
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"2823 txq empty and txq_cnt is %d\n ",
				txq_cnt);
			break;
		}
		txq_cnt--;

		ret = __lpfc_sli_issue_iocb(phba, pring->ringno, piocbq, 0);

		if (ret && ret != IOCB_BUSY) {
			fail_msg = " - Cannot send IO ";
			piocbq->cmd_flag &= ~LPFC_DRIVER_ABORTED;
		}
		if (fail_msg) {
			piocbq->cmd_flag |= LPFC_DRIVER_ABORTED;
			 
			lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
					"2822 IOCB failed %s iotag 0x%x "
					"xri 0x%x %d flg x%x\n",
					fail_msg, piocbq->iotag,
					piocbq->sli4_xritag, ret,
					piocbq->cmd_flag);
			list_add_tail(&piocbq->list, &completions);
			fail_msg = NULL;
		}
		spin_unlock_irqrestore(&pring->ring_lock, iflags);
		if (txq_cnt == 0 || ret == IOCB_BUSY)
			break;
	}
	 
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);

	return txq_cnt;
}

 
static uint16_t
lpfc_wqe_bpl2sgl(struct lpfc_hba *phba, struct lpfc_iocbq *pwqeq,
		 struct lpfc_sglq *sglq)
{
	uint16_t xritag = NO_XRI;
	struct ulp_bde64 *bpl = NULL;
	struct ulp_bde64 bde;
	struct sli4_sge *sgl  = NULL;
	struct lpfc_dmabuf *dmabuf;
	union lpfc_wqe128 *wqe;
	int numBdes = 0;
	int i = 0;
	uint32_t offset = 0;  
	int inbound = 0;  
	uint32_t cmd;

	if (!pwqeq || !sglq)
		return xritag;

	sgl  = (struct sli4_sge *)sglq->sgl;
	wqe = &pwqeq->wqe;
	pwqeq->iocb.ulpIoTag = pwqeq->iotag;

	cmd = bf_get(wqe_cmnd, &wqe->generic.wqe_com);
	if (cmd == CMD_XMIT_BLS_RSP64_WQE)
		return sglq->sli4_xritag;
	numBdes = pwqeq->num_bdes;
	if (numBdes) {
		 
		if (pwqeq->bpl_dmabuf)
			dmabuf = pwqeq->bpl_dmabuf;
		else
			return xritag;

		bpl  = (struct ulp_bde64 *)dmabuf->virt;
		if (!bpl)
			return xritag;

		for (i = 0; i < numBdes; i++) {
			 
			sgl->addr_hi = bpl->addrHigh;
			sgl->addr_lo = bpl->addrLow;

			sgl->word2 = le32_to_cpu(sgl->word2);
			if ((i+1) == numBdes)
				bf_set(lpfc_sli4_sge_last, sgl, 1);
			else
				bf_set(lpfc_sli4_sge_last, sgl, 0);
			 
			bde.tus.w = le32_to_cpu(bpl->tus.w);
			sgl->sge_len = cpu_to_le32(bde.tus.f.bdeSize);
			 
			switch (cmd) {
			case CMD_GEN_REQUEST64_WQE:
				 
				if (bpl->tus.f.bdeFlags == BUFF_TYPE_BDE_64I)
					inbound++;
				 
				if (inbound == 1)
					offset = 0;
				bf_set(lpfc_sli4_sge_offset, sgl, offset);
				bf_set(lpfc_sli4_sge_type, sgl,
					LPFC_SGE_TYPE_DATA);
				offset += bde.tus.f.bdeSize;
				break;
			case CMD_FCP_TRSP64_WQE:
				bf_set(lpfc_sli4_sge_offset, sgl, 0);
				bf_set(lpfc_sli4_sge_type, sgl,
					LPFC_SGE_TYPE_DATA);
				break;
			case CMD_FCP_TSEND64_WQE:
			case CMD_FCP_TRECEIVE64_WQE:
				bf_set(lpfc_sli4_sge_type, sgl,
					bpl->tus.f.bdeFlags);
				if (i < 3)
					offset = 0;
				else
					offset += bde.tus.f.bdeSize;
				bf_set(lpfc_sli4_sge_offset, sgl, offset);
				break;
			}
			sgl->word2 = cpu_to_le32(sgl->word2);
			bpl++;
			sgl++;
		}
	} else if (wqe->gen_req.bde.tus.f.bdeFlags == BUFF_TYPE_BDE_64) {
		 
		sgl->addr_hi = cpu_to_le32(wqe->gen_req.bde.addrHigh);
		sgl->addr_lo = cpu_to_le32(wqe->gen_req.bde.addrLow);
		sgl->word2 = le32_to_cpu(sgl->word2);
		bf_set(lpfc_sli4_sge_last, sgl, 1);
		sgl->word2 = cpu_to_le32(sgl->word2);
		sgl->sge_len = cpu_to_le32(wqe->gen_req.bde.tus.f.bdeSize);
	}
	return sglq->sli4_xritag;
}

 
int
lpfc_sli4_issue_wqe(struct lpfc_hba *phba, struct lpfc_sli4_hdw_queue *qp,
		    struct lpfc_iocbq *pwqe)
{
	union lpfc_wqe128 *wqe = &pwqe->wqe;
	struct lpfc_async_xchg_ctx *ctxp;
	struct lpfc_queue *wq;
	struct lpfc_sglq *sglq;
	struct lpfc_sli_ring *pring;
	unsigned long iflags;
	uint32_t ret = 0;

	 
	if (pwqe->cmd_flag & LPFC_IO_NVME_LS) {
		pring =  phba->sli4_hba.nvmels_wq->pring;
		lpfc_qp_spin_lock_irqsave(&pring->ring_lock, iflags,
					  qp, wq_access);
		sglq = __lpfc_sli_get_els_sglq(phba, pwqe);
		if (!sglq) {
			spin_unlock_irqrestore(&pring->ring_lock, iflags);
			return WQE_BUSY;
		}
		pwqe->sli4_lxritag = sglq->sli4_lxritag;
		pwqe->sli4_xritag = sglq->sli4_xritag;
		if (lpfc_wqe_bpl2sgl(phba, pwqe, sglq) == NO_XRI) {
			spin_unlock_irqrestore(&pring->ring_lock, iflags);
			return WQE_ERROR;
		}
		bf_set(wqe_xri_tag, &pwqe->wqe.xmit_bls_rsp.wqe_com,
		       pwqe->sli4_xritag);
		ret = lpfc_sli4_wq_put(phba->sli4_hba.nvmels_wq, wqe);
		if (ret) {
			spin_unlock_irqrestore(&pring->ring_lock, iflags);
			return ret;
		}

		lpfc_sli_ringtxcmpl_put(phba, pring, pwqe);
		spin_unlock_irqrestore(&pring->ring_lock, iflags);

		lpfc_sli4_poll_eq(qp->hba_eq);
		return 0;
	}

	 
	if (pwqe->cmd_flag & (LPFC_IO_NVME | LPFC_IO_FCP | LPFC_IO_CMF)) {
		 
		wq = qp->io_wq;
		pring = wq->pring;

		bf_set(wqe_cqid, &wqe->generic.wqe_com, qp->io_cq_map);

		lpfc_qp_spin_lock_irqsave(&pring->ring_lock, iflags,
					  qp, wq_access);
		ret = lpfc_sli4_wq_put(wq, wqe);
		if (ret) {
			spin_unlock_irqrestore(&pring->ring_lock, iflags);
			return ret;
		}
		lpfc_sli_ringtxcmpl_put(phba, pring, pwqe);
		spin_unlock_irqrestore(&pring->ring_lock, iflags);

		lpfc_sli4_poll_eq(qp->hba_eq);
		return 0;
	}

	 
	if (pwqe->cmd_flag & LPFC_IO_NVMET) {
		 
		wq = qp->io_wq;
		pring = wq->pring;

		ctxp = pwqe->context_un.axchg;
		sglq = ctxp->ctxbuf->sglq;
		if (pwqe->sli4_xritag ==  NO_XRI) {
			pwqe->sli4_lxritag = sglq->sli4_lxritag;
			pwqe->sli4_xritag = sglq->sli4_xritag;
		}
		bf_set(wqe_xri_tag, &pwqe->wqe.xmit_bls_rsp.wqe_com,
		       pwqe->sli4_xritag);
		bf_set(wqe_cqid, &wqe->generic.wqe_com, qp->io_cq_map);

		lpfc_qp_spin_lock_irqsave(&pring->ring_lock, iflags,
					  qp, wq_access);
		ret = lpfc_sli4_wq_put(wq, wqe);
		if (ret) {
			spin_unlock_irqrestore(&pring->ring_lock, iflags);
			return ret;
		}
		lpfc_sli_ringtxcmpl_put(phba, pring, pwqe);
		spin_unlock_irqrestore(&pring->ring_lock, iflags);

		lpfc_sli4_poll_eq(qp->hba_eq);
		return 0;
	}
	return WQE_ERROR;
}

 

int
lpfc_sli4_issue_abort_iotag(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
			    void *cmpl)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct lpfc_iocbq *abtsiocb = NULL;
	union lpfc_wqe128 *abtswqe;
	struct lpfc_io_buf *lpfc_cmd;
	int retval = IOCB_ERROR;
	u16 xritag = cmdiocb->sli4_xritag;

	 

	abtsiocb = __lpfc_sli_get_iocbq(phba);
	if (!abtsiocb)
		return WQE_NORESOURCE;

	 
	cmdiocb->cmd_flag |= LPFC_DRIVER_ABORTED;

	abtswqe = &abtsiocb->wqe;
	memset(abtswqe, 0, sizeof(*abtswqe));

	if (!lpfc_is_link_up(phba) || (phba->link_flag & LS_EXTERNAL_LOOPBACK))
		bf_set(abort_cmd_ia, &abtswqe->abort_cmd, 1);
	bf_set(abort_cmd_criteria, &abtswqe->abort_cmd, T_XRI_TAG);
	abtswqe->abort_cmd.rsrvd5 = 0;
	abtswqe->abort_cmd.wqe_com.abort_tag = xritag;
	bf_set(wqe_reqtag, &abtswqe->abort_cmd.wqe_com, abtsiocb->iotag);
	bf_set(wqe_cmnd, &abtswqe->abort_cmd.wqe_com, CMD_ABORT_XRI_CX);
	bf_set(wqe_xri_tag, &abtswqe->generic.wqe_com, 0);
	bf_set(wqe_qosd, &abtswqe->abort_cmd.wqe_com, 1);
	bf_set(wqe_lenloc, &abtswqe->abort_cmd.wqe_com, LPFC_WQE_LENLOC_NONE);
	bf_set(wqe_cmd_type, &abtswqe->abort_cmd.wqe_com, OTHER_COMMAND);

	 
	abtsiocb->hba_wqidx = cmdiocb->hba_wqidx;
	abtsiocb->cmd_flag |= LPFC_USE_FCPWQIDX;
	if (cmdiocb->cmd_flag & LPFC_IO_FCP)
		abtsiocb->cmd_flag |= LPFC_IO_FCP;
	if (cmdiocb->cmd_flag & LPFC_IO_NVME)
		abtsiocb->cmd_flag |= LPFC_IO_NVME;
	if (cmdiocb->cmd_flag & LPFC_IO_FOF)
		abtsiocb->cmd_flag |= LPFC_IO_FOF;
	abtsiocb->vport = vport;
	abtsiocb->cmd_cmpl = cmpl;

	lpfc_cmd = container_of(cmdiocb, struct lpfc_io_buf, cur_iocbq);
	retval = lpfc_sli4_issue_wqe(phba, lpfc_cmd->hdwq, abtsiocb);

	lpfc_printf_vlog(vport, KERN_INFO, LOG_SLI | LOG_NVME_ABTS | LOG_FCP,
			 "0359 Abort xri x%x, original iotag x%x, "
			 "abort cmd iotag x%x retval x%x\n",
			 xritag, cmdiocb->iotag, abtsiocb->iotag, retval);

	if (retval) {
		cmdiocb->cmd_flag &= ~LPFC_DRIVER_ABORTED;
		__lpfc_sli_release_iocbq(phba, abtsiocb);
	}

	return retval;
}

#ifdef LPFC_MXP_STAT
 
void lpfc_snapshot_mxp(struct lpfc_hba *phba, u32 hwqid)
{
	struct lpfc_sli4_hdw_queue *qp;
	struct lpfc_multixri_pool *multixri_pool;
	struct lpfc_pvt_pool *pvt_pool;
	struct lpfc_pbl_pool *pbl_pool;
	u32 txcmplq_cnt;

	qp = &phba->sli4_hba.hdwq[hwqid];
	multixri_pool = qp->p_multixri_pool;
	if (!multixri_pool)
		return;

	if (multixri_pool->stat_snapshot_taken == LPFC_MXP_SNAPSHOT_TAKEN) {
		pvt_pool = &qp->p_multixri_pool->pvt_pool;
		pbl_pool = &qp->p_multixri_pool->pbl_pool;
		txcmplq_cnt = qp->io_wq->pring->txcmplq_cnt;

		multixri_pool->stat_pbl_count = pbl_pool->count;
		multixri_pool->stat_pvt_count = pvt_pool->count;
		multixri_pool->stat_busy_count = txcmplq_cnt;
	}

	multixri_pool->stat_snapshot_taken++;
}
#endif

 
void lpfc_adjust_pvt_pool_count(struct lpfc_hba *phba, u32 hwqid)
{
	struct lpfc_multixri_pool *multixri_pool;
	u32 io_req_count;
	u32 prev_io_req_count;

	multixri_pool = phba->sli4_hba.hdwq[hwqid].p_multixri_pool;
	if (!multixri_pool)
		return;
	io_req_count = multixri_pool->io_req_count;
	prev_io_req_count = multixri_pool->prev_io_req_count;

	if (prev_io_req_count != io_req_count) {
		 
		multixri_pool->prev_io_req_count = io_req_count;
	} else {
		 
		lpfc_move_xri_pvt_to_pbl(phba, hwqid);
	}
}

 
void lpfc_adjust_high_watermark(struct lpfc_hba *phba, u32 hwqid)
{
	u32 new_watermark;
	u32 watermark_max;
	u32 watermark_min;
	u32 xri_limit;
	u32 txcmplq_cnt;
	u32 abts_io_bufs;
	struct lpfc_multixri_pool *multixri_pool;
	struct lpfc_sli4_hdw_queue *qp;

	qp = &phba->sli4_hba.hdwq[hwqid];
	multixri_pool = qp->p_multixri_pool;
	if (!multixri_pool)
		return;
	xri_limit = multixri_pool->xri_limit;

	watermark_max = xri_limit;
	watermark_min = xri_limit / 2;

	txcmplq_cnt = qp->io_wq->pring->txcmplq_cnt;
	abts_io_bufs = qp->abts_scsi_io_bufs;
	abts_io_bufs += qp->abts_nvme_io_bufs;

	new_watermark = txcmplq_cnt + abts_io_bufs;
	new_watermark = min(watermark_max, new_watermark);
	new_watermark = max(watermark_min, new_watermark);
	multixri_pool->pvt_pool.high_watermark = new_watermark;

#ifdef LPFC_MXP_STAT
	multixri_pool->stat_max_hwm = max(multixri_pool->stat_max_hwm,
					  new_watermark);
#endif
}

 
void lpfc_move_xri_pvt_to_pbl(struct lpfc_hba *phba, u32 hwqid)
{
	struct lpfc_pbl_pool *pbl_pool;
	struct lpfc_pvt_pool *pvt_pool;
	struct lpfc_sli4_hdw_queue *qp;
	struct lpfc_io_buf *lpfc_ncmd;
	struct lpfc_io_buf *lpfc_ncmd_next;
	unsigned long iflag;
	struct list_head tmp_list;
	u32 tmp_count;

	qp = &phba->sli4_hba.hdwq[hwqid];
	pbl_pool = &qp->p_multixri_pool->pbl_pool;
	pvt_pool = &qp->p_multixri_pool->pvt_pool;
	tmp_count = 0;

	lpfc_qp_spin_lock_irqsave(&pbl_pool->lock, iflag, qp, mv_to_pub_pool);
	lpfc_qp_spin_lock(&pvt_pool->lock, qp, mv_from_pvt_pool);

	if (pvt_pool->count > pvt_pool->low_watermark) {
		 

		 
		INIT_LIST_HEAD(&tmp_list);
		list_for_each_entry_safe(lpfc_ncmd, lpfc_ncmd_next,
					 &pvt_pool->list, list) {
			list_move_tail(&lpfc_ncmd->list, &tmp_list);
			tmp_count++;
			if (tmp_count >= pvt_pool->low_watermark)
				break;
		}

		 
		list_splice_init(&pvt_pool->list, &pbl_pool->list);

		 
		list_splice(&tmp_list, &pvt_pool->list);

		pbl_pool->count += (pvt_pool->count - tmp_count);
		pvt_pool->count = tmp_count;
	} else {
		 
		list_splice_init(&pvt_pool->list, &pbl_pool->list);
		pbl_pool->count += pvt_pool->count;
		pvt_pool->count = 0;
	}

	spin_unlock(&pvt_pool->lock);
	spin_unlock_irqrestore(&pbl_pool->lock, iflag);
}

 
static bool
_lpfc_move_xri_pbl_to_pvt(struct lpfc_hba *phba, struct lpfc_sli4_hdw_queue *qp,
			  struct lpfc_pbl_pool *pbl_pool,
			  struct lpfc_pvt_pool *pvt_pool, u32 count)
{
	struct lpfc_io_buf *lpfc_ncmd;
	struct lpfc_io_buf *lpfc_ncmd_next;
	unsigned long iflag;
	int ret;

	ret = spin_trylock_irqsave(&pbl_pool->lock, iflag);
	if (ret) {
		if (pbl_pool->count) {
			 
			lpfc_qp_spin_lock(&pvt_pool->lock, qp, mv_to_pvt_pool);
			list_for_each_entry_safe(lpfc_ncmd,
						 lpfc_ncmd_next,
						 &pbl_pool->list,
						 list) {
				list_move_tail(&lpfc_ncmd->list,
					       &pvt_pool->list);
				pvt_pool->count++;
				pbl_pool->count--;
				count--;
				if (count == 0)
					break;
			}

			spin_unlock(&pvt_pool->lock);
			spin_unlock_irqrestore(&pbl_pool->lock, iflag);
			return true;
		}
		spin_unlock_irqrestore(&pbl_pool->lock, iflag);
	}

	return false;
}

 
void lpfc_move_xri_pbl_to_pvt(struct lpfc_hba *phba, u32 hwqid, u32 count)
{
	struct lpfc_multixri_pool *multixri_pool;
	struct lpfc_multixri_pool *next_multixri_pool;
	struct lpfc_pvt_pool *pvt_pool;
	struct lpfc_pbl_pool *pbl_pool;
	struct lpfc_sli4_hdw_queue *qp;
	u32 next_hwqid;
	u32 hwq_count;
	int ret;

	qp = &phba->sli4_hba.hdwq[hwqid];
	multixri_pool = qp->p_multixri_pool;
	pvt_pool = &multixri_pool->pvt_pool;
	pbl_pool = &multixri_pool->pbl_pool;

	 
	ret = _lpfc_move_xri_pbl_to_pvt(phba, qp, pbl_pool, pvt_pool, count);
	if (ret) {
#ifdef LPFC_MXP_STAT
		multixri_pool->local_pbl_hit_count++;
#endif
		return;
	}

	hwq_count = phba->cfg_hdw_queue;

	 
	next_hwqid = multixri_pool->rrb_next_hwqid;

	do {
		 
		next_hwqid = (next_hwqid + 1) % hwq_count;

		next_multixri_pool =
			phba->sli4_hba.hdwq[next_hwqid].p_multixri_pool;
		pbl_pool = &next_multixri_pool->pbl_pool;

		 
		ret = _lpfc_move_xri_pbl_to_pvt(
			phba, qp, pbl_pool, pvt_pool, count);

		 
	} while (!ret && next_hwqid != multixri_pool->rrb_next_hwqid);

	 
	multixri_pool->rrb_next_hwqid = next_hwqid;

	if (!ret) {
		 
		multixri_pool->pbl_empty_count++;
	}

#ifdef LPFC_MXP_STAT
	if (ret) {
		if (next_hwqid == hwqid)
			multixri_pool->local_pbl_hit_count++;
		else
			multixri_pool->other_pbl_hit_count++;
	}
#endif
}

 
void lpfc_keep_pvt_pool_above_lowwm(struct lpfc_hba *phba, u32 hwqid)
{
	struct lpfc_multixri_pool *multixri_pool;
	struct lpfc_pvt_pool *pvt_pool;

	multixri_pool = phba->sli4_hba.hdwq[hwqid].p_multixri_pool;
	pvt_pool = &multixri_pool->pvt_pool;

	if (pvt_pool->count < pvt_pool->low_watermark)
		lpfc_move_xri_pbl_to_pvt(phba, hwqid, XRI_BATCH);
}

 
void lpfc_release_io_buf(struct lpfc_hba *phba, struct lpfc_io_buf *lpfc_ncmd,
			 struct lpfc_sli4_hdw_queue *qp)
{
	unsigned long iflag;
	struct lpfc_pbl_pool *pbl_pool;
	struct lpfc_pvt_pool *pvt_pool;
	struct lpfc_epd_pool *epd_pool;
	u32 txcmplq_cnt;
	u32 xri_owned;
	u32 xri_limit;
	u32 abts_io_bufs;

	 
	lpfc_ncmd->nvmeCmd = NULL;
	lpfc_ncmd->cur_iocbq.cmd_cmpl = NULL;

	if (phba->cfg_xpsgl && !phba->nvmet_support &&
	    !list_empty(&lpfc_ncmd->dma_sgl_xtra_list))
		lpfc_put_sgl_per_hdwq(phba, lpfc_ncmd);

	if (!list_empty(&lpfc_ncmd->dma_cmd_rsp_list))
		lpfc_put_cmd_rsp_buf_per_hdwq(phba, lpfc_ncmd);

	if (phba->cfg_xri_rebalancing) {
		if (lpfc_ncmd->expedite) {
			 
			epd_pool = &phba->epd_pool;
			spin_lock_irqsave(&epd_pool->lock, iflag);
			list_add_tail(&lpfc_ncmd->list, &epd_pool->list);
			epd_pool->count++;
			spin_unlock_irqrestore(&epd_pool->lock, iflag);
			return;
		}

		 
		if (!qp->p_multixri_pool)
			return;

		pbl_pool = &qp->p_multixri_pool->pbl_pool;
		pvt_pool = &qp->p_multixri_pool->pvt_pool;

		txcmplq_cnt = qp->io_wq->pring->txcmplq_cnt;
		abts_io_bufs = qp->abts_scsi_io_bufs;
		abts_io_bufs += qp->abts_nvme_io_bufs;

		xri_owned = pvt_pool->count + txcmplq_cnt + abts_io_bufs;
		xri_limit = qp->p_multixri_pool->xri_limit;

#ifdef LPFC_MXP_STAT
		if (xri_owned <= xri_limit)
			qp->p_multixri_pool->below_limit_count++;
		else
			qp->p_multixri_pool->above_limit_count++;
#endif

		 
		if ((pvt_pool->count < pvt_pool->low_watermark) ||
		    (xri_owned < xri_limit &&
		     pvt_pool->count < pvt_pool->high_watermark)) {
			lpfc_qp_spin_lock_irqsave(&pvt_pool->lock, iflag,
						  qp, free_pvt_pool);
			list_add_tail(&lpfc_ncmd->list,
				      &pvt_pool->list);
			pvt_pool->count++;
			spin_unlock_irqrestore(&pvt_pool->lock, iflag);
		} else {
			lpfc_qp_spin_lock_irqsave(&pbl_pool->lock, iflag,
						  qp, free_pub_pool);
			list_add_tail(&lpfc_ncmd->list,
				      &pbl_pool->list);
			pbl_pool->count++;
			spin_unlock_irqrestore(&pbl_pool->lock, iflag);
		}
	} else {
		lpfc_qp_spin_lock_irqsave(&qp->io_buf_list_put_lock, iflag,
					  qp, free_xri);
		list_add_tail(&lpfc_ncmd->list,
			      &qp->lpfc_io_buf_list_put);
		qp->put_io_bufs++;
		spin_unlock_irqrestore(&qp->io_buf_list_put_lock,
				       iflag);
	}
}

 
static struct lpfc_io_buf *
lpfc_get_io_buf_from_private_pool(struct lpfc_hba *phba,
				  struct lpfc_sli4_hdw_queue *qp,
				  struct lpfc_pvt_pool *pvt_pool,
				  struct lpfc_nodelist *ndlp)
{
	struct lpfc_io_buf *lpfc_ncmd;
	struct lpfc_io_buf *lpfc_ncmd_next;
	unsigned long iflag;

	lpfc_qp_spin_lock_irqsave(&pvt_pool->lock, iflag, qp, alloc_pvt_pool);
	list_for_each_entry_safe(lpfc_ncmd, lpfc_ncmd_next,
				 &pvt_pool->list, list) {
		if (lpfc_test_rrq_active(
			phba, ndlp, lpfc_ncmd->cur_iocbq.sli4_lxritag))
			continue;
		list_del(&lpfc_ncmd->list);
		pvt_pool->count--;
		spin_unlock_irqrestore(&pvt_pool->lock, iflag);
		return lpfc_ncmd;
	}
	spin_unlock_irqrestore(&pvt_pool->lock, iflag);

	return NULL;
}

 
static struct lpfc_io_buf *
lpfc_get_io_buf_from_expedite_pool(struct lpfc_hba *phba)
{
	struct lpfc_io_buf *lpfc_ncmd = NULL, *iter;
	struct lpfc_io_buf *lpfc_ncmd_next;
	unsigned long iflag;
	struct lpfc_epd_pool *epd_pool;

	epd_pool = &phba->epd_pool;

	spin_lock_irqsave(&epd_pool->lock, iflag);
	if (epd_pool->count > 0) {
		list_for_each_entry_safe(iter, lpfc_ncmd_next,
					 &epd_pool->list, list) {
			list_del(&iter->list);
			epd_pool->count--;
			lpfc_ncmd = iter;
			break;
		}
	}
	spin_unlock_irqrestore(&epd_pool->lock, iflag);

	return lpfc_ncmd;
}

 
static struct lpfc_io_buf *
lpfc_get_io_buf_from_multixri_pools(struct lpfc_hba *phba,
				    struct lpfc_nodelist *ndlp,
				    int hwqid, int expedite)
{
	struct lpfc_sli4_hdw_queue *qp;
	struct lpfc_multixri_pool *multixri_pool;
	struct lpfc_pvt_pool *pvt_pool;
	struct lpfc_io_buf *lpfc_ncmd;

	qp = &phba->sli4_hba.hdwq[hwqid];
	lpfc_ncmd = NULL;
	if (!qp) {
		lpfc_printf_log(phba, KERN_INFO,
				LOG_SLI | LOG_NVME_ABTS | LOG_FCP,
				"5556 NULL qp for hwqid  x%x\n", hwqid);
		return lpfc_ncmd;
	}
	multixri_pool = qp->p_multixri_pool;
	if (!multixri_pool) {
		lpfc_printf_log(phba, KERN_INFO,
				LOG_SLI | LOG_NVME_ABTS | LOG_FCP,
				"5557 NULL multixri for hwqid  x%x\n", hwqid);
		return lpfc_ncmd;
	}
	pvt_pool = &multixri_pool->pvt_pool;
	if (!pvt_pool) {
		lpfc_printf_log(phba, KERN_INFO,
				LOG_SLI | LOG_NVME_ABTS | LOG_FCP,
				"5558 NULL pvt_pool for hwqid  x%x\n", hwqid);
		return lpfc_ncmd;
	}
	multixri_pool->io_req_count++;

	 
	if (pvt_pool->count == 0)
		lpfc_move_xri_pbl_to_pvt(phba, hwqid, XRI_BATCH);

	 
	lpfc_ncmd = lpfc_get_io_buf_from_private_pool(phba, qp, pvt_pool, ndlp);

	if (lpfc_ncmd) {
		lpfc_ncmd->hdwq = qp;
		lpfc_ncmd->hdwq_no = hwqid;
	} else if (expedite) {
		 
		lpfc_ncmd = lpfc_get_io_buf_from_expedite_pool(phba);
	}

	return lpfc_ncmd;
}

static inline struct lpfc_io_buf *
lpfc_io_buf(struct lpfc_hba *phba, struct lpfc_nodelist *ndlp, int idx)
{
	struct lpfc_sli4_hdw_queue *qp;
	struct lpfc_io_buf *lpfc_cmd, *lpfc_cmd_next;

	qp = &phba->sli4_hba.hdwq[idx];
	list_for_each_entry_safe(lpfc_cmd, lpfc_cmd_next,
				 &qp->lpfc_io_buf_list_get, list) {
		if (lpfc_test_rrq_active(phba, ndlp,
					 lpfc_cmd->cur_iocbq.sli4_lxritag))
			continue;

		if (lpfc_cmd->flags & LPFC_SBUF_NOT_POSTED)
			continue;

		list_del_init(&lpfc_cmd->list);
		qp->get_io_bufs--;
		lpfc_cmd->hdwq = qp;
		lpfc_cmd->hdwq_no = idx;
		return lpfc_cmd;
	}
	return NULL;
}

 
struct lpfc_io_buf *lpfc_get_io_buf(struct lpfc_hba *phba,
				    struct lpfc_nodelist *ndlp,
				    u32 hwqid, int expedite)
{
	struct lpfc_sli4_hdw_queue *qp;
	unsigned long iflag;
	struct lpfc_io_buf *lpfc_cmd;

	qp = &phba->sli4_hba.hdwq[hwqid];
	lpfc_cmd = NULL;
	if (!qp) {
		lpfc_printf_log(phba, KERN_WARNING,
				LOG_SLI | LOG_NVME_ABTS | LOG_FCP,
				"5555 NULL qp for hwqid  x%x\n", hwqid);
		return lpfc_cmd;
	}

	if (phba->cfg_xri_rebalancing)
		lpfc_cmd = lpfc_get_io_buf_from_multixri_pools(
			phba, ndlp, hwqid, expedite);
	else {
		lpfc_qp_spin_lock_irqsave(&qp->io_buf_list_get_lock, iflag,
					  qp, alloc_xri_get);
		if (qp->get_io_bufs > LPFC_NVME_EXPEDITE_XRICNT || expedite)
			lpfc_cmd = lpfc_io_buf(phba, ndlp, hwqid);
		if (!lpfc_cmd) {
			lpfc_qp_spin_lock(&qp->io_buf_list_put_lock,
					  qp, alloc_xri_put);
			list_splice(&qp->lpfc_io_buf_list_put,
				    &qp->lpfc_io_buf_list_get);
			qp->get_io_bufs += qp->put_io_bufs;
			INIT_LIST_HEAD(&qp->lpfc_io_buf_list_put);
			qp->put_io_bufs = 0;
			spin_unlock(&qp->io_buf_list_put_lock);
			if (qp->get_io_bufs > LPFC_NVME_EXPEDITE_XRICNT ||
			    expedite)
				lpfc_cmd = lpfc_io_buf(phba, ndlp, hwqid);
		}
		spin_unlock_irqrestore(&qp->io_buf_list_get_lock, iflag);
	}

	return lpfc_cmd;
}

 
int
lpfc_read_object(struct lpfc_hba *phba, char *rdobject, uint32_t *datap,
		 uint32_t datasz)
{
	struct lpfc_mbx_read_object *read_object;
	LPFC_MBOXQ_t *mbox;
	int rc, length, eof, j, byte_cnt = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	struct lpfc_dmabuf *pcmd;
	u32 rd_object_name[LPFC_MBX_OBJECT_NAME_LEN_DW] = {0};

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_read_object) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_READ_OBJECT,
			 length, LPFC_SLI4_MBX_EMBED);
	read_object = &mbox->u.mqe.un.read_object;
	shdr = (union lpfc_sli4_cfg_shdr *)&read_object->header.cfg_shdr;

	bf_set(lpfc_mbox_hdr_version, &shdr->request, LPFC_Q_CREATE_VERSION_0);
	bf_set(lpfc_mbx_rd_object_rlen, &read_object->u.request, datasz);
	read_object->u.request.rd_object_offset = 0;
	read_object->u.request.rd_object_cnt = 1;

	memset((void *)read_object->u.request.rd_object_name, 0,
	       LPFC_OBJ_NAME_SZ);
	scnprintf((char *)rd_object_name, sizeof(rd_object_name), rdobject);
	for (j = 0; j < strlen(rdobject); j++)
		read_object->u.request.rd_object_name[j] =
			cpu_to_le32(rd_object_name[j]);

	pcmd = kmalloc(sizeof(*pcmd), GFP_KERNEL);
	if (pcmd)
		pcmd->virt = lpfc_mbuf_alloc(phba, MEM_PRI, &pcmd->phys);
	if (!pcmd || !pcmd->virt) {
		kfree(pcmd);
		mempool_free(mbox, phba->mbox_mem_pool);
		return -ENOMEM;
	}
	memset((void *)pcmd->virt, 0, LPFC_BPL_SIZE);
	read_object->u.request.rd_object_hbuf[0].pa_lo =
		putPaddrLow(pcmd->phys);
	read_object->u.request.rd_object_hbuf[0].pa_hi =
		putPaddrHigh(pcmd->phys);
	read_object->u.request.rd_object_hbuf[0].length = LPFC_BPL_SIZE;

	mbox->vport = phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	mbox->ctx_ndlp = NULL;

	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);

	if (shdr_status == STATUS_FAILED &&
	    shdr_add_status == ADD_STATUS_INVALID_OBJECT_NAME) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT | LOG_CGN_MGMT,
				"4674 No port cfg file in FW.\n");
		byte_cnt = -ENOENT;
	} else if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT | LOG_CGN_MGMT,
				"2625 READ_OBJECT mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		byte_cnt = -ENXIO;
	} else {
		 
		length = read_object->u.response.rd_object_actual_rlen;
		eof = bf_get(lpfc_mbx_rd_object_eof, &read_object->u.response);
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT | LOG_CGN_MGMT,
				"2626 READ_OBJECT Success len %d:%d, EOF %d\n",
				length, datasz, eof);

		 
		if (!length && eof) {
			byte_cnt = 0;
			goto exit;
		}

		byte_cnt = length;
		lpfc_sli_pcimem_bcopy(pcmd->virt, datap, byte_cnt);
	}

 exit:
	 
	lpfc_mbuf_free(phba, pcmd->virt, pcmd->phys);
	kfree(pcmd);
	lpfc_sli4_mbox_cmd_free(phba, mbox);
	return byte_cnt;
}

 
struct sli4_hybrid_sgl *
lpfc_get_sgl_per_hdwq(struct lpfc_hba *phba, struct lpfc_io_buf *lpfc_buf)
{
	struct sli4_hybrid_sgl *list_entry = NULL;
	struct sli4_hybrid_sgl *tmp = NULL;
	struct sli4_hybrid_sgl *allocated_sgl = NULL;
	struct lpfc_sli4_hdw_queue *hdwq = lpfc_buf->hdwq;
	struct list_head *buf_list = &hdwq->sgl_list;
	unsigned long iflags;

	spin_lock_irqsave(&hdwq->hdwq_lock, iflags);

	if (likely(!list_empty(buf_list))) {
		 
		list_for_each_entry_safe(list_entry, tmp,
					 buf_list, list_node) {
			list_move_tail(&list_entry->list_node,
				       &lpfc_buf->dma_sgl_xtra_list);
			break;
		}
	} else {
		 
		spin_unlock_irqrestore(&hdwq->hdwq_lock, iflags);
		tmp = kmalloc_node(sizeof(*tmp), GFP_ATOMIC,
				   cpu_to_node(hdwq->io_wq->chann));
		if (!tmp) {
			lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
					"8353 error kmalloc memory for HDWQ "
					"%d %s\n",
					lpfc_buf->hdwq_no, __func__);
			return NULL;
		}

		tmp->dma_sgl = dma_pool_alloc(phba->lpfc_sg_dma_buf_pool,
					      GFP_ATOMIC, &tmp->dma_phys_sgl);
		if (!tmp->dma_sgl) {
			lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
					"8354 error pool_alloc memory for HDWQ "
					"%d %s\n",
					lpfc_buf->hdwq_no, __func__);
			kfree(tmp);
			return NULL;
		}

		spin_lock_irqsave(&hdwq->hdwq_lock, iflags);
		list_add_tail(&tmp->list_node, &lpfc_buf->dma_sgl_xtra_list);
	}

	allocated_sgl = list_last_entry(&lpfc_buf->dma_sgl_xtra_list,
					struct sli4_hybrid_sgl,
					list_node);

	spin_unlock_irqrestore(&hdwq->hdwq_lock, iflags);

	return allocated_sgl;
}

 
int
lpfc_put_sgl_per_hdwq(struct lpfc_hba *phba, struct lpfc_io_buf *lpfc_buf)
{
	int rc = 0;
	struct sli4_hybrid_sgl *list_entry = NULL;
	struct sli4_hybrid_sgl *tmp = NULL;
	struct lpfc_sli4_hdw_queue *hdwq = lpfc_buf->hdwq;
	struct list_head *buf_list = &hdwq->sgl_list;
	unsigned long iflags;

	spin_lock_irqsave(&hdwq->hdwq_lock, iflags);

	if (likely(!list_empty(&lpfc_buf->dma_sgl_xtra_list))) {
		list_for_each_entry_safe(list_entry, tmp,
					 &lpfc_buf->dma_sgl_xtra_list,
					 list_node) {
			list_move_tail(&list_entry->list_node,
				       buf_list);
		}
	} else {
		rc = -EINVAL;
	}

	spin_unlock_irqrestore(&hdwq->hdwq_lock, iflags);
	return rc;
}

 
void
lpfc_free_sgl_per_hdwq(struct lpfc_hba *phba,
		       struct lpfc_sli4_hdw_queue *hdwq)
{
	struct list_head *buf_list = &hdwq->sgl_list;
	struct sli4_hybrid_sgl *list_entry = NULL;
	struct sli4_hybrid_sgl *tmp = NULL;
	unsigned long iflags;

	spin_lock_irqsave(&hdwq->hdwq_lock, iflags);

	 
	list_for_each_entry_safe(list_entry, tmp,
				 buf_list, list_node) {
		list_del(&list_entry->list_node);
		dma_pool_free(phba->lpfc_sg_dma_buf_pool,
			      list_entry->dma_sgl,
			      list_entry->dma_phys_sgl);
		kfree(list_entry);
	}

	spin_unlock_irqrestore(&hdwq->hdwq_lock, iflags);
}

 
struct fcp_cmd_rsp_buf *
lpfc_get_cmd_rsp_buf_per_hdwq(struct lpfc_hba *phba,
			      struct lpfc_io_buf *lpfc_buf)
{
	struct fcp_cmd_rsp_buf *list_entry = NULL;
	struct fcp_cmd_rsp_buf *tmp = NULL;
	struct fcp_cmd_rsp_buf *allocated_buf = NULL;
	struct lpfc_sli4_hdw_queue *hdwq = lpfc_buf->hdwq;
	struct list_head *buf_list = &hdwq->cmd_rsp_buf_list;
	unsigned long iflags;

	spin_lock_irqsave(&hdwq->hdwq_lock, iflags);

	if (likely(!list_empty(buf_list))) {
		 
		list_for_each_entry_safe(list_entry, tmp,
					 buf_list,
					 list_node) {
			list_move_tail(&list_entry->list_node,
				       &lpfc_buf->dma_cmd_rsp_list);
			break;
		}
	} else {
		 
		spin_unlock_irqrestore(&hdwq->hdwq_lock, iflags);
		tmp = kmalloc_node(sizeof(*tmp), GFP_ATOMIC,
				   cpu_to_node(hdwq->io_wq->chann));
		if (!tmp) {
			lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
					"8355 error kmalloc memory for HDWQ "
					"%d %s\n",
					lpfc_buf->hdwq_no, __func__);
			return NULL;
		}

		tmp->fcp_cmnd = dma_pool_zalloc(phba->lpfc_cmd_rsp_buf_pool,
						GFP_ATOMIC,
						&tmp->fcp_cmd_rsp_dma_handle);

		if (!tmp->fcp_cmnd) {
			lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
					"8356 error pool_alloc memory for HDWQ "
					"%d %s\n",
					lpfc_buf->hdwq_no, __func__);
			kfree(tmp);
			return NULL;
		}

		tmp->fcp_rsp = (struct fcp_rsp *)((uint8_t *)tmp->fcp_cmnd +
				sizeof(struct fcp_cmnd));

		spin_lock_irqsave(&hdwq->hdwq_lock, iflags);
		list_add_tail(&tmp->list_node, &lpfc_buf->dma_cmd_rsp_list);
	}

	allocated_buf = list_last_entry(&lpfc_buf->dma_cmd_rsp_list,
					struct fcp_cmd_rsp_buf,
					list_node);

	spin_unlock_irqrestore(&hdwq->hdwq_lock, iflags);

	return allocated_buf;
}

 
int
lpfc_put_cmd_rsp_buf_per_hdwq(struct lpfc_hba *phba,
			      struct lpfc_io_buf *lpfc_buf)
{
	int rc = 0;
	struct fcp_cmd_rsp_buf *list_entry = NULL;
	struct fcp_cmd_rsp_buf *tmp = NULL;
	struct lpfc_sli4_hdw_queue *hdwq = lpfc_buf->hdwq;
	struct list_head *buf_list = &hdwq->cmd_rsp_buf_list;
	unsigned long iflags;

	spin_lock_irqsave(&hdwq->hdwq_lock, iflags);

	if (likely(!list_empty(&lpfc_buf->dma_cmd_rsp_list))) {
		list_for_each_entry_safe(list_entry, tmp,
					 &lpfc_buf->dma_cmd_rsp_list,
					 list_node) {
			list_move_tail(&list_entry->list_node,
				       buf_list);
		}
	} else {
		rc = -EINVAL;
	}

	spin_unlock_irqrestore(&hdwq->hdwq_lock, iflags);
	return rc;
}

 
void
lpfc_free_cmd_rsp_buf_per_hdwq(struct lpfc_hba *phba,
			       struct lpfc_sli4_hdw_queue *hdwq)
{
	struct list_head *buf_list = &hdwq->cmd_rsp_buf_list;
	struct fcp_cmd_rsp_buf *list_entry = NULL;
	struct fcp_cmd_rsp_buf *tmp = NULL;
	unsigned long iflags;

	spin_lock_irqsave(&hdwq->hdwq_lock, iflags);

	 
	list_for_each_entry_safe(list_entry, tmp,
				 buf_list,
				 list_node) {
		list_del(&list_entry->list_node);
		dma_pool_free(phba->lpfc_cmd_rsp_buf_pool,
			      list_entry->fcp_cmnd,
			      list_entry->fcp_cmd_rsp_dma_handle);
		kfree(list_entry);
	}

	spin_unlock_irqrestore(&hdwq->hdwq_lock, iflags);
}

 
void
lpfc_sli_prep_wqe(struct lpfc_hba *phba, struct lpfc_iocbq *job)
{
	u8 cmnd;
	u32 *pcmd;
	u32 if_type = 0;
	u32 fip, abort_tag;
	struct lpfc_nodelist *ndlp = NULL;
	union lpfc_wqe128 *wqe = &job->wqe;
	u8 command_type = ELS_COMMAND_NON_FIP;

	fip = phba->hba_flag & HBA_FIP_SUPPORT;
	 
	if (job->cmd_flag &  LPFC_IO_FCP)
		command_type = FCP_COMMAND;
	else if (fip && (job->cmd_flag & LPFC_FIP_ELS_ID_MASK))
		command_type = ELS_COMMAND_FIP;
	else
		command_type = ELS_COMMAND_NON_FIP;

	abort_tag = job->iotag;
	cmnd = bf_get(wqe_cmnd, &wqe->els_req.wqe_com);

	switch (cmnd) {
	case CMD_ELS_REQUEST64_WQE:
		ndlp = job->ndlp;

		if_type = bf_get(lpfc_sli_intf_if_type,
				 &phba->sli4_hba.sli_intf);
		if (if_type >= LPFC_SLI_INTF_IF_TYPE_2) {
			pcmd = (u32 *)job->cmd_dmabuf->virt;
			if (pcmd && (*pcmd == ELS_CMD_FLOGI ||
				     *pcmd == ELS_CMD_SCR ||
				     *pcmd == ELS_CMD_RDF ||
				     *pcmd == ELS_CMD_EDC ||
				     *pcmd == ELS_CMD_RSCN_XMT ||
				     *pcmd == ELS_CMD_FDISC ||
				     *pcmd == ELS_CMD_LOGO ||
				     *pcmd == ELS_CMD_QFPA ||
				     *pcmd == ELS_CMD_UVEM ||
				     *pcmd == ELS_CMD_PLOGI)) {
				bf_set(els_req64_sp, &wqe->els_req, 1);
				bf_set(els_req64_sid, &wqe->els_req,
				       job->vport->fc_myDID);

				if ((*pcmd == ELS_CMD_FLOGI) &&
				    !(phba->fc_topology ==
				      LPFC_TOPOLOGY_LOOP))
					bf_set(els_req64_sid, &wqe->els_req, 0);

				bf_set(wqe_ct, &wqe->els_req.wqe_com, 1);
				bf_set(wqe_ctxt_tag, &wqe->els_req.wqe_com,
				       phba->vpi_ids[job->vport->vpi]);
			} else if (pcmd) {
				bf_set(wqe_ct, &wqe->els_req.wqe_com, 0);
				bf_set(wqe_ctxt_tag, &wqe->els_req.wqe_com,
				       phba->sli4_hba.rpi_ids[ndlp->nlp_rpi]);
			}
		}

		bf_set(wqe_temp_rpi, &wqe->els_req.wqe_com,
		       phba->sli4_hba.rpi_ids[ndlp->nlp_rpi]);

		bf_set(wqe_dbde, &wqe->els_req.wqe_com, 1);
		bf_set(wqe_iod, &wqe->els_req.wqe_com, LPFC_WQE_IOD_READ);
		bf_set(wqe_qosd, &wqe->els_req.wqe_com, 1);
		bf_set(wqe_lenloc, &wqe->els_req.wqe_com, LPFC_WQE_LENLOC_NONE);
		bf_set(wqe_ebde_cnt, &wqe->els_req.wqe_com, 0);
		break;
	case CMD_XMIT_ELS_RSP64_WQE:
		ndlp = job->ndlp;

		 
		wqe->xmit_els_rsp.word4 = 0;

		if_type = bf_get(lpfc_sli_intf_if_type,
				 &phba->sli4_hba.sli_intf);
		if (if_type >= LPFC_SLI_INTF_IF_TYPE_2) {
			if (job->vport->fc_flag & FC_PT2PT) {
				bf_set(els_rsp64_sp, &wqe->xmit_els_rsp, 1);
				bf_set(els_rsp64_sid, &wqe->xmit_els_rsp,
				       job->vport->fc_myDID);
				if (job->vport->fc_myDID == Fabric_DID) {
					bf_set(wqe_els_did,
					       &wqe->xmit_els_rsp.wqe_dest, 0);
				}
			}
		}

		bf_set(wqe_dbde, &wqe->xmit_els_rsp.wqe_com, 1);
		bf_set(wqe_iod, &wqe->xmit_els_rsp.wqe_com, LPFC_WQE_IOD_WRITE);
		bf_set(wqe_qosd, &wqe->xmit_els_rsp.wqe_com, 1);
		bf_set(wqe_lenloc, &wqe->xmit_els_rsp.wqe_com,
		       LPFC_WQE_LENLOC_WORD3);
		bf_set(wqe_ebde_cnt, &wqe->xmit_els_rsp.wqe_com, 0);

		if (phba->fc_topology == LPFC_TOPOLOGY_LOOP) {
			bf_set(els_rsp64_sp, &wqe->xmit_els_rsp, 1);
			bf_set(els_rsp64_sid, &wqe->xmit_els_rsp,
			       job->vport->fc_myDID);
			bf_set(wqe_ct, &wqe->xmit_els_rsp.wqe_com, 1);
		}

		if (phba->sli_rev == LPFC_SLI_REV4) {
			bf_set(wqe_rsp_temp_rpi, &wqe->xmit_els_rsp,
			       phba->sli4_hba.rpi_ids[ndlp->nlp_rpi]);

			if (bf_get(wqe_ct, &wqe->xmit_els_rsp.wqe_com))
				bf_set(wqe_ctxt_tag, &wqe->xmit_els_rsp.wqe_com,
				       phba->vpi_ids[job->vport->vpi]);
		}
		command_type = OTHER_COMMAND;
		break;
	case CMD_GEN_REQUEST64_WQE:
		 
		bf_set(wqe_dbde, &wqe->gen_req.wqe_com, 1);
		bf_set(wqe_iod, &wqe->gen_req.wqe_com, LPFC_WQE_IOD_READ);
		bf_set(wqe_qosd, &wqe->gen_req.wqe_com, 1);
		bf_set(wqe_lenloc, &wqe->gen_req.wqe_com, LPFC_WQE_LENLOC_NONE);
		bf_set(wqe_ebde_cnt, &wqe->gen_req.wqe_com, 0);
		command_type = OTHER_COMMAND;
		break;
	case CMD_XMIT_SEQUENCE64_WQE:
		if (phba->link_flag & LS_LOOPBACK_MODE)
			bf_set(wqe_xo, &wqe->xmit_sequence.wge_ctl, 1);

		wqe->xmit_sequence.rsvd3 = 0;
		bf_set(wqe_pu, &wqe->xmit_sequence.wqe_com, 0);
		bf_set(wqe_dbde, &wqe->xmit_sequence.wqe_com, 1);
		bf_set(wqe_iod, &wqe->xmit_sequence.wqe_com,
		       LPFC_WQE_IOD_WRITE);
		bf_set(wqe_lenloc, &wqe->xmit_sequence.wqe_com,
		       LPFC_WQE_LENLOC_WORD12);
		bf_set(wqe_ebde_cnt, &wqe->xmit_sequence.wqe_com, 0);
		command_type = OTHER_COMMAND;
		break;
	case CMD_XMIT_BLS_RSP64_WQE:
		bf_set(xmit_bls_rsp64_seqcnthi, &wqe->xmit_bls_rsp, 0xffff);
		bf_set(wqe_xmit_bls_pt, &wqe->xmit_bls_rsp.wqe_dest, 0x1);
		bf_set(wqe_ct, &wqe->xmit_bls_rsp.wqe_com, 1);
		bf_set(wqe_ctxt_tag, &wqe->xmit_bls_rsp.wqe_com,
		       phba->vpi_ids[phba->pport->vpi]);
		bf_set(wqe_qosd, &wqe->xmit_bls_rsp.wqe_com, 1);
		bf_set(wqe_lenloc, &wqe->xmit_bls_rsp.wqe_com,
		       LPFC_WQE_LENLOC_NONE);
		 
		command_type = OTHER_COMMAND;
		break;
	case CMD_FCP_ICMND64_WQE:	 
	case CMD_ABORT_XRI_WQE:		 
	case CMD_SEND_FRAME:		 
		 
		return;
	default:
		dump_stack();
		lpfc_printf_log(phba, KERN_ERR, LOG_TRACE_EVENT,
				"6207 Invalid command 0x%x\n",
				cmnd);
		break;
	}

	wqe->generic.wqe_com.abort_tag = abort_tag;
	bf_set(wqe_reqtag, &wqe->generic.wqe_com, job->iotag);
	bf_set(wqe_cmd_type, &wqe->generic.wqe_com, command_type);
	bf_set(wqe_cqid, &wqe->generic.wqe_com, LPFC_WQE_CQ_ID_DEFAULT);
}
