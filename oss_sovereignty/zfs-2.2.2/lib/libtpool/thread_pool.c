 

 

#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>
#include "thread_pool_impl.h"

static pthread_mutex_t thread_pool_lock = PTHREAD_MUTEX_INITIALIZER;
static tpool_t *thread_pools = NULL;

static void
delete_pool(tpool_t *tpool)
{
	tpool_job_t *job;

	ASSERT(tpool->tp_current == 0 && tpool->tp_active == NULL);

	 
	(void) pthread_mutex_lock(&thread_pool_lock);
	if (thread_pools == tpool)
		thread_pools = tpool->tp_forw;
	if (thread_pools == tpool)
		thread_pools = NULL;
	else {
		tpool->tp_back->tp_forw = tpool->tp_forw;
		tpool->tp_forw->tp_back = tpool->tp_back;
	}
	pthread_mutex_unlock(&thread_pool_lock);

	 
	for (job = tpool->tp_head; job != NULL; job = tpool->tp_head) {
		tpool->tp_head = job->tpj_next;
		free(job);
	}
	(void) pthread_attr_destroy(&tpool->tp_attr);
	free(tpool);
}

 
static void
worker_cleanup(void *arg)
{
	tpool_t *tpool = (tpool_t *)arg;

	if (--tpool->tp_current == 0 &&
	    (tpool->tp_flags & (TP_DESTROY | TP_ABANDON))) {
		if (tpool->tp_flags & TP_ABANDON) {
			pthread_mutex_unlock(&tpool->tp_mutex);
			delete_pool(tpool);
			return;
		}
		if (tpool->tp_flags & TP_DESTROY)
			(void) pthread_cond_broadcast(&tpool->tp_busycv);
	}
	pthread_mutex_unlock(&tpool->tp_mutex);
}

static void
notify_waiters(tpool_t *tpool)
{
	if (tpool->tp_head == NULL && tpool->tp_active == NULL) {
		tpool->tp_flags &= ~TP_WAIT;
		(void) pthread_cond_broadcast(&tpool->tp_waitcv);
	}
}

 
static void
job_cleanup(void *arg)
{
	tpool_t *tpool = (tpool_t *)arg;

	pthread_t my_tid = pthread_self();
	tpool_active_t *activep;
	tpool_active_t **activepp;

	pthread_mutex_lock(&tpool->tp_mutex);
	for (activepp = &tpool->tp_active; ; activepp = &activep->tpa_next) {
		activep = *activepp;
		if (activep->tpa_tid == my_tid) {
			*activepp = activep->tpa_next;
			break;
		}
	}
	if (tpool->tp_flags & TP_WAIT)
		notify_waiters(tpool);
}

