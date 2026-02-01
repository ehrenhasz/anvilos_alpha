
 
#include <linux/cpu.h>
#include <linux/swap.h>
#include <linux/migrate.h>
#include <linux/compaction.h>
#include <linux/mm_inline.h>
#include <linux/sched/signal.h>
#include <linux/backing-dev.h>
#include <linux/sysctl.h>
#include <linux/sysfs.h>
#include <linux/page-isolation.h>
#include <linux/kasan.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/page_owner.h>
#include <linux/psi.h>
#include "internal.h"

#ifdef CONFIG_COMPACTION
 
#define HPAGE_FRAG_CHECK_INTERVAL_MSEC	(500)

static inline void count_compact_event(enum vm_event_item item)
{
	count_vm_event(item);
}

static inline void count_compact_events(enum vm_event_item item, long delta)
{
	count_vm_events(item, delta);
}
#else
#define count_compact_event(item) do { } while (0)
#define count_compact_events(item, delta) do { } while (0)
#endif

#if defined CONFIG_COMPACTION || defined CONFIG_CMA

#define CREATE_TRACE_POINTS
#include <trace/events/compaction.h>

#define block_start_pfn(pfn, order)	round_down(pfn, 1UL << (order))
#define block_end_pfn(pfn, order)	ALIGN((pfn) + 1, 1UL << (order))

 
#if defined CONFIG_TRANSPARENT_HUGEPAGE
#define COMPACTION_HPAGE_ORDER	HPAGE_PMD_ORDER
#elif defined CONFIG_HUGETLBFS
#define COMPACTION_HPAGE_ORDER	HUGETLB_PAGE_ORDER
#else
#define COMPACTION_HPAGE_ORDER	(PMD_SHIFT - PAGE_SHIFT)
#endif

static unsigned long release_freepages(struct list_head *freelist)
{
	struct page *page, *next;
	unsigned long high_pfn = 0;

	list_for_each_entry_safe(page, next, freelist, lru) {
		unsigned long pfn = page_to_pfn(page);
		list_del(&page->lru);
		__free_page(page);
		if (pfn > high_pfn)
			high_pfn = pfn;
	}

	return high_pfn;
}

static void split_map_pages(struct list_head *list)
{
	unsigned int i, order, nr_pages;
	struct page *page, *next;
	LIST_HEAD(tmp_list);

	list_for_each_entry_safe(page, next, list, lru) {
		list_del(&page->lru);

		order = page_private(page);
		nr_pages = 1 << order;

		post_alloc_hook(page, order, __GFP_MOVABLE);
		if (order)
			split_page(page, order);

		for (i = 0; i < nr_pages; i++) {
			list_add(&page->lru, &tmp_list);
			page++;
		}
	}

	list_splice(&tmp_list, list);
}

#ifdef CONFIG_COMPACTION
bool PageMovable(struct page *page)
{
	const struct movable_operations *mops;

	VM_BUG_ON_PAGE(!PageLocked(page), page);
	if (!__PageMovable(page))
		return false;

	mops = page_movable_ops(page);
	if (mops)
		return true;

	return false;
}

void __SetPageMovable(struct page *page, const struct movable_operations *mops)
{
	VM_BUG_ON_PAGE(!PageLocked(page), page);
	VM_BUG_ON_PAGE((unsigned long)mops & PAGE_MAPPING_MOVABLE, page);
	page->mapping = (void *)((unsigned long)mops | PAGE_MAPPING_MOVABLE);
}
EXPORT_SYMBOL(__SetPageMovable);

void __ClearPageMovable(struct page *page)
{
	VM_BUG_ON_PAGE(!PageMovable(page), page);
	 
	page->mapping = (void *)PAGE_MAPPING_MOVABLE;
}
EXPORT_SYMBOL(__ClearPageMovable);

 
#define COMPACT_MAX_DEFER_SHIFT 6

 
static void defer_compaction(struct zone *zone, int order)
{
	zone->compact_considered = 0;
	zone->compact_defer_shift++;

	if (order < zone->compact_order_failed)
		zone->compact_order_failed = order;

	if (zone->compact_defer_shift > COMPACT_MAX_DEFER_SHIFT)
		zone->compact_defer_shift = COMPACT_MAX_DEFER_SHIFT;

	trace_mm_compaction_defer_compaction(zone, order);
}

 
static bool compaction_deferred(struct zone *zone, int order)
{
	unsigned long defer_limit = 1UL << zone->compact_defer_shift;

	if (order < zone->compact_order_failed)
		return false;

	 
	if (++zone->compact_considered >= defer_limit) {
		zone->compact_considered = defer_limit;
		return false;
	}

	trace_mm_compaction_deferred(zone, order);

	return true;
}

 
void compaction_defer_reset(struct zone *zone, int order,
		bool alloc_success)
{
	if (alloc_success) {
		zone->compact_considered = 0;
		zone->compact_defer_shift = 0;
	}
	if (order >= zone->compact_order_failed)
		zone->compact_order_failed = order + 1;

	trace_mm_compaction_defer_reset(zone, order);
}

 
static bool compaction_restarting(struct zone *zone, int order)
{
	if (order < zone->compact_order_failed)
		return false;

	return zone->compact_defer_shift == COMPACT_MAX_DEFER_SHIFT &&
		zone->compact_considered >= 1UL << zone->compact_defer_shift;
}

 
static inline bool isolation_suitable(struct compact_control *cc,
					struct page *page)
{
	if (cc->ignore_skip_hint)
		return true;

	return !get_pageblock_skip(page);
}

static void reset_cached_positions(struct zone *zone)
{
	zone->compact_cached_migrate_pfn[0] = zone->zone_start_pfn;
	zone->compact_cached_migrate_pfn[1] = zone->zone_start_pfn;
	zone->compact_cached_free_pfn =
				pageblock_start_pfn(zone_end_pfn(zone) - 1);
}

#ifdef CONFIG_SPARSEMEM
 
static unsigned long skip_offline_sections(unsigned long start_pfn)
{
	unsigned long start_nr = pfn_to_section_nr(start_pfn);

	if (online_section_nr(start_nr))
		return 0;

	while (++start_nr <= __highest_present_section_nr) {
		if (online_section_nr(start_nr))
			return section_nr_to_pfn(start_nr);
	}

	return 0;
}

 
static unsigned long skip_offline_sections_reverse(unsigned long start_pfn)
{
	unsigned long start_nr = pfn_to_section_nr(start_pfn);

	if (!start_nr || online_section_nr(start_nr))
		return 0;

	while (start_nr-- > 0) {
		if (online_section_nr(start_nr))
			return section_nr_to_pfn(start_nr) + PAGES_PER_SECTION;
	}

	return 0;
}
#else
static unsigned long skip_offline_sections(unsigned long start_pfn)
{
	return 0;
}

static unsigned long skip_offline_sections_reverse(unsigned long start_pfn)
{
	return 0;
}
#endif

 
static bool pageblock_skip_persistent(struct page *page)
{
	if (!PageCompound(page))
		return false;

	page = compound_head(page);

	if (compound_order(page) >= pageblock_order)
		return true;

	return false;
}

static bool
__reset_isolation_pfn(struct zone *zone, unsigned long pfn, bool check_source,
							bool check_target)
{
	struct page *page = pfn_to_online_page(pfn);
	struct page *block_page;
	struct page *end_page;
	unsigned long block_pfn;

	if (!page)
		return false;
	if (zone != page_zone(page))
		return false;
	if (pageblock_skip_persistent(page))
		return false;

	 
	if (check_source && check_target && !get_pageblock_skip(page))
		return true;

	 
	if (!check_source && check_target &&
	    get_pageblock_migratetype(page) != MIGRATE_MOVABLE)
		return false;

	 
	block_pfn = pageblock_start_pfn(pfn);
	block_pfn = max(block_pfn, zone->zone_start_pfn);
	block_page = pfn_to_online_page(block_pfn);
	if (block_page) {
		page = block_page;
		pfn = block_pfn;
	}

	 
	block_pfn = pageblock_end_pfn(pfn) - 1;
	block_pfn = min(block_pfn, zone_end_pfn(zone) - 1);
	end_page = pfn_to_online_page(block_pfn);
	if (!end_page)
		return false;

	 
	do {
		if (check_source && PageLRU(page)) {
			clear_pageblock_skip(page);
			return true;
		}

		if (check_target && PageBuddy(page)) {
			clear_pageblock_skip(page);
			return true;
		}

		page += (1 << PAGE_ALLOC_COSTLY_ORDER);
	} while (page <= end_page);

	return false;
}

 
static void __reset_isolation_suitable(struct zone *zone)
{
	unsigned long migrate_pfn = zone->zone_start_pfn;
	unsigned long free_pfn = zone_end_pfn(zone) - 1;
	unsigned long reset_migrate = free_pfn;
	unsigned long reset_free = migrate_pfn;
	bool source_set = false;
	bool free_set = false;

	if (!zone->compact_blockskip_flush)
		return;

	zone->compact_blockskip_flush = false;

	 
	for (; migrate_pfn < free_pfn; migrate_pfn += pageblock_nr_pages,
					free_pfn -= pageblock_nr_pages) {
		cond_resched();

		 
		if (__reset_isolation_pfn(zone, migrate_pfn, true, source_set) &&
		    migrate_pfn < reset_migrate) {
			source_set = true;
			reset_migrate = migrate_pfn;
			zone->compact_init_migrate_pfn = reset_migrate;
			zone->compact_cached_migrate_pfn[0] = reset_migrate;
			zone->compact_cached_migrate_pfn[1] = reset_migrate;
		}

		 
		if (__reset_isolation_pfn(zone, free_pfn, free_set, true) &&
		    free_pfn > reset_free) {
			free_set = true;
			reset_free = free_pfn;
			zone->compact_init_free_pfn = reset_free;
			zone->compact_cached_free_pfn = reset_free;
		}
	}

	 
	if (reset_migrate >= reset_free) {
		zone->compact_cached_migrate_pfn[0] = migrate_pfn;
		zone->compact_cached_migrate_pfn[1] = migrate_pfn;
		zone->compact_cached_free_pfn = free_pfn;
	}
}

