

#include <linux/ethtool_netlink.h>
#include <linux/bitmap.h>
#include "netlink.h"
#include "bitset.h"

 

static u32 ethnl_lower_bits(unsigned int n)
{
	return ~(u32)0 >> (32 - n % 32);
}

static u32 ethnl_upper_bits(unsigned int n)
{
	return ~(u32)0 << (n % 32);
}

 
static void ethnl_bitmap32_clear(u32 *dst, unsigned int start, unsigned int end,
				 bool *mod)
{
	unsigned int start_word = start / 32;
	unsigned int end_word = end / 32;
	unsigned int i;
	u32 mask;

	if (end <= start)
		return;

	if (start % 32) {
		mask = ethnl_upper_bits(start);
		if (end_word == start_word) {
			mask &= ethnl_lower_bits(end);
			if (dst[start_word] & mask) {
				dst[start_word] &= ~mask;
				*mod = true;
			}
			return;
		}
		if (dst[start_word] & mask) {
			dst[start_word] &= ~mask;
			*mod = true;
		}
		start_word++;
	}

	for (i = start_word; i < end_word; i++) {
		if (dst[i]) {
			dst[i] = 0;
			*mod = true;
		}
	}
	if (end % 32) {
		mask = ethnl_lower_bits(end);
		if (dst[end_word] & mask) {
			dst[end_word] &= ~mask;
			*mod = true;
		}
	}
}

 
static bool ethnl_bitmap32_not_zero(const u32 *map, unsigned int start,
				    unsigned int end)
{
	unsigned int start_word = start / 32;
	unsigned int end_word = end / 32;
	u32 mask;

	if (end <= start)
		return true;

	if (start % 32) {
		mask = ethnl_upper_bits(start);
		if (end_word == start_word) {
			mask &= ethnl_lower_bits(end);
			return map[start_word] & mask;
		}
		if (map[start_word] & mask)
			return true;
		start_word++;
	}

	if (!memchr_inv(map + start_word, '\0',
			(end_word - start_word) * sizeof(u32)))
		return true;
	if (end % 32 == 0)
		return true;
	return map[end_word] & ethnl_lower_bits(end);
}

 
static void ethnl_bitmap32_update(u32 *dst, unsigned int nbits,
				  const u32 *value, const u32 *mask, bool *mod)
{
	while (nbits > 0) {
		u32 real_mask = mask ? *mask : ~(u32)0;
		u32 new_value;

		if (nbits < 32)
			real_mask &= ethnl_lower_bits(nbits);
		new_value = (*dst & ~real_mask) | (*value & real_mask);
		if (new_value != *dst) {
			*dst = new_value;
			*mod = true;
		}

		if (nbits <= 32)
			break;
		dst++;
		nbits -= 32;
		value++;
		if (mask)
			mask++;
	}
}

