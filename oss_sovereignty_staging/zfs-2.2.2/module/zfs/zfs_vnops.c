 

 

 
 

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/sysmacros.h>
#include <sys/vfs.h>
#include <sys/uio_impl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/zfs_dir.h>
#include <sys/zfs_acl.h>
#include <sys/zfs_ioctl.h>
#include <sys/fs/zfs.h>
#include <sys/dmu.h>
#include <sys/dmu_objset.h>
#include <sys/spa.h>
#include <sys/txg.h>
#include <sys/dbuf.h>
#include <sys/policy.h>
#include <sys/zfeature.h>
#include <sys/zfs_vnops.h>
#include <sys/zfs_quota.h>
#include <sys/zfs_vfsops.h>
#include <sys/zfs_znode.h>


static ulong_t zfs_fsync_sync_cnt = 4;

int
zfs_fsync(znode_t *zp, int syncflag, cred_t *cr)
{
	int error = 0;
	zfsvfs_t *zfsvfs = ZTOZSB(zp);

	(void) tsd_set(zfs_fsyncer_key, (void *)(uintptr_t)zfs_fsync_sync_cnt);

	if (zfsvfs->z_os->os_sync != ZFS_SYNC_DISABLED) {
		if ((error = zfs_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
			goto out;
		atomic_inc_32(&zp->z_sync_writes_cnt);
		zil_commit(zfsvfs->z_log, zp->z_id);
		atomic_dec_32(&zp->z_sync_writes_cnt);
		zfs_exit(zfsvfs, FTAG);
	}
out:
	tsd_set(zfs_fsyncer_key, NULL);

	return (error);
}


#if defined(SEEK_HOLE) && defined(SEEK_DATA)
 
static int
zfs_holey_common(znode_t *zp, ulong_t cmd, loff_t *off)
{
	zfs_locked_range_t *lr;
	uint64_t noff = (uint64_t)*off;  
	uint64_t file_sz;
	int error;
	boolean_t hole;

	file_sz = zp->z_size;
	if (noff >= file_sz)  {
		return (SET_ERROR(ENXIO));
	}

	if (cmd == F_SEEK_HOLE)
		hole = B_TRUE;
	else
		hole = B_FALSE;

	 
	if (zn_has_cached_data(zp, 0, file_sz - 1))
		zn_flush_cached_data(zp, B_FALSE);

	lr = zfs_rangelock_enter(&zp->z_rangelock, 0, UINT64_MAX, RL_READER);
	error = dmu_offset_next(ZTOZSB(zp)->z_os, zp->z_id, hole, &noff);
	zfs_rangelock_exit(lr);

	if (error == ESRCH)
		return (SET_ERROR(ENXIO));

	 
	if (error == EBUSY) {
		if (hole)
			*off = file_sz;

		return (0);
	}

	 
	if (noff > file_sz) {
		ASSERT(hole);
		noff = file_sz;
	}

	if (noff < *off)
		return (error);
	*off = noff;
	return (error);
}

int
zfs_holey(znode_t *zp, ulong_t cmd, loff_t *off)
{
	zfsvfs_t *zfsvfs = ZTOZSB(zp);
	int error;

	if ((error = zfs_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		return (error);

	error = zfs_holey_common(zp, cmd, off);

	zfs_exit(zfsvfs, FTAG);
	return (error);
}
#endif  

int
zfs_access(znode_t *zp, int mode, int flag, cred_t *cr)
{
	zfsvfs_t *zfsvfs = ZTOZSB(zp);
	int error;

	if ((error = zfs_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		return (error);

	if (flag & V_ACE_MASK)
#if defined(__linux__)
		error = zfs_zaccess(zp, mode, flag, B_FALSE, cr,
		    zfs_init_idmap);
#else
		error = zfs_zaccess(zp, mode, flag, B_FALSE, cr,
		    NULL);
#endif
	else
#if defined(__linux__)
		error = zfs_zaccess_rwx(zp, mode, flag, cr, zfs_init_idmap);
#else
		error = zfs_zaccess_rwx(zp, mode, flag, cr, NULL);
#endif

	zfs_exit(zfsvfs, FTAG);
	return (error);
}

static uint64_t zfs_vnops_read_chunk_size = 1024 * 1024;  

 
int
zfs_read(struct znode *zp, zfs_uio_t *uio, int ioflag, cred_t *cr)
{
	(void) cr;
	int error = 0;
	boolean_t frsync = B_FALSE;

	zfsvfs_t *zfsvfs = ZTOZSB(zp);
	if ((error = zfs_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		return (error);

	if (zp->z_pflags & ZFS_AV_QUARANTINED) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EACCES));
	}

	 
	if (Z_ISDIR(ZTOTYPE(zp))) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EISDIR));
	}

	 
	if (zfs_uio_offset(uio) < (offset_t)0) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EINVAL));
	}

	 
	if (zfs_uio_resid(uio) == 0) {
		zfs_exit(zfsvfs, FTAG);
		return (0);
	}

