
 
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/cgroup.h>
#include <linux/ktime.h>
#include <linux/rbtree.h>
#include <linux/ioprio.h>
#include <linux/sbitmap.h>
#include <linux/delay.h>
#include <linux/backing-dev.h>

#include <trace/events/block.h>

#include "elevator.h"
#include "blk.h"
#include "blk-mq.h"
#include "blk-mq-sched.h"
#include "bfq-iosched.h"
#include "blk-wbt.h"

#define BFQ_BFQQ_FNS(name)						\
void bfq_mark_bfqq_##name(struct bfq_queue *bfqq)			\
{									\
	__set_bit(BFQQF_##name, &(bfqq)->flags);			\
}									\
void bfq_clear_bfqq_##name(struct bfq_queue *bfqq)			\
{									\
	__clear_bit(BFQQF_##name, &(bfqq)->flags);		\
}									\
int bfq_bfqq_##name(const struct bfq_queue *bfqq)			\
{									\
	return test_bit(BFQQF_##name, &(bfqq)->flags);		\
}

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
#undef BFQ_BFQQ_FNS						\

 
static const u64 bfq_fifo_expire[2] = { NSEC_PER_SEC / 4, NSEC_PER_SEC / 8 };

 
static const int bfq_back_max = 16 * 1024;

 
static const int bfq_back_penalty = 2;

 
static u64 bfq_slice_idle = NSEC_PER_SEC / 125;

 
static const int bfq_stats_min_budgets = 194;

 
static const int bfq_default_max_budget = 16 * 1024;

 
static const int bfq_async_charge_factor = 3;

 
const int bfq_timeout = HZ / 8;

 
static const unsigned long bfq_merge_time_limit = HZ/10;

static struct kmem_cache *bfq_pool;

 
#define BFQ_MIN_TT		(2 * NSEC_PER_MSEC)

 
#define BFQ_HW_QUEUE_THRESHOLD	3
#define BFQ_HW_QUEUE_SAMPLES	32

#define BFQQ_SEEK_THR		(sector_t)(8 * 100)
#define BFQQ_SECT_THR_NONROT	(sector_t)(2 * 32)
#define BFQ_RQ_SEEKY(bfqd, last_pos, rq) \
	(get_sdist(last_pos, rq) >			\
	 BFQQ_SEEK_THR &&				\
	 (!blk_queue_nonrot(bfqd->queue) ||		\
	  blk_rq_sectors(rq) < BFQQ_SECT_THR_NONROT))
#define BFQQ_CLOSE_THR		(sector_t)(8 * 1024)
#define BFQQ_SEEKY(bfqq)	(hweight32(bfqq->seek_history) > 19)
 
#define BFQQ_TOTALLY_SEEKY(bfqq)	(bfqq->seek_history == -1)

 
#define BFQ_RATE_MIN_SAMPLES	32
 
#define BFQ_RATE_MIN_INTERVAL	(300*NSEC_PER_MSEC)
 
#define BFQ_RATE_REF_INTERVAL	NSEC_PER_SEC

 
#define BFQ_RATE_SHIFT		16

 
static int ref_rate[2] = {14000, 33000};
 
static int ref_wr_duration[2];

 
static const unsigned long max_service_from_wr = 120000;

 
static const unsigned long bfq_activation_stable_merging = 600;
 
static const unsigned long bfq_late_stable_merging = 600;

#define RQ_BIC(rq)		((struct bfq_io_cq *)((rq)->elv.priv[0]))
#define RQ_BFQQ(rq)		((rq)->elv.priv[1])

struct bfq_queue *bic_to_bfqq(struct bfq_io_cq *bic, bool is_sync,
			      unsigned int actuator_idx)
{
	if (is_sync)
		return bic->bfqq[1][actuator_idx];

	return bic->bfqq[0][actuator_idx];
}

static void bfq_put_stable_ref(struct bfq_queue *bfqq);

void bic_set_bfqq(struct bfq_io_cq *bic,
		  struct bfq_queue *bfqq,
		  bool is_sync,
		  unsigned int actuator_idx)
{
	struct bfq_queue *old_bfqq = bic->bfqq[is_sync][actuator_idx];

	 
	struct bfq_iocq_bfqq_data *bfqq_data = &bic->bfqq_data[actuator_idx];

	 
	if (old_bfqq && old_bfqq->bic == bic)
		old_bfqq->bic = NULL;

	if (is_sync)
		bic->bfqq[1][actuator_idx] = bfqq;
	else
		bic->bfqq[0][actuator_idx] = bfqq;

	if (bfqq && bfqq_data->stable_merge_bfqq == bfqq) {
		 
		bfq_put_stable_ref(bfqq_data->stable_merge_bfqq);

		bfqq_data->stable_merge_bfqq = NULL;
	}
}

struct bfq_data *bic_to_bfqd(struct bfq_io_cq *bic)
{
	return bic->icq.q->elevator->elevator_data;
}

 
static struct bfq_io_cq *icq_to_bic(struct io_cq *icq)
{
	 
	return container_of(icq, struct bfq_io_cq, icq);
}

 
static struct bfq_io_cq *bfq_bic_lookup(struct request_queue *q)
{
	struct bfq_io_cq *icq;
	unsigned long flags;

	if (!current->io_context)
		return NULL;

	spin_lock_irqsave(&q->queue_lock, flags);
	icq = icq_to_bic(ioc_lookup_icq(q));
	spin_unlock_irqrestore(&q->queue_lock, flags);

	return icq;
}

 
void bfq_schedule_dispatch(struct bfq_data *bfqd)
{
	lockdep_assert_held(&bfqd->lock);

	if (bfqd->queued != 0) {
		bfq_log(bfqd, "schedule dispatch");
		blk_mq_run_hw_queues(bfqd->queue, true);
	}
}

#define bfq_class_idle(bfqq)	((bfqq)->ioprio_class == IOPRIO_CLASS_IDLE)

#define bfq_sample_valid(samples)	((samples) > 80)

 
static struct request *bfq_choose_req(struct bfq_data *bfqd,
				      struct request *rq1,
				      struct request *rq2,
				      sector_t last)
{
	sector_t s1, s2, d1 = 0, d2 = 0;
	unsigned long back_max;
#define BFQ_RQ1_WRAP	0x01  
#define BFQ_RQ2_WRAP	0x02  
	unsigned int wrap = 0;  

	if (!rq1 || rq1 == rq2)
		return rq2;
	if (!rq2)
		return rq1;

	if (rq_is_sync(rq1) && !rq_is_sync(rq2))
		return rq1;
	else if (rq_is_sync(rq2) && !rq_is_sync(rq1))
		return rq2;
	if ((rq1->cmd_flags & REQ_META) && !(rq2->cmd_flags & REQ_META))
		return rq1;
	else if ((rq2->cmd_flags & REQ_META) && !(rq1->cmd_flags & REQ_META))
		return rq2;

	s1 = blk_rq_pos(rq1);
	s2 = blk_rq_pos(rq2);

	 
	back_max = bfqd->bfq_back_max * 2;

	 
	if (s1 >= last)
		d1 = s1 - last;
	else if (s1 + back_max >= last)
		d1 = (last - s1) * bfqd->bfq_back_penalty;
	else
		wrap |= BFQ_RQ1_WRAP;

	if (s2 >= last)
		d2 = s2 - last;
	else if (s2 + back_max >= last)
		d2 = (last - s2) * bfqd->bfq_back_penalty;
	else
		wrap |= BFQ_RQ2_WRAP;

	 

	 
	switch (wrap) {
	case 0:  
		if (d1 < d2)
			return rq1;
		else if (d2 < d1)
			return rq2;

		if (s1 >= s2)
			return rq1;
		else
			return rq2;

	case BFQ_RQ2_WRAP:
		return rq1;
	case BFQ_RQ1_WRAP:
		return rq2;
	case BFQ_RQ1_WRAP|BFQ_RQ2_WRAP:  
	default:
		 
		if (s1 <= s2)
			return rq1;
		else
			return rq2;
	}
}

#define BFQ_LIMIT_INLINE_DEPTH 16

#ifdef CONFIG_BFQ_GROUP_IOSCHED
static bool bfqq_request_over_limit(struct bfq_queue *bfqq, int limit)
{
	struct bfq_data *bfqd = bfqq->bfqd;
	struct bfq_entity *entity = &bfqq->entity;
	struct bfq_entity *inline_entities[BFQ_LIMIT_INLINE_DEPTH];
	struct bfq_entity **entities = inline_entities;
	int depth, level, alloc_depth = BFQ_LIMIT_INLINE_DEPTH;
	int class_idx = bfqq->ioprio_class - 1;
	struct bfq_sched_data *sched_data;
	unsigned long wsum;
	bool ret = false;

	if (!entity->on_st_or_in_serv)
		return false;

retry:
	spin_lock_irq(&bfqd->lock);
	 
	depth = bfqg_to_blkg(bfqq_group(bfqq))->blkcg->css.cgroup->level + 1;
	if (depth > alloc_depth) {
		spin_unlock_irq(&bfqd->lock);
		if (entities != inline_entities)
			kfree(entities);
		entities = kmalloc_array(depth, sizeof(*entities), GFP_NOIO);
		if (!entities)
			return false;
		alloc_depth = depth;
		goto retry;
	}

	sched_data = entity->sched_data;
	 
	level = 0;
	for_each_entity(entity) {
		 
		if (!entity->on_st_or_in_serv)
			goto out;
		 
		if (WARN_ON_ONCE(level >= depth))
			break;
		entities[level++] = entity;
	}
	WARN_ON_ONCE(level != depth);
	for (level--; level >= 0; level--) {
		entity = entities[level];
		if (level > 0) {
			wsum = bfq_entity_service_tree(entity)->wsum;
		} else {
			int i;
			 
			wsum = 0;
			for (i = 0; i <= class_idx; i++) {
				wsum = wsum * IOPRIO_BE_NR +
					sched_data->service_tree[i].wsum;
			}
		}
		if (!wsum)
			continue;
		limit = DIV_ROUND_CLOSEST(limit * entity->weight, wsum);
		if (entity->allocated >= limit) {
			bfq_log_bfqq(bfqq->bfqd, bfqq,
				"too many requests: allocated %d limit %d level %d",
				entity->allocated, limit, level);
			ret = true;
			break;
		}
	}
out:
	spin_unlock_irq(&bfqd->lock);
	if (entities != inline_entities)
		kfree(entities);
	return ret;
}
#else
static bool bfqq_request_over_limit(struct bfq_queue *bfqq, int limit)
{
	return false;
}
#endif

 
static void bfq_limit_depth(blk_opf_t opf, struct blk_mq_alloc_data *data)
{
	struct bfq_data *bfqd = data->q->elevator->elevator_data;
	struct bfq_io_cq *bic = bfq_bic_lookup(data->q);
	int depth;
	unsigned limit = data->q->nr_requests;
	unsigned int act_idx;

	 
	if (op_is_sync(opf) && !op_is_write(opf)) {
		depth = 0;
	} else {
		depth = bfqd->word_depths[!!bfqd->wr_busy_queues][op_is_sync(opf)];
		limit = (limit * depth) >> bfqd->full_depth_shift;
	}

	for (act_idx = 0; bic && act_idx < bfqd->num_actuators; act_idx++) {
		struct bfq_queue *bfqq =
			bic_to_bfqq(bic, op_is_sync(opf), act_idx);

		 
		if (bfqq && bfqq_request_over_limit(bfqq, limit)) {
			depth = 1;
			break;
		}
	}
	bfq_log(bfqd, "[%s] wr_busy %d sync %d depth %u",
		__func__, bfqd->wr_busy_queues, op_is_sync(opf), depth);
	if (depth)
		data->shallow_depth = depth;
}

static struct bfq_queue *
bfq_rq_pos_tree_lookup(struct bfq_data *bfqd, struct rb_root *root,
		     sector_t sector, struct rb_node **ret_parent,
		     struct rb_node ***rb_link)
{
	struct rb_node **p, *parent;
	struct bfq_queue *bfqq = NULL;

	parent = NULL;
	p = &root->rb_node;
	while (*p) {
		struct rb_node **n;

		parent = *p;
		bfqq = rb_entry(parent, struct bfq_queue, pos_node);

		 
		if (sector > blk_rq_pos(bfqq->next_rq))
			n = &(*p)->rb_right;
		else if (sector < blk_rq_pos(bfqq->next_rq))
			n = &(*p)->rb_left;
		else
			break;
		p = n;
		bfqq = NULL;
	}

	*ret_parent = parent;
	if (rb_link)
		*rb_link = p;

	bfq_log(bfqd, "rq_pos_tree_lookup %llu: returning %d",
		(unsigned long long)sector,
		bfqq ? bfqq->pid : 0);

	return bfqq;
}

static bool bfq_too_late_for_merging(struct bfq_queue *bfqq)
{
	return bfqq->service_from_backlogged > 0 &&
		time_is_before_jiffies(bfqq->first_IO_time +
				       bfq_merge_time_limit);
}

 
void __cold
bfq_pos_tree_add_move(struct bfq_data *bfqd, struct bfq_queue *bfqq)
{
	struct rb_node **p, *parent;
	struct bfq_queue *__bfqq;

	if (bfqq->pos_root) {
		rb_erase(&bfqq->pos_node, bfqq->pos_root);
		bfqq->pos_root = NULL;
	}

	 
	if (bfqq == &bfqd->oom_bfqq)
		return;

	 
	if (bfq_too_late_for_merging(bfqq))
		return;

	if (bfq_class_idle(bfqq))
		return;
	if (!bfqq->next_rq)
		return;

	bfqq->pos_root = &bfqq_group(bfqq)->rq_pos_tree;
	__bfqq = bfq_rq_pos_tree_lookup(bfqd, bfqq->pos_root,
			blk_rq_pos(bfqq->next_rq), &parent, &p);
	if (!__bfqq) {
		rb_link_node(&bfqq->pos_node, parent, p);
		rb_insert_color(&bfqq->pos_node, bfqq->pos_root);
	} else
		bfqq->pos_root = NULL;
}

 
static bool bfq_asymmetric_scenario(struct bfq_data *bfqd,
				   struct bfq_queue *bfqq)
{
	bool smallest_weight = bfqq &&
		bfqq->weight_counter &&
		bfqq->weight_counter ==
		container_of(
			rb_first_cached(&bfqd->queue_weights_tree),
			struct bfq_weight_counter,
			weights_node);

	 
	bool varied_queue_weights = !smallest_weight &&
		!RB_EMPTY_ROOT(&bfqd->queue_weights_tree.rb_root) &&
		(bfqd->queue_weights_tree.rb_root.rb_node->rb_left ||
		 bfqd->queue_weights_tree.rb_root.rb_node->rb_right);

	bool multiple_classes_busy =
		(bfqd->busy_queues[0] && bfqd->busy_queues[1]) ||
		(bfqd->busy_queues[0] && bfqd->busy_queues[2]) ||
		(bfqd->busy_queues[1] && bfqd->busy_queues[2]);

	return varied_queue_weights || multiple_classes_busy
#ifdef CONFIG_BFQ_GROUP_IOSCHED
	       || bfqd->num_groups_with_pending_reqs > 1
#endif
		;
}

 
void bfq_weights_tree_add(struct bfq_queue *bfqq)
{
	struct rb_root_cached *root = &bfqq->bfqd->queue_weights_tree;
	struct bfq_entity *entity = &bfqq->entity;
	struct rb_node **new = &(root->rb_root.rb_node), *parent = NULL;
	bool leftmost = true;

	 
	if (bfqq->weight_counter)
		return;

	while (*new) {
		struct bfq_weight_counter *__counter = container_of(*new,
						struct bfq_weight_counter,
						weights_node);
		parent = *new;

		if (entity->weight == __counter->weight) {
			bfqq->weight_counter = __counter;
			goto inc_counter;
		}
		if (entity->weight < __counter->weight)
			new = &((*new)->rb_left);
		else {
			new = &((*new)->rb_right);
			leftmost = false;
		}
	}

	bfqq->weight_counter = kzalloc(sizeof(struct bfq_weight_counter),
				       GFP_ATOMIC);

	 
	if (unlikely(!bfqq->weight_counter))
		return;

	bfqq->weight_counter->weight = entity->weight;
	rb_link_node(&bfqq->weight_counter->weights_node, parent, new);
	rb_insert_color_cached(&bfqq->weight_counter->weights_node, root,
				leftmost);

inc_counter:
	bfqq->weight_counter->num_active++;
	bfqq->ref++;
}

 
void bfq_weights_tree_remove(struct bfq_queue *bfqq)
{
	struct rb_root_cached *root;

	if (!bfqq->weight_counter)
		return;

	root = &bfqq->bfqd->queue_weights_tree;
	bfqq->weight_counter->num_active--;
	if (bfqq->weight_counter->num_active > 0)
		goto reset_entity_pointer;

	rb_erase_cached(&bfqq->weight_counter->weights_node, root);
	kfree(bfqq->weight_counter);

reset_entity_pointer:
	bfqq->weight_counter = NULL;
	bfq_put_queue(bfqq);
}

 
static struct request *bfq_check_fifo(struct bfq_queue *bfqq,
				      struct request *last)
{
	struct request *rq;

	if (bfq_bfqq_fifo_expire(bfqq))
		return NULL;

	bfq_mark_bfqq_fifo_expire(bfqq);

	rq = rq_entry_fifo(bfqq->fifo.next);

	if (rq == last || ktime_get_ns() < rq->fifo_time)
		return NULL;

	bfq_log_bfqq(bfqq->bfqd, bfqq, "check_fifo: returned %p", rq);
	return rq;
}

static struct request *bfq_find_next_rq(struct bfq_data *bfqd,
					struct bfq_queue *bfqq,
					struct request *last)
{
	struct rb_node *rbnext = rb_next(&last->rb_node);
	struct rb_node *rbprev = rb_prev(&last->rb_node);
	struct request *next, *prev = NULL;

	 
	next = bfq_check_fifo(bfqq, last);
	if (next)
		return next;

	if (rbprev)
		prev = rb_entry_rq(rbprev);

	if (rbnext)
		next = rb_entry_rq(rbnext);
	else {
		rbnext = rb_first(&bfqq->sort_list);
		if (rbnext && rbnext != &last->rb_node)
			next = rb_entry_rq(rbnext);
	}

	return bfq_choose_req(bfqd, next, prev, blk_rq_pos(last));
}

 
static unsigned long bfq_serv_to_charge(struct request *rq,
					struct bfq_queue *bfqq)
{
	if (bfq_bfqq_sync(bfqq) || bfqq->wr_coeff > 1 ||
	    bfq_asymmetric_scenario(bfqq->bfqd, bfqq))
		return blk_rq_sectors(rq);

	return blk_rq_sectors(rq) * bfq_async_charge_factor;
}

 
static void bfq_updated_next_req(struct bfq_data *bfqd,
				 struct bfq_queue *bfqq)
{
	struct bfq_entity *entity = &bfqq->entity;
	struct request *next_rq = bfqq->next_rq;
	unsigned long new_budget;

	if (!next_rq)
		return;

	if (bfqq == bfqd->in_service_queue)
		 
		return;

	new_budget = max_t(unsigned long,
			   max_t(unsigned long, bfqq->max_budget,
				 bfq_serv_to_charge(next_rq, bfqq)),
			   entity->service);
	if (entity->budget != new_budget) {
		entity->budget = new_budget;
		bfq_log_bfqq(bfqd, bfqq, "updated next rq: new budget %lu",
					 new_budget);
		bfq_requeue_bfqq(bfqd, bfqq, false);
	}
}

static unsigned int bfq_wr_duration(struct bfq_data *bfqd)
{
	u64 dur;

	dur = bfqd->rate_dur_prod;
	do_div(dur, bfqd->peak_rate);

	 
	return clamp_val(dur, msecs_to_jiffies(3000), msecs_to_jiffies(25000));
}

 
static void switch_back_to_interactive_wr(struct bfq_queue *bfqq,
					  struct bfq_data *bfqd)
{
	bfqq->wr_coeff = bfqd->bfq_wr_coeff;
	bfqq->wr_cur_max_time = bfq_wr_duration(bfqd);
	bfqq->last_wr_start_finish = bfqq->wr_start_at_switch_to_srt;
}

static void
bfq_bfqq_resume_state(struct bfq_queue *bfqq, struct bfq_data *bfqd,
		      struct bfq_io_cq *bic, bool bfq_already_existing)
{
	unsigned int old_wr_coeff = 1;
	bool busy = bfq_already_existing && bfq_bfqq_busy(bfqq);
	unsigned int a_idx = bfqq->actuator_idx;
	struct bfq_iocq_bfqq_data *bfqq_data = &bic->bfqq_data[a_idx];

	if (bfqq_data->saved_has_short_ttime)
		bfq_mark_bfqq_has_short_ttime(bfqq);
	else
		bfq_clear_bfqq_has_short_ttime(bfqq);

	if (bfqq_data->saved_IO_bound)
		bfq_mark_bfqq_IO_bound(bfqq);
	else
		bfq_clear_bfqq_IO_bound(bfqq);

	bfqq->last_serv_time_ns = bfqq_data->saved_last_serv_time_ns;
	bfqq->inject_limit = bfqq_data->saved_inject_limit;
	bfqq->decrease_time_jif = bfqq_data->saved_decrease_time_jif;

	bfqq->entity.new_weight = bfqq_data->saved_weight;
	bfqq->ttime = bfqq_data->saved_ttime;
	bfqq->io_start_time = bfqq_data->saved_io_start_time;
	bfqq->tot_idle_time = bfqq_data->saved_tot_idle_time;
	 
	if (bfqd->low_latency) {
		old_wr_coeff = bfqq->wr_coeff;
		bfqq->wr_coeff = bfqq_data->saved_wr_coeff;
	}
	bfqq->service_from_wr = bfqq_data->saved_service_from_wr;
	bfqq->wr_start_at_switch_to_srt =
		bfqq_data->saved_wr_start_at_switch_to_srt;
	bfqq->last_wr_start_finish = bfqq_data->saved_last_wr_start_finish;
	bfqq->wr_cur_max_time = bfqq_data->saved_wr_cur_max_time;

	if (bfqq->wr_coeff > 1 && (bfq_bfqq_in_large_burst(bfqq) ||
	    time_is_before_jiffies(bfqq->last_wr_start_finish +
				   bfqq->wr_cur_max_time))) {
		if (bfqq->wr_cur_max_time == bfqd->bfq_wr_rt_max_time &&
		    !bfq_bfqq_in_large_burst(bfqq) &&
		    time_is_after_eq_jiffies(bfqq->wr_start_at_switch_to_srt +
					     bfq_wr_duration(bfqd))) {
			switch_back_to_interactive_wr(bfqq, bfqd);
		} else {
			bfqq->wr_coeff = 1;
			bfq_log_bfqq(bfqq->bfqd, bfqq,
				     "resume state: switching off wr");
		}
	}

	 
	bfqq->entity.prio_changed = 1;

	if (likely(!busy))
		return;

	if (old_wr_coeff == 1 && bfqq->wr_coeff > 1)
		bfqd->wr_busy_queues++;
	else if (old_wr_coeff > 1 && bfqq->wr_coeff == 1)
		bfqd->wr_busy_queues--;
}

static int bfqq_process_refs(struct bfq_queue *bfqq)
{
	return bfqq->ref - bfqq->entity.allocated -
		bfqq->entity.on_st_or_in_serv -
		(bfqq->weight_counter != NULL) - bfqq->stable_ref;
}

 
static void bfq_reset_burst_list(struct bfq_data *bfqd, struct bfq_queue *bfqq)
{
	struct bfq_queue *item;
	struct hlist_node *n;

	hlist_for_each_entry_safe(item, n, &bfqd->burst_list, burst_list_node)
		hlist_del_init(&item->burst_list_node);

	 
	if (bfq_tot_busy_queues(bfqd) == 0) {
		hlist_add_head(&bfqq->burst_list_node, &bfqd->burst_list);
		bfqd->burst_size = 1;
	} else
		bfqd->burst_size = 0;

	bfqd->burst_parent_entity = bfqq->entity.parent;
}

 
static void bfq_add_to_burst(struct bfq_data *bfqd, struct bfq_queue *bfqq)
{
	 
	bfqd->burst_size++;

	if (bfqd->burst_size == bfqd->bfq_large_burst_thresh) {
		struct bfq_queue *pos, *bfqq_item;
		struct hlist_node *n;

		 
		bfqd->large_burst = true;

		 
		hlist_for_each_entry(bfqq_item, &bfqd->burst_list,
				     burst_list_node)
			bfq_mark_bfqq_in_large_burst(bfqq_item);
		bfq_mark_bfqq_in_large_burst(bfqq);

		 
		hlist_for_each_entry_safe(pos, n, &bfqd->burst_list,
					  burst_list_node)
			hlist_del_init(&pos->burst_list_node);
	} else  
		hlist_add_head(&bfqq->burst_list_node, &bfqd->burst_list);
}

 
static void bfq_handle_burst(struct bfq_data *bfqd, struct bfq_queue *bfqq)
{
	 
	if (!hlist_unhashed(&bfqq->burst_list_node) ||
	    bfq_bfqq_in_large_burst(bfqq) ||
	    time_is_after_eq_jiffies(bfqq->split_time +
				     msecs_to_jiffies(10)))
		return;

	 
	if (time_is_before_jiffies(bfqd->last_ins_in_burst +
	    bfqd->bfq_burst_interval) ||
	    bfqq->entity.parent != bfqd->burst_parent_entity) {
		bfqd->large_burst = false;
		bfq_reset_burst_list(bfqd, bfqq);
		goto end;
	}

	 
	if (bfqd->large_burst) {
		bfq_mark_bfqq_in_large_burst(bfqq);
		goto end;
	}

	 
	bfq_add_to_burst(bfqd, bfqq);
end:
	 
	bfqd->last_ins_in_burst = jiffies;
}

static int bfq_bfqq_budget_left(struct bfq_queue *bfqq)
{
	struct bfq_entity *entity = &bfqq->entity;

	return entity->budget - entity->service;
}

 
static int bfq_max_budget(struct bfq_data *bfqd)
{
	if (bfqd->budgets_assigned < bfq_stats_min_budgets)
		return bfq_default_max_budget;
	else
		return bfqd->bfq_max_budget;
}

 
static int bfq_min_budget(struct bfq_data *bfqd)
{
	if (bfqd->budgets_assigned < bfq_stats_min_budgets)
		return bfq_default_max_budget / 32;
	else
		return bfqd->bfq_max_budget / 32;
}

 
static bool bfq_bfqq_update_budg_for_activation(struct bfq_data *bfqd,
						struct bfq_queue *bfqq,
						bool arrived_in_time)
{
	struct bfq_entity *entity = &bfqq->entity;

	 
	if (bfq_bfqq_non_blocking_wait_rq(bfqq) && arrived_in_time &&
	    bfq_bfqq_budget_left(bfqq) > 0) {
		 

		 
		entity->budget = min_t(unsigned long,
				       bfq_bfqq_budget_left(bfqq),
				       bfqq->max_budget);

		 
		entity->service = 0;

		return true;
	}

	 
	entity->service = 0;
	entity->budget = max_t(unsigned long, bfqq->max_budget,
			       bfq_serv_to_charge(bfqq->next_rq, bfqq));
	bfq_clear_bfqq_non_blocking_wait_rq(bfqq);
	return false;
}

 
static unsigned long bfq_smallest_from_now(void)
{
	return jiffies - MAX_JIFFY_OFFSET;
}

static void bfq_update_bfqq_wr_on_rq_arrival(struct bfq_data *bfqd,
					     struct bfq_queue *bfqq,
					     unsigned int old_wr_coeff,
					     bool wr_or_deserves_wr,
					     bool interactive,
					     bool in_burst,
					     bool soft_rt)
{
	if (old_wr_coeff == 1 && wr_or_deserves_wr) {
		 
		if (interactive) {
			bfqq->service_from_wr = 0;
			bfqq->wr_coeff = bfqd->bfq_wr_coeff;
			bfqq->wr_cur_max_time = bfq_wr_duration(bfqd);
		} else {
			 
			bfqq->wr_start_at_switch_to_srt =
				bfq_smallest_from_now();
			bfqq->wr_coeff = bfqd->bfq_wr_coeff *
				BFQ_SOFTRT_WEIGHT_FACTOR;
			bfqq->wr_cur_max_time =
				bfqd->bfq_wr_rt_max_time;
		}

		 
		bfqq->entity.budget = min_t(unsigned long,
					    bfqq->entity.budget,
					    2 * bfq_min_budget(bfqd));
	} else if (old_wr_coeff > 1) {
		if (interactive) {  
			bfqq->wr_coeff = bfqd->bfq_wr_coeff;
			bfqq->wr_cur_max_time = bfq_wr_duration(bfqd);
		} else if (in_burst)
			bfqq->wr_coeff = 1;
		else if (soft_rt) {
			 
			if (bfqq->wr_cur_max_time !=
				bfqd->bfq_wr_rt_max_time) {
				bfqq->wr_start_at_switch_to_srt =
					bfqq->last_wr_start_finish;

				bfqq->wr_cur_max_time =
					bfqd->bfq_wr_rt_max_time;
				bfqq->wr_coeff = bfqd->bfq_wr_coeff *
					BFQ_SOFTRT_WEIGHT_FACTOR;
			}
			bfqq->last_wr_start_finish = jiffies;
		}
	}
}

static bool bfq_bfqq_idle_for_long_time(struct bfq_data *bfqd,
					struct bfq_queue *bfqq)
{
	return bfqq->dispatched == 0 &&
		time_is_before_jiffies(
			bfqq->budget_timeout +
			bfqd->bfq_wr_min_idle_time);
}


 
static bool bfq_bfqq_higher_class_or_weight(struct bfq_queue *bfqq,
					    struct bfq_queue *in_serv_bfqq)
{
	int bfqq_weight, in_serv_weight;

	if (bfqq->ioprio_class < in_serv_bfqq->ioprio_class)
		return true;

	if (in_serv_bfqq->entity.parent == bfqq->entity.parent) {
		bfqq_weight = bfqq->entity.weight;
		in_serv_weight = in_serv_bfqq->entity.weight;
	} else {
		if (bfqq->entity.parent)
			bfqq_weight = bfqq->entity.parent->weight;
		else
			bfqq_weight = bfqq->entity.weight;
		if (in_serv_bfqq->entity.parent)
			in_serv_weight = in_serv_bfqq->entity.parent->weight;
		else
			in_serv_weight = in_serv_bfqq->entity.weight;
	}

	return bfqq_weight > in_serv_weight;
}

 
static unsigned int bfq_actuator_index(struct bfq_data *bfqd, struct bio *bio)
{
	unsigned int i;
	sector_t end;

	 
	if (bfqd->num_actuators == 1)
		return 0;

	 
	end = bio_end_sector(bio) - 1;

	for (i = 0; i < bfqd->num_actuators; i++) {
		if (end >= bfqd->sector[i] &&
		    end < bfqd->sector[i] + bfqd->nr_sectors[i])
			return i;
	}

	WARN_ONCE(true,
		  "bfq_actuator_index: bio sector out of ranges: end=%llu\n",
		  end);
	return 0;
}

static bool bfq_better_to_idle(struct bfq_queue *bfqq);

static void bfq_bfqq_handle_idle_busy_switch(struct bfq_data *bfqd,
					     struct bfq_queue *bfqq,
					     int old_wr_coeff,
					     struct request *rq,
					     bool *interactive)
{
	bool soft_rt, in_burst,	wr_or_deserves_wr,
		bfqq_wants_to_preempt,
		idle_for_long_time = bfq_bfqq_idle_for_long_time(bfqd, bfqq),
		 
		arrived_in_time =  ktime_get_ns() <=
			bfqq->ttime.last_end_request +
			bfqd->bfq_slice_idle * 3;
	unsigned int act_idx = bfq_actuator_index(bfqd, rq->bio);
	bool bfqq_non_merged_or_stably_merged =
		bfqq->bic || RQ_BIC(rq)->bfqq_data[act_idx].stably_merged;

	 
	in_burst = bfq_bfqq_in_large_burst(bfqq);
	soft_rt = bfqd->bfq_wr_max_softrt_rate > 0 &&
		!BFQQ_TOTALLY_SEEKY(bfqq) &&
		!in_burst &&
		time_is_before_jiffies(bfqq->soft_rt_next_start) &&
		bfqq->dispatched == 0 &&
		bfqq->entity.new_weight == 40;
	*interactive = !in_burst && idle_for_long_time &&
		bfqq->entity.new_weight == 40;
	 
	wr_or_deserves_wr = bfqd->low_latency &&
		(bfqq->wr_coeff > 1 ||
		 (bfq_bfqq_sync(bfqq) && bfqq_non_merged_or_stably_merged &&
		  (*interactive || soft_rt)));

	 
	bfqq_wants_to_preempt =
		bfq_bfqq_update_budg_for_activation(bfqd, bfqq,
						    arrived_in_time);

	 
	if (likely(!bfq_bfqq_just_created(bfqq)) &&
	    idle_for_long_time &&
	    time_is_before_jiffies(
		    bfqq->budget_timeout +
		    msecs_to_jiffies(10000))) {
		hlist_del_init(&bfqq->burst_list_node);
		bfq_clear_bfqq_in_large_burst(bfqq);
	}

	bfq_clear_bfqq_just_created(bfqq);

	if (bfqd->low_latency) {
		if (unlikely(time_is_after_jiffies(bfqq->split_time)))
			 
			bfqq->split_time =
				jiffies - bfqd->bfq_wr_min_idle_time - 1;

		if (time_is_before_jiffies(bfqq->split_time +
					   bfqd->bfq_wr_min_idle_time)) {
			bfq_update_bfqq_wr_on_rq_arrival(bfqd, bfqq,
							 old_wr_coeff,
							 wr_or_deserves_wr,
							 *interactive,
							 in_burst,
							 soft_rt);

			if (old_wr_coeff != bfqq->wr_coeff)
				bfqq->entity.prio_changed = 1;
		}
	}

	bfqq->last_idle_bklogged = jiffies;
	bfqq->service_from_backlogged = 0;
	bfq_clear_bfqq_softrt_update(bfqq);

	bfq_add_bfqq_busy(bfqq);

	 
	if (bfqd->in_service_queue &&
	    ((bfqq_wants_to_preempt &&
	      bfqq->wr_coeff >= bfqd->in_service_queue->wr_coeff) ||
	     bfq_bfqq_higher_class_or_weight(bfqq, bfqd->in_service_queue) ||
	     !bfq_better_to_idle(bfqd->in_service_queue)) &&
	    next_queue_may_preempt(bfqd))
		bfq_bfqq_expire(bfqd, bfqd->in_service_queue,
				false, BFQQE_PREEMPTED);
}

static void bfq_reset_inject_limit(struct bfq_data *bfqd,
				   struct bfq_queue *bfqq)
{
	 
	bfqq->last_serv_time_ns = 0;

	 
	bfqd->waited_rq = NULL;

	 
	if (bfq_bfqq_has_short_ttime(bfqq))
		bfqq->inject_limit = 0;
	else
		bfqq->inject_limit = 1;

	bfqq->decrease_time_jif = jiffies;
}

static void bfq_update_io_intensity(struct bfq_queue *bfqq, u64 now_ns)
{
	u64 tot_io_time = now_ns - bfqq->io_start_time;

	if (RB_EMPTY_ROOT(&bfqq->sort_list) && bfqq->dispatched == 0)
		bfqq->tot_idle_time +=
			now_ns - bfqq->ttime.last_end_request;

	if (unlikely(bfq_bfqq_just_created(bfqq)))
		return;

	 
	if (bfqq->tot_idle_time * 5 > tot_io_time)
		bfq_clear_bfqq_IO_bound(bfqq);
	else
		bfq_mark_bfqq_IO_bound(bfqq);

	 
	if (tot_io_time > 200 * NSEC_PER_MSEC) {
		bfqq->io_start_time = now_ns - (tot_io_time>>1);
		bfqq->tot_idle_time >>= 1;
	}
}

 
static void bfq_check_waker(struct bfq_data *bfqd, struct bfq_queue *bfqq,
			    u64 now_ns)
{
	char waker_name[MAX_BFQQ_NAME_LENGTH];

	if (!bfqd->last_completed_rq_bfqq ||
	    bfqd->last_completed_rq_bfqq == bfqq ||
	    bfq_bfqq_has_short_ttime(bfqq) ||
	    now_ns - bfqd->last_completion >= 4 * NSEC_PER_MSEC ||
	    bfqd->last_completed_rq_bfqq == &bfqd->oom_bfqq ||
	    bfqq == &bfqd->oom_bfqq)
		return;

	 
	if (bfqd->last_completed_rq_bfqq !=
	    bfqq->tentative_waker_bfqq ||
	    now_ns > bfqq->waker_detection_started +
					128 * (u64)bfqd->bfq_slice_idle) {
		 
		bfqq->tentative_waker_bfqq =
			bfqd->last_completed_rq_bfqq;
		bfqq->num_waker_detections = 1;
		bfqq->waker_detection_started = now_ns;
		bfq_bfqq_name(bfqq->tentative_waker_bfqq, waker_name,
			      MAX_BFQQ_NAME_LENGTH);
		bfq_log_bfqq(bfqd, bfqq, "set tentative waker %s", waker_name);
	} else  
		bfqq->num_waker_detections++;

	if (bfqq->num_waker_detections == 3) {
		bfqq->waker_bfqq = bfqd->last_completed_rq_bfqq;
		bfqq->tentative_waker_bfqq = NULL;
		bfq_bfqq_name(bfqq->waker_bfqq, waker_name,
			      MAX_BFQQ_NAME_LENGTH);
		bfq_log_bfqq(bfqd, bfqq, "set waker %s", waker_name);

		 
		if (!hlist_unhashed(&bfqq->woken_list_node))
			hlist_del_init(&bfqq->woken_list_node);
		hlist_add_head(&bfqq->woken_list_node,
			       &bfqd->last_completed_rq_bfqq->woken_list);
	}
}

static void bfq_add_request(struct request *rq)
{
	struct bfq_queue *bfqq = RQ_BFQQ(rq);
	struct bfq_data *bfqd = bfqq->bfqd;
	struct request *next_rq, *prev;
	unsigned int old_wr_coeff = bfqq->wr_coeff;
	bool interactive = false;
	u64 now_ns = ktime_get_ns();

	bfq_log_bfqq(bfqd, bfqq, "add_request %d", rq_is_sync(rq));
	bfqq->queued[rq_is_sync(rq)]++;
	 
	WRITE_ONCE(bfqd->queued, bfqd->queued + 1);

	if (bfq_bfqq_sync(bfqq) && RQ_BIC(rq)->requests <= 1) {
		bfq_check_waker(bfqd, bfqq, now_ns);

		 
		if (time_is_before_eq_jiffies(bfqq->decrease_time_jif +
					     msecs_to_jiffies(1000)))
			bfq_reset_inject_limit(bfqd, bfqq);

		 
		if (bfqq == bfqd->in_service_queue &&
		    (bfqd->tot_rq_in_driver == 0 ||
		     (bfqq->last_serv_time_ns > 0 &&
		      bfqd->rqs_injected && bfqd->tot_rq_in_driver > 0)) &&
		    time_is_before_eq_jiffies(bfqq->decrease_time_jif +
					      msecs_to_jiffies(10))) {
			bfqd->last_empty_occupied_ns = ktime_get_ns();
			 
			bfqd->wait_dispatch = true;
			 
			if (bfqd->tot_rq_in_driver == 0)
				bfqd->rqs_injected = false;
		}
	}

	if (bfq_bfqq_sync(bfqq))
		bfq_update_io_intensity(bfqq, now_ns);

	elv_rb_add(&bfqq->sort_list, rq);

	 
	prev = bfqq->next_rq;
	next_rq = bfq_choose_req(bfqd, bfqq->next_rq, rq, bfqd->last_position);
	bfqq->next_rq = next_rq;

	 
	if (unlikely(!bfqd->nonrot_with_queueing && prev != bfqq->next_rq))
		bfq_pos_tree_add_move(bfqd, bfqq);

	if (!bfq_bfqq_busy(bfqq))  
		bfq_bfqq_handle_idle_busy_switch(bfqd, bfqq, old_wr_coeff,
						 rq, &interactive);
	else {
		if (bfqd->low_latency && old_wr_coeff == 1 && !rq_is_sync(rq) &&
		    time_is_before_jiffies(
				bfqq->last_wr_start_finish +
				bfqd->bfq_wr_min_inter_arr_async)) {
			bfqq->wr_coeff = bfqd->bfq_wr_coeff;
			bfqq->wr_cur_max_time = bfq_wr_duration(bfqd);

			bfqd->wr_busy_queues++;
			bfqq->entity.prio_changed = 1;
		}
		if (prev != bfqq->next_rq)
			bfq_updated_next_req(bfqd, bfqq);
	}

	 
	if (bfqd->low_latency &&
		(old_wr_coeff == 1 || bfqq->wr_coeff == 1 || interactive))
		bfqq->last_wr_start_finish = jiffies;
}

static struct request *bfq_find_rq_fmerge(struct bfq_data *bfqd,
					  struct bio *bio,
					  struct request_queue *q)
{
	struct bfq_queue *bfqq = bfqd->bio_bfqq;


	if (bfqq)
		return elv_rb_find(&bfqq->sort_list, bio_end_sector(bio));

	return NULL;
}

static sector_t get_sdist(sector_t last_pos, struct request *rq)
{
	if (last_pos)
		return abs(blk_rq_pos(rq) - last_pos);

	return 0;
}

static void bfq_remove_request(struct request_queue *q,
			       struct request *rq)
{
	struct bfq_queue *bfqq = RQ_BFQQ(rq);
	struct bfq_data *bfqd = bfqq->bfqd;
	const int sync = rq_is_sync(rq);

	if (bfqq->next_rq == rq) {
		bfqq->next_rq = bfq_find_next_rq(bfqd, bfqq, rq);
		bfq_updated_next_req(bfqd, bfqq);
	}

	if (rq->queuelist.prev != &rq->queuelist)
		list_del_init(&rq->queuelist);
	bfqq->queued[sync]--;
	 
	WRITE_ONCE(bfqd->queued, bfqd->queued - 1);
	elv_rb_del(&bfqq->sort_list, rq);

	elv_rqhash_del(q, rq);
	if (q->last_merge == rq)
		q->last_merge = NULL;

	if (RB_EMPTY_ROOT(&bfqq->sort_list)) {
		bfqq->next_rq = NULL;

		if (bfq_bfqq_busy(bfqq) && bfqq != bfqd->in_service_queue) {
			bfq_del_bfqq_busy(bfqq, false);
			 
			bfqq->entity.budget = bfqq->entity.service = 0;
		}

		 
		if (bfqq->pos_root) {
			rb_erase(&bfqq->pos_node, bfqq->pos_root);
			bfqq->pos_root = NULL;
		}
	} else {
		 
		if (unlikely(!bfqd->nonrot_with_queueing))
			bfq_pos_tree_add_move(bfqd, bfqq);
	}

	if (rq->cmd_flags & REQ_META)
		bfqq->meta_pending--;

}

static bool bfq_bio_merge(struct request_queue *q, struct bio *bio,
		unsigned int nr_segs)
{
	struct bfq_data *bfqd = q->elevator->elevator_data;
	struct request *free = NULL;
	 
	struct bfq_io_cq *bic = bfq_bic_lookup(q);
	bool ret;

	spin_lock_irq(&bfqd->lock);

	if (bic) {
		 
		bfq_bic_update_cgroup(bic, bio);

		bfqd->bio_bfqq = bic_to_bfqq(bic, op_is_sync(bio->bi_opf),
					     bfq_actuator_index(bfqd, bio));
	} else {
		bfqd->bio_bfqq = NULL;
	}
	bfqd->bio_bic = bic;

	ret = blk_mq_sched_try_merge(q, bio, nr_segs, &free);

	spin_unlock_irq(&bfqd->lock);
	if (free)
		blk_mq_free_request(free);

	return ret;
}

static int bfq_request_merge(struct request_queue *q, struct request **req,
			     struct bio *bio)
{
	struct bfq_data *bfqd = q->elevator->elevator_data;
	struct request *__rq;

	__rq = bfq_find_rq_fmerge(bfqd, bio, q);
	if (__rq && elv_bio_merge_ok(__rq, bio)) {
		*req = __rq;

		if (blk_discard_mergable(__rq))
			return ELEVATOR_DISCARD_MERGE;
		return ELEVATOR_FRONT_MERGE;
	}

	return ELEVATOR_NO_MERGE;
}

static void bfq_request_merged(struct request_queue *q, struct request *req,
			       enum elv_merge type)
{
	if (type == ELEVATOR_FRONT_MERGE &&
	    rb_prev(&req->rb_node) &&
	    blk_rq_pos(req) <
	    blk_rq_pos(container_of(rb_prev(&req->rb_node),
				    struct request, rb_node))) {
		struct bfq_queue *bfqq = RQ_BFQQ(req);
		struct bfq_data *bfqd;
		struct request *prev, *next_rq;

		if (!bfqq)
			return;

		bfqd = bfqq->bfqd;

		 
		elv_rb_del(&bfqq->sort_list, req);
		elv_rb_add(&bfqq->sort_list, req);

		 
		prev = bfqq->next_rq;
		next_rq = bfq_choose_req(bfqd, bfqq->next_rq, req,
					 bfqd->last_position);
		bfqq->next_rq = next_rq;
		 
		if (prev != bfqq->next_rq) {
			bfq_updated_next_req(bfqd, bfqq);
			 
			if (unlikely(!bfqd->nonrot_with_queueing))
				bfq_pos_tree_add_move(bfqd, bfqq);
		}
	}
}

 
static void bfq_requests_merged(struct request_queue *q, struct request *rq,
				struct request *next)
{
	struct bfq_queue *bfqq = RQ_BFQQ(rq),
		*next_bfqq = RQ_BFQQ(next);

	if (!bfqq)
		goto remove;

	 
	if (bfqq == next_bfqq &&
	    !list_empty(&rq->queuelist) && !list_empty(&next->queuelist) &&
	    next->fifo_time < rq->fifo_time) {
		list_del_init(&rq->queuelist);
		list_replace_init(&next->queuelist, &rq->queuelist);
		rq->fifo_time = next->fifo_time;
	}

	if (bfqq->next_rq == next)
		bfqq->next_rq = rq;

	bfqg_stats_update_io_merged(bfqq_group(bfqq), next->cmd_flags);
remove:
	 
	if (!RB_EMPTY_NODE(&next->rb_node)) {
		bfq_remove_request(next->q, next);
		if (next_bfqq)
			bfqg_stats_update_io_remove(bfqq_group(next_bfqq),
						    next->cmd_flags);
	}
}

 
static void bfq_bfqq_end_wr(struct bfq_queue *bfqq)
{
	 

	if (bfqq->wr_cur_max_time !=
	    bfqq->bfqd->bfq_wr_rt_max_time)
		bfqq->soft_rt_next_start = jiffies;

	if (bfq_bfqq_busy(bfqq))
		bfqq->bfqd->wr_busy_queues--;
	bfqq->wr_coeff = 1;
	bfqq->wr_cur_max_time = 0;
	bfqq->last_wr_start_finish = jiffies;
	 
	bfqq->entity.prio_changed = 1;
}

void bfq_end_wr_async_queues(struct bfq_data *bfqd,
			     struct bfq_group *bfqg)
{
	int i, j, k;

	for (k = 0; k < bfqd->num_actuators; k++) {
		for (i = 0; i < 2; i++)
			for (j = 0; j < IOPRIO_NR_LEVELS; j++)
				if (bfqg->async_bfqq[i][j][k])
					bfq_bfqq_end_wr(bfqg->async_bfqq[i][j][k]);
		if (bfqg->async_idle_bfqq[k])
			bfq_bfqq_end_wr(bfqg->async_idle_bfqq[k]);
	}
}

static void bfq_end_wr(struct bfq_data *bfqd)
{
	struct bfq_queue *bfqq;
	int i;

	spin_lock_irq(&bfqd->lock);

	for (i = 0; i < bfqd->num_actuators; i++) {
		list_for_each_entry(bfqq, &bfqd->active_list[i], bfqq_list)
			bfq_bfqq_end_wr(bfqq);
	}
	list_for_each_entry(bfqq, &bfqd->idle_list, bfqq_list)
		bfq_bfqq_end_wr(bfqq);
	bfq_end_wr_async(bfqd);

	spin_unlock_irq(&bfqd->lock);
}

static sector_t bfq_io_struct_pos(void *io_struct, bool request)
{
	if (request)
		return blk_rq_pos(io_struct);
	else
		return ((struct bio *)io_struct)->bi_iter.bi_sector;
}

static int bfq_rq_close_to_sector(void *io_struct, bool request,
				  sector_t sector)
{
	return abs(bfq_io_struct_pos(io_struct, request) - sector) <=
	       BFQQ_CLOSE_THR;
}

static struct bfq_queue *bfqq_find_close(struct bfq_data *bfqd,
					 struct bfq_queue *bfqq,
					 sector_t sector)
{
	struct rb_root *root = &bfqq_group(bfqq)->rq_pos_tree;
	struct rb_node *parent, *node;
	struct bfq_queue *__bfqq;

	if (RB_EMPTY_ROOT(root))
		return NULL;

	 
	__bfqq = bfq_rq_pos_tree_lookup(bfqd, root, sector, &parent, NULL);
	if (__bfqq)
		return __bfqq;

	 
	__bfqq = rb_entry(parent, struct bfq_queue, pos_node);
	if (bfq_rq_close_to_sector(__bfqq->next_rq, true, sector))
		return __bfqq;

	if (blk_rq_pos(__bfqq->next_rq) < sector)
		node = rb_next(&__bfqq->pos_node);
	else
		node = rb_prev(&__bfqq->pos_node);
	if (!node)
		return NULL;

	__bfqq = rb_entry(node, struct bfq_queue, pos_node);
	if (bfq_rq_close_to_sector(__bfqq->next_rq, true, sector))
		return __bfqq;

	return NULL;
}

static struct bfq_queue *bfq_find_close_cooperator(struct bfq_data *bfqd,
						   struct bfq_queue *cur_bfqq,
						   sector_t sector)
{
	struct bfq_queue *bfqq;

	 
	bfqq = bfqq_find_close(bfqd, cur_bfqq, sector);
	if (!bfqq || bfqq == cur_bfqq)
		return NULL;

	return bfqq;
}

static struct bfq_queue *
bfq_setup_merge(struct bfq_queue *bfqq, struct bfq_queue *new_bfqq)
{
	int process_refs, new_process_refs;
	struct bfq_queue *__bfqq;

	 
	if (!bfqq_process_refs(new_bfqq))
		return NULL;

	 
	while ((__bfqq = new_bfqq->new_bfqq)) {
		if (__bfqq == bfqq)
			return NULL;
		new_bfqq = __bfqq;
	}

	process_refs = bfqq_process_refs(bfqq);
	new_process_refs = bfqq_process_refs(new_bfqq);
	 
	if (process_refs == 0 || new_process_refs == 0)
		return NULL;

	 
	if (new_bfqq->entity.parent != bfqq->entity.parent)
		return NULL;

	bfq_log_bfqq(bfqq->bfqd, bfqq, "scheduling merge with queue %d",
		new_bfqq->pid);

	 
	bfqq->new_bfqq = new_bfqq;
	 
	new_bfqq->ref += process_refs;
	return new_bfqq;
}

static bool bfq_may_be_close_cooperator(struct bfq_queue *bfqq,
					struct bfq_queue *new_bfqq)
{
	if (bfq_too_late_for_merging(new_bfqq))
		return false;

	if (bfq_class_idle(bfqq) || bfq_class_idle(new_bfqq) ||
	    (bfqq->ioprio_class != new_bfqq->ioprio_class))
		return false;

	 
	if (BFQQ_SEEKY(bfqq) || BFQQ_SEEKY(new_bfqq))
		return false;

	 
	if (!bfq_bfqq_sync(bfqq) || !bfq_bfqq_sync(new_bfqq))
		return false;

	return true;
}

static bool idling_boosts_thr_without_issues(struct bfq_data *bfqd,
					     struct bfq_queue *bfqq);

static struct bfq_queue *
bfq_setup_stable_merge(struct bfq_data *bfqd, struct bfq_queue *bfqq,
		       struct bfq_queue *stable_merge_bfqq,
		       struct bfq_iocq_bfqq_data *bfqq_data)
{
	int proc_ref = min(bfqq_process_refs(bfqq),
			   bfqq_process_refs(stable_merge_bfqq));
	struct bfq_queue *new_bfqq = NULL;

	bfqq_data->stable_merge_bfqq = NULL;
	if (idling_boosts_thr_without_issues(bfqd, bfqq) || proc_ref == 0)
		goto out;

	 
	new_bfqq = bfq_setup_merge(bfqq, stable_merge_bfqq);

	if (new_bfqq) {
		bfqq_data->stably_merged = true;
		if (new_bfqq->bic) {
			unsigned int new_a_idx = new_bfqq->actuator_idx;
			struct bfq_iocq_bfqq_data *new_bfqq_data =
				&new_bfqq->bic->bfqq_data[new_a_idx];

			new_bfqq_data->stably_merged = true;
		}
	}

out:
	 
	bfq_put_stable_ref(stable_merge_bfqq);

	return new_bfqq;
}

 
static struct bfq_queue *
bfq_setup_cooperator(struct bfq_data *bfqd, struct bfq_queue *bfqq,
		     void *io_struct, bool request, struct bfq_io_cq *bic)
{
	struct bfq_queue *in_service_bfqq, *new_bfqq;
	unsigned int a_idx = bfqq->actuator_idx;
	struct bfq_iocq_bfqq_data *bfqq_data = &bic->bfqq_data[a_idx];

	 
	if (bfqq->new_bfqq)
		return bfqq->new_bfqq;

	 
	if (unlikely(!bfqd->nonrot_with_queueing)) {
		 
		if (bfq_bfqq_sync(bfqq) && bfqq_data->stable_merge_bfqq &&
		    !bfq_bfqq_just_created(bfqq) &&
		    time_is_before_jiffies(bfqq->split_time +
					  msecs_to_jiffies(bfq_late_stable_merging)) &&
		    time_is_before_jiffies(bfqq->creation_time +
					   msecs_to_jiffies(bfq_late_stable_merging))) {
			struct bfq_queue *stable_merge_bfqq =
				bfqq_data->stable_merge_bfqq;

			return bfq_setup_stable_merge(bfqd, bfqq,
						      stable_merge_bfqq,
						      bfqq_data);
		}
	}

	 
	if (likely(bfqd->nonrot_with_queueing))
		return NULL;

	 
	if (bfq_too_late_for_merging(bfqq))
		return NULL;

	if (!io_struct || unlikely(bfqq == &bfqd->oom_bfqq))
		return NULL;

	 
	if (bfq_tot_busy_queues(bfqd) == 1)
		return NULL;

	in_service_bfqq = bfqd->in_service_queue;

	if (in_service_bfqq && in_service_bfqq != bfqq &&
	    likely(in_service_bfqq != &bfqd->oom_bfqq) &&
	    bfq_rq_close_to_sector(io_struct, request,
				   bfqd->in_serv_last_pos) &&
	    bfqq->entity.parent == in_service_bfqq->entity.parent &&
	    bfq_may_be_close_cooperator(bfqq, in_service_bfqq)) {
		new_bfqq = bfq_setup_merge(bfqq, in_service_bfqq);
		if (new_bfqq)
			return new_bfqq;
	}
	 
	new_bfqq = bfq_find_close_cooperator(bfqd, bfqq,
			bfq_io_struct_pos(io_struct, request));

	if (new_bfqq && likely(new_bfqq != &bfqd->oom_bfqq) &&
	    bfq_may_be_close_cooperator(bfqq, new_bfqq))
		return bfq_setup_merge(bfqq, new_bfqq);

	return NULL;
}

static void bfq_bfqq_save_state(struct bfq_queue *bfqq)
{
	struct bfq_io_cq *bic = bfqq->bic;
	unsigned int a_idx = bfqq->actuator_idx;
	struct bfq_iocq_bfqq_data *bfqq_data = &bic->bfqq_data[a_idx];

	 
	if (!bic)
		return;

	bfqq_data->saved_last_serv_time_ns = bfqq->last_serv_time_ns;
	bfqq_data->saved_inject_limit =	bfqq->inject_limit;
	bfqq_data->saved_decrease_time_jif = bfqq->decrease_time_jif;

	bfqq_data->saved_weight = bfqq->entity.orig_weight;
	bfqq_data->saved_ttime = bfqq->ttime;
	bfqq_data->saved_has_short_ttime =
		bfq_bfqq_has_short_ttime(bfqq);
	bfqq_data->saved_IO_bound = bfq_bfqq_IO_bound(bfqq);
	bfqq_data->saved_io_start_time = bfqq->io_start_time;
	bfqq_data->saved_tot_idle_time = bfqq->tot_idle_time;
	bfqq_data->saved_in_large_burst = bfq_bfqq_in_large_burst(bfqq);
	bfqq_data->was_in_burst_list =
		!hlist_unhashed(&bfqq->burst_list_node);

	if (unlikely(bfq_bfqq_just_created(bfqq) &&
		     !bfq_bfqq_in_large_burst(bfqq) &&
		     bfqq->bfqd->low_latency)) {
		 
		bfqq_data->saved_wr_coeff = bfqq->bfqd->bfq_wr_coeff;
		bfqq_data->saved_wr_start_at_switch_to_srt =
			bfq_smallest_from_now();
		bfqq_data->saved_wr_cur_max_time =
			bfq_wr_duration(bfqq->bfqd);
		bfqq_data->saved_last_wr_start_finish = jiffies;
	} else {
		bfqq_data->saved_wr_coeff = bfqq->wr_coeff;
		bfqq_data->saved_wr_start_at_switch_to_srt =
			bfqq->wr_start_at_switch_to_srt;
		bfqq_data->saved_service_from_wr =
			bfqq->service_from_wr;
		bfqq_data->saved_last_wr_start_finish =
			bfqq->last_wr_start_finish;
		bfqq_data->saved_wr_cur_max_time = bfqq->wr_cur_max_time;
	}
}


static void
bfq_reassign_last_bfqq(struct bfq_queue *cur_bfqq, struct bfq_queue *new_bfqq)
{
	if (cur_bfqq->entity.parent &&
	    cur_bfqq->entity.parent->last_bfqq_created == cur_bfqq)
		cur_bfqq->entity.parent->last_bfqq_created = new_bfqq;
	else if (cur_bfqq->bfqd && cur_bfqq->bfqd->last_bfqq_created == cur_bfqq)
		cur_bfqq->bfqd->last_bfqq_created = new_bfqq;
}

void bfq_release_process_ref(struct bfq_data *bfqd, struct bfq_queue *bfqq)
{
	 
	if (bfq_bfqq_busy(bfqq) && RB_EMPTY_ROOT(&bfqq->sort_list) &&
	    bfqq != bfqd->in_service_queue)
		bfq_del_bfqq_busy(bfqq, false);

	bfq_reassign_last_bfqq(bfqq, NULL);

	bfq_put_queue(bfqq);
}

static void
bfq_merge_bfqqs(struct bfq_data *bfqd, struct bfq_io_cq *bic,
		struct bfq_queue *bfqq, struct bfq_queue *new_bfqq)
{
	bfq_log_bfqq(bfqd, bfqq, "merging with queue %lu",
		(unsigned long)new_bfqq->pid);
	 
	bfq_bfqq_save_state(bfqq);
	bfq_bfqq_save_state(new_bfqq);
	if (bfq_bfqq_IO_bound(bfqq))
		bfq_mark_bfqq_IO_bound(new_bfqq);
	bfq_clear_bfqq_IO_bound(bfqq);

	 
	if (bfqq->waker_bfqq && !new_bfqq->waker_bfqq &&
	    bfqq->waker_bfqq != new_bfqq) {
		new_bfqq->waker_bfqq = bfqq->waker_bfqq;
		new_bfqq->tentative_waker_bfqq = NULL;

		 
		hlist_add_head(&new_bfqq->woken_list_node,
			       &new_bfqq->waker_bfqq->woken_list);

	}

	 
	if (new_bfqq->wr_coeff == 1 && bfqq->wr_coeff > 1) {
		new_bfqq->wr_coeff = bfqq->wr_coeff;
		new_bfqq->wr_cur_max_time = bfqq->wr_cur_max_time;
		new_bfqq->last_wr_start_finish = bfqq->last_wr_start_finish;
		new_bfqq->wr_start_at_switch_to_srt =
			bfqq->wr_start_at_switch_to_srt;
		if (bfq_bfqq_busy(new_bfqq))
			bfqd->wr_busy_queues++;
		new_bfqq->entity.prio_changed = 1;
	}

	if (bfqq->wr_coeff > 1) {  
		bfqq->wr_coeff = 1;
		bfqq->entity.prio_changed = 1;
		if (bfq_bfqq_busy(bfqq))
			bfqd->wr_busy_queues--;
	}

	bfq_log_bfqq(bfqd, new_bfqq, "merge_bfqqs: wr_busy %d",
		     bfqd->wr_busy_queues);

	 
	bic_set_bfqq(bic, new_bfqq, true, bfqq->actuator_idx);
	bfq_mark_bfqq_coop(new_bfqq);
	 
	new_bfqq->bic = NULL;
	 
	new_bfqq->pid = -1;
	bfqq->bic = NULL;

	bfq_reassign_last_bfqq(bfqq, new_bfqq);

	bfq_release_process_ref(bfqd, bfqq);
}

static bool bfq_allow_bio_merge(struct request_queue *q, struct request *rq,
				struct bio *bio)
{
	struct bfq_data *bfqd = q->elevator->elevator_data;
	bool is_sync = op_is_sync(bio->bi_opf);
	struct bfq_queue *bfqq = bfqd->bio_bfqq, *new_bfqq;

	 
	if (is_sync && !rq_is_sync(rq))
		return false;

	 
	if (!bfqq)
		return false;

	 
	new_bfqq = bfq_setup_cooperator(bfqd, bfqq, bio, false, bfqd->bio_bic);
	if (new_bfqq) {
		 
		bfq_merge_bfqqs(bfqd, bfqd->bio_bic, bfqq,
				new_bfqq);
		 
		bfqq = new_bfqq;

		 
		bfqd->bio_bfqq = bfqq;
	}

	return bfqq == RQ_BFQQ(rq);
}

 
static void bfq_set_budget_timeout(struct bfq_data *bfqd,
				   struct bfq_queue *bfqq)
{
	unsigned int timeout_coeff;

	if (bfqq->wr_cur_max_time == bfqd->bfq_wr_rt_max_time)
		timeout_coeff = 1;
	else
		timeout_coeff = bfqq->entity.weight / bfqq->entity.orig_weight;

	bfqd->last_budget_start = ktime_get();

	bfqq->budget_timeout = jiffies +
		bfqd->bfq_timeout * timeout_coeff;
}

static void __bfq_set_in_service_queue(struct bfq_data *bfqd,
				       struct bfq_queue *bfqq)
{
	if (bfqq) {
		bfq_clear_bfqq_fifo_expire(bfqq);

		bfqd->budgets_assigned = (bfqd->budgets_assigned * 7 + 256) / 8;

		if (time_is_before_jiffies(bfqq->last_wr_start_finish) &&
		    bfqq->wr_coeff > 1 &&
		    bfqq->wr_cur_max_time == bfqd->bfq_wr_rt_max_time &&
		    time_is_before_jiffies(bfqq->budget_timeout)) {
			 
			if (time_after(bfqq->budget_timeout,
				       bfqq->last_wr_start_finish))
				bfqq->last_wr_start_finish +=
					jiffies - bfqq->budget_timeout;
			else
				bfqq->last_wr_start_finish = jiffies;
		}

		bfq_set_budget_timeout(bfqd, bfqq);
		bfq_log_bfqq(bfqd, bfqq,
			     "set_in_service_queue, cur-budget = %d",
			     bfqq->entity.budget);
	}

	bfqd->in_service_queue = bfqq;
	bfqd->in_serv_last_pos = 0;
}

 
static struct bfq_queue *bfq_set_in_service_queue(struct bfq_data *bfqd)
{
	struct bfq_queue *bfqq = bfq_get_next_queue(bfqd);

	__bfq_set_in_service_queue(bfqd, bfqq);
	return bfqq;
}

static void bfq_arm_slice_timer(struct bfq_data *bfqd)
{
	struct bfq_queue *bfqq = bfqd->in_service_queue;
	u32 sl;

	bfq_mark_bfqq_wait_request(bfqq);

	 
	sl = bfqd->bfq_slice_idle;
	 
	if (BFQQ_SEEKY(bfqq) && bfqq->wr_coeff == 1 &&
	    !bfq_asymmetric_scenario(bfqd, bfqq))
		sl = min_t(u64, sl, BFQ_MIN_TT);
	else if (bfqq->wr_coeff > 1)
		sl = max_t(u32, sl, 20ULL * NSEC_PER_MSEC);

	bfqd->last_idling_start = ktime_get();
	bfqd->last_idling_start_jiffies = jiffies;

	hrtimer_start(&bfqd->idle_slice_timer, ns_to_ktime(sl),
		      HRTIMER_MODE_REL);
	bfqg_stats_set_start_idle_time(bfqq_group(bfqq));
}

 
static unsigned long bfq_calc_max_budget(struct bfq_data *bfqd)
{
	return (u64)bfqd->peak_rate * USEC_PER_MSEC *
		jiffies_to_msecs(bfqd->bfq_timeout)>>BFQ_RATE_SHIFT;
}

 
static void update_thr_responsiveness_params(struct bfq_data *bfqd)
{
	if (bfqd->bfq_user_max_budget == 0) {
		bfqd->bfq_max_budget =
			bfq_calc_max_budget(bfqd);
		bfq_log(bfqd, "new max_budget = %d", bfqd->bfq_max_budget);
	}
}

static void bfq_reset_rate_computation(struct bfq_data *bfqd,
				       struct request *rq)
{
	if (rq != NULL) {  
		bfqd->last_dispatch = bfqd->first_dispatch = ktime_get_ns();
		bfqd->peak_rate_samples = 1;
		bfqd->sequential_samples = 0;
		bfqd->tot_sectors_dispatched = bfqd->last_rq_max_size =
			blk_rq_sectors(rq);
	} else  
		bfqd->peak_rate_samples = 0;  

	bfq_log(bfqd,
		"reset_rate_computation at end, sample %u/%u tot_sects %llu",
		bfqd->peak_rate_samples, bfqd->sequential_samples,
		bfqd->tot_sectors_dispatched);
}

static void bfq_update_rate_reset(struct bfq_data *bfqd, struct request *rq)
{
	u32 rate, weight, divisor;

	 
	if (bfqd->peak_rate_samples < BFQ_RATE_MIN_SAMPLES ||
	    bfqd->delta_from_first < BFQ_RATE_MIN_INTERVAL)
		goto reset_computation;

	 
	bfqd->delta_from_first =
		max_t(u64, bfqd->delta_from_first,
		      bfqd->last_completion - bfqd->first_dispatch);

	 
	rate = div64_ul(bfqd->tot_sectors_dispatched<<BFQ_RATE_SHIFT,
			div_u64(bfqd->delta_from_first, NSEC_PER_USEC));

	 
	if ((bfqd->sequential_samples < (3 * bfqd->peak_rate_samples)>>2 &&
	     rate <= bfqd->peak_rate) ||
		rate > 20<<BFQ_RATE_SHIFT)
		goto reset_computation;

	 
	weight = (9 * bfqd->sequential_samples) / bfqd->peak_rate_samples;

	 
	weight = min_t(u32, 8,
		       div_u64(weight * bfqd->delta_from_first,
			       BFQ_RATE_REF_INTERVAL));

	 
	divisor = 10 - weight;

	 
	bfqd->peak_rate *= divisor-1;
	bfqd->peak_rate /= divisor;
	rate /= divisor;  

	bfqd->peak_rate += rate;

	 
	bfqd->peak_rate = max_t(u32, 1, bfqd->peak_rate);

	update_thr_responsiveness_params(bfqd);

reset_computation:
	bfq_reset_rate_computation(bfqd, rq);
}

 
static void bfq_update_peak_rate(struct bfq_data *bfqd, struct request *rq)
{
	u64 now_ns = ktime_get_ns();

	if (bfqd->peak_rate_samples == 0) {  
		bfq_log(bfqd, "update_peak_rate: goto reset, samples %d",
			bfqd->peak_rate_samples);
		bfq_reset_rate_computation(bfqd, rq);
		goto update_last_values;  
	}

	 
	if (now_ns - bfqd->last_dispatch > 100*NSEC_PER_MSEC &&
	    bfqd->tot_rq_in_driver == 0)
		goto update_rate_and_reset;

	 
	bfqd->peak_rate_samples++;

	if ((bfqd->tot_rq_in_driver > 0 ||
		now_ns - bfqd->last_completion < BFQ_MIN_TT)
	    && !BFQ_RQ_SEEKY(bfqd, bfqd->last_position, rq))
		bfqd->sequential_samples++;

	bfqd->tot_sectors_dispatched += blk_rq_sectors(rq);

	 
	if (likely(bfqd->peak_rate_samples % 32))
		bfqd->last_rq_max_size =
			max_t(u32, blk_rq_sectors(rq), bfqd->last_rq_max_size);
	else
		bfqd->last_rq_max_size = blk_rq_sectors(rq);

	bfqd->delta_from_first = now_ns - bfqd->first_dispatch;

	 
	if (bfqd->delta_from_first < BFQ_RATE_REF_INTERVAL)
		goto update_last_values;

update_rate_and_reset:
	bfq_update_rate_reset(bfqd, rq);
update_last_values:
	bfqd->last_position = blk_rq_pos(rq) + blk_rq_sectors(rq);
	if (RQ_BFQQ(rq) == bfqd->in_service_queue)
		bfqd->in_serv_last_pos = bfqd->last_position;
	bfqd->last_dispatch = now_ns;
}

 
static void bfq_dispatch_remove(struct request_queue *q, struct request *rq)
{
	struct bfq_queue *bfqq = RQ_BFQQ(rq);

	 
	bfqq->dispatched++;
	bfq_update_peak_rate(q->elevator->elevator_data, rq);

	bfq_remove_request(q, rq);
}

 
static bool idling_needed_for_service_guarantees(struct bfq_data *bfqd,
						 struct bfq_queue *bfqq)
{
	int tot_busy_queues = bfq_tot_busy_queues(bfqd);

	 
	if (unlikely(!bfqq_process_refs(bfqq)))
		return false;

	return (bfqq->wr_coeff > 1 &&
		(bfqd->wr_busy_queues < tot_busy_queues ||
		 bfqd->tot_rq_in_driver >= bfqq->dispatched + 4)) ||
		bfq_asymmetric_scenario(bfqd, bfqq) ||
		tot_busy_queues == 1;
}

static bool __bfq_bfqq_expire(struct bfq_data *bfqd, struct bfq_queue *bfqq,
			      enum bfqq_expiration reason)
{
	 
	if (bfq_bfqq_coop(bfqq) && BFQQ_SEEKY(bfqq))
		bfq_mark_bfqq_split_coop(bfqq);

	 
	if (RB_EMPTY_ROOT(&bfqq->sort_list) &&
	    !(reason == BFQQE_PREEMPTED &&
	      idling_needed_for_service_guarantees(bfqd, bfqq))) {
		if (bfqq->dispatched == 0)
			 
			bfqq->budget_timeout = jiffies;

		bfq_del_bfqq_busy(bfqq, true);
	} else {
		bfq_requeue_bfqq(bfqd, bfqq, true);
		 
		if (unlikely(!bfqd->nonrot_with_queueing &&
			     !RB_EMPTY_ROOT(&bfqq->sort_list)))
			bfq_pos_tree_add_move(bfqd, bfqq);
	}

	 
	return __bfq_bfqd_reset_in_service(bfqd);
}

 
static void __bfq_bfqq_recalc_budget(struct bfq_data *bfqd,
				     struct bfq_queue *bfqq,
				     enum bfqq_expiration reason)
{
	struct request *next_rq;
	int budget, min_budget;

	min_budget = bfq_min_budget(bfqd);

	if (bfqq->wr_coeff == 1)
		budget = bfqq->max_budget;
	else  
		budget = 2 * min_budget;

	bfq_log_bfqq(bfqd, bfqq, "recalc_budg: last budg %d, budg left %d",
		bfqq->entity.budget, bfq_bfqq_budget_left(bfqq));
	bfq_log_bfqq(bfqd, bfqq, "recalc_budg: last max_budg %d, min budg %d",
		budget, bfq_min_budget(bfqd));
	bfq_log_bfqq(bfqd, bfqq, "recalc_budg: sync %d, seeky %d",
		bfq_bfqq_sync(bfqq), BFQQ_SEEKY(bfqd->in_service_queue));

	if (bfq_bfqq_sync(bfqq) && bfqq->wr_coeff == 1) {
		switch (reason) {
		 
		case BFQQE_TOO_IDLE:
			 
			if (bfqq->dispatched > 0)  
				budget = min(budget * 2, bfqd->bfq_max_budget);
			else {
				if (budget > 5 * min_budget)
					budget -= 4 * min_budget;
				else
					budget = min_budget;
			}
			break;
		case BFQQE_BUDGET_TIMEOUT:
			 
			budget = min(budget * 2, bfqd->bfq_max_budget);
			break;
		case BFQQE_BUDGET_EXHAUSTED:
			 
			budget = min(budget * 4, bfqd->bfq_max_budget);
			break;
		case BFQQE_NO_MORE_REQUESTS:
			 
			budget = max_t(int, bfqq->entity.service, min_budget);
			break;
		default:
			return;
		}
	} else if (!bfq_bfqq_sync(bfqq)) {
		 
		budget = bfqd->bfq_max_budget;
	}

	bfqq->max_budget = budget;

	if (bfqd->budgets_assigned >= bfq_stats_min_budgets &&
	    !bfqd->bfq_user_max_budget)
		bfqq->max_budget = min(bfqq->max_budget, bfqd->bfq_max_budget);

	 
	next_rq = bfqq->next_rq;
	if (next_rq)
		bfqq->entity.budget = max_t(unsigned long, bfqq->max_budget,
					    bfq_serv_to_charge(next_rq, bfqq));

	bfq_log_bfqq(bfqd, bfqq, "head sect: %u, new budget %d",
			next_rq ? blk_rq_sectors(next_rq) : 0,
			bfqq->entity.budget);
}

 
static bool bfq_bfqq_is_slow(struct bfq_data *bfqd, struct bfq_queue *bfqq,
				 bool compensate, unsigned long *delta_ms)
{
	ktime_t delta_ktime;
	u32 delta_usecs;
	bool slow = BFQQ_SEEKY(bfqq);  

	if (!bfq_bfqq_sync(bfqq))
		return false;

	if (compensate)
		delta_ktime = bfqd->last_idling_start;
	else
		delta_ktime = ktime_get();
	delta_ktime = ktime_sub(delta_ktime, bfqd->last_budget_start);
	delta_usecs = ktime_to_us(delta_ktime);

	 
	if (delta_usecs < 1000) {
		if (blk_queue_nonrot(bfqd->queue))
			  
			*delta_ms = BFQ_MIN_TT / NSEC_PER_MSEC;
		else  
			*delta_ms = bfq_slice_idle / NSEC_PER_MSEC;

		return slow;
	}

	*delta_ms = delta_usecs / USEC_PER_MSEC;

	 
	if (delta_usecs > 20000) {
		 
		slow = bfqq->entity.service < bfqd->bfq_max_budget / 2;
	}

	bfq_log_bfqq(bfqd, bfqq, "bfq_bfqq_is_slow: slow %d", slow);

	return slow;
}

 
static unsigned long bfq_bfqq_softrt_next_start(struct bfq_data *bfqd,
						struct bfq_queue *bfqq)
{
	return max3(bfqq->soft_rt_next_start,
		    bfqq->last_idle_bklogged +
		    HZ * bfqq->service_from_backlogged /
		    bfqd->bfq_wr_max_softrt_rate,
		    jiffies + nsecs_to_jiffies(bfqq->bfqd->bfq_slice_idle) + 4);
}

 
void bfq_bfqq_expire(struct bfq_data *bfqd,
		     struct bfq_queue *bfqq,
		     bool compensate,
		     enum bfqq_expiration reason)
{
	bool slow;
	unsigned long delta = 0;
	struct bfq_entity *entity = &bfqq->entity;

	 
	slow = bfq_bfqq_is_slow(bfqd, bfqq, compensate, &delta);

	 
	if (bfqq->wr_coeff == 1 &&
	    (slow ||
	     (reason == BFQQE_BUDGET_TIMEOUT &&
	      bfq_bfqq_budget_left(bfqq) >=  entity->budget / 3)))
		bfq_bfqq_charge_time(bfqd, bfqq, delta);

	if (bfqd->low_latency && bfqq->wr_coeff == 1)
		bfqq->last_wr_start_finish = jiffies;

	if (bfqd->low_latency && bfqd->bfq_wr_max_softrt_rate > 0 &&
	    RB_EMPTY_ROOT(&bfqq->sort_list)) {
		 
		if (bfqq->dispatched == 0)
			bfqq->soft_rt_next_start =
				bfq_bfqq_softrt_next_start(bfqd, bfqq);
		else if (bfqq->dispatched > 0) {
			 
			bfq_mark_bfqq_softrt_update(bfqq);
		}
	}

	bfq_log_bfqq(bfqd, bfqq,
		"expire (%d, slow %d, num_disp %d, short_ttime %d)", reason,
		slow, bfqq->dispatched, bfq_bfqq_has_short_ttime(bfqq));

	 
	bfqd->rqs_injected = bfqd->wait_dispatch = false;
	bfqd->waited_rq = NULL;

	 
	__bfq_bfqq_recalc_budget(bfqd, bfqq, reason);
	if (__bfq_bfqq_expire(bfqd, bfqq, reason))
		 
		return;

	 
	if (!bfq_bfqq_busy(bfqq) &&
	    reason != BFQQE_BUDGET_TIMEOUT &&
	    reason != BFQQE_BUDGET_EXHAUSTED) {
		bfq_mark_bfqq_non_blocking_wait_rq(bfqq);
		 
	} else
		entity->service = 0;

	 
	entity = entity->parent;
	for_each_entity(entity)
		entity->service = 0;
}

 
static bool bfq_bfqq_budget_timeout(struct bfq_queue *bfqq)
{
	return time_is_before_eq_jiffies(bfqq->budget_timeout);
}

 
static bool bfq_may_expire_for_budg_timeout(struct bfq_queue *bfqq)
{
	bfq_log_bfqq(bfqq->bfqd, bfqq,
		"may_budget_timeout: wait_request %d left %d timeout %d",
		bfq_bfqq_wait_request(bfqq),
			bfq_bfqq_budget_left(bfqq) >=  bfqq->entity.budget / 3,
		bfq_bfqq_budget_timeout(bfqq));

	return (!bfq_bfqq_wait_request(bfqq) ||
		bfq_bfqq_budget_left(bfqq) >=  bfqq->entity.budget / 3)
		&&
		bfq_bfqq_budget_timeout(bfqq);
}

static bool idling_boosts_thr_without_issues(struct bfq_data *bfqd,
					     struct bfq_queue *bfqq)
{
	bool rot_without_queueing =
		!blk_queue_nonrot(bfqd->queue) && !bfqd->hw_tag,
		bfqq_sequential_and_IO_bound,
		idling_boosts_thr;

	 
	if (unlikely(!bfqq_process_refs(bfqq)))
		return false;

	bfqq_sequential_and_IO_bound = !BFQQ_SEEKY(bfqq) &&
		bfq_bfqq_IO_bound(bfqq) && bfq_bfqq_has_short_ttime(bfqq);

	 
	idling_boosts_thr = rot_without_queueing ||
		((!blk_queue_nonrot(bfqd->queue) || !bfqd->hw_tag) &&
		 bfqq_sequential_and_IO_bound);

	 
	return idling_boosts_thr &&
		bfqd->wr_busy_queues == 0;
}

 
static bool bfq_better_to_idle(struct bfq_queue *bfqq)
{
	struct bfq_data *bfqd = bfqq->bfqd;
	bool idling_boosts_thr_with_no_issue, idling_needed_for_service_guar;

	 
	if (unlikely(!bfqq_process_refs(bfqq)))
		return false;

	if (unlikely(bfqd->strict_guarantees))
		return true;

	 
	if (bfqd->bfq_slice_idle == 0 || !bfq_bfqq_sync(bfqq) ||
	   bfq_class_idle(bfqq))
		return false;

	idling_boosts_thr_with_no_issue =
		idling_boosts_thr_without_issues(bfqd, bfqq);

	idling_needed_for_service_guar =
		idling_needed_for_service_guarantees(bfqd, bfqq);

	 
	return idling_boosts_thr_with_no_issue ||
		idling_needed_for_service_guar;
}

 
static bool bfq_bfqq_must_idle(struct bfq_queue *bfqq)
{
	return RB_EMPTY_ROOT(&bfqq->sort_list) && bfq_better_to_idle(bfqq);
}

 
static struct bfq_queue *
bfq_choose_bfqq_for_injection(struct bfq_data *bfqd)
{
	struct bfq_queue *bfqq, *in_serv_bfqq = bfqd->in_service_queue;
	unsigned int limit = in_serv_bfqq->inject_limit;
	int i;

	 
	bool in_serv_always_inject = in_serv_bfqq->wr_coeff == 1 ||
		!bfq_bfqq_has_short_ttime(in_serv_bfqq);

	 
	if (limit == 0 && in_serv_bfqq->last_serv_time_ns == 0 &&
	    bfq_bfqq_wait_request(in_serv_bfqq) &&
	    time_is_before_eq_jiffies(bfqd->last_idling_start_jiffies +
				      bfqd->bfq_slice_idle)
		)
		limit = 1;

	if (bfqd->tot_rq_in_driver >= limit)
		return NULL;

	 
	for (i = 0; i < bfqd->num_actuators; i++) {
		list_for_each_entry(bfqq, &bfqd->active_list[i], bfqq_list)
			if (!RB_EMPTY_ROOT(&bfqq->sort_list) &&
				(in_serv_always_inject || bfqq->wr_coeff > 1) &&
				bfq_serv_to_charge(bfqq->next_rq, bfqq) <=
				bfq_bfqq_budget_left(bfqq)) {
			 
			if (blk_queue_nonrot(bfqd->queue) &&
			    blk_rq_sectors(bfqq->next_rq) >=
			    BFQQ_SECT_THR_NONROT &&
			    bfqd->tot_rq_in_driver >= 1)
				continue;
			else {
				bfqd->rqs_injected = true;
				return bfqq;
			}
		}
	}

	return NULL;
}

static struct bfq_queue *
bfq_find_active_bfqq_for_actuator(struct bfq_data *bfqd, int idx)
{
	struct bfq_queue *bfqq;

	if (bfqd->in_service_queue &&
	    bfqd->in_service_queue->actuator_idx == idx)
		return bfqd->in_service_queue;

	list_for_each_entry(bfqq, &bfqd->active_list[idx], bfqq_list) {
		if (!RB_EMPTY_ROOT(&bfqq->sort_list) &&
			bfq_serv_to_charge(bfqq->next_rq, bfqq) <=
				bfq_bfqq_budget_left(bfqq)) {
			return bfqq;
		}
	}

	return NULL;
}

 
static struct bfq_queue *
bfq_find_bfqq_for_underused_actuator(struct bfq_data *bfqd)
{
	int i;

	for (i = 0 ; i < bfqd->num_actuators; i++) {
		if (bfqd->rq_in_driver[i] < bfqd->actuator_load_threshold &&
		    (i == bfqd->num_actuators - 1 ||
		     bfqd->rq_in_driver[i] < bfqd->rq_in_driver[i+1])) {
			struct bfq_queue *bfqq =
				bfq_find_active_bfqq_for_actuator(bfqd, i);

			if (bfqq)
				return bfqq;
		}
	}

	return NULL;
}


 
static struct bfq_queue *bfq_select_queue(struct bfq_data *bfqd)
{
	struct bfq_queue *bfqq, *inject_bfqq;
	struct request *next_rq;
	enum bfqq_expiration reason = BFQQE_BUDGET_TIMEOUT;

	bfqq = bfqd->in_service_queue;
	if (!bfqq)
		goto new_queue;

	bfq_log_bfqq(bfqd, bfqq, "select_queue: already in-service queue");

	 
	if (bfq_may_expire_for_budg_timeout(bfqq) &&
	    !bfq_bfqq_must_idle(bfqq))
		goto expire;

check_queue:
	 
	inject_bfqq = bfq_find_bfqq_for_underused_actuator(bfqd);
	if (inject_bfqq && inject_bfqq != bfqq)
		return inject_bfqq;

	 
	next_rq = bfqq->next_rq;
	 
	if (next_rq) {
		if (bfq_serv_to_charge(next_rq, bfqq) >
			bfq_bfqq_budget_left(bfqq)) {
			 
			reason = BFQQE_BUDGET_EXHAUSTED;
			goto expire;
		} else {
			 
			if (bfq_bfqq_wait_request(bfqq)) {
				 
				bfq_clear_bfqq_wait_request(bfqq);
				hrtimer_try_to_cancel(&bfqd->idle_slice_timer);
			}
			goto keep_queue;
		}
	}

	 
	if (bfq_bfqq_wait_request(bfqq) ||
	    (bfqq->dispatched != 0 && bfq_better_to_idle(bfqq))) {
		unsigned int act_idx = bfqq->actuator_idx;
		struct bfq_queue *async_bfqq = NULL;
		struct bfq_queue *blocked_bfqq =
			!hlist_empty(&bfqq->woken_list) ?
			container_of(bfqq->woken_list.first,
				     struct bfq_queue,
				     woken_list_node)
			: NULL;

		if (bfqq->bic && bfqq->bic->bfqq[0][act_idx] &&
		    bfq_bfqq_busy(bfqq->bic->bfqq[0][act_idx]) &&
		    bfqq->bic->bfqq[0][act_idx]->next_rq)
			async_bfqq = bfqq->bic->bfqq[0][act_idx];
		 
		if (async_bfqq &&
		    icq_to_bic(async_bfqq->next_rq->elv.icq) == bfqq->bic &&
		    bfq_serv_to_charge(async_bfqq->next_rq, async_bfqq) <=
		    bfq_bfqq_budget_left(async_bfqq))
			bfqq = async_bfqq;
		else if (bfqq->waker_bfqq &&
			   bfq_bfqq_busy(bfqq->waker_bfqq) &&
			   bfqq->waker_bfqq->next_rq &&
			   bfq_serv_to_charge(bfqq->waker_bfqq->next_rq,
					      bfqq->waker_bfqq) <=
			   bfq_bfqq_budget_left(bfqq->waker_bfqq)
			)
			bfqq = bfqq->waker_bfqq;
		else if (blocked_bfqq &&
			   bfq_bfqq_busy(blocked_bfqq) &&
			   blocked_bfqq->next_rq &&
			   bfq_serv_to_charge(blocked_bfqq->next_rq,
					      blocked_bfqq) <=
			   bfq_bfqq_budget_left(blocked_bfqq)
			)
			bfqq = blocked_bfqq;
		else if (!idling_boosts_thr_without_issues(bfqd, bfqq) &&
			 (bfqq->wr_coeff == 1 || bfqd->wr_busy_queues > 1 ||
			  !bfq_bfqq_has_short_ttime(bfqq)))
			bfqq = bfq_choose_bfqq_for_injection(bfqd);
		else
			bfqq = NULL;

		goto keep_queue;
	}

	reason = BFQQE_NO_MORE_REQUESTS;
expire:
	bfq_bfqq_expire(bfqd, bfqq, false, reason);
new_queue:
	bfqq = bfq_set_in_service_queue(bfqd);
	if (bfqq) {
		bfq_log_bfqq(bfqd, bfqq, "select_queue: checking new queue");
		goto check_queue;
	}
keep_queue:
	if (bfqq)
		bfq_log_bfqq(bfqd, bfqq, "select_queue: returned this queue");
	else
		bfq_log(bfqd, "select_queue: no queue returned");

	return bfqq;
}

static void bfq_update_wr_data(struct bfq_data *bfqd, struct bfq_queue *bfqq)
{
	struct bfq_entity *entity = &bfqq->entity;

	if (bfqq->wr_coeff > 1) {  
		bfq_log_bfqq(bfqd, bfqq,
			"raising period dur %u/%u msec, old coeff %u, w %d(%d)",
			jiffies_to_msecs(jiffies - bfqq->last_wr_start_finish),
			jiffies_to_msecs(bfqq->wr_cur_max_time),
			bfqq->wr_coeff,
			bfqq->entity.weight, bfqq->entity.orig_weight);

		if (entity->prio_changed)
			bfq_log_bfqq(bfqd, bfqq, "WARN: pending prio change");

		 
		if (bfq_bfqq_in_large_burst(bfqq))
			bfq_bfqq_end_wr(bfqq);
		else if (time_is_before_jiffies(bfqq->last_wr_start_finish +
						bfqq->wr_cur_max_time)) {
			if (bfqq->wr_cur_max_time != bfqd->bfq_wr_rt_max_time ||
			time_is_before_jiffies(bfqq->wr_start_at_switch_to_srt +
					       bfq_wr_duration(bfqd))) {
				 
				bfq_bfqq_end_wr(bfqq);
			} else {  
				switch_back_to_interactive_wr(bfqq, bfqd);
				bfqq->entity.prio_changed = 1;
			}
		}
		if (bfqq->wr_coeff > 1 &&
		    bfqq->wr_cur_max_time != bfqd->bfq_wr_rt_max_time &&
		    bfqq->service_from_wr > max_service_from_wr) {
			 
			bfq_bfqq_end_wr(bfqq);
		}
	}
	 
	if ((entity->weight > entity->orig_weight) != (bfqq->wr_coeff > 1))
		__bfq_entity_update_weight_prio(bfq_entity_service_tree(entity),
						entity, false);
}

 
static struct request *bfq_dispatch_rq_from_bfqq(struct bfq_data *bfqd,
						 struct bfq_queue *bfqq)
{
	struct request *rq = bfqq->next_rq;
	unsigned long service_to_charge;

	service_to_charge = bfq_serv_to_charge(rq, bfqq);

	bfq_bfqq_served(bfqq, service_to_charge);

	if (bfqq == bfqd->in_service_queue && bfqd->wait_dispatch) {
		bfqd->wait_dispatch = false;
		bfqd->waited_rq = rq;
	}

	bfq_dispatch_remove(bfqd->queue, rq);

	if (bfqq != bfqd->in_service_queue)
		return rq;

	 
	bfq_update_wr_data(bfqd, bfqq);

	 
	if (bfq_tot_busy_queues(bfqd) > 1 && bfq_class_idle(bfqq))
		bfq_bfqq_expire(bfqd, bfqq, false, BFQQE_BUDGET_EXHAUSTED);

	return rq;
}

static bool bfq_has_work(struct blk_mq_hw_ctx *hctx)
{
	struct bfq_data *bfqd = hctx->queue->elevator->elevator_data;

	 
	return !list_empty_careful(&bfqd->dispatch) ||
		READ_ONCE(bfqd->queued);
}

static struct request *__bfq_dispatch_request(struct blk_mq_hw_ctx *hctx)
{
	struct bfq_data *bfqd = hctx->queue->elevator->elevator_data;
	struct request *rq = NULL;
	struct bfq_queue *bfqq = NULL;

	if (!list_empty(&bfqd->dispatch)) {
		rq = list_first_entry(&bfqd->dispatch, struct request,
				      queuelist);
		list_del_init(&rq->queuelist);

		bfqq = RQ_BFQQ(rq);

		if (bfqq) {
			 
			bfqq->dispatched++;

			goto inc_in_driver_start_rq;
		}

		 
		goto start_rq;
	}

	bfq_log(bfqd, "dispatch requests: %d busy queues",
		bfq_tot_busy_queues(bfqd));

	if (bfq_tot_busy_queues(bfqd) == 0)
		goto exit;

	 
	if (bfqd->strict_guarantees && bfqd->tot_rq_in_driver > 0)
		goto exit;

	bfqq = bfq_select_queue(bfqd);
	if (!bfqq)
		goto exit;

	rq = bfq_dispatch_rq_from_bfqq(bfqd, bfqq);

	if (rq) {
inc_in_driver_start_rq:
		bfqd->rq_in_driver[bfqq->actuator_idx]++;
		bfqd->tot_rq_in_driver++;
start_rq:
		rq->rq_flags |= RQF_STARTED;
	}
exit:
	return rq;
}

#ifdef CONFIG_BFQ_CGROUP_DEBUG
static void bfq_update_dispatch_stats(struct request_queue *q,
				      struct request *rq,
				      struct bfq_queue *in_serv_queue,
				      bool idle_timer_disabled)
{
	struct bfq_queue *bfqq = rq ? RQ_BFQQ(rq) : NULL;

	if (!idle_timer_disabled && !bfqq)
		return;

	 
	spin_lock_irq(&q->queue_lock);
	if (idle_timer_disabled)
		 
		bfqg_stats_update_idle_time(bfqq_group(in_serv_queue));
	if (bfqq) {
		struct bfq_group *bfqg = bfqq_group(bfqq);

		bfqg_stats_update_avg_queue_size(bfqg);
		bfqg_stats_set_start_empty_time(bfqg);
		bfqg_stats_update_io_remove(bfqg, rq->cmd_flags);
	}
	spin_unlock_irq(&q->queue_lock);
}
#else
static inline void bfq_update_dispatch_stats(struct request_queue *q,
					     struct request *rq,
					     struct bfq_queue *in_serv_queue,
					     bool idle_timer_disabled) {}
#endif  

static struct request *bfq_dispatch_request(struct blk_mq_hw_ctx *hctx)
{
	struct bfq_data *bfqd = hctx->queue->elevator->elevator_data;
	struct request *rq;
	struct bfq_queue *in_serv_queue;
	bool waiting_rq, idle_timer_disabled = false;

	spin_lock_irq(&bfqd->lock);

	in_serv_queue = bfqd->in_service_queue;
	waiting_rq = in_serv_queue && bfq_bfqq_wait_request(in_serv_queue);

	rq = __bfq_dispatch_request(hctx);
	if (in_serv_queue == bfqd->in_service_queue) {
		idle_timer_disabled =
			waiting_rq && !bfq_bfqq_wait_request(in_serv_queue);
	}

	spin_unlock_irq(&bfqd->lock);
	bfq_update_dispatch_stats(hctx->queue, rq,
			idle_timer_disabled ? in_serv_queue : NULL,
				idle_timer_disabled);

	return rq;
}

 
void bfq_put_queue(struct bfq_queue *bfqq)
{
	struct bfq_queue *item;
	struct hlist_node *n;
	struct bfq_group *bfqg = bfqq_group(bfqq);

	bfq_log_bfqq(bfqq->bfqd, bfqq, "put_queue: %p %d", bfqq, bfqq->ref);

	bfqq->ref--;
	if (bfqq->ref)
		return;

	if (!hlist_unhashed(&bfqq->burst_list_node)) {
		hlist_del_init(&bfqq->burst_list_node);
		 
		if (bfqq->bic && bfqq->bfqd->burst_size > 0)
			bfqq->bfqd->burst_size--;
	}

	 
	 
	if (!hlist_unhashed(&bfqq->woken_list_node))
		hlist_del_init(&bfqq->woken_list_node);

	 
	hlist_for_each_entry_safe(item, n, &bfqq->woken_list,
				  woken_list_node) {
		item->waker_bfqq = NULL;
		hlist_del_init(&item->woken_list_node);
	}

	if (bfqq->bfqd->last_completed_rq_bfqq == bfqq)
		bfqq->bfqd->last_completed_rq_bfqq = NULL;

	WARN_ON_ONCE(!list_empty(&bfqq->fifo));
	WARN_ON_ONCE(!RB_EMPTY_ROOT(&bfqq->sort_list));
	WARN_ON_ONCE(bfqq->dispatched);

	kmem_cache_free(bfq_pool, bfqq);
	bfqg_and_blkg_put(bfqg);
}

static void bfq_put_stable_ref(struct bfq_queue *bfqq)
{
	bfqq->stable_ref--;
	bfq_put_queue(bfqq);
}

void bfq_put_cooperator(struct bfq_queue *bfqq)
{
	struct bfq_queue *__bfqq, *next;

	 
	__bfqq = bfqq->new_bfqq;
	while (__bfqq) {
		next = __bfqq->new_bfqq;
		bfq_put_queue(__bfqq);
		__bfqq = next;
	}
}

static void bfq_exit_bfqq(struct bfq_data *bfqd, struct bfq_queue *bfqq)
{
	if (bfqq == bfqd->in_service_queue) {
		__bfq_bfqq_expire(bfqd, bfqq, BFQQE_BUDGET_TIMEOUT);
		bfq_schedule_dispatch(bfqd);
	}

	bfq_log_bfqq(bfqd, bfqq, "exit_bfqq: %p, %d", bfqq, bfqq->ref);

	bfq_put_cooperator(bfqq);

	bfq_release_process_ref(bfqd, bfqq);
}

static void bfq_exit_icq_bfqq(struct bfq_io_cq *bic, bool is_sync,
			      unsigned int actuator_idx)
{
	struct bfq_queue *bfqq = bic_to_bfqq(bic, is_sync, actuator_idx);
	struct bfq_data *bfqd;

	if (bfqq)
		bfqd = bfqq->bfqd;  

	if (bfqq && bfqd) {
		bic_set_bfqq(bic, NULL, is_sync, actuator_idx);
		bfq_exit_bfqq(bfqd, bfqq);
	}
}

static void bfq_exit_icq(struct io_cq *icq)
{
	struct bfq_io_cq *bic = icq_to_bic(icq);
	struct bfq_data *bfqd = bic_to_bfqd(bic);
	unsigned long flags;
	unsigned int act_idx;
	 
	unsigned int num_actuators = BFQ_MAX_ACTUATORS;
	struct bfq_iocq_bfqq_data *bfqq_data = bic->bfqq_data;

	 
	if (bfqd) {
		spin_lock_irqsave(&bfqd->lock, flags);
		num_actuators = bfqd->num_actuators;
	}

	for (act_idx = 0; act_idx < num_actuators; act_idx++) {
		if (bfqq_data[act_idx].stable_merge_bfqq)
			bfq_put_stable_ref(bfqq_data[act_idx].stable_merge_bfqq);

		bfq_exit_icq_bfqq(bic, true, act_idx);
		bfq_exit_icq_bfqq(bic, false, act_idx);
	}

	if (bfqd)
		spin_unlock_irqrestore(&bfqd->lock, flags);
}

 
static void
bfq_set_next_ioprio_data(struct bfq_queue *bfqq, struct bfq_io_cq *bic)
{
	struct task_struct *tsk = current;
	int ioprio_class;
	struct bfq_data *bfqd = bfqq->bfqd;

	if (!bfqd)
		return;

	ioprio_class = IOPRIO_PRIO_CLASS(bic->ioprio);
	switch (ioprio_class) {
	default:
		pr_err("bdi %s: bfq: bad prio class %d\n",
			bdi_dev_name(bfqq->bfqd->queue->disk->bdi),
			ioprio_class);
		fallthrough;
	case IOPRIO_CLASS_NONE:
		 
		bfqq->new_ioprio = task_nice_ioprio(tsk);
		bfqq->new_ioprio_class = task_nice_ioclass(tsk);
		break;
	case IOPRIO_CLASS_RT:
		bfqq->new_ioprio = IOPRIO_PRIO_LEVEL(bic->ioprio);
		bfqq->new_ioprio_class = IOPRIO_CLASS_RT;
		break;
	case IOPRIO_CLASS_BE:
		bfqq->new_ioprio = IOPRIO_PRIO_LEVEL(bic->ioprio);
		bfqq->new_ioprio_class = IOPRIO_CLASS_BE;
		break;
	case IOPRIO_CLASS_IDLE:
		bfqq->new_ioprio_class = IOPRIO_CLASS_IDLE;
		bfqq->new_ioprio = IOPRIO_NR_LEVELS - 1;
		break;
	}

	if (bfqq->new_ioprio >= IOPRIO_NR_LEVELS) {
		pr_crit("bfq_set_next_ioprio_data: new_ioprio %d\n",
			bfqq->new_ioprio);
		bfqq->new_ioprio = IOPRIO_NR_LEVELS - 1;
	}

	bfqq->entity.new_weight = bfq_ioprio_to_weight(bfqq->new_ioprio);
	bfq_log_bfqq(bfqd, bfqq, "new_ioprio %d new_weight %d",
		     bfqq->new_ioprio, bfqq->entity.new_weight);
	bfqq->entity.prio_changed = 1;
}

static struct bfq_queue *bfq_get_queue(struct bfq_data *bfqd,
				       struct bio *bio, bool is_sync,
				       struct bfq_io_cq *bic,
				       bool respawn);

static void bfq_check_ioprio_change(struct bfq_io_cq *bic, struct bio *bio)
{
	struct bfq_data *bfqd = bic_to_bfqd(bic);
	struct bfq_queue *bfqq;
	int ioprio = bic->icq.ioc->ioprio;

	 
	if (unlikely(!bfqd) || likely(bic->ioprio == ioprio))
		return;

	bic->ioprio = ioprio;

	bfqq = bic_to_bfqq(bic, false, bfq_actuator_index(bfqd, bio));
	if (bfqq) {
		struct bfq_queue *old_bfqq = bfqq;

		bfqq = bfq_get_queue(bfqd, bio, false, bic, true);
		bic_set_bfqq(bic, bfqq, false, bfq_actuator_index(bfqd, bio));
		bfq_release_process_ref(bfqd, old_bfqq);
	}

	bfqq = bic_to_bfqq(bic, true, bfq_actuator_index(bfqd, bio));
	if (bfqq)
		bfq_set_next_ioprio_data(bfqq, bic);
}

static void bfq_init_bfqq(struct bfq_data *bfqd, struct bfq_queue *bfqq,
			  struct bfq_io_cq *bic, pid_t pid, int is_sync,
			  unsigned int act_idx)
{
	u64 now_ns = ktime_get_ns();

	bfqq->actuator_idx = act_idx;
	RB_CLEAR_NODE(&bfqq->entity.rb_node);
	INIT_LIST_HEAD(&bfqq->fifo);
	INIT_HLIST_NODE(&bfqq->burst_list_node);
	INIT_HLIST_NODE(&bfqq->woken_list_node);
	INIT_HLIST_HEAD(&bfqq->woken_list);

	bfqq->ref = 0;
	bfqq->bfqd = bfqd;

	if (bic)
		bfq_set_next_ioprio_data(bfqq, bic);

	if (is_sync) {
		 
		if (!bfq_class_idle(bfqq))
			 
			bfq_mark_bfqq_has_short_ttime(bfqq);
		bfq_mark_bfqq_sync(bfqq);
		bfq_mark_bfqq_just_created(bfqq);
	} else
		bfq_clear_bfqq_sync(bfqq);

	 
	bfqq->ttime.last_end_request = now_ns + 1;

	bfqq->creation_time = jiffies;

	bfqq->io_start_time = now_ns;

	bfq_mark_bfqq_IO_bound(bfqq);

	bfqq->pid = pid;

	 
	bfqq->max_budget = (2 * bfq_max_budget(bfqd)) / 3;
	bfqq->budget_timeout = bfq_smallest_from_now();

	bfqq->wr_coeff = 1;
	bfqq->last_wr_start_finish = jiffies;
	bfqq->wr_start_at_switch_to_srt = bfq_smallest_from_now();
	bfqq->split_time = bfq_smallest_from_now();

	 
	bfqq->soft_rt_next_start = jiffies;

	 
	bfqq->seek_history = 1;

	bfqq->decrease_time_jif = jiffies;
}

static struct bfq_queue **bfq_async_queue_prio(struct bfq_data *bfqd,
					       struct bfq_group *bfqg,
					       int ioprio_class, int ioprio, int act_idx)
{
	switch (ioprio_class) {
	case IOPRIO_CLASS_RT:
		return &bfqg->async_bfqq[0][ioprio][act_idx];
	case IOPRIO_CLASS_NONE:
		ioprio = IOPRIO_BE_NORM;
		fallthrough;
	case IOPRIO_CLASS_BE:
		return &bfqg->async_bfqq[1][ioprio][act_idx];
	case IOPRIO_CLASS_IDLE:
		return &bfqg->async_idle_bfqq[act_idx];
	default:
		return NULL;
	}
}

static struct bfq_queue *
bfq_do_early_stable_merge(struct bfq_data *bfqd, struct bfq_queue *bfqq,
			  struct bfq_io_cq *bic,
			  struct bfq_queue *last_bfqq_created)
{
	unsigned int a_idx = last_bfqq_created->actuator_idx;
	struct bfq_queue *new_bfqq =
		bfq_setup_merge(bfqq, last_bfqq_created);

	if (!new_bfqq)
		return bfqq;

	if (new_bfqq->bic)
		new_bfqq->bic->bfqq_data[a_idx].stably_merged = true;
	bic->bfqq_data[a_idx].stably_merged = true;

	 
	bfqq->bic = bic;
	bfq_merge_bfqqs(bfqd, bic, bfqq, new_bfqq);

	return new_bfqq;
}

 
static struct bfq_queue *bfq_do_or_sched_stable_merge(struct bfq_data *bfqd,
						      struct bfq_queue *bfqq,
						      struct bfq_io_cq *bic)
{
	struct bfq_queue **source_bfqq = bfqq->entity.parent ?
		&bfqq->entity.parent->last_bfqq_created :
		&bfqd->last_bfqq_created;

	struct bfq_queue *last_bfqq_created = *source_bfqq;

	 
	if (!last_bfqq_created ||
	    time_before(last_bfqq_created->creation_time +
			msecs_to_jiffies(bfq_activation_stable_merging),
			bfqq->creation_time) ||
		bfqq->entity.parent != last_bfqq_created->entity.parent ||
		bfqq->ioprio != last_bfqq_created->ioprio ||
		bfqq->ioprio_class != last_bfqq_created->ioprio_class ||
		bfqq->actuator_idx != last_bfqq_created->actuator_idx)
		*source_bfqq = bfqq;
	else if (time_after_eq(last_bfqq_created->creation_time +
				 bfqd->bfq_burst_interval,
				 bfqq->creation_time)) {
		if (likely(bfqd->nonrot_with_queueing))
			 
			bfqq = bfq_do_early_stable_merge(bfqd, bfqq,
							 bic,
							 last_bfqq_created);
		else {  
			 
			last_bfqq_created->ref++;
			 
			last_bfqq_created->stable_ref++;
			 
			bic->bfqq_data[last_bfqq_created->actuator_idx].stable_merge_bfqq =
				last_bfqq_created;
		}
	}

	return bfqq;
}


static struct bfq_queue *bfq_get_queue(struct bfq_data *bfqd,
				       struct bio *bio, bool is_sync,
				       struct bfq_io_cq *bic,
				       bool respawn)
{
	const int ioprio = IOPRIO_PRIO_LEVEL(bic->ioprio);
	const int ioprio_class = IOPRIO_PRIO_CLASS(bic->ioprio);
	struct bfq_queue **async_bfqq = NULL;
	struct bfq_queue *bfqq;
	struct bfq_group *bfqg;

	bfqg = bfq_bio_bfqg(bfqd, bio);
	if (!is_sync) {
		async_bfqq = bfq_async_queue_prio(bfqd, bfqg, ioprio_class,
						  ioprio,
						  bfq_actuator_index(bfqd, bio));
		bfqq = *async_bfqq;
		if (bfqq)
			goto out;
	}

	bfqq = kmem_cache_alloc_node(bfq_pool,
				     GFP_NOWAIT | __GFP_ZERO | __GFP_NOWARN,
				     bfqd->queue->node);

	if (bfqq) {
		bfq_init_bfqq(bfqd, bfqq, bic, current->pid,
			      is_sync, bfq_actuator_index(bfqd, bio));
		bfq_init_entity(&bfqq->entity, bfqg);
		bfq_log_bfqq(bfqd, bfqq, "allocated");
	} else {
		bfqq = &bfqd->oom_bfqq;
		bfq_log_bfqq(bfqd, bfqq, "using oom bfqq");
		goto out;
	}

	 
	if (async_bfqq) {
		bfqq->ref++;  
		bfq_log_bfqq(bfqd, bfqq, "get_queue, bfqq not in async: %p, %d",
			     bfqq, bfqq->ref);
		*async_bfqq = bfqq;
	}

out:
	bfqq->ref++;  

	if (bfqq != &bfqd->oom_bfqq && is_sync && !respawn)
		bfqq = bfq_do_or_sched_stable_merge(bfqd, bfqq, bic);
	return bfqq;
}

static void bfq_update_io_thinktime(struct bfq_data *bfqd,
				    struct bfq_queue *bfqq)
{
	struct bfq_ttime *ttime = &bfqq->ttime;
	u64 elapsed;

	 
	if (bfqq->dispatched || bfq_bfqq_busy(bfqq))
		return;
	elapsed = ktime_get_ns() - bfqq->ttime.last_end_request;
	elapsed = min_t(u64, elapsed, 2ULL * bfqd->bfq_slice_idle);

	ttime->ttime_samples = (7*ttime->ttime_samples + 256) / 8;
	ttime->ttime_total = div_u64(7*ttime->ttime_total + 256*elapsed,  8);
	ttime->ttime_mean = div64_ul(ttime->ttime_total + 128,
				     ttime->ttime_samples);
}

static void
bfq_update_io_seektime(struct bfq_data *bfqd, struct bfq_queue *bfqq,
		       struct request *rq)
{
	bfqq->seek_history <<= 1;
	bfqq->seek_history |= BFQ_RQ_SEEKY(bfqd, bfqq->last_request_pos, rq);

	if (bfqq->wr_coeff > 1 &&
	    bfqq->wr_cur_max_time == bfqd->bfq_wr_rt_max_time &&
	    BFQQ_TOTALLY_SEEKY(bfqq)) {
		if (time_is_before_jiffies(bfqq->wr_start_at_switch_to_srt +
					   bfq_wr_duration(bfqd))) {
			 
			bfq_bfqq_end_wr(bfqq);
		} else {  
			switch_back_to_interactive_wr(bfqq, bfqd);
			bfqq->entity.prio_changed = 1;
		}
	}
}

static void bfq_update_has_short_ttime(struct bfq_data *bfqd,
				       struct bfq_queue *bfqq,
				       struct bfq_io_cq *bic)
{
	bool has_short_ttime = true, state_changed;

	 
	if (!bfq_bfqq_sync(bfqq) || bfq_class_idle(bfqq) ||
	    bfqd->bfq_slice_idle == 0)
		return;

	 
	if (time_is_after_eq_jiffies(bfqq->split_time +
				     bfqd->bfq_wr_min_idle_time))
		return;

	 
	if (atomic_read(&bic->icq.ioc->active_ref) == 0 ||
	    (bfq_sample_valid(bfqq->ttime.ttime_samples) &&
	     bfqq->ttime.ttime_mean > bfqd->bfq_slice_idle>>1))
		has_short_ttime = false;

	state_changed = has_short_ttime != bfq_bfqq_has_short_ttime(bfqq);

	if (has_short_ttime)
		bfq_mark_bfqq_has_short_ttime(bfqq);
	else
		bfq_clear_bfqq_has_short_ttime(bfqq);

	 
	if (state_changed && bfqq->last_serv_time_ns == 0 &&
	    (time_is_before_eq_jiffies(bfqq->decrease_time_jif +
				      msecs_to_jiffies(100)) ||
	     !has_short_ttime))
		bfq_reset_inject_limit(bfqd, bfqq);
}

 
static void bfq_rq_enqueued(struct bfq_data *bfqd, struct bfq_queue *bfqq,
			    struct request *rq)
{
	if (rq->cmd_flags & REQ_META)
		bfqq->meta_pending++;

	bfqq->last_request_pos = blk_rq_pos(rq) + blk_rq_sectors(rq);

	if (bfqq == bfqd->in_service_queue && bfq_bfqq_wait_request(bfqq)) {
		bool small_req = bfqq->queued[rq_is_sync(rq)] == 1 &&
				 blk_rq_sectors(rq) < 32;
		bool budget_timeout = bfq_bfqq_budget_timeout(bfqq);

		 
		if (small_req && idling_boosts_thr_without_issues(bfqd, bfqq) &&
		    !budget_timeout)
			return;

		 
		bfq_clear_bfqq_wait_request(bfqq);
		hrtimer_try_to_cancel(&bfqd->idle_slice_timer);

		 
		if (budget_timeout)
			bfq_bfqq_expire(bfqd, bfqq, false,
					BFQQE_BUDGET_TIMEOUT);
	}
}

static void bfqq_request_allocated(struct bfq_queue *bfqq)
{
	struct bfq_entity *entity = &bfqq->entity;

	for_each_entity(entity)
		entity->allocated++;
}

static void bfqq_request_freed(struct bfq_queue *bfqq)
{
	struct bfq_entity *entity = &bfqq->entity;

	for_each_entity(entity)
		entity->allocated--;
}

 
static bool __bfq_insert_request(struct bfq_data *bfqd, struct request *rq)
{
	struct bfq_queue *bfqq = RQ_BFQQ(rq),
		*new_bfqq = bfq_setup_cooperator(bfqd, bfqq, rq, true,
						 RQ_BIC(rq));
	bool waiting, idle_timer_disabled = false;

	if (new_bfqq) {
		 
		bfqq_request_allocated(new_bfqq);
		bfqq_request_freed(bfqq);
		new_bfqq->ref++;
		 
		if (bic_to_bfqq(RQ_BIC(rq), true,
				bfq_actuator_index(bfqd, rq->bio)) == bfqq)
			bfq_merge_bfqqs(bfqd, RQ_BIC(rq),
					bfqq, new_bfqq);

		bfq_clear_bfqq_just_created(bfqq);
		 
		bfq_put_queue(bfqq);
		rq->elv.priv[1] = new_bfqq;
		bfqq = new_bfqq;
	}

	bfq_update_io_thinktime(bfqd, bfqq);
	bfq_update_has_short_ttime(bfqd, bfqq, RQ_BIC(rq));
	bfq_update_io_seektime(bfqd, bfqq, rq);

	waiting = bfqq && bfq_bfqq_wait_request(bfqq);
	bfq_add_request(rq);
	idle_timer_disabled = waiting && !bfq_bfqq_wait_request(bfqq);

	rq->fifo_time = ktime_get_ns() + bfqd->bfq_fifo_expire[rq_is_sync(rq)];
	list_add_tail(&rq->queuelist, &bfqq->fifo);

	bfq_rq_enqueued(bfqd, bfqq, rq);

	return idle_timer_disabled;
}

#ifdef CONFIG_BFQ_CGROUP_DEBUG
static void bfq_update_insert_stats(struct request_queue *q,
				    struct bfq_queue *bfqq,
				    bool idle_timer_disabled,
				    blk_opf_t cmd_flags)
{
	if (!bfqq)
		return;

	 
	spin_lock_irq(&q->queue_lock);
	bfqg_stats_update_io_add(bfqq_group(bfqq), bfqq, cmd_flags);
	if (idle_timer_disabled)
		bfqg_stats_update_idle_time(bfqq_group(bfqq));
	spin_unlock_irq(&q->queue_lock);
}
#else
static inline void bfq_update_insert_stats(struct request_queue *q,
					   struct bfq_queue *bfqq,
					   bool idle_timer_disabled,
					   blk_opf_t cmd_flags) {}
#endif  

static struct bfq_queue *bfq_init_rq(struct request *rq);

static void bfq_insert_request(struct blk_mq_hw_ctx *hctx, struct request *rq,
			       blk_insert_t flags)
{
	struct request_queue *q = hctx->queue;
	struct bfq_data *bfqd = q->elevator->elevator_data;
	struct bfq_queue *bfqq;
	bool idle_timer_disabled = false;
	blk_opf_t cmd_flags;
	LIST_HEAD(free);

#ifdef CONFIG_BFQ_GROUP_IOSCHED
	if (!cgroup_subsys_on_dfl(io_cgrp_subsys) && rq->bio)
		bfqg_stats_update_legacy_io(q, rq);
#endif
	spin_lock_irq(&bfqd->lock);
	bfqq = bfq_init_rq(rq);
	if (blk_mq_sched_try_insert_merge(q, rq, &free)) {
		spin_unlock_irq(&bfqd->lock);
		blk_mq_free_requests(&free);
		return;
	}

	trace_block_rq_insert(rq);

	if (flags & BLK_MQ_INSERT_AT_HEAD) {
		list_add(&rq->queuelist, &bfqd->dispatch);
	} else if (!bfqq) {
		list_add_tail(&rq->queuelist, &bfqd->dispatch);
	} else {
		idle_timer_disabled = __bfq_insert_request(bfqd, rq);
		 
		bfqq = RQ_BFQQ(rq);

		if (rq_mergeable(rq)) {
			elv_rqhash_add(q, rq);
			if (!q->last_merge)
				q->last_merge = rq;
		}
	}

	 
	cmd_flags = rq->cmd_flags;
	spin_unlock_irq(&bfqd->lock);

	bfq_update_insert_stats(q, bfqq, idle_timer_disabled,
				cmd_flags);
}

static void bfq_insert_requests(struct blk_mq_hw_ctx *hctx,
				struct list_head *list,
				blk_insert_t flags)
{
	while (!list_empty(list)) {
		struct request *rq;

		rq = list_first_entry(list, struct request, queuelist);
		list_del_init(&rq->queuelist);
		bfq_insert_request(hctx, rq, flags);
	}
}

static void bfq_update_hw_tag(struct bfq_data *bfqd)
{
	struct bfq_queue *bfqq = bfqd->in_service_queue;

	bfqd->max_rq_in_driver = max_t(int, bfqd->max_rq_in_driver,
				       bfqd->tot_rq_in_driver);

	if (bfqd->hw_tag == 1)
		return;

	 
	if (bfqd->tot_rq_in_driver + bfqd->queued <= BFQ_HW_QUEUE_THRESHOLD)
		return;

	 
	if (bfqq && bfq_bfqq_has_short_ttime(bfqq) &&
	    bfqq->dispatched + bfqq->queued[0] + bfqq->queued[1] <
	    BFQ_HW_QUEUE_THRESHOLD &&
	    bfqd->tot_rq_in_driver < BFQ_HW_QUEUE_THRESHOLD)
		return;

	if (bfqd->hw_tag_samples++ < BFQ_HW_QUEUE_SAMPLES)
		return;

	bfqd->hw_tag = bfqd->max_rq_in_driver > BFQ_HW_QUEUE_THRESHOLD;
	bfqd->max_rq_in_driver = 0;
	bfqd->hw_tag_samples = 0;

	bfqd->nonrot_with_queueing =
		blk_queue_nonrot(bfqd->queue) && bfqd->hw_tag;
}

static void bfq_completed_request(struct bfq_queue *bfqq, struct bfq_data *bfqd)
{
	u64 now_ns;
	u32 delta_us;

	bfq_update_hw_tag(bfqd);

	bfqd->rq_in_driver[bfqq->actuator_idx]--;
	bfqd->tot_rq_in_driver--;
	bfqq->dispatched--;

	if (!bfqq->dispatched && !bfq_bfqq_busy(bfqq)) {
		 
		bfqq->budget_timeout = jiffies;

		bfq_del_bfqq_in_groups_with_pending_reqs(bfqq);
		bfq_weights_tree_remove(bfqq);
	}

	now_ns = ktime_get_ns();

	bfqq->ttime.last_end_request = now_ns;

	 
	delta_us = div_u64(now_ns - bfqd->last_completion, NSEC_PER_USEC);

	 
	if (delta_us > BFQ_MIN_TT/NSEC_PER_USEC &&
	   (bfqd->last_rq_max_size<<BFQ_RATE_SHIFT)/delta_us <
			1UL<<(BFQ_RATE_SHIFT - 10))
		bfq_update_rate_reset(bfqd, NULL);
	bfqd->last_completion = now_ns;
	 
	if (!bfq_bfqq_coop(bfqq))
		bfqd->last_completed_rq_bfqq = bfqq;
	else
		bfqd->last_completed_rq_bfqq = NULL;

	 
	if (bfq_bfqq_softrt_update(bfqq) && bfqq->dispatched == 0 &&
	    RB_EMPTY_ROOT(&bfqq->sort_list) &&
	    bfqq->wr_coeff != bfqd->bfq_wr_coeff)
		bfqq->soft_rt_next_start =
			bfq_bfqq_softrt_next_start(bfqd, bfqq);

	 
	if (bfqd->in_service_queue == bfqq) {
		if (bfq_bfqq_must_idle(bfqq)) {
			if (bfqq->dispatched == 0)
				bfq_arm_slice_timer(bfqd);
			 
			return;
		} else if (bfq_may_expire_for_budg_timeout(bfqq))
			bfq_bfqq_expire(bfqd, bfqq, false,
					BFQQE_BUDGET_TIMEOUT);
		else if (RB_EMPTY_ROOT(&bfqq->sort_list) &&
			 (bfqq->dispatched == 0 ||
			  !bfq_better_to_idle(bfqq)))
			bfq_bfqq_expire(bfqd, bfqq, false,
					BFQQE_NO_MORE_REQUESTS);
	}

	if (!bfqd->tot_rq_in_driver)
		bfq_schedule_dispatch(bfqd);
}

 
static void bfq_update_inject_limit(struct bfq_data *bfqd,
				    struct bfq_queue *bfqq)
{
	u64 tot_time_ns = ktime_get_ns() - bfqd->last_empty_occupied_ns;
	unsigned int old_limit = bfqq->inject_limit;

	if (bfqq->last_serv_time_ns > 0 && bfqd->rqs_injected) {
		u64 threshold = (bfqq->last_serv_time_ns * 3)>>1;

		if (tot_time_ns >= threshold && old_limit > 0) {
			bfqq->inject_limit--;
			bfqq->decrease_time_jif = jiffies;
		} else if (tot_time_ns < threshold &&
			   old_limit <= bfqd->max_rq_in_driver)
			bfqq->inject_limit++;
	}

	 
	if ((bfqq->last_serv_time_ns == 0 && bfqd->tot_rq_in_driver == 1) ||
	    tot_time_ns < bfqq->last_serv_time_ns) {
		if (bfqq->last_serv_time_ns == 0) {
			 
			bfqq->inject_limit = max_t(unsigned int, 1, old_limit);
		}
		bfqq->last_serv_time_ns = tot_time_ns;
	} else if (!bfqd->rqs_injected && bfqd->tot_rq_in_driver == 1)
		 
		bfqq->last_serv_time_ns = tot_time_ns;


	 
	bfqd->waited_rq = NULL;
	bfqd->rqs_injected = false;
}

 
static void bfq_finish_requeue_request(struct request *rq)
{
	struct bfq_queue *bfqq = RQ_BFQQ(rq);
	struct bfq_data *bfqd;
	unsigned long flags;

	 
	if (!rq->elv.icq || !bfqq)
		return;

	bfqd = bfqq->bfqd;

	if (rq->rq_flags & RQF_STARTED)
		bfqg_stats_update_completion(bfqq_group(bfqq),
					     rq->start_time_ns,
					     rq->io_start_time_ns,
					     rq->cmd_flags);

	spin_lock_irqsave(&bfqd->lock, flags);
	if (likely(rq->rq_flags & RQF_STARTED)) {
		if (rq == bfqd->waited_rq)
			bfq_update_inject_limit(bfqd, bfqq);

		bfq_completed_request(bfqq, bfqd);
	}
	bfqq_request_freed(bfqq);
	bfq_put_queue(bfqq);
	RQ_BIC(rq)->requests--;
	spin_unlock_irqrestore(&bfqd->lock, flags);

	 
	rq->elv.priv[0] = NULL;
	rq->elv.priv[1] = NULL;
}

static void bfq_finish_request(struct request *rq)
{
	bfq_finish_requeue_request(rq);

	if (rq->elv.icq) {
		put_io_context(rq->elv.icq->ioc);
		rq->elv.icq = NULL;
	}
}

 
static struct bfq_queue *
bfq_split_bfqq(struct bfq_io_cq *bic, struct bfq_queue *bfqq)
{
	bfq_log_bfqq(bfqq->bfqd, bfqq, "splitting queue");

	if (bfqq_process_refs(bfqq) == 1) {
		bfqq->pid = current->pid;
		bfq_clear_bfqq_coop(bfqq);
		bfq_clear_bfqq_split_coop(bfqq);
		return bfqq;
	}

	bic_set_bfqq(bic, NULL, true, bfqq->actuator_idx);

	bfq_put_cooperator(bfqq);

	bfq_release_process_ref(bfqq->bfqd, bfqq);
	return NULL;
}

static struct bfq_queue *bfq_get_bfqq_handle_split(struct bfq_data *bfqd,
						   struct bfq_io_cq *bic,
						   struct bio *bio,
						   bool split, bool is_sync,
						   bool *new_queue)
{
	unsigned int act_idx = bfq_actuator_index(bfqd, bio);
	struct bfq_queue *bfqq = bic_to_bfqq(bic, is_sync, act_idx);
	struct bfq_iocq_bfqq_data *bfqq_data = &bic->bfqq_data[act_idx];

	if (likely(bfqq && bfqq != &bfqd->oom_bfqq))
		return bfqq;

	if (new_queue)
		*new_queue = true;

	if (bfqq)
		bfq_put_queue(bfqq);
	bfqq = bfq_get_queue(bfqd, bio, is_sync, bic, split);

	bic_set_bfqq(bic, bfqq, is_sync, act_idx);
	if (split && is_sync) {
		if ((bfqq_data->was_in_burst_list && bfqd->large_burst) ||
		    bfqq_data->saved_in_large_burst)
			bfq_mark_bfqq_in_large_burst(bfqq);
		else {
			bfq_clear_bfqq_in_large_burst(bfqq);
			if (bfqq_data->was_in_burst_list)
				 
				hlist_add_head(&bfqq->burst_list_node,
					       &bfqd->burst_list);
		}
		bfqq->split_time = jiffies;
	}

	return bfqq;
}

 
static void bfq_prepare_request(struct request *rq)
{
	rq->elv.icq = ioc_find_get_icq(rq->q);

	 
	rq->elv.priv[0] = rq->elv.priv[1] = NULL;
}

 
static struct bfq_queue *bfq_init_rq(struct request *rq)
{
	struct request_queue *q = rq->q;
	struct bio *bio = rq->bio;
	struct bfq_data *bfqd = q->elevator->elevator_data;
	struct bfq_io_cq *bic;
	const int is_sync = rq_is_sync(rq);
	struct bfq_queue *bfqq;
	bool new_queue = false;
	bool bfqq_already_existing = false, split = false;
	unsigned int a_idx = bfq_actuator_index(bfqd, bio);

	if (unlikely(!rq->elv.icq))
		return NULL;

	 
	if (RQ_BFQQ(rq))
		return RQ_BFQQ(rq);

	bic = icq_to_bic(rq->elv.icq);

	bfq_check_ioprio_change(bic, bio);

	bfq_bic_update_cgroup(bic, bio);

	bfqq = bfq_get_bfqq_handle_split(bfqd, bic, bio, false, is_sync,
					 &new_queue);

	if (likely(!new_queue)) {
		 
		if (bfq_bfqq_coop(bfqq) && bfq_bfqq_split_coop(bfqq) &&
			!bic->bfqq_data[a_idx].stably_merged) {
			struct bfq_queue *old_bfqq = bfqq;

			 
			if (bfq_bfqq_in_large_burst(bfqq))
				bic->bfqq_data[a_idx].saved_in_large_burst =
					true;

			bfqq = bfq_split_bfqq(bic, bfqq);
			split = true;

			if (!bfqq) {
				bfqq = bfq_get_bfqq_handle_split(bfqd, bic, bio,
								 true, is_sync,
								 NULL);
				if (unlikely(bfqq == &bfqd->oom_bfqq))
					bfqq_already_existing = true;
			} else
				bfqq_already_existing = true;

			if (!bfqq_already_existing) {
				bfqq->waker_bfqq = old_bfqq->waker_bfqq;
				bfqq->tentative_waker_bfqq = NULL;

				 
				if (bfqq->waker_bfqq)
					hlist_add_head(&bfqq->woken_list_node,
						       &bfqq->waker_bfqq->woken_list);
			}
		}
	}

	bfqq_request_allocated(bfqq);
	bfqq->ref++;
	bic->requests++;
	bfq_log_bfqq(bfqd, bfqq, "get_request %p: bfqq %p, %d",
		     rq, bfqq, bfqq->ref);

	rq->elv.priv[0] = bic;
	rq->elv.priv[1] = bfqq;

	 
	if (likely(bfqq != &bfqd->oom_bfqq) && bfqq_process_refs(bfqq) == 1) {
		bfqq->bic = bic;
		if (split) {
			 
			bfq_bfqq_resume_state(bfqq, bfqd, bic,
					      bfqq_already_existing);
		}
	}

	 
	if (unlikely(bfq_bfqq_just_created(bfqq) &&
		     (bfqd->burst_size > 0 ||
		      bfq_tot_busy_queues(bfqd) == 0)))
		bfq_handle_burst(bfqd, bfqq);

	return bfqq;
}

static void
bfq_idle_slice_timer_body(struct bfq_data *bfqd, struct bfq_queue *bfqq)
{
	enum bfqq_expiration reason;
	unsigned long flags;

	spin_lock_irqsave(&bfqd->lock, flags);

	 
	if (bfqq != bfqd->in_service_queue) {
		spin_unlock_irqrestore(&bfqd->lock, flags);
		return;
	}

	bfq_clear_bfqq_wait_request(bfqq);

	if (bfq_bfqq_budget_timeout(bfqq))
		 
		reason = BFQQE_BUDGET_TIMEOUT;
	else if (bfqq->queued[0] == 0 && bfqq->queued[1] == 0)
		 
		reason = BFQQE_TOO_IDLE;
	else
		goto schedule_dispatch;

	bfq_bfqq_expire(bfqd, bfqq, true, reason);

schedule_dispatch:
	bfq_schedule_dispatch(bfqd);
	spin_unlock_irqrestore(&bfqd->lock, flags);
}

 
static enum hrtimer_restart bfq_idle_slice_timer(struct hrtimer *timer)
{
	struct bfq_data *bfqd = container_of(timer, struct bfq_data,
					     idle_slice_timer);
	struct bfq_queue *bfqq = bfqd->in_service_queue;

	 
	if (bfqq)
		bfq_idle_slice_timer_body(bfqd, bfqq);

	return HRTIMER_NORESTART;
}

static void __bfq_put_async_bfqq(struct bfq_data *bfqd,
				 struct bfq_queue **bfqq_ptr)
{
	struct bfq_queue *bfqq = *bfqq_ptr;

	bfq_log(bfqd, "put_async_bfqq: %p", bfqq);
	if (bfqq) {
		bfq_bfqq_move(bfqd, bfqq, bfqd->root_group);

		bfq_log_bfqq(bfqd, bfqq, "put_async_bfqq: putting %p, %d",
			     bfqq, bfqq->ref);
		bfq_put_queue(bfqq);
		*bfqq_ptr = NULL;
	}
}

 
void bfq_put_async_queues(struct bfq_data *bfqd, struct bfq_group *bfqg)
{
	int i, j, k;

	for (k = 0; k < bfqd->num_actuators; k++) {
		for (i = 0; i < 2; i++)
			for (j = 0; j < IOPRIO_NR_LEVELS; j++)
				__bfq_put_async_bfqq(bfqd, &bfqg->async_bfqq[i][j][k]);

		__bfq_put_async_bfqq(bfqd, &bfqg->async_idle_bfqq[k]);
	}
}

 
static void bfq_update_depths(struct bfq_data *bfqd, struct sbitmap_queue *bt)
{
	unsigned int depth = 1U << bt->sb.shift;

	bfqd->full_depth_shift = bt->sb.shift;
	 
	 
	bfqd->word_depths[0][0] = max(depth >> 1, 1U);
	 
	bfqd->word_depths[0][1] = max((depth * 3) >> 2, 1U);

	 
	 
	bfqd->word_depths[1][0] = max((depth * 3) >> 4, 1U);
	 
	bfqd->word_depths[1][1] = max((depth * 6) >> 4, 1U);
}

static void bfq_depth_updated(struct blk_mq_hw_ctx *hctx)
{
	struct bfq_data *bfqd = hctx->queue->elevator->elevator_data;
	struct blk_mq_tags *tags = hctx->sched_tags;

	bfq_update_depths(bfqd, &tags->bitmap_tags);
	sbitmap_queue_min_shallow_depth(&tags->bitmap_tags, 1);
}

static int bfq_init_hctx(struct blk_mq_hw_ctx *hctx, unsigned int index)
{
	bfq_depth_updated(hctx);
	return 0;
}

static void bfq_exit_queue(struct elevator_queue *e)
{
	struct bfq_data *bfqd = e->elevator_data;
	struct bfq_queue *bfqq, *n;
	unsigned int actuator;

	hrtimer_cancel(&bfqd->idle_slice_timer);

	spin_lock_irq(&bfqd->lock);
	list_for_each_entry_safe(bfqq, n, &bfqd->idle_list, bfqq_list)
		bfq_deactivate_bfqq(bfqd, bfqq, false, false);
	spin_unlock_irq(&bfqd->lock);

	for (actuator = 0; actuator < bfqd->num_actuators; actuator++)
		WARN_ON_ONCE(bfqd->rq_in_driver[actuator]);
	WARN_ON_ONCE(bfqd->tot_rq_in_driver);

	hrtimer_cancel(&bfqd->idle_slice_timer);

	 
	bfqg_and_blkg_put(bfqd->root_group);

#ifdef CONFIG_BFQ_GROUP_IOSCHED
	blkcg_deactivate_policy(bfqd->queue->disk, &blkcg_policy_bfq);
#else
	spin_lock_irq(&bfqd->lock);
	bfq_put_async_queues(bfqd, bfqd->root_group);
	kfree(bfqd->root_group);
	spin_unlock_irq(&bfqd->lock);
#endif

	blk_stat_disable_accounting(bfqd->queue);
	clear_bit(ELEVATOR_FLAG_DISABLE_WBT, &e->flags);
	wbt_enable_default(bfqd->queue->disk);

	kfree(bfqd);
}

static void bfq_init_root_group(struct bfq_group *root_group,
				struct bfq_data *bfqd)
{
	int i;

#ifdef CONFIG_BFQ_GROUP_IOSCHED
	root_group->entity.parent = NULL;
	root_group->my_entity = NULL;
	root_group->bfqd = bfqd;
#endif
	root_group->rq_pos_tree = RB_ROOT;
	for (i = 0; i < BFQ_IOPRIO_CLASSES; i++)
		root_group->sched_data.service_tree[i] = BFQ_SERVICE_TREE_INIT;
	root_group->sched_data.bfq_class_idle_last_service = jiffies;
}

static int bfq_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct bfq_data *bfqd;
	struct elevator_queue *eq;
	unsigned int i;
	struct blk_independent_access_ranges *ia_ranges = q->disk->ia_ranges;

	eq = elevator_alloc(q, e);
	if (!eq)
		return -ENOMEM;

	bfqd = kzalloc_node(sizeof(*bfqd), GFP_KERNEL, q->node);
	if (!bfqd) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}
	eq->elevator_data = bfqd;

	spin_lock_irq(&q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(&q->queue_lock);

	 
	bfq_init_bfqq(bfqd, &bfqd->oom_bfqq, NULL, 1, 0, 0);
	bfqd->oom_bfqq.ref++;
	bfqd->oom_bfqq.new_ioprio = BFQ_DEFAULT_QUEUE_IOPRIO;
	bfqd->oom_bfqq.new_ioprio_class = IOPRIO_CLASS_BE;
	bfqd->oom_bfqq.entity.new_weight =
		bfq_ioprio_to_weight(bfqd->oom_bfqq.new_ioprio);

	 
	bfq_clear_bfqq_just_created(&bfqd->oom_bfqq);

	 
	bfqd->oom_bfqq.entity.prio_changed = 1;

	bfqd->queue = q;

	bfqd->num_actuators = 1;
	 
	spin_lock_irq(&q->queue_lock);
	if (ia_ranges) {
		 
		if (ia_ranges->nr_ia_ranges > BFQ_MAX_ACTUATORS) {
			pr_crit("nr_ia_ranges higher than act limit: iars=%d, max=%d.\n",
				ia_ranges->nr_ia_ranges, BFQ_MAX_ACTUATORS);
			pr_crit("Falling back to single actuator mode.\n");
		} else {
			bfqd->num_actuators = ia_ranges->nr_ia_ranges;

			for (i = 0; i < bfqd->num_actuators; i++) {
				bfqd->sector[i] = ia_ranges->ia_range[i].sector;
				bfqd->nr_sectors[i] =
					ia_ranges->ia_range[i].nr_sectors;
			}
		}
	}

	 
	if (bfqd->num_actuators == 1) {
		bfqd->sector[0] = 0;
		bfqd->nr_sectors[0] = get_capacity(q->disk);
	}
	spin_unlock_irq(&q->queue_lock);

	INIT_LIST_HEAD(&bfqd->dispatch);

	hrtimer_init(&bfqd->idle_slice_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	bfqd->idle_slice_timer.function = bfq_idle_slice_timer;

	bfqd->queue_weights_tree = RB_ROOT_CACHED;
#ifdef CONFIG_BFQ_GROUP_IOSCHED
	bfqd->num_groups_with_pending_reqs = 0;
#endif

	INIT_LIST_HEAD(&bfqd->active_list[0]);
	INIT_LIST_HEAD(&bfqd->active_list[1]);
	INIT_LIST_HEAD(&bfqd->idle_list);
	INIT_HLIST_HEAD(&bfqd->burst_list);

	bfqd->hw_tag = -1;
	bfqd->nonrot_with_queueing = blk_queue_nonrot(bfqd->queue);

	bfqd->bfq_max_budget = bfq_default_max_budget;

	bfqd->bfq_fifo_expire[0] = bfq_fifo_expire[0];
	bfqd->bfq_fifo_expire[1] = bfq_fifo_expire[1];
	bfqd->bfq_back_max = bfq_back_max;
	bfqd->bfq_back_penalty = bfq_back_penalty;
	bfqd->bfq_slice_idle = bfq_slice_idle;
	bfqd->bfq_timeout = bfq_timeout;

	bfqd->bfq_large_burst_thresh = 8;
	bfqd->bfq_burst_interval = msecs_to_jiffies(180);

	bfqd->low_latency = true;

	 
	bfqd->bfq_wr_coeff = 30;
	bfqd->bfq_wr_rt_max_time = msecs_to_jiffies(300);
	bfqd->bfq_wr_min_idle_time = msecs_to_jiffies(2000);
	bfqd->bfq_wr_min_inter_arr_async = msecs_to_jiffies(500);
	bfqd->bfq_wr_max_softrt_rate = 7000;  
	bfqd->wr_busy_queues = 0;

	 
	bfqd->rate_dur_prod = ref_rate[blk_queue_nonrot(bfqd->queue)] *
		ref_wr_duration[blk_queue_nonrot(bfqd->queue)];
	bfqd->peak_rate = ref_rate[blk_queue_nonrot(bfqd->queue)] * 2 / 3;

	 
	bfqd->actuator_load_threshold = 4;

	spin_lock_init(&bfqd->lock);

	 
	bfqd->root_group = bfq_create_group_hierarchy(bfqd, q->node);
	if (!bfqd->root_group)
		goto out_free;
	bfq_init_root_group(bfqd->root_group, bfqd);
	bfq_init_entity(&bfqd->oom_bfqq.entity, bfqd->root_group);

	 
	blk_queue_flag_set(QUEUE_FLAG_SQ_SCHED, q);

	set_bit(ELEVATOR_FLAG_DISABLE_WBT, &eq->flags);
	wbt_disable_default(q->disk);
	blk_stat_enable_accounting(q);

	return 0;

out_free:
	kfree(bfqd);
	kobject_put(&eq->kobj);
	return -ENOMEM;
}

static void bfq_slab_kill(void)
{
	kmem_cache_destroy(bfq_pool);
}

static int __init bfq_slab_setup(void)
{
	bfq_pool = KMEM_CACHE(bfq_queue, 0);
	if (!bfq_pool)
		return -ENOMEM;
	return 0;
}

static ssize_t bfq_var_show(unsigned int var, char *page)
{
	return sprintf(page, "%u\n", var);
}

static int bfq_var_store(unsigned long *var, const char *page)
{
	unsigned long new_val;
	int ret = kstrtoul(page, 10, &new_val);

	if (ret)
		return ret;
	*var = new_val;
	return 0;
}

#define SHOW_FUNCTION(__FUNC, __VAR, __CONV)				\
static ssize_t __FUNC(struct elevator_queue *e, char *page)		\
{									\
	struct bfq_data *bfqd = e->elevator_data;			\
	u64 __data = __VAR;						\
	if (__CONV == 1)						\
		__data = jiffies_to_msecs(__data);			\
	else if (__CONV == 2)						\
		__data = div_u64(__data, NSEC_PER_MSEC);		\
	return bfq_var_show(__data, (page));				\
}
SHOW_FUNCTION(bfq_fifo_expire_sync_show, bfqd->bfq_fifo_expire[1], 2);
SHOW_FUNCTION(bfq_fifo_expire_async_show, bfqd->bfq_fifo_expire[0], 2);
SHOW_FUNCTION(bfq_back_seek_max_show, bfqd->bfq_back_max, 0);
SHOW_FUNCTION(bfq_back_seek_penalty_show, bfqd->bfq_back_penalty, 0);
SHOW_FUNCTION(bfq_slice_idle_show, bfqd->bfq_slice_idle, 2);
SHOW_FUNCTION(bfq_max_budget_show, bfqd->bfq_user_max_budget, 0);
SHOW_FUNCTION(bfq_timeout_sync_show, bfqd->bfq_timeout, 1);
SHOW_FUNCTION(bfq_strict_guarantees_show, bfqd->strict_guarantees, 0);
SHOW_FUNCTION(bfq_low_latency_show, bfqd->low_latency, 0);
#undef SHOW_FUNCTION

#define USEC_SHOW_FUNCTION(__FUNC, __VAR)				\
static ssize_t __FUNC(struct elevator_queue *e, char *page)		\
{									\
	struct bfq_data *bfqd = e->elevator_data;			\
	u64 __data = __VAR;						\
	__data = div_u64(__data, NSEC_PER_USEC);			\
	return bfq_var_show(__data, (page));				\
}
USEC_SHOW_FUNCTION(bfq_slice_idle_us_show, bfqd->bfq_slice_idle);
#undef USEC_SHOW_FUNCTION

#define STORE_FUNCTION(__FUNC, __PTR, MIN, MAX, __CONV)			\
static ssize_t								\
__FUNC(struct elevator_queue *e, const char *page, size_t count)	\
{									\
	struct bfq_data *bfqd = e->elevator_data;			\
	unsigned long __data, __min = (MIN), __max = (MAX);		\
	int ret;							\
									\
	ret = bfq_var_store(&__data, (page));				\
	if (ret)							\
		return ret;						\
	if (__data < __min)						\
		__data = __min;						\
	else if (__data > __max)					\
		__data = __max;						\
	if (__CONV == 1)						\
		*(__PTR) = msecs_to_jiffies(__data);			\
	else if (__CONV == 2)						\
		*(__PTR) = (u64)__data * NSEC_PER_MSEC;			\
	else								\
		*(__PTR) = __data;					\
	return count;							\
}
STORE_FUNCTION(bfq_fifo_expire_sync_store, &bfqd->bfq_fifo_expire[1], 1,
		INT_MAX, 2);
STORE_FUNCTION(bfq_fifo_expire_async_store, &bfqd->bfq_fifo_expire[0], 1,
		INT_MAX, 2);
STORE_FUNCTION(bfq_back_seek_max_store, &bfqd->bfq_back_max, 0, INT_MAX, 0);
STORE_FUNCTION(bfq_back_seek_penalty_store, &bfqd->bfq_back_penalty, 1,
		INT_MAX, 0);
STORE_FUNCTION(bfq_slice_idle_store, &bfqd->bfq_slice_idle, 0, INT_MAX, 2);
#undef STORE_FUNCTION

#define USEC_STORE_FUNCTION(__FUNC, __PTR, MIN, MAX)			\
static ssize_t __FUNC(struct elevator_queue *e, const char *page, size_t count)\
{									\
	struct bfq_data *bfqd = e->elevator_data;			\
	unsigned long __data, __min = (MIN), __max = (MAX);		\
	int ret;							\
									\
	ret = bfq_var_store(&__data, (page));				\
	if (ret)							\
		return ret;						\
	if (__data < __min)						\
		__data = __min;						\
	else if (__data > __max)					\
		__data = __max;						\
	*(__PTR) = (u64)__data * NSEC_PER_USEC;				\
	return count;							\
}
USEC_STORE_FUNCTION(bfq_slice_idle_us_store, &bfqd->bfq_slice_idle, 0,
		    UINT_MAX);
#undef USEC_STORE_FUNCTION

static ssize_t bfq_max_budget_store(struct elevator_queue *e,
				    const char *page, size_t count)
{
	struct bfq_data *bfqd = e->elevator_data;
	unsigned long __data;
	int ret;

	ret = bfq_var_store(&__data, (page));
	if (ret)
		return ret;

	if (__data == 0)
		bfqd->bfq_max_budget = bfq_calc_max_budget(bfqd);
	else {
		if (__data > INT_MAX)
			__data = INT_MAX;
		bfqd->bfq_max_budget = __data;
	}

	bfqd->bfq_user_max_budget = __data;

	return count;
}

 
static ssize_t bfq_timeout_sync_store(struct elevator_queue *e,
				      const char *page, size_t count)
{
	struct bfq_data *bfqd = e->elevator_data;
	unsigned long __data;
	int ret;

	ret = bfq_var_store(&__data, (page));
	if (ret)
		return ret;

	if (__data < 1)
		__data = 1;
	else if (__data > INT_MAX)
		__data = INT_MAX;

	bfqd->bfq_timeout = msecs_to_jiffies(__data);
	if (bfqd->bfq_user_max_budget == 0)
		bfqd->bfq_max_budget = bfq_calc_max_budget(bfqd);

	return count;
}

static ssize_t bfq_strict_guarantees_store(struct elevator_queue *e,
				     const char *page, size_t count)
{
	struct bfq_data *bfqd = e->elevator_data;
	unsigned long __data;
	int ret;

	ret = bfq_var_store(&__data, (page));
	if (ret)
		return ret;

	if (__data > 1)
		__data = 1;
	if (!bfqd->strict_guarantees && __data == 1
	    && bfqd->bfq_slice_idle < 8 * NSEC_PER_MSEC)
		bfqd->bfq_slice_idle = 8 * NSEC_PER_MSEC;

	bfqd->strict_guarantees = __data;

	return count;
}

static ssize_t bfq_low_latency_store(struct elevator_queue *e,
				     const char *page, size_t count)
{
	struct bfq_data *bfqd = e->elevator_data;
	unsigned long __data;
	int ret;

	ret = bfq_var_store(&__data, (page));
	if (ret)
		return ret;

	if (__data > 1)
		__data = 1;
	if (__data == 0 && bfqd->low_latency != 0)
		bfq_end_wr(bfqd);
	bfqd->low_latency = __data;

	return count;
}

#define BFQ_ATTR(name) \
	__ATTR(name, 0644, bfq_##name##_show, bfq_##name##_store)

static struct elv_fs_entry bfq_attrs[] = {
	BFQ_ATTR(fifo_expire_sync),
	BFQ_ATTR(fifo_expire_async),
	BFQ_ATTR(back_seek_max),
	BFQ_ATTR(back_seek_penalty),
	BFQ_ATTR(slice_idle),
	BFQ_ATTR(slice_idle_us),
	BFQ_ATTR(max_budget),
	BFQ_ATTR(timeout_sync),
	BFQ_ATTR(strict_guarantees),
	BFQ_ATTR(low_latency),
	__ATTR_NULL
};

static struct elevator_type iosched_bfq_mq = {
	.ops = {
		.limit_depth		= bfq_limit_depth,
		.prepare_request	= bfq_prepare_request,
		.requeue_request        = bfq_finish_requeue_request,
		.finish_request		= bfq_finish_request,
		.exit_icq		= bfq_exit_icq,
		.insert_requests	= bfq_insert_requests,
		.dispatch_request	= bfq_dispatch_request,
		.next_request		= elv_rb_latter_request,
		.former_request		= elv_rb_former_request,
		.allow_merge		= bfq_allow_bio_merge,
		.bio_merge		= bfq_bio_merge,
		.request_merge		= bfq_request_merge,
		.requests_merged	= bfq_requests_merged,
		.request_merged		= bfq_request_merged,
		.has_work		= bfq_has_work,
		.depth_updated		= bfq_depth_updated,
		.init_hctx		= bfq_init_hctx,
		.init_sched		= bfq_init_queue,
		.exit_sched		= bfq_exit_queue,
	},

	.icq_size =		sizeof(struct bfq_io_cq),
	.icq_align =		__alignof__(struct bfq_io_cq),
	.elevator_attrs =	bfq_attrs,
	.elevator_name =	"bfq",
	.elevator_owner =	THIS_MODULE,
};
MODULE_ALIAS("bfq-iosched");

static int __init bfq_init(void)
{
	int ret;

#ifdef CONFIG_BFQ_GROUP_IOSCHED
	ret = blkcg_policy_register(&blkcg_policy_bfq);
	if (ret)
		return ret;
#endif

	ret = -ENOMEM;
	if (bfq_slab_setup())
		goto err_pol_unreg;

	 
	ref_wr_duration[0] = msecs_to_jiffies(7000);  
	ref_wr_duration[1] = msecs_to_jiffies(2500);  

	ret = elv_register(&iosched_bfq_mq);
	if (ret)
		goto slab_kill;

	return 0;

slab_kill:
	bfq_slab_kill();
err_pol_unreg:
#ifdef CONFIG_BFQ_GROUP_IOSCHED
	blkcg_policy_unregister(&blkcg_policy_bfq);
#endif
	return ret;
}

static void __exit bfq_exit(void)
{
	elv_unregister(&iosched_bfq_mq);
#ifdef CONFIG_BFQ_GROUP_IOSCHED
	blkcg_policy_unregister(&blkcg_policy_bfq);
#endif
	bfq_slab_kill();
}

module_init(bfq_init);
module_exit(bfq_exit);

MODULE_AUTHOR("Paolo Valente");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MQ Budget Fair Queueing I/O Scheduler");
