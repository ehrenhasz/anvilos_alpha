#ifndef __ASM_HARDIRQ_H
#define __ASM_HARDIRQ_H
#include <linux/cache.h>
#include <linux/percpu.h>
#include <linux/threads.h>
#include <asm/barrier.h>
#include <asm/irq.h>
#include <asm/kvm_arm.h>
#include <asm/sysreg.h>
#define ack_bad_irq ack_bad_irq
#include <asm-generic/hardirq.h>
#define __ARCH_IRQ_EXIT_IRQS_DISABLED	1
struct nmi_ctx {
	u64 hcr;
	unsigned int cnt;
};
DECLARE_PER_CPU(struct nmi_ctx, nmi_contexts);
#define arch_nmi_enter()						\
do {									\
	struct nmi_ctx *___ctx;						\
	u64 ___hcr;							\
									\
	if (!is_kernel_in_hyp_mode())					\
		break;							\
									\
	___ctx = this_cpu_ptr(&nmi_contexts);				\
	if (___ctx->cnt) {						\
		___ctx->cnt++;						\
		break;							\
	}								\
									\
	___hcr = read_sysreg(hcr_el2);					\
	if (!(___hcr & HCR_TGE)) {					\
		write_sysreg(___hcr | HCR_TGE, hcr_el2);		\
		isb();							\
	}								\
	 								\
	barrier();							\
	___ctx->cnt = 1;                                                \
	 								\
	barrier();							\
	___ctx->hcr = ___hcr;						\
} while (0)
#define arch_nmi_exit()							\
do {									\
	struct nmi_ctx *___ctx;						\
	u64 ___hcr;							\
									\
	if (!is_kernel_in_hyp_mode())					\
		break;							\
									\
	___ctx = this_cpu_ptr(&nmi_contexts);				\
	___hcr = ___ctx->hcr;						\
	 								\
	barrier();							\
	___ctx->cnt--;							\
	 								\
	barrier();							\
	if (!___ctx->cnt && !(___hcr & HCR_TGE))			\
		write_sysreg(___hcr, hcr_el2);				\
} while (0)
static inline void ack_bad_irq(unsigned int irq)
{
	extern unsigned long irq_err_count;
	irq_err_count++;
}
#endif  
