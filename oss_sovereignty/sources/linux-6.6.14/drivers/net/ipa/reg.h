



#ifndef _REG_H_
#define _REG_H_

#include <linux/types.h>
#include <linux/log2.h>
#include <linux/bug.h>


struct reg {
	u32 offset;
	u32 stride;
	u32 fcount;
	const u32 *fmask;			
	const char *name;
};


#define REG(__NAME, __reg_id, __offset)					\
	REG_STRIDE(__NAME, __reg_id, __offset, 0)


#define REG_STRIDE(__NAME, __reg_id, __offset, __stride)		\
	static const struct reg reg_ ## __reg_id = {			\
		.name	= #__NAME,					\
		.offset	= __offset,					\
		.stride	= __stride,					\
	}

#define REG_FIELDS(__NAME, __name, __offset)				\
	REG_STRIDE_FIELDS(__NAME, __name, __offset, 0)

#define REG_STRIDE_FIELDS(__NAME, __name, __offset, __stride)		\
	static const struct reg reg_ ## __name = {			\
		.name   = #__NAME,					\
		.offset = __offset,					\
		.stride = __stride,					\
		.fcount = ARRAY_SIZE(reg_ ## __name ## _fmask),		\
		.fmask  = reg_ ## __name ## _fmask,			\
	}


struct regs {
	u32 reg_count;
	const struct reg **reg;
};

static inline const struct reg *reg(const struct regs *regs, u32 reg_id)
{
	if (WARN(reg_id >= regs->reg_count,
		 "reg out of range (%u > %u)\n", reg_id, regs->reg_count - 1))
		return NULL;

	return regs->reg[reg_id];
}


static inline u32 reg_fmask(const struct reg *reg, u32 field_id)
{
	if (!reg || WARN_ON(field_id >= reg->fcount))
		return 0;

	return reg->fmask[field_id];
}


static inline u32 reg_bit(const struct reg *reg, u32 field_id)
{
	u32 fmask = reg_fmask(reg, field_id);

	if (WARN_ON(!is_power_of_2(fmask)))
		return 0;

	return fmask;
}


static inline u32 reg_field_max(const struct reg *reg, u32 field_id)
{
	u32 fmask = reg_fmask(reg, field_id);

	return fmask ? fmask >> __ffs(fmask) : 0;
}


static inline u32 reg_encode(const struct reg *reg, u32 field_id, u32 val)
{
	u32 fmask = reg_fmask(reg, field_id);

	if (!fmask)
		return 0;

	val <<= __ffs(fmask);
	if (WARN_ON(val & ~fmask))
		return 0;

	return val;
}


static inline u32 reg_decode(const struct reg *reg, u32 field_id, u32 val)
{
	u32 fmask = reg_fmask(reg, field_id);

	return fmask ? (val & fmask) >> __ffs(fmask) : 0;
}


static inline u32 reg_offset(const struct reg *reg)
{
	return reg ? reg->offset : 0;
}


static inline u32 reg_n_offset(const struct reg *reg, u32 n)
{
	return reg ? reg->offset + n * reg->stride : 0;
}

#endif 
