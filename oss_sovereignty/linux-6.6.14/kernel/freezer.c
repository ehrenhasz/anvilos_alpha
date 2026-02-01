
 

#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/export.h>
#include <linux/syscalls.h>
#include <linux/freezer.h>
#include <linux/kthread.h>

 
DEFINE_STATIC_KEY_FALSE(freezer_active);
EXPORT_SYMBOL(freezer_active);

 
bool pm_freezing;
bool pm_nosig_freezing;

 
static DEFINE_SPINLOCK(freezer_lock);

 
bool freezing_slow_path(struct task_struct *p)
{
	if (p->flags & (PF_NOFREEZE | PF_SUSPEND_TASK))
		return false;

	if (test_tsk_thread_flag(p, TIF_MEMDIE))
		return false;

	if (pm_nosig_freezing || cgroup_freezing(p))
		return true;

	if (pm_freezing && !(p->flags & PF_KTHREAD))
		return true;

	return false;
}
EXPORT_SYMBOL(freezing_slow_path);

bool frozen(struct task_struct *p)
{
	return READ_ONCE(p->__state) & TASK_FROZEN;
}

 
bool __refrigerator(bool check_kthr_stop)
{
	unsigned int state = get_current_state();
	bool was_frozen = false;

	pr_debug("%s entered refrigerator\n", current->comm);

	WARN_ON_ONCE(state && !(state & TASK_NORMAL));

	for (;;) {
		bool freeze;

		set_current_state(TASK_FROZEN);

		spin_lock_irq(&freezer_lock);
		freeze = freezing(current) && !(check_kthr_stop && kthread_should_stop());
		spin_unlock_irq(&freezer_lock);

		if (!freeze)
			break;

		was_frozen = true;
		schedule();
	}
	__set_current_state(TASK_RUNNING);

	pr_debug("%s left refrigerator\n", current->comm);

	return was_frozen;
}
EXPORT_SYMBOL(__refrigerator);

static void fake_signal_wake_up(struct task_struct *p)
{
	unsigned long flags;

	if (lock_task_sighand(p, &flags)) {
		signal_wake_up(p, 0);
		unlock_task_sighand(p, &flags);
	}
}

static int __set_task_frozen(struct task_struct *p, void *arg)
{
	unsigned int state = READ_ONCE(p->__state);

	if (p->on_rq)
		return 0;

	if (p != current && task_curr(p))
		return 0;

	if (!(state & (TASK_FREEZABLE | __TASK_STOPPED | __TASK_TRACED)))
		return 0;

	 
	if (state & TASK_FREEZABLE)
		WARN_ON_ONCE(!(state & TASK_NORMAL));

#ifdef CONFIG_LOCKDEP
	 
	if (!(state & __TASK_FREEZABLE_UNSAFE))
		WARN_ON_ONCE(debug_locks && p->lockdep_depth);
#endif

	WRITE_ONCE(p->__state, TASK_FROZEN);
	return TASK_FROZEN;
}

static bool __freeze_task(struct task_struct *p)
{
	 
	return task_call_func(p, __set_task_frozen, NULL);
}

 
bool freeze_task(struct task_struct *p)
{
	unsigned long flags;

	spin_lock_irqsave(&freezer_lock, flags);
	if (!freezing(p) || frozen(p) || __freeze_task(p)) {
		spin_unlock_irqrestore(&freezer_lock, flags);
		return false;
	}

	if (!(p->flags & PF_KTHREAD))
		fake_signal_wake_up(p);
	else
		wake_up_state(p, TASK_NORMAL);

	spin_unlock_irqrestore(&freezer_lock, flags);
	return true;
}

 
static int __set_task_special(struct task_struct *p, void *arg)
{
	unsigned int state = 0;

	if (p->jobctl & JOBCTL_TRACED)
		state = TASK_TRACED;

	else if (p->jobctl & JOBCTL_STOPPED)
		state = TASK_STOPPED;

	if (state)
		WRITE_ONCE(p->__state, state);

	return state;
}

void __thaw_task(struct task_struct *p)
{
	unsigned long flags, flags2;

	spin_lock_irqsave(&freezer_lock, flags);
	if (WARN_ON_ONCE(freezing(p)))
		goto unlock;

	if (lock_task_sighand(p, &flags2)) {
		 
		bool ret = task_call_func(p, __set_task_special, NULL);
		unlock_task_sighand(p, &flags2);
		if (ret)
			goto unlock;
	}

	wake_up_state(p, TASK_FROZEN);
unlock:
	spin_unlock_irqrestore(&freezer_lock, flags);
}

 
bool set_freezable(void)
{
	might_sleep();

	 
	spin_lock_irq(&freezer_lock);
	current->flags &= ~PF_NOFREEZE;
	spin_unlock_irq(&freezer_lock);

	return try_to_freeze();
}
EXPORT_SYMBOL(set_freezable);
