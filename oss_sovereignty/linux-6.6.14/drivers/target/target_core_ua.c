
 

#include <linux/slab.h>
#include <linux/spinlock.h>
#include <scsi/scsi_proto.h>

#include <target/target_core_base.h>
#include <target/target_core_fabric.h>

#include "target_core_internal.h"
#include "target_core_alua.h"
#include "target_core_pr.h"
#include "target_core_ua.h"

sense_reason_t
target_scsi3_ua_check(struct se_cmd *cmd)
{
	struct se_dev_entry *deve;
	struct se_session *sess = cmd->se_sess;
	struct se_node_acl *nacl;

	if (!sess)
		return 0;

	nacl = sess->se_node_acl;
	if (!nacl)
		return 0;

	rcu_read_lock();
	deve = target_nacl_find_deve(nacl, cmd->orig_fe_lun);
	if (!deve) {
		rcu_read_unlock();
		return 0;
	}
	if (list_empty_careful(&deve->ua_list)) {
		rcu_read_unlock();
		return 0;
	}
	rcu_read_unlock();
	 
	switch (cmd->t_task_cdb[0]) {
	case INQUIRY:
	case REPORT_LUNS:
	case REQUEST_SENSE:
		return 0;
	default:
		return TCM_CHECK_CONDITION_UNIT_ATTENTION;
	}
}

int core_scsi3_ua_allocate(
	struct se_dev_entry *deve,
	u8 asc,
	u8 ascq)
{
	struct se_ua *ua, *ua_p, *ua_tmp;

	ua = kmem_cache_zalloc(se_ua_cache, GFP_ATOMIC);
	if (!ua) {
		pr_err("Unable to allocate struct se_ua\n");
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&ua->ua_nacl_list);

	ua->ua_asc = asc;
	ua->ua_ascq = ascq;

	spin_lock(&deve->ua_lock);
	list_for_each_entry_safe(ua_p, ua_tmp, &deve->ua_list, ua_nacl_list) {
		 
		if ((ua_p->ua_asc == asc) && (ua_p->ua_ascq == ascq)) {
			spin_unlock(&deve->ua_lock);
			kmem_cache_free(se_ua_cache, ua);
			return 0;
		}
		 
		if (ua_p->ua_asc == 0x29) {
			if ((asc == 0x29) && (ascq > ua_p->ua_ascq))
				list_add(&ua->ua_nacl_list,
						&deve->ua_list);
			else
				list_add_tail(&ua->ua_nacl_list,
						&deve->ua_list);
		} else if (ua_p->ua_asc == 0x2a) {
			 
			if ((asc == 0x29) || (ascq > ua_p->ua_asc))
				list_add(&ua->ua_nacl_list,
					&deve->ua_list);
			else
				list_add_tail(&ua->ua_nacl_list,
						&deve->ua_list);
		} else
			list_add_tail(&ua->ua_nacl_list,
				&deve->ua_list);
		spin_unlock(&deve->ua_lock);

		return 0;
	}
	list_add_tail(&ua->ua_nacl_list, &deve->ua_list);
	spin_unlock(&deve->ua_lock);

	pr_debug("Allocated UNIT ATTENTION, mapped LUN: %llu, ASC:"
		" 0x%02x, ASCQ: 0x%02x\n", deve->mapped_lun,
		asc, ascq);

	return 0;
}

void target_ua_allocate_lun(struct se_node_acl *nacl,
			    u32 unpacked_lun, u8 asc, u8 ascq)
{
	struct se_dev_entry *deve;

	if (!nacl)
		return;

	rcu_read_lock();
	deve = target_nacl_find_deve(nacl, unpacked_lun);
	if (!deve) {
		rcu_read_unlock();
		return;
	}

	core_scsi3_ua_allocate(deve, asc, ascq);
	rcu_read_unlock();
}

void core_scsi3_ua_release_all(
	struct se_dev_entry *deve)
{
	struct se_ua *ua, *ua_p;

