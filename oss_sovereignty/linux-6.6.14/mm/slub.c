
 

#include <linux/mm.h>
#include <linux/swap.h>  
#include <linux/module.h>
#include <linux/bit_spinlock.h>
#include <linux/interrupt.h>
#include <linux/swab.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include "slab.h"
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kasan.h>
#include <linux/kmsan.h>
#include <linux/cpu.h>
#include <linux/cpuset.h>
#include <linux/mempolicy.h>
#include <linux/ctype.h>
#include <linux/stackdepot.h>
#include <linux/debugobjects.h>
#include <linux/kallsyms.h>
#include <linux/kfence.h>
#include <linux/memory.h>
#include <linux/math64.h>
#include <linux/fault-inject.h>
#include <linux/stacktrace.h>
#include <linux/prefetch.h>
#include <linux/memcontrol.h>
#include <linux/random.h>
#include <kunit/test.h>
#include <kunit/test-bug.h>
#include <linux/sort.h>

#include <linux/debugfs.h>
#include <trace/events/kmem.h>

#include "internal.h"

 

 
#ifndef CONFIG_PREEMPT_RT
#define slub_get_cpu_ptr(var)		get_cpu_ptr(var)
#define slub_put_cpu_ptr(var)		put_cpu_ptr(var)
#define USE_LOCKLESS_FAST_PATH()	(true)
#else
#define slub_get_cpu_ptr(var)		\
({					\
	migrate_disable();		\
	this_cpu_ptr(var);		\
})
#define slub_put_cpu_ptr(var)		\
do {					\
	(void)(var);			\
	migrate_enable();		\
} while (0)
#define USE_LOCKLESS_FAST_PATH()	(false)
#endif

#ifndef CONFIG_SLUB_TINY
#define __fastpath_inline __always_inline
#else
#define __fastpath_inline
#endif

#ifdef CONFIG_SLUB_DEBUG
#ifdef CONFIG_SLUB_DEBUG_ON
DEFINE_STATIC_KEY_TRUE(slub_debug_enabled);
#else
DEFINE_STATIC_KEY_FALSE(slub_debug_enabled);
#endif
#endif		 

 
struct partial_context {
	struct slab **slab;
	gfp_t flags;
	unsigned int orig_size;
};

static inline bool kmem_cache_debug(struct kmem_cache *s)
{
	return kmem_cache_debug_flags(s, SLAB_DEBUG_FLAGS);
}

static inline bool slub_debug_orig_size(struct kmem_cache *s)
{
	return (kmem_cache_debug_flags(s, SLAB_STORE_USER) &&
			(s->flags & SLAB_KMALLOC));
}

void *fixup_red_left(struct kmem_cache *s, void *p)
{
	if (kmem_cache_debug_flags(s, SLAB_RED_ZONE))
		p += s->red_left_pad;

	return p;
}

static inline bool kmem_cache_has_cpu_partial(struct kmem_cache *s)
{
#ifdef CONFIG_SLUB_CPU_PARTIAL
	return !kmem_cache_debug(s);
#else
	return false;
#endif
}

 

 
#undef SLUB_DEBUG_CMPXCHG

#ifndef CONFIG_SLUB_TINY
 
#define MIN_PARTIAL 5

 
#define MAX_PARTIAL 10
#else
#define MIN_PARTIAL 0
#define MAX_PARTIAL 0
#endif

#define DEBUG_DEFAULT_FLAGS (SLAB_CONSISTENCY_CHECKS | SLAB_RED_ZONE | \
				SLAB_POISON | SLAB_STORE_USER)

 
#define SLAB_NO_CMPXCHG (SLAB_CONSISTENCY_CHECKS | SLAB_STORE_USER | \
				SLAB_TRACE)


 
#define DEBUG_METADATA_FLAGS (SLAB_RED_ZONE | SLAB_POISON | SLAB_STORE_USER)

#define OO_SHIFT	16
#define OO_MASK		((1 << OO_SHIFT) - 1)
#define MAX_OBJS_PER_PAGE	32767  

 
 
#define __OBJECT_POISON		((slab_flags_t __force)0x80000000U)
 

#ifdef system_has_freelist_aba
#define __CMPXCHG_DOUBLE	((slab_flags_t __force)0x40000000U)
#else
#define __CMPXCHG_DOUBLE	((slab_flags_t __force)0U)
#endif

 
#define TRACK_ADDRS_COUNT 16
struct track {
	unsigned long addr;	 
#ifdef CONFIG_STACKDEPOT
	depot_stack_handle_t handle;
#endif
	int cpu;		 
	int pid;		 
	unsigned long when;	 
};

enum track_item { TRACK_ALLOC, TRACK_FREE };

#ifdef SLAB_SUPPORTS_SYSFS
static int sysfs_slab_add(struct kmem_cache *);
static int sysfs_slab_alias(struct kmem_cache *, const char *);
#else
static inline int sysfs_slab_add(struct kmem_cache *s) { return 0; }
static inline int sysfs_slab_alias(struct kmem_cache *s, const char *p)
							{ return 0; }
#endif

#if defined(CONFIG_DEBUG_FS) && defined(CONFIG_SLUB_DEBUG)
static void debugfs_slab_add(struct kmem_cache *);
#else
static inline void debugfs_slab_add(struct kmem_cache *s) { }
#endif

static inline void stat(const struct kmem_cache *s, enum stat_item si)
{
#ifdef CONFIG_SLUB_STATS
	 
	raw_cpu_inc(s->cpu_slab->stat[si]);
#endif
}

 
static nodemask_t slab_nodes;

#ifndef CONFIG_SLUB_TINY
 
static struct workqueue_struct *flushwq;
#endif

 

 
typedef struct { unsigned long v; } freeptr_t;

 
static inline freeptr_t freelist_ptr_encode(const struct kmem_cache *s,
					    void *ptr, unsigned long ptr_addr)
{
	unsigned long encoded;

#ifdef CONFIG_SLAB_FREELIST_HARDENED
	encoded = (unsigned long)ptr ^ s->random ^ swab(ptr_addr);
#else
	encoded = (unsigned long)ptr;
#endif
	return (freeptr_t){.v = encoded};
}

static inline void *freelist_ptr_decode(const struct kmem_cache *s,
					freeptr_t ptr, unsigned long ptr_addr)
{
	void *decoded;

#ifdef CONFIG_SLAB_FREELIST_HARDENED
	decoded = (void *)(ptr.v ^ s->random ^ swab(ptr_addr));
#else
	decoded = (void *)ptr.v;
#endif
	return decoded;
}

static inline void *get_freepointer(struct kmem_cache *s, void *object)
{
	unsigned long ptr_addr;
	freeptr_t p;

	object = kasan_reset_tag(object);
	ptr_addr = (unsigned long)object + s->offset;
	p = *(freeptr_t *)(ptr_addr);
	return freelist_ptr_decode(s, p, ptr_addr);
}

#ifndef CONFIG_SLUB_TINY
static void prefetch_freepointer(const struct kmem_cache *s, void *object)
{
	prefetchw(object + s->offset);
}
#endif

 
__no_kmsan_checks
static inline void *get_freepointer_safe(struct kmem_cache *s, void *object)
{
	unsigned long freepointer_addr;
	freeptr_t p;

	if (!debug_pagealloc_enabled_static())
		return get_freepointer(s, object);

	object = kasan_reset_tag(object);
	freepointer_addr = (unsigned long)object + s->offset;
	copy_from_kernel_nofault(&p, (freeptr_t *)freepointer_addr, sizeof(p));
	return freelist_ptr_decode(s, p, freepointer_addr);
}

static inline void set_freepointer(struct kmem_cache *s, void *object, void *fp)
{
	unsigned long freeptr_addr = (unsigned long)object + s->offset;

#ifdef CONFIG_SLAB_FREELIST_HARDENED
	BUG_ON(object == fp);  
#endif

	freeptr_addr = (unsigned long)kasan_reset_tag((void *)freeptr_addr);
	*(freeptr_t *)freeptr_addr = freelist_ptr_encode(s, fp, freeptr_addr);
}

 
#define for_each_object(__p, __s, __addr, __objects) \
	for (__p = fixup_red_left(__s, __addr); \
		__p < (__addr) + (__objects) * (__s)->size; \
		__p += (__s)->size)

static inline unsigned int order_objects(unsigned int order, unsigned int size)
{
	return ((unsigned int)PAGE_SIZE << order) / size;
}

static inline struct kmem_cache_order_objects oo_make(unsigned int order,
		unsigned int size)
{
	struct kmem_cache_order_objects x = {
		(order << OO_SHIFT) + order_objects(order, size)
	};

	return x;
}

static inline unsigned int oo_order(struct kmem_cache_order_objects x)
{
	return x.x >> OO_SHIFT;
}

static inline unsigned int oo_objects(struct kmem_cache_order_objects x)
{
	return x.x & OO_MASK;
}

#ifdef CONFIG_SLUB_CPU_PARTIAL
static void slub_set_cpu_partial(struct kmem_cache *s, unsigned int nr_objects)
{
	unsigned int nr_slabs;

	s->cpu_partial = nr_objects;

	 
	nr_slabs = DIV_ROUND_UP(nr_objects * 2, oo_objects(s->oo));
	s->cpu_partial_slabs = nr_slabs;
}
#else
static inline void
slub_set_cpu_partial(struct kmem_cache *s, unsigned int nr_objects)
{
}
#endif  

 
static __always_inline void slab_lock(struct slab *slab)
{
	struct page *page = slab_page(slab);

	VM_BUG_ON_PAGE(PageTail(page), page);
	bit_spin_lock(PG_locked, &page->flags);
}

static __always_inline void slab_unlock(struct slab *slab)
{
	struct page *page = slab_page(slab);

	VM_BUG_ON_PAGE(PageTail(page), page);
	__bit_spin_unlock(PG_locked, &page->flags);
}

static inline bool
__update_freelist_fast(struct slab *slab,
		      void *freelist_old, unsigned long counters_old,
		      void *freelist_new, unsigned long counters_new)
{
#ifdef system_has_freelist_aba
	freelist_aba_t old = { .freelist = freelist_old, .counter = counters_old };
	freelist_aba_t new = { .freelist = freelist_new, .counter = counters_new };

	return try_cmpxchg_freelist(&slab->freelist_counter.full, &old.full, new.full);
#else
	return false;
#endif
}

static inline bool
__update_freelist_slow(struct slab *slab,
		      void *freelist_old, unsigned long counters_old,
		      void *freelist_new, unsigned long counters_new)
{
	bool ret = false;

	slab_lock(slab);
	if (slab->freelist == freelist_old &&
	    slab->counters == counters_old) {
		slab->freelist = freelist_new;
		slab->counters = counters_new;
		ret = true;
	}
	slab_unlock(slab);

	return ret;
}

 
static inline bool __slab_update_freelist(struct kmem_cache *s, struct slab *slab,
		void *freelist_old, unsigned long counters_old,
		void *freelist_new, unsigned long counters_new,
		const char *n)
{
	bool ret;

	if (USE_LOCKLESS_FAST_PATH())
		lockdep_assert_irqs_disabled();

	if (s->flags & __CMPXCHG_DOUBLE) {
		ret = __update_freelist_fast(slab, freelist_old, counters_old,
				            freelist_new, counters_new);
	} else {
		ret = __update_freelist_slow(slab, freelist_old, counters_old,
				            freelist_new, counters_new);
	}
	if (likely(ret))
		return true;

	cpu_relax();
	stat(s, CMPXCHG_DOUBLE_FAIL);

#ifdef SLUB_DEBUG_CMPXCHG
	pr_info("%s %s: cmpxchg double redo ", n, s->name);
#endif

	return false;
}

static inline bool slab_update_freelist(struct kmem_cache *s, struct slab *slab,
		void *freelist_old, unsigned long counters_old,
		void *freelist_new, unsigned long counters_new,
		const char *n)
{
	bool ret;

	if (s->flags & __CMPXCHG_DOUBLE) {
		ret = __update_freelist_fast(slab, freelist_old, counters_old,
				            freelist_new, counters_new);
	} else {
		unsigned long flags;

		local_irq_save(flags);
		ret = __update_freelist_slow(slab, freelist_old, counters_old,
				            freelist_new, counters_new);
		local_irq_restore(flags);
	}
	if (likely(ret))
		return true;

	cpu_relax();
	stat(s, CMPXCHG_DOUBLE_FAIL);

#ifdef SLUB_DEBUG_CMPXCHG
	pr_info("%s %s: cmpxchg double redo ", n, s->name);
#endif

	return false;
}

#ifdef CONFIG_SLUB_DEBUG
static unsigned long object_map[BITS_TO_LONGS(MAX_OBJS_PER_PAGE)];
static DEFINE_SPINLOCK(object_map_lock);

static void __fill_map(unsigned long *obj_map, struct kmem_cache *s,
		       struct slab *slab)
{
	void *addr = slab_address(slab);
	void *p;

	bitmap_zero(obj_map, slab->objects);

	for (p = slab->freelist; p; p = get_freepointer(s, p))
		set_bit(__obj_to_index(s, addr, p), obj_map);
}

#if IS_ENABLED(CONFIG_KUNIT)
static bool slab_add_kunit_errors(void)
{
	struct kunit_resource *resource;

	if (!kunit_get_current_test())
		return false;

	resource = kunit_find_named_resource(current->kunit_test, "slab_errors");
	if (!resource)
		return false;

	(*(int *)resource->data)++;
	kunit_put_resource(resource);
	return true;
}
#else
static inline bool slab_add_kunit_errors(void) { return false; }
#endif

static inline unsigned int size_from_object(struct kmem_cache *s)
{
	if (s->flags & SLAB_RED_ZONE)
		return s->size - s->red_left_pad;

	return s->size;
}

static inline void *restore_red_left(struct kmem_cache *s, void *p)
{
	if (s->flags & SLAB_RED_ZONE)
		p -= s->red_left_pad;

	return p;
}

 
#if defined(CONFIG_SLUB_DEBUG_ON)
static slab_flags_t slub_debug = DEBUG_DEFAULT_FLAGS;
#else
static slab_flags_t slub_debug;
#endif

static char *slub_debug_string;
static int disable_higher_order_debug;

 
static inline void metadata_access_enable(void)
{
	kasan_disable_current();
}

static inline void metadata_access_disable(void)
{
	kasan_enable_current();
}

 

 
static inline int check_valid_pointer(struct kmem_cache *s,
				struct slab *slab, void *object)
{
	void *base;

	if (!object)
		return 1;

	base = slab_address(slab);
	object = kasan_reset_tag(object);
	object = restore_red_left(s, object);
	if (object < base || object >= base + slab->objects * s->size ||
		(object - base) % s->size) {
		return 0;
	}

	return 1;
}

static void print_section(char *level, char *text, u8 *addr,
			  unsigned int length)
{
	metadata_access_enable();
	print_hex_dump(level, text, DUMP_PREFIX_ADDRESS,
			16, 1, kasan_reset_tag((void *)addr), length, 1);
	metadata_access_disable();
}

 
static inline bool freeptr_outside_object(struct kmem_cache *s)
{
	return s->offset >= s->inuse;
}

 
static inline unsigned int get_info_end(struct kmem_cache *s)
{
	if (freeptr_outside_object(s))
		return s->inuse + sizeof(void *);
	else
		return s->inuse;
}

static struct track *get_track(struct kmem_cache *s, void *object,
	enum track_item alloc)
{
	struct track *p;

	p = object + get_info_end(s);

	return kasan_reset_tag(p + alloc);
}

#ifdef CONFIG_STACKDEPOT
static noinline depot_stack_handle_t set_track_prepare(void)
{
	depot_stack_handle_t handle;
	unsigned long entries[TRACK_ADDRS_COUNT];
	unsigned int nr_entries;

	nr_entries = stack_trace_save(entries, ARRAY_SIZE(entries), 3);
	handle = stack_depot_save(entries, nr_entries, GFP_NOWAIT);

	return handle;
}
#else
static inline depot_stack_handle_t set_track_prepare(void)
{
	return 0;
}
#endif

static void set_track_update(struct kmem_cache *s, void *object,
			     enum track_item alloc, unsigned long addr,
			     depot_stack_handle_t handle)
{
	struct track *p = get_track(s, object, alloc);

#ifdef CONFIG_STACKDEPOT
	p->handle = handle;
#endif
	p->addr = addr;
	p->cpu = smp_processor_id();
	p->pid = current->pid;
	p->when = jiffies;
}

static __always_inline void set_track(struct kmem_cache *s, void *object,
				      enum track_item alloc, unsigned long addr)
{
	depot_stack_handle_t handle = set_track_prepare();

	set_track_update(s, object, alloc, addr, handle);
}

static void init_tracking(struct kmem_cache *s, void *object)
{
	struct track *p;

	if (!(s->flags & SLAB_STORE_USER))
		return;

	p = get_track(s, object, TRACK_ALLOC);
	memset(p, 0, 2*sizeof(struct track));
}

static void print_track(const char *s, struct track *t, unsigned long pr_time)
{
	depot_stack_handle_t handle __maybe_unused;

	if (!t->addr)
		return;

	pr_err("%s in %pS age=%lu cpu=%u pid=%d\n",
	       s, (void *)t->addr, pr_time - t->when, t->cpu, t->pid);
#ifdef CONFIG_STACKDEPOT
	handle = READ_ONCE(t->handle);
	if (handle)
		stack_depot_print(handle);
	else
		pr_err("object allocation/free stack trace missing\n");
#endif
}

void print_tracking(struct kmem_cache *s, void *object)
{
	unsigned long pr_time = jiffies;
	if (!(s->flags & SLAB_STORE_USER))
		return;

	print_track("Allocated", get_track(s, object, TRACK_ALLOC), pr_time);
	print_track("Freed", get_track(s, object, TRACK_FREE), pr_time);
}

static void print_slab_info(const struct slab *slab)
{
	struct folio *folio = (struct folio *)slab_folio(slab);

	pr_err("Slab 0x%p objects=%u used=%u fp=0x%p flags=%pGp\n",
	       slab, slab->objects, slab->inuse, slab->freelist,
	       folio_flags(folio, 0));
}

 
static inline void set_orig_size(struct kmem_cache *s,
				void *object, unsigned int orig_size)
{
	void *p = kasan_reset_tag(object);

	if (!slub_debug_orig_size(s))
		return;

#ifdef CONFIG_KASAN_GENERIC
	 
	if (kasan_metadata_size(s, true) > orig_size)
		orig_size = s->object_size;
#endif

	p += get_info_end(s);
	p += sizeof(struct track) * 2;

	*(unsigned int *)p = orig_size;
}

static inline unsigned int get_orig_size(struct kmem_cache *s, void *object)
{
	void *p = kasan_reset_tag(object);

	if (!slub_debug_orig_size(s))
		return s->object_size;

	p += get_info_end(s);
	p += sizeof(struct track) * 2;

	return *(unsigned int *)p;
}

void skip_orig_size_check(struct kmem_cache *s, const void *object)
{
	set_orig_size(s, (void *)object, s->object_size);
}

static void slab_bug(struct kmem_cache *s, char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	pr_err("=============================================================================\n");
	pr_err("BUG %s (%s): %pV\n", s->name, print_tainted(), &vaf);
	pr_err("-----------------------------------------------------------------------------\n\n");
	va_end(args);
}

__printf(2, 3)
static void slab_fix(struct kmem_cache *s, char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	if (slab_add_kunit_errors())
		return;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	pr_err("FIX %s: %pV\n", s->name, &vaf);
	va_end(args);
}

