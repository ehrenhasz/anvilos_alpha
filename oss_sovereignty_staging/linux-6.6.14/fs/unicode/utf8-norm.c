
 

#include "utf8n.h"

int utf8version_is_supported(const struct unicode_map *um, unsigned int version)
{
	int i = um->tables->utf8agetab_size - 1;

	while (i >= 0 && um->tables->utf8agetab[i] != 0) {
		if (version == um->tables->utf8agetab[i])
			return 1;
		i--;
	}
	return 0;
}

 

 
static inline int utf8clen(const char *s)
{
	unsigned char c = *s;

	return 1 + (c >= 0xC0) + (c >= 0xE0) + (c >= 0xF0);
}

 
static unsigned int
utf8decode3(const char *str)
{
	unsigned int		uc;

	uc = *str++ & 0x0F;
	uc <<= 6;
	uc |= *str++ & 0x3F;
	uc <<= 6;
	uc |= *str++ & 0x3F;

	return uc;
}

 
static int
utf8encode3(char *str, unsigned int val)
{
	str[2] = (val & 0x3F) | 0x80;
	val >>= 6;
	str[1] = (val & 0x3F) | 0x80;
	val >>= 6;
	str[0] = val | 0xE0;

	return 3;
}

 
typedef const unsigned char utf8trie_t;
#define BITNUM		0x07
#define NEXTBYTE	0x08
#define OFFLEN		0x30
#define OFFLEN_SHIFT	4
#define RIGHTPATH	0x40
#define TRIENODE	0x80
#define RIGHTNODE	0x40
#define LEFTNODE	0x80

 
typedef const unsigned char utf8leaf_t;

#define LEAF_GEN(LEAF)	((LEAF)[0])
#define LEAF_CCC(LEAF)	((LEAF)[1])
#define LEAF_STR(LEAF)	((const char *)((LEAF) + 2))

#define MINCCC		(0)
#define MAXCCC		(254)
#define STOPPER		(0)
#define	DECOMPOSE	(255)

 
#define HANGUL		((char)(255))
 
#define UTF8HANGULLEAF	(12)

 

 
#define SB	(0xAC00)
#define LB	(0x1100)
#define VB	(0x1161)
#define TB	(0x11A7)
#define LC	(19)
#define VC	(21)
#define TC	(28)
#define NC	(VC * TC)
#define SC	(LC * NC)

 
static utf8leaf_t *
utf8hangul(const char *str, unsigned char *hangul)
{
	unsigned int	si;
	unsigned int	li;
	unsigned int	vi;
	unsigned int	ti;
	unsigned char	*h;

	 
	si = utf8decode3(str) - SB;
	li = si / NC;
	vi = (si % NC) / TC;
	ti = si % TC;

	 
	h = hangul;
	LEAF_GEN(h) = 2;
	LEAF_CCC(h) = DECOMPOSE;
	h += 2;

	 
	h += utf8encode3((char *)h, li + LB);

	 
	h += utf8encode3((char *)h, vi + VB);

	 
	if (ti)
		h += utf8encode3((char *)h, ti + TB);

	 
	h[0] = '\0';

	return hangul;
}

 
static utf8leaf_t *utf8nlookup(const struct unicode_map *um,
		enum utf8_normalization n, unsigned char *hangul, const char *s,
		size_t len)
{
	utf8trie_t	*trie = um->tables->utf8data + um->ntab[n]->offset;
	int		offlen;
	int		offset;
	int		mask;
	int		node;

	if (len == 0)
		return NULL;

	node = 1;
	while (node) {
		offlen = (*trie & OFFLEN) >> OFFLEN_SHIFT;
		if (*trie & NEXTBYTE) {
			if (--len == 0)
				return NULL;
			s++;
		}
		mask = 1 << (*trie & BITNUM);
		if (*s & mask) {
			 
			if (offlen) {
				 
				node = (*trie & RIGHTNODE);
				offset = trie[offlen];
				while (--offlen) {
					offset <<= 8;
					offset |= trie[offlen];
				}
				trie += offset;
			} else if (*trie & RIGHTPATH) {
				 
				node = (*trie & TRIENODE);
				trie++;
			} else {
				 
				return NULL;
			}
		} else {
			 
			if (offlen) {
				 
				node = (*trie & LEFTNODE);
				trie += offlen + 1;
			} else if (*trie & RIGHTPATH) {
				 
				return NULL;
			} else {
				 
				node = (*trie & TRIENODE);
				trie++;
			}
		}
	}
	 
	if (LEAF_CCC(trie) == DECOMPOSE && LEAF_STR(trie)[0] == HANGUL)
		trie = utf8hangul(s - 2, hangul);
	return trie;
}

 
static utf8leaf_t *utf8lookup(const struct unicode_map *um,
		enum utf8_normalization n, unsigned char *hangul, const char *s)
{
	return utf8nlookup(um, n, hangul, s, (size_t)-1);
}

 
ssize_t utf8nlen(const struct unicode_map *um, enum utf8_normalization n,
		const char *s, size_t len)
{
	utf8leaf_t	*leaf;
	size_t		ret = 0;
	unsigned char	hangul[UTF8HANGULLEAF];

	while (len && *s) {
		leaf = utf8nlookup(um, n, hangul, s, len);
		if (!leaf)
			return -1;
		if (um->tables->utf8agetab[LEAF_GEN(leaf)] >
		    um->ntab[n]->maxage)
			ret += utf8clen(s);
		else if (LEAF_CCC(leaf) == DECOMPOSE)
			ret += strlen(LEAF_STR(leaf));
		else
			ret += utf8clen(s);
		len -= utf8clen(s);
		s += utf8clen(s);
	}
	return ret;
}

 
int utf8ncursor(struct utf8cursor *u8c, const struct unicode_map *um,
		enum utf8_normalization n, const char *s, size_t len)
{
	if (!s)
		return -1;
	u8c->um = um;
	u8c->n = n;
	u8c->s = s;
	u8c->p = NULL;
	u8c->ss = NULL;
	u8c->sp = NULL;
	u8c->len = len;
	u8c->slen = 0;
	u8c->ccc = STOPPER;
	u8c->nccc = STOPPER;
	 
