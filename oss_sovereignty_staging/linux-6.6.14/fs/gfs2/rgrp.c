
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/gfs2_ondisk.h>
#include <linux/prefetch.h>
#include <linux/blkdev.h>
#include <linux/rbtree.h>
#include <linux/random.h>

#include "gfs2.h"
#include "incore.h"
#include "glock.h"
#include "glops.h"
#include "lops.h"
#include "meta_io.h"
#include "quota.h"
#include "rgrp.h"
#include "super.h"
#include "trans.h"
#include "util.h"
#include "log.h"
#include "inode.h"
#include "trace_gfs2.h"
#include "dir.h"

#define BFITNOENT ((u32)~0)
#define NO_BLOCK ((u64)~0)

struct gfs2_rbm {
	struct gfs2_rgrpd *rgd;
	u32 offset;		 
	int bii;		 
};

static inline struct gfs2_bitmap *rbm_bi(const struct gfs2_rbm *rbm)
{
	return rbm->rgd->rd_bits + rbm->bii;
}

static inline u64 gfs2_rbm_to_block(const struct gfs2_rbm *rbm)
{
	BUG_ON(rbm->offset >= rbm->rgd->rd_data);
	return rbm->rgd->rd_data0 + (rbm_bi(rbm)->bi_start * GFS2_NBBY) +
		rbm->offset;
}

 

struct gfs2_extent {
	struct gfs2_rbm rbm;
	u32 len;
};

static const char valid_change[16] = {
	         
	  0, 1, 1, 1,
	  1, 0, 0, 0,
	  0, 0, 0, 1,
	        1, 0, 0, 0
};

static int gfs2_rbm_find(struct gfs2_rbm *rbm, u8 state, u32 *minext,
			 struct gfs2_blkreserv *rs, bool nowrap);


 

static inline void gfs2_setbit(const struct gfs2_rbm *rbm, bool do_clone,
			       unsigned char new_state)
{
	unsigned char *byte1, *byte2, *end, cur_state;
	struct gfs2_bitmap *bi = rbm_bi(rbm);
	unsigned int buflen = bi->bi_bytes;
	const unsigned int bit = (rbm->offset % GFS2_NBBY) * GFS2_BIT_SIZE;

	byte1 = bi->bi_bh->b_data + bi->bi_offset + (rbm->offset / GFS2_NBBY);
	end = bi->bi_bh->b_data + bi->bi_offset + buflen;

	BUG_ON(byte1 >= end);

	cur_state = (*byte1 >> bit) & GFS2_BIT_MASK;

	if (unlikely(!valid_change[new_state * 4 + cur_state])) {
		struct gfs2_sbd *sdp = rbm->rgd->rd_sbd;

		fs_warn(sdp, "buf_blk = 0x%x old_state=%d, new_state=%d\n",
			rbm->offset, cur_state, new_state);
		fs_warn(sdp, "rgrp=0x%llx bi_start=0x%x biblk: 0x%llx\n",
			(unsigned long long)rbm->rgd->rd_addr, bi->bi_start,
			(unsigned long long)bi->bi_bh->b_blocknr);
		fs_warn(sdp, "bi_offset=0x%x bi_bytes=0x%x block=0x%llx\n",
			bi->bi_offset, bi->bi_bytes,
			(unsigned long long)gfs2_rbm_to_block(rbm));
		dump_stack();
		gfs2_consist_rgrpd(rbm->rgd);
		return;
	}
	*byte1 ^= (cur_state ^ new_state) << bit;

	if (do_clone && bi->bi_clone) {
		byte2 = bi->bi_clone + bi->bi_offset + (rbm->offset / GFS2_NBBY);
		cur_state = (*byte2 >> bit) & GFS2_BIT_MASK;
		*byte2 ^= (cur_state ^ new_state) << bit;
	}
}

 

static inline u8 gfs2_testbit(const struct gfs2_rbm *rbm, bool use_clone)
{
	struct gfs2_bitmap *bi = rbm_bi(rbm);
	const u8 *buffer;
	const u8 *byte;
	unsigned int bit;

	if (use_clone && bi->bi_clone)
		buffer = bi->bi_clone;
	else
		buffer = bi->bi_bh->b_data;
	buffer += bi->bi_offset;
	byte = buffer + (rbm->offset / GFS2_NBBY);
	bit = (rbm->offset % GFS2_NBBY) * GFS2_BIT_SIZE;

	return (*byte >> bit) & GFS2_BIT_MASK;
}

 

static inline u64 gfs2_bit_search(const __le64 *ptr, u64 mask, u8 state)
{
	u64 tmp;
	static const u64 search[] = {
		[0] = 0xffffffffffffffffULL,
		[1] = 0xaaaaaaaaaaaaaaaaULL,
		[2] = 0x5555555555555555ULL,
		[3] = 0x0000000000000000ULL,
	};
	tmp = le64_to_cpu(*ptr) ^ search[state];
	tmp &= (tmp >> 1);
	tmp &= mask;
	return tmp;
}

 
static inline int rs_cmp(u64 start, u32 len, struct gfs2_blkreserv *rs)
{
	if (start >= rs->rs_start + rs->rs_requested)
		return 1;
	if (rs->rs_start >= start + len)
		return -1;
	return 0;
}

 

static u32 gfs2_bitfit(const u8 *buf, const unsigned int len,
		       u32 goal, u8 state)
{
	u32 spoint = (goal << 1) & ((8*sizeof(u64)) - 1);
	const __le64 *ptr = ((__le64 *)buf) + (goal >> 5);
	const __le64 *end = (__le64 *)(buf + ALIGN(len, sizeof(u64)));
	u64 tmp;
	u64 mask = 0x5555555555555555ULL;
	u32 bit;

	 
	mask <<= spoint;
	tmp = gfs2_bit_search(ptr, mask, state);
	ptr++;
	while(tmp == 0 && ptr < end) {
		tmp = gfs2_bit_search(ptr, 0x5555555555555555ULL, state);
		ptr++;
	}
	 
	if (ptr == end && (len & (sizeof(u64) - 1)))
		tmp &= (((u64)~0) >> (64 - 8*(len & (sizeof(u64) - 1))));
	 
	if (tmp == 0)
		return BFITNOENT;
	ptr--;
	bit = __ffs64(tmp);
	bit /= 2;	 
	return (((const unsigned char *)ptr - buf) * GFS2_NBBY) + bit;
}

 

static int gfs2_rbm_from_block(struct gfs2_rbm *rbm, u64 block)
{
	if (!rgrp_contains_block(rbm->rgd, block))
		return -E2BIG;
	rbm->bii = 0;
	rbm->offset = block - rbm->rgd->rd_data0;
	 
	if (rbm->offset < rbm_bi(rbm)->bi_blocks)
		return 0;

	 
	rbm->offset += (sizeof(struct gfs2_rgrp) -
			sizeof(struct gfs2_meta_header)) * GFS2_NBBY;
	rbm->bii = rbm->offset / rbm->rgd->rd_sbd->sd_blocks_per_bitmap;
	rbm->offset -= rbm->bii * rbm->rgd->rd_sbd->sd_blocks_per_bitmap;
	return 0;
}

 

static bool gfs2_rbm_add(struct gfs2_rbm *rbm, u32 blocks)
{
	struct gfs2_rgrpd *rgd = rbm->rgd;
	struct gfs2_bitmap *bi = rgd->rd_bits + rbm->bii;

	if (rbm->offset + blocks < bi->bi_blocks) {
		rbm->offset += blocks;
		return false;
	}
	blocks -= bi->bi_blocks - rbm->offset;

	for(;;) {
		bi++;
		if (bi == rgd->rd_bits + rgd->rd_length)
			return true;
		if (blocks < bi->bi_blocks) {
			rbm->offset = blocks;
			rbm->bii = bi - rgd->rd_bits;
			return false;
		}
		blocks -= bi->bi_blocks;
	}
}

 

static bool gfs2_unaligned_extlen(struct gfs2_rbm *rbm, u32 n_unaligned, u32 *len)
{
	u32 n;
	u8 res;

	for (n = 0; n < n_unaligned; n++) {
		res = gfs2_testbit(rbm, true);
		if (res != GFS2_BLKST_FREE)
			return true;
		(*len)--;
		if (*len == 0)
			return true;
		if (gfs2_rbm_add(rbm, 1))
			return true;
	}

	return false;
}

 

static u32 gfs2_free_extlen(const struct gfs2_rbm *rrbm, u32 len)
{
	struct gfs2_rbm rbm = *rrbm;
	u32 n_unaligned = rbm.offset & 3;
	u32 size = len;
	u32 bytes;
	u32 chunk_size;
	u8 *ptr, *start, *end;
	u64 block;
	struct gfs2_bitmap *bi;

	if (n_unaligned &&
	    gfs2_unaligned_extlen(&rbm, 4 - n_unaligned, &len))
		goto out;

	n_unaligned = len & 3;
	 
	while (len > 3) {
		bi = rbm_bi(&rbm);
		start = bi->bi_bh->b_data;
		if (bi->bi_clone)
			start = bi->bi_clone;
		start += bi->bi_offset;
		end = start + bi->bi_bytes;
		BUG_ON(rbm.offset & 3);
		start += (rbm.offset / GFS2_NBBY);
		bytes = min_t(u32, len / GFS2_NBBY, (end - start));
		ptr = memchr_inv(start, 0, bytes);
		chunk_size = ((ptr == NULL) ? bytes : (ptr - start));
		chunk_size *= GFS2_NBBY;
		BUG_ON(len < chunk_size);
		len -= chunk_size;
		block = gfs2_rbm_to_block(&rbm);
		if (gfs2_rbm_from_block(&rbm, block + chunk_size)) {
			n_unaligned = 0;
			break;
		}
		if (ptr) {
			n_unaligned = 3;
			break;
		}
		n_unaligned = len & 3;
	}

	 
	if (n_unaligned)
		gfs2_unaligned_extlen(&rbm, n_unaligned, &len);
out:
	return size - len;
}

 

