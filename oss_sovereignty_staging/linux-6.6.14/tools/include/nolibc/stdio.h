 
 

#ifndef _NOLIBC_STDIO_H
#define _NOLIBC_STDIO_H

#include <stdarg.h>

#include "std.h"
#include "arch.h"
#include "errno.h"
#include "types.h"
#include "sys.h"
#include "stdlib.h"
#include "string.h"

#ifndef EOF
#define EOF (-1)
#endif

 
#define _IOFBF 0	 
#define _IOLBF 1	 
#define _IONBF 2	 

 
typedef struct FILE {
	char dummy[1];
} FILE;

static __attribute__((unused)) FILE* const stdin  = (FILE*)(intptr_t)~STDIN_FILENO;
static __attribute__((unused)) FILE* const stdout = (FILE*)(intptr_t)~STDOUT_FILENO;
static __attribute__((unused)) FILE* const stderr = (FILE*)(intptr_t)~STDERR_FILENO;

 
static __attribute__((unused))
FILE *fdopen(int fd, const char *mode __attribute__((unused)))
{
	if (fd < 0) {
		SET_ERRNO(EBADF);
		return NULL;
	}
	return (FILE*)(intptr_t)~fd;
}

 
static __attribute__((unused))
int fileno(FILE *stream)
{
	intptr_t i = (intptr_t)stream;

	if (i >= 0) {
		SET_ERRNO(EBADF);
		return -1;
	}
	return ~i;
}

 
static __attribute__((unused))
int fflush(FILE *stream)
{
	intptr_t i = (intptr_t)stream;

	 
	if (i > 0) {
		SET_ERRNO(EBADF);
		return -1;
	}

	 
	return 0;
}

 
static __attribute__((unused))
int fclose(FILE *stream)
{
	intptr_t i = (intptr_t)stream;

	if (i >= 0) {
		SET_ERRNO(EBADF);
		return -1;
	}

	if (close(~i))
		return EOF;

	return 0;
}

 

#define getc(stream) fgetc(stream)

static __attribute__((unused))
int fgetc(FILE* stream)
{
	unsigned char ch;

	if (read(fileno(stream), &ch, 1) <= 0)
		return EOF;
	return ch;
}

static __attribute__((unused))
int getchar(void)
{
	return fgetc(stdin);
}


 

#define putc(c, stream) fputc(c, stream)

static __attribute__((unused))
int fputc(int c, FILE* stream)
{
	unsigned char ch = c;

	if (write(fileno(stream), &ch, 1) <= 0)
		return EOF;
	return ch;
}

static __attribute__((unused))
int putchar(int c)
{
	return fputc(c, stdout);
}


 

 
static __attribute__((unused))
int _fwrite(const void *buf, size_t size, FILE *stream)
{
	ssize_t ret;
	int fd = fileno(stream);

	while (size) {
		ret = write(fd, buf, size);
		if (ret <= 0)
			return EOF;
		size -= ret;
		buf += ret;
	}
	return 0;
}

static __attribute__((unused))
size_t fwrite(const void *s, size_t size, size_t nmemb, FILE *stream)
{
	size_t written;

	for (written = 0; written < nmemb; written++) {
		if (_fwrite(s, size, stream) != 0)
			break;
		s += size;
	}
	return written;
}

static __attribute__((unused))
int fputs(const char *s, FILE *stream)
{
	return _fwrite(s, strlen(s), stream);
}

static __attribute__((unused))
int puts(const char *s)
{
	if (fputs(s, stdout) == EOF)
		return EOF;
	return putchar('\n');
}


 
static __attribute__((unused))
char *fgets(char *s, int size, FILE *stream)
{
	int ofs;
	int c;

	for (ofs = 0; ofs + 1 < size;) {
		c = fgetc(stream);
		if (c == EOF)
			break;
		s[ofs++] = c;
		if (c == '\n')
			break;
	}
	if (ofs < size)
		s[ofs] = 0;
	return ofs ? s : NULL;
}


 
static __attribute__((unused))
int vfprintf(FILE *stream, const char *fmt, va_list args)
{
	char escape, lpref, c;
	unsigned long long v;
	unsigned int written;
	size_t len, ofs;
	char tmpbuf[21];
	const char *outstr;

	written = ofs = escape = lpref = 0;
	while (1) {
		c = fmt[ofs++];

		if (escape) {
			 
			escape = 0;
			if (c == 'c' || c == 'd' || c == 'u' || c == 'x' || c == 'p') {
				char *out = tmpbuf;

				if (c == 'p')
					v = va_arg(args, unsigned long);
				else if (lpref) {
					if (lpref > 1)
						v = va_arg(args, unsigned long long);
					else
						v = va_arg(args, unsigned long);
				} else
					v = va_arg(args, unsigned int);

				if (c == 'd') {
					 
					if (lpref == 0)
						v = (long long)(int)v;
					else if (lpref == 1)
						v = (long long)(long)v;
				}

				switch (c) {
				case 'c':
					out[0] = v;
					out[1] = 0;
					break;
				case 'd':
					i64toa_r(v, out);
					break;
				case 'u':
					u64toa_r(v, out);
					break;
				case 'p':
					*(out++) = '0';
					*(out++) = 'x';
					 
				default:  
					u64toh_r(v, out);
					break;
				}
				outstr = tmpbuf;
			}
			else if (c == 's') {
				outstr = va_arg(args, char *);
				if (!outstr)
					outstr="(null)";
			}
			else if (c == '%') {
				 
				continue;
			}
			else {
				 
				if (c == 'l') {
					 
					lpref++;
				}
				escape = 1;
				goto do_escape;
			}
			len = strlen(outstr);
			goto flush_str;
		}

		 
		if (c == 0 || c == '%') {
			 
			escape = 1;
			lpref = 0;
			outstr = fmt;
			len = ofs - 1;
		flush_str:
			if (_fwrite(outstr, len, stream) != 0)
				break;

			written += len;
		do_escape:
			if (c == 0)
				break;
			fmt += ofs;
			ofs = 0;
			continue;
		}

		 
	}
	return written;
}

static __attribute__((unused))
int vprintf(const char *fmt, va_list args)
{
	return vfprintf(stdout, fmt, args);
}

static __attribute__((unused, format(printf, 2, 3)))
int fprintf(FILE *stream, const char *fmt, ...)
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vfprintf(stream, fmt, args);
	va_end(args);
	return ret;
}

static __attribute__((unused, format(printf, 1, 2)))
int printf(const char *fmt, ...)
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vfprintf(stdout, fmt, args);
	va_end(args);
	return ret;
}

static __attribute__((unused))
void perror(const char *msg)
{
	fprintf(stderr, "%s%serrno=%d\n", (msg && *msg) ? msg : "", (msg && *msg) ? ": " : "", errno);
}

static __attribute__((unused))
int setvbuf(FILE *stream __attribute__((unused)),
	    char *buf __attribute__((unused)),
	    int mode,
	    size_t size __attribute__((unused)))
{
	 
	switch (mode) {
	case _IOFBF:
	case _IOLBF:
	case _IONBF:
		break;
	default:
		return EOF;
	}

	return 0;
}

 
#include "nolibc.h"

#endif  
