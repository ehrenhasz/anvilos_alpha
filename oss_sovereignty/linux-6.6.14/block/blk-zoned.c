
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rbtree.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/sched/mm.h>

#include "blk.h"

#define ZONE_COND_NAME(name) [BLK_ZONE_COND_##name] = #name
static const char *const zone_cond_name[] = {
	ZONE_COND_NAME(NOT_WP),
	ZONE_COND_NAME(EMPTY),
	ZONE_COND_NAME(IMP_OPEN),
	ZONE_COND_NAME(EXP_OPEN),
	ZONE_COND_NAME(CLOSED),
	ZONE_COND_NAME(READONLY),
	ZONE_COND_NAME(FULL),
	ZONE_COND_NAME(OFFLINE),
};
#undef ZONE_COND_NAME

 
const char *blk_zone_cond_str(enum blk_zone_cond zone_cond)
{
	static const char *zone_cond_str = "UNKNOWN";

	if (zone_cond < ARRAY_SIZE(zone_cond_name) && zone_cond_name[zone_cond])
		zone_cond_str = zone_cond_name[zone_cond];

	return zone_cond_str;
}
EXPORT_SYMBOL_GPL(blk_zone_cond_str);

 
bool blk_req_needs_zone_write_lock(struct request *rq)
{
	if (!rq->q->disk->seq_zones_wlock)
		return false;

	return blk_rq_is_seq_zoned_write(rq);
}
EXPORT_SYMBOL_GPL(blk_req_needs_zone_write_lock);

bool blk_req_zone_write_trylock(struct request *rq)
{
	unsigned int zno = blk_rq_zone_no(rq);

	if (test_and_set_bit(zno, rq->q->disk->seq_zones_wlock))
		return false;

	WARN_ON_ONCE(rq->rq_flags & RQF_ZONE_WRITE_LOCKED);
	rq->rq_flags |= RQF_ZONE_WRITE_LOCKED;

	return true;
}
EXPORT_SYMBOL_GPL(blk_req_zone_write_trylock);

void __blk_req_zone_write_lock(struct request *rq)
{
	if (WARN_ON_ONCE(test_and_set_bit(blk_rq_zone_no(rq),
					  rq->q->disk->seq_zones_wlock)))
		return;

	WARN_ON_ONCE(rq->rq_flags & RQF_ZONE_WRITE_LOCKED);
	rq->rq_flags |= RQF_ZONE_WRITE_LOCKED;
}
EXPORT_SYMBOL_GPL(__blk_req_zone_write_lock);

void __blk_req_zone_write_unlock(struct request *rq)
{
	rq->rq_flags &= ~RQF_ZONE_WRITE_LOCKED;
	if (rq->q->disk->seq_zones_wlock)
		WARN_ON_ONCE(!test_and_clear_bit(blk_rq_zone_no(rq),
						 rq->q->disk->seq_zones_wlock));
}
EXPORT_SYMBOL_GPL(__blk_req_zone_write_unlock);

 
unsigned int bdev_nr_zones(struct block_device *bdev)
{
	sector_t zone_sectors = bdev_zone_sectors(bdev);

	if (!bdev_is_zoned(bdev))
		return 0;
	return (bdev_nr_sectors(bdev) + zone_sectors - 1) >>
		ilog2(zone_sectors);
}
EXPORT_SYMBOL_GPL(bdev_nr_zones);

 
int blkdev_report_zones(struct block_device *bdev, sector_t sector,
			unsigned int nr_zones, report_zones_cb cb, void *data)
{
	struct gendisk *disk = bdev->bd_disk;
	sector_t capacity = get_capacity(disk);

	if (!bdev_is_zoned(bdev) || WARN_ON_ONCE(!disk->fops->report_zones))
		return -EOPNOTSUPP;

	if (!nr_zones || sector >= capacity)
		return 0;

	return disk->fops->report_zones(disk, sector, nr_zones, cb, data);
}
EXPORT_SYMBOL_GPL(blkdev_report_zones);

static inline unsigned long *blk_alloc_zone_bitmap(int node,
						   unsigned int nr_zones)
{
	return kcalloc_node(BITS_TO_LONGS(nr_zones), sizeof(unsigned long),
			    GFP_NOIO, node);
}

