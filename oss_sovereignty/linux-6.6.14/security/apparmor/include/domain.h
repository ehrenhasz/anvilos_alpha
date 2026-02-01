 
 

#include <linux/binfmts.h>
#include <linux/types.h>

#include "label.h"

#ifndef __AA_DOMAIN_H
#define __AA_DOMAIN_H

#define AA_CHANGE_NOFLAGS 0
#define AA_CHANGE_TEST 1
#define AA_CHANGE_CHILD 2
#define AA_CHANGE_ONEXEC  4
#define AA_CHANGE_STACK 8

struct aa_label *x_table_lookup(struct aa_profile *profile, u32 xindex,
				const char **name);

int apparmor_bprm_creds_for_exec(struct linux_binprm *bprm);

int aa_change_hat(const char *hats[], int count, u64 token, int flags);
int aa_change_profile(const char *fqname, int flags);

#endif  
