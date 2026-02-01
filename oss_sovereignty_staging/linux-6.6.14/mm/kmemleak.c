
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/sched/task_stack.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/kthread.h>
#include <linux/rbtree.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/cpumask.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>
#include <linux/stacktrace.h>
#include <linux/stackdepot.h>
#include <linux/cache.h>
#include <linux/percpu.h>
#include <linux/memblock.h>
#include <linux/pfn.h>
#include <linux/mmzone.h>
#include <linux/slab.h>
#include <linux/thread_info.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/nodemask.h>
#include <linux/mm.h>
#include <linux/workqueue.h>
#include <linux/crc32.h>

#include <asm/sections.h>
#include <asm/processor.h>
#include <linux/atomic.h>

#include <linux/kasan.h>
#include <linux/kfence.h>
#include <linux/kmemleak.h>
#include <linux/memory_hotplug.h>

 
#define MAX_TRACE		16	 
#define MSECS_MIN_AGE		5000	 
#define SECS_FIRST_SCAN		60	 
#define SECS_SCAN_WAIT		600	 
#define MAX_SCAN_SIZE		4096	 

#define BYTES_PER_POINTER	sizeof(void *)

 
#define gfp_kmemleak_mask(gfp)	(((gfp) & (GFP_KERNEL | GFP_ATOMIC | \
					   __GFP_NOLOCKDEP)) | \
				 __GFP_NORETRY | __GFP_NOMEMALLOC | \
				 __GFP_NOWARN)

 
struct kmemleak_scan_area {
	struct hlist_node node;
	unsigned long start;
	size_t size;
};

#define KMEMLEAK_GREY	0
#define KMEMLEAK_BLACK	-1

 
struct kmemleak_object {
	raw_spinlock_t lock;
	unsigned int flags;		 
	struct list_head object_list;
	struct list_head gray_list;
	struct rb_node rb_node;
	struct rcu_head rcu;		 
	 
	atomic_t use_count;
	unsigned int del_state;		 
	unsigned long pointer;
	size_t size;
	 
	unsigned long excess_ref;
	 
	int min_count;
	 
	int count;
	 
	u32 checksum;
	 
	struct hlist_head area_list;
	depot_stack_handle_t trace_handle;
	unsigned long jiffies;		 
	pid_t pid;			 
	char comm[TASK_COMM_LEN];	 
};

 
#define OBJECT_ALLOCATED	(1 << 0)
 
#define OBJECT_REPORTED		(1 << 1)
 
#define OBJECT_NO_SCAN		(1 << 2)
 
#define OBJECT_FULL_SCAN	(1 << 3)
 
#define OBJECT_PHYS		(1 << 4)

 
#define DELSTATE_REMOVED	(1 << 0)
 
#define DELSTATE_NO_DELETE	(1 << 1)

#define HEX_PREFIX		"    "
 
#define HEX_ROW_SIZE		16
 
#define HEX_GROUP_SIZE		1
 
#define HEX_ASCII		1
 
#define HEX_MAX_LINES		2

 
static LIST_HEAD(object_list);
 
static LIST_HEAD(gray_list);
 
static struct kmemleak_object mem_pool[CONFIG_DEBUG_KMEMLEAK_MEM_POOL_SIZE];
static int mem_pool_free_count = ARRAY_SIZE(mem_pool);
static LIST_HEAD(mem_pool_free_list);
 
static struct rb_root object_tree_root = RB_ROOT;
 
static struct rb_root object_phys_tree_root = RB_ROOT;
 
static DEFINE_RAW_SPINLOCK(kmemleak_lock);

 
static struct kmem_cache *object_cache;
static struct kmem_cache *scan_area_cache;

 
static int kmemleak_enabled = 1;
 
static int kmemleak_free_enabled = 1;
 
static int kmemleak_late_initialized;
 
static int kmemleak_warning;
 
static int kmemleak_error;

 
static unsigned long min_addr = ULONG_MAX;
static unsigned long max_addr;

static struct task_struct *scan_thread;
 
static unsigned long jiffies_min_age;
static unsigned long jiffies_last_scan;
 
static unsigned long jiffies_scan_wait;
 
static int kmemleak_stack_scan = 1;
 
static DEFINE_MUTEX(scan_mutex);
 
static int kmemleak_skip_disable;
 
static bool kmemleak_found_leaks;

static bool kmemleak_verbose;
module_param_named(verbose, kmemleak_verbose, bool, 0600);

static void kmemleak_disable(void);

 
#define kmemleak_warn(x...)	do {		\
	pr_warn(x);				\
	dump_stack();				\
	kmemleak_warning = 1;			\
} while (0)

 
#define kmemleak_stop(x...)	do {	\
	kmemleak_warn(x);		\
	kmemleak_disable();		\
} while (0)

