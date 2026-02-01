
 

#define pr_fmt(fmt) "stackdepot: " fmt

#include <linux/gfp.h>
#include <linux/jhash.h>
#include <linux/kernel.h>
#include <linux/kmsan.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/percpu.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/stacktrace.h>
#include <linux/stackdepot.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/memblock.h>
#include <linux/kasan-enabled.h>

#define DEPOT_HANDLE_BITS (sizeof(depot_stack_handle_t) * 8)

#define DEPOT_VALID_BITS 1
#define DEPOT_POOL_ORDER 2  
#define DEPOT_POOL_SIZE (1LL << (PAGE_SHIFT + DEPOT_POOL_ORDER))
#define DEPOT_STACK_ALIGN 4
#define DEPOT_OFFSET_BITS (DEPOT_POOL_ORDER + PAGE_SHIFT - DEPOT_STACK_ALIGN)
#define DEPOT_POOL_INDEX_BITS (DEPOT_HANDLE_BITS - DEPOT_VALID_BITS - \
			       DEPOT_OFFSET_BITS - STACK_DEPOT_EXTRA_BITS)
#define DEPOT_POOLS_CAP 8192
#define DEPOT_MAX_POOLS \
	(((1LL << (DEPOT_POOL_INDEX_BITS)) < DEPOT_POOLS_CAP) ? \
	 (1LL << (DEPOT_POOL_INDEX_BITS)) : DEPOT_POOLS_CAP)

 
union handle_parts {
	depot_stack_handle_t handle;
	struct {
		u32 pool_index	: DEPOT_POOL_INDEX_BITS;
		u32 offset	: DEPOT_OFFSET_BITS;
		u32 valid	: DEPOT_VALID_BITS;
		u32 extra	: STACK_DEPOT_EXTRA_BITS;
	};
};

struct stack_record {
	struct stack_record *next;	 
	u32 hash;			 
	u32 size;			 
	union handle_parts handle;
	unsigned long entries[];	 
};

static bool stack_depot_disabled;
static bool __stack_depot_early_init_requested __initdata = IS_ENABLED(CONFIG_STACKDEPOT_ALWAYS_INIT);
static bool __stack_depot_early_init_passed __initdata;

 
#define STACK_HASH_TABLE_SCALE 14
 
#define STACK_BUCKET_NUMBER_ORDER_MIN 12
#define STACK_BUCKET_NUMBER_ORDER_MAX 20
 
#define STACK_HASH_SEED 0x9747b28c

 
static struct stack_record **stack_table;
 
static unsigned int stack_bucket_number_order;
 
static unsigned int stack_hash_mask;

 
static void *stack_pools[DEPOT_MAX_POOLS];
 
static int pool_index;
 
static size_t pool_offset;
 
static DEFINE_RAW_SPINLOCK(pool_lock);
 
static int next_pool_required = 1;

static int __init disable_stack_depot(char *str)
{
	int ret;

	ret = kstrtobool(str, &stack_depot_disabled);
	if (!ret && stack_depot_disabled) {
		pr_info("disabled\n");
		stack_table = NULL;
	}
	return 0;
}
early_param("stack_depot_disable", disable_stack_depot);

