

#ifndef BTRFS_INODE_ITEM_H
#define BTRFS_INODE_ITEM_H

#include <linux/types.h>

struct btrfs_trans_handle;
struct btrfs_root;
struct btrfs_path;
struct btrfs_key;
struct btrfs_inode_extref;
struct btrfs_inode;
struct extent_buffer;


#define BTRFS_NEED_TRUNCATE_BLOCK		1

struct btrfs_truncate_control {
	
	struct btrfs_inode *inode;

	
	u64 new_size;

	
	u64 extents_found;

	
	u64 last_size;

	
	u64 sub_bytes;

	
	u64 ino;

	
	u32 min_type;

	
	bool skip_ref_updates;

	
	bool clear_extent_range;
};


static inline u64 btrfs_inode_combine_flags(u32 flags, u32 ro_flags)
{
	return (flags | ((u64)ro_flags << 32));
}

static inline void btrfs_inode_split_flags(u64 inode_item_flags,
					   u32 *flags, u32 *ro_flags)
{
	*flags = (u32)inode_item_flags;
	*ro_flags = (u32)(inode_item_flags >> 32);
}

int btrfs_truncate_inode_items(struct btrfs_trans_handle *trans,
			       struct btrfs_root *root,
			       struct btrfs_truncate_control *control);
int btrfs_insert_inode_ref(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root, const struct fscrypt_str *name,
			   u64 inode_objectid, u64 ref_objectid, u64 index);
int btrfs_del_inode_ref(struct btrfs_trans_handle *trans,
			struct btrfs_root *root, const struct fscrypt_str *name,
			u64 inode_objectid, u64 ref_objectid, u64 *index);
int btrfs_insert_empty_inode(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root,
			     struct btrfs_path *path, u64 objectid);
int btrfs_lookup_inode(struct btrfs_trans_handle *trans,
		       struct btrfs_root *root, struct btrfs_path *path,
		       struct btrfs_key *location, int mod);

struct btrfs_inode_extref *btrfs_lookup_inode_extref(
			  struct btrfs_trans_handle *trans,
			  struct btrfs_root *root,
			  struct btrfs_path *path,
			  const struct fscrypt_str *name,
			  u64 inode_objectid, u64 ref_objectid, int ins_len,
			  int cow);

struct btrfs_inode_ref *btrfs_find_name_in_backref(struct extent_buffer *leaf,
						   int slot,
						   const struct fscrypt_str *name);
struct btrfs_inode_extref *btrfs_find_name_in_ext_backref(
		struct extent_buffer *leaf, int slot, u64 ref_objectid,
		const struct fscrypt_str *name);

#endif