static void *
tpool_worker(void *arg)
{
	tpool_t *tpool = (tpool_t *)arg;
	int elapsed;
	tpool_job_t *job;
	void (*func)(void *);
	tpool_active_t active;

	pthread_mutex_lock(&tpool->tp_mutex);
	pthread_cleanup_push(worker_cleanup, tpool);

	 
	active.tpa_tid = pthread_self();
	for (;;) {
		elapsed = 0;
		tpool->tp_idle++;
		if (tpool->tp_flags & TP_WAIT)
			notify_waiters(tpool);
		while ((tpool->tp_head == NULL ||
		    (tpool->tp_flags & TP_SUSPEND)) &&
		    !(tpool->tp_flags & (TP_DESTROY | TP_ABANDON))) {
			if (tpool->tp_current <= tpool->tp_minimum ||
			    tpool->tp_linger == 0) {
				(void) pthread_cond_wait(&tpool->tp_workcv,
				    &tpool->tp_mutex);
			} else {
				struct timespec ts;

				clock_gettime(CLOCK_REALTIME, &ts);
				ts.tv_sec += tpool->tp_linger;

				if (pthread_cond_timedwait(&tpool->tp_workcv,
				    &tpool->tp_mutex, &ts) != 0) {
					elapsed = 1;
					break;
				}
			}
		}
		tpool->tp_idle--;
		if (tpool->tp_flags & TP_DESTROY)
			break;
		if (tpool->tp_flags & TP_ABANDON) {
			 
			if (tpool->tp_flags & TP_SUSPEND) {
				tpool->tp_flags &= ~TP_SUSPEND;
				(void) pthread_cond_broadcast(
				    &tpool->tp_workcv);
			}
			if (tpool->tp_head == NULL)
				break;
		}
		if ((job = tpool->tp_head) != NULL &&
		    !(tpool->tp_flags & TP_SUSPEND)) {
			elapsed = 0;
			func = job->tpj_func;
			arg = job->tpj_arg;
			tpool->tp_head = job->tpj_next;
			if (job == tpool->tp_tail)
				tpool->tp_tail = NULL;
			tpool->tp_njobs--;
			active.tpa_next = tpool->tp_active;
			tpool->tp_active = &active;
			pthread_mutex_unlock(&tpool->tp_mutex);
			pthread_cleanup_push(job_cleanup, tpool);
			free(job);

			sigset_t maskset;
			(void) pthread_sigmask(SIG_SETMASK, NULL, &maskset);

			 
			func(arg);
			 
			(void) pthread_sigmask(SIG_SETMASK, &maskset, NULL);
			(void) pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,
			    NULL);
			(void) pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,
			    NULL);
			pthread_cleanup_pop(1);
		}
		if (elapsed && tpool->tp_current > tpool->tp_minimum) {
			 
			break;
		}
	}
	pthread_cleanup_pop(1);
	return (arg);
}

 
static int
create_worker(tpool_t *tpool)
{
	pthread_t thread;
	sigset_t oset;
	int error;

	(void) pthread_sigmask(SIG_SETMASK, NULL, &oset);
	error = pthread_create(&thread, &tpool->tp_attr, tpool_worker, tpool);
	(void) pthread_sigmask(SIG_SETMASK, &oset, NULL);
	return (error);
}


 
static int
pthread_attr_clone(pthread_attr_t *attr, const pthread_attr_t *old_attr)
{
	int error;

	error = pthread_attr_init(attr);
	if (error || (old_attr == NULL))
		return (error);

#ifdef __GLIBC__
	cpu_set_t cpuset;
	size_t cpusetsize = sizeof (cpuset);
	error = pthread_attr_getaffinity_np(old_attr, cpusetsize, &cpuset);
	if (error == 0)
		error = pthread_attr_setaffinity_np(attr, cpusetsize, &cpuset);
	if (error)
		goto error;
#endif  

	int detachstate;
	error = pthread_attr_getdetachstate(old_attr, &detachstate);
	if (error == 0)
		error = pthread_attr_setdetachstate(attr, detachstate);
	if (error)
		goto error;

	size_t guardsize;
	error = pthread_attr_getguardsize(old_attr, &guardsize);
	if (error == 0)
		error = pthread_attr_setguardsize(attr, guardsize);
	if (error)
		goto error;

	int inheritsched;
	error = pthread_attr_getinheritsched(old_attr, &inheritsched);
	if (error == 0)
		error = pthread_attr_setinheritsched(attr, inheritsched);
	if (error)
		goto error;

	struct sched_param param;
	error = pthread_attr_getschedparam(old_attr, &param);
	if (error == 0)
		error = pthread_attr_setschedparam(attr, &param);
	if (error)
		goto error;

	int policy;
	error = pthread_attr_getschedpolicy(old_attr, &policy);
	if (error == 0)
		error = pthread_attr_setschedpolicy(attr, policy);
	if (error)
		goto error;

	int scope;
	error = pthread_attr_getscope(old_attr, &scope);
	if (error == 0)
		error = pthread_attr_setscope(attr, scope);
	if (error)
		goto error;

	void *stackaddr;
	size_t stacksize;
	error = pthread_attr_getstack(old_attr, &stackaddr, &stacksize);
	if (error == 0)
		error = pthread_attr_setstack(attr, stackaddr, stacksize);
	if (error)
		goto error;

	return (0);
error:
	pthread_attr_destroy(attr);
	return (error);
}

