
 
#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/export.h>
#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/limits.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/string_helpers.h>

 
void string_get_size(u64 size, u64 blk_size, const enum string_size_units units,
		     char *buf, int len)
{
	static const char *const units_10[] = {
		"B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"
	};
	static const char *const units_2[] = {
		"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"
	};
	static const char *const *const units_str[] = {
		[STRING_UNITS_10] = units_10,
		[STRING_UNITS_2] = units_2,
	};
	static const unsigned int divisor[] = {
		[STRING_UNITS_10] = 1000,
		[STRING_UNITS_2] = 1024,
	};
	static const unsigned int rounding[] = { 500, 50, 5 };
	int i = 0, j;
	u32 remainder = 0, sf_cap;
	char tmp[8];
	const char *unit;

	tmp[0] = '\0';

	if (blk_size == 0)
		size = 0;
	if (size == 0)
		goto out;

	 
	while (blk_size >> 32) {
		do_div(blk_size, divisor[units]);
		i++;
	}

	while (size >> 32) {
		do_div(size, divisor[units]);
		i++;
	}

	 
	size *= blk_size;

	 
	while (size >= divisor[units]) {
		remainder = do_div(size, divisor[units]);
		i++;
	}

	 
	sf_cap = size;
	for (j = 0; sf_cap*10 < 1000; j++)
		sf_cap *= 10;

	if (units == STRING_UNITS_2) {
		 
		remainder *= 1000;
		remainder >>= 10;
	}

	 
	remainder += rounding[j];
	if (remainder >= 1000) {
		remainder -= 1000;
		size += 1;
	}

	if (j) {
		snprintf(tmp, sizeof(tmp), ".%03u", remainder);
		tmp[j+1] = '\0';
	}

 out:
	if (i >= ARRAY_SIZE(units_2))
		unit = "UNK";
	else
		unit = units_str[units][i];

	snprintf(buf, len, "%u%s %s", (u32)size,
		 tmp, unit);
}
EXPORT_SYMBOL(string_get_size);

 
int parse_int_array_user(const char __user *from, size_t count, int **array)
{
	int *ints, nints;
	char *buf;
	int ret = 0;

	buf = memdup_user_nul(from, count);
	if (IS_ERR(buf))
		return PTR_ERR(buf);

	get_options(buf, 0, &nints);
	if (!nints) {
		ret = -ENOENT;
		goto free_buf;
	}

	ints = kcalloc(nints + 1, sizeof(*ints), GFP_KERNEL);
	if (!ints) {
		ret = -ENOMEM;
		goto free_buf;
	}

	get_options(buf, nints + 1, ints);
	*array = ints;

free_buf:
	kfree(buf);
	return ret;
}
EXPORT_SYMBOL(parse_int_array_user);

static bool unescape_space(char **src, char **dst)
{
	char *p = *dst, *q = *src;

	switch (*q) {
	case 'n':
		*p = '\n';
		break;
	case 'r':
		*p = '\r';
		break;
	case 't':
		*p = '\t';
		break;
	case 'v':
		*p = '\v';
		break;
	case 'f':
		*p = '\f';
		break;
	default:
		return false;
	}
	*dst += 1;
	*src += 1;
	return true;
}

static bool unescape_octal(char **src, char **dst)
{
	char *p = *dst, *q = *src;
	u8 num;

	if (isodigit(*q) == 0)
		return false;

	num = (*q++) & 7;
	while (num < 32 && isodigit(*q) && (q - *src < 3)) {
		num <<= 3;
		num += (*q++) & 7;
	}
	*p = num;
	*dst += 1;
	*src = q;
	return true;
}

static bool unescape_hex(char **src, char **dst)
{
	char *p = *dst, *q = *src;
	int digit;
	u8 num;

	if (*q++ != 'x')
		return false;

	num = digit = hex_to_bin(*q++);
	if (digit < 0)
		return false;

	digit = hex_to_bin(*q);
	if (digit >= 0) {
		q++;
		num = (num << 4) | digit;
	}
	*p = num;
	*dst += 1;
	*src = q;
	return true;
}

