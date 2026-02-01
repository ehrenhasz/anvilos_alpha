
 

#include "dm.h"
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/dm-bufio.h>

#define DM_MSG_PREFIX "ebs"

static void ebs_dtr(struct dm_target *ti);

 
struct ebs_c {
	struct dm_dev *dev;		 
	struct dm_bufio_client *bufio;	 
	struct workqueue_struct *wq;	 
	struct work_struct ws;		 
	struct bio_list bios_in;	 
	spinlock_t lock;		 
	sector_t start;			 
	unsigned int e_bs;		 
	unsigned int u_bs;		 
	unsigned char block_shift;	 
	bool u_bs_set:1;		 
};

static inline sector_t __sector_to_block(struct ebs_c *ec, sector_t sector)
{
	return sector >> ec->block_shift;
}

static inline sector_t __block_mod(sector_t sector, unsigned int bs)
{
	return sector & (bs - 1);
}

 
static inline unsigned int __nr_blocks(struct ebs_c *ec, struct bio *bio)
{
	sector_t end_sector = __block_mod(bio->bi_iter.bi_sector, ec->u_bs) + bio_sectors(bio);

	return __sector_to_block(ec, end_sector) + (__block_mod(end_sector, ec->u_bs) ? 1 : 0);
}

static inline bool __ebs_check_bs(unsigned int bs)
{
	return bs && is_power_of_2(bs);
}

 
static int __ebs_rw_bvec(struct ebs_c *ec, enum req_op op, struct bio_vec *bv,
			 struct bvec_iter *iter)
{
	int r = 0;
	unsigned char *ba, *pa;
	unsigned int cur_len;
	unsigned int bv_len = bv->bv_len;
	unsigned int buf_off = to_bytes(__block_mod(iter->bi_sector, ec->u_bs));
	sector_t block = __sector_to_block(ec, iter->bi_sector);
	struct dm_buffer *b;

	if (unlikely(!bv->bv_page || !bv_len))
		return -EIO;

	pa = bvec_virt(bv);

	 
	while (bv_len) {
		cur_len = min(dm_bufio_get_block_size(ec->bufio) - buf_off, bv_len);

		 
		if (op == REQ_OP_READ || buf_off || bv_len < dm_bufio_get_block_size(ec->bufio))
			ba = dm_bufio_read(ec->bufio, block, &b);
		else
			ba = dm_bufio_new(ec->bufio, block, &b);

		if (IS_ERR(ba)) {
			 
			r = PTR_ERR(ba);
		} else {
			 
			ba += buf_off;
			if (op == REQ_OP_READ) {
				memcpy(pa, ba, cur_len);
				flush_dcache_page(bv->bv_page);
			} else {
				flush_dcache_page(bv->bv_page);
				memcpy(ba, pa, cur_len);
				dm_bufio_mark_partial_buffer_dirty(b, buf_off, buf_off + cur_len);
			}

			dm_bufio_release(b);
		}

		pa += cur_len;
		bv_len -= cur_len;
		buf_off = 0;
		block++;
	}

