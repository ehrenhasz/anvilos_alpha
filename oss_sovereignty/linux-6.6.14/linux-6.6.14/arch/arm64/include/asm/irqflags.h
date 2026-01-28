#ifndef __ASM_IRQFLAGS_H
#define __ASM_IRQFLAGS_H
#include <asm/alternative.h>
#include <asm/barrier.h>
#include <asm/ptrace.h>
#include <asm/sysreg.h>
static __always_inline bool __irqflags_uses_pmr(void)
{
	return IS_ENABLED(CONFIG_ARM64_PSEUDO_NMI) &&
	       alternative_has_cap_unlikely(ARM64_HAS_GIC_PRIO_MASKING);
}
static __always_inline void __daif_local_irq_enable(void)
{
	barrier();
	asm volatile("msr daifclr, #3");
	barrier();
}
static __always_inline void __pmr_local_irq_enable(void)
{
	if (IS_ENABLED(CONFIG_ARM64_DEBUG_PRIORITY_MASKING)) {
		u32 pmr = read_sysreg_s(SYS_ICC_PMR_EL1);
		WARN_ON_ONCE(pmr != GIC_PRIO_IRQON && pmr != GIC_PRIO_IRQOFF);
	}
	barrier();
	write_sysreg_s(GIC_PRIO_IRQON, SYS_ICC_PMR_EL1);
	pmr_sync();
	barrier();
}
static inline void arch_local_irq_enable(void)
{
	if (__irqflags_uses_pmr()) {
		__pmr_local_irq_enable();
	} else {
		__daif_local_irq_enable();
	}
}
static __always_inline void __daif_local_irq_disable(void)
{
	barrier();
	asm volatile("msr daifset, #3");
	barrier();
}
static __always_inline void __pmr_local_irq_disable(void)
{
	if (IS_ENABLED(CONFIG_ARM64_DEBUG_PRIORITY_MASKING)) {
		u32 pmr = read_sysreg_s(SYS_ICC_PMR_EL1);
		WARN_ON_ONCE(pmr != GIC_PRIO_IRQON && pmr != GIC_PRIO_IRQOFF);
	}
	barrier();
	write_sysreg_s(GIC_PRIO_IRQOFF, SYS_ICC_PMR_EL1);
	barrier();
}
static inline void arch_local_irq_disable(void)
{
	if (__irqflags_uses_pmr()) {
		__pmr_local_irq_disable();
	} else {
		__daif_local_irq_disable();
	}
}
static __always_inline unsigned long __daif_local_save_flags(void)
{
	return read_sysreg(daif);
}
static __always_inline unsigned long __pmr_local_save_flags(void)
{
	return read_sysreg_s(SYS_ICC_PMR_EL1);
}
static inline unsigned long arch_local_save_flags(void)
{
	if (__irqflags_uses_pmr()) {
		return __pmr_local_save_flags();
	} else {
		return __daif_local_save_flags();
	}
}
static __always_inline bool __daif_irqs_disabled_flags(unsigned long flags)
{
	return flags & PSR_I_BIT;
}
static __always_inline bool __pmr_irqs_disabled_flags(unsigned long flags)
{
	return flags != GIC_PRIO_IRQON;
}
static inline bool arch_irqs_disabled_flags(unsigned long flags)
{
	if (__irqflags_uses_pmr()) {
		return __pmr_irqs_disabled_flags(flags);
	} else {
		return __daif_irqs_disabled_flags(flags);
	}
}
static __always_inline bool __daif_irqs_disabled(void)
{
	return __daif_irqs_disabled_flags(__daif_local_save_flags());
}
static __always_inline bool __pmr_irqs_disabled(void)
{
	return __pmr_irqs_disabled_flags(__pmr_local_save_flags());
}
static inline bool arch_irqs_disabled(void)
{
	if (__irqflags_uses_pmr()) {
		return __pmr_irqs_disabled();
	} else {
		return __daif_irqs_disabled();
	}
}
static __always_inline unsigned long __daif_local_irq_save(void)
{
	unsigned long flags = __daif_local_save_flags();
	__daif_local_irq_disable();
	return flags;
}
static __always_inline unsigned long __pmr_local_irq_save(void)
{
	unsigned long flags = __pmr_local_save_flags();
	if (!__pmr_irqs_disabled_flags(flags))
		__pmr_local_irq_disable();
	return flags;
}
static inline unsigned long arch_local_irq_save(void)
{
	if (__irqflags_uses_pmr()) {
		return __pmr_local_irq_save();
	} else {
		return __daif_local_irq_save();
	}
}
static __always_inline void __daif_local_irq_restore(unsigned long flags)
{
	barrier();
	write_sysreg(flags, daif);
	barrier();
}
static __always_inline void __pmr_local_irq_restore(unsigned long flags)
{
	barrier();
	write_sysreg_s(flags, SYS_ICC_PMR_EL1);
	pmr_sync();
	barrier();
}
static inline void arch_local_irq_restore(unsigned long flags)
{
	if (__irqflags_uses_pmr()) {
		__pmr_local_irq_restore(flags);
	} else {
		__daif_local_irq_restore(flags);
	}
}
#endif  
