 

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpuset.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mempolicy.h>
#include <linux/mm.h>
#include <linux/memory.h>
#include <linux/export.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/sched/deadline.h>
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#include <linux/security.h>
#include <linux/spinlock.h>
#include <linux/oom.h>
#include <linux/sched/isolation.h>
#include <linux/cgroup.h>
#include <linux/wait.h>

DEFINE_STATIC_KEY_FALSE(cpusets_pre_enable_key);
DEFINE_STATIC_KEY_FALSE(cpusets_enabled_key);

 
DEFINE_STATIC_KEY_FALSE(cpusets_insane_config_key);

 

struct fmeter {
	int cnt;		 
	int val;		 
	time64_t time;		 
	spinlock_t lock;	 
};

 
enum prs_errcode {
	PERR_NONE = 0,
	PERR_INVCPUS,
	PERR_INVPARENT,
	PERR_NOTPART,
	PERR_NOTEXCL,
	PERR_NOCPUS,
	PERR_HOTPLUG,
	PERR_CPUSEMPTY,
};

static const char * const perr_strings[] = {
	[PERR_INVCPUS]   = "Invalid cpu list in cpuset.cpus",
	[PERR_INVPARENT] = "Parent is an invalid partition root",
	[PERR_NOTPART]   = "Parent is not a partition root",
	[PERR_NOTEXCL]   = "Cpu list in cpuset.cpus not exclusive",
	[PERR_NOCPUS]    = "Parent unable to distribute cpu downstream",
	[PERR_HOTPLUG]   = "No cpu available due to hotplug",
	[PERR_CPUSEMPTY] = "cpuset.cpus is empty",
};

struct cpuset {
	struct cgroup_subsys_state css;

	unsigned long flags;		 

	 

	 
	cpumask_var_t cpus_allowed;
	nodemask_t mems_allowed;

	 
	cpumask_var_t effective_cpus;
	nodemask_t effective_mems;

	 
	cpumask_var_t subparts_cpus;

	 
	nodemask_t old_mems_allowed;

	struct fmeter fmeter;		 

	 
	int attach_in_progress;

	 
	int pn;

	 
	int relax_domain_level;

	 
	int nr_subparts_cpus;

	 
	int partition_root_state;

	 
	int use_parent_ecpus;
	int child_ecpus_count;

	 
	int nr_deadline_tasks;
	int nr_migrate_dl_tasks;
	u64 sum_migrate_dl_bw;

	 
	enum prs_errcode prs_err;

	 
	struct cgroup_file partition_file;
};

 
#define PRS_MEMBER		0
#define PRS_ROOT		1
#define PRS_ISOLATED		2
#define PRS_INVALID_ROOT	-1
#define PRS_INVALID_ISOLATED	-2

static inline bool is_prs_invalid(int prs_state)
{
	return prs_state < 0;
}

 
struct tmpmasks {
	cpumask_var_t addmask, delmask;	 
	cpumask_var_t new_cpus;		 
};

static inline struct cpuset *css_cs(struct cgroup_subsys_state *css)
{
	return css ? container_of(css, struct cpuset, css) : NULL;
}

 
static inline struct cpuset *task_cs(struct task_struct *task)
{
	return css_cs(task_css(task, cpuset_cgrp_id));
}

static inline struct cpuset *parent_cs(struct cpuset *cs)
{
	return css_cs(cs->css.parent);
}

void inc_dl_tasks_cs(struct task_struct *p)
{
	struct cpuset *cs = task_cs(p);

	cs->nr_deadline_tasks++;
}

void dec_dl_tasks_cs(struct task_struct *p)
{
	struct cpuset *cs = task_cs(p);

	cs->nr_deadline_tasks--;
}

 
typedef enum {
	CS_ONLINE,
	CS_CPU_EXCLUSIVE,
	CS_MEM_EXCLUSIVE,
	CS_MEM_HARDWALL,
	CS_MEMORY_MIGRATE,
	CS_SCHED_LOAD_BALANCE,
	CS_SPREAD_PAGE,
	CS_SPREAD_SLAB,
} cpuset_flagbits_t;

 
static inline bool is_cpuset_online(struct cpuset *cs)
{
	return test_bit(CS_ONLINE, &cs->flags) && !css_is_dying(&cs->css);
}

static inline int is_cpu_exclusive(const struct cpuset *cs)
{
	return test_bit(CS_CPU_EXCLUSIVE, &cs->flags);
}

static inline int is_mem_exclusive(const struct cpuset *cs)
{
	return test_bit(CS_MEM_EXCLUSIVE, &cs->flags);
}

static inline int is_mem_hardwall(const struct cpuset *cs)
{
	return test_bit(CS_MEM_HARDWALL, &cs->flags);
}

static inline int is_sched_load_balance(const struct cpuset *cs)
{
	return test_bit(CS_SCHED_LOAD_BALANCE, &cs->flags);
}

static inline int is_memory_migrate(const struct cpuset *cs)
{
	return test_bit(CS_MEMORY_MIGRATE, &cs->flags);
}

static inline int is_spread_page(const struct cpuset *cs)
{
	return test_bit(CS_SPREAD_PAGE, &cs->flags);
}

static inline int is_spread_slab(const struct cpuset *cs)
{
	return test_bit(CS_SPREAD_SLAB, &cs->flags);
}

static inline int is_partition_valid(const struct cpuset *cs)
{
	return cs->partition_root_state > 0;
}

static inline int is_partition_invalid(const struct cpuset *cs)
{
	return cs->partition_root_state < 0;
}

 
static inline void make_partition_invalid(struct cpuset *cs)
{
	if (is_partition_valid(cs))
		cs->partition_root_state = -cs->partition_root_state;
}

 
static inline void notify_partition_change(struct cpuset *cs, int old_prs)
{
	if (old_prs == cs->partition_root_state)
		return;
	cgroup_file_notify(&cs->partition_file);

	 
	if (is_partition_valid(cs))
		WRITE_ONCE(cs->prs_err, PERR_NONE);
}

static struct cpuset top_cpuset = {
	.flags = ((1 << CS_ONLINE) | (1 << CS_CPU_EXCLUSIVE) |
		  (1 << CS_MEM_EXCLUSIVE)),
	.partition_root_state = PRS_ROOT,
};

 
#define cpuset_for_each_child(child_cs, pos_css, parent_cs)		\
	css_for_each_child((pos_css), &(parent_cs)->css)		\
		if (is_cpuset_online(((child_cs) = css_cs((pos_css)))))

 
#define cpuset_for_each_descendant_pre(des_cs, pos_css, root_cs)	\
	css_for_each_descendant_pre((pos_css), &(root_cs)->css)		\
		if (is_cpuset_online(((des_cs) = css_cs((pos_css)))))

 

static DEFINE_MUTEX(cpuset_mutex);

void cpuset_lock(void)
{
	mutex_lock(&cpuset_mutex);
}

void cpuset_unlock(void)
{
	mutex_unlock(&cpuset_mutex);
}

static DEFINE_SPINLOCK(callback_lock);

static struct workqueue_struct *cpuset_migrate_mm_wq;

 
static void cpuset_hotplug_workfn(struct work_struct *work);
static DECLARE_WORK(cpuset_hotplug_work, cpuset_hotplug_workfn);

static DECLARE_WAIT_QUEUE_HEAD(cpuset_attach_wq);

static inline void check_insane_mems_config(nodemask_t *nodes)
{
	if (!cpusets_insane_config() &&
		movable_only_nodes(nodes)) {
		static_branch_enable(&cpusets_insane_config_key);
		pr_info("Unsupported (movable nodes only) cpuset configuration detected (nmask=%*pbl)!\n"
			"Cpuset allocations might fail even with a lot of memory available.\n",
			nodemask_pr_args(nodes));
	}
}

 
static inline bool is_in_v2_mode(void)
{
	return cgroup_subsys_on_dfl(cpuset_cgrp_subsys) ||
	      (cpuset_cgrp_subsys.root->flags & CGRP_ROOT_CPUSET_V2_MODE);
}

 
static inline bool partition_is_populated(struct cpuset *cs,
					  struct cpuset *excluded_child)
{
	struct cgroup_subsys_state *css;
	struct cpuset *child;

	if (cs->css.cgroup->nr_populated_csets)
		return true;
	if (!excluded_child && !cs->nr_subparts_cpus)
		return cgroup_is_populated(cs->css.cgroup);

	rcu_read_lock();
	cpuset_for_each_child(child, css, cs) {
		if (child == excluded_child)
			continue;
		if (is_partition_valid(child))
			continue;
		if (cgroup_is_populated(child->css.cgroup)) {
			rcu_read_unlock();
			return true;
		}
	}
	rcu_read_unlock();
	return false;
}

 
static void guarantee_online_cpus(struct task_struct *tsk,
				  struct cpumask *pmask)
{
	const struct cpumask *possible_mask = task_cpu_possible_mask(tsk);
	struct cpuset *cs;

	if (WARN_ON(!cpumask_and(pmask, possible_mask, cpu_online_mask)))
		cpumask_copy(pmask, cpu_online_mask);

	rcu_read_lock();
	cs = task_cs(tsk);

	while (!cpumask_intersects(cs->effective_cpus, pmask)) {
		cs = parent_cs(cs);
		if (unlikely(!cs)) {
			 
			goto out_unlock;
		}
	}
	cpumask_and(pmask, pmask, cs->effective_cpus);

out_unlock:
	rcu_read_unlock();
}

 
static void guarantee_online_mems(struct cpuset *cs, nodemask_t *pmask)
{
	while (!nodes_intersects(cs->effective_mems, node_states[N_MEMORY]))
		cs = parent_cs(cs);
	nodes_and(*pmask, cs->effective_mems, node_states[N_MEMORY]);
}

 
static void cpuset_update_task_spread_flags(struct cpuset *cs,
					struct task_struct *tsk)
{
	if (cgroup_subsys_on_dfl(cpuset_cgrp_subsys))
		return;

	if (is_spread_page(cs))
		task_set_spread_page(tsk);
	else
		task_clear_spread_page(tsk);

	if (is_spread_slab(cs))
		task_set_spread_slab(tsk);
	else
		task_clear_spread_slab(tsk);
}

 

static int is_cpuset_subset(const struct cpuset *p, const struct cpuset *q)
{
	return	cpumask_subset(p->cpus_allowed, q->cpus_allowed) &&
		nodes_subset(p->mems_allowed, q->mems_allowed) &&
		is_cpu_exclusive(p) <= is_cpu_exclusive(q) &&
		is_mem_exclusive(p) <= is_mem_exclusive(q);
}

 
static inline int alloc_cpumasks(struct cpuset *cs, struct tmpmasks *tmp)
{
	cpumask_var_t *pmask1, *pmask2, *pmask3;

	if (cs) {
		pmask1 = &cs->cpus_allowed;
		pmask2 = &cs->effective_cpus;
		pmask3 = &cs->subparts_cpus;
	} else {
		pmask1 = &tmp->new_cpus;
		pmask2 = &tmp->addmask;
		pmask3 = &tmp->delmask;
	}

	if (!zalloc_cpumask_var(pmask1, GFP_KERNEL))
		return -ENOMEM;

	if (!zalloc_cpumask_var(pmask2, GFP_KERNEL))
		goto free_one;

	if (!zalloc_cpumask_var(pmask3, GFP_KERNEL))
		goto free_two;

	return 0;

free_two:
	free_cpumask_var(*pmask2);
free_one:
	free_cpumask_var(*pmask1);
	return -ENOMEM;
}

 
static inline void free_cpumasks(struct cpuset *cs, struct tmpmasks *tmp)
{
	if (cs) {
		free_cpumask_var(cs->cpus_allowed);
		free_cpumask_var(cs->effective_cpus);
		free_cpumask_var(cs->subparts_cpus);
	}
	if (tmp) {
		free_cpumask_var(tmp->new_cpus);
		free_cpumask_var(tmp->addmask);
		free_cpumask_var(tmp->delmask);
	}
}

 
static struct cpuset *alloc_trial_cpuset(struct cpuset *cs)
{
	struct cpuset *trial;

