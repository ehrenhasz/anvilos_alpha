
 
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/cgroup.h>
#include <linux/ktime.h>
#include <linux/rbtree.h>
#include <linux/ioprio.h>
#include <linux/sbitmap.h>
#include <linux/delay.h>

#include "elevator.h"
#include "bfq-iosched.h"

#ifdef CONFIG_BFQ_CGROUP_DEBUG
static int bfq_stat_init(struct bfq_stat *stat, gfp_t gfp)
{
	int ret;

	ret = percpu_counter_init(&stat->cpu_cnt, 0, gfp);
	if (ret)
		return ret;

	atomic64_set(&stat->aux_cnt, 0);
	return 0;
}

static void bfq_stat_exit(struct bfq_stat *stat)
{
	percpu_counter_destroy(&stat->cpu_cnt);
}

 
static inline void bfq_stat_add(struct bfq_stat *stat, uint64_t val)
{
	percpu_counter_add_batch(&stat->cpu_cnt, val, BLKG_STAT_CPU_BATCH);
}

 
static inline uint64_t bfq_stat_read(struct bfq_stat *stat)
{
	return percpu_counter_sum_positive(&stat->cpu_cnt);
}

 
static inline void bfq_stat_reset(struct bfq_stat *stat)
{
	percpu_counter_set(&stat->cpu_cnt, 0);
	atomic64_set(&stat->aux_cnt, 0);
}

 
static inline void bfq_stat_add_aux(struct bfq_stat *to,
				     struct bfq_stat *from)
{
	atomic64_add(bfq_stat_read(from) + atomic64_read(&from->aux_cnt),
		     &to->aux_cnt);
}

 
static u64 blkg_prfill_stat(struct seq_file *sf, struct blkg_policy_data *pd,
		int off)
{
	return __blkg_prfill_u64(sf, pd, bfq_stat_read((void *)pd + off));
}

 
enum bfqg_stats_flags {
	BFQG_stats_waiting = 0,
	BFQG_stats_idling,
	BFQG_stats_empty,
};

