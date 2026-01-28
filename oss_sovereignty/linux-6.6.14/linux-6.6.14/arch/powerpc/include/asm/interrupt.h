#ifndef _ASM_POWERPC_INTERRUPT_H
#define _ASM_POWERPC_INTERRUPT_H
#define INTERRUPT_CRITICAL_INPUT  0x100
#define INTERRUPT_DEBUG           0xd00
#ifdef CONFIG_BOOKE
#define INTERRUPT_PERFMON         0x260
#define INTERRUPT_DOORBELL        0x280
#endif
#define INTERRUPT_MACHINE_CHECK   0x200
#define INTERRUPT_SYSTEM_RESET    0x100
#define INTERRUPT_DATA_SEGMENT    0x380
#define INTERRUPT_INST_SEGMENT    0x480
#define INTERRUPT_TRACE           0xd00
#define INTERRUPT_H_DATA_STORAGE  0xe00
#define INTERRUPT_HMI			0xe60
#define INTERRUPT_H_FAC_UNAVAIL   0xf80
#ifdef CONFIG_PPC_BOOK3S
#define INTERRUPT_DOORBELL        0xa00
#define INTERRUPT_PERFMON         0xf00
#define INTERRUPT_ALTIVEC_UNAVAIL	0xf20
#endif
#define INTERRUPT_DATA_STORAGE    0x300
#define INTERRUPT_INST_STORAGE    0x400
#define INTERRUPT_EXTERNAL		0x500
#define INTERRUPT_ALIGNMENT       0x600
#define INTERRUPT_PROGRAM         0x700
#define INTERRUPT_SYSCALL         0xc00
#define INTERRUPT_TRACE			0xd00
#define INTERRUPT_FP_UNAVAIL      0x800
#define INTERRUPT_DECREMENTER     0x900
#ifndef INTERRUPT_PERFMON
#define INTERRUPT_PERFMON         0x0
#endif
#define INTERRUPT_SOFT_EMU_8xx		0x1000
#define INTERRUPT_INST_TLB_MISS_8xx	0x1100
#define INTERRUPT_DATA_TLB_MISS_8xx	0x1200
#define INTERRUPT_INST_TLB_ERROR_8xx	0x1300
#define INTERRUPT_DATA_TLB_ERROR_8xx	0x1400
#define INTERRUPT_DATA_BREAKPOINT_8xx	0x1c00
#define INTERRUPT_INST_BREAKPOINT_8xx	0x1d00
#define INTERRUPT_INST_TLB_MISS_603		0x1000
#define INTERRUPT_DATA_LOAD_TLB_MISS_603	0x1100
#define INTERRUPT_DATA_STORE_TLB_MISS_603	0x1200
#ifndef __ASSEMBLY__
#include <linux/context_tracking.h>
#include <linux/hardirq.h>
#include <asm/cputime.h>
#include <asm/firmware.h>
#include <asm/ftrace.h>
#include <asm/kprobes.h>
#include <asm/runlatch.h>
#ifdef CONFIG_PPC_IRQ_SOFT_MASK_DEBUG
#define INT_SOFT_MASK_BUG_ON(regs, cond)				\
do {									\
	if ((user_mode(regs) || (TRAP(regs) != INTERRUPT_PROGRAM)))	\
		BUG_ON(cond);						\
} while (0)
#else
#define INT_SOFT_MASK_BUG_ON(regs, cond)
#endif
#ifdef CONFIG_PPC_BOOK3S_64
extern char __end_soft_masked[];
bool search_kernel_soft_mask_table(unsigned long addr);
unsigned long search_kernel_restart_table(unsigned long addr);
DECLARE_STATIC_KEY_FALSE(interrupt_exit_not_reentrant);
static inline bool is_implicit_soft_masked(struct pt_regs *regs)
{
	if (regs->msr & MSR_PR)
		return false;
	if (regs->nip >= (unsigned long)__end_soft_masked)
		return false;
	return search_kernel_soft_mask_table(regs->nip);
}
static inline void srr_regs_clobbered(void)
{
	local_paca->srr_valid = 0;
	local_paca->hsrr_valid = 0;
}
#else
static inline unsigned long search_kernel_restart_table(unsigned long addr)
{
	return 0;
}
static inline bool is_implicit_soft_masked(struct pt_regs *regs)
{
	return false;
}
static inline void srr_regs_clobbered(void)
{
}
#endif
static inline void nap_adjust_return(struct pt_regs *regs)
{
#ifdef CONFIG_PPC_970_NAP
	if (unlikely(test_thread_local_flags(_TLF_NAPPING))) {
		clear_thread_local_flags(_TLF_NAPPING);
		regs_set_return_ip(regs, (unsigned long)power4_idle_nap_return);
	}
#endif
}
static inline void booke_restore_dbcr0(void)
{
#ifdef CONFIG_PPC_ADV_DEBUG_REGS
	unsigned long dbcr0 = current->thread.debug.dbcr0;
	if (IS_ENABLED(CONFIG_PPC32) && unlikely(dbcr0 & DBCR0_IDM)) {
		mtspr(SPRN_DBSR, -1);
		mtspr(SPRN_DBCR0, global_dbcr0[smp_processor_id()]);
	}
#endif
}
static inline void interrupt_enter_prepare(struct pt_regs *regs)
{
#ifdef CONFIG_PPC64
	irq_soft_mask_set(IRQS_ALL_DISABLED);
	if (!(local_paca->irq_happened & PACA_IRQ_HARD_DIS)) {
		INT_SOFT_MASK_BUG_ON(regs, !(regs->msr & MSR_EE));
		__hard_irq_enable();
	} else {
		__hard_RI_enable();
	}
#endif
	if (!arch_irq_disabled_regs(regs))
		trace_hardirqs_off();
	if (user_mode(regs)) {
		kuap_lock();
		CT_WARN_ON(ct_state() != CONTEXT_USER);
		user_exit_irqoff();
		account_cpu_user_entry();
		account_stolen_time();
	} else {
		kuap_save_and_lock(regs);
		if (TRAP(regs) != INTERRUPT_PROGRAM)
			CT_WARN_ON(ct_state() != CONTEXT_KERNEL &&
				   ct_state() != CONTEXT_IDLE);
		INT_SOFT_MASK_BUG_ON(regs, is_implicit_soft_masked(regs));
		INT_SOFT_MASK_BUG_ON(regs, arch_irq_disabled_regs(regs) &&
					   search_kernel_restart_table(regs->nip));
	}
	INT_SOFT_MASK_BUG_ON(regs, !arch_irq_disabled_regs(regs) &&
				   !(regs->msr & MSR_EE));
	booke_restore_dbcr0();
}
static inline void interrupt_exit_prepare(struct pt_regs *regs)
{
}
static inline void interrupt_async_enter_prepare(struct pt_regs *regs)
{
#ifdef CONFIG_PPC64
	local_paca->irq_happened |= PACA_IRQ_HARD_DIS;
#endif
	interrupt_enter_prepare(regs);
#ifdef CONFIG_PPC_BOOK3S_64
	if (cpu_has_feature(CPU_FTR_CTRL) &&
	    !test_thread_local_flags(_TLF_RUNLATCH))
		__ppc64_runlatch_on();
#endif
	irq_enter();
}
static inline void interrupt_async_exit_prepare(struct pt_regs *regs)
{
	nap_adjust_return(regs);
	irq_exit();
	interrupt_exit_prepare(regs);
}
struct interrupt_nmi_state {
#ifdef CONFIG_PPC64
	u8 irq_soft_mask;
	u8 irq_happened;
	u8 ftrace_enabled;
	u64 softe;
#endif
};
static inline bool nmi_disables_ftrace(struct pt_regs *regs)
{
	if (IS_ENABLED(CONFIG_PPC_BOOK3S_64)) {
		if (TRAP(regs) == INTERRUPT_DECREMENTER)
		       return false;
		if (TRAP(regs) == INTERRUPT_PERFMON)
		       return false;
	}
	if (IS_ENABLED(CONFIG_PPC_BOOK3E_64)) {
		if (TRAP(regs) == INTERRUPT_PERFMON)
			return false;
	}
	return true;
}
static inline void interrupt_nmi_enter_prepare(struct pt_regs *regs, struct interrupt_nmi_state *state)
{
#ifdef CONFIG_PPC64
	state->irq_soft_mask = local_paca->irq_soft_mask;
	state->irq_happened = local_paca->irq_happened;
	state->softe = regs->softe;
	local_paca->irq_soft_mask = IRQS_ALL_DISABLED;
	local_paca->irq_happened |= PACA_IRQ_HARD_DIS;
	if (!(regs->msr & MSR_EE) || is_implicit_soft_masked(regs)) {
		regs->softe = IRQS_ALL_DISABLED;
	}
	__hard_RI_enable();
	if (nmi_disables_ftrace(regs)) {
		state->ftrace_enabled = this_cpu_get_ftrace_enabled();
		this_cpu_set_ftrace_enabled(0);
	}
#endif
	if (mfmsr() & MSR_DR) {
		nmi_enter();
		return;
	}
	if (IS_ENABLED(CONFIG_PPC_BOOK3S_64) &&
	    firmware_has_feature(FW_FEATURE_LPAR) &&
	    !radix_enabled())
		return;
	if (IS_ENABLED(CONFIG_KASAN))
		return;
	nmi_enter();
}
static inline void interrupt_nmi_exit_prepare(struct pt_regs *regs, struct interrupt_nmi_state *state)
{
	if (mfmsr() & MSR_DR) {
		nmi_exit();
	} else if (IS_ENABLED(CONFIG_PPC_BOOK3S_64) &&
		   firmware_has_feature(FW_FEATURE_LPAR) &&
		   !radix_enabled()) {
	} else if (IS_ENABLED(CONFIG_KASAN)) {
	} else {
		nmi_exit();
	}
#ifdef CONFIG_PPC64
#ifdef CONFIG_PPC_BOOK3S
	if (arch_irq_disabled_regs(regs)) {
		unsigned long rst = search_kernel_restart_table(regs->nip);
		if (rst)
			regs_set_return_ip(regs, rst);
	}
#endif
	if (nmi_disables_ftrace(regs))
		this_cpu_set_ftrace_enabled(state->ftrace_enabled);
	WARN_ON_ONCE((state->irq_happened | PACA_IRQ_HARD_DIS) != local_paca->irq_happened);
	regs->softe = state->softe;
	local_paca->irq_happened = state->irq_happened;
	local_paca->irq_soft_mask = state->irq_soft_mask;
#endif
}
#define interrupt_handler __visible noinline notrace __no_kcsan __no_sanitize_address
#define DECLARE_INTERRUPT_HANDLER_RAW(func)				\
	__visible long func(struct pt_regs *regs)