static void print_trailer(struct kmem_cache *s, struct slab *slab, u8 *p)
{
	unsigned int off;	 
	u8 *addr = slab_address(slab);

	print_tracking(s, p);

	print_slab_info(slab);

	pr_err("Object 0x%p @offset=%tu fp=0x%p\n\n",
	       p, p - addr, get_freepointer(s, p));

	if (s->flags & SLAB_RED_ZONE)
		print_section(KERN_ERR, "Redzone  ", p - s->red_left_pad,
			      s->red_left_pad);
	else if (p > addr + 16)
		print_section(KERN_ERR, "Bytes b4 ", p - 16, 16);

	print_section(KERN_ERR,         "Object   ", p,
		      min_t(unsigned int, s->object_size, PAGE_SIZE));
	if (s->flags & SLAB_RED_ZONE)
		print_section(KERN_ERR, "Redzone  ", p + s->object_size,
			s->inuse - s->object_size);

	off = get_info_end(s);

	if (s->flags & SLAB_STORE_USER)
		off += 2 * sizeof(struct track);

	if (slub_debug_orig_size(s))
		off += sizeof(unsigned int);

	off += kasan_metadata_size(s, false);

	if (off != size_from_object(s))
		 
		print_section(KERN_ERR, "Padding  ", p + off,
			      size_from_object(s) - off);

	dump_stack();
}

static void object_err(struct kmem_cache *s, struct slab *slab,
			u8 *object, char *reason)
{
	if (slab_add_kunit_errors())
		return;

	slab_bug(s, "%s", reason);
	print_trailer(s, slab, object);
	add_taint(TAINT_BAD_PAGE, LOCKDEP_NOW_UNRELIABLE);
}

static bool freelist_corrupted(struct kmem_cache *s, struct slab *slab,
			       void **freelist, void *nextfree)
{
	if ((s->flags & SLAB_CONSISTENCY_CHECKS) &&
	    !check_valid_pointer(s, slab, nextfree) && freelist) {
		object_err(s, slab, *freelist, "Freechain corrupt");
		*freelist = NULL;
		slab_fix(s, "Isolate corrupted freechain");
		return true;
	}

	return false;
}

static __printf(3, 4) void slab_err(struct kmem_cache *s, struct slab *slab,
			const char *fmt, ...)
{
	va_list args;
	char buf[100];

	if (slab_add_kunit_errors())
		return;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	slab_bug(s, "%s", buf);
	print_slab_info(slab);
	dump_stack();
	add_taint(TAINT_BAD_PAGE, LOCKDEP_NOW_UNRELIABLE);
}

static void init_object(struct kmem_cache *s, void *object, u8 val)
{
	u8 *p = kasan_reset_tag(object);
	unsigned int poison_size = s->object_size;

	if (s->flags & SLAB_RED_ZONE) {
		memset(p - s->red_left_pad, val, s->red_left_pad);

		if (slub_debug_orig_size(s) && val == SLUB_RED_ACTIVE) {
			 
			poison_size = get_orig_size(s, object);
		}
	}

	if (s->flags & __OBJECT_POISON) {
		memset(p, POISON_FREE, poison_size - 1);
		p[poison_size - 1] = POISON_END;
	}

	if (s->flags & SLAB_RED_ZONE)
		memset(p + poison_size, val, s->inuse - poison_size);
}

static void restore_bytes(struct kmem_cache *s, char *message, u8 data,
						void *from, void *to)
{
	slab_fix(s, "Restoring %s 0x%p-0x%p=0x%x", message, from, to - 1, data);
	memset(from, data, to - from);
}

static int check_bytes_and_report(struct kmem_cache *s, struct slab *slab,
			u8 *object, char *what,
			u8 *start, unsigned int value, unsigned int bytes)
{
	u8 *fault;
	u8 *end;
	u8 *addr = slab_address(slab);

	metadata_access_enable();
	fault = memchr_inv(kasan_reset_tag(start), value, bytes);
	metadata_access_disable();
	if (!fault)
		return 1;

	end = start + bytes;
	while (end > fault && end[-1] == value)
		end--;

	if (slab_add_kunit_errors())
		goto skip_bug_print;

	slab_bug(s, "%s overwritten", what);
	pr_err("0x%p-0x%p @offset=%tu. First byte 0x%x instead of 0x%x\n",
					fault, end - 1, fault - addr,
					fault[0], value);
	print_trailer(s, slab, object);
	add_taint(TAINT_BAD_PAGE, LOCKDEP_NOW_UNRELIABLE);

skip_bug_print:
	restore_bytes(s, what, value, fault, end);
	return 0;
}

 

static int check_pad_bytes(struct kmem_cache *s, struct slab *slab, u8 *p)
{
	unsigned long off = get_info_end(s);	 

	if (s->flags & SLAB_STORE_USER) {
		 
		off += 2 * sizeof(struct track);

		if (s->flags & SLAB_KMALLOC)
			off += sizeof(unsigned int);
	}

	off += kasan_metadata_size(s, false);

	if (size_from_object(s) == off)
		return 1;

	return check_bytes_and_report(s, slab, p, "Object padding",
			p + off, POISON_INUSE, size_from_object(s) - off);
}

 
static void slab_pad_check(struct kmem_cache *s, struct slab *slab)
{
	u8 *start;
	u8 *fault;
	u8 *end;
	u8 *pad;
	int length;
	int remainder;

	if (!(s->flags & SLAB_POISON))
		return;

	start = slab_address(slab);
	length = slab_size(slab);
	end = start + length;
	remainder = length % s->size;
	if (!remainder)
		return;

	pad = end - remainder;
	metadata_access_enable();
	fault = memchr_inv(kasan_reset_tag(pad), POISON_INUSE, remainder);
	metadata_access_disable();
	if (!fault)
		return;
	while (end > fault && end[-1] == POISON_INUSE)
		end--;

	slab_err(s, slab, "Padding overwritten. 0x%p-0x%p @offset=%tu",
			fault, end - 1, fault - start);
	print_section(KERN_ERR, "Padding ", pad, remainder);

	restore_bytes(s, "slab padding", POISON_INUSE, fault, end);
}

static int check_object(struct kmem_cache *s, struct slab *slab,
					void *object, u8 val)
{
	u8 *p = object;
	u8 *endobject = object + s->object_size;
	unsigned int orig_size;

	if (s->flags & SLAB_RED_ZONE) {
		if (!check_bytes_and_report(s, slab, object, "Left Redzone",
			object - s->red_left_pad, val, s->red_left_pad))
			return 0;

		if (!check_bytes_and_report(s, slab, object, "Right Redzone",
			endobject, val, s->inuse - s->object_size))
			return 0;

		if (slub_debug_orig_size(s) && val == SLUB_RED_ACTIVE) {
			orig_size = get_orig_size(s, object);

			if (s->object_size > orig_size  &&
				!check_bytes_and_report(s, slab, object,
					"kmalloc Redzone", p + orig_size,
					val, s->object_size - orig_size)) {
				return 0;
			}
		}
	} else {
		if ((s->flags & SLAB_POISON) && s->object_size < s->inuse) {
			check_bytes_and_report(s, slab, p, "Alignment padding",
				endobject, POISON_INUSE,
				s->inuse - s->object_size);
		}
	}

	if (s->flags & SLAB_POISON) {
		if (val != SLUB_RED_ACTIVE && (s->flags & __OBJECT_POISON) &&
			(!check_bytes_and_report(s, slab, p, "Poison", p,
					POISON_FREE, s->object_size - 1) ||
			 !check_bytes_and_report(s, slab, p, "End Poison",
				p + s->object_size - 1, POISON_END, 1)))
			return 0;
		 
		check_pad_bytes(s, slab, p);
	}

	if (!freeptr_outside_object(s) && val == SLUB_RED_ACTIVE)
		 
		return 1;

	 
	if (!check_valid_pointer(s, slab, get_freepointer(s, p))) {
		object_err(s, slab, p, "Freepointer corrupt");
		 
		set_freepointer(s, p, NULL);
		return 0;
	}
	return 1;
}

static int check_slab(struct kmem_cache *s, struct slab *slab)
{
	int maxobj;

	if (!folio_test_slab(slab_folio(slab))) {
		slab_err(s, slab, "Not a valid slab page");
		return 0;
	}

	maxobj = order_objects(slab_order(slab), s->size);
	if (slab->objects > maxobj) {
		slab_err(s, slab, "objects %u > max %u",
			slab->objects, maxobj);
		return 0;
	}
	if (slab->inuse > slab->objects) {
		slab_err(s, slab, "inuse %u > max %u",
			slab->inuse, slab->objects);
		return 0;
	}
	 
	slab_pad_check(s, slab);
	return 1;
}

 
static int on_freelist(struct kmem_cache *s, struct slab *slab, void *search)
{
	int nr = 0;
	void *fp;
	void *object = NULL;
	int max_objects;

	fp = slab->freelist;
	while (fp && nr <= slab->objects) {
		if (fp == search)
			return 1;
		if (!check_valid_pointer(s, slab, fp)) {
			if (object) {
				object_err(s, slab, object,
					"Freechain corrupt");
				set_freepointer(s, object, NULL);
			} else {
				slab_err(s, slab, "Freepointer corrupt");
				slab->freelist = NULL;
				slab->inuse = slab->objects;
				slab_fix(s, "Freelist cleared");
				return 0;
			}
			break;
		}
		object = fp;
		fp = get_freepointer(s, object);
		nr++;
	}

	max_objects = order_objects(slab_order(slab), s->size);
	if (max_objects > MAX_OBJS_PER_PAGE)
		max_objects = MAX_OBJS_PER_PAGE;

	if (slab->objects != max_objects) {
		slab_err(s, slab, "Wrong number of objects. Found %d but should be %d",
			 slab->objects, max_objects);
		slab->objects = max_objects;
		slab_fix(s, "Number of objects adjusted");
	}
	if (slab->inuse != slab->objects - nr) {
		slab_err(s, slab, "Wrong object count. Counter is %d but counted were %d",
			 slab->inuse, slab->objects - nr);
		slab->inuse = slab->objects - nr;
		slab_fix(s, "Object count adjusted");
	}
	return search == NULL;
}

static void trace(struct kmem_cache *s, struct slab *slab, void *object,
								int alloc)
{
	if (s->flags & SLAB_TRACE) {
		pr_info("TRACE %s %s 0x%p inuse=%d fp=0x%p\n",
			s->name,
			alloc ? "alloc" : "free",
			object, slab->inuse,
			slab->freelist);

		if (!alloc)
			print_section(KERN_INFO, "Object ", (void *)object,
					s->object_size);

		dump_stack();
	}
}

 
static void add_full(struct kmem_cache *s,
	struct kmem_cache_node *n, struct slab *slab)
{
	if (!(s->flags & SLAB_STORE_USER))
		return;

	lockdep_assert_held(&n->list_lock);
	list_add(&slab->slab_list, &n->full);
}

static void remove_full(struct kmem_cache *s, struct kmem_cache_node *n, struct slab *slab)
{
	if (!(s->flags & SLAB_STORE_USER))
		return;

	lockdep_assert_held(&n->list_lock);
	list_del(&slab->slab_list);
}

static inline unsigned long node_nr_slabs(struct kmem_cache_node *n)
{
	return atomic_long_read(&n->nr_slabs);
}

static inline void inc_slabs_node(struct kmem_cache *s, int node, int objects)
{
	struct kmem_cache_node *n = get_node(s, node);

	 
	if (likely(n)) {
		atomic_long_inc(&n->nr_slabs);
		atomic_long_add(objects, &n->total_objects);
	}
}
static inline void dec_slabs_node(struct kmem_cache *s, int node, int objects)
{
	struct kmem_cache_node *n = get_node(s, node);

	atomic_long_dec(&n->nr_slabs);
	atomic_long_sub(objects, &n->total_objects);
}

 
static void setup_object_debug(struct kmem_cache *s, void *object)
{
	if (!kmem_cache_debug_flags(s, SLAB_STORE_USER|SLAB_RED_ZONE|__OBJECT_POISON))
		return;

	init_object(s, object, SLUB_RED_INACTIVE);
	init_tracking(s, object);
}

static
void setup_slab_debug(struct kmem_cache *s, struct slab *slab, void *addr)
{
	if (!kmem_cache_debug_flags(s, SLAB_POISON))
		return;

	metadata_access_enable();
	memset(kasan_reset_tag(addr), POISON_INUSE, slab_size(slab));
	metadata_access_disable();
}

static inline int alloc_consistency_checks(struct kmem_cache *s,
					struct slab *slab, void *object)
{
	if (!check_slab(s, slab))
		return 0;

	if (!check_valid_pointer(s, slab, object)) {
		object_err(s, slab, object, "Freelist Pointer check fails");
		return 0;
	}

	if (!check_object(s, slab, object, SLUB_RED_INACTIVE))
		return 0;

	return 1;
}

static noinline bool alloc_debug_processing(struct kmem_cache *s,
			struct slab *slab, void *object, int orig_size)
{
	if (s->flags & SLAB_CONSISTENCY_CHECKS) {
		if (!alloc_consistency_checks(s, slab, object))
			goto bad;
	}

	 
	trace(s, slab, object, 1);
	set_orig_size(s, object, orig_size);
	init_object(s, object, SLUB_RED_ACTIVE);
	return true;

bad:
	if (folio_test_slab(slab_folio(slab))) {
		 
		slab_fix(s, "Marking all objects used");
		slab->inuse = slab->objects;
		slab->freelist = NULL;
	}
	return false;
}

static inline int free_consistency_checks(struct kmem_cache *s,
		struct slab *slab, void *object, unsigned long addr)
{
	if (!check_valid_pointer(s, slab, object)) {
		slab_err(s, slab, "Invalid object pointer 0x%p", object);
		return 0;
	}

	if (on_freelist(s, slab, object)) {
		object_err(s, slab, object, "Object already free");
		return 0;
	}

	if (!check_object(s, slab, object, SLUB_RED_ACTIVE))
		return 0;

	if (unlikely(s != slab->slab_cache)) {
		if (!folio_test_slab(slab_folio(slab))) {
			slab_err(s, slab, "Attempt to free object(0x%p) outside of slab",
				 object);
		} else if (!slab->slab_cache) {
			pr_err("SLUB <none>: no slab for object 0x%p.\n",
			       object);
			dump_stack();
		} else
			object_err(s, slab, object,
					"page slab pointer corrupt.");
		return 0;
	}
	return 1;
}

 
static char *
parse_slub_debug_flags(char *str, slab_flags_t *flags, char **slabs, bool init)
{
	bool higher_order_disable = false;

	 
	while (*str && *str == ';')
		str++;

	if (*str == ',') {
		 
		*flags = DEBUG_DEFAULT_FLAGS;
		goto check_slabs;
	}
	*flags = 0;

	 
	for (; *str && *str != ',' && *str != ';'; str++) {
		switch (tolower(*str)) {
		case '-':
			*flags = 0;
			break;
		case 'f':
			*flags |= SLAB_CONSISTENCY_CHECKS;
			break;
		case 'z':
			*flags |= SLAB_RED_ZONE;
			break;
		case 'p':
			*flags |= SLAB_POISON;
			break;
		case 'u':
			*flags |= SLAB_STORE_USER;
			break;
		case 't':
			*flags |= SLAB_TRACE;
			break;
		case 'a':
			*flags |= SLAB_FAILSLAB;
			break;
		case 'o':
			 
			higher_order_disable = true;
			break;
		default:
			if (init)
				pr_err("slub_debug option '%c' unknown. skipped\n", *str);
		}
	}
check_slabs:
	if (*str == ',')
		*slabs = ++str;
	else
		*slabs = NULL;

	 
	while (*str && *str != ';')
		str++;

	 
	while (*str && *str == ';')
		str++;

	if (init && higher_order_disable)
		disable_higher_order_debug = 1;

	if (*str)
		return str;
	else
		return NULL;
}

static int __init setup_slub_debug(char *str)
{
	slab_flags_t flags;
	slab_flags_t global_flags;
	char *saved_str;
	char *slab_list;
	bool global_slub_debug_changed = false;
	bool slab_list_specified = false;

	global_flags = DEBUG_DEFAULT_FLAGS;
	if (*str++ != '=' || !*str)
		 
		goto out;

	saved_str = str;
	while (str) {
		str = parse_slub_debug_flags(str, &flags, &slab_list, true);

		if (!slab_list) {
			global_flags = flags;
			global_slub_debug_changed = true;
		} else {
			slab_list_specified = true;
			if (flags & SLAB_STORE_USER)
				stack_depot_request_early_init();
		}
	}

	 
	if (slab_list_specified) {
		if (!global_slub_debug_changed)
			global_flags = slub_debug;
		slub_debug_string = saved_str;
	}
out:
	slub_debug = global_flags;
	if (slub_debug & SLAB_STORE_USER)
		stack_depot_request_early_init();
	if (slub_debug != 0 || slub_debug_string)
		static_branch_enable(&slub_debug_enabled);
	else
		static_branch_disable(&slub_debug_enabled);
	if ((static_branch_unlikely(&init_on_alloc) ||
	     static_branch_unlikely(&init_on_free)) &&
	    (slub_debug & SLAB_POISON))
		pr_info("mem auto-init: SLAB_POISON will take precedence over init_on_alloc/init_on_free\n");
	return 1;
}

__setup("slub_debug", setup_slub_debug);

 
slab_flags_t kmem_cache_flags(unsigned int object_size,
	slab_flags_t flags, const char *name)
{
	char *iter;
	size_t len;
	char *next_block;
	slab_flags_t block_flags;
	slab_flags_t slub_debug_local = slub_debug;

	if (flags & SLAB_NO_USER_FLAGS)
		return flags;

	 
	if (flags & SLAB_NOLEAKTRACE)
		slub_debug_local &= ~SLAB_STORE_USER;

	len = strlen(name);
	next_block = slub_debug_string;
	 
	while (next_block) {
		next_block = parse_slub_debug_flags(next_block, &block_flags, &iter, false);
		if (!iter)
			continue;
		 
		while (*iter) {
			char *end, *glob;
			size_t cmplen;

			end = strchrnul(iter, ',');
			if (next_block && next_block < end)
				end = next_block - 1;

			glob = strnchr(iter, end - iter, '*');
			if (glob)
				cmplen = glob - iter;
			else
				cmplen = max_t(size_t, len, (end - iter));

			if (!strncmp(name, iter, cmplen)) {
				flags |= block_flags;
				return flags;
			}

			if (!*end || *end == ';')
				break;
			iter = end + 1;
		}
	}

	return flags | slub_debug_local;
}
#else  
static inline void setup_object_debug(struct kmem_cache *s, void *object) {}
static inline
void setup_slab_debug(struct kmem_cache *s, struct slab *slab, void *addr) {}

static inline bool alloc_debug_processing(struct kmem_cache *s,
	struct slab *slab, void *object, int orig_size) { return true; }

static inline bool free_debug_processing(struct kmem_cache *s,
	struct slab *slab, void *head, void *tail, int *bulk_cnt,
	unsigned long addr, depot_stack_handle_t handle) { return true; }

static inline void slab_pad_check(struct kmem_cache *s, struct slab *slab) {}
static inline int check_object(struct kmem_cache *s, struct slab *slab,
			void *object, u8 val) { return 1; }
static inline depot_stack_handle_t set_track_prepare(void) { return 0; }
static inline void set_track(struct kmem_cache *s, void *object,
			     enum track_item alloc, unsigned long addr) {}
static inline void add_full(struct kmem_cache *s, struct kmem_cache_node *n,
					struct slab *slab) {}
static inline void remove_full(struct kmem_cache *s, struct kmem_cache_node *n,
					struct slab *slab) {}
slab_flags_t kmem_cache_flags(unsigned int object_size,
	slab_flags_t flags, const char *name)
{
	return flags;
}
#define slub_debug 0

#define disable_higher_order_debug 0

