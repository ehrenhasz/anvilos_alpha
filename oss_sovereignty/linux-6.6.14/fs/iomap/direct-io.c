
 
#include <linux/module.h>
#include <linux/compiler.h>
#include <linux/fs.h>
#include <linux/fscrypt.h>
#include <linux/pagemap.h>
#include <linux/iomap.h>
#include <linux/backing-dev.h>
#include <linux/uio.h>
#include <linux/task_io_accounting_ops.h>
#include "trace.h"

#include "../internal.h"

 
#define IOMAP_DIO_CALLER_COMP	(1U << 26)
#define IOMAP_DIO_INLINE_COMP	(1U << 27)
#define IOMAP_DIO_WRITE_THROUGH	(1U << 28)
#define IOMAP_DIO_NEED_SYNC	(1U << 29)
#define IOMAP_DIO_WRITE		(1U << 30)
#define IOMAP_DIO_DIRTY		(1U << 31)

struct iomap_dio {
	struct kiocb		*iocb;
	const struct iomap_dio_ops *dops;
	loff_t			i_size;
	loff_t			size;
	atomic_t		ref;
	unsigned		flags;
	int			error;
	size_t			done_before;
	bool			wait_for_completion;

	union {
		 
		struct {
			struct iov_iter		*iter;
			struct task_struct	*waiter;
		} submit;

		 
		struct {
			struct work_struct	work;
		} aio;
	};
};

static struct bio *iomap_dio_alloc_bio(const struct iomap_iter *iter,
		struct iomap_dio *dio, unsigned short nr_vecs, blk_opf_t opf)
{
	if (dio->dops && dio->dops->bio_set)
		return bio_alloc_bioset(iter->iomap.bdev, nr_vecs, opf,
					GFP_KERNEL, dio->dops->bio_set);
	return bio_alloc(iter->iomap.bdev, nr_vecs, opf, GFP_KERNEL);
}

static void iomap_dio_submit_bio(const struct iomap_iter *iter,
		struct iomap_dio *dio, struct bio *bio, loff_t pos)
{
	struct kiocb *iocb = dio->iocb;

	atomic_inc(&dio->ref);

	 
	if ((iocb->ki_flags & IOCB_HIPRI) && !is_sync_kiocb(iocb)) {
		bio_set_polled(bio, iocb);
		WRITE_ONCE(iocb->private, bio);
	}

	if (dio->dops && dio->dops->submit_io)
		dio->dops->submit_io(iter, bio, pos);
	else
		submit_bio(bio);
}

ssize_t iomap_dio_complete(struct iomap_dio *dio)
{
	const struct iomap_dio_ops *dops = dio->dops;
	struct kiocb *iocb = dio->iocb;
	loff_t offset = iocb->ki_pos;
	ssize_t ret = dio->error;

	if (dops && dops->end_io)
		ret = dops->end_io(iocb, dio->size, ret, dio->flags);

	if (likely(!ret)) {
		ret = dio->size;
		 
		if (offset + ret > dio->i_size &&
		    !(dio->flags & IOMAP_DIO_WRITE))
			ret = dio->i_size - offset;
	}

	 
	if (!dio->error && dio->size && (dio->flags & IOMAP_DIO_WRITE))
		kiocb_invalidate_post_direct_write(iocb, dio->size);

	inode_dio_end(file_inode(iocb->ki_filp));

	if (ret > 0) {
		iocb->ki_pos += ret;

		 
		if (dio->flags & IOMAP_DIO_NEED_SYNC)
			ret = generic_write_sync(iocb, ret);
		if (ret > 0)
			ret += dio->done_before;
	}
	trace_iomap_dio_complete(iocb, dio->error, ret);
	kfree(dio);
	return ret;
}
EXPORT_SYMBOL_GPL(iomap_dio_complete);

static ssize_t iomap_dio_deferred_complete(void *data)
{
	return iomap_dio_complete(data);
}

