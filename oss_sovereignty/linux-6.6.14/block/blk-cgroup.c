
 
#include <linux/ioprio.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/sched/signal.h>
#include <linux/err.h>
#include <linux/blkdev.h>
#include <linux/backing-dev.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/ctype.h>
#include <linux/resume_user_mode.h>
#include <linux/psi.h>
#include <linux/part_stat.h>
#include "blk.h"
#include "blk-cgroup.h"
#include "blk-ioprio.h"
#include "blk-throttle.h"

static void __blkcg_rstat_flush(struct blkcg *blkcg, int cpu);

 
static DEFINE_MUTEX(blkcg_pol_register_mutex);
static DEFINE_MUTEX(blkcg_pol_mutex);

struct blkcg blkcg_root;
EXPORT_SYMBOL_GPL(blkcg_root);

struct cgroup_subsys_state * const blkcg_root_css = &blkcg_root.css;
EXPORT_SYMBOL_GPL(blkcg_root_css);

static struct blkcg_policy *blkcg_policy[BLKCG_MAX_POLS];

static LIST_HEAD(all_blkcgs);		 

bool blkcg_debug_stats = false;

static DEFINE_RAW_SPINLOCK(blkg_stat_lock);

#define BLKG_DESTROY_BATCH_SIZE  64

 
static int init_blkcg_llists(struct blkcg *blkcg)
{
	int cpu;

	blkcg->lhead = alloc_percpu_gfp(struct llist_head, GFP_KERNEL);
	if (!blkcg->lhead)
		return -ENOMEM;

	for_each_possible_cpu(cpu)
		init_llist_head(per_cpu_ptr(blkcg->lhead, cpu));
	return 0;
}

 
static struct cgroup_subsys_state *blkcg_css(void)
{
	struct cgroup_subsys_state *css;

	css = kthread_blkcg();
	if (css)
		return css;
	return task_css(current, io_cgrp_id);
}

static bool blkcg_policy_enabled(struct request_queue *q,
				 const struct blkcg_policy *pol)
{
	return pol && test_bit(pol->plid, q->blkcg_pols);
}

static void blkg_free_workfn(struct work_struct *work)
{
	struct blkcg_gq *blkg = container_of(work, struct blkcg_gq,
					     free_work);
	struct request_queue *q = blkg->q;
	int i;

	 
	mutex_lock(&q->blkcg_mutex);
	for (i = 0; i < BLKCG_MAX_POLS; i++)
		if (blkg->pd[i])
			blkcg_policy[i]->pd_free_fn(blkg->pd[i]);
	if (blkg->parent)
		blkg_put(blkg->parent);
	spin_lock_irq(&q->queue_lock);
	list_del_init(&blkg->q_node);
	spin_unlock_irq(&q->queue_lock);
	mutex_unlock(&q->blkcg_mutex);

	blk_put_queue(q);
	free_percpu(blkg->iostat_cpu);
	percpu_ref_exit(&blkg->refcnt);
	kfree(blkg);
}

 
static void blkg_free(struct blkcg_gq *blkg)
{
	if (!blkg)
		return;

	 
	INIT_WORK(&blkg->free_work, blkg_free_workfn);
	schedule_work(&blkg->free_work);
}

static void __blkg_release(struct rcu_head *rcu)
{
	struct blkcg_gq *blkg = container_of(rcu, struct blkcg_gq, rcu_head);
	struct blkcg *blkcg = blkg->blkcg;
	int cpu;

#ifdef CONFIG_BLK_CGROUP_PUNT_BIO
	WARN_ON(!bio_list_empty(&blkg->async_bios));
#endif
	 
	for_each_possible_cpu(cpu)
		__blkcg_rstat_flush(blkcg, cpu);

	 
	css_put(&blkg->blkcg->css);
	blkg_free(blkg);
}

 
static void blkg_release(struct percpu_ref *ref)
{
	struct blkcg_gq *blkg = container_of(ref, struct blkcg_gq, refcnt);

	call_rcu(&blkg->rcu_head, __blkg_release);
}

#ifdef CONFIG_BLK_CGROUP_PUNT_BIO
static struct workqueue_struct *blkcg_punt_bio_wq;

static void blkg_async_bio_workfn(struct work_struct *work)
{
	struct blkcg_gq *blkg = container_of(work, struct blkcg_gq,
					     async_bio_work);
	struct bio_list bios = BIO_EMPTY_LIST;
	struct bio *bio;
	struct blk_plug plug;
	bool need_plug = false;

	 
	spin_lock(&blkg->async_bio_lock);
	bio_list_merge(&bios, &blkg->async_bios);
	bio_list_init(&blkg->async_bios);
	spin_unlock(&blkg->async_bio_lock);

	 
	if (bios.head && bios.head->bi_next) {
		need_plug = true;
		blk_start_plug(&plug);
	}
	while ((bio = bio_list_pop(&bios)))
		submit_bio(bio);
	if (need_plug)
		blk_finish_plug(&plug);
}

 
void blkcg_punt_bio_submit(struct bio *bio)
{
	struct blkcg_gq *blkg = bio->bi_blkg;

	if (blkg->parent) {
		spin_lock(&blkg->async_bio_lock);
		bio_list_add(&blkg->async_bios, bio);
		spin_unlock(&blkg->async_bio_lock);
		queue_work(blkcg_punt_bio_wq, &blkg->async_bio_work);
	} else {
		 
		submit_bio(bio);
	}
}
EXPORT_SYMBOL_GPL(blkcg_punt_bio_submit);

