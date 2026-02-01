 
 

#ifndef	_SYS_DBUF_H
#define	_SYS_DBUF_H

#include <sys/dmu.h>
#include <sys/spa.h>
#include <sys/txg.h>
#include <sys/zio.h>
#include <sys/arc.h>
#include <sys/zfs_context.h>
#include <sys/zfs_refcount.h>
#include <sys/zrlock.h>
#include <sys/multilist.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	IN_DMU_SYNC 2

 

#define	DB_RF_MUST_SUCCEED	(1 << 0)
#define	DB_RF_CANFAIL		(1 << 1)
#define	DB_RF_HAVESTRUCT	(1 << 2)
#define	DB_RF_NOPREFETCH	(1 << 3)
#define	DB_RF_NEVERWAIT		(1 << 4)
#define	DB_RF_CACHED		(1 << 5)
#define	DB_RF_NO_DECRYPT	(1 << 6)
#define	DB_RF_PARTIAL_FIRST	(1 << 7)
#define	DB_RF_PARTIAL_MORE	(1 << 8)

 
typedef enum dbuf_states {
	DB_SEARCH = -1,
	DB_UNCACHED,
	DB_FILL,
	DB_NOFILL,
	DB_READ,
	DB_CACHED,
	DB_EVICTING
} dbuf_states_t;

typedef enum dbuf_cached_state {
	DB_NO_CACHE = -1,
	DB_DBUF_CACHE,
	DB_DBUF_METADATA_CACHE,
	DB_CACHE_MAX
} dbuf_cached_state_t;

struct dnode;
struct dmu_tx;

 

struct dmu_buf_impl;

typedef enum override_states {
	DR_NOT_OVERRIDDEN,
	DR_IN_DMU_SYNC,
	DR_OVERRIDDEN
} override_states_t;

typedef enum db_lock_type {
	DLT_NONE,
	DLT_PARENT,
	DLT_OBJSET
} db_lock_type_t;

typedef struct dbuf_dirty_record {
	 
	list_node_t dr_dirty_node;

	 
	uint64_t dr_txg;

	 
	zio_t *dr_zio;

	 
	struct dmu_buf_impl *dr_dbuf;

	 
	list_node_t dr_dbuf_node;

	 
	dnode_t *dr_dnode;

	 
	struct dbuf_dirty_record *dr_parent;

	 
	unsigned int dr_accounted;

	 
	blkptr_t dr_bp_copy;

	union dirty_types {
		struct dirty_indirect {

			 
			kmutex_t dr_mtx;

			 
			list_t dr_children;
		} di;
		struct dirty_leaf {

			 
			arc_buf_t *dr_data;
			blkptr_t dr_overridden_by;
			override_states_t dr_override_state;
			uint8_t dr_copies;
			boolean_t dr_nopwrite;
			boolean_t dr_brtwrite;
			boolean_t dr_has_raw_params;

			 
			boolean_t dr_byteorder;
			uint8_t	dr_salt[ZIO_DATA_SALT_LEN];
			uint8_t	dr_iv[ZIO_DATA_IV_LEN];
			uint8_t	dr_mac[ZIO_DATA_MAC_LEN];
		} dl;
		struct dirty_lightweight_leaf {
			 
			uint64_t dr_blkid;
			abd_t *dr_abd;
			zio_prop_t dr_props;
			zio_flag_t dr_flags;
		} dll;
	} dt;
} dbuf_dirty_record_t;

typedef struct dmu_buf_impl {
	 

	 
	dmu_buf_t db;

	 
	struct objset *db_objset;

	 
	struct dnode_handle *db_dnode_handle;

	 
	struct dmu_buf_impl *db_parent;

	 
	struct dmu_buf_impl *db_hash_next;

	 
	avl_node_t db_link;

	 
	uint64_t db_blkid;

	 
	blkptr_t *db_blkptr;

	 
	uint8_t db_level;

	 
	krwlock_t db_rwlock;

	 
	arc_buf_t *db_buf;

	 
	kmutex_t db_mtx;

	 
	dbuf_states_t db_state;

	 
	zfs_refcount_t db_holds;

	kcondvar_t db_changed;
	dbuf_dirty_record_t *db_data_pending;

	 
	list_t db_dirty_records;

	 
	multilist_node_t db_cache_link;

	 
	dbuf_cached_state_t db_caching_status;

	uint64_t db_hash;

	 

	 
	dmu_buf_user_t *db_user;

	 
	uint8_t db_user_immediate_evict;

	 
	uint8_t db_freed_in_flight;

	 
	uint8_t db_pending_evict;

	uint8_t db_dirtycnt;

	 
	uint8_t db_partial_read;
} dmu_buf_impl_t;

