
 

#include <linux/fs.h>
#include <linux/cred.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/xattr.h>
#include <linux/exportfs.h>
#include <linux/ratelimit.h>
#include "overlayfs.h"

static int ovl_encode_maybe_copy_up(struct dentry *dentry)
{
	int err;

	if (ovl_dentry_upper(dentry))
		return 0;

	err = ovl_want_write(dentry);
	if (!err) {
		err = ovl_copy_up(dentry);
		ovl_drop_write(dentry);
	}

	if (err) {
		pr_warn_ratelimited("failed to copy up on encode (%pd2, err=%i)\n",
				    dentry, err);
	}

	return err;
}

 

 
static int ovl_connectable_layer(struct dentry *dentry)
{
	struct ovl_entry *oe = OVL_E(dentry);

	 
	if (dentry == dentry->d_sb->s_root)
		return ovl_numlower(oe);

	 
	if (ovl_dentry_upper(dentry) &&
	    !ovl_test_flag(OVL_INDEX, d_inode(dentry)))
		return 0;

	 
	return ovl_lowerstack(oe)->layer->idx;
}

 
static int ovl_connect_layer(struct dentry *dentry)
{
	struct dentry *next, *parent = NULL;
	struct ovl_entry *oe = OVL_E(dentry);
	int origin_layer;
	int err = 0;

	if (WARN_ON(dentry == dentry->d_sb->s_root) ||
	    WARN_ON(!ovl_dentry_lower(dentry)))
		return -EIO;

	origin_layer = ovl_lowerstack(oe)->layer->idx;
	if (ovl_dentry_test_flag(OVL_E_CONNECTED, dentry))
		return origin_layer;

	 
	next = dget(dentry);
	for (;;) {
		parent = dget_parent(next);
		if (WARN_ON(parent == next)) {
			err = -EIO;
			break;
		}

		 
		if (ovl_connectable_layer(parent) < origin_layer) {
			err = ovl_encode_maybe_copy_up(next);
			break;
		}

		 
		if (ovl_dentry_test_flag(OVL_E_CONNECTED, parent) ||
		    ovl_test_flag(OVL_INDEX, d_inode(parent)))
			break;

		dput(next);
		next = parent;
	}

	dput(parent);
	dput(next);

	if (!err)
		ovl_dentry_set_flag(OVL_E_CONNECTED, dentry);

	return err ?: origin_layer;
}

 
static int ovl_check_encode_origin(struct dentry *dentry)
{
	struct ovl_fs *ofs = OVL_FS(dentry->d_sb);
	bool decodable = ofs->config.nfs_export;

	 
	if (!ovl_dentry_upper(dentry) && !decodable)
		return 1;

	 
	if (!ovl_dentry_lower(dentry))
		return 0;

	 
	if (dentry == dentry->d_sb->s_root)
		return 0;

	 
	if (ovl_dentry_upper(dentry) && decodable &&
	    !ovl_test_flag(OVL_INDEX, d_inode(dentry)))
		return 0;

	 
	if (d_is_dir(dentry) && ovl_upper_mnt(ofs) && decodable)
		return ovl_connect_layer(dentry);

	 
	return 1;
}

static int ovl_dentry_to_fid(struct ovl_fs *ofs, struct dentry *dentry,
			     u32 *fid, int buflen)
{
	struct ovl_fh *fh = NULL;
	int err, enc_lower;
	int len;

	 
	err = enc_lower = ovl_check_encode_origin(dentry);
	if (enc_lower < 0)
		goto fail;

	 
	fh = ovl_encode_real_fh(ofs, enc_lower ? ovl_dentry_lower(dentry) :
				ovl_dentry_upper(dentry), !enc_lower);
	if (IS_ERR(fh))
		return PTR_ERR(fh);

	len = OVL_FH_LEN(fh);
	if (len <= buflen)
		memcpy(fid, fh, len);
	err = len;

out:
	kfree(fh);
	return err;

fail:
	pr_warn_ratelimited("failed to encode file handle (%pd2, err=%i)\n",
			    dentry, err);
	goto out;
}

static int ovl_encode_fh(struct inode *inode, u32 *fid, int *max_len,
			 struct inode *parent)
{
	struct ovl_fs *ofs = OVL_FS(inode->i_sb);
	struct dentry *dentry;
	int bytes, buflen = *max_len << 2;

	 
	if (parent)
		return FILEID_INVALID;

	dentry = d_find_any_alias(inode);
	if (!dentry)
		return FILEID_INVALID;

	bytes = ovl_dentry_to_fid(ofs, dentry, fid, buflen);
	dput(dentry);
	if (bytes <= 0)
		return FILEID_INVALID;

	*max_len = bytes >> 2;
	if (bytes > buflen)
		return FILEID_INVALID;

