
 

#include <kunit/test.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/kfence.h>
#include <linux/mm.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/tracepoint.h>
#include <trace/events/printk.h>

#include <asm/kfence.h>

#include "kfence.h"

 
#ifndef arch_kfence_test_address
#define arch_kfence_test_address(addr) (addr)
#endif

#define KFENCE_TEST_REQUIRES(test, cond) do {			\
	if (!(cond))						\
		kunit_skip((test), "Test requires: " #cond);	\
} while (0)

 
static struct {
	spinlock_t lock;
	int nlines;
	char lines[2][256];
} observed = {
	.lock = __SPIN_LOCK_UNLOCKED(observed.lock),
};

 
static void probe_console(void *ignore, const char *buf, size_t len)
{
	unsigned long flags;
	int nlines;

	spin_lock_irqsave(&observed.lock, flags);
	nlines = observed.nlines;

	if (strnstr(buf, "BUG: KFENCE: ", len) && strnstr(buf, "test_", len)) {
		 
		strscpy(observed.lines[0], buf, min(len + 1, sizeof(observed.lines[0])));
		nlines = 1;
	} else if (nlines == 1 && (strnstr(buf, "at 0x", len) || strnstr(buf, "of 0x", len))) {
		strscpy(observed.lines[nlines++], buf, min(len + 1, sizeof(observed.lines[0])));
	}

	WRITE_ONCE(observed.nlines, nlines);  
	spin_unlock_irqrestore(&observed.lock, flags);
}

 
static bool report_available(void)
{
	return READ_ONCE(observed.nlines) == ARRAY_SIZE(observed.lines);
}

 
struct expect_report {
	enum kfence_error_type type;  
	void *fn;  
	char *addr;  
	bool is_write;  
};

static const char *get_access_type(const struct expect_report *r)
{
	return r->is_write ? "write" : "read";
}

 
static bool report_matches(const struct expect_report *r)
{
	unsigned long addr = (unsigned long)r->addr;
	bool ret = false;
	unsigned long flags;
	typeof(observed.lines) expect;
	const char *end;
	char *cur;

	 
	if (!report_available())
		return false;

	 

	 
	cur = expect[0];
	end = &expect[0][sizeof(expect[0]) - 1];
	switch (r->type) {
	case KFENCE_ERROR_OOB:
		cur += scnprintf(cur, end - cur, "BUG: KFENCE: out-of-bounds %s",
				 get_access_type(r));
		break;
	case KFENCE_ERROR_UAF:
		cur += scnprintf(cur, end - cur, "BUG: KFENCE: use-after-free %s",
				 get_access_type(r));
		break;
	case KFENCE_ERROR_CORRUPTION:
		cur += scnprintf(cur, end - cur, "BUG: KFENCE: memory corruption");
		break;
	case KFENCE_ERROR_INVALID:
		cur += scnprintf(cur, end - cur, "BUG: KFENCE: invalid %s",
				 get_access_type(r));
		break;
	case KFENCE_ERROR_INVALID_FREE:
		cur += scnprintf(cur, end - cur, "BUG: KFENCE: invalid free");
		break;
	}

	scnprintf(cur, end - cur, " in %pS", r->fn);
	 
	cur = strchr(expect[0], '+');
	if (cur)
		*cur = '\0';

	 
	cur = expect[1];
	end = &expect[1][sizeof(expect[1]) - 1];

	switch (r->type) {
	case KFENCE_ERROR_OOB:
		cur += scnprintf(cur, end - cur, "Out-of-bounds %s at", get_access_type(r));
		addr = arch_kfence_test_address(addr);
		break;
	case KFENCE_ERROR_UAF:
		cur += scnprintf(cur, end - cur, "Use-after-free %s at", get_access_type(r));
		addr = arch_kfence_test_address(addr);
		break;
	case KFENCE_ERROR_CORRUPTION:
		cur += scnprintf(cur, end - cur, "Corrupted memory at");
		break;
	case KFENCE_ERROR_INVALID:
		cur += scnprintf(cur, end - cur, "Invalid %s at", get_access_type(r));
		addr = arch_kfence_test_address(addr);
		break;
	case KFENCE_ERROR_INVALID_FREE:
		cur += scnprintf(cur, end - cur, "Invalid free of");
		break;
	}

	cur += scnprintf(cur, end - cur, " 0x%p", (void *)addr);

	spin_lock_irqsave(&observed.lock, flags);
	if (!report_available())
		goto out;  

	 
	ret = strstr(observed.lines[0], expect[0]) && strstr(observed.lines[1], expect[1]);
out:
	spin_unlock_irqrestore(&observed.lock, flags);
	return ret;
}

 

