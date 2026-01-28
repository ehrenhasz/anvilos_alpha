


#ifndef __IRQ_REMAPPING_H
#define __IRQ_REMAPPING_H

#ifdef CONFIG_IRQ_REMAP

struct irq_data;
struct msi_msg;
struct irq_domain;
struct irq_alloc_info;

extern int irq_remap_broken;
extern int disable_sourceid_checking;
extern int no_x2apic_optout;
extern int irq_remapping_enabled;

extern int disable_irq_post;

struct irq_remap_ops {
	
	int capability;

	
	int  (*prepare)(void);

	
	int  (*enable)(void);

	
	void (*disable)(void);

	
	int  (*reenable)(int);

	
	int  (*enable_faulting)(void);
};

extern struct irq_remap_ops intel_irq_remap_ops;
extern struct irq_remap_ops amd_iommu_irq_ops;
extern struct irq_remap_ops hyperv_irq_remap_ops;

#else  

#define irq_remapping_enabled 0
#define irq_remap_broken      0
#define disable_irq_post      1

#endif 

#endif 
