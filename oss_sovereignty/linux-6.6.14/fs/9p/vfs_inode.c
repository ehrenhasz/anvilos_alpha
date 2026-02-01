
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/pagemap.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/namei.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/xattr.h>
#include <linux/posix_acl.h>
#include <net/9p/9p.h>
#include <net/9p/client.h>

#include "v9fs.h"
#include "v9fs_vfs.h"
#include "fid.h"
#include "cache.h"
#include "xattr.h"
#include "acl.h"

static const struct inode_operations v9fs_dir_inode_operations;
static const struct inode_operations v9fs_dir_inode_operations_dotu;
static const struct inode_operations v9fs_file_inode_operations;
static const struct inode_operations v9fs_symlink_inode_operations;

 

static u32 unixmode2p9mode(struct v9fs_session_info *v9ses, umode_t mode)
{
	int res;

	res = mode & 0777;
	if (S_ISDIR(mode))
		res |= P9_DMDIR;
	if (v9fs_proto_dotu(v9ses)) {
		if (v9ses->nodev == 0) {
			if (S_ISSOCK(mode))
				res |= P9_DMSOCKET;
			if (S_ISFIFO(mode))
				res |= P9_DMNAMEDPIPE;
			if (S_ISBLK(mode))
				res |= P9_DMDEVICE;
			if (S_ISCHR(mode))
				res |= P9_DMDEVICE;
		}

		if ((mode & S_ISUID) == S_ISUID)
			res |= P9_DMSETUID;
		if ((mode & S_ISGID) == S_ISGID)
			res |= P9_DMSETGID;
		if ((mode & S_ISVTX) == S_ISVTX)
			res |= P9_DMSETVTX;
	}
	return res;
}

 
static int p9mode2perm(struct v9fs_session_info *v9ses,
		       struct p9_wstat *stat)
{
	int res;
	int mode = stat->mode;

	res = mode & S_IALLUGO;
	if (v9fs_proto_dotu(v9ses)) {
		if ((mode & P9_DMSETUID) == P9_DMSETUID)
			res |= S_ISUID;

		if ((mode & P9_DMSETGID) == P9_DMSETGID)
			res |= S_ISGID;

		if ((mode & P9_DMSETVTX) == P9_DMSETVTX)
			res |= S_ISVTX;
	}
	return res;
}

 
static umode_t p9mode2unixmode(struct v9fs_session_info *v9ses,
			       struct p9_wstat *stat, dev_t *rdev)
{
	int res, r;
	u32 mode = stat->mode;

	*rdev = 0;
	res = p9mode2perm(v9ses, stat);

	if ((mode & P9_DMDIR) == P9_DMDIR)
		res |= S_IFDIR;
	else if ((mode & P9_DMSYMLINK) && (v9fs_proto_dotu(v9ses)))
		res |= S_IFLNK;
	else if ((mode & P9_DMSOCKET) && (v9fs_proto_dotu(v9ses))
		 && (v9ses->nodev == 0))
		res |= S_IFSOCK;
	else if ((mode & P9_DMNAMEDPIPE) && (v9fs_proto_dotu(v9ses))
		 && (v9ses->nodev == 0))
		res |= S_IFIFO;
	else if ((mode & P9_DMDEVICE) && (v9fs_proto_dotu(v9ses))
		 && (v9ses->nodev == 0)) {
		char type = 0;
		int major = -1, minor = -1;

		r = sscanf(stat->extension, "%c %i %i", &type, &major, &minor);
		if (r != 3) {
			p9_debug(P9_DEBUG_ERROR,
				 "invalid device string, umode will be bogus: %s\n",
				 stat->extension);
			return res;
		}
		switch (type) {
		case 'c':
			res |= S_IFCHR;
			break;
		case 'b':
			res |= S_IFBLK;
			break;
		default:
			p9_debug(P9_DEBUG_ERROR, "Unknown special type %c %s\n",
				 type, stat->extension);
		}
		*rdev = MKDEV(major, minor);
	} else
		res |= S_IFREG;

	return res;
}

 

int v9fs_uflags2omode(int uflags, int extended)
{
	int ret;

	switch (uflags&3) {
	default:
	case O_RDONLY:
		ret = P9_OREAD;
		break;

	case O_WRONLY:
		ret = P9_OWRITE;
		break;

	case O_RDWR:
		ret = P9_ORDWR;
		break;
	}

	if (extended) {
		if (uflags & O_EXCL)
			ret |= P9_OEXCL;

		if (uflags & O_APPEND)
			ret |= P9_OAPPEND;
	}

	return ret;
}

 

