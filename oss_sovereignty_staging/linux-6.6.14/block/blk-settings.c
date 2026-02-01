
 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/pagemap.h>
#include <linux/backing-dev-defs.h>
#include <linux/gcd.h>
#include <linux/lcm.h>
#include <linux/jiffies.h>
#include <linux/gfp.h>
#include <linux/dma-mapping.h>

#include "blk.h"
#include "blk-rq-qos.h"
#include "blk-wbt.h"

void blk_queue_rq_timeout(struct request_queue *q, unsigned int timeout)
{
	q->rq_timeout = timeout;
}
EXPORT_SYMBOL_GPL(blk_queue_rq_timeout);

 
void blk_set_default_limits(struct queue_limits *lim)
{
	lim->max_segments = BLK_MAX_SEGMENTS;
	lim->max_discard_segments = 1;
	lim->max_integrity_segments = 0;
	lim->seg_boundary_mask = BLK_SEG_BOUNDARY_MASK;
	lim->virt_boundary_mask = 0;
	lim->max_segment_size = BLK_MAX_SEGMENT_SIZE;
	lim->max_sectors = lim->max_hw_sectors = BLK_SAFE_MAX_SECTORS;
	lim->max_user_sectors = lim->max_dev_sectors = 0;
	lim->chunk_sectors = 0;
	lim->max_write_zeroes_sectors = 0;
	lim->max_zone_append_sectors = 0;
	lim->max_discard_sectors = 0;
	lim->max_hw_discard_sectors = 0;
	lim->max_secure_erase_sectors = 0;
	lim->discard_granularity = 0;
	lim->discard_alignment = 0;
	lim->discard_misaligned = 0;
	lim->logical_block_size = lim->physical_block_size = lim->io_min = 512;
	lim->bounce = BLK_BOUNCE_NONE;
	lim->alignment_offset = 0;
	lim->io_opt = 0;
	lim->misaligned = 0;
	lim->zoned = BLK_ZONED_NONE;
	lim->zone_write_granularity = 0;
	lim->dma_alignment = 511;
}

 
void blk_set_stacking_limits(struct queue_limits *lim)
{
	blk_set_default_limits(lim);

	 
	lim->max_segments = USHRT_MAX;
	lim->max_discard_segments = USHRT_MAX;
	lim->max_hw_sectors = UINT_MAX;
	lim->max_segment_size = UINT_MAX;
	lim->max_sectors = UINT_MAX;
	lim->max_dev_sectors = UINT_MAX;
	lim->max_write_zeroes_sectors = UINT_MAX;
	lim->max_zone_append_sectors = UINT_MAX;
}
EXPORT_SYMBOL(blk_set_stacking_limits);

 
void blk_queue_bounce_limit(struct request_queue *q, enum blk_bounce bounce)
{
	q->limits.bounce = bounce;
}
EXPORT_SYMBOL(blk_queue_bounce_limit);

 
void blk_queue_max_hw_sectors(struct request_queue *q, unsigned int max_hw_sectors)
{
	struct queue_limits *limits = &q->limits;
	unsigned int max_sectors;

	if ((max_hw_sectors << 9) < PAGE_SIZE) {
		max_hw_sectors = 1 << (PAGE_SHIFT - 9);
		printk(KERN_INFO "%s: set to minimum %d\n",
		       __func__, max_hw_sectors);
	}

	max_hw_sectors = round_down(max_hw_sectors,
				    limits->logical_block_size >> SECTOR_SHIFT);
	limits->max_hw_sectors = max_hw_sectors;

	max_sectors = min_not_zero(max_hw_sectors, limits->max_dev_sectors);

	if (limits->max_user_sectors)
		max_sectors = min(max_sectors, limits->max_user_sectors);
	else
		max_sectors = min(max_sectors, BLK_DEF_MAX_SECTORS);

	max_sectors = round_down(max_sectors,
				 limits->logical_block_size >> SECTOR_SHIFT);
	limits->max_sectors = max_sectors;

	if (!q->disk)
		return;
	q->disk->bdi->io_pages = max_sectors >> (PAGE_SHIFT - 9);
}
EXPORT_SYMBOL(blk_queue_max_hw_sectors);

 
void blk_queue_chunk_sectors(struct request_queue *q, unsigned int chunk_sectors)
{
	q->limits.chunk_sectors = chunk_sectors;
}
EXPORT_SYMBOL(blk_queue_chunk_sectors);

 
void blk_queue_max_discard_sectors(struct request_queue *q,
		unsigned int max_discard_sectors)
{
	q->limits.max_hw_discard_sectors = max_discard_sectors;
	q->limits.max_discard_sectors = max_discard_sectors;
}
EXPORT_SYMBOL(blk_queue_max_discard_sectors);

 
void blk_queue_max_secure_erase_sectors(struct request_queue *q,
		unsigned int max_sectors)
{
	q->limits.max_secure_erase_sectors = max_sectors;
}
EXPORT_SYMBOL(blk_queue_max_secure_erase_sectors);

 
void blk_queue_max_write_zeroes_sectors(struct request_queue *q,
		unsigned int max_write_zeroes_sectors)
{
	q->limits.max_write_zeroes_sectors = max_write_zeroes_sectors;
}
EXPORT_SYMBOL(blk_queue_max_write_zeroes_sectors);

 
void blk_queue_max_zone_append_sectors(struct request_queue *q,
		unsigned int max_zone_append_sectors)
{
	unsigned int max_sectors;

	if (WARN_ON(!blk_queue_is_zoned(q)))
		return;

	max_sectors = min(q->limits.max_hw_sectors, max_zone_append_sectors);
	max_sectors = min(q->limits.chunk_sectors, max_sectors);

	 
	WARN_ON(!max_sectors);

	q->limits.max_zone_append_sectors = max_sectors;
}
EXPORT_SYMBOL_GPL(blk_queue_max_zone_append_sectors);

 
void blk_queue_max_segments(struct request_queue *q, unsigned short max_segments)
{
	if (!max_segments) {
		max_segments = 1;
		printk(KERN_INFO "%s: set to minimum %d\n",
		       __func__, max_segments);
	}

	q->limits.max_segments = max_segments;
}
EXPORT_SYMBOL(blk_queue_max_segments);

 
void blk_queue_max_discard_segments(struct request_queue *q,
		unsigned short max_segments)
{
	q->limits.max_discard_segments = max_segments;
}
EXPORT_SYMBOL_GPL(blk_queue_max_discard_segments);

 
void blk_queue_max_segment_size(struct request_queue *q, unsigned int max_size)
{
	if (max_size < PAGE_SIZE) {
		max_size = PAGE_SIZE;
		printk(KERN_INFO "%s: set to minimum %d\n",
		       __func__, max_size);
	}

	 
	WARN_ON_ONCE(q->limits.virt_boundary_mask);

	q->limits.max_segment_size = max_size;
}
EXPORT_SYMBOL(blk_queue_max_segment_size);

 
void blk_queue_logical_block_size(struct request_queue *q, unsigned int size)
{
	struct queue_limits *limits = &q->limits;

	limits->logical_block_size = size;

	if (limits->physical_block_size < size)
		limits->physical_block_size = size;

	if (limits->io_min < limits->physical_block_size)
		limits->io_min = limits->physical_block_size;

	limits->max_hw_sectors =
		round_down(limits->max_hw_sectors, size >> SECTOR_SHIFT);
	limits->max_sectors =
		round_down(limits->max_sectors, size >> SECTOR_SHIFT);
}
EXPORT_SYMBOL(blk_queue_logical_block_size);

 
void blk_queue_physical_block_size(struct request_queue *q, unsigned int size)
{
	q->limits.physical_block_size = size;

	if (q->limits.physical_block_size < q->limits.logical_block_size)
		q->limits.physical_block_size = q->limits.logical_block_size;

	if (q->limits.io_min < q->limits.physical_block_size)
		q->limits.io_min = q->limits.physical_block_size;
}
EXPORT_SYMBOL(blk_queue_physical_block_size);

 
void blk_queue_zone_write_granularity(struct request_queue *q,
				      unsigned int size)
{
	if (WARN_ON_ONCE(!blk_queue_is_zoned(q)))
		return;

	q->limits.zone_write_granularity = size;

	if (q->limits.zone_write_granularity < q->limits.logical_block_size)
		q->limits.zone_write_granularity = q->limits.logical_block_size;
}
EXPORT_SYMBOL_GPL(blk_queue_zone_write_granularity);

 
void blk_queue_alignment_offset(struct request_queue *q, unsigned int offset)
{
	q->limits.alignment_offset =
		offset & (q->limits.physical_block_size - 1);
	q->limits.misaligned = 0;
}
EXPORT_SYMBOL(blk_queue_alignment_offset);