tpool_t	*
tpool_create(uint_t min_threads, uint_t max_threads, uint_t linger,
    pthread_attr_t *attr)
{
	tpool_t	*tpool;
	void *stackaddr;
	size_t stacksize;
	size_t minstack;
	int error;

	if (min_threads > max_threads || max_threads < 1) {
		errno = EINVAL;
		return (NULL);
	}
	if (attr != NULL) {
		if (pthread_attr_getstack(attr, &stackaddr, &stacksize) != 0) {
			errno = EINVAL;
			return (NULL);
		}
		 
		minstack = PTHREAD_STACK_MIN;
		if (stackaddr != NULL) {
			if (stacksize < minstack || max_threads != 1) {
				errno = EINVAL;
				return (NULL);
			}
		} else if (stacksize != 0 && stacksize < minstack) {
			errno = EINVAL;
			return (NULL);
		}
	}

	tpool = calloc(1, sizeof (*tpool));
	if (tpool == NULL) {
		errno = ENOMEM;
		return (NULL);
	}
	(void) pthread_mutex_init(&tpool->tp_mutex, NULL);
	(void) pthread_cond_init(&tpool->tp_busycv, NULL);
	(void) pthread_cond_init(&tpool->tp_workcv, NULL);
	(void) pthread_cond_init(&tpool->tp_waitcv, NULL);
	tpool->tp_minimum = min_threads;
	tpool->tp_maximum = max_threads;
	tpool->tp_linger = linger;

	 
	error = pthread_attr_clone(&tpool->tp_attr, attr);
	if (error) {
		free(tpool);
		errno = error;
		return (NULL);
	}

	 
	(void) pthread_attr_setdetachstate(&tpool->tp_attr,
	    PTHREAD_CREATE_DETACHED);

	 
	pthread_mutex_lock(&thread_pool_lock);
	if (thread_pools == NULL) {
		tpool->tp_forw = tpool;
		tpool->tp_back = tpool;
		thread_pools = tpool;
	} else {
		thread_pools->tp_back->tp_forw = tpool;
		tpool->tp_forw = thread_pools;
		tpool->tp_back = thread_pools->tp_back;
		thread_pools->tp_back = tpool;
	}
	pthread_mutex_unlock(&thread_pool_lock);

	return (tpool);
}

 
int
tpool_dispatch(tpool_t *tpool, void (*func)(void *), void *arg)
{
	tpool_job_t *job;

	ASSERT(!(tpool->tp_flags & (TP_DESTROY | TP_ABANDON)));

	if ((job = calloc(1, sizeof (*job))) == NULL)
		return (-1);
	job->tpj_next = NULL;
	job->tpj_func = func;
	job->tpj_arg = arg;

	pthread_mutex_lock(&tpool->tp_mutex);

	if (tpool->tp_head == NULL)
		tpool->tp_head = job;
	else
		tpool->tp_tail->tpj_next = job;
	tpool->tp_tail = job;
	tpool->tp_njobs++;

	if (!(tpool->tp_flags & TP_SUSPEND)) {
		if (tpool->tp_idle > 0)
			(void) pthread_cond_signal(&tpool->tp_workcv);
		else if (tpool->tp_current < tpool->tp_maximum &&
		    create_worker(tpool) == 0)
			tpool->tp_current++;
	}

	pthread_mutex_unlock(&tpool->tp_mutex);
	return (0);
}