#define TEST_PRIV_WANT_MEMCACHE ((void *)1)

 
static struct kmem_cache *test_cache;

static size_t setup_test_cache(struct kunit *test, size_t size, slab_flags_t flags,
			       void (*ctor)(void *))
{
	if (test->priv != TEST_PRIV_WANT_MEMCACHE)
		return size;

	kunit_info(test, "%s: size=%zu, ctor=%ps\n", __func__, size, ctor);

	 
	flags |= SLAB_NO_MERGE | SLAB_ACCOUNT;
	test_cache = kmem_cache_create("test", size, 1, flags, ctor);
	KUNIT_ASSERT_TRUE_MSG(test, test_cache, "could not create cache");

	return size;
}

static void test_cache_destroy(void)
{
	if (!test_cache)
		return;

	kmem_cache_destroy(test_cache);
	test_cache = NULL;
}

static inline size_t kmalloc_cache_alignment(size_t size)
{
	 
	enum kmalloc_cache_type type = kmalloc_type(GFP_KERNEL, 0);
	return kmalloc_caches[type][__kmalloc_index(size, false)]->align;
}

 
static __always_inline void test_free(void *ptr)
{
	if (test_cache)
		kmem_cache_free(test_cache, ptr);
	else
		kfree(ptr);
}

 
enum allocation_policy {
	ALLOCATE_ANY,  
	ALLOCATE_LEFT,  
	ALLOCATE_RIGHT,  
	ALLOCATE_NONE,  
};

 
static void *test_alloc(struct kunit *test, size_t size, gfp_t gfp, enum allocation_policy policy)
{
	void *alloc;
	unsigned long timeout, resched_after;
	const char *policy_name;

	switch (policy) {
	case ALLOCATE_ANY:
		policy_name = "any";
		break;
	case ALLOCATE_LEFT:
		policy_name = "left";
		break;
	case ALLOCATE_RIGHT:
		policy_name = "right";
		break;
	case ALLOCATE_NONE:
		policy_name = "none";
		break;
	}

	kunit_info(test, "%s: size=%zu, gfp=%x, policy=%s, cache=%i\n", __func__, size, gfp,
		   policy_name, !!test_cache);

	 
	timeout = jiffies + msecs_to_jiffies(100 * kfence_sample_interval);
	 
	resched_after = jiffies + msecs_to_jiffies(kfence_sample_interval);
	do {
		if (test_cache)
			alloc = kmem_cache_alloc(test_cache, gfp);
		else
			alloc = kmalloc(size, gfp);

		if (is_kfence_address(alloc)) {
			struct slab *slab = virt_to_slab(alloc);
			enum kmalloc_cache_type type = kmalloc_type(GFP_KERNEL, _RET_IP_);
			struct kmem_cache *s = test_cache ?:
					kmalloc_caches[type][__kmalloc_index(size, false)];

			 
			KUNIT_EXPECT_EQ(test, obj_to_index(s, slab, alloc), 0U);
			KUNIT_EXPECT_EQ(test, objs_per_slab(s, slab), 1);

			if (policy == ALLOCATE_ANY)
				return alloc;
			if (policy == ALLOCATE_LEFT && PAGE_ALIGNED(alloc))
				return alloc;
			if (policy == ALLOCATE_RIGHT && !PAGE_ALIGNED(alloc))
				return alloc;
		} else if (policy == ALLOCATE_NONE)
			return alloc;

		test_free(alloc);

		if (time_after(jiffies, resched_after))
			cond_resched();
	} while (time_before(jiffies, timeout));

	KUNIT_ASSERT_TRUE_MSG(test, false, "failed to allocate from KFENCE");
	return NULL;  
}