void reset_isolation_suitable(pg_data_t *pgdat)
{
	int zoneid;

	for (zoneid = 0; zoneid < MAX_NR_ZONES; zoneid++) {
		struct zone *zone = &pgdat->node_zones[zoneid];
		if (!populated_zone(zone))
			continue;

		 
		if (zone->compact_blockskip_flush)
			__reset_isolation_suitable(zone);
	}
}

 
static bool test_and_set_skip(struct compact_control *cc, struct page *page)
{
	bool skip;

	 
	if (cc->ignore_skip_hint)
		return false;

	skip = get_pageblock_skip(page);
	if (!skip && !cc->no_set_skip_hint)
		set_pageblock_skip(page);

	return skip;
}

static void update_cached_migrate(struct compact_control *cc, unsigned long pfn)
{
	struct zone *zone = cc->zone;

	 
	if (cc->no_set_skip_hint)
		return;

	pfn = pageblock_end_pfn(pfn);

	 
	if (pfn > zone->compact_cached_migrate_pfn[0])
		zone->compact_cached_migrate_pfn[0] = pfn;
	if (cc->mode != MIGRATE_ASYNC &&
	    pfn > zone->compact_cached_migrate_pfn[1])
		zone->compact_cached_migrate_pfn[1] = pfn;
}

 
static void update_pageblock_skip(struct compact_control *cc,
			struct page *page, unsigned long pfn)
{
	struct zone *zone = cc->zone;

	if (cc->no_set_skip_hint)
		return;

	set_pageblock_skip(page);

	if (pfn < zone->compact_cached_free_pfn)
		zone->compact_cached_free_pfn = pfn;
}
#else
static inline bool isolation_suitable(struct compact_control *cc,
					struct page *page)
{
	return true;
}

static inline bool pageblock_skip_persistent(struct page *page)
{
	return false;
}

static inline void update_pageblock_skip(struct compact_control *cc,
			struct page *page, unsigned long pfn)
{
}

static void update_cached_migrate(struct compact_control *cc, unsigned long pfn)
{
}

