 
 

 

#include <sys/dmu_tx.h>
#include <sys/spa.h>
#include <sys/spa_impl.h>
#include <sys/zap.h>
#include <sys/zio.h>
#include <sys/dsl_dir.h>
#include <sys/dmu_objset.h>
#include <sys/dbuf.h>
#include <sys/zfs_znode.h>

#define	NAME_MAX_LEN 64

typedef struct clones {
	uint64_t clone_ds;
	list_node_t node;
} clones_t;

 
static uint_t spa_upgrade_errlog_limit = 0;

 
static void
bookmark_to_name(zbookmark_phys_t *zb, char *buf, size_t len)
{
	(void) snprintf(buf, len, "%llx:%llx:%llx:%llx",
	    (u_longlong_t)zb->zb_objset, (u_longlong_t)zb->zb_object,
	    (u_longlong_t)zb->zb_level, (u_longlong_t)zb->zb_blkid);
}

 
static void
errphys_to_name(zbookmark_err_phys_t *zep, char *buf, size_t len)
{
	(void) snprintf(buf, len, "%llx:%llx:%llx:%llx",
	    (u_longlong_t)zep->zb_object, (u_longlong_t)zep->zb_level,
	    (u_longlong_t)zep->zb_blkid, (u_longlong_t)zep->zb_birth);
}

 
void
name_to_errphys(char *buf, zbookmark_err_phys_t *zep)
{
	zep->zb_object = zfs_strtonum(buf, &buf);
	ASSERT(*buf == ':');
	zep->zb_level = (int)zfs_strtonum(buf + 1, &buf);
	ASSERT(*buf == ':');
	zep->zb_blkid = zfs_strtonum(buf + 1, &buf);
	ASSERT(*buf == ':');
	zep->zb_birth = zfs_strtonum(buf + 1, &buf);
	ASSERT(*buf == '\0');
}

 
static void
name_to_bookmark(char *buf, zbookmark_phys_t *zb)
{
	zb->zb_objset = zfs_strtonum(buf, &buf);
	ASSERT(*buf == ':');
	zb->zb_object = zfs_strtonum(buf + 1, &buf);
	ASSERT(*buf == ':');
	zb->zb_level = (int)zfs_strtonum(buf + 1, &buf);
	ASSERT(*buf == ':');
	zb->zb_blkid = zfs_strtonum(buf + 1, &buf);
	ASSERT(*buf == '\0');
}

void
zep_to_zb(uint64_t dataset, zbookmark_err_phys_t *zep, zbookmark_phys_t *zb)
{
	zb->zb_objset = dataset;
	zb->zb_object = zep->zb_object;
	zb->zb_level = zep->zb_level;
	zb->zb_blkid = zep->zb_blkid;
}

static void
name_to_object(char *buf, uint64_t *obj)
{
	*obj = zfs_strtonum(buf, &buf);
	ASSERT(*buf == '\0');
}

 
static int get_head_ds(spa_t *spa, uint64_t dsobj, uint64_t *head_ds)
{
	dsl_dataset_t *ds;
	int error = dsl_dataset_hold_obj_flags(spa->spa_dsl_pool,
	    dsobj, DS_HOLD_FLAG_DECRYPT, FTAG, &ds);

	if (error != 0)
		return (error);

	ASSERT(head_ds);
	*head_ds = dsl_dir_phys(ds->ds_dir)->dd_head_dataset_obj;
	dsl_dataset_rele_flags(ds, DS_HOLD_FLAG_DECRYPT, FTAG);

	return (error);
}

 
void
spa_log_error(spa_t *spa, const zbookmark_phys_t *zb, const uint64_t *birth)
{
	spa_error_entry_t search;
	spa_error_entry_t *new;
	avl_tree_t *tree;
	avl_index_t where;

	 
	if (spa_load_state(spa) == SPA_LOAD_TRYIMPORT)
		return;

	mutex_enter(&spa->spa_errlist_lock);

	 
	if (spa->spa_scrub_active || spa->spa_scrub_finished)
		tree = &spa->spa_errlist_scrub;
	else
		tree = &spa->spa_errlist_last;

	search.se_bookmark = *zb;
	if (avl_find(tree, &search, &where) != NULL) {
		mutex_exit(&spa->spa_errlist_lock);
		return;
	}

	new = kmem_zalloc(sizeof (spa_error_entry_t), KM_SLEEP);
	new->se_bookmark = *zb;

	 
	if (spa_feature_is_enabled(spa, SPA_FEATURE_HEAD_ERRLOG)) {
		new->se_zep.zb_object = zb->zb_object;
		new->se_zep.zb_level = zb->zb_level;
		new->se_zep.zb_blkid = zb->zb_blkid;

		 
		if (birth != NULL)
			new->se_zep.zb_birth = *birth;
	}

	avl_insert(tree, new, where);
	mutex_exit(&spa->spa_errlist_lock);
}

int
find_birth_txg(dsl_dataset_t *ds, zbookmark_err_phys_t *zep,
    uint64_t *birth_txg)
{
	objset_t *os;
	int error = dmu_objset_from_ds(ds, &os);
	if (error != 0)
		return (error);

	dnode_t *dn;
	blkptr_t bp;

	error = dnode_hold(os, zep->zb_object, FTAG, &dn);
	if (error != 0)
		return (error);

	rw_enter(&dn->dn_struct_rwlock, RW_READER);
	error = dbuf_dnode_findbp(dn, zep->zb_level, zep->zb_blkid, &bp, NULL,
	    NULL);
	if (error == 0 && BP_IS_HOLE(&bp))
		error = SET_ERROR(ENOENT);

	*birth_txg = bp.blk_birth;
	rw_exit(&dn->dn_struct_rwlock);
	dnode_rele(dn, FTAG);
	return (error);
}

 
int
find_top_affected_fs(spa_t *spa, uint64_t head_ds, zbookmark_err_phys_t *zep,
    uint64_t *top_affected_fs)
{
	uint64_t oldest_dsobj;
	int error = dsl_dataset_oldest_snapshot(spa, head_ds, zep->zb_birth,
	    &oldest_dsobj);
	if (error != 0)
		return (error);

	dsl_dataset_t *ds;
	error = dsl_dataset_hold_obj_flags(spa->spa_dsl_pool, oldest_dsobj,
	    DS_HOLD_FLAG_DECRYPT, FTAG, &ds);
	if (error != 0)
		return (error);

	*top_affected_fs =
	    dsl_dir_phys(ds->ds_dir)->dd_head_dataset_obj;
	dsl_dataset_rele_flags(ds, DS_HOLD_FLAG_DECRYPT, FTAG);
	return (0);
}


