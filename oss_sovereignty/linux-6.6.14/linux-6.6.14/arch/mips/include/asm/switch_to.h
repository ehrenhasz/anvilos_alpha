#ifndef _ASM_SWITCH_TO_H
#define _ASM_SWITCH_TO_H
#include <asm/cpu-features.h>
#include <asm/watch.h>
#include <asm/dsp.h>
#include <asm/cop2.h>
#include <asm/fpu.h>
struct task_struct;
extern asmlinkage struct task_struct *resume(struct task_struct *prev,
		struct task_struct *next, struct thread_info *next_ti);
extern unsigned int ll_bit;
extern struct task_struct *ll_task;
#ifdef CONFIG_MIPS_MT_FPAFF
#define __mips_mt_fpaff_switch_to(prev)					\
do {									\
	struct thread_info *__prev_ti = task_thread_info(prev);		\
									\
	if (cpu_has_fpu &&						\
	    test_ti_thread_flag(__prev_ti, TIF_FPUBOUND) &&		\
	    (!(KSTK_STATUS(prev) & ST0_CU1))) {				\
		clear_ti_thread_flag(__prev_ti, TIF_FPUBOUND);		\
		prev->cpus_mask = prev->thread.user_cpus_allowed;	\
	}								\
	next->thread.emulated_fp = 0;					\
} while(0)
#else
#define __mips_mt_fpaff_switch_to(prev) do { (void) (prev); } while (0)
#endif
#define __clear_r5_hw_ll_bit() do {					\
	if (cpu_has_mips_r5 || cpu_has_mips_r6)				\
		write_c0_lladdr(0);					\
} while (0)
#define __clear_software_ll_bit() do {					\
	if (!__builtin_constant_p(cpu_has_llsc) || !cpu_has_llsc)	\
		ll_bit = 0;						\
} while (0)
#ifdef CONFIG_MIPS_FP_SUPPORT
# define __sanitize_fcr31(next)						\
do {									\
	unsigned long fcr31 = mask_fcr31_x(next->thread.fpu.fcr31);	\
	void __user *pc;						\
									\
	if (unlikely(fcr31)) {						\
		pc = (void __user *)task_pt_regs(next)->cp0_epc;	\
		next->thread.fpu.fcr31 &= ~fcr31;			\
		force_fcr31_sig(fcr31, pc, next);			\
	}								\
} while (0)
#else
# define __sanitize_fcr31(next)
#endif
#define switch_to(prev, next, last)					\
do {									\
	__mips_mt_fpaff_switch_to(prev);				\
	lose_fpu_inatomic(1, prev);					\
	if (tsk_used_math(next))					\
		__sanitize_fcr31(next);					\
	if (cpu_has_dsp) {						\
		__save_dsp(prev);					\
		__restore_dsp(next);					\
	}								\
	if (cop2_present) {						\
		u32 status = read_c0_status();				\
									\
		set_c0_status(ST0_CU2);					\
		if ((KSTK_STATUS(prev) & ST0_CU2)) {			\
			if (cop2_lazy_restore)				\
				KSTK_STATUS(prev) &= ~ST0_CU2;		\
			cop2_save(prev);				\
		}							\
		if (KSTK_STATUS(next) & ST0_CU2 &&			\
		    !cop2_lazy_restore) {				\
			cop2_restore(next);				\
		}							\
		write_c0_status(status);				\
	}								\
	__clear_r5_hw_ll_bit();						\
	__clear_software_ll_bit();					\
	if (cpu_has_userlocal)						\
		write_c0_userlocal(task_thread_info(next)->tp_value);	\
	__restore_watch(next);						\
	(last) = resume(prev, next, task_thread_info(next));		\
} while (0)
#endif  
