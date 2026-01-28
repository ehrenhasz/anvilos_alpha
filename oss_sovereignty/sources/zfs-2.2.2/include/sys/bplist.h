


#ifndef	_SYS_BPLIST_H
#define	_SYS_BPLIST_H

#include <sys/zfs_context.h>
#include <sys/spa.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct bplist_entry {
	blkptr_t	bpe_blk;
	list_node_t	bpe_node;
} bplist_entry_t;

typedef struct bplist {
	kmutex_t	bpl_lock;
	list_t		bpl_list;
} bplist_t;

typedef int bplist_itor_t(void *arg, const blkptr_t *bp, dmu_tx_t *tx);

void bplist_create(bplist_t *bpl);
void bplist_destroy(bplist_t *bpl);
void bplist_append(bplist_t *bpl, const blkptr_t *bp);
void bplist_iterate(bplist_t *bpl, bplist_itor_t *func,
    void *arg, dmu_tx_t *tx);
void bplist_clear(bplist_t *bpl);

#ifdef	__cplusplus
}
#endif

#endif 