#define	DBUF_HASH_MUTEX(h, idx) \
	(&(h)->hash_mutexes[(idx) & ((h)->hash_mutex_mask)])

typedef struct dbuf_hash_table {
	uint64_t hash_table_mask;
	uint64_t hash_mutex_mask;
	dmu_buf_impl_t **hash_table;
	kmutex_t *hash_mutexes;
} dbuf_hash_table_t;

typedef void (*dbuf_prefetch_fn)(void *, uint64_t, uint64_t, boolean_t);

uint64_t dbuf_whichblock(const struct dnode *di, const int64_t level,
    const uint64_t offset);

void dbuf_create_bonus(struct dnode *dn);
int dbuf_spill_set_blksz(dmu_buf_t *db, uint64_t blksz, dmu_tx_t *tx);

void dbuf_rm_spill(struct dnode *dn, dmu_tx_t *tx);

dmu_buf_impl_t *dbuf_hold(struct dnode *dn, uint64_t blkid, const void *tag);
dmu_buf_impl_t *dbuf_hold_level(struct dnode *dn, int level, uint64_t blkid,
    const void *tag);
int dbuf_hold_impl(struct dnode *dn, uint8_t level, uint64_t blkid,
    boolean_t fail_sparse, boolean_t fail_uncached,
    const void *tag, dmu_buf_impl_t **dbp);

int dbuf_prefetch_impl(struct dnode *dn, int64_t level, uint64_t blkid,
    zio_priority_t prio, arc_flags_t aflags, dbuf_prefetch_fn cb,
    void *arg);
int dbuf_prefetch(struct dnode *dn, int64_t level, uint64_t blkid,
    zio_priority_t prio, arc_flags_t aflags);

void dbuf_add_ref(dmu_buf_impl_t *db, const void *tag);
boolean_t dbuf_try_add_ref(dmu_buf_t *db, objset_t *os, uint64_t obj,
    uint64_t blkid, const void *tag);
uint64_t dbuf_refcount(dmu_buf_impl_t *db);

void dbuf_rele(dmu_buf_impl_t *db, const void *tag);
void dbuf_rele_and_unlock(dmu_buf_impl_t *db, const void *tag,
    boolean_t evicting);

dmu_buf_impl_t *dbuf_find(struct objset *os, uint64_t object, uint8_t level,
    uint64_t blkid, uint64_t *hash_out);

int dbuf_read(dmu_buf_impl_t *db, zio_t *zio, uint32_t flags);
void dmu_buf_will_clone(dmu_buf_t *db, dmu_tx_t *tx);
void dmu_buf_will_not_fill(dmu_buf_t *db, dmu_tx_t *tx);
void dmu_buf_will_fill(dmu_buf_t *db, dmu_tx_t *tx);
void dmu_buf_fill_done(dmu_buf_t *db, dmu_tx_t *tx);
void dbuf_assign_arcbuf(dmu_buf_impl_t *db, arc_buf_t *buf, dmu_tx_t *tx);
dbuf_dirty_record_t *dbuf_dirty(dmu_buf_impl_t *db, dmu_tx_t *tx);
dbuf_dirty_record_t *dbuf_dirty_lightweight(dnode_t *dn, uint64_t blkid,
    dmu_tx_t *tx);
boolean_t dbuf_undirty(dmu_buf_impl_t *db, dmu_tx_t *tx);
arc_buf_t *dbuf_loan_arcbuf(dmu_buf_impl_t *db);
void dmu_buf_write_embedded(dmu_buf_t *dbuf, void *data,
    bp_embedded_type_t etype, enum zio_compress comp,
    int uncompressed_size, int compressed_size, int byteorder, dmu_tx_t *tx);

int dmu_lightweight_write_by_dnode(dnode_t *dn, uint64_t offset, abd_t *abd,
    const struct zio_prop *zp, zio_flag_t flags, dmu_tx_t *tx);

void dmu_buf_redact(dmu_buf_t *dbuf, dmu_tx_t *tx);
void dbuf_destroy(dmu_buf_impl_t *db);

void dbuf_unoverride(dbuf_dirty_record_t *dr);
void dbuf_sync_list(list_t *list, int level, dmu_tx_t *tx);
void dbuf_release_bp(dmu_buf_impl_t *db);
db_lock_type_t dmu_buf_lock_parent(dmu_buf_impl_t *db, krw_t rw,
    const void *tag);
