
 

#include <linux/net.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/in.h>
#include <linux/export.h>
#include <linux/t10-pi.h>
#include <asm/unaligned.h>
#include <net/sock.h>
#include <net/tcp.h>
#include <scsi/scsi_common.h>
#include <scsi/scsi_proto.h>

#include <target/target_core_base.h>
#include <target/target_core_backend.h>
#include <target/target_core_fabric.h>

#include "target_core_internal.h"
#include "target_core_alua.h"
#include "target_core_pr.h"
#include "target_core_ua.h"

static DEFINE_MUTEX(device_mutex);
static LIST_HEAD(device_list);
static DEFINE_IDR(devices_idr);

static struct se_hba *lun0_hba;
 
struct se_device *g_lun0_dev;

sense_reason_t
transport_lookup_cmd_lun(struct se_cmd *se_cmd)
{
	struct se_lun *se_lun = NULL;
	struct se_session *se_sess = se_cmd->se_sess;
	struct se_node_acl *nacl = se_sess->se_node_acl;
	struct se_dev_entry *deve;
	sense_reason_t ret = TCM_NO_SENSE;

	rcu_read_lock();
	deve = target_nacl_find_deve(nacl, se_cmd->orig_fe_lun);
	if (deve) {
		atomic_long_inc(&deve->total_cmds);

		if (se_cmd->data_direction == DMA_TO_DEVICE)
			atomic_long_add(se_cmd->data_length,
					&deve->write_bytes);
		else if (se_cmd->data_direction == DMA_FROM_DEVICE)
			atomic_long_add(se_cmd->data_length,
					&deve->read_bytes);

		if ((se_cmd->data_direction == DMA_TO_DEVICE) &&
		    deve->lun_access_ro) {
			pr_err("TARGET_CORE[%s]: Detected WRITE_PROTECTED LUN"
				" Access for 0x%08llx\n",
				se_cmd->se_tfo->fabric_name,
				se_cmd->orig_fe_lun);
			rcu_read_unlock();
			return TCM_WRITE_PROTECTED;
		}

		se_lun = deve->se_lun;

		if (!percpu_ref_tryget_live(&se_lun->lun_ref)) {
			se_lun = NULL;
			goto out_unlock;
		}

		se_cmd->se_lun = se_lun;
		se_cmd->pr_res_key = deve->pr_res_key;
		se_cmd->se_cmd_flags |= SCF_SE_LUN_CMD;
		se_cmd->lun_ref_active = true;
	}
out_unlock:
	rcu_read_unlock();

	if (!se_lun) {
		 
		if (se_cmd->orig_fe_lun != 0) {
			pr_err("TARGET_CORE[%s]: Detected NON_EXISTENT_LUN"
				" Access for 0x%08llx from %s\n",
				se_cmd->se_tfo->fabric_name,
				se_cmd->orig_fe_lun,
				nacl->initiatorname);
			return TCM_NON_EXISTENT_LUN;
		}

		 
		if ((se_cmd->data_direction != DMA_FROM_DEVICE) &&
		    (se_cmd->data_direction != DMA_NONE))
			return TCM_WRITE_PROTECTED;

		se_lun = se_sess->se_tpg->tpg_virt_lun0;
		if (!percpu_ref_tryget_live(&se_lun->lun_ref))
			return TCM_NON_EXISTENT_LUN;

		se_cmd->se_lun = se_sess->se_tpg->tpg_virt_lun0;
		se_cmd->se_cmd_flags |= SCF_SE_LUN_CMD;
		se_cmd->lun_ref_active = true;
	}
	 
	se_cmd->se_dev = rcu_dereference_raw(se_lun->lun_se_dev);
	atomic_long_inc(&se_cmd->se_dev->num_cmds);

	if (se_cmd->data_direction == DMA_TO_DEVICE)
		atomic_long_add(se_cmd->data_length,
				&se_cmd->se_dev->write_bytes);
	else if (se_cmd->data_direction == DMA_FROM_DEVICE)
		atomic_long_add(se_cmd->data_length,
				&se_cmd->se_dev->read_bytes);

	return ret;
}
EXPORT_SYMBOL(transport_lookup_cmd_lun);

