


#ifndef	_SYS_DNODE_H
#define	_SYS_DNODE_H

#include <sys/zfs_context.h>
#include <sys/avl.h>
#include <sys/spa.h>
#include <sys/txg.h>
#include <sys/zio.h>
#include <sys/zfs_refcount.h>
#include <sys/dmu_zfetch.h>
#include <sys/zrlock.h>
#include <sys/multilist.h>
#include <sys/wmsum.h>

#ifdef	__cplusplus
extern "C" {
#endif


#define	DNODE_MUST_BE_ALLOCATED	1
#define	DNODE_MUST_BE_FREE	2
#define	DNODE_DRY_RUN		4


#define	DNODE_FIND_HOLE		1
#define	DNODE_FIND_BACKWARDS	2
#define	DNODE_FIND_HAVELOCK	4


#define	DNODE_SHIFT		9	
#define	DN_MIN_INDBLKSHIFT	12	

#define	DN_MAX_INDBLKSHIFT	17	
#define	DNODE_BLOCK_SHIFT	14	
#define	DNODE_CORE_SIZE		64	
#define	DN_MAX_OBJECT_SHIFT	48	
#define	DN_MAX_OFFSET_SHIFT	64	


#define	DN_ID_CHKED_BONUS	0x1
#define	DN_ID_CHKED_SPILL	0x2
#define	DN_ID_OLD_EXIST		0x4
#define	DN_ID_NEW_EXIST		0x8


#define	DNODE_MIN_SIZE		(1 << DNODE_SHIFT)
#define	DNODE_MAX_SIZE		(1 << DNODE_BLOCK_SHIFT)
#define	DNODE_BLOCK_SIZE	(1 << DNODE_BLOCK_SHIFT)
#define	DNODE_MIN_SLOTS		(DNODE_MIN_SIZE >> DNODE_SHIFT)
#define	DNODE_MAX_SLOTS		(DNODE_MAX_SIZE >> DNODE_SHIFT)
#define	DN_BONUS_SIZE(dnsize)	((dnsize) - DNODE_CORE_SIZE - \
	(1 << SPA_BLKPTRSHIFT))
#define	DN_SLOTS_TO_BONUSLEN(slots)	DN_BONUS_SIZE((slots) << DNODE_SHIFT)
#define	DN_OLD_MAX_BONUSLEN	(DN_BONUS_SIZE(DNODE_MIN_SIZE))
#define	DN_MAX_NBLKPTR	((DNODE_MIN_SIZE - DNODE_CORE_SIZE) >> SPA_BLKPTRSHIFT)
#define	DN_MAX_OBJECT	(1ULL << DN_MAX_OBJECT_SHIFT)
#define	DN_ZERO_BONUSLEN	(DN_BONUS_SIZE(DNODE_MAX_SIZE) + 1)
#define	DN_KILL_SPILLBLK (1)

#define	DN_SLOT_UNINIT		((void *)NULL)	
#define	DN_SLOT_FREE		((void *)1UL)	
#define	DN_SLOT_ALLOCATED	((void *)2UL)	
#define	DN_SLOT_INTERIOR	((void *)3UL)	
#define	DN_SLOT_IS_PTR(dn)	((void *)dn > DN_SLOT_INTERIOR)
#define	DN_SLOT_IS_VALID(dn)	((void *)dn != NULL)

#define	DNODES_PER_BLOCK_SHIFT	(DNODE_BLOCK_SHIFT - DNODE_SHIFT)
#define	DNODES_PER_BLOCK	(1ULL << DNODES_PER_BLOCK_SHIFT)


#define	DNODES_PER_LEVEL_SHIFT	(DN_MAX_INDBLKSHIFT - SPA_BLKPTRSHIFT)
#define	DNODES_PER_LEVEL	(1ULL << DNODES_PER_LEVEL_SHIFT)

#define	DN_MAX_LEVELS	(DIV_ROUND_UP(DN_MAX_OFFSET_SHIFT - SPA_MINBLOCKSHIFT, \
	DN_MIN_INDBLKSHIFT - SPA_BLKPTRSHIFT) + 1)


#define	DN_BONUS(dnp)	((void*)((dnp)->dn_bonus_flexible + \
	(((dnp)->dn_nblkptr - 1) * sizeof (blkptr_t))))
#define	DN_MAX_BONUS_LEN(dnp) \
	((dnp->dn_flags & DNODE_FLAG_SPILL_BLKPTR) ? \
	(uint8_t *)DN_SPILL_BLKPTR(dnp) - (uint8_t *)DN_BONUS(dnp) : \
	(uint8_t *)(dnp + (dnp->dn_extra_slots + 1)) - (uint8_t *)DN_BONUS(dnp))

