#ifndef __ASM_SN_IRQ_ALLOC_H
#define __ASM_SN_IRQ_ALLOC_H
struct irq_alloc_info {
	void *ctrl;
	nasid_t nasid;
	int pin;
};
#endif  
