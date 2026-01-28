#ifndef __MATH_SUPPORT_H
#define __MATH_SUPPORT_H
#include <linux/kernel.h>  
#define IS_ODD(a)            ((a) & 0x1)
#define IS_EVEN(a)           (!IS_ODD(a))
#define EVEN_FLOOR(x)        ((x) & ~1)
#define EVEN_CEIL(x)         ((IS_ODD(x)) ? ((x) + 1) : (x))
#define IMPLIES(a, b)        (!(a) || (b))
#define MAX(a, b)            (((a) > (b)) ? (a) : (b))
#define MIN(a, b)            (((a) < (b)) ? (a) : (b))
#define ROUND_DIV(a, b)      (((b) != 0) ? ((a) + ((b) >> 1)) / (b) : 0)
#define CEIL_DIV(a, b)       (((b) != 0) ? ((a) + (b) - 1) / (b) : 0)
#define CEIL_MUL(a, b)       (CEIL_DIV(a, b) * (b))
#define CEIL_MUL2(a, b)      (((a) + (b) - 1) & ~((b) - 1))
#define CEIL_SHIFT(a, b)     (((a) + (1 << (b)) - 1) >> (b))
#define CEIL_SHIFT_MUL(a, b) (CEIL_SHIFT(a, b) << (b))
#define ROUND_HALF_DOWN_DIV(a, b)	(((b) != 0) ? ((a) + (b / 2) - 1) / (b) : 0)
#define ROUND_HALF_DOWN_MUL(a, b)	(ROUND_HALF_DOWN_DIV(a, b) * (b))
#define bit2(x)            ((x)      | ((x) >> 1))
#define bit4(x)            (bit2(x)  | (bit2(x) >> 2))
#define bit8(x)            (bit4(x)  | (bit4(x) >> 4))
#define bit16(x)           (bit8(x)  | (bit8(x) >> 8))
#define bit32(x)           (bit16(x) | (bit16(x) >> 16))
#define NEXT_POWER_OF_2(x) (bit32(x - 1) + 1)
#if !defined(PIPE_GENERATION)
#define ceil_div(a, b)		(CEIL_DIV(a, b))
static inline unsigned int ceil_mul(unsigned int a, unsigned int b)
{
	return CEIL_MUL(a, b);
}
static inline unsigned int ceil_mul2(unsigned int a, unsigned int b)
{
	return CEIL_MUL2(a, b);
}
static inline unsigned int ceil_shift(unsigned int a, unsigned int b)
{
	return CEIL_SHIFT(a, b);
}
static inline unsigned int ceil_shift_mul(unsigned int a, unsigned int b)
{
	return CEIL_SHIFT_MUL(a, b);
}
static inline unsigned int round_half_down_div(unsigned int a, unsigned int b)
{
	return ROUND_HALF_DOWN_DIV(a, b);
}
static inline unsigned int round_half_down_mul(unsigned int a, unsigned int b)
{
	return ROUND_HALF_DOWN_MUL(a, b);
}
static inline unsigned int ceil_pow2(unsigned int a)
{
	if (a == 0) {
		return 1;
	}
	else if ((!((a) & ((a) - 1)))) {
		return a;
	} else {
		unsigned int v = a;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		return (v + 1);
	}
}
#endif  
#define OP_std_modadd(base, offset, size) ((base + offset) % (size))
#endif  
