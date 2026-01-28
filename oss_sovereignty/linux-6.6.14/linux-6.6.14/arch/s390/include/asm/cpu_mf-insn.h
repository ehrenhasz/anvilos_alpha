#ifndef _ASM_S390_CPU_MF_INSN_H
#define _ASM_S390_CPU_MF_INSN_H
#ifdef __ASSEMBLY__
.macro	STCCTM	r1 m3 db2
	.insn	rsy,0xeb0000000017,\r1,\m3 & 0xf,\db2
.endm
#endif  
#endif
