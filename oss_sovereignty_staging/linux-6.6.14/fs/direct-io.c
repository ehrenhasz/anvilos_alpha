
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/task_io_accounting_ops.h>
#include <linux/bio.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/rwsem.h>
#include <linux/uio.h>
#include <linux/atomic.h>
#include <linux/prefetch.h>

#include "internal.h"

 
#define DIO_PAGES	64

 
#define DIO_COMPLETE_ASYNC		0x01	 
#define DIO_COMPLETE_INVALIDATE		0x02	 

 

 

struct dio_submit {
	struct bio *bio;		 
	unsigned blkbits;		 
	unsigned blkfactor;		 
	unsigned start_zero_done;	 
	int pages_in_io;		 
	sector_t block_in_file;		 
	unsigned blocks_available;	 
	int reap_counter;		 
	sector_t final_block_in_request; 
	int boundary;			 
	get_block_t *get_block;		 

	loff_t logical_offset_in_bio;	 
	sector_t final_block_in_bio;	 
	sector_t next_block_for_io;	 

	 
	struct page *cur_page;		 
	unsigned cur_page_offset;	 
	unsigned cur_page_len;		 
	sector_t cur_page_block;	 
	loff_t cur_page_fs_offset;	 

	struct iov_iter *iter;
	 
	unsigned head;			 
	unsigned tail;			 
	size_t from, to;
};

 
struct dio {
	int flags;			 
	blk_opf_t opf;			 
	struct gendisk *bio_disk;
	struct inode *inode;
	loff_t i_size;			 
	dio_iodone_t *end_io;		 
	bool is_pinned;			 

	void *private;			 

	 
	spinlock_t bio_lock;		 
	int page_errors;		 
	int is_async;			 
	bool defer_completion;		 
	bool should_dirty;		 
	int io_error;			 
	unsigned long refcount;		 
	struct bio *bio_list;		 
	struct task_struct *waiter;	 

	 
	struct kiocb *iocb;		 
	ssize_t result;                  

	 
	union {
		struct page *pages[DIO_PAGES];	 
		struct work_struct complete_work; 
	};
} ____cacheline_aligned_in_smp;

static struct kmem_cache *dio_cache __read_mostly;

 
static inline unsigned dio_pages_present(struct dio_submit *sdio)
{
	return sdio->tail - sdio->head;
}

 
static inline int dio_refill_pages(struct dio *dio, struct dio_submit *sdio)
{
	struct page **pages = dio->pages;
	const enum req_op dio_op = dio->opf & REQ_OP_MASK;
	ssize_t ret;

	ret = iov_iter_extract_pages(sdio->iter, &pages, LONG_MAX,
				     DIO_PAGES, 0, &sdio->from);

	if (ret < 0 && sdio->blocks_available && dio_op == REQ_OP_WRITE) {
		 
		if (dio->page_errors == 0)
			dio->page_errors = ret;
		dio->pages[0] = ZERO_PAGE(0);
		sdio->head = 0;
		sdio->tail = 1;
		sdio->from = 0;
		sdio->to = PAGE_SIZE;
		return 0;
	}

	if (ret >= 0) {
		ret += sdio->from;
		sdio->head = 0;
		sdio->tail = (ret + PAGE_SIZE - 1) / PAGE_SIZE;
		sdio->to = ((ret - 1) & (PAGE_SIZE - 1)) + 1;
		return 0;
	}
	return ret;	
}

 
static inline struct page *dio_get_page(struct dio *dio,
					struct dio_submit *sdio)
{
	if (dio_pages_present(sdio) == 0) {
		int ret;

		ret = dio_refill_pages(dio, sdio);
		if (ret)
			return ERR_PTR(ret);
		BUG_ON(dio_pages_present(sdio) == 0);
	}
	return dio->pages[sdio->head];
}

static void dio_pin_page(struct dio *dio, struct page *page)
{
	if (dio->is_pinned)
		folio_add_pin(page_folio(page));
}

