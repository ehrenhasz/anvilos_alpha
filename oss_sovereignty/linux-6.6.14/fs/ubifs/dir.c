
 

 

#include "ubifs.h"

 
static int inherit_flags(const struct inode *dir, umode_t mode)
{
	int flags;
	const struct ubifs_inode *ui = ubifs_inode(dir);

	if (!S_ISDIR(dir->i_mode))
		 
		return 0;

	flags = ui->flags & (UBIFS_COMPR_FL | UBIFS_SYNC_FL | UBIFS_DIRSYNC_FL);
	if (!S_ISDIR(mode))
		 
		flags &= ~UBIFS_DIRSYNC_FL;
	return flags;
}

 
struct inode *ubifs_new_inode(struct ubifs_info *c, struct inode *dir,
			      umode_t mode, bool is_xattr)
{
	int err;
	struct inode *inode;
	struct ubifs_inode *ui;
	bool encrypted = false;

	inode = new_inode(c->vfs_sb);
	ui = ubifs_inode(inode);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	 
	inode->i_flags |= S_NOCMTIME;

	inode_init_owner(&nop_mnt_idmap, inode, dir, mode);
	inode->i_mtime = inode->i_atime = inode_set_ctime_current(inode);
	inode->i_mapping->nrpages = 0;

	if (!is_xattr) {
		err = fscrypt_prepare_new_inode(dir, inode, &encrypted);
		if (err) {
			ubifs_err(c, "fscrypt_prepare_new_inode failed: %i", err);
			goto out_iput;
		}
	}

	switch (mode & S_IFMT) {
	case S_IFREG:
		inode->i_mapping->a_ops = &ubifs_file_address_operations;
		inode->i_op = &ubifs_file_inode_operations;
		inode->i_fop = &ubifs_file_operations;
		break;
	case S_IFDIR:
		inode->i_op  = &ubifs_dir_inode_operations;
		inode->i_fop = &ubifs_dir_operations;
		inode->i_size = ui->ui_size = UBIFS_INO_NODE_SZ;
		break;
	case S_IFLNK:
		inode->i_op = &ubifs_symlink_inode_operations;
		break;
	case S_IFSOCK:
	case S_IFIFO:
	case S_IFBLK:
	case S_IFCHR:
		inode->i_op  = &ubifs_file_inode_operations;
		break;
	default:
		BUG();
	}

	ui->flags = inherit_flags(dir, mode);
	ubifs_set_inode_flags(inode);
	if (S_ISREG(mode))
		ui->compr_type = c->default_compr;
	else
		ui->compr_type = UBIFS_COMPR_NONE;
	ui->synced_i_size = 0;

	spin_lock(&c->cnt_lock);
	 
	if (c->highest_inum >= INUM_WARN_WATERMARK) {
		if (c->highest_inum >= INUM_WATERMARK) {
			spin_unlock(&c->cnt_lock);
			ubifs_err(c, "out of inode numbers");
			err = -EINVAL;
			goto out_iput;
		}
		ubifs_warn(c, "running out of inode numbers (current %lu, max %u)",
			   (unsigned long)c->highest_inum, INUM_WATERMARK);
	}

	inode->i_ino = ++c->highest_inum;
	 
	ui->creat_sqnum = ++c->max_sqnum;
	spin_unlock(&c->cnt_lock);

	if (encrypted) {
		err = fscrypt_set_context(inode, NULL);
		if (err) {
			ubifs_err(c, "fscrypt_set_context failed: %i", err);
			goto out_iput;
		}
	}

	return inode;

out_iput:
	make_bad_inode(inode);
	iput(inode);
	return ERR_PTR(err);
}

static int dbg_check_name(const struct ubifs_info *c,
			  const struct ubifs_dent_node *dent,
			  const struct fscrypt_name *nm)
{
	if (!dbg_is_chk_gen(c))
		return 0;
	if (le16_to_cpu(dent->nlen) != fname_len(nm))
		return -EINVAL;
	if (memcmp(dent->name, fname_name(nm), fname_len(nm)))
		return -EINVAL;
	return 0;
}

static struct dentry *ubifs_lookup(struct inode *dir, struct dentry *dentry,
				   unsigned int flags)
{
	int err;
	union ubifs_key key;
	struct inode *inode = NULL;
	struct ubifs_dent_node *dent = NULL;
	struct ubifs_info *c = dir->i_sb->s_fs_info;
	struct fscrypt_name nm;

	dbg_gen("'%pd' in dir ino %lu", dentry, dir->i_ino);

	err = fscrypt_prepare_lookup(dir, dentry, &nm);
	generic_set_encrypted_ci_d_ops(dentry);
	if (err == -ENOENT)
		return d_splice_alias(NULL, dentry);
	if (err)
		return ERR_PTR(err);

	if (fname_len(&nm) > UBIFS_MAX_NLEN) {
		inode = ERR_PTR(-ENAMETOOLONG);
		goto done;
	}

	dent = kmalloc(UBIFS_MAX_DENT_NODE_SZ, GFP_NOFS);
	if (!dent) {
		inode = ERR_PTR(-ENOMEM);
		goto done;
	}

	if (fname_name(&nm) == NULL) {
		if (nm.hash & ~UBIFS_S_KEY_HASH_MASK)
			goto done;  
		dent_key_init_hash(c, &key, dir->i_ino, nm.hash);
		err = ubifs_tnc_lookup_dh(c, &key, dent, nm.minor_hash);
	} else {
		dent_key_init(c, &key, dir->i_ino, &nm);
		err = ubifs_tnc_lookup_nm(c, &key, dent, &nm);
	}

	if (err) {
		if (err == -ENOENT)
			dbg_gen("not found");
		else
			inode = ERR_PTR(err);
		goto done;
	}

	if (dbg_check_name(c, dent, &nm)) {
		inode = ERR_PTR(-EINVAL);
		goto done;
	}

	inode = ubifs_iget(dir->i_sb, le64_to_cpu(dent->inum));
	if (IS_ERR(inode)) {
		 
		err = PTR_ERR(inode);
		ubifs_err(c, "dead directory entry '%pd', error %d",
			  dentry, err);
		ubifs_ro_mode(c, err);
		goto done;
	}

	if (IS_ENCRYPTED(dir) &&
	    (S_ISDIR(inode->i_mode) || S_ISLNK(inode->i_mode)) &&
	    !fscrypt_has_permitted_context(dir, inode)) {
		ubifs_warn(c, "Inconsistent encryption contexts: %lu/%lu",
			   dir->i_ino, inode->i_ino);
		iput(inode);
		inode = ERR_PTR(-EPERM);
	}

done:
	kfree(dent);
	fscrypt_free_filename(&nm);
	return d_splice_alias(inode, dentry);
}

