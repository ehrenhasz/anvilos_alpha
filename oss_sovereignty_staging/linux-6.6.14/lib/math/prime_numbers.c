
#define pr_fmt(fmt) "prime numbers: " fmt

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/prime_numbers.h>
#include <linux/slab.h>

#define bitmap_size(nbits) (BITS_TO_LONGS(nbits) * sizeof(unsigned long))

struct primes {
	struct rcu_head rcu;
	unsigned long last, sz;
	unsigned long primes[];
};

#if BITS_PER_LONG == 64
static const struct primes small_primes = {
	.last = 61,
	.sz = 64,
	.primes = {
		BIT(2) |
		BIT(3) |
		BIT(5) |
		BIT(7) |
		BIT(11) |
		BIT(13) |
		BIT(17) |
		BIT(19) |
		BIT(23) |
		BIT(29) |
		BIT(31) |
		BIT(37) |
		BIT(41) |
		BIT(43) |
		BIT(47) |
		BIT(53) |
		BIT(59) |
		BIT(61)
	}
};
#elif BITS_PER_LONG == 32
static const struct primes small_primes = {
	.last = 31,
	.sz = 32,
	.primes = {
		BIT(2) |
		BIT(3) |
		BIT(5) |
		BIT(7) |
		BIT(11) |
		BIT(13) |
		BIT(17) |
		BIT(19) |
		BIT(23) |
		BIT(29) |
		BIT(31)
	}
};
#else
#error "unhandled BITS_PER_LONG"
#endif

static DEFINE_MUTEX(lock);
static const struct primes __rcu *primes = RCU_INITIALIZER(&small_primes);

static unsigned long selftest_max;

static bool slow_is_prime_number(unsigned long x)
{
	unsigned long y = int_sqrt(x);

	while (y > 1) {
		if ((x % y) == 0)
			break;
		y--;
	}

	return y == 1;
}

static unsigned long slow_next_prime_number(unsigned long x)
{
	while (x < ULONG_MAX && !slow_is_prime_number(++x))
		;

	return x;
}

static unsigned long clear_multiples(unsigned long x,
				     unsigned long *p,
				     unsigned long start,
				     unsigned long end)
{
	unsigned long m;

	m = 2 * x;
	if (m < start)
		m = roundup(start, x);

	while (m < end) {
		__clear_bit(m, p);
		m += x;
	}

	return x;
}

static bool expand_to_next_prime(unsigned long x)
{
	const struct primes *p;
	struct primes *new;
	unsigned long sz, y;

	 
	sz = 2 * x;
	if (sz < x)
		return false;

	sz = round_up(sz, BITS_PER_LONG);
	new = kmalloc(sizeof(*new) + bitmap_size(sz),
		      GFP_KERNEL | __GFP_NOWARN);
	if (!new)
		return false;

	mutex_lock(&lock);
	p = rcu_dereference_protected(primes, lockdep_is_held(&lock));
	if (x < p->last) {
		kfree(new);
		goto unlock;
	}

	 
	bitmap_fill(new->primes, sz);
	bitmap_copy(new->primes, p->primes, p->sz);
	for (y = 2UL; y < sz; y = find_next_bit(new->primes, sz, y + 1))
		new->last = clear_multiples(y, new->primes, p->sz, sz);
	new->sz = sz;

	BUG_ON(new->last <= x);

	rcu_assign_pointer(primes, new);
	if (p != &small_primes)
		kfree_rcu((struct primes *)p, rcu);

unlock:
	mutex_unlock(&lock);
	return true;
}

static void free_primes(void)
{
	const struct primes *p;

	mutex_lock(&lock);
	p = rcu_dereference_protected(primes, lockdep_is_held(&lock));
	if (p != &small_primes) {
		rcu_assign_pointer(primes, &small_primes);
		kfree_rcu((struct primes *)p, rcu);
	}
	mutex_unlock(&lock);
}

 
unsigned long next_prime_number(unsigned long x)
{
	const struct primes *p;

	rcu_read_lock();
	p = rcu_dereference(primes);
	while (x >= p->last) {
		rcu_read_unlock();

		if (!expand_to_next_prime(x))
			return slow_next_prime_number(x);

		rcu_read_lock();
		p = rcu_dereference(primes);
	}
	x = find_next_bit(p->primes, p->last, x + 1);
	rcu_read_unlock();

	return x;
}
EXPORT_SYMBOL(next_prime_number);

 
bool is_prime_number(unsigned long x)
{
	const struct primes *p;
	bool result;

	rcu_read_lock();
	p = rcu_dereference(primes);
	while (x >= p->sz) {
		rcu_read_unlock();

		if (!expand_to_next_prime(x))
			return slow_is_prime_number(x);

		rcu_read_lock();
		p = rcu_dereference(primes);
	}
	result = test_bit(x, p->primes);
	rcu_read_unlock();

	return result;
}
EXPORT_SYMBOL(is_prime_number);

static void dump_primes(void)
{
	const struct primes *p;
	char *buf;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);

	rcu_read_lock();
	p = rcu_dereference(primes);

	if (buf)
		bitmap_print_to_pagebuf(true, buf, p->primes, p->sz);
	pr_info("primes.{last=%lu, .sz=%lu, .primes[]=...x%lx} = %s\n",
		p->last, p->sz, p->primes[BITS_TO_LONGS(p->sz) - 1], buf);

	rcu_read_unlock();

	kfree(buf);
}

static int selftest(unsigned long max)
{
	unsigned long x, last;

	if (!max)
		return 0;

	for (last = 0, x = 2; x < max; x++) {
		bool slow = slow_is_prime_number(x);
		bool fast = is_prime_number(x);

		if (slow != fast) {
			pr_err("inconsistent result for is-prime(%lu): slow=%s, fast=%s!\n",
			       x, slow ? "yes" : "no", fast ? "yes" : "no");
			goto err;
		}

		if (!slow)
			continue;

		if (next_prime_number(last) != x) {
			pr_err("incorrect result for next-prime(%lu): expected %lu, got %lu\n",
			       last, x, next_prime_number(last));
			goto err;
		}
		last = x;
	}

	pr_info("%s(%lu) passed, last prime was %lu\n", __func__, x, last);
	return 0;

err:
	dump_primes();
	return -EINVAL;
}

static int __init primes_init(void)
{
	return selftest(selftest_max);
}

static void __exit primes_exit(void)
{
	free_primes();
}

module_init(primes_init);
module_exit(primes_exit);

module_param_named(selftest, selftest_max, ulong, 0400);

MODULE_AUTHOR("Intel Corporation");
MODULE_LICENSE("GPL");