static int blk_zone_need_reset_cb(struct blk_zone *zone, unsigned int idx,
				  void *data)
{
	 
	switch (zone->cond) {
	case BLK_ZONE_COND_NOT_WP:
	case BLK_ZONE_COND_EMPTY:
	case BLK_ZONE_COND_READONLY:
	case BLK_ZONE_COND_OFFLINE:
		return 0;
	default:
		set_bit(idx, (unsigned long *)data);
		return 0;
	}
}

static int blkdev_zone_reset_all_emulated(struct block_device *bdev,
					  gfp_t gfp_mask)
{
	struct gendisk *disk = bdev->bd_disk;
	sector_t capacity = bdev_nr_sectors(bdev);
	sector_t zone_sectors = bdev_zone_sectors(bdev);
	unsigned long *need_reset;
	struct bio *bio = NULL;
	sector_t sector = 0;
	int ret;

	need_reset = blk_alloc_zone_bitmap(disk->queue->node, disk->nr_zones);
	if (!need_reset)
		return -ENOMEM;

	ret = disk->fops->report_zones(disk, 0, disk->nr_zones,
				       blk_zone_need_reset_cb, need_reset);
	if (ret < 0)
		goto out_free_need_reset;

	ret = 0;
	while (sector < capacity) {
		if (!test_bit(disk_zone_no(disk, sector), need_reset)) {
			sector += zone_sectors;
			continue;
		}

		bio = blk_next_bio(bio, bdev, 0, REQ_OP_ZONE_RESET | REQ_SYNC,
				   gfp_mask);
		bio->bi_iter.bi_sector = sector;
		sector += zone_sectors;

		 
		cond_resched();
	}

	if (bio) {
		ret = submit_bio_wait(bio);
		bio_put(bio);
	}

out_free_need_reset:
	kfree(need_reset);
	return ret;
}

static int blkdev_zone_reset_all(struct block_device *bdev, gfp_t gfp_mask)
{
	struct bio bio;

	bio_init(&bio, bdev, NULL, 0, REQ_OP_ZONE_RESET_ALL | REQ_SYNC);
	return submit_bio_wait(&bio);
}

 
int blkdev_zone_mgmt(struct block_device *bdev, enum req_op op,
		     sector_t sector, sector_t nr_sectors, gfp_t gfp_mask)
{
	struct request_queue *q = bdev_get_queue(bdev);
	sector_t zone_sectors = bdev_zone_sectors(bdev);
	sector_t capacity = bdev_nr_sectors(bdev);
	sector_t end_sector = sector + nr_sectors;
	struct bio *bio = NULL;
	int ret = 0;

	if (!bdev_is_zoned(bdev))
		return -EOPNOTSUPP;

	if (bdev_read_only(bdev))
		return -EPERM;

	if (!op_is_zone_mgmt(op))
		return -EOPNOTSUPP;

	if (end_sector <= sector || end_sector > capacity)
		 
		return -EINVAL;

	 
	if (!bdev_is_zone_start(bdev, sector))
		return -EINVAL;

	if (!bdev_is_zone_start(bdev, nr_sectors) && end_sector != capacity)
		return -EINVAL;

	 
	if (op == REQ_OP_ZONE_RESET && sector == 0 && nr_sectors == capacity) {
		if (!blk_queue_zone_resetall(q))
			return blkdev_zone_reset_all_emulated(bdev, gfp_mask);
		return blkdev_zone_reset_all(bdev, gfp_mask);
	}

	while (sector < end_sector) {
		bio = blk_next_bio(bio, bdev, 0, op | REQ_SYNC, gfp_mask);
		bio->bi_iter.bi_sector = sector;
		sector += zone_sectors;

		 
		cond_resched();
	}

	ret = submit_bio_wait(bio);
	bio_put(bio);

	return ret;
}
EXPORT_SYMBOL_GPL(blkdev_zone_mgmt);

struct zone_report_args {
	struct blk_zone __user *zones;
};

