 
 

#ifndef _NILFS_H
#define _NILFS_H

#include <linux/kernel.h>
#include <linux/buffer_head.h>
#include <linux/spinlock.h>
#include <linux/blkdev.h>
#include <linux/nilfs2_api.h>
#include <linux/nilfs2_ondisk.h>
#include "the_nilfs.h"
#include "bmap.h"

 
struct nilfs_inode_info {
	__u32 i_flags;
	unsigned long  i_state;		 
	struct nilfs_bmap *i_bmap;
	struct nilfs_bmap i_bmap_data;
	__u64 i_xattr;	 
	__u32 i_dir_start_lookup;
	__u64 i_cno;		 
	struct inode *i_assoc_inode;
	struct list_head i_dirty;	 

#ifdef CONFIG_NILFS_XATTR
	 
	struct rw_semaphore xattr_sem;
#endif
	struct buffer_head *i_bh;	 
	struct nilfs_root *i_root;
	struct inode vfs_inode;
};

static inline struct nilfs_inode_info *NILFS_I(const struct inode *inode)
{
	return container_of(inode, struct nilfs_inode_info, vfs_inode);
}

static inline struct nilfs_inode_info *
NILFS_BMAP_I(const struct nilfs_bmap *bmap)
{
	return container_of(bmap, struct nilfs_inode_info, i_bmap_data);
}

 
enum {
	NILFS_I_NEW = 0,		 
	NILFS_I_DIRTY,			 
	NILFS_I_QUEUED,			 
	NILFS_I_BUSY,			 
	NILFS_I_COLLECTED,		 
	NILFS_I_UPDATED,		 
	NILFS_I_INODE_SYNC,		 
	NILFS_I_BMAP,			 
	NILFS_I_GCINODE,		 
	NILFS_I_BTNC,			 
	NILFS_I_SHADOW,			 
};

 
enum {
	NILFS_SB_COMMIT = 0,	 
	NILFS_SB_COMMIT_ALL	 
};

 
#define NILFS_MDT_INO_BITS						\
	(BIT(NILFS_DAT_INO) | BIT(NILFS_CPFILE_INO) |			\
	 BIT(NILFS_SUFILE_INO) | BIT(NILFS_IFILE_INO) |			\
	 BIT(NILFS_ATIME_INO) | BIT(NILFS_SKETCH_INO))

#define NILFS_SYS_INO_BITS (BIT(NILFS_ROOT_INO) | NILFS_MDT_INO_BITS)

#define NILFS_FIRST_INO(sb) (((struct the_nilfs *)sb->s_fs_info)->ns_first_ino)

#define NILFS_MDT_INODE(sb, ino) \
	((ino) < NILFS_FIRST_INO(sb) && (NILFS_MDT_INO_BITS & BIT(ino)))
#define NILFS_VALID_INODE(sb, ino) \
	((ino) >= NILFS_FIRST_INO(sb) || (NILFS_SYS_INO_BITS & BIT(ino)))

 
struct nilfs_transaction_info {
	u32			ti_magic;
	void		       *ti_save;
				 
	unsigned short		ti_flags;
	unsigned short		ti_count;
};

 
#define NILFS_TI_MAGIC		0xd9e392fb

 
#define NILFS_TI_DYNAMIC_ALLOC	0x0001   
#define NILFS_TI_SYNC		0x0002	 
#define NILFS_TI_GC		0x0004	 
#define NILFS_TI_COMMIT		0x0008	 
#define NILFS_TI_WRITER		0x0010	 


int nilfs_transaction_begin(struct super_block *,
			    struct nilfs_transaction_info *, int);
int nilfs_transaction_commit(struct super_block *);
void nilfs_transaction_abort(struct super_block *);

static inline void nilfs_set_transaction_flag(unsigned int flag)
{
	struct nilfs_transaction_info *ti = current->journal_info;

	ti->ti_flags |= flag;
}

static inline int nilfs_test_transaction_flag(unsigned int flag)
{
	struct nilfs_transaction_info *ti = current->journal_info;

	if (ti == NULL || ti->ti_magic != NILFS_TI_MAGIC)
		return 0;
	return !!(ti->ti_flags & flag);
}

static inline int nilfs_doing_gc(void)
{
	return nilfs_test_transaction_flag(NILFS_TI_GC);
}

