
 
#include <linux/libnvdimm.h>
#include <linux/badblocks.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/ndctl.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/io.h>
#include "nd-core.h"
#include "nd.h"

void badrange_init(struct badrange *badrange)
{
	INIT_LIST_HEAD(&badrange->list);
	spin_lock_init(&badrange->lock);
}
EXPORT_SYMBOL_GPL(badrange_init);

static void append_badrange_entry(struct badrange *badrange,
		struct badrange_entry *bre, u64 addr, u64 length)
{
	lockdep_assert_held(&badrange->lock);
	bre->start = addr;
	bre->length = length;
	list_add_tail(&bre->list, &badrange->list);
}

static int alloc_and_append_badrange_entry(struct badrange *badrange,
		u64 addr, u64 length, gfp_t flags)
{
	struct badrange_entry *bre;

	bre = kzalloc(sizeof(*bre), flags);
	if (!bre)
		return -ENOMEM;

	append_badrange_entry(badrange, bre, addr, length);
	return 0;
}

static int add_badrange(struct badrange *badrange, u64 addr, u64 length)
{
	struct badrange_entry *bre, *bre_new;

	spin_unlock(&badrange->lock);
	bre_new = kzalloc(sizeof(*bre_new), GFP_KERNEL);
	spin_lock(&badrange->lock);

	if (list_empty(&badrange->list)) {
		if (!bre_new)
			return -ENOMEM;
		append_badrange_entry(badrange, bre_new, addr, length);
		return 0;
	}

	 
	list_for_each_entry(bre, &badrange->list, list)
		if (bre->start == addr) {
			 
			if (bre->length != length)
				bre->length = length;
			kfree(bre_new);
			return 0;
		}

	 
	if (!bre_new)
		return -ENOMEM;
	append_badrange_entry(badrange, bre_new, addr, length);

	return 0;
}

int badrange_add(struct badrange *badrange, u64 addr, u64 length)
{
	int rc;

	spin_lock(&badrange->lock);
	rc = add_badrange(badrange, addr, length);
	spin_unlock(&badrange->lock);

	return rc;
}
EXPORT_SYMBOL_GPL(badrange_add);

void badrange_forget(struct badrange *badrange, phys_addr_t start,
		unsigned int len)
{
	struct list_head *badrange_list = &badrange->list;
	u64 clr_end = start + len - 1;
	struct badrange_entry *bre, *next;

	spin_lock(&badrange->lock);

	 

	list_for_each_entry_safe(bre, next, badrange_list, list) {
		u64 bre_end = bre->start + bre->length - 1;

		 
		if (bre_end < start)
			continue;
		if (bre->start >  clr_end)
			continue;
		 
		if ((bre->start >= start) && (bre_end <= clr_end)) {
			list_del(&bre->list);
			kfree(bre);
			continue;
		}
		 
		if ((start <= bre->start) && (clr_end > bre->start)) {
			bre->length -= clr_end - bre->start + 1;
			bre->start = clr_end + 1;
			continue;
		}
		 
		if ((bre->start < start) && (bre_end <= clr_end)) {
			 
			bre->length = start - bre->start;
			continue;
		}
		 
		if ((bre->start < start) && (bre_end > clr_end)) {
			u64 new_start = clr_end + 1;
			u64 new_len = bre_end - new_start + 1;

			 
			alloc_and_append_badrange_entry(badrange, new_start,
					new_len, GFP_NOWAIT);
			 
			bre->length = start - bre->start;
			continue;
		}
	}
	spin_unlock(&badrange->lock);
}
EXPORT_SYMBOL_GPL(badrange_forget);

static void set_badblock(struct badblocks *bb, sector_t s, int num)
{
	dev_dbg(bb->dev, "Found a bad range (0x%llx, 0x%llx)\n",
			(u64) s * 512, (u64) num * 512);
	 
	if (badblocks_set(bb, s, num, 1))
		dev_info_once(bb->dev, "%s: failed for sector %llx\n",
				__func__, (u64) s);
}

 
static void __add_badblock_range(struct badblocks *bb, u64 ns_offset, u64 len)
{
	const unsigned int sector_size = 512;
	sector_t start_sector, end_sector;
	u64 num_sectors;
	u32 rem;

	start_sector = div_u64(ns_offset, sector_size);
	end_sector = div_u64_rem(ns_offset + len, sector_size, &rem);
	if (rem)
		end_sector++;
	num_sectors = end_sector - start_sector;

	if (unlikely(num_sectors > (u64)INT_MAX)) {
		u64 remaining = num_sectors;
		sector_t s = start_sector;

		while (remaining) {
			int done = min_t(u64, remaining, INT_MAX);

			set_badblock(bb, s, done);
			remaining -= done;
			s += done;
		}
	} else
		set_badblock(bb, start_sector, num_sectors);
}

static void badblocks_populate(struct badrange *badrange,
		struct badblocks *bb, const struct range *range)
{
	struct badrange_entry *bre;

	if (list_empty(&badrange->list))
		return;

	list_for_each_entry(bre, &badrange->list, list) {
		u64 bre_end = bre->start + bre->length - 1;

		 
		if (bre_end < range->start)
			continue;
		if (bre->start > range->end)
			continue;
		 
		if (bre->start >= range->start) {
			u64 start = bre->start;
			u64 len;

			if (bre_end <= range->end)
				len = bre->length;
			else
				len = range->start + range_len(range)
					- bre->start;
			__add_badblock_range(bb, start - range->start, len);
			continue;
		}
		 
		if (bre->start < range->start) {
			u64 len;

			if (bre_end < range->end)
				len = bre->start + bre->length - range->start;
			else
				len = range_len(range);
			__add_badblock_range(bb, 0, len);
		}
	}
}

 
void nvdimm_badblocks_populate(struct nd_region *nd_region,
		struct badblocks *bb, const struct range *range)
{
	struct nvdimm_bus *nvdimm_bus;

	if (!is_memory(&nd_region->dev)) {
		dev_WARN_ONCE(&nd_region->dev, 1,
				"%s only valid for pmem regions\n", __func__);
		return;
	}
	nvdimm_bus = walk_to_nvdimm_bus(&nd_region->dev);

	nvdimm_bus_lock(&nvdimm_bus->dev);
	badblocks_populate(&nvdimm_bus->badrange, bb, range);
	nvdimm_bus_unlock(&nvdimm_bus->dev);
}
EXPORT_SYMBOL_GPL(nvdimm_badblocks_populate);
