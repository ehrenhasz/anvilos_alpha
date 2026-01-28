


#ifndef _SYS_BRT_H
#define	_SYS_BRT_H

#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/fs/zfs.h>
#include <sys/zio.h>
#include <sys/dmu.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern boolean_t brt_entry_decref(spa_t *spa, const blkptr_t *bp);
extern uint64_t brt_entry_get_refcount(spa_t *spa, const blkptr_t *bp);

extern uint64_t brt_get_dspace(spa_t *spa);
extern uint64_t brt_get_used(spa_t *spa);
extern uint64_t brt_get_saved(spa_t *spa);
extern uint64_t brt_get_ratio(spa_t *spa);

extern boolean_t brt_maybe_exists(spa_t *spa, const blkptr_t *bp);
extern void brt_init(void);
extern void brt_fini(void);

extern void brt_pending_add(spa_t *spa, const blkptr_t *bp, dmu_tx_t *tx);
extern void brt_pending_remove(spa_t *spa, const blkptr_t *bp, dmu_tx_t *tx);
extern void brt_pending_apply(spa_t *spa, uint64_t txg);

extern void brt_create(spa_t *spa);
extern int brt_load(spa_t *spa);
extern void brt_unload(spa_t *spa);
extern void brt_sync(spa_t *spa, uint64_t txg);

#ifdef	__cplusplus
}
#endif

#endif	
