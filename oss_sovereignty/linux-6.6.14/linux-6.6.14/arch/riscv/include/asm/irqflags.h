#ifndef _ASM_RISCV_IRQFLAGS_H
#define _ASM_RISCV_IRQFLAGS_H
#include <asm/processor.h>
#include <asm/csr.h>
static inline unsigned long arch_local_save_flags(void)
{
	return csr_read(CSR_STATUS);
}
static inline void arch_local_irq_enable(void)
{
	csr_set(CSR_STATUS, SR_IE);
}
static inline void arch_local_irq_disable(void)
{
	csr_clear(CSR_STATUS, SR_IE);
}
static inline unsigned long arch_local_irq_save(void)
{
	return csr_read_clear(CSR_STATUS, SR_IE);
}
static inline int arch_irqs_disabled_flags(unsigned long flags)
{
	return !(flags & SR_IE);
}
static inline int arch_irqs_disabled(void)
{
	return arch_irqs_disabled_flags(arch_local_save_flags());
}
static inline void arch_local_irq_restore(unsigned long flags)
{
	csr_set(CSR_STATUS, flags & SR_IE);
}
#endif  
