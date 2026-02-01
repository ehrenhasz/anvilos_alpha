 
 

#ifndef __DRIVERS_USB_DWC3_IO_H
#define __DRIVERS_USB_DWC3_IO_H

#include <linux/io.h>
#include "trace.h"
#include "debug.h"
#include "core.h"

static inline u32 dwc3_readl(void __iomem *base, u32 offset)
{
	u32 value;

	 
	value = readl(base + offset - DWC3_GLOBALS_REGS_START);

	 
	trace_dwc3_readl(base - DWC3_GLOBALS_REGS_START, offset, value);

	return value;
}

static inline void dwc3_writel(void __iomem *base, u32 offset, u32 value)
{
	 
	writel(value, base + offset - DWC3_GLOBALS_REGS_START);

	 
	trace_dwc3_writel(base - DWC3_GLOBALS_REGS_START, offset, value);
}

#endif  
