
 

#include <linux/types.h>
#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include "mdt.h"
#include "alloc.h"


 
static inline unsigned long
nilfs_palloc_groups_per_desc_block(const struct inode *inode)
{
	return i_blocksize(inode) /
		sizeof(struct nilfs_palloc_group_desc);
}

 
static inline unsigned long
nilfs_palloc_groups_count(const struct inode *inode)
{
	return 1UL << (BITS_PER_LONG - (inode->i_blkbits + 3  ));
}

 
int nilfs_palloc_init_blockgroup(struct inode *inode, unsigned int entry_size)
{
	struct nilfs_mdt_info *mi = NILFS_MDT(inode);

	mi->mi_bgl = kmalloc(sizeof(*mi->mi_bgl), GFP_NOFS);
	if (!mi->mi_bgl)
		return -ENOMEM;

	bgl_lock_init(mi->mi_bgl);

	nilfs_mdt_set_entry_size(inode, entry_size, 0);

	mi->mi_blocks_per_group =
		DIV_ROUND_UP(nilfs_palloc_entries_per_group(inode),
			     mi->mi_entries_per_block) + 1;
		 
	mi->mi_blocks_per_desc_block =
		nilfs_palloc_groups_per_desc_block(inode) *
		mi->mi_blocks_per_group + 1;
		 
	return 0;
}

 
static unsigned long nilfs_palloc_group(const struct inode *inode, __u64 nr,
					unsigned long *offset)
{
	__u64 group = nr;

	*offset = do_div(group, nilfs_palloc_entries_per_group(inode));
	return group;
}

 
static unsigned long
nilfs_palloc_desc_blkoff(const struct inode *inode, unsigned long group)
{
	unsigned long desc_block =
		group / nilfs_palloc_groups_per_desc_block(inode);
	return desc_block * NILFS_MDT(inode)->mi_blocks_per_desc_block;
}

 
static unsigned long
nilfs_palloc_bitmap_blkoff(const struct inode *inode, unsigned long group)
{
	unsigned long desc_offset =
		group % nilfs_palloc_groups_per_desc_block(inode);
	return nilfs_palloc_desc_blkoff(inode, group) + 1 +
		desc_offset * NILFS_MDT(inode)->mi_blocks_per_group;
}

 
static unsigned long
nilfs_palloc_group_desc_nfrees(const struct nilfs_palloc_group_desc *desc,
			       spinlock_t *lock)
{
	unsigned long nfree;

	spin_lock(lock);
	nfree = le32_to_cpu(desc->pg_nfrees);
	spin_unlock(lock);
	return nfree;
}

 
static u32
nilfs_palloc_group_desc_add_entries(struct nilfs_palloc_group_desc *desc,
				    spinlock_t *lock, u32 n)
{
	u32 nfree;

	spin_lock(lock);
	le32_add_cpu(&desc->pg_nfrees, n);
	nfree = le32_to_cpu(desc->pg_nfrees);
	spin_unlock(lock);
	return nfree;
}

 
static unsigned long
nilfs_palloc_entry_blkoff(const struct inode *inode, __u64 nr)
{
	unsigned long group, group_offset;

	group = nilfs_palloc_group(inode, nr, &group_offset);

	return nilfs_palloc_bitmap_blkoff(inode, group) + 1 +
		group_offset / NILFS_MDT(inode)->mi_entries_per_block;
}

 
static void nilfs_palloc_desc_block_init(struct inode *inode,
					 struct buffer_head *bh, void *kaddr)
{
	struct nilfs_palloc_group_desc *desc = kaddr + bh_offset(bh);
	unsigned long n = nilfs_palloc_groups_per_desc_block(inode);
	__le32 nfrees;

	nfrees = cpu_to_le32(nilfs_palloc_entries_per_group(inode));
	while (n-- > 0) {
		desc->pg_nfrees = nfrees;
		desc++;
	}
}