#ifdef _KERNEL
 
static int
copyout_entry(const zbookmark_phys_t *zb, void *uaddr, uint64_t *count)
{
	if (*count == 0)
		return (SET_ERROR(ENOMEM));

	*count -= 1;
	if (copyout(zb, (char *)uaddr + (*count) * sizeof (zbookmark_phys_t),
	    sizeof (zbookmark_phys_t)) != 0)
		return (SET_ERROR(EFAULT));
	return (0);
}

 
static int
check_filesystem(spa_t *spa, uint64_t head_ds, zbookmark_err_phys_t *zep,
    void *uaddr, uint64_t *count, list_t *clones_list)
{
	dsl_dataset_t *ds;
	dsl_pool_t *dp = spa->spa_dsl_pool;

	int error = dsl_dataset_hold_obj_flags(dp, head_ds,
	    DS_HOLD_FLAG_DECRYPT, FTAG, &ds);
	if (error != 0)
		return (error);

	uint64_t latest_txg;
	uint64_t txg_to_consider = spa->spa_syncing_txg;
	boolean_t check_snapshot = B_TRUE;
	error = find_birth_txg(ds, zep, &latest_txg);

	 
	if (error == 0 && zep->zb_birth == latest_txg) {
		 
		zbookmark_phys_t zb;
		zep_to_zb(head_ds, zep, &zb);
		error = copyout_entry(&zb, uaddr, count);
		if (error != 0) {
			dsl_dataset_rele_flags(ds, DS_HOLD_FLAG_DECRYPT, FTAG);
			return (error);
		}
		check_snapshot = B_FALSE;
	} else if (error == 0) {
		txg_to_consider = latest_txg;
	}

	 
	uint64_t snap_count = 0;
	if (dsl_dataset_phys(ds)->ds_snapnames_zapobj != 0) {

		error = zap_count(spa->spa_meta_objset,
		    dsl_dataset_phys(ds)->ds_snapnames_zapobj, &snap_count);

		if (error != 0) {
			dsl_dataset_rele_flags(ds, DS_HOLD_FLAG_DECRYPT, FTAG);
			return (error);
		}
	}

	if (snap_count == 0) {
		 
		dsl_dataset_rele_flags(ds, DS_HOLD_FLAG_DECRYPT, FTAG);
		return (0);
	}

	uint64_t *snap_obj_array = kmem_zalloc(snap_count * sizeof (uint64_t),
	    KM_SLEEP);

	int aff_snap_count = 0;
	uint64_t snap_obj = dsl_dataset_phys(ds)->ds_prev_snap_obj;
	uint64_t snap_obj_txg = dsl_dataset_phys(ds)->ds_prev_snap_txg;
	uint64_t zap_clone = dsl_dir_phys(ds->ds_dir)->dd_clones;

	dsl_dataset_rele_flags(ds, DS_HOLD_FLAG_DECRYPT, FTAG);

	 
	while (snap_obj != 0 && zep->zb_birth < snap_obj_txg &&
	    snap_obj_txg <= txg_to_consider) {

		error = dsl_dataset_hold_obj_flags(dp, snap_obj,
		    DS_HOLD_FLAG_DECRYPT, FTAG, &ds);
		if (error != 0)
			goto out;

		if (dsl_dir_phys(ds->ds_dir)->dd_head_dataset_obj != head_ds) {
			snap_obj = dsl_dataset_phys(ds)->ds_prev_snap_obj;
			snap_obj_txg = dsl_dataset_phys(ds)->ds_prev_snap_txg;
			dsl_dataset_rele_flags(ds, DS_HOLD_FLAG_DECRYPT, FTAG);
			continue;
		}

		boolean_t affected = B_TRUE;
		if (check_snapshot) {
			uint64_t blk_txg;
			error = find_birth_txg(ds, zep, &blk_txg);
			affected = (error == 0 && zep->zb_birth == blk_txg);
		}

		 
		if (affected) {
			snap_obj_array[aff_snap_count] = snap_obj;
			aff_snap_count++;

			zbookmark_phys_t zb;
			zep_to_zb(snap_obj, zep, &zb);
			error = copyout_entry(&zb, uaddr, count);
			if (error != 0) {
				dsl_dataset_rele_flags(ds, DS_HOLD_FLAG_DECRYPT,
				    FTAG);
				goto out;
			}
		}
		snap_obj = dsl_dataset_phys(ds)->ds_prev_snap_obj;
		snap_obj_txg = dsl_dataset_phys(ds)->ds_prev_snap_txg;
		dsl_dataset_rele_flags(ds, DS_HOLD_FLAG_DECRYPT, FTAG);
	}

	if (zap_clone == 0 || aff_snap_count == 0)
		return (0);

	 
	zap_cursor_t *zc;
	zap_attribute_t *za;

	zc = kmem_zalloc(sizeof (zap_cursor_t), KM_SLEEP);
	za = kmem_zalloc(sizeof (zap_attribute_t), KM_SLEEP);

	for (zap_cursor_init(zc, spa->spa_meta_objset, zap_clone);
	    zap_cursor_retrieve(zc, za) == 0;
	    zap_cursor_advance(zc)) {

		dsl_dataset_t *clone;
		error = dsl_dataset_hold_obj_flags(dp, za->za_first_integer,
		    DS_HOLD_FLAG_DECRYPT, FTAG, &clone);

		if (error != 0)
			break;

		 
		boolean_t found = B_FALSE;
		for (int i = 0; i < snap_count; i++) {
			if (dsl_dir_phys(clone->ds_dir)->dd_origin_obj
			    == snap_obj_array[i])
				found = B_TRUE;
		}
		dsl_dataset_rele_flags(clone, DS_HOLD_FLAG_DECRYPT, FTAG);

		if (!found)
			continue;

		clones_t *ct = kmem_zalloc(sizeof (*ct), KM_SLEEP);
		ct->clone_ds = za->za_first_integer;
		list_insert_tail(clones_list, ct);
	}

	zap_cursor_fini(zc);
	kmem_free(za, sizeof (*za));
	kmem_free(zc, sizeof (*zc));

out:
	kmem_free(snap_obj_array, sizeof (*snap_obj_array));
	return (error);
}