int transport_lookup_tmr_lun(struct se_cmd *se_cmd)
{
	struct se_dev_entry *deve;
	struct se_lun *se_lun = NULL;
	struct se_session *se_sess = se_cmd->se_sess;
	struct se_node_acl *nacl = se_sess->se_node_acl;
	struct se_tmr_req *se_tmr = se_cmd->se_tmr_req;
	unsigned long flags;

	rcu_read_lock();
	deve = target_nacl_find_deve(nacl, se_cmd->orig_fe_lun);
	if (deve) {
		se_lun = deve->se_lun;

		if (!percpu_ref_tryget_live(&se_lun->lun_ref)) {
			se_lun = NULL;
			goto out_unlock;
		}

		se_cmd->se_lun = se_lun;
		se_cmd->pr_res_key = deve->pr_res_key;
		se_cmd->se_cmd_flags |= SCF_SE_LUN_CMD;
		se_cmd->lun_ref_active = true;
	}
out_unlock:
	rcu_read_unlock();

	if (!se_lun) {
		pr_debug("TARGET_CORE[%s]: Detected NON_EXISTENT_LUN"
			" Access for 0x%08llx for %s\n",
			se_cmd->se_tfo->fabric_name,
			se_cmd->orig_fe_lun,
			nacl->initiatorname);
		return -ENODEV;
	}
	se_cmd->se_dev = rcu_dereference_raw(se_lun->lun_se_dev);
	se_tmr->tmr_dev = rcu_dereference_raw(se_lun->lun_se_dev);

	spin_lock_irqsave(&se_tmr->tmr_dev->se_tmr_lock, flags);
	list_add_tail(&se_tmr->tmr_list, &se_tmr->tmr_dev->dev_tmr_list);
	spin_unlock_irqrestore(&se_tmr->tmr_dev->se_tmr_lock, flags);

	return 0;
}
EXPORT_SYMBOL(transport_lookup_tmr_lun);

bool target_lun_is_rdonly(struct se_cmd *cmd)
{
	struct se_session *se_sess = cmd->se_sess;
	struct se_dev_entry *deve;
	bool ret;

	rcu_read_lock();
	deve = target_nacl_find_deve(se_sess->se_node_acl, cmd->orig_fe_lun);
	ret = deve && deve->lun_access_ro;
	rcu_read_unlock();

	return ret;
}
EXPORT_SYMBOL(target_lun_is_rdonly);

 
struct se_dev_entry *core_get_se_deve_from_rtpi(
	struct se_node_acl *nacl,
	u16 rtpi)
{
	struct se_dev_entry *deve;
	struct se_lun *lun;
	struct se_portal_group *tpg = nacl->se_tpg;

	rcu_read_lock();
	hlist_for_each_entry_rcu(deve, &nacl->lun_entry_hlist, link) {
		lun = deve->se_lun;
		if (!lun) {
			pr_err("%s device entries device pointer is"
				" NULL, but Initiator has access.\n",
				tpg->se_tpg_tfo->fabric_name);
			continue;
		}
		if (lun->lun_tpg->tpg_rtpi != rtpi)
			continue;

		kref_get(&deve->pr_kref);
		rcu_read_unlock();

		return deve;
	}
	rcu_read_unlock();

	return NULL;
}

void core_free_device_list_for_node(
	struct se_node_acl *nacl,
	struct se_portal_group *tpg)
{
	struct se_dev_entry *deve;

	mutex_lock(&nacl->lun_entry_mutex);
	hlist_for_each_entry_rcu(deve, &nacl->lun_entry_hlist, link)
		core_disable_device_list_for_node(deve->se_lun, deve, nacl, tpg);
	mutex_unlock(&nacl->lun_entry_mutex);
}

void core_update_device_list_access(
	u64 mapped_lun,
	bool lun_access_ro,
	struct se_node_acl *nacl)
{
	struct se_dev_entry *deve;

	mutex_lock(&nacl->lun_entry_mutex);
	deve = target_nacl_find_deve(nacl, mapped_lun);
	if (deve)
		deve->lun_access_ro = lun_access_ro;
	mutex_unlock(&nacl->lun_entry_mutex);
}

 
struct se_dev_entry *target_nacl_find_deve(struct se_node_acl *nacl, u64 mapped_lun)
{
	struct se_dev_entry *deve;

	hlist_for_each_entry_rcu(deve, &nacl->lun_entry_hlist, link)
		if (deve->mapped_lun == mapped_lun)
			return deve;

	return NULL;
}
EXPORT_SYMBOL(target_nacl_find_deve);

void target_pr_kref_release(struct kref *kref)
{
	struct se_dev_entry *deve = container_of(kref, struct se_dev_entry,
						 pr_kref);
	complete(&deve->pr_comp);
}

 
void target_dev_ua_allocate(struct se_device *dev, u8 asc, u8 ascq)
{
	struct se_dev_entry *se_deve;
	struct se_lun *lun;

	spin_lock(&dev->se_port_lock);
	list_for_each_entry(lun, &dev->dev_sep_list, lun_dev_link) {

		spin_lock(&lun->lun_deve_lock);
		list_for_each_entry(se_deve, &lun->lun_deve_list, lun_link)
			core_scsi3_ua_allocate(se_deve, asc, ascq);
		spin_unlock(&lun->lun_deve_lock);
	}
	spin_unlock(&dev->se_port_lock);
}

