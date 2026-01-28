#ifndef _LINUX_SIMD_X86_H
#define	_LINUX_SIMD_X86_H
#if defined(__x86)
#include <sys/types.h>
#include <asm/cpufeature.h>
#if defined(CONFIG_X86_DEBUG_FPU) && !defined(KERNEL_EXPORTS_X86_FPU)
#undef CONFIG_X86_DEBUG_FPU
#endif
#if defined(KERNEL_EXPORTS_X86_FPU)
#if defined(HAVE_KERNEL_FPU_API_HEADER)
#include <asm/fpu/api.h>
#if defined(HAVE_KERNEL_FPU_INTERNAL_HEADER)
#include <asm/fpu/internal.h>
#endif
#else
#include <asm/i387.h>
#endif
#define	kfpu_allowed()		1
#define	kfpu_init()		0
#define	kfpu_fini()		((void) 0)
#if defined(HAVE_UNDERSCORE_KERNEL_FPU)
#define	kfpu_begin()		\
{				\
	preempt_disable();	\
	__kernel_fpu_begin();	\
}
#define	kfpu_end()		\
{				\
	__kernel_fpu_end();	\
	preempt_enable();	\
}
#elif defined(HAVE_KERNEL_FPU)
#define	kfpu_begin()		kernel_fpu_begin()
#define	kfpu_end()		kernel_fpu_end()
#else
#error "Unreachable kernel configuration"
#endif
#else  
#if defined(HAVE_KERNEL_FPU_INTERNAL)
#if !defined(HAVE_XSAVE)
#error "Toolchain needs to support the XSAVE assembler instruction"
#endif
#ifndef XFEATURE_MASK_XTILE
#define	XFEATURE_MASK_XTILE	0x60000
#endif
#include <linux/mm.h>
#include <linux/slab.h>
extern uint8_t **zfs_kfpu_fpregs;
static inline uint32_t
get_xsave_area_size(void)
{
	if (!boot_cpu_has(X86_FEATURE_OSXSAVE)) {
		return (0);
	}
	uint32_t eax, ebx, ecx, edx;
	eax = 13U;
	ecx = 0U;
	__asm__ __volatile__("cpuid"
	    : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
	    : "a" (eax), "c" (ecx));
	return (ecx);
}
static inline int
get_fpuregs_save_area_order(void)
{
	size_t area_size = (size_t)get_xsave_area_size();
	if (area_size == 0) {
		area_size = 512;
	}
	return (get_order(area_size));
}
static inline void
kfpu_fini(void)
{
	int cpu;
	int order = get_fpuregs_save_area_order();
	for_each_possible_cpu(cpu) {
		if (zfs_kfpu_fpregs[cpu] != NULL) {
			free_pages((unsigned long)zfs_kfpu_fpregs[cpu], order);
		}
	}
	kfree(zfs_kfpu_fpregs);
}
static inline int
kfpu_init(void)
{
	zfs_kfpu_fpregs = kzalloc(num_possible_cpus() * sizeof (uint8_t *),
	    GFP_KERNEL);
	if (zfs_kfpu_fpregs == NULL)
		return (-ENOMEM);
	int cpu;
	int order = get_fpuregs_save_area_order();
	for_each_possible_cpu(cpu) {
		struct page *page = alloc_pages_node(cpu_to_node(cpu),
		    GFP_KERNEL | __GFP_ZERO, order);
		if (page == NULL) {
			kfpu_fini();
			return (-ENOMEM);
		}
		zfs_kfpu_fpregs[cpu] = page_address(page);
	}
	return (0);
}
#define	kfpu_allowed()		1
#define	__asm			__asm__ __volatile__
#define	kfpu_fxsave(addr)	__asm("fxsave %0" : "=m" (*(addr)))
#define	kfpu_fxsaveq(addr)	__asm("fxsaveq %0" : "=m" (*(addr)))
#define	kfpu_fnsave(addr)	__asm("fnsave %0; fwait" : "=m" (*(addr)))
#define	kfpu_fxrstor(addr)	__asm("fxrstor %0" : : "m" (*(addr)))
#define	kfpu_fxrstorq(addr)	__asm("fxrstorq %0" : : "m" (*(addr)))
#define	kfpu_frstor(addr)	__asm("frstor %0" : : "m" (*(addr)))
#define	kfpu_fxsr_clean(rval)	__asm("fnclex; emms; fildl %P[addr]" \
				    : : [addr] "m" (rval));