static int nilfs_palloc_get_block(struct inode *inode, unsigned long blkoff,
				  int create,
				  void (*init_block)(struct inode *,
						     struct buffer_head *,
						     void *),
				  struct buffer_head **bhp,
				  struct nilfs_bh_assoc *prev,
				  spinlock_t *lock)
{
	int ret;

	spin_lock(lock);
	if (prev->bh && blkoff == prev->blkoff &&
	    likely(buffer_uptodate(prev->bh))) {
		get_bh(prev->bh);
		*bhp = prev->bh;
		spin_unlock(lock);
		return 0;
	}
	spin_unlock(lock);

	ret = nilfs_mdt_get_block(inode, blkoff, create, init_block, bhp);
	if (!ret) {
		spin_lock(lock);
		 
		brelse(prev->bh);
		get_bh(*bhp);
		prev->bh = *bhp;
		prev->blkoff = blkoff;
		spin_unlock(lock);
	}
	return ret;
}

 
static int nilfs_palloc_delete_block(struct inode *inode, unsigned long blkoff,
				     struct nilfs_bh_assoc *prev,
				     spinlock_t *lock)
{
	spin_lock(lock);
	if (prev->bh && blkoff == prev->blkoff) {
		brelse(prev->bh);
		prev->bh = NULL;
	}
	spin_unlock(lock);
	return nilfs_mdt_delete_block(inode, blkoff);
}

 
static int nilfs_palloc_get_desc_block(struct inode *inode,
				       unsigned long group,
				       int create, struct buffer_head **bhp)
{
	struct nilfs_palloc_cache *cache = NILFS_MDT(inode)->mi_palloc_cache;

	return nilfs_palloc_get_block(inode,
				      nilfs_palloc_desc_blkoff(inode, group),
				      create, nilfs_palloc_desc_block_init,
				      bhp, &cache->prev_desc, &cache->lock);
}

 
static int nilfs_palloc_get_bitmap_block(struct inode *inode,
					 unsigned long group,
					 int create, struct buffer_head **bhp)
{
	struct nilfs_palloc_cache *cache = NILFS_MDT(inode)->mi_palloc_cache;

	return nilfs_palloc_get_block(inode,
				      nilfs_palloc_bitmap_blkoff(inode, group),
				      create, NULL, bhp,
				      &cache->prev_bitmap, &cache->lock);
}

 
static int nilfs_palloc_delete_bitmap_block(struct inode *inode,
					    unsigned long group)
{
	struct nilfs_palloc_cache *cache = NILFS_MDT(inode)->mi_palloc_cache;

	return nilfs_palloc_delete_block(inode,
					 nilfs_palloc_bitmap_blkoff(inode,
								    group),
					 &cache->prev_bitmap, &cache->lock);
}

 
int nilfs_palloc_get_entry_block(struct inode *inode, __u64 nr,
				 int create, struct buffer_head **bhp)
{
	struct nilfs_palloc_cache *cache = NILFS_MDT(inode)->mi_palloc_cache;

	return nilfs_palloc_get_block(inode,
				      nilfs_palloc_entry_blkoff(inode, nr),
				      create, NULL, bhp,
				      &cache->prev_entry, &cache->lock);
}

 
static int nilfs_palloc_delete_entry_block(struct inode *inode, __u64 nr)
{
	struct nilfs_palloc_cache *cache = NILFS_MDT(inode)->mi_palloc_cache;

	return nilfs_palloc_delete_block(inode,
					 nilfs_palloc_entry_blkoff(inode, nr),
					 &cache->prev_entry, &cache->lock);
}

 
static struct nilfs_palloc_group_desc *
nilfs_palloc_block_get_group_desc(const struct inode *inode,
				  unsigned long group,
				  const struct buffer_head *bh, void *kaddr)
{
	return (struct nilfs_palloc_group_desc *)(kaddr + bh_offset(bh)) +
		group % nilfs_palloc_groups_per_desc_block(inode);
}

 
void *nilfs_palloc_block_get_entry(const struct inode *inode, __u64 nr,
				   const struct buffer_head *bh, void *kaddr)
{
	unsigned long entry_offset, group_offset;

	nilfs_palloc_group(inode, nr, &group_offset);
	entry_offset = group_offset % NILFS_MDT(inode)->mi_entries_per_block;

	return kaddr + bh_offset(bh) +
		entry_offset * NILFS_MDT(inode)->mi_entry_size;
}

 
static int nilfs_palloc_find_available_slot(unsigned char *bitmap,
					    unsigned long target,
					    unsigned int bsize,
					    spinlock_t *lock)
{
	int pos, end = bsize;

	if (likely(target < bsize)) {
		pos = target;
		do {
			pos = nilfs_find_next_zero_bit(bitmap, end, pos);
			if (pos >= end)
				break;
			if (!nilfs_set_bit_atomic(lock, pos, bitmap))
				return pos;
		} while (++pos < end);

		end = target;
	}

	 
	for (pos = 0; pos < end; pos++) {
		pos = nilfs_find_next_zero_bit(bitmap, end, pos);
		if (pos >= end)
			break;
		if (!nilfs_set_bit_atomic(lock, pos, bitmap))
			return pos;
	}

	return -ENOSPC;
}

 
static unsigned long
nilfs_palloc_rest_groups_in_desc_block(const struct inode *inode,
				       unsigned long curr, unsigned long max)
{
	return min_t(unsigned long,
		     nilfs_palloc_groups_per_desc_block(inode) -
		     curr % nilfs_palloc_groups_per_desc_block(inode),
		     max - curr + 1);
}

 
static int nilfs_palloc_count_desc_blocks(struct inode *inode,
					    unsigned long *desc_blocks)
{
	__u64 blknum;
	int ret;

	ret = nilfs_bmap_last_key(NILFS_I(inode)->i_bmap, &blknum);
	if (likely(!ret))
		*desc_blocks = DIV_ROUND_UP(
			(unsigned long)blknum,
			NILFS_MDT(inode)->mi_blocks_per_desc_block);
	return ret;
}

 
static inline bool nilfs_palloc_mdt_file_can_grow(struct inode *inode,
						    unsigned long desc_blocks)
{
	return (nilfs_palloc_groups_per_desc_block(inode) * desc_blocks) <
			nilfs_palloc_groups_count(inode);
}

 
int nilfs_palloc_count_max_entries(struct inode *inode, u64 nused, u64 *nmaxp)
{
	unsigned long desc_blocks = 0;
	u64 entries_per_desc_block, nmax;
	int err;

	err = nilfs_palloc_count_desc_blocks(inode, &desc_blocks);
	if (unlikely(err))
		return err;

	entries_per_desc_block = (u64)nilfs_palloc_entries_per_group(inode) *
				nilfs_palloc_groups_per_desc_block(inode);
	nmax = entries_per_desc_block * desc_blocks;

	if (nused == nmax &&
			nilfs_palloc_mdt_file_can_grow(inode, desc_blocks))
		nmax += entries_per_desc_block;

	if (nused > nmax)
		return -ERANGE;

	*nmaxp = nmax;
	return 0;
}

 
int nilfs_palloc_prepare_alloc_entry(struct inode *inode,
				     struct nilfs_palloc_req *req)
{
	struct buffer_head *desc_bh, *bitmap_bh;
	struct nilfs_palloc_group_desc *desc;
	unsigned char *bitmap;
	void *desc_kaddr, *bitmap_kaddr;
	unsigned long group, maxgroup, ngroups;
	unsigned long group_offset, maxgroup_offset;
	unsigned long n, entries_per_group;
	unsigned long i, j;
	spinlock_t *lock;
	int pos, ret;

