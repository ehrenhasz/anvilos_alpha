
 

#include <linux/mm.h>
#include <linux/page-isolation.h>
#include <linux/pageblock-flags.h>
#include <linux/memory.h>
#include <linux/hugetlb.h>
#include <linux/page_owner.h>
#include <linux/migrate.h>
#include "internal.h"

#define CREATE_TRACE_POINTS
#include <trace/events/page_isolation.h>

 
static struct page *has_unmovable_pages(unsigned long start_pfn, unsigned long end_pfn,
				int migratetype, int flags)
{
	struct page *page = pfn_to_page(start_pfn);
	struct zone *zone = page_zone(page);
	unsigned long pfn;

	VM_BUG_ON(pageblock_start_pfn(start_pfn) !=
		  pageblock_start_pfn(end_pfn - 1));

	if (is_migrate_cma_page(page)) {
		 
		if (is_migrate_cma(migratetype))
			return NULL;

		return page;
	}

	for (pfn = start_pfn; pfn < end_pfn; pfn++) {
		page = pfn_to_page(pfn);

		 
		if (PageReserved(page))
			return page;

		 
		if (zone_idx(zone) == ZONE_MOVABLE)
			continue;

		 
		if (PageHuge(page) || PageTransCompound(page)) {
			struct folio *folio = page_folio(page);
			unsigned int skip_pages;

			if (PageHuge(page)) {
				if (!hugepage_migration_supported(folio_hstate(folio)))
					return page;
			} else if (!folio_test_lru(folio) && !__folio_test_movable(folio)) {
				return page;
			}

			skip_pages = folio_nr_pages(folio) - folio_page_idx(folio, page);
			pfn += skip_pages - 1;
			continue;
		}

		 
		if (!page_ref_count(page)) {
			if (PageBuddy(page))
				pfn += (1 << buddy_order(page)) - 1;
			continue;
		}

		 
		if ((flags & MEMORY_OFFLINE) && PageHWPoison(page))
			continue;

		 
		if ((flags & MEMORY_OFFLINE) && PageOffline(page))
			continue;

		if (__PageMovable(page) || PageLRU(page))
			continue;

		 
		return page;
	}
	return NULL;
}

 
static int set_migratetype_isolate(struct page *page, int migratetype, int isol_flags,
			unsigned long start_pfn, unsigned long end_pfn)
{
	struct zone *zone = page_zone(page);
	struct page *unmovable;
	unsigned long flags;
	unsigned long check_unmovable_start, check_unmovable_end;

	spin_lock_irqsave(&zone->lock, flags);

	 
	if (is_migrate_isolate_page(page)) {
		spin_unlock_irqrestore(&zone->lock, flags);
		return -EBUSY;
	}

	 
	check_unmovable_start = max(page_to_pfn(page), start_pfn);
	check_unmovable_end = min(pageblock_end_pfn(page_to_pfn(page)),
				  end_pfn);

	unmovable = has_unmovable_pages(check_unmovable_start, check_unmovable_end,
			migratetype, isol_flags);
	if (!unmovable) {
		unsigned long nr_pages;
		int mt = get_pageblock_migratetype(page);

		set_pageblock_migratetype(page, MIGRATE_ISOLATE);
		zone->nr_isolate_pageblock++;
		nr_pages = move_freepages_block(zone, page, MIGRATE_ISOLATE,
									NULL);

		__mod_zone_freepage_state(zone, -nr_pages, mt);
		spin_unlock_irqrestore(&zone->lock, flags);
		return 0;
	}

	spin_unlock_irqrestore(&zone->lock, flags);
	if (isol_flags & REPORT_FAILURE) {
		 
		dump_page(unmovable, "unmovable page");
	}

	return -EBUSY;
}

static void unset_migratetype_isolate(struct page *page, int migratetype)
{
	struct zone *zone;
	unsigned long flags, nr_pages;
	bool isolated_page = false;
	unsigned int order;
	struct page *buddy;

	zone = page_zone(page);
	spin_lock_irqsave(&zone->lock, flags);
	if (!is_migrate_isolate_page(page))
		goto out;

	 
	if (PageBuddy(page)) {
		order = buddy_order(page);
		if (order >= pageblock_order && order < MAX_ORDER) {
			buddy = find_buddy_page_pfn(page, page_to_pfn(page),
						    order, NULL);
			if (buddy && !is_migrate_isolate_page(buddy)) {
				isolated_page = !!__isolate_free_page(page, order);
				 
				VM_WARN_ON(!isolated_page);
			}
		}
	}

	 
	if (!isolated_page) {
		nr_pages = move_freepages_block(zone, page, migratetype, NULL);
		__mod_zone_freepage_state(zone, nr_pages, migratetype);
	}
	set_pageblock_migratetype(page, migratetype);
	if (isolated_page)
		__putback_isolated_page(page, order, migratetype);
	zone->nr_isolate_pageblock--;
out:
	spin_unlock_irqrestore(&zone->lock, flags);
}