static int
process_error_block(spa_t *spa, uint64_t head_ds, zbookmark_err_phys_t *zep,
    void *uaddr, uint64_t *count)
{
	 
	if (zep->zb_birth == 0 || head_ds == 0) {
		zbookmark_phys_t zb;
		zep_to_zb(head_ds, zep, &zb);
		int error = copyout_entry(&zb, uaddr, count);
		if (error != 0) {
			return (error);
		}
		return (0);
	}

	uint64_t top_affected_fs;
	uint64_t init_count = *count;
	int error = find_top_affected_fs(spa, head_ds, zep, &top_affected_fs);
	if (error == 0) {
		clones_t *ct;
		list_t clones_list;

		list_create(&clones_list, sizeof (clones_t),
		    offsetof(clones_t, node));

		error = check_filesystem(spa, top_affected_fs, zep,
		    uaddr, count, &clones_list);

		while ((ct = list_remove_head(&clones_list)) != NULL) {
			error = check_filesystem(spa, ct->clone_ds, zep,
			    uaddr, count, &clones_list);
			kmem_free(ct, sizeof (*ct));

			if (error) {
				while (!list_is_empty(&clones_list)) {
					ct = list_remove_head(&clones_list);
					kmem_free(ct, sizeof (*ct));
				}
				break;
			}
		}

		list_destroy(&clones_list);
	}
	if (error == 0 && init_count == *count) {
		 
		zbookmark_phys_t zb;
		zep_to_zb(head_ds, zep, &zb);
		spa_remove_error(spa, &zb, &zep->zb_birth);
	}

	return (error);
}
#endif

 
uint64_t
spa_get_last_errlog_size(spa_t *spa)
{
	uint64_t total = 0, count;
	mutex_enter(&spa->spa_errlog_lock);

	if (spa->spa_errlog_last != 0 &&
	    zap_count(spa->spa_meta_objset, spa->spa_errlog_last,
	    &count) == 0)
		total += count;
	mutex_exit(&spa->spa_errlog_lock);
	return (total);
}

 
static void
spa_add_healed_error(spa_t *spa, uint64_t obj, zbookmark_phys_t *healed_zb,
    const uint64_t *birth)
{
	char name[NAME_MAX_LEN];

	if (obj == 0)
		return;

	boolean_t held_list = B_FALSE;
	boolean_t held_log = B_FALSE;

	if (!spa_feature_is_enabled(spa, SPA_FEATURE_HEAD_ERRLOG)) {
		bookmark_to_name(healed_zb, name, sizeof (name));

		if (zap_contains(spa->spa_meta_objset, healed_zb->zb_objset,
		    name) == 0) {
			if (!MUTEX_HELD(&spa->spa_errlog_lock)) {
				mutex_enter(&spa->spa_errlog_lock);
				held_log = B_TRUE;
			}

			 
			avl_tree_t *tree = &spa->spa_errlist_healed;
			spa_error_entry_t search;
			spa_error_entry_t *new;
			avl_index_t where;
			search.se_bookmark = *healed_zb;
			if (!MUTEX_HELD(&spa->spa_errlist_lock)) {
				mutex_enter(&spa->spa_errlist_lock);
				held_list = B_TRUE;
			}
			if (avl_find(tree, &search, &where) != NULL) {
				if (held_list)
					mutex_exit(&spa->spa_errlist_lock);
				if (held_log)
					mutex_exit(&spa->spa_errlog_lock);
				return;
			}
			new = kmem_zalloc(sizeof (spa_error_entry_t), KM_SLEEP);
			new->se_bookmark = *healed_zb;
			avl_insert(tree, new, where);
			if (held_list)
				mutex_exit(&spa->spa_errlist_lock);
			if (held_log)
				mutex_exit(&spa->spa_errlog_lock);
		}
		return;
	}

	zbookmark_err_phys_t healed_zep;
	healed_zep.zb_object = healed_zb->zb_object;
	healed_zep.zb_level = healed_zb->zb_level;
	healed_zep.zb_blkid = healed_zb->zb_blkid;

	if (birth != NULL)
		healed_zep.zb_birth = *birth;
	else
		healed_zep.zb_birth = 0;

	errphys_to_name(&healed_zep, name, sizeof (name));

	zap_cursor_t zc;
	zap_attribute_t za;
	for (zap_cursor_init(&zc, spa->spa_meta_objset, spa->spa_errlog_last);
	    zap_cursor_retrieve(&zc, &za) == 0; zap_cursor_advance(&zc)) {
		if (zap_contains(spa->spa_meta_objset, za.za_first_integer,
		    name) == 0) {
			if (!MUTEX_HELD(&spa->spa_errlog_lock)) {
				mutex_enter(&spa->spa_errlog_lock);
				held_log = B_TRUE;
			}

			avl_tree_t *tree = &spa->spa_errlist_healed;
			spa_error_entry_t search;
			spa_error_entry_t *new;
			avl_index_t where;
			search.se_bookmark = *healed_zb;

			if (!MUTEX_HELD(&spa->spa_errlist_lock)) {
				mutex_enter(&spa->spa_errlist_lock);
				held_list = B_TRUE;
			}

			if (avl_find(tree, &search, &where) != NULL) {
				if (held_list)
					mutex_exit(&spa->spa_errlist_lock);
				if (held_log)
					mutex_exit(&spa->spa_errlog_lock);
				continue;
			}
			new = kmem_zalloc(sizeof (spa_error_entry_t), KM_SLEEP);
			new->se_bookmark = *healed_zb;
			new->se_zep = healed_zep;
			avl_insert(tree, new, where);

			if (held_list)
				mutex_exit(&spa->spa_errlist_lock);
			if (held_log)
				mutex_exit(&spa->spa_errlog_lock);
		}
	}
	zap_cursor_fini(&zc);
}

 
static void
remove_error_from_list(spa_t *spa, avl_tree_t *t, const zbookmark_phys_t *zb)
{
	spa_error_entry_t search, *found;
	avl_index_t where;

	mutex_enter(&spa->spa_errlist_lock);
	search.se_bookmark = *zb;
	if ((found = avl_find(t, &search, &where)) != NULL) {
		avl_remove(t, found);
		kmem_free(found, sizeof (spa_error_entry_t));
	}
	mutex_exit(&spa->spa_errlist_lock);
}


 
static void
spa_remove_healed_errors(spa_t *spa, avl_tree_t *s, avl_tree_t *l, dmu_tx_t *tx)
{
	char name[NAME_MAX_LEN];
	spa_error_entry_t *se;
	void *cookie = NULL;

	ASSERT(MUTEX_HELD(&spa->spa_errlog_lock));

	while ((se = avl_destroy_nodes(&spa->spa_errlist_healed,
	    &cookie)) != NULL) {
		remove_error_from_list(spa, s, &se->se_bookmark);
		remove_error_from_list(spa, l, &se->se_bookmark);

		if (!spa_feature_is_enabled(spa, SPA_FEATURE_HEAD_ERRLOG)) {
			bookmark_to_name(&se->se_bookmark, name, sizeof (name));
			(void) zap_remove(spa->spa_meta_objset,
			    spa->spa_errlog_last, name, tx);
			(void) zap_remove(spa->spa_meta_objset,
			    spa->spa_errlog_scrub, name, tx);
		} else {
			errphys_to_name(&se->se_zep, name, sizeof (name));
			zap_cursor_t zc;
			zap_attribute_t za;
			for (zap_cursor_init(&zc, spa->spa_meta_objset,
			    spa->spa_errlog_last);
			    zap_cursor_retrieve(&zc, &za) == 0;
			    zap_cursor_advance(&zc)) {
				zap_remove(spa->spa_meta_objset,
				    za.za_first_integer, name, tx);
			}
			zap_cursor_fini(&zc);

			for (zap_cursor_init(&zc, spa->spa_meta_objset,
			    spa->spa_errlog_scrub);
			    zap_cursor_retrieve(&zc, &za) == 0;
			    zap_cursor_advance(&zc)) {
				zap_remove(spa->spa_meta_objset,
				    za.za_first_integer, name, tx);
			}
			zap_cursor_fini(&zc);
		}
		kmem_free(se, sizeof (spa_error_entry_t));
	}
}

 
void
spa_remove_error(spa_t *spa, zbookmark_phys_t *zb, const uint64_t *birth)
{
	spa_add_healed_error(spa, spa->spa_errlog_last, zb, birth);
	spa_add_healed_error(spa, spa->spa_errlog_scrub, zb, birth);
}

