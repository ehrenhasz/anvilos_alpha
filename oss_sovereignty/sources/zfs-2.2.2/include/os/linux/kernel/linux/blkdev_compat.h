



#ifndef _ZFS_BLKDEV_H
#define	_ZFS_BLKDEV_H

#include <linux/blkdev.h>
#include <linux/backing-dev.h>
#include <linux/hdreg.h>
#include <linux/major.h>
#include <linux/msdos_fs.h>	
#include <linux/bio.h>

#ifdef HAVE_BLK_MQ
#include <linux/blk-mq.h>
#endif

#ifndef HAVE_BLK_QUEUE_FLAG_SET
static inline void
blk_queue_flag_set(unsigned int flag, struct request_queue *q)
{
	queue_flag_set(flag, q);
}
#endif

#ifndef HAVE_BLK_QUEUE_FLAG_CLEAR
static inline void
blk_queue_flag_clear(unsigned int flag, struct request_queue *q)
{
	queue_flag_clear(flag, q);
}
#endif


static inline void
blk_queue_set_write_cache(struct request_queue *q, bool wc, bool fua)
{
#if defined(HAVE_BLK_QUEUE_WRITE_CACHE_GPL_ONLY)
	if (wc)
		blk_queue_flag_set(QUEUE_FLAG_WC, q);
	else
		blk_queue_flag_clear(QUEUE_FLAG_WC, q);
	if (fua)
		blk_queue_flag_set(QUEUE_FLAG_FUA, q);
	else
		blk_queue_flag_clear(QUEUE_FLAG_FUA, q);
#elif defined(HAVE_BLK_QUEUE_WRITE_CACHE)
	blk_queue_write_cache(q, wc, fua);
#elif defined(HAVE_BLK_QUEUE_FLUSH_GPL_ONLY)
	if (wc)
		q->flush_flags |= REQ_FLUSH;
	if (fua)
		q->flush_flags |= REQ_FUA;
#elif defined(HAVE_BLK_QUEUE_FLUSH)
	blk_queue_flush(q, (wc ? REQ_FLUSH : 0) | (fua ? REQ_FUA : 0));
#else
#error "Unsupported kernel"
#endif
}

static inline void
blk_queue_set_read_ahead(struct request_queue *q, unsigned long ra_pages)
{
#if !defined(HAVE_BLK_QUEUE_UPDATE_READAHEAD) && \
	!defined(HAVE_DISK_UPDATE_READAHEAD)
#ifdef HAVE_BLK_QUEUE_BDI_DYNAMIC
	q->backing_dev_info->ra_pages = ra_pages;
#else
	q->backing_dev_info.ra_pages = ra_pages;
#endif
#endif
}

#ifdef HAVE_BIO_BVEC_ITER
#define	BIO_BI_SECTOR(bio)	(bio)->bi_iter.bi_sector
#define	BIO_BI_SIZE(bio)	(bio)->bi_iter.bi_size
#define	BIO_BI_IDX(bio)		(bio)->bi_iter.bi_idx
#define	BIO_BI_SKIP(bio)	(bio)->bi_iter.bi_bvec_done
#define	bio_for_each_segment4(bv, bvp, b, i)	\
	bio_for_each_segment((bv), (b), (i))
typedef struct bvec_iter bvec_iterator_t;
#else
#define	BIO_BI_SECTOR(bio)	(bio)->bi_sector
#define	BIO_BI_SIZE(bio)	(bio)->bi_size
#define	BIO_BI_IDX(bio)		(bio)->bi_idx
#define	BIO_BI_SKIP(bio)	(0)
#define	bio_for_each_segment4(bv, bvp, b, i)	\
	bio_for_each_segment((bvp), (b), (i))
typedef int bvec_iterator_t;
#endif

static inline void
bio_set_flags_failfast(struct block_device *bdev, int *flags, bool dev,
    bool transport, bool driver)
{
#ifdef CONFIG_BUG
	
	if ((MAJOR(bdev->bd_dev) == LOOP_MAJOR) ||
	    (MAJOR(bdev->bd_dev) == MD_MAJOR))
		return;

#ifdef BLOCK_EXT_MAJOR
	if (MAJOR(bdev->bd_dev) == BLOCK_EXT_MAJOR)
		return;
#endif 
#endif 

	if (dev)
		*flags |= REQ_FAILFAST_DEV;
	if (transport)
		*flags |= REQ_FAILFAST_TRANSPORT;
	if (driver)
		*flags |= REQ_FAILFAST_DRIVER;
}


