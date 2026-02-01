
 

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/mm.h>
#include <linux/kdev_t.h>
#include <linux/gfp.h>
#include <linux/bio.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/blkdev.h>
#include <linux/highmem.h>
#include <linux/prefetch.h>
#include <linux/mpage.h>
#include <linux/mm_inline.h>
#include <linux/writeback.h>
#include <linux/backing-dev.h>
#include <linux/pagevec.h>
#include "internal.h"

 
static void mpage_read_end_io(struct bio *bio)
{
	struct folio_iter fi;
	int err = blk_status_to_errno(bio->bi_status);

	bio_for_each_folio_all(fi, bio) {
		if (err)
			folio_set_error(fi.folio);
		else
			folio_mark_uptodate(fi.folio);
		folio_unlock(fi.folio);
	}

	bio_put(bio);
}

static void mpage_write_end_io(struct bio *bio)
{
	struct folio_iter fi;
	int err = blk_status_to_errno(bio->bi_status);

	bio_for_each_folio_all(fi, bio) {
		if (err) {
			folio_set_error(fi.folio);
			mapping_set_error(fi.folio->mapping, err);
		}
		folio_end_writeback(fi.folio);
	}

	bio_put(bio);
}

static struct bio *mpage_bio_submit_read(struct bio *bio)
{
	bio->bi_end_io = mpage_read_end_io;
	guard_bio_eod(bio);
	submit_bio(bio);
	return NULL;
}

static struct bio *mpage_bio_submit_write(struct bio *bio)
{
	bio->bi_end_io = mpage_write_end_io;
	guard_bio_eod(bio);
	submit_bio(bio);
	return NULL;
}

 
static void map_buffer_to_folio(struct folio *folio, struct buffer_head *bh,
		int page_block)
{
	struct inode *inode = folio->mapping->host;
	struct buffer_head *page_bh, *head;
	int block = 0;

	head = folio_buffers(folio);
	if (!head) {
		 
		if (inode->i_blkbits == PAGE_SHIFT &&
		    buffer_uptodate(bh)) {
			folio_mark_uptodate(folio);
			return;
		}
		create_empty_buffers(&folio->page, i_blocksize(inode), 0);
		head = folio_buffers(folio);
	}

	page_bh = head;
	do {
		if (block == page_block) {
			page_bh->b_state = bh->b_state;
			page_bh->b_bdev = bh->b_bdev;
			page_bh->b_blocknr = bh->b_blocknr;
			break;
		}
		page_bh = page_bh->b_this_page;
		block++;
	} while (page_bh != head);
}

struct mpage_readpage_args {
	struct bio *bio;
	struct folio *folio;
	unsigned int nr_pages;
	bool is_readahead;
	sector_t last_block_in_bio;
	struct buffer_head map_bh;
	unsigned long first_logical_block;
	get_block_t *get_block;
};

 
static struct bio *do_mpage_readpage(struct mpage_readpage_args *args)
{
	struct folio *folio = args->folio;
	struct inode *inode = folio->mapping->host;
	const unsigned blkbits = inode->i_blkbits;
	const unsigned blocks_per_page = PAGE_SIZE >> blkbits;
	const unsigned blocksize = 1 << blkbits;
	struct buffer_head *map_bh = &args->map_bh;
	sector_t block_in_file;
	sector_t last_block;
	sector_t last_block_in_file;
	sector_t blocks[MAX_BUF_PER_PAGE];
	unsigned page_block;
	unsigned first_hole = blocks_per_page;
	struct block_device *bdev = NULL;
	int length;
	int fully_mapped = 1;
	blk_opf_t opf = REQ_OP_READ;
	unsigned nblocks;
	unsigned relative_block;
	gfp_t gfp = mapping_gfp_constraint(folio->mapping, GFP_KERNEL);

	 
	VM_BUG_ON_FOLIO(folio_test_large(folio), folio);

