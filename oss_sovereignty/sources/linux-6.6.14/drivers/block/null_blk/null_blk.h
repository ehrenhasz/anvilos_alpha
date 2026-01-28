
#ifndef __BLK_NULL_BLK_H
#define __BLK_NULL_BLK_H

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/blkdev.h>
#include <linux/slab.h>
#include <linux/blk-mq.h>
#include <linux/hrtimer.h>
#include <linux/configfs.h>
#include <linux/badblocks.h>
#include <linux/fault-inject.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>

struct nullb_cmd {
	union {
		struct request *rq;
		struct bio *bio;
	};
	unsigned int tag;
	blk_status_t error;
	bool fake_timeout;
	struct nullb_queue *nq;
	struct hrtimer timer;
};

struct nullb_queue {
	unsigned long *tag_map;
	wait_queue_head_t wait;
	unsigned int queue_depth;
	struct nullb_device *dev;
	unsigned int requeue_selection;

	struct list_head poll_list;
	spinlock_t poll_lock;

	struct nullb_cmd *cmds;
};

struct nullb_zone {
	
	union {
		spinlock_t spinlock;
		struct mutex mutex;
	};
	enum blk_zone_type type;
	enum blk_zone_cond cond;
	sector_t start;
	sector_t wp;
	unsigned int len;
	unsigned int capacity;
};


enum {
	NULL_Q_BIO	= 0,
	NULL_Q_RQ	= 1,
	NULL_Q_MQ	= 2,
};

struct nullb_device {
	struct nullb *nullb;
	struct config_group group;
#ifdef CONFIG_BLK_DEV_NULL_BLK_FAULT_INJECTION
	struct fault_config timeout_config;
	struct fault_config requeue_config;
	struct fault_config init_hctx_fault_config;
#endif
	struct radix_tree_root data; 
	struct radix_tree_root cache; 
	unsigned long flags; 
	unsigned int curr_cache;
	struct badblocks badblocks;

	unsigned int nr_zones;
	unsigned int nr_zones_imp_open;
	unsigned int nr_zones_exp_open;
	unsigned int nr_zones_closed;
	unsigned int imp_close_zone_no;
	struct nullb_zone *zones;
	sector_t zone_size_sects;
	bool need_zone_res_mgmt;
	spinlock_t zone_res_lock;

	unsigned long size; 
	unsigned long completion_nsec; 
	unsigned long cache_size; 
	unsigned long zone_size; 
	unsigned long zone_capacity; 
	unsigned int zone_nr_conv; 
	unsigned int zone_max_open; 
	unsigned int zone_max_active; 
	unsigned int submit_queues; 
	unsigned int prev_submit_queues; 
	unsigned int poll_queues; 
	unsigned int prev_poll_queues; 
	unsigned int home_node; 
	unsigned int queue_mode; 
	unsigned int blocksize; 
	unsigned int max_sectors; 
	unsigned int irqmode; 
	unsigned int hw_queue_depth; 
	unsigned int index; 
	unsigned int mbps; 
	bool blocking; 
	bool use_per_node_hctx; 
	bool power; 
	bool memory_backed; 
	bool discard; 
	bool zoned; 
	bool virt_boundary; 
	bool no_sched; 
	bool shared_tag_bitmap; 
};

struct nullb {
	struct nullb_device *dev;
	struct list_head list;
	unsigned int index;
	struct request_queue *q;
	struct gendisk *disk;
	struct blk_mq_tag_set *tag_set;
	struct blk_mq_tag_set __tag_set;
	unsigned int queue_depth;
	atomic_long_t cur_bytes;
	struct hrtimer bw_timer;
	unsigned long cache_flush_pos;
	spinlock_t lock;

	struct nullb_queue *queues;
	unsigned int nr_queues;
	char disk_name[DISK_NAME_LEN];
};

blk_status_t null_handle_discard(struct nullb_device *dev, sector_t sector,
				 sector_t nr_sectors);
blk_status_t null_process_cmd(struct nullb_cmd *cmd, enum req_op op,
			      sector_t sector, unsigned int nr_sectors);

#ifdef CONFIG_BLK_DEV_ZONED
int null_init_zoned_dev(struct nullb_device *dev, struct request_queue *q);
int null_register_zoned_dev(struct nullb *nullb);
void null_free_zoned_dev(struct nullb_device *dev);
int null_report_zones(struct gendisk *disk, sector_t sector,
		      unsigned int nr_zones, report_zones_cb cb, void *data);
blk_status_t null_process_zoned_cmd(struct nullb_cmd *cmd, enum req_op op,
				    sector_t sector, sector_t nr_sectors);
size_t null_zone_valid_read_len(struct nullb *nullb,
				sector_t sector, unsigned int len);
ssize_t zone_cond_store(struct nullb_device *dev, const char *page,
			size_t count, enum blk_zone_cond cond);
#else
static inline int null_init_zoned_dev(struct nullb_device *dev,
				      struct request_queue *q)
{
	pr_err("CONFIG_BLK_DEV_ZONED not enabled\n");
	return -EINVAL;
}
static inline int null_register_zoned_dev(struct nullb *nullb)
{
	return -ENODEV;
}
static inline void null_free_zoned_dev(struct nullb_device *dev) {}
static inline blk_status_t null_process_zoned_cmd(struct nullb_cmd *cmd,
			enum req_op op, sector_t sector, sector_t nr_sectors)
{
	return BLK_STS_NOTSUPP;
}
static inline size_t null_zone_valid_read_len(struct nullb *nullb,
					      sector_t sector,
					      unsigned int len)
{
	return len;
}
static inline ssize_t zone_cond_store(struct nullb_device *dev,
				      const char *page, size_t count,
				      enum blk_zone_cond cond)
{
	return -EOPNOTSUPP;
}
#define null_report_zones	NULL
#endif 
#endif 