static bool unescape_special(char **src, char **dst)
{
	char *p = *dst, *q = *src;

	switch (*q) {
	case '\"':
		*p = '\"';
		break;
	case '\\':
		*p = '\\';
		break;
	case 'a':
		*p = '\a';
		break;
	case 'e':
		*p = '\e';
		break;
	default:
		return false;
	}
	*dst += 1;
	*src += 1;
	return true;
}

 
int string_unescape(char *src, char *dst, size_t size, unsigned int flags)
{
	char *out = dst;

	while (*src && --size) {
		if (src[0] == '\\' && src[1] != '\0' && size > 1) {
			src++;
			size--;

			if (flags & UNESCAPE_SPACE &&
					unescape_space(&src, &out))
				continue;

			if (flags & UNESCAPE_OCTAL &&
					unescape_octal(&src, &out))
				continue;

			if (flags & UNESCAPE_HEX &&
					unescape_hex(&src, &out))
				continue;

			if (flags & UNESCAPE_SPECIAL &&
					unescape_special(&src, &out))
				continue;

			*out++ = '\\';
		}
		*out++ = *src++;
	}
	*out = '\0';

	return out - dst;
}
EXPORT_SYMBOL(string_unescape);

static bool escape_passthrough(unsigned char c, char **dst, char *end)
{
	char *out = *dst;

	if (out < end)
		*out = c;
	*dst = out + 1;
	return true;
}

static bool escape_space(unsigned char c, char **dst, char *end)
{
	char *out = *dst;
	unsigned char to;

	switch (c) {
	case '\n':
		to = 'n';
		break;
	case '\r':
		to = 'r';
		break;
	case '\t':
		to = 't';
		break;
	case '\v':
		to = 'v';
		break;
	case '\f':
		to = 'f';
		break;
	default:
		return false;
	}

	if (out < end)
		*out = '\\';
	++out;
	if (out < end)
		*out = to;
	++out;

	*dst = out;
	return true;
}

static bool escape_special(unsigned char c, char **dst, char *end)
{
	char *out = *dst;
	unsigned char to;

	switch (c) {
	case '\\':
		to = '\\';
		break;
	case '\a':
		to = 'a';
		break;
	case '\e':
		to = 'e';
		break;
	case '"':
		to = '"';
		break;
	default:
		return false;
	}

	if (out < end)
		*out = '\\';
	++out;
	if (out < end)
		*out = to;
	++out;

	*dst = out;
	return true;
}

static bool escape_null(unsigned char c, char **dst, char *end)
{
	char *out = *dst;

	if (c)
		return false;

	if (out < end)
		*out = '\\';
	++out;
	if (out < end)
		*out = '0';
	++out;

	*dst = out;
	return true;
}

static bool escape_octal(unsigned char c, char **dst, char *end)
{
	char *out = *dst;

	if (out < end)
		*out = '\\';
	++out;
	if (out < end)
		*out = ((c >> 6) & 0x07) + '0';
	++out;
	if (out < end)
		*out = ((c >> 3) & 0x07) + '0';
	++out;
	if (out < end)
		*out = ((c >> 0) & 0x07) + '0';
	++out;

	*dst = out;
	return true;
}

