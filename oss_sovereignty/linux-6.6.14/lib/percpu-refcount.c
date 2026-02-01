
#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/percpu-refcount.h>

 

#define PERCPU_COUNT_BIAS	(1LU << (BITS_PER_LONG - 1))

static DEFINE_SPINLOCK(percpu_ref_switch_lock);
static DECLARE_WAIT_QUEUE_HEAD(percpu_ref_switch_waitq);

static unsigned long __percpu *percpu_count_ptr(struct percpu_ref *ref)
{
	return (unsigned long __percpu *)
		(ref->percpu_count_ptr & ~__PERCPU_REF_ATOMIC_DEAD);
}

 
int percpu_ref_init(struct percpu_ref *ref, percpu_ref_func_t *release,
		    unsigned int flags, gfp_t gfp)
{
	size_t align = max_t(size_t, 1 << __PERCPU_REF_FLAG_BITS,
			     __alignof__(unsigned long));
	unsigned long start_count = 0;
	struct percpu_ref_data *data;

	ref->percpu_count_ptr = (unsigned long)
		__alloc_percpu_gfp(sizeof(unsigned long), align, gfp);
	if (!ref->percpu_count_ptr)
		return -ENOMEM;

	data = kzalloc(sizeof(*ref->data), gfp);
	if (!data) {
		free_percpu((void __percpu *)ref->percpu_count_ptr);
		ref->percpu_count_ptr = 0;
		return -ENOMEM;
	}

	data->force_atomic = flags & PERCPU_REF_INIT_ATOMIC;
	data->allow_reinit = flags & PERCPU_REF_ALLOW_REINIT;

	if (flags & (PERCPU_REF_INIT_ATOMIC | PERCPU_REF_INIT_DEAD)) {
		ref->percpu_count_ptr |= __PERCPU_REF_ATOMIC;
		data->allow_reinit = true;
	} else {
		start_count += PERCPU_COUNT_BIAS;
	}

	if (flags & PERCPU_REF_INIT_DEAD)
		ref->percpu_count_ptr |= __PERCPU_REF_DEAD;
	else
		start_count++;

	atomic_long_set(&data->count, start_count);

	data->release = release;
	data->confirm_switch = NULL;
	data->ref = ref;
	ref->data = data;
	return 0;
}
EXPORT_SYMBOL_GPL(percpu_ref_init);

static void __percpu_ref_exit(struct percpu_ref *ref)
{
	unsigned long __percpu *percpu_count = percpu_count_ptr(ref);

	if (percpu_count) {
		 
		WARN_ON_ONCE(ref->data && ref->data->confirm_switch);
		free_percpu(percpu_count);
		ref->percpu_count_ptr = __PERCPU_REF_ATOMIC_DEAD;
	}
}

 
void percpu_ref_exit(struct percpu_ref *ref)
{
	struct percpu_ref_data *data = ref->data;
	unsigned long flags;

	__percpu_ref_exit(ref);

	if (!data)
		return;

	spin_lock_irqsave(&percpu_ref_switch_lock, flags);
	ref->percpu_count_ptr |= atomic_long_read(&ref->data->count) <<
		__PERCPU_REF_FLAG_BITS;
	ref->data = NULL;
	spin_unlock_irqrestore(&percpu_ref_switch_lock, flags);

	kfree(data);
}
EXPORT_SYMBOL_GPL(percpu_ref_exit);

static void percpu_ref_call_confirm_rcu(struct rcu_head *rcu)
{
	struct percpu_ref_data *data = container_of(rcu,
			struct percpu_ref_data, rcu);
	struct percpu_ref *ref = data->ref;

	data->confirm_switch(ref);
	data->confirm_switch = NULL;
	wake_up_all(&percpu_ref_switch_waitq);

	if (!data->allow_reinit)
		__percpu_ref_exit(ref);

	 
	percpu_ref_put(ref);
}

static void percpu_ref_switch_to_atomic_rcu(struct rcu_head *rcu)
{
	struct percpu_ref_data *data = container_of(rcu,
			struct percpu_ref_data, rcu);
	struct percpu_ref *ref = data->ref;
	unsigned long __percpu *percpu_count = percpu_count_ptr(ref);
	static atomic_t underflows;
	unsigned long count = 0;
	int cpu;

	for_each_possible_cpu(cpu)
		count += *per_cpu_ptr(percpu_count, cpu);

	pr_debug("global %lu percpu %lu\n",
		 atomic_long_read(&data->count), count);

	 
	atomic_long_add((long)count - PERCPU_COUNT_BIAS, &data->count);

	if (WARN_ONCE(atomic_long_read(&data->count) <= 0,
		      "percpu ref (%ps) <= 0 (%ld) after switching to atomic",
		      data->release, atomic_long_read(&data->count)) &&
	    atomic_inc_return(&underflows) < 4) {
		pr_err("%s(): percpu_ref underflow", __func__);
		mem_dump_obj(data);
	}

	 
	percpu_ref_call_confirm_rcu(rcu);
}

static void percpu_ref_noop_confirm_switch(struct percpu_ref *ref)
{
}

