
 

#include <linux/maple_tree.h>
#include <linux/module.h>
#include <linux/rwsem.h>

#define MTREE_ALLOC_MAX 0x2000000000000Ul
#define CONFIG_MAPLE_SEARCH
#define MAPLE_32BIT (MAPLE_NODE_SLOTS > 31)

#ifndef CONFIG_DEBUG_MAPLE_TREE
#define mt_dump(mt, fmt)		do {} while (0)
#define mt_validate(mt)			do {} while (0)
#define mt_cache_shrink()		do {} while (0)
#define mas_dump(mas)			do {} while (0)
#define mas_wr_dump(mas)		do {} while (0)
atomic_t maple_tree_tests_run;
atomic_t maple_tree_tests_passed;
#undef MT_BUG_ON

#define MT_BUG_ON(__tree, __x) do {					\
	atomic_inc(&maple_tree_tests_run);				\
	if (__x) {							\
		pr_info("BUG at %s:%d (%u)\n",				\
		__func__, __LINE__, __x);				\
		pr_info("Pass: %u Run:%u\n",				\
			atomic_read(&maple_tree_tests_passed),		\
			atomic_read(&maple_tree_tests_run));		\
	} else {							\
		atomic_inc(&maple_tree_tests_passed);			\
	}								\
} while (0)
#endif

 
 
 
 
 
 
 
 

#ifdef __KERNEL__
#define mt_set_non_kernel(x)		do {} while (0)
#define mt_zero_nr_tallocated(x)	do {} while (0)
#else
#define cond_resched()			do {} while (0)
#endif
static int __init mtree_insert_index(struct maple_tree *mt,
				     unsigned long index, gfp_t gfp)
{
	return mtree_insert(mt, index, xa_mk_value(index & LONG_MAX), gfp);
}

static void __init mtree_erase_index(struct maple_tree *mt, unsigned long index)
{
	MT_BUG_ON(mt, mtree_erase(mt, index) != xa_mk_value(index & LONG_MAX));
	MT_BUG_ON(mt, mtree_load(mt, index) != NULL);
}

static int __init mtree_test_insert(struct maple_tree *mt, unsigned long index,
				void *ptr)
{
	return mtree_insert(mt, index, ptr, GFP_KERNEL);
}

static int __init mtree_test_store_range(struct maple_tree *mt,
			unsigned long start, unsigned long end, void *ptr)
{
	return mtree_store_range(mt, start, end, ptr, GFP_KERNEL);
}

static int __init mtree_test_store(struct maple_tree *mt, unsigned long start,
				void *ptr)
{
	return mtree_test_store_range(mt, start, start, ptr);
}

static int __init mtree_test_insert_range(struct maple_tree *mt,
			unsigned long start, unsigned long end, void *ptr)
{
	return mtree_insert_range(mt, start, end, ptr, GFP_KERNEL);
}

static void __init *mtree_test_load(struct maple_tree *mt, unsigned long index)
{
	return mtree_load(mt, index);
}

static void __init *mtree_test_erase(struct maple_tree *mt, unsigned long index)
{
	return mtree_erase(mt, index);
}

#if defined(CONFIG_64BIT)
static noinline void __init check_mtree_alloc_range(struct maple_tree *mt,
		unsigned long start, unsigned long end, unsigned long size,
		unsigned long expected, int eret, void *ptr)
{

	unsigned long result = expected + 1;
	int ret;

	ret = mtree_alloc_range(mt, &result, ptr, size, start, end,
			GFP_KERNEL);
	MT_BUG_ON(mt, ret != eret);
	if (ret)
		return;

	MT_BUG_ON(mt, result != expected);
}

static noinline void __init check_mtree_alloc_rrange(struct maple_tree *mt,
		unsigned long start, unsigned long end, unsigned long size,
		unsigned long expected, int eret, void *ptr)
{

	unsigned long result = expected + 1;
	int ret;

	ret = mtree_alloc_rrange(mt, &result, ptr, size, start, end,
			GFP_KERNEL);
	MT_BUG_ON(mt, ret != eret);
	if (ret)
		return;

	MT_BUG_ON(mt, result != expected);
}
#endif

static noinline void __init check_load(struct maple_tree *mt,
				       unsigned long index, void *ptr)
{
	void *ret = mtree_test_load(mt, index);

	if (ret != ptr)
		pr_err("Load %lu returned %p expect %p\n", index, ret, ptr);
	MT_BUG_ON(mt, ret != ptr);
}

static noinline void __init check_store_range(struct maple_tree *mt,
		unsigned long start, unsigned long end, void *ptr, int expected)
{
	int ret = -EINVAL;
	unsigned long i;

	ret = mtree_test_store_range(mt, start, end, ptr);
	MT_BUG_ON(mt, ret != expected);

	if (ret)
		return;

	for (i = start; i <= end; i++)
		check_load(mt, i, ptr);
}

static noinline void __init check_insert_range(struct maple_tree *mt,
		unsigned long start, unsigned long end, void *ptr, int expected)
{
	int ret = -EINVAL;
	unsigned long i;

	ret = mtree_test_insert_range(mt, start, end, ptr);
	MT_BUG_ON(mt, ret != expected);

	if (ret)
		return;

	for (i = start; i <= end; i++)
		check_load(mt, i, ptr);
}

static noinline void __init check_insert(struct maple_tree *mt,
					 unsigned long index, void *ptr)
{
	int ret = -EINVAL;

	ret = mtree_test_insert(mt, index, ptr);
	MT_BUG_ON(mt, ret != 0);
}

static noinline void __init check_dup_insert(struct maple_tree *mt,
				      unsigned long index, void *ptr)
{
	int ret = -EINVAL;

	ret = mtree_test_insert(mt, index, ptr);
	MT_BUG_ON(mt, ret != -EEXIST);
}


static noinline void __init check_index_load(struct maple_tree *mt,
					     unsigned long index)
{
	return check_load(mt, index, xa_mk_value(index & LONG_MAX));
}

static inline __init int not_empty(struct maple_node *node)
{
	int i;

	if (node->parent)
		return 1;

	for (i = 0; i < ARRAY_SIZE(node->slot); i++)
		if (node->slot[i])
			return 1;

	return 0;
}


static noinline void __init check_rev_seq(struct maple_tree *mt,
					  unsigned long max, bool verbose)
{
	unsigned long i = max, j;

	MT_BUG_ON(mt, !mtree_empty(mt));

	mt_zero_nr_tallocated();
	while (i) {
		MT_BUG_ON(mt, mtree_insert_index(mt, i, GFP_KERNEL));
		for (j = i; j <= max; j++)
			check_index_load(mt, j);

		check_load(mt, i - 1, NULL);
		mt_set_in_rcu(mt);
		MT_BUG_ON(mt, !mt_height(mt));
		mt_clear_in_rcu(mt);
		MT_BUG_ON(mt, !mt_height(mt));
		i--;
	}
	check_load(mt, max + 1, NULL);

#ifndef __KERNEL__
	if (verbose) {
		rcu_barrier();
		mt_dump(mt, mt_dump_dec);
		pr_info(" %s test of 0-%lu %luK in %d active (%d total)\n",
			__func__, max, mt_get_alloc_size()/1024, mt_nr_allocated(),
			mt_nr_tallocated());
	}
#endif
}

static noinline void __init check_seq(struct maple_tree *mt, unsigned long max,
		bool verbose)
{
	unsigned long i, j;

	MT_BUG_ON(mt, !mtree_empty(mt));

	mt_zero_nr_tallocated();
	for (i = 0; i <= max; i++) {
		MT_BUG_ON(mt, mtree_insert_index(mt, i, GFP_KERNEL));
		for (j = 0; j <= i; j++)
			check_index_load(mt, j);

		if (i)
			MT_BUG_ON(mt, !mt_height(mt));
		check_load(mt, i + 1, NULL);
	}

#ifndef __KERNEL__
	if (verbose) {
		rcu_barrier();
		mt_dump(mt, mt_dump_dec);
		pr_info(" seq test of 0-%lu %luK in %d active (%d total)\n",
			max, mt_get_alloc_size()/1024, mt_nr_allocated(),
			mt_nr_tallocated());
	}
#endif
}

static noinline void __init check_lb_not_empty(struct maple_tree *mt)
{
	unsigned long i, j;
	unsigned long huge = 4000UL * 1000 * 1000;


	i = huge;
	while (i > 4096) {
		check_insert(mt, i, (void *) i);
		for (j = huge; j >= i; j /= 2) {
			check_load(mt, j-1, NULL);
			check_load(mt, j, (void *) j);
			check_load(mt, j+1, NULL);
		}
		i /= 2;
	}
	mtree_destroy(mt);
}

static noinline void __init check_lower_bound_split(struct maple_tree *mt)
{
	MT_BUG_ON(mt, !mtree_empty(mt));
	check_lb_not_empty(mt);
}

static noinline void __init check_upper_bound_split(struct maple_tree *mt)
{
	unsigned long i, j;
	unsigned long huge;

	MT_BUG_ON(mt, !mtree_empty(mt));

	if (MAPLE_32BIT)
		huge = 2147483647UL;
	else
		huge = 4000UL * 1000 * 1000;

	i = 4096;
	while (i < huge) {
		check_insert(mt, i, (void *) i);
		for (j = i; j >= huge; j *= 2) {
			check_load(mt, j-1, NULL);
			check_load(mt, j, (void *) j);
			check_load(mt, j+1, NULL);
		}
		i *= 2;
	}
	mtree_destroy(mt);
}

static noinline void __init check_mid_split(struct maple_tree *mt)
{
	unsigned long huge = 8000UL * 1000 * 1000;

	check_insert(mt, huge, (void *) huge);
	check_insert(mt, 0, xa_mk_value(0));
	check_lb_not_empty(mt);
}

static noinline void __init check_rev_find(struct maple_tree *mt)
{
	int i, nr_entries = 200;
	void *val;
	MA_STATE(mas, mt, 0, 0);

	for (i = 0; i <= nr_entries; i++)
		mtree_store_range(mt, i*10, i*10 + 5,
				  xa_mk_value(i), GFP_KERNEL);

	rcu_read_lock();
	mas_set(&mas, 1000);
	val = mas_find_rev(&mas, 1000);
	MT_BUG_ON(mt, val != xa_mk_value(100));
	val = mas_find_rev(&mas, 1000);
	MT_BUG_ON(mt, val != NULL);

	mas_set(&mas, 999);
	val = mas_find_rev(&mas, 997);
	MT_BUG_ON(mt, val != NULL);

	mas_set(&mas, 1000);
	val = mas_find_rev(&mas, 900);
	MT_BUG_ON(mt, val != xa_mk_value(100));
	val = mas_find_rev(&mas, 900);
	MT_BUG_ON(mt, val != xa_mk_value(99));

	mas_set(&mas, 20);
	val = mas_find_rev(&mas, 0);
	MT_BUG_ON(mt, val != xa_mk_value(2));
	val = mas_find_rev(&mas, 0);
	MT_BUG_ON(mt, val != xa_mk_value(1));
	val = mas_find_rev(&mas, 0);
	MT_BUG_ON(mt, val != xa_mk_value(0));
	val = mas_find_rev(&mas, 0);
	MT_BUG_ON(mt, val != NULL);
	rcu_read_unlock();
}

static noinline void __init check_find(struct maple_tree *mt)
{
	unsigned long val = 0;
	unsigned long count;
	unsigned long max;
	unsigned long top;
	unsigned long last = 0, index = 0;
	void *entry, *entry2;

	MA_STATE(mas, mt, 0, 0);

	 
	MT_BUG_ON(mt, mtree_insert_index(mt, val++, GFP_KERNEL));

#if defined(CONFIG_64BIT)
	top = 4398046511104UL;
#else
	top = ULONG_MAX;
#endif

	if (MAPLE_32BIT) {
		count = 15;
	} else {
		count = 20;
	}

	for (int i = 0; i <= count; i++) {
		if (val != 64)
			MT_BUG_ON(mt, mtree_insert_index(mt, val, GFP_KERNEL));
		else
			MT_BUG_ON(mt, mtree_insert(mt, val,
				XA_ZERO_ENTRY, GFP_KERNEL));

		val <<= 2;
	}

	val = 0;
	mas_set(&mas, val);
	mas_lock(&mas);
	while ((entry = mas_find(&mas, 268435456)) != NULL) {
		if (val != 64)
			MT_BUG_ON(mt, xa_mk_value(val) != entry);
		else
			MT_BUG_ON(mt, entry != XA_ZERO_ENTRY);

		val <<= 2;
		 
		if (!val)
			val = 1;
	}
	mas_unlock(&mas);

	val = 0;
	mas_set(&mas, val);
	mas_lock(&mas);
	mas_for_each(&mas, entry, ULONG_MAX) {
		if (val != 64)
			MT_BUG_ON(mt, xa_mk_value(val) != entry);
		else
			MT_BUG_ON(mt, entry != XA_ZERO_ENTRY);
		val <<= 2;
		 
		if (!val)
			val = 1;
	}
	mas_unlock(&mas);

	 
	val = 0;
	mas_set(&mas, val);
	mas_lock(&mas);
	mas_for_each(&mas, entry, ULONG_MAX) {
		if (val != 64)
			MT_BUG_ON(mt, xa_mk_value(val) != entry);
		else
			MT_BUG_ON(mt, entry != XA_ZERO_ENTRY);
		val <<= 2;
		 
		if (!val)
			val = 1;

		mas_pause(&mas);
		mas_unlock(&mas);
		mas_lock(&mas);
	}
	mas_unlock(&mas);

	val = 0;
	max = 300;  
	mt_for_each(mt, entry, index, max) {
		MT_BUG_ON(mt, xa_mk_value(val) != entry);
		val <<= 2;
		if (val == 64)  
			val <<= 2;
		 
		if (!val)
			val = 1;
	}

	val = 0;
	max = 0;
	index = 0;
	MT_BUG_ON(mt, mtree_insert_index(mt, ULONG_MAX, GFP_KERNEL));
	mt_for_each(mt, entry, index, ULONG_MAX) {
		if (val == top)
			MT_BUG_ON(mt, entry != xa_mk_value(LONG_MAX));
		else
			MT_BUG_ON(mt, xa_mk_value(val) != entry);

		 
		if ((val << 2) < val)
			val = ULONG_MAX;
		else
			val <<= 2;

		if (val == 64)  
			val <<= 2;
		 
		if (!val)
			val = 1;
		max++;
		MT_BUG_ON(mt, max > 25);
	}
	mtree_erase_index(mt, ULONG_MAX);

	mas_reset(&mas);
	index = 17;
	entry = mt_find(mt, &index, 512);
	MT_BUG_ON(mt, xa_mk_value(256) != entry);

	mas_reset(&mas);
	index = 17;
	entry = mt_find(mt, &index, 20);
	MT_BUG_ON(mt, entry != NULL);


	 
	 
	MT_BUG_ON(mt, mtree_insert_index(mt, ULONG_MAX, GFP_KERNEL));

	val = 0;
	mas_set(&mas, 0);
	mas_lock(&mas);
	mas_for_each(&mas, entry, ULONG_MAX) {
		if (val == 64)
			MT_BUG_ON(mt, entry != XA_ZERO_ENTRY);
		else if (val == top)
			MT_BUG_ON(mt, entry != xa_mk_value(LONG_MAX));
		else
			MT_BUG_ON(mt, xa_mk_value(val) != entry);

		 
		if ((val << 2) < val)
			val = ULONG_MAX;
		else
			val <<= 2;

		 
		if (!val)
			val = 1;
		mas_pause(&mas);
		mas_unlock(&mas);
		mas_lock(&mas);
	}
	mas_unlock(&mas);

	mas_set(&mas, 1048576);
	mas_lock(&mas);
	entry = mas_find(&mas, 1048576);
	mas_unlock(&mas);
	MT_BUG_ON(mas.tree, entry == NULL);

	 
	 
	mas_lock(&mas);
	mas_reset(&mas);
	mas.index = ULONG_MAX;  
	entry = mas_find(&mas, ULONG_MAX);
	entry = mas_prev(&mas, 0);
	index = mas.index;
	last = mas.last;

	 
	mas_reset(&mas);
	mas.index = ULONG_MAX;
	mas.last = ULONG_MAX;
	mas_erase(&mas);

	 
	mas_reset(&mas);
	entry2 = mas_prev(&mas, 0);

	 
	MT_BUG_ON(mt, entry != entry2);
	MT_BUG_ON(mt, index != mas.index);
	MT_BUG_ON(mt, last != mas.last);


	mas.node = MAS_NONE;
	mas.index = ULONG_MAX;
	mas.last = ULONG_MAX;
	entry2 = mas_prev(&mas, 0);
	MT_BUG_ON(mt, entry != entry2);

	mas_set(&mas, 0);
	MT_BUG_ON(mt, mas_prev(&mas, 0) != NULL);

	mas_unlock(&mas);
	mtree_destroy(mt);
}

