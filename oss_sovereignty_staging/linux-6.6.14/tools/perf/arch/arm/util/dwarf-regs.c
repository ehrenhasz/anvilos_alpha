
 

#include <stddef.h>
#include <linux/stringify.h>
#include <dwarf-regs.h>

struct pt_regs_dwarfnum {
	const char *name;
	unsigned int dwarfnum;
};

#define REG_DWARFNUM_NAME(r, num) {.name = r, .dwarfnum = num}
#define GPR_DWARFNUM_NAME(num) \
	{.name = __stringify(%r##num), .dwarfnum = num}
#define REG_DWARFNUM_END {.name = NULL, .dwarfnum = 0}

 
static const struct pt_regs_dwarfnum regdwarfnum_table[] = {
	GPR_DWARFNUM_NAME(0),
	GPR_DWARFNUM_NAME(1),
	GPR_DWARFNUM_NAME(2),
	GPR_DWARFNUM_NAME(3),
	GPR_DWARFNUM_NAME(4),
	GPR_DWARFNUM_NAME(5),
	GPR_DWARFNUM_NAME(6),
	GPR_DWARFNUM_NAME(7),
	GPR_DWARFNUM_NAME(8),
	GPR_DWARFNUM_NAME(9),
	GPR_DWARFNUM_NAME(10),
	REG_DWARFNUM_NAME("%fp", 11),
	REG_DWARFNUM_NAME("%ip", 12),
	REG_DWARFNUM_NAME("%sp", 13),
	REG_DWARFNUM_NAME("%lr", 14),
	REG_DWARFNUM_NAME("%pc", 15),
	REG_DWARFNUM_END,
};

 
const char *get_arch_regstr(unsigned int n)
{
	const struct pt_regs_dwarfnum *roff;
	for (roff = regdwarfnum_table; roff->name != NULL; roff++)
		if (roff->dwarfnum == n)
			return roff->name;
	return NULL;
}
