#ifndef	_SYS_DMU_H
#define	_SYS_DMU_H
#include <sys/zfs_context.h>
#include <sys/inttypes.h>
#include <sys/cred.h>
#include <sys/fs/zfs.h>
#include <sys/zio_compress.h>
#include <sys/zio_priority.h>
#include <sys/uio.h>
#include <sys/zfs_file.h>
#ifdef	__cplusplus
extern "C" {
#endif
struct page;
struct vnode;
struct spa;
struct zilog;
struct zio;
struct blkptr;
struct zap_cursor;
struct dsl_dataset;
struct dsl_pool;
struct dnode;
struct drr_begin;
struct drr_end;
struct zbookmark_phys;
struct spa;
struct nvlist;
struct arc_buf;
struct zio_prop;
struct sa_handle;
struct dsl_crypto_params;
struct locked_range;
typedef struct objset objset_t;
typedef struct dmu_tx dmu_tx_t;
typedef struct dsl_dir dsl_dir_t;
typedef struct dnode dnode_t;
typedef enum dmu_object_byteswap {
	DMU_BSWAP_UINT8,
	DMU_BSWAP_UINT16,
	DMU_BSWAP_UINT32,
	DMU_BSWAP_UINT64,
	DMU_BSWAP_ZAP,
	DMU_BSWAP_DNODE,
	DMU_BSWAP_OBJSET,
	DMU_BSWAP_ZNODE,
	DMU_BSWAP_OLDACL,
	DMU_BSWAP_ACL,
	DMU_BSWAP_NUMFUNCS
} dmu_object_byteswap_t;
#define	DMU_OT_NEWTYPE 0x80
#define	DMU_OT_METADATA 0x40
#define	DMU_OT_ENCRYPTED 0x20
#define	DMU_OT_BYTESWAP_MASK 0x1f
#define	DMU_OT(byteswap, metadata, encrypted) \
	(DMU_OT_NEWTYPE | \
	((metadata) ? DMU_OT_METADATA : 0) | \
	((encrypted) ? DMU_OT_ENCRYPTED : 0) | \
	((byteswap) & DMU_OT_BYTESWAP_MASK))
#define	DMU_OT_IS_VALID(ot) (((ot) & DMU_OT_NEWTYPE) ? \
	((ot) & DMU_OT_BYTESWAP_MASK) < DMU_BSWAP_NUMFUNCS : \
	(ot) < DMU_OT_NUMTYPES)
#define	DMU_OT_IS_METADATA_CACHED(ot) (((ot) & DMU_OT_NEWTYPE) ? \
	B_TRUE : dmu_ot[(ot)].ot_dbuf_metadata_cache)
#ifndef ZFS_MDB
#define	DMU_OT_IS_METADATA_IMPL(ot) (dmu_ot[ot].ot_metadata)
#define	DMU_OT_IS_ENCRYPTED_IMPL(ot) (dmu_ot[ot].ot_encrypt)
#define	DMU_OT_BYTESWAP_IMPL(ot) (dmu_ot[ot].ot_byteswap)
#endif
#define	DMU_OT_IS_METADATA(ot) (((ot) & DMU_OT_NEWTYPE) ? \
	(((ot) & DMU_OT_METADATA) != 0) : \
	DMU_OT_IS_METADATA_IMPL(ot))
#define	DMU_OT_IS_DDT(ot) \
	((ot) == DMU_OT_DDT_ZAP)
#define	DMU_OT_IS_CRITICAL(ot) \
	(DMU_OT_IS_METADATA(ot) && \
	(ot) != DMU_OT_DNODE && \
	(ot) != DMU_OT_DIRECTORY_CONTENTS && \
	(ot) != DMU_OT_SA)
#define	DMU_OT_IS_FILE(ot) \
	((ot) == DMU_OT_PLAIN_FILE_CONTENTS || (ot) == DMU_OT_UINT64_OTHER)
#define	DMU_OT_IS_ENCRYPTED(ot) (((ot) & DMU_OT_NEWTYPE) ? \
	(((ot) & DMU_OT_ENCRYPTED) != 0) : \
	DMU_OT_IS_ENCRYPTED_IMPL(ot))
#define	DMU_OT_HAS_FILL(ot) \
	((ot) == DMU_OT_DNODE || (ot) == DMU_OT_OBJSET)
#define	DMU_OT_BYTESWAP(ot) (((ot) & DMU_OT_NEWTYPE) ? \
	((ot) & DMU_OT_BYTESWAP_MASK) : \
	DMU_OT_BYTESWAP_IMPL(ot))