static noinline void __init check_find_2(struct maple_tree *mt)
{
	unsigned long i, j;
	void *entry;

	MA_STATE(mas, mt, 0, 0);
	rcu_read_lock();
	mas_for_each(&mas, entry, ULONG_MAX)
		MT_BUG_ON(mt, true);
	rcu_read_unlock();

	for (i = 0; i < 256; i++) {
		mtree_insert_index(mt, i, GFP_KERNEL);
		j = 0;
		mas_set(&mas, 0);
		rcu_read_lock();
		mas_for_each(&mas, entry, ULONG_MAX) {
			MT_BUG_ON(mt, entry != xa_mk_value(j));
			j++;
		}
		rcu_read_unlock();
		MT_BUG_ON(mt, j != i + 1);
	}

	for (i = 0; i < 256; i++) {
		mtree_erase_index(mt, i);
		j = i + 1;
		mas_set(&mas, 0);
		rcu_read_lock();
		mas_for_each(&mas, entry, ULONG_MAX) {
			if (xa_is_zero(entry))
				continue;

			MT_BUG_ON(mt, entry != xa_mk_value(j));
			j++;
		}
		rcu_read_unlock();
		MT_BUG_ON(mt, j != 256);
	}

	 
}


#if defined(CONFIG_64BIT)
static noinline void __init check_alloc_rev_range(struct maple_tree *mt)
{
	 

	static const unsigned long range[] = {
	 
		0x565234af2000, 0x565234af4000,
		0x565234af4000, 0x565234af9000,
		0x565234af9000, 0x565234afb000,
		0x565234afc000, 0x565234afd000,
		0x565234afd000, 0x565234afe000,
		0x565235def000, 0x565235e10000,
		0x7f36d4bfd000, 0x7f36d4ee2000,
		0x7f36d4ee2000, 0x7f36d4f04000,
		0x7f36d4f04000, 0x7f36d504c000,
		0x7f36d504c000, 0x7f36d5098000,
		0x7f36d5098000, 0x7f36d5099000,
		0x7f36d5099000, 0x7f36d509d000,
		0x7f36d509d000, 0x7f36d509f000,
		0x7f36d509f000, 0x7f36d50a5000,
		0x7f36d50b9000, 0x7f36d50db000,
		0x7f36d50db000, 0x7f36d50dc000,
		0x7f36d50dc000, 0x7f36d50fa000,
		0x7f36d50fa000, 0x7f36d5102000,
		0x7f36d5102000, 0x7f36d5103000,
		0x7f36d5103000, 0x7f36d5104000,
		0x7f36d5104000, 0x7f36d5105000,
		0x7fff5876b000, 0x7fff5878d000,
		0x7fff5878e000, 0x7fff58791000,
		0x7fff58791000, 0x7fff58793000,
	};

	static const unsigned long holes[] = {
		 
		0x565234afb000, 0x565234afc000, 0x1000,
		0x565234afe000, 0x565235def000, 0x12F1000,
		0x565235e10000, 0x7f36d4bfd000, 0x28E49EDED000,
	};

	 
	static const unsigned long req_range[] = {
		0x565234af9000,  
		0x7fff58791000,  
		0x1000,          
		0x7fff5878d << 12,   
		0,               

		0x0,             
		0x565234AF0 << 12,     
		0x3000,          
		0x565234AEE << 12,   
		0,               

		0x0,             
		-1,              
		0x1000,          
		562949953421311 << 12, 
		0,               

		0x0,             
		0x7F36D5109 << 12,     
		0x4000,          
		0x7F36D5106 << 12,     
		0,               

		 
		0x0,
		34148798628 << 12,
		19 << 12,
		34148797418 << 12,
		0x0,

		 
		0x0,
		18446744073709551615UL,
		562915594369134UL << 12,
		0x0,
		-EBUSY,

		 
		34148798725 << 12,
		34148798725 << 12,
		1 << 12,
		34148798725 << 12,
		0,
	};

	int i, range_count = ARRAY_SIZE(range);
	int req_range_count = ARRAY_SIZE(req_range);
	unsigned long min = 0;

	MA_STATE(mas, mt, 0, 0);

	mtree_store_range(mt, MTREE_ALLOC_MAX, ULONG_MAX, XA_ZERO_ENTRY,
			  GFP_KERNEL);
#define DEBUG_REV_RANGE 0
	for (i = 0; i < range_count; i += 2) {
		 

#if DEBUG_REV_RANGE
		pr_debug("\t%s: Insert %lu-%lu\n", __func__, range[i] >> 12,
				(range[i + 1] >> 12) - 1);
#endif
		check_insert_range(mt, range[i] >> 12, (range[i + 1] >> 12) - 1,
				xa_mk_value(range[i] >> 12), 0);
		mt_validate(mt);
	}


	mas_lock(&mas);
	for (i = 0; i < ARRAY_SIZE(holes); i += 3) {
#if DEBUG_REV_RANGE
		pr_debug("Search from %lu-%lu for gap %lu should be at %lu\n",
				min, holes[i+1]>>12, holes[i+2]>>12,
				holes[i] >> 12);
#endif
		MT_BUG_ON(mt, mas_empty_area_rev(&mas, min,
					holes[i+1] >> 12,
					holes[i+2] >> 12));
#if DEBUG_REV_RANGE
		pr_debug("Found %lu %lu\n", mas.index, mas.last);
		pr_debug("gap %lu %lu\n", (holes[i] >> 12),
				(holes[i+1] >> 12));
#endif
		MT_BUG_ON(mt, mas.last + 1 != (holes[i+1] >> 12));
		MT_BUG_ON(mt, mas.index != (holes[i+1] >> 12) - (holes[i+2] >> 12));
		min = holes[i+1] >> 12;
		mas_reset(&mas);
	}

	mas_unlock(&mas);
	for (i = 0; i < req_range_count; i += 5) {
#if DEBUG_REV_RANGE
		pr_debug("\tReverse request %d between %lu-%lu size %lu, should get %lu\n",
				i, req_range[i] >> 12,
				(req_range[i + 1] >> 12),
				req_range[i+2] >> 12,
				req_range[i+3] >> 12);
#endif
		check_mtree_alloc_rrange(mt,
				req_range[i]   >> 12,  
				req_range[i+1] >> 12,  
				req_range[i+2] >> 12,  
				req_range[i+3] >> 12,  
				req_range[i+4],        
				xa_mk_value(req_range[i] >> 12));  
		mt_validate(mt);
	}

	mt_set_non_kernel(1);
	mtree_erase(mt, 34148798727);  
	mtree_erase(mt, 34148798725);
	check_mtree_alloc_rrange(mt, 0, 34359052173, 210253414,
			34148798725, 0, mt);

	mtree_destroy(mt);
}

static noinline void __init check_alloc_range(struct maple_tree *mt)
{
	 

	static const unsigned long range[] = {
	 
		0x565234af2000, 0x565234af4000,
		0x565234af4000, 0x565234af9000,
		0x565234af9000, 0x565234afb000,
		0x565234afc000, 0x565234afd000,
		0x565234afd000, 0x565234afe000,
		0x565235def000, 0x565235e10000,
		0x7f36d4bfd000, 0x7f36d4ee2000,
		0x7f36d4ee2000, 0x7f36d4f04000,
		0x7f36d4f04000, 0x7f36d504c000,
		0x7f36d504c000, 0x7f36d5098000,
		0x7f36d5098000, 0x7f36d5099000,
		0x7f36d5099000, 0x7f36d509d000,
		0x7f36d509d000, 0x7f36d509f000,
		0x7f36d509f000, 0x7f36d50a5000,
		0x7f36d50b9000, 0x7f36d50db000,
		0x7f36d50db000, 0x7f36d50dc000,
		0x7f36d50dc000, 0x7f36d50fa000,
		0x7f36d50fa000, 0x7f36d5102000,
		0x7f36d5102000, 0x7f36d5103000,
		0x7f36d5103000, 0x7f36d5104000,
		0x7f36d5104000, 0x7f36d5105000,
		0x7fff5876b000, 0x7fff5878d000,
		0x7fff5878e000, 0x7fff58791000,
		0x7fff58791000, 0x7fff58793000,
	};
	static const unsigned long holes[] = {
		 
		0x565234afb000, 0x565234afc000, 0x1000,
		0x565234afe000, 0x565235def000, 0x12F1000,
		0x565235e10000, 0x7f36d4bfd000, 0x28E49EDED000,
	};

	 
	static const unsigned long req_range[] = {
		0x565234af9000,  
		0x7fff58791000,  
		0x1000,          
		0x565234afb000,  
		0,               

		0x0,             
		0x7fff58791000,  
		0x1F00,          
		0x0,             
		0,               

		 
		34148797436 << 12,  
		0x7fff587AF000,     
		0x3000,          
		34148798629 << 12,              
		0,               

		 
		34148798623 << 12,   
		34148798683 << 12,   
		0x15000,             
		0,              
		-EBUSY,               

		 
		34148798623 << 12,   
		0x7fff587AF000,     
		0x10000,            
		34148798632 << 12,              
		0,               

		 
		0,                   
		-1,                  
		-1,                  
		0,                   
		-EBUSY,              

		 
		0,                   
		-1,                  
		4503599618982063UL << 12,   
		34359052178 << 12,   
		-EBUSY,              

		 
		34148798648 << 12,		 
		34148798648 << 12,		 
		4096,			 
		34148798648 << 12,	 
		0,			 
	};
	int i, range_count = ARRAY_SIZE(range);
	int req_range_count = ARRAY_SIZE(req_range);
	unsigned long min = 0x565234af2000;
	MA_STATE(mas, mt, 0, 0);

	mtree_store_range(mt, MTREE_ALLOC_MAX, ULONG_MAX, XA_ZERO_ENTRY,
			  GFP_KERNEL);
	for (i = 0; i < range_count; i += 2) {
#define DEBUG_ALLOC_RANGE 0
#if DEBUG_ALLOC_RANGE
		pr_debug("\tInsert %lu-%lu\n", range[i] >> 12,
			 (range[i + 1] >> 12) - 1);
		mt_dump(mt, mt_dump_hex);
#endif
		check_insert_range(mt, range[i] >> 12, (range[i + 1] >> 12) - 1,
				xa_mk_value(range[i] >> 12), 0);
		mt_validate(mt);
	}



	mas_lock(&mas);
	for (i = 0; i < ARRAY_SIZE(holes); i += 3) {

#if DEBUG_ALLOC_RANGE
		pr_debug("\tGet empty %lu-%lu size %lu (%lx-%lx)\n", min >> 12,
			holes[i+1] >> 12, holes[i+2] >> 12,
			min, holes[i+1]);
#endif
		MT_BUG_ON(mt, mas_empty_area(&mas, min >> 12,
					holes[i+1] >> 12,
					holes[i+2] >> 12));
		MT_BUG_ON(mt, mas.index != holes[i] >> 12);
		min = holes[i+1];
		mas_reset(&mas);
	}
	mas_unlock(&mas);
	for (i = 0; i < req_range_count; i += 5) {
#if DEBUG_ALLOC_RANGE
		pr_debug("\tTest %d: %lu-%lu size %lu expected %lu (%lu-%lu)\n",
			 i/5, req_range[i]   >> 12, req_range[i + 1]   >> 12,
			 req_range[i + 2]   >> 12, req_range[i + 3]   >> 12,
			 req_range[i], req_range[i+1]);
#endif
		check_mtree_alloc_range(mt,
				req_range[i]   >> 12,  
				req_range[i+1] >> 12,  
				req_range[i+2] >> 12,  
				req_range[i+3] >> 12,  
				req_range[i+4],        
				xa_mk_value(req_range[i] >> 12));  
		mt_validate(mt);
#if DEBUG_ALLOC_RANGE
		mt_dump(mt, mt_dump_hex);
#endif
	}

	mtree_destroy(mt);
}
#endif