	return OVL_FILEID_V1;
}

 
static struct dentry *ovl_obtain_alias(struct super_block *sb,
				       struct dentry *upper_alias,
				       struct ovl_path *lowerpath,
				       struct dentry *index)
{
	struct dentry *lower = lowerpath ? lowerpath->dentry : NULL;
	struct dentry *upper = upper_alias ?: index;
	struct dentry *dentry;
	struct inode *inode = NULL;
	struct ovl_entry *oe;
	struct ovl_inode_params oip = {
		.index = index,
	};

	 
	if (d_is_dir(upper ?: lower))
		return ERR_PTR(-EIO);

	oe = ovl_alloc_entry(!!lower);
	if (!oe)
		return ERR_PTR(-ENOMEM);

	oip.upperdentry = dget(upper);
	if (lower) {
		ovl_lowerstack(oe)->dentry = dget(lower);
		ovl_lowerstack(oe)->layer = lowerpath->layer;
	}
	oip.oe = oe;
	inode = ovl_get_inode(sb, &oip);
	if (IS_ERR(inode)) {
		ovl_free_entry(oe);
		dput(upper);
		return ERR_CAST(inode);
	}

	if (upper)
		ovl_set_flag(OVL_UPPERDATA, inode);

	dentry = d_find_any_alias(inode);
	if (dentry)
		goto out_iput;

	dentry = d_alloc_anon(inode->i_sb);
	if (unlikely(!dentry))
		goto nomem;

	if (upper_alias)
		ovl_dentry_set_upper_alias(dentry);

	ovl_dentry_init_reval(dentry, upper, OVL_I_E(inode));

	return d_instantiate_anon(dentry, inode);

nomem:
	dput(dentry);
	dentry = ERR_PTR(-ENOMEM);
out_iput:
	iput(inode);
	return dentry;
}

 
static struct dentry *ovl_dentry_real_at(struct dentry *dentry, int idx)
{
	struct ovl_entry *oe = OVL_E(dentry);
	struct ovl_path *lowerstack = ovl_lowerstack(oe);
	int i;

	if (!idx)
		return ovl_dentry_upper(dentry);

	for (i = 0; i < ovl_numlower(oe); i++) {
		if (lowerstack[i].layer->idx == idx)
			return lowerstack[i].dentry;
	}

	return NULL;
}

 
static struct dentry *ovl_lookup_real_one(struct dentry *connected,
					  struct dentry *real,
					  const struct ovl_layer *layer)
{
	struct inode *dir = d_inode(connected);
	struct dentry *this, *parent = NULL;
	struct name_snapshot name;
	int err;

	 
	inode_lock_nested(dir, I_MUTEX_PARENT);
	err = -ECHILD;
	parent = dget_parent(real);
	if (ovl_dentry_real_at(connected, layer->idx) != parent)
		goto fail;

	 
	take_dentry_name_snapshot(&name, real);
	 
	this = lookup_one_len(name.name.name, connected, name.name.len);
	release_dentry_name_snapshot(&name);
	err = PTR_ERR(this);
	if (IS_ERR(this)) {
		goto fail;
	} else if (!this || !this->d_inode) {
		dput(this);
		err = -ENOENT;
		goto fail;
	} else if (ovl_dentry_real_at(this, layer->idx) != real) {
		dput(this);
		err = -ESTALE;
		goto fail;
	}

out:
	dput(parent);
	inode_unlock(dir);
	return this;

fail:
	pr_warn_ratelimited("failed to lookup one by real (%pd2, layer=%d, connected=%pd2, err=%i)\n",
			    real, layer->idx, connected, err);
	this = ERR_PTR(err);
	goto out;
}

static struct dentry *ovl_lookup_real(struct super_block *sb,
				      struct dentry *real,
				      const struct ovl_layer *layer);

 
