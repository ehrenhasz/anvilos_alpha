 
#ifndef _LINUX_MINMAX_H
#define _LINUX_MINMAX_H

#include <linux/const.h>
#include <linux/types.h>

 
#define __typecheck(x, y) \
	(!!(sizeof((typeof(x) *)1 == (typeof(y) *)1)))

#define __no_side_effects(x, y) \
		(__is_constexpr(x) && __is_constexpr(y))

#define __safe_cmp(x, y) \
		(__typecheck(x, y) && __no_side_effects(x, y))

#define __cmp(x, y, op)	((x) op (y) ? (x) : (y))

#define __cmp_once(x, y, unique_x, unique_y, op) ({	\
		typeof(x) unique_x = (x);		\
		typeof(y) unique_y = (y);		\
		__cmp(unique_x, unique_y, op); })

#define __careful_cmp(x, y, op) \
	__builtin_choose_expr(__safe_cmp(x, y), \
		__cmp(x, y, op), \
		__cmp_once(x, y, __UNIQUE_ID(__x), __UNIQUE_ID(__y), op))

#define __clamp(val, lo, hi)	\
	((val) >= (hi) ? (hi) : ((val) <= (lo) ? (lo) : (val)))

#define __clamp_once(val, lo, hi, unique_val, unique_lo, unique_hi) ({	\
		typeof(val) unique_val = (val);				\
		typeof(lo) unique_lo = (lo);				\
		typeof(hi) unique_hi = (hi);				\
		__clamp(unique_val, unique_lo, unique_hi); })

#define __clamp_input_check(lo, hi)					\
        (BUILD_BUG_ON_ZERO(__builtin_choose_expr(			\
                __is_constexpr((lo) > (hi)), (lo) > (hi), false)))

#define __careful_clamp(val, lo, hi) ({					\
	__clamp_input_check(lo, hi) +					\
	__builtin_choose_expr(__typecheck(val, lo) && __typecheck(val, hi) && \
			      __typecheck(hi, lo) && __is_constexpr(val) && \
			      __is_constexpr(lo) && __is_constexpr(hi),	\
		__clamp(val, lo, hi),					\
		__clamp_once(val, lo, hi, __UNIQUE_ID(__val),		\
			     __UNIQUE_ID(__lo), __UNIQUE_ID(__hi))); })

 
#define min(x, y)	__careful_cmp(x, y, <)

 
#define max(x, y)	__careful_cmp(x, y, >)

 
#define min3(x, y, z) min((typeof(x))min(x, y), z)

 
#define max3(x, y, z) max((typeof(x))max(x, y), z)

 
#define min_not_zero(x, y) ({			\
	typeof(x) __x = (x);			\
	typeof(y) __y = (y);			\
	__x == 0 ? __y : ((__y == 0) ? __x : min(__x, __y)); })

 
#define clamp(val, lo, hi) __careful_clamp(val, lo, hi)

 

 
#define min_t(type, x, y)	__careful_cmp((type)(x), (type)(y), <)

 
#define max_t(type, x, y)	__careful_cmp((type)(x), (type)(y), >)

 
#define __unconst_integer_type_cases(type)	\
	unsigned type:  (unsigned type)0,	\
	signed type:    (signed type)0

#define __unconst_integer_typeof(x) typeof(			\
	_Generic((x),						\
		char: (char)0,					\
		__unconst_integer_type_cases(char),		\
		__unconst_integer_type_cases(short),		\
		__unconst_integer_type_cases(int),		\
		__unconst_integer_type_cases(long),		\
		__unconst_integer_type_cases(long long),	\
		default: (x)))

 
#define __minmax_array(op, array, len) ({				\
	typeof(&(array)[0]) __array = (array);				\
	typeof(len) __len = (len);					\
	__unconst_integer_typeof(__array[0]) __element = __array[--__len]; \
	while (__len--)							\
		__element = op(__element, __array[__len]);		\
	__element; })

 
#define min_array(array, len) __minmax_array(min, array, len)

 
#define max_array(array, len) __minmax_array(max, array, len)

 
#define clamp_t(type, val, lo, hi) __careful_clamp((type)(val), (type)(lo), (type)(hi))

 
#define clamp_val(val, lo, hi) clamp_t(typeof(val), val, lo, hi)

static inline bool in_range64(u64 val, u64 start, u64 len)
{
	return (val - start) < len;
}

static inline bool in_range32(u32 val, u32 start, u32 len)
{
	return (val - start) < len;
}

 
#define in_range(val, start, len)					\
	((sizeof(start) | sizeof(len) | sizeof(val)) <= sizeof(u32) ?	\
		in_range32(val, start, len) : in_range64(val, start, len))

 
#define swap(a, b) \
	do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)

#endif	 
