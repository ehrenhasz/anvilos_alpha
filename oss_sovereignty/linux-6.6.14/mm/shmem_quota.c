
 
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/shmem_fs.h>

#include <linux/quotaops.h>
#include <linux/quota.h>

#ifdef CONFIG_TMPFS_QUOTA

 
#define SHMEM_MAX_IQ_TIME 604800	 
#define SHMEM_MAX_DQ_TIME 604800	 

struct quota_id {
	struct rb_node	node;
	qid_t		id;
	qsize_t		bhardlimit;
	qsize_t		bsoftlimit;
	qsize_t		ihardlimit;
	qsize_t		isoftlimit;
};

static int shmem_check_quota_file(struct super_block *sb, int type)
{
	 
	return 1;
}

 
static int shmem_read_file_info(struct super_block *sb, int type)
{
	struct quota_info *dqopt = sb_dqopt(sb);
	struct mem_dqinfo *info = &dqopt->info[type];

	info->dqi_priv = kzalloc(sizeof(struct rb_root), GFP_NOFS);
	if (!info->dqi_priv)
		return -ENOMEM;

	info->dqi_max_spc_limit = SHMEM_QUOTA_MAX_SPC_LIMIT;
	info->dqi_max_ino_limit = SHMEM_QUOTA_MAX_INO_LIMIT;

	info->dqi_bgrace = SHMEM_MAX_DQ_TIME;
	info->dqi_igrace = SHMEM_MAX_IQ_TIME;
	info->dqi_flags = 0;

	return 0;
}

static int shmem_write_file_info(struct super_block *sb, int type)
{
	 
	return 0;
}

 
static int shmem_free_file_info(struct super_block *sb, int type)
{
	struct mem_dqinfo *info = &sb_dqopt(sb)->info[type];
	struct rb_root *root = info->dqi_priv;
	struct quota_id *entry;
	struct rb_node *node;

	info->dqi_priv = NULL;
	node = rb_first(root);
	while (node) {
		entry = rb_entry(node, struct quota_id, node);
		node = rb_next(&entry->node);

		rb_erase(&entry->node, root);
		kfree(entry);
	}

	kfree(root);
	return 0;
}

static int shmem_get_next_id(struct super_block *sb, struct kqid *qid)
{
	struct mem_dqinfo *info = sb_dqinfo(sb, qid->type);
	struct rb_node *node = ((struct rb_root *)info->dqi_priv)->rb_node;
	qid_t id = from_kqid(&init_user_ns, *qid);
	struct quota_info *dqopt = sb_dqopt(sb);
	struct quota_id *entry = NULL;
	int ret = 0;

	if (!sb_has_quota_active(sb, qid->type))
		return -ESRCH;

	down_read(&dqopt->dqio_sem);
	while (node) {
		entry = rb_entry(node, struct quota_id, node);

		if (id < entry->id)
			node = node->rb_left;
		else if (id > entry->id)
			node = node->rb_right;
		else
			goto got_next_id;
	}

	if (!entry) {
		ret = -ENOENT;
		goto out_unlock;
	}

	if (id > entry->id) {
		node = rb_next(&entry->node);
		if (!node) {
			ret = -ENOENT;
			goto out_unlock;
		}
		entry = rb_entry(node, struct quota_id, node);
	}

got_next_id:
	*qid = make_kqid(&init_user_ns, qid->type, entry->id);
out_unlock:
	up_read(&dqopt->dqio_sem);
	return ret;
}

 
static int shmem_acquire_dquot(struct dquot *dquot)
{
	struct mem_dqinfo *info = sb_dqinfo(dquot->dq_sb, dquot->dq_id.type);
	struct rb_node **n = &((struct rb_root *)info->dqi_priv)->rb_node;
	struct shmem_sb_info *sbinfo = dquot->dq_sb->s_fs_info;
	struct rb_node *parent = NULL, *new_node = NULL;
	struct quota_id *new_entry, *entry;
	qid_t id = from_kqid(&init_user_ns, dquot->dq_id);
	struct quota_info *dqopt = sb_dqopt(dquot->dq_sb);
	int ret = 0;

	mutex_lock(&dquot->dq_lock);

	down_write(&dqopt->dqio_sem);
	while (*n) {
		parent = *n;
		entry = rb_entry(parent, struct quota_id, node);

		if (id < entry->id)
			n = &(*n)->rb_left;
		else if (id > entry->id)
			n = &(*n)->rb_right;
		else
			goto found;
	}

	 
	new_entry = kzalloc(sizeof(struct quota_id), GFP_NOFS);
	if (!new_entry) {
		ret = -ENOMEM;
		goto out_unlock;
	}

	new_entry->id = id;
	if (dquot->dq_id.type == USRQUOTA) {
		new_entry->bhardlimit = sbinfo->qlimits.usrquota_bhardlimit;
		new_entry->ihardlimit = sbinfo->qlimits.usrquota_ihardlimit;
	} else if (dquot->dq_id.type == GRPQUOTA) {
		new_entry->bhardlimit = sbinfo->qlimits.grpquota_bhardlimit;
		new_entry->ihardlimit = sbinfo->qlimits.grpquota_ihardlimit;
	}

	new_node = &new_entry->node;
	rb_link_node(new_node, parent, n);
	rb_insert_color(new_node, (struct rb_root *)info->dqi_priv);
	entry = new_entry;

found:
	 
	spin_lock(&dquot->dq_dqb_lock);
	dquot->dq_dqb.dqb_bhardlimit = entry->bhardlimit;
	dquot->dq_dqb.dqb_bsoftlimit = entry->bsoftlimit;
	dquot->dq_dqb.dqb_ihardlimit = entry->ihardlimit;
	dquot->dq_dqb.dqb_isoftlimit = entry->isoftlimit;

	if (!dquot->dq_dqb.dqb_bhardlimit &&
	    !dquot->dq_dqb.dqb_bsoftlimit &&
	    !dquot->dq_dqb.dqb_ihardlimit &&
	    !dquot->dq_dqb.dqb_isoftlimit)
		set_bit(DQ_FAKE_B, &dquot->dq_flags);
	spin_unlock(&dquot->dq_dqb_lock);

	 
	smp_mb__before_atomic();
	set_bit(DQ_ACTIVE_B, &dquot->dq_flags);
out_unlock:
	up_write(&dqopt->dqio_sem);
	mutex_unlock(&dquot->dq_lock);
	return ret;
}