static void
target_luns_data_has_changed(struct se_node_acl *nacl, struct se_dev_entry *new,
			     bool skip_new)
{
	struct se_dev_entry *tmp;

	rcu_read_lock();
	hlist_for_each_entry_rcu(tmp, &nacl->lun_entry_hlist, link) {
		if (skip_new && tmp == new)
			continue;
		core_scsi3_ua_allocate(tmp, 0x3F,
				       ASCQ_3FH_REPORTED_LUNS_DATA_HAS_CHANGED);
	}
	rcu_read_unlock();
}

int core_enable_device_list_for_node(
	struct se_lun *lun,
	struct se_lun_acl *lun_acl,
	u64 mapped_lun,
	bool lun_access_ro,
	struct se_node_acl *nacl,
	struct se_portal_group *tpg)
{
	struct se_dev_entry *orig, *new;

	new = kzalloc(sizeof(*new), GFP_KERNEL);
	if (!new) {
		pr_err("Unable to allocate se_dev_entry memory\n");
		return -ENOMEM;
	}

	spin_lock_init(&new->ua_lock);
	INIT_LIST_HEAD(&new->ua_list);
	INIT_LIST_HEAD(&new->lun_link);

	new->mapped_lun = mapped_lun;
	kref_init(&new->pr_kref);
	init_completion(&new->pr_comp);

	new->lun_access_ro = lun_access_ro;
	new->creation_time = get_jiffies_64();
	new->attach_count++;

	mutex_lock(&nacl->lun_entry_mutex);
	orig = target_nacl_find_deve(nacl, mapped_lun);
	if (orig && orig->se_lun) {
		struct se_lun *orig_lun = orig->se_lun;

		if (orig_lun != lun) {
			pr_err("Existing orig->se_lun doesn't match new lun"
			       " for dynamic -> explicit NodeACL conversion:"
				" %s\n", nacl->initiatorname);
			mutex_unlock(&nacl->lun_entry_mutex);
			kfree(new);
			return -EINVAL;
		}
		if (orig->se_lun_acl != NULL) {
			pr_warn_ratelimited("Detected existing explicit"
				" se_lun_acl->se_lun_group reference for %s"
				" mapped_lun: %llu, failing\n",
				 nacl->initiatorname, mapped_lun);
			mutex_unlock(&nacl->lun_entry_mutex);
			kfree(new);
			return -EINVAL;
		}

		new->se_lun = lun;
		new->se_lun_acl = lun_acl;
		hlist_del_rcu(&orig->link);
		hlist_add_head_rcu(&new->link, &nacl->lun_entry_hlist);
		mutex_unlock(&nacl->lun_entry_mutex);

		spin_lock(&lun->lun_deve_lock);
		list_del(&orig->lun_link);
		list_add_tail(&new->lun_link, &lun->lun_deve_list);
		spin_unlock(&lun->lun_deve_lock);

		kref_put(&orig->pr_kref, target_pr_kref_release);
		wait_for_completion(&orig->pr_comp);

		target_luns_data_has_changed(nacl, new, true);
		kfree_rcu(orig, rcu_head);
		return 0;
	}

	new->se_lun = lun;
	new->se_lun_acl = lun_acl;
	hlist_add_head_rcu(&new->link, &nacl->lun_entry_hlist);
	mutex_unlock(&nacl->lun_entry_mutex);

	spin_lock(&lun->lun_deve_lock);
	list_add_tail(&new->lun_link, &lun->lun_deve_list);
	spin_unlock(&lun->lun_deve_lock);

	target_luns_data_has_changed(nacl, new, true);
	return 0;
}

void core_disable_device_list_for_node(
	struct se_lun *lun,
	struct se_dev_entry *orig,
	struct se_node_acl *nacl,
	struct se_portal_group *tpg)
{
	 
	struct se_device *dev = rcu_dereference_raw(lun->lun_se_dev);

	lockdep_assert_held(&nacl->lun_entry_mutex);

	 
	spin_lock(&lun->lun_deve_lock);
	list_del(&orig->lun_link);
	spin_unlock(&lun->lun_deve_lock);
	 
	core_scsi3_ua_release_all(orig);

	hlist_del_rcu(&orig->link);
	clear_bit(DEF_PR_REG_ACTIVE, &orig->deve_flags);
	orig->lun_access_ro = false;
	orig->creation_time = 0;
	orig->attach_count--;
	 
	kref_put(&orig->pr_kref, target_pr_kref_release);
	wait_for_completion(&orig->pr_comp);

	kfree_rcu(orig, rcu_head);