static uint64_t
approx_errlog_size_impl(spa_t *spa, uint64_t spa_err_obj)
{
	if (spa_err_obj == 0)
		return (0);
	uint64_t total = 0;

	zap_cursor_t zc;
	zap_attribute_t za;
	for (zap_cursor_init(&zc, spa->spa_meta_objset, spa_err_obj);
	    zap_cursor_retrieve(&zc, &za) == 0; zap_cursor_advance(&zc)) {
		uint64_t count;
		if (zap_count(spa->spa_meta_objset, za.za_first_integer,
		    &count) == 0)
			total += count;
	}
	zap_cursor_fini(&zc);
	return (total);
}

 
uint64_t
spa_approx_errlog_size(spa_t *spa)
{
	uint64_t total = 0;

	if (!spa_feature_is_enabled(spa, SPA_FEATURE_HEAD_ERRLOG)) {
		mutex_enter(&spa->spa_errlog_lock);
		uint64_t count;
		if (spa->spa_errlog_scrub != 0 &&
		    zap_count(spa->spa_meta_objset, spa->spa_errlog_scrub,
		    &count) == 0)
			total += count;

		if (spa->spa_errlog_last != 0 && !spa->spa_scrub_finished &&
		    zap_count(spa->spa_meta_objset, spa->spa_errlog_last,
		    &count) == 0)
			total += count;
		mutex_exit(&spa->spa_errlog_lock);

	} else {
		mutex_enter(&spa->spa_errlog_lock);
		total += approx_errlog_size_impl(spa, spa->spa_errlog_last);
		total += approx_errlog_size_impl(spa, spa->spa_errlog_scrub);
		mutex_exit(&spa->spa_errlog_lock);
	}
	mutex_enter(&spa->spa_errlist_lock);
	total += avl_numnodes(&spa->spa_errlist_last);
	total += avl_numnodes(&spa->spa_errlist_scrub);
	mutex_exit(&spa->spa_errlist_lock);
	return (total);
}

 
static void
sync_upgrade_errlog(spa_t *spa, uint64_t spa_err_obj, uint64_t *newobj,
    dmu_tx_t *tx)
{
	zap_cursor_t zc;
	zap_attribute_t za;
	zbookmark_phys_t zb;
	uint64_t count;

	*newobj = zap_create(spa->spa_meta_objset, DMU_OT_ERROR_LOG,
	    DMU_OT_NONE, 0, tx);

	 
	if (zap_count(spa->spa_meta_objset, spa_err_obj, &count) != 0) {
		VERIFY0(dmu_object_free(spa->spa_meta_objset, spa_err_obj, tx));
		return;
	}

	for (zap_cursor_init(&zc, spa->spa_meta_objset, spa_err_obj);
	    zap_cursor_retrieve(&zc, &za) == 0;
	    zap_cursor_advance(&zc)) {
		if (spa_upgrade_errlog_limit != 0 &&
		    zc.zc_cd == spa_upgrade_errlog_limit)
			break;

		name_to_bookmark(za.za_name, &zb);

		zbookmark_err_phys_t zep;
		zep.zb_object = zb.zb_object;
		zep.zb_level = zb.zb_level;
		zep.zb_blkid = zb.zb_blkid;
		zep.zb_birth = 0;

		 
		uint64_t head_ds;
		dsl_pool_t *dp = spa->spa_dsl_pool;
		dsl_dataset_t *ds;
		objset_t *os;

		int error = dsl_dataset_hold_obj_flags(dp, zb.zb_objset,
		    DS_HOLD_FLAG_DECRYPT, FTAG, &ds);
		if (error != 0)
			continue;

		head_ds = dsl_dir_phys(ds->ds_dir)->dd_head_dataset_obj;

		 
		if (dmu_objset_from_ds(ds, &os) != 0) {
			dsl_dataset_rele_flags(ds, DS_HOLD_FLAG_DECRYPT, FTAG);
			continue;
		}

		dnode_t *dn;
		blkptr_t bp;

		if (dnode_hold(os, zep.zb_object, FTAG, &dn) != 0) {
			dsl_dataset_rele_flags(ds, DS_HOLD_FLAG_DECRYPT, FTAG);
			continue;
		}

		rw_enter(&dn->dn_struct_rwlock, RW_READER);
		error = dbuf_dnode_findbp(dn, zep.zb_level, zep.zb_blkid, &bp,
		    NULL, NULL);
		if (error == EACCES)
			error = 0;
		else if (!error)
			zep.zb_birth = bp.blk_birth;

		rw_exit(&dn->dn_struct_rwlock);
		dnode_rele(dn, FTAG);
		dsl_dataset_rele_flags(ds, DS_HOLD_FLAG_DECRYPT, FTAG);

		if (error != 0 || BP_IS_HOLE(&bp))
			continue;

		uint64_t err_obj;
		error = zap_lookup_int_key(spa->spa_meta_objset, *newobj,
		    head_ds, &err_obj);

		if (error == ENOENT) {
			err_obj = zap_create(spa->spa_meta_objset,
			    DMU_OT_ERROR_LOG, DMU_OT_NONE, 0, tx);

			(void) zap_update_int_key(spa->spa_meta_objset,
			    *newobj, head_ds, err_obj, tx);
		}

		char buf[64];
		errphys_to_name(&zep, buf, sizeof (buf));

		const char *name = "";
		(void) zap_update(spa->spa_meta_objset, err_obj,
		    buf, 1, strlen(name) + 1, name, tx);
	}
	zap_cursor_fini(&zc);

	VERIFY0(dmu_object_free(spa->spa_meta_objset, spa_err_obj, tx));
}