static inline int nilfs_doing_construction(void)
{
	return nilfs_test_transaction_flag(NILFS_TI_WRITER);
}

 
#ifdef CONFIG_NILFS_POSIX_ACL
#error "NILFS: not yet supported POSIX ACL"
extern int nilfs_acl_chmod(struct inode *);
extern int nilfs_init_acl(struct inode *, struct inode *);
#else
static inline int nilfs_acl_chmod(struct inode *inode)
{
	return 0;
}

static inline int nilfs_init_acl(struct inode *inode, struct inode *dir)
{
	if (S_ISLNK(inode->i_mode))
		return 0;

	inode->i_mode &= ~current_umask();
	return 0;
}
#endif

#define NILFS_ATIME_DISABLE

 
#define NILFS_FL_INHERITED						\
	(FS_SECRM_FL | FS_UNRM_FL | FS_COMPR_FL | FS_SYNC_FL |		\
	 FS_IMMUTABLE_FL | FS_APPEND_FL | FS_NODUMP_FL | FS_NOATIME_FL |\
	 FS_COMPRBLK_FL | FS_NOCOMP_FL | FS_NOTAIL_FL | FS_DIRSYNC_FL)

 
static inline __u32 nilfs_mask_flags(umode_t mode, __u32 flags)
{
	if (S_ISDIR(mode))
		return flags;
	else if (S_ISREG(mode))
		return flags & ~(FS_DIRSYNC_FL | FS_TOPDIR_FL);
	else
		return flags & (FS_NODUMP_FL | FS_NOATIME_FL);
}

 
extern int nilfs_add_link(struct dentry *, struct inode *);
extern ino_t nilfs_inode_by_name(struct inode *, const struct qstr *);
extern int nilfs_make_empty(struct inode *, struct inode *);
extern struct nilfs_dir_entry *
nilfs_find_entry(struct inode *, const struct qstr *, struct page **);
extern int nilfs_delete_entry(struct nilfs_dir_entry *, struct page *);
extern int nilfs_empty_dir(struct inode *);
extern struct nilfs_dir_entry *nilfs_dotdot(struct inode *, struct page **);
extern void nilfs_set_link(struct inode *, struct nilfs_dir_entry *,
			   struct page *, struct inode *);

 
extern int nilfs_sync_file(struct file *, loff_t, loff_t, int);

 
int nilfs_fileattr_get(struct dentry *dentry, struct fileattr *m);
int nilfs_fileattr_set(struct mnt_idmap *idmap,
		       struct dentry *dentry, struct fileattr *fa);
long nilfs_ioctl(struct file *, unsigned int, unsigned long);
long nilfs_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
int nilfs_ioctl_prepare_clean_segments(struct the_nilfs *, struct nilfs_argv *,
				       void **);

 
void nilfs_inode_add_blocks(struct inode *inode, int n);
void nilfs_inode_sub_blocks(struct inode *inode, int n);
extern struct inode *nilfs_new_inode(struct inode *, umode_t);
extern int nilfs_get_block(struct inode *, sector_t, struct buffer_head *, int);
extern void nilfs_set_inode_flags(struct inode *);
extern int nilfs_read_inode_common(struct inode *, struct nilfs_inode *);
extern void nilfs_write_inode_common(struct inode *, struct nilfs_inode *, int);
struct inode *nilfs_ilookup(struct super_block *sb, struct nilfs_root *root,
			    unsigned long ino);
struct inode *nilfs_iget_locked(struct super_block *sb, struct nilfs_root *root,
				unsigned long ino);
struct inode *nilfs_iget(struct super_block *sb, struct nilfs_root *root,
			 unsigned long ino);
extern struct inode *nilfs_iget_for_gc(struct super_block *sb,
				       unsigned long ino, __u64 cno);
int nilfs_attach_btree_node_cache(struct inode *inode);
void nilfs_detach_btree_node_cache(struct inode *inode);
struct inode *nilfs_iget_for_shadow(struct inode *inode);
extern void nilfs_update_inode(struct inode *, struct buffer_head *, int);
extern void nilfs_truncate(struct inode *);
extern void nilfs_evict_inode(struct inode *);
extern int nilfs_setattr(struct mnt_idmap *, struct dentry *,
			 struct iattr *);
extern void nilfs_write_failed(struct address_space *mapping, loff_t to);
int nilfs_permission(struct mnt_idmap *idmap, struct inode *inode,
		     int mask);
int nilfs_load_inode_block(struct inode *inode, struct buffer_head **pbh);
extern int nilfs_inode_dirty(struct inode *);
int nilfs_set_file_dirty(struct inode *inode, unsigned int nr_dirty);
extern int __nilfs_mark_inode_dirty(struct inode *, int);
extern void nilfs_dirty_inode(struct inode *, int flags);
int nilfs_fiemap(struct inode *inode, struct fiemap_extent_info *fieinfo,
		 __u64 start, __u64 len);