static bool test_and_set_skip(struct compact_control *cc, struct page *page)
{
	return false;
}
#endif  

 
static bool compact_lock_irqsave(spinlock_t *lock, unsigned long *flags,
						struct compact_control *cc)
	__acquires(lock)
{
	 
	if (cc->mode == MIGRATE_ASYNC && !cc->contended) {
		if (spin_trylock_irqsave(lock, *flags))
			return true;

		cc->contended = true;
	}

	spin_lock_irqsave(lock, *flags);
	return true;
}

 
static bool compact_unlock_should_abort(spinlock_t *lock,
		unsigned long flags, bool *locked, struct compact_control *cc)
{
	if (*locked) {
		spin_unlock_irqrestore(lock, flags);
		*locked = false;
	}

	if (fatal_signal_pending(current)) {
		cc->contended = true;
		return true;
	}

	cond_resched();

	return false;
}

 
static unsigned long isolate_freepages_block(struct compact_control *cc,
				unsigned long *start_pfn,
				unsigned long end_pfn,
				struct list_head *freelist,
				unsigned int stride,
				bool strict)
{
	int nr_scanned = 0, total_isolated = 0;
	struct page *page;
	unsigned long flags = 0;
	bool locked = false;
	unsigned long blockpfn = *start_pfn;
	unsigned int order;

	 
	if (strict)
		stride = 1;

	page = pfn_to_page(blockpfn);

	 
	for (; blockpfn < end_pfn; blockpfn += stride, page += stride) {
		int isolated;

		 
		if (!(blockpfn % COMPACT_CLUSTER_MAX)
		    && compact_unlock_should_abort(&cc->zone->lock, flags,
								&locked, cc))
			break;

		nr_scanned++;

		 
		if (PageCompound(page)) {
			const unsigned int order = compound_order(page);

			if (likely(order <= MAX_ORDER)) {
				blockpfn += (1UL << order) - 1;
				page += (1UL << order) - 1;
				nr_scanned += (1UL << order) - 1;
			}
			goto isolate_fail;
		}

		if (!PageBuddy(page))
			goto isolate_fail;

		 
		if (!locked) {
			locked = compact_lock_irqsave(&cc->zone->lock,
								&flags, cc);

			 
			if (!PageBuddy(page))
				goto isolate_fail;
		}

		 
		order = buddy_order(page);
		isolated = __isolate_free_page(page, order);
		if (!isolated)
			break;
		set_page_private(page, order);

		nr_scanned += isolated - 1;
		total_isolated += isolated;
		cc->nr_freepages += isolated;
		list_add_tail(&page->lru, freelist);

		if (!strict && cc->nr_migratepages <= cc->nr_freepages) {
			blockpfn += isolated;
			break;
		}
		 
		blockpfn += isolated - 1;
		page += isolated - 1;
		continue;

isolate_fail:
		if (strict)
			break;

	}

	if (locked)
		spin_unlock_irqrestore(&cc->zone->lock, flags);

	 
	if (unlikely(blockpfn > end_pfn))
		blockpfn = end_pfn;

	trace_mm_compaction_isolate_freepages(*start_pfn, blockpfn,
					nr_scanned, total_isolated);

	 
	*start_pfn = blockpfn;

	 
	if (strict && blockpfn < end_pfn)
		total_isolated = 0;

	cc->total_free_scanned += nr_scanned;
	if (total_isolated)
		count_compact_events(COMPACTISOLATED, total_isolated);
	return total_isolated;
}

 
unsigned long
isolate_freepages_range(struct compact_control *cc,
			unsigned long start_pfn, unsigned long end_pfn)
{
	unsigned long isolated, pfn, block_start_pfn, block_end_pfn;
	LIST_HEAD(freelist);

	pfn = start_pfn;
	block_start_pfn = pageblock_start_pfn(pfn);
	if (block_start_pfn < cc->zone->zone_start_pfn)
		block_start_pfn = cc->zone->zone_start_pfn;
	block_end_pfn = pageblock_end_pfn(pfn);

	for (; pfn < end_pfn; pfn += isolated,
				block_start_pfn = block_end_pfn,
				block_end_pfn += pageblock_nr_pages) {
		 
		unsigned long isolate_start_pfn = pfn;

		 
		if (pfn >= block_end_pfn) {
			block_start_pfn = pageblock_start_pfn(pfn);
			block_end_pfn = pageblock_end_pfn(pfn);
		}

		block_end_pfn = min(block_end_pfn, end_pfn);

		if (!pageblock_pfn_to_page(block_start_pfn,
					block_end_pfn, cc->zone))
			break;

		isolated = isolate_freepages_block(cc, &isolate_start_pfn,
					block_end_pfn, &freelist, 0, true);

		 
		if (!isolated)
			break;

		 
	}

	 
	split_map_pages(&freelist);

	if (pfn < end_pfn) {
		 
		release_freepages(&freelist);
		return 0;
	}

	 
	return pfn;
}

 
static bool too_many_isolated(struct compact_control *cc)
{
	pg_data_t *pgdat = cc->zone->zone_pgdat;
	bool too_many;

	unsigned long active, inactive, isolated;

	inactive = node_page_state(pgdat, NR_INACTIVE_FILE) +
			node_page_state(pgdat, NR_INACTIVE_ANON);
	active = node_page_state(pgdat, NR_ACTIVE_FILE) +
			node_page_state(pgdat, NR_ACTIVE_ANON);
	isolated = node_page_state(pgdat, NR_ISOLATED_FILE) +
			node_page_state(pgdat, NR_ISOLATED_ANON);

	 
	if (cc->gfp_mask & __GFP_FS) {
		inactive >>= 3;
		active >>= 3;
	}

	too_many = isolated > (inactive + active) / 2;
	if (!too_many)
		wake_throttle_isolated(pgdat);

	return too_many;
}

 
static int
isolate_migratepages_block(struct compact_control *cc, unsigned long low_pfn,
			unsigned long end_pfn, isolate_mode_t mode)
{
	pg_data_t *pgdat = cc->zone->zone_pgdat;
	unsigned long nr_scanned = 0, nr_isolated = 0;
	struct lruvec *lruvec;
	unsigned long flags = 0;
	struct lruvec *locked = NULL;
	struct folio *folio = NULL;
	struct page *page = NULL, *valid_page = NULL;
	struct address_space *mapping;
	unsigned long start_pfn = low_pfn;
	bool skip_on_failure = false;
	unsigned long next_skip_pfn = 0;
	bool skip_updated = false;
	int ret = 0;

	cc->migrate_pfn = low_pfn;

	 
	while (unlikely(too_many_isolated(cc))) {
		 
		if (cc->nr_migratepages)
			return -EAGAIN;

		 
		if (cc->mode == MIGRATE_ASYNC)
			return -EAGAIN;

		reclaim_throttle(pgdat, VMSCAN_THROTTLE_ISOLATED);

		if (fatal_signal_pending(current))
			return -EINTR;
	}

	cond_resched();

	if (cc->direct_compaction && (cc->mode == MIGRATE_ASYNC)) {
		skip_on_failure = true;
		next_skip_pfn = block_end_pfn(low_pfn, cc->order);
	}

	 
	for (; low_pfn < end_pfn; low_pfn++) {

		if (skip_on_failure && low_pfn >= next_skip_pfn) {
			 
			if (nr_isolated)
				break;

			 
			next_skip_pfn = block_end_pfn(low_pfn, cc->order);
		}

		 
		if (!(low_pfn % COMPACT_CLUSTER_MAX)) {
			if (locked) {
				unlock_page_lruvec_irqrestore(locked, flags);
				locked = NULL;
			}

			if (fatal_signal_pending(current)) {
				cc->contended = true;
				ret = -EINTR;

				goto fatal_pending;
			}

			cond_resched();
		}

		nr_scanned++;

		page = pfn_to_page(low_pfn);

		 
		if (!valid_page && (pageblock_aligned(low_pfn) ||
				    low_pfn == cc->zone->zone_start_pfn)) {
			if (!isolation_suitable(cc, page)) {
				low_pfn = end_pfn;
				folio = NULL;
				goto isolate_abort;
			}
			valid_page = page;
		}

		if (PageHuge(page) && cc->alloc_contig) {
			if (locked) {
				unlock_page_lruvec_irqrestore(locked, flags);
				locked = NULL;
			}

			ret = isolate_or_dissolve_huge_page(page, &cc->migratepages);

			 
			if (ret < 0) {
				  
				if (ret == -EBUSY)
					ret = 0;
				low_pfn += compound_nr(page) - 1;
				nr_scanned += compound_nr(page) - 1;
				goto isolate_fail;
			}

			if (PageHuge(page)) {
				 
				folio = page_folio(page);
				low_pfn += folio_nr_pages(folio) - 1;
				goto isolate_success_no_list;
			}

			 
		}

		 
		if (PageBuddy(page)) {
			unsigned long freepage_order = buddy_order_unsafe(page);

			 
			if (freepage_order > 0 && freepage_order <= MAX_ORDER) {
				low_pfn += (1UL << freepage_order) - 1;
				nr_scanned += (1UL << freepage_order) - 1;
			}
			continue;
		}

		 
		if (PageCompound(page) && !cc->alloc_contig) {
			const unsigned int order = compound_order(page);

			if (likely(order <= MAX_ORDER)) {
				low_pfn += (1UL << order) - 1;
				nr_scanned += (1UL << order) - 1;
			}
			goto isolate_fail;
		}

		 
		if (!PageLRU(page)) {
			 
			if (unlikely(__PageMovable(page)) &&
					!PageIsolated(page)) {
				if (locked) {
					unlock_page_lruvec_irqrestore(locked, flags);
					locked = NULL;
				}

				if (isolate_movable_page(page, mode)) {
					folio = page_folio(page);
					goto isolate_success;
				}
			}

			goto isolate_fail;
		}

		 
		folio = folio_get_nontail_page(page);
		if (unlikely(!folio))
			goto isolate_fail;

		 
		mapping = folio_mapping(folio);
		if (!mapping && (folio_ref_count(folio) - 1) > folio_mapcount(folio))
			goto isolate_fail_put;

		 
		if (!(cc->gfp_mask & __GFP_FS) && mapping)
			goto isolate_fail_put;

		 
		if (!folio_test_lru(folio))
			goto isolate_fail_put;

		 
		if (!(mode & ISOLATE_UNEVICTABLE) && folio_test_unevictable(folio))
			goto isolate_fail_put;

		 
		if ((mode & ISOLATE_ASYNC_MIGRATE) && folio_test_writeback(folio))
			goto isolate_fail_put;

		if ((mode & ISOLATE_ASYNC_MIGRATE) && folio_test_dirty(folio)) {
			bool migrate_dirty;

			 
			if (!folio_trylock(folio))
				goto isolate_fail_put;

			mapping = folio_mapping(folio);
			migrate_dirty = !mapping ||
					mapping->a_ops->migrate_folio;
			folio_unlock(folio);
			if (!migrate_dirty)
				goto isolate_fail_put;
		}

		 
		if (!folio_test_clear_lru(folio))
			goto isolate_fail_put;

		lruvec = folio_lruvec(folio);

		 
		if (lruvec != locked) {
			if (locked)
				unlock_page_lruvec_irqrestore(locked, flags);

			compact_lock_irqsave(&lruvec->lru_lock, &flags, cc);
			locked = lruvec;

			lruvec_memcg_debug(lruvec, folio);

			 
			if (!skip_updated && valid_page) {
				skip_updated = true;
				if (test_and_set_skip(cc, valid_page) &&
				    !cc->finish_pageblock) {
					low_pfn = end_pfn;
					goto isolate_abort;
				}
			}

			 
			if (unlikely(folio_test_large(folio) && !cc->alloc_contig)) {
				low_pfn += folio_nr_pages(folio) - 1;
				nr_scanned += folio_nr_pages(folio) - 1;
				folio_set_lru(folio);
				goto isolate_fail_put;
			}
		}

		 
		if (folio_test_large(folio))
			low_pfn += folio_nr_pages(folio) - 1;

		 
		lruvec_del_folio(lruvec, folio);
		node_stat_mod_folio(folio,
				NR_ISOLATED_ANON + folio_is_file_lru(folio),
				folio_nr_pages(folio));

isolate_success:
		list_add(&folio->lru, &cc->migratepages);
isolate_success_no_list:
		cc->nr_migratepages += folio_nr_pages(folio);
		nr_isolated += folio_nr_pages(folio);
		nr_scanned += folio_nr_pages(folio) - 1;

		 
		if (cc->nr_migratepages >= COMPACT_CLUSTER_MAX &&
		    !cc->finish_pageblock && !cc->contended) {
			++low_pfn;
			break;
		}

		continue;

isolate_fail_put:
		 
		if (locked) {
			unlock_page_lruvec_irqrestore(locked, flags);
			locked = NULL;
		}
		folio_put(folio);

isolate_fail:
		if (!skip_on_failure && ret != -ENOMEM)
			continue;

		 
		if (nr_isolated) {
			if (locked) {
				unlock_page_lruvec_irqrestore(locked, flags);
				locked = NULL;
			}
			putback_movable_pages(&cc->migratepages);
			cc->nr_migratepages = 0;
			nr_isolated = 0;
		}

		if (low_pfn < next_skip_pfn) {
			low_pfn = next_skip_pfn - 1;
			 
			next_skip_pfn += 1UL << cc->order;
		}

		if (ret == -ENOMEM)
			break;
	}

	 
	if (unlikely(low_pfn > end_pfn))
		low_pfn = end_pfn;

	folio = NULL;

isolate_abort:
	if (locked)
		unlock_page_lruvec_irqrestore(locked, flags);
	if (folio) {
		folio_set_lru(folio);
		folio_put(folio);
	}

	 
	if (low_pfn == end_pfn && (!nr_isolated || cc->finish_pageblock)) {
		if (!cc->no_set_skip_hint && valid_page && !skip_updated)
			set_pageblock_skip(valid_page);
		update_cached_migrate(cc, low_pfn);
	}

	trace_mm_compaction_isolate_migratepages(start_pfn, low_pfn,
						nr_scanned, nr_isolated);

fatal_pending:
	cc->total_migrate_scanned += nr_scanned;
	if (nr_isolated)
		count_compact_events(COMPACTISOLATED, nr_isolated);

	cc->migrate_pfn = low_pfn;

	return ret;
}

 
int
isolate_migratepages_range(struct compact_control *cc, unsigned long start_pfn,
							unsigned long end_pfn)
{
	unsigned long pfn, block_start_pfn, block_end_pfn;
	int ret = 0;

	 
	pfn = start_pfn;
	block_start_pfn = pageblock_start_pfn(pfn);
	if (block_start_pfn < cc->zone->zone_start_pfn)
		block_start_pfn = cc->zone->zone_start_pfn;
	block_end_pfn = pageblock_end_pfn(pfn);

	for (; pfn < end_pfn; pfn = block_end_pfn,
				block_start_pfn = block_end_pfn,
				block_end_pfn += pageblock_nr_pages) {

		block_end_pfn = min(block_end_pfn, end_pfn);

		if (!pageblock_pfn_to_page(block_start_pfn,
					block_end_pfn, cc->zone))
			continue;

		ret = isolate_migratepages_block(cc, pfn, block_end_pfn,
						 ISOLATE_UNEVICTABLE);

		if (ret)
			break;

		if (cc->nr_migratepages >= COMPACT_CLUSTER_MAX)
			break;
	}

	return ret;
}