#define DEFINE_INTERRUPT_HANDLER_RAW(func)				\
static __always_inline __no_sanitize_address __no_kcsan long		\
____##func(struct pt_regs *regs);					\
									\
interrupt_handler long func(struct pt_regs *regs)			\
{									\
	long ret;							\
									\
	__hard_RI_enable();						\
									\
	ret = ____##func (regs);					\
									\
	return ret;							\
}									\
NOKPROBE_SYMBOL(func);							\
									\
static __always_inline __no_sanitize_address __no_kcsan long		\
____##func(struct pt_regs *regs)
#define DECLARE_INTERRUPT_HANDLER(func)					\
	__visible void func(struct pt_regs *regs)
#define DEFINE_INTERRUPT_HANDLER(func)					\
static __always_inline void ____##func(struct pt_regs *regs);		\
									\
interrupt_handler void func(struct pt_regs *regs)			\
{									\
	interrupt_enter_prepare(regs);					\
									\
	____##func (regs);						\
									\
	interrupt_exit_prepare(regs);					\
}									\
NOKPROBE_SYMBOL(func);							\
									\
static __always_inline void ____##func(struct pt_regs *regs)
#define DECLARE_INTERRUPT_HANDLER_RET(func)				\
	__visible long func(struct pt_regs *regs)