#ifdef FRSYNC
	 
	frsync = !!(ioflag & FRSYNC);
#endif
	if (zfsvfs->z_log &&
	    (frsync || zfsvfs->z_os->os_sync == ZFS_SYNC_ALWAYS))
		zil_commit(zfsvfs->z_log, zp->z_id);

	 
	zfs_locked_range_t *lr = zfs_rangelock_enter(&zp->z_rangelock,
	    zfs_uio_offset(uio), zfs_uio_resid(uio), RL_READER);

	 
	if (zfs_uio_offset(uio) >= zp->z_size) {
		error = 0;
		goto out;
	}

	ASSERT(zfs_uio_offset(uio) < zp->z_size);
#if defined(__linux__)
	ssize_t start_offset = zfs_uio_offset(uio);
#endif
	ssize_t n = MIN(zfs_uio_resid(uio), zp->z_size - zfs_uio_offset(uio));
	ssize_t start_resid = n;

	while (n > 0) {
		ssize_t nbytes = MIN(n, zfs_vnops_read_chunk_size -
		    P2PHASE(zfs_uio_offset(uio), zfs_vnops_read_chunk_size));
#ifdef UIO_NOCOPY
		if (zfs_uio_segflg(uio) == UIO_NOCOPY)
			error = mappedread_sf(zp, nbytes, uio);
		else
#endif
		if (zn_has_cached_data(zp, zfs_uio_offset(uio),
		    zfs_uio_offset(uio) + nbytes - 1) && !(ioflag & O_DIRECT)) {
			error = mappedread(zp, nbytes, uio);
		} else {
			error = dmu_read_uio_dbuf(sa_get_db(zp->z_sa_hdl),
			    uio, nbytes);
		}

		if (error) {
			 
			if (error == ECKSUM)
				error = SET_ERROR(EIO);

#if defined(__linux__)
			 
			if (error == EFAULT &&
			    (zfs_uio_offset(uio) - start_offset) != 0)
				error = 0;
#endif
			break;
		}

		n -= nbytes;
	}

	int64_t nread = start_resid - n;
	dataset_kstats_update_read_kstats(&zfsvfs->z_kstat, nread);
	task_io_account_read(nread);
out:
	zfs_rangelock_exit(lr);

	ZFS_ACCESSTIME_STAMP(zfsvfs, zp);
	zfs_exit(zfsvfs, FTAG);
	return (error);
}