static int ubifs_prepare_create(struct inode *dir, struct dentry *dentry,
				struct fscrypt_name *nm)
{
	if (fscrypt_is_nokey_name(dentry))
		return -ENOKEY;

	return fscrypt_setup_filename(dir, &dentry->d_name, 0, nm);
}

static int ubifs_create(struct mnt_idmap *idmap, struct inode *dir,
			struct dentry *dentry, umode_t mode, bool excl)
{
	struct inode *inode;
	struct ubifs_info *c = dir->i_sb->s_fs_info;
	struct ubifs_budget_req req = { .new_ino = 1, .new_dent = 1,
					.dirtied_ino = 1 };
	struct ubifs_inode *dir_ui = ubifs_inode(dir);
	struct fscrypt_name nm;
	int err, sz_change;

	 

	dbg_gen("dent '%pd', mode %#hx in dir ino %lu",
		dentry, mode, dir->i_ino);

	err = ubifs_budget_space(c, &req);
	if (err)
		return err;

	err = ubifs_prepare_create(dir, dentry, &nm);
	if (err)
		goto out_budg;

	sz_change = CALC_DENT_SIZE(fname_len(&nm));

	inode = ubifs_new_inode(c, dir, mode, false);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_fname;
	}

	err = ubifs_init_security(dir, inode, &dentry->d_name);
	if (err)
		goto out_inode;

	mutex_lock(&dir_ui->ui_mutex);
	dir->i_size += sz_change;
	dir_ui->ui_size = dir->i_size;
	dir->i_mtime = inode_set_ctime_to_ts(dir, inode_get_ctime(inode));
	err = ubifs_jnl_update(c, dir, &nm, inode, 0, 0);
	if (err)
		goto out_cancel;
	mutex_unlock(&dir_ui->ui_mutex);

	ubifs_release_budget(c, &req);
	fscrypt_free_filename(&nm);
	insert_inode_hash(inode);
	d_instantiate(dentry, inode);
	return 0;

out_cancel:
	dir->i_size -= sz_change;
	dir_ui->ui_size = dir->i_size;
	mutex_unlock(&dir_ui->ui_mutex);
out_inode:
	make_bad_inode(inode);
	iput(inode);
out_fname:
	fscrypt_free_filename(&nm);
out_budg:
	ubifs_release_budget(c, &req);
	ubifs_err(c, "cannot create regular file, error %d", err);
	return err;
}

static struct inode *create_whiteout(struct inode *dir, struct dentry *dentry)
{
	int err;
	umode_t mode = S_IFCHR | WHITEOUT_MODE;
	struct inode *inode;
	struct ubifs_info *c = dir->i_sb->s_fs_info;

	 

	dbg_gen("dent '%pd', mode %#hx in dir ino %lu",
		dentry, mode, dir->i_ino);

	inode = ubifs_new_inode(c, dir, mode, false);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_free;
	}

	init_special_inode(inode, inode->i_mode, WHITEOUT_DEV);
	ubifs_assert(c, inode->i_op == &ubifs_file_inode_operations);

	err = ubifs_init_security(dir, inode, &dentry->d_name);
	if (err)
		goto out_inode;

	 
	insert_inode_hash(inode);

	return inode;

out_inode:
	make_bad_inode(inode);
	iput(inode);
out_free:
	ubifs_err(c, "cannot create whiteout file, error %d", err);
	return ERR_PTR(err);
}

 
static void lock_2_inodes(struct inode *inode1, struct inode *inode2)
{
	mutex_lock_nested(&ubifs_inode(inode1)->ui_mutex, WB_MUTEX_1);
	mutex_lock_nested(&ubifs_inode(inode2)->ui_mutex, WB_MUTEX_2);
}

 
static void unlock_2_inodes(struct inode *inode1, struct inode *inode2)
{
	mutex_unlock(&ubifs_inode(inode2)->ui_mutex);
	mutex_unlock(&ubifs_inode(inode1)->ui_mutex);
}

static int ubifs_tmpfile(struct mnt_idmap *idmap, struct inode *dir,
			 struct file *file, umode_t mode)
{
	struct dentry *dentry = file->f_path.dentry;
	struct inode *inode;
	struct ubifs_info *c = dir->i_sb->s_fs_info;
	struct ubifs_budget_req req = { .new_ino = 1, .new_dent = 1,
					.dirtied_ino = 1};
	struct ubifs_budget_req ino_req = { .dirtied_ino = 1 };
	struct ubifs_inode *ui;
	int err, instantiated = 0;
	struct fscrypt_name nm;

	 

	dbg_gen("dent '%pd', mode %#hx in dir ino %lu",
		dentry, mode, dir->i_ino);

	err = fscrypt_setup_filename(dir, &dentry->d_name, 0, &nm);
	if (err)
		return err;

	err = ubifs_budget_space(c, &req);
	if (err) {
		fscrypt_free_filename(&nm);
		return err;
	}

	err = ubifs_budget_space(c, &ino_req);
	if (err) {
		ubifs_release_budget(c, &req);
		fscrypt_free_filename(&nm);
		return err;
	}