typedef enum dmu_object_type {
	DMU_OT_NONE,
	DMU_OT_OBJECT_DIRECTORY,	 
	DMU_OT_OBJECT_ARRAY,		 
	DMU_OT_PACKED_NVLIST,		 
	DMU_OT_PACKED_NVLIST_SIZE,	 
	DMU_OT_BPOBJ,			 
	DMU_OT_BPOBJ_HDR,		 
	DMU_OT_SPACE_MAP_HEADER,	 
	DMU_OT_SPACE_MAP,		 
	DMU_OT_INTENT_LOG,		 
	DMU_OT_DNODE,			 
	DMU_OT_OBJSET,			 
	DMU_OT_DSL_DIR,			 
	DMU_OT_DSL_DIR_CHILD_MAP,	 
	DMU_OT_DSL_DS_SNAP_MAP,		 
	DMU_OT_DSL_PROPS,		 
	DMU_OT_DSL_DATASET,		 
	DMU_OT_ZNODE,			 
	DMU_OT_OLDACL,			 
	DMU_OT_PLAIN_FILE_CONTENTS,	 
	DMU_OT_DIRECTORY_CONTENTS,	 
	DMU_OT_MASTER_NODE,		 
	DMU_OT_UNLINKED_SET,		 
	DMU_OT_ZVOL,			 
	DMU_OT_ZVOL_PROP,		 
	DMU_OT_PLAIN_OTHER,		 
	DMU_OT_UINT64_OTHER,		 
	DMU_OT_ZAP_OTHER,		 
	DMU_OT_ERROR_LOG,		 
	DMU_OT_SPA_HISTORY,		 
	DMU_OT_SPA_HISTORY_OFFSETS,	 
	DMU_OT_POOL_PROPS,		 
	DMU_OT_DSL_PERMS,		 
	DMU_OT_ACL,			 
	DMU_OT_SYSACL,			 
	DMU_OT_FUID,			 
	DMU_OT_FUID_SIZE,		 
	DMU_OT_NEXT_CLONES,		 
	DMU_OT_SCAN_QUEUE,		 
	DMU_OT_USERGROUP_USED,		 
	DMU_OT_USERGROUP_QUOTA,		 
	DMU_OT_USERREFS,		 
	DMU_OT_DDT_ZAP,			 
	DMU_OT_DDT_STATS,		 
	DMU_OT_SA,			 
	DMU_OT_SA_MASTER_NODE,		 
	DMU_OT_SA_ATTR_REGISTRATION,	 
	DMU_OT_SA_ATTR_LAYOUTS,		 
	DMU_OT_SCAN_XLATE,		 
	DMU_OT_DEDUP,			 
	DMU_OT_DEADLIST,		 
	DMU_OT_DEADLIST_HDR,		 
	DMU_OT_DSL_CLONES,		 
	DMU_OT_BPOBJ_SUBOBJ,		 
	DMU_OT_NUMTYPES,
	DMU_OTN_UINT8_DATA = DMU_OT(DMU_BSWAP_UINT8, B_FALSE, B_FALSE),
	DMU_OTN_UINT8_METADATA = DMU_OT(DMU_BSWAP_UINT8, B_TRUE, B_FALSE),
	DMU_OTN_UINT16_DATA = DMU_OT(DMU_BSWAP_UINT16, B_FALSE, B_FALSE),
	DMU_OTN_UINT16_METADATA = DMU_OT(DMU_BSWAP_UINT16, B_TRUE, B_FALSE),
	DMU_OTN_UINT32_DATA = DMU_OT(DMU_BSWAP_UINT32, B_FALSE, B_FALSE),
	DMU_OTN_UINT32_METADATA = DMU_OT(DMU_BSWAP_UINT32, B_TRUE, B_FALSE),
	DMU_OTN_UINT64_DATA = DMU_OT(DMU_BSWAP_UINT64, B_FALSE, B_FALSE),
	DMU_OTN_UINT64_METADATA = DMU_OT(DMU_BSWAP_UINT64, B_TRUE, B_FALSE),
	DMU_OTN_ZAP_DATA = DMU_OT(DMU_BSWAP_ZAP, B_FALSE, B_FALSE),
	DMU_OTN_ZAP_METADATA = DMU_OT(DMU_BSWAP_ZAP, B_TRUE, B_FALSE),
	DMU_OTN_UINT8_ENC_DATA = DMU_OT(DMU_BSWAP_UINT8, B_FALSE, B_TRUE),
	DMU_OTN_UINT8_ENC_METADATA = DMU_OT(DMU_BSWAP_UINT8, B_TRUE, B_TRUE),
	DMU_OTN_UINT16_ENC_DATA = DMU_OT(DMU_BSWAP_UINT16, B_FALSE, B_TRUE),
	DMU_OTN_UINT16_ENC_METADATA = DMU_OT(DMU_BSWAP_UINT16, B_TRUE, B_TRUE),
	DMU_OTN_UINT32_ENC_DATA = DMU_OT(DMU_BSWAP_UINT32, B_FALSE, B_TRUE),
	DMU_OTN_UINT32_ENC_METADATA = DMU_OT(DMU_BSWAP_UINT32, B_TRUE, B_TRUE),
	DMU_OTN_UINT64_ENC_DATA = DMU_OT(DMU_BSWAP_UINT64, B_FALSE, B_TRUE),
	DMU_OTN_UINT64_ENC_METADATA = DMU_OT(DMU_BSWAP_UINT64, B_TRUE, B_TRUE),
	DMU_OTN_ZAP_ENC_DATA = DMU_OT(DMU_BSWAP_ZAP, B_FALSE, B_TRUE),
	DMU_OTN_ZAP_ENC_METADATA = DMU_OT(DMU_BSWAP_ZAP, B_TRUE, B_TRUE),
} dmu_object_type_t;
#define	TXG_NOWAIT	(0ULL)
#define	TXG_WAIT	(1ULL<<0)
#define	TXG_NOTHROTTLE	(1ULL<<1)
void byteswap_uint64_array(void *buf, size_t size);
void byteswap_uint32_array(void *buf, size_t size);
void byteswap_uint16_array(void *buf, size_t size);
void byteswap_uint8_array(void *buf, size_t size);
void zap_byteswap(void *buf, size_t size);
void zfs_oldacl_byteswap(void *buf, size_t size);
void zfs_acl_byteswap(void *buf, size_t size);
void zfs_znode_byteswap(void *buf, size_t size);
#define	DS_FIND_SNAPSHOTS	(1<<0)
#define	DS_FIND_CHILDREN	(1<<1)
#define	DS_FIND_SERIALIZE	(1<<2)
#define	DMU_MAX_ACCESS (64 * 1024 * 1024)  
#define	DMU_MAX_DELETEBLKCNT (20480)  
#define	DMU_USERUSED_OBJECT	(-1ULL)
#define	DMU_GROUPUSED_OBJECT	(-2ULL)
#define	DMU_PROJECTUSED_OBJECT	(-3ULL)
#define	DMU_OBJACCT_PREFIX	"obj-"
#define	DMU_OBJACCT_PREFIX_LEN	4
#define	DMU_BONUS_BLKID		(-1ULL)
#define	DMU_SPILL_BLKID		(-2ULL)
typedef void dmu_objset_create_sync_func_t(objset_t *os, void *arg,
    cred_t *cr, dmu_tx_t *tx);
