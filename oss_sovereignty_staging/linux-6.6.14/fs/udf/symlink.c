
 

#include "udfdecl.h"
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/stat.h>
#include <linux/pagemap.h>
#include "udf_i.h"

static int udf_pc_to_char(struct super_block *sb, unsigned char *from,
			  int fromlen, unsigned char *to, int tolen)
{
	struct pathComponent *pc;
	int elen = 0;
	int comp_len;
	unsigned char *p = to;

	 
	tolen--;
	while (elen < fromlen) {
		pc = (struct pathComponent *)(from + elen);
		elen += sizeof(struct pathComponent);
		switch (pc->componentType) {
		case 1:
			 
			if (pc->lengthComponentIdent > 0) {
				elen += pc->lengthComponentIdent;
				break;
			}
			fallthrough;
		case 2:
			if (tolen == 0)
				return -ENAMETOOLONG;
			p = to;
			*p++ = '/';
			tolen--;
			break;
		case 3:
			if (tolen < 3)
				return -ENAMETOOLONG;
			memcpy(p, "../", 3);
			p += 3;
			tolen -= 3;
			break;
		case 4:
			if (tolen < 2)
				return -ENAMETOOLONG;
			memcpy(p, "./", 2);
			p += 2;
			tolen -= 2;
			 
			break;
		case 5:
			elen += pc->lengthComponentIdent;
			if (elen > fromlen)
				return -EIO;
			comp_len = udf_get_filename(sb, pc->componentIdent,
						    pc->lengthComponentIdent,
						    p, tolen);
			if (comp_len < 0)
				return comp_len;

			p += comp_len;
			tolen -= comp_len;
			if (tolen == 0)
				return -ENAMETOOLONG;
			*p++ = '/';
			tolen--;
			break;
		}
	}
	if (p > to + 1)
		p[-1] = '\0';
	else
		p[0] = '\0';
	return 0;
}

static int udf_symlink_filler(struct file *file, struct folio *folio)
{
	struct page *page = &folio->page;
	struct inode *inode = page->mapping->host;
	struct buffer_head *bh = NULL;
	unsigned char *symlink;
	int err = 0;
	unsigned char *p = page_address(page);
	struct udf_inode_info *iinfo = UDF_I(inode);

	 
	if (inode->i_size > inode->i_sb->s_blocksize) {
		err = -ENAMETOOLONG;
		goto out_unlock;
	}

	if (iinfo->i_alloc_type == ICBTAG_FLAG_AD_IN_ICB) {
		symlink = iinfo->i_data + iinfo->i_lenEAttr;
	} else {
		bh = udf_bread(inode, 0, 0, &err);
		if (!bh) {
			if (!err)
				err = -EFSCORRUPTED;
			goto out_err;
		}
		symlink = bh->b_data;
	}

	err = udf_pc_to_char(inode->i_sb, symlink, inode->i_size, p, PAGE_SIZE);
	brelse(bh);
	if (err)
		goto out_err;

	SetPageUptodate(page);
	unlock_page(page);
	return 0;

out_err:
	SetPageError(page);
out_unlock:
	unlock_page(page);
	return err;
}

static int udf_symlink_getattr(struct mnt_idmap *idmap,
			       const struct path *path, struct kstat *stat,
			       u32 request_mask, unsigned int flags)
{
	struct dentry *dentry = path->dentry;
	struct inode *inode = d_backing_inode(dentry);
	struct page *page;

	generic_fillattr(&nop_mnt_idmap, request_mask, inode, stat);
	page = read_mapping_page(inode->i_mapping, 0, NULL);
	if (IS_ERR(page))
		return PTR_ERR(page);
	 
	stat->size = strlen(page_address(page));
	put_page(page);

	return 0;
}

 
const struct address_space_operations udf_symlink_aops = {
	.read_folio		= udf_symlink_filler,
};

const struct inode_operations udf_symlink_inode_operations = {
	.get_link	= page_get_link,
	.getattr	= udf_symlink_getattr,
};