	inode = ubifs_new_inode(c, dir, mode, false);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_budg;
	}
	ui = ubifs_inode(inode);

	err = ubifs_init_security(dir, inode, &dentry->d_name);
	if (err)
		goto out_inode;

	mutex_lock(&ui->ui_mutex);
	insert_inode_hash(inode);
	d_tmpfile(file, inode);
	ubifs_assert(c, ui->dirty);

	instantiated = 1;
	mutex_unlock(&ui->ui_mutex);

	lock_2_inodes(dir, inode);
	err = ubifs_jnl_update(c, dir, &nm, inode, 1, 0);
	if (err)
		goto out_cancel;
	unlock_2_inodes(dir, inode);

	ubifs_release_budget(c, &req);
	fscrypt_free_filename(&nm);

	return finish_open_simple(file, 0);

out_cancel:
	unlock_2_inodes(dir, inode);
out_inode:
	make_bad_inode(inode);
	if (!instantiated)
		iput(inode);
out_budg:
	ubifs_release_budget(c, &req);
	if (!instantiated)
		ubifs_release_budget(c, &ino_req);
	fscrypt_free_filename(&nm);
	ubifs_err(c, "cannot create temporary file, error %d", err);
	return err;
}

 
static unsigned int vfs_dent_type(uint8_t type)
{
	switch (type) {
	case UBIFS_ITYPE_REG:
		return DT_REG;
	case UBIFS_ITYPE_DIR:
		return DT_DIR;
	case UBIFS_ITYPE_LNK:
		return DT_LNK;
	case UBIFS_ITYPE_BLK:
		return DT_BLK;
	case UBIFS_ITYPE_CHR:
		return DT_CHR;
	case UBIFS_ITYPE_FIFO:
		return DT_FIFO;
	case UBIFS_ITYPE_SOCK:
		return DT_SOCK;
	default:
		BUG();
	}
	return 0;
}

 
static int ubifs_readdir(struct file *file, struct dir_context *ctx)
{
	int fstr_real_len = 0, err = 0;
	struct fscrypt_name nm;
	struct fscrypt_str fstr = {0};
	union ubifs_key key;
	struct ubifs_dent_node *dent;
	struct inode *dir = file_inode(file);
	struct ubifs_info *c = dir->i_sb->s_fs_info;
	bool encrypted = IS_ENCRYPTED(dir);

	dbg_gen("dir ino %lu, f_pos %#llx", dir->i_ino, ctx->pos);

	if (ctx->pos > UBIFS_S_KEY_HASH_MASK || ctx->pos == 2)
		 
		return 0;

	if (encrypted) {
		err = fscrypt_prepare_readdir(dir);
		if (err)
			return err;

		err = fscrypt_fname_alloc_buffer(UBIFS_MAX_NLEN, &fstr);
		if (err)
			return err;

		fstr_real_len = fstr.len;
	}

	if (file->f_version == 0) {
		 
		kfree(file->private_data);
		file->private_data = NULL;
	}

	 
	file->f_version = 1;

	 
	if (ctx->pos < 2) {
		ubifs_assert(c, !file->private_data);
		if (!dir_emit_dots(file, ctx)) {
			if (encrypted)
				fscrypt_fname_free_buffer(&fstr);
			return 0;
		}

		 
		lowest_dent_key(c, &key, dir->i_ino);
		fname_len(&nm) = 0;
		dent = ubifs_tnc_next_ent(c, &key, &nm);
		if (IS_ERR(dent)) {
			err = PTR_ERR(dent);
			goto out;
		}

		ctx->pos = key_hash_flash(c, &dent->key);
		file->private_data = dent;
	}

	dent = file->private_data;
	if (!dent) {
		 
		dent_key_init_hash(c, &key, dir->i_ino, ctx->pos);
		fname_len(&nm) = 0;
		dent = ubifs_tnc_next_ent(c, &key, &nm);
		if (IS_ERR(dent)) {
			err = PTR_ERR(dent);
			goto out;
		}
		ctx->pos = key_hash_flash(c, &dent->key);
		file->private_data = dent;
	}

	while (1) {
		dbg_gen("ino %llu, new f_pos %#x",
			(unsigned long long)le64_to_cpu(dent->inum),
			key_hash_flash(c, &dent->key));
		ubifs_assert(c, le64_to_cpu(dent->ch.sqnum) >
			     ubifs_inode(dir)->creat_sqnum);

		fname_len(&nm) = le16_to_cpu(dent->nlen);
		fname_name(&nm) = dent->name;

		if (encrypted) {
			fstr.len = fstr_real_len;

			err = fscrypt_fname_disk_to_usr(dir, key_hash_flash(c,
							&dent->key),
							le32_to_cpu(dent->cookie),
							&nm.disk_name, &fstr);
			if (err)
				goto out;
		} else {
			fstr.len = fname_len(&nm);
			fstr.name = fname_name(&nm);
		}

		if (!dir_emit(ctx, fstr.name, fstr.len,
			       le64_to_cpu(dent->inum),
			       vfs_dent_type(dent->type))) {
			if (encrypted)
				fscrypt_fname_free_buffer(&fstr);
			return 0;
		}

		 
		key_read(c, &dent->key, &key);
		dent = ubifs_tnc_next_ent(c, &key, &nm);
		if (IS_ERR(dent)) {
			err = PTR_ERR(dent);
			goto out;
		}

		kfree(file->private_data);
		ctx->pos = key_hash_flash(c, &dent->key);
		file->private_data = dent;
		cond_resched();
	}

out:
	kfree(file->private_data);
	file->private_data = NULL;

	if (encrypted)
		fscrypt_fname_free_buffer(&fstr);

	if (err != -ENOENT)
		ubifs_err(c, "cannot find next direntry, error %d", err);
	else
		 
		err = 0;


	 
	ctx->pos = 2;
	return err;
}

 
static int ubifs_dir_release(struct inode *dir, struct file *file)
{
	kfree(file->private_data);
	file->private_data = NULL;
	return 0;
}

