#ifndef	_SYS_DSL_USERHOLD_H
#define	_SYS_DSL_USERHOLD_H
#include <sys/nvpair.h>
#include <sys/types.h>
#ifdef	__cplusplus
extern "C" {
#endif
struct dsl_pool;
struct dsl_dataset;
struct dmu_tx;
int dsl_dataset_user_hold(nvlist_t *holds, minor_t cleanup_minor,
    nvlist_t *errlist);
int dsl_dataset_user_release(nvlist_t *holds, nvlist_t *errlist);
int dsl_dataset_get_holds(const char *dsname, nvlist_t *nvl);
void dsl_dataset_user_release_tmp(struct dsl_pool *dp, nvlist_t *holds);
int dsl_dataset_user_hold_check_one(struct dsl_dataset *ds, const char *htag,
    boolean_t temphold, struct dmu_tx *tx);
void dsl_dataset_user_hold_sync_one(struct dsl_dataset *ds, const char *htag,
    minor_t minor, uint64_t now, struct dmu_tx *tx);
#ifdef	__cplusplus
}
#endif
#endif  
