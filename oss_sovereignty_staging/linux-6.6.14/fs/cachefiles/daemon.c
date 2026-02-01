
 

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/namei.h>
#include <linux/poll.h>
#include <linux/mount.h>
#include <linux/statfs.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/fs_struct.h>
#include "internal.h"

static int cachefiles_daemon_open(struct inode *, struct file *);
static int cachefiles_daemon_release(struct inode *, struct file *);
static ssize_t cachefiles_daemon_read(struct file *, char __user *, size_t,
				      loff_t *);
static ssize_t cachefiles_daemon_write(struct file *, const char __user *,
				       size_t, loff_t *);
static __poll_t cachefiles_daemon_poll(struct file *,
					   struct poll_table_struct *);
static int cachefiles_daemon_frun(struct cachefiles_cache *, char *);
static int cachefiles_daemon_fcull(struct cachefiles_cache *, char *);
static int cachefiles_daemon_fstop(struct cachefiles_cache *, char *);
static int cachefiles_daemon_brun(struct cachefiles_cache *, char *);
static int cachefiles_daemon_bcull(struct cachefiles_cache *, char *);
static int cachefiles_daemon_bstop(struct cachefiles_cache *, char *);
static int cachefiles_daemon_cull(struct cachefiles_cache *, char *);
static int cachefiles_daemon_debug(struct cachefiles_cache *, char *);
static int cachefiles_daemon_dir(struct cachefiles_cache *, char *);
static int cachefiles_daemon_inuse(struct cachefiles_cache *, char *);
static int cachefiles_daemon_secctx(struct cachefiles_cache *, char *);
static int cachefiles_daemon_tag(struct cachefiles_cache *, char *);
static int cachefiles_daemon_bind(struct cachefiles_cache *, char *);
static void cachefiles_daemon_unbind(struct cachefiles_cache *);

static unsigned long cachefiles_open;

const struct file_operations cachefiles_daemon_fops = {
	.owner		= THIS_MODULE,
	.open		= cachefiles_daemon_open,
	.release	= cachefiles_daemon_release,
	.read		= cachefiles_daemon_read,
	.write		= cachefiles_daemon_write,
	.poll		= cachefiles_daemon_poll,
	.llseek		= noop_llseek,
};

struct cachefiles_daemon_cmd {
	char name[8];
	int (*handler)(struct cachefiles_cache *cache, char *args);
};

static const struct cachefiles_daemon_cmd cachefiles_daemon_cmds[] = {
	{ "bind",	cachefiles_daemon_bind		},
	{ "brun",	cachefiles_daemon_brun		},
	{ "bcull",	cachefiles_daemon_bcull		},
	{ "bstop",	cachefiles_daemon_bstop		},
	{ "cull",	cachefiles_daemon_cull		},
	{ "debug",	cachefiles_daemon_debug		},
	{ "dir",	cachefiles_daemon_dir		},
	{ "frun",	cachefiles_daemon_frun		},
	{ "fcull",	cachefiles_daemon_fcull		},
	{ "fstop",	cachefiles_daemon_fstop		},
	{ "inuse",	cachefiles_daemon_inuse		},
	{ "secctx",	cachefiles_daemon_secctx	},
	{ "tag",	cachefiles_daemon_tag		},
#ifdef CONFIG_CACHEFILES_ONDEMAND
	{ "copen",	cachefiles_ondemand_copen	},
#endif
	{ "",		NULL				}
};


 
static int cachefiles_daemon_open(struct inode *inode, struct file *file)
{
	struct cachefiles_cache *cache;

	_enter("");

	 
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	 
	if (xchg(&cachefiles_open, 1) == 1)
		return -EBUSY;

	 
	cache = kzalloc(sizeof(struct cachefiles_cache), GFP_KERNEL);
	if (!cache) {
		cachefiles_open = 0;
		return -ENOMEM;
	}

	mutex_init(&cache->daemon_mutex);
	init_waitqueue_head(&cache->daemon_pollwq);
	INIT_LIST_HEAD(&cache->volumes);
	INIT_LIST_HEAD(&cache->object_list);
	spin_lock_init(&cache->object_list_lock);
	refcount_set(&cache->unbind_pincount, 1);
	xa_init_flags(&cache->reqs, XA_FLAGS_ALLOC);
	xa_init_flags(&cache->ondemand_ids, XA_FLAGS_ALLOC1);

	 
	cache->frun_percent = 7;
	cache->fcull_percent = 5;
	cache->fstop_percent = 1;
	cache->brun_percent = 7;
	cache->bcull_percent = 5;
	cache->bstop_percent = 1;

	file->private_data = cache;
	cache->cachefilesd = file;
	return 0;
}

