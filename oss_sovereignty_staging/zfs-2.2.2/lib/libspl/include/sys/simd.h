 
 

#ifndef _LIBSPL_SYS_SIMD_H
#define	_LIBSPL_SYS_SIMD_H

#include <sys/isa_defs.h>
#include <sys/types.h>

 
#if defined(__arm__) || defined(__aarch64__) || defined(__powerpc__)
#if defined(__FreeBSD__)
#define	AT_HWCAP	25
#define	AT_HWCAP2	26
extern int elf_aux_info(int aux, void *buf, int buflen);
static inline unsigned long getauxval(unsigned long key)
{
	unsigned long val = 0UL;

	if (elf_aux_info((int)key, &val, sizeof (val)) != 0)
		return (0UL);

	return (val);
}
#elif defined(__linux__)
#define	AT_HWCAP	16
#define	AT_HWCAP2	26
extern unsigned long getauxval(unsigned long type);
#endif  
#endif  

#if defined(__x86)
#include <cpuid.h>

#define	kfpu_allowed()		1
#define	kfpu_begin()		do {} while (0)
#define	kfpu_end()		do {} while (0)
#define	kfpu_init()		0
#define	kfpu_fini()		((void) 0)

 
typedef enum cpuid_regs {
	EAX = 0,
	EBX,
	ECX,
	EDX,
	CPUID_REG_CNT = 4
} cpuid_regs_t;

 
typedef enum cpuid_inst_sets {
	SSE = 0,
	SSE2,
	SSE3,
	SSSE3,
	SSE4_1,
	SSE4_2,
	OSXSAVE,
	AVX,
	AVX2,
	BMI1,
	BMI2,
	AVX512F,
	AVX512CD,
	AVX512DQ,
	AVX512BW,
	AVX512IFMA,
	AVX512VBMI,
	AVX512PF,
	AVX512ER,
	AVX512VL,
	AES,
	PCLMULQDQ,
	MOVBE,
	SHA_NI
} cpuid_inst_sets_t;

 
typedef struct cpuid_feature_desc {
	uint32_t leaf;		 
	uint32_t subleaf;	 
	uint32_t flag;		 
	cpuid_regs_t reg;	 
} cpuid_feature_desc_t;

#define	_AVX512F_BIT		(1U << 16)
#define	_AVX512CD_BIT		(_AVX512F_BIT | (1U << 28))
#define	_AVX512DQ_BIT		(_AVX512F_BIT | (1U << 17))
#define	_AVX512BW_BIT		(_AVX512F_BIT | (1U << 30))
#define	_AVX512IFMA_BIT		(_AVX512F_BIT | (1U << 21))
#define	_AVX512VBMI_BIT		(1U << 1)  
#define	_AVX512PF_BIT		(_AVX512F_BIT | (1U << 26))
#define	_AVX512ER_BIT		(_AVX512F_BIT | (1U << 27))
#define	_AVX512VL_BIT		(1U << 31)  
#define	_AES_BIT		(1U << 25)
#define	_PCLMULQDQ_BIT		(1U << 1)
#define	_MOVBE_BIT		(1U << 22)
#define	_SHA_NI_BIT		(1U << 29)

 
static const cpuid_feature_desc_t cpuid_features[] = {
	[SSE]		= {1U, 0U,	1U << 25,	EDX	},
	[SSE2]		= {1U, 0U,	1U << 26,	EDX	},
	[SSE3]		= {1U, 0U,	1U << 0,	ECX	},
	[SSSE3]		= {1U, 0U,	1U << 9,	ECX	},
	[SSE4_1]	= {1U, 0U,	1U << 19,	ECX	},
	[SSE4_2]	= {1U, 0U,	1U << 20,	ECX	},
	[OSXSAVE]	= {1U, 0U,	1U << 27,	ECX	},
	[AVX]		= {1U, 0U,	1U << 28,	ECX	},
	[AVX2]		= {7U, 0U,	1U << 5,	EBX	},
	[BMI1]		= {7U, 0U,	1U << 3,	EBX	},
	[BMI2]		= {7U, 0U,	1U << 8,	EBX	},
	[AVX512F]	= {7U, 0U, _AVX512F_BIT,	EBX	},
	[AVX512CD]	= {7U, 0U, _AVX512CD_BIT,	EBX	},
	[AVX512DQ]	= {7U, 0U, _AVX512DQ_BIT,	EBX	},
	[AVX512BW]	= {7U, 0U, _AVX512BW_BIT,	EBX	},
	[AVX512IFMA]	= {7U, 0U, _AVX512IFMA_BIT,	EBX	},
	[AVX512VBMI]	= {7U, 0U, _AVX512VBMI_BIT,	ECX	},
	[AVX512PF]	= {7U, 0U, _AVX512PF_BIT,	EBX	},
	[AVX512ER]	= {7U, 0U, _AVX512ER_BIT,	EBX	},
	[AVX512VL]	= {7U, 0U, _AVX512ER_BIT,	EBX	},
	[AES]		= {1U, 0U, _AES_BIT,		ECX	},
	[PCLMULQDQ]	= {1U, 0U, _PCLMULQDQ_BIT,	ECX	},
	[MOVBE]		= {1U, 0U, _MOVBE_BIT,		ECX	},
	[SHA_NI]	= {7U, 0U, _SHA_NI_BIT,		EBX	},
};

 
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
__cpuid_check_feature(const cpuid_feature_desc_t *desc)
{
	uint32_t r[CPUID_REG_CNT];

	if (__get_cpuid_max(0, NULL) >= desc->leaf) {
		 
		__cpuid_count(desc->leaf, desc->subleaf,
		    r[EAX], r[EBX], r[ECX], r[EDX]);
		return ((r[desc->reg] & desc->flag) == desc->flag);
	}
	return (B_FALSE);
}