static noinline void __init check_ranges(struct maple_tree *mt)
{
	int i, val, val2;
	static const unsigned long r[] = {
		10, 15,
		20, 25,
		17, 22,  
		9, 1000,  
		100, 200,
		45, 168,
		118, 128,
			};

	MT_BUG_ON(mt, !mtree_empty(mt));
	check_insert_range(mt, r[0], r[1], xa_mk_value(r[0]), 0);
	check_insert_range(mt, r[2], r[3], xa_mk_value(r[2]), 0);
	check_insert_range(mt, r[4], r[5], xa_mk_value(r[4]), -EEXIST);
	MT_BUG_ON(mt, !mt_height(mt));
	 
	check_store_range(mt, r[4], r[5], xa_mk_value(r[4]), 0);
	check_store_range(mt, r[6], r[7], xa_mk_value(r[6]), 0);
	check_store_range(mt, r[8], r[9], xa_mk_value(r[8]), 0);
	MT_BUG_ON(mt, !mt_height(mt));
	mtree_destroy(mt);
	MT_BUG_ON(mt, mt_height(mt));

	check_seq(mt, 50, false);
	mt_set_non_kernel(4);
	check_store_range(mt, 5, 47,  xa_mk_value(47), 0);
	MT_BUG_ON(mt, !mt_height(mt));
	mtree_destroy(mt);

	 
	check_seq(mt, 100, false);
	 
	mt_set_non_kernel(10);
	check_store_range(mt, r[10], r[11], xa_mk_value(r[10]), 0);
	MT_BUG_ON(mt, !mt_height(mt));
	mtree_destroy(mt);

	 
	check_seq(mt, 200, false);
	 
	check_store_range(mt, r[10], r[11], xa_mk_value(r[10]), 0);
	MT_BUG_ON(mt, !mt_height(mt));
	mtree_destroy(mt);

	check_seq(mt, 30, false);
	check_store_range(mt, 6, 18, xa_mk_value(6), 0);
	MT_BUG_ON(mt, !mt_height(mt));
	mtree_destroy(mt);

	 
	 
	check_seq(mt, 400, false);
	mt_set_non_kernel(50);
	 
	check_store_range(mt, r[12], r[13], xa_mk_value(r[12]), 0);
	mt_set_non_kernel(50);
	mtree_test_erase(mt, 140);
	mtree_test_erase(mt, 141);
	mtree_test_erase(mt, 142);
	mtree_test_erase(mt, 143);
	mtree_test_erase(mt, 130);
	mtree_test_erase(mt, 131);
	mtree_test_erase(mt, 132);
	mtree_test_erase(mt, 133);
	mtree_test_erase(mt, 134);
	mtree_test_erase(mt, 135);
	check_load(mt, r[12], xa_mk_value(r[12]));
	check_load(mt, r[13], xa_mk_value(r[12]));
	check_load(mt, r[13] - 1, xa_mk_value(r[12]));
	check_load(mt, r[13] + 1, xa_mk_value(r[13] + 1));
	check_load(mt, 135, NULL);
	check_load(mt, 140, NULL);
	mt_set_non_kernel(0);
	MT_BUG_ON(mt, !mt_height(mt));
	mtree_destroy(mt);



	 
	mt_set_non_kernel(50);
	check_seq(mt, 400, false);
	check_store_range(mt, 353, 361, xa_mk_value(353), 0);
	check_store_range(mt, 347, 352, xa_mk_value(347), 0);

	check_load(mt, 346, xa_mk_value(346));
	for (i = 347; i <= 352; i++)
		check_load(mt, i, xa_mk_value(347));
	for (i = 353; i <= 361; i++)
		check_load(mt, i, xa_mk_value(353));
	check_load(mt, 362, xa_mk_value(362));
	mt_set_non_kernel(0);
	MT_BUG_ON(mt, !mt_height(mt));
	mtree_destroy(mt);

	mt_set_non_kernel(50);
	check_seq(mt, 400, false);
	check_store_range(mt, 352, 364, NULL, 0);
	check_store_range(mt, 351, 363, xa_mk_value(352), 0);
	check_load(mt, 350, xa_mk_value(350));
	check_load(mt, 351, xa_mk_value(352));
	for (i = 352; i <= 363; i++)
		check_load(mt, i, xa_mk_value(352));
	check_load(mt, 364, NULL);
	check_load(mt, 365, xa_mk_value(365));
	mt_set_non_kernel(0);
	MT_BUG_ON(mt, !mt_height(mt));
	mtree_destroy(mt);

	mt_set_non_kernel(5);
	check_seq(mt, 400, false);
	check_store_range(mt, 352, 364, NULL, 0);
	check_store_range(mt, 351, 364, xa_mk_value(352), 0);
	check_load(mt, 350, xa_mk_value(350));
	check_load(mt, 351, xa_mk_value(352));
	for (i = 352; i <= 364; i++)
		check_load(mt, i, xa_mk_value(352));
	check_load(mt, 365, xa_mk_value(365));
	mt_set_non_kernel(0);
	MT_BUG_ON(mt, !mt_height(mt));
	mtree_destroy(mt);


	mt_set_non_kernel(50);
	check_seq(mt, 400, false);
	check_store_range(mt, 362, 367, xa_mk_value(362), 0);
	check_store_range(mt, 353, 361, xa_mk_value(353), 0);
	mt_set_non_kernel(0);
	mt_validate(mt);
	MT_BUG_ON(mt, !mt_height(mt));
	mtree_destroy(mt);
	 

	mt_set_non_kernel(50);
	for (i = 0; i <= 500; i++) {
		val = i*5;
		val2 = (i+1)*5;
		check_store_range(mt, val, val2, xa_mk_value(val), 0);
	}
	check_store_range(mt, 2400, 2400, xa_mk_value(2400), 0);
	check_store_range(mt, 2411, 2411, xa_mk_value(2411), 0);
	check_store_range(mt, 2412, 2412, xa_mk_value(2412), 0);
	check_store_range(mt, 2396, 2400, xa_mk_value(4052020), 0);
	check_store_range(mt, 2402, 2402, xa_mk_value(2402), 0);
	mtree_destroy(mt);
	mt_set_non_kernel(0);

	mt_set_non_kernel(50);
	for (i = 0; i <= 500; i++) {
		val = i*5;
		val2 = (i+1)*5;
		check_store_range(mt, val, val2, xa_mk_value(val), 0);
	}
	check_store_range(mt, 2422, 2422, xa_mk_value(2422), 0);
	check_store_range(mt, 2424, 2424, xa_mk_value(2424), 0);
	check_store_range(mt, 2425, 2425, xa_mk_value(2), 0);
	check_store_range(mt, 2460, 2470, NULL, 0);
	check_store_range(mt, 2435, 2460, xa_mk_value(2435), 0);
	check_store_range(mt, 2461, 2470, xa_mk_value(2461), 0);
	mt_set_non_kernel(0);
	MT_BUG_ON(mt, !mt_height(mt));
	mtree_destroy(mt);

	 
	mt_init_flags(mt, MT_FLAGS_ALLOC_RANGE);
	 
	mt_set_non_kernel(50);
	for (i = 0; i <= 500; i++) {
		val = i * 5 + 1;
		val2 = val + 4;
		check_store_range(mt, val, val2, xa_mk_value(val), 0);
	}

	 
	for (i = 0; i < 10; i++) {
		val = val2 + 5;
		val2 = val + 4;
		check_store_range(mt, val, val2, xa_mk_value(val), 0);
	}

	 
	val = val2;
	for (i = 0; i < 10; i++) {
		val += 5;
		MT_BUG_ON(mt, mtree_test_store_range(mt, val, ULONG_MAX,
						     xa_mk_value(val)) != 0);
	}

	 
	for (i = 10; i < 30; i += 2) {
		val = i * 5 + 1;
		val2 = val + 5;
		check_store_range(mt, val, val2, xa_mk_value(val), 0);
	}

	 
	for (i = 50; i < 70; i += 2) {
		val2 = i * 5;
		val = val2 - 5;
		check_store_range(mt, val, val2, xa_mk_value(val), 0);
	}

	 
	for (i = 100; i < 130; i += 3) {
		val = i * 5 - 5;
		val2 = i * 5 + 1;
		check_store_range(mt, val, val2, xa_mk_value(val), 0);
	}

	 
	mt_set_in_rcu(mt);
	for (i = 150; i < 180; i += 3) {
		val = i * 5 - 5;
		val2 = i * 5 + 1;
		check_store_range(mt, val, val2, xa_mk_value(val), 0);
	}

	MT_BUG_ON(mt, !mt_height(mt));
	mt_validate(mt);
	mt_set_non_kernel(0);
	mtree_destroy(mt);

	 
	mt_init_flags(mt, MT_FLAGS_ALLOC_RANGE);
	mt_set_non_kernel(50);
	for (i = 0; i <= 50; i++) {
		val = i*10;
		val2 = (i+1)*10;
		check_store_range(mt, val, val2, xa_mk_value(val), 0);
	}
	check_store_range(mt, 161, 161, xa_mk_value(161), 0);
	check_store_range(mt, 162, 162, xa_mk_value(162), 0);
	check_store_range(mt, 163, 163, xa_mk_value(163), 0);
	check_store_range(mt, 240, 249, NULL, 0);
	mtree_erase(mt, 200);
	mtree_erase(mt, 210);
	mtree_erase(mt, 220);
	mtree_erase(mt, 230);
	mt_set_non_kernel(0);
	MT_BUG_ON(mt, !mt_height(mt));
	mtree_destroy(mt);

	mt_init_flags(mt, MT_FLAGS_ALLOC_RANGE);
	for (i = 0; i <= 500; i++) {
		val = i*10;
		val2 = (i+1)*10;
		check_store_range(mt, val, val2, xa_mk_value(val), 0);
	}
	check_store_range(mt, 4600, 4959, xa_mk_value(1), 0);
	mt_validate(mt);
	MT_BUG_ON(mt, !mt_height(mt));
	mtree_destroy(mt);

	mt_init_flags(mt, MT_FLAGS_ALLOC_RANGE);
	for (i = 0; i <= 500; i++) {
		val = i*10;
		val2 = (i+1)*10;
		check_store_range(mt, val, val2, xa_mk_value(val), 0);
	}
	check_store_range(mt, 4811, 4811, xa_mk_value(4811), 0);
	check_store_range(mt, 4812, 4812, xa_mk_value(4812), 0);
	check_store_range(mt, 4861, 4861, xa_mk_value(4861), 0);
	check_store_range(mt, 4862, 4862, xa_mk_value(4862), 0);
	check_store_range(mt, 4842, 4849, NULL, 0);
	mt_validate(mt);
	MT_BUG_ON(mt, !mt_height(mt));
	mtree_destroy(mt);

	mt_init_flags(mt, MT_FLAGS_ALLOC_RANGE);
	for (i = 0; i <= 1300; i++) {
		val = i*10;
		val2 = (i+1)*10;
		check_store_range(mt, val, val2, xa_mk_value(val), 0);
		MT_BUG_ON(mt, mt_height(mt) >= 4);
	}
	 
	for (i = 5; i < 215; i += 10)
		check_store_range(mt, 11450 + i, 11450 + i + 1, NULL, 0);
	for (i = 5; i < 65; i += 10)
		check_store_range(mt, 11770 + i, 11770 + i + 1, NULL, 0);

	MT_BUG_ON(mt, mt_height(mt) >= 4);
	for (i = 5; i < 45; i += 10)
		check_store_range(mt, 11700 + i, 11700 + i + 1, NULL, 0);
	if (!MAPLE_32BIT)
		MT_BUG_ON(mt, mt_height(mt) < 4);
	mtree_destroy(mt);


	mt_init_flags(mt, MT_FLAGS_ALLOC_RANGE);
	for (i = 0; i <= 1200; i++) {
		val = i*10;
		val2 = (i+1)*10;
		check_store_range(mt, val, val2, xa_mk_value(val), 0);
		MT_BUG_ON(mt, mt_height(mt) >= 4);
	}
	 
	for (i = 5; i < 455; i += 10)
		check_store_range(mt, 7800 + i, 7800 + i + 1, NULL, 0);

	for (i = 1; i < 16; i++)
		check_store_range(mt, 8185 + i, 8185 + i + 1,
				  xa_mk_value(8185+i), 0);
	MT_BUG_ON(mt, mt_height(mt) >= 4);
	 
	check_store_range(mt, 8184, 8184, xa_mk_value(8184), 0);
	if (!MAPLE_32BIT)
		MT_BUG_ON(mt, mt_height(mt) != 4);
}

static noinline void __init check_next_entry(struct maple_tree *mt)
{
	void *entry = NULL;
	unsigned long limit = 30, i = 0;
	MA_STATE(mas, mt, i, i);

	MT_BUG_ON(mt, !mtree_empty(mt));

	check_seq(mt, limit, false);
	rcu_read_lock();

	 
	MT_BUG_ON(mt, mas_walk(&mas) != xa_mk_value(i++));
	for ( ; i <= limit + 1; i++) {
		entry = mas_next(&mas, limit);
		if (i > limit)
			MT_BUG_ON(mt, entry != NULL);
		else
			MT_BUG_ON(mt, xa_mk_value(i) != entry);
	}
	rcu_read_unlock();
	mtree_destroy(mt);
}

static noinline void __init check_prev_entry(struct maple_tree *mt)
{
	unsigned long index = 16;
	void *value;
	int i;

	MA_STATE(mas, mt, index, index);

	MT_BUG_ON(mt, !mtree_empty(mt));
	check_seq(mt, 30, false);

	rcu_read_lock();
	value = mas_find(&mas, ULONG_MAX);
	MT_BUG_ON(mt, value != xa_mk_value(index));
	value = mas_prev(&mas, 0);
	MT_BUG_ON(mt, value != xa_mk_value(index - 1));
	rcu_read_unlock();
	mtree_destroy(mt);

	 
	mt_init_flags(mt, MT_FLAGS_ALLOC_RANGE);
	mas_lock(&mas);
	for (i = 0; i <= index; i++) {
		mas_set_range(&mas, i*10, i*10+5);
		mas_store_gfp(&mas, xa_mk_value(i), GFP_KERNEL);
	}

	mas_set(&mas, 20);
	value = mas_walk(&mas);
	MT_BUG_ON(mt, value != xa_mk_value(2));

	value = mas_prev(&mas, 19);
	MT_BUG_ON(mt, value != NULL);

	mas_set(&mas, 80);
	value = mas_walk(&mas);
	MT_BUG_ON(mt, value != xa_mk_value(8));

	value = mas_prev(&mas, 76);
	MT_BUG_ON(mt, value != NULL);

	mas_unlock(&mas);
}

