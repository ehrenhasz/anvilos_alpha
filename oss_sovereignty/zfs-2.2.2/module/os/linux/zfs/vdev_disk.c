 
 

#include <sys/zfs_context.h>
#include <sys/spa_impl.h>
#include <sys/vdev_disk.h>
#include <sys/vdev_impl.h>
#include <sys/vdev_trim.h>
#include <sys/abd.h>
#include <sys/fs/zfs.h>
#include <sys/zio.h>
#include <linux/blkpg.h>
#include <linux/msdos_fs.h>
#include <linux/vfs_compat.h>
#ifdef HAVE_LINUX_BLK_CGROUP_HEADER
#include <linux/blk-cgroup.h>
#endif

typedef struct vdev_disk {
	struct block_device		*vd_bdev;
	krwlock_t			vd_lock;
} vdev_disk_t;

 
static void *zfs_vdev_holder = VDEV_HOLDER;

 
static uint_t zfs_vdev_open_timeout_ms = 1000;

 
#define	EFI_MIN_RESV_SIZE	(16 * 1024)

 
typedef struct dio_request {
	zio_t			*dr_zio;	 
	atomic_t		dr_ref;		 
	int			dr_error;	 
	int			dr_bio_count;	 
	struct bio		*dr_bio[];	 
} dio_request_t;

 

static unsigned int zfs_vdev_failfast_mask = 1;

#ifdef HAVE_BLK_MODE_T
static blk_mode_t
#else
static fmode_t
#endif
vdev_bdev_mode(spa_mode_t spa_mode)
{
#ifdef HAVE_BLK_MODE_T
	blk_mode_t mode = 0;

	if (spa_mode & SPA_MODE_READ)
		mode |= BLK_OPEN_READ;

	if (spa_mode & SPA_MODE_WRITE)
		mode |= BLK_OPEN_WRITE;
#else
	fmode_t mode = 0;

	if (spa_mode & SPA_MODE_READ)
		mode |= FMODE_READ;

	if (spa_mode & SPA_MODE_WRITE)
		mode |= FMODE_WRITE;
#endif

	return (mode);
}

 
static uint64_t
bdev_capacity(struct block_device *bdev)
{
	return (i_size_read(bdev->bd_inode));
}

#if !defined(HAVE_BDEV_WHOLE)
static inline struct block_device *
bdev_whole(struct block_device *bdev)
{
	return (bdev->bd_contains);
}
#endif

#if defined(HAVE_BDEVNAME)
#define	vdev_bdevname(bdev, name)	bdevname(bdev, name)
#else
static inline void
vdev_bdevname(struct block_device *bdev, char *name)
{
	snprintf(name, BDEVNAME_SIZE, "%pg", bdev);
}
#endif

 
static uint64_t
bdev_max_capacity(struct block_device *bdev, uint64_t wholedisk)
{
	uint64_t psize;
	int64_t available;

	if (wholedisk && bdev != bdev_whole(bdev)) {
		 
		available = i_size_read(bdev_whole(bdev)->bd_inode) -
		    ((EFI_MIN_RESV_SIZE + NEW_START_BLOCK +
		    PARTITION_END_ALIGNMENT) << SECTOR_BITS);
		psize = MAX(available, bdev_capacity(bdev));
	} else {
		psize = bdev_capacity(bdev);
	}

	return (psize);
}

static void
vdev_disk_error(zio_t *zio)
{
	 
	printk(KERN_WARNING "zio pool=%s vdev=%s error=%d type=%d "
	    "offset=%llu size=%llu flags=%llu\n", spa_name(zio->io_spa),
	    zio->io_vd->vdev_path, zio->io_error, zio->io_type,
	    (u_longlong_t)zio->io_offset, (u_longlong_t)zio->io_size,
	    zio->io_flags);
}

static void
vdev_disk_kobj_evt_post(vdev_t *v)
{
	vdev_disk_t *vd = v->vdev_tsd;
	if (vd && vd->vd_bdev) {
		spl_signal_kobj_evt(vd->vd_bdev);
	} else {
		vdev_dbgmsg(v, "vdev_disk_t is NULL for VDEV:%s\n",
		    v->vdev_path);
	}
}

