#ifndef _ASM_POWERPC_PTRACE_H
#define _ASM_POWERPC_PTRACE_H
#include <linux/err.h>
#include <uapi/asm/ptrace.h>
#include <asm/asm-const.h>
#include <asm/reg.h>
#ifndef __ASSEMBLY__
struct pt_regs
{
	union {
		struct user_pt_regs user_regs;
		struct {
			unsigned long gpr[32];
			unsigned long nip;
			unsigned long msr;
			unsigned long orig_gpr3;
			unsigned long ctr;
			unsigned long link;
			unsigned long xer;
			unsigned long ccr;
#ifdef CONFIG_PPC64
			unsigned long softe;
#else
			unsigned long mq;
#endif
			unsigned long trap;
			union {
				unsigned long dar;
				unsigned long dear;
			};
			union {
				unsigned long dsisr;
				unsigned long esr;
			};
			unsigned long result;
		};
	};
#if defined(CONFIG_PPC64) || defined(CONFIG_PPC_KUAP)
	union {
		struct {
#ifdef CONFIG_PPC64
			unsigned long ppr;
			unsigned long exit_result;
#endif
			union {
#ifdef CONFIG_PPC_KUAP
				unsigned long kuap;
#endif
#ifdef CONFIG_PPC_PKEY
				unsigned long amr;
#endif
			};
#ifdef CONFIG_PPC_PKEY
			unsigned long iamr;
#endif
		};
		unsigned long __pad[4];	 
	};
#endif
#if defined(CONFIG_PPC32) && defined(CONFIG_BOOKE)
	struct {  
		unsigned long mas0;
		unsigned long mas1;
		unsigned long mas2;
		unsigned long mas3;
		unsigned long mas6;
		unsigned long mas7;
		unsigned long srr0;
		unsigned long srr1;
		unsigned long csrr0;
		unsigned long csrr1;
		unsigned long dsrr0;
		unsigned long dsrr1;
	};
#endif
};
#endif
#ifdef CONFIG_CPU_BIG_ENDIAN
#define STACK_FRAME_REGS_MARKER	ASM_CONST(0x52454753)
#else
#define STACK_FRAME_REGS_MARKER	ASM_CONST(0x53474552)
#endif
#ifdef __powerpc64__
#define USER_REDZONE_SIZE	512
#define KERNEL_REDZONE_SIZE	288
#define STACK_FRAME_LR_SAVE	2	 
#ifdef CONFIG_PPC64_ELF_ABI_V2
#define STACK_FRAME_MIN_SIZE	32
#define STACK_USER_INT_FRAME_SIZE	(sizeof(struct pt_regs) + STACK_FRAME_MIN_SIZE + 16)
#define STACK_INT_FRAME_REGS	(STACK_FRAME_MIN_SIZE + 16)
#define STACK_INT_FRAME_MARKER	STACK_FRAME_MIN_SIZE
#define STACK_SWITCH_FRAME_SIZE (sizeof(struct pt_regs) + STACK_FRAME_MIN_SIZE + 16)
#define STACK_SWITCH_FRAME_REGS	(STACK_FRAME_MIN_SIZE + 16)
#else
#define STACK_FRAME_MIN_SIZE	112
#define STACK_USER_INT_FRAME_SIZE	(sizeof(struct pt_regs) + STACK_FRAME_MIN_SIZE)
#define STACK_INT_FRAME_REGS	STACK_FRAME_MIN_SIZE
#define STACK_INT_FRAME_MARKER	(STACK_FRAME_MIN_SIZE - 16)
#define STACK_SWITCH_FRAME_SIZE	(sizeof(struct pt_regs) + STACK_FRAME_MIN_SIZE)
#define STACK_SWITCH_FRAME_REGS	STACK_FRAME_MIN_SIZE
#endif
#define __SIGNAL_FRAMESIZE	128
#define __SIGNAL_FRAMESIZE32	64
#else  
#define USER_REDZONE_SIZE	0
#define KERNEL_REDZONE_SIZE	0
#define STACK_FRAME_MIN_SIZE	16
#define STACK_FRAME_LR_SAVE	1	 
#define STACK_USER_INT_FRAME_SIZE	(sizeof(struct pt_regs) + STACK_FRAME_MIN_SIZE)
#define STACK_INT_FRAME_REGS	STACK_FRAME_MIN_SIZE
#define STACK_INT_FRAME_MARKER	(STACK_FRAME_MIN_SIZE - 8)
#define STACK_SWITCH_FRAME_SIZE	(sizeof(struct pt_regs) + STACK_FRAME_MIN_SIZE)
#define STACK_SWITCH_FRAME_REGS	STACK_FRAME_MIN_SIZE
#define __SIGNAL_FRAMESIZE	64
#endif  
#define STACK_INT_FRAME_SIZE	(KERNEL_REDZONE_SIZE + STACK_USER_INT_FRAME_SIZE)
#define STACK_INT_FRAME_MARKER_LONGS	(STACK_INT_FRAME_MARKER/sizeof(long))
#ifndef __ASSEMBLY__
#include <asm/paca.h>
#ifdef CONFIG_SMP
extern unsigned long profile_pc(struct pt_regs *regs);
#else
#define profile_pc(regs) instruction_pointer(regs)
#endif
long do_syscall_trace_enter(struct pt_regs *regs);
void do_syscall_trace_leave(struct pt_regs *regs);
static inline void set_return_regs_changed(void)
{
#ifdef CONFIG_PPC_BOOK3S_64
	WRITE_ONCE(local_paca->hsrr_valid, 0);
	WRITE_ONCE(local_paca->srr_valid, 0);
#endif
}
static inline void regs_set_return_ip(struct pt_regs *regs, unsigned long ip)
{
	regs->nip = ip;
	set_return_regs_changed();
}
static inline void regs_set_return_msr(struct pt_regs *regs, unsigned long msr)
{
	regs->msr = msr;
	set_return_regs_changed();
}
static inline void regs_add_return_ip(struct pt_regs *regs, long offset)
{
	regs_set_return_ip(regs, regs->nip + offset);
}
static inline unsigned long instruction_pointer(struct pt_regs *regs)
{
	return regs->nip;
}
static inline void instruction_pointer_set(struct pt_regs *regs,
		unsigned long val)
{
	regs_set_return_ip(regs, val);
}
static inline unsigned long user_stack_pointer(struct pt_regs *regs)
{
	return regs->gpr[1];
}
static inline unsigned long frame_pointer(struct pt_regs *regs)
{
	return 0;
}
#define user_mode(regs) (((regs)->msr & MSR_PR) != 0)
#define force_successful_syscall_return()   \
	do { \
		set_thread_flag(TIF_NOERROR); \
	} while(0)