#endif  
#ifdef CONFIG_COMPACTION

static bool suitable_migration_source(struct compact_control *cc,
							struct page *page)
{
	int block_mt;

	if (pageblock_skip_persistent(page))
		return false;

	if ((cc->mode != MIGRATE_ASYNC) || !cc->direct_compaction)
		return true;

	block_mt = get_pageblock_migratetype(page);

	if (cc->migratetype == MIGRATE_MOVABLE)
		return is_migrate_movable(block_mt);
	else
		return block_mt == cc->migratetype;
}

 
static bool suitable_migration_target(struct compact_control *cc,
							struct page *page)
{
	 
	if (PageBuddy(page)) {
		 
		if (buddy_order_unsafe(page) >= pageblock_order)
			return false;
	}

	if (cc->ignore_block_suitable)
		return true;

	 
	if (is_migrate_movable(get_pageblock_migratetype(page)))
		return true;

	 
	return false;
}

static inline unsigned int
freelist_scan_limit(struct compact_control *cc)
{
	unsigned short shift = BITS_PER_LONG - 1;

	return (COMPACT_CLUSTER_MAX >> min(shift, cc->fast_search_fail)) + 1;
}

 
static inline bool compact_scanners_met(struct compact_control *cc)
{
	return (cc->free_pfn >> pageblock_order)
		<= (cc->migrate_pfn >> pageblock_order);
}

 
static void
move_freelist_head(struct list_head *freelist, struct page *freepage)
{
	LIST_HEAD(sublist);

	if (!list_is_last(freelist, &freepage->lru)) {
		list_cut_before(&sublist, freelist, &freepage->lru);
		list_splice_tail(&sublist, freelist);
	}
}

 
static void
move_freelist_tail(struct list_head *freelist, struct page *freepage)
{
	LIST_HEAD(sublist);

	if (!list_is_first(freelist, &freepage->lru)) {
		list_cut_position(&sublist, freelist, &freepage->lru);
		list_splice_tail(&sublist, freelist);
	}
}

static void
fast_isolate_around(struct compact_control *cc, unsigned long pfn)
{
	unsigned long start_pfn, end_pfn;
	struct page *page;

	 
	if (cc->nr_freepages >= cc->nr_migratepages)
		return;

	 
	if (cc->direct_compaction && cc->mode == MIGRATE_ASYNC)
		return;

	 
	start_pfn = max(pageblock_start_pfn(pfn), cc->zone->zone_start_pfn);
	end_pfn = min(pageblock_end_pfn(pfn), zone_end_pfn(cc->zone));

	page = pageblock_pfn_to_page(start_pfn, end_pfn, cc->zone);
	if (!page)
		return;

	isolate_freepages_block(cc, &start_pfn, end_pfn, &cc->freepages, 1, false);

	 
	if (start_pfn == end_pfn && !cc->no_set_skip_hint)
		set_pageblock_skip(page);
}

 
static int next_search_order(struct compact_control *cc, int order)
{
	order--;
	if (order < 0)
		order = cc->order - 1;

	 
	if (order == cc->search_order) {
		cc->search_order--;
		if (cc->search_order < 0)
			cc->search_order = cc->order - 1;
		return -1;
	}

	return order;
}

static void fast_isolate_freepages(struct compact_control *cc)
{
	unsigned int limit = max(1U, freelist_scan_limit(cc) >> 1);
	unsigned int nr_scanned = 0, total_isolated = 0;
	unsigned long low_pfn, min_pfn, highest = 0;
	unsigned long nr_isolated = 0;
	unsigned long distance;
	struct page *page = NULL;
	bool scan_start = false;
	int order;

	 
	if (cc->order <= 0)
		return;

	 
	if (cc->free_pfn >= cc->zone->compact_init_free_pfn) {
		limit = pageblock_nr_pages >> 1;
		scan_start = true;
	}

	 
	distance = (cc->free_pfn - cc->migrate_pfn);
	low_pfn = pageblock_start_pfn(cc->free_pfn - (distance >> 2));
	min_pfn = pageblock_start_pfn(cc->free_pfn - (distance >> 1));

	if (WARN_ON_ONCE(min_pfn > low_pfn))
		low_pfn = min_pfn;

	 
	cc->search_order = min_t(unsigned int, cc->order - 1, cc->search_order);

	for (order = cc->search_order;
	     !page && order >= 0;
	     order = next_search_order(cc, order)) {
		struct free_area *area = &cc->zone->free_area[order];
		struct list_head *freelist;
		struct page *freepage;
		unsigned long flags;
		unsigned int order_scanned = 0;
		unsigned long high_pfn = 0;

		if (!area->nr_free)
			continue;

		spin_lock_irqsave(&cc->zone->lock, flags);
		freelist = &area->free_list[MIGRATE_MOVABLE];
		list_for_each_entry_reverse(freepage, freelist, buddy_list) {
			unsigned long pfn;

			order_scanned++;
			nr_scanned++;
			pfn = page_to_pfn(freepage);

			if (pfn >= highest)
				highest = max(pageblock_start_pfn(pfn),
					      cc->zone->zone_start_pfn);

			if (pfn >= low_pfn) {
				cc->fast_search_fail = 0;
				cc->search_order = order;
				page = freepage;
				break;
			}

			if (pfn >= min_pfn && pfn > high_pfn) {
				high_pfn = pfn;

				 
				limit >>= 1;
			}

			if (order_scanned >= limit)
				break;
		}

		 
		if (!page && high_pfn) {
			page = pfn_to_page(high_pfn);

			 
			freepage = page;
		}

		 
		move_freelist_head(freelist, freepage);

		 
		if (page) {
			if (__isolate_free_page(page, order)) {
				set_page_private(page, order);
				nr_isolated = 1 << order;
				nr_scanned += nr_isolated - 1;
				total_isolated += nr_isolated;
				cc->nr_freepages += nr_isolated;
				list_add_tail(&page->lru, &cc->freepages);
				count_compact_events(COMPACTISOLATED, nr_isolated);
			} else {
				 
				order = cc->search_order + 1;
				page = NULL;
			}
		}

		spin_unlock_irqrestore(&cc->zone->lock, flags);

		 
		if (cc->nr_freepages >= cc->nr_migratepages)
			break;

		 
		if (order_scanned >= limit)
			limit = max(1U, limit >> 1);
	}

	trace_mm_compaction_fast_isolate_freepages(min_pfn, cc->free_pfn,
						   nr_scanned, total_isolated);

	if (!page) {
		cc->fast_search_fail++;
		if (scan_start) {
			 
			if (highest >= min_pfn) {
				page = pfn_to_page(highest);
				cc->free_pfn = highest;
			} else {
				if (cc->direct_compaction && pfn_valid(min_pfn)) {
					page = pageblock_pfn_to_page(min_pfn,
						min(pageblock_end_pfn(min_pfn),
						    zone_end_pfn(cc->zone)),
						cc->zone);
					cc->free_pfn = min_pfn;
				}
			}
		}
	}

	if (highest && highest >= cc->zone->compact_cached_free_pfn) {
		highest -= pageblock_nr_pages;
		cc->zone->compact_cached_free_pfn = highest;
	}

	cc->total_free_scanned += nr_scanned;
	if (!page)
		return;

	low_pfn = page_to_pfn(page);
	fast_isolate_around(cc, low_pfn);
}

 
static void isolate_freepages(struct compact_control *cc)
{
	struct zone *zone = cc->zone;
	struct page *page;
	unsigned long block_start_pfn;	 
	unsigned long isolate_start_pfn;  
	unsigned long block_end_pfn;	 
	unsigned long low_pfn;	      
	struct list_head *freelist = &cc->freepages;
	unsigned int stride;

	 
	fast_isolate_freepages(cc);
	if (cc->nr_freepages)
		goto splitmap;

	 
	isolate_start_pfn = cc->free_pfn;
	block_start_pfn = pageblock_start_pfn(isolate_start_pfn);
	block_end_pfn = min(block_start_pfn + pageblock_nr_pages,
						zone_end_pfn(zone));
	low_pfn = pageblock_end_pfn(cc->migrate_pfn);
	stride = cc->mode == MIGRATE_ASYNC ? COMPACT_CLUSTER_MAX : 1;

	 
	for (; block_start_pfn >= low_pfn;
				block_end_pfn = block_start_pfn,
				block_start_pfn -= pageblock_nr_pages,
				isolate_start_pfn = block_start_pfn) {
		unsigned long nr_isolated;

		 
		if (!(block_start_pfn % (COMPACT_CLUSTER_MAX * pageblock_nr_pages)))
			cond_resched();

		page = pageblock_pfn_to_page(block_start_pfn, block_end_pfn,
									zone);
		if (!page) {
			unsigned long next_pfn;

			next_pfn = skip_offline_sections_reverse(block_start_pfn);
			if (next_pfn)
				block_start_pfn = max(next_pfn, low_pfn);

			continue;
		}

		 
		if (!suitable_migration_target(cc, page))
			continue;

		 
		if (!isolation_suitable(cc, page))
			continue;

		 
		nr_isolated = isolate_freepages_block(cc, &isolate_start_pfn,
					block_end_pfn, freelist, stride, false);

		 
		if (isolate_start_pfn == block_end_pfn)
			update_pageblock_skip(cc, page, block_start_pfn -
					      pageblock_nr_pages);

		 
		if (cc->nr_freepages >= cc->nr_migratepages) {
			if (isolate_start_pfn >= block_end_pfn) {
				 
				isolate_start_pfn =
					block_start_pfn - pageblock_nr_pages;
			}
			break;
		} else if (isolate_start_pfn < block_end_pfn) {
			 
			break;
		}

		 
		if (nr_isolated) {
			stride = 1;
			continue;
		}
		stride = min_t(unsigned int, COMPACT_CLUSTER_MAX, stride << 1);
	}

	 
	cc->free_pfn = isolate_start_pfn;

splitmap:
	 
	split_map_pages(freelist);
}

 
static struct folio *compaction_alloc(struct folio *src, unsigned long data)
{
	struct compact_control *cc = (struct compact_control *)data;
	struct folio *dst;