void
spa_upgrade_errlog(spa_t *spa, dmu_tx_t *tx)
{
	uint64_t newobj = 0;

	mutex_enter(&spa->spa_errlog_lock);
	if (spa->spa_errlog_last != 0) {
		sync_upgrade_errlog(spa, spa->spa_errlog_last, &newobj, tx);
		spa->spa_errlog_last = newobj;

		(void) zap_update(spa->spa_meta_objset,
		    DMU_POOL_DIRECTORY_OBJECT, DMU_POOL_ERRLOG_LAST,
		    sizeof (uint64_t), 1, &spa->spa_errlog_last, tx);
	}

	if (spa->spa_errlog_scrub != 0) {
		sync_upgrade_errlog(spa, spa->spa_errlog_scrub, &newobj, tx);
		spa->spa_errlog_scrub = newobj;

		(void) zap_update(spa->spa_meta_objset,
		    DMU_POOL_DIRECTORY_OBJECT, DMU_POOL_ERRLOG_SCRUB,
		    sizeof (uint64_t), 1, &spa->spa_errlog_scrub, tx);
	}

	mutex_exit(&spa->spa_errlog_lock);
}

#ifdef _KERNEL
 
static int
process_error_log(spa_t *spa, uint64_t obj, void *uaddr, uint64_t *count)
{
	if (obj == 0)
		return (0);

	zap_cursor_t *zc;
	zap_attribute_t *za;

	zc = kmem_zalloc(sizeof (zap_cursor_t), KM_SLEEP);
	za = kmem_zalloc(sizeof (zap_attribute_t), KM_SLEEP);

	if (!spa_feature_is_enabled(spa, SPA_FEATURE_HEAD_ERRLOG)) {
		for (zap_cursor_init(zc, spa->spa_meta_objset, obj);
		    zap_cursor_retrieve(zc, za) == 0;
		    zap_cursor_advance(zc)) {
			if (*count == 0) {
				zap_cursor_fini(zc);
				kmem_free(zc, sizeof (*zc));
				kmem_free(za, sizeof (*za));
				return (SET_ERROR(ENOMEM));
			}

			zbookmark_phys_t zb;
			name_to_bookmark(za->za_name, &zb);

			int error = copyout_entry(&zb, uaddr, count);
			if (error != 0) {
				zap_cursor_fini(zc);
				kmem_free(zc, sizeof (*zc));
				kmem_free(za, sizeof (*za));
				return (error);
			}
		}
		zap_cursor_fini(zc);
		kmem_free(zc, sizeof (*zc));
		kmem_free(za, sizeof (*za));
		return (0);
	}

	for (zap_cursor_init(zc, spa->spa_meta_objset, obj);
	    zap_cursor_retrieve(zc, za) == 0;
	    zap_cursor_advance(zc)) {

		zap_cursor_t *head_ds_cursor;
		zap_attribute_t *head_ds_attr;

		head_ds_cursor = kmem_zalloc(sizeof (zap_cursor_t), KM_SLEEP);
		head_ds_attr = kmem_zalloc(sizeof (zap_attribute_t), KM_SLEEP);

		uint64_t head_ds_err_obj = za->za_first_integer;
		uint64_t head_ds;
		name_to_object(za->za_name, &head_ds);
		for (zap_cursor_init(head_ds_cursor, spa->spa_meta_objset,
		    head_ds_err_obj); zap_cursor_retrieve(head_ds_cursor,
		    head_ds_attr) == 0; zap_cursor_advance(head_ds_cursor)) {

			zbookmark_err_phys_t head_ds_block;
			name_to_errphys(head_ds_attr->za_name, &head_ds_block);
			int error = process_error_block(spa, head_ds,
			    &head_ds_block, uaddr, count);

			if (error != 0) {
				zap_cursor_fini(head_ds_cursor);
				kmem_free(head_ds_cursor,
				    sizeof (*head_ds_cursor));
				kmem_free(head_ds_attr, sizeof (*head_ds_attr));

				zap_cursor_fini(zc);
				kmem_free(za, sizeof (*za));
				kmem_free(zc, sizeof (*zc));
				return (error);
			}
		}
		zap_cursor_fini(head_ds_cursor);
		kmem_free(head_ds_cursor, sizeof (*head_ds_cursor));
		kmem_free(head_ds_attr, sizeof (*head_ds_attr));
	}
	zap_cursor_fini(zc);
	kmem_free(za, sizeof (*za));
	kmem_free(zc, sizeof (*zc));
	return (0);
}

