
 

 

#include "reiserfs.h"
#include <linux/capability.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/errno.h>
#include <linux/gfp.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/pagemap.h>
#include <linux/xattr.h>
#include "xattr.h"
#include "acl.h"
#include <linux/uaccess.h>
#include <net/checksum.h>
#include <linux/stat.h>
#include <linux/quotaops.h>
#include <linux/security.h>
#include <linux/posix_acl_xattr.h>
#include <linux/xattr.h>

#define PRIVROOT_NAME ".reiserfs_priv"
#define XAROOT_NAME   "xattrs"


 
#ifdef CONFIG_REISERFS_FS_XATTR
static int xattr_create(struct inode *dir, struct dentry *dentry, int mode)
{
	BUG_ON(!inode_is_locked(dir));
	return dir->i_op->create(&nop_mnt_idmap, dir, dentry, mode, true);
}
#endif

static int xattr_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	BUG_ON(!inode_is_locked(dir));
	return dir->i_op->mkdir(&nop_mnt_idmap, dir, dentry, mode);
}

 
static int xattr_unlink(struct inode *dir, struct dentry *dentry)
{
	int error;

	BUG_ON(!inode_is_locked(dir));

	inode_lock_nested(d_inode(dentry), I_MUTEX_CHILD);
	error = dir->i_op->unlink(dir, dentry);
	inode_unlock(d_inode(dentry));

	if (!error)
		d_delete(dentry);
	return error;
}

static int xattr_rmdir(struct inode *dir, struct dentry *dentry)
{
	int error;

	BUG_ON(!inode_is_locked(dir));

	inode_lock_nested(d_inode(dentry), I_MUTEX_CHILD);
	error = dir->i_op->rmdir(dir, dentry);
	if (!error)
		d_inode(dentry)->i_flags |= S_DEAD;
	inode_unlock(d_inode(dentry));
	if (!error)
		d_delete(dentry);

	return error;
}

#define xattr_may_create(flags)	(!flags || flags & XATTR_CREATE)

static struct dentry *open_xa_root(struct super_block *sb, int flags)
{
	struct dentry *privroot = REISERFS_SB(sb)->priv_root;
	struct dentry *xaroot;

	if (d_really_is_negative(privroot))
		return ERR_PTR(-EOPNOTSUPP);

	inode_lock_nested(d_inode(privroot), I_MUTEX_XATTR);

	xaroot = dget(REISERFS_SB(sb)->xattr_root);
	if (!xaroot)
		xaroot = ERR_PTR(-EOPNOTSUPP);
	else if (d_really_is_negative(xaroot)) {
		int err = -ENODATA;

		if (xattr_may_create(flags))
			err = xattr_mkdir(d_inode(privroot), xaroot, 0700);
		if (err) {
			dput(xaroot);
			xaroot = ERR_PTR(err);
		}
	}

	inode_unlock(d_inode(privroot));
	return xaroot;
}

static struct dentry *open_xa_dir(const struct inode *inode, int flags)
{
	struct dentry *xaroot, *xadir;
	char namebuf[17];

	xaroot = open_xa_root(inode->i_sb, flags);
	if (IS_ERR(xaroot))
		return xaroot;

	snprintf(namebuf, sizeof(namebuf), "%X.%X",
		 le32_to_cpu(INODE_PKEY(inode)->k_objectid),
		 inode->i_generation);

	inode_lock_nested(d_inode(xaroot), I_MUTEX_XATTR);

	xadir = lookup_one_len(namebuf, xaroot, strlen(namebuf));
	if (!IS_ERR(xadir) && d_really_is_negative(xadir)) {
		int err = -ENODATA;

		if (xattr_may_create(flags))
			err = xattr_mkdir(d_inode(xaroot), xadir, 0700);
		if (err) {
			dput(xadir);
			xadir = ERR_PTR(err);
		}
	}

