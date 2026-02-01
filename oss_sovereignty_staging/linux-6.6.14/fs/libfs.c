
 

#include <linux/blkdev.h>
#include <linux/export.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/cred.h>
#include <linux/mount.h>
#include <linux/vfs.h>
#include <linux/quotaops.h>
#include <linux/mutex.h>
#include <linux/namei.h>
#include <linux/exportfs.h>
#include <linux/iversion.h>
#include <linux/writeback.h>
#include <linux/buffer_head.h>  
#include <linux/fs_context.h>
#include <linux/pseudo_fs.h>
#include <linux/fsnotify.h>
#include <linux/unicode.h>
#include <linux/fscrypt.h>

#include <linux/uaccess.h>

#include "internal.h"

int simple_getattr(struct mnt_idmap *idmap, const struct path *path,
		   struct kstat *stat, u32 request_mask,
		   unsigned int query_flags)
{
	struct inode *inode = d_inode(path->dentry);
	generic_fillattr(&nop_mnt_idmap, request_mask, inode, stat);
	stat->blocks = inode->i_mapping->nrpages << (PAGE_SHIFT - 9);
	return 0;
}
EXPORT_SYMBOL(simple_getattr);

int simple_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	buf->f_type = dentry->d_sb->s_magic;
	buf->f_bsize = PAGE_SIZE;
	buf->f_namelen = NAME_MAX;
	return 0;
}
EXPORT_SYMBOL(simple_statfs);

 
int always_delete_dentry(const struct dentry *dentry)
{
	return 1;
}
EXPORT_SYMBOL(always_delete_dentry);

const struct dentry_operations simple_dentry_operations = {
	.d_delete = always_delete_dentry,
};
EXPORT_SYMBOL(simple_dentry_operations);

 
struct dentry *simple_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)
{
	if (dentry->d_name.len > NAME_MAX)
		return ERR_PTR(-ENAMETOOLONG);
	if (!dentry->d_sb->s_d_op)
		d_set_d_op(dentry, &simple_dentry_operations);
	d_add(dentry, NULL);
	return NULL;
}
EXPORT_SYMBOL(simple_lookup);

int dcache_dir_open(struct inode *inode, struct file *file)
{
	file->private_data = d_alloc_cursor(file->f_path.dentry);

	return file->private_data ? 0 : -ENOMEM;
}
EXPORT_SYMBOL(dcache_dir_open);

int dcache_dir_close(struct inode *inode, struct file *file)
{
	dput(file->private_data);
	return 0;
}
EXPORT_SYMBOL(dcache_dir_close);

 
 
static struct dentry *scan_positives(struct dentry *cursor,
					struct list_head *p,
					loff_t count,
					struct dentry *last)
{
	struct dentry *dentry = cursor->d_parent, *found = NULL;

	spin_lock(&dentry->d_lock);
	while ((p = p->next) != &dentry->d_subdirs) {
		struct dentry *d = list_entry(p, struct dentry, d_child);
		
		if (d->d_flags & DCACHE_DENTRY_CURSOR)
			continue;
		if (simple_positive(d) && !--count) {
			spin_lock_nested(&d->d_lock, DENTRY_D_LOCK_NESTED);
			if (simple_positive(d))
				found = dget_dlock(d);
			spin_unlock(&d->d_lock);
			if (likely(found))
				break;
			count = 1;
		}
		if (need_resched()) {
			list_move(&cursor->d_child, p);
			p = &cursor->d_child;
			spin_unlock(&dentry->d_lock);
			cond_resched();
			spin_lock(&dentry->d_lock);
		}
	}
	spin_unlock(&dentry->d_lock);
	dput(last);
	return found;
}

loff_t dcache_dir_lseek(struct file *file, loff_t offset, int whence)
{
	struct dentry *dentry = file->f_path.dentry;
	switch (whence) {
		case 1:
			offset += file->f_pos;
			fallthrough;
		case 0:
			if (offset >= 0)
				break;
			fallthrough;
		default:
			return -EINVAL;
	}
	if (offset != file->f_pos) {
		struct dentry *cursor = file->private_data;
		struct dentry *to = NULL;

		inode_lock_shared(dentry->d_inode);

		if (offset > 2)
			to = scan_positives(cursor, &dentry->d_subdirs,
					    offset - 2, NULL);
		spin_lock(&dentry->d_lock);
		if (to)
			list_move(&cursor->d_child, &to->d_child);
		else
			list_del_init(&cursor->d_child);
		spin_unlock(&dentry->d_lock);
		dput(to);

		file->f_pos = offset;

		inode_unlock_shared(dentry->d_inode);
	}
	return offset;
}
EXPORT_SYMBOL(dcache_dir_lseek);

 

int dcache_readdir(struct file *file, struct dir_context *ctx)
{
	struct dentry *dentry = file->f_path.dentry;
	struct dentry *cursor = file->private_data;
	struct list_head *anchor = &dentry->d_subdirs;
	struct dentry *next = NULL;
	struct list_head *p;

	if (!dir_emit_dots(file, ctx))
		return 0;

	if (ctx->pos == 2)
		p = anchor;
	else if (!list_empty(&cursor->d_child))
		p = &cursor->d_child;
	else
		return 0;

	while ((next = scan_positives(cursor, p, 1, next)) != NULL) {
		if (!dir_emit(ctx, next->d_name.name, next->d_name.len,
			      d_inode(next)->i_ino,
			      fs_umode_to_dtype(d_inode(next)->i_mode)))
			break;
		ctx->pos++;
		p = &next->d_child;
	}
	spin_lock(&dentry->d_lock);
	if (next)
		list_move_tail(&cursor->d_child, &next->d_child);
	else
		list_del_init(&cursor->d_child);
	spin_unlock(&dentry->d_lock);
	dput(next);

	return 0;
}
EXPORT_SYMBOL(dcache_readdir);

