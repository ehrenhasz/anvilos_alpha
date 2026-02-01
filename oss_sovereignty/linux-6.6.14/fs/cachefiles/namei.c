
 

#include <linux/fs.h>
#include <linux/namei.h>
#include "internal.h"

 
static bool __cachefiles_mark_inode_in_use(struct cachefiles_object *object,
					   struct inode *inode)
{
	bool can_use = false;

	if (!(inode->i_flags & S_KERNEL_FILE)) {
		inode->i_flags |= S_KERNEL_FILE;
		trace_cachefiles_mark_active(object, inode);
		can_use = true;
	} else {
		trace_cachefiles_mark_failed(object, inode);
	}

	return can_use;
}

static bool cachefiles_mark_inode_in_use(struct cachefiles_object *object,
					 struct inode *inode)
{
	bool can_use;

	inode_lock(inode);
	can_use = __cachefiles_mark_inode_in_use(object, inode);
	inode_unlock(inode);
	return can_use;
}

 
static void __cachefiles_unmark_inode_in_use(struct cachefiles_object *object,
					     struct inode *inode)
{
	inode->i_flags &= ~S_KERNEL_FILE;
	trace_cachefiles_mark_inactive(object, inode);
}

static void cachefiles_do_unmark_inode_in_use(struct cachefiles_object *object,
					      struct inode *inode)
{
	inode_lock(inode);
	__cachefiles_unmark_inode_in_use(object, inode);
	inode_unlock(inode);
}

 
void cachefiles_unmark_inode_in_use(struct cachefiles_object *object,
				    struct file *file)
{
	struct cachefiles_cache *cache = object->volume->cache;
	struct inode *inode = file_inode(file);

	cachefiles_do_unmark_inode_in_use(object, inode);