	spin_lock(&deve->ua_lock);
	list_for_each_entry_safe(ua, ua_p, &deve->ua_list, ua_nacl_list) {
		list_del(&ua->ua_nacl_list);
		kmem_cache_free(se_ua_cache, ua);
	}
	spin_unlock(&deve->ua_lock);
}

 
bool core_scsi3_ua_for_check_condition(struct se_cmd *cmd, u8 *key, u8 *asc,
				       u8 *ascq)
{
	struct se_device *dev = cmd->se_dev;
	struct se_dev_entry *deve;
	struct se_session *sess = cmd->se_sess;
	struct se_node_acl *nacl;
	struct se_ua *ua = NULL, *ua_p;
	int head = 1;
	bool dev_ua_intlck_clear = (dev->dev_attrib.emulate_ua_intlck_ctrl
						== TARGET_UA_INTLCK_CTRL_CLEAR);

	if (WARN_ON_ONCE(!sess))
		return false;

	nacl = sess->se_node_acl;
	if (WARN_ON_ONCE(!nacl))
		return false;

	rcu_read_lock();
	deve = target_nacl_find_deve(nacl, cmd->orig_fe_lun);
	if (!deve) {
		rcu_read_unlock();
		*key = ILLEGAL_REQUEST;
		*asc = 0x25;  
		*ascq = 0;
		return true;
	}
	*key = UNIT_ATTENTION;
	 
	spin_lock(&deve->ua_lock);
	list_for_each_entry_safe(ua, ua_p, &deve->ua_list, ua_nacl_list) {
		 
		if (!dev_ua_intlck_clear) {
			*asc = ua->ua_asc;
			*ascq = ua->ua_ascq;
			break;
		}
		 
		if (head) {
			*asc = ua->ua_asc;
			*ascq = ua->ua_ascq;
			head = 0;
		}
		list_del(&ua->ua_nacl_list);
		kmem_cache_free(se_ua_cache, ua);
	}
	spin_unlock(&deve->ua_lock);
	rcu_read_unlock();

	pr_debug("[%s]: %s UNIT ATTENTION condition with"
		" INTLCK_CTRL: %d, mapped LUN: %llu, got CDB: 0x%02x"
		" reported ASC: 0x%02x, ASCQ: 0x%02x\n",
		nacl->se_tpg->se_tpg_tfo->fabric_name,
		dev_ua_intlck_clear ? "Releasing" : "Reporting",
		dev->dev_attrib.emulate_ua_intlck_ctrl,
		cmd->orig_fe_lun, cmd->t_task_cdb[0], *asc, *ascq);

	return head == 0;
}

int core_scsi3_ua_clear_for_request_sense(
	struct se_cmd *cmd,
	u8 *asc,
	u8 *ascq)
{
	struct se_dev_entry *deve;
	struct se_session *sess = cmd->se_sess;
	struct se_node_acl *nacl;
	struct se_ua *ua = NULL, *ua_p;
	int head = 1;

	if (!sess)
		return -EINVAL;

	nacl = sess->se_node_acl;
	if (!nacl)
		return -EINVAL;

	rcu_read_lock();
	deve = target_nacl_find_deve(nacl, cmd->orig_fe_lun);
	if (!deve) {
		rcu_read_unlock();
		return -EINVAL;
	}
	if (list_empty_careful(&deve->ua_list)) {
		rcu_read_unlock();
		return -EPERM;
	}
	 
	spin_lock(&deve->ua_lock);
	list_for_each_entry_safe(ua, ua_p, &deve->ua_list, ua_nacl_list) {
		if (head) {
			*asc = ua->ua_asc;
			*ascq = ua->ua_ascq;
			head = 0;
		}
		list_del(&ua->ua_nacl_list);
		kmem_cache_free(se_ua_cache, ua);
	}
	spin_unlock(&deve->ua_lock);
	rcu_read_unlock();

	pr_debug("[%s]: Released UNIT ATTENTION condition, mapped"
		" LUN: %llu, got REQUEST_SENSE reported ASC: 0x%02x,"
		" ASCQ: 0x%02x\n", nacl->se_tpg->se_tpg_tfo->fabric_name,
		cmd->orig_fe_lun, *asc, *ascq);

	return (head) ? -EPERM : 0;
}
