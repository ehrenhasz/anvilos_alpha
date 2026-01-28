#ifndef __TOOLS_ASM_GENERIC_BITOPS_H
#define __TOOLS_ASM_GENERIC_BITOPS_H
#include <asm-generic/bitops/__ffs.h>
#include <asm-generic/bitops/__ffz.h>
#include <asm-generic/bitops/fls.h>
#include <asm-generic/bitops/__fls.h>
#include <asm-generic/bitops/fls64.h>
#ifndef _TOOLS_LINUX_BITOPS_H_
#error only <linux/bitops.h> can be included directly
#endif
#include <asm-generic/bitops/hweight.h>
#include <asm-generic/bitops/atomic.h>
#include <asm-generic/bitops/non-atomic.h>
#endif  