#if !defined(DISK_NAME_LEN)
#define	DISK_NAME_LEN	32
#endif 

#ifdef HAVE_BIO_BI_STATUS
static inline int
bi_status_to_errno(blk_status_t status)
{
	switch (status)	{
	case BLK_STS_OK:
		return (0);
	case BLK_STS_NOTSUPP:
		return (EOPNOTSUPP);
	case BLK_STS_TIMEOUT:
		return (ETIMEDOUT);
	case BLK_STS_NOSPC:
		return (ENOSPC);
	case BLK_STS_TRANSPORT:
		return (ENOLINK);
	case BLK_STS_TARGET:
		return (EREMOTEIO);
#ifdef HAVE_BLK_STS_RESV_CONFLICT
	case BLK_STS_RESV_CONFLICT:
#else
	case BLK_STS_NEXUS:
#endif
		return (EBADE);
	case BLK_STS_MEDIUM:
		return (ENODATA);
	case BLK_STS_PROTECTION:
		return (EILSEQ);
	case BLK_STS_RESOURCE:
		return (ENOMEM);
	case BLK_STS_AGAIN:
		return (EAGAIN);
	case BLK_STS_IOERR:
		return (EIO);
	default:
		return (EIO);
	}
}

static inline blk_status_t
errno_to_bi_status(int error)
{
	switch (error) {
	case 0:
		return (BLK_STS_OK);
	case EOPNOTSUPP:
		return (BLK_STS_NOTSUPP);
	case ETIMEDOUT:
		return (BLK_STS_TIMEOUT);
	case ENOSPC:
		return (BLK_STS_NOSPC);
	case ENOLINK:
		return (BLK_STS_TRANSPORT);
	case EREMOTEIO:
		return (BLK_STS_TARGET);
	case EBADE:
#ifdef HAVE_BLK_STS_RESV_CONFLICT
		return (BLK_STS_RESV_CONFLICT);
#else
		return (BLK_STS_NEXUS);
#endif
	case ENODATA:
		return (BLK_STS_MEDIUM);
	case EILSEQ:
		return (BLK_STS_PROTECTION);
	case ENOMEM:
		return (BLK_STS_RESOURCE);
	case EAGAIN:
		return (BLK_STS_AGAIN);
	case EIO:
		return (BLK_STS_IOERR);
	default:
		return (BLK_STS_IOERR);
	}
}
#endif 


#ifdef HAVE_1ARG_BIO_END_IO_T
#ifdef HAVE_BIO_BI_STATUS
#define	BIO_END_IO_ERROR(bio)		bi_status_to_errno(bio->bi_status)
#define	BIO_END_IO_PROTO(fn, x, z)	static void fn(struct bio *x)
#define	BIO_END_IO(bio, error)		bio_set_bi_status(bio, error)
static inline void
bio_set_bi_status(struct bio *bio, int error)
{
	ASSERT3S(error, <=, 0);
	bio->bi_status = errno_to_bi_status(-error);
	bio_endio(bio);
}
#else
#define	BIO_END_IO_ERROR(bio)		(-(bio->bi_error))
#define	BIO_END_IO_PROTO(fn, x, z)	static void fn(struct bio *x)
#define	BIO_END_IO(bio, error)		bio_set_bi_error(bio, error)
static inline void
bio_set_bi_error(struct bio *bio, int error)
{
	ASSERT3S(error, <=, 0);
	bio->bi_error = error;
	bio_endio(bio);
}
#endif 

#else
#define	BIO_END_IO_PROTO(fn, x, z)	static void fn(struct bio *x, int z)
#define	BIO_END_IO(bio, error)		bio_endio(bio, error);
#endif 


static inline boolean_t
zfs_check_disk_status(struct block_device *bdev)
{
#if defined(GENHD_FL_UP)
	return (!!(bdev->bd_disk->flags & GENHD_FL_UP));
#elif defined(GD_DEAD)
	return (!test_bit(GD_DEAD, &bdev->bd_disk->state));
#else

#error "Unsupported kernel: no usable disk status check"
#endif
}


