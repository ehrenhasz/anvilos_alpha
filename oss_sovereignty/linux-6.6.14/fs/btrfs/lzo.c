
 

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/pagemap.h>
#include <linux/bio.h>
#include <linux/lzo.h>
#include <linux/refcount.h>
#include "messages.h"
#include "compression.h"
#include "ctree.h"
#include "super.h"
#include "btrfs_inode.h"

#define LZO_LEN	4

 

#define WORKSPACE_BUF_LENGTH	(lzo1x_worst_compress(PAGE_SIZE))
#define WORKSPACE_CBUF_LENGTH	(lzo1x_worst_compress(PAGE_SIZE))

struct workspace {
	void *mem;
	void *buf;	 
	void *cbuf;	 
	struct list_head list;
};

static struct workspace_manager wsm;

void lzo_free_workspace(struct list_head *ws)
{
	struct workspace *workspace = list_entry(ws, struct workspace, list);

	kvfree(workspace->buf);
	kvfree(workspace->cbuf);
	kvfree(workspace->mem);
	kfree(workspace);
}

struct list_head *lzo_alloc_workspace(unsigned int level)
{
	struct workspace *workspace;

	workspace = kzalloc(sizeof(*workspace), GFP_KERNEL);
	if (!workspace)
		return ERR_PTR(-ENOMEM);

	workspace->mem = kvmalloc(LZO1X_MEM_COMPRESS, GFP_KERNEL | __GFP_NOWARN);
	workspace->buf = kvmalloc(WORKSPACE_BUF_LENGTH, GFP_KERNEL | __GFP_NOWARN);
	workspace->cbuf = kvmalloc(WORKSPACE_CBUF_LENGTH, GFP_KERNEL | __GFP_NOWARN);
	if (!workspace->mem || !workspace->buf || !workspace->cbuf)
		goto fail;

	INIT_LIST_HEAD(&workspace->list);

	return &workspace->list;
fail:
	lzo_free_workspace(&workspace->list);
	return ERR_PTR(-ENOMEM);
}

static inline void write_compress_length(char *buf, size_t len)
{
	__le32 dlen;

	dlen = cpu_to_le32(len);
	memcpy(buf, &dlen, LZO_LEN);
}

static inline size_t read_compress_length(const char *buf)
{
	__le32 dlen;

	memcpy(&dlen, buf, LZO_LEN);
	return le32_to_cpu(dlen);
}

 
static int copy_compressed_data_to_page(char *compressed_data,
					size_t compressed_size,
					struct page **out_pages,
					unsigned long max_nr_page,
					u32 *cur_out,
					const u32 sectorsize)
{
	u32 sector_bytes_left;
	u32 orig_out;
	struct page *cur_page;
	char *kaddr;

	if ((*cur_out / PAGE_SIZE) >= max_nr_page)
		return -E2BIG;

	 
	ASSERT((*cur_out / sectorsize) == (*cur_out + LZO_LEN - 1) / sectorsize);

	cur_page = out_pages[*cur_out / PAGE_SIZE];
	 
	if (!cur_page) {
		cur_page = alloc_page(GFP_NOFS);
		if (!cur_page)
			return -ENOMEM;
		out_pages[*cur_out / PAGE_SIZE] = cur_page;
	}

	kaddr = kmap_local_page(cur_page);
	write_compress_length(kaddr + offset_in_page(*cur_out),
			      compressed_size);
	*cur_out += LZO_LEN;

	orig_out = *cur_out;

	 
	while (*cur_out - orig_out < compressed_size) {
		u32 copy_len = min_t(u32, sectorsize - *cur_out % sectorsize,
				     orig_out + compressed_size - *cur_out);

		kunmap_local(kaddr);

		if ((*cur_out / PAGE_SIZE) >= max_nr_page)
			return -E2BIG;

		cur_page = out_pages[*cur_out / PAGE_SIZE];
		 
		if (!cur_page) {
			cur_page = alloc_page(GFP_NOFS);
			if (!cur_page)
				return -ENOMEM;
			out_pages[*cur_out / PAGE_SIZE] = cur_page;
		}
		kaddr = kmap_local_page(cur_page);

		memcpy(kaddr + offset_in_page(*cur_out),
		       compressed_data + *cur_out - orig_out, copy_len);

		*cur_out += copy_len;
	}

	 
	sector_bytes_left = round_up(*cur_out, sectorsize) - *cur_out;
	if (sector_bytes_left >= LZO_LEN || sector_bytes_left == 0)
		goto out;

	 
	memset(kaddr + offset_in_page(*cur_out), 0,
	       sector_bytes_left);
	*cur_out += sector_bytes_left;

out:
	kunmap_local(kaddr);
	return 0;
}