static noinline void __init check_root_expand(struct maple_tree *mt)
{
	MA_STATE(mas, mt, 0, 0);
	void *ptr;


	mas_lock(&mas);
	mas_set(&mas, 3);
	ptr = mas_walk(&mas);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, ptr != NULL);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != ULONG_MAX);

	ptr = &check_prev_entry;
	mas_set(&mas, 1);
	mas_store_gfp(&mas, ptr, GFP_KERNEL);

	mas_set(&mas, 0);
	ptr = mas_walk(&mas);
	MT_BUG_ON(mt, ptr != NULL);

	mas_set(&mas, 1);
	ptr = mas_walk(&mas);
	MT_BUG_ON(mt, ptr != &check_prev_entry);

	mas_set(&mas, 2);
	ptr = mas_walk(&mas);
	MT_BUG_ON(mt, ptr != NULL);
	mas_unlock(&mas);
	mtree_destroy(mt);


	mt_init_flags(mt, 0);
	mas_lock(&mas);

	mas_set(&mas, 0);
	ptr = &check_prev_entry;
	mas_store_gfp(&mas, ptr, GFP_KERNEL);

	mas_set(&mas, 5);
	ptr = mas_walk(&mas);
	MT_BUG_ON(mt, ptr != NULL);
	MT_BUG_ON(mt, mas.index != 1);
	MT_BUG_ON(mt, mas.last != ULONG_MAX);

	mas_set_range(&mas, 0, 100);
	ptr = mas_walk(&mas);
	MT_BUG_ON(mt, ptr != &check_prev_entry);
	MT_BUG_ON(mt, mas.last != 0);
	mas_unlock(&mas);
	mtree_destroy(mt);

	mt_init_flags(mt, 0);
	mas_lock(&mas);

	mas_set(&mas, 0);
	ptr = (void *)((unsigned long) check_prev_entry | 1UL);
	mas_store_gfp(&mas, ptr, GFP_KERNEL);
	ptr = mas_next(&mas, ULONG_MAX);
	MT_BUG_ON(mt, ptr != NULL);
	MT_BUG_ON(mt, (mas.index != 1) && (mas.last != ULONG_MAX));

	mas_set(&mas, 1);
	ptr = mas_prev(&mas, 0);
	MT_BUG_ON(mt, (mas.index != 0) && (mas.last != 0));
	MT_BUG_ON(mt, ptr != (void *)((unsigned long) check_prev_entry | 1UL));

	mas_unlock(&mas);

	mtree_destroy(mt);

	mt_init_flags(mt, 0);
	mas_lock(&mas);
	mas_set(&mas, 0);
	ptr = (void *)((unsigned long) check_prev_entry | 2UL);
	mas_store_gfp(&mas, ptr, GFP_KERNEL);
	ptr = mas_next(&mas, ULONG_MAX);
	MT_BUG_ON(mt, ptr != NULL);
	MT_BUG_ON(mt, (mas.index != ULONG_MAX) && (mas.last != ULONG_MAX));

	mas_set(&mas, 1);
	ptr = mas_prev(&mas, 0);
	MT_BUG_ON(mt, (mas.index != 0) && (mas.last != 0));
	MT_BUG_ON(mt, ptr != (void *)((unsigned long) check_prev_entry | 2UL));


	mas_unlock(&mas);
}

static noinline void __init check_gap_combining(struct maple_tree *mt)
{
	struct maple_enode *mn1, *mn2;
	void *entry;
	unsigned long singletons = 100;
	static const unsigned long *seq100;
	static const unsigned long seq100_64[] = {
		 
		74, 75, 76,
		50, 100, 2,

		 
		44, 45, 46, 43,
		20, 50, 3,

		 
		80, 81, 82,
		76, 2, 79, 85, 4,
	};

	static const unsigned long seq100_32[] = {
		 
		61, 62, 63,
		50, 100, 2,

		 
		31, 32, 33, 30,
		20, 50, 3,

		 
		80, 81, 82,
		76, 2, 79, 85, 4,
	};

	static const unsigned long seq2000[] = {
		1152, 1151,
		1100, 1200, 2,
	};
	static const unsigned long seq400[] = {
		286, 318,
		256, 260, 266, 270, 275, 280, 290, 398,
		286, 310,
	};

	unsigned long index;

	MA_STATE(mas, mt, 0, 0);

	if (MAPLE_32BIT)
		seq100 = seq100_32;
	else
		seq100 = seq100_64;

	index = seq100[0];
	mas_set(&mas, index);
	MT_BUG_ON(mt, !mtree_empty(mt));
	check_seq(mt, singletons, false);  

	mt_set_non_kernel(1);
	mtree_test_erase(mt, seq100[2]);
	check_load(mt, seq100[2], NULL);
	mtree_test_erase(mt, seq100[1]);
	check_load(mt, seq100[1], NULL);

	rcu_read_lock();
	entry = mas_find(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != xa_mk_value(index));
	mn1 = mas.node;
	mas_next(&mas, ULONG_MAX);
	entry = mas_next(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != xa_mk_value(index + 4));
	mn2 = mas.node;
	MT_BUG_ON(mt, mn1 == mn2);  

	 
	mt_set_non_kernel(1);
	mas_reset(&mas);
	MT_BUG_ON(mt, mas_empty_area_rev(&mas, seq100[3], seq100[4],
					     seq100[5]));
	MT_BUG_ON(mt, mas.index != index + 1);
	rcu_read_unlock();

	mtree_test_erase(mt, seq100[6]);
	check_load(mt, seq100[6], NULL);
	mtree_test_erase(mt, seq100[7]);
	check_load(mt, seq100[7], NULL);
	mtree_test_erase(mt, seq100[8]);
	index = seq100[9];

	rcu_read_lock();
	mas.index = index;
	mas.last = index;
	mas_reset(&mas);
	entry = mas_find(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != xa_mk_value(index));
	mn1 = mas.node;
	entry = mas_next(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != xa_mk_value(index + 4));
	mas_next(&mas, ULONG_MAX);  
	mn2 = mas.node;
	MT_BUG_ON(mt, mn1 == mn2);  

	 
	mas_reset(&mas);
	MT_BUG_ON(mt, mas_empty_area_rev(&mas, seq100[10], seq100[11],
					     seq100[12]));
	MT_BUG_ON(mt, mas.index != seq100[6]);
	rcu_read_unlock();

	mt_set_non_kernel(1);
	mtree_store(mt, seq100[13], NULL, GFP_KERNEL);
	check_load(mt, seq100[13], NULL);
	check_load(mt, seq100[14], xa_mk_value(seq100[14]));
	mtree_store(mt, seq100[14], NULL, GFP_KERNEL);
	check_load(mt, seq100[13], NULL);
	check_load(mt, seq100[14], NULL);

	mas_reset(&mas);
	rcu_read_lock();
	MT_BUG_ON(mt, mas_empty_area_rev(&mas, seq100[16], seq100[15],
					     seq100[17]));
	MT_BUG_ON(mt, mas.index != seq100[13]);
	mt_validate(mt);
	rcu_read_unlock();

	 
	mt_set_non_kernel(2);
	mtree_test_store_range(mt, seq100[18], seq100[14], NULL);
	mtree_test_erase(mt, seq100[15]);
	mas_reset(&mas);
	rcu_read_lock();
	MT_BUG_ON(mt, mas_empty_area_rev(&mas, seq100[16], seq100[19],
					     seq100[20]));
	rcu_read_unlock();
	MT_BUG_ON(mt, mas.index != seq100[18]);
	mt_validate(mt);
	mtree_destroy(mt);

	 
	mt_init_flags(mt, MT_FLAGS_ALLOC_RANGE);
	check_seq(mt, 2000, false);
	mt_set_non_kernel(1);
	mtree_test_erase(mt, seq2000[0]);
	mtree_test_erase(mt, seq2000[1]);

	mt_set_non_kernel(2);
	mas_reset(&mas);
	rcu_read_lock();
	MT_BUG_ON(mt, mas_empty_area_rev(&mas, seq2000[2], seq2000[3],
					     seq2000[4]));
	MT_BUG_ON(mt, mas.index != seq2000[1]);
	rcu_read_unlock();
	mt_validate(mt);
	mtree_destroy(mt);

	 
	mt_set_non_kernel(99);
	mt_init_flags(mt, MT_FLAGS_ALLOC_RANGE);
	check_seq(mt, 400, false);
	mtree_test_store_range(mt, seq400[0], seq400[1], NULL);
	mt_set_non_kernel(0);
	mtree_destroy(mt);

	mt_init_flags(mt, MT_FLAGS_ALLOC_RANGE);
	check_seq(mt, 400, false);
	mt_set_non_kernel(50);
	mtree_test_store_range(mt, seq400[2], seq400[9],
			       xa_mk_value(seq400[2]));
	mtree_test_store_range(mt, seq400[3], seq400[9],
			       xa_mk_value(seq400[3]));
	mtree_test_store_range(mt, seq400[4], seq400[9],
			       xa_mk_value(seq400[4]));
	mtree_test_store_range(mt, seq400[5], seq400[9],
			       xa_mk_value(seq400[5]));
	mtree_test_store_range(mt, seq400[0], seq400[9],
			       xa_mk_value(seq400[0]));
	mtree_test_store_range(mt, seq400[6], seq400[9],
			       xa_mk_value(seq400[6]));
	mtree_test_store_range(mt, seq400[7], seq400[9],
			       xa_mk_value(seq400[7]));
	mtree_test_store_range(mt, seq400[8], seq400[9],
			       xa_mk_value(seq400[8]));
	mtree_test_store_range(mt, seq400[10], seq400[11],
			       xa_mk_value(seq400[10]));
	mt_validate(mt);
	mt_set_non_kernel(0);
	mtree_destroy(mt);
}
static noinline void __init check_node_overwrite(struct maple_tree *mt)
{
	int i, max = 4000;

	for (i = 0; i < max; i++)
		mtree_test_store_range(mt, i*100, i*100 + 50, xa_mk_value(i*100));

	mtree_test_store_range(mt, 319951, 367950, NULL);
	 
	mt_validate(mt);
}

#if defined(BENCH_SLOT_STORE)
static noinline void __init bench_slot_store(struct maple_tree *mt)
{
	int i, brk = 105, max = 1040, brk_start = 100, count = 20000000;

	for (i = 0; i < max; i += 10)
		mtree_store_range(mt, i, i + 5, xa_mk_value(i), GFP_KERNEL);

	for (i = 0; i < count; i++) {
		mtree_store_range(mt, brk, brk, NULL, GFP_KERNEL);
		mtree_store_range(mt, brk_start, brk, xa_mk_value(brk),
				  GFP_KERNEL);
	}
}
#endif

#if defined(BENCH_NODE_STORE)
static noinline void __init bench_node_store(struct maple_tree *mt)
{
	int i, overwrite = 76, max = 240, count = 20000000;

	for (i = 0; i < max; i += 10)
		mtree_store_range(mt, i, i + 5, xa_mk_value(i), GFP_KERNEL);

	for (i = 0; i < count; i++) {
		mtree_store_range(mt, overwrite,  overwrite + 15,
				  xa_mk_value(overwrite), GFP_KERNEL);

		overwrite += 5;
		if (overwrite >= 135)
			overwrite = 76;
	}
}
#endif

#if defined(BENCH_AWALK)
static noinline void __init bench_awalk(struct maple_tree *mt)
{
	int i, max = 2500, count = 50000000;
	MA_STATE(mas, mt, 1470, 1470);

	for (i = 0; i < max; i += 10)
		mtree_store_range(mt, i, i + 5, xa_mk_value(i), GFP_KERNEL);

	mtree_store_range(mt, 1470, 1475, NULL, GFP_KERNEL);

	for (i = 0; i < count; i++) {
		mas_empty_area_rev(&mas, 0, 2000, 10);
		mas_reset(&mas);
	}
}
#endif
#if defined(BENCH_WALK)
static noinline void __init bench_walk(struct maple_tree *mt)
{
	int i, max = 2500, count = 550000000;
	MA_STATE(mas, mt, 1470, 1470);

	for (i = 0; i < max; i += 10)
		mtree_store_range(mt, i, i + 5, xa_mk_value(i), GFP_KERNEL);

	for (i = 0; i < count; i++) {
		mas_walk(&mas);
		mas_reset(&mas);
	}

}
#endif

#if defined(BENCH_MT_FOR_EACH)
static noinline void __init bench_mt_for_each(struct maple_tree *mt)
{
	int i, count = 1000000;
	unsigned long max = 2500, index = 0;
	void *entry;

	for (i = 0; i < max; i += 5)
		mtree_store_range(mt, i, i + 4, xa_mk_value(i), GFP_KERNEL);

	for (i = 0; i < count; i++) {
		unsigned long j = 0;

		mt_for_each(mt, entry, index, max) {
			MT_BUG_ON(mt, entry != xa_mk_value(j));
			j += 5;
		}

		index = 0;
	}

}
#endif

#if defined(BENCH_MAS_FOR_EACH)
static noinline void __init bench_mas_for_each(struct maple_tree *mt)
{
	int i, count = 1000000;
	unsigned long max = 2500;
	void *entry;
	MA_STATE(mas, mt, 0, 0);

	for (i = 0; i < max; i += 5) {
		int gap = 4;

		if (i % 30 == 0)
			gap = 3;
		mtree_store_range(mt, i, i + gap, xa_mk_value(i), GFP_KERNEL);
	}

	rcu_read_lock();
	for (i = 0; i < count; i++) {
		unsigned long j = 0;

		mas_for_each(&mas, entry, max) {
			MT_BUG_ON(mt, entry != xa_mk_value(j));
			j += 5;
		}
		mas_set(&mas, 0);
	}
	rcu_read_unlock();

}
#endif
#if defined(BENCH_MAS_PREV)
static noinline void __init bench_mas_prev(struct maple_tree *mt)
{
	int i, count = 1000000;
	unsigned long max = 2500;
	void *entry;
	MA_STATE(mas, mt, 0, 0);

	for (i = 0; i < max; i += 5) {
		int gap = 4;

		if (i % 30 == 0)
			gap = 3;
		mtree_store_range(mt, i, i + gap, xa_mk_value(i), GFP_KERNEL);
	}

	rcu_read_lock();
	for (i = 0; i < count; i++) {
		unsigned long j = 2495;

		mas_set(&mas, ULONG_MAX);
		while ((entry = mas_prev(&mas, 0)) != NULL) {
			MT_BUG_ON(mt, entry != xa_mk_value(j));
			j -= 5;
		}
	}
	rcu_read_unlock();

}
#endif
 
static noinline void __init check_forking(struct maple_tree *mt)
{

	struct maple_tree newmt;
	int i, nr_entries = 134;
	void *val;
	MA_STATE(mas, mt, 0, 0);
	MA_STATE(newmas, mt, 0, 0);
	struct rw_semaphore newmt_lock;

	init_rwsem(&newmt_lock);

	for (i = 0; i <= nr_entries; i++)
		mtree_store_range(mt, i*10, i*10 + 5,
				  xa_mk_value(i), GFP_KERNEL);

	mt_set_non_kernel(99999);
	mt_init_flags(&newmt, MT_FLAGS_ALLOC_RANGE | MT_FLAGS_LOCK_EXTERN);
	mt_set_external_lock(&newmt, &newmt_lock);
	newmas.tree = &newmt;
	mas_reset(&newmas);
	mas_reset(&mas);
	down_write(&newmt_lock);
	mas.index = 0;
	mas.last = 0;
	if (mas_expected_entries(&newmas, nr_entries)) {
		pr_err("OOM!");
		BUG_ON(1);
	}
	rcu_read_lock();
	mas_for_each(&mas, val, ULONG_MAX) {
		newmas.index = mas.index;
		newmas.last = mas.last;
		mas_store(&newmas, val);
	}
	rcu_read_unlock();
	mas_destroy(&newmas);
	mt_validate(&newmt);
	mt_set_non_kernel(0);
	__mt_destroy(&newmt);
	up_write(&newmt_lock);
}

