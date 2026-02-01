
 

 

#include <linux/stdarg.h>

#include <linux/compiler.h>
#include <linux/ctype.h>
#include <linux/kernel.h>
#include <linux/limits.h>
#include <linux/string.h>
#include <linux/types.h>

static
int skip_atoi(const char **s)
{
	int i = 0;

	while (isdigit(**s))
		i = i * 10 + *((*s)++) - '0';
	return i;
}

 
static
void put_dec_full4(char *end, unsigned int r)
{
	int i;

	for (i = 0; i < 3; i++) {
		unsigned int q = (r * 0xccd) >> 15;
		*--end = '0' + (r - q * 10);
		r = q;
	}
	*--end = '0' + r;
}

 

 
static
unsigned int put_dec_helper4(char *end, unsigned int x)
{
	unsigned int q = (x * 0x346DC5D7ULL) >> 43;

	put_dec_full4(end, x - q * 10000);
	return q;
}

 
static
char *put_dec(char *end, unsigned long long n)
{
	unsigned int d3, d2, d1, q, h;
	char *p = end;

	d1  = ((unsigned int)n >> 16);  
	h   = (n >> 32);
	d2  = (h      ) & 0xffff;
	d3  = (h >> 16);  

	 
	q = 656 * d3 + 7296 * d2 + 5536 * d1 + ((unsigned int)n & 0xffff);
	q = put_dec_helper4(p, q);
	p -= 4;

	q += 7671 * d3 + 9496 * d2 + 6 * d1;
	q = put_dec_helper4(p, q);
	p -= 4;

	q += 4749 * d3 + 42 * d2;
	q = put_dec_helper4(p, q);
	p -= 4;

	q += 281 * d3;
	q = put_dec_helper4(p, q);
	p -= 4;

	put_dec_full4(p, q);
	p -= 4;

	 
	while (p < end && *p == '0')
		++p;

	return p;
}

static
char *number(char *end, unsigned long long num, int base, char locase)
{
	 

	 
	static const char digits[16] = "0123456789ABCDEF";  

	switch (base) {
	case 10:
		if (num != 0)
			end = put_dec(end, num);
		break;
	case 8:
		for (; num != 0; num >>= 3)
			*--end = '0' + (num & 07);
		break;
	case 16:
		for (; num != 0; num >>= 4)
			*--end = digits[num & 0xf] | locase;
		break;
	default:
		unreachable();
	}

	return end;
}

#define ZEROPAD	1		 
#define SIGN	2		 
#define PLUS	4		 
#define SPACE	8		 
#define LEFT	16		 
#define SMALL	32		 
#define SPECIAL	64		 
#define WIDE	128		 

static
int get_flags(const char **fmt)
{
	int flags = 0;

	do {
		switch (**fmt) {
		case '-':
			flags |= LEFT;
			break;
		case '+':
			flags |= PLUS;
			break;
		case ' ':
			flags |= SPACE;
			break;
		case '#':
			flags |= SPECIAL;
			break;
		case '0':
			flags |= ZEROPAD;
			break;
		default:
			return flags;
		}
		++(*fmt);
	} while (1);
}

static
int get_int(const char **fmt, va_list *ap)
{
	if (isdigit(**fmt))
		return skip_atoi(fmt);
	if (**fmt == '*') {
		++(*fmt);
		 
		return va_arg(*ap, int);
	}
	return 0;
}

static
unsigned long long get_number(int sign, int qualifier, va_list *ap)
{
	if (sign) {
		switch (qualifier) {
		case 'L':
			return va_arg(*ap, long long);
		case 'l':
			return va_arg(*ap, long);
		case 'h':
			return (short)va_arg(*ap, int);
		case 'H':
			return (signed char)va_arg(*ap, int);
		default:
			return va_arg(*ap, int);
		};
	} else {
		switch (qualifier) {
		case 'L':
			return va_arg(*ap, unsigned long long);
		case 'l':
			return va_arg(*ap, unsigned long);
		case 'h':
			return (unsigned short)va_arg(*ap, int);
		case 'H':
			return (unsigned char)va_arg(*ap, int);
		default:
			return va_arg(*ap, unsigned int);
		}
	}
}

static
char get_sign(long long *num, int flags)
{
	if (!(flags & SIGN))
		return 0;
	if (*num < 0) {
		*num = -(*num);
		return '-';
	}
	if (flags & PLUS)
		return '+';
	if (flags & SPACE)
		return ' ';
	return 0;
}

static
size_t utf16s_utf8nlen(const u16 *s16, size_t maxlen)
{
	size_t len, clen;

	for (len = 0; len < maxlen && *s16; len += clen) {
		u16 c0 = *s16++;

		 
		clen = 1 + (c0 >= 0x80) + (c0 >= 0x800);
		if (len + clen > maxlen)
			break;
		 
		if ((c0 & 0xfc00) == 0xd800) {
			if (len + clen == maxlen)
				break;
			if ((*s16 & 0xfc00) == 0xdc00) {
				++s16;
				++clen;
			}
		}
	}

	return len;
}