ssize_t generic_read_dir(struct file *filp, char __user *buf, size_t siz, loff_t *ppos)
{
	return -EISDIR;
}
EXPORT_SYMBOL(generic_read_dir);

const struct file_operations simple_dir_operations = {
	.open		= dcache_dir_open,
	.release	= dcache_dir_close,
	.llseek		= dcache_dir_lseek,
	.read		= generic_read_dir,
	.iterate_shared	= dcache_readdir,
	.fsync		= noop_fsync,
};
EXPORT_SYMBOL(simple_dir_operations);

const struct inode_operations simple_dir_inode_operations = {
	.lookup		= simple_lookup,
};
EXPORT_SYMBOL(simple_dir_inode_operations);

static void offset_set(struct dentry *dentry, u32 offset)
{
	dentry->d_fsdata = (void *)((uintptr_t)(offset));
}

static u32 dentry2offset(struct dentry *dentry)
{
	return (u32)((uintptr_t)(dentry->d_fsdata));
}

static struct lock_class_key simple_offset_xa_lock;

 
void simple_offset_init(struct offset_ctx *octx)
{
	xa_init_flags(&octx->xa, XA_FLAGS_ALLOC1);
	lockdep_set_class(&octx->xa.xa_lock, &simple_offset_xa_lock);

	 
	octx->next_offset = 2;
}

 
int simple_offset_add(struct offset_ctx *octx, struct dentry *dentry)
{
	static const struct xa_limit limit = XA_LIMIT(2, U32_MAX);
	u32 offset;
	int ret;

	if (dentry2offset(dentry) != 0)
		return -EBUSY;

	ret = xa_alloc_cyclic(&octx->xa, &offset, dentry, limit,
			      &octx->next_offset, GFP_KERNEL);
	if (ret < 0)
		return ret;

	offset_set(dentry, offset);
	return 0;
}

 
void simple_offset_remove(struct offset_ctx *octx, struct dentry *dentry)
{
	u32 offset;

	offset = dentry2offset(dentry);
	if (offset == 0)
		return;

	xa_erase(&octx->xa, offset);
	offset_set(dentry, 0);
}

 
int simple_offset_rename_exchange(struct inode *old_dir,
				  struct dentry *old_dentry,
				  struct inode *new_dir,
				  struct dentry *new_dentry)
{
	struct offset_ctx *old_ctx = old_dir->i_op->get_offset_ctx(old_dir);
	struct offset_ctx *new_ctx = new_dir->i_op->get_offset_ctx(new_dir);
	u32 old_index = dentry2offset(old_dentry);
	u32 new_index = dentry2offset(new_dentry);
	int ret;

	simple_offset_remove(old_ctx, old_dentry);
	simple_offset_remove(new_ctx, new_dentry);

	ret = simple_offset_add(new_ctx, old_dentry);
	if (ret)
		goto out_restore;

	ret = simple_offset_add(old_ctx, new_dentry);
	if (ret) {
		simple_offset_remove(new_ctx, old_dentry);
		goto out_restore;
	}

	ret = simple_rename_exchange(old_dir, old_dentry, new_dir, new_dentry);
	if (ret) {
		simple_offset_remove(new_ctx, old_dentry);
		simple_offset_remove(old_ctx, new_dentry);
		goto out_restore;
	}
	return 0;

out_restore:
	offset_set(old_dentry, old_index);
	xa_store(&old_ctx->xa, old_index, old_dentry, GFP_KERNEL);
	offset_set(new_dentry, new_index);
	xa_store(&new_ctx->xa, new_index, new_dentry, GFP_KERNEL);
	return ret;
}

 
void simple_offset_destroy(struct offset_ctx *octx)
{
	xa_destroy(&octx->xa);
}

 
static loff_t offset_dir_llseek(struct file *file, loff_t offset, int whence)
{
	switch (whence) {
	case SEEK_CUR:
		offset += file->f_pos;
		fallthrough;
	case SEEK_SET:
		if (offset >= 0)
			break;
		fallthrough;
	default:
		return -EINVAL;
	}

	 
	file->private_data = NULL;
	return vfs_setpos(file, offset, U32_MAX);
}

static struct dentry *offset_find_next(struct xa_state *xas)
{
	struct dentry *child, *found = NULL;

	rcu_read_lock();
	child = xas_next_entry(xas, U32_MAX);
	if (!child)
		goto out;
	spin_lock(&child->d_lock);
	if (simple_positive(child))
		found = dget_dlock(child);
	spin_unlock(&child->d_lock);
out:
	rcu_read_unlock();
	return found;
}

static bool offset_dir_emit(struct dir_context *ctx, struct dentry *dentry)
{
	u32 offset = dentry2offset(dentry);
	struct inode *inode = d_inode(dentry);

	return ctx->actor(ctx, dentry->d_name.name, dentry->d_name.len, offset,
			  inode->i_ino, fs_umode_to_dtype(inode->i_mode));
}

static void *offset_iterate_dir(struct inode *inode, struct dir_context *ctx)
{
	struct offset_ctx *so_ctx = inode->i_op->get_offset_ctx(inode);
	XA_STATE(xas, &so_ctx->xa, ctx->pos);
	struct dentry *dentry;

	while (true) {
		dentry = offset_find_next(&xas);
		if (!dentry)
			return ERR_PTR(-ENOENT);

		if (!offset_dir_emit(ctx, dentry)) {
			dput(dentry);
			break;
		}

		dput(dentry);
		ctx->pos = xas.xa_index + 1;
	}
	return NULL;
}

 
static int offset_readdir(struct file *file, struct dir_context *ctx)
{
	struct dentry *dir = file->f_path.dentry;

	lockdep_assert_held(&d_inode(dir)->i_rwsem);

	if (!dir_emit_dots(file, ctx))
		return 0;

	 
	if (ctx->pos == 2)
		file->private_data = NULL;
	else if (file->private_data == ERR_PTR(-ENOENT))
		return 0;
	file->private_data = offset_iterate_dir(d_inode(dir), ctx);
	return 0;
}

