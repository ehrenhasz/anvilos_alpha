#include <sys/dmu.h>
#include <sys/zap.h>
#include <sys/zfs_context.h>
#include <sys/dsl_pool.h>
#include <sys/dsl_dataset.h>
uint64_t zfs_livelist_max_entries = 500000;
int zfs_livelist_min_percent_shared = 75;
static int
dsl_deadlist_compare(const void *arg1, const void *arg2)
{
	const dsl_deadlist_entry_t *dle1 = arg1;
	const dsl_deadlist_entry_t *dle2 = arg2;
	return (TREE_CMP(dle1->dle_mintxg, dle2->dle_mintxg));
}
static int
dsl_deadlist_cache_compare(const void *arg1, const void *arg2)
{
	const dsl_deadlist_cache_entry_t *dlce1 = arg1;
	const dsl_deadlist_cache_entry_t *dlce2 = arg2;
	return (TREE_CMP(dlce1->dlce_mintxg, dlce2->dlce_mintxg));
}
static void
dsl_deadlist_load_tree(dsl_deadlist_t *dl)
{
	zap_cursor_t zc;
	zap_attribute_t za;
	int error;
	ASSERT(MUTEX_HELD(&dl->dl_lock));
	ASSERT(!dl->dl_oldfmt);
	if (dl->dl_havecache) {
		dsl_deadlist_cache_entry_t *dlce;
		void *cookie = NULL;
		while ((dlce = avl_destroy_nodes(&dl->dl_cache, &cookie))
		    != NULL) {
			kmem_free(dlce, sizeof (*dlce));
		}
		avl_destroy(&dl->dl_cache);
		dl->dl_havecache = B_FALSE;
	}
	if (dl->dl_havetree)
		return;
	avl_create(&dl->dl_tree, dsl_deadlist_compare,
	    sizeof (dsl_deadlist_entry_t),
	    offsetof(dsl_deadlist_entry_t, dle_node));
	for (zap_cursor_init(&zc, dl->dl_os, dl->dl_object);
	    (error = zap_cursor_retrieve(&zc, &za)) == 0;
	    zap_cursor_advance(&zc)) {
		dsl_deadlist_entry_t *dle = kmem_alloc(sizeof (*dle), KM_SLEEP);
		dle->dle_mintxg = zfs_strtonum(za.za_name, NULL);
		dle->dle_bpobj.bpo_object = za.za_first_integer;
		dmu_prefetch(dl->dl_os, dle->dle_bpobj.bpo_object,
		    0, 0, 0, ZIO_PRIORITY_SYNC_READ);
		avl_add(&dl->dl_tree, dle);
	}
	VERIFY3U(error, ==, ENOENT);
	zap_cursor_fini(&zc);
	for (dsl_deadlist_entry_t *dle = avl_first(&dl->dl_tree);
	    dle != NULL; dle = AVL_NEXT(&dl->dl_tree, dle)) {
		VERIFY0(bpobj_open(&dle->dle_bpobj, dl->dl_os,
		    dle->dle_bpobj.bpo_object));
	}
	dl->dl_havetree = B_TRUE;
}
static void
dsl_deadlist_load_cache(dsl_deadlist_t *dl)
{
	zap_cursor_t zc;
	zap_attribute_t za;
	int error;
	ASSERT(MUTEX_HELD(&dl->dl_lock));
	ASSERT(!dl->dl_oldfmt);
	if (dl->dl_havecache)
		return;
	uint64_t empty_bpobj = dmu_objset_pool(dl->dl_os)->dp_empty_bpobj;
	avl_create(&dl->dl_cache, dsl_deadlist_cache_compare,
	    sizeof (dsl_deadlist_cache_entry_t),
	    offsetof(dsl_deadlist_cache_entry_t, dlce_node));
	for (zap_cursor_init(&zc, dl->dl_os, dl->dl_object);
	    (error = zap_cursor_retrieve(&zc, &za)) == 0;
	    zap_cursor_advance(&zc)) {
		if (za.za_first_integer == empty_bpobj)
			continue;
		dsl_deadlist_cache_entry_t *dlce =
		    kmem_zalloc(sizeof (*dlce), KM_SLEEP);
		dlce->dlce_mintxg = zfs_strtonum(za.za_name, NULL);
		dlce->dlce_bpobj = za.za_first_integer;
		dmu_prefetch(dl->dl_os, dlce->dlce_bpobj,
		    0, 0, 0, ZIO_PRIORITY_SYNC_READ);
		avl_add(&dl->dl_cache, dlce);
	}
	VERIFY3U(error, ==, ENOENT);
	zap_cursor_fini(&zc);
	for (dsl_deadlist_cache_entry_t *dlce = avl_first(&dl->dl_cache);
	    dlce != NULL; dlce = AVL_NEXT(&dl->dl_cache, dlce)) {
		bpobj_t bpo;
		VERIFY0(bpobj_open(&bpo, dl->dl_os, dlce->dlce_bpobj));
		VERIFY0(bpobj_space(&bpo,
		    &dlce->dlce_bytes, &dlce->dlce_comp, &dlce->dlce_uncomp));
		bpobj_close(&bpo);
	}
	dl->dl_havecache = B_TRUE;
}
void
dsl_deadlist_discard_tree(dsl_deadlist_t *dl)
{
	mutex_enter(&dl->dl_lock);
	if (!dl->dl_havetree) {
		mutex_exit(&dl->dl_lock);
		return;
	}
	dsl_deadlist_entry_t *dle;
	void *cookie = NULL;
	while ((dle = avl_destroy_nodes(&dl->dl_tree, &cookie)) != NULL) {
		bpobj_close(&dle->dle_bpobj);
		kmem_free(dle, sizeof (*dle));
	}
	avl_destroy(&dl->dl_tree);
	dl->dl_havetree = B_FALSE;
	mutex_exit(&dl->dl_lock);
}
void
dsl_deadlist_iterate(dsl_deadlist_t *dl, deadlist_iter_t func, void *args)
{
	dsl_deadlist_entry_t *dle;
	ASSERT(dsl_deadlist_is_open(dl));
	mutex_enter(&dl->dl_lock);
	dsl_deadlist_load_tree(dl);
	mutex_exit(&dl->dl_lock);
	for (dle = avl_first(&dl->dl_tree); dle != NULL;
	    dle = AVL_NEXT(&dl->dl_tree, dle)) {
		if (func(args, dle) != 0)
			break;
	}
}
void
dsl_deadlist_open(dsl_deadlist_t *dl, objset_t *os, uint64_t object)
{
	dmu_object_info_t doi;
	ASSERT(!dsl_deadlist_is_open(dl));
	mutex_init(&dl->dl_lock, NULL, MUTEX_DEFAULT, NULL);
	dl->dl_os = os;
	dl->dl_object = object;
	VERIFY0(dmu_bonus_hold(os, object, dl, &dl->dl_dbuf));
	dmu_object_info_from_db(dl->dl_dbuf, &doi);
	if (doi.doi_type == DMU_OT_BPOBJ) {
		dmu_buf_rele(dl->dl_dbuf, dl);
		dl->dl_dbuf = NULL;
		dl->dl_oldfmt = B_TRUE;
		VERIFY0(bpobj_open(&dl->dl_bpobj, os, object));
		return;
	}
	dl->dl_oldfmt = B_FALSE;
	dl->dl_phys = dl->dl_dbuf->db_data;
	dl->dl_havetree = B_FALSE;
	dl->dl_havecache = B_FALSE;
}
boolean_t
dsl_deadlist_is_open(dsl_deadlist_t *dl)
{
	return (dl->dl_os != NULL);
}
void
dsl_deadlist_close(dsl_deadlist_t *dl)
{
	ASSERT(dsl_deadlist_is_open(dl));
	mutex_destroy(&dl->dl_lock);
	if (dl->dl_oldfmt) {
		dl->dl_oldfmt = B_FALSE;
		bpobj_close(&dl->dl_bpobj);
		dl->dl_os = NULL;
		dl->dl_object = 0;
		return;
	}
	if (dl->dl_havetree) {
		dsl_deadlist_entry_t *dle;
		void *cookie = NULL;
		while ((dle = avl_destroy_nodes(&dl->dl_tree, &cookie))
		    != NULL) {
			bpobj_close(&dle->dle_bpobj);
			kmem_free(dle, sizeof (*dle));
		}
		avl_destroy(&dl->dl_tree);
	}
	if (dl->dl_havecache) {
		dsl_deadlist_cache_entry_t *dlce;
		void *cookie = NULL;
		while ((dlce = avl_destroy_nodes(&dl->dl_cache, &cookie))
		    != NULL) {
			kmem_free(dlce, sizeof (*dlce));
		}
		avl_destroy(&dl->dl_cache);
	}
	dmu_buf_rele(dl->dl_dbuf, dl);
	dl->dl_dbuf = NULL;
	dl->dl_phys = NULL;
	dl->dl_os = NULL;
	dl->dl_object = 0;
}
uint64_t
dsl_deadlist_alloc(objset_t *os, dmu_tx_t *tx)
{
	if (spa_version(dmu_objset_spa(os)) < SPA_VERSION_DEADLISTS)
		return (bpobj_alloc(os, SPA_OLD_MAXBLOCKSIZE, tx));
	return (zap_create(os, DMU_OT_DEADLIST, DMU_OT_DEADLIST_HDR,
	    sizeof (dsl_deadlist_phys_t), tx));
}
void
dsl_deadlist_free(objset_t *os, uint64_t dlobj, dmu_tx_t *tx)
{
	dmu_object_info_t doi;
	zap_cursor_t zc;
	zap_attribute_t za;
	int error;
	VERIFY0(dmu_object_info(os, dlobj, &doi));
	if (doi.doi_type == DMU_OT_BPOBJ) {
		bpobj_free(os, dlobj, tx);
		return;
	}
	for (zap_cursor_init(&zc, os, dlobj);
	    (error = zap_cursor_retrieve(&zc, &za)) == 0;
	    zap_cursor_advance(&zc)) {
		uint64_t obj = za.za_first_integer;
		if (obj == dmu_objset_pool(os)->dp_empty_bpobj)
			bpobj_decr_empty(os, tx);
		else
			bpobj_free(os, obj, tx);
	}
	VERIFY3U(error, ==, ENOENT);
	zap_cursor_fini(&zc);
	VERIFY0(dmu_object_free(os, dlobj, tx));
}
static void
dle_enqueue(dsl_deadlist_t *dl, dsl_deadlist_entry_t *dle,
    const blkptr_t *bp, boolean_t bp_freed, dmu_tx_t *tx)
{
	ASSERT(MUTEX_HELD(&dl->dl_lock));
	if (dle->dle_bpobj.bpo_object ==
	    dmu_objset_pool(dl->dl_os)->dp_empty_bpobj) {
		uint64_t obj = bpobj_alloc(dl->dl_os, SPA_OLD_MAXBLOCKSIZE, tx);
		bpobj_close(&dle->dle_bpobj);
		bpobj_decr_empty(dl->dl_os, tx);
		VERIFY0(bpobj_open(&dle->dle_bpobj, dl->dl_os, obj));
		VERIFY0(zap_update_int_key(dl->dl_os, dl->dl_object,
		    dle->dle_mintxg, obj, tx));
	}
	bpobj_enqueue(&dle->dle_bpobj, bp, bp_freed, tx);
}
static void
dle_enqueue_subobj(dsl_deadlist_t *dl, dsl_deadlist_entry_t *dle,
    uint64_t obj, dmu_tx_t *tx)
{
	ASSERT(MUTEX_HELD(&dl->dl_lock));
	if (dle->dle_bpobj.bpo_object !=
	    dmu_objset_pool(dl->dl_os)->dp_empty_bpobj) {
		bpobj_enqueue_subobj(&dle->dle_bpobj, obj, tx);
	} else {
		bpobj_close(&dle->dle_bpobj);
		bpobj_decr_empty(dl->dl_os, tx);
		VERIFY0(bpobj_open(&dle->dle_bpobj, dl->dl_os, obj));
		VERIFY0(zap_update_int_key(dl->dl_os, dl->dl_object,
		    dle->dle_mintxg, obj, tx));
	}
}
static void
dle_prefetch_subobj(dsl_deadlist_t *dl, dsl_deadlist_entry_t *dle,
    uint64_t obj)
{
	if (dle->dle_bpobj.bpo_object !=
	    dmu_objset_pool(dl->dl_os)->dp_empty_bpobj)
		bpobj_prefetch_subobj(&dle->dle_bpobj, obj);
}
void
dsl_deadlist_insert(dsl_deadlist_t *dl, const blkptr_t *bp, boolean_t bp_freed,
    dmu_tx_t *tx)
{
	dsl_deadlist_entry_t dle_tofind;
	dsl_deadlist_entry_t *dle;
	avl_index_t where;
	if (dl->dl_oldfmt) {
		bpobj_enqueue(&dl->dl_bpobj, bp, bp_freed, tx);
		return;
	}
	mutex_enter(&dl->dl_lock);
	dsl_deadlist_load_tree(dl);
	dmu_buf_will_dirty(dl->dl_dbuf, tx);
	int sign = bp_freed ? -1 : +1;
	dl->dl_phys->dl_used +=
	    sign * bp_get_dsize_sync(dmu_objset_spa(dl->dl_os), bp);
	dl->dl_phys->dl_comp += sign * BP_GET_PSIZE(bp);
	dl->dl_phys->dl_uncomp += sign * BP_GET_UCSIZE(bp);
	dle_tofind.dle_mintxg = bp->blk_birth;
	dle = avl_find(&dl->dl_tree, &dle_tofind, &where);
	if (dle == NULL)
		dle = avl_nearest(&dl->dl_tree, where, AVL_BEFORE);
	else
		dle = AVL_PREV(&dl->dl_tree, dle);
	if (dle == NULL) {
		zfs_panic_recover("blkptr at %p has invalid BLK_BIRTH %llu",
		    bp, (longlong_t)bp->blk_birth);
		dle = avl_first(&dl->dl_tree);
	}
	ASSERT3P(dle, !=, NULL);
	dle_enqueue(dl, dle, bp, bp_freed, tx);
	mutex_exit(&dl->dl_lock);
}
int
dsl_deadlist_insert_alloc_cb(void *arg, const blkptr_t *bp, dmu_tx_t *tx)
{
	dsl_deadlist_t *dl = arg;
	dsl_deadlist_insert(dl, bp, B_FALSE, tx);
	return (0);
}
int
dsl_deadlist_insert_free_cb(void *arg, const blkptr_t *bp, dmu_tx_t *tx)
{
	dsl_deadlist_t *dl = arg;
	dsl_deadlist_insert(dl, bp, B_TRUE, tx);
	return (0);
}
void
dsl_deadlist_add_key(dsl_deadlist_t *dl, uint64_t mintxg, dmu_tx_t *tx)
{
	uint64_t obj;
	dsl_deadlist_entry_t *dle;
	if (dl->dl_oldfmt)
		return;
	dle = kmem_alloc(sizeof (*dle), KM_SLEEP);
	dle->dle_mintxg = mintxg;
	mutex_enter(&dl->dl_lock);
	dsl_deadlist_load_tree(dl);
	obj = bpobj_alloc_empty(dl->dl_os, SPA_OLD_MAXBLOCKSIZE, tx);
	VERIFY0(bpobj_open(&dle->dle_bpobj, dl->dl_os, obj));
	avl_add(&dl->dl_tree, dle);
	VERIFY0(zap_add_int_key(dl->dl_os, dl->dl_object,
	    mintxg, obj, tx));
	mutex_exit(&dl->dl_lock);
}
void
dsl_deadlist_remove_key(dsl_deadlist_t *dl, uint64_t mintxg, dmu_tx_t *tx)
{
	dsl_deadlist_entry_t dle_tofind;
	dsl_deadlist_entry_t *dle, *dle_prev;
	if (dl->dl_oldfmt)
		return;
	mutex_enter(&dl->dl_lock);
	dsl_deadlist_load_tree(dl);
	dle_tofind.dle_mintxg = mintxg;
	dle = avl_find(&dl->dl_tree, &dle_tofind, NULL);
	ASSERT3P(dle, !=, NULL);
	dle_prev = AVL_PREV(&dl->dl_tree, dle);
	ASSERT3P(dle_prev, !=, NULL);
	dle_enqueue_subobj(dl, dle_prev, dle->dle_bpobj.bpo_object, tx);
	avl_remove(&dl->dl_tree, dle);
	bpobj_close(&dle->dle_bpobj);
	kmem_free(dle, sizeof (*dle));
	VERIFY0(zap_remove_int(dl->dl_os, dl->dl_object, mintxg, tx));
	mutex_exit(&dl->dl_lock);
}
void
dsl_deadlist_remove_entry(dsl_deadlist_t *dl, uint64_t mintxg, dmu_tx_t *tx)
{
	uint64_t used, comp, uncomp;
	dsl_deadlist_entry_t dle_tofind;
	dsl_deadlist_entry_t *dle;
	objset_t *os = dl->dl_os;
	if (dl->dl_oldfmt)
		return;
	mutex_enter(&dl->dl_lock);
	dsl_deadlist_load_tree(dl);
	dle_tofind.dle_mintxg = mintxg;
	dle = avl_find(&dl->dl_tree, &dle_tofind, NULL);
	VERIFY3P(dle, !=, NULL);
	avl_remove(&dl->dl_tree, dle);
	VERIFY0(zap_remove_int(os, dl->dl_object, mintxg, tx));
	VERIFY0(bpobj_space(&dle->dle_bpobj, &used, &comp, &uncomp));
	dmu_buf_will_dirty(dl->dl_dbuf, tx);
	dl->dl_phys->dl_used -= used;
	dl->dl_phys->dl_comp -= comp;
	dl->dl_phys->dl_uncomp -= uncomp;
	if (dle->dle_bpobj.bpo_object == dmu_objset_pool(os)->dp_empty_bpobj) {
		bpobj_decr_empty(os, tx);
	} else {
		bpobj_free(os, dle->dle_bpobj.bpo_object, tx);
	}
	bpobj_close(&dle->dle_bpobj);
	kmem_free(dle, sizeof (*dle));
	mutex_exit(&dl->dl_lock);
}
void
dsl_deadlist_clear_entry(dsl_deadlist_entry_t *dle, dsl_deadlist_t *dl,
    dmu_tx_t *tx)
{
	uint64_t new_obj, used, comp, uncomp;
	objset_t *os = dl->dl_os;
	mutex_enter(&dl->dl_lock);
	VERIFY0(zap_remove_int(os, dl->dl_object, dle->dle_mintxg, tx));
	VERIFY0(bpobj_space(&dle->dle_bpobj, &used, &comp, &uncomp));
	dmu_buf_will_dirty(dl->dl_dbuf, tx);
	dl->dl_phys->dl_used -= used;
	dl->dl_phys->dl_comp -= comp;
	dl->dl_phys->dl_uncomp -= uncomp;
	if (dle->dle_bpobj.bpo_object == dmu_objset_pool(os)->dp_empty_bpobj)
		bpobj_decr_empty(os, tx);
	else
		bpobj_free(os, dle->dle_bpobj.bpo_object, tx);
	bpobj_close(&dle->dle_bpobj);
	new_obj = bpobj_alloc_empty(os, SPA_OLD_MAXBLOCKSIZE, tx);
	VERIFY0(bpobj_open(&dle->dle_bpobj, os, new_obj));
	VERIFY0(zap_add_int_key(os, dl->dl_object, dle->dle_mintxg,
	    new_obj, tx));
	ASSERT(bpobj_is_empty(&dle->dle_bpobj));
	mutex_exit(&dl->dl_lock);
}
dsl_deadlist_entry_t *
dsl_deadlist_first(dsl_deadlist_t *dl)
{
	dsl_deadlist_entry_t *dle;
	mutex_enter(&dl->dl_lock);
	dsl_deadlist_load_tree(dl);
	dle = avl_first(&dl->dl_tree);
	mutex_exit(&dl->dl_lock);
	return (dle);
}
dsl_deadlist_entry_t *
dsl_deadlist_last(dsl_deadlist_t *dl)
{
	dsl_deadlist_entry_t *dle;
	mutex_enter(&dl->dl_lock);
	dsl_deadlist_load_tree(dl);
	dle = avl_last(&dl->dl_tree);
	mutex_exit(&dl->dl_lock);
	return (dle);
}
static void
dsl_deadlist_regenerate(objset_t *os, uint64_t dlobj,
    uint64_t mrs_obj, dmu_tx_t *tx)
{
	dsl_deadlist_t dl = { 0 };
	dsl_pool_t *dp = dmu_objset_pool(os);
	dsl_deadlist_open(&dl, os, dlobj);
	if (dl.dl_oldfmt) {
		dsl_deadlist_close(&dl);
		return;
	}
	while (mrs_obj != 0) {
		dsl_dataset_t *ds;
		VERIFY0(dsl_dataset_hold_obj(dp, mrs_obj, FTAG, &ds));
		dsl_deadlist_add_key(&dl,
		    dsl_dataset_phys(ds)->ds_prev_snap_txg, tx);
		mrs_obj = dsl_dataset_phys(ds)->ds_prev_snap_obj;
		dsl_dataset_rele(ds, FTAG);
	}
	dsl_deadlist_close(&dl);
}
uint64_t
dsl_deadlist_clone(dsl_deadlist_t *dl, uint64_t maxtxg,
    uint64_t mrs_obj, dmu_tx_t *tx)
{
	dsl_deadlist_entry_t *dle;
	uint64_t newobj;
	newobj = dsl_deadlist_alloc(dl->dl_os, tx);
	if (dl->dl_oldfmt) {
		dsl_deadlist_regenerate(dl->dl_os, newobj, mrs_obj, tx);
		return (newobj);
	}
	mutex_enter(&dl->dl_lock);
	dsl_deadlist_load_tree(dl);
	for (dle = avl_first(&dl->dl_tree); dle;
	    dle = AVL_NEXT(&dl->dl_tree, dle)) {
		uint64_t obj;
		if (dle->dle_mintxg >= maxtxg)
			break;
		obj = bpobj_alloc_empty(dl->dl_os, SPA_OLD_MAXBLOCKSIZE, tx);
		VERIFY0(zap_add_int_key(dl->dl_os, newobj,
		    dle->dle_mintxg, obj, tx));
	}
	mutex_exit(&dl->dl_lock);
	return (newobj);
}
void
dsl_deadlist_space(dsl_deadlist_t *dl,
    uint64_t *usedp, uint64_t *compp, uint64_t *uncompp)
{
	ASSERT(dsl_deadlist_is_open(dl));
	if (dl->dl_oldfmt) {
		VERIFY0(bpobj_space(&dl->dl_bpobj,
		    usedp, compp, uncompp));
		return;
	}
	mutex_enter(&dl->dl_lock);
	*usedp = dl->dl_phys->dl_used;
	*compp = dl->dl_phys->dl_comp;
	*uncompp = dl->dl_phys->dl_uncomp;
	mutex_exit(&dl->dl_lock);
}
void
dsl_deadlist_space_range(dsl_deadlist_t *dl, uint64_t mintxg, uint64_t maxtxg,
    uint64_t *usedp, uint64_t *compp, uint64_t *uncompp)
{
	dsl_deadlist_cache_entry_t *dlce;
	dsl_deadlist_cache_entry_t dlce_tofind;
	avl_index_t where;
	if (dl->dl_oldfmt) {
		VERIFY0(bpobj_space_range(&dl->dl_bpobj,
		    mintxg, maxtxg, usedp, compp, uncompp));
		return;
	}
	*usedp = *compp = *uncompp = 0;
	mutex_enter(&dl->dl_lock);
	dsl_deadlist_load_cache(dl);
	dlce_tofind.dlce_mintxg = mintxg;
	dlce = avl_find(&dl->dl_cache, &dlce_tofind, &where);
	if (dlce == NULL)
		dlce = avl_nearest(&dl->dl_cache, where, AVL_AFTER);
	for (; dlce && dlce->dlce_mintxg < maxtxg;
	    dlce = AVL_NEXT(&dl->dl_tree, dlce)) {
		*usedp += dlce->dlce_bytes;
		*compp += dlce->dlce_comp;
		*uncompp += dlce->dlce_uncomp;
	}
	mutex_exit(&dl->dl_lock);
}
static void
dsl_deadlist_insert_bpobj(dsl_deadlist_t *dl, uint64_t obj, uint64_t birth,
    dmu_tx_t *tx)
{
	dsl_deadlist_entry_t dle_tofind;
	dsl_deadlist_entry_t *dle;
	avl_index_t where;
	uint64_t used, comp, uncomp;
	bpobj_t bpo;
	ASSERT(MUTEX_HELD(&dl->dl_lock));
	VERIFY0(bpobj_open(&bpo, dl->dl_os, obj));
	VERIFY0(bpobj_space(&bpo, &used, &comp, &uncomp));
	bpobj_close(&bpo);
	dsl_deadlist_load_tree(dl);
	dmu_buf_will_dirty(dl->dl_dbuf, tx);
	dl->dl_phys->dl_used += used;
	dl->dl_phys->dl_comp += comp;
	dl->dl_phys->dl_uncomp += uncomp;
	dle_tofind.dle_mintxg = birth;
	dle = avl_find(&dl->dl_tree, &dle_tofind, &where);
	if (dle == NULL)
		dle = avl_nearest(&dl->dl_tree, where, AVL_BEFORE);
	dle_enqueue_subobj(dl, dle, obj, tx);
}
static void
dsl_deadlist_prefetch_bpobj(dsl_deadlist_t *dl, uint64_t obj, uint64_t birth)
{
	dsl_deadlist_entry_t dle_tofind;
	dsl_deadlist_entry_t *dle;
	avl_index_t where;
	ASSERT(MUTEX_HELD(&dl->dl_lock));
	dsl_deadlist_load_tree(dl);
	dle_tofind.dle_mintxg = birth;
	dle = avl_find(&dl->dl_tree, &dle_tofind, &where);
	if (dle == NULL)
		dle = avl_nearest(&dl->dl_tree, where, AVL_BEFORE);
	dle_prefetch_subobj(dl, dle, obj);
}
static int
dsl_deadlist_insert_cb(void *arg, const blkptr_t *bp, boolean_t bp_freed,
    dmu_tx_t *tx)
{
	dsl_deadlist_t *dl = arg;
	dsl_deadlist_insert(dl, bp, bp_freed, tx);
	return (0);
}
void
dsl_deadlist_merge(dsl_deadlist_t *dl, uint64_t obj, dmu_tx_t *tx)
{
	zap_cursor_t zc, pzc;
	zap_attribute_t *za, *pza;
	dmu_buf_t *bonus;
	dsl_deadlist_phys_t *dlp;
	dmu_object_info_t doi;
	int error, perror, i;
	VERIFY0(dmu_object_info(dl->dl_os, obj, &doi));
	if (doi.doi_type == DMU_OT_BPOBJ) {
		bpobj_t bpo;
		VERIFY0(bpobj_open(&bpo, dl->dl_os, obj));
		VERIFY0(bpobj_iterate(&bpo, dsl_deadlist_insert_cb, dl, tx));
		bpobj_close(&bpo);
		return;
	}
	za = kmem_alloc(sizeof (*za), KM_SLEEP);
	pza = kmem_alloc(sizeof (*pza), KM_SLEEP);
	mutex_enter(&dl->dl_lock);
	for (zap_cursor_init(&pzc, dl->dl_os, obj), i = 0;
	    (perror = zap_cursor_retrieve(&pzc, pza)) == 0 && i < 128;
	    zap_cursor_advance(&pzc), i++) {
		dsl_deadlist_prefetch_bpobj(dl, pza->za_first_integer,
		    zfs_strtonum(pza->za_name, NULL));
	}
	for (zap_cursor_init(&zc, dl->dl_os, obj);
	    (error = zap_cursor_retrieve(&zc, za)) == 0;
	    zap_cursor_advance(&zc)) {
		dsl_deadlist_insert_bpobj(dl, za->za_first_integer,
		    zfs_strtonum(za->za_name, NULL), tx);
		VERIFY0(zap_remove(dl->dl_os, obj, za->za_name, tx));
		if (perror == 0) {
			dsl_deadlist_prefetch_bpobj(dl, pza->za_first_integer,
			    zfs_strtonum(pza->za_name, NULL));
			zap_cursor_advance(&pzc);
			perror = zap_cursor_retrieve(&pzc, pza);
		}
	}
	VERIFY3U(error, ==, ENOENT);
	zap_cursor_fini(&zc);
	zap_cursor_fini(&pzc);
	VERIFY0(dmu_bonus_hold(dl->dl_os, obj, FTAG, &bonus));
	dlp = bonus->db_data;
	dmu_buf_will_dirty(bonus, tx);
	memset(dlp, 0, sizeof (*dlp));
	dmu_buf_rele(bonus, FTAG);
	mutex_exit(&dl->dl_lock);
	kmem_free(za, sizeof (*za));
	kmem_free(pza, sizeof (*pza));
}
void
dsl_deadlist_move_bpobj(dsl_deadlist_t *dl, bpobj_t *bpo, uint64_t mintxg,
    dmu_tx_t *tx)
{
	dsl_deadlist_entry_t dle_tofind;
	dsl_deadlist_entry_t *dle, *pdle;
	avl_index_t where;
	int i;
	ASSERT(!dl->dl_oldfmt);
	mutex_enter(&dl->dl_lock);
	dmu_buf_will_dirty(dl->dl_dbuf, tx);
	dsl_deadlist_load_tree(dl);
	dle_tofind.dle_mintxg = mintxg;
	dle = avl_find(&dl->dl_tree, &dle_tofind, &where);
	if (dle == NULL)
		dle = avl_nearest(&dl->dl_tree, where, AVL_AFTER);
	for (pdle = dle, i = 0; pdle && i < 128; i++) {
		bpobj_prefetch_subobj(bpo, pdle->dle_bpobj.bpo_object);
		pdle = AVL_NEXT(&dl->dl_tree, pdle);
	}
	while (dle) {
		uint64_t used, comp, uncomp;
		dsl_deadlist_entry_t *dle_next;
		bpobj_enqueue_subobj(bpo, dle->dle_bpobj.bpo_object, tx);
		if (pdle) {
			bpobj_prefetch_subobj(bpo, pdle->dle_bpobj.bpo_object);
			pdle = AVL_NEXT(&dl->dl_tree, pdle);
		}
		VERIFY0(bpobj_space(&dle->dle_bpobj,
		    &used, &comp, &uncomp));
		ASSERT3U(dl->dl_phys->dl_used, >=, used);
		ASSERT3U(dl->dl_phys->dl_comp, >=, comp);
		ASSERT3U(dl->dl_phys->dl_uncomp, >=, uncomp);
		dl->dl_phys->dl_used -= used;
		dl->dl_phys->dl_comp -= comp;
		dl->dl_phys->dl_uncomp -= uncomp;
		VERIFY0(zap_remove_int(dl->dl_os, dl->dl_object,
		    dle->dle_mintxg, tx));
		dle_next = AVL_NEXT(&dl->dl_tree, dle);
		avl_remove(&dl->dl_tree, dle);
		bpobj_close(&dle->dle_bpobj);
		kmem_free(dle, sizeof (*dle));
		dle = dle_next;
	}
	mutex_exit(&dl->dl_lock);
}
typedef struct livelist_entry {
	blkptr_t le_bp;
	uint32_t le_refcnt;
	avl_node_t le_node;
} livelist_entry_t;
static int
livelist_compare(const void *larg, const void *rarg)
{
	const blkptr_t *l = &((livelist_entry_t *)larg)->le_bp;
	const blkptr_t *r = &((livelist_entry_t *)rarg)->le_bp;
	uint64_t l_dva0_vdev = DVA_GET_VDEV(&l->blk_dva[0]);
	uint64_t r_dva0_vdev = DVA_GET_VDEV(&r->blk_dva[0]);
	if (l_dva0_vdev != r_dva0_vdev)
		return (TREE_CMP(l_dva0_vdev, r_dva0_vdev));
	uint64_t l_dva0_offset = DVA_GET_OFFSET(&l->blk_dva[0]);
	uint64_t r_dva0_offset = DVA_GET_OFFSET(&r->blk_dva[0]);
	if (l_dva0_offset == r_dva0_offset)
		ASSERT3U(l->blk_birth, ==, r->blk_birth);
	return (TREE_CMP(l_dva0_offset, r_dva0_offset));
}
struct livelist_iter_arg {
	avl_tree_t *avl;
	bplist_t *to_free;
	zthr_t *t;
};
static int
dsl_livelist_iterate(void *arg, const blkptr_t *bp, boolean_t bp_freed,
    dmu_tx_t *tx)
{
	struct livelist_iter_arg *lia = arg;
	avl_tree_t *avl = lia->avl;
	bplist_t *to_free = lia->to_free;
	zthr_t *t = lia->t;
	ASSERT(tx == NULL);
	if ((t != NULL) && (zthr_has_waiters(t) || zthr_iscancelled(t)))
		return (SET_ERROR(EINTR));
	livelist_entry_t node;
	node.le_bp = *bp;
	livelist_entry_t *found = avl_find(avl, &node, NULL);
	if (bp_freed) {
		if (found == NULL) {
			livelist_entry_t *e =
			    kmem_alloc(sizeof (livelist_entry_t), KM_SLEEP);
			e->le_bp = *bp;
			e->le_refcnt = 1;
			avl_add(avl, e);
		} else {
			ASSERT(BP_GET_DEDUP(bp));
			ASSERT3U(BP_GET_CHECKSUM(bp), ==,
			    BP_GET_CHECKSUM(&found->le_bp));
			ASSERT3U(found->le_refcnt + 1, >, found->le_refcnt);
			found->le_refcnt++;
		}
	} else {
		if (found == NULL) {
			bplist_append(to_free, bp);
		} else {
			ASSERT3U(found->le_refcnt, !=, 0);
			found->le_refcnt--;
			if (found->le_refcnt == 0) {
				avl_remove(avl, found);
				kmem_free(found, sizeof (livelist_entry_t));
			} else {
				ASSERT(BP_GET_DEDUP(bp));
				ASSERT3U(BP_GET_CHECKSUM(bp), ==,
				    BP_GET_CHECKSUM(&found->le_bp));
			}
		}
	}
	return (0);
}
int
dsl_process_sub_livelist(bpobj_t *bpobj, bplist_t *to_free, zthr_t *t,
    uint64_t *size)
{
	avl_tree_t avl;
	avl_create(&avl, livelist_compare, sizeof (livelist_entry_t),
	    offsetof(livelist_entry_t, le_node));
	struct livelist_iter_arg arg = {
	    .avl = &avl,
	    .to_free = to_free,
	    .t = t
	};
	int err = bpobj_iterate_nofree(bpobj, dsl_livelist_iterate, &arg, size);
	VERIFY(err != 0 || avl_numnodes(&avl) == 0);
	void *cookie = NULL;
	livelist_entry_t *le = NULL;
	while ((le = avl_destroy_nodes(&avl, &cookie)) != NULL) {
		kmem_free(le, sizeof (livelist_entry_t));
	}
	avl_destroy(&avl);
	return (err);
}
ZFS_MODULE_PARAM(zfs_livelist, zfs_livelist_, max_entries, U64, ZMOD_RW,
	"Size to start the next sub-livelist in a livelist");
ZFS_MODULE_PARAM(zfs_livelist, zfs_livelist_, min_percent_shared, INT, ZMOD_RW,
	"Threshold at which livelist is disabled");
