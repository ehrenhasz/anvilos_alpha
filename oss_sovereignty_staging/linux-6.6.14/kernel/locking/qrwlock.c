
 
#include <linux/smp.h>
#include <linux/bug.h>
#include <linux/cpumask.h>
#include <linux/percpu.h>
#include <linux/hardirq.h>
#include <linux/spinlock.h>
#include <trace/events/lock.h>

 
void __lockfunc queued_read_lock_slowpath(struct qrwlock *lock)
{
	 
	if (unlikely(in_interrupt())) {
		 
		atomic_cond_read_acquire(&lock->cnts, !(VAL & _QW_LOCKED));
		return;
	}
	atomic_sub(_QR_BIAS, &lock->cnts);

	trace_contention_begin(lock, LCB_F_SPIN | LCB_F_READ);

	 
	arch_spin_lock(&lock->wait_lock);
	atomic_add(_QR_BIAS, &lock->cnts);

	 
	atomic_cond_read_acquire(&lock->cnts, !(VAL & _QW_LOCKED));

	 
	arch_spin_unlock(&lock->wait_lock);

	trace_contention_end(lock, 0);
}
EXPORT_SYMBOL(queued_read_lock_slowpath);

 
void __lockfunc queued_write_lock_slowpath(struct qrwlock *lock)
{
	int cnts;

	trace_contention_begin(lock, LCB_F_SPIN | LCB_F_WRITE);

	 
	arch_spin_lock(&lock->wait_lock);

	 
	if (!(cnts = atomic_read(&lock->cnts)) &&
	    atomic_try_cmpxchg_acquire(&lock->cnts, &cnts, _QW_LOCKED))
		goto unlock;

	 
	atomic_or(_QW_WAITING, &lock->cnts);

	 
	do {
		cnts = atomic_cond_read_relaxed(&lock->cnts, VAL == _QW_WAITING);
	} while (!atomic_try_cmpxchg_acquire(&lock->cnts, &cnts, _QW_LOCKED));
unlock:
	arch_spin_unlock(&lock->wait_lock);

	trace_contention_end(lock, 0);
}
EXPORT_SYMBOL(queued_write_lock_slowpath);
