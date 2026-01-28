#ifndef	_SYS_DMU_TRAVERSE_H
#define	_SYS_DMU_TRAVERSE_H
#include <sys/zfs_context.h>
#include <sys/spa.h>
#include <sys/zio.h>
#ifdef	__cplusplus
extern "C" {
#endif
struct dnode_phys;
struct dsl_dataset;
struct zilog;
struct arc_buf;
typedef int (blkptr_cb_t)(spa_t *spa, zilog_t *zilog, const blkptr_t *bp,
    const zbookmark_phys_t *zb, const struct dnode_phys *dnp, void *arg);
#define	TRAVERSE_PRE			(1<<0)
#define	TRAVERSE_POST			(1<<1)
#define	TRAVERSE_PREFETCH_METADATA	(1<<2)
#define	TRAVERSE_PREFETCH_DATA		(1<<3)
#define	TRAVERSE_PREFETCH (TRAVERSE_PREFETCH_METADATA | TRAVERSE_PREFETCH_DATA)
#define	TRAVERSE_HARD			(1<<4)
#define	TRAVERSE_NO_DECRYPT		(1<<5)
#define	TRAVERSE_VISIT_NO_CHILDREN	-1
int traverse_dataset(struct dsl_dataset *ds,
    uint64_t txg_start, int flags, blkptr_cb_t func, void *arg);
int traverse_dataset_resume(struct dsl_dataset *ds, uint64_t txg_start,
    zbookmark_phys_t *resume, int flags, blkptr_cb_t func, void *arg);
int traverse_dataset_destroyed(spa_t *spa, blkptr_t *blkptr,
    uint64_t txg_start, zbookmark_phys_t *resume, int flags,
    blkptr_cb_t func, void *arg);
int traverse_pool(spa_t *spa,
    uint64_t txg_start, int flags, blkptr_cb_t func, void *arg);
static inline uint64_t
bp_span_in_blocks(uint8_t indblkshift, uint64_t level)
{
	unsigned int shift = level * (indblkshift - SPA_BLKPTRSHIFT);
	ASSERT3U(shift, <, 64);
	return (1ULL << shift);
}
#ifdef	__cplusplus
}
#endif
#endif  
