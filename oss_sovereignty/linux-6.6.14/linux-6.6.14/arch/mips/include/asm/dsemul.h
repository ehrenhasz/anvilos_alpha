#ifndef __MIPS_ASM_DSEMUL_H__
#define __MIPS_ASM_DSEMUL_H__
#include <asm/break.h>
#include <asm/inst.h>
#define BREAK_MATH(micromips)	(((micromips) ? 0x7 : 0xd) | (BRK_MEMU << 16))
#define BD_EMUFRAME_NONE	((int)BIT(31))
struct mm_struct;
struct pt_regs;
struct task_struct;
extern int mips_dsemul(struct pt_regs *regs, mips_instruction ir,
		       unsigned long branch_pc, unsigned long cont_pc);
#ifdef CONFIG_MIPS_FP_SUPPORT
extern bool do_dsemulret(struct pt_regs *xcp);
#else
static inline bool do_dsemulret(struct pt_regs *xcp)
{
	return false;
}
#endif
#ifdef CONFIG_MIPS_FP_SUPPORT
extern bool dsemul_thread_cleanup(struct task_struct *tsk);
#else
static inline bool dsemul_thread_cleanup(struct task_struct *tsk)
{
	return false;
}
#endif
#ifdef CONFIG_MIPS_FP_SUPPORT
extern bool dsemul_thread_rollback(struct pt_regs *regs);
#else
static inline bool dsemul_thread_rollback(struct pt_regs *regs)
{
	return false;
}
#endif
#ifdef CONFIG_MIPS_FP_SUPPORT
extern void dsemul_mm_cleanup(struct mm_struct *mm);
#else
static inline void dsemul_mm_cleanup(struct mm_struct *mm)
{
}
#endif
#endif  