#define warn_or_seq_printf(seq, fmt, ...)	do {	\
	if (seq)					\
		seq_printf(seq, fmt, ##__VA_ARGS__);	\
	else						\
		pr_warn(fmt, ##__VA_ARGS__);		\
} while (0)

static void warn_or_seq_hex_dump(struct seq_file *seq, int prefix_type,
				 int rowsize, int groupsize, const void *buf,
				 size_t len, bool ascii)
{
	if (seq)
		seq_hex_dump(seq, HEX_PREFIX, prefix_type, rowsize, groupsize,
			     buf, len, ascii);
	else
		print_hex_dump(KERN_WARNING, pr_fmt(HEX_PREFIX), prefix_type,
			       rowsize, groupsize, buf, len, ascii);
}

 
static void hex_dump_object(struct seq_file *seq,
			    struct kmemleak_object *object)
{
	const u8 *ptr = (const u8 *)object->pointer;
	size_t len;

	if (WARN_ON_ONCE(object->flags & OBJECT_PHYS))
		return;

	 
	len = min_t(size_t, object->size, HEX_MAX_LINES * HEX_ROW_SIZE);

	warn_or_seq_printf(seq, "  hex dump (first %zu bytes):\n", len);
	kasan_disable_current();
	warn_or_seq_hex_dump(seq, DUMP_PREFIX_NONE, HEX_ROW_SIZE,
			     HEX_GROUP_SIZE, kasan_reset_tag((void *)ptr), len, HEX_ASCII);
	kasan_enable_current();
}

 
static bool color_white(const struct kmemleak_object *object)
{
	return object->count != KMEMLEAK_BLACK &&
		object->count < object->min_count;
}

static bool color_gray(const struct kmemleak_object *object)
{
	return object->min_count != KMEMLEAK_BLACK &&
		object->count >= object->min_count;
}

 
static bool unreferenced_object(struct kmemleak_object *object)
{
	return (color_white(object) && object->flags & OBJECT_ALLOCATED) &&
		time_before_eq(object->jiffies + jiffies_min_age,
			       jiffies_last_scan);
}

 
static void print_unreferenced(struct seq_file *seq,
			       struct kmemleak_object *object)
{
	int i;
	unsigned long *entries;
	unsigned int nr_entries;
	unsigned int msecs_age = jiffies_to_msecs(jiffies - object->jiffies);

	nr_entries = stack_depot_fetch(object->trace_handle, &entries);
	warn_or_seq_printf(seq, "unreferenced object 0x%08lx (size %zu):\n",
			  object->pointer, object->size);
	warn_or_seq_printf(seq, "  comm \"%s\", pid %d, jiffies %lu (age %d.%03ds)\n",
			   object->comm, object->pid, object->jiffies,
			   msecs_age / 1000, msecs_age % 1000);
	hex_dump_object(seq, object);
	warn_or_seq_printf(seq, "  backtrace:\n");

	for (i = 0; i < nr_entries; i++) {
		void *ptr = (void *)entries[i];
		warn_or_seq_printf(seq, "    [<%pK>] %pS\n", ptr, ptr);
	}
}

 
static void dump_object_info(struct kmemleak_object *object)
{
	pr_notice("Object 0x%08lx (size %zu):\n",
			object->pointer, object->size);
	pr_notice("  comm \"%s\", pid %d, jiffies %lu\n",
			object->comm, object->pid, object->jiffies);
	pr_notice("  min_count = %d\n", object->min_count);
	pr_notice("  count = %d\n", object->count);
	pr_notice("  flags = 0x%x\n", object->flags);
	pr_notice("  checksum = %u\n", object->checksum);
	pr_notice("  backtrace:\n");
	if (object->trace_handle)
		stack_depot_print(object->trace_handle);
}

 
static struct kmemleak_object *__lookup_object(unsigned long ptr, int alias,
					       bool is_phys)
{
	struct rb_node *rb = is_phys ? object_phys_tree_root.rb_node :
			     object_tree_root.rb_node;
	unsigned long untagged_ptr = (unsigned long)kasan_reset_tag((void *)ptr);

	while (rb) {
		struct kmemleak_object *object;
		unsigned long untagged_objp;

		object = rb_entry(rb, struct kmemleak_object, rb_node);
		untagged_objp = (unsigned long)kasan_reset_tag((void *)object->pointer);

		if (untagged_ptr < untagged_objp)
			rb = object->rb_node.rb_left;
		else if (untagged_objp + object->size <= untagged_ptr)
			rb = object->rb_node.rb_right;
		else if (untagged_objp == untagged_ptr || alias)
			return object;
		else {
			kmemleak_warn("Found object by alias at 0x%08lx\n",
				      ptr);
			dump_object_info(object);
			break;
		}
	}
	return NULL;
}

 
static struct kmemleak_object *lookup_object(unsigned long ptr, int alias)
{
	return __lookup_object(ptr, alias, false);
}

 
static int get_object(struct kmemleak_object *object)
{
	return atomic_inc_not_zero(&object->use_count);
}

 
static struct kmemleak_object *mem_pool_alloc(gfp_t gfp)
{
	unsigned long flags;
	struct kmemleak_object *object;

	 
	if (object_cache) {
		object = kmem_cache_alloc(object_cache, gfp_kmemleak_mask(gfp));
		if (object)
			return object;
	}

	 
	raw_spin_lock_irqsave(&kmemleak_lock, flags);
	object = list_first_entry_or_null(&mem_pool_free_list,
					  typeof(*object), object_list);
	if (object)
		list_del(&object->object_list);
	else if (mem_pool_free_count)
		object = &mem_pool[--mem_pool_free_count];
	else
		pr_warn_once("Memory pool empty, consider increasing CONFIG_DEBUG_KMEMLEAK_MEM_POOL_SIZE\n");
	raw_spin_unlock_irqrestore(&kmemleak_lock, flags);

	return object;
}

 
static void mem_pool_free(struct kmemleak_object *object)
{
	unsigned long flags;

	if (object < mem_pool || object >= mem_pool + ARRAY_SIZE(mem_pool)) {
		kmem_cache_free(object_cache, object);
		return;
	}

	 
	raw_spin_lock_irqsave(&kmemleak_lock, flags);
	list_add(&object->object_list, &mem_pool_free_list);
	raw_spin_unlock_irqrestore(&kmemleak_lock, flags);
}

 
static void free_object_rcu(struct rcu_head *rcu)
{
	struct hlist_node *tmp;
	struct kmemleak_scan_area *area;
	struct kmemleak_object *object =
		container_of(rcu, struct kmemleak_object, rcu);

	 
	hlist_for_each_entry_safe(area, tmp, &object->area_list, node) {
		hlist_del(&area->node);
		kmem_cache_free(scan_area_cache, area);
	}
	mem_pool_free(object);
}

 
static void put_object(struct kmemleak_object *object)
{
	if (!atomic_dec_and_test(&object->use_count))
		return;

	 
	WARN_ON(object->flags & OBJECT_ALLOCATED);

	 
	if (object_cache)
		call_rcu(&object->rcu, free_object_rcu);
	else
		free_object_rcu(&object->rcu);
}

 
static struct kmemleak_object *__find_and_get_object(unsigned long ptr, int alias,
						     bool is_phys)
{
	unsigned long flags;
	struct kmemleak_object *object;

	rcu_read_lock();
	raw_spin_lock_irqsave(&kmemleak_lock, flags);
	object = __lookup_object(ptr, alias, is_phys);
	raw_spin_unlock_irqrestore(&kmemleak_lock, flags);

	 
	if (object && !get_object(object))
		object = NULL;
	rcu_read_unlock();

	return object;
}

 
static struct kmemleak_object *find_and_get_object(unsigned long ptr, int alias)
{
	return __find_and_get_object(ptr, alias, false);
}

 
static void __remove_object(struct kmemleak_object *object)
{
	rb_erase(&object->rb_node, object->flags & OBJECT_PHYS ?
				   &object_phys_tree_root :
				   &object_tree_root);
	if (!(object->del_state & DELSTATE_NO_DELETE))
		list_del_rcu(&object->object_list);
	object->del_state |= DELSTATE_REMOVED;
}

 
static struct kmemleak_object *find_and_remove_object(unsigned long ptr, int alias,
						      bool is_phys)
{
	unsigned long flags;
	struct kmemleak_object *object;

	raw_spin_lock_irqsave(&kmemleak_lock, flags);
	object = __lookup_object(ptr, alias, is_phys);
	if (object)
		__remove_object(object);
	raw_spin_unlock_irqrestore(&kmemleak_lock, flags);

	return object;
}

static noinline depot_stack_handle_t set_track_prepare(void)
{
	depot_stack_handle_t trace_handle;
	unsigned long entries[MAX_TRACE];
	unsigned int nr_entries;

	 
	if (!object_cache)
		return 0;
	nr_entries = stack_trace_save(entries, ARRAY_SIZE(entries), 3);
	trace_handle = stack_depot_save(entries, nr_entries, GFP_NOWAIT);

	return trace_handle;
}

 
static void __create_object(unsigned long ptr, size_t size,
			    int min_count, gfp_t gfp, bool is_phys)
{
	unsigned long flags;
	struct kmemleak_object *object, *parent;
	struct rb_node **link, *rb_parent;
	unsigned long untagged_ptr;
	unsigned long untagged_objp;

	object = mem_pool_alloc(gfp);
	if (!object) {
		pr_warn("Cannot allocate a kmemleak_object structure\n");
		kmemleak_disable();
		return;
	}

	INIT_LIST_HEAD(&object->object_list);
	INIT_LIST_HEAD(&object->gray_list);
	INIT_HLIST_HEAD(&object->area_list);
	raw_spin_lock_init(&object->lock);
	atomic_set(&object->use_count, 1);
	object->flags = OBJECT_ALLOCATED | (is_phys ? OBJECT_PHYS : 0);
	object->pointer = ptr;
	object->size = kfence_ksize((void *)ptr) ?: size;
	object->excess_ref = 0;
	object->min_count = min_count;
	object->count = 0;			 
	object->jiffies = jiffies;
	object->checksum = 0;
	object->del_state = 0;

	 
	if (in_hardirq()) {
		object->pid = 0;
		strncpy(object->comm, "hardirq", sizeof(object->comm));
	} else if (in_serving_softirq()) {
		object->pid = 0;
		strncpy(object->comm, "softirq", sizeof(object->comm));
	} else {
		object->pid = current->pid;
		 
		strncpy(object->comm, current->comm, sizeof(object->comm));
	}

	 
	object->trace_handle = set_track_prepare();

	raw_spin_lock_irqsave(&kmemleak_lock, flags);

	untagged_ptr = (unsigned long)kasan_reset_tag((void *)ptr);
	 
	if (!is_phys) {
		min_addr = min(min_addr, untagged_ptr);
		max_addr = max(max_addr, untagged_ptr + size);
	}
	link = is_phys ? &object_phys_tree_root.rb_node :
		&object_tree_root.rb_node;
	rb_parent = NULL;
	while (*link) {
		rb_parent = *link;
		parent = rb_entry(rb_parent, struct kmemleak_object, rb_node);
		untagged_objp = (unsigned long)kasan_reset_tag((void *)parent->pointer);
		if (untagged_ptr + size <= untagged_objp)
			link = &parent->rb_node.rb_left;
		else if (untagged_objp + parent->size <= untagged_ptr)
			link = &parent->rb_node.rb_right;
		else {
			kmemleak_stop("Cannot insert 0x%lx into the object search tree (overlaps existing)\n",
				      ptr);
			 
			dump_object_info(parent);
			kmem_cache_free(object_cache, object);
			goto out;
		}
	}
	rb_link_node(&object->rb_node, rb_parent, link);
	rb_insert_color(&object->rb_node, is_phys ? &object_phys_tree_root :
					  &object_tree_root);
	list_add_tail_rcu(&object->object_list, &object_list);
out:
	raw_spin_unlock_irqrestore(&kmemleak_lock, flags);
}

 
static void create_object(unsigned long ptr, size_t size,
			  int min_count, gfp_t gfp)
{
	__create_object(ptr, size, min_count, gfp, false);
}

 
static void create_object_phys(unsigned long ptr, size_t size,
			       int min_count, gfp_t gfp)
{
	__create_object(ptr, size, min_count, gfp, true);
}

 
static void __delete_object(struct kmemleak_object *object)
{
	unsigned long flags;

	WARN_ON(!(object->flags & OBJECT_ALLOCATED));
	WARN_ON(atomic_read(&object->use_count) < 1);

	 
	raw_spin_lock_irqsave(&object->lock, flags);
	object->flags &= ~OBJECT_ALLOCATED;
	raw_spin_unlock_irqrestore(&object->lock, flags);
	put_object(object);
}

 
static void delete_object_full(unsigned long ptr)
{
	struct kmemleak_object *object;

	object = find_and_remove_object(ptr, 0, false);
	if (!object) {
#ifdef DEBUG
		kmemleak_warn("Freeing unknown object at 0x%08lx\n",
			      ptr);
#endif
		return;
	}
	__delete_object(object);
}

 
static void delete_object_part(unsigned long ptr, size_t size, bool is_phys)
{
	struct kmemleak_object *object;
	unsigned long start, end;

	object = find_and_remove_object(ptr, 1, is_phys);
	if (!object) {
#ifdef DEBUG
		kmemleak_warn("Partially freeing unknown object at 0x%08lx (size %zu)\n",
			      ptr, size);
#endif
		return;
	}

	 
	start = object->pointer;
	end = object->pointer + object->size;
	if (ptr > start)
		__create_object(start, ptr - start, object->min_count,
			      GFP_KERNEL, is_phys);
	if (ptr + size < end)
		__create_object(ptr + size, end - ptr - size, object->min_count,
			      GFP_KERNEL, is_phys);

	__delete_object(object);
}

static void __paint_it(struct kmemleak_object *object, int color)
{
	object->min_count = color;
	if (color == KMEMLEAK_BLACK)
		object->flags |= OBJECT_NO_SCAN;
}

static void paint_it(struct kmemleak_object *object, int color)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&object->lock, flags);
	__paint_it(object, color);
	raw_spin_unlock_irqrestore(&object->lock, flags);
}

