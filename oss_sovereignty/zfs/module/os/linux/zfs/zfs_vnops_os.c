#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/sysmacros.h>
#include <sys/vfs.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/kmem.h>
#include <sys/taskq.h>
#include <sys/uio.h>
#include <sys/vmsystm.h>
#include <sys/atomic.h>
#include <sys/pathname.h>
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
#include <sys/zap.h>
#include <sys/sa.h>
#include <sys/policy.h>
#include <sys/sunddi.h>
#include <sys/sid.h>
#include <sys/zfs_ctldir.h>
#include <sys/zfs_fuid.h>
#include <sys/zfs_quota.h>
#include <sys/zfs_sa.h>
#include <sys/zfs_vnops.h>
#include <sys/zfs_rlock.h>
#include <sys/cred.h>
#include <sys/zpl.h>
#include <sys/zil.h>
#include <sys/sa_impl.h>
int
zfs_open(struct inode *ip, int mode, int flag, cred_t *cr)
{
	(void) cr;
	znode_t	*zp = ITOZ(ip);
	zfsvfs_t *zfsvfs = ITOZSB(ip);
	int error;
	if ((error = zfs_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		return (error);
	if (blk_mode_is_open_write(mode) && (zp->z_pflags & ZFS_APPENDONLY) &&
	    ((flag & O_APPEND) == 0)) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EPERM));
	}
	if (flag & O_SYNC)
		atomic_inc_32(&zp->z_sync_cnt);
	zfs_exit(zfsvfs, FTAG);
	return (0);
}
int
zfs_close(struct inode *ip, int flag, cred_t *cr)
{
	(void) cr;
	znode_t	*zp = ITOZ(ip);
	zfsvfs_t *zfsvfs = ITOZSB(ip);
	int error;
	if ((error = zfs_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		return (error);
	if (flag & O_SYNC)
		atomic_dec_32(&zp->z_sync_cnt);
	zfs_exit(zfsvfs, FTAG);
	return (0);
}
#if defined(_KERNEL)
static int zfs_fillpage(struct inode *ip, struct page *pp);
void
update_pages(znode_t *zp, int64_t start, int len, objset_t *os)
{
	struct address_space *mp = ZTOI(zp)->i_mapping;
	int64_t off = start & (PAGE_SIZE - 1);
	for (start &= PAGE_MASK; len > 0; start += PAGE_SIZE) {
		uint64_t nbytes = MIN(PAGE_SIZE - off, len);
		struct page *pp = find_lock_page(mp, start >> PAGE_SHIFT);
		if (pp) {
			if (mapping_writably_mapped(mp))
				flush_dcache_page(pp);
			void *pb = kmap(pp);
			int error = dmu_read(os, zp->z_id, start + off,
			    nbytes, pb + off, DMU_READ_PREFETCH);
			kunmap(pp);
			if (error) {
				SetPageError(pp);
				ClearPageUptodate(pp);
			} else {
				ClearPageError(pp);
				SetPageUptodate(pp);
				if (mapping_writably_mapped(mp))
					flush_dcache_page(pp);
				mark_page_accessed(pp);
			}
			unlock_page(pp);
			put_page(pp);
		}
		len -= nbytes;
		off = 0;
	}
}
int
mappedread(znode_t *zp, int nbytes, zfs_uio_t *uio)
{
	struct inode *ip = ZTOI(zp);
	struct address_space *mp = ip->i_mapping;
	int64_t start = uio->uio_loffset;
	int64_t off = start & (PAGE_SIZE - 1);
	int len = nbytes;
	int error = 0;
	for (start &= PAGE_MASK; len > 0; start += PAGE_SIZE) {
		uint64_t bytes = MIN(PAGE_SIZE - off, len);
		struct page *pp = find_lock_page(mp, start >> PAGE_SHIFT);
		if (pp) {
			if (unlikely(!PageUptodate(pp))) {
				error = zfs_fillpage(ip, pp);
				if (error) {
					unlock_page(pp);
					put_page(pp);
					return (error);
				}
			}
			ASSERT(PageUptodate(pp) || PageDirty(pp));
			unlock_page(pp);
			void *pb = kmap(pp);
			error = zfs_uiomove(pb + off, bytes, UIO_READ, uio);
			kunmap(pp);
			if (mapping_writably_mapped(mp))
				flush_dcache_page(pp);
			mark_page_accessed(pp);
			put_page(pp);
		} else {
			error = dmu_read_uio_dbuf(sa_get_db(zp->z_sa_hdl),
			    uio, bytes);
		}
		len -= bytes;
		off = 0;
		if (error)
			break;
	}
	return (error);
}
#endif  
static unsigned long zfs_delete_blocks = DMU_MAX_DELETEBLKCNT;
int
zfs_write_simple(znode_t *zp, const void *data, size_t len,
    loff_t pos, size_t *residp)
{
	fstrans_cookie_t cookie;
	int error;
	struct iovec iov;
	iov.iov_base = (void *)data;
	iov.iov_len = len;
	zfs_uio_t uio;
	zfs_uio_iovec_init(&uio, &iov, 1, pos, UIO_SYSSPACE, len, 0);
	cookie = spl_fstrans_mark();
	error = zfs_write(zp, &uio, 0, kcred);
	spl_fstrans_unmark(cookie);
	if (error == 0) {
		if (residp != NULL)
			*residp = zfs_uio_resid(&uio);
		else if (zfs_uio_resid(&uio) != 0)
			error = SET_ERROR(EIO);
	}
	return (error);
}
static void
zfs_rele_async_task(void *arg)
{
	iput(arg);
}
void
zfs_zrele_async(znode_t *zp)
{
	struct inode *ip = ZTOI(zp);
	objset_t *os = ITOZSB(ip)->z_os;
	ASSERT(atomic_read(&ip->i_count) > 0);
	ASSERT(os != NULL);
	if (!atomic_add_unless(&ip->i_count, -1, 1)) {
		VERIFY(taskq_dispatch(dsl_pool_zrele_taskq(dmu_objset_pool(os)),
		    zfs_rele_async_task, ip, TQ_SLEEP) != TASKQID_INVALID);
	}
}
int
zfs_lookup(znode_t *zdp, char *nm, znode_t **zpp, int flags, cred_t *cr,
    int *direntflags, pathname_t *realpnp)
{
	zfsvfs_t *zfsvfs = ZTOZSB(zdp);
	int error = 0;
	if (!(flags & (LOOKUP_XATTR | FIGNORECASE))) {
		if (!S_ISDIR(ZTOI(zdp)->i_mode)) {
			return (SET_ERROR(ENOTDIR));
		} else if (zdp->z_sa_hdl == NULL) {
			return (SET_ERROR(EIO));
		}
		if (nm[0] == 0 || (nm[0] == '.' && nm[1] == '\0')) {
			error = zfs_fastaccesschk_execute(zdp, cr);
			if (!error) {
				*zpp = zdp;
				zhold(*zpp);
				return (0);
			}
			return (error);
		}
	}
	if ((error = zfs_enter_verify_zp(zfsvfs, zdp, FTAG)) != 0)
		return (error);
	*zpp = NULL;
	if (flags & LOOKUP_XATTR) {
		if (zdp->z_pflags & ZFS_XATTR) {
			zfs_exit(zfsvfs, FTAG);
			return (SET_ERROR(EINVAL));
		}
		if ((error = zfs_get_xattrdir(zdp, zpp, cr, flags))) {
			zfs_exit(zfsvfs, FTAG);
			return (error);
		}
		if ((error = zfs_zaccess(*zpp, ACE_EXECUTE, 0,
		    B_TRUE, cr, zfs_init_idmap))) {
			zrele(*zpp);
			*zpp = NULL;
		}
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	if (!S_ISDIR(ZTOI(zdp)->i_mode)) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(ENOTDIR));
	}
	if ((error = zfs_zaccess(zdp, ACE_EXECUTE, 0, B_FALSE, cr,
	    zfs_init_idmap))) {
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	if (zfsvfs->z_utf8 && u8_validate(nm, strlen(nm),
	    NULL, U8_VALIDATE_ENTIRE, &error) < 0) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EILSEQ));
	}
	error = zfs_dirlook(zdp, nm, zpp, flags, direntflags, realpnp);
	if ((error == 0) && (*zpp))
		zfs_znode_update_vfs(*zpp);
	zfs_exit(zfsvfs, FTAG);
	return (error);
}
int
zfs_create(znode_t *dzp, char *name, vattr_t *vap, int excl,
    int mode, znode_t **zpp, cred_t *cr, int flag, vsecattr_t *vsecp,
    zidmap_t *mnt_ns)
{
	znode_t		*zp;
	zfsvfs_t	*zfsvfs = ZTOZSB(dzp);
	zilog_t		*zilog;
	objset_t	*os;
	zfs_dirlock_t	*dl;
	dmu_tx_t	*tx;
	int		error;
	uid_t		uid;
	gid_t		gid;
	zfs_acl_ids_t   acl_ids;
	boolean_t	fuid_dirtied;
	boolean_t	have_acl = B_FALSE;
	boolean_t	waited = B_FALSE;
	boolean_t	skip_acl = (flag & ATTR_NOACLCHECK) ? B_TRUE : B_FALSE;
	gid = crgetgid(cr);
	uid = crgetuid(cr);
	if (zfsvfs->z_use_fuids == B_FALSE &&
	    (vsecp || IS_EPHEMERAL(uid) || IS_EPHEMERAL(gid)))
		return (SET_ERROR(EINVAL));
	if (name == NULL)
		return (SET_ERROR(EINVAL));
	if ((error = zfs_enter_verify_zp(zfsvfs, dzp, FTAG)) != 0)
		return (error);
	os = zfsvfs->z_os;
	zilog = zfsvfs->z_log;
	if (zfsvfs->z_utf8 && u8_validate(name, strlen(name),
	    NULL, U8_VALIDATE_ENTIRE, &error) < 0) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EILSEQ));
	}
	if (vap->va_mask & ATTR_XVATTR) {
		if ((error = secpolicy_xvattr((xvattr_t *)vap,
		    crgetuid(cr), cr, vap->va_mode)) != 0) {
			zfs_exit(zfsvfs, FTAG);
			return (error);
		}
	}
top:
	*zpp = NULL;
	if (*name == '\0') {
		zhold(dzp);
		zp = dzp;
		dl = NULL;
		error = 0;
	} else {
		int zflg = 0;
		if (flag & FIGNORECASE)
			zflg |= ZCILOOK;
		error = zfs_dirent_lock(&dl, dzp, name, &zp, zflg,
		    NULL, NULL);
		if (error) {
			if (have_acl)
				zfs_acl_ids_free(&acl_ids);
			if (strcmp(name, "..") == 0)
				error = SET_ERROR(EISDIR);
			zfs_exit(zfsvfs, FTAG);
			return (error);
		}
	}
	if (zp == NULL) {
		uint64_t txtype;
		uint64_t projid = ZFS_DEFAULT_PROJID;
		if ((error = zfs_zaccess(dzp, ACE_ADD_FILE, 0, skip_acl, cr,
		    mnt_ns))) {
			if (have_acl)
				zfs_acl_ids_free(&acl_ids);
			goto out;
		}
		if ((dzp->z_pflags & ZFS_XATTR) && !S_ISREG(vap->va_mode)) {
			if (have_acl)
				zfs_acl_ids_free(&acl_ids);
			error = SET_ERROR(EINVAL);
			goto out;
		}
		if (!have_acl && (error = zfs_acl_ids_create(dzp, 0, vap,
		    cr, vsecp, &acl_ids, mnt_ns)) != 0)
			goto out;
		have_acl = B_TRUE;
		if (S_ISREG(vap->va_mode) || S_ISDIR(vap->va_mode))
			projid = zfs_inherit_projid(dzp);
		if (zfs_acl_ids_overquota(zfsvfs, &acl_ids, projid)) {
			zfs_acl_ids_free(&acl_ids);
			error = SET_ERROR(EDQUOT);
			goto out;
		}
		tx = dmu_tx_create(os);
		dmu_tx_hold_sa_create(tx, acl_ids.z_aclp->z_acl_bytes +
		    ZFS_SA_BASE_ATTR_SIZE);
		fuid_dirtied = zfsvfs->z_fuid_dirty;
		if (fuid_dirtied)
			zfs_fuid_txhold(zfsvfs, tx);
		dmu_tx_hold_zap(tx, dzp->z_id, TRUE, name);
		dmu_tx_hold_sa(tx, dzp->z_sa_hdl, B_FALSE);
		if (!zfsvfs->z_use_sa &&
		    acl_ids.z_aclp->z_acl_bytes > ZFS_ACE_SPACE) {
			dmu_tx_hold_write(tx, DMU_NEW_OBJECT,
			    0, acl_ids.z_aclp->z_acl_bytes);
		}
		error = dmu_tx_assign(tx,
		    (waited ? TXG_NOTHROTTLE : 0) | TXG_NOWAIT);
		if (error) {
			zfs_dirent_unlock(dl);
			if (error == ERESTART) {
				waited = B_TRUE;
				dmu_tx_wait(tx);
				dmu_tx_abort(tx);
				goto top;
			}
			zfs_acl_ids_free(&acl_ids);
			dmu_tx_abort(tx);
			zfs_exit(zfsvfs, FTAG);
			return (error);
		}
		zfs_mknode(dzp, vap, tx, cr, 0, &zp, &acl_ids);
		error = zfs_link_create(dl, zp, tx, ZNEW);
		if (error != 0) {
			zfs_znode_delete(zp, tx);
			remove_inode_hash(ZTOI(zp));
			zfs_acl_ids_free(&acl_ids);
			dmu_tx_commit(tx);
			goto out;
		}
		if (fuid_dirtied)
			zfs_fuid_sync(zfsvfs, tx);
		txtype = zfs_log_create_txtype(Z_FILE, vsecp, vap);
		if (flag & FIGNORECASE)
			txtype |= TX_CI;
		zfs_log_create(zilog, tx, txtype, dzp, zp, name,
		    vsecp, acl_ids.z_fuidp, vap);
		zfs_acl_ids_free(&acl_ids);
		dmu_tx_commit(tx);
	} else {
		int aflags = (flag & O_APPEND) ? V_APPEND : 0;
		if (have_acl)
			zfs_acl_ids_free(&acl_ids);
		if (excl) {
			error = SET_ERROR(EEXIST);
			goto out;
		}
		if (S_ISDIR(ZTOI(zp)->i_mode)) {
			error = SET_ERROR(EISDIR);
			goto out;
		}
		if (mode && (error = zfs_zaccess_rwx(zp, mode, aflags, cr,
		    mnt_ns))) {
			goto out;
		}
		mutex_enter(&dzp->z_lock);
		dzp->z_seq++;
		mutex_exit(&dzp->z_lock);
		if (S_ISREG(ZTOI(zp)->i_mode) &&
		    (vap->va_mask & ATTR_SIZE) && (vap->va_size == 0)) {
			if (dl) {
				zfs_dirent_unlock(dl);
				dl = NULL;
			}
			error = zfs_freesp(zp, 0, 0, mode, TRUE);
		}
	}