	if (args->is_readahead) {
		opf |= REQ_RAHEAD;
		gfp |= __GFP_NORETRY | __GFP_NOWARN;
	}

	if (folio_buffers(folio))
		goto confused;

	block_in_file = (sector_t)folio->index << (PAGE_SHIFT - blkbits);
	last_block = block_in_file + args->nr_pages * blocks_per_page;
	last_block_in_file = (i_size_read(inode) + blocksize - 1) >> blkbits;
	if (last_block > last_block_in_file)
		last_block = last_block_in_file;
	page_block = 0;

	 
	nblocks = map_bh->b_size >> blkbits;
	if (buffer_mapped(map_bh) &&
			block_in_file > args->first_logical_block &&
			block_in_file < (args->first_logical_block + nblocks)) {
		unsigned map_offset = block_in_file - args->first_logical_block;
		unsigned last = nblocks - map_offset;

		for (relative_block = 0; ; relative_block++) {
			if (relative_block == last) {
				clear_buffer_mapped(map_bh);
				break;
			}
			if (page_block == blocks_per_page)
				break;
			blocks[page_block] = map_bh->b_blocknr + map_offset +
						relative_block;
			page_block++;
			block_in_file++;
		}
		bdev = map_bh->b_bdev;
	}

	 
	map_bh->b_folio = folio;
	while (page_block < blocks_per_page) {
		map_bh->b_state = 0;
		map_bh->b_size = 0;

		if (block_in_file < last_block) {
			map_bh->b_size = (last_block-block_in_file) << blkbits;
			if (args->get_block(inode, block_in_file, map_bh, 0))
				goto confused;
			args->first_logical_block = block_in_file;
		}

		if (!buffer_mapped(map_bh)) {
			fully_mapped = 0;
			if (first_hole == blocks_per_page)
				first_hole = page_block;
			page_block++;
			block_in_file++;
			continue;
		}

		 
		if (buffer_uptodate(map_bh)) {
			map_buffer_to_folio(folio, map_bh, page_block);
			goto confused;
		}
	
		if (first_hole != blocks_per_page)
			goto confused;		 

		 
		if (page_block && blocks[page_block-1] != map_bh->b_blocknr-1)
			goto confused;
		nblocks = map_bh->b_size >> blkbits;
		for (relative_block = 0; ; relative_block++) {
			if (relative_block == nblocks) {
				clear_buffer_mapped(map_bh);
				break;
			} else if (page_block == blocks_per_page)
				break;
			blocks[page_block] = map_bh->b_blocknr+relative_block;
			page_block++;
			block_in_file++;
		}
		bdev = map_bh->b_bdev;
	}

	if (first_hole != blocks_per_page) {
		folio_zero_segment(folio, first_hole << blkbits, PAGE_SIZE);
		if (first_hole == 0) {
			folio_mark_uptodate(folio);
			folio_unlock(folio);
			goto out;
		}
	} else if (fully_mapped) {
		folio_set_mappedtodisk(folio);
	}

	 
	if (args->bio && (args->last_block_in_bio != blocks[0] - 1))
		args->bio = mpage_bio_submit_read(args->bio);

alloc_new:
	if (args->bio == NULL) {
		args->bio = bio_alloc(bdev, bio_max_segs(args->nr_pages), opf,
				      gfp);
		if (args->bio == NULL)
			goto confused;
		args->bio->bi_iter.bi_sector = blocks[0] << (blkbits - 9);
	}

	length = first_hole << blkbits;
	if (!bio_add_folio(args->bio, folio, length, 0)) {
		args->bio = mpage_bio_submit_read(args->bio);
		goto alloc_new;
	}

