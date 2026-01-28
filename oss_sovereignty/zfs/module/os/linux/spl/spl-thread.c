#include <sys/thread.h>
#include <sys/kmem.h>
#include <sys/tsd.h>
typedef struct thread_priv_s {
	unsigned long tp_magic;		 
	int tp_name_size;		 
	char *tp_name;			 
	void (*tp_func)(void *);	 
	void *tp_args;			 
	size_t tp_len;			 
	int tp_state;			 
	pri_t tp_pri;			 
} thread_priv_t;
static int
thread_generic_wrapper(void *arg)
{
	thread_priv_t *tp = (thread_priv_t *)arg;
	void (*func)(void *);
	void *args;
	ASSERT(tp->tp_magic == TP_MAGIC);
	func = tp->tp_func;
	args = tp->tp_args;
	set_current_state(tp->tp_state);
	set_user_nice((kthread_t *)current, PRIO_TO_NICE(tp->tp_pri));
	kmem_free(tp->tp_name, tp->tp_name_size);
	kmem_free(tp, sizeof (thread_priv_t));
	if (func)
		func(args);
	return (0);
}
kthread_t *
__thread_create(caddr_t stk, size_t  stksize, thread_func_t func,
    const char *name, void *args, size_t len, proc_t *pp, int state, pri_t pri)
{
	thread_priv_t *tp;
	struct task_struct *tsk;
	char *p;
	ASSERT(stk == NULL);
	tp = kmem_alloc(sizeof (thread_priv_t), KM_PUSHPAGE);
	if (tp == NULL)
		return (NULL);
	tp->tp_magic = TP_MAGIC;
	tp->tp_name_size = strlen(name) + 1;
	tp->tp_name = kmem_alloc(tp->tp_name_size, KM_PUSHPAGE);
	if (tp->tp_name == NULL) {
		kmem_free(tp, sizeof (thread_priv_t));
		return (NULL);
	}
	strlcpy(tp->tp_name, name, tp->tp_name_size);
	p = strstr(tp->tp_name, "_thread");
	if (p)
		p[0] = '\0';
	tp->tp_func  = func;
	tp->tp_args  = args;
	tp->tp_len   = len;
	tp->tp_state = state;
	tp->tp_pri   = pri;
	tsk = spl_kthread_create(thread_generic_wrapper, (void *)tp,
	    "%s", tp->tp_name);
	if (IS_ERR(tsk))
		return (NULL);
	wake_up_process(tsk);
	return ((kthread_t *)tsk);
}
EXPORT_SYMBOL(__thread_create);
struct task_struct *
spl_kthread_create(int (*func)(void *), void *data, const char namefmt[], ...)
{
	struct task_struct *tsk;
	va_list args;
	char name[TASK_COMM_LEN];
	va_start(args, namefmt);
	vsnprintf(name, sizeof (name), namefmt, args);
	va_end(args);
	do {
		tsk = kthread_create(func, data, "%s", name);
		if (IS_ERR(tsk)) {
			if (signal_pending(current)) {
				clear_thread_flag(TIF_SIGPENDING);
				continue;
			}
			if (PTR_ERR(tsk) == -ENOMEM)
				continue;
			return (NULL);
		} else {
			return (tsk);
		}
	} while (1);
}
EXPORT_SYMBOL(spl_kthread_create);
int
issig(int why)
{
	ASSERT(why == FORREAL || why == JUSTLOOKING);
	if (!signal_pending(current))
		return (0);
	if (why != FORREAL)
		return (1);
	struct task_struct *task = current;
	spl_kernel_siginfo_t __info;
	sigset_t set;
	siginitsetinv(&set, 1ULL << (SIGSTOP - 1) | 1ULL << (SIGTSTP - 1));
	sigorsets(&set, &task->blocked, &set);
	spin_lock_irq(&task->sighand->siglock);
#ifdef HAVE_DEQUEUE_SIGNAL_4ARG
	enum pid_type __type;
	if (dequeue_signal(task, &set, &__info, &__type) != 0) {
#else
	if (dequeue_signal(task, &set, &__info) != 0) {
#endif
#ifdef HAVE_SIGNAL_STOP
		spin_unlock_irq(&task->sighand->siglock);
		kernel_signal_stop();
#else
		if (current->jobctl & JOBCTL_STOP_DEQUEUED)
			spl_set_special_state(TASK_STOPPED);
		spin_unlock_irq(&current->sighand->siglock);
		schedule();
#endif
		return (0);
	}
	spin_unlock_irq(&task->sighand->siglock);
	return (1);
}
EXPORT_SYMBOL(issig);