#if !defined(HAVE_BLKDEV_GET_BY_PATH_4ARG)
 
struct blk_holder_ops {};
#endif

static struct block_device *
vdev_blkdev_get_by_path(const char *path, spa_mode_t mode, void *holder,
    const struct blk_holder_ops *hops)
{
#ifdef HAVE_BLKDEV_GET_BY_PATH_4ARG
	return (blkdev_get_by_path(path,
	    vdev_bdev_mode(mode) | BLK_OPEN_EXCL, holder, hops));
#else
	return (blkdev_get_by_path(path,
	    vdev_bdev_mode(mode) | FMODE_EXCL, holder));
#endif
}

static void
vdev_blkdev_put(struct block_device *bdev, spa_mode_t mode, void *holder)
{
#ifdef HAVE_BLKDEV_PUT_HOLDER
	return (blkdev_put(bdev, holder));
#else
	return (blkdev_put(bdev, vdev_bdev_mode(mode) | FMODE_EXCL));
#endif
}

static int
vdev_disk_open(vdev_t *v, uint64_t *psize, uint64_t *max_psize,
    uint64_t *logical_ashift, uint64_t *physical_ashift)
{
	struct block_device *bdev;
#ifdef HAVE_BLK_MODE_T
	blk_mode_t mode = vdev_bdev_mode(spa_mode(v->vdev_spa));
#else
	fmode_t mode = vdev_bdev_mode(spa_mode(v->vdev_spa));
#endif
	hrtime_t timeout = MSEC2NSEC(zfs_vdev_open_timeout_ms);
	vdev_disk_t *vd;

	 
	if (v->vdev_path == NULL || v->vdev_path[0] != '/') {
		v->vdev_stat.vs_aux = VDEV_AUX_BAD_LABEL;
		vdev_dbgmsg(v, "invalid vdev_path");
		return (SET_ERROR(EINVAL));
	}

	 
	vd = v->vdev_tsd;
	if (vd) {
		char disk_name[BDEVNAME_SIZE + 6] = "/dev/";
		boolean_t reread_part = B_FALSE;

		rw_enter(&vd->vd_lock, RW_WRITER);
		bdev = vd->vd_bdev;
		vd->vd_bdev = NULL;

		if (bdev) {
			if (v->vdev_expanding && bdev != bdev_whole(bdev)) {
				vdev_bdevname(bdev_whole(bdev), disk_name + 5);
				 
				if (v->vdev_psize == bdev_capacity(bdev))
					reread_part = B_TRUE;
			}

			vdev_blkdev_put(bdev, mode, zfs_vdev_holder);
		}

		if (reread_part) {
			bdev = vdev_blkdev_get_by_path(disk_name, mode,
			    zfs_vdev_holder, NULL);
			if (!IS_ERR(bdev)) {
				int error = vdev_bdev_reread_part(bdev);
				vdev_blkdev_put(bdev, mode, zfs_vdev_holder);
				if (error == 0) {
					timeout = MSEC2NSEC(
					    zfs_vdev_open_timeout_ms * 2);
				}
			}
		}
	} else {
		vd = kmem_zalloc(sizeof (vdev_disk_t), KM_SLEEP);

		rw_init(&vd->vd_lock, NULL, RW_DEFAULT, NULL);
		rw_enter(&vd->vd_lock, RW_WRITER);
	}

	 
	hrtime_t start = gethrtime();
	bdev = ERR_PTR(-ENXIO);
	while (IS_ERR(bdev) && ((gethrtime() - start) < timeout)) {
		bdev = vdev_blkdev_get_by_path(v->vdev_path, mode,
		    zfs_vdev_holder, NULL);
		if (unlikely(PTR_ERR(bdev) == -ENOENT)) {
			 
			if (v->vdev_removed)
				break;

			schedule_timeout(MSEC_TO_TICK(10));
		} else if (unlikely(PTR_ERR(bdev) == -ERESTARTSYS)) {
			timeout = MSEC2NSEC(zfs_vdev_open_timeout_ms * 10);
			continue;
		} else if (IS_ERR(bdev)) {
			break;
		}
	}

	if (IS_ERR(bdev)) {
		int error = -PTR_ERR(bdev);
		vdev_dbgmsg(v, "open error=%d timeout=%llu/%llu", error,
		    (u_longlong_t)(gethrtime() - start),
		    (u_longlong_t)timeout);
		vd->vd_bdev = NULL;
		v->vdev_tsd = vd;
		rw_exit(&vd->vd_lock);
		return (SET_ERROR(error));
	} else {
		vd->vd_bdev = bdev;
		v->vdev_tsd = vd;
		rw_exit(&vd->vd_lock);
	}

	 
	int physical_block_size = bdev_physical_block_size(vd->vd_bdev);

	 
	int logical_block_size = bdev_logical_block_size(vd->vd_bdev);

	 
	v->vdev_nowritecache = B_FALSE;

	 
	v->vdev_has_trim = bdev_discard_supported(vd->vd_bdev);

	 
	v->vdev_has_securetrim = bdev_secure_discard_supported(vd->vd_bdev);

	 
	v->vdev_nonrot = blk_queue_nonrot(bdev_get_queue(vd->vd_bdev));

	 
	*psize = bdev_capacity(vd->vd_bdev);

	 
	*max_psize = bdev_max_capacity(vd->vd_bdev, v->vdev_wholedisk);

	 
	*physical_ashift = highbit64(MAX(physical_block_size,
	    SPA_MINBLOCKSIZE)) - 1;

	*logical_ashift = highbit64(MAX(logical_block_size,
	    SPA_MINBLOCKSIZE)) - 1;

	return (0);
}

