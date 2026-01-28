#ifndef __AA_IPC_H
#define __AA_IPC_H
#include <linux/sched.h>
int aa_may_signal(const struct cred *subj_cred, struct aa_label *sender,
		  const struct cred *target_cred, struct aa_label *target,
		  int sig);
#endif  
