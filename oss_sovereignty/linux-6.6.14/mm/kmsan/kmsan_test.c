
 

#include <kunit/test.h>
#include "kmsan.h"

#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/kmsan.h>
#include <linux/mm.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/tracepoint.h>
#include <linux/vmalloc.h>
#include <trace/events/printk.h>

static DEFINE_PER_CPU(int, per_cpu_var);

 
static struct {
	spinlock_t lock;
	bool available;
	bool ignore;  
	char header[256];
} observed = {
	.lock = __SPIN_LOCK_UNLOCKED(observed.lock),
};

 
static void probe_console(void *ignore, const char *buf, size_t len)
{
	unsigned long flags;

	if (observed.ignore)
		return;
	spin_lock_irqsave(&observed.lock, flags);

	if (strnstr(buf, "BUG: KMSAN: ", len)) {
		 
		strscpy(observed.header, buf,
			min(len + 1, sizeof(observed.header)));
		WRITE_ONCE(observed.available, true);
		observed.ignore = true;
	}
	spin_unlock_irqrestore(&observed.lock, flags);
}

 
static bool report_available(void)
{
	return READ_ONCE(observed.available);
}

 
struct expect_report {
	const char *error_type;  
	 
	const char *symbol;
};

 
static bool report_matches(const struct expect_report *r)
{
	typeof(observed.header) expected_header;
	unsigned long flags;
	bool ret = false;
	const char *end;
	char *cur;

	 
	if (!report_available() || !r->symbol)
		return (!report_available() && !r->symbol);

	 

	 
	cur = expected_header;
	end = &expected_header[sizeof(expected_header) - 1];

	cur += scnprintf(cur, end - cur, "BUG: KMSAN: %s", r->error_type);

	scnprintf(cur, end - cur, " in %s", r->symbol);
	 
	cur = strchr(expected_header, '+');
	if (cur)
		*cur = '\0';

	spin_lock_irqsave(&observed.lock, flags);
	if (!report_available())
		goto out;  

	 
	ret = strstr(observed.header, expected_header);
out:
	spin_unlock_irqrestore(&observed.lock, flags);

	return ret;
}

 

 
static noinline void check_true(char *arg)
{
	pr_info("%s is true\n", arg);
}

static noinline void check_false(char *arg)
{
	pr_info("%s is false\n", arg);
}