void __init stack_depot_request_early_init(void)
{
	 
	WARN_ON(__stack_depot_early_init_passed);

	__stack_depot_early_init_requested = true;
}

 
int __init stack_depot_early_init(void)
{
	unsigned long entries = 0;

	 
	if (WARN_ON(__stack_depot_early_init_passed))
		return 0;
	__stack_depot_early_init_passed = true;

	 
	if (kasan_enabled() && !stack_bucket_number_order)
		stack_bucket_number_order = STACK_BUCKET_NUMBER_ORDER_MAX;

	if (!__stack_depot_early_init_requested || stack_depot_disabled)
		return 0;

	 
	if (stack_bucket_number_order)
		entries = 1UL << stack_bucket_number_order;
	pr_info("allocating hash table via alloc_large_system_hash\n");
	stack_table = alloc_large_system_hash("stackdepot",
						sizeof(struct stack_record *),
						entries,
						STACK_HASH_TABLE_SCALE,
						HASH_EARLY | HASH_ZERO,
						NULL,
						&stack_hash_mask,
						1UL << STACK_BUCKET_NUMBER_ORDER_MIN,
						1UL << STACK_BUCKET_NUMBER_ORDER_MAX);
	if (!stack_table) {
		pr_err("hash table allocation failed, disabling\n");
		stack_depot_disabled = true;
		return -ENOMEM;
	}

	return 0;
}

 
int stack_depot_init(void)
{
	static DEFINE_MUTEX(stack_depot_init_mutex);
	unsigned long entries;
	int ret = 0;

	mutex_lock(&stack_depot_init_mutex);

	if (stack_depot_disabled || stack_table)
		goto out_unlock;

	 
	if (stack_bucket_number_order) {
		entries = 1UL << stack_bucket_number_order;
	} else {
		int scale = STACK_HASH_TABLE_SCALE;

		entries = nr_free_buffer_pages();
		entries = roundup_pow_of_two(entries);

		if (scale > PAGE_SHIFT)
			entries >>= (scale - PAGE_SHIFT);
		else
			entries <<= (PAGE_SHIFT - scale);
	}

	if (entries < 1UL << STACK_BUCKET_NUMBER_ORDER_MIN)
		entries = 1UL << STACK_BUCKET_NUMBER_ORDER_MIN;
	if (entries > 1UL << STACK_BUCKET_NUMBER_ORDER_MAX)
		entries = 1UL << STACK_BUCKET_NUMBER_ORDER_MAX;

	pr_info("allocating hash table of %lu entries via kvcalloc\n", entries);
	stack_table = kvcalloc(entries, sizeof(struct stack_record *), GFP_KERNEL);
	if (!stack_table) {
		pr_err("hash table allocation failed, disabling\n");
		stack_depot_disabled = true;
		ret = -ENOMEM;
		goto out_unlock;
	}
	stack_hash_mask = entries - 1;

out_unlock:
	mutex_unlock(&stack_depot_init_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(stack_depot_init);

 
static void depot_init_pool(void **prealloc)
{
	 
	if (!smp_load_acquire(&next_pool_required))
		return;

	 
	if (stack_pools[pool_index] == NULL) {
		 
		stack_pools[pool_index] = *prealloc;
		*prealloc = NULL;
	} else {
		 
		if (pool_index + 1 < DEPOT_MAX_POOLS) {
			stack_pools[pool_index + 1] = *prealloc;
			*prealloc = NULL;
		}
		 
		smp_store_release(&next_pool_required, 0);
	}
}

 
static struct stack_record *
depot_alloc_stack(unsigned long *entries, int size, u32 hash, void **prealloc)
{
	struct stack_record *stack;
	size_t required_size = struct_size(stack, entries, size);

	required_size = ALIGN(required_size, 1 << DEPOT_STACK_ALIGN);

	 
	if (unlikely(pool_offset + required_size > DEPOT_POOL_SIZE)) {
		 
		if (unlikely(pool_index + 1 >= DEPOT_MAX_POOLS)) {
			WARN_ONCE(1, "Stack depot reached limit capacity");
			return NULL;
		}

		 
		WRITE_ONCE(pool_index, pool_index + 1);
		pool_offset = 0;
		 
		if (pool_index + 1 < DEPOT_MAX_POOLS)
			smp_store_release(&next_pool_required, 1);
	}

	 
	if (*prealloc)
		depot_init_pool(prealloc);

	 
	if (stack_pools[pool_index] == NULL)
		return NULL;

	 
	stack = stack_pools[pool_index] + pool_offset;
	stack->hash = hash;
	stack->size = size;
	stack->handle.pool_index = pool_index;
	stack->handle.offset = pool_offset >> DEPOT_STACK_ALIGN;
	stack->handle.valid = 1;
	stack->handle.extra = 0;
	memcpy(stack->entries, entries, flex_array_size(stack, entries, size));
	pool_offset += required_size;
	 
	kmsan_unpoison_memory(stack, required_size);

	return stack;
}

 
static inline u32 hash_stack(unsigned long *entries, unsigned int size)
{
	return jhash2((u32 *)entries,
		      array_size(size,  sizeof(*entries)) / sizeof(u32),
		      STACK_HASH_SEED);
}

 
static inline
int stackdepot_memcmp(const unsigned long *u1, const unsigned long *u2,
			unsigned int n)
{
	for ( ; n-- ; u1++, u2++) {
		if (*u1 != *u2)
			return 1;
	}
	return 0;
}

 
static inline struct stack_record *find_stack(struct stack_record *bucket,
					     unsigned long *entries, int size,
					     u32 hash)
{
	struct stack_record *found;

	for (found = bucket; found; found = found->next) {
		if (found->hash == hash &&
		    found->size == size &&
		    !stackdepot_memcmp(entries, found->entries, size))
			return found;
	}
	return NULL;
}

depot_stack_handle_t __stack_depot_save(unsigned long *entries,
					unsigned int nr_entries,
					gfp_t alloc_flags, bool can_alloc)
{
	struct stack_record *found = NULL, **bucket;
	union handle_parts retval = { .handle = 0 };
	struct page *page = NULL;
	void *prealloc = NULL;
	unsigned long flags;
	u32 hash;

	 
	nr_entries = filter_irq_stacks(entries, nr_entries);

	if (unlikely(nr_entries == 0) || stack_depot_disabled)
		goto fast_exit;

	hash = hash_stack(entries, nr_entries);
	bucket = &stack_table[hash & stack_hash_mask];

	 
	found = find_stack(smp_load_acquire(bucket), entries, nr_entries, hash);
	if (found)
		goto exit;

	 
	if (unlikely(can_alloc && smp_load_acquire(&next_pool_required))) {
		 
		alloc_flags &= ~GFP_ZONEMASK;
		alloc_flags &= (GFP_ATOMIC | GFP_KERNEL);
		alloc_flags |= __GFP_NOWARN;
		page = alloc_pages(alloc_flags, DEPOT_POOL_ORDER);
		if (page)
			prealloc = page_address(page);
	}

	raw_spin_lock_irqsave(&pool_lock, flags);

	found = find_stack(*bucket, entries, nr_entries, hash);
	if (!found) {
		struct stack_record *new =
			depot_alloc_stack(entries, nr_entries, hash, &prealloc);

		if (new) {
			new->next = *bucket;
			 
			smp_store_release(bucket, new);
			found = new;
		}
	} else if (prealloc) {
		 
		depot_init_pool(&prealloc);
	}

	raw_spin_unlock_irqrestore(&pool_lock, flags);
exit:
	if (prealloc) {
		 
		free_pages((unsigned long)prealloc, DEPOT_POOL_ORDER);
	}
	if (found)
		retval.handle = found->handle.handle;
fast_exit:
	return retval.handle;
}
EXPORT_SYMBOL_GPL(__stack_depot_save);

depot_stack_handle_t stack_depot_save(unsigned long *entries,
				      unsigned int nr_entries,
				      gfp_t alloc_flags)
{
	return __stack_depot_save(entries, nr_entries, alloc_flags, true);
}
EXPORT_SYMBOL_GPL(stack_depot_save);

unsigned int stack_depot_fetch(depot_stack_handle_t handle,
			       unsigned long **entries)
{
	union handle_parts parts = { .handle = handle };
	 
	int pool_index_cached = READ_ONCE(pool_index);
	void *pool;
	size_t offset = parts.offset << DEPOT_STACK_ALIGN;
	struct stack_record *stack;

	*entries = NULL;
	 
	kmsan_unpoison_memory(entries, sizeof(*entries));

	if (!handle)
		return 0;

	if (parts.pool_index > pool_index_cached) {
		WARN(1, "pool index %d out of bounds (%d) for stack id %08x\n",
			parts.pool_index, pool_index_cached, handle);
		return 0;
	}
	pool = stack_pools[parts.pool_index];
	if (!pool)
		return 0;
	stack = pool + offset;

	*entries = stack->entries;
	return stack->size;
}
EXPORT_SYMBOL_GPL(stack_depot_fetch);

void stack_depot_print(depot_stack_handle_t stack)
{
	unsigned long *entries;
	unsigned int nr_entries;

	nr_entries = stack_depot_fetch(stack, &entries);
	if (nr_entries > 0)
		stack_trace_print(entries, nr_entries, 0);
}
EXPORT_SYMBOL_GPL(stack_depot_print);

int stack_depot_snprint(depot_stack_handle_t handle, char *buf, size_t size,
		       int spaces)
{
	unsigned long *entries;
	unsigned int nr_entries;

	nr_entries = stack_depot_fetch(handle, &entries);
	return nr_entries ? stack_trace_snprint(buf, size, entries, nr_entries,
						spaces) : 0;
}
EXPORT_SYMBOL_GPL(stack_depot_snprint);

depot_stack_handle_t __must_check stack_depot_set_extra_bits(
			depot_stack_handle_t handle, unsigned int extra_bits)
{
	union handle_parts parts = { .handle = handle };

	 
	if (!handle)
		return 0;

	parts.extra = extra_bits;
	return parts.handle;
}
EXPORT_SYMBOL(stack_depot_set_extra_bits);

unsigned int stack_depot_get_extra_bits(depot_stack_handle_t handle)
{
	union handle_parts parts = { .handle = handle };

	return parts.extra;
}
EXPORT_SYMBOL(stack_depot_get_extra_bits);
