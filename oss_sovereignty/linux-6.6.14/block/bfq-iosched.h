 
 
#ifndef _BFQ_H
#define _BFQ_H

#include <linux/blktrace_api.h>
#include <linux/hrtimer.h>

#include "blk-cgroup-rwstat.h"

#define BFQ_IOPRIO_CLASSES	3
#define BFQ_CL_IDLE_TIMEOUT	(HZ/5)

#define BFQ_MIN_WEIGHT			1
#define BFQ_MAX_WEIGHT			1000
#define BFQ_WEIGHT_CONVERSION_COEFF	10

#define BFQ_DEFAULT_QUEUE_IOPRIO	4

#define BFQ_DEFAULT_GRP_IOPRIO	0
#define BFQ_DEFAULT_GRP_CLASS	IOPRIO_CLASS_BE

#define MAX_BFQQ_NAME_LENGTH 16

 
#define BFQ_SOFTRT_WEIGHT_FACTOR	100

 
#define BFQ_MAX_ACTUATORS 8

struct bfq_entity;

 
struct bfq_service_tree {
	 
	struct rb_root active;
	 
	struct rb_root idle;

	 
	struct bfq_entity *first_idle;
	 
	struct bfq_entity *last_idle;

	 
	u64 vtime;
	 
	unsigned long wsum;
};

 
struct bfq_sched_data {
	 
	struct bfq_entity *in_service_entity;
	 
	struct bfq_entity *next_in_service;
	 
	struct bfq_service_tree service_tree[BFQ_IOPRIO_CLASSES];
	 
	unsigned long bfq_class_idle_last_service;

};

 
struct bfq_weight_counter {
	unsigned int weight;  
	unsigned int num_active;  
	 
	struct rb_node weights_node;
};

 
struct bfq_entity {
	 
	struct rb_node rb_node;

	 
	bool on_st_or_in_serv;

	 
	u64 start, finish;

	 
	struct rb_root *tree;

	 
	u64 min_start;

	 
	int service;

	 
	int budget;

	 
	int allocated;

	 
	int dev_weight;
	 
	int weight;
	 
	int new_weight;

	 
	int orig_weight;

	 
	struct bfq_entity *parent;

	 
	struct bfq_sched_data *my_sched_data;
	 
	struct bfq_sched_data *sched_data;

	 
	int prio_changed;

#ifdef CONFIG_BFQ_GROUP_IOSCHED
	 
	bool in_groups_with_pending_reqs;
#endif

	 
	struct bfq_queue *last_bfqq_created;
};

struct bfq_group;

 
struct bfq_ttime {
	 
	u64 last_end_request;

	 
	u64 ttime_total;
	 
	unsigned long ttime_samples;
	 
	u64 ttime_mean;
};

 
struct bfq_queue {
	 
	int ref;
	 
	int stable_ref;
	 
	struct bfq_data *bfqd;

	 
	unsigned short ioprio, ioprio_class;
	 
	unsigned short new_ioprio, new_ioprio_class;

	 
	u64 last_serv_time_ns;
	 
	unsigned int inject_limit;
	 
	unsigned long decrease_time_jif;

	 
	struct bfq_queue *new_bfqq;
	 
	struct rb_node pos_node;
	 
	struct rb_root *pos_root;

	 
	struct rb_root sort_list;
	 
	struct request *next_rq;
	 
	int queued[2];
	 
	int meta_pending;
	 
	struct list_head fifo;

	 
	struct bfq_entity entity;

	 
	struct bfq_weight_counter *weight_counter;

	 
	int max_budget;
	 
	unsigned long budget_timeout;

	 
	int dispatched;

	 
	unsigned long flags;

	 
	struct list_head bfqq_list;

	 
	struct bfq_ttime ttime;

	 
	u64 io_start_time;
	 
	u64 tot_idle_time;

	 
	u32 seek_history;

	 
	struct hlist_node burst_list_node;

	 
	sector_t last_request_pos;

	 
	unsigned int requests_within_timer;

	 
	pid_t pid;

	 
	struct bfq_io_cq *bic;

	 
	unsigned long wr_cur_max_time;
	 
	unsigned long soft_rt_next_start;
	 
	unsigned long last_wr_start_finish;
	 
	unsigned int wr_coeff;
	 
	unsigned long last_idle_bklogged;
	 
	unsigned long service_from_backlogged;
	 
	unsigned long service_from_wr;

	 
	unsigned long wr_start_at_switch_to_srt;