static int blkdev_copy_zone_to_user(struct blk_zone *zone, unsigned int idx,
				    void *data)
{
	struct zone_report_args *args = data;

	if (copy_to_user(&args->zones[idx], zone, sizeof(struct blk_zone)))
		return -EFAULT;
	return 0;
}

 
int blkdev_report_zones_ioctl(struct block_device *bdev, unsigned int cmd,
		unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct zone_report_args args;
	struct blk_zone_report rep;
	int ret;

	if (!argp)
		return -EINVAL;

	if (!bdev_is_zoned(bdev))
		return -ENOTTY;

	if (copy_from_user(&rep, argp, sizeof(struct blk_zone_report)))
		return -EFAULT;

	if (!rep.nr_zones)
		return -EINVAL;

	args.zones = argp + sizeof(struct blk_zone_report);
	ret = blkdev_report_zones(bdev, rep.sector, rep.nr_zones,
				  blkdev_copy_zone_to_user, &args);
	if (ret < 0)
		return ret;

	rep.nr_zones = ret;
	rep.flags = BLK_ZONE_REP_CAPACITY;
	if (copy_to_user(argp, &rep, sizeof(struct blk_zone_report)))
		return -EFAULT;
	return 0;
}

static int blkdev_truncate_zone_range(struct block_device *bdev,
		blk_mode_t mode, const struct blk_zone_range *zrange)
{
	loff_t start, end;

	if (zrange->sector + zrange->nr_sectors <= zrange->sector ||
	    zrange->sector + zrange->nr_sectors > get_capacity(bdev->bd_disk))
		 
		return -EINVAL;

	start = zrange->sector << SECTOR_SHIFT;
	end = ((zrange->sector + zrange->nr_sectors) << SECTOR_SHIFT) - 1;

	return truncate_bdev_range(bdev, mode, start, end);
}

 
int blkdev_zone_mgmt_ioctl(struct block_device *bdev, blk_mode_t mode,
			   unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct blk_zone_range zrange;
	enum req_op op;
	int ret;

	if (!argp)
		return -EINVAL;

	if (!bdev_is_zoned(bdev))
		return -ENOTTY;

	if (!(mode & BLK_OPEN_WRITE))
		return -EBADF;

	if (copy_from_user(&zrange, argp, sizeof(struct blk_zone_range)))
		return -EFAULT;

	switch (cmd) {
	case BLKRESETZONE:
		op = REQ_OP_ZONE_RESET;

		 
		filemap_invalidate_lock(bdev->bd_inode->i_mapping);
		ret = blkdev_truncate_zone_range(bdev, mode, &zrange);
		if (ret)
			goto fail;
		break;
	case BLKOPENZONE:
		op = REQ_OP_ZONE_OPEN;
		break;
	case BLKCLOSEZONE:
		op = REQ_OP_ZONE_CLOSE;
		break;
	case BLKFINISHZONE:
		op = REQ_OP_ZONE_FINISH;
		break;
	default:
		return -ENOTTY;
	}

	ret = blkdev_zone_mgmt(bdev, op, zrange.sector, zrange.nr_sectors,
			       GFP_KERNEL);

fail:
	if (cmd == BLKRESETZONE)
		filemap_invalidate_unlock(bdev->bd_inode->i_mapping);

	return ret;
}

void disk_free_zone_bitmaps(struct gendisk *disk)
{
	kfree(disk->conv_zones_bitmap);
	disk->conv_zones_bitmap = NULL;
	kfree(disk->seq_zones_wlock);
	disk->seq_zones_wlock = NULL;
}