int dmu_objset_hold(const char *name, const void *tag, objset_t **osp);
int dmu_objset_own(const char *name, dmu_objset_type_t type,
    boolean_t readonly, boolean_t key_required, const void *tag,
    objset_t **osp);
void dmu_objset_rele(objset_t *os, const void *tag);
void dmu_objset_disown(objset_t *os, boolean_t key_required, const void *tag);
int dmu_objset_open_ds(struct dsl_dataset *ds, objset_t **osp);
void dmu_objset_evict_dbufs(objset_t *os);
int dmu_objset_create(const char *name, dmu_objset_type_t type, uint64_t flags,
    struct dsl_crypto_params *dcp, dmu_objset_create_sync_func_t func,
    void *arg);
int dmu_objset_clone(const char *name, const char *origin);
int dsl_destroy_snapshots_nvl(struct nvlist *snaps, boolean_t defer,
    struct nvlist *errlist);
int dmu_objset_snapshot_one(const char *fsname, const char *snapname);
int dmu_objset_find(const char *name, int func(const char *, void *), void *arg,
    int flags);
void dmu_objset_byteswap(void *buf, size_t size);
int dsl_dataset_rename_snapshot(const char *fsname,
    const char *oldsnapname, const char *newsnapname, boolean_t recursive);
typedef struct dmu_buf {
	uint64_t db_object;		 
	uint64_t db_offset;		 
	uint64_t db_size;		 
	void *db_data;			 
} dmu_buf_t;
#define	DMU_POOL_DIRECTORY_OBJECT	1
#define	DMU_POOL_CONFIG			"config"
#define	DMU_POOL_FEATURES_FOR_WRITE	"features_for_write"
#define	DMU_POOL_FEATURES_FOR_READ	"features_for_read"
#define	DMU_POOL_FEATURE_DESCRIPTIONS	"feature_descriptions"
#define	DMU_POOL_FEATURE_ENABLED_TXG	"feature_enabled_txg"
#define	DMU_POOL_ROOT_DATASET		"root_dataset"
#define	DMU_POOL_SYNC_BPOBJ		"sync_bplist"
#define	DMU_POOL_ERRLOG_SCRUB		"errlog_scrub"
#define	DMU_POOL_ERRLOG_LAST		"errlog_last"
#define	DMU_POOL_SPARES			"spares"
#define	DMU_POOL_DEFLATE		"deflate"
#define	DMU_POOL_HISTORY		"history"
#define	DMU_POOL_PROPS			"pool_props"
#define	DMU_POOL_L2CACHE		"l2cache"
#define	DMU_POOL_TMP_USERREFS		"tmp_userrefs"
#define	DMU_POOL_DDT			"DDT-%s-%s-%s"
#define	DMU_POOL_DDT_STATS		"DDT-statistics"
#define	DMU_POOL_CREATION_VERSION	"creation_version"
#define	DMU_POOL_SCAN			"scan"
#define	DMU_POOL_ERRORSCRUB		"error_scrub"
#define	DMU_POOL_FREE_BPOBJ		"free_bpobj"
#define	DMU_POOL_BPTREE_OBJ		"bptree_obj"
#define	DMU_POOL_EMPTY_BPOBJ		"empty_bpobj"
#define	DMU_POOL_CHECKSUM_SALT		"org.illumos:checksum_salt"
#define	DMU_POOL_VDEV_ZAP_MAP		"com.delphix:vdev_zap_map"
#define	DMU_POOL_REMOVING		"com.delphix:removing"
#define	DMU_POOL_OBSOLETE_BPOBJ		"com.delphix:obsolete_bpobj"
#define	DMU_POOL_CONDENSING_INDIRECT	"com.delphix:condensing_indirect"
#define	DMU_POOL_ZPOOL_CHECKPOINT	"com.delphix:zpool_checkpoint"
#define	DMU_POOL_LOG_SPACEMAP_ZAP	"com.delphix:log_spacemap_zap"
#define	DMU_POOL_DELETED_CLONES		"com.delphix:deleted_clones"
uint64_t dmu_object_alloc(objset_t *os, dmu_object_type_t ot,
    int blocksize, dmu_object_type_t bonus_type, int bonus_len, dmu_tx_t *tx);