void
v9fs_blank_wstat(struct p9_wstat *wstat)
{
	wstat->type = ~0;
	wstat->dev = ~0;
	wstat->qid.type = ~0;
	wstat->qid.version = ~0;
	*((long long *)&wstat->qid.path) = ~0;
	wstat->mode = ~0;
	wstat->atime = ~0;
	wstat->mtime = ~0;
	wstat->length = ~0;
	wstat->name = NULL;
	wstat->uid = NULL;
	wstat->gid = NULL;
	wstat->muid = NULL;
	wstat->n_uid = INVALID_UID;
	wstat->n_gid = INVALID_GID;
	wstat->n_muid = INVALID_UID;
	wstat->extension = NULL;
}

 
struct inode *v9fs_alloc_inode(struct super_block *sb)
{
	struct v9fs_inode *v9inode;

	v9inode = alloc_inode_sb(sb, v9fs_inode_cache, GFP_KERNEL);
	if (!v9inode)
		return NULL;
	v9inode->cache_validity = 0;
	mutex_init(&v9inode->v_mutex);
	return &v9inode->netfs.inode;
}

 

void v9fs_free_inode(struct inode *inode)
{
	kmem_cache_free(v9fs_inode_cache, V9FS_I(inode));
}

 
static void v9fs_set_netfs_context(struct inode *inode)
{
	struct v9fs_inode *v9inode = V9FS_I(inode);
	netfs_inode_init(&v9inode->netfs, &v9fs_req_ops);
}

int v9fs_init_inode(struct v9fs_session_info *v9ses,
		    struct inode *inode, umode_t mode, dev_t rdev)
{
	int err = 0;

	inode_init_owner(&nop_mnt_idmap, inode, NULL, mode);
	inode->i_blocks = 0;
	inode->i_rdev = rdev;
	inode->i_atime = inode->i_mtime = inode_set_ctime_current(inode);
	inode->i_mapping->a_ops = &v9fs_addr_operations;
	inode->i_private = NULL;

	switch (mode & S_IFMT) {
	case S_IFIFO:
	case S_IFBLK:
	case S_IFCHR:
	case S_IFSOCK:
		if (v9fs_proto_dotl(v9ses)) {
			inode->i_op = &v9fs_file_inode_operations_dotl;
		} else if (v9fs_proto_dotu(v9ses)) {
			inode->i_op = &v9fs_file_inode_operations;
		} else {
			p9_debug(P9_DEBUG_ERROR,
				 "special files without extended mode\n");
			err = -EINVAL;
			goto error;
		}
		init_special_inode(inode, inode->i_mode, inode->i_rdev);
		break;
	case S_IFREG:
		if (v9fs_proto_dotl(v9ses)) {
			inode->i_op = &v9fs_file_inode_operations_dotl;
			inode->i_fop = &v9fs_file_operations_dotl;
		} else {
			inode->i_op = &v9fs_file_inode_operations;
			inode->i_fop = &v9fs_file_operations;
		}

		break;
	case S_IFLNK:
		if (!v9fs_proto_dotu(v9ses) && !v9fs_proto_dotl(v9ses)) {
			p9_debug(P9_DEBUG_ERROR,
				 "extended modes used with legacy protocol\n");
			err = -EINVAL;
			goto error;
		}

		if (v9fs_proto_dotl(v9ses))
			inode->i_op = &v9fs_symlink_inode_operations_dotl;
		else
			inode->i_op = &v9fs_symlink_inode_operations;

		break;
	case S_IFDIR:
		inc_nlink(inode);
		if (v9fs_proto_dotl(v9ses))
			inode->i_op = &v9fs_dir_inode_operations_dotl;
		else if (v9fs_proto_dotu(v9ses))
			inode->i_op = &v9fs_dir_inode_operations_dotu;
		else
			inode->i_op = &v9fs_dir_inode_operations;

		if (v9fs_proto_dotl(v9ses))
			inode->i_fop = &v9fs_dir_operations_dotl;
		else
			inode->i_fop = &v9fs_dir_operations;

		break;
	default:
		p9_debug(P9_DEBUG_ERROR, "BAD mode 0x%hx S_IFMT 0x%x\n",
			 mode, mode & S_IFMT);
		err = -EINVAL;
		goto error;
	}

	v9fs_set_netfs_context(inode);
error:
	return err;

}

 

struct inode *v9fs_get_inode(struct super_block *sb, umode_t mode, dev_t rdev)
{
	int err;
	struct inode *inode;
	struct v9fs_session_info *v9ses = sb->s_fs_info;

	p9_debug(P9_DEBUG_VFS, "super block: %p mode: %ho\n", sb, mode);

	inode = new_inode(sb);
	if (!inode) {
		pr_warn("%s (%d): Problem allocating inode\n",
			__func__, task_pid_nr(current));
		return ERR_PTR(-ENOMEM);
	}
	err = v9fs_init_inode(v9ses, inode, mode, rdev);
	if (err) {
		iput(inode);
		return ERR_PTR(err);
	}
	return inode;
}

 
void v9fs_evict_inode(struct inode *inode)
{
	struct v9fs_inode __maybe_unused *v9inode = V9FS_I(inode);
	__le32 __maybe_unused version;

	truncate_inode_pages_final(&inode->i_data);

#ifdef CONFIG_9P_FSCACHE
	version = cpu_to_le32(v9inode->qid.version);
	fscache_clear_inode_writeback(v9fs_inode_cookie(v9inode), inode,
				      &version);
#endif

	clear_inode(inode);
	filemap_fdatawrite(&inode->i_data);

#ifdef CONFIG_9P_FSCACHE
	fscache_relinquish_cookie(v9fs_inode_cookie(v9inode), false);
#endif
}