static bool ethnl_bitmap32_test_bit(const u32 *map, unsigned int index)
{
	return map[index / 32] & (1U << (index % 32));
}

 
int ethnl_bitset32_size(const u32 *val, const u32 *mask, unsigned int nbits,
			ethnl_string_array_t names, bool compact)
{
	unsigned int len = 0;

	 
	if (!mask)
		len += nla_total_size(sizeof(u32));
	 
	len += nla_total_size(sizeof(u32));

	if (compact) {
		unsigned int nwords = DIV_ROUND_UP(nbits, 32);

		 
		len += (mask ? 2 : 1) * nla_total_size(nwords * sizeof(u32));
	} else {
		unsigned int bits_len = 0;
		unsigned int bit_len, i;

		for (i = 0; i < nbits; i++) {
			const char *name = names ? names[i] : NULL;

			if (!ethnl_bitmap32_test_bit(mask ?: val, i))
				continue;
			 
			bit_len = nla_total_size(sizeof(u32));
			 
			if (name)
				bit_len += ethnl_strz_size(name);
			 
			if (mask && ethnl_bitmap32_test_bit(val, i))
				bit_len += nla_total_size(0);

			 
			bits_len += nla_total_size(bit_len);
		}
		 
		len += nla_total_size(bits_len);
	}

	 
	return nla_total_size(len);
}

 
int ethnl_put_bitset32(struct sk_buff *skb, int attrtype, const u32 *val,
		       const u32 *mask, unsigned int nbits,
		       ethnl_string_array_t names, bool compact)
{
	struct nlattr *nest;
	struct nlattr *attr;

	nest = nla_nest_start(skb, attrtype);
	if (!nest)
		return -EMSGSIZE;

	if (!mask && nla_put_flag(skb, ETHTOOL_A_BITSET_NOMASK))
		goto nla_put_failure;
	if (nla_put_u32(skb, ETHTOOL_A_BITSET_SIZE, nbits))
		goto nla_put_failure;
	if (compact) {
		unsigned int nwords = DIV_ROUND_UP(nbits, 32);
		unsigned int nbytes = nwords * sizeof(u32);
		u32 *dst;

		attr = nla_reserve(skb, ETHTOOL_A_BITSET_VALUE, nbytes);
		if (!attr)
			goto nla_put_failure;
		dst = nla_data(attr);
		memcpy(dst, val, nbytes);
		if (nbits % 32)
			dst[nwords - 1] &= ethnl_lower_bits(nbits);

		if (mask) {
			attr = nla_reserve(skb, ETHTOOL_A_BITSET_MASK, nbytes);
			if (!attr)
				goto nla_put_failure;
			dst = nla_data(attr);
			memcpy(dst, mask, nbytes);
			if (nbits % 32)
				dst[nwords - 1] &= ethnl_lower_bits(nbits);
		}
	} else {
		struct nlattr *bits;
		unsigned int i;

		bits = nla_nest_start(skb, ETHTOOL_A_BITSET_BITS);
		if (!bits)
			goto nla_put_failure;
		for (i = 0; i < nbits; i++) {
			const char *name = names ? names[i] : NULL;

			if (!ethnl_bitmap32_test_bit(mask ?: val, i))
				continue;
			attr = nla_nest_start(skb, ETHTOOL_A_BITSET_BITS_BIT);
			if (!attr)
				goto nla_put_failure;
			if (nla_put_u32(skb, ETHTOOL_A_BITSET_BIT_INDEX, i))
				goto nla_put_failure;
			if (name &&
			    ethnl_put_strz(skb, ETHTOOL_A_BITSET_BIT_NAME, name))
				goto nla_put_failure;
			if (mask && ethnl_bitmap32_test_bit(val, i) &&
			    nla_put_flag(skb, ETHTOOL_A_BITSET_BIT_VALUE))
				goto nla_put_failure;
			nla_nest_end(skb, attr);
		}
		nla_nest_end(skb, bits);
	}

	nla_nest_end(skb, nest);
	return 0;

nla_put_failure:
	nla_nest_cancel(skb, nest);
	return -EMSGSIZE;
}

static const struct nla_policy bitset_policy[] = {
	[ETHTOOL_A_BITSET_NOMASK]	= { .type = NLA_FLAG },
	[ETHTOOL_A_BITSET_SIZE]		= NLA_POLICY_MAX(NLA_U32,
							 ETHNL_MAX_BITSET_SIZE),
	[ETHTOOL_A_BITSET_BITS]		= { .type = NLA_NESTED },
	[ETHTOOL_A_BITSET_VALUE]	= { .type = NLA_BINARY },
	[ETHTOOL_A_BITSET_MASK]		= { .type = NLA_BINARY },
};

static const struct nla_policy bit_policy[] = {
	[ETHTOOL_A_BITSET_BIT_INDEX]	= { .type = NLA_U32 },
	[ETHTOOL_A_BITSET_BIT_NAME]	= { .type = NLA_NUL_STRING },
	[ETHTOOL_A_BITSET_BIT_VALUE]	= { .type = NLA_FLAG },
};

 
int ethnl_bitset_is_compact(const struct nlattr *bitset, bool *compact)
{
	struct nlattr *tb[ARRAY_SIZE(bitset_policy)];
	int ret;

	ret = nla_parse_nested(tb, ARRAY_SIZE(bitset_policy) - 1, bitset,
			       bitset_policy, NULL);
	if (ret < 0)
		return ret;

	if (tb[ETHTOOL_A_BITSET_BITS]) {
		if (tb[ETHTOOL_A_BITSET_VALUE] || tb[ETHTOOL_A_BITSET_MASK])
			return -EINVAL;
		*compact = false;
		return 0;
	}
	if (!tb[ETHTOOL_A_BITSET_SIZE] || !tb[ETHTOOL_A_BITSET_VALUE])
		return -EINVAL;

	*compact = true;
	return 0;
}

 
static int ethnl_name_to_idx(ethnl_string_array_t names, unsigned int n_names,
			     const char *name)
{
	unsigned int i;

	if (!names)
		return -ENOENT;

	for (i = 0; i < n_names; i++) {
		 
		if (!strncmp(names[i], name, ETH_GSTRING_LEN) &&
		    strlen(name) <= ETH_GSTRING_LEN)
			return i;
	}

	return -ENOENT;
}