static inline unsigned long node_nr_slabs(struct kmem_cache_node *n)
							{ return 0; }
static inline void inc_slabs_node(struct kmem_cache *s, int node,
							int objects) {}
static inline void dec_slabs_node(struct kmem_cache *s, int node,
							int objects) {}

#ifndef CONFIG_SLUB_TINY
static bool freelist_corrupted(struct kmem_cache *s, struct slab *slab,
			       void **freelist, void *nextfree)
{
	return false;
}
#endif
#endif  

 
static __always_inline bool slab_free_hook(struct kmem_cache *s,
						void *x, bool init)
{
	kmemleak_free_recursive(x, s->flags);
	kmsan_slab_free(s, x);

	debug_check_no_locks_freed(x, s->object_size);

	if (!(s->flags & SLAB_DEBUG_OBJECTS))
		debug_check_no_obj_freed(x, s->object_size);

	 
	if (!(s->flags & SLAB_TYPESAFE_BY_RCU))
		__kcsan_check_access(x, s->object_size,
				     KCSAN_ACCESS_WRITE | KCSAN_ACCESS_ASSERT);

	 
	if (init) {
		int rsize;

		if (!kasan_has_integrated_init())
			memset(kasan_reset_tag(x), 0, s->object_size);
		rsize = (s->flags & SLAB_RED_ZONE) ? s->red_left_pad : 0;
		memset((char *)kasan_reset_tag(x) + s->inuse, 0,
		       s->size - s->inuse - rsize);
	}
	 
	return kasan_slab_free(s, x, init);
}

static inline bool slab_free_freelist_hook(struct kmem_cache *s,
					   void **head, void **tail,
					   int *cnt)
{

	void *object;
	void *next = *head;
	void *old_tail = *tail ? *tail : *head;

	if (is_kfence_address(next)) {
		slab_free_hook(s, next, false);
		return true;
	}

	 
	*head = NULL;
	*tail = NULL;

	do {
		object = next;
		next = get_freepointer(s, object);

		 
		if (!slab_free_hook(s, object, slab_want_init_on_free(s))) {
			 
			set_freepointer(s, object, *head);
			*head = object;
			if (!*tail)
				*tail = object;
		} else {
			 
			--(*cnt);
		}
	} while (object != old_tail);

	if (*head == *tail)
		*tail = NULL;

	return *head != NULL;
}

static void *setup_object(struct kmem_cache *s, void *object)
{
	setup_object_debug(s, object);
	object = kasan_init_slab_obj(s, object);
	if (unlikely(s->ctor)) {
		kasan_unpoison_object_data(s, object);
		s->ctor(object);
		kasan_poison_object_data(s, object);
	}
	return object;
}

 
static inline struct slab *alloc_slab_page(gfp_t flags, int node,
		struct kmem_cache_order_objects oo)
{
	struct folio *folio;
	struct slab *slab;
	unsigned int order = oo_order(oo);

	if (node == NUMA_NO_NODE)
		folio = (struct folio *)alloc_pages(flags, order);
	else
		folio = (struct folio *)__alloc_pages_node(node, flags, order);

	if (!folio)
		return NULL;

	slab = folio_slab(folio);
	__folio_set_slab(folio);
	 
	smp_wmb();
	if (folio_is_pfmemalloc(folio))
		slab_set_pfmemalloc(slab);

	return slab;
}

#ifdef CONFIG_SLAB_FREELIST_RANDOM
 
static int init_cache_random_seq(struct kmem_cache *s)
{
	unsigned int count = oo_objects(s->oo);
	int err;

	 
	if (s->random_seq)
		return 0;

	err = cache_random_seq_create(s, count, GFP_KERNEL);
	if (err) {
		pr_err("SLUB: Unable to initialize free list for %s\n",
			s->name);
		return err;
	}

	 
	if (s->random_seq) {
		unsigned int i;

		for (i = 0; i < count; i++)
			s->random_seq[i] *= s->size;
	}
	return 0;
}

 
static void __init init_freelist_randomization(void)
{
	struct kmem_cache *s;

	mutex_lock(&slab_mutex);

	list_for_each_entry(s, &slab_caches, list)
		init_cache_random_seq(s);

	mutex_unlock(&slab_mutex);
}

 
static void *next_freelist_entry(struct kmem_cache *s, struct slab *slab,
				unsigned long *pos, void *start,
				unsigned long page_limit,
				unsigned long freelist_count)
{
	unsigned int idx;

	 
	do {
		idx = s->random_seq[*pos];
		*pos += 1;
		if (*pos >= freelist_count)
			*pos = 0;
	} while (unlikely(idx >= page_limit));

	return (char *)start + idx;
}

 
static bool shuffle_freelist(struct kmem_cache *s, struct slab *slab)
{
	void *start;
	void *cur;
	void *next;
	unsigned long idx, pos, page_limit, freelist_count;

	if (slab->objects < 2 || !s->random_seq)
		return false;

	freelist_count = oo_objects(s->oo);
	pos = get_random_u32_below(freelist_count);

	page_limit = slab->objects * s->size;
	start = fixup_red_left(s, slab_address(slab));

	 
	cur = next_freelist_entry(s, slab, &pos, start, page_limit,
				freelist_count);
	cur = setup_object(s, cur);
	slab->freelist = cur;

	for (idx = 1; idx < slab->objects; idx++) {
		next = next_freelist_entry(s, slab, &pos, start, page_limit,
			freelist_count);
		next = setup_object(s, next);
		set_freepointer(s, cur, next);
		cur = next;
	}
	set_freepointer(s, cur, NULL);

	return true;
}
#else
static inline int init_cache_random_seq(struct kmem_cache *s)
{
	return 0;
}
static inline void init_freelist_randomization(void) { }
static inline bool shuffle_freelist(struct kmem_cache *s, struct slab *slab)
{
	return false;
}
#endif  

static struct slab *allocate_slab(struct kmem_cache *s, gfp_t flags, int node)
{
	struct slab *slab;
	struct kmem_cache_order_objects oo = s->oo;
	gfp_t alloc_gfp;
	void *start, *p, *next;
	int idx;
	bool shuffle;

	flags &= gfp_allowed_mask;

	flags |= s->allocflags;

	 
	alloc_gfp = (flags | __GFP_NOWARN | __GFP_NORETRY) & ~__GFP_NOFAIL;
	if ((alloc_gfp & __GFP_DIRECT_RECLAIM) && oo_order(oo) > oo_order(s->min))
		alloc_gfp = (alloc_gfp | __GFP_NOMEMALLOC) & ~__GFP_RECLAIM;

	slab = alloc_slab_page(alloc_gfp, node, oo);
	if (unlikely(!slab)) {
		oo = s->min;
		alloc_gfp = flags;
		 
		slab = alloc_slab_page(alloc_gfp, node, oo);
		if (unlikely(!slab))
			return NULL;
		stat(s, ORDER_FALLBACK);
	}

	slab->objects = oo_objects(oo);
	slab->inuse = 0;
	slab->frozen = 0;

	account_slab(slab, oo_order(oo), s, flags);

	slab->slab_cache = s;

	kasan_poison_slab(slab);

	start = slab_address(slab);

	setup_slab_debug(s, slab, start);

	shuffle = shuffle_freelist(s, slab);

	if (!shuffle) {
		start = fixup_red_left(s, start);
		start = setup_object(s, start);
		slab->freelist = start;
		for (idx = 0, p = start; idx < slab->objects - 1; idx++) {
			next = p + s->size;
			next = setup_object(s, next);
			set_freepointer(s, p, next);
			p = next;
		}
		set_freepointer(s, p, NULL);
	}

	return slab;
}

static struct slab *new_slab(struct kmem_cache *s, gfp_t flags, int node)
{
	if (unlikely(flags & GFP_SLAB_BUG_MASK))
		flags = kmalloc_fix_flags(flags);

	WARN_ON_ONCE(s->ctor && (flags & __GFP_ZERO));

	return allocate_slab(s,
		flags & (GFP_RECLAIM_MASK | GFP_CONSTRAINT_MASK), node);
}

static void __free_slab(struct kmem_cache *s, struct slab *slab)
{
	struct folio *folio = slab_folio(slab);
	int order = folio_order(folio);
	int pages = 1 << order;

	__slab_clear_pfmemalloc(slab);
	folio->mapping = NULL;
	 
	smp_wmb();
	__folio_clear_slab(folio);
	mm_account_reclaimed_pages(pages);
	unaccount_slab(slab, order, s);
	__free_pages(&folio->page, order);
}

static void rcu_free_slab(struct rcu_head *h)
{
	struct slab *slab = container_of(h, struct slab, rcu_head);

	__free_slab(slab->slab_cache, slab);
}

static void free_slab(struct kmem_cache *s, struct slab *slab)
{
	if (kmem_cache_debug_flags(s, SLAB_CONSISTENCY_CHECKS)) {
		void *p;

		slab_pad_check(s, slab);
		for_each_object(p, s, slab_address(slab), slab->objects)
			check_object(s, slab, p, SLUB_RED_INACTIVE);
	}

	if (unlikely(s->flags & SLAB_TYPESAFE_BY_RCU))
		call_rcu(&slab->rcu_head, rcu_free_slab);
	else
		__free_slab(s, slab);
}

static void discard_slab(struct kmem_cache *s, struct slab *slab)
{
	dec_slabs_node(s, slab_nid(slab), slab->objects);
	free_slab(s, slab);
}

 
static inline void
__add_partial(struct kmem_cache_node *n, struct slab *slab, int tail)
{
	n->nr_partial++;
	if (tail == DEACTIVATE_TO_TAIL)
		list_add_tail(&slab->slab_list, &n->partial);
	else
		list_add(&slab->slab_list, &n->partial);
}

static inline void add_partial(struct kmem_cache_node *n,
				struct slab *slab, int tail)
{
	lockdep_assert_held(&n->list_lock);
	__add_partial(n, slab, tail);
}

static inline void remove_partial(struct kmem_cache_node *n,
					struct slab *slab)
{
	lockdep_assert_held(&n->list_lock);
	list_del(&slab->slab_list);
	n->nr_partial--;
}

 
static void *alloc_single_from_partial(struct kmem_cache *s,
		struct kmem_cache_node *n, struct slab *slab, int orig_size)
{
	void *object;

	lockdep_assert_held(&n->list_lock);

	object = slab->freelist;
	slab->freelist = get_freepointer(s, object);
	slab->inuse++;

	if (!alloc_debug_processing(s, slab, object, orig_size)) {
		remove_partial(n, slab);
		return NULL;
	}

	if (slab->inuse == slab->objects) {
		remove_partial(n, slab);
		add_full(s, n, slab);
	}

	return object;
}

 
static void *alloc_single_from_new_slab(struct kmem_cache *s,
					struct slab *slab, int orig_size)
{
	int nid = slab_nid(slab);
	struct kmem_cache_node *n = get_node(s, nid);
	unsigned long flags;
	void *object;


	object = slab->freelist;
	slab->freelist = get_freepointer(s, object);
	slab->inuse = 1;

	if (!alloc_debug_processing(s, slab, object, orig_size))
		 
		return NULL;

	spin_lock_irqsave(&n->list_lock, flags);

	if (slab->inuse == slab->objects)
		add_full(s, n, slab);
	else
		add_partial(n, slab, DEACTIVATE_TO_HEAD);

	inc_slabs_node(s, nid, slab->objects);
	spin_unlock_irqrestore(&n->list_lock, flags);

	return object;
}

 
static inline void *acquire_slab(struct kmem_cache *s,
		struct kmem_cache_node *n, struct slab *slab,
		int mode)
{
	void *freelist;
	unsigned long counters;
	struct slab new;

	lockdep_assert_held(&n->list_lock);

	 
	freelist = slab->freelist;
	counters = slab->counters;
	new.counters = counters;
	if (mode) {
		new.inuse = slab->objects;
		new.freelist = NULL;
	} else {
		new.freelist = freelist;
	}

	VM_BUG_ON(new.frozen);
	new.frozen = 1;

	if (!__slab_update_freelist(s, slab,
			freelist, counters,
			new.freelist, new.counters,
			"acquire_slab"))
		return NULL;

	remove_partial(n, slab);
	WARN_ON(!freelist);
	return freelist;
}

#ifdef CONFIG_SLUB_CPU_PARTIAL
static void put_cpu_partial(struct kmem_cache *s, struct slab *slab, int drain);
#else
static inline void put_cpu_partial(struct kmem_cache *s, struct slab *slab,
				   int drain) { }
#endif
static inline bool pfmemalloc_match(struct slab *slab, gfp_t gfpflags);

 
static void *get_partial_node(struct kmem_cache *s, struct kmem_cache_node *n,
			      struct partial_context *pc)
{
	struct slab *slab, *slab2;
	void *object = NULL;
	unsigned long flags;
	unsigned int partial_slabs = 0;

	 
	if (!n || !n->nr_partial)
		return NULL;

	spin_lock_irqsave(&n->list_lock, flags);
	list_for_each_entry_safe(slab, slab2, &n->partial, slab_list) {
		void *t;

		if (!pfmemalloc_match(slab, pc->flags))
			continue;

		if (IS_ENABLED(CONFIG_SLUB_TINY) || kmem_cache_debug(s)) {
			object = alloc_single_from_partial(s, n, slab,
							pc->orig_size);
			if (object)
				break;
			continue;
		}

		t = acquire_slab(s, n, slab, object == NULL);
		if (!t)
			break;

		if (!object) {
			*pc->slab = slab;
			stat(s, ALLOC_FROM_PARTIAL);
			object = t;
		} else {
			put_cpu_partial(s, slab, 0);
			stat(s, CPU_PARTIAL_NODE);
			partial_slabs++;
		}
#ifdef CONFIG_SLUB_CPU_PARTIAL
		if (!kmem_cache_has_cpu_partial(s)
			|| partial_slabs > s->cpu_partial_slabs / 2)
			break;
#else
		break;
#endif

	}
	spin_unlock_irqrestore(&n->list_lock, flags);
	return object;
}

 
static void *get_any_partial(struct kmem_cache *s, struct partial_context *pc)
{
#ifdef CONFIG_NUMA
	struct zonelist *zonelist;
	struct zoneref *z;
	struct zone *zone;
	enum zone_type highest_zoneidx = gfp_zone(pc->flags);
	void *object;
	unsigned int cpuset_mems_cookie;

	 
	if (!s->remote_node_defrag_ratio ||
			get_cycles() % 1024 > s->remote_node_defrag_ratio)
		return NULL;

	do {
		cpuset_mems_cookie = read_mems_allowed_begin();
		zonelist = node_zonelist(mempolicy_slab_node(), pc->flags);
		for_each_zone_zonelist(zone, z, zonelist, highest_zoneidx) {
			struct kmem_cache_node *n;

			n = get_node(s, zone_to_nid(zone));

			if (n && cpuset_zone_allowed(zone, pc->flags) &&
					n->nr_partial > s->min_partial) {
				object = get_partial_node(s, n, pc);
				if (object) {
					 
					return object;
				}
			}
		}
	} while (read_mems_allowed_retry(cpuset_mems_cookie));
#endif	 
	return NULL;
}

 
static void *get_partial(struct kmem_cache *s, int node, struct partial_context *pc)
{
	void *object;
	int searchnode = node;

	if (node == NUMA_NO_NODE)
		searchnode = numa_mem_id();

	object = get_partial_node(s, get_node(s, searchnode), pc);
	if (object || node != NUMA_NO_NODE)
		return object;

	return get_any_partial(s, pc);
}

#ifndef CONFIG_SLUB_TINY

#ifdef CONFIG_PREEMPTION
 
#define TID_STEP  roundup_pow_of_two(CONFIG_NR_CPUS)
#else
 
#define TID_STEP 1
#endif  

static inline unsigned long next_tid(unsigned long tid)
{
	return tid + TID_STEP;
}

#ifdef SLUB_DEBUG_CMPXCHG
static inline unsigned int tid_to_cpu(unsigned long tid)
{
	return tid % TID_STEP;
}

static inline unsigned long tid_to_event(unsigned long tid)
{
	return tid / TID_STEP;
}
#endif

static inline unsigned int init_tid(int cpu)
{
	return cpu;
}

static inline void note_cmpxchg_failure(const char *n,
		const struct kmem_cache *s, unsigned long tid)
{
#ifdef SLUB_DEBUG_CMPXCHG
	unsigned long actual_tid = __this_cpu_read(s->cpu_slab->tid);

	pr_info("%s %s: cmpxchg redo ", n, s->name);

#ifdef CONFIG_PREEMPTION
	if (tid_to_cpu(tid) != tid_to_cpu(actual_tid))
		pr_warn("due to cpu change %d -> %d\n",
			tid_to_cpu(tid), tid_to_cpu(actual_tid));
	else
#endif
	if (tid_to_event(tid) != tid_to_event(actual_tid))
		pr_warn("due to cpu running other code. Event %ld->%ld\n",
			tid_to_event(tid), tid_to_event(actual_tid));
	else
		pr_warn("for unknown reason: actual=%lx was=%lx target=%lx\n",
			actual_tid, tid, next_tid(tid));
#endif
	stat(s, CMPXCHG_DOUBLE_CPU_FAIL);
}

static void init_kmem_cache_cpus(struct kmem_cache *s)
{
	int cpu;
	struct kmem_cache_cpu *c;

	for_each_possible_cpu(cpu) {
		c = per_cpu_ptr(s->cpu_slab, cpu);
		local_lock_init(&c->lock);
		c->tid = init_tid(cpu);
	}
}

 
static void deactivate_slab(struct kmem_cache *s, struct slab *slab,
			    void *freelist)
{
	enum slab_modes { M_NONE, M_PARTIAL, M_FREE, M_FULL_NOLIST };
	struct kmem_cache_node *n = get_node(s, slab_nid(slab));
	int free_delta = 0;
	enum slab_modes mode = M_NONE;
	void *nextfree, *freelist_iter, *freelist_tail;
	int tail = DEACTIVATE_TO_HEAD;
	unsigned long flags = 0;
	struct slab new;
	struct slab old;

	if (slab->freelist) {
		stat(s, DEACTIVATE_REMOTE_FREES);
		tail = DEACTIVATE_TO_TAIL;
	}

	 
	freelist_tail = NULL;
	freelist_iter = freelist;
	while (freelist_iter) {
		nextfree = get_freepointer(s, freelist_iter);

		 
		if (freelist_corrupted(s, slab, &freelist_iter, nextfree))
			break;

		freelist_tail = freelist_iter;
		free_delta++;

		freelist_iter = nextfree;
	}

	 
redo:

	old.freelist = READ_ONCE(slab->freelist);
	old.counters = READ_ONCE(slab->counters);
	VM_BUG_ON(!old.frozen);

	 
	new.counters = old.counters;
	if (freelist_tail) {
		new.inuse -= free_delta;
		set_freepointer(s, freelist_tail, old.freelist);
		new.freelist = freelist;
	} else
		new.freelist = old.freelist;

	new.frozen = 0;

	if (!new.inuse && n->nr_partial >= s->min_partial) {
		mode = M_FREE;
	} else if (new.freelist) {
		mode = M_PARTIAL;
		 
		spin_lock_irqsave(&n->list_lock, flags);
	} else {
		mode = M_FULL_NOLIST;
	}


	if (!slab_update_freelist(s, slab,
				old.freelist, old.counters,
				new.freelist, new.counters,
				"unfreezing slab")) {
		if (mode == M_PARTIAL)
			spin_unlock_irqrestore(&n->list_lock, flags);
		goto redo;
	}


	if (mode == M_PARTIAL) {
		add_partial(n, slab, tail);
		spin_unlock_irqrestore(&n->list_lock, flags);
		stat(s, tail);
	} else if (mode == M_FREE) {
		stat(s, DEACTIVATE_EMPTY);
		discard_slab(s, slab);
		stat(s, FREE_SLAB);
	} else if (mode == M_FULL_NOLIST) {
		stat(s, DEACTIVATE_FULL);
	}
}