static int v9fs_test_inode(struct inode *inode, void *data)
{
	int umode;
	dev_t rdev;
	struct v9fs_inode *v9inode = V9FS_I(inode);
	struct p9_wstat *st = (struct p9_wstat *)data;
	struct v9fs_session_info *v9ses = v9fs_inode2v9ses(inode);

	umode = p9mode2unixmode(v9ses, st, &rdev);
	 
	if (inode_wrong_type(inode, umode))
		return 0;

	 
	if (memcmp(&v9inode->qid.version,
		   &st->qid.version, sizeof(v9inode->qid.version)))
		return 0;

	if (v9inode->qid.type != st->qid.type)
		return 0;

	if (v9inode->qid.path != st->qid.path)
		return 0;
	return 1;
}

static int v9fs_test_new_inode(struct inode *inode, void *data)
{
	return 0;
}

static int v9fs_set_inode(struct inode *inode,  void *data)
{
	struct v9fs_inode *v9inode = V9FS_I(inode);
	struct p9_wstat *st = (struct p9_wstat *)data;

	memcpy(&v9inode->qid, &st->qid, sizeof(st->qid));
	return 0;
}

static struct inode *v9fs_qid_iget(struct super_block *sb,
				   struct p9_qid *qid,
				   struct p9_wstat *st,
				   int new)
{
	dev_t rdev;
	int retval;
	umode_t umode;
	unsigned long i_ino;
	struct inode *inode;
	struct v9fs_session_info *v9ses = sb->s_fs_info;
	int (*test)(struct inode *inode, void *data);

	if (new)
		test = v9fs_test_new_inode;
	else
		test = v9fs_test_inode;

	i_ino = v9fs_qid2ino(qid);
	inode = iget5_locked(sb, i_ino, test, v9fs_set_inode, st);
	if (!inode)
		return ERR_PTR(-ENOMEM);
	if (!(inode->i_state & I_NEW))
		return inode;
	 
	inode->i_ino = i_ino;
	umode = p9mode2unixmode(v9ses, st, &rdev);
	retval = v9fs_init_inode(v9ses, inode, umode, rdev);
	if (retval)
		goto error;

	v9fs_stat2inode(st, inode, sb, 0);
	v9fs_cache_inode_get_cookie(inode);
	unlock_new_inode(inode);
	return inode;
error:
	iget_failed(inode);
	return ERR_PTR(retval);

}

struct inode *
v9fs_inode_from_fid(struct v9fs_session_info *v9ses, struct p9_fid *fid,
		    struct super_block *sb, int new)
{
	struct p9_wstat *st;
	struct inode *inode = NULL;

	st = p9_client_stat(fid);
	if (IS_ERR(st))
		return ERR_CAST(st);

	inode = v9fs_qid_iget(sb, &st->qid, st, new);
	p9stat_free(st);
	kfree(st);
	return inode;
}

 
static int v9fs_at_to_dotl_flags(int flags)
{
	int rflags = 0;

	if (flags & AT_REMOVEDIR)
		rflags |= P9_DOTL_AT_REMOVEDIR;

	return rflags;
}

 
static void v9fs_dec_count(struct inode *inode)
{
	if (!S_ISDIR(inode->i_mode) || inode->i_nlink > 2)
		drop_nlink(inode);
}

 

static int v9fs_remove(struct inode *dir, struct dentry *dentry, int flags)
{
	struct inode *inode;
	int retval = -EOPNOTSUPP;
	struct p9_fid *v9fid, *dfid;
	struct v9fs_session_info *v9ses;

	p9_debug(P9_DEBUG_VFS, "inode: %p dentry: %p rmdir: %x\n",
		 dir, dentry, flags);

	v9ses = v9fs_inode2v9ses(dir);
	inode = d_inode(dentry);
	dfid = v9fs_parent_fid(dentry);
	if (IS_ERR(dfid)) {
		retval = PTR_ERR(dfid);
		p9_debug(P9_DEBUG_VFS, "fid lookup failed %d\n", retval);
		return retval;
	}
	if (v9fs_proto_dotl(v9ses))
		retval = p9_client_unlinkat(dfid, dentry->d_name.name,
					    v9fs_at_to_dotl_flags(flags));
	p9_fid_put(dfid);
	if (retval == -EOPNOTSUPP) {
		 
		v9fid = v9fs_fid_clone(dentry);
		if (IS_ERR(v9fid))
			return PTR_ERR(v9fid);
		retval = p9_client_remove(v9fid);
	}
	if (!retval) {
		 
		if (flags & AT_REMOVEDIR) {
			clear_nlink(inode);
			v9fs_dec_count(dir);
		} else
			v9fs_dec_count(inode);

		v9fs_invalidate_inode_attr(inode);
		v9fs_invalidate_inode_attr(dir);

		 
		 
		dentry->d_op->d_release(dentry);
	}
	return retval;
}

 
static struct p9_fid *
v9fs_create(struct v9fs_session_info *v9ses, struct inode *dir,
		struct dentry *dentry, char *extension, u32 perm, u8 mode)
{
	int err;
	const unsigned char *name;
	struct p9_fid *dfid, *ofid = NULL, *fid = NULL;
	struct inode *inode;