	unsigned long split_time;  

	unsigned long first_IO_time;  
	unsigned long creation_time;  

	 
	struct bfq_queue *waker_bfqq;
	 
	struct bfq_queue *tentative_waker_bfqq;
	 
	unsigned int num_waker_detections;
	 
	u64 waker_detection_started;

	 
	struct hlist_node woken_list_node;
	 
	struct hlist_head woken_list;

	 
	unsigned int actuator_idx;
};

 
struct bfq_iocq_bfqq_data {
	 
	bool saved_has_short_ttime;
	 
	bool saved_IO_bound;

	u64 saved_io_start_time;
	u64 saved_tot_idle_time;

	 
	bool saved_in_large_burst;
	 
	bool was_in_burst_list;

	 
	unsigned int saved_weight;

	 
	unsigned long saved_wr_coeff;
	unsigned long saved_last_wr_start_finish;
	unsigned long saved_service_from_wr;
	unsigned long saved_wr_start_at_switch_to_srt;
	unsigned int saved_wr_cur_max_time;
	struct bfq_ttime saved_ttime;

	 
	u64 saved_last_serv_time_ns;
	unsigned int saved_inject_limit;
	unsigned long saved_decrease_time_jif;

	 
	struct bfq_queue *stable_merge_bfqq;

	bool stably_merged;	 
};

 
struct bfq_io_cq {
	 
	struct io_cq icq;  
	 
	struct bfq_queue *bfqq[2][BFQ_MAX_ACTUATORS];
	 
	int ioprio;
#ifdef CONFIG_BFQ_GROUP_IOSCHED
	uint64_t blkcg_serial_nr;  
#endif

	 
	struct bfq_iocq_bfqq_data bfqq_data[BFQ_MAX_ACTUATORS];

	unsigned int requests;	 
};

 
struct bfq_data {
	 
	struct request_queue *queue;
	 
	struct list_head dispatch;

	 
	struct bfq_group *root_group;

	 
	struct rb_root_cached queue_weights_tree;

#ifdef CONFIG_BFQ_GROUP_IOSCHED
	 
	unsigned int num_groups_with_pending_reqs;
#endif

	 
	unsigned int busy_queues[3];
	 
	int wr_busy_queues;
	 
	int queued;
	 
	int tot_rq_in_driver;
	 
	int rq_in_driver[BFQ_MAX_ACTUATORS];

	 
	bool nonrot_with_queueing;

	 
	int max_rq_in_driver;
	 
	int hw_tag_samples;
	 
	int hw_tag;

	 
	int budgets_assigned;

	 
	struct hrtimer idle_slice_timer;

	 
	struct bfq_queue *in_service_queue;

	 
	sector_t last_position;

	 
	sector_t in_serv_last_pos;

	 
	u64 last_completion;

	 
	struct bfq_queue *last_completed_rq_bfqq;

	 
	struct bfq_queue *last_bfqq_created;

	 
	u64 last_empty_occupied_ns;

	 
	bool wait_dispatch;
	 
	struct request *waited_rq;
	 
	bool rqs_injected;

	 
	u64 first_dispatch;
	 
	u64 last_dispatch;

	 
	ktime_t last_budget_start;
	 
	ktime_t last_idling_start;
	unsigned long last_idling_start_jiffies;

	 
	int peak_rate_samples;
	 
	u32 sequential_samples;
	 
	u64 tot_sectors_dispatched;
	 
	u32 last_rq_max_size;
	 
	u64 delta_from_first;
	 
	u32 peak_rate;

	 
	int bfq_max_budget;

	 
	struct list_head active_list[BFQ_MAX_ACTUATORS];
	 
	struct list_head idle_list;

	 
	u64 bfq_fifo_expire[2];
	 
	unsigned int bfq_back_penalty;
	 
	unsigned int bfq_back_max;
	 
	u32 bfq_slice_idle;

	 
	int bfq_user_max_budget;
	 
	unsigned int bfq_timeout;

	 
	bool strict_guarantees;

	 
	unsigned long last_ins_in_burst;
	 
	unsigned long bfq_burst_interval;
	 
	int burst_size;

	 
	struct bfq_entity *burst_parent_entity;
	 
	unsigned long bfq_large_burst_thresh;
	 
	bool large_burst;
	 
	struct hlist_head burst_list;

	 
	bool low_latency;
	 
	unsigned int bfq_wr_coeff;

	 
	unsigned int bfq_wr_rt_max_time;
	 