#ifdef CONFIG_SLUB_CPU_PARTIAL
static void __unfreeze_partials(struct kmem_cache *s, struct slab *partial_slab)
{
	struct kmem_cache_node *n = NULL, *n2 = NULL;
	struct slab *slab, *slab_to_discard = NULL;
	unsigned long flags = 0;

	while (partial_slab) {
		struct slab new;
		struct slab old;

		slab = partial_slab;
		partial_slab = slab->next;

		n2 = get_node(s, slab_nid(slab));
		if (n != n2) {
			if (n)
				spin_unlock_irqrestore(&n->list_lock, flags);

			n = n2;
			spin_lock_irqsave(&n->list_lock, flags);
		}

		do {

			old.freelist = slab->freelist;
			old.counters = slab->counters;
			VM_BUG_ON(!old.frozen);

			new.counters = old.counters;
			new.freelist = old.freelist;

			new.frozen = 0;

		} while (!__slab_update_freelist(s, slab,
				old.freelist, old.counters,
				new.freelist, new.counters,
				"unfreezing slab"));

		if (unlikely(!new.inuse && n->nr_partial >= s->min_partial)) {
			slab->next = slab_to_discard;
			slab_to_discard = slab;
		} else {
			add_partial(n, slab, DEACTIVATE_TO_TAIL);
			stat(s, FREE_ADD_PARTIAL);
		}
	}

	if (n)
		spin_unlock_irqrestore(&n->list_lock, flags);

	while (slab_to_discard) {
		slab = slab_to_discard;
		slab_to_discard = slab_to_discard->next;

		stat(s, DEACTIVATE_EMPTY);
		discard_slab(s, slab);
		stat(s, FREE_SLAB);
	}
}

 
static void unfreeze_partials(struct kmem_cache *s)
{
	struct slab *partial_slab;
	unsigned long flags;

	local_lock_irqsave(&s->cpu_slab->lock, flags);
	partial_slab = this_cpu_read(s->cpu_slab->partial);
	this_cpu_write(s->cpu_slab->partial, NULL);
	local_unlock_irqrestore(&s->cpu_slab->lock, flags);

	if (partial_slab)
		__unfreeze_partials(s, partial_slab);
}

static void unfreeze_partials_cpu(struct kmem_cache *s,
				  struct kmem_cache_cpu *c)
{
	struct slab *partial_slab;

	partial_slab = slub_percpu_partial(c);
	c->partial = NULL;

	if (partial_slab)
		__unfreeze_partials(s, partial_slab);
}

 
static void put_cpu_partial(struct kmem_cache *s, struct slab *slab, int drain)
{
	struct slab *oldslab;
	struct slab *slab_to_unfreeze = NULL;
	unsigned long flags;
	int slabs = 0;

	local_lock_irqsave(&s->cpu_slab->lock, flags);

	oldslab = this_cpu_read(s->cpu_slab->partial);

	if (oldslab) {
		if (drain && oldslab->slabs >= s->cpu_partial_slabs) {
			 
			slab_to_unfreeze = oldslab;
			oldslab = NULL;
		} else {
			slabs = oldslab->slabs;
		}
	}

	slabs++;

	slab->slabs = slabs;
	slab->next = oldslab;

	this_cpu_write(s->cpu_slab->partial, slab);

	local_unlock_irqrestore(&s->cpu_slab->lock, flags);

	if (slab_to_unfreeze) {
		__unfreeze_partials(s, slab_to_unfreeze);
		stat(s, CPU_PARTIAL_DRAIN);
	}
}

#else	 

static inline void unfreeze_partials(struct kmem_cache *s) { }
static inline void unfreeze_partials_cpu(struct kmem_cache *s,
				  struct kmem_cache_cpu *c) { }

#endif	 

static inline void flush_slab(struct kmem_cache *s, struct kmem_cache_cpu *c)
{
	unsigned long flags;
	struct slab *slab;
	void *freelist;

	local_lock_irqsave(&s->cpu_slab->lock, flags);

	slab = c->slab;
	freelist = c->freelist;

	c->slab = NULL;
	c->freelist = NULL;
	c->tid = next_tid(c->tid);

	local_unlock_irqrestore(&s->cpu_slab->lock, flags);

	if (slab) {
		deactivate_slab(s, slab, freelist);
		stat(s, CPUSLAB_FLUSH);
	}
}

static inline void __flush_cpu_slab(struct kmem_cache *s, int cpu)
{
	struct kmem_cache_cpu *c = per_cpu_ptr(s->cpu_slab, cpu);
	void *freelist = c->freelist;
	struct slab *slab = c->slab;

	c->slab = NULL;
	c->freelist = NULL;
	c->tid = next_tid(c->tid);

	if (slab) {
		deactivate_slab(s, slab, freelist);
		stat(s, CPUSLAB_FLUSH);
	}

	unfreeze_partials_cpu(s, c);
}

struct slub_flush_work {
	struct work_struct work;
	struct kmem_cache *s;
	bool skip;
};

 
static void flush_cpu_slab(struct work_struct *w)
{
	struct kmem_cache *s;
	struct kmem_cache_cpu *c;
	struct slub_flush_work *sfw;

	sfw = container_of(w, struct slub_flush_work, work);

	s = sfw->s;
	c = this_cpu_ptr(s->cpu_slab);

	if (c->slab)
		flush_slab(s, c);

	unfreeze_partials(s);
}

static bool has_cpu_slab(int cpu, struct kmem_cache *s)
{
	struct kmem_cache_cpu *c = per_cpu_ptr(s->cpu_slab, cpu);

	return c->slab || slub_percpu_partial(c);
}

static DEFINE_MUTEX(flush_lock);
static DEFINE_PER_CPU(struct slub_flush_work, slub_flush);

static void flush_all_cpus_locked(struct kmem_cache *s)
{
	struct slub_flush_work *sfw;
	unsigned int cpu;

	lockdep_assert_cpus_held();
	mutex_lock(&flush_lock);

	for_each_online_cpu(cpu) {
		sfw = &per_cpu(slub_flush, cpu);
		if (!has_cpu_slab(cpu, s)) {
			sfw->skip = true;
			continue;
		}
		INIT_WORK(&sfw->work, flush_cpu_slab);
		sfw->skip = false;
		sfw->s = s;
		queue_work_on(cpu, flushwq, &sfw->work);
	}

	for_each_online_cpu(cpu) {
		sfw = &per_cpu(slub_flush, cpu);
		if (sfw->skip)
			continue;
		flush_work(&sfw->work);
	}

	mutex_unlock(&flush_lock);
}

static void flush_all(struct kmem_cache *s)
{
	cpus_read_lock();
	flush_all_cpus_locked(s);
	cpus_read_unlock();
}

 
static int slub_cpu_dead(unsigned int cpu)
{
	struct kmem_cache *s;

	mutex_lock(&slab_mutex);
	list_for_each_entry(s, &slab_caches, list)
		__flush_cpu_slab(s, cpu);
	mutex_unlock(&slab_mutex);
	return 0;
}

#else  
static inline void flush_all_cpus_locked(struct kmem_cache *s) { }
static inline void flush_all(struct kmem_cache *s) { }
static inline void __flush_cpu_slab(struct kmem_cache *s, int cpu) { }
static inline int slub_cpu_dead(unsigned int cpu) { return 0; }
#endif  

 
static inline int node_match(struct slab *slab, int node)
{
#ifdef CONFIG_NUMA
	if (node != NUMA_NO_NODE && slab_nid(slab) != node)
		return 0;
#endif
	return 1;
}

#ifdef CONFIG_SLUB_DEBUG
static int count_free(struct slab *slab)
{
	return slab->objects - slab->inuse;
}

static inline unsigned long node_nr_objs(struct kmem_cache_node *n)
{
	return atomic_long_read(&n->total_objects);
}

 
static inline bool free_debug_processing(struct kmem_cache *s,
	struct slab *slab, void *head, void *tail, int *bulk_cnt,
	unsigned long addr, depot_stack_handle_t handle)
{
	bool checks_ok = false;
	void *object = head;
	int cnt = 0;

	if (s->flags & SLAB_CONSISTENCY_CHECKS) {
		if (!check_slab(s, slab))
			goto out;
	}

	if (slab->inuse < *bulk_cnt) {
		slab_err(s, slab, "Slab has %d allocated objects but %d are to be freed\n",
			 slab->inuse, *bulk_cnt);
		goto out;
	}

next_object:

	if (++cnt > *bulk_cnt)
		goto out_cnt;

	if (s->flags & SLAB_CONSISTENCY_CHECKS) {
		if (!free_consistency_checks(s, slab, object, addr))
			goto out;
	}

	if (s->flags & SLAB_STORE_USER)
		set_track_update(s, object, TRACK_FREE, addr, handle);
	trace(s, slab, object, 0);
	 
	init_object(s, object, SLUB_RED_INACTIVE);

	 
	if (object != tail) {
		object = get_freepointer(s, object);
		goto next_object;
	}
	checks_ok = true;

out_cnt:
	if (cnt != *bulk_cnt) {
		slab_err(s, slab, "Bulk free expected %d objects but found %d\n",
			 *bulk_cnt, cnt);
		*bulk_cnt = cnt;
	}

out:

	if (!checks_ok)
		slab_fix(s, "Object at 0x%p not freed", object);

	return checks_ok;
}
#endif  

#if defined(CONFIG_SLUB_DEBUG) || defined(SLAB_SUPPORTS_SYSFS)
static unsigned long count_partial(struct kmem_cache_node *n,
					int (*get_count)(struct slab *))
{
	unsigned long flags;
	unsigned long x = 0;
	struct slab *slab;

	spin_lock_irqsave(&n->list_lock, flags);
	list_for_each_entry(slab, &n->partial, slab_list)
		x += get_count(slab);
	spin_unlock_irqrestore(&n->list_lock, flags);
	return x;
}
#endif  

#ifdef CONFIG_SLUB_DEBUG
static noinline void
slab_out_of_memory(struct kmem_cache *s, gfp_t gfpflags, int nid)
{
	static DEFINE_RATELIMIT_STATE(slub_oom_rs, DEFAULT_RATELIMIT_INTERVAL,
				      DEFAULT_RATELIMIT_BURST);
	int node;
	struct kmem_cache_node *n;

	if ((gfpflags & __GFP_NOWARN) || !__ratelimit(&slub_oom_rs))
		return;

	pr_warn("SLUB: Unable to allocate memory on node %d, gfp=%#x(%pGg)\n",
		nid, gfpflags, &gfpflags);
	pr_warn("  cache: %s, object size: %u, buffer size: %u, default order: %u, min order: %u\n",
		s->name, s->object_size, s->size, oo_order(s->oo),
		oo_order(s->min));

	if (oo_order(s->min) > get_order(s->object_size))
		pr_warn("  %s debugging increased min order, use slub_debug=O to disable.\n",
			s->name);

	for_each_kmem_cache_node(s, node, n) {
		unsigned long nr_slabs;
		unsigned long nr_objs;
		unsigned long nr_free;

		nr_free  = count_partial(n, count_free);
		nr_slabs = node_nr_slabs(n);
		nr_objs  = node_nr_objs(n);

		pr_warn("  node %d: slabs: %ld, objs: %ld, free: %ld\n",
			node, nr_slabs, nr_objs, nr_free);
	}
}
#else  
static inline void
slab_out_of_memory(struct kmem_cache *s, gfp_t gfpflags, int nid) { }
#endif

static inline bool pfmemalloc_match(struct slab *slab, gfp_t gfpflags)
{
	if (unlikely(slab_test_pfmemalloc(slab)))
		return gfp_pfmemalloc_allowed(gfpflags);

	return true;
}

#ifndef CONFIG_SLUB_TINY
static inline bool
__update_cpu_freelist_fast(struct kmem_cache *s,
			   void *freelist_old, void *freelist_new,
			   unsigned long tid)
{
	freelist_aba_t old = { .freelist = freelist_old, .counter = tid };
	freelist_aba_t new = { .freelist = freelist_new, .counter = next_tid(tid) };

	return this_cpu_try_cmpxchg_freelist(s->cpu_slab->freelist_tid.full,
					     &old.full, new.full);
}

 
static inline void *get_freelist(struct kmem_cache *s, struct slab *slab)
{
	struct slab new;
	unsigned long counters;
	void *freelist;

	lockdep_assert_held(this_cpu_ptr(&s->cpu_slab->lock));

	do {
		freelist = slab->freelist;
		counters = slab->counters;

		new.counters = counters;
		VM_BUG_ON(!new.frozen);

		new.inuse = slab->objects;
		new.frozen = freelist != NULL;

	} while (!__slab_update_freelist(s, slab,
		freelist, counters,
		NULL, new.counters,
		"get_freelist"));

	return freelist;
}

 
static void *___slab_alloc(struct kmem_cache *s, gfp_t gfpflags, int node,
			  unsigned long addr, struct kmem_cache_cpu *c, unsigned int orig_size)
{
	void *freelist;
	struct slab *slab;
	unsigned long flags;
	struct partial_context pc;

	stat(s, ALLOC_SLOWPATH);

reread_slab:

	slab = READ_ONCE(c->slab);
	if (!slab) {
		 
		if (unlikely(node != NUMA_NO_NODE &&
			     !node_isset(node, slab_nodes)))
			node = NUMA_NO_NODE;
		goto new_slab;
	}
redo:

	if (unlikely(!node_match(slab, node))) {
		 
		if (!node_isset(node, slab_nodes)) {
			node = NUMA_NO_NODE;
		} else {
			stat(s, ALLOC_NODE_MISMATCH);
			goto deactivate_slab;
		}
	}

	 
	if (unlikely(!pfmemalloc_match(slab, gfpflags)))
		goto deactivate_slab;

	 
	local_lock_irqsave(&s->cpu_slab->lock, flags);
	if (unlikely(slab != c->slab)) {
		local_unlock_irqrestore(&s->cpu_slab->lock, flags);
		goto reread_slab;
	}
	freelist = c->freelist;
	if (freelist)
		goto load_freelist;

	freelist = get_freelist(s, slab);

	if (!freelist) {
		c->slab = NULL;
		c->tid = next_tid(c->tid);
		local_unlock_irqrestore(&s->cpu_slab->lock, flags);
		stat(s, DEACTIVATE_BYPASS);
		goto new_slab;
	}

	stat(s, ALLOC_REFILL);

load_freelist:

	lockdep_assert_held(this_cpu_ptr(&s->cpu_slab->lock));

	 
	VM_BUG_ON(!c->slab->frozen);
	c->freelist = get_freepointer(s, freelist);
	c->tid = next_tid(c->tid);
	local_unlock_irqrestore(&s->cpu_slab->lock, flags);
	return freelist;

deactivate_slab:

	local_lock_irqsave(&s->cpu_slab->lock, flags);
	if (slab != c->slab) {
		local_unlock_irqrestore(&s->cpu_slab->lock, flags);
		goto reread_slab;
	}
	freelist = c->freelist;
	c->slab = NULL;
	c->freelist = NULL;
	c->tid = next_tid(c->tid);
	local_unlock_irqrestore(&s->cpu_slab->lock, flags);
	deactivate_slab(s, slab, freelist);

new_slab:

	if (slub_percpu_partial(c)) {
		local_lock_irqsave(&s->cpu_slab->lock, flags);
		if (unlikely(c->slab)) {
			local_unlock_irqrestore(&s->cpu_slab->lock, flags);
			goto reread_slab;
		}
		if (unlikely(!slub_percpu_partial(c))) {
			local_unlock_irqrestore(&s->cpu_slab->lock, flags);
			 
			goto new_objects;
		}

		slab = c->slab = slub_percpu_partial(c);
		slub_set_percpu_partial(c, slab);
		local_unlock_irqrestore(&s->cpu_slab->lock, flags);
		stat(s, CPU_PARTIAL_ALLOC);
		goto redo;
	}

new_objects:

	pc.flags = gfpflags;
	pc.slab = &slab;
	pc.orig_size = orig_size;
	freelist = get_partial(s, node, &pc);
	if (freelist)
		goto check_new_slab;

	slub_put_cpu_ptr(s->cpu_slab);
	slab = new_slab(s, gfpflags, node);
	c = slub_get_cpu_ptr(s->cpu_slab);

	if (unlikely(!slab)) {
		slab_out_of_memory(s, gfpflags, node);
		return NULL;
	}

	stat(s, ALLOC_SLAB);

	if (kmem_cache_debug(s)) {
		freelist = alloc_single_from_new_slab(s, slab, orig_size);

		if (unlikely(!freelist))
			goto new_objects;

		if (s->flags & SLAB_STORE_USER)
			set_track(s, freelist, TRACK_ALLOC, addr);

		return freelist;
	}

	 
	freelist = slab->freelist;
	slab->freelist = NULL;
	slab->inuse = slab->objects;
	slab->frozen = 1;

	inc_slabs_node(s, slab_nid(slab), slab->objects);

check_new_slab:

	if (kmem_cache_debug(s)) {
		 
		if (s->flags & SLAB_STORE_USER)
			set_track(s, freelist, TRACK_ALLOC, addr);

		return freelist;
	}

	if (unlikely(!pfmemalloc_match(slab, gfpflags))) {
		 
		deactivate_slab(s, slab, get_freepointer(s, freelist));
		return freelist;
	}

retry_load_slab:

	local_lock_irqsave(&s->cpu_slab->lock, flags);
	if (unlikely(c->slab)) {
		void *flush_freelist = c->freelist;
		struct slab *flush_slab = c->slab;

		c->slab = NULL;
		c->freelist = NULL;
		c->tid = next_tid(c->tid);

		local_unlock_irqrestore(&s->cpu_slab->lock, flags);

		deactivate_slab(s, flush_slab, flush_freelist);

		stat(s, CPUSLAB_FLUSH);

		goto retry_load_slab;
	}
	c->slab = slab;

	goto load_freelist;
}

 
static void *__slab_alloc(struct kmem_cache *s, gfp_t gfpflags, int node,
			  unsigned long addr, struct kmem_cache_cpu *c, unsigned int orig_size)
{
	void *p;

#ifdef CONFIG_PREEMPT_COUNT
	 
	c = slub_get_cpu_ptr(s->cpu_slab);
#endif

	p = ___slab_alloc(s, gfpflags, node, addr, c, orig_size);
#ifdef CONFIG_PREEMPT_COUNT
	slub_put_cpu_ptr(s->cpu_slab);
#endif
	return p;
}

