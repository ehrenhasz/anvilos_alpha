 
 

#ifndef BTRFS_DELAYED_REF_H
#define BTRFS_DELAYED_REF_H

#include <linux/refcount.h>

 
#define BTRFS_ADD_DELAYED_REF    1  
#define BTRFS_DROP_DELAYED_REF   2  
#define BTRFS_ADD_DELAYED_EXTENT 3  
#define BTRFS_UPDATE_DELAYED_HEAD 4  

struct btrfs_delayed_ref_node {
	struct rb_node ref_node;
	 
	struct list_head add_list;

	 
	u64 bytenr;

	 
	u64 num_bytes;

	 
	u64 seq;

	 
	refcount_t refs;

	 
	int ref_mod;

	unsigned int action:8;
	unsigned int type:8;
};

struct btrfs_delayed_extent_op {
	struct btrfs_disk_key key;
	u8 level;
	bool update_key;
	bool update_flags;
	u64 flags_to_set;
};

 
struct btrfs_delayed_ref_head {
	u64 bytenr;
	u64 num_bytes;
	 
	struct rb_node href_node;
	 
	struct mutex mutex;

	refcount_t refs;

	 
	spinlock_t lock;
	struct rb_root_cached ref_tree;
	 
	struct list_head ref_add_list;

	struct btrfs_delayed_extent_op *extent_op;

	 
	int total_ref_mod;

	 
	int ref_mod;

	 
	bool must_insert_reserved;
	bool is_data;
	bool is_system;
	bool processing;
};

struct btrfs_delayed_tree_ref {
	struct btrfs_delayed_ref_node node;
	u64 root;
	u64 parent;
	int level;
};

struct btrfs_delayed_data_ref {
	struct btrfs_delayed_ref_node node;
	u64 root;
	u64 parent;
	u64 objectid;
	u64 offset;
};

enum btrfs_delayed_ref_flags {
	 
	BTRFS_DELAYED_REFS_FLUSHING,
};

struct btrfs_delayed_ref_root {
	 
	struct rb_root_cached href_root;

	 
	struct rb_root dirty_extent_root;

	 
	spinlock_t lock;

	 
	atomic_t num_entries;

	 
	unsigned long num_heads;

	 
	unsigned long num_heads_ready;

	u64 pending_csums;

	unsigned long flags;

	u64 run_delayed_start;

	 
	u64 qgroup_to_skip;
};

enum btrfs_ref_type {
	BTRFS_REF_NOT_SET,
	BTRFS_REF_DATA,
	BTRFS_REF_METADATA,
	BTRFS_REF_LAST,
};

struct btrfs_data_ref {
	 

	 
	u64 owning_root;

	 
	u64 ino;

	 
	u64 offset;
};

struct btrfs_tree_ref {
	 
	int level;

	 
	u64 owning_root;

	 
};

struct btrfs_ref {
	enum btrfs_ref_type type;
	int action;

	 
	bool skip_qgroup;

#ifdef CONFIG_BTRFS_FS_REF_VERIFY
	 
	u64 real_root;
#endif
	u64 bytenr;
	u64 len;

	 
	u64 parent;
	union {
		struct btrfs_data_ref data_ref;
		struct btrfs_tree_ref tree_ref;
	};
};

extern struct kmem_cache *btrfs_delayed_ref_head_cachep;
extern struct kmem_cache *btrfs_delayed_tree_ref_cachep;
extern struct kmem_cache *btrfs_delayed_data_ref_cachep;
extern struct kmem_cache *btrfs_delayed_extent_op_cachep;

int __init btrfs_delayed_ref_init(void);
void __cold btrfs_delayed_ref_exit(void);

static inline u64 btrfs_calc_delayed_ref_bytes(const struct btrfs_fs_info *fs_info,
					       int num_delayed_refs)
{
	u64 num_bytes;

	num_bytes = btrfs_calc_insert_metadata_size(fs_info, num_delayed_refs);

	 
	if (btrfs_test_opt(fs_info, FREE_SPACE_TREE))
		num_bytes *= 2;

	return num_bytes;
}

static inline void btrfs_init_generic_ref(struct btrfs_ref *generic_ref,
				int action, u64 bytenr, u64 len, u64 parent)
{
	generic_ref->action = action;
	generic_ref->bytenr = bytenr;
	generic_ref->len = len;
	generic_ref->parent = parent;
}

static inline void btrfs_init_tree_ref(struct btrfs_ref *generic_ref,
				int level, u64 root, u64 mod_root, bool skip_qgroup)
{
#ifdef CONFIG_BTRFS_FS_REF_VERIFY
	 
	generic_ref->real_root = mod_root ?: root;
#endif
	generic_ref->tree_ref.level = level;
	generic_ref->tree_ref.owning_root = root;
	generic_ref->type = BTRFS_REF_METADATA;
	if (skip_qgroup || !(is_fstree(root) &&
			     (!mod_root || is_fstree(mod_root))))
		generic_ref->skip_qgroup = true;
	else
		generic_ref->skip_qgroup = false;

}

