 

#ifndef _SPL_PROC_H
#define	_SPL_PROC_H

#include <linux/proc_fs.h>
#include <linux/sched.h>

extern struct proc_dir_entry *proc_spl_kstat;

int spl_proc_init(void);
void spl_proc_fini(void);

static inline boolean_t
zfs_proc_is_caller(struct task_struct *t)
{
	return (t->group_leader == current->group_leader);
}

#endif  
