#ifndef BTRFS_BACKREF_H
#define BTRFS_BACKREF_H
#include <linux/btrfs.h>
#include "messages.h"
#include "ulist.h"
#include "disk-io.h"
#include "extent_io.h"
#define BTRFS_ITERATE_EXTENT_INODES_STOP 5
typedef int (iterate_extent_inodes_t)(u64 inum, u64 offset, u64 num_bytes,
				      u64 root, void *ctx);
struct btrfs_backref_walk_ctx {
	u64 bytenr;
	u64 extent_item_pos;
	bool ignore_extent_item_pos;
	bool skip_inode_ref_list;
	struct btrfs_trans_handle *trans;
	struct btrfs_fs_info *fs_info;
	u64 time_seq;
	struct ulist *refs;
	struct ulist *roots;
	bool (*cache_lookup)(u64 leaf_bytenr, void *user_ctx,
			     const u64 **root_ids_ret, int *root_count_ret);
	void (*cache_store)(u64 leaf_bytenr, const struct ulist *root_ids,
			    void *user_ctx);
	iterate_extent_inodes_t *indirect_ref_iterator;
	int (*check_extent_item)(u64 bytenr, const struct btrfs_extent_item *ei,
				 const struct extent_buffer *leaf, void *user_ctx);
	bool (*skip_data_ref)(u64 root, u64 ino, u64 offset, void *user_ctx);
	void *user_ctx;
};
struct inode_fs_paths {
	struct btrfs_path		*btrfs_path;
	struct btrfs_root		*fs_root;
	struct btrfs_data_container	*fspath;
};
struct btrfs_backref_shared_cache_entry {
	u64 bytenr;
	u64 gen;
	bool is_shared;
};
#define BTRFS_BACKREF_CTX_PREV_EXTENTS_SIZE 8
struct btrfs_backref_share_check_ctx {
	struct ulist refs;
	u64 curr_leaf_bytenr;
	u64 prev_leaf_bytenr;
	struct btrfs_backref_shared_cache_entry path_cache_entries[BTRFS_MAX_LEVEL];
	bool use_path_cache;
	struct {
		u64 bytenr;
		bool is_shared;
	} prev_extents_cache[BTRFS_BACKREF_CTX_PREV_EXTENTS_SIZE];
	int prev_extents_cache_slot;
};
struct btrfs_backref_share_check_ctx *btrfs_alloc_backref_share_check_ctx(void);
void btrfs_free_backref_share_ctx(struct btrfs_backref_share_check_ctx *ctx);
int extent_from_logical(struct btrfs_fs_info *fs_info, u64 logical,
			struct btrfs_path *path, struct btrfs_key *found_key,
			u64 *flags);
int tree_backref_for_extent(unsigned long *ptr, struct extent_buffer *eb,
			    struct btrfs_key *key, struct btrfs_extent_item *ei,
			    u32 item_size, u64 *out_root, u8 *out_level);
int iterate_extent_inodes(struct btrfs_backref_walk_ctx *ctx,
			  bool search_commit_root,
			  iterate_extent_inodes_t *iterate, void *user_ctx);
int iterate_inodes_from_logical(u64 logical, struct btrfs_fs_info *fs_info,
				struct btrfs_path *path, void *ctx,
				bool ignore_offset);
int paths_from_inode(u64 inum, struct inode_fs_paths *ipath);
int btrfs_find_all_leafs(struct btrfs_backref_walk_ctx *ctx);
int btrfs_find_all_roots(struct btrfs_backref_walk_ctx *ctx,
			 bool skip_commit_root_sem);
char *btrfs_ref_to_path(struct btrfs_root *fs_root, struct btrfs_path *path,
			u32 name_len, unsigned long name_off,
			struct extent_buffer *eb_in, u64 parent,
			char *dest, u32 size);
struct btrfs_data_container *init_data_container(u32 total_bytes);
struct inode_fs_paths *init_ipath(s32 total_bytes, struct btrfs_root *fs_root,
					struct btrfs_path *path);
