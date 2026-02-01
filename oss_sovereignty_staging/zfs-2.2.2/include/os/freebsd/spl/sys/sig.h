 

#ifndef _OPENSOLARIS_SYS_SIG_H_
#define	_OPENSOLARIS_SYS_SIG_H_

#ifndef _STANDALONE

#include_next <sys/signal.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/debug.h>

#define	FORREAL		0
#define	JUSTLOOKING	1

static __inline int
issig(int why)
{
	struct thread *td = curthread;
	struct proc *p;
	int sig;

	ASSERT(why == FORREAL || why == JUSTLOOKING);
	if (SIGPENDING(td)) {
		if (why == JUSTLOOKING)
			return (1);
		p = td->td_proc;
		PROC_LOCK(p);
		mtx_lock(&p->p_sigacts->ps_mtx);
		sig = cursig(td);
		mtx_unlock(&p->p_sigacts->ps_mtx);
		PROC_UNLOCK(p);
		if (sig != 0)
			return (1);
	}
	return (0);
}

#endif  

#endif	 
