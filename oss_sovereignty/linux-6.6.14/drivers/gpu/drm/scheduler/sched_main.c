 

 

#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/dma-resv.h>
#include <uapi/linux/sched/types.h>

#include <drm/drm_print.h>
#include <drm/drm_gem.h>
#include <drm/drm_syncobj.h>
#include <drm/gpu_scheduler.h>
#include <drm/spsc_queue.h>

#define CREATE_TRACE_POINTS
#include "gpu_scheduler_trace.h"

#define to_drm_sched_job(sched_job)		\
		container_of((sched_job), struct drm_sched_job, queue_node)

int drm_sched_policy = DRM_SCHED_POLICY_FIFO;

 
MODULE_PARM_DESC(sched_policy, "Specify the scheduling policy for entities on a run-queue, " __stringify(DRM_SCHED_POLICY_RR) " = Round Robin, " __stringify(DRM_SCHED_POLICY_FIFO) " = FIFO (default).");
module_param_named(sched_policy, drm_sched_policy, int, 0444);

static __always_inline bool drm_sched_entity_compare_before(struct rb_node *a,
							    const struct rb_node *b)
{
	struct drm_sched_entity *ent_a =  rb_entry((a), struct drm_sched_entity, rb_tree_node);
	struct drm_sched_entity *ent_b =  rb_entry((b), struct drm_sched_entity, rb_tree_node);

	return ktime_before(ent_a->oldest_job_waiting, ent_b->oldest_job_waiting);
}

static inline void drm_sched_rq_remove_fifo_locked(struct drm_sched_entity *entity)
{
	struct drm_sched_rq *rq = entity->rq;

	if (!RB_EMPTY_NODE(&entity->rb_tree_node)) {
		rb_erase_cached(&entity->rb_tree_node, &rq->rb_tree_root);
		RB_CLEAR_NODE(&entity->rb_tree_node);
	}
}

void drm_sched_rq_update_fifo(struct drm_sched_entity *entity, ktime_t ts)
{
	 
	spin_lock(&entity->rq_lock);
	spin_lock(&entity->rq->lock);

	drm_sched_rq_remove_fifo_locked(entity);

	entity->oldest_job_waiting = ts;

	rb_add_cached(&entity->rb_tree_node, &entity->rq->rb_tree_root,
		      drm_sched_entity_compare_before);

	spin_unlock(&entity->rq->lock);
	spin_unlock(&entity->rq_lock);
}

 
static void drm_sched_rq_init(struct drm_gpu_scheduler *sched,
			      struct drm_sched_rq *rq)
{
	spin_lock_init(&rq->lock);
	INIT_LIST_HEAD(&rq->entities);
	rq->rb_tree_root = RB_ROOT_CACHED;
	rq->current_entity = NULL;
	rq->sched = sched;
}

 
void drm_sched_rq_add_entity(struct drm_sched_rq *rq,
			     struct drm_sched_entity *entity)
{
	if (!list_empty(&entity->list))
		return;

	spin_lock(&rq->lock);

	atomic_inc(rq->sched->score);
	list_add_tail(&entity->list, &rq->entities);

	spin_unlock(&rq->lock);
}

 
void drm_sched_rq_remove_entity(struct drm_sched_rq *rq,
				struct drm_sched_entity *entity)
{
	if (list_empty(&entity->list))
		return;

	spin_lock(&rq->lock);

	atomic_dec(rq->sched->score);
	list_del_init(&entity->list);

	if (rq->current_entity == entity)
		rq->current_entity = NULL;

	if (drm_sched_policy == DRM_SCHED_POLICY_FIFO)
		drm_sched_rq_remove_fifo_locked(entity);