void free_ipath(struct inode_fs_paths *ipath);
int btrfs_find_one_extref(struct btrfs_root *root, u64 inode_objectid,
			  u64 start_off, struct btrfs_path *path,
			  struct btrfs_inode_extref **ret_extref,
			  u64 *found_off);
int btrfs_is_data_extent_shared(struct btrfs_inode *inode, u64 bytenr,
				u64 extent_gen,
				struct btrfs_backref_share_check_ctx *ctx);
int __init btrfs_prelim_ref_init(void);
void __cold btrfs_prelim_ref_exit(void);
struct prelim_ref {
	struct rb_node rbnode;
	u64 root_id;
	struct btrfs_key key_for_search;
	int level;
	int count;
	struct extent_inode_elem *inode_list;
	u64 parent;
	u64 wanted_disk_byte;
};
struct btrfs_backref_iter {
	u64 bytenr;
	struct btrfs_path *path;
	struct btrfs_fs_info *fs_info;
	struct btrfs_key cur_key;
	u32 item_ptr;
	u32 cur_ptr;
	u32 end_ptr;
};
struct btrfs_backref_iter *btrfs_backref_iter_alloc(struct btrfs_fs_info *fs_info);
static inline void btrfs_backref_iter_free(struct btrfs_backref_iter *iter)
{
	if (!iter)
		return;
	btrfs_free_path(iter->path);
	kfree(iter);
}
static inline struct extent_buffer *btrfs_backref_get_eb(
		struct btrfs_backref_iter *iter)
{
	if (!iter)
		return NULL;
	return iter->path->nodes[0];
}
static inline bool btrfs_backref_has_tree_block_info(
		struct btrfs_backref_iter *iter)
{
	if (iter->cur_key.type == BTRFS_EXTENT_ITEM_KEY &&
	    iter->cur_ptr - iter->item_ptr == sizeof(struct btrfs_extent_item))
		return true;
	return false;
}
int btrfs_backref_iter_start(struct btrfs_backref_iter *iter, u64 bytenr);
int btrfs_backref_iter_next(struct btrfs_backref_iter *iter);
static inline bool btrfs_backref_iter_is_inline_ref(
		struct btrfs_backref_iter *iter)
{
	if (iter->cur_key.type == BTRFS_EXTENT_ITEM_KEY ||
	    iter->cur_key.type == BTRFS_METADATA_ITEM_KEY)
		return true;
	return false;
}
static inline void btrfs_backref_iter_release(struct btrfs_backref_iter *iter)
{
	iter->bytenr = 0;
	iter->item_ptr = 0;
	iter->cur_ptr = 0;
	iter->end_ptr = 0;
	btrfs_release_path(iter->path);
	memset(&iter->cur_key, 0, sizeof(iter->cur_key));
}
struct btrfs_backref_node {
	struct {
		struct rb_node rb_node;
		u64 bytenr;
	};  
	u64 new_bytenr;
	u64 owner;
	struct list_head list;
	struct list_head upper;
	struct list_head lower;
	struct btrfs_root *root;
	struct extent_buffer *eb;
	unsigned int level:8;
	unsigned int cowonly:1;
	unsigned int lowest:1;
	unsigned int locked:1;
	unsigned int processed:1;
	unsigned int checked:1;
	unsigned int pending:1;
	unsigned int detached:1;
	unsigned int is_reloc_root:1;
};
#define LOWER	0
#define UPPER	1
struct btrfs_backref_edge {
	struct list_head list[2];
	struct btrfs_backref_node *node[2];
};
struct btrfs_backref_cache {
	struct rb_root rb_root;
	struct btrfs_backref_node *path[BTRFS_MAX_LEVEL];
	struct list_head pending[BTRFS_MAX_LEVEL];
	struct list_head leaves;
	struct list_head changed;
	struct list_head detached;
	u64 last_trans;
	int nr_nodes;
	int nr_edges;
	struct list_head pending_edge;
	struct list_head useless_node;
	struct btrfs_fs_info *fs_info;
	unsigned int is_reloc;
};
void btrfs_backref_init_cache(struct btrfs_fs_info *fs_info,
			      struct btrfs_backref_cache *cache, int is_reloc);
