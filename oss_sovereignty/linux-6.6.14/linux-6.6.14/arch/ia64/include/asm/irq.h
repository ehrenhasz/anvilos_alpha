#ifndef _ASM_IA64_IRQ_H
#define _ASM_IA64_IRQ_H
#include <linux/types.h>
#include <linux/cpumask.h>
#include <asm/native/irq.h>
#define NR_IRQS		IA64_NATIVE_NR_IRQS
static __inline__ int
irq_canonicalize (int irq)
{
	return ((irq == 2) ? 9 : irq);
}
extern void set_irq_affinity_info (unsigned int irq, int dest, int redir);
int create_irq(void);
void destroy_irq(unsigned int irq);
#endif  
