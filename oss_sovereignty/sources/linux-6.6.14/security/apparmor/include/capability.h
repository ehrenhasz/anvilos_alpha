


#ifndef __AA_CAPABILITY_H
#define __AA_CAPABILITY_H

#include <linux/sched.h>

#include "apparmorfs.h"

struct aa_label;


struct aa_caps {
	kernel_cap_t allow;
	kernel_cap_t audit;
	kernel_cap_t denied;
	kernel_cap_t quiet;
	kernel_cap_t kill;
	kernel_cap_t extended;
};

extern struct aa_sfs_entry aa_sfs_entry_caps[];

int aa_capable(const struct cred *subj_cred, struct aa_label *label,
	       int cap, unsigned int opts);

static inline void aa_free_cap_rules(struct aa_caps *caps)
{
	
}

#endif 