	if (list_empty(&cc->freepages)) {
		isolate_freepages(cc);

		if (list_empty(&cc->freepages))
			return NULL;
	}

	dst = list_entry(cc->freepages.next, struct folio, lru);
	list_del(&dst->lru);
	cc->nr_freepages--;

	return dst;
}

 
static void compaction_free(struct folio *dst, unsigned long data)
{
	struct compact_control *cc = (struct compact_control *)data;

	list_add(&dst->lru, &cc->freepages);
	cc->nr_freepages++;
}

 
typedef enum {
	ISOLATE_ABORT,		 
	ISOLATE_NONE,		 
	ISOLATE_SUCCESS,	 
} isolate_migrate_t;

 
static int sysctl_compact_unevictable_allowed __read_mostly = CONFIG_COMPACT_UNEVICTABLE_DEFAULT;
 
static unsigned int __read_mostly sysctl_compaction_proactiveness = 20;
static int sysctl_extfrag_threshold = 500;
static int __read_mostly sysctl_compact_memory;

static inline void
update_fast_start_pfn(struct compact_control *cc, unsigned long pfn)
{
	if (cc->fast_start_pfn == ULONG_MAX)
		return;

	if (!cc->fast_start_pfn)
		cc->fast_start_pfn = pfn;

	cc->fast_start_pfn = min(cc->fast_start_pfn, pfn);
}

static inline unsigned long
reinit_migrate_pfn(struct compact_control *cc)
{
	if (!cc->fast_start_pfn || cc->fast_start_pfn == ULONG_MAX)
		return cc->migrate_pfn;

	cc->migrate_pfn = cc->fast_start_pfn;
	cc->fast_start_pfn = ULONG_MAX;

	return cc->migrate_pfn;
}

 
static unsigned long fast_find_migrateblock(struct compact_control *cc)
{
	unsigned int limit = freelist_scan_limit(cc);
	unsigned int nr_scanned = 0;
	unsigned long distance;
	unsigned long pfn = cc->migrate_pfn;
	unsigned long high_pfn;
	int order;
	bool found_block = false;

	 
	if (cc->ignore_skip_hint)
		return pfn;

	 
	if (cc->finish_pageblock)
		return pfn;

	 
	if (pfn != cc->zone->zone_start_pfn && pfn != pageblock_start_pfn(pfn))
		return pfn;

	 
	if (cc->order <= PAGE_ALLOC_COSTLY_ORDER)
		return pfn;

	 
	if (cc->direct_compaction && cc->migratetype != MIGRATE_MOVABLE)
		return pfn;

	 
	distance = (cc->free_pfn - cc->migrate_pfn) >> 1;
	if (cc->migrate_pfn != cc->zone->zone_start_pfn)
		distance >>= 2;
	high_pfn = pageblock_start_pfn(cc->migrate_pfn + distance);

	for (order = cc->order - 1;
	     order >= PAGE_ALLOC_COSTLY_ORDER && !found_block && nr_scanned < limit;
	     order--) {
		struct free_area *area = &cc->zone->free_area[order];
		struct list_head *freelist;
		unsigned long flags;
		struct page *freepage;

		if (!area->nr_free)
			continue;

		spin_lock_irqsave(&cc->zone->lock, flags);
		freelist = &area->free_list[MIGRATE_MOVABLE];
		list_for_each_entry(freepage, freelist, buddy_list) {
			unsigned long free_pfn;

			if (nr_scanned++ >= limit) {
				move_freelist_tail(freelist, freepage);
				break;
			}

			free_pfn = page_to_pfn(freepage);
			if (free_pfn < high_pfn) {
				 
				if (get_pageblock_skip(freepage))
					continue;

				 
				move_freelist_tail(freelist, freepage);

				update_fast_start_pfn(cc, free_pfn);
				pfn = pageblock_start_pfn(free_pfn);
				if (pfn < cc->zone->zone_start_pfn)
					pfn = cc->zone->zone_start_pfn;
				cc->fast_search_fail = 0;
				found_block = true;
				break;
			}
		}
		spin_unlock_irqrestore(&cc->zone->lock, flags);
	}

	cc->total_migrate_scanned += nr_scanned;

	 
	if (!found_block) {
		cc->fast_search_fail++;
		pfn = reinit_migrate_pfn(cc);
	}
	return pfn;
}

 
static isolate_migrate_t isolate_migratepages(struct compact_control *cc)
{
	unsigned long block_start_pfn;
	unsigned long block_end_pfn;
	unsigned long low_pfn;
	struct page *page;
	const isolate_mode_t isolate_mode =
		(sysctl_compact_unevictable_allowed ? ISOLATE_UNEVICTABLE : 0) |
		(cc->mode != MIGRATE_SYNC ? ISOLATE_ASYNC_MIGRATE : 0);
	bool fast_find_block;

	 
	low_pfn = fast_find_migrateblock(cc);
	block_start_pfn = pageblock_start_pfn(low_pfn);
	if (block_start_pfn < cc->zone->zone_start_pfn)
		block_start_pfn = cc->zone->zone_start_pfn;

	 
	fast_find_block = low_pfn != cc->migrate_pfn && !cc->fast_search_fail;

	 
	block_end_pfn = pageblock_end_pfn(low_pfn);

	 
	for (; block_end_pfn <= cc->free_pfn;
			fast_find_block = false,
			cc->migrate_pfn = low_pfn = block_end_pfn,
			block_start_pfn = block_end_pfn,
			block_end_pfn += pageblock_nr_pages) {

		 
		if (!(low_pfn % (COMPACT_CLUSTER_MAX * pageblock_nr_pages)))
			cond_resched();

		page = pageblock_pfn_to_page(block_start_pfn,
						block_end_pfn, cc->zone);
		if (!page) {
			unsigned long next_pfn;

			next_pfn = skip_offline_sections(block_start_pfn);
			if (next_pfn)
				block_end_pfn = min(next_pfn, cc->free_pfn);
			continue;
		}

		 
		if ((pageblock_aligned(low_pfn) ||
		     low_pfn == cc->zone->zone_start_pfn) &&
		    !fast_find_block && !isolation_suitable(cc, page))
			continue;

		 
		if (!suitable_migration_source(cc, page)) {
			update_cached_migrate(cc, block_end_pfn);
			continue;
		}

		 
		if (isolate_migratepages_block(cc, low_pfn, block_end_pfn,
						isolate_mode))
			return ISOLATE_ABORT;

		 
		break;
	}

	return cc->nr_migratepages ? ISOLATE_SUCCESS : ISOLATE_NONE;
}

 
static inline bool is_via_compact_memory(int order)
{
	return order == -1;
}

 
static bool kswapd_is_running(pg_data_t *pgdat)
{
	bool running;

	pgdat_kswapd_lock(pgdat);
	running = pgdat->kswapd && task_is_running(pgdat->kswapd);
	pgdat_kswapd_unlock(pgdat);

	return running;
}

 
static unsigned int fragmentation_score_zone(struct zone *zone)
{
	return extfrag_for_order(zone, COMPACTION_HPAGE_ORDER);
}

 
static unsigned int fragmentation_score_zone_weighted(struct zone *zone)
{
	unsigned long score;

	score = zone->present_pages * fragmentation_score_zone(zone);
	return div64_ul(score, zone->zone_pgdat->node_present_pages + 1);
}

 
static unsigned int fragmentation_score_node(pg_data_t *pgdat)
{
	unsigned int score = 0;
	int zoneid;

	for (zoneid = 0; zoneid < MAX_NR_ZONES; zoneid++) {
		struct zone *zone;

		zone = &pgdat->node_zones[zoneid];
		if (!populated_zone(zone))
			continue;
		score += fragmentation_score_zone_weighted(zone);
	}

	return score;
}