static struct dentry *ovl_lookup_real_inode(struct super_block *sb,
					    struct dentry *real,
					    const struct ovl_layer *layer)
{
	struct ovl_fs *ofs = OVL_FS(sb);
	struct dentry *index = NULL;
	struct dentry *this = NULL;
	struct inode *inode;

	 
	inode = ovl_lookup_inode(sb, real, !layer->idx);
	if (IS_ERR(inode))
		return ERR_CAST(inode);
	if (inode) {
		this = d_find_any_alias(inode);
		iput(inode);
	}

	 
	if (!this && layer->idx && ofs->indexdir && !WARN_ON(!d_is_dir(real))) {
		index = ovl_lookup_index(ofs, NULL, real, false);
		if (IS_ERR(index))
			return index;
	}

	 
	if (index) {
		struct dentry *upper = ovl_index_upper(ofs, index, true);

		dput(index);
		if (IS_ERR_OR_NULL(upper))
			return upper;

		 
		this = ovl_lookup_real(sb, upper, &ofs->layers[0]);
		dput(upper);
	}

	if (IS_ERR_OR_NULL(this))
		return this;

	if (ovl_dentry_real_at(this, layer->idx) != real) {
		dput(this);
		this = ERR_PTR(-EIO);
	}

	return this;
}

 
static struct dentry *ovl_lookup_real_ancestor(struct super_block *sb,
					       struct dentry *real,
					       const struct ovl_layer *layer)
{
	struct dentry *next, *parent = NULL;
	struct dentry *ancestor = ERR_PTR(-EIO);

	if (real == layer->mnt->mnt_root)
		return dget(sb->s_root);

	 
	next = dget(real);
	for (;;) {
		parent = dget_parent(next);

		 
		ancestor = ovl_lookup_real_inode(sb, next, layer);
		if (ancestor)
			break;

		if (parent == layer->mnt->mnt_root) {
			ancestor = dget(sb->s_root);
			break;
		}

		 
		if (parent == next) {
			ancestor = ERR_PTR(-EXDEV);
			break;
		}

		dput(next);
		next = parent;
	}

	dput(parent);
	dput(next);

	return ancestor;
}

 
static struct dentry *ovl_lookup_real(struct super_block *sb,
				      struct dentry *real,
				      const struct ovl_layer *layer)
{
	struct dentry *connected;
	int err = 0;

	connected = ovl_lookup_real_ancestor(sb, real, layer);
	if (IS_ERR(connected))
		return connected;

	while (!err) {
		struct dentry *next, *this;
		struct dentry *parent = NULL;
		struct dentry *real_connected = ovl_dentry_real_at(connected,
								   layer->idx);

		if (real_connected == real)
			break;

		 
		next = dget(real);
		for (;;) {
			parent = dget_parent(next);

			if (parent == real_connected)
				break;

			 
			if (parent == layer->mnt->mnt_root) {
				dput(connected);
				connected = dget(sb->s_root);
				break;
			}

			 
			if (parent == next) {
				err = -EXDEV;
				break;
			}

			dput(next);
			next = parent;
		}

		if (!err) {
			this = ovl_lookup_real_one(connected, next, layer);
			if (IS_ERR(this))
				err = PTR_ERR(this);

			 
			if (err == -ECHILD) {
				this = ovl_lookup_real_ancestor(sb, real,
								layer);
				err = PTR_ERR_OR_ZERO(this);
			}
			if (!err) {
				dput(connected);
				connected = this;
			}
		}

		dput(parent);
		dput(next);
	}

	if (err)
		goto fail;

	return connected;

fail:
	pr_warn_ratelimited("failed to lookup by real (%pd2, layer=%d, connected=%pd2, err=%i)\n",
			    real, layer->idx, connected, err);
	dput(connected);
	return ERR_PTR(err);
}

 
static struct dentry *ovl_get_dentry(struct super_block *sb,
				     struct dentry *upper,
				     struct ovl_path *lowerpath,
				     struct dentry *index)
{
	struct ovl_fs *ofs = OVL_FS(sb);
	const struct ovl_layer *layer = upper ? &ofs->layers[0] : lowerpath->layer;
	struct dentry *real = upper ?: (index ?: lowerpath->dentry);

	 
	if (!d_is_dir(real))
		return ovl_obtain_alias(sb, upper, lowerpath, index);

	 
	if ((real->d_flags & DCACHE_DISCONNECTED) || d_unhashed(real))
		return ERR_PTR(-ENOENT);

	 
	return ovl_lookup_real(sb, real, layer);
}

static struct dentry *ovl_upper_fh_to_d(struct super_block *sb,
					struct ovl_fh *fh)
{
	struct ovl_fs *ofs = OVL_FS(sb);
	struct dentry *dentry;
	struct dentry *upper;

	if (!ovl_upper_mnt(ofs))
		return ERR_PTR(-EACCES);

	upper = ovl_decode_real_fh(ofs, fh, ovl_upper_mnt(ofs), true);
	if (IS_ERR_OR_NULL(upper))
		return upper;

	dentry = ovl_get_dentry(sb, upper, NULL, NULL);
	dput(upper);

	return dentry;
}

static struct dentry *ovl_lower_fh_to_d(struct super_block *sb,
					struct ovl_fh *fh)
{
	struct ovl_fs *ofs = OVL_FS(sb);
	struct ovl_path origin = { };
	struct ovl_path *stack = &origin;
	struct dentry *dentry = NULL;
	struct dentry *index = NULL;
	struct inode *inode;
	int err;

	 
	err = ovl_check_origin_fh(ofs, fh, false, NULL, &stack);
	if (err)
		return ERR_PTR(err);

