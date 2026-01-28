#ifndef _ASM_S390_DWARF_H
#define _ASM_S390_DWARF_H
#ifdef __ASSEMBLY__
#define CFI_STARTPROC		.cfi_startproc
#define CFI_ENDPROC		.cfi_endproc
#define CFI_DEF_CFA_OFFSET	.cfi_def_cfa_offset
#define CFI_ADJUST_CFA_OFFSET	.cfi_adjust_cfa_offset
#define CFI_RESTORE		.cfi_restore
#ifdef CONFIG_AS_CFI_VAL_OFFSET
#define CFI_VAL_OFFSET		.cfi_val_offset
#else
#define CFI_VAL_OFFSET		#
#endif
#ifndef BUILD_VDSO
	.cfi_sections .debug_frame
#else
	.cfi_sections .eh_frame, .debug_frame
#endif
#endif	 
#endif	 