#define DEFINE_INTERRUPT_HANDLER_RET(func)				\
static __always_inline long ____##func(struct pt_regs *regs);		\
									\
interrupt_handler long func(struct pt_regs *regs)			\
{									\
	long ret;							\
									\
	interrupt_enter_prepare(regs);					\
									\
	ret = ____##func (regs);					\
									\
	interrupt_exit_prepare(regs);					\
									\
	return ret;							\
}									\
NOKPROBE_SYMBOL(func);							\
									\
static __always_inline long ____##func(struct pt_regs *regs)
#define DECLARE_INTERRUPT_HANDLER_ASYNC(func)				\
	__visible void func(struct pt_regs *regs)
#define DEFINE_INTERRUPT_HANDLER_ASYNC(func)				\
static __always_inline void ____##func(struct pt_regs *regs);		\
									\
interrupt_handler void func(struct pt_regs *regs)			\
{									\
	interrupt_async_enter_prepare(regs);				\
									\
	____##func (regs);						\
									\
	interrupt_async_exit_prepare(regs);				\
}									\
NOKPROBE_SYMBOL(func);							\
									\
static __always_inline void ____##func(struct pt_regs *regs)
#define DECLARE_INTERRUPT_HANDLER_NMI(func)				\
	__visible long func(struct pt_regs *regs)
#define DEFINE_INTERRUPT_HANDLER_NMI(func)				\
static __always_inline __no_sanitize_address __no_kcsan long		\
____##func(struct pt_regs *regs);					\
									\
interrupt_handler long func(struct pt_regs *regs)			\
{									\
	struct interrupt_nmi_state state;				\
	long ret;							\
									\
	interrupt_nmi_enter_prepare(regs, &state);			\
									\
	ret = ____##func (regs);					\
									\
	interrupt_nmi_exit_prepare(regs, &state);			\
									\
	return ret;							\
}									\
NOKPROBE_SYMBOL(func);							\
									\
