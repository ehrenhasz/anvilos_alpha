#ifndef _ASM_POWERPC_SECURITY_FEATURES_H
#define _ASM_POWERPC_SECURITY_FEATURES_H
extern u64 powerpc_security_features;
extern bool rfi_flush;
enum stf_barrier_type {
	STF_BARRIER_NONE	= 0x1,
	STF_BARRIER_FALLBACK	= 0x2,
	STF_BARRIER_EIEIO	= 0x4,
	STF_BARRIER_SYNC_ORI	= 0x8,
};
void setup_stf_barrier(void);
void do_stf_barrier_fixups(enum stf_barrier_type types);
void setup_count_cache_flush(void);
static inline void security_ftr_set(u64 feature)
{
	powerpc_security_features |= feature;
}
static inline void security_ftr_clear(u64 feature)
{
	powerpc_security_features &= ~feature;
}
static inline bool security_ftr_enabled(u64 feature)
{
	return !!(powerpc_security_features & feature);
}
#ifdef CONFIG_PPC_BOOK3S_64
enum stf_barrier_type stf_barrier_type_get(void);
#else
static inline enum stf_barrier_type stf_barrier_type_get(void) { return STF_BARRIER_NONE; }
#endif
#define SEC_FTR_L1D_FLUSH_ORI30		0x0000000000000001ull
#define SEC_FTR_L1D_FLUSH_TRIG2		0x0000000000000002ull
#define SEC_FTR_SPEC_BAR_ORI31		0x0000000000000004ull
#define SEC_FTR_BCCTRL_SERIALISED	0x0000000000000008ull
#define SEC_FTR_L1D_THREAD_PRIV		0x0000000000000010ull
#define SEC_FTR_COUNT_CACHE_DISABLED	0x0000000000000020ull
#define SEC_FTR_BCCTR_FLUSH_ASSIST	0x0000000000000800ull
#define SEC_FTR_BCCTR_LINK_FLUSH_ASSIST	0x0000000000002000ull
#define SEC_FTR_L1D_FLUSH_HV		0x0000000000000040ull
#define SEC_FTR_L1D_FLUSH_PR		0x0000000000000080ull
#define SEC_FTR_BNDS_CHK_SPEC_BAR	0x0000000000000100ull
#define SEC_FTR_FAVOUR_SECURITY		0x0000000000000200ull
#define SEC_FTR_FLUSH_COUNT_CACHE	0x0000000000000400ull
#define SEC_FTR_FLUSH_LINK_STACK	0x0000000000001000ull
#define SEC_FTR_L1D_FLUSH_ENTRY		0x0000000000004000ull
#define SEC_FTR_L1D_FLUSH_UACCESS	0x0000000000008000ull
#define SEC_FTR_STF_BARRIER		0x0000000000010000ull
#define SEC_FTR_DEFAULT \
	(SEC_FTR_L1D_FLUSH_HV | \
	 SEC_FTR_L1D_FLUSH_PR | \
	 SEC_FTR_BNDS_CHK_SPEC_BAR | \
	 SEC_FTR_L1D_FLUSH_ENTRY | \
	 SEC_FTR_L1D_FLUSH_UACCESS | \
	 SEC_FTR_STF_BARRIER | \
	 SEC_FTR_FAVOUR_SECURITY)
#endif  
