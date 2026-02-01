
 

#define pr_fmt(fmt) "cma: " fmt

#ifdef CONFIG_CMA_DEBUG
#ifndef DEBUG
#  define DEBUG
#endif
#endif
#define CREATE_TRACE_POINTS

#include <linux/memblock.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/log2.h>
#include <linux/cma.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/kmemleak.h>
#include <trace/events/cma.h>

#include "internal.h"
#include "cma.h"

struct cma cma_areas[MAX_CMA_AREAS];
unsigned cma_area_count;
static DEFINE_MUTEX(cma_mutex);

phys_addr_t cma_get_base(const struct cma *cma)
{
	return PFN_PHYS(cma->base_pfn);
}

unsigned long cma_get_size(const struct cma *cma)
{
	return cma->count << PAGE_SHIFT;
}

const char *cma_get_name(const struct cma *cma)
{
	return cma->name;
}

static unsigned long cma_bitmap_aligned_mask(const struct cma *cma,
					     unsigned int align_order)
{
	if (align_order <= cma->order_per_bit)
		return 0;
	return (1UL << (align_order - cma->order_per_bit)) - 1;
}

 
static unsigned long cma_bitmap_aligned_offset(const struct cma *cma,
					       unsigned int align_order)
{
	return (cma->base_pfn & ((1UL << align_order) - 1))
		>> cma->order_per_bit;
}

static unsigned long cma_bitmap_pages_to_bits(const struct cma *cma,
					      unsigned long pages)
{
	return ALIGN(pages, 1UL << cma->order_per_bit) >> cma->order_per_bit;
}

static void cma_clear_bitmap(struct cma *cma, unsigned long pfn,
			     unsigned long count)
{
	unsigned long bitmap_no, bitmap_count;
	unsigned long flags;

	bitmap_no = (pfn - cma->base_pfn) >> cma->order_per_bit;
	bitmap_count = cma_bitmap_pages_to_bits(cma, count);

	spin_lock_irqsave(&cma->lock, flags);
	bitmap_clear(cma->bitmap, bitmap_no, bitmap_count);
	spin_unlock_irqrestore(&cma->lock, flags);
}

static void __init cma_activate_area(struct cma *cma)
{
	unsigned long base_pfn = cma->base_pfn, pfn;
	struct zone *zone;

	cma->bitmap = bitmap_zalloc(cma_bitmap_maxno(cma), GFP_KERNEL);
	if (!cma->bitmap)
		goto out_error;

	 
	WARN_ON_ONCE(!pfn_valid(base_pfn));
	zone = page_zone(pfn_to_page(base_pfn));
	for (pfn = base_pfn + 1; pfn < base_pfn + cma->count; pfn++) {
		WARN_ON_ONCE(!pfn_valid(pfn));
		if (page_zone(pfn_to_page(pfn)) != zone)
			goto not_in_zone;
	}

	for (pfn = base_pfn; pfn < base_pfn + cma->count;
	     pfn += pageblock_nr_pages)
		init_cma_reserved_pageblock(pfn_to_page(pfn));

	spin_lock_init(&cma->lock);

#ifdef CONFIG_CMA_DEBUGFS
	INIT_HLIST_HEAD(&cma->mem_head);
	spin_lock_init(&cma->mem_head_lock);
#endif

	return;

not_in_zone:
	bitmap_free(cma->bitmap);
out_error:
	 
	if (!cma->reserve_pages_on_error) {
		for (pfn = base_pfn; pfn < base_pfn + cma->count; pfn++)
			free_reserved_page(pfn_to_page(pfn));
	}
	totalcma_pages -= cma->count;
	cma->count = 0;
	pr_err("CMA area %s could not be activated\n", cma->name);
	return;
}

static int __init cma_init_reserved_areas(void)
{
	int i;

	for (i = 0; i < cma_area_count; i++)
		cma_activate_area(&cma_areas[i]);

	return 0;
}
core_initcall(cma_init_reserved_areas);

void __init cma_reserve_pages_on_error(struct cma *cma)
{
	cma->reserve_pages_on_error = true;
}

 
int __init cma_init_reserved_mem(phys_addr_t base, phys_addr_t size,
				 unsigned int order_per_bit,
				 const char *name,
				 struct cma **res_cma)
{
	struct cma *cma;

	 
	if (cma_area_count == ARRAY_SIZE(cma_areas)) {
		pr_err("Not enough slots for CMA reserved regions!\n");
		return -ENOSPC;
	}

	if (!size || !memblock_is_region_reserved(base, size))
		return -EINVAL;

	 
	if (!IS_ALIGNED(CMA_MIN_ALIGNMENT_PAGES, 1 << order_per_bit))
		return -EINVAL;

	 
	if (!IS_ALIGNED(base | size, CMA_MIN_ALIGNMENT_BYTES))
		return -EINVAL;

	 
	cma = &cma_areas[cma_area_count];

