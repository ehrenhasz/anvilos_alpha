#ifndef _ASM_RISCV_CLINT_H
#define _ASM_RISCV_CLINT_H
#include <linux/types.h>
#include <asm/mmio.h>
#ifdef CONFIG_RISCV_M_MODE
extern u64 __iomem *clint_time_val;
#endif
#endif