static void paint_ptr(unsigned long ptr, int color, bool is_phys)
{
	struct kmemleak_object *object;

	object = __find_and_get_object(ptr, 0, is_phys);
	if (!object) {
		kmemleak_warn("Trying to color unknown object at 0x%08lx as %s\n",
			      ptr,
			      (color == KMEMLEAK_GREY) ? "Grey" :
			      (color == KMEMLEAK_BLACK) ? "Black" : "Unknown");
		return;
	}
	paint_it(object, color);
	put_object(object);
}

 
static void make_gray_object(unsigned long ptr)
{
	paint_ptr(ptr, KMEMLEAK_GREY, false);
}

 
static void make_black_object(unsigned long ptr, bool is_phys)
{
	paint_ptr(ptr, KMEMLEAK_BLACK, is_phys);
}

 
static void add_scan_area(unsigned long ptr, size_t size, gfp_t gfp)
{
	unsigned long flags;
	struct kmemleak_object *object;
	struct kmemleak_scan_area *area = NULL;
	unsigned long untagged_ptr;
	unsigned long untagged_objp;

	object = find_and_get_object(ptr, 1);
	if (!object) {
		kmemleak_warn("Adding scan area to unknown object at 0x%08lx\n",
			      ptr);
		return;
	}

	untagged_ptr = (unsigned long)kasan_reset_tag((void *)ptr);
	untagged_objp = (unsigned long)kasan_reset_tag((void *)object->pointer);

	if (scan_area_cache)
		area = kmem_cache_alloc(scan_area_cache, gfp_kmemleak_mask(gfp));

	raw_spin_lock_irqsave(&object->lock, flags);
	if (!area) {
		pr_warn_once("Cannot allocate a scan area, scanning the full object\n");
		 
		object->flags |= OBJECT_FULL_SCAN;
		goto out_unlock;
	}
	if (size == SIZE_MAX) {
		size = untagged_objp + object->size - untagged_ptr;
	} else if (untagged_ptr + size > untagged_objp + object->size) {
		kmemleak_warn("Scan area larger than object 0x%08lx\n", ptr);
		dump_object_info(object);
		kmem_cache_free(scan_area_cache, area);
		goto out_unlock;
	}

	INIT_HLIST_NODE(&area->node);
	area->start = ptr;
	area->size = size;

	hlist_add_head(&area->node, &object->area_list);
out_unlock:
	raw_spin_unlock_irqrestore(&object->lock, flags);
	put_object(object);
}

 
static void object_set_excess_ref(unsigned long ptr, unsigned long excess_ref)
{
	unsigned long flags;
	struct kmemleak_object *object;

	object = find_and_get_object(ptr, 0);
	if (!object) {
		kmemleak_warn("Setting excess_ref on unknown object at 0x%08lx\n",
			      ptr);
		return;
	}

	raw_spin_lock_irqsave(&object->lock, flags);
	object->excess_ref = excess_ref;
	raw_spin_unlock_irqrestore(&object->lock, flags);
	put_object(object);
}

 
static void object_no_scan(unsigned long ptr)
{
	unsigned long flags;
	struct kmemleak_object *object;

	object = find_and_get_object(ptr, 0);
	if (!object) {
		kmemleak_warn("Not scanning unknown object at 0x%08lx\n", ptr);
		return;
	}

	raw_spin_lock_irqsave(&object->lock, flags);
	object->flags |= OBJECT_NO_SCAN;
	raw_spin_unlock_irqrestore(&object->lock, flags);
	put_object(object);
}

 
void __ref kmemleak_alloc(const void *ptr, size_t size, int min_count,
			  gfp_t gfp)
{
	pr_debug("%s(0x%p, %zu, %d)\n", __func__, ptr, size, min_count);

	if (kmemleak_enabled && ptr && !IS_ERR(ptr))
		create_object((unsigned long)ptr, size, min_count, gfp);
}
EXPORT_SYMBOL_GPL(kmemleak_alloc);

 
void __ref kmemleak_alloc_percpu(const void __percpu *ptr, size_t size,
				 gfp_t gfp)
{
	unsigned int cpu;

	pr_debug("%s(0x%p, %zu)\n", __func__, ptr, size);

	 
	if (kmemleak_enabled && ptr && !IS_ERR(ptr))
		for_each_possible_cpu(cpu)
			create_object((unsigned long)per_cpu_ptr(ptr, cpu),
				      size, 0, gfp);
}
EXPORT_SYMBOL_GPL(kmemleak_alloc_percpu);

 
void __ref kmemleak_vmalloc(const struct vm_struct *area, size_t size, gfp_t gfp)
{
	pr_debug("%s(0x%p, %zu)\n", __func__, area, size);

	 
	if (kmemleak_enabled) {
		create_object((unsigned long)area->addr, size, 2, gfp);
		object_set_excess_ref((unsigned long)area,
				      (unsigned long)area->addr);
	}
}
EXPORT_SYMBOL_GPL(kmemleak_vmalloc);

 
void __ref kmemleak_free(const void *ptr)
{
	pr_debug("%s(0x%p)\n", __func__, ptr);

	if (kmemleak_free_enabled && ptr && !IS_ERR(ptr))
		delete_object_full((unsigned long)ptr);
}
EXPORT_SYMBOL_GPL(kmemleak_free);

 
void __ref kmemleak_free_part(const void *ptr, size_t size)
{
	pr_debug("%s(0x%p)\n", __func__, ptr);

	if (kmemleak_enabled && ptr && !IS_ERR(ptr))
		delete_object_part((unsigned long)ptr, size, false);
}
EXPORT_SYMBOL_GPL(kmemleak_free_part);

 
void __ref kmemleak_free_percpu(const void __percpu *ptr)
{
	unsigned int cpu;

	pr_debug("%s(0x%p)\n", __func__, ptr);

	if (kmemleak_free_enabled && ptr && !IS_ERR(ptr))
		for_each_possible_cpu(cpu)
			delete_object_full((unsigned long)per_cpu_ptr(ptr,
								      cpu));
}
EXPORT_SYMBOL_GPL(kmemleak_free_percpu);

 
void __ref kmemleak_update_trace(const void *ptr)
{
	struct kmemleak_object *object;
	unsigned long flags;

	pr_debug("%s(0x%p)\n", __func__, ptr);

	if (!kmemleak_enabled || IS_ERR_OR_NULL(ptr))
		return;

	object = find_and_get_object((unsigned long)ptr, 1);
	if (!object) {
#ifdef DEBUG
		kmemleak_warn("Updating stack trace for unknown object at %p\n",
			      ptr);
#endif
		return;
	}

	raw_spin_lock_irqsave(&object->lock, flags);
	object->trace_handle = set_track_prepare();
	raw_spin_unlock_irqrestore(&object->lock, flags);

	put_object(object);
}
EXPORT_SYMBOL(kmemleak_update_trace);

 
void __ref kmemleak_not_leak(const void *ptr)
{
	pr_debug("%s(0x%p)\n", __func__, ptr);

	if (kmemleak_enabled && ptr && !IS_ERR(ptr))
		make_gray_object((unsigned long)ptr);
}
EXPORT_SYMBOL(kmemleak_not_leak);

 
void __ref kmemleak_ignore(const void *ptr)
{
	pr_debug("%s(0x%p)\n", __func__, ptr);

	if (kmemleak_enabled && ptr && !IS_ERR(ptr))
		make_black_object((unsigned long)ptr, false);
}
EXPORT_SYMBOL(kmemleak_ignore);

 
void __ref kmemleak_scan_area(const void *ptr, size_t size, gfp_t gfp)
{
	pr_debug("%s(0x%p)\n", __func__, ptr);

	if (kmemleak_enabled && ptr && size && !IS_ERR(ptr))
		add_scan_area((unsigned long)ptr, size, gfp);
}
EXPORT_SYMBOL(kmemleak_scan_area);

 
void __ref kmemleak_no_scan(const void *ptr)
{
	pr_debug("%s(0x%p)\n", __func__, ptr);

	if (kmemleak_enabled && ptr && !IS_ERR(ptr))
		object_no_scan((unsigned long)ptr);
}
EXPORT_SYMBOL(kmemleak_no_scan);

 
void __ref kmemleak_alloc_phys(phys_addr_t phys, size_t size, gfp_t gfp)
{
	pr_debug("%s(0x%pa, %zu)\n", __func__, &phys, size);

	if (kmemleak_enabled)
		 
		create_object_phys((unsigned long)phys, size, 0, gfp);
}
EXPORT_SYMBOL(kmemleak_alloc_phys);

 
void __ref kmemleak_free_part_phys(phys_addr_t phys, size_t size)
{
	pr_debug("%s(0x%pa)\n", __func__, &phys);

	if (kmemleak_enabled)
		delete_object_part((unsigned long)phys, size, true);
}
EXPORT_SYMBOL(kmemleak_free_part_phys);

 
void __ref kmemleak_ignore_phys(phys_addr_t phys)
{
	pr_debug("%s(0x%pa)\n", __func__, &phys);

	if (kmemleak_enabled)
		make_black_object((unsigned long)phys, true);
}
EXPORT_SYMBOL(kmemleak_ignore_phys);

 
static bool update_checksum(struct kmemleak_object *object)
{
	u32 old_csum = object->checksum;

	if (WARN_ON_ONCE(object->flags & OBJECT_PHYS))
		return false;

	kasan_disable_current();
	kcsan_disable_current();
	object->checksum = crc32(0, kasan_reset_tag((void *)object->pointer), object->size);
	kasan_enable_current();
	kcsan_enable_current();

	return object->checksum != old_csum;
}

 
static void update_refs(struct kmemleak_object *object)
{
	if (!color_white(object)) {
		 
		return;
	}

	 
	object->count++;
	if (color_gray(object)) {
		 
		WARN_ON(!get_object(object));
		list_add_tail(&object->gray_list, &gray_list);
	}
}

 
static int scan_should_stop(void)
{
	if (!kmemleak_enabled)
		return 1;

	 
	if (current->mm)
		return signal_pending(current);
	else
		return kthread_should_stop();

	return 0;
}

 
static void scan_block(void *_start, void *_end,
		       struct kmemleak_object *scanned)
{
	unsigned long *ptr;
	unsigned long *start = PTR_ALIGN(_start, BYTES_PER_POINTER);
	unsigned long *end = _end - (BYTES_PER_POINTER - 1);
	unsigned long flags;
	unsigned long untagged_ptr;

