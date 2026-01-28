#ifndef OCFS2_INODE_H
#define OCFS2_INODE_H
#include "extent_map.h"
struct ocfs2_inode_info
{
	u64			ip_blkno;
	struct ocfs2_lock_res		ip_rw_lockres;
	struct ocfs2_lock_res		ip_inode_lockres;
	struct ocfs2_lock_res		ip_open_lockres;
	struct rw_semaphore		ip_alloc_sem;
	struct rw_semaphore		ip_xattr_sem;
	spinlock_t			ip_lock;
	u32				ip_open_count;
	struct list_head		ip_io_markers;
	u32				ip_clusters;
	u16				ip_dyn_features;
	struct mutex			ip_io_mutex;
	u32				ip_flags;  
	u32				ip_attr;  
	struct list_head		ip_unwritten_list;
	struct inode			*ip_next_orphan;
	struct ocfs2_caching_info	ip_metadata_cache;
	struct ocfs2_extent_map		ip_extent_map;
	struct inode			vfs_inode;
	struct jbd2_inode		ip_jinode;
	u32				ip_dir_start_lookup;
	u32				ip_last_used_slot;
	u64				ip_last_used_group;
	u32				ip_dir_lock_gen;
	struct ocfs2_alloc_reservation	ip_la_data_resv;
	tid_t i_sync_tid;
	tid_t i_datasync_tid;
	struct dquot *i_dquot[MAXQUOTAS];
};
#define OCFS2_INODE_SYSTEM_FILE		0x00000001
#define OCFS2_INODE_JOURNAL		0x00000002
#define OCFS2_INODE_BITMAP		0x00000004
#define OCFS2_INODE_DELETED		0x00000008
#define OCFS2_INODE_MAYBE_ORPHANED	0x00000010
#define OCFS2_INODE_OPEN_DIRECT		0x00000020
#define OCFS2_INODE_SKIP_ORPHAN_DIR     0x00000040
#define OCFS2_INODE_DIO_ORPHAN_ENTRY	0x00000080
static inline struct ocfs2_inode_info *OCFS2_I(struct inode *inode)
{
	return container_of(inode, struct ocfs2_inode_info, vfs_inode);
}
#define INODE_JOURNAL(i) (OCFS2_I(i)->ip_flags & OCFS2_INODE_JOURNAL)
#define SET_INODE_JOURNAL(i) (OCFS2_I(i)->ip_flags |= OCFS2_INODE_JOURNAL)
extern const struct address_space_operations ocfs2_aops;
extern const struct ocfs2_caching_operations ocfs2_inode_caching_ops;
static inline struct ocfs2_caching_info *INODE_CACHE(struct inode *inode)
{
	return &OCFS2_I(inode)->ip_metadata_cache;
}
void ocfs2_evict_inode(struct inode *inode);
int ocfs2_drop_inode(struct inode *inode);
#define OCFS2_FI_FLAG_SYSFILE		0x1
#define OCFS2_FI_FLAG_ORPHAN_RECOVERY	0x2
#define OCFS2_FI_FLAG_FILECHECK_CHK	0x4
#define OCFS2_FI_FLAG_FILECHECK_FIX	0x8
struct inode *ocfs2_ilookup(struct super_block *sb, u64 feoff);
struct inode *ocfs2_iget(struct ocfs2_super *osb, u64 feoff, unsigned flags,
			 int sysfile_type);
int ocfs2_inode_revalidate(struct dentry *dentry);
void ocfs2_populate_inode(struct inode *inode, struct ocfs2_dinode *fe,
			  int create_ino);
void ocfs2_sync_blockdev(struct super_block *sb);
void ocfs2_refresh_inode(struct inode *inode,
			 struct ocfs2_dinode *fe);
int ocfs2_mark_inode_dirty(handle_t *handle,
			   struct inode *inode,
			   struct buffer_head *bh);
void ocfs2_set_inode_flags(struct inode *inode);
void ocfs2_get_inode_flags(struct ocfs2_inode_info *oi);
static inline blkcnt_t ocfs2_inode_sector_count(struct inode *inode)
{
	int c_to_s_bits = OCFS2_SB(inode->i_sb)->s_clustersize_bits - 9;
	return (blkcnt_t)OCFS2_I(inode)->ip_clusters << c_to_s_bits;
}
int ocfs2_validate_inode_block(struct super_block *sb,
			       struct buffer_head *bh);
int ocfs2_read_inode_block(struct inode *inode, struct buffer_head **bh);
int ocfs2_read_inode_block_full(struct inode *inode, struct buffer_head **bh,
				int flags);
static inline struct ocfs2_inode_info *cache_info_to_inode(struct ocfs2_caching_info *ci)
{
	return container_of(ci, struct ocfs2_inode_info, ip_metadata_cache);
}
static inline bool ocfs2_is_refcount_inode(struct inode *inode)
{
	return (OCFS2_I(inode)->ip_dyn_features & OCFS2_HAS_REFCOUNT_FL);
}
#endif  