out:
	if (dl)
		zfs_dirent_unlock(dl);
	if (error) {
		if (zp)
			zrele(zp);
	} else {
		zfs_znode_update_vfs(dzp);
		zfs_znode_update_vfs(zp);
		*zpp = zp;
	}
	if (zfsvfs->z_os->os_sync == ZFS_SYNC_ALWAYS)
		zil_commit(zilog, 0);
	zfs_exit(zfsvfs, FTAG);
	return (error);
}
int
zfs_tmpfile(struct inode *dip, vattr_t *vap, int excl,
    int mode, struct inode **ipp, cred_t *cr, int flag, vsecattr_t *vsecp,
    zidmap_t *mnt_ns)
{
	(void) excl, (void) mode, (void) flag;
	znode_t		*zp = NULL, *dzp = ITOZ(dip);
	zfsvfs_t	*zfsvfs = ITOZSB(dip);
	objset_t	*os;
	dmu_tx_t	*tx;
	int		error;
	uid_t		uid;
	gid_t		gid;
	zfs_acl_ids_t   acl_ids;
	uint64_t	projid = ZFS_DEFAULT_PROJID;
	boolean_t	fuid_dirtied;
	boolean_t	have_acl = B_FALSE;
	boolean_t	waited = B_FALSE;
	gid = crgetgid(cr);
	uid = crgetuid(cr);
	if (zfsvfs->z_use_fuids == B_FALSE &&
	    (vsecp || IS_EPHEMERAL(uid) || IS_EPHEMERAL(gid)))
		return (SET_ERROR(EINVAL));
	if ((error = zfs_enter_verify_zp(zfsvfs, dzp, FTAG)) != 0)
		return (error);
	os = zfsvfs->z_os;
	if (vap->va_mask & ATTR_XVATTR) {
		if ((error = secpolicy_xvattr((xvattr_t *)vap,
		    crgetuid(cr), cr, vap->va_mode)) != 0) {
			zfs_exit(zfsvfs, FTAG);
			return (error);
		}
	}
top:
	*ipp = NULL;
	if ((error = zfs_zaccess(dzp, ACE_ADD_FILE, 0, B_FALSE, cr, mnt_ns))) {
		if (have_acl)
			zfs_acl_ids_free(&acl_ids);
		goto out;
	}
	if (!have_acl && (error = zfs_acl_ids_create(dzp, 0, vap,
	    cr, vsecp, &acl_ids, mnt_ns)) != 0)
		goto out;
	have_acl = B_TRUE;
	if (S_ISREG(vap->va_mode) || S_ISDIR(vap->va_mode))
		projid = zfs_inherit_projid(dzp);
	if (zfs_acl_ids_overquota(zfsvfs, &acl_ids, projid)) {
		zfs_acl_ids_free(&acl_ids);
		error = SET_ERROR(EDQUOT);
		goto out;
	}
	tx = dmu_tx_create(os);
	dmu_tx_hold_sa_create(tx, acl_ids.z_aclp->z_acl_bytes +
	    ZFS_SA_BASE_ATTR_SIZE);
	dmu_tx_hold_zap(tx, zfsvfs->z_unlinkedobj, FALSE, NULL);
	fuid_dirtied = zfsvfs->z_fuid_dirty;
	if (fuid_dirtied)
		zfs_fuid_txhold(zfsvfs, tx);
	if (!zfsvfs->z_use_sa &&
	    acl_ids.z_aclp->z_acl_bytes > ZFS_ACE_SPACE) {
		dmu_tx_hold_write(tx, DMU_NEW_OBJECT,
		    0, acl_ids.z_aclp->z_acl_bytes);
	}
	error = dmu_tx_assign(tx, (waited ? TXG_NOTHROTTLE : 0) | TXG_NOWAIT);
	if (error) {
		if (error == ERESTART) {
			waited = B_TRUE;
			dmu_tx_wait(tx);
			dmu_tx_abort(tx);
			goto top;
		}
		zfs_acl_ids_free(&acl_ids);
		dmu_tx_abort(tx);
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	zfs_mknode(dzp, vap, tx, cr, IS_TMPFILE, &zp, &acl_ids);
	if (fuid_dirtied)
		zfs_fuid_sync(zfsvfs, tx);
	zp->z_unlinked = B_TRUE;
	zfs_unlinked_add(zp, tx);
	zfs_acl_ids_free(&acl_ids);
	dmu_tx_commit(tx);
out:
	if (error) {
		if (zp)
			zrele(zp);
	} else {
		zfs_znode_update_vfs(dzp);
		zfs_znode_update_vfs(zp);
		*ipp = ZTOI(zp);
	}
	zfs_exit(zfsvfs, FTAG);
	return (error);
}
static uint64_t null_xattr = 0;
int
zfs_remove(znode_t *dzp, char *name, cred_t *cr, int flags)
{
	znode_t		*zp;
	znode_t		*xzp;
	zfsvfs_t	*zfsvfs = ZTOZSB(dzp);
	zilog_t		*zilog;
	uint64_t	acl_obj, xattr_obj;
	uint64_t	xattr_obj_unlinked = 0;
	uint64_t	obj = 0;
	uint64_t	links;
	zfs_dirlock_t	*dl;
	dmu_tx_t	*tx;
	boolean_t	may_delete_now, delete_now = FALSE;
	boolean_t	unlinked, toobig = FALSE;
	uint64_t	txtype;
	pathname_t	*realnmp = NULL;
	pathname_t	realnm;
	int		error;
	int		zflg = ZEXISTS;
	boolean_t	waited = B_FALSE;
	if (name == NULL)
		return (SET_ERROR(EINVAL));
	if ((error = zfs_enter_verify_zp(zfsvfs, dzp, FTAG)) != 0)
		return (error);
	zilog = zfsvfs->z_log;
	if (flags & FIGNORECASE) {
		zflg |= ZCILOOK;
		pn_alloc(&realnm);
		realnmp = &realnm;
	}
top:
	xattr_obj = 0;
	xzp = NULL;
	if ((error = zfs_dirent_lock(&dl, dzp, name, &zp, zflg,
	    NULL, realnmp))) {
		if (realnmp)
			pn_free(realnmp);
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	if ((error = zfs_zaccess_delete(dzp, zp, cr, zfs_init_idmap))) {
		goto out;
	}
	if (S_ISDIR(ZTOI(zp)->i_mode)) {
		error = SET_ERROR(EPERM);
		goto out;
	}
	mutex_enter(&zp->z_lock);
	may_delete_now = atomic_read(&ZTOI(zp)->i_count) == 1 &&
	    !zn_has_cached_data(zp, 0, LLONG_MAX);
	mutex_exit(&zp->z_lock);
	obj = zp->z_id;
	tx = dmu_tx_create(zfsvfs->z_os);
	dmu_tx_hold_zap(tx, dzp->z_id, FALSE, name);
	dmu_tx_hold_sa(tx, zp->z_sa_hdl, B_FALSE);
	zfs_sa_upgrade_txholds(tx, zp);
	zfs_sa_upgrade_txholds(tx, dzp);
	if (may_delete_now) {
		toobig = zp->z_size > zp->z_blksz * zfs_delete_blocks;
		dmu_tx_hold_free(tx, zp->z_id, 0,
		    (toobig ? DMU_MAX_ACCESS : DMU_OBJECT_END));
	}
	error = sa_lookup(zp->z_sa_hdl, SA_ZPL_XATTR(zfsvfs),
	    &xattr_obj, sizeof (xattr_obj));
	if (error == 0 && xattr_obj) {
		error = zfs_zget(zfsvfs, xattr_obj, &xzp);
		ASSERT0(error);
		dmu_tx_hold_sa(tx, zp->z_sa_hdl, B_TRUE);
		dmu_tx_hold_sa(tx, xzp->z_sa_hdl, B_FALSE);
	}
	mutex_enter(&zp->z_lock);
	if ((acl_obj = zfs_external_acl(zp)) != 0 && may_delete_now)
		dmu_tx_hold_free(tx, acl_obj, 0, DMU_OBJECT_END);
	mutex_exit(&zp->z_lock);
	dmu_tx_hold_zap(tx, zfsvfs->z_unlinkedobj, FALSE, NULL);
	dmu_tx_mark_netfree(tx);
	error = dmu_tx_assign(tx, (waited ? TXG_NOTHROTTLE : 0) | TXG_NOWAIT);
	if (error) {
		zfs_dirent_unlock(dl);
		if (error == ERESTART) {
			waited = B_TRUE;
			dmu_tx_wait(tx);
			dmu_tx_abort(tx);
			zrele(zp);
			if (xzp)
				zrele(xzp);
			goto top;
		}
		if (realnmp)
			pn_free(realnmp);
		dmu_tx_abort(tx);
		zrele(zp);
		if (xzp)
			zrele(xzp);
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	error = zfs_link_destroy(dl, zp, tx, zflg, &unlinked);
	if (error) {
		dmu_tx_commit(tx);
		goto out;
	}
	if (unlinked) {
		mutex_enter(&zp->z_lock);
		(void) sa_lookup(zp->z_sa_hdl, SA_ZPL_XATTR(zfsvfs),
		    &xattr_obj_unlinked, sizeof (xattr_obj_unlinked));
		delete_now = may_delete_now && !toobig &&
		    atomic_read(&ZTOI(zp)->i_count) == 1 &&
		    !zn_has_cached_data(zp, 0, LLONG_MAX) &&
		    xattr_obj == xattr_obj_unlinked &&
		    zfs_external_acl(zp) == acl_obj;
		VERIFY_IMPLY(xattr_obj_unlinked, xzp);
	}
	if (delete_now) {
		if (xattr_obj_unlinked) {
			ASSERT3U(ZTOI(xzp)->i_nlink, ==, 2);
			mutex_enter(&xzp->z_lock);
			xzp->z_unlinked = B_TRUE;
			clear_nlink(ZTOI(xzp));
			links = 0;
			error = sa_update(xzp->z_sa_hdl, SA_ZPL_LINKS(zfsvfs),
			    &links, sizeof (links), tx);
			ASSERT3U(error,  ==,  0);
			mutex_exit(&xzp->z_lock);
			zfs_unlinked_add(xzp, tx);
			if (zp->z_is_sa)
				error = sa_remove(zp->z_sa_hdl,
				    SA_ZPL_XATTR(zfsvfs), tx);
			else
				error = sa_update(zp->z_sa_hdl,
				    SA_ZPL_XATTR(zfsvfs), &null_xattr,
				    sizeof (uint64_t), tx);
			ASSERT0(error);
		}
		zfs_unlinked_add(zp, tx);
		mutex_exit(&zp->z_lock);
	} else if (unlinked) {
		mutex_exit(&zp->z_lock);
		zfs_unlinked_add(zp, tx);
	}
	txtype = TX_REMOVE;
	if (flags & FIGNORECASE)
		txtype |= TX_CI;
	zfs_log_remove(zilog, tx, txtype, dzp, name, obj, unlinked);
	dmu_tx_commit(tx);
out:
	if (realnmp)
		pn_free(realnmp);
	zfs_dirent_unlock(dl);
	zfs_znode_update_vfs(dzp);
	zfs_znode_update_vfs(zp);
	if (delete_now)
		zrele(zp);
	else
		zfs_zrele_async(zp);
	if (xzp) {
		zfs_znode_update_vfs(xzp);
		zfs_zrele_async(xzp);
	}
	if (zfsvfs->z_os->os_sync == ZFS_SYNC_ALWAYS)
		zil_commit(zilog, 0);
	zfs_exit(zfsvfs, FTAG);
	return (error);
}
int
zfs_mkdir(znode_t *dzp, char *dirname, vattr_t *vap, znode_t **zpp,
    cred_t *cr, int flags, vsecattr_t *vsecp, zidmap_t *mnt_ns)
{
	znode_t		*zp;
	zfsvfs_t	*zfsvfs = ZTOZSB(dzp);
	zilog_t		*zilog;
	zfs_dirlock_t	*dl;
	uint64_t	txtype;
	dmu_tx_t	*tx;
	int		error;
	int		zf = ZNEW;
	uid_t		uid;
	gid_t		gid = crgetgid(cr);
	zfs_acl_ids_t   acl_ids;
	boolean_t	fuid_dirtied;
	boolean_t	waited = B_FALSE;
	ASSERT(S_ISDIR(vap->va_mode));
	uid = crgetuid(cr);
	if (zfsvfs->z_use_fuids == B_FALSE &&
	    (vsecp || IS_EPHEMERAL(uid) || IS_EPHEMERAL(gid)))
		return (SET_ERROR(EINVAL));
	if (dirname == NULL)
		return (SET_ERROR(EINVAL));
	if ((error = zfs_enter_verify_zp(zfsvfs, dzp, FTAG)) != 0)
		return (error);
	zilog = zfsvfs->z_log;
	if (dzp->z_pflags & ZFS_XATTR) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EINVAL));
	}
	if (zfsvfs->z_utf8 && u8_validate(dirname,
	    strlen(dirname), NULL, U8_VALIDATE_ENTIRE, &error) < 0) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EILSEQ));
	}
	if (flags & FIGNORECASE)
		zf |= ZCILOOK;
	if (vap->va_mask & ATTR_XVATTR) {
		if ((error = secpolicy_xvattr((xvattr_t *)vap,
		    crgetuid(cr), cr, vap->va_mode)) != 0) {
			zfs_exit(zfsvfs, FTAG);
			return (error);
		}
	}
	if ((error = zfs_acl_ids_create(dzp, 0, vap, cr,
	    vsecp, &acl_ids, mnt_ns)) != 0) {
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
top:
	*zpp = NULL;
	if ((error = zfs_dirent_lock(&dl, dzp, dirname, &zp, zf,
	    NULL, NULL))) {
		zfs_acl_ids_free(&acl_ids);
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	if ((error = zfs_zaccess(dzp, ACE_ADD_SUBDIRECTORY, 0, B_FALSE, cr,
	    mnt_ns))) {
		zfs_acl_ids_free(&acl_ids);
		zfs_dirent_unlock(dl);
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	if (zfs_acl_ids_overquota(zfsvfs, &acl_ids, zfs_inherit_projid(dzp))) {
		zfs_acl_ids_free(&acl_ids);
		zfs_dirent_unlock(dl);
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EDQUOT));
	}
	tx = dmu_tx_create(zfsvfs->z_os);
	dmu_tx_hold_zap(tx, dzp->z_id, TRUE, dirname);
	dmu_tx_hold_zap(tx, DMU_NEW_OBJECT, FALSE, NULL);
	fuid_dirtied = zfsvfs->z_fuid_dirty;
	if (fuid_dirtied)
		zfs_fuid_txhold(zfsvfs, tx);
	if (!zfsvfs->z_use_sa && acl_ids.z_aclp->z_acl_bytes > ZFS_ACE_SPACE) {
		dmu_tx_hold_write(tx, DMU_NEW_OBJECT, 0,
		    acl_ids.z_aclp->z_acl_bytes);
	}
	dmu_tx_hold_sa_create(tx, acl_ids.z_aclp->z_acl_bytes +
	    ZFS_SA_BASE_ATTR_SIZE);
	error = dmu_tx_assign(tx, (waited ? TXG_NOTHROTTLE : 0) | TXG_NOWAIT);
	if (error) {
		zfs_dirent_unlock(dl);
		if (error == ERESTART) {
			waited = B_TRUE;
			dmu_tx_wait(tx);
			dmu_tx_abort(tx);
			goto top;
		}
		zfs_acl_ids_free(&acl_ids);
		dmu_tx_abort(tx);
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	zfs_mknode(dzp, vap, tx, cr, 0, &zp, &acl_ids);
	error = zfs_link_create(dl, zp, tx, ZNEW);
	if (error != 0) {
		zfs_znode_delete(zp, tx);
		remove_inode_hash(ZTOI(zp));
		goto out;
	}
	if (fuid_dirtied)
		zfs_fuid_sync(zfsvfs, tx);
	*zpp = zp;
	txtype = zfs_log_create_txtype(Z_DIR, vsecp, vap);
	if (flags & FIGNORECASE)
		txtype |= TX_CI;
	zfs_log_create(zilog, tx, txtype, dzp, zp, dirname, vsecp,
	    acl_ids.z_fuidp, vap);
out:
	zfs_acl_ids_free(&acl_ids);
	dmu_tx_commit(tx);
	zfs_dirent_unlock(dl);
	if (zfsvfs->z_os->os_sync == ZFS_SYNC_ALWAYS)
		zil_commit(zilog, 0);
	if (error != 0) {
		zrele(zp);
	} else {
		zfs_znode_update_vfs(dzp);
		zfs_znode_update_vfs(zp);
	}
	zfs_exit(zfsvfs, FTAG);
	return (error);
}
int
zfs_rmdir(znode_t *dzp, char *name, znode_t *cwd, cred_t *cr,
    int flags)
{
	znode_t		*zp;
	zfsvfs_t	*zfsvfs = ZTOZSB(dzp);
	zilog_t		*zilog;
	zfs_dirlock_t	*dl;
	dmu_tx_t	*tx;
	int		error;
	int		zflg = ZEXISTS;
	boolean_t	waited = B_FALSE;
	if (name == NULL)
		return (SET_ERROR(EINVAL));
	if ((error = zfs_enter_verify_zp(zfsvfs, dzp, FTAG)) != 0)
		return (error);
	zilog = zfsvfs->z_log;
	if (flags & FIGNORECASE)
		zflg |= ZCILOOK;
top:
	zp = NULL;
	if ((error = zfs_dirent_lock(&dl, dzp, name, &zp, zflg,
	    NULL, NULL))) {
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	if ((error = zfs_zaccess_delete(dzp, zp, cr, zfs_init_idmap))) {
		goto out;
	}
	if (!S_ISDIR(ZTOI(zp)->i_mode)) {
		error = SET_ERROR(ENOTDIR);
		goto out;
	}
	if (zp == cwd) {
		error = SET_ERROR(EINVAL);
		goto out;
	}
	rw_enter(&zp->z_name_lock, RW_WRITER);
	rw_enter(&zp->z_parent_lock, RW_WRITER);
	tx = dmu_tx_create(zfsvfs->z_os);
	dmu_tx_hold_zap(tx, dzp->z_id, FALSE, name);
	dmu_tx_hold_sa(tx, zp->z_sa_hdl, B_FALSE);
	dmu_tx_hold_zap(tx, zfsvfs->z_unlinkedobj, FALSE, NULL);
	zfs_sa_upgrade_txholds(tx, zp);
	zfs_sa_upgrade_txholds(tx, dzp);
	dmu_tx_mark_netfree(tx);
	error = dmu_tx_assign(tx, (waited ? TXG_NOTHROTTLE : 0) | TXG_NOWAIT);
	if (error) {
		rw_exit(&zp->z_parent_lock);
		rw_exit(&zp->z_name_lock);
		zfs_dirent_unlock(dl);
		if (error == ERESTART) {
			waited = B_TRUE;
			dmu_tx_wait(tx);
			dmu_tx_abort(tx);
			zrele(zp);
			goto top;
		}
		dmu_tx_abort(tx);
		zrele(zp);
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	error = zfs_link_destroy(dl, zp, tx, zflg, NULL);
	if (error == 0) {
		uint64_t txtype = TX_RMDIR;
		if (flags & FIGNORECASE)
			txtype |= TX_CI;
		zfs_log_remove(zilog, tx, txtype, dzp, name, ZFS_NO_OBJECT,
		    B_FALSE);
	}
	dmu_tx_commit(tx);
	rw_exit(&zp->z_parent_lock);
	rw_exit(&zp->z_name_lock);
out:
	zfs_dirent_unlock(dl);
	zfs_znode_update_vfs(dzp);
	zfs_znode_update_vfs(zp);
	zrele(zp);
	if (zfsvfs->z_os->os_sync == ZFS_SYNC_ALWAYS)
		zil_commit(zilog, 0);
	zfs_exit(zfsvfs, FTAG);
	return (error);
}
int
zfs_readdir(struct inode *ip, zpl_dir_context_t *ctx, cred_t *cr)
{
	(void) cr;
	znode_t		*zp = ITOZ(ip);
	zfsvfs_t	*zfsvfs = ITOZSB(ip);
	objset_t	*os;
	zap_cursor_t	zc;
	zap_attribute_t	zap;
	int		error;
	uint8_t		prefetch;
	uint8_t		type;
	int		done = 0;
	uint64_t	parent;
	uint64_t	offset;  
	if ((error = zfs_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		return (error);
	if ((error = sa_lookup(zp->z_sa_hdl, SA_ZPL_PARENT(zfsvfs),
	    &parent, sizeof (parent))) != 0)
		goto out;
	if (zp->z_unlinked)
		goto out;
	error = 0;
	os = zfsvfs->z_os;
	offset = ctx->pos;
	prefetch = zp->z_zn_prefetch;
	if (offset <= 3) {
		zap_cursor_init(&zc, os, zp->z_id);
	} else {
		zap_cursor_init_serialized(&zc, os, zp->z_id, offset);
	}
	while (!done) {
		uint64_t objnum;
		if (offset == 0) {
			(void) strcpy(zap.za_name, ".");
			zap.za_normalization_conflict = 0;
			objnum = zp->z_id;
			type = DT_DIR;
		} else if (offset == 1) {
			(void) strcpy(zap.za_name, "..");
			zap.za_normalization_conflict = 0;
			objnum = parent;
			type = DT_DIR;
		} else if (offset == 2 && zfs_show_ctldir(zp)) {
			(void) strcpy(zap.za_name, ZFS_CTLDIR_NAME);
			zap.za_normalization_conflict = 0;
			objnum = ZFSCTL_INO_ROOT;
			type = DT_DIR;
		} else {
			if ((error = zap_cursor_retrieve(&zc, &zap))) {
				if (error == ENOENT)
					break;
				else
					goto update;
			}
			if (zap.za_integer_length != 8 ||
			    zap.za_num_integers == 0) {
				cmn_err(CE_WARN, "zap_readdir: bad directory "
				    "entry, obj = %lld, offset = %lld, "
				    "length = %d, num = %lld\n",
				    (u_longlong_t)zp->z_id,
				    (u_longlong_t)offset,
				    zap.za_integer_length,
				    (u_longlong_t)zap.za_num_integers);
				error = SET_ERROR(ENXIO);
				goto update;
			}
			objnum = ZFS_DIRENT_OBJ(zap.za_first_integer);
			type = ZFS_DIRENT_TYPE(zap.za_first_integer);
		}
		done = !zpl_dir_emit(ctx, zap.za_name, strlen(zap.za_name),
		    objnum, type);
		if (done)
			break;
		if (prefetch) {
			dmu_prefetch(os, objnum, 0, 0, 0,
			    ZIO_PRIORITY_SYNC_READ);
		}
		if (offset > 2 || (offset == 2 && !zfs_show_ctldir(zp))) {
			zap_cursor_advance(&zc);
			offset = zap_cursor_serialize(&zc);
		} else {
			offset += 1;
		}
		ctx->pos = offset;
	}
	zp->z_zn_prefetch = B_FALSE;  
update:
	zap_cursor_fini(&zc);
	if (error == ENOENT)
		error = 0;
out:
	zfs_exit(zfsvfs, FTAG);
	return (error);
}
int
#ifdef HAVE_GENERIC_FILLATTR_IDMAP_REQMASK
zfs_getattr_fast(zidmap_t *user_ns, u32 request_mask, struct inode *ip,
    struct kstat *sp)
#else
zfs_getattr_fast(zidmap_t *user_ns, struct inode *ip, struct kstat *sp)
#endif
{
	znode_t *zp = ITOZ(ip);
	zfsvfs_t *zfsvfs = ITOZSB(ip);
	uint32_t blksize;
	u_longlong_t nblocks;
	int error;
	if ((error = zfs_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		return (error);
	mutex_enter(&zp->z_lock);
#ifdef HAVE_GENERIC_FILLATTR_IDMAP_REQMASK
	zpl_generic_fillattr(user_ns, request_mask, ip, sp);
#else
	zpl_generic_fillattr(user_ns, ip, sp);
#endif
	if ((zp->z_id == zfsvfs->z_root) && zfs_show_ctldir(zp))
		if (sp->nlink < ZFS_LINK_MAX)
			sp->nlink++;
	sa_object_size(zp->z_sa_hdl, &blksize, &nblocks);
	sp->blksize = blksize;
	sp->blocks = nblocks;
	if (unlikely(zp->z_blksz == 0)) {
		sp->blksize = zfsvfs->z_max_blksz;
	}
	mutex_exit(&zp->z_lock);
	if (zfsvfs->z_issnap) {
		if (ip->i_sb->s_root->d_inode == ip)
			sp->ino = ZFSCTL_INO_SNAPDIRS -
			    dmu_objset_id(zfsvfs->z_os);
	}
	zfs_exit(zfsvfs, FTAG);
	return (0);
}
static int
zfs_setattr_dir(znode_t *dzp)
{
	struct inode	*dxip = ZTOI(dzp);
	struct inode	*xip = NULL;
	zfsvfs_t	*zfsvfs = ZTOZSB(dzp);
	objset_t	*os = zfsvfs->z_os;
	zap_cursor_t	zc;
	zap_attribute_t	zap;
	zfs_dirlock_t	*dl;
	znode_t		*zp = NULL;
	dmu_tx_t	*tx = NULL;
	uint64_t	uid, gid;
	sa_bulk_attr_t	bulk[4];
	int		count;
	int		err;
	zap_cursor_init(&zc, os, dzp->z_id);
	while ((err = zap_cursor_retrieve(&zc, &zap)) == 0) {
		count = 0;
		if (zap.za_integer_length != 8 || zap.za_num_integers != 1) {
			err = ENXIO;
			break;
		}
		err = zfs_dirent_lock(&dl, dzp, (char *)zap.za_name, &zp,
		    ZEXISTS, NULL, NULL);
		if (err == ENOENT)
			goto next;
		if (err)
			break;
		xip = ZTOI(zp);
		if (KUID_TO_SUID(xip->i_uid) == KUID_TO_SUID(dxip->i_uid) &&
		    KGID_TO_SGID(xip->i_gid) == KGID_TO_SGID(dxip->i_gid) &&
		    zp->z_projid == dzp->z_projid)
			goto next;
		tx = dmu_tx_create(os);
		if (!(zp->z_pflags & ZFS_PROJID))
			dmu_tx_hold_sa(tx, zp->z_sa_hdl, B_TRUE);
		else
			dmu_tx_hold_sa(tx, zp->z_sa_hdl, B_FALSE);
		err = dmu_tx_assign(tx, TXG_WAIT);
		if (err)
			break;
		mutex_enter(&dzp->z_lock);
		if (KUID_TO_SUID(xip->i_uid) != KUID_TO_SUID(dxip->i_uid)) {
			xip->i_uid = dxip->i_uid;
			uid = zfs_uid_read(dxip);
			SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_UID(zfsvfs), NULL,
			    &uid, sizeof (uid));
		}
		if (KGID_TO_SGID(xip->i_gid) != KGID_TO_SGID(dxip->i_gid)) {
			xip->i_gid = dxip->i_gid;
			gid = zfs_gid_read(dxip);
			SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_GID(zfsvfs), NULL,
			    &gid, sizeof (gid));
		}
		if (zp->z_projid != dzp->z_projid) {
			if (!(zp->z_pflags & ZFS_PROJID)) {
				zp->z_pflags |= ZFS_PROJID;
				SA_ADD_BULK_ATTR(bulk, count,
				    SA_ZPL_FLAGS(zfsvfs), NULL, &zp->z_pflags,
				    sizeof (zp->z_pflags));
			}
			zp->z_projid = dzp->z_projid;
			SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_PROJID(zfsvfs),
			    NULL, &zp->z_projid, sizeof (zp->z_projid));
		}
		mutex_exit(&dzp->z_lock);
		if (likely(count > 0)) {
			err = sa_bulk_update(zp->z_sa_hdl, bulk, count, tx);
			dmu_tx_commit(tx);
		} else {
			dmu_tx_abort(tx);
		}
		tx = NULL;
		if (err != 0 && err != ENOENT)
			break;
next:
		if (zp) {
			zrele(zp);
			zp = NULL;
			zfs_dirent_unlock(dl);
		}
		zap_cursor_advance(&zc);
	}
	if (tx)
		dmu_tx_abort(tx);
	if (zp) {
		zrele(zp);
		zfs_dirent_unlock(dl);
	}
	zap_cursor_fini(&zc);
	return (err == ENOENT ? 0 : err);
}
int
zfs_setattr(znode_t *zp, vattr_t *vap, int flags, cred_t *cr, zidmap_t *mnt_ns)
{
	struct inode	*ip;
	zfsvfs_t	*zfsvfs = ZTOZSB(zp);
	objset_t	*os = zfsvfs->z_os;
	zilog_t		*zilog;
	dmu_tx_t	*tx;
	vattr_t		oldva;
	xvattr_t	*tmpxvattr;
	uint_t		mask = vap->va_mask;
	uint_t		saved_mask = 0;
	int		trim_mask = 0;
	uint64_t	new_mode;
	uint64_t	new_kuid = 0, new_kgid = 0, new_uid, new_gid;
	uint64_t	xattr_obj;
	uint64_t	mtime[2], ctime[2], atime[2];
	uint64_t	projid = ZFS_INVALID_PROJID;
	znode_t		*attrzp;
	int		need_policy = FALSE;
	int		err, err2 = 0;
	zfs_fuid_info_t *fuidp = NULL;
	xvattr_t *xvap = (xvattr_t *)vap;	 
	xoptattr_t	*xoap;
	zfs_acl_t	*aclp;
	boolean_t skipaclchk = (flags & ATTR_NOACLCHECK) ? B_TRUE : B_FALSE;
	boolean_t	fuid_dirtied = B_FALSE;
	boolean_t	handle_eadir = B_FALSE;
	sa_bulk_attr_t	*bulk, *xattr_bulk;
	int		count = 0, xattr_count = 0, bulks = 8;
	if (mask == 0)
		return (0);
	if ((err = zfs_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		return (err);
	ip = ZTOI(zp);
	xoap = xva_getxoptattr(xvap);
	if (xoap != NULL && (mask & ATTR_XVATTR)) {
		if (XVA_ISSET_REQ(xvap, XAT_PROJID)) {
			if (!dmu_objset_projectquota_enabled(os) ||
			    (!S_ISREG(ip->i_mode) && !S_ISDIR(ip->i_mode))) {
				zfs_exit(zfsvfs, FTAG);
				return (SET_ERROR(ENOTSUP));
			}
			projid = xoap->xoa_projid;
			if (unlikely(projid == ZFS_INVALID_PROJID)) {
				zfs_exit(zfsvfs, FTAG);
				return (SET_ERROR(EINVAL));
			}
			if (projid == zp->z_projid && zp->z_pflags & ZFS_PROJID)
				projid = ZFS_INVALID_PROJID;
			else
				need_policy = TRUE;
		}
		if (XVA_ISSET_REQ(xvap, XAT_PROJINHERIT) &&
		    (xoap->xoa_projinherit !=
		    ((zp->z_pflags & ZFS_PROJINHERIT) != 0)) &&
		    (!dmu_objset_projectquota_enabled(os) ||
		    (!S_ISREG(ip->i_mode) && !S_ISDIR(ip->i_mode)))) {
			zfs_exit(zfsvfs, FTAG);
			return (SET_ERROR(ENOTSUP));
		}
	}
	zilog = zfsvfs->z_log;
	if (zfsvfs->z_use_fuids == B_FALSE &&
	    (((mask & ATTR_UID) && IS_EPHEMERAL(vap->va_uid)) ||
	    ((mask & ATTR_GID) && IS_EPHEMERAL(vap->va_gid)) ||
	    (mask & ATTR_XVATTR))) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EINVAL));
	}
	if (mask & ATTR_SIZE && S_ISDIR(ip->i_mode)) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EISDIR));
	}
	if (mask & ATTR_SIZE && !S_ISREG(ip->i_mode) && !S_ISFIFO(ip->i_mode)) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EINVAL));
	}
	tmpxvattr = kmem_alloc(sizeof (xvattr_t), KM_SLEEP);
	xva_init(tmpxvattr);
	bulk = kmem_alloc(sizeof (sa_bulk_attr_t) * bulks, KM_SLEEP);
	xattr_bulk = kmem_alloc(sizeof (sa_bulk_attr_t) * bulks, KM_SLEEP);
	if ((zp->z_pflags & ZFS_IMMUTABLE) &&
	    ((mask & (ATTR_SIZE|ATTR_UID|ATTR_GID|ATTR_MTIME|ATTR_MODE)) ||
	    ((mask & ATTR_XVATTR) && XVA_ISSET_REQ(xvap, XAT_CREATETIME)))) {
		err = SET_ERROR(EPERM);
		goto out3;
	}
	if ((mask & ATTR_SIZE) && (zp->z_pflags & ZFS_READONLY)) {
		err = SET_ERROR(EPERM);
		goto out3;
	}
	if (mask & (ATTR_ATIME | ATTR_MTIME)) {
		if (((mask & ATTR_ATIME) &&
		    TIMESPEC_OVERFLOW(&vap->va_atime)) ||
		    ((mask & ATTR_MTIME) &&
		    TIMESPEC_OVERFLOW(&vap->va_mtime))) {
			err = SET_ERROR(EOVERFLOW);
			goto out3;
		}
	}
