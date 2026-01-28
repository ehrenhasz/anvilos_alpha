#ifndef _XTENSA_PROCESSOR_H
#define _XTENSA_PROCESSOR_H
#include <asm/core.h>
#include <linux/compiler.h>
#include <linux/stringify.h>
#include <asm/bootparam.h>
#include <asm/ptrace.h>
#include <asm/types.h>
#include <asm/regs.h>
#define ARCH_SLAB_MINALIGN XTENSA_STACK_ALIGNMENT
#ifdef CONFIG_MMU
#define TASK_SIZE	__XTENSA_UL_CONST(0x40000000)
#else
#define TASK_SIZE	__XTENSA_UL_CONST(0xffffffff)
#endif
#define STACK_TOP	TASK_SIZE
#define STACK_TOP_MAX	STACK_TOP
#define EXCCAUSE_MAPPED_NMI	62
#define EXCCAUSE_MAPPED_DEBUG	63
#define VALID_DOUBLE_EXCEPTION_ADDRESS	64
#define XTENSA_INT_LEVEL(intno) _XTENSA_INT_LEVEL(intno)
#define _XTENSA_INT_LEVEL(intno) XCHAL_INT##intno##_LEVEL
#define XTENSA_INTLEVEL_MASK(level) _XTENSA_INTLEVEL_MASK(level)
#define _XTENSA_INTLEVEL_MASK(level) (XCHAL_INTLEVEL##level##_MASK)
#define XTENSA_INTLEVEL_ANDBELOW_MASK(l) _XTENSA_INTLEVEL_ANDBELOW_MASK(l)
#define _XTENSA_INTLEVEL_ANDBELOW_MASK(l) (XCHAL_INTLEVEL##l##_ANDBELOW_MASK)
#define PROFILING_INTLEVEL XTENSA_INT_LEVEL(XCHAL_PROFILING_INTERRUPT)
#if defined(CONFIG_XTENSA_FAKE_NMI) && defined(XCHAL_PROFILING_INTERRUPT)
#define LOCKLEVEL (PROFILING_INTLEVEL - 1)
#else
#define LOCKLEVEL XCHAL_EXCM_LEVEL
#endif
#define TOPLEVEL XCHAL_EXCM_LEVEL
#define XTENSA_FAKE_NMI (LOCKLEVEL < TOPLEVEL)
#define WSBITS  (XCHAL_NUM_AREGS / 4)       
#define WBBITS  (XCHAL_NUM_AREGS_LOG2 - 2)  
#if defined(__XTENSA_WINDOWED_ABI__)
#define KERNEL_PS_WOE_MASK PS_WOE_MASK
#elif defined(__XTENSA_CALL0_ABI__)
#define KERNEL_PS_WOE_MASK 0
#else
#error Unsupported xtensa ABI
#endif
#ifndef __ASSEMBLY__
#if defined(__XTENSA_WINDOWED_ABI__)
#define MAKE_RA_FOR_CALL(ra,ws)   (((ra) & 0x3fffffff) | (ws) << 30)
#define MAKE_PC_FROM_RA(ra,sp)    (((ra) & 0x3fffffff) | ((sp) & 0xc0000000))
#elif defined(__XTENSA_CALL0_ABI__)
#define MAKE_RA_FOR_CALL(ra, ws)   (ra)
#define MAKE_PC_FROM_RA(ra, sp)    (ra)
#else
#error Unsupported Xtensa ABI
#endif
#define SPILL_SLOT(sp, reg) (*(((unsigned long *)(sp)) - 4 + (reg)))
#define SPILL_SLOT_CALL8(sp, reg) (*(((unsigned long *)(sp)) - 12 + (reg)))
#define SPILL_SLOT_CALL12(sp, reg) (*(((unsigned long *)(sp)) - 16 + (reg)))
struct thread_struct {
	unsigned long ra;  
	unsigned long sp;  
#ifdef CONFIG_HAVE_HW_BREAKPOINT
	struct perf_event *ptrace_bp[XCHAL_NUM_IBREAK];
	struct perf_event *ptrace_wp[XCHAL_NUM_DBREAK];
#endif
	int align[0] __attribute__ ((aligned(16)));
};
#define TASK_UNMAPPED_BASE	(TASK_SIZE / 2)
#define INIT_THREAD  \
{									\
	ra:		0, 						\
	sp:		sizeof(init_stack) + (long) &init_stack,	\
}
#if IS_ENABLED(CONFIG_USER_ABI_CALL0)
#define USER_PS_VALUE ((USER_RING << PS_RING_SHIFT) |			\
		       (1 << PS_UM_BIT) |				\
		       (1 << PS_EXCM_BIT))
#else
#define USER_PS_VALUE (PS_WOE_MASK |					\
		       (1 << PS_CALLINC_SHIFT) |			\
		       (USER_RING << PS_RING_SHIFT) |			\
		       (1 << PS_UM_BIT) |				\
		       (1 << PS_EXCM_BIT))
#endif
#define start_thread(regs, new_pc, new_sp) \
	do { \
		unsigned long syscall = (regs)->syscall; \
		unsigned long current_aregs[16]; \
		memcpy(current_aregs, (regs)->areg, sizeof(current_aregs)); \
		memset((regs), 0, sizeof(*(regs))); \
		(regs)->pc = (new_pc); \
		(regs)->ps = USER_PS_VALUE; \
		memcpy((regs)->areg, current_aregs, sizeof(current_aregs)); \
		(regs)->areg[1] = (new_sp); \
		(regs)->areg[0] = 0; \
		(regs)->wmask = 1; \
		(regs)->depc = 0; \
		(regs)->windowbase = 0; \
		(regs)->windowstart = 1; \
		(regs)->syscall = syscall; \
	} while (0)
struct task_struct;
struct mm_struct;
extern unsigned long __get_wchan(struct task_struct *p);
void init_arch(bp_tag_t *bp_start);
void do_notify_resume(struct pt_regs *regs);
#define KSTK_EIP(tsk)		(task_pt_regs(tsk)->pc)
#define KSTK_ESP(tsk)		(task_pt_regs(tsk)->areg[1])
#define cpu_relax()  barrier()
#define xtensa_set_sr(x, sr) \
	({ \
	 __asm__ __volatile__ ("wsr %0, "__stringify(sr) :: \
			       "a"((unsigned int)(x))); \
	 })
#define xtensa_get_sr(sr) \
	({ \
	 unsigned int v; \
	 __asm__ __volatile__ ("rsr %0, "__stringify(sr) : "=a"(v)); \
	 v; \
	 })
#define xtensa_xsr(x, sr) \
	({ \
	 unsigned int __v__ = (unsigned int)(x); \
	 __asm__ __volatile__ ("xsr %0, " __stringify(sr) : "+a"(__v__)); \
	 __v__; \
	 })
#if XCHAL_HAVE_EXTERN_REGS
static inline void set_er(unsigned long value, unsigned long addr)
{
	asm volatile ("wer %0, %1" : : "a" (value), "a" (addr) : "memory");
}
static inline unsigned long get_er(unsigned long addr)
{
	register unsigned long value;
	asm volatile ("rer %0, %1" : "=a" (value) : "a" (addr) : "memory");
	return value;
}
#endif  
#endif	 
#endif	 