#define	DN_USED_BYTES(dnp) (((dnp)->dn_flags & DNODE_FLAG_USED_BYTES) ? \
	(dnp)->dn_used : (dnp)->dn_used << SPA_MINBLOCKSHIFT)

#define	EPB(blkshift, typeshift)	(1 << (blkshift - typeshift))

struct dmu_buf_impl;
struct objset;
struct zio;

enum dnode_dirtycontext {
	DN_UNDIRTIED,
	DN_DIRTY_OPEN,
	DN_DIRTY_SYNC
};


#define	DNODE_FLAG_USED_BYTES			(1 << 0)
#define	DNODE_FLAG_USERUSED_ACCOUNTED		(1 << 1)


#define	DNODE_FLAG_SPILL_BLKPTR			(1 << 2)


#define	DNODE_FLAG_USEROBJUSED_ACCOUNTED	(1 << 3)


#define	DNODE_CRYPT_PORTABLE_FLAGS_MASK		(DNODE_FLAG_SPILL_BLKPTR)



typedef struct dnode_phys {
	uint8_t dn_type;		
	uint8_t dn_indblkshift;		
	uint8_t dn_nlevels;		
	uint8_t dn_nblkptr;		
	uint8_t dn_bonustype;		
	uint8_t	dn_checksum;		
	uint8_t	dn_compress;		
	uint8_t dn_flags;		
	uint16_t dn_datablkszsec;	
	uint16_t dn_bonuslen;		
	uint8_t dn_extra_slots;		
	uint8_t dn_pad2[3];

	
	uint64_t dn_maxblkid;		
	uint64_t dn_used;		

	
	uint64_t dn_pad3[4];

	
	union {
		blkptr_t dn_blkptr[1+DN_OLD_MAX_BONUSLEN/sizeof (blkptr_t)];
		struct {
			blkptr_t __dn_ignore1;
			uint8_t dn_bonus[DN_OLD_MAX_BONUSLEN];
		};
		struct {
			blkptr_t __dn_ignore2;
			uint8_t __dn_ignore3[DN_OLD_MAX_BONUSLEN -
			    sizeof (blkptr_t)];
			blkptr_t dn_spill;
		};
		struct {
			blkptr_t __dn_ignore4;
			uint8_t dn_bonus_flexible[];
		};
	};
} dnode_phys_t;

#define	DN_SPILL_BLKPTR(dnp)	((blkptr_t *)((char *)(dnp) + \
	(((dnp)->dn_extra_slots + 1) << DNODE_SHIFT) - (1 << SPA_BLKPTRSHIFT)))

struct dnode {
	
	krwlock_t dn_struct_rwlock;

	
	list_node_t dn_link;

	
	struct objset *dn_objset;
	uint64_t dn_object;
	struct dmu_buf_impl *dn_dbuf;
	struct dnode_handle *dn_handle;
	dnode_phys_t *dn_phys; 

	
	dmu_object_type_t dn_type;	
	uint16_t dn_bonuslen;		
	uint8_t dn_bonustype;		
	uint8_t dn_nblkptr;		
	uint8_t dn_checksum;		
	uint8_t dn_compress;		
	uint8_t dn_nlevels;
	uint8_t dn_indblkshift;
	uint8_t dn_datablkshift;	
	uint8_t dn_moved;		
	uint16_t dn_datablkszsec;	
	uint32_t dn_datablksz;		
	uint64_t dn_maxblkid;
	uint8_t dn_next_type[TXG_SIZE];
	uint8_t dn_num_slots;		
	uint8_t dn_next_nblkptr[TXG_SIZE];
	uint8_t dn_next_nlevels[TXG_SIZE];
	uint8_t dn_next_indblkshift[TXG_SIZE];
	uint8_t dn_next_bonustype[TXG_SIZE];
	uint8_t dn_rm_spillblk[TXG_SIZE];	
	uint16_t dn_next_bonuslen[TXG_SIZE];
	uint32_t dn_next_blksz[TXG_SIZE];	
	uint64_t dn_next_maxblkid[TXG_SIZE];	

	
	uint32_t dn_dbufs_count;	

	
	multilist_node_t dn_dirty_link[TXG_SIZE]; 

	
	kmutex_t dn_mtx;
	list_t dn_dirty_records[TXG_SIZE];
	struct range_tree *dn_free_ranges[TXG_SIZE];
	uint64_t dn_allocated_txg;
	uint64_t dn_free_txg;
	uint64_t dn_assigned_txg;
	uint64_t dn_dirty_txg;			
	kcondvar_t dn_notxholds;
	kcondvar_t dn_nodnholds;
	enum dnode_dirtycontext dn_dirtyctx;
	const void *dn_dirtyctx_firstset;	

	
	zfs_refcount_t dn_tx_holds;
	zfs_refcount_t dn_holds;