static int ethnl_parse_bit(unsigned int *index, bool *val, unsigned int nbits,
			   const struct nlattr *bit_attr, bool no_mask,
			   ethnl_string_array_t names,
			   struct netlink_ext_ack *extack)
{
	struct nlattr *tb[ARRAY_SIZE(bit_policy)];
	int ret, idx;

	ret = nla_parse_nested(tb, ARRAY_SIZE(bit_policy) - 1, bit_attr,
			       bit_policy, extack);
	if (ret < 0)
		return ret;

	if (tb[ETHTOOL_A_BITSET_BIT_INDEX]) {
		const char *name;

		idx = nla_get_u32(tb[ETHTOOL_A_BITSET_BIT_INDEX]);
		if (idx >= nbits) {
			NL_SET_ERR_MSG_ATTR(extack,
					    tb[ETHTOOL_A_BITSET_BIT_INDEX],
					    "bit index too high");
			return -EOPNOTSUPP;
		}
		name = names ? names[idx] : NULL;
		if (tb[ETHTOOL_A_BITSET_BIT_NAME] && name &&
		    strncmp(nla_data(tb[ETHTOOL_A_BITSET_BIT_NAME]), name,
			    nla_len(tb[ETHTOOL_A_BITSET_BIT_NAME]))) {
			NL_SET_ERR_MSG_ATTR(extack, bit_attr,
					    "bit index and name mismatch");
			return -EINVAL;
		}
	} else if (tb[ETHTOOL_A_BITSET_BIT_NAME]) {
		idx = ethnl_name_to_idx(names, nbits,
					nla_data(tb[ETHTOOL_A_BITSET_BIT_NAME]));
		if (idx < 0) {
			NL_SET_ERR_MSG_ATTR(extack,
					    tb[ETHTOOL_A_BITSET_BIT_NAME],
					    "bit name not found");
			return -EOPNOTSUPP;
		}
	} else {
		NL_SET_ERR_MSG_ATTR(extack, bit_attr,
				    "neither bit index nor name specified");
		return -EINVAL;
	}

	*index = idx;
	*val = no_mask || tb[ETHTOOL_A_BITSET_BIT_VALUE];
	return 0;
}

static int
ethnl_update_bitset32_verbose(u32 *bitmap, unsigned int nbits,
			      const struct nlattr *attr, struct nlattr **tb,
			      ethnl_string_array_t names,
			      struct netlink_ext_ack *extack, bool *mod)
{
	struct nlattr *bit_attr;
	bool no_mask;
	int rem;
	int ret;

	if (tb[ETHTOOL_A_BITSET_VALUE]) {
		NL_SET_ERR_MSG_ATTR(extack, tb[ETHTOOL_A_BITSET_VALUE],
				    "value only allowed in compact bitset");
		return -EINVAL;
	}
	if (tb[ETHTOOL_A_BITSET_MASK]) {
		NL_SET_ERR_MSG_ATTR(extack, tb[ETHTOOL_A_BITSET_MASK],
				    "mask only allowed in compact bitset");
		return -EINVAL;
	}

	no_mask = tb[ETHTOOL_A_BITSET_NOMASK];
	if (no_mask)
		ethnl_bitmap32_clear(bitmap, 0, nbits, mod);