#ifdef HAVE_CHECK_DISK_CHANGE
#define	zfs_check_media_change(bdev)	check_disk_change(bdev)
#ifdef HAVE_BLKDEV_REREAD_PART
#define	vdev_bdev_reread_part(bdev)	blkdev_reread_part(bdev)
#else
#define	vdev_bdev_reread_part(bdev)	check_disk_change(bdev)
#endif 
#else
#ifdef HAVE_BDEV_CHECK_MEDIA_CHANGE
static inline int
zfs_check_media_change(struct block_device *bdev)
{
#ifdef HAVE_BLOCK_DEVICE_OPERATIONS_REVALIDATE_DISK
	struct gendisk *gd = bdev->bd_disk;
	const struct block_device_operations *bdo = gd->fops;
#endif

	if (!bdev_check_media_change(bdev))
		return (0);

#ifdef HAVE_BLOCK_DEVICE_OPERATIONS_REVALIDATE_DISK
	
	if (bdo->revalidate_disk)
		bdo->revalidate_disk(gd);
#endif

	return (0);
}
#define	vdev_bdev_reread_part(bdev)	zfs_check_media_change(bdev)
#elif defined(HAVE_DISK_CHECK_MEDIA_CHANGE)
#define	vdev_bdev_reread_part(bdev)	disk_check_media_change(bdev->bd_disk)
#define	zfs_check_media_change(bdev)	disk_check_media_change(bdev->bd_disk)
#else

#error "Unsupported kernel: no usable disk change check"
#endif 
#endif 


static inline int
vdev_lookup_bdev(const char *path, dev_t *dev)
{
#if defined(HAVE_DEVT_LOOKUP_BDEV)
	return (lookup_bdev(path, dev));
#elif defined(HAVE_1ARG_LOOKUP_BDEV)
	struct block_device *bdev = lookup_bdev(path);
	if (IS_ERR(bdev))
		return (PTR_ERR(bdev));

	*dev = bdev->bd_dev;
	bdput(bdev);

	return (0);
#elif defined(HAVE_MODE_LOOKUP_BDEV)
	struct block_device *bdev = lookup_bdev(path, FMODE_READ);
	if (IS_ERR(bdev))
		return (PTR_ERR(bdev));

	*dev = bdev->bd_dev;
	bdput(bdev);

	return (0);
#else
#error "Unsupported kernel"
#endif
}

#if defined(HAVE_BLK_MODE_T)
#define	blk_mode_is_open_write(flag)	((flag) & BLK_OPEN_WRITE)
#else
#define	blk_mode_is_open_write(flag)	((flag) & FMODE_WRITE)
#endif


#if !defined(HAVE_BIO_SET_OP_ATTRS)
static inline void
bio_set_op_attrs(struct bio *bio, unsigned rw, unsigned flags)
{
#if defined(HAVE_BIO_BI_OPF)
	bio->bi_opf = rw | flags;
#else
	bio->bi_rw |= rw | flags;
#endif 
}
#endif


static inline void
bio_set_flush(struct bio *bio)
{
#if defined(HAVE_REQ_PREFLUSH)	
	bio_set_op_attrs(bio, 0, REQ_PREFLUSH | REQ_OP_WRITE);
#elif defined(WRITE_FLUSH_FUA)	
	bio_set_op_attrs(bio, 0, WRITE_FLUSH_FUA);
#else
#error	"Allowing the build will cause bio_set_flush requests to be ignored."
#endif
}


static inline boolean_t
bio_is_flush(struct bio *bio)
{
#if defined(HAVE_REQ_OP_FLUSH) && defined(HAVE_BIO_BI_OPF)
	return ((bio_op(bio) == REQ_OP_FLUSH) || (bio->bi_opf & REQ_PREFLUSH));
#elif defined(HAVE_REQ_PREFLUSH) && defined(HAVE_BIO_BI_OPF)
	return (bio->bi_opf & REQ_PREFLUSH);
#elif defined(HAVE_REQ_PREFLUSH) && !defined(HAVE_BIO_BI_OPF)
	return (bio->bi_rw & REQ_PREFLUSH);
#elif defined(HAVE_REQ_FLUSH)
	return (bio->bi_rw & REQ_FLUSH);
#else
#error	"Unsupported kernel"
#endif
}