top:
	attrzp = NULL;
	aclp = NULL;
	if (zfs_is_readonly(zfsvfs)) {
		err = SET_ERROR(EROFS);
		goto out3;
	}
	if (mask & ATTR_SIZE) {
		err = zfs_zaccess(zp, ACE_WRITE_DATA, 0, skipaclchk, cr,
		    mnt_ns);
		if (err)
			goto out3;
		err = zfs_freesp(zp, vap->va_size, 0, 0, FALSE);
		if (err)
			goto out3;
	}
	if (mask & (ATTR_ATIME|ATTR_MTIME) ||
	    ((mask & ATTR_XVATTR) && (XVA_ISSET_REQ(xvap, XAT_HIDDEN) ||
	    XVA_ISSET_REQ(xvap, XAT_READONLY) ||
	    XVA_ISSET_REQ(xvap, XAT_ARCHIVE) ||
	    XVA_ISSET_REQ(xvap, XAT_OFFLINE) ||
	    XVA_ISSET_REQ(xvap, XAT_SPARSE) ||
	    XVA_ISSET_REQ(xvap, XAT_CREATETIME) ||
	    XVA_ISSET_REQ(xvap, XAT_SYSTEM)))) {
		need_policy = zfs_zaccess(zp, ACE_WRITE_ATTRIBUTES, 0,
		    skipaclchk, cr, mnt_ns);
	}
	if (mask & (ATTR_UID|ATTR_GID)) {
		int	idmask = (mask & (ATTR_UID|ATTR_GID));
		int	take_owner;
		int	take_group;
		uid_t	uid;
		gid_t	gid;
		if (!(mask & ATTR_MODE))
			vap->va_mode = zp->z_mode;
		uid = zfs_uid_to_vfsuid(mnt_ns, zfs_i_user_ns(ip),
		    vap->va_uid);
		gid = zfs_gid_to_vfsgid(mnt_ns, zfs_i_user_ns(ip),
		    vap->va_gid);
		take_owner = (mask & ATTR_UID) && (uid == crgetuid(cr));
		take_group = (mask & ATTR_GID) &&
		    zfs_groupmember(zfsvfs, gid, cr);
		if (((idmask == (ATTR_UID|ATTR_GID)) &&
		    take_owner && take_group) ||
		    ((idmask == ATTR_UID) && take_owner) ||
		    ((idmask == ATTR_GID) && take_group)) {
			if (zfs_zaccess(zp, ACE_WRITE_OWNER, 0,
			    skipaclchk, cr, mnt_ns) == 0) {
				(void) secpolicy_setid_clear(vap, cr);
				trim_mask = (mask & (ATTR_UID|ATTR_GID));
			} else {
				need_policy =  TRUE;
			}
		} else {
			need_policy =  TRUE;
		}
	}
	mutex_enter(&zp->z_lock);
	oldva.va_mode = zp->z_mode;
	zfs_fuid_map_ids(zp, cr, &oldva.va_uid, &oldva.va_gid);
	if (mask & ATTR_XVATTR) {
		if (XVA_ISSET_REQ(xvap, XAT_APPENDONLY)) {
			if (xoap->xoa_appendonly !=
			    ((zp->z_pflags & ZFS_APPENDONLY) != 0)) {
				need_policy = TRUE;
			} else {
				XVA_CLR_REQ(xvap, XAT_APPENDONLY);
				XVA_SET_REQ(tmpxvattr, XAT_APPENDONLY);
			}
		}
		if (XVA_ISSET_REQ(xvap, XAT_PROJINHERIT)) {
			if (xoap->xoa_projinherit !=
			    ((zp->z_pflags & ZFS_PROJINHERIT) != 0)) {
				need_policy = TRUE;
			} else {
				XVA_CLR_REQ(xvap, XAT_PROJINHERIT);
				XVA_SET_REQ(tmpxvattr, XAT_PROJINHERIT);
			}
		}
		if (XVA_ISSET_REQ(xvap, XAT_NOUNLINK)) {
			if (xoap->xoa_nounlink !=
			    ((zp->z_pflags & ZFS_NOUNLINK) != 0)) {
				need_policy = TRUE;
			} else {
				XVA_CLR_REQ(xvap, XAT_NOUNLINK);
				XVA_SET_REQ(tmpxvattr, XAT_NOUNLINK);
			}
		}
		if (XVA_ISSET_REQ(xvap, XAT_IMMUTABLE)) {
			if (xoap->xoa_immutable !=
			    ((zp->z_pflags & ZFS_IMMUTABLE) != 0)) {
				need_policy = TRUE;
			} else {
				XVA_CLR_REQ(xvap, XAT_IMMUTABLE);
				XVA_SET_REQ(tmpxvattr, XAT_IMMUTABLE);
			}
		}
		if (XVA_ISSET_REQ(xvap, XAT_NODUMP)) {
			if (xoap->xoa_nodump !=
			    ((zp->z_pflags & ZFS_NODUMP) != 0)) {
				need_policy = TRUE;
			} else {
				XVA_CLR_REQ(xvap, XAT_NODUMP);
				XVA_SET_REQ(tmpxvattr, XAT_NODUMP);
			}
		}
		if (XVA_ISSET_REQ(xvap, XAT_AV_MODIFIED)) {
			if (xoap->xoa_av_modified !=
			    ((zp->z_pflags & ZFS_AV_MODIFIED) != 0)) {
				need_policy = TRUE;
			} else {
				XVA_CLR_REQ(xvap, XAT_AV_MODIFIED);
				XVA_SET_REQ(tmpxvattr, XAT_AV_MODIFIED);
			}
		}
		if (XVA_ISSET_REQ(xvap, XAT_AV_QUARANTINED)) {
			if ((!S_ISREG(ip->i_mode) &&
			    xoap->xoa_av_quarantined) ||
			    xoap->xoa_av_quarantined !=
			    ((zp->z_pflags & ZFS_AV_QUARANTINED) != 0)) {
				need_policy = TRUE;
			} else {
				XVA_CLR_REQ(xvap, XAT_AV_QUARANTINED);
				XVA_SET_REQ(tmpxvattr, XAT_AV_QUARANTINED);
			}
		}
		if (XVA_ISSET_REQ(xvap, XAT_REPARSE)) {
			mutex_exit(&zp->z_lock);
			err = SET_ERROR(EPERM);
			goto out3;
		}
		if (need_policy == FALSE &&
		    (XVA_ISSET_REQ(xvap, XAT_AV_SCANSTAMP) ||
		    XVA_ISSET_REQ(xvap, XAT_OPAQUE))) {
			need_policy = TRUE;
		}
	}
	mutex_exit(&zp->z_lock);
	if (mask & ATTR_MODE) {
		if (zfs_zaccess(zp, ACE_WRITE_ACL, 0, skipaclchk, cr,
		    mnt_ns) == 0) {
			err = secpolicy_setid_setsticky_clear(ip, vap,
			    &oldva, cr, mnt_ns, zfs_i_user_ns(ip));
			if (err)
				goto out3;
			trim_mask |= ATTR_MODE;
		} else {
			need_policy = TRUE;
		}
	}
	if (need_policy) {
		if (trim_mask) {
			saved_mask = vap->va_mask;
			vap->va_mask &= ~trim_mask;
		}
		err = secpolicy_vnode_setattr(cr, ip, vap, &oldva, flags,
		    zfs_zaccess_unix, zp);
		if (err)
			goto out3;
		if (trim_mask)
			vap->va_mask |= saved_mask;
	}
	mask = vap->va_mask;
	if ((mask & (ATTR_UID | ATTR_GID)) || projid != ZFS_INVALID_PROJID) {
		handle_eadir = B_TRUE;
		err = sa_lookup(zp->z_sa_hdl, SA_ZPL_XATTR(zfsvfs),
		    &xattr_obj, sizeof (xattr_obj));
		if (err == 0 && xattr_obj) {
			err = zfs_zget(ZTOZSB(zp), xattr_obj, &attrzp);
			if (err)
				goto out2;
		}
		if (mask & ATTR_UID) {
			new_kuid = zfs_fuid_create(zfsvfs,
			    (uint64_t)vap->va_uid, cr, ZFS_OWNER, &fuidp);
			if (new_kuid != KUID_TO_SUID(ZTOI(zp)->i_uid) &&
			    zfs_id_overquota(zfsvfs, DMU_USERUSED_OBJECT,
			    new_kuid)) {
				if (attrzp)
					zrele(attrzp);
				err = SET_ERROR(EDQUOT);
				goto out2;
			}
		}
		if (mask & ATTR_GID) {
			new_kgid = zfs_fuid_create(zfsvfs,
			    (uint64_t)vap->va_gid, cr, ZFS_GROUP, &fuidp);
			if (new_kgid != KGID_TO_SGID(ZTOI(zp)->i_gid) &&
			    zfs_id_overquota(zfsvfs, DMU_GROUPUSED_OBJECT,
			    new_kgid)) {
				if (attrzp)
					zrele(attrzp);
				err = SET_ERROR(EDQUOT);
				goto out2;
			}
		}
		if (projid != ZFS_INVALID_PROJID &&
		    zfs_id_overquota(zfsvfs, DMU_PROJECTUSED_OBJECT, projid)) {
			if (attrzp)
				zrele(attrzp);
			err = EDQUOT;
			goto out2;
		}
	}
	tx = dmu_tx_create(os);
	if (mask & ATTR_MODE) {
		uint64_t pmode = zp->z_mode;
		uint64_t acl_obj;
		new_mode = (pmode & S_IFMT) | (vap->va_mode & ~S_IFMT);
		if (ZTOZSB(zp)->z_acl_mode == ZFS_ACL_RESTRICTED &&
		    !(zp->z_pflags & ZFS_ACL_TRIVIAL)) {
			err = EPERM;
			goto out;
		}
		if ((err = zfs_acl_chmod_setattr(zp, &aclp, new_mode)))
			goto out;
		mutex_enter(&zp->z_lock);
		if (!zp->z_is_sa && ((acl_obj = zfs_external_acl(zp)) != 0)) {
			if (zfsvfs->z_version >= ZPL_VERSION_FUID &&
			    zfs_znode_acl_version(zp) ==
			    ZFS_ACL_VERSION_INITIAL) {
				dmu_tx_hold_free(tx, acl_obj, 0,
				    DMU_OBJECT_END);
				dmu_tx_hold_write(tx, DMU_NEW_OBJECT,
				    0, aclp->z_acl_bytes);
			} else {
				dmu_tx_hold_write(tx, acl_obj, 0,
				    aclp->z_acl_bytes);
			}
		} else if (!zp->z_is_sa && aclp->z_acl_bytes > ZFS_ACE_SPACE) {
			dmu_tx_hold_write(tx, DMU_NEW_OBJECT,
			    0, aclp->z_acl_bytes);
		}
		mutex_exit(&zp->z_lock);
		dmu_tx_hold_sa(tx, zp->z_sa_hdl, B_TRUE);
	} else {
		if (((mask & ATTR_XVATTR) &&
		    XVA_ISSET_REQ(xvap, XAT_AV_SCANSTAMP)) ||
		    (projid != ZFS_INVALID_PROJID &&
		    !(zp->z_pflags & ZFS_PROJID)))
			dmu_tx_hold_sa(tx, zp->z_sa_hdl, B_TRUE);
		else
			dmu_tx_hold_sa(tx, zp->z_sa_hdl, B_FALSE);
	}
	if (attrzp) {
		dmu_tx_hold_sa(tx, attrzp->z_sa_hdl, B_FALSE);
	}
	fuid_dirtied = zfsvfs->z_fuid_dirty;
	if (fuid_dirtied)
		zfs_fuid_txhold(zfsvfs, tx);
	zfs_sa_upgrade_txholds(tx, zp);
	err = dmu_tx_assign(tx, TXG_WAIT);
	if (err)
		goto out;
	count = 0;
	if (projid != ZFS_INVALID_PROJID && !(zp->z_pflags & ZFS_PROJID)) {
		if (attrzp)
			err = sa_add_projid(attrzp->z_sa_hdl, tx, projid);
		if (err == 0)
			err = sa_add_projid(zp->z_sa_hdl, tx, projid);
		if (unlikely(err == EEXIST))
			err = 0;
		else if (err != 0)
			goto out;
		else
			projid = ZFS_INVALID_PROJID;
	}
	if (mask & (ATTR_UID|ATTR_GID|ATTR_MODE))
		mutex_enter(&zp->z_acl_lock);
	mutex_enter(&zp->z_lock);
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_FLAGS(zfsvfs), NULL,
	    &zp->z_pflags, sizeof (zp->z_pflags));
	if (attrzp) {
		if (mask & (ATTR_UID|ATTR_GID|ATTR_MODE))
			mutex_enter(&attrzp->z_acl_lock);
		mutex_enter(&attrzp->z_lock);
		SA_ADD_BULK_ATTR(xattr_bulk, xattr_count,
		    SA_ZPL_FLAGS(zfsvfs), NULL, &attrzp->z_pflags,
		    sizeof (attrzp->z_pflags));
		if (projid != ZFS_INVALID_PROJID) {
			attrzp->z_projid = projid;
			SA_ADD_BULK_ATTR(xattr_bulk, xattr_count,
			    SA_ZPL_PROJID(zfsvfs), NULL, &attrzp->z_projid,
			    sizeof (attrzp->z_projid));
		}
	}
	if (mask & (ATTR_UID|ATTR_GID)) {
		if (mask & ATTR_UID) {
			ZTOI(zp)->i_uid = SUID_TO_KUID(new_kuid);
			new_uid = zfs_uid_read(ZTOI(zp));
			SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_UID(zfsvfs), NULL,
			    &new_uid, sizeof (new_uid));
			if (attrzp) {
				SA_ADD_BULK_ATTR(xattr_bulk, xattr_count,
				    SA_ZPL_UID(zfsvfs), NULL, &new_uid,
				    sizeof (new_uid));
				ZTOI(attrzp)->i_uid = SUID_TO_KUID(new_uid);
			}
		}
		if (mask & ATTR_GID) {
			ZTOI(zp)->i_gid = SGID_TO_KGID(new_kgid);
			new_gid = zfs_gid_read(ZTOI(zp));
			SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_GID(zfsvfs),
			    NULL, &new_gid, sizeof (new_gid));
			if (attrzp) {
				SA_ADD_BULK_ATTR(xattr_bulk, xattr_count,
				    SA_ZPL_GID(zfsvfs), NULL, &new_gid,
				    sizeof (new_gid));
				ZTOI(attrzp)->i_gid = SGID_TO_KGID(new_kgid);
			}
		}
		if (!(mask & ATTR_MODE)) {
			SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_MODE(zfsvfs),
			    NULL, &new_mode, sizeof (new_mode));
			new_mode = zp->z_mode;
		}
		err = zfs_acl_chown_setattr(zp);
		ASSERT(err == 0);
		if (attrzp) {
			err = zfs_acl_chown_setattr(attrzp);
			ASSERT(err == 0);
		}
	}
	if (mask & ATTR_MODE) {
		SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_MODE(zfsvfs), NULL,
		    &new_mode, sizeof (new_mode));
		zp->z_mode = ZTOI(zp)->i_mode = new_mode;
		ASSERT3P(aclp, !=, NULL);
		err = zfs_aclset_common(zp, aclp, cr, tx);
		ASSERT0(err);
		if (zp->z_acl_cached)
			zfs_acl_free(zp->z_acl_cached);
		zp->z_acl_cached = aclp;
		aclp = NULL;
	}
	if ((mask & ATTR_ATIME) || zp->z_atime_dirty) {
		zp->z_atime_dirty = B_FALSE;
		ZFS_TIME_ENCODE(&ip->i_atime, atime);
		SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_ATIME(zfsvfs), NULL,
		    &atime, sizeof (atime));
	}
	if (mask & (ATTR_MTIME | ATTR_SIZE)) {
		ZFS_TIME_ENCODE(&vap->va_mtime, mtime);
		ZTOI(zp)->i_mtime = zpl_inode_timestamp_truncate(
		    vap->va_mtime, ZTOI(zp));
		SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_MTIME(zfsvfs), NULL,
		    mtime, sizeof (mtime));
	}
	if (mask & (ATTR_CTIME | ATTR_SIZE)) {
		ZFS_TIME_ENCODE(&vap->va_ctime, ctime);
		zpl_inode_set_ctime_to_ts(ZTOI(zp),
		    zpl_inode_timestamp_truncate(vap->va_ctime, ZTOI(zp)));
		SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_CTIME(zfsvfs), NULL,
		    ctime, sizeof (ctime));
	}
	if (projid != ZFS_INVALID_PROJID) {
		zp->z_projid = projid;
		SA_ADD_BULK_ATTR(bulk, count,
		    SA_ZPL_PROJID(zfsvfs), NULL, &zp->z_projid,
		    sizeof (zp->z_projid));
	}
	if (attrzp && mask) {
		SA_ADD_BULK_ATTR(xattr_bulk, xattr_count,
		    SA_ZPL_CTIME(zfsvfs), NULL, &ctime,
		    sizeof (ctime));
	}
	if (xoap && (mask & ATTR_XVATTR)) {
		if (XVA_ISSET_REQ(tmpxvattr, XAT_APPENDONLY)) {
			XVA_SET_REQ(xvap, XAT_APPENDONLY);
		}
		if (XVA_ISSET_REQ(tmpxvattr, XAT_NOUNLINK)) {
			XVA_SET_REQ(xvap, XAT_NOUNLINK);
		}
		if (XVA_ISSET_REQ(tmpxvattr, XAT_IMMUTABLE)) {
			XVA_SET_REQ(xvap, XAT_IMMUTABLE);
		}
		if (XVA_ISSET_REQ(tmpxvattr, XAT_NODUMP)) {
			XVA_SET_REQ(xvap, XAT_NODUMP);
		}
		if (XVA_ISSET_REQ(tmpxvattr, XAT_AV_MODIFIED)) {
			XVA_SET_REQ(xvap, XAT_AV_MODIFIED);
		}
		if (XVA_ISSET_REQ(tmpxvattr, XAT_AV_QUARANTINED)) {
			XVA_SET_REQ(xvap, XAT_AV_QUARANTINED);
		}
		if (XVA_ISSET_REQ(tmpxvattr, XAT_PROJINHERIT)) {
			XVA_SET_REQ(xvap, XAT_PROJINHERIT);
		}
		if (XVA_ISSET_REQ(xvap, XAT_AV_SCANSTAMP))
			ASSERT(S_ISREG(ip->i_mode));
		zfs_xvattr_set(zp, xvap, tx);
	}
	if (fuid_dirtied)
		zfs_fuid_sync(zfsvfs, tx);
	if (mask != 0)
		zfs_log_setattr(zilog, tx, TX_SETATTR, zp, vap, mask, fuidp);
	mutex_exit(&zp->z_lock);
	if (mask & (ATTR_UID|ATTR_GID|ATTR_MODE))
		mutex_exit(&zp->z_acl_lock);
	if (attrzp) {
		if (mask & (ATTR_UID|ATTR_GID|ATTR_MODE))
			mutex_exit(&attrzp->z_acl_lock);
		mutex_exit(&attrzp->z_lock);
	}