static bool escape_hex(unsigned char c, char **dst, char *end)
{
	char *out = *dst;

	if (out < end)
		*out = '\\';
	++out;
	if (out < end)
		*out = 'x';
	++out;
	if (out < end)
		*out = hex_asc_hi(c);
	++out;
	if (out < end)
		*out = hex_asc_lo(c);
	++out;

	*dst = out;
	return true;
}

 
int string_escape_mem(const char *src, size_t isz, char *dst, size_t osz,
		      unsigned int flags, const char *only)
{
	char *p = dst;
	char *end = p + osz;
	bool is_dict = only && *only;
	bool is_append = flags & ESCAPE_APPEND;

	while (isz--) {
		unsigned char c = *src++;
		bool in_dict = is_dict && strchr(only, c);

		 
		if (!(is_append || in_dict) && is_dict &&
					  escape_passthrough(c, &p, end))
			continue;

		if (!(is_append && in_dict) && isascii(c) && isprint(c) &&
		    flags & ESCAPE_NAP && escape_passthrough(c, &p, end))
			continue;

		if (!(is_append && in_dict) && isprint(c) &&
		    flags & ESCAPE_NP && escape_passthrough(c, &p, end))
			continue;

		if (!(is_append && in_dict) && isascii(c) &&
		    flags & ESCAPE_NA && escape_passthrough(c, &p, end))
			continue;

		if (flags & ESCAPE_SPACE && escape_space(c, &p, end))
			continue;

		if (flags & ESCAPE_SPECIAL && escape_special(c, &p, end))
			continue;

		if (flags & ESCAPE_NULL && escape_null(c, &p, end))
			continue;

		 
		if (flags & ESCAPE_OCTAL && escape_octal(c, &p, end))
			continue;

		if (flags & ESCAPE_HEX && escape_hex(c, &p, end))
			continue;

		escape_passthrough(c, &p, end);
	}

	return p - dst;
}
EXPORT_SYMBOL(string_escape_mem);

 
char *kstrdup_quotable(const char *src, gfp_t gfp)
{
	size_t slen, dlen;
	char *dst;
	const int flags = ESCAPE_HEX;
	const char esc[] = "\f\n\r\t\v\a\e\\\"";

	if (!src)
		return NULL;
	slen = strlen(src);

	dlen = string_escape_mem(src, slen, NULL, 0, flags, esc);
	dst = kmalloc(dlen + 1, gfp);
	if (!dst)
		return NULL;

	WARN_ON(string_escape_mem(src, slen, dst, dlen, flags, esc) != dlen);
	dst[dlen] = '\0';

	return dst;
}
EXPORT_SYMBOL_GPL(kstrdup_quotable);

 
char *kstrdup_quotable_cmdline(struct task_struct *task, gfp_t gfp)
{
	char *buffer, *quoted;
	int i, res;

	buffer = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buffer)
		return NULL;

	res = get_cmdline(task, buffer, PAGE_SIZE - 1);
	buffer[res] = '\0';

	 
	while (--res >= 0 && buffer[res] == '\0')
		;

	 
	for (i = 0; i <= res; i++)
		if (buffer[i] == '\0')
			buffer[i] = ' ';

	 
	quoted = kstrdup_quotable(buffer, gfp);
	kfree(buffer);
	return quoted;
}
EXPORT_SYMBOL_GPL(kstrdup_quotable_cmdline);

 
char *kstrdup_quotable_file(struct file *file, gfp_t gfp)
{
	char *temp, *pathname;

	if (!file)
		return kstrdup("<unknown>", gfp);

	 
	temp = kmalloc(PATH_MAX + 11, GFP_KERNEL);
	if (!temp)
		return kstrdup("<no_memory>", gfp);

	pathname = file_path(file, temp, PATH_MAX + 11);
	if (IS_ERR(pathname))
		pathname = kstrdup("<too_long>", gfp);
	else
		pathname = kstrdup_quotable(pathname, gfp);

	kfree(temp);
	return pathname;
}
EXPORT_SYMBOL_GPL(kstrdup_quotable_file);

 
char *kstrdup_and_replace(const char *src, char old, char new, gfp_t gfp)
{
	char *dst;

	dst = kstrdup(src, gfp);
	if (!dst)
		return NULL;

	return strreplace(dst, old, new);
}
EXPORT_SYMBOL_GPL(kstrdup_and_replace);

 
char **kasprintf_strarray(gfp_t gfp, const char *prefix, size_t n)
{
	char **names;
	size_t i;

	names = kcalloc(n + 1, sizeof(char *), gfp);
	if (!names)
		return NULL;

	for (i = 0; i < n; i++) {
		names[i] = kasprintf(gfp, "%s-%zu", prefix, i);
		if (!names[i]) {
			kfree_strarray(names, i);
			return NULL;
		}
	}

	return names;
}
EXPORT_SYMBOL_GPL(kasprintf_strarray);

 
void kfree_strarray(char **array, size_t n)
{
	unsigned int i;

	if (!array)
		return;

	for (i = 0; i < n; i++)
		kfree(array[i]);
	kfree(array);
}
EXPORT_SYMBOL_GPL(kfree_strarray);

