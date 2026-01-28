#ifndef __ASM_POWERPC_CPUFEATURE_H
#define __ASM_POWERPC_CPUFEATURE_H
#include <asm/cputable.h>
#define MAX_CPU_FEATURES (2 * 32)
#define PPC_MODULE_FEATURE_VEC_CRYPTO			(32 + ilog2(PPC_FEATURE2_VEC_CRYPTO))
#define PPC_MODULE_FEATURE_P10				(32 + ilog2(PPC_FEATURE2_ARCH_3_1))
#define cpu_feature(x)		(x)
static inline bool cpu_have_feature(unsigned int num)
{
	if (num < 32)
		return !!(cur_cpu_spec->cpu_user_features & 1UL << num);
	else
		return !!(cur_cpu_spec->cpu_user_features2 & 1UL << (num - 32));
}
#endif  