static u32 gfs2_bitcount(struct gfs2_rgrpd *rgd, const u8 *buffer,
			 unsigned int buflen, u8 state)
{
	const u8 *byte = buffer;
	const u8 *end = buffer + buflen;
	const u8 state1 = state << 2;
	const u8 state2 = state << 4;
	const u8 state3 = state << 6;
	u32 count = 0;

	for (; byte < end; byte++) {
		if (((*byte) & 0x03) == state)
			count++;
		if (((*byte) & 0x0C) == state1)
			count++;
		if (((*byte) & 0x30) == state2)
			count++;
		if (((*byte) & 0xC0) == state3)
			count++;
	}

	return count;
}

 

void gfs2_rgrp_verify(struct gfs2_rgrpd *rgd)
{
	struct gfs2_sbd *sdp = rgd->rd_sbd;
	struct gfs2_bitmap *bi = NULL;
	u32 length = rgd->rd_length;
	u32 count[4], tmp;
	int buf, x;

	memset(count, 0, 4 * sizeof(u32));

	 
	for (buf = 0; buf < length; buf++) {
		bi = rgd->rd_bits + buf;
		for (x = 0; x < 4; x++)
			count[x] += gfs2_bitcount(rgd,
						  bi->bi_bh->b_data +
						  bi->bi_offset,
						  bi->bi_bytes, x);
	}

	if (count[0] != rgd->rd_free) {
		gfs2_lm(sdp, "free data mismatch:  %u != %u\n",
			count[0], rgd->rd_free);
		gfs2_consist_rgrpd(rgd);
		return;
	}

	tmp = rgd->rd_data - rgd->rd_free - rgd->rd_dinodes;
	if (count[1] != tmp) {
		gfs2_lm(sdp, "used data mismatch:  %u != %u\n",
			count[1], tmp);
		gfs2_consist_rgrpd(rgd);
		return;
	}

	if (count[2] + count[3] != rgd->rd_dinodes) {
		gfs2_lm(sdp, "used metadata mismatch:  %u != %u\n",
			count[2] + count[3], rgd->rd_dinodes);
		gfs2_consist_rgrpd(rgd);
		return;
	}
}

 

struct gfs2_rgrpd *gfs2_blk2rgrpd(struct gfs2_sbd *sdp, u64 blk, bool exact)
{
	struct rb_node *n, *next;
	struct gfs2_rgrpd *cur;

	spin_lock(&sdp->sd_rindex_spin);
	n = sdp->sd_rindex_tree.rb_node;
	while (n) {
		cur = rb_entry(n, struct gfs2_rgrpd, rd_node);
		next = NULL;
		if (blk < cur->rd_addr)
			next = n->rb_left;
		else if (blk >= cur->rd_data0 + cur->rd_data)
			next = n->rb_right;
		if (next == NULL) {
			spin_unlock(&sdp->sd_rindex_spin);
			if (exact) {
				if (blk < cur->rd_addr)
					return NULL;
				if (blk >= cur->rd_data0 + cur->rd_data)
					return NULL;
			}
			return cur;
		}
		n = next;
	}
	spin_unlock(&sdp->sd_rindex_spin);

	return NULL;
}

 

struct gfs2_rgrpd *gfs2_rgrpd_get_first(struct gfs2_sbd *sdp)
{
	const struct rb_node *n;
	struct gfs2_rgrpd *rgd;

	spin_lock(&sdp->sd_rindex_spin);
	n = rb_first(&sdp->sd_rindex_tree);
	rgd = rb_entry(n, struct gfs2_rgrpd, rd_node);
	spin_unlock(&sdp->sd_rindex_spin);

	return rgd;
}

 

struct gfs2_rgrpd *gfs2_rgrpd_get_next(struct gfs2_rgrpd *rgd)
{
	struct gfs2_sbd *sdp = rgd->rd_sbd;
	const struct rb_node *n;

	spin_lock(&sdp->sd_rindex_spin);
	n = rb_next(&rgd->rd_node);
	if (n == NULL)
		n = rb_first(&sdp->sd_rindex_tree);

	if (unlikely(&rgd->rd_node == n)) {
		spin_unlock(&sdp->sd_rindex_spin);
		return NULL;
	}
	rgd = rb_entry(n, struct gfs2_rgrpd, rd_node);
	spin_unlock(&sdp->sd_rindex_spin);
	return rgd;
}

void check_and_update_goal(struct gfs2_inode *ip)
{
	struct gfs2_sbd *sdp = GFS2_SB(&ip->i_inode);
	if (!ip->i_goal || gfs2_blk2rgrpd(sdp, ip->i_goal, 1) == NULL)
		ip->i_goal = ip->i_no_addr;
}

void gfs2_free_clones(struct gfs2_rgrpd *rgd)
{
	int x;

	for (x = 0; x < rgd->rd_length; x++) {
		struct gfs2_bitmap *bi = rgd->rd_bits + x;
		kfree(bi->bi_clone);
		bi->bi_clone = NULL;
	}
}

static void dump_rs(struct seq_file *seq, const struct gfs2_blkreserv *rs,
		    const char *fs_id_buf)
{
	struct gfs2_inode *ip = container_of(rs, struct gfs2_inode, i_res);

	gfs2_print_dbg(seq, "%s  B: n:%llu s:%llu f:%u\n",
		       fs_id_buf,
		       (unsigned long long)ip->i_no_addr,
		       (unsigned long long)rs->rs_start,
		       rs->rs_requested);
}

 
static void __rs_deltree(struct gfs2_blkreserv *rs)
{
	struct gfs2_rgrpd *rgd;

	if (!gfs2_rs_active(rs))
		return;

	rgd = rs->rs_rgd;
	trace_gfs2_rs(rs, TRACE_RS_TREEDEL);
	rb_erase(&rs->rs_node, &rgd->rd_rstree);
	RB_CLEAR_NODE(&rs->rs_node);

	if (rs->rs_requested) {
		 
		BUG_ON(rs->rs_rgd->rd_requested < rs->rs_requested);
		rs->rs_rgd->rd_requested -= rs->rs_requested;

		 
		rgd->rd_extfail_pt += rs->rs_requested;
		rs->rs_requested = 0;
	}
}

 
void gfs2_rs_deltree(struct gfs2_blkreserv *rs)
{
	struct gfs2_rgrpd *rgd;

	rgd = rs->rs_rgd;
	if (rgd) {
		spin_lock(&rgd->rd_rsspin);
		__rs_deltree(rs);
		BUG_ON(rs->rs_requested);
		spin_unlock(&rgd->rd_rsspin);
	}
}

 
void gfs2_rs_delete(struct gfs2_inode *ip)
{
	struct inode *inode = &ip->i_inode;

	down_write(&ip->i_rw_mutex);
	if (atomic_read(&inode->i_writecount) <= 1)
		gfs2_rs_deltree(&ip->i_res);
	up_write(&ip->i_rw_mutex);
}

 
static void return_all_reservations(struct gfs2_rgrpd *rgd)
{
	struct rb_node *n;
	struct gfs2_blkreserv *rs;

	spin_lock(&rgd->rd_rsspin);
	while ((n = rb_first(&rgd->rd_rstree))) {
		rs = rb_entry(n, struct gfs2_blkreserv, rs_node);
		__rs_deltree(rs);
	}
	spin_unlock(&rgd->rd_rsspin);
}

void gfs2_clear_rgrpd(struct gfs2_sbd *sdp)
{
	struct rb_node *n;
	struct gfs2_rgrpd *rgd;
	struct gfs2_glock *gl;

	while ((n = rb_first(&sdp->sd_rindex_tree))) {
		rgd = rb_entry(n, struct gfs2_rgrpd, rd_node);
		gl = rgd->rd_gl;

		rb_erase(n, &sdp->sd_rindex_tree);

		if (gl) {
			if (gl->gl_state != LM_ST_UNLOCKED) {
				gfs2_glock_cb(gl, LM_ST_UNLOCKED);
				flush_delayed_work(&gl->gl_work);
			}
			gfs2_rgrp_brelse(rgd);
			glock_clear_object(gl, rgd);
			gfs2_glock_put(gl);
		}

		gfs2_free_clones(rgd);
		return_all_reservations(rgd);
		kfree(rgd->rd_bits);
		rgd->rd_bits = NULL;
		kmem_cache_free(gfs2_rgrpd_cachep, rgd);
	}
}

 

