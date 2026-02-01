
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/types.h>
#include <linux/export.h>
#include <linux/sort.h>

 
__attribute_const__ __always_inline
static bool is_aligned(const void *base, size_t size, unsigned char align)
{
	unsigned char lsbits = (unsigned char)size;

	(void)base;
#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
	lsbits |= (unsigned char)(uintptr_t)base;
#endif
	return (lsbits & (align - 1)) == 0;
}

 
static void swap_words_32(void *a, void *b, size_t n)
{
	do {
		u32 t = *(u32 *)(a + (n -= 4));
		*(u32 *)(a + n) = *(u32 *)(b + n);
		*(u32 *)(b + n) = t;
	} while (n);
}

 
static void swap_words_64(void *a, void *b, size_t n)
{
	do {
#ifdef CONFIG_64BIT
		u64 t = *(u64 *)(a + (n -= 8));
		*(u64 *)(a + n) = *(u64 *)(b + n);
		*(u64 *)(b + n) = t;
#else
		 
		u32 t = *(u32 *)(a + (n -= 4));
		*(u32 *)(a + n) = *(u32 *)(b + n);
		*(u32 *)(b + n) = t;

		t = *(u32 *)(a + (n -= 4));
		*(u32 *)(a + n) = *(u32 *)(b + n);
		*(u32 *)(b + n) = t;
#endif
	} while (n);
}

 
static void swap_bytes(void *a, void *b, size_t n)
{
	do {
		char t = ((char *)a)[--n];
		((char *)a)[n] = ((char *)b)[n];
		((char *)b)[n] = t;
	} while (n);
}

 
#define SWAP_WORDS_64 (swap_r_func_t)0
#define SWAP_WORDS_32 (swap_r_func_t)1
#define SWAP_BYTES    (swap_r_func_t)2
#define SWAP_WRAPPER  (swap_r_func_t)3

struct wrapper {
	cmp_func_t cmp;
	swap_func_t swap;
};

 
static void do_swap(void *a, void *b, size_t size, swap_r_func_t swap_func, const void *priv)
{
	if (swap_func == SWAP_WRAPPER) {
		((const struct wrapper *)priv)->swap(a, b, (int)size);
		return;
	}

	if (swap_func == SWAP_WORDS_64)
		swap_words_64(a, b, size);
	else if (swap_func == SWAP_WORDS_32)
		swap_words_32(a, b, size);
	else if (swap_func == SWAP_BYTES)
		swap_bytes(a, b, size);
	else
		swap_func(a, b, (int)size, priv);
}

#define _CMP_WRAPPER ((cmp_r_func_t)0L)

static int do_cmp(const void *a, const void *b, cmp_r_func_t cmp, const void *priv)
{
	if (cmp == _CMP_WRAPPER)
		return ((const struct wrapper *)priv)->cmp(a, b);
	return cmp(a, b, priv);
}

 
__attribute_const__ __always_inline
static size_t parent(size_t i, unsigned int lsbit, size_t size)
{
	i -= size;
	i -= size & -(i & lsbit);
	return i / 2;
}

 
void sort_r(void *base, size_t num, size_t size,
	    cmp_r_func_t cmp_func,
	    swap_r_func_t swap_func,
	    const void *priv)
{
	 
	size_t n = num * size, a = (num/2) * size;
	const unsigned int lsbit = size & -size;   

	if (!a)		 
		return;

	 
	if (swap_func == SWAP_WRAPPER && !((struct wrapper *)priv)->swap)
		swap_func = NULL;

	if (!swap_func) {
		if (is_aligned(base, size, 8))
			swap_func = SWAP_WORDS_64;
		else if (is_aligned(base, size, 4))
			swap_func = SWAP_WORDS_32;
		else
			swap_func = SWAP_BYTES;
	}

	 
	for (;;) {
		size_t b, c, d;

		if (a)			 
			a -= size;
		else if (n -= size)	 
			do_swap(base, base + n, size, swap_func, priv);
		else			 
			break;

		 
		for (b = a; c = 2*b + size, (d = c + size) < n;)
			b = do_cmp(base + c, base + d, cmp_func, priv) >= 0 ? c : d;
		if (d == n)	 
			b = c;

		 
		while (b != a && do_cmp(base + a, base + b, cmp_func, priv) >= 0)
			b = parent(b, lsbit, size);
		c = b;			 
		while (b != a) {	 
			b = parent(b, lsbit, size);
			do_swap(base + b, base + c, size, swap_func, priv);
		}
	}
}
EXPORT_SYMBOL(sort_r);

void sort(void *base, size_t num, size_t size,
	  cmp_func_t cmp_func,
	  swap_func_t swap_func)
{
	struct wrapper w = {
		.cmp  = cmp_func,
		.swap = swap_func,
	};

	return sort_r(base, num, size, _CMP_WRAPPER, SWAP_WRAPPER, &w);
}
EXPORT_SYMBOL(sort);
