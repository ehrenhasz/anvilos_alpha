 

 

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/sysmacros.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/kmem.h>
#include <sys/uio.h>
#include <sys/pathname.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/sunddi.h>
#include <sys/random.h>
#include <sys/policy.h>
#include <sys/zfs_dir.h>
#include <sys/zfs_acl.h>
#include <sys/zfs_vnops.h>
#include <sys/fs/zfs.h>
#include <sys/zap.h>
#include <sys/dmu.h>
#include <sys/atomic.h>
#include <sys/zfs_ctldir.h>
#include <sys/zfs_fuid.h>
#include <sys/sa.h>
#include <sys/zfs_sa.h>
#include <sys/dmu_objset.h>
#include <sys/dsl_dir.h>

 
static int
zfs_match_find(zfsvfs_t *zfsvfs, znode_t *dzp, const char *name,
    matchtype_t mt, boolean_t update, int *deflags, pathname_t *rpnp,
    uint64_t *zoid)
{
	boolean_t conflict = B_FALSE;
	int error;

	if (zfsvfs->z_norm) {
		size_t bufsz = 0;
		char *buf = NULL;

		if (rpnp) {
			buf = rpnp->pn_buf;
			bufsz = rpnp->pn_bufsize;
		}

		 
		error = zap_lookup_norm(zfsvfs->z_os, dzp->z_id, name, 8, 1,
		    zoid, mt, buf, bufsz, &conflict);
	} else {
		error = zap_lookup(zfsvfs->z_os, dzp->z_id, name, 8, 1, zoid);
	}

	 
	if (error == EOVERFLOW)
		error = 0;

	if (zfsvfs->z_norm && !error && deflags)
		*deflags = conflict ? ED_CASE_CONFLICT : 0;

	*zoid = ZFS_DIRENT_OBJ(*zoid);

	return (error);
}

 
int
zfs_dirent_lock(zfs_dirlock_t **dlpp, znode_t *dzp, char *name,
    znode_t **zpp, int flag, int *direntflags, pathname_t *realpnp)
{
	zfsvfs_t	*zfsvfs = ZTOZSB(dzp);
	zfs_dirlock_t	*dl;
	boolean_t	update;
	matchtype_t	mt = 0;
	uint64_t	zoid;
	int		error = 0;
	int		cmpflags;

	*zpp = NULL;
	*dlpp = NULL;

	 
	if ((name[0] == '.' &&
	    (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) ||
	    (zfs_has_ctldir(dzp) && strcmp(name, ZFS_CTLDIR_NAME) == 0))
		return (SET_ERROR(EEXIST));

	 

	 
	if (zfsvfs->z_norm != 0) {
		mt = MT_NORMALIZE;

		 
		if ((zfsvfs->z_case == ZFS_CASE_INSENSITIVE &&
		    (flag & ZCIEXACT)) ||
		    (zfsvfs->z_case == ZFS_CASE_MIXED && !(flag & ZCILOOK))) {
			mt |= MT_MATCH_CASE;
		}
	}

	 
	update = !zfsvfs->z_norm ||
	    (zfsvfs->z_case == ZFS_CASE_MIXED &&
	    !(zfsvfs->z_norm & ~U8_TEXTPREP_TOUPPER) && !(flag & ZCILOOK));

	 
	if (flag & ZRENAMING)
		cmpflags = 0;
	else
		cmpflags = zfsvfs->z_norm;

	 
	ASSERT(!(flag & ZSHARED) || !(flag & ZHAVELOCK));
	if (!(flag & ZHAVELOCK))
		rw_enter(&dzp->z_name_lock, RW_READER);

	mutex_enter(&dzp->z_lock);
	for (;;) {
		if (dzp->z_unlinked && !(flag & ZXATTR)) {
			mutex_exit(&dzp->z_lock);
			if (!(flag & ZHAVELOCK))
				rw_exit(&dzp->z_name_lock);
			return (SET_ERROR(ENOENT));
		}
		for (dl = dzp->z_dirlocks; dl != NULL; dl = dl->dl_next) {
			if ((u8_strcmp(name, dl->dl_name, 0, cmpflags,
			    U8_UNICODE_LATEST, &error) == 0) || error != 0)
				break;
		}
		if (error != 0) {
			mutex_exit(&dzp->z_lock);
			if (!(flag & ZHAVELOCK))
				rw_exit(&dzp->z_name_lock);
			return (SET_ERROR(ENOENT));
		}
		if (dl == NULL)	{
			 
			dl = kmem_alloc(sizeof (zfs_dirlock_t), KM_SLEEP);
			cv_init(&dl->dl_cv, NULL, CV_DEFAULT, NULL);
			dl->dl_name = name;
			dl->dl_sharecnt = 0;
			dl->dl_namelock = 0;
			dl->dl_namesize = 0;
			dl->dl_dzp = dzp;
			dl->dl_next = dzp->z_dirlocks;
			dzp->z_dirlocks = dl;
			break;
		}
		if ((flag & ZSHARED) && dl->dl_sharecnt != 0)
			break;
		cv_wait(&dl->dl_cv, &dzp->z_lock);
	}

	 
	if (flag & ZHAVELOCK)
		dl->dl_namelock = 1;

	if ((flag & ZSHARED) && ++dl->dl_sharecnt > 1 && dl->dl_namesize == 0) {
		 
		dl->dl_namesize = strlen(dl->dl_name) + 1;
		name = kmem_alloc(dl->dl_namesize, KM_SLEEP);
		memcpy(name, dl->dl_name, dl->dl_namesize);
		dl->dl_name = name;
	}

	mutex_exit(&dzp->z_lock);

	 
	if (flag & ZXATTR) {
		error = sa_lookup(dzp->z_sa_hdl, SA_ZPL_XATTR(zfsvfs), &zoid,
		    sizeof (zoid));
		if (error == 0)
			error = (zoid == 0 ? SET_ERROR(ENOENT) : 0);
	} else {
		error = zfs_match_find(zfsvfs, dzp, name, mt,
		    update, direntflags, realpnp, &zoid);
	}
	if (error) {
		if (error != ENOENT || (flag & ZEXISTS)) {
			zfs_dirent_unlock(dl);
			return (error);
		}
	} else {
		if (flag & ZNEW) {
			zfs_dirent_unlock(dl);
			return (SET_ERROR(EEXIST));
		}
		error = zfs_zget(zfsvfs, zoid, zpp);
		if (error) {
			zfs_dirent_unlock(dl);
			return (error);
		}
	}

	*dlpp = dl;

	return (0);
}

 
void
zfs_dirent_unlock(zfs_dirlock_t *dl)
{
	znode_t *dzp = dl->dl_dzp;
	zfs_dirlock_t **prev_dl, *cur_dl;

	mutex_enter(&dzp->z_lock);

	if (!dl->dl_namelock)
		rw_exit(&dzp->z_name_lock);

	if (dl->dl_sharecnt > 1) {
		dl->dl_sharecnt--;
		mutex_exit(&dzp->z_lock);
		return;
	}
	prev_dl = &dzp->z_dirlocks;
	while ((cur_dl = *prev_dl) != dl)
		prev_dl = &cur_dl->dl_next;
	*prev_dl = dl->dl_next;
	cv_broadcast(&dl->dl_cv);
	mutex_exit(&dzp->z_lock);

	if (dl->dl_namesize != 0)
		kmem_free(dl->dl_name, dl->dl_namesize);
	cv_destroy(&dl->dl_cv);
	kmem_free(dl, sizeof (*dl));
}

 
int
zfs_dirlook(znode_t *dzp, char *name, znode_t **zpp, int flags,
    int *deflg, pathname_t *rpnp)
{
	zfs_dirlock_t *dl;
	znode_t *zp;
	struct inode *ip;
	int error = 0;
	uint64_t parent;

	if (name[0] == 0 || (name[0] == '.' && name[1] == 0)) {
		*zpp = dzp;
		zhold(*zpp);
	} else if (name[0] == '.' && name[1] == '.' && name[2] == 0) {
		zfsvfs_t *zfsvfs = ZTOZSB(dzp);

		 
		if ((error = sa_lookup(dzp->z_sa_hdl,
		    SA_ZPL_PARENT(zfsvfs), &parent, sizeof (parent))) != 0)
			return (error);

		if (parent == dzp->z_id && zfsvfs->z_parent != zfsvfs) {
			error = zfsctl_root_lookup(zfsvfs->z_parent->z_ctldir,
			    "snapshot", &ip, 0, kcred, NULL, NULL);
			*zpp = ITOZ(ip);
			return (error);
		}
		rw_enter(&dzp->z_parent_lock, RW_READER);
		error = zfs_zget(zfsvfs, parent, &zp);
		if (error == 0)
			*zpp = zp;
		rw_exit(&dzp->z_parent_lock);
	} else if (zfs_has_ctldir(dzp) && strcmp(name, ZFS_CTLDIR_NAME) == 0) {
		ip = zfsctl_root(dzp);
		*zpp = ITOZ(ip);
	} else {
		int zf;

		zf = ZEXISTS | ZSHARED;
		if (flags & FIGNORECASE)
			zf |= ZCILOOK;

		error = zfs_dirent_lock(&dl, dzp, name, &zp, zf, deflg, rpnp);
		if (error == 0) {
			*zpp = zp;
			zfs_dirent_unlock(dl);
			dzp->z_zn_prefetch = B_TRUE;  
		}
		rpnp = NULL;
	}

	if ((flags & FIGNORECASE) && rpnp && !error)
		(void) strlcpy(rpnp->pn_buf, name, rpnp->pn_bufsize);

	return (error);
}

 
void
zfs_unlinked_add(znode_t *zp, dmu_tx_t *tx)
{
	zfsvfs_t *zfsvfs = ZTOZSB(zp);

	ASSERT(zp->z_unlinked);
	ASSERT(ZTOI(zp)->i_nlink == 0);

	VERIFY3U(0, ==,
	    zap_add_int(zfsvfs->z_os, zfsvfs->z_unlinkedobj, zp->z_id, tx));

	dataset_kstats_update_nunlinks_kstat(&zfsvfs->z_kstat, 1);
}

 
static void
zfs_unlinked_drain_task(void *arg)
{
	zfsvfs_t *zfsvfs = arg;
	zap_cursor_t	zc;
	zap_attribute_t zap;
	dmu_object_info_t doi;
	znode_t		*zp;
	int		error;

	ASSERT3B(zfsvfs->z_draining, ==, B_TRUE);

	 
	for (zap_cursor_init(&zc, zfsvfs->z_os, zfsvfs->z_unlinkedobj);
	    zap_cursor_retrieve(&zc, &zap) == 0 && !zfsvfs->z_drain_cancel;
	    zap_cursor_advance(&zc)) {

		 

		error = dmu_object_info(zfsvfs->z_os,
		    zap.za_first_integer, &doi);
		if (error != 0)
			continue;

		ASSERT((doi.doi_type == DMU_OT_PLAIN_FILE_CONTENTS) ||
		    (doi.doi_type == DMU_OT_DIRECTORY_CONTENTS));
		 
		error = zfs_zget(zfsvfs, zap.za_first_integer, &zp);

		 
		if (error != 0)
			continue;

		zp->z_unlinked = B_TRUE;

		 
		zrele(zp);
		ASSERT3B(zfsvfs->z_unmounted, ==, B_FALSE);
	}
	zap_cursor_fini(&zc);

	zfsvfs->z_draining = B_FALSE;
	zfsvfs->z_drain_task = TASKQID_INVALID;
}

 
void
zfs_unlinked_drain(zfsvfs_t *zfsvfs)
{
	ASSERT3B(zfsvfs->z_unmounted, ==, B_FALSE);
	ASSERT3B(zfsvfs->z_draining, ==, B_FALSE);

	zfsvfs->z_draining = B_TRUE;
	zfsvfs->z_drain_cancel = B_FALSE;

	zfsvfs->z_drain_task = taskq_dispatch(
	    dsl_pool_unlinked_drain_taskq(dmu_objset_pool(zfsvfs->z_os)),
	    zfs_unlinked_drain_task, zfsvfs, TQ_SLEEP);
	if (zfsvfs->z_drain_task == TASKQID_INVALID) {
		zfs_dbgmsg("async zfs_unlinked_drain dispatch failed");
		zfs_unlinked_drain_task(zfsvfs);
	}
}

 
void
zfs_unlinked_drain_stop_wait(zfsvfs_t *zfsvfs)
{
	ASSERT3B(zfsvfs->z_unmounted, ==, B_FALSE);

	if (zfsvfs->z_draining) {
		zfsvfs->z_drain_cancel = B_TRUE;
		taskq_cancel_id(dsl_pool_unlinked_drain_taskq(
		    dmu_objset_pool(zfsvfs->z_os)), zfsvfs->z_drain_task);
		zfsvfs->z_drain_task = TASKQID_INVALID;
		zfsvfs->z_draining = B_FALSE;
	}
}

 
static int
zfs_purgedir(znode_t *dzp)
{
	zap_cursor_t	zc;
	zap_attribute_t	zap;
	znode_t		*xzp;
	dmu_tx_t	*tx;
	zfsvfs_t	*zfsvfs = ZTOZSB(dzp);
	zfs_dirlock_t	dl;
	int skipped = 0;
	int error;

	for (zap_cursor_init(&zc, zfsvfs->z_os, dzp->z_id);
	    (error = zap_cursor_retrieve(&zc, &zap)) == 0;
	    zap_cursor_advance(&zc)) {
		error = zfs_zget(zfsvfs,
		    ZFS_DIRENT_OBJ(zap.za_first_integer), &xzp);
		if (error) {
			skipped += 1;
			continue;
		}

		ASSERT(S_ISREG(ZTOI(xzp)->i_mode) ||
		    S_ISLNK(ZTOI(xzp)->i_mode));

		tx = dmu_tx_create(zfsvfs->z_os);
		dmu_tx_hold_sa(tx, dzp->z_sa_hdl, B_FALSE);
		dmu_tx_hold_zap(tx, dzp->z_id, FALSE, zap.za_name);
		dmu_tx_hold_sa(tx, xzp->z_sa_hdl, B_FALSE);
		dmu_tx_hold_zap(tx, zfsvfs->z_unlinkedobj, FALSE, NULL);
		 
		zfs_sa_upgrade_txholds(tx, xzp);
		dmu_tx_mark_netfree(tx);
		error = dmu_tx_assign(tx, TXG_WAIT);
		if (error) {
			dmu_tx_abort(tx);
			zfs_zrele_async(xzp);
			skipped += 1;
			continue;
		}
		memset(&dl, 0, sizeof (dl));
		dl.dl_dzp = dzp;
		dl.dl_name = zap.za_name;

		error = zfs_link_destroy(&dl, xzp, tx, 0, NULL);
		if (error)
			skipped += 1;
		dmu_tx_commit(tx);

		zfs_zrele_async(xzp);
	}
	zap_cursor_fini(&zc);
	if (error != ENOENT)
		skipped += 1;
	return (skipped);
}

void
zfs_rmnode(znode_t *zp)
{
	zfsvfs_t	*zfsvfs = ZTOZSB(zp);
	objset_t	*os = zfsvfs->z_os;
	znode_t		*xzp = NULL;
	dmu_tx_t	*tx;
	znode_hold_t	*zh;
	uint64_t	z_id = zp->z_id;
	uint64_t	acl_obj;
	uint64_t	xattr_obj;
	uint64_t	links;
	int		error;

	ASSERT(ZTOI(zp)->i_nlink == 0);
	ASSERT(atomic_read(&ZTOI(zp)->i_count) == 0);

	 
	if (S_ISDIR(ZTOI(zp)->i_mode) && (zp->z_pflags & ZFS_XATTR)) {
		if (zfs_purgedir(zp) != 0) {
			 
			zh = zfs_znode_hold_enter(zfsvfs, z_id);
			zfs_znode_dmu_fini(zp);
			zfs_znode_hold_exit(zfsvfs, zh);
			return;
		}
	}

	 
	if (S_ISREG(ZTOI(zp)->i_mode)) {
		error = dmu_free_long_range(os, zp->z_id, 0, DMU_OBJECT_END);
		if (error) {
			 
			zh = zfs_znode_hold_enter(zfsvfs, z_id);
			zfs_znode_dmu_fini(zp);
			zfs_znode_hold_exit(zfsvfs, zh);
			return;
		}
	}

	 
	error = sa_lookup(zp->z_sa_hdl, SA_ZPL_XATTR(zfsvfs),
	    &xattr_obj, sizeof (xattr_obj));
	if (error == 0 && xattr_obj) {
		error = zfs_zget(zfsvfs, xattr_obj, &xzp);
		ASSERT(error == 0);
	}

	acl_obj = zfs_external_acl(zp);

	 
	tx = dmu_tx_create(os);
	dmu_tx_hold_free(tx, zp->z_id, 0, DMU_OBJECT_END);
	dmu_tx_hold_zap(tx, zfsvfs->z_unlinkedobj, FALSE, NULL);
	if (xzp) {
		dmu_tx_hold_zap(tx, zfsvfs->z_unlinkedobj, TRUE, NULL);
		dmu_tx_hold_sa(tx, xzp->z_sa_hdl, B_FALSE);
	}
	if (acl_obj)
		dmu_tx_hold_free(tx, acl_obj, 0, DMU_OBJECT_END);

	zfs_sa_upgrade_txholds(tx, zp);
	error = dmu_tx_assign(tx, TXG_WAIT);
	if (error) {
		 
		dmu_tx_abort(tx);
		zh = zfs_znode_hold_enter(zfsvfs, z_id);
		zfs_znode_dmu_fini(zp);
		zfs_znode_hold_exit(zfsvfs, zh);
		goto out;
	}

	if (xzp) {
		ASSERT(error == 0);
		mutex_enter(&xzp->z_lock);
		xzp->z_unlinked = B_TRUE;	 
		clear_nlink(ZTOI(xzp));		 
		links = 0;
		VERIFY(0 == sa_update(xzp->z_sa_hdl, SA_ZPL_LINKS(zfsvfs),
		    &links, sizeof (links), tx));
		mutex_exit(&xzp->z_lock);
		zfs_unlinked_add(xzp, tx);
	}

	mutex_enter(&os->os_dsl_dataset->ds_dir->dd_activity_lock);

	 
	error = zap_remove_int(zfsvfs->z_os, zfsvfs->z_unlinkedobj,
	    zp->z_id, tx);
	VERIFY(error == 0 || error == ENOENT);

	uint64_t count;
	if (zap_count(os, zfsvfs->z_unlinkedobj, &count) == 0 && count == 0) {
		cv_broadcast(&os->os_dsl_dataset->ds_dir->dd_activity_cv);
	}

	mutex_exit(&os->os_dsl_dataset->ds_dir->dd_activity_lock);

	dataset_kstats_update_nunlinked_kstat(&zfsvfs->z_kstat, 1);

	zfs_znode_delete(zp, tx);

	dmu_tx_commit(tx);
out:
	if (xzp)
		zfs_zrele_async(xzp);
}

static uint64_t
zfs_dirent(znode_t *zp, uint64_t mode)
{
	uint64_t de = zp->z_id;

	if (ZTOZSB(zp)->z_version >= ZPL_VERSION_DIRENT_TYPE)
		de |= IFTODT(mode) << 60;
	return (de);
}

 
int
zfs_link_create(zfs_dirlock_t *dl, znode_t *zp, dmu_tx_t *tx, int flag)
{
	znode_t *dzp = dl->dl_dzp;
	zfsvfs_t *zfsvfs = ZTOZSB(zp);
	uint64_t value;
	int zp_is_dir = S_ISDIR(ZTOI(zp)->i_mode);
	sa_bulk_attr_t bulk[5];
	uint64_t mtime[2], ctime[2];
	uint64_t links;
	int count = 0;
	int error;

	mutex_enter(&zp->z_lock);

	if (!(flag & ZRENAMING)) {
		if (zp->z_unlinked) {	 
			ASSERT(!(flag & (ZNEW | ZEXISTS)));
			mutex_exit(&zp->z_lock);
			return (SET_ERROR(ENOENT));
		}
		if (!(flag & ZNEW)) {
			 
			inc_nlink(ZTOI(zp));
			links = ZTOI(zp)->i_nlink;
			SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_LINKS(zfsvfs),
			    NULL, &links, sizeof (links));
		}
	}

	value = zfs_dirent(zp, zp->z_mode);
	error = zap_add(ZTOZSB(zp)->z_os, dzp->z_id, dl->dl_name, 8, 1,
	    &value, tx);

	 
	if (error != 0) {
		if (!(flag & ZRENAMING) && !(flag & ZNEW))
			drop_nlink(ZTOI(zp));
		mutex_exit(&zp->z_lock);
		return (error);
	}

	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_PARENT(zfsvfs), NULL,
	    &dzp->z_id, sizeof (dzp->z_id));
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_FLAGS(zfsvfs), NULL,
	    &zp->z_pflags, sizeof (zp->z_pflags));

	if (!(flag & ZNEW)) {
		SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_CTIME(zfsvfs), NULL,
		    ctime, sizeof (ctime));
		zfs_tstamp_update_setup(zp, STATE_CHANGED, mtime,
		    ctime);
	}
	error = sa_bulk_update(zp->z_sa_hdl, bulk, count, tx);
	ASSERT(error == 0);

	mutex_exit(&zp->z_lock);

	mutex_enter(&dzp->z_lock);
	dzp->z_size++;
	if (zp_is_dir)
		inc_nlink(ZTOI(dzp));
	links = ZTOI(dzp)->i_nlink;
	count = 0;
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_SIZE(zfsvfs), NULL,
	    &dzp->z_size, sizeof (dzp->z_size));
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_LINKS(zfsvfs), NULL,
	    &links, sizeof (links));
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_MTIME(zfsvfs), NULL,
	    mtime, sizeof (mtime));
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_CTIME(zfsvfs), NULL,
	    ctime, sizeof (ctime));
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_FLAGS(zfsvfs), NULL,
	    &dzp->z_pflags, sizeof (dzp->z_pflags));
	zfs_tstamp_update_setup(dzp, CONTENT_MODIFIED, mtime, ctime);
	error = sa_bulk_update(dzp->z_sa_hdl, bulk, count, tx);
	ASSERT(error == 0);
	mutex_exit(&dzp->z_lock);

	return (0);
}

 
static int
zfs_dropname(zfs_dirlock_t *dl, znode_t *zp, znode_t *dzp, dmu_tx_t *tx,
    int flag)
{
	int error;

	if (ZTOZSB(zp)->z_norm) {
		matchtype_t mt = MT_NORMALIZE;

		if ((ZTOZSB(zp)->z_case == ZFS_CASE_INSENSITIVE &&
		    (flag & ZCIEXACT)) ||
		    (ZTOZSB(zp)->z_case == ZFS_CASE_MIXED &&
		    !(flag & ZCILOOK))) {
			mt |= MT_MATCH_CASE;
		}

		error = zap_remove_norm(ZTOZSB(zp)->z_os, dzp->z_id,
		    dl->dl_name, mt, tx);
	} else {
		error = zap_remove(ZTOZSB(zp)->z_os, dzp->z_id, dl->dl_name,
		    tx);
	}

	return (error);
}