static void
vdev_disk_close(vdev_t *v)
{
	vdev_disk_t *vd = v->vdev_tsd;

	if (v->vdev_reopening || vd == NULL)
		return;

	if (vd->vd_bdev != NULL) {
		vdev_blkdev_put(vd->vd_bdev, spa_mode(v->vdev_spa),
		    zfs_vdev_holder);
	}

	rw_destroy(&vd->vd_lock);
	kmem_free(vd, sizeof (vdev_disk_t));
	v->vdev_tsd = NULL;
}

static dio_request_t *
vdev_disk_dio_alloc(int bio_count)
{
	dio_request_t *dr = kmem_zalloc(sizeof (dio_request_t) +
	    sizeof (struct bio *) * bio_count, KM_SLEEP);
	atomic_set(&dr->dr_ref, 0);
	dr->dr_bio_count = bio_count;
	dr->dr_error = 0;

	for (int i = 0; i < dr->dr_bio_count; i++)
		dr->dr_bio[i] = NULL;

	return (dr);
}

static void
vdev_disk_dio_free(dio_request_t *dr)
{
	int i;

	for (i = 0; i < dr->dr_bio_count; i++)
		if (dr->dr_bio[i])
			bio_put(dr->dr_bio[i]);

	kmem_free(dr, sizeof (dio_request_t) +
	    sizeof (struct bio *) * dr->dr_bio_count);
}

static void
vdev_disk_dio_get(dio_request_t *dr)
{
	atomic_inc(&dr->dr_ref);
}

static void
vdev_disk_dio_put(dio_request_t *dr)
{
	int rc = atomic_dec_return(&dr->dr_ref);

	 
	if (rc == 0) {
		zio_t *zio = dr->dr_zio;
		int error = dr->dr_error;

		vdev_disk_dio_free(dr);

		if (zio) {
			zio->io_error = error;
			ASSERT3S(zio->io_error, >=, 0);
			if (zio->io_error)
				vdev_disk_error(zio);

			zio_delay_interrupt(zio);
		}
	}
}

BIO_END_IO_PROTO(vdev_disk_physio_completion, bio, error)
{
	dio_request_t *dr = bio->bi_private;

	if (dr->dr_error == 0) {
#ifdef HAVE_1ARG_BIO_END_IO_T
		dr->dr_error = BIO_END_IO_ERROR(bio);
#else
		if (error)
			dr->dr_error = -(error);
		else if (!test_bit(BIO_UPTODATE, &bio->bi_flags))
			dr->dr_error = EIO;
#endif
	}

	 
	vdev_disk_dio_put(dr);
}

