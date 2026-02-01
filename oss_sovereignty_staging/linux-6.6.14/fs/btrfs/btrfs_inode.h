 
 

#ifndef BTRFS_INODE_H
#define BTRFS_INODE_H

#include <linux/hash.h>
#include <linux/refcount.h>
#include "extent_map.h"
#include "extent_io.h"
#include "ordered-data.h"
#include "delayed-inode.h"

 
#define BTRFS_DIR_START_INDEX 2

 
enum {
	BTRFS_INODE_FLUSH_ON_CLOSE,
	BTRFS_INODE_DUMMY,
	BTRFS_INODE_IN_DEFRAG,
	BTRFS_INODE_HAS_ASYNC_EXTENT,
	  
	BTRFS_INODE_NEEDS_FULL_SYNC,
	BTRFS_INODE_COPY_EVERYTHING,
	BTRFS_INODE_IN_DELALLOC_LIST,
	BTRFS_INODE_HAS_PROPS,
	BTRFS_INODE_SNAPSHOT_FLUSH,
	 
	BTRFS_INODE_NO_XATTRS,
	 
	BTRFS_INODE_NO_DELALLOC_FLUSH,
	 
	BTRFS_INODE_VERITY_IN_PROGRESS,
	 
	BTRFS_INODE_FREE_SPACE_INODE,
};

 
struct btrfs_inode {
	 
	struct btrfs_root *root;

	 
	struct btrfs_key location;

	 
	spinlock_t lock;

	 
	struct extent_map_tree extent_tree;

	 
	struct extent_io_tree io_tree;

	 
	struct extent_io_tree file_extent_tree;

	 
	struct mutex log_mutex;

	 
	struct btrfs_ordered_inode_tree ordered_tree;

	 
	struct list_head delalloc_inodes;

	 
	struct rb_node rb_node;

	unsigned long runtime_flags;

	 
	u64 generation;

	 
	u64 last_trans;

	 
	u64 logged_trans;

	 
	int last_sub_trans;

	 
	int last_log_commit;

	union {
		 
		u64 delalloc_bytes;
		 
		u64 first_dir_index_to_log;
	};

	union {
		 
		u64 new_delalloc_bytes;
		 
		u64 last_dir_index_offset;
	};

	 
	u64 defrag_bytes;

	 
	u64 disk_i_size;

	 
	u64 index_cnt;

	 
	u64 dir_index;

	 
	u64 last_unlink_trans;

	 
	u64 last_reflink_trans;

	 
	u64 csum_bytes;

	 
	u32 flags;
	 
	u32 ro_flags;

	 
	unsigned outstanding_extents;

	struct btrfs_block_rsv block_rsv;

	 
	unsigned prop_compress;		 
	 
	unsigned defrag_compress;

	struct btrfs_delayed_node *delayed_node;

	 
	struct timespec64 i_otime;

	 
	struct list_head delayed_iput;

	struct rw_semaphore i_mmap_lock;
	struct inode vfs_inode;
};

static inline u64 btrfs_get_first_dir_index_to_log(const struct btrfs_inode *inode)
{
	return READ_ONCE(inode->first_dir_index_to_log);
}

static inline void btrfs_set_first_dir_index_to_log(struct btrfs_inode *inode,
						    u64 index)
{
	WRITE_ONCE(inode->first_dir_index_to_log, index);
}

static inline struct btrfs_inode *BTRFS_I(const struct inode *inode)
{
	return container_of(inode, struct btrfs_inode, vfs_inode);
}

static inline unsigned long btrfs_inode_hash(u64 objectid,
					     const struct btrfs_root *root)
{
	u64 h = objectid ^ (root->root_key.objectid * GOLDEN_RATIO_PRIME);

#if BITS_PER_LONG == 32
	h = (h >> 32) ^ (h & 0xffffffff);
#endif

	return (unsigned long)h;
}

#if BITS_PER_LONG == 32

 
static inline u64 btrfs_ino(const struct btrfs_inode *inode)
{
	u64 ino = inode->location.objectid;

	 
	if (inode->location.type == BTRFS_ROOT_ITEM_KEY)
		ino = inode->vfs_inode.i_ino;
	return ino;
}

#else

static inline u64 btrfs_ino(const struct btrfs_inode *inode)
{
	return inode->vfs_inode.i_ino;
}

#endif

static inline void btrfs_i_size_write(struct btrfs_inode *inode, u64 size)
{
	i_size_write(&inode->vfs_inode, size);
	inode->disk_i_size = size;
}

static inline bool btrfs_is_free_space_inode(struct btrfs_inode *inode)
{
	return test_bit(BTRFS_INODE_FREE_SPACE_INODE, &inode->runtime_flags);
}