static int
process_error_list(spa_t *spa, avl_tree_t *list, void *uaddr, uint64_t *count)
{
	spa_error_entry_t *se;

	if (!spa_feature_is_enabled(spa, SPA_FEATURE_HEAD_ERRLOG)) {
		for (se = avl_first(list); se != NULL;
		    se = AVL_NEXT(list, se)) {
			int error =
			    copyout_entry(&se->se_bookmark, uaddr, count);
			if (error != 0) {
				return (error);
			}
		}
		return (0);
	}

	for (se = avl_first(list); se != NULL; se = AVL_NEXT(list, se)) {
		uint64_t head_ds = 0;
		int error = get_head_ds(spa, se->se_bookmark.zb_objset,
		    &head_ds);

		 
		if (error != 0)
			head_ds = se->se_bookmark.zb_objset;

		error = process_error_block(spa, head_ds,
		    &se->se_zep, uaddr, count);
		if (error != 0)
			return (error);
	}
	return (0);
}
#endif

 
int
spa_get_errlog(spa_t *spa, void *uaddr, uint64_t *count)
{
	int ret = 0;

#ifdef _KERNEL
	 
	dsl_pool_config_enter(spa->spa_dsl_pool, FTAG);
	mutex_enter(&spa->spa_errlog_lock);

	ret = process_error_log(spa, spa->spa_errlog_scrub, uaddr, count);

	if (!ret && !spa->spa_scrub_finished)
		ret = process_error_log(spa, spa->spa_errlog_last, uaddr,
		    count);

	mutex_enter(&spa->spa_errlist_lock);
	if (!ret)
		ret = process_error_list(spa, &spa->spa_errlist_scrub, uaddr,
		    count);
	if (!ret)
		ret = process_error_list(spa, &spa->spa_errlist_last, uaddr,
		    count);
	mutex_exit(&spa->spa_errlist_lock);

	mutex_exit(&spa->spa_errlog_lock);
	dsl_pool_config_exit(spa->spa_dsl_pool, FTAG);
#else
	(void) spa, (void) uaddr, (void) count;
#endif

	return (ret);
}

 
void
spa_errlog_rotate(spa_t *spa)
{
	mutex_enter(&spa->spa_errlist_lock);
	spa->spa_scrub_finished = B_TRUE;
	mutex_exit(&spa->spa_errlist_lock);
}

 
void
spa_errlog_drain(spa_t *spa)
{
	spa_error_entry_t *se;
	void *cookie;

	mutex_enter(&spa->spa_errlist_lock);

	cookie = NULL;
	while ((se = avl_destroy_nodes(&spa->spa_errlist_last,
	    &cookie)) != NULL)
		kmem_free(se, sizeof (spa_error_entry_t));
	cookie = NULL;
	while ((se = avl_destroy_nodes(&spa->spa_errlist_scrub,
	    &cookie)) != NULL)
		kmem_free(se, sizeof (spa_error_entry_t));

	mutex_exit(&spa->spa_errlist_lock);
}

 
void
sync_error_list(spa_t *spa, avl_tree_t *t, uint64_t *obj, dmu_tx_t *tx)
{
	spa_error_entry_t *se;
	char buf[NAME_MAX_LEN];
	void *cookie;

	if (avl_numnodes(t) == 0)
		return;

	 
	if (*obj == 0)
		*obj = zap_create(spa->spa_meta_objset, DMU_OT_ERROR_LOG,
		    DMU_OT_NONE, 0, tx);

	 
	if (!spa_feature_is_enabled(spa, SPA_FEATURE_HEAD_ERRLOG)) {
		for (se = avl_first(t); se != NULL; se = AVL_NEXT(t, se)) {
			bookmark_to_name(&se->se_bookmark, buf, sizeof (buf));

			const char *name = se->se_name ? se->se_name : "";
			(void) zap_update(spa->spa_meta_objset, *obj, buf, 1,
			    strlen(name) + 1, name, tx);
		}
	} else {
		for (se = avl_first(t); se != NULL; se = AVL_NEXT(t, se)) {
			zbookmark_err_phys_t zep;
			zep.zb_object = se->se_zep.zb_object;
			zep.zb_level = se->se_zep.zb_level;
			zep.zb_blkid = se->se_zep.zb_blkid;
			zep.zb_birth = se->se_zep.zb_birth;

			uint64_t head_ds = 0;
			int error = get_head_ds(spa, se->se_bookmark.zb_objset,
			    &head_ds);

			 
			if (error != 0)
				head_ds = se->se_bookmark.zb_objset;

			uint64_t err_obj;
			error = zap_lookup_int_key(spa->spa_meta_objset,
			    *obj, head_ds, &err_obj);

			if (error == ENOENT) {
				err_obj = zap_create(spa->spa_meta_objset,
				    DMU_OT_ERROR_LOG, DMU_OT_NONE, 0, tx);

				(void) zap_update_int_key(spa->spa_meta_objset,
				    *obj, head_ds, err_obj, tx);
			}
			errphys_to_name(&zep, buf, sizeof (buf));

			const char *name = se->se_name ? se->se_name : "";
			(void) zap_update(spa->spa_meta_objset,
			    err_obj, buf, 1, strlen(name) + 1, name, tx);
		}
	}
	 
	cookie = NULL;
	while ((se = avl_destroy_nodes(t, &cookie)) != NULL)
		kmem_free(se, sizeof (spa_error_entry_t));
}

