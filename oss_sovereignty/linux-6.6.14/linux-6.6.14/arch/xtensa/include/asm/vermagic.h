#ifndef _ASM_VERMAGIC_H
#define _ASM_VERMAGIC_H
#include <linux/stringify.h>
#include <variant/core.h>
#define MODULE_ARCH_VERMAGIC "xtensa-" __stringify(XCHAL_CORE_ID) " "
#endif	 
