 

 
#ifndef _IPA_UC_H_
#define _IPA_UC_H_

struct ipa;
enum ipa_irq_id;

 
void ipa_uc_interrupt_handler(struct ipa *ipa, enum ipa_irq_id irq_id);

 
void ipa_uc_config(struct ipa *ipa);

 
void ipa_uc_deconfig(struct ipa *ipa);

 
void ipa_uc_power(struct ipa *ipa);

 
void ipa_uc_panic_notifier(struct ipa *ipa);

#endif  