	unsigned int bfq_wr_min_idle_time;
	 
	unsigned long bfq_wr_min_inter_arr_async;

	 
	unsigned int bfq_wr_max_softrt_rate;
	 
	u64 rate_dur_prod;

	 
	struct bfq_queue oom_bfqq;

	spinlock_t lock;

	 
	struct bfq_io_cq *bio_bic;
	 
	struct bfq_queue *bio_bfqq;

	 
	unsigned int word_depths[2][2];
	unsigned int full_depth_shift;

	 
	unsigned int num_actuators;
	 
	sector_t sector[BFQ_MAX_ACTUATORS];
	sector_t nr_sectors[BFQ_MAX_ACTUATORS];
	struct blk_independent_access_range ia_ranges[BFQ_MAX_ACTUATORS];

	 
	unsigned int actuator_load_threshold;
};

enum bfqq_state_flags {
	BFQQF_just_created = 0,	 
	BFQQF_busy,		 
	BFQQF_wait_request,	 
	BFQQF_non_blocking_wait_rq,  
	BFQQF_fifo_expire,	 
	BFQQF_has_short_ttime,	 
	BFQQF_sync,		 
	BFQQF_IO_bound,		 
	BFQQF_in_large_burst,	 
	BFQQF_softrt_update,	 
	BFQQF_coop,		 
	BFQQF_split_coop,	 
};

#define BFQ_BFQQ_FNS(name)						\
void bfq_mark_bfqq_##name(struct bfq_queue *bfqq);			\
void bfq_clear_bfqq_##name(struct bfq_queue *bfqq);			\
int bfq_bfqq_##name(const struct bfq_queue *bfqq);

BFQ_BFQQ_FNS(just_created);
BFQ_BFQQ_FNS(busy);
BFQ_BFQQ_FNS(wait_request);
BFQ_BFQQ_FNS(non_blocking_wait_rq);
BFQ_BFQQ_FNS(fifo_expire);
BFQ_BFQQ_FNS(has_short_ttime);
BFQ_BFQQ_FNS(sync);
BFQ_BFQQ_FNS(IO_bound);
BFQ_BFQQ_FNS(in_large_burst);
BFQ_BFQQ_FNS(coop);
BFQ_BFQQ_FNS(split_coop);
BFQ_BFQQ_FNS(softrt_update);
#undef BFQ_BFQQ_FNS

 
enum bfqq_expiration {
	BFQQE_TOO_IDLE = 0,		 
	BFQQE_BUDGET_TIMEOUT,	 
	BFQQE_BUDGET_EXHAUSTED,	 
	BFQQE_NO_MORE_REQUESTS,	 
	BFQQE_PREEMPTED		 
};

struct bfq_stat {
	struct percpu_counter		cpu_cnt;
	atomic64_t			aux_cnt;
};

struct bfqg_stats {
	 
	struct blkg_rwstat		bytes;
	struct blkg_rwstat		ios;
#ifdef CONFIG_BFQ_CGROUP_DEBUG
	 
	struct blkg_rwstat		merged;
	 
	struct blkg_rwstat		service_time;
	 
	struct blkg_rwstat		wait_time;
	 
	struct blkg_rwstat		queued;
	 
	struct bfq_stat		time;
	 
	struct bfq_stat		avg_queue_size_sum;
	 
	struct bfq_stat		avg_queue_size_samples;
	 
	struct bfq_stat		dequeue;
	 
	struct bfq_stat		group_wait_time;
	 
	struct bfq_stat		idle_time;
	 
	struct bfq_stat		empty_time;
	 
	u64				start_group_wait_time;
	u64				start_idle_time;
	u64				start_empty_time;
	uint16_t			flags;
#endif  
};

#ifdef CONFIG_BFQ_GROUP_IOSCHED

 
struct bfq_group_data {
	 
	struct blkcg_policy_data pd;

	unsigned int weight;
};

 
struct bfq_group {
	 
	struct blkg_policy_data pd;

	 
	char blkg_path[128];

	 
	refcount_t ref;

	struct bfq_entity entity;
	struct bfq_sched_data sched_data;

	struct bfq_data *bfqd;

	struct bfq_queue *async_bfqq[2][IOPRIO_NR_LEVELS][BFQ_MAX_ACTUATORS];
	struct bfq_queue *async_idle_bfqq[BFQ_MAX_ACTUATORS];

	struct bfq_entity *my_entity;