static __always_inline void *__slab_alloc_node(struct kmem_cache *s,
		gfp_t gfpflags, int node, unsigned long addr, size_t orig_size)
{
	struct kmem_cache_cpu *c;
	struct slab *slab;
	unsigned long tid;
	void *object;

redo:
	 
	c = raw_cpu_ptr(s->cpu_slab);
	tid = READ_ONCE(c->tid);

	 
	barrier();

	 

	object = c->freelist;
	slab = c->slab;

	if (!USE_LOCKLESS_FAST_PATH() ||
	    unlikely(!object || !slab || !node_match(slab, node))) {
		object = __slab_alloc(s, gfpflags, node, addr, c, orig_size);
	} else {
		void *next_object = get_freepointer_safe(s, object);

		 
		if (unlikely(!__update_cpu_freelist_fast(s, object, next_object, tid))) {
			note_cmpxchg_failure("slab_alloc", s, tid);
			goto redo;
		}
		prefetch_freepointer(s, next_object);
		stat(s, ALLOC_FASTPATH);
	}

	return object;
}
#else  
static void *__slab_alloc_node(struct kmem_cache *s,
		gfp_t gfpflags, int node, unsigned long addr, size_t orig_size)
{
	struct partial_context pc;
	struct slab *slab;
	void *object;

	pc.flags = gfpflags;
	pc.slab = &slab;
	pc.orig_size = orig_size;
	object = get_partial(s, node, &pc);

	if (object)
		return object;

	slab = new_slab(s, gfpflags, node);
	if (unlikely(!slab)) {
		slab_out_of_memory(s, gfpflags, node);
		return NULL;
	}

	object = alloc_single_from_new_slab(s, slab, orig_size);

	return object;
}
#endif  

 
static __always_inline void maybe_wipe_obj_freeptr(struct kmem_cache *s,
						   void *obj)
{
	if (unlikely(slab_want_init_on_free(s)) && obj)
		memset((void *)((char *)kasan_reset_tag(obj) + s->offset),
			0, sizeof(void *));
}

 
static __fastpath_inline void *slab_alloc_node(struct kmem_cache *s, struct list_lru *lru,
		gfp_t gfpflags, int node, unsigned long addr, size_t orig_size)
{
	void *object;
	struct obj_cgroup *objcg = NULL;
	bool init = false;

	s = slab_pre_alloc_hook(s, lru, &objcg, 1, gfpflags);
	if (!s)
		return NULL;

	object = kfence_alloc(s, orig_size, gfpflags);
	if (unlikely(object))
		goto out;

	object = __slab_alloc_node(s, gfpflags, node, addr, orig_size);

	maybe_wipe_obj_freeptr(s, object);
	init = slab_want_init_on_alloc(gfpflags, s);

out:
	 
	slab_post_alloc_hook(s, objcg, gfpflags, 1, &object, init, orig_size);

	return object;
}

static __fastpath_inline void *slab_alloc(struct kmem_cache *s, struct list_lru *lru,
		gfp_t gfpflags, unsigned long addr, size_t orig_size)
{
	return slab_alloc_node(s, lru, gfpflags, NUMA_NO_NODE, addr, orig_size);
}

static __fastpath_inline
void *__kmem_cache_alloc_lru(struct kmem_cache *s, struct list_lru *lru,
			     gfp_t gfpflags)
{
	void *ret = slab_alloc(s, lru, gfpflags, _RET_IP_, s->object_size);

	trace_kmem_cache_alloc(_RET_IP_, ret, s, gfpflags, NUMA_NO_NODE);

	return ret;
}

void *kmem_cache_alloc(struct kmem_cache *s, gfp_t gfpflags)
{
	return __kmem_cache_alloc_lru(s, NULL, gfpflags);
}
EXPORT_SYMBOL(kmem_cache_alloc);

void *kmem_cache_alloc_lru(struct kmem_cache *s, struct list_lru *lru,
			   gfp_t gfpflags)
{
	return __kmem_cache_alloc_lru(s, lru, gfpflags);
}
EXPORT_SYMBOL(kmem_cache_alloc_lru);

void *__kmem_cache_alloc_node(struct kmem_cache *s, gfp_t gfpflags,
			      int node, size_t orig_size,
			      unsigned long caller)
{
	return slab_alloc_node(s, NULL, gfpflags, node,
			       caller, orig_size);
}

void *kmem_cache_alloc_node(struct kmem_cache *s, gfp_t gfpflags, int node)
{
	void *ret = slab_alloc_node(s, NULL, gfpflags, node, _RET_IP_, s->object_size);

	trace_kmem_cache_alloc(_RET_IP_, ret, s, gfpflags, node);

	return ret;
}
EXPORT_SYMBOL(kmem_cache_alloc_node);

static noinline void free_to_partial_list(
	struct kmem_cache *s, struct slab *slab,
	void *head, void *tail, int bulk_cnt,
	unsigned long addr)
{
	struct kmem_cache_node *n = get_node(s, slab_nid(slab));
	struct slab *slab_free = NULL;
	int cnt = bulk_cnt;
	unsigned long flags;
	depot_stack_handle_t handle = 0;

	if (s->flags & SLAB_STORE_USER)
		handle = set_track_prepare();

	spin_lock_irqsave(&n->list_lock, flags);

	if (free_debug_processing(s, slab, head, tail, &cnt, addr, handle)) {
		void *prior = slab->freelist;

		 
		slab->inuse -= cnt;
		set_freepointer(s, tail, prior);
		slab->freelist = head;

		 
		if (slab->inuse == 0 && n->nr_partial >= s->min_partial)
			slab_free = slab;

		if (!prior) {
			 
			remove_full(s, n, slab);
			if (!slab_free) {
				add_partial(n, slab, DEACTIVATE_TO_TAIL);
				stat(s, FREE_ADD_PARTIAL);
			}
		} else if (slab_free) {
			remove_partial(n, slab);
			stat(s, FREE_REMOVE_PARTIAL);
		}
	}

	if (slab_free) {
		 
		dec_slabs_node(s, slab_nid(slab_free), slab_free->objects);
	}

	spin_unlock_irqrestore(&n->list_lock, flags);

	if (slab_free) {
		stat(s, FREE_SLAB);
		free_slab(s, slab_free);
	}
}

 
static void __slab_free(struct kmem_cache *s, struct slab *slab,
			void *head, void *tail, int cnt,
			unsigned long addr)

{
	void *prior;
	int was_frozen;
	struct slab new;
	unsigned long counters;
	struct kmem_cache_node *n = NULL;
	unsigned long flags;

	stat(s, FREE_SLOWPATH);

	if (kfence_free(head))
		return;

	if (IS_ENABLED(CONFIG_SLUB_TINY) || kmem_cache_debug(s)) {
		free_to_partial_list(s, slab, head, tail, cnt, addr);
		return;
	}

	do {
		if (unlikely(n)) {
			spin_unlock_irqrestore(&n->list_lock, flags);
			n = NULL;
		}
		prior = slab->freelist;
		counters = slab->counters;
		set_freepointer(s, tail, prior);
		new.counters = counters;
		was_frozen = new.frozen;
		new.inuse -= cnt;
		if ((!new.inuse || !prior) && !was_frozen) {

			if (kmem_cache_has_cpu_partial(s) && !prior) {

				 
				new.frozen = 1;

			} else {  

				n = get_node(s, slab_nid(slab));
				 
				spin_lock_irqsave(&n->list_lock, flags);

			}
		}

	} while (!slab_update_freelist(s, slab,
		prior, counters,
		head, new.counters,
		"__slab_free"));

	if (likely(!n)) {

		if (likely(was_frozen)) {
			 
			stat(s, FREE_FROZEN);
		} else if (new.frozen) {
			 
			put_cpu_partial(s, slab, 1);
			stat(s, CPU_PARTIAL_FREE);
		}

		return;
	}

	if (unlikely(!new.inuse && n->nr_partial >= s->min_partial))
		goto slab_empty;

	 
	if (!kmem_cache_has_cpu_partial(s) && unlikely(!prior)) {
		remove_full(s, n, slab);
		add_partial(n, slab, DEACTIVATE_TO_TAIL);
		stat(s, FREE_ADD_PARTIAL);
	}
	spin_unlock_irqrestore(&n->list_lock, flags);
	return;

slab_empty:
	if (prior) {
		 
		remove_partial(n, slab);
		stat(s, FREE_REMOVE_PARTIAL);
	} else {
		 
		remove_full(s, n, slab);
	}

	spin_unlock_irqrestore(&n->list_lock, flags);
	stat(s, FREE_SLAB);
	discard_slab(s, slab);
}

#ifndef CONFIG_SLUB_TINY
 
static __always_inline void do_slab_free(struct kmem_cache *s,
				struct slab *slab, void *head, void *tail,
				int cnt, unsigned long addr)
{
	void *tail_obj = tail ? : head;
	struct kmem_cache_cpu *c;
	unsigned long tid;
	void **freelist;

redo:
	 
	c = raw_cpu_ptr(s->cpu_slab);
	tid = READ_ONCE(c->tid);

	 
	barrier();

	if (unlikely(slab != c->slab)) {
		__slab_free(s, slab, head, tail_obj, cnt, addr);
		return;
	}

	if (USE_LOCKLESS_FAST_PATH()) {
		freelist = READ_ONCE(c->freelist);

		set_freepointer(s, tail_obj, freelist);

		if (unlikely(!__update_cpu_freelist_fast(s, freelist, head, tid))) {
			note_cmpxchg_failure("slab_free", s, tid);
			goto redo;
		}
	} else {
		 
		local_lock(&s->cpu_slab->lock);
		c = this_cpu_ptr(s->cpu_slab);
		if (unlikely(slab != c->slab)) {
			local_unlock(&s->cpu_slab->lock);
			goto redo;
		}
		tid = c->tid;
		freelist = c->freelist;

		set_freepointer(s, tail_obj, freelist);
		c->freelist = head;
		c->tid = next_tid(tid);

		local_unlock(&s->cpu_slab->lock);
	}
	stat(s, FREE_FASTPATH);
}
#else  
static void do_slab_free(struct kmem_cache *s,
				struct slab *slab, void *head, void *tail,
				int cnt, unsigned long addr)
{
	void *tail_obj = tail ? : head;

	__slab_free(s, slab, head, tail_obj, cnt, addr);
}
#endif  

static __fastpath_inline void slab_free(struct kmem_cache *s, struct slab *slab,
				      void *head, void *tail, void **p, int cnt,
				      unsigned long addr)
{
	memcg_slab_free_hook(s, slab, p, cnt);
	 
	if (slab_free_freelist_hook(s, &head, &tail, &cnt))
		do_slab_free(s, slab, head, tail, cnt, addr);
}

#ifdef CONFIG_KASAN_GENERIC
void ___cache_free(struct kmem_cache *cache, void *x, unsigned long addr)
{
	do_slab_free(cache, virt_to_slab(x), x, NULL, 1, addr);
}
#endif

void __kmem_cache_free(struct kmem_cache *s, void *x, unsigned long caller)
{
	slab_free(s, virt_to_slab(x), x, NULL, &x, 1, caller);
}

void kmem_cache_free(struct kmem_cache *s, void *x)
{
	s = cache_from_obj(s, x);
	if (!s)
		return;
	trace_kmem_cache_free(_RET_IP_, x, s);
	slab_free(s, virt_to_slab(x), x, NULL, &x, 1, _RET_IP_);
}
EXPORT_SYMBOL(kmem_cache_free);

struct detached_freelist {
	struct slab *slab;
	void *tail;
	void *freelist;
	int cnt;
	struct kmem_cache *s;
};

 
static inline
int build_detached_freelist(struct kmem_cache *s, size_t size,
			    void **p, struct detached_freelist *df)
{
	int lookahead = 3;
	void *object;
	struct folio *folio;
	size_t same;

	object = p[--size];
	folio = virt_to_folio(object);
	if (!s) {
		 
		if (unlikely(!folio_test_slab(folio))) {
			free_large_kmalloc(folio, object);
			df->slab = NULL;
			return size;
		}
		 
		df->slab = folio_slab(folio);
		df->s = df->slab->slab_cache;
	} else {
		df->slab = folio_slab(folio);
		df->s = cache_from_obj(s, object);  
	}

	 
	df->tail = object;
	df->freelist = object;
	df->cnt = 1;

	if (is_kfence_address(object))
		return size;

	set_freepointer(df->s, object, NULL);

	same = size;
	while (size) {
		object = p[--size];
		 
		if (df->slab == virt_to_slab(object)) {
			 
			set_freepointer(df->s, object, df->freelist);
			df->freelist = object;
			df->cnt++;
			same--;
			if (size != same)
				swap(p[size], p[same]);
			continue;
		}

		 
		if (!--lookahead)
			break;
	}

	return same;
}

 
void kmem_cache_free_bulk(struct kmem_cache *s, size_t size, void **p)
{
	if (!size)
		return;

	do {
		struct detached_freelist df;

		size = build_detached_freelist(s, size, p, &df);
		if (!df.slab)
			continue;

		slab_free(df.s, df.slab, df.freelist, df.tail, &p[size], df.cnt,
			  _RET_IP_);
	} while (likely(size));
}
EXPORT_SYMBOL(kmem_cache_free_bulk);

#ifndef CONFIG_SLUB_TINY
static inline int __kmem_cache_alloc_bulk(struct kmem_cache *s, gfp_t flags,
			size_t size, void **p, struct obj_cgroup *objcg)
{
	struct kmem_cache_cpu *c;
	unsigned long irqflags;
	int i;

	 
	c = slub_get_cpu_ptr(s->cpu_slab);
	local_lock_irqsave(&s->cpu_slab->lock, irqflags);

	for (i = 0; i < size; i++) {
		void *object = kfence_alloc(s, s->object_size, flags);

		if (unlikely(object)) {
			p[i] = object;
			continue;
		}

		object = c->freelist;
		if (unlikely(!object)) {
			 
			c->tid = next_tid(c->tid);

			local_unlock_irqrestore(&s->cpu_slab->lock, irqflags);

			 
			p[i] = ___slab_alloc(s, flags, NUMA_NO_NODE,
					    _RET_IP_, c, s->object_size);
			if (unlikely(!p[i]))
				goto error;

			c = this_cpu_ptr(s->cpu_slab);
			maybe_wipe_obj_freeptr(s, p[i]);

			local_lock_irqsave(&s->cpu_slab->lock, irqflags);

			continue;  
		}
		c->freelist = get_freepointer(s, object);
		p[i] = object;
		maybe_wipe_obj_freeptr(s, p[i]);
	}
	c->tid = next_tid(c->tid);
	local_unlock_irqrestore(&s->cpu_slab->lock, irqflags);
	slub_put_cpu_ptr(s->cpu_slab);

	return i;

error:
	slub_put_cpu_ptr(s->cpu_slab);
	slab_post_alloc_hook(s, objcg, flags, i, p, false, s->object_size);
	kmem_cache_free_bulk(s, i, p);
	return 0;

}
#else  
static int __kmem_cache_alloc_bulk(struct kmem_cache *s, gfp_t flags,
			size_t size, void **p, struct obj_cgroup *objcg)
{
	int i;

	for (i = 0; i < size; i++) {
		void *object = kfence_alloc(s, s->object_size, flags);

		if (unlikely(object)) {
			p[i] = object;
			continue;
		}

		p[i] = __slab_alloc_node(s, flags, NUMA_NO_NODE,
					 _RET_IP_, s->object_size);
		if (unlikely(!p[i]))
			goto error;

		maybe_wipe_obj_freeptr(s, p[i]);
	}

	return i;

error:
	slab_post_alloc_hook(s, objcg, flags, i, p, false, s->object_size);
	kmem_cache_free_bulk(s, i, p);
	return 0;
}
#endif  

 
int kmem_cache_alloc_bulk(struct kmem_cache *s, gfp_t flags, size_t size,
			  void **p)
{
	int i;
	struct obj_cgroup *objcg = NULL;

	if (!size)
		return 0;

	 
	s = slab_pre_alloc_hook(s, NULL, &objcg, size, flags);
	if (unlikely(!s))
		return 0;

	i = __kmem_cache_alloc_bulk(s, flags, size, p, objcg);

	 
	if (i != 0)
		slab_post_alloc_hook(s, objcg, flags, size, p,
			slab_want_init_on_alloc(flags, s), s->object_size);
	return i;
}
EXPORT_SYMBOL(kmem_cache_alloc_bulk);


 

 
static unsigned int slub_min_order;
static unsigned int slub_max_order =
	IS_ENABLED(CONFIG_SLUB_TINY) ? 1 : PAGE_ALLOC_COSTLY_ORDER;
static unsigned int slub_min_objects;

 
static inline unsigned int calc_slab_order(unsigned int size,
		unsigned int min_objects, unsigned int max_order,
		unsigned int fract_leftover)
{
	unsigned int min_order = slub_min_order;
	unsigned int order;

	if (order_objects(min_order, size) > MAX_OBJS_PER_PAGE)
		return get_order(size * MAX_OBJS_PER_PAGE) - 1;

	for (order = max(min_order, (unsigned int)get_order(min_objects * size));
			order <= max_order; order++) {

		unsigned int slab_size = (unsigned int)PAGE_SIZE << order;
		unsigned int rem;

		rem = slab_size % size;

		if (rem <= slab_size / fract_leftover)
			break;
	}

	return order;
}

static inline int calculate_order(unsigned int size)
{
	unsigned int order;
	unsigned int min_objects;
	unsigned int max_objects;
	unsigned int nr_cpus;

	 
	min_objects = slub_min_objects;
	if (!min_objects) {
		 
		nr_cpus = num_present_cpus();
		if (nr_cpus <= 1)
			nr_cpus = nr_cpu_ids;
		min_objects = 4 * (fls(nr_cpus) + 1);
	}
	max_objects = order_objects(slub_max_order, size);
	min_objects = min(min_objects, max_objects);

	while (min_objects > 1) {
		unsigned int fraction;

		fraction = 16;
		while (fraction >= 4) {
			order = calc_slab_order(size, min_objects,
					slub_max_order, fraction);
			if (order <= slub_max_order)
				return order;
			fraction /= 2;
		}
		min_objects--;
	}

	 
	order = calc_slab_order(size, 1, slub_max_order, 1);
	if (order <= slub_max_order)
		return order;

	 
	order = calc_slab_order(size, 1, MAX_ORDER, 1);
	if (order <= MAX_ORDER)
		return order;
	return -ENOSYS;
}

static void
init_kmem_cache_node(struct kmem_cache_node *n)
{
	n->nr_partial = 0;
	spin_lock_init(&n->list_lock);
	INIT_LIST_HEAD(&n->partial);
#ifdef CONFIG_SLUB_DEBUG
	atomic_long_set(&n->nr_slabs, 0);
	atomic_long_set(&n->total_objects, 0);
	INIT_LIST_HEAD(&n->full);
#endif
}

#ifndef CONFIG_SLUB_TINY
static inline int alloc_kmem_cache_cpus(struct kmem_cache *s)
{
	BUILD_BUG_ON(PERCPU_DYNAMIC_EARLY_SIZE <
			NR_KMALLOC_TYPES * KMALLOC_SHIFT_HIGH *
			sizeof(struct kmem_cache_cpu));

	 
	s->cpu_slab = __alloc_percpu(sizeof(struct kmem_cache_cpu),
				     2 * sizeof(void *));

	if (!s->cpu_slab)
		return 0;

	init_kmem_cache_cpus(s);

	return 1;
}
#else
static inline int alloc_kmem_cache_cpus(struct kmem_cache *s)
{
	return 1;
}
#endif  

static struct kmem_cache *kmem_cache_node;

 
static void early_kmem_cache_node_alloc(int node)
{
	struct slab *slab;
	struct kmem_cache_node *n;

	BUG_ON(kmem_cache_node->size < sizeof(struct kmem_cache_node));

	slab = new_slab(kmem_cache_node, GFP_NOWAIT, node);

	BUG_ON(!slab);
	inc_slabs_node(kmem_cache_node, slab_nid(slab), slab->objects);
	if (slab_nid(slab) != node) {
		pr_err("SLUB: Unable to allocate memory from node %d\n", node);
		pr_err("SLUB: Allocating a useless per node structure in order to be able to continue\n");
	}

	n = slab->freelist;
	BUG_ON(!n);
#ifdef CONFIG_SLUB_DEBUG
	init_object(kmem_cache_node, n, SLUB_RED_ACTIVE);
	init_tracking(kmem_cache_node, n);
#endif
	n = kasan_slab_alloc(kmem_cache_node, n, GFP_KERNEL, false);
	slab->freelist = get_freepointer(kmem_cache_node, n);
	slab->inuse = 1;
	kmem_cache_node->node[node] = n;
	init_kmem_cache_node(n);
	inc_slabs_node(kmem_cache_node, node, slab->objects);

	 
	__add_partial(n, slab, DEACTIVATE_TO_HEAD);
}

