


#ifndef DM_RQ_INTERNAL_H
#define DM_RQ_INTERNAL_H

#include <linux/bio.h>
#include <linux/kthread.h>

#include "dm-stats.h"

struct mapped_device;


struct dm_rq_clone_bio_info {
	struct bio *orig;
	struct dm_rq_target_io *tio;
	struct bio clone;
};

int dm_mq_init_request_queue(struct mapped_device *md, struct dm_table *t);
void dm_mq_cleanup_mapped_device(struct mapped_device *md);

void dm_start_queue(struct request_queue *q);
void dm_stop_queue(struct request_queue *q);

void dm_mq_kick_requeue_list(struct mapped_device *md);

unsigned int dm_get_reserved_rq_based_ios(void);

ssize_t dm_attr_rq_based_seq_io_merge_deadline_show(struct mapped_device *md, char *buf);
ssize_t dm_attr_rq_based_seq_io_merge_deadline_store(struct mapped_device *md,
						     const char *buf, size_t count);

#endif