static unsigned int fragmentation_score_wmark(bool low)
{
	unsigned int wmark_low;

	 
	wmark_low = max(100U - sysctl_compaction_proactiveness, 5U);
	return low ? wmark_low : min(wmark_low + 10, 100U);
}

static bool should_proactive_compact_node(pg_data_t *pgdat)
{
	int wmark_high;

	if (!sysctl_compaction_proactiveness || kswapd_is_running(pgdat))
		return false;

	wmark_high = fragmentation_score_wmark(false);
	return fragmentation_score_node(pgdat) > wmark_high;
}

static enum compact_result __compact_finished(struct compact_control *cc)
{
	unsigned int order;
	const int migratetype = cc->migratetype;
	int ret;

	 
	if (compact_scanners_met(cc)) {
		 
		reset_cached_positions(cc->zone);

		 
		if (cc->direct_compaction)
			cc->zone->compact_blockskip_flush = true;

		if (cc->whole_zone)
			return COMPACT_COMPLETE;
		else
			return COMPACT_PARTIAL_SKIPPED;
	}

	if (cc->proactive_compaction) {
		int score, wmark_low;
		pg_data_t *pgdat;

		pgdat = cc->zone->zone_pgdat;
		if (kswapd_is_running(pgdat))
			return COMPACT_PARTIAL_SKIPPED;

		score = fragmentation_score_zone(cc->zone);
		wmark_low = fragmentation_score_wmark(true);

		if (score > wmark_low)
			ret = COMPACT_CONTINUE;
		else
			ret = COMPACT_SUCCESS;

		goto out;
	}

	if (is_via_compact_memory(cc->order))
		return COMPACT_CONTINUE;

	 
	if (!pageblock_aligned(cc->migrate_pfn))
		return COMPACT_CONTINUE;

	 
	ret = COMPACT_NO_SUITABLE_PAGE;
	for (order = cc->order; order <= MAX_ORDER; order++) {
		struct free_area *area = &cc->zone->free_area[order];
		bool can_steal;

		 
		if (!free_area_empty(area, migratetype))
			return COMPACT_SUCCESS;

#ifdef CONFIG_CMA
		 
		if (migratetype == MIGRATE_MOVABLE &&
			!free_area_empty(area, MIGRATE_CMA))
			return COMPACT_SUCCESS;
#endif
		 
		if (find_suitable_fallback(area, order, migratetype,
						true, &can_steal) != -1)
			 
			return COMPACT_SUCCESS;
	}

out:
	if (cc->contended || fatal_signal_pending(current))
		ret = COMPACT_CONTENDED;

	return ret;
}

static enum compact_result compact_finished(struct compact_control *cc)
{
	int ret;

	ret = __compact_finished(cc);
	trace_mm_compaction_finished(cc->zone, cc->order, ret);
	if (ret == COMPACT_NO_SUITABLE_PAGE)
		ret = COMPACT_CONTINUE;

	return ret;
}

static bool __compaction_suitable(struct zone *zone, int order,
				  int highest_zoneidx,
				  unsigned long wmark_target)
{
	unsigned long watermark;
	 
	watermark = (order > PAGE_ALLOC_COSTLY_ORDER) ?
				low_wmark_pages(zone) : min_wmark_pages(zone);
	watermark += compact_gap(order);
	return __zone_watermark_ok(zone, 0, watermark, highest_zoneidx,
				   ALLOC_CMA, wmark_target);
}

 
bool compaction_suitable(struct zone *zone, int order, int highest_zoneidx)
{
	enum compact_result compact_result;
	bool suitable;

	suitable = __compaction_suitable(zone, order, highest_zoneidx,
					 zone_page_state(zone, NR_FREE_PAGES));
	 
	if (suitable) {
		compact_result = COMPACT_CONTINUE;
		if (order > PAGE_ALLOC_COSTLY_ORDER) {
			int fragindex = fragmentation_index(zone, order);

			if (fragindex >= 0 &&
			    fragindex <= sysctl_extfrag_threshold) {
				suitable = false;
				compact_result = COMPACT_NOT_SUITABLE_ZONE;
			}
		}
	} else {
		compact_result = COMPACT_SKIPPED;
	}

	trace_mm_compaction_suitable(zone, order, compact_result);

	return suitable;
}

bool compaction_zonelist_suitable(struct alloc_context *ac, int order,
		int alloc_flags)
{
	struct zone *zone;
	struct zoneref *z;

	 
	for_each_zone_zonelist_nodemask(zone, z, ac->zonelist,
				ac->highest_zoneidx, ac->nodemask) {
		unsigned long available;

		 
		available = zone_reclaimable_pages(zone) / order;
		available += zone_page_state_snapshot(zone, NR_FREE_PAGES);
		if (__compaction_suitable(zone, order, ac->highest_zoneidx,
					  available))
			return true;
	}

	return false;
}

static enum compact_result
compact_zone(struct compact_control *cc, struct capture_control *capc)
{
	enum compact_result ret;
	unsigned long start_pfn = cc->zone->zone_start_pfn;
	unsigned long end_pfn = zone_end_pfn(cc->zone);
	unsigned long last_migrated_pfn;
	const bool sync = cc->mode != MIGRATE_ASYNC;
	bool update_cached;
	unsigned int nr_succeeded = 0;

	 
	cc->total_migrate_scanned = 0;
	cc->total_free_scanned = 0;
	cc->nr_migratepages = 0;
	cc->nr_freepages = 0;
	INIT_LIST_HEAD(&cc->freepages);
	INIT_LIST_HEAD(&cc->migratepages);

	cc->migratetype = gfp_migratetype(cc->gfp_mask);

	if (!is_via_compact_memory(cc->order)) {
		unsigned long watermark;

		 
		watermark = wmark_pages(cc->zone,
					cc->alloc_flags & ALLOC_WMARK_MASK);
		if (zone_watermark_ok(cc->zone, cc->order, watermark,
				      cc->highest_zoneidx, cc->alloc_flags))
			return COMPACT_SUCCESS;

		 
		if (!compaction_suitable(cc->zone, cc->order,
					 cc->highest_zoneidx))
			return COMPACT_SKIPPED;
	}

	 
	if (compaction_restarting(cc->zone, cc->order))
		__reset_isolation_suitable(cc->zone);

	 
	cc->fast_start_pfn = 0;
	if (cc->whole_zone) {
		cc->migrate_pfn = start_pfn;
		cc->free_pfn = pageblock_start_pfn(end_pfn - 1);
	} else {
		cc->migrate_pfn = cc->zone->compact_cached_migrate_pfn[sync];
		cc->free_pfn = cc->zone->compact_cached_free_pfn;
		if (cc->free_pfn < start_pfn || cc->free_pfn >= end_pfn) {
			cc->free_pfn = pageblock_start_pfn(end_pfn - 1);
			cc->zone->compact_cached_free_pfn = cc->free_pfn;
		}
		if (cc->migrate_pfn < start_pfn || cc->migrate_pfn >= end_pfn) {
			cc->migrate_pfn = start_pfn;
			cc->zone->compact_cached_migrate_pfn[0] = cc->migrate_pfn;
			cc->zone->compact_cached_migrate_pfn[1] = cc->migrate_pfn;
		}

		if (cc->migrate_pfn <= cc->zone->compact_init_migrate_pfn)
			cc->whole_zone = true;
	}

	last_migrated_pfn = 0;

	 
	update_cached = !sync &&
		cc->zone->compact_cached_migrate_pfn[0] == cc->zone->compact_cached_migrate_pfn[1];

	trace_mm_compaction_begin(cc, start_pfn, end_pfn, sync);

	 
	lru_add_drain();

	while ((ret = compact_finished(cc)) == COMPACT_CONTINUE) {
		int err;
		unsigned long iteration_start_pfn = cc->migrate_pfn;

		 
		cc->finish_pageblock = false;
		if (pageblock_start_pfn(last_migrated_pfn) ==
		    pageblock_start_pfn(iteration_start_pfn)) {
			cc->finish_pageblock = true;
		}

rescan:
		switch (isolate_migratepages(cc)) {
		case ISOLATE_ABORT:
			ret = COMPACT_CONTENDED;
			putback_movable_pages(&cc->migratepages);
			cc->nr_migratepages = 0;
			goto out;
		case ISOLATE_NONE:
			if (update_cached) {
				cc->zone->compact_cached_migrate_pfn[1] =
					cc->zone->compact_cached_migrate_pfn[0];
			}

			 
			goto check_drain;
		case ISOLATE_SUCCESS:
			update_cached = false;
			last_migrated_pfn = max(cc->zone->zone_start_pfn,
				pageblock_start_pfn(cc->migrate_pfn - 1));
		}

		err = migrate_pages(&cc->migratepages, compaction_alloc,
				compaction_free, (unsigned long)cc, cc->mode,
				MR_COMPACTION, &nr_succeeded);

		trace_mm_compaction_migratepages(cc, nr_succeeded);

		 
		cc->nr_migratepages = 0;
		if (err) {
			putback_movable_pages(&cc->migratepages);
			 
			if (err == -ENOMEM && !compact_scanners_met(cc)) {
				ret = COMPACT_CONTENDED;
				goto out;
			}
			 
			if (!pageblock_aligned(cc->migrate_pfn) &&
			    !cc->ignore_skip_hint && !cc->finish_pageblock &&
			    (cc->mode < MIGRATE_SYNC)) {
				cc->finish_pageblock = true;

				 
				if (cc->order == COMPACTION_HPAGE_ORDER)
					last_migrated_pfn = 0;

				goto rescan;
			}
		}

		 
		if (capc && capc->page) {
			ret = COMPACT_SUCCESS;
			break;
		}

check_drain:
		 
		if (cc->order > 0 && last_migrated_pfn) {
			unsigned long current_block_start =
				block_start_pfn(cc->migrate_pfn, cc->order);

			if (last_migrated_pfn < current_block_start) {
				lru_add_drain_cpu_zone(cc->zone);
				 
				last_migrated_pfn = 0;
			}
		}
	}

out:
	 
	if (cc->nr_freepages > 0) {
		unsigned long free_pfn = release_freepages(&cc->freepages);

		cc->nr_freepages = 0;
		VM_BUG_ON(free_pfn == 0);
		 
		free_pfn = pageblock_start_pfn(free_pfn);
		 
		if (free_pfn > cc->zone->compact_cached_free_pfn)
			cc->zone->compact_cached_free_pfn = free_pfn;
	}

	count_compact_events(COMPACTMIGRATE_SCANNED, cc->total_migrate_scanned);
	count_compact_events(COMPACTFREE_SCANNED, cc->total_free_scanned);

	trace_mm_compaction_end(cc, start_pfn, end_pfn, sync, ret);

	VM_BUG_ON(!list_empty(&cc->freepages));
	VM_BUG_ON(!list_empty(&cc->migratepages));

	return ret;
}

