
 

#define pr_fmt(fmt) "SafeSetID: " fmt

#include <linux/lsm_hooks.h>
#include <linux/module.h>
#include <linux/ptrace.h>
#include <linux/sched/task_stack.h>
#include <linux/security.h>
#include "lsm.h"

 
int safesetid_initialized __initdata;

struct setid_ruleset __rcu *safesetid_setuid_rules;
struct setid_ruleset __rcu *safesetid_setgid_rules;


 
enum sid_policy_type _setid_policy_lookup(struct setid_ruleset *policy,
		kid_t src, kid_t dst)
{
	struct setid_rule *rule;
	enum sid_policy_type result = SIDPOL_DEFAULT;

	if (policy->type == UID) {
		hash_for_each_possible(policy->rules, rule, next, __kuid_val(src.uid)) {
			if (!uid_eq(rule->src_id.uid, src.uid))
				continue;
			if (uid_eq(rule->dst_id.uid, dst.uid))
				return SIDPOL_ALLOWED;
			result = SIDPOL_CONSTRAINED;
		}
	} else if (policy->type == GID) {
		hash_for_each_possible(policy->rules, rule, next, __kgid_val(src.gid)) {
			if (!gid_eq(rule->src_id.gid, src.gid))
				continue;
			if (gid_eq(rule->dst_id.gid, dst.gid)){
				return SIDPOL_ALLOWED;
			}
			result = SIDPOL_CONSTRAINED;
		}
	} else {
		 
		result = SIDPOL_CONSTRAINED;
	}
	return result;
}

 
static enum sid_policy_type setid_policy_lookup(kid_t src, kid_t dst, enum setid_type new_type)
{
	enum sid_policy_type result = SIDPOL_DEFAULT;
	struct setid_ruleset *pol;

	rcu_read_lock();
	if (new_type == UID)
		pol = rcu_dereference(safesetid_setuid_rules);
	else if (new_type == GID)
		pol = rcu_dereference(safesetid_setgid_rules);
	else {  
		result = SIDPOL_CONSTRAINED;
		rcu_read_unlock();
		return result;
	}

	if (pol) {
		pol->type = new_type;
		result = _setid_policy_lookup(pol, src, dst);
	}
	rcu_read_unlock();
	return result;
}

static int safesetid_security_capable(const struct cred *cred,
				      struct user_namespace *ns,
				      int cap,
				      unsigned int opts)
{
	 
	if (cap != CAP_SETUID && cap != CAP_SETGID)
		return 0;

	 
	if ((opts & CAP_OPT_INSETID) != 0)
		return 0;

	switch (cap) {
	case CAP_SETUID:
		 
		if (setid_policy_lookup((kid_t){.uid = cred->uid}, INVALID_ID, UID) == SIDPOL_DEFAULT)
			return 0;
		 
		pr_warn("Operation requires CAP_SETUID, which is not available to UID %u for operations besides approved set*uid transitions\n",
			__kuid_val(cred->uid));
		return -EPERM;
	case CAP_SETGID:
		 
		if (setid_policy_lookup((kid_t){.gid = cred->gid}, INVALID_ID, GID) == SIDPOL_DEFAULT)
			return 0;
		 
		pr_warn("Operation requires CAP_SETGID, which is not available to GID %u for operations besides approved set*gid transitions\n",
			__kgid_val(cred->gid));
		return -EPERM;
	default:
		 
		return 0;
	}
	return 0;
}

 
static bool id_permitted_for_cred(const struct cred *old, kid_t new_id, enum setid_type new_type)
{
	bool permitted;

	 
	if (new_type == UID) {
		if (uid_eq(new_id.uid, old->uid) || uid_eq(new_id.uid, old->euid) ||
			uid_eq(new_id.uid, old->suid))
			return true;
	} else if (new_type == GID){
		if (gid_eq(new_id.gid, old->gid) || gid_eq(new_id.gid, old->egid) ||
			gid_eq(new_id.gid, old->sgid))
			return true;
	} else  
		return false;

	 
	permitted =
	    setid_policy_lookup((kid_t){.uid = old->uid}, new_id, new_type) != SIDPOL_CONSTRAINED;

	if (!permitted) {
		if (new_type == UID) {
			pr_warn("UID transition ((%d,%d,%d) -> %d) blocked\n",
				__kuid_val(old->uid), __kuid_val(old->euid),
				__kuid_val(old->suid), __kuid_val(new_id.uid));
		} else if (new_type == GID) {
			pr_warn("GID transition ((%d,%d,%d) -> %d) blocked\n",
				__kgid_val(old->gid), __kgid_val(old->egid),
				__kgid_val(old->sgid), __kgid_val(new_id.gid));
		} else  
			return false;
	}
	return permitted;
}

 
static int safesetid_task_fix_setuid(struct cred *new,
				     const struct cred *old,
				     int flags)
{

	 
	if (setid_policy_lookup((kid_t){.uid = old->uid}, INVALID_ID, UID) == SIDPOL_DEFAULT)
		return 0;

	if (id_permitted_for_cred(old, (kid_t){.uid = new->uid}, UID) &&
	    id_permitted_for_cred(old, (kid_t){.uid = new->euid}, UID) &&
	    id_permitted_for_cred(old, (kid_t){.uid = new->suid}, UID) &&
	    id_permitted_for_cred(old, (kid_t){.uid = new->fsuid}, UID))
		return 0;

	 
	force_sig(SIGKILL);
	return -EACCES;
}

static int safesetid_task_fix_setgid(struct cred *new,
				     const struct cred *old,
				     int flags)
{

	 
	if (setid_policy_lookup((kid_t){.gid = old->gid}, INVALID_ID, GID) == SIDPOL_DEFAULT)
		return 0;

	if (id_permitted_for_cred(old, (kid_t){.gid = new->gid}, GID) &&
	    id_permitted_for_cred(old, (kid_t){.gid = new->egid}, GID) &&
	    id_permitted_for_cred(old, (kid_t){.gid = new->sgid}, GID) &&
	    id_permitted_for_cred(old, (kid_t){.gid = new->fsgid}, GID))
		return 0;

	 
	force_sig(SIGKILL);
	return -EACCES;
}

static int safesetid_task_fix_setgroups(struct cred *new, const struct cred *old)
{
	int i;

	 
	if (setid_policy_lookup((kid_t){.gid = old->gid}, INVALID_ID, GID) == SIDPOL_DEFAULT)
		return 0;

	get_group_info(new->group_info);
	for (i = 0; i < new->group_info->ngroups; i++) {
		if (!id_permitted_for_cred(old, (kid_t){.gid = new->group_info->gid[i]}, GID)) {
			put_group_info(new->group_info);
			 
			force_sig(SIGKILL);
			return -EACCES;
		}
	}

	put_group_info(new->group_info);
	return 0;
}

static struct security_hook_list safesetid_security_hooks[] = {
	LSM_HOOK_INIT(task_fix_setuid, safesetid_task_fix_setuid),
	LSM_HOOK_INIT(task_fix_setgid, safesetid_task_fix_setgid),
	LSM_HOOK_INIT(task_fix_setgroups, safesetid_task_fix_setgroups),
	LSM_HOOK_INIT(capable, safesetid_security_capable)
};

static int __init safesetid_security_init(void)
{
	security_add_hooks(safesetid_security_hooks,
			   ARRAY_SIZE(safesetid_security_hooks), "safesetid");

	 
	safesetid_initialized = 1;

	return 0;
}

DEFINE_LSM(safesetid_security_init) = {
	.init = safesetid_security_init,
	.name = "safesetid",
};