struct btrfs_backref_node *btrfs_backref_alloc_node(
		struct btrfs_backref_cache *cache, u64 bytenr, int level);
struct btrfs_backref_edge *btrfs_backref_alloc_edge(
		struct btrfs_backref_cache *cache);
#define		LINK_LOWER	(1 << 0)
#define		LINK_UPPER	(1 << 1)
static inline void btrfs_backref_link_edge(struct btrfs_backref_edge *edge,
					   struct btrfs_backref_node *lower,
					   struct btrfs_backref_node *upper,
					   int link_which)
{
	ASSERT(upper && lower && upper->level == lower->level + 1);
	edge->node[LOWER] = lower;
	edge->node[UPPER] = upper;
	if (link_which & LINK_LOWER)
		list_add_tail(&edge->list[LOWER], &lower->upper);
	if (link_which & LINK_UPPER)
		list_add_tail(&edge->list[UPPER], &upper->lower);
}
static inline void btrfs_backref_free_node(struct btrfs_backref_cache *cache,
					   struct btrfs_backref_node *node)
{
	if (node) {
		ASSERT(list_empty(&node->list));
		ASSERT(list_empty(&node->lower));
		ASSERT(node->eb == NULL);
		cache->nr_nodes--;
		btrfs_put_root(node->root);
		kfree(node);
	}
}
static inline void btrfs_backref_free_edge(struct btrfs_backref_cache *cache,
					   struct btrfs_backref_edge *edge)
{
	if (edge) {
		cache->nr_edges--;
		kfree(edge);
	}
}
static inline void btrfs_backref_unlock_node_buffer(
		struct btrfs_backref_node *node)
{
	if (node->locked) {
		btrfs_tree_unlock(node->eb);
		node->locked = 0;
	}
}
static inline void btrfs_backref_drop_node_buffer(
		struct btrfs_backref_node *node)
{
	if (node->eb) {
		btrfs_backref_unlock_node_buffer(node);
		free_extent_buffer(node->eb);
		node->eb = NULL;
	}
}
static inline void btrfs_backref_drop_node(struct btrfs_backref_cache *tree,
					   struct btrfs_backref_node *node)
{
	ASSERT(list_empty(&node->upper));
	btrfs_backref_drop_node_buffer(node);
	list_del_init(&node->list);
	list_del_init(&node->lower);
	if (!RB_EMPTY_NODE(&node->rb_node))
		rb_erase(&node->rb_node, &tree->rb_root);
	btrfs_backref_free_node(tree, node);
}
void btrfs_backref_cleanup_node(struct btrfs_backref_cache *cache,
				struct btrfs_backref_node *node);
void btrfs_backref_release_cache(struct btrfs_backref_cache *cache);
static inline void btrfs_backref_panic(struct btrfs_fs_info *fs_info,
				       u64 bytenr, int errno)
{
	btrfs_panic(fs_info, errno,
		    "Inconsistency in backref cache found at offset %llu",
		    bytenr);
}
int btrfs_backref_add_tree_node(struct btrfs_trans_handle *trans,
				struct btrfs_backref_cache *cache,
				struct btrfs_path *path,
				struct btrfs_backref_iter *iter,
				struct btrfs_key *node_key,
				struct btrfs_backref_node *cur);
int btrfs_backref_finish_upper_links(struct btrfs_backref_cache *cache,
				     struct btrfs_backref_node *start);
void btrfs_backref_error_cleanup(struct btrfs_backref_cache *cache,
				 struct btrfs_backref_node *node);
#endif
