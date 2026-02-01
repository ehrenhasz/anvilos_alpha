
 

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/cacheinfo.h>
#include <linux/cpu.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/fs_parser.h>
#include <linux/sysfs.h>
#include <linux/kernfs.h>
#include <linux/seq_buf.h>
#include <linux/seq_file.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <linux/task_work.h>
#include <linux/user_namespace.h>

#include <uapi/linux/magic.h>

#include <asm/resctrl.h>
#include "internal.h"

DEFINE_STATIC_KEY_FALSE(rdt_enable_key);
DEFINE_STATIC_KEY_FALSE(rdt_mon_enable_key);
DEFINE_STATIC_KEY_FALSE(rdt_alloc_enable_key);
static struct kernfs_root *rdt_root;
struct rdtgroup rdtgroup_default;
LIST_HEAD(rdt_all_groups);

 
LIST_HEAD(resctrl_schema_all);

 
static struct kernfs_node *kn_info;

 
static struct kernfs_node *kn_mongrp;

 
static struct kernfs_node *kn_mondata;

static struct seq_buf last_cmd_status;
static char last_cmd_status_buf[512];

struct dentry *debugfs_resctrl;

void rdt_last_cmd_clear(void)
{
	lockdep_assert_held(&rdtgroup_mutex);
	seq_buf_clear(&last_cmd_status);
}

void rdt_last_cmd_puts(const char *s)
{
	lockdep_assert_held(&rdtgroup_mutex);
	seq_buf_puts(&last_cmd_status, s);
}

void rdt_last_cmd_printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	lockdep_assert_held(&rdtgroup_mutex);
	seq_buf_vprintf(&last_cmd_status, fmt, ap);
	va_end(ap);
}

void rdt_staged_configs_clear(void)
{
	struct rdt_resource *r;
	struct rdt_domain *dom;

	lockdep_assert_held(&rdtgroup_mutex);

	for_each_alloc_capable_rdt_resource(r) {
		list_for_each_entry(dom, &r->domains, list)
			memset(dom->staged_config, 0, sizeof(dom->staged_config));
	}
}

 
static int closid_free_map;
static int closid_free_map_len;

int closids_supported(void)
{
	return closid_free_map_len;
}

static void closid_init(void)
{
	struct resctrl_schema *s;
	u32 rdt_min_closid = 32;

	 
	list_for_each_entry(s, &resctrl_schema_all, list)
		rdt_min_closid = min(rdt_min_closid, s->num_closid);

	closid_free_map = BIT_MASK(rdt_min_closid) - 1;

	 
	closid_free_map &= ~1;
	closid_free_map_len = rdt_min_closid;
}

static int closid_alloc(void)
{
	u32 closid = ffs(closid_free_map);

	if (closid == 0)
		return -ENOSPC;
	closid--;
	closid_free_map &= ~(1 << closid);

	return closid;
}

void closid_free(int closid)
{
	closid_free_map |= 1 << closid;
}

 
static bool closid_allocated(unsigned int closid)
{
	return (closid_free_map & (1 << closid)) == 0;
}

 
enum rdtgrp_mode rdtgroup_mode_by_closid(int closid)
{
	struct rdtgroup *rdtgrp;

	list_for_each_entry(rdtgrp, &rdt_all_groups, rdtgroup_list) {
		if (rdtgrp->closid == closid)
			return rdtgrp->mode;
	}

	return RDT_NUM_MODES;
}

static const char * const rdt_mode_str[] = {
	[RDT_MODE_SHAREABLE]		= "shareable",
	[RDT_MODE_EXCLUSIVE]		= "exclusive",
	[RDT_MODE_PSEUDO_LOCKSETUP]	= "pseudo-locksetup",
	[RDT_MODE_PSEUDO_LOCKED]	= "pseudo-locked",
};

 
static const char *rdtgroup_mode_str(enum rdtgrp_mode mode)
{
	if (mode < RDT_MODE_SHAREABLE || mode >= RDT_NUM_MODES)
		return "unknown";

	return rdt_mode_str[mode];
}

 
static int rdtgroup_kn_set_ugid(struct kernfs_node *kn)
{
	struct iattr iattr = { .ia_valid = ATTR_UID | ATTR_GID,
				.ia_uid = current_fsuid(),
				.ia_gid = current_fsgid(), };

	if (uid_eq(iattr.ia_uid, GLOBAL_ROOT_UID) &&
	    gid_eq(iattr.ia_gid, GLOBAL_ROOT_GID))
		return 0;

	return kernfs_setattr(kn, &iattr);
}

static int rdtgroup_add_file(struct kernfs_node *parent_kn, struct rftype *rft)
{
	struct kernfs_node *kn;
	int ret;

	kn = __kernfs_create_file(parent_kn, rft->name, rft->mode,
				  GLOBAL_ROOT_UID, GLOBAL_ROOT_GID,
				  0, rft->kf_ops, rft, NULL, NULL);
	if (IS_ERR(kn))
		return PTR_ERR(kn);

	ret = rdtgroup_kn_set_ugid(kn);
	if (ret) {
		kernfs_remove(kn);
		return ret;
	}

	return 0;
}

static int rdtgroup_seqfile_show(struct seq_file *m, void *arg)
{
	struct kernfs_open_file *of = m->private;
	struct rftype *rft = of->kn->priv;

	if (rft->seq_show)
		return rft->seq_show(of, m, arg);
	return 0;
}

static ssize_t rdtgroup_file_write(struct kernfs_open_file *of, char *buf,
				   size_t nbytes, loff_t off)
{
	struct rftype *rft = of->kn->priv;

	if (rft->write)
		return rft->write(of, buf, nbytes, off);

	return -EINVAL;
}

static const struct kernfs_ops rdtgroup_kf_single_ops = {
	.atomic_write_len	= PAGE_SIZE,
	.write			= rdtgroup_file_write,
	.seq_show		= rdtgroup_seqfile_show,
};

static const struct kernfs_ops kf_mondata_ops = {
	.atomic_write_len	= PAGE_SIZE,
	.seq_show		= rdtgroup_mondata_show,
};

static bool is_cpu_list(struct kernfs_open_file *of)
{
	struct rftype *rft = of->kn->priv;

	return rft->flags & RFTYPE_FLAGS_CPUS_LIST;
}

static int rdtgroup_cpus_show(struct kernfs_open_file *of,
			      struct seq_file *s, void *v)
{
	struct rdtgroup *rdtgrp;
	struct cpumask *mask;
	int ret = 0;

	rdtgrp = rdtgroup_kn_lock_live(of->kn);

	if (rdtgrp) {
		if (rdtgrp->mode == RDT_MODE_PSEUDO_LOCKED) {
			if (!rdtgrp->plr->d) {
				rdt_last_cmd_clear();
				rdt_last_cmd_puts("Cache domain offline\n");
				ret = -ENODEV;
			} else {
				mask = &rdtgrp->plr->d->cpu_mask;
				seq_printf(s, is_cpu_list(of) ?
					   "%*pbl\n" : "%*pb\n",
					   cpumask_pr_args(mask));
			}
		} else {
			seq_printf(s, is_cpu_list(of) ? "%*pbl\n" : "%*pb\n",
				   cpumask_pr_args(&rdtgrp->cpu_mask));
		}
	} else {
		ret = -ENOENT;
	}
	rdtgroup_kn_unlock(of->kn);

	return ret;
}

 
static void update_cpu_closid_rmid(void *info)
{
	struct rdtgroup *r = info;

	if (r) {
		this_cpu_write(pqr_state.default_closid, r->closid);
		this_cpu_write(pqr_state.default_rmid, r->mon.rmid);
	}

	 
	resctrl_sched_in(current);
}

 
static void
update_closid_rmid(const struct cpumask *cpu_mask, struct rdtgroup *r)
{
	on_each_cpu_mask(cpu_mask, update_cpu_closid_rmid, r, 1);
}

static int cpus_mon_write(struct rdtgroup *rdtgrp, cpumask_var_t newmask,
			  cpumask_var_t tmpmask)
{
	struct rdtgroup *prgrp = rdtgrp->mon.parent, *crgrp;
	struct list_head *head;

	 
	cpumask_andnot(tmpmask, newmask, &prgrp->cpu_mask);
	if (!cpumask_empty(tmpmask)) {
		rdt_last_cmd_puts("Can only add CPUs to mongroup that belong to parent\n");
		return -EINVAL;
	}

	 
	cpumask_andnot(tmpmask, &rdtgrp->cpu_mask, newmask);
	if (!cpumask_empty(tmpmask)) {
		 
		cpumask_or(&prgrp->cpu_mask, &prgrp->cpu_mask, tmpmask);
		update_closid_rmid(tmpmask, prgrp);
	}

	 
	cpumask_andnot(tmpmask, newmask, &rdtgrp->cpu_mask);
	if (!cpumask_empty(tmpmask)) {
		head = &prgrp->mon.crdtgrp_list;
		list_for_each_entry(crgrp, head, mon.crdtgrp_list) {
			if (crgrp == rdtgrp)
				continue;
			cpumask_andnot(&crgrp->cpu_mask, &crgrp->cpu_mask,
				       tmpmask);
		}
		update_closid_rmid(tmpmask, rdtgrp);
	}

	 
	cpumask_copy(&rdtgrp->cpu_mask, newmask);

	return 0;
}

static void cpumask_rdtgrp_clear(struct rdtgroup *r, struct cpumask *m)
{
	struct rdtgroup *crgrp;

	cpumask_andnot(&r->cpu_mask, &r->cpu_mask, m);
	 
	list_for_each_entry(crgrp, &r->mon.crdtgrp_list, mon.crdtgrp_list)
		cpumask_and(&crgrp->cpu_mask, &r->cpu_mask, &crgrp->cpu_mask);
}

static int cpus_ctrl_write(struct rdtgroup *rdtgrp, cpumask_var_t newmask,
			   cpumask_var_t tmpmask, cpumask_var_t tmpmask1)
{
	struct rdtgroup *r, *crgrp;
	struct list_head *head;

	 
	cpumask_andnot(tmpmask, &rdtgrp->cpu_mask, newmask);
	if (!cpumask_empty(tmpmask)) {
		 
		if (rdtgrp == &rdtgroup_default) {
			rdt_last_cmd_puts("Can't drop CPUs from default group\n");
			return -EINVAL;
		}

		 
		cpumask_or(&rdtgroup_default.cpu_mask,
			   &rdtgroup_default.cpu_mask, tmpmask);
		update_closid_rmid(tmpmask, &rdtgroup_default);
	}

	 
	cpumask_andnot(tmpmask, newmask, &rdtgrp->cpu_mask);
	if (!cpumask_empty(tmpmask)) {
		list_for_each_entry(r, &rdt_all_groups, rdtgroup_list) {
			if (r == rdtgrp)
				continue;
			cpumask_and(tmpmask1, &r->cpu_mask, tmpmask);
			if (!cpumask_empty(tmpmask1))
				cpumask_rdtgrp_clear(r, tmpmask1);
		}
		update_closid_rmid(tmpmask, rdtgrp);
	}

	 
	cpumask_copy(&rdtgrp->cpu_mask, newmask);

	 
	head = &rdtgrp->mon.crdtgrp_list;
	list_for_each_entry(crgrp, head, mon.crdtgrp_list) {
		cpumask_and(tmpmask, &rdtgrp->cpu_mask, &crgrp->cpu_mask);
		update_closid_rmid(tmpmask, rdtgrp);
		cpumask_clear(&crgrp->cpu_mask);
	}

	return 0;
}

