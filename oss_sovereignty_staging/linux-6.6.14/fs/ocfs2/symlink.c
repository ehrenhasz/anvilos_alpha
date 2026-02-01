 

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/namei.h>

#include <cluster/masklog.h>

#include "ocfs2.h"

#include "alloc.h"
#include "file.h"
#include "inode.h"
#include "journal.h"
#include "symlink.h"
#include "xattr.h"

#include "buffer_head_io.h"


static int ocfs2_fast_symlink_read_folio(struct file *f, struct folio *folio)
{
	struct page *page = &folio->page;
	struct inode *inode = page->mapping->host;
	struct buffer_head *bh = NULL;
	int status = ocfs2_read_inode_block(inode, &bh);
	struct ocfs2_dinode *fe;
	const char *link;
	void *kaddr;
	size_t len;

	if (status < 0) {
		mlog_errno(status);
		return status;
	}

	fe = (struct ocfs2_dinode *) bh->b_data;
	link = (char *) fe->id2.i_symlink;
	 
	len = strnlen(link, ocfs2_fast_symlink_chars(inode->i_sb));
	kaddr = kmap_atomic(page);
	memcpy(kaddr, link, len + 1);
	kunmap_atomic(kaddr);
	SetPageUptodate(page);
	unlock_page(page);
	brelse(bh);
	return 0;
}

const struct address_space_operations ocfs2_fast_symlink_aops = {
	.read_folio		= ocfs2_fast_symlink_read_folio,
};

const struct inode_operations ocfs2_symlink_inode_operations = {
	.get_link	= page_get_link,
	.getattr	= ocfs2_getattr,
	.setattr	= ocfs2_setattr,
	.listxattr	= ocfs2_listxattr,
	.fiemap		= ocfs2_fiemap,
};