	core_scsi3_free_pr_reg_from_nacl(dev, nacl);
	target_luns_data_has_changed(nacl, NULL, false);
}

 
void core_clear_lun_from_tpg(struct se_lun *lun, struct se_portal_group *tpg)
{
	struct se_node_acl *nacl;
	struct se_dev_entry *deve;

	mutex_lock(&tpg->acl_node_mutex);
	list_for_each_entry(nacl, &tpg->acl_node_list, acl_list) {

		mutex_lock(&nacl->lun_entry_mutex);
		hlist_for_each_entry_rcu(deve, &nacl->lun_entry_hlist, link) {
			if (lun != deve->se_lun)
				continue;

			core_disable_device_list_for_node(lun, deve, nacl, tpg);
		}
		mutex_unlock(&nacl->lun_entry_mutex);
	}
	mutex_unlock(&tpg->acl_node_mutex);
}

static void se_release_vpd_for_dev(struct se_device *dev)
{
	struct t10_vpd *vpd, *vpd_tmp;

	spin_lock(&dev->t10_wwn.t10_vpd_lock);
	list_for_each_entry_safe(vpd, vpd_tmp,
			&dev->t10_wwn.t10_vpd_list, vpd_list) {
		list_del(&vpd->vpd_list);
		kfree(vpd);
	}
	spin_unlock(&dev->t10_wwn.t10_vpd_lock);
}

static u32 se_dev_align_max_sectors(u32 max_sectors, u32 block_size)
{
	u32 aligned_max_sectors;
	u32 alignment;
	 
	alignment = max(1ul, PAGE_SIZE / block_size);
	aligned_max_sectors = rounddown(max_sectors, alignment);

	if (max_sectors != aligned_max_sectors)
		pr_info("Rounding down aligned max_sectors from %u to %u\n",
			max_sectors, aligned_max_sectors);

	return aligned_max_sectors;
}

int core_dev_add_lun(
	struct se_portal_group *tpg,
	struct se_device *dev,
	struct se_lun *lun)
{
	int rc;

	rc = core_tpg_add_lun(tpg, lun, false, dev);
	if (rc < 0)
		return rc;

	pr_debug("%s_TPG[%u]_LUN[%llu] - Activated %s Logical Unit from"
		" CORE HBA: %u\n", tpg->se_tpg_tfo->fabric_name,
		tpg->se_tpg_tfo->tpg_get_tag(tpg), lun->unpacked_lun,
		tpg->se_tpg_tfo->fabric_name, dev->se_hba->hba_id);
	 
	if (tpg->se_tpg_tfo->tpg_check_demo_mode(tpg)) {
		struct se_node_acl *acl;

		mutex_lock(&tpg->acl_node_mutex);
		list_for_each_entry(acl, &tpg->acl_node_list, acl_list) {
			if (acl->dynamic_node_acl &&
			    (!tpg->se_tpg_tfo->tpg_check_demo_mode_login_only ||
			     !tpg->se_tpg_tfo->tpg_check_demo_mode_login_only(tpg))) {
				core_tpg_add_node_to_devs(acl, tpg, lun);
			}
		}
		mutex_unlock(&tpg->acl_node_mutex);
	}

	return 0;
}

 
void core_dev_del_lun(
	struct se_portal_group *tpg,
	struct se_lun *lun)
{
	pr_debug("%s_TPG[%u]_LUN[%llu] - Deactivating %s Logical Unit from"
		" device object\n", tpg->se_tpg_tfo->fabric_name,
		tpg->se_tpg_tfo->tpg_get_tag(tpg), lun->unpacked_lun,
		tpg->se_tpg_tfo->fabric_name);

	core_tpg_remove_lun(tpg, lun);
}

struct se_lun_acl *core_dev_init_initiator_node_lun_acl(
	struct se_portal_group *tpg,
	struct se_node_acl *nacl,
	u64 mapped_lun,
	int *ret)
{
	struct se_lun_acl *lacl;

	if (strlen(nacl->initiatorname) >= TRANSPORT_IQN_LEN) {
		pr_err("%s InitiatorName exceeds maximum size.\n",
			tpg->se_tpg_tfo->fabric_name);
		*ret = -EOVERFLOW;
		return NULL;
	}
	lacl = kzalloc(sizeof(struct se_lun_acl), GFP_KERNEL);
	if (!lacl) {
		pr_err("Unable to allocate memory for struct se_lun_acl.\n");
		*ret = -ENOMEM;
		return NULL;
	}

	lacl->mapped_lun = mapped_lun;
	lacl->se_lun_nacl = nacl;

	return lacl;
}

