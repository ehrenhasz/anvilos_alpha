#ifndef	_SYS_ZAP_LEAF_H
#define	_SYS_ZAP_LEAF_H
#include <sys/zap.h>
#ifdef	__cplusplus
extern "C" {
#endif
struct zap;
struct zap_name;
struct zap_stats;
#define	ZAP_LEAF_MAGIC 0x2AB1EAF
#define	ZAP_LEAF_CHUNKSIZE 24
#define	ZAP_LEAF_NUMCHUNKS_BS(bs) \
	(((1<<(bs)) - 2*ZAP_LEAF_HASH_NUMENTRIES_BS(bs)) / \
	ZAP_LEAF_CHUNKSIZE - 2)
#define	ZAP_LEAF_NUMCHUNKS(l) (ZAP_LEAF_NUMCHUNKS_BS(((l)->l_bs)))
#define	ZAP_LEAF_NUMCHUNKS_DEF \
	(ZAP_LEAF_NUMCHUNKS_BS(fzap_default_block_shift))
#define	ZAP_LEAF_ARRAY_BYTES (ZAP_LEAF_CHUNKSIZE - 3)
#define	ZAP_LEAF_ARRAY_NCHUNKS(bytes) \
	(((bytes)+ZAP_LEAF_ARRAY_BYTES-1)/ZAP_LEAF_ARRAY_BYTES)
#define	ZAP_LEAF_LOW_WATER (20)
#define	ZAP_LEAF_HASH_SHIFT_BS(bs) ((bs) - 5)
#define	ZAP_LEAF_HASH_NUMENTRIES_BS(bs) (1 << ZAP_LEAF_HASH_SHIFT_BS(bs))
#define	ZAP_LEAF_HASH_SHIFT(l) (ZAP_LEAF_HASH_SHIFT_BS(((l)->l_bs)))
#define	ZAP_LEAF_HASH_NUMENTRIES(l) (ZAP_LEAF_HASH_NUMENTRIES_BS(((l)->l_bs)))
#define	ZAP_LEAF_CHUNK(l, idx) \
	((zap_leaf_chunk_t *) \
	(zap_leaf_phys(l)->l_hash + ZAP_LEAF_HASH_NUMENTRIES(l)))[idx]
#define	ZAP_LEAF_ENTRY(l, idx) (&ZAP_LEAF_CHUNK(l, idx).l_entry)
typedef enum zap_chunk_type {
	ZAP_CHUNK_FREE = 253,
	ZAP_CHUNK_ENTRY = 252,
	ZAP_CHUNK_ARRAY = 251,
	ZAP_CHUNK_TYPE_MAX = 250
} zap_chunk_type_t;
#define	ZLF_ENTRIES_CDSORTED (1<<0)
typedef struct zap_leaf_phys {
	struct zap_leaf_header {
		uint64_t lh_block_type;		 
		uint64_t lh_pad1;
		uint64_t lh_prefix;		 
		uint32_t lh_magic;		 
		uint16_t lh_nfree;		 
		uint16_t lh_nentries;		 
		uint16_t lh_prefix_len;		 
		uint16_t lh_freelist;		 
		uint8_t lh_flags;		 
		uint8_t lh_pad2[11];
	} l_hdr;  
	uint16_t l_hash[1];
} zap_leaf_phys_t;
typedef union zap_leaf_chunk {
	struct zap_leaf_entry {
		uint8_t le_type; 		 
		uint8_t le_value_intlen;	 
		uint16_t le_next;		 
		uint16_t le_name_chunk;		 
		uint16_t le_name_numints;	 
		uint16_t le_value_chunk;	 
		uint16_t le_value_numints;	 
		uint32_t le_cd;			 
		uint64_t le_hash;		 
	} l_entry;
	struct zap_leaf_array {
		uint8_t la_type;		 
		uint8_t la_array[ZAP_LEAF_ARRAY_BYTES];
		uint16_t la_next;		 
	} l_array;
	struct zap_leaf_free {
		uint8_t lf_type;		 
		uint8_t lf_pad[ZAP_LEAF_ARRAY_BYTES];
		uint16_t lf_next;	 
	} l_free;
} zap_leaf_chunk_t;
typedef struct zap_leaf {
	dmu_buf_user_t l_dbu;
	krwlock_t l_rwlock;
	uint64_t l_blkid;		 
	int l_bs;			 
	dmu_buf_t *l_dbuf;
} zap_leaf_t;
static inline zap_leaf_phys_t *
zap_leaf_phys(zap_leaf_t *l)
{
	return (l->l_dbuf->db_data);
}
typedef struct zap_entry_handle {
	uint64_t zeh_num_integers;
	uint64_t zeh_hash;
	uint32_t zeh_cd;
	uint8_t zeh_integer_size;
	uint16_t zeh_fakechunk;
	uint16_t *zeh_chunkp;
	zap_leaf_t *zeh_leaf;
} zap_entry_handle_t;
extern int zap_leaf_lookup(zap_leaf_t *l,
    struct zap_name *zn, zap_entry_handle_t *zeh);
extern int zap_leaf_lookup_closest(zap_leaf_t *l,
    uint64_t hash, uint32_t cd, zap_entry_handle_t *zeh);
extern int zap_entry_read(const zap_entry_handle_t *zeh,
    uint8_t integer_size, uint64_t num_integers, void *buf);
extern int zap_entry_read_name(struct zap *zap, const zap_entry_handle_t *zeh,
    uint16_t buflen, char *buf);
extern int zap_entry_update(zap_entry_handle_t *zeh,
    uint8_t integer_size, uint64_t num_integers, const void *buf);
extern void zap_entry_remove(zap_entry_handle_t *zeh);
extern int zap_entry_create(zap_leaf_t *l, struct zap_name *zn, uint32_t cd,
    uint8_t integer_size, uint64_t num_integers, const void *buf,
    zap_entry_handle_t *zeh);
extern boolean_t zap_entry_normalization_conflict(zap_entry_handle_t *zeh,
    struct zap_name *zn, const char *name, struct zap *zap);
extern void zap_leaf_init(zap_leaf_t *l, boolean_t sort);
extern void zap_leaf_byteswap(zap_leaf_phys_t *buf, int len);
extern void zap_leaf_split(zap_leaf_t *l, zap_leaf_t *nl, boolean_t sort);
extern void zap_leaf_stats(struct zap *zap, zap_leaf_t *l,
    struct zap_stats *zs);
#ifdef	__cplusplus
}
#endif
#endif  
