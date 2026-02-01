 
 

#ifndef _LINUX_DM_TRANSACTION_MANAGER_H
#define _LINUX_DM_TRANSACTION_MANAGER_H

#include "dm-block-manager.h"

struct dm_transaction_manager;
struct dm_space_map;

 

 

void dm_tm_destroy(struct dm_transaction_manager *tm);

 
struct dm_transaction_manager *dm_tm_create_non_blocking_clone(struct dm_transaction_manager *real);

 
int dm_tm_pre_commit(struct dm_transaction_manager *tm);
int dm_tm_commit(struct dm_transaction_manager *tm, struct dm_block *superblock);

 

 
int dm_tm_new_block(struct dm_transaction_manager *tm,
		    struct dm_block_validator *v,
		    struct dm_block **result);

 
int dm_tm_shadow_block(struct dm_transaction_manager *tm, dm_block_t orig,
		       struct dm_block_validator *v,
		       struct dm_block **result, int *inc_children);

 
int dm_tm_read_lock(struct dm_transaction_manager *tm, dm_block_t b,
		    struct dm_block_validator *v,
		    struct dm_block **result);

void dm_tm_unlock(struct dm_transaction_manager *tm, struct dm_block *b);

 
void dm_tm_inc(struct dm_transaction_manager *tm, dm_block_t b);
void dm_tm_inc_range(struct dm_transaction_manager *tm, dm_block_t b, dm_block_t e);
void dm_tm_dec(struct dm_transaction_manager *tm, dm_block_t b);
void dm_tm_dec_range(struct dm_transaction_manager *tm, dm_block_t b, dm_block_t e);

 
typedef void (*dm_tm_run_fn)(struct dm_transaction_manager *, dm_block_t, dm_block_t);
void dm_tm_with_runs(struct dm_transaction_manager *tm,
		     const __le64 *value_le, unsigned int count, dm_tm_run_fn fn);

int dm_tm_ref(struct dm_transaction_manager *tm, dm_block_t b, uint32_t *result);

 
int dm_tm_block_is_shared(struct dm_transaction_manager *tm, dm_block_t b,
			  int *result);

struct dm_block_manager *dm_tm_get_bm(struct dm_transaction_manager *tm);

 
void dm_tm_issue_prefetches(struct dm_transaction_manager *tm);

 
int dm_tm_create_with_sm(struct dm_block_manager *bm, dm_block_t sb_location,
			 struct dm_transaction_manager **tm,
			 struct dm_space_map **sm);

int dm_tm_open_with_sm(struct dm_block_manager *bm, dm_block_t sb_location,
		       void *sm_root, size_t root_len,
		       struct dm_transaction_manager **tm,
		       struct dm_space_map **sm);

#endif	 
