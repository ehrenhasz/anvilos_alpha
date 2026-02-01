
 

#include "dc_trace.h"

#if defined(CONFIG_X86)
#include <asm/fpu/api.h>
#elif defined(CONFIG_PPC64)
#include <asm/switch_to.h>
#include <asm/cputable.h>
#elif defined(CONFIG_ARM64)
#include <asm/neon.h>
#elif defined(CONFIG_LOONGARCH)
#include <asm/fpu.h>
#endif

 

static DEFINE_PER_CPU(int, fpu_recursion_depth);

 
inline void dc_assert_fp_enabled(void)
{
	int *pcpu, depth = 0;

	pcpu = get_cpu_ptr(&fpu_recursion_depth);
	depth = *pcpu;
	put_cpu_ptr(&fpu_recursion_depth);

	ASSERT(depth >= 1);
}

 
void dc_fpu_begin(const char *function_name, const int line)
{
	int *pcpu;

	pcpu = get_cpu_ptr(&fpu_recursion_depth);
	*pcpu += 1;

	if (*pcpu == 1) {
#if defined(CONFIG_X86) || defined(CONFIG_LOONGARCH)
		migrate_disable();
		kernel_fpu_begin();
#elif defined(CONFIG_PPC64)
		if (cpu_has_feature(CPU_FTR_VSX_COMP)) {
			preempt_disable();
			enable_kernel_vsx();
		} else if (cpu_has_feature(CPU_FTR_ALTIVEC_COMP)) {
			preempt_disable();
			enable_kernel_altivec();
		} else if (!cpu_has_feature(CPU_FTR_FPU_UNAVAILABLE)) {
			preempt_disable();
			enable_kernel_fp();
		}
#elif defined(CONFIG_ARM64)
		kernel_neon_begin();
#endif
	}

	TRACE_DCN_FPU(true, function_name, line, *pcpu);
	put_cpu_ptr(&fpu_recursion_depth);
}

 
void dc_fpu_end(const char *function_name, const int line)
{
	int *pcpu;

	pcpu = get_cpu_ptr(&fpu_recursion_depth);
	*pcpu -= 1;
	if (*pcpu <= 0) {
#if defined(CONFIG_X86) || defined(CONFIG_LOONGARCH)
		kernel_fpu_end();
		migrate_enable();
#elif defined(CONFIG_PPC64)
		if (cpu_has_feature(CPU_FTR_VSX_COMP)) {
			disable_kernel_vsx();
			preempt_enable();
		} else if (cpu_has_feature(CPU_FTR_ALTIVEC_COMP)) {
			disable_kernel_altivec();
			preempt_enable();
		} else if (!cpu_has_feature(CPU_FTR_FPU_UNAVAILABLE)) {
			disable_kernel_fp();
			preempt_enable();
		}
#elif defined(CONFIG_ARM64)
		kernel_neon_end();
#endif
	}

	TRACE_DCN_FPU(false, function_name, line, *pcpu);
	put_cpu_ptr(&fpu_recursion_depth);
}
