
 

#include <linux/bpf-cgroup.h>
#include <linux/device_cgroup.h>
#include <linux/cgroup.h>
#include <linux/ctype.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/rcupdate.h>
#include <linux/mutex.h>

#ifdef CONFIG_CGROUP_DEVICE

static DEFINE_MUTEX(devcgroup_mutex);

enum devcg_behavior {
	DEVCG_DEFAULT_NONE,
	DEVCG_DEFAULT_ALLOW,
	DEVCG_DEFAULT_DENY,
};

 

struct dev_exception_item {
	u32 major, minor;
	short type;
	short access;
	struct list_head list;
	struct rcu_head rcu;
};

struct dev_cgroup {
	struct cgroup_subsys_state css;
	struct list_head exceptions;
	enum devcg_behavior behavior;
};

static inline struct dev_cgroup *css_to_devcgroup(struct cgroup_subsys_state *s)
{
	return s ? container_of(s, struct dev_cgroup, css) : NULL;
}

static inline struct dev_cgroup *task_devcgroup(struct task_struct *task)
{
	return css_to_devcgroup(task_css(task, devices_cgrp_id));
}

 
static int dev_exceptions_copy(struct list_head *dest, struct list_head *orig)
{
	struct dev_exception_item *ex, *tmp, *new;

	lockdep_assert_held(&devcgroup_mutex);

	list_for_each_entry(ex, orig, list) {
		new = kmemdup(ex, sizeof(*ex), GFP_KERNEL);
		if (!new)
			goto free_and_exit;
		list_add_tail(&new->list, dest);
	}

	return 0;

free_and_exit:
	list_for_each_entry_safe(ex, tmp, dest, list) {
		list_del(&ex->list);
		kfree(ex);
	}
	return -ENOMEM;
}

static void dev_exceptions_move(struct list_head *dest, struct list_head *orig)
{
	struct dev_exception_item *ex, *tmp;

	lockdep_assert_held(&devcgroup_mutex);

	list_for_each_entry_safe(ex, tmp, orig, list) {
		list_move_tail(&ex->list, dest);
	}
}

 
static int dev_exception_add(struct dev_cgroup *dev_cgroup,
			     struct dev_exception_item *ex)
{
	struct dev_exception_item *excopy, *walk;

	lockdep_assert_held(&devcgroup_mutex);

	excopy = kmemdup(ex, sizeof(*ex), GFP_KERNEL);
	if (!excopy)
		return -ENOMEM;

	list_for_each_entry(walk, &dev_cgroup->exceptions, list) {
		if (walk->type != ex->type)
			continue;
		if (walk->major != ex->major)
			continue;
		if (walk->minor != ex->minor)
			continue;

		walk->access |= ex->access;
		kfree(excopy);
		excopy = NULL;
	}

	if (excopy != NULL)
		list_add_tail_rcu(&excopy->list, &dev_cgroup->exceptions);
	return 0;
}

 
static void dev_exception_rm(struct dev_cgroup *dev_cgroup,
			     struct dev_exception_item *ex)
{
	struct dev_exception_item *walk, *tmp;

	lockdep_assert_held(&devcgroup_mutex);

	list_for_each_entry_safe(walk, tmp, &dev_cgroup->exceptions, list) {
		if (walk->type != ex->type)
			continue;
		if (walk->major != ex->major)
			continue;
		if (walk->minor != ex->minor)
			continue;

		walk->access &= ~ex->access;
		if (!walk->access) {
			list_del_rcu(&walk->list);
			kfree_rcu(walk, rcu);
		}
	}
}

static void __dev_exception_clean(struct dev_cgroup *dev_cgroup)
{
	struct dev_exception_item *ex, *tmp;

	list_for_each_entry_safe(ex, tmp, &dev_cgroup->exceptions, list) {
		list_del_rcu(&ex->list);
		kfree_rcu(ex, rcu);
	}
}

 
static void dev_exception_clean(struct dev_cgroup *dev_cgroup)
{
	lockdep_assert_held(&devcgroup_mutex);

	__dev_exception_clean(dev_cgroup);
}