	kmutex_t dn_dbufs_mtx;
	
	avl_tree_t dn_dbufs;

	
	struct dmu_buf_impl *dn_bonus;	

	boolean_t dn_have_spill;	

	
	zio_t *dn_zio;

	
	uint64_t dn_oldused;	
	uint64_t dn_oldflags;	
	uint64_t dn_olduid, dn_oldgid, dn_oldprojid;
	uint64_t dn_newuid, dn_newgid, dn_newprojid;
	int dn_id_flags;

	
	struct zfetch	dn_zfetch;
};


#define	DN_DBUFS_COUNT(dn)	((dn)->dn_dbufs_count + \
    avl_numnodes(&(dn)->dn_dbufs))


#define	DMU_NEXT_MAXBLKID_SET		(1ULL << 63)


typedef struct dnode_handle {
	
	zrlock_t dnh_zrlock;
	dnode_t *dnh_dnode;
} dnode_handle_t;

typedef struct dnode_children {
	dmu_buf_user_t dnc_dbu;		
	size_t dnc_count;		
	dnode_handle_t dnc_children[];	
} dnode_children_t;

typedef struct free_range {
	avl_node_t fr_node;
	uint64_t fr_blkid;
	uint64_t fr_nblks;
} free_range_t;

void dnode_special_open(struct objset *dd, dnode_phys_t *dnp,
    uint64_t object, dnode_handle_t *dnh);
void dnode_special_close(dnode_handle_t *dnh);

void dnode_setbonuslen(dnode_t *dn, int newsize, dmu_tx_t *tx);
void dnode_setbonus_type(dnode_t *dn, dmu_object_type_t, dmu_tx_t *tx);
void dnode_rm_spill(dnode_t *dn, dmu_tx_t *tx);

int dnode_hold(struct objset *dd, uint64_t object,
    const void *ref, dnode_t **dnp);
int dnode_hold_impl(struct objset *dd, uint64_t object, int flag, int dn_slots,
    const void *ref, dnode_t **dnp);
boolean_t dnode_add_ref(dnode_t *dn, const void *ref);
void dnode_rele(dnode_t *dn, const void *ref);
void dnode_rele_and_unlock(dnode_t *dn, const void *tag, boolean_t evicting);
int dnode_try_claim(objset_t *os, uint64_t object, int slots);
boolean_t dnode_is_dirty(dnode_t *dn);
void dnode_setdirty(dnode_t *dn, dmu_tx_t *tx);
void dnode_set_dirtyctx(dnode_t *dn, dmu_tx_t *tx, const void *tag);
void dnode_sync(dnode_t *dn, dmu_tx_t *tx);
void dnode_allocate(dnode_t *dn, dmu_object_type_t ot, int blocksize, int ibs,
    dmu_object_type_t bonustype, int bonuslen, int dn_slots, dmu_tx_t *tx);
void dnode_reallocate(dnode_t *dn, dmu_object_type_t ot, int blocksize,
    dmu_object_type_t bonustype, int bonuslen, int dn_slots,
    boolean_t keep_spill, dmu_tx_t *tx);
void dnode_free(dnode_t *dn, dmu_tx_t *tx);
void dnode_byteswap(dnode_phys_t *dnp);
void dnode_buf_byteswap(void *buf, size_t size);
void dnode_verify(dnode_t *dn);
int dnode_set_nlevels(dnode_t *dn, int nlevels, dmu_tx_t *tx);
int dnode_set_blksz(dnode_t *dn, uint64_t size, int ibs, dmu_tx_t *tx);
void dnode_free_range(dnode_t *dn, uint64_t off, uint64_t len, dmu_tx_t *tx);
void dnode_diduse_space(dnode_t *dn, int64_t space);
void dnode_new_blkid(dnode_t *dn, uint64_t blkid, dmu_tx_t *tx,
    boolean_t have_read, boolean_t force);
uint64_t dnode_block_freed(dnode_t *dn, uint64_t blkid);
void dnode_init(void);
void dnode_fini(void);
int dnode_next_offset(dnode_t *dn, int flags, uint64_t *off,
    int minlvl, uint64_t blkfill, uint64_t txg);
void dnode_evict_dbufs(dnode_t *dn);
void dnode_evict_bonus(dnode_t *dn);
void dnode_free_interior_slots(dnode_t *dn);

#define	DNODE_IS_DIRTY(_dn)						\
	((_dn)->dn_dirty_txg >= spa_syncing_txg((_dn)->dn_objset->os_spa))

#define	DNODE_LEVEL_IS_CACHEABLE(_dn, _level)				\
	((_dn)->dn_objset->os_primary_cache == ZFS_CACHE_ALL ||		\
	(((_level) > 0 || DMU_OT_IS_METADATA((_dn)->dn_type)) &&	\
	(_dn)->dn_objset->os_primary_cache == ZFS_CACHE_METADATA))


