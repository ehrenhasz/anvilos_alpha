#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/callb.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/kobj.h>
#include <sys/systm.h>	 
#include <sys/taskq.h>   
#include <sys/kernel.h>
#define	CB_MAXNAME	TASKQ_NAMELEN
typedef struct callb {
	struct callb	*c_next; 	 
	kthread_id_t	c_thread;	 
	char		c_flag;		 
	uchar_t		c_class;	 
	kcondvar_t	c_done_cv;	 
	boolean_t	(*c_func)(void *, int);
	void		*c_arg;		 
	char		c_name[CB_MAXNAME+1];  
} callb_t;
#define	CALLB_FREE		0x0
#define	CALLB_TAKEN		0x1
#define	CALLB_EXECUTING		0x2
typedef struct callb_table {
	kmutex_t ct_lock;		 
	callb_t	*ct_freelist; 		 
	boolean_t ct_busy;		 
	kcondvar_t ct_busy_cv;		 
	int	ct_ncallb; 		 
	callb_t	*ct_first_cb[NCBCLASS];	 
} callb_table_t;
int callb_timeout_sec = CPR_KTHREAD_TIMEOUT_SEC;
static callb_id_t callb_add_common(boolean_t (*)(void *, int),
    void *, int, char *, kthread_id_t);
static callb_table_t callb_table;	 
static callb_table_t *ct = &callb_table;
static kmutex_t	callb_safe_mutex;
callb_cpr_t	callb_cprinfo_safe = {
	&callb_safe_mutex, CALLB_CPR_ALWAYS_SAFE, 0, {0, 0} };
