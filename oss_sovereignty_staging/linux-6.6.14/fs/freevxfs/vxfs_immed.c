
 

 
#include <linux/fs.h>
#include <linux/pagemap.h>

#include "vxfs.h"
#include "vxfs_extern.h"
#include "vxfs_inode.h"

 
static int vxfs_immed_read_folio(struct file *fp, struct folio *folio)
{
	struct vxfs_inode_info *vip = VXFS_INO(folio->mapping->host);
	void *src = vip->vii_immed.vi_immed + folio_pos(folio);
	unsigned long i;

	for (i = 0; i < folio_nr_pages(folio); i++) {
		memcpy_to_page(folio_page(folio, i), 0, src, PAGE_SIZE);
		src += PAGE_SIZE;
	}

	folio_mark_uptodate(folio);
	folio_unlock(folio);

	return 0;
}

 
const struct address_space_operations vxfs_immed_aops = {
	.read_folio =	vxfs_immed_read_folio,
};