static void free_kmem_cache_nodes(struct kmem_cache *s)
{
	int node;
	struct kmem_cache_node *n;

	for_each_kmem_cache_node(s, node, n) {
		s->node[node] = NULL;
		kmem_cache_free(kmem_cache_node, n);
	}
}

void __kmem_cache_release(struct kmem_cache *s)
{
	cache_random_seq_destroy(s);
#ifndef CONFIG_SLUB_TINY
	free_percpu(s->cpu_slab);
#endif
	free_kmem_cache_nodes(s);
}

static int init_kmem_cache_nodes(struct kmem_cache *s)
{
	int node;

	for_each_node_mask(node, slab_nodes) {
		struct kmem_cache_node *n;

		if (slab_state == DOWN) {
			early_kmem_cache_node_alloc(node);
			continue;
		}
		n = kmem_cache_alloc_node(kmem_cache_node,
						GFP_KERNEL, node);

		if (!n) {
			free_kmem_cache_nodes(s);
			return 0;
		}

		init_kmem_cache_node(n);
		s->node[node] = n;
	}
	return 1;
}

static void set_cpu_partial(struct kmem_cache *s)
{
#ifdef CONFIG_SLUB_CPU_PARTIAL
	unsigned int nr_objects;

	 
	if (!kmem_cache_has_cpu_partial(s))
		nr_objects = 0;
	else if (s->size >= PAGE_SIZE)
		nr_objects = 6;
	else if (s->size >= 1024)
		nr_objects = 24;
	else if (s->size >= 256)
		nr_objects = 52;
	else
		nr_objects = 120;

	slub_set_cpu_partial(s, nr_objects);
#endif
}

 
static int calculate_sizes(struct kmem_cache *s)
{
	slab_flags_t flags = s->flags;
	unsigned int size = s->object_size;
	unsigned int order;

	 
	size = ALIGN(size, sizeof(void *));

#ifdef CONFIG_SLUB_DEBUG
	 
	if ((flags & SLAB_POISON) && !(flags & SLAB_TYPESAFE_BY_RCU) &&
			!s->ctor)
		s->flags |= __OBJECT_POISON;
	else
		s->flags &= ~__OBJECT_POISON;


	 
	if ((flags & SLAB_RED_ZONE) && size == s->object_size)
		size += sizeof(void *);
#endif

	 
	s->inuse = size;

	if (slub_debug_orig_size(s) ||
	    (flags & (SLAB_TYPESAFE_BY_RCU | SLAB_POISON)) ||
	    ((flags & SLAB_RED_ZONE) && s->object_size < sizeof(void *)) ||
	    s->ctor) {
		 
		s->offset = size;
		size += sizeof(void *);
	} else {
		 
		s->offset = ALIGN_DOWN(s->object_size / 2, sizeof(void *));
	}

#ifdef CONFIG_SLUB_DEBUG
	if (flags & SLAB_STORE_USER) {
		 
		size += 2 * sizeof(struct track);

		 
		if (flags & SLAB_KMALLOC)
			size += sizeof(unsigned int);
	}
#endif

	kasan_cache_create(s, &size, &s->flags);
#ifdef CONFIG_SLUB_DEBUG
	if (flags & SLAB_RED_ZONE) {
		 
		size += sizeof(void *);

		s->red_left_pad = sizeof(void *);
		s->red_left_pad = ALIGN(s->red_left_pad, s->align);
		size += s->red_left_pad;
	}
#endif

	 
	size = ALIGN(size, s->align);
	s->size = size;
	s->reciprocal_size = reciprocal_value(size);
	order = calculate_order(size);

	if ((int)order < 0)
		return 0;

	s->allocflags = 0;
	if (order)
		s->allocflags |= __GFP_COMP;

	if (s->flags & SLAB_CACHE_DMA)
		s->allocflags |= GFP_DMA;

	if (s->flags & SLAB_CACHE_DMA32)
		s->allocflags |= GFP_DMA32;

	if (s->flags & SLAB_RECLAIM_ACCOUNT)
		s->allocflags |= __GFP_RECLAIMABLE;

	 
	s->oo = oo_make(order, size);
	s->min = oo_make(get_order(size), size);

	return !!oo_objects(s->oo);
}

static int kmem_cache_open(struct kmem_cache *s, slab_flags_t flags)
{
	s->flags = kmem_cache_flags(s->size, flags, s->name);
#ifdef CONFIG_SLAB_FREELIST_HARDENED
	s->random = get_random_long();
#endif

	if (!calculate_sizes(s))
		goto error;
	if (disable_higher_order_debug) {
		 
		if (get_order(s->size) > get_order(s->object_size)) {
			s->flags &= ~DEBUG_METADATA_FLAGS;
			s->offset = 0;
			if (!calculate_sizes(s))
				goto error;
		}
	}

#ifdef system_has_freelist_aba
	if (system_has_freelist_aba() && !(s->flags & SLAB_NO_CMPXCHG)) {
		 
		s->flags |= __CMPXCHG_DOUBLE;
	}
#endif

	 
	s->min_partial = min_t(unsigned long, MAX_PARTIAL, ilog2(s->size) / 2);
	s->min_partial = max_t(unsigned long, MIN_PARTIAL, s->min_partial);

	set_cpu_partial(s);

#ifdef CONFIG_NUMA
	s->remote_node_defrag_ratio = 1000;
#endif

	 
	if (slab_state >= UP) {
		if (init_cache_random_seq(s))
			goto error;
	}

	if (!init_kmem_cache_nodes(s))
		goto error;

	if (alloc_kmem_cache_cpus(s))
		return 0;

error:
	__kmem_cache_release(s);
	return -EINVAL;
}

static void list_slab_objects(struct kmem_cache *s, struct slab *slab,
			      const char *text)
{
#ifdef CONFIG_SLUB_DEBUG
	void *addr = slab_address(slab);
	void *p;

	slab_err(s, slab, text, s->name);

	spin_lock(&object_map_lock);
	__fill_map(object_map, s, slab);

	for_each_object(p, s, addr, slab->objects) {

		if (!test_bit(__obj_to_index(s, addr, p), object_map)) {
			pr_err("Object 0x%p @offset=%tu\n", p, p - addr);
			print_tracking(s, p);
		}
	}
	spin_unlock(&object_map_lock);
#endif
}

 
static void free_partial(struct kmem_cache *s, struct kmem_cache_node *n)
{
	LIST_HEAD(discard);
	struct slab *slab, *h;

	BUG_ON(irqs_disabled());
	spin_lock_irq(&n->list_lock);
	list_for_each_entry_safe(slab, h, &n->partial, slab_list) {
		if (!slab->inuse) {
			remove_partial(n, slab);
			list_add(&slab->slab_list, &discard);
		} else {
			list_slab_objects(s, slab,
			  "Objects remaining in %s on __kmem_cache_shutdown()");
		}
	}
	spin_unlock_irq(&n->list_lock);

	list_for_each_entry_safe(slab, h, &discard, slab_list)
		discard_slab(s, slab);
}

bool __kmem_cache_empty(struct kmem_cache *s)
{
	int node;
	struct kmem_cache_node *n;

	for_each_kmem_cache_node(s, node, n)
		if (n->nr_partial || node_nr_slabs(n))
			return false;
	return true;
}

 
int __kmem_cache_shutdown(struct kmem_cache *s)
{
	int node;
	struct kmem_cache_node *n;

	flush_all_cpus_locked(s);
	 
	for_each_kmem_cache_node(s, node, n) {
		free_partial(s, n);
		if (n->nr_partial || node_nr_slabs(n))
			return 1;
	}
	return 0;
}

#ifdef CONFIG_PRINTK
void __kmem_obj_info(struct kmem_obj_info *kpp, void *object, struct slab *slab)
{
	void *base;
	int __maybe_unused i;
	unsigned int objnr;
	void *objp;
	void *objp0;
	struct kmem_cache *s = slab->slab_cache;
	struct track __maybe_unused *trackp;

	kpp->kp_ptr = object;
	kpp->kp_slab = slab;
	kpp->kp_slab_cache = s;
	base = slab_address(slab);
	objp0 = kasan_reset_tag(object);
#ifdef CONFIG_SLUB_DEBUG
	objp = restore_red_left(s, objp0);
#else
	objp = objp0;
#endif
	objnr = obj_to_index(s, slab, objp);
	kpp->kp_data_offset = (unsigned long)((char *)objp0 - (char *)objp);
	objp = base + s->size * objnr;
	kpp->kp_objp = objp;
	if (WARN_ON_ONCE(objp < base || objp >= base + slab->objects * s->size
			 || (objp - base) % s->size) ||
	    !(s->flags & SLAB_STORE_USER))
		return;
#ifdef CONFIG_SLUB_DEBUG
	objp = fixup_red_left(s, objp);
	trackp = get_track(s, objp, TRACK_ALLOC);
	kpp->kp_ret = (void *)trackp->addr;
#ifdef CONFIG_STACKDEPOT
	{
		depot_stack_handle_t handle;
		unsigned long *entries;
		unsigned int nr_entries;

		handle = READ_ONCE(trackp->handle);
		if (handle) {
			nr_entries = stack_depot_fetch(handle, &entries);
			for (i = 0; i < KS_ADDRS_COUNT && i < nr_entries; i++)
				kpp->kp_stack[i] = (void *)entries[i];
		}

		trackp = get_track(s, objp, TRACK_FREE);
		handle = READ_ONCE(trackp->handle);
		if (handle) {
			nr_entries = stack_depot_fetch(handle, &entries);
			for (i = 0; i < KS_ADDRS_COUNT && i < nr_entries; i++)
				kpp->kp_free_stack[i] = (void *)entries[i];
		}
	}
#endif
#endif
}
#endif

 

static int __init setup_slub_min_order(char *str)
{
	get_option(&str, (int *)&slub_min_order);

	return 1;
}

__setup("slub_min_order=", setup_slub_min_order);

static int __init setup_slub_max_order(char *str)
{
	get_option(&str, (int *)&slub_max_order);
	slub_max_order = min_t(unsigned int, slub_max_order, MAX_ORDER);

	return 1;
}

__setup("slub_max_order=", setup_slub_max_order);

static int __init setup_slub_min_objects(char *str)
{
	get_option(&str, (int *)&slub_min_objects);

	return 1;
}

__setup("slub_min_objects=", setup_slub_min_objects);

#ifdef CONFIG_HARDENED_USERCOPY
 
void __check_heap_object(const void *ptr, unsigned long n,
			 const struct slab *slab, bool to_user)
{
	struct kmem_cache *s;
	unsigned int offset;
	bool is_kfence = is_kfence_address(ptr);

	ptr = kasan_reset_tag(ptr);

	 
	s = slab->slab_cache;

	 
	if (ptr < slab_address(slab))
		usercopy_abort("SLUB object not in SLUB page?!", NULL,
			       to_user, 0, n);

	 
	if (is_kfence)
		offset = ptr - kfence_object_start(ptr);
	else
		offset = (ptr - slab_address(slab)) % s->size;

	 
	if (!is_kfence && kmem_cache_debug_flags(s, SLAB_RED_ZONE)) {
		if (offset < s->red_left_pad)
			usercopy_abort("SLUB object in left red zone",
				       s->name, to_user, offset, n);
		offset -= s->red_left_pad;
	}

	 
	if (offset >= s->useroffset &&
	    offset - s->useroffset <= s->usersize &&
	    n <= s->useroffset - offset + s->usersize)
		return;

	usercopy_abort("SLUB object", s->name, to_user, offset, n);
}
#endif  

#define SHRINK_PROMOTE_MAX 32

 
static int __kmem_cache_do_shrink(struct kmem_cache *s)
{
	int node;
	int i;
	struct kmem_cache_node *n;
	struct slab *slab;
	struct slab *t;
	struct list_head discard;
	struct list_head promote[SHRINK_PROMOTE_MAX];
	unsigned long flags;
	int ret = 0;

	for_each_kmem_cache_node(s, node, n) {
		INIT_LIST_HEAD(&discard);
		for (i = 0; i < SHRINK_PROMOTE_MAX; i++)
			INIT_LIST_HEAD(promote + i);

		spin_lock_irqsave(&n->list_lock, flags);

		 
		list_for_each_entry_safe(slab, t, &n->partial, slab_list) {
			int free = slab->objects - slab->inuse;

			 
			barrier();

			 
			BUG_ON(free <= 0);

			if (free == slab->objects) {
				list_move(&slab->slab_list, &discard);
				n->nr_partial--;
				dec_slabs_node(s, node, slab->objects);
			} else if (free <= SHRINK_PROMOTE_MAX)
				list_move(&slab->slab_list, promote + free - 1);
		}

		 
		for (i = SHRINK_PROMOTE_MAX - 1; i >= 0; i--)
			list_splice(promote + i, &n->partial);

		spin_unlock_irqrestore(&n->list_lock, flags);

		 
		list_for_each_entry_safe(slab, t, &discard, slab_list)
			free_slab(s, slab);

		if (node_nr_slabs(n))
			ret = 1;
	}

	return ret;
}

int __kmem_cache_shrink(struct kmem_cache *s)
{
	flush_all(s);
	return __kmem_cache_do_shrink(s);
}

static int slab_mem_going_offline_callback(void *arg)
{
	struct kmem_cache *s;

	mutex_lock(&slab_mutex);
	list_for_each_entry(s, &slab_caches, list) {
		flush_all_cpus_locked(s);
		__kmem_cache_do_shrink(s);
	}
	mutex_unlock(&slab_mutex);

	return 0;
}

static void slab_mem_offline_callback(void *arg)
{
	struct memory_notify *marg = arg;
	int offline_node;

	offline_node = marg->status_change_nid_normal;

	 
	if (offline_node < 0)
		return;

	mutex_lock(&slab_mutex);
	node_clear(offline_node, slab_nodes);
	 
	mutex_unlock(&slab_mutex);
}

static int slab_mem_going_online_callback(void *arg)
{
	struct kmem_cache_node *n;
	struct kmem_cache *s;
	struct memory_notify *marg = arg;
	int nid = marg->status_change_nid_normal;
	int ret = 0;

	 
	if (nid < 0)
		return 0;

	 
	mutex_lock(&slab_mutex);
	list_for_each_entry(s, &slab_caches, list) {
		 
		if (get_node(s, nid))
			continue;
		 
		n = kmem_cache_alloc(kmem_cache_node, GFP_KERNEL);
		if (!n) {
			ret = -ENOMEM;
			goto out;
		}
		init_kmem_cache_node(n);
		s->node[nid] = n;
	}
	 
	node_set(nid, slab_nodes);
out:
	mutex_unlock(&slab_mutex);
	return ret;
}

static int slab_memory_callback(struct notifier_block *self,
				unsigned long action, void *arg)
{
	int ret = 0;

	switch (action) {
	case MEM_GOING_ONLINE:
		ret = slab_mem_going_online_callback(arg);
		break;
	case MEM_GOING_OFFLINE:
		ret = slab_mem_going_offline_callback(arg);
		break;
	case MEM_OFFLINE:
	case MEM_CANCEL_ONLINE:
		slab_mem_offline_callback(arg);
		break;
	case MEM_ONLINE:
	case MEM_CANCEL_OFFLINE:
		break;
	}
	if (ret)
		ret = notifier_from_errno(ret);
	else
		ret = NOTIFY_OK;
	return ret;
}

 

 

static struct kmem_cache * __init bootstrap(struct kmem_cache *static_cache)
{
	int node;
	struct kmem_cache *s = kmem_cache_zalloc(kmem_cache, GFP_NOWAIT);
	struct kmem_cache_node *n;

	memcpy(s, static_cache, kmem_cache->object_size);

	 
	__flush_cpu_slab(s, smp_processor_id());
	for_each_kmem_cache_node(s, node, n) {
		struct slab *p;

		list_for_each_entry(p, &n->partial, slab_list)
			p->slab_cache = s;

#ifdef CONFIG_SLUB_DEBUG
		list_for_each_entry(p, &n->full, slab_list)
			p->slab_cache = s;
#endif
	}
	list_add(&s->list, &slab_caches);
	return s;
}

void __init kmem_cache_init(void)
{
	static __initdata struct kmem_cache boot_kmem_cache,
		boot_kmem_cache_node;
	int node;

	if (debug_guardpage_minorder())
		slub_max_order = 0;

	 
	if (__slub_debug_enabled())
		no_hash_pointers_enable(NULL);

	kmem_cache_node = &boot_kmem_cache_node;
	kmem_cache = &boot_kmem_cache;

	 
	for_each_node_state(node, N_NORMAL_MEMORY)
		node_set(node, slab_nodes);

	create_boot_cache(kmem_cache_node, "kmem_cache_node",
		sizeof(struct kmem_cache_node), SLAB_HWCACHE_ALIGN, 0, 0);

	hotplug_memory_notifier(slab_memory_callback, SLAB_CALLBACK_PRI);

	 
	slab_state = PARTIAL;

	create_boot_cache(kmem_cache, "kmem_cache",
			offsetof(struct kmem_cache, node) +
				nr_node_ids * sizeof(struct kmem_cache_node *),
		       SLAB_HWCACHE_ALIGN, 0, 0);

	kmem_cache = bootstrap(&boot_kmem_cache);
	kmem_cache_node = bootstrap(&boot_kmem_cache_node);

	 
	setup_kmalloc_cache_index_table();
	create_kmalloc_caches(0);

	 
	init_freelist_randomization();

	cpuhp_setup_state_nocalls(CPUHP_SLUB_DEAD, "slub:dead", NULL,
				  slub_cpu_dead);

	pr_info("SLUB: HWalign=%d, Order=%u-%u, MinObjects=%u, CPUs=%u, Nodes=%u\n",
		cache_line_size(),
		slub_min_order, slub_max_order, slub_min_objects,
		nr_cpu_ids, nr_node_ids);
}

void __init kmem_cache_init_late(void)
{
#ifndef CONFIG_SLUB_TINY
	flushwq = alloc_workqueue("slub_flushwq", WQ_MEM_RECLAIM, 0);
	WARN_ON(!flushwq);
#endif
}

struct kmem_cache *
__kmem_cache_alias(const char *name, unsigned int size, unsigned int align,
		   slab_flags_t flags, void (*ctor)(void *))
{
	struct kmem_cache *s;

	s = find_mergeable(size, align, flags, name, ctor);
	if (s) {
		if (sysfs_slab_alias(s, name))
			return NULL;

		s->refcount++;

		 
		s->object_size = max(s->object_size, size);
		s->inuse = max(s->inuse, ALIGN(size, sizeof(void *)));
	}

	return s;
}

int __kmem_cache_create(struct kmem_cache *s, slab_flags_t flags)
{
	int err;

	err = kmem_cache_open(s, flags);
	if (err)
		return err;

	 
	if (slab_state <= UP)
		return 0;

	err = sysfs_slab_add(s);
	if (err) {
		__kmem_cache_release(s);
		return err;
	}

	if (s->flags & SLAB_STORE_USER)
		debugfs_slab_add(s);

	return 0;
}

#ifdef SLAB_SUPPORTS_SYSFS
static int count_inuse(struct slab *slab)
{
	return slab->inuse;
}

static int count_total(struct slab *slab)
{
	return slab->objects;
}
#endif