	if (!d_is_dir(origin.dentry) ||
	    !(origin.dentry->d_flags & DCACHE_DISCONNECTED)) {
		inode = ovl_lookup_inode(sb, origin.dentry, false);
		err = PTR_ERR(inode);
		if (IS_ERR(inode))
			goto out_err;
		if (inode) {
			dentry = d_find_any_alias(inode);
			iput(inode);
			if (dentry)
				goto out;
		}
	}

	 
	if (ofs->indexdir) {
		index = ovl_get_index_fh(ofs, fh);
		err = PTR_ERR(index);
		if (IS_ERR(index)) {
			index = NULL;
			goto out_err;
		}
	}

	 
	if (index && d_is_dir(index)) {
		struct dentry *upper = ovl_index_upper(ofs, index, true);

		err = PTR_ERR(upper);
		if (IS_ERR_OR_NULL(upper))
			goto out_err;

		dentry = ovl_get_dentry(sb, upper, NULL, NULL);
		dput(upper);
		goto out;
	}

	 
	if (d_is_dir(origin.dentry)) {
		dput(origin.dentry);
		origin.dentry = NULL;
		err = ovl_check_origin_fh(ofs, fh, true, NULL, &stack);
		if (err)
			goto out_err;
	}
	if (index) {
		err = ovl_verify_origin(ofs, index, origin.dentry, false);
		if (err)
			goto out_err;
	}

	 
	dentry = ovl_get_dentry(sb, NULL, &origin, index);

out:
	dput(origin.dentry);
	dput(index);
	return dentry;

out_err:
	dentry = ERR_PTR(err);
	goto out;
}

static struct ovl_fh *ovl_fid_to_fh(struct fid *fid, int buflen, int fh_type)
{
	struct ovl_fh *fh;

	 
	if (fh_type == OVL_FILEID_V1)
		return (struct ovl_fh *)fid;

	if (fh_type != OVL_FILEID_V0)
		return ERR_PTR(-EINVAL);

	if (buflen <= OVL_FH_WIRE_OFFSET)
		return ERR_PTR(-EINVAL);

	fh = kzalloc(buflen, GFP_KERNEL);
	if (!fh)
		return ERR_PTR(-ENOMEM);

	 
	memcpy(fh->buf, fid, buflen - OVL_FH_WIRE_OFFSET);
	return fh;
}

static struct dentry *ovl_fh_to_dentry(struct super_block *sb, struct fid *fid,
				       int fh_len, int fh_type)
{
	struct dentry *dentry = NULL;
	struct ovl_fh *fh = NULL;
	int len = fh_len << 2;
	unsigned int flags = 0;
	int err;

	fh = ovl_fid_to_fh(fid, len, fh_type);
	err = PTR_ERR(fh);
	if (IS_ERR(fh))
		goto out_err;

	err = ovl_check_fh_len(fh, len);
	if (err)
		goto out_err;

	flags = fh->fb.flags;
	dentry = (flags & OVL_FH_FLAG_PATH_UPPER) ?
		 ovl_upper_fh_to_d(sb, fh) :
		 ovl_lower_fh_to_d(sb, fh);
	err = PTR_ERR(dentry);
	if (IS_ERR(dentry) && err != -ESTALE)
		goto out_err;

out:
	 
	if (!IS_ERR_OR_NULL(fh) && fh != (void *)fid)
		kfree(fh);

	return dentry;

out_err:
	pr_warn_ratelimited("failed to decode file handle (len=%d, type=%d, flags=%x, err=%i)\n",
			    fh_len, fh_type, flags, err);
	dentry = ERR_PTR(err);
	goto out;
}

static struct dentry *ovl_fh_to_parent(struct super_block *sb, struct fid *fid,
				       int fh_len, int fh_type)
{
	pr_warn_ratelimited("connectable file handles not supported; use 'no_subtree_check' exportfs option.\n");
	return ERR_PTR(-EACCES);
}

static int ovl_get_name(struct dentry *parent, char *name,
			struct dentry *child)
{
	 
	WARN_ON_ONCE(1);
	return -EIO;
}

static struct dentry *ovl_get_parent(struct dentry *dentry)
{
	 
	WARN_ON_ONCE(1);
	return ERR_PTR(-EIO);
}

const struct export_operations ovl_export_operations = {
	.encode_fh	= ovl_encode_fh,
	.fh_to_dentry	= ovl_fh_to_dentry,
	.fh_to_parent	= ovl_fh_to_parent,
	.get_name	= ovl_get_name,
	.get_parent	= ovl_get_parent,
};

 
const struct export_operations ovl_export_fid_operations = {
	.encode_fh	= ovl_encode_fh,
};