	nla_for_each_nested(bit_attr, tb[ETHTOOL_A_BITSET_BITS], rem) {
		bool old_val, new_val;
		unsigned int idx;

		if (nla_type(bit_attr) != ETHTOOL_A_BITSET_BITS_BIT) {
			NL_SET_ERR_MSG_ATTR(extack, bit_attr,
					    "only ETHTOOL_A_BITSET_BITS_BIT allowed in ETHTOOL_A_BITSET_BITS");
			return -EINVAL;
		}
		ret = ethnl_parse_bit(&idx, &new_val, nbits, bit_attr, no_mask,
				      names, extack);
		if (ret < 0)
			return ret;
		old_val = bitmap[idx / 32] & ((u32)1 << (idx % 32));
		if (new_val != old_val) {
			if (new_val)
				bitmap[idx / 32] |= ((u32)1 << (idx % 32));
			else
				bitmap[idx / 32] &= ~((u32)1 << (idx % 32));
			*mod = true;
		}
	}

	return 0;
}

static int ethnl_compact_sanity_checks(unsigned int nbits,
				       const struct nlattr *nest,
				       struct nlattr **tb,
				       struct netlink_ext_ack *extack)
{
	bool no_mask = tb[ETHTOOL_A_BITSET_NOMASK];
	unsigned int attr_nbits, attr_nwords;
	const struct nlattr *test_attr;

	if (no_mask && tb[ETHTOOL_A_BITSET_MASK]) {
		NL_SET_ERR_MSG_ATTR(extack, tb[ETHTOOL_A_BITSET_MASK],
				    "mask not allowed in list bitset");
		return -EINVAL;
	}
	if (!tb[ETHTOOL_A_BITSET_SIZE]) {
		NL_SET_ERR_MSG_ATTR(extack, nest,
				    "missing size in compact bitset");
		return -EINVAL;
	}
	if (!tb[ETHTOOL_A_BITSET_VALUE]) {
		NL_SET_ERR_MSG_ATTR(extack, nest,
				    "missing value in compact bitset");
		return -EINVAL;
	}
	if (!no_mask && !tb[ETHTOOL_A_BITSET_MASK]) {
		NL_SET_ERR_MSG_ATTR(extack, nest,
				    "missing mask in compact nonlist bitset");
		return -EINVAL;
	}

	attr_nbits = nla_get_u32(tb[ETHTOOL_A_BITSET_SIZE]);
	attr_nwords = DIV_ROUND_UP(attr_nbits, 32);
	if (nla_len(tb[ETHTOOL_A_BITSET_VALUE]) != attr_nwords * sizeof(u32)) {
		NL_SET_ERR_MSG_ATTR(extack, tb[ETHTOOL_A_BITSET_VALUE],
				    "bitset value length does not match size");
		return -EINVAL;
	}
	if (tb[ETHTOOL_A_BITSET_MASK] &&
	    nla_len(tb[ETHTOOL_A_BITSET_MASK]) != attr_nwords * sizeof(u32)) {
		NL_SET_ERR_MSG_ATTR(extack, tb[ETHTOOL_A_BITSET_MASK],
				    "bitset mask length does not match size");
		return -EINVAL;
	}
	if (attr_nbits <= nbits)
		return 0;

	test_attr = no_mask ? tb[ETHTOOL_A_BITSET_VALUE] :
			      tb[ETHTOOL_A_BITSET_MASK];
	if (ethnl_bitmap32_not_zero(nla_data(test_attr), nbits, attr_nbits)) {
		NL_SET_ERR_MSG_ATTR(extack, test_attr,
				    "cannot modify bits past kernel bitset size");
		return -EINVAL;
	}
	return 0;
}

 
int ethnl_update_bitset32(u32 *bitmap, unsigned int nbits,
			  const struct nlattr *attr, ethnl_string_array_t names,
			  struct netlink_ext_ack *extack, bool *mod)
{
	struct nlattr *tb[ARRAY_SIZE(bitset_policy)];
	unsigned int change_bits;
	bool no_mask;
	int ret;

	if (!attr)
		return 0;
	ret = nla_parse_nested(tb, ARRAY_SIZE(bitset_policy) - 1, attr,
			       bitset_policy, extack);
	if (ret < 0)
		return ret;

	if (tb[ETHTOOL_A_BITSET_BITS])
		return ethnl_update_bitset32_verbose(bitmap, nbits, attr, tb,
						     names, extack, mod);
	ret = ethnl_compact_sanity_checks(nbits, attr, tb, extack);
	if (ret < 0)
		return ret;