static ssize_t rdtgroup_cpus_write(struct kernfs_open_file *of,
				   char *buf, size_t nbytes, loff_t off)
{
	cpumask_var_t tmpmask, newmask, tmpmask1;
	struct rdtgroup *rdtgrp;
	int ret;

	if (!buf)
		return -EINVAL;

	if (!zalloc_cpumask_var(&tmpmask, GFP_KERNEL))
		return -ENOMEM;
	if (!zalloc_cpumask_var(&newmask, GFP_KERNEL)) {
		free_cpumask_var(tmpmask);
		return -ENOMEM;
	}
	if (!zalloc_cpumask_var(&tmpmask1, GFP_KERNEL)) {
		free_cpumask_var(tmpmask);
		free_cpumask_var(newmask);
		return -ENOMEM;
	}

	rdtgrp = rdtgroup_kn_lock_live(of->kn);
	if (!rdtgrp) {
		ret = -ENOENT;
		goto unlock;
	}

	if (rdtgrp->mode == RDT_MODE_PSEUDO_LOCKED ||
	    rdtgrp->mode == RDT_MODE_PSEUDO_LOCKSETUP) {
		ret = -EINVAL;
		rdt_last_cmd_puts("Pseudo-locking in progress\n");
		goto unlock;
	}

	if (is_cpu_list(of))
		ret = cpulist_parse(buf, newmask);
	else
		ret = cpumask_parse(buf, newmask);

	if (ret) {
		rdt_last_cmd_puts("Bad CPU list/mask\n");
		goto unlock;
	}

	 
	cpumask_andnot(tmpmask, newmask, cpu_online_mask);
	if (!cpumask_empty(tmpmask)) {
		ret = -EINVAL;
		rdt_last_cmd_puts("Can only assign online CPUs\n");
		goto unlock;
	}

	if (rdtgrp->type == RDTCTRL_GROUP)
		ret = cpus_ctrl_write(rdtgrp, newmask, tmpmask, tmpmask1);
	else if (rdtgrp->type == RDTMON_GROUP)
		ret = cpus_mon_write(rdtgrp, newmask, tmpmask);
	else
		ret = -EINVAL;

unlock:
	rdtgroup_kn_unlock(of->kn);
	free_cpumask_var(tmpmask);
	free_cpumask_var(newmask);
	free_cpumask_var(tmpmask1);

	return ret ?: nbytes;
}

 
static void rdtgroup_remove(struct rdtgroup *rdtgrp)
{
	kernfs_put(rdtgrp->kn);
	kfree(rdtgrp);
}

static void _update_task_closid_rmid(void *task)
{
	 
	if (task == current)
		resctrl_sched_in(task);
}

static void update_task_closid_rmid(struct task_struct *t)
{
	if (IS_ENABLED(CONFIG_SMP) && task_curr(t))
		smp_call_function_single(task_cpu(t), _update_task_closid_rmid, t, 1);
	else
		_update_task_closid_rmid(t);
}

static int __rdtgroup_move_task(struct task_struct *tsk,
				struct rdtgroup *rdtgrp)
{
	 
	if ((rdtgrp->type == RDTCTRL_GROUP && tsk->closid == rdtgrp->closid &&
	     tsk->rmid == rdtgrp->mon.rmid) ||
	    (rdtgrp->type == RDTMON_GROUP && tsk->rmid == rdtgrp->mon.rmid &&
	     tsk->closid == rdtgrp->mon.parent->closid))
		return 0;

	 

	if (rdtgrp->type == RDTCTRL_GROUP) {
		WRITE_ONCE(tsk->closid, rdtgrp->closid);
		WRITE_ONCE(tsk->rmid, rdtgrp->mon.rmid);
	} else if (rdtgrp->type == RDTMON_GROUP) {
		if (rdtgrp->mon.parent->closid == tsk->closid) {
			WRITE_ONCE(tsk->rmid, rdtgrp->mon.rmid);
		} else {
			rdt_last_cmd_puts("Can't move task to different control group\n");
			return -EINVAL;
		}
	}

	 
	smp_mb();

	 
	update_task_closid_rmid(tsk);

	return 0;
}

static bool is_closid_match(struct task_struct *t, struct rdtgroup *r)
{
	return (rdt_alloc_capable &&
	       (r->type == RDTCTRL_GROUP) && (t->closid == r->closid));
}

static bool is_rmid_match(struct task_struct *t, struct rdtgroup *r)
{
	return (rdt_mon_capable &&
	       (r->type == RDTMON_GROUP) && (t->rmid == r->mon.rmid));
}

 
int rdtgroup_tasks_assigned(struct rdtgroup *r)
{
	struct task_struct *p, *t;
	int ret = 0;

	lockdep_assert_held(&rdtgroup_mutex);

	rcu_read_lock();
	for_each_process_thread(p, t) {
		if (is_closid_match(t, r) || is_rmid_match(t, r)) {
			ret = 1;
			break;
		}
	}
	rcu_read_unlock();

	return ret;
}

static int rdtgroup_task_write_permission(struct task_struct *task,
					  struct kernfs_open_file *of)
{
	const struct cred *tcred = get_task_cred(task);
	const struct cred *cred = current_cred();
	int ret = 0;

	 
	if (!uid_eq(cred->euid, GLOBAL_ROOT_UID) &&
	    !uid_eq(cred->euid, tcred->uid) &&
	    !uid_eq(cred->euid, tcred->suid)) {
		rdt_last_cmd_printf("No permission to move task %d\n", task->pid);
		ret = -EPERM;
	}

	put_cred(tcred);
	return ret;
}

static int rdtgroup_move_task(pid_t pid, struct rdtgroup *rdtgrp,
			      struct kernfs_open_file *of)
{
	struct task_struct *tsk;
	int ret;

	rcu_read_lock();
	if (pid) {
		tsk = find_task_by_vpid(pid);
		if (!tsk) {
			rcu_read_unlock();
			rdt_last_cmd_printf("No task %d\n", pid);
			return -ESRCH;
		}
	} else {
		tsk = current;
	}

	get_task_struct(tsk);
	rcu_read_unlock();

	ret = rdtgroup_task_write_permission(tsk, of);
	if (!ret)
		ret = __rdtgroup_move_task(tsk, rdtgrp);

	put_task_struct(tsk);
	return ret;
}

static ssize_t rdtgroup_tasks_write(struct kernfs_open_file *of,
				    char *buf, size_t nbytes, loff_t off)
{
	struct rdtgroup *rdtgrp;
	int ret = 0;
	pid_t pid;

	if (kstrtoint(strstrip(buf), 0, &pid) || pid < 0)
		return -EINVAL;
	rdtgrp = rdtgroup_kn_lock_live(of->kn);
	if (!rdtgrp) {
		rdtgroup_kn_unlock(of->kn);
		return -ENOENT;
	}
	rdt_last_cmd_clear();

	if (rdtgrp->mode == RDT_MODE_PSEUDO_LOCKED ||
	    rdtgrp->mode == RDT_MODE_PSEUDO_LOCKSETUP) {
		ret = -EINVAL;
		rdt_last_cmd_puts("Pseudo-locking in progress\n");
		goto unlock;
	}

	ret = rdtgroup_move_task(pid, rdtgrp, of);

unlock:
	rdtgroup_kn_unlock(of->kn);

	return ret ?: nbytes;
}

static void show_rdt_tasks(struct rdtgroup *r, struct seq_file *s)
{
	struct task_struct *p, *t;
	pid_t pid;

	rcu_read_lock();
	for_each_process_thread(p, t) {
		if (is_closid_match(t, r) || is_rmid_match(t, r)) {
			pid = task_pid_vnr(t);
			if (pid)
				seq_printf(s, "%d\n", pid);
		}
	}
	rcu_read_unlock();
}

static int rdtgroup_tasks_show(struct kernfs_open_file *of,
			       struct seq_file *s, void *v)
{
	struct rdtgroup *rdtgrp;
	int ret = 0;

	rdtgrp = rdtgroup_kn_lock_live(of->kn);
	if (rdtgrp)
		show_rdt_tasks(rdtgrp, s);
	else
		ret = -ENOENT;
	rdtgroup_kn_unlock(of->kn);

	return ret;
}

#ifdef CONFIG_PROC_CPU_RESCTRL

 
int proc_resctrl_show(struct seq_file *s, struct pid_namespace *ns,
		      struct pid *pid, struct task_struct *tsk)
{
	struct rdtgroup *rdtg;
	int ret = 0;

	mutex_lock(&rdtgroup_mutex);

	 
	if (!static_branch_unlikely(&rdt_enable_key)) {
		seq_puts(s, "res:\nmon:\n");
		goto unlock;
	}

	list_for_each_entry(rdtg, &rdt_all_groups, rdtgroup_list) {
		struct rdtgroup *crg;

		 
		if (rdtg->mode != RDT_MODE_SHAREABLE &&
		    rdtg->mode != RDT_MODE_EXCLUSIVE)
			continue;

		if (rdtg->closid != tsk->closid)
			continue;

		seq_printf(s, "res:%s%s\n", (rdtg == &rdtgroup_default) ? "/" : "",
			   rdtg->kn->name);
		seq_puts(s, "mon:");
		list_for_each_entry(crg, &rdtg->mon.crdtgrp_list,
				    mon.crdtgrp_list) {
			if (tsk->rmid != crg->mon.rmid)
				continue;
			seq_printf(s, "%s", crg->kn->name);
			break;
		}
		seq_putc(s, '\n');
		goto unlock;
	}
	 
	ret = -ENOENT;
unlock:
	mutex_unlock(&rdtgroup_mutex);

	return ret;
}
#endif

static int rdt_last_cmd_status_show(struct kernfs_open_file *of,
				    struct seq_file *seq, void *v)
{
	int len;

	mutex_lock(&rdtgroup_mutex);
	len = seq_buf_used(&last_cmd_status);
	if (len)
		seq_printf(seq, "%.*s", len, last_cmd_status_buf);
	else
		seq_puts(seq, "ok\n");
	mutex_unlock(&rdtgroup_mutex);
	return 0;
}

static int rdt_num_closids_show(struct kernfs_open_file *of,
				struct seq_file *seq, void *v)
{
	struct resctrl_schema *s = of->kn->parent->priv;

	seq_printf(seq, "%u\n", s->num_closid);
	return 0;
}

static int rdt_default_ctrl_show(struct kernfs_open_file *of,
			     struct seq_file *seq, void *v)
{
	struct resctrl_schema *s = of->kn->parent->priv;
	struct rdt_resource *r = s->res;

	seq_printf(seq, "%x\n", r->default_ctrl);
	return 0;
}

static int rdt_min_cbm_bits_show(struct kernfs_open_file *of,
			     struct seq_file *seq, void *v)
{
	struct resctrl_schema *s = of->kn->parent->priv;
	struct rdt_resource *r = s->res;

	seq_printf(seq, "%u\n", r->cache.min_cbm_bits);
	return 0;
}

static int rdt_shareable_bits_show(struct kernfs_open_file *of,
				   struct seq_file *seq, void *v)
{
	struct resctrl_schema *s = of->kn->parent->priv;
	struct rdt_resource *r = s->res;

	seq_printf(seq, "%x\n", r->cache.shareable_bits);
	return 0;
}

 
static int rdt_bit_usage_show(struct kernfs_open_file *of,
			      struct seq_file *seq, void *v)
{
	struct resctrl_schema *s = of->kn->parent->priv;
	 
	unsigned long sw_shareable = 0, hw_shareable = 0;
	unsigned long exclusive = 0, pseudo_locked = 0;
	struct rdt_resource *r = s->res;
	struct rdt_domain *dom;
	int i, hwb, swb, excl, psl;
	enum rdtgrp_mode mode;
	bool sep = false;
	u32 ctrl_val;

	mutex_lock(&rdtgroup_mutex);
	hw_shareable = r->cache.shareable_bits;
	list_for_each_entry(dom, &r->domains, list) {
		if (sep)
			seq_putc(seq, ';');
		sw_shareable = 0;
		exclusive = 0;
		seq_printf(seq, "%d=", dom->id);
		for (i = 0; i < closids_supported(); i++) {
			if (!closid_allocated(i))
				continue;
			ctrl_val = resctrl_arch_get_config(r, dom, i,
							   s->conf_type);
			mode = rdtgroup_mode_by_closid(i);
			switch (mode) {
			case RDT_MODE_SHAREABLE:
				sw_shareable |= ctrl_val;
				break;
			case RDT_MODE_EXCLUSIVE:
				exclusive |= ctrl_val;
				break;
			case RDT_MODE_PSEUDO_LOCKSETUP:
			 
				break;
			case RDT_MODE_PSEUDO_LOCKED:
			case RDT_NUM_MODES:
				WARN(1,
				     "invalid mode for closid %d\n", i);
				break;
			}
		}
		for (i = r->cache.cbm_len - 1; i >= 0; i--) {
			pseudo_locked = dom->plr ? dom->plr->cbm : 0;
			hwb = test_bit(i, &hw_shareable);
			swb = test_bit(i, &sw_shareable);
			excl = test_bit(i, &exclusive);
			psl = test_bit(i, &pseudo_locked);
			if (hwb && swb)
				seq_putc(seq, 'X');
			else if (hwb && !swb)
				seq_putc(seq, 'H');
			else if (!hwb && swb)
				seq_putc(seq, 'S');
			else if (excl)
				seq_putc(seq, 'E');
			else if (psl)
				seq_putc(seq, 'P');
			else  
				seq_putc(seq, '0');
		}
		sep = true;
	}
	seq_putc(seq, '\n');
	mutex_unlock(&rdtgroup_mutex);
	return 0;
}

