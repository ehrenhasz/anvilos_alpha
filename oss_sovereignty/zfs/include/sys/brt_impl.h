#ifndef _SYS_BRT_IMPL_H
#define	_SYS_BRT_IMPL_H
#ifdef	__cplusplus
extern "C" {
#endif
#define	BRT_OBJECT_VDEV_PREFIX	"com.fudosecurity:brt:vdev:"
#define	BRT_RANGESIZE	(16 * 1024 * 1024)
_Static_assert(BRT_RANGESIZE / SPA_MINBLOCKSIZE <= UINT16_MAX,
	"BRT_RANGESIZE is too large.");
#define	BRT_BLOCKSIZE	(32 * 1024)
#define	BRT_RANGESIZE_TO_NBLOCKS(size)					\
	(((size) - 1) / BRT_BLOCKSIZE / sizeof (uint16_t) + 1)
#define	BRT_LITTLE_ENDIAN	0
#define	BRT_BIG_ENDIAN		1
#ifdef _ZFS_LITTLE_ENDIAN
#define	BRT_NATIVE_BYTEORDER		BRT_LITTLE_ENDIAN
#define	BRT_NON_NATIVE_BYTEORDER	BRT_BIG_ENDIAN
#else
#define	BRT_NATIVE_BYTEORDER		BRT_BIG_ENDIAN
#define	BRT_NON_NATIVE_BYTEORDER	BRT_LITTLE_ENDIAN
#endif
typedef struct brt_vdev_phys {
	uint64_t	bvp_mos_entries;
	uint64_t	bvp_size;
	uint64_t	bvp_byteorder;
	uint64_t	bvp_totalcount;
	uint64_t	bvp_rangesize;
	uint64_t	bvp_usedspace;
	uint64_t	bvp_savedspace;
} brt_vdev_phys_t;
typedef struct brt_vdev {
	uint64_t	bv_vdevid;
	boolean_t	bv_initiated;
	uint64_t	bv_mos_brtvdev;
	uint64_t	bv_mos_entries;
	avl_tree_t	bv_tree;
	boolean_t	bv_need_byteswap;
	uint64_t	bv_size;
	uint16_t	*bv_entcount;
	uint64_t	bv_totalcount;
	uint64_t	bv_usedspace;
	uint64_t	bv_savedspace;
	boolean_t	bv_meta_dirty;
	boolean_t	bv_entcount_dirty;
	ulong_t		*bv_bitmap;
	uint64_t	bv_nblocks;
} brt_vdev_t;
typedef struct brt {
	krwlock_t	brt_lock;
	spa_t		*brt_spa;
#define	brt_mos		brt_spa->spa_meta_objset
	uint64_t	brt_rangesize;
	uint64_t	brt_usedspace;
	uint64_t	brt_savedspace;
	avl_tree_t	brt_pending_tree[TXG_SIZE];
	kmutex_t	brt_pending_lock[TXG_SIZE];
	uint64_t	brt_nentries;
	brt_vdev_t	*brt_vdevs;
	uint64_t	brt_nvdevs;
} brt_t;
#define	BRT_KEY_WORDS	(1)
typedef struct brt_entry {
	uint64_t	bre_offset;
	uint64_t	bre_refcount;
	avl_node_t	bre_node;
} brt_entry_t;
typedef struct brt_pending_entry {
	blkptr_t	bpe_bp;
	int		bpe_count;
	avl_node_t	bpe_node;
} brt_pending_entry_t;
#ifdef	__cplusplus
}
#endif
#endif	 