typedef struct dnode_stats {
	
	kstat_named_t dnode_hold_dbuf_hold;
	
	kstat_named_t dnode_hold_dbuf_read;
	
	kstat_named_t dnode_hold_alloc_hits;
	
	kstat_named_t dnode_hold_alloc_misses;
	
	kstat_named_t dnode_hold_alloc_interior;
	
	kstat_named_t dnode_hold_alloc_lock_retry;
	
	kstat_named_t dnode_hold_alloc_lock_misses;
	
	kstat_named_t dnode_hold_alloc_type_none;
	
	kstat_named_t dnode_hold_free_hits;
	
	kstat_named_t dnode_hold_free_misses;
	
	kstat_named_t dnode_hold_free_lock_misses;
	
	kstat_named_t dnode_hold_free_lock_retry;
	
	kstat_named_t dnode_hold_free_refcount;
	
	kstat_named_t dnode_hold_free_overflow;
	
	kstat_named_t dnode_free_interior_lock_retry;
	
	kstat_named_t dnode_allocate;
	
	kstat_named_t dnode_reallocate;
	
	kstat_named_t dnode_buf_evict;
	
	kstat_named_t dnode_alloc_next_chunk;
	
	kstat_named_t dnode_alloc_race;
	
	kstat_named_t dnode_alloc_next_block;
	
	kstat_named_t dnode_move_invalid;
	kstat_named_t dnode_move_recheck1;
	kstat_named_t dnode_move_recheck2;
	kstat_named_t dnode_move_special;
	kstat_named_t dnode_move_handle;
	kstat_named_t dnode_move_rwlock;
	kstat_named_t dnode_move_active;
} dnode_stats_t;

typedef struct dnode_sums {
	wmsum_t dnode_hold_dbuf_hold;
	wmsum_t dnode_hold_dbuf_read;
	wmsum_t dnode_hold_alloc_hits;
	wmsum_t dnode_hold_alloc_misses;
	wmsum_t dnode_hold_alloc_interior;
	wmsum_t dnode_hold_alloc_lock_retry;
	wmsum_t dnode_hold_alloc_lock_misses;
	wmsum_t dnode_hold_alloc_type_none;
	wmsum_t dnode_hold_free_hits;
	wmsum_t dnode_hold_free_misses;
	wmsum_t dnode_hold_free_lock_misses;
	wmsum_t dnode_hold_free_lock_retry;
	wmsum_t dnode_hold_free_refcount;
	wmsum_t dnode_hold_free_overflow;
	wmsum_t dnode_free_interior_lock_retry;
	wmsum_t dnode_allocate;
	wmsum_t dnode_reallocate;
	wmsum_t dnode_buf_evict;
	wmsum_t dnode_alloc_next_chunk;
	wmsum_t dnode_alloc_race;
	wmsum_t dnode_alloc_next_block;
	wmsum_t dnode_move_invalid;
	wmsum_t dnode_move_recheck1;
	wmsum_t dnode_move_recheck2;
	wmsum_t dnode_move_special;
	wmsum_t dnode_move_handle;
	wmsum_t dnode_move_rwlock;
	wmsum_t dnode_move_active;
} dnode_sums_t;

extern dnode_stats_t dnode_stats;
extern dnode_sums_t dnode_sums;

#define	DNODE_STAT_INCR(stat, val) \
    wmsum_add(&dnode_sums.stat, (val))
#define	DNODE_STAT_BUMP(stat) \
    DNODE_STAT_INCR(stat, 1);

#ifdef ZFS_DEBUG

#define	dprintf_dnode(dn, fmt, ...) do { \
	if (zfs_flags & ZFS_DEBUG_DPRINTF) { \
	char __db_buf[32]; \
	uint64_t __db_obj = (dn)->dn_object; \
	if (__db_obj == DMU_META_DNODE_OBJECT) \
		(void) strlcpy(__db_buf, "mdn", sizeof (__db_buf));	\
	else \
		(void) snprintf(__db_buf, sizeof (__db_buf), "%lld", \
		    (u_longlong_t)__db_obj);\
	dprintf_ds((dn)->dn_objset->os_dsl_dataset, "obj=%s " fmt, \
	    __db_buf, __VA_ARGS__); \
	} \
} while (0)

#define	DNODE_VERIFY(dn)		dnode_verify(dn)
#define	FREE_VERIFY(db, start, end, tx)	free_verify(db, start, end, tx)

#else

#define	dprintf_dnode(db, fmt, ...)
#define	DNODE_VERIFY(dn)		((void) sizeof ((uintptr_t)(dn)))
#define	FREE_VERIFY(db, start, end, tx)

#endif

#ifdef	__cplusplus
}
#endif

#endif	