static inline bool is_devcg_online(const struct dev_cgroup *devcg)
{
	return (devcg->behavior != DEVCG_DEFAULT_NONE);
}

 
static int devcgroup_online(struct cgroup_subsys_state *css)
{
	struct dev_cgroup *dev_cgroup = css_to_devcgroup(css);
	struct dev_cgroup *parent_dev_cgroup = css_to_devcgroup(css->parent);
	int ret = 0;

	mutex_lock(&devcgroup_mutex);

	if (parent_dev_cgroup == NULL)
		dev_cgroup->behavior = DEVCG_DEFAULT_ALLOW;
	else {
		ret = dev_exceptions_copy(&dev_cgroup->exceptions,
					  &parent_dev_cgroup->exceptions);
		if (!ret)
			dev_cgroup->behavior = parent_dev_cgroup->behavior;
	}
	mutex_unlock(&devcgroup_mutex);

	return ret;
}

static void devcgroup_offline(struct cgroup_subsys_state *css)
{
	struct dev_cgroup *dev_cgroup = css_to_devcgroup(css);

	mutex_lock(&devcgroup_mutex);
	dev_cgroup->behavior = DEVCG_DEFAULT_NONE;
	mutex_unlock(&devcgroup_mutex);
}

 
static struct cgroup_subsys_state *
devcgroup_css_alloc(struct cgroup_subsys_state *parent_css)
{
	struct dev_cgroup *dev_cgroup;

	dev_cgroup = kzalloc(sizeof(*dev_cgroup), GFP_KERNEL);
	if (!dev_cgroup)
		return ERR_PTR(-ENOMEM);
	INIT_LIST_HEAD(&dev_cgroup->exceptions);
	dev_cgroup->behavior = DEVCG_DEFAULT_NONE;

	return &dev_cgroup->css;
}

static void devcgroup_css_free(struct cgroup_subsys_state *css)
{
	struct dev_cgroup *dev_cgroup = css_to_devcgroup(css);

	__dev_exception_clean(dev_cgroup);
	kfree(dev_cgroup);
}

#define DEVCG_ALLOW 1
#define DEVCG_DENY 2
#define DEVCG_LIST 3

#define MAJMINLEN 13
#define ACCLEN 4

static void set_access(char *acc, short access)
{
	int idx = 0;
	memset(acc, 0, ACCLEN);
	if (access & DEVCG_ACC_READ)
		acc[idx++] = 'r';
	if (access & DEVCG_ACC_WRITE)
		acc[idx++] = 'w';
	if (access & DEVCG_ACC_MKNOD)
		acc[idx++] = 'm';
}

static char type_to_char(short type)
{
	if (type == DEVCG_DEV_ALL)
		return 'a';
	if (type == DEVCG_DEV_CHAR)
		return 'c';
	if (type == DEVCG_DEV_BLOCK)
		return 'b';
	return 'X';
}

static void set_majmin(char *str, unsigned m)
{
	if (m == ~0)
		strcpy(str, "*");
	else
		sprintf(str, "%u", m);
}