uint64_t dmu_object_alloc_ibs(objset_t *os, dmu_object_type_t ot, int blocksize,
    int indirect_blockshift,
    dmu_object_type_t bonustype, int bonuslen, dmu_tx_t *tx);
uint64_t dmu_object_alloc_dnsize(objset_t *os, dmu_object_type_t ot,
    int blocksize, dmu_object_type_t bonus_type, int bonus_len,
    int dnodesize, dmu_tx_t *tx);
uint64_t dmu_object_alloc_hold(objset_t *os, dmu_object_type_t ot,
    int blocksize, int indirect_blockshift, dmu_object_type_t bonustype,
    int bonuslen, int dnodesize, dnode_t **allocated_dnode, const void *tag,
    dmu_tx_t *tx);
int dmu_object_claim(objset_t *os, uint64_t object, dmu_object_type_t ot,
    int blocksize, dmu_object_type_t bonus_type, int bonus_len, dmu_tx_t *tx);
int dmu_object_claim_dnsize(objset_t *os, uint64_t object, dmu_object_type_t ot,
    int blocksize, dmu_object_type_t bonus_type, int bonus_len,
    int dnodesize, dmu_tx_t *tx);
int dmu_object_reclaim(objset_t *os, uint64_t object, dmu_object_type_t ot,
    int blocksize, dmu_object_type_t bonustype, int bonuslen, dmu_tx_t *txp);
int dmu_object_reclaim_dnsize(objset_t *os, uint64_t object,
    dmu_object_type_t ot, int blocksize, dmu_object_type_t bonustype,
    int bonuslen, int dnodesize, boolean_t keep_spill, dmu_tx_t *tx);
int dmu_object_rm_spill(objset_t *os, uint64_t object, dmu_tx_t *tx);
int dmu_object_free(objset_t *os, uint64_t object, dmu_tx_t *tx);
int dmu_object_next(objset_t *os, uint64_t *objectp,
    boolean_t hole, uint64_t txg);
int dmu_object_set_nlevels(objset_t *os, uint64_t object, int nlevels,
    dmu_tx_t *tx);
