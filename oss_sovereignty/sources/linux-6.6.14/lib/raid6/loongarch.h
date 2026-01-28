


#ifndef _LIB_RAID6_LOONGARCH_H
#define _LIB_RAID6_LOONGARCH_H

#ifdef __KERNEL__

#include <asm/cpu-features.h>
#include <asm/fpu.h>

#else 

#include <sys/auxv.h>


#ifndef HWCAP_LOONGARCH_LSX
#define HWCAP_LOONGARCH_LSX	(1 << 4)
#endif
#ifndef HWCAP_LOONGARCH_LASX
#define HWCAP_LOONGARCH_LASX	(1 << 5)
#endif

#define kernel_fpu_begin()
#define kernel_fpu_end()

#define cpu_has_lsx	(getauxval(AT_HWCAP) & HWCAP_LOONGARCH_LSX)
#define cpu_has_lasx	(getauxval(AT_HWCAP) & HWCAP_LOONGARCH_LASX)

#endif 

#endif 