	spin_unlock(&rq->lock);
}

 
static struct drm_sched_entity *
drm_sched_rq_select_entity_rr(struct drm_sched_rq *rq)
{
	struct drm_sched_entity *entity;

	spin_lock(&rq->lock);

	entity = rq->current_entity;
	if (entity) {
		list_for_each_entry_continue(entity, &rq->entities, list) {
			if (drm_sched_entity_is_ready(entity)) {
				rq->current_entity = entity;
				reinit_completion(&entity->entity_idle);
				spin_unlock(&rq->lock);
				return entity;
			}
		}
	}

	list_for_each_entry(entity, &rq->entities, list) {

		if (drm_sched_entity_is_ready(entity)) {
			rq->current_entity = entity;
			reinit_completion(&entity->entity_idle);
			spin_unlock(&rq->lock);
			return entity;
		}

		if (entity == rq->current_entity)
			break;
	}

	spin_unlock(&rq->lock);

	return NULL;
}

 
static struct drm_sched_entity *
drm_sched_rq_select_entity_fifo(struct drm_sched_rq *rq)
{
	struct rb_node *rb;

	spin_lock(&rq->lock);
	for (rb = rb_first_cached(&rq->rb_tree_root); rb; rb = rb_next(rb)) {
		struct drm_sched_entity *entity;

		entity = rb_entry(rb, struct drm_sched_entity, rb_tree_node);
		if (drm_sched_entity_is_ready(entity)) {
			rq->current_entity = entity;
			reinit_completion(&entity->entity_idle);
			break;
		}
	}
	spin_unlock(&rq->lock);

	return rb ? rb_entry(rb, struct drm_sched_entity, rb_tree_node) : NULL;
}

 
static void drm_sched_job_done(struct drm_sched_job *s_job, int result)
{
	struct drm_sched_fence *s_fence = s_job->s_fence;
	struct drm_gpu_scheduler *sched = s_fence->sched;

	atomic_dec(&sched->hw_rq_count);
	atomic_dec(sched->score);

	trace_drm_sched_process_job(s_fence);

	dma_fence_get(&s_fence->finished);
	drm_sched_fence_finished(s_fence, result);
	dma_fence_put(&s_fence->finished);
	wake_up_interruptible(&sched->wake_up_worker);
}

 
static void drm_sched_job_done_cb(struct dma_fence *f, struct dma_fence_cb *cb)
{
	struct drm_sched_job *s_job = container_of(cb, struct drm_sched_job, cb);

	drm_sched_job_done(s_job, f->error);
}

 
static void drm_sched_start_timeout(struct drm_gpu_scheduler *sched)
{
	if (sched->timeout != MAX_SCHEDULE_TIMEOUT &&
	    !list_empty(&sched->pending_list))
		queue_delayed_work(sched->timeout_wq, &sched->work_tdr, sched->timeout);
}

 
void drm_sched_fault(struct drm_gpu_scheduler *sched)
{
	if (sched->timeout_wq)
		mod_delayed_work(sched->timeout_wq, &sched->work_tdr, 0);
}
EXPORT_SYMBOL(drm_sched_fault);

 
unsigned long drm_sched_suspend_timeout(struct drm_gpu_scheduler *sched)
{
	unsigned long sched_timeout, now = jiffies;

	sched_timeout = sched->work_tdr.timer.expires;

	 
	if (mod_delayed_work(sched->timeout_wq, &sched->work_tdr, MAX_SCHEDULE_TIMEOUT)
			&& time_after(sched_timeout, now))
		return sched_timeout - now;
	else
		return sched->timeout;
}
EXPORT_SYMBOL(drm_sched_suspend_timeout);

 
void drm_sched_resume_timeout(struct drm_gpu_scheduler *sched,
		unsigned long remaining)
{
	spin_lock(&sched->job_list_lock);

	if (list_empty(&sched->pending_list))
		cancel_delayed_work(&sched->work_tdr);
	else
		mod_delayed_work(sched->timeout_wq, &sched->work_tdr, remaining);

	spin_unlock(&sched->job_list_lock);
}
EXPORT_SYMBOL(drm_sched_resume_timeout);

static void drm_sched_job_begin(struct drm_sched_job *s_job)
{
	struct drm_gpu_scheduler *sched = s_job->sched;

	spin_lock(&sched->job_list_lock);
	list_add_tail(&s_job->list, &sched->pending_list);
	drm_sched_start_timeout(sched);
	spin_unlock(&sched->job_list_lock);
}