static noinline void __init check_iteration(struct maple_tree *mt)
{
	int i, nr_entries = 125;
	void *val;
	MA_STATE(mas, mt, 0, 0);

	for (i = 0; i <= nr_entries; i++)
		mtree_store_range(mt, i * 10, i * 10 + 9,
				  xa_mk_value(i), GFP_KERNEL);

	mt_set_non_kernel(99999);

	i = 0;
	mas_lock(&mas);
	mas_for_each(&mas, val, 925) {
		MT_BUG_ON(mt, mas.index != i * 10);
		MT_BUG_ON(mt, mas.last != i * 10 + 9);
		 
		if (i == 92) {
			mas.index = 925;
			mas.last = 929;
			mas_store(&mas, val);
		}
		i++;
	}
	 
	val = mas_find(&mas, ULONG_MAX);
	MT_BUG_ON(mt, val != xa_mk_value(i));

	mas_set(&mas, 0);
	i = 0;
	mas_for_each(&mas, val, 785) {
		MT_BUG_ON(mt, mas.index != i * 10);
		MT_BUG_ON(mt, mas.last != i * 10 + 9);
		 
		if (i == 78) {
			mas.index = 780;
			mas.last = 785;
			mas_store(&mas, val);
		} else {
			i++;
		}
	}
	val = mas_find(&mas, ULONG_MAX);
	MT_BUG_ON(mt, val != xa_mk_value(i));

	mas_set(&mas, 0);
	i = 0;
	mas_for_each(&mas, val, 765) {
		MT_BUG_ON(mt, mas.index != i * 10);
		MT_BUG_ON(mt, mas.last != i * 10 + 9);
		 
		if (i == 76) {
			mas.index = 760;
			mas.last = 765;
			mas_store(&mas, val);
		}
		i++;
	}
	 
	val = mas_find(&mas, ULONG_MAX);
	MT_BUG_ON(mt, val != xa_mk_value(76));
	mas_unlock(&mas);
	mas_destroy(&mas);
	mt_set_non_kernel(0);
}

static noinline void __init check_mas_store_gfp(struct maple_tree *mt)
{

	struct maple_tree newmt;
	int i, nr_entries = 135;
	void *val;
	MA_STATE(mas, mt, 0, 0);
	MA_STATE(newmas, mt, 0, 0);

	for (i = 0; i <= nr_entries; i++)
		mtree_store_range(mt, i*10, i*10 + 5,
				  xa_mk_value(i), GFP_KERNEL);

	mt_set_non_kernel(99999);
	mt_init_flags(&newmt, MT_FLAGS_ALLOC_RANGE);
	newmas.tree = &newmt;
	rcu_read_lock();
	mas_lock(&newmas);
	mas_reset(&newmas);
	mas_set(&mas, 0);
	mas_for_each(&mas, val, ULONG_MAX) {
		newmas.index = mas.index;
		newmas.last = mas.last;
		mas_store_gfp(&newmas, val, GFP_KERNEL);
	}
	mas_unlock(&newmas);
	rcu_read_unlock();
	mt_validate(&newmt);
	mt_set_non_kernel(0);
	mtree_destroy(&newmt);
}

#if defined(BENCH_FORK)
static noinline void __init bench_forking(struct maple_tree *mt)
{

	struct maple_tree newmt;
	int i, nr_entries = 134, nr_fork = 80000;
	void *val;
	MA_STATE(mas, mt, 0, 0);
	MA_STATE(newmas, mt, 0, 0);
	struct rw_semaphore newmt_lock;

	init_rwsem(&newmt_lock);
	mt_set_external_lock(&newmt, &newmt_lock);

	for (i = 0; i <= nr_entries; i++)
		mtree_store_range(mt, i*10, i*10 + 5,
				  xa_mk_value(i), GFP_KERNEL);

	for (i = 0; i < nr_fork; i++) {
		mt_set_non_kernel(99999);
		mt_init_flags(&newmt, MT_FLAGS_ALLOC_RANGE);
		newmas.tree = &newmt;
		mas_reset(&newmas);
		mas_reset(&mas);
		mas.index = 0;
		mas.last = 0;
		rcu_read_lock();
		down_write(&newmt_lock);
		if (mas_expected_entries(&newmas, nr_entries)) {
			printk("OOM!");
			BUG_ON(1);
		}
		mas_for_each(&mas, val, ULONG_MAX) {
			newmas.index = mas.index;
			newmas.last = mas.last;
			mas_store(&newmas, val);
		}
		mas_destroy(&newmas);
		rcu_read_unlock();
		mt_validate(&newmt);
		mt_set_non_kernel(0);
		__mt_destroy(&newmt);
		up_write(&newmt_lock);
	}
}
#endif

static noinline void __init next_prev_test(struct maple_tree *mt)
{
	int i, nr_entries;
	void *val;
	MA_STATE(mas, mt, 0, 0);
	struct maple_enode *mn;
	static const unsigned long *level2;
	static const unsigned long level2_64[] = { 707, 1000, 710, 715, 720,
						   725};
	static const unsigned long level2_32[] = { 1747, 2000, 1750, 1755,
						   1760, 1765};
	unsigned long last_index;

	if (MAPLE_32BIT) {
		nr_entries = 500;
		level2 = level2_32;
		last_index = 0x138e;
	} else {
		nr_entries = 200;
		level2 = level2_64;
		last_index = 0x7d6;
	}

	for (i = 0; i <= nr_entries; i++)
		mtree_store_range(mt, i*10, i*10 + 5,
				  xa_mk_value(i), GFP_KERNEL);

	mas_lock(&mas);
	for (i = 0; i <= nr_entries / 2; i++) {
		mas_next(&mas, 1000);
		if (mas_is_none(&mas))
			break;

	}
	mas_reset(&mas);
	mas_set(&mas, 0);
	i = 0;
	mas_for_each(&mas, val, 1000) {
		i++;
	}

	mas_reset(&mas);
	mas_set(&mas, 0);
	i = 0;
	mas_for_each(&mas, val, 1000) {
		mas_pause(&mas);
		i++;
	}

	 
	mas_set(&mas, 686);
	val = mas_walk(&mas);
	MT_BUG_ON(mt, val != NULL);

	val = mas_next(&mas, 1000);
	MT_BUG_ON(mt, val != xa_mk_value(690 / 10));
	MT_BUG_ON(mt, mas.index != 690);
	MT_BUG_ON(mt, mas.last != 695);

	val = mas_prev(&mas, 0);
	MT_BUG_ON(mt, val != xa_mk_value(680 / 10));
	MT_BUG_ON(mt, mas.index != 680);
	MT_BUG_ON(mt, mas.last != 685);

	val = mas_next(&mas, 1000);
	MT_BUG_ON(mt, val != xa_mk_value(690 / 10));
	MT_BUG_ON(mt, mas.index != 690);
	MT_BUG_ON(mt, mas.last != 695);

	val = mas_next(&mas, 1000);
	MT_BUG_ON(mt, val != xa_mk_value(700 / 10));
	MT_BUG_ON(mt, mas.index != 700);
	MT_BUG_ON(mt, mas.last != 705);

	 
	mas_set(&mas, 70);
	val = mas_walk(&mas);
	MT_BUG_ON(mt, val != xa_mk_value(70 / 10));
	MT_BUG_ON(mt, mas.index != 70);
	MT_BUG_ON(mt, mas.last != 75);

	val = mas_next(&mas, 1000);
	MT_BUG_ON(mt, val != xa_mk_value(80 / 10));
	MT_BUG_ON(mt, mas.index != 80);
	MT_BUG_ON(mt, mas.last != 85);

	val = mas_prev(&mas, 70);
	MT_BUG_ON(mt, val != xa_mk_value(70 / 10));
	MT_BUG_ON(mt, mas.index != 70);
	MT_BUG_ON(mt, mas.last != 75);

	 
	mas_reset(&mas);
	mas_set(&mas, level2[0]);
	val = mas_walk(&mas);
	MT_BUG_ON(mt, val != NULL);
	val = mas_next(&mas, level2[1]);
	MT_BUG_ON(mt, val != xa_mk_value(level2[2] / 10));
	MT_BUG_ON(mt, mas.index != level2[2]);
	MT_BUG_ON(mt, mas.last != level2[3]);
	mn = mas.node;

	val = mas_next(&mas, level2[1]);
	MT_BUG_ON(mt, val != xa_mk_value(level2[4] / 10));
	MT_BUG_ON(mt, mas.index != level2[4]);
	MT_BUG_ON(mt, mas.last != level2[5]);
	MT_BUG_ON(mt, mn == mas.node);

	val = mas_prev(&mas, 0);
	MT_BUG_ON(mt, val != xa_mk_value(level2[2] / 10));
	MT_BUG_ON(mt, mas.index != level2[2]);
	MT_BUG_ON(mt, mas.last != level2[3]);

	 
	mas_set(&mas, nr_entries * 10);
	val = mas_walk(&mas);
	MT_BUG_ON(mt, val != xa_mk_value(nr_entries));
	MT_BUG_ON(mt, mas.index != (nr_entries * 10));
	MT_BUG_ON(mt, mas.last != (nr_entries * 10 + 5));

	val = mas_next(&mas, ULONG_MAX);
	MT_BUG_ON(mt, val != NULL);
	MT_BUG_ON(mt, mas.index != last_index);
	MT_BUG_ON(mt, mas.last != ULONG_MAX);

	val = mas_prev(&mas, 0);
	MT_BUG_ON(mt, val != xa_mk_value(nr_entries));
	MT_BUG_ON(mt, mas.index != (nr_entries * 10));
	MT_BUG_ON(mt, mas.last != (nr_entries * 10 + 5));

	 
	mas_reset(&mas);
	mas_set(&mas, 10);
	val = mas_walk(&mas);
	MT_BUG_ON(mt, val != xa_mk_value(1));
	MT_BUG_ON(mt, mas.index != 10);
	MT_BUG_ON(mt, mas.last != 15);

	val = mas_prev(&mas, 0);
	MT_BUG_ON(mt, val != xa_mk_value(0));
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 5);

	val = mas_prev(&mas, 0);
	MT_BUG_ON(mt, val != NULL);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 5);
	MT_BUG_ON(mt, mas.node != MAS_UNDERFLOW);

	mas.index = 0;
	mas.last = 5;
	mas_store(&mas, NULL);
	mas_reset(&mas);
	mas_set(&mas, 10);
	mas_walk(&mas);

	val = mas_prev(&mas, 0);
	MT_BUG_ON(mt, val != NULL);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 9);
	mas_unlock(&mas);

	mtree_destroy(mt);

	mt_init(mt);
	mtree_store_range(mt, 0, 0, xa_mk_value(0), GFP_KERNEL);
	mtree_store_range(mt, 5, 5, xa_mk_value(5), GFP_KERNEL);
	rcu_read_lock();
	mas_set(&mas, 5);
	val = mas_prev(&mas, 4);
	MT_BUG_ON(mt, val != NULL);
	rcu_read_unlock();
}



 
static noinline void __init check_spanning_relatives(struct maple_tree *mt)
{

	unsigned long i, nr_entries = 1000;

	for (i = 0; i <= nr_entries; i++)
		mtree_store_range(mt, i*10, i*10 + 5,
				  xa_mk_value(i), GFP_KERNEL);


	mtree_store_range(mt, 9365, 9955, NULL, GFP_KERNEL);
}