	inode_unlock(d_inode(xaroot));
	dput(xaroot);
	return xadir;
}

 
struct reiserfs_dentry_buf {
	struct dir_context ctx;
	struct dentry *xadir;
	int count;
	int err;
	struct dentry *dentries[8];
};

static bool
fill_with_dentries(struct dir_context *ctx, const char *name, int namelen,
		   loff_t offset, u64 ino, unsigned int d_type)
{
	struct reiserfs_dentry_buf *dbuf =
		container_of(ctx, struct reiserfs_dentry_buf, ctx);
	struct dentry *dentry;

	WARN_ON_ONCE(!inode_is_locked(d_inode(dbuf->xadir)));

	if (dbuf->count == ARRAY_SIZE(dbuf->dentries))
		return false;

	if (name[0] == '.' && (namelen < 2 ||
			       (namelen == 2 && name[1] == '.')))
		return true;

	dentry = lookup_one_len(name, dbuf->xadir, namelen);
	if (IS_ERR(dentry)) {
		dbuf->err = PTR_ERR(dentry);
		return false;
	} else if (d_really_is_negative(dentry)) {
		 
		reiserfs_error(dentry->d_sb, "xattr-20003",
			       "Corrupted directory: xattr %pd listed but "
			       "not found for file %pd.\n",
			       dentry, dbuf->xadir);
		dput(dentry);
		dbuf->err = -EIO;
		return false;
	}

	dbuf->dentries[dbuf->count++] = dentry;
	return true;
}

static void
cleanup_dentry_buf(struct reiserfs_dentry_buf *buf)
{
	int i;

	for (i = 0; i < buf->count; i++)
		if (buf->dentries[i])
			dput(buf->dentries[i]);
}

static int reiserfs_for_each_xattr(struct inode *inode,
				   int (*action)(struct dentry *, void *),
				   void *data)
{
	struct dentry *dir;
	int i, err = 0;
	struct reiserfs_dentry_buf buf = {
		.ctx.actor = fill_with_dentries,
	};

	 
	if (IS_PRIVATE(inode) || get_inode_sd_version(inode) == STAT_DATA_V1)
		return 0;

	dir = open_xa_dir(inode, XATTR_REPLACE);
	if (IS_ERR(dir)) {
		err = PTR_ERR(dir);
		goto out;
	} else if (d_really_is_negative(dir)) {
		err = 0;
		goto out_dir;
	}

	inode_lock_nested(d_inode(dir), I_MUTEX_XATTR);

	buf.xadir = dir;
	while (1) {
		err = reiserfs_readdir_inode(d_inode(dir), &buf.ctx);
		if (err)
			break;
		if (buf.err) {
			err = buf.err;
			break;
		}
		if (!buf.count)
			break;
		for (i = 0; !err && i < buf.count && buf.dentries[i]; i++) {
			struct dentry *dentry = buf.dentries[i];

			if (!d_is_dir(dentry))
				err = action(dentry, data);

			dput(dentry);
			buf.dentries[i] = NULL;
		}
		if (err)
			break;
		buf.count = 0;
	}
	inode_unlock(d_inode(dir));

	cleanup_dentry_buf(&buf);

	if (!err) {
		 
		int blocks = JOURNAL_PER_BALANCE_CNT * 2 + 2 +
			     4 * REISERFS_QUOTA_TRANS_BLOCKS(inode->i_sb);
		struct reiserfs_transaction_handle th;

		reiserfs_write_lock(inode->i_sb);
		err = journal_begin(&th, inode->i_sb, blocks);
		reiserfs_write_unlock(inode->i_sb);
		if (!err) {
			int jerror;

			inode_lock_nested(d_inode(dir->d_parent),
					  I_MUTEX_XATTR);
			err = action(dir, data);
			reiserfs_write_lock(inode->i_sb);
			jerror = journal_end(&th);
			reiserfs_write_unlock(inode->i_sb);
			inode_unlock(d_inode(dir->d_parent));
			err = jerror ?: err;
		}
	}
out_dir:
	dput(dir);
out:
	 
	if (err == -ENODATA || err == -EOPNOTSUPP)
		err = 0;
	return err;
}