void disk_update_readahead(struct gendisk *disk)
{
	struct request_queue *q = disk->queue;

	 
	disk->bdi->ra_pages =
		max(queue_io_opt(q) * 2 / PAGE_SIZE, VM_READAHEAD_PAGES);
	disk->bdi->io_pages = queue_max_sectors(q) >> (PAGE_SHIFT - 9);
}
EXPORT_SYMBOL_GPL(disk_update_readahead);

 
void blk_limits_io_min(struct queue_limits *limits, unsigned int min)
{
	limits->io_min = min;

	if (limits->io_min < limits->logical_block_size)
		limits->io_min = limits->logical_block_size;

	if (limits->io_min < limits->physical_block_size)
		limits->io_min = limits->physical_block_size;
}
EXPORT_SYMBOL(blk_limits_io_min);

 
void blk_queue_io_min(struct request_queue *q, unsigned int min)
{
	blk_limits_io_min(&q->limits, min);
}
EXPORT_SYMBOL(blk_queue_io_min);

 
void blk_limits_io_opt(struct queue_limits *limits, unsigned int opt)
{
	limits->io_opt = opt;
}
EXPORT_SYMBOL(blk_limits_io_opt);

 
void blk_queue_io_opt(struct request_queue *q, unsigned int opt)
{
	blk_limits_io_opt(&q->limits, opt);
	if (!q->disk)
		return;
	q->disk->bdi->ra_pages =
		max(queue_io_opt(q) * 2 / PAGE_SIZE, VM_READAHEAD_PAGES);
}
EXPORT_SYMBOL(blk_queue_io_opt);