	relative_block = block_in_file - args->first_logical_block;
	nblocks = map_bh->b_size >> blkbits;
	if ((buffer_boundary(map_bh) && relative_block == nblocks) ||
	    (first_hole != blocks_per_page))
		args->bio = mpage_bio_submit_read(args->bio);
	else
		args->last_block_in_bio = blocks[blocks_per_page - 1];
out:
	return args->bio;

confused:
	if (args->bio)
		args->bio = mpage_bio_submit_read(args->bio);
	if (!folio_test_uptodate(folio))
		block_read_full_folio(folio, args->get_block);
	else
		folio_unlock(folio);
	goto out;
}

 
void mpage_readahead(struct readahead_control *rac, get_block_t get_block)
{
	struct folio *folio;
	struct mpage_readpage_args args = {
		.get_block = get_block,
		.is_readahead = true,
	};

	while ((folio = readahead_folio(rac))) {
		prefetchw(&folio->flags);
		args.folio = folio;
		args.nr_pages = readahead_count(rac);
		args.bio = do_mpage_readpage(&args);
	}
	if (args.bio)
		mpage_bio_submit_read(args.bio);
}
EXPORT_SYMBOL(mpage_readahead);

 
int mpage_read_folio(struct folio *folio, get_block_t get_block)
{
	struct mpage_readpage_args args = {
		.folio = folio,
		.nr_pages = 1,
		.get_block = get_block,
	};

	args.bio = do_mpage_readpage(&args);
	if (args.bio)
		mpage_bio_submit_read(args.bio);
	return 0;
}
EXPORT_SYMBOL(mpage_read_folio);

 

struct mpage_data {
	struct bio *bio;
	sector_t last_block_in_bio;
	get_block_t *get_block;
};

 
static void clean_buffers(struct page *page, unsigned first_unmapped)
{
	unsigned buffer_counter = 0;
	struct buffer_head *bh, *head;
	if (!page_has_buffers(page))
		return;
	head = page_buffers(page);
	bh = head;

	do {
		if (buffer_counter++ == first_unmapped)
			break;
		clear_buffer_dirty(bh);
		bh = bh->b_this_page;
	} while (bh != head);

	 
	if (buffer_heads_over_limit && PageUptodate(page))
		try_to_free_buffers(page_folio(page));
}

 
void clean_page_buffers(struct page *page)
{
	clean_buffers(page, ~0U);
}