static void drm_sched_job_timedout(struct work_struct *work)
{
	struct drm_gpu_scheduler *sched;
	struct drm_sched_job *job;
	enum drm_gpu_sched_stat status = DRM_GPU_SCHED_STAT_NOMINAL;

	sched = container_of(work, struct drm_gpu_scheduler, work_tdr.work);

	 
	spin_lock(&sched->job_list_lock);
	job = list_first_entry_or_null(&sched->pending_list,
				       struct drm_sched_job, list);

	if (job) {
		 
		list_del_init(&job->list);
		spin_unlock(&sched->job_list_lock);

		status = job->sched->ops->timedout_job(job);

		 
		if (sched->free_guilty) {
			job->sched->ops->free_job(job);
			sched->free_guilty = false;
		}
	} else {
		spin_unlock(&sched->job_list_lock);
	}

	if (status != DRM_GPU_SCHED_STAT_ENODEV) {
		spin_lock(&sched->job_list_lock);
		drm_sched_start_timeout(sched);
		spin_unlock(&sched->job_list_lock);
	}
}

 
void drm_sched_stop(struct drm_gpu_scheduler *sched, struct drm_sched_job *bad)
{
	struct drm_sched_job *s_job, *tmp;

	kthread_park(sched->thread);

	 
	if (bad && bad->sched == sched)
		 
		list_add(&bad->list, &sched->pending_list);

	 
	list_for_each_entry_safe_reverse(s_job, tmp, &sched->pending_list,
					 list) {
		if (s_job->s_fence->parent &&
		    dma_fence_remove_callback(s_job->s_fence->parent,
					      &s_job->cb)) {
			dma_fence_put(s_job->s_fence->parent);
			s_job->s_fence->parent = NULL;
			atomic_dec(&sched->hw_rq_count);
		} else {
			 
			spin_lock(&sched->job_list_lock);
			list_del_init(&s_job->list);
			spin_unlock(&sched->job_list_lock);

			 
			dma_fence_wait(&s_job->s_fence->finished, false);

			 
			if (bad != s_job)
				sched->ops->free_job(s_job);
			else
				sched->free_guilty = true;
		}
	}

	 
	cancel_delayed_work(&sched->work_tdr);
}

EXPORT_SYMBOL(drm_sched_stop);

 
void drm_sched_start(struct drm_gpu_scheduler *sched, bool full_recovery)
{
	struct drm_sched_job *s_job, *tmp;
	int r;

	 
	list_for_each_entry_safe(s_job, tmp, &sched->pending_list, list) {
		struct dma_fence *fence = s_job->s_fence->parent;

		atomic_inc(&sched->hw_rq_count);

		if (!full_recovery)
			continue;

		if (fence) {
			r = dma_fence_add_callback(fence, &s_job->cb,
						   drm_sched_job_done_cb);
			if (r == -ENOENT)
				drm_sched_job_done(s_job, fence->error);
			else if (r)
				DRM_DEV_ERROR(sched->dev, "fence add callback failed (%d)\n",
					  r);
		} else
			drm_sched_job_done(s_job, -ECANCELED);
	}

	if (full_recovery) {
		spin_lock(&sched->job_list_lock);
		drm_sched_start_timeout(sched);
		spin_unlock(&sched->job_list_lock);
	}

	kthread_unpark(sched->thread);
}
EXPORT_SYMBOL(drm_sched_start);

 
void drm_sched_resubmit_jobs(struct drm_gpu_scheduler *sched)
{
	struct drm_sched_job *s_job, *tmp;
	uint64_t guilty_context;
	bool found_guilty = false;
	struct dma_fence *fence;

	list_for_each_entry_safe(s_job, tmp, &sched->pending_list, list) {
		struct drm_sched_fence *s_fence = s_job->s_fence;

		if (!found_guilty && atomic_read(&s_job->karma) > sched->hang_limit) {
			found_guilty = true;
			guilty_context = s_job->s_fence->scheduled.context;
		}

		if (found_guilty && s_job->s_fence->scheduled.context == guilty_context)
			dma_fence_set_error(&s_fence->finished, -ECANCELED);

		fence = sched->ops->run_job(s_job);

		if (IS_ERR_OR_NULL(fence)) {
			if (IS_ERR(fence))
				dma_fence_set_error(&s_fence->finished, PTR_ERR(fence));

			s_job->s_fence->parent = NULL;
		} else {

			s_job->s_fence->parent = dma_fence_get(fence);

			 
			dma_fence_put(fence);
		}
	}
}
EXPORT_SYMBOL(drm_sched_resubmit_jobs);

 
int drm_sched_job_init(struct drm_sched_job *job,
		       struct drm_sched_entity *entity,
		       void *owner)
{
	if (!entity->rq)
		return -ENOENT;

	job->entity = entity;
	job->s_fence = drm_sched_fence_alloc(entity, owner);
	if (!job->s_fence)
		return -ENOMEM;

	INIT_LIST_HEAD(&job->list);

	xa_init_flags(&job->dependencies, XA_FLAGS_ALLOC);

	return 0;
}
EXPORT_SYMBOL(drm_sched_job_init);

 
void drm_sched_job_arm(struct drm_sched_job *job)
{
	struct drm_gpu_scheduler *sched;
	struct drm_sched_entity *entity = job->entity;

	BUG_ON(!entity);
	drm_sched_entity_select_rq(entity);
	sched = entity->rq->sched;

	job->sched = sched;
	job->s_priority = entity->rq - sched->sched_rq;
	job->id = atomic64_inc_return(&sched->job_id_count);

	drm_sched_fence_init(job->s_fence, job->entity);
}
EXPORT_SYMBOL(drm_sched_job_arm);

 
int drm_sched_job_add_dependency(struct drm_sched_job *job,
				 struct dma_fence *fence)
{
	struct dma_fence *entry;
	unsigned long index;
	u32 id = 0;
	int ret;