	if (!test_bit(CACHEFILES_OBJECT_USING_TMPFILE, &object->flags)) {
		atomic_long_add(inode->i_blocks, &cache->b_released);
		if (atomic_inc_return(&cache->f_released))
			cachefiles_state_changed(cache);
	}
}

 
struct dentry *cachefiles_get_directory(struct cachefiles_cache *cache,
					struct dentry *dir,
					const char *dirname,
					bool *_is_new)
{
	struct dentry *subdir;
	struct path path;
	int ret;

	_enter(",,%s", dirname);

	 
	inode_lock_nested(d_inode(dir), I_MUTEX_PARENT);

retry:
	ret = cachefiles_inject_read_error();
	if (ret == 0)
		subdir = lookup_one_len(dirname, dir, strlen(dirname));
	else
		subdir = ERR_PTR(ret);
	trace_cachefiles_lookup(NULL, dir, subdir);
	if (IS_ERR(subdir)) {
		trace_cachefiles_vfs_error(NULL, d_backing_inode(dir),
					   PTR_ERR(subdir),
					   cachefiles_trace_lookup_error);
		if (PTR_ERR(subdir) == -ENOMEM)
			goto nomem_d_alloc;
		goto lookup_error;
	}

	_debug("subdir -> %pd %s",
	       subdir, d_backing_inode(subdir) ? "positive" : "negative");

	 
	if (d_is_negative(subdir)) {
		ret = cachefiles_has_space(cache, 1, 0,
					   cachefiles_has_space_for_create);
		if (ret < 0)
			goto mkdir_error;

		_debug("attempt mkdir");

		path.mnt = cache->mnt;
		path.dentry = dir;
		ret = security_path_mkdir(&path, subdir, 0700);
		if (ret < 0)
			goto mkdir_error;
		ret = cachefiles_inject_write_error();
		if (ret == 0)
			ret = vfs_mkdir(&nop_mnt_idmap, d_inode(dir), subdir, 0700);
		if (ret < 0) {
			trace_cachefiles_vfs_error(NULL, d_inode(dir), ret,
						   cachefiles_trace_mkdir_error);
			goto mkdir_error;
		}
		trace_cachefiles_mkdir(dir, subdir);

		if (unlikely(d_unhashed(subdir))) {
			cachefiles_put_directory(subdir);
			goto retry;
		}
		ASSERT(d_backing_inode(subdir));

		_debug("mkdir -> %pd{ino=%lu}",
		       subdir, d_backing_inode(subdir)->i_ino);
		if (_is_new)
			*_is_new = true;
	}

	 
	inode_lock(d_inode(subdir));
	inode_unlock(d_inode(dir));

	if (!__cachefiles_mark_inode_in_use(NULL, d_inode(subdir))) {
		pr_notice("cachefiles: Inode already in use: %pd (B=%lx)\n",
			  subdir, d_inode(subdir)->i_ino);
		goto mark_error;
	}

	inode_unlock(d_inode(subdir));

	 
	ASSERT(d_backing_inode(subdir));

	if (!d_can_lookup(subdir)) {
		pr_err("%s is not a directory\n", dirname);
		ret = -EIO;
		goto check_error;
	}

	ret = -EPERM;
	if (!(d_backing_inode(subdir)->i_opflags & IOP_XATTR) ||
	    !d_backing_inode(subdir)->i_op->lookup ||
	    !d_backing_inode(subdir)->i_op->mkdir ||
	    !d_backing_inode(subdir)->i_op->rename ||
	    !d_backing_inode(subdir)->i_op->rmdir ||
	    !d_backing_inode(subdir)->i_op->unlink)
		goto check_error;

	_leave(" = [%lu]", d_backing_inode(subdir)->i_ino);
	return subdir;

check_error:
	cachefiles_put_directory(subdir);
	_leave(" = %d [check]", ret);
	return ERR_PTR(ret);

mark_error:
	inode_unlock(d_inode(subdir));
	dput(subdir);
	return ERR_PTR(-EBUSY);

mkdir_error:
	inode_unlock(d_inode(dir));
	dput(subdir);
	pr_err("mkdir %s failed with error %d\n", dirname, ret);
	return ERR_PTR(ret);

lookup_error:
	inode_unlock(d_inode(dir));
	ret = PTR_ERR(subdir);
	pr_err("Lookup %s failed with error %d\n", dirname, ret);
	return ERR_PTR(ret);

nomem_d_alloc:
	inode_unlock(d_inode(dir));
	_leave(" = -ENOMEM");
	return ERR_PTR(-ENOMEM);
}

 
void cachefiles_put_directory(struct dentry *dir)
{
	if (dir) {
		cachefiles_do_unmark_inode_in_use(NULL, d_inode(dir));
		dput(dir);
	}
}

 
static int cachefiles_unlink(struct cachefiles_cache *cache,
			     struct cachefiles_object *object,
			     struct dentry *dir, struct dentry *dentry,
			     enum fscache_why_object_killed why)
{
	struct path path = {
		.mnt	= cache->mnt,
		.dentry	= dir,
	};
	int ret;

	trace_cachefiles_unlink(object, d_inode(dentry)->i_ino, why);
	ret = security_path_unlink(&path, dentry);
	if (ret < 0) {
		cachefiles_io_error(cache, "Unlink security error");
		return ret;
	}

	ret = cachefiles_inject_remove_error();
	if (ret == 0) {
		ret = vfs_unlink(&nop_mnt_idmap, d_backing_inode(dir), dentry, NULL);
		if (ret == -EIO)
			cachefiles_io_error(cache, "Unlink failed");
	}
	if (ret != 0)
		trace_cachefiles_vfs_error(object, d_backing_inode(dir), ret,
					   cachefiles_trace_unlink_error);
	return ret;
}

 
int cachefiles_bury_object(struct cachefiles_cache *cache,
			   struct cachefiles_object *object,
			   struct dentry *dir,
			   struct dentry *rep,
			   enum fscache_why_object_killed why)
{
	struct dentry *grave, *trap;
	struct path path, path_to_graveyard;
	char nbuffer[8 + 8 + 1];
	int ret;

	_enter(",'%pd','%pd'", dir, rep);

	if (rep->d_parent != dir) {
		inode_unlock(d_inode(dir));
		_leave(" = -ESTALE");
		return -ESTALE;
	}

	 
	if (!d_is_dir(rep)) {
		dget(rep);  
		ret = cachefiles_unlink(cache, object, dir, rep, why);
		dput(rep);

		inode_unlock(d_inode(dir));
		_leave(" = %d", ret);
		return ret;
	}

	 
	_debug("move stale object to graveyard");
	inode_unlock(d_inode(dir));

try_again:
	 