static void dio_unpin_page(struct dio *dio, struct page *page)
{
	if (dio->is_pinned)
		unpin_user_page(page);
}

 
static ssize_t dio_complete(struct dio *dio, ssize_t ret, unsigned int flags)
{
	const enum req_op dio_op = dio->opf & REQ_OP_MASK;
	loff_t offset = dio->iocb->ki_pos;
	ssize_t transferred = 0;
	int err;

	 
	if (ret == -EIOCBQUEUED)
		ret = 0;

	if (dio->result) {
		transferred = dio->result;

		 
		if (dio_op == REQ_OP_READ &&
		    ((offset + transferred) > dio->i_size))
			transferred = dio->i_size - offset;
		 
		if (unlikely(ret == -EFAULT) && transferred)
			ret = 0;
	}

	if (ret == 0)
		ret = dio->page_errors;
	if (ret == 0)
		ret = dio->io_error;
	if (ret == 0)
		ret = transferred;

	if (dio->end_io) {
		 
		err = dio->end_io(dio->iocb, offset, ret, dio->private);
		if (err)
			ret = err;
	}

	 
	if (flags & DIO_COMPLETE_INVALIDATE &&
	    ret > 0 && dio_op == REQ_OP_WRITE)
		kiocb_invalidate_post_direct_write(dio->iocb, ret);

	inode_dio_end(dio->inode);

	if (flags & DIO_COMPLETE_ASYNC) {
		 
		dio->iocb->ki_pos += transferred;

		if (ret > 0 && dio_op == REQ_OP_WRITE)
			ret = generic_write_sync(dio->iocb, ret);
		dio->iocb->ki_complete(dio->iocb, ret);
	}

	kmem_cache_free(dio_cache, dio);
	return ret;
}

static void dio_aio_complete_work(struct work_struct *work)
{
	struct dio *dio = container_of(work, struct dio, complete_work);

	dio_complete(dio, 0, DIO_COMPLETE_ASYNC | DIO_COMPLETE_INVALIDATE);
}

static blk_status_t dio_bio_complete(struct dio *dio, struct bio *bio);

 
static void dio_bio_end_aio(struct bio *bio)
{
	struct dio *dio = bio->bi_private;
	const enum req_op dio_op = dio->opf & REQ_OP_MASK;
	unsigned long remaining;
	unsigned long flags;
	bool defer_completion = false;

	 
	dio_bio_complete(dio, bio);

	spin_lock_irqsave(&dio->bio_lock, flags);
	remaining = --dio->refcount;
	if (remaining == 1 && dio->waiter)
		wake_up_process(dio->waiter);
	spin_unlock_irqrestore(&dio->bio_lock, flags);

	if (remaining == 0) {
		 
		if (dio->result)
			defer_completion = dio->defer_completion ||
					   (dio_op == REQ_OP_WRITE &&
					    dio->inode->i_mapping->nrpages);
		if (defer_completion) {
			INIT_WORK(&dio->complete_work, dio_aio_complete_work);
			queue_work(dio->inode->i_sb->s_dio_done_wq,
				   &dio->complete_work);
		} else {
			dio_complete(dio, 0, DIO_COMPLETE_ASYNC);
		}
	}
}

 
static void dio_bio_end_io(struct bio *bio)
{
	struct dio *dio = bio->bi_private;
	unsigned long flags;

	spin_lock_irqsave(&dio->bio_lock, flags);
	bio->bi_private = dio->bio_list;
	dio->bio_list = bio;
	if (--dio->refcount == 1 && dio->waiter)
		wake_up_process(dio->waiter);
	spin_unlock_irqrestore(&dio->bio_lock, flags);
}