int dmu_object_set_blocksize(objset_t *os, uint64_t object, uint64_t size,
    int ibs, dmu_tx_t *tx);
int dmu_object_set_maxblkid(objset_t *os, uint64_t object, uint64_t maxblkid,
    dmu_tx_t *tx);
void dmu_object_set_checksum(objset_t *os, uint64_t object, uint8_t checksum,
    dmu_tx_t *tx);
void dmu_object_set_compress(objset_t *os, uint64_t object, uint8_t compress,
    dmu_tx_t *tx);
void dmu_write_embedded(objset_t *os, uint64_t object, uint64_t offset,
    void *data, uint8_t etype, uint8_t comp, int uncompressed_size,
    int compressed_size, int byteorder, dmu_tx_t *tx);
void dmu_redact(objset_t *os, uint64_t object, uint64_t offset, uint64_t size,
    dmu_tx_t *tx);
#define	WP_NOFILL	0x1
#define	WP_DMU_SYNC	0x2
#define	WP_SPILL	0x4
void dmu_write_policy(objset_t *os, dnode_t *dn, int level, int wp,
    struct zio_prop *zp);
int dmu_bonus_hold(objset_t *os, uint64_t object, const void *tag,
    dmu_buf_t **dbp);
int dmu_bonus_hold_by_dnode(dnode_t *dn, const void *tag, dmu_buf_t **dbp,
    uint32_t flags);
int dmu_bonus_max(void);
int dmu_set_bonus(dmu_buf_t *, int, dmu_tx_t *);
int dmu_set_bonustype(dmu_buf_t *, dmu_object_type_t, dmu_tx_t *);
dmu_object_type_t dmu_get_bonustype(dmu_buf_t *);
int dmu_rm_spill(objset_t *, uint64_t, dmu_tx_t *);
int dmu_spill_hold_by_bonus(dmu_buf_t *bonus, uint32_t flags, const void *tag,
    dmu_buf_t **dbp);
int dmu_spill_hold_by_dnode(dnode_t *dn, uint32_t flags,
    const void *tag, dmu_buf_t **dbp);
int dmu_spill_hold_existing(dmu_buf_t *bonus, const void *tag, dmu_buf_t **dbp);
int dmu_buf_hold(objset_t *os, uint64_t object, uint64_t offset,
    const void *tag, dmu_buf_t **, int flags);
int dmu_buf_hold_array(objset_t *os, uint64_t object, uint64_t offset,
    uint64_t length, int read, const void *tag, int *numbufsp,
    dmu_buf_t ***dbpp);
int dmu_buf_hold_noread(objset_t *os, uint64_t object, uint64_t offset,
    const void *tag, dmu_buf_t **dbp);
int dmu_buf_hold_by_dnode(dnode_t *dn, uint64_t offset,
    const void *tag, dmu_buf_t **dbp, int flags);
int dmu_buf_hold_array_by_dnode(dnode_t *dn, uint64_t offset,
    uint64_t length, boolean_t read, const void *tag, int *numbufsp,
    dmu_buf_t ***dbpp, uint32_t flags);
int dmu_buf_hold_noread_by_dnode(dnode_t *dn, uint64_t offset, const void *tag,
    dmu_buf_t **dbp);
void dmu_buf_add_ref(dmu_buf_t *db, const void *tag);
boolean_t dmu_buf_try_add_ref(dmu_buf_t *, objset_t *os, uint64_t object,
    uint64_t blkid, const void *tag);
void dmu_buf_rele(dmu_buf_t *db, const void *tag);
uint64_t dmu_buf_refcount(dmu_buf_t *db);
uint64_t dmu_buf_user_refcount(dmu_buf_t *db);
int dmu_buf_hold_array_by_bonus(dmu_buf_t *db, uint64_t offset,
    uint64_t length, boolean_t read, const void *tag,
    int *numbufsp, dmu_buf_t ***dbpp);