	sprintf(nbuffer, "%08x%08x",
		(uint32_t) ktime_get_real_seconds(),
		(uint32_t) atomic_inc_return(&cache->gravecounter));

	 
	trap = lock_rename(cache->graveyard, dir);

	 
	if (rep->d_parent != dir || IS_DEADDIR(d_inode(rep))) {
		 
		unlock_rename(cache->graveyard, dir);
		_leave(" = 0 [culled?]");
		return 0;
	}

	if (!d_can_lookup(cache->graveyard)) {
		unlock_rename(cache->graveyard, dir);
		cachefiles_io_error(cache, "Graveyard no longer a directory");
		return -EIO;
	}

	if (trap == rep) {
		unlock_rename(cache->graveyard, dir);
		cachefiles_io_error(cache, "May not make directory loop");
		return -EIO;
	}

	if (d_mountpoint(rep)) {
		unlock_rename(cache->graveyard, dir);
		cachefiles_io_error(cache, "Mountpoint in cache");
		return -EIO;
	}

	grave = lookup_one_len(nbuffer, cache->graveyard, strlen(nbuffer));
	if (IS_ERR(grave)) {
		unlock_rename(cache->graveyard, dir);
		trace_cachefiles_vfs_error(object, d_inode(cache->graveyard),
					   PTR_ERR(grave),
					   cachefiles_trace_lookup_error);

		if (PTR_ERR(grave) == -ENOMEM) {
			_leave(" = -ENOMEM");
			return -ENOMEM;
		}

		cachefiles_io_error(cache, "Lookup error %ld", PTR_ERR(grave));
		return -EIO;
	}

	if (d_is_positive(grave)) {
		unlock_rename(cache->graveyard, dir);
		dput(grave);
		grave = NULL;
		cond_resched();
		goto try_again;
	}

	if (d_mountpoint(grave)) {
		unlock_rename(cache->graveyard, dir);
		dput(grave);
		cachefiles_io_error(cache, "Mountpoint in graveyard");
		return -EIO;
	}

	 
	if (trap == grave) {
		unlock_rename(cache->graveyard, dir);
		dput(grave);
		cachefiles_io_error(cache, "May not make directory loop");
		return -EIO;
	}

	 
	path.mnt = cache->mnt;
	path.dentry = dir;
	path_to_graveyard.mnt = cache->mnt;
	path_to_graveyard.dentry = cache->graveyard;
	ret = security_path_rename(&path, rep, &path_to_graveyard, grave, 0);
	if (ret < 0) {
		cachefiles_io_error(cache, "Rename security error %d", ret);
	} else {
		struct renamedata rd = {
			.old_mnt_idmap	= &nop_mnt_idmap,
			.old_dir	= d_inode(dir),
			.old_dentry	= rep,
			.new_mnt_idmap	= &nop_mnt_idmap,
			.new_dir	= d_inode(cache->graveyard),
			.new_dentry	= grave,
		};
		trace_cachefiles_rename(object, d_inode(rep)->i_ino, why);
		ret = cachefiles_inject_read_error();
		if (ret == 0)
			ret = vfs_rename(&rd);
		if (ret != 0)
			trace_cachefiles_vfs_error(object, d_inode(dir), ret,
						   cachefiles_trace_rename_error);
		if (ret != 0 && ret != -ENOMEM)
			cachefiles_io_error(cache,
					    "Rename failed with error %d", ret);
	}

	__cachefiles_unmark_inode_in_use(object, d_inode(rep));
	unlock_rename(cache->graveyard, dir);
	dput(grave);
	_leave(" = 0");
	return 0;
}

 
int cachefiles_delete_object(struct cachefiles_object *object,
			     enum fscache_why_object_killed why)
{
	struct cachefiles_volume *volume = object->volume;
	struct dentry *dentry = object->file->f_path.dentry;
	struct dentry *fan = volume->fanout[(u8)object->cookie->key_hash];
	int ret;

	_enter(",OBJ%x{%pD}", object->debug_id, object->file);

	 
	dget(dentry);