static void
delete_errlog(spa_t *spa, uint64_t spa_err_obj, dmu_tx_t *tx)
{
	if (spa_feature_is_enabled(spa, SPA_FEATURE_HEAD_ERRLOG)) {
		zap_cursor_t zc;
		zap_attribute_t za;
		for (zap_cursor_init(&zc, spa->spa_meta_objset, spa_err_obj);
		    zap_cursor_retrieve(&zc, &za) == 0;
		    zap_cursor_advance(&zc)) {
			VERIFY0(dmu_object_free(spa->spa_meta_objset,
			    za.za_first_integer, tx));
		}
		zap_cursor_fini(&zc);
	}
	VERIFY0(dmu_object_free(spa->spa_meta_objset, spa_err_obj, tx));
}

 
void
spa_errlog_sync(spa_t *spa, uint64_t txg)
{
	dmu_tx_t *tx;
	avl_tree_t scrub, last;
	int scrub_finished;

	mutex_enter(&spa->spa_errlist_lock);

	 
	if (avl_numnodes(&spa->spa_errlist_scrub) == 0 &&
	    avl_numnodes(&spa->spa_errlist_last) == 0 &&
	    avl_numnodes(&spa->spa_errlist_healed) == 0 &&
	    !spa->spa_scrub_finished) {
		mutex_exit(&spa->spa_errlist_lock);
		return;
	}

	spa_get_errlists(spa, &last, &scrub);
	scrub_finished = spa->spa_scrub_finished;
	spa->spa_scrub_finished = B_FALSE;

	mutex_exit(&spa->spa_errlist_lock);

	 
	dsl_pool_config_enter(spa->spa_dsl_pool, FTAG);
	mutex_enter(&spa->spa_errlog_lock);

	tx = dmu_tx_create_assigned(spa->spa_dsl_pool, txg);

	 
	spa_remove_healed_errors(spa, &last, &scrub, tx);

	 
	sync_error_list(spa, &last, &spa->spa_errlog_last, tx);

	 
	if (scrub_finished) {
		if (spa->spa_errlog_last != 0)
			delete_errlog(spa, spa->spa_errlog_last, tx);
		spa->spa_errlog_last = spa->spa_errlog_scrub;
		spa->spa_errlog_scrub = 0;

		sync_error_list(spa, &scrub, &spa->spa_errlog_last, tx);
	}

	 
	sync_error_list(spa, &scrub, &spa->spa_errlog_scrub, tx);

	 
	(void) zap_update(spa->spa_meta_objset, DMU_POOL_DIRECTORY_OBJECT,
	    DMU_POOL_ERRLOG_LAST, sizeof (uint64_t), 1,
	    &spa->spa_errlog_last, tx);
	(void) zap_update(spa->spa_meta_objset, DMU_POOL_DIRECTORY_OBJECT,
	    DMU_POOL_ERRLOG_SCRUB, sizeof (uint64_t), 1,
	    &spa->spa_errlog_scrub, tx);

	dmu_tx_commit(tx);

	mutex_exit(&spa->spa_errlog_lock);
	dsl_pool_config_exit(spa->spa_dsl_pool, FTAG);
}

static void
delete_dataset_errlog(spa_t *spa, uint64_t spa_err_obj, uint64_t ds,
    dmu_tx_t *tx)
{
	if (spa_err_obj == 0)
		return;

	zap_cursor_t zc;
	zap_attribute_t za;
	for (zap_cursor_init(&zc, spa->spa_meta_objset, spa_err_obj);
	    zap_cursor_retrieve(&zc, &za) == 0; zap_cursor_advance(&zc)) {
		uint64_t head_ds;
		name_to_object(za.za_name, &head_ds);
		if (head_ds == ds) {
			(void) zap_remove(spa->spa_meta_objset, spa_err_obj,
			    za.za_name, tx);
			VERIFY0(dmu_object_free(spa->spa_meta_objset,
			    za.za_first_integer, tx));
			break;
		}
	}
	zap_cursor_fini(&zc);
}