static int ubifs_link(struct dentry *old_dentry, struct inode *dir,
		      struct dentry *dentry)
{
	struct ubifs_info *c = dir->i_sb->s_fs_info;
	struct inode *inode = d_inode(old_dentry);
	struct ubifs_inode *ui = ubifs_inode(inode);
	struct ubifs_inode *dir_ui = ubifs_inode(dir);
	int err, sz_change = CALC_DENT_SIZE(dentry->d_name.len);
	struct ubifs_budget_req req = { .new_dent = 1, .dirtied_ino = 2,
				.dirtied_ino_d = ALIGN(ui->data_len, 8) };
	struct fscrypt_name nm;

	 

	dbg_gen("dent '%pd' to ino %lu (nlink %d) in dir ino %lu",
		dentry, inode->i_ino,
		inode->i_nlink, dir->i_ino);
	ubifs_assert(c, inode_is_locked(dir));
	ubifs_assert(c, inode_is_locked(inode));

	err = fscrypt_prepare_link(old_dentry, dir, dentry);
	if (err)
		return err;

	err = fscrypt_setup_filename(dir, &dentry->d_name, 0, &nm);
	if (err)
		return err;

	err = dbg_check_synced_i_size(c, inode);
	if (err)
		goto out_fname;

	err = ubifs_budget_space(c, &req);
	if (err)
		goto out_fname;

	lock_2_inodes(dir, inode);

	 
	if (inode->i_nlink == 0)
		ubifs_delete_orphan(c, inode->i_ino);

	inc_nlink(inode);
	ihold(inode);
	inode_set_ctime_current(inode);
	dir->i_size += sz_change;
	dir_ui->ui_size = dir->i_size;
	dir->i_mtime = inode_set_ctime_to_ts(dir, inode_get_ctime(inode));
	err = ubifs_jnl_update(c, dir, &nm, inode, 0, 0);
	if (err)
		goto out_cancel;
	unlock_2_inodes(dir, inode);

	ubifs_release_budget(c, &req);
	d_instantiate(dentry, inode);
	fscrypt_free_filename(&nm);
	return 0;

out_cancel:
	dir->i_size -= sz_change;
	dir_ui->ui_size = dir->i_size;
	drop_nlink(inode);
	if (inode->i_nlink == 0)
		ubifs_add_orphan(c, inode->i_ino);
	unlock_2_inodes(dir, inode);
	ubifs_release_budget(c, &req);
	iput(inode);
out_fname:
	fscrypt_free_filename(&nm);
	return err;
}

static int ubifs_unlink(struct inode *dir, struct dentry *dentry)
{
	struct ubifs_info *c = dir->i_sb->s_fs_info;
	struct inode *inode = d_inode(dentry);
	struct ubifs_inode *dir_ui = ubifs_inode(dir);
	int err, sz_change, budgeted = 1;
	struct ubifs_budget_req req = { .mod_dent = 1, .dirtied_ino = 2 };
	unsigned int saved_nlink = inode->i_nlink;
	struct fscrypt_name nm;

	 

	dbg_gen("dent '%pd' from ino %lu (nlink %d) in dir ino %lu",
		dentry, inode->i_ino,
		inode->i_nlink, dir->i_ino);

	err = fscrypt_setup_filename(dir, &dentry->d_name, 1, &nm);
	if (err)
		return err;

	err = ubifs_purge_xattrs(inode);
	if (err)
		return err;

	sz_change = CALC_DENT_SIZE(fname_len(&nm));

	ubifs_assert(c, inode_is_locked(dir));
	ubifs_assert(c, inode_is_locked(inode));
	err = dbg_check_synced_i_size(c, inode);
	if (err)
		goto out_fname;

	err = ubifs_budget_space(c, &req);
	if (err) {
		if (err != -ENOSPC)
			goto out_fname;
		budgeted = 0;
	}

	lock_2_inodes(dir, inode);
	inode_set_ctime_current(inode);
	drop_nlink(inode);
	dir->i_size -= sz_change;
	dir_ui->ui_size = dir->i_size;
	dir->i_mtime = inode_set_ctime_to_ts(dir, inode_get_ctime(inode));
	err = ubifs_jnl_update(c, dir, &nm, inode, 1, 0);
	if (err)
		goto out_cancel;
	unlock_2_inodes(dir, inode);

	if (budgeted)
		ubifs_release_budget(c, &req);
	else {
		 
		c->bi.nospace = c->bi.nospace_rp = 0;
		smp_wmb();
	}
	fscrypt_free_filename(&nm);
	return 0;

out_cancel:
	dir->i_size += sz_change;
	dir_ui->ui_size = dir->i_size;
	set_nlink(inode, saved_nlink);
	unlock_2_inodes(dir, inode);
	if (budgeted)
		ubifs_release_budget(c, &req);
out_fname:
	fscrypt_free_filename(&nm);
	return err;
}

 
int ubifs_check_dir_empty(struct inode *dir)
{
	struct ubifs_info *c = dir->i_sb->s_fs_info;
	struct fscrypt_name nm = { 0 };
	struct ubifs_dent_node *dent;
	union ubifs_key key;
	int err;

	lowest_dent_key(c, &key, dir->i_ino);
	dent = ubifs_tnc_next_ent(c, &key, &nm);
	if (IS_ERR(dent)) {
		err = PTR_ERR(dent);
		if (err == -ENOENT)
			err = 0;
	} else {
		kfree(dent);
		err = -ENOTEMPTY;
	}
	return err;
}