static void
tpool_cleanup(void *arg)
{
	tpool_t *tpool = (tpool_t *)arg;

	pthread_mutex_unlock(&tpool->tp_mutex);
}

 
void
tpool_destroy(tpool_t *tpool)
{
	tpool_active_t *activep;

	ASSERT(!tpool_member(tpool));
	ASSERT(!(tpool->tp_flags & (TP_DESTROY | TP_ABANDON)));

	pthread_mutex_lock(&tpool->tp_mutex);
	pthread_cleanup_push(tpool_cleanup, tpool);

	 
	tpool->tp_flags |= TP_DESTROY;
	tpool->tp_flags &= ~TP_SUSPEND;
	(void) pthread_cond_broadcast(&tpool->tp_workcv);

	 
	for (activep = tpool->tp_active; activep; activep = activep->tpa_next)
		(void) pthread_cancel(activep->tpa_tid);

	 
	while (tpool->tp_active != NULL) {
		tpool->tp_flags |= TP_WAIT;
		(void) pthread_cond_wait(&tpool->tp_waitcv, &tpool->tp_mutex);
	}

	 
	while (tpool->tp_current != 0)
		(void) pthread_cond_wait(&tpool->tp_busycv, &tpool->tp_mutex);

	pthread_cleanup_pop(1);	 
	delete_pool(tpool);
}

 
void
tpool_abandon(tpool_t *tpool)
{
	ASSERT(!(tpool->tp_flags & (TP_DESTROY | TP_ABANDON)));

	pthread_mutex_lock(&tpool->tp_mutex);
	if (tpool->tp_current == 0) {
		 
		pthread_mutex_unlock(&tpool->tp_mutex);
		delete_pool(tpool);
	} else {
		 
		tpool->tp_flags |= TP_ABANDON;
		tpool->tp_flags &= ~TP_SUSPEND;
		(void) pthread_cond_broadcast(&tpool->tp_workcv);
		pthread_mutex_unlock(&tpool->tp_mutex);
	}
}

 
void
tpool_wait(tpool_t *tpool)
{
	ASSERT(!tpool_member(tpool));
	ASSERT(!(tpool->tp_flags & (TP_DESTROY | TP_ABANDON)));

	pthread_mutex_lock(&tpool->tp_mutex);
	pthread_cleanup_push(tpool_cleanup, tpool);
	while (tpool->tp_head != NULL || tpool->tp_active != NULL) {
		tpool->tp_flags |= TP_WAIT;
		(void) pthread_cond_wait(&tpool->tp_waitcv, &tpool->tp_mutex);
		ASSERT(!(tpool->tp_flags & (TP_DESTROY | TP_ABANDON)));
	}
	pthread_cleanup_pop(1);	 
}

void
tpool_suspend(tpool_t *tpool)
{
	ASSERT(!(tpool->tp_flags & (TP_DESTROY | TP_ABANDON)));

	pthread_mutex_lock(&tpool->tp_mutex);
	tpool->tp_flags |= TP_SUSPEND;
	pthread_mutex_unlock(&tpool->tp_mutex);
}

int
tpool_suspended(tpool_t *tpool)
{
	int suspended;

	ASSERT(!(tpool->tp_flags & (TP_DESTROY | TP_ABANDON)));

	pthread_mutex_lock(&tpool->tp_mutex);
	suspended = (tpool->tp_flags & TP_SUSPEND) != 0;
	pthread_mutex_unlock(&tpool->tp_mutex);

	return (suspended);
}

void
tpool_resume(tpool_t *tpool)
{
	int excess;

	ASSERT(!(tpool->tp_flags & (TP_DESTROY | TP_ABANDON)));

	pthread_mutex_lock(&tpool->tp_mutex);
	if (!(tpool->tp_flags & TP_SUSPEND)) {
		pthread_mutex_unlock(&tpool->tp_mutex);
		return;
	}
	tpool->tp_flags &= ~TP_SUSPEND;
	(void) pthread_cond_broadcast(&tpool->tp_workcv);
	excess = tpool->tp_njobs - tpool->tp_idle;
	while (excess-- > 0 && tpool->tp_current < tpool->tp_maximum) {
		if (create_worker(tpool) != 0)
			break;		 
		tpool->tp_current++;
	}
	pthread_mutex_unlock(&tpool->tp_mutex);
}

int
tpool_member(tpool_t *tpool)
{
	pthread_t my_tid = pthread_self();
	tpool_active_t *activep;

	ASSERT(!(tpool->tp_flags & (TP_DESTROY | TP_ABANDON)));

	pthread_mutex_lock(&tpool->tp_mutex);
	for (activep = tpool->tp_active; activep; activep = activep->tpa_next) {
		if (activep->tpa_tid == my_tid) {
			pthread_mutex_unlock(&tpool->tp_mutex);
			return (1);
		}
	}
	pthread_mutex_unlock(&tpool->tp_mutex);
	return (0);
}
