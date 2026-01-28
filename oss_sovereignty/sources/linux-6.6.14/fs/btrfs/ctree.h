


#ifndef BTRFS_CTREE_H
#define BTRFS_CTREE_H

#include <linux/mm.h>
#include <linux/sched/signal.h>
#include <linux/highmem.h>
#include <linux/fs.h>
#include <linux/rwsem.h>
#include <linux/semaphore.h>
#include <linux/completion.h>
#include <linux/backing-dev.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <trace/events/btrfs.h>
#include <asm/unaligned.h>
#include <linux/pagemap.h>
#include <linux/btrfs.h>
#include <linux/btrfs_tree.h>
#include <linux/workqueue.h>
#include <linux/security.h>
#include <linux/sizes.h>
#include <linux/dynamic_debug.h>
#include <linux/refcount.h>
#include <linux/crc32c.h>
#include <linux/iomap.h>
#include <linux/fscrypt.h>
#include "extent-io-tree.h"
#include "extent_io.h"
#include "extent_map.h"
#include "async-thread.h"
#include "block-rsv.h"
#include "locking.h"
#include "misc.h"
#include "fs.h"

struct btrfs_trans_handle;
struct btrfs_transaction;
struct btrfs_pending_snapshot;
struct btrfs_delayed_ref_root;
struct btrfs_space_info;
struct btrfs_block_group;
struct btrfs_ordered_sum;
struct btrfs_ref;
struct btrfs_bio;
struct btrfs_ioctl_encoded_io_args;
struct btrfs_device;
struct btrfs_fs_devices;
struct btrfs_balance_control;
struct btrfs_delayed_root;
struct reloc_control;


enum {
	READA_NONE,
	READA_BACK,
	READA_FORWARD,
	
	READA_FORWARD_ALWAYS,
};


struct btrfs_path {
	struct extent_buffer *nodes[BTRFS_MAX_LEVEL];
	int slots[BTRFS_MAX_LEVEL];
	
	u8 locks[BTRFS_MAX_LEVEL];
	u8 reada;
	
	u8 lowest_level;

	
	unsigned int search_for_split:1;
	unsigned int keep_locks:1;
	unsigned int skip_locking:1;
	unsigned int search_commit_root:1;
	unsigned int need_commit_sem:1;
	unsigned int skip_release_on_error:1;
	
	unsigned int search_for_extension:1;
	
	unsigned int nowait:1;
};


enum {
	
	BTRFS_ROOT_IN_TRANS_SETUP,

	
	BTRFS_ROOT_SHAREABLE,
	BTRFS_ROOT_TRACK_DIRTY,
	BTRFS_ROOT_IN_RADIX,
	BTRFS_ROOT_ORPHAN_ITEM_INSERTED,
	BTRFS_ROOT_DEFRAG_RUNNING,
	BTRFS_ROOT_FORCE_COW,
	BTRFS_ROOT_MULTI_LOG_TASKS,
	BTRFS_ROOT_DIRTY,
	BTRFS_ROOT_DELETING,

	
	BTRFS_ROOT_DEAD_RELOC_TREE,
	
	BTRFS_ROOT_DEAD_TREE,
	
	BTRFS_ROOT_HAS_LOG_TREE,
	
	BTRFS_ROOT_QGROUP_FLUSHING,
	
	BTRFS_ROOT_ORPHAN_CLEANUP,
	
	BTRFS_ROOT_UNFINISHED_DROP,
	
	BTRFS_ROOT_RESET_LOCKDEP_CLASS,
};


struct btrfs_qgroup_swapped_blocks {
	spinlock_t lock;
	
	bool swapped;
	struct rb_root blocks[BTRFS_MAX_LEVEL];
};


struct btrfs_root {
	struct rb_node rb_node;

	struct extent_buffer *node;

	struct extent_buffer *commit_root;
	struct btrfs_root *log_root;
	struct btrfs_root *reloc_root;

	unsigned long state;
	struct btrfs_root_item root_item;
	struct btrfs_key root_key;
	struct btrfs_fs_info *fs_info;
	struct extent_io_tree dirty_log_pages;

	struct mutex objectid_mutex;

	spinlock_t accounting_lock;
	struct btrfs_block_rsv *block_rsv;