static int ubifs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct ubifs_info *c = dir->i_sb->s_fs_info;
	struct inode *inode = d_inode(dentry);
	int err, sz_change, budgeted = 1;
	struct ubifs_inode *dir_ui = ubifs_inode(dir);
	struct ubifs_budget_req req = { .mod_dent = 1, .dirtied_ino = 2 };
	struct fscrypt_name nm;

	 

	dbg_gen("directory '%pd', ino %lu in dir ino %lu", dentry,
		inode->i_ino, dir->i_ino);
	ubifs_assert(c, inode_is_locked(dir));
	ubifs_assert(c, inode_is_locked(inode));
	err = ubifs_check_dir_empty(d_inode(dentry));
	if (err)
		return err;

	err = fscrypt_setup_filename(dir, &dentry->d_name, 1, &nm);
	if (err)
		return err;

	err = ubifs_purge_xattrs(inode);
	if (err)
		return err;

	sz_change = CALC_DENT_SIZE(fname_len(&nm));

	err = ubifs_budget_space(c, &req);
	if (err) {
		if (err != -ENOSPC)
			goto out_fname;
		budgeted = 0;
	}

	lock_2_inodes(dir, inode);
	inode_set_ctime_current(inode);
	clear_nlink(inode);
	drop_nlink(dir);
	dir->i_size -= sz_change;
	dir_ui->ui_size = dir->i_size;
	dir->i_mtime = inode_set_ctime_to_ts(dir, inode_get_ctime(inode));
	err = ubifs_jnl_update(c, dir, &nm, inode, 1, 0);
	if (err)
		goto out_cancel;
	unlock_2_inodes(dir, inode);

	if (budgeted)
		ubifs_release_budget(c, &req);
	else {
		 
		c->bi.nospace = c->bi.nospace_rp = 0;
		smp_wmb();
	}
	fscrypt_free_filename(&nm);
	return 0;

out_cancel:
	dir->i_size += sz_change;
	dir_ui->ui_size = dir->i_size;
	inc_nlink(dir);
	set_nlink(inode, 2);
	unlock_2_inodes(dir, inode);
	if (budgeted)
		ubifs_release_budget(c, &req);
out_fname:
	fscrypt_free_filename(&nm);
	return err;
}

static int ubifs_mkdir(struct mnt_idmap *idmap, struct inode *dir,
		       struct dentry *dentry, umode_t mode)
{
	struct inode *inode;
	struct ubifs_inode *dir_ui = ubifs_inode(dir);
	struct ubifs_info *c = dir->i_sb->s_fs_info;
	int err, sz_change;
	struct ubifs_budget_req req = { .new_ino = 1, .new_dent = 1,
					.dirtied_ino = 1};
	struct fscrypt_name nm;

	 

	dbg_gen("dent '%pd', mode %#hx in dir ino %lu",
		dentry, mode, dir->i_ino);

	err = ubifs_budget_space(c, &req);
	if (err)
		return err;

	err = ubifs_prepare_create(dir, dentry, &nm);
	if (err)
		goto out_budg;

	sz_change = CALC_DENT_SIZE(fname_len(&nm));

	inode = ubifs_new_inode(c, dir, S_IFDIR | mode, false);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_fname;
	}

	err = ubifs_init_security(dir, inode, &dentry->d_name);
	if (err)
		goto out_inode;

	mutex_lock(&dir_ui->ui_mutex);
	insert_inode_hash(inode);
	inc_nlink(inode);
	inc_nlink(dir);
	dir->i_size += sz_change;
	dir_ui->ui_size = dir->i_size;
	dir->i_mtime = inode_set_ctime_to_ts(dir, inode_get_ctime(inode));
	err = ubifs_jnl_update(c, dir, &nm, inode, 0, 0);
	if (err) {
		ubifs_err(c, "cannot create directory, error %d", err);
		goto out_cancel;
	}
	mutex_unlock(&dir_ui->ui_mutex);

	ubifs_release_budget(c, &req);
	d_instantiate(dentry, inode);
	fscrypt_free_filename(&nm);
	return 0;

out_cancel:
	dir->i_size -= sz_change;
	dir_ui->ui_size = dir->i_size;
	drop_nlink(dir);
	mutex_unlock(&dir_ui->ui_mutex);
out_inode:
	make_bad_inode(inode);
	iput(inode);
out_fname:
	fscrypt_free_filename(&nm);
out_budg:
	ubifs_release_budget(c, &req);
	return err;
}

static int ubifs_mknod(struct mnt_idmap *idmap, struct inode *dir,
		       struct dentry *dentry, umode_t mode, dev_t rdev)
{
	struct inode *inode;
	struct ubifs_inode *ui;
	struct ubifs_inode *dir_ui = ubifs_inode(dir);
	struct ubifs_info *c = dir->i_sb->s_fs_info;
	union ubifs_dev_desc *dev = NULL;
	int sz_change;
	int err, devlen = 0;
	struct ubifs_budget_req req = { .new_ino = 1, .new_dent = 1,
					.dirtied_ino = 1 };
	struct fscrypt_name nm;

	 

	dbg_gen("dent '%pd' in dir ino %lu", dentry, dir->i_ino);

	if (S_ISBLK(mode) || S_ISCHR(mode)) {
		dev = kmalloc(sizeof(union ubifs_dev_desc), GFP_NOFS);
		if (!dev)
			return -ENOMEM;
		devlen = ubifs_encode_dev(dev, rdev);
	}

	req.new_ino_d = ALIGN(devlen, 8);
	err = ubifs_budget_space(c, &req);
	if (err) {
		kfree(dev);
		return err;
	}

	err = ubifs_prepare_create(dir, dentry, &nm);
	if (err) {
		kfree(dev);
		goto out_budg;
	}

	sz_change = CALC_DENT_SIZE(fname_len(&nm));

	inode = ubifs_new_inode(c, dir, mode, false);
	if (IS_ERR(inode)) {
		kfree(dev);
		err = PTR_ERR(inode);
		goto out_fname;
	}

	init_special_inode(inode, inode->i_mode, rdev);
	inode->i_size = ubifs_inode(inode)->ui_size = devlen;
	ui = ubifs_inode(inode);
	ui->data = dev;
	ui->data_len = devlen;

	err = ubifs_init_security(dir, inode, &dentry->d_name);
	if (err)
		goto out_inode;

