
#ifdef DEFINE_DWARF_REGSTR_TABLE


static const char * const x86_32_regstr_tbl[] = {
	"%ax", "%cx", "%dx", "%bx", "$stack",
	"%bp", "%si", "%di",
};

static const char * const x86_64_regstr_tbl[] = {
	"%ax", "%dx", "%cx", "%bx", "%si", "%di",
	"%bp", "%sp", "%r8", "%r9", "%r10", "%r11",
	"%r12", "%r13", "%r14", "%r15",
};
#endif
