 
 
 

#ifndef _SYS_TXG_H
#define	_SYS_TXG_H

#include <sys/spa.h>
#include <sys/zfs_context.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	TXG_CONCURRENT_STATES	3	 
#define	TXG_SIZE		4		 
#define	TXG_MASK		(TXG_SIZE - 1)	 
#define	TXG_INITIAL		TXG_SIZE	 
#define	TXG_IDX			(txg & TXG_MASK)
#define	TXG_UNKNOWN		0

 
#define	TXG_DEFER_SIZE		2

typedef struct tx_cpu tx_cpu_t;

typedef struct txg_handle {
	tx_cpu_t	*th_cpu;
	uint64_t	th_txg;
} txg_handle_t;

typedef struct txg_node {
	struct txg_node	*tn_next[TXG_SIZE];
	uint8_t		tn_member[TXG_SIZE];
} txg_node_t;

typedef struct txg_list {
	kmutex_t	tl_lock;
	size_t		tl_offset;
	spa_t		*tl_spa;
	txg_node_t	*tl_head[TXG_SIZE];
} txg_list_t;

struct dsl_pool;

extern void txg_init(struct dsl_pool *dp, uint64_t txg);
extern void txg_fini(struct dsl_pool *dp);
extern void txg_sync_start(struct dsl_pool *dp);
extern void txg_sync_stop(struct dsl_pool *dp);
extern uint64_t txg_hold_open(struct dsl_pool *dp, txg_handle_t *txghp);
extern void txg_rele_to_quiesce(txg_handle_t *txghp);
extern void txg_rele_to_sync(txg_handle_t *txghp);
extern void txg_register_callbacks(txg_handle_t *txghp, list_t *tx_callbacks);

extern void txg_delay(struct dsl_pool *dp, uint64_t txg, hrtime_t delta,
    hrtime_t resolution);
extern void txg_kick(struct dsl_pool *dp, uint64_t txg);

 
extern void txg_wait_synced(struct dsl_pool *dp, uint64_t txg);

 
extern boolean_t txg_wait_synced_sig(struct dsl_pool *dp, uint64_t txg);

 
extern void txg_wait_open(struct dsl_pool *dp, uint64_t txg,
    boolean_t should_quiesce);

 
extern boolean_t txg_stalled(struct dsl_pool *dp);

 
extern boolean_t txg_sync_waiting(struct dsl_pool *dp);

extern void txg_verify(spa_t *spa, uint64_t txg);

 
extern void txg_wait_callbacks(struct dsl_pool *dp);

 

#define	TXG_CLEAN(txg)	((txg) - 1)

extern void txg_list_create(txg_list_t *tl, spa_t *spa, size_t offset);
extern void txg_list_destroy(txg_list_t *tl);
extern boolean_t txg_list_empty(txg_list_t *tl, uint64_t txg);
extern boolean_t txg_all_lists_empty(txg_list_t *tl);
extern boolean_t txg_list_add(txg_list_t *tl, void *p, uint64_t txg);
extern boolean_t txg_list_add_tail(txg_list_t *tl, void *p, uint64_t txg);
extern void *txg_list_remove(txg_list_t *tl, uint64_t txg);
extern void *txg_list_remove_this(txg_list_t *tl, void *p, uint64_t txg);
extern boolean_t txg_list_member(txg_list_t *tl, void *p, uint64_t txg);
extern void *txg_list_head(txg_list_t *tl, uint64_t txg);
extern void *txg_list_next(txg_list_t *tl, void *p, uint64_t txg);

 
extern uint_t zfs_txg_timeout;


#ifdef ZFS_DEBUG
#define	TXG_VERIFY(spa, txg)		txg_verify(spa, txg)
#else
#define	TXG_VERIFY(spa, txg)
#endif

#ifdef	__cplusplus
}
#endif

#endif	 
