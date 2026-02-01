 
 

#ifndef _SELINUX_CONDITIONAL_H_
#define _SELINUX_CONDITIONAL_H_

#include "security.h"

int security_get_bools(struct selinux_policy *policy,
		       u32 *len, char ***names, int **values);

int security_set_bools(u32 len, int *values);

int security_get_bool_value(u32 index);

#endif