	trial = kmemdup(cs, sizeof(*cs), GFP_KERNEL);
	if (!trial)
		return NULL;

	if (alloc_cpumasks(trial, NULL)) {
		kfree(trial);
		return NULL;
	}

	cpumask_copy(trial->cpus_allowed, cs->cpus_allowed);
	cpumask_copy(trial->effective_cpus, cs->effective_cpus);
	return trial;
}

 
static inline void free_cpuset(struct cpuset *cs)
{
	free_cpumasks(cs, NULL);
	kfree(cs);
}

 
static int validate_change_legacy(struct cpuset *cur, struct cpuset *trial)
{
	struct cgroup_subsys_state *css;
	struct cpuset *c, *par;
	int ret;

	WARN_ON_ONCE(!rcu_read_lock_held());

	 
	ret = -EBUSY;
	cpuset_for_each_child(c, css, cur)
		if (!is_cpuset_subset(c, trial))
			goto out;

	 
	ret = -EACCES;
	par = parent_cs(cur);
	if (par && !is_cpuset_subset(trial, par))
		goto out;

	ret = 0;
out:
	return ret;
}

 

static int validate_change(struct cpuset *cur, struct cpuset *trial)
{
	struct cgroup_subsys_state *css;
	struct cpuset *c, *par;
	int ret = 0;

	rcu_read_lock();

	if (!is_in_v2_mode())
		ret = validate_change_legacy(cur, trial);
	if (ret)
		goto out;

	 
	if (cur == &top_cpuset)
		goto out;

	par = parent_cs(cur);

	 
	ret = -ENOSPC;
	if ((cgroup_is_populated(cur->css.cgroup) || cur->attach_in_progress)) {
		if (!cpumask_empty(cur->cpus_allowed) &&
		    cpumask_empty(trial->cpus_allowed))
			goto out;
		if (!nodes_empty(cur->mems_allowed) &&
		    nodes_empty(trial->mems_allowed))
			goto out;
	}

	 
	ret = -EBUSY;
	if (is_cpu_exclusive(cur) &&
	    !cpuset_cpumask_can_shrink(cur->cpus_allowed,
				       trial->cpus_allowed))
		goto out;

	 
	ret = -EINVAL;
	cpuset_for_each_child(c, css, par) {
		if ((is_cpu_exclusive(trial) || is_cpu_exclusive(c)) &&
		    c != cur &&
		    cpumask_intersects(trial->cpus_allowed, c->cpus_allowed))
			goto out;
		if ((is_mem_exclusive(trial) || is_mem_exclusive(c)) &&
		    c != cur &&
		    nodes_intersects(trial->mems_allowed, c->mems_allowed))
			goto out;
	}

	ret = 0;
out:
	rcu_read_unlock();
	return ret;
}

#ifdef CONFIG_SMP
 
static int cpusets_overlap(struct cpuset *a, struct cpuset *b)
{
	return cpumask_intersects(a->effective_cpus, b->effective_cpus);
}

static void
update_domain_attr(struct sched_domain_attr *dattr, struct cpuset *c)
{
	if (dattr->relax_domain_level < c->relax_domain_level)
		dattr->relax_domain_level = c->relax_domain_level;
	return;
}

static void update_domain_attr_tree(struct sched_domain_attr *dattr,
				    struct cpuset *root_cs)
{
	struct cpuset *cp;
	struct cgroup_subsys_state *pos_css;

	rcu_read_lock();
	cpuset_for_each_descendant_pre(cp, pos_css, root_cs) {
		 
		if (cpumask_empty(cp->cpus_allowed)) {
			pos_css = css_rightmost_descendant(pos_css);
			continue;
		}

		if (is_sched_load_balance(cp))
			update_domain_attr(dattr, cp);
	}
	rcu_read_unlock();
}

 
static inline int nr_cpusets(void)
{
	 
	return static_key_count(&cpusets_enabled_key.key) + 1;
}

 
static int generate_sched_domains(cpumask_var_t **domains,
			struct sched_domain_attr **attributes)
{
	struct cpuset *cp;	 
	struct cpuset **csa;	 
	int csn;		 
	int i, j, k;		 
	cpumask_var_t *doms;	 
	struct sched_domain_attr *dattr;   
	int ndoms = 0;		 
	int nslot;		 
	struct cgroup_subsys_state *pos_css;
	bool root_load_balance = is_sched_load_balance(&top_cpuset);

	doms = NULL;
	dattr = NULL;
	csa = NULL;

	 
	if (root_load_balance && !top_cpuset.nr_subparts_cpus) {
		ndoms = 1;
		doms = alloc_sched_domains(ndoms);
		if (!doms)
			goto done;

		dattr = kmalloc(sizeof(struct sched_domain_attr), GFP_KERNEL);
		if (dattr) {
			*dattr = SD_ATTR_INIT;
			update_domain_attr_tree(dattr, &top_cpuset);
		}
		cpumask_and(doms[0], top_cpuset.effective_cpus,
			    housekeeping_cpumask(HK_TYPE_DOMAIN));

		goto done;
	}

	csa = kmalloc_array(nr_cpusets(), sizeof(cp), GFP_KERNEL);
	if (!csa)
		goto done;
	csn = 0;

	rcu_read_lock();
	if (root_load_balance)
		csa[csn++] = &top_cpuset;
	cpuset_for_each_descendant_pre(cp, pos_css, &top_cpuset) {
		if (cp == &top_cpuset)
			continue;
		 
		if (!cpumask_empty(cp->cpus_allowed) &&
		    !(is_sched_load_balance(cp) &&
		      cpumask_intersects(cp->cpus_allowed,
					 housekeeping_cpumask(HK_TYPE_DOMAIN))))
			continue;

		if (root_load_balance &&
		    cpumask_subset(cp->cpus_allowed, top_cpuset.effective_cpus))
			continue;

		if (is_sched_load_balance(cp) &&
		    !cpumask_empty(cp->effective_cpus))
			csa[csn++] = cp;

		 
		if (!is_partition_valid(cp))
			pos_css = css_rightmost_descendant(pos_css);
	}
	rcu_read_unlock();

	for (i = 0; i < csn; i++)
		csa[i]->pn = i;
	ndoms = csn;

restart:
	 
	for (i = 0; i < csn; i++) {
		struct cpuset *a = csa[i];
		int apn = a->pn;

		for (j = 0; j < csn; j++) {
			struct cpuset *b = csa[j];
			int bpn = b->pn;

			if (apn != bpn && cpusets_overlap(a, b)) {
				for (k = 0; k < csn; k++) {
					struct cpuset *c = csa[k];

					if (c->pn == bpn)
						c->pn = apn;
				}
				ndoms--;	 
				goto restart;
			}
		}
	}

	 
	doms = alloc_sched_domains(ndoms);
	if (!doms)
		goto done;

	 
	dattr = kmalloc_array(ndoms, sizeof(struct sched_domain_attr),
			      GFP_KERNEL);

	for (nslot = 0, i = 0; i < csn; i++) {
		struct cpuset *a = csa[i];
		struct cpumask *dp;
		int apn = a->pn;

		if (apn < 0) {
			 
			continue;
		}

		dp = doms[nslot];

		if (nslot == ndoms) {
			static int warnings = 10;
			if (warnings) {
				pr_warn("rebuild_sched_domains confused: nslot %d, ndoms %d, csn %d, i %d, apn %d\n",
					nslot, ndoms, csn, i, apn);
				warnings--;
			}
			continue;
		}

		cpumask_clear(dp);
		if (dattr)
			*(dattr + nslot) = SD_ATTR_INIT;
		for (j = i; j < csn; j++) {
			struct cpuset *b = csa[j];

			if (apn == b->pn) {
				cpumask_or(dp, dp, b->effective_cpus);
				cpumask_and(dp, dp, housekeeping_cpumask(HK_TYPE_DOMAIN));
				if (dattr)
					update_domain_attr_tree(dattr + nslot, b);

				 
				b->pn = -1;
			}
		}
		nslot++;
	}
	BUG_ON(nslot != ndoms);

done:
	kfree(csa);

	 
	if (doms == NULL)
		ndoms = 1;

	*domains    = doms;
	*attributes = dattr;
	return ndoms;
}

static void dl_update_tasks_root_domain(struct cpuset *cs)
{
	struct css_task_iter it;
	struct task_struct *task;

	if (cs->nr_deadline_tasks == 0)
		return;

	css_task_iter_start(&cs->css, 0, &it);

	while ((task = css_task_iter_next(&it)))
		dl_add_task_root_domain(task);

	css_task_iter_end(&it);
}

static void dl_rebuild_rd_accounting(void)
{
	struct cpuset *cs = NULL;
	struct cgroup_subsys_state *pos_css;

	lockdep_assert_held(&cpuset_mutex);
	lockdep_assert_cpus_held();
	lockdep_assert_held(&sched_domains_mutex);

	rcu_read_lock();

	 
	dl_clear_root_domain(&def_root_domain);

	cpuset_for_each_descendant_pre(cs, pos_css, &top_cpuset) {

		if (cpumask_empty(cs->effective_cpus)) {
			pos_css = css_rightmost_descendant(pos_css);
			continue;
		}

		css_get(&cs->css);

		rcu_read_unlock();

		dl_update_tasks_root_domain(cs);

		rcu_read_lock();
		css_put(&cs->css);
	}
	rcu_read_unlock();
}

static void
partition_and_rebuild_sched_domains(int ndoms_new, cpumask_var_t doms_new[],
				    struct sched_domain_attr *dattr_new)
{
	mutex_lock(&sched_domains_mutex);
	partition_sched_domains_locked(ndoms_new, doms_new, dattr_new);
	dl_rebuild_rd_accounting();
	mutex_unlock(&sched_domains_mutex);
}

 
static void rebuild_sched_domains_locked(void)
{
	struct cgroup_subsys_state *pos_css;
	struct sched_domain_attr *attr;
	cpumask_var_t *doms;
	struct cpuset *cs;
	int ndoms;

	lockdep_assert_cpus_held();
	lockdep_assert_held(&cpuset_mutex);

	 
	if (!top_cpuset.nr_subparts_cpus &&
	    !cpumask_equal(top_cpuset.effective_cpus, cpu_active_mask))
		return;

	 
	if (top_cpuset.nr_subparts_cpus) {
		rcu_read_lock();
		cpuset_for_each_descendant_pre(cs, pos_css, &top_cpuset) {
			if (!is_partition_valid(cs)) {
				pos_css = css_rightmost_descendant(pos_css);
				continue;
			}
			if (!cpumask_subset(cs->effective_cpus,
					    cpu_active_mask)) {
				rcu_read_unlock();
				return;
			}
		}
		rcu_read_unlock();
	}

	 
	ndoms = generate_sched_domains(&doms, &attr);

	 
	partition_and_rebuild_sched_domains(ndoms, doms, attr);
}
#else  
static void rebuild_sched_domains_locked(void)
{
}
#endif  