const struct file_operations simple_offset_dir_operations = {
	.llseek		= offset_dir_llseek,
	.iterate_shared	= offset_readdir,
	.read		= generic_read_dir,
	.fsync		= noop_fsync,
};

static struct dentry *find_next_child(struct dentry *parent, struct dentry *prev)
{
	struct dentry *child = NULL;
	struct list_head *p = prev ? &prev->d_child : &parent->d_subdirs;

	spin_lock(&parent->d_lock);
	while ((p = p->next) != &parent->d_subdirs) {
		struct dentry *d = container_of(p, struct dentry, d_child);
		if (simple_positive(d)) {
			spin_lock_nested(&d->d_lock, DENTRY_D_LOCK_NESTED);
			if (simple_positive(d))
				child = dget_dlock(d);
			spin_unlock(&d->d_lock);
			if (likely(child))
				break;
		}
	}
	spin_unlock(&parent->d_lock);
	dput(prev);
	return child;
}

void simple_recursive_removal(struct dentry *dentry,
                              void (*callback)(struct dentry *))
{
	struct dentry *this = dget(dentry);
	while (true) {
		struct dentry *victim = NULL, *child;
		struct inode *inode = this->d_inode;

		inode_lock(inode);
		if (d_is_dir(this))
			inode->i_flags |= S_DEAD;
		while ((child = find_next_child(this, victim)) == NULL) {
			
			
			inode_set_ctime_current(inode);
			clear_nlink(inode);
			inode_unlock(inode);
			victim = this;
			this = this->d_parent;
			inode = this->d_inode;
			inode_lock(inode);
			if (simple_positive(victim)) {
				d_invalidate(victim);	
				if (d_is_dir(victim))
					fsnotify_rmdir(inode, victim);
				else
					fsnotify_unlink(inode, victim);
				if (callback)
					callback(victim);
				dput(victim);		
			}
			if (victim == dentry) {
				inode_set_mtime_to_ts(inode,
						      inode_set_ctime_current(inode));
				if (d_is_dir(dentry))
					drop_nlink(inode);
				inode_unlock(inode);
				dput(dentry);
				return;
			}
		}
		inode_unlock(inode);
		this = child;
	}
}
EXPORT_SYMBOL(simple_recursive_removal);

static const struct super_operations simple_super_operations = {
	.statfs		= simple_statfs,
};

static int pseudo_fs_fill_super(struct super_block *s, struct fs_context *fc)
{
	struct pseudo_fs_context *ctx = fc->fs_private;
	struct inode *root;

	s->s_maxbytes = MAX_LFS_FILESIZE;
	s->s_blocksize = PAGE_SIZE;
	s->s_blocksize_bits = PAGE_SHIFT;
	s->s_magic = ctx->magic;
	s->s_op = ctx->ops ?: &simple_super_operations;
	s->s_xattr = ctx->xattr;
	s->s_time_gran = 1;
	root = new_inode(s);
	if (!root)
		return -ENOMEM;

	 
	root->i_ino = 1;
	root->i_mode = S_IFDIR | S_IRUSR | S_IWUSR;
	simple_inode_init_ts(root);
	s->s_root = d_make_root(root);
	if (!s->s_root)
		return -ENOMEM;
	s->s_d_op = ctx->dops;
	return 0;
}

static int pseudo_fs_get_tree(struct fs_context *fc)
{
	return get_tree_nodev(fc, pseudo_fs_fill_super);
}

static void pseudo_fs_free(struct fs_context *fc)
{
	kfree(fc->fs_private);
}

static const struct fs_context_operations pseudo_fs_context_ops = {
	.free		= pseudo_fs_free,
	.get_tree	= pseudo_fs_get_tree,
};

 
struct pseudo_fs_context *init_pseudo(struct fs_context *fc,
					unsigned long magic)
{
	struct pseudo_fs_context *ctx;

	ctx = kzalloc(sizeof(struct pseudo_fs_context), GFP_KERNEL);
	if (likely(ctx)) {
		ctx->magic = magic;
		fc->fs_private = ctx;
		fc->ops = &pseudo_fs_context_ops;
		fc->sb_flags |= SB_NOUSER;
		fc->global = true;
	}
	return ctx;
}
EXPORT_SYMBOL(init_pseudo);

int simple_open(struct inode *inode, struct file *file)
{
	if (inode->i_private)
		file->private_data = inode->i_private;
	return 0;
}
EXPORT_SYMBOL(simple_open);

int simple_link(struct dentry *old_dentry, struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = d_inode(old_dentry);

	inode_set_mtime_to_ts(dir,
			      inode_set_ctime_to_ts(dir, inode_set_ctime_current(inode)));
	inc_nlink(inode);
	ihold(inode);
	dget(dentry);
	d_instantiate(dentry, inode);
	return 0;
}
EXPORT_SYMBOL(simple_link);

int simple_empty(struct dentry *dentry)
{
	struct dentry *child;
	int ret = 0;

	spin_lock(&dentry->d_lock);
	list_for_each_entry(child, &dentry->d_subdirs, d_child) {
		spin_lock_nested(&child->d_lock, DENTRY_D_LOCK_NESTED);
		if (simple_positive(child)) {
			spin_unlock(&child->d_lock);
			goto out;
		}
		spin_unlock(&child->d_lock);
	}
	ret = 1;
out:
	spin_unlock(&dentry->d_lock);
	return ret;
}
EXPORT_SYMBOL(simple_empty);

