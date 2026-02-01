

 

 

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/netfilter.h>
#include <linux/netfilter/nf_tables.h>
#include <net/netfilter/nf_tables_core.h>
#include <uapi/linux/netfilter/nf_tables.h>
#include <linux/bitmap.h>
#include <linux/bitops.h>

#include "nft_set_pipapo_avx2.h"
#include "nft_set_pipapo.h"

 
static DEFINE_PER_CPU(bool, nft_pipapo_scratch_index);

 
int pipapo_refill(unsigned long *map, int len, int rules, unsigned long *dst,
		  union nft_pipapo_map_bucket *mt, bool match_only)
{
	unsigned long bitset;
	int k, ret = -1;

	for (k = 0; k < len; k++) {
		bitset = map[k];
		while (bitset) {
			unsigned long t = bitset & -bitset;
			int r = __builtin_ctzl(bitset);
			int i = k * BITS_PER_LONG + r;

			if (unlikely(i >= rules)) {
				map[k] = 0;
				return -1;
			}

			if (match_only) {
				bitmap_clear(map, i, 1);
				return i;
			}

			ret = 0;

			bitmap_set(dst, mt[i].to, mt[i].n);

			bitset ^= t;
		}
		map[k] = 0;
	}

	return ret;
}

 
bool nft_pipapo_lookup(const struct net *net, const struct nft_set *set,
		       const u32 *key, const struct nft_set_ext **ext)
{
	struct nft_pipapo *priv = nft_set_priv(set);
	unsigned long *res_map, *fill_map;
	u8 genmask = nft_genmask_cur(net);
	const u8 *rp = (const u8 *)key;
	struct nft_pipapo_match *m;
	struct nft_pipapo_field *f;
	bool map_index;
	int i;

	local_bh_disable();

	map_index = raw_cpu_read(nft_pipapo_scratch_index);

	m = rcu_dereference(priv->match);

	if (unlikely(!m || !*raw_cpu_ptr(m->scratch)))
		goto out;

	res_map  = *raw_cpu_ptr(m->scratch) + (map_index ? m->bsize_max : 0);
	fill_map = *raw_cpu_ptr(m->scratch) + (map_index ? 0 : m->bsize_max);

	memset(res_map, 0xff, m->bsize_max * sizeof(*res_map));

	nft_pipapo_for_each_field(f, i, m) {
		bool last = i == m->field_count - 1;
		int b;

		 
		if (likely(f->bb == 8))
			pipapo_and_field_buckets_8bit(f, res_map, rp);
		else
			pipapo_and_field_buckets_4bit(f, res_map, rp);
		NFT_PIPAPO_GROUP_BITS_ARE_8_OR_4;

		rp += f->groups / NFT_PIPAPO_GROUPS_PER_BYTE(f);

		 
next_match:
		b = pipapo_refill(res_map, f->bsize, f->rules, fill_map, f->mt,
				  last);
		if (b < 0) {
			raw_cpu_write(nft_pipapo_scratch_index, map_index);
			local_bh_enable();

			return false;
		}

		if (last) {
			*ext = &f->mt[b].e->ext;
			if (unlikely(nft_set_elem_expired(*ext) ||
				     !nft_set_elem_active(*ext, genmask)))
				goto next_match;

			 
			raw_cpu_write(nft_pipapo_scratch_index, map_index);
			local_bh_enable();

			return true;
		}

		 
		map_index = !map_index;
		swap(res_map, fill_map);

		rp += NFT_PIPAPO_GROUPS_PADDING(f);
	}

out:
	local_bh_enable();
	return false;
}

 
static struct nft_pipapo_elem *pipapo_get(const struct net *net,
					  const struct nft_set *set,
					  const u8 *data, u8 genmask)
{
	struct nft_pipapo_elem *ret = ERR_PTR(-ENOENT);
	struct nft_pipapo *priv = nft_set_priv(set);
	struct nft_pipapo_match *m = priv->clone;
	unsigned long *res_map, *fill_map = NULL;
	struct nft_pipapo_field *f;
	int i;

	res_map = kmalloc_array(m->bsize_max, sizeof(*res_map), GFP_ATOMIC);
	if (!res_map) {
		ret = ERR_PTR(-ENOMEM);
		goto out;
	}

	fill_map = kcalloc(m->bsize_max, sizeof(*res_map), GFP_ATOMIC);
	if (!fill_map) {
		ret = ERR_PTR(-ENOMEM);
		goto out;
	}

	memset(res_map, 0xff, m->bsize_max * sizeof(*res_map));

	nft_pipapo_for_each_field(f, i, m) {
		bool last = i == m->field_count - 1;
		int b;

		 
		if (f->bb == 8)
			pipapo_and_field_buckets_8bit(f, res_map, data);
		else if (f->bb == 4)
			pipapo_and_field_buckets_4bit(f, res_map, data);
		else
			BUG();

		data += f->groups / NFT_PIPAPO_GROUPS_PER_BYTE(f);

		 
next_match:
		b = pipapo_refill(res_map, f->bsize, f->rules, fill_map, f->mt,
				  last);
		if (b < 0)
			goto out;

		if (last) {
			if (nft_set_elem_expired(&f->mt[b].e->ext))
				goto next_match;
			if ((genmask &&
			     !nft_set_elem_active(&f->mt[b].e->ext, genmask)))
				goto next_match;

			ret = f->mt[b].e;
			goto out;
		}

		data += NFT_PIPAPO_GROUPS_PADDING(f);

		 
		swap(res_map, fill_map);
	}

out:
	kfree(fill_map);
	kfree(res_map);
	return ret;
}

 
static void *nft_pipapo_get(const struct net *net, const struct nft_set *set,
			    const struct nft_set_elem *elem, unsigned int flags)
{
	return pipapo_get(net, set, (const u8 *)elem->key.val.data,
			 nft_genmask_cur(net));
}

 
static int pipapo_resize(struct nft_pipapo_field *f, int old_rules, int rules)
{
	long *new_lt = NULL, *new_p, *old_lt = f->lt, *old_p;
	union nft_pipapo_map_bucket *new_mt, *old_mt = f->mt;
	size_t new_bucket_size, copy;
	int group, bucket;

	new_bucket_size = DIV_ROUND_UP(rules, BITS_PER_LONG);
#ifdef NFT_PIPAPO_ALIGN
	new_bucket_size = roundup(new_bucket_size,
				  NFT_PIPAPO_ALIGN / sizeof(*new_lt));
#endif

	if (new_bucket_size == f->bsize)
		goto mt;

	if (new_bucket_size > f->bsize)
		copy = f->bsize;
	else
		copy = new_bucket_size;

	new_lt = kvzalloc(f->groups * NFT_PIPAPO_BUCKETS(f->bb) *
			  new_bucket_size * sizeof(*new_lt) +
			  NFT_PIPAPO_ALIGN_HEADROOM,
			  GFP_KERNEL);
	if (!new_lt)
		return -ENOMEM;

	new_p = NFT_PIPAPO_LT_ALIGN(new_lt);
	old_p = NFT_PIPAPO_LT_ALIGN(old_lt);

	for (group = 0; group < f->groups; group++) {
		for (bucket = 0; bucket < NFT_PIPAPO_BUCKETS(f->bb); bucket++) {
			memcpy(new_p, old_p, copy * sizeof(*new_p));
			new_p += copy;
			old_p += copy;

			if (new_bucket_size > f->bsize)
				new_p += new_bucket_size - f->bsize;
			else
				old_p += f->bsize - new_bucket_size;
		}
	}

mt:
	new_mt = kvmalloc(rules * sizeof(*new_mt), GFP_KERNEL);
	if (!new_mt) {
		kvfree(new_lt);
		return -ENOMEM;
	}

	memcpy(new_mt, f->mt, min(old_rules, rules) * sizeof(*new_mt));
	if (rules > old_rules) {
		memset(new_mt + old_rules, 0,
		       (rules - old_rules) * sizeof(*new_mt));
	}

	if (new_lt) {
		f->bsize = new_bucket_size;
		NFT_PIPAPO_LT_ASSIGN(f, new_lt);
		kvfree(old_lt);
	}

	f->mt = new_mt;
	kvfree(old_mt);

	return 0;
}

 
static void pipapo_bucket_set(struct nft_pipapo_field *f, int rule, int group,
			      int v)
{
	unsigned long *pos;

	pos = NFT_PIPAPO_LT_ALIGN(f->lt);
	pos += f->bsize * NFT_PIPAPO_BUCKETS(f->bb) * group;
	pos += f->bsize * v;

	__set_bit(rule, pos);
}

 
static void pipapo_lt_4b_to_8b(int old_groups, int bsize,
			       unsigned long *old_lt, unsigned long *new_lt)
{
	int g, b, i;

	for (g = 0; g < old_groups / 2; g++) {
		int src_g0 = g * 2, src_g1 = g * 2 + 1;

		for (b = 0; b < NFT_PIPAPO_BUCKETS(8); b++) {
			int src_b0 = b / NFT_PIPAPO_BUCKETS(4);
			int src_b1 = b % NFT_PIPAPO_BUCKETS(4);
			int src_i0 = src_g0 * NFT_PIPAPO_BUCKETS(4) + src_b0;
			int src_i1 = src_g1 * NFT_PIPAPO_BUCKETS(4) + src_b1;

			for (i = 0; i < bsize; i++) {
				*new_lt = old_lt[src_i0 * bsize + i] &
					  old_lt[src_i1 * bsize + i];
				new_lt++;
			}
		}
	}
}

 
static void pipapo_lt_8b_to_4b(int old_groups, int bsize,
			       unsigned long *old_lt, unsigned long *new_lt)
{
	int g, b, bsrc, i;

	memset(new_lt, 0, old_groups * 2 * NFT_PIPAPO_BUCKETS(4) * bsize *
			  sizeof(unsigned long));

	for (g = 0; g < old_groups * 2; g += 2) {
		int src_g = g / 2;

		for (b = 0; b < NFT_PIPAPO_BUCKETS(4); b++) {
			for (bsrc = NFT_PIPAPO_BUCKETS(8) * src_g;
			     bsrc < NFT_PIPAPO_BUCKETS(8) * (src_g + 1);
			     bsrc++) {
				if (((bsrc & 0xf0) >> 4) != b)
					continue;

				for (i = 0; i < bsize; i++)
					new_lt[i] |= old_lt[bsrc * bsize + i];
			}

			new_lt += bsize;
		}

		for (b = 0; b < NFT_PIPAPO_BUCKETS(4); b++) {
			for (bsrc = NFT_PIPAPO_BUCKETS(8) * src_g;
			     bsrc < NFT_PIPAPO_BUCKETS(8) * (src_g + 1);
			     bsrc++) {
				if ((bsrc & 0x0f) != b)
					continue;

				for (i = 0; i < bsize; i++)
					new_lt[i] |= old_lt[bsrc * bsize + i];
			}

			new_lt += bsize;
		}
	}
}

 
static void pipapo_lt_bits_adjust(struct nft_pipapo_field *f)
{
	unsigned long *new_lt;
	int groups, bb;
	size_t lt_size;

	lt_size = f->groups * NFT_PIPAPO_BUCKETS(f->bb) * f->bsize *
		  sizeof(*f->lt);

	if (f->bb == NFT_PIPAPO_GROUP_BITS_SMALL_SET &&
	    lt_size > NFT_PIPAPO_LT_SIZE_HIGH) {
		groups = f->groups * 2;
		bb = NFT_PIPAPO_GROUP_BITS_LARGE_SET;

		lt_size = groups * NFT_PIPAPO_BUCKETS(bb) * f->bsize *
			  sizeof(*f->lt);
	} else if (f->bb == NFT_PIPAPO_GROUP_BITS_LARGE_SET &&
		   lt_size < NFT_PIPAPO_LT_SIZE_LOW) {
		groups = f->groups / 2;
		bb = NFT_PIPAPO_GROUP_BITS_SMALL_SET;

		lt_size = groups * NFT_PIPAPO_BUCKETS(bb) * f->bsize *
			  sizeof(*f->lt);

		 
		if (lt_size > NFT_PIPAPO_LT_SIZE_HIGH)
			return;
	} else {
		return;
	}

	new_lt = kvzalloc(lt_size + NFT_PIPAPO_ALIGN_HEADROOM, GFP_KERNEL);
	if (!new_lt)
		return;

	NFT_PIPAPO_GROUP_BITS_ARE_8_OR_4;
	if (f->bb == 4 && bb == 8) {
		pipapo_lt_4b_to_8b(f->groups, f->bsize,
				   NFT_PIPAPO_LT_ALIGN(f->lt),
				   NFT_PIPAPO_LT_ALIGN(new_lt));
	} else if (f->bb == 8 && bb == 4) {
		pipapo_lt_8b_to_4b(f->groups, f->bsize,
				   NFT_PIPAPO_LT_ALIGN(f->lt),
				   NFT_PIPAPO_LT_ALIGN(new_lt));
	} else {
		BUG();
	}

	f->groups = groups;
	f->bb = bb;
	kvfree(f->lt);
	NFT_PIPAPO_LT_ASSIGN(f, new_lt);
}

 
static int pipapo_insert(struct nft_pipapo_field *f, const uint8_t *k,
			 int mask_bits)
{
	int rule = f->rules, group, ret, bit_offset = 0;

	ret = pipapo_resize(f, f->rules, f->rules + 1);
	if (ret)
		return ret;

	f->rules++;

	for (group = 0; group < f->groups; group++) {
		int i, v;
		u8 mask;

		v = k[group / (BITS_PER_BYTE / f->bb)];
		v &= GENMASK(BITS_PER_BYTE - bit_offset - 1, 0);
		v >>= (BITS_PER_BYTE - bit_offset) - f->bb;

		bit_offset += f->bb;
		bit_offset %= BITS_PER_BYTE;

		if (mask_bits >= (group + 1) * f->bb) {
			 
			pipapo_bucket_set(f, rule, group, v);
		} else if (mask_bits <= group * f->bb) {
			 
			for (i = 0; i < NFT_PIPAPO_BUCKETS(f->bb); i++)
				pipapo_bucket_set(f, rule, group, i);
		} else {
			 
			mask = GENMASK(f->bb - 1, 0);
			mask >>= mask_bits - group * f->bb;
			for (i = 0; i < NFT_PIPAPO_BUCKETS(f->bb); i++) {
				if ((i & ~mask) == (v & ~mask))
					pipapo_bucket_set(f, rule, group, i);
			}
		}
	}

	pipapo_lt_bits_adjust(f);

	return 1;
}

 
static bool pipapo_step_diff(u8 *base, int step, int len)
{
	 
#ifdef __BIG_ENDIAN__
	return !(BIT(step % BITS_PER_BYTE) & base[step / BITS_PER_BYTE]);
#else
	return !(BIT(step % BITS_PER_BYTE) &
		 base[len - 1 - step / BITS_PER_BYTE]);
#endif
}

 
static bool pipapo_step_after_end(const u8 *base, const u8 *end, int step,
				  int len)
{
	u8 tmp[NFT_PIPAPO_MAX_BYTES];
	int i;

	memcpy(tmp, base, len);

	 
	for (i = 0; i <= step; i++)
#ifdef __BIG_ENDIAN__
		tmp[i / BITS_PER_BYTE] |= BIT(i % BITS_PER_BYTE);
#else
		tmp[len - 1 - i / BITS_PER_BYTE] |= BIT(i % BITS_PER_BYTE);
#endif

	return memcmp(tmp, end, len) > 0;
}

 
static void pipapo_base_sum(u8 *base, int step, int len)
{
	bool carry = false;
	int i;

	 
#ifdef __BIG_ENDIAN__
	for (i = step / BITS_PER_BYTE; i < len; i++) {
#else
	for (i = len - 1 - step / BITS_PER_BYTE; i >= 0; i--) {
#endif
		if (carry)
			base[i]++;
		else
			base[i] += 1 << (step % BITS_PER_BYTE);

		if (base[i])
			break;

		carry = true;
	}
}

 
static int pipapo_expand(struct nft_pipapo_field *f,
			 const u8 *start, const u8 *end, int len)
{
	int step, masks = 0, bytes = DIV_ROUND_UP(len, BITS_PER_BYTE);
	u8 base[NFT_PIPAPO_MAX_BYTES];

	memcpy(base, start, bytes);
	while (memcmp(base, end, bytes) <= 0) {
		int err;

		step = 0;
		while (pipapo_step_diff(base, step, bytes)) {
			if (pipapo_step_after_end(base, end, step, bytes))
				break;

			step++;
			if (step >= len) {
				if (!masks) {
					err = pipapo_insert(f, base, 0);
					if (err < 0)
						return err;
					masks = 1;
				}
				goto out;
			}
		}

		err = pipapo_insert(f, base, len - step);

		if (err < 0)
			return err;

		masks++;
		pipapo_base_sum(base, step, bytes);
	}
out:
	return masks;
}

 
static void pipapo_map(struct nft_pipapo_match *m,
		       union nft_pipapo_map_bucket map[NFT_PIPAPO_MAX_FIELDS],
		       struct nft_pipapo_elem *e)
{
	struct nft_pipapo_field *f;
	int i, j;

	for (i = 0, f = m->f; i < m->field_count - 1; i++, f++) {
		for (j = 0; j < map[i].n; j++) {
			f->mt[map[i].to + j].to = map[i + 1].to;
			f->mt[map[i].to + j].n = map[i + 1].n;
		}
	}

	 
	for (j = 0; j < map[i].n; j++)
		f->mt[map[i].to + j].e = e;
}

 
static int pipapo_realloc_scratch(struct nft_pipapo_match *clone,
				  unsigned long bsize_max)
{
	int i;

	for_each_possible_cpu(i) {
		unsigned long *scratch;
#ifdef NFT_PIPAPO_ALIGN
		unsigned long *scratch_aligned;
#endif

		scratch = kzalloc_node(bsize_max * sizeof(*scratch) * 2 +
				       NFT_PIPAPO_ALIGN_HEADROOM,
				       GFP_KERNEL, cpu_to_node(i));
		if (!scratch) {
			 
			return -ENOMEM;
		}

		kfree(*per_cpu_ptr(clone->scratch, i));

		*per_cpu_ptr(clone->scratch, i) = scratch;

#ifdef NFT_PIPAPO_ALIGN
		scratch_aligned = NFT_PIPAPO_LT_ALIGN(scratch);
		*per_cpu_ptr(clone->scratch_aligned, i) = scratch_aligned;
#endif
	}

	return 0;
}

 
static int nft_pipapo_insert(const struct net *net, const struct nft_set *set,
			     const struct nft_set_elem *elem,
			     struct nft_set_ext **ext2)
{
	const struct nft_set_ext *ext = nft_set_elem_ext(set, elem->priv);
	union nft_pipapo_map_bucket rulemap[NFT_PIPAPO_MAX_FIELDS];
	const u8 *start = (const u8 *)elem->key.val.data, *end;
	struct nft_pipapo_elem *e = elem->priv, *dup;
	struct nft_pipapo *priv = nft_set_priv(set);
	struct nft_pipapo_match *m = priv->clone;
	u8 genmask = nft_genmask_next(net);
	struct nft_pipapo_field *f;
	const u8 *start_p, *end_p;
	int i, bsize_max, err = 0;

	if (nft_set_ext_exists(ext, NFT_SET_EXT_KEY_END))
		end = (const u8 *)nft_set_ext_key_end(ext)->data;
	else
		end = start;

	dup = pipapo_get(net, set, start, genmask);
	if (!IS_ERR(dup)) {
		 
		const struct nft_data *dup_key, *dup_end;

		dup_key = nft_set_ext_key(&dup->ext);
		if (nft_set_ext_exists(&dup->ext, NFT_SET_EXT_KEY_END))
			dup_end = nft_set_ext_key_end(&dup->ext);
		else
			dup_end = dup_key;

		if (!memcmp(start, dup_key->data, sizeof(*dup_key->data)) &&
		    !memcmp(end, dup_end->data, sizeof(*dup_end->data))) {
			*ext2 = &dup->ext;
			return -EEXIST;
		}

		return -ENOTEMPTY;
	}

	if (PTR_ERR(dup) == -ENOENT) {
		 
		dup = pipapo_get(net, set, end, nft_genmask_next(net));
	}

	if (PTR_ERR(dup) != -ENOENT) {
		if (IS_ERR(dup))
			return PTR_ERR(dup);
		*ext2 = &dup->ext;
		return -ENOTEMPTY;
	}

	 
	start_p = start;
	end_p = end;
	nft_pipapo_for_each_field(f, i, m) {
		if (f->rules >= (unsigned long)NFT_PIPAPO_RULE0_MAX)
			return -ENOSPC;

		if (memcmp(start_p, end_p,
			   f->groups / NFT_PIPAPO_GROUPS_PER_BYTE(f)) > 0)
			return -EINVAL;

		start_p += NFT_PIPAPO_GROUPS_PADDED_SIZE(f);
		end_p += NFT_PIPAPO_GROUPS_PADDED_SIZE(f);
	}

	 
	priv->dirty = true;

	bsize_max = m->bsize_max;

	nft_pipapo_for_each_field(f, i, m) {
		int ret;

		rulemap[i].to = f->rules;

		ret = memcmp(start, end,
			     f->groups / NFT_PIPAPO_GROUPS_PER_BYTE(f));
		if (!ret)
			ret = pipapo_insert(f, start, f->groups * f->bb);
		else
			ret = pipapo_expand(f, start, end, f->groups * f->bb);

		if (ret < 0)
			return ret;

		if (f->bsize > bsize_max)
			bsize_max = f->bsize;

		rulemap[i].n = ret;

		start += NFT_PIPAPO_GROUPS_PADDED_SIZE(f);
		end += NFT_PIPAPO_GROUPS_PADDED_SIZE(f);
	}

	if (!*get_cpu_ptr(m->scratch) || bsize_max > m->bsize_max) {
		put_cpu_ptr(m->scratch);

		err = pipapo_realloc_scratch(m, bsize_max);
		if (err)
			return err;

		m->bsize_max = bsize_max;
	} else {
		put_cpu_ptr(m->scratch);
	}

	*ext2 = &e->ext;

	pipapo_map(m, rulemap, e);

	return 0;
}

 
static struct nft_pipapo_match *pipapo_clone(struct nft_pipapo_match *old)
{
	struct nft_pipapo_field *dst, *src;
	struct nft_pipapo_match *new;
	int i;

	new = kmalloc(struct_size(new, f, old->field_count), GFP_KERNEL);
	if (!new)
		return ERR_PTR(-ENOMEM);

	new->field_count = old->field_count;
	new->bsize_max = old->bsize_max;

	new->scratch = alloc_percpu(*new->scratch);
	if (!new->scratch)
		goto out_scratch;

#ifdef NFT_PIPAPO_ALIGN
	new->scratch_aligned = alloc_percpu(*new->scratch_aligned);
	if (!new->scratch_aligned)
		goto out_scratch;
#endif
	for_each_possible_cpu(i)
		*per_cpu_ptr(new->scratch, i) = NULL;

	if (pipapo_realloc_scratch(new, old->bsize_max))
		goto out_scratch_realloc;

	rcu_head_init(&new->rcu);

	src = old->f;
	dst = new->f;

	for (i = 0; i < old->field_count; i++) {
		unsigned long *new_lt;

		memcpy(dst, src, offsetof(struct nft_pipapo_field, lt));

		new_lt = kvzalloc(src->groups * NFT_PIPAPO_BUCKETS(src->bb) *
				  src->bsize * sizeof(*dst->lt) +
				  NFT_PIPAPO_ALIGN_HEADROOM,
				  GFP_KERNEL);
		if (!new_lt)
			goto out_lt;

		NFT_PIPAPO_LT_ASSIGN(dst, new_lt);

		memcpy(NFT_PIPAPO_LT_ALIGN(new_lt),
		       NFT_PIPAPO_LT_ALIGN(src->lt),
		       src->bsize * sizeof(*dst->lt) *
		       src->groups * NFT_PIPAPO_BUCKETS(src->bb));

		dst->mt = kvmalloc(src->rules * sizeof(*src->mt), GFP_KERNEL);
		if (!dst->mt)
			goto out_mt;

		memcpy(dst->mt, src->mt, src->rules * sizeof(*src->mt));
		src++;
		dst++;
	}

	return new;

out_mt:
	kvfree(dst->lt);
out_lt:
	for (dst--; i > 0; i--) {
		kvfree(dst->mt);
		kvfree(dst->lt);
		dst--;
	}
out_scratch_realloc:
	for_each_possible_cpu(i)
		kfree(*per_cpu_ptr(new->scratch, i));
#ifdef NFT_PIPAPO_ALIGN
	free_percpu(new->scratch_aligned);
#endif
out_scratch:
	free_percpu(new->scratch);
	kfree(new);

	return ERR_PTR(-ENOMEM);
}

 
static int pipapo_rules_same_key(struct nft_pipapo_field *f, int first)
{
	struct nft_pipapo_elem *e = NULL;  
	int r;

	for (r = first; r < f->rules; r++) {
		if (r != first && e != f->mt[r].e)
			return r - first;

		e = f->mt[r].e;
	}

	if (r != first)
		return r - first;

	return 0;
}

 
static void pipapo_unmap(union nft_pipapo_map_bucket *mt, int rules,
			 int start, int n, int to_offset, bool is_last)
{
	int i;

	memmove(mt + start, mt + start + n, (rules - start - n) * sizeof(*mt));
	memset(mt + rules - n, 0, n * sizeof(*mt));

	if (is_last)
		return;

	for (i = start; i < rules - n; i++)
		mt[i].to -= to_offset;
}

 
static void pipapo_drop(struct nft_pipapo_match *m,
			union nft_pipapo_map_bucket rulemap[])
{
	struct nft_pipapo_field *f;
	int i;

	nft_pipapo_for_each_field(f, i, m) {
		int g;

		for (g = 0; g < f->groups; g++) {
			unsigned long *pos;
			int b;

			pos = NFT_PIPAPO_LT_ALIGN(f->lt) + g *
			      NFT_PIPAPO_BUCKETS(f->bb) * f->bsize;

			for (b = 0; b < NFT_PIPAPO_BUCKETS(f->bb); b++) {
				bitmap_cut(pos, pos, rulemap[i].to,
					   rulemap[i].n,
					   f->bsize * BITS_PER_LONG);

				pos += f->bsize;
			}
		}

		pipapo_unmap(f->mt, f->rules, rulemap[i].to, rulemap[i].n,
			     rulemap[i + 1].n, i == m->field_count - 1);
		if (pipapo_resize(f, f->rules, f->rules - rulemap[i].n)) {
			 
			;
		}
		f->rules -= rulemap[i].n;

		pipapo_lt_bits_adjust(f);
	}
}

static void nft_pipapo_gc_deactivate(struct net *net, struct nft_set *set,
				     struct nft_pipapo_elem *e)

{
	struct nft_set_elem elem = {
		.priv	= e,
	};

	nft_setelem_data_deactivate(net, set, &elem);
}

 
static void pipapo_gc(const struct nft_set *_set, struct nft_pipapo_match *m)
{
	struct nft_set *set = (struct nft_set *) _set;
	struct nft_pipapo *priv = nft_set_priv(set);
	struct net *net = read_pnet(&set->net);
	int rules_f0, first_rule = 0;
	struct nft_pipapo_elem *e;
	struct nft_trans_gc *gc;

	gc = nft_trans_gc_alloc(set, 0, GFP_KERNEL);
	if (!gc)
		return;

	while ((rules_f0 = pipapo_rules_same_key(m->f, first_rule))) {
		union nft_pipapo_map_bucket rulemap[NFT_PIPAPO_MAX_FIELDS];
		struct nft_pipapo_field *f;
		int i, start, rules_fx;

		start = first_rule;
		rules_fx = rules_f0;

		nft_pipapo_for_each_field(f, i, m) {
			rulemap[i].to = start;
			rulemap[i].n = rules_fx;

			if (i < m->field_count - 1) {
				rules_fx = f->mt[start].n;
				start = f->mt[start].to;
			}
		}

		 
		f--;
		i--;
		e = f->mt[rulemap[i].to].e;

		 
		if (nft_set_elem_expired(&e->ext)) {
			priv->dirty = true;

			gc = nft_trans_gc_queue_sync(gc, GFP_ATOMIC);
			if (!gc)
				return;

			nft_pipapo_gc_deactivate(net, set, e);
			pipapo_drop(m, rulemap);
			nft_trans_gc_elem_add(gc, e);

			 
		} else {
			first_rule += rules_f0;
		}
	}

	gc = nft_trans_gc_catchall_sync(gc);
	if (gc) {
		nft_trans_gc_queue_sync_done(gc);
		priv->last_gc = jiffies;
	}
}

 
static void pipapo_free_fields(struct nft_pipapo_match *m)
{
	struct nft_pipapo_field *f;
	int i;

	nft_pipapo_for_each_field(f, i, m) {
		kvfree(f->lt);
		kvfree(f->mt);
	}
}

static void pipapo_free_match(struct nft_pipapo_match *m)
{
	int i;

	for_each_possible_cpu(i)
		kfree(*per_cpu_ptr(m->scratch, i));

#ifdef NFT_PIPAPO_ALIGN
	free_percpu(m->scratch_aligned);
#endif
	free_percpu(m->scratch);

	pipapo_free_fields(m);

	kfree(m);
}

 
static void pipapo_reclaim_match(struct rcu_head *rcu)
{
	struct nft_pipapo_match *m;

	m = container_of(rcu, struct nft_pipapo_match, rcu);
	pipapo_free_match(m);
}

 
static void nft_pipapo_commit(const struct nft_set *set)
{
	struct nft_pipapo *priv = nft_set_priv(set);
	struct nft_pipapo_match *new_clone, *old;

	if (time_after_eq(jiffies, priv->last_gc + nft_set_gc_interval(set)))
		pipapo_gc(set, priv->clone);

	if (!priv->dirty)
		return;

	new_clone = pipapo_clone(priv->clone);
	if (IS_ERR(new_clone))
		return;

	priv->dirty = false;

	old = rcu_access_pointer(priv->match);
	rcu_assign_pointer(priv->match, priv->clone);
	if (old)
		call_rcu(&old->rcu, pipapo_reclaim_match);

	priv->clone = new_clone;
}

static bool nft_pipapo_transaction_mutex_held(const struct nft_set *set)
{
#ifdef CONFIG_PROVE_LOCKING
	const struct net *net = read_pnet(&set->net);

	return lockdep_is_held(&nft_pernet(net)->commit_mutex);
#else
	return true;
#endif
}

static void nft_pipapo_abort(const struct nft_set *set)
{
	struct nft_pipapo *priv = nft_set_priv(set);
	struct nft_pipapo_match *new_clone, *m;

	if (!priv->dirty)
		return;

	m = rcu_dereference_protected(priv->match, nft_pipapo_transaction_mutex_held(set));

	new_clone = pipapo_clone(m);
	if (IS_ERR(new_clone))
		return;

	priv->dirty = false;

	pipapo_free_match(priv->clone);
	priv->clone = new_clone;
}

 
static void nft_pipapo_activate(const struct net *net,
				const struct nft_set *set,
				const struct nft_set_elem *elem)
{
	struct nft_pipapo_elem *e = elem->priv;

	nft_set_elem_change_active(net, set, &e->ext);
}

 
static void *pipapo_deactivate(const struct net *net, const struct nft_set *set,
			       const u8 *data, const struct nft_set_ext *ext)
{
	struct nft_pipapo_elem *e;

	e = pipapo_get(net, set, data, nft_genmask_next(net));
	if (IS_ERR(e))
		return NULL;

	nft_set_elem_change_active(net, set, &e->ext);

	return e;
}

 
static void *nft_pipapo_deactivate(const struct net *net,
				   const struct nft_set *set,
				   const struct nft_set_elem *elem)
{
	const struct nft_set_ext *ext = nft_set_elem_ext(set, elem->priv);

	return pipapo_deactivate(net, set, (const u8 *)elem->key.val.data, ext);
}

 
static bool nft_pipapo_flush(const struct net *net, const struct nft_set *set,
			     void *elem)
{
	struct nft_pipapo_elem *e = elem;

	return pipapo_deactivate(net, set, (const u8 *)nft_set_ext_key(&e->ext),
				 &e->ext);
}

 
static int pipapo_get_boundaries(struct nft_pipapo_field *f, int first_rule,
				 int rule_count, u8 *left, u8 *right)
{
	int g, mask_len = 0, bit_offset = 0;
	u8 *l = left, *r = right;

	for (g = 0; g < f->groups; g++) {
		int b, x0, x1;

		x0 = -1;
		x1 = -1;
		for (b = 0; b < NFT_PIPAPO_BUCKETS(f->bb); b++) {
			unsigned long *pos;

			pos = NFT_PIPAPO_LT_ALIGN(f->lt) +
			      (g * NFT_PIPAPO_BUCKETS(f->bb) + b) * f->bsize;
			if (test_bit(first_rule, pos) && x0 == -1)
				x0 = b;
			if (test_bit(first_rule + rule_count - 1, pos))
				x1 = b;
		}

		*l |= x0 << (BITS_PER_BYTE - f->bb - bit_offset);
		*r |= x1 << (BITS_PER_BYTE - f->bb - bit_offset);

		bit_offset += f->bb;
		if (bit_offset >= BITS_PER_BYTE) {
			bit_offset %= BITS_PER_BYTE;
			l++;
			r++;
		}

		if (x1 - x0 == 0)
			mask_len += 4;
		else if (x1 - x0 == 1)
			mask_len += 3;
		else if (x1 - x0 == 3)
			mask_len += 2;
		else if (x1 - x0 == 7)
			mask_len += 1;
	}

	return mask_len;
}

 
static bool pipapo_match_field(struct nft_pipapo_field *f,
			       int first_rule, int rule_count,
			       const u8 *start, const u8 *end)
{
	u8 right[NFT_PIPAPO_MAX_BYTES] = { 0 };
	u8 left[NFT_PIPAPO_MAX_BYTES] = { 0 };

	pipapo_get_boundaries(f, first_rule, rule_count, left, right);

	return !memcmp(start, left,
		       f->groups / NFT_PIPAPO_GROUPS_PER_BYTE(f)) &&
	       !memcmp(end, right, f->groups / NFT_PIPAPO_GROUPS_PER_BYTE(f));
}

 
static void nft_pipapo_remove(const struct net *net, const struct nft_set *set,
			      const struct nft_set_elem *elem)
{
	struct nft_pipapo *priv = nft_set_priv(set);
	struct nft_pipapo_match *m = priv->clone;
	struct nft_pipapo_elem *e = elem->priv;
	int rules_f0, first_rule = 0;
	const u8 *data;

	data = (const u8 *)nft_set_ext_key(&e->ext);

	while ((rules_f0 = pipapo_rules_same_key(m->f, first_rule))) {
		union nft_pipapo_map_bucket rulemap[NFT_PIPAPO_MAX_FIELDS];
		const u8 *match_start, *match_end;
		struct nft_pipapo_field *f;
		int i, start, rules_fx;

		match_start = data;

		if (nft_set_ext_exists(&e->ext, NFT_SET_EXT_KEY_END))
			match_end = (const u8 *)nft_set_ext_key_end(&e->ext)->data;
		else
			match_end = data;

		start = first_rule;
		rules_fx = rules_f0;

		nft_pipapo_for_each_field(f, i, m) {
			if (!pipapo_match_field(f, start, rules_fx,
						match_start, match_end))
				break;

			rulemap[i].to = start;
			rulemap[i].n = rules_fx;

			rules_fx = f->mt[start].n;
			start = f->mt[start].to;

			match_start += NFT_PIPAPO_GROUPS_PADDED_SIZE(f);
			match_end += NFT_PIPAPO_GROUPS_PADDED_SIZE(f);
		}

		if (i == m->field_count) {
			priv->dirty = true;
			pipapo_drop(m, rulemap);
			return;
		}

		first_rule += rules_f0;
	}
}

 
static void nft_pipapo_walk(const struct nft_ctx *ctx, struct nft_set *set,
			    struct nft_set_iter *iter)
{
	struct nft_pipapo *priv = nft_set_priv(set);
	struct net *net = read_pnet(&set->net);
	struct nft_pipapo_match *m;
	struct nft_pipapo_field *f;
	int i, r;

	rcu_read_lock();
	if (iter->genmask == nft_genmask_cur(net))
		m = rcu_dereference(priv->match);
	else
		m = priv->clone;

	if (unlikely(!m))
		goto out;

	for (i = 0, f = m->f; i < m->field_count - 1; i++, f++)
		;

	for (r = 0; r < f->rules; r++) {
		struct nft_pipapo_elem *e;
		struct nft_set_elem elem;

		if (r < f->rules - 1 && f->mt[r + 1].e == f->mt[r].e)
			continue;

		if (iter->count < iter->skip)
			goto cont;

		e = f->mt[r].e;

		if (!nft_set_elem_active(&e->ext, iter->genmask))
			goto cont;

		elem.priv = e;

		iter->err = iter->fn(ctx, set, iter, &elem);
		if (iter->err < 0)
			goto out;

cont:
		iter->count++;
	}

out:
	rcu_read_unlock();
}

 
static u64 nft_pipapo_privsize(const struct nlattr * const nla[],
			       const struct nft_set_desc *desc)
{
	return sizeof(struct nft_pipapo);
}

 
static bool nft_pipapo_estimate(const struct nft_set_desc *desc, u32 features,
				struct nft_set_estimate *est)
{
	if (!(features & NFT_SET_INTERVAL) ||
	    desc->field_count < NFT_PIPAPO_MIN_FIELDS)
		return false;

	est->size = pipapo_estimate_size(desc);
	if (!est->size)
		return false;

	est->lookup = NFT_SET_CLASS_O_LOG_N;

	est->space = NFT_SET_CLASS_O_N;

	return true;
}

 
static int nft_pipapo_init(const struct nft_set *set,
			   const struct nft_set_desc *desc,
			   const struct nlattr * const nla[])
{
	struct nft_pipapo *priv = nft_set_priv(set);
	struct nft_pipapo_match *m;
	struct nft_pipapo_field *f;
	int err, i, field_count;

	field_count = desc->field_count ? : 1;

	if (field_count > NFT_PIPAPO_MAX_FIELDS)
		return -EINVAL;

	m = kmalloc(struct_size(m, f, field_count), GFP_KERNEL);
	if (!m)
		return -ENOMEM;

	m->field_count = field_count;
	m->bsize_max = 0;

	m->scratch = alloc_percpu(unsigned long *);
	if (!m->scratch) {
		err = -ENOMEM;
		goto out_scratch;
	}
	for_each_possible_cpu(i)
		*per_cpu_ptr(m->scratch, i) = NULL;

#ifdef NFT_PIPAPO_ALIGN
	m->scratch_aligned = alloc_percpu(unsigned long *);
	if (!m->scratch_aligned) {
		err = -ENOMEM;
		goto out_free;
	}
	for_each_possible_cpu(i)
		*per_cpu_ptr(m->scratch_aligned, i) = NULL;
#endif

	rcu_head_init(&m->rcu);

	nft_pipapo_for_each_field(f, i, m) {
		int len = desc->field_len[i] ? : set->klen;

		f->bb = NFT_PIPAPO_GROUP_BITS_INIT;
		f->groups = len * NFT_PIPAPO_GROUPS_PER_BYTE(f);

		priv->width += round_up(len, sizeof(u32));

		f->bsize = 0;
		f->rules = 0;
		NFT_PIPAPO_LT_ASSIGN(f, NULL);
		f->mt = NULL;
	}

	 
	priv->clone = pipapo_clone(m);
	if (IS_ERR(priv->clone)) {
		err = PTR_ERR(priv->clone);
		goto out_free;
	}

	priv->dirty = false;

	rcu_assign_pointer(priv->match, m);

	return 0;

out_free:
#ifdef NFT_PIPAPO_ALIGN
	free_percpu(m->scratch_aligned);
#endif
	free_percpu(m->scratch);
out_scratch:
	kfree(m);

	return err;
}

 
static void nft_set_pipapo_match_destroy(const struct nft_ctx *ctx,
					 const struct nft_set *set,
					 struct nft_pipapo_match *m)
{
	struct nft_pipapo_field *f;
	int i, r;

	for (i = 0, f = m->f; i < m->field_count - 1; i++, f++)
		;

	for (r = 0; r < f->rules; r++) {
		struct nft_pipapo_elem *e;

		if (r < f->rules - 1 && f->mt[r + 1].e == f->mt[r].e)
			continue;

		e = f->mt[r].e;

		nf_tables_set_elem_destroy(ctx, set, e);
	}
}

 
static void nft_pipapo_destroy(const struct nft_ctx *ctx,
			       const struct nft_set *set)
{
	struct nft_pipapo *priv = nft_set_priv(set);
	struct nft_pipapo_match *m;
	int cpu;

	m = rcu_dereference_protected(priv->match, true);
	if (m) {
		rcu_barrier();

		nft_set_pipapo_match_destroy(ctx, set, m);

#ifdef NFT_PIPAPO_ALIGN
		free_percpu(m->scratch_aligned);
#endif
		for_each_possible_cpu(cpu)
			kfree(*per_cpu_ptr(m->scratch, cpu));
		free_percpu(m->scratch);
		pipapo_free_fields(m);
		kfree(m);
		priv->match = NULL;
	}

	if (priv->clone) {
		m = priv->clone;

		if (priv->dirty)
			nft_set_pipapo_match_destroy(ctx, set, m);

#ifdef NFT_PIPAPO_ALIGN
		free_percpu(priv->clone->scratch_aligned);
#endif
		for_each_possible_cpu(cpu)
			kfree(*per_cpu_ptr(priv->clone->scratch, cpu));
		free_percpu(priv->clone->scratch);

		pipapo_free_fields(priv->clone);
		kfree(priv->clone);
		priv->clone = NULL;
	}
}

 
static void nft_pipapo_gc_init(const struct nft_set *set)
{
	struct nft_pipapo *priv = nft_set_priv(set);

	priv->last_gc = jiffies;
}

const struct nft_set_type nft_set_pipapo_type = {
	.features	= NFT_SET_INTERVAL | NFT_SET_MAP | NFT_SET_OBJECT |
			  NFT_SET_TIMEOUT,
	.ops		= {
		.lookup		= nft_pipapo_lookup,
		.insert		= nft_pipapo_insert,
		.activate	= nft_pipapo_activate,
		.deactivate	= nft_pipapo_deactivate,
		.flush		= nft_pipapo_flush,
		.remove		= nft_pipapo_remove,
		.walk		= nft_pipapo_walk,
		.get		= nft_pipapo_get,
		.privsize	= nft_pipapo_privsize,
		.estimate	= nft_pipapo_estimate,
		.init		= nft_pipapo_init,
		.destroy	= nft_pipapo_destroy,
		.gc_init	= nft_pipapo_gc_init,
		.commit		= nft_pipapo_commit,
		.abort		= nft_pipapo_abort,
		.elemsize	= offsetof(struct nft_pipapo_elem, ext),
	},
};

#if defined(CONFIG_X86_64) && !defined(CONFIG_UML)
const struct nft_set_type nft_set_pipapo_avx2_type = {
	.features	= NFT_SET_INTERVAL | NFT_SET_MAP | NFT_SET_OBJECT |
			  NFT_SET_TIMEOUT,
	.ops		= {
		.lookup		= nft_pipapo_avx2_lookup,
		.insert		= nft_pipapo_insert,
		.activate	= nft_pipapo_activate,
		.deactivate	= nft_pipapo_deactivate,
		.flush		= nft_pipapo_flush,
		.remove		= nft_pipapo_remove,
		.walk		= nft_pipapo_walk,
		.get		= nft_pipapo_get,
		.privsize	= nft_pipapo_privsize,
		.estimate	= nft_pipapo_avx2_estimate,
		.init		= nft_pipapo_init,
		.destroy	= nft_pipapo_destroy,
		.gc_init	= nft_pipapo_gc_init,
		.commit		= nft_pipapo_commit,
		.abort		= nft_pipapo_abort,
		.elemsize	= offsetof(struct nft_pipapo_elem, ext),
	},
};
#endif
