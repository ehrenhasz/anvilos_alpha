


#ifndef _IPA_MODEM_H_
#define _IPA_MODEM_H_

struct ipa;
struct net_device;
struct sk_buff;

int ipa_modem_start(struct ipa *ipa);
int ipa_modem_stop(struct ipa *ipa);

void ipa_modem_skb_rx(struct net_device *netdev, struct sk_buff *skb);

void ipa_modem_suspend(struct net_device *netdev);
void ipa_modem_resume(struct net_device *netdev);

int ipa_modem_config(struct ipa *ipa);
void ipa_modem_deconfig(struct ipa *ipa);

#endif 
