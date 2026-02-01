 
 

#ifndef _DELL_RBTN_H_
#define _DELL_RBTN_H_

struct notifier_block;

int dell_rbtn_notifier_register(struct notifier_block *nb);
int dell_rbtn_notifier_unregister(struct notifier_block *nb);

#endif
