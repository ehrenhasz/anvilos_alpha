#ifndef BLK_THROTTLE_H
#define BLK_THROTTLE_H

#include "blk-cgroup-rwstat.h"

 
struct throtl_qnode {
	struct list_head	node;		 
	struct bio_list		bios;		 
	struct throtl_grp	*tg;		 
};

struct throtl_service_queue {
	struct throtl_service_queue *parent_sq;	 

	 
	struct list_head	queued[2];	 
	unsigned int		nr_queued[2];	 

	 
	struct rb_root_cached	pending_tree;	 
	unsigned int		nr_pending;	 
	unsigned long		first_pending_disptime;	 
	struct timer_list	pending_timer;	 
};

enum tg_state_flags {
	THROTL_TG_PENDING	= 1 << 0,	 
	THROTL_TG_WAS_EMPTY	= 1 << 1,	 
	THROTL_TG_CANCELING	= 1 << 2,	 
};

enum {
	LIMIT_LOW,
	LIMIT_MAX,
	LIMIT_CNT,
};

struct throtl_grp {
	 
	struct blkg_policy_data pd;

	 
	struct rb_node rb_node;

	 
	struct throtl_data *td;

	 
	struct throtl_service_queue service_queue;

	 
	struct throtl_qnode qnode_on_self[2];
	struct throtl_qnode qnode_on_parent[2];

	 
	unsigned long disptime;

	unsigned int flags;

	 
	bool has_rules_bps[2];
	bool has_rules_iops[2];

	 
	uint64_t bps[2][LIMIT_CNT];
	 
	uint64_t bps_conf[2][LIMIT_CNT];

	 
	unsigned int iops[2][LIMIT_CNT];
	 
	unsigned int iops_conf[2][LIMIT_CNT];

	 
	uint64_t bytes_disp[2];
	 
	unsigned int io_disp[2];

	unsigned long last_low_overflow_time[2];

	uint64_t last_bytes_disp[2];
	unsigned int last_io_disp[2];

	 
	long long carryover_bytes[2];
	int carryover_ios[2];

	unsigned long last_check_time;

	unsigned long latency_target;  
	unsigned long latency_target_conf;  
	 
	unsigned long slice_start[2];
	unsigned long slice_end[2];

	unsigned long last_finish_time;  
	unsigned long checked_last_finish_time;  
	unsigned long avg_idletime;  
	unsigned long idletime_threshold;  
	unsigned long idletime_threshold_conf;  

	unsigned int bio_cnt;  
	unsigned int bad_bio_cnt;  
	unsigned long bio_cnt_reset_time;

	struct blkg_rwstat stat_bytes;
	struct blkg_rwstat stat_ios;
};

extern struct blkcg_policy blkcg_policy_throtl;

static inline struct throtl_grp *pd_to_tg(struct blkg_policy_data *pd)
{
	return pd ? container_of(pd, struct throtl_grp, pd) : NULL;
}

static inline struct throtl_grp *blkg_to_tg(struct blkcg_gq *blkg)
{
	return pd_to_tg(blkg_to_pd(blkg, &blkcg_policy_throtl));
}

 
#ifndef CONFIG_BLK_DEV_THROTTLING
static inline int blk_throtl_init(struct gendisk *disk) { return 0; }
static inline void blk_throtl_exit(struct gendisk *disk) { }
static inline void blk_throtl_register(struct gendisk *disk) { }
static inline bool blk_throtl_bio(struct bio *bio) { return false; }
static inline void blk_throtl_cancel_bios(struct gendisk *disk) { }
#else  
int blk_throtl_init(struct gendisk *disk);
void blk_throtl_exit(struct gendisk *disk);
void blk_throtl_register(struct gendisk *disk);
bool __blk_throtl_bio(struct bio *bio);
void blk_throtl_cancel_bios(struct gendisk *disk);

static inline bool blk_should_throtl(struct bio *bio)
{
	struct throtl_grp *tg = blkg_to_tg(bio->bi_blkg);
	int rw = bio_data_dir(bio);

	if (!cgroup_subsys_on_dfl(io_cgrp_subsys)) {
		if (!bio_flagged(bio, BIO_CGROUP_ACCT)) {
			bio_set_flag(bio, BIO_CGROUP_ACCT);
			blkg_rwstat_add(&tg->stat_bytes, bio->bi_opf,
					bio->bi_iter.bi_size);
		}
		blkg_rwstat_add(&tg->stat_ios, bio->bi_opf, 1);
	}

	 
	if (tg->has_rules_iops[rw])
		return true;

	if (tg->has_rules_bps[rw] && !bio_flagged(bio, BIO_BPS_THROTTLED))
		return true;

	return false;
}

static inline bool blk_throtl_bio(struct bio *bio)
{

	if (!blk_should_throtl(bio))
		return false;

	return __blk_throtl_bio(bio);
}
#endif  

#endif
