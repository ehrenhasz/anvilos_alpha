


#ifndef DM_CACHE_BACKGROUND_WORK_H
#define DM_CACHE_BACKGROUND_WORK_H

#include <linux/vmalloc.h>
#include "dm-cache-policy.h"





struct background_work;
struct background_tracker;


struct background_tracker *btracker_create(unsigned int max_work);


void btracker_destroy(struct background_tracker *b);

unsigned int btracker_nr_writebacks_queued(struct background_tracker *b);
unsigned int btracker_nr_demotions_queued(struct background_tracker *b);


int btracker_queue(struct background_tracker *b,
		   struct policy_work *work,
		   struct policy_work **pwork);


int btracker_issue(struct background_tracker *b, struct policy_work **work);


void btracker_complete(struct background_tracker *b, struct policy_work *op);


bool btracker_promotion_already_present(struct background_tracker *b,
					dm_oblock_t oblock);



#endif