int simple_unlink(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);

	inode_set_mtime_to_ts(dir,
			      inode_set_ctime_to_ts(dir, inode_set_ctime_current(inode)));
	drop_nlink(inode);
	dput(dentry);
	return 0;
}
EXPORT_SYMBOL(simple_unlink);

int simple_rmdir(struct inode *dir, struct dentry *dentry)
{
	if (!simple_empty(dentry))
		return -ENOTEMPTY;

	drop_nlink(d_inode(dentry));
	simple_unlink(dir, dentry);
	drop_nlink(dir);
	return 0;
}
EXPORT_SYMBOL(simple_rmdir);

 
void simple_rename_timestamp(struct inode *old_dir, struct dentry *old_dentry,
			     struct inode *new_dir, struct dentry *new_dentry)
{
	struct inode *newino = d_inode(new_dentry);

	inode_set_mtime_to_ts(old_dir, inode_set_ctime_current(old_dir));
	if (new_dir != old_dir)
		inode_set_mtime_to_ts(new_dir,
				      inode_set_ctime_current(new_dir));
	inode_set_ctime_current(d_inode(old_dentry));
	if (newino)
		inode_set_ctime_current(newino);
}
EXPORT_SYMBOL_GPL(simple_rename_timestamp);

int simple_rename_exchange(struct inode *old_dir, struct dentry *old_dentry,
			   struct inode *new_dir, struct dentry *new_dentry)
{
	bool old_is_dir = d_is_dir(old_dentry);
	bool new_is_dir = d_is_dir(new_dentry);

	if (old_dir != new_dir && old_is_dir != new_is_dir) {
		if (old_is_dir) {
			drop_nlink(old_dir);
			inc_nlink(new_dir);
		} else {
			drop_nlink(new_dir);
			inc_nlink(old_dir);
		}
	}
	simple_rename_timestamp(old_dir, old_dentry, new_dir, new_dentry);
	return 0;
}
EXPORT_SYMBOL_GPL(simple_rename_exchange);

int simple_rename(struct mnt_idmap *idmap, struct inode *old_dir,
		  struct dentry *old_dentry, struct inode *new_dir,
		  struct dentry *new_dentry, unsigned int flags)
{
	int they_are_dirs = d_is_dir(old_dentry);

	if (flags & ~(RENAME_NOREPLACE | RENAME_EXCHANGE))
		return -EINVAL;

	if (flags & RENAME_EXCHANGE)
		return simple_rename_exchange(old_dir, old_dentry, new_dir, new_dentry);

	if (!simple_empty(new_dentry))
		return -ENOTEMPTY;

	if (d_really_is_positive(new_dentry)) {
		simple_unlink(new_dir, new_dentry);
		if (they_are_dirs) {
			drop_nlink(d_inode(new_dentry));
			drop_nlink(old_dir);
		}
	} else if (they_are_dirs) {
		drop_nlink(old_dir);
		inc_nlink(new_dir);
	}

	simple_rename_timestamp(old_dir, old_dentry, new_dir, new_dentry);
	return 0;
}
EXPORT_SYMBOL(simple_rename);

 
int simple_setattr(struct mnt_idmap *idmap, struct dentry *dentry,
		   struct iattr *iattr)
{
	struct inode *inode = d_inode(dentry);
	int error;

	error = setattr_prepare(idmap, dentry, iattr);
	if (error)
		return error;

	if (iattr->ia_valid & ATTR_SIZE)
		truncate_setsize(inode, iattr->ia_size);
	setattr_copy(idmap, inode, iattr);
	mark_inode_dirty(inode);
	return 0;
}
EXPORT_SYMBOL(simple_setattr);

static int simple_read_folio(struct file *file, struct folio *folio)
{
	folio_zero_range(folio, 0, folio_size(folio));
	flush_dcache_folio(folio);
	folio_mark_uptodate(folio);
	folio_unlock(folio);
	return 0;
}

int simple_write_begin(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned len,
			struct page **pagep, void **fsdata)
{
	struct folio *folio;

	folio = __filemap_get_folio(mapping, pos / PAGE_SIZE, FGP_WRITEBEGIN,
			mapping_gfp_mask(mapping));
	if (IS_ERR(folio))
		return PTR_ERR(folio);

	*pagep = &folio->page;

	if (!folio_test_uptodate(folio) && (len != folio_size(folio))) {
		size_t from = offset_in_folio(folio, pos);

		folio_zero_segments(folio, 0, from,
				from + len, folio_size(folio));
	}
	return 0;
}
EXPORT_SYMBOL(simple_write_begin);

 
static int simple_write_end(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned copied,
			struct page *page, void *fsdata)
{
	struct folio *folio = page_folio(page);
	struct inode *inode = folio->mapping->host;
	loff_t last_pos = pos + copied;

	 
	if (!folio_test_uptodate(folio)) {
		if (copied < len) {
			size_t from = offset_in_folio(folio, pos);

			folio_zero_range(folio, from + copied, len - copied);
		}
		folio_mark_uptodate(folio);
	}
	 
	if (last_pos > inode->i_size)
		i_size_write(inode, last_pos);

	folio_mark_dirty(folio);
	folio_unlock(folio);
	folio_put(folio);

