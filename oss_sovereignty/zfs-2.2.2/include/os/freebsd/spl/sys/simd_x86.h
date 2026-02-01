 

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <machine/pcb.h>
#include <x86/x86_var.h>
#include <x86/specialreg.h>

#define	kfpu_init()		(0)
#define	kfpu_fini()		do {} while (0)
#define	kfpu_allowed()		1
#define	kfpu_initialize(tsk)	do {} while (0)

#define	kfpu_begin() {					\
	if (__predict_false(!is_fpu_kern_thread(0)))		\
		fpu_kern_enter(curthread, NULL, FPU_KERN_NOCTX);\
}

#ifndef PCB_FPUNOSAVE
#define	PCB_FPUNOSAVE	PCB_NPXNOSAVE
#endif

#define	kfpu_end()	{			\
	if (__predict_false(curpcb->pcb_flags & PCB_FPUNOSAVE))	\
		fpu_kern_leave(curthread, NULL);	\
}

 
static inline uint64_t
xgetbv(uint32_t index)
{
	uint32_t eax, edx;
	 
	__asm__ __volatile__(".byte 0x0f; .byte 0x01; .byte 0xd0"
	    : "=a" (eax), "=d" (edx)
	    : "c" (index));

	return ((((uint64_t)edx)<<32) | (uint64_t)eax);
}


 
static inline boolean_t
__simd_state_enabled(const uint64_t state)
{
	boolean_t has_osxsave;
	uint64_t xcr0;

	has_osxsave = (cpu_feature2 & CPUID2_OSXSAVE) != 0;

	if (!has_osxsave)
		return (B_FALSE);

	xcr0 = xgetbv(0);
	return ((xcr0 & state) == state);
}

#define	_XSTATE_SSE_AVX		(0x2 | 0x4)
#define	_XSTATE_AVX512		(0xE0 | _XSTATE_SSE_AVX)

#define	__ymm_enabled() __simd_state_enabled(_XSTATE_SSE_AVX)
#define	__zmm_enabled() __simd_state_enabled(_XSTATE_AVX512)


 
static inline boolean_t
zfs_sse_available(void)
{
	return ((cpu_feature & CPUID_SSE) != 0);
}

 
static inline boolean_t
zfs_sse2_available(void)
{
	return ((cpu_feature & CPUID_SSE2) != 0);
}

 
static inline boolean_t
zfs_sse3_available(void)
{
	return ((cpu_feature2 & CPUID2_SSE3) != 0);
}

 
static inline boolean_t
zfs_ssse3_available(void)
{
	return ((cpu_feature2 & CPUID2_SSSE3) != 0);
}

 
static inline boolean_t
zfs_sse4_1_available(void)
{
	return ((cpu_feature2 & CPUID2_SSE41) != 0);
}

 
static inline boolean_t
zfs_sse4_2_available(void)
{
	return ((cpu_feature2 & CPUID2_SSE42) != 0);
}

 
static inline boolean_t
zfs_avx_available(void)
{
	boolean_t has_avx;

	has_avx = (cpu_feature2 & CPUID2_AVX) != 0;

	return (has_avx && __ymm_enabled());
}

 
static inline boolean_t
zfs_avx2_available(void)
{
	boolean_t has_avx2;

	has_avx2 = (cpu_stdext_feature & CPUID_STDEXT_AVX2) != 0;

	return (has_avx2 && __ymm_enabled());
}

 
static inline boolean_t
zfs_shani_available(void)
{
	boolean_t has_shani;

	has_shani = (cpu_stdext_feature & CPUID_STDEXT_SHA) != 0;

	return (has_shani && __ymm_enabled());
}

 


 
static inline boolean_t
zfs_avx512f_available(void)
{
	boolean_t has_avx512;

	has_avx512 = (cpu_stdext_feature & CPUID_STDEXT_AVX512F) != 0;

	return (has_avx512 && __zmm_enabled());
}

 
static inline boolean_t
zfs_avx512cd_available(void)
{
	boolean_t has_avx512;

	has_avx512 = (cpu_stdext_feature & CPUID_STDEXT_AVX512F) != 0 &&
	    (cpu_stdext_feature & CPUID_STDEXT_AVX512CD) != 0;

	return (has_avx512 && __zmm_enabled());
}

 
static inline boolean_t
zfs_avx512er_available(void)
{
	boolean_t has_avx512;

	has_avx512 = (cpu_stdext_feature & CPUID_STDEXT_AVX512F) != 0 &&
	    (cpu_stdext_feature & CPUID_STDEXT_AVX512CD) != 0;

	return (has_avx512 && __zmm_enabled());
}

 
static inline boolean_t
zfs_avx512pf_available(void)
{
	boolean_t has_avx512;

	has_avx512 = (cpu_stdext_feature & CPUID_STDEXT_AVX512F) != 0 &&
	    (cpu_stdext_feature & CPUID_STDEXT_AVX512PF) != 0;

	return (has_avx512 && __zmm_enabled());
}

 
static inline boolean_t
zfs_avx512bw_available(void)
{
	boolean_t has_avx512 = B_FALSE;

	has_avx512 = (cpu_stdext_feature & CPUID_STDEXT_AVX512BW) != 0;

	return (has_avx512 && __zmm_enabled());
}

 
static inline boolean_t
zfs_avx512dq_available(void)
{
	boolean_t has_avx512;

	has_avx512 = (cpu_stdext_feature & CPUID_STDEXT_AVX512F) != 0 &&
	    (cpu_stdext_feature & CPUID_STDEXT_AVX512DQ) != 0;

	return (has_avx512 && __zmm_enabled());
}

 
static inline boolean_t
zfs_avx512vl_available(void)
{
	boolean_t has_avx512;

	has_avx512 = (cpu_stdext_feature & CPUID_STDEXT_AVX512F) != 0 &&
	    (cpu_stdext_feature & CPUID_STDEXT_AVX512VL) != 0;

	return (has_avx512 && __zmm_enabled());
}

 
static inline boolean_t
zfs_avx512ifma_available(void)
{
	boolean_t has_avx512;

	has_avx512 = (cpu_stdext_feature & CPUID_STDEXT_AVX512F) != 0 &&
	    (cpu_stdext_feature & CPUID_STDEXT_AVX512IFMA) != 0;

	return (has_avx512 && __zmm_enabled());
}

 
static inline boolean_t
zfs_avx512vbmi_available(void)
{
	boolean_t has_avx512;

	has_avx512 = (cpu_stdext_feature & CPUID_STDEXT_AVX512F) != 0 &&
	    (cpu_stdext_feature & CPUID_STDEXT_BMI1) != 0;

	return (has_avx512 && __zmm_enabled());
}