static void __percpu_ref_switch_to_atomic(struct percpu_ref *ref,
					  percpu_ref_func_t *confirm_switch)
{
	if (ref->percpu_count_ptr & __PERCPU_REF_ATOMIC) {
		if (confirm_switch)
			confirm_switch(ref);
		return;
	}

	 
	ref->percpu_count_ptr |= __PERCPU_REF_ATOMIC;

	 
	ref->data->confirm_switch = confirm_switch ?:
		percpu_ref_noop_confirm_switch;

	percpu_ref_get(ref);	 
	call_rcu_hurry(&ref->data->rcu,
		       percpu_ref_switch_to_atomic_rcu);
}

static void __percpu_ref_switch_to_percpu(struct percpu_ref *ref)
{
	unsigned long __percpu *percpu_count = percpu_count_ptr(ref);
	int cpu;

	BUG_ON(!percpu_count);

	if (!(ref->percpu_count_ptr & __PERCPU_REF_ATOMIC))
		return;

	if (WARN_ON_ONCE(!ref->data->allow_reinit))
		return;

	atomic_long_add(PERCPU_COUNT_BIAS, &ref->data->count);

	 
	for_each_possible_cpu(cpu)
		*per_cpu_ptr(percpu_count, cpu) = 0;

	smp_store_release(&ref->percpu_count_ptr,
			  ref->percpu_count_ptr & ~__PERCPU_REF_ATOMIC);
}

static void __percpu_ref_switch_mode(struct percpu_ref *ref,
				     percpu_ref_func_t *confirm_switch)
{
	struct percpu_ref_data *data = ref->data;

	lockdep_assert_held(&percpu_ref_switch_lock);

	 
	wait_event_lock_irq(percpu_ref_switch_waitq, !data->confirm_switch,
			    percpu_ref_switch_lock);

	if (data->force_atomic || percpu_ref_is_dying(ref))
		__percpu_ref_switch_to_atomic(ref, confirm_switch);
	else
		__percpu_ref_switch_to_percpu(ref);
}

 
void percpu_ref_switch_to_atomic(struct percpu_ref *ref,
				 percpu_ref_func_t *confirm_switch)
{
	unsigned long flags;

	spin_lock_irqsave(&percpu_ref_switch_lock, flags);

	ref->data->force_atomic = true;
	__percpu_ref_switch_mode(ref, confirm_switch);

	spin_unlock_irqrestore(&percpu_ref_switch_lock, flags);
}
EXPORT_SYMBOL_GPL(percpu_ref_switch_to_atomic);

 
void percpu_ref_switch_to_atomic_sync(struct percpu_ref *ref)
{
	percpu_ref_switch_to_atomic(ref, NULL);
	wait_event(percpu_ref_switch_waitq, !ref->data->confirm_switch);
}
EXPORT_SYMBOL_GPL(percpu_ref_switch_to_atomic_sync);

 
void percpu_ref_switch_to_percpu(struct percpu_ref *ref)
{
	unsigned long flags;

	spin_lock_irqsave(&percpu_ref_switch_lock, flags);

	ref->data->force_atomic = false;
	__percpu_ref_switch_mode(ref, NULL);

	spin_unlock_irqrestore(&percpu_ref_switch_lock, flags);
}
EXPORT_SYMBOL_GPL(percpu_ref_switch_to_percpu);

 
void percpu_ref_kill_and_confirm(struct percpu_ref *ref,
				 percpu_ref_func_t *confirm_kill)
{
	unsigned long flags;

	spin_lock_irqsave(&percpu_ref_switch_lock, flags);

	WARN_ONCE(percpu_ref_is_dying(ref),
		  "%s called more than once on %ps!", __func__,
		  ref->data->release);

	ref->percpu_count_ptr |= __PERCPU_REF_DEAD;
	__percpu_ref_switch_mode(ref, confirm_kill);
	percpu_ref_put(ref);

	spin_unlock_irqrestore(&percpu_ref_switch_lock, flags);
}
EXPORT_SYMBOL_GPL(percpu_ref_kill_and_confirm);

 
bool percpu_ref_is_zero(struct percpu_ref *ref)
{
	unsigned long __percpu *percpu_count;
	unsigned long count, flags;

	if (__ref_is_percpu(ref, &percpu_count))
		return false;

	 
	spin_lock_irqsave(&percpu_ref_switch_lock, flags);
	if (ref->data)
		count = atomic_long_read(&ref->data->count);
	else
		count = ref->percpu_count_ptr >> __PERCPU_REF_FLAG_BITS;
	spin_unlock_irqrestore(&percpu_ref_switch_lock, flags);

	return count == 0;
}
EXPORT_SYMBOL_GPL(percpu_ref_is_zero);

 
void percpu_ref_reinit(struct percpu_ref *ref)
{
	WARN_ON_ONCE(!percpu_ref_is_zero(ref));

	percpu_ref_resurrect(ref);
}
EXPORT_SYMBOL_GPL(percpu_ref_reinit);

 
void percpu_ref_resurrect(struct percpu_ref *ref)
{
	unsigned long __percpu *percpu_count;
	unsigned long flags;

	spin_lock_irqsave(&percpu_ref_switch_lock, flags);

	WARN_ON_ONCE(!percpu_ref_is_dying(ref));
	WARN_ON_ONCE(__ref_is_percpu(ref, &percpu_count));

	ref->percpu_count_ptr &= ~__PERCPU_REF_DEAD;
	percpu_ref_get(ref);
	__percpu_ref_switch_mode(ref, NULL);

	spin_unlock_irqrestore(&percpu_ref_switch_lock, flags);
}
EXPORT_SYMBOL_GPL(percpu_ref_resurrect);