static inline struct page *
__first_valid_page(unsigned long pfn, unsigned long nr_pages)
{
	int i;

	for (i = 0; i < nr_pages; i++) {
		struct page *page;

		page = pfn_to_online_page(pfn + i);
		if (!page)
			continue;
		return page;
	}
	return NULL;
}

 
static int isolate_single_pageblock(unsigned long boundary_pfn, int flags,
			gfp_t gfp_flags, bool isolate_before, bool skip_isolation,
			int migratetype)
{
	unsigned long start_pfn;
	unsigned long isolate_pageblock;
	unsigned long pfn;
	struct zone *zone;
	int ret;

	VM_BUG_ON(!pageblock_aligned(boundary_pfn));

	if (isolate_before)
		isolate_pageblock = boundary_pfn - pageblock_nr_pages;
	else
		isolate_pageblock = boundary_pfn;

	 
	zone  = page_zone(pfn_to_page(isolate_pageblock));
	start_pfn  = max(ALIGN_DOWN(isolate_pageblock, MAX_ORDER_NR_PAGES),
				      zone->zone_start_pfn);

	if (skip_isolation) {
		int mt __maybe_unused = get_pageblock_migratetype(pfn_to_page(isolate_pageblock));

		VM_BUG_ON(!is_migrate_isolate(mt));
	} else {
		ret = set_migratetype_isolate(pfn_to_page(isolate_pageblock), migratetype,
				flags, isolate_pageblock, isolate_pageblock + pageblock_nr_pages);

		if (ret)
			return ret;
	}

	 
	if (isolate_before) {
		if (!pfn_to_online_page(boundary_pfn))
			return 0;
	} else {
		if (!pfn_to_online_page(boundary_pfn - 1))
			return 0;
	}

	for (pfn = start_pfn; pfn < boundary_pfn;) {
		struct page *page = __first_valid_page(pfn, boundary_pfn - pfn);

		VM_BUG_ON(!page);
		pfn = page_to_pfn(page);
		 
		if (PageBuddy(page)) {
			int order = buddy_order(page);

			if (pfn + (1UL << order) > boundary_pfn) {
				 
				if (split_free_page(page, order, boundary_pfn - pfn))
					continue;
			}

			pfn += 1UL << order;
			continue;
		}
		 
		if (PageCompound(page)) {
			struct page *head = compound_head(page);
			unsigned long head_pfn = page_to_pfn(head);
			unsigned long nr_pages = compound_nr(head);

			if (head_pfn + nr_pages <= boundary_pfn) {
				pfn = head_pfn + nr_pages;
				continue;
			}
#if defined CONFIG_COMPACTION || defined CONFIG_CMA
			 
			if (PageHuge(page) || PageLRU(page) || __PageMovable(page)) {
				int order;
				unsigned long outer_pfn;
				int page_mt = get_pageblock_migratetype(page);
				bool isolate_page = !is_migrate_isolate_page(page);
				struct compact_control cc = {
					.nr_migratepages = 0,
					.order = -1,
					.zone = page_zone(pfn_to_page(head_pfn)),
					.mode = MIGRATE_SYNC,
					.ignore_skip_hint = true,
					.no_set_skip_hint = true,
					.gfp_mask = gfp_flags,
					.alloc_contig = true,
				};
				INIT_LIST_HEAD(&cc.migratepages);

				 
				if (isolate_page) {
					ret = set_migratetype_isolate(page, page_mt,
						flags, head_pfn, head_pfn + nr_pages);
					if (ret)
						goto failed;
				}

				ret = __alloc_contig_migrate_range(&cc, head_pfn,
							head_pfn + nr_pages);

				 
				if (isolate_page)
					unset_migratetype_isolate(page, page_mt);

				if (ret)
					goto failed;
				 
				order = 0;
				outer_pfn = pfn;
				while (!PageBuddy(pfn_to_page(outer_pfn))) {
					 
					if (++order > MAX_ORDER)
						goto failed;
					outer_pfn &= ~0UL << order;
				}
				pfn = outer_pfn;
				continue;
			} else
#endif
				goto failed;
		}

		pfn++;
	}
	return 0;
failed:
	 
	if (!skip_isolation)
		unset_migratetype_isolate(pfn_to_page(isolate_pageblock), migratetype);
	return -EBUSY;
}

 
int start_isolate_page_range(unsigned long start_pfn, unsigned long end_pfn,
			     int migratetype, int flags, gfp_t gfp_flags)
{
	unsigned long pfn;
	struct page *page;
	 
	unsigned long isolate_start = pageblock_start_pfn(start_pfn);
	unsigned long isolate_end = pageblock_align(end_pfn);
	int ret;
	bool skip_isolation = false;

	 
	ret = isolate_single_pageblock(isolate_start, flags, gfp_flags, false,
			skip_isolation, migratetype);
	if (ret)
		return ret;

	if (isolate_start == isolate_end - pageblock_nr_pages)
		skip_isolation = true;

	 
	ret = isolate_single_pageblock(isolate_end, flags, gfp_flags, true,
			skip_isolation, migratetype);
	if (ret) {
		unset_migratetype_isolate(pfn_to_page(isolate_start), migratetype);
		return ret;
	}

	 
	for (pfn = isolate_start + pageblock_nr_pages;
	     pfn < isolate_end - pageblock_nr_pages;
	     pfn += pageblock_nr_pages) {
		page = __first_valid_page(pfn, pageblock_nr_pages);
		if (page && set_migratetype_isolate(page, migratetype, flags,
					start_pfn, end_pfn)) {
			undo_isolate_page_range(isolate_start, pfn, migratetype);
			unset_migratetype_isolate(
				pfn_to_page(isolate_end - pageblock_nr_pages),
				migratetype);
			return -EBUSY;
		}
	}
	return 0;
}

 
void undo_isolate_page_range(unsigned long start_pfn, unsigned long end_pfn,
			    int migratetype)
{
	unsigned long pfn;
	struct page *page;
	unsigned long isolate_start = pageblock_start_pfn(start_pfn);
	unsigned long isolate_end = pageblock_align(end_pfn);

	for (pfn = isolate_start;
	     pfn < isolate_end;
	     pfn += pageblock_nr_pages) {
		page = __first_valid_page(pfn, pageblock_nr_pages);
		if (!page || !is_migrate_isolate_page(page))
			continue;
		unset_migratetype_isolate(page, migratetype);
	}
}
 
