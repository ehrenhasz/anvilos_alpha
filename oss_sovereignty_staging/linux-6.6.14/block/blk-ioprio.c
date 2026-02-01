
 

#include <linux/blk-mq.h>
#include <linux/blk_types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include "blk-cgroup.h"
#include "blk-ioprio.h"
#include "blk-rq-qos.h"

 
enum prio_policy {
	POLICY_NO_CHANGE	= 0,
	POLICY_PROMOTE_TO_RT	= 1,
	POLICY_RESTRICT_TO_BE	= 2,
	POLICY_ALL_TO_IDLE	= 3,
	POLICY_NONE_TO_RT	= 4,
};

static const char *policy_name[] = {
	[POLICY_NO_CHANGE]	= "no-change",
	[POLICY_PROMOTE_TO_RT]	= "promote-to-rt",
	[POLICY_RESTRICT_TO_BE]	= "restrict-to-be",
	[POLICY_ALL_TO_IDLE]	= "idle",
	[POLICY_NONE_TO_RT]	= "none-to-rt",
};

static struct blkcg_policy ioprio_policy;

 
struct ioprio_blkg {
	struct blkg_policy_data pd;
};

 
struct ioprio_blkcg {
	struct blkcg_policy_data cpd;
	enum prio_policy	 prio_policy;
};

static inline struct ioprio_blkg *pd_to_ioprio(struct blkg_policy_data *pd)
{
	return pd ? container_of(pd, struct ioprio_blkg, pd) : NULL;
}

static struct ioprio_blkcg *blkcg_to_ioprio_blkcg(struct blkcg *blkcg)
{
	return container_of(blkcg_to_cpd(blkcg, &ioprio_policy),
			    struct ioprio_blkcg, cpd);
}

static struct ioprio_blkcg *
ioprio_blkcg_from_css(struct cgroup_subsys_state *css)
{
	return blkcg_to_ioprio_blkcg(css_to_blkcg(css));
}

static struct ioprio_blkcg *ioprio_blkcg_from_bio(struct bio *bio)
{
	struct blkg_policy_data *pd = blkg_to_pd(bio->bi_blkg, &ioprio_policy);

	if (!pd)
		return NULL;

	return blkcg_to_ioprio_blkcg(pd->blkg->blkcg);
}

static int ioprio_show_prio_policy(struct seq_file *sf, void *v)
{
	struct ioprio_blkcg *blkcg = ioprio_blkcg_from_css(seq_css(sf));

	seq_printf(sf, "%s\n", policy_name[blkcg->prio_policy]);
	return 0;
}

static ssize_t ioprio_set_prio_policy(struct kernfs_open_file *of, char *buf,
				      size_t nbytes, loff_t off)
{
	struct ioprio_blkcg *blkcg = ioprio_blkcg_from_css(of_css(of));
	int ret;

	if (off != 0)
		return -EIO;
	 
	ret = sysfs_match_string(policy_name, buf);
	if (ret < 0)
		return ret;
	blkcg->prio_policy = ret;
	return nbytes;
}

static struct blkg_policy_data *
ioprio_alloc_pd(struct gendisk *disk, struct blkcg *blkcg, gfp_t gfp)
{
	struct ioprio_blkg *ioprio_blkg;

	ioprio_blkg = kzalloc(sizeof(*ioprio_blkg), gfp);
	if (!ioprio_blkg)
		return NULL;

	return &ioprio_blkg->pd;
}

static void ioprio_free_pd(struct blkg_policy_data *pd)
{
	struct ioprio_blkg *ioprio_blkg = pd_to_ioprio(pd);

	kfree(ioprio_blkg);
}

static struct blkcg_policy_data *ioprio_alloc_cpd(gfp_t gfp)
{
	struct ioprio_blkcg *blkcg;

	blkcg = kzalloc(sizeof(*blkcg), gfp);
	if (!blkcg)
		return NULL;
	blkcg->prio_policy = POLICY_NO_CHANGE;
	return &blkcg->cpd;
}

static void ioprio_free_cpd(struct blkcg_policy_data *cpd)
{
	struct ioprio_blkcg *blkcg = container_of(cpd, typeof(*blkcg), cpd);

	kfree(blkcg);
}

#define IOPRIO_ATTRS						\
	{							\
		.name		= "prio.class",			\
		.seq_show	= ioprio_show_prio_policy,	\
		.write		= ioprio_set_prio_policy,	\
	},							\
	{ }  

 
static struct cftype ioprio_files[] = {
	IOPRIO_ATTRS
};

 
static struct cftype ioprio_legacy_files[] = {
	IOPRIO_ATTRS
};

static struct blkcg_policy ioprio_policy = {
	.dfl_cftypes	= ioprio_files,
	.legacy_cftypes = ioprio_legacy_files,

	.cpd_alloc_fn	= ioprio_alloc_cpd,
	.cpd_free_fn	= ioprio_free_cpd,

	.pd_alloc_fn	= ioprio_alloc_pd,
	.pd_free_fn	= ioprio_free_pd,
};

void blkcg_set_ioprio(struct bio *bio)
{
	struct ioprio_blkcg *blkcg = ioprio_blkcg_from_bio(bio);
	u16 prio;

	if (!blkcg || blkcg->prio_policy == POLICY_NO_CHANGE)
		return;

	if (blkcg->prio_policy == POLICY_PROMOTE_TO_RT ||
	    blkcg->prio_policy == POLICY_NONE_TO_RT) {
		 
		if (IOPRIO_PRIO_CLASS(bio->bi_ioprio) != IOPRIO_CLASS_RT)
			bio->bi_ioprio = IOPRIO_PRIO_VALUE(IOPRIO_CLASS_RT, 4);
		return;
	}

	 
	prio = max_t(u16, bio->bi_ioprio,
			IOPRIO_PRIO_VALUE(blkcg->prio_policy, 0));
	if (prio > bio->bi_ioprio)
		bio->bi_ioprio = prio;
}

void blk_ioprio_exit(struct gendisk *disk)
{
	blkcg_deactivate_policy(disk, &ioprio_policy);
}

int blk_ioprio_init(struct gendisk *disk)
{
	return blkcg_activate_policy(disk, &ioprio_policy);
}

static int __init ioprio_init(void)
{
	return blkcg_policy_register(&ioprio_policy);
}

static void __exit ioprio_exit(void)
{
	blkcg_policy_unregister(&ioprio_policy);
}

module_init(ioprio_init);
module_exit(ioprio_exit);