static int rdt_min_bw_show(struct kernfs_open_file *of,
			     struct seq_file *seq, void *v)
{
	struct resctrl_schema *s = of->kn->parent->priv;
	struct rdt_resource *r = s->res;

	seq_printf(seq, "%u\n", r->membw.min_bw);
	return 0;
}

static int rdt_num_rmids_show(struct kernfs_open_file *of,
			      struct seq_file *seq, void *v)
{
	struct rdt_resource *r = of->kn->parent->priv;

	seq_printf(seq, "%d\n", r->num_rmid);

	return 0;
}

static int rdt_mon_features_show(struct kernfs_open_file *of,
				 struct seq_file *seq, void *v)
{
	struct rdt_resource *r = of->kn->parent->priv;
	struct mon_evt *mevt;

	list_for_each_entry(mevt, &r->evt_list, list) {
		seq_printf(seq, "%s\n", mevt->name);
		if (mevt->configurable)
			seq_printf(seq, "%s_config\n", mevt->name);
	}

	return 0;
}

static int rdt_bw_gran_show(struct kernfs_open_file *of,
			     struct seq_file *seq, void *v)
{
	struct resctrl_schema *s = of->kn->parent->priv;
	struct rdt_resource *r = s->res;

	seq_printf(seq, "%u\n", r->membw.bw_gran);
	return 0;
}

static int rdt_delay_linear_show(struct kernfs_open_file *of,
			     struct seq_file *seq, void *v)
{
	struct resctrl_schema *s = of->kn->parent->priv;
	struct rdt_resource *r = s->res;

	seq_printf(seq, "%u\n", r->membw.delay_linear);
	return 0;
}

static int max_threshold_occ_show(struct kernfs_open_file *of,
				  struct seq_file *seq, void *v)
{
	seq_printf(seq, "%u\n", resctrl_rmid_realloc_threshold);

	return 0;
}

static int rdt_thread_throttle_mode_show(struct kernfs_open_file *of,
					 struct seq_file *seq, void *v)
{
	struct resctrl_schema *s = of->kn->parent->priv;
	struct rdt_resource *r = s->res;

	if (r->membw.throttle_mode == THREAD_THROTTLE_PER_THREAD)
		seq_puts(seq, "per-thread\n");
	else
		seq_puts(seq, "max\n");

	return 0;
}

static ssize_t max_threshold_occ_write(struct kernfs_open_file *of,
				       char *buf, size_t nbytes, loff_t off)
{
	unsigned int bytes;
	int ret;

	ret = kstrtouint(buf, 0, &bytes);
	if (ret)
		return ret;

	if (bytes > resctrl_rmid_realloc_limit)
		return -EINVAL;

	resctrl_rmid_realloc_threshold = resctrl_arch_round_mon_val(bytes);

	return nbytes;
}

 
static int rdtgroup_mode_show(struct kernfs_open_file *of,
			      struct seq_file *s, void *v)
{
	struct rdtgroup *rdtgrp;

	rdtgrp = rdtgroup_kn_lock_live(of->kn);
	if (!rdtgrp) {
		rdtgroup_kn_unlock(of->kn);
		return -ENOENT;
	}

	seq_printf(s, "%s\n", rdtgroup_mode_str(rdtgrp->mode));

	rdtgroup_kn_unlock(of->kn);
	return 0;
}

static enum resctrl_conf_type resctrl_peer_type(enum resctrl_conf_type my_type)
{
	switch (my_type) {
	case CDP_CODE:
		return CDP_DATA;
	case CDP_DATA:
		return CDP_CODE;
	default:
	case CDP_NONE:
		return CDP_NONE;
	}
}

 
static bool __rdtgroup_cbm_overlaps(struct rdt_resource *r, struct rdt_domain *d,
				    unsigned long cbm, int closid,
				    enum resctrl_conf_type type, bool exclusive)
{
	enum rdtgrp_mode mode;
	unsigned long ctrl_b;
	int i;

	 
	if (!exclusive) {
		ctrl_b = r->cache.shareable_bits;
		if (bitmap_intersects(&cbm, &ctrl_b, r->cache.cbm_len))
			return true;
	}

	 
	for (i = 0; i < closids_supported(); i++) {
		ctrl_b = resctrl_arch_get_config(r, d, i, type);
		mode = rdtgroup_mode_by_closid(i);
		if (closid_allocated(i) && i != closid &&
		    mode != RDT_MODE_PSEUDO_LOCKSETUP) {
			if (bitmap_intersects(&cbm, &ctrl_b, r->cache.cbm_len)) {
				if (exclusive) {
					if (mode == RDT_MODE_EXCLUSIVE)
						return true;
					continue;
				}
				return true;
			}
		}
	}

	return false;
}

 
bool rdtgroup_cbm_overlaps(struct resctrl_schema *s, struct rdt_domain *d,
			   unsigned long cbm, int closid, bool exclusive)
{
	enum resctrl_conf_type peer_type = resctrl_peer_type(s->conf_type);
	struct rdt_resource *r = s->res;

	if (__rdtgroup_cbm_overlaps(r, d, cbm, closid, s->conf_type,
				    exclusive))
		return true;

	if (!resctrl_arch_get_cdp_enabled(r->rid))
		return false;
	return  __rdtgroup_cbm_overlaps(r, d, cbm, closid, peer_type, exclusive);
}

 
static bool rdtgroup_mode_test_exclusive(struct rdtgroup *rdtgrp)
{
	int closid = rdtgrp->closid;
	struct resctrl_schema *s;
	struct rdt_resource *r;
	bool has_cache = false;
	struct rdt_domain *d;
	u32 ctrl;

	list_for_each_entry(s, &resctrl_schema_all, list) {
		r = s->res;
		if (r->rid == RDT_RESOURCE_MBA || r->rid == RDT_RESOURCE_SMBA)
			continue;
		has_cache = true;
		list_for_each_entry(d, &r->domains, list) {
			ctrl = resctrl_arch_get_config(r, d, closid,
						       s->conf_type);
			if (rdtgroup_cbm_overlaps(s, d, ctrl, closid, false)) {
				rdt_last_cmd_puts("Schemata overlaps\n");
				return false;
			}
		}
	}

	if (!has_cache) {
		rdt_last_cmd_puts("Cannot be exclusive without CAT/CDP\n");
		return false;
	}

	return true;
}

 
static ssize_t rdtgroup_mode_write(struct kernfs_open_file *of,
				   char *buf, size_t nbytes, loff_t off)
{
	struct rdtgroup *rdtgrp;
	enum rdtgrp_mode mode;
	int ret = 0;

	 
	if (nbytes == 0 || buf[nbytes - 1] != '\n')
		return -EINVAL;
	buf[nbytes - 1] = '\0';

	rdtgrp = rdtgroup_kn_lock_live(of->kn);
	if (!rdtgrp) {
		rdtgroup_kn_unlock(of->kn);
		return -ENOENT;
	}

	rdt_last_cmd_clear();

	mode = rdtgrp->mode;

	if ((!strcmp(buf, "shareable") && mode == RDT_MODE_SHAREABLE) ||
	    (!strcmp(buf, "exclusive") && mode == RDT_MODE_EXCLUSIVE) ||
	    (!strcmp(buf, "pseudo-locksetup") &&
	     mode == RDT_MODE_PSEUDO_LOCKSETUP) ||
	    (!strcmp(buf, "pseudo-locked") && mode == RDT_MODE_PSEUDO_LOCKED))
		goto out;

	if (mode == RDT_MODE_PSEUDO_LOCKED) {
		rdt_last_cmd_puts("Cannot change pseudo-locked group\n");
		ret = -EINVAL;
		goto out;
	}

	if (!strcmp(buf, "shareable")) {
		if (rdtgrp->mode == RDT_MODE_PSEUDO_LOCKSETUP) {
			ret = rdtgroup_locksetup_exit(rdtgrp);
			if (ret)
				goto out;
		}
		rdtgrp->mode = RDT_MODE_SHAREABLE;
	} else if (!strcmp(buf, "exclusive")) {
		if (!rdtgroup_mode_test_exclusive(rdtgrp)) {
			ret = -EINVAL;
			goto out;
		}
		if (rdtgrp->mode == RDT_MODE_PSEUDO_LOCKSETUP) {
			ret = rdtgroup_locksetup_exit(rdtgrp);
			if (ret)
				goto out;
		}
		rdtgrp->mode = RDT_MODE_EXCLUSIVE;
	} else if (!strcmp(buf, "pseudo-locksetup")) {
		ret = rdtgroup_locksetup_enter(rdtgrp);
		if (ret)
			goto out;
		rdtgrp->mode = RDT_MODE_PSEUDO_LOCKSETUP;
	} else {
		rdt_last_cmd_puts("Unknown or unsupported mode\n");
		ret = -EINVAL;
	}

out:
	rdtgroup_kn_unlock(of->kn);
	return ret ?: nbytes;
}

 
unsigned int rdtgroup_cbm_to_size(struct rdt_resource *r,
				  struct rdt_domain *d, unsigned long cbm)
{
	struct cpu_cacheinfo *ci;
	unsigned int size = 0;
	int num_b, i;

	num_b = bitmap_weight(&cbm, r->cache.cbm_len);
	ci = get_cpu_cacheinfo(cpumask_any(&d->cpu_mask));
	for (i = 0; i < ci->num_leaves; i++) {
		if (ci->info_list[i].level == r->cache_level) {
			size = ci->info_list[i].size / r->cache.cbm_len * num_b;
			break;
		}
	}

	return size;
}

 
static int rdtgroup_size_show(struct kernfs_open_file *of,
			      struct seq_file *s, void *v)
{
	struct resctrl_schema *schema;
	enum resctrl_conf_type type;
	struct rdtgroup *rdtgrp;
	struct rdt_resource *r;
	struct rdt_domain *d;
	unsigned int size;
	int ret = 0;
	u32 closid;
	bool sep;
	u32 ctrl;

	rdtgrp = rdtgroup_kn_lock_live(of->kn);
	if (!rdtgrp) {
		rdtgroup_kn_unlock(of->kn);
		return -ENOENT;
	}

	if (rdtgrp->mode == RDT_MODE_PSEUDO_LOCKED) {
		if (!rdtgrp->plr->d) {
			rdt_last_cmd_clear();
			rdt_last_cmd_puts("Cache domain offline\n");
			ret = -ENODEV;
		} else {
			seq_printf(s, "%*s:", max_name_width,
				   rdtgrp->plr->s->name);
			size = rdtgroup_cbm_to_size(rdtgrp->plr->s->res,
						    rdtgrp->plr->d,
						    rdtgrp->plr->cbm);
			seq_printf(s, "%d=%u\n", rdtgrp->plr->d->id, size);
		}
		goto out;
	}

	closid = rdtgrp->closid;

	list_for_each_entry(schema, &resctrl_schema_all, list) {
		r = schema->res;
		type = schema->conf_type;
		sep = false;
		seq_printf(s, "%*s:", max_name_width, schema->name);
		list_for_each_entry(d, &r->domains, list) {
			if (sep)
				seq_putc(s, ';');
			if (rdtgrp->mode == RDT_MODE_PSEUDO_LOCKSETUP) {
				size = 0;
			} else {
				if (is_mba_sc(r))
					ctrl = d->mbps_val[closid];
				else
					ctrl = resctrl_arch_get_config(r, d,
								       closid,
								       type);
				if (r->rid == RDT_RESOURCE_MBA ||
				    r->rid == RDT_RESOURCE_SMBA)
					size = ctrl;
				else
					size = rdtgroup_cbm_to_size(r, d, ctrl);
			}
			seq_printf(s, "%d=%u", d->id, size);
			sep = true;
		}
		seq_putc(s, '\n');
	}

out:
	rdtgroup_kn_unlock(of->kn);

	return ret;
}

struct mon_config_info {
	u32 evtid;
	u32 mon_config;
};

#define INVALID_CONFIG_INDEX   UINT_MAX

 
static inline unsigned int mon_event_config_index_get(u32 evtid)
{
	switch (evtid) {
	case QOS_L3_MBM_TOTAL_EVENT_ID:
		return 0;
	case QOS_L3_MBM_LOCAL_EVENT_ID:
		return 1;
	default:
		 
		return INVALID_CONFIG_INDEX;
	}
}

static void mon_event_config_read(void *info)
{
	struct mon_config_info *mon_info = info;
	unsigned int index;
	u64 msrval;

	index = mon_event_config_index_get(mon_info->evtid);
	if (index == INVALID_CONFIG_INDEX) {
		pr_warn_once("Invalid event id %d\n", mon_info->evtid);
		return;
	}
	rdmsrl(MSR_IA32_EVT_CFG_BASE + index, msrval);

	 
	mon_info->mon_config = msrval & MAX_EVT_CONFIG_BITS;
}

