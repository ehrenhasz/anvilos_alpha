
 

#include <linux/stddef.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/kasan.h>
#include <linux/kmsan.h>
#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/ratelimit.h>
#include <linux/oom.h>
#include <linux/topology.h>
#include <linux/sysctl.h>
#include <linux/cpu.h>
#include <linux/cpuset.h>
#include <linux/memory_hotplug.h>
#include <linux/nodemask.h>
#include <linux/vmstat.h>
#include <linux/fault-inject.h>
#include <linux/compaction.h>
#include <trace/events/kmem.h>
#include <trace/events/oom.h>
#include <linux/prefetch.h>
#include <linux/mm_inline.h>
#include <linux/mmu_notifier.h>
#include <linux/migrate.h>
#include <linux/sched/mm.h>
#include <linux/page_owner.h>
#include <linux/page_table_check.h>
#include <linux/memcontrol.h>
#include <linux/ftrace.h>
#include <linux/lockdep.h>
#include <linux/psi.h>
#include <linux/khugepaged.h>
#include <linux/delayacct.h>
#include <asm/div64.h>
#include "internal.h"
#include "shuffle.h"
#include "page_reporting.h"

 
typedef int __bitwise fpi_t;

 
#define FPI_NONE		((__force fpi_t)0)

 
#define FPI_SKIP_REPORT_NOTIFY	((__force fpi_t)BIT(0))

 
#define FPI_TO_TAIL		((__force fpi_t)BIT(1))

 
static DEFINE_MUTEX(pcp_batch_high_lock);
#define MIN_PERCPU_PAGELIST_HIGH_FRACTION (8)

#if defined(CONFIG_SMP) || defined(CONFIG_PREEMPT_RT)
 
#define pcp_trylock_prepare(flags)	do { } while (0)
#define pcp_trylock_finish(flag)	do { } while (0)
#else

 
#define pcp_trylock_prepare(flags)	local_irq_save(flags)
#define pcp_trylock_finish(flags)	local_irq_restore(flags)
#endif

 
#ifndef CONFIG_PREEMPT_RT
#define pcpu_task_pin()		preempt_disable()
#define pcpu_task_unpin()	preempt_enable()
#else
#define pcpu_task_pin()		migrate_disable()
#define pcpu_task_unpin()	migrate_enable()
#endif

 
#define pcpu_spin_lock(type, member, ptr)				\
({									\
	type *_ret;							\
	pcpu_task_pin();						\
	_ret = this_cpu_ptr(ptr);					\
	spin_lock(&_ret->member);					\
	_ret;								\
})

#define pcpu_spin_trylock(type, member, ptr)				\
({									\
	type *_ret;							\
	pcpu_task_pin();						\
	_ret = this_cpu_ptr(ptr);					\
	if (!spin_trylock(&_ret->member)) {				\
		pcpu_task_unpin();					\
		_ret = NULL;						\
	}								\
	_ret;								\
})

#define pcpu_spin_unlock(member, ptr)					\
({									\
	spin_unlock(&ptr->member);					\
	pcpu_task_unpin();						\
})

 
#define pcp_spin_lock(ptr)						\
	pcpu_spin_lock(struct per_cpu_pages, lock, ptr)

#define pcp_spin_trylock(ptr)						\
	pcpu_spin_trylock(struct per_cpu_pages, lock, ptr)

#define pcp_spin_unlock(ptr)						\
	pcpu_spin_unlock(lock, ptr)

#ifdef CONFIG_USE_PERCPU_NUMA_NODE_ID
DEFINE_PER_CPU(int, numa_node);
EXPORT_PER_CPU_SYMBOL(numa_node);
#endif

DEFINE_STATIC_KEY_TRUE(vm_numa_stat_key);

#ifdef CONFIG_HAVE_MEMORYLESS_NODES
 
DEFINE_PER_CPU(int, _numa_mem_);		 
EXPORT_PER_CPU_SYMBOL(_numa_mem_);
#endif

static DEFINE_MUTEX(pcpu_drain_mutex);

#ifdef CONFIG_GCC_PLUGIN_LATENT_ENTROPY
volatile unsigned long latent_entropy __latent_entropy;
EXPORT_SYMBOL(latent_entropy);
#endif

 
nodemask_t node_states[NR_NODE_STATES] __read_mostly = {
	[N_POSSIBLE] = NODE_MASK_ALL,
	[N_ONLINE] = { { [0] = 1UL } },
#ifndef CONFIG_NUMA
	[N_NORMAL_MEMORY] = { { [0] = 1UL } },
#ifdef CONFIG_HIGHMEM
	[N_HIGH_MEMORY] = { { [0] = 1UL } },
#endif
	[N_MEMORY] = { { [0] = 1UL } },
	[N_CPU] = { { [0] = 1UL } },
#endif	 
};
EXPORT_SYMBOL(node_states);

gfp_t gfp_allowed_mask __read_mostly = GFP_BOOT_MASK;

 
static inline int get_pcppage_migratetype(struct page *page)
{
	return page->index;
}

static inline void set_pcppage_migratetype(struct page *page, int migratetype)
{
	page->index = migratetype;
}

#ifdef CONFIG_HUGETLB_PAGE_SIZE_VARIABLE
unsigned int pageblock_order __read_mostly;
#endif

static void __free_pages_ok(struct page *page, unsigned int order,
			    fpi_t fpi_flags);

 
static int sysctl_lowmem_reserve_ratio[MAX_NR_ZONES] = {
#ifdef CONFIG_ZONE_DMA
	[ZONE_DMA] = 256,
#endif
#ifdef CONFIG_ZONE_DMA32
	[ZONE_DMA32] = 256,
#endif
	[ZONE_NORMAL] = 32,
#ifdef CONFIG_HIGHMEM
	[ZONE_HIGHMEM] = 0,
#endif
	[ZONE_MOVABLE] = 0,
};

char * const zone_names[MAX_NR_ZONES] = {
#ifdef CONFIG_ZONE_DMA
	 "DMA",
#endif
#ifdef CONFIG_ZONE_DMA32
	 "DMA32",
#endif
	 "Normal",
#ifdef CONFIG_HIGHMEM
	 "HighMem",
#endif
	 "Movable",
#ifdef CONFIG_ZONE_DEVICE
	 "Device",
#endif
};

const char * const migratetype_names[MIGRATE_TYPES] = {
	"Unmovable",
	"Movable",
	"Reclaimable",
	"HighAtomic",
#ifdef CONFIG_CMA
	"CMA",
#endif
#ifdef CONFIG_MEMORY_ISOLATION
	"Isolate",
#endif
};

int min_free_kbytes = 1024;
int user_min_free_kbytes = -1;
static int watermark_boost_factor __read_mostly = 15000;
static int watermark_scale_factor = 10;

 
int movable_zone;
EXPORT_SYMBOL(movable_zone);

#if MAX_NUMNODES > 1
unsigned int nr_node_ids __read_mostly = MAX_NUMNODES;
unsigned int nr_online_nodes __read_mostly = 1;
EXPORT_SYMBOL(nr_node_ids);
EXPORT_SYMBOL(nr_online_nodes);
#endif

static bool page_contains_unaccepted(struct page *page, unsigned int order);
static void accept_page(struct page *page, unsigned int order);
static bool try_to_accept_memory(struct zone *zone, unsigned int order);
static inline bool has_unaccepted_memory(void);
static bool __free_unaccepted(struct page *page);

int page_group_by_mobility_disabled __read_mostly;

#ifdef CONFIG_DEFERRED_STRUCT_PAGE_INIT
 
DEFINE_STATIC_KEY_TRUE(deferred_pages);

static inline bool deferred_pages_enabled(void)
{
	return static_branch_unlikely(&deferred_pages);
}

 
static bool __ref
_deferred_grow_zone(struct zone *zone, unsigned int order)
{
       return deferred_grow_zone(zone, order);
}
#else
static inline bool deferred_pages_enabled(void)
{
	return false;
}
#endif  

 
static inline unsigned long *get_pageblock_bitmap(const struct page *page,
							unsigned long pfn)
{
#ifdef CONFIG_SPARSEMEM
	return section_to_usemap(__pfn_to_section(pfn));
#else
	return page_zone(page)->pageblock_flags;
#endif  
}

static inline int pfn_to_bitidx(const struct page *page, unsigned long pfn)
{
#ifdef CONFIG_SPARSEMEM
	pfn &= (PAGES_PER_SECTION-1);
#else
	pfn = pfn - pageblock_start_pfn(page_zone(page)->zone_start_pfn);
#endif  
	return (pfn >> pageblock_order) * NR_PAGEBLOCK_BITS;
}

 
unsigned long get_pfnblock_flags_mask(const struct page *page,
					unsigned long pfn, unsigned long mask)
{
	unsigned long *bitmap;
	unsigned long bitidx, word_bitidx;
	unsigned long word;

	bitmap = get_pageblock_bitmap(page, pfn);
	bitidx = pfn_to_bitidx(page, pfn);
	word_bitidx = bitidx / BITS_PER_LONG;
	bitidx &= (BITS_PER_LONG-1);
	 
	word = READ_ONCE(bitmap[word_bitidx]);
	return (word >> bitidx) & mask;
}

static __always_inline int get_pfnblock_migratetype(const struct page *page,
					unsigned long pfn)
{
	return get_pfnblock_flags_mask(page, pfn, MIGRATETYPE_MASK);
}

 
void set_pfnblock_flags_mask(struct page *page, unsigned long flags,
					unsigned long pfn,
					unsigned long mask)
{
	unsigned long *bitmap;
	unsigned long bitidx, word_bitidx;
	unsigned long word;

	BUILD_BUG_ON(NR_PAGEBLOCK_BITS != 4);
	BUILD_BUG_ON(MIGRATE_TYPES > (1 << PB_migratetype_bits));

	bitmap = get_pageblock_bitmap(page, pfn);
	bitidx = pfn_to_bitidx(page, pfn);
	word_bitidx = bitidx / BITS_PER_LONG;
	bitidx &= (BITS_PER_LONG-1);

	VM_BUG_ON_PAGE(!zone_spans_pfn(page_zone(page), pfn), page);

	mask <<= bitidx;
	flags <<= bitidx;

	word = READ_ONCE(bitmap[word_bitidx]);
	do {
	} while (!try_cmpxchg(&bitmap[word_bitidx], &word, (word & ~mask) | flags));
}

void set_pageblock_migratetype(struct page *page, int migratetype)
{
	if (unlikely(page_group_by_mobility_disabled &&
		     migratetype < MIGRATE_PCPTYPES))
		migratetype = MIGRATE_UNMOVABLE;

	set_pfnblock_flags_mask(page, (unsigned long)migratetype,
				page_to_pfn(page), MIGRATETYPE_MASK);
}

#ifdef CONFIG_DEBUG_VM
static int page_outside_zone_boundaries(struct zone *zone, struct page *page)
{
	int ret;
	unsigned seq;
	unsigned long pfn = page_to_pfn(page);
	unsigned long sp, start_pfn;

	do {
		seq = zone_span_seqbegin(zone);
		start_pfn = zone->zone_start_pfn;
		sp = zone->spanned_pages;
		ret = !zone_spans_pfn(zone, pfn);
	} while (zone_span_seqretry(zone, seq));

	if (ret)
		pr_err("page 0x%lx outside node %d zone %s [ 0x%lx - 0x%lx ]\n",
			pfn, zone_to_nid(zone), zone->name,
			start_pfn, start_pfn + sp);

	return ret;
}

 
static int __maybe_unused bad_range(struct zone *zone, struct page *page)
{
	if (page_outside_zone_boundaries(zone, page))
		return 1;
	if (zone != page_zone(page))
		return 1;

	return 0;
}
#else
static inline int __maybe_unused bad_range(struct zone *zone, struct page *page)
{
	return 0;
}
#endif

static void bad_page(struct page *page, const char *reason)
{
	static unsigned long resume;
	static unsigned long nr_shown;
	static unsigned long nr_unshown;

	 
	if (nr_shown == 60) {
		if (time_before(jiffies, resume)) {
			nr_unshown++;
			goto out;
		}
		if (nr_unshown) {
			pr_alert(
			      "BUG: Bad page state: %lu messages suppressed\n",
				nr_unshown);
			nr_unshown = 0;
		}
		nr_shown = 0;
	}
	if (nr_shown++ == 0)
		resume = jiffies + 60 * HZ;

	pr_alert("BUG: Bad page state in process %s  pfn:%05lx\n",
		current->comm, page_to_pfn(page));
	dump_page(page, reason);

	print_modules();
	dump_stack();
out:
	 
	page_mapcount_reset(page);  
	add_taint(TAINT_BAD_PAGE, LOCKDEP_NOW_UNRELIABLE);
}

static inline unsigned int order_to_pindex(int migratetype, int order)
{
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	if (order > PAGE_ALLOC_COSTLY_ORDER) {
		VM_BUG_ON(order != pageblock_order);
		return NR_LOWORDER_PCP_LISTS;
	}
#else
	VM_BUG_ON(order > PAGE_ALLOC_COSTLY_ORDER);
#endif

	return (MIGRATE_PCPTYPES * order) + migratetype;
}

static inline int pindex_to_order(unsigned int pindex)
{
	int order = pindex / MIGRATE_PCPTYPES;

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	if (pindex == NR_LOWORDER_PCP_LISTS)
		order = pageblock_order;
#else
	VM_BUG_ON(order > PAGE_ALLOC_COSTLY_ORDER);
#endif

	return order;
}

static inline bool pcp_allowed_order(unsigned int order)
{
	if (order <= PAGE_ALLOC_COSTLY_ORDER)
		return true;
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	if (order == pageblock_order)
		return true;
#endif
	return false;
}

static inline void free_the_page(struct page *page, unsigned int order)
{
	if (pcp_allowed_order(order))		 
		free_unref_page(page, order);
	else
		__free_pages_ok(page, order, FPI_NONE);
}

 

void prep_compound_page(struct page *page, unsigned int order)
{
	int i;
	int nr_pages = 1 << order;

	__SetPageHead(page);
	for (i = 1; i < nr_pages; i++)
		prep_compound_tail(page, i);

	prep_compound_head(page, order);
}

void destroy_large_folio(struct folio *folio)
{
	if (folio_test_hugetlb(folio)) {
		free_huge_folio(folio);
		return;
	}

	if (folio_test_large_rmappable(folio))
		folio_undo_large_rmappable(folio);

	mem_cgroup_uncharge(folio);
	free_the_page(&folio->page, folio_order(folio));
}

static inline void set_buddy_order(struct page *page, unsigned int order)
{
	set_page_private(page, order);
	__SetPageBuddy(page);
}

#ifdef CONFIG_COMPACTION
static inline struct capture_control *task_capc(struct zone *zone)
{
	struct capture_control *capc = current->capture_control;

	return unlikely(capc) &&
		!(current->flags & PF_KTHREAD) &&
		!capc->page &&
		capc->cc->zone == zone ? capc : NULL;
}

static inline bool
compaction_capture(struct capture_control *capc, struct page *page,
		   int order, int migratetype)
{
	if (!capc || order != capc->cc->order)
		return false;

	 
	if (is_migrate_cma(migratetype) ||
	    is_migrate_isolate(migratetype))
		return false;

	 
	if (order < pageblock_order && migratetype == MIGRATE_MOVABLE)
		return false;

	capc->page = page;
	return true;
}

#else
static inline struct capture_control *task_capc(struct zone *zone)
{
	return NULL;
}

static inline bool
compaction_capture(struct capture_control *capc, struct page *page,
		   int order, int migratetype)
{
	return false;
}
#endif  

 
static inline void add_to_free_list(struct page *page, struct zone *zone,
				    unsigned int order, int migratetype)
{
	struct free_area *area = &zone->free_area[order];

	list_add(&page->buddy_list, &area->free_list[migratetype]);
	area->nr_free++;
}

 
static inline void add_to_free_list_tail(struct page *page, struct zone *zone,
					 unsigned int order, int migratetype)
{
	struct free_area *area = &zone->free_area[order];

	list_add_tail(&page->buddy_list, &area->free_list[migratetype]);
	area->nr_free++;
}

 
static inline void move_to_free_list(struct page *page, struct zone *zone,
				     unsigned int order, int migratetype)
{
	struct free_area *area = &zone->free_area[order];

	list_move_tail(&page->buddy_list, &area->free_list[migratetype]);
}

static inline void del_page_from_free_list(struct page *page, struct zone *zone,
					   unsigned int order)
{
	 
	if (page_reported(page))
		__ClearPageReported(page);

	list_del(&page->buddy_list);
	__ClearPageBuddy(page);
	set_page_private(page, 0);
	zone->free_area[order].nr_free--;
}

static inline struct page *get_page_from_free_area(struct free_area *area,
					    int migratetype)
{
	return list_first_entry_or_null(&area->free_list[migratetype],
					struct page, buddy_list);
}

 
static inline bool
buddy_merge_likely(unsigned long pfn, unsigned long buddy_pfn,
		   struct page *page, unsigned int order)
{
	unsigned long higher_page_pfn;
	struct page *higher_page;

	if (order >= MAX_ORDER - 1)
		return false;

	higher_page_pfn = buddy_pfn & pfn;
	higher_page = page + (higher_page_pfn - pfn);

	return find_buddy_page_pfn(higher_page, higher_page_pfn, order + 1,
			NULL) != NULL;
}

 