static int queue_limit_alignment_offset(const struct queue_limits *lim,
		sector_t sector)
{
	unsigned int granularity = max(lim->physical_block_size, lim->io_min);
	unsigned int alignment = sector_div(sector, granularity >> SECTOR_SHIFT)
		<< SECTOR_SHIFT;

	return (granularity + lim->alignment_offset - alignment) % granularity;
}

static unsigned int queue_limit_discard_alignment(
		const struct queue_limits *lim, sector_t sector)
{
	unsigned int alignment, granularity, offset;

	if (!lim->max_discard_sectors)
		return 0;

	 
	alignment = lim->discard_alignment >> SECTOR_SHIFT;
	granularity = lim->discard_granularity >> SECTOR_SHIFT;
	if (!granularity)
		return 0;

	 
	offset = sector_div(sector, granularity);

	 
	offset = (granularity + alignment - offset) % granularity;

	 
	return offset << SECTOR_SHIFT;
}

static unsigned int blk_round_down_sectors(unsigned int sectors, unsigned int lbs)
{
	sectors = round_down(sectors, lbs >> SECTOR_SHIFT);
	if (sectors < PAGE_SIZE >> SECTOR_SHIFT)
		sectors = PAGE_SIZE >> SECTOR_SHIFT;
	return sectors;
}

 
int blk_stack_limits(struct queue_limits *t, struct queue_limits *b,
		     sector_t start)
{
	unsigned int top, bottom, alignment, ret = 0;

	t->max_sectors = min_not_zero(t->max_sectors, b->max_sectors);
	t->max_hw_sectors = min_not_zero(t->max_hw_sectors, b->max_hw_sectors);
	t->max_dev_sectors = min_not_zero(t->max_dev_sectors, b->max_dev_sectors);
	t->max_write_zeroes_sectors = min(t->max_write_zeroes_sectors,
					b->max_write_zeroes_sectors);
	t->max_zone_append_sectors = min(t->max_zone_append_sectors,
					b->max_zone_append_sectors);
	t->bounce = max(t->bounce, b->bounce);

	t->seg_boundary_mask = min_not_zero(t->seg_boundary_mask,
					    b->seg_boundary_mask);
	t->virt_boundary_mask = min_not_zero(t->virt_boundary_mask,
					    b->virt_boundary_mask);

	t->max_segments = min_not_zero(t->max_segments, b->max_segments);
	t->max_discard_segments = min_not_zero(t->max_discard_segments,
					       b->max_discard_segments);
	t->max_integrity_segments = min_not_zero(t->max_integrity_segments,
						 b->max_integrity_segments);

	t->max_segment_size = min_not_zero(t->max_segment_size,
					   b->max_segment_size);

	t->misaligned |= b->misaligned;

	alignment = queue_limit_alignment_offset(b, start);

	 
	if (t->alignment_offset != alignment) {

		top = max(t->physical_block_size, t->io_min)
			+ t->alignment_offset;
		bottom = max(b->physical_block_size, b->io_min) + alignment;

		 
		if (max(top, bottom) % min(top, bottom)) {
			t->misaligned = 1;
			ret = -1;
		}
	}

	t->logical_block_size = max(t->logical_block_size,
				    b->logical_block_size);

	t->physical_block_size = max(t->physical_block_size,
				     b->physical_block_size);

	t->io_min = max(t->io_min, b->io_min);
	t->io_opt = lcm_not_zero(t->io_opt, b->io_opt);
	t->dma_alignment = max(t->dma_alignment, b->dma_alignment);

	 
	if (b->chunk_sectors)
		t->chunk_sectors = gcd(t->chunk_sectors, b->chunk_sectors);

	 
	if (t->physical_block_size & (t->logical_block_size - 1)) {
		t->physical_block_size = t->logical_block_size;
		t->misaligned = 1;
		ret = -1;
	}

	 
	if (t->io_min & (t->physical_block_size - 1)) {
		t->io_min = t->physical_block_size;
		t->misaligned = 1;
		ret = -1;
	}

	 
	if (t->io_opt & (t->physical_block_size - 1)) {
		t->io_opt = 0;
		t->misaligned = 1;
		ret = -1;
	}

	 
	if ((t->chunk_sectors << 9) & (t->physical_block_size - 1)) {
		t->chunk_sectors = 0;
		t->misaligned = 1;
		ret = -1;
	}

	t->raid_partial_stripes_expensive =
		max(t->raid_partial_stripes_expensive,
		    b->raid_partial_stripes_expensive);

	 
	t->alignment_offset = lcm_not_zero(t->alignment_offset, alignment)
		% max(t->physical_block_size, t->io_min);

	 
	if (t->alignment_offset & (t->logical_block_size - 1)) {
		t->misaligned = 1;
		ret = -1;
	}

	t->max_sectors = blk_round_down_sectors(t->max_sectors, t->logical_block_size);
	t->max_hw_sectors = blk_round_down_sectors(t->max_hw_sectors, t->logical_block_size);
	t->max_dev_sectors = blk_round_down_sectors(t->max_dev_sectors, t->logical_block_size);

	 
	if (b->discard_granularity) {
		alignment = queue_limit_discard_alignment(b, start);

		if (t->discard_granularity != 0 &&
		    t->discard_alignment != alignment) {
			top = t->discard_granularity + t->discard_alignment;
			bottom = b->discard_granularity + alignment;

			 
			if ((max(top, bottom) % min(top, bottom)) != 0)
				t->discard_misaligned = 1;
		}

		t->max_discard_sectors = min_not_zero(t->max_discard_sectors,
						      b->max_discard_sectors);
		t->max_hw_discard_sectors = min_not_zero(t->max_hw_discard_sectors,
							 b->max_hw_discard_sectors);
		t->discard_granularity = max(t->discard_granularity,
					     b->discard_granularity);
		t->discard_alignment = lcm_not_zero(t->discard_alignment, alignment) %
			t->discard_granularity;
	}
	t->max_secure_erase_sectors = min_not_zero(t->max_secure_erase_sectors,
						   b->max_secure_erase_sectors);
	t->zone_write_granularity = max(t->zone_write_granularity,
					b->zone_write_granularity);
	t->zoned = max(t->zoned, b->zoned);
	return ret;
}
EXPORT_SYMBOL(blk_stack_limits);

 
void disk_stack_limits(struct gendisk *disk, struct block_device *bdev,
		       sector_t offset)
{
	struct request_queue *t = disk->queue;

	if (blk_stack_limits(&t->limits, &bdev_get_queue(bdev)->limits,
			get_start_sect(bdev) + (offset >> 9)) < 0)
		pr_notice("%s: Warning: Device %pg is misaligned\n",
			disk->disk_name, bdev);

	disk_update_readahead(disk);
}
EXPORT_SYMBOL(disk_stack_limits);

 
void blk_queue_update_dma_pad(struct request_queue *q, unsigned int mask)
{
	if (mask > q->dma_pad_mask)
		q->dma_pad_mask = mask;
}
EXPORT_SYMBOL(blk_queue_update_dma_pad);

 
void blk_queue_segment_boundary(struct request_queue *q, unsigned long mask)
{
	if (mask < PAGE_SIZE - 1) {
		mask = PAGE_SIZE - 1;
		printk(KERN_INFO "%s: set to minimum %lx\n",
		       __func__, mask);
	}

	q->limits.seg_boundary_mask = mask;
}
EXPORT_SYMBOL(blk_queue_segment_boundary);

 
void blk_queue_virt_boundary(struct request_queue *q, unsigned long mask)
{
	q->limits.virt_boundary_mask = mask;

	 
	if (mask)
		q->limits.max_segment_size = UINT_MAX;
}
EXPORT_SYMBOL(blk_queue_virt_boundary);

 
void blk_queue_dma_alignment(struct request_queue *q, int mask)
{
	q->limits.dma_alignment = mask;
}
EXPORT_SYMBOL(blk_queue_dma_alignment);

 
void blk_queue_update_dma_alignment(struct request_queue *q, int mask)
{
	BUG_ON(mask > PAGE_SIZE);

	if (mask > q->limits.dma_alignment)
		q->limits.dma_alignment = mask;
}
EXPORT_SYMBOL(blk_queue_update_dma_alignment);

 
void blk_set_queue_depth(struct request_queue *q, unsigned int depth)
{
	q->queue_depth = depth;
	rq_qos_queue_depth_changed(q);
}
EXPORT_SYMBOL(blk_set_queue_depth);

 
void blk_queue_write_cache(struct request_queue *q, bool wc, bool fua)
{
	if (wc) {
		blk_queue_flag_set(QUEUE_FLAG_HW_WC, q);
		blk_queue_flag_set(QUEUE_FLAG_WC, q);
	} else {
		blk_queue_flag_clear(QUEUE_FLAG_HW_WC, q);
		blk_queue_flag_clear(QUEUE_FLAG_WC, q);
	}
	if (fua)
		blk_queue_flag_set(QUEUE_FLAG_FUA, q);
	else
		blk_queue_flag_clear(QUEUE_FLAG_FUA, q);

	wbt_set_write_cache(q, test_bit(QUEUE_FLAG_WC, &q->queue_flags));
}
EXPORT_SYMBOL_GPL(blk_queue_write_cache);

 
void blk_queue_required_elevator_features(struct request_queue *q,
					  unsigned int features)
{
	q->required_elevator_features = features;
}
EXPORT_SYMBOL_GPL(blk_queue_required_elevator_features);

 
bool blk_queue_can_use_dma_map_merging(struct request_queue *q,
				       struct device *dev)
{
	unsigned long boundary = dma_get_merge_boundary(dev);