static int compute_bitstructs(struct gfs2_rgrpd *rgd)
{
	struct gfs2_sbd *sdp = rgd->rd_sbd;
	struct gfs2_bitmap *bi;
	u32 length = rgd->rd_length;  
	u32 bytes_left, bytes;
	int x;

	if (!length)
		return -EINVAL;

	rgd->rd_bits = kcalloc(length, sizeof(struct gfs2_bitmap), GFP_NOFS);
	if (!rgd->rd_bits)
		return -ENOMEM;

	bytes_left = rgd->rd_bitbytes;

	for (x = 0; x < length; x++) {
		bi = rgd->rd_bits + x;

		bi->bi_flags = 0;
		 
		if (length == 1) {
			bytes = bytes_left;
			bi->bi_offset = sizeof(struct gfs2_rgrp);
			bi->bi_start = 0;
			bi->bi_bytes = bytes;
			bi->bi_blocks = bytes * GFS2_NBBY;
		 
		} else if (x == 0) {
			bytes = sdp->sd_sb.sb_bsize - sizeof(struct gfs2_rgrp);
			bi->bi_offset = sizeof(struct gfs2_rgrp);
			bi->bi_start = 0;
			bi->bi_bytes = bytes;
			bi->bi_blocks = bytes * GFS2_NBBY;
		 
		} else if (x + 1 == length) {
			bytes = bytes_left;
			bi->bi_offset = sizeof(struct gfs2_meta_header);
			bi->bi_start = rgd->rd_bitbytes - bytes_left;
			bi->bi_bytes = bytes;
			bi->bi_blocks = bytes * GFS2_NBBY;
		 
		} else {
			bytes = sdp->sd_sb.sb_bsize -
				sizeof(struct gfs2_meta_header);
			bi->bi_offset = sizeof(struct gfs2_meta_header);
			bi->bi_start = rgd->rd_bitbytes - bytes_left;
			bi->bi_bytes = bytes;
			bi->bi_blocks = bytes * GFS2_NBBY;
		}

		bytes_left -= bytes;
	}

	if (bytes_left) {
		gfs2_consist_rgrpd(rgd);
		return -EIO;
	}
	bi = rgd->rd_bits + (length - 1);
	if ((bi->bi_start + bi->bi_bytes) * GFS2_NBBY != rgd->rd_data) {
		gfs2_lm(sdp,
			"ri_addr = %llu\n"
			"ri_length = %u\n"
			"ri_data0 = %llu\n"
			"ri_data = %u\n"
			"ri_bitbytes = %u\n"
			"start=%u len=%u offset=%u\n",
			(unsigned long long)rgd->rd_addr,
			rgd->rd_length,
			(unsigned long long)rgd->rd_data0,
			rgd->rd_data,
			rgd->rd_bitbytes,
			bi->bi_start, bi->bi_bytes, bi->bi_offset);
		gfs2_consist_rgrpd(rgd);
		return -EIO;
	}

	return 0;
}

 
u64 gfs2_ri_total(struct gfs2_sbd *sdp)
{
	u64 total_data = 0;	
	struct inode *inode = sdp->sd_rindex;
	struct gfs2_inode *ip = GFS2_I(inode);
	char buf[sizeof(struct gfs2_rindex)];
	int error, rgrps;

	for (rgrps = 0;; rgrps++) {
		loff_t pos = rgrps * sizeof(struct gfs2_rindex);

		if (pos + sizeof(struct gfs2_rindex) > i_size_read(inode))
			break;
		error = gfs2_internal_read(ip, buf, &pos,
					   sizeof(struct gfs2_rindex));
		if (error != sizeof(struct gfs2_rindex))
			break;
		total_data += be32_to_cpu(((struct gfs2_rindex *)buf)->ri_data);
	}
	return total_data;
}

static int rgd_insert(struct gfs2_rgrpd *rgd)
{
	struct gfs2_sbd *sdp = rgd->rd_sbd;
	struct rb_node **newn = &sdp->sd_rindex_tree.rb_node, *parent = NULL;

	 
	while (*newn) {
		struct gfs2_rgrpd *cur = rb_entry(*newn, struct gfs2_rgrpd,
						  rd_node);

		parent = *newn;
		if (rgd->rd_addr < cur->rd_addr)
			newn = &((*newn)->rb_left);
		else if (rgd->rd_addr > cur->rd_addr)
			newn = &((*newn)->rb_right);
		else
			return -EEXIST;
	}

	rb_link_node(&rgd->rd_node, parent, newn);
	rb_insert_color(&rgd->rd_node, &sdp->sd_rindex_tree);
	sdp->sd_rgrps++;
	return 0;
}

 

static int read_rindex_entry(struct gfs2_inode *ip)
{
	struct gfs2_sbd *sdp = GFS2_SB(&ip->i_inode);
	loff_t pos = sdp->sd_rgrps * sizeof(struct gfs2_rindex);
	struct gfs2_rindex buf;
	int error;
	struct gfs2_rgrpd *rgd;

	if (pos >= i_size_read(&ip->i_inode))
		return 1;

	error = gfs2_internal_read(ip, (char *)&buf, &pos,
				   sizeof(struct gfs2_rindex));

	if (error != sizeof(struct gfs2_rindex))
		return (error == 0) ? 1 : error;

	rgd = kmem_cache_zalloc(gfs2_rgrpd_cachep, GFP_NOFS);
	error = -ENOMEM;
	if (!rgd)
		return error;

	rgd->rd_sbd = sdp;
	rgd->rd_addr = be64_to_cpu(buf.ri_addr);
	rgd->rd_length = be32_to_cpu(buf.ri_length);
	rgd->rd_data0 = be64_to_cpu(buf.ri_data0);
	rgd->rd_data = be32_to_cpu(buf.ri_data);
	rgd->rd_bitbytes = be32_to_cpu(buf.ri_bitbytes);
	spin_lock_init(&rgd->rd_rsspin);
	mutex_init(&rgd->rd_mutex);

	error = gfs2_glock_get(sdp, rgd->rd_addr,
			       &gfs2_rgrp_glops, CREATE, &rgd->rd_gl);
	if (error)
		goto fail;

	error = compute_bitstructs(rgd);
	if (error)
		goto fail_glock;

	rgd->rd_rgl = (struct gfs2_rgrp_lvb *)rgd->rd_gl->gl_lksb.sb_lvbptr;
	rgd->rd_flags &= ~GFS2_RDF_PREFERRED;
	if (rgd->rd_data > sdp->sd_max_rg_data)
		sdp->sd_max_rg_data = rgd->rd_data;
	spin_lock(&sdp->sd_rindex_spin);
	error = rgd_insert(rgd);
	spin_unlock(&sdp->sd_rindex_spin);
	if (!error) {
		glock_set_object(rgd->rd_gl, rgd);
		return 0;
	}

	error = 0;  
fail_glock:
	gfs2_glock_put(rgd->rd_gl);

fail:
	kfree(rgd->rd_bits);
	rgd->rd_bits = NULL;
	kmem_cache_free(gfs2_rgrpd_cachep, rgd);
	return error;
}

 
static void set_rgrp_preferences(struct gfs2_sbd *sdp)
{
	struct gfs2_rgrpd *rgd, *first;
	int i;

	 
	rgd = gfs2_rgrpd_get_first(sdp);
	for (i = 0; i < sdp->sd_lockstruct.ls_jid; i++)
		rgd = gfs2_rgrpd_get_next(rgd);
	first = rgd;

	do {
		rgd->rd_flags |= GFS2_RDF_PREFERRED;
		for (i = 0; i < sdp->sd_journals; i++) {
			rgd = gfs2_rgrpd_get_next(rgd);
			if (!rgd || rgd == first)
				break;
		}
	} while (rgd && rgd != first);
}

 

static int gfs2_ri_update(struct gfs2_inode *ip)
{
	struct gfs2_sbd *sdp = GFS2_SB(&ip->i_inode);
	int error;

	do {
		error = read_rindex_entry(ip);
	} while (error == 0);

	if (error < 0)
		return error;

	if (RB_EMPTY_ROOT(&sdp->sd_rindex_tree)) {
		fs_err(sdp, "no resource groups found in the file system.\n");
		return -ENOENT;
	}
	set_rgrp_preferences(sdp);

	sdp->sd_rindex_uptodate = 1;
	return 0;
}

 

int gfs2_rindex_update(struct gfs2_sbd *sdp)
{
	struct gfs2_inode *ip = GFS2_I(sdp->sd_rindex);
	struct gfs2_glock *gl = ip->i_gl;
	struct gfs2_holder ri_gh;
	int error = 0;
	int unlock_required = 0;

	 
	if (!sdp->sd_rindex_uptodate) {
		if (!gfs2_glock_is_locked_by_me(gl)) {
			error = gfs2_glock_nq_init(gl, LM_ST_SHARED, 0, &ri_gh);
			if (error)
				return error;
			unlock_required = 1;
		}
		if (!sdp->sd_rindex_uptodate)
			error = gfs2_ri_update(ip);
		if (unlock_required)
			gfs2_glock_dq_uninit(&ri_gh);
	}

	return error;
}

static void gfs2_rgrp_in(struct gfs2_rgrpd *rgd, const void *buf)
{
	const struct gfs2_rgrp *str = buf;
	u32 rg_flags;

	rg_flags = be32_to_cpu(str->rg_flags);
	rg_flags &= ~GFS2_RDF_MASK;
	rgd->rd_flags &= GFS2_RDF_MASK;
	rgd->rd_flags |= rg_flags;
	rgd->rd_free = be32_to_cpu(str->rg_free);
	rgd->rd_dinodes = be32_to_cpu(str->rg_dinodes);
	rgd->rd_igeneration = be64_to_cpu(str->rg_igeneration);
	 
}

static void gfs2_rgrp_ondisk2lvb(struct gfs2_rgrp_lvb *rgl, const void *buf)
{
	const struct gfs2_rgrp *str = buf;

	rgl->rl_magic = cpu_to_be32(GFS2_MAGIC);
	rgl->rl_flags = str->rg_flags;
	rgl->rl_free = str->rg_free;
	rgl->rl_dinodes = str->rg_dinodes;
	rgl->rl_igeneration = str->rg_igeneration;
	rgl->__pad = 0UL;
}

static void gfs2_rgrp_out(struct gfs2_rgrpd *rgd, void *buf)
{
	struct gfs2_rgrpd *next = gfs2_rgrpd_get_next(rgd);
	struct gfs2_rgrp *str = buf;
	u32 crc;

	str->rg_flags = cpu_to_be32(rgd->rd_flags & ~GFS2_RDF_MASK);
	str->rg_free = cpu_to_be32(rgd->rd_free);
	str->rg_dinodes = cpu_to_be32(rgd->rd_dinodes);
	if (next == NULL)
		str->rg_skip = 0;
	else if (next->rd_addr > rgd->rd_addr)
		str->rg_skip = cpu_to_be32(next->rd_addr - rgd->rd_addr);
	str->rg_igeneration = cpu_to_be64(rgd->rd_igeneration);
	str->rg_data0 = cpu_to_be64(rgd->rd_data0);
	str->rg_data = cpu_to_be32(rgd->rd_data);
	str->rg_bitbytes = cpu_to_be32(rgd->rd_bitbytes);
	str->rg_crc = 0;
	crc = gfs2_disk_hash(buf, sizeof(struct gfs2_rgrp));
	str->rg_crc = cpu_to_be32(crc);

	memset(&str->rg_reserved, 0, sizeof(str->rg_reserved));
	gfs2_rgrp_ondisk2lvb(rgd->rd_rgl, buf);
}