int core_dev_add_initiator_node_lun_acl(
	struct se_portal_group *tpg,
	struct se_lun_acl *lacl,
	struct se_lun *lun,
	bool lun_access_ro)
{
	struct se_node_acl *nacl = lacl->se_lun_nacl;
	 
	struct se_device *dev = rcu_dereference_raw(lun->lun_se_dev);

	if (!nacl)
		return -EINVAL;

	if (lun->lun_access_ro)
		lun_access_ro = true;

	lacl->se_lun = lun;

	if (core_enable_device_list_for_node(lun, lacl, lacl->mapped_lun,
			lun_access_ro, nacl, tpg) < 0)
		return -EINVAL;

	pr_debug("%s_TPG[%hu]_LUN[%llu->%llu] - Added %s ACL for "
		" InitiatorNode: %s\n", tpg->se_tpg_tfo->fabric_name,
		tpg->se_tpg_tfo->tpg_get_tag(tpg), lun->unpacked_lun, lacl->mapped_lun,
		lun_access_ro ? "RO" : "RW",
		nacl->initiatorname);
	 
	core_scsi3_check_aptpl_registration(dev, tpg, lun, nacl,
					    lacl->mapped_lun);
	return 0;
}

int core_dev_del_initiator_node_lun_acl(
	struct se_lun *lun,
	struct se_lun_acl *lacl)
{
	struct se_portal_group *tpg = lun->lun_tpg;
	struct se_node_acl *nacl;
	struct se_dev_entry *deve;

	nacl = lacl->se_lun_nacl;
	if (!nacl)
		return -EINVAL;

	mutex_lock(&nacl->lun_entry_mutex);
	deve = target_nacl_find_deve(nacl, lacl->mapped_lun);
	if (deve)
		core_disable_device_list_for_node(lun, deve, nacl, tpg);
	mutex_unlock(&nacl->lun_entry_mutex);

	pr_debug("%s_TPG[%hu]_LUN[%llu] - Removed ACL for"
		" InitiatorNode: %s Mapped LUN: %llu\n",
		tpg->se_tpg_tfo->fabric_name,
		tpg->se_tpg_tfo->tpg_get_tag(tpg), lun->unpacked_lun,
		nacl->initiatorname, lacl->mapped_lun);

	return 0;
}

void core_dev_free_initiator_node_lun_acl(
	struct se_portal_group *tpg,
	struct se_lun_acl *lacl)
{
	pr_debug("%s_TPG[%hu] - Freeing ACL for %s InitiatorNode: %s"
		" Mapped LUN: %llu\n", tpg->se_tpg_tfo->fabric_name,
		tpg->se_tpg_tfo->tpg_get_tag(tpg),
		tpg->se_tpg_tfo->fabric_name,
		lacl->se_lun_nacl->initiatorname, lacl->mapped_lun);

	kfree(lacl);
}

static void scsi_dump_inquiry(struct se_device *dev)
{
	struct t10_wwn *wwn = &dev->t10_wwn;
	int device_type = dev->transport->get_device_type(dev);

	 
	pr_debug("  Vendor: %-" __stringify(INQUIRY_VENDOR_LEN) "s\n",
		wwn->vendor);
	pr_debug("  Model: %-" __stringify(INQUIRY_MODEL_LEN) "s\n",
		wwn->model);
	pr_debug("  Revision: %-" __stringify(INQUIRY_REVISION_LEN) "s\n",
		wwn->revision);
	pr_debug("  Type:   %s ", scsi_device_type(device_type));
}

struct se_device *target_alloc_device(struct se_hba *hba, const char *name)
{
	struct se_device *dev;
	struct se_lun *xcopy_lun;
	int i;

	dev = hba->backend->ops->alloc_device(hba, name);
	if (!dev)
		return NULL;

	dev->queues = kcalloc(nr_cpu_ids, sizeof(*dev->queues), GFP_KERNEL);
	if (!dev->queues) {
		dev->transport->free_device(dev);
		return NULL;
	}

	dev->queue_cnt = nr_cpu_ids;
	for (i = 0; i < dev->queue_cnt; i++) {
		struct se_device_queue *q;

		q = &dev->queues[i];
		INIT_LIST_HEAD(&q->state_list);
		spin_lock_init(&q->lock);

		init_llist_head(&q->sq.cmd_list);
		INIT_WORK(&q->sq.work, target_queued_submit_work);
	}

	dev->se_hba = hba;
	dev->transport = hba->backend->ops;
	dev->transport_flags = dev->transport->transport_flags_default;
	dev->prot_length = sizeof(struct t10_pi_tuple);
	dev->hba_index = hba->hba_index;