static inline void
vdev_submit_bio_impl(struct bio *bio)
{
#ifdef HAVE_1ARG_SUBMIT_BIO
	(void) submit_bio(bio);
#else
	(void) submit_bio(bio_data_dir(bio), bio);
#endif
}

 
#if defined(CONFIG_ARM64) && \
    defined(CONFIG_PREEMPTION) && \
    defined(CONFIG_BLK_CGROUP)
#define	preempt_schedule_notrace(x) preempt_schedule(x)
#endif

 
#if !defined(HAVE_BIO_ALLOC_4ARG)

#ifdef HAVE_BIO_SET_DEV
#if defined(CONFIG_BLK_CGROUP) && defined(HAVE_BIO_SET_DEV_GPL_ONLY)
 
#if defined(HAVE_BLKG_TRYGET_GPL_ONLY) || !defined(HAVE_BLKG_TRYGET)
static inline bool
vdev_blkg_tryget(struct blkcg_gq *blkg)
{
	struct percpu_ref *ref = &blkg->refcnt;
	unsigned long __percpu *count;
	bool rc;

	rcu_read_lock_sched();

	if (__ref_is_percpu(ref, &count)) {
		this_cpu_inc(*count);
		rc = true;
	} else {
#ifdef ZFS_PERCPU_REF_COUNT_IN_DATA
		rc = atomic_long_inc_not_zero(&ref->data->count);
#else
		rc = atomic_long_inc_not_zero(&ref->count);
#endif
	}

	rcu_read_unlock_sched();

	return (rc);
}
#else
#define	vdev_blkg_tryget(bg)	blkg_tryget(bg)
#endif
#ifdef HAVE_BIO_SET_DEV_MACRO
 
static inline void
vdev_bio_associate_blkg(struct bio *bio)
{
#if defined(HAVE_BIO_BDEV_DISK)
	struct request_queue *q = bio->bi_bdev->bd_disk->queue;
#else
	struct request_queue *q = bio->bi_disk->queue;
#endif

	ASSERT3P(q, !=, NULL);
	ASSERT3P(bio->bi_blkg, ==, NULL);

	if (q->root_blkg && vdev_blkg_tryget(q->root_blkg))
		bio->bi_blkg = q->root_blkg;
}

#define	bio_associate_blkg vdev_bio_associate_blkg
#else
static inline void
vdev_bio_set_dev(struct bio *bio, struct block_device *bdev)
{
#if defined(HAVE_BIO_BDEV_DISK)
	struct request_queue *q = bdev->bd_disk->queue;
#else
	struct request_queue *q = bio->bi_disk->queue;
#endif
	bio_clear_flag(bio, BIO_REMAPPED);
	if (bio->bi_bdev != bdev)
		bio_clear_flag(bio, BIO_THROTTLED);
	bio->bi_bdev = bdev;

	ASSERT3P(q, !=, NULL);
	ASSERT3P(bio->bi_blkg, ==, NULL);

	if (q->root_blkg && vdev_blkg_tryget(q->root_blkg))
		bio->bi_blkg = q->root_blkg;
}
#define	bio_set_dev		vdev_bio_set_dev
#endif
#endif
#else
 
static inline void
bio_set_dev(struct bio *bio, struct block_device *bdev)
{
	bio->bi_bdev = bdev;
}
#endif  
#endif  

static inline void
vdev_submit_bio(struct bio *bio)
{
	struct bio_list *bio_list = current->bio_list;
	current->bio_list = NULL;
	vdev_submit_bio_impl(bio);
	current->bio_list = bio_list;
}

static inline struct bio *
vdev_bio_alloc(struct block_device *bdev, gfp_t gfp_mask,
    unsigned short nr_vecs)
{
	struct bio *bio;

#ifdef HAVE_BIO_ALLOC_4ARG
	bio = bio_alloc(bdev, nr_vecs, 0, gfp_mask);
#else
	bio = bio_alloc(gfp_mask, nr_vecs);
	if (likely(bio != NULL))
		bio_set_dev(bio, bdev);
#endif

	return (bio);
}