	raw_spin_lock_irqsave(&kmemleak_lock, flags);
	for (ptr = start; ptr < end; ptr++) {
		struct kmemleak_object *object;
		unsigned long pointer;
		unsigned long excess_ref;

		if (scan_should_stop())
			break;

		kasan_disable_current();
		pointer = *(unsigned long *)kasan_reset_tag((void *)ptr);
		kasan_enable_current();

		untagged_ptr = (unsigned long)kasan_reset_tag((void *)pointer);
		if (untagged_ptr < min_addr || untagged_ptr >= max_addr)
			continue;

		 
		object = lookup_object(pointer, 1);
		if (!object)
			continue;
		if (object == scanned)
			 
			continue;

		 
		raw_spin_lock_nested(&object->lock, SINGLE_DEPTH_NESTING);
		 
		if (color_gray(object)) {
			excess_ref = object->excess_ref;
			 
		} else {
			excess_ref = 0;
			update_refs(object);
		}
		raw_spin_unlock(&object->lock);

		if (excess_ref) {
			object = lookup_object(excess_ref, 0);
			if (!object)
				continue;
			if (object == scanned)
				 
				continue;
			raw_spin_lock_nested(&object->lock, SINGLE_DEPTH_NESTING);
			update_refs(object);
			raw_spin_unlock(&object->lock);
		}
	}
	raw_spin_unlock_irqrestore(&kmemleak_lock, flags);
}

 
#ifdef CONFIG_SMP
static void scan_large_block(void *start, void *end)
{
	void *next;

	while (start < end) {
		next = min(start + MAX_SCAN_SIZE, end);
		scan_block(start, next, NULL);
		start = next;
		cond_resched();
	}
}
#endif

 
static void scan_object(struct kmemleak_object *object)
{
	struct kmemleak_scan_area *area;
	unsigned long flags;
	void *obj_ptr;

	 
	raw_spin_lock_irqsave(&object->lock, flags);
	if (object->flags & OBJECT_NO_SCAN)
		goto out;
	if (!(object->flags & OBJECT_ALLOCATED))
		 
		goto out;

	obj_ptr = object->flags & OBJECT_PHYS ?
		  __va((phys_addr_t)object->pointer) :
		  (void *)object->pointer;

	if (hlist_empty(&object->area_list) ||
	    object->flags & OBJECT_FULL_SCAN) {
		void *start = obj_ptr;
		void *end = obj_ptr + object->size;
		void *next;

		do {
			next = min(start + MAX_SCAN_SIZE, end);
			scan_block(start, next, object);

			start = next;
			if (start >= end)
				break;

			raw_spin_unlock_irqrestore(&object->lock, flags);
			cond_resched();
			raw_spin_lock_irqsave(&object->lock, flags);
		} while (object->flags & OBJECT_ALLOCATED);
	} else
		hlist_for_each_entry(area, &object->area_list, node)
			scan_block((void *)area->start,
				   (void *)(area->start + area->size),
				   object);
out:
	raw_spin_unlock_irqrestore(&object->lock, flags);
}

 
static void scan_gray_list(void)
{
	struct kmemleak_object *object, *tmp;

	 
	object = list_entry(gray_list.next, typeof(*object), gray_list);
	while (&object->gray_list != &gray_list) {
		cond_resched();

		 
		if (!scan_should_stop())
			scan_object(object);

		tmp = list_entry(object->gray_list.next, typeof(*object),
				 gray_list);

		 
		list_del(&object->gray_list);
		put_object(object);

		object = tmp;
	}
	WARN_ON(!list_empty(&gray_list));
}

 
static void kmemleak_cond_resched(struct kmemleak_object *object)
{
	if (!get_object(object))
		return;	 

	raw_spin_lock_irq(&kmemleak_lock);
	if (object->del_state & DELSTATE_REMOVED)
		goto unlock_put;	 
	object->del_state |= DELSTATE_NO_DELETE;
	raw_spin_unlock_irq(&kmemleak_lock);

	rcu_read_unlock();
	cond_resched();
	rcu_read_lock();

	raw_spin_lock_irq(&kmemleak_lock);
	if (object->del_state & DELSTATE_REMOVED)
		list_del_rcu(&object->object_list);
	object->del_state &= ~DELSTATE_NO_DELETE;
unlock_put:
	raw_spin_unlock_irq(&kmemleak_lock);
	put_object(object);
}

 
static void kmemleak_scan(void)
{
	struct kmemleak_object *object;
	struct zone *zone;
	int __maybe_unused i;
	int new_leaks = 0;

	jiffies_last_scan = jiffies;

	 
	rcu_read_lock();
	list_for_each_entry_rcu(object, &object_list, object_list) {
		raw_spin_lock_irq(&object->lock);
#ifdef DEBUG
		 
		if (atomic_read(&object->use_count) > 1) {
			pr_debug("object->use_count = %d\n",
				 atomic_read(&object->use_count));
			dump_object_info(object);
		}
#endif

		 
		if ((object->flags & OBJECT_PHYS) &&
		   !(object->flags & OBJECT_NO_SCAN)) {
			unsigned long phys = object->pointer;

			if (PHYS_PFN(phys) < min_low_pfn ||
			    PHYS_PFN(phys + object->size) >= max_low_pfn)
				__paint_it(object, KMEMLEAK_BLACK);
		}

		 
		object->count = 0;
		if (color_gray(object) && get_object(object))
			list_add_tail(&object->gray_list, &gray_list);

		raw_spin_unlock_irq(&object->lock);

		if (need_resched())
			kmemleak_cond_resched(object);
	}
	rcu_read_unlock();

#ifdef CONFIG_SMP
	 
	for_each_possible_cpu(i)
		scan_large_block(__per_cpu_start + per_cpu_offset(i),
				 __per_cpu_end + per_cpu_offset(i));
#endif

	 
	get_online_mems();
	for_each_populated_zone(zone) {
		unsigned long start_pfn = zone->zone_start_pfn;
		unsigned long end_pfn = zone_end_pfn(zone);
		unsigned long pfn;

		for (pfn = start_pfn; pfn < end_pfn; pfn++) {
			struct page *page = pfn_to_online_page(pfn);

			if (!(pfn & 63))
				cond_resched();

			if (!page)
				continue;

			 
			if (page_zone(page) != zone)
				continue;
			 
			if (page_count(page) == 0)
				continue;
			scan_block(page, page + 1, NULL);
		}
	}
	put_online_mems();

	 
	if (kmemleak_stack_scan) {
		struct task_struct *p, *g;

		rcu_read_lock();
		for_each_process_thread(g, p) {
			void *stack = try_get_task_stack(p);
			if (stack) {
				scan_block(stack, stack + THREAD_SIZE, NULL);
				put_task_stack(p);
			}
		}
		rcu_read_unlock();
	}

	 
	scan_gray_list();

	 
	rcu_read_lock();
	list_for_each_entry_rcu(object, &object_list, object_list) {
		if (need_resched())
			kmemleak_cond_resched(object);

		 
		if (!color_white(object))
			continue;
		raw_spin_lock_irq(&object->lock);
		if (color_white(object) && (object->flags & OBJECT_ALLOCATED)
		    && update_checksum(object) && get_object(object)) {
			 
			object->count = object->min_count;
			list_add_tail(&object->gray_list, &gray_list);
		}
		raw_spin_unlock_irq(&object->lock);
	}
	rcu_read_unlock();

	 
	scan_gray_list();

	 
	if (scan_should_stop())
		return;

	 
	rcu_read_lock();
	list_for_each_entry_rcu(object, &object_list, object_list) {
		if (need_resched())
			kmemleak_cond_resched(object);

		 
		if (!color_white(object))
			continue;
		raw_spin_lock_irq(&object->lock);
		if (unreferenced_object(object) &&
		    !(object->flags & OBJECT_REPORTED)) {
			object->flags |= OBJECT_REPORTED;

			if (kmemleak_verbose)
				print_unreferenced(NULL, object);

			new_leaks++;
		}
		raw_spin_unlock_irq(&object->lock);
	}
	rcu_read_unlock();

	if (new_leaks) {
		kmemleak_found_leaks = true;

		pr_info("%d new suspected memory leaks (see /sys/kernel/debug/kmemleak)\n",
			new_leaks);
	}

}

 
static int kmemleak_scan_thread(void *arg)
{
	static int first_run = IS_ENABLED(CONFIG_DEBUG_KMEMLEAK_AUTO_SCAN);

	pr_info("Automatic memory scanning thread started\n");
	set_user_nice(current, 10);

	 
	if (first_run) {
		signed long timeout = msecs_to_jiffies(SECS_FIRST_SCAN * 1000);
		first_run = 0;
		while (timeout && !kthread_should_stop())
			timeout = schedule_timeout_interruptible(timeout);
	}

	while (!kthread_should_stop()) {
		signed long timeout = READ_ONCE(jiffies_scan_wait);

		mutex_lock(&scan_mutex);
		kmemleak_scan();
		mutex_unlock(&scan_mutex);

		 
		while (timeout && !kthread_should_stop())
			timeout = schedule_timeout_interruptible(timeout);
	}

	pr_info("Automatic memory scanning thread ended\n");

	return 0;
}

 
static void start_scan_thread(void)
{
	if (scan_thread)
		return;
	scan_thread = kthread_run(kmemleak_scan_thread, NULL, "kmemleak");
	if (IS_ERR(scan_thread)) {
		pr_warn("Failed to create the scan thread\n");
		scan_thread = NULL;
	}
}

 
static void stop_scan_thread(void)
{
	if (scan_thread) {
		kthread_stop(scan_thread);
		scan_thread = NULL;
	}
}

 
static void *kmemleak_seq_start(struct seq_file *seq, loff_t *pos)
{
	struct kmemleak_object *object;
	loff_t n = *pos;
	int err;

	err = mutex_lock_interruptible(&scan_mutex);
	if (err < 0)
		return ERR_PTR(err);

	rcu_read_lock();
	list_for_each_entry_rcu(object, &object_list, object_list) {
		if (n-- > 0)
			continue;
		if (get_object(object))
			goto out;
	}
	object = NULL;
out:
	return object;
}

 
static void *kmemleak_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct kmemleak_object *prev_obj = v;
	struct kmemleak_object *next_obj = NULL;
	struct kmemleak_object *obj = prev_obj;

	++(*pos);

	list_for_each_entry_continue_rcu(obj, &object_list, object_list) {
		if (get_object(obj)) {
			next_obj = obj;
			break;
		}
	}

	put_object(prev_obj);
	return next_obj;
}

 
static void kmemleak_seq_stop(struct seq_file *seq, void *v)
{
	if (!IS_ERR(v)) {
		 
		rcu_read_unlock();
		mutex_unlock(&scan_mutex);
		if (v)
			put_object(v);
	}
}

 
static int kmemleak_seq_show(struct seq_file *seq, void *v)
{
	struct kmemleak_object *object = v;
	unsigned long flags;

	raw_spin_lock_irqsave(&object->lock, flags);
	if ((object->flags & OBJECT_REPORTED) && unreferenced_object(object))
		print_unreferenced(seq, object);
	raw_spin_unlock_irqrestore(&object->lock, flags);
	return 0;
}