static void iomap_dio_complete_work(struct work_struct *work)
{
	struct iomap_dio *dio = container_of(work, struct iomap_dio, aio.work);
	struct kiocb *iocb = dio->iocb;

	iocb->ki_complete(iocb, iomap_dio_complete(dio));
}

 
static inline void iomap_dio_set_error(struct iomap_dio *dio, int ret)
{
	cmpxchg(&dio->error, 0, ret);
}

void iomap_dio_bio_end_io(struct bio *bio)
{
	struct iomap_dio *dio = bio->bi_private;
	bool should_dirty = (dio->flags & IOMAP_DIO_DIRTY);
	struct kiocb *iocb = dio->iocb;

	if (bio->bi_status)
		iomap_dio_set_error(dio, blk_status_to_errno(bio->bi_status));
	if (!atomic_dec_and_test(&dio->ref))
		goto release_bio;

	 
	if (dio->wait_for_completion) {
		struct task_struct *waiter = dio->submit.waiter;

		WRITE_ONCE(dio->submit.waiter, NULL);
		blk_wake_io_task(waiter);
		goto release_bio;
	}

	 
	if (dio->flags & IOMAP_DIO_INLINE_COMP) {
		WRITE_ONCE(iocb->private, NULL);
		iomap_dio_complete_work(&dio->aio.work);
		goto release_bio;
	}

	 
	if (dio->flags & IOMAP_DIO_CALLER_COMP) {
		 
		iocb->private = dio;
		iocb->dio_complete = iomap_dio_deferred_complete;

		 
		iocb->ki_complete(iocb, 0);
		goto release_bio;
	}

	 
	INIT_WORK(&dio->aio.work, iomap_dio_complete_work);
	queue_work(file_inode(iocb->ki_filp)->i_sb->s_dio_done_wq,
			&dio->aio.work);
release_bio:
	if (should_dirty) {
		bio_check_pages_dirty(bio);
	} else {
		bio_release_pages(bio, false);
		bio_put(bio);
	}
}
EXPORT_SYMBOL_GPL(iomap_dio_bio_end_io);

static void iomap_dio_zero(const struct iomap_iter *iter, struct iomap_dio *dio,
		loff_t pos, unsigned len)
{
	struct inode *inode = file_inode(dio->iocb->ki_filp);
	struct page *page = ZERO_PAGE(0);
	struct bio *bio;

	bio = iomap_dio_alloc_bio(iter, dio, 1, REQ_OP_WRITE | REQ_SYNC | REQ_IDLE);
	fscrypt_set_bio_crypt_ctx(bio, inode, pos >> inode->i_blkbits,
				  GFP_KERNEL);
	bio->bi_iter.bi_sector = iomap_sector(&iter->iomap, pos);
	bio->bi_private = dio;
	bio->bi_end_io = iomap_dio_bio_end_io;

	__bio_add_page(bio, page, len, 0);
	iomap_dio_submit_bio(iter, dio, bio, pos);
}

 
static inline blk_opf_t iomap_dio_bio_opflags(struct iomap_dio *dio,
		const struct iomap *iomap, bool use_fua)
{
	blk_opf_t opflags = REQ_SYNC | REQ_IDLE;

	if (!(dio->flags & IOMAP_DIO_WRITE))
		return REQ_OP_READ;

	opflags |= REQ_OP_WRITE;
	if (use_fua)
		opflags |= REQ_FUA;
	else
		dio->flags &= ~IOMAP_DIO_WRITE_THROUGH;

	return opflags;
}

static loff_t iomap_dio_bio_iter(const struct iomap_iter *iter,
		struct iomap_dio *dio)
{
	const struct iomap *iomap = &iter->iomap;
	struct inode *inode = iter->inode;
	unsigned int fs_block_size = i_blocksize(inode), pad;
	loff_t length = iomap_length(iter);
	loff_t pos = iter->pos;
	blk_opf_t bio_opf;
	struct bio *bio;
	bool need_zeroout = false;
	bool use_fua = false;
	int nr_pages, ret = 0;
	size_t copied = 0;
	size_t orig_count;

	if ((pos | length) & (bdev_logical_block_size(iomap->bdev) - 1) ||
	    !bdev_iter_is_aligned(iomap->bdev, dio->submit.iter))
		return -EINVAL;

