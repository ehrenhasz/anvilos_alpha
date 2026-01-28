#ifndef _ASM_COMPILER_H
#define _ASM_COMPILER_H
#undef barrier_before_unreachable
#define barrier_before_unreachable() asm volatile(".insn")
#define GCC_OFF_SMALL_ASM() "ZC"
#ifdef CONFIG_CPU_MIPSR6
#define MIPS_ISA_LEVEL "mips64r6"
#define MIPS_ISA_ARCH_LEVEL MIPS_ISA_LEVEL
#define MIPS_ISA_LEVEL_RAW mips64r6
#define MIPS_ISA_ARCH_LEVEL_RAW MIPS_ISA_LEVEL_RAW
#elif defined(CONFIG_CPU_MIPSR5)
#define MIPS_ISA_LEVEL "mips64r5"
#define MIPS_ISA_ARCH_LEVEL MIPS_ISA_LEVEL
#define MIPS_ISA_LEVEL_RAW mips64r5
#define MIPS_ISA_ARCH_LEVEL_RAW MIPS_ISA_LEVEL_RAW
#else
#define MIPS_ISA_LEVEL "mips64r2"
#define MIPS_ISA_ARCH_LEVEL "arch=r4000"
#define MIPS_ISA_LEVEL_RAW mips64r2
#define MIPS_ISA_ARCH_LEVEL_RAW MIPS_ISA_LEVEL_RAW
#endif  
#endif  