static void mondata_config_read(struct rdt_domain *d, struct mon_config_info *mon_info)
{
	smp_call_function_any(&d->cpu_mask, mon_event_config_read, mon_info, 1);
}

static int mbm_config_show(struct seq_file *s, struct rdt_resource *r, u32 evtid)
{
	struct mon_config_info mon_info = {0};
	struct rdt_domain *dom;
	bool sep = false;

	mutex_lock(&rdtgroup_mutex);

	list_for_each_entry(dom, &r->domains, list) {
		if (sep)
			seq_puts(s, ";");

		memset(&mon_info, 0, sizeof(struct mon_config_info));
		mon_info.evtid = evtid;
		mondata_config_read(dom, &mon_info);

		seq_printf(s, "%d=0x%02x", dom->id, mon_info.mon_config);
		sep = true;
	}
	seq_puts(s, "\n");

	mutex_unlock(&rdtgroup_mutex);

	return 0;
}

static int mbm_total_bytes_config_show(struct kernfs_open_file *of,
				       struct seq_file *seq, void *v)
{
	struct rdt_resource *r = of->kn->parent->priv;

	mbm_config_show(seq, r, QOS_L3_MBM_TOTAL_EVENT_ID);

	return 0;
}

static int mbm_local_bytes_config_show(struct kernfs_open_file *of,
				       struct seq_file *seq, void *v)
{
	struct rdt_resource *r = of->kn->parent->priv;

	mbm_config_show(seq, r, QOS_L3_MBM_LOCAL_EVENT_ID);

	return 0;
}

static void mon_event_config_write(void *info)
{
	struct mon_config_info *mon_info = info;
	unsigned int index;

	index = mon_event_config_index_get(mon_info->evtid);
	if (index == INVALID_CONFIG_INDEX) {
		pr_warn_once("Invalid event id %d\n", mon_info->evtid);
		return;
	}
	wrmsr(MSR_IA32_EVT_CFG_BASE + index, mon_info->mon_config, 0);
}

static int mbm_config_write_domain(struct rdt_resource *r,
				   struct rdt_domain *d, u32 evtid, u32 val)
{
	struct mon_config_info mon_info = {0};
	int ret = 0;

	 
	if (val > MAX_EVT_CONFIG_BITS) {
		rdt_last_cmd_puts("Invalid event configuration\n");
		return -EINVAL;
	}

	 
	mon_info.evtid = evtid;
	mondata_config_read(d, &mon_info);
	if (mon_info.mon_config == val)
		goto out;

	mon_info.mon_config = val;

	 
	smp_call_function_any(&d->cpu_mask, mon_event_config_write,
			      &mon_info, 1);

	 
	resctrl_arch_reset_rmid_all(r, d);

out:
	return ret;
}

static int mon_config_write(struct rdt_resource *r, char *tok, u32 evtid)
{
	char *dom_str = NULL, *id_str;
	unsigned long dom_id, val;
	struct rdt_domain *d;
	int ret = 0;

next:
	if (!tok || tok[0] == '\0')
		return 0;

	 
	dom_str = strim(strsep(&tok, ";"));
	id_str = strsep(&dom_str, "=");

	if (!id_str || kstrtoul(id_str, 10, &dom_id)) {
		rdt_last_cmd_puts("Missing '=' or non-numeric domain id\n");
		return -EINVAL;
	}

	if (!dom_str || kstrtoul(dom_str, 16, &val)) {
		rdt_last_cmd_puts("Non-numeric event configuration value\n");
		return -EINVAL;
	}

	list_for_each_entry(d, &r->domains, list) {
		if (d->id == dom_id) {
			ret = mbm_config_write_domain(r, d, evtid, val);
			if (ret)
				return -EINVAL;
			goto next;
		}
	}

	return -EINVAL;
}

static ssize_t mbm_total_bytes_config_write(struct kernfs_open_file *of,
					    char *buf, size_t nbytes,
					    loff_t off)
{
	struct rdt_resource *r = of->kn->parent->priv;
	int ret;

	 
	if (nbytes == 0 || buf[nbytes - 1] != '\n')
		return -EINVAL;

	mutex_lock(&rdtgroup_mutex);

	rdt_last_cmd_clear();

	buf[nbytes - 1] = '\0';

	ret = mon_config_write(r, buf, QOS_L3_MBM_TOTAL_EVENT_ID);

	mutex_unlock(&rdtgroup_mutex);

	return ret ?: nbytes;
}

static ssize_t mbm_local_bytes_config_write(struct kernfs_open_file *of,
					    char *buf, size_t nbytes,
					    loff_t off)
{
	struct rdt_resource *r = of->kn->parent->priv;
	int ret;

	 
	if (nbytes == 0 || buf[nbytes - 1] != '\n')
		return -EINVAL;

	mutex_lock(&rdtgroup_mutex);

	rdt_last_cmd_clear();

	buf[nbytes - 1] = '\0';

	ret = mon_config_write(r, buf, QOS_L3_MBM_LOCAL_EVENT_ID);

	mutex_unlock(&rdtgroup_mutex);

	return ret ?: nbytes;
}

 
static struct rftype res_common_files[] = {
	{
		.name		= "last_cmd_status",
		.mode		= 0444,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.seq_show	= rdt_last_cmd_status_show,
		.fflags		= RF_TOP_INFO,
	},
	{
		.name		= "num_closids",
		.mode		= 0444,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.seq_show	= rdt_num_closids_show,
		.fflags		= RF_CTRL_INFO,
	},
	{
		.name		= "mon_features",
		.mode		= 0444,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.seq_show	= rdt_mon_features_show,
		.fflags		= RF_MON_INFO,
	},
	{
		.name		= "num_rmids",
		.mode		= 0444,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.seq_show	= rdt_num_rmids_show,
		.fflags		= RF_MON_INFO,
	},
	{
		.name		= "cbm_mask",
		.mode		= 0444,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.seq_show	= rdt_default_ctrl_show,
		.fflags		= RF_CTRL_INFO | RFTYPE_RES_CACHE,
	},
	{
		.name		= "min_cbm_bits",
		.mode		= 0444,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.seq_show	= rdt_min_cbm_bits_show,
		.fflags		= RF_CTRL_INFO | RFTYPE_RES_CACHE,
	},
	{
		.name		= "shareable_bits",
		.mode		= 0444,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.seq_show	= rdt_shareable_bits_show,
		.fflags		= RF_CTRL_INFO | RFTYPE_RES_CACHE,
	},
	{
		.name		= "bit_usage",
		.mode		= 0444,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.seq_show	= rdt_bit_usage_show,
		.fflags		= RF_CTRL_INFO | RFTYPE_RES_CACHE,
	},
	{
		.name		= "min_bandwidth",
		.mode		= 0444,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.seq_show	= rdt_min_bw_show,
		.fflags		= RF_CTRL_INFO | RFTYPE_RES_MB,
	},
	{
		.name		= "bandwidth_gran",
		.mode		= 0444,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.seq_show	= rdt_bw_gran_show,
		.fflags		= RF_CTRL_INFO | RFTYPE_RES_MB,
	},
	{
		.name		= "delay_linear",
		.mode		= 0444,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.seq_show	= rdt_delay_linear_show,
		.fflags		= RF_CTRL_INFO | RFTYPE_RES_MB,
	},
	 
	{
		.name		= "thread_throttle_mode",
		.mode		= 0444,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.seq_show	= rdt_thread_throttle_mode_show,
	},
	{
		.name		= "max_threshold_occupancy",
		.mode		= 0644,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.write		= max_threshold_occ_write,
		.seq_show	= max_threshold_occ_show,
		.fflags		= RF_MON_INFO | RFTYPE_RES_CACHE,
	},
	{
		.name		= "mbm_total_bytes_config",
		.mode		= 0644,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.seq_show	= mbm_total_bytes_config_show,
		.write		= mbm_total_bytes_config_write,
	},
	{
		.name		= "mbm_local_bytes_config",
		.mode		= 0644,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.seq_show	= mbm_local_bytes_config_show,
		.write		= mbm_local_bytes_config_write,
	},
	{
		.name		= "cpus",
		.mode		= 0644,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.write		= rdtgroup_cpus_write,
		.seq_show	= rdtgroup_cpus_show,
		.fflags		= RFTYPE_BASE,
	},
	{
		.name		= "cpus_list",
		.mode		= 0644,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.write		= rdtgroup_cpus_write,
		.seq_show	= rdtgroup_cpus_show,
		.flags		= RFTYPE_FLAGS_CPUS_LIST,
		.fflags		= RFTYPE_BASE,
	},
	{
		.name		= "tasks",
		.mode		= 0644,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.write		= rdtgroup_tasks_write,
		.seq_show	= rdtgroup_tasks_show,
		.fflags		= RFTYPE_BASE,
	},
	{
		.name		= "schemata",
		.mode		= 0644,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.write		= rdtgroup_schemata_write,
		.seq_show	= rdtgroup_schemata_show,
		.fflags		= RF_CTRL_BASE,
	},
	{
		.name		= "mode",
		.mode		= 0644,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.write		= rdtgroup_mode_write,
		.seq_show	= rdtgroup_mode_show,
		.fflags		= RF_CTRL_BASE,
	},
	{
		.name		= "size",
		.mode		= 0444,
		.kf_ops		= &rdtgroup_kf_single_ops,
		.seq_show	= rdtgroup_size_show,
		.fflags		= RF_CTRL_BASE,
	},

};

static int rdtgroup_add_files(struct kernfs_node *kn, unsigned long fflags)
{
	struct rftype *rfts, *rft;
	int ret, len;

	rfts = res_common_files;
	len = ARRAY_SIZE(res_common_files);

	lockdep_assert_held(&rdtgroup_mutex);

	for (rft = rfts; rft < rfts + len; rft++) {
		if (rft->fflags && ((fflags & rft->fflags) == rft->fflags)) {
			ret = rdtgroup_add_file(kn, rft);
			if (ret)
				goto error;
		}
	}

	return 0;
error:
	pr_warn("Failed to add %s, err=%d\n", rft->name, ret);
	while (--rft >= rfts) {
		if ((fflags & rft->fflags) == rft->fflags)
			kernfs_remove_by_name(kn, rft->name);
	}
	return ret;
}

static struct rftype *rdtgroup_get_rftype_by_name(const char *name)
{
	struct rftype *rfts, *rft;
	int len;

	rfts = res_common_files;
	len = ARRAY_SIZE(res_common_files);

	for (rft = rfts; rft < rfts + len; rft++) {
		if (!strcmp(rft->name, name))
			return rft;
	}

	return NULL;
}

void __init thread_throttle_mode_init(void)
{
	struct rftype *rft;

	rft = rdtgroup_get_rftype_by_name("thread_throttle_mode");
	if (!rft)
		return;

	rft->fflags = RF_CTRL_INFO | RFTYPE_RES_MB;
}

void __init mbm_config_rftype_init(const char *config)
{
	struct rftype *rft;

	rft = rdtgroup_get_rftype_by_name(config);
	if (rft)
		rft->fflags = RF_MON_INFO | RFTYPE_RES_CACHE;
}

 
int rdtgroup_kn_mode_restrict(struct rdtgroup *r, const char *name)
{
	struct iattr iattr = {.ia_valid = ATTR_MODE,};
	struct kernfs_node *kn;
	int ret = 0;

	kn = kernfs_find_and_get_ns(r->kn, name, NULL);
	if (!kn)
		return -ENOENT;

	switch (kernfs_type(kn)) {
	case KERNFS_DIR:
		iattr.ia_mode = S_IFDIR;
		break;
	case KERNFS_FILE:
		iattr.ia_mode = S_IFREG;
		break;
	case KERNFS_LINK:
		iattr.ia_mode = S_IFLNK;
		break;
	}

	ret = kernfs_setattr(kn, &iattr);
	kernfs_put(kn);
	return ret;
}

 
int rdtgroup_kn_mode_restore(struct rdtgroup *r, const char *name,
			     umode_t mask)
{
	struct iattr iattr = {.ia_valid = ATTR_MODE,};
	struct kernfs_node *kn, *parent;
	struct rftype *rfts, *rft;
	int ret, len;

	rfts = res_common_files;
	len = ARRAY_SIZE(res_common_files);

	for (rft = rfts; rft < rfts + len; rft++) {
		if (!strcmp(rft->name, name))
			iattr.ia_mode = rft->mode & mask;
	}

	kn = kernfs_find_and_get_ns(r->kn, name, NULL);
	if (!kn)
		return -ENOENT;

	switch (kernfs_type(kn)) {
	case KERNFS_DIR:
		parent = kernfs_get_parent(kn);
		if (parent) {
			iattr.ia_mode |= parent->mode;
			kernfs_put(parent);
		}
		iattr.ia_mode |= S_IFDIR;
		break;
	case KERNFS_FILE:
		iattr.ia_mode |= S_IFREG;
		break;
	case KERNFS_LINK:
		iattr.ia_mode |= S_IFLNK;
		break;
	}

	ret = kernfs_setattr(kn, &iattr);
	kernfs_put(kn);
	return ret;
}