	if (iomap->type == IOMAP_UNWRITTEN) {
		dio->flags |= IOMAP_DIO_UNWRITTEN;
		need_zeroout = true;
	}

	if (iomap->flags & IOMAP_F_SHARED)
		dio->flags |= IOMAP_DIO_COW;

	if (iomap->flags & IOMAP_F_NEW) {
		need_zeroout = true;
	} else if (iomap->type == IOMAP_MAPPED) {
		 
		if (!(iomap->flags & (IOMAP_F_SHARED|IOMAP_F_DIRTY)) &&
		    (dio->flags & IOMAP_DIO_WRITE_THROUGH) &&
		    (bdev_fua(iomap->bdev) || !bdev_write_cache(iomap->bdev)))
			use_fua = true;
		else if (dio->flags & IOMAP_DIO_NEED_SYNC)
			dio->flags &= ~IOMAP_DIO_CALLER_COMP;
	}

	 
	orig_count = iov_iter_count(dio->submit.iter);
	iov_iter_truncate(dio->submit.iter, length);

	if (!iov_iter_count(dio->submit.iter))
		goto out;

	 
	if (need_zeroout ||
	    ((dio->flags & IOMAP_DIO_NEED_SYNC) && !use_fua) ||
	    ((dio->flags & IOMAP_DIO_WRITE) && pos >= i_size_read(inode)))
		dio->flags &= ~IOMAP_DIO_CALLER_COMP;

	 
	if (!(dio->flags & (IOMAP_DIO_INLINE_COMP|IOMAP_DIO_CALLER_COMP)))
		dio->iocb->ki_flags &= ~IOCB_HIPRI;

	if (need_zeroout) {
		 
		pad = pos & (fs_block_size - 1);
		if (pad)
			iomap_dio_zero(iter, dio, pos - pad, pad);
	}

	 
	bio_opf = iomap_dio_bio_opflags(dio, iomap, use_fua);

	nr_pages = bio_iov_vecs_to_alloc(dio->submit.iter, BIO_MAX_VECS);
	do {
		size_t n;
		if (dio->error) {
			iov_iter_revert(dio->submit.iter, copied);
			copied = ret = 0;
			goto out;
		}

		bio = iomap_dio_alloc_bio(iter, dio, nr_pages, bio_opf);
		fscrypt_set_bio_crypt_ctx(bio, inode, pos >> inode->i_blkbits,
					  GFP_KERNEL);
		bio->bi_iter.bi_sector = iomap_sector(iomap, pos);
		bio->bi_ioprio = dio->iocb->ki_ioprio;
		bio->bi_private = dio;
		bio->bi_end_io = iomap_dio_bio_end_io;

		ret = bio_iov_iter_get_pages(bio, dio->submit.iter);
		if (unlikely(ret)) {
			 
			bio_put(bio);
			goto zero_tail;
		}

		n = bio->bi_iter.bi_size;
		if (dio->flags & IOMAP_DIO_WRITE) {
			task_io_account_write(n);
		} else {
			if (dio->flags & IOMAP_DIO_DIRTY)
				bio_set_pages_dirty(bio);
		}

		dio->size += n;
		copied += n;

		nr_pages = bio_iov_vecs_to_alloc(dio->submit.iter,
						 BIO_MAX_VECS);
		 
		if (nr_pages)
			dio->iocb->ki_flags &= ~IOCB_HIPRI;
		iomap_dio_submit_bio(iter, dio, bio, pos);
		pos += n;
	} while (nr_pages);

	 
zero_tail:
	if (need_zeroout ||
	    ((dio->flags & IOMAP_DIO_WRITE) && pos >= i_size_read(inode))) {
		 
		pad = pos & (fs_block_size - 1);
		if (pad)
			iomap_dio_zero(iter, dio, pos, fs_block_size - pad);
	}
out:
	 
	iov_iter_reexpand(dio->submit.iter, orig_count - copied);
	if (copied)
		return copied;
	return ret;
}

static loff_t iomap_dio_hole_iter(const struct iomap_iter *iter,
		struct iomap_dio *dio)
{
	loff_t length = iov_iter_zero(iomap_length(iter), dio->submit.iter);

