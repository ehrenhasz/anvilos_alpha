#ifndef __ASM_LINKAGE_H
#define __ASM_LINKAGE_H
#include <asm/dwarf.h>
#define ASM_NL		 `	 
#define __ALIGN		.align 4
#define __ALIGN_STR	__stringify(__ALIGN)
#ifdef __ASSEMBLY__
.macro ST2 e, o, off
#ifdef CONFIG_ARC_HAS_LL64
	std	\e, [sp, \off]
#else
	st	\e, [sp, \off]
	st	\o, [sp, \off+4]
#endif
.endm
.macro LD2 e, o, off
#ifdef CONFIG_ARC_HAS_LL64
	ldd	\e, [sp, \off]
#else
	ld	\e, [sp, \off]
	ld	\o, [sp, \off+4]
#endif
.endm
.macro ARCFP_DATA nm
#ifdef CONFIG_ARC_HAS_DCCM
	.section .data.arcfp
#else
	.section .data
#endif
	.global \nm
.endm
.macro ARCFP_CODE
#ifdef CONFIG_ARC_HAS_ICCM
	.section .text.arcfp, "ax",@progbits
#else
	.section .text, "ax",@progbits
#endif
.endm
#define ENTRY_CFI(name)		\
	.globl name ASM_NL	\
	ALIGN ASM_NL 		\
	name: ASM_NL		\
	CFI_STARTPROC ASM_NL
#define END_CFI(name) 		\
	CFI_ENDPROC ASM_NL	\
	.size name, .-name
#else	 
#ifdef CONFIG_ARC_HAS_ICCM
#define __arcfp_code __section(".text.arcfp")
#else
#define __arcfp_code __section(".text")
#endif
#ifdef CONFIG_ARC_HAS_DCCM
#define __arcfp_data __section(".data.arcfp")
#else
#define __arcfp_data __section(".data")
#endif
#endif  
#endif