	struct mutex log_mutex;
	wait_queue_head_t log_writer_wait;
	wait_queue_head_t log_commit_wait[2];
	struct list_head log_ctxs[2];
	
	atomic_t log_writers;
	atomic_t log_commit[2];
	
	atomic_t log_batch;
	int log_transid;
	
	int log_transid_committed;
	
	int last_log_commit;
	pid_t log_start_pid;

	u64 last_trans;

	u32 type;

	u64 free_objectid;

	struct btrfs_key defrag_progress;
	struct btrfs_key defrag_max;

	
	struct list_head dirty_list;

	struct list_head root_list;

	spinlock_t log_extents_lock[2];
	struct list_head logged_list[2];

	spinlock_t inode_lock;
	
	struct rb_root inode_tree;

	
	struct radix_tree_root delayed_nodes_tree;
	
	dev_t anon_dev;

	spinlock_t root_item_lock;
	refcount_t refs;

	struct mutex delalloc_mutex;
	spinlock_t delalloc_lock;
	
	struct list_head delalloc_inodes;
	struct list_head delalloc_root;
	u64 nr_delalloc_inodes;

	struct mutex ordered_extent_mutex;
	
	spinlock_t ordered_extent_lock;

	
	struct list_head ordered_extents;
	struct list_head ordered_root;
	u64 nr_ordered_extents;

	
	struct list_head reloc_dirty_list;

	
	int send_in_progress;
	
	int dedupe_in_progress;
	
	struct btrfs_drew_lock snapshot_lock;

	atomic_t snapshot_force_cow;

	
	spinlock_t qgroup_meta_rsv_lock;
	u64 qgroup_meta_rsv_pertrans;
	u64 qgroup_meta_rsv_prealloc;
	wait_queue_head_t qgroup_flush_wait;

	
	atomic_t nr_swapfiles;

	
	struct btrfs_qgroup_swapped_blocks swapped_blocks;

	
	struct extent_io_tree log_csum_range;

#ifdef CONFIG_BTRFS_FS_RUN_SANITY_TESTS
	u64 alloc_bytenr;
#endif

#ifdef CONFIG_BTRFS_DEBUG
	struct list_head leak_list;
#endif
};

static inline bool btrfs_root_readonly(const struct btrfs_root *root)
{
	
	return (root->root_item.flags & cpu_to_le64(BTRFS_ROOT_SUBVOL_RDONLY)) != 0;
}

static inline bool btrfs_root_dead(const struct btrfs_root *root)
{
	
	return (root->root_item.flags & cpu_to_le64(BTRFS_ROOT_SUBVOL_DEAD)) != 0;
}

static inline u64 btrfs_root_id(const struct btrfs_root *root)
{
	return root->root_key.objectid;
}


struct btrfs_replace_extent_info {
	u64 disk_offset;
	u64 disk_len;
	u64 data_offset;
	u64 data_len;
	u64 file_offset;
	
	char *extent_buf;
	
	bool is_new_extent;
	
	bool update_times;
	
	int qgroup_reserved;
	
	int insertions;
};


struct btrfs_drop_extents_args {
	

	
	struct btrfs_path *path;
	
	u64 start;
	
	u64 end;
	
	bool drop_cache;
	
	bool replace_extent;
	
	u32 extent_item_size;

	

	
	u64 drop_end;
	
	u64 bytes_found;
	
	bool extent_inserted;
};

struct btrfs_file_private {
	void *filldir_buf;
	u64 last_index;
	struct extent_state *llseek_cached_state;
};

static inline u32 BTRFS_LEAF_DATA_SIZE(const struct btrfs_fs_info *info)
{
	return info->nodesize - sizeof(struct btrfs_header);
}

static inline u32 BTRFS_MAX_ITEM_SIZE(const struct btrfs_fs_info *info)
{
	return BTRFS_LEAF_DATA_SIZE(info) - sizeof(struct btrfs_item);
}

static inline u32 BTRFS_NODEPTRS_PER_BLOCK(const struct btrfs_fs_info *info)
{
	return BTRFS_LEAF_DATA_SIZE(info) / sizeof(struct btrfs_key_ptr);
}

static inline u32 BTRFS_MAX_XATTR_SIZE(const struct btrfs_fs_info *info)
{
	return BTRFS_MAX_ITEM_SIZE(info) - sizeof(struct btrfs_dir_item);
}