static noinline void __init check_fuzzer(struct maple_tree *mt)
{
	 
	mtree_test_insert(mt, 88, (void *)0xb1);
	mtree_test_insert(mt, 84, (void *)0xa9);
	mtree_test_insert(mt, 2,  (void *)0x5);
	mtree_test_insert(mt, 4,  (void *)0x9);
	mtree_test_insert(mt, 14, (void *)0x1d);
	mtree_test_insert(mt, 7,  (void *)0xf);
	mtree_test_insert(mt, 12, (void *)0x19);
	mtree_test_insert(mt, 18, (void *)0x25);
	mtree_test_store_range(mt, 8, 18, (void *)0x11);
	mtree_destroy(mt);


	 
	mt_init_flags(mt, 0);
	mtree_test_store(mt, 87, (void *)0xaf);
	mtree_test_store(mt, 0, (void *)0x1);
	mtree_test_load(mt, 4);
	mtree_test_insert(mt, 4, (void *)0x9);
	mtree_test_store(mt, 8, (void *)0x11);
	mtree_test_store(mt, 44, (void *)0x59);
	mtree_test_store(mt, 68, (void *)0x89);
	mtree_test_store(mt, 2, (void *)0x5);
	mtree_test_insert(mt, 43, (void *)0x57);
	mtree_test_insert(mt, 24, (void *)0x31);
	mtree_test_insert(mt, 844, (void *)0x699);
	mtree_test_store(mt, 84, (void *)0xa9);
	mtree_test_store(mt, 4, (void *)0x9);
	mtree_test_erase(mt, 4);
	mtree_test_load(mt, 5);
	mtree_test_erase(mt, 0);
	mtree_destroy(mt);

	 
	mt_init_flags(mt, 0);
	mtree_test_store_range(mt, 0, ULONG_MAX, (void *)0x1);
	mtree_test_store(mt, 4, (void *)0x9);
	mtree_test_erase(mt, 5);
	mtree_test_erase(mt, 0);
	mtree_test_erase(mt, 4);
	mtree_test_store(mt, 5, (void *)0xb);
	mtree_test_erase(mt, 5);
	mtree_test_store(mt, 5, (void *)0xb);
	mtree_test_erase(mt, 5);
	mtree_test_erase(mt, 4);
	mtree_test_store(mt, 4, (void *)0x9);
	mtree_test_store(mt, 444, (void *)0x379);
	mtree_test_store(mt, 0, (void *)0x1);
	mtree_test_load(mt, 0);
	mtree_test_store(mt, 5, (void *)0xb);
	mtree_test_erase(mt, 0);
	mtree_destroy(mt);

	 
	mt_init_flags(mt, 0);
	mtree_test_insert(mt, 261, (void *)0x20b);
	mtree_test_store(mt, 516, (void *)0x409);
	mtree_test_store(mt, 6, (void *)0xd);
	mtree_test_insert(mt, 5, (void *)0xb);
	mtree_test_insert(mt, 1256, (void *)0x9d1);
	mtree_test_store(mt, 4, (void *)0x9);
	mtree_test_erase(mt, 1);
	mtree_test_store(mt, 56, (void *)0x71);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_store(mt, 24, (void *)0x31);
	mtree_test_erase(mt, 1);
	mtree_test_insert(mt, 2263, (void *)0x11af);
	mtree_test_insert(mt, 446, (void *)0x37d);
	mtree_test_store_range(mt, 6, 45, (void *)0xd);
	mtree_test_store_range(mt, 3, 446, (void *)0x7);
	mtree_destroy(mt);

	 
	mt_init_flags(mt, 0);
	mtree_test_store(mt, 48, (void *)0x61);
	mtree_test_store(mt, 3, (void *)0x7);
	mtree_test_load(mt, 0);
	mtree_test_store(mt, 88, (void *)0xb1);
	mtree_test_store(mt, 81, (void *)0xa3);
	mtree_test_insert(mt, 0, (void *)0x1);
	mtree_test_insert(mt, 8, (void *)0x11);
	mtree_test_insert(mt, 4, (void *)0x9);
	mtree_test_insert(mt, 2480, (void *)0x1361);
	mtree_test_insert(mt, ULONG_MAX,
			  (void *)0xffffffffffffffff);
	mtree_test_erase(mt, ULONG_MAX);
	mtree_destroy(mt);

	 
	mt_init_flags(mt, 0);
	mtree_test_erase(mt, 3);
	mtree_test_insert(mt, 22, (void *)0x2d);
	mtree_test_insert(mt, 15, (void *)0x1f);
	mtree_test_load(mt, 2);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_insert(mt, 5, (void *)0xb);
	mtree_test_erase(mt, 1);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_insert(mt, 4, (void *)0x9);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_erase(mt, 1);
	mtree_test_insert(mt, 2, (void *)0x5);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_erase(mt, 3);
	mtree_test_insert(mt, 22, (void *)0x2d);
	mtree_test_insert(mt, 15, (void *)0x1f);
	mtree_test_insert(mt, 2, (void *)0x5);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_insert(mt, 8, (void *)0x11);
	mtree_test_load(mt, 2);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_store(mt, 1, (void *)0x3);
	mtree_test_insert(mt, 5, (void *)0xb);
	mtree_test_erase(mt, 1);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_insert(mt, 4, (void *)0x9);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_erase(mt, 1);
	mtree_test_insert(mt, 2, (void *)0x5);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_erase(mt, 3);
	mtree_test_insert(mt, 22, (void *)0x2d);
	mtree_test_insert(mt, 15, (void *)0x1f);
	mtree_test_insert(mt, 2, (void *)0x5);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_insert(mt, 8, (void *)0x11);
	mtree_test_insert(mt, 12, (void *)0x19);
	mtree_test_erase(mt, 1);
	mtree_test_store_range(mt, 4, 62, (void *)0x9);
	mtree_test_erase(mt, 62);
	mtree_test_store_range(mt, 1, 0, (void *)0x3);
	mtree_test_insert(mt, 11, (void *)0x17);
	mtree_test_insert(mt, 3, (void *)0x7);
	mtree_test_insert(mt, 3, (void *)0x7);
	mtree_test_store(mt, 62, (void *)0x7d);
	mtree_test_erase(mt, 62);
	mtree_test_store_range(mt, 1, 15, (void *)0x3);
	mtree_test_erase(mt, 1);
	mtree_test_insert(mt, 22, (void *)0x2d);
	mtree_test_insert(mt, 12, (void *)0x19);
	mtree_test_erase(mt, 1);
	mtree_test_insert(mt, 3, (void *)0x7);
	mtree_test_store(mt, 62, (void *)0x7d);
	mtree_test_erase(mt, 62);
	mtree_test_insert(mt, 122, (void *)0xf5);
	mtree_test_store(mt, 3, (void *)0x7);
	mtree_test_insert(mt, 0, (void *)0x1);
	mtree_test_store_range(mt, 0, 1, (void *)0x1);
	mtree_test_insert(mt, 85, (void *)0xab);
	mtree_test_insert(mt, 72, (void *)0x91);
	mtree_test_insert(mt, 81, (void *)0xa3);
	mtree_test_insert(mt, 726, (void *)0x5ad);
	mtree_test_insert(mt, 0, (void *)0x1);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_store(mt, 51, (void *)0x67);
	mtree_test_insert(mt, 611, (void *)0x4c7);
	mtree_test_insert(mt, 485, (void *)0x3cb);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_erase(mt, 1);
	mtree_test_insert(mt, 0, (void *)0x1);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_insert_range(mt, 26, 1, (void *)0x35);
	mtree_test_load(mt, 1);
	mtree_test_store_range(mt, 1, 22, (void *)0x3);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_erase(mt, 1);
	mtree_test_load(mt, 53);
	mtree_test_load(mt, 1);
	mtree_test_store_range(mt, 1, 1, (void *)0x3);
	mtree_test_insert(mt, 222, (void *)0x1bd);
	mtree_test_insert(mt, 485, (void *)0x3cb);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_erase(mt, 1);
	mtree_test_load(mt, 0);
	mtree_test_insert(mt, 21, (void *)0x2b);
	mtree_test_insert(mt, 3, (void *)0x7);
	mtree_test_store(mt, 621, (void *)0x4db);
	mtree_test_insert(mt, 0, (void *)0x1);
	mtree_test_erase(mt, 5);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_store(mt, 62, (void *)0x7d);
	mtree_test_erase(mt, 62);
	mtree_test_store_range(mt, 1, 0, (void *)0x3);
	mtree_test_insert(mt, 22, (void *)0x2d);
	mtree_test_insert(mt, 12, (void *)0x19);
	mtree_test_erase(mt, 1);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_store_range(mt, 4, 62, (void *)0x9);
	mtree_test_erase(mt, 62);
	mtree_test_erase(mt, 1);
	mtree_test_load(mt, 1);
	mtree_test_store_range(mt, 1, 22, (void *)0x3);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_erase(mt, 1);
	mtree_test_load(mt, 53);
	mtree_test_load(mt, 1);
	mtree_test_store_range(mt, 1, 1, (void *)0x3);
	mtree_test_insert(mt, 222, (void *)0x1bd);
	mtree_test_insert(mt, 485, (void *)0x3cb);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_erase(mt, 1);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_load(mt, 0);
	mtree_test_load(mt, 0);
	mtree_destroy(mt);

	 
	mt_init_flags(mt, 0);
	mtree_test_load(mt, 1);
	mtree_test_insert(mt, 102, (void *)0xcd);
	mtree_test_erase(mt, 2);
	mtree_test_erase(mt, 0);
	mtree_test_load(mt, 0);
	mtree_test_insert(mt, 4, (void *)0x9);
	mtree_test_insert(mt, 2, (void *)0x5);
	mtree_test_insert(mt, 110, (void *)0xdd);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_insert_range(mt, 5, 0, (void *)0xb);
	mtree_test_erase(mt, 2);
	mtree_test_store(mt, 0, (void *)0x1);
	mtree_test_store(mt, 112, (void *)0xe1);
	mtree_test_insert(mt, 21, (void *)0x2b);
	mtree_test_store(mt, 1, (void *)0x3);
	mtree_test_insert_range(mt, 110, 2, (void *)0xdd);
	mtree_test_store(mt, 2, (void *)0x5);
	mtree_test_load(mt, 22);
	mtree_test_erase(mt, 2);
	mtree_test_store(mt, 210, (void *)0x1a5);
	mtree_test_store_range(mt, 0, 2, (void *)0x1);
	mtree_test_store(mt, 2, (void *)0x5);
	mtree_test_erase(mt, 2);
	mtree_test_erase(mt, 22);
	mtree_test_erase(mt, 1);
	mtree_test_erase(mt, 2);
	mtree_test_store(mt, 0, (void *)0x1);
	mtree_test_load(mt, 112);
	mtree_test_insert(mt, 2, (void *)0x5);
	mtree_test_erase(mt, 2);
	mtree_test_store(mt, 1, (void *)0x3);
	mtree_test_insert_range(mt, 1, 2, (void *)0x3);
	mtree_test_erase(mt, 0);
	mtree_test_erase(mt, 2);
	mtree_test_store(mt, 2, (void *)0x5);
	mtree_test_erase(mt, 0);
	mtree_test_erase(mt, 2);
	mtree_test_store(mt, 0, (void *)0x1);
	mtree_test_store(mt, 0, (void *)0x1);
	mtree_test_erase(mt, 2);
	mtree_test_store(mt, 2, (void *)0x5);
	mtree_test_erase(mt, 2);
	mtree_test_insert(mt, 2, (void *)0x5);
	mtree_test_insert_range(mt, 1, 2, (void *)0x3);
	mtree_test_erase(mt, 0);
	mtree_test_erase(mt, 2);
	mtree_test_store(mt, 0, (void *)0x1);
	mtree_test_load(mt, 112);
	mtree_test_store_range(mt, 110, 12, (void *)0xdd);
	mtree_test_store(mt, 2, (void *)0x5);
	mtree_test_load(mt, 110);
	mtree_test_insert_range(mt, 4, 71, (void *)0x9);
	mtree_test_load(mt, 2);
	mtree_test_store(mt, 2, (void *)0x5);
	mtree_test_insert_range(mt, 11, 22, (void *)0x17);
	mtree_test_erase(mt, 12);
	mtree_test_store(mt, 2, (void *)0x5);
	mtree_test_load(mt, 22);
	mtree_destroy(mt);


	 
	mt_init_flags(mt, 0);
	mtree_test_store(mt, 6, (void *)0xd);
	mtree_test_store(mt, 67, (void *)0x87);
	mtree_test_insert(mt, 15, (void *)0x1f);
	mtree_test_insert(mt, 6716, (void *)0x3479);
	mtree_test_store(mt, 61, (void *)0x7b);
	mtree_test_insert(mt, 13, (void *)0x1b);
	mtree_test_store(mt, 8, (void *)0x11);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_load(mt, 0);
	mtree_test_erase(mt, 67167);
	mtree_test_insert_range(mt, 6, 7167, (void *)0xd);
	mtree_test_insert(mt, 6, (void *)0xd);
	mtree_test_erase(mt, 67);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_erase(mt, 667167);
	mtree_test_insert(mt, 6, (void *)0xd);
	mtree_test_store(mt, 67, (void *)0x87);
	mtree_test_insert(mt, 5, (void *)0xb);
	mtree_test_erase(mt, 1);
	mtree_test_insert(mt, 6, (void *)0xd);
	mtree_test_erase(mt, 67);
	mtree_test_insert(mt, 15, (void *)0x1f);
	mtree_test_insert(mt, 67167, (void *)0x20cbf);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_load(mt, 7);
	mtree_test_insert(mt, 16, (void *)0x21);
	mtree_test_insert(mt, 36, (void *)0x49);
	mtree_test_store(mt, 67, (void *)0x87);
	mtree_test_store(mt, 6, (void *)0xd);
	mtree_test_insert(mt, 367, (void *)0x2df);
	mtree_test_insert(mt, 115, (void *)0xe7);
	mtree_test_store(mt, 0, (void *)0x1);
	mtree_test_store_range(mt, 1, 3, (void *)0x3);
	mtree_test_store(mt, 1, (void *)0x3);
	mtree_test_erase(mt, 67167);
	mtree_test_insert_range(mt, 6, 47, (void *)0xd);
	mtree_test_store(mt, 1, (void *)0x3);
	mtree_test_insert_range(mt, 1, 67, (void *)0x3);
	mtree_test_load(mt, 67);
	mtree_test_insert(mt, 1, (void *)0x3);
	mtree_test_erase(mt, 67167);
	mtree_destroy(mt);

	 
	mt_init_flags(mt, 0);
	mtree_test_insert(mt, 21, (void *)0x2b);
	mtree_test_insert(mt, 12, (void *)0x19);
	mtree_test_insert(mt, 6, (void *)0xd);
	mtree_test_insert(mt, 8, (void *)0x11);
	mtree_test_insert(mt, 2, (void *)0x5);
	mtree_test_insert(mt, 91, (void *)0xb7);
	mtree_test_insert(mt, 18, (void *)0x25);
	mtree_test_insert(mt, 81, (void *)0xa3);
	mtree_test_store_range(mt, 0, 128, (void *)0x1);
	mtree_test_store(mt, 1, (void *)0x3);
	mtree_test_erase(mt, 8);
	mtree_test_insert(mt, 11, (void *)0x17);
	mtree_test_insert(mt, 8, (void *)0x11);
	mtree_test_insert(mt, 21, (void *)0x2b);
	mtree_test_insert(mt, 2, (void *)0x5);
	mtree_test_insert(mt, ULONG_MAX - 10, (void *)0xffffffffffffffeb);
	mtree_test_erase(mt, ULONG_MAX - 10);
	mtree_test_store_range(mt, 0, 281, (void *)0x1);
	mtree_test_erase(mt, 2);
	mtree_test_insert(mt, 1211, (void *)0x977);
	mtree_test_insert(mt, 111, (void *)0xdf);
	mtree_test_insert(mt, 13, (void *)0x1b);
	mtree_test_insert(mt, 211, (void *)0x1a7);
	mtree_test_insert(mt, 11, (void *)0x17);
	mtree_test_insert(mt, 5, (void *)0xb);
	mtree_test_insert(mt, 1218, (void *)0x985);
	mtree_test_insert(mt, 61, (void *)0x7b);
	mtree_test_store(mt, 1, (void *)0x3);
	mtree_test_insert(mt, 121, (void *)0xf3);
	mtree_test_insert(mt, 8, (void *)0x11);
	mtree_test_insert(mt, 21, (void *)0x2b);
	mtree_test_insert(mt, 2, (void *)0x5);
	mtree_test_insert(mt, ULONG_MAX - 10, (void *)0xffffffffffffffeb);
	mtree_test_erase(mt, ULONG_MAX - 10);
}

 
static noinline void __init check_dup_gaps(struct maple_tree *mt,
				    unsigned long nr_entries, bool zero_start,
				    unsigned long gap)
{
	unsigned long i = 0;
	struct maple_tree newmt;
	int ret;
	void *tmp;
	MA_STATE(mas, mt, 0, 0);
	MA_STATE(newmas, &newmt, 0, 0);
	struct rw_semaphore newmt_lock;

	init_rwsem(&newmt_lock);
	mt_set_external_lock(&newmt, &newmt_lock);

	if (!zero_start)
		i = 1;

	mt_zero_nr_tallocated();
	for (; i <= nr_entries; i++)
		mtree_store_range(mt, i*10, (i+1)*10 - gap,
				  xa_mk_value(i), GFP_KERNEL);

	mt_init_flags(&newmt, MT_FLAGS_ALLOC_RANGE | MT_FLAGS_LOCK_EXTERN);
	mt_set_non_kernel(99999);
	down_write(&newmt_lock);
	ret = mas_expected_entries(&newmas, nr_entries);
	mt_set_non_kernel(0);
	MT_BUG_ON(mt, ret != 0);

	rcu_read_lock();
	mas_for_each(&mas, tmp, ULONG_MAX) {
		newmas.index = mas.index;
		newmas.last = mas.last;
		mas_store(&newmas, tmp);
	}
	rcu_read_unlock();
	mas_destroy(&newmas);

	__mt_destroy(&newmt);
	up_write(&newmt_lock);
}

 
static noinline void __init check_dup(struct maple_tree *mt)
{
	int i;
	int big_start = 100010;

	 
	for (i = 10; i < 1000; i++) {
		mt_init_flags(mt, MT_FLAGS_ALLOC_RANGE);
		check_dup_gaps(mt, i, true, 5);
		mtree_destroy(mt);
		rcu_barrier();
	}

	cond_resched();
	mt_cache_shrink();
	 
	for (i = 1000; i < 2000; i++) {
		mt_init_flags(mt, MT_FLAGS_ALLOC_RANGE);
		check_dup_gaps(mt, i, true, 0);
		mtree_destroy(mt);
		rcu_barrier();
	}

	cond_resched();
	mt_cache_shrink();
	 
	for (i = big_start; i < big_start + 10; i++) {
		mt_init_flags(mt, MT_FLAGS_ALLOC_RANGE);
		check_dup_gaps(mt, i, true, 5);
		mtree_destroy(mt);
		rcu_barrier();
	}

	cond_resched();
	mt_cache_shrink();
	 
	for (i = 200; i < 1000; i++) {
		mt_init_flags(mt, MT_FLAGS_ALLOC_RANGE);
		check_dup_gaps(mt, i, false, 5);
		mtree_destroy(mt);
		rcu_barrier();
	}

	cond_resched();
	mt_cache_shrink();
	 
	for (i = big_start; i < big_start + 10; i++) {
		mt_init_flags(mt, MT_FLAGS_ALLOC_RANGE);
		check_dup_gaps(mt, i, false, 5);
		mtree_destroy(mt);
		rcu_barrier();
		cond_resched();
		mt_cache_shrink();
	}

	 
	for (i = 1500; i < 3000; i++) {
		mt_init_flags(mt, 0);
		check_dup_gaps(mt, i, false, 5);
		mtree_destroy(mt);
		rcu_barrier();
		cond_resched();
		if (i % 2 == 0)
			mt_cache_shrink();
	}

	mt_cache_shrink();
	 
	for (i = 200; i < 1000; i++) {
		mt_init_flags(mt, 0);
		check_dup_gaps(mt, i, true, 5);
		mtree_destroy(mt);
		rcu_barrier();
		cond_resched();
	}

	mt_cache_shrink();
	 
	for (i = big_start + 5; i < big_start + 10; i++) {
		mt_init_flags(mt, 0);
		check_dup_gaps(mt, i, true, 5);
		mtree_destroy(mt);
		rcu_barrier();
		mt_cache_shrink();
		cond_resched();
	}
}

