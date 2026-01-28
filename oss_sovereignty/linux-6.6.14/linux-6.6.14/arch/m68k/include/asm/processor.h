#ifndef __ASM_M68K_PROCESSOR_H
#define __ASM_M68K_PROCESSOR_H
#include <linux/thread_info.h>
#include <asm/fpu.h>
#include <asm/ptrace.h>
static inline unsigned long rdusp(void)
{
#ifdef CONFIG_COLDFIRE_SW_A7
	extern unsigned int sw_usp;
	return sw_usp;
#else
	register unsigned long usp __asm__("a0");
	__asm__ __volatile__(".word 0x4e68" : "=a" (usp));
	return usp;
#endif
}
static inline void wrusp(unsigned long usp)
{
#ifdef CONFIG_COLDFIRE_SW_A7
	extern unsigned int sw_usp;
	sw_usp = usp;
#else
	register unsigned long a0 __asm__("a0") = usp;
	__asm__ __volatile__(".word 0x4e60" : : "a" (a0) );
#endif
}
#ifdef CONFIG_MMU
#if defined(CONFIG_COLDFIRE)
#define TASK_SIZE	(0xC0000000UL)
#elif defined(CONFIG_SUN3)
#define TASK_SIZE	(0x0E000000UL)
#else
#define TASK_SIZE	(0xF0000000UL)
#endif
#else
#define TASK_SIZE	(0xFFFFFFFFUL)
#endif
#ifdef __KERNEL__
#define STACK_TOP	TASK_SIZE
#define STACK_TOP_MAX	STACK_TOP
#endif
#ifdef CONFIG_MMU
#if defined(CONFIG_COLDFIRE)
#define TASK_UNMAPPED_BASE	0x60000000UL
#elif defined(CONFIG_SUN3)
#define TASK_UNMAPPED_BASE	0x0A000000UL
#else
#define TASK_UNMAPPED_BASE	0xC0000000UL
#endif
#define TASK_UNMAPPED_ALIGN(addr, off)	PAGE_ALIGN(addr)
#else
#define TASK_UNMAPPED_BASE	0
#endif
#define USER_DATA     1
#define USER_PROGRAM  2
#define SUPER_DATA    5
#define SUPER_PROGRAM 6
#define CPU_SPACE     7
#ifdef CONFIG_CPU_HAS_ADDRESS_SPACES
static inline void set_fc(unsigned long val)
{
	WARN_ON_ONCE(in_interrupt());
	__asm__ __volatile__ ("movec %0,%/sfc\n\t"
			      "movec %0,%/dfc\n\t"
			      :   : "r" (val) : "memory");
}
#else
static inline void set_fc(unsigned long val)
{
}
#endif  
struct thread_struct {
	unsigned long  ksp;		 
	unsigned long  usp;		 
	unsigned short sr;		 
	unsigned short fc;		 
	unsigned long  crp[2];		 
	unsigned long  esp0;		 
	unsigned long  faddr;		 
	int            signo, code;
	unsigned long  fp[8*3];
	unsigned long  fpcntl[3];	 
	unsigned char  fpstate[FPSTATESIZE];   
};
#define INIT_THREAD  {							\
	.ksp	= sizeof(init_stack) + (unsigned long) init_stack,	\
	.sr	= PS_S,							\
	.fc	= USER_DATA,						\
}
#ifdef CONFIG_COLDFIRE
#define setframeformat(_regs)	do { (_regs)->format = 0x4; } while(0)
#else
#define setframeformat(_regs)	do { } while (0)
#endif
static inline void start_thread(struct pt_regs * regs, unsigned long pc,
				unsigned long usp)
{
	regs->pc = pc;
	regs->sr &= ~0x2000;
	setframeformat(regs);
	wrusp(usp);
}
struct task_struct;
unsigned long __get_wchan(struct task_struct *p);
void show_registers(struct pt_regs *regs);
#define	KSTK_EIP(tsk)	\
    ({			\
	unsigned long eip = 0;	 \
	if ((tsk)->thread.esp0 > PAGE_SIZE && \
	    (virt_addr_valid((tsk)->thread.esp0))) \
	      eip = ((struct pt_regs *) (tsk)->thread.esp0)->pc; \
	eip; })
#define	KSTK_ESP(tsk)	((tsk) == current ? rdusp() : (tsk)->thread.usp)
#define task_pt_regs(tsk)	((struct pt_regs *) ((tsk)->thread.esp0))
#define cpu_relax()	barrier()
#endif