	if (u8c->len != len)
		return -1;
	 
	if (len > 0 && (*s & 0xC0) == 0x80)
		return -1;
	return 0;
}

 
int utf8byte(struct utf8cursor *u8c)
{
	utf8leaf_t *leaf;
	int ccc;

	for (;;) {
		 
		if (u8c->p && *u8c->s == '\0') {
			u8c->s = u8c->p;
			u8c->p = NULL;
		}

		 
		if (!u8c->p && (u8c->len == 0 || *u8c->s == '\0')) {
			 
			if (u8c->ccc == STOPPER)
				return 0;
			 
			ccc = STOPPER;
			goto ccc_mismatch;
		} else if ((*u8c->s & 0xC0) == 0x80) {
			 
			if (!u8c->p)
				u8c->len--;
			return (unsigned char)*u8c->s++;
		}

		 
		if (u8c->p) {
			leaf = utf8lookup(u8c->um, u8c->n, u8c->hangul, u8c->s);
		} else {
			leaf = utf8nlookup(u8c->um, u8c->n, u8c->hangul,
					   u8c->s, u8c->len);
		}

		 
		if (!leaf)
			return -1;

		ccc = LEAF_CCC(leaf);
		 
		if (u8c->um->tables->utf8agetab[LEAF_GEN(leaf)] >
		    u8c->um->ntab[u8c->n]->maxage) {
			ccc = STOPPER;
		} else if (ccc == DECOMPOSE) {
			u8c->len -= utf8clen(u8c->s);
			u8c->p = u8c->s + utf8clen(u8c->s);
			u8c->s = LEAF_STR(leaf);
			 
			if (*u8c->s == '\0') {
				if (u8c->ccc == STOPPER)
					continue;
				ccc = STOPPER;
				goto ccc_mismatch;
			}

			leaf = utf8lookup(u8c->um, u8c->n, u8c->hangul, u8c->s);
			if (!leaf)
				return -1;
			ccc = LEAF_CCC(leaf);
		}

		 
		if (ccc != STOPPER && u8c->ccc < ccc && ccc < u8c->nccc)
			u8c->nccc = ccc;

		 
		if (ccc == u8c->ccc) {
			if (!u8c->p)
				u8c->len--;
			return (unsigned char)*u8c->s++;
		}

		 
ccc_mismatch:
		if (u8c->nccc == STOPPER) {
			 
			u8c->ccc = MINCCC - 1;
			u8c->nccc = ccc;
			u8c->sp = u8c->p;
			u8c->ss = u8c->s;
			u8c->slen = u8c->len;
			if (!u8c->p)
				u8c->len -= utf8clen(u8c->s);
			u8c->s += utf8clen(u8c->s);
		} else if (ccc != STOPPER) {
			 
			if (!u8c->p)
				u8c->len -= utf8clen(u8c->s);
			u8c->s += utf8clen(u8c->s);
		} else if (u8c->nccc != MAXCCC + 1) {
			 
			u8c->ccc = u8c->nccc;
			u8c->nccc = MAXCCC + 1;
			u8c->s = u8c->ss;
			u8c->p = u8c->sp;
			u8c->len = u8c->slen;
		} else {
			 
			u8c->ccc = STOPPER;
			u8c->nccc = STOPPER;
			u8c->sp = NULL;
			u8c->ss = NULL;
			u8c->slen = 0;
		}
	}
}

#ifdef CONFIG_UNICODE_NORMALIZATION_SELFTEST_MODULE
EXPORT_SYMBOL_GPL(utf8version_is_supported);
EXPORT_SYMBOL_GPL(utf8nlen);
EXPORT_SYMBOL_GPL(utf8ncursor);
EXPORT_SYMBOL_GPL(utf8byte);
#endif