static inline bool is_data_inode(struct inode *inode)
{
	return btrfs_ino(BTRFS_I(inode)) != BTRFS_BTREE_INODE_OBJECTID;
}

static inline void btrfs_mod_outstanding_extents(struct btrfs_inode *inode,
						 int mod)
{
	lockdep_assert_held(&inode->lock);
	inode->outstanding_extents += mod;
	if (btrfs_is_free_space_inode(inode))
		return;
	trace_btrfs_inode_mod_outstanding_extents(inode->root, btrfs_ino(inode),
						  mod, inode->outstanding_extents);
}

 
static inline void btrfs_set_inode_last_sub_trans(struct btrfs_inode *inode)
{
	spin_lock(&inode->lock);
	inode->last_sub_trans = inode->root->log_transid;
	spin_unlock(&inode->lock);
}

 
static inline void btrfs_set_inode_full_sync(struct btrfs_inode *inode)
{
	set_bit(BTRFS_INODE_NEEDS_FULL_SYNC, &inode->runtime_flags);
	 
	spin_lock(&inode->lock);
	if (inode->last_reflink_trans < inode->last_trans)
		inode->last_reflink_trans = inode->last_trans;
	spin_unlock(&inode->lock);
}

static inline bool btrfs_inode_in_log(struct btrfs_inode *inode, u64 generation)
{
	bool ret = false;

	spin_lock(&inode->lock);
	if (inode->logged_trans == generation &&
	    inode->last_sub_trans <= inode->last_log_commit &&
	    inode->last_sub_trans <= inode->root->last_log_commit)
		ret = true;
	spin_unlock(&inode->lock);
	return ret;
}

 
static inline bool btrfs_inode_can_compress(const struct btrfs_inode *inode)
{
	if (inode->flags & BTRFS_INODE_NODATACOW ||
	    inode->flags & BTRFS_INODE_NODATASUM)
		return false;
	return true;
}

 
#define CSUM_FMT				"0x%*phN"
#define CSUM_FMT_VALUE(size, bytes)		size, bytes

int btrfs_check_sector_csum(struct btrfs_fs_info *fs_info, struct page *page,
			    u32 pgoff, u8 *csum, const u8 * const csum_expected);
bool btrfs_data_csum_ok(struct btrfs_bio *bbio, struct btrfs_device *dev,
			u32 bio_offset, struct bio_vec *bv);
noinline int can_nocow_extent(struct inode *inode, u64 offset, u64 *len,
			      u64 *orig_start, u64 *orig_block_len,
			      u64 *ram_bytes, bool nowait, bool strict);

void __btrfs_del_delalloc_inode(struct btrfs_root *root, struct btrfs_inode *inode);
struct inode *btrfs_lookup_dentry(struct inode *dir, struct dentry *dentry);
int btrfs_set_inode_index(struct btrfs_inode *dir, u64 *index);
int btrfs_unlink_inode(struct btrfs_trans_handle *trans,
		       struct btrfs_inode *dir, struct btrfs_inode *inode,
		       const struct fscrypt_str *name);
int btrfs_add_link(struct btrfs_trans_handle *trans,
		   struct btrfs_inode *parent_inode, struct btrfs_inode *inode,
		   const struct fscrypt_str *name, int add_backref, u64 index);
int btrfs_delete_subvolume(struct btrfs_inode *dir, struct dentry *dentry);
int btrfs_truncate_block(struct btrfs_inode *inode, loff_t from, loff_t len,
			 int front);

int btrfs_start_delalloc_snapshot(struct btrfs_root *root, bool in_reclaim_context);
int btrfs_start_delalloc_roots(struct btrfs_fs_info *fs_info, long nr,
			       bool in_reclaim_context);
int btrfs_set_extent_delalloc(struct btrfs_inode *inode, u64 start, u64 end,
			      unsigned int extra_bits,
			      struct extent_state **cached_state);

struct btrfs_new_inode_args {
	 
	struct inode *dir;
	struct dentry *dentry;
	struct inode *inode;
	bool orphan;
	bool subvol;

	 
	struct posix_acl *default_acl;
	struct posix_acl *acl;
	struct fscrypt_name fname;
};

int btrfs_new_inode_prepare(struct btrfs_new_inode_args *args,
			    unsigned int *trans_num_items);
int btrfs_create_new_inode(struct btrfs_trans_handle *trans,
			   struct btrfs_new_inode_args *args);
