


#ifndef UTF8NORM_H
#define UTF8NORM_H

#include <linux/types.h>
#include <linux/export.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/unicode.h>

int utf8version_is_supported(const struct unicode_map *um, unsigned int version);


ssize_t utf8nlen(const struct unicode_map *um, enum utf8_normalization n,
		const char *s, size_t len);


#define UTF8HANGULLEAF	(12)


struct utf8cursor {
	const struct unicode_map *um;
	enum utf8_normalization n;
	const char	*s;
	const char	*p;
	const char	*ss;
	const char	*sp;
	unsigned int	len;
	unsigned int	slen;
	short int	ccc;
	short int	nccc;
	unsigned char	hangul[UTF8HANGULLEAF];
};


int utf8ncursor(struct utf8cursor *u8c, const struct unicode_map *um,
		enum utf8_normalization n, const char *s, size_t len);


extern int utf8byte(struct utf8cursor *u8c);

struct utf8data {
	unsigned int maxage;
	unsigned int offset;
};

struct utf8data_table {
	const unsigned int *utf8agetab;
	int utf8agetab_size;

	const struct utf8data *utf8nfdicfdata;
	int utf8nfdicfdata_size;

	const struct utf8data *utf8nfdidata;
	int utf8nfdidata_size;

	const unsigned char *utf8data;
};

extern struct utf8data_table utf8_data_table;

#endif 