static int __init blkcg_punt_bio_init(void)
{
	blkcg_punt_bio_wq = alloc_workqueue("blkcg_punt_bio",
					    WQ_MEM_RECLAIM | WQ_FREEZABLE |
					    WQ_UNBOUND | WQ_SYSFS, 0);
	if (!blkcg_punt_bio_wq)
		return -ENOMEM;
	return 0;
}
subsys_initcall(blkcg_punt_bio_init);
#endif  

 
struct cgroup_subsys_state *bio_blkcg_css(struct bio *bio)
{
	if (!bio || !bio->bi_blkg)
		return NULL;
	return &bio->bi_blkg->blkcg->css;
}
EXPORT_SYMBOL_GPL(bio_blkcg_css);

 
static inline struct blkcg *blkcg_parent(struct blkcg *blkcg)
{
	return css_to_blkcg(blkcg->css.parent);
}

 
static struct blkcg_gq *blkg_alloc(struct blkcg *blkcg, struct gendisk *disk,
				   gfp_t gfp_mask)
{
	struct blkcg_gq *blkg;
	int i, cpu;

	 
	blkg = kzalloc_node(sizeof(*blkg), gfp_mask, disk->queue->node);
	if (!blkg)
		return NULL;
	if (percpu_ref_init(&blkg->refcnt, blkg_release, 0, gfp_mask))
		goto out_free_blkg;
	blkg->iostat_cpu = alloc_percpu_gfp(struct blkg_iostat_set, gfp_mask);
	if (!blkg->iostat_cpu)
		goto out_exit_refcnt;
	if (!blk_get_queue(disk->queue))
		goto out_free_iostat;

	blkg->q = disk->queue;
	INIT_LIST_HEAD(&blkg->q_node);
	blkg->blkcg = blkcg;
#ifdef CONFIG_BLK_CGROUP_PUNT_BIO
	spin_lock_init(&blkg->async_bio_lock);
	bio_list_init(&blkg->async_bios);
	INIT_WORK(&blkg->async_bio_work, blkg_async_bio_workfn);
#endif

	u64_stats_init(&blkg->iostat.sync);
	for_each_possible_cpu(cpu) {
		u64_stats_init(&per_cpu_ptr(blkg->iostat_cpu, cpu)->sync);
		per_cpu_ptr(blkg->iostat_cpu, cpu)->blkg = blkg;
	}

	for (i = 0; i < BLKCG_MAX_POLS; i++) {
		struct blkcg_policy *pol = blkcg_policy[i];
		struct blkg_policy_data *pd;

		if (!blkcg_policy_enabled(disk->queue, pol))
			continue;

		 
		pd = pol->pd_alloc_fn(disk, blkcg, gfp_mask);
		if (!pd)
			goto out_free_pds;
		blkg->pd[i] = pd;
		pd->blkg = blkg;
		pd->plid = i;
		pd->online = false;
	}

	return blkg;

out_free_pds:
	while (--i >= 0)
		if (blkg->pd[i])
			blkcg_policy[i]->pd_free_fn(blkg->pd[i]);
	blk_put_queue(disk->queue);
out_free_iostat:
	free_percpu(blkg->iostat_cpu);
out_exit_refcnt:
	percpu_ref_exit(&blkg->refcnt);
out_free_blkg:
	kfree(blkg);
	return NULL;
}

 
static struct blkcg_gq *blkg_create(struct blkcg *blkcg, struct gendisk *disk,
				    struct blkcg_gq *new_blkg)
{
	struct blkcg_gq *blkg;
	int i, ret;

	lockdep_assert_held(&disk->queue->queue_lock);

	 
	if (blk_queue_dying(disk->queue)) {
		ret = -ENODEV;
		goto err_free_blkg;
	}

	 
	if (!css_tryget_online(&blkcg->css)) {
		ret = -ENODEV;
		goto err_free_blkg;
	}

	 
	if (!new_blkg) {
		new_blkg = blkg_alloc(blkcg, disk, GFP_NOWAIT | __GFP_NOWARN);
		if (unlikely(!new_blkg)) {
			ret = -ENOMEM;
			goto err_put_css;
		}
	}
	blkg = new_blkg;

	 
	if (blkcg_parent(blkcg)) {
		blkg->parent = blkg_lookup(blkcg_parent(blkcg), disk->queue);
		if (WARN_ON_ONCE(!blkg->parent)) {
			ret = -ENODEV;
			goto err_put_css;
		}
		blkg_get(blkg->parent);
	}

	 
	for (i = 0; i < BLKCG_MAX_POLS; i++) {
		struct blkcg_policy *pol = blkcg_policy[i];

		if (blkg->pd[i] && pol->pd_init_fn)
			pol->pd_init_fn(blkg->pd[i]);
	}

	 
	spin_lock(&blkcg->lock);
	ret = radix_tree_insert(&blkcg->blkg_tree, disk->queue->id, blkg);
	if (likely(!ret)) {
		hlist_add_head_rcu(&blkg->blkcg_node, &blkcg->blkg_list);
		list_add(&blkg->q_node, &disk->queue->blkg_list);

		for (i = 0; i < BLKCG_MAX_POLS; i++) {
			struct blkcg_policy *pol = blkcg_policy[i];

			if (blkg->pd[i]) {
				if (pol->pd_online_fn)
					pol->pd_online_fn(blkg->pd[i]);
				blkg->pd[i]->online = true;
			}
		}
	}
	blkg->online = true;
	spin_unlock(&blkcg->lock);

	if (!ret)
		return blkg;

	 
	blkg_put(blkg);
	return ERR_PTR(ret);

err_put_css:
	css_put(&blkcg->css);
err_free_blkg:
	if (new_blkg)
		blkg_free(new_blkg);
	return ERR_PTR(ret);
}

 
static struct blkcg_gq *blkg_lookup_create(struct blkcg *blkcg,
		struct gendisk *disk)
{
	struct request_queue *q = disk->queue;
	struct blkcg_gq *blkg;
	unsigned long flags;

	WARN_ON_ONCE(!rcu_read_lock_held());

	blkg = blkg_lookup(blkcg, q);
	if (blkg)
		return blkg;

	spin_lock_irqsave(&q->queue_lock, flags);
	blkg = blkg_lookup(blkcg, q);
	if (blkg) {
		if (blkcg != &blkcg_root &&
		    blkg != rcu_dereference(blkcg->blkg_hint))
			rcu_assign_pointer(blkcg->blkg_hint, blkg);
		goto found;
	}

	 
	while (true) {
		struct blkcg *pos = blkcg;
		struct blkcg *parent = blkcg_parent(blkcg);
		struct blkcg_gq *ret_blkg = q->root_blkg;

		while (parent) {
			blkg = blkg_lookup(parent, q);
			if (blkg) {
				 
				ret_blkg = blkg;
				break;
			}
			pos = parent;
			parent = blkcg_parent(parent);
		}

		blkg = blkg_create(pos, disk, NULL);
		if (IS_ERR(blkg)) {
			blkg = ret_blkg;
			break;
		}
		if (pos == blkcg)
			break;
	}

found:
	spin_unlock_irqrestore(&q->queue_lock, flags);
	return blkg;
}

static void blkg_destroy(struct blkcg_gq *blkg)
{
	struct blkcg *blkcg = blkg->blkcg;
	int i;

	lockdep_assert_held(&blkg->q->queue_lock);
	lockdep_assert_held(&blkcg->lock);

	 
	if (hlist_unhashed(&blkg->blkcg_node))
		return;

	for (i = 0; i < BLKCG_MAX_POLS; i++) {
		struct blkcg_policy *pol = blkcg_policy[i];

		if (blkg->pd[i] && blkg->pd[i]->online) {
			blkg->pd[i]->online = false;
			if (pol->pd_offline_fn)
				pol->pd_offline_fn(blkg->pd[i]);
		}
	}

	blkg->online = false;

	radix_tree_delete(&blkcg->blkg_tree, blkg->q->id);
	hlist_del_init_rcu(&blkg->blkcg_node);

	 
	if (rcu_access_pointer(blkcg->blkg_hint) == blkg)
		rcu_assign_pointer(blkcg->blkg_hint, NULL);

	 
	percpu_ref_kill(&blkg->refcnt);
}