static inline void
dio_bio_alloc(struct dio *dio, struct dio_submit *sdio,
	      struct block_device *bdev,
	      sector_t first_sector, int nr_vecs)
{
	struct bio *bio;

	 
	bio = bio_alloc(bdev, nr_vecs, dio->opf, GFP_KERNEL);
	bio->bi_iter.bi_sector = first_sector;
	if (dio->is_async)
		bio->bi_end_io = dio_bio_end_aio;
	else
		bio->bi_end_io = dio_bio_end_io;
	if (dio->is_pinned)
		bio_set_flag(bio, BIO_PAGE_PINNED);
	sdio->bio = bio;
	sdio->logical_offset_in_bio = sdio->cur_page_fs_offset;
}

 
static inline void dio_bio_submit(struct dio *dio, struct dio_submit *sdio)
{
	const enum req_op dio_op = dio->opf & REQ_OP_MASK;
	struct bio *bio = sdio->bio;
	unsigned long flags;

	bio->bi_private = dio;

	spin_lock_irqsave(&dio->bio_lock, flags);
	dio->refcount++;
	spin_unlock_irqrestore(&dio->bio_lock, flags);

	if (dio->is_async && dio_op == REQ_OP_READ && dio->should_dirty)
		bio_set_pages_dirty(bio);

	dio->bio_disk = bio->bi_bdev->bd_disk;

	submit_bio(bio);

	sdio->bio = NULL;
	sdio->boundary = 0;
	sdio->logical_offset_in_bio = 0;
}

 
static inline void dio_cleanup(struct dio *dio, struct dio_submit *sdio)
{
	if (dio->is_pinned)
		unpin_user_pages(dio->pages + sdio->head,
				 sdio->tail - sdio->head);
	sdio->head = sdio->tail;
}

 
static struct bio *dio_await_one(struct dio *dio)
{
	unsigned long flags;
	struct bio *bio = NULL;

	spin_lock_irqsave(&dio->bio_lock, flags);

	 
	while (dio->refcount > 1 && dio->bio_list == NULL) {
		__set_current_state(TASK_UNINTERRUPTIBLE);
		dio->waiter = current;
		spin_unlock_irqrestore(&dio->bio_lock, flags);
		blk_io_schedule();
		 
		spin_lock_irqsave(&dio->bio_lock, flags);
		dio->waiter = NULL;
	}
	if (dio->bio_list) {
		bio = dio->bio_list;
		dio->bio_list = bio->bi_private;
	}
	spin_unlock_irqrestore(&dio->bio_lock, flags);
	return bio;
}

 
static blk_status_t dio_bio_complete(struct dio *dio, struct bio *bio)
{
	blk_status_t err = bio->bi_status;
	const enum req_op dio_op = dio->opf & REQ_OP_MASK;
	bool should_dirty = dio_op == REQ_OP_READ && dio->should_dirty;

	if (err) {
		if (err == BLK_STS_AGAIN && (bio->bi_opf & REQ_NOWAIT))
			dio->io_error = -EAGAIN;
		else
			dio->io_error = -EIO;
	}

	if (dio->is_async && should_dirty) {
		bio_check_pages_dirty(bio);	 
	} else {
		bio_release_pages(bio, should_dirty);
		bio_put(bio);
	}
	return err;
}

 
static void dio_await_completion(struct dio *dio)
{
	struct bio *bio;
	do {
		bio = dio_await_one(dio);
		if (bio)
			dio_bio_complete(dio, bio);
	} while (bio);
}

 
static inline int dio_bio_reap(struct dio *dio, struct dio_submit *sdio)
{
	int ret = 0;

	if (sdio->reap_counter++ >= 64) {
		while (dio->bio_list) {
			unsigned long flags;
			struct bio *bio;
			int ret2;

			spin_lock_irqsave(&dio->bio_lock, flags);
			bio = dio->bio_list;
			dio->bio_list = bio->bi_private;
			spin_unlock_irqrestore(&dio->bio_lock, flags);
			ret2 = blk_status_to_errno(dio_bio_complete(dio, bio));
			if (ret == 0)
				ret = ret2;
		}
		sdio->reap_counter = 0;
	}
	return ret;
}

