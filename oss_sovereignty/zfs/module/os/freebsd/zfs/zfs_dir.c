#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/resource.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/kmem.h>
#include <sys/uio.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <sys/sunddi.h>
#include <sys/random.h>
#include <sys/policy.h>
#include <sys/condvar.h>
#include <sys/callb.h>
#include <sys/smp.h>
#include <sys/zfs_dir.h>
#include <sys/zfs_acl.h>
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
#include <sys/ccompat.h>
static int
zfs_match_find(zfsvfs_t *zfsvfs, znode_t *dzp, const char *name,
    matchtype_t mt, uint64_t *zoid)
{
	int error;
	if (zfsvfs->z_norm) {
		error = zap_lookup_norm(zfsvfs->z_os, dzp->z_id, name, 8, 1,
		    zoid, mt, NULL, 0, NULL);
	} else {
		error = zap_lookup(zfsvfs->z_os, dzp->z_id, name, 8, 1, zoid);
	}
	*zoid = ZFS_DIRENT_OBJ(*zoid);
	return (error);
}
int
zfs_dirent_lookup(znode_t *dzp, const char *name, znode_t **zpp, int flag)
{
	zfsvfs_t	*zfsvfs = dzp->z_zfsvfs;
	znode_t		*zp;
	matchtype_t	mt = 0;
	uint64_t	zoid;
	int		error = 0;
	if (zfsvfs->z_replay == B_FALSE)
		ASSERT_VOP_LOCKED(ZTOV(dzp), __func__);
	*zpp = NULL;
	if (name[0] == '.' &&
	    (((name[1] == '\0') || (name[1] == '.' && name[2] == '\0')) ||
	    (zfs_has_ctldir(dzp) && strcmp(name, ZFS_CTLDIR_NAME) == 0)))
		return (SET_ERROR(EEXIST));
	if (zfsvfs->z_norm != 0) {
		mt = MT_NORMALIZE;
		if (zfsvfs->z_case == ZFS_CASE_MIXED) {
			mt |= MT_MATCH_CASE;
		}
	}
	if (dzp->z_unlinked && !(flag & ZXATTR))
		return (ENOENT);
	if (flag & ZXATTR) {
		error = sa_lookup(dzp->z_sa_hdl, SA_ZPL_XATTR(zfsvfs), &zoid,
		    sizeof (zoid));
		if (error == 0)
			error = (zoid == 0 ? ENOENT : 0);
	} else {
		error = zfs_match_find(zfsvfs, dzp, name, mt, &zoid);
	}
	if (error) {
		if (error != ENOENT || (flag & ZEXISTS)) {
			return (error);
		}
	} else {
		if (flag & ZNEW) {
			return (SET_ERROR(EEXIST));
		}
		error = zfs_zget(zfsvfs, zoid, &zp);
		if (error)
			return (error);
		ASSERT(!zp->z_unlinked);
		*zpp = zp;
	}
	return (0);
}
static int
zfs_dd_lookup(znode_t *dzp, znode_t **zpp)
{
	zfsvfs_t *zfsvfs = dzp->z_zfsvfs;
	znode_t *zp;
	uint64_t parent;
	int error;
#ifdef ZFS_DEBUG
	if (zfsvfs->z_replay == B_FALSE)
		ASSERT_VOP_LOCKED(ZTOV(dzp), __func__);
#endif
	if (dzp->z_unlinked)
		return (ENOENT);
	if ((error = sa_lookup(dzp->z_sa_hdl,
	    SA_ZPL_PARENT(zfsvfs), &parent, sizeof (parent))) != 0)
		return (error);
	error = zfs_zget(zfsvfs, parent, &zp);
	if (error == 0)
		*zpp = zp;
	return (error);
}
int
zfs_dirlook(znode_t *dzp, const char *name, znode_t **zpp)
{
	zfsvfs_t *zfsvfs __unused = dzp->z_zfsvfs;
	znode_t *zp = NULL;
	int error = 0;
#ifdef ZFS_DEBUG
	if (zfsvfs->z_replay == B_FALSE)
		ASSERT_VOP_LOCKED(ZTOV(dzp), __func__);
#endif
	if (dzp->z_unlinked)
		return (SET_ERROR(ENOENT));
	if (name[0] == 0 || (name[0] == '.' && name[1] == 0)) {
		*zpp = dzp;
	} else if (name[0] == '.' && name[1] == '.' && name[2] == 0) {
		error = zfs_dd_lookup(dzp, &zp);
		if (error == 0)
			*zpp = zp;
	} else {
		error = zfs_dirent_lookup(dzp, name, &zp, ZEXISTS);
		if (error == 0) {
			dzp->z_zn_prefetch = B_TRUE;  
			*zpp = zp;
		}
	}
	return (error);
}
void
zfs_unlinked_add(znode_t *zp, dmu_tx_t *tx)
{
	zfsvfs_t *zfsvfs = zp->z_zfsvfs;
	ASSERT(zp->z_unlinked);
	ASSERT3U(zp->z_links, ==, 0);
	VERIFY0(zap_add_int(zfsvfs->z_os, zfsvfs->z_unlinkedobj, zp->z_id, tx));
	dataset_kstats_update_nunlinks_kstat(&zfsvfs->z_kstat, 1);
}
void
zfs_unlinked_drain(zfsvfs_t *zfsvfs)
{
	zap_cursor_t	zc;
	zap_attribute_t zap;
	dmu_object_info_t doi;
	znode_t		*zp;
	dmu_tx_t	*tx;
	int		error;
	for (zap_cursor_init(&zc, zfsvfs->z_os, zfsvfs->z_unlinkedobj);
	    zap_cursor_retrieve(&zc, &zap) == 0;
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
		vn_lock(ZTOV(zp), LK_EXCLUSIVE | LK_RETRY);
		if (zp->z_links != 0) {
			tx = dmu_tx_create(zfsvfs->z_os);
			dmu_tx_hold_sa(tx, zp->z_sa_hdl, B_FALSE);
			error = dmu_tx_assign(tx, TXG_WAIT);
			if (error != 0) {
				dmu_tx_abort(tx);
				vput(ZTOV(zp));
				continue;
			}
			zp->z_links = 0;
			VERIFY0(sa_update(zp->z_sa_hdl, SA_ZPL_LINKS(zfsvfs),
			    &zp->z_links, sizeof (zp->z_links), tx));
			dmu_tx_commit(tx);
		}
		zp->z_unlinked = B_TRUE;
		vput(ZTOV(zp));
	}
	zap_cursor_fini(&zc);
}
static int
zfs_purgedir(znode_t *dzp)
{
	zap_cursor_t	zc;
	zap_attribute_t	zap;
	znode_t		*xzp;
	dmu_tx_t	*tx;
	zfsvfs_t	*zfsvfs = dzp->z_zfsvfs;
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
		vn_lock(ZTOV(xzp), LK_EXCLUSIVE | LK_RETRY);
		ASSERT((ZTOV(xzp)->v_type == VREG) ||
		    (ZTOV(xzp)->v_type == VLNK));
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
			vput(ZTOV(xzp));
			skipped += 1;
			continue;
		}
		error = zfs_link_destroy(dzp, zap.za_name, xzp, tx, 0, NULL);
		if (error)
			skipped += 1;
		dmu_tx_commit(tx);
		vput(ZTOV(xzp));
	}
	zap_cursor_fini(&zc);
	if (error != ENOENT)
		skipped += 1;
	return (skipped);
}
extern taskq_t *zfsvfs_taskq;
void
zfs_rmnode(znode_t *zp)
{
	zfsvfs_t	*zfsvfs = zp->z_zfsvfs;
	objset_t	*os = zfsvfs->z_os;
	dmu_tx_t	*tx;
	uint64_t	z_id = zp->z_id;
	uint64_t	acl_obj;
	uint64_t	xattr_obj;
	uint64_t	count;
	int		error;
	ASSERT3U(zp->z_links, ==, 0);
	if (zfsvfs->z_replay == B_FALSE)
		ASSERT_VOP_ELOCKED(ZTOV(zp), __func__);
	if (ZTOV(zp) != NULL && ZTOV(zp)->v_type == VDIR &&
	    (zp->z_pflags & ZFS_XATTR)) {
		if (zfs_purgedir(zp) != 0) {
			ZFS_OBJ_HOLD_ENTER(zfsvfs, z_id);
			zfs_znode_dmu_fini(zp);
			zfs_znode_free(zp);
			ZFS_OBJ_HOLD_EXIT(zfsvfs, z_id);
			return;
		}
	} else {
		error = dmu_free_long_range(os, zp->z_id, 0, DMU_OBJECT_END);
		if (error) {
			ZFS_OBJ_HOLD_ENTER(zfsvfs, z_id);
			zfs_znode_dmu_fini(zp);
			zfs_znode_free(zp);
			ZFS_OBJ_HOLD_EXIT(zfsvfs, z_id);
			return;
		}
	}
	error = sa_lookup(zp->z_sa_hdl, SA_ZPL_XATTR(zfsvfs),
	    &xattr_obj, sizeof (xattr_obj));
	if (error)
		xattr_obj = 0;
	acl_obj = zfs_external_acl(zp);
	tx = dmu_tx_create(os);
	dmu_tx_hold_free(tx, zp->z_id, 0, DMU_OBJECT_END);
	dmu_tx_hold_zap(tx, zfsvfs->z_unlinkedobj, FALSE, NULL);
	if (xattr_obj)
		dmu_tx_hold_zap(tx, zfsvfs->z_unlinkedobj, TRUE, NULL);
	if (acl_obj)
		dmu_tx_hold_free(tx, acl_obj, 0, DMU_OBJECT_END);
	zfs_sa_upgrade_txholds(tx, zp);
	error = dmu_tx_assign(tx, TXG_WAIT);
	if (error) {
		dmu_tx_abort(tx);
		ZFS_OBJ_HOLD_ENTER(zfsvfs, z_id);
		zfs_znode_dmu_fini(zp);
		zfs_znode_free(zp);
		ZFS_OBJ_HOLD_EXIT(zfsvfs, z_id);
		return;
	}
	if (xattr_obj) {
		VERIFY3U(0, ==,
		    zap_add_int(os, zfsvfs->z_unlinkedobj, xattr_obj, tx));
	}
	mutex_enter(&os->os_dsl_dataset->ds_dir->dd_activity_lock);
	VERIFY3U(0, ==,
	    zap_remove_int(os, zfsvfs->z_unlinkedobj, zp->z_id, tx));
	if (zap_count(os, zfsvfs->z_unlinkedobj, &count) == 0 && count == 0) {
		cv_broadcast(&os->os_dsl_dataset->ds_dir->dd_activity_cv);
	}
	mutex_exit(&os->os_dsl_dataset->ds_dir->dd_activity_lock);
	dataset_kstats_update_nunlinked_kstat(&zfsvfs->z_kstat, 1);
	zfs_znode_delete(zp, tx);
	dmu_tx_commit(tx);
	if (xattr_obj) {
		taskqueue_enqueue(zfsvfs_taskq->tq_queue,
		    &zfsvfs->z_unlinked_drain_task);
	}
}
static uint64_t
zfs_dirent(znode_t *zp, uint64_t mode)
{
	uint64_t de = zp->z_id;
	if (zp->z_zfsvfs->z_version >= ZPL_VERSION_DIRENT_TYPE)
		de |= IFTODT(mode) << 60;
	return (de);
}
int
zfs_link_create(znode_t *dzp, const char *name, znode_t *zp, dmu_tx_t *tx,
    int flag)
{
	zfsvfs_t *zfsvfs = zp->z_zfsvfs;
	vnode_t *vp = ZTOV(zp);
	uint64_t value;
	int zp_is_dir = (vp->v_type == VDIR);
	sa_bulk_attr_t bulk[5];
	uint64_t mtime[2], ctime[2];
	int count = 0;
	int error;
	if (zfsvfs->z_replay == B_FALSE) {
		ASSERT_VOP_ELOCKED(ZTOV(dzp), __func__);
		ASSERT_VOP_ELOCKED(ZTOV(zp), __func__);
	}
	if (zp_is_dir) {
		if (dzp->z_links >= ZFS_LINK_MAX)
			return (SET_ERROR(EMLINK));
	}
	if (!(flag & ZRENAMING)) {
		if (zp->z_unlinked) {	 
			ASSERT(!(flag & (ZNEW | ZEXISTS)));
			return (SET_ERROR(ENOENT));
		}
		if (zp->z_links >= ZFS_LINK_MAX - zp_is_dir) {
			return (SET_ERROR(EMLINK));
		}
		zp->z_links++;
		SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_LINKS(zfsvfs), NULL,
		    &zp->z_links, sizeof (zp->z_links));
	} else {
		ASSERT(!zp->z_unlinked);
	}
	value = zfs_dirent(zp, zp->z_mode);
	error = zap_add(zp->z_zfsvfs->z_os, dzp->z_id, name,
	    8, 1, &value, tx);
	if (error != 0) {
		if (!(flag & ZRENAMING) && !(flag & ZNEW))
			zp->z_links--;
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
	ASSERT0(error);
	dzp->z_size++;
	dzp->z_links += zp_is_dir;
	count = 0;
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_SIZE(zfsvfs), NULL,
	    &dzp->z_size, sizeof (dzp->z_size));
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_LINKS(zfsvfs), NULL,
	    &dzp->z_links, sizeof (dzp->z_links));
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_MTIME(zfsvfs), NULL,
	    mtime, sizeof (mtime));
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_CTIME(zfsvfs), NULL,
	    ctime, sizeof (ctime));
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_FLAGS(zfsvfs), NULL,
	    &dzp->z_pflags, sizeof (dzp->z_pflags));
	zfs_tstamp_update_setup(dzp, CONTENT_MODIFIED, mtime, ctime);
	error = sa_bulk_update(dzp->z_sa_hdl, bulk, count, tx);
	ASSERT0(error);
	return (0);
}
static int
zfs_dropname(znode_t *dzp, const char *name, znode_t *zp, dmu_tx_t *tx,
    int flag)
{
	int error;
	if (zp->z_zfsvfs->z_norm) {
		matchtype_t mt = MT_NORMALIZE;
		if (zp->z_zfsvfs->z_case == ZFS_CASE_MIXED) {
			mt |= MT_MATCH_CASE;
		}
		error = zap_remove_norm(zp->z_zfsvfs->z_os, dzp->z_id,
		    name, mt, tx);
	} else {
		error = zap_remove(zp->z_zfsvfs->z_os, dzp->z_id, name, tx);
	}
	return (error);
}
int
zfs_link_destroy(znode_t *dzp, const char *name, znode_t *zp, dmu_tx_t *tx,
    int flag, boolean_t *unlinkedp)
{
	zfsvfs_t *zfsvfs = dzp->z_zfsvfs;
	vnode_t *vp = ZTOV(zp);
	int zp_is_dir = (vp->v_type == VDIR);
	boolean_t unlinked = B_FALSE;
	sa_bulk_attr_t bulk[5];
	uint64_t mtime[2], ctime[2];
	int count = 0;
	int error;
	if (zfsvfs->z_replay == B_FALSE) {
		ASSERT_VOP_ELOCKED(ZTOV(dzp), __func__);
		ASSERT_VOP_ELOCKED(ZTOV(zp), __func__);
	}
	if (!(flag & ZRENAMING)) {
		if (zp_is_dir && !zfs_dirempty(zp))
			return (SET_ERROR(ENOTEMPTY));
		error = zfs_dropname(dzp, name, zp, tx, flag);
		if (error != 0) {
			return (error);
		}
		if (zp->z_links <= zp_is_dir) {
			zfs_panic_recover("zfs: link count on vnode %p is %u, "
			    "should be at least %u", zp->z_vnode,
			    (int)zp->z_links,
			    zp_is_dir + 1);
			zp->z_links = zp_is_dir + 1;
		}
		if (--zp->z_links == zp_is_dir) {
			zp->z_unlinked = B_TRUE;
			zp->z_links = 0;
			unlinked = B_TRUE;
		} else {
			SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_CTIME(zfsvfs),
			    NULL, &ctime, sizeof (ctime));
			SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_FLAGS(zfsvfs),
			    NULL, &zp->z_pflags, sizeof (zp->z_pflags));
			zfs_tstamp_update_setup(zp, STATE_CHANGED, mtime,
			    ctime);
		}
		SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_LINKS(zfsvfs),
		    NULL, &zp->z_links, sizeof (zp->z_links));
		error = sa_bulk_update(zp->z_sa_hdl, bulk, count, tx);
		count = 0;
		ASSERT0(error);
	} else {
		ASSERT(!zp->z_unlinked);
		error = zfs_dropname(dzp, name, zp, tx, flag);
		if (error != 0)
			return (error);
	}
	dzp->z_size--;		 
	dzp->z_links -= zp_is_dir;	 
	SA_ADD_BULK_ATTR(bulk, count, SA_ZPL_LINKS(zfsvfs),
	    NULL, &dzp->z_links, sizeof (dzp->z_links));
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
	ASSERT0(error);
	if (unlinkedp != NULL)
		*unlinkedp = unlinked;
	else if (unlinked)
		zfs_unlinked_add(zp, tx);
	return (0);
}
boolean_t
zfs_dirempty(znode_t *dzp)
{
	return (dzp->z_size == 2);
}
int
zfs_make_xattrdir(znode_t *zp, vattr_t *vap, znode_t **xvpp, cred_t *cr)
{
	zfsvfs_t *zfsvfs = zp->z_zfsvfs;
	znode_t *xzp;
	dmu_tx_t *tx;
	int error;
	zfs_acl_ids_t acl_ids;
	boolean_t fuid_dirtied;
	uint64_t parent __maybe_unused;
	*xvpp = NULL;
	if ((error = zfs_acl_ids_create(zp, IS_XATTR, vap, cr, NULL,
	    &acl_ids, NULL)) != 0)
		return (error);
	if (zfs_acl_ids_overquota(zfsvfs, &acl_ids, 0)) {
		zfs_acl_ids_free(&acl_ids);
		return (SET_ERROR(EDQUOT));
	}
	getnewvnode_reserve_();
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
		getnewvnode_drop_reserve();
		return (error);
	}
	zfs_mknode(zp, vap, tx, cr, IS_XATTR, &xzp, &acl_ids);
	if (fuid_dirtied)
		zfs_fuid_sync(zfsvfs, tx);
	ASSERT0(sa_lookup(xzp->z_sa_hdl, SA_ZPL_PARENT(zfsvfs), &parent,
	    sizeof (parent)));
	ASSERT3U(parent, ==, zp->z_id);
	VERIFY0(sa_update(zp->z_sa_hdl, SA_ZPL_XATTR(zfsvfs), &xzp->z_id,
	    sizeof (xzp->z_id), tx));
	zfs_log_create(zfsvfs->z_log, tx, TX_MKXATTR, zp, xzp, "", NULL,
	    acl_ids.z_fuidp, vap);
	zfs_acl_ids_free(&acl_ids);
	dmu_tx_commit(tx);
	getnewvnode_drop_reserve();
	*xvpp = xzp;
	return (0);
}
int
zfs_get_xattrdir(znode_t *zp, znode_t **xzpp, cred_t *cr, int flags)
{
	zfsvfs_t	*zfsvfs = zp->z_zfsvfs;
	znode_t		*xzp;
	vattr_t		va;
	int		error;
top:
	error = zfs_dirent_lookup(zp, "", &xzp, ZXATTR);
	if (error)
		return (error);
	if (xzp != NULL) {
		*xzpp = xzp;
		return (0);
	}
	if (!(flags & CREATE_XATTR_DIR))
		return (SET_ERROR(ENOATTR));
	if (zfsvfs->z_vfs->vfs_flag & VFS_RDONLY) {
		return (SET_ERROR(EROFS));
	}
	va.va_mask = AT_MODE | AT_UID | AT_GID;
	va.va_type = VDIR;
	va.va_mode = S_IFDIR | S_ISVTX | 0777;
	zfs_fuid_map_ids(zp, cr, &va.va_uid, &va.va_gid);
	error = zfs_make_xattrdir(zp, &va, xzpp, cr);
	if (error == ERESTART) {
		goto top;
	}
	if (error == 0)
		VOP_UNLOCK1(ZTOV(*xzpp));
	return (error);
}
int
zfs_sticky_remove_access(znode_t *zdp, znode_t *zp, cred_t *cr)
{
	uid_t  		uid;
	uid_t		downer;
	uid_t		fowner;
	zfsvfs_t	*zfsvfs = zdp->z_zfsvfs;
	if (zdp->z_zfsvfs->z_replay)
		return (0);
	if ((zdp->z_mode & S_ISVTX) == 0)
		return (0);
	downer = zfs_fuid_map_id(zfsvfs, zdp->z_uid, cr, ZFS_OWNER);
	fowner = zfs_fuid_map_id(zfsvfs, zp->z_uid, cr, ZFS_OWNER);
	if ((uid = crgetuid(cr)) == downer || uid == fowner ||
	    (ZTOV(zp)->v_type == VREG &&
	    zfs_zaccess(zp, ACE_WRITE_DATA, 0, B_FALSE, cr, NULL) == 0))
		return (0);
	else
		return (secpolicy_vnode_remove(ZTOV(zp), cr));
}