static int
zfs_drop_nlink_locked(znode_t *zp, dmu_tx_t *tx, boolean_t *unlinkedp)
{
	zfsvfs_t	*zfsvfs = ZTOZSB(zp);
	int		zp_is_dir = S_ISDIR(ZTOI(zp)->i_mode);
	boolean_t	unlinked = B_FALSE;
	sa_bulk_attr_t	bulk[3];
	uint64_t	mtime[2], ctime[2];
	uint64_t	links;
	int		count = 0;
	int		error;

	if (zp_is_dir && !zfs_dirempty(zp))
		return (SET_ERROR(ENOTEMPTY));

	if (ZTOI(zp)->i_nlink <= zp_is_dir) {
		zfs_panic_recover("zfs: link count on %lu is %u, "
		    "should be at least %u", zp->z_id,
		    (int)ZTOI(zp)->i_nlink, zp_is_dir + 1);
		set_nlink(ZTOI(zp), zp_is_dir + 1);
	}
	drop_nlink(ZTOI(zp));
	if (ZTOI(zp)->i_nlink == zp_is_dir) {
		zp->z_unlinked = B_TRUE;
		clear_nlink(ZTOI(zp));
		unlinked = B_TRUE;
	} else {
		SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_CTIME(zfsvfs),
		    NULL, &ctime, sizeof (ctime));
		SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_FLAGS(zfsvfs),
		    NULL, &zp->z_pflags, sizeof (zp->z_pflags));
		zfs_tstamp_update_setup(zp, STATE_CHANGED, mtime,
		    ctime);
	}
	links = ZTOI(zp)->i_nlink;
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_LINKS(zfsvfs),
	    NULL, &links, sizeof (links));
	error = sa_bulk_update(zp->z_sa_hdl, bulk, count, tx);
	ASSERT3U(error, ==, 0);

	if (unlinkedp != NULL)
		*unlinkedp = unlinked;
	else if (unlinked)
		zfs_unlinked_add(zp, tx);

	return (0);
}

 
int
zfs_drop_nlink(znode_t *zp, dmu_tx_t *tx, boolean_t *unlinkedp)
{
	int error;

	mutex_enter(&zp->z_lock);
	error = zfs_drop_nlink_locked(zp, tx, unlinkedp);
	mutex_exit(&zp->z_lock);

	return (error);
}

 
int
zfs_link_destroy(zfs_dirlock_t *dl, znode_t *zp, dmu_tx_t *tx, int flag,
    boolean_t *unlinkedp)
{
	znode_t *dzp = dl->dl_dzp;
	zfsvfs_t *zfsvfs = ZTOZSB(dzp);
	int zp_is_dir = S_ISDIR(ZTOI(zp)->i_mode);
	boolean_t unlinked = B_FALSE;
	sa_bulk_attr_t bulk[5];
	uint64_t mtime[2], ctime[2];
	uint64_t links;
	int count = 0;
	int error;

	if (!(flag & ZRENAMING)) {
		mutex_enter(&zp->z_lock);

		if (zp_is_dir && !zfs_dirempty(zp)) {
			mutex_exit(&zp->z_lock);
			return (SET_ERROR(ENOTEMPTY));
		}

		 
		error = zfs_dropname(dl, zp, dzp, tx, flag);
		if (error != 0) {
			mutex_exit(&zp->z_lock);
			return (error);
		}

		 
		error = zfs_drop_nlink_locked(zp, tx, &unlinked);
		ASSERT3U(error, ==, 0);
		mutex_exit(&zp->z_lock);
	} else {
		error = zfs_dropname(dl, zp, dzp, tx, flag);
		if (error != 0)
			return (error);
	}

	mutex_enter(&dzp->z_lock);
	dzp->z_size--;		 
	if (zp_is_dir)
		drop_nlink(ZTOI(dzp));	 
	links = ZTOI(dzp)->i_nlink;
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_LINKS(zfsvfs),
	    NULL, &links, sizeof (links));
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_SIZE(zfsvfs),
	    NULL, &dzp->z_size, sizeof (dzp->z_size));
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_CTIME(zfsvfs),
	    NULL, ctime, sizeof (ctime));
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_MTIME(zfsvfs),
	    NULL, mtime, sizeof (mtime));
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_FLAGS(zfsvfs),
	    NULL, &dzp->z_pflags, sizeof (dzp->z_pflags));
	zfs_tstamp_update_setup(dzp, CONTENT_MODIFIED, mtime, ctime);
	error = sa_bulk_update(dzp->z_sa_hdl, bulk, count, tx);
	ASSERT(error == 0);
	mutex_exit(&dzp->z_lock);

	if (unlinkedp != NULL)
		*unlinkedp = unlinked;
	else if (unlinked)
		zfs_unlinked_add(zp, tx);

	return (0);
}

 
boolean_t
zfs_dirempty(znode_t *dzp)
{
	zfsvfs_t *zfsvfs = ZTOZSB(dzp);
	uint64_t count;
	int error;

	if (dzp->z_dirlocks != NULL)
		return (B_FALSE);

	error = zap_count(zfsvfs->z_os, dzp->z_id, &count);
	if (error != 0 || count != 0)
		return (B_FALSE);

	return (B_TRUE);
}

