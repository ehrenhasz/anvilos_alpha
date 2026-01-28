#ifndef _ASM_MICROBLAZE_BARRIER_H
#define _ASM_MICROBLAZE_BARRIER_H
#define mb()	__asm__ __volatile__ ("mbar 1" : : : "memory")
#include <asm-generic/barrier.h>
#endif  