static const struct seq_operations kmemleak_seq_ops = {
	.start = kmemleak_seq_start,
	.next  = kmemleak_seq_next,
	.stop  = kmemleak_seq_stop,
	.show  = kmemleak_seq_show,
};

static int kmemleak_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &kmemleak_seq_ops);
}

static int dump_str_object_info(const char *str)
{
	unsigned long flags;
	struct kmemleak_object *object;
	unsigned long addr;

	if (kstrtoul(str, 0, &addr))
		return -EINVAL;
	object = find_and_get_object(addr, 0);
	if (!object) {
		pr_info("Unknown object at 0x%08lx\n", addr);
		return -EINVAL;
	}

	raw_spin_lock_irqsave(&object->lock, flags);
	dump_object_info(object);
	raw_spin_unlock_irqrestore(&object->lock, flags);

	put_object(object);
	return 0;
}

 
static void kmemleak_clear(void)
{
	struct kmemleak_object *object;

	rcu_read_lock();
	list_for_each_entry_rcu(object, &object_list, object_list) {
		raw_spin_lock_irq(&object->lock);
		if ((object->flags & OBJECT_REPORTED) &&
		    unreferenced_object(object))
			__paint_it(object, KMEMLEAK_GREY);
		raw_spin_unlock_irq(&object->lock);
	}
	rcu_read_unlock();

	kmemleak_found_leaks = false;
}