#define current_pt_regs() \
	((struct pt_regs *)((unsigned long)task_stack_page(current) + THREAD_SIZE) - 1)
#ifdef __powerpc64__
#define TRAP_FLAGS_MASK		0x1
#else
#define TRAP_FLAGS_MASK		0xf
#define IS_CRITICAL_EXC(regs)	(((regs)->trap & 2) != 0)
#define IS_MCHECK_EXC(regs)	(((regs)->trap & 4) != 0)
#define IS_DEBUG_EXC(regs)	(((regs)->trap & 8) != 0)
#endif  
#define TRAP(regs)		((regs)->trap & ~TRAP_FLAGS_MASK)
static __always_inline void set_trap(struct pt_regs *regs, unsigned long val)
{
	regs->trap = (regs->trap & TRAP_FLAGS_MASK) | (val & ~TRAP_FLAGS_MASK);
}
static inline bool trap_is_scv(struct pt_regs *regs)
{
	return (IS_ENABLED(CONFIG_PPC_BOOK3S_64) && TRAP(regs) == 0x3000);
}
static inline bool trap_is_unsupported_scv(struct pt_regs *regs)
{
	return IS_ENABLED(CONFIG_PPC_BOOK3S_64) && TRAP(regs) == 0x7ff0;
}
static inline bool trap_is_syscall(struct pt_regs *regs)
{
	return (trap_is_scv(regs) || TRAP(regs) == 0xc00);
}
static inline bool trap_norestart(struct pt_regs *regs)
{
	return regs->trap & 0x1;
}
static __always_inline void set_trap_norestart(struct pt_regs *regs)
{
	regs->trap |= 0x1;
}
#define kernel_stack_pointer(regs) ((regs)->gpr[1])
static inline int is_syscall_success(struct pt_regs *regs)
{
	if (trap_is_scv(regs))
		return !IS_ERR_VALUE((unsigned long)regs->gpr[3]);
	else
		return !(regs->ccr & 0x10000000);
}
static inline long regs_return_value(struct pt_regs *regs)
{
	if (trap_is_scv(regs))
		return regs->gpr[3];
	if (is_syscall_success(regs))
		return regs->gpr[3];
	else
		return -regs->gpr[3];
}
static inline void regs_set_return_value(struct pt_regs *regs, unsigned long rc)
{
	regs->gpr[3] = rc;
}
static inline bool cpu_has_msr_ri(void)
{
	return !IS_ENABLED(CONFIG_BOOKE_OR_40x);
}
static inline bool regs_is_unrecoverable(struct pt_regs *regs)
{
	return unlikely(cpu_has_msr_ri() && !(regs->msr & MSR_RI));
}
static inline void regs_set_recoverable(struct pt_regs *regs)
{
	if (cpu_has_msr_ri())
		regs_set_return_msr(regs, regs->msr | MSR_RI);
}
static inline void regs_set_unrecoverable(struct pt_regs *regs)
{
	if (cpu_has_msr_ri())
		regs_set_return_msr(regs, regs->msr & ~MSR_RI);
}
#define arch_has_single_step()	(1)
#define arch_has_block_step()	(true)
#define ARCH_HAS_USER_SINGLE_STEP_REPORT
#include <linux/stddef.h>
#include <linux/thread_info.h>
extern int regs_query_register_offset(const char *name);
extern const char *regs_query_register_name(unsigned int offset);
#define MAX_REG_OFFSET (offsetof(struct pt_regs, dsisr))
static inline unsigned long regs_get_register(struct pt_regs *regs,
						unsigned int offset)
{
	if (unlikely(offset > MAX_REG_OFFSET))
		return 0;
	return *(unsigned long *)((unsigned long)regs + offset);
}
static inline bool regs_within_kernel_stack(struct pt_regs *regs,
						unsigned long addr)
{
	return ((addr & ~(THREAD_SIZE - 1))  ==
		(kernel_stack_pointer(regs) & ~(THREAD_SIZE - 1)));
}
static inline unsigned long regs_get_kernel_stack_nth(struct pt_regs *regs,
						      unsigned int n)
{
	unsigned long *addr = (unsigned long *)kernel_stack_pointer(regs);
	addr += n;
	if (regs_within_kernel_stack(regs, (unsigned long)addr))
		return *addr;
	else
		return 0;
}
#endif  
#ifndef __powerpc64__
#define PT_SOFTE PT_MQ
#else  
#define PT_FPSCR32 (PT_FPR0 + 2*32 + 1)	 
#define PT_VR0_32 164	 
#define PT_VSCR_32 (PT_VR0 + 32*4 + 3)
#define PT_VRSAVE_32 (PT_VR0 + 33*4)
#define PT_VSR0_32 300 	 
#endif  
#endif  
