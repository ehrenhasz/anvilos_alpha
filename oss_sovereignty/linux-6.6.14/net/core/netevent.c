
 

#include <linux/rtnetlink.h>
#include <linux/notifier.h>
#include <linux/export.h>
#include <net/netevent.h>

static ATOMIC_NOTIFIER_HEAD(netevent_notif_chain);

 
int register_netevent_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&netevent_notif_chain, nb);
}
EXPORT_SYMBOL_GPL(register_netevent_notifier);

 

int unregister_netevent_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&netevent_notif_chain, nb);
}
EXPORT_SYMBOL_GPL(unregister_netevent_notifier);

 

int call_netevent_notifiers(unsigned long val, void *v)
{
	return atomic_notifier_call_chain(&netevent_notif_chain, val, v);
}
EXPORT_SYMBOL_GPL(call_netevent_notifiers);