static void
zfs_clear_setid_bits_if_necessary(zfsvfs_t *zfsvfs, znode_t *zp, cred_t *cr,
    uint64_t *clear_setid_bits_txgp, dmu_tx_t *tx)
{
	zilog_t *zilog = zfsvfs->z_log;
	const uint64_t uid = KUID_TO_SUID(ZTOUID(zp));

	ASSERT(clear_setid_bits_txgp != NULL);
	ASSERT(tx != NULL);

	 
	mutex_enter(&zp->z_acl_lock);
	if ((zp->z_mode & (S_IXUSR | (S_IXUSR >> 3) | (S_IXUSR >> 6))) != 0 &&
	    (zp->z_mode & (S_ISUID | S_ISGID)) != 0 &&
	    secpolicy_vnode_setid_retain(zp, cr,
	    ((zp->z_mode & S_ISUID) != 0 && uid == 0)) != 0) {
		uint64_t newmode;

		zp->z_mode &= ~(S_ISUID | S_ISGID);
		newmode = zp->z_mode;
		(void) sa_update(zp->z_sa_hdl, SA_ZPL_MODE(zfsvfs),
		    (void *)&newmode, sizeof (uint64_t), tx);

		mutex_exit(&zp->z_acl_lock);

		 
		if (*clear_setid_bits_txgp != dmu_tx_get_txg(tx)) {
			vattr_t va = {0};

			va.va_mask = ATTR_MODE;
			va.va_nodeid = zp->z_id;
			va.va_mode = newmode;
			zfs_log_setattr(zilog, tx, TX_SETATTR, zp, &va,
			    ATTR_MODE, NULL);
			*clear_setid_bits_txgp = dmu_tx_get_txg(tx);
		}
	} else {
		mutex_exit(&zp->z_acl_lock);
	}
}

 
int
zfs_write(znode_t *zp, zfs_uio_t *uio, int ioflag, cred_t *cr)
{
	int error = 0, error1;
	ssize_t start_resid = zfs_uio_resid(uio);
	uint64_t clear_setid_bits_txg = 0;

	 
	ssize_t n = start_resid;
	if (n == 0)
		return (0);

	zfsvfs_t *zfsvfs = ZTOZSB(zp);
	if ((error = zfs_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		return (error);

	sa_bulk_attr_t bulk[4];
	int count = 0;
	uint64_t mtime[2], ctime[2];
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_MTIME(zfsvfs), NULL, &mtime, 16);
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_CTIME(zfsvfs), NULL, &ctime, 16);
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_SIZE(zfsvfs), NULL,
	    &zp->z_size, 8);
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_FLAGS(zfsvfs), NULL,
	    &zp->z_pflags, 8);

	 
	if (zfs_is_readonly(zfsvfs)) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EROFS));
	}

	 
	if ((zp->z_pflags & ZFS_IMMUTABLE) ||
	    ((zp->z_pflags & ZFS_APPENDONLY) && !(ioflag & O_APPEND) &&
	    (zfs_uio_offset(uio) < zp->z_size))) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EPERM));
	}

	 
	offset_t woff = ioflag & O_APPEND ? zp->z_size : zfs_uio_offset(uio);
	if (woff < 0) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EINVAL));
	}

	 
	ssize_t pfbytes = MIN(n, DMU_MAX_ACCESS >> 1);
	if (zfs_uio_prefaultpages(pfbytes, uio)) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EFAULT));
	}

	 
	zfs_locked_range_t *lr;
	if (ioflag & O_APPEND) {
		 
		lr = zfs_rangelock_enter(&zp->z_rangelock, 0, n, RL_APPEND);
		woff = lr->lr_offset;
		if (lr->lr_length == UINT64_MAX) {
			 
			woff = zp->z_size;
		}
		zfs_uio_setoffset(uio, woff);
	} else {
		 
		lr = zfs_rangelock_enter(&zp->z_rangelock, woff, n, RL_WRITER);
	}

	if (zn_rlimit_fsize_uio(zp, uio)) {
		zfs_rangelock_exit(lr);
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EFBIG));
	}

	const rlim64_t limit = MAXOFFSET_T;

	if (woff >= limit) {
		zfs_rangelock_exit(lr);
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EFBIG));
	}

	if (n > limit - woff)
		n = limit - woff;

	uint64_t end_size = MAX(zp->z_size, woff + n);
	zilog_t *zilog = zfsvfs->z_log;

	const uint64_t uid = KUID_TO_SUID(ZTOUID(zp));
	const uint64_t gid = KGID_TO_SGID(ZTOGID(zp));
	const uint64_t projid = zp->z_projid;

	 
	while (n > 0) {
		woff = zfs_uio_offset(uio);

		if (zfs_id_overblockquota(zfsvfs, DMU_USERUSED_OBJECT, uid) ||
		    zfs_id_overblockquota(zfsvfs, DMU_GROUPUSED_OBJECT, gid) ||
		    (projid != ZFS_DEFAULT_PROJID &&
		    zfs_id_overblockquota(zfsvfs, DMU_PROJECTUSED_OBJECT,
		    projid))) {
			error = SET_ERROR(EDQUOT);
			break;
		}

		uint64_t blksz;
		if (lr->lr_length == UINT64_MAX && zp->z_size <= zp->z_blksz) {
			if (zp->z_blksz > zfsvfs->z_max_blksz &&
			    !ISP2(zp->z_blksz)) {
				 
				blksz = 1 << highbit64(zp->z_blksz);
			} else {
				blksz = zfsvfs->z_max_blksz;
			}
			blksz = MIN(blksz, P2ROUNDUP(end_size,
			    SPA_MINBLOCKSIZE));
			blksz = MAX(blksz, zp->z_blksz);
		} else {
			blksz = zp->z_blksz;
		}

		arc_buf_t *abuf = NULL;
		ssize_t nbytes = n;
		if (n >= blksz && woff >= zp->z_size &&
		    P2PHASE(woff, blksz) == 0 &&
		    (blksz >= SPA_OLD_MAXBLOCKSIZE || n < 4 * blksz)) {
			 
			abuf = dmu_request_arcbuf(sa_get_db(zp->z_sa_hdl),
			    blksz);
			ASSERT(abuf != NULL);
			ASSERT(arc_buf_size(abuf) == blksz);
			if ((error = zfs_uiocopy(abuf->b_data, blksz,
			    UIO_WRITE, uio, &nbytes))) {
				dmu_return_arcbuf(abuf);
				break;
			}
			ASSERT3S(nbytes, ==, blksz);
		} else {
			nbytes = MIN(n, (DMU_MAX_ACCESS >> 1) -
			    P2PHASE(woff, blksz));
			if (pfbytes < nbytes) {
				if (zfs_uio_prefaultpages(nbytes, uio)) {
					error = SET_ERROR(EFAULT);
					break;
				}
				pfbytes = nbytes;
			}
		}

		 
		dmu_tx_t *tx = dmu_tx_create(zfsvfs->z_os);
		dmu_tx_hold_sa(tx, zp->z_sa_hdl, B_FALSE);
		dmu_buf_impl_t *db = (dmu_buf_impl_t *)sa_get_db(zp->z_sa_hdl);
		DB_DNODE_ENTER(db);
		dmu_tx_hold_write_by_dnode(tx, DB_DNODE(db), woff, nbytes);
		DB_DNODE_EXIT(db);
		zfs_sa_upgrade_txholds(tx, zp);
		error = dmu_tx_assign(tx, TXG_WAIT);
		if (error) {
			dmu_tx_abort(tx);
			if (abuf != NULL)
				dmu_return_arcbuf(abuf);
			break;
		}

		 

		 
		if (lr->lr_length == UINT64_MAX) {
			zfs_grow_blocksize(zp, blksz, tx);
			zfs_rangelock_reduce(lr, woff, n);
		}

		ssize_t tx_bytes;
		if (abuf == NULL) {
			tx_bytes = zfs_uio_resid(uio);
			zfs_uio_fault_disable(uio, B_TRUE);
			error = dmu_write_uio_dbuf(sa_get_db(zp->z_sa_hdl),
			    uio, nbytes, tx);
			zfs_uio_fault_disable(uio, B_FALSE);
#ifdef __linux__
			if (error == EFAULT) {
				zfs_clear_setid_bits_if_necessary(zfsvfs, zp,
				    cr, &clear_setid_bits_txg, tx);
				dmu_tx_commit(tx);
				 
				n -= tx_bytes - zfs_uio_resid(uio);
				pfbytes -= tx_bytes - zfs_uio_resid(uio);
				continue;
			}
#endif
			 
			if (error != 0 && error != EFAULT) {
				zfs_clear_setid_bits_if_necessary(zfsvfs, zp,
				    cr, &clear_setid_bits_txg, tx);
				dmu_tx_commit(tx);
				break;
			}
			tx_bytes -= zfs_uio_resid(uio);
		} else {
			 
			error = dmu_assign_arcbuf_by_dbuf(
			    sa_get_db(zp->z_sa_hdl), woff, abuf, tx);
			if (error != 0) {
				 
				zfs_clear_setid_bits_if_necessary(zfsvfs, zp,
				    cr, &clear_setid_bits_txg, tx);
				dmu_return_arcbuf(abuf);
				dmu_tx_commit(tx);
				break;
			}
			ASSERT3S(nbytes, <=, zfs_uio_resid(uio));
			zfs_uioskip(uio, nbytes);
			tx_bytes = nbytes;
		}
		if (tx_bytes &&
		    zn_has_cached_data(zp, woff, woff + tx_bytes - 1) &&
		    !(ioflag & O_DIRECT)) {
			update_pages(zp, woff, tx_bytes, zfsvfs->z_os);
		}

		 
		if (tx_bytes == 0) {
			(void) sa_update(zp->z_sa_hdl, SA_ZPL_SIZE(zfsvfs),
			    (void *)&zp->z_size, sizeof (uint64_t), tx);
			dmu_tx_commit(tx);
			ASSERT(error != 0);
			break;
		}

		zfs_clear_setid_bits_if_necessary(zfsvfs, zp, cr,
		    &clear_setid_bits_txg, tx);

		zfs_tstamp_update_setup(zp, CONTENT_MODIFIED, mtime, ctime);

		 
		while ((end_size = zp->z_size) < zfs_uio_offset(uio)) {
			(void) atomic_cas_64(&zp->z_size, end_size,
			    zfs_uio_offset(uio));
			ASSERT(error == 0 || error == EFAULT);
		}
		 
		if (zfsvfs->z_replay && zfsvfs->z_replay_eof != 0)
			zp->z_size = zfsvfs->z_replay_eof;

		error1 = sa_bulk_update(zp->z_sa_hdl, bulk, count, tx);
		if (error1 != 0)
			 
			error = error1;

		 
		zfs_log_write(zilog, tx, TX_WRITE, zp, woff, tx_bytes, ioflag,
		    NULL, NULL);

		dmu_tx_commit(tx);

		if (error != 0)
			break;
		ASSERT3S(tx_bytes, ==, nbytes);
		n -= nbytes;
		pfbytes -= nbytes;
	}

	zfs_znode_update_vfs(zp);
	zfs_rangelock_exit(lr);

	 
	if (zfsvfs->z_replay || zfs_uio_resid(uio) == start_resid ||
	    error == EFAULT) {
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}

	if (ioflag & (O_SYNC | O_DSYNC) ||
	    zfsvfs->z_os->os_sync == ZFS_SYNC_ALWAYS)
		zil_commit(zilog, zp->z_id);

	const int64_t nwritten = start_resid - zfs_uio_resid(uio);
	dataset_kstats_update_write_kstats(&zfsvfs->z_kstat, nwritten);
	task_io_account_write(nwritten);

	zfs_exit(zfsvfs, FTAG);
	return (0);
}