static unsigned long
__test_page_isolated_in_pageblock(unsigned long pfn, unsigned long end_pfn,
				  int flags)
{
	struct page *page;

	while (pfn < end_pfn) {
		page = pfn_to_page(pfn);
		if (PageBuddy(page))
			 
			pfn += 1 << buddy_order(page);
		else if ((flags & MEMORY_OFFLINE) && PageHWPoison(page))
			 
			pfn++;
		else if ((flags & MEMORY_OFFLINE) && PageOffline(page) &&
			 !page_count(page))
			 
			pfn++;
		else
			break;
	}

	return pfn;
}

 
int test_pages_isolated(unsigned long start_pfn, unsigned long end_pfn,
			int isol_flags)
{
	unsigned long pfn, flags;
	struct page *page;
	struct zone *zone;
	int ret;

	 
	for (pfn = start_pfn; pfn < end_pfn; pfn += pageblock_nr_pages) {
		page = __first_valid_page(pfn, pageblock_nr_pages);
		if (page && !is_migrate_isolate_page(page))
			break;
	}
	page = __first_valid_page(start_pfn, end_pfn - start_pfn);
	if ((pfn < end_pfn) || !page) {
		ret = -EBUSY;
		goto out;
	}

	 
	zone = page_zone(page);
	spin_lock_irqsave(&zone->lock, flags);
	pfn = __test_page_isolated_in_pageblock(start_pfn, end_pfn, isol_flags);
	spin_unlock_irqrestore(&zone->lock, flags);

	ret = pfn < end_pfn ? -EBUSY : 0;

out:
	trace_test_pages_isolated(start_pfn, end_pfn, pfn);

	return ret;
}
