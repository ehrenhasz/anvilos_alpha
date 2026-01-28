


#ifdef DEFINE_DWARF_REGSTR_TABLE
#undef REG_DWARFNUM_NAME
#define REG_DWARFNUM_NAME(reg, idx)	[idx] = "$" #reg
static const char * const mips_regstr_tbl[] = {
	"$0", "$1", "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	"$10", "$11", "$12", "$13", "$14", "$15", "$16", "$17", "$18", "$19",
	"$20", "$21", "$22", "$23", "$24", "$25", "$26", "$27", "$28", "%29",
	"$30", "$31",
	REG_DWARFNUM_NAME(hi, 64),
	REG_DWARFNUM_NAME(lo, 65),
};
#endif
