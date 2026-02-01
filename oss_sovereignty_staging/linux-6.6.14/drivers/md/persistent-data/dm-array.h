 
 
#ifndef _LINUX_DM_ARRAY_H
#define _LINUX_DM_ARRAY_H

#include "dm-btree.h"

 

 

 
struct dm_array_info {
	struct dm_transaction_manager *tm;
	struct dm_btree_value_type value_type;
	struct dm_btree_info btree_info;
};

 
void dm_array_info_init(struct dm_array_info *info,
			struct dm_transaction_manager *tm,
			struct dm_btree_value_type *vt);

 
int dm_array_empty(struct dm_array_info *info, dm_block_t *root);

 
int dm_array_resize(struct dm_array_info *info, dm_block_t root,
		    uint32_t old_size, uint32_t new_size,
		    const void *value, dm_block_t *new_root)
	__dm_written_to_disk(value);

 
typedef int (*value_fn)(uint32_t index, void *value_le, void *context);
int dm_array_new(struct dm_array_info *info, dm_block_t *root,
		 uint32_t size, value_fn fn, void *context);

 
int dm_array_del(struct dm_array_info *info, dm_block_t root);

 
int dm_array_get_value(struct dm_array_info *info, dm_block_t root,
		       uint32_t index, void *value);

 
int dm_array_set_value(struct dm_array_info *info, dm_block_t root,
		       uint32_t index, const void *value, dm_block_t *new_root)
	__dm_written_to_disk(value);

 
int dm_array_walk(struct dm_array_info *info, dm_block_t root,
		  int (*fn)(void *context, uint64_t key, void *leaf),
		  void *context);

 

 
struct dm_array_cursor {
	struct dm_array_info *info;
	struct dm_btree_cursor cursor;

	struct dm_block *block;
	struct array_block *ab;
	unsigned int index;
};

int dm_array_cursor_begin(struct dm_array_info *info,
			  dm_block_t root, struct dm_array_cursor *c);
void dm_array_cursor_end(struct dm_array_cursor *c);

uint32_t dm_array_cursor_index(struct dm_array_cursor *c);
int dm_array_cursor_next(struct dm_array_cursor *c);
int dm_array_cursor_skip(struct dm_array_cursor *c, uint32_t count);

 
void dm_array_cursor_get_value(struct dm_array_cursor *c, void **value_le);

 

#endif	 