static inline int nilfs_mark_inode_dirty(struct inode *inode)
{
	return __nilfs_mark_inode_dirty(inode, I_DIRTY);
}
static inline int nilfs_mark_inode_dirty_sync(struct inode *inode)
{
	return __nilfs_mark_inode_dirty(inode, I_DIRTY_SYNC);
}

 
extern struct inode *nilfs_alloc_inode(struct super_block *);

__printf(2, 3)
void __nilfs_msg(struct super_block *sb, const char *fmt, ...);
extern __printf(3, 4)
void __nilfs_error(struct super_block *sb, const char *function,
		   const char *fmt, ...);

#ifdef CONFIG_PRINTK

#define nilfs_msg(sb, level, fmt, ...)					\
	__nilfs_msg(sb, level fmt, ##__VA_ARGS__)
#define nilfs_error(sb, fmt, ...)					\
	__nilfs_error(sb, __func__, fmt, ##__VA_ARGS__)

#else

#define nilfs_msg(sb, level, fmt, ...)					\
	do {								\
		no_printk(level fmt, ##__VA_ARGS__);			\
		(void)(sb);						\
	} while (0)
#define nilfs_error(sb, fmt, ...)					\
	do {								\
		no_printk(fmt, ##__VA_ARGS__);				\
		__nilfs_error(sb, "", " ");				\
	} while (0)

#endif  

#define nilfs_crit(sb, fmt, ...)					\
	nilfs_msg(sb, KERN_CRIT, fmt, ##__VA_ARGS__)
#define nilfs_err(sb, fmt, ...)						\
	nilfs_msg(sb, KERN_ERR, fmt, ##__VA_ARGS__)
#define nilfs_warn(sb, fmt, ...)					\
	nilfs_msg(sb, KERN_WARNING, fmt, ##__VA_ARGS__)
#define nilfs_info(sb, fmt, ...)					\
	nilfs_msg(sb, KERN_INFO, fmt, ##__VA_ARGS__)

extern struct nilfs_super_block *
nilfs_read_super_block(struct super_block *, u64, int, struct buffer_head **);
extern int nilfs_store_magic_and_option(struct super_block *,
					struct nilfs_super_block *, char *);
extern int nilfs_check_feature_compatibility(struct super_block *,
					     struct nilfs_super_block *);
extern void nilfs_set_log_cursor(struct nilfs_super_block *,
				 struct the_nilfs *);
struct nilfs_super_block **nilfs_prepare_super(struct super_block *sb,
					       int flip);
int nilfs_commit_super(struct super_block *sb, int flag);
int nilfs_cleanup_super(struct super_block *sb);
int nilfs_resize_fs(struct super_block *sb, __u64 newsize);
int nilfs_attach_checkpoint(struct super_block *sb, __u64 cno, int curr_mnt,
			    struct nilfs_root **root);
int nilfs_checkpoint_is_mounted(struct super_block *sb, __u64 cno);

 
int nilfs_gccache_submit_read_data(struct inode *, sector_t, sector_t, __u64,
				   struct buffer_head **);
int nilfs_gccache_submit_read_node(struct inode *, sector_t, __u64,
				   struct buffer_head **);
int nilfs_gccache_wait_and_mark_dirty(struct buffer_head *);
int nilfs_init_gcinode(struct inode *inode);
void nilfs_remove_all_gcinodes(struct the_nilfs *nilfs);

 
int __init nilfs_sysfs_init(void);
void nilfs_sysfs_exit(void);
int nilfs_sysfs_create_device_group(struct super_block *);
void nilfs_sysfs_delete_device_group(struct the_nilfs *);
int nilfs_sysfs_create_snapshot_group(struct nilfs_root *);
void nilfs_sysfs_delete_snapshot_group(struct nilfs_root *);

 
extern const struct file_operations nilfs_dir_operations;
extern const struct inode_operations nilfs_file_inode_operations;
extern const struct file_operations nilfs_file_operations;
extern const struct address_space_operations nilfs_aops;
extern const struct inode_operations nilfs_dir_inode_operations;
extern const struct inode_operations nilfs_special_inode_operations;
extern const struct inode_operations nilfs_symlink_inode_operations;

 
extern struct file_system_type nilfs_fs_type;


#endif	 