static int delete_one_xattr(struct dentry *dentry, void *data)
{
	struct inode *dir = d_inode(dentry->d_parent);

	 
	if (d_is_dir(dentry))
		return xattr_rmdir(dir, dentry);

	return xattr_unlink(dir, dentry);
}

static int chown_one_xattr(struct dentry *dentry, void *data)
{
	struct iattr *attrs = data;
	int ia_valid = attrs->ia_valid;
	int err;

	 
	attrs->ia_valid &= (ATTR_UID|ATTR_GID);
	err = reiserfs_setattr(&nop_mnt_idmap, dentry, attrs);
	attrs->ia_valid = ia_valid;

	return err;
}

 
int reiserfs_delete_xattrs(struct inode *inode)
{
	int err = reiserfs_for_each_xattr(inode, delete_one_xattr, NULL);

	if (err)
		reiserfs_warning(inode->i_sb, "jdm-20004",
				 "Couldn't delete all xattrs (%d)\n", err);
	return err;
}

 
int reiserfs_chown_xattrs(struct inode *inode, struct iattr *attrs)
{
	int err = reiserfs_for_each_xattr(inode, chown_one_xattr, attrs);

	if (err)
		reiserfs_warning(inode->i_sb, "jdm-20007",
				 "Couldn't chown all xattrs (%d)\n", err);
	return err;
}

#ifdef CONFIG_REISERFS_FS_XATTR
 
static struct dentry *xattr_lookup(struct inode *inode, const char *name,
				    int flags)
{
	struct dentry *xadir, *xafile;
	int err = 0;

	xadir = open_xa_dir(inode, flags);
	if (IS_ERR(xadir))
		return ERR_CAST(xadir);

	inode_lock_nested(d_inode(xadir), I_MUTEX_XATTR);
	xafile = lookup_one_len(name, xadir, strlen(name));
	if (IS_ERR(xafile)) {
		err = PTR_ERR(xafile);
		goto out;
	}

	if (d_really_is_positive(xafile) && (flags & XATTR_CREATE))
		err = -EEXIST;

	if (d_really_is_negative(xafile)) {
		err = -ENODATA;
		if (xattr_may_create(flags))
			err = xattr_create(d_inode(xadir), xafile,
					      0700|S_IFREG);
	}

	if (err)
		dput(xafile);
out:
	inode_unlock(d_inode(xadir));
	dput(xadir);
	if (err)
		return ERR_PTR(err);
	return xafile;
}

 
static inline void reiserfs_put_page(struct page *page)
{
	kunmap(page);
	put_page(page);
}

static struct page *reiserfs_get_page(struct inode *dir, size_t n)
{
	struct address_space *mapping = dir->i_mapping;
	struct page *page;
	 
	mapping_set_gfp_mask(mapping, GFP_NOFS);
	page = read_mapping_page(mapping, n >> PAGE_SHIFT, NULL);
	if (!IS_ERR(page))
		kmap(page);
	return page;
}

static inline __u32 xattr_hash(const char *msg, int len)
{
	 
	return csum_partial(msg, len, 0);
}

int reiserfs_commit_write(struct file *f, struct page *page,
			  unsigned from, unsigned to);

static void update_ctime(struct inode *inode)
{
	struct timespec64 now = current_time(inode);
	struct timespec64 ctime = inode_get_ctime(inode);

	if (inode_unhashed(inode) || !inode->i_nlink ||
	    timespec64_equal(&ctime, &now))
		return;

	inode_set_ctime_to_ts(inode, now);
	mark_inode_dirty(inode);
}

