
 

 

#include <linux/module.h>
#include <linux/init.h>
#include <linux/bio.h>

#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/zlib.h>

#include "isofs.h"
#include "zisofs.h"

 
static char zisofs_sink_page[PAGE_SIZE];

 
static void *zisofs_zlib_workspace;
static DEFINE_MUTEX(zisofs_zlib_lock);

 
static loff_t zisofs_uncompress_block(struct inode *inode, loff_t block_start,
				      loff_t block_end, int pcount,
				      struct page **pages, unsigned poffset,
				      int *errp)
{
	unsigned int zisofs_block_shift = ISOFS_I(inode)->i_format_parm[1];
	unsigned int bufsize = ISOFS_BUFFER_SIZE(inode);
	unsigned int bufshift = ISOFS_BUFFER_BITS(inode);
	unsigned int bufmask = bufsize - 1;
	int i, block_size = block_end - block_start;
	z_stream stream = { .total_out = 0,
			    .avail_in = 0,
			    .avail_out = 0, };
	int zerr;
	int needblocks = (block_size + (block_start & bufmask) + bufmask)
				>> bufshift;
	int haveblocks;
	blkcnt_t blocknum;
	struct buffer_head **bhs;
	int curbh, curpage;

	if (block_size > deflateBound(1UL << zisofs_block_shift)) {
		*errp = -EIO;
		return 0;
	}
	 
	if (block_size == 0) {
		for ( i = 0 ; i < pcount ; i++ ) {
			if (!pages[i])
				continue;
			memzero_page(pages[i], 0, PAGE_SIZE);
			SetPageUptodate(pages[i]);
		}
		return ((loff_t)pcount) << PAGE_SHIFT;
	}

	 
	blocknum = block_start >> bufshift;
	bhs = kcalloc(needblocks + 1, sizeof(*bhs), GFP_KERNEL);
	if (!bhs) {
		*errp = -ENOMEM;
		return 0;
	}
	haveblocks = isofs_get_blocks(inode, blocknum, bhs, needblocks);
	bh_read_batch(haveblocks, bhs);

	curbh = 0;
	curpage = 0;
	 

	if (!bhs[0])
		goto b_eio;

	wait_on_buffer(bhs[0]);
	if (!buffer_uptodate(bhs[0])) {
		*errp = -EIO;
		goto b_eio;
	}

	stream.workspace = zisofs_zlib_workspace;
	mutex_lock(&zisofs_zlib_lock);
		
	zerr = zlib_inflateInit(&stream);
	if (zerr != Z_OK) {
		if (zerr == Z_MEM_ERROR)
			*errp = -ENOMEM;
		else
			*errp = -EIO;
		printk(KERN_DEBUG "zisofs: zisofs_inflateInit returned %d\n",
			       zerr);
		goto z_eio;
	}

	while (curpage < pcount && curbh < haveblocks &&
	       zerr != Z_STREAM_END) {
		if (!stream.avail_out) {
			if (pages[curpage]) {
				stream.next_out = kmap_local_page(pages[curpage])
						+ poffset;
				stream.avail_out = PAGE_SIZE - poffset;
				poffset = 0;
			} else {
				stream.next_out = (void *)&zisofs_sink_page;
				stream.avail_out = PAGE_SIZE;
			}
		}
		if (!stream.avail_in) {
			wait_on_buffer(bhs[curbh]);
			if (!buffer_uptodate(bhs[curbh])) {
				*errp = -EIO;
				break;
			}
			stream.next_in  = bhs[curbh]->b_data +
						(block_start & bufmask);
			stream.avail_in = min_t(unsigned, bufsize -
						(block_start & bufmask),
						block_size);
			block_size -= stream.avail_in;
			block_start = 0;
		}

		while (stream.avail_out && stream.avail_in) {
			zerr = zlib_inflate(&stream, Z_SYNC_FLUSH);
			if (zerr == Z_BUF_ERROR && stream.avail_in == 0)
				break;
			if (zerr == Z_STREAM_END)
				break;
			if (zerr != Z_OK) {
				 
				if (zerr == Z_MEM_ERROR)
					*errp = -ENOMEM;
				else {
					printk(KERN_DEBUG
					       "zisofs: zisofs_inflate returned"
					       " %d, inode = %lu,"
					       " page idx = %d, bh idx = %d,"
					       " avail_in = %ld,"
					       " avail_out = %ld\n",
					       zerr, inode->i_ino, curpage,
					       curbh, stream.avail_in,
					       stream.avail_out);
					*errp = -EIO;
				}
				goto inflate_out;
			}
		}

		if (!stream.avail_out) {
			 
			if (pages[curpage]) {
				flush_dcache_page(pages[curpage]);
				SetPageUptodate(pages[curpage]);
			}
			if (stream.next_out != (unsigned char *)zisofs_sink_page) {
				kunmap_local(stream.next_out);
				stream.next_out = NULL;
			}
			curpage++;
		}
		if (!stream.avail_in)
			curbh++;
	}
inflate_out:
	zlib_inflateEnd(&stream);
	if (stream.next_out && stream.next_out != (unsigned char *)zisofs_sink_page)
		kunmap_local(stream.next_out);

z_eio:
	mutex_unlock(&zisofs_zlib_lock);

b_eio:
	for (i = 0; i < haveblocks; i++)
		brelse(bhs[i]);
	kfree(bhs);
	return stream.total_out;
}

 
static int zisofs_fill_pages(struct inode *inode, int full_page, int pcount,
			     struct page **pages)
{
	loff_t start_off, end_off;
	loff_t block_start, block_end;
	unsigned int header_size = ISOFS_I(inode)->i_format_parm[0];
	unsigned int zisofs_block_shift = ISOFS_I(inode)->i_format_parm[1];
	unsigned int blockptr;
	loff_t poffset = 0;
	blkcnt_t cstart_block, cend_block;
	struct buffer_head *bh;
	unsigned int blkbits = ISOFS_BUFFER_BITS(inode);
	unsigned int blksize = 1 << blkbits;
	int err;
	loff_t ret;