static int gfs2_rgrp_lvb_valid(struct gfs2_rgrpd *rgd)
{
	struct gfs2_rgrp_lvb *rgl = rgd->rd_rgl;
	struct gfs2_rgrp *str = (struct gfs2_rgrp *)rgd->rd_bits[0].bi_bh->b_data;
	struct gfs2_sbd *sdp = rgd->rd_sbd;
	int valid = 1;

	if (rgl->rl_flags != str->rg_flags) {
		fs_warn(sdp, "GFS2: rgd: %llu lvb flag mismatch %u/%u",
			(unsigned long long)rgd->rd_addr,
		       be32_to_cpu(rgl->rl_flags), be32_to_cpu(str->rg_flags));
		valid = 0;
	}
	if (rgl->rl_free != str->rg_free) {
		fs_warn(sdp, "GFS2: rgd: %llu lvb free mismatch %u/%u",
			(unsigned long long)rgd->rd_addr,
			be32_to_cpu(rgl->rl_free), be32_to_cpu(str->rg_free));
		valid = 0;
	}
	if (rgl->rl_dinodes != str->rg_dinodes) {
		fs_warn(sdp, "GFS2: rgd: %llu lvb dinode mismatch %u/%u",
			(unsigned long long)rgd->rd_addr,
			be32_to_cpu(rgl->rl_dinodes),
			be32_to_cpu(str->rg_dinodes));
		valid = 0;
	}
	if (rgl->rl_igeneration != str->rg_igeneration) {
		fs_warn(sdp, "GFS2: rgd: %llu lvb igen mismatch %llu/%llu",
			(unsigned long long)rgd->rd_addr,
			(unsigned long long)be64_to_cpu(rgl->rl_igeneration),
			(unsigned long long)be64_to_cpu(str->rg_igeneration));
		valid = 0;
	}
	return valid;
}

static u32 count_unlinked(struct gfs2_rgrpd *rgd)
{
	struct gfs2_bitmap *bi;
	const u32 length = rgd->rd_length;
	const u8 *buffer = NULL;
	u32 i, goal, count = 0;

	for (i = 0, bi = rgd->rd_bits; i < length; i++, bi++) {
		goal = 0;
		buffer = bi->bi_bh->b_data + bi->bi_offset;
		WARN_ON(!buffer_uptodate(bi->bi_bh));
		while (goal < bi->bi_blocks) {
			goal = gfs2_bitfit(buffer, bi->bi_bytes, goal,
					   GFS2_BLKST_UNLINKED);
			if (goal == BFITNOENT)
				break;
			count++;
			goal++;
		}
	}

	return count;
}

static void rgrp_set_bitmap_flags(struct gfs2_rgrpd *rgd)
{
	struct gfs2_bitmap *bi;
	int x;

	if (rgd->rd_free) {
		for (x = 0; x < rgd->rd_length; x++) {
			bi = rgd->rd_bits + x;
			clear_bit(GBF_FULL, &bi->bi_flags);
		}
	} else {
		for (x = 0; x < rgd->rd_length; x++) {
			bi = rgd->rd_bits + x;
			set_bit(GBF_FULL, &bi->bi_flags);
		}
	}
}

 

int gfs2_rgrp_go_instantiate(struct gfs2_glock *gl)
{
	struct gfs2_rgrpd *rgd = gl->gl_object;
	struct gfs2_sbd *sdp = rgd->rd_sbd;
	unsigned int length = rgd->rd_length;
	struct gfs2_bitmap *bi;
	unsigned int x, y;
	int error;

	if (rgd->rd_bits[0].bi_bh != NULL)
		return 0;

	for (x = 0; x < length; x++) {
		bi = rgd->rd_bits + x;
		error = gfs2_meta_read(gl, rgd->rd_addr + x, 0, 0, &bi->bi_bh);
		if (error)
			goto fail;
	}

	for (y = length; y--;) {
		bi = rgd->rd_bits + y;
		error = gfs2_meta_wait(sdp, bi->bi_bh);
		if (error)
			goto fail;
		if (gfs2_metatype_check(sdp, bi->bi_bh, y ? GFS2_METATYPE_RB :
					      GFS2_METATYPE_RG)) {
			error = -EIO;
			goto fail;
		}
	}

	gfs2_rgrp_in(rgd, (rgd->rd_bits[0].bi_bh)->b_data);
	rgrp_set_bitmap_flags(rgd);
	rgd->rd_flags |= GFS2_RDF_CHECK;
	rgd->rd_free_clone = rgd->rd_free;
	GLOCK_BUG_ON(rgd->rd_gl, rgd->rd_reserved);
	 
	rgd->rd_extfail_pt = rgd->rd_free;
	if (cpu_to_be32(GFS2_MAGIC) != rgd->rd_rgl->rl_magic) {
		rgd->rd_rgl->rl_unlinked = cpu_to_be32(count_unlinked(rgd));
		gfs2_rgrp_ondisk2lvb(rgd->rd_rgl,
				     rgd->rd_bits[0].bi_bh->b_data);
	} else if (sdp->sd_args.ar_rgrplvb) {
		if (!gfs2_rgrp_lvb_valid(rgd)){
			gfs2_consist_rgrpd(rgd);
			error = -EIO;
			goto fail;
		}
		if (rgd->rd_rgl->rl_unlinked == 0)
			rgd->rd_flags &= ~GFS2_RDF_CHECK;
	}
	return 0;

fail:
	while (x--) {
		bi = rgd->rd_bits + x;
		brelse(bi->bi_bh);
		bi->bi_bh = NULL;
		gfs2_assert_warn(sdp, !bi->bi_clone);
	}
	return error;
}

static int update_rgrp_lvb(struct gfs2_rgrpd *rgd, struct gfs2_holder *gh)
{
	u32 rl_flags;

	if (!test_bit(GLF_INSTANTIATE_NEEDED, &gh->gh_gl->gl_flags))
		return 0;

	if (cpu_to_be32(GFS2_MAGIC) != rgd->rd_rgl->rl_magic)
		return gfs2_instantiate(gh);

	rl_flags = be32_to_cpu(rgd->rd_rgl->rl_flags);
	rl_flags &= ~GFS2_RDF_MASK;
	rgd->rd_flags &= GFS2_RDF_MASK;
	rgd->rd_flags |= (rl_flags | GFS2_RDF_CHECK);
	if (rgd->rd_rgl->rl_unlinked == 0)
		rgd->rd_flags &= ~GFS2_RDF_CHECK;
	rgd->rd_free = be32_to_cpu(rgd->rd_rgl->rl_free);
	rgrp_set_bitmap_flags(rgd);
	rgd->rd_free_clone = rgd->rd_free;
	GLOCK_BUG_ON(rgd->rd_gl, rgd->rd_reserved);
	 
	rgd->rd_extfail_pt = rgd->rd_free;
	rgd->rd_dinodes = be32_to_cpu(rgd->rd_rgl->rl_dinodes);
	rgd->rd_igeneration = be64_to_cpu(rgd->rd_rgl->rl_igeneration);
	return 0;
}

 

void gfs2_rgrp_brelse(struct gfs2_rgrpd *rgd)
{
	int x, length = rgd->rd_length;

	for (x = 0; x < length; x++) {
		struct gfs2_bitmap *bi = rgd->rd_bits + x;
		if (bi->bi_bh) {
			brelse(bi->bi_bh);
			bi->bi_bh = NULL;
		}
	}
	set_bit(GLF_INSTANTIATE_NEEDED, &rgd->rd_gl->gl_flags);
}

int gfs2_rgrp_send_discards(struct gfs2_sbd *sdp, u64 offset,
			     struct buffer_head *bh,
			     const struct gfs2_bitmap *bi, unsigned minlen, u64 *ptrimmed)
{
	struct super_block *sb = sdp->sd_vfs;
	u64 blk;
	sector_t start = 0;
	sector_t nr_blks = 0;
	int rv = -EIO;
	unsigned int x;
	u32 trimmed = 0;
	u8 diff;

	for (x = 0; x < bi->bi_bytes; x++) {
		const u8 *clone = bi->bi_clone ? bi->bi_clone : bi->bi_bh->b_data;
		clone += bi->bi_offset;
		clone += x;
		if (bh) {
			const u8 *orig = bh->b_data + bi->bi_offset + x;
			diff = ~(*orig | (*orig >> 1)) & (*clone | (*clone >> 1));
		} else {
			diff = ~(*clone | (*clone >> 1));
		}
		diff &= 0x55;
		if (diff == 0)
			continue;
		blk = offset + ((bi->bi_start + x) * GFS2_NBBY);
		while(diff) {
			if (diff & 1) {
				if (nr_blks == 0)
					goto start_new_extent;
				if ((start + nr_blks) != blk) {
					if (nr_blks >= minlen) {
						rv = sb_issue_discard(sb,
							start, nr_blks,
							GFP_NOFS, 0);
						if (rv)
							goto fail;
						trimmed += nr_blks;
					}
					nr_blks = 0;
start_new_extent:
					start = blk;
				}
				nr_blks++;
			}
			diff >>= 2;
			blk++;
		}
	}
	if (nr_blks >= minlen) {
		rv = sb_issue_discard(sb, start, nr_blks, GFP_NOFS, 0);
		if (rv)
			goto fail;
		trimmed += nr_blks;
	}
	if (ptrimmed)
		*ptrimmed = trimmed;
	return 0;

fail:
	if (sdp->sd_args.ar_discard)
		fs_warn(sdp, "error %d on discard request, turning discards off for this filesystem\n", rv);
	sdp->sd_args.ar_discard = 0;
	return rv;
}

 