static int lookup_and_delete_xattr(struct inode *inode, const char *name)
{
	int err = 0;
	struct dentry *dentry, *xadir;

	xadir = open_xa_dir(inode, XATTR_REPLACE);
	if (IS_ERR(xadir))
		return PTR_ERR(xadir);

	inode_lock_nested(d_inode(xadir), I_MUTEX_XATTR);
	dentry = lookup_one_len(name, xadir, strlen(name));
	if (IS_ERR(dentry)) {
		err = PTR_ERR(dentry);
		goto out_dput;
	}

	if (d_really_is_positive(dentry)) {
		err = xattr_unlink(d_inode(xadir), dentry);
		update_ctime(inode);
	}

	dput(dentry);
out_dput:
	inode_unlock(d_inode(xadir));
	dput(xadir);
	return err;
}


 

 
int
reiserfs_xattr_set_handle(struct reiserfs_transaction_handle *th,
			  struct inode *inode, const char *name,
			  const void *buffer, size_t buffer_size, int flags)
{
	int err = 0;
	struct dentry *dentry;
	struct page *page;
	char *data;
	size_t file_pos = 0;
	size_t buffer_pos = 0;
	size_t new_size;
	__u32 xahash = 0;

	if (get_inode_sd_version(inode) == STAT_DATA_V1)
		return -EOPNOTSUPP;

	if (!buffer) {
		err = lookup_and_delete_xattr(inode, name);
		return err;
	}

	dentry = xattr_lookup(inode, name, flags);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	down_write(&REISERFS_I(inode)->i_xattr_sem);

	xahash = xattr_hash(buffer, buffer_size);
	while (buffer_pos < buffer_size || buffer_pos == 0) {
		size_t chunk;
		size_t skip = 0;
		size_t page_offset = (file_pos & (PAGE_SIZE - 1));

		if (buffer_size - buffer_pos > PAGE_SIZE)
			chunk = PAGE_SIZE;
		else
			chunk = buffer_size - buffer_pos;

		page = reiserfs_get_page(d_inode(dentry), file_pos);
		if (IS_ERR(page)) {
			err = PTR_ERR(page);
			goto out_unlock;
		}

		lock_page(page);
		data = page_address(page);

		if (file_pos == 0) {
			struct reiserfs_xattr_header *rxh;

			skip = file_pos = sizeof(struct reiserfs_xattr_header);
			if (chunk + skip > PAGE_SIZE)
				chunk = PAGE_SIZE - skip;
			rxh = (struct reiserfs_xattr_header *)data;
			rxh->h_magic = cpu_to_le32(REISERFS_XATTR_MAGIC);
			rxh->h_hash = cpu_to_le32(xahash);
		}

		reiserfs_write_lock(inode->i_sb);
		err = __reiserfs_write_begin(page, page_offset, chunk + skip);
		if (!err) {
			if (buffer)
				memcpy(data + skip, buffer + buffer_pos, chunk);
			err = reiserfs_commit_write(NULL, page, page_offset,
						    page_offset + chunk +
						    skip);
		}
		reiserfs_write_unlock(inode->i_sb);
		unlock_page(page);
		reiserfs_put_page(page);
		buffer_pos += chunk;
		file_pos += chunk;
		skip = 0;
		if (err || buffer_size == 0 || !buffer)
			break;
	}

	new_size = buffer_size + sizeof(struct reiserfs_xattr_header);
	if (!err && new_size < i_size_read(d_inode(dentry))) {
		struct iattr newattrs = {
			.ia_ctime = current_time(inode),
			.ia_size = new_size,
			.ia_valid = ATTR_SIZE | ATTR_CTIME,
		};

		inode_lock_nested(d_inode(dentry), I_MUTEX_XATTR);
		inode_dio_wait(d_inode(dentry));

		err = reiserfs_setattr(&nop_mnt_idmap, dentry, &newattrs);
		inode_unlock(d_inode(dentry));
	} else
		update_ctime(inode);
out_unlock:
	up_write(&REISERFS_I(inode)->i_xattr_sem);
	dput(dentry);
	return err;
}

 
int reiserfs_xattr_set(struct inode *inode, const char *name,
		       const void *buffer, size_t buffer_size, int flags)
{

	struct reiserfs_transaction_handle th;
	int error, error2;
	size_t jbegin_count = reiserfs_xattr_nblocks(inode, buffer_size);

	 
	if (!d_really_is_positive(REISERFS_SB(inode->i_sb)->priv_root))
		return -EOPNOTSUPP;

	if (!(flags & XATTR_REPLACE))
		jbegin_count += reiserfs_xattr_jcreate_nblocks(inode);

	reiserfs_write_lock(inode->i_sb);
	error = journal_begin(&th, inode->i_sb, jbegin_count);
	reiserfs_write_unlock(inode->i_sb);
	if (error) {
		return error;
	}

	error = reiserfs_xattr_set_handle(&th, inode, name,
					  buffer, buffer_size, flags);

	reiserfs_write_lock(inode->i_sb);
	error2 = journal_end(&th);
	reiserfs_write_unlock(inode->i_sb);
	if (error == 0)
		error = error2;

	return error;
}

 
int
reiserfs_xattr_get(struct inode *inode, const char *name, void *buffer,
		   size_t buffer_size)
{
	ssize_t err = 0;
	struct dentry *dentry;
	size_t isize;
	size_t file_pos = 0;
	size_t buffer_pos = 0;
	struct page *page;
	__u32 hash = 0;

	if (name == NULL)
		return -EINVAL;

	 
	if (get_inode_sd_version(inode) == STAT_DATA_V1)
		return -EOPNOTSUPP;

	 
	if (!REISERFS_SB(inode->i_sb)->priv_root)
		return 0;

	dentry = xattr_lookup(inode, name, XATTR_REPLACE);
	if (IS_ERR(dentry)) {
		err = PTR_ERR(dentry);
		goto out;
	}

	down_read(&REISERFS_I(inode)->i_xattr_sem);

	isize = i_size_read(d_inode(dentry));

	 
	if (buffer == NULL) {
		err = isize - sizeof(struct reiserfs_xattr_header);
		goto out_unlock;
	}

	if (buffer_size < isize - sizeof(struct reiserfs_xattr_header)) {
		err = -ERANGE;
		goto out_unlock;
	}

	while (file_pos < isize) {
		size_t chunk;
		char *data;
		size_t skip = 0;

		if (isize - file_pos > PAGE_SIZE)
			chunk = PAGE_SIZE;
		else
			chunk = isize - file_pos;

		page = reiserfs_get_page(d_inode(dentry), file_pos);
		if (IS_ERR(page)) {
			err = PTR_ERR(page);
			goto out_unlock;
		}

		lock_page(page);
		data = page_address(page);
		if (file_pos == 0) {
			struct reiserfs_xattr_header *rxh =
			    (struct reiserfs_xattr_header *)data;
			skip = file_pos = sizeof(struct reiserfs_xattr_header);
			chunk -= skip;
			 
			if (rxh->h_magic != cpu_to_le32(REISERFS_XATTR_MAGIC)) {
				unlock_page(page);
				reiserfs_put_page(page);
				reiserfs_warning(inode->i_sb, "jdm-20001",
						 "Invalid magic for xattr (%s) "
						 "associated with %k", name,
						 INODE_PKEY(inode));
				err = -EIO;
				goto out_unlock;
			}
			hash = le32_to_cpu(rxh->h_hash);
		}
		memcpy(buffer + buffer_pos, data + skip, chunk);
		unlock_page(page);
		reiserfs_put_page(page);
		file_pos += chunk;
		buffer_pos += chunk;
		skip = 0;
	}
	err = isize - sizeof(struct reiserfs_xattr_header);

	if (xattr_hash(buffer, isize - sizeof(struct reiserfs_xattr_header)) !=
	    hash) {
		reiserfs_warning(inode->i_sb, "jdm-20002",
				 "Invalid hash for xattr (%s) associated "
				 "with %k", name, INODE_PKEY(inode));
		err = -EIO;
	}

out_unlock:
	up_read(&REISERFS_I(inode)->i_xattr_sem);
	dput(dentry);

out:
	return err;
}

 
#define for_each_xattr_handler(handlers, handler)		\
		for ((handler) = *(handlers)++;			\
			(handler) != NULL;			\
			(handler) = *(handlers)++)