	return r;
}

 
static int __ebs_rw_bio(struct ebs_c *ec, enum req_op op, struct bio *bio)
{
	int r = 0, rr;
	struct bio_vec bv;
	struct bvec_iter iter;

	bio_for_each_bvec(bv, bio, iter) {
		rr = __ebs_rw_bvec(ec, op, &bv, &iter);
		if (rr)
			r = rr;
	}

	return r;
}

 
static int __ebs_discard_bio(struct ebs_c *ec, struct bio *bio)
{
	sector_t block, blocks, sector = bio->bi_iter.bi_sector;

	block = __sector_to_block(ec, sector);
	blocks = __nr_blocks(ec, bio);

	 
	if (__block_mod(sector, ec->u_bs)) {
		block++;
		blocks--;
	}

	 
	if (blocks && __block_mod(bio_end_sector(bio), ec->u_bs))
		blocks--;

	return blocks ? dm_bufio_issue_discard(ec->bufio, block, blocks) : 0;
}

 
static void __ebs_forget_bio(struct ebs_c *ec, struct bio *bio)
{
	sector_t blocks, sector = bio->bi_iter.bi_sector;

	blocks = __nr_blocks(ec, bio);

	dm_bufio_forget_buffers(ec->bufio, __sector_to_block(ec, sector), blocks);
}

 
static void __ebs_process_bios(struct work_struct *ws)
{
	int r;
	bool write = false;
	sector_t block1, block2;
	struct ebs_c *ec = container_of(ws, struct ebs_c, ws);
	struct bio *bio;
	struct bio_list bios;

	bio_list_init(&bios);

	spin_lock_irq(&ec->lock);
	bios = ec->bios_in;
	bio_list_init(&ec->bios_in);
	spin_unlock_irq(&ec->lock);

	 
	bio_list_for_each(bio, &bios) {
		block1 = __sector_to_block(ec, bio->bi_iter.bi_sector);
		if (bio_op(bio) == REQ_OP_READ)
			dm_bufio_prefetch(ec->bufio, block1, __nr_blocks(ec, bio));
		else if (bio_op(bio) == REQ_OP_WRITE && !(bio->bi_opf & REQ_PREFLUSH)) {
			block2 = __sector_to_block(ec, bio_end_sector(bio));
			if (__block_mod(bio->bi_iter.bi_sector, ec->u_bs))
				dm_bufio_prefetch(ec->bufio, block1, 1);
			if (__block_mod(bio_end_sector(bio), ec->u_bs) && block2 != block1)
				dm_bufio_prefetch(ec->bufio, block2, 1);
		}
	}

	bio_list_for_each(bio, &bios) {
		r = -EIO;
		if (bio_op(bio) == REQ_OP_READ)
			r = __ebs_rw_bio(ec, REQ_OP_READ, bio);
		else if (bio_op(bio) == REQ_OP_WRITE) {
			write = true;
			r = __ebs_rw_bio(ec, REQ_OP_WRITE, bio);
		} else if (bio_op(bio) == REQ_OP_DISCARD) {
			__ebs_forget_bio(ec, bio);
			r = __ebs_discard_bio(ec, bio);
		}

		if (r < 0)
			bio->bi_status = errno_to_blk_status(r);
	}

	 
	r = write ? dm_bufio_write_dirty_buffers(ec->bufio) : 0;

	while ((bio = bio_list_pop(&bios))) {
		 
		if (unlikely(r && bio_op(bio) == REQ_OP_WRITE))
			bio_io_error(bio);
		else
			bio_endio(bio);
	}
}

 
static int ebs_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
	int r;
	unsigned short tmp1;
	unsigned long long tmp;
	char dummy;
	struct ebs_c *ec;

	if (argc < 3 || argc > 4) {
		ti->error = "Invalid argument count";
		return -EINVAL;
	}

	ec = ti->private = kzalloc(sizeof(*ec), GFP_KERNEL);
	if (!ec) {
		ti->error = "Cannot allocate ebs context";
		return -ENOMEM;
	}

	r = -EINVAL;
	if (sscanf(argv[1], "%llu%c", &tmp, &dummy) != 1 ||
	    tmp != (sector_t)tmp ||
	    (sector_t)tmp >= ti->len) {
		ti->error = "Invalid device offset sector";
		goto bad;
	}
	ec->start = tmp;

	if (sscanf(argv[2], "%hu%c", &tmp1, &dummy) != 1 ||
	    !__ebs_check_bs(tmp1) ||
	    to_bytes(tmp1) > PAGE_SIZE) {
		ti->error = "Invalid emulated block size";
		goto bad;
	}
	ec->e_bs = tmp1;

	if (argc > 3) {
		if (sscanf(argv[3], "%hu%c", &tmp1, &dummy) != 1 || !__ebs_check_bs(tmp1)) {
			ti->error = "Invalid underlying block size";
			goto bad;
		}
		ec->u_bs = tmp1;
		ec->u_bs_set = true;
	} else
		ec->u_bs_set = false;

	r = dm_get_device(ti, argv[0], dm_table_get_mode(ti->table), &ec->dev);
	if (r) {
		ti->error = "Device lookup failed";
		ec->dev = NULL;
		goto bad;
	}

	r = -EINVAL;
	if (!ec->u_bs_set) {
		ec->u_bs = to_sector(bdev_logical_block_size(ec->dev->bdev));
		if (!__ebs_check_bs(ec->u_bs)) {
			ti->error = "Invalid retrieved underlying block size";
			goto bad;
		}
	}

	if (!ec->u_bs_set && ec->e_bs == ec->u_bs)
		DMINFO("Emulation superfluous: emulated equal to underlying block size");

	if (__block_mod(ec->start, ec->u_bs)) {
		ti->error = "Device offset must be multiple of underlying block size";
		goto bad;
	}

	ec->bufio = dm_bufio_client_create(ec->dev->bdev, to_bytes(ec->u_bs), 1,
					   0, NULL, NULL, 0);
	if (IS_ERR(ec->bufio)) {
		ti->error = "Cannot create dm bufio client";
		r = PTR_ERR(ec->bufio);
		ec->bufio = NULL;
		goto bad;
	}

	ec->wq = alloc_ordered_workqueue("dm-" DM_MSG_PREFIX, WQ_MEM_RECLAIM);
	if (!ec->wq) {
		ti->error = "Cannot create dm-" DM_MSG_PREFIX " workqueue";
		r = -ENOMEM;
		goto bad;
	}

	ec->block_shift = __ffs(ec->u_bs);
	INIT_WORK(&ec->ws, &__ebs_process_bios);
	bio_list_init(&ec->bios_in);
	spin_lock_init(&ec->lock);

	ti->num_flush_bios = 1;
	ti->num_discard_bios = 1;
	ti->num_secure_erase_bios = 0;
	ti->num_write_zeroes_bios = 0;
	return 0;