static inline unsigned int
vdev_bio_max_segs(zio_t *zio, int bio_size, uint64_t abd_offset)
{
	unsigned long nr_segs = abd_nr_pages_off(zio->io_abd,
	    bio_size, abd_offset);

#ifdef HAVE_BIO_MAX_SEGS
	return (bio_max_segs(nr_segs));
#else
	return (MIN(nr_segs, BIO_MAX_PAGES));
#endif
}

static int
__vdev_disk_physio(struct block_device *bdev, zio_t *zio,
    size_t io_size, uint64_t io_offset, int rw, int flags)
{
	dio_request_t *dr;
	uint64_t abd_offset;
	uint64_t bio_offset;
	int bio_size;
	int bio_count = 16;
	int error = 0;
	struct blk_plug plug;
	unsigned short nr_vecs;

	 
	if (io_offset + io_size > bdev->bd_inode->i_size) {
		vdev_dbgmsg(zio->io_vd,
		    "Illegal access %llu size %llu, device size %llu",
		    (u_longlong_t)io_offset,
		    (u_longlong_t)io_size,
		    (u_longlong_t)i_size_read(bdev->bd_inode));
		return (SET_ERROR(EIO));
	}

retry:
	dr = vdev_disk_dio_alloc(bio_count);

	if (!(zio->io_flags & (ZIO_FLAG_IO_RETRY | ZIO_FLAG_TRYHARD)) &&
	    zio->io_vd->vdev_failfast == B_TRUE) {
		bio_set_flags_failfast(bdev, &flags, zfs_vdev_failfast_mask & 1,
		    zfs_vdev_failfast_mask & 2, zfs_vdev_failfast_mask & 4);
	}

	dr->dr_zio = zio;

	 

	abd_offset = 0;
	bio_offset = io_offset;
	bio_size = io_size;
	for (int i = 0; i <= dr->dr_bio_count; i++) {

		 
		if (bio_size <= 0)
			break;

		 
		if (dr->dr_bio_count == i) {
			vdev_disk_dio_free(dr);
			bio_count *= 2;
			goto retry;
		}

		nr_vecs = vdev_bio_max_segs(zio, bio_size, abd_offset);
		dr->dr_bio[i] = vdev_bio_alloc(bdev, GFP_NOIO, nr_vecs);
		if (unlikely(dr->dr_bio[i] == NULL)) {
			vdev_disk_dio_free(dr);
			return (SET_ERROR(ENOMEM));
		}

		 
		vdev_disk_dio_get(dr);

		BIO_BI_SECTOR(dr->dr_bio[i]) = bio_offset >> 9;
		dr->dr_bio[i]->bi_end_io = vdev_disk_physio_completion;
		dr->dr_bio[i]->bi_private = dr;
		bio_set_op_attrs(dr->dr_bio[i], rw, flags);

		 
		bio_size = abd_bio_map_off(dr->dr_bio[i], zio->io_abd,
		    bio_size, abd_offset);

		 
		abd_offset += BIO_BI_SIZE(dr->dr_bio[i]);
		bio_offset += BIO_BI_SIZE(dr->dr_bio[i]);
	}

	 
	vdev_disk_dio_get(dr);

	if (dr->dr_bio_count > 1)
		blk_start_plug(&plug);

	 
	for (int i = 0; i < dr->dr_bio_count; i++) {
		if (dr->dr_bio[i])
			vdev_submit_bio(dr->dr_bio[i]);
	}

	if (dr->dr_bio_count > 1)
		blk_finish_plug(&plug);

	vdev_disk_dio_put(dr);

	return (error);
}

BIO_END_IO_PROTO(vdev_disk_io_flush_completion, bio, error)
{
	zio_t *zio = bio->bi_private;
#ifdef HAVE_1ARG_BIO_END_IO_T
	zio->io_error = BIO_END_IO_ERROR(bio);
#else
	zio->io_error = -error;
#endif

	if (zio->io_error && (zio->io_error == EOPNOTSUPP))
		zio->io_vd->vdev_nowritecache = B_TRUE;

	bio_put(bio);
	ASSERT3S(zio->io_error, >=, 0);
	if (zio->io_error)
		vdev_disk_error(zio);
	zio_interrupt(zio);
}

