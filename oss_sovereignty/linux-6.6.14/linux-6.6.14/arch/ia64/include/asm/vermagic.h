#ifndef _ASM_VERMAGIC_H
#define _ASM_VERMAGIC_H
#include <linux/stringify.h>
#define MODULE_ARCH_VERMAGIC	"ia64" \
	"gcc-" __stringify(__GNUC__) "." __stringify(__GNUC_MINOR__)
#endif  