static enum compact_result compact_zone_order(struct zone *zone, int order,
		gfp_t gfp_mask, enum compact_priority prio,
		unsigned int alloc_flags, int highest_zoneidx,
		struct page **capture)
{
	enum compact_result ret;
	struct compact_control cc = {
		.order = order,
		.search_order = order,
		.gfp_mask = gfp_mask,
		.zone = zone,
		.mode = (prio == COMPACT_PRIO_ASYNC) ?
					MIGRATE_ASYNC :	MIGRATE_SYNC_LIGHT,
		.alloc_flags = alloc_flags,
		.highest_zoneidx = highest_zoneidx,
		.direct_compaction = true,
		.whole_zone = (prio == MIN_COMPACT_PRIORITY),
		.ignore_skip_hint = (prio == MIN_COMPACT_PRIORITY),
		.ignore_block_suitable = (prio == MIN_COMPACT_PRIORITY)
	};
	struct capture_control capc = {
		.cc = &cc,
		.page = NULL,
	};

	 
	barrier();
	WRITE_ONCE(current->capture_control, &capc);

	ret = compact_zone(&cc, &capc);

	 
	WRITE_ONCE(current->capture_control, NULL);
	*capture = READ_ONCE(capc.page);
	 
	if (*capture)
		ret = COMPACT_SUCCESS;

	return ret;
}

 
enum compact_result try_to_compact_pages(gfp_t gfp_mask, unsigned int order,
		unsigned int alloc_flags, const struct alloc_context *ac,
		enum compact_priority prio, struct page **capture)
{
	int may_perform_io = (__force int)(gfp_mask & __GFP_IO);
	struct zoneref *z;
	struct zone *zone;
	enum compact_result rc = COMPACT_SKIPPED;

	 
	if (!may_perform_io)
		return COMPACT_SKIPPED;

	trace_mm_compaction_try_to_compact_pages(order, gfp_mask, prio);

	 
	for_each_zone_zonelist_nodemask(zone, z, ac->zonelist,
					ac->highest_zoneidx, ac->nodemask) {
		enum compact_result status;

		if (prio > MIN_COMPACT_PRIORITY
					&& compaction_deferred(zone, order)) {
			rc = max_t(enum compact_result, COMPACT_DEFERRED, rc);
			continue;
		}

		status = compact_zone_order(zone, order, gfp_mask, prio,
				alloc_flags, ac->highest_zoneidx, capture);
		rc = max(status, rc);

		 
		if (status == COMPACT_SUCCESS) {
			 
			compaction_defer_reset(zone, order, false);

			break;
		}

		if (prio != COMPACT_PRIO_ASYNC && (status == COMPACT_COMPLETE ||
					status == COMPACT_PARTIAL_SKIPPED))
			 
			defer_compaction(zone, order);

		 
		if ((prio == COMPACT_PRIO_ASYNC && need_resched())
					|| fatal_signal_pending(current))
			break;
	}

	return rc;
}

 
static void proactive_compact_node(pg_data_t *pgdat)
{
	int zoneid;
	struct zone *zone;
	struct compact_control cc = {
		.order = -1,
		.mode = MIGRATE_SYNC_LIGHT,
		.ignore_skip_hint = true,
		.whole_zone = true,
		.gfp_mask = GFP_KERNEL,
		.proactive_compaction = true,
	};

	for (zoneid = 0; zoneid < MAX_NR_ZONES; zoneid++) {
		zone = &pgdat->node_zones[zoneid];
		if (!populated_zone(zone))
			continue;

		cc.zone = zone;

		compact_zone(&cc, NULL);

		count_compact_events(KCOMPACTD_MIGRATE_SCANNED,
				     cc.total_migrate_scanned);
		count_compact_events(KCOMPACTD_FREE_SCANNED,
				     cc.total_free_scanned);
	}
}

 
static void compact_node(int nid)
{
	pg_data_t *pgdat = NODE_DATA(nid);
	int zoneid;
	struct zone *zone;
	struct compact_control cc = {
		.order = -1,
		.mode = MIGRATE_SYNC,
		.ignore_skip_hint = true,
		.whole_zone = true,
		.gfp_mask = GFP_KERNEL,
	};


	for (zoneid = 0; zoneid < MAX_NR_ZONES; zoneid++) {

		zone = &pgdat->node_zones[zoneid];
		if (!populated_zone(zone))
			continue;

		cc.zone = zone;

		compact_zone(&cc, NULL);
	}
}

 
static void compact_nodes(void)
{
	int nid;

	 
	lru_add_drain_all();

	for_each_online_node(nid)
		compact_node(nid);
}

static int compaction_proactiveness_sysctl_handler(struct ctl_table *table, int write,
		void *buffer, size_t *length, loff_t *ppos)
{
	int rc, nid;

	rc = proc_dointvec_minmax(table, write, buffer, length, ppos);
	if (rc)
		return rc;

	if (write && sysctl_compaction_proactiveness) {
		for_each_online_node(nid) {
			pg_data_t *pgdat = NODE_DATA(nid);

			if (pgdat->proactive_compact_trigger)
				continue;

			pgdat->proactive_compact_trigger = true;
			trace_mm_compaction_wakeup_kcompactd(pgdat->node_id, -1,
							     pgdat->nr_zones - 1);
			wake_up_interruptible(&pgdat->kcompactd_wait);
		}
	}

	return 0;
}

 
static int sysctl_compaction_handler(struct ctl_table *table, int write,
			void *buffer, size_t *length, loff_t *ppos)
{
	int ret;

	ret = proc_dointvec(table, write, buffer, length, ppos);
	if (ret)
		return ret;

	if (sysctl_compact_memory != 1)
		return -EINVAL;

	if (write)
		compact_nodes();

	return 0;
}

#if defined(CONFIG_SYSFS) && defined(CONFIG_NUMA)
static ssize_t compact_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int nid = dev->id;

	if (nid >= 0 && nid < nr_node_ids && node_online(nid)) {
		 
		lru_add_drain_all();

		compact_node(nid);
	}

	return count;
}
static DEVICE_ATTR_WO(compact);

int compaction_register_node(struct node *node)
{
	return device_create_file(&node->dev, &dev_attr_compact);
}