static int
vdev_disk_io_flush(struct block_device *bdev, zio_t *zio)
{
	struct request_queue *q;
	struct bio *bio;

	q = bdev_get_queue(bdev);
	if (!q)
		return (SET_ERROR(ENXIO));

	bio = vdev_bio_alloc(bdev, GFP_NOIO, 0);
	if (unlikely(bio == NULL))
		return (SET_ERROR(ENOMEM));

	bio->bi_end_io = vdev_disk_io_flush_completion;
	bio->bi_private = zio;
	bio_set_flush(bio);
	vdev_submit_bio(bio);
	invalidate_bdev(bdev);

	return (0);
}

static int
vdev_disk_io_trim(zio_t *zio)
{
	vdev_t *v = zio->io_vd;
	vdev_disk_t *vd = v->vdev_tsd;

#if defined(HAVE_BLKDEV_ISSUE_SECURE_ERASE)
	if (zio->io_trim_flags & ZIO_TRIM_SECURE) {
		return (-blkdev_issue_secure_erase(vd->vd_bdev,
		    zio->io_offset >> 9, zio->io_size >> 9, GFP_NOFS));
	} else {
		return (-blkdev_issue_discard(vd->vd_bdev,
		    zio->io_offset >> 9, zio->io_size >> 9, GFP_NOFS));
	}
#elif defined(HAVE_BLKDEV_ISSUE_DISCARD)
	unsigned long trim_flags = 0;
#if defined(BLKDEV_DISCARD_SECURE)
	if (zio->io_trim_flags & ZIO_TRIM_SECURE)
		trim_flags |= BLKDEV_DISCARD_SECURE;
#endif
	return (-blkdev_issue_discard(vd->vd_bdev,
	    zio->io_offset >> 9, zio->io_size >> 9, GFP_NOFS, trim_flags));
#else
#error "Unsupported kernel"
#endif
}

static void
vdev_disk_io_start(zio_t *zio)
{
	vdev_t *v = zio->io_vd;
	vdev_disk_t *vd = v->vdev_tsd;
	int rw, error;

	 
	if (vd == NULL) {
		zio->io_error = ENXIO;
		zio_interrupt(zio);
		return;
	}

	rw_enter(&vd->vd_lock, RW_READER);

	 
	if (vd->vd_bdev == NULL) {
		rw_exit(&vd->vd_lock);
		zio->io_error = ENXIO;
		zio_interrupt(zio);
		return;
	}

	switch (zio->io_type) {
	case ZIO_TYPE_IOCTL:

		if (!vdev_readable(v)) {
			rw_exit(&vd->vd_lock);
			zio->io_error = SET_ERROR(ENXIO);
			zio_interrupt(zio);
			return;
		}

		switch (zio->io_cmd) {
		case DKIOCFLUSHWRITECACHE:

			if (zfs_nocacheflush)
				break;

			if (v->vdev_nowritecache) {
				zio->io_error = SET_ERROR(ENOTSUP);
				break;
			}

			error = vdev_disk_io_flush(vd->vd_bdev, zio);
			if (error == 0) {
				rw_exit(&vd->vd_lock);
				return;
			}

			zio->io_error = error;

			break;

		default:
			zio->io_error = SET_ERROR(ENOTSUP);
		}

		rw_exit(&vd->vd_lock);
		zio_execute(zio);
		return;
	case ZIO_TYPE_WRITE:
		rw = WRITE;
		break;

	case ZIO_TYPE_READ:
		rw = READ;
		break;

	case ZIO_TYPE_TRIM:
		zio->io_error = vdev_disk_io_trim(zio);
		rw_exit(&vd->vd_lock);
		zio_interrupt(zio);
		return;

	default:
		rw_exit(&vd->vd_lock);
		zio->io_error = SET_ERROR(ENOTSUP);
		zio_interrupt(zio);
		return;
	}

	zio->io_target_timestamp = zio_handle_io_delay(zio);
	error = __vdev_disk_physio(vd->vd_bdev, zio,
	    zio->io_size, zio->io_offset, rw, 0);
	rw_exit(&vd->vd_lock);

	if (error) {
		zio->io_error = error;
		zio_interrupt(zio);
		return;
	}
}

