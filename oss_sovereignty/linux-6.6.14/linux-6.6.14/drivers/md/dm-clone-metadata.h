#ifndef DM_CLONE_METADATA_H
#define DM_CLONE_METADATA_H
#include "persistent-data/dm-block-manager.h"
#include "persistent-data/dm-space-map-metadata.h"
#define DM_CLONE_METADATA_BLOCK_SIZE DM_SM_METADATA_BLOCK_SIZE
#define DM_CLONE_METADATA_MAX_SECTORS DM_SM_METADATA_MAX_SECTORS
#define DM_CLONE_METADATA_MAX_SECTORS_WARNING (16 * (1024 * 1024 * 1024 >> SECTOR_SHIFT))
#define SPACE_MAP_ROOT_SIZE 128
struct dm_clone_metadata;
int dm_clone_set_region_hydrated(struct dm_clone_metadata *cmd, unsigned long region_nr);
int dm_clone_cond_set_range(struct dm_clone_metadata *cmd, unsigned long start,
			    unsigned long nr_regions);
struct dm_clone_metadata *dm_clone_metadata_open(struct block_device *bdev,
						 sector_t target_size,
						 sector_t region_size);
void dm_clone_metadata_close(struct dm_clone_metadata *cmd);
int dm_clone_metadata_pre_commit(struct dm_clone_metadata *cmd);
int dm_clone_metadata_commit(struct dm_clone_metadata *cmd);
int dm_clone_reload_in_core_bitset(struct dm_clone_metadata *cmd);
bool dm_clone_changed_this_transaction(struct dm_clone_metadata *cmd);
int dm_clone_metadata_abort(struct dm_clone_metadata *cmd);
void dm_clone_metadata_set_read_only(struct dm_clone_metadata *cmd);
void dm_clone_metadata_set_read_write(struct dm_clone_metadata *cmd);
bool dm_clone_is_hydration_done(struct dm_clone_metadata *cmd);
bool dm_clone_is_region_hydrated(struct dm_clone_metadata *cmd, unsigned long region_nr);
bool dm_clone_is_range_hydrated(struct dm_clone_metadata *cmd,
				unsigned long start, unsigned long nr_regions);
unsigned int dm_clone_nr_of_hydrated_regions(struct dm_clone_metadata *cmd);
unsigned long dm_clone_find_next_unhydrated_region(struct dm_clone_metadata *cmd,
						   unsigned long start);
int dm_clone_get_free_metadata_block_count(struct dm_clone_metadata *cmd, dm_block_t *result);
int dm_clone_get_metadata_dev_size(struct dm_clone_metadata *cmd, dm_block_t *result);
#endif  