static void blkg_destroy_all(struct gendisk *disk)
{
	struct request_queue *q = disk->queue;
	struct blkcg_gq *blkg, *n;
	int count = BLKG_DESTROY_BATCH_SIZE;
	int i;

restart:
	spin_lock_irq(&q->queue_lock);
	list_for_each_entry_safe(blkg, n, &q->blkg_list, q_node) {
		struct blkcg *blkcg = blkg->blkcg;

		if (hlist_unhashed(&blkg->blkcg_node))
			continue;

		spin_lock(&blkcg->lock);
		blkg_destroy(blkg);
		spin_unlock(&blkcg->lock);

		 
		if (!(--count)) {
			count = BLKG_DESTROY_BATCH_SIZE;
			spin_unlock_irq(&q->queue_lock);
			cond_resched();
			goto restart;
		}
	}

	 
	for (i = 0; i < BLKCG_MAX_POLS; i++) {
		struct blkcg_policy *pol = blkcg_policy[i];

		if (pol)
			__clear_bit(pol->plid, q->blkcg_pols);
	}

	q->root_blkg = NULL;
	spin_unlock_irq(&q->queue_lock);
}

static int blkcg_reset_stats(struct cgroup_subsys_state *css,
			     struct cftype *cftype, u64 val)
{
	struct blkcg *blkcg = css_to_blkcg(css);
	struct blkcg_gq *blkg;
	int i, cpu;

	mutex_lock(&blkcg_pol_mutex);
	spin_lock_irq(&blkcg->lock);

	 
	hlist_for_each_entry(blkg, &blkcg->blkg_list, blkcg_node) {
		for_each_possible_cpu(cpu) {
			struct blkg_iostat_set *bis =
				per_cpu_ptr(blkg->iostat_cpu, cpu);
			memset(bis, 0, sizeof(*bis));

			 
			u64_stats_init(&bis->sync);
			bis->blkg = blkg;
		}
		memset(&blkg->iostat, 0, sizeof(blkg->iostat));
		u64_stats_init(&blkg->iostat.sync);

		for (i = 0; i < BLKCG_MAX_POLS; i++) {
			struct blkcg_policy *pol = blkcg_policy[i];

			if (blkg->pd[i] && pol->pd_reset_stats_fn)
				pol->pd_reset_stats_fn(blkg->pd[i]);
		}
	}

	spin_unlock_irq(&blkcg->lock);
	mutex_unlock(&blkcg_pol_mutex);
	return 0;
}

const char *blkg_dev_name(struct blkcg_gq *blkg)
{
	if (!blkg->q->disk)
		return NULL;
	return bdi_dev_name(blkg->q->disk->bdi);
}

 
void blkcg_print_blkgs(struct seq_file *sf, struct blkcg *blkcg,
		       u64 (*prfill)(struct seq_file *,
				     struct blkg_policy_data *, int),
		       const struct blkcg_policy *pol, int data,
		       bool show_total)
{
	struct blkcg_gq *blkg;
	u64 total = 0;

	rcu_read_lock();
	hlist_for_each_entry_rcu(blkg, &blkcg->blkg_list, blkcg_node) {
		spin_lock_irq(&blkg->q->queue_lock);
		if (blkcg_policy_enabled(blkg->q, pol))
			total += prfill(sf, blkg->pd[pol->plid], data);
		spin_unlock_irq(&blkg->q->queue_lock);
	}
	rcu_read_unlock();

	if (show_total)
		seq_printf(sf, "Total %llu\n", (unsigned long long)total);
}
EXPORT_SYMBOL_GPL(blkcg_print_blkgs);

 
u64 __blkg_prfill_u64(struct seq_file *sf, struct blkg_policy_data *pd, u64 v)
{
	const char *dname = blkg_dev_name(pd->blkg);

	if (!dname)
		return 0;

	seq_printf(sf, "%s %llu\n", dname, (unsigned long long)v);
	return v;
}
EXPORT_SYMBOL_GPL(__blkg_prfill_u64);

 
void blkg_conf_init(struct blkg_conf_ctx *ctx, char *input)
{
	*ctx = (struct blkg_conf_ctx){ .input = input };
}
EXPORT_SYMBOL_GPL(blkg_conf_init);

 
int blkg_conf_open_bdev(struct blkg_conf_ctx *ctx)
{
	char *input = ctx->input;
	unsigned int major, minor;
	struct block_device *bdev;
	int key_len;

	if (ctx->bdev)
		return 0;

	if (sscanf(input, "%u:%u%n", &major, &minor, &key_len) != 2)
		return -EINVAL;

	input += key_len;
	if (!isspace(*input))
		return -EINVAL;
	input = skip_spaces(input);

	bdev = blkdev_get_no_open(MKDEV(major, minor));
	if (!bdev)
		return -ENODEV;
	if (bdev_is_partition(bdev)) {
		blkdev_put_no_open(bdev);
		return -ENODEV;
	}

	mutex_lock(&bdev->bd_queue->rq_qos_mutex);
	if (!disk_live(bdev->bd_disk)) {
		blkdev_put_no_open(bdev);
		mutex_unlock(&bdev->bd_queue->rq_qos_mutex);
		return -ENODEV;
	}

	ctx->body = input;
	ctx->bdev = bdev;
	return 0;
}

 
int blkg_conf_prep(struct blkcg *blkcg, const struct blkcg_policy *pol,
		   struct blkg_conf_ctx *ctx)
	__acquires(&bdev->bd_queue->queue_lock)
{
	struct gendisk *disk;
	struct request_queue *q;
	struct blkcg_gq *blkg;
	int ret;

	ret = blkg_conf_open_bdev(ctx);
	if (ret)
		return ret;

	disk = ctx->bdev->bd_disk;
	q = disk->queue;

	 
	ret = blk_queue_enter(q, 0);
	if (ret)
		goto fail;

	spin_lock_irq(&q->queue_lock);

	if (!blkcg_policy_enabled(q, pol)) {
		ret = -EOPNOTSUPP;
		goto fail_unlock;
	}

	blkg = blkg_lookup(blkcg, q);
	if (blkg)
		goto success;

	 
	while (true) {
		struct blkcg *pos = blkcg;
		struct blkcg *parent;
		struct blkcg_gq *new_blkg;

		parent = blkcg_parent(blkcg);
		while (parent && !blkg_lookup(parent, q)) {
			pos = parent;
			parent = blkcg_parent(parent);
		}

		 
		spin_unlock_irq(&q->queue_lock);

		new_blkg = blkg_alloc(pos, disk, GFP_KERNEL);
		if (unlikely(!new_blkg)) {
			ret = -ENOMEM;
			goto fail_exit_queue;
		}

		if (radix_tree_preload(GFP_KERNEL)) {
			blkg_free(new_blkg);
			ret = -ENOMEM;
			goto fail_exit_queue;
		}

		spin_lock_irq(&q->queue_lock);

		if (!blkcg_policy_enabled(q, pol)) {
			blkg_free(new_blkg);
			ret = -EOPNOTSUPP;
			goto fail_preloaded;
		}

		blkg = blkg_lookup(pos, q);
		if (blkg) {
			blkg_free(new_blkg);
		} else {
			blkg = blkg_create(pos, disk, new_blkg);
			if (IS_ERR(blkg)) {
				ret = PTR_ERR(blkg);
				goto fail_preloaded;
			}
		}

		radix_tree_preload_end();

		if (pos == blkcg)
			goto success;
	}
success:
	blk_queue_exit(q);
	ctx->blkg = blkg;
	return 0;

fail_preloaded:
	radix_tree_preload_end();
fail_unlock:
	spin_unlock_irq(&q->queue_lock);
fail_exit_queue:
	blk_queue_exit(q);
fail:
	 
	if (ret == -EBUSY) {
		msleep(10);
		ret = restart_syscall();
	}
	return ret;
}
EXPORT_SYMBOL_GPL(blkg_conf_prep);

 
void blkg_conf_exit(struct blkg_conf_ctx *ctx)
	__releases(&ctx->bdev->bd_queue->queue_lock)
	__releases(&ctx->bdev->bd_queue->rq_qos_mutex)
{
	if (ctx->blkg) {
		spin_unlock_irq(&bdev_get_queue(ctx->bdev)->queue_lock);
		ctx->blkg = NULL;
	}

	if (ctx->bdev) {
		mutex_unlock(&ctx->bdev->bd_queue->rq_qos_mutex);
		blkdev_put_no_open(ctx->bdev);
		ctx->body = NULL;
		ctx->bdev = NULL;
	}
}
EXPORT_SYMBOL_GPL(blkg_conf_exit);

