 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/fs_context.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/statfs.h>
#include <linux/mtd/super.h>
#include <linux/ctype.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/uaccess.h>
#include <linux/major.h>
#include "internal.h"

static struct kmem_cache *romfs_inode_cachep;

static const umode_t romfs_modemap[8] = {
	0,			 
	S_IFDIR  | 0644,	 
	S_IFREG  | 0644,	 
	S_IFLNK  | 0777,	 
	S_IFBLK  | 0600,	 
	S_IFCHR  | 0600,	 
	S_IFSOCK | 0644,	 
	S_IFIFO  | 0644		 
};

static const unsigned char romfs_dtype_table[] = {
	DT_UNKNOWN, DT_DIR, DT_REG, DT_LNK, DT_BLK, DT_CHR, DT_SOCK, DT_FIFO
};

static struct inode *romfs_iget(struct super_block *sb, unsigned long pos);

 
static int romfs_read_folio(struct file *file, struct folio *folio)
{
	struct page *page = &folio->page;
	struct inode *inode = page->mapping->host;
	loff_t offset, size;
	unsigned long fillsize, pos;
	void *buf;
	int ret;

	buf = kmap(page);
	if (!buf)
		return -ENOMEM;

	 
	offset = page_offset(page);
	size = i_size_read(inode);
	fillsize = 0;
	ret = 0;
	if (offset < size) {
		size -= offset;
		fillsize = size > PAGE_SIZE ? PAGE_SIZE : size;

		pos = ROMFS_I(inode)->i_dataoffset + offset;

		ret = romfs_dev_read(inode->i_sb, pos, buf, fillsize);
		if (ret < 0) {
			SetPageError(page);
			fillsize = 0;
			ret = -EIO;
		}
	}

	if (fillsize < PAGE_SIZE)
		memset(buf + fillsize, 0, PAGE_SIZE - fillsize);
	if (ret == 0)
		SetPageUptodate(page);

	flush_dcache_page(page);
	kunmap(page);
	unlock_page(page);
	return ret;
}

static const struct address_space_operations romfs_aops = {
	.read_folio	= romfs_read_folio
};

 
static int romfs_readdir(struct file *file, struct dir_context *ctx)
{
	struct inode *i = file_inode(file);
	struct romfs_inode ri;
	unsigned long offset, maxoff;
	int j, ino, nextfh;
	char fsname[ROMFS_MAXFN];	 
	int ret;

	maxoff = romfs_maxsize(i->i_sb);

	offset = ctx->pos;
	if (!offset) {
		offset = i->i_ino & ROMFH_MASK;
		ret = romfs_dev_read(i->i_sb, offset, &ri, ROMFH_SIZE);
		if (ret < 0)
			goto out;
		offset = be32_to_cpu(ri.spec) & ROMFH_MASK;
	}

	 
	for (;;) {
		if (!offset || offset >= maxoff) {
			offset = maxoff;
			ctx->pos = offset;
			goto out;
		}
		ctx->pos = offset;

		 
		ret = romfs_dev_read(i->i_sb, offset, &ri, ROMFH_SIZE);
		if (ret < 0)
			goto out;

		j = romfs_dev_strnlen(i->i_sb, offset + ROMFH_SIZE,
				      sizeof(fsname) - 1);
		if (j < 0)
			goto out;

		ret = romfs_dev_read(i->i_sb, offset + ROMFH_SIZE, fsname, j);
		if (ret < 0)
			goto out;
		fsname[j] = '\0';

		ino = offset;
		nextfh = be32_to_cpu(ri.next);
		if ((nextfh & ROMFH_TYPE) == ROMFH_HRD)
			ino = be32_to_cpu(ri.spec);
		if (!dir_emit(ctx, fsname, j, ino,
			    romfs_dtype_table[nextfh & ROMFH_TYPE]))
			goto out;

		offset = nextfh & ROMFH_MASK;
	}
out:
	return 0;
}

 
static struct dentry *romfs_lookup(struct inode *dir, struct dentry *dentry,
				   unsigned int flags)
{
	unsigned long offset, maxoff;
	struct inode *inode = NULL;
	struct romfs_inode ri;
	const char *name;		 
	int len, ret;

	offset = dir->i_ino & ROMFH_MASK;
	ret = romfs_dev_read(dir->i_sb, offset, &ri, ROMFH_SIZE);
	if (ret < 0)
		goto error;

	 
	maxoff = romfs_maxsize(dir->i_sb);
	offset = be32_to_cpu(ri.spec) & ROMFH_MASK;

	name = dentry->d_name.name;
	len = dentry->d_name.len;

	for (;;) {
		if (!offset || offset >= maxoff)
			break;

		ret = romfs_dev_read(dir->i_sb, offset, &ri, sizeof(ri));
		if (ret < 0)
			goto error;

		 
		ret = romfs_dev_strcmp(dir->i_sb, offset + ROMFH_SIZE, name,
				       len);
		if (ret < 0)
			goto error;
		if (ret == 1) {
			 
			if ((be32_to_cpu(ri.next) & ROMFH_TYPE) == ROMFH_HRD)
				offset = be32_to_cpu(ri.spec) & ROMFH_MASK;
			inode = romfs_iget(dir->i_sb, offset);
			break;
		}

		 
		offset = be32_to_cpu(ri.next) & ROMFH_MASK;
	}

	return d_splice_alias(inode, dentry);
error:
	return ERR_PTR(ret);
}

