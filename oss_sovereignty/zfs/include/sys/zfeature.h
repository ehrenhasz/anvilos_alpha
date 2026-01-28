#ifndef _SYS_ZFEATURE_H
#define	_SYS_ZFEATURE_H
#include <sys/nvpair.h>
#include <sys/txg.h>
#include "zfeature_common.h"
#ifdef	__cplusplus
extern "C" {
#endif
#define	VALID_FEATURE_FID(fid)	((fid) >= 0 && (fid) < SPA_FEATURES)
#define	VALID_FEATURE_OR_NONE(fid)	((fid) == SPA_FEATURE_NONE ||	\
    VALID_FEATURE_FID(fid))
struct spa;
struct dmu_tx;
struct objset;
extern void spa_feature_create_zap_objects(struct spa *, struct dmu_tx *);
extern void spa_feature_enable(struct spa *, spa_feature_t,
    struct dmu_tx *);
extern void spa_feature_incr(struct spa *, spa_feature_t, struct dmu_tx *);
extern void spa_feature_decr(struct spa *, spa_feature_t, struct dmu_tx *);
extern boolean_t spa_feature_is_enabled(struct spa *, spa_feature_t);
extern boolean_t spa_feature_is_active(struct spa *, spa_feature_t);
extern boolean_t spa_feature_enabled_txg(spa_t *spa, spa_feature_t fid,
    uint64_t *txg);
extern uint64_t spa_feature_refcount(spa_t *, spa_feature_t, uint64_t);
extern boolean_t spa_features_check(spa_t *, boolean_t, nvlist_t *, nvlist_t *);
extern int feature_get_refcount(struct spa *, zfeature_info_t *, uint64_t *);
extern int feature_get_refcount_from_disk(spa_t *spa, zfeature_info_t *feature,
    uint64_t *res);
extern void feature_enable_sync(struct spa *, zfeature_info_t *,
    struct dmu_tx *);
extern void feature_sync(struct spa *, zfeature_info_t *, uint64_t,
    struct dmu_tx *);
#ifdef	__cplusplus
}
#endif
#endif  