	mutex_lock(&dir_ui->ui_mutex);
	dir->i_size += sz_change;
	dir_ui->ui_size = dir->i_size;
	dir->i_mtime = inode_set_ctime_to_ts(dir, inode_get_ctime(inode));
	err = ubifs_jnl_update(c, dir, &nm, inode, 0, 0);
	if (err)
		goto out_cancel;
	mutex_unlock(&dir_ui->ui_mutex);

	ubifs_release_budget(c, &req);
	insert_inode_hash(inode);
	d_instantiate(dentry, inode);
	fscrypt_free_filename(&nm);
	return 0;

out_cancel:
	dir->i_size -= sz_change;
	dir_ui->ui_size = dir->i_size;
	mutex_unlock(&dir_ui->ui_mutex);
out_inode:
	make_bad_inode(inode);
	iput(inode);
out_fname:
	fscrypt_free_filename(&nm);
out_budg:
	ubifs_release_budget(c, &req);
	return err;
}

static int ubifs_symlink(struct mnt_idmap *idmap, struct inode *dir,
			 struct dentry *dentry, const char *symname)
{
	struct inode *inode;
	struct ubifs_inode *ui;
	struct ubifs_inode *dir_ui = ubifs_inode(dir);
	struct ubifs_info *c = dir->i_sb->s_fs_info;
	int err, sz_change, len = strlen(symname);
	struct fscrypt_str disk_link;
	struct ubifs_budget_req req = { .new_ino = 1, .new_dent = 1,
					.dirtied_ino = 1 };
	struct fscrypt_name nm;

	dbg_gen("dent '%pd', target '%s' in dir ino %lu", dentry,
		symname, dir->i_ino);

	err = fscrypt_prepare_symlink(dir, symname, len, UBIFS_MAX_INO_DATA,
				      &disk_link);
	if (err)
		return err;

	 
	req.new_ino_d = ALIGN(disk_link.len - 1, 8);
	err = ubifs_budget_space(c, &req);
	if (err)
		return err;

	err = ubifs_prepare_create(dir, dentry, &nm);
	if (err)
		goto out_budg;

	sz_change = CALC_DENT_SIZE(fname_len(&nm));

	inode = ubifs_new_inode(c, dir, S_IFLNK | S_IRWXUGO, false);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_fname;
	}

	ui = ubifs_inode(inode);
	ui->data = kmalloc(disk_link.len, GFP_NOFS);
	if (!ui->data) {
		err = -ENOMEM;
		goto out_inode;
	}

	if (IS_ENCRYPTED(inode)) {
		disk_link.name = ui->data;  
		err = fscrypt_encrypt_symlink(inode, symname, len, &disk_link);
		if (err)
			goto out_inode;
	} else {
		memcpy(ui->data, disk_link.name, disk_link.len);
		inode->i_link = ui->data;
	}

	 
	ui->data_len = disk_link.len - 1;
	inode->i_size = ubifs_inode(inode)->ui_size = disk_link.len - 1;

	err = ubifs_init_security(dir, inode, &dentry->d_name);
	if (err)
		goto out_inode;

	mutex_lock(&dir_ui->ui_mutex);
	dir->i_size += sz_change;
	dir_ui->ui_size = dir->i_size;
	dir->i_mtime = inode_set_ctime_to_ts(dir, inode_get_ctime(inode));
	err = ubifs_jnl_update(c, dir, &nm, inode, 0, 0);
	if (err)
		goto out_cancel;
	mutex_unlock(&dir_ui->ui_mutex);

	insert_inode_hash(inode);
	d_instantiate(dentry, inode);
	err = 0;
	goto out_fname;

out_cancel:
	dir->i_size -= sz_change;
	dir_ui->ui_size = dir->i_size;
	mutex_unlock(&dir_ui->ui_mutex);
out_inode:
	make_bad_inode(inode);
	iput(inode);
out_fname:
	fscrypt_free_filename(&nm);
out_budg:
	ubifs_release_budget(c, &req);
	return err;
}

 
static void lock_4_inodes(struct inode *inode1, struct inode *inode2,
			  struct inode *inode3, struct inode *inode4)
{
	mutex_lock_nested(&ubifs_inode(inode1)->ui_mutex, WB_MUTEX_1);
	if (inode2 != inode1)
		mutex_lock_nested(&ubifs_inode(inode2)->ui_mutex, WB_MUTEX_2);
	if (inode3)
		mutex_lock_nested(&ubifs_inode(inode3)->ui_mutex, WB_MUTEX_3);
	if (inode4)
		mutex_lock_nested(&ubifs_inode(inode4)->ui_mutex, WB_MUTEX_4);
}

 
static void unlock_4_inodes(struct inode *inode1, struct inode *inode2,
			    struct inode *inode3, struct inode *inode4)
{
	if (inode4)
		mutex_unlock(&ubifs_inode(inode4)->ui_mutex);
	if (inode3)
		mutex_unlock(&ubifs_inode(inode3)->ui_mutex);
	if (inode1 != inode2)
		mutex_unlock(&ubifs_inode(inode2)->ui_mutex);
	mutex_unlock(&ubifs_inode(inode1)->ui_mutex);
}