void rebuild_sched_domains(void)
{
	cpus_read_lock();
	mutex_lock(&cpuset_mutex);
	rebuild_sched_domains_locked();
	mutex_unlock(&cpuset_mutex);
	cpus_read_unlock();
}

 
static void update_tasks_cpumask(struct cpuset *cs, struct cpumask *new_cpus)
{
	struct css_task_iter it;
	struct task_struct *task;
	bool top_cs = cs == &top_cpuset;

	css_task_iter_start(&cs->css, 0, &it);
	while ((task = css_task_iter_next(&it))) {
		const struct cpumask *possible_mask = task_cpu_possible_mask(task);

		if (top_cs) {
			 
			if (kthread_is_per_cpu(task))
				continue;
			cpumask_andnot(new_cpus, possible_mask, cs->subparts_cpus);
		} else {
			cpumask_and(new_cpus, possible_mask, cs->effective_cpus);
		}
		set_cpus_allowed_ptr(task, new_cpus);
	}
	css_task_iter_end(&it);
}

 
static void compute_effective_cpumask(struct cpumask *new_cpus,
				      struct cpuset *cs, struct cpuset *parent)
{
	if (parent->nr_subparts_cpus && is_partition_valid(cs)) {
		cpumask_or(new_cpus, parent->effective_cpus,
			   parent->subparts_cpus);
		cpumask_and(new_cpus, new_cpus, cs->cpus_allowed);
		cpumask_and(new_cpus, new_cpus, cpu_active_mask);
	} else {
		cpumask_and(new_cpus, cs->cpus_allowed, parent->effective_cpus);
	}
}

 
enum subparts_cmd {
	partcmd_enable,		 
	partcmd_disable,	 
	partcmd_update,		 
	partcmd_invalidate,	 
};

static int update_flag(cpuset_flagbits_t bit, struct cpuset *cs,
		       int turning_on);
static void update_sibling_cpumasks(struct cpuset *parent, struct cpuset *cs,
				    struct tmpmasks *tmp);

 
static int update_partition_exclusive(struct cpuset *cs, int new_prs)
{
	bool exclusive = (new_prs > 0);

	if (exclusive && !is_cpu_exclusive(cs)) {
		if (update_flag(CS_CPU_EXCLUSIVE, cs, 1))
			return PERR_NOTEXCL;
	} else if (!exclusive && is_cpu_exclusive(cs)) {
		 
		update_flag(CS_CPU_EXCLUSIVE, cs, 0);
	}
	return 0;
}

 
static void update_partition_sd_lb(struct cpuset *cs, int old_prs)
{
	int new_prs = cs->partition_root_state;
	bool rebuild_domains = (new_prs > 0) || (old_prs > 0);
	bool new_lb;

	 
	if (new_prs > 0) {
		new_lb = (new_prs != PRS_ISOLATED);
	} else {
		new_lb = is_sched_load_balance(parent_cs(cs));
	}
	if (new_lb != !!is_sched_load_balance(cs)) {
		rebuild_domains = true;
		if (new_lb)
			set_bit(CS_SCHED_LOAD_BALANCE, &cs->flags);
		else
			clear_bit(CS_SCHED_LOAD_BALANCE, &cs->flags);
	}

	if (rebuild_domains)
		rebuild_sched_domains_locked();
}

 
static int update_parent_subparts_cpumask(struct cpuset *cs, int cmd,
					  struct cpumask *newmask,
					  struct tmpmasks *tmp)
{
	struct cpuset *parent = parent_cs(cs);
	int adding;	 
	int deleting;	 
	int old_prs, new_prs;
	int part_error = PERR_NONE;	 

	lockdep_assert_held(&cpuset_mutex);

	 
	if (!is_partition_valid(parent)) {
		return is_partition_invalid(parent)
		       ? PERR_INVPARENT : PERR_NOTPART;
	}
	if (!newmask && cpumask_empty(cs->cpus_allowed))
		return PERR_CPUSEMPTY;

	 
	adding = deleting = false;
	old_prs = new_prs = cs->partition_root_state;
	if (cmd == partcmd_enable) {
		 
		if (!cpumask_intersects(cs->cpus_allowed, parent->cpus_allowed))
			return PERR_INVCPUS;

		 
		if (cpumask_subset(parent->effective_cpus, cs->cpus_allowed) &&
		    partition_is_populated(parent, cs))
			return PERR_NOCPUS;

		cpumask_copy(tmp->addmask, cs->cpus_allowed);
		adding = true;
	} else if (cmd == partcmd_disable) {
		 
		deleting = !is_prs_invalid(old_prs) &&
			   cpumask_and(tmp->delmask, cs->cpus_allowed,
				       parent->subparts_cpus);
	} else if (cmd == partcmd_invalidate) {
		if (is_prs_invalid(old_prs))
			return 0;

		 
		deleting = cpumask_and(tmp->delmask, cs->cpus_allowed,
				       parent->subparts_cpus);
		if (old_prs > 0) {
			new_prs = -old_prs;
			part_error = PERR_NOTEXCL;
		}
	} else if (newmask) {
		 
		cpumask_andnot(tmp->delmask, cs->cpus_allowed, newmask);
		deleting = cpumask_and(tmp->delmask, tmp->delmask,
				       parent->subparts_cpus);

		cpumask_and(tmp->addmask, newmask, parent->cpus_allowed);
		adding = cpumask_andnot(tmp->addmask, tmp->addmask,
					parent->subparts_cpus);
		 
		if (cpumask_empty(newmask)) {
			part_error = PERR_CPUSEMPTY;
		 
		} else if (adding &&
		    cpumask_subset(parent->effective_cpus, tmp->addmask) &&
		    !cpumask_intersects(tmp->delmask, cpu_active_mask) &&
		    partition_is_populated(parent, cs)) {
			part_error = PERR_NOCPUS;
			adding = false;
			deleting = cpumask_and(tmp->delmask, cs->cpus_allowed,
					       parent->subparts_cpus);
		}
	} else {
		 
		cpumask_and(tmp->addmask, cs->cpus_allowed,
					  parent->cpus_allowed);
		adding = cpumask_andnot(tmp->addmask, tmp->addmask,
					parent->subparts_cpus);

		if ((is_partition_valid(cs) && !parent->nr_subparts_cpus) ||
		    (adding &&
		     cpumask_subset(parent->effective_cpus, tmp->addmask) &&
		     partition_is_populated(parent, cs))) {
			part_error = PERR_NOCPUS;
			adding = false;
		}

		if (part_error && is_partition_valid(cs) &&
		    parent->nr_subparts_cpus)
			deleting = cpumask_and(tmp->delmask, cs->cpus_allowed,
					       parent->subparts_cpus);
	}
	if (part_error)
		WRITE_ONCE(cs->prs_err, part_error);

	if (cmd == partcmd_update) {
		 
		switch (cs->partition_root_state) {
		case PRS_ROOT:
		case PRS_ISOLATED:
			if (part_error)
				new_prs = -old_prs;
			break;
		case PRS_INVALID_ROOT:
		case PRS_INVALID_ISOLATED:
			if (!part_error)
				new_prs = -old_prs;
			break;
		}
	}

	if (!adding && !deleting && (new_prs == old_prs))
		return 0;

	 
	if (old_prs != new_prs) {
		int err = update_partition_exclusive(cs, new_prs);

		if (err)
			return err;
	}

	 
	spin_lock_irq(&callback_lock);
	if (adding) {
		cpumask_or(parent->subparts_cpus,
			   parent->subparts_cpus, tmp->addmask);
		cpumask_andnot(parent->effective_cpus,
			       parent->effective_cpus, tmp->addmask);
	}
	if (deleting) {
		cpumask_andnot(parent->subparts_cpus,
			       parent->subparts_cpus, tmp->delmask);
		 
		cpumask_and(tmp->delmask, tmp->delmask, cpu_active_mask);
		cpumask_or(parent->effective_cpus,
			   parent->effective_cpus, tmp->delmask);
	}

	parent->nr_subparts_cpus = cpumask_weight(parent->subparts_cpus);

	if (old_prs != new_prs)
		cs->partition_root_state = new_prs;

	spin_unlock_irq(&callback_lock);

	if (adding || deleting) {
		update_tasks_cpumask(parent, tmp->addmask);
		if (parent->child_ecpus_count)
			update_sibling_cpumasks(parent, cs, tmp);
	}

	 
	if ((cmd == partcmd_update) && !newmask && cpus_read_trylock()) {
		update_partition_sd_lb(cs, old_prs);
		cpus_read_unlock();
	}

	notify_partition_change(cs, old_prs);
	return 0;
}

 
#define HIER_CHECKALL		0x01	 
#define HIER_NO_SD_REBUILD	0x02	 

 
static void update_cpumasks_hier(struct cpuset *cs, struct tmpmasks *tmp,
				 int flags)
{
	struct cpuset *cp;
	struct cgroup_subsys_state *pos_css;
	bool need_rebuild_sched_domains = false;
	int old_prs, new_prs;

	rcu_read_lock();
	cpuset_for_each_descendant_pre(cp, pos_css, cs) {
		struct cpuset *parent = parent_cs(cp);
		bool update_parent = false;

		compute_effective_cpumask(tmp->new_cpus, cp, parent);

		 
		if (is_in_v2_mode() && cpumask_empty(tmp->new_cpus)) {
			if (is_partition_valid(cp) &&
			    cpumask_equal(cp->cpus_allowed, cp->subparts_cpus))
				goto update_parent_subparts;

			cpumask_copy(tmp->new_cpus, parent->effective_cpus);
			if (!cp->use_parent_ecpus) {
				cp->use_parent_ecpus = true;
				parent->child_ecpus_count++;
			}
		} else if (cp->use_parent_ecpus) {
			cp->use_parent_ecpus = false;
			WARN_ON_ONCE(!parent->child_ecpus_count);
			parent->child_ecpus_count--;
		}

		 
		if (!cp->partition_root_state && !(flags & HIER_CHECKALL) &&
		    cpumask_equal(tmp->new_cpus, cp->effective_cpus) &&
		    (!cgroup_subsys_on_dfl(cpuset_cgrp_subsys) ||
		    (is_sched_load_balance(parent) == is_sched_load_balance(cp)))) {
			pos_css = css_rightmost_descendant(pos_css);
			continue;
		}

update_parent_subparts:
		 
		old_prs = new_prs = cp->partition_root_state;
		if ((cp != cs) && old_prs) {
			switch (parent->partition_root_state) {
			case PRS_ROOT:
			case PRS_ISOLATED:
				update_parent = true;
				break;

			default:
				 
				if (is_partition_valid(cp))
					new_prs = -cp->partition_root_state;
				WRITE_ONCE(cp->prs_err,
					   is_partition_invalid(parent)
					   ? PERR_INVPARENT : PERR_NOTPART);
				break;
			}
		}

		if (!css_tryget_online(&cp->css))
			continue;
		rcu_read_unlock();

		if (update_parent) {
			update_parent_subparts_cpumask(cp, partcmd_update, NULL,
						       tmp);
			 
			new_prs = cp->partition_root_state;
		}

		spin_lock_irq(&callback_lock);

		if (cp->nr_subparts_cpus && !is_partition_valid(cp)) {
			 
			cpumask_or(tmp->new_cpus, tmp->new_cpus,
				   cp->subparts_cpus);
			cpumask_and(tmp->new_cpus, tmp->new_cpus,
				   cpu_active_mask);
			cp->nr_subparts_cpus = 0;
			cpumask_clear(cp->subparts_cpus);
		}

		cpumask_copy(cp->effective_cpus, tmp->new_cpus);
		if (cp->nr_subparts_cpus) {
			 
			cpumask_andnot(cp->effective_cpus, cp->effective_cpus,
				       cp->subparts_cpus);
		}

		cp->partition_root_state = new_prs;
		spin_unlock_irq(&callback_lock);

		notify_partition_change(cp, old_prs);

		WARN_ON(!is_in_v2_mode() &&
			!cpumask_equal(cp->cpus_allowed, cp->effective_cpus));

		update_tasks_cpumask(cp, tmp->new_cpus);

		 
		if (cgroup_subsys_on_dfl(cpuset_cgrp_subsys) &&
		    !is_partition_valid(cp) &&
		    (is_sched_load_balance(parent) != is_sched_load_balance(cp))) {
			if (is_sched_load_balance(parent))
				set_bit(CS_SCHED_LOAD_BALANCE, &cp->flags);
			else
				clear_bit(CS_SCHED_LOAD_BALANCE, &cp->flags);
		}

		 
		if (!cpumask_empty(cp->cpus_allowed) &&
		    is_sched_load_balance(cp) &&
		   (!cgroup_subsys_on_dfl(cpuset_cgrp_subsys) ||
		    is_partition_valid(cp)))
			need_rebuild_sched_domains = true;

		rcu_read_lock();
		css_put(&cp->css);
	}
	rcu_read_unlock();

	if (need_rebuild_sched_domains && !(flags & HIER_NO_SD_REBUILD))
		rebuild_sched_domains_locked();
}

 
static void update_sibling_cpumasks(struct cpuset *parent, struct cpuset *cs,
				    struct tmpmasks *tmp)
{
	struct cpuset *sibling;
	struct cgroup_subsys_state *pos_css;

