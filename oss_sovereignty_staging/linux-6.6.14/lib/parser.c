
 

#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/export.h>
#include <linux/kstrtox.h>
#include <linux/parser.h>
#include <linux/slab.h>
#include <linux/string.h>

 
#define NUMBER_BUF_LEN 24

 
static int match_one(char *s, const char *p, substring_t args[])
{
	char *meta;
	int argc = 0;

	if (!p)
		return 1;

	while(1) {
		int len = -1;
		meta = strchr(p, '%');
		if (!meta)
			return strcmp(p, s) == 0;

		if (strncmp(p, s, meta-p))
			return 0;

		s += meta - p;
		p = meta + 1;

		if (isdigit(*p))
			len = simple_strtoul(p, (char **) &p, 10);
		else if (*p == '%') {
			if (*s++ != '%')
				return 0;
			p++;
			continue;
		}

		if (argc >= MAX_OPT_ARGS)
			return 0;

		args[argc].from = s;
		switch (*p++) {
		case 's': {
			size_t str_len = strlen(s);

			if (str_len == 0)
				return 0;
			if (len == -1 || len > str_len)
				len = str_len;
			args[argc].to = s + len;
			break;
		}
		case 'd':
			simple_strtol(s, &args[argc].to, 0);
			goto num;
		case 'u':
			simple_strtoul(s, &args[argc].to, 0);
			goto num;
		case 'o':
			simple_strtoul(s, &args[argc].to, 8);
			goto num;
		case 'x':
			simple_strtoul(s, &args[argc].to, 16);
		num:
			if (args[argc].to == args[argc].from)
				return 0;
			break;
		default:
			return 0;
		}
		s = args[argc].to;
		argc++;
	}
}

 
int match_token(char *s, const match_table_t table, substring_t args[])
{
	const struct match_token *p;

	for (p = table; !match_one(s, p->pattern, args) ; p++)
		;

	return p->token;
}
EXPORT_SYMBOL(match_token);

 
static int match_number(substring_t *s, int *result, int base)
{
	char *endp;
	char buf[NUMBER_BUF_LEN];
	int ret;
	long val;

	if (match_strlcpy(buf, s, NUMBER_BUF_LEN) >= NUMBER_BUF_LEN)
		return -ERANGE;
	ret = 0;
	val = simple_strtol(buf, &endp, base);
	if (endp == buf)
		ret = -EINVAL;
	else if (val < (long)INT_MIN || val > (long)INT_MAX)
		ret = -ERANGE;
	else
		*result = (int) val;
	return ret;
}

 
static int match_u64int(substring_t *s, u64 *result, int base)
{
	char buf[NUMBER_BUF_LEN];
	int ret;
	u64 val;

	if (match_strlcpy(buf, s, NUMBER_BUF_LEN) >= NUMBER_BUF_LEN)
		return -ERANGE;
	ret = kstrtoull(buf, base, &val);
	if (!ret)
		*result = val;
	return ret;
}

 
int match_int(substring_t *s, int *result)
{
	return match_number(s, result, 0);
}
EXPORT_SYMBOL(match_int);

 
int match_uint(substring_t *s, unsigned int *result)
{
	char buf[NUMBER_BUF_LEN];

	if (match_strlcpy(buf, s, NUMBER_BUF_LEN) >= NUMBER_BUF_LEN)
		return -ERANGE;

	return kstrtouint(buf, 10, result);
}
EXPORT_SYMBOL(match_uint);

 
int match_u64(substring_t *s, u64 *result)
{
	return match_u64int(s, result, 0);
}
EXPORT_SYMBOL(match_u64);

 
int match_octal(substring_t *s, int *result)
{
	return match_number(s, result, 8);
}
EXPORT_SYMBOL(match_octal);

 
int match_hex(substring_t *s, int *result)
{
	return match_number(s, result, 16);
}
EXPORT_SYMBOL(match_hex);

 
bool match_wildcard(const char *pattern, const char *str)
{
	const char *s = str;
	const char *p = pattern;
	bool star = false;

	while (*s) {
		switch (*p) {
		case '?':
			s++;
			p++;
			break;
		case '*':
			star = true;
			str = s;
			if (!*++p)
				return true;
			pattern = p;
			break;
		default:
			if (*s == *p) {
				s++;
				p++;
			} else {
				if (!star)
					return false;
				str++;
				s = str;
				p = pattern;
			}
			break;
		}
	}

	if (*p == '*')
		++p;
	return !*p;
}
EXPORT_SYMBOL(match_wildcard);

 
size_t match_strlcpy(char *dest, const substring_t *src, size_t size)
{
	size_t ret = src->to - src->from;

	if (size) {
		size_t len = ret >= size ? size - 1 : ret;
		memcpy(dest, src->from, len);
		dest[len] = '\0';
	}
	return ret;
}
EXPORT_SYMBOL(match_strlcpy);

 
char *match_strdup(const substring_t *s)
{
	return kmemdup_nul(s->from, s->to - s->from, GFP_KERNEL);
}
EXPORT_SYMBOL(match_strdup);