void dmu_buf_rele_array(dmu_buf_t **, int numbufs, const void *tag);
typedef void dmu_buf_evict_func_t(void *user_ptr);
typedef struct dmu_buf_user {
	taskq_ent_t	dbu_tqent;
	dmu_buf_evict_func_t *dbu_evict_func_sync;
	dmu_buf_evict_func_t *dbu_evict_func_async;
#ifdef ZFS_DEBUG
	dmu_buf_t **dbu_clear_on_evict_dbufp;
#endif
} dmu_buf_user_t;
static inline void
dmu_buf_init_user(dmu_buf_user_t *dbu, dmu_buf_evict_func_t *evict_func_sync,
    dmu_buf_evict_func_t *evict_func_async,
    dmu_buf_t **clear_on_evict_dbufp __maybe_unused)
{
	ASSERT(dbu->dbu_evict_func_sync == NULL);
	ASSERT(dbu->dbu_evict_func_async == NULL);
	IMPLY(evict_func_sync == NULL, evict_func_async != NULL);
	dbu->dbu_evict_func_sync = evict_func_sync;
	dbu->dbu_evict_func_async = evict_func_async;
	taskq_init_ent(&dbu->dbu_tqent);
#ifdef ZFS_DEBUG
	dbu->dbu_clear_on_evict_dbufp = clear_on_evict_dbufp;
#endif
}
void *dmu_buf_set_user(dmu_buf_t *db, dmu_buf_user_t *user);
void *dmu_buf_set_user_ie(dmu_buf_t *db, dmu_buf_user_t *user);
void *dmu_buf_replace_user(dmu_buf_t *db,
    dmu_buf_user_t *old_user, dmu_buf_user_t *new_user);
void *dmu_buf_remove_user(dmu_buf_t *db, dmu_buf_user_t *user);
void *dmu_buf_get_user(dmu_buf_t *db);
objset_t *dmu_buf_get_objset(dmu_buf_t *db);
dnode_t *dmu_buf_dnode_enter(dmu_buf_t *db);
void dmu_buf_dnode_exit(dmu_buf_t *db);
void dmu_buf_user_evict_wait(void);
struct blkptr *dmu_buf_get_blkptr(dmu_buf_t *db);
void dmu_buf_will_dirty(dmu_buf_t *db, dmu_tx_t *tx);
boolean_t dmu_buf_is_dirty(dmu_buf_t *db, dmu_tx_t *tx);
void dmu_buf_set_crypt_params(dmu_buf_t *db_fake, boolean_t byteorder,
    const uint8_t *salt, const uint8_t *iv, const uint8_t *mac, dmu_tx_t *tx);
#define	DMU_NEW_OBJECT	(-1ULL)
#define	DMU_OBJECT_END	(-1ULL)
dmu_tx_t *dmu_tx_create(objset_t *os);
void dmu_tx_hold_write(dmu_tx_t *tx, uint64_t object, uint64_t off, int len);
void dmu_tx_hold_write_by_dnode(dmu_tx_t *tx, dnode_t *dn, uint64_t off,
    int len);
void dmu_tx_hold_append(dmu_tx_t *tx, uint64_t object, uint64_t off, int len);
void dmu_tx_hold_append_by_dnode(dmu_tx_t *tx, dnode_t *dn, uint64_t off,
    int len);
void dmu_tx_hold_clone_by_dnode(dmu_tx_t *tx, dnode_t *dn, uint64_t off,
    int len);
void dmu_tx_hold_free(dmu_tx_t *tx, uint64_t object, uint64_t off,
    uint64_t len);
void dmu_tx_hold_free_by_dnode(dmu_tx_t *tx, dnode_t *dn, uint64_t off,
    uint64_t len);
void dmu_tx_hold_zap(dmu_tx_t *tx, uint64_t object, int add, const char *name);
void dmu_tx_hold_zap_by_dnode(dmu_tx_t *tx, dnode_t *dn, int add,
    const char *name);
void dmu_tx_hold_bonus(dmu_tx_t *tx, uint64_t object);
void dmu_tx_hold_bonus_by_dnode(dmu_tx_t *tx, dnode_t *dn);
void dmu_tx_hold_spill(dmu_tx_t *tx, uint64_t object);
void dmu_tx_hold_sa(dmu_tx_t *tx, struct sa_handle *hdl, boolean_t may_grow);
void dmu_tx_hold_sa_create(dmu_tx_t *tx, int total_size);
void dmu_tx_abort(dmu_tx_t *tx);
int dmu_tx_assign(dmu_tx_t *tx, uint64_t txg_how);
void dmu_tx_wait(dmu_tx_t *tx);
void dmu_tx_commit(dmu_tx_t *tx);
void dmu_tx_mark_netfree(dmu_tx_t *tx);
typedef void dmu_tx_callback_func_t(void *dcb_data, int error);
void dmu_tx_callback_register(dmu_tx_t *tx, dmu_tx_callback_func_t *dcb_func,
    void *dcb_data);
void dmu_tx_do_callbacks(list_t *cb_list, int error);
int dmu_free_range(objset_t *os, uint64_t object, uint64_t offset,
    uint64_t size, dmu_tx_t *tx);
