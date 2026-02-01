
 

#include <linux/slab.h>
#include "internal.h"

static const char cachefiles_charmap[64] =
	"0123456789"			 
	"abcdefghijklmnopqrstuvwxyz"	 
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"	 
	"_-"				 
	;

static const char cachefiles_filecharmap[256] = {
	 
	[33 ... 46] = 1,		 
	 
	[48 ... 127] = 1,		 
};

static inline unsigned int how_many_hex_digits(unsigned int x)
{
	return x ? round_up(ilog2(x) + 1, 4) / 4 : 0;
}

 
bool cachefiles_cook_key(struct cachefiles_object *object)
{
	const u8 *key = fscache_get_key(object->cookie), *kend;
	unsigned char ch;
	unsigned int acc, i, n, nle, nbe, keylen = object->cookie->key_len;
	unsigned int b64len, len, print, pad;
	char *name, sep;

	_enter(",%u,%*phN", keylen, keylen, key);

	BUG_ON(keylen > NAME_MAX - 3);

	print = 1;
	for (i = 0; i < keylen; i++) {
		ch = key[i];
		print &= cachefiles_filecharmap[ch];
	}

	 
	if (print) {
		len = 1 + keylen;
		name = kmalloc(len + 1, GFP_KERNEL);
		if (!name)
			return false;

		name[0] = 'D';  
		memcpy(name + 1, key, keylen);
		goto success;
	}

	 
	n = round_up(keylen, 4);
	nbe = nle = 0;
	for (i = 0; i < n; i += 4) {
		u32 be = be32_to_cpu(*(__be32 *)(key + i));
		u32 le = le32_to_cpu(*(__le32 *)(key + i));

		nbe += 1 + how_many_hex_digits(be);
		nle += 1 + how_many_hex_digits(le);
	}

	b64len = DIV_ROUND_UP(keylen, 3);
	pad = b64len * 3 - keylen;
	b64len = 2 + b64len * 4;  
	_debug("len=%u nbe=%u nle=%u b64=%u", keylen, nbe, nle, b64len);
	if (nbe < b64len || nle < b64len) {
		unsigned int nlen = min(nbe, nle) + 1;
		name = kmalloc(nlen, GFP_KERNEL);
		if (!name)
			return false;
		sep = (nbe <= nle) ? 'S' : 'T';  
		len = 0;
		for (i = 0; i < n; i += 4) {
			u32 x;
			if (nbe <= nle)
				x = be32_to_cpu(*(__be32 *)(key + i));
			else
				x = le32_to_cpu(*(__le32 *)(key + i));
			name[len++] = sep;
			if (x != 0)
				len += snprintf(name + len, nlen - len, "%x", x);
			sep = ',';
		}
		goto success;
	}

	 
	name = kmalloc(b64len + 1, GFP_KERNEL);
	if (!name)
		return false;

	name[0] = 'E';
	name[1] = '0' + pad;
	len = 2;
	kend = key + keylen;
	do {
		acc  = *key++;
		if (key < kend) {
			acc |= *key++ << 8;
			if (key < kend)
				acc |= *key++ << 16;
		}

		name[len++] = cachefiles_charmap[acc & 63];
		acc >>= 6;
		name[len++] = cachefiles_charmap[acc & 63];
		acc >>= 6;
		name[len++] = cachefiles_charmap[acc & 63];
		acc >>= 6;
		name[len++] = cachefiles_charmap[acc & 63];
	} while (key < kend);

success:
	name[len] = 0;
	object->d_name = name;
	object->d_name_len = len;
	_leave(" = %s", object->d_name);
	return true;
}
