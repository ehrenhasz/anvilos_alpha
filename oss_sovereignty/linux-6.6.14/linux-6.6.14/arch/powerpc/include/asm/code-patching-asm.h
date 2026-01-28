#ifndef _ASM_POWERPC_CODE_PATCHING_ASM_H
#define _ASM_POWERPC_CODE_PATCHING_ASM_H
.macro patch_site label name
	.pushsection ".rodata"
	.balign 4
	.global \name
\name:
	.4byte	\label - .
	.popsection
.endm
#endif  