	INIT_LIST_HEAD(&dev->dev_sep_list);
	INIT_LIST_HEAD(&dev->dev_tmr_list);
	INIT_LIST_HEAD(&dev->delayed_cmd_list);
	INIT_LIST_HEAD(&dev->qf_cmd_list);
	spin_lock_init(&dev->delayed_cmd_lock);
	spin_lock_init(&dev->dev_reservation_lock);
	spin_lock_init(&dev->se_port_lock);
	spin_lock_init(&dev->se_tmr_lock);
	spin_lock_init(&dev->qf_cmd_lock);
	sema_init(&dev->caw_sem, 1);
	INIT_LIST_HEAD(&dev->t10_wwn.t10_vpd_list);
	spin_lock_init(&dev->t10_wwn.t10_vpd_lock);
	INIT_LIST_HEAD(&dev->t10_pr.registration_list);
	INIT_LIST_HEAD(&dev->t10_pr.aptpl_reg_list);
	spin_lock_init(&dev->t10_pr.registration_lock);
	spin_lock_init(&dev->t10_pr.aptpl_reg_lock);
	INIT_LIST_HEAD(&dev->t10_alua.tg_pt_gps_list);
	spin_lock_init(&dev->t10_alua.tg_pt_gps_lock);
	INIT_LIST_HEAD(&dev->t10_alua.lba_map_list);
	spin_lock_init(&dev->t10_alua.lba_map_lock);

	INIT_WORK(&dev->delayed_cmd_work, target_do_delayed_work);
	mutex_init(&dev->lun_reset_mutex);

	dev->t10_wwn.t10_dev = dev;
	 
	dev->t10_wwn.company_id = 0x001405;

	dev->t10_alua.t10_dev = dev;

	dev->dev_attrib.da_dev = dev;
	dev->dev_attrib.emulate_model_alias = DA_EMULATE_MODEL_ALIAS;
	dev->dev_attrib.emulate_dpo = 1;
	dev->dev_attrib.emulate_fua_write = 1;
	dev->dev_attrib.emulate_fua_read = 1;
	dev->dev_attrib.emulate_write_cache = DA_EMULATE_WRITE_CACHE;
	dev->dev_attrib.emulate_ua_intlck_ctrl = TARGET_UA_INTLCK_CTRL_CLEAR;
	dev->dev_attrib.emulate_tas = DA_EMULATE_TAS;
	dev->dev_attrib.emulate_tpu = DA_EMULATE_TPU;
	dev->dev_attrib.emulate_tpws = DA_EMULATE_TPWS;
	dev->dev_attrib.emulate_caw = DA_EMULATE_CAW;
	dev->dev_attrib.emulate_3pc = DA_EMULATE_3PC;
	dev->dev_attrib.emulate_pr = DA_EMULATE_PR;
	dev->dev_attrib.emulate_rsoc = DA_EMULATE_RSOC;
	dev->dev_attrib.pi_prot_type = TARGET_DIF_TYPE0_PROT;
	dev->dev_attrib.enforce_pr_isids = DA_ENFORCE_PR_ISIDS;
	dev->dev_attrib.force_pr_aptpl = DA_FORCE_PR_APTPL;
	dev->dev_attrib.is_nonrot = DA_IS_NONROT;
	dev->dev_attrib.emulate_rest_reord = DA_EMULATE_REST_REORD;
	dev->dev_attrib.max_unmap_lba_count = DA_MAX_UNMAP_LBA_COUNT;
	dev->dev_attrib.max_unmap_block_desc_count =
		DA_MAX_UNMAP_BLOCK_DESC_COUNT;
	dev->dev_attrib.unmap_granularity = DA_UNMAP_GRANULARITY_DEFAULT;
	dev->dev_attrib.unmap_granularity_alignment =
				DA_UNMAP_GRANULARITY_ALIGNMENT_DEFAULT;
	dev->dev_attrib.unmap_zeroes_data =
				DA_UNMAP_ZEROES_DATA_DEFAULT;
	dev->dev_attrib.max_write_same_len = DA_MAX_WRITE_SAME_LEN;

	xcopy_lun = &dev->xcopy_lun;
	rcu_assign_pointer(xcopy_lun->lun_se_dev, dev);
	init_completion(&xcopy_lun->lun_shutdown_comp);
	INIT_LIST_HEAD(&xcopy_lun->lun_deve_list);
	INIT_LIST_HEAD(&xcopy_lun->lun_dev_link);
	mutex_init(&xcopy_lun->lun_tg_pt_md_mutex);
	xcopy_lun->lun_tpg = &xcopy_pt_tpg;

	 
	strscpy(dev->t10_wwn.vendor, "LIO-ORG", sizeof(dev->t10_wwn.vendor));
	strscpy(dev->t10_wwn.model, dev->transport->inquiry_prod,
		sizeof(dev->t10_wwn.model));
	strscpy(dev->t10_wwn.revision, dev->transport->inquiry_rev,
		sizeof(dev->t10_wwn.revision));

	return dev;
}

 
bool target_configure_unmap_from_queue(struct se_dev_attrib *attrib,
				       struct block_device *bdev)
{
	int block_size = bdev_logical_block_size(bdev);