static int dio_set_defer_completion(struct dio *dio)
{
	struct super_block *sb = dio->inode->i_sb;

	if (dio->defer_completion)
		return 0;
	dio->defer_completion = true;
	if (!sb->s_dio_done_wq)
		return sb_init_dio_done_wq(sb);
	return 0;
}

 
static int get_more_blocks(struct dio *dio, struct dio_submit *sdio,
			   struct buffer_head *map_bh)
{
	const enum req_op dio_op = dio->opf & REQ_OP_MASK;
	int ret;
	sector_t fs_startblk;	 
	sector_t fs_endblk;	 
	unsigned long fs_count;	 
	int create;
	unsigned int i_blkbits = sdio->blkbits + sdio->blkfactor;
	loff_t i_size;

	 
	ret = dio->page_errors;
	if (ret == 0) {
		BUG_ON(sdio->block_in_file >= sdio->final_block_in_request);
		fs_startblk = sdio->block_in_file >> sdio->blkfactor;
		fs_endblk = (sdio->final_block_in_request - 1) >>
					sdio->blkfactor;
		fs_count = fs_endblk - fs_startblk + 1;

		map_bh->b_state = 0;
		map_bh->b_size = fs_count << i_blkbits;

		 
		create = dio_op == REQ_OP_WRITE;
		if (dio->flags & DIO_SKIP_HOLES) {
			i_size = i_size_read(dio->inode);
			if (i_size && fs_startblk <= (i_size - 1) >> i_blkbits)
				create = 0;
		}

		ret = (*sdio->get_block)(dio->inode, fs_startblk,
						map_bh, create);

		 
		dio->private = map_bh->b_private;

		if (ret == 0 && buffer_defer_completion(map_bh))
			ret = dio_set_defer_completion(dio);
	}
	return ret;
}

 
static inline int dio_new_bio(struct dio *dio, struct dio_submit *sdio,
		sector_t start_sector, struct buffer_head *map_bh)
{
	sector_t sector;
	int ret, nr_pages;

	ret = dio_bio_reap(dio, sdio);
	if (ret)
		goto out;
	sector = start_sector << (sdio->blkbits - 9);
	nr_pages = bio_max_segs(sdio->pages_in_io);
	BUG_ON(nr_pages <= 0);
	dio_bio_alloc(dio, sdio, map_bh->b_bdev, sector, nr_pages);
	sdio->boundary = 0;
out:
	return ret;
}

 
static inline int dio_bio_add_page(struct dio *dio, struct dio_submit *sdio)
{
	int ret;

	ret = bio_add_page(sdio->bio, sdio->cur_page,
			sdio->cur_page_len, sdio->cur_page_offset);
	if (ret == sdio->cur_page_len) {
		 
		if ((sdio->cur_page_len + sdio->cur_page_offset) == PAGE_SIZE)
			sdio->pages_in_io--;
		dio_pin_page(dio, sdio->cur_page);
		sdio->final_block_in_bio = sdio->cur_page_block +
			(sdio->cur_page_len >> sdio->blkbits);
		ret = 0;
	} else {
		ret = 1;
	}
	return ret;
}
		
 
static inline int dio_send_cur_page(struct dio *dio, struct dio_submit *sdio,
		struct buffer_head *map_bh)
{
	int ret = 0;

	if (sdio->bio) {
		loff_t cur_offset = sdio->cur_page_fs_offset;
		loff_t bio_next_offset = sdio->logical_offset_in_bio +
			sdio->bio->bi_iter.bi_size;

		 
		if (sdio->final_block_in_bio != sdio->cur_page_block ||
		    cur_offset != bio_next_offset)
			dio_bio_submit(dio, sdio);
	}

	if (sdio->bio == NULL) {
		ret = dio_new_bio(dio, sdio, sdio->cur_page_block, map_bh);
		if (ret)
			goto out;
	}

	if (dio_bio_add_page(dio, sdio) != 0) {
		dio_bio_submit(dio, sdio);
		ret = dio_new_bio(dio, sdio, sdio->cur_page_block, map_bh);
		if (ret == 0) {
			ret = dio_bio_add_page(dio, sdio);
			BUG_ON(ret != 0);
		}
	}
out:
	return ret;
}

 
static inline int
submit_page_section(struct dio *dio, struct dio_submit *sdio, struct page *page,
		    unsigned offset, unsigned len, sector_t blocknr,
		    struct buffer_head *map_bh)
{
	const enum req_op dio_op = dio->opf & REQ_OP_MASK;
	int ret = 0;
	int boundary = sdio->boundary;	 