static int do_rename(struct inode *old_dir, struct dentry *old_dentry,
		     struct inode *new_dir, struct dentry *new_dentry,
		     unsigned int flags)
{
	struct ubifs_info *c = old_dir->i_sb->s_fs_info;
	struct inode *old_inode = d_inode(old_dentry);
	struct inode *new_inode = d_inode(new_dentry);
	struct inode *whiteout = NULL;
	struct ubifs_inode *old_inode_ui = ubifs_inode(old_inode);
	struct ubifs_inode *whiteout_ui = NULL;
	int err, release, sync = 0, move = (new_dir != old_dir);
	int is_dir = S_ISDIR(old_inode->i_mode);
	int unlink = !!new_inode, new_sz, old_sz;
	struct ubifs_budget_req req = { .new_dent = 1, .mod_dent = 1,
					.dirtied_ino = 3 };
	struct ubifs_budget_req ino_req = { .dirtied_ino = 1,
			.dirtied_ino_d = ALIGN(old_inode_ui->data_len, 8) };
	struct ubifs_budget_req wht_req;
	unsigned int saved_nlink;
	struct fscrypt_name old_nm, new_nm;

	 

	dbg_gen("dent '%pd' ino %lu in dir ino %lu to dent '%pd' in dir ino %lu flags 0x%x",
		old_dentry, old_inode->i_ino, old_dir->i_ino,
		new_dentry, new_dir->i_ino, flags);

	if (unlink) {
		ubifs_assert(c, inode_is_locked(new_inode));

		 
		req.dirtied_ino_d = ALIGN(ubifs_inode(new_inode)->data_len, 8);
		err = ubifs_purge_xattrs(new_inode);
		if (err)
			return err;
	}

	if (unlink && is_dir) {
		err = ubifs_check_dir_empty(new_inode);
		if (err)
			return err;
	}

	err = fscrypt_setup_filename(old_dir, &old_dentry->d_name, 0, &old_nm);
	if (err)
		return err;

	err = fscrypt_setup_filename(new_dir, &new_dentry->d_name, 0, &new_nm);
	if (err) {
		fscrypt_free_filename(&old_nm);
		return err;
	}

	new_sz = CALC_DENT_SIZE(fname_len(&new_nm));
	old_sz = CALC_DENT_SIZE(fname_len(&old_nm));

	err = ubifs_budget_space(c, &req);
	if (err) {
		fscrypt_free_filename(&old_nm);
		fscrypt_free_filename(&new_nm);
		return err;
	}
	err = ubifs_budget_space(c, &ino_req);
	if (err) {
		fscrypt_free_filename(&old_nm);
		fscrypt_free_filename(&new_nm);
		ubifs_release_budget(c, &req);
		return err;
	}

	if (flags & RENAME_WHITEOUT) {
		union ubifs_dev_desc *dev = NULL;

		dev = kmalloc(sizeof(union ubifs_dev_desc), GFP_NOFS);
		if (!dev) {
			err = -ENOMEM;
			goto out_release;
		}

		 
		whiteout = create_whiteout(old_dir, old_dentry);
		if (IS_ERR(whiteout)) {
			err = PTR_ERR(whiteout);
			kfree(dev);
			goto out_release;
		}

		whiteout_ui = ubifs_inode(whiteout);
		whiteout_ui->data = dev;
		whiteout_ui->data_len = ubifs_encode_dev(dev, MKDEV(0, 0));
		ubifs_assert(c, !whiteout_ui->dirty);

		memset(&wht_req, 0, sizeof(struct ubifs_budget_req));
		wht_req.new_ino = 1;
		wht_req.new_ino_d = ALIGN(whiteout_ui->data_len, 8);
		 
		err = ubifs_budget_space(c, &wht_req);
		if (err) {
			 
			iput(whiteout);
			goto out_release;
		}

		 
		old_sz -= CALC_DENT_SIZE(fname_len(&old_nm));
	}

	lock_4_inodes(old_dir, new_dir, new_inode, whiteout);

	 
	simple_rename_timestamp(old_dir, old_dentry, new_dir, new_dentry);

	 
	if (is_dir) {
		if (move) {
			 
			drop_nlink(old_dir);
			 
			if (!unlink)
				inc_nlink(new_dir);
		} else {
			 
			if (unlink)
				drop_nlink(old_dir);
		}
	}

	old_dir->i_size -= old_sz;
	ubifs_inode(old_dir)->ui_size = old_dir->i_size;

	 
	if (unlink) {
		 
		saved_nlink = new_inode->i_nlink;
		if (is_dir)
			clear_nlink(new_inode);
		else
			drop_nlink(new_inode);
	} else {
		new_dir->i_size += new_sz;
		ubifs_inode(new_dir)->ui_size = new_dir->i_size;
	}

	 
	if (IS_SYNC(old_inode)) {
		sync = IS_DIRSYNC(old_dir) || IS_DIRSYNC(new_dir);
		if (unlink && IS_SYNC(new_inode))
			sync = 1;
		 
	}

	err = ubifs_jnl_rename(c, old_dir, old_inode, &old_nm, new_dir,
			       new_inode, &new_nm, whiteout, sync);
	if (err)
		goto out_cancel;

	unlock_4_inodes(old_dir, new_dir, new_inode, whiteout);
	ubifs_release_budget(c, &req);

	if (whiteout) {
		ubifs_release_budget(c, &wht_req);
		iput(whiteout);
	}

	mutex_lock(&old_inode_ui->ui_mutex);
	release = old_inode_ui->dirty;
	mark_inode_dirty_sync(old_inode);
	mutex_unlock(&old_inode_ui->ui_mutex);

	if (release)
		ubifs_release_budget(c, &ino_req);
	if (IS_SYNC(old_inode))
		 
		old_inode->i_sb->s_op->write_inode(old_inode, NULL);

	fscrypt_free_filename(&old_nm);
	fscrypt_free_filename(&new_nm);
	return 0;

out_cancel:
	if (unlink) {
		set_nlink(new_inode, saved_nlink);
	} else {
		new_dir->i_size -= new_sz;
		ubifs_inode(new_dir)->ui_size = new_dir->i_size;
	}
	old_dir->i_size += old_sz;
	ubifs_inode(old_dir)->ui_size = old_dir->i_size;
	if (is_dir) {
		if (move) {
			inc_nlink(old_dir);
			if (!unlink)
				drop_nlink(new_dir);
		} else {
			if (unlink)
				inc_nlink(old_dir);
		}
	}
	unlock_4_inodes(old_dir, new_dir, new_inode, whiteout);
	if (whiteout) {
		ubifs_release_budget(c, &wht_req);
		iput(whiteout);
	}
out_release:
	ubifs_release_budget(c, &ino_req);
	ubifs_release_budget(c, &req);
	fscrypt_free_filename(&old_nm);
	fscrypt_free_filename(&new_nm);
	return err;
}

