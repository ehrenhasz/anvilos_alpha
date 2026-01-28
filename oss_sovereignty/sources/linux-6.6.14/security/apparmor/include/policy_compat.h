


#ifndef __POLICY_COMPAT_H
#define __POLICY_COMPAT_H

#include "policy.h"

#define K_ABI_MASK 0x3ff
#define FORCE_COMPLAIN_FLAG 0x800
#define VERSION_LT(X, Y) (((X) & K_ABI_MASK) < ((Y) & K_ABI_MASK))
#define VERSION_LE(X, Y) (((X) & K_ABI_MASK) <= ((Y) & K_ABI_MASK))
#define VERSION_GT(X, Y) (((X) & K_ABI_MASK) > ((Y) & K_ABI_MASK))

#define v5	5	
#define v6	6	
#define v7	7
#define v8	8	
#define v9	9	

int aa_compat_map_xmatch(struct aa_policydb *policy);
int aa_compat_map_policy(struct aa_policydb *policy, u32 version);
int aa_compat_map_file(struct aa_policydb *policy);

#endif 