	lockdep_assert_held(&cpuset_mutex);

	 
	rcu_read_lock();
	cpuset_for_each_child(sibling, pos_css, parent) {
		if (sibling == cs)
			continue;
		if (!sibling->use_parent_ecpus)
			continue;
		if (!css_tryget_online(&sibling->css))
			continue;

		rcu_read_unlock();
		update_cpumasks_hier(sibling, tmp, HIER_NO_SD_REBUILD);
		rcu_read_lock();
		css_put(&sibling->css);
	}
	rcu_read_unlock();
}

 
static int update_cpumask(struct cpuset *cs, struct cpuset *trialcs,
			  const char *buf)
{
	int retval;
	struct tmpmasks tmp;
	bool invalidate = false;
	int old_prs = cs->partition_root_state;

	 
	if (cs == &top_cpuset)
		return -EACCES;

	 
	if (!*buf) {
		cpumask_clear(trialcs->cpus_allowed);
	} else {
		retval = cpulist_parse(buf, trialcs->cpus_allowed);
		if (retval < 0)
			return retval;

		if (!cpumask_subset(trialcs->cpus_allowed,
				    top_cpuset.cpus_allowed))
			return -EINVAL;
	}

	 
	if (cpumask_equal(cs->cpus_allowed, trialcs->cpus_allowed))
		return 0;

	if (alloc_cpumasks(NULL, &tmp))
		return -ENOMEM;

	retval = validate_change(cs, trialcs);

	if ((retval == -EINVAL) && cgroup_subsys_on_dfl(cpuset_cgrp_subsys)) {
		struct cpuset *cp, *parent;
		struct cgroup_subsys_state *css;

		 
		invalidate = true;
		rcu_read_lock();
		parent = parent_cs(cs);
		cpuset_for_each_child(cp, css, parent)
			if (is_partition_valid(cp) &&
			    cpumask_intersects(trialcs->cpus_allowed, cp->cpus_allowed)) {
				rcu_read_unlock();
				update_parent_subparts_cpumask(cp, partcmd_invalidate, NULL, &tmp);
				rcu_read_lock();
			}
		rcu_read_unlock();
		retval = 0;
	}
	if (retval < 0)
		goto out_free;

	if (cs->partition_root_state) {
		if (invalidate)
			update_parent_subparts_cpumask(cs, partcmd_invalidate,
						       NULL, &tmp);
		else
			update_parent_subparts_cpumask(cs, partcmd_update,
						trialcs->cpus_allowed, &tmp);
	}

	compute_effective_cpumask(trialcs->effective_cpus, trialcs,
				  parent_cs(cs));
	spin_lock_irq(&callback_lock);
	cpumask_copy(cs->cpus_allowed, trialcs->cpus_allowed);

	 
	if (cs->nr_subparts_cpus) {
		if (!is_partition_valid(cs) ||
		   (cpumask_subset(trialcs->effective_cpus, cs->subparts_cpus) &&
		    partition_is_populated(cs, NULL))) {
			cs->nr_subparts_cpus = 0;
			cpumask_clear(cs->subparts_cpus);
		} else {
			cpumask_and(cs->subparts_cpus, cs->subparts_cpus,
				    cs->cpus_allowed);
			cs->nr_subparts_cpus = cpumask_weight(cs->subparts_cpus);
		}
	}
	spin_unlock_irq(&callback_lock);

	 
	update_cpumasks_hier(cs, &tmp, 0);

	if (cs->partition_root_state) {
		struct cpuset *parent = parent_cs(cs);

		 
		if (parent->child_ecpus_count)
			update_sibling_cpumasks(parent, cs, &tmp);

		 
		update_partition_sd_lb(cs, old_prs);
	}
out_free:
	free_cpumasks(NULL, &tmp);
	return 0;
}

 

struct cpuset_migrate_mm_work {
	struct work_struct	work;
	struct mm_struct	*mm;
	nodemask_t		from;
	nodemask_t		to;
};

static void cpuset_migrate_mm_workfn(struct work_struct *work)
{
	struct cpuset_migrate_mm_work *mwork =
		container_of(work, struct cpuset_migrate_mm_work, work);

	 
	do_migrate_pages(mwork->mm, &mwork->from, &mwork->to, MPOL_MF_MOVE_ALL);
	mmput(mwork->mm);
	kfree(mwork);
}

static void cpuset_migrate_mm(struct mm_struct *mm, const nodemask_t *from,
							const nodemask_t *to)
{
	struct cpuset_migrate_mm_work *mwork;

	if (nodes_equal(*from, *to)) {
		mmput(mm);
		return;
	}

	mwork = kzalloc(sizeof(*mwork), GFP_KERNEL);
	if (mwork) {
		mwork->mm = mm;
		mwork->from = *from;
		mwork->to = *to;
		INIT_WORK(&mwork->work, cpuset_migrate_mm_workfn);
		queue_work(cpuset_migrate_mm_wq, &mwork->work);
	} else {
		mmput(mm);
	}
}

static void cpuset_post_attach(void)
{
	flush_workqueue(cpuset_migrate_mm_wq);
}

 
static void cpuset_change_task_nodemask(struct task_struct *tsk,
					nodemask_t *newmems)
{
	task_lock(tsk);

	local_irq_disable();
	write_seqcount_begin(&tsk->mems_allowed_seq);

	nodes_or(tsk->mems_allowed, tsk->mems_allowed, *newmems);
	mpol_rebind_task(tsk, newmems);
	tsk->mems_allowed = *newmems;

	write_seqcount_end(&tsk->mems_allowed_seq);
	local_irq_enable();

	task_unlock(tsk);
}

static void *cpuset_being_rebound;

 
static void update_tasks_nodemask(struct cpuset *cs)
{
	static nodemask_t newmems;	 
	struct css_task_iter it;
	struct task_struct *task;

	cpuset_being_rebound = cs;		 

	guarantee_online_mems(cs, &newmems);

	 
	css_task_iter_start(&cs->css, 0, &it);
	while ((task = css_task_iter_next(&it))) {
		struct mm_struct *mm;
		bool migrate;

		cpuset_change_task_nodemask(task, &newmems);

		mm = get_task_mm(task);
		if (!mm)
			continue;

		migrate = is_memory_migrate(cs);

		mpol_rebind_mm(mm, &cs->mems_allowed);
		if (migrate)
			cpuset_migrate_mm(mm, &cs->old_mems_allowed, &newmems);
		else
			mmput(mm);
	}
	css_task_iter_end(&it);

	 
	cs->old_mems_allowed = newmems;

	 
	cpuset_being_rebound = NULL;
}

 
static void update_nodemasks_hier(struct cpuset *cs, nodemask_t *new_mems)
{
	struct cpuset *cp;
	struct cgroup_subsys_state *pos_css;

	rcu_read_lock();
	cpuset_for_each_descendant_pre(cp, pos_css, cs) {
		struct cpuset *parent = parent_cs(cp);

		nodes_and(*new_mems, cp->mems_allowed, parent->effective_mems);

		 
		if (is_in_v2_mode() && nodes_empty(*new_mems))
			*new_mems = parent->effective_mems;

		 
		if (nodes_equal(*new_mems, cp->effective_mems)) {
			pos_css = css_rightmost_descendant(pos_css);
			continue;
		}

		if (!css_tryget_online(&cp->css))
			continue;
		rcu_read_unlock();

		spin_lock_irq(&callback_lock);
		cp->effective_mems = *new_mems;
		spin_unlock_irq(&callback_lock);

		WARN_ON(!is_in_v2_mode() &&
			!nodes_equal(cp->mems_allowed, cp->effective_mems));

		update_tasks_nodemask(cp);

		rcu_read_lock();
		css_put(&cp->css);
	}
	rcu_read_unlock();
}

 
static int update_nodemask(struct cpuset *cs, struct cpuset *trialcs,
			   const char *buf)
{
	int retval;

	 
	if (cs == &top_cpuset) {
		retval = -EACCES;
		goto done;
	}

	 
	if (!*buf) {
		nodes_clear(trialcs->mems_allowed);
	} else {
		retval = nodelist_parse(buf, trialcs->mems_allowed);
		if (retval < 0)
			goto done;

		if (!nodes_subset(trialcs->mems_allowed,
				  top_cpuset.mems_allowed)) {
			retval = -EINVAL;
			goto done;
		}
	}

	if (nodes_equal(cs->mems_allowed, trialcs->mems_allowed)) {
		retval = 0;		 
		goto done;
	}
	retval = validate_change(cs, trialcs);
	if (retval < 0)
		goto done;

	check_insane_mems_config(&trialcs->mems_allowed);

	spin_lock_irq(&callback_lock);
	cs->mems_allowed = trialcs->mems_allowed;
	spin_unlock_irq(&callback_lock);

	 
	update_nodemasks_hier(cs, &trialcs->mems_allowed);
done:
	return retval;
}

bool current_cpuset_is_being_rebound(void)
{
	bool ret;

	rcu_read_lock();
	ret = task_cs(current) == cpuset_being_rebound;
	rcu_read_unlock();

	return ret;
}

static int update_relax_domain_level(struct cpuset *cs, s64 val)
{
#ifdef CONFIG_SMP
	if (val < -1 || val >= sched_domain_level_max)
		return -EINVAL;
#endif

	if (val != cs->relax_domain_level) {
		cs->relax_domain_level = val;
		if (!cpumask_empty(cs->cpus_allowed) &&
		    is_sched_load_balance(cs))
			rebuild_sched_domains_locked();
	}

	return 0;
}

 
static void update_tasks_flags(struct cpuset *cs)
{
	struct css_task_iter it;
	struct task_struct *task;

	css_task_iter_start(&cs->css, 0, &it);
	while ((task = css_task_iter_next(&it)))
		cpuset_update_task_spread_flags(cs, task);
	css_task_iter_end(&it);
}

 

