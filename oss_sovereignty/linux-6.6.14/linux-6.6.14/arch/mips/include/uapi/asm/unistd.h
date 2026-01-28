#ifndef _UAPI_ASM_UNISTD_H
#define _UAPI_ASM_UNISTD_H
#include <asm/sgidefs.h>
#if _MIPS_SIM == _MIPS_SIM_ABI32
#define __NR_Linux	4000
#include <asm/unistd_o32.h>
#endif  
#if _MIPS_SIM == _MIPS_SIM_ABI64
#define __NR_Linux	5000
#include <asm/unistd_n64.h>
#endif  
#if _MIPS_SIM == _MIPS_SIM_NABI32
#define __NR_Linux	6000
#include <asm/unistd_n32.h>
#endif  
#endif  
