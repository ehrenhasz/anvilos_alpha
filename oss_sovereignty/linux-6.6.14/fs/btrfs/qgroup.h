#ifndef BTRFS_QGROUP_H
#define BTRFS_QGROUP_H
#include <linux/spinlock.h>
#include <linux/rbtree.h>
#include <linux/kobject.h>
#include "ulist.h"
#include "delayed-ref.h"
#include "misc.h"
#define BTRFS_QGROUP_RUNTIME_FLAG_CANCEL_RESCAN		(1UL << 3)
#define BTRFS_QGROUP_RUNTIME_FLAG_NO_ACCOUNTING		(1UL << 4)
struct btrfs_qgroup_extent_record {
	struct rb_node node;
	u64 bytenr;
	u64 num_bytes;
	u32 data_rsv;		 
	u64 data_rsv_refroot;	 
	struct ulist *old_roots;
};
struct btrfs_qgroup_swapped_block {
	struct rb_node node;
	int level;
	bool trace_leaf;
	u64 subvol_bytenr;
	u64 subvol_generation;
	u64 reloc_bytenr;
	u64 reloc_generation;
	u64 last_snapshot;
	struct btrfs_key first_key;
};
enum btrfs_qgroup_rsv_type {
	BTRFS_QGROUP_RSV_DATA,
	BTRFS_QGROUP_RSV_META_PERTRANS,
	BTRFS_QGROUP_RSV_META_PREALLOC,
	BTRFS_QGROUP_RSV_LAST,
};
struct btrfs_qgroup_rsv {
	u64 values[BTRFS_QGROUP_RSV_LAST];
};
struct btrfs_qgroup {
	u64 qgroupid;
	u64 rfer;	 
	u64 rfer_cmpr;	 
	u64 excl;	 
	u64 excl_cmpr;	 
	u64 lim_flags;	 
	u64 max_rfer;
	u64 max_excl;
	u64 rsv_rfer;
	u64 rsv_excl;
	struct btrfs_qgroup_rsv rsv;
	struct list_head groups;   
	struct list_head members;  
	struct list_head dirty;    
	struct list_head iterator;
	struct rb_node node;	   
	u64 old_refcnt;
	u64 new_refcnt;
	struct kobject kobj;
};
static inline u64 btrfs_qgroup_subvolid(u64 qgroupid)
{
	return (qgroupid & ((1ULL << BTRFS_QGROUP_LEVEL_SHIFT) - 1));
}
enum {
	ENUM_BIT(QGROUP_RESERVE),
	ENUM_BIT(QGROUP_RELEASE),
	ENUM_BIT(QGROUP_FREE),
};
int btrfs_quota_enable(struct btrfs_fs_info *fs_info);
int btrfs_quota_disable(struct btrfs_fs_info *fs_info);
int btrfs_qgroup_rescan(struct btrfs_fs_info *fs_info);
void btrfs_qgroup_rescan_resume(struct btrfs_fs_info *fs_info);
int btrfs_qgroup_wait_for_completion(struct btrfs_fs_info *fs_info,
				     bool interruptible);
int btrfs_add_qgroup_relation(struct btrfs_trans_handle *trans, u64 src,
			      u64 dst);
int btrfs_del_qgroup_relation(struct btrfs_trans_handle *trans, u64 src,
			      u64 dst);
int btrfs_create_qgroup(struct btrfs_trans_handle *trans, u64 qgroupid);
int btrfs_remove_qgroup(struct btrfs_trans_handle *trans, u64 qgroupid);
int btrfs_limit_qgroup(struct btrfs_trans_handle *trans, u64 qgroupid,
		       struct btrfs_qgroup_limit *limit);
int btrfs_read_qgroup_config(struct btrfs_fs_info *fs_info);
void btrfs_free_qgroup_config(struct btrfs_fs_info *fs_info);
struct btrfs_delayed_extent_op;
int btrfs_qgroup_trace_extent_nolock(
		struct btrfs_fs_info *fs_info,
		struct btrfs_delayed_ref_root *delayed_refs,
		struct btrfs_qgroup_extent_record *record);
int btrfs_qgroup_trace_extent_post(struct btrfs_trans_handle *trans,
				   struct btrfs_qgroup_extent_record *qrecord);
int btrfs_qgroup_trace_extent(struct btrfs_trans_handle *trans, u64 bytenr,
			      u64 num_bytes);
int btrfs_qgroup_trace_leaf_items(struct btrfs_trans_handle *trans,
				  struct extent_buffer *eb);
