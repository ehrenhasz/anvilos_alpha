

#ifndef _LINUX_DM_BTREE_H
#define _LINUX_DM_BTREE_H

#include "dm-block-manager.h"

struct dm_transaction_manager;




#ifdef __CHECKER__
#  define __dm_written_to_disk(x) __releases(x)
#  define __dm_reads_from_disk(x) __acquires(x)
#  define __dm_bless_for_disk(x) __acquire(x)
#  define __dm_unbless_for_disk(x) __release(x)
#else
#  define __dm_written_to_disk(x)
#  define __dm_reads_from_disk(x)
#  define __dm_bless_for_disk(x)
#  define __dm_unbless_for_disk(x)
#endif






struct dm_btree_value_type {
	void *context;

	
	uint32_t size;

	

	
	void (*inc)(void *context, const void *value, unsigned int count);

	
	void (*dec)(void *context, const void *value, unsigned int count);

	
	int (*equal)(void *context, const void *value1, const void *value2);
};


struct dm_btree_info {
	struct dm_transaction_manager *tm;

	
	unsigned int levels;
	struct dm_btree_value_type value_type;
};


int dm_btree_empty(struct dm_btree_info *info, dm_block_t *root);


int dm_btree_del(struct dm_btree_info *info, dm_block_t root);




int dm_btree_lookup(struct dm_btree_info *info, dm_block_t root,
		    uint64_t *keys, void *value_le);


int dm_btree_lookup_next(struct dm_btree_info *info, dm_block_t root,
			 uint64_t *keys, uint64_t *rkey, void *value_le);


int dm_btree_insert(struct dm_btree_info *info, dm_block_t root,
		    uint64_t *keys, void *value, dm_block_t *new_root)
	__dm_written_to_disk(value);


int dm_btree_insert_notify(struct dm_btree_info *info, dm_block_t root,
			   uint64_t *keys, void *value, dm_block_t *new_root,
			   int *inserted)
			   __dm_written_to_disk(value);


int dm_btree_remove(struct dm_btree_info *info, dm_block_t root,
		    uint64_t *keys, dm_block_t *new_root);


int dm_btree_remove_leaves(struct dm_btree_info *info, dm_block_t root,
			   uint64_t *keys, uint64_t end_key,
			   dm_block_t *new_root, unsigned int *nr_removed);


int dm_btree_find_lowest_key(struct dm_btree_info *info, dm_block_t root,
			     uint64_t *result_keys);


int dm_btree_find_highest_key(struct dm_btree_info *info, dm_block_t root,
			      uint64_t *result_keys);


int dm_btree_walk(struct dm_btree_info *info, dm_block_t root,
		  int (*fn)(void *context, uint64_t *keys, void *leaf),
		  void *context);





#define DM_BTREE_CURSOR_MAX_DEPTH 16

struct cursor_node {
	struct dm_block *b;
	unsigned int index;
};

struct dm_btree_cursor {
	struct dm_btree_info *info;
	dm_block_t root;

	bool prefetch_leaves;
	unsigned int depth;
	struct cursor_node nodes[DM_BTREE_CURSOR_MAX_DEPTH];
};


int dm_btree_cursor_begin(struct dm_btree_info *info, dm_block_t root,
			  bool prefetch_leaves, struct dm_btree_cursor *c);
void dm_btree_cursor_end(struct dm_btree_cursor *c);
int dm_btree_cursor_next(struct dm_btree_cursor *c);
int dm_btree_cursor_skip(struct dm_btree_cursor *c, uint32_t count);
int dm_btree_cursor_get_value(struct dm_btree_cursor *c, uint64_t *key, void *value_le);

#endif	