static inline bool reiserfs_posix_acl_list(const char *name,
					   struct dentry *dentry)
{
	return (posix_acl_type(name) >= 0) &&
	       IS_POSIXACL(d_backing_inode(dentry));
}

 
static inline bool reiserfs_xattr_list(const struct xattr_handler **handlers,
				       const char *name, struct dentry *dentry)
{
	if (handlers) {
		const struct xattr_handler *xah = NULL;

		for_each_xattr_handler(handlers, xah) {
			const char *prefix = xattr_prefix(xah);

			if (strncmp(prefix, name, strlen(prefix)))
				continue;

			if (!xattr_handler_can_list(xah, dentry))
				return false;

			return true;
		}
	}

	return reiserfs_posix_acl_list(name, dentry);
}

struct listxattr_buf {
	struct dir_context ctx;
	size_t size;
	size_t pos;
	char *buf;
	struct dentry *dentry;
};

static bool listxattr_filler(struct dir_context *ctx, const char *name,
			    int namelen, loff_t offset, u64 ino,
			    unsigned int d_type)
{
	struct listxattr_buf *b =
		container_of(ctx, struct listxattr_buf, ctx);
	size_t size;

	if (name[0] != '.' ||
	    (namelen != 1 && (name[1] != '.' || namelen != 2))) {
		if (!reiserfs_xattr_list(b->dentry->d_sb->s_xattr, name,
					 b->dentry))
			return true;
		size = namelen + 1;
		if (b->buf) {
			if (b->pos + size > b->size) {
				b->pos = -ERANGE;
				return false;
			}
			memcpy(b->buf + b->pos, name, namelen);
			b->buf[b->pos + namelen] = 0;
		}
		b->pos += size;
	}
	return true;
}

 
ssize_t reiserfs_listxattr(struct dentry * dentry, char *buffer, size_t size)
{
	struct dentry *dir;
	int err = 0;
	struct listxattr_buf buf = {
		.ctx.actor = listxattr_filler,
		.dentry = dentry,
		.buf = buffer,
		.size = buffer ? size : 0,
	};

	if (d_really_is_negative(dentry))
		return -EINVAL;

	if (get_inode_sd_version(d_inode(dentry)) == STAT_DATA_V1)
		return -EOPNOTSUPP;

	dir = open_xa_dir(d_inode(dentry), XATTR_REPLACE);
	if (IS_ERR(dir)) {
		err = PTR_ERR(dir);
		if (err == -ENODATA)
			err = 0;   
		goto out;
	}

	inode_lock_nested(d_inode(dir), I_MUTEX_XATTR);
	err = reiserfs_readdir_inode(d_inode(dir), &buf.ctx);
	inode_unlock(d_inode(dir));

	if (!err)
		err = buf.pos;

	dput(dir);
out:
	return err;
}

