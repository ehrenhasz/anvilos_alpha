#ifndef _ASM_MICROBLAZE_PROCESSOR_H
#define _ASM_MICROBLAZE_PROCESSOR_H
#include <asm/ptrace.h>
#include <asm/setup.h>
#include <asm/registers.h>
#include <asm/entry.h>
#include <asm/current.h>
# ifndef __ASSEMBLY__
extern const struct seq_operations cpuinfo_op;
# define cpu_relax()		barrier()
#define task_pt_regs(tsk) \
		(((struct pt_regs *)(THREAD_SIZE + task_stack_page(tsk))) - 1)
void start_thread(struct pt_regs *regs, unsigned long pc, unsigned long usp);
extern void ret_from_fork(void);
extern void ret_from_kernel_thread(void);
# endif  
# define TASK_SIZE	(CONFIG_KERNEL_START)
# define TASK_UNMAPPED_BASE	(TASK_SIZE / 8 * 3)
# define THREAD_KSP	0
#  ifndef __ASSEMBLY__
struct thread_struct {
	unsigned long	ksp;
	unsigned long	ksp_limit;	 
	void		*pgdir;		 
	struct pt_regs	*regs;		 
};
#  define INIT_THREAD { \
	.ksp   = sizeof init_stack + (unsigned long)init_stack, \
	.pgdir = swapper_pg_dir, \
}
unsigned long __get_wchan(struct task_struct *p);
# define KERNEL_STACK_SIZE	0x2000
#  define task_tos(task)	((unsigned long)(task) + KERNEL_STACK_SIZE)
#  define task_regs(task) ((struct pt_regs *)task_tos(task) - 1)
#  define task_pt_regs_plus_args(tsk) \
	((void *)task_pt_regs(tsk))
#  define task_sp(task)	(task_regs(task)->r1)
#  define task_pc(task)	(task_regs(task)->pc)
#  define KSTK_EIP(task)	(task_pc(task))
#  define KSTK_ESP(task)	(task_sp(task))
#  define STACK_TOP	TASK_SIZE
#  define STACK_TOP_MAX	STACK_TOP
#ifdef CONFIG_DEBUG_FS
extern struct dentry *of_debugfs_root;
#endif
#  endif  
#endif  