#define	kfpu_do_xsave(instruction, addr, mask)			\
{								\
	uint32_t low, hi;					\
								\
	low = mask;						\
	hi = (uint64_t)(mask) >> 32;				\
	__asm(instruction " %[dst]\n\t"				\
	    :							\
	    : [dst] "m" (*(addr)), "a" (low), "d" (hi)		\
	    : "memory");					\
}
static inline void
kfpu_save_fxsr(uint8_t  *addr)
{
	if (IS_ENABLED(CONFIG_X86_32))
		kfpu_fxsave(addr);
	else
		kfpu_fxsaveq(addr);
}
static inline void
kfpu_save_fsave(uint8_t *addr)
{
	kfpu_fnsave(addr);
}
static inline void
kfpu_begin(void)
{
	preempt_disable();
	local_irq_disable();
	uint8_t *state = zfs_kfpu_fpregs[smp_processor_id()];
#if defined(HAVE_XSAVES)
	if (static_cpu_has(X86_FEATURE_XSAVES)) {
		kfpu_do_xsave("xsaves", state, ~XFEATURE_MASK_XTILE);
		return;
	}
#endif
#if defined(HAVE_XSAVEOPT)
	if (static_cpu_has(X86_FEATURE_XSAVEOPT)) {
		kfpu_do_xsave("xsaveopt", state, ~XFEATURE_MASK_XTILE);
		return;
	}
#endif
	if (static_cpu_has(X86_FEATURE_XSAVE)) {
		kfpu_do_xsave("xsave", state, ~XFEATURE_MASK_XTILE);
	} else if (static_cpu_has(X86_FEATURE_FXSR)) {
		kfpu_save_fxsr(state);
	} else {
		kfpu_save_fsave(state);
	}
}
#define	kfpu_do_xrstor(instruction, addr, mask)			\
{								\
	uint32_t low, hi;					\
								\
	low = mask;						\
	hi = (uint64_t)(mask) >> 32;				\
	__asm(instruction " %[src]"				\
	    :							\
	    : [src] "m" (*(addr)), "a" (low), "d" (hi)		\
	    : "memory");					\
}
static inline void
kfpu_restore_fxsr(uint8_t *addr)
{
	if (unlikely(static_cpu_has_bug(X86_BUG_FXSAVE_LEAK)))
		kfpu_fxsr_clean(addr);
	if (IS_ENABLED(CONFIG_X86_32)) {
		kfpu_fxrstor(addr);
	} else {
		kfpu_fxrstorq(addr);
	}
}
static inline void
kfpu_restore_fsave(uint8_t *addr)
{
	kfpu_frstor(addr);
}
static inline void
kfpu_end(void)
{
	uint8_t  *state = zfs_kfpu_fpregs[smp_processor_id()];
#if defined(HAVE_XSAVES)
	if (static_cpu_has(X86_FEATURE_XSAVES)) {
		kfpu_do_xrstor("xrstors", state, ~XFEATURE_MASK_XTILE);
		goto out;
	}
#endif
	if (static_cpu_has(X86_FEATURE_XSAVE)) {
		kfpu_do_xrstor("xrstor", state, ~XFEATURE_MASK_XTILE);
	} else if (static_cpu_has(X86_FEATURE_FXSR)) {
		kfpu_restore_fxsr(state);
	} else {
		kfpu_restore_fsave(state);
	}
out:
	local_irq_enable();
	preempt_enable();
}
#else
#error	"Exactly one of KERNEL_EXPORTS_X86_FPU or HAVE_KERNEL_FPU_INTERNAL" \
	" must be defined"
#endif  
#endif  
static inline uint64_t
zfs_xgetbv(uint32_t index)
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
#if defined(X86_FEATURE_OSXSAVE)
	has_osxsave = !!boot_cpu_has(X86_FEATURE_OSXSAVE);
#else
	has_osxsave = B_FALSE;
