#ifndef __DAL_FIXED31_32_H__
#define __DAL_FIXED31_32_H__
#ifndef LLONG_MAX
#define LLONG_MAX 9223372036854775807ll
#endif
#ifndef LLONG_MIN
#define LLONG_MIN (-LLONG_MAX - 1ll)
#endif
#define FIXED31_32_BITS_PER_FRACTIONAL_PART 32
#ifndef LLONG_MIN
#define LLONG_MIN (1LL<<63)
#endif
#ifndef LLONG_MAX
#define LLONG_MAX (-1LL>>1)
#endif
struct fixed31_32 {
	long long value;
};
static const struct fixed31_32 dc_fixpt_zero = { 0 };
static const struct fixed31_32 dc_fixpt_epsilon = { 1LL };
static const struct fixed31_32 dc_fixpt_half = { 0x80000000LL };
static const struct fixed31_32 dc_fixpt_one = { 0x100000000LL };
struct fixed31_32 dc_fixpt_from_fraction(long long numerator, long long denominator);
static inline struct fixed31_32 dc_fixpt_from_int(int arg)
{
	struct fixed31_32 res;
	res.value = (long long) arg << FIXED31_32_BITS_PER_FRACTIONAL_PART;
	return res;
}
static inline struct fixed31_32 dc_fixpt_neg(struct fixed31_32 arg)
{
	struct fixed31_32 res;
	res.value = -arg.value;
	return res;
}
static inline struct fixed31_32 dc_fixpt_abs(struct fixed31_32 arg)
{
	if (arg.value < 0)
		return dc_fixpt_neg(arg);
	else
		return arg;
}
static inline bool dc_fixpt_lt(struct fixed31_32 arg1, struct fixed31_32 arg2)
{
	return arg1.value < arg2.value;
}
static inline bool dc_fixpt_le(struct fixed31_32 arg1, struct fixed31_32 arg2)
{
	return arg1.value <= arg2.value;
}
static inline bool dc_fixpt_eq(struct fixed31_32 arg1, struct fixed31_32 arg2)
{
	return arg1.value == arg2.value;
}
static inline struct fixed31_32 dc_fixpt_min(struct fixed31_32 arg1, struct fixed31_32 arg2)
{
	if (arg1.value <= arg2.value)
		return arg1;
	else
		return arg2;
}
static inline struct fixed31_32 dc_fixpt_max(struct fixed31_32 arg1, struct fixed31_32 arg2)
{
	if (arg1.value <= arg2.value)
		return arg2;
	else
		return arg1;
}
static inline struct fixed31_32 dc_fixpt_clamp(
	struct fixed31_32 arg,
	struct fixed31_32 min_value,
	struct fixed31_32 max_value)
{
	if (dc_fixpt_le(arg, min_value))
		return min_value;
	else if (dc_fixpt_le(max_value, arg))
		return max_value;
	else
		return arg;
}
static inline struct fixed31_32 dc_fixpt_shl(struct fixed31_32 arg, unsigned char shift)
{
	ASSERT(((arg.value >= 0) && (arg.value <= LLONG_MAX >> shift)) ||
		((arg.value < 0) && (arg.value >= ~(LLONG_MAX >> shift))));
	arg.value = arg.value << shift;
	return arg;
}
static inline struct fixed31_32 dc_fixpt_shr(struct fixed31_32 arg, unsigned char shift)
{
	bool negative = arg.value < 0;
	if (negative)
		arg.value = -arg.value;
	arg.value = arg.value >> shift;
	if (negative)
		arg.value = -arg.value;
	return arg;
}
static inline struct fixed31_32 dc_fixpt_add(struct fixed31_32 arg1, struct fixed31_32 arg2)
{
	struct fixed31_32 res;
	ASSERT(((arg1.value >= 0) && (LLONG_MAX - arg1.value >= arg2.value)) ||
		((arg1.value < 0) && (LLONG_MIN - arg1.value <= arg2.value)));
	res.value = arg1.value + arg2.value;
	return res;
}
static inline struct fixed31_32 dc_fixpt_add_int(struct fixed31_32 arg1, int arg2)
{
	return dc_fixpt_add(arg1, dc_fixpt_from_int(arg2));
}
static inline struct fixed31_32 dc_fixpt_sub(struct fixed31_32 arg1, struct fixed31_32 arg2)
{
	struct fixed31_32 res;
	ASSERT(((arg2.value >= 0) && (LLONG_MIN + arg2.value <= arg1.value)) ||
		((arg2.value < 0) && (LLONG_MAX + arg2.value >= arg1.value)));
	res.value = arg1.value - arg2.value;
	return res;
}
static inline struct fixed31_32 dc_fixpt_sub_int(struct fixed31_32 arg1, int arg2)
{
	return dc_fixpt_sub(arg1, dc_fixpt_from_int(arg2));
}
struct fixed31_32 dc_fixpt_mul(struct fixed31_32 arg1, struct fixed31_32 arg2);
static inline struct fixed31_32 dc_fixpt_mul_int(struct fixed31_32 arg1, int arg2)
{
	return dc_fixpt_mul(arg1, dc_fixpt_from_int(arg2));
}
struct fixed31_32 dc_fixpt_sqr(struct fixed31_32 arg);
static inline struct fixed31_32 dc_fixpt_div_int(struct fixed31_32 arg1, long long arg2)
{
	return dc_fixpt_from_fraction(arg1.value, dc_fixpt_from_int((int)arg2).value);
}
static inline struct fixed31_32 dc_fixpt_div(struct fixed31_32 arg1, struct fixed31_32 arg2)
{
	return dc_fixpt_from_fraction(arg1.value, arg2.value);
}
struct fixed31_32 dc_fixpt_recip(struct fixed31_32 arg);
struct fixed31_32 dc_fixpt_sinc(struct fixed31_32 arg);
struct fixed31_32 dc_fixpt_sin(struct fixed31_32 arg);
struct fixed31_32 dc_fixpt_cos(struct fixed31_32 arg);
struct fixed31_32 dc_fixpt_exp(struct fixed31_32 arg);
struct fixed31_32 dc_fixpt_log(struct fixed31_32 arg);
static inline struct fixed31_32 dc_fixpt_pow(struct fixed31_32 arg1, struct fixed31_32 arg2)
{
	if (arg1.value == 0)
		return arg2.value == 0 ? dc_fixpt_one : dc_fixpt_zero;
	return dc_fixpt_exp(
		dc_fixpt_mul(
			dc_fixpt_log(arg1),
			arg2));
}
static inline int dc_fixpt_floor(struct fixed31_32 arg)
{
	unsigned long long arg_value = arg.value > 0 ? arg.value : -arg.value;
	if (arg.value >= 0)
		return (int)(arg_value >> FIXED31_32_BITS_PER_FRACTIONAL_PART);
	else
		return -(int)(arg_value >> FIXED31_32_BITS_PER_FRACTIONAL_PART);
}
static inline int dc_fixpt_round(struct fixed31_32 arg)
{
	unsigned long long arg_value = arg.value > 0 ? arg.value : -arg.value;
	const long long summand = dc_fixpt_half.value;
	ASSERT(LLONG_MAX - (long long)arg_value >= summand);
	arg_value += summand;
	if (arg.value >= 0)
		return (int)(arg_value >> FIXED31_32_BITS_PER_FRACTIONAL_PART);
	else
		return -(int)(arg_value >> FIXED31_32_BITS_PER_FRACTIONAL_PART);
}
static inline int dc_fixpt_ceil(struct fixed31_32 arg)
{
	unsigned long long arg_value = arg.value > 0 ? arg.value : -arg.value;
	const long long summand = dc_fixpt_one.value -
		dc_fixpt_epsilon.value;
	ASSERT(LLONG_MAX - (long long)arg_value >= summand);
	arg_value += summand;
	if (arg.value >= 0)
		return (int)(arg_value >> FIXED31_32_BITS_PER_FRACTIONAL_PART);
	else
		return -(int)(arg_value >> FIXED31_32_BITS_PER_FRACTIONAL_PART);
}
unsigned int dc_fixpt_u4d19(struct fixed31_32 arg);
unsigned int dc_fixpt_u3d19(struct fixed31_32 arg);
unsigned int dc_fixpt_u2d19(struct fixed31_32 arg);
unsigned int dc_fixpt_u0d19(struct fixed31_32 arg);
unsigned int dc_fixpt_clamp_u0d14(struct fixed31_32 arg);
unsigned int dc_fixpt_clamp_u0d10(struct fixed31_32 arg);
int dc_fixpt_s4d19(struct fixed31_32 arg);
static inline struct fixed31_32 dc_fixpt_truncate(struct fixed31_32 arg, unsigned int frac_bits)
{
	bool negative = arg.value < 0;
	if (frac_bits >= FIXED31_32_BITS_PER_FRACTIONAL_PART) {
		ASSERT(frac_bits == FIXED31_32_BITS_PER_FRACTIONAL_PART);
		return arg;
	}
	if (negative)
		arg.value = -arg.value;
	arg.value &= (~0ULL) << (FIXED31_32_BITS_PER_FRACTIONAL_PART - frac_bits);
	if (negative)
		arg.value = -arg.value;
	return arg;
}
#endif