static const struct file_operations romfs_dir_operations = {
	.read		= generic_read_dir,
	.iterate_shared	= romfs_readdir,
	.llseek		= generic_file_llseek,
};

static const struct inode_operations romfs_dir_inode_operations = {
	.lookup		= romfs_lookup,
};

 
static struct inode *romfs_iget(struct super_block *sb, unsigned long pos)
{
	struct romfs_inode_info *inode;
	struct romfs_inode ri;
	struct inode *i;
	unsigned long nlen;
	unsigned nextfh;
	int ret;
	umode_t mode;

	 
	for (;;) {
		ret = romfs_dev_read(sb, pos, &ri, sizeof(ri));
		if (ret < 0)
			goto error;

		 

		nextfh = be32_to_cpu(ri.next);
		if ((nextfh & ROMFH_TYPE) != ROMFH_HRD)
			break;

		pos = be32_to_cpu(ri.spec) & ROMFH_MASK;
	}

	 
	nlen = romfs_dev_strnlen(sb, pos + ROMFH_SIZE, ROMFS_MAXFN);
	if (IS_ERR_VALUE(nlen))
		goto eio;

	 
	i = iget_locked(sb, pos);
	if (!i)
		return ERR_PTR(-ENOMEM);

	if (!(i->i_state & I_NEW))
		return i;

	 
	inode = ROMFS_I(i);
	inode->i_metasize = (ROMFH_SIZE + nlen + 1 + ROMFH_PAD) & ROMFH_MASK;
	inode->i_dataoffset = pos + inode->i_metasize;

	set_nlink(i, 1);		 
	i->i_size = be32_to_cpu(ri.size);
	i->i_mtime = i->i_atime = inode_set_ctime(i, 0, 0);

	 
	mode = romfs_modemap[nextfh & ROMFH_TYPE];

	switch (nextfh & ROMFH_TYPE) {
	case ROMFH_DIR:
		i->i_size = ROMFS_I(i)->i_metasize;
		i->i_op = &romfs_dir_inode_operations;
		i->i_fop = &romfs_dir_operations;
		if (nextfh & ROMFH_EXEC)
			mode |= S_IXUGO;
		break;
	case ROMFH_REG:
		i->i_fop = &romfs_ro_fops;
		i->i_data.a_ops = &romfs_aops;
		if (nextfh & ROMFH_EXEC)
			mode |= S_IXUGO;
		break;
	case ROMFH_SYM:
		i->i_op = &page_symlink_inode_operations;
		inode_nohighmem(i);
		i->i_data.a_ops = &romfs_aops;
		mode |= S_IRWXUGO;
		break;
	default:
		 
		nextfh = be32_to_cpu(ri.spec);
		init_special_inode(i, mode, MKDEV(nextfh >> 16,
						  nextfh & 0xffff));
		break;
	}