int lzo_compress_pages(struct list_head *ws, struct address_space *mapping,
		u64 start, struct page **pages, unsigned long *out_pages,
		unsigned long *total_in, unsigned long *total_out)
{
	struct workspace *workspace = list_entry(ws, struct workspace, list);
	const u32 sectorsize = btrfs_sb(mapping->host->i_sb)->sectorsize;
	struct page *page_in = NULL;
	char *sizes_ptr;
	const unsigned long max_nr_page = *out_pages;
	int ret = 0;
	 
	u64 cur_in = start;
	 
	u32 cur_out = 0;
	u32 len = *total_out;

	ASSERT(max_nr_page > 0);
	*out_pages = 0;
	*total_out = 0;
	*total_in = 0;

	 
	cur_out += LZO_LEN;
	while (cur_in < start + len) {
		char *data_in;
		const u32 sectorsize_mask = sectorsize - 1;
		u32 sector_off = (cur_in - start) & sectorsize_mask;
		u32 in_len;
		size_t out_len;

		 
		if (!page_in) {
			page_in = find_get_page(mapping, cur_in >> PAGE_SHIFT);
			ASSERT(page_in);
		}

		 
		in_len = min_t(u32, start + len - cur_in, sectorsize - sector_off);
		ASSERT(in_len);
		data_in = kmap_local_page(page_in);
		ret = lzo1x_1_compress(data_in +
				       offset_in_page(cur_in), in_len,
				       workspace->cbuf, &out_len,
				       workspace->mem);
		kunmap_local(data_in);
		if (ret < 0) {
			pr_debug("BTRFS: lzo in loop returned %d\n", ret);
			ret = -EIO;
			goto out;
		}

		ret = copy_compressed_data_to_page(workspace->cbuf, out_len,
						   pages, max_nr_page,
						   &cur_out, sectorsize);
		if (ret < 0)
			goto out;

		cur_in += in_len;

		 
		if (cur_in - start > sectorsize * 2 && cur_in - start < cur_out) {
			ret = -E2BIG;
			goto out;
		}

		 
		if (PAGE_ALIGNED(cur_in)) {
			put_page(page_in);
			page_in = NULL;
		}
	}

	 
	sizes_ptr = kmap_local_page(pages[0]);
	write_compress_length(sizes_ptr, cur_out);
	kunmap_local(sizes_ptr);

	ret = 0;
	*total_out = cur_out;
	*total_in = cur_in - start;
out:
	if (page_in)
		put_page(page_in);
	*out_pages = DIV_ROUND_UP(cur_out, PAGE_SIZE);
	return ret;
}

 
static void copy_compressed_segment(struct compressed_bio *cb,
				    char *dest, u32 len, u32 *cur_in)
{
	u32 orig_in = *cur_in;

	while (*cur_in < orig_in + len) {
		struct page *cur_page;
		u32 copy_len = min_t(u32, PAGE_SIZE - offset_in_page(*cur_in),
					  orig_in + len - *cur_in);

		ASSERT(copy_len);
		cur_page = cb->compressed_pages[*cur_in / PAGE_SIZE];

		memcpy_from_page(dest + *cur_in - orig_in, cur_page,
				 offset_in_page(*cur_in), copy_len);

		*cur_in += copy_len;
	}
}