static inline void __free_one_page(struct page *page,
		unsigned long pfn,
		struct zone *zone, unsigned int order,
		int migratetype, fpi_t fpi_flags)
{
	struct capture_control *capc = task_capc(zone);
	unsigned long buddy_pfn = 0;
	unsigned long combined_pfn;
	struct page *buddy;
	bool to_tail;

	VM_BUG_ON(!zone_is_initialized(zone));
	VM_BUG_ON_PAGE(page->flags & PAGE_FLAGS_CHECK_AT_PREP, page);

	VM_BUG_ON(migratetype == -1);
	if (likely(!is_migrate_isolate(migratetype)))
		__mod_zone_freepage_state(zone, 1 << order, migratetype);

	VM_BUG_ON_PAGE(pfn & ((1 << order) - 1), page);
	VM_BUG_ON_PAGE(bad_range(zone, page), page);

	while (order < MAX_ORDER) {
		if (compaction_capture(capc, page, order, migratetype)) {
			__mod_zone_freepage_state(zone, -(1 << order),
								migratetype);
			return;
		}

		buddy = find_buddy_page_pfn(page, pfn, order, &buddy_pfn);
		if (!buddy)
			goto done_merging;

		if (unlikely(order >= pageblock_order)) {
			 
			int buddy_mt = get_pfnblock_migratetype(buddy, buddy_pfn);

			if (migratetype != buddy_mt
					&& (!migratetype_is_mergeable(migratetype) ||
						!migratetype_is_mergeable(buddy_mt)))
				goto done_merging;
		}

		 
		if (page_is_guard(buddy))
			clear_page_guard(zone, buddy, order, migratetype);
		else
			del_page_from_free_list(buddy, zone, order);
		combined_pfn = buddy_pfn & pfn;
		page = page + (combined_pfn - pfn);
		pfn = combined_pfn;
		order++;
	}

done_merging:
	set_buddy_order(page, order);

	if (fpi_flags & FPI_TO_TAIL)
		to_tail = true;
	else if (is_shuffle_order(order))
		to_tail = shuffle_pick_tail();
	else
		to_tail = buddy_merge_likely(pfn, buddy_pfn, page, order);

	if (to_tail)
		add_to_free_list_tail(page, zone, order, migratetype);
	else
		add_to_free_list(page, zone, order, migratetype);

	 
	if (!(fpi_flags & FPI_SKIP_REPORT_NOTIFY))
		page_reporting_notify_free(order);
}

 
int split_free_page(struct page *free_page,
			unsigned int order, unsigned long split_pfn_offset)
{
	struct zone *zone = page_zone(free_page);
	unsigned long free_page_pfn = page_to_pfn(free_page);
	unsigned long pfn;
	unsigned long flags;
	int free_page_order;
	int mt;
	int ret = 0;

	if (split_pfn_offset == 0)
		return ret;

	spin_lock_irqsave(&zone->lock, flags);

	if (!PageBuddy(free_page) || buddy_order(free_page) != order) {
		ret = -ENOENT;
		goto out;
	}

	mt = get_pfnblock_migratetype(free_page, free_page_pfn);
	if (likely(!is_migrate_isolate(mt)))
		__mod_zone_freepage_state(zone, -(1UL << order), mt);

	del_page_from_free_list(free_page, zone, order);
	for (pfn = free_page_pfn;
	     pfn < free_page_pfn + (1UL << order);) {
		int mt = get_pfnblock_migratetype(pfn_to_page(pfn), pfn);

		free_page_order = min_t(unsigned int,
					pfn ? __ffs(pfn) : order,
					__fls(split_pfn_offset));
		__free_one_page(pfn_to_page(pfn), pfn, zone, free_page_order,
				mt, FPI_NONE);
		pfn += 1UL << free_page_order;
		split_pfn_offset -= (1UL << free_page_order);
		 
		if (split_pfn_offset == 0)
			split_pfn_offset = (1UL << order) - (pfn - free_page_pfn);
	}
out:
	spin_unlock_irqrestore(&zone->lock, flags);
	return ret;
}
 
static inline bool page_expected_state(struct page *page,
					unsigned long check_flags)
{
	if (unlikely(atomic_read(&page->_mapcount) != -1))
		return false;

	if (unlikely((unsigned long)page->mapping |
			page_ref_count(page) |
#ifdef CONFIG_MEMCG
			page->memcg_data |
#endif
			(page->flags & check_flags)))
		return false;

	return true;
}

static const char *page_bad_reason(struct page *page, unsigned long flags)
{
	const char *bad_reason = NULL;

	if (unlikely(atomic_read(&page->_mapcount) != -1))
		bad_reason = "nonzero mapcount";
	if (unlikely(page->mapping != NULL))
		bad_reason = "non-NULL mapping";
	if (unlikely(page_ref_count(page) != 0))
		bad_reason = "nonzero _refcount";
	if (unlikely(page->flags & flags)) {
		if (flags == PAGE_FLAGS_CHECK_AT_PREP)
			bad_reason = "PAGE_FLAGS_CHECK_AT_PREP flag(s) set";
		else
			bad_reason = "PAGE_FLAGS_CHECK_AT_FREE flag(s) set";
	}
#ifdef CONFIG_MEMCG
	if (unlikely(page->memcg_data))
		bad_reason = "page still charged to cgroup";
#endif
	return bad_reason;
}

static void free_page_is_bad_report(struct page *page)
{
	bad_page(page,
		 page_bad_reason(page, PAGE_FLAGS_CHECK_AT_FREE));
}

static inline bool free_page_is_bad(struct page *page)
{
	if (likely(page_expected_state(page, PAGE_FLAGS_CHECK_AT_FREE)))
		return false;

	 
	free_page_is_bad_report(page);
	return true;
}

static inline bool is_check_pages_enabled(void)
{
	return static_branch_unlikely(&check_pages_enabled);
}

static int free_tail_page_prepare(struct page *head_page, struct page *page)
{
	struct folio *folio = (struct folio *)head_page;
	int ret = 1;

	 
	BUILD_BUG_ON((unsigned long)LIST_POISON1 & 1);

	if (!is_check_pages_enabled()) {
		ret = 0;
		goto out;
	}
	switch (page - head_page) {
	case 1:
		 
		if (unlikely(folio_entire_mapcount(folio))) {
			bad_page(page, "nonzero entire_mapcount");
			goto out;
		}
		if (unlikely(atomic_read(&folio->_nr_pages_mapped))) {
			bad_page(page, "nonzero nr_pages_mapped");
			goto out;
		}
		if (unlikely(atomic_read(&folio->_pincount))) {
			bad_page(page, "nonzero pincount");
			goto out;
		}
		break;
	case 2:
		 
		break;
	default:
		if (page->mapping != TAIL_MAPPING) {
			bad_page(page, "corrupted mapping in tail page");
			goto out;
		}
		break;
	}
	if (unlikely(!PageTail(page))) {
		bad_page(page, "PageTail not set");
		goto out;
	}
	if (unlikely(compound_head(page) != head_page)) {
		bad_page(page, "compound_head not consistent");
		goto out;
	}
	ret = 0;
out:
	page->mapping = NULL;
	clear_compound_head(page);
	return ret;
}

 
static inline bool should_skip_kasan_poison(struct page *page, fpi_t fpi_flags)
{
	if (IS_ENABLED(CONFIG_KASAN_GENERIC))
		return deferred_pages_enabled();

	return page_kasan_tag(page) == 0xff;
}

static void kernel_init_pages(struct page *page, int numpages)
{
	int i;

	 
	kasan_disable_current();
	for (i = 0; i < numpages; i++)
		clear_highpage_kasan_tagged(page + i);
	kasan_enable_current();
}

static __always_inline bool free_pages_prepare(struct page *page,
			unsigned int order, fpi_t fpi_flags)
{
	int bad = 0;
	bool skip_kasan_poison = should_skip_kasan_poison(page, fpi_flags);
	bool init = want_init_on_free();

	VM_BUG_ON_PAGE(PageTail(page), page);

	trace_mm_page_free(page, order);
	kmsan_free_page(page, order);

	if (unlikely(PageHWPoison(page)) && !order) {
		 
		if (memcg_kmem_online() && PageMemcgKmem(page))
			__memcg_kmem_uncharge_page(page, order);
		reset_page_owner(page, order);
		page_table_check_free(page, order);
		return false;
	}

	 
	if (unlikely(order)) {
		bool compound = PageCompound(page);
		int i;

		VM_BUG_ON_PAGE(compound && compound_order(page) != order, page);

		if (compound)
			page[1].flags &= ~PAGE_FLAGS_SECOND;
		for (i = 1; i < (1 << order); i++) {
			if (compound)
				bad += free_tail_page_prepare(page, page + i);
			if (is_check_pages_enabled()) {
				if (free_page_is_bad(page + i)) {
					bad++;
					continue;
				}
			}
			(page + i)->flags &= ~PAGE_FLAGS_CHECK_AT_PREP;
		}
	}
	if (PageMappingFlags(page))
		page->mapping = NULL;
	if (memcg_kmem_online() && PageMemcgKmem(page))
		__memcg_kmem_uncharge_page(page, order);
	if (is_check_pages_enabled()) {
		if (free_page_is_bad(page))
			bad++;
		if (bad)
			return false;
	}

	page_cpupid_reset_last(page);
	page->flags &= ~PAGE_FLAGS_CHECK_AT_PREP;
	reset_page_owner(page, order);
	page_table_check_free(page, order);

	if (!PageHighMem(page)) {
		debug_check_no_locks_freed(page_address(page),
					   PAGE_SIZE << order);
		debug_check_no_obj_freed(page_address(page),
					   PAGE_SIZE << order);
	}

	kernel_poison_pages(page, 1 << order);

	 
	if (!skip_kasan_poison) {
		kasan_poison_pages(page, order, init);

		 
		if (kasan_has_integrated_init())
			init = false;
	}
	if (init)
		kernel_init_pages(page, 1 << order);

	 
	arch_free_page(page, order);

	debug_pagealloc_unmap_pages(page, 1 << order);

	return true;
}

 
static void free_pcppages_bulk(struct zone *zone, int count,
					struct per_cpu_pages *pcp,
					int pindex)
{
	unsigned long flags;
	unsigned int order;
	bool isolated_pageblocks;
	struct page *page;

	 
	count = min(pcp->count, count);

	 
	pindex = pindex - 1;

	spin_lock_irqsave(&zone->lock, flags);
	isolated_pageblocks = has_isolate_pageblock(zone);

	while (count > 0) {
		struct list_head *list;
		int nr_pages;

		 
		do {
			if (++pindex > NR_PCP_LISTS - 1)
				pindex = 0;
			list = &pcp->lists[pindex];
		} while (list_empty(list));

		order = pindex_to_order(pindex);
		nr_pages = 1 << order;
		do {
			int mt;

			page = list_last_entry(list, struct page, pcp_list);
			mt = get_pcppage_migratetype(page);

			 
			list_del(&page->pcp_list);
			count -= nr_pages;
			pcp->count -= nr_pages;

			 
			VM_BUG_ON_PAGE(is_migrate_isolate(mt), page);
			 
			if (unlikely(isolated_pageblocks))
				mt = get_pageblock_migratetype(page);

			__free_one_page(page, page_to_pfn(page), zone, order, mt, FPI_NONE);
			trace_mm_page_pcpu_drain(page, order, mt);
		} while (count > 0 && !list_empty(list));
	}

	spin_unlock_irqrestore(&zone->lock, flags);
}

static void free_one_page(struct zone *zone,
				struct page *page, unsigned long pfn,
				unsigned int order,
				int migratetype, fpi_t fpi_flags)
{
	unsigned long flags;

	spin_lock_irqsave(&zone->lock, flags);
	if (unlikely(has_isolate_pageblock(zone) ||
		is_migrate_isolate(migratetype))) {
		migratetype = get_pfnblock_migratetype(page, pfn);
	}
	__free_one_page(page, pfn, zone, order, migratetype, fpi_flags);
	spin_unlock_irqrestore(&zone->lock, flags);
}

static void __free_pages_ok(struct page *page, unsigned int order,
			    fpi_t fpi_flags)
{
	unsigned long flags;
	int migratetype;
	unsigned long pfn = page_to_pfn(page);
	struct zone *zone = page_zone(page);

	if (!free_pages_prepare(page, order, fpi_flags))
		return;

	 
	migratetype = get_pfnblock_migratetype(page, pfn);

	spin_lock_irqsave(&zone->lock, flags);
	if (unlikely(has_isolate_pageblock(zone) ||
		is_migrate_isolate(migratetype))) {
		migratetype = get_pfnblock_migratetype(page, pfn);
	}
	__free_one_page(page, pfn, zone, order, migratetype, fpi_flags);
	spin_unlock_irqrestore(&zone->lock, flags);

	__count_vm_events(PGFREE, 1 << order);
}

void __free_pages_core(struct page *page, unsigned int order)
{
	unsigned int nr_pages = 1 << order;
	struct page *p = page;
	unsigned int loop;

	 
	prefetchw(p);
	for (loop = 0; loop < (nr_pages - 1); loop++, p++) {
		prefetchw(p + 1);
		__ClearPageReserved(p);
		set_page_count(p, 0);
	}
	__ClearPageReserved(p);
	set_page_count(p, 0);

	atomic_long_add(nr_pages, &page_zone(page)->managed_pages);

	if (page_contains_unaccepted(page, order)) {
		if (order == MAX_ORDER && __free_unaccepted(page))
			return;

		accept_page(page, order);
	}

	 
	__free_pages_ok(page, order, FPI_TO_TAIL);
}

 
struct page *__pageblock_pfn_to_page(unsigned long start_pfn,
				     unsigned long end_pfn, struct zone *zone)
{
	struct page *start_page;
	struct page *end_page;

	 
	end_pfn--;

	if (!pfn_valid(end_pfn))
		return NULL;

	start_page = pfn_to_online_page(start_pfn);
	if (!start_page)
		return NULL;

	if (page_zone(start_page) != zone)
		return NULL;

	end_page = pfn_to_page(end_pfn);

	 
	if (page_zone_id(start_page) != page_zone_id(end_page))
		return NULL;

	return start_page;
}

 
static inline void expand(struct zone *zone, struct page *page,
	int low, int high, int migratetype)
{
	unsigned long size = 1 << high;

	while (high > low) {
		high--;
		size >>= 1;
		VM_BUG_ON_PAGE(bad_range(zone, &page[size]), &page[size]);

		 
		if (set_page_guard(zone, &page[size], high, migratetype))
			continue;

		add_to_free_list(&page[size], zone, high, migratetype);
		set_buddy_order(&page[size], high);
	}
}

static void check_new_page_bad(struct page *page)
{
	if (unlikely(page->flags & __PG_HWPOISON)) {
		 
		page_mapcount_reset(page);  
		return;
	}

	bad_page(page,
		 page_bad_reason(page, PAGE_FLAGS_CHECK_AT_PREP));
}

 
static int check_new_page(struct page *page)
{
	if (likely(page_expected_state(page,
				PAGE_FLAGS_CHECK_AT_PREP|__PG_HWPOISON)))
		return 0;

	check_new_page_bad(page);
	return 1;
}

static inline bool check_new_pages(struct page *page, unsigned int order)
{
	if (is_check_pages_enabled()) {
		for (int i = 0; i < (1 << order); i++) {
			struct page *p = page + i;

			if (check_new_page(p))
				return true;
		}
	}

	return false;
}

static inline bool should_skip_kasan_unpoison(gfp_t flags)
{
	 
	if (IS_ENABLED(CONFIG_KASAN_GENERIC) ||
	    IS_ENABLED(CONFIG_KASAN_SW_TAGS))
		return false;

	 
	if (!kasan_hw_tags_enabled())
		return true;

	 
	return flags & __GFP_SKIP_KASAN;
}

static inline bool should_skip_init(gfp_t flags)
{
	 
	if (!kasan_hw_tags_enabled())
		return false;

	 
	return (flags & __GFP_SKIP_ZERO);
}

inline void post_alloc_hook(struct page *page, unsigned int order,
				gfp_t gfp_flags)
{
	bool init = !want_init_on_free() && want_init_on_alloc(gfp_flags) &&
			!should_skip_init(gfp_flags);
	bool zero_tags = init && (gfp_flags & __GFP_ZEROTAGS);
	int i;

	set_page_private(page, 0);
	set_page_refcounted(page);

	arch_alloc_page(page, order);
	debug_pagealloc_map_pages(page, 1 << order);

	 
	kernel_unpoison_pages(page, 1 << order);

	 

	 
	if (zero_tags) {
		 
		for (i = 0; i != 1 << order; ++i)
			tag_clear_highpage(page + i);

		 
		init = false;
	}
	if (!should_skip_kasan_unpoison(gfp_flags) &&
	    kasan_unpoison_pages(page, order, init)) {
		 
		if (kasan_has_integrated_init())
			init = false;
	} else {
		 
		for (i = 0; i != 1 << order; ++i)
			page_kasan_tag_reset(page + i);
	}
	 
	if (init)
		kernel_init_pages(page, 1 << order);

	set_page_owner(page, order, gfp_flags);
	page_table_check_alloc(page, order);
}

static void prep_new_page(struct page *page, unsigned int order, gfp_t gfp_flags,
							unsigned int alloc_flags)
{
	post_alloc_hook(page, order, gfp_flags);

	if (order && (gfp_flags & __GFP_COMP))
		prep_compound_page(page, order);

	 
	if (alloc_flags & ALLOC_NO_WATERMARKS)
		set_page_pfmemalloc(page);
	else
		clear_page_pfmemalloc(page);
}

 
static __always_inline
struct page *__rmqueue_smallest(struct zone *zone, unsigned int order,
						int migratetype)
{
	unsigned int current_order;
	struct free_area *area;
	struct page *page;

	 
	for (current_order = order; current_order <= MAX_ORDER; ++current_order) {
		area = &(zone->free_area[current_order]);
		page = get_page_from_free_area(area, migratetype);
		if (!page)
			continue;
		del_page_from_free_list(page, zone, current_order);
		expand(zone, page, order, current_order, migratetype);
		set_pcppage_migratetype(page, migratetype);
		trace_mm_page_alloc_zone_locked(page, order, migratetype,
				pcp_allowed_order(order) &&
				migratetype < MIGRATE_PCPTYPES);
		return page;
	}

	return NULL;
}


 
static int fallbacks[MIGRATE_TYPES][MIGRATE_PCPTYPES - 1] = {
	[MIGRATE_UNMOVABLE]   = { MIGRATE_RECLAIMABLE, MIGRATE_MOVABLE   },
	[MIGRATE_MOVABLE]     = { MIGRATE_RECLAIMABLE, MIGRATE_UNMOVABLE },
	[MIGRATE_RECLAIMABLE] = { MIGRATE_UNMOVABLE,   MIGRATE_MOVABLE   },
};

#ifdef CONFIG_CMA
static __always_inline struct page *__rmqueue_cma_fallback(struct zone *zone,
					unsigned int order)
{
	return __rmqueue_smallest(zone, order, MIGRATE_CMA);
}
#else
static inline struct page *__rmqueue_cma_fallback(struct zone *zone,
					unsigned int order) { return NULL; }
#endif

 
static int move_freepages(struct zone *zone,
			  unsigned long start_pfn, unsigned long end_pfn,
			  int migratetype, int *num_movable)
{
	struct page *page;
	unsigned long pfn;
	unsigned int order;
	int pages_moved = 0;

	for (pfn = start_pfn; pfn <= end_pfn;) {
		page = pfn_to_page(pfn);
		if (!PageBuddy(page)) {
			 
			if (num_movable &&
					(PageLRU(page) || __PageMovable(page)))
				(*num_movable)++;
			pfn++;
			continue;
		}

		 
		VM_BUG_ON_PAGE(page_to_nid(page) != zone_to_nid(zone), page);
		VM_BUG_ON_PAGE(page_zone(page) != zone, page);

		order = buddy_order(page);
		move_to_free_list(page, zone, order, migratetype);
		pfn += 1 << order;
		pages_moved += 1 << order;
	}

	return pages_moved;
}