static void blkg_iostat_set(struct blkg_iostat *dst, struct blkg_iostat *src)
{
	int i;

	for (i = 0; i < BLKG_IOSTAT_NR; i++) {
		dst->bytes[i] = src->bytes[i];
		dst->ios[i] = src->ios[i];
	}
}

static void blkg_iostat_add(struct blkg_iostat *dst, struct blkg_iostat *src)
{
	int i;

	for (i = 0; i < BLKG_IOSTAT_NR; i++) {
		dst->bytes[i] += src->bytes[i];
		dst->ios[i] += src->ios[i];
	}
}

static void blkg_iostat_sub(struct blkg_iostat *dst, struct blkg_iostat *src)
{
	int i;

	for (i = 0; i < BLKG_IOSTAT_NR; i++) {
		dst->bytes[i] -= src->bytes[i];
		dst->ios[i] -= src->ios[i];
	}
}

static void blkcg_iostat_update(struct blkcg_gq *blkg, struct blkg_iostat *cur,
				struct blkg_iostat *last)
{
	struct blkg_iostat delta;
	unsigned long flags;

	 
	flags = u64_stats_update_begin_irqsave(&blkg->iostat.sync);
	blkg_iostat_set(&delta, cur);
	blkg_iostat_sub(&delta, last);
	blkg_iostat_add(&blkg->iostat.cur, &delta);
	blkg_iostat_add(last, &delta);
	u64_stats_update_end_irqrestore(&blkg->iostat.sync, flags);
}

static void __blkcg_rstat_flush(struct blkcg *blkcg, int cpu)
{
	struct llist_head *lhead = per_cpu_ptr(blkcg->lhead, cpu);
	struct llist_node *lnode;
	struct blkg_iostat_set *bisc, *next_bisc;
	unsigned long flags;

	rcu_read_lock();

	lnode = llist_del_all(lhead);
	if (!lnode)
		goto out;

	 
	raw_spin_lock_irqsave(&blkg_stat_lock, flags);

	 
	llist_for_each_entry_safe(bisc, next_bisc, lnode, lnode) {
		struct blkcg_gq *blkg = bisc->blkg;
		struct blkcg_gq *parent = blkg->parent;
		struct blkg_iostat cur;
		unsigned int seq;

		WRITE_ONCE(bisc->lqueued, false);

		 
		do {
			seq = u64_stats_fetch_begin(&bisc->sync);
			blkg_iostat_set(&cur, &bisc->cur);
		} while (u64_stats_fetch_retry(&bisc->sync, seq));

		blkcg_iostat_update(blkg, &cur, &bisc->last);

		 
		if (parent && parent->parent)
			blkcg_iostat_update(parent, &blkg->iostat.cur,
					    &blkg->iostat.last);
	}
	raw_spin_unlock_irqrestore(&blkg_stat_lock, flags);
out:
	rcu_read_unlock();
}

static void blkcg_rstat_flush(struct cgroup_subsys_state *css, int cpu)
{
	 
	if (cgroup_parent(css->cgroup))
		__blkcg_rstat_flush(css_to_blkcg(css), cpu);
}

 
static void blkcg_fill_root_iostats(void)
{
	struct class_dev_iter iter;
	struct device *dev;

	class_dev_iter_init(&iter, &block_class, NULL, &disk_type);
	while ((dev = class_dev_iter_next(&iter))) {
		struct block_device *bdev = dev_to_bdev(dev);
		struct blkcg_gq *blkg = bdev->bd_disk->queue->root_blkg;
		struct blkg_iostat tmp;
		int cpu;
		unsigned long flags;

		memset(&tmp, 0, sizeof(tmp));
		for_each_possible_cpu(cpu) {
			struct disk_stats *cpu_dkstats;

			cpu_dkstats = per_cpu_ptr(bdev->bd_stats, cpu);
			tmp.ios[BLKG_IOSTAT_READ] +=
				cpu_dkstats->ios[STAT_READ];
			tmp.ios[BLKG_IOSTAT_WRITE] +=
				cpu_dkstats->ios[STAT_WRITE];
			tmp.ios[BLKG_IOSTAT_DISCARD] +=
				cpu_dkstats->ios[STAT_DISCARD];
			
			tmp.bytes[BLKG_IOSTAT_READ] +=
				cpu_dkstats->sectors[STAT_READ] << 9;
			tmp.bytes[BLKG_IOSTAT_WRITE] +=
				cpu_dkstats->sectors[STAT_WRITE] << 9;
			tmp.bytes[BLKG_IOSTAT_DISCARD] +=
				cpu_dkstats->sectors[STAT_DISCARD] << 9;
		}

		flags = u64_stats_update_begin_irqsave(&blkg->iostat.sync);
		blkg_iostat_set(&blkg->iostat.cur, &tmp);
		u64_stats_update_end_irqrestore(&blkg->iostat.sync, flags);
	}
}

