#ifdef CONFIG_PPC_PERF_CTRS
#include <asm/perf_event_server.h>
#else
static inline bool is_sier_available(void) { return false; }
static inline unsigned long get_pmcs_ext_regs(int idx) { return 0; }
#endif
#ifdef CONFIG_FSL_EMB_PERF_EVENT
#include <asm/perf_event_fsl_emb.h>
#endif
#ifdef CONFIG_PERF_EVENTS
#include <asm/ptrace.h>
#include <asm/reg.h>
#define perf_arch_bpf_user_pt_regs(regs) &regs->user_regs
#define perf_arch_fetch_caller_regs(regs, __ip)			\
	do {							\
		(regs)->result = 0;				\
		(regs)->nip = __ip;				\
		(regs)->gpr[1] = current_stack_frame();		\
		asm volatile("mfmsr %0" : "=r" ((regs)->msr));	\
	} while (0)
extern bool is_sier_available(void);
extern unsigned long get_pmcs_ext_regs(int idx);
extern u64 PERF_REG_EXTENDED_MASK;
#define PERF_REG_EXTENDED_MASK	PERF_REG_EXTENDED_MASK
#endif