	inode_lock_nested(d_backing_inode(fan), I_MUTEX_PARENT);
	ret = cachefiles_unlink(volume->cache, object, fan, dentry, why);
	inode_unlock(d_backing_inode(fan));
	dput(dentry);
	return ret;
}

 
struct file *cachefiles_create_tmpfile(struct cachefiles_object *object)
{
	struct cachefiles_volume *volume = object->volume;
	struct cachefiles_cache *cache = volume->cache;
	const struct cred *saved_cred;
	struct dentry *fan = volume->fanout[(u8)object->cookie->key_hash];
	struct file *file;
	const struct path parentpath = { .mnt = cache->mnt, .dentry = fan };
	uint64_t ni_size;
	long ret;


	cachefiles_begin_secure(cache, &saved_cred);

	ret = cachefiles_inject_write_error();
	if (ret == 0) {
		file = kernel_tmpfile_open(&nop_mnt_idmap, &parentpath,
					   S_IFREG | 0600,
					   O_RDWR | O_LARGEFILE | O_DIRECT,
					   cache->cache_cred);
		ret = PTR_ERR_OR_ZERO(file);
	}
	if (ret) {
		trace_cachefiles_vfs_error(object, d_inode(fan), ret,
					   cachefiles_trace_tmpfile_error);
		if (ret == -EIO)
			cachefiles_io_error_obj(object, "Failed to create tmpfile");
		goto err;
	}

	trace_cachefiles_tmpfile(object, file_inode(file));

	 
	if (!cachefiles_mark_inode_in_use(object, file_inode(file)))
		WARN_ON(1);

	ret = cachefiles_ondemand_init_object(object);
	if (ret < 0)
		goto err_unuse;

	ni_size = object->cookie->object_size;
	ni_size = round_up(ni_size, CACHEFILES_DIO_BLOCK_SIZE);

	if (ni_size > 0) {
		trace_cachefiles_trunc(object, file_inode(file), 0, ni_size,
				       cachefiles_trunc_expand_tmpfile);
		ret = cachefiles_inject_write_error();
		if (ret == 0)
			ret = vfs_truncate(&file->f_path, ni_size);
		if (ret < 0) {
			trace_cachefiles_vfs_error(
				object, file_inode(file), ret,
				cachefiles_trace_trunc_error);
			goto err_unuse;
		}
	}

	ret = -EINVAL;
	if (unlikely(!file->f_op->read_iter) ||
	    unlikely(!file->f_op->write_iter)) {
		fput(file);
		pr_notice("Cache does not support read_iter and write_iter\n");
		goto err_unuse;
	}
out:
	cachefiles_end_secure(cache, saved_cred);
	return file;

err_unuse:
	cachefiles_do_unmark_inode_in_use(object, file_inode(file));
	fput(file);
err:
	file = ERR_PTR(ret);
	goto out;
}

 
static bool cachefiles_create_file(struct cachefiles_object *object)
{
	struct file *file;
	int ret;

	ret = cachefiles_has_space(object->volume->cache, 1, 0,
				   cachefiles_has_space_for_create);
	if (ret < 0)
		return false;

	file = cachefiles_create_tmpfile(object);
	if (IS_ERR(file))
		return false;

	set_bit(FSCACHE_COOKIE_NEEDS_UPDATE, &object->cookie->flags);
	set_bit(CACHEFILES_OBJECT_USING_TMPFILE, &object->flags);
	_debug("create -> %pD{ino=%lu}", file, file_inode(file)->i_ino);
	object->file = file;
	return true;
}

 
static bool cachefiles_open_file(struct cachefiles_object *object,
				 struct dentry *dentry)
{
	struct cachefiles_cache *cache = object->volume->cache;
	struct file *file;
	struct path path;
	int ret;

	_enter("%pd", dentry);

	if (!cachefiles_mark_inode_in_use(object, d_inode(dentry))) {
		pr_notice("cachefiles: Inode already in use: %pd (B=%lx)\n",
			  dentry, d_inode(dentry)->i_ino);
		return false;
	}

	 
	path.mnt = cache->mnt;
	path.dentry = dentry;
	file = kernel_file_open(&path, O_RDWR | O_LARGEFILE | O_DIRECT,
				d_backing_inode(dentry), cache->cache_cred);
	if (IS_ERR(file)) {
		trace_cachefiles_vfs_error(object, d_backing_inode(dentry),
					   PTR_ERR(file),
					   cachefiles_trace_open_error);
		goto error;
	}

