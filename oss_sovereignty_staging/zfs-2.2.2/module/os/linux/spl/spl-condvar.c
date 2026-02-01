 

#include <sys/condvar.h>
#include <sys/time.h>
#include <sys/sysmacros.h>
#include <linux/hrtimer.h>
#include <linux/compiler_compat.h>
#include <linux/mod_compat.h>

#include <linux/sched.h>

#ifdef HAVE_SCHED_SIGNAL_HEADER
#include <linux/sched/signal.h>
#endif

#define	MAX_HRTIMEOUT_SLACK_US	1000
static unsigned int spl_schedule_hrtimeout_slack_us = 0;

static int
param_set_hrtimeout_slack(const char *buf, zfs_kernel_param_t *kp)
{
	unsigned long val;
	int error;

	error = kstrtoul(buf, 0, &val);
	if (error)
		return (error);

	if (val > MAX_HRTIMEOUT_SLACK_US)
		return (-EINVAL);

	error = param_set_uint(buf, kp);
	if (error < 0)
		return (error);

	return (0);
}

module_param_call(spl_schedule_hrtimeout_slack_us, param_set_hrtimeout_slack,
	param_get_uint, &spl_schedule_hrtimeout_slack_us, 0644);
MODULE_PARM_DESC(spl_schedule_hrtimeout_slack_us,
	"schedule_hrtimeout_range() delta/slack value in us, default(0)");

void
__cv_init(kcondvar_t *cvp, char *name, kcv_type_t type, void *arg)
{
	ASSERT(cvp);
	ASSERT(name == NULL);
	ASSERT(type == CV_DEFAULT);
	ASSERT(arg == NULL);

	cvp->cv_magic = CV_MAGIC;
	init_waitqueue_head(&cvp->cv_event);
	init_waitqueue_head(&cvp->cv_destroy);
	atomic_set(&cvp->cv_waiters, 0);
	atomic_set(&cvp->cv_refs, 1);
	cvp->cv_mutex = NULL;
}
EXPORT_SYMBOL(__cv_init);

static int
cv_destroy_wakeup(kcondvar_t *cvp)
{
	if (!atomic_read(&cvp->cv_waiters) && !atomic_read(&cvp->cv_refs)) {
		ASSERT(cvp->cv_mutex == NULL);
		ASSERT(!waitqueue_active(&cvp->cv_event));
		return (1);
	}

	return (0);
}

void
__cv_destroy(kcondvar_t *cvp)
{
	ASSERT(cvp);
	ASSERT(cvp->cv_magic == CV_MAGIC);

	cvp->cv_magic = CV_DESTROY;
	atomic_dec(&cvp->cv_refs);

	 
	while (cv_destroy_wakeup(cvp) == 0)
		wait_event_timeout(cvp->cv_destroy, cv_destroy_wakeup(cvp), 1);

	ASSERT3P(cvp->cv_mutex, ==, NULL);
	ASSERT3S(atomic_read(&cvp->cv_refs), ==, 0);
	ASSERT3S(atomic_read(&cvp->cv_waiters), ==, 0);
	ASSERT3S(waitqueue_active(&cvp->cv_event), ==, 0);
}
EXPORT_SYMBOL(__cv_destroy);

static void
cv_wait_common(kcondvar_t *cvp, kmutex_t *mp, int state, int io)
{
	DEFINE_WAIT(wait);
	kmutex_t *m;

	ASSERT(cvp);
	ASSERT(mp);
	ASSERT(cvp->cv_magic == CV_MAGIC);
	ASSERT(mutex_owned(mp));
	atomic_inc(&cvp->cv_refs);

	m = READ_ONCE(cvp->cv_mutex);
	if (!m)
		m = xchg(&cvp->cv_mutex, mp);
	 
	ASSERT(m == NULL || m == mp);

	prepare_to_wait_exclusive(&cvp->cv_event, &wait, state);
	atomic_inc(&cvp->cv_waiters);

	 
	mutex_exit(mp);
	if (io)
		io_schedule();
	else
		schedule();

	 
	if (atomic_dec_and_test(&cvp->cv_waiters)) {
		 
		cvp->cv_mutex = NULL;
		wake_up(&cvp->cv_destroy);
	}

	finish_wait(&cvp->cv_event, &wait);
	atomic_dec(&cvp->cv_refs);

	 
	mutex_enter(mp);
}

void
__cv_wait(kcondvar_t *cvp, kmutex_t *mp)
{
	cv_wait_common(cvp, mp, TASK_UNINTERRUPTIBLE, 0);
}
EXPORT_SYMBOL(__cv_wait);

void
__cv_wait_io(kcondvar_t *cvp, kmutex_t *mp)
{
	cv_wait_common(cvp, mp, TASK_UNINTERRUPTIBLE, 1);
}
EXPORT_SYMBOL(__cv_wait_io);