int gfs2_fitrim(struct file *filp, void __user *argp)
{
	struct inode *inode = file_inode(filp);
	struct gfs2_sbd *sdp = GFS2_SB(inode);
	struct block_device *bdev = sdp->sd_vfs->s_bdev;
	struct buffer_head *bh;
	struct gfs2_rgrpd *rgd;
	struct gfs2_rgrpd *rgd_end;
	struct gfs2_holder gh;
	struct fstrim_range r;
	int ret = 0;
	u64 amt;
	u64 trimmed = 0;
	u64 start, end, minlen;
	unsigned int x;
	unsigned bs_shift = sdp->sd_sb.sb_bsize_shift;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if (!test_bit(SDF_JOURNAL_LIVE, &sdp->sd_flags))
		return -EROFS;

	if (!bdev_max_discard_sectors(bdev))
		return -EOPNOTSUPP;

	if (copy_from_user(&r, argp, sizeof(r)))
		return -EFAULT;

	ret = gfs2_rindex_update(sdp);
	if (ret)
		return ret;

	start = r.start >> bs_shift;
	end = start + (r.len >> bs_shift);
	minlen = max_t(u64, r.minlen, sdp->sd_sb.sb_bsize);
	minlen = max_t(u64, minlen, bdev_discard_granularity(bdev)) >> bs_shift;

	if (end <= start || minlen > sdp->sd_max_rg_data)
		return -EINVAL;

	rgd = gfs2_blk2rgrpd(sdp, start, 0);
	rgd_end = gfs2_blk2rgrpd(sdp, end, 0);

	if ((gfs2_rgrpd_get_first(sdp) == gfs2_rgrpd_get_next(rgd_end))
	    && (start > rgd_end->rd_data0 + rgd_end->rd_data))
		return -EINVAL;  

	while (1) {

		ret = gfs2_glock_nq_init(rgd->rd_gl, LM_ST_EXCLUSIVE,
					 LM_FLAG_NODE_SCOPE, &gh);
		if (ret)
			goto out;

		if (!(rgd->rd_flags & GFS2_RGF_TRIMMED)) {
			 
			for (x = 0; x < rgd->rd_length; x++) {
				struct gfs2_bitmap *bi = rgd->rd_bits + x;
				rgrp_lock_local(rgd);
				ret = gfs2_rgrp_send_discards(sdp,
						rgd->rd_data0, NULL, bi, minlen,
						&amt);
				rgrp_unlock_local(rgd);
				if (ret) {
					gfs2_glock_dq_uninit(&gh);
					goto out;
				}
				trimmed += amt;
			}

			 
			ret = gfs2_trans_begin(sdp, RES_RG_HDR, 0);
			if (ret == 0) {
				bh = rgd->rd_bits[0].bi_bh;
				rgrp_lock_local(rgd);
				rgd->rd_flags |= GFS2_RGF_TRIMMED;
				gfs2_trans_add_meta(rgd->rd_gl, bh);
				gfs2_rgrp_out(rgd, bh->b_data);
				rgrp_unlock_local(rgd);
				gfs2_trans_end(sdp);
			}
		}
		gfs2_glock_dq_uninit(&gh);

		if (rgd == rgd_end)
			break;

		rgd = gfs2_rgrpd_get_next(rgd);
	}

out:
	r.len = trimmed << bs_shift;
	if (copy_to_user(argp, &r, sizeof(r)))
		return -EFAULT;

	return ret;
}

 
static void rs_insert(struct gfs2_inode *ip)
{
	struct rb_node **newn, *parent = NULL;
	int rc;
	struct gfs2_blkreserv *rs = &ip->i_res;
	struct gfs2_rgrpd *rgd = rs->rs_rgd;

	BUG_ON(gfs2_rs_active(rs));

	spin_lock(&rgd->rd_rsspin);
	newn = &rgd->rd_rstree.rb_node;
	while (*newn) {
		struct gfs2_blkreserv *cur =
			rb_entry(*newn, struct gfs2_blkreserv, rs_node);

		parent = *newn;
		rc = rs_cmp(rs->rs_start, rs->rs_requested, cur);
		if (rc > 0)
			newn = &((*newn)->rb_right);
		else if (rc < 0)
			newn = &((*newn)->rb_left);
		else {
			spin_unlock(&rgd->rd_rsspin);
			WARN_ON(1);
			return;
		}
	}

	rb_link_node(&rs->rs_node, parent, newn);
	rb_insert_color(&rs->rs_node, &rgd->rd_rstree);

	 
	rgd->rd_requested += rs->rs_requested;  
	spin_unlock(&rgd->rd_rsspin);
	trace_gfs2_rs(rs, TRACE_RS_INSERT);
}

 
static inline u32 rgd_free(struct gfs2_rgrpd *rgd, struct gfs2_blkreserv *rs)
{
	u32 tot_reserved, tot_free;

	if (WARN_ON_ONCE(rgd->rd_requested < rs->rs_requested))
		return 0;
	tot_reserved = rgd->rd_requested - rs->rs_requested;

	if (rgd->rd_free_clone < tot_reserved)
		tot_reserved = 0;

	tot_free = rgd->rd_free_clone - tot_reserved;

	return tot_free;
}

 

static void rg_mblk_search(struct gfs2_rgrpd *rgd, struct gfs2_inode *ip,
			   const struct gfs2_alloc_parms *ap)
{
	struct gfs2_rbm rbm = { .rgd = rgd, };
	u64 goal;
	struct gfs2_blkreserv *rs = &ip->i_res;
	u32 extlen;
	u32 free_blocks, blocks_available;
	int ret;
	struct inode *inode = &ip->i_inode;

	spin_lock(&rgd->rd_rsspin);
	free_blocks = rgd_free(rgd, rs);
	if (rgd->rd_free_clone < rgd->rd_requested)
		free_blocks = 0;
	blocks_available = rgd->rd_free_clone - rgd->rd_reserved;
	if (rgd == rs->rs_rgd)
		blocks_available += rs->rs_reserved;
	spin_unlock(&rgd->rd_rsspin);

	if (S_ISDIR(inode->i_mode))
		extlen = 1;
	else {
		extlen = max_t(u32, atomic_read(&ip->i_sizehint), ap->target);
		extlen = clamp(extlen, (u32)RGRP_RSRV_MINBLKS, free_blocks);
	}
	if (free_blocks < extlen || blocks_available < extlen)
		return;

	 
	if (rgrp_contains_block(rgd, ip->i_goal))
		goal = ip->i_goal;
	else
		goal = rgd->rd_last_alloc + rgd->rd_data0;

	if (WARN_ON(gfs2_rbm_from_block(&rbm, goal)))
		return;

	ret = gfs2_rbm_find(&rbm, GFS2_BLKST_FREE, &extlen, &ip->i_res, true);
	if (ret == 0) {
		rs->rs_start = gfs2_rbm_to_block(&rbm);
		rs->rs_requested = extlen;
		rs_insert(ip);
	} else {
		if (goal == rgd->rd_last_alloc + rgd->rd_data0)
			rgd->rd_last_alloc = 0;
	}
}

 

static u64 gfs2_next_unreserved_block(struct gfs2_rgrpd *rgd, u64 block,
				      u32 length,
				      struct gfs2_blkreserv *ignore_rs)
{
	struct gfs2_blkreserv *rs;
	struct rb_node *n;
	int rc;

	spin_lock(&rgd->rd_rsspin);
	n = rgd->rd_rstree.rb_node;
	while (n) {
		rs = rb_entry(n, struct gfs2_blkreserv, rs_node);
		rc = rs_cmp(block, length, rs);
		if (rc < 0)
			n = n->rb_left;
		else if (rc > 0)
			n = n->rb_right;
		else
			break;
	}

	if (n) {
		while (rs_cmp(block, length, rs) == 0 && rs != ignore_rs) {
			block = rs->rs_start + rs->rs_requested;
			n = n->rb_right;
			if (n == NULL)
				break;
			rs = rb_entry(n, struct gfs2_blkreserv, rs_node);
		}
	}

	spin_unlock(&rgd->rd_rsspin);
	return block;
}

 

static int gfs2_reservation_check_and_update(struct gfs2_rbm *rbm,
					     struct gfs2_blkreserv *rs,
					     u32 minext,
					     struct gfs2_extent *maxext)
{
	u64 block = gfs2_rbm_to_block(rbm);
	u32 extlen = 1;
	u64 nblock;

	 
	if (minext > 1) {
		extlen = gfs2_free_extlen(rbm, minext);
		if (extlen <= maxext->len)
			goto fail;
	}

	 
	nblock = gfs2_next_unreserved_block(rbm->rgd, block, extlen, rs);
	if (nblock == block) {
		if (!minext || extlen >= minext)
			return 0;

		if (extlen > maxext->len) {
			maxext->len = extlen;
			maxext->rbm = *rbm;
		}
	} else {
		u64 len = nblock - block;
		if (len >= (u64)1 << 32)
			return -E2BIG;
		extlen = len;
	}
fail:
	if (gfs2_rbm_add(rbm, extlen))
		return -E2BIG;
	return 1;
}

 

static int gfs2_rbm_find(struct gfs2_rbm *rbm, u8 state, u32 *minext,
			 struct gfs2_blkreserv *rs, bool nowrap)
{
	bool scan_from_start = rbm->bii == 0 && rbm->offset == 0;
	struct buffer_head *bh;
	int last_bii;
	u32 offset;
	u8 *buffer;
	bool wrapped = false;
	int ret;
	struct gfs2_bitmap *bi;
	struct gfs2_extent maxext = { .rbm.rgd = rbm->rgd, };

	 
	last_bii = rbm->bii - (rbm->offset == 0);

	while(1) {
		bi = rbm_bi(rbm);
		if (test_bit(GBF_FULL, &bi->bi_flags) &&
		    (state == GFS2_BLKST_FREE))
			goto next_bitmap;

		bh = bi->bi_bh;
		buffer = bh->b_data + bi->bi_offset;
		WARN_ON(!buffer_uptodate(bh));
		if (state != GFS2_BLKST_UNLINKED && bi->bi_clone)
			buffer = bi->bi_clone + bi->bi_offset;
		offset = gfs2_bitfit(buffer, bi->bi_bytes, rbm->offset, state);
		if (offset == BFITNOENT) {
			if (state == GFS2_BLKST_FREE && rbm->offset == 0)
				set_bit(GBF_FULL, &bi->bi_flags);
			goto next_bitmap;
		}
		rbm->offset = offset;
		if (!rs || !minext)
			return 0;

		ret = gfs2_reservation_check_and_update(rbm, rs, *minext,
							&maxext);
		if (ret == 0)
			return 0;
		if (ret > 0)
			goto next_iter;
		if (ret == -E2BIG) {
			rbm->bii = 0;
			rbm->offset = 0;
			goto res_covered_end_of_rgrp;
		}
		return ret;

next_bitmap:	 
		rbm->offset = 0;
		rbm->bii++;
		if (rbm->bii == rbm->rgd->rd_length)
			rbm->bii = 0;
res_covered_end_of_rgrp:
		if (rbm->bii == 0) {
			if (wrapped)
				break;
			wrapped = true;
			if (nowrap)
				break;
		}
next_iter:
		 
		if (wrapped && rbm->bii > last_bii)
			break;
	}