	no_mask = tb[ETHTOOL_A_BITSET_NOMASK];
	change_bits = min_t(unsigned int,
			    nla_get_u32(tb[ETHTOOL_A_BITSET_SIZE]), nbits);
	ethnl_bitmap32_update(bitmap, change_bits,
			      nla_data(tb[ETHTOOL_A_BITSET_VALUE]),
			      no_mask ? NULL :
					nla_data(tb[ETHTOOL_A_BITSET_MASK]),
			      mod);
	if (no_mask && change_bits < nbits)
		ethnl_bitmap32_clear(bitmap, change_bits, nbits, mod);

	return 0;
}

 
int ethnl_parse_bitset(unsigned long *val, unsigned long *mask,
		       unsigned int nbits, const struct nlattr *attr,
		       ethnl_string_array_t names,
		       struct netlink_ext_ack *extack)
{
	struct nlattr *tb[ARRAY_SIZE(bitset_policy)];
	const struct nlattr *bit_attr;
	bool no_mask;
	int rem;
	int ret;

	if (!attr)
		return 0;
	ret = nla_parse_nested(tb, ARRAY_SIZE(bitset_policy) - 1, attr,
			       bitset_policy, extack);
	if (ret < 0)
		return ret;
	no_mask = tb[ETHTOOL_A_BITSET_NOMASK];

	if (!tb[ETHTOOL_A_BITSET_BITS]) {
		unsigned int change_bits;

		ret = ethnl_compact_sanity_checks(nbits, attr, tb, extack);
		if (ret < 0)
			return ret;

		change_bits = nla_get_u32(tb[ETHTOOL_A_BITSET_SIZE]);
		if (change_bits > nbits)
			change_bits = nbits;
		bitmap_from_arr32(val, nla_data(tb[ETHTOOL_A_BITSET_VALUE]),
				  change_bits);
		if (change_bits < nbits)
			bitmap_clear(val, change_bits, nbits - change_bits);
		if (no_mask) {
			bitmap_fill(mask, nbits);
		} else {
			bitmap_from_arr32(mask,
					  nla_data(tb[ETHTOOL_A_BITSET_MASK]),
					  change_bits);
			if (change_bits < nbits)
				bitmap_clear(mask, change_bits,
					     nbits - change_bits);
		}

		return 0;
	}

	if (tb[ETHTOOL_A_BITSET_VALUE]) {
		NL_SET_ERR_MSG_ATTR(extack, tb[ETHTOOL_A_BITSET_VALUE],
				    "value only allowed in compact bitset");
		return -EINVAL;
	}
	if (tb[ETHTOOL_A_BITSET_MASK]) {
		NL_SET_ERR_MSG_ATTR(extack, tb[ETHTOOL_A_BITSET_MASK],
				    "mask only allowed in compact bitset");
		return -EINVAL;
	}

	bitmap_zero(val, nbits);
	if (no_mask)
		bitmap_fill(mask, nbits);
	else
		bitmap_zero(mask, nbits);

	nla_for_each_nested(bit_attr, tb[ETHTOOL_A_BITSET_BITS], rem) {
		unsigned int idx;
		bool bit_val;

		ret = ethnl_parse_bit(&idx, &bit_val, nbits, bit_attr, no_mask,
				      names, extack);
		if (ret < 0)
			return ret;
		if (bit_val)
			__set_bit(idx, val);
		if (!no_mask)
			__set_bit(idx, mask);
	}

	return 0;
}

#if BITS_PER_LONG == 64 && defined(__BIG_ENDIAN)

 
#define ETHNL_SMALL_BITMAP_BITS 128
#define ETHNL_SMALL_BITMAP_WORDS DIV_ROUND_UP(ETHNL_SMALL_BITMAP_BITS, 32)