void compaction_unregister_node(struct node *node)
{
	device_remove_file(&node->dev, &dev_attr_compact);
}
#endif  

static inline bool kcompactd_work_requested(pg_data_t *pgdat)
{
	return pgdat->kcompactd_max_order > 0 || kthread_should_stop() ||
		pgdat->proactive_compact_trigger;
}

static bool kcompactd_node_suitable(pg_data_t *pgdat)
{
	int zoneid;
	struct zone *zone;
	enum zone_type highest_zoneidx = pgdat->kcompactd_highest_zoneidx;

	for (zoneid = 0; zoneid <= highest_zoneidx; zoneid++) {
		zone = &pgdat->node_zones[zoneid];

		if (!populated_zone(zone))
			continue;

		 
		if (zone_watermark_ok(zone, pgdat->kcompactd_max_order,
				      min_wmark_pages(zone),
				      highest_zoneidx, 0))
			continue;

		if (compaction_suitable(zone, pgdat->kcompactd_max_order,
					highest_zoneidx))
			return true;
	}

	return false;
}

static void kcompactd_do_work(pg_data_t *pgdat)
{
	 
	int zoneid;
	struct zone *zone;
	struct compact_control cc = {
		.order = pgdat->kcompactd_max_order,
		.search_order = pgdat->kcompactd_max_order,
		.highest_zoneidx = pgdat->kcompactd_highest_zoneidx,
		.mode = MIGRATE_SYNC_LIGHT,
		.ignore_skip_hint = false,
		.gfp_mask = GFP_KERNEL,
	};
	trace_mm_compaction_kcompactd_wake(pgdat->node_id, cc.order,
							cc.highest_zoneidx);
	count_compact_event(KCOMPACTD_WAKE);

	for (zoneid = 0; zoneid <= cc.highest_zoneidx; zoneid++) {
		int status;

		zone = &pgdat->node_zones[zoneid];
		if (!populated_zone(zone))
			continue;

		if (compaction_deferred(zone, cc.order))
			continue;

		 
		if (zone_watermark_ok(zone, cc.order,
				      min_wmark_pages(zone), zoneid, 0))
			continue;

		if (!compaction_suitable(zone, cc.order, zoneid))
			continue;

		if (kthread_should_stop())
			return;

		cc.zone = zone;
		status = compact_zone(&cc, NULL);

		if (status == COMPACT_SUCCESS) {
			compaction_defer_reset(zone, cc.order, false);
		} else if (status == COMPACT_PARTIAL_SKIPPED || status == COMPACT_COMPLETE) {
			 
			drain_all_pages(zone);

			 
			defer_compaction(zone, cc.order);
		}

		count_compact_events(KCOMPACTD_MIGRATE_SCANNED,
				     cc.total_migrate_scanned);
		count_compact_events(KCOMPACTD_FREE_SCANNED,
				     cc.total_free_scanned);
	}

	 
	if (pgdat->kcompactd_max_order <= cc.order)
		pgdat->kcompactd_max_order = 0;
	if (pgdat->kcompactd_highest_zoneidx >= cc.highest_zoneidx)
		pgdat->kcompactd_highest_zoneidx = pgdat->nr_zones - 1;
}

void wakeup_kcompactd(pg_data_t *pgdat, int order, int highest_zoneidx)
{
	if (!order)
		return;

	if (pgdat->kcompactd_max_order < order)
		pgdat->kcompactd_max_order = order;

	if (pgdat->kcompactd_highest_zoneidx > highest_zoneidx)
		pgdat->kcompactd_highest_zoneidx = highest_zoneidx;

	 
	if (!wq_has_sleeper(&pgdat->kcompactd_wait))
		return;

	if (!kcompactd_node_suitable(pgdat))
		return;

	trace_mm_compaction_wakeup_kcompactd(pgdat->node_id, order,
							highest_zoneidx);
	wake_up_interruptible(&pgdat->kcompactd_wait);
}

 
static int kcompactd(void *p)
{
	pg_data_t *pgdat = (pg_data_t *)p;
	struct task_struct *tsk = current;
	long default_timeout = msecs_to_jiffies(HPAGE_FRAG_CHECK_INTERVAL_MSEC);
	long timeout = default_timeout;

	const struct cpumask *cpumask = cpumask_of_node(pgdat->node_id);

	if (!cpumask_empty(cpumask))
		set_cpus_allowed_ptr(tsk, cpumask);

	set_freezable();

	pgdat->kcompactd_max_order = 0;
	pgdat->kcompactd_highest_zoneidx = pgdat->nr_zones - 1;

	while (!kthread_should_stop()) {
		unsigned long pflags;

		 
		if (!sysctl_compaction_proactiveness)
			timeout = MAX_SCHEDULE_TIMEOUT;
		trace_mm_compaction_kcompactd_sleep(pgdat->node_id);
		if (wait_event_freezable_timeout(pgdat->kcompactd_wait,
			kcompactd_work_requested(pgdat), timeout) &&
			!pgdat->proactive_compact_trigger) {

			psi_memstall_enter(&pflags);
			kcompactd_do_work(pgdat);
			psi_memstall_leave(&pflags);
			 
			timeout = default_timeout;
			continue;
		}

		 
		timeout = default_timeout;
		if (should_proactive_compact_node(pgdat)) {
			unsigned int prev_score, score;

			prev_score = fragmentation_score_node(pgdat);
			proactive_compact_node(pgdat);
			score = fragmentation_score_node(pgdat);
			 
			if (unlikely(score >= prev_score))
				timeout =
				   default_timeout << COMPACT_MAX_DEFER_SHIFT;
		}
		if (unlikely(pgdat->proactive_compact_trigger))
			pgdat->proactive_compact_trigger = false;
	}

	return 0;
}

 
void __meminit kcompactd_run(int nid)
{
	pg_data_t *pgdat = NODE_DATA(nid);

	if (pgdat->kcompactd)
		return;

	pgdat->kcompactd = kthread_run(kcompactd, pgdat, "kcompactd%d", nid);
	if (IS_ERR(pgdat->kcompactd)) {
		pr_err("Failed to start kcompactd on node %d\n", nid);
		pgdat->kcompactd = NULL;
	}
}

 
void __meminit kcompactd_stop(int nid)
{
	struct task_struct *kcompactd = NODE_DATA(nid)->kcompactd;

	if (kcompactd) {
		kthread_stop(kcompactd);
		NODE_DATA(nid)->kcompactd = NULL;
	}
}

 
static int kcompactd_cpu_online(unsigned int cpu)
{
	int nid;

	for_each_node_state(nid, N_MEMORY) {
		pg_data_t *pgdat = NODE_DATA(nid);
		const struct cpumask *mask;

		mask = cpumask_of_node(pgdat->node_id);

		if (cpumask_any_and(cpu_online_mask, mask) < nr_cpu_ids)
			 
			if (pgdat->kcompactd)
				set_cpus_allowed_ptr(pgdat->kcompactd, mask);
	}
	return 0;
}

static int proc_dointvec_minmax_warn_RT_change(struct ctl_table *table,
		int write, void *buffer, size_t *lenp, loff_t *ppos)
{
	int ret, old;

	if (!IS_ENABLED(CONFIG_PREEMPT_RT) || !write)
		return proc_dointvec_minmax(table, write, buffer, lenp, ppos);

	old = *(int *)table->data;
	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
	if (ret)
		return ret;
	if (old != *(int *)table->data)
		pr_warn_once("sysctl attribute %s changed by %s[%d]\n",
			     table->procname, current->comm,
			     task_pid_nr(current));
	return ret;
}

static struct ctl_table vm_compaction[] = {
	{
		.procname	= "compact_memory",
		.data		= &sysctl_compact_memory,
		.maxlen		= sizeof(int),
		.mode		= 0200,
		.proc_handler	= sysctl_compaction_handler,
	},
	{
		.procname	= "compaction_proactiveness",
		.data		= &sysctl_compaction_proactiveness,
		.maxlen		= sizeof(sysctl_compaction_proactiveness),
		.mode		= 0644,
		.proc_handler	= compaction_proactiveness_sysctl_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE_HUNDRED,
	},
	{
		.procname	= "extfrag_threshold",
		.data		= &sysctl_extfrag_threshold,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE_THOUSAND,
	},
	{
		.procname	= "compact_unevictable_allowed",
		.data		= &sysctl_compact_unevictable_allowed,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax_warn_RT_change,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{ }
};

static int __init kcompactd_init(void)
{
	int nid;
	int ret;

	ret = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
					"mm/compaction:online",
					kcompactd_cpu_online, NULL);
	if (ret < 0) {
		pr_err("kcompactd: failed to register hotplug callbacks.\n");
		return ret;
	}

	for_each_node_state(nid, N_MEMORY)
		kcompactd_run(nid);
	register_sysctl_init("vm", vm_compaction);
	return 0;
}
subsys_initcall(kcompactd_init)

#endif  