static void blkcg_print_one_stat(struct blkcg_gq *blkg, struct seq_file *s)
{
	struct blkg_iostat_set *bis = &blkg->iostat;
	u64 rbytes, wbytes, rios, wios, dbytes, dios;
	const char *dname;
	unsigned seq;
	int i;

	if (!blkg->online)
		return;

	dname = blkg_dev_name(blkg);
	if (!dname)
		return;

	seq_printf(s, "%s ", dname);

	do {
		seq = u64_stats_fetch_begin(&bis->sync);

		rbytes = bis->cur.bytes[BLKG_IOSTAT_READ];
		wbytes = bis->cur.bytes[BLKG_IOSTAT_WRITE];
		dbytes = bis->cur.bytes[BLKG_IOSTAT_DISCARD];
		rios = bis->cur.ios[BLKG_IOSTAT_READ];
		wios = bis->cur.ios[BLKG_IOSTAT_WRITE];
		dios = bis->cur.ios[BLKG_IOSTAT_DISCARD];
	} while (u64_stats_fetch_retry(&bis->sync, seq));

	if (rbytes || wbytes || rios || wios) {
		seq_printf(s, "rbytes=%llu wbytes=%llu rios=%llu wios=%llu dbytes=%llu dios=%llu",
			rbytes, wbytes, rios, wios,
			dbytes, dios);
	}

	if (blkcg_debug_stats && atomic_read(&blkg->use_delay)) {
		seq_printf(s, " use_delay=%d delay_nsec=%llu",
			atomic_read(&blkg->use_delay),
			atomic64_read(&blkg->delay_nsec));
	}

	for (i = 0; i < BLKCG_MAX_POLS; i++) {
		struct blkcg_policy *pol = blkcg_policy[i];

		if (!blkg->pd[i] || !pol->pd_stat_fn)
			continue;

		pol->pd_stat_fn(blkg->pd[i], s);
	}

	seq_puts(s, "\n");
}

static int blkcg_print_stat(struct seq_file *sf, void *v)
{
	struct blkcg *blkcg = css_to_blkcg(seq_css(sf));
	struct blkcg_gq *blkg;

	if (!seq_css(sf)->parent)
		blkcg_fill_root_iostats();
	else
		cgroup_rstat_flush(blkcg->css.cgroup);

	rcu_read_lock();
	hlist_for_each_entry_rcu(blkg, &blkcg->blkg_list, blkcg_node) {
		spin_lock_irq(&blkg->q->queue_lock);
		blkcg_print_one_stat(blkg, sf);
		spin_unlock_irq(&blkg->q->queue_lock);
	}
	rcu_read_unlock();
	return 0;
}

static struct cftype blkcg_files[] = {
	{
		.name = "stat",
		.seq_show = blkcg_print_stat,
	},
	{ }	 
};

static struct cftype blkcg_legacy_files[] = {
	{
		.name = "reset_stats",
		.write_u64 = blkcg_reset_stats,
	},
	{ }	 
};

#ifdef CONFIG_CGROUP_WRITEBACK
struct list_head *blkcg_get_cgwb_list(struct cgroup_subsys_state *css)
{
	return &css_to_blkcg(css)->cgwb_list;
}
#endif

 

 
static void blkcg_destroy_blkgs(struct blkcg *blkcg)
{
	might_sleep();

	spin_lock_irq(&blkcg->lock);

	while (!hlist_empty(&blkcg->blkg_list)) {
		struct blkcg_gq *blkg = hlist_entry(blkcg->blkg_list.first,
						struct blkcg_gq, blkcg_node);
		struct request_queue *q = blkg->q;

		if (need_resched() || !spin_trylock(&q->queue_lock)) {
			 
			spin_unlock_irq(&blkcg->lock);
			cond_resched();
			spin_lock_irq(&blkcg->lock);
			continue;
		}

		blkg_destroy(blkg);
		spin_unlock(&q->queue_lock);
	}

	spin_unlock_irq(&blkcg->lock);
}

 
void blkcg_pin_online(struct cgroup_subsys_state *blkcg_css)
{
	refcount_inc(&css_to_blkcg(blkcg_css)->online_pin);
}

 
void blkcg_unpin_online(struct cgroup_subsys_state *blkcg_css)
{
	struct blkcg *blkcg = css_to_blkcg(blkcg_css);

	do {
		if (!refcount_dec_and_test(&blkcg->online_pin))
			break;
		blkcg_destroy_blkgs(blkcg);
		blkcg = blkcg_parent(blkcg);
	} while (blkcg);
}

 
static void blkcg_css_offline(struct cgroup_subsys_state *css)
{
	 
	wb_blkcg_offline(css);

	 
	blkcg_unpin_online(css);
}

static void blkcg_css_free(struct cgroup_subsys_state *css)
{
	struct blkcg *blkcg = css_to_blkcg(css);
	int i;

	mutex_lock(&blkcg_pol_mutex);

	list_del(&blkcg->all_blkcgs_node);

	for (i = 0; i < BLKCG_MAX_POLS; i++)
		if (blkcg->cpd[i])
			blkcg_policy[i]->cpd_free_fn(blkcg->cpd[i]);

	mutex_unlock(&blkcg_pol_mutex);

	free_percpu(blkcg->lhead);
	kfree(blkcg);
}