	if (unlikely(!file->f_op->read_iter) ||
	    unlikely(!file->f_op->write_iter)) {
		pr_notice("Cache does not support read_iter and write_iter\n");
		goto error_fput;
	}
	_debug("file -> %pd positive", dentry);

	ret = cachefiles_ondemand_init_object(object);
	if (ret < 0)
		goto error_fput;

	ret = cachefiles_check_auxdata(object, file);
	if (ret < 0)
		goto check_failed;

	clear_bit(FSCACHE_COOKIE_NO_DATA_TO_READ, &object->cookie->flags);

	object->file = file;

	 
	touch_atime(&file->f_path);
	dput(dentry);
	return true;

check_failed:
	fscache_cookie_lookup_negative(object->cookie);
	cachefiles_unmark_inode_in_use(object, file);
	fput(file);
	dput(dentry);
	if (ret == -ESTALE)
		return cachefiles_create_file(object);
	return false;

error_fput:
	fput(file);
error:
	cachefiles_do_unmark_inode_in_use(object, d_inode(dentry));
	dput(dentry);
	return false;
}

 
bool cachefiles_look_up_object(struct cachefiles_object *object)
{
	struct cachefiles_volume *volume = object->volume;
	struct dentry *dentry, *fan = volume->fanout[(u8)object->cookie->key_hash];
	int ret;

	_enter("OBJ%x,%s,", object->debug_id, object->d_name);

	 
	ret = cachefiles_inject_read_error();
	if (ret == 0)
		dentry = lookup_positive_unlocked(object->d_name, fan,
						  object->d_name_len);
	else
		dentry = ERR_PTR(ret);
	trace_cachefiles_lookup(object, fan, dentry);
	if (IS_ERR(dentry)) {
		if (dentry == ERR_PTR(-ENOENT))
			goto new_file;
		if (dentry == ERR_PTR(-EIO))
			cachefiles_io_error_obj(object, "Lookup failed");
		return false;
	}

	if (!d_is_reg(dentry)) {
		pr_err("%pd is not a file\n", dentry);
		inode_lock_nested(d_inode(fan), I_MUTEX_PARENT);
		ret = cachefiles_bury_object(volume->cache, object, fan, dentry,
					     FSCACHE_OBJECT_IS_WEIRD);
		dput(dentry);
		if (ret < 0)
			return false;
		goto new_file;
	}

	if (!cachefiles_open_file(object, dentry))
		return false;

	_leave(" = t [%lu]", file_inode(object->file)->i_ino);
	return true;

new_file:
	fscache_cookie_lookup_negative(object->cookie);
	return cachefiles_create_file(object);
}

 
bool cachefiles_commit_tmpfile(struct cachefiles_cache *cache,
			       struct cachefiles_object *object)
{
	struct cachefiles_volume *volume = object->volume;
	struct dentry *dentry, *fan = volume->fanout[(u8)object->cookie->key_hash];
	bool success = false;
	int ret;

	_enter(",%pD", object->file);

	inode_lock_nested(d_inode(fan), I_MUTEX_PARENT);
	ret = cachefiles_inject_read_error();
	if (ret == 0)
		dentry = lookup_one_len(object->d_name, fan, object->d_name_len);
	else
		dentry = ERR_PTR(ret);
	if (IS_ERR(dentry)) {
		trace_cachefiles_vfs_error(object, d_inode(fan), PTR_ERR(dentry),
					   cachefiles_trace_lookup_error);
		_debug("lookup fail %ld", PTR_ERR(dentry));
		goto out_unlock;
	}

