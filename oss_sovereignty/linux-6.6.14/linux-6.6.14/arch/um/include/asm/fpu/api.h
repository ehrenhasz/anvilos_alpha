#ifndef _ASM_UM_FPU_API_H
#define _ASM_UM_FPU_API_H
#define kernel_fpu_begin() (void)0
#define kernel_fpu_end() (void)0
static inline bool irq_fpu_usable(void)
{
	return true;
}
#endif