static int rdtgroup_mkdir_info_resdir(void *priv, char *name,
				      unsigned long fflags)
{
	struct kernfs_node *kn_subdir;
	int ret;

	kn_subdir = kernfs_create_dir(kn_info, name,
				      kn_info->mode, priv);
	if (IS_ERR(kn_subdir))
		return PTR_ERR(kn_subdir);

	ret = rdtgroup_kn_set_ugid(kn_subdir);
	if (ret)
		return ret;

	ret = rdtgroup_add_files(kn_subdir, fflags);
	if (!ret)
		kernfs_activate(kn_subdir);

	return ret;
}

static int rdtgroup_create_info_dir(struct kernfs_node *parent_kn)
{
	struct resctrl_schema *s;
	struct rdt_resource *r;
	unsigned long fflags;
	char name[32];
	int ret;

	 
	kn_info = kernfs_create_dir(parent_kn, "info", parent_kn->mode, NULL);
	if (IS_ERR(kn_info))
		return PTR_ERR(kn_info);

	ret = rdtgroup_add_files(kn_info, RF_TOP_INFO);
	if (ret)
		goto out_destroy;

	 
	list_for_each_entry(s, &resctrl_schema_all, list) {
		r = s->res;
		fflags =  r->fflags | RF_CTRL_INFO;
		ret = rdtgroup_mkdir_info_resdir(s, s->name, fflags);
		if (ret)
			goto out_destroy;
	}

	for_each_mon_capable_rdt_resource(r) {
		fflags =  r->fflags | RF_MON_INFO;
		sprintf(name, "%s_MON", r->name);
		ret = rdtgroup_mkdir_info_resdir(r, name, fflags);
		if (ret)
			goto out_destroy;
	}

	ret = rdtgroup_kn_set_ugid(kn_info);
	if (ret)
		goto out_destroy;

	kernfs_activate(kn_info);

	return 0;

out_destroy:
	kernfs_remove(kn_info);
	return ret;
}

static int
mongroup_create_dir(struct kernfs_node *parent_kn, struct rdtgroup *prgrp,
		    char *name, struct kernfs_node **dest_kn)
{
	struct kernfs_node *kn;
	int ret;

	 
	kn = kernfs_create_dir(parent_kn, name, parent_kn->mode, prgrp);
	if (IS_ERR(kn))
		return PTR_ERR(kn);

	if (dest_kn)
		*dest_kn = kn;

	ret = rdtgroup_kn_set_ugid(kn);
	if (ret)
		goto out_destroy;

	kernfs_activate(kn);

	return 0;

out_destroy:
	kernfs_remove(kn);
	return ret;
}

static void l3_qos_cfg_update(void *arg)
{
	bool *enable = arg;

	wrmsrl(MSR_IA32_L3_QOS_CFG, *enable ? L3_QOS_CDP_ENABLE : 0ULL);
}

static void l2_qos_cfg_update(void *arg)
{
	bool *enable = arg;

	wrmsrl(MSR_IA32_L2_QOS_CFG, *enable ? L2_QOS_CDP_ENABLE : 0ULL);
}

static inline bool is_mba_linear(void)
{
	return rdt_resources_all[RDT_RESOURCE_MBA].r_resctrl.membw.delay_linear;
}

static int set_cache_qos_cfg(int level, bool enable)
{
	void (*update)(void *arg);
	struct rdt_resource *r_l;
	cpumask_var_t cpu_mask;
	struct rdt_domain *d;
	int cpu;

	if (level == RDT_RESOURCE_L3)
		update = l3_qos_cfg_update;
	else if (level == RDT_RESOURCE_L2)
		update = l2_qos_cfg_update;
	else
		return -EINVAL;

	if (!zalloc_cpumask_var(&cpu_mask, GFP_KERNEL))
		return -ENOMEM;

	r_l = &rdt_resources_all[level].r_resctrl;
	list_for_each_entry(d, &r_l->domains, list) {
		if (r_l->cache.arch_has_per_cpu_cfg)
			 
			for_each_cpu(cpu, &d->cpu_mask)
				cpumask_set_cpu(cpu, cpu_mask);
		else
			 
			cpumask_set_cpu(cpumask_any(&d->cpu_mask), cpu_mask);
	}

	 
	on_each_cpu_mask(cpu_mask, update, &enable, 1);

	free_cpumask_var(cpu_mask);

	return 0;
}

 
void rdt_domain_reconfigure_cdp(struct rdt_resource *r)
{
	struct rdt_hw_resource *hw_res = resctrl_to_arch_res(r);

	if (!r->cdp_capable)
		return;

	if (r->rid == RDT_RESOURCE_L2)
		l2_qos_cfg_update(&hw_res->cdp_enabled);

	if (r->rid == RDT_RESOURCE_L3)
		l3_qos_cfg_update(&hw_res->cdp_enabled);
}

static int mba_sc_domain_allocate(struct rdt_resource *r, struct rdt_domain *d)
{
	u32 num_closid = resctrl_arch_get_num_closid(r);
	int cpu = cpumask_any(&d->cpu_mask);
	int i;

	d->mbps_val = kcalloc_node(num_closid, sizeof(*d->mbps_val),
				   GFP_KERNEL, cpu_to_node(cpu));
	if (!d->mbps_val)
		return -ENOMEM;

	for (i = 0; i < num_closid; i++)
		d->mbps_val[i] = MBA_MAX_MBPS;

	return 0;
}

static void mba_sc_domain_destroy(struct rdt_resource *r,
				  struct rdt_domain *d)
{
	kfree(d->mbps_val);
	d->mbps_val = NULL;
}

 
static bool supports_mba_mbps(void)
{
	struct rdt_resource *r = &rdt_resources_all[RDT_RESOURCE_MBA].r_resctrl;

	return (is_mbm_local_enabled() &&
		r->alloc_capable && is_mba_linear());
}

 
static int set_mba_sc(bool mba_sc)
{
	struct rdt_resource *r = &rdt_resources_all[RDT_RESOURCE_MBA].r_resctrl;
	u32 num_closid = resctrl_arch_get_num_closid(r);
	struct rdt_domain *d;
	int i;

	if (!supports_mba_mbps() || mba_sc == is_mba_sc(r))
		return -EINVAL;

	r->membw.mba_sc = mba_sc;

	list_for_each_entry(d, &r->domains, list) {
		for (i = 0; i < num_closid; i++)
			d->mbps_val[i] = MBA_MAX_MBPS;
	}

	return 0;
}

static int cdp_enable(int level)
{
	struct rdt_resource *r_l = &rdt_resources_all[level].r_resctrl;
	int ret;

	if (!r_l->alloc_capable)
		return -EINVAL;

	ret = set_cache_qos_cfg(level, true);
	if (!ret)
		rdt_resources_all[level].cdp_enabled = true;

	return ret;
}

static void cdp_disable(int level)
{
	struct rdt_hw_resource *r_hw = &rdt_resources_all[level];

	if (r_hw->cdp_enabled) {
		set_cache_qos_cfg(level, false);
		r_hw->cdp_enabled = false;
	}
}

int resctrl_arch_set_cdp_enabled(enum resctrl_res_level l, bool enable)
{
	struct rdt_hw_resource *hw_res = &rdt_resources_all[l];

	if (!hw_res->r_resctrl.cdp_capable)
		return -EINVAL;

	if (enable)
		return cdp_enable(l);

	cdp_disable(l);

	return 0;
}

static void cdp_disable_all(void)
{
	if (resctrl_arch_get_cdp_enabled(RDT_RESOURCE_L3))
		resctrl_arch_set_cdp_enabled(RDT_RESOURCE_L3, false);
	if (resctrl_arch_get_cdp_enabled(RDT_RESOURCE_L2))
		resctrl_arch_set_cdp_enabled(RDT_RESOURCE_L2, false);
}

 
static struct rdtgroup *kernfs_to_rdtgroup(struct kernfs_node *kn)
{
	if (kernfs_type(kn) == KERNFS_DIR) {
		 
		if (kn == kn_info || kn->parent == kn_info)
			return NULL;
		else
			return kn->priv;
	} else {
		return kn->parent->priv;
	}
}

static void rdtgroup_kn_get(struct rdtgroup *rdtgrp, struct kernfs_node *kn)
{
	atomic_inc(&rdtgrp->waitcount);
	kernfs_break_active_protection(kn);
}

static void rdtgroup_kn_put(struct rdtgroup *rdtgrp, struct kernfs_node *kn)
{
	if (atomic_dec_and_test(&rdtgrp->waitcount) &&
	    (rdtgrp->flags & RDT_DELETED)) {
		if (rdtgrp->mode == RDT_MODE_PSEUDO_LOCKSETUP ||
		    rdtgrp->mode == RDT_MODE_PSEUDO_LOCKED)
			rdtgroup_pseudo_lock_remove(rdtgrp);
		kernfs_unbreak_active_protection(kn);
		rdtgroup_remove(rdtgrp);
	} else {
		kernfs_unbreak_active_protection(kn);
	}
}

struct rdtgroup *rdtgroup_kn_lock_live(struct kernfs_node *kn)
{
	struct rdtgroup *rdtgrp = kernfs_to_rdtgroup(kn);

	if (!rdtgrp)
		return NULL;

	rdtgroup_kn_get(rdtgrp, kn);

	mutex_lock(&rdtgroup_mutex);

	 
	if (rdtgrp->flags & RDT_DELETED)
		return NULL;

	return rdtgrp;
}

void rdtgroup_kn_unlock(struct kernfs_node *kn)
{
	struct rdtgroup *rdtgrp = kernfs_to_rdtgroup(kn);

	if (!rdtgrp)
		return;

	mutex_unlock(&rdtgroup_mutex);
	rdtgroup_kn_put(rdtgrp, kn);
}

static int mkdir_mondata_all(struct kernfs_node *parent_kn,
			     struct rdtgroup *prgrp,
			     struct kernfs_node **mon_data_kn);

static int rdt_enable_ctx(struct rdt_fs_context *ctx)
{
	int ret = 0;

	if (ctx->enable_cdpl2)
		ret = resctrl_arch_set_cdp_enabled(RDT_RESOURCE_L2, true);

	if (!ret && ctx->enable_cdpl3)
		ret = resctrl_arch_set_cdp_enabled(RDT_RESOURCE_L3, true);

	if (!ret && ctx->enable_mba_mbps)
		ret = set_mba_sc(true);

	return ret;
}

static int schemata_list_add(struct rdt_resource *r, enum resctrl_conf_type type)
{
	struct resctrl_schema *s;
	const char *suffix = "";
	int ret, cl;

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s)
		return -ENOMEM;

	s->res = r;
	s->num_closid = resctrl_arch_get_num_closid(r);
	if (resctrl_arch_get_cdp_enabled(r->rid))
		s->num_closid /= 2;

	s->conf_type = type;
	switch (type) {
	case CDP_CODE:
		suffix = "CODE";
		break;
	case CDP_DATA:
		suffix = "DATA";
		break;
	case CDP_NONE:
		suffix = "";
		break;
	}

	ret = snprintf(s->name, sizeof(s->name), "%s%s", r->name, suffix);
	if (ret >= sizeof(s->name)) {
		kfree(s);
		return -EINVAL;
	}

	cl = strlen(s->name);

	 
	if (r->cdp_capable && !resctrl_arch_get_cdp_enabled(r->rid))
		cl += 4;

	if (cl > max_name_width)
		max_name_width = cl;

	INIT_LIST_HEAD(&s->list);
	list_add(&s->list, &resctrl_schema_all);

	return 0;
}

static int schemata_list_create(void)
{
	struct rdt_resource *r;
	int ret = 0;

	for_each_alloc_capable_rdt_resource(r) {
		if (resctrl_arch_get_cdp_enabled(r->rid)) {
			ret = schemata_list_add(r, CDP_CODE);
			if (ret)
				break;

			ret = schemata_list_add(r, CDP_DATA);
		} else {
			ret = schemata_list_add(r, CDP_NONE);
		}

		if (ret)
			break;
	}

	return ret;
}