int
zfs_getsecattr(znode_t *zp, vsecattr_t *vsecp, int flag, cred_t *cr)
{
	zfsvfs_t *zfsvfs = ZTOZSB(zp);
	int error;
	boolean_t skipaclchk = (flag & ATTR_NOACLCHECK) ? B_TRUE : B_FALSE;

	if ((error = zfs_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		return (error);
	error = zfs_getacl(zp, vsecp, skipaclchk, cr);
	zfs_exit(zfsvfs, FTAG);

	return (error);
}

int
zfs_setsecattr(znode_t *zp, vsecattr_t *vsecp, int flag, cred_t *cr)
{
	zfsvfs_t *zfsvfs = ZTOZSB(zp);
	int error;
	boolean_t skipaclchk = (flag & ATTR_NOACLCHECK) ? B_TRUE : B_FALSE;
	zilog_t	*zilog = zfsvfs->z_log;

	if ((error = zfs_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		return (error);

	error = zfs_setacl(zp, vsecp, skipaclchk, cr);

	if (zfsvfs->z_os->os_sync == ZFS_SYNC_ALWAYS)
		zil_commit(zilog, 0);

	zfs_exit(zfsvfs, FTAG);
	return (error);
}

#ifdef ZFS_DEBUG
static int zil_fault_io = 0;
#endif

static void zfs_get_done(zgd_t *zgd, int error);

 
int
zfs_get_data(void *arg, uint64_t gen, lr_write_t *lr, char *buf,
    struct lwb *lwb, zio_t *zio)
{
	zfsvfs_t *zfsvfs = arg;
	objset_t *os = zfsvfs->z_os;
	znode_t *zp;
	uint64_t object = lr->lr_foid;
	uint64_t offset = lr->lr_offset;
	uint64_t size = lr->lr_length;
	dmu_buf_t *db;
	zgd_t *zgd;
	int error = 0;
	uint64_t zp_gen;

	ASSERT3P(lwb, !=, NULL);
	ASSERT3U(size, !=, 0);

	 
	if (zfs_zget(zfsvfs, object, &zp) != 0)
		return (SET_ERROR(ENOENT));
	if (zp->z_unlinked) {
		 
		zfs_zrele_async(zp);
		return (SET_ERROR(ENOENT));
	}
	 
	if (sa_lookup(zp->z_sa_hdl, SA_ZPL_GEN(zfsvfs), &zp_gen,
	    sizeof (zp_gen)) != 0) {
		zfs_zrele_async(zp);
		return (SET_ERROR(EIO));
	}
	if (zp_gen != gen) {
		zfs_zrele_async(zp);
		return (SET_ERROR(ENOENT));
	}

	zgd = kmem_zalloc(sizeof (zgd_t), KM_SLEEP);
	zgd->zgd_lwb = lwb;
	zgd->zgd_private = zp;

	 
	if (buf != NULL) {  
		zgd->zgd_lr = zfs_rangelock_enter(&zp->z_rangelock,
		    offset, size, RL_READER);
		 
		if (offset >= zp->z_size) {
			error = SET_ERROR(ENOENT);
		} else {
			error = dmu_read(os, object, offset, size, buf,
			    DMU_READ_NO_PREFETCH);
		}
		ASSERT(error == 0 || error == ENOENT);
	} else {  
		ASSERT3P(zio, !=, NULL);
		 
		for (;;) {
			uint64_t blkoff;
			size = zp->z_blksz;
			blkoff = ISP2(size) ? P2PHASE(offset, size) : offset;
			offset -= blkoff;
			zgd->zgd_lr = zfs_rangelock_enter(&zp->z_rangelock,
			    offset, size, RL_READER);
			if (zp->z_blksz == size)
				break;
			offset += blkoff;
			zfs_rangelock_exit(zgd->zgd_lr);
		}
		 
		if (lr->lr_offset >= zp->z_size)
			error = SET_ERROR(ENOENT);
#ifdef ZFS_DEBUG
		if (zil_fault_io) {
			error = SET_ERROR(EIO);
			zil_fault_io = 0;
		}
#endif
		if (error == 0)
			error = dmu_buf_hold_noread(os, object, offset, zgd,
			    &db);

		if (error == 0) {
			blkptr_t *bp = &lr->lr_blkptr;

			zgd->zgd_db = db;
			zgd->zgd_bp = bp;

			ASSERT(db->db_offset == offset);
			ASSERT(db->db_size == size);

			error = dmu_sync(zio, lr->lr_common.lrc_txg,
			    zfs_get_done, zgd);
			ASSERT(error || lr->lr_length <= size);

			 
			if (error == 0)
				return (0);

			if (error == EALREADY) {
				lr->lr_common.lrc_txtype = TX_WRITE2;
				 
				zgd->zgd_bp = NULL;
				BP_ZERO(bp);
				error = 0;
			}
		}
	}

	zfs_get_done(zgd, error);

	return (error);
}


static void
zfs_get_done(zgd_t *zgd, int error)
{
	(void) error;
	znode_t *zp = zgd->zgd_private;

	if (zgd->zgd_db)
		dmu_buf_rele(zgd->zgd_db, zgd);

	zfs_rangelock_exit(zgd->zgd_lr);

	 
	zfs_zrele_async(zp);

	kmem_free(zgd, sizeof (zgd_t));
}

static int
zfs_enter_two(zfsvfs_t *zfsvfs1, zfsvfs_t *zfsvfs2, const char *tag)
{
	int error;

	 
	if (zfsvfs1 > zfsvfs2) {
		zfsvfs_t *tmpzfsvfs;

		tmpzfsvfs = zfsvfs2;
		zfsvfs2 = zfsvfs1;
		zfsvfs1 = tmpzfsvfs;
	}

	error = zfs_enter(zfsvfs1, tag);
	if (error != 0)
		return (error);
	if (zfsvfs1 != zfsvfs2) {
		error = zfs_enter(zfsvfs2, tag);
		if (error != 0) {
			zfs_exit(zfsvfs1, tag);
			return (error);
		}
	}

	return (0);
}

static void
zfs_exit_two(zfsvfs_t *zfsvfs1, zfsvfs_t *zfsvfs2, const char *tag)
{

	zfs_exit(zfsvfs1, tag);
	if (zfsvfs1 != zfsvfs2)
		zfs_exit(zfsvfs2, tag);
}

 
int
zfs_clone_range(znode_t *inzp, uint64_t *inoffp, znode_t *outzp,
    uint64_t *outoffp, uint64_t *lenp, cred_t *cr)
{
	zfsvfs_t	*inzfsvfs, *outzfsvfs;
	objset_t	*inos, *outos;
	zfs_locked_range_t *inlr, *outlr;
	dmu_buf_impl_t	*db;
	dmu_tx_t	*tx;
	zilog_t		*zilog;
	uint64_t	inoff, outoff, len, done;
	uint64_t	outsize, size;
	int		error;
	int		count = 0;
	sa_bulk_attr_t	bulk[3];
	uint64_t	mtime[2], ctime[2];
	uint64_t	uid, gid, projid;
	blkptr_t	*bps;
	size_t		maxblocks, nbps;
	uint_t		inblksz;
	uint64_t	clear_setid_bits_txg = 0;

	inoff = *inoffp;
	outoff = *outoffp;
	len = *lenp;
	done = 0;

	inzfsvfs = ZTOZSB(inzp);
	outzfsvfs = ZTOZSB(outzp);

	 
	error = zfs_enter_two(inzfsvfs, outzfsvfs, FTAG);
	if (error != 0)
		return (error);

	inos = inzfsvfs->z_os;
	outos = outzfsvfs->z_os;

	 
	if (dmu_objset_spa(inos) != dmu_objset_spa(outos)) {
		zfs_exit_two(inzfsvfs, outzfsvfs, FTAG);
		return (SET_ERROR(EXDEV));
	}

	 
	if (!spa_feature_is_enabled(dmu_objset_spa(outos),
	    SPA_FEATURE_BLOCK_CLONING)) {
		zfs_exit_two(inzfsvfs, outzfsvfs, FTAG);
		return (SET_ERROR(EOPNOTSUPP));
	}

	ASSERT(!outzfsvfs->z_replay);

	 
	if (inos->os_encrypted != outos->os_encrypted) {
		zfs_exit_two(inzfsvfs, outzfsvfs, FTAG);
		return (SET_ERROR(EXDEV));
	}

	error = zfs_verify_zp(inzp);
	if (error == 0)
		error = zfs_verify_zp(outzp);
	if (error != 0) {
		zfs_exit_two(inzfsvfs, outzfsvfs, FTAG);
		return (error);
	}

	 
	if (inzp->z_pflags & ZFS_AV_QUARANTINED) {
		zfs_exit_two(inzfsvfs, outzfsvfs, FTAG);
		return (SET_ERROR(EACCES));
	}

	if (inoff >= inzp->z_size) {
		*lenp = 0;
		zfs_exit_two(inzfsvfs, outzfsvfs, FTAG);
		return (0);
	}
	if (len > inzp->z_size - inoff) {
		len = inzp->z_size - inoff;
	}
	if (len == 0) {
		*lenp = 0;
		zfs_exit_two(inzfsvfs, outzfsvfs, FTAG);
		return (0);
	}

	 
	if (zfs_is_readonly(outzfsvfs)) {
		zfs_exit_two(inzfsvfs, outzfsvfs, FTAG);
		return (SET_ERROR(EROFS));
	}

	 
	if ((outzp->z_pflags & ZFS_IMMUTABLE) != 0) {
		zfs_exit_two(inzfsvfs, outzfsvfs, FTAG);
		return (SET_ERROR(EPERM));
	}

	 
	if (inzp == outzp) {
		if (inoff < outoff + len && outoff < inoff + len) {
			zfs_exit_two(inzfsvfs, outzfsvfs, FTAG);
			return (SET_ERROR(EINVAL));
		}
	}

	 
	if (inzp < outzp || (inzp == outzp && inoff < outoff)) {
		inlr = zfs_rangelock_enter(&inzp->z_rangelock, inoff, len,
		    RL_READER);
		outlr = zfs_rangelock_enter(&outzp->z_rangelock, outoff, len,
		    RL_WRITER);
	} else {
		outlr = zfs_rangelock_enter(&outzp->z_rangelock, outoff, len,
		    RL_WRITER);
		inlr = zfs_rangelock_enter(&inzp->z_rangelock, inoff, len,
		    RL_READER);
	}

	inblksz = inzp->z_blksz;

	 
	if (inblksz != outzp->z_blksz && (outzp->z_size > outzp->z_blksz ||
	    outzp->z_size > inblksz)) {
		error = SET_ERROR(EINVAL);
		goto unlock;
	}

	 
	if (outoff != 0 && !ISP2(inblksz)) {
		error = SET_ERROR(EINVAL);
		goto unlock;
	}

	 
	if ((inoff % inblksz) != 0 || (outoff % inblksz) != 0) {
		error = SET_ERROR(EINVAL);
		goto unlock;
	}
	 
	if ((len % inblksz) != 0 &&
	    (len < inzp->z_size - inoff || len < outzp->z_size - outoff)) {
		error = SET_ERROR(EINVAL);
		goto unlock;
	}

	 
	if (len <= inblksz && inblksz < outzfsvfs->z_max_blksz &&
	    outzp->z_size <= inblksz && outoff + len > inblksz) {
		error = SET_ERROR(EINVAL);
		goto unlock;
	}

	error = zn_rlimit_fsize(outoff + len);
	if (error != 0) {
		goto unlock;
	}

	if (inoff >= MAXOFFSET_T || outoff >= MAXOFFSET_T) {
		error = SET_ERROR(EFBIG);
		goto unlock;
	}

	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_MTIME(outzfsvfs), NULL,
	    &mtime, 16);
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_CTIME(outzfsvfs), NULL,
	    &ctime, 16);
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_SIZE(outzfsvfs), NULL,
	    &outzp->z_size, 8);

	zilog = outzfsvfs->z_log;
	maxblocks = zil_max_log_data(zilog, sizeof (lr_clone_range_t)) /
	    sizeof (bps[0]);

	uid = KUID_TO_SUID(ZTOUID(outzp));
	gid = KGID_TO_SGID(ZTOGID(outzp));
	projid = outzp->z_projid;

	bps = vmem_alloc(sizeof (bps[0]) * maxblocks, KM_SLEEP);

	 
	while (len > 0) {
		size = MIN(inblksz * maxblocks, len);

		if (zfs_id_overblockquota(outzfsvfs, DMU_USERUSED_OBJECT,
		    uid) ||
		    zfs_id_overblockquota(outzfsvfs, DMU_GROUPUSED_OBJECT,
		    gid) ||
		    (projid != ZFS_DEFAULT_PROJID &&
		    zfs_id_overblockquota(outzfsvfs, DMU_PROJECTUSED_OBJECT,
		    projid))) {
			error = SET_ERROR(EDQUOT);
			break;
		}

		nbps = maxblocks;
		error = dmu_read_l0_bps(inos, inzp->z_id, inoff, size, bps,
		    &nbps);
		if (error != 0) {
			 
			break;
		}
		 
		if (BP_IS_PROTECTED(&bps[0])) {
			if (inzfsvfs != outzfsvfs) {
				error = SET_ERROR(EXDEV);
				break;
			}
		}

		 
		tx = dmu_tx_create(outos);
		dmu_tx_hold_sa(tx, outzp->z_sa_hdl, B_FALSE);
		db = (dmu_buf_impl_t *)sa_get_db(outzp->z_sa_hdl);
		DB_DNODE_ENTER(db);
		dmu_tx_hold_clone_by_dnode(tx, DB_DNODE(db), outoff, size);
		DB_DNODE_EXIT(db);
		zfs_sa_upgrade_txholds(tx, outzp);
		error = dmu_tx_assign(tx, TXG_WAIT);
		if (error != 0) {
			dmu_tx_abort(tx);
			break;
		}

		 
		if (outlr->lr_length == UINT64_MAX) {
			zfs_grow_blocksize(outzp, inblksz, tx);
			 
			zfs_rangelock_reduce(outlr, outoff,
			    ((len - 1) / inblksz + 1) * inblksz);
		}

		error = dmu_brt_clone(outos, outzp->z_id, outoff, size, tx,
		    bps, nbps);
		if (error != 0) {
			dmu_tx_commit(tx);
			break;
		}

		zfs_clear_setid_bits_if_necessary(outzfsvfs, outzp, cr,
		    &clear_setid_bits_txg, tx);

		zfs_tstamp_update_setup(outzp, CONTENT_MODIFIED, mtime, ctime);

		 
		while ((outsize = outzp->z_size) < outoff + size) {
			(void) atomic_cas_64(&outzp->z_size, outsize,
			    outoff + size);
		}

		error = sa_bulk_update(outzp->z_sa_hdl, bulk, count, tx);

		zfs_log_clone_range(zilog, tx, TX_CLONE_RANGE, outzp, outoff,
		    size, inblksz, bps, nbps);

		dmu_tx_commit(tx);

		if (error != 0)
			break;

		inoff += size;
		outoff += size;
		len -= size;
		done += size;
	}

	vmem_free(bps, sizeof (bps[0]) * maxblocks);
	zfs_znode_update_vfs(outzp);

unlock:
	zfs_rangelock_exit(outlr);
	zfs_rangelock_exit(inlr);

	if (done > 0) {
		 
		error = 0;

		ZFS_ACCESSTIME_STAMP(inzfsvfs, inzp);

		if (outos->os_sync == ZFS_SYNC_ALWAYS) {
			zil_commit(zilog, outzp->z_id);
		}

		*inoffp += done;
		*outoffp += done;
		*lenp = done;
	} else {
		 
		ASSERT3S(error, !=, 0);
	}

	zfs_exit_two(inzfsvfs, outzfsvfs, FTAG);

	return (error);
}

 
int
zfs_clone_range_replay(znode_t *zp, uint64_t off, uint64_t len, uint64_t blksz,
    const blkptr_t *bps, size_t nbps)
{
	zfsvfs_t	*zfsvfs;
	dmu_buf_impl_t	*db;
	dmu_tx_t	*tx;
	int		error;
	int		count = 0;
	sa_bulk_attr_t	bulk[3];
	uint64_t	mtime[2], ctime[2];

	ASSERT3U(off, <, MAXOFFSET_T);
	ASSERT3U(len, >, 0);
	ASSERT3U(nbps, >, 0);

	zfsvfs = ZTOZSB(zp);

	ASSERT(spa_feature_is_enabled(dmu_objset_spa(zfsvfs->z_os),
	    SPA_FEATURE_BLOCK_CLONING));

	if ((error = zfs_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		return (error);

	ASSERT(zfsvfs->z_replay);
	ASSERT(!zfs_is_readonly(zfsvfs));

	if ((off % blksz) != 0) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EINVAL));
	}

	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_MTIME(zfsvfs), NULL, &mtime, 16);
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_CTIME(zfsvfs), NULL, &ctime, 16);
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_SIZE(zfsvfs), NULL,
	    &zp->z_size, 8);

	 
	tx = dmu_tx_create(zfsvfs->z_os);

	dmu_tx_hold_sa(tx, zp->z_sa_hdl, B_FALSE);
	db = (dmu_buf_impl_t *)sa_get_db(zp->z_sa_hdl);
	DB_DNODE_ENTER(db);
	dmu_tx_hold_clone_by_dnode(tx, DB_DNODE(db), off, len);
	DB_DNODE_EXIT(db);
	zfs_sa_upgrade_txholds(tx, zp);
	error = dmu_tx_assign(tx, TXG_WAIT);
	if (error != 0) {
		dmu_tx_abort(tx);
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}

	if (zp->z_blksz < blksz)
		zfs_grow_blocksize(zp, blksz, tx);

	dmu_brt_clone(zfsvfs->z_os, zp->z_id, off, len, tx, bps, nbps);

	zfs_tstamp_update_setup(zp, CONTENT_MODIFIED, mtime, ctime);

	if (zp->z_size < off + len)
		zp->z_size = off + len;

	error = sa_bulk_update(zp->z_sa_hdl, bulk, count, tx);

	 
	VERIFY(zil_replaying(zfsvfs->z_log, tx));

	dmu_tx_commit(tx);

	zfs_znode_update_vfs(zp);

	zfs_exit(zfsvfs, FTAG);

	return (error);
}

EXPORT_SYMBOL(zfs_access);
EXPORT_SYMBOL(zfs_fsync);
EXPORT_SYMBOL(zfs_holey);
EXPORT_SYMBOL(zfs_read);
EXPORT_SYMBOL(zfs_write);
EXPORT_SYMBOL(zfs_getsecattr);
EXPORT_SYMBOL(zfs_setsecattr);
EXPORT_SYMBOL(zfs_clone_range);
EXPORT_SYMBOL(zfs_clone_range_replay);

ZFS_MODULE_PARAM(zfs_vnops, zfs_vnops_, read_chunk_size, U64, ZMOD_RW,
	"Bytes to read per chunk");