static
u32 utf16_to_utf32(const u16 **s16)
{
	u16 c0, c1;

	c0 = *(*s16)++;
	 
	if ((c0 & 0xf800) != 0xd800)
		return c0;
	 
	if (c0 & 0x0400)
		return 0xfffd;
	c1 = **s16;
	 
	if ((c1 & 0xfc00) != 0xdc00)
		return 0xfffd;
	 
	++(*s16);
	return (0x10000 - (0xd800 << 10) - 0xdc00) + (c0 << 10) + c1;
}

#define PUTC(c) \
do {				\
	if (pos < size)		\
		buf[pos] = (c);	\
	++pos;			\
} while (0);

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	 
	char tmp[(sizeof(unsigned long long) * 8 + 2) / 3];
	char *tmp_end = &tmp[ARRAY_SIZE(tmp)];
	long long num;
	int base;
	const char *s;
	size_t len, pos;
	char sign;

	int flags;		 

	int field_width;	 
	int precision;		 
	int qualifier;		 

	va_list args;

	 
	va_copy(args, ap);

	for (pos = 0; *fmt; ++fmt) {
		if (*fmt != '%' || *++fmt == '%') {
			PUTC(*fmt);
			continue;
		}

		 
		flags = get_flags(&fmt);

		 
		field_width = get_int(&fmt, &args);
		if (field_width < 0) {
			field_width = -field_width;
			flags |= LEFT;
		}

		if (flags & LEFT)
			flags &= ~ZEROPAD;

		 
		precision = -1;
		if (*fmt == '.') {
			++fmt;
			precision = get_int(&fmt, &args);
			if (precision >= 0)
				flags &= ~ZEROPAD;
		}

		 
		qualifier = -1;
		if (*fmt == 'h' || *fmt == 'l') {
			qualifier = *fmt;
			++fmt;
			if (qualifier == *fmt) {
				qualifier -= 'a'-'A';
				++fmt;
			}
		}

		sign = 0;

		switch (*fmt) {
		case 'c':
			flags &= LEFT;
			s = tmp;
			if (qualifier == 'l') {
				((u16 *)tmp)[0] = (u16)va_arg(args, unsigned int);
				((u16 *)tmp)[1] = L'\0';
				precision = INT_MAX;
				goto wstring;
			} else {
				tmp[0] = (unsigned char)va_arg(args, int);
				precision = len = 1;
			}
			goto output;

		case 's':
			flags &= LEFT;
			if (precision < 0)
				precision = INT_MAX;
			s = va_arg(args, void *);
			if (!s)
				s = precision < 6 ? "" : "(null)";
			else if (qualifier == 'l') {
		wstring:
				flags |= WIDE;
				precision = len = utf16s_utf8nlen((const u16 *)s, precision);
				goto output;
			}
			precision = len = strnlen(s, precision);
			goto output;

			 
		case 'o':
			base = 8;
			break;

		case 'p':
			if (precision < 0)
				precision = 2 * sizeof(void *);
			fallthrough;
		case 'x':
			flags |= SMALL;
			fallthrough;
		case 'X':
			base = 16;
			break;

		case 'd':
		case 'i':
			flags |= SIGN;
			fallthrough;
		case 'u':
			flags &= ~SPECIAL;
			base = 10;
			break;

		default:
			 
			goto fail;
		}
		if (*fmt == 'p') {
			num = (unsigned long)va_arg(args, void *);
		} else {
			num = get_number(flags & SIGN, qualifier, &args);
		}

		sign = get_sign(&num, flags);
		if (sign)
			--field_width;

		s = number(tmp_end, num, base, flags & SMALL);
		len = tmp_end - s;
		 
		if (precision < 0)
			precision = 1;
		 
		if (precision < len)
			precision = len;
		if (flags & SPECIAL) {
			 
			if (base == 8 && precision == len)
				++precision;
			 
			if (base == 16 && precision > 0)
				field_width -= 2;
			else
				flags &= ~SPECIAL;
		}
		 
		if ((flags & ZEROPAD) && field_width > precision)
			precision = field_width;

output:
		 
		field_width -= precision;
		 
		if (!(flags & LEFT))
			while (field_width-- > 0)
				PUTC(' ');
		 
		if (sign)
			PUTC(sign);
		 
		if (flags & SPECIAL) {
			PUTC('0');
			PUTC( 'X' | (flags & SMALL));
		}
		 
		while (precision-- > len)
			PUTC('0');
		 
		if (flags & WIDE) {
			const u16 *ws = (const u16 *)s;

			while (len-- > 0) {
				u32 c32 = utf16_to_utf32(&ws);
				u8 *s8;
				size_t clen;

				if (c32 < 0x80) {
					PUTC(c32);
					continue;
				}

				 
				clen = 1 + (c32 >= 0x800) + (c32 >= 0x10000);

				len -= clen;
				s8 = (u8 *)&buf[pos];

				 
				PUTC('\0');
				pos += clen;
				if (pos >= size)
					continue;

				 
				*s8 = (0xf00 >> 1) >> clen;
				 
				for (s8 += clen; clen; --clen, c32 >>= 6)
					*s8-- = 0x80 | (c32 & 0x3f);
				 
				*s8 |= c32;
			}
		} else {
			while (len-- > 0)
				PUTC(*s++);
		}
		 
		while (field_width-- > 0)
			PUTC(' ');
	}
fail:
	va_end(args);

	if (size)
		buf[min(pos, size-1)] = '\0';

	return pos;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsnprintf(buf, size, fmt, args);
	va_end(args);
	return i;
}