#define	CPUID_FEATURE_CHECK(name, id)				\
static inline boolean_t						\
__cpuid_has_ ## name(void)					\
{								\
	return (__cpuid_check_feature(&cpuid_features[id]));	\
}

 
CPUID_FEATURE_CHECK(sse, SSE);
CPUID_FEATURE_CHECK(sse2, SSE2);
CPUID_FEATURE_CHECK(sse3, SSE3);
CPUID_FEATURE_CHECK(ssse3, SSSE3);
CPUID_FEATURE_CHECK(sse4_1, SSE4_1);
CPUID_FEATURE_CHECK(sse4_2, SSE4_2);
CPUID_FEATURE_CHECK(avx, AVX);
CPUID_FEATURE_CHECK(avx2, AVX2);
CPUID_FEATURE_CHECK(osxsave, OSXSAVE);
CPUID_FEATURE_CHECK(bmi1, BMI1);
CPUID_FEATURE_CHECK(bmi2, BMI2);
CPUID_FEATURE_CHECK(avx512f, AVX512F);
CPUID_FEATURE_CHECK(avx512cd, AVX512CD);
CPUID_FEATURE_CHECK(avx512dq, AVX512DQ);
CPUID_FEATURE_CHECK(avx512bw, AVX512BW);
CPUID_FEATURE_CHECK(avx512ifma, AVX512IFMA);
CPUID_FEATURE_CHECK(avx512vbmi, AVX512VBMI);
CPUID_FEATURE_CHECK(avx512pf, AVX512PF);
CPUID_FEATURE_CHECK(avx512er, AVX512ER);
CPUID_FEATURE_CHECK(avx512vl, AVX512VL);
CPUID_FEATURE_CHECK(aes, AES);
CPUID_FEATURE_CHECK(pclmulqdq, PCLMULQDQ);
CPUID_FEATURE_CHECK(movbe, MOVBE);
CPUID_FEATURE_CHECK(shani, SHA_NI);

 
static inline boolean_t
__simd_state_enabled(const uint64_t state)
{
	boolean_t has_osxsave;
	uint64_t xcr0;

	has_osxsave = __cpuid_has_osxsave();
	if (!has_osxsave)
		return (B_FALSE);

	xcr0 = xgetbv(0);
	return ((xcr0 & state) == state);
}

#define	_XSTATE_SSE_AVX		(0x2 | 0x4)
#define	_XSTATE_AVX512		(0xE0 | _XSTATE_SSE_AVX)

