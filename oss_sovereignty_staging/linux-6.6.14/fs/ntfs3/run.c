
 

#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/log2.h>

#include "debug.h"
#include "ntfs.h"
#include "ntfs_fs.h"

 
#define NTFS3_RUN_MAX_BYTES 0x10000

struct ntfs_run {
	CLST vcn;  
	CLST len;  
	CLST lcn;  
};

 
static bool run_lookup(const struct runs_tree *run, CLST vcn, size_t *index)
{
	size_t min_idx, max_idx, mid_idx;
	struct ntfs_run *r;

	if (!run->count) {
		*index = 0;
		return false;
	}

	min_idx = 0;
	max_idx = run->count - 1;

	 
	r = run->runs;
	if (vcn < r->vcn) {
		*index = 0;
		return false;
	}

	if (vcn < r->vcn + r->len) {
		*index = 0;
		return true;
	}

	r += max_idx;
	if (vcn >= r->vcn + r->len) {
		*index = run->count;
		return false;
	}

	if (vcn >= r->vcn) {
		*index = max_idx;
		return true;
	}

	do {
		mid_idx = min_idx + ((max_idx - min_idx) >> 1);
		r = run->runs + mid_idx;

		if (vcn < r->vcn) {
			max_idx = mid_idx - 1;
			if (!mid_idx)
				break;
		} else if (vcn >= r->vcn + r->len) {
			min_idx = mid_idx + 1;
		} else {
			*index = mid_idx;
			return true;
		}
	} while (min_idx <= max_idx);

	*index = max_idx + 1;
	return false;
}

 
static void run_consolidate(struct runs_tree *run, size_t index)
{
	size_t i;
	struct ntfs_run *r = run->runs + index;

	while (index + 1 < run->count) {
		 
		struct ntfs_run *n = r + 1;
		CLST end = r->vcn + r->len;
		CLST dl;

		 
		if (n->vcn > end)
			break;

		dl = end - n->vcn;

		 
		if (dl > 0) {
			if (n->len <= dl)
				goto remove_next_range;

			n->len -= dl;
			n->vcn += dl;
			if (n->lcn != SPARSE_LCN)
				n->lcn += dl;
			dl = 0;
		}

		 
		if ((n->lcn == SPARSE_LCN) != (r->lcn == SPARSE_LCN)) {
			index += 1;
			r = n;
			continue;
		}

		 
		if (n->lcn != SPARSE_LCN && n->lcn != r->lcn + r->len)
			break;

		 
		r->len += n->len - dl;

remove_next_range:
		i = run->count - (index + 1);
		if (i > 1)
			memmove(n, n + 1, sizeof(*n) * (i - 1));

		run->count -= 1;
	}
}

 
bool run_is_mapped_full(const struct runs_tree *run, CLST svcn, CLST evcn)
{
	size_t i;
	const struct ntfs_run *r, *end;
	CLST next_vcn;

	if (!run_lookup(run, svcn, &i))
		return false;

	end = run->runs + run->count;
	r = run->runs + i;

	for (;;) {
		next_vcn = r->vcn + r->len;
		if (next_vcn > evcn)
			return true;

		if (++r >= end)
			return false;

		if (r->vcn != next_vcn)
			return false;
	}
}

