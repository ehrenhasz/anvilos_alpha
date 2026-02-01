 

 

#ifndef	_SYS_ZAP_H
#define	_SYS_ZAP_H

 

 

#include <sys/dmu.h>

#ifdef	__cplusplus
extern "C" {
#endif

 
typedef enum matchtype {
	MT_NORMALIZE = 1 << 0,
	MT_MATCH_CASE = 1 << 1,
} matchtype_t;

typedef enum zap_flags {
	 
	ZAP_FLAG_HASH64 = 1 << 0,
	 
	ZAP_FLAG_UINT64_KEY = 1 << 1,
	 
	ZAP_FLAG_PRE_HASHED_KEY = 1 << 2,
#if defined(__linux__) && defined(_KERNEL)
} zfs_zap_flags_t;
#define	zap_flags_t	zfs_zap_flags_t
#else
} zap_flags_t;
#endif

 
uint64_t zap_create(objset_t *ds, dmu_object_type_t ot,
    dmu_object_type_t bonustype, int bonuslen, dmu_tx_t *tx);
uint64_t zap_create_dnsize(objset_t *ds, dmu_object_type_t ot,
    dmu_object_type_t bonustype, int bonuslen, int dnodesize, dmu_tx_t *tx);
uint64_t zap_create_norm(objset_t *ds, int normflags, dmu_object_type_t ot,
    dmu_object_type_t bonustype, int bonuslen, dmu_tx_t *tx);
uint64_t zap_create_norm_dnsize(objset_t *ds, int normflags,
    dmu_object_type_t ot, dmu_object_type_t bonustype, int bonuslen,
    int dnodesize, dmu_tx_t *tx);
uint64_t zap_create_flags(objset_t *os, int normflags, zap_flags_t flags,
    dmu_object_type_t ot, int leaf_blockshift, int indirect_blockshift,
    dmu_object_type_t bonustype, int bonuslen, dmu_tx_t *tx);
uint64_t zap_create_flags_dnsize(objset_t *os, int normflags,
    zap_flags_t flags, dmu_object_type_t ot, int leaf_blockshift,
    int indirect_blockshift, dmu_object_type_t bonustype, int bonuslen,
    int dnodesize, dmu_tx_t *tx);
uint64_t zap_create_hold(objset_t *os, int normflags, zap_flags_t flags,
    dmu_object_type_t ot, int leaf_blockshift, int indirect_blockshift,
    dmu_object_type_t bonustype, int bonuslen, int dnodesize,
    dnode_t **allocated_dnode, const void *tag, dmu_tx_t *tx);

uint64_t zap_create_link(objset_t *os, dmu_object_type_t ot,
    uint64_t parent_obj, const char *name, dmu_tx_t *tx);
uint64_t zap_create_link_dnsize(objset_t *os, dmu_object_type_t ot,
    uint64_t parent_obj, const char *name, int dnodesize, dmu_tx_t *tx);

 
void mzap_create_impl(dnode_t *dn, int normflags, zap_flags_t flags,
    dmu_tx_t *tx);

 
int zap_create_claim(objset_t *ds, uint64_t obj, dmu_object_type_t ot,
    dmu_object_type_t bonustype, int bonuslen, dmu_tx_t *tx);
int zap_create_claim_dnsize(objset_t *ds, uint64_t obj, dmu_object_type_t ot,
    dmu_object_type_t bonustype, int bonuslen, int dnodesize, dmu_tx_t *tx);
int zap_create_claim_norm(objset_t *ds, uint64_t obj,
    int normflags, dmu_object_type_t ot,
    dmu_object_type_t bonustype, int bonuslen, dmu_tx_t *tx);
int zap_create_claim_norm_dnsize(objset_t *ds, uint64_t obj,
    int normflags, dmu_object_type_t ot,
    dmu_object_type_t bonustype, int bonuslen, int dnodesize, dmu_tx_t *tx);

 

 
int zap_destroy(objset_t *ds, uint64_t zapobj, dmu_tx_t *tx);

 

 
int zap_lookup(objset_t *ds, uint64_t zapobj, const char *name,
    uint64_t integer_size, uint64_t num_integers, void *buf);

 
int zap_lookup_norm(objset_t *ds, uint64_t zapobj, const char *name,
    uint64_t integer_size, uint64_t num_integers, void *buf,
    matchtype_t mt, char *realname, int rn_len,
    boolean_t *normalization_conflictp);
int zap_lookup_uint64(objset_t *os, uint64_t zapobj, const uint64_t *key,
    int key_numints, uint64_t integer_size, uint64_t num_integers, void *buf);
int zap_contains(objset_t *ds, uint64_t zapobj, const char *name);
int zap_prefetch(objset_t *os, uint64_t zapobj, const char *name);
int zap_prefetch_uint64(objset_t *os, uint64_t zapobj, const uint64_t *key,
    int key_numints);

int zap_lookup_by_dnode(dnode_t *dn, const char *name,
    uint64_t integer_size, uint64_t num_integers, void *buf);
int zap_lookup_norm_by_dnode(dnode_t *dn, const char *name,
    uint64_t integer_size, uint64_t num_integers, void *buf,
    matchtype_t mt, char *realname, int rn_len,
    boolean_t *ncp);

int zap_count_write_by_dnode(dnode_t *dn, const char *name,
    int add, zfs_refcount_t *towrite, zfs_refcount_t *tooverwrite);

 
int zap_add(objset_t *ds, uint64_t zapobj, const char *key,
    int integer_size, uint64_t num_integers,
    const void *val, dmu_tx_t *tx);
int zap_add_by_dnode(dnode_t *dn, const char *key,
    int integer_size, uint64_t num_integers,
    const void *val, dmu_tx_t *tx);
