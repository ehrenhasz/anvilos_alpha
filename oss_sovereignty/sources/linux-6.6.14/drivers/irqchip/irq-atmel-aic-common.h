

#ifndef __IRQ_ATMEL_AIC_COMMON_H
#define __IRQ_ATMEL_AIC_COMMON_H


int aic_common_set_type(struct irq_data *d, unsigned type, unsigned *val);

void aic_common_set_priority(int priority, unsigned *val);

int aic_common_irq_domain_xlate(struct irq_domain *d,
				struct device_node *ctrlr,
				const u32 *intspec,
				unsigned int intsize,
				irq_hw_number_t *out_hwirq,
				unsigned int *out_type);

struct irq_domain *__init aic_common_of_init(struct device_node *node,
					     const struct irq_domain_ops *ops,
					     const char *name, int nirqs,
					     const struct of_device_id *matches);

void __init aic_common_rtc_irq_fixup(void);

void __init aic_common_rtt_irq_fixup(void);

#endif 