static void __kmemleak_do_cleanup(void);

 
static ssize_t kmemleak_write(struct file *file, const char __user *user_buf,
			      size_t size, loff_t *ppos)
{
	char buf[64];
	int buf_size;
	int ret;

	buf_size = min(size, (sizeof(buf) - 1));
	if (strncpy_from_user(buf, user_buf, buf_size) < 0)
		return -EFAULT;
	buf[buf_size] = 0;

	ret = mutex_lock_interruptible(&scan_mutex);
	if (ret < 0)
		return ret;

	if (strncmp(buf, "clear", 5) == 0) {
		if (kmemleak_enabled)
			kmemleak_clear();
		else
			__kmemleak_do_cleanup();
		goto out;
	}

	if (!kmemleak_enabled) {
		ret = -EPERM;
		goto out;
	}

	if (strncmp(buf, "off", 3) == 0)
		kmemleak_disable();
	else if (strncmp(buf, "stack=on", 8) == 0)
		kmemleak_stack_scan = 1;
	else if (strncmp(buf, "stack=off", 9) == 0)
		kmemleak_stack_scan = 0;
	else if (strncmp(buf, "scan=on", 7) == 0)
		start_scan_thread();
	else if (strncmp(buf, "scan=off", 8) == 0)
		stop_scan_thread();
	else if (strncmp(buf, "scan=", 5) == 0) {
		unsigned secs;
		unsigned long msecs;

		ret = kstrtouint(buf + 5, 0, &secs);
		if (ret < 0)
			goto out;

		msecs = secs * MSEC_PER_SEC;
		if (msecs > UINT_MAX)
			msecs = UINT_MAX;

		stop_scan_thread();
		if (msecs) {
			WRITE_ONCE(jiffies_scan_wait, msecs_to_jiffies(msecs));
			start_scan_thread();
		}
	} else if (strncmp(buf, "scan", 4) == 0)
		kmemleak_scan();
	else if (strncmp(buf, "dump=", 5) == 0)
		ret = dump_str_object_info(buf + 5);
	else
		ret = -EINVAL;

out:
	mutex_unlock(&scan_mutex);
	if (ret < 0)
		return ret;

	 
	*ppos += size;
	return size;
}

