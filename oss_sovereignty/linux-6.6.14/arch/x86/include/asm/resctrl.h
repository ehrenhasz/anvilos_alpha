 
#ifndef _ASM_X86_RESCTRL_H
#define _ASM_X86_RESCTRL_H

#ifdef CONFIG_X86_CPU_RESCTRL

#include <linux/sched.h>
#include <linux/jump_label.h>

 
struct resctrl_pqr_state {
	u32			cur_rmid;
	u32			cur_closid;
	u32			default_rmid;
	u32			default_closid;
};

DECLARE_PER_CPU(struct resctrl_pqr_state, pqr_state);

DECLARE_STATIC_KEY_FALSE(rdt_enable_key);
DECLARE_STATIC_KEY_FALSE(rdt_alloc_enable_key);
DECLARE_STATIC_KEY_FALSE(rdt_mon_enable_key);

 
static inline void __resctrl_sched_in(struct task_struct *tsk)
{
	struct resctrl_pqr_state *state = this_cpu_ptr(&pqr_state);
	u32 closid = state->default_closid;
	u32 rmid = state->default_rmid;
	u32 tmp;

	 
	if (static_branch_likely(&rdt_alloc_enable_key)) {
		tmp = READ_ONCE(tsk->closid);
		if (tmp)
			closid = tmp;
	}

	if (static_branch_likely(&rdt_mon_enable_key)) {
		tmp = READ_ONCE(tsk->rmid);
		if (tmp)
			rmid = tmp;
	}

	if (closid != state->cur_closid || rmid != state->cur_rmid) {
		state->cur_closid = closid;
		state->cur_rmid = rmid;
		wrmsr(MSR_IA32_PQR_ASSOC, rmid, closid);
	}
}

static inline unsigned int resctrl_arch_round_mon_val(unsigned int val)
{
	unsigned int scale = boot_cpu_data.x86_cache_occ_scale;

	 
	val /= scale;
	return val * scale;
}

static inline void resctrl_sched_in(struct task_struct *tsk)
{
	if (static_branch_likely(&rdt_enable_key))
		__resctrl_sched_in(tsk);
}

void resctrl_cpu_detect(struct cpuinfo_x86 *c);

#else

static inline void resctrl_sched_in(struct task_struct *tsk) {}
static inline void resctrl_cpu_detect(struct cpuinfo_x86 *c) {}

#endif  

#endif  