static void
callb_init(void *dummy __unused)
{
	callb_table.ct_busy = B_FALSE;	 
	mutex_init(&callb_safe_mutex, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&callb_table.ct_lock, NULL, MUTEX_DEFAULT, NULL);
}
static void
callb_fini(void *dummy __unused)
{
	callb_t *cp;
	int i;
	mutex_enter(&ct->ct_lock);
	for (i = 0; i < 16; i++) {
		while ((cp = ct->ct_freelist) != NULL) {
			ct->ct_freelist = cp->c_next;
			ct->ct_ncallb--;
			kmem_free(cp, sizeof (callb_t));
		}
		if (ct->ct_ncallb == 0)
			break;
		mutex_exit(&ct->ct_lock);
		tsleep(ct, 0, "callb", hz / 4);
		mutex_enter(&ct->ct_lock);
	}
	if (ct->ct_ncallb > 0)
		printf("%s: Leaked %d callbacks!\n", __func__, ct->ct_ncallb);
	mutex_exit(&ct->ct_lock);
	mutex_destroy(&callb_safe_mutex);
	mutex_destroy(&callb_table.ct_lock);
}
static callb_id_t
callb_add_common(boolean_t (*func)(void *arg, int code),
    void *arg, int class, char *name, kthread_id_t t)
{
	callb_t *cp;
	ASSERT3S(class, <, NCBCLASS);
	mutex_enter(&ct->ct_lock);
	while (ct->ct_busy)
		cv_wait(&ct->ct_busy_cv, &ct->ct_lock);
	if ((cp = ct->ct_freelist) == NULL) {
		ct->ct_ncallb++;
		cp = kmem_zalloc(sizeof (callb_t), KM_SLEEP);
	}
	ct->ct_freelist = cp->c_next;
	cp->c_thread = t;
	cp->c_func = func;
	cp->c_arg = arg;
	cp->c_class = (uchar_t)class;
	cp->c_flag |= CALLB_TAKEN;
#ifdef ZFS_DEBUG
	if (strlen(name) > CB_MAXNAME)
		cmn_err(CE_WARN, "callb_add: name of callback function '%s' "
		    "too long -- truncated to %d chars",
		    name, CB_MAXNAME);
#endif
	(void) strlcpy(cp->c_name, name, sizeof (cp->c_name));
	cp->c_next = ct->ct_first_cb[class];
	ct->ct_first_cb[class] = cp;
	mutex_exit(&ct->ct_lock);
	return ((callb_id_t)cp);
}
callb_id_t
callb_add(boolean_t (*func)(void *arg, int code),
    void *arg, int class, char *name)
{
	return (callb_add_common(func, arg, class, name, curthread));
}
callb_id_t
callb_add_thread(boolean_t (*func)(void *arg, int code),
    void *arg, int class, char *name, kthread_id_t t)
{
	return (callb_add_common(func, arg, class, name, t));
}
int
callb_delete(callb_id_t id)
{
	callb_t **pp;
	callb_t *me = (callb_t *)id;
	mutex_enter(&ct->ct_lock);
	for (;;) {
		pp = &ct->ct_first_cb[me->c_class];
		while (*pp != NULL && *pp != me)
			pp = &(*pp)->c_next;
#ifdef ZFS_DEBUG
		if (*pp != me) {
			cmn_err(CE_WARN, "callb delete bogus entry 0x%p",
			    (void *)me);
			mutex_exit(&ct->ct_lock);
			return (-1);
		}
#endif  
		if (!(me->c_flag & CALLB_EXECUTING))
			break;
		cv_wait(&me->c_done_cv, &ct->ct_lock);
	}
	*pp = me->c_next;
	me->c_flag = CALLB_FREE;
	me->c_next = ct->ct_freelist;
	ct->ct_freelist = me;
	mutex_exit(&ct->ct_lock);
	return (0);
}
void *
callb_execute_class(int class, int code)
{
	callb_t *cp;
	void *ret = NULL;
	ASSERT3S(class, <, NCBCLASS);
	mutex_enter(&ct->ct_lock);
	for (cp = ct->ct_first_cb[class];
	    cp != NULL && ret == NULL; cp = cp->c_next) {
		while (cp->c_flag & CALLB_EXECUTING)
			cv_wait(&cp->c_done_cv, &ct->ct_lock);
		if (cp->c_flag == CALLB_FREE)
			continue;
		cp->c_flag |= CALLB_EXECUTING;
#ifdef CALLB_DEBUG
		printf("callb_execute: name=%s func=%p arg=%p\n",
		    cp->c_name, (void *)cp->c_func, (void *)cp->c_arg);
#endif  
		mutex_exit(&ct->ct_lock);
		if (!(*cp->c_func)(cp->c_arg, code))
			ret = cp->c_name;
		mutex_enter(&ct->ct_lock);
		cp->c_flag &= ~CALLB_EXECUTING;
		cv_broadcast(&cp->c_done_cv);
	}
	mutex_exit(&ct->ct_lock);
	return (ret);
}
boolean_t
callb_generic_cpr(void *arg, int code)
{
	callb_cpr_t *cp = (callb_cpr_t *)arg;
	clock_t ret = 0;			 
	mutex_enter(cp->cc_lockp);
	switch (code) {
	case CB_CODE_CPR_CHKPT:
		cp->cc_events |= CALLB_CPR_START;
#ifdef CPR_NOT_THREAD_SAFE
		while (!(cp->cc_events & CALLB_CPR_SAFE))
			if ((ret = cv_reltimedwait(&cp->cc_callb_cv,
			    cp->cc_lockp, (callb_timeout_sec * hz),
			    TR_CLOCK_TICK)) == -1)
				break;
#endif
		break;
	case CB_CODE_CPR_RESUME:
		cp->cc_events &= ~CALLB_CPR_START;
		cv_signal(&cp->cc_stop_cv);
		break;
	}
	mutex_exit(cp->cc_lockp);
	return (ret != -1);
}
boolean_t
callb_generic_cpr_safe(void *arg, int code)
{
	(void) arg, (void) code;
	return (B_TRUE);
}
void
callb_lock_table(void)
{
	mutex_enter(&ct->ct_lock);
	ASSERT(!ct->ct_busy);
	ct->ct_busy = B_TRUE;
	mutex_exit(&ct->ct_lock);
}
void
callb_unlock_table(void)
{
	mutex_enter(&ct->ct_lock);
	ASSERT(ct->ct_busy);
	ct->ct_busy = B_FALSE;
	cv_broadcast(&ct->ct_busy_cv);
	mutex_exit(&ct->ct_lock);
}
SYSINIT(sol_callb, SI_SUB_DRIVERS, SI_ORDER_FIRST, callb_init, NULL);
SYSUNINIT(sol_callb, SI_SUB_DRIVERS, SI_ORDER_FIRST, callb_fini, NULL);
