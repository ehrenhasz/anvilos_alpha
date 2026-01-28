#ifndef _LINUX_DM_BITSET_H
#define _LINUX_DM_BITSET_H
#include "dm-array.h"
struct dm_disk_bitset {
	struct dm_array_info array_info;
	uint32_t current_index;
	uint64_t current_bits;
	bool current_index_set:1;
	bool dirty:1;
};
void dm_disk_bitset_init(struct dm_transaction_manager *tm,
			 struct dm_disk_bitset *info);
int dm_bitset_empty(struct dm_disk_bitset *info, dm_block_t *new_root);
typedef int (*bit_value_fn)(uint32_t index, bool *value, void *context);
int dm_bitset_new(struct dm_disk_bitset *info, dm_block_t *root,
		  uint32_t size, bit_value_fn fn, void *context);
int dm_bitset_resize(struct dm_disk_bitset *info, dm_block_t old_root,
		     uint32_t old_nr_entries, uint32_t new_nr_entries,
		     bool default_value, dm_block_t *new_root);
int dm_bitset_del(struct dm_disk_bitset *info, dm_block_t root);
int dm_bitset_set_bit(struct dm_disk_bitset *info, dm_block_t root,
		      uint32_t index, dm_block_t *new_root);
int dm_bitset_clear_bit(struct dm_disk_bitset *info, dm_block_t root,
			uint32_t index, dm_block_t *new_root);
int dm_bitset_test_bit(struct dm_disk_bitset *info, dm_block_t root,
		       uint32_t index, dm_block_t *new_root, bool *result);
int dm_bitset_flush(struct dm_disk_bitset *info, dm_block_t root,
		    dm_block_t *new_root);
struct dm_bitset_cursor {
	struct dm_disk_bitset *info;
	struct dm_array_cursor cursor;
	uint32_t entries_remaining;
	uint32_t array_index;
	uint32_t bit_index;
	uint64_t current_bits;
};
int dm_bitset_cursor_begin(struct dm_disk_bitset *info,
			   dm_block_t root, uint32_t nr_entries,
			   struct dm_bitset_cursor *c);
void dm_bitset_cursor_end(struct dm_bitset_cursor *c);
int dm_bitset_cursor_next(struct dm_bitset_cursor *c);
int dm_bitset_cursor_skip(struct dm_bitset_cursor *c, uint32_t count);
bool dm_bitset_cursor_get_value(struct dm_bitset_cursor *c);
#endif  