int move_freepages_block(struct zone *zone, struct page *page,
				int migratetype, int *num_movable)
{
	unsigned long start_pfn, end_pfn, pfn;

	if (num_movable)
		*num_movable = 0;

	pfn = page_to_pfn(page);
	start_pfn = pageblock_start_pfn(pfn);
	end_pfn = pageblock_end_pfn(pfn) - 1;

	 
	if (!zone_spans_pfn(zone, start_pfn))
		start_pfn = pfn;
	if (!zone_spans_pfn(zone, end_pfn))
		return 0;

	return move_freepages(zone, start_pfn, end_pfn, migratetype,
								num_movable);
}

static void change_pageblock_range(struct page *pageblock_page,
					int start_order, int migratetype)
{
	int nr_pageblocks = 1 << (start_order - pageblock_order);

	while (nr_pageblocks--) {
		set_pageblock_migratetype(pageblock_page, migratetype);
		pageblock_page += pageblock_nr_pages;
	}
}

 
static bool can_steal_fallback(unsigned int order, int start_mt)
{
	 
	if (order >= pageblock_order)
		return true;

	if (order >= pageblock_order / 2 ||
		start_mt == MIGRATE_RECLAIMABLE ||
		start_mt == MIGRATE_UNMOVABLE ||
		page_group_by_mobility_disabled)
		return true;

	return false;
}

static inline bool boost_watermark(struct zone *zone)
{
	unsigned long max_boost;

	if (!watermark_boost_factor)
		return false;
	 
	if ((pageblock_nr_pages * 4) > zone_managed_pages(zone))
		return false;

	max_boost = mult_frac(zone->_watermark[WMARK_HIGH],
			watermark_boost_factor, 10000);

	 
	if (!max_boost)
		return false;

	max_boost = max(pageblock_nr_pages, max_boost);

	zone->watermark_boost = min(zone->watermark_boost + pageblock_nr_pages,
		max_boost);

	return true;
}

 
static void steal_suitable_fallback(struct zone *zone, struct page *page,
		unsigned int alloc_flags, int start_type, bool whole_block)
{
	unsigned int current_order = buddy_order(page);
	int free_pages, movable_pages, alike_pages;
	int old_block_type;

	old_block_type = get_pageblock_migratetype(page);

	 
	if (is_migrate_highatomic(old_block_type))
		goto single_page;

	 
	if (current_order >= pageblock_order) {
		change_pageblock_range(page, current_order, start_type);
		goto single_page;
	}

	 
	if (boost_watermark(zone) && (alloc_flags & ALLOC_KSWAPD))
		set_bit(ZONE_BOOSTED_WATERMARK, &zone->flags);

	 
	if (!whole_block)
		goto single_page;

	free_pages = move_freepages_block(zone, page, start_type,
						&movable_pages);
	 
	if (!free_pages)
		goto single_page;

	 
	if (start_type == MIGRATE_MOVABLE) {
		alike_pages = movable_pages;
	} else {
		 
		if (old_block_type == MIGRATE_MOVABLE)
			alike_pages = pageblock_nr_pages
						- (free_pages + movable_pages);
		else
			alike_pages = 0;
	}
	 
	if (free_pages + alike_pages >= (1 << (pageblock_order-1)) ||
			page_group_by_mobility_disabled)
		set_pageblock_migratetype(page, start_type);

	return;

single_page:
	move_to_free_list(page, zone, current_order, start_type);
}

 
int find_suitable_fallback(struct free_area *area, unsigned int order,
			int migratetype, bool only_stealable, bool *can_steal)
{
	int i;
	int fallback_mt;

	if (area->nr_free == 0)
		return -1;

	*can_steal = false;
	for (i = 0; i < MIGRATE_PCPTYPES - 1 ; i++) {
		fallback_mt = fallbacks[migratetype][i];
		if (free_area_empty(area, fallback_mt))
			continue;

		if (can_steal_fallback(order, migratetype))
			*can_steal = true;

		if (!only_stealable)
			return fallback_mt;

		if (*can_steal)
			return fallback_mt;
	}

	return -1;
}

 
static void reserve_highatomic_pageblock(struct page *page, struct zone *zone)
{
	int mt;
	unsigned long max_managed, flags;

	 
	max_managed = (zone_managed_pages(zone) / 100) + pageblock_nr_pages;
	if (zone->nr_reserved_highatomic >= max_managed)
		return;

	spin_lock_irqsave(&zone->lock, flags);

	 
	if (zone->nr_reserved_highatomic >= max_managed)
		goto out_unlock;

	 
	mt = get_pageblock_migratetype(page);
	 
	if (migratetype_is_mergeable(mt)) {
		zone->nr_reserved_highatomic += pageblock_nr_pages;
		set_pageblock_migratetype(page, MIGRATE_HIGHATOMIC);
		move_freepages_block(zone, page, MIGRATE_HIGHATOMIC, NULL);
	}

out_unlock:
	spin_unlock_irqrestore(&zone->lock, flags);
}

 
static bool unreserve_highatomic_pageblock(const struct alloc_context *ac,
						bool force)
{
	struct zonelist *zonelist = ac->zonelist;
	unsigned long flags;
	struct zoneref *z;
	struct zone *zone;
	struct page *page;
	int order;
	bool ret;

	for_each_zone_zonelist_nodemask(zone, z, zonelist, ac->highest_zoneidx,
								ac->nodemask) {
		 
		if (!force && zone->nr_reserved_highatomic <=
					pageblock_nr_pages)
			continue;

		spin_lock_irqsave(&zone->lock, flags);
		for (order = 0; order <= MAX_ORDER; order++) {
			struct free_area *area = &(zone->free_area[order]);

			page = get_page_from_free_area(area, MIGRATE_HIGHATOMIC);
			if (!page)
				continue;

			 
			if (is_migrate_highatomic_page(page)) {
				 
				zone->nr_reserved_highatomic -= min(
						pageblock_nr_pages,
						zone->nr_reserved_highatomic);
			}

			 
			set_pageblock_migratetype(page, ac->migratetype);
			ret = move_freepages_block(zone, page, ac->migratetype,
									NULL);
			if (ret) {
				spin_unlock_irqrestore(&zone->lock, flags);
				return ret;
			}
		}
		spin_unlock_irqrestore(&zone->lock, flags);
	}

	return false;
}

 
static __always_inline bool
__rmqueue_fallback(struct zone *zone, int order, int start_migratetype,
						unsigned int alloc_flags)
{
	struct free_area *area;
	int current_order;
	int min_order = order;
	struct page *page;
	int fallback_mt;
	bool can_steal;

	 
	if (order < pageblock_order && alloc_flags & ALLOC_NOFRAGMENT)
		min_order = pageblock_order;

	 
	for (current_order = MAX_ORDER; current_order >= min_order;
				--current_order) {
		area = &(zone->free_area[current_order]);
		fallback_mt = find_suitable_fallback(area, current_order,
				start_migratetype, false, &can_steal);
		if (fallback_mt == -1)
			continue;

		 
		if (!can_steal && start_migratetype == MIGRATE_MOVABLE
					&& current_order > order)
			goto find_smallest;

		goto do_steal;
	}

	return false;

find_smallest:
	for (current_order = order; current_order <= MAX_ORDER;
							current_order++) {
		area = &(zone->free_area[current_order]);
		fallback_mt = find_suitable_fallback(area, current_order,
				start_migratetype, false, &can_steal);
		if (fallback_mt != -1)
			break;
	}

	 
	VM_BUG_ON(current_order > MAX_ORDER);

do_steal:
	page = get_page_from_free_area(area, fallback_mt);

	steal_suitable_fallback(zone, page, alloc_flags, start_migratetype,
								can_steal);

	trace_mm_page_alloc_extfrag(page, order, current_order,
		start_migratetype, fallback_mt);

	return true;

}

 
static __always_inline struct page *
__rmqueue(struct zone *zone, unsigned int order, int migratetype,
						unsigned int alloc_flags)
{
	struct page *page;

	if (IS_ENABLED(CONFIG_CMA)) {
		 
		if (alloc_flags & ALLOC_CMA &&
		    zone_page_state(zone, NR_FREE_CMA_PAGES) >
		    zone_page_state(zone, NR_FREE_PAGES) / 2) {
			page = __rmqueue_cma_fallback(zone, order);
			if (page)
				return page;
		}
	}
retry:
	page = __rmqueue_smallest(zone, order, migratetype);
	if (unlikely(!page)) {
		if (alloc_flags & ALLOC_CMA)
			page = __rmqueue_cma_fallback(zone, order);

		if (!page && __rmqueue_fallback(zone, order, migratetype,
								alloc_flags))
			goto retry;
	}
	return page;
}

 
static int rmqueue_bulk(struct zone *zone, unsigned int order,
			unsigned long count, struct list_head *list,
			int migratetype, unsigned int alloc_flags)
{
	unsigned long flags;
	int i;

	spin_lock_irqsave(&zone->lock, flags);
	for (i = 0; i < count; ++i) {
		struct page *page = __rmqueue(zone, order, migratetype,
								alloc_flags);
		if (unlikely(page == NULL))
			break;

		 
		list_add_tail(&page->pcp_list, list);
		if (is_migrate_cma(get_pcppage_migratetype(page)))
			__mod_zone_page_state(zone, NR_FREE_CMA_PAGES,
					      -(1 << order));
	}

	__mod_zone_page_state(zone, NR_FREE_PAGES, -(i << order));
	spin_unlock_irqrestore(&zone->lock, flags);

	return i;
}

#ifdef CONFIG_NUMA
 
void drain_zone_pages(struct zone *zone, struct per_cpu_pages *pcp)
{
	int to_drain, batch;

	batch = READ_ONCE(pcp->batch);
	to_drain = min(pcp->count, batch);
	if (to_drain > 0) {
		spin_lock(&pcp->lock);
		free_pcppages_bulk(zone, to_drain, pcp, 0);
		spin_unlock(&pcp->lock);
	}
}
#endif

 
static void drain_pages_zone(unsigned int cpu, struct zone *zone)
{
	struct per_cpu_pages *pcp;

	pcp = per_cpu_ptr(zone->per_cpu_pageset, cpu);
	if (pcp->count) {
		spin_lock(&pcp->lock);
		free_pcppages_bulk(zone, pcp->count, pcp, 0);
		spin_unlock(&pcp->lock);
	}
}

 
static void drain_pages(unsigned int cpu)
{
	struct zone *zone;

	for_each_populated_zone(zone) {
		drain_pages_zone(cpu, zone);
	}
}

 
void drain_local_pages(struct zone *zone)
{
	int cpu = smp_processor_id();

	if (zone)
		drain_pages_zone(cpu, zone);
	else
		drain_pages(cpu);
}

 
static void __drain_all_pages(struct zone *zone, bool force_all_cpus)
{
	int cpu;

	 
	static cpumask_t cpus_with_pcps;

	 
	if (unlikely(!mutex_trylock(&pcpu_drain_mutex))) {
		if (!zone)
			return;
		mutex_lock(&pcpu_drain_mutex);
	}

	 
	for_each_online_cpu(cpu) {
		struct per_cpu_pages *pcp;
		struct zone *z;
		bool has_pcps = false;

		if (force_all_cpus) {
			 
			has_pcps = true;
		} else if (zone) {
			pcp = per_cpu_ptr(zone->per_cpu_pageset, cpu);
			if (pcp->count)
				has_pcps = true;
		} else {
			for_each_populated_zone(z) {
				pcp = per_cpu_ptr(z->per_cpu_pageset, cpu);
				if (pcp->count) {
					has_pcps = true;
					break;
				}
			}
		}

		if (has_pcps)
			cpumask_set_cpu(cpu, &cpus_with_pcps);
		else
			cpumask_clear_cpu(cpu, &cpus_with_pcps);
	}

	for_each_cpu(cpu, &cpus_with_pcps) {
		if (zone)
			drain_pages_zone(cpu, zone);
		else
			drain_pages(cpu);
	}

	mutex_unlock(&pcpu_drain_mutex);
}

 
void drain_all_pages(struct zone *zone)
{
	__drain_all_pages(zone, false);
}

static bool free_unref_page_prepare(struct page *page, unsigned long pfn,
							unsigned int order)
{
	int migratetype;

	if (!free_pages_prepare(page, order, FPI_NONE))
		return false;

	migratetype = get_pfnblock_migratetype(page, pfn);
	set_pcppage_migratetype(page, migratetype);
	return true;
}

static int nr_pcp_free(struct per_cpu_pages *pcp, int high, bool free_high)
{
	int min_nr_free, max_nr_free;
	int batch = READ_ONCE(pcp->batch);

	 
	if (unlikely(free_high))
		return pcp->count;

	 
	if (unlikely(high < batch))
		return 1;

	 
	min_nr_free = batch;
	max_nr_free = high - batch;

	 
	batch <<= pcp->free_factor;
	if (batch < max_nr_free)
		pcp->free_factor++;
	batch = clamp(batch, min_nr_free, max_nr_free);

	return batch;
}

static int nr_pcp_high(struct per_cpu_pages *pcp, struct zone *zone,
		       bool free_high)
{
	int high = READ_ONCE(pcp->high);

	if (unlikely(!high || free_high))
		return 0;

	if (!test_bit(ZONE_RECLAIM_ACTIVE, &zone->flags))
		return high;

	 
	return min(READ_ONCE(pcp->batch) << 2, high);
}

static void free_unref_page_commit(struct zone *zone, struct per_cpu_pages *pcp,
				   struct page *page, int migratetype,
				   unsigned int order)
{
	int high;
	int pindex;
	bool free_high;

	__count_vm_events(PGFREE, 1 << order);
	pindex = order_to_pindex(migratetype, order);
	list_add(&page->pcp_list, &pcp->lists[pindex]);
	pcp->count += 1 << order;

	 
	free_high = (pcp->free_factor && order && order <= PAGE_ALLOC_COSTLY_ORDER);

	high = nr_pcp_high(pcp, zone, free_high);
	if (pcp->count >= high) {
		free_pcppages_bulk(zone, nr_pcp_free(pcp, high, free_high), pcp, pindex);
	}
}

 
void free_unref_page(struct page *page, unsigned int order)
{
	unsigned long __maybe_unused UP_flags;
	struct per_cpu_pages *pcp;
	struct zone *zone;
	unsigned long pfn = page_to_pfn(page);
	int migratetype, pcpmigratetype;

	if (!free_unref_page_prepare(page, pfn, order))
		return;

	 
	migratetype = pcpmigratetype = get_pcppage_migratetype(page);
	if (unlikely(migratetype >= MIGRATE_PCPTYPES)) {
		if (unlikely(is_migrate_isolate(migratetype))) {
			free_one_page(page_zone(page), page, pfn, order, migratetype, FPI_NONE);
			return;
		}
		pcpmigratetype = MIGRATE_MOVABLE;
	}

	zone = page_zone(page);
	pcp_trylock_prepare(UP_flags);
	pcp = pcp_spin_trylock(zone->per_cpu_pageset);
	if (pcp) {
		free_unref_page_commit(zone, pcp, page, pcpmigratetype, order);
		pcp_spin_unlock(pcp);
	} else {
		free_one_page(zone, page, pfn, order, migratetype, FPI_NONE);
	}
	pcp_trylock_finish(UP_flags);
}

 
void free_unref_page_list(struct list_head *list)
{
	unsigned long __maybe_unused UP_flags;
	struct page *page, *next;
	struct per_cpu_pages *pcp = NULL;
	struct zone *locked_zone = NULL;
	int batch_count = 0;
	int migratetype;

	 
	list_for_each_entry_safe(page, next, list, lru) {
		unsigned long pfn = page_to_pfn(page);
		if (!free_unref_page_prepare(page, pfn, 0)) {
			list_del(&page->lru);
			continue;
		}

		 
		migratetype = get_pcppage_migratetype(page);
		if (unlikely(is_migrate_isolate(migratetype))) {
			list_del(&page->lru);
			free_one_page(page_zone(page), page, pfn, 0, migratetype, FPI_NONE);
			continue;
		}
	}

	list_for_each_entry_safe(page, next, list, lru) {
		struct zone *zone = page_zone(page);

		list_del(&page->lru);
		migratetype = get_pcppage_migratetype(page);

		 
		if (zone != locked_zone || batch_count == SWAP_CLUSTER_MAX) {
			if (pcp) {
				pcp_spin_unlock(pcp);
				pcp_trylock_finish(UP_flags);
			}

			batch_count = 0;

			 
			pcp_trylock_prepare(UP_flags);
			pcp = pcp_spin_trylock(zone->per_cpu_pageset);
			if (unlikely(!pcp)) {
				pcp_trylock_finish(UP_flags);
				free_one_page(zone, page, page_to_pfn(page),
					      0, migratetype, FPI_NONE);
				locked_zone = NULL;
				continue;
			}
			locked_zone = zone;
		}

		 
		if (unlikely(migratetype >= MIGRATE_PCPTYPES))
			migratetype = MIGRATE_MOVABLE;

		trace_mm_page_free_batched(page);
		free_unref_page_commit(zone, pcp, page, migratetype, 0);
		batch_count++;
	}

	if (pcp) {
		pcp_spin_unlock(pcp);
		pcp_trylock_finish(UP_flags);
	}
}

 
void split_page(struct page *page, unsigned int order)
{
	int i;

	VM_BUG_ON_PAGE(PageCompound(page), page);
	VM_BUG_ON_PAGE(!page_count(page), page);

	for (i = 1; i < (1 << order); i++)
		set_page_refcounted(page + i);
	split_page_owner(page, 1 << order);
	split_page_memcg(page, 1 << order);
}
EXPORT_SYMBOL_GPL(split_page);

