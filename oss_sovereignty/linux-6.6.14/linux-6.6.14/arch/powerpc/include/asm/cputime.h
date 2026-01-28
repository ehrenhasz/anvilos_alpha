#ifndef __POWERPC_CPUTIME_H
#define __POWERPC_CPUTIME_H
#ifdef CONFIG_VIRT_CPU_ACCOUNTING_NATIVE
#include <linux/types.h>
#include <linux/time.h>
#include <asm/div64.h>
#include <asm/time.h>
#include <asm/param.h>
#include <asm/firmware.h>
#ifdef __KERNEL__
#define cputime_to_nsecs(cputime) tb_to_ns(cputime)
#ifdef CONFIG_PPC64
#define get_accounting(tsk)	(&get_paca()->accounting)
#define raw_get_accounting(tsk)	(&local_paca->accounting)
static inline void arch_vtime_task_switch(struct task_struct *tsk) { }
#else
#define get_accounting(tsk)	(&task_thread_info(tsk)->accounting)
#define raw_get_accounting(tsk)	get_accounting(tsk)
static inline void arch_vtime_task_switch(struct task_struct *prev)
{
	struct cpu_accounting_data *acct = get_accounting(current);
	struct cpu_accounting_data *acct0 = get_accounting(prev);
	acct->starttime = acct0->starttime;
}
#endif
static notrace inline void account_cpu_user_entry(void)
{
	unsigned long tb = mftb();
	struct cpu_accounting_data *acct = raw_get_accounting(current);
	acct->utime += (tb - acct->starttime_user);
	acct->starttime = tb;
}
static notrace inline void account_cpu_user_exit(void)
{
	unsigned long tb = mftb();
	struct cpu_accounting_data *acct = raw_get_accounting(current);
	acct->stime += (tb - acct->starttime);
	acct->starttime_user = tb;
}
static notrace inline void account_stolen_time(void)
{
#ifdef CONFIG_PPC_SPLPAR
	if (firmware_has_feature(FW_FEATURE_SPLPAR)) {
		struct lppaca *lp = local_paca->lppaca_ptr;
		if (unlikely(local_paca->dtl_ridx != be64_to_cpu(lp->dtl_idx)))
			pseries_accumulate_stolen_time();
	}
#endif
}
#endif  
#else  
static inline void account_cpu_user_entry(void)
{
}
static inline void account_cpu_user_exit(void)
{
}
static notrace inline void account_stolen_time(void)
{
}
#endif  
#endif  