	if (state != GFS2_BLKST_FREE)
		return -ENOSPC;

	 
	if (wrapped && (scan_from_start || rbm->bii > last_bii) &&
	    *minext < rbm->rgd->rd_extfail_pt)
		rbm->rgd->rd_extfail_pt = *minext - 1;

	 
	if (maxext.len) {
		*rbm = maxext.rbm;
		*minext = maxext.len;
		return 0;
	}

	return -ENOSPC;
}

 

static void try_rgrp_unlink(struct gfs2_rgrpd *rgd, u64 *last_unlinked, u64 skip)
{
	u64 block;
	struct gfs2_sbd *sdp = rgd->rd_sbd;
	struct gfs2_glock *gl;
	struct gfs2_inode *ip;
	int error;
	int found = 0;
	struct gfs2_rbm rbm = { .rgd = rgd, .bii = 0, .offset = 0 };

	while (1) {
		error = gfs2_rbm_find(&rbm, GFS2_BLKST_UNLINKED, NULL, NULL,
				      true);
		if (error == -ENOSPC)
			break;
		if (WARN_ON_ONCE(error))
			break;

		block = gfs2_rbm_to_block(&rbm);
		if (gfs2_rbm_from_block(&rbm, block + 1))
			break;
		if (*last_unlinked != NO_BLOCK && block <= *last_unlinked)
			continue;
		if (block == skip)
			continue;
		*last_unlinked = block;

		error = gfs2_glock_get(sdp, block, &gfs2_iopen_glops, CREATE, &gl);
		if (error)
			continue;

		 
		ip = gl->gl_object;

		if (ip || !gfs2_queue_try_to_evict(gl))
			gfs2_glock_put(gl);
		else
			found++;

		 
		if (found > NR_CPUS)
			return;
	}

	rgd->rd_flags &= ~GFS2_RDF_CHECK;
	return;
}

 

static bool gfs2_rgrp_congested(const struct gfs2_rgrpd *rgd, int loops)
{
	const struct gfs2_glock *gl = rgd->rd_gl;
	const struct gfs2_sbd *sdp = gl->gl_name.ln_sbd;
	struct gfs2_lkstats *st;
	u64 r_dcount, l_dcount;
	u64 l_srttb, a_srttb = 0;
	s64 srttb_diff;
	u64 sqr_diff;
	u64 var;
	int cpu, nonzero = 0;

	preempt_disable();
	for_each_present_cpu(cpu) {
		st = &per_cpu_ptr(sdp->sd_lkstats, cpu)->lkstats[LM_TYPE_RGRP];
		if (st->stats[GFS2_LKS_SRTTB]) {
			a_srttb += st->stats[GFS2_LKS_SRTTB];
			nonzero++;
		}
	}
	st = &this_cpu_ptr(sdp->sd_lkstats)->lkstats[LM_TYPE_RGRP];
	if (nonzero)
		do_div(a_srttb, nonzero);
	r_dcount = st->stats[GFS2_LKS_DCOUNT];
	var = st->stats[GFS2_LKS_SRTTVARB] +
	      gl->gl_stats.stats[GFS2_LKS_SRTTVARB];
	preempt_enable();

	l_srttb = gl->gl_stats.stats[GFS2_LKS_SRTTB];
	l_dcount = gl->gl_stats.stats[GFS2_LKS_DCOUNT];

	if ((l_dcount < 1) || (r_dcount < 1) || (a_srttb == 0))
		return false;

	srttb_diff = a_srttb - l_srttb;
	sqr_diff = srttb_diff * srttb_diff;

	var *= 2;
	if (l_dcount < 8 || r_dcount < 8)
		var *= 2;
	if (loops == 1)
		var *= 2;

	return ((srttb_diff < 0) && (sqr_diff > var));
}

 
static bool gfs2_rgrp_used_recently(const struct gfs2_blkreserv *rs,
				    u64 msecs)
{
	u64 tdiff;

	tdiff = ktime_to_ns(ktime_sub(ktime_get_real(),
                            rs->rs_rgd->rd_gl->gl_dstamp));

	return tdiff > (msecs * 1000 * 1000);
}

static u32 gfs2_orlov_skip(const struct gfs2_inode *ip)
{
	const struct gfs2_sbd *sdp = GFS2_SB(&ip->i_inode);
	u32 skip;

	get_random_bytes(&skip, sizeof(skip));
	return skip % sdp->sd_rgrps;
}

static bool gfs2_select_rgrp(struct gfs2_rgrpd **pos, const struct gfs2_rgrpd *begin)
{
	struct gfs2_rgrpd *rgd = *pos;
	struct gfs2_sbd *sdp = rgd->rd_sbd;

	rgd = gfs2_rgrpd_get_next(rgd);
	if (rgd == NULL)
		rgd = gfs2_rgrpd_get_first(sdp);
	*pos = rgd;
	if (rgd != begin)  
		return true;
	return false;
}

 
static inline int fast_to_acquire(struct gfs2_rgrpd *rgd)
{
	struct gfs2_glock *gl = rgd->rd_gl;

	if (gl->gl_state != LM_ST_UNLOCKED && list_empty(&gl->gl_holders) &&
	    !test_bit(GLF_DEMOTE_IN_PROGRESS, &gl->gl_flags) &&
	    !test_bit(GLF_DEMOTE, &gl->gl_flags))
		return 1;
	if (rgd->rd_flags & GFS2_RDF_PREFERRED)
		return 1;
	return 0;
}

 

int gfs2_inplace_reserve(struct gfs2_inode *ip, struct gfs2_alloc_parms *ap)
{
	struct gfs2_sbd *sdp = GFS2_SB(&ip->i_inode);
	struct gfs2_rgrpd *begin = NULL;
	struct gfs2_blkreserv *rs = &ip->i_res;
	int error = 0, flags = LM_FLAG_NODE_SCOPE;
	bool rg_locked;
	u64 last_unlinked = NO_BLOCK;
	u32 target = ap->target;
	int loops = 0;
	u32 free_blocks, blocks_available, skip = 0;

	BUG_ON(rs->rs_reserved);

	if (sdp->sd_args.ar_rgrplvb)
		flags |= GL_SKIP;
	if (gfs2_assert_warn(sdp, target))
		return -EINVAL;
	if (gfs2_rs_active(rs)) {
		begin = rs->rs_rgd;
	} else if (rs->rs_rgd &&
		   rgrp_contains_block(rs->rs_rgd, ip->i_goal)) {
		begin = rs->rs_rgd;
	} else {
		check_and_update_goal(ip);
		rs->rs_rgd = begin = gfs2_blk2rgrpd(sdp, ip->i_goal, 1);
	}
	if (S_ISDIR(ip->i_inode.i_mode) && (ap->aflags & GFS2_AF_ORLOV))
		skip = gfs2_orlov_skip(ip);
	if (rs->rs_rgd == NULL)
		return -EBADSLT;

	while (loops < 3) {
		struct gfs2_rgrpd *rgd;

		rg_locked = gfs2_glock_is_locked_by_me(rs->rs_rgd->rd_gl);
		if (rg_locked) {
			rgrp_lock_local(rs->rs_rgd);
		} else {
			if (skip && skip--)
				goto next_rgrp;
			if (!gfs2_rs_active(rs)) {
				if (loops == 0 &&
				    !fast_to_acquire(rs->rs_rgd))
					goto next_rgrp;
				if ((loops < 2) &&
				    gfs2_rgrp_used_recently(rs, 1000) &&
				    gfs2_rgrp_congested(rs->rs_rgd, loops))
					goto next_rgrp;
			}
			error = gfs2_glock_nq_init(rs->rs_rgd->rd_gl,
						   LM_ST_EXCLUSIVE, flags,
						   &ip->i_rgd_gh);
			if (unlikely(error))
				return error;
			rgrp_lock_local(rs->rs_rgd);
			if (!gfs2_rs_active(rs) && (loops < 2) &&
			    gfs2_rgrp_congested(rs->rs_rgd, loops))
				goto skip_rgrp;
			if (sdp->sd_args.ar_rgrplvb) {
				error = update_rgrp_lvb(rs->rs_rgd,
							&ip->i_rgd_gh);
				if (unlikely(error)) {
					rgrp_unlock_local(rs->rs_rgd);
					gfs2_glock_dq_uninit(&ip->i_rgd_gh);
					return error;
				}
			}
		}

		 
		if ((rs->rs_rgd->rd_flags & (GFS2_RGF_NOALLOC |
						 GFS2_RDF_ERROR)) ||
		    (loops == 0 && target > rs->rs_rgd->rd_extfail_pt))
			goto skip_rgrp;

		if (sdp->sd_args.ar_rgrplvb) {
			error = gfs2_instantiate(&ip->i_rgd_gh);
			if (error)
				goto skip_rgrp;
		}

		 
		if (!gfs2_rs_active(rs))
			rg_mblk_search(rs->rs_rgd, ip, ap);

		 
		if (!gfs2_rs_active(rs) && (loops < 1))
			goto check_rgrp;

		 
		rgd = rs->rs_rgd;
		spin_lock(&rgd->rd_rsspin);
		free_blocks = rgd_free(rgd, rs);
		blocks_available = rgd->rd_free_clone - rgd->rd_reserved;
		if (free_blocks < target || blocks_available < target) {
			spin_unlock(&rgd->rd_rsspin);
			goto check_rgrp;
		}
		rs->rs_reserved = ap->target;
		if (rs->rs_reserved > blocks_available)
			rs->rs_reserved = blocks_available;
		rgd->rd_reserved += rs->rs_reserved;
		spin_unlock(&rgd->rd_rsspin);
		rgrp_unlock_local(rs->rs_rgd);
		return 0;
check_rgrp:
		 
		if (rs->rs_rgd->rd_flags & GFS2_RDF_CHECK)
			try_rgrp_unlink(rs->rs_rgd, &last_unlinked,
					ip->i_no_addr);
skip_rgrp:
		rgrp_unlock_local(rs->rs_rgd);

		 
		if (gfs2_rs_active(rs))
			gfs2_rs_deltree(rs);

		 
		if (!rg_locked)
			gfs2_glock_dq_uninit(&ip->i_rgd_gh);
next_rgrp:
		 
		if (gfs2_select_rgrp(&rs->rs_rgd, begin))
			continue;
		if (skip)
			continue;

		 
		loops++;
		 
		if (ip == GFS2_I(sdp->sd_rindex) && !sdp->sd_rindex_uptodate) {
			error = gfs2_ri_update(ip);
			if (error)
				return error;
		}
		 
		if (loops == 2) {
			if (ap->min_target)
				target = ap->min_target;
			gfs2_log_flush(sdp, NULL, GFS2_LOG_HEAD_FLUSH_NORMAL |
				       GFS2_LFC_INPLACE_RESERVE);
		}
	}

	return -ENOSPC;
}

 