int __isolate_free_page(struct page *page, unsigned int order)
{
	struct zone *zone = page_zone(page);
	int mt = get_pageblock_migratetype(page);

	if (!is_migrate_isolate(mt)) {
		unsigned long watermark;
		 
		watermark = zone->_watermark[WMARK_MIN] + (1UL << order);
		if (!zone_watermark_ok(zone, 0, watermark, 0, ALLOC_CMA))
			return 0;

		__mod_zone_freepage_state(zone, -(1UL << order), mt);
	}

	del_page_from_free_list(page, zone, order);

	 
	if (order >= pageblock_order - 1) {
		struct page *endpage = page + (1 << order) - 1;
		for (; page < endpage; page += pageblock_nr_pages) {
			int mt = get_pageblock_migratetype(page);
			 
			if (migratetype_is_mergeable(mt))
				set_pageblock_migratetype(page,
							  MIGRATE_MOVABLE);
		}
	}

	return 1UL << order;
}

 
void __putback_isolated_page(struct page *page, unsigned int order, int mt)
{
	struct zone *zone = page_zone(page);

	 
	lockdep_assert_held(&zone->lock);

	 
	__free_one_page(page, page_to_pfn(page), zone, order, mt,
			FPI_SKIP_REPORT_NOTIFY | FPI_TO_TAIL);
}

 
static inline void zone_statistics(struct zone *preferred_zone, struct zone *z,
				   long nr_account)
{
#ifdef CONFIG_NUMA
	enum numa_stat_item local_stat = NUMA_LOCAL;

	 
	if (!static_branch_likely(&vm_numa_stat_key))
		return;

	if (zone_to_nid(z) != numa_node_id())
		local_stat = NUMA_OTHER;

	if (zone_to_nid(z) == zone_to_nid(preferred_zone))
		__count_numa_events(z, NUMA_HIT, nr_account);
	else {
		__count_numa_events(z, NUMA_MISS, nr_account);
		__count_numa_events(preferred_zone, NUMA_FOREIGN, nr_account);
	}
	__count_numa_events(z, local_stat, nr_account);
#endif
}

static __always_inline
struct page *rmqueue_buddy(struct zone *preferred_zone, struct zone *zone,
			   unsigned int order, unsigned int alloc_flags,
			   int migratetype)
{
	struct page *page;
	unsigned long flags;

	do {
		page = NULL;
		spin_lock_irqsave(&zone->lock, flags);
		if (alloc_flags & ALLOC_HIGHATOMIC)
			page = __rmqueue_smallest(zone, order, MIGRATE_HIGHATOMIC);
		if (!page) {
			page = __rmqueue(zone, order, migratetype, alloc_flags);

			 
			if (!page && (alloc_flags & ALLOC_OOM))
				page = __rmqueue_smallest(zone, order, MIGRATE_HIGHATOMIC);

			if (!page) {
				spin_unlock_irqrestore(&zone->lock, flags);
				return NULL;
			}
		}
		__mod_zone_freepage_state(zone, -(1 << order),
					  get_pcppage_migratetype(page));
		spin_unlock_irqrestore(&zone->lock, flags);
	} while (check_new_pages(page, order));

	__count_zid_vm_events(PGALLOC, page_zonenum(page), 1 << order);
	zone_statistics(preferred_zone, zone, 1);

	return page;
}

 
static inline
struct page *__rmqueue_pcplist(struct zone *zone, unsigned int order,
			int migratetype,
			unsigned int alloc_flags,
			struct per_cpu_pages *pcp,
			struct list_head *list)
{
	struct page *page;

	do {
		if (list_empty(list)) {
			int batch = READ_ONCE(pcp->batch);
			int alloced;

			 
			if (batch > 1)
				batch = max(batch >> order, 2);
			alloced = rmqueue_bulk(zone, order,
					batch, list,
					migratetype, alloc_flags);

			pcp->count += alloced << order;
			if (unlikely(list_empty(list)))
				return NULL;
		}

		page = list_first_entry(list, struct page, pcp_list);
		list_del(&page->pcp_list);
		pcp->count -= 1 << order;
	} while (check_new_pages(page, order));

	return page;
}

 
static struct page *rmqueue_pcplist(struct zone *preferred_zone,
			struct zone *zone, unsigned int order,
			int migratetype, unsigned int alloc_flags)
{
	struct per_cpu_pages *pcp;
	struct list_head *list;
	struct page *page;
	unsigned long __maybe_unused UP_flags;

	 
	pcp_trylock_prepare(UP_flags);
	pcp = pcp_spin_trylock(zone->per_cpu_pageset);
	if (!pcp) {
		pcp_trylock_finish(UP_flags);
		return NULL;
	}

	 
	pcp->free_factor >>= 1;
	list = &pcp->lists[order_to_pindex(migratetype, order)];
	page = __rmqueue_pcplist(zone, order, migratetype, alloc_flags, pcp, list);
	pcp_spin_unlock(pcp);
	pcp_trylock_finish(UP_flags);
	if (page) {
		__count_zid_vm_events(PGALLOC, page_zonenum(page), 1 << order);
		zone_statistics(preferred_zone, zone, 1);
	}
	return page;
}

 

 
__no_sanitize_memory
static inline
struct page *rmqueue(struct zone *preferred_zone,
			struct zone *zone, unsigned int order,
			gfp_t gfp_flags, unsigned int alloc_flags,
			int migratetype)
{
	struct page *page;

	 
	WARN_ON_ONCE((gfp_flags & __GFP_NOFAIL) && (order > 1));

	if (likely(pcp_allowed_order(order))) {
		page = rmqueue_pcplist(preferred_zone, zone, order,
				       migratetype, alloc_flags);
		if (likely(page))
			goto out;
	}

	page = rmqueue_buddy(preferred_zone, zone, order, alloc_flags,
							migratetype);

out:
	 
	if ((alloc_flags & ALLOC_KSWAPD) &&
	    unlikely(test_bit(ZONE_BOOSTED_WATERMARK, &zone->flags))) {
		clear_bit(ZONE_BOOSTED_WATERMARK, &zone->flags);
		wakeup_kswapd(zone, 0, 0, zone_idx(zone));
	}

	VM_BUG_ON_PAGE(page && bad_range(zone, page), page);
	return page;
}

noinline bool should_fail_alloc_page(gfp_t gfp_mask, unsigned int order)
{
	return __should_fail_alloc_page(gfp_mask, order);
}
ALLOW_ERROR_INJECTION(should_fail_alloc_page, TRUE);

static inline long __zone_watermark_unusable_free(struct zone *z,
				unsigned int order, unsigned int alloc_flags)
{
	long unusable_free = (1 << order) - 1;

	 
	if (likely(!(alloc_flags & ALLOC_RESERVES)))
		unusable_free += z->nr_reserved_highatomic;

#ifdef CONFIG_CMA
	 
	if (!(alloc_flags & ALLOC_CMA))
		unusable_free += zone_page_state(z, NR_FREE_CMA_PAGES);
#endif
#ifdef CONFIG_UNACCEPTED_MEMORY
	unusable_free += zone_page_state(z, NR_UNACCEPTED);
#endif

	return unusable_free;
}

 
bool __zone_watermark_ok(struct zone *z, unsigned int order, unsigned long mark,
			 int highest_zoneidx, unsigned int alloc_flags,
			 long free_pages)
{
	long min = mark;
	int o;

	 
	free_pages -= __zone_watermark_unusable_free(z, order, alloc_flags);

	if (unlikely(alloc_flags & ALLOC_RESERVES)) {
		 
		if (alloc_flags & ALLOC_MIN_RESERVE) {
			min -= min / 2;

			 
			if (alloc_flags & ALLOC_NON_BLOCK)
				min -= min / 4;
		}

		 
		if (alloc_flags & ALLOC_OOM)
			min -= min / 2;
	}

	 
	if (free_pages <= min + z->lowmem_reserve[highest_zoneidx])
		return false;

	 
	if (!order)
		return true;

	 
	for (o = order; o <= MAX_ORDER; o++) {
		struct free_area *area = &z->free_area[o];
		int mt;

		if (!area->nr_free)
			continue;

		for (mt = 0; mt < MIGRATE_PCPTYPES; mt++) {
			if (!free_area_empty(area, mt))
				return true;
		}

#ifdef CONFIG_CMA
		if ((alloc_flags & ALLOC_CMA) &&
		    !free_area_empty(area, MIGRATE_CMA)) {
			return true;
		}
#endif
		if ((alloc_flags & (ALLOC_HIGHATOMIC|ALLOC_OOM)) &&
		    !free_area_empty(area, MIGRATE_HIGHATOMIC)) {
			return true;
		}
	}
	return false;
}

bool zone_watermark_ok(struct zone *z, unsigned int order, unsigned long mark,
		      int highest_zoneidx, unsigned int alloc_flags)
{
	return __zone_watermark_ok(z, order, mark, highest_zoneidx, alloc_flags,
					zone_page_state(z, NR_FREE_PAGES));
}

static inline bool zone_watermark_fast(struct zone *z, unsigned int order,
				unsigned long mark, int highest_zoneidx,
				unsigned int alloc_flags, gfp_t gfp_mask)
{
	long free_pages;

	free_pages = zone_page_state(z, NR_FREE_PAGES);

	 
	if (!order) {
		long usable_free;
		long reserved;

		usable_free = free_pages;
		reserved = __zone_watermark_unusable_free(z, 0, alloc_flags);

		 
		usable_free -= min(usable_free, reserved);
		if (usable_free > mark + z->lowmem_reserve[highest_zoneidx])
			return true;
	}

	if (__zone_watermark_ok(z, order, mark, highest_zoneidx, alloc_flags,
					free_pages))
		return true;

	 
	if (unlikely(!order && (alloc_flags & ALLOC_MIN_RESERVE) && z->watermark_boost
		&& ((alloc_flags & ALLOC_WMARK_MASK) == WMARK_MIN))) {
		mark = z->_watermark[WMARK_MIN];
		return __zone_watermark_ok(z, order, mark, highest_zoneidx,
					alloc_flags, free_pages);
	}

	return false;
}

bool zone_watermark_ok_safe(struct zone *z, unsigned int order,
			unsigned long mark, int highest_zoneidx)
{
	long free_pages = zone_page_state(z, NR_FREE_PAGES);

	if (z->percpu_drift_mark && free_pages < z->percpu_drift_mark)
		free_pages = zone_page_state_snapshot(z, NR_FREE_PAGES);

	return __zone_watermark_ok(z, order, mark, highest_zoneidx, 0,
								free_pages);
}

#ifdef CONFIG_NUMA
int __read_mostly node_reclaim_distance = RECLAIM_DISTANCE;

static bool zone_allows_reclaim(struct zone *local_zone, struct zone *zone)
{
	return node_distance(zone_to_nid(local_zone), zone_to_nid(zone)) <=
				node_reclaim_distance;
}
#else	 
static bool zone_allows_reclaim(struct zone *local_zone, struct zone *zone)
{
	return true;
}
#endif	 

 
static inline unsigned int
alloc_flags_nofragment(struct zone *zone, gfp_t gfp_mask)
{
	unsigned int alloc_flags;

	 
	alloc_flags = (__force int) (gfp_mask & __GFP_KSWAPD_RECLAIM);

#ifdef CONFIG_ZONE_DMA32
	if (!zone)
		return alloc_flags;

	if (zone_idx(zone) != ZONE_NORMAL)
		return alloc_flags;

	 
	BUILD_BUG_ON(ZONE_NORMAL - ZONE_DMA32 != 1);
	if (nr_online_nodes > 1 && !populated_zone(--zone))
		return alloc_flags;

	alloc_flags |= ALLOC_NOFRAGMENT;
#endif  
	return alloc_flags;
}

 
static inline unsigned int gfp_to_alloc_flags_cma(gfp_t gfp_mask,
						  unsigned int alloc_flags)
{
#ifdef CONFIG_CMA
	if (gfp_migratetype(gfp_mask) == MIGRATE_MOVABLE)
		alloc_flags |= ALLOC_CMA;
#endif
	return alloc_flags;
}

 
static struct page *
get_page_from_freelist(gfp_t gfp_mask, unsigned int order, int alloc_flags,
						const struct alloc_context *ac)
{
	struct zoneref *z;
	struct zone *zone;
	struct pglist_data *last_pgdat = NULL;
	bool last_pgdat_dirty_ok = false;
	bool no_fallback;

retry:
	 
	no_fallback = alloc_flags & ALLOC_NOFRAGMENT;
	z = ac->preferred_zoneref;
	for_next_zone_zonelist_nodemask(zone, z, ac->highest_zoneidx,
					ac->nodemask) {
		struct page *page;
		unsigned long mark;

		if (cpusets_enabled() &&
			(alloc_flags & ALLOC_CPUSET) &&
			!__cpuset_zone_allowed(zone, gfp_mask))
				continue;
		 
		if (ac->spread_dirty_pages) {
			if (last_pgdat != zone->zone_pgdat) {
				last_pgdat = zone->zone_pgdat;
				last_pgdat_dirty_ok = node_dirty_ok(zone->zone_pgdat);
			}

			if (!last_pgdat_dirty_ok)
				continue;
		}

		if (no_fallback && nr_online_nodes > 1 &&
		    zone != ac->preferred_zoneref->zone) {
			int local_nid;

			 
			local_nid = zone_to_nid(ac->preferred_zoneref->zone);
			if (zone_to_nid(zone) != local_nid) {
				alloc_flags &= ~ALLOC_NOFRAGMENT;
				goto retry;
			}
		}

		mark = wmark_pages(zone, alloc_flags & ALLOC_WMARK_MASK);
		if (!zone_watermark_fast(zone, order, mark,
				       ac->highest_zoneidx, alloc_flags,
				       gfp_mask)) {
			int ret;

			if (has_unaccepted_memory()) {
				if (try_to_accept_memory(zone, order))
					goto try_this_zone;
			}

#ifdef CONFIG_DEFERRED_STRUCT_PAGE_INIT
			 
			if (deferred_pages_enabled()) {
				if (_deferred_grow_zone(zone, order))
					goto try_this_zone;
			}
#endif
			 
			BUILD_BUG_ON(ALLOC_NO_WATERMARKS < NR_WMARK);
			if (alloc_flags & ALLOC_NO_WATERMARKS)
				goto try_this_zone;

			if (!node_reclaim_enabled() ||
			    !zone_allows_reclaim(ac->preferred_zoneref->zone, zone))
				continue;

			ret = node_reclaim(zone->zone_pgdat, gfp_mask, order);
			switch (ret) {
			case NODE_RECLAIM_NOSCAN:
				 
				continue;
			case NODE_RECLAIM_FULL:
				 
				continue;
			default:
				 
				if (zone_watermark_ok(zone, order, mark,
					ac->highest_zoneidx, alloc_flags))
					goto try_this_zone;

				continue;
			}
		}

try_this_zone:
		page = rmqueue(ac->preferred_zoneref->zone, zone, order,
				gfp_mask, alloc_flags, ac->migratetype);
		if (page) {
			prep_new_page(page, order, gfp_mask, alloc_flags);

			 
			if (unlikely(alloc_flags & ALLOC_HIGHATOMIC))
				reserve_highatomic_pageblock(page, zone);

			return page;
		} else {
			if (has_unaccepted_memory()) {
				if (try_to_accept_memory(zone, order))
					goto try_this_zone;
			}

#ifdef CONFIG_DEFERRED_STRUCT_PAGE_INIT
			 
			if (deferred_pages_enabled()) {
				if (_deferred_grow_zone(zone, order))
					goto try_this_zone;
			}
#endif
		}
	}

	 
	if (no_fallback) {
		alloc_flags &= ~ALLOC_NOFRAGMENT;
		goto retry;
	}

	return NULL;
}

static void warn_alloc_show_mem(gfp_t gfp_mask, nodemask_t *nodemask)
{
	unsigned int filter = SHOW_MEM_FILTER_NODES;

	 
	if (!(gfp_mask & __GFP_NOMEMALLOC))
		if (tsk_is_oom_victim(current) ||
		    (current->flags & (PF_MEMALLOC | PF_EXITING)))
			filter &= ~SHOW_MEM_FILTER_NODES;
	if (!in_task() || !(gfp_mask & __GFP_DIRECT_RECLAIM))
		filter &= ~SHOW_MEM_FILTER_NODES;

	__show_mem(filter, nodemask, gfp_zone(gfp_mask));
}

void warn_alloc(gfp_t gfp_mask, nodemask_t *nodemask, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	static DEFINE_RATELIMIT_STATE(nopage_rs, 10*HZ, 1);

	if ((gfp_mask & __GFP_NOWARN) ||
	     !__ratelimit(&nopage_rs) ||
	     ((gfp_mask & __GFP_DMA) && !has_managed_dma()))
		return;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	pr_warn("%s: %pV, mode:%#x(%pGg), nodemask=%*pbl",
			current->comm, &vaf, gfp_mask, &gfp_mask,
			nodemask_pr_args(nodemask));
	va_end(args);

	cpuset_print_current_mems_allowed();
	pr_cont("\n");
	dump_stack();
	warn_alloc_show_mem(gfp_mask, nodemask);
}

static inline struct page *
__alloc_pages_cpuset_fallback(gfp_t gfp_mask, unsigned int order,
			      unsigned int alloc_flags,
			      const struct alloc_context *ac)
{
	struct page *page;

	page = get_page_from_freelist(gfp_mask, order,
			alloc_flags|ALLOC_CPUSET, ac);
	 
	if (!page)
		page = get_page_from_freelist(gfp_mask, order,
				alloc_flags, ac);

	return page;
}

static inline struct page *
__alloc_pages_may_oom(gfp_t gfp_mask, unsigned int order,
	const struct alloc_context *ac, unsigned long *did_some_progress)
{
	struct oom_control oc = {
		.zonelist = ac->zonelist,
		.nodemask = ac->nodemask,
		.memcg = NULL,
		.gfp_mask = gfp_mask,
		.order = order,
	};
	struct page *page;

	*did_some_progress = 0;

	 
	if (!mutex_trylock(&oom_lock)) {
		*did_some_progress = 1;
		schedule_timeout_uninterruptible(1);
		return NULL;
	}

	 
	page = get_page_from_freelist((gfp_mask | __GFP_HARDWALL) &
				      ~__GFP_DIRECT_RECLAIM, order,
				      ALLOC_WMARK_HIGH|ALLOC_CPUSET, ac);
	if (page)
		goto out;

	 
	if (current->flags & PF_DUMPCORE)
		goto out;
	 
	if (order > PAGE_ALLOC_COSTLY_ORDER)
		goto out;
	 
	if (gfp_mask & (__GFP_RETRY_MAYFAIL | __GFP_THISNODE))
		goto out;
	 
	if (ac->highest_zoneidx < ZONE_NORMAL)
		goto out;
	if (pm_suspended_storage())
		goto out;
	 

	 
	if (out_of_memory(&oc) ||
	    WARN_ON_ONCE_GFP(gfp_mask & __GFP_NOFAIL, gfp_mask)) {
		*did_some_progress = 1;

		 
		if (gfp_mask & __GFP_NOFAIL)
			page = __alloc_pages_cpuset_fallback(gfp_mask, order,
					ALLOC_NO_WATERMARKS, ac);
	}
out:
	mutex_unlock(&oom_lock);
	return page;
}

 
#define MAX_COMPACT_RETRIES 16

#ifdef CONFIG_COMPACTION
 