int lzo_decompress_bio(struct list_head *ws, struct compressed_bio *cb)
{
	struct workspace *workspace = list_entry(ws, struct workspace, list);
	const struct btrfs_fs_info *fs_info = cb->bbio.inode->root->fs_info;
	const u32 sectorsize = fs_info->sectorsize;
	char *kaddr;
	int ret;
	 
	u32 len_in;
	 
	u32 cur_in = 0;
	 
	u32 cur_out = 0;

	kaddr = kmap_local_page(cb->compressed_pages[0]);
	len_in = read_compress_length(kaddr);
	kunmap_local(kaddr);
	cur_in += LZO_LEN;

	 
	if (len_in > min_t(size_t, BTRFS_MAX_COMPRESSED, cb->compressed_len) ||
	    round_up(len_in, sectorsize) < cb->compressed_len) {
		btrfs_err(fs_info,
			"invalid lzo header, lzo len %u compressed len %u",
			len_in, cb->compressed_len);
		return -EUCLEAN;
	}

	 
	while (cur_in < len_in) {
		struct page *cur_page;
		 
		u32 seg_len;
		u32 sector_bytes_left;
		size_t out_len = lzo1x_worst_compress(sectorsize);

		 
		ASSERT(cur_in / sectorsize ==
		       (cur_in + LZO_LEN - 1) / sectorsize);
		cur_page = cb->compressed_pages[cur_in / PAGE_SIZE];
		ASSERT(cur_page);
		kaddr = kmap_local_page(cur_page);
		seg_len = read_compress_length(kaddr + offset_in_page(cur_in));
		kunmap_local(kaddr);
		cur_in += LZO_LEN;

		if (seg_len > WORKSPACE_CBUF_LENGTH) {
			 
			btrfs_err(fs_info, "unexpectedly large lzo segment len %u",
					seg_len);
			return -EIO;
		}

		 
		copy_compressed_segment(cb, workspace->cbuf, seg_len, &cur_in);

		 
		ret = lzo1x_decompress_safe(workspace->cbuf, seg_len,
					    workspace->buf, &out_len);
		if (ret != LZO_E_OK) {
			btrfs_err(fs_info, "failed to decompress");
			return -EIO;
		}

		 
		ret = btrfs_decompress_buf2page(workspace->buf, out_len, cb, cur_out);
		cur_out += out_len;

		 
		if (ret == 0)
			return 0;
		ret = 0;

		 
		sector_bytes_left = sectorsize - (cur_in % sectorsize);
		if (sector_bytes_left >= LZO_LEN)
			continue;

		 
		cur_in += sector_bytes_left;
	}

	return 0;
}

int lzo_decompress(struct list_head *ws, const u8 *data_in,
		struct page *dest_page, unsigned long start_byte, size_t srclen,
		size_t destlen)
{
	struct workspace *workspace = list_entry(ws, struct workspace, list);
	size_t in_len;
	size_t out_len;
	size_t max_segment_len = WORKSPACE_BUF_LENGTH;
	int ret = 0;
	char *kaddr;
	unsigned long bytes;

	if (srclen < LZO_LEN || srclen > max_segment_len + LZO_LEN * 2)
		return -EUCLEAN;

	in_len = read_compress_length(data_in);
	if (in_len != srclen)
		return -EUCLEAN;
	data_in += LZO_LEN;

	in_len = read_compress_length(data_in);
	if (in_len != srclen - LZO_LEN * 2) {
		ret = -EUCLEAN;
		goto out;
	}
	data_in += LZO_LEN;

	out_len = PAGE_SIZE;
	ret = lzo1x_decompress_safe(data_in, in_len, workspace->buf, &out_len);
	if (ret != LZO_E_OK) {
		pr_warn("BTRFS: decompress failed!\n");
		ret = -EIO;
		goto out;
	}

	if (out_len < start_byte) {
		ret = -EIO;
		goto out;
	}

	 
	destlen = min_t(unsigned long, destlen, PAGE_SIZE);
	bytes = min_t(unsigned long, destlen, out_len - start_byte);

	kaddr = kmap_local_page(dest_page);
	memcpy(kaddr, workspace->buf + start_byte, bytes);

	 
	if (bytes < destlen)
		memset(kaddr+bytes, 0, destlen-bytes);
	kunmap_local(kaddr);
out:
	return ret;
}

const struct btrfs_compress_op btrfs_lzo_compress = {
	.workspace_manager	= &wsm,
	.max_level		= 1,
	.default_level		= 1,
};