	if (name)
		snprintf(cma->name, CMA_MAX_NAME, name);
	else
		snprintf(cma->name, CMA_MAX_NAME,  "cma%d\n", cma_area_count);

	cma->base_pfn = PFN_DOWN(base);
	cma->count = size >> PAGE_SHIFT;
	cma->order_per_bit = order_per_bit;
	*res_cma = cma;
	cma_area_count++;
	totalcma_pages += (size / PAGE_SIZE);

	return 0;
}

 
int __init cma_declare_contiguous_nid(phys_addr_t base,
			phys_addr_t size, phys_addr_t limit,
			phys_addr_t alignment, unsigned int order_per_bit,
			bool fixed, const char *name, struct cma **res_cma,
			int nid)
{
	phys_addr_t memblock_end = memblock_end_of_DRAM();
	phys_addr_t highmem_start;
	int ret = 0;

	 
	highmem_start = __pa(high_memory - 1) + 1;
	pr_debug("%s(size %pa, base %pa, limit %pa alignment %pa)\n",
		__func__, &size, &base, &limit, &alignment);

	if (cma_area_count == ARRAY_SIZE(cma_areas)) {
		pr_err("Not enough slots for CMA reserved regions!\n");
		return -ENOSPC;
	}

	if (!size)
		return -EINVAL;

	if (alignment && !is_power_of_2(alignment))
		return -EINVAL;

	if (!IS_ENABLED(CONFIG_NUMA))
		nid = NUMA_NO_NODE;

	 
	alignment = max_t(phys_addr_t, alignment, CMA_MIN_ALIGNMENT_BYTES);
	if (fixed && base & (alignment - 1)) {
		ret = -EINVAL;
		pr_err("Region at %pa must be aligned to %pa bytes\n",
			&base, &alignment);
		goto err;
	}
	base = ALIGN(base, alignment);
	size = ALIGN(size, alignment);
	limit &= ~(alignment - 1);

	if (!base)
		fixed = false;

	 
	if (!IS_ALIGNED(size >> PAGE_SHIFT, 1 << order_per_bit))
		return -EINVAL;

	 
	if (fixed && base < highmem_start && base + size > highmem_start) {
		ret = -EINVAL;
		pr_err("Region at %pa defined on low/high memory boundary (%pa)\n",
			&base, &highmem_start);
		goto err;
	}

	 
	if (limit == 0 || limit > memblock_end)
		limit = memblock_end;

	if (base + size > limit) {
		ret = -EINVAL;
		pr_err("Size (%pa) of region at %pa exceeds limit (%pa)\n",
			&size, &base, &limit);
		goto err;
	}

	 
	if (fixed) {
		if (memblock_is_region_reserved(base, size) ||
		    memblock_reserve(base, size) < 0) {
			ret = -EBUSY;
			goto err;
		}
	} else {
		phys_addr_t addr = 0;

		 
#ifdef CONFIG_PHYS_ADDR_T_64BIT
		if (!memblock_bottom_up() && memblock_end >= SZ_4G + size) {
			memblock_set_bottom_up(true);
			addr = memblock_alloc_range_nid(size, alignment, SZ_4G,
							limit, nid, true);
			memblock_set_bottom_up(false);
		}
#endif

		 
		if (!addr && base < highmem_start && limit > highmem_start) {
			addr = memblock_alloc_range_nid(size, alignment,
					highmem_start, limit, nid, true);
			limit = highmem_start;
		}

		if (!addr) {
			addr = memblock_alloc_range_nid(size, alignment, base,
					limit, nid, true);
			if (!addr) {
				ret = -ENOMEM;
				goto err;
			}
		}

		 
		kmemleak_ignore_phys(addr);
		base = addr;
	}

	ret = cma_init_reserved_mem(base, size, order_per_bit, name, res_cma);
	if (ret)
		goto free_mem;

	pr_info("Reserved %ld MiB at %pa on node %d\n", (unsigned long)size / SZ_1M,
		&base, nid);
	return 0;

free_mem:
	memblock_phys_free(base, size);
err:
	pr_err("Failed to reserve %ld MiB on node %d\n", (unsigned long)size / SZ_1M,
	       nid);
	return ret;
}