static int devcgroup_seq_show(struct seq_file *m, void *v)
{
	struct dev_cgroup *devcgroup = css_to_devcgroup(seq_css(m));
	struct dev_exception_item *ex;
	char maj[MAJMINLEN], min[MAJMINLEN], acc[ACCLEN];

	rcu_read_lock();
	 
	if (devcgroup->behavior == DEVCG_DEFAULT_ALLOW) {
		set_access(acc, DEVCG_ACC_MASK);
		set_majmin(maj, ~0);
		set_majmin(min, ~0);
		seq_printf(m, "%c %s:%s %s\n", type_to_char(DEVCG_DEV_ALL),
			   maj, min, acc);
	} else {
		list_for_each_entry_rcu(ex, &devcgroup->exceptions, list) {
			set_access(acc, ex->access);
			set_majmin(maj, ex->major);
			set_majmin(min, ex->minor);
			seq_printf(m, "%c %s:%s %s\n", type_to_char(ex->type),
				   maj, min, acc);
		}
	}
	rcu_read_unlock();

	return 0;
}

 
static bool match_exception(struct list_head *exceptions, short type,
			    u32 major, u32 minor, short access)
{
	struct dev_exception_item *ex;

	list_for_each_entry_rcu(ex, exceptions, list) {
		if ((type & DEVCG_DEV_BLOCK) && !(ex->type & DEVCG_DEV_BLOCK))
			continue;
		if ((type & DEVCG_DEV_CHAR) && !(ex->type & DEVCG_DEV_CHAR))
			continue;
		if (ex->major != ~0 && ex->major != major)
			continue;
		if (ex->minor != ~0 && ex->minor != minor)
			continue;
		 
		if (access & (~ex->access))
			continue;
		return true;
	}
	return false;
}

 
static bool match_exception_partial(struct list_head *exceptions, short type,
				    u32 major, u32 minor, short access)
{
	struct dev_exception_item *ex;

	list_for_each_entry_rcu(ex, exceptions, list,
				lockdep_is_held(&devcgroup_mutex)) {
		if ((type & DEVCG_DEV_BLOCK) && !(ex->type & DEVCG_DEV_BLOCK))
			continue;
		if ((type & DEVCG_DEV_CHAR) && !(ex->type & DEVCG_DEV_CHAR))
			continue;
		 
		if (ex->major != ~0 && major != ~0 && ex->major != major)
			continue;
		if (ex->minor != ~0 && minor != ~0 && ex->minor != minor)
			continue;
		 
		if (!(access & ex->access))
			continue;
		return true;
	}
	return false;
}

 
static bool verify_new_ex(struct dev_cgroup *dev_cgroup,
		          struct dev_exception_item *refex,
		          enum devcg_behavior behavior)
{
	bool match = false;

	RCU_LOCKDEP_WARN(!rcu_read_lock_held() &&
			 !lockdep_is_held(&devcgroup_mutex),
			 "device_cgroup:verify_new_ex called without proper synchronization");

	if (dev_cgroup->behavior == DEVCG_DEFAULT_ALLOW) {
		if (behavior == DEVCG_DEFAULT_ALLOW) {
			  
			return true;
		} else {
			  
			match = match_exception_partial(&dev_cgroup->exceptions,
							refex->type,
							refex->major,
							refex->minor,
							refex->access);

			if (match)
				return false;
			return true;
		}
	} else {
		 
		match = match_exception(&dev_cgroup->exceptions, refex->type,
					refex->major, refex->minor,
					refex->access);

		if (match)
			 
			return true;
		else
			return false;
	}
	return false;
}

 
static int parent_has_perm(struct dev_cgroup *childcg,
				  struct dev_exception_item *ex)
{
	struct dev_cgroup *parent = css_to_devcgroup(childcg->css.parent);

	if (!parent)
		return 1;
	return verify_new_ex(parent, ex, childcg->behavior);
}

 
static bool parent_allows_removal(struct dev_cgroup *childcg,
				  struct dev_exception_item *ex)
{
	struct dev_cgroup *parent = css_to_devcgroup(childcg->css.parent);

	if (!parent)
		return true;

	 
	if (childcg->behavior == DEVCG_DEFAULT_DENY)
		return true;

	 
	return !match_exception_partial(&parent->exceptions, ex->type,
					ex->major, ex->minor, ex->access);
}

 
static inline int may_allow_all(struct dev_cgroup *parent)
{
	if (!parent)
		return 1;
	return parent->behavior == DEVCG_DEFAULT_ALLOW;
}

 
static void revalidate_active_exceptions(struct dev_cgroup *devcg)
{
	struct dev_exception_item *ex;
	struct list_head *this, *tmp;

	list_for_each_safe(this, tmp, &devcg->exceptions) {
		ex = container_of(this, struct dev_exception_item, list);
		if (!parent_has_perm(devcg, ex))
			dev_exception_rm(devcg, ex);
	}
}

 
static int propagate_exception(struct dev_cgroup *devcg_root,
			       struct dev_exception_item *ex)
{
	struct cgroup_subsys_state *pos;
	int rc = 0;