	if (!fence)
		return 0;

	 
	xa_for_each(&job->dependencies, index, entry) {
		if (entry->context != fence->context)
			continue;

		if (dma_fence_is_later(fence, entry)) {
			dma_fence_put(entry);
			xa_store(&job->dependencies, index, fence, GFP_KERNEL);
		} else {
			dma_fence_put(fence);
		}
		return 0;
	}

	ret = xa_alloc(&job->dependencies, &id, fence, xa_limit_32b, GFP_KERNEL);
	if (ret != 0)
		dma_fence_put(fence);

	return ret;
}
EXPORT_SYMBOL(drm_sched_job_add_dependency);

 
int drm_sched_job_add_syncobj_dependency(struct drm_sched_job *job,
					 struct drm_file *file,
					 u32 handle,
					 u32 point)
{
	struct dma_fence *fence;
	int ret;

	ret = drm_syncobj_find_fence(file, handle, point, 0, &fence);
	if (ret)
		return ret;

	return drm_sched_job_add_dependency(job, fence);
}
EXPORT_SYMBOL(drm_sched_job_add_syncobj_dependency);

 
int drm_sched_job_add_resv_dependencies(struct drm_sched_job *job,
					struct dma_resv *resv,
					enum dma_resv_usage usage)
{
	struct dma_resv_iter cursor;
	struct dma_fence *fence;
	int ret;

	dma_resv_assert_held(resv);

