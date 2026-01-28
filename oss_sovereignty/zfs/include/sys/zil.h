#ifndef	_SYS_ZIL_H
#define	_SYS_ZIL_H
#include <sys/types.h>
#include <sys/spa.h>
#include <sys/zio.h>
#include <sys/dmu.h>
#include <sys/zio_crypt.h>
#include <sys/wmsum.h>
#ifdef	__cplusplus
extern "C" {
#endif
struct dsl_pool;
struct dsl_dataset;
struct lwb;
typedef struct zil_header {
	uint64_t zh_claim_txg;	 
	uint64_t zh_replay_seq;	 
	blkptr_t zh_log;	 
	uint64_t zh_claim_blk_seq;  
	uint64_t zh_flags;	 
	uint64_t zh_claim_lr_seq;  
	uint64_t zh_pad[3];
} zil_header_t;
#define	ZIL_REPLAY_NEEDED	0x1	 
#define	ZIL_CLAIM_LR_SEQ_VALID	0x2	 
typedef struct zil_chain {
	uint64_t zc_pad;
	blkptr_t zc_next_blk;	 
	uint64_t zc_nused;	 
	zio_eck_t zc_eck;	 
} zil_chain_t;
#define	ZIL_MIN_BLKSZ	4096ULL
#define	ZILTEST_TXG (UINT64_MAX - TXG_CONCURRENT_STATES)
#define	ZIL_ZC_GUID_0	0
#define	ZIL_ZC_GUID_1	1
#define	ZIL_ZC_OBJSET	2
#define	ZIL_ZC_SEQ	3
typedef enum zil_create {
	Z_FILE,
	Z_DIR,
	Z_XATTRDIR,
} zil_create_t;
#define	ZIL_XVAT_SIZE(mapsize) \
	sizeof (lr_attr_t) + (sizeof (uint32_t) * (mapsize - 1)) + \
	(sizeof (uint64_t) * 7)
#define	ZIL_ACE_LENGTH(x)	(roundup(x, sizeof (uint64_t)))
#define	TX_COMMIT		0	 
#define	TX_CREATE		1	 
#define	TX_MKDIR		2	 
#define	TX_MKXATTR		3	 
#define	TX_SYMLINK		4	 
#define	TX_REMOVE		5	 
#define	TX_RMDIR		6	 
#define	TX_LINK			7	 
#define	TX_RENAME		8	 
#define	TX_WRITE		9	 
#define	TX_TRUNCATE		10	 
#define	TX_SETATTR		11	 
#define	TX_ACL_V0		12	 
#define	TX_ACL			13	 
#define	TX_CREATE_ACL		14	 
#define	TX_CREATE_ATTR		15	 
#define	TX_CREATE_ACL_ATTR 	16	 
#define	TX_MKDIR_ACL		17	 
#define	TX_MKDIR_ATTR		18	 
#define	TX_MKDIR_ACL_ATTR	19	 
#define	TX_WRITE2		20	 
#define	TX_SETSAXATTR		21	 
#define	TX_RENAME_EXCHANGE	22	 
#define	TX_RENAME_WHITEOUT	23	 
#define	TX_CLONE_RANGE		24	 
#define	TX_MAX_TYPE		25	 
#define	TX_CI	((uint64_t)0x1 << 63)  
#define	TX_OOO(txtype)			\
	((txtype) == TX_WRITE ||	\
	(txtype) == TX_TRUNCATE ||	\
	(txtype) == TX_SETATTR ||	\
	(txtype) == TX_ACL_V0 ||	\
	(txtype) == TX_ACL ||		\
	(txtype) == TX_WRITE2 ||	\
	(txtype) == TX_SETSAXATTR ||	\
	(txtype) == TX_CLONE_RANGE)
#define	LR_FOID_GET_SLOTS(oid) (BF64_GET((oid), 56, 8) + 1)
#define	LR_FOID_SET_SLOTS(oid, x) BF64_SET((oid), 56, 8, (x) - 1)
#define	LR_FOID_GET_OBJ(oid) BF64_GET((oid), 0, DN_MAX_OBJECT_SHIFT)
#define	LR_FOID_SET_OBJ(oid, x) BF64_SET((oid), 0, DN_MAX_OBJECT_SHIFT, (x))
typedef struct {			 
	uint64_t	lrc_txtype;	 
	uint64_t	lrc_reclen;	 
	uint64_t	lrc_txg;	 
	uint64_t	lrc_seq;	 
} lr_t;
typedef struct {
	lr_t		lr_common;	 
	uint64_t	lr_foid;	 
} lr_ooo_t;
typedef struct {
	uint64_t	lr_attr_attrs;		 
	uint64_t	lr_attr_crtime[2];	 
	uint8_t		lr_attr_scanstamp[32];
} lr_attr_end_t;
typedef struct {
	uint32_t	lr_attr_masksize;  
	uint32_t	lr_attr_bitmap;  
} lr_attr_t;
typedef struct {
	lr_t		lr_common;	 
	uint64_t	lr_doid;	 
	uint64_t	lr_foid;	 
	uint64_t	lr_mode;	 
	uint64_t	lr_uid;		 
	uint64_t	lr_gid;		 
	uint64_t	lr_gen;		 
	uint64_t	lr_crtime[2];	 
	uint64_t	lr_rdev;	 
} lr_create_t;
typedef struct {
	lr_create_t	lr_create;	 
	uint64_t	lr_aclcnt;	 
	uint64_t	lr_domcnt;	 
	uint64_t	lr_fuidcnt;	 
	uint64_t	lr_acl_bytes;	 
	uint64_t	lr_acl_flags;	 
} lr_acl_create_t;
typedef struct {
	lr_t		lr_common;	 
	uint64_t	lr_doid;	 
} lr_remove_t;
typedef struct {
	lr_t		lr_common;	 
	uint64_t	lr_doid;	 
	uint64_t	lr_link_obj;	 
} lr_link_t;
typedef struct {
	lr_t		lr_common;	 
	uint64_t	lr_sdoid;	 
	uint64_t	lr_tdoid;	 
} lr_rename_t;
typedef struct {
	lr_rename_t	lr_rename;	 
	uint64_t	lr_wfoid;	 
	uint64_t	lr_wmode;	 
	uint64_t	lr_wuid;	 
	uint64_t	lr_wgid;	 
	uint64_t	lr_wgen;	 
	uint64_t	lr_wcrtime[2];	 
	uint64_t	lr_wrdev;	 
} lr_rename_whiteout_t;
typedef struct {
	lr_t		lr_common;	 
	uint64_t	lr_foid;	 
	uint64_t	lr_offset;	 
	uint64_t	lr_length;	 
	uint64_t	lr_blkoff;	 
	blkptr_t	lr_blkptr;	 
} lr_write_t;
typedef struct {
	lr_t		lr_common;	 
	uint64_t	lr_foid;	 
	uint64_t	lr_offset;	 
	uint64_t	lr_length;	 
} lr_truncate_t;
typedef struct {
	lr_t		lr_common;	 
	uint64_t	lr_foid;	 
	uint64_t	lr_mask;	 
	uint64_t	lr_mode;	 
	uint64_t	lr_uid;		 
	uint64_t	lr_gid;		 
	uint64_t	lr_size;	 
	uint64_t	lr_atime[2];	 
	uint64_t	lr_mtime[2];	 
} lr_setattr_t;
typedef struct {
	lr_t		lr_common;	 
	uint64_t	lr_foid;	 
	uint64_t	lr_size;
} lr_setsaxattr_t;
typedef struct {
	lr_t		lr_common;	 
	uint64_t	lr_foid;	 
	uint64_t	lr_aclcnt;	 
} lr_acl_v0_t;
typedef struct {
	lr_t		lr_common;	 
	uint64_t	lr_foid;	 
	uint64_t	lr_aclcnt;	 
	uint64_t	lr_domcnt;	 
	uint64_t	lr_fuidcnt;	 
	uint64_t	lr_acl_bytes;	 
	uint64_t	lr_acl_flags;	 
} lr_acl_t;
typedef struct {
	lr_t		lr_common;	 
	uint64_t	lr_foid;	 
	uint64_t	lr_offset;	 
	uint64_t	lr_length;	 
	uint64_t	lr_blksz;	 
	uint64_t	lr_nbps;	 
	blkptr_t	lr_bps[];
} lr_clone_range_t;
typedef enum {
	WR_INDIRECT,	 
	WR_COPIED,	 
	WR_NEED_COPY,	 
	WR_NUM_STATES	 
} itx_wr_state_t;
typedef void (*zil_callback_t)(void *data);
typedef struct itx {
	list_node_t	itx_node;	 
	void		*itx_private;	 
	itx_wr_state_t	itx_wr_state;	 
	uint8_t		itx_sync;	 
	zil_callback_t	itx_callback;    
	void		*itx_callback_data;  
	size_t		itx_size;	 
	uint64_t	itx_oid;	 
	uint64_t	itx_gen;	 
	lr_t		itx_lr;		 
} itx_t;
typedef struct zil_stats {
	kstat_named_t zil_commit_count;
	kstat_named_t zil_commit_writer_count;
	kstat_named_t zil_itx_count;
	kstat_named_t zil_itx_indirect_count;
	kstat_named_t zil_itx_indirect_bytes;
	kstat_named_t zil_itx_copied_count;
	kstat_named_t zil_itx_copied_bytes;
	kstat_named_t zil_itx_needcopy_count;
	kstat_named_t zil_itx_needcopy_bytes;
	kstat_named_t zil_itx_metaslab_normal_count;
	kstat_named_t zil_itx_metaslab_normal_bytes;
	kstat_named_t zil_itx_metaslab_normal_write;
	kstat_named_t zil_itx_metaslab_normal_alloc;
	kstat_named_t zil_itx_metaslab_slog_count;
	kstat_named_t zil_itx_metaslab_slog_bytes;
	kstat_named_t zil_itx_metaslab_slog_write;
	kstat_named_t zil_itx_metaslab_slog_alloc;
} zil_kstat_values_t;
typedef struct zil_sums {
	wmsum_t zil_commit_count;
	wmsum_t zil_commit_writer_count;
	wmsum_t zil_itx_count;
	wmsum_t zil_itx_indirect_count;
	wmsum_t zil_itx_indirect_bytes;
	wmsum_t zil_itx_copied_count;
	wmsum_t zil_itx_copied_bytes;
	wmsum_t zil_itx_needcopy_count;
	wmsum_t zil_itx_needcopy_bytes;
	wmsum_t zil_itx_metaslab_normal_count;
	wmsum_t zil_itx_metaslab_normal_bytes;
	wmsum_t zil_itx_metaslab_normal_write;
	wmsum_t zil_itx_metaslab_normal_alloc;
	wmsum_t zil_itx_metaslab_slog_count;
	wmsum_t zil_itx_metaslab_slog_bytes;
	wmsum_t zil_itx_metaslab_slog_write;
	wmsum_t zil_itx_metaslab_slog_alloc;
} zil_sums_t;
#define	ZIL_STAT_INCR(zil, stat, val) \
	do { \
		int64_t tmpval = (val); \
		wmsum_add(&(zil_sums_global.stat), tmpval); \
		if ((zil)->zl_sums) \
			wmsum_add(&((zil)->zl_sums->stat), tmpval); \
	} while (0)
#define	ZIL_STAT_BUMP(zil, stat) \
    ZIL_STAT_INCR(zil, stat, 1);
typedef int zil_parse_blk_func_t(zilog_t *zilog, const blkptr_t *bp, void *arg,
    uint64_t txg);
typedef int zil_parse_lr_func_t(zilog_t *zilog, const lr_t *lr, void *arg,
    uint64_t txg);
typedef int zil_replay_func_t(void *arg1, void *arg2, boolean_t byteswap);
typedef int zil_get_data_t(void *arg, uint64_t arg2, lr_write_t *lr, char *dbuf,
    struct lwb *lwb, zio_t *zio);
extern int zil_parse(zilog_t *zilog, zil_parse_blk_func_t *parse_blk_func,
    zil_parse_lr_func_t *parse_lr_func, void *arg, uint64_t txg,
    boolean_t decrypt);
extern void	zil_init(void);
extern void	zil_fini(void);
extern zilog_t	*zil_alloc(objset_t *os, zil_header_t *zh_phys);
extern void	zil_free(zilog_t *zilog);
extern zilog_t	*zil_open(objset_t *os, zil_get_data_t *get_data,
    zil_sums_t *zil_sums);
extern void	zil_close(zilog_t *zilog);
extern boolean_t zil_replay(objset_t *os, void *arg,
    zil_replay_func_t *const replay_func[TX_MAX_TYPE]);
extern boolean_t zil_replaying(zilog_t *zilog, dmu_tx_t *tx);
extern boolean_t zil_destroy(zilog_t *zilog, boolean_t keep_first);
extern void	zil_destroy_sync(zilog_t *zilog, dmu_tx_t *tx);
extern itx_t	*zil_itx_create(uint64_t txtype, size_t lrsize);
extern void	zil_itx_destroy(itx_t *itx);
extern void	zil_itx_assign(zilog_t *zilog, itx_t *itx, dmu_tx_t *tx);
extern void	zil_async_to_sync(zilog_t *zilog, uint64_t oid);
extern void	zil_commit(zilog_t *zilog, uint64_t oid);
extern void	zil_commit_impl(zilog_t *zilog, uint64_t oid);
extern void	zil_remove_async(zilog_t *zilog, uint64_t oid);
extern int	zil_reset(const char *osname, void *txarg);
extern int	zil_claim(struct dsl_pool *dp,
    struct dsl_dataset *ds, void *txarg);
extern int 	zil_check_log_chain(struct dsl_pool *dp,
    struct dsl_dataset *ds, void *tx);
extern void	zil_sync(zilog_t *zilog, dmu_tx_t *tx);
extern void	zil_clean(zilog_t *zilog, uint64_t synced_txg);
extern int	zil_suspend(const char *osname, void **cookiep);
extern void	zil_resume(void *cookie);
extern void	zil_lwb_add_block(struct lwb *lwb, const blkptr_t *bp);
extern void	zil_lwb_add_txg(struct lwb *lwb, uint64_t txg);
extern int	zil_bp_tree_add(zilog_t *zilog, const blkptr_t *bp);
extern void	zil_set_sync(zilog_t *zilog, uint64_t syncval);
extern void	zil_set_logbias(zilog_t *zilog, uint64_t slogval);
extern uint64_t	zil_max_copied_data(zilog_t *zilog);
extern uint64_t	zil_max_log_data(zilog_t *zilog, size_t hdrsize);
extern void zil_sums_init(zil_sums_t *zs);
extern void zil_sums_fini(zil_sums_t *zs);
extern void zil_kstat_values_update(zil_kstat_values_t *zs,
    zil_sums_t *zil_sums);
extern int zil_replay_disable;
#ifdef	__cplusplus
}
#endif
#endif	 