static void test_out_of_bounds_read(struct kunit *test)
{
	size_t size = 32;
	struct expect_report expect = {
		.type = KFENCE_ERROR_OOB,
		.fn = test_out_of_bounds_read,
		.is_write = false,
	};
	char *buf;

	setup_test_cache(test, size, 0, NULL);

	 
	if (!test_cache)
		size = kmalloc_cache_alignment(size);

	 

	buf = test_alloc(test, size, GFP_KERNEL, ALLOCATE_LEFT);
	expect.addr = buf - 1;
	READ_ONCE(*expect.addr);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
	test_free(buf);

	buf = test_alloc(test, size, GFP_KERNEL, ALLOCATE_RIGHT);
	expect.addr = buf + size;
	READ_ONCE(*expect.addr);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
	test_free(buf);
}

static void test_out_of_bounds_write(struct kunit *test)
{
	size_t size = 32;
	struct expect_report expect = {
		.type = KFENCE_ERROR_OOB,
		.fn = test_out_of_bounds_write,
		.is_write = true,
	};
	char *buf;

	setup_test_cache(test, size, 0, NULL);
	buf = test_alloc(test, size, GFP_KERNEL, ALLOCATE_LEFT);
	expect.addr = buf - 1;
	WRITE_ONCE(*expect.addr, 42);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
	test_free(buf);
}

static void test_use_after_free_read(struct kunit *test)
{
	const size_t size = 32;
	struct expect_report expect = {
		.type = KFENCE_ERROR_UAF,
		.fn = test_use_after_free_read,
		.is_write = false,
	};

	setup_test_cache(test, size, 0, NULL);
	expect.addr = test_alloc(test, size, GFP_KERNEL, ALLOCATE_ANY);
	test_free(expect.addr);
	READ_ONCE(*expect.addr);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

static void test_double_free(struct kunit *test)
{
	const size_t size = 32;
	struct expect_report expect = {
		.type = KFENCE_ERROR_INVALID_FREE,
		.fn = test_double_free,
	};

	setup_test_cache(test, size, 0, NULL);
	expect.addr = test_alloc(test, size, GFP_KERNEL, ALLOCATE_ANY);
	test_free(expect.addr);
	test_free(expect.addr);  
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

static void test_invalid_addr_free(struct kunit *test)
{
	const size_t size = 32;
	struct expect_report expect = {
		.type = KFENCE_ERROR_INVALID_FREE,
		.fn = test_invalid_addr_free,
	};
	char *buf;

	setup_test_cache(test, size, 0, NULL);
	buf = test_alloc(test, size, GFP_KERNEL, ALLOCATE_ANY);
	expect.addr = buf + 1;  
	test_free(expect.addr);  
	test_free(buf);  
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

static void test_corruption(struct kunit *test)
{
	size_t size = 32;
	struct expect_report expect = {
		.type = KFENCE_ERROR_CORRUPTION,
		.fn = test_corruption,
	};
	char *buf;

	setup_test_cache(test, size, 0, NULL);

	 

	buf = test_alloc(test, size, GFP_KERNEL, ALLOCATE_LEFT);
	expect.addr = buf + size;
	WRITE_ONCE(*expect.addr, 42);
	test_free(buf);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));

	buf = test_alloc(test, size, GFP_KERNEL, ALLOCATE_RIGHT);
	expect.addr = buf - 1;
	WRITE_ONCE(*expect.addr, 42);
	test_free(buf);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

 
static void test_kmalloc_aligned_oob_read(struct kunit *test)
{
	const size_t size = 73;
	const size_t align = kmalloc_cache_alignment(size);
	struct expect_report expect = {
		.type = KFENCE_ERROR_OOB,
		.fn = test_kmalloc_aligned_oob_read,
		.is_write = false,
	};
	char *buf;

	buf = test_alloc(test, size, GFP_KERNEL, ALLOCATE_RIGHT);

	 
	READ_ONCE(*(buf - 1));
	KUNIT_EXPECT_FALSE(test, report_available());

	 
	READ_ONCE(*(buf + size));
	KUNIT_EXPECT_FALSE(test, report_available());

	 
	expect.addr = buf + size + align;
	READ_ONCE(*expect.addr);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));

	test_free(buf);
}

