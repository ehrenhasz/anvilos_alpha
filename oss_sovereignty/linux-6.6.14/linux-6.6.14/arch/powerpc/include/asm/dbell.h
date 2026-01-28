#ifndef _ASM_POWERPC_DBELL_H
#define _ASM_POWERPC_DBELL_H
#include <linux/smp.h>
#include <linux/threads.h>
#include <asm/cputhreads.h>
#include <asm/ppc-opcode.h>
#include <asm/feature-fixups.h>
#include <asm/kvm_ppc.h>
#define PPC_DBELL_MSG_BRDCAST	(0x04000000)
#define PPC_DBELL_TYPE(x)	(((x) & 0xf) << (63-36))
#define PPC_DBELL_TYPE_MASK	PPC_DBELL_TYPE(0xf)
#define PPC_DBELL_LPID(x)	((x) << (63 - 49))
#define PPC_DBELL_PIR_MASK	0x3fff
enum ppc_dbell {
	PPC_DBELL = 0,		 
	PPC_DBELL_CRIT = 1,	 
	PPC_G_DBELL = 2,	 
	PPC_G_DBELL_CRIT = 3,	 
	PPC_G_DBELL_MC = 4,	 
	PPC_DBELL_SERVER = 5,	 
};
#ifdef CONFIG_PPC_BOOK3S
#define PPC_DBELL_MSGTYPE		PPC_DBELL_SERVER
static inline void _ppc_msgsnd(u32 msg)
{
	__asm__ __volatile__ (ASM_FTR_IFSET(PPC_MSGSND(%1), PPC_MSGSNDP(%1), %0)
				: : "i" (CPU_FTR_HVMODE), "r" (msg));
}
static inline void ppc_msgsnd_sync(void)
{
	__asm__ __volatile__ ("sync" : : : "memory");
}
static inline void ppc_msgsync(void)
{
	__asm__ __volatile__ (ASM_FTR_IFSET(PPC_MSGSYNC " ; lwsync", "", %0)
				: : "i" (CPU_FTR_HVMODE|CPU_FTR_ARCH_300));
}
static inline void _ppc_msgclr(u32 msg)
{
	__asm__ __volatile__ (ASM_FTR_IFSET(PPC_MSGCLR(%1), PPC_MSGCLRP(%1), %0)
				: : "i" (CPU_FTR_HVMODE), "r" (msg));
}
static inline void ppc_msgclr(enum ppc_dbell type)
{
	u32 msg = PPC_DBELL_TYPE(type);
	_ppc_msgclr(msg);
}
#else  
#define PPC_DBELL_MSGTYPE		PPC_DBELL
static inline void _ppc_msgsnd(u32 msg)
{
	__asm__ __volatile__ (PPC_MSGSND(%0) : : "r" (msg));
}
static inline void ppc_msgsnd_sync(void)
{
	__asm__ __volatile__ ("sync" : : : "memory");
}
static inline void ppc_msgsync(void)
{
}
#endif  
extern void doorbell_exception(struct pt_regs *regs);
static inline void ppc_msgsnd(enum ppc_dbell type, u32 flags, u32 tag)
{
	u32 msg = PPC_DBELL_TYPE(type) | (flags & PPC_DBELL_MSG_BRDCAST) |
			(tag & 0x07ffffff);
	_ppc_msgsnd(msg);
}
#ifdef CONFIG_SMP
static inline void doorbell_global_ipi(int cpu)
{
	u32 tag = get_hard_smp_processor_id(cpu);
	kvmppc_set_host_ipi(cpu);
	ppc_msgsnd_sync();
	ppc_msgsnd(PPC_DBELL_MSGTYPE, 0, tag);
}
static inline void doorbell_core_ipi(int cpu)
{
	u32 tag = cpu_thread_in_core(cpu);
	kvmppc_set_host_ipi(cpu);
	ppc_msgsnd_sync();
	ppc_msgsnd(PPC_DBELL_MSGTYPE, 0, tag);
}
static inline int doorbell_try_core_ipi(int cpu)
{
	int this_cpu = get_cpu();
	int ret = 0;
	if (cpumask_test_cpu(cpu, cpu_sibling_mask(this_cpu))) {
		doorbell_core_ipi(cpu);
		ret = 1;
	}
	put_cpu();
	return ret;
}
#endif  
#endif  