static void schemata_list_destroy(void)
{
	struct resctrl_schema *s, *tmp;

	list_for_each_entry_safe(s, tmp, &resctrl_schema_all, list) {
		list_del(&s->list);
		kfree(s);
	}
}

static int rdt_get_tree(struct fs_context *fc)
{
	struct rdt_fs_context *ctx = rdt_fc2context(fc);
	struct rdt_domain *dom;
	struct rdt_resource *r;
	int ret;

	cpus_read_lock();
	mutex_lock(&rdtgroup_mutex);
	 
	if (static_branch_unlikely(&rdt_enable_key)) {
		ret = -EBUSY;
		goto out;
	}

	ret = rdt_enable_ctx(ctx);
	if (ret < 0)
		goto out_cdp;

	ret = schemata_list_create();
	if (ret) {
		schemata_list_destroy();
		goto out_mba;
	}

	closid_init();

	ret = rdtgroup_create_info_dir(rdtgroup_default.kn);
	if (ret < 0)
		goto out_schemata_free;

	if (rdt_mon_capable) {
		ret = mongroup_create_dir(rdtgroup_default.kn,
					  &rdtgroup_default, "mon_groups",
					  &kn_mongrp);
		if (ret < 0)
			goto out_info;

		ret = mkdir_mondata_all(rdtgroup_default.kn,
					&rdtgroup_default, &kn_mondata);
		if (ret < 0)
			goto out_mongrp;
		rdtgroup_default.mon.mon_data_kn = kn_mondata;
	}

	ret = rdt_pseudo_lock_init();
	if (ret)
		goto out_mondata;

	ret = kernfs_get_tree(fc);
	if (ret < 0)
		goto out_psl;

	if (rdt_alloc_capable)
		static_branch_enable_cpuslocked(&rdt_alloc_enable_key);
	if (rdt_mon_capable)
		static_branch_enable_cpuslocked(&rdt_mon_enable_key);

	if (rdt_alloc_capable || rdt_mon_capable)
		static_branch_enable_cpuslocked(&rdt_enable_key);

	if (is_mbm_enabled()) {
		r = &rdt_resources_all[RDT_RESOURCE_L3].r_resctrl;
		list_for_each_entry(dom, &r->domains, list)
			mbm_setup_overflow_handler(dom, MBM_OVERFLOW_INTERVAL);
	}

	goto out;

out_psl:
	rdt_pseudo_lock_release();
out_mondata:
	if (rdt_mon_capable)
		kernfs_remove(kn_mondata);
out_mongrp:
	if (rdt_mon_capable)
		kernfs_remove(kn_mongrp);
out_info:
	kernfs_remove(kn_info);
out_schemata_free:
	schemata_list_destroy();
out_mba:
	if (ctx->enable_mba_mbps)
		set_mba_sc(false);
out_cdp:
	cdp_disable_all();
out:
	rdt_last_cmd_clear();
	mutex_unlock(&rdtgroup_mutex);
	cpus_read_unlock();
	return ret;
}

enum rdt_param {
	Opt_cdp,
	Opt_cdpl2,
	Opt_mba_mbps,
	nr__rdt_params
};

static const struct fs_parameter_spec rdt_fs_parameters[] = {
	fsparam_flag("cdp",		Opt_cdp),
	fsparam_flag("cdpl2",		Opt_cdpl2),
	fsparam_flag("mba_MBps",	Opt_mba_mbps),
	{}
};

static int rdt_parse_param(struct fs_context *fc, struct fs_parameter *param)
{
	struct rdt_fs_context *ctx = rdt_fc2context(fc);
	struct fs_parse_result result;
	int opt;

	opt = fs_parse(fc, rdt_fs_parameters, param, &result);
	if (opt < 0)
		return opt;

	switch (opt) {
	case Opt_cdp:
		ctx->enable_cdpl3 = true;
		return 0;
	case Opt_cdpl2:
		ctx->enable_cdpl2 = true;
		return 0;
	case Opt_mba_mbps:
		if (!supports_mba_mbps())
			return -EINVAL;
		ctx->enable_mba_mbps = true;
		return 0;
	}

	return -EINVAL;
}

static void rdt_fs_context_free(struct fs_context *fc)
{
	struct rdt_fs_context *ctx = rdt_fc2context(fc);

	kernfs_free_fs_context(fc);
	kfree(ctx);
}

static const struct fs_context_operations rdt_fs_context_ops = {
	.free		= rdt_fs_context_free,
	.parse_param	= rdt_parse_param,
	.get_tree	= rdt_get_tree,
};