	ngroups = nilfs_palloc_groups_count(inode);
	maxgroup = ngroups - 1;
	group = nilfs_palloc_group(inode, req->pr_entry_nr, &group_offset);
	entries_per_group = nilfs_palloc_entries_per_group(inode);

	for (i = 0; i < ngroups; i += n) {
		if (group >= ngroups) {
			 
			group = 0;
			maxgroup = nilfs_palloc_group(inode, req->pr_entry_nr,
						      &maxgroup_offset) - 1;
		}
		ret = nilfs_palloc_get_desc_block(inode, group, 1, &desc_bh);
		if (ret < 0)
			return ret;
		desc_kaddr = kmap(desc_bh->b_page);
		desc = nilfs_palloc_block_get_group_desc(
			inode, group, desc_bh, desc_kaddr);
		n = nilfs_palloc_rest_groups_in_desc_block(inode, group,
							   maxgroup);
		for (j = 0; j < n; j++, desc++, group++) {
			lock = nilfs_mdt_bgl_lock(inode, group);
			if (nilfs_palloc_group_desc_nfrees(desc, lock) > 0) {
				ret = nilfs_palloc_get_bitmap_block(
					inode, group, 1, &bitmap_bh);
				if (ret < 0)
					goto out_desc;
				bitmap_kaddr = kmap(bitmap_bh->b_page);
				bitmap = bitmap_kaddr + bh_offset(bitmap_bh);
				pos = nilfs_palloc_find_available_slot(
					bitmap, group_offset,
					entries_per_group, lock);
				if (pos >= 0) {
					 
					nilfs_palloc_group_desc_add_entries(
						desc, lock, -1);
					req->pr_entry_nr =
						entries_per_group * group + pos;
					kunmap(desc_bh->b_page);
					kunmap(bitmap_bh->b_page);

					req->pr_desc_bh = desc_bh;
					req->pr_bitmap_bh = bitmap_bh;
					return 0;
				}
				kunmap(bitmap_bh->b_page);
				brelse(bitmap_bh);
			}

			group_offset = 0;
		}

		kunmap(desc_bh->b_page);
		brelse(desc_bh);
	}

	 
	return -ENOSPC;

