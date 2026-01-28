#ifndef _SPARC64_IRQ_H
#define _SPARC64_IRQ_H
#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <asm/pil.h>
#include <asm/ptrace.h>
#define IMAP_VALID		0x80000000UL	 
#define IMAP_TID_UPA		0x7c000000UL	 
#define IMAP_TID_JBUS		0x7c000000UL	 
#define IMAP_TID_SHIFT		26
#define IMAP_AID_SAFARI		0x7c000000UL	 
#define IMAP_AID_SHIFT		26
#define IMAP_NID_SAFARI		0x03e00000UL	 
#define IMAP_NID_SHIFT		21
#define IMAP_IGN		0x000007c0UL	 
#define IMAP_INO		0x0000003fUL	 
#define IMAP_INR		0x000007ffUL	 
#define ICLR_IDLE		0x00000000UL	 
#define ICLR_TRANSMIT		0x00000001UL	 
#define ICLR_PENDING		0x00000003UL	 
#define NR_IRQS		(2048)
void irq_install_pre_handler(int irq,
			     void (*func)(unsigned int, void *, void *),
			     void *arg1, void *arg2);
#define irq_canonicalize(irq)	(irq)
unsigned int build_irq(int inofixup, unsigned long iclr, unsigned long imap);
unsigned int sun4v_build_irq(u32 devhandle, unsigned int devino);
unsigned int sun4v_build_virq(u32 devhandle, unsigned int devino);
unsigned int sun4v_build_msi(u32 devhandle, unsigned int *irq_p,
			     unsigned int msi_devino_start,
			     unsigned int msi_devino_end);
void sun4v_destroy_msi(unsigned int irq);
unsigned int sun4u_build_msi(u32 portid, unsigned int *irq_p,
			     unsigned int msi_devino_start,
			     unsigned int msi_devino_end,
			     unsigned long imap_base,
			     unsigned long iclr_base);
void sun4u_destroy_msi(unsigned int irq);
unsigned int irq_alloc(unsigned int dev_handle, unsigned int dev_ino);
void irq_free(unsigned int irq);
void fixup_irqs(void);
static inline void set_softint(unsigned long bits)
{
	__asm__ __volatile__("wr	%0, 0x0, %%set_softint"
			     :  
			     : "r" (bits));
}
static inline void clear_softint(unsigned long bits)
{
	__asm__ __volatile__("wr	%0, 0x0, %%clear_softint"
			     :  
			     : "r" (bits));
}
static inline unsigned long get_softint(void)
{
	unsigned long retval;
	__asm__ __volatile__("rd	%%softint, %0"
			     : "=r" (retval));
	return retval;
}
void arch_trigger_cpumask_backtrace(const struct cpumask *mask,
				    int exclude_cpu);
#define arch_trigger_cpumask_backtrace arch_trigger_cpumask_backtrace
extern void *hardirq_stack[NR_CPUS];
extern void *softirq_stack[NR_CPUS];
#define NO_IRQ		0xffffffff
#endif
