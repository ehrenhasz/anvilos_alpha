
 

#include "fscrypt_private.h"

 
int fscrypt_file_open(struct inode *inode, struct file *filp)
{
	int err;
	struct dentry *dir;

	err = fscrypt_require_key(inode);
	if (err)
		return err;

	dir = dget_parent(file_dentry(filp));
	if (IS_ENCRYPTED(d_inode(dir)) &&
	    !fscrypt_has_permitted_context(d_inode(dir), inode)) {
		fscrypt_warn(inode,
			     "Inconsistent encryption context (parent directory: %lu)",
			     d_inode(dir)->i_ino);
		err = -EPERM;
	}
	dput(dir);
	return err;
}
EXPORT_SYMBOL_GPL(fscrypt_file_open);

int __fscrypt_prepare_link(struct inode *inode, struct inode *dir,
			   struct dentry *dentry)
{
	if (fscrypt_is_nokey_name(dentry))
		return -ENOKEY;
	 

	if (!fscrypt_has_permitted_context(dir, inode))
		return -EXDEV;

	return 0;
}
EXPORT_SYMBOL_GPL(__fscrypt_prepare_link);

int __fscrypt_prepare_rename(struct inode *old_dir, struct dentry *old_dentry,
			     struct inode *new_dir, struct dentry *new_dentry,
			     unsigned int flags)
{
	if (fscrypt_is_nokey_name(old_dentry) ||
	    fscrypt_is_nokey_name(new_dentry))
		return -ENOKEY;
	 

	if (old_dir != new_dir) {
		if (IS_ENCRYPTED(new_dir) &&
		    !fscrypt_has_permitted_context(new_dir,
						   d_inode(old_dentry)))
			return -EXDEV;

		if ((flags & RENAME_EXCHANGE) &&
		    IS_ENCRYPTED(old_dir) &&
		    !fscrypt_has_permitted_context(old_dir,
						   d_inode(new_dentry)))
			return -EXDEV;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(__fscrypt_prepare_rename);

int __fscrypt_prepare_lookup(struct inode *dir, struct dentry *dentry,
			     struct fscrypt_name *fname)
{
	int err = fscrypt_setup_filename(dir, &dentry->d_name, 1, fname);

	if (err && err != -ENOENT)
		return err;

	if (fname->is_nokey_name) {
		spin_lock(&dentry->d_lock);
		dentry->d_flags |= DCACHE_NOKEY_NAME;
		spin_unlock(&dentry->d_lock);
	}
	return err;
}
EXPORT_SYMBOL_GPL(__fscrypt_prepare_lookup);

 
int fscrypt_prepare_lookup_partial(struct inode *dir, struct dentry *dentry)
{
	int err = fscrypt_get_encryption_info(dir, true);

	if (!err && !fscrypt_has_encryption_key(dir)) {
		spin_lock(&dentry->d_lock);
		dentry->d_flags |= DCACHE_NOKEY_NAME;
		spin_unlock(&dentry->d_lock);
	}
	return err;
}
EXPORT_SYMBOL_GPL(fscrypt_prepare_lookup_partial);

int __fscrypt_prepare_readdir(struct inode *dir)
{
	return fscrypt_get_encryption_info(dir, true);
}
EXPORT_SYMBOL_GPL(__fscrypt_prepare_readdir);

int __fscrypt_prepare_setattr(struct dentry *dentry, struct iattr *attr)
{
	if (attr->ia_valid & ATTR_SIZE)
		return fscrypt_require_key(d_inode(dentry));
	return 0;
}
EXPORT_SYMBOL_GPL(__fscrypt_prepare_setattr);

 
int fscrypt_prepare_setflags(struct inode *inode,
			     unsigned int oldflags, unsigned int flags)
{
	struct fscrypt_info *ci;
	struct fscrypt_master_key *mk;
	int err;

	 
	if (IS_ENCRYPTED(inode) && (flags & ~oldflags & FS_CASEFOLD_FL)) {
		err = fscrypt_require_key(inode);
		if (err)
			return err;
		ci = inode->i_crypt_info;
		if (ci->ci_policy.version != FSCRYPT_POLICY_V2)
			return -EINVAL;
		mk = ci->ci_master_key;
		down_read(&mk->mk_sem);
		if (is_master_key_secret_present(&mk->mk_secret))
			err = fscrypt_derive_dirhash_key(ci, mk);
		else
			err = -ENOKEY;
		up_read(&mk->mk_sem);
		return err;
	}
	return 0;
}

 
int fscrypt_prepare_symlink(struct inode *dir, const char *target,
			    unsigned int len, unsigned int max_len,
			    struct fscrypt_str *disk_link)
{
	const union fscrypt_policy *policy;

	 
	policy = fscrypt_policy_to_inherit(dir);
	if (policy == NULL) {
		 
		disk_link->name = (unsigned char *)target;
		disk_link->len = len + 1;
		if (disk_link->len > max_len)
			return -ENAMETOOLONG;
		return 0;
	}
	if (IS_ERR(policy))
		return PTR_ERR(policy);

	 
	if (!__fscrypt_fname_encrypted_size(policy, len,
					    max_len - sizeof(struct fscrypt_symlink_data) - 1,
					    &disk_link->len))
		return -ENAMETOOLONG;
	disk_link->len += sizeof(struct fscrypt_symlink_data) + 1;

	disk_link->name = NULL;
	return 0;
}
EXPORT_SYMBOL_GPL(fscrypt_prepare_symlink);

int __fscrypt_encrypt_symlink(struct inode *inode, const char *target,
			      unsigned int len, struct fscrypt_str *disk_link)
{
	int err;
	struct qstr iname = QSTR_INIT(target, len);
	struct fscrypt_symlink_data *sd;
	unsigned int ciphertext_len;

	 
	if (WARN_ON_ONCE(!fscrypt_has_encryption_key(inode)))
		return -ENOKEY;

	if (disk_link->name) {
		 
		sd = (struct fscrypt_symlink_data *)disk_link->name;
	} else {
		sd = kmalloc(disk_link->len, GFP_NOFS);
		if (!sd)
			return -ENOMEM;
	}
	ciphertext_len = disk_link->len - sizeof(*sd) - 1;
	sd->len = cpu_to_le16(ciphertext_len);

	err = fscrypt_fname_encrypt(inode, &iname, sd->encrypted_path,
				    ciphertext_len);
	if (err)
		goto err_free_sd;

	 
	sd->encrypted_path[ciphertext_len] = '\0';

	 
	err = -ENOMEM;
	inode->i_link = kmemdup(target, len + 1, GFP_NOFS);
	if (!inode->i_link)
		goto err_free_sd;

	if (!disk_link->name)
		disk_link->name = (unsigned char *)sd;
	return 0;

err_free_sd:
	if (!disk_link->name)
		kfree(sd);
	return err;
}
EXPORT_SYMBOL_GPL(__fscrypt_encrypt_symlink);

 
const char *fscrypt_get_symlink(struct inode *inode, const void *caddr,
				unsigned int max_size,
				struct delayed_call *done)
{
	const struct fscrypt_symlink_data *sd;
	struct fscrypt_str cstr, pstr;
	bool has_key;
	int err;

	 
	if (WARN_ON_ONCE(!IS_ENCRYPTED(inode)))
		return ERR_PTR(-EINVAL);

	 
	pstr.name = READ_ONCE(inode->i_link);
	if (pstr.name)
		return pstr.name;

	 
	err = fscrypt_get_encryption_info(inode, false);
	if (err)
		return ERR_PTR(err);
	has_key = fscrypt_has_encryption_key(inode);

	 

	if (max_size < sizeof(*sd) + 1)
		return ERR_PTR(-EUCLEAN);
	sd = caddr;
	cstr.name = (unsigned char *)sd->encrypted_path;
	cstr.len = le16_to_cpu(sd->len);

	if (cstr.len == 0)
		return ERR_PTR(-EUCLEAN);

	if (cstr.len + sizeof(*sd) > max_size)
		return ERR_PTR(-EUCLEAN);

	err = fscrypt_fname_alloc_buffer(cstr.len, &pstr);
	if (err)
		return ERR_PTR(err);

	err = fscrypt_fname_disk_to_usr(inode, 0, 0, &cstr, &pstr);
	if (err)
		goto err_kfree;

	err = -EUCLEAN;
	if (pstr.name[0] == '\0')
		goto err_kfree;

	pstr.name[pstr.len] = '\0';

	 
	if (!has_key ||
	    cmpxchg_release(&inode->i_link, NULL, pstr.name) != NULL)
		set_delayed_call(done, kfree_link, pstr.name);

	return pstr.name;

err_kfree:
	kfree(pstr.name);
	return ERR_PTR(err);
}
EXPORT_SYMBOL_GPL(fscrypt_get_symlink);

 
int fscrypt_symlink_getattr(const struct path *path, struct kstat *stat)
{
	struct dentry *dentry = path->dentry;
	struct inode *inode = d_inode(dentry);
	const char *link;
	DEFINE_DELAYED_CALL(done);

	 
	link = READ_ONCE(inode->i_link);
	if (!link) {
		link = inode->i_op->get_link(dentry, inode, &done);
		if (IS_ERR(link))
			return PTR_ERR(link);
	}
	stat->size = strlen(link);
	do_delayed_call(&done);
	return 0;
}
EXPORT_SYMBOL_GPL(fscrypt_symlink_getattr);
