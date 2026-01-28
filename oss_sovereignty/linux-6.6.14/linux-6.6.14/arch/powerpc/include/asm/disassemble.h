#ifndef __ASM_PPC_DISASSEMBLE_H__
#define __ASM_PPC_DISASSEMBLE_H__
#include <linux/types.h>
static inline unsigned int get_op(u32 inst)
{
	return inst >> 26;
}
static inline unsigned int get_xop(u32 inst)
{
	return (inst >> 1) & 0x3ff;
}
static inline unsigned int get_sprn(u32 inst)
{
	return ((inst >> 16) & 0x1f) | ((inst >> 6) & 0x3e0);
}
static inline unsigned int get_dcrn(u32 inst)
{
	return ((inst >> 16) & 0x1f) | ((inst >> 6) & 0x3e0);
}
static inline unsigned int get_tmrn(u32 inst)
{
	return ((inst >> 16) & 0x1f) | ((inst >> 6) & 0x3e0);
}
static inline unsigned int get_rt(u32 inst)
{
	return (inst >> 21) & 0x1f;
}
static inline unsigned int get_rs(u32 inst)
{
	return (inst >> 21) & 0x1f;
}
static inline unsigned int get_ra(u32 inst)
{
	return (inst >> 16) & 0x1f;
}
static inline unsigned int get_rb(u32 inst)
{
	return (inst >> 11) & 0x1f;
}
static inline unsigned int get_rc(u32 inst)
{
	return inst & 0x1;
}
static inline unsigned int get_ws(u32 inst)
{
	return (inst >> 11) & 0x1f;
}
static inline unsigned int get_d(u32 inst)
{
	return inst & 0xffff;
}
static inline unsigned int get_oc(u32 inst)
{
	return (inst >> 11) & 0x7fff;
}
static inline unsigned int get_tx_or_sx(u32 inst)
{
	return (inst) & 0x1;
}
#define IS_XFORM(inst)	(get_op(inst)  == 31)
#define IS_DSFORM(inst)	(get_op(inst) >= 56)
static inline unsigned make_dsisr(unsigned instr)
{
	unsigned dsisr;
	dsisr = (instr & 0x03ff0000) >> 16;
	if (IS_XFORM(instr)) {
		dsisr |= (instr & 0x00000006) << 14;
		dsisr |= (instr & 0x00000040) << 8;
		dsisr |= (instr & 0x00000780) << 3;
	} else {
		dsisr |= (instr & 0x04000000) >> 12;
		dsisr |= (instr & 0x78000000) >> 17;
		if (IS_DSFORM(instr))
			dsisr |= (instr & 0x00000003) << 18;
	}
	return dsisr;
}
#endif  
