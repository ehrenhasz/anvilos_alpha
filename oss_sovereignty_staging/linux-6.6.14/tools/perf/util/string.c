
#include "string2.h"
#include <linux/kernel.h>
#include <linux/string.h>
#include <stdlib.h>

#include <linux/ctype.h>

const char *graph_dotted_line =
	"---------------------------------------------------------------------"
	"---------------------------------------------------------------------"
	"---------------------------------------------------------------------";
const char *dots =
	"....................................................................."
	"....................................................................."
	".....................................................................";

 
s64 perf_atoll(const char *str)
{
	s64 length;
	char *p;
	char c;

	if (!isdigit(str[0]))
		goto out_err;

	length = strtoll(str, &p, 10);
	switch (c = *p++) {
		case 'b': case 'B':
			if (*p)
				goto out_err;

			fallthrough;
		case '\0':
			return length;
		default:
			goto out_err;
		 
		case 'k': case 'K':
			length <<= 10;
			break;
		case 'm': case 'M':
			length <<= 20;
			break;
		case 'g': case 'G':
			length <<= 30;
			break;
		case 't': case 'T':
			length <<= 40;
			break;
	}
	 
	if (islower(c)) {
		if (strcmp(p, "b") != 0)
			goto out_err;
	} else {
		if (strcmp(p, "B") != 0)
			goto out_err;
	}
	return length;

out_err:
	return -1;
}

 
static bool __match_charclass(const char *pat, char c, const char **npat)
{
	bool complement = false, ret = true;

	if (*pat == '!') {
		complement = true;
		pat++;
	}
	if (*pat++ == c)	 
		goto end;

	while (*pat && *pat != ']') {	 
		if (*pat == '-' && *(pat + 1) != ']') {	 
			if (*(pat - 1) <= c && c <= *(pat + 1))
				goto end;
			if (*(pat - 1) > *(pat + 1))
				goto error;
			pat += 2;
		} else if (*pat++ == c)
			goto end;
	}
	if (!*pat)
		goto error;
	ret = false;

end:
	while (*pat && *pat != ']')	 
		pat++;
	if (!*pat)
		goto error;
	*npat = pat + 1;
	return complement ? !ret : ret;

error:
	return false;
}

 
static bool __match_glob(const char *str, const char *pat, bool ignore_space,
			bool case_ins)
{
	while (*str && *pat && *pat != '*') {
		if (ignore_space) {
			 
			if (isspace(*str)) {
				str++;
				continue;
			}
			if (isspace(*pat)) {
				pat++;
				continue;
			}
		}
		if (*pat == '?') {	 
			str++;
			pat++;
			continue;
		} else if (*pat == '[')	 
			if (__match_charclass(pat + 1, *str, &pat)) {
				str++;
				continue;
			} else
				return false;
		else if (*pat == '\\')  
			pat++;
		if (case_ins) {
			if (tolower(*str) != tolower(*pat))
				return false;
		} else if (*str != *pat)
			return false;
		str++;
		pat++;
	}
	 
	if (*pat == '*') {
		while (*pat == '*')
			pat++;
		if (!*pat)	 
			return true;
		while (*str)
			if (__match_glob(str++, pat, ignore_space, case_ins))
				return true;
	}
	return !*str && !*pat;
}

 
bool strglobmatch(const char *str, const char *pat)
{
	return __match_glob(str, pat, false, false);
}

bool strglobmatch_nocase(const char *str, const char *pat)
{
	return __match_glob(str, pat, false, true);
}

 
bool strlazymatch(const char *str, const char *pat)
{
	return __match_glob(str, pat, true, false);
}

 
int strtailcmp(const char *s1, const char *s2)
{
	int i1 = strlen(s1);
	int i2 = strlen(s2);
	while (--i1 >= 0 && --i2 >= 0) {
		if (s1[i1] != s2[i2])
			return s1[i1] - s2[i2];
	}
	return 0;
}

char *asprintf_expr_inout_ints(const char *var, bool in, size_t nints, int *ints)
{
	 
	size_t size = nints * 28 + 1;  
	size_t i, printed = 0;
	char *expr = malloc(size);

	if (expr) {
		const char *or_and = "||", *eq_neq = "==";
		char *e = expr;

		if (!in) {
			or_and = "&&";
			eq_neq = "!=";
		}

		for (i = 0; i < nints; ++i) {
			if (printed == size)
				goto out_err_overflow;

			if (i > 0)
				printed += scnprintf(e + printed, size - printed, " %s ", or_and);
			printed += scnprintf(e + printed, size - printed,
					     "%s %s %d", var, eq_neq, ints[i]);
		}
	}

	return expr;

out_err_overflow:
	free(expr);
	return NULL;
}

 
char *strpbrk_esc(char *str, const char *stopset)
{
	char *ptr;

	do {
		ptr = strpbrk(str, stopset);
		if (ptr == str ||
		    (ptr == str + 1 && *(ptr - 1) != '\\'))
			break;
		str = ptr + 1;
	} while (ptr && *(ptr - 1) == '\\' && *(ptr - 2) != '\\');

	return ptr;
}

 
char *strdup_esc(const char *str)
{
	char *s, *d, *p, *ret = strdup(str);

	if (!ret)
		return NULL;

	d = strchr(ret, '\\');
	if (!d)
		return ret;

	s = d + 1;
	do {
		if (*s == '\0') {
			*d = '\0';
			break;
		}
		p = strchr(s + 1, '\\');
		if (p) {
			memmove(d, s, p - s);
			d += p - s;
			s = p + 1;
		} else
			memmove(d, s, strlen(s) + 1);
	} while (p);

	return ret;
}

unsigned int hex(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return c - 'A' + 10;
}