	i->i_mode = mode;
	i->i_blocks = (i->i_size + 511) >> 9;

	unlock_new_inode(i);
	return i;

eio:
	ret = -EIO;
error:
	pr_err("read error for inode 0x%lx\n", pos);
	return ERR_PTR(ret);
}

 
static struct inode *romfs_alloc_inode(struct super_block *sb)
{
	struct romfs_inode_info *inode;

	inode = alloc_inode_sb(sb, romfs_inode_cachep, GFP_KERNEL);
	return inode ? &inode->vfs_inode : NULL;
}

 
static void romfs_free_inode(struct inode *inode)
{
	kmem_cache_free(romfs_inode_cachep, ROMFS_I(inode));
}

 
static int romfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	struct super_block *sb = dentry->d_sb;
	u64 id = 0;

	 
	if (sb->s_bdev)
		id = huge_encode_dev(sb->s_bdev->bd_dev);
	else if (sb->s_dev)
		id = huge_encode_dev(sb->s_dev);

	buf->f_type = ROMFS_MAGIC;
	buf->f_namelen = ROMFS_MAXFN;
	buf->f_bsize = ROMBSIZE;
	buf->f_bfree = buf->f_bavail = buf->f_ffree;
	buf->f_blocks =
		(romfs_maxsize(dentry->d_sb) + ROMBSIZE - 1) >> ROMBSBITS;
	buf->f_fsid = u64_to_fsid(id);
	return 0;
}

 
static int romfs_reconfigure(struct fs_context *fc)
{
	sync_filesystem(fc->root->d_sb);
	fc->sb_flags |= SB_RDONLY;
	return 0;
}

static const struct super_operations romfs_super_ops = {
	.alloc_inode	= romfs_alloc_inode,
	.free_inode	= romfs_free_inode,
	.statfs		= romfs_statfs,
};

 
static __u32 romfs_checksum(const void *data, int size)
{
	const __be32 *ptr = data;
	__u32 sum;

	sum = 0;
	size >>= 2;
	while (size > 0) {
		sum += be32_to_cpu(*ptr++);
		size--;
	}
	return sum;
}

 
static int romfs_fill_super(struct super_block *sb, struct fs_context *fc)
{
	struct romfs_super_block *rsb;
	struct inode *root;
	unsigned long pos, img_size;
	const char *storage;
	size_t len;
	int ret;

#ifdef CONFIG_BLOCK
	if (!sb->s_mtd) {
		sb_set_blocksize(sb, ROMBSIZE);
	} else {
		sb->s_blocksize = ROMBSIZE;
		sb->s_blocksize_bits = blksize_bits(ROMBSIZE);
	}
#endif

	sb->s_maxbytes = 0xFFFFFFFF;
	sb->s_magic = ROMFS_MAGIC;
	sb->s_flags |= SB_RDONLY | SB_NOATIME;
	sb->s_time_min = 0;
	sb->s_time_max = 0;
	sb->s_op = &romfs_super_ops;

#ifdef CONFIG_ROMFS_ON_MTD
	 
	if (sb->s_mtd)
		sb->s_dev = MKDEV(MTD_BLOCK_MAJOR, sb->s_mtd->index);
#endif
	 
	rsb = kmalloc(512, GFP_KERNEL);
	if (!rsb)
		return -ENOMEM;

	sb->s_fs_info = (void *) 512;
	ret = romfs_dev_read(sb, 0, rsb, 512);
	if (ret < 0)
		goto error_rsb;

	img_size = be32_to_cpu(rsb->size);

	if (sb->s_mtd && img_size > sb->s_mtd->size)
		goto error_rsb_inval;

	sb->s_fs_info = (void *) img_size;

	if (rsb->word0 != ROMSB_WORD0 || rsb->word1 != ROMSB_WORD1 ||
	    img_size < ROMFH_SIZE) {
		if (!(fc->sb_flags & SB_SILENT))
			errorf(fc, "VFS: Can't find a romfs filesystem on dev %s.\n",
			       sb->s_id);
		goto error_rsb_inval;
	}

	if (romfs_checksum(rsb, min_t(size_t, img_size, 512))) {
		pr_err("bad initial checksum on dev %s.\n", sb->s_id);
		goto error_rsb_inval;
	}

	storage = sb->s_mtd ? "MTD" : "the block layer";

	len = strnlen(rsb->name, ROMFS_MAXFN);
	if (!(fc->sb_flags & SB_SILENT))
		pr_notice("Mounting image '%*.*s' through %s\n",
			  (unsigned) len, (unsigned) len, rsb->name, storage);

	kfree(rsb);
	rsb = NULL;

	 
	pos = (ROMFH_SIZE + len + 1 + ROMFH_PAD) & ROMFH_MASK;

	root = romfs_iget(sb, pos);
	if (IS_ERR(root))
		return PTR_ERR(root);

	sb->s_root = d_make_root(root);
	if (!sb->s_root)
		return -ENOMEM;

	return 0;

error_rsb_inval:
	ret = -EINVAL;
error_rsb:
	kfree(rsb);
	return ret;
}

 
static int romfs_get_tree(struct fs_context *fc)
{
	int ret = -EINVAL;

#ifdef CONFIG_ROMFS_ON_MTD
	ret = get_tree_mtd(fc, romfs_fill_super);
#endif
#ifdef CONFIG_ROMFS_ON_BLOCK
	if (ret == -EINVAL)
		ret = get_tree_bdev(fc, romfs_fill_super);
#endif
	return ret;
}