static int __mpage_writepage(struct folio *folio, struct writeback_control *wbc,
		      void *data)
{
	struct mpage_data *mpd = data;
	struct bio *bio = mpd->bio;
	struct address_space *mapping = folio->mapping;
	struct inode *inode = mapping->host;
	const unsigned blkbits = inode->i_blkbits;
	const unsigned blocks_per_page = PAGE_SIZE >> blkbits;
	sector_t last_block;
	sector_t block_in_file;
	sector_t blocks[MAX_BUF_PER_PAGE];
	unsigned page_block;
	unsigned first_unmapped = blocks_per_page;
	struct block_device *bdev = NULL;
	int boundary = 0;
	sector_t boundary_block = 0;
	struct block_device *boundary_bdev = NULL;
	size_t length;
	struct buffer_head map_bh;
	loff_t i_size = i_size_read(inode);
	int ret = 0;
	struct buffer_head *head = folio_buffers(folio);

	if (head) {
		struct buffer_head *bh = head;

		 
		page_block = 0;
		do {
			BUG_ON(buffer_locked(bh));
			if (!buffer_mapped(bh)) {
				 
				if (buffer_dirty(bh))
					goto confused;
				if (first_unmapped == blocks_per_page)
					first_unmapped = page_block;
				continue;
			}

			if (first_unmapped != blocks_per_page)
				goto confused;	 

			if (!buffer_dirty(bh) || !buffer_uptodate(bh))
				goto confused;
			if (page_block) {
				if (bh->b_blocknr != blocks[page_block-1] + 1)
					goto confused;
			}
			blocks[page_block++] = bh->b_blocknr;
			boundary = buffer_boundary(bh);
			if (boundary) {
				boundary_block = bh->b_blocknr;
				boundary_bdev = bh->b_bdev;
			}
			bdev = bh->b_bdev;
		} while ((bh = bh->b_this_page) != head);

		if (first_unmapped)
			goto page_is_mapped;

		 
		goto confused;
	}

	 
	BUG_ON(!folio_test_uptodate(folio));
	block_in_file = (sector_t)folio->index << (PAGE_SHIFT - blkbits);
	 
	if (block_in_file >= (i_size + (1 << blkbits) - 1) >> blkbits)
		goto page_is_mapped;
	last_block = (i_size - 1) >> blkbits;
	map_bh.b_folio = folio;
	for (page_block = 0; page_block < blocks_per_page; ) {

		map_bh.b_state = 0;
		map_bh.b_size = 1 << blkbits;
		if (mpd->get_block(inode, block_in_file, &map_bh, 1))
			goto confused;
		if (!buffer_mapped(&map_bh))
			goto confused;
		if (buffer_new(&map_bh))
			clean_bdev_bh_alias(&map_bh);
		if (buffer_boundary(&map_bh)) {
			boundary_block = map_bh.b_blocknr;
			boundary_bdev = map_bh.b_bdev;
		}
		if (page_block) {
			if (map_bh.b_blocknr != blocks[page_block-1] + 1)
				goto confused;
		}
		blocks[page_block++] = map_bh.b_blocknr;
		boundary = buffer_boundary(&map_bh);
		bdev = map_bh.b_bdev;
		if (block_in_file == last_block)
			break;
		block_in_file++;
	}
	BUG_ON(page_block == 0);

	first_unmapped = page_block;

page_is_mapped:
	 
	if (folio_pos(folio) >= i_size)
		goto confused;
	length = folio_size(folio);
	if (folio_pos(folio) + length > i_size) {
		 
		length = i_size - folio_pos(folio);
		folio_zero_segment(folio, length, folio_size(folio));
	}

	 
	if (bio && mpd->last_block_in_bio != blocks[0] - 1)
		bio = mpage_bio_submit_write(bio);

alloc_new:
	if (bio == NULL) {
		bio = bio_alloc(bdev, BIO_MAX_VECS,
				REQ_OP_WRITE | wbc_to_write_flags(wbc),
				GFP_NOFS);
		bio->bi_iter.bi_sector = blocks[0] << (blkbits - 9);
		wbc_init_bio(wbc, bio);
	}

	 
	wbc_account_cgroup_owner(wbc, &folio->page, folio_size(folio));
	length = first_unmapped << blkbits;
	if (!bio_add_folio(bio, folio, length, 0)) {
		bio = mpage_bio_submit_write(bio);
		goto alloc_new;
	}

	clean_buffers(&folio->page, first_unmapped);

	BUG_ON(folio_test_writeback(folio));
	folio_start_writeback(folio);
	folio_unlock(folio);
	if (boundary || (first_unmapped != blocks_per_page)) {
		bio = mpage_bio_submit_write(bio);
		if (boundary_block) {
			write_boundary_block(boundary_bdev,
					boundary_block, 1 << blkbits);
		}
	} else {
		mpd->last_block_in_bio = blocks[blocks_per_page - 1];
	}
	goto out;

confused:
	if (bio)
		bio = mpage_bio_submit_write(bio);

	 
	ret = block_write_full_page(&folio->page, mpd->get_block, wbc);
	mapping_set_error(mapping, ret);
out:
	mpd->bio = bio;
	return ret;
}

 
int
mpage_writepages(struct address_space *mapping,
		struct writeback_control *wbc, get_block_t get_block)
{
	struct mpage_data mpd = {
		.get_block	= get_block,
	};
	struct blk_plug plug;
	int ret;

	blk_start_plug(&plug);
	ret = write_cache_pages(mapping, wbc, __mpage_writepage, &mpd);
	if (mpd.bio)
		mpage_bio_submit_write(mpd.bio);
	blk_finish_plug(&plug);
	return ret;
}
EXPORT_SYMBOL(mpage_writepages);