static void
vdev_disk_io_done(zio_t *zio)
{
	 
	if (zio->io_error == EIO) {
		vdev_t *v = zio->io_vd;
		vdev_disk_t *vd = v->vdev_tsd;

		if (!zfs_check_disk_status(vd->vd_bdev)) {
			invalidate_bdev(vd->vd_bdev);
			v->vdev_remove_wanted = B_TRUE;
			spa_async_request(zio->io_spa, SPA_ASYNC_REMOVE);
		}
	}
}

static void
vdev_disk_hold(vdev_t *vd)
{
	ASSERT(spa_config_held(vd->vdev_spa, SCL_STATE, RW_WRITER));

	 
	if (vd->vdev_path == NULL || vd->vdev_path[0] != '/')
		return;

	 
	if (vd->vdev_tsd != NULL)
		return;

}

static void
vdev_disk_rele(vdev_t *vd)
{
	ASSERT(spa_config_held(vd->vdev_spa, SCL_STATE, RW_WRITER));

	 
}

vdev_ops_t vdev_disk_ops = {
	.vdev_op_init = NULL,
	.vdev_op_fini = NULL,
	.vdev_op_open = vdev_disk_open,
	.vdev_op_close = vdev_disk_close,
	.vdev_op_asize = vdev_default_asize,
	.vdev_op_min_asize = vdev_default_min_asize,
	.vdev_op_min_alloc = NULL,
	.vdev_op_io_start = vdev_disk_io_start,
	.vdev_op_io_done = vdev_disk_io_done,
	.vdev_op_state_change = NULL,
	.vdev_op_need_resilver = NULL,
	.vdev_op_hold = vdev_disk_hold,
	.vdev_op_rele = vdev_disk_rele,
	.vdev_op_remap = NULL,
	.vdev_op_xlate = vdev_default_xlate,
	.vdev_op_rebuild_asize = NULL,
	.vdev_op_metaslab_init = NULL,
	.vdev_op_config_generate = NULL,
	.vdev_op_nparity = NULL,
	.vdev_op_ndisks = NULL,
	.vdev_op_type = VDEV_TYPE_DISK,		 
	.vdev_op_leaf = B_TRUE,			 
	.vdev_op_kobj_evt_post = vdev_disk_kobj_evt_post
};

 
static int
param_set_vdev_scheduler(const char *val, zfs_kernel_param_t *kp)
{
	int error = param_set_charp(val, kp);
	if (error == 0) {
		printk(KERN_INFO "The 'zfs_vdev_scheduler' module option "
		    "is not supported.\n");
	}

	return (error);
}

static const char *zfs_vdev_scheduler = "unused";
module_param_call(zfs_vdev_scheduler, param_set_vdev_scheduler,
    param_get_charp, &zfs_vdev_scheduler, 0644);
MODULE_PARM_DESC(zfs_vdev_scheduler, "I/O scheduler");

int
param_set_min_auto_ashift(const char *buf, zfs_kernel_param_t *kp)
{
	uint_t val;
	int error;

	error = kstrtouint(buf, 0, &val);
	if (error < 0)
		return (SET_ERROR(error));

	if (val < ASHIFT_MIN || val > zfs_vdev_max_auto_ashift)
		return (SET_ERROR(-EINVAL));

	error = param_set_uint(buf, kp);
	if (error < 0)
		return (SET_ERROR(error));

	return (0);
}

int
param_set_max_auto_ashift(const char *buf, zfs_kernel_param_t *kp)
{
	uint_t val;
	int error;

	error = kstrtouint(buf, 0, &val);
	if (error < 0)
		return (SET_ERROR(error));

	if (val > ASHIFT_MAX || val < zfs_vdev_min_auto_ashift)
		return (SET_ERROR(-EINVAL));

	error = param_set_uint(buf, kp);
	if (error < 0)
		return (SET_ERROR(error));

	return (0);
}

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, open_timeout_ms, UINT, ZMOD_RW,
	"Timeout before determining that a device is missing");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, failfast_mask, UINT, ZMOD_RW,
	"Defines failfast mask: 1 - device, 2 - transport, 4 - driver");
