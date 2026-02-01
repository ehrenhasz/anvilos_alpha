 
#ifndef _BLK_CGROUP_PRIVATE_H
#define _BLK_CGROUP_PRIVATE_H
 

#include <linux/blk-cgroup.h>
#include <linux/cgroup.h>
#include <linux/kthread.h>
#include <linux/blk-mq.h>
#include <linux/llist.h>

struct blkcg_gq;
struct blkg_policy_data;


 
#define BLKG_STAT_CPU_BATCH	(INT_MAX / 2)

#ifdef CONFIG_BLK_CGROUP

enum blkg_iostat_type {
	BLKG_IOSTAT_READ,
	BLKG_IOSTAT_WRITE,
	BLKG_IOSTAT_DISCARD,

	BLKG_IOSTAT_NR,
};

struct blkg_iostat {
	u64				bytes[BLKG_IOSTAT_NR];
	u64				ios[BLKG_IOSTAT_NR];
};

struct blkg_iostat_set {
	struct u64_stats_sync		sync;
	struct blkcg_gq		       *blkg;
	struct llist_node		lnode;
	int				lqueued;	 
	struct blkg_iostat		cur;
	struct blkg_iostat		last;
};

 
struct blkcg_gq {
	 
	struct request_queue		*q;
	struct list_head		q_node;
	struct hlist_node		blkcg_node;
	struct blkcg			*blkcg;

	 
	struct blkcg_gq			*parent;

	 
	struct percpu_ref		refcnt;

	 
	bool				online;

	struct blkg_iostat_set __percpu	*iostat_cpu;
	struct blkg_iostat_set		iostat;

	struct blkg_policy_data		*pd[BLKCG_MAX_POLS];
#ifdef CONFIG_BLK_CGROUP_PUNT_BIO
	spinlock_t			async_bio_lock;
	struct bio_list			async_bios;
#endif
	union {
		struct work_struct	async_bio_work;
		struct work_struct	free_work;
	};

	atomic_t			use_delay;
	atomic64_t			delay_nsec;
	atomic64_t			delay_start;
	u64				last_delay;
	int				last_use;

	struct rcu_head			rcu_head;
};

struct blkcg {
	struct cgroup_subsys_state	css;
	spinlock_t			lock;
	refcount_t			online_pin;

	struct radix_tree_root		blkg_tree;
	struct blkcg_gq	__rcu		*blkg_hint;
	struct hlist_head		blkg_list;

	struct blkcg_policy_data	*cpd[BLKCG_MAX_POLS];

	struct list_head		all_blkcgs_node;

	 
	struct llist_head __percpu	*lhead;

#ifdef CONFIG_BLK_CGROUP_FC_APPID
	char                            fc_app_id[FC_APPID_LEN];
#endif
#ifdef CONFIG_CGROUP_WRITEBACK
	struct list_head		cgwb_list;
#endif
};

static inline struct blkcg *css_to_blkcg(struct cgroup_subsys_state *css)
{
	return css ? container_of(css, struct blkcg, css) : NULL;
}

 
struct blkg_policy_data {
	 
	struct blkcg_gq			*blkg;
	int				plid;
	bool				online;
};

 
struct blkcg_policy_data {
	 
	struct blkcg			*blkcg;
	int				plid;
};

typedef struct blkcg_policy_data *(blkcg_pol_alloc_cpd_fn)(gfp_t gfp);
typedef void (blkcg_pol_init_cpd_fn)(struct blkcg_policy_data *cpd);
typedef void (blkcg_pol_free_cpd_fn)(struct blkcg_policy_data *cpd);
typedef void (blkcg_pol_bind_cpd_fn)(struct blkcg_policy_data *cpd);
typedef struct blkg_policy_data *(blkcg_pol_alloc_pd_fn)(struct gendisk *disk,
		struct blkcg *blkcg, gfp_t gfp);
typedef void (blkcg_pol_init_pd_fn)(struct blkg_policy_data *pd);
typedef void (blkcg_pol_online_pd_fn)(struct blkg_policy_data *pd);
typedef void (blkcg_pol_offline_pd_fn)(struct blkg_policy_data *pd);
typedef void (blkcg_pol_free_pd_fn)(struct blkg_policy_data *pd);
typedef void (blkcg_pol_reset_pd_stats_fn)(struct blkg_policy_data *pd);
typedef void (blkcg_pol_stat_pd_fn)(struct blkg_policy_data *pd,
				struct seq_file *s);