	p9_debug(P9_DEBUG_VFS, "name %pd\n", dentry);

	name = dentry->d_name.name;
	dfid = v9fs_parent_fid(dentry);
	if (IS_ERR(dfid)) {
		err = PTR_ERR(dfid);
		p9_debug(P9_DEBUG_VFS, "fid lookup failed %d\n", err);
		return ERR_PTR(err);
	}

	 
	ofid = clone_fid(dfid);
	if (IS_ERR(ofid)) {
		err = PTR_ERR(ofid);
		p9_debug(P9_DEBUG_VFS, "p9_client_walk failed %d\n", err);
		goto error;
	}

	err = p9_client_fcreate(ofid, name, perm, mode, extension);
	if (err < 0) {
		p9_debug(P9_DEBUG_VFS, "p9_client_fcreate failed %d\n", err);
		goto error;
	}

	if (!(perm & P9_DMLINK)) {
		 
		fid = p9_client_walk(dfid, 1, &name, 1);
		if (IS_ERR(fid)) {
			err = PTR_ERR(fid);
			p9_debug(P9_DEBUG_VFS,
				   "p9_client_walk failed %d\n", err);
			goto error;
		}
		 
		inode = v9fs_get_new_inode_from_fid(v9ses, fid, dir->i_sb);
		if (IS_ERR(inode)) {
			err = PTR_ERR(inode);
			p9_debug(P9_DEBUG_VFS,
				   "inode creation failed %d\n", err);
			goto error;
		}
		v9fs_fid_add(dentry, &fid);
		d_instantiate(dentry, inode);
	}
	p9_fid_put(dfid);
	return ofid;
error:
	p9_fid_put(dfid);
	p9_fid_put(ofid);
	p9_fid_put(fid);
	return ERR_PTR(err);
}

 

static int
v9fs_vfs_create(struct mnt_idmap *idmap, struct inode *dir,
		struct dentry *dentry, umode_t mode, bool excl)
{
	struct v9fs_session_info *v9ses = v9fs_inode2v9ses(dir);
	u32 perm = unixmode2p9mode(v9ses, mode);
	struct p9_fid *fid;

	 
	fid = v9fs_create(v9ses, dir, dentry, NULL, perm, P9_ORDWR);
	if (IS_ERR(fid))
		return PTR_ERR(fid);

	v9fs_invalidate_inode_attr(dir);
	p9_fid_put(fid);

	return 0;
}

 

static int v9fs_vfs_mkdir(struct mnt_idmap *idmap, struct inode *dir,
			  struct dentry *dentry, umode_t mode)
{
	int err;
	u32 perm;
	struct p9_fid *fid;
	struct v9fs_session_info *v9ses;

	p9_debug(P9_DEBUG_VFS, "name %pd\n", dentry);
	err = 0;
	v9ses = v9fs_inode2v9ses(dir);
	perm = unixmode2p9mode(v9ses, mode | S_IFDIR);
	fid = v9fs_create(v9ses, dir, dentry, NULL, perm, P9_OREAD);
	if (IS_ERR(fid)) {
		err = PTR_ERR(fid);
		fid = NULL;
	} else {
		inc_nlink(dir);
		v9fs_invalidate_inode_attr(dir);
	}

	if (fid)
		p9_fid_put(fid);

	return err;
}

 

struct dentry *v9fs_vfs_lookup(struct inode *dir, struct dentry *dentry,
				      unsigned int flags)
{
	struct dentry *res;
	struct v9fs_session_info *v9ses;
	struct p9_fid *dfid, *fid;
	struct inode *inode;
	const unsigned char *name;

	p9_debug(P9_DEBUG_VFS, "dir: %p dentry: (%pd) %p flags: %x\n",
		 dir, dentry, dentry, flags);

	if (dentry->d_name.len > NAME_MAX)
		return ERR_PTR(-ENAMETOOLONG);

	v9ses = v9fs_inode2v9ses(dir);
	 
	dfid = v9fs_parent_fid(dentry);
	if (IS_ERR(dfid))
		return ERR_CAST(dfid);

	 
	name = dentry->d_name.name;
	fid = p9_client_walk(dfid, 1, &name, 1);
	p9_fid_put(dfid);
	if (fid == ERR_PTR(-ENOENT))
		inode = NULL;
	else if (IS_ERR(fid))
		inode = ERR_CAST(fid);
	else if (v9ses->cache & (CACHE_META|CACHE_LOOSE))
		inode = v9fs_get_inode_from_fid(v9ses, fid, dir->i_sb);
	else
		inode = v9fs_get_new_inode_from_fid(v9ses, fid, dir->i_sb);
	 
	res = d_splice_alias(inode, dentry);
	if (!IS_ERR(fid)) {
		if (!res)
			v9fs_fid_add(dentry, &fid);
		else if (!IS_ERR(res))
			v9fs_fid_add(res, &fid);
		else
			p9_fid_put(fid);
	}
	return res;
}