	BUG_ON(!pages[full_page]);

	 
	start_off = page_offset(pages[full_page]);
	end_off = min_t(loff_t, start_off + PAGE_SIZE, inode->i_size);

	cstart_block = start_off >> zisofs_block_shift;
	cend_block = (end_off + (1 << zisofs_block_shift) - 1)
			>> zisofs_block_shift;

	WARN_ON(start_off - (full_page << PAGE_SHIFT) !=
		((cstart_block << zisofs_block_shift) & PAGE_MASK));

	 
	 
	 
	blockptr = (header_size + cstart_block) << 2;
	bh = isofs_bread(inode, blockptr >> blkbits);
	if (!bh)
		return -EIO;
	block_start = le32_to_cpu(*(__le32 *)
				(bh->b_data + (blockptr & (blksize - 1))));

	while (cstart_block < cend_block && pcount > 0) {
		 
		blockptr += 4;
		 
		if (!(blockptr & (blksize - 1))) {
			brelse(bh);

			bh = isofs_bread(inode, blockptr >> blkbits);
			if (!bh)
				return -EIO;
		}
		block_end = le32_to_cpu(*(__le32 *)
				(bh->b_data + (blockptr & (blksize - 1))));
		if (block_start > block_end) {
			brelse(bh);
			return -EIO;
		}
		err = 0;
		ret = zisofs_uncompress_block(inode, block_start, block_end,
					      pcount, pages, poffset, &err);
		poffset += ret;
		pages += poffset >> PAGE_SHIFT;
		pcount -= poffset >> PAGE_SHIFT;
		full_page -= poffset >> PAGE_SHIFT;
		poffset &= ~PAGE_MASK;

		if (err) {
			brelse(bh);
			 
			if (full_page < 0)
				return 0;
			return err;
		}

		block_start = block_end;
		cstart_block++;
	}

	if (poffset && *pages) {
		memzero_page(*pages, poffset, PAGE_SIZE - poffset);
		SetPageUptodate(*pages);
	}
	return 0;
}

 
static int zisofs_read_folio(struct file *file, struct folio *folio)
{
	struct page *page = &folio->page;
	struct inode *inode = file_inode(file);
	struct address_space *mapping = inode->i_mapping;
	int err;
	int i, pcount, full_page;
	unsigned int zisofs_block_shift = ISOFS_I(inode)->i_format_parm[1];
	unsigned int zisofs_pages_per_cblock =
		PAGE_SHIFT <= zisofs_block_shift ?
		(1 << (zisofs_block_shift - PAGE_SHIFT)) : 0;
	struct page **pages;
	pgoff_t index = page->index, end_index;

	end_index = (inode->i_size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	 
	if (index >= end_index) {
		SetPageUptodate(page);
		unlock_page(page);
		return 0;
	}

	if (PAGE_SHIFT <= zisofs_block_shift) {
		 
		full_page = index & (zisofs_pages_per_cblock - 1);
		pcount = min_t(int, zisofs_pages_per_cblock,
			end_index - (index & ~(zisofs_pages_per_cblock - 1)));
		index -= full_page;
	} else {
		full_page = 0;
		pcount = 1;
	}
	pages = kcalloc(max_t(unsigned int, zisofs_pages_per_cblock, 1),
					sizeof(*pages), GFP_KERNEL);
	if (!pages) {
		unlock_page(page);
		return -ENOMEM;
	}
	pages[full_page] = page;

	for (i = 0; i < pcount; i++, index++) {
		if (i != full_page)
			pages[i] = grab_cache_page_nowait(mapping, index);
		if (pages[i])
			ClearPageError(pages[i]);
	}

	err = zisofs_fill_pages(inode, full_page, pcount, pages);

	 
	for (i = 0; i < pcount; i++) {
		if (pages[i]) {
			flush_dcache_page(pages[i]);
			if (i == full_page && err)
				SetPageError(pages[i]);
			unlock_page(pages[i]);
			if (i != full_page)
				put_page(pages[i]);
		}
	}			

	 
	kfree(pages);
	return err;
}

const struct address_space_operations zisofs_aops = {
	.read_folio = zisofs_read_folio,
	 
};

int __init zisofs_init(void)
{
	zisofs_zlib_workspace = vmalloc(zlib_inflate_workspacesize());
	if ( !zisofs_zlib_workspace )
		return -ENOMEM;

	return 0;
}

void zisofs_cleanup(void)
{
	vfree(zisofs_zlib_workspace);
}