bool run_lookup_entry(const struct runs_tree *run, CLST vcn, CLST *lcn,
		      CLST *len, size_t *index)
{
	size_t idx;
	CLST gap;
	struct ntfs_run *r;

	 
	if (!run->runs)
		return false;

	if (!run_lookup(run, vcn, &idx))
		return false;

	r = run->runs + idx;

	if (vcn >= r->vcn + r->len)
		return false;

	gap = vcn - r->vcn;
	if (r->len <= gap)
		return false;

	*lcn = r->lcn == SPARSE_LCN ? SPARSE_LCN : (r->lcn + gap);

	if (len)
		*len = r->len - gap;
	if (index)
		*index = idx;

	return true;
}

 
void run_truncate_head(struct runs_tree *run, CLST vcn)
{
	size_t index;
	struct ntfs_run *r;

	if (run_lookup(run, vcn, &index)) {
		r = run->runs + index;

		if (vcn > r->vcn) {
			CLST dlen = vcn - r->vcn;

			r->vcn = vcn;
			r->len -= dlen;
			if (r->lcn != SPARSE_LCN)
				r->lcn += dlen;
		}

		if (!index)
			return;
	}
	r = run->runs;
	memmove(r, r + index, sizeof(*r) * (run->count - index));

	run->count -= index;

	if (!run->count) {
		kvfree(run->runs);
		run->runs = NULL;
		run->allocated = 0;
	}
}

 
void run_truncate(struct runs_tree *run, CLST vcn)
{
	size_t index;

	 
	if (run_lookup(run, vcn, &index)) {
		struct ntfs_run *r = run->runs + index;

		r->len = vcn - r->vcn;

		if (r->len > 0)
			index += 1;
	}

	 
	run->count = index;

	 
	if (!index) {
		kvfree(run->runs);
		run->runs = NULL;
		run->allocated = 0;
	}
}

 
void run_truncate_around(struct runs_tree *run, CLST vcn)
{
	run_truncate_head(run, vcn);

	if (run->count >= NTFS3_RUN_MAX_BYTES / sizeof(struct ntfs_run) / 2)
		run_truncate(run, (run->runs + (run->count >> 1))->vcn);
}

 
bool run_add_entry(struct runs_tree *run, CLST vcn, CLST lcn, CLST len,
		   bool is_mft)
{
	size_t used, index;
	struct ntfs_run *r;
	bool inrange;
	CLST tail_vcn = 0, tail_len = 0, tail_lcn = 0;
	bool should_add_tail = false;

	 
	inrange = run_lookup(run, vcn, &index);

	 
	if (!inrange && index > 0) {
		struct ntfs_run *t = run->runs + index - 1;

		if (t->vcn + t->len == vcn &&
		    (t->lcn == SPARSE_LCN) == (lcn == SPARSE_LCN) &&
		    (lcn == SPARSE_LCN || lcn == t->lcn + t->len)) {
			inrange = true;
			index -= 1;
		}
	}

	 
	if (!inrange) {
requires_new_range:
		 
		used = run->count * sizeof(struct ntfs_run);

		 
		if (run->allocated < used + sizeof(struct ntfs_run)) {
			size_t bytes;
			struct ntfs_run *new_ptr;

			 
			if (!used) {
				bytes = 64;
			} else if (used <= 16 * PAGE_SIZE) {
				if (is_power_of_2(run->allocated))
					bytes = run->allocated << 1;
				else
					bytes = (size_t)1
						<< (2 + blksize_bits(used));
			} else {
				bytes = run->allocated + (16 * PAGE_SIZE);
			}

			WARN_ON(!is_mft && bytes > NTFS3_RUN_MAX_BYTES);

			new_ptr = kvmalloc(bytes, GFP_KERNEL);

			if (!new_ptr)
				return false;

			r = new_ptr + index;
			memcpy(new_ptr, run->runs,
			       index * sizeof(struct ntfs_run));
			memcpy(r + 1, run->runs + index,
			       sizeof(struct ntfs_run) * (run->count - index));

			kvfree(run->runs);
			run->runs = new_ptr;
			run->allocated = bytes;

		} else {
			size_t i = run->count - index;

			r = run->runs + index;

			 
			if (i > 0)
				memmove(r + 1, r, sizeof(struct ntfs_run) * i);
		}

		r->vcn = vcn;
		r->lcn = lcn;
		r->len = len;
		run->count += 1;
	} else {
		r = run->runs + index;

		 
		if (((lcn == SPARSE_LCN) != (r->lcn == SPARSE_LCN)) ||
		    (lcn != SPARSE_LCN && lcn != r->lcn + (vcn - r->vcn))) {
			CLST to_eat = vcn - r->vcn;
			CLST Tovcn = to_eat + len;

			should_add_tail = Tovcn < r->len;

			if (should_add_tail) {
				tail_lcn = r->lcn == SPARSE_LCN ?
						   SPARSE_LCN :
						   (r->lcn + Tovcn);
				tail_vcn = r->vcn + Tovcn;
				tail_len = r->len - Tovcn;
			}

			if (to_eat > 0) {
				r->len = to_eat;
				inrange = false;
				index += 1;
				goto requires_new_range;
			}

			 
			r->lcn = lcn;
		}

		 
		if (r->vcn + r->len < vcn + len)
			r->len += len - ((r->vcn + r->len) - vcn);
	}

	 
	if (inrange && index > 0)
		index -= 1;
	run_consolidate(run, index);
	run_consolidate(run, index + 1);

	 
	if (should_add_tail &&
	    !run_add_entry(run, tail_vcn, tail_lcn, tail_len, is_mft))
		return false;

	return true;
}

 
bool run_collapse_range(struct runs_tree *run, CLST vcn, CLST len)
{
	size_t index, eat;
	struct ntfs_run *r, *e, *eat_start, *eat_end;
	CLST end;

	if (WARN_ON(!run_lookup(run, vcn, &index)))
		return true;  

	e = run->runs + run->count;
	r = run->runs + index;
	end = vcn + len;

	if (vcn > r->vcn) {
		if (r->vcn + r->len <= end) {
			 
			r->len = vcn - r->vcn;
		} else if (r->lcn == SPARSE_LCN) {
			 
			r->len -= len;
		} else {
			 
			if (!run_add_entry(run, vcn, SPARSE_LCN, len, false))
				return false;
			return run_collapse_range(run, vcn, len);
		}

		r += 1;
	}

	eat_start = r;
	eat_end = r;

	for (; r < e; r++) {
		CLST d;

		if (r->vcn >= end) {
			r->vcn -= len;
			continue;
		}

		if (r->vcn + r->len <= end) {
			 
			eat_end = r + 1;
			continue;
		}

		d = end - r->vcn;
		if (r->lcn != SPARSE_LCN)
			r->lcn += d;
		r->len -= d;
		r->vcn -= len - d;
	}

	eat = eat_end - eat_start;
	memmove(eat_start, eat_end, (e - eat_end) * sizeof(*r));
	run->count -= eat;

	return true;
}

 
bool run_insert_range(struct runs_tree *run, CLST vcn, CLST len)
{
	size_t index;
	struct ntfs_run *r, *e;

	if (WARN_ON(!run_lookup(run, vcn, &index)))
		return false;  

	e = run->runs + run->count;
	r = run->runs + index;

	if (vcn > r->vcn)
		r += 1;

	for (; r < e; r++)
		r->vcn += len;

	r = run->runs + index;

	if (vcn > r->vcn) {
		 
		CLST len1 = vcn - r->vcn;
		CLST len2 = r->len - len1;
		CLST lcn2 = r->lcn == SPARSE_LCN ? SPARSE_LCN : (r->lcn + len1);

		r->len = len1;

		if (!run_add_entry(run, vcn + len, lcn2, len2, false))
			return false;
	}

	if (!run_add_entry(run, vcn, SPARSE_LCN, len, false))
		return false;

	return true;
}

 
bool run_get_entry(const struct runs_tree *run, size_t index, CLST *vcn,
		   CLST *lcn, CLST *len)
{
	const struct ntfs_run *r;

	if (index >= run->count)
		return false;

	r = run->runs + index;

	if (!r->len)
		return false;

	if (vcn)
		*vcn = r->vcn;
	if (lcn)
		*lcn = r->lcn;
	if (len)
		*len = r->len;
	return true;
}

 
#ifdef __BIG_ENDIAN
static inline int run_packed_size(const s64 n)
{
	const u8 *p = (const u8 *)&n + sizeof(n) - 1;

	if (n >= 0) {
		if (p[-7] || p[-6] || p[-5] || p[-4])
			p -= 4;
		if (p[-3] || p[-2])
			p -= 2;
		if (p[-1])
			p -= 1;
		if (p[0] & 0x80)
			p -= 1;
	} else {
		if (p[-7] != 0xff || p[-6] != 0xff || p[-5] != 0xff ||
		    p[-4] != 0xff)
			p -= 4;
		if (p[-3] != 0xff || p[-2] != 0xff)
			p -= 2;
		if (p[-1] != 0xff)
			p -= 1;
		if (!(p[0] & 0x80))
			p -= 1;
	}
	return (const u8 *)&n + sizeof(n) - p;
}

 
static inline void run_pack_s64(u8 *run_buf, u8 size, s64 v)
{
	const u8 *p = (u8 *)&v;

	switch (size) {
	case 8:
		run_buf[7] = p[0];
		fallthrough;
	case 7:
		run_buf[6] = p[1];
		fallthrough;
	case 6:
		run_buf[5] = p[2];
		fallthrough;
	case 5:
		run_buf[4] = p[3];
		fallthrough;
	case 4:
		run_buf[3] = p[4];
		fallthrough;
	case 3:
		run_buf[2] = p[5];
		fallthrough;
	case 2:
		run_buf[1] = p[6];
		fallthrough;
	case 1:
		run_buf[0] = p[7];
	}
}

 
static inline s64 run_unpack_s64(const u8 *run_buf, u8 size, s64 v)
{
	u8 *p = (u8 *)&v;

	switch (size) {
	case 8:
		p[0] = run_buf[7];
		fallthrough;
	case 7:
		p[1] = run_buf[6];
		fallthrough;
	case 6:
		p[2] = run_buf[5];
		fallthrough;
	case 5:
		p[3] = run_buf[4];
		fallthrough;
	case 4:
		p[4] = run_buf[3];
		fallthrough;
	case 3:
		p[5] = run_buf[2];
		fallthrough;
	case 2:
		p[6] = run_buf[1];
		fallthrough;
	case 1:
		p[7] = run_buf[0];
	}
	return v;
}

