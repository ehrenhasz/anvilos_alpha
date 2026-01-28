


#ifndef _IPA_INTERRUPT_H_
#define _IPA_INTERRUPT_H_

#include <linux/types.h>
#include <linux/bits.h>

struct ipa;
struct ipa_interrupt;
enum ipa_irq_id;


void ipa_interrupt_suspend_enable(struct ipa_interrupt *interrupt,
				  u32 endpoint_id);


void ipa_interrupt_suspend_disable(struct ipa_interrupt *interrupt,
				   u32 endpoint_id);


void ipa_interrupt_suspend_clear_all(struct ipa_interrupt *interrupt);


void ipa_interrupt_simulate_suspend(struct ipa_interrupt *interrupt);


void ipa_interrupt_enable(struct ipa *ipa, enum ipa_irq_id ipa_irq);


void ipa_interrupt_disable(struct ipa *ipa, enum ipa_irq_id ipa_irq);


void ipa_interrupt_irq_enable(struct ipa *ipa);


void ipa_interrupt_irq_disable(struct ipa *ipa);


struct ipa_interrupt *ipa_interrupt_config(struct ipa *ipa);


void ipa_interrupt_deconfig(struct ipa_interrupt *interrupt);

#endif 