static const struct file_operations kmemleak_fops = {
	.owner		= THIS_MODULE,
	.open		= kmemleak_open,
	.read		= seq_read,
	.write		= kmemleak_write,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static void __kmemleak_do_cleanup(void)
{
	struct kmemleak_object *object, *tmp;

	 
	list_for_each_entry_safe(object, tmp, &object_list, object_list) {
		__remove_object(object);
		__delete_object(object);
	}
}

 
static void kmemleak_do_cleanup(struct work_struct *work)
{
	stop_scan_thread();

	mutex_lock(&scan_mutex);
	 
	kmemleak_free_enabled = 0;
	mutex_unlock(&scan_mutex);

	if (!kmemleak_found_leaks)
		__kmemleak_do_cleanup();
	else
		pr_info("Kmemleak disabled without freeing internal data. Reclaim the memory with \"echo clear > /sys/kernel/debug/kmemleak\".\n");
}

static DECLARE_WORK(cleanup_work, kmemleak_do_cleanup);

 
static void kmemleak_disable(void)
{
	 
	if (cmpxchg(&kmemleak_error, 0, 1))
		return;

	 
	kmemleak_enabled = 0;

	 
	if (kmemleak_late_initialized)
		schedule_work(&cleanup_work);
	else
		kmemleak_free_enabled = 0;

	pr_info("Kernel memory leak detector disabled\n");
}

 
static int __init kmemleak_boot_config(char *str)
{
	if (!str)
		return -EINVAL;
	if (strcmp(str, "off") == 0)
		kmemleak_disable();
	else if (strcmp(str, "on") == 0) {
		kmemleak_skip_disable = 1;
		stack_depot_request_early_init();
	}
	else
		return -EINVAL;
	return 0;
}
early_param("kmemleak", kmemleak_boot_config);

 
void __init kmemleak_init(void)
{
#ifdef CONFIG_DEBUG_KMEMLEAK_DEFAULT_OFF
	if (!kmemleak_skip_disable) {
		kmemleak_disable();
		return;
	}
#endif

	if (kmemleak_error)
		return;

	jiffies_min_age = msecs_to_jiffies(MSECS_MIN_AGE);
	jiffies_scan_wait = msecs_to_jiffies(SECS_SCAN_WAIT * 1000);

	object_cache = KMEM_CACHE(kmemleak_object, SLAB_NOLEAKTRACE);
	scan_area_cache = KMEM_CACHE(kmemleak_scan_area, SLAB_NOLEAKTRACE);

	 
	create_object((unsigned long)_sdata, _edata - _sdata,
		      KMEMLEAK_GREY, GFP_ATOMIC);
	create_object((unsigned long)__bss_start, __bss_stop - __bss_start,
		      KMEMLEAK_GREY, GFP_ATOMIC);
	 
	if (&__start_ro_after_init < &_sdata || &__end_ro_after_init > &_edata)
		create_object((unsigned long)__start_ro_after_init,
			      __end_ro_after_init - __start_ro_after_init,
			      KMEMLEAK_GREY, GFP_ATOMIC);
}

 
static int __init kmemleak_late_init(void)
{
	kmemleak_late_initialized = 1;

	debugfs_create_file("kmemleak", 0644, NULL, NULL, &kmemleak_fops);

	if (kmemleak_error) {
		 
		schedule_work(&cleanup_work);
		return -ENOMEM;
	}

	if (IS_ENABLED(CONFIG_DEBUG_KMEMLEAK_AUTO_SCAN)) {
		mutex_lock(&scan_mutex);
		start_scan_thread();
		mutex_unlock(&scan_mutex);
	}

	pr_info("Kernel memory leak detector initialized (mem pool available: %d)\n",
		mem_pool_free_count);

	return 0;
}
late_initcall(kmemleak_late_init);