#define	__ymm_enabled()		__simd_state_enabled(_XSTATE_SSE_AVX)
#define	__zmm_enabled()		__simd_state_enabled(_XSTATE_AVX512)

 
static inline boolean_t
zfs_sse_available(void)
{
	return (__cpuid_has_sse());
}

 
static inline boolean_t
zfs_sse2_available(void)
{
	return (__cpuid_has_sse2());
}

 
static inline boolean_t
zfs_sse3_available(void)
{
	return (__cpuid_has_sse3());
}

 
static inline boolean_t
zfs_ssse3_available(void)
{
	return (__cpuid_has_ssse3());
}

 
static inline boolean_t
zfs_sse4_1_available(void)
{
	return (__cpuid_has_sse4_1());
}

 
static inline boolean_t
zfs_sse4_2_available(void)
{
	return (__cpuid_has_sse4_2());
}

 
static inline boolean_t
zfs_avx_available(void)
{
	return (__cpuid_has_avx() && __ymm_enabled());
}

 
static inline boolean_t
zfs_avx2_available(void)
{
	return (__cpuid_has_avx2() && __ymm_enabled());
}

 
static inline boolean_t
zfs_bmi1_available(void)
{
	return (__cpuid_has_bmi1());
}

 
static inline boolean_t
zfs_bmi2_available(void)
{
	return (__cpuid_has_bmi2());
}

 
static inline boolean_t
zfs_aes_available(void)
{
	return (__cpuid_has_aes());
}

 
static inline boolean_t
zfs_pclmulqdq_available(void)
{
	return (__cpuid_has_pclmulqdq());
}

 
static inline boolean_t
zfs_movbe_available(void)
{
	return (__cpuid_has_movbe());
}

 
static inline boolean_t
zfs_shani_available(void)
{
	return (__cpuid_has_shani());
}

 

 
static inline boolean_t
zfs_avx512f_available(void)
{
	return (__cpuid_has_avx512f() && __zmm_enabled());
}

 
static inline boolean_t
zfs_avx512cd_available(void)
{
	return (__cpuid_has_avx512cd() && __zmm_enabled());
}

 
static inline boolean_t
zfs_avx512er_available(void)
{
	return (__cpuid_has_avx512er() && __zmm_enabled());
}

 
static inline boolean_t
zfs_avx512pf_available(void)
{
	return (__cpuid_has_avx512pf() && __zmm_enabled());
}

 
static inline boolean_t
zfs_avx512bw_available(void)
{
	return (__cpuid_has_avx512bw() && __zmm_enabled());
}

 
static inline boolean_t
zfs_avx512dq_available(void)
{
	return (__cpuid_has_avx512dq() && __zmm_enabled());
}

 
static inline boolean_t
zfs_avx512vl_available(void)
{
	return (__cpuid_has_avx512vl() && __zmm_enabled());
}

 
static inline boolean_t
zfs_avx512ifma_available(void)
{
	return (__cpuid_has_avx512ifma() && __zmm_enabled());
}

 
static inline boolean_t
zfs_avx512vbmi_available(void)
{
	return (__cpuid_has_avx512f() && __cpuid_has_avx512vbmi() &&
	    __zmm_enabled());
}

#elif defined(__arm__)

#define	kfpu_allowed()		1
#define	kfpu_initialize(tsk)	do {} while (0)
#define	kfpu_begin()		do {} while (0)
#define	kfpu_end()		do {} while (0)

#define	HWCAP_NEON		0x00001000
#define	HWCAP2_SHA2		0x00000008

 
static inline boolean_t
zfs_neon_available(void)
{
	unsigned long hwcap = getauxval(AT_HWCAP);
	return (hwcap & HWCAP_NEON);
}

 
static inline boolean_t
zfs_sha256_available(void)
{
	unsigned long hwcap = getauxval(AT_HWCAP);
	return (hwcap & HWCAP2_SHA2);
}

#elif defined(__aarch64__)

#define	kfpu_allowed()		1
#define	kfpu_initialize(tsk)	do {} while (0)
#define	kfpu_begin()		do {} while (0)
#define	kfpu_end()		do {} while (0)

#define	HWCAP_FP		0x00000001
#define	HWCAP_SHA2		0x00000040
#define	HWCAP_SHA512		0x00200000

 
static inline boolean_t
zfs_neon_available(void)
{
	unsigned long hwcap = getauxval(AT_HWCAP);
	return (hwcap & HWCAP_FP);
}

 
static inline boolean_t
zfs_sha256_available(void)
{
	unsigned long hwcap = getauxval(AT_HWCAP);
	return (hwcap & HWCAP_SHA2);
}

 
static inline boolean_t
zfs_sha512_available(void)
{
	unsigned long hwcap = getauxval(AT_HWCAP);
	return (hwcap & HWCAP_SHA512);
}

#elif defined(__powerpc__)

#define	kfpu_allowed()		0
#define	kfpu_initialize(tsk)	do {} while (0)
#define	kfpu_begin()		do {} while (0)
#define	kfpu_end()		do {} while (0)

#define	PPC_FEATURE_HAS_ALTIVEC	0x10000000
#define	PPC_FEATURE_HAS_VSX	0x00000080
#define	PPC_FEATURE2_ARCH_2_07	0x80000000

static inline boolean_t
zfs_altivec_available(void)
{
	unsigned long hwcap = getauxval(AT_HWCAP);
	return (hwcap & PPC_FEATURE_HAS_ALTIVEC);
}

static inline boolean_t
zfs_vsx_available(void)
{
	unsigned long hwcap = getauxval(AT_HWCAP);
	return (hwcap & PPC_FEATURE_HAS_VSX);
}

static inline boolean_t
zfs_isa207_available(void)
{
	unsigned long hwcap = getauxval(AT_HWCAP);
	unsigned long hwcap2 = getauxval(AT_HWCAP2);
	return ((hwcap & PPC_FEATURE_HAS_VSX) &&
	    (hwcap2 & PPC_FEATURE2_ARCH_2_07));
}

#else

#define	kfpu_allowed()		0
#define	kfpu_initialize(tsk)	do {} while (0)
#define	kfpu_begin()		do {} while (0)
#define	kfpu_end()		do {} while (0)

#endif

#endif  
