#ifndef __ASM_OPENRISC_BITOPS_H
#define __ASM_OPENRISC_BITOPS_H
#include <linux/irqflags.h>
#include <linux/compiler.h>
#include <asm/barrier.h>
#include <asm/bitops/__ffs.h>
#include <asm-generic/bitops/ffz.h>
#include <asm/bitops/fls.h>
#include <asm/bitops/__fls.h>
#include <asm-generic/bitops/fls64.h>
#ifndef _LINUX_BITOPS_H
#error only <linux/bitops.h> can be included directly
#endif
#include <asm-generic/bitops/sched.h>
#include <asm/bitops/ffs.h>
#include <asm-generic/bitops/hweight.h>
#include <asm-generic/bitops/lock.h>
#include <asm/bitops/atomic.h>
#include <asm-generic/bitops/non-atomic.h>
#include <asm-generic/bitops/le.h>
#include <asm-generic/bitops/ext2-atomic.h>
#endif  
