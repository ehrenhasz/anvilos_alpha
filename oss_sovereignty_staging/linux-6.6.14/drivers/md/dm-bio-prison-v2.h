 
 

#ifndef DM_BIO_PRISON_V2_H
#define DM_BIO_PRISON_V2_H

#include "persistent-data/dm-block-manager.h"  
#include "dm-thin-metadata.h"  

#include <linux/bio.h>
#include <linux/rbtree.h>
#include <linux/workqueue.h>

 

int dm_bio_prison_init_v2(void);
void dm_bio_prison_exit_v2(void);

 
struct dm_bio_prison_v2;

 
struct dm_cell_key_v2 {
	int virtual;
	dm_thin_id dev;
	dm_block_t block_begin, block_end;
};

 
struct dm_bio_prison_cell_v2 {
	
	bool exclusive_lock;
	unsigned int exclusive_level;
	unsigned int shared_count;
	struct work_struct *quiesce_continuation;

	struct rb_node node;
	struct dm_cell_key_v2 key;
	struct bio_list bios;
};

struct dm_bio_prison_v2 *dm_bio_prison_create_v2(struct workqueue_struct *wq);
void dm_bio_prison_destroy_v2(struct dm_bio_prison_v2 *prison);

 
struct dm_bio_prison_cell_v2 *dm_bio_prison_alloc_cell_v2(struct dm_bio_prison_v2 *prison,
						    gfp_t gfp);
void dm_bio_prison_free_cell_v2(struct dm_bio_prison_v2 *prison,
				struct dm_bio_prison_cell_v2 *cell);

 
bool dm_cell_get_v2(struct dm_bio_prison_v2 *prison,
		    struct dm_cell_key_v2 *key,
		    unsigned int lock_level,
		    struct bio *inmate,
		    struct dm_bio_prison_cell_v2 *cell_prealloc,
		    struct dm_bio_prison_cell_v2 **cell_result);

 
bool dm_cell_put_v2(struct dm_bio_prison_v2 *prison,
		    struct dm_bio_prison_cell_v2 *cell);

 
int dm_cell_lock_v2(struct dm_bio_prison_v2 *prison,
		    struct dm_cell_key_v2 *key,
		    unsigned int lock_level,
		    struct dm_bio_prison_cell_v2 *cell_prealloc,
		    struct dm_bio_prison_cell_v2 **cell_result);

void dm_cell_quiesce_v2(struct dm_bio_prison_v2 *prison,
			struct dm_bio_prison_cell_v2 *cell,
			struct work_struct *continuation);

 
int dm_cell_lock_promote_v2(struct dm_bio_prison_v2 *prison,
			    struct dm_bio_prison_cell_v2 *cell,
			    unsigned int new_lock_level);

 
bool dm_cell_unlock_v2(struct dm_bio_prison_v2 *prison,
		       struct dm_bio_prison_cell_v2 *cell,
		       struct bio_list *bios);

 

#endif