static int
v9fs_vfs_atomic_open(struct inode *dir, struct dentry *dentry,
		     struct file *file, unsigned int flags, umode_t mode)
{
	int err;
	u32 perm;
	struct v9fs_inode __maybe_unused *v9inode;
	struct v9fs_session_info *v9ses;
	struct p9_fid *fid;
	struct dentry *res = NULL;
	struct inode *inode;
	int p9_omode;

	if (d_in_lookup(dentry)) {
		res = v9fs_vfs_lookup(dir, dentry, 0);
		if (IS_ERR(res))
			return PTR_ERR(res);

		if (res)
			dentry = res;
	}

	 
	if (!(flags & O_CREAT) || d_really_is_positive(dentry))
		return finish_no_open(file, res);

	v9ses = v9fs_inode2v9ses(dir);
	perm = unixmode2p9mode(v9ses, mode);
	p9_omode = v9fs_uflags2omode(flags, v9fs_proto_dotu(v9ses));

	if ((v9ses->cache & CACHE_WRITEBACK) && (p9_omode & P9_OWRITE)) {
		p9_omode = (p9_omode & ~P9_OWRITE) | P9_ORDWR;
		p9_debug(P9_DEBUG_CACHE,
			"write-only file with writeback enabled, creating w/ O_RDWR\n");
	}
	fid = v9fs_create(v9ses, dir, dentry, NULL, perm, p9_omode);
	if (IS_ERR(fid)) {
		err = PTR_ERR(fid);
		goto error;
	}

	v9fs_invalidate_inode_attr(dir);
	inode = d_inode(dentry);
	v9inode = V9FS_I(inode);
	err = finish_open(file, dentry, generic_file_open);
	if (err)
		goto error;

	file->private_data = fid;
#ifdef CONFIG_9P_FSCACHE
	if (v9ses->cache & CACHE_FSCACHE)
		fscache_use_cookie(v9fs_inode_cookie(v9inode),
				   file->f_mode & FMODE_WRITE);
#endif

	v9fs_fid_add_modes(fid, v9ses->flags, v9ses->cache, file->f_flags);
	v9fs_open_fid_add(inode, &fid);

	file->f_mode |= FMODE_CREATED;
out:
	dput(res);
	return err;

error:
	p9_fid_put(fid);
	goto out;
}

 

int v9fs_vfs_unlink(struct inode *i, struct dentry *d)
{
	return v9fs_remove(i, d, 0);
}

 

int v9fs_vfs_rmdir(struct inode *i, struct dentry *d)
{
	return v9fs_remove(i, d, AT_REMOVEDIR);
}

 

int
v9fs_vfs_rename(struct mnt_idmap *idmap, struct inode *old_dir,
		struct dentry *old_dentry, struct inode *new_dir,
		struct dentry *new_dentry, unsigned int flags)
{
	int retval;
	struct inode *old_inode;
	struct inode *new_inode;
	struct v9fs_session_info *v9ses;
	struct p9_fid *oldfid = NULL, *dfid = NULL;
	struct p9_fid *olddirfid = NULL;
	struct p9_fid *newdirfid = NULL;
	struct p9_wstat wstat;

	if (flags)
		return -EINVAL;

	p9_debug(P9_DEBUG_VFS, "\n");
	old_inode = d_inode(old_dentry);
	new_inode = d_inode(new_dentry);
	v9ses = v9fs_inode2v9ses(old_inode);
	oldfid = v9fs_fid_lookup(old_dentry);
	if (IS_ERR(oldfid))
		return PTR_ERR(oldfid);

	dfid = v9fs_parent_fid(old_dentry);
	olddirfid = clone_fid(dfid);
	p9_fid_put(dfid);
	dfid = NULL;

	if (IS_ERR(olddirfid)) {
		retval = PTR_ERR(olddirfid);
		goto error;
	}

	dfid = v9fs_parent_fid(new_dentry);
	newdirfid = clone_fid(dfid);
	p9_fid_put(dfid);
	dfid = NULL;

	if (IS_ERR(newdirfid)) {
		retval = PTR_ERR(newdirfid);
		goto error;
	}