bad:
	ebs_dtr(ti);
	return r;
}

static void ebs_dtr(struct dm_target *ti)
{
	struct ebs_c *ec = ti->private;

	if (ec->wq)
		destroy_workqueue(ec->wq);
	if (ec->bufio)
		dm_bufio_client_destroy(ec->bufio);
	if (ec->dev)
		dm_put_device(ti, ec->dev);
	kfree(ec);
}

static int ebs_map(struct dm_target *ti, struct bio *bio)
{
	struct ebs_c *ec = ti->private;

	bio_set_dev(bio, ec->dev->bdev);
	bio->bi_iter.bi_sector = ec->start + dm_target_offset(ti, bio->bi_iter.bi_sector);

	if (unlikely(bio_op(bio) == REQ_OP_FLUSH))
		return DM_MAPIO_REMAPPED;
	 
	if (likely(__block_mod(bio->bi_iter.bi_sector, ec->u_bs) ||
		   __block_mod(bio_end_sector(bio), ec->u_bs) ||
		   ec->e_bs == ec->u_bs)) {
		spin_lock_irq(&ec->lock);
		bio_list_add(&ec->bios_in, bio);
		spin_unlock_irq(&ec->lock);

		queue_work(ec->wq, &ec->ws);

		return DM_MAPIO_SUBMITTED;
	}

	 
	__ebs_forget_bio(ec, bio);

	return DM_MAPIO_REMAPPED;
}

static void ebs_status(struct dm_target *ti, status_type_t type,
		       unsigned int status_flags, char *result, unsigned int maxlen)
{
	struct ebs_c *ec = ti->private;

	switch (type) {
	case STATUSTYPE_INFO:
		*result = '\0';
		break;
	case STATUSTYPE_TABLE:
		snprintf(result, maxlen, ec->u_bs_set ? "%s %llu %u %u" : "%s %llu %u",
			 ec->dev->name, (unsigned long long) ec->start, ec->e_bs, ec->u_bs);
		break;
	case STATUSTYPE_IMA:
		*result = '\0';
		break;
	}
}

static int ebs_prepare_ioctl(struct dm_target *ti, struct block_device **bdev)
{
	struct ebs_c *ec = ti->private;
	struct dm_dev *dev = ec->dev;

	 
	*bdev = dev->bdev;
	return !!(ec->start || ti->len != bdev_nr_sectors(dev->bdev));
}

static void ebs_io_hints(struct dm_target *ti, struct queue_limits *limits)
{
	struct ebs_c *ec = ti->private;

	limits->logical_block_size = to_bytes(ec->e_bs);
	limits->physical_block_size = to_bytes(ec->u_bs);
	limits->alignment_offset = limits->physical_block_size;
	blk_limits_io_min(limits, limits->logical_block_size);
}

static int ebs_iterate_devices(struct dm_target *ti,
				  iterate_devices_callout_fn fn, void *data)
{
	struct ebs_c *ec = ti->private;

	return fn(ti, ec->dev, ec->start, ti->len, data);
}

static struct target_type ebs_target = {
	.name		 = "ebs",
	.version	 = {1, 0, 1},
	.features	 = DM_TARGET_PASSES_INTEGRITY,
	.module		 = THIS_MODULE,
	.ctr		 = ebs_ctr,
	.dtr		 = ebs_dtr,
	.map		 = ebs_map,
	.status		 = ebs_status,
	.io_hints	 = ebs_io_hints,
	.prepare_ioctl	 = ebs_prepare_ioctl,
	.iterate_devices = ebs_iterate_devices,
};
module_dm(ebs);

MODULE_AUTHOR("Heinz Mauelshagen <dm-devel@redhat.com>");
MODULE_DESCRIPTION(DM_NAME " emulated block size target");
MODULE_LICENSE("GPL");