int dmu_free_long_range(objset_t *os, uint64_t object, uint64_t offset,
    uint64_t size);
int dmu_free_long_object(objset_t *os, uint64_t object);
#define	DMU_READ_PREFETCH	0  
#define	DMU_READ_NO_PREFETCH	1  
#define	DMU_READ_NO_DECRYPT	2  
int dmu_read(objset_t *os, uint64_t object, uint64_t offset, uint64_t size,
	void *buf, uint32_t flags);
int dmu_read_by_dnode(dnode_t *dn, uint64_t offset, uint64_t size, void *buf,
    uint32_t flags);
void dmu_write(objset_t *os, uint64_t object, uint64_t offset, uint64_t size,
	const void *buf, dmu_tx_t *tx);
void dmu_write_by_dnode(dnode_t *dn, uint64_t offset, uint64_t size,
    const void *buf, dmu_tx_t *tx);
void dmu_prealloc(objset_t *os, uint64_t object, uint64_t offset, uint64_t size,
	dmu_tx_t *tx);
#ifdef _KERNEL
int dmu_read_uio(objset_t *os, uint64_t object, zfs_uio_t *uio, uint64_t size);
int dmu_read_uio_dbuf(dmu_buf_t *zdb, zfs_uio_t *uio, uint64_t size);
int dmu_read_uio_dnode(dnode_t *dn, zfs_uio_t *uio, uint64_t size);
int dmu_write_uio(objset_t *os, uint64_t object, zfs_uio_t *uio, uint64_t size,
	dmu_tx_t *tx);
int dmu_write_uio_dbuf(dmu_buf_t *zdb, zfs_uio_t *uio, uint64_t size,
	dmu_tx_t *tx);
int dmu_write_uio_dnode(dnode_t *dn, zfs_uio_t *uio, uint64_t size,
	dmu_tx_t *tx);
#endif
struct arc_buf *dmu_request_arcbuf(dmu_buf_t *handle, int size);
void dmu_return_arcbuf(struct arc_buf *buf);
int dmu_assign_arcbuf_by_dnode(dnode_t *dn, uint64_t offset,
    struct arc_buf *buf, dmu_tx_t *tx);
int dmu_assign_arcbuf_by_dbuf(dmu_buf_t *handle, uint64_t offset,
    struct arc_buf *buf, dmu_tx_t *tx);
#define	dmu_assign_arcbuf	dmu_assign_arcbuf_by_dbuf
extern uint_t zfs_max_recordsize;
void dmu_prefetch(objset_t *os, uint64_t object, int64_t level, uint64_t offset,
	uint64_t len, enum zio_priority pri);
typedef struct dmu_object_info {
	uint32_t doi_data_block_size;
	uint32_t doi_metadata_block_size;
	dmu_object_type_t doi_type;
	dmu_object_type_t doi_bonus_type;
	uint64_t doi_bonus_size;
	uint8_t doi_indirection;		 
	uint8_t doi_checksum;
	uint8_t doi_compress;
	uint8_t doi_nblkptr;
	uint8_t doi_pad[4];
	uint64_t doi_dnodesize;
	uint64_t doi_physical_blocks_512;	 
	uint64_t doi_max_offset;
	uint64_t doi_fill_count;		 
} dmu_object_info_t;
typedef void (*const arc_byteswap_func_t)(void *buf, size_t size);
typedef struct dmu_object_type_info {
	dmu_object_byteswap_t	ot_byteswap;
	boolean_t		ot_metadata;
	boolean_t		ot_dbuf_metadata_cache;
	boolean_t		ot_encrypt;
	const char		*ot_name;
} dmu_object_type_info_t;
typedef const struct dmu_object_byteswap_info {
	arc_byteswap_func_t	 ob_func;
	const char		*ob_name;
} dmu_object_byteswap_info_t;
extern const dmu_object_type_info_t dmu_ot[DMU_OT_NUMTYPES];
extern dmu_object_byteswap_info_t dmu_ot_byteswap[DMU_BSWAP_NUMFUNCS];
int dmu_object_info(objset_t *os, uint64_t object, dmu_object_info_t *doi);
void __dmu_object_info_from_dnode(struct dnode *dn, dmu_object_info_t *doi);
void dmu_object_info_from_dnode(dnode_t *dn, dmu_object_info_t *doi);
void dmu_object_info_from_db(dmu_buf_t *db, dmu_object_info_t *doi);
void dmu_object_size_from_db(dmu_buf_t *db, uint32_t *blksize,
    u_longlong_t *nblk512);