struct strarray {
	char **array;
	size_t n;
};

static void devm_kfree_strarray(struct device *dev, void *res)
{
	struct strarray *array = res;

	kfree_strarray(array->array, array->n);
}

char **devm_kasprintf_strarray(struct device *dev, const char *prefix, size_t n)
{
	struct strarray *ptr;

	ptr = devres_alloc(devm_kfree_strarray, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	ptr->array = kasprintf_strarray(GFP_KERNEL, prefix, n);
	if (!ptr->array) {
		devres_free(ptr);
		return ERR_PTR(-ENOMEM);
	}

	ptr->n = n;
	devres_add(dev, ptr);

	return ptr->array;
}
EXPORT_SYMBOL_GPL(devm_kasprintf_strarray);

 
ssize_t strscpy_pad(char *dest, const char *src, size_t count)
{
	ssize_t written;

	written = strscpy(dest, src, count);
	if (written < 0 || written == count - 1)
		return written;

	memset(dest + written + 1, 0, count - written - 1);

	return written;
}
EXPORT_SYMBOL(strscpy_pad);

 
char *skip_spaces(const char *str)
{
	while (isspace(*str))
		++str;
	return (char *)str;
}
EXPORT_SYMBOL(skip_spaces);

 
char *strim(char *s)
{
	size_t size;
	char *end;

	size = strlen(s);
	if (!size)
		return s;

	end = s + size - 1;
	while (end >= s && isspace(*end))
		end--;
	*(end + 1) = '\0';

	return skip_spaces(s);
}
EXPORT_SYMBOL(strim);

 
bool sysfs_streq(const char *s1, const char *s2)
{
	while (*s1 && *s1 == *s2) {
		s1++;
		s2++;
	}

	if (*s1 == *s2)
		return true;
	if (!*s1 && *s2 == '\n' && !s2[1])
		return true;
	if (*s1 == '\n' && !s1[1] && !*s2)
		return true;
	return false;
}
EXPORT_SYMBOL(sysfs_streq);

 
int match_string(const char * const *array, size_t n, const char *string)
{
	int index;
	const char *item;

	for (index = 0; index < n; index++) {
		item = array[index];
		if (!item)
			break;
		if (!strcmp(item, string))
			return index;
	}

	return -EINVAL;
}
EXPORT_SYMBOL(match_string);

 
int __sysfs_match_string(const char * const *array, size_t n, const char *str)
{
	const char *item;
	int index;

	for (index = 0; index < n; index++) {
		item = array[index];
		if (!item)
			break;
		if (sysfs_streq(item, str))
			return index;
	}

	return -EINVAL;
}
EXPORT_SYMBOL(__sysfs_match_string);

 
char *strreplace(char *str, char old, char new)
{
	char *s = str;

	for (; *s; ++s)
		if (*s == old)
			*s = new;
	return str;
}
EXPORT_SYMBOL(strreplace);

 
void memcpy_and_pad(void *dest, size_t dest_len, const void *src, size_t count,
		    int pad)
{
	if (dest_len > count) {
		memcpy(dest, src, count);
		memset(dest + count, pad,  dest_len - count);
	} else {
		memcpy(dest, src, dest_len);
	}
}
EXPORT_SYMBOL(memcpy_and_pad);

#ifdef CONFIG_FORTIFY_SOURCE
 
void __read_overflow2_field(size_t avail, size_t wanted) { }
EXPORT_SYMBOL(__read_overflow2_field);
void __write_overflow_field(size_t avail, size_t wanted) { }
EXPORT_SYMBOL(__write_overflow_field);

void fortify_panic(const char *name)
{
	pr_emerg("detected buffer overflow in %s\n", name);
	BUG();
}
EXPORT_SYMBOL(fortify_panic);
#endif  