#ifdef CONFIG_SLUB_DEBUG
static void validate_slab(struct kmem_cache *s, struct slab *slab,
			  unsigned long *obj_map)
{
	void *p;
	void *addr = slab_address(slab);

	if (!check_slab(s, slab) || !on_freelist(s, slab, NULL))
		return;

	 
	__fill_map(obj_map, s, slab);
	for_each_object(p, s, addr, slab->objects) {
		u8 val = test_bit(__obj_to_index(s, addr, p), obj_map) ?
			 SLUB_RED_INACTIVE : SLUB_RED_ACTIVE;

		if (!check_object(s, slab, p, val))
			break;
	}
}

static int validate_slab_node(struct kmem_cache *s,
		struct kmem_cache_node *n, unsigned long *obj_map)
{
	unsigned long count = 0;
	struct slab *slab;
	unsigned long flags;

	spin_lock_irqsave(&n->list_lock, flags);

	list_for_each_entry(slab, &n->partial, slab_list) {
		validate_slab(s, slab, obj_map);
		count++;
	}
	if (count != n->nr_partial) {
		pr_err("SLUB %s: %ld partial slabs counted but counter=%ld\n",
		       s->name, count, n->nr_partial);
		slab_add_kunit_errors();
	}

	if (!(s->flags & SLAB_STORE_USER))
		goto out;

	list_for_each_entry(slab, &n->full, slab_list) {
		validate_slab(s, slab, obj_map);
		count++;
	}
	if (count != node_nr_slabs(n)) {
		pr_err("SLUB: %s %ld slabs counted but counter=%ld\n",
		       s->name, count, node_nr_slabs(n));
		slab_add_kunit_errors();
	}

out:
	spin_unlock_irqrestore(&n->list_lock, flags);
	return count;
}

long validate_slab_cache(struct kmem_cache *s)
{
	int node;
	unsigned long count = 0;
	struct kmem_cache_node *n;
	unsigned long *obj_map;

	obj_map = bitmap_alloc(oo_objects(s->oo), GFP_KERNEL);
	if (!obj_map)
		return -ENOMEM;

	flush_all(s);
	for_each_kmem_cache_node(s, node, n)
		count += validate_slab_node(s, n, obj_map);

	bitmap_free(obj_map);

	return count;
}
EXPORT_SYMBOL(validate_slab_cache);

#ifdef CONFIG_DEBUG_FS
 

struct location {
	depot_stack_handle_t handle;
	unsigned long count;
	unsigned long addr;
	unsigned long waste;
	long long sum_time;
	long min_time;
	long max_time;
	long min_pid;
	long max_pid;
	DECLARE_BITMAP(cpus, NR_CPUS);
	nodemask_t nodes;
};

struct loc_track {
	unsigned long max;
	unsigned long count;
	struct location *loc;
	loff_t idx;
};

static struct dentry *slab_debugfs_root;

static void free_loc_track(struct loc_track *t)
{
	if (t->max)
		free_pages((unsigned long)t->loc,
			get_order(sizeof(struct location) * t->max));
}

static int alloc_loc_track(struct loc_track *t, unsigned long max, gfp_t flags)
{
	struct location *l;
	int order;

	order = get_order(sizeof(struct location) * max);

	l = (void *)__get_free_pages(flags, order);
	if (!l)
		return 0;

	if (t->count) {
		memcpy(l, t->loc, sizeof(struct location) * t->count);
		free_loc_track(t);
	}
	t->max = max;
	t->loc = l;
	return 1;
}

static int add_location(struct loc_track *t, struct kmem_cache *s,
				const struct track *track,
				unsigned int orig_size)
{
	long start, end, pos;
	struct location *l;
	unsigned long caddr, chandle, cwaste;
	unsigned long age = jiffies - track->when;
	depot_stack_handle_t handle = 0;
	unsigned int waste = s->object_size - orig_size;

#ifdef CONFIG_STACKDEPOT
	handle = READ_ONCE(track->handle);
#endif
	start = -1;
	end = t->count;

	for ( ; ; ) {
		pos = start + (end - start + 1) / 2;

		 
		if (pos == end)
			break;

		l = &t->loc[pos];
		caddr = l->addr;
		chandle = l->handle;
		cwaste = l->waste;
		if ((track->addr == caddr) && (handle == chandle) &&
			(waste == cwaste)) {

			l->count++;
			if (track->when) {
				l->sum_time += age;
				if (age < l->min_time)
					l->min_time = age;
				if (age > l->max_time)
					l->max_time = age;

				if (track->pid < l->min_pid)
					l->min_pid = track->pid;
				if (track->pid > l->max_pid)
					l->max_pid = track->pid;

				cpumask_set_cpu(track->cpu,
						to_cpumask(l->cpus));
			}
			node_set(page_to_nid(virt_to_page(track)), l->nodes);
			return 1;
		}

		if (track->addr < caddr)
			end = pos;
		else if (track->addr == caddr && handle < chandle)
			end = pos;
		else if (track->addr == caddr && handle == chandle &&
				waste < cwaste)
			end = pos;
		else
			start = pos;
	}

	 
	if (t->count >= t->max && !alloc_loc_track(t, 2 * t->max, GFP_ATOMIC))
		return 0;

	l = t->loc + pos;
	if (pos < t->count)
		memmove(l + 1, l,
			(t->count - pos) * sizeof(struct location));
	t->count++;
	l->count = 1;
	l->addr = track->addr;
	l->sum_time = age;
	l->min_time = age;
	l->max_time = age;
	l->min_pid = track->pid;
	l->max_pid = track->pid;
	l->handle = handle;
	l->waste = waste;
	cpumask_clear(to_cpumask(l->cpus));
	cpumask_set_cpu(track->cpu, to_cpumask(l->cpus));
	nodes_clear(l->nodes);
	node_set(page_to_nid(virt_to_page(track)), l->nodes);
	return 1;
}

static void process_slab(struct loc_track *t, struct kmem_cache *s,
		struct slab *slab, enum track_item alloc,
		unsigned long *obj_map)
{
	void *addr = slab_address(slab);
	bool is_alloc = (alloc == TRACK_ALLOC);
	void *p;

	__fill_map(obj_map, s, slab);

	for_each_object(p, s, addr, slab->objects)
		if (!test_bit(__obj_to_index(s, addr, p), obj_map))
			add_location(t, s, get_track(s, p, alloc),
				     is_alloc ? get_orig_size(s, p) :
						s->object_size);
}
#endif   
#endif	 

#ifdef SLAB_SUPPORTS_SYSFS
enum slab_stat_type {
	SL_ALL,			 
	SL_PARTIAL,		 
	SL_CPU,			 
	SL_OBJECTS,		 
	SL_TOTAL		 
};

#define SO_ALL		(1 << SL_ALL)
#define SO_PARTIAL	(1 << SL_PARTIAL)
#define SO_CPU		(1 << SL_CPU)
#define SO_OBJECTS	(1 << SL_OBJECTS)
#define SO_TOTAL	(1 << SL_TOTAL)

static ssize_t show_slab_objects(struct kmem_cache *s,
				 char *buf, unsigned long flags)
{
	unsigned long total = 0;
	int node;
	int x;
	unsigned long *nodes;
	int len = 0;

	nodes = kcalloc(nr_node_ids, sizeof(unsigned long), GFP_KERNEL);
	if (!nodes)
		return -ENOMEM;

	if (flags & SO_CPU) {
		int cpu;

		for_each_possible_cpu(cpu) {
			struct kmem_cache_cpu *c = per_cpu_ptr(s->cpu_slab,
							       cpu);
			int node;
			struct slab *slab;

			slab = READ_ONCE(c->slab);
			if (!slab)
				continue;

			node = slab_nid(slab);
			if (flags & SO_TOTAL)
				x = slab->objects;
			else if (flags & SO_OBJECTS)
				x = slab->inuse;
			else
				x = 1;

			total += x;
			nodes[node] += x;

#ifdef CONFIG_SLUB_CPU_PARTIAL
			slab = slub_percpu_partial_read_once(c);
			if (slab) {
				node = slab_nid(slab);
				if (flags & SO_TOTAL)
					WARN_ON_ONCE(1);
				else if (flags & SO_OBJECTS)
					WARN_ON_ONCE(1);
				else
					x = slab->slabs;
				total += x;
				nodes[node] += x;
			}
#endif
		}
	}

	 

#ifdef CONFIG_SLUB_DEBUG
	if (flags & SO_ALL) {
		struct kmem_cache_node *n;

		for_each_kmem_cache_node(s, node, n) {

			if (flags & SO_TOTAL)
				x = node_nr_objs(n);
			else if (flags & SO_OBJECTS)
				x = node_nr_objs(n) - count_partial(n, count_free);
			else
				x = node_nr_slabs(n);
			total += x;
			nodes[node] += x;
		}

	} else
#endif
	if (flags & SO_PARTIAL) {
		struct kmem_cache_node *n;

		for_each_kmem_cache_node(s, node, n) {
			if (flags & SO_TOTAL)
				x = count_partial(n, count_total);
			else if (flags & SO_OBJECTS)
				x = count_partial(n, count_inuse);
			else
				x = n->nr_partial;
			total += x;
			nodes[node] += x;
		}
	}

	len += sysfs_emit_at(buf, len, "%lu", total);
#ifdef CONFIG_NUMA
	for (node = 0; node < nr_node_ids; node++) {
		if (nodes[node])
			len += sysfs_emit_at(buf, len, " N%d=%lu",
					     node, nodes[node]);
	}
#endif
	len += sysfs_emit_at(buf, len, "\n");
	kfree(nodes);

	return len;
}

#define to_slab_attr(n) container_of(n, struct slab_attribute, attr)
#define to_slab(n) container_of(n, struct kmem_cache, kobj)

struct slab_attribute {
	struct attribute attr;
	ssize_t (*show)(struct kmem_cache *s, char *buf);
	ssize_t (*store)(struct kmem_cache *s, const char *x, size_t count);
};

#define SLAB_ATTR_RO(_name) \
	static struct slab_attribute _name##_attr = __ATTR_RO_MODE(_name, 0400)

#define SLAB_ATTR(_name) \
	static struct slab_attribute _name##_attr = __ATTR_RW_MODE(_name, 0600)

static ssize_t slab_size_show(struct kmem_cache *s, char *buf)
{
	return sysfs_emit(buf, "%u\n", s->size);
}
SLAB_ATTR_RO(slab_size);

static ssize_t align_show(struct kmem_cache *s, char *buf)
{
	return sysfs_emit(buf, "%u\n", s->align);
}
SLAB_ATTR_RO(align);

static ssize_t object_size_show(struct kmem_cache *s, char *buf)
{
	return sysfs_emit(buf, "%u\n", s->object_size);
}
SLAB_ATTR_RO(object_size);

static ssize_t objs_per_slab_show(struct kmem_cache *s, char *buf)
{
	return sysfs_emit(buf, "%u\n", oo_objects(s->oo));
}
SLAB_ATTR_RO(objs_per_slab);

static ssize_t order_show(struct kmem_cache *s, char *buf)
{
	return sysfs_emit(buf, "%u\n", oo_order(s->oo));
}
SLAB_ATTR_RO(order);

static ssize_t min_partial_show(struct kmem_cache *s, char *buf)
{
	return sysfs_emit(buf, "%lu\n", s->min_partial);
}

static ssize_t min_partial_store(struct kmem_cache *s, const char *buf,
				 size_t length)
{
	unsigned long min;
	int err;

	err = kstrtoul(buf, 10, &min);
	if (err)
		return err;

	s->min_partial = min;
	return length;
}
SLAB_ATTR(min_partial);

static ssize_t cpu_partial_show(struct kmem_cache *s, char *buf)
{
	unsigned int nr_partial = 0;
#ifdef CONFIG_SLUB_CPU_PARTIAL
	nr_partial = s->cpu_partial;
#endif

	return sysfs_emit(buf, "%u\n", nr_partial);
}

static ssize_t cpu_partial_store(struct kmem_cache *s, const char *buf,
				 size_t length)
{
	unsigned int objects;
	int err;

	err = kstrtouint(buf, 10, &objects);
	if (err)
		return err;
	if (objects && !kmem_cache_has_cpu_partial(s))
		return -EINVAL;

	slub_set_cpu_partial(s, objects);
	flush_all(s);
	return length;
}
SLAB_ATTR(cpu_partial);

static ssize_t ctor_show(struct kmem_cache *s, char *buf)
{
	if (!s->ctor)
		return 0;
	return sysfs_emit(buf, "%pS\n", s->ctor);
}
SLAB_ATTR_RO(ctor);

static ssize_t aliases_show(struct kmem_cache *s, char *buf)
{
	return sysfs_emit(buf, "%d\n", s->refcount < 0 ? 0 : s->refcount - 1);
}
SLAB_ATTR_RO(aliases);

static ssize_t partial_show(struct kmem_cache *s, char *buf)
{
	return show_slab_objects(s, buf, SO_PARTIAL);
}
SLAB_ATTR_RO(partial);

static ssize_t cpu_slabs_show(struct kmem_cache *s, char *buf)
{
	return show_slab_objects(s, buf, SO_CPU);
}
SLAB_ATTR_RO(cpu_slabs);

static ssize_t objects_partial_show(struct kmem_cache *s, char *buf)
{
	return show_slab_objects(s, buf, SO_PARTIAL|SO_OBJECTS);
}
SLAB_ATTR_RO(objects_partial);

static ssize_t slabs_cpu_partial_show(struct kmem_cache *s, char *buf)
{
	int objects = 0;
	int slabs = 0;
	int cpu __maybe_unused;
	int len = 0;

#ifdef CONFIG_SLUB_CPU_PARTIAL
	for_each_online_cpu(cpu) {
		struct slab *slab;

		slab = slub_percpu_partial(per_cpu_ptr(s->cpu_slab, cpu));

		if (slab)
			slabs += slab->slabs;
	}
#endif

	 
	objects = (slabs * oo_objects(s->oo)) / 2;
	len += sysfs_emit_at(buf, len, "%d(%d)", objects, slabs);

#ifdef CONFIG_SLUB_CPU_PARTIAL
	for_each_online_cpu(cpu) {
		struct slab *slab;

		slab = slub_percpu_partial(per_cpu_ptr(s->cpu_slab, cpu));
		if (slab) {
			slabs = READ_ONCE(slab->slabs);
			objects = (slabs * oo_objects(s->oo)) / 2;
			len += sysfs_emit_at(buf, len, " C%d=%d(%d)",
					     cpu, objects, slabs);
		}
	}
#endif
	len += sysfs_emit_at(buf, len, "\n");

	return len;
}
SLAB_ATTR_RO(slabs_cpu_partial);

static ssize_t reclaim_account_show(struct kmem_cache *s, char *buf)
{
	return sysfs_emit(buf, "%d\n", !!(s->flags & SLAB_RECLAIM_ACCOUNT));
}
SLAB_ATTR_RO(reclaim_account);

static ssize_t hwcache_align_show(struct kmem_cache *s, char *buf)
{
	return sysfs_emit(buf, "%d\n", !!(s->flags & SLAB_HWCACHE_ALIGN));
}
SLAB_ATTR_RO(hwcache_align);

#ifdef CONFIG_ZONE_DMA
static ssize_t cache_dma_show(struct kmem_cache *s, char *buf)
{
	return sysfs_emit(buf, "%d\n", !!(s->flags & SLAB_CACHE_DMA));
}
SLAB_ATTR_RO(cache_dma);
#endif

#ifdef CONFIG_HARDENED_USERCOPY
static ssize_t usersize_show(struct kmem_cache *s, char *buf)
{
	return sysfs_emit(buf, "%u\n", s->usersize);
}
SLAB_ATTR_RO(usersize);
#endif

static ssize_t destroy_by_rcu_show(struct kmem_cache *s, char *buf)
{
	return sysfs_emit(buf, "%d\n", !!(s->flags & SLAB_TYPESAFE_BY_RCU));
}
SLAB_ATTR_RO(destroy_by_rcu);

#ifdef CONFIG_SLUB_DEBUG
static ssize_t slabs_show(struct kmem_cache *s, char *buf)
{
	return show_slab_objects(s, buf, SO_ALL);
}
SLAB_ATTR_RO(slabs);

static ssize_t total_objects_show(struct kmem_cache *s, char *buf)
{
	return show_slab_objects(s, buf, SO_ALL|SO_TOTAL);
}
SLAB_ATTR_RO(total_objects);

static ssize_t objects_show(struct kmem_cache *s, char *buf)
{
	return show_slab_objects(s, buf, SO_ALL|SO_OBJECTS);
}
SLAB_ATTR_RO(objects);

static ssize_t sanity_checks_show(struct kmem_cache *s, char *buf)
{
	return sysfs_emit(buf, "%d\n", !!(s->flags & SLAB_CONSISTENCY_CHECKS));
}
SLAB_ATTR_RO(sanity_checks);

static ssize_t trace_show(struct kmem_cache *s, char *buf)
{
	return sysfs_emit(buf, "%d\n", !!(s->flags & SLAB_TRACE));
}
SLAB_ATTR_RO(trace);

static ssize_t red_zone_show(struct kmem_cache *s, char *buf)
{
	return sysfs_emit(buf, "%d\n", !!(s->flags & SLAB_RED_ZONE));
}

SLAB_ATTR_RO(red_zone);

static ssize_t poison_show(struct kmem_cache *s, char *buf)
{
	return sysfs_emit(buf, "%d\n", !!(s->flags & SLAB_POISON));
}

SLAB_ATTR_RO(poison);

static ssize_t store_user_show(struct kmem_cache *s, char *buf)
{
	return sysfs_emit(buf, "%d\n", !!(s->flags & SLAB_STORE_USER));
}

SLAB_ATTR_RO(store_user);

static ssize_t validate_show(struct kmem_cache *s, char *buf)
{
	return 0;
}

static ssize_t validate_store(struct kmem_cache *s,
			const char *buf, size_t length)
{
	int ret = -EINVAL;

	if (buf[0] == '1' && kmem_cache_debug(s)) {
		ret = validate_slab_cache(s);
		if (ret >= 0)
			ret = length;
	}
	return ret;
}
SLAB_ATTR(validate);

#endif  

#ifdef CONFIG_FAILSLAB
static ssize_t failslab_show(struct kmem_cache *s, char *buf)
{
	return sysfs_emit(buf, "%d\n", !!(s->flags & SLAB_FAILSLAB));
}

static ssize_t failslab_store(struct kmem_cache *s, const char *buf,
				size_t length)
{
	if (s->refcount > 1)
		return -EINVAL;

	if (buf[0] == '1')
		WRITE_ONCE(s->flags, s->flags | SLAB_FAILSLAB);
	else
		WRITE_ONCE(s->flags, s->flags & ~SLAB_FAILSLAB);

	return length;
}
SLAB_ATTR(failslab);
#endif

static ssize_t shrink_show(struct kmem_cache *s, char *buf)
{
	return 0;
}

static ssize_t shrink_store(struct kmem_cache *s,
			const char *buf, size_t length)
{
	if (buf[0] == '1')
		kmem_cache_shrink(s);
	else
		return -EINVAL;
	return length;
}
SLAB_ATTR(shrink);

#ifdef CONFIG_NUMA
static ssize_t remote_node_defrag_ratio_show(struct kmem_cache *s, char *buf)
{
	return sysfs_emit(buf, "%u\n", s->remote_node_defrag_ratio / 10);
}

static ssize_t remote_node_defrag_ratio_store(struct kmem_cache *s,
				const char *buf, size_t length)
{
	unsigned int ratio;
	int err;

	err = kstrtouint(buf, 10, &ratio);
	if (err)
		return err;
	if (ratio > 100)
		return -ERANGE;

	s->remote_node_defrag_ratio = ratio * 10;

	return length;
}
SLAB_ATTR(remote_node_defrag_ratio);
#endif