struct blk_revalidate_zone_args {
	struct gendisk	*disk;
	unsigned long	*conv_zones_bitmap;
	unsigned long	*seq_zones_wlock;
	unsigned int	nr_zones;
	sector_t	sector;
};

 
static int blk_revalidate_zone_cb(struct blk_zone *zone, unsigned int idx,
				  void *data)
{
	struct blk_revalidate_zone_args *args = data;
	struct gendisk *disk = args->disk;
	struct request_queue *q = disk->queue;
	sector_t capacity = get_capacity(disk);
	sector_t zone_sectors = q->limits.chunk_sectors;

	 
	if (zone->start != args->sector) {
		pr_warn("%s: Zone gap at sectors %llu..%llu\n",
			disk->disk_name, args->sector, zone->start);
		return -ENODEV;
	}

	if (zone->start >= capacity || !zone->len) {
		pr_warn("%s: Invalid zone start %llu, length %llu\n",
			disk->disk_name, zone->start, zone->len);
		return -ENODEV;
	}

	 
	if (zone->start + zone->len < capacity) {
		if (zone->len != zone_sectors) {
			pr_warn("%s: Invalid zoned device with non constant zone size\n",
				disk->disk_name);
			return -ENODEV;
		}
	} else if (zone->len > zone_sectors) {
		pr_warn("%s: Invalid zoned device with larger last zone size\n",
			disk->disk_name);
		return -ENODEV;
	}

	 
	switch (zone->type) {
	case BLK_ZONE_TYPE_CONVENTIONAL:
		if (!args->conv_zones_bitmap) {
			args->conv_zones_bitmap =
				blk_alloc_zone_bitmap(q->node, args->nr_zones);
			if (!args->conv_zones_bitmap)
				return -ENOMEM;
		}
		set_bit(idx, args->conv_zones_bitmap);
		break;
	case BLK_ZONE_TYPE_SEQWRITE_REQ:
	case BLK_ZONE_TYPE_SEQWRITE_PREF:
		if (!args->seq_zones_wlock) {
			args->seq_zones_wlock =
				blk_alloc_zone_bitmap(q->node, args->nr_zones);
			if (!args->seq_zones_wlock)
				return -ENOMEM;
		}
		break;
	default:
		pr_warn("%s: Invalid zone type 0x%x at sectors %llu\n",
			disk->disk_name, (int)zone->type, zone->start);
		return -ENODEV;
	}

	args->sector += zone->len;
	return 0;
}

 
int blk_revalidate_disk_zones(struct gendisk *disk,
			      void (*update_driver_data)(struct gendisk *disk))
{
	struct request_queue *q = disk->queue;
	sector_t zone_sectors = q->limits.chunk_sectors;
	sector_t capacity = get_capacity(disk);
	struct blk_revalidate_zone_args args = { };
	unsigned int noio_flag;
	int ret;

	if (WARN_ON_ONCE(!blk_queue_is_zoned(q)))
		return -EIO;
	if (WARN_ON_ONCE(!queue_is_mq(q)))
		return -EIO;

	if (!capacity)
		return -ENODEV;

	 
	if (!zone_sectors || !is_power_of_2(zone_sectors)) {
		pr_warn("%s: Invalid non power of two zone size (%llu)\n",
			disk->disk_name, zone_sectors);
		return -ENODEV;
	}

	if (!q->limits.max_zone_append_sectors) {
		pr_warn("%s: Invalid 0 maximum zone append limit\n",
			disk->disk_name);
		return -ENODEV;
	}

	 
	args.disk = disk;
	args.nr_zones = (capacity + zone_sectors - 1) >> ilog2(zone_sectors);
	noio_flag = memalloc_noio_save();
	ret = disk->fops->report_zones(disk, 0, UINT_MAX,
				       blk_revalidate_zone_cb, &args);
	if (!ret) {
		pr_warn("%s: No zones reported\n", disk->disk_name);
		ret = -ENODEV;
	}
	memalloc_noio_restore(noio_flag);

	 
	if (ret > 0 && args.sector != capacity) {
		pr_warn("%s: Missing zones from sector %llu\n",
			disk->disk_name, args.sector);
		ret = -ENODEV;
	}

	 
	blk_mq_freeze_queue(q);
	if (ret > 0) {
		disk->nr_zones = args.nr_zones;
		swap(disk->seq_zones_wlock, args.seq_zones_wlock);
		swap(disk->conv_zones_bitmap, args.conv_zones_bitmap);
		if (update_driver_data)
			update_driver_data(disk);
		ret = 0;
	} else {
		pr_warn("%s: failed to revalidate zones\n", disk->disk_name);
		disk_free_zone_bitmaps(disk);
	}
	blk_mq_unfreeze_queue(q);

	kfree(args.seq_zones_wlock);
	kfree(args.conv_zones_bitmap);
	return ret;
}
EXPORT_SYMBOL_GPL(blk_revalidate_disk_zones);

void disk_clear_zone_settings(struct gendisk *disk)
{
	struct request_queue *q = disk->queue;

	blk_mq_freeze_queue(q);

	disk_free_zone_bitmaps(disk);
	blk_queue_flag_clear(QUEUE_FLAG_ZONE_RESETALL, q);
	q->required_elevator_features &= ~ELEVATOR_F_ZBD_SEQ_WRITE;
	disk->nr_zones = 0;
	disk->max_open_zones = 0;
	disk->max_active_zones = 0;
	q->limits.chunk_sectors = 0;
	q->limits.zone_write_granularity = 0;
	q->limits.max_zone_append_sectors = 0;

	blk_mq_unfreeze_queue(q);
}