void dmu_object_dnsize_from_db(dmu_buf_t *db, int *dnsize);
typedef struct dmu_objset_stats {
	uint64_t dds_num_clones;  
	uint64_t dds_creation_txg;
	uint64_t dds_guid;
	dmu_objset_type_t dds_type;
	uint8_t dds_is_snapshot;
	uint8_t dds_inconsistent;
	uint8_t dds_redacted;
	char dds_origin[ZFS_MAX_DATASET_NAME_LEN];
} dmu_objset_stats_t;
void dmu_objset_fast_stat(objset_t *os, dmu_objset_stats_t *stat);
void dmu_objset_stats(objset_t *os, struct nvlist *nv);
void dmu_objset_space(objset_t *os, uint64_t *refdbytesp, uint64_t *availbytesp,
    uint64_t *usedobjsp, uint64_t *availobjsp);
uint64_t dmu_objset_fsid_guid(objset_t *os);
inode_timespec_t dmu_objset_snap_cmtime(objset_t *os);
int dmu_objset_is_snapshot(objset_t *os);
extern struct spa *dmu_objset_spa(objset_t *os);
extern struct zilog *dmu_objset_zil(objset_t *os);
extern struct dsl_pool *dmu_objset_pool(objset_t *os);
extern struct dsl_dataset *dmu_objset_ds(objset_t *os);
extern void dmu_objset_name(objset_t *os, char *buf);
extern dmu_objset_type_t dmu_objset_type(objset_t *os);
extern uint64_t dmu_objset_id(objset_t *os);
extern uint64_t dmu_objset_dnodesize(objset_t *os);
extern zfs_sync_type_t dmu_objset_syncprop(objset_t *os);
extern zfs_logbias_op_t dmu_objset_logbias(objset_t *os);
extern int dmu_objset_blksize(objset_t *os);
extern int dmu_snapshot_list_next(objset_t *os, int namelen, char *name,
    uint64_t *id, uint64_t *offp, boolean_t *case_conflict);
extern int dmu_snapshot_lookup(objset_t *os, const char *name, uint64_t *val);
extern int dmu_snapshot_realname(objset_t *os, const char *name, char *real,
    int maxlen, boolean_t *conflict);
extern int dmu_dir_list_next(objset_t *os, int namelen, char *name,
    uint64_t *idp, uint64_t *offp);
typedef struct zfs_file_info {
	uint64_t zfi_user;
	uint64_t zfi_group;
	uint64_t zfi_project;
	uint64_t zfi_generation;
} zfs_file_info_t;
typedef int file_info_cb_t(dmu_object_type_t bonustype, const void *data,
    struct zfs_file_info *zoi);
extern void dmu_objset_register_type(dmu_objset_type_t ost,
    file_info_cb_t *cb);
extern void dmu_objset_set_user(objset_t *os, void *user_ptr);
extern void *dmu_objset_get_user(objset_t *os);
uint64_t dmu_tx_get_txg(dmu_tx_t *tx);
typedef struct zgd {
	struct lwb	*zgd_lwb;
	struct blkptr	*zgd_bp;
	dmu_buf_t	*zgd_db;
	struct zfs_locked_range *zgd_lr;
	void		*zgd_private;
} zgd_t;
typedef void dmu_sync_cb_t(zgd_t *arg, int error);
int dmu_sync(struct zio *zio, uint64_t txg, dmu_sync_cb_t *done, zgd_t *zgd);
int dmu_offset_next(objset_t *os, uint64_t object, boolean_t hole,
    uint64_t *off);
int dmu_read_l0_bps(objset_t *os, uint64_t object, uint64_t offset,
    uint64_t length, struct blkptr *bps, size_t *nbpsp);
int dmu_brt_clone(objset_t *os, uint64_t object, uint64_t offset,
    uint64_t length, dmu_tx_t *tx, const struct blkptr *bps, size_t nbps);
extern void dmu_init(void);
extern void dmu_fini(void);
typedef void (*dmu_traverse_cb_t)(objset_t *os, void *arg, struct blkptr *bp,
    uint64_t object, uint64_t offset, int len);
void dmu_traverse_objset(objset_t *os, uint64_t txg_start,
    dmu_traverse_cb_t cb, void *arg);
int dmu_diff(const char *tosnap_name, const char *fromsnap_name,
    zfs_file_t *fp, offset_t *offp);
#define	ZFS_CRC64_POLY	0xC96C5795D7870F42ULL	 
extern uint64_t zfs_crc64_table[256];
extern uint_t dmu_prefetch_max;
#ifdef	__cplusplus
}
#endif
#endif	 