	if (dio_op == REQ_OP_WRITE) {
		 
		task_io_account_write(len);
	}

	 
	if (sdio->cur_page == page &&
	    sdio->cur_page_offset + sdio->cur_page_len == offset &&
	    sdio->cur_page_block +
	    (sdio->cur_page_len >> sdio->blkbits) == blocknr) {
		sdio->cur_page_len += len;
		goto out;
	}

	 
	if (sdio->cur_page) {
		ret = dio_send_cur_page(dio, sdio, map_bh);
		dio_unpin_page(dio, sdio->cur_page);
		sdio->cur_page = NULL;
		if (ret)
			return ret;
	}

	dio_pin_page(dio, page);		 
	sdio->cur_page = page;
	sdio->cur_page_offset = offset;
	sdio->cur_page_len = len;
	sdio->cur_page_block = blocknr;
	sdio->cur_page_fs_offset = sdio->block_in_file << sdio->blkbits;
out:
	 
	if (boundary) {
		ret = dio_send_cur_page(dio, sdio, map_bh);
		if (sdio->bio)
			dio_bio_submit(dio, sdio);
		dio_unpin_page(dio, sdio->cur_page);
		sdio->cur_page = NULL;
	}
	return ret;
}

 
static inline void dio_zero_block(struct dio *dio, struct dio_submit *sdio,
		int end, struct buffer_head *map_bh)
{
	unsigned dio_blocks_per_fs_block;
	unsigned this_chunk_blocks;	 
	unsigned this_chunk_bytes;
	struct page *page;

	sdio->start_zero_done = 1;
	if (!sdio->blkfactor || !buffer_new(map_bh))
		return;

	dio_blocks_per_fs_block = 1 << sdio->blkfactor;
	this_chunk_blocks = sdio->block_in_file & (dio_blocks_per_fs_block - 1);

	if (!this_chunk_blocks)
		return;

	 
	if (end) 
		this_chunk_blocks = dio_blocks_per_fs_block - this_chunk_blocks;

	this_chunk_bytes = this_chunk_blocks << sdio->blkbits;

	page = ZERO_PAGE(0);
	if (submit_page_section(dio, sdio, page, 0, this_chunk_bytes,
				sdio->next_block_for_io, map_bh))
		return;

	sdio->next_block_for_io += this_chunk_blocks;
}

 
static int do_direct_IO(struct dio *dio, struct dio_submit *sdio,
			struct buffer_head *map_bh)
{
	const enum req_op dio_op = dio->opf & REQ_OP_MASK;
	const unsigned blkbits = sdio->blkbits;
	const unsigned i_blkbits = blkbits + sdio->blkfactor;
	int ret = 0;

