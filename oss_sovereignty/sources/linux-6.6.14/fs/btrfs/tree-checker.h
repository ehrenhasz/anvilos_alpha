


#ifndef BTRFS_TREE_CHECKER_H
#define BTRFS_TREE_CHECKER_H

#include <uapi/linux/btrfs_tree.h>

struct extent_buffer;
struct btrfs_chunk;


struct btrfs_tree_parent_check {
	
	u64 owner_root;

	
	u64 transid;

	
	struct btrfs_key first_key;
	bool has_first_key;

	
	u8 level;
};

enum btrfs_tree_block_status {
	BTRFS_TREE_BLOCK_CLEAN,
	BTRFS_TREE_BLOCK_INVALID_NRITEMS,
	BTRFS_TREE_BLOCK_INVALID_PARENT_KEY,
	BTRFS_TREE_BLOCK_BAD_KEY_ORDER,
	BTRFS_TREE_BLOCK_INVALID_LEVEL,
	BTRFS_TREE_BLOCK_INVALID_FREE_SPACE,
	BTRFS_TREE_BLOCK_INVALID_OFFSETS,
	BTRFS_TREE_BLOCK_INVALID_BLOCKPTR,
	BTRFS_TREE_BLOCK_INVALID_ITEM,
	BTRFS_TREE_BLOCK_INVALID_OWNER,
};


enum btrfs_tree_block_status __btrfs_check_leaf(struct extent_buffer *leaf);
enum btrfs_tree_block_status __btrfs_check_node(struct extent_buffer *node);

int btrfs_check_leaf(struct extent_buffer *leaf);
int btrfs_check_node(struct extent_buffer *node);

int btrfs_check_chunk_valid(struct extent_buffer *leaf,
			    struct btrfs_chunk *chunk, u64 logical);
int btrfs_check_eb_owner(const struct extent_buffer *eb, u64 root_owner);
int btrfs_verify_level_key(struct extent_buffer *eb, int level,
			   struct btrfs_key *first_key, u64 parent_transid);

#endif