	if (!bdev_max_discard_sectors(bdev))
		return false;

	attrib->max_unmap_lba_count =
		bdev_max_discard_sectors(bdev) >> (ilog2(block_size) - 9);
	 
	attrib->max_unmap_block_desc_count = 1;
	attrib->unmap_granularity = bdev_discard_granularity(bdev) / block_size;
	attrib->unmap_granularity_alignment =
		bdev_discard_alignment(bdev) / block_size;
	return true;
}
EXPORT_SYMBOL(target_configure_unmap_from_queue);

 
sector_t target_to_linux_sector(struct se_device *dev, sector_t lb)
{
	switch (dev->dev_attrib.block_size) {
	case 4096:
		return lb << 3;
	case 2048:
		return lb << 2;
	case 1024:
		return lb << 1;
	default:
		return lb;
	}
}
EXPORT_SYMBOL(target_to_linux_sector);

struct devices_idr_iter {
	int (*fn)(struct se_device *dev, void *data);
	void *data;
};

static int target_devices_idr_iter(int id, void *p, void *data)
	 __must_hold(&device_mutex)
{
	struct devices_idr_iter *iter = data;
	struct se_device *dev = p;
	struct config_item *item;
	int ret;

	 
	if (!target_dev_configured(dev))
		return 0;

	item = config_item_get_unless_zero(&dev->dev_group.cg_item);
	if (!item)
		return 0;
	mutex_unlock(&device_mutex);

	ret = iter->fn(dev, iter->data);
	config_item_put(item);

	mutex_lock(&device_mutex);
	return ret;
}

 
int target_for_each_device(int (*fn)(struct se_device *dev, void *data),
			   void *data)
{
	struct devices_idr_iter iter = { .fn = fn, .data = data };
	int ret;

	mutex_lock(&device_mutex);
	ret = idr_for_each(&devices_idr, target_devices_idr_iter, &iter);
	mutex_unlock(&device_mutex);
	return ret;
}

int target_configure_device(struct se_device *dev)
{
	struct se_hba *hba = dev->se_hba;
	int ret, id;

	if (target_dev_configured(dev)) {
		pr_err("se_dev->se_dev_ptr already set for storage"
				" object\n");
		return -EEXIST;
	}

	 
	mutex_lock(&device_mutex);
	 
	id = idr_alloc_cyclic(&devices_idr, dev, 0, INT_MAX, GFP_KERNEL);
	mutex_unlock(&device_mutex);
	if (id < 0) {
		ret = -ENOMEM;
		goto out;
	}
	dev->dev_index = id;

	ret = dev->transport->configure_device(dev);
	if (ret)
		goto out_free_index;

	if (dev->transport->configure_unmap &&
	    dev->transport->configure_unmap(dev)) {
		pr_debug("Discard support available, but disabled by default.\n");
	}

	 
	dev->dev_attrib.block_size = dev->dev_attrib.hw_block_size;
	dev->dev_attrib.queue_depth = dev->dev_attrib.hw_queue_depth;

	 
	dev->dev_attrib.hw_max_sectors =
		se_dev_align_max_sectors(dev->dev_attrib.hw_max_sectors,
					 dev->dev_attrib.hw_block_size);
	dev->dev_attrib.optimal_sectors = dev->dev_attrib.hw_max_sectors;

	dev->creation_time = get_jiffies_64();

	ret = core_setup_alua(dev);
	if (ret)
		goto out_destroy_device;

	 
	INIT_WORK(&dev->qf_work_queue, target_qf_do_work);

	scsi_dump_inquiry(dev);

	spin_lock(&hba->device_lock);
	hba->dev_count++;
	spin_unlock(&hba->device_lock);

	dev->dev_flags |= DF_CONFIGURED;

	return 0;

out_destroy_device:
	dev->transport->destroy_device(dev);
out_free_index:
	mutex_lock(&device_mutex);
	idr_remove(&devices_idr, dev->dev_index);
	mutex_unlock(&device_mutex);
out:
	se_release_vpd_for_dev(dev);
	return ret;
}

void target_free_device(struct se_device *dev)
{
	struct se_hba *hba = dev->se_hba;

	WARN_ON(!list_empty(&dev->dev_sep_list));

	if (target_dev_configured(dev)) {
		dev->transport->destroy_device(dev);

		mutex_lock(&device_mutex);
		idr_remove(&devices_idr, dev->dev_index);
		mutex_unlock(&device_mutex);

		spin_lock(&hba->device_lock);
		hba->dev_count--;
		spin_unlock(&hba->device_lock);
	}

	core_alua_free_lu_gp_mem(dev);
	core_alua_set_lba_map(dev, NULL, 0, 0);
	core_scsi3_free_all_registrations(dev);
	se_release_vpd_for_dev(dev);

	if (dev->transport->free_prot)
		dev->transport->free_prot(dev);

	kfree(dev->queues);
	dev->transport->free_device(dev);
}