void
spa_delete_dataset_errlog(spa_t *spa, uint64_t ds, dmu_tx_t *tx)
{
	mutex_enter(&spa->spa_errlog_lock);
	delete_dataset_errlog(spa, spa->spa_errlog_scrub, ds, tx);
	delete_dataset_errlog(spa, spa->spa_errlog_last, ds, tx);
	mutex_exit(&spa->spa_errlog_lock);
}

static int
find_txg_ancestor_snapshot(spa_t *spa, uint64_t new_head, uint64_t old_head,
    uint64_t *txg)
{
	dsl_dataset_t *ds;
	dsl_pool_t *dp = spa->spa_dsl_pool;

	int error = dsl_dataset_hold_obj_flags(dp, old_head,
	    DS_HOLD_FLAG_DECRYPT, FTAG, &ds);
	if (error != 0)
		return (error);

	uint64_t prev_obj = dsl_dataset_phys(ds)->ds_prev_snap_obj;
	uint64_t prev_obj_txg = dsl_dataset_phys(ds)->ds_prev_snap_txg;

	while (prev_obj != 0) {
		dsl_dataset_rele_flags(ds, DS_HOLD_FLAG_DECRYPT, FTAG);
		if ((error = dsl_dataset_hold_obj_flags(dp, prev_obj,
		    DS_HOLD_FLAG_DECRYPT, FTAG, &ds)) == 0 &&
		    dsl_dir_phys(ds->ds_dir)->dd_head_dataset_obj == new_head)
			break;

		if (error != 0)
			return (error);

		prev_obj_txg = dsl_dataset_phys(ds)->ds_prev_snap_txg;
		prev_obj = dsl_dataset_phys(ds)->ds_prev_snap_obj;
	}
	dsl_dataset_rele_flags(ds, DS_HOLD_FLAG_DECRYPT, FTAG);
	ASSERT(prev_obj != 0);
	*txg = prev_obj_txg;
	return (0);
}

static void
swap_errlog(spa_t *spa, uint64_t spa_err_obj, uint64_t new_head, uint64_t
    old_head, dmu_tx_t *tx)
{
	if (spa_err_obj == 0)
		return;

	uint64_t old_head_errlog;
	int error = zap_lookup_int_key(spa->spa_meta_objset, spa_err_obj,
	    old_head, &old_head_errlog);

	 
	if (error != 0)
		return;

	uint64_t txg;
	error = find_txg_ancestor_snapshot(spa, new_head, old_head, &txg);
	if (error != 0)
		return;

	 
	uint64_t new_head_errlog;
	error = zap_lookup_int_key(spa->spa_meta_objset, spa_err_obj, new_head,
	    &new_head_errlog);

	if (error != 0) {
		new_head_errlog = zap_create(spa->spa_meta_objset,
		    DMU_OT_ERROR_LOG, DMU_OT_NONE, 0, tx);

		(void) zap_update_int_key(spa->spa_meta_objset, spa_err_obj,
		    new_head, new_head_errlog, tx);
	}

	zap_cursor_t zc;
	zap_attribute_t za;
	zbookmark_err_phys_t err_block;
	for (zap_cursor_init(&zc, spa->spa_meta_objset, old_head_errlog);
	    zap_cursor_retrieve(&zc, &za) == 0; zap_cursor_advance(&zc)) {

		const char *name = "";
		name_to_errphys(za.za_name, &err_block);
		if (err_block.zb_birth < txg) {
			(void) zap_update(spa->spa_meta_objset, new_head_errlog,
			    za.za_name, 1, strlen(name) + 1, name, tx);

			(void) zap_remove(spa->spa_meta_objset, old_head_errlog,
			    za.za_name, tx);
		}
	}
	zap_cursor_fini(&zc);
}

void
spa_swap_errlog(spa_t *spa, uint64_t new_head_ds, uint64_t old_head_ds,
    dmu_tx_t *tx)
{
	mutex_enter(&spa->spa_errlog_lock);
	swap_errlog(spa, spa->spa_errlog_scrub, new_head_ds, old_head_ds, tx);
	swap_errlog(spa, spa->spa_errlog_last, new_head_ds, old_head_ds, tx);
	mutex_exit(&spa->spa_errlog_lock);
}

#if defined(_KERNEL)
 
EXPORT_SYMBOL(spa_log_error);
EXPORT_SYMBOL(spa_approx_errlog_size);
EXPORT_SYMBOL(spa_get_last_errlog_size);
EXPORT_SYMBOL(spa_get_errlog);
EXPORT_SYMBOL(spa_errlog_rotate);
EXPORT_SYMBOL(spa_errlog_drain);
EXPORT_SYMBOL(spa_errlog_sync);
EXPORT_SYMBOL(spa_get_errlists);
EXPORT_SYMBOL(spa_delete_dataset_errlog);
EXPORT_SYMBOL(spa_swap_errlog);
EXPORT_SYMBOL(sync_error_list);
EXPORT_SYMBOL(spa_upgrade_errlog);
EXPORT_SYMBOL(find_top_affected_fs);
EXPORT_SYMBOL(find_birth_txg);
EXPORT_SYMBOL(zep_to_zb);
EXPORT_SYMBOL(name_to_errphys);
#endif

 
ZFS_MODULE_PARAM(zfs_spa, spa_, upgrade_errlog_limit, UINT, ZMOD_RW,
	"Limit the number of errors which will be upgraded to the new "
	"on-disk error log when enabling head_errlog");
 