int zap_add_uint64(objset_t *ds, uint64_t zapobj, const uint64_t *key,
    int key_numints, int integer_size, uint64_t num_integers,
    const void *val, dmu_tx_t *tx);

 
int zap_update(objset_t *ds, uint64_t zapobj, const char *name,
    int integer_size, uint64_t num_integers, const void *val, dmu_tx_t *tx);
int zap_update_uint64(objset_t *os, uint64_t zapobj, const uint64_t *key,
    int key_numints,
    int integer_size, uint64_t num_integers, const void *val, dmu_tx_t *tx);

 
int zap_length(objset_t *ds, uint64_t zapobj, const char *name,
    uint64_t *integer_size, uint64_t *num_integers);
int zap_length_uint64(objset_t *os, uint64_t zapobj, const uint64_t *key,
    int key_numints, uint64_t *integer_size, uint64_t *num_integers);

 
int zap_remove(objset_t *ds, uint64_t zapobj, const char *name, dmu_tx_t *tx);
int zap_remove_norm(objset_t *ds, uint64_t zapobj, const char *name,
    matchtype_t mt, dmu_tx_t *tx);
int zap_remove_by_dnode(dnode_t *dn, const char *name, dmu_tx_t *tx);
int zap_remove_uint64(objset_t *os, uint64_t zapobj, const uint64_t *key,
    int key_numints, dmu_tx_t *tx);

 
int zap_count(objset_t *ds, uint64_t zapobj, uint64_t *count);

 
int zap_value_search(objset_t *os, uint64_t zapobj,
    uint64_t value, uint64_t mask, char *name);

 
int zap_join(objset_t *os, uint64_t fromobj, uint64_t intoobj, dmu_tx_t *tx);

 
int zap_join_key(objset_t *os, uint64_t fromobj, uint64_t intoobj,
    uint64_t value, dmu_tx_t *tx);

 
int zap_join_increment(objset_t *os, uint64_t fromobj, uint64_t intoobj,
    dmu_tx_t *tx);

 
int zap_add_int(objset_t *os, uint64_t obj, uint64_t value, dmu_tx_t *tx);
int zap_remove_int(objset_t *os, uint64_t obj, uint64_t value, dmu_tx_t *tx);
int zap_lookup_int(objset_t *os, uint64_t obj, uint64_t value);
int zap_increment_int(objset_t *os, uint64_t obj, uint64_t key, int64_t delta,
    dmu_tx_t *tx);

 
int zap_add_int_key(objset_t *os, uint64_t obj,
    uint64_t key, uint64_t value, dmu_tx_t *tx);
int zap_update_int_key(objset_t *os, uint64_t obj,
    uint64_t key, uint64_t value, dmu_tx_t *tx);
int zap_lookup_int_key(objset_t *os, uint64_t obj,
    uint64_t key, uint64_t *valuep);

int zap_increment(objset_t *os, uint64_t obj, const char *name, int64_t delta,
    dmu_tx_t *tx);

struct zap;
struct zap_leaf;
typedef struct zap_cursor {
	 
	objset_t *zc_objset;
	struct zap *zc_zap;
	struct zap_leaf *zc_leaf;
	uint64_t zc_zapobj;
	uint64_t zc_serialized;
	uint64_t zc_hash;
	uint32_t zc_cd;
	boolean_t zc_prefetch;
} zap_cursor_t;

typedef struct {
	int za_integer_length;
	 
	boolean_t za_normalization_conflict;
	uint64_t za_num_integers;
	uint64_t za_first_integer;	 
	char za_name[ZAP_MAXNAMELEN];
} zap_attribute_t;

 

 
void zap_cursor_init(zap_cursor_t *zc, objset_t *os, uint64_t zapobj);
void zap_cursor_init_noprefetch(zap_cursor_t *zc, objset_t *os,
    uint64_t zapobj);
void zap_cursor_fini(zap_cursor_t *zc);

 
int zap_cursor_retrieve(zap_cursor_t *zc, zap_attribute_t *za);

 
void zap_cursor_advance(zap_cursor_t *zc);

 
uint64_t zap_cursor_serialize(zap_cursor_t *zc);

 
void zap_cursor_init_serialized(zap_cursor_t *zc, objset_t *ds,
    uint64_t zapobj, uint64_t serialized);


#define	ZAP_HISTOGRAM_SIZE 10

typedef struct zap_stats {
	 
	uint64_t zs_ptrtbl_len;

	uint64_t zs_blocksize;		 

	 
	uint64_t zs_num_blocks;

	 
	uint64_t zs_ptrtbl_nextblk;	   
	uint64_t zs_ptrtbl_blks_copied;    
	uint64_t zs_ptrtbl_zt_blk;	   
	uint64_t zs_ptrtbl_zt_numblks;     
	uint64_t zs_ptrtbl_zt_shift;	   

	 
	uint64_t zs_block_type;		 
	uint64_t zs_magic;		 
	uint64_t zs_num_leafs;		 
	uint64_t zs_num_entries;	 
	uint64_t zs_salt;		 

	 

	 
	uint64_t zs_leafs_with_2n_pointers[ZAP_HISTOGRAM_SIZE];

	 
	uint64_t zs_blocks_with_n5_entries[ZAP_HISTOGRAM_SIZE];

	 
	uint64_t zs_blocks_n_tenths_full[ZAP_HISTOGRAM_SIZE];

	 
	uint64_t zs_entries_using_n_chunks[ZAP_HISTOGRAM_SIZE];

	 
	uint64_t zs_buckets_with_n_entries[ZAP_HISTOGRAM_SIZE];
} zap_stats_t;

 
int zap_get_stats(objset_t *ds, uint64_t zapobj, zap_stats_t *zs);

#ifdef	__cplusplus
}
#endif

#endif	 
