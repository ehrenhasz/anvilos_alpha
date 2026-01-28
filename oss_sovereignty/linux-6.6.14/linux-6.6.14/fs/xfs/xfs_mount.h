#ifndef __XFS_MOUNT_H__
#define	__XFS_MOUNT_H__
struct xlog;
struct xfs_inode;
struct xfs_mru_cache;
struct xfs_ail;
struct xfs_quotainfo;
struct xfs_da_geometry;
struct xfs_perag;
enum {
	XFS_LOWSP_1_PCNT = 0,
	XFS_LOWSP_2_PCNT,
	XFS_LOWSP_3_PCNT,
	XFS_LOWSP_4_PCNT,
	XFS_LOWSP_5_PCNT,
	XFS_LOWSP_MAX,
};
enum {
	XFS_ERR_METADATA,
	XFS_ERR_CLASS_MAX,
};
enum {
	XFS_ERR_DEFAULT,
	XFS_ERR_EIO,
	XFS_ERR_ENOSPC,
	XFS_ERR_ENODEV,
	XFS_ERR_ERRNO_MAX,
};
#define XFS_ERR_RETRY_FOREVER	-1
struct xfs_error_cfg {
	struct xfs_kobj	kobj;
	int		max_retries;
	long		retry_timeout;	 
};
struct xfs_inodegc {
	struct xfs_mount	*mp;
	struct llist_head	list;
	struct delayed_work	work;
	int			error;
	unsigned int		items;
	unsigned int		shrinker_hits;
	unsigned int		cpu;
};
typedef struct xfs_mount {
	struct xfs_sb		m_sb;		 
	struct super_block	*m_super;
	struct xfs_ail		*m_ail;		 
	struct xfs_buf		*m_sb_bp;	 
	char			*m_rtname;	 
	char			*m_logname;	 
	struct xfs_da_geometry	*m_dir_geo;	 
	struct xfs_da_geometry	*m_attr_geo;	 
	struct xlog		*m_log;		 
	struct xfs_inode	*m_rbmip;	 
	struct xfs_inode	*m_rsumip;	 
	struct xfs_inode	*m_rootip;	 
	struct xfs_quotainfo	*m_quotainfo;	 
	xfs_buftarg_t		*m_ddev_targp;	 
	xfs_buftarg_t		*m_logdev_targp; 
	xfs_buftarg_t		*m_rtdev_targp;	 
	void __percpu		*m_inodegc;	 
	uint8_t			*m_rsum_cache;
	struct xfs_mru_cache	*m_filestream;   
	struct workqueue_struct *m_buf_workqueue;
	struct workqueue_struct	*m_unwritten_workqueue;
	struct workqueue_struct	*m_reclaim_workqueue;
	struct workqueue_struct	*m_sync_workqueue;
	struct workqueue_struct *m_blockgc_wq;
	struct workqueue_struct *m_inodegc_wq;
	int			m_bsize;	 
	uint8_t			m_blkbit_log;	 
	uint8_t			m_blkbb_log;	 
	uint8_t			m_agno_log;	 
	uint8_t			m_sectbb_log;	 
	uint			m_blockmask;	 
	uint			m_blockwsize;	 
	uint			m_blockwmask;	 
	uint			m_alloc_mxr[2];	 
	uint			m_alloc_mnr[2];	 
	uint			m_bmap_dmxr[2];	 
	uint			m_bmap_dmnr[2];	 
	uint			m_rmap_mxr[2];	 
	uint			m_rmap_mnr[2];	 
	uint			m_refc_mxr[2];	 
	uint			m_refc_mnr[2];	 
	uint			m_alloc_maxlevels;  
	uint			m_bm_maxlevels[2];  
	uint			m_rmap_maxlevels;  
	uint			m_refc_maxlevels;  
	unsigned int		m_agbtree_maxlevels;  
	xfs_extlen_t		m_ag_prealloc_blocks;  
	uint			m_alloc_set_aside;  
	uint			m_ag_max_usable;  
	int			m_dalign;	 
	int			m_swidth;	 
	xfs_agnumber_t		m_maxagi;	 
	uint			m_allocsize_log; 
	uint			m_allocsize_blocks;  
	int			m_logbufs;	 
	int			m_logbsize;	 
	uint			m_rsumlevels;	 
	uint			m_rsumsize;	 
	int			m_fixedfsid[2];	 
	uint			m_qflags;	 
	uint64_t		m_features;	 
	uint64_t		m_low_space[XFS_LOWSP_MAX];
	uint64_t		m_low_rtexts[XFS_LOWSP_MAX];
	struct xfs_ino_geometry	m_ino_geo;	 
	struct xfs_trans_resv	m_resv;		 
	unsigned long		m_opstate;	 
	bool			m_always_cow;
	bool			m_fail_unmount;
	bool			m_finobt_nores;  
	bool			m_update_sb;	 
	uint8_t			m_fs_checked;
	uint8_t			m_fs_sick;
	uint8_t			m_rt_checked;
	uint8_t			m_rt_sick;
	spinlock_t ____cacheline_aligned m_sb_lock;  
	struct percpu_counter	m_icount;	 
	struct percpu_counter	m_ifree;	 
	struct percpu_counter	m_fdblocks;	 
	struct percpu_counter	m_frextents;	 
	struct percpu_counter	m_delalloc_blks;
	atomic64_t		m_allocbt_blks;
	struct radix_tree_root	m_perag_tree;	 
	spinlock_t		m_perag_lock;	 
	uint64_t		m_resblks;	 
	uint64_t		m_resblks_avail; 
	uint64_t		m_resblks_save;	 
	struct delayed_work	m_reclaim_work;	 
	struct dentry		*m_debugfs;	 
	struct xfs_kobj		m_kobj;
	struct xfs_kobj		m_error_kobj;
	struct xfs_kobj		m_error_meta_kobj;
	struct xfs_error_cfg	m_error_cfg[XFS_ERR_CLASS_MAX][XFS_ERR_ERRNO_MAX];
	struct xstats		m_stats;	 
#ifdef CONFIG_XFS_ONLINE_SCRUB_STATS
	struct xchk_stats	*m_scrub_stats;
#endif
	xfs_agnumber_t		m_agfrotor;	 
	atomic_t		m_agirotor;	 
	struct shrinker		m_inodegc_shrinker;
	struct work_struct	m_flush_inodes_work;
	uint32_t		m_generation;
	struct mutex		m_growlock;	 
#ifdef DEBUG
	unsigned int		*m_errortag;
	struct xfs_kobj		m_errortag_kobj;
#endif
	struct cpumask		m_inodegc_cpumask;
} xfs_mount_t;
#define M_IGEO(mp)		(&(mp)->m_ino_geo)
#define XFS_FEAT_ATTR		(1ULL << 0)	 
#define XFS_FEAT_NLINK		(1ULL << 1)	 
#define XFS_FEAT_QUOTA		(1ULL << 2)	 
#define XFS_FEAT_ALIGN		(1ULL << 3)	 
#define XFS_FEAT_DALIGN		(1ULL << 4)	 
#define XFS_FEAT_LOGV2		(1ULL << 5)	 
#define XFS_FEAT_SECTOR		(1ULL << 6)	 
#define XFS_FEAT_EXTFLG		(1ULL << 7)	 
#define XFS_FEAT_ASCIICI	(1ULL << 8)	 
#define XFS_FEAT_LAZYSBCOUNT	(1ULL << 9)	 
#define XFS_FEAT_ATTR2		(1ULL << 10)	 
#define XFS_FEAT_PARENT		(1ULL << 11)	 
#define XFS_FEAT_PROJID32	(1ULL << 12)	 
#define XFS_FEAT_CRC		(1ULL << 13)	 
#define XFS_FEAT_V3INODES	(1ULL << 14)	 
#define XFS_FEAT_PQUOTINO	(1ULL << 15)	 
#define XFS_FEAT_FTYPE		(1ULL << 16)	 
#define XFS_FEAT_FINOBT		(1ULL << 17)	 
#define XFS_FEAT_RMAPBT		(1ULL << 18)	 
#define XFS_FEAT_REFLINK	(1ULL << 19)	 
#define XFS_FEAT_SPINODES	(1ULL << 20)	 
#define XFS_FEAT_META_UUID	(1ULL << 21)	 
#define XFS_FEAT_REALTIME	(1ULL << 22)	 
#define XFS_FEAT_INOBTCNT	(1ULL << 23)	 
#define XFS_FEAT_BIGTIME	(1ULL << 24)	 
#define XFS_FEAT_NEEDSREPAIR	(1ULL << 25)	 
#define XFS_FEAT_NREXT64	(1ULL << 26)	 
#define XFS_FEAT_NOATTR2	(1ULL << 48)	 
#define XFS_FEAT_NOALIGN	(1ULL << 49)	 
#define XFS_FEAT_ALLOCSIZE	(1ULL << 50)	 
#define XFS_FEAT_LARGE_IOSIZE	(1ULL << 51)	 
#define XFS_FEAT_WSYNC		(1ULL << 52)	 
#define XFS_FEAT_DIRSYNC	(1ULL << 53)	 
#define XFS_FEAT_DISCARD	(1ULL << 54)	 
#define XFS_FEAT_GRPID		(1ULL << 55)	 
#define XFS_FEAT_SMALL_INUMS	(1ULL << 56)	 
#define XFS_FEAT_IKEEP		(1ULL << 57)	 
#define XFS_FEAT_SWALLOC	(1ULL << 58)	 
#define XFS_FEAT_FILESTREAMS	(1ULL << 59)	 
#define XFS_FEAT_DAX_ALWAYS	(1ULL << 60)	 
#define XFS_FEAT_DAX_NEVER	(1ULL << 61)	 
#define XFS_FEAT_NORECOVERY	(1ULL << 62)	 
#define XFS_FEAT_NOUUID		(1ULL << 63)	 
#define __XFS_HAS_FEAT(name, NAME) \
static inline bool xfs_has_ ## name (struct xfs_mount *mp) \
{ \
	return mp->m_features & XFS_FEAT_ ## NAME; \
}
#define __XFS_ADD_FEAT(name, NAME) \
	__XFS_HAS_FEAT(name, NAME); \