static void cachefiles_flush_reqs(struct cachefiles_cache *cache)
{
	struct xarray *xa = &cache->reqs;
	struct cachefiles_req *req;
	unsigned long index;

	 
	smp_mb();

	xa_lock(xa);
	xa_for_each(xa, index, req) {
		req->error = -EIO;
		complete(&req->done);
	}
	xa_unlock(xa);

	xa_destroy(&cache->reqs);
	xa_destroy(&cache->ondemand_ids);
}

void cachefiles_put_unbind_pincount(struct cachefiles_cache *cache)
{
	if (refcount_dec_and_test(&cache->unbind_pincount)) {
		cachefiles_daemon_unbind(cache);
		cachefiles_open = 0;
		kfree(cache);
	}
}

void cachefiles_get_unbind_pincount(struct cachefiles_cache *cache)
{
	refcount_inc(&cache->unbind_pincount);
}

 
static int cachefiles_daemon_release(struct inode *inode, struct file *file)
{
	struct cachefiles_cache *cache = file->private_data;

	_enter("");

	ASSERT(cache);

	set_bit(CACHEFILES_DEAD, &cache->flags);

	if (cachefiles_in_ondemand_mode(cache))
		cachefiles_flush_reqs(cache);

	 
	cache->cachefilesd = NULL;
	file->private_data = NULL;

	cachefiles_put_unbind_pincount(cache);

	_leave("");
	return 0;
}

static ssize_t cachefiles_do_daemon_read(struct cachefiles_cache *cache,
					 char __user *_buffer, size_t buflen)
{
	unsigned long long b_released;
	unsigned f_released;
	char buffer[256];
	int n;

	 
	cachefiles_has_space(cache, 0, 0, cachefiles_has_space_check);

	 
	f_released = atomic_xchg(&cache->f_released, 0);
	b_released = atomic_long_xchg(&cache->b_released, 0);
	clear_bit(CACHEFILES_STATE_CHANGED, &cache->flags);

	n = snprintf(buffer, sizeof(buffer),
		     "cull=%c"
		     " frun=%llx"
		     " fcull=%llx"
		     " fstop=%llx"
		     " brun=%llx"
		     " bcull=%llx"
		     " bstop=%llx"
		     " freleased=%x"
		     " breleased=%llx",
		     test_bit(CACHEFILES_CULLING, &cache->flags) ? '1' : '0',
		     (unsigned long long) cache->frun,
		     (unsigned long long) cache->fcull,
		     (unsigned long long) cache->fstop,
		     (unsigned long long) cache->brun,
		     (unsigned long long) cache->bcull,
		     (unsigned long long) cache->bstop,
		     f_released,
		     b_released);

	if (n > buflen)
		return -EMSGSIZE;

	if (copy_to_user(_buffer, buffer, n) != 0)
		return -EFAULT;

	return n;
}

 
static ssize_t cachefiles_daemon_read(struct file *file, char __user *_buffer,
				      size_t buflen, loff_t *pos)
{
	struct cachefiles_cache *cache = file->private_data;

	 

	if (!test_bit(CACHEFILES_READY, &cache->flags))
		return 0;

	if (cachefiles_in_ondemand_mode(cache))
		return cachefiles_ondemand_daemon_read(cache, _buffer, buflen);
	else
		return cachefiles_do_daemon_read(cache, _buffer, buflen);
}

 
static ssize_t cachefiles_daemon_write(struct file *file,
				       const char __user *_data,
				       size_t datalen,
				       loff_t *pos)
{
	const struct cachefiles_daemon_cmd *cmd;
	struct cachefiles_cache *cache = file->private_data;
	ssize_t ret;
	char *data, *args, *cp;

	 

	ASSERT(cache);

	if (test_bit(CACHEFILES_DEAD, &cache->flags))
		return -EIO;

	if (datalen > PAGE_SIZE - 1)
		return -EOPNOTSUPP;

	 
	data = memdup_user_nul(_data, datalen);
	if (IS_ERR(data))
		return PTR_ERR(data);

	ret = -EINVAL;
	if (memchr(data, '\0', datalen))
		goto error;

	 
	cp = memchr(data, '\n', datalen);
	if (cp) {
		if (cp == data)
			goto error;

		*cp = '\0';
	}

	 
	ret = -EOPNOTSUPP;

	for (args = data; *args; args++)
		if (isspace(*args))
			break;
	if (*args) {
		if (args == data)
			goto error;
		*args = '\0';
		args = skip_spaces(++args);
	}

	 
	for (cmd = cachefiles_daemon_cmds; cmd->name[0]; cmd++)
		if (strcmp(cmd->name, data) == 0)
			goto found_command;

error:
	kfree(data);
	 
	return ret;

found_command:
	mutex_lock(&cache->daemon_mutex);

	ret = -EIO;
	if (!test_bit(CACHEFILES_DEAD, &cache->flags))
		ret = cmd->handler(cache, args);

	mutex_unlock(&cache->daemon_mutex);

	if (ret == 0)
		ret = datalen;
	goto error;
}

 
static __poll_t cachefiles_daemon_poll(struct file *file,
					   struct poll_table_struct *poll)
{
	struct cachefiles_cache *cache = file->private_data;
	__poll_t mask;