#define USE(x)                           \
	do {                             \
		if (x)                   \
			check_true(#x);  \
		else                     \
			check_false(#x); \
	} while (0)

#define EXPECTATION_ETYPE_FN(e, reason, fn) \
	struct expect_report e = {          \
		.error_type = reason,       \
		.symbol = fn,               \
	}

#define EXPECTATION_NO_REPORT(e) EXPECTATION_ETYPE_FN(e, NULL, NULL)
#define EXPECTATION_UNINIT_VALUE_FN(e, fn) \
	EXPECTATION_ETYPE_FN(e, "uninit-value", fn)
#define EXPECTATION_UNINIT_VALUE(e) EXPECTATION_UNINIT_VALUE_FN(e, __func__)
#define EXPECTATION_USE_AFTER_FREE(e) \
	EXPECTATION_ETYPE_FN(e, "use-after-free", __func__)

 
static void test_uninit_kmalloc(struct kunit *test)
{
	EXPECTATION_UNINIT_VALUE(expect);
	int *ptr;

	kunit_info(test, "uninitialized kmalloc test (UMR report)\n");
	ptr = kmalloc(sizeof(*ptr), GFP_KERNEL);
	USE(*ptr);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

 
static void test_init_kmalloc(struct kunit *test)
{
	EXPECTATION_NO_REPORT(expect);
	int *ptr;

	kunit_info(test, "initialized kmalloc test (no reports)\n");
	ptr = kmalloc(sizeof(*ptr), GFP_KERNEL);
	memset(ptr, 0, sizeof(*ptr));
	USE(*ptr);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

 
static void test_init_kzalloc(struct kunit *test)
{
	EXPECTATION_NO_REPORT(expect);
	int *ptr;

	kunit_info(test, "initialized kzalloc test (no reports)\n");
	ptr = kzalloc(sizeof(*ptr), GFP_KERNEL);
	USE(*ptr);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

 
static void test_uninit_stack_var(struct kunit *test)
{
	EXPECTATION_UNINIT_VALUE(expect);
	volatile int cond;

	kunit_info(test, "uninitialized stack variable (UMR report)\n");
	USE(cond);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

 
static void test_init_stack_var(struct kunit *test)
{
	EXPECTATION_NO_REPORT(expect);
	volatile int cond = 1;

	kunit_info(test, "initialized stack variable (no reports)\n");
	USE(cond);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

static noinline void two_param_fn_2(int arg1, int arg2)
{
	USE(arg1);
	USE(arg2);
}

static noinline void one_param_fn(int arg)
{
	two_param_fn_2(arg, arg);
	USE(arg);
}

static noinline void two_param_fn(int arg1, int arg2)
{
	int init = 0;

	one_param_fn(init);
	USE(arg1);
	USE(arg2);
}

static void test_params(struct kunit *test)
{
#ifdef CONFIG_KMSAN_CHECK_PARAM_RETVAL
	 
	EXPECTATION_UNINIT_VALUE_FN(expect, "test_params");
#else
	EXPECTATION_UNINIT_VALUE_FN(expect, "two_param_fn");
#endif
	volatile int uninit, init = 1;

	kunit_info(test,
		   "uninit passed through a function parameter (UMR report)\n");
	two_param_fn(uninit, init);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

static int signed_sum3(int a, int b, int c)
{
	return a + b + c;
}

 
static void test_uninit_multiple_params(struct kunit *test)
{
	EXPECTATION_UNINIT_VALUE(expect);
	volatile char b = 3, c;
	volatile int a;

	kunit_info(test, "uninitialized local passed to fn (UMR report)\n");
	USE(signed_sum3(a, b, c));
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

 
static noinline void do_uninit_local_array(char *array, int start, int stop)
{
	volatile char uninit;

	for (int i = start; i < stop; i++)
		array[i] = uninit;
}

 
static void test_uninit_kmsan_check_memory(struct kunit *test)
{
	EXPECTATION_UNINIT_VALUE_FN(expect, "test_uninit_kmsan_check_memory");
	volatile char local_array[8];

	kunit_info(
		test,
		"kmsan_check_memory() called on uninit local (UMR report)\n");
	do_uninit_local_array((char *)local_array, 5, 7);

	kmsan_check_memory((char *)local_array, 8);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

 
static void test_init_kmsan_vmap_vunmap(struct kunit *test)
{
	EXPECTATION_NO_REPORT(expect);
	const int npages = 2;
	struct page **pages;
	void *vbuf;

	kunit_info(test, "pages initialized via vmap (no reports)\n");

	pages = kmalloc_array(npages, sizeof(*pages), GFP_KERNEL);
	for (int i = 0; i < npages; i++)
		pages[i] = alloc_page(GFP_KERNEL);
	vbuf = vmap(pages, npages, VM_MAP, PAGE_KERNEL);
	memset(vbuf, 0xfe, npages * PAGE_SIZE);
	for (int i = 0; i < npages; i++)
		kmsan_check_memory(page_address(pages[i]), PAGE_SIZE);

	if (vbuf)
		vunmap(vbuf);
	for (int i = 0; i < npages; i++) {
		if (pages[i])
			__free_page(pages[i]);
	}
	kfree(pages);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

 
static void test_init_vmalloc(struct kunit *test)
{
	EXPECTATION_NO_REPORT(expect);
	int npages = 8;
	char *buf;

	kunit_info(test, "vmalloc buffer can be initialized (no reports)\n");
	buf = vmalloc(PAGE_SIZE * npages);
	buf[0] = 1;
	memset(buf, 0xfe, PAGE_SIZE * npages);
	USE(buf[0]);
	for (int i = 0; i < npages; i++)
		kmsan_check_memory(&buf[PAGE_SIZE * i], PAGE_SIZE);
	vfree(buf);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

 
static void test_uaf(struct kunit *test)
{
	EXPECTATION_USE_AFTER_FREE(expect);
	volatile int value;
	volatile int *var;

	kunit_info(test, "use-after-free in kmalloc-ed buffer (UMR report)\n");
	var = kmalloc(80, GFP_KERNEL);
	var[3] = 0xfeedface;
	kfree((int *)var);
	 
	value = var[3];
	USE(value);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

 
static void test_percpu_propagate(struct kunit *test)
{
	EXPECTATION_UNINIT_VALUE(expect);
	volatile int uninit, check;

	kunit_info(test,
		   "uninit local stored to per_cpu memory (UMR report)\n");

	this_cpu_write(per_cpu_var, uninit);
	check = this_cpu_read(per_cpu_var);
	USE(check);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

 
static void test_printk(struct kunit *test)
{
#ifdef CONFIG_KMSAN_CHECK_PARAM_RETVAL
	 
	EXPECTATION_UNINIT_VALUE_FN(expect, "test_printk");
#else
	EXPECTATION_UNINIT_VALUE_FN(expect, "number");
#endif
	volatile int uninit;

	kunit_info(test, "uninit local passed to pr_info() (UMR report)\n");
	pr_info("%px contains %d\n", &uninit, uninit);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

 
#define DO_NOT_OPTIMIZE(var) barrier()

 
static void test_init_memcpy(struct kunit *test)
{
	EXPECTATION_NO_REPORT(expect);
	volatile int src;
	volatile int dst = 0;

	DO_NOT_OPTIMIZE(src);
	src = 1;
	kunit_info(
		test,
		"memcpy()ing aligned initialized src to aligned dst (no reports)\n");
	memcpy((void *)&dst, (void *)&src, sizeof(src));
	kmsan_check_memory((void *)&dst, sizeof(dst));
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

 
static void test_memcpy_aligned_to_aligned(struct kunit *test)
{
	EXPECTATION_UNINIT_VALUE_FN(expect, "test_memcpy_aligned_to_aligned");
	volatile int uninit_src;
	volatile int dst = 0;

	kunit_info(
		test,
		"memcpy()ing aligned uninit src to aligned dst (UMR report)\n");
	DO_NOT_OPTIMIZE(uninit_src);
	memcpy((void *)&dst, (void *)&uninit_src, sizeof(uninit_src));
	kmsan_check_memory((void *)&dst, sizeof(dst));
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

 
static void test_memcpy_aligned_to_unaligned(struct kunit *test)
{
	EXPECTATION_UNINIT_VALUE_FN(expect, "test_memcpy_aligned_to_unaligned");
	volatile int uninit_src;
	volatile char dst[8] = { 0 };

	kunit_info(
		test,
		"memcpy()ing aligned uninit src to unaligned dst (UMR report)\n");
	DO_NOT_OPTIMIZE(uninit_src);
	memcpy((void *)&dst[1], (void *)&uninit_src, sizeof(uninit_src));
	kmsan_check_memory((void *)dst, 4);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

 
static void test_memcpy_aligned_to_unaligned2(struct kunit *test)
{
	EXPECTATION_UNINIT_VALUE_FN(expect,
				    "test_memcpy_aligned_to_unaligned2");
	volatile int uninit_src;
	volatile char dst[8] = { 0 };

	kunit_info(
		test,
		"memcpy()ing aligned uninit src to unaligned dst - part 2 (UMR report)\n");
	DO_NOT_OPTIMIZE(uninit_src);
	memcpy((void *)&dst[1], (void *)&uninit_src, sizeof(uninit_src));
	kmsan_check_memory((void *)&dst[4], sizeof(uninit_src));
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

 
#define DEFINE_TEST_MEMSETXX(size)                                          \
	static void test_memset##size(struct kunit *test)                   \
	{                                                                   \
		EXPECTATION_NO_REPORT(expect);                              \
		volatile uint##size##_t uninit;                             \
                                                                            \
		kunit_info(test,                                            \
			   "memset" #size "() should initialize memory\n"); \
		DO_NOT_OPTIMIZE(uninit);                                    \
		memset##size((uint##size##_t *)&uninit, 0, 1);              \
		kmsan_check_memory((void *)&uninit, sizeof(uninit));        \
		KUNIT_EXPECT_TRUE(test, report_matches(&expect));           \
	}

DEFINE_TEST_MEMSETXX(16)
DEFINE_TEST_MEMSETXX(32)
DEFINE_TEST_MEMSETXX(64)

static noinline void fibonacci(int *array, int size, int start)
{
	if (start < 2 || (start == size))
		return;
	array[start] = array[start - 1] + array[start - 2];
	fibonacci(array, size, start + 1);
}

static void test_long_origin_chain(struct kunit *test)
{
	EXPECTATION_UNINIT_VALUE_FN(expect, "test_long_origin_chain");
	 
	volatile int accum[KMSAN_MAX_ORIGIN_DEPTH * 2 + 2];
	int last = ARRAY_SIZE(accum) - 1;

	kunit_info(
		test,
		"origin chain exceeding KMSAN_MAX_ORIGIN_DEPTH (UMR report)\n");
	 
	accum[0] = 1;
	fibonacci((int *)accum, ARRAY_SIZE(accum), 2);
	kmsan_check_memory((void *)&accum[last], sizeof(int));
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

 
static void test_stackdepot_roundtrip(struct kunit *test)
{
	unsigned long src_entries[16], *dst_entries;
	unsigned int src_nentries, dst_nentries;
	EXPECTATION_NO_REPORT(expect);
	depot_stack_handle_t handle;

	kunit_info(test, "testing stackdepot roundtrip (no reports)\n");

	src_nentries =
		stack_trace_save(src_entries, ARRAY_SIZE(src_entries), 1);
	handle = stack_depot_save(src_entries, src_nentries, GFP_KERNEL);
	stack_depot_print(handle);
	dst_nentries = stack_depot_fetch(handle, &dst_entries);
	KUNIT_EXPECT_TRUE(test, src_nentries == dst_nentries);

	kmsan_check_memory((void *)dst_entries,
			   sizeof(*dst_entries) * dst_nentries);
	KUNIT_EXPECT_TRUE(test, report_matches(&expect));
}

static struct kunit_case kmsan_test_cases[] = {
	KUNIT_CASE(test_uninit_kmalloc),
	KUNIT_CASE(test_init_kmalloc),
	KUNIT_CASE(test_init_kzalloc),
	KUNIT_CASE(test_uninit_stack_var),
	KUNIT_CASE(test_init_stack_var),
	KUNIT_CASE(test_params),
	KUNIT_CASE(test_uninit_multiple_params),
	KUNIT_CASE(test_uninit_kmsan_check_memory),
	KUNIT_CASE(test_init_kmsan_vmap_vunmap),
	KUNIT_CASE(test_init_vmalloc),
	KUNIT_CASE(test_uaf),
	KUNIT_CASE(test_percpu_propagate),
	KUNIT_CASE(test_printk),
	KUNIT_CASE(test_init_memcpy),
	KUNIT_CASE(test_memcpy_aligned_to_aligned),
	KUNIT_CASE(test_memcpy_aligned_to_unaligned),
	KUNIT_CASE(test_memcpy_aligned_to_unaligned2),
	KUNIT_CASE(test_memset16),
	KUNIT_CASE(test_memset32),
	KUNIT_CASE(test_memset64),
	KUNIT_CASE(test_long_origin_chain),
	KUNIT_CASE(test_stackdepot_roundtrip),
	{},
};

 

static int test_init(struct kunit *test)
{
	unsigned long flags;

	spin_lock_irqsave(&observed.lock, flags);
	observed.header[0] = '\0';
	observed.ignore = false;
	observed.available = false;
	spin_unlock_irqrestore(&observed.lock, flags);

	return 0;
}

static void test_exit(struct kunit *test)
{
}

static int kmsan_suite_init(struct kunit_suite *suite)
{
	register_trace_console(probe_console, NULL);
	return 0;
}

static void kmsan_suite_exit(struct kunit_suite *suite)
{
	unregister_trace_console(probe_console, NULL);
	tracepoint_synchronize_unregister();
}

static struct kunit_suite kmsan_test_suite = {
	.name = "kmsan",
	.test_cases = kmsan_test_cases,
	.init = test_init,
	.exit = test_exit,
	.suite_init = kmsan_suite_init,
	.suite_exit = kmsan_suite_exit,
};
kunit_test_suites(&kmsan_test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alexander Potapenko <glider@google.com>");
