#ifndef SME_INST_H
#define SME_INST_H
.macro rdsvl nx, imm
	.inst	0x4bf5800			\
		| (\imm << 5)			\
		| (\nx)
.endm
.macro smstop
	msr	S0_3_C4_C6_3, xzr
.endm
.macro smstart_za
	msr	S0_3_C4_C5_3, xzr
.endm
.macro smstart_sm
	msr	S0_3_C4_C3_3, xzr
.endm
.macro _ldr_za nw, nxbase, offset=0
	.inst	0xe1000000			\
		| (((\nw) & 3) << 13)		\
		| ((\nxbase) << 5)		\
		| ((\offset) & 7)
.endm
.macro _str_za nw, nxbase, offset=0
	.inst	0xe1200000			\
		| (((\nw) & 3) << 13)		\
		| ((\nxbase) << 5)		\
		| ((\offset) & 7)
.endm
.macro _ldr_zt nx
	.inst	0xe11f8000			\
		| (((\nx) & 0x1f) << 5)
.endm
.macro _str_zt nx
	.inst	0xe13f8000			\
		| (((\nx) & 0x1f) << 5)
.endm
#endif