int core_dev_setup_virtual_lun0(void)
{
	struct se_hba *hba;
	struct se_device *dev;
	char buf[] = "rd_pages=8,rd_nullio=1,rd_dummy=1";
	int ret;

	hba = core_alloc_hba("rd_mcp", 0, HBA_FLAGS_INTERNAL_USE);
	if (IS_ERR(hba))
		return PTR_ERR(hba);

	dev = target_alloc_device(hba, "virt_lun0");
	if (!dev) {
		ret = -ENOMEM;
		goto out_free_hba;
	}

	hba->backend->ops->set_configfs_dev_params(dev, buf, sizeof(buf));

	ret = target_configure_device(dev);
	if (ret)
		goto out_free_se_dev;

	lun0_hba = hba;
	g_lun0_dev = dev;
	return 0;

out_free_se_dev:
	target_free_device(dev);
out_free_hba:
	core_delete_hba(hba);
	return ret;
}


void core_dev_release_virtual_lun0(void)
{
	struct se_hba *hba = lun0_hba;

	if (!hba)
		return;

	if (g_lun0_dev)
		target_free_device(g_lun0_dev);
	core_delete_hba(hba);
}

 
sense_reason_t
passthrough_parse_cdb(struct se_cmd *cmd,
	sense_reason_t (*exec_cmd)(struct se_cmd *cmd))
{
	unsigned char *cdb = cmd->t_task_cdb;
	struct se_device *dev = cmd->se_dev;
	unsigned int size;

	 
	if (cdb[0] == REPORT_LUNS) {
		cmd->execute_cmd = spc_emulate_report_luns;
		return TCM_NO_SENSE;
	}

	 
	if (!dev->dev_attrib.emulate_pr &&
	    ((cdb[0] == PERSISTENT_RESERVE_IN) ||
	     (cdb[0] == PERSISTENT_RESERVE_OUT) ||
	     (cdb[0] == RELEASE || cdb[0] == RELEASE_10) ||
	     (cdb[0] == RESERVE || cdb[0] == RESERVE_10))) {
		return TCM_UNSUPPORTED_SCSI_OPCODE;
	}

	 
	if (!(dev->transport_flags &
	      TRANSPORT_FLAG_PASSTHROUGH_PGR)) {
		if (cdb[0] == PERSISTENT_RESERVE_IN) {
			cmd->execute_cmd = target_scsi3_emulate_pr_in;
			size = get_unaligned_be16(&cdb[7]);
			return target_cmd_size_check(cmd, size);
		}
		if (cdb[0] == PERSISTENT_RESERVE_OUT) {
			cmd->execute_cmd = target_scsi3_emulate_pr_out;
			size = get_unaligned_be32(&cdb[5]);
			return target_cmd_size_check(cmd, size);
		}

		if (cdb[0] == RELEASE || cdb[0] == RELEASE_10) {
			cmd->execute_cmd = target_scsi2_reservation_release;
			if (cdb[0] == RELEASE_10)
				size = get_unaligned_be16(&cdb[7]);
			else
				size = cmd->data_length;
			return target_cmd_size_check(cmd, size);
		}
		if (cdb[0] == RESERVE || cdb[0] == RESERVE_10) {
			cmd->execute_cmd = target_scsi2_reservation_reserve;
			if (cdb[0] == RESERVE_10)
				size = get_unaligned_be16(&cdb[7]);
			else
				size = cmd->data_length;
			return target_cmd_size_check(cmd, size);
		}
	}

	 
	switch (cdb[0]) {
	case READ_6:
	case READ_10:
	case READ_12:
	case READ_16:
	case WRITE_6:
	case WRITE_10:
	case WRITE_12:
	case WRITE_16:
	case WRITE_VERIFY:
	case WRITE_VERIFY_12:
	case WRITE_VERIFY_16:
	case COMPARE_AND_WRITE:
	case XDWRITEREAD_10:
		cmd->se_cmd_flags |= SCF_SCSI_DATA_CDB;
		break;
	case VARIABLE_LENGTH_CMD:
		switch (get_unaligned_be16(&cdb[8])) {
		case READ_32:
		case WRITE_32:
		case WRITE_VERIFY_32:
		case XDWRITEREAD_32:
			cmd->se_cmd_flags |= SCF_SCSI_DATA_CDB;
			break;
		}
	}

	cmd->execute_cmd = exec_cmd;

	return TCM_NO_SENSE;
}
EXPORT_SYMBOL(passthrough_parse_cdb);