	while (sdio->block_in_file < sdio->final_block_in_request) {
		struct page *page;
		size_t from, to;

		page = dio_get_page(dio, sdio);
		if (IS_ERR(page)) {
			ret = PTR_ERR(page);
			goto out;
		}
		from = sdio->head ? 0 : sdio->from;
		to = (sdio->head == sdio->tail - 1) ? sdio->to : PAGE_SIZE;
		sdio->head++;

		while (from < to) {
			unsigned this_chunk_bytes;	 
			unsigned this_chunk_blocks;	 
			unsigned u;

			if (sdio->blocks_available == 0) {
				 
				unsigned long blkmask;
				unsigned long dio_remainder;

				ret = get_more_blocks(dio, sdio, map_bh);
				if (ret) {
					dio_unpin_page(dio, page);
					goto out;
				}
				if (!buffer_mapped(map_bh))
					goto do_holes;

				sdio->blocks_available =
						map_bh->b_size >> blkbits;
				sdio->next_block_for_io =
					map_bh->b_blocknr << sdio->blkfactor;
				if (buffer_new(map_bh)) {
					clean_bdev_aliases(
						map_bh->b_bdev,
						map_bh->b_blocknr,
						map_bh->b_size >> i_blkbits);
				}

				if (!sdio->blkfactor)
					goto do_holes;

				blkmask = (1 << sdio->blkfactor) - 1;
				dio_remainder = (sdio->block_in_file & blkmask);

				 
				if (!buffer_new(map_bh))
					sdio->next_block_for_io += dio_remainder;
				sdio->blocks_available -= dio_remainder;
			}
do_holes:
			 
			if (!buffer_mapped(map_bh)) {
				loff_t i_size_aligned;

				 
				if (dio_op == REQ_OP_WRITE) {
					dio_unpin_page(dio, page);
					return -ENOTBLK;
				}

				 
				i_size_aligned = ALIGN(i_size_read(dio->inode),
							1 << blkbits);
				if (sdio->block_in_file >=
						i_size_aligned >> blkbits) {
					 
					dio_unpin_page(dio, page);
					goto out;
				}
				zero_user(page, from, 1 << blkbits);
				sdio->block_in_file++;
				from += 1 << blkbits;
				dio->result += 1 << blkbits;
				goto next_block;
			}

			 
			if (unlikely(sdio->blkfactor && !sdio->start_zero_done))
				dio_zero_block(dio, sdio, 0, map_bh);

			 
			this_chunk_blocks = sdio->blocks_available;
			u = (to - from) >> blkbits;
			if (this_chunk_blocks > u)
				this_chunk_blocks = u;
			u = sdio->final_block_in_request - sdio->block_in_file;
			if (this_chunk_blocks > u)
				this_chunk_blocks = u;
			this_chunk_bytes = this_chunk_blocks << blkbits;
			BUG_ON(this_chunk_bytes == 0);

			if (this_chunk_blocks == sdio->blocks_available)
				sdio->boundary = buffer_boundary(map_bh);
			ret = submit_page_section(dio, sdio, page,
						  from,
						  this_chunk_bytes,
						  sdio->next_block_for_io,
						  map_bh);
			if (ret) {
				dio_unpin_page(dio, page);
				goto out;
			}
			sdio->next_block_for_io += this_chunk_blocks;

			sdio->block_in_file += this_chunk_blocks;
			from += this_chunk_bytes;
			dio->result += this_chunk_bytes;
			sdio->blocks_available -= this_chunk_blocks;
next_block:
			BUG_ON(sdio->block_in_file > sdio->final_block_in_request);
			if (sdio->block_in_file == sdio->final_block_in_request)
				break;
		}

		 
		dio_unpin_page(dio, page);
	}
out:
	return ret;
}

