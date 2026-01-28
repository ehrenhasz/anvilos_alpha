#ifndef _ASM_I8259_H
#define _ASM_I8259_H
#include <linux/compiler.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <irq.h>
#define PIC_MASTER_CMD		0x20
#define PIC_MASTER_IMR		0x21
#define PIC_MASTER_ISR		PIC_MASTER_CMD
#define PIC_MASTER_POLL		PIC_MASTER_ISR
#define PIC_MASTER_OCW3		PIC_MASTER_ISR
#define PIC_SLAVE_CMD		0xa0
#define PIC_SLAVE_IMR		0xa1
#define PIC_CASCADE_IR		2
#define MASTER_ICW4_DEFAULT	0x01
#define SLAVE_ICW4_DEFAULT	0x01
#define PIC_ICW4_AEOI		2
extern raw_spinlock_t i8259A_lock;
extern void make_8259A_irq(unsigned int irq);
extern void init_i8259_irqs(void);
extern struct irq_domain *__init_i8259_irqs(struct device_node *node);
extern void i8259_set_poll(int (*poll)(void));
static inline int i8259_irq(void)
{
	int irq;
	raw_spin_lock(&i8259A_lock);
	outb(0x0C, PIC_MASTER_CMD);		 
	irq = inb(PIC_MASTER_CMD) & 7;
	if (irq == PIC_CASCADE_IR) {
		outb(0x0C, PIC_SLAVE_CMD);		 
		irq = (inb(PIC_SLAVE_CMD) & 7) + 8;
	}
	if (unlikely(irq == 7)) {
		outb(0x0B, PIC_MASTER_ISR);		 
		if(~inb(PIC_MASTER_ISR) & 0x80)
			irq = -1;
	}
	raw_spin_unlock(&i8259A_lock);
	return likely(irq >= 0) ? irq + I8259A_IRQ_BASE : irq;
}
#endif  