 out_desc:
	kunmap(desc_bh->b_page);
	brelse(desc_bh);
	return ret;
}

 
void nilfs_palloc_commit_alloc_entry(struct inode *inode,
				     struct nilfs_palloc_req *req)
{
	mark_buffer_dirty(req->pr_bitmap_bh);
	mark_buffer_dirty(req->pr_desc_bh);
	nilfs_mdt_mark_dirty(inode);

	brelse(req->pr_bitmap_bh);
	brelse(req->pr_desc_bh);
}

 
void nilfs_palloc_commit_free_entry(struct inode *inode,
				    struct nilfs_palloc_req *req)
{
	struct nilfs_palloc_group_desc *desc;
	unsigned long group, group_offset;
	unsigned char *bitmap;
	void *desc_kaddr, *bitmap_kaddr;
	spinlock_t *lock;

	group = nilfs_palloc_group(inode, req->pr_entry_nr, &group_offset);
	desc_kaddr = kmap(req->pr_desc_bh->b_page);
	desc = nilfs_palloc_block_get_group_desc(inode, group,
						 req->pr_desc_bh, desc_kaddr);
	bitmap_kaddr = kmap(req->pr_bitmap_bh->b_page);
	bitmap = bitmap_kaddr + bh_offset(req->pr_bitmap_bh);
	lock = nilfs_mdt_bgl_lock(inode, group);

	if (!nilfs_clear_bit_atomic(lock, group_offset, bitmap))
		nilfs_warn(inode->i_sb,
			   "%s (ino=%lu): entry number %llu already freed",
			   __func__, inode->i_ino,
			   (unsigned long long)req->pr_entry_nr);
	else
		nilfs_palloc_group_desc_add_entries(desc, lock, 1);

	kunmap(req->pr_bitmap_bh->b_page);
	kunmap(req->pr_desc_bh->b_page);

