
 

#include <linux/page_counter.h>
#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/bug.h>
#include <asm/page.h>

static void propagate_protected_usage(struct page_counter *c,
				      unsigned long usage)
{
	unsigned long protected, old_protected;
	long delta;

	if (!c->parent)
		return;

	protected = min(usage, READ_ONCE(c->min));
	old_protected = atomic_long_read(&c->min_usage);
	if (protected != old_protected) {
		old_protected = atomic_long_xchg(&c->min_usage, protected);
		delta = protected - old_protected;
		if (delta)
			atomic_long_add(delta, &c->parent->children_min_usage);
	}

	protected = min(usage, READ_ONCE(c->low));
	old_protected = atomic_long_read(&c->low_usage);
	if (protected != old_protected) {
		old_protected = atomic_long_xchg(&c->low_usage, protected);
		delta = protected - old_protected;
		if (delta)
			atomic_long_add(delta, &c->parent->children_low_usage);
	}
}

 
void page_counter_cancel(struct page_counter *counter, unsigned long nr_pages)
{
	long new;

	new = atomic_long_sub_return(nr_pages, &counter->usage);
	 
	if (WARN_ONCE(new < 0, "page_counter underflow: %ld nr_pages=%lu\n",
		      new, nr_pages)) {
		new = 0;
		atomic_long_set(&counter->usage, new);
	}
	propagate_protected_usage(counter, new);
}

 
void page_counter_charge(struct page_counter *counter, unsigned long nr_pages)
{
	struct page_counter *c;

	for (c = counter; c; c = c->parent) {
		long new;

		new = atomic_long_add_return(nr_pages, &c->usage);
		propagate_protected_usage(c, new);
		 
		if (new > READ_ONCE(c->watermark))
			WRITE_ONCE(c->watermark, new);
	}
}

 
bool page_counter_try_charge(struct page_counter *counter,
			     unsigned long nr_pages,
			     struct page_counter **fail)
{
	struct page_counter *c;

	for (c = counter; c; c = c->parent) {
		long new;
		 
		new = atomic_long_add_return(nr_pages, &c->usage);
		if (new > c->max) {
			atomic_long_sub(nr_pages, &c->usage);
			 
			data_race(c->failcnt++);
			*fail = c;
			goto failed;
		}
		propagate_protected_usage(c, new);
		 
		if (new > READ_ONCE(c->watermark))
			WRITE_ONCE(c->watermark, new);
	}
	return true;

failed:
	for (c = counter; c != *fail; c = c->parent)
		page_counter_cancel(c, nr_pages);

	return false;
}

 
void page_counter_uncharge(struct page_counter *counter, unsigned long nr_pages)
{
	struct page_counter *c;

	for (c = counter; c; c = c->parent)
		page_counter_cancel(c, nr_pages);
}

 
int page_counter_set_max(struct page_counter *counter, unsigned long nr_pages)
{
	for (;;) {
		unsigned long old;
		long usage;

		 
		usage = page_counter_read(counter);

		if (usage > nr_pages)
			return -EBUSY;

		old = xchg(&counter->max, nr_pages);

		if (page_counter_read(counter) <= usage || nr_pages >= old)
			return 0;

		counter->max = old;
		cond_resched();
	}
}

 
void page_counter_set_min(struct page_counter *counter, unsigned long nr_pages)
{
	struct page_counter *c;

	WRITE_ONCE(counter->min, nr_pages);

	for (c = counter; c; c = c->parent)
		propagate_protected_usage(c, atomic_long_read(&c->usage));
}

 
void page_counter_set_low(struct page_counter *counter, unsigned long nr_pages)
{
	struct page_counter *c;

	WRITE_ONCE(counter->low, nr_pages);

	for (c = counter; c; c = c->parent)
		propagate_protected_usage(c, atomic_long_read(&c->usage));
}

 
int page_counter_memparse(const char *buf, const char *max,
			  unsigned long *nr_pages)
{
	char *end;
	u64 bytes;

	if (!strcmp(buf, max)) {
		*nr_pages = PAGE_COUNTER_MAX;
		return 0;
	}

	bytes = memparse(buf, &end);
	if (*end != '\0')
		return -EINVAL;

	*nr_pages = min(bytes / PAGE_SIZE, (u64)PAGE_COUNTER_MAX);

	return 0;
}
