 
 

#ifndef	_BTREE_H
#define	_BTREE_H

#ifdef	__cplusplus
extern "C" {
#endif

#include	<sys/zfs_context.h>

 

 
#define	BTREE_CORE_ELEMS	126
#define	BTREE_LEAF_SIZE		4096

extern kmem_cache_t *zfs_btree_leaf_cache;

typedef struct zfs_btree_hdr {
	struct zfs_btree_core	*bth_parent;
	 
	uint32_t		bth_first;
	 
	uint32_t		bth_count;
} zfs_btree_hdr_t;

typedef struct zfs_btree_core {
	zfs_btree_hdr_t	btc_hdr;
	zfs_btree_hdr_t	*btc_children[BTREE_CORE_ELEMS + 1];
	uint8_t		btc_elems[];
} zfs_btree_core_t;

typedef struct zfs_btree_leaf {
	zfs_btree_hdr_t	btl_hdr;
	uint8_t		btl_elems[];
} zfs_btree_leaf_t;

typedef struct zfs_btree_index {
	zfs_btree_hdr_t	*bti_node;
	uint32_t	bti_offset;
	 
	boolean_t	bti_before;
} zfs_btree_index_t;

typedef struct btree zfs_btree_t;
typedef void * (*bt_find_in_buf_f) (zfs_btree_t *, uint8_t *, uint32_t,
    const void *, zfs_btree_index_t *);

struct btree {
	int (*bt_compar) (const void *, const void *);
	bt_find_in_buf_f	bt_find_in_buf;
	size_t			bt_elem_size;
	size_t			bt_leaf_size;
	uint32_t		bt_leaf_cap;
	int32_t			bt_height;
	uint64_t		bt_num_elems;
	uint64_t		bt_num_nodes;
	zfs_btree_hdr_t		*bt_root;
	zfs_btree_leaf_t	*bt_bulk;  
};

 
 
#define	ZFS_BTREE_FIND_IN_BUF_FUNC(NAME, T, COMP)			\
_Pragma("GCC diagnostic push")						\
_Pragma("GCC diagnostic ignored \"-Wunknown-pragmas\"")			\
static void *								\
NAME(zfs_btree_t *tree, uint8_t *buf, uint32_t nelems,			\
    const void *value, zfs_btree_index_t *where)			\
{									\
	T *i = (T *)buf;						\
	(void) tree;							\
	_Pragma("GCC unroll 9")						\
	while (nelems > 1) {						\
		uint32_t half = nelems / 2;				\
		nelems -= half;						\
		i += (COMP(&i[half - 1], value) < 0) * half;		\
	}								\
									\
	int comp = COMP(i, value);					\
	where->bti_offset = (i - (T *)buf) + (comp < 0);		\
	where->bti_before = (comp != 0);				\
									\
	if (comp == 0) {						\
		return (i);						\
	}								\
									\
	return (NULL);							\
}									\
_Pragma("GCC diagnostic pop")
 

 
void zfs_btree_init(void);
void zfs_btree_fini(void);

 
void zfs_btree_create(zfs_btree_t *, int (*) (const void *, const void *),
    bt_find_in_buf_f, size_t);
void zfs_btree_create_custom(zfs_btree_t *, int (*)(const void *, const void *),
    bt_find_in_buf_f, size_t, size_t);

 
void *zfs_btree_find(zfs_btree_t *, const void *, zfs_btree_index_t *);

 
void zfs_btree_add_idx(zfs_btree_t *, const void *, const zfs_btree_index_t *);

 
void *zfs_btree_first(zfs_btree_t *, zfs_btree_index_t *);
void *zfs_btree_last(zfs_btree_t *, zfs_btree_index_t *);

 
void *zfs_btree_next(zfs_btree_t *, const zfs_btree_index_t *,
    zfs_btree_index_t *);
void *zfs_btree_prev(zfs_btree_t *, const zfs_btree_index_t *,
    zfs_btree_index_t *);

 
void *zfs_btree_get(zfs_btree_t *, zfs_btree_index_t *);

 
void zfs_btree_add(zfs_btree_t *, const void *);

 
void zfs_btree_remove(zfs_btree_t *, const void *);

 
void zfs_btree_remove_idx(zfs_btree_t *, zfs_btree_index_t *);

 
ulong_t zfs_btree_numnodes(zfs_btree_t *);

 
void *zfs_btree_destroy_nodes(zfs_btree_t *, zfs_btree_index_t **);

 
void zfs_btree_clear(zfs_btree_t *);

 
void zfs_btree_destroy(zfs_btree_t *tree);

 
void zfs_btree_verify(zfs_btree_t *tree);

#ifdef	__cplusplus
}
#endif

#endif	 