static int rdt_init_fs_context(struct fs_context *fc)
{
	struct rdt_fs_context *ctx;

	ctx = kzalloc(sizeof(struct rdt_fs_context), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->kfc.root = rdt_root;
	ctx->kfc.magic = RDTGROUP_SUPER_MAGIC;
	fc->fs_private = &ctx->kfc;
	fc->ops = &rdt_fs_context_ops;
	put_user_ns(fc->user_ns);
	fc->user_ns = get_user_ns(&init_user_ns);
	fc->global = true;
	return 0;
}

static int reset_all_ctrls(struct rdt_resource *r)
{
	struct rdt_hw_resource *hw_res = resctrl_to_arch_res(r);
	struct rdt_hw_domain *hw_dom;
	struct msr_param msr_param;
	cpumask_var_t cpu_mask;
	struct rdt_domain *d;
	int i;

	if (!zalloc_cpumask_var(&cpu_mask, GFP_KERNEL))
		return -ENOMEM;

	msr_param.res = r;
	msr_param.low = 0;
	msr_param.high = hw_res->num_closid;

	 
	list_for_each_entry(d, &r->domains, list) {
		hw_dom = resctrl_to_arch_dom(d);
		cpumask_set_cpu(cpumask_any(&d->cpu_mask), cpu_mask);

		for (i = 0; i < hw_res->num_closid; i++)
			hw_dom->ctrl_val[i] = r->default_ctrl;
	}

	 
	on_each_cpu_mask(cpu_mask, rdt_ctrl_update, &msr_param, 1);

	free_cpumask_var(cpu_mask);

	return 0;
}

 
static void rdt_move_group_tasks(struct rdtgroup *from, struct rdtgroup *to,
				 struct cpumask *mask)
{
	struct task_struct *p, *t;

	read_lock(&tasklist_lock);
	for_each_process_thread(p, t) {
		if (!from || is_closid_match(t, from) ||
		    is_rmid_match(t, from)) {
			WRITE_ONCE(t->closid, to->closid);
			WRITE_ONCE(t->rmid, to->mon.rmid);

			 
			smp_mb();

			 
			if (IS_ENABLED(CONFIG_SMP) && mask && task_curr(t))
				cpumask_set_cpu(task_cpu(t), mask);
		}
	}
	read_unlock(&tasklist_lock);
}

static void free_all_child_rdtgrp(struct rdtgroup *rdtgrp)
{
	struct rdtgroup *sentry, *stmp;
	struct list_head *head;

	head = &rdtgrp->mon.crdtgrp_list;
	list_for_each_entry_safe(sentry, stmp, head, mon.crdtgrp_list) {
		free_rmid(sentry->mon.rmid);
		list_del(&sentry->mon.crdtgrp_list);

		if (atomic_read(&sentry->waitcount) != 0)
			sentry->flags = RDT_DELETED;
		else
			rdtgroup_remove(sentry);
	}
}

 
static void rmdir_all_sub(void)
{
	struct rdtgroup *rdtgrp, *tmp;

	 
	rdt_move_group_tasks(NULL, &rdtgroup_default, NULL);

	list_for_each_entry_safe(rdtgrp, tmp, &rdt_all_groups, rdtgroup_list) {
		 
		free_all_child_rdtgrp(rdtgrp);

		 
		if (rdtgrp == &rdtgroup_default)
			continue;

		if (rdtgrp->mode == RDT_MODE_PSEUDO_LOCKSETUP ||
		    rdtgrp->mode == RDT_MODE_PSEUDO_LOCKED)
			rdtgroup_pseudo_lock_remove(rdtgrp);

		 
		cpumask_or(&rdtgroup_default.cpu_mask,
			   &rdtgroup_default.cpu_mask, &rdtgrp->cpu_mask);

		free_rmid(rdtgrp->mon.rmid);

		kernfs_remove(rdtgrp->kn);
		list_del(&rdtgrp->rdtgroup_list);

		if (atomic_read(&rdtgrp->waitcount) != 0)
			rdtgrp->flags = RDT_DELETED;
		else
			rdtgroup_remove(rdtgrp);
	}
	 
	update_closid_rmid(cpu_online_mask, &rdtgroup_default);

	kernfs_remove(kn_info);
	kernfs_remove(kn_mongrp);
	kernfs_remove(kn_mondata);
}

static void rdt_kill_sb(struct super_block *sb)
{
	struct rdt_resource *r;

	cpus_read_lock();
	mutex_lock(&rdtgroup_mutex);

	set_mba_sc(false);

	 
	for_each_alloc_capable_rdt_resource(r)
		reset_all_ctrls(r);
	cdp_disable_all();
	rmdir_all_sub();
	rdt_pseudo_lock_release();
	rdtgroup_default.mode = RDT_MODE_SHAREABLE;
	schemata_list_destroy();
	static_branch_disable_cpuslocked(&rdt_alloc_enable_key);
	static_branch_disable_cpuslocked(&rdt_mon_enable_key);
	static_branch_disable_cpuslocked(&rdt_enable_key);
	kernfs_kill_sb(sb);
	mutex_unlock(&rdtgroup_mutex);
	cpus_read_unlock();
}

static struct file_system_type rdt_fs_type = {
	.name			= "resctrl",
	.init_fs_context	= rdt_init_fs_context,
	.parameters		= rdt_fs_parameters,
	.kill_sb		= rdt_kill_sb,
};

static int mon_addfile(struct kernfs_node *parent_kn, const char *name,
		       void *priv)
{
	struct kernfs_node *kn;
	int ret = 0;

	kn = __kernfs_create_file(parent_kn, name, 0444,
				  GLOBAL_ROOT_UID, GLOBAL_ROOT_GID, 0,
				  &kf_mondata_ops, priv, NULL, NULL);
	if (IS_ERR(kn))
		return PTR_ERR(kn);

	ret = rdtgroup_kn_set_ugid(kn);
	if (ret) {
		kernfs_remove(kn);
		return ret;
	}

	return ret;
}

 
static void rmdir_mondata_subdir_allrdtgrp(struct rdt_resource *r,
					   unsigned int dom_id)
{
	struct rdtgroup *prgrp, *crgrp;
	char name[32];

	list_for_each_entry(prgrp, &rdt_all_groups, rdtgroup_list) {
		sprintf(name, "mon_%s_%02d", r->name, dom_id);
		kernfs_remove_by_name(prgrp->mon.mon_data_kn, name);

		list_for_each_entry(crgrp, &prgrp->mon.crdtgrp_list, mon.crdtgrp_list)
			kernfs_remove_by_name(crgrp->mon.mon_data_kn, name);
	}
}

static int mkdir_mondata_subdir(struct kernfs_node *parent_kn,
				struct rdt_domain *d,
				struct rdt_resource *r, struct rdtgroup *prgrp)
{
	union mon_data_bits priv;
	struct kernfs_node *kn;
	struct mon_evt *mevt;
	struct rmid_read rr;
	char name[32];
	int ret;

	sprintf(name, "mon_%s_%02d", r->name, d->id);
	 
	kn = kernfs_create_dir(parent_kn, name, parent_kn->mode, prgrp);
	if (IS_ERR(kn))
		return PTR_ERR(kn);

	ret = rdtgroup_kn_set_ugid(kn);
	if (ret)
		goto out_destroy;

	if (WARN_ON(list_empty(&r->evt_list))) {
		ret = -EPERM;
		goto out_destroy;
	}

	priv.u.rid = r->rid;
	priv.u.domid = d->id;
	list_for_each_entry(mevt, &r->evt_list, list) {
		priv.u.evtid = mevt->evtid;
		ret = mon_addfile(kn, mevt->name, priv.priv);
		if (ret)
			goto out_destroy;

		if (is_mbm_event(mevt->evtid))
			mon_event_read(&rr, r, d, prgrp, mevt->evtid, true);
	}
	kernfs_activate(kn);
	return 0;

out_destroy:
	kernfs_remove(kn);
	return ret;
}

 
static void mkdir_mondata_subdir_allrdtgrp(struct rdt_resource *r,
					   struct rdt_domain *d)
{
	struct kernfs_node *parent_kn;
	struct rdtgroup *prgrp, *crgrp;
	struct list_head *head;

	list_for_each_entry(prgrp, &rdt_all_groups, rdtgroup_list) {
		parent_kn = prgrp->mon.mon_data_kn;
		mkdir_mondata_subdir(parent_kn, d, r, prgrp);

		head = &prgrp->mon.crdtgrp_list;
		list_for_each_entry(crgrp, head, mon.crdtgrp_list) {
			parent_kn = crgrp->mon.mon_data_kn;
			mkdir_mondata_subdir(parent_kn, d, r, crgrp);
		}
	}
}

static int mkdir_mondata_subdir_alldom(struct kernfs_node *parent_kn,
				       struct rdt_resource *r,
				       struct rdtgroup *prgrp)
{
	struct rdt_domain *dom;
	int ret;

	list_for_each_entry(dom, &r->domains, list) {
		ret = mkdir_mondata_subdir(parent_kn, dom, r, prgrp);
		if (ret)
			return ret;
	}

	return 0;
}

 
static int mkdir_mondata_all(struct kernfs_node *parent_kn,
			     struct rdtgroup *prgrp,
			     struct kernfs_node **dest_kn)
{
	struct rdt_resource *r;
	struct kernfs_node *kn;
	int ret;

	 
	ret = mongroup_create_dir(parent_kn, prgrp, "mon_data", &kn);
	if (ret)
		return ret;

	if (dest_kn)
		*dest_kn = kn;

	 
	for_each_mon_capable_rdt_resource(r) {
		ret = mkdir_mondata_subdir_alldom(kn, r, prgrp);
		if (ret)
			goto out_destroy;
	}

	return 0;

out_destroy:
	kernfs_remove(kn);
	return ret;
}

 
static u32 cbm_ensure_valid(u32 _val, struct rdt_resource *r)
{
	unsigned int cbm_len = r->cache.cbm_len;
	unsigned long first_bit, zero_bit;
	unsigned long val = _val;

	if (!val)
		return 0;

	first_bit = find_first_bit(&val, cbm_len);
	zero_bit = find_next_zero_bit(&val, cbm_len, first_bit);

	 
	bitmap_clear(&val, zero_bit, cbm_len - zero_bit);
	return (u32)val;
}

 
static int __init_one_rdt_domain(struct rdt_domain *d, struct resctrl_schema *s,
				 u32 closid)
{
	enum resctrl_conf_type peer_type = resctrl_peer_type(s->conf_type);
	enum resctrl_conf_type t = s->conf_type;
	struct resctrl_staged_config *cfg;
	struct rdt_resource *r = s->res;
	u32 used_b = 0, unused_b = 0;
	unsigned long tmp_cbm;
	enum rdtgrp_mode mode;
	u32 peer_ctl, ctrl_val;
	int i;

	cfg = &d->staged_config[t];
	cfg->have_new_ctrl = false;
	cfg->new_ctrl = r->cache.shareable_bits;
	used_b = r->cache.shareable_bits;
	for (i = 0; i < closids_supported(); i++) {
		if (closid_allocated(i) && i != closid) {
			mode = rdtgroup_mode_by_closid(i);
			if (mode == RDT_MODE_PSEUDO_LOCKSETUP)
				 
				continue;
			 
			if (resctrl_arch_get_cdp_enabled(r->rid))
				peer_ctl = resctrl_arch_get_config(r, d, i,
								   peer_type);
			else
				peer_ctl = 0;
			ctrl_val = resctrl_arch_get_config(r, d, i,
							   s->conf_type);
			used_b |= ctrl_val | peer_ctl;
			if (mode == RDT_MODE_SHAREABLE)
				cfg->new_ctrl |= ctrl_val | peer_ctl;
		}
	}
	if (d->plr && d->plr->cbm > 0)
		used_b |= d->plr->cbm;
	unused_b = used_b ^ (BIT_MASK(r->cache.cbm_len) - 1);
	unused_b &= BIT_MASK(r->cache.cbm_len) - 1;
	cfg->new_ctrl |= unused_b;
	 
	cfg->new_ctrl = cbm_ensure_valid(cfg->new_ctrl, r);
	 
	tmp_cbm = cfg->new_ctrl;
	if (bitmap_weight(&tmp_cbm, r->cache.cbm_len) < r->cache.min_cbm_bits) {
		rdt_last_cmd_printf("No space on %s:%d\n", s->name, d->id);
		return -ENOSPC;
	}
	cfg->have_new_ctrl = true;

	return 0;
}

 
static int rdtgroup_init_cat(struct resctrl_schema *s, u32 closid)
{
	struct rdt_domain *d;
	int ret;

	list_for_each_entry(d, &s->res->domains, list) {
		ret = __init_one_rdt_domain(d, s, closid);
		if (ret < 0)
			return ret;
	}

	return 0;
}

 
static void rdtgroup_init_mba(struct rdt_resource *r, u32 closid)
{
	struct resctrl_staged_config *cfg;
	struct rdt_domain *d;

	list_for_each_entry(d, &r->domains, list) {
		if (is_mba_sc(r)) {
			d->mbps_val[closid] = MBA_MAX_MBPS;
			continue;
		}

		cfg = &d->staged_config[CDP_NONE];
		cfg->new_ctrl = r->default_ctrl;
		cfg->have_new_ctrl = true;
	}
}

 
static int rdtgroup_init_alloc(struct rdtgroup *rdtgrp)
{
	struct resctrl_schema *s;
	struct rdt_resource *r;
	int ret = 0;

	rdt_staged_configs_clear();

	list_for_each_entry(s, &resctrl_schema_all, list) {
		r = s->res;
		if (r->rid == RDT_RESOURCE_MBA ||
		    r->rid == RDT_RESOURCE_SMBA) {
			rdtgroup_init_mba(r, rdtgrp->closid);
			if (is_mba_sc(r))
				continue;
		} else {
			ret = rdtgroup_init_cat(s, rdtgrp->closid);
			if (ret < 0)
				goto out;
		}

		ret = resctrl_arch_update_domains(r, rdtgrp->closid);
		if (ret < 0) {
			rdt_last_cmd_puts("Failed to initialize allocations\n");
			goto out;
		}

	}

	rdtgrp->mode = RDT_MODE_SHAREABLE;

out:
	rdt_staged_configs_clear();
	return ret;
}

static int mkdir_rdt_prepare(struct kernfs_node *parent_kn,
			     const char *name, umode_t mode,
			     enum rdt_group_type rtype, struct rdtgroup **r)
{
	struct rdtgroup *prdtgrp, *rdtgrp;
	struct kernfs_node *kn;
	uint files = 0;
	int ret;

	prdtgrp = rdtgroup_kn_lock_live(parent_kn);
	if (!prdtgrp) {
		ret = -ENODEV;
		goto out_unlock;
	}

	if (rtype == RDTMON_GROUP &&
	    (prdtgrp->mode == RDT_MODE_PSEUDO_LOCKSETUP ||
	     prdtgrp->mode == RDT_MODE_PSEUDO_LOCKED)) {
		ret = -EINVAL;
		rdt_last_cmd_puts("Pseudo-locking in progress\n");
		goto out_unlock;
	}

	 
	rdtgrp = kzalloc(sizeof(*rdtgrp), GFP_KERNEL);
	if (!rdtgrp) {
		ret = -ENOSPC;
		rdt_last_cmd_puts("Kernel out of memory\n");
		goto out_unlock;
	}
	*r = rdtgrp;
	rdtgrp->mon.parent = prdtgrp;
	rdtgrp->type = rtype;
	INIT_LIST_HEAD(&rdtgrp->mon.crdtgrp_list);

	 
	kn = kernfs_create_dir(parent_kn, name, mode, rdtgrp);
	if (IS_ERR(kn)) {
		ret = PTR_ERR(kn);
		rdt_last_cmd_puts("kernfs create error\n");
		goto out_free_rgrp;
	}
	rdtgrp->kn = kn;

	 
	kernfs_get(kn);

	ret = rdtgroup_kn_set_ugid(kn);
	if (ret) {
		rdt_last_cmd_puts("kernfs perm error\n");
		goto out_destroy;
	}

	files = RFTYPE_BASE | BIT(RF_CTRLSHIFT + rtype);
	ret = rdtgroup_add_files(kn, files);
	if (ret) {
		rdt_last_cmd_puts("kernfs fill error\n");
		goto out_destroy;
	}

	if (rdt_mon_capable) {
		ret = alloc_rmid();
		if (ret < 0) {
			rdt_last_cmd_puts("Out of RMIDs\n");
			goto out_destroy;
		}
		rdtgrp->mon.rmid = ret;

		ret = mkdir_mondata_all(kn, rdtgrp, &rdtgrp->mon.mon_data_kn);
		if (ret) {
			rdt_last_cmd_puts("kernfs subdir error\n");
			goto out_idfree;
		}
	}
	kernfs_activate(kn);

	 
	return 0;

out_idfree:
	free_rmid(rdtgrp->mon.rmid);
out_destroy:
	kernfs_put(rdtgrp->kn);
	kernfs_remove(rdtgrp->kn);
out_free_rgrp:
	kfree(rdtgrp);
out_unlock:
	rdtgroup_kn_unlock(parent_kn);
	return ret;
}

static void mkdir_rdt_prepare_clean(struct rdtgroup *rgrp)
{
	kernfs_remove(rgrp->kn);
	free_rmid(rgrp->mon.rmid);
	rdtgroup_remove(rgrp);
}

 
static int rdtgroup_mkdir_mon(struct kernfs_node *parent_kn,
			      const char *name, umode_t mode)
{
	struct rdtgroup *rdtgrp, *prgrp;
	int ret;

	ret = mkdir_rdt_prepare(parent_kn, name, mode, RDTMON_GROUP, &rdtgrp);
	if (ret)
		return ret;

	prgrp = rdtgrp->mon.parent;
	rdtgrp->closid = prgrp->closid;

	 
	list_add_tail(&rdtgrp->mon.crdtgrp_list, &prgrp->mon.crdtgrp_list);

	rdtgroup_kn_unlock(parent_kn);
	return ret;
}

 
static int rdtgroup_mkdir_ctrl_mon(struct kernfs_node *parent_kn,
				   const char *name, umode_t mode)
{
	struct rdtgroup *rdtgrp;
	struct kernfs_node *kn;
	u32 closid;
	int ret;

	ret = mkdir_rdt_prepare(parent_kn, name, mode, RDTCTRL_GROUP, &rdtgrp);
	if (ret)
		return ret;

	kn = rdtgrp->kn;
	ret = closid_alloc();
	if (ret < 0) {
		rdt_last_cmd_puts("Out of CLOSIDs\n");
		goto out_common_fail;
	}
	closid = ret;
	ret = 0;

	rdtgrp->closid = closid;
	ret = rdtgroup_init_alloc(rdtgrp);
	if (ret < 0)
		goto out_id_free;

	list_add(&rdtgrp->rdtgroup_list, &rdt_all_groups);

	if (rdt_mon_capable) {
		 
		ret = mongroup_create_dir(kn, rdtgrp, "mon_groups", NULL);
		if (ret) {
			rdt_last_cmd_puts("kernfs subdir error\n");
			goto out_del_list;
		}
	}

	goto out_unlock;

out_del_list:
	list_del(&rdtgrp->rdtgroup_list);
out_id_free:
	closid_free(closid);
out_common_fail:
	mkdir_rdt_prepare_clean(rdtgrp);
out_unlock:
	rdtgroup_kn_unlock(parent_kn);
	return ret;
}

 
static bool is_mon_groups(struct kernfs_node *kn, const char *name)
{
	return (!strcmp(kn->name, "mon_groups") &&
		strcmp(name, "mon_groups"));
}

static int rdtgroup_mkdir(struct kernfs_node *parent_kn, const char *name,
			  umode_t mode)
{
	 
	if (strchr(name, '\n'))
		return -EINVAL;

	 
	if (rdt_alloc_capable && parent_kn == rdtgroup_default.kn)
		return rdtgroup_mkdir_ctrl_mon(parent_kn, name, mode);

	 
	if (rdt_mon_capable && is_mon_groups(parent_kn, name))
		return rdtgroup_mkdir_mon(parent_kn, name, mode);

	return -EPERM;
}

static int rdtgroup_rmdir_mon(struct rdtgroup *rdtgrp, cpumask_var_t tmpmask)
{
	struct rdtgroup *prdtgrp = rdtgrp->mon.parent;
	int cpu;

	 
	rdt_move_group_tasks(rdtgrp, prdtgrp, tmpmask);

	 
	for_each_cpu(cpu, &rdtgrp->cpu_mask)
		per_cpu(pqr_state.default_rmid, cpu) = prdtgrp->mon.rmid;
	 
	cpumask_or(tmpmask, tmpmask, &rdtgrp->cpu_mask);
	update_closid_rmid(tmpmask, NULL);

	rdtgrp->flags = RDT_DELETED;
	free_rmid(rdtgrp->mon.rmid);

	 
	WARN_ON(list_empty(&prdtgrp->mon.crdtgrp_list));
	list_del(&rdtgrp->mon.crdtgrp_list);

	kernfs_remove(rdtgrp->kn);

	return 0;
}

static int rdtgroup_ctrl_remove(struct rdtgroup *rdtgrp)
{
	rdtgrp->flags = RDT_DELETED;
	list_del(&rdtgrp->rdtgroup_list);

	kernfs_remove(rdtgrp->kn);
	return 0;
}

static int rdtgroup_rmdir_ctrl(struct rdtgroup *rdtgrp, cpumask_var_t tmpmask)
{
	int cpu;

	 
	rdt_move_group_tasks(rdtgrp, &rdtgroup_default, tmpmask);

	 
	cpumask_or(&rdtgroup_default.cpu_mask,
		   &rdtgroup_default.cpu_mask, &rdtgrp->cpu_mask);

	 
	for_each_cpu(cpu, &rdtgrp->cpu_mask) {
		per_cpu(pqr_state.default_closid, cpu) = rdtgroup_default.closid;
		per_cpu(pqr_state.default_rmid, cpu) = rdtgroup_default.mon.rmid;
	}

	 
	cpumask_or(tmpmask, tmpmask, &rdtgrp->cpu_mask);
	update_closid_rmid(tmpmask, NULL);

	closid_free(rdtgrp->closid);
	free_rmid(rdtgrp->mon.rmid);

	rdtgroup_ctrl_remove(rdtgrp);

	 
	free_all_child_rdtgrp(rdtgrp);

	return 0;
}

static int rdtgroup_rmdir(struct kernfs_node *kn)
{
	struct kernfs_node *parent_kn = kn->parent;
	struct rdtgroup *rdtgrp;
	cpumask_var_t tmpmask;
	int ret = 0;

	if (!zalloc_cpumask_var(&tmpmask, GFP_KERNEL))
		return -ENOMEM;

	rdtgrp = rdtgroup_kn_lock_live(kn);
	if (!rdtgrp) {
		ret = -EPERM;
		goto out;
	}

	 
	if (rdtgrp->type == RDTCTRL_GROUP && parent_kn == rdtgroup_default.kn &&
	    rdtgrp != &rdtgroup_default) {
		if (rdtgrp->mode == RDT_MODE_PSEUDO_LOCKSETUP ||
		    rdtgrp->mode == RDT_MODE_PSEUDO_LOCKED) {
			ret = rdtgroup_ctrl_remove(rdtgrp);
		} else {
			ret = rdtgroup_rmdir_ctrl(rdtgrp, tmpmask);
		}
	} else if (rdtgrp->type == RDTMON_GROUP &&
		 is_mon_groups(parent_kn, kn->name)) {
		ret = rdtgroup_rmdir_mon(rdtgrp, tmpmask);
	} else {
		ret = -EPERM;
	}

out:
	rdtgroup_kn_unlock(kn);
	free_cpumask_var(tmpmask);
	return ret;
}

 
static void mongrp_reparent(struct rdtgroup *rdtgrp,
			    struct rdtgroup *new_prdtgrp,
			    cpumask_var_t cpus)
{
	struct rdtgroup *prdtgrp = rdtgrp->mon.parent;

	WARN_ON(rdtgrp->type != RDTMON_GROUP);
	WARN_ON(new_prdtgrp->type != RDTCTRL_GROUP);

	 
	if (prdtgrp == new_prdtgrp)
		return;

	WARN_ON(list_empty(&prdtgrp->mon.crdtgrp_list));
	list_move_tail(&rdtgrp->mon.crdtgrp_list,
		       &new_prdtgrp->mon.crdtgrp_list);

	rdtgrp->mon.parent = new_prdtgrp;
	rdtgrp->closid = new_prdtgrp->closid;

	 
	rdt_move_group_tasks(rdtgrp, rdtgrp, cpus);

	update_closid_rmid(cpus, NULL);
}

static int rdtgroup_rename(struct kernfs_node *kn,
			   struct kernfs_node *new_parent, const char *new_name)
{
	struct rdtgroup *new_prdtgrp;
	struct rdtgroup *rdtgrp;
	cpumask_var_t tmpmask;
	int ret;

	rdtgrp = kernfs_to_rdtgroup(kn);
	new_prdtgrp = kernfs_to_rdtgroup(new_parent);
	if (!rdtgrp || !new_prdtgrp)
		return -ENOENT;

	 
	rdtgroup_kn_get(rdtgrp, kn);
	rdtgroup_kn_get(new_prdtgrp, new_parent);

	mutex_lock(&rdtgroup_mutex);

	rdt_last_cmd_clear();

	 
	if (kernfs_type(kn) != KERNFS_DIR ||
	    kernfs_type(new_parent) != KERNFS_DIR) {
		rdt_last_cmd_puts("Source and destination must be directories");
		ret = -EPERM;
		goto out;
	}

	if ((rdtgrp->flags & RDT_DELETED) || (new_prdtgrp->flags & RDT_DELETED)) {
		ret = -ENOENT;
		goto out;
	}

	if (rdtgrp->type != RDTMON_GROUP || !kn->parent ||
	    !is_mon_groups(kn->parent, kn->name)) {
		rdt_last_cmd_puts("Source must be a MON group\n");
		ret = -EPERM;
		goto out;
	}

	if (!is_mon_groups(new_parent, new_name)) {
		rdt_last_cmd_puts("Destination must be a mon_groups subdirectory\n");
		ret = -EPERM;
		goto out;
	}

	 
	if (!cpumask_empty(&rdtgrp->cpu_mask) &&
	    rdtgrp->mon.parent != new_prdtgrp) {
		rdt_last_cmd_puts("Cannot move a MON group that monitors CPUs\n");
		ret = -EPERM;
		goto out;
	}

	 
	if (!zalloc_cpumask_var(&tmpmask, GFP_KERNEL)) {
		ret = -ENOMEM;
		goto out;
	}

	 
	ret = kernfs_rename(kn, new_parent, new_name);
	if (!ret)
		mongrp_reparent(rdtgrp, new_prdtgrp, tmpmask);

	free_cpumask_var(tmpmask);

out:
	mutex_unlock(&rdtgroup_mutex);
	rdtgroup_kn_put(rdtgrp, kn);
	rdtgroup_kn_put(new_prdtgrp, new_parent);
	return ret;
}

static int rdtgroup_show_options(struct seq_file *seq, struct kernfs_root *kf)
{
	if (resctrl_arch_get_cdp_enabled(RDT_RESOURCE_L3))
		seq_puts(seq, ",cdp");

	if (resctrl_arch_get_cdp_enabled(RDT_RESOURCE_L2))
		seq_puts(seq, ",cdpl2");

	if (is_mba_sc(&rdt_resources_all[RDT_RESOURCE_MBA].r_resctrl))
		seq_puts(seq, ",mba_MBps");

	return 0;
}

static struct kernfs_syscall_ops rdtgroup_kf_syscall_ops = {
	.mkdir		= rdtgroup_mkdir,
	.rmdir		= rdtgroup_rmdir,
	.rename		= rdtgroup_rename,
	.show_options	= rdtgroup_show_options,
};

static int __init rdtgroup_setup_root(void)
{
	int ret;

	rdt_root = kernfs_create_root(&rdtgroup_kf_syscall_ops,
				      KERNFS_ROOT_CREATE_DEACTIVATED |
				      KERNFS_ROOT_EXTRA_OPEN_PERM_CHECK,
				      &rdtgroup_default);
	if (IS_ERR(rdt_root))
		return PTR_ERR(rdt_root);

	mutex_lock(&rdtgroup_mutex);

	rdtgroup_default.closid = 0;
	rdtgroup_default.mon.rmid = 0;
	rdtgroup_default.type = RDTCTRL_GROUP;
	INIT_LIST_HEAD(&rdtgroup_default.mon.crdtgrp_list);

	list_add(&rdtgroup_default.rdtgroup_list, &rdt_all_groups);

	ret = rdtgroup_add_files(kernfs_root_to_node(rdt_root), RF_CTRL_BASE);
	if (ret) {
		kernfs_destroy_root(rdt_root);
		goto out;
	}

	rdtgroup_default.kn = kernfs_root_to_node(rdt_root);
	kernfs_activate(rdtgroup_default.kn);

out:
	mutex_unlock(&rdtgroup_mutex);

	return ret;
}

static void domain_destroy_mon_state(struct rdt_domain *d)
{
	bitmap_free(d->rmid_busy_llc);
	kfree(d->mbm_total);
	kfree(d->mbm_local);
}

void resctrl_offline_domain(struct rdt_resource *r, struct rdt_domain *d)
{
	lockdep_assert_held(&rdtgroup_mutex);

	if (supports_mba_mbps() && r->rid == RDT_RESOURCE_MBA)
		mba_sc_domain_destroy(r, d);

	if (!r->mon_capable)
		return;

	 
	if (static_branch_unlikely(&rdt_mon_enable_key))
		rmdir_mondata_subdir_allrdtgrp(r, d->id);

	if (is_mbm_enabled())
		cancel_delayed_work(&d->mbm_over);
	if (is_llc_occupancy_enabled() && has_busy_rmid(r, d)) {
		 
		__check_limbo(d, true);
		cancel_delayed_work(&d->cqm_limbo);
	}

	domain_destroy_mon_state(d);
}

static int domain_setup_mon_state(struct rdt_resource *r, struct rdt_domain *d)
{
	size_t tsize;

	if (is_llc_occupancy_enabled()) {
		d->rmid_busy_llc = bitmap_zalloc(r->num_rmid, GFP_KERNEL);
		if (!d->rmid_busy_llc)
			return -ENOMEM;
	}
	if (is_mbm_total_enabled()) {
		tsize = sizeof(*d->mbm_total);
		d->mbm_total = kcalloc(r->num_rmid, tsize, GFP_KERNEL);
		if (!d->mbm_total) {
			bitmap_free(d->rmid_busy_llc);
			return -ENOMEM;
		}
	}
	if (is_mbm_local_enabled()) {
		tsize = sizeof(*d->mbm_local);
		d->mbm_local = kcalloc(r->num_rmid, tsize, GFP_KERNEL);
		if (!d->mbm_local) {
			bitmap_free(d->rmid_busy_llc);
			kfree(d->mbm_total);
			return -ENOMEM;
		}
	}

	return 0;
}

int resctrl_online_domain(struct rdt_resource *r, struct rdt_domain *d)
{
	int err;

	lockdep_assert_held(&rdtgroup_mutex);

	if (supports_mba_mbps() && r->rid == RDT_RESOURCE_MBA)
		 
		return mba_sc_domain_allocate(r, d);

	if (!r->mon_capable)
		return 0;

	err = domain_setup_mon_state(r, d);
	if (err)
		return err;

	if (is_mbm_enabled()) {
		INIT_DELAYED_WORK(&d->mbm_over, mbm_handle_overflow);
		mbm_setup_overflow_handler(d, MBM_OVERFLOW_INTERVAL);
	}

	if (is_llc_occupancy_enabled())
		INIT_DELAYED_WORK(&d->cqm_limbo, cqm_handle_limbo);

	 
	if (static_branch_unlikely(&rdt_mon_enable_key))
		mkdir_mondata_subdir_allrdtgrp(r, d);

	return 0;
}

 
int __init rdtgroup_init(void)
{
	int ret = 0;

	seq_buf_init(&last_cmd_status, last_cmd_status_buf,
		     sizeof(last_cmd_status_buf));

	ret = rdtgroup_setup_root();
	if (ret)
		return ret;

	ret = sysfs_create_mount_point(fs_kobj, "resctrl");
	if (ret)
		goto cleanup_root;

	ret = register_filesystem(&rdt_fs_type);
	if (ret)
		goto cleanup_mountpoint;

	 
	debugfs_resctrl = debugfs_create_dir("resctrl", NULL);

	return 0;

cleanup_mountpoint:
	sysfs_remove_mount_point(fs_kobj, "resctrl");
cleanup_root:
	kernfs_destroy_root(rdt_root);

	return ret;
}

void __exit rdtgroup_exit(void)
{
	debugfs_remove_recursive(debugfs_resctrl);
	unregister_filesystem(&rdt_fs_type);
	sysfs_remove_mount_point(fs_kobj, "resctrl");
	kernfs_destroy_root(rdt_root);
}