static bool shmem_is_empty_dquot(struct dquot *dquot)
{
	struct shmem_sb_info *sbinfo = dquot->dq_sb->s_fs_info;
	qsize_t bhardlimit;
	qsize_t ihardlimit;

	if (dquot->dq_id.type == USRQUOTA) {
		bhardlimit = sbinfo->qlimits.usrquota_bhardlimit;
		ihardlimit = sbinfo->qlimits.usrquota_ihardlimit;
	} else if (dquot->dq_id.type == GRPQUOTA) {
		bhardlimit = sbinfo->qlimits.grpquota_bhardlimit;
		ihardlimit = sbinfo->qlimits.grpquota_ihardlimit;
	}

	if (test_bit(DQ_FAKE_B, &dquot->dq_flags) ||
		(dquot->dq_dqb.dqb_curspace == 0 &&
		 dquot->dq_dqb.dqb_curinodes == 0 &&
		 dquot->dq_dqb.dqb_bhardlimit == bhardlimit &&
		 dquot->dq_dqb.dqb_ihardlimit == ihardlimit))
		return true;

	return false;
}
 
static int shmem_release_dquot(struct dquot *dquot)
{
	struct mem_dqinfo *info = sb_dqinfo(dquot->dq_sb, dquot->dq_id.type);
	struct rb_node *node = ((struct rb_root *)info->dqi_priv)->rb_node;
	qid_t id = from_kqid(&init_user_ns, dquot->dq_id);
	struct quota_info *dqopt = sb_dqopt(dquot->dq_sb);
	struct quota_id *entry = NULL;

	mutex_lock(&dquot->dq_lock);
	 
	if (dquot_is_busy(dquot))
		goto out_dqlock;

	down_write(&dqopt->dqio_sem);
	while (node) {
		entry = rb_entry(node, struct quota_id, node);

		if (id < entry->id)
			node = node->rb_left;
		else if (id > entry->id)
			node = node->rb_right;
		else
			goto found;
	}

	 
	WARN_ONCE(1, "quota id %u from dquot %p, not in rb tree!\n", id, dquot);
	up_write(&dqopt->dqio_sem);
	mutex_unlock(&dquot->dq_lock);
	return -ENOENT;

found:
	if (shmem_is_empty_dquot(dquot)) {
		 
		rb_erase(&entry->node, info->dqi_priv);
		kfree(entry);
	} else {
		 
		spin_lock(&dquot->dq_dqb_lock);
		entry->bhardlimit = dquot->dq_dqb.dqb_bhardlimit;
		entry->bsoftlimit = dquot->dq_dqb.dqb_bsoftlimit;
		entry->ihardlimit = dquot->dq_dqb.dqb_ihardlimit;
		entry->isoftlimit = dquot->dq_dqb.dqb_isoftlimit;
		spin_unlock(&dquot->dq_dqb_lock);
	}

	clear_bit(DQ_ACTIVE_B, &dquot->dq_flags);
	up_write(&dqopt->dqio_sem);

out_dqlock:
	mutex_unlock(&dquot->dq_lock);
	return 0;
}

static int shmem_mark_dquot_dirty(struct dquot *dquot)
{
	return 0;
}

static int shmem_dquot_write_info(struct super_block *sb, int type)
{
	return 0;
}

static const struct quota_format_ops shmem_format_ops = {
	.check_quota_file	= shmem_check_quota_file,
	.read_file_info		= shmem_read_file_info,
	.write_file_info	= shmem_write_file_info,
	.free_file_info		= shmem_free_file_info,
};

struct quota_format_type shmem_quota_format = {
	.qf_fmt_id = QFMT_SHMEM,
	.qf_ops = &shmem_format_ops,
	.qf_owner = THIS_MODULE
};

const struct dquot_operations shmem_quota_operations = {
	.acquire_dquot		= shmem_acquire_dquot,
	.release_dquot		= shmem_release_dquot,
	.alloc_dquot		= dquot_alloc,
	.destroy_dquot		= dquot_destroy,
	.write_info		= shmem_dquot_write_info,
	.mark_dirty		= shmem_mark_dquot_dirty,
	.get_next_id		= shmem_get_next_id,
};
#endif  
