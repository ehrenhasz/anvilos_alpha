#ifndef __AA_RESOURCE_H
#define __AA_RESOURCE_H
#include <linux/resource.h>
#include <linux/sched.h>
#include "apparmorfs.h"
struct aa_profile;
struct aa_rlimit {
	unsigned int mask;
	struct rlimit limits[RLIM_NLIMITS];
};
extern struct aa_sfs_entry aa_sfs_entry_rlimit[];
int aa_map_resource(int resource);
int aa_task_setrlimit(const struct cred *subj_cred, struct aa_label *label,
		      struct task_struct *task,
		      unsigned int resource, struct rlimit *new_rlim);
void __aa_transition_rlimits(struct aa_label *old, struct aa_label *new);
static inline void aa_free_rlimit_rules(struct aa_rlimit *rlims)
{
}
#endif  
