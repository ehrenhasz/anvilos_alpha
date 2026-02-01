 

#include <linux/interrupt.h>
#include <linux/dma-direction.h>

#include <scsi/scsi_transport_fc.h>

#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_nl.h"
#include "lpfc_disc.h"
#include "lpfc.h"
#include "lpfc_crtn.h"


 
struct lpfc_vmid *lpfc_get_vmid_from_hashtable(struct lpfc_vport *vport,
					       u32 hash, u8 *buf)
{
	struct lpfc_vmid *vmp;

	hash_for_each_possible(vport->hash_table, vmp, hnode, hash) {
		if (memcmp(&vmp->host_vmid[0], buf, 16) == 0)
			return vmp;
	}
	return NULL;
}

 
static void
lpfc_put_vmid_in_hashtable(struct lpfc_vport *vport, u32 hash,
			   struct lpfc_vmid *vmp)
{
	hash_add(vport->hash_table, &vmp->hnode, hash);
}

 
int lpfc_vmid_hash_fn(const char *vmid, int len)
{
	int c;
	int hash = 0;

	if (len == 0)
		return 0;
	while (len--) {
		c = *vmid++;
		if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';

		hash = (hash + (c << LPFC_VMID_HASH_SHIFT) +
			(c >> LPFC_VMID_HASH_SHIFT)) * 19;
	}

	return hash & LPFC_VMID_HASH_MASK;
}

 
static void lpfc_vmid_update_entry(struct lpfc_vport *vport,
				   enum dma_data_direction iodir,
				   struct lpfc_vmid *vmp,
				   union lpfc_vmid_io_tag *tag)
{
	u64 *lta;

	if (vport->phba->pport->vmid_flag & LPFC_VMID_TYPE_PRIO)
		tag->cs_ctl_vmid = vmp->un.cs_ctl_vmid;
	else if (vport->phba->cfg_vmid_app_header)
		tag->app_id = vmp->un.app_id;

	if (iodir == DMA_TO_DEVICE)
		vmp->io_wr_cnt++;
	else if (iodir == DMA_FROM_DEVICE)
		vmp->io_rd_cnt++;

	 
	lta = per_cpu_ptr(vmp->last_io_time, raw_smp_processor_id());
	*lta = jiffies;
}

static void lpfc_vmid_assign_cs_ctl(struct lpfc_vport *vport,
				    struct lpfc_vmid *vmid)
{
	u32 hash;
	struct lpfc_vmid *pvmid;