static int update_flag(cpuset_flagbits_t bit, struct cpuset *cs,
		       int turning_on)
{
	struct cpuset *trialcs;
	int balance_flag_changed;
	int spread_flag_changed;
	int err;

	trialcs = alloc_trial_cpuset(cs);
	if (!trialcs)
		return -ENOMEM;

	if (turning_on)
		set_bit(bit, &trialcs->flags);
	else
		clear_bit(bit, &trialcs->flags);

	err = validate_change(cs, trialcs);
	if (err < 0)
		goto out;

	balance_flag_changed = (is_sched_load_balance(cs) !=
				is_sched_load_balance(trialcs));

	spread_flag_changed = ((is_spread_slab(cs) != is_spread_slab(trialcs))
			|| (is_spread_page(cs) != is_spread_page(trialcs)));

	spin_lock_irq(&callback_lock);
	cs->flags = trialcs->flags;
	spin_unlock_irq(&callback_lock);

	if (!cpumask_empty(trialcs->cpus_allowed) && balance_flag_changed)
		rebuild_sched_domains_locked();

	if (spread_flag_changed)
		update_tasks_flags(cs);
out:
	free_cpuset(trialcs);
	return err;
}

 
static int update_prstate(struct cpuset *cs, int new_prs)
{
	int err = PERR_NONE, old_prs = cs->partition_root_state;
	struct cpuset *parent = parent_cs(cs);
	struct tmpmasks tmpmask;

	if (old_prs == new_prs)
		return 0;

	 
	if (new_prs && is_prs_invalid(old_prs)) {
		cs->partition_root_state = -new_prs;
		return 0;
	}

	if (alloc_cpumasks(NULL, &tmpmask))
		return -ENOMEM;

	err = update_partition_exclusive(cs, new_prs);
	if (err)
		goto out;

	if (!old_prs) {
		 
		if (cpumask_empty(cs->cpus_allowed)) {
			err = PERR_CPUSEMPTY;
			goto out;
		}

		err = update_parent_subparts_cpumask(cs, partcmd_enable,
						     NULL, &tmpmask);
	} else if (old_prs && new_prs) {
		 
		;
	} else {
		 
		update_parent_subparts_cpumask(cs, partcmd_disable, NULL,
					       &tmpmask);

		 
		if (unlikely(cs->nr_subparts_cpus)) {
			spin_lock_irq(&callback_lock);
			cs->nr_subparts_cpus = 0;
			cpumask_clear(cs->subparts_cpus);
			compute_effective_cpumask(cs->effective_cpus, cs, parent);
			spin_unlock_irq(&callback_lock);
		}
	}
out:
	 
	if (err) {
		new_prs = -new_prs;
		update_partition_exclusive(cs, new_prs);
	}

	spin_lock_irq(&callback_lock);
	cs->partition_root_state = new_prs;
	WRITE_ONCE(cs->prs_err, err);
	spin_unlock_irq(&callback_lock);

	 
	if (!list_empty(&cs->css.children))
		update_cpumasks_hier(cs, &tmpmask, !new_prs ? HIER_CHECKALL : 0);

	 
	update_partition_sd_lb(cs, old_prs);

	notify_partition_change(cs, old_prs);
	free_cpumasks(NULL, &tmpmask);
	return 0;
}

 

#define FM_COEF 933		 
#define FM_MAXTICKS ((u32)99)    
#define FM_MAXCNT 1000000	 
#define FM_SCALE 1000		 

 
static void fmeter_init(struct fmeter *fmp)
{
	fmp->cnt = 0;
	fmp->val = 0;
	fmp->time = 0;
	spin_lock_init(&fmp->lock);
}

 
static void fmeter_update(struct fmeter *fmp)
{
	time64_t now;
	u32 ticks;

	now = ktime_get_seconds();
	ticks = now - fmp->time;

	if (ticks == 0)
		return;

	ticks = min(FM_MAXTICKS, ticks);
	while (ticks-- > 0)
		fmp->val = (FM_COEF * fmp->val) / FM_SCALE;
	fmp->time = now;

	fmp->val += ((FM_SCALE - FM_COEF) * fmp->cnt) / FM_SCALE;
	fmp->cnt = 0;
}

 
static void fmeter_markevent(struct fmeter *fmp)
{
	spin_lock(&fmp->lock);
	fmeter_update(fmp);
	fmp->cnt = min(FM_MAXCNT, fmp->cnt + FM_SCALE);
	spin_unlock(&fmp->lock);
}

 
static int fmeter_getrate(struct fmeter *fmp)
{
	int val;

	spin_lock(&fmp->lock);
	fmeter_update(fmp);
	val = fmp->val;
	spin_unlock(&fmp->lock);
	return val;
}

static struct cpuset *cpuset_attach_old_cs;

 
static int cpuset_can_attach_check(struct cpuset *cs)
{
	if (cpumask_empty(cs->effective_cpus) ||
	   (!is_in_v2_mode() && nodes_empty(cs->mems_allowed)))
		return -ENOSPC;
	return 0;
}

static void reset_migrate_dl_data(struct cpuset *cs)
{
	cs->nr_migrate_dl_tasks = 0;
	cs->sum_migrate_dl_bw = 0;
}

 
static int cpuset_can_attach(struct cgroup_taskset *tset)
{
	struct cgroup_subsys_state *css;
	struct cpuset *cs, *oldcs;
	struct task_struct *task;
	bool cpus_updated, mems_updated;
	int ret;

	 
	cpuset_attach_old_cs = task_cs(cgroup_taskset_first(tset, &css));
	oldcs = cpuset_attach_old_cs;
	cs = css_cs(css);

	mutex_lock(&cpuset_mutex);

	 
	ret = cpuset_can_attach_check(cs);
	if (ret)
		goto out_unlock;

	cpus_updated = !cpumask_equal(cs->effective_cpus, oldcs->effective_cpus);
	mems_updated = !nodes_equal(cs->effective_mems, oldcs->effective_mems);

	cgroup_taskset_for_each(task, css, tset) {
		ret = task_can_attach(task);
		if (ret)
			goto out_unlock;

		 
		if (!cgroup_subsys_on_dfl(cpuset_cgrp_subsys) ||
		    (cpus_updated || mems_updated)) {
			ret = security_task_setscheduler(task);
			if (ret)
				goto out_unlock;
		}

		if (dl_task(task)) {
			cs->nr_migrate_dl_tasks++;
			cs->sum_migrate_dl_bw += task->dl.dl_bw;
		}
	}

	if (!cs->nr_migrate_dl_tasks)
		goto out_success;

	if (!cpumask_intersects(oldcs->effective_cpus, cs->effective_cpus)) {
		int cpu = cpumask_any_and(cpu_active_mask, cs->effective_cpus);

		if (unlikely(cpu >= nr_cpu_ids)) {
			reset_migrate_dl_data(cs);
			ret = -EINVAL;
			goto out_unlock;
		}

		ret = dl_bw_alloc(cpu, cs->sum_migrate_dl_bw);
		if (ret) {
			reset_migrate_dl_data(cs);
			goto out_unlock;
		}
	}

out_success:
	 
	cs->attach_in_progress++;
out_unlock:
	mutex_unlock(&cpuset_mutex);
	return ret;
}

static void cpuset_cancel_attach(struct cgroup_taskset *tset)
{
	struct cgroup_subsys_state *css;
	struct cpuset *cs;

	cgroup_taskset_first(tset, &css);
	cs = css_cs(css);

	mutex_lock(&cpuset_mutex);
	cs->attach_in_progress--;
	if (!cs->attach_in_progress)
		wake_up(&cpuset_attach_wq);

	if (cs->nr_migrate_dl_tasks) {
		int cpu = cpumask_any(cs->effective_cpus);

		dl_bw_free(cpu, cs->sum_migrate_dl_bw);
		reset_migrate_dl_data(cs);
	}

	mutex_unlock(&cpuset_mutex);
}

 
static cpumask_var_t cpus_attach;
static nodemask_t cpuset_attach_nodemask_to;

static void cpuset_attach_task(struct cpuset *cs, struct task_struct *task)
{
	lockdep_assert_held(&cpuset_mutex);

	if (cs != &top_cpuset)
		guarantee_online_cpus(task, cpus_attach);
	else
		cpumask_andnot(cpus_attach, task_cpu_possible_mask(task),
			       cs->subparts_cpus);
	 
	WARN_ON_ONCE(set_cpus_allowed_ptr(task, cpus_attach));

	cpuset_change_task_nodemask(task, &cpuset_attach_nodemask_to);
	cpuset_update_task_spread_flags(cs, task);
}

static void cpuset_attach(struct cgroup_taskset *tset)
{
	struct task_struct *task;
	struct task_struct *leader;
	struct cgroup_subsys_state *css;
	struct cpuset *cs;
	struct cpuset *oldcs = cpuset_attach_old_cs;
	bool cpus_updated, mems_updated;

	cgroup_taskset_first(tset, &css);
	cs = css_cs(css);

	lockdep_assert_cpus_held();	 
	mutex_lock(&cpuset_mutex);
	cpus_updated = !cpumask_equal(cs->effective_cpus,
				      oldcs->effective_cpus);
	mems_updated = !nodes_equal(cs->effective_mems, oldcs->effective_mems);

	 
	if (cgroup_subsys_on_dfl(cpuset_cgrp_subsys) &&
	    !cpus_updated && !mems_updated) {
		cpuset_attach_nodemask_to = cs->effective_mems;
		goto out;
	}

	guarantee_online_mems(cs, &cpuset_attach_nodemask_to);

	cgroup_taskset_for_each(task, css, tset)
		cpuset_attach_task(cs, task);

	 
	cpuset_attach_nodemask_to = cs->effective_mems;
	if (!is_memory_migrate(cs) && !mems_updated)
		goto out;

	cgroup_taskset_for_each_leader(leader, css, tset) {
		struct mm_struct *mm = get_task_mm(leader);

		if (mm) {
			mpol_rebind_mm(mm, &cpuset_attach_nodemask_to);

			 
			if (is_memory_migrate(cs))
				cpuset_migrate_mm(mm, &oldcs->old_mems_allowed,
						  &cpuset_attach_nodemask_to);
			else
				mmput(mm);
		}
	}

out:
	cs->old_mems_allowed = cpuset_attach_nodemask_to;

	if (cs->nr_migrate_dl_tasks) {
		cs->nr_deadline_tasks += cs->nr_migrate_dl_tasks;
		oldcs->nr_deadline_tasks -= cs->nr_migrate_dl_tasks;
		reset_migrate_dl_data(cs);
	}

	cs->attach_in_progress--;
	if (!cs->attach_in_progress)
		wake_up(&cpuset_attach_wq);

	mutex_unlock(&cpuset_mutex);
}

 

typedef enum {
	FILE_MEMORY_MIGRATE,
	FILE_CPULIST,
	FILE_MEMLIST,
	FILE_EFFECTIVE_CPULIST,
	FILE_EFFECTIVE_MEMLIST,
	FILE_SUBPARTS_CPULIST,
	FILE_CPU_EXCLUSIVE,
	FILE_MEM_EXCLUSIVE,
	FILE_MEM_HARDWALL,
	FILE_SCHED_LOAD_BALANCE,
	FILE_PARTITION_ROOT,
	FILE_SCHED_RELAX_DOMAIN_LEVEL,
	FILE_MEMORY_PRESSURE_ENABLED,
	FILE_MEMORY_PRESSURE,
	FILE_SPREAD_PAGE,
	FILE_SPREAD_SLAB,
} cpuset_filetype_t;

static int cpuset_write_u64(struct cgroup_subsys_state *css, struct cftype *cft,
			    u64 val)
{
	struct cpuset *cs = css_cs(css);
	cpuset_filetype_t type = cft->private;
	int retval = 0;

	cpus_read_lock();
	mutex_lock(&cpuset_mutex);
	if (!is_cpuset_online(cs)) {
		retval = -ENODEV;
		goto out_unlock;
	}

	switch (type) {
	case FILE_CPU_EXCLUSIVE:
		retval = update_flag(CS_CPU_EXCLUSIVE, cs, val);
		break;
	case FILE_MEM_EXCLUSIVE:
		retval = update_flag(CS_MEM_EXCLUSIVE, cs, val);
		break;
	case FILE_MEM_HARDWALL:
		retval = update_flag(CS_MEM_HARDWALL, cs, val);
		break;
	case FILE_SCHED_LOAD_BALANCE:
		retval = update_flag(CS_SCHED_LOAD_BALANCE, cs, val);
		break;
	case FILE_MEMORY_MIGRATE:
		retval = update_flag(CS_MEMORY_MIGRATE, cs, val);
		break;
	case FILE_MEMORY_PRESSURE_ENABLED:
		cpuset_memory_pressure_enabled = !!val;
		break;
	case FILE_SPREAD_PAGE:
		retval = update_flag(CS_SPREAD_PAGE, cs, val);
		break;
	case FILE_SPREAD_SLAB:
		retval = update_flag(CS_SPREAD_SLAB, cs, val);
		break;
	default:
		retval = -EINVAL;
		break;
	}
out_unlock:
	mutex_unlock(&cpuset_mutex);
	cpus_read_unlock();
	return retval;
}

