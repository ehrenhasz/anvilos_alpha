#ifndef _ASM_EVA_H
#define _ASM_EVA_H
#include <kernel-entry-init.h>
#ifdef __ASSEMBLY__
#ifdef CONFIG_EVA
.macro eva_init
platform_eva_init
.endm
#else
.macro eva_init
.endm
#endif  
#endif  
#endif
