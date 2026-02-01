 

#include "hfs_fs.h"
#include "btree.h"

 
static struct dentry *hfs_lookup(struct inode *dir, struct dentry *dentry,
				 unsigned int flags)
{
	hfs_cat_rec rec;
	struct hfs_find_data fd;
	struct inode *inode = NULL;
	int res;

	res = hfs_find_init(HFS_SB(dir->i_sb)->cat_tree, &fd);
	if (res)
		return ERR_PTR(res);
	hfs_cat_build_key(dir->i_sb, fd.search_key, dir->i_ino, &dentry->d_name);
	res = hfs_brec_read(&fd, &rec, sizeof(rec));
	if (res) {
		if (res != -ENOENT)
			inode = ERR_PTR(res);
	} else {
		inode = hfs_iget(dir->i_sb, &fd.search_key->cat, &rec);
		if (!inode)
			inode = ERR_PTR(-EACCES);
	}
	hfs_find_exit(&fd);
	return d_splice_alias(inode, dentry);
}

 
static int hfs_readdir(struct file *file, struct dir_context *ctx)
{
	struct inode *inode = file_inode(file);
	struct super_block *sb = inode->i_sb;
	int len, err;
	char strbuf[HFS_MAX_NAMELEN];
	union hfs_cat_rec entry;
	struct hfs_find_data fd;
	struct hfs_readdir_data *rd;
	u16 type;

	if (ctx->pos >= inode->i_size)
		return 0;

	err = hfs_find_init(HFS_SB(sb)->cat_tree, &fd);
	if (err)
		return err;
	hfs_cat_build_key(sb, fd.search_key, inode->i_ino, NULL);
	err = hfs_brec_find(&fd);
	if (err)
		goto out;

	if (ctx->pos == 0) {
		 
		if (!dir_emit_dot(file, ctx))
			goto out;
		ctx->pos = 1;
	}
	if (ctx->pos == 1) {
		if (fd.entrylength > sizeof(entry) || fd.entrylength < 0) {
			err = -EIO;
			goto out;
		}

		hfs_bnode_read(fd.bnode, &entry, fd.entryoffset, fd.entrylength);
		if (entry.type != HFS_CDR_THD) {
			pr_err("bad catalog folder thread\n");
			err = -EIO;
			goto out;
		}
		
		
		
		
		
		if (!dir_emit(ctx, "..", 2,
			    be32_to_cpu(entry.thread.ParID), DT_DIR))
			goto out;
		ctx->pos = 2;
	}
	if (ctx->pos >= inode->i_size)
		goto out;
	err = hfs_brec_goto(&fd, ctx->pos - 1);
	if (err)
		goto out;

	for (;;) {
		if (be32_to_cpu(fd.key->cat.ParID) != inode->i_ino) {
			pr_err("walked past end of dir\n");
			err = -EIO;
			goto out;
		}

		if (fd.entrylength > sizeof(entry) || fd.entrylength < 0) {
			err = -EIO;
			goto out;
		}

		hfs_bnode_read(fd.bnode, &entry, fd.entryoffset, fd.entrylength);
		type = entry.type;
		len = hfs_mac2asc(sb, strbuf, &fd.key->cat.CName);
		if (type == HFS_CDR_DIR) {
			if (fd.entrylength < sizeof(struct hfs_cat_dir)) {
				pr_err("small dir entry\n");
				err = -EIO;
				goto out;
			}
			if (!dir_emit(ctx, strbuf, len,
				    be32_to_cpu(entry.dir.DirID), DT_DIR))
				break;
		} else if (type == HFS_CDR_FIL) {
			if (fd.entrylength < sizeof(struct hfs_cat_file)) {
				pr_err("small file entry\n");
				err = -EIO;
				goto out;
			}
			if (!dir_emit(ctx, strbuf, len,
				    be32_to_cpu(entry.file.FlNum), DT_REG))
				break;
		} else {
			pr_err("bad catalog entry type %d\n", type);
			err = -EIO;
			goto out;
		}
		ctx->pos++;
		if (ctx->pos >= inode->i_size)
			goto out;
		err = hfs_brec_goto(&fd, 1);
		if (err)
			goto out;
	}
	rd = file->private_data;
	if (!rd) {
		rd = kmalloc(sizeof(struct hfs_readdir_data), GFP_KERNEL);
		if (!rd) {
			err = -ENOMEM;
			goto out;
		}
		file->private_data = rd;
		rd->file = file;
		spin_lock(&HFS_I(inode)->open_dir_lock);
		list_add(&rd->list, &HFS_I(inode)->open_dir_list);
		spin_unlock(&HFS_I(inode)->open_dir_lock);
	}
	 
	memcpy(&rd->key, &fd.key->cat, sizeof(struct hfs_cat_key));
out:
	hfs_find_exit(&fd);
	return err;
}