	mark_buffer_dirty(req->pr_desc_bh);
	mark_buffer_dirty(req->pr_bitmap_bh);
	nilfs_mdt_mark_dirty(inode);

	brelse(req->pr_bitmap_bh);
	brelse(req->pr_desc_bh);
}

 
void nilfs_palloc_abort_alloc_entry(struct inode *inode,
				    struct nilfs_palloc_req *req)
{
	struct nilfs_palloc_group_desc *desc;
	void *desc_kaddr, *bitmap_kaddr;
	unsigned char *bitmap;
	unsigned long group, group_offset;
	spinlock_t *lock;

	group = nilfs_palloc_group(inode, req->pr_entry_nr, &group_offset);
	desc_kaddr = kmap(req->pr_desc_bh->b_page);
	desc = nilfs_palloc_block_get_group_desc(inode, group,
						 req->pr_desc_bh, desc_kaddr);
	bitmap_kaddr = kmap(req->pr_bitmap_bh->b_page);
	bitmap = bitmap_kaddr + bh_offset(req->pr_bitmap_bh);
	lock = nilfs_mdt_bgl_lock(inode, group);

	if (!nilfs_clear_bit_atomic(lock, group_offset, bitmap))
		nilfs_warn(inode->i_sb,
			   "%s (ino=%lu): entry number %llu already freed",
			   __func__, inode->i_ino,
			   (unsigned long long)req->pr_entry_nr);
	else
		nilfs_palloc_group_desc_add_entries(desc, lock, 1);

	kunmap(req->pr_bitmap_bh->b_page);
	kunmap(req->pr_desc_bh->b_page);

	brelse(req->pr_bitmap_bh);
	brelse(req->pr_desc_bh);

	req->pr_entry_nr = 0;
	req->pr_bitmap_bh = NULL;
	req->pr_desc_bh = NULL;
}

 
int nilfs_palloc_prepare_free_entry(struct inode *inode,
				    struct nilfs_palloc_req *req)
{
	struct buffer_head *desc_bh, *bitmap_bh;
	unsigned long group, group_offset;
	int ret;

	group = nilfs_palloc_group(inode, req->pr_entry_nr, &group_offset);
	ret = nilfs_palloc_get_desc_block(inode, group, 1, &desc_bh);
	if (ret < 0)
		return ret;
	ret = nilfs_palloc_get_bitmap_block(inode, group, 1, &bitmap_bh);
	if (ret < 0) {
		brelse(desc_bh);
		return ret;
	}

	req->pr_desc_bh = desc_bh;
	req->pr_bitmap_bh = bitmap_bh;
	return 0;
}

 
void nilfs_palloc_abort_free_entry(struct inode *inode,
				   struct nilfs_palloc_req *req)
{
	brelse(req->pr_bitmap_bh);
	brelse(req->pr_desc_bh);

