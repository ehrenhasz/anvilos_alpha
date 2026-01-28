#ifndef __ASM_OPENRISC_SERIAL_H
#define __ASM_OPENRISC_SERIAL_H
#ifdef __KERNEL__
#include <asm/cpuinfo.h>
#define BASE_BAUD (cpuinfo_or1k[smp_processor_id()].clock_frequency/16)
#endif  
#endif  
