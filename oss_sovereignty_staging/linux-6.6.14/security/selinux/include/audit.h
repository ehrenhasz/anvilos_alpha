 
 

#ifndef _SELINUX_AUDIT_H
#define _SELINUX_AUDIT_H

#include <linux/audit.h>
#include <linux/types.h>

 
int selinux_audit_rule_init(u32 field, u32 op, char *rulestr, void **rule);

 
void selinux_audit_rule_free(void *rule);

 
int selinux_audit_rule_match(u32 sid, u32 field, u32 op, void *rule);

 
int selinux_audit_rule_known(struct audit_krule *rule);

#endif  

