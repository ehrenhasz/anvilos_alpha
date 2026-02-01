
 

#include <stdio.h>
#include <dwarf-regs.h>

static const char *mips_gpr_names[32] = {
	"$0", "$1", "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	"$10", "$11", "$12", "$13", "$14", "$15", "$16", "$17", "$18", "$19",
	"$20", "$21", "$22", "$23", "$24", "$25", "$26", "$27", "$28", "$29",
	"$30", "$31"
};

const char *get_arch_regstr(unsigned int n)
{
	if (n < 32)
		return mips_gpr_names[n];
	if (n == 64)
		return "hi";
	if (n == 65)
		return "lo";
	return NULL;
}