static inline void btrfs_init_data_ref(struct btrfs_ref *generic_ref,
				u64 ref_root, u64 ino, u64 offset, u64 mod_root,
				bool skip_qgroup)
{
#ifdef CONFIG_BTRFS_FS_REF_VERIFY
	 
	generic_ref->real_root = mod_root ?: ref_root;
#endif
	generic_ref->data_ref.owning_root = ref_root;
	generic_ref->data_ref.ino = ino;
	generic_ref->data_ref.offset = offset;
	generic_ref->type = BTRFS_REF_DATA;
	if (skip_qgroup || !(is_fstree(ref_root) &&
			     (!mod_root || is_fstree(mod_root))))
		generic_ref->skip_qgroup = true;
	else
		generic_ref->skip_qgroup = false;
}

static inline struct btrfs_delayed_extent_op *
btrfs_alloc_delayed_extent_op(void)
{
	return kmem_cache_alloc(btrfs_delayed_extent_op_cachep, GFP_NOFS);
}

static inline void
btrfs_free_delayed_extent_op(struct btrfs_delayed_extent_op *op)
{
	if (op)
		kmem_cache_free(btrfs_delayed_extent_op_cachep, op);
}

static inline void btrfs_put_delayed_ref(struct btrfs_delayed_ref_node *ref)
{
	WARN_ON(refcount_read(&ref->refs) == 0);
	if (refcount_dec_and_test(&ref->refs)) {
		WARN_ON(!RB_EMPTY_NODE(&ref->ref_node));
		switch (ref->type) {
		case BTRFS_TREE_BLOCK_REF_KEY:
		case BTRFS_SHARED_BLOCK_REF_KEY:
			kmem_cache_free(btrfs_delayed_tree_ref_cachep, ref);
			break;
		case BTRFS_EXTENT_DATA_REF_KEY:
		case BTRFS_SHARED_DATA_REF_KEY:
			kmem_cache_free(btrfs_delayed_data_ref_cachep, ref);
			break;
		default:
			BUG();
		}
	}
}

static inline u64 btrfs_ref_head_to_space_flags(
				struct btrfs_delayed_ref_head *head_ref)
{
	if (head_ref->is_data)
		return BTRFS_BLOCK_GROUP_DATA;
	else if (head_ref->is_system)
		return BTRFS_BLOCK_GROUP_SYSTEM;
	return BTRFS_BLOCK_GROUP_METADATA;
}

static inline void btrfs_put_delayed_ref_head(struct btrfs_delayed_ref_head *head)
{
	if (refcount_dec_and_test(&head->refs))
		kmem_cache_free(btrfs_delayed_ref_head_cachep, head);
}

int btrfs_add_delayed_tree_ref(struct btrfs_trans_handle *trans,
			       struct btrfs_ref *generic_ref,
			       struct btrfs_delayed_extent_op *extent_op);
int btrfs_add_delayed_data_ref(struct btrfs_trans_handle *trans,
			       struct btrfs_ref *generic_ref,
			       u64 reserved);
int btrfs_add_delayed_extent_op(struct btrfs_trans_handle *trans,
				u64 bytenr, u64 num_bytes,
				struct btrfs_delayed_extent_op *extent_op);
void btrfs_merge_delayed_refs(struct btrfs_fs_info *fs_info,
			      struct btrfs_delayed_ref_root *delayed_refs,
			      struct btrfs_delayed_ref_head *head);

struct btrfs_delayed_ref_head *
btrfs_find_delayed_ref_head(struct btrfs_delayed_ref_root *delayed_refs,
			    u64 bytenr);
int btrfs_delayed_ref_lock(struct btrfs_delayed_ref_root *delayed_refs,
			   struct btrfs_delayed_ref_head *head);
static inline void btrfs_delayed_ref_unlock(struct btrfs_delayed_ref_head *head)
{
	mutex_unlock(&head->mutex);
}
void btrfs_delete_ref_head(struct btrfs_delayed_ref_root *delayed_refs,
			   struct btrfs_delayed_ref_head *head);

struct btrfs_delayed_ref_head *btrfs_select_ref_head(
		struct btrfs_delayed_ref_root *delayed_refs);

int btrfs_check_delayed_seq(struct btrfs_fs_info *fs_info, u64 seq);

void btrfs_delayed_refs_rsv_release(struct btrfs_fs_info *fs_info, int nr);
void btrfs_update_delayed_refs_rsv(struct btrfs_trans_handle *trans);
int btrfs_delayed_refs_rsv_refill(struct btrfs_fs_info *fs_info,
				  enum btrfs_reserve_flush_enum flush);
void btrfs_migrate_to_delayed_refs_rsv(struct btrfs_fs_info *fs_info,
				       u64 num_bytes);
bool btrfs_check_space_for_delayed_refs(struct btrfs_fs_info *fs_info);

 
static inline struct btrfs_delayed_tree_ref *
btrfs_delayed_node_to_tree_ref(struct btrfs_delayed_ref_node *node)
{
	return container_of(node, struct btrfs_delayed_tree_ref, node);
}

static inline struct btrfs_delayed_data_ref *
btrfs_delayed_node_to_data_ref(struct btrfs_delayed_ref_node *node)
{
	return container_of(node, struct btrfs_delayed_data_ref, node);
}

#endif