#define BTRFS_BYTES_TO_BLKS(fs_info, bytes) \
				((bytes) >> (fs_info)->sectorsize_bits)

static inline u32 btrfs_crc32c(u32 crc, const void *address, unsigned length)
{
	return crc32c(crc, address, length);
}

static inline void btrfs_crc32c_final(u32 crc, u8 *result)
{
	put_unaligned_le32(~crc, result);
}

static inline u64 btrfs_name_hash(const char *name, int len)
{
       return crc32c((u32)~1, name, len);
}


static inline u64 btrfs_extref_hash(u64 parent_objectid, const char *name,
                                   int len)
{
       return (u64) crc32c(parent_objectid, name, len);
}

static inline gfp_t btrfs_alloc_write_mask(struct address_space *mapping)
{
	return mapping_gfp_constraint(mapping, ~__GFP_FS);
}

int btrfs_error_unpin_extent_range(struct btrfs_fs_info *fs_info,
				   u64 start, u64 end);
int btrfs_discard_extent(struct btrfs_fs_info *fs_info, u64 bytenr,
			 u64 num_bytes, u64 *actual_bytes);
int btrfs_trim_fs(struct btrfs_fs_info *fs_info, struct fstrim_range *range);


int __init btrfs_ctree_init(void);
void __cold btrfs_ctree_exit(void);

int btrfs_bin_search(struct extent_buffer *eb, int first_slot,
		     const struct btrfs_key *key, int *slot);

int __pure btrfs_comp_cpu_keys(const struct btrfs_key *k1, const struct btrfs_key *k2);
int btrfs_previous_item(struct btrfs_root *root,
			struct btrfs_path *path, u64 min_objectid,
			int type);
int btrfs_previous_extent_item(struct btrfs_root *root,
			struct btrfs_path *path, u64 min_objectid);
void btrfs_set_item_key_safe(struct btrfs_trans_handle *trans,
			     struct btrfs_path *path,
			     const struct btrfs_key *new_key);
struct extent_buffer *btrfs_root_node(struct btrfs_root *root);
int btrfs_find_next_key(struct btrfs_root *root, struct btrfs_path *path,
			struct btrfs_key *key, int lowest_level,
			u64 min_trans);
int btrfs_search_forward(struct btrfs_root *root, struct btrfs_key *min_key,
			 struct btrfs_path *path,
			 u64 min_trans);
struct extent_buffer *btrfs_read_node_slot(struct extent_buffer *parent,
					   int slot);

int btrfs_cow_block(struct btrfs_trans_handle *trans,
		    struct btrfs_root *root, struct extent_buffer *buf,
		    struct extent_buffer *parent, int parent_slot,
		    struct extent_buffer **cow_ret,
		    enum btrfs_lock_nesting nest);
int btrfs_copy_root(struct btrfs_trans_handle *trans,
		      struct btrfs_root *root,
		      struct extent_buffer *buf,
		      struct extent_buffer **cow_ret, u64 new_root_objectid);
int btrfs_block_can_be_shared(struct btrfs_trans_handle *trans,
			      struct btrfs_root *root,
			      struct extent_buffer *buf);
int btrfs_del_ptr(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		  struct btrfs_path *path, int level, int slot);
void btrfs_extend_item(struct btrfs_trans_handle *trans,
		       struct btrfs_path *path, u32 data_size);
void btrfs_truncate_item(struct btrfs_trans_handle *trans,
			 struct btrfs_path *path, u32 new_size, int from_end);
int btrfs_split_item(struct btrfs_trans_handle *trans,
		     struct btrfs_root *root,
		     struct btrfs_path *path,
		     const struct btrfs_key *new_key,
		     unsigned long split_offset);
int btrfs_duplicate_item(struct btrfs_trans_handle *trans,
			 struct btrfs_root *root,
			 struct btrfs_path *path,
			 const struct btrfs_key *new_key);
int btrfs_find_item(struct btrfs_root *fs_root, struct btrfs_path *path,
		u64 inum, u64 ioff, u8 key_type, struct btrfs_key *found_key);
int btrfs_search_slot(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		      const struct btrfs_key *key, struct btrfs_path *p,
		      int ins_len, int cow);
int btrfs_search_old_slot(struct btrfs_root *root, const struct btrfs_key *key,
			  struct btrfs_path *p, u64 time_seq);
