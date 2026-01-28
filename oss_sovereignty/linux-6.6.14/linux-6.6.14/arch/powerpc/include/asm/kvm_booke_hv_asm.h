#ifndef ASM_KVM_BOOKE_HV_ASM_H
#define ASM_KVM_BOOKE_HV_ASM_H
#include <asm/feature-fixups.h>
#ifdef __ASSEMBLY__
.macro DO_KVM intno srr1
#ifdef CONFIG_KVM_BOOKE_HV
BEGIN_FTR_SECTION
	mtocrf	0x80, r11	 
	bf	3, 1975f
	b	kvmppc_handler_\intno\()_\srr1
1975:
END_FTR_SECTION_IFSET(CPU_FTR_EMB_HV)
#endif
.endm
#endif  
#endif  