	req->pr_entry_nr = 0;
	req->pr_bitmap_bh = NULL;
	req->pr_desc_bh = NULL;
}

 
int nilfs_palloc_freev(struct inode *inode, __u64 *entry_nrs, size_t nitems)
{
	struct buffer_head *desc_bh, *bitmap_bh;
	struct nilfs_palloc_group_desc *desc;
	unsigned char *bitmap;
	void *desc_kaddr, *bitmap_kaddr;
	unsigned long group, group_offset;
	__u64 group_min_nr, last_nrs[8];
	const unsigned long epg = nilfs_palloc_entries_per_group(inode);
	const unsigned int epb = NILFS_MDT(inode)->mi_entries_per_block;
	unsigned int entry_start, end, pos;
	spinlock_t *lock;
	int i, j, k, ret;
	u32 nfree;

	for (i = 0; i < nitems; i = j) {
		int change_group = false;
		int nempties = 0, n = 0;

		group = nilfs_palloc_group(inode, entry_nrs[i], &group_offset);
		ret = nilfs_palloc_get_desc_block(inode, group, 0, &desc_bh);
		if (ret < 0)
			return ret;
		ret = nilfs_palloc_get_bitmap_block(inode, group, 0,
						    &bitmap_bh);
		if (ret < 0) {
			brelse(desc_bh);
			return ret;
		}

		 
		group_min_nr = (__u64)group * epg;

		bitmap_kaddr = kmap(bitmap_bh->b_page);
		bitmap = bitmap_kaddr + bh_offset(bitmap_bh);
		lock = nilfs_mdt_bgl_lock(inode, group);

		j = i;
		entry_start = rounddown(group_offset, epb);
		do {
			if (!nilfs_clear_bit_atomic(lock, group_offset,
						    bitmap)) {
				nilfs_warn(inode->i_sb,
					   "%s (ino=%lu): entry number %llu already freed",
					   __func__, inode->i_ino,
					   (unsigned long long)entry_nrs[j]);
			} else {
				n++;
			}

			j++;
			if (j >= nitems || entry_nrs[j] < group_min_nr ||
			    entry_nrs[j] >= group_min_nr + epg) {
				change_group = true;
			} else {
				group_offset = entry_nrs[j] - group_min_nr;
				if (group_offset >= entry_start &&
				    group_offset < entry_start + epb) {
					 
					continue;
				}
			}

			 
			end = entry_start + epb;
			pos = nilfs_find_next_bit(bitmap, end, entry_start);
			if (pos >= end) {
				last_nrs[nempties++] = entry_nrs[j - 1];
				if (nempties >= ARRAY_SIZE(last_nrs))
					break;
			}

			if (change_group)
				break;

			 
			entry_start = rounddown(group_offset, epb);
		} while (true);

		kunmap(bitmap_bh->b_page);
		mark_buffer_dirty(bitmap_bh);
		brelse(bitmap_bh);

		for (k = 0; k < nempties; k++) {
			ret = nilfs_palloc_delete_entry_block(inode,
							      last_nrs[k]);
			if (ret && ret != -ENOENT)
				nilfs_warn(inode->i_sb,
					   "error %d deleting block that object (entry=%llu, ino=%lu) belongs to",
					   ret, (unsigned long long)last_nrs[k],
					   inode->i_ino);
		}

		desc_kaddr = kmap_atomic(desc_bh->b_page);
		desc = nilfs_palloc_block_get_group_desc(
			inode, group, desc_bh, desc_kaddr);
		nfree = nilfs_palloc_group_desc_add_entries(desc, lock, n);
		kunmap_atomic(desc_kaddr);
		mark_buffer_dirty(desc_bh);
		nilfs_mdt_mark_dirty(inode);
		brelse(desc_bh);

		if (nfree == nilfs_palloc_entries_per_group(inode)) {
			ret = nilfs_palloc_delete_bitmap_block(inode, group);
			if (ret && ret != -ENOENT)
				nilfs_warn(inode->i_sb,
					   "error %d deleting bitmap block of group=%lu, ino=%lu",
					   ret, group, inode->i_ino);
		}
	}
	return 0;
}

void nilfs_palloc_setup_cache(struct inode *inode,
			      struct nilfs_palloc_cache *cache)
{
	NILFS_MDT(inode)->mi_palloc_cache = cache;
	spin_lock_init(&cache->lock);
}

void nilfs_palloc_clear_cache(struct inode *inode)
{
	struct nilfs_palloc_cache *cache = NILFS_MDT(inode)->mi_palloc_cache;

	spin_lock(&cache->lock);
	brelse(cache->prev_desc.bh);
	brelse(cache->prev_bitmap.bh);
	brelse(cache->prev_entry.bh);
	cache->prev_desc.bh = NULL;
	cache->prev_bitmap.bh = NULL;
	cache->prev_entry.bh = NULL;
	spin_unlock(&cache->lock);
}

void nilfs_palloc_destroy_cache(struct inode *inode)
{
	nilfs_palloc_clear_cache(inode);
	NILFS_MDT(inode)->mi_palloc_cache = NULL;
}
