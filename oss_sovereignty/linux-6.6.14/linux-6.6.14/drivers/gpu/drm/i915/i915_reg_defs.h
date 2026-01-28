#ifndef __I915_REG_DEFS__
#define __I915_REG_DEFS__
#include <linux/bitfield.h>
#include <linux/bits.h>
#define REG_BIT(__n)							\
	((u32)(BIT(__n) +						\
	       BUILD_BUG_ON_ZERO(__is_constexpr(__n) &&		\
				 ((__n) < 0 || (__n) > 31))))
#define REG_BIT8(__n)                                                   \
	((u8)(BIT(__n) +                                                \
	       BUILD_BUG_ON_ZERO(__is_constexpr(__n) &&         \
				 ((__n) < 0 || (__n) > 7))))
#define REG_GENMASK(__high, __low)					\
	((u32)(GENMASK(__high, __low) +					\
	       BUILD_BUG_ON_ZERO(__is_constexpr(__high) &&	\
				 __is_constexpr(__low) &&		\
				 ((__low) < 0 || (__high) > 31 || (__low) > (__high)))))
#define REG_GENMASK64(__high, __low)					\
	((u64)(GENMASK_ULL(__high, __low) +				\
	       BUILD_BUG_ON_ZERO(__is_constexpr(__high) &&		\
				 __is_constexpr(__low) &&		\
				 ((__low) < 0 || (__high) > 63 || (__low) > (__high)))))
#define REG_GENMASK8(__high, __low)                                     \
	((u8)(GENMASK(__high, __low) +                                  \
	       BUILD_BUG_ON_ZERO(__is_constexpr(__high) &&      \
				 __is_constexpr(__low) &&               \
				 ((__low) < 0 || (__high) > 7 || (__low) > (__high)))))
#define IS_POWER_OF_2(__x)		((__x) && (((__x) & ((__x) - 1)) == 0))
#define REG_FIELD_PREP(__mask, __val)						\
	((u32)((((typeof(__mask))(__val) << __bf_shf(__mask)) & (__mask)) +	\
	       BUILD_BUG_ON_ZERO(!__is_constexpr(__mask)) +		\
	       BUILD_BUG_ON_ZERO((__mask) == 0 || (__mask) > U32_MAX) +		\
	       BUILD_BUG_ON_ZERO(!IS_POWER_OF_2((__mask) + (1ULL << __bf_shf(__mask)))) + \
	       BUILD_BUG_ON_ZERO(__builtin_choose_expr(__is_constexpr(__val), (~((__mask) >> __bf_shf(__mask)) & (__val)), 0))))
#define REG_FIELD_PREP8(__mask, __val)                                          \
	((u8)((((typeof(__mask))(__val) << __bf_shf(__mask)) & (__mask)) +      \
	       BUILD_BUG_ON_ZERO(!__is_constexpr(__mask)) +             \
	       BUILD_BUG_ON_ZERO((__mask) == 0 || (__mask) > U8_MAX) +          \
	       BUILD_BUG_ON_ZERO(!IS_POWER_OF_2((__mask) + (1ULL << __bf_shf(__mask)))) + \
	       BUILD_BUG_ON_ZERO(__builtin_choose_expr(__is_constexpr(__val), (~((__mask) >> __bf_shf(__mask)) & (__val)), 0))))
#define REG_FIELD_GET(__mask, __val)	((u32)FIELD_GET(__mask, __val))
#define REG_FIELD_GET64(__mask, __val)	((u64)FIELD_GET(__mask, __val))
#define REG_BIT16(__n)                                                   \
	((u16)(BIT(__n) +                                                \
	       BUILD_BUG_ON_ZERO(__is_constexpr(__n) &&         \
				 ((__n) < 0 || (__n) > 15))))
#define REG_GENMASK16(__high, __low)                                     \
	((u16)(GENMASK(__high, __low) +                                  \
	       BUILD_BUG_ON_ZERO(__is_constexpr(__high) &&      \
				 __is_constexpr(__low) &&               \
				 ((__low) < 0 || (__high) > 15 || (__low) > (__high)))))
#define REG_FIELD_PREP16(__mask, __val)                                          \
	((u16)((((typeof(__mask))(__val) << __bf_shf(__mask)) & (__mask)) +      \
	       BUILD_BUG_ON_ZERO(!__is_constexpr(__mask)) +             \
	       BUILD_BUG_ON_ZERO((__mask) == 0 || (__mask) > U16_MAX) +          \
	       BUILD_BUG_ON_ZERO(!IS_POWER_OF_2((__mask) + (1ULL << __bf_shf(__mask)))) + \
	       BUILD_BUG_ON_ZERO(__builtin_choose_expr(__is_constexpr(__val), (~((__mask) >> __bf_shf(__mask)) & (__val)), 0))))
#define __MASKED_FIELD(mask, value) ((mask) << 16 | (value))
#define _MASKED_FIELD(mask, value) ({					   \
	if (__builtin_constant_p(mask))					   \
		BUILD_BUG_ON_MSG(((mask) & 0xffff0000), "Incorrect mask"); \
	if (__builtin_constant_p(value))				   \
		BUILD_BUG_ON_MSG((value) & 0xffff0000, "Incorrect value"); \
	if (__builtin_constant_p(mask) && __builtin_constant_p(value))	   \
		BUILD_BUG_ON_MSG((value) & ~(mask),			   \
				 "Incorrect value for mask");		   \
	__MASKED_FIELD(mask, value); })
#define _MASKED_BIT_ENABLE(a)	({ typeof(a) _a = (a); _MASKED_FIELD(_a, _a); })
#define _MASKED_BIT_DISABLE(a)	(_MASKED_FIELD((a), 0))
#define _PICK_EVEN(__index, __a, __b) ((__a) + (__index) * ((__b) - (__a)))
#define _PICK_EVEN_2RANGES(__index, __c_index, __a, __b, __c, __d)		\
	(BUILD_BUG_ON_ZERO(!__is_constexpr(__c_index)) +			\
	 ((__index) < (__c_index) ? _PICK_EVEN(__index, __a, __b) :		\
				   _PICK_EVEN((__index) - (__c_index), __c, __d)))
#define _PICK(__index, ...) (((const u32 []){ __VA_ARGS__ })[__index])
#define REG_FIELD_GET8(__mask, __val)   ((u8)FIELD_GET(__mask, __val))
typedef struct {
	u32 reg;
} i915_reg_t;
#define _MMIO(r) ((const i915_reg_t){ .reg = (r) })
typedef struct {
	u32 reg;
} i915_mcr_reg_t;
#define MCR_REG(offset)	((const i915_mcr_reg_t){ .reg = (offset) })
#define INVALID_MMIO_REG _MMIO(0)
#define i915_mmio_reg_offset(r) \
	_Generic((r), i915_reg_t: (r).reg, i915_mcr_reg_t: (r).reg)
#define i915_mmio_reg_equal(a, b) (i915_mmio_reg_offset(a) == i915_mmio_reg_offset(b))
#define i915_mmio_reg_valid(r) (!i915_mmio_reg_equal(r, INVALID_MMIO_REG))
#endif  