	return copied;
}

 
const struct address_space_operations ram_aops = {
	.read_folio	= simple_read_folio,
	.write_begin	= simple_write_begin,
	.write_end	= simple_write_end,
	.dirty_folio	= noop_dirty_folio,
};
EXPORT_SYMBOL(ram_aops);

 
int simple_fill_super(struct super_block *s, unsigned long magic,
		      const struct tree_descr *files)
{
	struct inode *inode;
	struct dentry *root;
	struct dentry *dentry;
	int i;

	s->s_blocksize = PAGE_SIZE;
	s->s_blocksize_bits = PAGE_SHIFT;
	s->s_magic = magic;
	s->s_op = &simple_super_operations;
	s->s_time_gran = 1;

	inode = new_inode(s);
	if (!inode)
		return -ENOMEM;
	 
	inode->i_ino = 1;
	inode->i_mode = S_IFDIR | 0755;
	simple_inode_init_ts(inode);
	inode->i_op = &simple_dir_inode_operations;
	inode->i_fop = &simple_dir_operations;
	set_nlink(inode, 2);
	root = d_make_root(inode);
	if (!root)
		return -ENOMEM;
	for (i = 0; !files->name || files->name[0]; i++, files++) {
		if (!files->name)
			continue;

		 
		if (unlikely(i == 1))
			printk(KERN_WARNING "%s: %s passed in a files array"
				"with an index of 1!\n", __func__,
				s->s_type->name);

		dentry = d_alloc_name(root, files->name);
		if (!dentry)
			goto out;
		inode = new_inode(s);
		if (!inode) {
			dput(dentry);
			goto out;
		}
		inode->i_mode = S_IFREG | files->mode;
		simple_inode_init_ts(inode);
		inode->i_fop = files->ops;
		inode->i_ino = i;
		d_add(dentry, inode);
	}
	s->s_root = root;
	return 0;
out:
	d_genocide(root);
	shrink_dcache_parent(root);
	dput(root);
	return -ENOMEM;
}
EXPORT_SYMBOL(simple_fill_super);

static DEFINE_SPINLOCK(pin_fs_lock);

int simple_pin_fs(struct file_system_type *type, struct vfsmount **mount, int *count)
{
	struct vfsmount *mnt = NULL;
	spin_lock(&pin_fs_lock);
	if (unlikely(!*mount)) {
		spin_unlock(&pin_fs_lock);
		mnt = vfs_kern_mount(type, SB_KERNMOUNT, type->name, NULL);
		if (IS_ERR(mnt))
			return PTR_ERR(mnt);
		spin_lock(&pin_fs_lock);
		if (!*mount)
			*mount = mnt;
	}
	mntget(*mount);
	++*count;
	spin_unlock(&pin_fs_lock);
	mntput(mnt);
	return 0;
}
EXPORT_SYMBOL(simple_pin_fs);

void simple_release_fs(struct vfsmount **mount, int *count)
{
	struct vfsmount *mnt;
	spin_lock(&pin_fs_lock);
	mnt = *mount;
	if (!--*count)
		*mount = NULL;
	spin_unlock(&pin_fs_lock);
	mntput(mnt);
}
EXPORT_SYMBOL(simple_release_fs);

 
ssize_t simple_read_from_buffer(void __user *to, size_t count, loff_t *ppos,
				const void *from, size_t available)
{
	loff_t pos = *ppos;
	size_t ret;

	if (pos < 0)
		return -EINVAL;
	if (pos >= available || !count)
		return 0;
	if (count > available - pos)
		count = available - pos;
	ret = copy_to_user(to, from + pos, count);
	if (ret == count)
		return -EFAULT;
	count -= ret;
	*ppos = pos + count;
	return count;
}
EXPORT_SYMBOL(simple_read_from_buffer);

 
ssize_t simple_write_to_buffer(void *to, size_t available, loff_t *ppos,
		const void __user *from, size_t count)
{
	loff_t pos = *ppos;
	size_t res;

	if (pos < 0)
		return -EINVAL;
	if (pos >= available || !count)
		return 0;
	if (count > available - pos)
		count = available - pos;
	res = copy_from_user(to + pos, from, count);
	if (res == count)
		return -EFAULT;
	count -= res;
	*ppos = pos + count;
	return count;
}
EXPORT_SYMBOL(simple_write_to_buffer);

 
ssize_t memory_read_from_buffer(void *to, size_t count, loff_t *ppos,
				const void *from, size_t available)
{
	loff_t pos = *ppos;

	if (pos < 0)
		return -EINVAL;
	if (pos >= available)
		return 0;
	if (count > available - pos)
		count = available - pos;
	memcpy(to, from + pos, count);
	*ppos = pos + count;

	return count;
}
EXPORT_SYMBOL(memory_read_from_buffer);

 

void simple_transaction_set(struct file *file, size_t n)
{
	struct simple_transaction_argresp *ar = file->private_data;

	BUG_ON(n > SIMPLE_TRANSACTION_LIMIT);

	 
	smp_mb();
	ar->size = n;
}
EXPORT_SYMBOL(simple_transaction_set);

char *simple_transaction_get(struct file *file, const char __user *buf, size_t size)
{
	struct simple_transaction_argresp *ar;
	static DEFINE_SPINLOCK(simple_transaction_lock);

	if (size > SIMPLE_TRANSACTION_LIMIT - 1)
		return ERR_PTR(-EFBIG);

	ar = (struct simple_transaction_argresp *)get_zeroed_page(GFP_KERNEL);
	if (!ar)
		return ERR_PTR(-ENOMEM);

	spin_lock(&simple_transaction_lock);

	 
	if (file->private_data) {
		spin_unlock(&simple_transaction_lock);
		free_page((unsigned long)ar);
		return ERR_PTR(-EBUSY);
	}

	file->private_data = ar;

	spin_unlock(&simple_transaction_lock);

	if (copy_from_user(ar->data, buf, size))
		return ERR_PTR(-EFAULT);

	return ar->data;
}
EXPORT_SYMBOL(simple_transaction_get);