void btrfs_new_inode_args_destroy(struct btrfs_new_inode_args *args);
struct inode *btrfs_new_subvol_inode(struct mnt_idmap *idmap,
				     struct inode *dir);
 void btrfs_set_delalloc_extent(struct btrfs_inode *inode, struct extent_state *state,
			        u32 bits);
void btrfs_clear_delalloc_extent(struct btrfs_inode *inode,
				 struct extent_state *state, u32 bits);
void btrfs_merge_delalloc_extent(struct btrfs_inode *inode, struct extent_state *new,
				 struct extent_state *other);
void btrfs_split_delalloc_extent(struct btrfs_inode *inode,
				 struct extent_state *orig, u64 split);
void btrfs_set_range_writeback(struct btrfs_inode *inode, u64 start, u64 end);
vm_fault_t btrfs_page_mkwrite(struct vm_fault *vmf);
void btrfs_evict_inode(struct inode *inode);
struct inode *btrfs_alloc_inode(struct super_block *sb);
void btrfs_destroy_inode(struct inode *inode);
void btrfs_free_inode(struct inode *inode);
int btrfs_drop_inode(struct inode *inode);
int __init btrfs_init_cachep(void);
void __cold btrfs_destroy_cachep(void);
struct inode *btrfs_iget_path(struct super_block *s, u64 ino,
			      struct btrfs_root *root, struct btrfs_path *path);
struct inode *btrfs_iget(struct super_block *s, u64 ino, struct btrfs_root *root);
struct extent_map *btrfs_get_extent(struct btrfs_inode *inode,
				    struct page *page, size_t pg_offset,
				    u64 start, u64 end);
int btrfs_update_inode(struct btrfs_trans_handle *trans,
		       struct btrfs_root *root, struct btrfs_inode *inode);
int btrfs_update_inode_fallback(struct btrfs_trans_handle *trans,
				struct btrfs_root *root, struct btrfs_inode *inode);
int btrfs_orphan_add(struct btrfs_trans_handle *trans, struct btrfs_inode *inode);
int btrfs_orphan_cleanup(struct btrfs_root *root);
int btrfs_cont_expand(struct btrfs_inode *inode, loff_t oldsize, loff_t size);
void btrfs_add_delayed_iput(struct btrfs_inode *inode);
void btrfs_run_delayed_iputs(struct btrfs_fs_info *fs_info);
int btrfs_wait_on_delayed_iputs(struct btrfs_fs_info *fs_info);
int btrfs_prealloc_file_range(struct inode *inode, int mode,
			      u64 start, u64 num_bytes, u64 min_size,
			      loff_t actual_len, u64 *alloc_hint);
int btrfs_prealloc_file_range_trans(struct inode *inode,
				    struct btrfs_trans_handle *trans, int mode,
				    u64 start, u64 num_bytes, u64 min_size,
				    loff_t actual_len, u64 *alloc_hint);
int btrfs_run_delalloc_range(struct btrfs_inode *inode, struct page *locked_page,
			     u64 start, u64 end, struct writeback_control *wbc);
int btrfs_writepage_cow_fixup(struct page *page);
int btrfs_encoded_io_compression_from_extent(struct btrfs_fs_info *fs_info,
					     int compress_type);
int btrfs_encoded_read_regular_fill_pages(struct btrfs_inode *inode,
					  u64 file_offset, u64 disk_bytenr,
					  u64 disk_io_size,
					  struct page **pages);
ssize_t btrfs_encoded_read(struct kiocb *iocb, struct iov_iter *iter,
			   struct btrfs_ioctl_encoded_io_args *encoded);
ssize_t btrfs_do_encoded_write(struct kiocb *iocb, struct iov_iter *from,
			       const struct btrfs_ioctl_encoded_io_args *encoded);

ssize_t btrfs_dio_read(struct kiocb *iocb, struct iov_iter *iter,
		       size_t done_before);
struct iomap_dio *btrfs_dio_write(struct kiocb *iocb, struct iov_iter *iter,
				  size_t done_before);

extern const struct dentry_operations btrfs_dentry_operations;

 
enum btrfs_ilock_type {
	ENUM_BIT(BTRFS_ILOCK_SHARED),
	ENUM_BIT(BTRFS_ILOCK_TRY),
	ENUM_BIT(BTRFS_ILOCK_MMAP),
};

int btrfs_inode_lock(struct btrfs_inode *inode, unsigned int ilock_flags);
void btrfs_inode_unlock(struct btrfs_inode *inode, unsigned int ilock_flags);
void btrfs_update_inode_bytes(struct btrfs_inode *inode, const u64 add_bytes,
			      const u64 del_bytes);
void btrfs_assert_inode_range_clean(struct btrfs_inode *inode, u64 start, u64 end);

#endif