out:
	if (err == 0 && xattr_count > 0) {
		err2 = sa_bulk_update(attrzp->z_sa_hdl, xattr_bulk,
		    xattr_count, tx);
		ASSERT(err2 == 0);
	}
	if (aclp)
		zfs_acl_free(aclp);
	if (fuidp) {
		zfs_fuid_info_free(fuidp);
		fuidp = NULL;
	}
	if (err) {
		dmu_tx_abort(tx);
		if (attrzp)
			zrele(attrzp);
		if (err == ERESTART)
			goto top;
	} else {
		if (count > 0)
			err2 = sa_bulk_update(zp->z_sa_hdl, bulk, count, tx);
		dmu_tx_commit(tx);
		if (attrzp) {
			if (err2 == 0 && handle_eadir)
				err = zfs_setattr_dir(attrzp);
			zrele(attrzp);
		}
		zfs_znode_update_vfs(zp);
	}
out2:
	if (os->os_sync == ZFS_SYNC_ALWAYS)
		zil_commit(zilog, 0);
out3:
	kmem_free(xattr_bulk, sizeof (sa_bulk_attr_t) * bulks);
	kmem_free(bulk, sizeof (sa_bulk_attr_t) * bulks);
	kmem_free(tmpxvattr, sizeof (xvattr_t));
	zfs_exit(zfsvfs, FTAG);
	return (err);
}
typedef struct zfs_zlock {
	krwlock_t	*zl_rwlock;	 
	znode_t		*zl_znode;	 
	struct zfs_zlock *zl_next;	 
} zfs_zlock_t;
static void
zfs_rename_unlock(zfs_zlock_t **zlpp)
{
	zfs_zlock_t *zl;
	while ((zl = *zlpp) != NULL) {
		if (zl->zl_znode != NULL)
			zfs_zrele_async(zl->zl_znode);
		rw_exit(zl->zl_rwlock);
		*zlpp = zl->zl_next;
		kmem_free(zl, sizeof (*zl));
	}
}
static int
zfs_rename_lock(znode_t *szp, znode_t *tdzp, znode_t *sdzp, zfs_zlock_t **zlpp)
{
	zfs_zlock_t	*zl;
	znode_t		*zp = tdzp;
	uint64_t	rootid = ZTOZSB(zp)->z_root;
	uint64_t	oidp = zp->z_id;
	krwlock_t	*rwlp = &szp->z_parent_lock;
	krw_t		rw = RW_WRITER;
	do {
		if (!rw_tryenter(rwlp, rw)) {
			if (rw == RW_READER && zp->z_id > szp->z_id) {
				zfs_rename_unlock(&zl);
				*zlpp = NULL;
				zp = tdzp;
				oidp = zp->z_id;
				rwlp = &szp->z_parent_lock;
				rw = RW_WRITER;
				continue;
			} else {
				rw_enter(rwlp, rw);
			}
		}
		zl = kmem_alloc(sizeof (*zl), KM_SLEEP);
		zl->zl_rwlock = rwlp;
		zl->zl_znode = NULL;
		zl->zl_next = *zlpp;
		*zlpp = zl;
		if (oidp == szp->z_id)		 
			return (SET_ERROR(EINVAL));
		if (oidp == rootid)		 
			return (0);
		if (rw == RW_READER) {		 
			int error = zfs_zget(ZTOZSB(zp), oidp, &zp);
			if (error)
				return (error);
			zl->zl_znode = zp;
		}
		(void) sa_lookup(zp->z_sa_hdl, SA_ZPL_PARENT(ZTOZSB(zp)),
		    &oidp, sizeof (oidp));
		rwlp = &zp->z_parent_lock;
		rw = RW_READER;
	} while (zp->z_id != sdzp->z_id);
	return (0);
}
int
zfs_rename(znode_t *sdzp, char *snm, znode_t *tdzp, char *tnm,
    cred_t *cr, int flags, uint64_t rflags, vattr_t *wo_vap, zidmap_t *mnt_ns)
{
	znode_t		*szp, *tzp;
	zfsvfs_t	*zfsvfs = ZTOZSB(sdzp);
	zilog_t		*zilog;
	zfs_dirlock_t	*sdl, *tdl;
	dmu_tx_t	*tx;
	zfs_zlock_t	*zl;
	int		cmp, serr, terr;
	int		error = 0;
	int		zflg = 0;
	boolean_t	waited = B_FALSE;
	boolean_t	fuid_dirtied;
	zfs_acl_ids_t	acl_ids;
	boolean_t	have_acl = B_FALSE;
	znode_t		*wzp = NULL;
	if (snm == NULL || tnm == NULL)
		return (SET_ERROR(EINVAL));
	if (rflags & ~(RENAME_NOREPLACE | RENAME_EXCHANGE | RENAME_WHITEOUT))
		return (SET_ERROR(EINVAL));
	if (rflags & RENAME_EXCHANGE &&
	    (rflags & (RENAME_NOREPLACE | RENAME_WHITEOUT)))
		return (SET_ERROR(EINVAL));
	VERIFY_EQUIV(rflags & RENAME_WHITEOUT, wo_vap != NULL);
	VERIFY_IMPLY(wo_vap, wo_vap->va_mode == S_IFCHR);
	VERIFY_IMPLY(wo_vap, wo_vap->va_rdev == makedevice(0, 0));
	if ((error = zfs_enter_verify_zp(zfsvfs, sdzp, FTAG)) != 0)
		return (error);
	zilog = zfsvfs->z_log;
	if ((error = zfs_verify_zp(tdzp)) != 0) {
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	if (ZTOI(tdzp)->i_sb != ZTOI(sdzp)->i_sb ||
	    zfsctl_is_node(ZTOI(tdzp))) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EXDEV));
	}
	if (zfsvfs->z_utf8 && u8_validate(tnm,
	    strlen(tnm), NULL, U8_VALIDATE_ENTIRE, &error) < 0) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EILSEQ));
	}
	if (flags & FIGNORECASE)
		zflg |= ZCILOOK;