#ifdef CONFIG_SLUB_STATS
static int show_stat(struct kmem_cache *s, char *buf, enum stat_item si)
{
	unsigned long sum  = 0;
	int cpu;
	int len = 0;
	int *data = kmalloc_array(nr_cpu_ids, sizeof(int), GFP_KERNEL);

	if (!data)
		return -ENOMEM;

	for_each_online_cpu(cpu) {
		unsigned x = per_cpu_ptr(s->cpu_slab, cpu)->stat[si];

		data[cpu] = x;
		sum += x;
	}

	len += sysfs_emit_at(buf, len, "%lu", sum);

#ifdef CONFIG_SMP
	for_each_online_cpu(cpu) {
		if (data[cpu])
			len += sysfs_emit_at(buf, len, " C%d=%u",
					     cpu, data[cpu]);
	}
#endif
	kfree(data);
	len += sysfs_emit_at(buf, len, "\n");

	return len;
}

static void clear_stat(struct kmem_cache *s, enum stat_item si)
{
	int cpu;

	for_each_online_cpu(cpu)
		per_cpu_ptr(s->cpu_slab, cpu)->stat[si] = 0;
}

#define STAT_ATTR(si, text) 					\
static ssize_t text##_show(struct kmem_cache *s, char *buf)	\
{								\
	return show_stat(s, buf, si);				\
}								\
static ssize_t text##_store(struct kmem_cache *s,		\
				const char *buf, size_t length)	\
{								\
	if (buf[0] != '0')					\
		return -EINVAL;					\
	clear_stat(s, si);					\
	return length;						\
}								\
SLAB_ATTR(text);						\

STAT_ATTR(ALLOC_FASTPATH, alloc_fastpath);
STAT_ATTR(ALLOC_SLOWPATH, alloc_slowpath);
STAT_ATTR(FREE_FASTPATH, free_fastpath);
STAT_ATTR(FREE_SLOWPATH, free_slowpath);
STAT_ATTR(FREE_FROZEN, free_frozen);
STAT_ATTR(FREE_ADD_PARTIAL, free_add_partial);
STAT_ATTR(FREE_REMOVE_PARTIAL, free_remove_partial);
STAT_ATTR(ALLOC_FROM_PARTIAL, alloc_from_partial);
STAT_ATTR(ALLOC_SLAB, alloc_slab);
STAT_ATTR(ALLOC_REFILL, alloc_refill);
STAT_ATTR(ALLOC_NODE_MISMATCH, alloc_node_mismatch);
STAT_ATTR(FREE_SLAB, free_slab);
STAT_ATTR(CPUSLAB_FLUSH, cpuslab_flush);
STAT_ATTR(DEACTIVATE_FULL, deactivate_full);
STAT_ATTR(DEACTIVATE_EMPTY, deactivate_empty);
STAT_ATTR(DEACTIVATE_TO_HEAD, deactivate_to_head);
STAT_ATTR(DEACTIVATE_TO_TAIL, deactivate_to_tail);
STAT_ATTR(DEACTIVATE_REMOTE_FREES, deactivate_remote_frees);
STAT_ATTR(DEACTIVATE_BYPASS, deactivate_bypass);
STAT_ATTR(ORDER_FALLBACK, order_fallback);
STAT_ATTR(CMPXCHG_DOUBLE_CPU_FAIL, cmpxchg_double_cpu_fail);
STAT_ATTR(CMPXCHG_DOUBLE_FAIL, cmpxchg_double_fail);
STAT_ATTR(CPU_PARTIAL_ALLOC, cpu_partial_alloc);
STAT_ATTR(CPU_PARTIAL_FREE, cpu_partial_free);
STAT_ATTR(CPU_PARTIAL_NODE, cpu_partial_node);
STAT_ATTR(CPU_PARTIAL_DRAIN, cpu_partial_drain);
#endif	 

#ifdef CONFIG_KFENCE
static ssize_t skip_kfence_show(struct kmem_cache *s, char *buf)
{
	return sysfs_emit(buf, "%d\n", !!(s->flags & SLAB_SKIP_KFENCE));
}

static ssize_t skip_kfence_store(struct kmem_cache *s,
			const char *buf, size_t length)
{
	int ret = length;

	if (buf[0] == '0')
		s->flags &= ~SLAB_SKIP_KFENCE;
	else if (buf[0] == '1')
		s->flags |= SLAB_SKIP_KFENCE;
	else
		ret = -EINVAL;

	return ret;
}
SLAB_ATTR(skip_kfence);
#endif

static struct attribute *slab_attrs[] = {
	&slab_size_attr.attr,
	&object_size_attr.attr,
	&objs_per_slab_attr.attr,
	&order_attr.attr,
	&min_partial_attr.attr,
	&cpu_partial_attr.attr,
	&objects_partial_attr.attr,
	&partial_attr.attr,
	&cpu_slabs_attr.attr,
	&ctor_attr.attr,
	&aliases_attr.attr,
	&align_attr.attr,
	&hwcache_align_attr.attr,
	&reclaim_account_attr.attr,
	&destroy_by_rcu_attr.attr,
	&shrink_attr.attr,
	&slabs_cpu_partial_attr.attr,
#ifdef CONFIG_SLUB_DEBUG
	&total_objects_attr.attr,
	&objects_attr.attr,
	&slabs_attr.attr,
	&sanity_checks_attr.attr,
	&trace_attr.attr,
	&red_zone_attr.attr,
	&poison_attr.attr,
	&store_user_attr.attr,
	&validate_attr.attr,
#endif
#ifdef CONFIG_ZONE_DMA
	&cache_dma_attr.attr,
#endif
#ifdef CONFIG_NUMA
	&remote_node_defrag_ratio_attr.attr,
#endif
#ifdef CONFIG_SLUB_STATS
	&alloc_fastpath_attr.attr,
	&alloc_slowpath_attr.attr,
	&free_fastpath_attr.attr,
	&free_slowpath_attr.attr,
	&free_frozen_attr.attr,
	&free_add_partial_attr.attr,
	&free_remove_partial_attr.attr,
	&alloc_from_partial_attr.attr,
	&alloc_slab_attr.attr,
	&alloc_refill_attr.attr,
	&alloc_node_mismatch_attr.attr,
	&free_slab_attr.attr,
	&cpuslab_flush_attr.attr,
	&deactivate_full_attr.attr,
	&deactivate_empty_attr.attr,
	&deactivate_to_head_attr.attr,
	&deactivate_to_tail_attr.attr,
	&deactivate_remote_frees_attr.attr,
	&deactivate_bypass_attr.attr,
	&order_fallback_attr.attr,
	&cmpxchg_double_fail_attr.attr,
	&cmpxchg_double_cpu_fail_attr.attr,
	&cpu_partial_alloc_attr.attr,
	&cpu_partial_free_attr.attr,
	&cpu_partial_node_attr.attr,
	&cpu_partial_drain_attr.attr,
#endif
#ifdef CONFIG_FAILSLAB
	&failslab_attr.attr,
#endif
#ifdef CONFIG_HARDENED_USERCOPY
	&usersize_attr.attr,
#endif
#ifdef CONFIG_KFENCE
	&skip_kfence_attr.attr,
#endif

	NULL
};

static const struct attribute_group slab_attr_group = {
	.attrs = slab_attrs,
};

static ssize_t slab_attr_show(struct kobject *kobj,
				struct attribute *attr,
				char *buf)
{
	struct slab_attribute *attribute;
	struct kmem_cache *s;

	attribute = to_slab_attr(attr);
	s = to_slab(kobj);

	if (!attribute->show)
		return -EIO;

	return attribute->show(s, buf);
}

static ssize_t slab_attr_store(struct kobject *kobj,
				struct attribute *attr,
				const char *buf, size_t len)
{
	struct slab_attribute *attribute;
	struct kmem_cache *s;

	attribute = to_slab_attr(attr);
	s = to_slab(kobj);

	if (!attribute->store)
		return -EIO;

	return attribute->store(s, buf, len);
}

static void kmem_cache_release(struct kobject *k)
{
	slab_kmem_cache_release(to_slab(k));
}

static const struct sysfs_ops slab_sysfs_ops = {
	.show = slab_attr_show,
	.store = slab_attr_store,
};

static const struct kobj_type slab_ktype = {
	.sysfs_ops = &slab_sysfs_ops,
	.release = kmem_cache_release,
};

static struct kset *slab_kset;

static inline struct kset *cache_kset(struct kmem_cache *s)
{
	return slab_kset;
}

#define ID_STR_LENGTH 32

 
static char *create_unique_id(struct kmem_cache *s)
{
	char *name = kmalloc(ID_STR_LENGTH, GFP_KERNEL);
	char *p = name;

	if (!name)
		return ERR_PTR(-ENOMEM);

	*p++ = ':';
	 
	if (s->flags & SLAB_CACHE_DMA)
		*p++ = 'd';
	if (s->flags & SLAB_CACHE_DMA32)
		*p++ = 'D';
	if (s->flags & SLAB_RECLAIM_ACCOUNT)
		*p++ = 'a';
	if (s->flags & SLAB_CONSISTENCY_CHECKS)
		*p++ = 'F';
	if (s->flags & SLAB_ACCOUNT)
		*p++ = 'A';
	if (p != name + 1)
		*p++ = '-';
	p += snprintf(p, ID_STR_LENGTH - (p - name), "%07u", s->size);

	if (WARN_ON(p > name + ID_STR_LENGTH - 1)) {
		kfree(name);
		return ERR_PTR(-EINVAL);
	}
	kmsan_unpoison_memory(name, p - name);
	return name;
}

static int sysfs_slab_add(struct kmem_cache *s)
{
	int err;
	const char *name;
	struct kset *kset = cache_kset(s);
	int unmergeable = slab_unmergeable(s);

	if (!unmergeable && disable_higher_order_debug &&
			(slub_debug & DEBUG_METADATA_FLAGS))
		unmergeable = 1;

	if (unmergeable) {
		 
		sysfs_remove_link(&slab_kset->kobj, s->name);
		name = s->name;
	} else {
		 
		name = create_unique_id(s);
		if (IS_ERR(name))
			return PTR_ERR(name);
	}

	s->kobj.kset = kset;
	err = kobject_init_and_add(&s->kobj, &slab_ktype, NULL, "%s", name);
	if (err)
		goto out;

	err = sysfs_create_group(&s->kobj, &slab_attr_group);
	if (err)
		goto out_del_kobj;

	if (!unmergeable) {
		 
		sysfs_slab_alias(s, s->name);
	}
out:
	if (!unmergeable)
		kfree(name);
	return err;
out_del_kobj:
	kobject_del(&s->kobj);
	goto out;
}

void sysfs_slab_unlink(struct kmem_cache *s)
{
	if (slab_state >= FULL)
		kobject_del(&s->kobj);
}

void sysfs_slab_release(struct kmem_cache *s)
{
	if (slab_state >= FULL)
		kobject_put(&s->kobj);
}

 
struct saved_alias {
	struct kmem_cache *s;
	const char *name;
	struct saved_alias *next;
};

static struct saved_alias *alias_list;

static int sysfs_slab_alias(struct kmem_cache *s, const char *name)
{
	struct saved_alias *al;

	if (slab_state == FULL) {
		 
		sysfs_remove_link(&slab_kset->kobj, name);
		return sysfs_create_link(&slab_kset->kobj, &s->kobj, name);
	}

	al = kmalloc(sizeof(struct saved_alias), GFP_KERNEL);
	if (!al)
		return -ENOMEM;

	al->s = s;
	al->name = name;
	al->next = alias_list;
	alias_list = al;
	kmsan_unpoison_memory(al, sizeof(*al));
	return 0;
}

static int __init slab_sysfs_init(void)
{
	struct kmem_cache *s;
	int err;

	mutex_lock(&slab_mutex);

	slab_kset = kset_create_and_add("slab", NULL, kernel_kobj);
	if (!slab_kset) {
		mutex_unlock(&slab_mutex);
		pr_err("Cannot register slab subsystem.\n");
		return -ENOMEM;
	}

	slab_state = FULL;

	list_for_each_entry(s, &slab_caches, list) {
		err = sysfs_slab_add(s);
		if (err)
			pr_err("SLUB: Unable to add boot slab %s to sysfs\n",
			       s->name);
	}

	while (alias_list) {
		struct saved_alias *al = alias_list;

		alias_list = alias_list->next;
		err = sysfs_slab_alias(al->s, al->name);
		if (err)
			pr_err("SLUB: Unable to add boot slab alias %s to sysfs\n",
			       al->name);
		kfree(al);
	}

	mutex_unlock(&slab_mutex);
	return 0;
}
late_initcall(slab_sysfs_init);
#endif  

#if defined(CONFIG_SLUB_DEBUG) && defined(CONFIG_DEBUG_FS)
static int slab_debugfs_show(struct seq_file *seq, void *v)
{
	struct loc_track *t = seq->private;
	struct location *l;
	unsigned long idx;

	idx = (unsigned long) t->idx;
	if (idx < t->count) {
		l = &t->loc[idx];

		seq_printf(seq, "%7ld ", l->count);

		if (l->addr)
			seq_printf(seq, "%pS", (void *)l->addr);
		else
			seq_puts(seq, "<not-available>");

		if (l->waste)
			seq_printf(seq, " waste=%lu/%lu",
				l->count * l->waste, l->waste);

		if (l->sum_time != l->min_time) {
			seq_printf(seq, " age=%ld/%llu/%ld",
				l->min_time, div_u64(l->sum_time, l->count),
				l->max_time);
		} else
			seq_printf(seq, " age=%ld", l->min_time);

		if (l->min_pid != l->max_pid)
			seq_printf(seq, " pid=%ld-%ld", l->min_pid, l->max_pid);
		else
			seq_printf(seq, " pid=%ld",
				l->min_pid);

		if (num_online_cpus() > 1 && !cpumask_empty(to_cpumask(l->cpus)))
			seq_printf(seq, " cpus=%*pbl",
				 cpumask_pr_args(to_cpumask(l->cpus)));

		if (nr_online_nodes > 1 && !nodes_empty(l->nodes))
			seq_printf(seq, " nodes=%*pbl",
				 nodemask_pr_args(&l->nodes));

#ifdef CONFIG_STACKDEPOT
		{
			depot_stack_handle_t handle;
			unsigned long *entries;
			unsigned int nr_entries, j;

			handle = READ_ONCE(l->handle);
			if (handle) {
				nr_entries = stack_depot_fetch(handle, &entries);
				seq_puts(seq, "\n");
				for (j = 0; j < nr_entries; j++)
					seq_printf(seq, "        %pS\n", (void *)entries[j]);
			}
		}
#endif
		seq_puts(seq, "\n");
	}

	if (!idx && !t->count)
		seq_puts(seq, "No data\n");

	return 0;
}

static void slab_debugfs_stop(struct seq_file *seq, void *v)
{
}

static void *slab_debugfs_next(struct seq_file *seq, void *v, loff_t *ppos)
{
	struct loc_track *t = seq->private;

	t->idx = ++(*ppos);
	if (*ppos <= t->count)
		return ppos;

	return NULL;
}

static int cmp_loc_by_count(const void *a, const void *b, const void *data)
{
	struct location *loc1 = (struct location *)a;
	struct location *loc2 = (struct location *)b;

	if (loc1->count > loc2->count)
		return -1;
	else
		return 1;
}

static void *slab_debugfs_start(struct seq_file *seq, loff_t *ppos)
{
	struct loc_track *t = seq->private;

	t->idx = *ppos;
	return ppos;
}

static const struct seq_operations slab_debugfs_sops = {
	.start  = slab_debugfs_start,
	.next   = slab_debugfs_next,
	.stop   = slab_debugfs_stop,
	.show   = slab_debugfs_show,
};

static int slab_debug_trace_open(struct inode *inode, struct file *filep)
{

	struct kmem_cache_node *n;
	enum track_item alloc;
	int node;
	struct loc_track *t = __seq_open_private(filep, &slab_debugfs_sops,
						sizeof(struct loc_track));
	struct kmem_cache *s = file_inode(filep)->i_private;
	unsigned long *obj_map;

	if (!t)
		return -ENOMEM;

	obj_map = bitmap_alloc(oo_objects(s->oo), GFP_KERNEL);
	if (!obj_map) {
		seq_release_private(inode, filep);
		return -ENOMEM;
	}

	if (strcmp(filep->f_path.dentry->d_name.name, "alloc_traces") == 0)
		alloc = TRACK_ALLOC;
	else
		alloc = TRACK_FREE;

	if (!alloc_loc_track(t, PAGE_SIZE / sizeof(struct location), GFP_KERNEL)) {
		bitmap_free(obj_map);
		seq_release_private(inode, filep);
		return -ENOMEM;
	}

	for_each_kmem_cache_node(s, node, n) {
		unsigned long flags;
		struct slab *slab;

		if (!node_nr_slabs(n))
			continue;

		spin_lock_irqsave(&n->list_lock, flags);
		list_for_each_entry(slab, &n->partial, slab_list)
			process_slab(t, s, slab, alloc, obj_map);
		list_for_each_entry(slab, &n->full, slab_list)
			process_slab(t, s, slab, alloc, obj_map);
		spin_unlock_irqrestore(&n->list_lock, flags);
	}

	 
	sort_r(t->loc, t->count, sizeof(struct location),
		cmp_loc_by_count, NULL, NULL);

	bitmap_free(obj_map);
	return 0;
}

static int slab_debug_trace_release(struct inode *inode, struct file *file)
{
	struct seq_file *seq = file->private_data;
	struct loc_track *t = seq->private;

	free_loc_track(t);
	return seq_release_private(inode, file);
}

static const struct file_operations slab_debugfs_fops = {
	.open    = slab_debug_trace_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = slab_debug_trace_release,
};

static void debugfs_slab_add(struct kmem_cache *s)
{
	struct dentry *slab_cache_dir;

	if (unlikely(!slab_debugfs_root))
		return;

	slab_cache_dir = debugfs_create_dir(s->name, slab_debugfs_root);

	debugfs_create_file("alloc_traces", 0400,
		slab_cache_dir, s, &slab_debugfs_fops);

	debugfs_create_file("free_traces", 0400,
		slab_cache_dir, s, &slab_debugfs_fops);
}

void debugfs_slab_release(struct kmem_cache *s)
{
	debugfs_lookup_and_remove(s->name, slab_debugfs_root);
}

static int __init slab_debugfs_init(void)
{
	struct kmem_cache *s;

	slab_debugfs_root = debugfs_create_dir("slab", NULL);

	list_for_each_entry(s, &slab_caches, list)
		if (s->flags & SLAB_STORE_USER)
			debugfs_slab_add(s);

	return 0;

}
__initcall(slab_debugfs_init);
#endif
 
#ifdef CONFIG_SLUB_DEBUG
void get_slabinfo(struct kmem_cache *s, struct slabinfo *sinfo)
{
	unsigned long nr_slabs = 0;
	unsigned long nr_objs = 0;
	unsigned long nr_free = 0;
	int node;
	struct kmem_cache_node *n;

	for_each_kmem_cache_node(s, node, n) {
		nr_slabs += node_nr_slabs(n);
		nr_objs += node_nr_objs(n);
		nr_free += count_partial(n, count_free);
	}

	sinfo->active_objs = nr_objs - nr_free;
	sinfo->num_objs = nr_objs;
	sinfo->active_slabs = nr_slabs;
	sinfo->num_slabs = nr_slabs;
	sinfo->objects_per_slab = oo_objects(s->oo);
	sinfo->cache_order = oo_order(s->oo);
}

void slabinfo_show_stats(struct seq_file *m, struct kmem_cache *s)
{
}

ssize_t slabinfo_write(struct file *file, const char __user *buffer,
		       size_t count, loff_t *ppos)
{
	return -EIO;
}
#endif  