	poll_wait(file, &cache->daemon_pollwq, poll);
	mask = 0;

	if (cachefiles_in_ondemand_mode(cache)) {
		if (!xa_empty(&cache->reqs))
			mask |= EPOLLIN;
	} else {
		if (test_bit(CACHEFILES_STATE_CHANGED, &cache->flags))
			mask |= EPOLLIN;
	}

	if (test_bit(CACHEFILES_CULLING, &cache->flags))
		mask |= EPOLLOUT;

	return mask;
}

 
static int cachefiles_daemon_range_error(struct cachefiles_cache *cache,
					 char *args)
{
	pr_err("Free space limits must be in range 0%%<=stop<cull<run<100%%\n");

	return -EINVAL;
}

 
static int cachefiles_daemon_frun(struct cachefiles_cache *cache, char *args)
{
	unsigned long frun;

	_enter(",%s", args);

	if (!*args)
		return -EINVAL;

	frun = simple_strtoul(args, &args, 10);
	if (args[0] != '%' || args[1] != '\0')
		return -EINVAL;

	if (frun <= cache->fcull_percent || frun >= 100)
		return cachefiles_daemon_range_error(cache, args);

	cache->frun_percent = frun;
	return 0;
}

 
static int cachefiles_daemon_fcull(struct cachefiles_cache *cache, char *args)
{
	unsigned long fcull;

	_enter(",%s", args);

	if (!*args)
		return -EINVAL;

	fcull = simple_strtoul(args, &args, 10);
	if (args[0] != '%' || args[1] != '\0')
		return -EINVAL;

	if (fcull <= cache->fstop_percent || fcull >= cache->frun_percent)
		return cachefiles_daemon_range_error(cache, args);

	cache->fcull_percent = fcull;
	return 0;
}

 
static int cachefiles_daemon_fstop(struct cachefiles_cache *cache, char *args)
{
	unsigned long fstop;

	_enter(",%s", args);

	if (!*args)
		return -EINVAL;

	fstop = simple_strtoul(args, &args, 10);
	if (args[0] != '%' || args[1] != '\0')
		return -EINVAL;

	if (fstop >= cache->fcull_percent)
		return cachefiles_daemon_range_error(cache, args);

	cache->fstop_percent = fstop;
	return 0;
}

 
static int cachefiles_daemon_brun(struct cachefiles_cache *cache, char *args)
{
	unsigned long brun;

	_enter(",%s", args);

	if (!*args)
		return -EINVAL;

	brun = simple_strtoul(args, &args, 10);
	if (args[0] != '%' || args[1] != '\0')
		return -EINVAL;

	if (brun <= cache->bcull_percent || brun >= 100)
		return cachefiles_daemon_range_error(cache, args);

	cache->brun_percent = brun;
	return 0;
}

 
static int cachefiles_daemon_bcull(struct cachefiles_cache *cache, char *args)
{
	unsigned long bcull;

	_enter(",%s", args);

	if (!*args)
		return -EINVAL;

	bcull = simple_strtoul(args, &args, 10);
	if (args[0] != '%' || args[1] != '\0')
		return -EINVAL;

	if (bcull <= cache->bstop_percent || bcull >= cache->brun_percent)
		return cachefiles_daemon_range_error(cache, args);

	cache->bcull_percent = bcull;
	return 0;
}

 
static int cachefiles_daemon_bstop(struct cachefiles_cache *cache, char *args)
{
	unsigned long bstop;

	_enter(",%s", args);

	if (!*args)
		return -EINVAL;

	bstop = simple_strtoul(args, &args, 10);
	if (args[0] != '%' || args[1] != '\0')
		return -EINVAL;

	if (bstop >= cache->bcull_percent)
		return cachefiles_daemon_range_error(cache, args);

	cache->bstop_percent = bstop;
	return 0;
}

 
static int cachefiles_daemon_dir(struct cachefiles_cache *cache, char *args)
{
	char *dir;

	_enter(",%s", args);

	if (!*args) {
		pr_err("Empty directory specified\n");
		return -EINVAL;
	}

	if (cache->rootdirname) {
		pr_err("Second cache directory specified\n");
		return -EEXIST;
	}

	dir = kstrdup(args, GFP_KERNEL);
	if (!dir)
		return -ENOMEM;

	cache->rootdirname = dir;
	return 0;
}

 
static int cachefiles_daemon_secctx(struct cachefiles_cache *cache, char *args)
{
	char *secctx;

	_enter(",%s", args);

	if (!*args) {
		pr_err("Empty security context specified\n");
		return -EINVAL;
	}

	if (cache->secctx) {
		pr_err("Second security context specified\n");
		return -EINVAL;
	}

	secctx = kstrdup(args, GFP_KERNEL);
	if (!secctx)
		return -ENOMEM;

	cache->secctx = secctx;
	return 0;
}

 
static int cachefiles_daemon_tag(struct cachefiles_cache *cache, char *args)
{
	char *tag;

	_enter(",%s", args);

	if (!*args) {
		pr_err("Empty tag specified\n");
		return -EINVAL;
	}

	if (cache->tag)
		return -EEXIST;

	tag = kstrdup(args, GFP_KERNEL);
	if (!tag)
		return -ENOMEM;

	cache->tag = tag;
	return 0;
}

 
static int cachefiles_daemon_cull(struct cachefiles_cache *cache, char *args)
{
	struct path path;
	const struct cred *saved_cred;
	int ret;

	_enter(",%s", args);

	if (strchr(args, '/'))
		goto inval;

	if (!test_bit(CACHEFILES_READY, &cache->flags)) {
		pr_err("cull applied to unready cache\n");
		return -EIO;
	}

	if (test_bit(CACHEFILES_DEAD, &cache->flags)) {
		pr_err("cull applied to dead cache\n");
		return -EIO;
	}

	get_fs_pwd(current->fs, &path);

	if (!d_can_lookup(path.dentry))
		goto notdir;

	cachefiles_begin_secure(cache, &saved_cred);
	ret = cachefiles_cull(cache, path.dentry, args);
	cachefiles_end_secure(cache, saved_cred);

	path_put(&path);
	_leave(" = %d", ret);
	return ret;

notdir:
	path_put(&path);
	pr_err("cull command requires dirfd to be a directory\n");
	return -ENOTDIR;

inval:
	pr_err("cull command requires dirfd and filename\n");
	return -EINVAL;
}

 
static int cachefiles_daemon_debug(struct cachefiles_cache *cache, char *args)
{
	unsigned long mask;

	_enter(",%s", args);

	mask = simple_strtoul(args, &args, 0);
	if (args[0] != '\0')
		goto inval;

	cachefiles_debug = mask;
	_leave(" = 0");
	return 0;

inval:
	pr_err("debug command requires mask\n");
	return -EINVAL;
}

 
static int cachefiles_daemon_inuse(struct cachefiles_cache *cache, char *args)
{
	struct path path;
	const struct cred *saved_cred;
	int ret;

	 

	if (strchr(args, '/'))
		goto inval;

	if (!test_bit(CACHEFILES_READY, &cache->flags)) {
		pr_err("inuse applied to unready cache\n");
		return -EIO;
	}

	if (test_bit(CACHEFILES_DEAD, &cache->flags)) {
		pr_err("inuse applied to dead cache\n");
		return -EIO;
	}

	get_fs_pwd(current->fs, &path);

	if (!d_can_lookup(path.dentry))
		goto notdir;

	cachefiles_begin_secure(cache, &saved_cred);
	ret = cachefiles_check_in_use(cache, path.dentry, args);
	cachefiles_end_secure(cache, saved_cred);

	path_put(&path);
	 
	return ret;

notdir:
	path_put(&path);
	pr_err("inuse command requires dirfd to be a directory\n");
	return -ENOTDIR;

inval:
	pr_err("inuse command requires dirfd and filename\n");
	return -EINVAL;
}

 
static int cachefiles_daemon_bind(struct cachefiles_cache *cache, char *args)
{
	_enter("{%u,%u,%u,%u,%u,%u},%s",
	       cache->frun_percent,
	       cache->fcull_percent,
	       cache->fstop_percent,
	       cache->brun_percent,
	       cache->bcull_percent,
	       cache->bstop_percent,
	       args);

	if (cache->fstop_percent >= cache->fcull_percent ||
	    cache->fcull_percent >= cache->frun_percent ||
	    cache->frun_percent  >= 100)
		return -ERANGE;

	if (cache->bstop_percent >= cache->bcull_percent ||
	    cache->bcull_percent >= cache->brun_percent ||
	    cache->brun_percent  >= 100)
		return -ERANGE;

	if (!cache->rootdirname) {
		pr_err("No cache directory specified\n");
		return -EINVAL;
	}

	 
	if (test_bit(CACHEFILES_READY, &cache->flags)) {
		pr_err("Cache already bound\n");
		return -EBUSY;
	}

	if (IS_ENABLED(CONFIG_CACHEFILES_ONDEMAND)) {
		if (!strcmp(args, "ondemand")) {
			set_bit(CACHEFILES_ONDEMAND_MODE, &cache->flags);
		} else if (*args) {
			pr_err("Invalid argument to the 'bind' command\n");
			return -EINVAL;
		}
	} else if (*args) {
		pr_err("'bind' command doesn't take an argument\n");
		return -EINVAL;
	}

	 
	if (!cache->tag) {
		 
		cache->tag = kstrdup("CacheFiles", GFP_KERNEL);
		if (!cache->tag)
			return -ENOMEM;
	}

	return cachefiles_add_cache(cache);
}

 
static void cachefiles_daemon_unbind(struct cachefiles_cache *cache)
{
	_enter("");

	if (test_bit(CACHEFILES_READY, &cache->flags))
		cachefiles_withdraw_cache(cache);

	cachefiles_put_directory(cache->graveyard);
	cachefiles_put_directory(cache->store);
	mntput(cache->mnt);

	kfree(cache->rootdirname);
	kfree(cache->secctx);
	kfree(cache->tag);

	_leave("");
}