ssize_t simple_transaction_read(struct file *file, char __user *buf, size_t size, loff_t *pos)
{
	struct simple_transaction_argresp *ar = file->private_data;

	if (!ar)
		return 0;
	return simple_read_from_buffer(buf, size, pos, ar->data, ar->size);
}
EXPORT_SYMBOL(simple_transaction_read);

int simple_transaction_release(struct inode *inode, struct file *file)
{
	free_page((unsigned long)file->private_data);
	return 0;
}
EXPORT_SYMBOL(simple_transaction_release);

 

struct simple_attr {
	int (*get)(void *, u64 *);
	int (*set)(void *, u64);
	char get_buf[24];	 
	char set_buf[24];
	void *data;
	const char *fmt;	 
	struct mutex mutex;	 
};

 
int simple_attr_open(struct inode *inode, struct file *file,
		     int (*get)(void *, u64 *), int (*set)(void *, u64),
		     const char *fmt)
{
	struct simple_attr *attr;

	attr = kzalloc(sizeof(*attr), GFP_KERNEL);
	if (!attr)
		return -ENOMEM;

	attr->get = get;
	attr->set = set;
	attr->data = inode->i_private;
	attr->fmt = fmt;
	mutex_init(&attr->mutex);

	file->private_data = attr;

	return nonseekable_open(inode, file);
}
EXPORT_SYMBOL_GPL(simple_attr_open);