struct blkcg_policy {
	int				plid;
	 
	struct cftype			*dfl_cftypes;
	struct cftype			*legacy_cftypes;

	 
	blkcg_pol_alloc_cpd_fn		*cpd_alloc_fn;
	blkcg_pol_free_cpd_fn		*cpd_free_fn;

	blkcg_pol_alloc_pd_fn		*pd_alloc_fn;
	blkcg_pol_init_pd_fn		*pd_init_fn;
	blkcg_pol_online_pd_fn		*pd_online_fn;
	blkcg_pol_offline_pd_fn		*pd_offline_fn;
	blkcg_pol_free_pd_fn		*pd_free_fn;
	blkcg_pol_reset_pd_stats_fn	*pd_reset_stats_fn;
	blkcg_pol_stat_pd_fn		*pd_stat_fn;
};

extern struct blkcg blkcg_root;
extern bool blkcg_debug_stats;

int blkcg_init_disk(struct gendisk *disk);
void blkcg_exit_disk(struct gendisk *disk);

 
int blkcg_policy_register(struct blkcg_policy *pol);
void blkcg_policy_unregister(struct blkcg_policy *pol);
int blkcg_activate_policy(struct gendisk *disk, const struct blkcg_policy *pol);
void blkcg_deactivate_policy(struct gendisk *disk,
			     const struct blkcg_policy *pol);

const char *blkg_dev_name(struct blkcg_gq *blkg);
void blkcg_print_blkgs(struct seq_file *sf, struct blkcg *blkcg,
		       u64 (*prfill)(struct seq_file *,
				     struct blkg_policy_data *, int),
		       const struct blkcg_policy *pol, int data,
		       bool show_total);
u64 __blkg_prfill_u64(struct seq_file *sf, struct blkg_policy_data *pd, u64 v);

struct blkg_conf_ctx {
	char				*input;
	char				*body;
	struct block_device		*bdev;
	struct blkcg_gq			*blkg;
};

void blkg_conf_init(struct blkg_conf_ctx *ctx, char *input);
int blkg_conf_open_bdev(struct blkg_conf_ctx *ctx);
int blkg_conf_prep(struct blkcg *blkcg, const struct blkcg_policy *pol,
		   struct blkg_conf_ctx *ctx);
void blkg_conf_exit(struct blkg_conf_ctx *ctx);

 
static inline bool bio_issue_as_root_blkg(struct bio *bio)
{
	return (bio->bi_opf & (REQ_META | REQ_SWAP)) != 0;
}

 
static inline struct blkcg_gq *blkg_lookup(struct blkcg *blkcg,
					   struct request_queue *q)
{
	struct blkcg_gq *blkg;

	if (blkcg == &blkcg_root)
		return q->root_blkg;

	blkg = rcu_dereference_check(blkcg->blkg_hint,
			lockdep_is_held(&q->queue_lock));
	if (blkg && blkg->q == q)
		return blkg;

	blkg = radix_tree_lookup(&blkcg->blkg_tree, q->id);
	if (blkg && blkg->q != q)
		blkg = NULL;
	return blkg;
}

 
static inline struct blkg_policy_data *blkg_to_pd(struct blkcg_gq *blkg,
						  struct blkcg_policy *pol)
{
	return blkg ? blkg->pd[pol->plid] : NULL;
}

static inline struct blkcg_policy_data *blkcg_to_cpd(struct blkcg *blkcg,
						     struct blkcg_policy *pol)
{
	return blkcg ? blkcg->cpd[pol->plid] : NULL;
}

 
static inline struct blkcg_gq *pd_to_blkg(struct blkg_policy_data *pd)
{
	return pd ? pd->blkg : NULL;
}

static inline struct blkcg *cpd_to_blkcg(struct blkcg_policy_data *cpd)
{
	return cpd ? cpd->blkcg : NULL;
}

 
static inline int blkg_path(struct blkcg_gq *blkg, char *buf, int buflen)
{
	return cgroup_path(blkg->blkcg->css.cgroup, buf, buflen);
}

 
static inline void blkg_get(struct blkcg_gq *blkg)
{
	percpu_ref_get(&blkg->refcnt);
}

 
static inline bool blkg_tryget(struct blkcg_gq *blkg)
{
	return blkg && percpu_ref_tryget(&blkg->refcnt);
}

 
static inline void blkg_put(struct blkcg_gq *blkg)
{
	percpu_ref_put(&blkg->refcnt);
}

 
#define blkg_for_each_descendant_pre(d_blkg, pos_css, p_blkg)		\
	css_for_each_descendant_pre((pos_css), &(p_blkg)->blkcg->css)	\
		if (((d_blkg) = blkg_lookup(css_to_blkcg(pos_css),	\
					    (p_blkg)->q)))

 