top:
	szp = NULL;
	tzp = NULL;
	zl = NULL;
	if ((tdzp->z_pflags & ZFS_XATTR) != (sdzp->z_pflags & ZFS_XATTR)) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EINVAL));
	}
	if (sdzp->z_id < tdzp->z_id) {
		cmp = -1;
	} else if (sdzp->z_id > tdzp->z_id) {
		cmp = 1;
	} else {
		int nofold = (zfsvfs->z_norm & ~U8_TEXTPREP_TOUPPER);
		cmp = u8_strcmp(snm, tnm, 0, nofold, U8_UNICODE_LATEST, &error);
		ASSERT(error == 0 || !zfsvfs->z_utf8);
		if (cmp == 0) {
			zfs_exit(zfsvfs, FTAG);
			return (0);
		}
		if ((zfsvfs->z_case == ZFS_CASE_INSENSITIVE ||
		    (zfsvfs->z_case == ZFS_CASE_MIXED &&
		    flags & FIGNORECASE)) &&
		    u8_strcmp(snm, tnm, 0, zfsvfs->z_norm, U8_UNICODE_LATEST,
		    &error) == 0) {
			zflg |= ZCIEXACT;
			zflg &= ~ZCILOOK;
		}
	}
	if (sdzp == tdzp) {
		zflg |= ZHAVELOCK;
		rw_enter(&sdzp->z_name_lock, RW_READER);
	}
	if (cmp < 0) {
		serr = zfs_dirent_lock(&sdl, sdzp, snm, &szp,
		    ZEXISTS | zflg, NULL, NULL);
		terr = zfs_dirent_lock(&tdl,
		    tdzp, tnm, &tzp, ZRENAMING | zflg, NULL, NULL);
	} else {
		terr = zfs_dirent_lock(&tdl,
		    tdzp, tnm, &tzp, zflg, NULL, NULL);
		serr = zfs_dirent_lock(&sdl,
		    sdzp, snm, &szp, ZEXISTS | ZRENAMING | zflg,
		    NULL, NULL);
	}
	if (serr) {
		if (!terr) {
			zfs_dirent_unlock(tdl);
			if (tzp)
				zrele(tzp);
		}
		if (sdzp == tdzp)
			rw_exit(&sdzp->z_name_lock);
		if (strcmp(snm, "..") == 0)
			serr = EINVAL;
		zfs_exit(zfsvfs, FTAG);
		return (serr);
	}
	if (terr) {
		zfs_dirent_unlock(sdl);
		zrele(szp);
		if (sdzp == tdzp)
			rw_exit(&sdzp->z_name_lock);
		if (strcmp(tnm, "..") == 0)
			terr = EINVAL;
		zfs_exit(zfsvfs, FTAG);
		return (terr);
	}
	if (tdzp->z_pflags & ZFS_PROJINHERIT &&
	    tdzp->z_projid != szp->z_projid) {
		error = SET_ERROR(EXDEV);
		goto out;
	}
	if ((error = zfs_zaccess_rename(sdzp, szp, tdzp, tzp, cr, mnt_ns)))
		goto out;
	if (S_ISDIR(ZTOI(szp)->i_mode)) {
		if ((error = zfs_rename_lock(szp, tdzp, sdzp, &zl)))
			goto out;
	}
	if (tzp) {
		if (rflags & RENAME_NOREPLACE) {
			error = SET_ERROR(EEXIST);
			goto out;
		}
		if (!(rflags & RENAME_EXCHANGE)) {
			boolean_t s_is_dir = S_ISDIR(ZTOI(szp)->i_mode) != 0;
			boolean_t t_is_dir = S_ISDIR(ZTOI(tzp)->i_mode) != 0;
			if (s_is_dir != t_is_dir) {
				error = SET_ERROR(s_is_dir ? ENOTDIR : EISDIR);
				goto out;
			}
		}
		if (szp->z_id == tzp->z_id) {
			error = 0;
			goto out;
		}
	} else if (rflags & RENAME_EXCHANGE) {
		error = SET_ERROR(ENOENT);
		goto out;
	}
	if (rflags & RENAME_WHITEOUT) {
		uint64_t wo_projid = ZFS_DEFAULT_PROJID;
		error = zfs_zaccess(sdzp, ACE_ADD_FILE, 0, B_FALSE, cr, mnt_ns);
		if (error)
			goto out;
		if (!have_acl) {
			error = zfs_acl_ids_create(sdzp, 0, wo_vap, cr, NULL,
			    &acl_ids, mnt_ns);
			if (error)
				goto out;
			have_acl = B_TRUE;
		}
		if (zfs_acl_ids_overquota(zfsvfs, &acl_ids, wo_projid)) {
			error = SET_ERROR(EDQUOT);
			goto out;
		}
	}
	tx = dmu_tx_create(zfsvfs->z_os);
	dmu_tx_hold_sa(tx, szp->z_sa_hdl, B_FALSE);
	dmu_tx_hold_sa(tx, sdzp->z_sa_hdl, B_FALSE);
	dmu_tx_hold_zap(tx, sdzp->z_id,
	    (rflags & RENAME_EXCHANGE) ? TRUE : FALSE, snm);
	dmu_tx_hold_zap(tx, tdzp->z_id, TRUE, tnm);
	if (sdzp != tdzp) {
		dmu_tx_hold_sa(tx, tdzp->z_sa_hdl, B_FALSE);
		zfs_sa_upgrade_txholds(tx, tdzp);
	}
	if (tzp) {
		dmu_tx_hold_sa(tx, tzp->z_sa_hdl, B_FALSE);
		zfs_sa_upgrade_txholds(tx, tzp);
	}
	if (rflags & RENAME_WHITEOUT) {
		dmu_tx_hold_sa_create(tx, acl_ids.z_aclp->z_acl_bytes +
		    ZFS_SA_BASE_ATTR_SIZE);
		dmu_tx_hold_zap(tx, sdzp->z_id, TRUE, snm);
		dmu_tx_hold_sa(tx, sdzp->z_sa_hdl, B_FALSE);
		if (!zfsvfs->z_use_sa &&
		    acl_ids.z_aclp->z_acl_bytes > ZFS_ACE_SPACE) {
			dmu_tx_hold_write(tx, DMU_NEW_OBJECT,
			    0, acl_ids.z_aclp->z_acl_bytes);
		}
	}
	fuid_dirtied = zfsvfs->z_fuid_dirty;
	if (fuid_dirtied)
		zfs_fuid_txhold(zfsvfs, tx);
	zfs_sa_upgrade_txholds(tx, szp);
	dmu_tx_hold_zap(tx, zfsvfs->z_unlinkedobj, FALSE, NULL);
	error = dmu_tx_assign(tx, (waited ? TXG_NOTHROTTLE : 0) | TXG_NOWAIT);
	if (error) {
		if (zl != NULL)
			zfs_rename_unlock(&zl);
		zfs_dirent_unlock(sdl);
		zfs_dirent_unlock(tdl);
		if (sdzp == tdzp)
			rw_exit(&sdzp->z_name_lock);
		if (error == ERESTART) {
			waited = B_TRUE;
			dmu_tx_wait(tx);
			dmu_tx_abort(tx);
			zrele(szp);
			if (tzp)
				zrele(tzp);
			goto top;
		}
		dmu_tx_abort(tx);
		zrele(szp);
		if (tzp)
			zrele(tzp);
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	szp->z_pflags |= ZFS_AV_MODIFIED;
	if (tdzp->z_pflags & ZFS_PROJINHERIT)
		szp->z_pflags |= ZFS_PROJINHERIT;
	error = sa_update(szp->z_sa_hdl, SA_ZPL_FLAGS(zfsvfs),
	    (void *)&szp->z_pflags, sizeof (uint64_t), tx);
	VERIFY0(error);
	error = zfs_link_destroy(sdl, szp, tx, ZRENAMING, NULL);
	if (error)
		goto commit;
	if (tzp) {
		int tzflg = zflg;
		if (rflags & RENAME_EXCHANGE) {
			tzflg |= ZRENAMING;
			tzp->z_pflags |= ZFS_AV_MODIFIED;
			if (sdzp->z_pflags & ZFS_PROJINHERIT)
				tzp->z_pflags |= ZFS_PROJINHERIT;
			error = sa_update(tzp->z_sa_hdl, SA_ZPL_FLAGS(zfsvfs),
			    (void *)&tzp->z_pflags, sizeof (uint64_t), tx);
			ASSERT0(error);
		}
		error = zfs_link_destroy(tdl, tzp, tx, tzflg, NULL);
		if (error)
			goto commit_link_szp;
	}
	error = zfs_link_create(tdl, szp, tx, ZRENAMING);
	if (error) {
		ASSERT3P(tzp, ==, NULL);
		goto commit_link_tzp;
	}
	switch (rflags & (RENAME_EXCHANGE | RENAME_WHITEOUT)) {
	case RENAME_EXCHANGE:
		error = zfs_link_create(sdl, tzp, tx, ZRENAMING);
		ASSERT0(error);
		if (error)
			goto commit_unlink_td_szp;
		break;
	case RENAME_WHITEOUT:
		zfs_mknode(sdzp, wo_vap, tx, cr, 0, &wzp, &acl_ids);
		error = zfs_link_create(sdl, wzp, tx, ZNEW);
		if (error) {
			zfs_znode_delete(wzp, tx);
			remove_inode_hash(ZTOI(wzp));
			goto commit_unlink_td_szp;
		}
		break;
	}
	if (fuid_dirtied)
		zfs_fuid_sync(zfsvfs, tx);
	switch (rflags & (RENAME_EXCHANGE | RENAME_WHITEOUT)) {
	case RENAME_EXCHANGE:
		zfs_log_rename_exchange(zilog, tx,
		    (flags & FIGNORECASE ? TX_CI : 0), sdzp, sdl->dl_name,
		    tdzp, tdl->dl_name, szp);
		break;
	case RENAME_WHITEOUT:
		zfs_log_rename_whiteout(zilog, tx,
		    (flags & FIGNORECASE ? TX_CI : 0), sdzp, sdl->dl_name,
		    tdzp, tdl->dl_name, szp, wzp);
		break;
	default:
		ASSERT0(rflags & ~RENAME_NOREPLACE);
		zfs_log_rename(zilog, tx, (flags & FIGNORECASE ? TX_CI : 0),
		    sdzp, sdl->dl_name, tdzp, tdl->dl_name, szp);
		break;
	}
commit:
	dmu_tx_commit(tx);
out:
	if (have_acl)
		zfs_acl_ids_free(&acl_ids);
	zfs_znode_update_vfs(sdzp);
	if (sdzp == tdzp)
		rw_exit(&sdzp->z_name_lock);
	if (sdzp != tdzp)
		zfs_znode_update_vfs(tdzp);
	zfs_znode_update_vfs(szp);
	zrele(szp);
	if (wzp) {
		zfs_znode_update_vfs(wzp);
		zrele(wzp);
	}
	if (tzp) {
		zfs_znode_update_vfs(tzp);
		zrele(tzp);
	}
	if (zl != NULL)
		zfs_rename_unlock(&zl);
	zfs_dirent_unlock(sdl);
	zfs_dirent_unlock(tdl);
	if (zfsvfs->z_os->os_sync == ZFS_SYNC_ALWAYS)
		zil_commit(zilog, 0);
	zfs_exit(zfsvfs, FTAG);
	return (error);
commit_unlink_td_szp:
	VERIFY0(zfs_link_destroy(tdl, szp, tx, ZRENAMING, NULL));
commit_link_tzp:
	if (tzp) {
		if (zfs_link_create(tdl, tzp, tx, ZRENAMING))
			VERIFY0(zfs_drop_nlink(tzp, tx, NULL));
	}
commit_link_szp:
	if (zfs_link_create(sdl, szp, tx, ZRENAMING))
		VERIFY0(zfs_drop_nlink(szp, tx, NULL));
	goto commit;
}
int
zfs_symlink(znode_t *dzp, char *name, vattr_t *vap, char *link,
    znode_t **zpp, cred_t *cr, int flags, zidmap_t *mnt_ns)
{
	znode_t		*zp;
	zfs_dirlock_t	*dl;
	dmu_tx_t	*tx;
	zfsvfs_t	*zfsvfs = ZTOZSB(dzp);
	zilog_t		*zilog;
	uint64_t	len = strlen(link);
	int		error;
	int		zflg = ZNEW;
	zfs_acl_ids_t	acl_ids;
	boolean_t	fuid_dirtied;
	uint64_t	txtype = TX_SYMLINK;
	boolean_t	waited = B_FALSE;
	ASSERT(S_ISLNK(vap->va_mode));
	if (name == NULL)
		return (SET_ERROR(EINVAL));
	if ((error = zfs_enter_verify_zp(zfsvfs, dzp, FTAG)) != 0)
		return (error);
	zilog = zfsvfs->z_log;
	if (zfsvfs->z_utf8 && u8_validate(name, strlen(name),
	    NULL, U8_VALIDATE_ENTIRE, &error) < 0) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EILSEQ));
	}
	if (flags & FIGNORECASE)
		zflg |= ZCILOOK;
	if (len > MAXPATHLEN) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(ENAMETOOLONG));
	}
	if ((error = zfs_acl_ids_create(dzp, 0,
	    vap, cr, NULL, &acl_ids, mnt_ns)) != 0) {
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
top:
	*zpp = NULL;
	error = zfs_dirent_lock(&dl, dzp, name, &zp, zflg, NULL, NULL);
	if (error) {
		zfs_acl_ids_free(&acl_ids);
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	if ((error = zfs_zaccess(dzp, ACE_ADD_FILE, 0, B_FALSE, cr, mnt_ns))) {
		zfs_acl_ids_free(&acl_ids);
		zfs_dirent_unlock(dl);
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	if (zfs_acl_ids_overquota(zfsvfs, &acl_ids, ZFS_DEFAULT_PROJID)) {
		zfs_acl_ids_free(&acl_ids);
		zfs_dirent_unlock(dl);
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EDQUOT));
	}
	tx = dmu_tx_create(zfsvfs->z_os);
	fuid_dirtied = zfsvfs->z_fuid_dirty;
	dmu_tx_hold_write(tx, DMU_NEW_OBJECT, 0, MAX(1, len));
	dmu_tx_hold_zap(tx, dzp->z_id, TRUE, name);
	dmu_tx_hold_sa_create(tx, acl_ids.z_aclp->z_acl_bytes +
	    ZFS_SA_BASE_ATTR_SIZE + len);
	dmu_tx_hold_sa(tx, dzp->z_sa_hdl, B_FALSE);
	if (!zfsvfs->z_use_sa && acl_ids.z_aclp->z_acl_bytes > ZFS_ACE_SPACE) {
		dmu_tx_hold_write(tx, DMU_NEW_OBJECT, 0,
		    acl_ids.z_aclp->z_acl_bytes);
	}
	if (fuid_dirtied)
		zfs_fuid_txhold(zfsvfs, tx);
	error = dmu_tx_assign(tx, (waited ? TXG_NOTHROTTLE : 0) | TXG_NOWAIT);
	if (error) {
		zfs_dirent_unlock(dl);
		if (error == ERESTART) {
			waited = B_TRUE;
			dmu_tx_wait(tx);
			dmu_tx_abort(tx);
			goto top;
		}
		zfs_acl_ids_free(&acl_ids);
		dmu_tx_abort(tx);
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	zfs_mknode(dzp, vap, tx, cr, 0, &zp, &acl_ids);
	if (fuid_dirtied)
		zfs_fuid_sync(zfsvfs, tx);
	mutex_enter(&zp->z_lock);
	if (zp->z_is_sa)
		error = sa_update(zp->z_sa_hdl, SA_ZPL_SYMLINK(zfsvfs),
		    link, len, tx);
	else
		zfs_sa_symlink(zp, link, len, tx);
	mutex_exit(&zp->z_lock);
	zp->z_size = len;
	(void) sa_update(zp->z_sa_hdl, SA_ZPL_SIZE(zfsvfs),
	    &zp->z_size, sizeof (zp->z_size), tx);
	error = zfs_link_create(dl, zp, tx, ZNEW);
	if (error != 0) {
		zfs_znode_delete(zp, tx);
		remove_inode_hash(ZTOI(zp));
	} else {
		if (flags & FIGNORECASE)
			txtype |= TX_CI;
		zfs_log_symlink(zilog, tx, txtype, dzp, zp, name, link);
		zfs_znode_update_vfs(dzp);
		zfs_znode_update_vfs(zp);
	}
	zfs_acl_ids_free(&acl_ids);
	dmu_tx_commit(tx);
	zfs_dirent_unlock(dl);
	if (error == 0) {
		*zpp = zp;
		if (zfsvfs->z_os->os_sync == ZFS_SYNC_ALWAYS)
			zil_commit(zilog, 0);
	} else {
		zrele(zp);
	}
	zfs_exit(zfsvfs, FTAG);
	return (error);
}
int
zfs_readlink(struct inode *ip, zfs_uio_t *uio, cred_t *cr)
{
	(void) cr;
	znode_t		*zp = ITOZ(ip);
	zfsvfs_t	*zfsvfs = ITOZSB(ip);
	int		error;
	if ((error = zfs_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		return (error);
	mutex_enter(&zp->z_lock);
	if (zp->z_is_sa)
		error = sa_lookup_uio(zp->z_sa_hdl,
		    SA_ZPL_SYMLINK(zfsvfs), uio);
	else
		error = zfs_sa_readlink(zp, uio);
	mutex_exit(&zp->z_lock);
	zfs_exit(zfsvfs, FTAG);
	return (error);
}
int
zfs_link(znode_t *tdzp, znode_t *szp, char *name, cred_t *cr,
    int flags)
{
	struct inode *sip = ZTOI(szp);
	znode_t		*tzp;
	zfsvfs_t	*zfsvfs = ZTOZSB(tdzp);
	zilog_t		*zilog;
	zfs_dirlock_t	*dl;
	dmu_tx_t	*tx;
	int		error;
	int		zf = ZNEW;
	uint64_t	parent;
	uid_t		owner;
	boolean_t	waited = B_FALSE;
	boolean_t	is_tmpfile = 0;
	uint64_t	txg;
#ifdef HAVE_TMPFILE
	is_tmpfile = (sip->i_nlink == 0 && (sip->i_state & I_LINKABLE));
#endif
	ASSERT(S_ISDIR(ZTOI(tdzp)->i_mode));
	if (name == NULL)
		return (SET_ERROR(EINVAL));
	if ((error = zfs_enter_verify_zp(zfsvfs, tdzp, FTAG)) != 0)
		return (error);
	zilog = zfsvfs->z_log;
	if (S_ISDIR(sip->i_mode)) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EPERM));
	}
	if ((error = zfs_verify_zp(szp)) != 0) {
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	if (tdzp->z_pflags & ZFS_PROJINHERIT &&
	    tdzp->z_projid != szp->z_projid) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EXDEV));
	}
	if (sip->i_sb != ZTOI(tdzp)->i_sb || zfsctl_is_node(sip)) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EXDEV));
	}
	if ((error = sa_lookup(szp->z_sa_hdl, SA_ZPL_PARENT(zfsvfs),
	    &parent, sizeof (uint64_t))) != 0) {
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	if (parent == zfsvfs->z_shares_dir) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EPERM));
	}
	if (zfsvfs->z_utf8 && u8_validate(name,
	    strlen(name), NULL, U8_VALIDATE_ENTIRE, &error) < 0) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EILSEQ));
	}
	if (flags & FIGNORECASE)
		zf |= ZCILOOK;
	if ((szp->z_pflags & ZFS_XATTR) != (tdzp->z_pflags & ZFS_XATTR)) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EINVAL));
	}
	owner = zfs_fuid_map_id(zfsvfs, KUID_TO_SUID(sip->i_uid),
	    cr, ZFS_OWNER);
	if (owner != crgetuid(cr) && secpolicy_basic_link(cr) != 0) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EPERM));
	}
	if ((error = zfs_zaccess(tdzp, ACE_ADD_FILE, 0, B_FALSE, cr,
	    zfs_init_idmap))) {
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
top:
	error = zfs_dirent_lock(&dl, tdzp, name, &tzp, zf, NULL, NULL);
	if (error) {
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	tx = dmu_tx_create(zfsvfs->z_os);
	dmu_tx_hold_sa(tx, szp->z_sa_hdl, B_FALSE);
	dmu_tx_hold_zap(tx, tdzp->z_id, TRUE, name);
	if (is_tmpfile)
		dmu_tx_hold_zap(tx, zfsvfs->z_unlinkedobj, FALSE, NULL);
	zfs_sa_upgrade_txholds(tx, szp);
	zfs_sa_upgrade_txholds(tx, tdzp);
	error = dmu_tx_assign(tx, (waited ? TXG_NOTHROTTLE : 0) | TXG_NOWAIT);
	if (error) {
		zfs_dirent_unlock(dl);
		if (error == ERESTART) {
			waited = B_TRUE;
			dmu_tx_wait(tx);
			dmu_tx_abort(tx);
			goto top;
		}
		dmu_tx_abort(tx);
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	if (is_tmpfile)
		szp->z_unlinked = B_FALSE;
	error = zfs_link_create(dl, szp, tx, 0);
	if (error == 0) {
		uint64_t txtype = TX_LINK;
		if (is_tmpfile) {
			VERIFY(zap_remove_int(zfsvfs->z_os,
			    zfsvfs->z_unlinkedobj, szp->z_id, tx) == 0);
		} else {
			if (flags & FIGNORECASE)
				txtype |= TX_CI;
			zfs_log_link(zilog, tx, txtype, tdzp, szp, name);
		}
	} else if (is_tmpfile) {
		szp->z_unlinked = B_TRUE;
	}
	txg = dmu_tx_get_txg(tx);
	dmu_tx_commit(tx);
	zfs_dirent_unlock(dl);
	if (!is_tmpfile && zfsvfs->z_os->os_sync == ZFS_SYNC_ALWAYS)
		zil_commit(zilog, 0);
	if (is_tmpfile && zfsvfs->z_os->os_sync != ZFS_SYNC_DISABLED)
		txg_wait_synced(dmu_objset_pool(zfsvfs->z_os), txg);
	zfs_znode_update_vfs(tdzp);
	zfs_znode_update_vfs(szp);
	zfs_exit(zfsvfs, FTAG);
	return (error);
}
static void
zfs_putpage_sync_commit_cb(void *arg)
{
	struct page *pp = arg;
	ClearPageError(pp);
	end_page_writeback(pp);
}
static void
zfs_putpage_async_commit_cb(void *arg)
{
	struct page *pp = arg;
	znode_t *zp = ITOZ(pp->mapping->host);
	ClearPageError(pp);
	end_page_writeback(pp);
	atomic_dec_32(&zp->z_async_writes_cnt);
}
int
zfs_putpage(struct inode *ip, struct page *pp, struct writeback_control *wbc,
    boolean_t for_sync)
{
	znode_t		*zp = ITOZ(ip);
	zfsvfs_t	*zfsvfs = ITOZSB(ip);
	loff_t		offset;
	loff_t		pgoff;
	unsigned int	pglen;
	dmu_tx_t	*tx;
	caddr_t		va;
	int		err = 0;
	uint64_t	mtime[2], ctime[2];
	inode_timespec_t tmp_ctime;
	sa_bulk_attr_t	bulk[3];
	int		cnt = 0;
	struct address_space *mapping;
	if ((err = zfs_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		return (err);
	ASSERT(PageLocked(pp));
	pgoff = page_offset(pp);	 
	offset = i_size_read(ip);	 
	pglen = MIN(PAGE_SIZE,		 
	    P2ROUNDUP(offset, PAGE_SIZE)-pgoff);
	if (pgoff >= offset) {
		unlock_page(pp);
		zfs_exit(zfsvfs, FTAG);
		return (0);
	}
	if (pgoff + pglen > offset)
		pglen = offset - pgoff;
#if 0
	if (zfs_id_overblockquota(zfsvfs, DMU_USERUSED_OBJECT,
	    KUID_TO_SUID(ip->i_uid)) ||
	    zfs_id_overblockquota(zfsvfs, DMU_GROUPUSED_OBJECT,
	    KGID_TO_SGID(ip->i_gid)) ||
	    (zp->z_projid != ZFS_DEFAULT_PROJID &&
	    zfs_id_overblockquota(zfsvfs, DMU_PROJECTUSED_OBJECT,
	    zp->z_projid))) {
		err = EDQUOT;
	}
#endif
	mapping = pp->mapping;
	redirty_page_for_writepage(wbc, pp);
	unlock_page(pp);
	zfs_locked_range_t *lr = zfs_rangelock_enter(&zp->z_rangelock,
	    pgoff, pglen, RL_WRITER);
	lock_page(pp);
	if (unlikely((mapping != pp->mapping) || !PageDirty(pp))) {
		unlock_page(pp);
		zfs_rangelock_exit(lr);
		zfs_exit(zfsvfs, FTAG);
		return (0);
	}
	if (PageWriteback(pp)) {
		unlock_page(pp);
		zfs_rangelock_exit(lr);
		if (wbc->sync_mode != WB_SYNC_NONE) {
			if (atomic_load_32(&zp->z_async_writes_cnt) > 0) {
				zil_commit(zfsvfs->z_log, zp->z_id);
			}
			if (PageWriteback(pp))
#ifdef HAVE_PAGEMAP_FOLIO_WAIT_BIT
				folio_wait_bit(page_folio(pp), PG_writeback);
#else
				wait_on_page_bit(pp, PG_writeback);
#endif
		}
		zfs_exit(zfsvfs, FTAG);
		return (0);
	}
	if (!clear_page_dirty_for_io(pp)) {
		unlock_page(pp);
		zfs_rangelock_exit(lr);
		zfs_exit(zfsvfs, FTAG);
		return (0);
	}
	wbc->pages_skipped--;
	if (!for_sync)
		atomic_inc_32(&zp->z_async_writes_cnt);
	set_page_writeback(pp);
	unlock_page(pp);
	tx = dmu_tx_create(zfsvfs->z_os);
	dmu_tx_hold_write(tx, zp->z_id, pgoff, pglen);
	dmu_tx_hold_sa(tx, zp->z_sa_hdl, B_FALSE);
	zfs_sa_upgrade_txholds(tx, zp);
	err = dmu_tx_assign(tx, TXG_NOWAIT);
	if (err != 0) {
		if (err == ERESTART)
			dmu_tx_wait(tx);
		dmu_tx_abort(tx);
#ifdef HAVE_VFS_FILEMAP_DIRTY_FOLIO
		filemap_dirty_folio(page_mapping(pp), page_folio(pp));
#else
		__set_page_dirty_nobuffers(pp);
#endif
		ClearPageError(pp);
		end_page_writeback(pp);
		if (!for_sync)
			atomic_dec_32(&zp->z_async_writes_cnt);
		zfs_rangelock_exit(lr);
		zfs_exit(zfsvfs, FTAG);
		return (err);
	}
	va = kmap(pp);
	ASSERT3U(pglen, <=, PAGE_SIZE);
	dmu_write(zfsvfs->z_os, zp->z_id, pgoff, pglen, va, tx);
	kunmap(pp);
	SA_ADD_BULK_ATTR(bulk, cnt, SA_ZPL_MTIME(zfsvfs), NULL, &mtime, 16);
	SA_ADD_BULK_ATTR(bulk, cnt, SA_ZPL_CTIME(zfsvfs), NULL, &ctime, 16);
	SA_ADD_BULK_ATTR(bulk, cnt, SA_ZPL_FLAGS(zfsvfs), NULL,
	    &zp->z_pflags, 8);
	ZFS_TIME_ENCODE(&ip->i_mtime, mtime);
	tmp_ctime = zpl_inode_get_ctime(ip);
	ZFS_TIME_ENCODE(&tmp_ctime, ctime);
	zp->z_atime_dirty = B_FALSE;
	zp->z_seq++;
	err = sa_bulk_update(zp->z_sa_hdl, bulk, cnt, tx);
	zfs_log_write(zfsvfs->z_log, tx, TX_WRITE, zp, pgoff, pglen, 0,
	    for_sync ? zfs_putpage_sync_commit_cb :
	    zfs_putpage_async_commit_cb, pp);
	dmu_tx_commit(tx);
	zfs_rangelock_exit(lr);
	if (wbc->sync_mode != WB_SYNC_NONE) {
		zil_commit(zfsvfs->z_log, zp->z_id);
	} else if (!for_sync && atomic_load_32(&zp->z_sync_writes_cnt) > 0) {
		zil_commit(zfsvfs->z_log, zp->z_id);
	}
	dataset_kstats_update_write_kstats(&zfsvfs->z_kstat, pglen);
	zfs_exit(zfsvfs, FTAG);
	return (err);
}
int
zfs_dirty_inode(struct inode *ip, int flags)
{
	znode_t		*zp = ITOZ(ip);
	zfsvfs_t	*zfsvfs = ITOZSB(ip);
	dmu_tx_t	*tx;
	uint64_t	mode, atime[2], mtime[2], ctime[2];
	inode_timespec_t tmp_ctime;
	sa_bulk_attr_t	bulk[4];
	int		error = 0;
	int		cnt = 0;
	if (zfs_is_readonly(zfsvfs) || dmu_objset_is_snapshot(zfsvfs->z_os))
		return (0);
	if ((error = zfs_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		return (error);
#ifdef I_DIRTY_TIME
	if (flags == I_DIRTY_TIME) {
		zp->z_atime_dirty = B_TRUE;
		goto out;
	}
#endif
	tx = dmu_tx_create(zfsvfs->z_os);
	dmu_tx_hold_sa(tx, zp->z_sa_hdl, B_FALSE);
	zfs_sa_upgrade_txholds(tx, zp);
	error = dmu_tx_assign(tx, TXG_WAIT);
	if (error) {
		dmu_tx_abort(tx);
		goto out;
	}
	mutex_enter(&zp->z_lock);
	zp->z_atime_dirty = B_FALSE;
	SA_ADD_BULK_ATTR(bulk, cnt, SA_ZPL_MODE(zfsvfs), NULL, &mode, 8);
	SA_ADD_BULK_ATTR(bulk, cnt, SA_ZPL_ATIME(zfsvfs), NULL, &atime, 16);
	SA_ADD_BULK_ATTR(bulk, cnt, SA_ZPL_MTIME(zfsvfs), NULL, &mtime, 16);
	SA_ADD_BULK_ATTR(bulk, cnt, SA_ZPL_CTIME(zfsvfs), NULL, &ctime, 16);
	ZFS_TIME_ENCODE(&ip->i_atime, atime);
	ZFS_TIME_ENCODE(&ip->i_mtime, mtime);
	tmp_ctime = zpl_inode_get_ctime(ip);
	ZFS_TIME_ENCODE(&tmp_ctime, ctime);
	mode = ip->i_mode;
	zp->z_mode = mode;
	error = sa_bulk_update(zp->z_sa_hdl, bulk, cnt, tx);
	mutex_exit(&zp->z_lock);
	dmu_tx_commit(tx);
out:
	zfs_exit(zfsvfs, FTAG);
	return (error);
}
void
zfs_inactive(struct inode *ip)
{
	znode_t	*zp = ITOZ(ip);
	zfsvfs_t *zfsvfs = ITOZSB(ip);
	uint64_t atime[2];
	int error;
	int need_unlock = 0;
	if (!RW_WRITE_HELD(&zfsvfs->z_teardown_inactive_lock)) {
		need_unlock = 1;
		rw_enter(&zfsvfs->z_teardown_inactive_lock, RW_READER);
	}
	if (zp->z_sa_hdl == NULL) {
		if (need_unlock)
			rw_exit(&zfsvfs->z_teardown_inactive_lock);
		return;
	}
	if (zp->z_atime_dirty && zp->z_unlinked == B_FALSE) {
		dmu_tx_t *tx = dmu_tx_create(zfsvfs->z_os);
		dmu_tx_hold_sa(tx, zp->z_sa_hdl, B_FALSE);
		zfs_sa_upgrade_txholds(tx, zp);
		error = dmu_tx_assign(tx, TXG_WAIT);
		if (error) {
			dmu_tx_abort(tx);
		} else {
			ZFS_TIME_ENCODE(&ip->i_atime, atime);
			mutex_enter(&zp->z_lock);
			(void) sa_update(zp->z_sa_hdl, SA_ZPL_ATIME(zfsvfs),
			    (void *)&atime, sizeof (atime), tx);
			zp->z_atime_dirty = B_FALSE;
			mutex_exit(&zp->z_lock);
			dmu_tx_commit(tx);
		}
	}
	zfs_zinactive(zp);
	if (need_unlock)
		rw_exit(&zfsvfs->z_teardown_inactive_lock);
}
static int
zfs_fillpage(struct inode *ip, struct page *pp)
{
	zfsvfs_t *zfsvfs = ITOZSB(ip);
	loff_t i_size = i_size_read(ip);
	u_offset_t io_off = page_offset(pp);
	size_t io_len = PAGE_SIZE;
	ASSERT3U(io_off, <, i_size);
	if (io_off + io_len > i_size)
		io_len = i_size - io_off;
	void *va = kmap(pp);
	int error = dmu_read(zfsvfs->z_os, ITOZ(ip)->z_id, io_off,
	    io_len, va, DMU_READ_PREFETCH);
	if (io_len != PAGE_SIZE)
		memset((char *)va + io_len, 0, PAGE_SIZE - io_len);
	kunmap(pp);
	if (error) {
		if (error == ECKSUM)
			error = SET_ERROR(EIO);
		SetPageError(pp);
		ClearPageUptodate(pp);
	} else {
		ClearPageError(pp);
		SetPageUptodate(pp);
	}
	return (error);
}
int
zfs_getpage(struct inode *ip, struct page *pp)
{
	zfsvfs_t *zfsvfs = ITOZSB(ip);
	znode_t *zp = ITOZ(ip);
	int error;
	if ((error = zfs_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		return (error);
	error = zfs_fillpage(ip, pp);
	if (error == 0)
		dataset_kstats_update_read_kstats(&zfsvfs->z_kstat, PAGE_SIZE);
	zfs_exit(zfsvfs, FTAG);
	return (error);
}
int
zfs_map(struct inode *ip, offset_t off, caddr_t *addrp, size_t len,
    unsigned long vm_flags)
{
	(void) addrp;
	znode_t  *zp = ITOZ(ip);
	zfsvfs_t *zfsvfs = ITOZSB(ip);
	int error;
	if ((error = zfs_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		return (error);
	if ((vm_flags & VM_WRITE) && (vm_flags & VM_SHARED) &&
	    (zp->z_pflags & (ZFS_IMMUTABLE | ZFS_READONLY | ZFS_APPENDONLY))) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EPERM));
	}
	if ((vm_flags & (VM_READ | VM_EXEC)) &&
	    (zp->z_pflags & ZFS_AV_QUARANTINED)) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EACCES));
	}
	if (off < 0 || len > MAXOFFSET_T - off) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(ENXIO));
	}
	zfs_exit(zfsvfs, FTAG);
	return (0);
}
int
zfs_space(znode_t *zp, int cmd, flock64_t *bfp, int flag,
    offset_t offset, cred_t *cr)
{
	(void) offset;
	zfsvfs_t	*zfsvfs = ZTOZSB(zp);
	uint64_t	off, len;
	int		error;
	if ((error = zfs_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		return (error);
	if (cmd != F_FREESP) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EINVAL));
	}
	if (zfs_is_readonly(zfsvfs)) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EROFS));
	}
	if (bfp->l_len < 0) {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EINVAL));
	}
	if ((error = zfs_zaccess(zp, ACE_WRITE_DATA, 0, B_FALSE, cr,
	    zfs_init_idmap))) {
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	off = bfp->l_start;
	len = bfp->l_len;  
	error = zfs_freesp(zp, off, len, flag, TRUE);
	zfs_exit(zfsvfs, FTAG);
	return (error);
}
int
zfs_fid(struct inode *ip, fid_t *fidp)
{
	znode_t		*zp = ITOZ(ip);
	zfsvfs_t	*zfsvfs = ITOZSB(ip);
	uint32_t	gen;
	uint64_t	gen64;
	uint64_t	object = zp->z_id;
	zfid_short_t	*zfid;
	int		size, i, error;
	if ((error = zfs_enter(zfsvfs, FTAG)) != 0)
		return (error);
	if (fidp->fid_len < SHORT_FID_LEN) {
		fidp->fid_len = SHORT_FID_LEN;
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(ENOSPC));
	}
	if ((error = zfs_verify_zp(zp)) != 0) {
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	if ((error = sa_lookup(zp->z_sa_hdl, SA_ZPL_GEN(zfsvfs),
	    &gen64, sizeof (uint64_t))) != 0) {
		zfs_exit(zfsvfs, FTAG);
		return (error);
	}
	gen = (uint32_t)gen64;
	size = SHORT_FID_LEN;
	zfid = (zfid_short_t *)fidp;
	zfid->zf_len = size;
	for (i = 0; i < sizeof (zfid->zf_object); i++)
		zfid->zf_object[i] = (uint8_t)(object >> (8 * i));
	if (gen == 0)
		gen = 1;
	for (i = 0; i < sizeof (zfid->zf_gen); i++)
		zfid->zf_gen[i] = (uint8_t)(gen >> (8 * i));
	zfs_exit(zfsvfs, FTAG);
	return (0);
}
#if defined(_KERNEL)
EXPORT_SYMBOL(zfs_open);
EXPORT_SYMBOL(zfs_close);
EXPORT_SYMBOL(zfs_lookup);
EXPORT_SYMBOL(zfs_create);
EXPORT_SYMBOL(zfs_tmpfile);
EXPORT_SYMBOL(zfs_remove);
EXPORT_SYMBOL(zfs_mkdir);
EXPORT_SYMBOL(zfs_rmdir);
EXPORT_SYMBOL(zfs_readdir);
EXPORT_SYMBOL(zfs_getattr_fast);
EXPORT_SYMBOL(zfs_setattr);
EXPORT_SYMBOL(zfs_rename);
EXPORT_SYMBOL(zfs_symlink);
EXPORT_SYMBOL(zfs_readlink);
EXPORT_SYMBOL(zfs_link);
EXPORT_SYMBOL(zfs_inactive);
EXPORT_SYMBOL(zfs_space);
EXPORT_SYMBOL(zfs_fid);
EXPORT_SYMBOL(zfs_getpage);
EXPORT_SYMBOL(zfs_putpage);
EXPORT_SYMBOL(zfs_dirty_inode);
EXPORT_SYMBOL(zfs_map);
module_param(zfs_delete_blocks, ulong, 0644);
MODULE_PARM_DESC(zfs_delete_blocks, "Delete files larger than N blocks async");
module_param(zfs_bclone_enabled, uint, 0644);
MODULE_PARM_DESC(zfs_bclone_enabled, "Enable block cloning");
#endif