static int cpuset_write_s64(struct cgroup_subsys_state *css, struct cftype *cft,
			    s64 val)
{
	struct cpuset *cs = css_cs(css);
	cpuset_filetype_t type = cft->private;
	int retval = -ENODEV;

	cpus_read_lock();
	mutex_lock(&cpuset_mutex);
	if (!is_cpuset_online(cs))
		goto out_unlock;

	switch (type) {
	case FILE_SCHED_RELAX_DOMAIN_LEVEL:
		retval = update_relax_domain_level(cs, val);
		break;
	default:
		retval = -EINVAL;
		break;
	}
out_unlock:
	mutex_unlock(&cpuset_mutex);
	cpus_read_unlock();
	return retval;
}

 
static ssize_t cpuset_write_resmask(struct kernfs_open_file *of,
				    char *buf, size_t nbytes, loff_t off)
{
	struct cpuset *cs = css_cs(of_css(of));
	struct cpuset *trialcs;
	int retval = -ENODEV;

	buf = strstrip(buf);

	 
	css_get(&cs->css);
	kernfs_break_active_protection(of->kn);
	flush_work(&cpuset_hotplug_work);

	cpus_read_lock();
	mutex_lock(&cpuset_mutex);
	if (!is_cpuset_online(cs))
		goto out_unlock;

	trialcs = alloc_trial_cpuset(cs);
	if (!trialcs) {
		retval = -ENOMEM;
		goto out_unlock;
	}

	switch (of_cft(of)->private) {
	case FILE_CPULIST:
		retval = update_cpumask(cs, trialcs, buf);
		break;
	case FILE_MEMLIST:
		retval = update_nodemask(cs, trialcs, buf);
		break;
	default:
		retval = -EINVAL;
		break;
	}

	free_cpuset(trialcs);
out_unlock:
	mutex_unlock(&cpuset_mutex);
	cpus_read_unlock();
	kernfs_unbreak_active_protection(of->kn);
	css_put(&cs->css);
	flush_workqueue(cpuset_migrate_mm_wq);
	return retval ?: nbytes;
}

 
static int cpuset_common_seq_show(struct seq_file *sf, void *v)
{
	struct cpuset *cs = css_cs(seq_css(sf));
	cpuset_filetype_t type = seq_cft(sf)->private;
	int ret = 0;

	spin_lock_irq(&callback_lock);

	switch (type) {
	case FILE_CPULIST:
		seq_printf(sf, "%*pbl\n", cpumask_pr_args(cs->cpus_allowed));
		break;
	case FILE_MEMLIST:
		seq_printf(sf, "%*pbl\n", nodemask_pr_args(&cs->mems_allowed));
		break;
	case FILE_EFFECTIVE_CPULIST:
		seq_printf(sf, "%*pbl\n", cpumask_pr_args(cs->effective_cpus));
		break;
	case FILE_EFFECTIVE_MEMLIST:
		seq_printf(sf, "%*pbl\n", nodemask_pr_args(&cs->effective_mems));
		break;
	case FILE_SUBPARTS_CPULIST:
		seq_printf(sf, "%*pbl\n", cpumask_pr_args(cs->subparts_cpus));
		break;
	default:
		ret = -EINVAL;
	}

	spin_unlock_irq(&callback_lock);
	return ret;
}

static u64 cpuset_read_u64(struct cgroup_subsys_state *css, struct cftype *cft)
{
	struct cpuset *cs = css_cs(css);
	cpuset_filetype_t type = cft->private;
	switch (type) {
	case FILE_CPU_EXCLUSIVE:
		return is_cpu_exclusive(cs);
	case FILE_MEM_EXCLUSIVE:
		return is_mem_exclusive(cs);
	case FILE_MEM_HARDWALL:
		return is_mem_hardwall(cs);
	case FILE_SCHED_LOAD_BALANCE:
		return is_sched_load_balance(cs);
	case FILE_MEMORY_MIGRATE:
		return is_memory_migrate(cs);
	case FILE_MEMORY_PRESSURE_ENABLED:
		return cpuset_memory_pressure_enabled;
	case FILE_MEMORY_PRESSURE:
		return fmeter_getrate(&cs->fmeter);
	case FILE_SPREAD_PAGE:
		return is_spread_page(cs);
	case FILE_SPREAD_SLAB:
		return is_spread_slab(cs);
	default:
		BUG();
	}

	 
	return 0;
}

static s64 cpuset_read_s64(struct cgroup_subsys_state *css, struct cftype *cft)
{
	struct cpuset *cs = css_cs(css);
	cpuset_filetype_t type = cft->private;
	switch (type) {
	case FILE_SCHED_RELAX_DOMAIN_LEVEL:
		return cs->relax_domain_level;
	default:
		BUG();
	}

	 
	return 0;
}

static int sched_partition_show(struct seq_file *seq, void *v)
{
	struct cpuset *cs = css_cs(seq_css(seq));
	const char *err, *type = NULL;

	switch (cs->partition_root_state) {
	case PRS_ROOT:
		seq_puts(seq, "root\n");
		break;
	case PRS_ISOLATED:
		seq_puts(seq, "isolated\n");
		break;
	case PRS_MEMBER:
		seq_puts(seq, "member\n");
		break;
	case PRS_INVALID_ROOT:
		type = "root";
		fallthrough;
	case PRS_INVALID_ISOLATED:
		if (!type)
			type = "isolated";
		err = perr_strings[READ_ONCE(cs->prs_err)];
		if (err)
			seq_printf(seq, "%s invalid (%s)\n", type, err);
		else
			seq_printf(seq, "%s invalid\n", type);
		break;
	}
	return 0;
}

static ssize_t sched_partition_write(struct kernfs_open_file *of, char *buf,
				     size_t nbytes, loff_t off)
{
	struct cpuset *cs = css_cs(of_css(of));
	int val;
	int retval = -ENODEV;

	buf = strstrip(buf);

	 
	if (!strcmp(buf, "root"))
		val = PRS_ROOT;
	else if (!strcmp(buf, "member"))
		val = PRS_MEMBER;
	else if (!strcmp(buf, "isolated"))
		val = PRS_ISOLATED;
	else
		return -EINVAL;

	css_get(&cs->css);
	cpus_read_lock();
	mutex_lock(&cpuset_mutex);
	if (!is_cpuset_online(cs))
		goto out_unlock;

	retval = update_prstate(cs, val);
out_unlock:
	mutex_unlock(&cpuset_mutex);
	cpus_read_unlock();
	css_put(&cs->css);
	return retval ?: nbytes;
}

 

static struct cftype legacy_files[] = {
	{
		.name = "cpus",
		.seq_show = cpuset_common_seq_show,
		.write = cpuset_write_resmask,
		.max_write_len = (100U + 6 * NR_CPUS),
		.private = FILE_CPULIST,
	},

	{
		.name = "mems",
		.seq_show = cpuset_common_seq_show,
		.write = cpuset_write_resmask,
		.max_write_len = (100U + 6 * MAX_NUMNODES),
		.private = FILE_MEMLIST,
	},

	{
		.name = "effective_cpus",
		.seq_show = cpuset_common_seq_show,
		.private = FILE_EFFECTIVE_CPULIST,
	},

	{
		.name = "effective_mems",
		.seq_show = cpuset_common_seq_show,
		.private = FILE_EFFECTIVE_MEMLIST,
	},

	{
		.name = "cpu_exclusive",
		.read_u64 = cpuset_read_u64,
		.write_u64 = cpuset_write_u64,
		.private = FILE_CPU_EXCLUSIVE,
	},

	{
		.name = "mem_exclusive",
		.read_u64 = cpuset_read_u64,
		.write_u64 = cpuset_write_u64,
		.private = FILE_MEM_EXCLUSIVE,
	},

	{
		.name = "mem_hardwall",
		.read_u64 = cpuset_read_u64,
		.write_u64 = cpuset_write_u64,
		.private = FILE_MEM_HARDWALL,
	},

	{
		.name = "sched_load_balance",
		.read_u64 = cpuset_read_u64,
		.write_u64 = cpuset_write_u64,
		.private = FILE_SCHED_LOAD_BALANCE,
	},

	{
		.name = "sched_relax_domain_level",
		.read_s64 = cpuset_read_s64,
		.write_s64 = cpuset_write_s64,
		.private = FILE_SCHED_RELAX_DOMAIN_LEVEL,
	},

	{
		.name = "memory_migrate",
		.read_u64 = cpuset_read_u64,
		.write_u64 = cpuset_write_u64,
		.private = FILE_MEMORY_MIGRATE,
	},

	{
		.name = "memory_pressure",
		.read_u64 = cpuset_read_u64,
		.private = FILE_MEMORY_PRESSURE,
	},

	{
		.name = "memory_spread_page",
		.read_u64 = cpuset_read_u64,
		.write_u64 = cpuset_write_u64,
		.private = FILE_SPREAD_PAGE,
	},

	{
		.name = "memory_spread_slab",
		.read_u64 = cpuset_read_u64,
		.write_u64 = cpuset_write_u64,
		.private = FILE_SPREAD_SLAB,
	},

	{
		.name = "memory_pressure_enabled",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.read_u64 = cpuset_read_u64,
		.write_u64 = cpuset_write_u64,
		.private = FILE_MEMORY_PRESSURE_ENABLED,
	},

	{ }	 
};

 
static struct cftype dfl_files[] = {
	{
		.name = "cpus",
		.seq_show = cpuset_common_seq_show,
		.write = cpuset_write_resmask,
		.max_write_len = (100U + 6 * NR_CPUS),
		.private = FILE_CPULIST,
		.flags = CFTYPE_NOT_ON_ROOT,
	},

	{
		.name = "mems",
		.seq_show = cpuset_common_seq_show,
		.write = cpuset_write_resmask,
		.max_write_len = (100U + 6 * MAX_NUMNODES),
		.private = FILE_MEMLIST,
		.flags = CFTYPE_NOT_ON_ROOT,
	},

	{
		.name = "cpus.effective",
		.seq_show = cpuset_common_seq_show,
		.private = FILE_EFFECTIVE_CPULIST,
	},

	{
		.name = "mems.effective",
		.seq_show = cpuset_common_seq_show,
		.private = FILE_EFFECTIVE_MEMLIST,
	},

	{
		.name = "cpus.partition",
		.seq_show = sched_partition_show,
		.write = sched_partition_write,
		.private = FILE_PARTITION_ROOT,
		.flags = CFTYPE_NOT_ON_ROOT,
		.file_offset = offsetof(struct cpuset, partition_file),
	},

	{
		.name = "cpus.subpartitions",
		.seq_show = cpuset_common_seq_show,
		.private = FILE_SUBPARTS_CPULIST,
		.flags = CFTYPE_DEBUG,
	},

	{ }	 
};


 
static struct cgroup_subsys_state *
cpuset_css_alloc(struct cgroup_subsys_state *parent_css)
{
	struct cpuset *cs;

	if (!parent_css)
		return &top_cpuset.css;

	cs = kzalloc(sizeof(*cs), GFP_KERNEL);
	if (!cs)
		return ERR_PTR(-ENOMEM);

	if (alloc_cpumasks(cs, NULL)) {
		kfree(cs);
		return ERR_PTR(-ENOMEM);
	}

	__set_bit(CS_SCHED_LOAD_BALANCE, &cs->flags);
	nodes_clear(cs->mems_allowed);
	nodes_clear(cs->effective_mems);
	fmeter_init(&cs->fmeter);
	cs->relax_domain_level = -1;

	 
	if (cgroup_subsys_on_dfl(cpuset_cgrp_subsys))
		__set_bit(CS_MEMORY_MIGRATE, &cs->flags);

	return &cs->css;
}