#define blkg_for_each_descendant_post(d_blkg, pos_css, p_blkg)		\
	css_for_each_descendant_post((pos_css), &(p_blkg)->blkcg->css)	\
		if (((d_blkg) = blkg_lookup(css_to_blkcg(pos_css),	\
					    (p_blkg)->q)))

static inline void blkcg_bio_issue_init(struct bio *bio)
{
	bio_issue_init(&bio->bi_issue, bio_sectors(bio));
}

static inline void blkcg_use_delay(struct blkcg_gq *blkg)
{
	if (WARN_ON_ONCE(atomic_read(&blkg->use_delay) < 0))
		return;
	if (atomic_add_return(1, &blkg->use_delay) == 1)
		atomic_inc(&blkg->blkcg->css.cgroup->congestion_count);
}

static inline int blkcg_unuse_delay(struct blkcg_gq *blkg)
{
	int old = atomic_read(&blkg->use_delay);

	if (WARN_ON_ONCE(old < 0))
		return 0;
	if (old == 0)
		return 0;

	 
	while (old && !atomic_try_cmpxchg(&blkg->use_delay, &old, old - 1))
		;

	if (old == 0)
		return 0;
	if (old == 1)
		atomic_dec(&blkg->blkcg->css.cgroup->congestion_count);
	return 1;
}

 
static inline void blkcg_set_delay(struct blkcg_gq *blkg, u64 delay)
{
	int old = atomic_read(&blkg->use_delay);

	 
	if (!old && atomic_try_cmpxchg(&blkg->use_delay, &old, -1))
		atomic_inc(&blkg->blkcg->css.cgroup->congestion_count);

	atomic64_set(&blkg->delay_nsec, delay);
}

 
static inline void blkcg_clear_delay(struct blkcg_gq *blkg)
{
	int old = atomic_read(&blkg->use_delay);

	 
	if (old && atomic_try_cmpxchg(&blkg->use_delay, &old, 0))
		atomic_dec(&blkg->blkcg->css.cgroup->congestion_count);
}

 
static inline bool blk_cgroup_mergeable(struct request *rq, struct bio *bio)
{
	return rq->bio->bi_blkg == bio->bi_blkg &&
		bio_issue_as_root_blkg(rq->bio) == bio_issue_as_root_blkg(bio);
}

void blk_cgroup_bio_start(struct bio *bio);
void blkcg_add_delay(struct blkcg_gq *blkg, u64 now, u64 delta);
#else	 

struct blkg_policy_data {
};

struct blkcg_policy_data {
};

struct blkcg_policy {
};

struct blkcg {
};

static inline struct blkcg_gq *blkg_lookup(struct blkcg *blkcg, void *key) { return NULL; }
static inline int blkcg_init_disk(struct gendisk *disk) { return 0; }
static inline void blkcg_exit_disk(struct gendisk *disk) { }
static inline int blkcg_policy_register(struct blkcg_policy *pol) { return 0; }
static inline void blkcg_policy_unregister(struct blkcg_policy *pol) { }
static inline int blkcg_activate_policy(struct gendisk *disk,
					const struct blkcg_policy *pol) { return 0; }
static inline void blkcg_deactivate_policy(struct gendisk *disk,
					   const struct blkcg_policy *pol) { }

static inline struct blkg_policy_data *blkg_to_pd(struct blkcg_gq *blkg,
						  struct blkcg_policy *pol) { return NULL; }
static inline struct blkcg_gq *pd_to_blkg(struct blkg_policy_data *pd) { return NULL; }
static inline char *blkg_path(struct blkcg_gq *blkg) { return NULL; }
static inline void blkg_get(struct blkcg_gq *blkg) { }
static inline void blkg_put(struct blkcg_gq *blkg) { }
static inline void blkcg_bio_issue_init(struct bio *bio) { }
static inline void blk_cgroup_bio_start(struct bio *bio) { }
static inline bool blk_cgroup_mergeable(struct request *rq, struct bio *bio) { return true; }

#define blk_queue_for_each_rl(rl, q)	\
	for ((rl) = &(q)->root_rl; (rl); (rl) = NULL)

#endif	 

#endif  
