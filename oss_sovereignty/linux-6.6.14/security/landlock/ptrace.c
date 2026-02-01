
 

#include <asm/current.h>
#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/lsm_hooks.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>

#include "common.h"
#include "cred.h"
#include "ptrace.h"
#include "ruleset.h"
#include "setup.h"

 
static bool domain_scope_le(const struct landlock_ruleset *const parent,
			    const struct landlock_ruleset *const child)
{
	const struct landlock_hierarchy *walker;

	if (!parent)
		return true;
	if (!child)
		return false;
	for (walker = child->hierarchy; walker; walker = walker->parent) {
		if (walker == parent->hierarchy)
			 
			return true;
	}
	 
	return false;
}

static bool task_is_scoped(const struct task_struct *const parent,
			   const struct task_struct *const child)
{
	bool is_scoped;
	const struct landlock_ruleset *dom_parent, *dom_child;

	rcu_read_lock();
	dom_parent = landlock_get_task_domain(parent);
	dom_child = landlock_get_task_domain(child);
	is_scoped = domain_scope_le(dom_parent, dom_child);
	rcu_read_unlock();
	return is_scoped;
}

static int task_ptrace(const struct task_struct *const parent,
		       const struct task_struct *const child)
{
	 
	if (!landlocked(parent))
		return 0;
	if (task_is_scoped(parent, child))
		return 0;
	return -EPERM;
}

 
static int hook_ptrace_access_check(struct task_struct *const child,
				    const unsigned int mode)
{
	return task_ptrace(current, child);
}

 
static int hook_ptrace_traceme(struct task_struct *const parent)
{
	return task_ptrace(parent, current);
}

static struct security_hook_list landlock_hooks[] __ro_after_init = {
	LSM_HOOK_INIT(ptrace_access_check, hook_ptrace_access_check),
	LSM_HOOK_INIT(ptrace_traceme, hook_ptrace_traceme),
};

__init void landlock_add_ptrace_hooks(void)
{
	security_add_hooks(landlock_hooks, ARRAY_SIZE(landlock_hooks),
			   LANDLOCK_NAME);
}