static inline boolean_t
bio_is_fua(struct bio *bio)
{
#if defined(HAVE_BIO_BI_OPF)
	return (bio->bi_opf & REQ_FUA);
#elif defined(REQ_FUA)
	return (bio->bi_rw & REQ_FUA);
#else
#error	"Allowing the build will cause fua requests to be ignored."
#endif
}


static inline boolean_t
bio_is_discard(struct bio *bio)
{
#if defined(HAVE_REQ_OP_DISCARD)
	return (bio_op(bio) == REQ_OP_DISCARD);
#elif defined(HAVE_REQ_DISCARD)
	return (bio->bi_rw & REQ_DISCARD);
#else
#error "Unsupported kernel"
#endif
}


static inline boolean_t
bio_is_secure_erase(struct bio *bio)
{
#if defined(HAVE_REQ_OP_SECURE_ERASE)
	return (bio_op(bio) == REQ_OP_SECURE_ERASE);
#elif defined(REQ_SECURE)
	return (bio->bi_rw & REQ_SECURE);
#else
	return (0);
#endif
}


static inline void
blk_queue_discard_granularity(struct request_queue *q, unsigned int dg)
{
	q->limits.discard_granularity = dg;
}


static inline boolean_t
bdev_discard_supported(struct block_device *bdev)
{
#if defined(HAVE_BDEV_MAX_DISCARD_SECTORS)
	return (!!bdev_max_discard_sectors(bdev));
#elif defined(HAVE_BLK_QUEUE_DISCARD)
	return (!!blk_queue_discard(bdev_get_queue(bdev)));
#else
#error "Unsupported kernel"
#endif
}


static inline boolean_t
bdev_secure_discard_supported(struct block_device *bdev)
{
#if defined(HAVE_BDEV_MAX_SECURE_ERASE_SECTORS)
	return (!!bdev_max_secure_erase_sectors(bdev));
#elif defined(HAVE_BLK_QUEUE_SECURE_ERASE)
	return (!!blk_queue_secure_erase(bdev_get_queue(bdev)));
#elif defined(HAVE_BLK_QUEUE_SECDISCARD)
	return (!!blk_queue_secdiscard(bdev_get_queue(bdev)));
#else
#error "Unsupported kernel"
#endif
}


#define	VDEV_HOLDER			((void *)0x2401de7)

static inline unsigned long
blk_generic_start_io_acct(struct request_queue *q __attribute__((unused)),
    struct gendisk *disk __attribute__((unused)),
    int rw __attribute__((unused)), struct bio *bio)
{
#if defined(HAVE_BDEV_IO_ACCT_63)
	return (bdev_start_io_acct(bio->bi_bdev, bio_op(bio),
	    jiffies));
#elif defined(HAVE_BDEV_IO_ACCT_OLD)
	return (bdev_start_io_acct(bio->bi_bdev, bio_sectors(bio),
	    bio_op(bio), jiffies));
#elif defined(HAVE_DISK_IO_ACCT)
	return (disk_start_io_acct(disk, bio_sectors(bio), bio_op(bio)));
#elif defined(HAVE_BIO_IO_ACCT)
	return (bio_start_io_acct(bio));
#elif defined(HAVE_GENERIC_IO_ACCT_3ARG)
	unsigned long start_time = jiffies;
	generic_start_io_acct(rw, bio_sectors(bio), &disk->part0);
	return (start_time);
#elif defined(HAVE_GENERIC_IO_ACCT_4ARG)
	unsigned long start_time = jiffies;
	generic_start_io_acct(q, rw, bio_sectors(bio), &disk->part0);
	return (start_time);
#else
	
	return (0);
#endif
}