int
zfs_make_xattrdir(znode_t *zp, vattr_t *vap, znode_t **xzpp, cred_t *cr)
{
	zfsvfs_t *zfsvfs = ZTOZSB(zp);
	znode_t *xzp;
	dmu_tx_t *tx;
	int error;
	zfs_acl_ids_t acl_ids;
	boolean_t fuid_dirtied;
#ifdef ZFS_DEBUG
	uint64_t parent;
#endif

	*xzpp = NULL;

	if ((error = zfs_acl_ids_create(zp, IS_XATTR, vap, cr, NULL,
	    &acl_ids, zfs_init_idmap)) != 0)
		return (error);
	if (zfs_acl_ids_overquota(zfsvfs, &acl_ids, zp->z_projid)) {
		zfs_acl_ids_free(&acl_ids);
		return (SET_ERROR(EDQUOT));
	}

	tx = dmu_tx_create(zfsvfs->z_os);
	dmu_tx_hold_sa_create(tx, acl_ids.z_aclp->z_acl_bytes +
	    ZFS_SA_BASE_ATTR_SIZE);
	dmu_tx_hold_sa(tx, zp->z_sa_hdl, B_TRUE);
	dmu_tx_hold_zap(tx, DMU_NEW_OBJECT, FALSE, NULL);
	fuid_dirtied = zfsvfs->z_fuid_dirty;
	if (fuid_dirtied)
		zfs_fuid_txhold(zfsvfs, tx);
	error = dmu_tx_assign(tx, TXG_WAIT);
	if (error) {
		zfs_acl_ids_free(&acl_ids);
		dmu_tx_abort(tx);
		return (error);
	}
	zfs_mknode(zp, vap, tx, cr, IS_XATTR, &xzp, &acl_ids);

	if (fuid_dirtied)
		zfs_fuid_sync(zfsvfs, tx);

#ifdef ZFS_DEBUG
	error = sa_lookup(xzp->z_sa_hdl, SA_ZPL_PARENT(zfsvfs),
	    &parent, sizeof (parent));
	ASSERT(error == 0 && parent == zp->z_id);
#endif

	VERIFY(0 == sa_update(zp->z_sa_hdl, SA_ZPL_XATTR(zfsvfs), &xzp->z_id,
	    sizeof (xzp->z_id), tx));

	if (!zp->z_unlinked)
		zfs_log_create(zfsvfs->z_log, tx, TX_MKXATTR, zp, xzp, "", NULL,
		    acl_ids.z_fuidp, vap);

	zfs_acl_ids_free(&acl_ids);
	dmu_tx_commit(tx);

	*xzpp = xzp;

	return (0);
}

 
int
zfs_get_xattrdir(znode_t *zp, znode_t **xzpp, cred_t *cr, int flags)
{
	zfsvfs_t	*zfsvfs = ZTOZSB(zp);
	znode_t		*xzp;
	zfs_dirlock_t	*dl;
	vattr_t		va;
	int		error;
top:
	error = zfs_dirent_lock(&dl, zp, "", &xzp, ZXATTR, NULL, NULL);
	if (error)
		return (error);

	if (xzp != NULL) {
		*xzpp = xzp;
		zfs_dirent_unlock(dl);
		return (0);
	}

	if (!(flags & CREATE_XATTR_DIR)) {
		zfs_dirent_unlock(dl);
		return (SET_ERROR(ENOENT));
	}

	if (zfs_is_readonly(zfsvfs)) {
		zfs_dirent_unlock(dl);
		return (SET_ERROR(EROFS));
	}

	 
	va.va_mask = ATTR_MODE | ATTR_UID | ATTR_GID;
	va.va_mode = S_IFDIR | S_ISVTX | 0777;
	zfs_fuid_map_ids(zp, cr, &va.va_uid, &va.va_gid);

	va.va_dentry = NULL;
	error = zfs_make_xattrdir(zp, &va, xzpp, cr);
	zfs_dirent_unlock(dl);

	if (error == ERESTART) {
		 
		goto top;
	}

	return (error);
}

 
int
zfs_sticky_remove_access(znode_t *zdp, znode_t *zp, cred_t *cr)
{
	uid_t		uid;
	uid_t		downer;
	uid_t		fowner;
	zfsvfs_t	*zfsvfs = ZTOZSB(zdp);

	if (zfsvfs->z_replay)
		return (0);

	if ((zdp->z_mode & S_ISVTX) == 0)
		return (0);

	downer = zfs_fuid_map_id(zfsvfs, KUID_TO_SUID(ZTOI(zdp)->i_uid),
	    cr, ZFS_OWNER);
	fowner = zfs_fuid_map_id(zfsvfs, KUID_TO_SUID(ZTOI(zp)->i_uid),
	    cr, ZFS_OWNER);

	if ((uid = crgetuid(cr)) == downer || uid == fowner ||
	    zfs_zaccess(zp, ACE_WRITE_DATA, 0, B_FALSE, cr,
	    zfs_init_idmap) == 0)
		return (0);
	else
		return (secpolicy_vnode_remove(cr));
}