	down_write(&v9ses->rename_sem);
	if (v9fs_proto_dotl(v9ses)) {
		retval = p9_client_renameat(olddirfid, old_dentry->d_name.name,
					    newdirfid, new_dentry->d_name.name);
		if (retval == -EOPNOTSUPP)
			retval = p9_client_rename(oldfid, newdirfid,
						  new_dentry->d_name.name);
		if (retval != -EOPNOTSUPP)
			goto error_locked;
	}
	if (old_dentry->d_parent != new_dentry->d_parent) {
		 

		p9_debug(P9_DEBUG_ERROR, "old dir and new dir are different\n");
		retval = -EXDEV;
		goto error_locked;
	}
	v9fs_blank_wstat(&wstat);
	wstat.muid = v9ses->uname;
	wstat.name = new_dentry->d_name.name;
	retval = p9_client_wstat(oldfid, &wstat);

error_locked:
	if (!retval) {
		if (new_inode) {
			if (S_ISDIR(new_inode->i_mode))
				clear_nlink(new_inode);
			else
				v9fs_dec_count(new_inode);
		}
		if (S_ISDIR(old_inode->i_mode)) {
			if (!new_inode)
				inc_nlink(new_dir);
			v9fs_dec_count(old_dir);
		}
		v9fs_invalidate_inode_attr(old_inode);
		v9fs_invalidate_inode_attr(old_dir);
		v9fs_invalidate_inode_attr(new_dir);

		 
		d_move(old_dentry, new_dentry);
	}
	up_write(&v9ses->rename_sem);

error:
	p9_fid_put(newdirfid);
	p9_fid_put(olddirfid);
	p9_fid_put(oldfid);
	return retval;
}

 

static int
v9fs_vfs_getattr(struct mnt_idmap *idmap, const struct path *path,
		 struct kstat *stat, u32 request_mask, unsigned int flags)
{
	struct dentry *dentry = path->dentry;
	struct inode *inode = d_inode(dentry);
	struct v9fs_session_info *v9ses;
	struct p9_fid *fid;
	struct p9_wstat *st;

	p9_debug(P9_DEBUG_VFS, "dentry: %p\n", dentry);
	v9ses = v9fs_dentry2v9ses(dentry);
	if (v9ses->cache & (CACHE_META|CACHE_LOOSE)) {
		generic_fillattr(&nop_mnt_idmap, request_mask, inode, stat);
		return 0;
	} else if (v9ses->cache & CACHE_WRITEBACK) {
		if (S_ISREG(inode->i_mode)) {
			int retval = filemap_fdatawrite(inode->i_mapping);

			if (retval)
				p9_debug(P9_DEBUG_ERROR,
				    "flushing writeback during getattr returned %d\n", retval);
		}
	}
	fid = v9fs_fid_lookup(dentry);
	if (IS_ERR(fid))
		return PTR_ERR(fid);

	st = p9_client_stat(fid);
	p9_fid_put(fid);
	if (IS_ERR(st))
		return PTR_ERR(st);

	v9fs_stat2inode(st, d_inode(dentry), dentry->d_sb, 0);
	generic_fillattr(&nop_mnt_idmap, request_mask, d_inode(dentry), stat);

	p9stat_free(st);
	kfree(st);
	return 0;
}

 

static int v9fs_vfs_setattr(struct mnt_idmap *idmap,
			    struct dentry *dentry, struct iattr *iattr)
{
	int retval, use_dentry = 0;
	struct inode *inode = d_inode(dentry);
	struct v9fs_session_info *v9ses;
	struct p9_fid *fid = NULL;
	struct p9_wstat wstat;

	p9_debug(P9_DEBUG_VFS, "\n");
	retval = setattr_prepare(&nop_mnt_idmap, dentry, iattr);
	if (retval)
		return retval;

	v9ses = v9fs_dentry2v9ses(dentry);
	if (iattr->ia_valid & ATTR_FILE) {
		fid = iattr->ia_file->private_data;
		WARN_ON(!fid);
	}
	if (!fid) {
		fid = v9fs_fid_lookup(dentry);
		use_dentry = 1;
	}
	if (IS_ERR(fid))
		return PTR_ERR(fid);

	v9fs_blank_wstat(&wstat);
	if (iattr->ia_valid & ATTR_MODE)
		wstat.mode = unixmode2p9mode(v9ses, iattr->ia_mode);

	if (iattr->ia_valid & ATTR_MTIME)
		wstat.mtime = iattr->ia_mtime.tv_sec;

	if (iattr->ia_valid & ATTR_ATIME)
		wstat.atime = iattr->ia_atime.tv_sec;

	if (iattr->ia_valid & ATTR_SIZE)
		wstat.length = iattr->ia_size;

	if (v9fs_proto_dotu(v9ses)) {
		if (iattr->ia_valid & ATTR_UID)
			wstat.n_uid = iattr->ia_uid;

		if (iattr->ia_valid & ATTR_GID)
			wstat.n_gid = iattr->ia_gid;
	}

	 
	if (d_is_reg(dentry)) {
		retval = filemap_fdatawrite(inode->i_mapping);
		if (retval)
			p9_debug(P9_DEBUG_ERROR,
			    "flushing writeback during setattr returned %d\n", retval);
	}