static noinline void __init check_bnode_min_spanning(struct maple_tree *mt)
{
	int i = 50;
	MA_STATE(mas, mt, 0, 0);

	mt_set_non_kernel(9999);
	mas_lock(&mas);
	do {
		mas_set_range(&mas, i*10, i*10+9);
		mas_store(&mas, check_bnode_min_spanning);
	} while (i--);

	mas_set_range(&mas, 240, 509);
	mas_store(&mas, NULL);
	mas_unlock(&mas);
	mas_destroy(&mas);
	mt_set_non_kernel(0);
}

static noinline void __init check_empty_area_window(struct maple_tree *mt)
{
	unsigned long i, nr_entries = 20;
	MA_STATE(mas, mt, 0, 0);

	for (i = 1; i <= nr_entries; i++)
		mtree_store_range(mt, i*10, i*10 + 9,
				  xa_mk_value(i), GFP_KERNEL);

	 
	mtree_store_range(mt, 160, 169, NULL, GFP_KERNEL);

	 
	rcu_read_lock();
	MT_BUG_ON(mt, mas_empty_area_rev(&mas, 5, 90, 10) != -EBUSY);

	mas_reset(&mas);
	MT_BUG_ON(mt, mas_empty_area_rev(&mas, 6, 90, 5) != -EBUSY);

	 
	mas_reset(&mas);
	MT_BUG_ON(mt, mas_empty_area_rev(&mas, 5, 90, 5) != 0);
	MT_BUG_ON(mt, mas.index != 5);
	MT_BUG_ON(mt, mas.last != 9);
	rcu_read_unlock();

	 
	rcu_read_lock();
	mas_reset(&mas);
	MT_BUG_ON(mt, mas_empty_area_rev(&mas, 5, 217, 9) != 0);
	MT_BUG_ON(mt, mas.index != 161);
	MT_BUG_ON(mt, mas.last != 169);

	 
	mas_reset(&mas);
	MT_BUG_ON(mt, mas_empty_area_rev(&mas, 100, 218, 3) != 0);
	MT_BUG_ON(mt, mas.index != 216);
	MT_BUG_ON(mt, mas.last != 218);

	 
	mas_reset(&mas);
	MT_BUG_ON(mt, mas_empty_area_rev(&mas, 100, 218, 16) != -EBUSY);

	 
	mas_reset(&mas);
	MT_BUG_ON(mt, mas_empty_area_rev(&mas, 167, 200, 4) != -EBUSY);

	 
	mas_reset(&mas);
	MT_BUG_ON(mt, mas_empty_area_rev(&mas, 100, 162, 4) != -EBUSY);

	 
	mas_reset(&mas);
	MT_BUG_ON(mt, mas_empty_area(&mas, 0, 100, 9) != 0);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 8);

	mas_reset(&mas);
	MT_BUG_ON(mt, mas_empty_area(&mas, 0, 100, 4) != 0);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 3);

	mas_reset(&mas);
	MT_BUG_ON(mt, mas_empty_area(&mas, 0, 100, 11) != -EBUSY);

	mas_reset(&mas);
	MT_BUG_ON(mt, mas_empty_area(&mas, 5, 100, 6) != -EBUSY);

	mas_reset(&mas);
	MT_BUG_ON(mt, mas_empty_area(&mas, 0, 8, 10) != -EINVAL);

	mas_reset(&mas);
	mas_empty_area(&mas, 100, 165, 3);

	mas_reset(&mas);
	MT_BUG_ON(mt, mas_empty_area(&mas, 100, 163, 6) != -EBUSY);
	rcu_read_unlock();
}

static noinline void __init check_empty_area_fill(struct maple_tree *mt)
{
	const unsigned long max = 0x25D78000;
	unsigned long size;
	int loop, shift;
	MA_STATE(mas, mt, 0, 0);

	mt_set_non_kernel(99999);
	for (shift = 12; shift <= 16; shift++) {
		loop = 5000;
		size = 1 << shift;
		while (loop--) {
			mas_set(&mas, 0);
			mas_lock(&mas);
			MT_BUG_ON(mt, mas_empty_area(&mas, 0, max, size) != 0);
			MT_BUG_ON(mt, mas.last != mas.index + size - 1);
			mas_store_gfp(&mas, (void *)size, GFP_KERNEL);
			mas_unlock(&mas);
			mas_reset(&mas);
		}
	}

	 
	size = 0x1000;
	rcu_read_lock();
	MT_BUG_ON(mt, mas_empty_area(&mas, 0, max, size) != -EBUSY);
	rcu_read_unlock();

	 
	for (unsigned long i = 629440511; i <= 629440800; i += 6)
		mtree_store_range(mt, i, i + 5, (void *)i, GFP_KERNEL);
	 
	mtree_erase(mt, 631668735);
	 
	mtree_erase(mt, 629506047);
	mas_reset(&mas);
	 
	rcu_read_lock();
	MT_BUG_ON(mt, mas_empty_area(&mas, 629506048, 690000000, 0x5000) != 0);
	rcu_read_unlock();
	mt_set_non_kernel(0);
}

 

#define mas_active(x)		(((x).node != MAS_ROOT) && \
				 ((x).node != MAS_START) && \
				 ((x).node != MAS_PAUSE) && \
				 ((x).node != MAS_NONE))