	dma_resv_for_each_fence(&cursor, resv, usage, fence) {
		 
		dma_fence_get(fence);
		ret = drm_sched_job_add_dependency(job, fence);
		if (ret) {
			dma_fence_put(fence);
			return ret;
		}
	}
	return 0;
}
EXPORT_SYMBOL(drm_sched_job_add_resv_dependencies);

 
int drm_sched_job_add_implicit_dependencies(struct drm_sched_job *job,
					    struct drm_gem_object *obj,
					    bool write)
{
	return drm_sched_job_add_resv_dependencies(job, obj->resv,
						   dma_resv_usage_rw(write));
}
EXPORT_SYMBOL(drm_sched_job_add_implicit_dependencies);

 
void drm_sched_job_cleanup(struct drm_sched_job *job)
{
	struct dma_fence *fence;
	unsigned long index;

	if (kref_read(&job->s_fence->finished.refcount)) {
		 
		dma_fence_put(&job->s_fence->finished);
	} else {
		 
		drm_sched_fence_free(job->s_fence);
	}

	job->s_fence = NULL;

	xa_for_each(&job->dependencies, index, fence) {
		dma_fence_put(fence);
	}
	xa_destroy(&job->dependencies);

}
EXPORT_SYMBOL(drm_sched_job_cleanup);

 
static bool drm_sched_can_queue(struct drm_gpu_scheduler *sched)
{
	return atomic_read(&sched->hw_rq_count) <
		sched->hw_submission_limit;
}

 
void drm_sched_wakeup_if_can_queue(struct drm_gpu_scheduler *sched)
{
	if (drm_sched_can_queue(sched))
		wake_up_interruptible(&sched->wake_up_worker);
}

 
static struct drm_sched_entity *
drm_sched_select_entity(struct drm_gpu_scheduler *sched)
{
	struct drm_sched_entity *entity;
	int i;

	if (!drm_sched_can_queue(sched))
		return NULL;

	 
	for (i = DRM_SCHED_PRIORITY_COUNT - 1; i >= DRM_SCHED_PRIORITY_MIN; i--) {
		entity = drm_sched_policy == DRM_SCHED_POLICY_FIFO ?
			drm_sched_rq_select_entity_fifo(&sched->sched_rq[i]) :
			drm_sched_rq_select_entity_rr(&sched->sched_rq[i]);
		if (entity)
			break;
	}

	return entity;
}

 
static struct drm_sched_job *
drm_sched_get_cleanup_job(struct drm_gpu_scheduler *sched)
{
	struct drm_sched_job *job, *next;

	spin_lock(&sched->job_list_lock);

	job = list_first_entry_or_null(&sched->pending_list,
				       struct drm_sched_job, list);

	if (job && dma_fence_is_signaled(&job->s_fence->finished)) {
		 
		list_del_init(&job->list);

		 
		cancel_delayed_work(&sched->work_tdr);
		 
		next = list_first_entry_or_null(&sched->pending_list,
						typeof(*next), list);

		if (next) {
			next->s_fence->scheduled.timestamp =
				dma_fence_timestamp(&job->s_fence->finished);
			 
			drm_sched_start_timeout(sched);
		}
	} else {
		job = NULL;
	}

	spin_unlock(&sched->job_list_lock);

	return job;
}

 
struct drm_gpu_scheduler *
drm_sched_pick_best(struct drm_gpu_scheduler **sched_list,
		     unsigned int num_sched_list)
{
	struct drm_gpu_scheduler *sched, *picked_sched = NULL;
	int i;
	unsigned int min_score = UINT_MAX, num_score;

	for (i = 0; i < num_sched_list; ++i) {
		sched = sched_list[i];

		if (!sched->ready) {
			DRM_WARN("scheduler %s is not ready, skipping",
				 sched->name);
			continue;
		}

		num_score = atomic_read(sched->score);
		if (num_score < min_score) {
			min_score = num_score;
			picked_sched = sched;
		}
	}

	return picked_sched;
}
EXPORT_SYMBOL(drm_sched_pick_best);

 
static bool drm_sched_blocked(struct drm_gpu_scheduler *sched)
{
	if (kthread_should_park()) {
		kthread_parkme();
		return true;
	}

	return false;
}

 
static int drm_sched_main(void *param)
{
	struct drm_gpu_scheduler *sched = (struct drm_gpu_scheduler *)param;
	int r;

	sched_set_fifo_low(current);

	while (!kthread_should_stop()) {
		struct drm_sched_entity *entity = NULL;
		struct drm_sched_fence *s_fence;
		struct drm_sched_job *sched_job;
		struct dma_fence *fence;
		struct drm_sched_job *cleanup_job = NULL;

		wait_event_interruptible(sched->wake_up_worker,
					 (cleanup_job = drm_sched_get_cleanup_job(sched)) ||
					 (!drm_sched_blocked(sched) &&
					  (entity = drm_sched_select_entity(sched))) ||
					 kthread_should_stop());

		if (cleanup_job)
			sched->ops->free_job(cleanup_job);

		if (!entity)
			continue;

		sched_job = drm_sched_entity_pop_job(entity);

		if (!sched_job) {
			complete_all(&entity->entity_idle);
			continue;
		}

		s_fence = sched_job->s_fence;

		atomic_inc(&sched->hw_rq_count);
		drm_sched_job_begin(sched_job);

		trace_drm_run_job(sched_job, entity);
		fence = sched->ops->run_job(sched_job);
		complete_all(&entity->entity_idle);
		drm_sched_fence_scheduled(s_fence, fence);

		if (!IS_ERR_OR_NULL(fence)) {
			 
			dma_fence_put(fence);

			r = dma_fence_add_callback(fence, &sched_job->cb,
						   drm_sched_job_done_cb);
			if (r == -ENOENT)
				drm_sched_job_done(sched_job, fence->error);
			else if (r)
				DRM_DEV_ERROR(sched->dev, "fence add callback failed (%d)\n",
					  r);
		} else {
			drm_sched_job_done(sched_job, IS_ERR(fence) ?
					   PTR_ERR(fence) : 0);
		}

		wake_up(&sched->job_scheduled);
	}
	return 0;
}

 
int drm_sched_init(struct drm_gpu_scheduler *sched,
		   const struct drm_sched_backend_ops *ops,
		   unsigned hw_submission, unsigned hang_limit,
		   long timeout, struct workqueue_struct *timeout_wq,
		   atomic_t *score, const char *name, struct device *dev)
{
	int i, ret;
	sched->ops = ops;
	sched->hw_submission_limit = hw_submission;
	sched->name = name;
	sched->timeout = timeout;
	sched->timeout_wq = timeout_wq ? : system_wq;
	sched->hang_limit = hang_limit;
	sched->score = score ? score : &sched->_score;
	sched->dev = dev;
	for (i = DRM_SCHED_PRIORITY_MIN; i < DRM_SCHED_PRIORITY_COUNT; i++)
		drm_sched_rq_init(sched, &sched->sched_rq[i]);

	init_waitqueue_head(&sched->wake_up_worker);
	init_waitqueue_head(&sched->job_scheduled);
	INIT_LIST_HEAD(&sched->pending_list);
	spin_lock_init(&sched->job_list_lock);
	atomic_set(&sched->hw_rq_count, 0);
	INIT_DELAYED_WORK(&sched->work_tdr, drm_sched_job_timedout);
	atomic_set(&sched->_score, 0);
	atomic64_set(&sched->job_id_count, 0);

	 
	sched->thread = kthread_run(drm_sched_main, sched, sched->name);
	if (IS_ERR(sched->thread)) {
		ret = PTR_ERR(sched->thread);
		sched->thread = NULL;
		DRM_DEV_ERROR(sched->dev, "Failed to create scheduler for %s.\n", name);
		return ret;
	}

	sched->ready = true;
	return 0;
}
EXPORT_SYMBOL(drm_sched_init);

 
void drm_sched_fini(struct drm_gpu_scheduler *sched)
{
	struct drm_sched_entity *s_entity;
	int i;

	if (sched->thread)
		kthread_stop(sched->thread);

	for (i = DRM_SCHED_PRIORITY_COUNT - 1; i >= DRM_SCHED_PRIORITY_MIN; i--) {
		struct drm_sched_rq *rq = &sched->sched_rq[i];

		spin_lock(&rq->lock);
		list_for_each_entry(s_entity, &rq->entities, list)
			 
			s_entity->stopped = true;
		spin_unlock(&rq->lock);

	}

	 
	wake_up_all(&sched->job_scheduled);

	 
	cancel_delayed_work_sync(&sched->work_tdr);

	sched->ready = false;
}
EXPORT_SYMBOL(drm_sched_fini);

 
void drm_sched_increase_karma(struct drm_sched_job *bad)
{
	int i;
	struct drm_sched_entity *tmp;
	struct drm_sched_entity *entity;
	struct drm_gpu_scheduler *sched = bad->sched;

	 
	if (bad->s_priority != DRM_SCHED_PRIORITY_KERNEL) {
		atomic_inc(&bad->karma);

		for (i = DRM_SCHED_PRIORITY_MIN; i < DRM_SCHED_PRIORITY_KERNEL;
		     i++) {
			struct drm_sched_rq *rq = &sched->sched_rq[i];

			spin_lock(&rq->lock);
			list_for_each_entry_safe(entity, tmp, &rq->entities, list) {
				if (bad->s_fence->scheduled.context ==
				    entity->fence_context) {
					if (entity->guilty)
						atomic_set(entity->guilty, 1);
					break;
				}
			}
			spin_unlock(&rq->lock);
			if (&entity->list != &rq->entities)
				break;
		}
	}
}
EXPORT_SYMBOL(drm_sched_increase_karma);