static int hfs_dir_release(struct inode *inode, struct file *file)
{
	struct hfs_readdir_data *rd = file->private_data;
	if (rd) {
		spin_lock(&HFS_I(inode)->open_dir_lock);
		list_del(&rd->list);
		spin_unlock(&HFS_I(inode)->open_dir_lock);
		kfree(rd);
	}
	return 0;
}

 
static int hfs_create(struct mnt_idmap *idmap, struct inode *dir,
		      struct dentry *dentry, umode_t mode, bool excl)
{
	struct inode *inode;
	int res;

	inode = hfs_new_inode(dir, &dentry->d_name, mode);
	if (!inode)
		return -ENOMEM;

	res = hfs_cat_create(inode->i_ino, dir, &dentry->d_name, inode);
	if (res) {
		clear_nlink(inode);
		hfs_delete_inode(inode);
		iput(inode);
		return res;
	}
	d_instantiate(dentry, inode);
	mark_inode_dirty(inode);
	return 0;
}

 
static int hfs_mkdir(struct mnt_idmap *idmap, struct inode *dir,
		     struct dentry *dentry, umode_t mode)
{
	struct inode *inode;
	int res;

	inode = hfs_new_inode(dir, &dentry->d_name, S_IFDIR | mode);
	if (!inode)
		return -ENOMEM;

	res = hfs_cat_create(inode->i_ino, dir, &dentry->d_name, inode);
	if (res) {
		clear_nlink(inode);
		hfs_delete_inode(inode);
		iput(inode);
		return res;
	}
	d_instantiate(dentry, inode);
	mark_inode_dirty(inode);
	return 0;
}

 
static int hfs_remove(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);
	int res;

	if (S_ISDIR(inode->i_mode) && inode->i_size != 2)
		return -ENOTEMPTY;
	res = hfs_cat_delete(inode->i_ino, dir, &dentry->d_name);
	if (res)
		return res;
	clear_nlink(inode);
	inode_set_ctime_current(inode);
	hfs_delete_inode(inode);
	mark_inode_dirty(inode);
	return 0;
}

 
static int hfs_rename(struct mnt_idmap *idmap, struct inode *old_dir,
		      struct dentry *old_dentry, struct inode *new_dir,
		      struct dentry *new_dentry, unsigned int flags)
{
	int res;

	if (flags & ~RENAME_NOREPLACE)
		return -EINVAL;

	 
	if (d_really_is_positive(new_dentry)) {
		res = hfs_remove(new_dir, new_dentry);
		if (res)
			return res;
	}

	res = hfs_cat_move(d_inode(old_dentry)->i_ino,
			   old_dir, &old_dentry->d_name,
			   new_dir, &new_dentry->d_name);
	if (!res)
		hfs_cat_build_key(old_dir->i_sb,
				  (btree_key *)&HFS_I(d_inode(old_dentry))->cat_key,
				  new_dir->i_ino, &new_dentry->d_name);
	return res;
}

const struct file_operations hfs_dir_operations = {
	.read		= generic_read_dir,
	.iterate_shared	= hfs_readdir,
	.llseek		= generic_file_llseek,
	.release	= hfs_dir_release,
};

const struct inode_operations hfs_dir_inode_operations = {
	.create		= hfs_create,
	.lookup		= hfs_lookup,
	.unlink		= hfs_remove,
	.mkdir		= hfs_mkdir,
	.rmdir		= hfs_remove,
	.rename		= hfs_rename,
	.setattr	= hfs_inode_setattr,
};
