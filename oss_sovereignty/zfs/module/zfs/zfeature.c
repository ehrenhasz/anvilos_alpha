#include <sys/zfs_context.h>
#include <sys/zfeature.h>
#include <sys/dmu.h>
#include <sys/nvpair.h>
#include <sys/zap.h>
#include <sys/dmu_tx.h>
#include "zfeature_common.h"
#include <sys/spa_impl.h>
typedef enum {
	FEATURE_ACTION_INCR,
	FEATURE_ACTION_DECR,
} feature_action_t;
boolean_t
spa_features_check(spa_t *spa, boolean_t for_write,
    nvlist_t *unsup_feat, nvlist_t *enabled_feat)
{
	objset_t *os = spa->spa_meta_objset;
	boolean_t supported;
	zap_cursor_t *zc;
	zap_attribute_t *za;
	uint64_t obj = for_write ?
	    spa->spa_feat_for_write_obj : spa->spa_feat_for_read_obj;
	char *buf;
	zc = kmem_alloc(sizeof (zap_cursor_t), KM_SLEEP);
	za = kmem_alloc(sizeof (zap_attribute_t), KM_SLEEP);
	buf = kmem_alloc(MAXPATHLEN, KM_SLEEP);
	supported = B_TRUE;
	for (zap_cursor_init(zc, os, obj);
	    zap_cursor_retrieve(zc, za) == 0;
	    zap_cursor_advance(zc)) {
		ASSERT(za->za_integer_length == sizeof (uint64_t) &&
		    za->za_num_integers == 1);
		if (NULL != enabled_feat) {
			fnvlist_add_uint64(enabled_feat, za->za_name,
			    za->za_first_integer);
		}
		if (za->za_first_integer != 0 &&
		    !zfeature_is_supported(za->za_name)) {
			supported = B_FALSE;
			if (NULL != unsup_feat) {
				const char *desc = "";
				if (zap_lookup(os, spa->spa_feat_desc_obj,
				    za->za_name, 1, MAXPATHLEN, buf) == 0)
					desc = buf;
				VERIFY(nvlist_add_string(unsup_feat,
				    za->za_name, desc) == 0);
			}
		}
	}
	zap_cursor_fini(zc);
	kmem_free(buf, MAXPATHLEN);
	kmem_free(za, sizeof (zap_attribute_t));
	kmem_free(zc, sizeof (zap_cursor_t));
	return (supported);
}
int
feature_get_refcount(spa_t *spa, zfeature_info_t *feature, uint64_t *res)
{
	ASSERT(VALID_FEATURE_FID(feature->fi_feature));
	if (spa->spa_feat_refcount_cache[feature->fi_feature] ==
	    SPA_FEATURE_DISABLED) {
		return (SET_ERROR(ENOTSUP));
	}
	*res = spa->spa_feat_refcount_cache[feature->fi_feature];
	return (0);
}
int
feature_get_refcount_from_disk(spa_t *spa, zfeature_info_t *feature,
    uint64_t *res)
{
	int err;
	uint64_t refcount;
	uint64_t zapobj = (feature->fi_flags & ZFEATURE_FLAG_READONLY_COMPAT) ?
	    spa->spa_feat_for_write_obj : spa->spa_feat_for_read_obj;
	if (zapobj == 0)
		return (SET_ERROR(ENOTSUP));
	err = zap_lookup(spa->spa_meta_objset, zapobj,
	    feature->fi_guid, sizeof (uint64_t), 1, &refcount);
	if (err != 0) {
		if (err == ENOENT)
			return (SET_ERROR(ENOTSUP));
		else
			return (err);
	}
	*res = refcount;
	return (0);
}
static int
feature_get_enabled_txg(spa_t *spa, zfeature_info_t *feature, uint64_t *res)
{
	uint64_t enabled_txg_obj __maybe_unused = spa->spa_feat_enabled_txg_obj;
	ASSERT(zfeature_depends_on(feature->fi_feature,
	    SPA_FEATURE_ENABLED_TXG));
	if (!spa_feature_is_enabled(spa, feature->fi_feature)) {
		return (SET_ERROR(ENOTSUP));
	}
	ASSERT(enabled_txg_obj != 0);
	VERIFY0(zap_lookup(spa->spa_meta_objset, spa->spa_feat_enabled_txg_obj,
	    feature->fi_guid, sizeof (uint64_t), 1, res));
	return (0);
}
void
feature_sync(spa_t *spa, zfeature_info_t *feature, uint64_t refcount,
    dmu_tx_t *tx)
{
	ASSERT(VALID_FEATURE_OR_NONE(feature->fi_feature));
	uint64_t zapobj = (feature->fi_flags & ZFEATURE_FLAG_READONLY_COMPAT) ?
	    spa->spa_feat_for_write_obj : spa->spa_feat_for_read_obj;
	VERIFY0(zap_update(spa->spa_meta_objset, zapobj, feature->fi_guid,
	    sizeof (uint64_t), 1, &refcount, tx));
	if (feature->fi_feature != SPA_FEATURE_NONE) {
		uint64_t *refcount_cache =
		    &spa->spa_feat_refcount_cache[feature->fi_feature];
		VERIFY3U(*refcount_cache, ==,
		    atomic_swap_64(refcount_cache, refcount));
	}
	if (refcount == 0)
		spa_deactivate_mos_feature(spa, feature->fi_guid);
	else if (feature->fi_flags & ZFEATURE_FLAG_MOS)
		spa_activate_mos_feature(spa, feature->fi_guid, tx);
}
void
feature_enable_sync(spa_t *spa, zfeature_info_t *feature, dmu_tx_t *tx)
{
	uint64_t initial_refcount =
	    (feature->fi_flags & ZFEATURE_FLAG_ACTIVATE_ON_ENABLE) ? 1 : 0;
	uint64_t zapobj = (feature->fi_flags & ZFEATURE_FLAG_READONLY_COMPAT) ?
	    spa->spa_feat_for_write_obj : spa->spa_feat_for_read_obj;
	ASSERT(0 != zapobj);
	ASSERT(zfeature_is_valid_guid(feature->fi_guid));
	ASSERT3U(spa_version(spa), >=, SPA_VERSION_FEATURES);
	if (zap_contains(spa->spa_meta_objset, zapobj, feature->fi_guid) == 0)
		return;
	for (int i = 0; feature->fi_depends[i] != SPA_FEATURE_NONE; i++)
		spa_feature_enable(spa, feature->fi_depends[i], tx);
	VERIFY0(zap_update(spa->spa_meta_objset, spa->spa_feat_desc_obj,
	    feature->fi_guid, 1, strlen(feature->fi_desc) + 1,
	    feature->fi_desc, tx));
	feature_sync(spa, feature, initial_refcount, tx);
	if (spa_feature_is_enabled(spa, SPA_FEATURE_ENABLED_TXG)) {
		uint64_t enabling_txg = dmu_tx_get_txg(tx);
		if (spa->spa_feat_enabled_txg_obj == 0ULL) {
			spa->spa_feat_enabled_txg_obj =
			    zap_create_link(spa->spa_meta_objset,
			    DMU_OTN_ZAP_METADATA, DMU_POOL_DIRECTORY_OBJECT,
			    DMU_POOL_FEATURE_ENABLED_TXG, tx);
		}
		spa_feature_incr(spa, SPA_FEATURE_ENABLED_TXG, tx);
		VERIFY0(zap_add(spa->spa_meta_objset,
		    spa->spa_feat_enabled_txg_obj, feature->fi_guid,
		    sizeof (uint64_t), 1, &enabling_txg, tx));
	}
	if (spa->spa_errata == ZPOOL_ERRATA_ZOL_8308_ENCRYPTION &&
	    spa_feature_is_enabled(spa, SPA_FEATURE_ENCRYPTION) &&
	    !spa_feature_is_active(spa, SPA_FEATURE_ENCRYPTION) &&
	    feature->fi_feature == SPA_FEATURE_BOOKMARK_V2)
		spa->spa_errata = 0;
	if (feature->fi_feature == SPA_FEATURE_HEAD_ERRLOG)
		spa_upgrade_errlog(spa, tx);
}
static void
feature_do_action(spa_t *spa, spa_feature_t fid, feature_action_t action,
    dmu_tx_t *tx)
{
	uint64_t refcount = 0;
	zfeature_info_t *feature = &spa_feature_table[fid];
	uint64_t zapobj __maybe_unused =
	    (feature->fi_flags & ZFEATURE_FLAG_READONLY_COMPAT) ?
	    spa->spa_feat_for_write_obj : spa->spa_feat_for_read_obj;
	ASSERT(VALID_FEATURE_FID(fid));
	ASSERT(0 != zapobj);
	ASSERT(zfeature_is_valid_guid(feature->fi_guid));
	ASSERT(dmu_tx_is_syncing(tx));
	ASSERT3U(spa_version(spa), >=, SPA_VERSION_FEATURES);
	VERIFY3U(feature_get_refcount(spa, feature, &refcount), !=, ENOTSUP);
	switch (action) {
	case FEATURE_ACTION_INCR:
		VERIFY3U(refcount, !=, UINT64_MAX);
		refcount++;
		break;
	case FEATURE_ACTION_DECR:
		VERIFY3U(refcount, !=, 0);
		refcount--;
		break;
	default:
		ASSERT(0);
		break;
	}
	feature_sync(spa, feature, refcount, tx);
}
void
spa_feature_create_zap_objects(spa_t *spa, dmu_tx_t *tx)
{
	ASSERT((!spa->spa_sync_on && tx->tx_txg == TXG_INITIAL) ||
	    dsl_pool_sync_context(spa_get_dsl(spa)));
	spa->spa_feat_for_read_obj = zap_create_link(spa->spa_meta_objset,
	    DMU_OTN_ZAP_METADATA, DMU_POOL_DIRECTORY_OBJECT,
	    DMU_POOL_FEATURES_FOR_READ, tx);
	spa->spa_feat_for_write_obj = zap_create_link(spa->spa_meta_objset,
	    DMU_OTN_ZAP_METADATA, DMU_POOL_DIRECTORY_OBJECT,
	    DMU_POOL_FEATURES_FOR_WRITE, tx);
	spa->spa_feat_desc_obj = zap_create_link(spa->spa_meta_objset,
	    DMU_OTN_ZAP_METADATA, DMU_POOL_DIRECTORY_OBJECT,
	    DMU_POOL_FEATURE_DESCRIPTIONS, tx);
}
void
spa_feature_enable(spa_t *spa, spa_feature_t fid, dmu_tx_t *tx)
{
	ASSERT3U(spa_version(spa), >=, SPA_VERSION_FEATURES);
	ASSERT(VALID_FEATURE_FID(fid));
	feature_enable_sync(spa, &spa_feature_table[fid], tx);
}
void
spa_feature_incr(spa_t *spa, spa_feature_t fid, dmu_tx_t *tx)
{
	feature_do_action(spa, fid, FEATURE_ACTION_INCR, tx);
}
void
spa_feature_decr(spa_t *spa, spa_feature_t fid, dmu_tx_t *tx)
{
	feature_do_action(spa, fid, FEATURE_ACTION_DECR, tx);
}
boolean_t
spa_feature_is_enabled(spa_t *spa, spa_feature_t fid)
{
	int err;
	uint64_t refcount = 0;
	ASSERT(VALID_FEATURE_FID(fid));
	if (spa_version(spa) < SPA_VERSION_FEATURES)
		return (B_FALSE);
	err = feature_get_refcount(spa, &spa_feature_table[fid], &refcount);
	ASSERT(err == 0 || err == ENOTSUP);
	return (err == 0);
}
boolean_t
spa_feature_is_active(spa_t *spa, spa_feature_t fid)
{
	int err;
	uint64_t refcount = 0;
	ASSERT(VALID_FEATURE_FID(fid));
	if (spa_version(spa) < SPA_VERSION_FEATURES)
		return (B_FALSE);
	err = feature_get_refcount(spa, &spa_feature_table[fid], &refcount);
	ASSERT(err == 0 || err == ENOTSUP);
	return (err == 0 && refcount > 0);
}
boolean_t
spa_feature_enabled_txg(spa_t *spa, spa_feature_t fid, uint64_t *txg)
{
	int err;
	ASSERT(VALID_FEATURE_FID(fid));
	if (spa_version(spa) < SPA_VERSION_FEATURES)
		return (B_FALSE);
	err = feature_get_enabled_txg(spa, &spa_feature_table[fid], txg);
	ASSERT(err == 0 || err == ENOTSUP);
	return (err == 0);
}