	rcu_read_lock();

	css_for_each_descendant_pre(pos, &devcg_root->css) {
		struct dev_cgroup *devcg = css_to_devcgroup(pos);

		 
		if (pos == &devcg_root->css || !is_devcg_online(devcg))
			continue;

		rcu_read_unlock();

		 
		if (devcg_root->behavior == DEVCG_DEFAULT_ALLOW &&
		    devcg->behavior == DEVCG_DEFAULT_ALLOW) {
			rc = dev_exception_add(devcg, ex);
			if (rc)
				return rc;
		} else {
			 
			dev_exception_rm(devcg, ex);
		}
		revalidate_active_exceptions(devcg);

		rcu_read_lock();
	}

	rcu_read_unlock();
	return rc;
}

 
static int devcgroup_update_access(struct dev_cgroup *devcgroup,
				   int filetype, char *buffer)
{
	const char *b;
	char temp[12];		 
	int count, rc = 0;
	struct dev_exception_item ex;
	struct dev_cgroup *parent = css_to_devcgroup(devcgroup->css.parent);
	struct dev_cgroup tmp_devcgrp;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	memset(&ex, 0, sizeof(ex));
	memset(&tmp_devcgrp, 0, sizeof(tmp_devcgrp));
	b = buffer;

	switch (*b) {
	case 'a':
		switch (filetype) {
		case DEVCG_ALLOW:
			if (css_has_online_children(&devcgroup->css))
				return -EINVAL;

			if (!may_allow_all(parent))
				return -EPERM;
			if (!parent) {
				devcgroup->behavior = DEVCG_DEFAULT_ALLOW;
				dev_exception_clean(devcgroup);
				break;
			}

			INIT_LIST_HEAD(&tmp_devcgrp.exceptions);
			rc = dev_exceptions_copy(&tmp_devcgrp.exceptions,
						 &devcgroup->exceptions);
			if (rc)
				return rc;
			dev_exception_clean(devcgroup);
			rc = dev_exceptions_copy(&devcgroup->exceptions,
						 &parent->exceptions);
			if (rc) {
				dev_exceptions_move(&devcgroup->exceptions,
						    &tmp_devcgrp.exceptions);
				return rc;
			}
			devcgroup->behavior = DEVCG_DEFAULT_ALLOW;
			dev_exception_clean(&tmp_devcgrp);
			break;
		case DEVCG_DENY:
			if (css_has_online_children(&devcgroup->css))
				return -EINVAL;

			dev_exception_clean(devcgroup);
			devcgroup->behavior = DEVCG_DEFAULT_DENY;
			break;
		default:
			return -EINVAL;
		}
		return 0;
	case 'b':
		ex.type = DEVCG_DEV_BLOCK;
		break;
	case 'c':
		ex.type = DEVCG_DEV_CHAR;
		break;
	default:
		return -EINVAL;
	}
	b++;
	if (!isspace(*b))
		return -EINVAL;
	b++;
	if (*b == '*') {
		ex.major = ~0;
		b++;
	} else if (isdigit(*b)) {
		memset(temp, 0, sizeof(temp));
		for (count = 0; count < sizeof(temp) - 1; count++) {
			temp[count] = *b;
			b++;
			if (!isdigit(*b))
				break;
		}
		rc = kstrtou32(temp, 10, &ex.major);
		if (rc)
			return -EINVAL;
	} else {
		return -EINVAL;
	}
	if (*b != ':')
		return -EINVAL;
	b++;

	 
	if (*b == '*') {
		ex.minor = ~0;
		b++;
	} else if (isdigit(*b)) {
		memset(temp, 0, sizeof(temp));
		for (count = 0; count < sizeof(temp) - 1; count++) {
			temp[count] = *b;
			b++;
			if (!isdigit(*b))
				break;
		}
		rc = kstrtou32(temp, 10, &ex.minor);
		if (rc)
			return -EINVAL;
	} else {
		return -EINVAL;
	}
	if (!isspace(*b))
		return -EINVAL;
	for (b++, count = 0; count < 3; count++, b++) {
		switch (*b) {
		case 'r':
			ex.access |= DEVCG_ACC_READ;
			break;
		case 'w':
			ex.access |= DEVCG_ACC_WRITE;
			break;
		case 'm':
			ex.access |= DEVCG_ACC_MKNOD;
			break;
		case '\n':
		case '\0':
			count = 3;
			break;
		default:
			return -EINVAL;
		}
	}

	switch (filetype) {
	case DEVCG_ALLOW:
		 
		if (devcgroup->behavior == DEVCG_DEFAULT_ALLOW) {
			 
			if (!parent_allows_removal(devcgroup, &ex))
				return -EPERM;
			dev_exception_rm(devcgroup, &ex);
			break;
		}

		if (!parent_has_perm(devcgroup, &ex))
			return -EPERM;
		rc = dev_exception_add(devcgroup, &ex);
		break;
	case DEVCG_DENY:
		 
		if (devcgroup->behavior == DEVCG_DEFAULT_DENY)
			dev_exception_rm(devcgroup, &ex);
		else
			rc = dev_exception_add(devcgroup, &ex);

		if (rc)
			break;
		 
		rc = propagate_exception(devcgroup, &ex);
		break;
	default:
		rc = -EINVAL;
	}
	return rc;
}