int btrfs_qgroup_trace_subtree(struct btrfs_trans_handle *trans,
			       struct extent_buffer *root_eb,
			       u64 root_gen, int root_level);
int btrfs_qgroup_account_extent(struct btrfs_trans_handle *trans, u64 bytenr,
				u64 num_bytes, struct ulist *old_roots,
				struct ulist *new_roots);
int btrfs_qgroup_account_extents(struct btrfs_trans_handle *trans);
int btrfs_run_qgroups(struct btrfs_trans_handle *trans);
int btrfs_qgroup_inherit(struct btrfs_trans_handle *trans, u64 srcid,
			 u64 objectid, struct btrfs_qgroup_inherit *inherit);
void btrfs_qgroup_free_refroot(struct btrfs_fs_info *fs_info,
			       u64 ref_root, u64 num_bytes,
			       enum btrfs_qgroup_rsv_type type);
#ifdef CONFIG_BTRFS_FS_RUN_SANITY_TESTS
int btrfs_verify_qgroup_counts(struct btrfs_fs_info *fs_info, u64 qgroupid,
			       u64 rfer, u64 excl);
#endif
int btrfs_qgroup_reserve_data(struct btrfs_inode *inode,
			struct extent_changeset **reserved, u64 start, u64 len);
int btrfs_qgroup_release_data(struct btrfs_inode *inode, u64 start, u64 len, u64 *released);
int btrfs_qgroup_free_data(struct btrfs_inode *inode,
			   struct extent_changeset *reserved, u64 start,
			   u64 len, u64 *freed);
int btrfs_qgroup_reserve_meta(struct btrfs_root *root, int num_bytes,
			      enum btrfs_qgroup_rsv_type type, bool enforce);
int __btrfs_qgroup_reserve_meta(struct btrfs_root *root, int num_bytes,
				enum btrfs_qgroup_rsv_type type, bool enforce,
				bool noflush);
static inline int btrfs_qgroup_reserve_meta_pertrans(struct btrfs_root *root,
				int num_bytes, bool enforce)
{
	return __btrfs_qgroup_reserve_meta(root, num_bytes,
					   BTRFS_QGROUP_RSV_META_PERTRANS,
					   enforce, false);
}
static inline int btrfs_qgroup_reserve_meta_prealloc(struct btrfs_root *root,
						     int num_bytes, bool enforce,
						     bool noflush)
{
	return __btrfs_qgroup_reserve_meta(root, num_bytes,
					   BTRFS_QGROUP_RSV_META_PREALLOC,
					   enforce, noflush);
}
void __btrfs_qgroup_free_meta(struct btrfs_root *root, int num_bytes,
			     enum btrfs_qgroup_rsv_type type);
static inline void btrfs_qgroup_free_meta_pertrans(struct btrfs_root *root,
						   int num_bytes)
{
	__btrfs_qgroup_free_meta(root, num_bytes,
			BTRFS_QGROUP_RSV_META_PERTRANS);
}
static inline void btrfs_qgroup_free_meta_prealloc(struct btrfs_root *root,
						   int num_bytes)
{
	__btrfs_qgroup_free_meta(root, num_bytes,
			BTRFS_QGROUP_RSV_META_PREALLOC);
}
void btrfs_qgroup_free_meta_all_pertrans(struct btrfs_root *root);
void btrfs_qgroup_convert_reserved_meta(struct btrfs_root *root, int num_bytes);
void btrfs_qgroup_check_reserved_leak(struct btrfs_inode *inode);
void btrfs_qgroup_init_swapped_blocks(
	struct btrfs_qgroup_swapped_blocks *swapped_blocks);
void btrfs_qgroup_clean_swapped_blocks(struct btrfs_root *root);
int btrfs_qgroup_add_swapped_blocks(struct btrfs_trans_handle *trans,
		struct btrfs_root *subvol_root,
		struct btrfs_block_group *bg,
		struct extent_buffer *subvol_parent, int subvol_slot,
		struct extent_buffer *reloc_parent, int reloc_slot,
		u64 last_snapshot);
int btrfs_qgroup_trace_subtree_after_cow(struct btrfs_trans_handle *trans,
		struct btrfs_root *root, struct extent_buffer *eb);
void btrfs_qgroup_destroy_extent_records(struct btrfs_transaction *trans);
bool btrfs_check_quota_leak(struct btrfs_fs_info *fs_info);
#endif
