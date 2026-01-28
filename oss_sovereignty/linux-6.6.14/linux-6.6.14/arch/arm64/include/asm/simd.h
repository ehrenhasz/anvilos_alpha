#ifndef __ASM_SIMD_H
#define __ASM_SIMD_H
#include <linux/compiler.h>
#include <linux/irqflags.h>
#include <linux/percpu.h>
#include <linux/preempt.h>
#include <linux/types.h>
DECLARE_PER_CPU(bool, fpsimd_context_busy);
#ifdef CONFIG_KERNEL_MODE_NEON
static __must_check inline bool may_use_simd(void)
{
	return !WARN_ON(!system_capabilities_finalized()) &&
	       system_supports_fpsimd() &&
	       !in_hardirq() && !irqs_disabled() && !in_nmi() &&
	       !this_cpu_read(fpsimd_context_busy);
}
#else  
static __must_check inline bool may_use_simd(void) {
	return false;
}
#endif  
#endif