int
__cv_wait_io_sig(kcondvar_t *cvp, kmutex_t *mp)
{
	cv_wait_common(cvp, mp, TASK_INTERRUPTIBLE, 1);

	return (signal_pending(current) ? 0 : 1);
}
EXPORT_SYMBOL(__cv_wait_io_sig);

int
__cv_wait_sig(kcondvar_t *cvp, kmutex_t *mp)
{
	cv_wait_common(cvp, mp, TASK_INTERRUPTIBLE, 0);

	return (signal_pending(current) ? 0 : 1);
}
EXPORT_SYMBOL(__cv_wait_sig);

void
__cv_wait_idle(kcondvar_t *cvp, kmutex_t *mp)
{
	sigset_t blocked, saved;

	sigfillset(&blocked);
	(void) sigprocmask(SIG_BLOCK, &blocked, &saved);
	cv_wait_common(cvp, mp, TASK_INTERRUPTIBLE, 0);
	(void) sigprocmask(SIG_SETMASK, &saved, NULL);
}
EXPORT_SYMBOL(__cv_wait_idle);

#if defined(HAVE_IO_SCHEDULE_TIMEOUT)
#define	spl_io_schedule_timeout(t)	io_schedule_timeout(t)
#else

struct spl_task_timer {
	struct timer_list timer;
	struct task_struct *task;
};

static void
__cv_wakeup(spl_timer_list_t t)
{
	struct timer_list *tmr = (struct timer_list *)t;
	struct spl_task_timer *task_timer = from_timer(task_timer, tmr, timer);

	wake_up_process(task_timer->task);
}

static long
spl_io_schedule_timeout(long time_left)
{
	long expire_time = jiffies + time_left;
	struct spl_task_timer task_timer;
	struct timer_list *timer = &task_timer.timer;

	task_timer.task = current;

	timer_setup(timer, __cv_wakeup, 0);

	timer->expires = expire_time;
	add_timer(timer);

	io_schedule();

	del_timer_sync(timer);

	time_left = expire_time - jiffies;

	return (time_left < 0 ? 0 : time_left);
}
#endif

 
static clock_t
__cv_timedwait_common(kcondvar_t *cvp, kmutex_t *mp, clock_t expire_time,
    int state, int io)
{
	DEFINE_WAIT(wait);
	kmutex_t *m;
	clock_t time_left;

	ASSERT(cvp);
	ASSERT(mp);
	ASSERT(cvp->cv_magic == CV_MAGIC);
	ASSERT(mutex_owned(mp));

	 
	time_left = expire_time - jiffies;
	if (time_left <= 0)
		return (-1);

	atomic_inc(&cvp->cv_refs);
	m = READ_ONCE(cvp->cv_mutex);
	if (!m)
		m = xchg(&cvp->cv_mutex, mp);
	 
	ASSERT(m == NULL || m == mp);

	prepare_to_wait_exclusive(&cvp->cv_event, &wait, state);
	atomic_inc(&cvp->cv_waiters);

	 
	mutex_exit(mp);
	if (io)
		time_left = spl_io_schedule_timeout(time_left);
	else
		time_left = schedule_timeout(time_left);

	 
	if (atomic_dec_and_test(&cvp->cv_waiters)) {
		 
		cvp->cv_mutex = NULL;
		wake_up(&cvp->cv_destroy);
	}

	finish_wait(&cvp->cv_event, &wait);
	atomic_dec(&cvp->cv_refs);

	 
	mutex_enter(mp);
	return (time_left > 0 ? 1 : -1);
}

int
__cv_timedwait(kcondvar_t *cvp, kmutex_t *mp, clock_t exp_time)
{
	return (__cv_timedwait_common(cvp, mp, exp_time,
	    TASK_UNINTERRUPTIBLE, 0));
}
EXPORT_SYMBOL(__cv_timedwait);

int
__cv_timedwait_io(kcondvar_t *cvp, kmutex_t *mp, clock_t exp_time)
{
	return (__cv_timedwait_common(cvp, mp, exp_time,
	    TASK_UNINTERRUPTIBLE, 1));
}
EXPORT_SYMBOL(__cv_timedwait_io);

int
__cv_timedwait_sig(kcondvar_t *cvp, kmutex_t *mp, clock_t exp_time)
{
	int rc;

	rc = __cv_timedwait_common(cvp, mp, exp_time, TASK_INTERRUPTIBLE, 0);
	return (signal_pending(current) ? 0 : rc);
}
EXPORT_SYMBOL(__cv_timedwait_sig);