static struct page *
__alloc_pages_direct_compact(gfp_t gfp_mask, unsigned int order,
		unsigned int alloc_flags, const struct alloc_context *ac,
		enum compact_priority prio, enum compact_result *compact_result)
{
	struct page *page = NULL;
	unsigned long pflags;
	unsigned int noreclaim_flag;

	if (!order)
		return NULL;

	psi_memstall_enter(&pflags);
	delayacct_compact_start();
	noreclaim_flag = memalloc_noreclaim_save();

	*compact_result = try_to_compact_pages(gfp_mask, order, alloc_flags, ac,
								prio, &page);

	memalloc_noreclaim_restore(noreclaim_flag);
	psi_memstall_leave(&pflags);
	delayacct_compact_end();

	if (*compact_result == COMPACT_SKIPPED)
		return NULL;
	 
	count_vm_event(COMPACTSTALL);

	 
	if (page)
		prep_new_page(page, order, gfp_mask, alloc_flags);

	 
	if (!page)
		page = get_page_from_freelist(gfp_mask, order, alloc_flags, ac);

	if (page) {
		struct zone *zone = page_zone(page);

		zone->compact_blockskip_flush = false;
		compaction_defer_reset(zone, order, true);
		count_vm_event(COMPACTSUCCESS);
		return page;
	}

	 
	count_vm_event(COMPACTFAIL);

	cond_resched();

	return NULL;
}

static inline bool
should_compact_retry(struct alloc_context *ac, int order, int alloc_flags,
		     enum compact_result compact_result,
		     enum compact_priority *compact_priority,
		     int *compaction_retries)
{
	int max_retries = MAX_COMPACT_RETRIES;
	int min_priority;
	bool ret = false;
	int retries = *compaction_retries;
	enum compact_priority priority = *compact_priority;

	if (!order)
		return false;

	if (fatal_signal_pending(current))
		return false;

	 
	if (compact_result == COMPACT_SKIPPED) {
		ret = compaction_zonelist_suitable(ac, order, alloc_flags);
		goto out;
	}

	 
	if (compact_result == COMPACT_SUCCESS) {
		 
		if (order > PAGE_ALLOC_COSTLY_ORDER)
			max_retries /= 4;

		if (++(*compaction_retries) <= max_retries) {
			ret = true;
			goto out;
		}
	}

	 
	min_priority = (order > PAGE_ALLOC_COSTLY_ORDER) ?
			MIN_COMPACT_COSTLY_PRIORITY : MIN_COMPACT_PRIORITY;

	if (*compact_priority > min_priority) {
		(*compact_priority)--;
		*compaction_retries = 0;
		ret = true;
	}
out:
	trace_compact_retry(order, priority, compact_result, retries, max_retries, ret);
	return ret;
}
#else
static inline struct page *
__alloc_pages_direct_compact(gfp_t gfp_mask, unsigned int order,
		unsigned int alloc_flags, const struct alloc_context *ac,
		enum compact_priority prio, enum compact_result *compact_result)
{
	*compact_result = COMPACT_SKIPPED;
	return NULL;
}

static inline bool
should_compact_retry(struct alloc_context *ac, unsigned int order, int alloc_flags,
		     enum compact_result compact_result,
		     enum compact_priority *compact_priority,
		     int *compaction_retries)
{
	struct zone *zone;
	struct zoneref *z;

	if (!order || order > PAGE_ALLOC_COSTLY_ORDER)
		return false;

	 
	for_each_zone_zonelist_nodemask(zone, z, ac->zonelist,
				ac->highest_zoneidx, ac->nodemask) {
		if (zone_watermark_ok(zone, 0, min_wmark_pages(zone),
					ac->highest_zoneidx, alloc_flags))
			return true;
	}
	return false;
}
#endif  

#ifdef CONFIG_LOCKDEP
static struct lockdep_map __fs_reclaim_map =
	STATIC_LOCKDEP_MAP_INIT("fs_reclaim", &__fs_reclaim_map);

static bool __need_reclaim(gfp_t gfp_mask)
{
	 
	if (!(gfp_mask & __GFP_DIRECT_RECLAIM))
		return false;

	 
	if (current->flags & PF_MEMALLOC)
		return false;

	if (gfp_mask & __GFP_NOLOCKDEP)
		return false;

	return true;
}

void __fs_reclaim_acquire(unsigned long ip)
{
	lock_acquire_exclusive(&__fs_reclaim_map, 0, 0, NULL, ip);
}

void __fs_reclaim_release(unsigned long ip)
{
	lock_release(&__fs_reclaim_map, ip);
}

void fs_reclaim_acquire(gfp_t gfp_mask)
{
	gfp_mask = current_gfp_context(gfp_mask);

	if (__need_reclaim(gfp_mask)) {
		if (gfp_mask & __GFP_FS)
			__fs_reclaim_acquire(_RET_IP_);

#ifdef CONFIG_MMU_NOTIFIER
		lock_map_acquire(&__mmu_notifier_invalidate_range_start_map);
		lock_map_release(&__mmu_notifier_invalidate_range_start_map);
#endif

	}
}
EXPORT_SYMBOL_GPL(fs_reclaim_acquire);

void fs_reclaim_release(gfp_t gfp_mask)
{
	gfp_mask = current_gfp_context(gfp_mask);

	if (__need_reclaim(gfp_mask)) {
		if (gfp_mask & __GFP_FS)
			__fs_reclaim_release(_RET_IP_);
	}
}
EXPORT_SYMBOL_GPL(fs_reclaim_release);
#endif

 
static DEFINE_SEQLOCK(zonelist_update_seq);

static unsigned int zonelist_iter_begin(void)
{
	if (IS_ENABLED(CONFIG_MEMORY_HOTREMOVE))
		return read_seqbegin(&zonelist_update_seq);

	return 0;
}

static unsigned int check_retry_zonelist(unsigned int seq)
{
	if (IS_ENABLED(CONFIG_MEMORY_HOTREMOVE))
		return read_seqretry(&zonelist_update_seq, seq);

	return seq;
}

 
static unsigned long
__perform_reclaim(gfp_t gfp_mask, unsigned int order,
					const struct alloc_context *ac)
{
	unsigned int noreclaim_flag;
	unsigned long progress;

	cond_resched();

	 
	cpuset_memory_pressure_bump();
	fs_reclaim_acquire(gfp_mask);
	noreclaim_flag = memalloc_noreclaim_save();

	progress = try_to_free_pages(ac->zonelist, order, gfp_mask,
								ac->nodemask);

	memalloc_noreclaim_restore(noreclaim_flag);
	fs_reclaim_release(gfp_mask);

	cond_resched();

	return progress;
}

 
static inline struct page *
__alloc_pages_direct_reclaim(gfp_t gfp_mask, unsigned int order,
		unsigned int alloc_flags, const struct alloc_context *ac,
		unsigned long *did_some_progress)
{
	struct page *page = NULL;
	unsigned long pflags;
	bool drained = false;

	psi_memstall_enter(&pflags);
	*did_some_progress = __perform_reclaim(gfp_mask, order, ac);
	if (unlikely(!(*did_some_progress)))
		goto out;

retry:
	page = get_page_from_freelist(gfp_mask, order, alloc_flags, ac);

	 
	if (!page && !drained) {
		unreserve_highatomic_pageblock(ac, false);
		drain_all_pages(NULL);
		drained = true;
		goto retry;
	}
out:
	psi_memstall_leave(&pflags);

	return page;
}

static void wake_all_kswapds(unsigned int order, gfp_t gfp_mask,
			     const struct alloc_context *ac)
{
	struct zoneref *z;
	struct zone *zone;
	pg_data_t *last_pgdat = NULL;
	enum zone_type highest_zoneidx = ac->highest_zoneidx;

	for_each_zone_zonelist_nodemask(zone, z, ac->zonelist, highest_zoneidx,
					ac->nodemask) {
		if (!managed_zone(zone))
			continue;
		if (last_pgdat != zone->zone_pgdat) {
			wakeup_kswapd(zone, gfp_mask, order, highest_zoneidx);
			last_pgdat = zone->zone_pgdat;
		}
	}
}

static inline unsigned int
gfp_to_alloc_flags(gfp_t gfp_mask, unsigned int order)
{
	unsigned int alloc_flags = ALLOC_WMARK_MIN | ALLOC_CPUSET;

	 
	BUILD_BUG_ON(__GFP_HIGH != (__force gfp_t) ALLOC_MIN_RESERVE);
	BUILD_BUG_ON(__GFP_KSWAPD_RECLAIM != (__force gfp_t) ALLOC_KSWAPD);

	 
	alloc_flags |= (__force int)
		(gfp_mask & (__GFP_HIGH | __GFP_KSWAPD_RECLAIM));

	if (!(gfp_mask & __GFP_DIRECT_RECLAIM)) {
		 
		if (!(gfp_mask & __GFP_NOMEMALLOC)) {
			alloc_flags |= ALLOC_NON_BLOCK;

			if (order > 0)
				alloc_flags |= ALLOC_HIGHATOMIC;
		}

		 
		if (alloc_flags & ALLOC_MIN_RESERVE)
			alloc_flags &= ~ALLOC_CPUSET;
	} else if (unlikely(rt_task(current)) && in_task())
		alloc_flags |= ALLOC_MIN_RESERVE;

	alloc_flags = gfp_to_alloc_flags_cma(gfp_mask, alloc_flags);

	return alloc_flags;
}

static bool oom_reserves_allowed(struct task_struct *tsk)
{
	if (!tsk_is_oom_victim(tsk))
		return false;

	 
	if (!IS_ENABLED(CONFIG_MMU) && !test_thread_flag(TIF_MEMDIE))
		return false;

	return true;
}

 
static inline int __gfp_pfmemalloc_flags(gfp_t gfp_mask)
{
	if (unlikely(gfp_mask & __GFP_NOMEMALLOC))
		return 0;
	if (gfp_mask & __GFP_MEMALLOC)
		return ALLOC_NO_WATERMARKS;
	if (in_serving_softirq() && (current->flags & PF_MEMALLOC))
		return ALLOC_NO_WATERMARKS;
	if (!in_interrupt()) {
		if (current->flags & PF_MEMALLOC)
			return ALLOC_NO_WATERMARKS;
		else if (oom_reserves_allowed(current))
			return ALLOC_OOM;
	}

	return 0;
}

bool gfp_pfmemalloc_allowed(gfp_t gfp_mask)
{
	return !!__gfp_pfmemalloc_flags(gfp_mask);
}

 
static inline bool
should_reclaim_retry(gfp_t gfp_mask, unsigned order,
		     struct alloc_context *ac, int alloc_flags,
		     bool did_some_progress, int *no_progress_loops)
{
	struct zone *zone;
	struct zoneref *z;
	bool ret = false;

	 
	if (did_some_progress && order <= PAGE_ALLOC_COSTLY_ORDER)
		*no_progress_loops = 0;
	else
		(*no_progress_loops)++;

	 
	if (*no_progress_loops > MAX_RECLAIM_RETRIES) {
		 
		return unreserve_highatomic_pageblock(ac, true);
	}

	 
	for_each_zone_zonelist_nodemask(zone, z, ac->zonelist,
				ac->highest_zoneidx, ac->nodemask) {
		unsigned long available;
		unsigned long reclaimable;
		unsigned long min_wmark = min_wmark_pages(zone);
		bool wmark;

		available = reclaimable = zone_reclaimable_pages(zone);
		available += zone_page_state_snapshot(zone, NR_FREE_PAGES);

		 
		wmark = __zone_watermark_ok(zone, order, min_wmark,
				ac->highest_zoneidx, alloc_flags, available);
		trace_reclaim_retry_zone(z, order, reclaimable,
				available, min_wmark, *no_progress_loops, wmark);
		if (wmark) {
			ret = true;
			break;
		}
	}

	 
	if (current->flags & PF_WQ_WORKER)
		schedule_timeout_uninterruptible(1);
	else
		cond_resched();
	return ret;
}

static inline bool
check_retry_cpuset(int cpuset_mems_cookie, struct alloc_context *ac)
{
	 
	if (cpusets_enabled() && ac->nodemask &&
			!cpuset_nodemask_valid_mems_allowed(ac->nodemask)) {
		ac->nodemask = NULL;
		return true;
	}

	 
	if (read_mems_allowed_retry(cpuset_mems_cookie))
		return true;

	return false;
}

static inline struct page *
__alloc_pages_slowpath(gfp_t gfp_mask, unsigned int order,
						struct alloc_context *ac)
{
	bool can_direct_reclaim = gfp_mask & __GFP_DIRECT_RECLAIM;
	const bool costly_order = order > PAGE_ALLOC_COSTLY_ORDER;
	struct page *page = NULL;
	unsigned int alloc_flags;
	unsigned long did_some_progress;
	enum compact_priority compact_priority;
	enum compact_result compact_result;
	int compaction_retries;
	int no_progress_loops;
	unsigned int cpuset_mems_cookie;
	unsigned int zonelist_iter_cookie;
	int reserve_flags;

restart:
	compaction_retries = 0;
	no_progress_loops = 0;
	compact_priority = DEF_COMPACT_PRIORITY;
	cpuset_mems_cookie = read_mems_allowed_begin();
	zonelist_iter_cookie = zonelist_iter_begin();

	 
	alloc_flags = gfp_to_alloc_flags(gfp_mask, order);

	 
	ac->preferred_zoneref = first_zones_zonelist(ac->zonelist,
					ac->highest_zoneidx, ac->nodemask);
	if (!ac->preferred_zoneref->zone)
		goto nopage;

	 
	if (cpusets_insane_config() && (gfp_mask & __GFP_HARDWALL)) {
		struct zoneref *z = first_zones_zonelist(ac->zonelist,
					ac->highest_zoneidx,
					&cpuset_current_mems_allowed);
		if (!z->zone)
			goto nopage;
	}

	if (alloc_flags & ALLOC_KSWAPD)
		wake_all_kswapds(order, gfp_mask, ac);

	 
	page = get_page_from_freelist(gfp_mask, order, alloc_flags, ac);
	if (page)
		goto got_pg;

	 
	if (can_direct_reclaim &&
			(costly_order ||
			   (order > 0 && ac->migratetype != MIGRATE_MOVABLE))
			&& !gfp_pfmemalloc_allowed(gfp_mask)) {
		page = __alloc_pages_direct_compact(gfp_mask, order,
						alloc_flags, ac,
						INIT_COMPACT_PRIORITY,
						&compact_result);
		if (page)
			goto got_pg;

		 
		if (costly_order && (gfp_mask & __GFP_NORETRY)) {
			 
			if (compact_result == COMPACT_SKIPPED ||
			    compact_result == COMPACT_DEFERRED)
				goto nopage;

			 
			compact_priority = INIT_COMPACT_PRIORITY;
		}
	}

retry:
	 
	if (alloc_flags & ALLOC_KSWAPD)
		wake_all_kswapds(order, gfp_mask, ac);

	reserve_flags = __gfp_pfmemalloc_flags(gfp_mask);
	if (reserve_flags)
		alloc_flags = gfp_to_alloc_flags_cma(gfp_mask, reserve_flags) |
					  (alloc_flags & ALLOC_KSWAPD);

	 
	if (!(alloc_flags & ALLOC_CPUSET) || reserve_flags) {
		ac->nodemask = NULL;
		ac->preferred_zoneref = first_zones_zonelist(ac->zonelist,
					ac->highest_zoneidx, ac->nodemask);
	}

	 
	page = get_page_from_freelist(gfp_mask, order, alloc_flags, ac);
	if (page)
		goto got_pg;

	 
	if (!can_direct_reclaim)
		goto nopage;

	 
	if (current->flags & PF_MEMALLOC)
		goto nopage;

	 
	page = __alloc_pages_direct_reclaim(gfp_mask, order, alloc_flags, ac,
							&did_some_progress);
	if (page)
		goto got_pg;

	 
	page = __alloc_pages_direct_compact(gfp_mask, order, alloc_flags, ac,
					compact_priority, &compact_result);
	if (page)
		goto got_pg;

	 
	if (gfp_mask & __GFP_NORETRY)
		goto nopage;

	 
	if (costly_order && !(gfp_mask & __GFP_RETRY_MAYFAIL))
		goto nopage;

	if (should_reclaim_retry(gfp_mask, order, ac, alloc_flags,
				 did_some_progress > 0, &no_progress_loops))
		goto retry;

	 
	if (did_some_progress > 0 &&
			should_compact_retry(ac, order, alloc_flags,
				compact_result, &compact_priority,
				&compaction_retries))
		goto retry;


	 
	if (check_retry_cpuset(cpuset_mems_cookie, ac) ||
	    check_retry_zonelist(zonelist_iter_cookie))
		goto restart;

	 
	page = __alloc_pages_may_oom(gfp_mask, order, ac, &did_some_progress);
	if (page)
		goto got_pg;

	 
	if (tsk_is_oom_victim(current) &&
	    (alloc_flags & ALLOC_OOM ||
	     (gfp_mask & __GFP_NOMEMALLOC)))
		goto nopage;

	 
	if (did_some_progress) {
		no_progress_loops = 0;
		goto retry;
	}

nopage:
	 
	if (check_retry_cpuset(cpuset_mems_cookie, ac) ||
	    check_retry_zonelist(zonelist_iter_cookie))
		goto restart;

	 
	if (gfp_mask & __GFP_NOFAIL) {
		 
		if (WARN_ON_ONCE_GFP(!can_direct_reclaim, gfp_mask))
			goto fail;

		 
		WARN_ON_ONCE_GFP(current->flags & PF_MEMALLOC, gfp_mask);

		 
		WARN_ON_ONCE_GFP(costly_order, gfp_mask);

		 
		page = __alloc_pages_cpuset_fallback(gfp_mask, order, ALLOC_MIN_RESERVE, ac);
		if (page)
			goto got_pg;

		cond_resched();
		goto retry;
	}
fail:
	warn_alloc(gfp_mask, ac->nodemask,
			"page allocation failure: order:%u", order);
got_pg:
	return page;
}