	int active_entities;
	int num_queues_with_pending_reqs;

	struct rb_root rq_pos_tree;

	struct bfqg_stats stats;
};

#else
struct bfq_group {
	struct bfq_entity entity;
	struct bfq_sched_data sched_data;

	struct bfq_queue *async_bfqq[2][IOPRIO_NR_LEVELS][BFQ_MAX_ACTUATORS];
	struct bfq_queue *async_idle_bfqq[BFQ_MAX_ACTUATORS];

	struct rb_root rq_pos_tree;
};
#endif

 

#define BFQ_SERVICE_TREE_INIT	((struct bfq_service_tree)		\
				{ RB_ROOT, RB_ROOT, NULL, NULL, 0, 0 })

extern const int bfq_timeout;

struct bfq_queue *bic_to_bfqq(struct bfq_io_cq *bic, bool is_sync,
				unsigned int actuator_idx);
void bic_set_bfqq(struct bfq_io_cq *bic, struct bfq_queue *bfqq, bool is_sync,
				unsigned int actuator_idx);
struct bfq_data *bic_to_bfqd(struct bfq_io_cq *bic);
void bfq_pos_tree_add_move(struct bfq_data *bfqd, struct bfq_queue *bfqq);
void bfq_weights_tree_add(struct bfq_queue *bfqq);
void bfq_weights_tree_remove(struct bfq_queue *bfqq);
void bfq_bfqq_expire(struct bfq_data *bfqd, struct bfq_queue *bfqq,
		     bool compensate, enum bfqq_expiration reason);
void bfq_put_queue(struct bfq_queue *bfqq);
void bfq_put_cooperator(struct bfq_queue *bfqq);
void bfq_end_wr_async_queues(struct bfq_data *bfqd, struct bfq_group *bfqg);
void bfq_release_process_ref(struct bfq_data *bfqd, struct bfq_queue *bfqq);
void bfq_schedule_dispatch(struct bfq_data *bfqd);
void bfq_put_async_queues(struct bfq_data *bfqd, struct bfq_group *bfqg);

 

 

void bfqg_stats_update_legacy_io(struct request_queue *q, struct request *rq);
void bfqg_stats_update_io_remove(struct bfq_group *bfqg, blk_opf_t opf);
void bfqg_stats_update_io_merged(struct bfq_group *bfqg, blk_opf_t opf);
void bfqg_stats_update_completion(struct bfq_group *bfqg, u64 start_time_ns,
				  u64 io_start_time_ns, blk_opf_t opf);
void bfqg_stats_update_dequeue(struct bfq_group *bfqg);
void bfqg_stats_set_start_idle_time(struct bfq_group *bfqg);
void bfq_bfqq_move(struct bfq_data *bfqd, struct bfq_queue *bfqq,
		   struct bfq_group *bfqg);

#ifdef CONFIG_BFQ_CGROUP_DEBUG
void bfqg_stats_update_io_add(struct bfq_group *bfqg, struct bfq_queue *bfqq,
			      blk_opf_t opf);
void bfqg_stats_set_start_empty_time(struct bfq_group *bfqg);
void bfqg_stats_update_idle_time(struct bfq_group *bfqg);
void bfqg_stats_update_avg_queue_size(struct bfq_group *bfqg);
#endif

void bfq_init_entity(struct bfq_entity *entity, struct bfq_group *bfqg);
void bfq_bic_update_cgroup(struct bfq_io_cq *bic, struct bio *bio);
void bfq_end_wr_async(struct bfq_data *bfqd);
struct bfq_group *bfq_bio_bfqg(struct bfq_data *bfqd, struct bio *bio);
struct blkcg_gq *bfqg_to_blkg(struct bfq_group *bfqg);
struct bfq_group *bfqq_group(struct bfq_queue *bfqq);
struct bfq_group *bfq_create_group_hierarchy(struct bfq_data *bfqd, int node);
void bfqg_and_blkg_put(struct bfq_group *bfqg);

#ifdef CONFIG_BFQ_GROUP_IOSCHED
extern struct cftype bfq_blkcg_legacy_files[];
extern struct cftype bfq_blkg_files[];
extern struct blkcg_policy blkcg_policy_bfq;
#endif

 

 

#ifdef CONFIG_BFQ_GROUP_IOSCHED
 
#define for_each_entity(entity)	\
	for (; entity ; entity = entity->parent)

 
#define for_each_entity_safe(entity, parent) \
	for (; entity && ({ parent = entity->parent; 1; }); entity = parent)