int ethnl_bitset_size(const unsigned long *val, const unsigned long *mask,
		      unsigned int nbits, ethnl_string_array_t names,
		      bool compact)
{
	u32 small_mask32[ETHNL_SMALL_BITMAP_WORDS];
	u32 small_val32[ETHNL_SMALL_BITMAP_WORDS];
	u32 *mask32;
	u32 *val32;
	int ret;

	if (nbits > ETHNL_SMALL_BITMAP_BITS) {
		unsigned int nwords = DIV_ROUND_UP(nbits, 32);

		val32 = kmalloc_array(2 * nwords, sizeof(u32), GFP_KERNEL);
		if (!val32)
			return -ENOMEM;
		mask32 = val32 + nwords;
	} else {
		val32 = small_val32;
		mask32 = small_mask32;
	}

	bitmap_to_arr32(val32, val, nbits);
	if (mask)
		bitmap_to_arr32(mask32, mask, nbits);
	else
		mask32 = NULL;
	ret = ethnl_bitset32_size(val32, mask32, nbits, names, compact);

	if (nbits > ETHNL_SMALL_BITMAP_BITS)
		kfree(val32);

	return ret;
}

int ethnl_put_bitset(struct sk_buff *skb, int attrtype,
		     const unsigned long *val, const unsigned long *mask,
		     unsigned int nbits, ethnl_string_array_t names,
		     bool compact)
{
	u32 small_mask32[ETHNL_SMALL_BITMAP_WORDS];
	u32 small_val32[ETHNL_SMALL_BITMAP_WORDS];
	u32 *mask32;
	u32 *val32;
	int ret;

	if (nbits > ETHNL_SMALL_BITMAP_BITS) {
		unsigned int nwords = DIV_ROUND_UP(nbits, 32);

		val32 = kmalloc_array(2 * nwords, sizeof(u32), GFP_KERNEL);
		if (!val32)
			return -ENOMEM;
		mask32 = val32 + nwords;
	} else {
		val32 = small_val32;
		mask32 = small_mask32;
	}

	bitmap_to_arr32(val32, val, nbits);
	if (mask)
		bitmap_to_arr32(mask32, mask, nbits);
	else
		mask32 = NULL;
	ret = ethnl_put_bitset32(skb, attrtype, val32, mask32, nbits, names,
				 compact);

	if (nbits > ETHNL_SMALL_BITMAP_BITS)
		kfree(val32);

	return ret;
}

int ethnl_update_bitset(unsigned long *bitmap, unsigned int nbits,
			const struct nlattr *attr, ethnl_string_array_t names,
			struct netlink_ext_ack *extack, bool *mod)
{
	u32 small_bitmap32[ETHNL_SMALL_BITMAP_WORDS];
	u32 *bitmap32 = small_bitmap32;
	bool u32_mod = false;
	int ret;

	if (nbits > ETHNL_SMALL_BITMAP_BITS) {
		unsigned int dst_words = DIV_ROUND_UP(nbits, 32);

		bitmap32 = kmalloc_array(dst_words, sizeof(u32), GFP_KERNEL);
		if (!bitmap32)
			return -ENOMEM;
	}

	bitmap_to_arr32(bitmap32, bitmap, nbits);
	ret = ethnl_update_bitset32(bitmap32, nbits, attr, names, extack,
				    &u32_mod);
	if (u32_mod) {
		bitmap_from_arr32(bitmap, bitmap32, nbits);
		*mod = true;
	}

	if (nbits > ETHNL_SMALL_BITMAP_BITS)
		kfree(bitmap32);

	return ret;
}

#else

 

int ethnl_bitset_size(const unsigned long *val, const unsigned long *mask,
		      unsigned int nbits, ethnl_string_array_t names,
		      bool compact)
{
	return ethnl_bitset32_size((const u32 *)val, (const u32 *)mask, nbits,
				   names, compact);
}

int ethnl_put_bitset(struct sk_buff *skb, int attrtype,
		     const unsigned long *val, const unsigned long *mask,
		     unsigned int nbits, ethnl_string_array_t names,
		     bool compact)
{
	return ethnl_put_bitset32(skb, attrtype, (const u32 *)val,
				  (const u32 *)mask, nbits, names, compact);
}

int ethnl_update_bitset(unsigned long *bitmap, unsigned int nbits,
			const struct nlattr *attr, ethnl_string_array_t names,
			struct netlink_ext_ack *extack, bool *mod)
{
	return ethnl_update_bitset32((u32 *)bitmap, nbits, attr, names, extack,
				     mod);
}

#endif  