int btrfs_search_slot_for_read(struct btrfs_root *root,
			       const struct btrfs_key *key,
			       struct btrfs_path *p, int find_higher,
			       int return_any);
int btrfs_realloc_node(struct btrfs_trans_handle *trans,
		       struct btrfs_root *root, struct extent_buffer *parent,
		       int start_slot, u64 *last_ret,
		       struct btrfs_key *progress);
void btrfs_release_path(struct btrfs_path *p);
struct btrfs_path *btrfs_alloc_path(void);
void btrfs_free_path(struct btrfs_path *p);

int btrfs_del_items(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		   struct btrfs_path *path, int slot, int nr);
static inline int btrfs_del_item(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path)
{
	return btrfs_del_items(trans, root, path, path->slots[0], 1);
}


struct btrfs_item_batch {
	
	const struct btrfs_key *keys;
	
	const u32 *data_sizes;
	
	u32 total_data_size;
	
	int nr;
};

void btrfs_setup_item_for_insert(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 const struct btrfs_key *key,
				 u32 data_size);
int btrfs_insert_item(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		      const struct btrfs_key *key, void *data, u32 data_size);
int btrfs_insert_empty_items(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root,
			     struct btrfs_path *path,
			     const struct btrfs_item_batch *batch);

static inline int btrfs_insert_empty_item(struct btrfs_trans_handle *trans,
					  struct btrfs_root *root,
					  struct btrfs_path *path,
					  const struct btrfs_key *key,
					  u32 data_size)
{
	struct btrfs_item_batch batch;

	batch.keys = key;
	batch.data_sizes = &data_size;
	batch.total_data_size = data_size;
	batch.nr = 1;

	return btrfs_insert_empty_items(trans, root, path, &batch);
}

int btrfs_next_old_leaf(struct btrfs_root *root, struct btrfs_path *path,
			u64 time_seq);

int btrfs_search_backwards(struct btrfs_root *root, struct btrfs_key *key,
			   struct btrfs_path *path);

int btrfs_get_next_valid_item(struct btrfs_root *root, struct btrfs_key *key,
			      struct btrfs_path *path);


#define btrfs_for_each_slot(root, key, found_key, path, iter_ret)		\
	for (iter_ret = btrfs_search_slot(NULL, (root), (key), (path), 0, 0);	\
		(iter_ret) >= 0 &&						\
		(iter_ret = btrfs_get_next_valid_item((root), (found_key), (path))) == 0; \
		(path)->slots[0]++						\
	)

int btrfs_next_old_item(struct btrfs_root *root, struct btrfs_path *path, u64 time_seq);


static inline int btrfs_next_leaf(struct btrfs_root *root, struct btrfs_path *path)
{
	return btrfs_next_old_leaf(root, path, 0);
}

static inline int btrfs_next_item(struct btrfs_root *root, struct btrfs_path *p)
{
	return btrfs_next_old_item(root, p, 0);
}
int btrfs_leaf_free_space(const struct extent_buffer *leaf);

static inline int is_fstree(u64 rootid)
{
	if (rootid == BTRFS_FS_TREE_OBJECTID ||
	    ((s64)rootid >= (s64)BTRFS_FIRST_FREE_OBJECTID &&
	      !btrfs_qgroup_level(rootid)))
		return 1;
	return 0;
}

static inline bool btrfs_is_data_reloc_root(const struct btrfs_root *root)
{
	return root->root_key.objectid == BTRFS_DATA_RELOC_TREE_OBJECTID;
}

u16 btrfs_csum_type_size(u16 type);
int btrfs_super_csum_size(const struct btrfs_super_block *s);
const char *btrfs_super_csum_name(u16 csum_type);
const char *btrfs_super_csum_driver(u16 csum_type);
size_t __attribute_const__ btrfs_get_num_csums(void);


#define PageOrdered(page)		PagePrivate2(page)
#define SetPageOrdered(page)		SetPagePrivate2(page)
#define ClearPageOrdered(page)		ClearPagePrivate2(page)
#define folio_test_ordered(folio)	folio_test_private_2(folio)
#define folio_set_ordered(folio)	folio_set_private_2(folio)
#define folio_clear_ordered(folio)	folio_clear_private_2(folio)

#endif
