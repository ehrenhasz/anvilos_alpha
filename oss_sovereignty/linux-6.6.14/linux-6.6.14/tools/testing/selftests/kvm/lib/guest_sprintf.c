#include "test_util.h"
#include "kvm_util.h"
#include "ucall_common.h"
#define APPEND_BUFFER_SAFE(str, end, v) \
do {					\
	GUEST_ASSERT(str < end);	\
	*str++ = (v);			\
} while (0)
static int isdigit(int ch)
{
	return (ch >= '0') && (ch <= '9');
}
static int skip_atoi(const char **s)
{
	int i = 0;
	while (isdigit(**s))
		i = i * 10 + *((*s)++) - '0';
	return i;
}
#define ZEROPAD	1		 
#define SIGN	2		 
#define PLUS	4		 
#define SPACE	8		 
#define LEFT	16		 
#define SMALL	32		 
#define SPECIAL	64		 
#define __do_div(n, base)				\
({							\
	int __res;					\
							\
	__res = ((uint64_t) n) % (uint32_t) base;	\
	n = ((uint64_t) n) / (uint32_t) base;		\
	__res;						\
})
static char *number(char *str, const char *end, long num, int base, int size,
		    int precision, int type)
{
	static const char digits[16] = "0123456789ABCDEF";  
	char tmp[66];
	char c, sign, locase;
	int i;
	locase = (type & SMALL);
	if (type & LEFT)
		type &= ~ZEROPAD;
	if (base < 2 || base > 16)
		return NULL;
	c = (type & ZEROPAD) ? '0' : ' ';
	sign = 0;
	if (type & SIGN) {
		if (num < 0) {
			sign = '-';
			num = -num;
			size--;
		} else if (type & PLUS) {
			sign = '+';
			size--;
		} else if (type & SPACE) {
			sign = ' ';
			size--;
		}
	}
	if (type & SPECIAL) {
		if (base == 16)
			size -= 2;
		else if (base == 8)
			size--;
	}
	i = 0;
	if (num == 0)
		tmp[i++] = '0';
	else
		while (num != 0)
			tmp[i++] = (digits[__do_div(num, base)] | locase);
	if (i > precision)
		precision = i;
	size -= precision;
	if (!(type & (ZEROPAD + LEFT)))
		while (size-- > 0)
			APPEND_BUFFER_SAFE(str, end, ' ');
	if (sign)
		APPEND_BUFFER_SAFE(str, end, sign);
	if (type & SPECIAL) {
		if (base == 8)
			APPEND_BUFFER_SAFE(str, end, '0');
		else if (base == 16) {
			APPEND_BUFFER_SAFE(str, end, '0');
			APPEND_BUFFER_SAFE(str, end, 'x');
		}
	}
	if (!(type & LEFT))
		while (size-- > 0)
			APPEND_BUFFER_SAFE(str, end, c);
	while (i < precision--)
		APPEND_BUFFER_SAFE(str, end, '0');
	while (i-- > 0)
		APPEND_BUFFER_SAFE(str, end, tmp[i]);
	while (size-- > 0)
		APPEND_BUFFER_SAFE(str, end, ' ');
	return str;
}
int guest_vsnprintf(char *buf, int n, const char *fmt, va_list args)
{
	char *str, *end;
	const char *s;
	uint64_t num;
	int i, base;
	int len;
	int flags;		 
	int field_width;	 
	int precision;		 
	int qualifier;		 
	end = buf + n;
	GUEST_ASSERT(buf < end);
	GUEST_ASSERT(n > 0);
	for (str = buf; *fmt; ++fmt) {
		if (*fmt != '%') {
			APPEND_BUFFER_SAFE(str, end, *fmt);
			continue;
		}
		flags = 0;
repeat:
		++fmt;		 
		switch (*fmt) {
		case '-':
			flags |= LEFT;
			goto repeat;
		case '+':
			flags |= PLUS;
			goto repeat;
		case ' ':
			flags |= SPACE;
			goto repeat;
		case '#':
			flags |= SPECIAL;
			goto repeat;
		case '0':
			flags |= ZEROPAD;
			goto repeat;
		}
		field_width = -1;
		if (isdigit(*fmt))
			field_width = skip_atoi(&fmt);
		else if (*fmt == '*') {
			++fmt;
			field_width = va_arg(args, int);
			if (field_width < 0) {
				field_width = -field_width;
				flags |= LEFT;
			}
		}
		precision = -1;
		if (*fmt == '.') {
			++fmt;
			if (isdigit(*fmt))
				precision = skip_atoi(&fmt);
			else if (*fmt == '*') {
				++fmt;
				precision = va_arg(args, int);
			}
			if (precision < 0)
				precision = 0;
		}
		qualifier = -1;
		if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
			qualifier = *fmt;
			++fmt;
		}
		if (qualifier == 'l' && *fmt == 'l')
			++fmt;
		base = 10;
		switch (*fmt) {
		case 'c':
			if (!(flags & LEFT))
				while (--field_width > 0)
					APPEND_BUFFER_SAFE(str, end, ' ');
			APPEND_BUFFER_SAFE(str, end,
					    (uint8_t)va_arg(args, int));
			while (--field_width > 0)
				APPEND_BUFFER_SAFE(str, end, ' ');
			continue;
		case 's':
			s = va_arg(args, char *);
			len = strnlen(s, precision);
			if (!(flags & LEFT))
				while (len < field_width--)
					APPEND_BUFFER_SAFE(str, end, ' ');
			for (i = 0; i < len; ++i)
				APPEND_BUFFER_SAFE(str, end, *s++);
			while (len < field_width--)
				APPEND_BUFFER_SAFE(str, end, ' ');
			continue;
		case 'p':
			if (field_width == -1) {
				field_width = 2 * sizeof(void *);
				flags |= SPECIAL | SMALL | ZEROPAD;
			}
			str = number(str, end,
				     (uint64_t)va_arg(args, void *), 16,
				     field_width, precision, flags);
			continue;
		case 'n':
			if (qualifier == 'l') {
				long *ip = va_arg(args, long *);
				*ip = (str - buf);
			} else {
				int *ip = va_arg(args, int *);
				*ip = (str - buf);
			}
			continue;
		case '%':
			APPEND_BUFFER_SAFE(str, end, '%');
			continue;
		case 'o':
			base = 8;
			break;
		case 'x':
			flags |= SMALL;
		case 'X':
			base = 16;
			break;
		case 'd':
		case 'i':
			flags |= SIGN;
		case 'u':
			break;
		default:
			APPEND_BUFFER_SAFE(str, end, '%');
			if (*fmt)
				APPEND_BUFFER_SAFE(str, end, *fmt);
			else
				--fmt;
			continue;
		}
		if (qualifier == 'l')
			num = va_arg(args, uint64_t);
		else if (qualifier == 'h') {
			num = (uint16_t)va_arg(args, int);
			if (flags & SIGN)
				num = (int16_t)num;
		} else if (flags & SIGN)
			num = va_arg(args, int);
		else
			num = va_arg(args, uint32_t);
		str = number(str, end, num, base, field_width, precision, flags);
	}
	GUEST_ASSERT(str < end);
	*str = '\0';
	return str - buf;
}
int guest_snprintf(char *buf, int n, const char *fmt, ...)
{
	va_list va;
	int len;
	va_start(va, fmt);
	len = guest_vsnprintf(buf, n, fmt, va);
	va_end(va);
	return len;
}