static void test_kmalloc_aligned_oob_write(struct kunit *test)
{
	const size_t size = 73;
	struct expect_report expect = {
		.type = KFENCE_ERROR_CORRUPTION,
		.fn = test_kmalloc_aligned_oob_write,
	};
	char *buf;

	buf = test_alloc(test, size, GFP_KERNEL, ALLOCATE_RIGHT);
	 
	expect.addr = buf + size;
	WRITE_ONCE(*expect.addr, READ_ONCE(*expect.addr) + 1);
	KUNIT_EXPECT_FALSE(test, report_available());
	test_free(buf);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

 
static void test_shrink_memcache(struct kunit *test)
{
	const size_t size = 32;
	void *buf;

	setup_test_cache(test, size, 0, NULL);
	KUNIT_EXPECT_TRUE(test, test_cache);
	buf = test_alloc(test, size, GFP_KERNEL, ALLOCATE_ANY);
	kmem_cache_shrink(test_cache);
	test_free(buf);

	KUNIT_EXPECT_FALSE(test, report_available());
}

static void ctor_set_x(void *obj)
{
	 
	memset(obj, 'x', 8);
}

 
static void test_free_bulk(struct kunit *test)
{
	int iter;

	for (iter = 0; iter < 5; iter++) {
		const size_t size = setup_test_cache(test, get_random_u32_inclusive(8, 307),
						     0, (iter & 1) ? ctor_set_x : NULL);
		void *objects[] = {
			test_alloc(test, size, GFP_KERNEL, ALLOCATE_RIGHT),
			test_alloc(test, size, GFP_KERNEL, ALLOCATE_NONE),
			test_alloc(test, size, GFP_KERNEL, ALLOCATE_LEFT),
			test_alloc(test, size, GFP_KERNEL, ALLOCATE_NONE),
			test_alloc(test, size, GFP_KERNEL, ALLOCATE_NONE),
		};

		kmem_cache_free_bulk(test_cache, ARRAY_SIZE(objects), objects);
		KUNIT_ASSERT_FALSE(test, report_available());
		test_cache_destroy();
	}
}

 
static void test_init_on_free(struct kunit *test)
{
	const size_t size = 32;
	struct expect_report expect = {
		.type = KFENCE_ERROR_UAF,
		.fn = test_init_on_free,
		.is_write = false,
	};
	int i;

	KFENCE_TEST_REQUIRES(test, IS_ENABLED(CONFIG_INIT_ON_FREE_DEFAULT_ON));
	 

	setup_test_cache(test, size, 0, NULL);
	expect.addr = test_alloc(test, size, GFP_KERNEL, ALLOCATE_ANY);
	for (i = 0; i < size; i++)
		expect.addr[i] = i + 1;
	test_free(expect.addr);

	for (i = 0; i < size; i++) {
		 
		KUNIT_EXPECT_EQ(test, expect.addr[i], (char)0);

		if (!i)  
			KUNIT_EXPECT_TRUE(test, report_matches(&expect));
	}
}

 
static void test_memcache_ctor(struct kunit *test)
{
	const size_t size = 32;
	char *buf;
	int i;

	setup_test_cache(test, size, 0, ctor_set_x);
	buf = test_alloc(test, size, GFP_KERNEL, ALLOCATE_ANY);

	for (i = 0; i < 8; i++)
		KUNIT_EXPECT_EQ(test, buf[i], (char)'x');

	test_free(buf);

	KUNIT_EXPECT_FALSE(test, report_available());
}

 
static void test_gfpzero(struct kunit *test)
{
	const size_t size = PAGE_SIZE;  
	char *buf1, *buf2;
	int i;

	 
	KFENCE_TEST_REQUIRES(test, kfence_sample_interval <= 100);

	setup_test_cache(test, size, 0, NULL);
	buf1 = test_alloc(test, size, GFP_KERNEL, ALLOCATE_ANY);
	for (i = 0; i < size; i++)
		buf1[i] = i + 1;
	test_free(buf1);

	 
	for (i = 0;; i++) {
		buf2 = test_alloc(test, size, GFP_KERNEL | __GFP_ZERO, ALLOCATE_ANY);
		if (buf1 == buf2)
			break;
		test_free(buf2);

		if (kthread_should_stop() || (i == CONFIG_KFENCE_NUM_OBJECTS)) {
			kunit_warn(test, "giving up ... cannot get same object back\n");
			return;
		}
		cond_resched();
	}

	for (i = 0; i < size; i++)
		KUNIT_EXPECT_EQ(test, buf2[i], (char)0);

	test_free(buf2);

	KUNIT_EXPECT_FALSE(test, report_available());
}

static void test_invalid_access(struct kunit *test)
{
	const struct expect_report expect = {
		.type = KFENCE_ERROR_INVALID,
		.fn = test_invalid_access,
		.addr = &__kfence_pool[10],
		.is_write = false,
	};

	READ_ONCE(__kfence_pool[10]);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

 
static void test_memcache_typesafe_by_rcu(struct kunit *test)
{
	const size_t size = 32;
	struct expect_report expect = {
		.type = KFENCE_ERROR_UAF,
		.fn = test_memcache_typesafe_by_rcu,
		.is_write = false,
	};

	setup_test_cache(test, size, SLAB_TYPESAFE_BY_RCU, NULL);
	KUNIT_EXPECT_TRUE(test, test_cache);  

	expect.addr = test_alloc(test, size, GFP_KERNEL, ALLOCATE_ANY);
	*expect.addr = 42;

	rcu_read_lock();
	test_free(expect.addr);
	KUNIT_EXPECT_EQ(test, *expect.addr, (char)42);
	 
	rcu_read_unlock();

	 
	KUNIT_EXPECT_FALSE(test, report_available());

	 
	rcu_barrier();

	 
	KUNIT_EXPECT_EQ(test, *expect.addr, (char)42);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

 
static void test_krealloc(struct kunit *test)
{
	const size_t size = 32;
	const struct expect_report expect = {
		.type = KFENCE_ERROR_UAF,
		.fn = test_krealloc,
		.addr = test_alloc(test, size, GFP_KERNEL, ALLOCATE_ANY),
		.is_write = false,
	};
	char *buf = expect.addr;
	int i;

	KUNIT_EXPECT_FALSE(test, test_cache);
	KUNIT_EXPECT_EQ(test, ksize(buf), size);  
	for (i = 0; i < size; i++)
		buf[i] = i + 1;

	 
	buf = krealloc(buf, size * 3, GFP_KERNEL);  
	 
	KUNIT_EXPECT_GE(test, ksize(buf), size * 3);
	for (i = 0; i < size; i++)
		KUNIT_EXPECT_EQ(test, buf[i], (char)(i + 1));
	for (; i < size * 3; i++)  
		buf[i] = i + 1;

	buf = krealloc(buf, size * 2, GFP_KERNEL);  
	KUNIT_EXPECT_GE(test, ksize(buf), size * 2);
	for (i = 0; i < size * 2; i++)
		KUNIT_EXPECT_EQ(test, buf[i], (char)(i + 1));

	buf = krealloc(buf, 0, GFP_KERNEL);  
	KUNIT_EXPECT_EQ(test, (unsigned long)buf, (unsigned long)ZERO_SIZE_PTR);
	KUNIT_ASSERT_FALSE(test, report_available());  

	READ_ONCE(*expect.addr);  
	KUNIT_ASSERT_TRUE(test, report_matches(&expect));
}

 
static void test_memcache_alloc_bulk(struct kunit *test)
{
	const size_t size = 32;
	bool pass = false;
	unsigned long timeout;

	setup_test_cache(test, size, 0, NULL);
	KUNIT_EXPECT_TRUE(test, test_cache);  
	 
	timeout = jiffies + msecs_to_jiffies(100 * kfence_sample_interval);
	do {
		void *objects[100];
		int i, num = kmem_cache_alloc_bulk(test_cache, GFP_ATOMIC, ARRAY_SIZE(objects),
						   objects);
		if (!num)
			continue;
		for (i = 0; i < ARRAY_SIZE(objects); i++) {
			if (is_kfence_address(objects[i])) {
				pass = true;
				break;
			}
		}
		kmem_cache_free_bulk(test_cache, num, objects);
		 
		cond_resched();
	} while (!pass && time_before(jiffies, timeout));

	KUNIT_EXPECT_TRUE(test, pass);
	KUNIT_EXPECT_FALSE(test, report_available());
}

 
#define KFENCE_KUNIT_CASE(test_name)						\
	{ .run_case = test_name, .name = #test_name },				\
	{ .run_case = test_name, .name = #test_name "-memcache" }

static struct kunit_case kfence_test_cases[] = {
	KFENCE_KUNIT_CASE(test_out_of_bounds_read),
	KFENCE_KUNIT_CASE(test_out_of_bounds_write),
	KFENCE_KUNIT_CASE(test_use_after_free_read),
	KFENCE_KUNIT_CASE(test_double_free),
	KFENCE_KUNIT_CASE(test_invalid_addr_free),
	KFENCE_KUNIT_CASE(test_corruption),
	KFENCE_KUNIT_CASE(test_free_bulk),
	KFENCE_KUNIT_CASE(test_init_on_free),
	KUNIT_CASE(test_kmalloc_aligned_oob_read),
	KUNIT_CASE(test_kmalloc_aligned_oob_write),
	KUNIT_CASE(test_shrink_memcache),
	KUNIT_CASE(test_memcache_ctor),
	KUNIT_CASE(test_invalid_access),
	KUNIT_CASE(test_gfpzero),
	KUNIT_CASE(test_memcache_typesafe_by_rcu),
	KUNIT_CASE(test_krealloc),
	KUNIT_CASE(test_memcache_alloc_bulk),
	{},
};

 

static int test_init(struct kunit *test)
{
	unsigned long flags;
	int i;

	if (!__kfence_pool)
		return -EINVAL;

	spin_lock_irqsave(&observed.lock, flags);
	for (i = 0; i < ARRAY_SIZE(observed.lines); i++)
		observed.lines[i][0] = '\0';
	observed.nlines = 0;
	spin_unlock_irqrestore(&observed.lock, flags);

	 
	if (strstr(test->name, "memcache"))
		test->priv = TEST_PRIV_WANT_MEMCACHE;
	else
		test->priv = NULL;

	return 0;
}

static void test_exit(struct kunit *test)
{
	test_cache_destroy();
}

static int kfence_suite_init(struct kunit_suite *suite)
{
	register_trace_console(probe_console, NULL);
	return 0;
}

static void kfence_suite_exit(struct kunit_suite *suite)
{
	unregister_trace_console(probe_console, NULL);
	tracepoint_synchronize_unregister();
}

static struct kunit_suite kfence_test_suite = {
	.name = "kfence",
	.test_cases = kfence_test_cases,
	.init = test_init,
	.exit = test_exit,
	.suite_init = kfence_suite_init,
	.suite_exit = kfence_suite_exit,
};

kunit_test_suites(&kfence_test_suite);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Alexander Potapenko <glider@google.com>, Marco Elver <elver@google.com>");