static int create_privroot(struct dentry *dentry)
{
	int err;
	struct inode *inode = d_inode(dentry->d_parent);

	WARN_ON_ONCE(!inode_is_locked(inode));

	err = xattr_mkdir(inode, dentry, 0700);
	if (err || d_really_is_negative(dentry)) {
		reiserfs_warning(dentry->d_sb, "jdm-20006",
				 "xattrs/ACLs enabled and couldn't "
				 "find/create .reiserfs_priv. "
				 "Failing mount.");
		return -EOPNOTSUPP;
	}

	reiserfs_init_priv_inode(d_inode(dentry));
	reiserfs_info(dentry->d_sb, "Created %s - reserved for xattr "
		      "storage.\n", PRIVROOT_NAME);

	return 0;
}

#else
int __init reiserfs_xattr_register_handlers(void) { return 0; }
void reiserfs_xattr_unregister_handlers(void) {}
static int create_privroot(struct dentry *dentry) { return 0; }
#endif

 
const struct xattr_handler *reiserfs_xattr_handlers[] = {
#ifdef CONFIG_REISERFS_FS_XATTR
	&reiserfs_xattr_user_handler,
	&reiserfs_xattr_trusted_handler,
#endif
#ifdef CONFIG_REISERFS_FS_SECURITY
	&reiserfs_xattr_security_handler,
#endif
	NULL
};