static int ubifs_xrename(struct inode *old_dir, struct dentry *old_dentry,
			struct inode *new_dir, struct dentry *new_dentry)
{
	struct ubifs_info *c = old_dir->i_sb->s_fs_info;
	struct ubifs_budget_req req = { .new_dent = 1, .mod_dent = 1,
				.dirtied_ino = 2 };
	int sync = IS_DIRSYNC(old_dir) || IS_DIRSYNC(new_dir);
	struct inode *fst_inode = d_inode(old_dentry);
	struct inode *snd_inode = d_inode(new_dentry);
	int err;
	struct fscrypt_name fst_nm, snd_nm;

	ubifs_assert(c, fst_inode && snd_inode);

	 

	dbg_gen("dent '%pd' ino %lu in dir ino %lu exchange dent '%pd' ino %lu in dir ino %lu",
		old_dentry, fst_inode->i_ino, old_dir->i_ino,
		new_dentry, snd_inode->i_ino, new_dir->i_ino);

	err = fscrypt_setup_filename(old_dir, &old_dentry->d_name, 0, &fst_nm);
	if (err)
		return err;

	err = fscrypt_setup_filename(new_dir, &new_dentry->d_name, 0, &snd_nm);
	if (err) {
		fscrypt_free_filename(&fst_nm);
		return err;
	}

	err = ubifs_budget_space(c, &req);
	if (err)
		goto out;

	lock_4_inodes(old_dir, new_dir, NULL, NULL);

	simple_rename_timestamp(old_dir, old_dentry, new_dir, new_dentry);

	if (old_dir != new_dir) {
		if (S_ISDIR(fst_inode->i_mode) && !S_ISDIR(snd_inode->i_mode)) {
			inc_nlink(new_dir);
			drop_nlink(old_dir);
		}
		else if (!S_ISDIR(fst_inode->i_mode) && S_ISDIR(snd_inode->i_mode)) {
			drop_nlink(new_dir);
			inc_nlink(old_dir);
		}
	}

	err = ubifs_jnl_xrename(c, old_dir, fst_inode, &fst_nm, new_dir,
				snd_inode, &snd_nm, sync);

	unlock_4_inodes(old_dir, new_dir, NULL, NULL);
	ubifs_release_budget(c, &req);

out:
	fscrypt_free_filename(&fst_nm);
	fscrypt_free_filename(&snd_nm);
	return err;
}

static int ubifs_rename(struct mnt_idmap *idmap,
			struct inode *old_dir, struct dentry *old_dentry,
			struct inode *new_dir, struct dentry *new_dentry,
			unsigned int flags)
{
	int err;
	struct ubifs_info *c = old_dir->i_sb->s_fs_info;

	if (flags & ~(RENAME_NOREPLACE | RENAME_WHITEOUT | RENAME_EXCHANGE))
		return -EINVAL;

	ubifs_assert(c, inode_is_locked(old_dir));
	ubifs_assert(c, inode_is_locked(new_dir));

	err = fscrypt_prepare_rename(old_dir, old_dentry, new_dir, new_dentry,
				     flags);
	if (err)
		return err;

	if (flags & RENAME_EXCHANGE)
		return ubifs_xrename(old_dir, old_dentry, new_dir, new_dentry);

	return do_rename(old_dir, old_dentry, new_dir, new_dentry, flags);
}

int ubifs_getattr(struct mnt_idmap *idmap, const struct path *path,
		  struct kstat *stat, u32 request_mask, unsigned int flags)
{
	loff_t size;
	struct inode *inode = d_inode(path->dentry);
	struct ubifs_inode *ui = ubifs_inode(inode);

	mutex_lock(&ui->ui_mutex);

	if (ui->flags & UBIFS_APPEND_FL)
		stat->attributes |= STATX_ATTR_APPEND;
	if (ui->flags & UBIFS_COMPR_FL)
		stat->attributes |= STATX_ATTR_COMPRESSED;
	if (ui->flags & UBIFS_CRYPT_FL)
		stat->attributes |= STATX_ATTR_ENCRYPTED;
	if (ui->flags & UBIFS_IMMUTABLE_FL)
		stat->attributes |= STATX_ATTR_IMMUTABLE;

	stat->attributes_mask |= (STATX_ATTR_APPEND |
				STATX_ATTR_COMPRESSED |
				STATX_ATTR_ENCRYPTED |
				STATX_ATTR_IMMUTABLE);

	generic_fillattr(&nop_mnt_idmap, request_mask, inode, stat);
	stat->blksize = UBIFS_BLOCK_SIZE;
	stat->size = ui->ui_size;

	 
	if (S_ISREG(inode->i_mode)) {
		size = ui->xattr_size;
		size += stat->size;
		size = ALIGN(size, UBIFS_BLOCK_SIZE);
		 
		stat->blocks = size >> 9;
	} else
		stat->blocks = 0;
	mutex_unlock(&ui->ui_mutex);
	return 0;
}

const struct inode_operations ubifs_dir_inode_operations = {
	.lookup      = ubifs_lookup,
	.create      = ubifs_create,
	.link        = ubifs_link,
	.symlink     = ubifs_symlink,
	.unlink      = ubifs_unlink,
	.mkdir       = ubifs_mkdir,
	.rmdir       = ubifs_rmdir,
	.mknod       = ubifs_mknod,
	.rename      = ubifs_rename,
	.setattr     = ubifs_setattr,
	.getattr     = ubifs_getattr,
	.listxattr   = ubifs_listxattr,
	.update_time = ubifs_update_time,
	.tmpfile     = ubifs_tmpfile,
	.fileattr_get = ubifs_fileattr_get,
	.fileattr_set = ubifs_fileattr_set,
};

const struct file_operations ubifs_dir_operations = {
	.llseek         = generic_file_llseek,
	.release        = ubifs_dir_release,
	.read           = generic_read_dir,
	.iterate_shared = ubifs_readdir,
	.fsync          = ubifs_fsync,
	.unlocked_ioctl = ubifs_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = ubifs_compat_ioctl,
#endif
};