	if (!d_is_negative(dentry)) {
		if (d_backing_inode(dentry) == file_inode(object->file)) {
			success = true;
			goto out_dput;
		}

		ret = cachefiles_unlink(volume->cache, object, fan, dentry,
					FSCACHE_OBJECT_IS_STALE);
		if (ret < 0)
			goto out_dput;

		dput(dentry);
		ret = cachefiles_inject_read_error();
		if (ret == 0)
			dentry = lookup_one_len(object->d_name, fan, object->d_name_len);
		else
			dentry = ERR_PTR(ret);
		if (IS_ERR(dentry)) {
			trace_cachefiles_vfs_error(object, d_inode(fan), PTR_ERR(dentry),
						   cachefiles_trace_lookup_error);
			_debug("lookup fail %ld", PTR_ERR(dentry));
			goto out_unlock;
		}
	}

	ret = cachefiles_inject_read_error();
	if (ret == 0)
		ret = vfs_link(object->file->f_path.dentry, &nop_mnt_idmap,
			       d_inode(fan), dentry, NULL);
	if (ret < 0) {
		trace_cachefiles_vfs_error(object, d_inode(fan), ret,
					   cachefiles_trace_link_error);
		_debug("link fail %d", ret);
	} else {
		trace_cachefiles_link(object, file_inode(object->file));
		spin_lock(&object->lock);
		 
		clear_bit(CACHEFILES_OBJECT_USING_TMPFILE, &object->flags);
		spin_unlock(&object->lock);
		success = true;
	}

out_dput:
	dput(dentry);
out_unlock:
	inode_unlock(d_inode(fan));
	_leave(" = %u", success);
	return success;
}

 
static struct dentry *cachefiles_lookup_for_cull(struct cachefiles_cache *cache,
						 struct dentry *dir,
						 char *filename)
{
	struct dentry *victim;
	int ret = -ENOENT;

	inode_lock_nested(d_inode(dir), I_MUTEX_PARENT);

	victim = lookup_one_len(filename, dir, strlen(filename));
	if (IS_ERR(victim))
		goto lookup_error;
	if (d_is_negative(victim))
		goto lookup_put;
	if (d_inode(victim)->i_flags & S_KERNEL_FILE)
		goto lookup_busy;
	return victim;

lookup_busy:
	ret = -EBUSY;
lookup_put:
	inode_unlock(d_inode(dir));
	dput(victim);
	return ERR_PTR(ret);

lookup_error:
	inode_unlock(d_inode(dir));
	ret = PTR_ERR(victim);
	if (ret == -ENOENT)
		return ERR_PTR(-ESTALE);  

	if (ret == -EIO) {
		cachefiles_io_error(cache, "Lookup failed");
	} else if (ret != -ENOMEM) {
		pr_err("Internal error: %d\n", ret);
		ret = -EIO;
	}

	return ERR_PTR(ret);
}

 
int cachefiles_cull(struct cachefiles_cache *cache, struct dentry *dir,
		    char *filename)
{
	struct dentry *victim;
	struct inode *inode;
	int ret;

	_enter(",%pd/,%s", dir, filename);

	victim = cachefiles_lookup_for_cull(cache, dir, filename);
	if (IS_ERR(victim))
		return PTR_ERR(victim);

	 
	inode = d_inode(victim);
	inode_lock(inode);
	if (inode->i_flags & S_KERNEL_FILE) {
		ret = -EBUSY;
	} else {
		 
		inode->i_flags |= S_KERNEL_FILE;
		ret = 0;
	}
	inode_unlock(inode);
	if (ret < 0)
		goto error_unlock;

	ret = cachefiles_bury_object(cache, NULL, dir, victim,
				     FSCACHE_OBJECT_WAS_CULLED);
	if (ret < 0)
		goto error;

	fscache_count_culled();
	dput(victim);
	_leave(" = 0");
	return 0;

error_unlock:
	inode_unlock(d_inode(dir));
error:
	dput(victim);
	if (ret == -ENOENT)
		return -ESTALE;  

	if (ret != -ENOMEM) {
		pr_err("Internal error: %d\n", ret);
		ret = -EIO;
	}

	_leave(" = %d", ret);
	return ret;
}

 
int cachefiles_check_in_use(struct cachefiles_cache *cache, struct dentry *dir,
			    char *filename)
{
	struct dentry *victim;
	int ret = 0;

	victim = cachefiles_lookup_for_cull(cache, dir, filename);
	if (IS_ERR(victim))
		return PTR_ERR(victim);

	inode_unlock(d_inode(dir));
	dput(victim);
	return ret;
}