static int cpuset_css_online(struct cgroup_subsys_state *css)
{
	struct cpuset *cs = css_cs(css);
	struct cpuset *parent = parent_cs(cs);
	struct cpuset *tmp_cs;
	struct cgroup_subsys_state *pos_css;

	if (!parent)
		return 0;

	cpus_read_lock();
	mutex_lock(&cpuset_mutex);

	set_bit(CS_ONLINE, &cs->flags);
	if (is_spread_page(parent))
		set_bit(CS_SPREAD_PAGE, &cs->flags);
	if (is_spread_slab(parent))
		set_bit(CS_SPREAD_SLAB, &cs->flags);

	cpuset_inc();

	spin_lock_irq(&callback_lock);
	if (is_in_v2_mode()) {
		cpumask_copy(cs->effective_cpus, parent->effective_cpus);
		cs->effective_mems = parent->effective_mems;
		cs->use_parent_ecpus = true;
		parent->child_ecpus_count++;
	}

	 
	if (cgroup_subsys_on_dfl(cpuset_cgrp_subsys) &&
	    !is_sched_load_balance(parent))
		clear_bit(CS_SCHED_LOAD_BALANCE, &cs->flags);

	spin_unlock_irq(&callback_lock);

	if (!test_bit(CGRP_CPUSET_CLONE_CHILDREN, &css->cgroup->flags))
		goto out_unlock;

	 
	rcu_read_lock();
	cpuset_for_each_child(tmp_cs, pos_css, parent) {
		if (is_mem_exclusive(tmp_cs) || is_cpu_exclusive(tmp_cs)) {
			rcu_read_unlock();
			goto out_unlock;
		}
	}
	rcu_read_unlock();

	spin_lock_irq(&callback_lock);
	cs->mems_allowed = parent->mems_allowed;
	cs->effective_mems = parent->mems_allowed;
	cpumask_copy(cs->cpus_allowed, parent->cpus_allowed);
	cpumask_copy(cs->effective_cpus, parent->cpus_allowed);
	spin_unlock_irq(&callback_lock);
out_unlock:
	mutex_unlock(&cpuset_mutex);
	cpus_read_unlock();
	return 0;
}

 

static void cpuset_css_offline(struct cgroup_subsys_state *css)
{
	struct cpuset *cs = css_cs(css);

	cpus_read_lock();
	mutex_lock(&cpuset_mutex);

	if (is_partition_valid(cs))
		update_prstate(cs, 0);

	if (!cgroup_subsys_on_dfl(cpuset_cgrp_subsys) &&
	    is_sched_load_balance(cs))
		update_flag(CS_SCHED_LOAD_BALANCE, cs, 0);

	if (cs->use_parent_ecpus) {
		struct cpuset *parent = parent_cs(cs);

		cs->use_parent_ecpus = false;
		parent->child_ecpus_count--;
	}

	cpuset_dec();
	clear_bit(CS_ONLINE, &cs->flags);

	mutex_unlock(&cpuset_mutex);
	cpus_read_unlock();
}

static void cpuset_css_free(struct cgroup_subsys_state *css)
{
	struct cpuset *cs = css_cs(css);

	free_cpuset(cs);
}

static void cpuset_bind(struct cgroup_subsys_state *root_css)
{
	mutex_lock(&cpuset_mutex);
	spin_lock_irq(&callback_lock);

	if (is_in_v2_mode()) {
		cpumask_copy(top_cpuset.cpus_allowed, cpu_possible_mask);
		top_cpuset.mems_allowed = node_possible_map;
	} else {
		cpumask_copy(top_cpuset.cpus_allowed,
			     top_cpuset.effective_cpus);
		top_cpuset.mems_allowed = top_cpuset.effective_mems;
	}

	spin_unlock_irq(&callback_lock);
	mutex_unlock(&cpuset_mutex);
}

 
static int cpuset_can_fork(struct task_struct *task, struct css_set *cset)
{
	struct cpuset *cs = css_cs(cset->subsys[cpuset_cgrp_id]);
	bool same_cs;
	int ret;

	rcu_read_lock();
	same_cs = (cs == task_cs(current));
	rcu_read_unlock();

	if (same_cs)
		return 0;

	lockdep_assert_held(&cgroup_mutex);
	mutex_lock(&cpuset_mutex);

	 
	ret = cpuset_can_attach_check(cs);
	if (ret)
		goto out_unlock;

	ret = task_can_attach(task);
	if (ret)
		goto out_unlock;

	ret = security_task_setscheduler(task);
	if (ret)
		goto out_unlock;

	 
	cs->attach_in_progress++;
out_unlock:
	mutex_unlock(&cpuset_mutex);
	return ret;
}

static void cpuset_cancel_fork(struct task_struct *task, struct css_set *cset)
{
	struct cpuset *cs = css_cs(cset->subsys[cpuset_cgrp_id]);
	bool same_cs;

	rcu_read_lock();
	same_cs = (cs == task_cs(current));
	rcu_read_unlock();

	if (same_cs)
		return;

	mutex_lock(&cpuset_mutex);
	cs->attach_in_progress--;
	if (!cs->attach_in_progress)
		wake_up(&cpuset_attach_wq);
	mutex_unlock(&cpuset_mutex);
}

 
static void cpuset_fork(struct task_struct *task)
{
	struct cpuset *cs;
	bool same_cs;

	rcu_read_lock();
	cs = task_cs(task);
	same_cs = (cs == task_cs(current));
	rcu_read_unlock();

	if (same_cs) {
		if (cs == &top_cpuset)
			return;

		set_cpus_allowed_ptr(task, current->cpus_ptr);
		task->mems_allowed = current->mems_allowed;
		return;
	}

	 
	mutex_lock(&cpuset_mutex);
	guarantee_online_mems(cs, &cpuset_attach_nodemask_to);
	cpuset_attach_task(cs, task);

	cs->attach_in_progress--;
	if (!cs->attach_in_progress)
		wake_up(&cpuset_attach_wq);

	mutex_unlock(&cpuset_mutex);
}

struct cgroup_subsys cpuset_cgrp_subsys = {
	.css_alloc	= cpuset_css_alloc,
	.css_online	= cpuset_css_online,
	.css_offline	= cpuset_css_offline,
	.css_free	= cpuset_css_free,
	.can_attach	= cpuset_can_attach,
	.cancel_attach	= cpuset_cancel_attach,
	.attach		= cpuset_attach,
	.post_attach	= cpuset_post_attach,
	.bind		= cpuset_bind,
	.can_fork	= cpuset_can_fork,
	.cancel_fork	= cpuset_cancel_fork,
	.fork		= cpuset_fork,
	.legacy_cftypes	= legacy_files,
	.dfl_cftypes	= dfl_files,
	.early_init	= true,
	.threaded	= true,
};

 

int __init cpuset_init(void)
{
	BUG_ON(!alloc_cpumask_var(&top_cpuset.cpus_allowed, GFP_KERNEL));
	BUG_ON(!alloc_cpumask_var(&top_cpuset.effective_cpus, GFP_KERNEL));
	BUG_ON(!zalloc_cpumask_var(&top_cpuset.subparts_cpus, GFP_KERNEL));

	cpumask_setall(top_cpuset.cpus_allowed);
	nodes_setall(top_cpuset.mems_allowed);
	cpumask_setall(top_cpuset.effective_cpus);
	nodes_setall(top_cpuset.effective_mems);

	fmeter_init(&top_cpuset.fmeter);
	set_bit(CS_SCHED_LOAD_BALANCE, &top_cpuset.flags);
	top_cpuset.relax_domain_level = -1;

	BUG_ON(!alloc_cpumask_var(&cpus_attach, GFP_KERNEL));

	return 0;
}

 
static void remove_tasks_in_empty_cpuset(struct cpuset *cs)
{
	struct cpuset *parent;

	 
	parent = parent_cs(cs);
	while (cpumask_empty(parent->cpus_allowed) ||
			nodes_empty(parent->mems_allowed))
		parent = parent_cs(parent);

	if (cgroup_transfer_tasks(parent->css.cgroup, cs->css.cgroup)) {
		pr_err("cpuset: failed to transfer tasks out of empty cpuset ");
		pr_cont_cgroup_name(cs->css.cgroup);
		pr_cont("\n");
	}
}

static void
hotplug_update_tasks_legacy(struct cpuset *cs,
			    struct cpumask *new_cpus, nodemask_t *new_mems,
			    bool cpus_updated, bool mems_updated)
{
	bool is_empty;

	spin_lock_irq(&callback_lock);
	cpumask_copy(cs->cpus_allowed, new_cpus);
	cpumask_copy(cs->effective_cpus, new_cpus);
	cs->mems_allowed = *new_mems;
	cs->effective_mems = *new_mems;
	spin_unlock_irq(&callback_lock);

	 
	if (cpus_updated && !cpumask_empty(cs->cpus_allowed))
		update_tasks_cpumask(cs, new_cpus);
	if (mems_updated && !nodes_empty(cs->mems_allowed))
		update_tasks_nodemask(cs);

	is_empty = cpumask_empty(cs->cpus_allowed) ||
		   nodes_empty(cs->mems_allowed);

	 
	if (is_empty) {
		mutex_unlock(&cpuset_mutex);
		remove_tasks_in_empty_cpuset(cs);
		mutex_lock(&cpuset_mutex);
	}
}

static void
hotplug_update_tasks(struct cpuset *cs,
		     struct cpumask *new_cpus, nodemask_t *new_mems,
		     bool cpus_updated, bool mems_updated)
{
	 
	if (cpumask_empty(new_cpus) && !is_partition_valid(cs))
		cpumask_copy(new_cpus, parent_cs(cs)->effective_cpus);
	if (nodes_empty(*new_mems))
		*new_mems = parent_cs(cs)->effective_mems;

	spin_lock_irq(&callback_lock);
	cpumask_copy(cs->effective_cpus, new_cpus);
	cs->effective_mems = *new_mems;
	spin_unlock_irq(&callback_lock);

	if (cpus_updated)
		update_tasks_cpumask(cs, new_cpus);
	if (mems_updated)
		update_tasks_nodemask(cs);
}

static bool force_rebuild;