static ssize_t devcgroup_access_write(struct kernfs_open_file *of,
				      char *buf, size_t nbytes, loff_t off)
{
	int retval;

	mutex_lock(&devcgroup_mutex);
	retval = devcgroup_update_access(css_to_devcgroup(of_css(of)),
					 of_cft(of)->private, strstrip(buf));
	mutex_unlock(&devcgroup_mutex);
	return retval ?: nbytes;
}

static struct cftype dev_cgroup_files[] = {
	{
		.name = "allow",
		.write = devcgroup_access_write,
		.private = DEVCG_ALLOW,
	},
	{
		.name = "deny",
		.write = devcgroup_access_write,
		.private = DEVCG_DENY,
	},
	{
		.name = "list",
		.seq_show = devcgroup_seq_show,
		.private = DEVCG_LIST,
	},
	{ }	 
};

struct cgroup_subsys devices_cgrp_subsys = {
	.css_alloc = devcgroup_css_alloc,
	.css_free = devcgroup_css_free,
	.css_online = devcgroup_online,
	.css_offline = devcgroup_offline,
	.legacy_cftypes = dev_cgroup_files,
};

 
static int devcgroup_legacy_check_permission(short type, u32 major, u32 minor,
					short access)
{
	struct dev_cgroup *dev_cgroup;
	bool rc;

	rcu_read_lock();
	dev_cgroup = task_devcgroup(current);
	if (dev_cgroup->behavior == DEVCG_DEFAULT_ALLOW)
		 
		rc = !match_exception_partial(&dev_cgroup->exceptions,
					      type, major, minor, access);
	else
		 
		rc = match_exception(&dev_cgroup->exceptions, type, major,
				     minor, access);
	rcu_read_unlock();

	if (!rc)
		return -EPERM;

	return 0;
}

#endif  

#if defined(CONFIG_CGROUP_DEVICE) || defined(CONFIG_CGROUP_BPF)

int devcgroup_check_permission(short type, u32 major, u32 minor, short access)
{
	int rc = BPF_CGROUP_RUN_PROG_DEVICE_CGROUP(type, major, minor, access);

	if (rc)
		return rc;

	#ifdef CONFIG_CGROUP_DEVICE
	return devcgroup_legacy_check_permission(type, major, minor, access);

	#else  
	return 0;

	#endif  
}
EXPORT_SYMBOL(devcgroup_check_permission);
#endif  