static struct cgroup_subsys_state *
blkcg_css_alloc(struct cgroup_subsys_state *parent_css)
{
	struct blkcg *blkcg;
	int i;

	mutex_lock(&blkcg_pol_mutex);

	if (!parent_css) {
		blkcg = &blkcg_root;
	} else {
		blkcg = kzalloc(sizeof(*blkcg), GFP_KERNEL);
		if (!blkcg)
			goto unlock;
	}

	if (init_blkcg_llists(blkcg))
		goto free_blkcg;

	for (i = 0; i < BLKCG_MAX_POLS ; i++) {
		struct blkcg_policy *pol = blkcg_policy[i];
		struct blkcg_policy_data *cpd;

		 
		if (!pol || !pol->cpd_alloc_fn)
			continue;

		cpd = pol->cpd_alloc_fn(GFP_KERNEL);
		if (!cpd)
			goto free_pd_blkcg;

		blkcg->cpd[i] = cpd;
		cpd->blkcg = blkcg;
		cpd->plid = i;
	}

	spin_lock_init(&blkcg->lock);
	refcount_set(&blkcg->online_pin, 1);
	INIT_RADIX_TREE(&blkcg->blkg_tree, GFP_NOWAIT | __GFP_NOWARN);
	INIT_HLIST_HEAD(&blkcg->blkg_list);
#ifdef CONFIG_CGROUP_WRITEBACK
	INIT_LIST_HEAD(&blkcg->cgwb_list);
#endif
	list_add_tail(&blkcg->all_blkcgs_node, &all_blkcgs);

	mutex_unlock(&blkcg_pol_mutex);
	return &blkcg->css;

free_pd_blkcg:
	for (i--; i >= 0; i--)
		if (blkcg->cpd[i])
			blkcg_policy[i]->cpd_free_fn(blkcg->cpd[i]);
	free_percpu(blkcg->lhead);
free_blkcg:
	if (blkcg != &blkcg_root)
		kfree(blkcg);
unlock:
	mutex_unlock(&blkcg_pol_mutex);
	return ERR_PTR(-ENOMEM);
}

static int blkcg_css_online(struct cgroup_subsys_state *css)
{
	struct blkcg *parent = blkcg_parent(css_to_blkcg(css));

	 
	if (parent)
		blkcg_pin_online(&parent->css);
	return 0;
}

int blkcg_init_disk(struct gendisk *disk)
{
	struct request_queue *q = disk->queue;
	struct blkcg_gq *new_blkg, *blkg;
	bool preloaded;
	int ret;

	INIT_LIST_HEAD(&q->blkg_list);
	mutex_init(&q->blkcg_mutex);

	new_blkg = blkg_alloc(&blkcg_root, disk, GFP_KERNEL);
	if (!new_blkg)
		return -ENOMEM;

	preloaded = !radix_tree_preload(GFP_KERNEL);

	 
	 
	spin_lock_irq(&q->queue_lock);
	blkg = blkg_create(&blkcg_root, disk, new_blkg);
	if (IS_ERR(blkg))
		goto err_unlock;
	q->root_blkg = blkg;
	spin_unlock_irq(&q->queue_lock);

	if (preloaded)
		radix_tree_preload_end();

	ret = blk_ioprio_init(disk);
	if (ret)
		goto err_destroy_all;

	ret = blk_throtl_init(disk);
	if (ret)
		goto err_ioprio_exit;

	return 0;

err_ioprio_exit:
	blk_ioprio_exit(disk);
err_destroy_all:
	blkg_destroy_all(disk);
	return ret;
err_unlock:
	spin_unlock_irq(&q->queue_lock);
	if (preloaded)
		radix_tree_preload_end();
	return PTR_ERR(blkg);
}

void blkcg_exit_disk(struct gendisk *disk)
{
	blkg_destroy_all(disk);
	blk_throtl_exit(disk);
}

static void blkcg_exit(struct task_struct *tsk)
{
	if (tsk->throttle_disk)
		put_disk(tsk->throttle_disk);
	tsk->throttle_disk = NULL;
}

struct cgroup_subsys io_cgrp_subsys = {
	.css_alloc = blkcg_css_alloc,
	.css_online = blkcg_css_online,
	.css_offline = blkcg_css_offline,
	.css_free = blkcg_css_free,
	.css_rstat_flush = blkcg_rstat_flush,
	.dfl_cftypes = blkcg_files,
	.legacy_cftypes = blkcg_legacy_files,
	.legacy_name = "blkio",
	.exit = blkcg_exit,
#ifdef CONFIG_MEMCG
	 
	.depends_on = 1 << memory_cgrp_id,
#endif
};
EXPORT_SYMBOL_GPL(io_cgrp_subsys);

 
int blkcg_activate_policy(struct gendisk *disk, const struct blkcg_policy *pol)
{
	struct request_queue *q = disk->queue;
	struct blkg_policy_data *pd_prealloc = NULL;
	struct blkcg_gq *blkg, *pinned_blkg = NULL;
	int ret;

	if (blkcg_policy_enabled(q, pol))
		return 0;

	if (queue_is_mq(q))
		blk_mq_freeze_queue(q);
retry:
	spin_lock_irq(&q->queue_lock);

	 
	list_for_each_entry_reverse(blkg, &q->blkg_list, q_node) {
		struct blkg_policy_data *pd;

		if (blkg->pd[pol->plid])
			continue;

		 
		if (blkg == pinned_blkg) {
			pd = pd_prealloc;
			pd_prealloc = NULL;
		} else {
			pd = pol->pd_alloc_fn(disk, blkg->blkcg,
					      GFP_NOWAIT | __GFP_NOWARN);
		}

		if (!pd) {
			 
			if (pinned_blkg)
				blkg_put(pinned_blkg);
			blkg_get(blkg);
			pinned_blkg = blkg;

			spin_unlock_irq(&q->queue_lock);

			if (pd_prealloc)
				pol->pd_free_fn(pd_prealloc);
			pd_prealloc = pol->pd_alloc_fn(disk, blkg->blkcg,
						       GFP_KERNEL);
			if (pd_prealloc)
				goto retry;
			else
				goto enomem;
		}

		spin_lock(&blkg->blkcg->lock);

		pd->blkg = blkg;
		pd->plid = pol->plid;
		blkg->pd[pol->plid] = pd;

		if (pol->pd_init_fn)
			pol->pd_init_fn(pd);

		if (pol->pd_online_fn)
			pol->pd_online_fn(pd);
		pd->online = true;

		spin_unlock(&blkg->blkcg->lock);
	}

	__set_bit(pol->plid, q->blkcg_pols);
	ret = 0;

	spin_unlock_irq(&q->queue_lock);
out:
	if (queue_is_mq(q))
		blk_mq_unfreeze_queue(q);
	if (pinned_blkg)
		blkg_put(pinned_blkg);
	if (pd_prealloc)
		pol->pd_free_fn(pd_prealloc);
	return ret;

enomem:
	 
	spin_lock_irq(&q->queue_lock);
	list_for_each_entry(blkg, &q->blkg_list, q_node) {
		struct blkcg *blkcg = blkg->blkcg;
		struct blkg_policy_data *pd;

		spin_lock(&blkcg->lock);
		pd = blkg->pd[pol->plid];
		if (pd) {
			if (pd->online && pol->pd_offline_fn)
				pol->pd_offline_fn(pd);
			pd->online = false;
			pol->pd_free_fn(pd);
			blkg->pd[pol->plid] = NULL;
		}
		spin_unlock(&blkcg->lock);
	}
	spin_unlock_irq(&q->queue_lock);
	ret = -ENOMEM;
	goto out;
}
EXPORT_SYMBOL_GPL(blkcg_activate_policy);

 
void blkcg_deactivate_policy(struct gendisk *disk,
			     const struct blkcg_policy *pol)
{
	struct request_queue *q = disk->queue;
	struct blkcg_gq *blkg;

	if (!blkcg_policy_enabled(q, pol))
		return;

	if (queue_is_mq(q))
		blk_mq_freeze_queue(q);

	mutex_lock(&q->blkcg_mutex);
	spin_lock_irq(&q->queue_lock);

	__clear_bit(pol->plid, q->blkcg_pols);

	list_for_each_entry(blkg, &q->blkg_list, q_node) {
		struct blkcg *blkcg = blkg->blkcg;

		spin_lock(&blkcg->lock);
		if (blkg->pd[pol->plid]) {
			if (blkg->pd[pol->plid]->online && pol->pd_offline_fn)
				pol->pd_offline_fn(blkg->pd[pol->plid]);
			pol->pd_free_fn(blkg->pd[pol->plid]);
			blkg->pd[pol->plid] = NULL;
		}
		spin_unlock(&blkcg->lock);
	}

	spin_unlock_irq(&q->queue_lock);
	mutex_unlock(&q->blkcg_mutex);

	if (queue_is_mq(q))
		blk_mq_unfreeze_queue(q);
}
EXPORT_SYMBOL_GPL(blkcg_deactivate_policy);