#else

static inline int run_packed_size(const s64 n)
{
	const u8 *p = (const u8 *)&n;

	if (n >= 0) {
		if (p[7] || p[6] || p[5] || p[4])
			p += 4;
		if (p[3] || p[2])
			p += 2;
		if (p[1])
			p += 1;
		if (p[0] & 0x80)
			p += 1;
	} else {
		if (p[7] != 0xff || p[6] != 0xff || p[5] != 0xff ||
		    p[4] != 0xff)
			p += 4;
		if (p[3] != 0xff || p[2] != 0xff)
			p += 2;
		if (p[1] != 0xff)
			p += 1;
		if (!(p[0] & 0x80))
			p += 1;
	}

	return 1 + p - (const u8 *)&n;
}

 
static inline void run_pack_s64(u8 *run_buf, u8 size, s64 v)
{
	const u8 *p = (u8 *)&v;

	 
	switch (size) {
	case 8:
		run_buf[7] = p[7];
		fallthrough;
	case 7:
		run_buf[6] = p[6];
		fallthrough;
	case 6:
		run_buf[5] = p[5];
		fallthrough;
	case 5:
		run_buf[4] = p[4];
		fallthrough;
	case 4:
		run_buf[3] = p[3];
		fallthrough;
	case 3:
		run_buf[2] = p[2];
		fallthrough;
	case 2:
		run_buf[1] = p[1];
		fallthrough;
	case 1:
		run_buf[0] = p[0];
	}
}

 
static inline s64 run_unpack_s64(const u8 *run_buf, u8 size, s64 v)
{
	u8 *p = (u8 *)&v;

	 
	switch (size) {
	case 8:
		p[7] = run_buf[7];
		fallthrough;
	case 7:
		p[6] = run_buf[6];
		fallthrough;
	case 6:
		p[5] = run_buf[5];
		fallthrough;
	case 5:
		p[4] = run_buf[4];
		fallthrough;
	case 4:
		p[3] = run_buf[3];
		fallthrough;
	case 3:
		p[2] = run_buf[2];
		fallthrough;
	case 2:
		p[1] = run_buf[1];
		fallthrough;
	case 1:
		p[0] = run_buf[0];
	}
	return v;
}
#endif

 
int run_pack(const struct runs_tree *run, CLST svcn, CLST len, u8 *run_buf,
	     u32 run_buf_size, CLST *packed_vcns)
{
	CLST next_vcn, vcn, lcn;
	CLST prev_lcn = 0;
	CLST evcn1 = svcn + len;
	const struct ntfs_run *r, *r_end;
	int packed_size = 0;
	size_t i;
	s64 dlcn;
	int offset_size, size_size, tmp;

	*packed_vcns = 0;

	if (!len)
		goto out;

	 
	if (!run_lookup(run, svcn, &i))
		return -ENOENT;

	r_end = run->runs + run->count;
	r = run->runs + i;

	for (next_vcn = r->vcn + r->len; next_vcn < evcn1;
	     next_vcn = r->vcn + r->len) {
		if (++r >= r_end || r->vcn != next_vcn)
			return -ENOENT;
	}

	 
	r = run->runs + i;
	len = svcn - r->vcn;
	vcn = svcn;
	lcn = r->lcn == SPARSE_LCN ? SPARSE_LCN : (r->lcn + len);
	len = r->len - len;

	for (;;) {
		next_vcn = vcn + len;
		if (next_vcn > evcn1)
			len = evcn1 - vcn;

		 
		size_size = run_packed_size(len);

		 
		if (lcn == SPARSE_LCN) {
			offset_size = 0;
			dlcn = 0;
		} else {
			 
			dlcn = (s64)lcn - prev_lcn;
			offset_size = run_packed_size(dlcn);
			prev_lcn = lcn;
		}

		tmp = run_buf_size - packed_size - 2 - offset_size;
		if (tmp <= 0)
			goto out;

		 
		if (tmp < size_size)
			goto out;

		if (run_buf) {
			 
			run_buf[0] = ((u8)(size_size | (offset_size << 4)));
			run_buf += 1;

			 
			run_pack_s64(run_buf, size_size, len);

			run_buf += size_size;
			 
			run_pack_s64(run_buf, offset_size, dlcn);
			run_buf += offset_size;
		}

		packed_size += 1 + offset_size + size_size;
		*packed_vcns += len;

		if (packed_size + 1 >= run_buf_size || next_vcn >= evcn1)
			goto out;

		r += 1;
		vcn = r->vcn;
		lcn = r->lcn;
		len = r->len;
	}

out:
	 
	if (run_buf)
		run_buf[0] = 0;

	return packed_size + 1;
}

 
int run_unpack(struct runs_tree *run, struct ntfs_sb_info *sbi, CLST ino,
	       CLST svcn, CLST evcn, CLST vcn, const u8 *run_buf,
	       int run_buf_size)
{
	u64 prev_lcn, vcn64, lcn, next_vcn;
	const u8 *run_last, *run_0;
	bool is_mft = ino == MFT_REC_MFT;

	if (run_buf_size < 0)
		return -EINVAL;

	 
	if (evcn + 1 == svcn)
		return 0;

	if (evcn < svcn)
		return -EINVAL;

	run_0 = run_buf;
	run_last = run_buf + run_buf_size;
	prev_lcn = 0;
	vcn64 = svcn;

	 
	 
	while (run_buf < run_last) {
		 
		u8 size_size = *run_buf & 0xF;
		 
		u8 offset_size = *run_buf++ >> 4;
		u64 len;

		if (!size_size)
			break;

		 
		if (size_size > 8)
			return -EINVAL;

		len = run_unpack_s64(run_buf, size_size, 0);
		 
		run_buf += size_size;

		if (!len)
			return -EINVAL;

		if (!offset_size)
			lcn = SPARSE_LCN64;
		else if (offset_size <= 8) {
			s64 dlcn;

			 
			dlcn = (run_buf[offset_size - 1] & 0x80) ? (s64)-1 : 0;
			dlcn = run_unpack_s64(run_buf, offset_size, dlcn);
			 
			run_buf += offset_size;

			if (!dlcn)
				return -EINVAL;
			lcn = prev_lcn + dlcn;
			prev_lcn = lcn;
		} else
			return -EINVAL;

		next_vcn = vcn64 + len;
		 
		if (next_vcn > evcn + 1)
			return -EINVAL;

#ifndef CONFIG_NTFS3_64BIT_CLUSTER
		if (next_vcn > 0x100000000ull || (lcn + len) > 0x100000000ull) {
			ntfs_err(
				sbi->sb,
				"This driver is compiled without CONFIG_NTFS3_64BIT_CLUSTER (like windows driver).\n"
				"Volume contains 64 bits run: vcn %llx, lcn %llx, len %llx.\n"
				"Activate CONFIG_NTFS3_64BIT_CLUSTER to process this case",
				vcn64, lcn, len);
			return -EOPNOTSUPP;
		}
#endif
		if (lcn != SPARSE_LCN64 && lcn + len > sbi->used.bitmap.nbits) {
			 
			return -EINVAL;
		}

		if (!run)
			;  
		else if (run == RUN_DEALLOCATE) {
			 
			if (lcn != SPARSE_LCN64)
				mark_as_free_ex(sbi, lcn, len, true);
		} else if (vcn64 >= vcn) {
			if (!run_add_entry(run, vcn64, lcn, len, is_mft))
				return -ENOMEM;
		} else if (next_vcn > vcn) {
			u64 dlen = vcn - vcn64;

			if (!run_add_entry(run, vcn, lcn + dlen, len - dlen,
					   is_mft))
				return -ENOMEM;
		}

		vcn64 = next_vcn;
	}

	if (vcn64 != evcn + 1) {
		 
		return -EINVAL;
	}

	return run_buf - run_0;
}

