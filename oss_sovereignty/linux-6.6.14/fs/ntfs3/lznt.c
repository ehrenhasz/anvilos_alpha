
 

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/types.h>

#include "debug.h"
#include "ntfs_fs.h"


 
#define LZNT_ERROR_ALL_ZEROS	1
#define LZNT_CHUNK_SIZE		0x1000


struct lznt_hash {
	const u8 *p1;
	const u8 *p2;
};

struct lznt {
	const u8 *unc;
	const u8 *unc_end;
	const u8 *best_match;
	size_t max_len;
	bool std;

	struct lznt_hash hash[LZNT_CHUNK_SIZE];
};

static inline size_t get_match_len(const u8 *ptr, const u8 *end, const u8 *prev,
				   size_t max_len)
{
	size_t len = 0;

	while (ptr + len < end && ptr[len] == prev[len] && ++len < max_len)
		;
	return len;
}

static size_t longest_match_std(const u8 *src, struct lznt *ctx)
{
	size_t hash_index;
	size_t len1 = 0, len2 = 0;
	const u8 **hash;

	hash_index =
		((40543U * ((((src[0] << 4) ^ src[1]) << 4) ^ src[2])) >> 4) &
		(LZNT_CHUNK_SIZE - 1);

	hash = &(ctx->hash[hash_index].p1);

	if (hash[0] >= ctx->unc && hash[0] < src && hash[0][0] == src[0] &&
	    hash[0][1] == src[1] && hash[0][2] == src[2]) {
		len1 = 3;
		if (ctx->max_len > 3)
			len1 += get_match_len(src + 3, ctx->unc_end,
					      hash[0] + 3, ctx->max_len - 3);
	}

	if (hash[1] >= ctx->unc && hash[1] < src && hash[1][0] == src[0] &&
	    hash[1][1] == src[1] && hash[1][2] == src[2]) {
		len2 = 3;
		if (ctx->max_len > 3)
			len2 += get_match_len(src + 3, ctx->unc_end,
					      hash[1] + 3, ctx->max_len - 3);
	}

	 
	if (len1 < len2) {
		ctx->best_match = hash[1];
		len1 = len2;
	} else {
		ctx->best_match = hash[0];
	}

	hash[1] = hash[0];
	hash[0] = src;
	return len1;
}

static size_t longest_match_best(const u8 *src, struct lznt *ctx)
{
	size_t max_len;
	const u8 *ptr;

	if (ctx->unc >= src || !ctx->max_len)
		return 0;

	max_len = 0;
	for (ptr = ctx->unc; ptr < src; ++ptr) {
		size_t len =
			get_match_len(src, ctx->unc_end, ptr, ctx->max_len);
		if (len >= max_len) {
			max_len = len;
			ctx->best_match = ptr;
		}
	}

	return max_len >= 3 ? max_len : 0;
}

static const size_t s_max_len[] = {
	0x1002, 0x802, 0x402, 0x202, 0x102, 0x82, 0x42, 0x22, 0x12,
};

static const size_t s_max_off[] = {
	0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000,
};

static inline u16 make_pair(size_t offset, size_t len, size_t index)
{
	return ((offset - 1) << (12 - index)) |
	       ((len - 3) & (((1 << (12 - index)) - 1)));
}

static inline size_t parse_pair(u16 pair, size_t *offset, size_t index)
{
	*offset = 1 + (pair >> (12 - index));
	return 3 + (pair & ((1 << (12 - index)) - 1));
}

 
static inline int compress_chunk(size_t (*match)(const u8 *, struct lznt *),
				 const u8 *unc, const u8 *unc_end, u8 *cmpr,
				 u8 *cmpr_end, size_t *cmpr_chunk_size,
				 struct lznt *ctx)
{
	size_t cnt = 0;
	size_t idx = 0;
	const u8 *up = unc;
	u8 *cp = cmpr + 3;
	u8 *cp2 = cmpr + 2;
	u8 not_zero = 0;
	 
	u8 ohdr = 0;
	u8 *last;
	u16 t16;

	if (unc + LZNT_CHUNK_SIZE < unc_end)
		unc_end = unc + LZNT_CHUNK_SIZE;

	last = min(cmpr + LZNT_CHUNK_SIZE + sizeof(short), cmpr_end);

	ctx->unc = unc;
	ctx->unc_end = unc_end;
	ctx->max_len = s_max_len[0];

	while (up < unc_end) {
		size_t max_len;

		while (unc + s_max_off[idx] < up)
			ctx->max_len = s_max_len[++idx];

		 
		max_len = up + 3 <= unc_end ? (*match)(up, ctx) : 0;

		if (!max_len) {
			if (cp >= last)
				goto NotCompressed;
			not_zero |= *cp++ = *up++;
		} else if (cp + 1 >= last) {
			goto NotCompressed;
		} else {
			t16 = make_pair(up - ctx->best_match, max_len, idx);
			*cp++ = t16;
			*cp++ = t16 >> 8;

			ohdr |= 1 << cnt;
			up += max_len;
		}

		cnt = (cnt + 1) & 7;
		if (!cnt) {
			*cp2 = ohdr;
			ohdr = 0;
			cp2 = cp;
			cp += 1;
		}
	}

	if (cp2 < last)
		*cp2 = ohdr;
	else
		cp -= 1;

	*cmpr_chunk_size = cp - cmpr;

	t16 = (*cmpr_chunk_size - 3) | 0xB000;
	cmpr[0] = t16;
	cmpr[1] = t16 >> 8;

	return not_zero ? 0 : LZNT_ERROR_ALL_ZEROS;

NotCompressed:

	if ((cmpr + LZNT_CHUNK_SIZE + sizeof(short)) > last)
		return -2;

	 
	cmpr[0] = 0xff;
	cmpr[1] = 0x3f;

	memcpy(cmpr + sizeof(short), unc, LZNT_CHUNK_SIZE);
	*cmpr_chunk_size = LZNT_CHUNK_SIZE + sizeof(short);

	return 0;
}

static inline ssize_t decompress_chunk(u8 *unc, u8 *unc_end, const u8 *cmpr,
				       const u8 *cmpr_end)
{
	u8 *up = unc;
	u8 ch = *cmpr++;
	size_t bit = 0;
	size_t index = 0;
	u16 pair;
	size_t offset, length;

	 
	while (up < unc_end && cmpr < cmpr_end) {
		 
		while (unc + s_max_off[index] < up)
			index += 1;

		 
		if (!(ch & (1 << bit))) {
			 
			*up++ = *cmpr++;
			goto next;
		}

		 
		if (cmpr + 1 >= cmpr_end)
			return -EINVAL;

		 
		pair = cmpr[1];
		pair <<= 8;
		pair |= cmpr[0];

		cmpr += 2;

		 
		length = parse_pair(pair, &offset, index);

		 
		if (unc + offset > up)
			return -EINVAL;

		 
		if (up + length >= unc_end)
			length = unc_end - up;

		 
		for (; length > 0; length--, up++)
			*up = *(up - offset);

next:
		 
		bit = (bit + 1) & 7;

		if (!bit) {
			if (cmpr >= cmpr_end)
				break;

			ch = *cmpr++;
		}
	}

	 
	return up - unc;
}

 
struct lznt *get_lznt_ctx(int level)
{
	struct lznt *r = kzalloc(level ? offsetof(struct lznt, hash) :
					 sizeof(struct lznt),
				 GFP_NOFS);

	if (r)
		r->std = !level;
	return r;
}

 
size_t compress_lznt(const void *unc, size_t unc_size, void *cmpr,
		     size_t cmpr_size, struct lznt *ctx)
{
	int err;
	size_t (*match)(const u8 *src, struct lznt *ctx);
	u8 *p = cmpr;
	u8 *end = p + cmpr_size;
	const u8 *unc_chunk = unc;
	const u8 *unc_end = unc_chunk + unc_size;
	bool is_zero = true;

	if (ctx->std) {
		match = &longest_match_std;
		memset(ctx->hash, 0, sizeof(ctx->hash));
	} else {
		match = &longest_match_best;
	}

	 
	for (; unc_chunk < unc_end; unc_chunk += LZNT_CHUNK_SIZE) {
		cmpr_size = 0;
		err = compress_chunk(match, unc_chunk, unc_end, p, end,
				     &cmpr_size, ctx);
		if (err < 0)
			return unc_size;

		if (is_zero && err != LZNT_ERROR_ALL_ZEROS)
			is_zero = false;

		p += cmpr_size;
	}

	if (p <= end - 2)
		p[0] = p[1] = 0;

	return is_zero ? 0 : PtrOffset(cmpr, p);
}

 
ssize_t decompress_lznt(const void *cmpr, size_t cmpr_size, void *unc,
			size_t unc_size)
{
	const u8 *cmpr_chunk = cmpr;
	const u8 *cmpr_end = cmpr_chunk + cmpr_size;
	u8 *unc_chunk = unc;
	u8 *unc_end = unc_chunk + unc_size;
	u16 chunk_hdr;

	if (cmpr_size < sizeof(short))
		return -EINVAL;

	 
	chunk_hdr = cmpr_chunk[1];
	chunk_hdr <<= 8;
	chunk_hdr |= cmpr_chunk[0];

	 
	for (;;) {
		size_t chunk_size_saved;
		size_t unc_use;
		size_t cmpr_use = 3 + (chunk_hdr & (LZNT_CHUNK_SIZE - 1));

		 
		if (cmpr_chunk + cmpr_use > cmpr_end)
			return -EINVAL;

		 
		if (chunk_hdr & 0x8000) {
			 
			ssize_t err =
				decompress_chunk(unc_chunk, unc_end,
						 cmpr_chunk + sizeof(chunk_hdr),
						 cmpr_chunk + cmpr_use);
			if (err < 0)
				return err;
			unc_use = err;
		} else {
			 
			unc_use = unc_chunk + LZNT_CHUNK_SIZE > unc_end ?
					  unc_end - unc_chunk :
					  LZNT_CHUNK_SIZE;

			if (cmpr_chunk + sizeof(chunk_hdr) + unc_use >
			    cmpr_end) {
				return -EINVAL;
			}

			memcpy(unc_chunk, cmpr_chunk + sizeof(chunk_hdr),
			       unc_use);
		}

		 
		cmpr_chunk += cmpr_use;
		unc_chunk += unc_use;

		 
		if (unc_chunk >= unc_end)
			break;

		 
		if (cmpr_chunk > cmpr_end - 2)
			break;

		chunk_size_saved = LZNT_CHUNK_SIZE;

		 
		chunk_hdr = cmpr_chunk[1];
		chunk_hdr <<= 8;
		chunk_hdr |= cmpr_chunk[0];

		if (!chunk_hdr)
			break;

		 
		if (unc_use < chunk_size_saved) {
			size_t t1 = chunk_size_saved - unc_use;
			u8 *t2 = unc_chunk + t1;

			 
			if (t2 >= unc_end)
				break;

			memset(unc_chunk, 0, t1);
			unc_chunk = t2;
		}
	}

	 
	if (cmpr_chunk > cmpr_end)
		return -EINVAL;

	 
	return PtrOffset(unc, unc_chunk);
}