static void blkcg_free_all_cpd(struct blkcg_policy *pol)
{
	struct blkcg *blkcg;

	list_for_each_entry(blkcg, &all_blkcgs, all_blkcgs_node) {
		if (blkcg->cpd[pol->plid]) {
			pol->cpd_free_fn(blkcg->cpd[pol->plid]);
			blkcg->cpd[pol->plid] = NULL;
		}
	}
}

 
int blkcg_policy_register(struct blkcg_policy *pol)
{
	struct blkcg *blkcg;
	int i, ret;

	mutex_lock(&blkcg_pol_register_mutex);
	mutex_lock(&blkcg_pol_mutex);

	 
	ret = -ENOSPC;
	for (i = 0; i < BLKCG_MAX_POLS; i++)
		if (!blkcg_policy[i])
			break;
	if (i >= BLKCG_MAX_POLS) {
		pr_warn("blkcg_policy_register: BLKCG_MAX_POLS too small\n");
		goto err_unlock;
	}

	 
	if ((!pol->cpd_alloc_fn ^ !pol->cpd_free_fn) ||
		(!pol->pd_alloc_fn ^ !pol->pd_free_fn))
		goto err_unlock;

	 
	pol->plid = i;
	blkcg_policy[pol->plid] = pol;

	 
	if (pol->cpd_alloc_fn) {
		list_for_each_entry(blkcg, &all_blkcgs, all_blkcgs_node) {
			struct blkcg_policy_data *cpd;

			cpd = pol->cpd_alloc_fn(GFP_KERNEL);
			if (!cpd)
				goto err_free_cpds;

			blkcg->cpd[pol->plid] = cpd;
			cpd->blkcg = blkcg;
			cpd->plid = pol->plid;
		}
	}

	mutex_unlock(&blkcg_pol_mutex);

	 
	if (pol->dfl_cftypes)
		WARN_ON(cgroup_add_dfl_cftypes(&io_cgrp_subsys,
					       pol->dfl_cftypes));
	if (pol->legacy_cftypes)
		WARN_ON(cgroup_add_legacy_cftypes(&io_cgrp_subsys,
						  pol->legacy_cftypes));
	mutex_unlock(&blkcg_pol_register_mutex);
	return 0;

err_free_cpds:
	if (pol->cpd_free_fn)
		blkcg_free_all_cpd(pol);

	blkcg_policy[pol->plid] = NULL;
err_unlock:
	mutex_unlock(&blkcg_pol_mutex);
	mutex_unlock(&blkcg_pol_register_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(blkcg_policy_register);

 
void blkcg_policy_unregister(struct blkcg_policy *pol)
{
	mutex_lock(&blkcg_pol_register_mutex);

	if (WARN_ON(blkcg_policy[pol->plid] != pol))
		goto out_unlock;

	 
	if (pol->dfl_cftypes)
		cgroup_rm_cftypes(pol->dfl_cftypes);
	if (pol->legacy_cftypes)
		cgroup_rm_cftypes(pol->legacy_cftypes);

	 
	mutex_lock(&blkcg_pol_mutex);

	if (pol->cpd_free_fn)
		blkcg_free_all_cpd(pol);

	blkcg_policy[pol->plid] = NULL;

	mutex_unlock(&blkcg_pol_mutex);
out_unlock:
	mutex_unlock(&blkcg_pol_register_mutex);
}
EXPORT_SYMBOL_GPL(blkcg_policy_unregister);

 
static void blkcg_scale_delay(struct blkcg_gq *blkg, u64 now)
{
	u64 old = atomic64_read(&blkg->delay_start);

	 
	if (atomic_read(&blkg->use_delay) < 0)
		return;

	 
	if (time_before64(old + NSEC_PER_SEC, now) &&
	    atomic64_try_cmpxchg(&blkg->delay_start, &old, now)) {
		u64 cur = atomic64_read(&blkg->delay_nsec);
		u64 sub = min_t(u64, blkg->last_delay, now - old);
		int cur_use = atomic_read(&blkg->use_delay);

		 
		if (cur_use < blkg->last_use)
			sub = max_t(u64, sub, blkg->last_delay >> 1);

		 
		if (unlikely(cur < sub)) {
			atomic64_set(&blkg->delay_nsec, 0);
			blkg->last_delay = 0;
		} else {
			atomic64_sub(sub, &blkg->delay_nsec);
			blkg->last_delay = cur - sub;
		}
		blkg->last_use = cur_use;
	}
}

 
static void blkcg_maybe_throttle_blkg(struct blkcg_gq *blkg, bool use_memdelay)
{
	unsigned long pflags;
	bool clamp;
	u64 now = ktime_to_ns(ktime_get());
	u64 exp;
	u64 delay_nsec = 0;
	int tok;

	while (blkg->parent) {
		int use_delay = atomic_read(&blkg->use_delay);

		if (use_delay) {
			u64 this_delay;

			blkcg_scale_delay(blkg, now);
			this_delay = atomic64_read(&blkg->delay_nsec);
			if (this_delay > delay_nsec) {
				delay_nsec = this_delay;
				clamp = use_delay > 0;
			}
		}
		blkg = blkg->parent;
	}

	if (!delay_nsec)
		return;

	 
	if (clamp)
		delay_nsec = min_t(u64, delay_nsec, 250 * NSEC_PER_MSEC);

	if (use_memdelay)
		psi_memstall_enter(&pflags);

	exp = ktime_add_ns(now, delay_nsec);
	tok = io_schedule_prepare();
	do {
		__set_current_state(TASK_KILLABLE);
		if (!schedule_hrtimeout(&exp, HRTIMER_MODE_ABS))
			break;
	} while (!fatal_signal_pending(current));
	io_schedule_finish(tok);

	if (use_memdelay)
		psi_memstall_leave(&pflags);
}

 
void blkcg_maybe_throttle_current(void)
{
	struct gendisk *disk = current->throttle_disk;
	struct blkcg *blkcg;
	struct blkcg_gq *blkg;
	bool use_memdelay = current->use_memdelay;

	if (!disk)
		return;

	current->throttle_disk = NULL;
	current->use_memdelay = false;

	rcu_read_lock();
	blkcg = css_to_blkcg(blkcg_css());
	if (!blkcg)
		goto out;
	blkg = blkg_lookup(blkcg, disk->queue);
	if (!blkg)
		goto out;
	if (!blkg_tryget(blkg))
		goto out;
	rcu_read_unlock();

	blkcg_maybe_throttle_blkg(blkg, use_memdelay);
	blkg_put(blkg);
	put_disk(disk);
	return;
out:
	rcu_read_unlock();
}

 
void blkcg_schedule_throttle(struct gendisk *disk, bool use_memdelay)
{
	if (unlikely(current->flags & PF_KTHREAD))
		return;

	if (current->throttle_disk != disk) {
		if (test_bit(GD_DEAD, &disk->state))
			return;
		get_device(disk_to_dev(disk));

		if (current->throttle_disk)
			put_disk(current->throttle_disk);
		current->throttle_disk = disk;
	}

	if (use_memdelay)
		current->use_memdelay = use_memdelay;
	set_notify_resume(current);
}

 
void blkcg_add_delay(struct blkcg_gq *blkg, u64 now, u64 delta)
{
	if (WARN_ON_ONCE(atomic_read(&blkg->use_delay) < 0))
		return;
	blkcg_scale_delay(blkg, now);
	atomic64_add(delta, &blkg->delay_nsec);
}

 
static inline struct blkcg_gq *blkg_tryget_closest(struct bio *bio,
		struct cgroup_subsys_state *css)
{
	struct blkcg_gq *blkg, *ret_blkg = NULL;

	rcu_read_lock();
	blkg = blkg_lookup_create(css_to_blkcg(css), bio->bi_bdev->bd_disk);
	while (blkg) {
		if (blkg_tryget(blkg)) {
			ret_blkg = blkg;
			break;
		}
		blkg = blkg->parent;
	}
	rcu_read_unlock();

	return ret_blkg;
}

 
void bio_associate_blkg_from_css(struct bio *bio,
				 struct cgroup_subsys_state *css)
{
	if (bio->bi_blkg)
		blkg_put(bio->bi_blkg);

	if (css && css->parent) {
		bio->bi_blkg = blkg_tryget_closest(bio, css);
	} else {
		blkg_get(bdev_get_queue(bio->bi_bdev)->root_blkg);
		bio->bi_blkg = bdev_get_queue(bio->bi_bdev)->root_blkg;
	}
}
EXPORT_SYMBOL_GPL(bio_associate_blkg_from_css);

 
void bio_associate_blkg(struct bio *bio)
{
	struct cgroup_subsys_state *css;

	rcu_read_lock();

	if (bio->bi_blkg)
		css = bio_blkcg_css(bio);
	else
		css = blkcg_css();

	bio_associate_blkg_from_css(bio, css);

	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(bio_associate_blkg);

 
void bio_clone_blkg_association(struct bio *dst, struct bio *src)
{
	if (src->bi_blkg)
		bio_associate_blkg_from_css(dst, bio_blkcg_css(src));
}
EXPORT_SYMBOL_GPL(bio_clone_blkg_association);

static int blk_cgroup_io_type(struct bio *bio)
{
	if (op_is_discard(bio->bi_opf))
		return BLKG_IOSTAT_DISCARD;
	if (op_is_write(bio->bi_opf))
		return BLKG_IOSTAT_WRITE;
	return BLKG_IOSTAT_READ;
}

void blk_cgroup_bio_start(struct bio *bio)
{
	struct blkcg *blkcg = bio->bi_blkg->blkcg;
	int rwd = blk_cgroup_io_type(bio), cpu;
	struct blkg_iostat_set *bis;
	unsigned long flags;

	if (!cgroup_subsys_on_dfl(io_cgrp_subsys))
		return;

	 
	if (!cgroup_parent(blkcg->css.cgroup))
		return;

	cpu = get_cpu();
	bis = per_cpu_ptr(bio->bi_blkg->iostat_cpu, cpu);
	flags = u64_stats_update_begin_irqsave(&bis->sync);

	 
	if (!bio_flagged(bio, BIO_CGROUP_ACCT)) {
		bio_set_flag(bio, BIO_CGROUP_ACCT);
		bis->cur.bytes[rwd] += bio->bi_iter.bi_size;
	}
	bis->cur.ios[rwd]++;

	 
	if (!READ_ONCE(bis->lqueued)) {
		struct llist_head *lhead = this_cpu_ptr(blkcg->lhead);

		llist_add(&bis->lnode, lhead);
		WRITE_ONCE(bis->lqueued, true);
	}

	u64_stats_update_end_irqrestore(&bis->sync, flags);
	cgroup_rstat_updated(blkcg->css.cgroup, cpu);
	put_cpu();
}

bool blk_cgroup_congested(void)
{
	struct cgroup_subsys_state *css;
	bool ret = false;

	rcu_read_lock();
	for (css = blkcg_css(); css; css = css->parent) {
		if (atomic_read(&css->cgroup->congestion_count)) {
			ret = true;
			break;
		}
	}
	rcu_read_unlock();
	return ret;
}

module_param(blkcg_debug_stats, bool, 0644);
MODULE_PARM_DESC(blkcg_debug_stats, "True if you want debug stats, false if not");
