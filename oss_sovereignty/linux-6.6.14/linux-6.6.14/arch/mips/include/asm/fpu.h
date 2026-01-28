#ifndef _ASM_FPU_H
#define _ASM_FPU_H
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/ptrace.h>
#include <linux/thread_info.h>
#include <linux/bitops.h>
#include <asm/mipsregs.h>
#include <asm/cpu.h>
#include <asm/cpu-features.h>
#include <asm/fpu_emulator.h>
#include <asm/hazards.h>
#include <asm/ptrace.h>
#include <asm/processor.h>
#include <asm/current.h>
#include <asm/msa.h>
#ifdef CONFIG_MIPS_MT_FPAFF
#include <asm/mips_mt.h>
#endif
enum fpu_mode {
	FPU_32BIT = 0,		 
	FPU_64BIT,		 
	FPU_AS_IS,
	FPU_HYBRID,		 
#define FPU_FR_MASK		0x1
};
#ifdef CONFIG_MIPS_FP_SUPPORT
extern void _save_fp(struct task_struct *);
extern void _restore_fp(struct task_struct *);
#define __disable_fpu()							\
do {									\
	clear_c0_status(ST0_CU1);					\
	disable_fpu_hazard();						\
} while (0)
static inline int __enable_fpu(enum fpu_mode mode)
{
	int fr;
	switch (mode) {
	case FPU_AS_IS:
		set_c0_status(ST0_CU1);
		enable_fpu_hazard();
		return 0;
	case FPU_HYBRID:
		if (!cpu_has_fre)
			return SIGFPE;
		set_c0_config5(MIPS_CONF5_FRE);
		goto fr_common;
	case FPU_64BIT:
#if !(defined(CONFIG_CPU_MIPSR2) || defined(CONFIG_CPU_MIPSR5) || \
      defined(CONFIG_CPU_MIPSR6) || defined(CONFIG_64BIT))
		return SIGFPE;
#endif
	case FPU_32BIT:
		if (cpu_has_fre) {
			clear_c0_config5(MIPS_CONF5_FRE);
		}
fr_common:
		fr = (int)mode & FPU_FR_MASK;
		change_c0_status(ST0_CU1 | ST0_FR, ST0_CU1 | (fr ? ST0_FR : 0));
		enable_fpu_hazard();
		if (!!(read_c0_status() & ST0_FR) == !!fr)
			return 0;
		__disable_fpu();
		return SIGFPE;
	default:
		BUG();
	}
	return SIGFPE;
}
#define clear_fpu_owner()	clear_thread_flag(TIF_USEDFPU)
static inline int __is_fpu_owner(void)
{
	return test_thread_flag(TIF_USEDFPU);
}
static inline int is_fpu_owner(void)
{
	return cpu_has_fpu && __is_fpu_owner();
}
static inline int __own_fpu(void)
{
	enum fpu_mode mode;
	int ret;
	if (test_thread_flag(TIF_HYBRID_FPREGS))
		mode = FPU_HYBRID;
	else
		mode = !test_thread_flag(TIF_32BIT_FPREGS);
	ret = __enable_fpu(mode);
	if (ret)
		return ret;
	KSTK_STATUS(current) |= ST0_CU1;
	if (mode == FPU_64BIT || mode == FPU_HYBRID)
		KSTK_STATUS(current) |= ST0_FR;
	else  
		KSTK_STATUS(current) &= ~ST0_FR;
	set_thread_flag(TIF_USEDFPU);
	return 0;
}
static inline int own_fpu_inatomic(int restore)
{
	int ret = 0;
	if (cpu_has_fpu && !__is_fpu_owner()) {
		ret = __own_fpu();
		if (restore && !ret)
			_restore_fp(current);
	}
	return ret;
}
static inline int own_fpu(int restore)
{
	int ret;
	preempt_disable();
	ret = own_fpu_inatomic(restore);
	preempt_enable();
	return ret;
}
static inline void lose_fpu_inatomic(int save, struct task_struct *tsk)
{
	if (is_msa_enabled()) {
		if (save) {
			save_msa(tsk);
			tsk->thread.fpu.fcr31 =
					read_32bit_cp1_register(CP1_STATUS);
		}
		disable_msa();
		clear_tsk_thread_flag(tsk, TIF_USEDMSA);
		__disable_fpu();
	} else if (is_fpu_owner()) {
		if (save)
			_save_fp(tsk);
		__disable_fpu();
	} else {
		WARN(read_c0_status() & ST0_CU1,
		     "Orphaned FPU left enabled");
	}
	KSTK_STATUS(tsk) &= ~ST0_CU1;
	clear_tsk_thread_flag(tsk, TIF_USEDFPU);
}
static inline void lose_fpu(int save)
{
	preempt_disable();
	lose_fpu_inatomic(save, current);
	preempt_enable();
}
static inline bool init_fp_ctx(struct task_struct *target)
{
	if (tsk_used_math(target))
		return false;
	memset(&target->thread.fpu.fpr, ~0, sizeof(target->thread.fpu.fpr));
	set_stopped_child_used_math(target);
	return true;
}
static inline void save_fp(struct task_struct *tsk)
{
	if (cpu_has_fpu)
		_save_fp(tsk);
}
static inline void restore_fp(struct task_struct *tsk)
{
	if (cpu_has_fpu)
		_restore_fp(tsk);
}
static inline union fpureg *get_fpu_regs(struct task_struct *tsk)
{
	if (tsk == current) {
		preempt_disable();
		if (is_fpu_owner())
			_save_fp(current);
		preempt_enable();
	}
	return tsk->thread.fpu.fpr;
}
#else  
static inline int __enable_fpu(enum fpu_mode mode)
{
	return SIGILL;
}
static inline void __disable_fpu(void)
{
}
static inline int is_fpu_owner(void)
{
	return 0;
}
static inline void clear_fpu_owner(void)
{
}
static inline int own_fpu_inatomic(int restore)
{
	return SIGILL;
}
static inline int own_fpu(int restore)
{
	return SIGILL;
}
static inline void lose_fpu_inatomic(int save, struct task_struct *tsk)
{
}
static inline void lose_fpu(int save)
{
}
static inline bool init_fp_ctx(struct task_struct *target)
{
	return false;
}
extern void save_fp(struct task_struct *tsk)
	__compiletime_error("save_fp() should not be called when CONFIG_MIPS_FP_SUPPORT=n");
extern void _save_fp(struct task_struct *)
	__compiletime_error("_save_fp() should not be called when CONFIG_MIPS_FP_SUPPORT=n");
extern void restore_fp(struct task_struct *tsk)
	__compiletime_error("restore_fp() should not be called when CONFIG_MIPS_FP_SUPPORT=n");
extern void _restore_fp(struct task_struct *)
	__compiletime_error("_restore_fp() should not be called when CONFIG_MIPS_FP_SUPPORT=n");
extern union fpureg *get_fpu_regs(struct task_struct *tsk)
	__compiletime_error("get_fpu_regs() should not be called when CONFIG_MIPS_FP_SUPPORT=n");
#endif  
#endif  
