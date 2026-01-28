




#ifndef _SYS_SPACE_MAP_H
#define	_SYS_SPACE_MAP_H

#include <sys/avl.h>
#include <sys/range_tree.h>
#include <sys/dmu.h>

#ifdef	__cplusplus
extern "C" {
#endif


#define	SPACE_MAP_SIZE_V0	(3 * sizeof (uint64_t))
#define	SPACE_MAP_HISTOGRAM_SIZE	32


typedef struct space_map_phys {
	
	uint64_t	smp_object;

	
	uint64_t	smp_length;

	
	int64_t		smp_alloc;

	
	uint64_t	smp_pad[5];

	
	uint64_t	smp_histogram[SPACE_MAP_HISTOGRAM_SIZE];
} space_map_phys_t;


typedef struct space_map {
	uint64_t	sm_start;	
	uint64_t	sm_size;	
	uint8_t		sm_shift;	
	objset_t	*sm_os;		
	uint64_t	sm_object;	
	uint32_t	sm_blksz;	
	dmu_buf_t	*sm_dbuf;	
	space_map_phys_t *sm_phys;	
} space_map_t;



typedef enum {
	SM_ALLOC,
	SM_FREE
} maptype_t;

typedef struct space_map_entry {
	maptype_t sme_type;
	uint32_t sme_vdev;	
	uint64_t sme_offset;	
	uint64_t sme_run;	

	
	uint64_t sme_txg;
	uint64_t sme_sync_pass;
} space_map_entry_t;

#define	SM_NO_VDEVID	(1 << SPA_VDEVBITS)


#define	SM_DEBUG_PREFIX	2
#define	SM_OFFSET_BITS	47
#define	SM_RUN_BITS	15


#define	SM2_PREFIX	3
#define	SM2_OFFSET_BITS	63
#define	SM2_RUN_BITS	36

#define	SM_PREFIX_DECODE(x)	BF64_DECODE(x, 62, 2)
#define	SM_PREFIX_ENCODE(x)	BF64_ENCODE(x, 62, 2)

#define	SM_DEBUG_ACTION_DECODE(x)	BF64_DECODE(x, 60, 2)
#define	SM_DEBUG_ACTION_ENCODE(x)	BF64_ENCODE(x, 60, 2)
#define	SM_DEBUG_SYNCPASS_DECODE(x)	BF64_DECODE(x, 50, 10)
#define	SM_DEBUG_SYNCPASS_ENCODE(x)	BF64_ENCODE(x, 50, 10)
#define	SM_DEBUG_TXG_DECODE(x)		BF64_DECODE(x, 0, 50)
#define	SM_DEBUG_TXG_ENCODE(x)		BF64_ENCODE(x, 0, 50)

#define	SM_OFFSET_DECODE(x)	BF64_DECODE(x, 16, SM_OFFSET_BITS)
#define	SM_OFFSET_ENCODE(x)	BF64_ENCODE(x, 16, SM_OFFSET_BITS)
#define	SM_TYPE_DECODE(x)	BF64_DECODE(x, 15, 1)
#define	SM_TYPE_ENCODE(x)	BF64_ENCODE(x, 15, 1)
#define	SM_RUN_DECODE(x)	(BF64_DECODE(x, 0, SM_RUN_BITS) + 1)
#define	SM_RUN_ENCODE(x)	BF64_ENCODE((x) - 1, 0, SM_RUN_BITS)
#define	SM_RUN_MAX		SM_RUN_DECODE(~0ULL)
#define	SM_OFFSET_MAX		SM_OFFSET_DECODE(~0ULL)

#define	SM2_RUN_DECODE(x)	(BF64_DECODE(x, SPA_VDEVBITS, SM2_RUN_BITS) + 1)
#define	SM2_RUN_ENCODE(x)	BF64_ENCODE((x) - 1, SPA_VDEVBITS, SM2_RUN_BITS)
#define	SM2_VDEV_DECODE(x)	BF64_DECODE(x, 0, SPA_VDEVBITS)
#define	SM2_VDEV_ENCODE(x)	BF64_ENCODE(x, 0, SPA_VDEVBITS)
#define	SM2_TYPE_DECODE(x)	BF64_DECODE(x, SM2_OFFSET_BITS, 1)
#define	SM2_TYPE_ENCODE(x)	BF64_ENCODE(x, SM2_OFFSET_BITS, 1)
#define	SM2_OFFSET_DECODE(x)	BF64_DECODE(x, 0, SM2_OFFSET_BITS)
#define	SM2_OFFSET_ENCODE(x)	BF64_ENCODE(x, 0, SM2_OFFSET_BITS)
#define	SM2_RUN_MAX		SM2_RUN_DECODE(~0ULL)
#define	SM2_OFFSET_MAX		SM2_OFFSET_DECODE(~0ULL)

boolean_t sm_entry_is_debug(uint64_t e);
boolean_t sm_entry_is_single_word(uint64_t e);
boolean_t sm_entry_is_double_word(uint64_t e);

typedef int (*sm_cb_t)(space_map_entry_t *sme, void *arg);

int space_map_load(space_map_t *sm, range_tree_t *rt, maptype_t maptype);
int space_map_load_length(space_map_t *sm, range_tree_t *rt, maptype_t maptype,
    uint64_t length);
int space_map_iterate(space_map_t *sm, uint64_t length,
    sm_cb_t callback, void *arg);
int space_map_incremental_destroy(space_map_t *sm, sm_cb_t callback, void *arg,
    dmu_tx_t *tx);

boolean_t space_map_histogram_verify(space_map_t *sm, range_tree_t *rt);
void space_map_histogram_clear(space_map_t *sm);
void space_map_histogram_add(space_map_t *sm, range_tree_t *rt,
    dmu_tx_t *tx);

uint64_t space_map_object(space_map_t *sm);
int64_t space_map_allocated(space_map_t *sm);
uint64_t space_map_length(space_map_t *sm);
uint64_t space_map_entries(space_map_t *sm, range_tree_t *rt);
uint64_t space_map_nblocks(space_map_t *sm);

void space_map_write(space_map_t *sm, range_tree_t *rt, maptype_t maptype,
    uint64_t vdev_id, dmu_tx_t *tx);
uint64_t space_map_estimate_optimal_size(space_map_t *sm, range_tree_t *rt,
    uint64_t vdev_id);
void space_map_truncate(space_map_t *sm, int blocksize, dmu_tx_t *tx);
uint64_t space_map_alloc(objset_t *os, int blocksize, dmu_tx_t *tx);
void space_map_free(space_map_t *sm, dmu_tx_t *tx);
void space_map_free_obj(objset_t *os, uint64_t smobj, dmu_tx_t *tx);

int space_map_open(space_map_t **smp, objset_t *os, uint64_t object,
    uint64_t start, uint64_t size, uint8_t shift);
void space_map_close(space_map_t *sm);

#ifdef	__cplusplus
}
#endif

#endif	