	if (!boundary)
		return false;

	 
	blk_queue_virt_boundary(q, boundary);

	return true;
}
EXPORT_SYMBOL_GPL(blk_queue_can_use_dma_map_merging);

static bool disk_has_partitions(struct gendisk *disk)
{
	unsigned long idx;
	struct block_device *part;
	bool ret = false;

	rcu_read_lock();
	xa_for_each(&disk->part_tbl, idx, part) {
		if (bdev_is_partition(part)) {
			ret = true;
			break;
		}
	}
	rcu_read_unlock();

	return ret;
}

 
void disk_set_zoned(struct gendisk *disk, enum blk_zoned_model model)
{
	struct request_queue *q = disk->queue;
	unsigned int old_model = q->limits.zoned;

	switch (model) {
	case BLK_ZONED_HM:
		 
		WARN_ON_ONCE(!IS_ENABLED(CONFIG_BLK_DEV_ZONED));
		break;
	case BLK_ZONED_HA:
		 
		if (!IS_ENABLED(CONFIG_BLK_DEV_ZONED) ||
		    disk_has_partitions(disk))
			model = BLK_ZONED_NONE;
		break;
	case BLK_ZONED_NONE:
	default:
		if (WARN_ON_ONCE(model != BLK_ZONED_NONE))
			model = BLK_ZONED_NONE;
		break;
	}

	q->limits.zoned = model;
	if (model != BLK_ZONED_NONE) {
		 
		blk_queue_zone_write_granularity(q,
						queue_logical_block_size(q));
	} else if (old_model != BLK_ZONED_NONE) {
		disk_clear_zone_settings(disk);
	}
}
EXPORT_SYMBOL_GPL(disk_set_zoned);

int bdev_alignment_offset(struct block_device *bdev)
{
	struct request_queue *q = bdev_get_queue(bdev);

	if (q->limits.misaligned)
		return -1;
	if (bdev_is_partition(bdev))
		return queue_limit_alignment_offset(&q->limits,
				bdev->bd_start_sect);
	return q->limits.alignment_offset;
}
EXPORT_SYMBOL_GPL(bdev_alignment_offset);

unsigned int bdev_discard_alignment(struct block_device *bdev)
{
	struct request_queue *q = bdev_get_queue(bdev);

	if (bdev_is_partition(bdev))
		return queue_limit_discard_alignment(&q->limits,
				bdev->bd_start_sect);
	return q->limits.discard_alignment;
}
EXPORT_SYMBOL_GPL(bdev_discard_alignment);
