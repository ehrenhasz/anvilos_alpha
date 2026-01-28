#ifndef __INTEL_PXP_REGS_H__
#define __INTEL_PXP_REGS_H__
#include "i915_reg_defs.h"
#define GEN12_KCR_BASE 0x32000
#define MTL_KCR_BASE 0x386000
#define KCR_INIT(base) _MMIO((base) + 0xf0)
#define KCR_INIT_ALLOW_DISPLAY_ME_WRITES REG_BIT(14)
#define KCR_SIP(base) _MMIO((base) + 0x260)
#define KCR_GLOBAL_TERMINATE(base) _MMIO((base) + 0xf8)
#endif  