static inline bool prepare_alloc_pages(gfp_t gfp_mask, unsigned int order,
		int preferred_nid, nodemask_t *nodemask,
		struct alloc_context *ac, gfp_t *alloc_gfp,
		unsigned int *alloc_flags)
{
	ac->highest_zoneidx = gfp_zone(gfp_mask);
	ac->zonelist = node_zonelist(preferred_nid, gfp_mask);
	ac->nodemask = nodemask;
	ac->migratetype = gfp_migratetype(gfp_mask);

	if (cpusets_enabled()) {
		*alloc_gfp |= __GFP_HARDWALL;
		 
		if (in_task() && !ac->nodemask)
			ac->nodemask = &cpuset_current_mems_allowed;
		else
			*alloc_flags |= ALLOC_CPUSET;
	}

	might_alloc(gfp_mask);

	if (should_fail_alloc_page(gfp_mask, order))
		return false;

	*alloc_flags = gfp_to_alloc_flags_cma(gfp_mask, *alloc_flags);

	 
	ac->spread_dirty_pages = (gfp_mask & __GFP_WRITE);

	 
	ac->preferred_zoneref = first_zones_zonelist(ac->zonelist,
					ac->highest_zoneidx, ac->nodemask);

	return true;
}

 
unsigned long __alloc_pages_bulk(gfp_t gfp, int preferred_nid,
			nodemask_t *nodemask, int nr_pages,
			struct list_head *page_list,
			struct page **page_array)
{
	struct page *page;
	unsigned long __maybe_unused UP_flags;
	struct zone *zone;
	struct zoneref *z;
	struct per_cpu_pages *pcp;
	struct list_head *pcp_list;
	struct alloc_context ac;
	gfp_t alloc_gfp;
	unsigned int alloc_flags = ALLOC_WMARK_LOW;
	int nr_populated = 0, nr_account = 0;

	 
	while (page_array && nr_populated < nr_pages && page_array[nr_populated])
		nr_populated++;

	 
	if (unlikely(nr_pages <= 0))
		goto out;

	 
	if (unlikely(page_array && nr_pages - nr_populated == 0))
		goto out;

	 
	if (memcg_kmem_online() && (gfp & __GFP_ACCOUNT))
		goto failed;

	 
	if (nr_pages - nr_populated == 1)
		goto failed;

#ifdef CONFIG_PAGE_OWNER
	 
	if (static_branch_unlikely(&page_owner_inited))
		goto failed;
#endif

	 
	gfp &= gfp_allowed_mask;
	alloc_gfp = gfp;
	if (!prepare_alloc_pages(gfp, 0, preferred_nid, nodemask, &ac, &alloc_gfp, &alloc_flags))
		goto out;
	gfp = alloc_gfp;

	 
	for_each_zone_zonelist_nodemask(zone, z, ac.zonelist, ac.highest_zoneidx, ac.nodemask) {
		unsigned long mark;

		if (cpusets_enabled() && (alloc_flags & ALLOC_CPUSET) &&
		    !__cpuset_zone_allowed(zone, gfp)) {
			continue;
		}

		if (nr_online_nodes > 1 && zone != ac.preferred_zoneref->zone &&
		    zone_to_nid(zone) != zone_to_nid(ac.preferred_zoneref->zone)) {
			goto failed;
		}

		mark = wmark_pages(zone, alloc_flags & ALLOC_WMARK_MASK) + nr_pages;
		if (zone_watermark_fast(zone, 0,  mark,
				zonelist_zone_idx(ac.preferred_zoneref),
				alloc_flags, gfp)) {
			break;
		}
	}

	 
	if (unlikely(!zone))
		goto failed;

	 
	pcp_trylock_prepare(UP_flags);
	pcp = pcp_spin_trylock(zone->per_cpu_pageset);
	if (!pcp)
		goto failed_irq;

	 
	pcp_list = &pcp->lists[order_to_pindex(ac.migratetype, 0)];
	while (nr_populated < nr_pages) {

		 
		if (page_array && page_array[nr_populated]) {
			nr_populated++;
			continue;
		}

		page = __rmqueue_pcplist(zone, 0, ac.migratetype, alloc_flags,
								pcp, pcp_list);
		if (unlikely(!page)) {
			 
			if (!nr_account) {
				pcp_spin_unlock(pcp);
				goto failed_irq;
			}
			break;
		}
		nr_account++;

		prep_new_page(page, 0, gfp, 0);
		if (page_list)
			list_add(&page->lru, page_list);
		else
			page_array[nr_populated] = page;
		nr_populated++;
	}

	pcp_spin_unlock(pcp);
	pcp_trylock_finish(UP_flags);

	__count_zid_vm_events(PGALLOC, zone_idx(zone), nr_account);
	zone_statistics(ac.preferred_zoneref->zone, zone, nr_account);

out:
	return nr_populated;

failed_irq:
	pcp_trylock_finish(UP_flags);

failed:
	page = __alloc_pages(gfp, 0, preferred_nid, nodemask);
	if (page) {
		if (page_list)
			list_add(&page->lru, page_list);
		else
			page_array[nr_populated] = page;
		nr_populated++;
	}

	goto out;
}
EXPORT_SYMBOL_GPL(__alloc_pages_bulk);

 
struct page *__alloc_pages(gfp_t gfp, unsigned int order, int preferred_nid,
							nodemask_t *nodemask)
{
	struct page *page;
	unsigned int alloc_flags = ALLOC_WMARK_LOW;
	gfp_t alloc_gfp;  
	struct alloc_context ac = { };

	 
	if (WARN_ON_ONCE_GFP(order > MAX_ORDER, gfp))
		return NULL;

	gfp &= gfp_allowed_mask;
	 
	gfp = current_gfp_context(gfp);
	alloc_gfp = gfp;
	if (!prepare_alloc_pages(gfp, order, preferred_nid, nodemask, &ac,
			&alloc_gfp, &alloc_flags))
		return NULL;

	 
	alloc_flags |= alloc_flags_nofragment(ac.preferred_zoneref->zone, gfp);

	 
	page = get_page_from_freelist(alloc_gfp, order, alloc_flags, &ac);
	if (likely(page))
		goto out;

	alloc_gfp = gfp;
	ac.spread_dirty_pages = false;

	 
	ac.nodemask = nodemask;

	page = __alloc_pages_slowpath(alloc_gfp, order, &ac);

out:
	if (memcg_kmem_online() && (gfp & __GFP_ACCOUNT) && page &&
	    unlikely(__memcg_kmem_charge_page(page, gfp, order) != 0)) {
		__free_pages(page, order);
		page = NULL;
	}

	trace_mm_page_alloc(page, order, alloc_gfp, ac.migratetype);
	kmsan_alloc_page(page, order, alloc_gfp);

	return page;
}
EXPORT_SYMBOL(__alloc_pages);

struct folio *__folio_alloc(gfp_t gfp, unsigned int order, int preferred_nid,
		nodemask_t *nodemask)
{
	struct page *page = __alloc_pages(gfp | __GFP_COMP, order,
			preferred_nid, nodemask);
	struct folio *folio = (struct folio *)page;

	if (folio && order > 1)
		folio_prep_large_rmappable(folio);
	return folio;
}
EXPORT_SYMBOL(__folio_alloc);

 
unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order)
{
	struct page *page;

	page = alloc_pages(gfp_mask & ~__GFP_HIGHMEM, order);
	if (!page)
		return 0;
	return (unsigned long) page_address(page);
}
EXPORT_SYMBOL(__get_free_pages);

unsigned long get_zeroed_page(gfp_t gfp_mask)
{
	return __get_free_page(gfp_mask | __GFP_ZERO);
}
EXPORT_SYMBOL(get_zeroed_page);

 
void __free_pages(struct page *page, unsigned int order)
{
	 
	int head = PageHead(page);

	if (put_page_testzero(page))
		free_the_page(page, order);
	else if (!head)
		while (order-- > 0)
			free_the_page(page + (1 << order), order);
}
EXPORT_SYMBOL(__free_pages);

void free_pages(unsigned long addr, unsigned int order)
{
	if (addr != 0) {
		VM_BUG_ON(!virt_addr_valid((void *)addr));
		__free_pages(virt_to_page((void *)addr), order);
	}
}

EXPORT_SYMBOL(free_pages);

 
static struct page *__page_frag_cache_refill(struct page_frag_cache *nc,
					     gfp_t gfp_mask)
{
	struct page *page = NULL;
	gfp_t gfp = gfp_mask;

#if (PAGE_SIZE < PAGE_FRAG_CACHE_MAX_SIZE)
	gfp_mask |= __GFP_COMP | __GFP_NOWARN | __GFP_NORETRY |
		    __GFP_NOMEMALLOC;
	page = alloc_pages_node(NUMA_NO_NODE, gfp_mask,
				PAGE_FRAG_CACHE_MAX_ORDER);
	nc->size = page ? PAGE_FRAG_CACHE_MAX_SIZE : PAGE_SIZE;
#endif
	if (unlikely(!page))
		page = alloc_pages_node(NUMA_NO_NODE, gfp, 0);

	nc->va = page ? page_address(page) : NULL;

	return page;
}

void __page_frag_cache_drain(struct page *page, unsigned int count)
{
	VM_BUG_ON_PAGE(page_ref_count(page) == 0, page);

	if (page_ref_sub_and_test(page, count))
		free_the_page(page, compound_order(page));
}
EXPORT_SYMBOL(__page_frag_cache_drain);

void *page_frag_alloc_align(struct page_frag_cache *nc,
		      unsigned int fragsz, gfp_t gfp_mask,
		      unsigned int align_mask)
{
	unsigned int size = PAGE_SIZE;
	struct page *page;
	int offset;

	if (unlikely(!nc->va)) {
refill:
		page = __page_frag_cache_refill(nc, gfp_mask);
		if (!page)
			return NULL;

#if (PAGE_SIZE < PAGE_FRAG_CACHE_MAX_SIZE)
		 
		size = nc->size;
#endif
		 
		page_ref_add(page, PAGE_FRAG_CACHE_MAX_SIZE);

		 
		nc->pfmemalloc = page_is_pfmemalloc(page);
		nc->pagecnt_bias = PAGE_FRAG_CACHE_MAX_SIZE + 1;
		nc->offset = size;
	}

	offset = nc->offset - fragsz;
	if (unlikely(offset < 0)) {
		page = virt_to_page(nc->va);

		if (!page_ref_sub_and_test(page, nc->pagecnt_bias))
			goto refill;

		if (unlikely(nc->pfmemalloc)) {
			free_the_page(page, compound_order(page));
			goto refill;
		}

#if (PAGE_SIZE < PAGE_FRAG_CACHE_MAX_SIZE)
		 
		size = nc->size;
#endif
		 
		set_page_count(page, PAGE_FRAG_CACHE_MAX_SIZE + 1);

		 
		nc->pagecnt_bias = PAGE_FRAG_CACHE_MAX_SIZE + 1;
		offset = size - fragsz;
		if (unlikely(offset < 0)) {
			 
			return NULL;
		}
	}

	nc->pagecnt_bias--;
	offset &= align_mask;
	nc->offset = offset;

	return nc->va + offset;
}
EXPORT_SYMBOL(page_frag_alloc_align);

 
void page_frag_free(void *addr)
{
	struct page *page = virt_to_head_page(addr);

	if (unlikely(put_page_testzero(page)))
		free_the_page(page, compound_order(page));
}
EXPORT_SYMBOL(page_frag_free);

static void *make_alloc_exact(unsigned long addr, unsigned int order,
		size_t size)
{
	if (addr) {
		unsigned long nr = DIV_ROUND_UP(size, PAGE_SIZE);
		struct page *page = virt_to_page((void *)addr);
		struct page *last = page + nr;

		split_page_owner(page, 1 << order);
		split_page_memcg(page, 1 << order);
		while (page < --last)
			set_page_refcounted(last);

		last = page + (1UL << order);
		for (page += nr; page < last; page++)
			__free_pages_ok(page, 0, FPI_TO_TAIL);
	}
	return (void *)addr;
}

 
void *alloc_pages_exact(size_t size, gfp_t gfp_mask)
{
	unsigned int order = get_order(size);
	unsigned long addr;

	if (WARN_ON_ONCE(gfp_mask & (__GFP_COMP | __GFP_HIGHMEM)))
		gfp_mask &= ~(__GFP_COMP | __GFP_HIGHMEM);

	addr = __get_free_pages(gfp_mask, order);
	return make_alloc_exact(addr, order, size);
}
EXPORT_SYMBOL(alloc_pages_exact);

 
void * __meminit alloc_pages_exact_nid(int nid, size_t size, gfp_t gfp_mask)
{
	unsigned int order = get_order(size);
	struct page *p;

	if (WARN_ON_ONCE(gfp_mask & (__GFP_COMP | __GFP_HIGHMEM)))
		gfp_mask &= ~(__GFP_COMP | __GFP_HIGHMEM);

	p = alloc_pages_node(nid, gfp_mask, order);
	if (!p)
		return NULL;
	return make_alloc_exact((unsigned long)page_address(p), order, size);
}

 
void free_pages_exact(void *virt, size_t size)
{
	unsigned long addr = (unsigned long)virt;
	unsigned long end = addr + PAGE_ALIGN(size);

	while (addr < end) {
		free_page(addr);
		addr += PAGE_SIZE;
	}
}
EXPORT_SYMBOL(free_pages_exact);

 
static unsigned long nr_free_zone_pages(int offset)
{
	struct zoneref *z;
	struct zone *zone;

	 
	unsigned long sum = 0;

	struct zonelist *zonelist = node_zonelist(numa_node_id(), GFP_KERNEL);

	for_each_zone_zonelist(zone, z, zonelist, offset) {
		unsigned long size = zone_managed_pages(zone);
		unsigned long high = high_wmark_pages(zone);
		if (size > high)
			sum += size - high;
	}

	return sum;
}

 
unsigned long nr_free_buffer_pages(void)
{
	return nr_free_zone_pages(gfp_zone(GFP_USER));
}
EXPORT_SYMBOL_GPL(nr_free_buffer_pages);

static void zoneref_set_zone(struct zone *zone, struct zoneref *zoneref)
{
	zoneref->zone = zone;
	zoneref->zone_idx = zone_idx(zone);
}

 
static int build_zonerefs_node(pg_data_t *pgdat, struct zoneref *zonerefs)
{
	struct zone *zone;
	enum zone_type zone_type = MAX_NR_ZONES;
	int nr_zones = 0;

	do {
		zone_type--;
		zone = pgdat->node_zones + zone_type;
		if (populated_zone(zone)) {
			zoneref_set_zone(zone, &zonerefs[nr_zones++]);
			check_highest_zone(zone_type);
		}
	} while (zone_type);

	return nr_zones;
}

#ifdef CONFIG_NUMA

static int __parse_numa_zonelist_order(char *s)
{
	 
	if (!(*s == 'd' || *s == 'D' || *s == 'n' || *s == 'N')) {
		pr_warn("Ignoring unsupported numa_zonelist_order value:  %s\n", s);
		return -EINVAL;
	}
	return 0;
}

static char numa_zonelist_order[] = "Node";
#define NUMA_ZONELIST_ORDER_LEN	16
 
static int numa_zonelist_order_handler(struct ctl_table *table, int write,
		void *buffer, size_t *length, loff_t *ppos)
{
	if (write)
		return __parse_numa_zonelist_order(buffer);
	return proc_dostring(table, write, buffer, length, ppos);
}

static int node_load[MAX_NUMNODES];

 
int find_next_best_node(int node, nodemask_t *used_node_mask)
{
	int n, val;
	int min_val = INT_MAX;
	int best_node = NUMA_NO_NODE;

	 
	if (!node_isset(node, *used_node_mask)) {
		node_set(node, *used_node_mask);
		return node;
	}

	for_each_node_state(n, N_MEMORY) {

		 
		if (node_isset(n, *used_node_mask))
			continue;

		 
		val = node_distance(node, n);

		 
		val += (n < node);

		 
		if (!cpumask_empty(cpumask_of_node(n)))
			val += PENALTY_FOR_NODE_WITH_CPUS;

		 
		val *= MAX_NUMNODES;
		val += node_load[n];

		if (val < min_val) {
			min_val = val;
			best_node = n;
		}
	}

	if (best_node >= 0)
		node_set(best_node, *used_node_mask);

	return best_node;
}


 
static void build_zonelists_in_node_order(pg_data_t *pgdat, int *node_order,
		unsigned nr_nodes)
{
	struct zoneref *zonerefs;
	int i;

	zonerefs = pgdat->node_zonelists[ZONELIST_FALLBACK]._zonerefs;

	for (i = 0; i < nr_nodes; i++) {
		int nr_zones;

		pg_data_t *node = NODE_DATA(node_order[i]);

		nr_zones = build_zonerefs_node(node, zonerefs);
		zonerefs += nr_zones;
	}
	zonerefs->zone = NULL;
	zonerefs->zone_idx = 0;
}

 
static void build_thisnode_zonelists(pg_data_t *pgdat)
{
	struct zoneref *zonerefs;
	int nr_zones;

	zonerefs = pgdat->node_zonelists[ZONELIST_NOFALLBACK]._zonerefs;
	nr_zones = build_zonerefs_node(pgdat, zonerefs);
	zonerefs += nr_zones;
	zonerefs->zone = NULL;
	zonerefs->zone_idx = 0;
}

 

static void build_zonelists(pg_data_t *pgdat)
{
	static int node_order[MAX_NUMNODES];
	int node, nr_nodes = 0;
	nodemask_t used_mask = NODE_MASK_NONE;
	int local_node, prev_node;

	 
	local_node = pgdat->node_id;
	prev_node = local_node;

	memset(node_order, 0, sizeof(node_order));
	while ((node = find_next_best_node(local_node, &used_mask)) >= 0) {
		 
		if (node_distance(local_node, node) !=
		    node_distance(local_node, prev_node))
			node_load[node] += 1;

		node_order[nr_nodes++] = node;
		prev_node = node;
	}

	build_zonelists_in_node_order(pgdat, node_order, nr_nodes);
	build_thisnode_zonelists(pgdat);
	pr_info("Fallback order for Node %d: ", local_node);
	for (node = 0; node < nr_nodes; node++)
		pr_cont("%d ", node_order[node]);
	pr_cont("\n");
}

#ifdef CONFIG_HAVE_MEMORYLESS_NODES
 
int local_memory_node(int node)
{
	struct zoneref *z;

	z = first_zones_zonelist(node_zonelist(node, GFP_KERNEL),
				   gfp_zone(GFP_KERNEL),
				   NULL);
	return zone_to_nid(z->zone);
}
#endif

static void setup_min_unmapped_ratio(void);
static void setup_min_slab_ratio(void);
#else	 

static void build_zonelists(pg_data_t *pgdat)
{
	int node, local_node;
	struct zoneref *zonerefs;
	int nr_zones;

	local_node = pgdat->node_id;

	zonerefs = pgdat->node_zonelists[ZONELIST_FALLBACK]._zonerefs;
	nr_zones = build_zonerefs_node(pgdat, zonerefs);
	zonerefs += nr_zones;

	 
	for (node = local_node + 1; node < MAX_NUMNODES; node++) {
		if (!node_online(node))
			continue;
		nr_zones = build_zonerefs_node(NODE_DATA(node), zonerefs);
		zonerefs += nr_zones;
	}
	for (node = 0; node < local_node; node++) {
		if (!node_online(node))
			continue;
		nr_zones = build_zonerefs_node(NODE_DATA(node), zonerefs);
		zonerefs += nr_zones;
	}

	zonerefs->zone = NULL;
	zonerefs->zone_idx = 0;
}