	if (vport->port_type == LPFC_PHYSICAL_PORT) {
		vmid->un.cs_ctl_vmid = lpfc_vmid_get_cs_ctl(vport);
	} else {
		hash = lpfc_vmid_hash_fn(vmid->host_vmid, vmid->vmid_len);
		pvmid =
		    lpfc_get_vmid_from_hashtable(vport->phba->pport, hash,
						 vmid->host_vmid);
		if (pvmid)
			vmid->un.cs_ctl_vmid = pvmid->un.cs_ctl_vmid;
		else
			vmid->un.cs_ctl_vmid = lpfc_vmid_get_cs_ctl(vport);
	}
}

 
int lpfc_vmid_get_appid(struct lpfc_vport *vport, char *uuid,
			enum dma_data_direction iodir,
			union lpfc_vmid_io_tag *tag)
{
	struct lpfc_vmid *vmp = NULL;
	int hash, len, rc = -EPERM, i;

	 
	if (lpfc_vmid_is_type_priority_tag(vport) &&
	    !(vport->vmid_flag & LPFC_VMID_QFPA_CMPL) &&
	    (vport->vmid_flag & LPFC_VMID_ISSUE_QFPA)) {
		vport->work_port_events |= WORKER_CHECK_VMID_ISSUE_QFPA;
		return -EAGAIN;
	}

	 
	len = strlen(uuid);
	hash = lpfc_vmid_hash_fn(uuid, len);

	 
	read_lock(&vport->vmid_lock);
	vmp = lpfc_get_vmid_from_hashtable(vport, hash, uuid);

	 
	if (vmp  && vmp->flag & LPFC_VMID_REGISTERED) {
		read_unlock(&vport->vmid_lock);
		lpfc_vmid_update_entry(vport, iodir, vmp, tag);
		rc = 0;
	} else if (vmp && (vmp->flag & LPFC_VMID_REQ_REGISTER ||
			   vmp->flag & LPFC_VMID_DE_REGISTER)) {
		 
		 
		read_unlock(&vport->vmid_lock);
		rc = -EBUSY;
	} else {
		 
		 
		read_unlock(&vport->vmid_lock);
		 
		 
		write_lock(&vport->vmid_lock);
		vmp = lpfc_get_vmid_from_hashtable(vport, hash, uuid);

		 
		 
		if (vmp && vmp->flag & LPFC_VMID_REGISTERED) {
			lpfc_vmid_update_entry(vport, iodir, vmp, tag);
			write_unlock(&vport->vmid_lock);
			return 0;
		} else if (vmp && vmp->flag & LPFC_VMID_REQ_REGISTER) {
			write_unlock(&vport->vmid_lock);
			return -EBUSY;
		}

		 
		if (vport->cur_vmid_cnt < vport->max_vmid) {
			for (i = 0; i < vport->max_vmid; i++) {
				vmp = vport->vmid + i;
				if (vmp->flag == LPFC_VMID_SLOT_FREE)
					break;
			}
			if (i == vport->max_vmid)
				vmp = NULL;
		} else {
			vmp = NULL;
		}

		if (!vmp) {
			write_unlock(&vport->vmid_lock);
			return -ENOMEM;
		}

		 
		lpfc_put_vmid_in_hashtable(vport, hash, vmp);
		vmp->vmid_len = len;
		memcpy(vmp->host_vmid, uuid, vmp->vmid_len);
		vmp->io_rd_cnt = 0;
		vmp->io_wr_cnt = 0;
		vmp->flag = LPFC_VMID_SLOT_USED;

		vmp->delete_inactive =
			vport->vmid_inactivity_timeout ? 1 : 0;

		 
		if (vport->phba->pport->vmid_flag & LPFC_VMID_TYPE_PRIO)
			lpfc_vmid_assign_cs_ctl(vport, vmp);

		 
		 
		if (!vmp->last_io_time)
			vmp->last_io_time = alloc_percpu_gfp(u64, GFP_ATOMIC);
		if (!vmp->last_io_time) {
			hash_del(&vmp->hnode);
			vmp->flag = LPFC_VMID_SLOT_FREE;
			write_unlock(&vport->vmid_lock);
			return -EIO;
		}

		write_unlock(&vport->vmid_lock);

		 
		if (vport->phba->pport->vmid_flag & LPFC_VMID_TYPE_PRIO)
			rc = lpfc_vmid_uvem(vport, vmp, true);
		else if (vport->phba->cfg_vmid_app_header)
			rc = lpfc_vmid_cmd(vport, SLI_CTAS_RAPP_IDENT, vmp);
		if (!rc) {
			write_lock(&vport->vmid_lock);
			vport->cur_vmid_cnt++;
			vmp->flag |= LPFC_VMID_REQ_REGISTER;
			write_unlock(&vport->vmid_lock);
		} else {
			write_lock(&vport->vmid_lock);
			hash_del(&vmp->hnode);
			vmp->flag = LPFC_VMID_SLOT_FREE;
			free_percpu(vmp->last_io_time);
			write_unlock(&vport->vmid_lock);
			return -EIO;
		}

		 
		if (!(vport->phba->pport->vmid_flag & LPFC_VMID_TIMER_ENBLD)) {
			mod_timer(&vport->phba->inactive_vmid_poll,
				  jiffies +
				  msecs_to_jiffies(1000 * LPFC_VMID_TIMER));
			vport->phba->pport->vmid_flag |= LPFC_VMID_TIMER_ENBLD;
		}
	}
	return rc;
}

 
void
lpfc_reinit_vmid(struct lpfc_vport *vport)
{
	u32 bucket, i, cpu;
	struct lpfc_vmid *cur;
	struct lpfc_vmid *vmp = NULL;
	struct hlist_node *tmp;

	write_lock(&vport->vmid_lock);
	vport->cur_vmid_cnt = 0;

	for (i = 0; i < vport->max_vmid; i++) {
		vmp = &vport->vmid[i];
		vmp->flag = LPFC_VMID_SLOT_FREE;
		memset(vmp->host_vmid, 0, sizeof(vmp->host_vmid));
		vmp->io_rd_cnt = 0;
		vmp->io_wr_cnt = 0;

		if (vmp->last_io_time)
			for_each_possible_cpu(cpu)
				*per_cpu_ptr(vmp->last_io_time, cpu) = 0;
	}

	 
	if (!hash_empty(vport->hash_table))
		hash_for_each_safe(vport->hash_table, bucket, tmp, cur, hnode)
			hash_del(&cur->hnode);
	write_unlock(&vport->vmid_lock);
}
