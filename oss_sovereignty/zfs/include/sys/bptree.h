#ifndef	_SYS_BPTREE_H
#define	_SYS_BPTREE_H
#include <sys/spa.h>
#include <sys/zio.h>
#ifdef	__cplusplus
extern "C" {
#endif
typedef struct bptree_phys {
	uint64_t bt_begin;
	uint64_t bt_end;
	uint64_t bt_bytes;
	uint64_t bt_comp;
	uint64_t bt_uncomp;
} bptree_phys_t;
typedef struct bptree_entry_phys {
	blkptr_t be_bp;
	uint64_t be_birth_txg;  
	zbookmark_phys_t be_zb;  
} bptree_entry_phys_t;
typedef int bptree_itor_t(void *arg, const blkptr_t *bp, dmu_tx_t *tx);
uint64_t bptree_alloc(objset_t *os, dmu_tx_t *tx);
int bptree_free(objset_t *os, uint64_t obj, dmu_tx_t *tx);
boolean_t bptree_is_empty(objset_t *os, uint64_t obj);
void bptree_add(objset_t *os, uint64_t obj, blkptr_t *bp, uint64_t birth_txg,
    uint64_t bytes, uint64_t comp, uint64_t uncomp, dmu_tx_t *tx);
int bptree_iterate(objset_t *os, uint64_t obj, boolean_t free,
    bptree_itor_t func, void *arg, dmu_tx_t *tx);
#ifdef	__cplusplus
}
#endif
#endif	 