#define BFQG_FLAG_FNS(name)						\
static void bfqg_stats_mark_##name(struct bfqg_stats *stats)	\
{									\
	stats->flags |= (1 << BFQG_stats_##name);			\
}									\
static void bfqg_stats_clear_##name(struct bfqg_stats *stats)	\
{									\
	stats->flags &= ~(1 << BFQG_stats_##name);			\
}									\
static int bfqg_stats_##name(struct bfqg_stats *stats)		\
{									\
	return (stats->flags & (1 << BFQG_stats_##name)) != 0;		\
}									\

BFQG_FLAG_FNS(waiting)
BFQG_FLAG_FNS(idling)
BFQG_FLAG_FNS(empty)
#undef BFQG_FLAG_FNS

 
static void bfqg_stats_update_group_wait_time(struct bfqg_stats *stats)
{
	u64 now;

	if (!bfqg_stats_waiting(stats))
		return;

	now = ktime_get_ns();
	if (now > stats->start_group_wait_time)
		bfq_stat_add(&stats->group_wait_time,
			      now - stats->start_group_wait_time);
	bfqg_stats_clear_waiting(stats);
}

 
static void bfqg_stats_set_start_group_wait_time(struct bfq_group *bfqg,
						 struct bfq_group *curr_bfqg)
{
	struct bfqg_stats *stats = &bfqg->stats;

	if (bfqg_stats_waiting(stats))
		return;
	if (bfqg == curr_bfqg)
		return;
	stats->start_group_wait_time = ktime_get_ns();
	bfqg_stats_mark_waiting(stats);
}

 
static void bfqg_stats_end_empty_time(struct bfqg_stats *stats)
{
	u64 now;

	if (!bfqg_stats_empty(stats))
		return;

	now = ktime_get_ns();
	if (now > stats->start_empty_time)
		bfq_stat_add(&stats->empty_time,
			      now - stats->start_empty_time);
	bfqg_stats_clear_empty(stats);
}

void bfqg_stats_update_dequeue(struct bfq_group *bfqg)
{
	bfq_stat_add(&bfqg->stats.dequeue, 1);
}

void bfqg_stats_set_start_empty_time(struct bfq_group *bfqg)
{
	struct bfqg_stats *stats = &bfqg->stats;

	if (blkg_rwstat_total(&stats->queued))
		return;

	 
	if (bfqg_stats_empty(stats))
		return;

	stats->start_empty_time = ktime_get_ns();
	bfqg_stats_mark_empty(stats);
}

void bfqg_stats_update_idle_time(struct bfq_group *bfqg)
{
	struct bfqg_stats *stats = &bfqg->stats;

	if (bfqg_stats_idling(stats)) {
		u64 now = ktime_get_ns();

		if (now > stats->start_idle_time)
			bfq_stat_add(&stats->idle_time,
				      now - stats->start_idle_time);
		bfqg_stats_clear_idling(stats);
	}
}

void bfqg_stats_set_start_idle_time(struct bfq_group *bfqg)
{
	struct bfqg_stats *stats = &bfqg->stats;

	stats->start_idle_time = ktime_get_ns();
	bfqg_stats_mark_idling(stats);
}

void bfqg_stats_update_avg_queue_size(struct bfq_group *bfqg)
{
	struct bfqg_stats *stats = &bfqg->stats;

	bfq_stat_add(&stats->avg_queue_size_sum,
		      blkg_rwstat_total(&stats->queued));
	bfq_stat_add(&stats->avg_queue_size_samples, 1);
	bfqg_stats_update_group_wait_time(stats);
}

void bfqg_stats_update_io_add(struct bfq_group *bfqg, struct bfq_queue *bfqq,
			      blk_opf_t opf)
{
	blkg_rwstat_add(&bfqg->stats.queued, opf, 1);
	bfqg_stats_end_empty_time(&bfqg->stats);
	if (!(bfqq == bfqg->bfqd->in_service_queue))
		bfqg_stats_set_start_group_wait_time(bfqg, bfqq_group(bfqq));
}

void bfqg_stats_update_io_remove(struct bfq_group *bfqg, blk_opf_t opf)
{
	blkg_rwstat_add(&bfqg->stats.queued, opf, -1);
}

void bfqg_stats_update_io_merged(struct bfq_group *bfqg, blk_opf_t opf)
{
	blkg_rwstat_add(&bfqg->stats.merged, opf, 1);
}

void bfqg_stats_update_completion(struct bfq_group *bfqg, u64 start_time_ns,
				  u64 io_start_time_ns, blk_opf_t opf)
{
	struct bfqg_stats *stats = &bfqg->stats;
	u64 now = ktime_get_ns();

	if (now > io_start_time_ns)
		blkg_rwstat_add(&stats->service_time, opf,
				now - io_start_time_ns);
	if (io_start_time_ns > start_time_ns)
		blkg_rwstat_add(&stats->wait_time, opf,
				io_start_time_ns - start_time_ns);
}

#else  

void bfqg_stats_update_io_remove(struct bfq_group *bfqg, blk_opf_t opf) { }
void bfqg_stats_update_io_merged(struct bfq_group *bfqg, blk_opf_t opf) { }
void bfqg_stats_update_completion(struct bfq_group *bfqg, u64 start_time_ns,
				  u64 io_start_time_ns, blk_opf_t opf) { }
void bfqg_stats_update_dequeue(struct bfq_group *bfqg) { }
void bfqg_stats_set_start_idle_time(struct bfq_group *bfqg) { }

#endif  

#ifdef CONFIG_BFQ_GROUP_IOSCHED

 

static struct bfq_group *pd_to_bfqg(struct blkg_policy_data *pd)
{
	return pd ? container_of(pd, struct bfq_group, pd) : NULL;
}

struct blkcg_gq *bfqg_to_blkg(struct bfq_group *bfqg)
{
	return pd_to_blkg(&bfqg->pd);
}

static struct bfq_group *blkg_to_bfqg(struct blkcg_gq *blkg)
{
	return pd_to_bfqg(blkg_to_pd(blkg, &blkcg_policy_bfq));
}

 

static struct bfq_group *bfqg_parent(struct bfq_group *bfqg)
{
	struct blkcg_gq *pblkg = bfqg_to_blkg(bfqg)->parent;

	return pblkg ? blkg_to_bfqg(pblkg) : NULL;
}

struct bfq_group *bfqq_group(struct bfq_queue *bfqq)
{
	struct bfq_entity *group_entity = bfqq->entity.parent;

	return group_entity ? container_of(group_entity, struct bfq_group,
					   entity) :
			      bfqq->bfqd->root_group;
}

 

static void bfqg_get(struct bfq_group *bfqg)
{
	refcount_inc(&bfqg->ref);
}

static void bfqg_put(struct bfq_group *bfqg)
{
	if (refcount_dec_and_test(&bfqg->ref))
		kfree(bfqg);
}

static void bfqg_and_blkg_get(struct bfq_group *bfqg)
{
	 
	bfqg_get(bfqg);

	blkg_get(bfqg_to_blkg(bfqg));
}

void bfqg_and_blkg_put(struct bfq_group *bfqg)
{
	blkg_put(bfqg_to_blkg(bfqg));

	bfqg_put(bfqg);
}

void bfqg_stats_update_legacy_io(struct request_queue *q, struct request *rq)
{
	struct bfq_group *bfqg = blkg_to_bfqg(rq->bio->bi_blkg);

	if (!bfqg)
		return;

	blkg_rwstat_add(&bfqg->stats.bytes, rq->cmd_flags, blk_rq_bytes(rq));
	blkg_rwstat_add(&bfqg->stats.ios, rq->cmd_flags, 1);
}

 
static void bfqg_stats_reset(struct bfqg_stats *stats)
{
#ifdef CONFIG_BFQ_CGROUP_DEBUG
	 
	blkg_rwstat_reset(&stats->merged);
	blkg_rwstat_reset(&stats->service_time);
	blkg_rwstat_reset(&stats->wait_time);
	bfq_stat_reset(&stats->time);
	bfq_stat_reset(&stats->avg_queue_size_sum);
	bfq_stat_reset(&stats->avg_queue_size_samples);
	bfq_stat_reset(&stats->dequeue);
	bfq_stat_reset(&stats->group_wait_time);
	bfq_stat_reset(&stats->idle_time);
	bfq_stat_reset(&stats->empty_time);
#endif
}

 
static void bfqg_stats_add_aux(struct bfqg_stats *to, struct bfqg_stats *from)
{
	if (!to || !from)
		return;

#ifdef CONFIG_BFQ_CGROUP_DEBUG
	 
	blkg_rwstat_add_aux(&to->merged, &from->merged);
	blkg_rwstat_add_aux(&to->service_time, &from->service_time);
	blkg_rwstat_add_aux(&to->wait_time, &from->wait_time);
	bfq_stat_add_aux(&from->time, &from->time);
	bfq_stat_add_aux(&to->avg_queue_size_sum, &from->avg_queue_size_sum);
	bfq_stat_add_aux(&to->avg_queue_size_samples,
			  &from->avg_queue_size_samples);
	bfq_stat_add_aux(&to->dequeue, &from->dequeue);
	bfq_stat_add_aux(&to->group_wait_time, &from->group_wait_time);
	bfq_stat_add_aux(&to->idle_time, &from->idle_time);
	bfq_stat_add_aux(&to->empty_time, &from->empty_time);
#endif
}

 
static void bfqg_stats_xfer_dead(struct bfq_group *bfqg)
{
	struct bfq_group *parent;

	if (!bfqg)  
		return;

	parent = bfqg_parent(bfqg);

	lockdep_assert_held(&bfqg_to_blkg(bfqg)->q->queue_lock);

	if (unlikely(!parent))
		return;

	bfqg_stats_add_aux(&parent->stats, &bfqg->stats);
	bfqg_stats_reset(&bfqg->stats);
}

void bfq_init_entity(struct bfq_entity *entity, struct bfq_group *bfqg)
{
	struct bfq_queue *bfqq = bfq_entity_to_bfqq(entity);

	entity->weight = entity->new_weight;
	entity->orig_weight = entity->new_weight;
	if (bfqq) {
		bfqq->ioprio = bfqq->new_ioprio;
		bfqq->ioprio_class = bfqq->new_ioprio_class;
		 
		bfqg_and_blkg_get(bfqg);
	}
	entity->parent = bfqg->my_entity;  
	entity->sched_data = &bfqg->sched_data;
}

static void bfqg_stats_exit(struct bfqg_stats *stats)
{
	blkg_rwstat_exit(&stats->bytes);
	blkg_rwstat_exit(&stats->ios);
#ifdef CONFIG_BFQ_CGROUP_DEBUG
	blkg_rwstat_exit(&stats->merged);
	blkg_rwstat_exit(&stats->service_time);
	blkg_rwstat_exit(&stats->wait_time);
	blkg_rwstat_exit(&stats->queued);
	bfq_stat_exit(&stats->time);
	bfq_stat_exit(&stats->avg_queue_size_sum);
	bfq_stat_exit(&stats->avg_queue_size_samples);
	bfq_stat_exit(&stats->dequeue);
	bfq_stat_exit(&stats->group_wait_time);
	bfq_stat_exit(&stats->idle_time);
	bfq_stat_exit(&stats->empty_time);
#endif
}

static int bfqg_stats_init(struct bfqg_stats *stats, gfp_t gfp)
{
	if (blkg_rwstat_init(&stats->bytes, gfp) ||
	    blkg_rwstat_init(&stats->ios, gfp))
		goto error;

#ifdef CONFIG_BFQ_CGROUP_DEBUG
	if (blkg_rwstat_init(&stats->merged, gfp) ||
	    blkg_rwstat_init(&stats->service_time, gfp) ||
	    blkg_rwstat_init(&stats->wait_time, gfp) ||
	    blkg_rwstat_init(&stats->queued, gfp) ||
	    bfq_stat_init(&stats->time, gfp) ||
	    bfq_stat_init(&stats->avg_queue_size_sum, gfp) ||
	    bfq_stat_init(&stats->avg_queue_size_samples, gfp) ||
	    bfq_stat_init(&stats->dequeue, gfp) ||
	    bfq_stat_init(&stats->group_wait_time, gfp) ||
	    bfq_stat_init(&stats->idle_time, gfp) ||
	    bfq_stat_init(&stats->empty_time, gfp))
		goto error;
#endif

	return 0;

error:
	bfqg_stats_exit(stats);
	return -ENOMEM;
}

static struct bfq_group_data *cpd_to_bfqgd(struct blkcg_policy_data *cpd)
{
	return cpd ? container_of(cpd, struct bfq_group_data, pd) : NULL;
}

static struct bfq_group_data *blkcg_to_bfqgd(struct blkcg *blkcg)
{
	return cpd_to_bfqgd(blkcg_to_cpd(blkcg, &blkcg_policy_bfq));
}

static struct blkcg_policy_data *bfq_cpd_alloc(gfp_t gfp)
{
	struct bfq_group_data *bgd;

	bgd = kzalloc(sizeof(*bgd), gfp);
	if (!bgd)
		return NULL;

	bgd->weight = CGROUP_WEIGHT_DFL;
	return &bgd->pd;
}

static void bfq_cpd_free(struct blkcg_policy_data *cpd)
{
	kfree(cpd_to_bfqgd(cpd));
}

static struct blkg_policy_data *bfq_pd_alloc(struct gendisk *disk,
		struct blkcg *blkcg, gfp_t gfp)
{
	struct bfq_group *bfqg;

	bfqg = kzalloc_node(sizeof(*bfqg), gfp, disk->node_id);
	if (!bfqg)
		return NULL;

	if (bfqg_stats_init(&bfqg->stats, gfp)) {
		kfree(bfqg);
		return NULL;
	}

	 
	refcount_set(&bfqg->ref, 1);
	return &bfqg->pd;
}

static void bfq_pd_init(struct blkg_policy_data *pd)
{
	struct blkcg_gq *blkg = pd_to_blkg(pd);
	struct bfq_group *bfqg = blkg_to_bfqg(blkg);
	struct bfq_data *bfqd = blkg->q->elevator->elevator_data;
	struct bfq_entity *entity = &bfqg->entity;
	struct bfq_group_data *d = blkcg_to_bfqgd(blkg->blkcg);

	entity->orig_weight = entity->weight = entity->new_weight = d->weight;
	entity->my_sched_data = &bfqg->sched_data;
	entity->last_bfqq_created = NULL;

	bfqg->my_entity = entity;  
	bfqg->bfqd = bfqd;
	bfqg->active_entities = 0;
	bfqg->num_queues_with_pending_reqs = 0;
	bfqg->rq_pos_tree = RB_ROOT;
}

static void bfq_pd_free(struct blkg_policy_data *pd)
{
	struct bfq_group *bfqg = pd_to_bfqg(pd);

	bfqg_stats_exit(&bfqg->stats);
	bfqg_put(bfqg);
}

static void bfq_pd_reset_stats(struct blkg_policy_data *pd)
{
	struct bfq_group *bfqg = pd_to_bfqg(pd);

	bfqg_stats_reset(&bfqg->stats);
}

static void bfq_group_set_parent(struct bfq_group *bfqg,
					struct bfq_group *parent)
{
	struct bfq_entity *entity;

	entity = &bfqg->entity;
	entity->parent = parent->my_entity;
	entity->sched_data = &parent->sched_data;
}

static void bfq_link_bfqg(struct bfq_data *bfqd, struct bfq_group *bfqg)
{
	struct bfq_group *parent;
	struct bfq_entity *entity;

	 
	entity = &bfqg->entity;
	for_each_entity(entity) {
		struct bfq_group *curr_bfqg = container_of(entity,
						struct bfq_group, entity);
		if (curr_bfqg != bfqd->root_group) {
			parent = bfqg_parent(curr_bfqg);
			if (!parent)
				parent = bfqd->root_group;
			bfq_group_set_parent(curr_bfqg, parent);
		}
	}
}

struct bfq_group *bfq_bio_bfqg(struct bfq_data *bfqd, struct bio *bio)
{
	struct blkcg_gq *blkg = bio->bi_blkg;
	struct bfq_group *bfqg;

	while (blkg) {
		if (!blkg->online) {
			blkg = blkg->parent;
			continue;
		}
		bfqg = blkg_to_bfqg(blkg);
		if (bfqg->pd.online) {
			bio_associate_blkg_from_css(bio, &blkg->blkcg->css);
			return bfqg;
		}
		blkg = blkg->parent;
	}
	bio_associate_blkg_from_css(bio,
				&bfqg_to_blkg(bfqd->root_group)->blkcg->css);
	return bfqd->root_group;
}

 
void bfq_bfqq_move(struct bfq_data *bfqd, struct bfq_queue *bfqq,
		   struct bfq_group *bfqg)
{
	struct bfq_entity *entity = &bfqq->entity;
	struct bfq_group *old_parent = bfqq_group(bfqq);
	bool has_pending_reqs = false;

	 
	if (old_parent == bfqg)
		return;

	 
	if (bfqq == &bfqd->oom_bfqq)
		return;
	 
	bfqq->ref++;

	if (entity->in_groups_with_pending_reqs) {
		has_pending_reqs = true;
		bfq_del_bfqq_in_groups_with_pending_reqs(bfqq);
	}

	 
	if (bfqq == bfqd->in_service_queue)
		bfq_bfqq_expire(bfqd, bfqd->in_service_queue,
				false, BFQQE_PREEMPTED);

	if (bfq_bfqq_busy(bfqq))
		bfq_deactivate_bfqq(bfqd, bfqq, false, false);
	else if (entity->on_st_or_in_serv)
		bfq_put_idle_entity(bfq_entity_service_tree(entity), entity);
	bfqg_and_blkg_put(old_parent);

	if (entity->parent &&
	    entity->parent->last_bfqq_created == bfqq)
		entity->parent->last_bfqq_created = NULL;
	else if (bfqd->last_bfqq_created == bfqq)
		bfqd->last_bfqq_created = NULL;

	entity->parent = bfqg->my_entity;
	entity->sched_data = &bfqg->sched_data;
	 
	bfqg_and_blkg_get(bfqg);

	if (has_pending_reqs)
		bfq_add_bfqq_in_groups_with_pending_reqs(bfqq);

	if (bfq_bfqq_busy(bfqq)) {
		if (unlikely(!bfqd->nonrot_with_queueing))
			bfq_pos_tree_add_move(bfqd, bfqq);
		bfq_activate_bfqq(bfqd, bfqq);
	}

	if (!bfqd->in_service_queue && !bfqd->tot_rq_in_driver)
		bfq_schedule_dispatch(bfqd);
	 
	bfq_put_queue(bfqq);
}

static void bfq_sync_bfqq_move(struct bfq_data *bfqd,
			       struct bfq_queue *sync_bfqq,
			       struct bfq_io_cq *bic,
			       struct bfq_group *bfqg,
			       unsigned int act_idx)
{
	struct bfq_queue *bfqq;

	if (!sync_bfqq->new_bfqq && !bfq_bfqq_coop(sync_bfqq)) {
		 
		if (sync_bfqq->entity.sched_data != &bfqg->sched_data)
			bfq_bfqq_move(bfqd, sync_bfqq, bfqg);
		return;
	}

	 
	for (bfqq = sync_bfqq; bfqq; bfqq = bfqq->new_bfqq)
		if (bfqq->entity.sched_data != &bfqg->sched_data)
			break;
	if (bfqq) {
		 
		bfq_put_cooperator(sync_bfqq);
		bic_set_bfqq(bic, NULL, true, act_idx);
		bfq_release_process_ref(bfqd, sync_bfqq);
	}
}

 
static void __bfq_bic_change_cgroup(struct bfq_data *bfqd,
				    struct bfq_io_cq *bic,
				    struct bfq_group *bfqg)
{
	unsigned int act_idx;

	for (act_idx = 0; act_idx < bfqd->num_actuators; act_idx++) {
		struct bfq_queue *async_bfqq = bic_to_bfqq(bic, false, act_idx);
		struct bfq_queue *sync_bfqq = bic_to_bfqq(bic, true, act_idx);

		if (async_bfqq &&
		    async_bfqq->entity.sched_data != &bfqg->sched_data) {
			bic_set_bfqq(bic, NULL, false, act_idx);
			bfq_release_process_ref(bfqd, async_bfqq);
		}

		if (sync_bfqq)
			bfq_sync_bfqq_move(bfqd, sync_bfqq, bic, bfqg, act_idx);
	}
}

void bfq_bic_update_cgroup(struct bfq_io_cq *bic, struct bio *bio)
{
	struct bfq_data *bfqd = bic_to_bfqd(bic);
	struct bfq_group *bfqg = bfq_bio_bfqg(bfqd, bio);
	uint64_t serial_nr;

	serial_nr = bfqg_to_blkg(bfqg)->blkcg->css.serial_nr;

	 
	if (unlikely(!bfqd) || likely(bic->blkcg_serial_nr == serial_nr))
		return;

	 
	bfq_link_bfqg(bfqd, bfqg);
	__bfq_bic_change_cgroup(bfqd, bic, bfqg);
	 
	blkg_path(bfqg_to_blkg(bfqg), bfqg->blkg_path, sizeof(bfqg->blkg_path));
	bic->blkcg_serial_nr = serial_nr;
}

 
static void bfq_flush_idle_tree(struct bfq_service_tree *st)
{
	struct bfq_entity *entity = st->first_idle;

	for (; entity ; entity = st->first_idle)
		__bfq_deactivate_entity(entity, false);
}

 
static void bfq_reparent_leaf_entity(struct bfq_data *bfqd,
				     struct bfq_entity *entity,
				     int ioprio_class)
{
	struct bfq_queue *bfqq;
	struct bfq_entity *child_entity = entity;

	while (child_entity->my_sched_data) {  
		struct bfq_sched_data *child_sd = child_entity->my_sched_data;
		struct bfq_service_tree *child_st = child_sd->service_tree +
			ioprio_class;
		struct rb_root *child_active = &child_st->active;

		child_entity = bfq_entity_of(rb_first(child_active));

		if (!child_entity)
			child_entity = child_sd->in_service_entity;
	}

	bfqq = bfq_entity_to_bfqq(child_entity);
	bfq_bfqq_move(bfqd, bfqq, bfqd->root_group);
}

 
static void bfq_reparent_active_queues(struct bfq_data *bfqd,
				       struct bfq_group *bfqg,
				       struct bfq_service_tree *st,
				       int ioprio_class)
{
	struct rb_root *active = &st->active;
	struct bfq_entity *entity;

	while ((entity = bfq_entity_of(rb_first(active))))
		bfq_reparent_leaf_entity(bfqd, entity, ioprio_class);

	if (bfqg->sched_data.in_service_entity)
		bfq_reparent_leaf_entity(bfqd,
					 bfqg->sched_data.in_service_entity,
					 ioprio_class);
}

 
static void bfq_pd_offline(struct blkg_policy_data *pd)
{
	struct bfq_service_tree *st;
	struct bfq_group *bfqg = pd_to_bfqg(pd);
	struct bfq_data *bfqd = bfqg->bfqd;
	struct bfq_entity *entity = bfqg->my_entity;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&bfqd->lock, flags);

	if (!entity)  
		goto put_async_queues;

	 
	for (i = 0; i < BFQ_IOPRIO_CLASSES; i++) {
		st = bfqg->sched_data.service_tree + i;

		 
		bfq_reparent_active_queues(bfqd, bfqg, st, i);

		 
		bfq_flush_idle_tree(st);
	}

	__bfq_deactivate_entity(entity, false);

put_async_queues:
	bfq_put_async_queues(bfqd, bfqg);

	spin_unlock_irqrestore(&bfqd->lock, flags);
	 
	bfqg_stats_xfer_dead(bfqg);
}

void bfq_end_wr_async(struct bfq_data *bfqd)
{
	struct blkcg_gq *blkg;

	list_for_each_entry(blkg, &bfqd->queue->blkg_list, q_node) {
		struct bfq_group *bfqg = blkg_to_bfqg(blkg);

		bfq_end_wr_async_queues(bfqd, bfqg);
	}
	bfq_end_wr_async_queues(bfqd, bfqd->root_group);
}

static int bfq_io_show_weight_legacy(struct seq_file *sf, void *v)
{
	struct blkcg *blkcg = css_to_blkcg(seq_css(sf));
	struct bfq_group_data *bfqgd = blkcg_to_bfqgd(blkcg);
	unsigned int val = 0;

	if (bfqgd)
		val = bfqgd->weight;

	seq_printf(sf, "%u\n", val);

	return 0;
}

static u64 bfqg_prfill_weight_device(struct seq_file *sf,
				     struct blkg_policy_data *pd, int off)
{
	struct bfq_group *bfqg = pd_to_bfqg(pd);

	if (!bfqg->entity.dev_weight)
		return 0;
	return __blkg_prfill_u64(sf, pd, bfqg->entity.dev_weight);
}

static int bfq_io_show_weight(struct seq_file *sf, void *v)
{
	struct blkcg *blkcg = css_to_blkcg(seq_css(sf));
	struct bfq_group_data *bfqgd = blkcg_to_bfqgd(blkcg);

	seq_printf(sf, "default %u\n", bfqgd->weight);
	blkcg_print_blkgs(sf, blkcg, bfqg_prfill_weight_device,
			  &blkcg_policy_bfq, 0, false);
	return 0;
}

static void bfq_group_set_weight(struct bfq_group *bfqg, u64 weight, u64 dev_weight)
{
	weight = dev_weight ?: weight;

	bfqg->entity.dev_weight = dev_weight;
	 
	if ((unsigned short)weight != bfqg->entity.new_weight) {
		bfqg->entity.new_weight = (unsigned short)weight;
		 
		smp_wmb();
		bfqg->entity.prio_changed = 1;
	}
}

static int bfq_io_set_weight_legacy(struct cgroup_subsys_state *css,
				    struct cftype *cftype,
				    u64 val)
{
	struct blkcg *blkcg = css_to_blkcg(css);
	struct bfq_group_data *bfqgd = blkcg_to_bfqgd(blkcg);
	struct blkcg_gq *blkg;
	int ret = -ERANGE;

	if (val < BFQ_MIN_WEIGHT || val > BFQ_MAX_WEIGHT)
		return ret;

	ret = 0;
	spin_lock_irq(&blkcg->lock);
	bfqgd->weight = (unsigned short)val;
	hlist_for_each_entry(blkg, &blkcg->blkg_list, blkcg_node) {
		struct bfq_group *bfqg = blkg_to_bfqg(blkg);

		if (bfqg)
			bfq_group_set_weight(bfqg, val, 0);
	}
	spin_unlock_irq(&blkcg->lock);

	return ret;
}

static ssize_t bfq_io_set_device_weight(struct kernfs_open_file *of,
					char *buf, size_t nbytes,
					loff_t off)
{
	int ret;
	struct blkg_conf_ctx ctx;
	struct blkcg *blkcg = css_to_blkcg(of_css(of));
	struct bfq_group *bfqg;
	u64 v;

	blkg_conf_init(&ctx, buf);

	ret = blkg_conf_prep(blkcg, &blkcg_policy_bfq, &ctx);
	if (ret)
		goto out;

	if (sscanf(ctx.body, "%llu", &v) == 1) {
		 
		ret = -ERANGE;
		if (!v)
			goto out;
	} else if (!strcmp(strim(ctx.body), "default")) {
		v = 0;
	} else {
		ret = -EINVAL;
		goto out;
	}

	bfqg = blkg_to_bfqg(ctx.blkg);

	ret = -ERANGE;
	if (!v || (v >= BFQ_MIN_WEIGHT && v <= BFQ_MAX_WEIGHT)) {
		bfq_group_set_weight(bfqg, bfqg->entity.weight, v);
		ret = 0;
	}
out:
	blkg_conf_exit(&ctx);
	return ret ?: nbytes;
}

static ssize_t bfq_io_set_weight(struct kernfs_open_file *of,
				 char *buf, size_t nbytes,
				 loff_t off)
{
	char *endp;
	int ret;
	u64 v;

	buf = strim(buf);

	 
	v = simple_strtoull(buf, &endp, 0);
	if (*endp == '\0' || sscanf(buf, "default %llu", &v) == 1) {
		ret = bfq_io_set_weight_legacy(of_css(of), NULL, v);
		return ret ?: nbytes;
	}

	return bfq_io_set_device_weight(of, buf, nbytes, off);
}

static int bfqg_print_rwstat(struct seq_file *sf, void *v)
{
	blkcg_print_blkgs(sf, css_to_blkcg(seq_css(sf)), blkg_prfill_rwstat,
			  &blkcg_policy_bfq, seq_cft(sf)->private, true);
	return 0;
}

static u64 bfqg_prfill_rwstat_recursive(struct seq_file *sf,
					struct blkg_policy_data *pd, int off)
{
	struct blkg_rwstat_sample sum;

	blkg_rwstat_recursive_sum(pd_to_blkg(pd), &blkcg_policy_bfq, off, &sum);
	return __blkg_prfill_rwstat(sf, pd, &sum);
}

static int bfqg_print_rwstat_recursive(struct seq_file *sf, void *v)
{
	blkcg_print_blkgs(sf, css_to_blkcg(seq_css(sf)),
			  bfqg_prfill_rwstat_recursive, &blkcg_policy_bfq,
			  seq_cft(sf)->private, true);
	return 0;
}

#ifdef CONFIG_BFQ_CGROUP_DEBUG
static int bfqg_print_stat(struct seq_file *sf, void *v)
{
	blkcg_print_blkgs(sf, css_to_blkcg(seq_css(sf)), blkg_prfill_stat,
			  &blkcg_policy_bfq, seq_cft(sf)->private, false);
	return 0;
}

static u64 bfqg_prfill_stat_recursive(struct seq_file *sf,
				      struct blkg_policy_data *pd, int off)
{
	struct blkcg_gq *blkg = pd_to_blkg(pd);
	struct blkcg_gq *pos_blkg;
	struct cgroup_subsys_state *pos_css;
	u64 sum = 0;

	lockdep_assert_held(&blkg->q->queue_lock);

	rcu_read_lock();
	blkg_for_each_descendant_pre(pos_blkg, pos_css, blkg) {
		struct bfq_stat *stat;

		if (!pos_blkg->online)
			continue;

		stat = (void *)blkg_to_pd(pos_blkg, &blkcg_policy_bfq) + off;
		sum += bfq_stat_read(stat) + atomic64_read(&stat->aux_cnt);
	}
	rcu_read_unlock();

	return __blkg_prfill_u64(sf, pd, sum);
}

static int bfqg_print_stat_recursive(struct seq_file *sf, void *v)
{
	blkcg_print_blkgs(sf, css_to_blkcg(seq_css(sf)),
			  bfqg_prfill_stat_recursive, &blkcg_policy_bfq,
			  seq_cft(sf)->private, false);
	return 0;
}

static u64 bfqg_prfill_sectors(struct seq_file *sf, struct blkg_policy_data *pd,
			       int off)
{
	struct bfq_group *bfqg = blkg_to_bfqg(pd->blkg);
	u64 sum = blkg_rwstat_total(&bfqg->stats.bytes);

	return __blkg_prfill_u64(sf, pd, sum >> 9);
}

static int bfqg_print_stat_sectors(struct seq_file *sf, void *v)
{
	blkcg_print_blkgs(sf, css_to_blkcg(seq_css(sf)),
			  bfqg_prfill_sectors, &blkcg_policy_bfq, 0, false);
	return 0;
}

static u64 bfqg_prfill_sectors_recursive(struct seq_file *sf,
					 struct blkg_policy_data *pd, int off)
{
	struct blkg_rwstat_sample tmp;

	blkg_rwstat_recursive_sum(pd->blkg, &blkcg_policy_bfq,
			offsetof(struct bfq_group, stats.bytes), &tmp);

	return __blkg_prfill_u64(sf, pd,
		(tmp.cnt[BLKG_RWSTAT_READ] + tmp.cnt[BLKG_RWSTAT_WRITE]) >> 9);
}

static int bfqg_print_stat_sectors_recursive(struct seq_file *sf, void *v)
{
	blkcg_print_blkgs(sf, css_to_blkcg(seq_css(sf)),
			  bfqg_prfill_sectors_recursive, &blkcg_policy_bfq, 0,
			  false);
	return 0;
}

static u64 bfqg_prfill_avg_queue_size(struct seq_file *sf,
				      struct blkg_policy_data *pd, int off)
{
	struct bfq_group *bfqg = pd_to_bfqg(pd);
	u64 samples = bfq_stat_read(&bfqg->stats.avg_queue_size_samples);
	u64 v = 0;

	if (samples) {
		v = bfq_stat_read(&bfqg->stats.avg_queue_size_sum);
		v = div64_u64(v, samples);
	}
	__blkg_prfill_u64(sf, pd, v);
	return 0;
}

 
static int bfqg_print_avg_queue_size(struct seq_file *sf, void *v)
{
	blkcg_print_blkgs(sf, css_to_blkcg(seq_css(sf)),
			  bfqg_prfill_avg_queue_size, &blkcg_policy_bfq,
			  0, false);
	return 0;
}
#endif  

struct bfq_group *bfq_create_group_hierarchy(struct bfq_data *bfqd, int node)
{
	int ret;

	ret = blkcg_activate_policy(bfqd->queue->disk, &blkcg_policy_bfq);
	if (ret)
		return NULL;

	return blkg_to_bfqg(bfqd->queue->root_blkg);
}

struct blkcg_policy blkcg_policy_bfq = {
	.dfl_cftypes		= bfq_blkg_files,
	.legacy_cftypes		= bfq_blkcg_legacy_files,

	.cpd_alloc_fn		= bfq_cpd_alloc,
	.cpd_free_fn		= bfq_cpd_free,

	.pd_alloc_fn		= bfq_pd_alloc,
	.pd_init_fn		= bfq_pd_init,
	.pd_offline_fn		= bfq_pd_offline,
	.pd_free_fn		= bfq_pd_free,
	.pd_reset_stats_fn	= bfq_pd_reset_stats,
};

struct cftype bfq_blkcg_legacy_files[] = {
	{
		.name = "bfq.weight",
		.flags = CFTYPE_NOT_ON_ROOT,
		.seq_show = bfq_io_show_weight_legacy,
		.write_u64 = bfq_io_set_weight_legacy,
	},
	{
		.name = "bfq.weight_device",
		.flags = CFTYPE_NOT_ON_ROOT,
		.seq_show = bfq_io_show_weight,
		.write = bfq_io_set_weight,
	},

	 
	{
		.name = "bfq.io_service_bytes",
		.private = offsetof(struct bfq_group, stats.bytes),
		.seq_show = bfqg_print_rwstat,
	},
	{
		.name = "bfq.io_serviced",
		.private = offsetof(struct bfq_group, stats.ios),
		.seq_show = bfqg_print_rwstat,
	},
#ifdef CONFIG_BFQ_CGROUP_DEBUG
	{
		.name = "bfq.time",
		.private = offsetof(struct bfq_group, stats.time),
		.seq_show = bfqg_print_stat,
	},
	{
		.name = "bfq.sectors",
		.seq_show = bfqg_print_stat_sectors,
	},
	{
		.name = "bfq.io_service_time",
		.private = offsetof(struct bfq_group, stats.service_time),
		.seq_show = bfqg_print_rwstat,
	},
	{
		.name = "bfq.io_wait_time",
		.private = offsetof(struct bfq_group, stats.wait_time),
		.seq_show = bfqg_print_rwstat,
	},
	{
		.name = "bfq.io_merged",
		.private = offsetof(struct bfq_group, stats.merged),
		.seq_show = bfqg_print_rwstat,
	},
	{
		.name = "bfq.io_queued",
		.private = offsetof(struct bfq_group, stats.queued),
		.seq_show = bfqg_print_rwstat,
	},
#endif  

	 
	{
		.name = "bfq.io_service_bytes_recursive",
		.private = offsetof(struct bfq_group, stats.bytes),
		.seq_show = bfqg_print_rwstat_recursive,
	},
	{
		.name = "bfq.io_serviced_recursive",
		.private = offsetof(struct bfq_group, stats.ios),
		.seq_show = bfqg_print_rwstat_recursive,
	},
#ifdef CONFIG_BFQ_CGROUP_DEBUG
	{
		.name = "bfq.time_recursive",
		.private = offsetof(struct bfq_group, stats.time),
		.seq_show = bfqg_print_stat_recursive,
	},
	{
		.name = "bfq.sectors_recursive",
		.seq_show = bfqg_print_stat_sectors_recursive,
	},
	{
		.name = "bfq.io_service_time_recursive",
		.private = offsetof(struct bfq_group, stats.service_time),
		.seq_show = bfqg_print_rwstat_recursive,
	},
	{
		.name = "bfq.io_wait_time_recursive",
		.private = offsetof(struct bfq_group, stats.wait_time),
		.seq_show = bfqg_print_rwstat_recursive,
	},
	{
		.name = "bfq.io_merged_recursive",
		.private = offsetof(struct bfq_group, stats.merged),
		.seq_show = bfqg_print_rwstat_recursive,
	},
	{
		.name = "bfq.io_queued_recursive",
		.private = offsetof(struct bfq_group, stats.queued),
		.seq_show = bfqg_print_rwstat_recursive,
	},
	{
		.name = "bfq.avg_queue_size",
		.seq_show = bfqg_print_avg_queue_size,
	},
	{
		.name = "bfq.group_wait_time",
		.private = offsetof(struct bfq_group, stats.group_wait_time),
		.seq_show = bfqg_print_stat,
	},
	{
		.name = "bfq.idle_time",
		.private = offsetof(struct bfq_group, stats.idle_time),
		.seq_show = bfqg_print_stat,
	},
	{
		.name = "bfq.empty_time",
		.private = offsetof(struct bfq_group, stats.empty_time),
		.seq_show = bfqg_print_stat,
	},
	{
		.name = "bfq.dequeue",
		.private = offsetof(struct bfq_group, stats.dequeue),
		.seq_show = bfqg_print_stat,
	},
#endif	 
	{ }	 
};

struct cftype bfq_blkg_files[] = {
	{
		.name = "bfq.weight",
		.flags = CFTYPE_NOT_ON_ROOT,
		.seq_show = bfq_io_show_weight,
		.write = bfq_io_set_weight,
	},
	{}  
};

#else	 

void bfq_bfqq_move(struct bfq_data *bfqd, struct bfq_queue *bfqq,
		   struct bfq_group *bfqg) {}

void bfq_init_entity(struct bfq_entity *entity, struct bfq_group *bfqg)
{
	struct bfq_queue *bfqq = bfq_entity_to_bfqq(entity);

	entity->weight = entity->new_weight;
	entity->orig_weight = entity->new_weight;
	if (bfqq) {
		bfqq->ioprio = bfqq->new_ioprio;
		bfqq->ioprio_class = bfqq->new_ioprio_class;
	}
	entity->sched_data = &bfqg->sched_data;
}

void bfq_bic_update_cgroup(struct bfq_io_cq *bic, struct bio *bio) {}

void bfq_end_wr_async(struct bfq_data *bfqd)
{
	bfq_end_wr_async_queues(bfqd, bfqd->root_group);
}

struct bfq_group *bfq_bio_bfqg(struct bfq_data *bfqd, struct bio *bio)
{
	return bfqd->root_group;
}

struct bfq_group *bfqq_group(struct bfq_queue *bfqq)
{
	return bfqq->bfqd->root_group;
}

void bfqg_and_blkg_put(struct bfq_group *bfqg) {}

struct bfq_group *bfq_create_group_hierarchy(struct bfq_data *bfqd, int node)
{
	struct bfq_group *bfqg;
	int i;

	bfqg = kmalloc_node(sizeof(*bfqg), GFP_KERNEL | __GFP_ZERO, node);
	if (!bfqg)
		return NULL;

	for (i = 0; i < BFQ_IOPRIO_CLASSES; i++)
		bfqg->sched_data.service_tree[i] = BFQ_SERVICE_TREE_INIT;

	return bfqg;
}
#endif	 
