


#ifndef DM_CACHE_POLICY_H
#define DM_CACHE_POLICY_H

#include "dm-cache-block-types.h"

#include <linux/device-mapper.h>




enum policy_operation {
	POLICY_PROMOTE,
	POLICY_DEMOTE,
	POLICY_WRITEBACK
};


struct policy_work {
	enum policy_operation op;
	dm_oblock_t oblock;
	dm_cblock_t cblock;
};


struct dm_cache_policy {
	
	void (*destroy)(struct dm_cache_policy *p);

	
	int (*lookup)(struct dm_cache_policy *p, dm_oblock_t oblock, dm_cblock_t *cblock,
		      int data_dir, bool fast_copy, bool *background_queued);

	
	int (*lookup_with_work)(struct dm_cache_policy *p,
				dm_oblock_t oblock, dm_cblock_t *cblock,
				int data_dir, bool fast_copy,
				struct policy_work **work);

	
	int (*get_background_work)(struct dm_cache_policy *p, bool idle,
				   struct policy_work **result);

	
	void (*complete_background_work)(struct dm_cache_policy *p,
					 struct policy_work *work,
					 bool success);

	void (*set_dirty)(struct dm_cache_policy *p, dm_cblock_t cblock);
	void (*clear_dirty)(struct dm_cache_policy *p, dm_cblock_t cblock);

	
	int (*load_mapping)(struct dm_cache_policy *p, dm_oblock_t oblock,
			    dm_cblock_t cblock, bool dirty,
			    uint32_t hint, bool hint_valid);

	
	int (*invalidate_mapping)(struct dm_cache_policy *p, dm_cblock_t cblock);

	
	uint32_t (*get_hint)(struct dm_cache_policy *p, dm_cblock_t cblock);

	
	dm_cblock_t (*residency)(struct dm_cache_policy *p);

	
	void (*tick)(struct dm_cache_policy *p, bool can_block);

	
	int (*emit_config_values)(struct dm_cache_policy *p, char *result,
				  unsigned int maxlen, ssize_t *sz_ptr);
	int (*set_config_value)(struct dm_cache_policy *p,
				const char *key, const char *value);

	void (*allow_migrations)(struct dm_cache_policy *p, bool allow);

	
	void *private;
};




#define CACHE_POLICY_NAME_SIZE 16
#define CACHE_POLICY_VERSION_SIZE 3

struct dm_cache_policy_type {
	
	struct list_head list;

	
	char name[CACHE_POLICY_NAME_SIZE];
	unsigned int version[CACHE_POLICY_VERSION_SIZE];

	
	struct dm_cache_policy_type *real;

	
	size_t hint_size;

	struct module *owner;
	struct dm_cache_policy *(*create)(dm_cblock_t cache_size,
					  sector_t origin_size,
					  sector_t block_size);
};

int dm_cache_policy_register(struct dm_cache_policy_type *type);
void dm_cache_policy_unregister(struct dm_cache_policy_type *type);



#endif	
