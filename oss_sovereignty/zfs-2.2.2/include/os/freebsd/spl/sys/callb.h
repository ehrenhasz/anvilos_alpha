 
 

#ifndef	_SYS_CALLB_H
#define	_SYS_CALLB_H

#include <sys/condvar.h>

#ifdef	__cplusplus
extern "C" {
#endif

 
#define	CB_CL_CPR_DAEMON	0
#define	CB_CL_CPR_VM		1
#define	CB_CL_CPR_CALLOUT	2
#define	CB_CL_CPR_OBP		3
#define	CB_CL_CPR_FB		4
#define	CB_CL_PANIC		5
#define	CB_CL_CPR_RPC		6
#define	CB_CL_CPR_PROMPRINTF	7
#define	CB_CL_UADMIN		8
#define	CB_CL_CPR_PM		9
#define	CB_CL_HALT		10
#define	CB_CL_CPR_DMA		11
#define	CB_CL_CPR_POST_USER	12
#define	CB_CL_UADMIN_PRE_VFS    13
#define	CB_CL_MDBOOT		CB_CL_UADMIN
#define	CB_CL_ENTER_DEBUGGER	14
#define	CB_CL_CPR_POST_KERNEL	15
#define	CB_CL_CPU_DEEP_IDLE	16
#define	NCBCLASS		17  

 

 
#define	CB_CODE_CPR_CHKPT	0
#define	CB_CODE_CPR_RESUME	1

typedef	void *		callb_id_t;
 
typedef struct callb_cpr {
	kmutex_t	*cc_lockp;	 
	char		cc_events;	 
	callb_id_t	cc_id;		 
	kcondvar_t	cc_callb_cv;	 
	kcondvar_t	cc_stop_cv;	 
} callb_cpr_t;

 
#define	CALLB_CPR_START		1	 
#define	CALLB_CPR_SAFE		2	 
#define	CALLB_CPR_ALWAYS_SAFE	4	 

 
#define	CALLB_MAX_RETRY		3	 
#define	CALLB_THREAD_DELAY	10	 
#define	CPR_KTHREAD_TIMEOUT_SEC	90	 
					 
					 

 
#define	CALLB_CPR_INIT(cp, lockp, func, name)	{			\
		strlcpy(curthread->td_name, (name),			\
		    sizeof (curthread->td_name));			\
		memset(cp, 0, sizeof (callb_cpr_t));		\
		(cp)->cc_lockp = lockp;					\
		(cp)->cc_id = callb_add(func, (void *)(cp),		\
			CB_CL_CPR_DAEMON, name);			\
		cv_init(&(cp)->cc_callb_cv, NULL, CV_DEFAULT, NULL);	\
		cv_init(&(cp)->cc_stop_cv, NULL, CV_DEFAULT, NULL);	\
	}

#ifndef __lock_lint
#define	CALLB_CPR_ASSERT(cp)	ASSERT(MUTEX_HELD((cp)->cc_lockp));
#else
#define	CALLB_CPR_ASSERT(cp)
#endif
 
#define	CALLB_CPR_INIT_SAFE(t, name) {					\
		(void) callb_add_thread(callb_generic_cpr_safe,		\
		(void *) &callb_cprinfo_safe, CB_CL_CPR_DAEMON,		\
		    name, t);						\
	}
 
#define	CALLB_CPR_SAFE_BEGIN(cp) { 			\
		CALLB_CPR_ASSERT(cp)			\
		(cp)->cc_events |= CALLB_CPR_SAFE;	\
		if ((cp)->cc_events & CALLB_CPR_START)	\
			cv_signal(&(cp)->cc_callb_cv);	\
	}
#define	CALLB_CPR_SAFE_END(cp, lockp) {				\
		CALLB_CPR_ASSERT(cp)				\
		while ((cp)->cc_events & CALLB_CPR_START)	\
			cv_wait(&(cp)->cc_stop_cv, lockp);	\
		(cp)->cc_events &= ~CALLB_CPR_SAFE;		\
	}
 
#define	CALLB_CPR_EXIT(cp) {				\
		CALLB_CPR_ASSERT(cp)			\
		(cp)->cc_events |= CALLB_CPR_SAFE;	\
		if ((cp)->cc_events & CALLB_CPR_START)	\
			cv_signal(&(cp)->cc_callb_cv);	\
		mutex_exit((cp)->cc_lockp);		\
		(void) callb_delete((cp)->cc_id);	\
		cv_destroy(&(cp)->cc_callb_cv);		\
		cv_destroy(&(cp)->cc_stop_cv);		\
	}

extern callb_cpr_t callb_cprinfo_safe;
extern callb_id_t callb_add(boolean_t  (*)(void *, int), void *, int, char *);
extern callb_id_t callb_add_thread(boolean_t (*)(void *, int),
    void *, int, char *, kthread_id_t);
extern int	callb_delete(callb_id_t);
extern void	callb_execute(callb_id_t, int);
extern void	*callb_execute_class(int, int);
extern boolean_t callb_generic_cpr(void *, int);
extern boolean_t callb_generic_cpr_safe(void *, int);
extern boolean_t callb_is_stopped(kthread_id_t, caddr_t *);
extern void	callb_lock_table(void);
extern void	callb_unlock_table(void);

#ifdef	__cplusplus
}
#endif

#endif	 
