#ifndef __ASM_ALPHA_PROCESSOR_H
#define __ASM_ALPHA_PROCESSOR_H
#include <linux/personality.h>	 
#define TASK_SIZE (0x40000000000UL)
#define STACK_TOP \
  (current->personality & ADDR_LIMIT_32BIT ? 0x80000000 : 0x00120000000UL)
#define STACK_TOP_MAX	0x00120000000UL
#define TASK_UNMAPPED_BASE \
  ((current->personality & ADDR_LIMIT_32BIT) ? 0x40000000 : TASK_SIZE / 2)
struct thread_struct { };
#define INIT_THREAD  { }
struct pt_regs;
extern void start_thread(struct pt_regs *, unsigned long, unsigned long);
struct task_struct;
unsigned long __get_wchan(struct task_struct *p);
#define KSTK_EIP(tsk) (task_pt_regs(tsk)->pc)
#define KSTK_ESP(tsk) \
  ((tsk) == current ? rdusp() : task_thread_info(tsk)->pcb.usp)
#define cpu_relax()	barrier()
#define ARCH_HAS_PREFETCH
#define ARCH_HAS_PREFETCHW
extern inline void prefetch(const void *ptr)  
{ 
	__builtin_prefetch(ptr, 0, 3);
}
extern inline void prefetchw(const void *ptr)  
{
	__builtin_prefetch(ptr, 1, 3);
}
#endif  
