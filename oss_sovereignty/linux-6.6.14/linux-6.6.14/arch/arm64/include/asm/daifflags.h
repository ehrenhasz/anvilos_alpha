#ifndef __ASM_DAIFFLAGS_H
#define __ASM_DAIFFLAGS_H
#include <linux/irqflags.h>
#include <asm/arch_gicv3.h>
#include <asm/barrier.h>
#include <asm/cpufeature.h>
#include <asm/ptrace.h>
#define DAIF_PROCCTX		0
#define DAIF_PROCCTX_NOIRQ	(PSR_I_BIT | PSR_F_BIT)
#define DAIF_ERRCTX		(PSR_A_BIT | PSR_I_BIT | PSR_F_BIT)
#define DAIF_MASK		(PSR_D_BIT | PSR_A_BIT | PSR_I_BIT | PSR_F_BIT)
static inline void local_daif_mask(void)
{
	WARN_ON(system_has_prio_mask_debugging() &&
		(read_sysreg_s(SYS_ICC_PMR_EL1) == (GIC_PRIO_IRQOFF |
						    GIC_PRIO_PSR_I_SET)));
	asm volatile(
		"msr	daifset, #0xf		// local_daif_mask\n"
		:
		:
		: "memory");
	if (system_uses_irq_prio_masking())
		gic_write_pmr(GIC_PRIO_IRQON | GIC_PRIO_PSR_I_SET);
	trace_hardirqs_off();
}
static inline unsigned long local_daif_save_flags(void)
{
	unsigned long flags;
	flags = read_sysreg(daif);
	if (system_uses_irq_prio_masking()) {
		if (read_sysreg_s(SYS_ICC_PMR_EL1) != GIC_PRIO_IRQON)
			flags |= PSR_I_BIT | PSR_F_BIT;
	}
	return flags;
}
static inline unsigned long local_daif_save(void)
{
	unsigned long flags;
	flags = local_daif_save_flags();
	local_daif_mask();
	return flags;
}
static inline void local_daif_restore(unsigned long flags)
{
	bool irq_disabled = flags & PSR_I_BIT;
	WARN_ON(system_has_prio_mask_debugging() &&
		(read_sysreg(daif) & (PSR_I_BIT | PSR_F_BIT)) != (PSR_I_BIT | PSR_F_BIT));
	if (!irq_disabled) {
		trace_hardirqs_on();
		if (system_uses_irq_prio_masking()) {
			gic_write_pmr(GIC_PRIO_IRQON);
			pmr_sync();
		}
	} else if (system_uses_irq_prio_masking()) {
		u64 pmr;
		if (!(flags & PSR_A_BIT)) {
			flags &= ~(PSR_I_BIT | PSR_F_BIT);
			pmr = GIC_PRIO_IRQOFF;
		} else {
			pmr = GIC_PRIO_IRQON | GIC_PRIO_PSR_I_SET;
		}
		gic_write_pmr(pmr);
	}
	write_sysreg(flags, daif);
	if (irq_disabled)
		trace_hardirqs_off();
}
static inline void local_daif_inherit(struct pt_regs *regs)
{
	unsigned long flags = regs->pstate & DAIF_MASK;
	if (interrupts_enabled(regs))
		trace_hardirqs_on();
	if (system_uses_irq_prio_masking())
		gic_write_pmr(regs->pmr_save);
	write_sysreg(flags, daif);
}
#endif