int simple_attr_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}
EXPORT_SYMBOL_GPL(simple_attr_release);	 

 
ssize_t simple_attr_read(struct file *file, char __user *buf,
			 size_t len, loff_t *ppos)
{
	struct simple_attr *attr;
	size_t size;
	ssize_t ret;

	attr = file->private_data;

	if (!attr->get)
		return -EACCES;

	ret = mutex_lock_interruptible(&attr->mutex);
	if (ret)
		return ret;

	if (*ppos && attr->get_buf[0]) {
		 
		size = strlen(attr->get_buf);
	} else {
		 
		u64 val;
		ret = attr->get(attr->data, &val);
		if (ret)
			goto out;

		size = scnprintf(attr->get_buf, sizeof(attr->get_buf),
				 attr->fmt, (unsigned long long)val);
	}

	ret = simple_read_from_buffer(buf, len, ppos, attr->get_buf, size);
out:
	mutex_unlock(&attr->mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(simple_attr_read);

 
static ssize_t simple_attr_write_xsigned(struct file *file, const char __user *buf,
			  size_t len, loff_t *ppos, bool is_signed)
{
	struct simple_attr *attr;
	unsigned long long val;
	size_t size;
	ssize_t ret;

	attr = file->private_data;
	if (!attr->set)
		return -EACCES;

	ret = mutex_lock_interruptible(&attr->mutex);
	if (ret)
		return ret;

	ret = -EFAULT;
	size = min(sizeof(attr->set_buf) - 1, len);
	if (copy_from_user(attr->set_buf, buf, size))
		goto out;

	attr->set_buf[size] = '\0';
	if (is_signed)
		ret = kstrtoll(attr->set_buf, 0, &val);
	else
		ret = kstrtoull(attr->set_buf, 0, &val);
	if (ret)
		goto out;
	ret = attr->set(attr->data, val);
	if (ret == 0)
		ret = len;  
out:
	mutex_unlock(&attr->mutex);
	return ret;
}

ssize_t simple_attr_write(struct file *file, const char __user *buf,
			  size_t len, loff_t *ppos)
{
	return simple_attr_write_xsigned(file, buf, len, ppos, false);
}
EXPORT_SYMBOL_GPL(simple_attr_write);

ssize_t simple_attr_write_signed(struct file *file, const char __user *buf,
			  size_t len, loff_t *ppos)
{
	return simple_attr_write_xsigned(file, buf, len, ppos, true);
}
EXPORT_SYMBOL_GPL(simple_attr_write_signed);

 
struct dentry *generic_fh_to_dentry(struct super_block *sb, struct fid *fid,
		int fh_len, int fh_type, struct inode *(*get_inode)
			(struct super_block *sb, u64 ino, u32 gen))
{
	struct inode *inode = NULL;

	if (fh_len < 2)
		return NULL;

	switch (fh_type) {
	case FILEID_INO32_GEN:
	case FILEID_INO32_GEN_PARENT:
		inode = get_inode(sb, fid->i32.ino, fid->i32.gen);
		break;
	}

	return d_obtain_alias(inode);
}
EXPORT_SYMBOL_GPL(generic_fh_to_dentry);

 
struct dentry *generic_fh_to_parent(struct super_block *sb, struct fid *fid,
		int fh_len, int fh_type, struct inode *(*get_inode)
			(struct super_block *sb, u64 ino, u32 gen))
{
	struct inode *inode = NULL;

	if (fh_len <= 2)
		return NULL;

	switch (fh_type) {
	case FILEID_INO32_GEN_PARENT:
		inode = get_inode(sb, fid->i32.parent_ino,
				  (fh_len > 3 ? fid->i32.parent_gen : 0));
		break;
	}

	return d_obtain_alias(inode);
}
EXPORT_SYMBOL_GPL(generic_fh_to_parent);

 
int __generic_file_fsync(struct file *file, loff_t start, loff_t end,
				 int datasync)
{
	struct inode *inode = file->f_mapping->host;
	int err;
	int ret;

	err = file_write_and_wait_range(file, start, end);
	if (err)
		return err;

	inode_lock(inode);
	ret = sync_mapping_buffers(inode->i_mapping);
	if (!(inode->i_state & I_DIRTY_ALL))
		goto out;
	if (datasync && !(inode->i_state & I_DIRTY_DATASYNC))
		goto out;

	err = sync_inode_metadata(inode, 1);
	if (ret == 0)
		ret = err;

out:
	inode_unlock(inode);
	 
	err = file_check_and_advance_wb_err(file);
	if (ret == 0)
		ret = err;
	return ret;
}
EXPORT_SYMBOL(__generic_file_fsync);

 

int generic_file_fsync(struct file *file, loff_t start, loff_t end,
		       int datasync)
{
	struct inode *inode = file->f_mapping->host;
	int err;

	err = __generic_file_fsync(file, start, end, datasync);
	if (err)
		return err;
	return blkdev_issue_flush(inode->i_sb->s_bdev);
}
EXPORT_SYMBOL(generic_file_fsync);

 
int generic_check_addressable(unsigned blocksize_bits, u64 num_blocks)
{
	u64 last_fs_block = num_blocks - 1;
	u64 last_fs_page =
		last_fs_block >> (PAGE_SHIFT - blocksize_bits);

	if (unlikely(num_blocks == 0))
		return 0;

	if ((blocksize_bits < 9) || (blocksize_bits > PAGE_SHIFT))
		return -EINVAL;

	if ((last_fs_block > (sector_t)(~0ULL) >> (blocksize_bits - 9)) ||
	    (last_fs_page > (pgoff_t)(~0ULL))) {
		return -EFBIG;
	}
	return 0;
}
EXPORT_SYMBOL(generic_check_addressable);

 
int noop_fsync(struct file *file, loff_t start, loff_t end, int datasync)
{
	return 0;
}
EXPORT_SYMBOL(noop_fsync);

ssize_t noop_direct_IO(struct kiocb *iocb, struct iov_iter *iter)
{
	 
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(noop_direct_IO);

 
void kfree_link(void *p)
{
	kfree(p);
}
EXPORT_SYMBOL(kfree_link);

struct inode *alloc_anon_inode(struct super_block *s)
{
	static const struct address_space_operations anon_aops = {
		.dirty_folio	= noop_dirty_folio,
	};
	struct inode *inode = new_inode_pseudo(s);

	if (!inode)
		return ERR_PTR(-ENOMEM);

	inode->i_ino = get_next_ino();
	inode->i_mapping->a_ops = &anon_aops;

	 
	inode->i_state = I_DIRTY;
	inode->i_mode = S_IRUSR | S_IWUSR;
	inode->i_uid = current_fsuid();
	inode->i_gid = current_fsgid();
	inode->i_flags |= S_PRIVATE;
	simple_inode_init_ts(inode);
	return inode;
}
EXPORT_SYMBOL(alloc_anon_inode);

 
int
simple_nosetlease(struct file *filp, int arg, struct file_lock **flp,
		  void **priv)
{
	return -EINVAL;
}
EXPORT_SYMBOL(simple_nosetlease);

 
const char *simple_get_link(struct dentry *dentry, struct inode *inode,
			    struct delayed_call *done)
{
	return inode->i_link;
}
EXPORT_SYMBOL(simple_get_link);

const struct inode_operations simple_symlink_inode_operations = {
	.get_link = simple_get_link,
};
EXPORT_SYMBOL(simple_symlink_inode_operations);

 
static struct dentry *empty_dir_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)
{
	return ERR_PTR(-ENOENT);
}

static int empty_dir_getattr(struct mnt_idmap *idmap,
			     const struct path *path, struct kstat *stat,
			     u32 request_mask, unsigned int query_flags)
{
	struct inode *inode = d_inode(path->dentry);
	generic_fillattr(&nop_mnt_idmap, request_mask, inode, stat);
	return 0;
}

static int empty_dir_setattr(struct mnt_idmap *idmap,
			     struct dentry *dentry, struct iattr *attr)
{
	return -EPERM;
}

static ssize_t empty_dir_listxattr(struct dentry *dentry, char *list, size_t size)
{
	return -EOPNOTSUPP;
}

static const struct inode_operations empty_dir_inode_operations = {
	.lookup		= empty_dir_lookup,
	.permission	= generic_permission,
	.setattr	= empty_dir_setattr,
	.getattr	= empty_dir_getattr,
	.listxattr	= empty_dir_listxattr,
};

static loff_t empty_dir_llseek(struct file *file, loff_t offset, int whence)
{
	 
	return generic_file_llseek_size(file, offset, whence, 2, 2);
}

static int empty_dir_readdir(struct file *file, struct dir_context *ctx)
{
	dir_emit_dots(file, ctx);
	return 0;
}

static const struct file_operations empty_dir_operations = {
	.llseek		= empty_dir_llseek,
	.read		= generic_read_dir,
	.iterate_shared	= empty_dir_readdir,
	.fsync		= noop_fsync,
};


void make_empty_dir_inode(struct inode *inode)
{
	set_nlink(inode, 2);
	inode->i_mode = S_IFDIR | S_IRUGO | S_IXUGO;
	inode->i_uid = GLOBAL_ROOT_UID;
	inode->i_gid = GLOBAL_ROOT_GID;
	inode->i_rdev = 0;
	inode->i_size = 0;
	inode->i_blkbits = PAGE_SHIFT;
	inode->i_blocks = 0;

	inode->i_op = &empty_dir_inode_operations;
	inode->i_opflags &= ~IOP_XATTR;
	inode->i_fop = &empty_dir_operations;
}

bool is_empty_dir_inode(struct inode *inode)
{
	return (inode->i_fop == &empty_dir_operations) &&
		(inode->i_op == &empty_dir_inode_operations);
}

#if IS_ENABLED(CONFIG_UNICODE)
 
static int generic_ci_d_compare(const struct dentry *dentry, unsigned int len,
				const char *str, const struct qstr *name)
{
	const struct dentry *parent = READ_ONCE(dentry->d_parent);
	const struct inode *dir = READ_ONCE(parent->d_inode);
	const struct super_block *sb = dentry->d_sb;
	const struct unicode_map *um = sb->s_encoding;
	struct qstr qstr = QSTR_INIT(str, len);
	char strbuf[DNAME_INLINE_LEN];
	int ret;

	if (!dir || !IS_CASEFOLDED(dir))
		goto fallback;
	 
	if (len <= DNAME_INLINE_LEN - 1) {
		memcpy(strbuf, str, len);
		strbuf[len] = 0;
		qstr.name = strbuf;
		 
		barrier();
	}
	ret = utf8_strncasecmp(um, name, &qstr);
	if (ret >= 0)
		return ret;

	if (sb_has_strict_encoding(sb))
		return -EINVAL;
fallback:
	if (len != name->len)
		return 1;
	return !!memcmp(str, name->name, len);
}

 
static int generic_ci_d_hash(const struct dentry *dentry, struct qstr *str)
{
	const struct inode *dir = READ_ONCE(dentry->d_inode);
	struct super_block *sb = dentry->d_sb;
	const struct unicode_map *um = sb->s_encoding;
	int ret = 0;

	if (!dir || !IS_CASEFOLDED(dir))
		return 0;

	ret = utf8_casefold_hash(um, dentry, str);
	if (ret < 0 && sb_has_strict_encoding(sb))
		return -EINVAL;
	return 0;
}

static const struct dentry_operations generic_ci_dentry_ops = {
	.d_hash = generic_ci_d_hash,
	.d_compare = generic_ci_d_compare,
};
#endif

#ifdef CONFIG_FS_ENCRYPTION
static const struct dentry_operations generic_encrypted_dentry_ops = {
	.d_revalidate = fscrypt_d_revalidate,
};
#endif

#if defined(CONFIG_FS_ENCRYPTION) && IS_ENABLED(CONFIG_UNICODE)
static const struct dentry_operations generic_encrypted_ci_dentry_ops = {
	.d_hash = generic_ci_d_hash,
	.d_compare = generic_ci_d_compare,
	.d_revalidate = fscrypt_d_revalidate,
};
#endif

 
void generic_set_encrypted_ci_d_ops(struct dentry *dentry)
{
#ifdef CONFIG_FS_ENCRYPTION
	bool needs_encrypt_ops = dentry->d_flags & DCACHE_NOKEY_NAME;
#endif
#if IS_ENABLED(CONFIG_UNICODE)
	bool needs_ci_ops = dentry->d_sb->s_encoding;
#endif
#if defined(CONFIG_FS_ENCRYPTION) && IS_ENABLED(CONFIG_UNICODE)
	if (needs_encrypt_ops && needs_ci_ops) {
		d_set_d_op(dentry, &generic_encrypted_ci_dentry_ops);
		return;
	}
#endif
#ifdef CONFIG_FS_ENCRYPTION
	if (needs_encrypt_ops) {
		d_set_d_op(dentry, &generic_encrypted_dentry_ops);
		return;
	}
#endif
#if IS_ENABLED(CONFIG_UNICODE)
	if (needs_ci_ops) {
		d_set_d_op(dentry, &generic_ci_dentry_ops);
		return;
	}
#endif
}
EXPORT_SYMBOL(generic_set_encrypted_ci_d_ops);

 
bool inode_maybe_inc_iversion(struct inode *inode, bool force)
{
	u64 cur, new;

	 
	smp_mb();
	cur = inode_peek_iversion_raw(inode);
	do {
		 
		if (!force && !(cur & I_VERSION_QUERIED))
			return false;

		 
		new = (cur & ~I_VERSION_QUERIED) + I_VERSION_INCREMENT;
	} while (!atomic64_try_cmpxchg(&inode->i_version, &cur, new));
	return true;
}
EXPORT_SYMBOL(inode_maybe_inc_iversion);

 
u64 inode_query_iversion(struct inode *inode)
{
	u64 cur, new;

	cur = inode_peek_iversion_raw(inode);
	do {
		 
		if (cur & I_VERSION_QUERIED) {
			 
			smp_mb();
			break;
		}

		new = cur | I_VERSION_QUERIED;
	} while (!atomic64_try_cmpxchg(&inode->i_version, &cur, new));
	return cur >> I_VERSION_QUERIED_SHIFT;
}
EXPORT_SYMBOL(inode_query_iversion);

ssize_t direct_write_fallback(struct kiocb *iocb, struct iov_iter *iter,
		ssize_t direct_written, ssize_t buffered_written)
{
	struct address_space *mapping = iocb->ki_filp->f_mapping;
	loff_t pos = iocb->ki_pos - buffered_written;
	loff_t end = iocb->ki_pos - 1;
	int err;

	 
	if (unlikely(buffered_written < 0)) {
		if (direct_written)
			return direct_written;
		return buffered_written;
	}

	 
	err = filemap_write_and_wait_range(mapping, pos, end);
	if (err < 0) {
		 
		iocb->ki_pos -= buffered_written;
		if (direct_written)
			return direct_written;
		return err;
	}
	invalidate_mapping_pages(mapping, pos >> PAGE_SHIFT, end >> PAGE_SHIFT);
	return direct_written + buffered_written;
}
EXPORT_SYMBOL_GPL(direct_write_fallback);

 
struct timespec64 simple_inode_init_ts(struct inode *inode)
{
	struct timespec64 ts = inode_set_ctime_current(inode);

	inode_set_atime_to_ts(inode, ts);
	inode_set_mtime_to_ts(inode, ts);
	return ts;
}
EXPORT_SYMBOL(simple_inode_init_ts);