#endif	 

 
static void per_cpu_pages_init(struct per_cpu_pages *pcp, struct per_cpu_zonestat *pzstats);
 
#define BOOT_PAGESET_HIGH	0
#define BOOT_PAGESET_BATCH	1
static DEFINE_PER_CPU(struct per_cpu_pages, boot_pageset);
static DEFINE_PER_CPU(struct per_cpu_zonestat, boot_zonestats);

static void __build_all_zonelists(void *data)
{
	int nid;
	int __maybe_unused cpu;
	pg_data_t *self = data;
	unsigned long flags;

	 
	write_seqlock_irqsave(&zonelist_update_seq, flags);
	 
	printk_deferred_enter();

#ifdef CONFIG_NUMA
	memset(node_load, 0, sizeof(node_load));
#endif

	 
	if (self && !node_online(self->node_id)) {
		build_zonelists(self);
	} else {
		 
		for_each_node(nid) {
			pg_data_t *pgdat = NODE_DATA(nid);

			build_zonelists(pgdat);
		}

#ifdef CONFIG_HAVE_MEMORYLESS_NODES
		 
		for_each_online_cpu(cpu)
			set_cpu_numa_mem(cpu, local_memory_node(cpu_to_node(cpu)));
#endif
	}

	printk_deferred_exit();
	write_sequnlock_irqrestore(&zonelist_update_seq, flags);
}

static noinline void __init
build_all_zonelists_init(void)
{
	int cpu;

	__build_all_zonelists(NULL);

	 
	for_each_possible_cpu(cpu)
		per_cpu_pages_init(&per_cpu(boot_pageset, cpu), &per_cpu(boot_zonestats, cpu));

	mminit_verify_zonelist();
	cpuset_init_current_mems_allowed();
}

 
void __ref build_all_zonelists(pg_data_t *pgdat)
{
	unsigned long vm_total_pages;

	if (system_state == SYSTEM_BOOTING) {
		build_all_zonelists_init();
	} else {
		__build_all_zonelists(pgdat);
		 
	}
	 
	vm_total_pages = nr_free_zone_pages(gfp_zone(GFP_HIGHUSER_MOVABLE));
	 
	if (vm_total_pages < (pageblock_nr_pages * MIGRATE_TYPES))
		page_group_by_mobility_disabled = 1;
	else
		page_group_by_mobility_disabled = 0;

	pr_info("Built %u zonelists, mobility grouping %s.  Total pages: %ld\n",
		nr_online_nodes,
		page_group_by_mobility_disabled ? "off" : "on",
		vm_total_pages);
#ifdef CONFIG_NUMA
	pr_info("Policy zone: %s\n", zone_names[policy_zone]);
#endif
}

static int zone_batchsize(struct zone *zone)
{
#ifdef CONFIG_MMU
	int batch;

	 
	batch = min(zone_managed_pages(zone) >> 10, SZ_1M / PAGE_SIZE);
	batch /= 4;		 
	if (batch < 1)
		batch = 1;

	 
	batch = rounddown_pow_of_two(batch + batch/2) - 1;

	return batch;

#else
	 
	return 0;
#endif
}

static int percpu_pagelist_high_fraction;
static int zone_highsize(struct zone *zone, int batch, int cpu_online)
{
#ifdef CONFIG_MMU
	int high;
	int nr_split_cpus;
	unsigned long total_pages;

	if (!percpu_pagelist_high_fraction) {
		 
		total_pages = low_wmark_pages(zone);
	} else {
		 
		total_pages = zone_managed_pages(zone) / percpu_pagelist_high_fraction;
	}

	 
	nr_split_cpus = cpumask_weight(cpumask_of_node(zone_to_nid(zone))) + cpu_online;
	if (!nr_split_cpus)
		nr_split_cpus = num_online_cpus();
	high = total_pages / nr_split_cpus;

	 
	high = max(high, batch << 2);

	return high;
#else
	return 0;
#endif
}

 
static void pageset_update(struct per_cpu_pages *pcp, unsigned long high,
		unsigned long batch)
{
	WRITE_ONCE(pcp->batch, batch);
	WRITE_ONCE(pcp->high, high);
}

static void per_cpu_pages_init(struct per_cpu_pages *pcp, struct per_cpu_zonestat *pzstats)
{
	int pindex;

	memset(pcp, 0, sizeof(*pcp));
	memset(pzstats, 0, sizeof(*pzstats));

	spin_lock_init(&pcp->lock);
	for (pindex = 0; pindex < NR_PCP_LISTS; pindex++)
		INIT_LIST_HEAD(&pcp->lists[pindex]);

	 
	pcp->high = BOOT_PAGESET_HIGH;
	pcp->batch = BOOT_PAGESET_BATCH;
	pcp->free_factor = 0;
}

static void __zone_set_pageset_high_and_batch(struct zone *zone, unsigned long high,
		unsigned long batch)
{
	struct per_cpu_pages *pcp;
	int cpu;

	for_each_possible_cpu(cpu) {
		pcp = per_cpu_ptr(zone->per_cpu_pageset, cpu);
		pageset_update(pcp, high, batch);
	}
}

 
static void zone_set_pageset_high_and_batch(struct zone *zone, int cpu_online)
{
	int new_high, new_batch;

	new_batch = max(1, zone_batchsize(zone));
	new_high = zone_highsize(zone, new_batch, cpu_online);

	if (zone->pageset_high == new_high &&
	    zone->pageset_batch == new_batch)
		return;

	zone->pageset_high = new_high;
	zone->pageset_batch = new_batch;

	__zone_set_pageset_high_and_batch(zone, new_high, new_batch);
}

void __meminit setup_zone_pageset(struct zone *zone)
{
	int cpu;

	 
	if (sizeof(struct per_cpu_zonestat) > 0)
		zone->per_cpu_zonestats = alloc_percpu(struct per_cpu_zonestat);

	zone->per_cpu_pageset = alloc_percpu(struct per_cpu_pages);
	for_each_possible_cpu(cpu) {
		struct per_cpu_pages *pcp;
		struct per_cpu_zonestat *pzstats;

		pcp = per_cpu_ptr(zone->per_cpu_pageset, cpu);
		pzstats = per_cpu_ptr(zone->per_cpu_zonestats, cpu);
		per_cpu_pages_init(pcp, pzstats);
	}

	zone_set_pageset_high_and_batch(zone, 0);
}

 
static void zone_pcp_update(struct zone *zone, int cpu_online)
{
	mutex_lock(&pcp_batch_high_lock);
	zone_set_pageset_high_and_batch(zone, cpu_online);
	mutex_unlock(&pcp_batch_high_lock);
}

 
void __init setup_per_cpu_pageset(void)
{
	struct pglist_data *pgdat;
	struct zone *zone;
	int __maybe_unused cpu;

	for_each_populated_zone(zone)
		setup_zone_pageset(zone);

#ifdef CONFIG_NUMA
	 
	for_each_possible_cpu(cpu) {
		struct per_cpu_zonestat *pzstats = &per_cpu(boot_zonestats, cpu);
		memset(pzstats->vm_numa_event, 0,
		       sizeof(pzstats->vm_numa_event));
	}
#endif

	for_each_online_pgdat(pgdat)
		pgdat->per_cpu_nodestats =
			alloc_percpu(struct per_cpu_nodestat);
}

__meminit void zone_pcp_init(struct zone *zone)
{
	 
	zone->per_cpu_pageset = &boot_pageset;
	zone->per_cpu_zonestats = &boot_zonestats;
	zone->pageset_high = BOOT_PAGESET_HIGH;
	zone->pageset_batch = BOOT_PAGESET_BATCH;

	if (populated_zone(zone))
		pr_debug("  %s zone: %lu pages, LIFO batch:%u\n", zone->name,
			 zone->present_pages, zone_batchsize(zone));
}

void adjust_managed_page_count(struct page *page, long count)
{
	atomic_long_add(count, &page_zone(page)->managed_pages);
	totalram_pages_add(count);
#ifdef CONFIG_HIGHMEM
	if (PageHighMem(page))
		totalhigh_pages_add(count);
#endif
}
EXPORT_SYMBOL(adjust_managed_page_count);

unsigned long free_reserved_area(void *start, void *end, int poison, const char *s)
{
	void *pos;
	unsigned long pages = 0;

	start = (void *)PAGE_ALIGN((unsigned long)start);
	end = (void *)((unsigned long)end & PAGE_MASK);
	for (pos = start; pos < end; pos += PAGE_SIZE, pages++) {
		struct page *page = virt_to_page(pos);
		void *direct_map_addr;

		 
		direct_map_addr = page_address(page);
		 
		direct_map_addr = kasan_reset_tag(direct_map_addr);
		if ((unsigned int)poison <= 0xFF)
			memset(direct_map_addr, poison, PAGE_SIZE);

		free_reserved_page(page);
	}

	if (pages && s)
		pr_info("Freeing %s memory: %ldK\n", s, K(pages));

	return pages;
}

static int page_alloc_cpu_dead(unsigned int cpu)
{
	struct zone *zone;

	lru_add_drain_cpu(cpu);
	mlock_drain_remote(cpu);
	drain_pages(cpu);

	 
	vm_events_fold_cpu(cpu);

	 
	cpu_vm_stats_fold(cpu);

	for_each_populated_zone(zone)
		zone_pcp_update(zone, 0);

	return 0;
}

static int page_alloc_cpu_online(unsigned int cpu)
{
	struct zone *zone;

	for_each_populated_zone(zone)
		zone_pcp_update(zone, 1);
	return 0;
}

void __init page_alloc_init_cpuhp(void)
{
	int ret;

	ret = cpuhp_setup_state_nocalls(CPUHP_PAGE_ALLOC,
					"mm/page_alloc:pcp",
					page_alloc_cpu_online,
					page_alloc_cpu_dead);
	WARN_ON(ret < 0);
}

 
static void calculate_totalreserve_pages(void)
{
	struct pglist_data *pgdat;
	unsigned long reserve_pages = 0;
	enum zone_type i, j;

	for_each_online_pgdat(pgdat) {

		pgdat->totalreserve_pages = 0;

		for (i = 0; i < MAX_NR_ZONES; i++) {
			struct zone *zone = pgdat->node_zones + i;
			long max = 0;
			unsigned long managed_pages = zone_managed_pages(zone);

			 
			for (j = i; j < MAX_NR_ZONES; j++) {
				if (zone->lowmem_reserve[j] > max)
					max = zone->lowmem_reserve[j];
			}

			 
			max += high_wmark_pages(zone);

			if (max > managed_pages)
				max = managed_pages;

			pgdat->totalreserve_pages += max;

			reserve_pages += max;
		}
	}
	totalreserve_pages = reserve_pages;
}

 
static void setup_per_zone_lowmem_reserve(void)
{
	struct pglist_data *pgdat;
	enum zone_type i, j;

	for_each_online_pgdat(pgdat) {
		for (i = 0; i < MAX_NR_ZONES - 1; i++) {
			struct zone *zone = &pgdat->node_zones[i];
			int ratio = sysctl_lowmem_reserve_ratio[i];
			bool clear = !ratio || !zone_managed_pages(zone);
			unsigned long managed_pages = 0;

			for (j = i + 1; j < MAX_NR_ZONES; j++) {
				struct zone *upper_zone = &pgdat->node_zones[j];

				managed_pages += zone_managed_pages(upper_zone);

				if (clear)
					zone->lowmem_reserve[j] = 0;
				else
					zone->lowmem_reserve[j] = managed_pages / ratio;
			}
		}
	}

	 
	calculate_totalreserve_pages();
}

static void __setup_per_zone_wmarks(void)
{
	unsigned long pages_min = min_free_kbytes >> (PAGE_SHIFT - 10);
	unsigned long lowmem_pages = 0;
	struct zone *zone;
	unsigned long flags;

	 
	for_each_zone(zone) {
		if (!is_highmem(zone) && zone_idx(zone) != ZONE_MOVABLE)
			lowmem_pages += zone_managed_pages(zone);
	}

	for_each_zone(zone) {
		u64 tmp;

		spin_lock_irqsave(&zone->lock, flags);
		tmp = (u64)pages_min * zone_managed_pages(zone);
		do_div(tmp, lowmem_pages);
		if (is_highmem(zone) || zone_idx(zone) == ZONE_MOVABLE) {
			 
			unsigned long min_pages;

			min_pages = zone_managed_pages(zone) / 1024;
			min_pages = clamp(min_pages, SWAP_CLUSTER_MAX, 128UL);
			zone->_watermark[WMARK_MIN] = min_pages;
		} else {
			 
			zone->_watermark[WMARK_MIN] = tmp;
		}

		 
		tmp = max_t(u64, tmp >> 2,
			    mult_frac(zone_managed_pages(zone),
				      watermark_scale_factor, 10000));

		zone->watermark_boost = 0;
		zone->_watermark[WMARK_LOW]  = min_wmark_pages(zone) + tmp;
		zone->_watermark[WMARK_HIGH] = low_wmark_pages(zone) + tmp;
		zone->_watermark[WMARK_PROMO] = high_wmark_pages(zone) + tmp;

		spin_unlock_irqrestore(&zone->lock, flags);
	}

	 
	calculate_totalreserve_pages();
}

 
void setup_per_zone_wmarks(void)
{
	struct zone *zone;
	static DEFINE_SPINLOCK(lock);

	spin_lock(&lock);
	__setup_per_zone_wmarks();
	spin_unlock(&lock);

	 
	for_each_zone(zone)
		zone_pcp_update(zone, 0);
}

 
void calculate_min_free_kbytes(void)
{
	unsigned long lowmem_kbytes;
	int new_min_free_kbytes;

	lowmem_kbytes = nr_free_buffer_pages() * (PAGE_SIZE >> 10);
	new_min_free_kbytes = int_sqrt(lowmem_kbytes * 16);

	if (new_min_free_kbytes > user_min_free_kbytes)
		min_free_kbytes = clamp(new_min_free_kbytes, 128, 262144);
	else
		pr_warn("min_free_kbytes is not updated to %d because user defined value %d is preferred\n",
				new_min_free_kbytes, user_min_free_kbytes);

}

int __meminit init_per_zone_wmark_min(void)
{
	calculate_min_free_kbytes();
	setup_per_zone_wmarks();
	refresh_zone_stat_thresholds();
	setup_per_zone_lowmem_reserve();

#ifdef CONFIG_NUMA
	setup_min_unmapped_ratio();
	setup_min_slab_ratio();
#endif

	khugepaged_min_free_kbytes_update();

	return 0;
}
postcore_initcall(init_per_zone_wmark_min)

 
static int min_free_kbytes_sysctl_handler(struct ctl_table *table, int write,
		void *buffer, size_t *length, loff_t *ppos)
{
	int rc;

	rc = proc_dointvec_minmax(table, write, buffer, length, ppos);
	if (rc)
		return rc;

	if (write) {
		user_min_free_kbytes = min_free_kbytes;
		setup_per_zone_wmarks();
	}
	return 0;
}

static int watermark_scale_factor_sysctl_handler(struct ctl_table *table, int write,
		void *buffer, size_t *length, loff_t *ppos)
{
	int rc;

	rc = proc_dointvec_minmax(table, write, buffer, length, ppos);
	if (rc)
		return rc;

	if (write)
		setup_per_zone_wmarks();

	return 0;
}

#ifdef CONFIG_NUMA
static void setup_min_unmapped_ratio(void)
{
	pg_data_t *pgdat;
	struct zone *zone;

	for_each_online_pgdat(pgdat)
		pgdat->min_unmapped_pages = 0;

	for_each_zone(zone)
		zone->zone_pgdat->min_unmapped_pages += (zone_managed_pages(zone) *
						         sysctl_min_unmapped_ratio) / 100;
}


static int sysctl_min_unmapped_ratio_sysctl_handler(struct ctl_table *table, int write,
		void *buffer, size_t *length, loff_t *ppos)
{
	int rc;

	rc = proc_dointvec_minmax(table, write, buffer, length, ppos);
	if (rc)
		return rc;

	setup_min_unmapped_ratio();

	return 0;
}

static void setup_min_slab_ratio(void)
{
	pg_data_t *pgdat;
	struct zone *zone;

	for_each_online_pgdat(pgdat)
		pgdat->min_slab_pages = 0;

	for_each_zone(zone)
		zone->zone_pgdat->min_slab_pages += (zone_managed_pages(zone) *
						     sysctl_min_slab_ratio) / 100;
}

static int sysctl_min_slab_ratio_sysctl_handler(struct ctl_table *table, int write,
		void *buffer, size_t *length, loff_t *ppos)
{
	int rc;

	rc = proc_dointvec_minmax(table, write, buffer, length, ppos);
	if (rc)
		return rc;

	setup_min_slab_ratio();

	return 0;
}
#endif

 
static int lowmem_reserve_ratio_sysctl_handler(struct ctl_table *table,
		int write, void *buffer, size_t *length, loff_t *ppos)
{
	int i;

	proc_dointvec_minmax(table, write, buffer, length, ppos);

	for (i = 0; i < MAX_NR_ZONES; i++) {
		if (sysctl_lowmem_reserve_ratio[i] < 1)
			sysctl_lowmem_reserve_ratio[i] = 0;
	}

	setup_per_zone_lowmem_reserve();
	return 0;
}

 
static int percpu_pagelist_high_fraction_sysctl_handler(struct ctl_table *table,
		int write, void *buffer, size_t *length, loff_t *ppos)
{
	struct zone *zone;
	int old_percpu_pagelist_high_fraction;
	int ret;

	mutex_lock(&pcp_batch_high_lock);
	old_percpu_pagelist_high_fraction = percpu_pagelist_high_fraction;

	ret = proc_dointvec_minmax(table, write, buffer, length, ppos);
	if (!write || ret < 0)
		goto out;

	 
	if (percpu_pagelist_high_fraction &&
	    percpu_pagelist_high_fraction < MIN_PERCPU_PAGELIST_HIGH_FRACTION) {
		percpu_pagelist_high_fraction = old_percpu_pagelist_high_fraction;
		ret = -EINVAL;
		goto out;
	}

	 
	if (percpu_pagelist_high_fraction == old_percpu_pagelist_high_fraction)
		goto out;

	for_each_populated_zone(zone)
		zone_set_pageset_high_and_batch(zone, 0);
out:
	mutex_unlock(&pcp_batch_high_lock);
	return ret;
}