static int xattr_mount_check(struct super_block *s)
{
	 
	if (old_format_only(s)) {
		if (reiserfs_xattrs_optional(s)) {
			 
			reiserfs_warning(s, "jdm-2005",
					 "xattrs/ACLs not supported "
					 "on pre-v3.6 format filesystems. "
					 "Failing mount.");
			return -EOPNOTSUPP;
		}
	}

	return 0;
}

int reiserfs_permission(struct mnt_idmap *idmap, struct inode *inode,
			int mask)
{
	 
	if (IS_PRIVATE(inode))
		return 0;

	return generic_permission(&nop_mnt_idmap, inode, mask);
}

static int xattr_hide_revalidate(struct dentry *dentry, unsigned int flags)
{
	return -EPERM;
}

static const struct dentry_operations xattr_lookup_poison_ops = {
	.d_revalidate = xattr_hide_revalidate,
};

int reiserfs_lookup_privroot(struct super_block *s)
{
	struct dentry *dentry;
	int err = 0;

	 
	inode_lock(d_inode(s->s_root));
	dentry = lookup_one_len(PRIVROOT_NAME, s->s_root,
				strlen(PRIVROOT_NAME));
	if (!IS_ERR(dentry)) {
		REISERFS_SB(s)->priv_root = dentry;
		d_set_d_op(dentry, &xattr_lookup_poison_ops);
		if (d_really_is_positive(dentry))
			reiserfs_init_priv_inode(d_inode(dentry));
	} else
		err = PTR_ERR(dentry);
	inode_unlock(d_inode(s->s_root));

	return err;
}

 
int reiserfs_xattr_init(struct super_block *s, int mount_flags)
{
	int err = 0;
	struct dentry *privroot = REISERFS_SB(s)->priv_root;

	err = xattr_mount_check(s);
	if (err)
		goto error;

	if (d_really_is_negative(privroot) && !(mount_flags & SB_RDONLY)) {
		inode_lock(d_inode(s->s_root));
		err = create_privroot(REISERFS_SB(s)->priv_root);
		inode_unlock(d_inode(s->s_root));
	}

	if (d_really_is_positive(privroot)) {
		inode_lock(d_inode(privroot));
		if (!REISERFS_SB(s)->xattr_root) {
			struct dentry *dentry;

			dentry = lookup_one_len(XAROOT_NAME, privroot,
						strlen(XAROOT_NAME));
			if (!IS_ERR(dentry))
				REISERFS_SB(s)->xattr_root = dentry;
			else
				err = PTR_ERR(dentry);
		}
		inode_unlock(d_inode(privroot));
	}

error:
	if (err) {
		clear_bit(REISERFS_XATTRS_USER, &REISERFS_SB(s)->s_mount_opt);
		clear_bit(REISERFS_POSIXACL, &REISERFS_SB(s)->s_mount_opt);
	}

	 
	if (reiserfs_posixacl(s))
		s->s_flags |= SB_POSIXACL;
	else
		s->s_flags &= ~SB_POSIXACL;

	return err;
}