static __always_inline  __no_sanitize_address __no_kcsan long		\
____##func(struct pt_regs *regs)
DECLARE_INTERRUPT_HANDLER_NMI(system_reset_exception);
#ifdef CONFIG_PPC_BOOK3S_64
DECLARE_INTERRUPT_HANDLER_RAW(machine_check_early_boot);
DECLARE_INTERRUPT_HANDLER_ASYNC(machine_check_exception_async);
#endif
DECLARE_INTERRUPT_HANDLER_NMI(machine_check_exception);
DECLARE_INTERRUPT_HANDLER(SMIException);
DECLARE_INTERRUPT_HANDLER(handle_hmi_exception);
DECLARE_INTERRUPT_HANDLER(unknown_exception);
DECLARE_INTERRUPT_HANDLER_ASYNC(unknown_async_exception);
DECLARE_INTERRUPT_HANDLER_NMI(unknown_nmi_exception);
DECLARE_INTERRUPT_HANDLER(instruction_breakpoint_exception);
DECLARE_INTERRUPT_HANDLER(RunModeException);
DECLARE_INTERRUPT_HANDLER(single_step_exception);
DECLARE_INTERRUPT_HANDLER(program_check_exception);
DECLARE_INTERRUPT_HANDLER(emulation_assist_interrupt);
DECLARE_INTERRUPT_HANDLER(alignment_exception);
DECLARE_INTERRUPT_HANDLER(StackOverflow);
DECLARE_INTERRUPT_HANDLER(stack_overflow_exception);
DECLARE_INTERRUPT_HANDLER(kernel_fp_unavailable_exception);
DECLARE_INTERRUPT_HANDLER(altivec_unavailable_exception);
DECLARE_INTERRUPT_HANDLER(vsx_unavailable_exception);
DECLARE_INTERRUPT_HANDLER(facility_unavailable_exception);
DECLARE_INTERRUPT_HANDLER(fp_unavailable_tm);
DECLARE_INTERRUPT_HANDLER(altivec_unavailable_tm);
DECLARE_INTERRUPT_HANDLER(vsx_unavailable_tm);
DECLARE_INTERRUPT_HANDLER_NMI(performance_monitor_exception_nmi);
DECLARE_INTERRUPT_HANDLER_ASYNC(performance_monitor_exception_async);
DECLARE_INTERRUPT_HANDLER_RAW(performance_monitor_exception);
DECLARE_INTERRUPT_HANDLER(DebugException);
DECLARE_INTERRUPT_HANDLER(altivec_assist_exception);
DECLARE_INTERRUPT_HANDLER(CacheLockingException);
DECLARE_INTERRUPT_HANDLER(SPEFloatingPointException);
DECLARE_INTERRUPT_HANDLER(SPEFloatingPointRoundException);
DECLARE_INTERRUPT_HANDLER_NMI(WatchdogException);
DECLARE_INTERRUPT_HANDLER(kernel_bad_stack);
DECLARE_INTERRUPT_HANDLER_RAW(do_slb_fault);
DECLARE_INTERRUPT_HANDLER(do_bad_segment_interrupt);
DECLARE_INTERRUPT_HANDLER(do_hash_fault);
DECLARE_INTERRUPT_HANDLER(do_page_fault);
DECLARE_INTERRUPT_HANDLER(do_bad_page_fault_segv);
DECLARE_INTERRUPT_HANDLER(do_break);
DECLARE_INTERRUPT_HANDLER_ASYNC(timer_interrupt);
DECLARE_INTERRUPT_HANDLER_NMI(machine_check_early);
DECLARE_INTERRUPT_HANDLER_NMI(hmi_exception_realmode);
DECLARE_INTERRUPT_HANDLER_ASYNC(TAUException);
DECLARE_INTERRUPT_HANDLER_ASYNC(do_IRQ);
void __noreturn unrecoverable_exception(struct pt_regs *regs);
void replay_system_reset(void);
void replay_soft_interrupts(void);
static inline void interrupt_cond_local_irq_enable(struct pt_regs *regs)
{
	if (!arch_irq_disabled_regs(regs))
		local_irq_enable();
}
long system_call_exception(struct pt_regs *regs, unsigned long r0);
notrace unsigned long syscall_exit_prepare(unsigned long r3, struct pt_regs *regs, long scv);
notrace unsigned long interrupt_exit_user_prepare(struct pt_regs *regs);
notrace unsigned long interrupt_exit_kernel_prepare(struct pt_regs *regs);
#ifdef CONFIG_PPC64
unsigned long syscall_exit_restart(unsigned long r3, struct pt_regs *regs);
unsigned long interrupt_exit_user_restart(struct pt_regs *regs);
unsigned long interrupt_exit_kernel_restart(struct pt_regs *regs);
#endif
#endif  
#endif  