void cpuset_force_rebuild(void)
{
	force_rebuild = true;
}

 
static void cpuset_hotplug_update_tasks(struct cpuset *cs, struct tmpmasks *tmp)
{
	static cpumask_t new_cpus;
	static nodemask_t new_mems;
	bool cpus_updated;
	bool mems_updated;
	struct cpuset *parent;
retry:
	wait_event(cpuset_attach_wq, cs->attach_in_progress == 0);

	mutex_lock(&cpuset_mutex);

	 
	if (cs->attach_in_progress) {
		mutex_unlock(&cpuset_mutex);
		goto retry;
	}

	parent = parent_cs(cs);
	compute_effective_cpumask(&new_cpus, cs, parent);
	nodes_and(new_mems, cs->mems_allowed, parent->effective_mems);

	if (cs->nr_subparts_cpus)
		 
		cpumask_andnot(&new_cpus, &new_cpus, cs->subparts_cpus);

	if (!tmp || !cs->partition_root_state)
		goto update_tasks;

	 
	if (cs->nr_subparts_cpus && is_partition_valid(cs) &&
	    cpumask_empty(&new_cpus) && partition_is_populated(cs, NULL)) {
		spin_lock_irq(&callback_lock);
		cs->nr_subparts_cpus = 0;
		cpumask_clear(cs->subparts_cpus);
		spin_unlock_irq(&callback_lock);
		compute_effective_cpumask(&new_cpus, cs, parent);
	}

	 
	if (is_partition_valid(cs) && (!parent->nr_subparts_cpus ||
	   (cpumask_empty(&new_cpus) && partition_is_populated(cs, NULL)))) {
		int old_prs, parent_prs;

		update_parent_subparts_cpumask(cs, partcmd_disable, NULL, tmp);
		if (cs->nr_subparts_cpus) {
			spin_lock_irq(&callback_lock);
			cs->nr_subparts_cpus = 0;
			cpumask_clear(cs->subparts_cpus);
			spin_unlock_irq(&callback_lock);
			compute_effective_cpumask(&new_cpus, cs, parent);
		}

		old_prs = cs->partition_root_state;
		parent_prs = parent->partition_root_state;
		if (is_partition_valid(cs)) {
			spin_lock_irq(&callback_lock);
			make_partition_invalid(cs);
			spin_unlock_irq(&callback_lock);
			if (is_prs_invalid(parent_prs))
				WRITE_ONCE(cs->prs_err, PERR_INVPARENT);
			else if (!parent_prs)
				WRITE_ONCE(cs->prs_err, PERR_NOTPART);
			else
				WRITE_ONCE(cs->prs_err, PERR_HOTPLUG);
			notify_partition_change(cs, old_prs);
		}
		cpuset_force_rebuild();
	}

	 
	else if (is_partition_valid(parent) && is_partition_invalid(cs)) {
		update_parent_subparts_cpumask(cs, partcmd_update, NULL, tmp);
		if (is_partition_valid(cs))
			cpuset_force_rebuild();
	}

update_tasks:
	cpus_updated = !cpumask_equal(&new_cpus, cs->effective_cpus);
	mems_updated = !nodes_equal(new_mems, cs->effective_mems);
	if (!cpus_updated && !mems_updated)
		goto unlock;	 

	if (mems_updated)
		check_insane_mems_config(&new_mems);

	if (is_in_v2_mode())
		hotplug_update_tasks(cs, &new_cpus, &new_mems,
				     cpus_updated, mems_updated);
	else
		hotplug_update_tasks_legacy(cs, &new_cpus, &new_mems,
					    cpus_updated, mems_updated);

unlock:
	mutex_unlock(&cpuset_mutex);
}

 
static void cpuset_hotplug_workfn(struct work_struct *work)
{
	static cpumask_t new_cpus;
	static nodemask_t new_mems;
	bool cpus_updated, mems_updated;
	bool on_dfl = is_in_v2_mode();
	struct tmpmasks tmp, *ptmp = NULL;

	if (on_dfl && !alloc_cpumasks(NULL, &tmp))
		ptmp = &tmp;

	mutex_lock(&cpuset_mutex);

	 
	cpumask_copy(&new_cpus, cpu_active_mask);
	new_mems = node_states[N_MEMORY];

	 
	cpus_updated = !cpumask_equal(top_cpuset.effective_cpus, &new_cpus);
	mems_updated = !nodes_equal(top_cpuset.effective_mems, new_mems);

	 
	if (!cpus_updated && top_cpuset.nr_subparts_cpus)
		cpus_updated = true;

	 
	if (cpus_updated) {
		spin_lock_irq(&callback_lock);
		if (!on_dfl)
			cpumask_copy(top_cpuset.cpus_allowed, &new_cpus);
		 
		if (top_cpuset.nr_subparts_cpus) {
			if (cpumask_subset(&new_cpus,
					   top_cpuset.subparts_cpus)) {
				top_cpuset.nr_subparts_cpus = 0;
				cpumask_clear(top_cpuset.subparts_cpus);
			} else {
				cpumask_andnot(&new_cpus, &new_cpus,
					       top_cpuset.subparts_cpus);
			}
		}
		cpumask_copy(top_cpuset.effective_cpus, &new_cpus);
		spin_unlock_irq(&callback_lock);
		 
	}

	 
	if (mems_updated) {
		spin_lock_irq(&callback_lock);
		if (!on_dfl)
			top_cpuset.mems_allowed = new_mems;
		top_cpuset.effective_mems = new_mems;
		spin_unlock_irq(&callback_lock);
		update_tasks_nodemask(&top_cpuset);
	}

	mutex_unlock(&cpuset_mutex);

	 
	if (cpus_updated || mems_updated) {
		struct cpuset *cs;
		struct cgroup_subsys_state *pos_css;

		rcu_read_lock();
		cpuset_for_each_descendant_pre(cs, pos_css, &top_cpuset) {
			if (cs == &top_cpuset || !css_tryget_online(&cs->css))
				continue;
			rcu_read_unlock();

			cpuset_hotplug_update_tasks(cs, ptmp);

			rcu_read_lock();
			css_put(&cs->css);
		}
		rcu_read_unlock();
	}

	 
	if (cpus_updated || force_rebuild) {
		force_rebuild = false;
		rebuild_sched_domains();
	}

	free_cpumasks(NULL, ptmp);
}

void cpuset_update_active_cpus(void)
{
	 
	schedule_work(&cpuset_hotplug_work);
}

void cpuset_wait_for_hotplug(void)
{
	flush_work(&cpuset_hotplug_work);
}

 
static int cpuset_track_online_nodes(struct notifier_block *self,
				unsigned long action, void *arg)
{
	schedule_work(&cpuset_hotplug_work);
	return NOTIFY_OK;
}

 
void __init cpuset_init_smp(void)
{
	 
	top_cpuset.old_mems_allowed = top_cpuset.mems_allowed;

	cpumask_copy(top_cpuset.effective_cpus, cpu_active_mask);
	top_cpuset.effective_mems = node_states[N_MEMORY];

	hotplug_memory_notifier(cpuset_track_online_nodes, CPUSET_CALLBACK_PRI);

	cpuset_migrate_mm_wq = alloc_ordered_workqueue("cpuset_migrate_mm", 0);
	BUG_ON(!cpuset_migrate_mm_wq);
}

 

void cpuset_cpus_allowed(struct task_struct *tsk, struct cpumask *pmask)
{
	unsigned long flags;
	struct cpuset *cs;

	spin_lock_irqsave(&callback_lock, flags);
	rcu_read_lock();

	cs = task_cs(tsk);
	if (cs != &top_cpuset)
		guarantee_online_cpus(tsk, pmask);
	 
	if ((cs == &top_cpuset) || cpumask_empty(pmask)) {
		const struct cpumask *possible_mask = task_cpu_possible_mask(tsk);

		 
		cpumask_andnot(pmask, possible_mask, top_cpuset.subparts_cpus);
		if (!cpumask_intersects(pmask, cpu_online_mask))
			cpumask_copy(pmask, possible_mask);
	}

	rcu_read_unlock();
	spin_unlock_irqrestore(&callback_lock, flags);
}

 

bool cpuset_cpus_allowed_fallback(struct task_struct *tsk)
{
	const struct cpumask *possible_mask = task_cpu_possible_mask(tsk);
	const struct cpumask *cs_mask;
	bool changed = false;

	rcu_read_lock();
	cs_mask = task_cs(tsk)->cpus_allowed;
	if (is_in_v2_mode() && cpumask_subset(cs_mask, possible_mask)) {
		do_set_cpus_allowed(tsk, cs_mask);
		changed = true;
	}
	rcu_read_unlock();

	 
	return changed;
}

void __init cpuset_init_current_mems_allowed(void)
{
	nodes_setall(current->mems_allowed);
}

 

nodemask_t cpuset_mems_allowed(struct task_struct *tsk)
{
	nodemask_t mask;
	unsigned long flags;

	spin_lock_irqsave(&callback_lock, flags);
	rcu_read_lock();
	guarantee_online_mems(task_cs(tsk), &mask);
	rcu_read_unlock();
	spin_unlock_irqrestore(&callback_lock, flags);

	return mask;
}

 
int cpuset_nodemask_valid_mems_allowed(nodemask_t *nodemask)
{
	return nodes_intersects(*nodemask, current->mems_allowed);
}

 
static struct cpuset *nearest_hardwall_ancestor(struct cpuset *cs)
{
	while (!(is_mem_exclusive(cs) || is_mem_hardwall(cs)) && parent_cs(cs))
		cs = parent_cs(cs);
	return cs;
}

 
bool cpuset_node_allowed(int node, gfp_t gfp_mask)
{
	struct cpuset *cs;		 
	bool allowed;			 
	unsigned long flags;

	if (in_interrupt())
		return true;
	if (node_isset(node, current->mems_allowed))
		return true;
	 
	if (unlikely(tsk_is_oom_victim(current)))
		return true;
	if (gfp_mask & __GFP_HARDWALL)	 
		return false;

	if (current->flags & PF_EXITING)  
		return true;

	 
	spin_lock_irqsave(&callback_lock, flags);

	rcu_read_lock();
	cs = nearest_hardwall_ancestor(task_cs(current));
	allowed = node_isset(node, cs->mems_allowed);
	rcu_read_unlock();

	spin_unlock_irqrestore(&callback_lock, flags);
	return allowed;
}

 
static int cpuset_spread_node(int *rotor)
{
	return *rotor = next_node_in(*rotor, current->mems_allowed);
}

 
int cpuset_mem_spread_node(void)
{
	if (current->cpuset_mem_spread_rotor == NUMA_NO_NODE)
		current->cpuset_mem_spread_rotor =
			node_random(&current->mems_allowed);

	return cpuset_spread_node(&current->cpuset_mem_spread_rotor);
}

 
int cpuset_slab_spread_node(void)
{
	if (current->cpuset_slab_spread_rotor == NUMA_NO_NODE)
		current->cpuset_slab_spread_rotor =
			node_random(&current->mems_allowed);

	return cpuset_spread_node(&current->cpuset_slab_spread_rotor);
}
EXPORT_SYMBOL_GPL(cpuset_mem_spread_node);

 

int cpuset_mems_allowed_intersects(const struct task_struct *tsk1,
				   const struct task_struct *tsk2)
{
	return nodes_intersects(tsk1->mems_allowed, tsk2->mems_allowed);
}

 
void cpuset_print_current_mems_allowed(void)
{
	struct cgroup *cgrp;

	rcu_read_lock();

	cgrp = task_cs(current)->css.cgroup;
	pr_cont(",cpuset=");
	pr_cont_cgroup_name(cgrp);
	pr_cont(",mems_allowed=%*pbl",
		nodemask_pr_args(&current->mems_allowed));

	rcu_read_unlock();
}

 

int cpuset_memory_pressure_enabled __read_mostly;

 

void __cpuset_memory_pressure_bump(void)
{
	rcu_read_lock();
	fmeter_markevent(&task_cs(current)->fmeter);
	rcu_read_unlock();
}

#ifdef CONFIG_PROC_PID_CPUSET
 
int proc_cpuset_show(struct seq_file *m, struct pid_namespace *ns,
		     struct pid *pid, struct task_struct *tsk)
{
	char *buf;
	struct cgroup_subsys_state *css;
	int retval;

	retval = -ENOMEM;
	buf = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!buf)
		goto out;

	css = task_get_css(tsk, cpuset_cgrp_id);
	retval = cgroup_path_ns(css->cgroup, buf, PATH_MAX,
				current->nsproxy->cgroup_ns);
	css_put(css);
	if (retval >= PATH_MAX)
		retval = -ENAMETOOLONG;
	if (retval < 0)
		goto out_free;
	seq_puts(m, buf);
	seq_putc(m, '\n');
	retval = 0;
out_free:
	kfree(buf);
out:
	return retval;
}
#endif  

 
void cpuset_task_status_allowed(struct seq_file *m, struct task_struct *task)
{
	seq_printf(m, "Mems_allowed:\t%*pb\n",
		   nodemask_pr_args(&task->mems_allowed));
	seq_printf(m, "Mems_allowed_list:\t%*pbl\n",
		   nodemask_pr_args(&task->mems_allowed));
}