void gfs2_inplace_release(struct gfs2_inode *ip)
{
	struct gfs2_blkreserv *rs = &ip->i_res;

	if (rs->rs_reserved) {
		struct gfs2_rgrpd *rgd = rs->rs_rgd;

		spin_lock(&rgd->rd_rsspin);
		GLOCK_BUG_ON(rgd->rd_gl, rgd->rd_reserved < rs->rs_reserved);
		rgd->rd_reserved -= rs->rs_reserved;
		spin_unlock(&rgd->rd_rsspin);
		rs->rs_reserved = 0;
	}
	if (gfs2_holder_initialized(&ip->i_rgd_gh))
		gfs2_glock_dq_uninit(&ip->i_rgd_gh);
}

 
static void gfs2_alloc_extent(const struct gfs2_rbm *rbm, bool dinode,
			     unsigned int *n)
{
	struct gfs2_rbm pos = { .rgd = rbm->rgd, };
	const unsigned int elen = *n;
	u64 block;
	int ret;

	*n = 1;
	block = gfs2_rbm_to_block(rbm);
	gfs2_trans_add_meta(rbm->rgd->rd_gl, rbm_bi(rbm)->bi_bh);
	gfs2_setbit(rbm, true, dinode ? GFS2_BLKST_DINODE : GFS2_BLKST_USED);
	block++;
	while (*n < elen) {
		ret = gfs2_rbm_from_block(&pos, block);
		if (ret || gfs2_testbit(&pos, true) != GFS2_BLKST_FREE)
			break;
		gfs2_trans_add_meta(pos.rgd->rd_gl, rbm_bi(&pos)->bi_bh);
		gfs2_setbit(&pos, true, GFS2_BLKST_USED);
		(*n)++;
		block++;
	}
}

 

static void rgblk_free(struct gfs2_sbd *sdp, struct gfs2_rgrpd *rgd,
		       u64 bstart, u32 blen, unsigned char new_state)
{
	struct gfs2_rbm rbm;
	struct gfs2_bitmap *bi, *bi_prev = NULL;

	rbm.rgd = rgd;
	if (WARN_ON_ONCE(gfs2_rbm_from_block(&rbm, bstart)))
		return;
	while (blen--) {
		bi = rbm_bi(&rbm);
		if (bi != bi_prev) {
			if (!bi->bi_clone) {
				bi->bi_clone = kmalloc(bi->bi_bh->b_size,
						      GFP_NOFS | __GFP_NOFAIL);
				memcpy(bi->bi_clone + bi->bi_offset,
				       bi->bi_bh->b_data + bi->bi_offset,
				       bi->bi_bytes);
			}
			gfs2_trans_add_meta(rbm.rgd->rd_gl, bi->bi_bh);
			bi_prev = bi;
		}
		gfs2_setbit(&rbm, false, new_state);
		gfs2_rbm_add(&rbm, 1);
	}
}

 

void gfs2_rgrp_dump(struct seq_file *seq, struct gfs2_rgrpd *rgd,
		    const char *fs_id_buf)
{
	struct gfs2_blkreserv *trs;
	const struct rb_node *n;

	spin_lock(&rgd->rd_rsspin);
	gfs2_print_dbg(seq, "%s R: n:%llu f:%02x b:%u/%u i:%u q:%u r:%u e:%u\n",
		       fs_id_buf,
		       (unsigned long long)rgd->rd_addr, rgd->rd_flags,
		       rgd->rd_free, rgd->rd_free_clone, rgd->rd_dinodes,
		       rgd->rd_requested, rgd->rd_reserved, rgd->rd_extfail_pt);
	if (rgd->rd_sbd->sd_args.ar_rgrplvb && rgd->rd_rgl) {
		struct gfs2_rgrp_lvb *rgl = rgd->rd_rgl;

		gfs2_print_dbg(seq, "%s  L: f:%02x b:%u i:%u\n", fs_id_buf,
			       be32_to_cpu(rgl->rl_flags),
			       be32_to_cpu(rgl->rl_free),
			       be32_to_cpu(rgl->rl_dinodes));
	}
	for (n = rb_first(&rgd->rd_rstree); n; n = rb_next(&trs->rs_node)) {
		trs = rb_entry(n, struct gfs2_blkreserv, rs_node);
		dump_rs(seq, trs, fs_id_buf);
	}
	spin_unlock(&rgd->rd_rsspin);
}

static void gfs2_rgrp_error(struct gfs2_rgrpd *rgd)
{
	struct gfs2_sbd *sdp = rgd->rd_sbd;
	char fs_id_buf[sizeof(sdp->sd_fsname) + 7];

	fs_warn(sdp, "rgrp %llu has an error, marking it readonly until umount\n",
		(unsigned long long)rgd->rd_addr);
	fs_warn(sdp, "umount on all nodes and run fsck.gfs2 to fix the error\n");
	sprintf(fs_id_buf, "fsid=%s: ", sdp->sd_fsname);
	gfs2_rgrp_dump(NULL, rgd, fs_id_buf);
	rgd->rd_flags |= GFS2_RDF_ERROR;
}

 

static void gfs2_adjust_reservation(struct gfs2_inode *ip,
				    const struct gfs2_rbm *rbm, unsigned len)
{
	struct gfs2_blkreserv *rs = &ip->i_res;
	struct gfs2_rgrpd *rgd = rbm->rgd;

	BUG_ON(rs->rs_reserved < len);
	rs->rs_reserved -= len;
	if (gfs2_rs_active(rs)) {
		u64 start = gfs2_rbm_to_block(rbm);

		if (rs->rs_start == start) {
			unsigned int rlen;

			rs->rs_start += len;
			rlen = min(rs->rs_requested, len);
			rs->rs_requested -= rlen;
			rgd->rd_requested -= rlen;
			trace_gfs2_rs(rs, TRACE_RS_CLAIM);
			if (rs->rs_start < rgd->rd_data0 + rgd->rd_data &&
			    rs->rs_requested)
				return;
			 
			atomic_add(RGRP_RSRV_ADDBLKS, &ip->i_sizehint);
		}
		__rs_deltree(rs);
	}
}

 

static void gfs2_set_alloc_start(struct gfs2_rbm *rbm,
				 const struct gfs2_inode *ip, bool dinode)
{
	u64 goal;

	if (gfs2_rs_active(&ip->i_res)) {
		goal = ip->i_res.rs_start;
	} else {
		if (!dinode && rgrp_contains_block(rbm->rgd, ip->i_goal))
			goal = ip->i_goal;
		else
			goal = rbm->rgd->rd_last_alloc + rbm->rgd->rd_data0;
	}
	if (WARN_ON_ONCE(gfs2_rbm_from_block(rbm, goal))) {
		rbm->bii = 0;
		rbm->offset = 0;
	}
}

 