#endif
	if (!has_osxsave)
		return (B_FALSE);
	xcr0 = zfs_xgetbv(0);
	return ((xcr0 & state) == state);
}
#define	_XSTATE_SSE_AVX		(0x2 | 0x4)
#define	_XSTATE_AVX512		(0xE0 | _XSTATE_SSE_AVX)
#define	__ymm_enabled() __simd_state_enabled(_XSTATE_SSE_AVX)
#define	__zmm_enabled() __simd_state_enabled(_XSTATE_AVX512)
static inline boolean_t
zfs_sse_available(void)
{
	return (!!boot_cpu_has(X86_FEATURE_XMM));
}
static inline boolean_t
zfs_sse2_available(void)
{
	return (!!boot_cpu_has(X86_FEATURE_XMM2));
}
static inline boolean_t
zfs_sse3_available(void)
{
	return (!!boot_cpu_has(X86_FEATURE_XMM3));
}
static inline boolean_t
zfs_ssse3_available(void)
{
	return (!!boot_cpu_has(X86_FEATURE_SSSE3));
}
static inline boolean_t
zfs_sse4_1_available(void)
{
	return (!!boot_cpu_has(X86_FEATURE_XMM4_1));
}
static inline boolean_t
zfs_sse4_2_available(void)
{
	return (!!boot_cpu_has(X86_FEATURE_XMM4_2));
}
static inline boolean_t
zfs_avx_available(void)
{
	return (boot_cpu_has(X86_FEATURE_AVX) && __ymm_enabled());
}
static inline boolean_t
zfs_avx2_available(void)
{
	return (boot_cpu_has(X86_FEATURE_AVX2) && __ymm_enabled());
}
static inline boolean_t
zfs_bmi1_available(void)
{
#if defined(X86_FEATURE_BMI1)
	return (!!boot_cpu_has(X86_FEATURE_BMI1));
#else
	return (B_FALSE);
#endif
}
static inline boolean_t
zfs_bmi2_available(void)
{
#if defined(X86_FEATURE_BMI2)
	return (!!boot_cpu_has(X86_FEATURE_BMI2));
#else
	return (B_FALSE);
#endif
}
static inline boolean_t
zfs_aes_available(void)
{
#if defined(X86_FEATURE_AES)
	return (!!boot_cpu_has(X86_FEATURE_AES));
#else
	return (B_FALSE);
#endif
}
static inline boolean_t
zfs_pclmulqdq_available(void)
{
#if defined(X86_FEATURE_PCLMULQDQ)
	return (!!boot_cpu_has(X86_FEATURE_PCLMULQDQ));
#else
	return (B_FALSE);
#endif
}
static inline boolean_t
zfs_movbe_available(void)
{
#if defined(X86_FEATURE_MOVBE)
	return (!!boot_cpu_has(X86_FEATURE_MOVBE));
#else
	return (B_FALSE);
#endif
}
static inline boolean_t
zfs_shani_available(void)
{
#if defined(X86_FEATURE_SHA_NI)
	return (!!boot_cpu_has(X86_FEATURE_SHA_NI));
#else
	return (B_FALSE);
#endif
}
static inline boolean_t
zfs_avx512f_available(void)
{
	boolean_t has_avx512 = B_FALSE;
#if defined(X86_FEATURE_AVX512F)
	has_avx512 = !!boot_cpu_has(X86_FEATURE_AVX512F);
#endif
	return (has_avx512 && __zmm_enabled());
}
static inline boolean_t
zfs_avx512cd_available(void)
{
	boolean_t has_avx512 = B_FALSE;
#if defined(X86_FEATURE_AVX512CD)
	has_avx512 = boot_cpu_has(X86_FEATURE_AVX512F) &&
	    boot_cpu_has(X86_FEATURE_AVX512CD);
#endif
	return (has_avx512 && __zmm_enabled());
}
static inline boolean_t
zfs_avx512er_available(void)
{
	boolean_t has_avx512 = B_FALSE;
#if defined(X86_FEATURE_AVX512ER)
	has_avx512 = boot_cpu_has(X86_FEATURE_AVX512F) &&
	    boot_cpu_has(X86_FEATURE_AVX512ER);
#endif
	return (has_avx512 && __zmm_enabled());
}
static inline boolean_t
zfs_avx512pf_available(void)
{
	boolean_t has_avx512 = B_FALSE;
#if defined(X86_FEATURE_AVX512PF)
	has_avx512 = boot_cpu_has(X86_FEATURE_AVX512F) &&
	    boot_cpu_has(X86_FEATURE_AVX512PF);
#endif
	return (has_avx512 && __zmm_enabled());
}
static inline boolean_t
zfs_avx512bw_available(void)
{
	boolean_t has_avx512 = B_FALSE;
#if defined(X86_FEATURE_AVX512BW)
	has_avx512 = boot_cpu_has(X86_FEATURE_AVX512F) &&
	    boot_cpu_has(X86_FEATURE_AVX512BW);
#endif
	return (has_avx512 && __zmm_enabled());
}
static inline boolean_t
zfs_avx512dq_available(void)
{
	boolean_t has_avx512 = B_FALSE;
#if defined(X86_FEATURE_AVX512DQ)
	has_avx512 = boot_cpu_has(X86_FEATURE_AVX512F) &&
	    boot_cpu_has(X86_FEATURE_AVX512DQ);
#endif
	return (has_avx512 && __zmm_enabled());
}
static inline boolean_t
zfs_avx512vl_available(void)
{
	boolean_t has_avx512 = B_FALSE;
#if defined(X86_FEATURE_AVX512VL)
	has_avx512 = boot_cpu_has(X86_FEATURE_AVX512F) &&
	    boot_cpu_has(X86_FEATURE_AVX512VL);
#endif
	return (has_avx512 && __zmm_enabled());
}
static inline boolean_t
zfs_avx512ifma_available(void)
{
	boolean_t has_avx512 = B_FALSE;
#if defined(X86_FEATURE_AVX512IFMA)
	has_avx512 = boot_cpu_has(X86_FEATURE_AVX512F) &&
	    boot_cpu_has(X86_FEATURE_AVX512IFMA);
#endif
	return (has_avx512 && __zmm_enabled());
}
static inline boolean_t
zfs_avx512vbmi_available(void)
{
	boolean_t has_avx512 = B_FALSE;
#if defined(X86_FEATURE_AVX512VBMI)
	has_avx512 = boot_cpu_has(X86_FEATURE_AVX512F) &&
	    boot_cpu_has(X86_FEATURE_AVX512VBMI);
#endif
	return (has_avx512 && __zmm_enabled());
}
#endif  
#endif  