static inline int drop_refcount(struct dio *dio)
{
	int ret2;
	unsigned long flags;

	 
	spin_lock_irqsave(&dio->bio_lock, flags);
	ret2 = --dio->refcount;
	spin_unlock_irqrestore(&dio->bio_lock, flags);
	return ret2;
}

 
ssize_t __blockdev_direct_IO(struct kiocb *iocb, struct inode *inode,
		struct block_device *bdev, struct iov_iter *iter,
		get_block_t get_block, dio_iodone_t end_io,
		int flags)
{
	unsigned i_blkbits = READ_ONCE(inode->i_blkbits);
	unsigned blkbits = i_blkbits;
	unsigned blocksize_mask = (1 << blkbits) - 1;
	ssize_t retval = -EINVAL;
	const size_t count = iov_iter_count(iter);
	loff_t offset = iocb->ki_pos;
	const loff_t end = offset + count;
	struct dio *dio;
	struct dio_submit sdio = { 0, };
	struct buffer_head map_bh = { 0, };
	struct blk_plug plug;
	unsigned long align = offset | iov_iter_alignment(iter);

	 

	 
	if (iov_iter_rw(iter) == READ && !count)
		return 0;

	dio = kmem_cache_alloc(dio_cache, GFP_KERNEL);
	if (!dio)
		return -ENOMEM;
	 
	memset(dio, 0, offsetof(struct dio, pages));

	dio->flags = flags;
	if (dio->flags & DIO_LOCKING && iov_iter_rw(iter) == READ) {
		 
		inode_lock(inode);
	}
	dio->is_pinned = iov_iter_extract_will_pin(iter);

	 
	dio->i_size = i_size_read(inode);
	if (iov_iter_rw(iter) == READ && offset >= dio->i_size) {
		retval = 0;
		goto fail_dio;
	}

	if (align & blocksize_mask) {
		if (bdev)
			blkbits = blksize_bits(bdev_logical_block_size(bdev));
		blocksize_mask = (1 << blkbits) - 1;
		if (align & blocksize_mask)
			goto fail_dio;
	}

	if (dio->flags & DIO_LOCKING && iov_iter_rw(iter) == READ) {
		struct address_space *mapping = iocb->ki_filp->f_mapping;

		retval = filemap_write_and_wait_range(mapping, offset, end - 1);
		if (retval)
			goto fail_dio;
	}

	 
	if (is_sync_kiocb(iocb))
		dio->is_async = false;
	else if (iov_iter_rw(iter) == WRITE && end > i_size_read(inode))
		dio->is_async = false;
	else
		dio->is_async = true;

	dio->inode = inode;
	if (iov_iter_rw(iter) == WRITE) {
		dio->opf = REQ_OP_WRITE | REQ_SYNC | REQ_IDLE;
		if (iocb->ki_flags & IOCB_NOWAIT)
			dio->opf |= REQ_NOWAIT;
	} else {
		dio->opf = REQ_OP_READ;
	}

	 
	if (dio->is_async && iov_iter_rw(iter) == WRITE) {
		retval = 0;
		if (iocb_is_dsync(iocb))
			retval = dio_set_defer_completion(dio);
		else if (!dio->inode->i_sb->s_dio_done_wq) {
			 
			retval = sb_init_dio_done_wq(dio->inode->i_sb);
		}
		if (retval)
			goto fail_dio;
	}

	 
	inode_dio_begin(inode);

	retval = 0;
	sdio.blkbits = blkbits;
	sdio.blkfactor = i_blkbits - blkbits;
	sdio.block_in_file = offset >> blkbits;

	sdio.get_block = get_block;
	dio->end_io = end_io;
	sdio.final_block_in_bio = -1;
	sdio.next_block_for_io = -1;

	dio->iocb = iocb;

	spin_lock_init(&dio->bio_lock);
	dio->refcount = 1;

	dio->should_dirty = user_backed_iter(iter) && iov_iter_rw(iter) == READ;
	sdio.iter = iter;
	sdio.final_block_in_request = end >> blkbits;

	 
	if (unlikely(sdio.blkfactor))
		sdio.pages_in_io = 2;

	sdio.pages_in_io += iov_iter_npages(iter, INT_MAX);

	blk_start_plug(&plug);

	retval = do_direct_IO(dio, &sdio, &map_bh);
	if (retval)
		dio_cleanup(dio, &sdio);

	if (retval == -ENOTBLK) {
		 
		retval = 0;
	}
	 
	dio_zero_block(dio, &sdio, 1, &map_bh);

	if (sdio.cur_page) {
		ssize_t ret2;

		ret2 = dio_send_cur_page(dio, &sdio, &map_bh);
		if (retval == 0)
			retval = ret2;
		dio_unpin_page(dio, sdio.cur_page);
		sdio.cur_page = NULL;
	}
	if (sdio.bio)
		dio_bio_submit(dio, &sdio);

	blk_finish_plug(&plug);

	 
	dio_cleanup(dio, &sdio);

	 
	if (iov_iter_rw(iter) == READ && (dio->flags & DIO_LOCKING))
		inode_unlock(dio->inode);

	 
	BUG_ON(retval == -EIOCBQUEUED);
	if (dio->is_async && retval == 0 && dio->result &&
	    (iov_iter_rw(iter) == READ || dio->result == count))
		retval = -EIOCBQUEUED;
	else
		dio_await_completion(dio);

	if (drop_refcount(dio) == 0) {
		retval = dio_complete(dio, retval, DIO_COMPLETE_INVALIDATE);
	} else
		BUG_ON(retval != -EIOCBQUEUED);

	return retval;

fail_dio:
	if (dio->flags & DIO_LOCKING && iov_iter_rw(iter) == READ)
		inode_unlock(inode);

	kmem_cache_free(dio_cache, dio);
	return retval;
}
EXPORT_SYMBOL(__blockdev_direct_IO);

static __init int dio_init(void)
{
	dio_cache = KMEM_CACHE(dio, SLAB_PANIC);
	return 0;
}
module_init(dio_init)