int
__cv_timedwait_idle(kcondvar_t *cvp, kmutex_t *mp, clock_t exp_time)
{
	sigset_t blocked, saved;
	int rc;

	sigfillset(&blocked);
	(void) sigprocmask(SIG_BLOCK, &blocked, &saved);
	rc = __cv_timedwait_common(cvp, mp, exp_time,
	    TASK_INTERRUPTIBLE, 0);
	(void) sigprocmask(SIG_SETMASK, &saved, NULL);

	return (rc);
}
EXPORT_SYMBOL(__cv_timedwait_idle);
 
static clock_t
__cv_timedwait_hires(kcondvar_t *cvp, kmutex_t *mp, hrtime_t expire_time,
    hrtime_t res, int state)
{
	DEFINE_WAIT(wait);
	kmutex_t *m;
	hrtime_t time_left;
	ktime_t ktime_left;
	u64 slack = 0;
	int rc;

	ASSERT(cvp);
	ASSERT(mp);
	ASSERT(cvp->cv_magic == CV_MAGIC);
	ASSERT(mutex_owned(mp));

	time_left = expire_time - gethrtime();
	if (time_left <= 0)
		return (-1);

	atomic_inc(&cvp->cv_refs);
	m = READ_ONCE(cvp->cv_mutex);
	if (!m)
		m = xchg(&cvp->cv_mutex, mp);
	 
	ASSERT(m == NULL || m == mp);

	prepare_to_wait_exclusive(&cvp->cv_event, &wait, state);
	atomic_inc(&cvp->cv_waiters);

	 
	mutex_exit(mp);

	ktime_left = ktime_set(0, time_left);
	slack = MIN(MAX(res, spl_schedule_hrtimeout_slack_us * NSEC_PER_USEC),
	    MAX_HRTIMEOUT_SLACK_US * NSEC_PER_USEC);
	rc = schedule_hrtimeout_range(&ktime_left, slack, HRTIMER_MODE_REL);

	 
	if (atomic_dec_and_test(&cvp->cv_waiters)) {
		 
		cvp->cv_mutex = NULL;
		wake_up(&cvp->cv_destroy);
	}

	finish_wait(&cvp->cv_event, &wait);
	atomic_dec(&cvp->cv_refs);

	mutex_enter(mp);
	return (rc == -EINTR ? 1 : -1);
}

 
static int
cv_timedwait_hires_common(kcondvar_t *cvp, kmutex_t *mp, hrtime_t tim,
    hrtime_t res, int flag, int state)
{
	if (!(flag & CALLOUT_FLAG_ABSOLUTE))
		tim += gethrtime();

	return (__cv_timedwait_hires(cvp, mp, tim, res, state));
}

int
cv_timedwait_hires(kcondvar_t *cvp, kmutex_t *mp, hrtime_t tim, hrtime_t res,
    int flag)
{
	return (cv_timedwait_hires_common(cvp, mp, tim, res, flag,
	    TASK_UNINTERRUPTIBLE));
}
EXPORT_SYMBOL(cv_timedwait_hires);

int
cv_timedwait_sig_hires(kcondvar_t *cvp, kmutex_t *mp, hrtime_t tim,
    hrtime_t res, int flag)
{
	int rc;

	rc = cv_timedwait_hires_common(cvp, mp, tim, res, flag,
	    TASK_INTERRUPTIBLE);
	return (signal_pending(current) ? 0 : rc);
}
EXPORT_SYMBOL(cv_timedwait_sig_hires);

int
cv_timedwait_idle_hires(kcondvar_t *cvp, kmutex_t *mp, hrtime_t tim,
    hrtime_t res, int flag)
{
	sigset_t blocked, saved;
	int rc;

	sigfillset(&blocked);
	(void) sigprocmask(SIG_BLOCK, &blocked, &saved);
	rc = cv_timedwait_hires_common(cvp, mp, tim, res, flag,
	    TASK_INTERRUPTIBLE);
	(void) sigprocmask(SIG_SETMASK, &saved, NULL);

	return (rc);
}
EXPORT_SYMBOL(cv_timedwait_idle_hires);

void
__cv_signal(kcondvar_t *cvp)
{
	ASSERT(cvp);
	ASSERT(cvp->cv_magic == CV_MAGIC);
	atomic_inc(&cvp->cv_refs);

	 
	if (atomic_read(&cvp->cv_waiters) > 0)
		wake_up(&cvp->cv_event);

	atomic_dec(&cvp->cv_refs);
}
EXPORT_SYMBOL(__cv_signal);

void
__cv_broadcast(kcondvar_t *cvp)
{
	ASSERT(cvp);
	ASSERT(cvp->cv_magic == CV_MAGIC);
	atomic_inc(&cvp->cv_refs);

	 
	if (atomic_read(&cvp->cv_waiters) > 0)
		wake_up_all(&cvp->cv_event);

	atomic_dec(&cvp->cv_refs);
}
EXPORT_SYMBOL(__cv_broadcast);