void dmu_buf_unlock_parent(dmu_buf_impl_t *db, db_lock_type_t type,
    const void *tag);

void dbuf_free_range(struct dnode *dn, uint64_t start, uint64_t end,
    struct dmu_tx *);

void dbuf_new_size(dmu_buf_impl_t *db, int size, dmu_tx_t *tx);

void dbuf_stats_init(dbuf_hash_table_t *hash);
void dbuf_stats_destroy(void);

int dbuf_dnode_findbp(dnode_t *dn, uint64_t level, uint64_t blkid,
    blkptr_t *bp, uint16_t *datablkszsec, uint8_t *indblkshift);

#define	DB_DNODE(_db)		((_db)->db_dnode_handle->dnh_dnode)
#define	DB_DNODE_LOCK(_db)	((_db)->db_dnode_handle->dnh_zrlock)
#define	DB_DNODE_ENTER(_db)	(zrl_add(&DB_DNODE_LOCK(_db)))
#define	DB_DNODE_EXIT(_db)	(zrl_remove(&DB_DNODE_LOCK(_db)))
#define	DB_DNODE_HELD(_db)	(!zrl_is_zero(&DB_DNODE_LOCK(_db)))

void dbuf_init(void);
void dbuf_fini(void);

boolean_t dbuf_is_metadata(dmu_buf_impl_t *db);

static inline dbuf_dirty_record_t *
dbuf_find_dirty_lte(dmu_buf_impl_t *db, uint64_t txg)
{
	dbuf_dirty_record_t *dr;

	for (dr = list_head(&db->db_dirty_records);
	    dr != NULL && dr->dr_txg > txg;
	    dr = list_next(&db->db_dirty_records, dr))
		continue;
	return (dr);
}

static inline dbuf_dirty_record_t *
dbuf_find_dirty_eq(dmu_buf_impl_t *db, uint64_t txg)
{
	dbuf_dirty_record_t *dr;

	dr = dbuf_find_dirty_lte(db, txg);
	if (dr && dr->dr_txg == txg)
		return (dr);
	return (NULL);
}

#define	DBUF_GET_BUFC_TYPE(_db)	\
	(dbuf_is_metadata(_db) ? ARC_BUFC_METADATA : ARC_BUFC_DATA)

#define	DBUF_IS_CACHEABLE(_db)						\
	((_db)->db_objset->os_primary_cache == ZFS_CACHE_ALL ||		\
	(dbuf_is_metadata(_db) &&					\
	((_db)->db_objset->os_primary_cache == ZFS_CACHE_METADATA)))

boolean_t dbuf_is_l2cacheable(dmu_buf_impl_t *db);

#ifdef ZFS_DEBUG

 
#define	dprintf_dbuf(dbuf, fmt, ...) do { \
	if (zfs_flags & ZFS_DEBUG_DPRINTF) { \
	char __db_buf[32]; \
	uint64_t __db_obj = (dbuf)->db.db_object; \
	if (__db_obj == DMU_META_DNODE_OBJECT) \
		(void) strlcpy(__db_buf, "mdn", sizeof (__db_buf));	\
	else \
		(void) snprintf(__db_buf, sizeof (__db_buf), "%lld", \
		    (u_longlong_t)__db_obj); \
	dprintf_ds((dbuf)->db_objset->os_dsl_dataset, \
	    "obj=%s lvl=%u blkid=%lld " fmt, \
	    __db_buf, (dbuf)->db_level, \
	    (u_longlong_t)(dbuf)->db_blkid, __VA_ARGS__); \
	} \
} while (0)

#define	dprintf_dbuf_bp(db, bp, fmt, ...) do {			\
	if (zfs_flags & ZFS_DEBUG_DPRINTF) {			\
	char *__blkbuf = kmem_alloc(BP_SPRINTF_LEN, KM_SLEEP);	\
	snprintf_blkptr(__blkbuf, BP_SPRINTF_LEN, bp);		\
	dprintf_dbuf(db, fmt " %s\n", __VA_ARGS__, __blkbuf);	\
	kmem_free(__blkbuf, BP_SPRINTF_LEN);			\
	}							\
} while (0)

#define	DBUF_VERIFY(db)	dbuf_verify(db)

#else

#define	dprintf_dbuf(db, fmt, ...)
#define	dprintf_dbuf_bp(db, bp, fmt, ...)
#define	DBUF_VERIFY(db)

#endif


#ifdef	__cplusplus
}
#endif

#endif  