int gfs2_alloc_blocks(struct gfs2_inode *ip, u64 *bn, unsigned int *nblocks,
		      bool dinode, u64 *generation)
{
	struct gfs2_sbd *sdp = GFS2_SB(&ip->i_inode);
	struct buffer_head *dibh;
	struct gfs2_rbm rbm = { .rgd = ip->i_res.rs_rgd, };
	u64 block;  
	u32 minext = 1;
	int error = -ENOSPC;

	BUG_ON(ip->i_res.rs_reserved < *nblocks);

	rgrp_lock_local(rbm.rgd);
	if (gfs2_rs_active(&ip->i_res)) {
		gfs2_set_alloc_start(&rbm, ip, dinode);
		error = gfs2_rbm_find(&rbm, GFS2_BLKST_FREE, &minext, &ip->i_res, false);
	}
	if (error == -ENOSPC) {
		gfs2_set_alloc_start(&rbm, ip, dinode);
		error = gfs2_rbm_find(&rbm, GFS2_BLKST_FREE, &minext, NULL, false);
	}

	 
	if (error) {
		fs_warn(sdp, "inum=%llu error=%d, nblocks=%u, full=%d fail_pt=%d\n",
			(unsigned long long)ip->i_no_addr, error, *nblocks,
			test_bit(GBF_FULL, &rbm.rgd->rd_bits->bi_flags),
			rbm.rgd->rd_extfail_pt);
		goto rgrp_error;
	}

	gfs2_alloc_extent(&rbm, dinode, nblocks);
	block = gfs2_rbm_to_block(&rbm);
	rbm.rgd->rd_last_alloc = block - rbm.rgd->rd_data0;
	if (!dinode) {
		ip->i_goal = block + *nblocks - 1;
		error = gfs2_meta_inode_buffer(ip, &dibh);
		if (error == 0) {
			struct gfs2_dinode *di =
				(struct gfs2_dinode *)dibh->b_data;
			gfs2_trans_add_meta(ip->i_gl, dibh);
			di->di_goal_meta = di->di_goal_data =
				cpu_to_be64(ip->i_goal);
			brelse(dibh);
		}
	}
	spin_lock(&rbm.rgd->rd_rsspin);
	gfs2_adjust_reservation(ip, &rbm, *nblocks);
	if (rbm.rgd->rd_free < *nblocks || rbm.rgd->rd_reserved < *nblocks) {
		fs_warn(sdp, "nblocks=%u\n", *nblocks);
		spin_unlock(&rbm.rgd->rd_rsspin);
		goto rgrp_error;
	}
	GLOCK_BUG_ON(rbm.rgd->rd_gl, rbm.rgd->rd_reserved < *nblocks);
	GLOCK_BUG_ON(rbm.rgd->rd_gl, rbm.rgd->rd_free_clone < *nblocks);
	GLOCK_BUG_ON(rbm.rgd->rd_gl, rbm.rgd->rd_free < *nblocks);
	rbm.rgd->rd_reserved -= *nblocks;
	rbm.rgd->rd_free_clone -= *nblocks;
	rbm.rgd->rd_free -= *nblocks;
	spin_unlock(&rbm.rgd->rd_rsspin);
	if (dinode) {
		rbm.rgd->rd_dinodes++;
		*generation = rbm.rgd->rd_igeneration++;
		if (*generation == 0)
			*generation = rbm.rgd->rd_igeneration++;
	}

	gfs2_trans_add_meta(rbm.rgd->rd_gl, rbm.rgd->rd_bits[0].bi_bh);
	gfs2_rgrp_out(rbm.rgd, rbm.rgd->rd_bits[0].bi_bh->b_data);
	rgrp_unlock_local(rbm.rgd);

	gfs2_statfs_change(sdp, 0, -(s64)*nblocks, dinode ? 1 : 0);
	if (dinode)
		gfs2_trans_remove_revoke(sdp, block, *nblocks);

	gfs2_quota_change(ip, *nblocks, ip->i_inode.i_uid, ip->i_inode.i_gid);

	trace_gfs2_block_alloc(ip, rbm.rgd, block, *nblocks,
			       dinode ? GFS2_BLKST_DINODE : GFS2_BLKST_USED);
	*bn = block;
	return 0;

rgrp_error:
	rgrp_unlock_local(rbm.rgd);
	gfs2_rgrp_error(rbm.rgd);
	return -EIO;
}

 

void __gfs2_free_blocks(struct gfs2_inode *ip, struct gfs2_rgrpd *rgd,
			u64 bstart, u32 blen, int meta)
{
	struct gfs2_sbd *sdp = GFS2_SB(&ip->i_inode);

	rgrp_lock_local(rgd);
	rgblk_free(sdp, rgd, bstart, blen, GFS2_BLKST_FREE);
	trace_gfs2_block_alloc(ip, rgd, bstart, blen, GFS2_BLKST_FREE);
	rgd->rd_free += blen;
	rgd->rd_flags &= ~GFS2_RGF_TRIMMED;
	gfs2_trans_add_meta(rgd->rd_gl, rgd->rd_bits[0].bi_bh);
	gfs2_rgrp_out(rgd, rgd->rd_bits[0].bi_bh->b_data);
	rgrp_unlock_local(rgd);

	 
	if (meta || ip->i_depth || gfs2_is_jdata(ip))
		gfs2_journal_wipe(ip, bstart, blen);
}

 

void gfs2_free_meta(struct gfs2_inode *ip, struct gfs2_rgrpd *rgd,
		    u64 bstart, u32 blen)
{
	struct gfs2_sbd *sdp = GFS2_SB(&ip->i_inode);

	__gfs2_free_blocks(ip, rgd, bstart, blen, 1);
	gfs2_statfs_change(sdp, 0, +blen, 0);
	gfs2_quota_change(ip, -(s64)blen, ip->i_inode.i_uid, ip->i_inode.i_gid);
}

void gfs2_unlink_di(struct inode *inode)
{
	struct gfs2_inode *ip = GFS2_I(inode);
	struct gfs2_sbd *sdp = GFS2_SB(inode);
	struct gfs2_rgrpd *rgd;
	u64 blkno = ip->i_no_addr;

	rgd = gfs2_blk2rgrpd(sdp, blkno, true);
	if (!rgd)
		return;
	rgrp_lock_local(rgd);
	rgblk_free(sdp, rgd, blkno, 1, GFS2_BLKST_UNLINKED);
	trace_gfs2_block_alloc(ip, rgd, blkno, 1, GFS2_BLKST_UNLINKED);
	gfs2_trans_add_meta(rgd->rd_gl, rgd->rd_bits[0].bi_bh);
	gfs2_rgrp_out(rgd, rgd->rd_bits[0].bi_bh->b_data);
	be32_add_cpu(&rgd->rd_rgl->rl_unlinked, 1);
	rgrp_unlock_local(rgd);
}

void gfs2_free_di(struct gfs2_rgrpd *rgd, struct gfs2_inode *ip)
{
	struct gfs2_sbd *sdp = rgd->rd_sbd;

	rgrp_lock_local(rgd);
	rgblk_free(sdp, rgd, ip->i_no_addr, 1, GFS2_BLKST_FREE);
	if (!rgd->rd_dinodes)
		gfs2_consist_rgrpd(rgd);
	rgd->rd_dinodes--;
	rgd->rd_free++;

	gfs2_trans_add_meta(rgd->rd_gl, rgd->rd_bits[0].bi_bh);
	gfs2_rgrp_out(rgd, rgd->rd_bits[0].bi_bh->b_data);
	be32_add_cpu(&rgd->rd_rgl->rl_unlinked, -1);
	rgrp_unlock_local(rgd);

	gfs2_statfs_change(sdp, 0, +1, -1);
	trace_gfs2_block_alloc(ip, rgd, ip->i_no_addr, 1, GFS2_BLKST_FREE);
	gfs2_quota_change(ip, -1, ip->i_inode.i_uid, ip->i_inode.i_gid);
	gfs2_journal_wipe(ip, ip->i_no_addr, 1);
}

 

int gfs2_check_blk_type(struct gfs2_sbd *sdp, u64 no_addr, unsigned int type)
{
	struct gfs2_rgrpd *rgd;
	struct gfs2_holder rgd_gh;
	struct gfs2_rbm rbm;
	int error = -EINVAL;

	rgd = gfs2_blk2rgrpd(sdp, no_addr, 1);
	if (!rgd)
		goto fail;

	error = gfs2_glock_nq_init(rgd->rd_gl, LM_ST_SHARED, 0, &rgd_gh);
	if (error)
		goto fail;

	rbm.rgd = rgd;
	error = gfs2_rbm_from_block(&rbm, no_addr);
	if (!WARN_ON_ONCE(error)) {
		 
		if (gfs2_testbit(&rbm, false) != type)
			error = -ESTALE;
	}

	gfs2_glock_dq_uninit(&rgd_gh);

fail:
	return error;
}

 

void gfs2_rlist_add(struct gfs2_inode *ip, struct gfs2_rgrp_list *rlist,
		    u64 block)
{
	struct gfs2_sbd *sdp = GFS2_SB(&ip->i_inode);
	struct gfs2_rgrpd *rgd;
	struct gfs2_rgrpd **tmp;
	unsigned int new_space;
	unsigned int x;

	if (gfs2_assert_warn(sdp, !rlist->rl_ghs))
		return;

	 

	if (rlist->rl_rgrps) {
		rgd = rlist->rl_rgd[rlist->rl_rgrps - 1];
		if (rgrp_contains_block(rgd, block))
			return;
		rgd = gfs2_blk2rgrpd(sdp, block, 1);
	} else {
		rgd = ip->i_res.rs_rgd;
		if (!rgd || !rgrp_contains_block(rgd, block))
			rgd = gfs2_blk2rgrpd(sdp, block, 1);
	}

	if (!rgd) {
		fs_err(sdp, "rlist_add: no rgrp for block %llu\n",
		       (unsigned long long)block);
		return;
	}

	for (x = 0; x < rlist->rl_rgrps; x++) {
		if (rlist->rl_rgd[x] == rgd) {
			swap(rlist->rl_rgd[x],
			     rlist->rl_rgd[rlist->rl_rgrps - 1]);
			return;
		}
	}

	if (rlist->rl_rgrps == rlist->rl_space) {
		new_space = rlist->rl_space + 10;

		tmp = kcalloc(new_space, sizeof(struct gfs2_rgrpd *),
			      GFP_NOFS | __GFP_NOFAIL);

		if (rlist->rl_rgd) {
			memcpy(tmp, rlist->rl_rgd,
			       rlist->rl_space * sizeof(struct gfs2_rgrpd *));
			kfree(rlist->rl_rgd);
		}

		rlist->rl_space = new_space;
		rlist->rl_rgd = tmp;
	}

	rlist->rl_rgd[rlist->rl_rgrps++] = rgd;
}

 

void gfs2_rlist_alloc(struct gfs2_rgrp_list *rlist,
		      unsigned int state, u16 flags)
{
	unsigned int x;

	rlist->rl_ghs = kmalloc_array(rlist->rl_rgrps,
				      sizeof(struct gfs2_holder),
				      GFP_NOFS | __GFP_NOFAIL);
	for (x = 0; x < rlist->rl_rgrps; x++)
		gfs2_holder_init(rlist->rl_rgd[x]->rd_gl, state, flags,
				 &rlist->rl_ghs[x]);
}

 

void gfs2_rlist_free(struct gfs2_rgrp_list *rlist)
{
	unsigned int x;

	kfree(rlist->rl_rgd);

	if (rlist->rl_ghs) {
		for (x = 0; x < rlist->rl_rgrps; x++)
			gfs2_holder_uninit(&rlist->rl_ghs[x]);
		kfree(rlist->rl_ghs);
		rlist->rl_ghs = NULL;
	}
}

void rgrp_lock_local(struct gfs2_rgrpd *rgd)
{
	mutex_lock(&rgd->rd_mutex);
}

void rgrp_unlock_local(struct gfs2_rgrpd *rgd)
{
	mutex_unlock(&rgd->rd_mutex);
}