static const struct fs_context_operations romfs_context_ops = {
	.get_tree	= romfs_get_tree,
	.reconfigure	= romfs_reconfigure,
};

 
static int romfs_init_fs_context(struct fs_context *fc)
{
	fc->ops = &romfs_context_ops;
	return 0;
}

 
static void romfs_kill_sb(struct super_block *sb)
{
	generic_shutdown_super(sb);

#ifdef CONFIG_ROMFS_ON_MTD
	if (sb->s_mtd) {
		put_mtd_device(sb->s_mtd);
		sb->s_mtd = NULL;
	}
#endif
#ifdef CONFIG_ROMFS_ON_BLOCK
	if (sb->s_bdev) {
		sync_blockdev(sb->s_bdev);
		blkdev_put(sb->s_bdev, sb);
	}
#endif
}

static struct file_system_type romfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "romfs",
	.init_fs_context = romfs_init_fs_context,
	.kill_sb	= romfs_kill_sb,
	.fs_flags	= FS_REQUIRES_DEV,
};
MODULE_ALIAS_FS("romfs");

 
static void romfs_i_init_once(void *_inode)
{
	struct romfs_inode_info *inode = _inode;

	inode_init_once(&inode->vfs_inode);
}

 
static int __init init_romfs_fs(void)
{
	int ret;

	pr_info("ROMFS MTD (C) 2007 Red Hat, Inc.\n");

	romfs_inode_cachep =
		kmem_cache_create("romfs_i",
				  sizeof(struct romfs_inode_info), 0,
				  SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD |
				  SLAB_ACCOUNT, romfs_i_init_once);

	if (!romfs_inode_cachep) {
		pr_err("Failed to initialise inode cache\n");
		return -ENOMEM;
	}
	ret = register_filesystem(&romfs_fs_type);
	if (ret) {
		pr_err("Failed to register filesystem\n");
		goto error_register;
	}
	return 0;

error_register:
	kmem_cache_destroy(romfs_inode_cachep);
	return ret;
}

 
static void __exit exit_romfs_fs(void)
{
	unregister_filesystem(&romfs_fs_type);
	 
	rcu_barrier();
	kmem_cache_destroy(romfs_inode_cachep);
}

module_init(init_romfs_fs);
module_exit(exit_romfs_fs);

MODULE_DESCRIPTION("Direct-MTD Capable RomFS");
MODULE_AUTHOR("Red Hat, Inc.");
MODULE_LICENSE("GPL");  