#ifdef CONFIG_CMA_DEBUG
static void cma_debug_show_areas(struct cma *cma)
{
	unsigned long next_zero_bit, next_set_bit, nr_zero;
	unsigned long start = 0;
	unsigned long nr_part, nr_total = 0;
	unsigned long nbits = cma_bitmap_maxno(cma);

	spin_lock_irq(&cma->lock);
	pr_info("number of available pages: ");
	for (;;) {
		next_zero_bit = find_next_zero_bit(cma->bitmap, nbits, start);
		if (next_zero_bit >= nbits)
			break;
		next_set_bit = find_next_bit(cma->bitmap, nbits, next_zero_bit);
		nr_zero = next_set_bit - next_zero_bit;
		nr_part = nr_zero << cma->order_per_bit;
		pr_cont("%s%lu@%lu", nr_total ? "+" : "", nr_part,
			next_zero_bit);
		nr_total += nr_part;
		start = next_zero_bit + nr_zero;
	}
	pr_cont("=> %lu free of %lu total pages\n", nr_total, cma->count);
	spin_unlock_irq(&cma->lock);
}
#else
static inline void cma_debug_show_areas(struct cma *cma) { }
#endif

 
struct page *cma_alloc(struct cma *cma, unsigned long count,
		       unsigned int align, bool no_warn)
{
	unsigned long mask, offset;
	unsigned long pfn = -1;
	unsigned long start = 0;
	unsigned long bitmap_maxno, bitmap_no, bitmap_count;
	unsigned long i;
	struct page *page = NULL;
	int ret = -ENOMEM;

	if (!cma || !cma->count || !cma->bitmap)
		goto out;

	pr_debug("%s(cma %p, name: %s, count %lu, align %d)\n", __func__,
		(void *)cma, cma->name, count, align);

	if (!count)
		goto out;

	trace_cma_alloc_start(cma->name, count, align);

	mask = cma_bitmap_aligned_mask(cma, align);
	offset = cma_bitmap_aligned_offset(cma, align);
	bitmap_maxno = cma_bitmap_maxno(cma);
	bitmap_count = cma_bitmap_pages_to_bits(cma, count);

	if (bitmap_count > bitmap_maxno)
		goto out;

	for (;;) {
		spin_lock_irq(&cma->lock);
		bitmap_no = bitmap_find_next_zero_area_off(cma->bitmap,
				bitmap_maxno, start, bitmap_count, mask,
				offset);
		if (bitmap_no >= bitmap_maxno) {
			spin_unlock_irq(&cma->lock);
			break;
		}
		bitmap_set(cma->bitmap, bitmap_no, bitmap_count);
		 
		spin_unlock_irq(&cma->lock);

		pfn = cma->base_pfn + (bitmap_no << cma->order_per_bit);
		mutex_lock(&cma_mutex);
		ret = alloc_contig_range(pfn, pfn + count, MIGRATE_CMA,
				     GFP_KERNEL | (no_warn ? __GFP_NOWARN : 0));
		mutex_unlock(&cma_mutex);
		if (ret == 0) {
			page = pfn_to_page(pfn);
			break;
		}

		cma_clear_bitmap(cma, pfn, count);
		if (ret != -EBUSY)
			break;

		pr_debug("%s(): memory range at pfn 0x%lx %p is busy, retrying\n",
			 __func__, pfn, pfn_to_page(pfn));

		trace_cma_alloc_busy_retry(cma->name, pfn, pfn_to_page(pfn),
					   count, align);
		 
		start = bitmap_no + mask + 1;
	}

	trace_cma_alloc_finish(cma->name, pfn, page, count, align, ret);

	 
	if (page) {
		for (i = 0; i < count; i++)
			page_kasan_tag_reset(nth_page(page, i));
	}

	if (ret && !no_warn) {
		pr_err_ratelimited("%s: %s: alloc failed, req-size: %lu pages, ret: %d\n",
				   __func__, cma->name, count, ret);
		cma_debug_show_areas(cma);
	}

	pr_debug("%s(): returned %p\n", __func__, page);
out:
	if (page) {
		count_vm_event(CMA_ALLOC_SUCCESS);
		cma_sysfs_account_success_pages(cma, count);
	} else {
		count_vm_event(CMA_ALLOC_FAIL);
		if (cma)
			cma_sysfs_account_fail_pages(cma, count);
	}

	return page;
}

bool cma_pages_valid(struct cma *cma, const struct page *pages,
		     unsigned long count)
{
	unsigned long pfn;

	if (!cma || !pages)
		return false;

	pfn = page_to_pfn(pages);

	if (pfn < cma->base_pfn || pfn >= cma->base_pfn + cma->count) {
		pr_debug("%s(page %p, count %lu)\n", __func__,
						(void *)pages, count);
		return false;
	}

	return true;
}

 
bool cma_release(struct cma *cma, const struct page *pages,
		 unsigned long count)
{
	unsigned long pfn;

	if (!cma_pages_valid(cma, pages, count))
		return false;

	pr_debug("%s(page %p, count %lu)\n", __func__, (void *)pages, count);

	pfn = page_to_pfn(pages);

	VM_BUG_ON(pfn + count > cma->base_pfn + cma->count);

	free_contig_range(pfn, count);
	cma_clear_bitmap(cma, pfn, count);
	trace_cma_release(cma->name, pfn, pages, count);

	return true;
}

int cma_for_each_area(int (*it)(struct cma *cma, void *data), void *data)
{
	int i;

	for (i = 0; i < cma_area_count; i++) {
		int ret = it(&cma_areas[i], data);

		if (ret)
			return ret;
	}

	return 0;
}