#else  
 
#define for_each_entity(entity)	\
	for (; entity ; entity = NULL)

#define for_each_entity_safe(entity, parent) \
	for (parent = NULL; entity ; entity = parent)
#endif  

struct bfq_queue *bfq_entity_to_bfqq(struct bfq_entity *entity);
unsigned int bfq_tot_busy_queues(struct bfq_data *bfqd);
struct bfq_service_tree *bfq_entity_service_tree(struct bfq_entity *entity);
struct bfq_entity *bfq_entity_of(struct rb_node *node);
unsigned short bfq_ioprio_to_weight(int ioprio);
void bfq_put_idle_entity(struct bfq_service_tree *st,
			 struct bfq_entity *entity);
struct bfq_service_tree *
__bfq_entity_update_weight_prio(struct bfq_service_tree *old_st,
				struct bfq_entity *entity,
				bool update_class_too);
void bfq_bfqq_served(struct bfq_queue *bfqq, int served);
void bfq_bfqq_charge_time(struct bfq_data *bfqd, struct bfq_queue *bfqq,
			  unsigned long time_ms);
bool __bfq_deactivate_entity(struct bfq_entity *entity,
			     bool ins_into_idle_tree);
bool next_queue_may_preempt(struct bfq_data *bfqd);
struct bfq_queue *bfq_get_next_queue(struct bfq_data *bfqd);
bool __bfq_bfqd_reset_in_service(struct bfq_data *bfqd);
void bfq_deactivate_bfqq(struct bfq_data *bfqd, struct bfq_queue *bfqq,
			 bool ins_into_idle_tree, bool expiration);
void bfq_activate_bfqq(struct bfq_data *bfqd, struct bfq_queue *bfqq);
void bfq_requeue_bfqq(struct bfq_data *bfqd, struct bfq_queue *bfqq,
		      bool expiration);
void bfq_del_bfqq_busy(struct bfq_queue *bfqq, bool expiration);
void bfq_add_bfqq_busy(struct bfq_queue *bfqq);
void bfq_add_bfqq_in_groups_with_pending_reqs(struct bfq_queue *bfqq);
void bfq_del_bfqq_in_groups_with_pending_reqs(struct bfq_queue *bfqq);

 

 
static inline void bfq_bfqq_name(struct bfq_queue *bfqq, char *str, int len)
{
	char type = bfq_bfqq_sync(bfqq) ? 'S' : 'A';

	if (bfqq->pid != -1)
		snprintf(str, len, "bfq%d%c", bfqq->pid, type);
	else
		snprintf(str, len, "bfqSHARED-%c", type);
}

#ifdef CONFIG_BFQ_GROUP_IOSCHED
struct bfq_group *bfqq_group(struct bfq_queue *bfqq);

#define bfq_log_bfqq(bfqd, bfqq, fmt, args...)	do {			\
	char pid_str[MAX_BFQQ_NAME_LENGTH];				\
	if (likely(!blk_trace_note_message_enabled((bfqd)->queue)))	\
		break;							\
	bfq_bfqq_name((bfqq), pid_str, MAX_BFQQ_NAME_LENGTH);		\
	blk_add_cgroup_trace_msg((bfqd)->queue,				\
			&bfqg_to_blkg(bfqq_group(bfqq))->blkcg->css,	\
			"%s " fmt, pid_str, ##args);			\
} while (0)

#define bfq_log_bfqg(bfqd, bfqg, fmt, args...)	do {			\
	blk_add_cgroup_trace_msg((bfqd)->queue,				\
		&bfqg_to_blkg(bfqg)->blkcg->css, fmt, ##args);		\
} while (0)

#else  

#define bfq_log_bfqq(bfqd, bfqq, fmt, args...) do {	\
	char pid_str[MAX_BFQQ_NAME_LENGTH];				\
	if (likely(!blk_trace_note_message_enabled((bfqd)->queue)))	\
		break;							\
	bfq_bfqq_name((bfqq), pid_str, MAX_BFQQ_NAME_LENGTH);		\
	blk_add_trace_msg((bfqd)->queue, "%s " fmt, pid_str, ##args);	\
} while (0)
#define bfq_log_bfqg(bfqd, bfqg, fmt, args...)		do {} while (0)

#endif  

#define bfq_log(bfqd, fmt, args...) \
	blk_add_trace_msg((bfqd)->queue, "bfq " fmt, ##args)

#endif  