static struct ctl_table page_alloc_sysctl_table[] = {
	{
		.procname	= "min_free_kbytes",
		.data		= &min_free_kbytes,
		.maxlen		= sizeof(min_free_kbytes),
		.mode		= 0644,
		.proc_handler	= min_free_kbytes_sysctl_handler,
		.extra1		= SYSCTL_ZERO,
	},
	{
		.procname	= "watermark_boost_factor",
		.data		= &watermark_boost_factor,
		.maxlen		= sizeof(watermark_boost_factor),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
	},
	{
		.procname	= "watermark_scale_factor",
		.data		= &watermark_scale_factor,
		.maxlen		= sizeof(watermark_scale_factor),
		.mode		= 0644,
		.proc_handler	= watermark_scale_factor_sysctl_handler,
		.extra1		= SYSCTL_ONE,
		.extra2		= SYSCTL_THREE_THOUSAND,
	},
	{
		.procname	= "percpu_pagelist_high_fraction",
		.data		= &percpu_pagelist_high_fraction,
		.maxlen		= sizeof(percpu_pagelist_high_fraction),
		.mode		= 0644,
		.proc_handler	= percpu_pagelist_high_fraction_sysctl_handler,
		.extra1		= SYSCTL_ZERO,
	},
	{
		.procname	= "lowmem_reserve_ratio",
		.data		= &sysctl_lowmem_reserve_ratio,
		.maxlen		= sizeof(sysctl_lowmem_reserve_ratio),
		.mode		= 0644,
		.proc_handler	= lowmem_reserve_ratio_sysctl_handler,
	},
#ifdef CONFIG_NUMA
	{
		.procname	= "numa_zonelist_order",
		.data		= &numa_zonelist_order,
		.maxlen		= NUMA_ZONELIST_ORDER_LEN,
		.mode		= 0644,
		.proc_handler	= numa_zonelist_order_handler,
	},
	{
		.procname	= "min_unmapped_ratio",
		.data		= &sysctl_min_unmapped_ratio,
		.maxlen		= sizeof(sysctl_min_unmapped_ratio),
		.mode		= 0644,
		.proc_handler	= sysctl_min_unmapped_ratio_sysctl_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE_HUNDRED,
	},
	{
		.procname	= "min_slab_ratio",
		.data		= &sysctl_min_slab_ratio,
		.maxlen		= sizeof(sysctl_min_slab_ratio),
		.mode		= 0644,
		.proc_handler	= sysctl_min_slab_ratio_sysctl_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE_HUNDRED,
	},
#endif
	{}
};

void __init page_alloc_sysctl_init(void)
{
	register_sysctl_init("vm", page_alloc_sysctl_table);
}

#ifdef CONFIG_CONTIG_ALLOC
 
static void alloc_contig_dump_pages(struct list_head *page_list)
{
	DEFINE_DYNAMIC_DEBUG_METADATA(descriptor, "migrate failure");

	if (DYNAMIC_DEBUG_BRANCH(descriptor)) {
		struct page *page;

		dump_stack();
		list_for_each_entry(page, page_list, lru)
			dump_page(page, "migration failure");
	}
}

 
int __alloc_contig_migrate_range(struct compact_control *cc,
					unsigned long start, unsigned long end)
{
	 
	unsigned int nr_reclaimed;
	unsigned long pfn = start;
	unsigned int tries = 0;
	int ret = 0;
	struct migration_target_control mtc = {
		.nid = zone_to_nid(cc->zone),
		.gfp_mask = GFP_USER | __GFP_MOVABLE | __GFP_RETRY_MAYFAIL,
	};

	lru_cache_disable();

	while (pfn < end || !list_empty(&cc->migratepages)) {
		if (fatal_signal_pending(current)) {
			ret = -EINTR;
			break;
		}

		if (list_empty(&cc->migratepages)) {
			cc->nr_migratepages = 0;
			ret = isolate_migratepages_range(cc, pfn, end);
			if (ret && ret != -EAGAIN)
				break;
			pfn = cc->migrate_pfn;
			tries = 0;
		} else if (++tries == 5) {
			ret = -EBUSY;
			break;
		}

		nr_reclaimed = reclaim_clean_pages_from_list(cc->zone,
							&cc->migratepages);
		cc->nr_migratepages -= nr_reclaimed;

		ret = migrate_pages(&cc->migratepages, alloc_migration_target,
			NULL, (unsigned long)&mtc, cc->mode, MR_CONTIG_RANGE, NULL);

		 
		if (ret == -ENOMEM)
			break;
	}

	lru_cache_enable();
	if (ret < 0) {
		if (!(cc->gfp_mask & __GFP_NOWARN) && ret == -EBUSY)
			alloc_contig_dump_pages(&cc->migratepages);
		putback_movable_pages(&cc->migratepages);
		return ret;
	}
	return 0;
}

 
int alloc_contig_range(unsigned long start, unsigned long end,
		       unsigned migratetype, gfp_t gfp_mask)
{
	unsigned long outer_start, outer_end;
	int order;
	int ret = 0;

	struct compact_control cc = {
		.nr_migratepages = 0,
		.order = -1,
		.zone = page_zone(pfn_to_page(start)),
		.mode = MIGRATE_SYNC,
		.ignore_skip_hint = true,
		.no_set_skip_hint = true,
		.gfp_mask = current_gfp_context(gfp_mask),
		.alloc_contig = true,
	};
	INIT_LIST_HEAD(&cc.migratepages);

	 

	ret = start_isolate_page_range(start, end, migratetype, 0, gfp_mask);
	if (ret)
		goto done;

	drain_all_pages(cc.zone);

	 
	ret = __alloc_contig_migrate_range(&cc, start, end);
	if (ret && ret != -EBUSY)
		goto done;
	ret = 0;

	 

	order = 0;
	outer_start = start;
	while (!PageBuddy(pfn_to_page(outer_start))) {
		if (++order > MAX_ORDER) {
			outer_start = start;
			break;
		}
		outer_start &= ~0UL << order;
	}

	if (outer_start != start) {
		order = buddy_order(pfn_to_page(outer_start));

		 
		if (outer_start + (1UL << order) <= start)
			outer_start = start;
	}

	 
	if (test_pages_isolated(outer_start, end, 0)) {
		ret = -EBUSY;
		goto done;
	}

	 
	outer_end = isolate_freepages_range(&cc, outer_start, end);
	if (!outer_end) {
		ret = -EBUSY;
		goto done;
	}

	 
	if (start != outer_start)
		free_contig_range(outer_start, start - outer_start);
	if (end != outer_end)
		free_contig_range(end, outer_end - end);

done:
	undo_isolate_page_range(start, end, migratetype);
	return ret;
}
EXPORT_SYMBOL(alloc_contig_range);

static int __alloc_contig_pages(unsigned long start_pfn,
				unsigned long nr_pages, gfp_t gfp_mask)
{
	unsigned long end_pfn = start_pfn + nr_pages;

	return alloc_contig_range(start_pfn, end_pfn, MIGRATE_MOVABLE,
				  gfp_mask);
}

static bool pfn_range_valid_contig(struct zone *z, unsigned long start_pfn,
				   unsigned long nr_pages)
{
	unsigned long i, end_pfn = start_pfn + nr_pages;
	struct page *page;

	for (i = start_pfn; i < end_pfn; i++) {
		page = pfn_to_online_page(i);
		if (!page)
			return false;

		if (page_zone(page) != z)
			return false;

		if (PageReserved(page))
			return false;

		if (PageHuge(page))
			return false;
	}
	return true;
}

static bool zone_spans_last_pfn(const struct zone *zone,
				unsigned long start_pfn, unsigned long nr_pages)
{
	unsigned long last_pfn = start_pfn + nr_pages - 1;

	return zone_spans_pfn(zone, last_pfn);
}

 
struct page *alloc_contig_pages(unsigned long nr_pages, gfp_t gfp_mask,
				int nid, nodemask_t *nodemask)
{
	unsigned long ret, pfn, flags;
	struct zonelist *zonelist;
	struct zone *zone;
	struct zoneref *z;

	zonelist = node_zonelist(nid, gfp_mask);
	for_each_zone_zonelist_nodemask(zone, z, zonelist,
					gfp_zone(gfp_mask), nodemask) {
		spin_lock_irqsave(&zone->lock, flags);

		pfn = ALIGN(zone->zone_start_pfn, nr_pages);
		while (zone_spans_last_pfn(zone, pfn, nr_pages)) {
			if (pfn_range_valid_contig(zone, pfn, nr_pages)) {
				 
				spin_unlock_irqrestore(&zone->lock, flags);
				ret = __alloc_contig_pages(pfn, nr_pages,
							gfp_mask);
				if (!ret)
					return pfn_to_page(pfn);
				spin_lock_irqsave(&zone->lock, flags);
			}
			pfn += nr_pages;
		}
		spin_unlock_irqrestore(&zone->lock, flags);
	}
	return NULL;
}
#endif  

void free_contig_range(unsigned long pfn, unsigned long nr_pages)
{
	unsigned long count = 0;

	for (; nr_pages--; pfn++) {
		struct page *page = pfn_to_page(pfn);

		count += page_count(page) != 1;
		__free_page(page);
	}
	WARN(count != 0, "%lu pages are still in use!\n", count);
}
EXPORT_SYMBOL(free_contig_range);

 
void zone_pcp_disable(struct zone *zone)
{
	mutex_lock(&pcp_batch_high_lock);
	__zone_set_pageset_high_and_batch(zone, 0, 1);
	__drain_all_pages(zone, true);
}

void zone_pcp_enable(struct zone *zone)
{
	__zone_set_pageset_high_and_batch(zone, zone->pageset_high, zone->pageset_batch);
	mutex_unlock(&pcp_batch_high_lock);
}

void zone_pcp_reset(struct zone *zone)
{
	int cpu;
	struct per_cpu_zonestat *pzstats;

	if (zone->per_cpu_pageset != &boot_pageset) {
		for_each_online_cpu(cpu) {
			pzstats = per_cpu_ptr(zone->per_cpu_zonestats, cpu);
			drain_zonestat(zone, pzstats);
		}
		free_percpu(zone->per_cpu_pageset);
		zone->per_cpu_pageset = &boot_pageset;
		if (zone->per_cpu_zonestats != &boot_zonestats) {
			free_percpu(zone->per_cpu_zonestats);
			zone->per_cpu_zonestats = &boot_zonestats;
		}
	}
}

#ifdef CONFIG_MEMORY_HOTREMOVE
 
void __offline_isolated_pages(unsigned long start_pfn, unsigned long end_pfn)
{
	unsigned long pfn = start_pfn;
	struct page *page;
	struct zone *zone;
	unsigned int order;
	unsigned long flags;

	offline_mem_sections(pfn, end_pfn);
	zone = page_zone(pfn_to_page(pfn));
	spin_lock_irqsave(&zone->lock, flags);
	while (pfn < end_pfn) {
		page = pfn_to_page(pfn);
		 
		if (unlikely(!PageBuddy(page) && PageHWPoison(page))) {
			pfn++;
			continue;
		}
		 
		if (PageOffline(page)) {
			BUG_ON(page_count(page));
			BUG_ON(PageBuddy(page));
			pfn++;
			continue;
		}

		BUG_ON(page_count(page));
		BUG_ON(!PageBuddy(page));
		order = buddy_order(page);
		del_page_from_free_list(page, zone, order);
		pfn += (1 << order);
	}
	spin_unlock_irqrestore(&zone->lock, flags);
}
#endif

 
bool is_free_buddy_page(struct page *page)
{
	unsigned long pfn = page_to_pfn(page);
	unsigned int order;

	for (order = 0; order <= MAX_ORDER; order++) {
		struct page *page_head = page - (pfn & ((1 << order) - 1));

		if (PageBuddy(page_head) &&
		    buddy_order_unsafe(page_head) >= order)
			break;
	}

	return order <= MAX_ORDER;
}
EXPORT_SYMBOL(is_free_buddy_page);

#ifdef CONFIG_MEMORY_FAILURE
 
static void break_down_buddy_pages(struct zone *zone, struct page *page,
				   struct page *target, int low, int high,
				   int migratetype)
{
	unsigned long size = 1 << high;
	struct page *current_buddy, *next_page;

	while (high > low) {
		high--;
		size >>= 1;

		if (target >= &page[size]) {
			next_page = page + size;
			current_buddy = page;
		} else {
			next_page = page;
			current_buddy = page + size;
		}
		page = next_page;

		if (set_page_guard(zone, current_buddy, high, migratetype))
			continue;

		if (current_buddy != target) {
			add_to_free_list(current_buddy, zone, high, migratetype);
			set_buddy_order(current_buddy, high);
		}
	}
}

 
bool take_page_off_buddy(struct page *page)
{
	struct zone *zone = page_zone(page);
	unsigned long pfn = page_to_pfn(page);
	unsigned long flags;
	unsigned int order;
	bool ret = false;

	spin_lock_irqsave(&zone->lock, flags);
	for (order = 0; order <= MAX_ORDER; order++) {
		struct page *page_head = page - (pfn & ((1 << order) - 1));
		int page_order = buddy_order(page_head);

		if (PageBuddy(page_head) && page_order >= order) {
			unsigned long pfn_head = page_to_pfn(page_head);
			int migratetype = get_pfnblock_migratetype(page_head,
								   pfn_head);

			del_page_from_free_list(page_head, zone, page_order);
			break_down_buddy_pages(zone, page_head, page, 0,
						page_order, migratetype);
			SetPageHWPoisonTakenOff(page);
			if (!is_migrate_isolate(migratetype))
				__mod_zone_freepage_state(zone, -1, migratetype);
			ret = true;
			break;
		}
		if (page_count(page_head) > 0)
			break;
	}
	spin_unlock_irqrestore(&zone->lock, flags);
	return ret;
}

 
bool put_page_back_buddy(struct page *page)
{
	struct zone *zone = page_zone(page);
	unsigned long pfn = page_to_pfn(page);
	unsigned long flags;
	int migratetype = get_pfnblock_migratetype(page, pfn);
	bool ret = false;

	spin_lock_irqsave(&zone->lock, flags);
	if (put_page_testzero(page)) {
		ClearPageHWPoisonTakenOff(page);
		__free_one_page(page, pfn, zone, 0, migratetype, FPI_NONE);
		if (TestClearPageHWPoison(page)) {
			ret = true;
		}
	}
	spin_unlock_irqrestore(&zone->lock, flags);

	return ret;
}
#endif

#ifdef CONFIG_ZONE_DMA
bool has_managed_dma(void)
{
	struct pglist_data *pgdat;

	for_each_online_pgdat(pgdat) {
		struct zone *zone = &pgdat->node_zones[ZONE_DMA];

		if (managed_zone(zone))
			return true;
	}
	return false;
}
#endif  

#ifdef CONFIG_UNACCEPTED_MEMORY

 
static DEFINE_STATIC_KEY_FALSE(zones_with_unaccepted_pages);

static bool lazy_accept = true;

static int __init accept_memory_parse(char *p)
{
	if (!strcmp(p, "lazy")) {
		lazy_accept = true;
		return 0;
	} else if (!strcmp(p, "eager")) {
		lazy_accept = false;
		return 0;
	} else {
		return -EINVAL;
	}
}
early_param("accept_memory", accept_memory_parse);

static bool page_contains_unaccepted(struct page *page, unsigned int order)
{
	phys_addr_t start = page_to_phys(page);
	phys_addr_t end = start + (PAGE_SIZE << order);

	return range_contains_unaccepted_memory(start, end);
}

static void accept_page(struct page *page, unsigned int order)
{
	phys_addr_t start = page_to_phys(page);

	accept_memory(start, start + (PAGE_SIZE << order));
}

static bool try_to_accept_memory_one(struct zone *zone)
{
	unsigned long flags;
	struct page *page;
	bool last;

	if (list_empty(&zone->unaccepted_pages))
		return false;

	spin_lock_irqsave(&zone->lock, flags);
	page = list_first_entry_or_null(&zone->unaccepted_pages,
					struct page, lru);
	if (!page) {
		spin_unlock_irqrestore(&zone->lock, flags);
		return false;
	}

	list_del(&page->lru);
	last = list_empty(&zone->unaccepted_pages);

	__mod_zone_freepage_state(zone, -MAX_ORDER_NR_PAGES, MIGRATE_MOVABLE);
	__mod_zone_page_state(zone, NR_UNACCEPTED, -MAX_ORDER_NR_PAGES);
	spin_unlock_irqrestore(&zone->lock, flags);

	accept_page(page, MAX_ORDER);

	__free_pages_ok(page, MAX_ORDER, FPI_TO_TAIL);

	if (last)
		static_branch_dec(&zones_with_unaccepted_pages);

	return true;
}

static bool try_to_accept_memory(struct zone *zone, unsigned int order)
{
	long to_accept;
	int ret = false;

	 
	to_accept = high_wmark_pages(zone) -
		    (zone_page_state(zone, NR_FREE_PAGES) -
		    __zone_watermark_unusable_free(zone, order, 0));

	 
	do {
		if (!try_to_accept_memory_one(zone))
			break;
		ret = true;
		to_accept -= MAX_ORDER_NR_PAGES;
	} while (to_accept > 0);

	return ret;
}

static inline bool has_unaccepted_memory(void)
{
	return static_branch_unlikely(&zones_with_unaccepted_pages);
}

static bool __free_unaccepted(struct page *page)
{
	struct zone *zone = page_zone(page);
	unsigned long flags;
	bool first = false;

	if (!lazy_accept)
		return false;

	spin_lock_irqsave(&zone->lock, flags);
	first = list_empty(&zone->unaccepted_pages);
	list_add_tail(&page->lru, &zone->unaccepted_pages);
	__mod_zone_freepage_state(zone, MAX_ORDER_NR_PAGES, MIGRATE_MOVABLE);
	__mod_zone_page_state(zone, NR_UNACCEPTED, MAX_ORDER_NR_PAGES);
	spin_unlock_irqrestore(&zone->lock, flags);

	if (first)
		static_branch_inc(&zones_with_unaccepted_pages);

	return true;
}

#else

static bool page_contains_unaccepted(struct page *page, unsigned int order)
{
	return false;
}

static void accept_page(struct page *page, unsigned int order)
{
}

static bool try_to_accept_memory(struct zone *zone, unsigned int order)
{
	return false;
}

static inline bool has_unaccepted_memory(void)
{
	return false;
}

static bool __free_unaccepted(struct page *page)
{
	BUILD_BUG();
	return false;
}

#endif  
