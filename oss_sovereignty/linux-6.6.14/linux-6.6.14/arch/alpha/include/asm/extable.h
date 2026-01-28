#ifndef _ASM_EXTABLE_H
#define _ASM_EXTABLE_H
struct exception_table_entry
{
	signed int insn;
	union exception_fixup {
		unsigned unit;
		struct {
			signed int nextinsn : 16;
			unsigned int errreg : 5;
			unsigned int valreg : 5;
		} bits;
	} fixup;
};
#define fixup_exception(map_reg, _fixup, pc)			\
({								\
	if ((_fixup)->fixup.bits.valreg != 31)			\
		map_reg((_fixup)->fixup.bits.valreg) = 0;	\
	if ((_fixup)->fixup.bits.errreg != 31)			\
		map_reg((_fixup)->fixup.bits.errreg) = -EFAULT;	\
	(pc) + (_fixup)->fixup.bits.nextinsn;			\
})
#define ARCH_HAS_RELATIVE_EXTABLE
#define swap_ex_entry_fixup(a, b, tmp, delta)			\
	do {							\
		(a)->fixup.unit = (b)->fixup.unit;		\
		(b)->fixup.unit = (tmp).fixup.unit;		\
	} while (0)
#endif