#ifdef NTFS3_CHECK_FREE_CLST
 
int run_unpack_ex(struct runs_tree *run, struct ntfs_sb_info *sbi, CLST ino,
		  CLST svcn, CLST evcn, CLST vcn, const u8 *run_buf,
		  int run_buf_size)
{
	int ret, err;
	CLST next_vcn, lcn, len;
	size_t index;
	bool ok;
	struct wnd_bitmap *wnd;

	ret = run_unpack(run, sbi, ino, svcn, evcn, vcn, run_buf, run_buf_size);
	if (ret <= 0)
		return ret;

	if (!sbi->used.bitmap.sb || !run || run == RUN_DEALLOCATE)
		return ret;

	if (ino == MFT_REC_BADCLUST)
		return ret;

	next_vcn = vcn = svcn;
	wnd = &sbi->used.bitmap;

	for (ok = run_lookup_entry(run, vcn, &lcn, &len, &index);
	     next_vcn <= evcn;
	     ok = run_get_entry(run, ++index, &vcn, &lcn, &len)) {
		if (!ok || next_vcn != vcn)
			return -EINVAL;

		next_vcn = vcn + len;

		if (lcn == SPARSE_LCN)
			continue;

		if (sbi->flags & NTFS_FLAGS_NEED_REPLAY)
			continue;

		down_read_nested(&wnd->rw_lock, BITMAP_MUTEX_CLUSTERS);
		 
		ok = wnd_is_used(wnd, lcn, len);
		up_read(&wnd->rw_lock);
		if (ok)
			continue;

		 
		ntfs_set_state(sbi, NTFS_DIRTY_ERROR);

		if (down_write_trylock(&wnd->rw_lock)) {
			 
			size_t done;
			err = wnd_set_used_safe(wnd, lcn, len, &done);
			up_write(&wnd->rw_lock);
			if (err)
				return err;
		}
	}

	return ret;
}
#endif

 
int run_get_highest_vcn(CLST vcn, const u8 *run_buf, u64 *highest_vcn)
{
	u64 vcn64 = vcn;
	u8 size_size;

	while ((size_size = *run_buf & 0xF)) {
		u8 offset_size = *run_buf++ >> 4;
		u64 len;

		if (size_size > 8 || offset_size > 8)
			return -EINVAL;

		len = run_unpack_s64(run_buf, size_size, 0);
		if (!len)
			return -EINVAL;

		run_buf += size_size + offset_size;
		vcn64 += len;

#ifndef CONFIG_NTFS3_64BIT_CLUSTER
		if (vcn64 > 0x100000000ull)
			return -EINVAL;
#endif
	}

	*highest_vcn = vcn64 - 1;
	return 0;
}

 
int run_clone(const struct runs_tree *run, struct runs_tree *new_run)
{
	size_t bytes = run->count * sizeof(struct ntfs_run);

	if (bytes > new_run->allocated) {
		struct ntfs_run *new_ptr = kvmalloc(bytes, GFP_KERNEL);

		if (!new_ptr)
			return -ENOMEM;

		kvfree(new_run->runs);
		new_run->runs = new_ptr;
		new_run->allocated = bytes;
	}

	memcpy(new_run->runs, run->runs, bytes);
	new_run->count = run->count;
	return 0;
}
