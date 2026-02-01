 
 

#ifndef DM_THIN_METADATA_H
#define DM_THIN_METADATA_H

#include "persistent-data/dm-block-manager.h"
#include "persistent-data/dm-space-map.h"
#include "persistent-data/dm-space-map-metadata.h"

#define THIN_METADATA_BLOCK_SIZE DM_SM_METADATA_BLOCK_SIZE

 
#define THIN_METADATA_MAX_SECTORS DM_SM_METADATA_MAX_SECTORS

 
#define THIN_METADATA_MAX_SECTORS_WARNING (16 * (1024 * 1024 * 1024 >> SECTOR_SHIFT))

 

 
#define THIN_METADATA_NEEDS_CHECK_FLAG (1 << 0)

struct dm_pool_metadata;
struct dm_thin_device;

 
typedef uint64_t dm_thin_id;

 
struct dm_pool_metadata *dm_pool_metadata_open(struct block_device *bdev,
					       sector_t data_block_size,
					       bool format_device);

int dm_pool_metadata_close(struct dm_pool_metadata *pmd);

 
#define THIN_FEATURE_COMPAT_SUPP	  0UL
#define THIN_FEATURE_COMPAT_RO_SUPP	  0UL
#define THIN_FEATURE_INCOMPAT_SUPP	  0UL

 
int dm_pool_create_thin(struct dm_pool_metadata *pmd, dm_thin_id dev);

 
int dm_pool_create_snap(struct dm_pool_metadata *pmd, dm_thin_id dev,
			dm_thin_id origin);

 
int dm_pool_delete_thin_device(struct dm_pool_metadata *pmd,
			       dm_thin_id dev);

 
int dm_pool_commit_metadata(struct dm_pool_metadata *pmd);

 
int dm_pool_abort_metadata(struct dm_pool_metadata *pmd);

 
int dm_pool_set_metadata_transaction_id(struct dm_pool_metadata *pmd,
					uint64_t current_id,
					uint64_t new_id);

int dm_pool_get_metadata_transaction_id(struct dm_pool_metadata *pmd,
					uint64_t *result);

 
int dm_pool_reserve_metadata_snap(struct dm_pool_metadata *pmd);
int dm_pool_release_metadata_snap(struct dm_pool_metadata *pmd);

int dm_pool_get_metadata_snap(struct dm_pool_metadata *pmd,
			      dm_block_t *result);

 

 
int dm_pool_open_thin_device(struct dm_pool_metadata *pmd, dm_thin_id dev,
			     struct dm_thin_device **td);

int dm_pool_close_thin_device(struct dm_thin_device *td);

dm_thin_id dm_thin_dev_id(struct dm_thin_device *td);

struct dm_thin_lookup_result {
	dm_block_t block;
	bool shared:1;
};

 
int dm_thin_find_block(struct dm_thin_device *td, dm_block_t block,
		       int can_issue_io, struct dm_thin_lookup_result *result);

 
int dm_thin_find_mapped_range(struct dm_thin_device *td,
			      dm_block_t begin, dm_block_t end,
			      dm_block_t *thin_begin, dm_block_t *thin_end,
			      dm_block_t *pool_begin, bool *maybe_shared);

 
int dm_pool_alloc_data_block(struct dm_pool_metadata *pmd, dm_block_t *result);

 
int dm_thin_insert_block(struct dm_thin_device *td, dm_block_t block,
			 dm_block_t data_block);

int dm_thin_remove_range(struct dm_thin_device *td,
			 dm_block_t begin, dm_block_t end);

 
bool dm_thin_changed_this_transaction(struct dm_thin_device *td);

bool dm_pool_changed_this_transaction(struct dm_pool_metadata *pmd);

bool dm_thin_aborted_changes(struct dm_thin_device *td);

int dm_thin_get_highest_mapped_block(struct dm_thin_device *td,
				     dm_block_t *highest_mapped);

int dm_thin_get_mapped_count(struct dm_thin_device *td, dm_block_t *result);

int dm_pool_get_free_block_count(struct dm_pool_metadata *pmd,
				 dm_block_t *result);

int dm_pool_get_free_metadata_block_count(struct dm_pool_metadata *pmd,
					  dm_block_t *result);

int dm_pool_get_metadata_dev_size(struct dm_pool_metadata *pmd,
				  dm_block_t *result);

int dm_pool_get_data_dev_size(struct dm_pool_metadata *pmd, dm_block_t *result);

int dm_pool_block_is_shared(struct dm_pool_metadata *pmd, dm_block_t b, bool *result);

int dm_pool_inc_data_range(struct dm_pool_metadata *pmd, dm_block_t b, dm_block_t e);
int dm_pool_dec_data_range(struct dm_pool_metadata *pmd, dm_block_t b, dm_block_t e);

 
int dm_pool_resize_data_dev(struct dm_pool_metadata *pmd, dm_block_t new_size);
int dm_pool_resize_metadata_dev(struct dm_pool_metadata *pmd, dm_block_t new_size);

 
void dm_pool_metadata_read_only(struct dm_pool_metadata *pmd);
void dm_pool_metadata_read_write(struct dm_pool_metadata *pmd);

int dm_pool_register_metadata_threshold(struct dm_pool_metadata *pmd,
					dm_block_t threshold,
					dm_sm_threshold_fn fn,
					void *context);

 
int dm_pool_metadata_set_needs_check(struct dm_pool_metadata *pmd);
bool dm_pool_metadata_needs_check(struct dm_pool_metadata *pmd);

 
void dm_pool_issue_prefetches(struct dm_pool_metadata *pmd);

 
typedef int (*dm_pool_pre_commit_fn)(void *context);

void dm_pool_register_pre_commit_callback(struct dm_pool_metadata *pmd,
					  dm_pool_pre_commit_fn fn,
					  void *context);

 

#endif