	retval = p9_client_wstat(fid, &wstat);

	if (use_dentry)
		p9_fid_put(fid);

	if (retval < 0)
		return retval;

	if ((iattr->ia_valid & ATTR_SIZE) &&
		 iattr->ia_size != i_size_read(inode)) {
		truncate_setsize(inode, iattr->ia_size);
		truncate_pagecache(inode, iattr->ia_size);

#ifdef CONFIG_9P_FSCACHE
		if (v9ses->cache & CACHE_FSCACHE) {
			struct v9fs_inode *v9inode = V9FS_I(inode);

			fscache_resize_cookie(v9fs_inode_cookie(v9inode), iattr->ia_size);
		}
#endif
	}

	v9fs_invalidate_inode_attr(inode);

	setattr_copy(&nop_mnt_idmap, inode, iattr);
	mark_inode_dirty(inode);
	return 0;
}

 

void
v9fs_stat2inode(struct p9_wstat *stat, struct inode *inode,
		 struct super_block *sb, unsigned int flags)
{
	umode_t mode;
	struct v9fs_session_info *v9ses = sb->s_fs_info;
	struct v9fs_inode *v9inode = V9FS_I(inode);

	set_nlink(inode, 1);

	inode->i_atime.tv_sec = stat->atime;
	inode->i_mtime.tv_sec = stat->mtime;
	inode_set_ctime(inode, stat->mtime, 0);

	inode->i_uid = v9ses->dfltuid;
	inode->i_gid = v9ses->dfltgid;

	if (v9fs_proto_dotu(v9ses)) {
		inode->i_uid = stat->n_uid;
		inode->i_gid = stat->n_gid;
	}
	if ((S_ISREG(inode->i_mode)) || (S_ISDIR(inode->i_mode))) {
		if (v9fs_proto_dotu(v9ses)) {
			unsigned int i_nlink;
			 
			 
			if (sscanf(stat->extension,
				   " HARDLINKCOUNT %u", &i_nlink) == 1)
				set_nlink(inode, i_nlink);
		}
	}
	mode = p9mode2perm(v9ses, stat);
	mode |= inode->i_mode & ~S_IALLUGO;
	inode->i_mode = mode;

	if (!(flags & V9FS_STAT2INODE_KEEP_ISIZE))
		v9fs_i_size_write(inode, stat->length);
	 
	inode->i_blocks = (stat->length + 512 - 1) >> 9;
	v9inode->cache_validity &= ~V9FS_INO_INVALID_ATTR;
}

 

ino_t v9fs_qid2ino(struct p9_qid *qid)
{
	u64 path = qid->path + 2;
	ino_t i = 0;

	if (sizeof(ino_t) == sizeof(path))
		memcpy(&i, &path, sizeof(ino_t));
	else
		i = (ino_t) (path ^ (path >> 32));

	return i;
}

 

static const char *v9fs_vfs_get_link(struct dentry *dentry,
				     struct inode *inode,
				     struct delayed_call *done)
{
	struct v9fs_session_info *v9ses;
	struct p9_fid *fid;
	struct p9_wstat *st;
	char *res;

	if (!dentry)
		return ERR_PTR(-ECHILD);

	v9ses = v9fs_dentry2v9ses(dentry);
	if (!v9fs_proto_dotu(v9ses))
		return ERR_PTR(-EBADF);

	p9_debug(P9_DEBUG_VFS, "%pd\n", dentry);
	fid = v9fs_fid_lookup(dentry);

	if (IS_ERR(fid))
		return ERR_CAST(fid);

	st = p9_client_stat(fid);
	p9_fid_put(fid);
	if (IS_ERR(st))
		return ERR_CAST(st);

	if (!(st->mode & P9_DMSYMLINK)) {
		p9stat_free(st);
		kfree(st);
		return ERR_PTR(-EINVAL);
	}
	res = st->extension;
	st->extension = NULL;
	if (strlen(res) >= PATH_MAX)
		res[PATH_MAX - 1] = '\0';

	p9stat_free(st);
	kfree(st);
	set_delayed_call(done, kfree_link, res);
	return res;
}

 

static int v9fs_vfs_mkspecial(struct inode *dir, struct dentry *dentry,
	u32 perm, const char *extension)
{
	struct p9_fid *fid;
	struct v9fs_session_info *v9ses;

	v9ses = v9fs_inode2v9ses(dir);
	if (!v9fs_proto_dotu(v9ses)) {
		p9_debug(P9_DEBUG_ERROR, "not extended\n");
		return -EPERM;
	}

	fid = v9fs_create(v9ses, dir, dentry, (char *) extension, perm,
								P9_OREAD);
	if (IS_ERR(fid))
		return PTR_ERR(fid);

	v9fs_invalidate_inode_attr(dir);
	p9_fid_put(fid);
	return 0;
}

 