	dio->size += length;
	if (!length)
		return -EFAULT;
	return length;
}

static loff_t iomap_dio_inline_iter(const struct iomap_iter *iomi,
		struct iomap_dio *dio)
{
	const struct iomap *iomap = &iomi->iomap;
	struct iov_iter *iter = dio->submit.iter;
	void *inline_data = iomap_inline_data(iomap, iomi->pos);
	loff_t length = iomap_length(iomi);
	loff_t pos = iomi->pos;
	size_t copied;

	if (WARN_ON_ONCE(!iomap_inline_data_valid(iomap)))
		return -EIO;

	if (dio->flags & IOMAP_DIO_WRITE) {
		loff_t size = iomi->inode->i_size;

		if (pos > size)
			memset(iomap_inline_data(iomap, size), 0, pos - size);
		copied = copy_from_iter(inline_data, length, iter);
		if (copied) {
			if (pos + copied > size)
				i_size_write(iomi->inode, pos + copied);
			mark_inode_dirty(iomi->inode);
		}
	} else {
		copied = copy_to_iter(inline_data, length, iter);
	}
	dio->size += copied;
	if (!copied)
		return -EFAULT;
	return copied;
}

static loff_t iomap_dio_iter(const struct iomap_iter *iter,
		struct iomap_dio *dio)
{
	switch (iter->iomap.type) {
	case IOMAP_HOLE:
		if (WARN_ON_ONCE(dio->flags & IOMAP_DIO_WRITE))
			return -EIO;
		return iomap_dio_hole_iter(iter, dio);
	case IOMAP_UNWRITTEN:
		if (!(dio->flags & IOMAP_DIO_WRITE))
			return iomap_dio_hole_iter(iter, dio);
		return iomap_dio_bio_iter(iter, dio);
	case IOMAP_MAPPED:
		return iomap_dio_bio_iter(iter, dio);
	case IOMAP_INLINE:
		return iomap_dio_inline_iter(iter, dio);
	case IOMAP_DELALLOC:
		 
		pr_warn_ratelimited("Direct I/O collision with buffered writes! File: %pD4 Comm: %.20s\n",
				    dio->iocb->ki_filp, current->comm);
		return -EIO;
	default:
		WARN_ON_ONCE(1);
		return -EIO;
	}
}

 
struct iomap_dio *
__iomap_dio_rw(struct kiocb *iocb, struct iov_iter *iter,
		const struct iomap_ops *ops, const struct iomap_dio_ops *dops,
		unsigned int dio_flags, void *private, size_t done_before)
{
	struct inode *inode = file_inode(iocb->ki_filp);
	struct iomap_iter iomi = {
		.inode		= inode,
		.pos		= iocb->ki_pos,
		.len		= iov_iter_count(iter),
		.flags		= IOMAP_DIRECT,
		.private	= private,
	};
	bool wait_for_completion =
		is_sync_kiocb(iocb) || (dio_flags & IOMAP_DIO_FORCE_WAIT);
	struct blk_plug plug;
	struct iomap_dio *dio;
	loff_t ret = 0;

	trace_iomap_dio_rw_begin(iocb, iter, dio_flags, done_before);

	if (!iomi.len)
		return NULL;

	dio = kmalloc(sizeof(*dio), GFP_KERNEL);
	if (!dio)
		return ERR_PTR(-ENOMEM);

	dio->iocb = iocb;
	atomic_set(&dio->ref, 1);
	dio->size = 0;
	dio->i_size = i_size_read(inode);
	dio->dops = dops;
	dio->error = 0;
	dio->flags = 0;
	dio->done_before = done_before;

	dio->submit.iter = iter;
	dio->submit.waiter = current;

	if (iocb->ki_flags & IOCB_NOWAIT)
		iomi.flags |= IOMAP_NOWAIT;

	if (iov_iter_rw(iter) == READ) {
		 
		dio->flags |= IOMAP_DIO_INLINE_COMP;

		if (iomi.pos >= dio->i_size)
			goto out_free_dio;

		if (user_backed_iter(iter))
			dio->flags |= IOMAP_DIO_DIRTY;

		ret = kiocb_write_and_wait(iocb, iomi.len);
		if (ret)
			goto out_free_dio;
	} else {
		iomi.flags |= IOMAP_WRITE;
		dio->flags |= IOMAP_DIO_WRITE;

		 
		if (iocb->ki_flags & IOCB_DIO_CALLER_COMP)
			dio->flags |= IOMAP_DIO_CALLER_COMP;

		if (dio_flags & IOMAP_DIO_OVERWRITE_ONLY) {
			ret = -EAGAIN;
			if (iomi.pos >= dio->i_size ||
			    iomi.pos + iomi.len > dio->i_size)
				goto out_free_dio;
			iomi.flags |= IOMAP_OVERWRITE_ONLY;
		}

		 
		if (iocb_is_dsync(iocb)) {
			dio->flags |= IOMAP_DIO_NEED_SYNC;

		        
			if (!(iocb->ki_flags & IOCB_SYNC))
				dio->flags |= IOMAP_DIO_WRITE_THROUGH;
		}

		 
		ret = kiocb_invalidate_pages(iocb, iomi.len);
		if (ret) {
			if (ret != -EAGAIN) {
				trace_iomap_dio_invalidate_fail(inode, iomi.pos,
								iomi.len);
				ret = -ENOTBLK;
			}
			goto out_free_dio;
		}

		if (!wait_for_completion && !inode->i_sb->s_dio_done_wq) {
			ret = sb_init_dio_done_wq(inode->i_sb);
			if (ret < 0)
				goto out_free_dio;
		}
	}

	inode_dio_begin(inode);

	blk_start_plug(&plug);
	while ((ret = iomap_iter(&iomi, ops)) > 0) {
		iomi.processed = iomap_dio_iter(&iomi, dio);

		 
		iocb->ki_flags &= ~IOCB_HIPRI;
	}

	blk_finish_plug(&plug);

	 
	if (iov_iter_rw(iter) == READ && iomi.pos >= dio->i_size)
		iov_iter_revert(iter, iomi.pos - dio->i_size);

	if (ret == -EFAULT && dio->size && (dio_flags & IOMAP_DIO_PARTIAL)) {
		if (!(iocb->ki_flags & IOCB_NOWAIT))
			wait_for_completion = true;
		ret = 0;
	}

	 
	if (ret == -ENOTBLK) {
		wait_for_completion = true;
		ret = 0;
	}
	if (ret < 0)
		iomap_dio_set_error(dio, ret);

	 
	if (dio->flags & IOMAP_DIO_WRITE_THROUGH)
		dio->flags &= ~IOMAP_DIO_NEED_SYNC;

	 
	dio->wait_for_completion = wait_for_completion;
	if (!atomic_dec_and_test(&dio->ref)) {
		if (!wait_for_completion) {
			trace_iomap_dio_rw_queued(inode, iomi.pos, iomi.len);
			return ERR_PTR(-EIOCBQUEUED);
		}

		for (;;) {
			set_current_state(TASK_UNINTERRUPTIBLE);
			if (!READ_ONCE(dio->submit.waiter))
				break;

			blk_io_schedule();
		}
		__set_current_state(TASK_RUNNING);
	}

	return dio;

out_free_dio:
	kfree(dio);
	if (ret)
		return ERR_PTR(ret);
	return NULL;
}
EXPORT_SYMBOL_GPL(__iomap_dio_rw);

ssize_t
iomap_dio_rw(struct kiocb *iocb, struct iov_iter *iter,
		const struct iomap_ops *ops, const struct iomap_dio_ops *dops,
		unsigned int dio_flags, void *private, size_t done_before)
{
	struct iomap_dio *dio;

	dio = __iomap_dio_rw(iocb, iter, ops, dops, dio_flags, private,
			     done_before);
	if (IS_ERR_OR_NULL(dio))
		return PTR_ERR_OR_ZERO(dio);
	return iomap_dio_complete(dio);
}
EXPORT_SYMBOL_GPL(iomap_dio_rw);
