 
 

#ifndef _NOLIBC_STDLIB_H
#define _NOLIBC_STDLIB_H

#include "std.h"
#include "arch.h"
#include "types.h"
#include "sys.h"
#include "string.h"
#include <linux/auxvec.h>

struct nolibc_heap {
	size_t	len;
	char	user_p[] __attribute__((__aligned__));
};

 
static __attribute__((unused)) char itoa_buffer[21];

 

 
__attribute__((weak,unused,noreturn,section(".text.nolibc_abort")))
void abort(void)
{
	sys_kill(sys_getpid(), SIGABRT);
	for (;;);
}

static __attribute__((unused))
long atol(const char *s)
{
	unsigned long ret = 0;
	unsigned long d;
	int neg = 0;

	if (*s == '-') {
		neg = 1;
		s++;
	}

	while (1) {
		d = (*s++) - '0';
		if (d > 9)
			break;
		ret *= 10;
		ret += d;
	}

	return neg ? -ret : ret;
}

static __attribute__((unused))
int atoi(const char *s)
{
	return atol(s);
}

static __attribute__((unused))
void free(void *ptr)
{
	struct nolibc_heap *heap;

	if (!ptr)
		return;

	heap = container_of(ptr, struct nolibc_heap, user_p);
	munmap(heap, heap->len);
}

 
static __attribute__((unused))
char *getenv(const char *name)
{
	int idx, i;

	if (environ) {
		for (idx = 0; environ[idx]; idx++) {
			for (i = 0; name[i] && name[i] == environ[idx][i];)
				i++;
			if (!name[i] && environ[idx][i] == '=')
				return &environ[idx][i+1];
		}
	}
	return NULL;
}

static __attribute__((unused))
unsigned long getauxval(unsigned long type)
{
	const unsigned long *auxv = _auxv;
	unsigned long ret;

	if (!auxv)
		return 0;

	while (1) {
		if (!auxv[0] && !auxv[1]) {
			ret = 0;
			break;
		}

		if (auxv[0] == type) {
			ret = auxv[1];
			break;
		}

		auxv += 2;
	}

	return ret;
}

static __attribute__((unused))
void *malloc(size_t len)
{
	struct nolibc_heap *heap;

	 
	len  = sizeof(*heap) + len;
	len  = (len + 4095UL) & -4096UL;
	heap = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE,
		    -1, 0);
	if (__builtin_expect(heap == MAP_FAILED, 0))
		return NULL;

	heap->len = len;
	return heap->user_p;
}

static __attribute__((unused))
void *calloc(size_t size, size_t nmemb)
{
	size_t x = size * nmemb;

	if (__builtin_expect(size && ((x / size) != nmemb), 0)) {
		SET_ERRNO(ENOMEM);
		return NULL;
	}

	 
	return malloc(x);
}

static __attribute__((unused))
void *realloc(void *old_ptr, size_t new_size)
{
	struct nolibc_heap *heap;
	size_t user_p_len;
	void *ret;

	if (!old_ptr)
		return malloc(new_size);

	heap = container_of(old_ptr, struct nolibc_heap, user_p);
	user_p_len = heap->len - sizeof(*heap);
	 
	if (user_p_len >= new_size)
		return old_ptr;

	ret = malloc(new_size);
	if (__builtin_expect(!ret, 0))
		return NULL;

	memcpy(ret, heap->user_p, heap->len);
	munmap(heap, heap->len);
	return ret;
}

 
static __attribute__((unused))
int utoh_r(unsigned long in, char *buffer)
{
	signed char pos = (~0UL > 0xfffffffful) ? 60 : 28;
	int digits = 0;
	int dig;

	do {
		dig = in >> pos;
		in -= (uint64_t)dig << pos;
		pos -= 4;
		if (dig || digits || pos < 0) {
			if (dig > 9)
				dig += 'a' - '0' - 10;
			buffer[digits++] = '0' + dig;
		}
	} while (pos >= 0);

	buffer[digits] = 0;
	return digits;
}

 
static __inline__ __attribute__((unused))
char *utoh(unsigned long in)
{
	utoh_r(in, itoa_buffer);
	return itoa_buffer;
}

 
static __attribute__((unused))
int utoa_r(unsigned long in, char *buffer)
{
	unsigned long lim;
	int digits = 0;
	int pos = (~0UL > 0xfffffffful) ? 19 : 9;
	int dig;

	do {
		for (dig = 0, lim = 1; dig < pos; dig++)
			lim *= 10;

		if (digits || in >= lim || !pos) {
			for (dig = 0; in >= lim; dig++)
				in -= lim;
			buffer[digits++] = '0' + dig;
		}
	} while (pos--);

	buffer[digits] = 0;
	return digits;
}

 
static __attribute__((unused))
int itoa_r(long in, char *buffer)
{
	char *ptr = buffer;
	int len = 0;

	if (in < 0) {
		in = -in;
		*(ptr++) = '-';
		len++;
	}
	len += utoa_r(in, ptr);
	return len;
}

 
static __inline__ __attribute__((unused))
char *ltoa_r(long in, char *buffer)
{
	itoa_r(in, buffer);
	return buffer;
}

 
static __inline__ __attribute__((unused))
char *itoa(long in)
{
	itoa_r(in, itoa_buffer);
	return itoa_buffer;
}

 
static __inline__ __attribute__((unused))
char *ltoa(long in)
{
	itoa_r(in, itoa_buffer);
	return itoa_buffer;
}

 
static __inline__ __attribute__((unused))
char *utoa(unsigned long in)
{
	utoa_r(in, itoa_buffer);
	return itoa_buffer;
}

 
static __attribute__((unused))
int u64toh_r(uint64_t in, char *buffer)
{
	signed char pos = 60;
	int digits = 0;
	int dig;

	do {
		if (sizeof(long) >= 8) {
			dig = (in >> pos) & 0xF;
		} else {
			 
			uint32_t d = (pos >= 32) ? (in >> 32) : in;
			dig = (d >> (pos & 31)) & 0xF;
		}
		if (dig > 9)
			dig += 'a' - '0' - 10;
		pos -= 4;
		if (dig || digits || pos < 0)
			buffer[digits++] = '0' + dig;
	} while (pos >= 0);

	buffer[digits] = 0;
	return digits;
}

 
static __inline__ __attribute__((unused))
char *u64toh(uint64_t in)
{
	u64toh_r(in, itoa_buffer);
	return itoa_buffer;
}

 
static __attribute__((unused))
int u64toa_r(uint64_t in, char *buffer)
{
	unsigned long long lim;
	int digits = 0;
	int pos = 19;  
	int dig;

	do {
		for (dig = 0, lim = 1; dig < pos; dig++)
			lim *= 10;

		if (digits || in >= lim || !pos) {
			for (dig = 0; in >= lim; dig++)
				in -= lim;
			buffer[digits++] = '0' + dig;
		}
	} while (pos--);

	buffer[digits] = 0;
	return digits;
}

 
static __attribute__((unused))
int i64toa_r(int64_t in, char *buffer)
{
	char *ptr = buffer;
	int len = 0;

	if (in < 0) {
		in = -in;
		*(ptr++) = '-';
		len++;
	}
	len += u64toa_r(in, ptr);
	return len;
}

 
static __inline__ __attribute__((unused))
char *i64toa(int64_t in)
{
	i64toa_r(in, itoa_buffer);
	return itoa_buffer;
}

 
static __inline__ __attribute__((unused))
char *u64toa(uint64_t in)
{
	u64toa_r(in, itoa_buffer);
	return itoa_buffer;
}

 
#include "nolibc.h"

#endif  