static inline void xfs_add_ ## name (struct xfs_mount *mp) \
{ \
	mp->m_features |= XFS_FEAT_ ## NAME; \
	xfs_sb_version_add ## name(&mp->m_sb); \
}
__XFS_ADD_FEAT(attr, ATTR)
__XFS_HAS_FEAT(nlink, NLINK)
__XFS_ADD_FEAT(quota, QUOTA)
__XFS_HAS_FEAT(align, ALIGN)
__XFS_HAS_FEAT(dalign, DALIGN)
__XFS_HAS_FEAT(logv2, LOGV2)
__XFS_HAS_FEAT(sector, SECTOR)
__XFS_HAS_FEAT(extflg, EXTFLG)
__XFS_HAS_FEAT(asciici, ASCIICI)
__XFS_HAS_FEAT(lazysbcount, LAZYSBCOUNT)
__XFS_ADD_FEAT(attr2, ATTR2)
__XFS_HAS_FEAT(parent, PARENT)
__XFS_ADD_FEAT(projid32, PROJID32)
__XFS_HAS_FEAT(crc, CRC)
__XFS_HAS_FEAT(v3inodes, V3INODES)
__XFS_HAS_FEAT(pquotino, PQUOTINO)
__XFS_HAS_FEAT(ftype, FTYPE)
__XFS_HAS_FEAT(finobt, FINOBT)
__XFS_HAS_FEAT(rmapbt, RMAPBT)
__XFS_HAS_FEAT(reflink, REFLINK)
__XFS_HAS_FEAT(sparseinodes, SPINODES)
__XFS_HAS_FEAT(metauuid, META_UUID)
__XFS_HAS_FEAT(realtime, REALTIME)
__XFS_HAS_FEAT(inobtcounts, INOBTCNT)
__XFS_HAS_FEAT(bigtime, BIGTIME)
__XFS_HAS_FEAT(needsrepair, NEEDSREPAIR)
__XFS_HAS_FEAT(large_extent_counts, NREXT64)
__XFS_HAS_FEAT(noattr2, NOATTR2)
__XFS_HAS_FEAT(noalign, NOALIGN)
__XFS_HAS_FEAT(allocsize, ALLOCSIZE)
__XFS_HAS_FEAT(large_iosize, LARGE_IOSIZE)
__XFS_HAS_FEAT(wsync, WSYNC)
__XFS_HAS_FEAT(dirsync, DIRSYNC)
__XFS_HAS_FEAT(discard, DISCARD)
__XFS_HAS_FEAT(grpid, GRPID)
__XFS_HAS_FEAT(small_inums, SMALL_INUMS)
__XFS_HAS_FEAT(ikeep, IKEEP)
__XFS_HAS_FEAT(swalloc, SWALLOC)
__XFS_HAS_FEAT(filestreams, FILESTREAMS)
__XFS_HAS_FEAT(dax_always, DAX_ALWAYS)
__XFS_HAS_FEAT(dax_never, DAX_NEVER)
__XFS_HAS_FEAT(norecovery, NORECOVERY)
__XFS_HAS_FEAT(nouuid, NOUUID)
#define XFS_OPSTATE_UNMOUNTING		0	 
#define XFS_OPSTATE_CLEAN		1	 
#define XFS_OPSTATE_SHUTDOWN		2	 
#define XFS_OPSTATE_INODE32		3	 
#define XFS_OPSTATE_READONLY		4	 
#define XFS_OPSTATE_INODEGC_ENABLED	5
#define XFS_OPSTATE_BLOCKGC_ENABLED	6
#define XFS_OPSTATE_WARNED_SCRUB	7
#define XFS_OPSTATE_WARNED_SHRINK	8
#define XFS_OPSTATE_WARNED_LARP		9
#define XFS_OPSTATE_QUOTACHECK_RUNNING	10
#define __XFS_IS_OPSTATE(name, NAME) \
static inline bool xfs_is_ ## name (struct xfs_mount *mp) \
{ \
	return test_bit(XFS_OPSTATE_ ## NAME, &mp->m_opstate); \
} \
static inline bool xfs_clear_ ## name (struct xfs_mount *mp) \
{ \
	return test_and_clear_bit(XFS_OPSTATE_ ## NAME, &mp->m_opstate); \
} \
static inline bool xfs_set_ ## name (struct xfs_mount *mp) \
{ \
	return test_and_set_bit(XFS_OPSTATE_ ## NAME, &mp->m_opstate); \
}
__XFS_IS_OPSTATE(unmounting, UNMOUNTING)
__XFS_IS_OPSTATE(clean, CLEAN)
__XFS_IS_OPSTATE(shutdown, SHUTDOWN)
__XFS_IS_OPSTATE(inode32, INODE32)
__XFS_IS_OPSTATE(readonly, READONLY)
__XFS_IS_OPSTATE(inodegc_enabled, INODEGC_ENABLED)
__XFS_IS_OPSTATE(blockgc_enabled, BLOCKGC_ENABLED)
#ifdef CONFIG_XFS_QUOTA
__XFS_IS_OPSTATE(quotacheck_running, QUOTACHECK_RUNNING)
#else
# define xfs_is_quotacheck_running(mp)	(false)
#endif
static inline bool
xfs_should_warn(struct xfs_mount *mp, long nr)
{
	return !test_and_set_bit(nr, &mp->m_opstate);
}
#define XFS_OPSTATE_STRINGS \
	{ (1UL << XFS_OPSTATE_UNMOUNTING),		"unmounting" }, \
	{ (1UL << XFS_OPSTATE_CLEAN),			"clean" }, \
	{ (1UL << XFS_OPSTATE_SHUTDOWN),		"shutdown" }, \
	{ (1UL << XFS_OPSTATE_INODE32),			"inode32" }, \
	{ (1UL << XFS_OPSTATE_READONLY),		"read_only" }, \
	{ (1UL << XFS_OPSTATE_INODEGC_ENABLED),		"inodegc" }, \
	{ (1UL << XFS_OPSTATE_BLOCKGC_ENABLED),		"blockgc" }, \
	{ (1UL << XFS_OPSTATE_WARNED_SCRUB),		"wscrub" }, \
	{ (1UL << XFS_OPSTATE_WARNED_SHRINK),		"wshrink" }, \
	{ (1UL << XFS_OPSTATE_WARNED_LARP),		"wlarp" }, \
	{ (1UL << XFS_OPSTATE_QUOTACHECK_RUNNING),	"quotacheck" }
#define XFS_MAX_IO_LOG		30	 
#define XFS_MIN_IO_LOG		PAGE_SHIFT
void xfs_do_force_shutdown(struct xfs_mount *mp, uint32_t flags, char *fname,
		int lnnum);
#define xfs_force_shutdown(m,f)	\
	xfs_do_force_shutdown(m, f, __FILE__, __LINE__)
#define SHUTDOWN_META_IO_ERROR	(1u << 0)  
#define SHUTDOWN_LOG_IO_ERROR	(1u << 1)  
#define SHUTDOWN_FORCE_UMOUNT	(1u << 2)  
#define SHUTDOWN_CORRUPT_INCORE	(1u << 3)  
#define SHUTDOWN_CORRUPT_ONDISK	(1u << 4)   
#define SHUTDOWN_DEVICE_REMOVED	(1u << 5)  
#define XFS_SHUTDOWN_STRINGS \
	{ SHUTDOWN_META_IO_ERROR,	"metadata_io" }, \
	{ SHUTDOWN_LOG_IO_ERROR,	"log_io" }, \
	{ SHUTDOWN_FORCE_UMOUNT,	"force_umount" }, \
	{ SHUTDOWN_CORRUPT_INCORE,	"corruption" }, \
	{ SHUTDOWN_DEVICE_REMOVED,	"device_removed" }
#define XFS_MFSI_QUIET		0x40	 
static inline xfs_agnumber_t
xfs_daddr_to_agno(struct xfs_mount *mp, xfs_daddr_t d)
{
	xfs_rfsblock_t ld = XFS_BB_TO_FSBT(mp, d);
	do_div(ld, mp->m_sb.sb_agblocks);
	return (xfs_agnumber_t) ld;
}
static inline xfs_agblock_t
xfs_daddr_to_agbno(struct xfs_mount *mp, xfs_daddr_t d)
{
	xfs_rfsblock_t ld = XFS_BB_TO_FSBT(mp, d);
	return (xfs_agblock_t) do_div(ld, mp->m_sb.sb_agblocks);
}
int xfs_buf_hash_init(struct xfs_perag *pag);
void xfs_buf_hash_destroy(struct xfs_perag *pag);
extern void	xfs_uuid_table_free(void);
extern uint64_t xfs_default_resblks(xfs_mount_t *mp);
extern int	xfs_mountfs(xfs_mount_t *mp);
extern void	xfs_unmountfs(xfs_mount_t *);
#define XFS_FDBLOCKS_BATCH	1024
static inline uint64_t
xfs_fdblocks_unavailable(
	struct xfs_mount	*mp)
{
	return mp->m_alloc_set_aside + atomic64_read(&mp->m_allocbt_blks);
}
int xfs_mod_freecounter(struct xfs_mount *mp, struct percpu_counter *counter,
		int64_t delta, bool rsvd);
static inline int
xfs_mod_fdblocks(struct xfs_mount *mp, int64_t delta, bool reserved)
{
	return xfs_mod_freecounter(mp, &mp->m_fdblocks, delta, reserved);
}
static inline int
xfs_mod_frextents(struct xfs_mount *mp, int64_t delta)
{
	return xfs_mod_freecounter(mp, &mp->m_frextents, delta, false);
}
extern int	xfs_readsb(xfs_mount_t *, int);
extern void	xfs_freesb(xfs_mount_t *);
extern bool	xfs_fs_writable(struct xfs_mount *mp, int level);
extern int	xfs_sb_validate_fsb_count(struct xfs_sb *, uint64_t);
extern int	xfs_dev_is_read_only(struct xfs_mount *, char *);
extern void	xfs_set_low_space_thresholds(struct xfs_mount *);
int	xfs_zero_extent(struct xfs_inode *ip, xfs_fsblock_t start_fsb,
			xfs_off_t count_fsb);
struct xfs_error_cfg * xfs_error_get_cfg(struct xfs_mount *mp,
		int error_class, int error);
void xfs_force_summary_recalc(struct xfs_mount *mp);
int xfs_add_incompat_log_feature(struct xfs_mount *mp, uint32_t feature);
bool xfs_clear_incompat_log_features(struct xfs_mount *mp);
void xfs_mod_delalloc(struct xfs_mount *mp, int64_t delta);
#endif	 
