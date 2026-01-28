#ifndef _OPENSOLARIS_SYS_PROC_H_
#define	_OPENSOLARIS_SYS_PROC_H_
#include <sys/param.h>
#include <sys/kthread.h>
#include_next <sys/proc.h>
#include <sys/stdint.h>
#include <sys/smp.h>
#include <sys/sched.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/unistd.h>
#include <sys/kmem.h>
#include <sys/malloc.h>
#ifdef _KERNEL
#define	CPU		curcpu
#define	minclsyspri	PRIBIO
#define	defclsyspri minclsyspri
#define	maxclsyspri	PVM
#define	max_ncpus	(mp_maxid + 1)
#define	boot_max_ncpus	(mp_maxid + 1)
#define	TS_RUN	0
#define	p0	proc0
#define	t_tid	td_tid
typedef	short		pri_t;
typedef	struct thread	_kthread;
typedef	struct thread	kthread_t;
typedef struct thread	*kthread_id_t;
typedef struct proc	proc_t;
extern proc_t *system_proc;
static __inline kthread_t *
do_thread_create(caddr_t stk, size_t stksize, void (*proc)(void *), void *arg,
    size_t len, proc_t *pp, int state, pri_t pri, const char *name)
{
	kthread_t *td = NULL;
	proc_t **ppp;
	int error;
	ASSERT(stk == NULL);
	ASSERT(len == 0);
	ASSERT(state == TS_RUN);
	if (pp == &p0)
		ppp = &system_proc;
	else
		ppp = &pp;
	error = kproc_kthread_add(proc, arg, ppp, &td, RFSTOPPED,
	    stksize / PAGE_SIZE, "zfskern", "%s", name);
	if (error == 0) {
		thread_lock(td);
		sched_prio(td, pri);
		sched_add(td, SRQ_BORING);
#if __FreeBSD_version < 1300068
		thread_unlock(td);
#endif
	}
	return (td);
}
#define	thread_create_named(name, stk, stksize, proc, arg, len,	\
    pp, state, pri) \
	do_thread_create(stk, stksize, proc, arg, len, pp, state, pri, name)
#define	thread_create(stk, stksize, proc, arg, len, pp, state, pri) \
	do_thread_create(stk, stksize, proc, arg, len, pp, state, pri, #proc)
#define	thread_exit()	kthread_exit()
int	uread(proc_t *, void *, size_t, uintptr_t);
int	uwrite(proc_t *, void *, size_t, uintptr_t);
static inline boolean_t
zfs_proc_is_caller(proc_t *p)
{
	return (p == curproc);
}
#endif	 
#endif	 
