#ifndef _ASM_WORD_AT_A_TIME_H
#define _ASM_WORD_AT_A_TIME_H
#include <linux/kernel.h>
#include <asm/asm-compat.h>
#include <asm/extable.h>
#ifdef __BIG_ENDIAN__
struct word_at_a_time {
	const unsigned long high_bits, low_bits;
};
#define WORD_AT_A_TIME_CONSTANTS { REPEAT_BYTE(0xfe) + 1, REPEAT_BYTE(0x7f) }
static inline long prep_zero_mask(unsigned long val, unsigned long rhs, const struct word_at_a_time *c)
{
	unsigned long mask = (val & c->low_bits) + c->low_bits;
	return ~(mask | rhs);
}
#define create_zero_mask(mask) (mask)
static inline long find_zero(unsigned long mask)
{
	long leading_zero_bits;
	asm (PPC_CNTLZL "%0,%1" : "=r" (leading_zero_bits) : "r" (mask));
	return leading_zero_bits >> 3;
}
static inline unsigned long has_zero(unsigned long val, unsigned long *data, const struct word_at_a_time *c)
{
	unsigned long rhs = val | c->low_bits;
	*data = rhs;
	return (val + c->high_bits) & ~rhs;
}
static inline unsigned long zero_bytemask(unsigned long mask)
{
	return ~1ul << __fls(mask);
}
#else
#ifdef CONFIG_64BIT
struct word_at_a_time {
};
#define WORD_AT_A_TIME_CONSTANTS { }
static inline unsigned long has_zero(unsigned long a, unsigned long *bits, const struct word_at_a_time *c)
{
	unsigned long ret;
	unsigned long zero = 0;
	asm("cmpb %0,%1,%2" : "=r" (ret) : "r" (a), "r" (zero));
	*bits = ret;
	return ret;
}
static inline unsigned long prep_zero_mask(unsigned long a, unsigned long bits, const struct word_at_a_time *c)
{
	return bits;
}
static inline unsigned long create_zero_mask(unsigned long bits)
{
	unsigned long leading_zero_bits;
	long trailing_zero_bit_mask;
	asm("addi	%1,%2,-1\n\t"
	    "andc	%1,%1,%2\n\t"
	    "popcntd	%0,%1"
		: "=r" (leading_zero_bits), "=&r" (trailing_zero_bit_mask)
		: "b" (bits));
	return leading_zero_bits;
}
static inline unsigned long find_zero(unsigned long mask)
{
	return mask >> 3;
}
static inline unsigned long zero_bytemask(unsigned long mask)
{
	return (1UL << mask) - 1;
}
#else	 
struct word_at_a_time {
	const unsigned long one_bits, high_bits;
};
#define WORD_AT_A_TIME_CONSTANTS { REPEAT_BYTE(0x01), REPEAT_BYTE(0x80) }
static inline long count_masked_bytes(long mask)
{
	long a = (0x0ff0001+mask) >> 23;
	return a & mask;
}
static inline unsigned long create_zero_mask(unsigned long bits)
{
	bits = (bits - 1) & ~bits;
	return bits >> 7;
}
static inline unsigned long find_zero(unsigned long mask)
{
	return count_masked_bytes(mask);
}
static inline unsigned long has_zero(unsigned long a, unsigned long *bits, const struct word_at_a_time *c)
{
	unsigned long mask = ((a - c->one_bits) & ~a) & c->high_bits;
	*bits = mask;
	return mask;
}
static inline unsigned long prep_zero_mask(unsigned long a, unsigned long bits, const struct word_at_a_time *c)
{
	return bits;
}
#define zero_bytemask(mask) (mask)
#endif  
#endif  
#ifndef FIXUP_SECTION
#define FIXUP_SECTION ".fixup"
#endif
static inline unsigned long load_unaligned_zeropad(const void *addr)
{
	unsigned long ret, offset, tmp;
	asm(
	"1:	" PPC_LL "%[ret], 0(%[addr])\n"
	"2:\n"
	".section " FIXUP_SECTION ",\"ax\"\n"
	"3:	"
#ifdef __powerpc64__
	"clrrdi		%[tmp], %[addr], 3\n\t"
	"clrlsldi	%[offset], %[addr], 61, 3\n\t"
	"ld		%[ret], 0(%[tmp])\n\t"
#ifdef __BIG_ENDIAN__
	"sld		%[ret], %[ret], %[offset]\n\t"
#else
	"srd		%[ret], %[ret], %[offset]\n\t"
#endif
#else
	"clrrwi		%[tmp], %[addr], 2\n\t"
	"clrlslwi	%[offset], %[addr], 30, 3\n\t"
	"lwz		%[ret], 0(%[tmp])\n\t"
#ifdef __BIG_ENDIAN__
	"slw		%[ret], %[ret], %[offset]\n\t"
#else
	"srw		%[ret], %[ret], %[offset]\n\t"
#endif
#endif
	"b	2b\n"
	".previous\n"
	EX_TABLE(1b, 3b)
	: [tmp] "=&b" (tmp), [offset] "=&r" (offset), [ret] "=&r" (ret)
	: [addr] "b" (addr), "m" (*(unsigned long *)addr));
	return ret;
}
#undef FIXUP_SECTION
#endif  