static inline void
blk_generic_end_io_acct(struct request_queue *q __attribute__((unused)),
    struct gendisk *disk __attribute__((unused)),
    int rw __attribute__((unused)), struct bio *bio, unsigned long start_time)
{
#if defined(HAVE_BDEV_IO_ACCT_63)
	bdev_end_io_acct(bio->bi_bdev, bio_op(bio), bio_sectors(bio),
	    start_time);
#elif defined(HAVE_BDEV_IO_ACCT_OLD)
	bdev_end_io_acct(bio->bi_bdev, bio_op(bio), start_time);
#elif defined(HAVE_DISK_IO_ACCT)
	disk_end_io_acct(disk, bio_op(bio), start_time);
#elif defined(HAVE_BIO_IO_ACCT)
	bio_end_io_acct(bio, start_time);
#elif defined(HAVE_GENERIC_IO_ACCT_3ARG)
	generic_end_io_acct(rw, &disk->part0, start_time);
#elif defined(HAVE_GENERIC_IO_ACCT_4ARG)
	generic_end_io_acct(q, rw, &disk->part0, start_time);
#endif
}

#ifndef HAVE_SUBMIT_BIO_IN_BLOCK_DEVICE_OPERATIONS
static inline struct request_queue *
blk_generic_alloc_queue(make_request_fn make_request, int node_id)
{
#if defined(HAVE_BLK_ALLOC_QUEUE_REQUEST_FN)
	return (blk_alloc_queue(make_request, node_id));
#elif defined(HAVE_BLK_ALLOC_QUEUE_REQUEST_FN_RH)
	return (blk_alloc_queue_rh(make_request, node_id));
#else
	struct request_queue *q = blk_alloc_queue(GFP_KERNEL);
	if (q != NULL)
		blk_queue_make_request(q, make_request);

	return (q);
#endif
}
#endif 


static inline int
io_data_dir(struct bio *bio, struct request *rq)
{
#ifdef HAVE_BLK_MQ
	if (rq != NULL) {
		if (op_is_write(req_op(rq))) {
			return (WRITE);
		} else {
			return (READ);
		}
	}
#else
	ASSERT3P(rq, ==, NULL);
#endif
	return (bio_data_dir(bio));
}

static inline int
io_is_flush(struct bio *bio, struct request *rq)
{
#ifdef HAVE_BLK_MQ
	if (rq != NULL)
		return (req_op(rq) == REQ_OP_FLUSH);
#else
	ASSERT3P(rq, ==, NULL);
#endif
	return (bio_is_flush(bio));
}

static inline int
io_is_discard(struct bio *bio, struct request *rq)
{
#ifdef HAVE_BLK_MQ
	if (rq != NULL)
		return (req_op(rq) == REQ_OP_DISCARD);
#else
	ASSERT3P(rq, ==, NULL);
#endif
	return (bio_is_discard(bio));
}

static inline int
io_is_secure_erase(struct bio *bio, struct request *rq)
{
#ifdef HAVE_BLK_MQ
	if (rq != NULL)
		return (req_op(rq) == REQ_OP_SECURE_ERASE);
#else
	ASSERT3P(rq, ==, NULL);
#endif
	return (bio_is_secure_erase(bio));
}

static inline int
io_is_fua(struct bio *bio, struct request *rq)
{
#ifdef HAVE_BLK_MQ
	if (rq != NULL)
		return (rq->cmd_flags & REQ_FUA);
#else
	ASSERT3P(rq, ==, NULL);
#endif
	return (bio_is_fua(bio));
}


static inline uint64_t
io_offset(struct bio *bio, struct request *rq)
{
#ifdef HAVE_BLK_MQ
	if (rq != NULL)
		return (blk_rq_pos(rq) << 9);
#else
	ASSERT3P(rq, ==, NULL);
#endif
	return (BIO_BI_SECTOR(bio) << 9);
}

static inline uint64_t
io_size(struct bio *bio, struct request *rq)
{
#ifdef HAVE_BLK_MQ
	if (rq != NULL)
		return (blk_rq_bytes(rq));
#else
	ASSERT3P(rq, ==, NULL);
#endif
	return (BIO_BI_SIZE(bio));
}

static inline int
io_has_data(struct bio *bio, struct request *rq)
{
#ifdef HAVE_BLK_MQ
	if (rq != NULL)
		return (bio_has_data(rq->bio));
#else
	ASSERT3P(rq, ==, NULL);
#endif
	return (bio_has_data(bio));
}
#endif 