static noinline void __init check_state_handling(struct maple_tree *mt)
{
	MA_STATE(mas, mt, 0, 0);
	void *entry, *ptr = (void *) 0x1234500;
	void *ptr2 = &ptr;
	void *ptr3 = &ptr2;

	 
	mtree_store_range(mt, 0, 0, ptr, GFP_KERNEL);

	mas_lock(&mas);
	 
	entry = mas_prev(&mas, 0);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.node != MAS_UNDERFLOW);

	 
	mas_set(&mas, 10);
	entry = mas_prev(&mas, 0);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 0);
	MT_BUG_ON(mt, mas.node != MAS_ROOT);

	 
	mas_set(&mas, 10);
	mas_pause(&mas);
	entry = mas_prev(&mas, 0);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 0);
	MT_BUG_ON(mt, mas.node != MAS_ROOT);

	 
	mas_set(&mas, 0);
	entry = mas_next(&mas, ULONG_MAX);
	MT_BUG_ON(mt, mas.index != 1);
	MT_BUG_ON(mt, mas.last != ULONG_MAX);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.node != MAS_NONE);

	 
	mas_set(&mas, 10);
	entry = mas_next(&mas, ULONG_MAX);
	MT_BUG_ON(mt, mas.index != 1);
	MT_BUG_ON(mt, mas.last != ULONG_MAX);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.node != MAS_NONE);

	 
	mas_set(&mas, 0);
	entry = mas_find(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 0);
	MT_BUG_ON(mt, mas.node != MAS_ROOT);

	 
	entry = mas_find(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 1);
	MT_BUG_ON(mt, mas.last != ULONG_MAX);
	MT_BUG_ON(mt, mas.node != MAS_NONE);

	 
	entry = mas_find(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 1);
	MT_BUG_ON(mt, mas.last != ULONG_MAX);
	MT_BUG_ON(mt, mas.node != MAS_NONE);

	 
	mas_set(&mas, 10);
	entry = mas_find(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 1);
	MT_BUG_ON(mt, mas.last != ULONG_MAX);
	MT_BUG_ON(mt, mas.node != MAS_NONE);

	 
	entry = mas_find_rev(&mas, 0);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 0);
	MT_BUG_ON(mt, mas.node != MAS_ROOT);

	 
	mas_set(&mas, 0);
	entry = mas_find_rev(&mas, 0);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 0);
	MT_BUG_ON(mt, mas.node != MAS_ROOT);

	 
	entry = mas_find_rev(&mas, 0);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 0);
	MT_BUG_ON(mt, mas.node != MAS_NONE);

	 
	entry = mas_find_rev(&mas, 0);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 0);
	MT_BUG_ON(mt, mas.node != MAS_NONE);

	 
	mas_set(&mas, 10);
	entry = mas_find_rev(&mas, 0);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 0);
	MT_BUG_ON(mt, mas.node != MAS_ROOT);

	 
	mas_set(&mas, 10);
	entry = mas_walk(&mas);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 1);
	MT_BUG_ON(mt, mas.last != ULONG_MAX);
	MT_BUG_ON(mt, mas.node != MAS_NONE);

	 
	mas_set(&mas, 10);
	mas_pause(&mas);
	entry = mas_walk(&mas);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 1);
	MT_BUG_ON(mt, mas.last != ULONG_MAX);
	MT_BUG_ON(mt, mas.node != MAS_NONE);

	 
	mas.index = mas.last = 10;
	entry = mas_walk(&mas);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 1);
	MT_BUG_ON(mt, mas.last != ULONG_MAX);
	MT_BUG_ON(mt, mas.node != MAS_NONE);

	 
	entry = mas_walk(&mas);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 1);
	MT_BUG_ON(mt, mas.last != ULONG_MAX);
	MT_BUG_ON(mt, mas.node != MAS_NONE);

	 
	mas_set(&mas, 0);
	entry = mas_walk(&mas);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 0);
	MT_BUG_ON(mt, mas.node != MAS_ROOT);

	 
	mas_set(&mas, 0);
	mas_pause(&mas);
	entry = mas_walk(&mas);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 0);
	MT_BUG_ON(mt, mas.node != MAS_ROOT);

	 
	mas.node = MAS_NONE;
	entry = mas_walk(&mas);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 0);
	MT_BUG_ON(mt, mas.node != MAS_ROOT);

	 
	entry = mas_walk(&mas);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 0);
	MT_BUG_ON(mt, mas.node != MAS_ROOT);

	 
	mas_set(&mas, 10);
	entry = mas_walk(&mas);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 1);
	MT_BUG_ON(mt, mas.last != ULONG_MAX);
	MT_BUG_ON(mt, mas.node != MAS_NONE);

	 
	mas.index = mas.last = 0;
	entry = mas_walk(&mas);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 0);
	MT_BUG_ON(mt, mas.node != MAS_ROOT);

	mas_unlock(&mas);

	 
	mtree_store_range(mt, 0, 0, NULL, GFP_KERNEL);
	mtree_store_range(mt, 0x1000, 0x1500, ptr, GFP_KERNEL);
	mtree_store_range(mt, 0x2000, 0x2500, ptr2, GFP_KERNEL);
	mtree_store_range(mt, 0x3000, 0x3500, ptr3, GFP_KERNEL);

	mas_lock(&mas);

	 
	mas_set(&mas, 0);
	entry = mas_next(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0x1000);
	MT_BUG_ON(mt, mas.last != 0x1500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	mas_set(&mas, 0);
	mas_pause(&mas);
	entry = mas_next(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0x1000);
	MT_BUG_ON(mt, mas.last != 0x1500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	mas.index = mas.last = 0;
	mas.offset = 0;
	mas.node = MAS_NONE;
	entry = mas_next(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0x1000);
	MT_BUG_ON(mt, mas.last != 0x1500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	entry = mas_next(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != ptr2);
	MT_BUG_ON(mt, mas.index != 0x2000);
	MT_BUG_ON(mt, mas.last != 0x2500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	entry = mas_next(&mas, 0x2999);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 0x2501);
	MT_BUG_ON(mt, mas.last != 0x2fff);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	entry = mas_next(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != ptr3);
	MT_BUG_ON(mt, mas.index != 0x3000);
	MT_BUG_ON(mt, mas.last != 0x3500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	entry = mas_next(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 0x3501);
	MT_BUG_ON(mt, mas.last != ULONG_MAX);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	entry = mas_next(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 0x3501);
	MT_BUG_ON(mt, mas.last != ULONG_MAX);
	MT_BUG_ON(mt, mas.node != MAS_OVERFLOW);

	 
	entry = mas_next(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 0x3501);
	MT_BUG_ON(mt, mas.last != ULONG_MAX);
	MT_BUG_ON(mt, mas.node != MAS_OVERFLOW);

	 
	entry = mas_prev(&mas, 0);
	MT_BUG_ON(mt, entry != ptr3);
	MT_BUG_ON(mt, mas.index != 0x3000);
	MT_BUG_ON(mt, mas.last != 0x3500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	mas_set(&mas, 0);
	entry = mas_next(&mas, ULONG_MAX);
	mas.node = MAS_NONE;
	mas.offset = 0;
	entry = mas_next(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != ptr2);
	MT_BUG_ON(mt, mas.index != 0x2000);
	MT_BUG_ON(mt, mas.last != 0x2500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	entry = mas_prev(&mas, 0);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0x1000);
	MT_BUG_ON(mt, mas.last != 0x1500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	entry = mas_prev(&mas, 0x0100);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 0x0FFF);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	entry = mas_prev(&mas, 0);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 0x0FFF);
	MT_BUG_ON(mt, mas.node != MAS_UNDERFLOW);

	 
	entry = mas_prev(&mas, 0);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 0x0FFF);
	MT_BUG_ON(mt, mas.node != MAS_UNDERFLOW);

	 
	entry = mas_next(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0x1000);
	MT_BUG_ON(mt, mas.last != 0x1500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	entry = mas_prev(&mas, 0x1000);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 0x1000);
	MT_BUG_ON(mt, mas.last != 0x1500);
	MT_BUG_ON(mt, mas.node != MAS_UNDERFLOW);

	 
	entry = mas_find(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0x1000);
	MT_BUG_ON(mt, mas.last != 0x1500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	mas_set(&mas, 0x3600);
	entry = mas_prev(&mas, 0);
	MT_BUG_ON(mt, entry != ptr3);
	mas_pause(&mas);
	entry = mas_prev(&mas, 0);
	MT_BUG_ON(mt, entry != ptr2);
	MT_BUG_ON(mt, mas.index != 0x2000);
	MT_BUG_ON(mt, mas.last != 0x2500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	entry = mas_prev(&mas, 0x1600);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 0x1501);
	MT_BUG_ON(mt, mas.last != 0x1FFF);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	entry = mas_prev(&mas, 0);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0x1000);
	MT_BUG_ON(mt, mas.last != 0x1500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	mas_set(&mas, 0);
	entry = mas_find(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0x1000);
	MT_BUG_ON(mt, mas.last != 0x1500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	mas_set(&mas, 0);
	mas_pause(&mas);
	entry = mas_find(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0x1000);
	MT_BUG_ON(mt, mas.last != 0x1500);
	MT_BUG_ON(mt, !mas_active(mas));

	 ;
	mas_set(&mas, 1200);
	entry = mas_find(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0x1000);
	MT_BUG_ON(mt, mas.last != 0x1500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	entry = mas_find(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != ptr2);
	MT_BUG_ON(mt, mas.index != 0x2000);
	MT_BUG_ON(mt, mas.last != 0x2500);
	MT_BUG_ON(mt, !mas_active(mas));


	 
	entry = mas_find(&mas, 0x2700);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 0x2501);
	MT_BUG_ON(mt, mas.last != 0x2FFF);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	entry = mas_find(&mas, 0x5000);
	MT_BUG_ON(mt, entry != ptr3);
	MT_BUG_ON(mt, mas.index != 0x3000);
	MT_BUG_ON(mt, mas.last != 0x3500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	entry = mas_find(&mas, ULONG_MAX);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 0x3501);
	MT_BUG_ON(mt, mas.last != ULONG_MAX);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	entry = mas_find_rev(&mas, 0);
	MT_BUG_ON(mt, entry != ptr3);
	MT_BUG_ON(mt, mas.index != 0x3000);
	MT_BUG_ON(mt, mas.last != 0x3500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	entry = mas_find_rev(&mas, 0);
	MT_BUG_ON(mt, entry != ptr2);
	MT_BUG_ON(mt, mas.index != 0x2000);
	MT_BUG_ON(mt, mas.last != 0x2500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	mas_pause(&mas);
	entry = mas_find_rev(&mas, 0);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0x1000);
	MT_BUG_ON(mt, mas.last != 0x1500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	entry = mas_find_rev(&mas, 0);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 0);
	MT_BUG_ON(mt, mas.last != 0x0FFF);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	mas_set(&mas, 0x1200);
	entry = mas_find_rev(&mas, 0);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0x1000);
	MT_BUG_ON(mt, mas.last != 0x1500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	mas_set(&mas, 0x1200);
	entry = mas_walk(&mas);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0x1000);
	MT_BUG_ON(mt, mas.last != 0x1500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	mas_set(&mas, 0x1600);
	entry = mas_walk(&mas);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 0x1501);
	MT_BUG_ON(mt, mas.last != 0x1fff);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	mas_set(&mas, 0x1200);
	mas_pause(&mas);
	entry = mas_walk(&mas);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0x1000);
	MT_BUG_ON(mt, mas.last != 0x1500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	mas_set(&mas, 0x1600);
	mas_pause(&mas);
	entry = mas_walk(&mas);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 0x1501);
	MT_BUG_ON(mt, mas.last != 0x1fff);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	mas_set(&mas, 0x1200);
	mas.node = MAS_NONE;
	entry = mas_walk(&mas);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0x1000);
	MT_BUG_ON(mt, mas.last != 0x1500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	mas_set(&mas, 0x1600);
	mas.node = MAS_NONE;
	entry = mas_walk(&mas);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 0x1501);
	MT_BUG_ON(mt, mas.last != 0x1fff);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	mas.index = 0x1200;
	mas.last = 0x1200;
	mas.offset = 0;
	entry = mas_walk(&mas);
	MT_BUG_ON(mt, entry != ptr);
	MT_BUG_ON(mt, mas.index != 0x1000);
	MT_BUG_ON(mt, mas.last != 0x1500);
	MT_BUG_ON(mt, !mas_active(mas));

	 
	mas.index = 0x1600;
	mas.last = 0x1600;
	entry = mas_walk(&mas);
	MT_BUG_ON(mt, entry != NULL);
	MT_BUG_ON(mt, mas.index != 0x1501);
	MT_BUG_ON(mt, mas.last != 0x1fff);
	MT_BUG_ON(mt, !mas_active(mas));

	mas_unlock(&mas);
}

static DEFINE_MTREE(tree);
static int __init maple_tree_seed(void)
{
	unsigned long set[] = { 5015, 5014, 5017, 25, 1000,
				1001, 1002, 1003, 1005, 0,
				5003, 5002};
	void *ptr = &set;

	pr_info("\nTEST STARTING\n\n");

	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	check_root_expand(&tree);
	mtree_destroy(&tree);

#if defined(BENCH_SLOT_STORE)
#define BENCH
	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	bench_slot_store(&tree);
	mtree_destroy(&tree);
	goto skip;
#endif
#if defined(BENCH_NODE_STORE)
#define BENCH
	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	bench_node_store(&tree);
	mtree_destroy(&tree);
	goto skip;
#endif
#if defined(BENCH_AWALK)
#define BENCH
	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	bench_awalk(&tree);
	mtree_destroy(&tree);
	goto skip;
#endif
#if defined(BENCH_WALK)
#define BENCH
	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	bench_walk(&tree);
	mtree_destroy(&tree);
	goto skip;
#endif
#if defined(BENCH_FORK)
#define BENCH
	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	bench_forking(&tree);
	mtree_destroy(&tree);
	goto skip;
#endif
#if defined(BENCH_MT_FOR_EACH)
#define BENCH
	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	bench_mt_for_each(&tree);
	mtree_destroy(&tree);
	goto skip;
#endif
#if defined(BENCH_MAS_FOR_EACH)
#define BENCH
	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	bench_mas_for_each(&tree);
	mtree_destroy(&tree);
	goto skip;
#endif
#if defined(BENCH_MAS_PREV)
#define BENCH
	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	bench_mas_prev(&tree);
	mtree_destroy(&tree);
	goto skip;
#endif

	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	check_iteration(&tree);
	mtree_destroy(&tree);

	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	check_forking(&tree);
	mtree_destroy(&tree);

	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	check_mas_store_gfp(&tree);
	mtree_destroy(&tree);

	 
	mt_init_flags(&tree, 0);
	check_ranges(&tree);
	mtree_destroy(&tree);

#if defined(CONFIG_64BIT)
	 
	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	check_alloc_range(&tree);
	mtree_destroy(&tree);

	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	check_alloc_rev_range(&tree);
	mtree_destroy(&tree);
#endif

	mt_init_flags(&tree, 0);

	check_load(&tree, set[0], NULL);        

	check_insert(&tree, set[9], &tree);      
	check_load(&tree, set[9], &tree);        
	check_load(&tree, set[0], NULL);        

	check_insert(&tree, set[10], ptr);       
	check_load(&tree, set[9], &tree);        
	check_load(&tree, set[11], NULL);        
	check_load(&tree, set[10], ptr);        

	 
	mtree_destroy(&tree);

	 
	mt_init_flags(&tree, 0);
	check_insert(&tree, set[0], &tree);      
	check_dup_insert(&tree, set[0], &tree);  
	check_load(&tree, set[0], &tree);        

	 
	check_load(&tree, set[1], NULL);         
	check_insert(&tree, set[1], ptr);        
	check_load(&tree, set[1], ptr);          
	check_load(&tree, set[0], &tree);        
	 
	check_insert(&tree, set[6], ptr);        
	check_insert(&tree, set[7], &tree);        

	check_load(&tree, set[0], &tree);        
	check_load(&tree, set[1], ptr);          
	check_load(&tree, set[6], ptr);          
	check_load(&tree, set[7], &tree);        

	 
	mtree_destroy(&tree);

	mt_init_flags(&tree, 0);
	 
	check_insert(&tree, set[5], ptr);        
	check_insert(&tree, set[7], &tree);        
	check_insert(&tree, set[6], ptr);        
	check_load(&tree, set[5], ptr);          
	check_load(&tree, set[6], ptr);          
	check_load(&tree, set[7], &tree);        

	 
	mtree_destroy(&tree);

	mt_init_flags(&tree, 0);
	 

	check_insert(&tree, set[0], ptr);  
	check_insert(&tree, set[1], &tree);  
	check_insert(&tree, set[2], ptr);  
	check_insert(&tree, set[3], &tree);  
	check_load(&tree, set[0], ptr);
	check_load(&tree, set[1], &tree);
	check_load(&tree, set[2], ptr);
	check_load(&tree, set[3], &tree);
	check_insert(&tree, set[4], ptr);  
	check_load(&tree, set[0], ptr);
	check_load(&tree, set[1], &tree);
	check_load(&tree, set[2], ptr);
	check_load(&tree, set[3], &tree);  
	check_load(&tree, set[4], ptr);
	check_insert(&tree, set[5], &tree);  
	check_load(&tree, set[0], ptr);
	check_load(&tree, set[1], &tree);
	check_load(&tree, set[2], ptr);
	check_load(&tree, set[3], &tree);
	check_load(&tree, set[4], ptr);
	check_load(&tree, set[5], &tree);
	check_insert(&tree, set[6], ptr);
	check_load(&tree, set[0], ptr);
	check_load(&tree, set[1], &tree);
	check_load(&tree, set[2], ptr);
	check_load(&tree, set[3], &tree);
	check_load(&tree, set[4], ptr);
	check_load(&tree, set[5], &tree);
	check_load(&tree, set[6], ptr);
	check_insert(&tree, set[7], &tree);
	check_load(&tree, set[0], ptr);
	check_insert(&tree, set[8], ptr);

	check_insert(&tree, set[9], &tree);

	check_load(&tree, set[0], ptr);
	check_load(&tree, set[1], &tree);
	check_load(&tree, set[2], ptr);
	check_load(&tree, set[3], &tree);
	check_load(&tree, set[4], ptr);
	check_load(&tree, set[5], &tree);
	check_load(&tree, set[6], ptr);
	check_load(&tree, set[9], &tree);
	mtree_destroy(&tree);

	mt_init_flags(&tree, 0);
	check_seq(&tree, 16, false);
	mtree_destroy(&tree);

	mt_init_flags(&tree, 0);
	check_seq(&tree, 1000, true);
	mtree_destroy(&tree);

	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	check_rev_seq(&tree, 1000, true);
	mtree_destroy(&tree);

	check_lower_bound_split(&tree);
	check_upper_bound_split(&tree);
	check_mid_split(&tree);

	mt_init_flags(&tree, 0);
	check_next_entry(&tree);
	check_find(&tree);
	check_find_2(&tree);
	mtree_destroy(&tree);

	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	check_prev_entry(&tree);
	mtree_destroy(&tree);

	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	check_gap_combining(&tree);
	mtree_destroy(&tree);

	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	check_node_overwrite(&tree);
	mtree_destroy(&tree);

	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	next_prev_test(&tree);
	mtree_destroy(&tree);

	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	check_spanning_relatives(&tree);
	mtree_destroy(&tree);

	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	check_rev_find(&tree);
	mtree_destroy(&tree);

	mt_init_flags(&tree, 0);
	check_fuzzer(&tree);
	mtree_destroy(&tree);

	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	check_dup(&tree);
	mtree_destroy(&tree);

	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	check_bnode_min_spanning(&tree);
	mtree_destroy(&tree);

	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	check_empty_area_window(&tree);
	mtree_destroy(&tree);

	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	check_empty_area_fill(&tree);
	mtree_destroy(&tree);

	mt_init_flags(&tree, MT_FLAGS_ALLOC_RANGE);
	check_state_handling(&tree);
	mtree_destroy(&tree);

#if defined(BENCH)
skip:
#endif
	rcu_barrier();
	pr_info("maple_tree: %u of %u tests passed\n",
			atomic_read(&maple_tree_tests_passed),
			atomic_read(&maple_tree_tests_run));
	if (atomic_read(&maple_tree_tests_run) ==
	    atomic_read(&maple_tree_tests_passed))
		return 0;

	return -EINVAL;
}

static void __exit maple_tree_harvest(void)
{

}

module_init(maple_tree_seed);
module_exit(maple_tree_harvest);
MODULE_AUTHOR("Liam R. Howlett <Liam.Howlett@Oracle.com>");
MODULE_LICENSE("GPL");