static int
v9fs_vfs_symlink(struct mnt_idmap *idmap, struct inode *dir,
		 struct dentry *dentry, const char *symname)
{
	p9_debug(P9_DEBUG_VFS, " %lu,%pd,%s\n",
		 dir->i_ino, dentry, symname);

	return v9fs_vfs_mkspecial(dir, dentry, P9_DMSYMLINK, symname);
}

#define U32_MAX_DIGITS 10

 

static int
v9fs_vfs_link(struct dentry *old_dentry, struct inode *dir,
	      struct dentry *dentry)
{
	int retval;
	char name[1 + U32_MAX_DIGITS + 2];  
	struct p9_fid *oldfid;

	p9_debug(P9_DEBUG_VFS, " %lu,%pd,%pd\n",
		 dir->i_ino, dentry, old_dentry);

	oldfid = v9fs_fid_clone(old_dentry);
	if (IS_ERR(oldfid))
		return PTR_ERR(oldfid);

	sprintf(name, "%d\n", oldfid->fid);
	retval = v9fs_vfs_mkspecial(dir, dentry, P9_DMLINK, name);
	if (!retval) {
		v9fs_refresh_inode(oldfid, d_inode(old_dentry));
		v9fs_invalidate_inode_attr(dir);
	}
	p9_fid_put(oldfid);
	return retval;
}

 

static int
v9fs_vfs_mknod(struct mnt_idmap *idmap, struct inode *dir,
	       struct dentry *dentry, umode_t mode, dev_t rdev)
{
	struct v9fs_session_info *v9ses = v9fs_inode2v9ses(dir);
	int retval;
	char name[2 + U32_MAX_DIGITS + 1 + U32_MAX_DIGITS + 1];
	u32 perm;

	p9_debug(P9_DEBUG_VFS, " %lu,%pd mode: %x MAJOR: %u MINOR: %u\n",
		 dir->i_ino, dentry, mode,
		 MAJOR(rdev), MINOR(rdev));

	 
	if (S_ISBLK(mode))
		sprintf(name, "b %u %u", MAJOR(rdev), MINOR(rdev));
	else if (S_ISCHR(mode))
		sprintf(name, "c %u %u", MAJOR(rdev), MINOR(rdev));
	else
		*name = 0;

	perm = unixmode2p9mode(v9ses, mode);
	retval = v9fs_vfs_mkspecial(dir, dentry, perm, name);

	return retval;
}

int v9fs_refresh_inode(struct p9_fid *fid, struct inode *inode)
{
	int umode;
	dev_t rdev;
	struct p9_wstat *st;
	struct v9fs_session_info *v9ses;
	unsigned int flags;

	v9ses = v9fs_inode2v9ses(inode);
	st = p9_client_stat(fid);
	if (IS_ERR(st))
		return PTR_ERR(st);
	 
	umode = p9mode2unixmode(v9ses, st, &rdev);
	if (inode_wrong_type(inode, umode))
		goto out;

	 
	flags = (v9ses->cache & CACHE_LOOSE) ?
		V9FS_STAT2INODE_KEEP_ISIZE : 0;
	v9fs_stat2inode(st, inode, inode->i_sb, flags);
out:
	p9stat_free(st);
	kfree(st);
	return 0;
}

static const struct inode_operations v9fs_dir_inode_operations_dotu = {
	.create = v9fs_vfs_create,
	.lookup = v9fs_vfs_lookup,
	.atomic_open = v9fs_vfs_atomic_open,
	.symlink = v9fs_vfs_symlink,
	.link = v9fs_vfs_link,
	.unlink = v9fs_vfs_unlink,
	.mkdir = v9fs_vfs_mkdir,
	.rmdir = v9fs_vfs_rmdir,
	.mknod = v9fs_vfs_mknod,
	.rename = v9fs_vfs_rename,
	.getattr = v9fs_vfs_getattr,
	.setattr = v9fs_vfs_setattr,
};

static const struct inode_operations v9fs_dir_inode_operations = {
	.create = v9fs_vfs_create,
	.lookup = v9fs_vfs_lookup,
	.atomic_open = v9fs_vfs_atomic_open,
	.unlink = v9fs_vfs_unlink,
	.mkdir = v9fs_vfs_mkdir,
	.rmdir = v9fs_vfs_rmdir,
	.mknod = v9fs_vfs_mknod,
	.rename = v9fs_vfs_rename,
	.getattr = v9fs_vfs_getattr,
	.setattr = v9fs_vfs_setattr,
};

static const struct inode_operations v9fs_file_inode_operations = {
	.getattr = v9fs_vfs_getattr,
	.setattr = v9fs_vfs_setattr,
};

static const struct inode_operations v9fs_symlink_inode_operations = {
	.get_link = v9fs_vfs_get_link,
	.getattr = v9fs_vfs_getattr,
	.setattr = v9fs_vfs_setattr,
};

