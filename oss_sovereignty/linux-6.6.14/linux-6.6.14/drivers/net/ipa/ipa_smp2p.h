#ifndef _IPA_SMP2P_H_
#define _IPA_SMP2P_H_
#include <linux/types.h>
struct ipa;
int ipa_smp2p_init(struct ipa *ipa, bool modem_init);
void ipa_smp2p_exit(struct ipa *ipa);
void ipa_smp2p_irq_disable_setup(struct ipa *ipa);
void ipa_smp2p_notify_reset(struct ipa *ipa);
#endif  
