#ifndef __ASM_CPUFEATURE_H
#define __ASM_CPUFEATURE_H
#include <asm/alternative-macros.h>
#include <asm/cpucaps.h>
#include <asm/cputype.h>
#include <asm/hwcap.h>
#include <asm/sysreg.h>
#define MAX_CPU_FEATURES	128
#define cpu_feature(x)		KERNEL_HWCAP_ ## x
#define ARM64_SW_FEATURE_OVERRIDE_NOKASLR	0
#define ARM64_SW_FEATURE_OVERRIDE_HVHE		4
#ifndef __ASSEMBLY__
#include <linux/bug.h>
#include <linux/jump_label.h>
#include <linux/kernel.h>
enum ftr_type {
	FTR_EXACT,			 
	FTR_LOWER_SAFE,			 
	FTR_HIGHER_SAFE,		 
	FTR_HIGHER_OR_ZERO_SAFE,	 
};
#define FTR_STRICT	true	 
#define FTR_NONSTRICT	false	 
#define FTR_SIGNED	true	 
#define FTR_UNSIGNED	false	 
#define FTR_VISIBLE	true	 
#define FTR_HIDDEN	false	 
#define FTR_VISIBLE_IF_IS_ENABLED(config)		\
	(IS_ENABLED(config) ? FTR_VISIBLE : FTR_HIDDEN)
struct arm64_ftr_bits {
	bool		sign;	 
	bool		visible;
	bool		strict;	 
	enum ftr_type	type;
	u8		shift;
	u8		width;
	s64		safe_val;  
};
struct arm64_ftr_override {
	u64		val;
	u64		mask;
};
struct arm64_ftr_reg {
	const char			*name;
	u64				strict_mask;
	u64				user_mask;
	u64				sys_val;
	u64				user_val;
	struct arm64_ftr_override	*override;
	const struct arm64_ftr_bits	*ftr_bits;
};
extern struct arm64_ftr_reg arm64_ftr_reg_ctrel0;
#define ARM64_CPUCAP_SCOPE_LOCAL_CPU		((u16)BIT(0))
#define ARM64_CPUCAP_SCOPE_SYSTEM		((u16)BIT(1))
#define ARM64_CPUCAP_SCOPE_BOOT_CPU		((u16)BIT(2))
#define ARM64_CPUCAP_SCOPE_MASK			\
	(ARM64_CPUCAP_SCOPE_SYSTEM	|	\
	 ARM64_CPUCAP_SCOPE_LOCAL_CPU	|	\
	 ARM64_CPUCAP_SCOPE_BOOT_CPU)
#define SCOPE_SYSTEM				ARM64_CPUCAP_SCOPE_SYSTEM
#define SCOPE_LOCAL_CPU				ARM64_CPUCAP_SCOPE_LOCAL_CPU
#define SCOPE_BOOT_CPU				ARM64_CPUCAP_SCOPE_BOOT_CPU
#define SCOPE_ALL				ARM64_CPUCAP_SCOPE_MASK
#define ARM64_CPUCAP_PERMITTED_FOR_LATE_CPU	((u16)BIT(4))
#define ARM64_CPUCAP_OPTIONAL_FOR_LATE_CPU	((u16)BIT(5))
#define ARM64_CPUCAP_PANIC_ON_CONFLICT		((u16)BIT(6))
#define ARM64_CPUCAP_LOCAL_CPU_ERRATUM		\
	(ARM64_CPUCAP_SCOPE_LOCAL_CPU | ARM64_CPUCAP_OPTIONAL_FOR_LATE_CPU)
#define ARM64_CPUCAP_SYSTEM_FEATURE	\
	(ARM64_CPUCAP_SCOPE_SYSTEM | ARM64_CPUCAP_PERMITTED_FOR_LATE_CPU)
#define ARM64_CPUCAP_WEAK_LOCAL_CPU_FEATURE		\
	(ARM64_CPUCAP_SCOPE_LOCAL_CPU		|	\
	 ARM64_CPUCAP_OPTIONAL_FOR_LATE_CPU	|	\
	 ARM64_CPUCAP_PERMITTED_FOR_LATE_CPU)
#define ARM64_CPUCAP_BOOT_RESTRICTED_CPU_LOCAL_FEATURE	\
	(ARM64_CPUCAP_SCOPE_LOCAL_CPU		|	\
	 ARM64_CPUCAP_OPTIONAL_FOR_LATE_CPU)
#define ARM64_CPUCAP_STRICT_BOOT_CPU_FEATURE		\
	(ARM64_CPUCAP_SCOPE_BOOT_CPU | ARM64_CPUCAP_PANIC_ON_CONFLICT)
#define ARM64_CPUCAP_BOOT_CPU_FEATURE                  \
	(ARM64_CPUCAP_SCOPE_BOOT_CPU | ARM64_CPUCAP_PERMITTED_FOR_LATE_CPU)
struct arm64_cpu_capabilities {
	const char *desc;
	u16 capability;
	u16 type;
	bool (*matches)(const struct arm64_cpu_capabilities *caps, int scope);
	void (*cpu_enable)(const struct arm64_cpu_capabilities *cap);
	union {
		struct {	 
			struct midr_range midr_range;
			const struct arm64_midr_revidr {
				u32 midr_rv;		 
				u32 revidr_mask;
			} * const fixed_revs;
		};
		const struct midr_range *midr_range_list;
		struct {	 
			u32 sys_reg;
			u8 field_pos;
			u8 field_width;
			u8 min_field_value;
			u8 hwcap_type;
			bool sign;
			unsigned long hwcap;
		};
	};
	const struct arm64_cpu_capabilities *match_list;
};
static inline int cpucap_default_scope(const struct arm64_cpu_capabilities *cap)
{
	return cap->type & ARM64_CPUCAP_SCOPE_MASK;
}
static inline bool
cpucap_multi_entry_cap_matches(const struct arm64_cpu_capabilities *entry,
			       int scope)
{
	const struct arm64_cpu_capabilities *caps;
	for (caps = entry->match_list; caps->matches; caps++)
		if (caps->matches(caps, scope))
			return true;
	return false;
}
static __always_inline bool is_vhe_hyp_code(void)
{
	return __is_defined(__KVM_VHE_HYPERVISOR__);
}
static __always_inline bool is_nvhe_hyp_code(void)
{
	return __is_defined(__KVM_NVHE_HYPERVISOR__);
}
static __always_inline bool is_hyp_code(void)
{
	return is_vhe_hyp_code() || is_nvhe_hyp_code();
}
extern DECLARE_BITMAP(system_cpucaps, ARM64_NCAPS);
extern DECLARE_BITMAP(boot_cpucaps, ARM64_NCAPS);
#define for_each_available_cap(cap)		\
	for_each_set_bit(cap, system_cpucaps, ARM64_NCAPS)
bool this_cpu_has_cap(unsigned int cap);
void cpu_set_feature(unsigned int num);
bool cpu_have_feature(unsigned int num);
unsigned long cpu_get_elf_hwcap(void);
unsigned long cpu_get_elf_hwcap2(void);
#define cpu_set_named_feature(name) cpu_set_feature(cpu_feature(name))
#define cpu_have_named_feature(name) cpu_have_feature(cpu_feature(name))
static __always_inline bool system_capabilities_finalized(void)
{
	return alternative_has_cap_likely(ARM64_ALWAYS_SYSTEM);
}
static __always_inline bool cpus_have_cap(unsigned int num)
{
	if (num >= ARM64_NCAPS)
		return false;
	return arch_test_bit(num, system_cpucaps);
}
static __always_inline bool __cpus_have_const_cap(int num)
{
	if (num >= ARM64_NCAPS)
		return false;
	return alternative_has_cap_unlikely(num);
}
static __always_inline bool cpus_have_final_cap(int num)
{
	if (system_capabilities_finalized())
		return __cpus_have_const_cap(num);
	else
		BUG();
}
static __always_inline bool cpus_have_const_cap(int num)
{
	if (is_hyp_code())
		return cpus_have_final_cap(num);
	else if (system_capabilities_finalized())
		return __cpus_have_const_cap(num);
	else
		return cpus_have_cap(num);
}
static inline int __attribute_const__
cpuid_feature_extract_signed_field_width(u64 features, int field, int width)
{
	return (s64)(features << (64 - width - field)) >> (64 - width);
}
static inline int __attribute_const__
cpuid_feature_extract_signed_field(u64 features, int field)
{
	return cpuid_feature_extract_signed_field_width(features, field, 4);
}
static __always_inline unsigned int __attribute_const__
cpuid_feature_extract_unsigned_field_width(u64 features, int field, int width)
{
	return (u64)(features << (64 - width - field)) >> (64 - width);
}
static __always_inline unsigned int __attribute_const__
cpuid_feature_extract_unsigned_field(u64 features, int field)
{
	return cpuid_feature_extract_unsigned_field_width(features, field, 4);
}
static inline u64 __attribute_const__
cpuid_feature_cap_perfmon_field(u64 features, int field, u64 cap)
{
	u64 val = cpuid_feature_extract_unsigned_field(features, field);
	u64 mask = GENMASK_ULL(field + 3, field);
	if (val == ID_AA64DFR0_EL1_PMUVer_IMP_DEF)
		val = 0;
	if (val > cap) {
		features &= ~mask;
		features |= (cap << field) & mask;
	}
	return features;
}
static inline u64 arm64_ftr_mask(const struct arm64_ftr_bits *ftrp)
{
	return (u64)GENMASK(ftrp->shift + ftrp->width - 1, ftrp->shift);
}
static inline u64 arm64_ftr_reg_user_value(const struct arm64_ftr_reg *reg)
{
	return (reg->user_val | (reg->sys_val & reg->user_mask));
}
static inline int __attribute_const__
cpuid_feature_extract_field_width(u64 features, int field, int width, bool sign)
{
	if (WARN_ON_ONCE(!width))
		width = 4;
	return (sign) ?
		cpuid_feature_extract_signed_field_width(features, field, width) :
		cpuid_feature_extract_unsigned_field_width(features, field, width);
}
static inline int __attribute_const__
cpuid_feature_extract_field(u64 features, int field, bool sign)
{
	return cpuid_feature_extract_field_width(features, field, 4, sign);
}
static inline s64 arm64_ftr_value(const struct arm64_ftr_bits *ftrp, u64 val)
{
	return (s64)cpuid_feature_extract_field_width(val, ftrp->shift, ftrp->width, ftrp->sign);
}
static inline bool id_aa64mmfr0_mixed_endian_el0(u64 mmfr0)
{
	return cpuid_feature_extract_unsigned_field(mmfr0, ID_AA64MMFR0_EL1_BIGEND_SHIFT) == 0x1 ||
		cpuid_feature_extract_unsigned_field(mmfr0, ID_AA64MMFR0_EL1_BIGENDEL0_SHIFT) == 0x1;
}
static inline bool id_aa64pfr0_32bit_el1(u64 pfr0)
{
	u32 val = cpuid_feature_extract_unsigned_field(pfr0, ID_AA64PFR0_EL1_EL1_SHIFT);
	return val == ID_AA64PFR0_EL1_ELx_32BIT_64BIT;
}
static inline bool id_aa64pfr0_32bit_el0(u64 pfr0)
{
	u32 val = cpuid_feature_extract_unsigned_field(pfr0, ID_AA64PFR0_EL1_EL0_SHIFT);
	return val == ID_AA64PFR0_EL1_ELx_32BIT_64BIT;
}
static inline bool id_aa64pfr0_sve(u64 pfr0)
{
	u32 val = cpuid_feature_extract_unsigned_field(pfr0, ID_AA64PFR0_EL1_SVE_SHIFT);
	return val > 0;
}
static inline bool id_aa64pfr1_sme(u64 pfr1)
{
	u32 val = cpuid_feature_extract_unsigned_field(pfr1, ID_AA64PFR1_EL1_SME_SHIFT);
	return val > 0;
}
static inline bool id_aa64pfr1_mte(u64 pfr1)
{
	u32 val = cpuid_feature_extract_unsigned_field(pfr1, ID_AA64PFR1_EL1_MTE_SHIFT);
	return val >= ID_AA64PFR1_EL1_MTE_MTE2;
}
void __init setup_cpu_features(void);
void check_local_cpu_capabilities(void);
u64 read_sanitised_ftr_reg(u32 id);
u64 __read_sysreg_by_encoding(u32 sys_id);
static inline bool cpu_supports_mixed_endian_el0(void)
{
	return id_aa64mmfr0_mixed_endian_el0(read_cpuid(ID_AA64MMFR0_EL1));
}
static inline bool supports_csv2p3(int scope)
{
	u64 pfr0;
	u8 csv2_val;
	if (scope == SCOPE_LOCAL_CPU)
		pfr0 = read_sysreg_s(SYS_ID_AA64PFR0_EL1);
	else
		pfr0 = read_sanitised_ftr_reg(SYS_ID_AA64PFR0_EL1);
	csv2_val = cpuid_feature_extract_unsigned_field(pfr0,
							ID_AA64PFR0_EL1_CSV2_SHIFT);
	return csv2_val == 3;
}
static inline bool supports_clearbhb(int scope)
{
	u64 isar2;
	if (scope == SCOPE_LOCAL_CPU)
		isar2 = read_sysreg_s(SYS_ID_AA64ISAR2_EL1);
	else
		isar2 = read_sanitised_ftr_reg(SYS_ID_AA64ISAR2_EL1);
	return cpuid_feature_extract_unsigned_field(isar2,
						    ID_AA64ISAR2_EL1_CLRBHB_SHIFT);
}
const struct cpumask *system_32bit_el0_cpumask(void);
DECLARE_STATIC_KEY_FALSE(arm64_mismatched_32bit_el0);
static inline bool system_supports_32bit_el0(void)
{
	u64 pfr0 = read_sanitised_ftr_reg(SYS_ID_AA64PFR0_EL1);
	return static_branch_unlikely(&arm64_mismatched_32bit_el0) ||
	       id_aa64pfr0_32bit_el0(pfr0);
}
static inline bool system_supports_4kb_granule(void)
{
	u64 mmfr0;
	u32 val;
	mmfr0 =	read_sanitised_ftr_reg(SYS_ID_AA64MMFR0_EL1);
	val = cpuid_feature_extract_unsigned_field(mmfr0,
						ID_AA64MMFR0_EL1_TGRAN4_SHIFT);
	return (val >= ID_AA64MMFR0_EL1_TGRAN4_SUPPORTED_MIN) &&
	       (val <= ID_AA64MMFR0_EL1_TGRAN4_SUPPORTED_MAX);
}
static inline bool system_supports_64kb_granule(void)
{
	u64 mmfr0;
	u32 val;
	mmfr0 =	read_sanitised_ftr_reg(SYS_ID_AA64MMFR0_EL1);
	val = cpuid_feature_extract_unsigned_field(mmfr0,
						ID_AA64MMFR0_EL1_TGRAN64_SHIFT);
	return (val >= ID_AA64MMFR0_EL1_TGRAN64_SUPPORTED_MIN) &&
	       (val <= ID_AA64MMFR0_EL1_TGRAN64_SUPPORTED_MAX);
}
static inline bool system_supports_16kb_granule(void)
{
	u64 mmfr0;
	u32 val;
	mmfr0 =	read_sanitised_ftr_reg(SYS_ID_AA64MMFR0_EL1);
	val = cpuid_feature_extract_unsigned_field(mmfr0,
						ID_AA64MMFR0_EL1_TGRAN16_SHIFT);
	return (val >= ID_AA64MMFR0_EL1_TGRAN16_SUPPORTED_MIN) &&
	       (val <= ID_AA64MMFR0_EL1_TGRAN16_SUPPORTED_MAX);
}
static inline bool system_supports_mixed_endian_el0(void)
{
	return id_aa64mmfr0_mixed_endian_el0(read_sanitised_ftr_reg(SYS_ID_AA64MMFR0_EL1));
}
static inline bool system_supports_mixed_endian(void)
{
	u64 mmfr0;
	u32 val;
	mmfr0 =	read_sanitised_ftr_reg(SYS_ID_AA64MMFR0_EL1);
	val = cpuid_feature_extract_unsigned_field(mmfr0,
						ID_AA64MMFR0_EL1_BIGEND_SHIFT);
	return val == 0x1;
}
static __always_inline bool system_supports_fpsimd(void)
{
	return !cpus_have_const_cap(ARM64_HAS_NO_FPSIMD);
}
static inline bool system_uses_hw_pan(void)
{
	return IS_ENABLED(CONFIG_ARM64_PAN) &&
		cpus_have_const_cap(ARM64_HAS_PAN);
}
static inline bool system_uses_ttbr0_pan(void)
{
	return IS_ENABLED(CONFIG_ARM64_SW_TTBR0_PAN) &&
		!system_uses_hw_pan();
}
static __always_inline bool system_supports_sve(void)
{
	return IS_ENABLED(CONFIG_ARM64_SVE) &&
		cpus_have_const_cap(ARM64_SVE);
}
static __always_inline bool system_supports_sme(void)
{
	return IS_ENABLED(CONFIG_ARM64_SME) &&
		cpus_have_const_cap(ARM64_SME);
}
static __always_inline bool system_supports_sme2(void)
{
	return IS_ENABLED(CONFIG_ARM64_SME) &&
		cpus_have_const_cap(ARM64_SME2);
}
static __always_inline bool system_supports_fa64(void)
{
	return IS_ENABLED(CONFIG_ARM64_SME) &&
		cpus_have_const_cap(ARM64_SME_FA64);
}
static __always_inline bool system_supports_tpidr2(void)
{
	return system_supports_sme();
}
static __always_inline bool system_supports_cnp(void)
{
	return IS_ENABLED(CONFIG_ARM64_CNP) &&
		cpus_have_const_cap(ARM64_HAS_CNP);
}
static inline bool system_supports_address_auth(void)
{
	return IS_ENABLED(CONFIG_ARM64_PTR_AUTH) &&
		cpus_have_const_cap(ARM64_HAS_ADDRESS_AUTH);
}
static inline bool system_supports_generic_auth(void)
{
	return IS_ENABLED(CONFIG_ARM64_PTR_AUTH) &&
		cpus_have_const_cap(ARM64_HAS_GENERIC_AUTH);
}
static inline bool system_has_full_ptr_auth(void)
{
	return system_supports_address_auth() && system_supports_generic_auth();
}
static __always_inline bool system_uses_irq_prio_masking(void)
{
	return IS_ENABLED(CONFIG_ARM64_PSEUDO_NMI) &&
	       cpus_have_const_cap(ARM64_HAS_GIC_PRIO_MASKING);
}
static inline bool system_supports_mte(void)
{
	return IS_ENABLED(CONFIG_ARM64_MTE) &&
		cpus_have_const_cap(ARM64_MTE);
}
static inline bool system_has_prio_mask_debugging(void)
{
	return IS_ENABLED(CONFIG_ARM64_DEBUG_PRIORITY_MASKING) &&
	       system_uses_irq_prio_masking();
}
static inline bool system_supports_bti(void)
{
	return IS_ENABLED(CONFIG_ARM64_BTI) && cpus_have_const_cap(ARM64_BTI);
}
static inline bool system_supports_tlb_range(void)
{
	return IS_ENABLED(CONFIG_ARM64_TLB_RANGE) &&
		cpus_have_const_cap(ARM64_HAS_TLB_RANGE);
}
int do_emulate_mrs(struct pt_regs *regs, u32 sys_reg, u32 rt);
bool try_emulate_mrs(struct pt_regs *regs, u32 isn);
static inline u32 id_aa64mmfr0_parange_to_phys_shift(int parange)
{
	switch (parange) {
	case ID_AA64MMFR0_EL1_PARANGE_32: return 32;
	case ID_AA64MMFR0_EL1_PARANGE_36: return 36;
	case ID_AA64MMFR0_EL1_PARANGE_40: return 40;
	case ID_AA64MMFR0_EL1_PARANGE_42: return 42;
	case ID_AA64MMFR0_EL1_PARANGE_44: return 44;
	case ID_AA64MMFR0_EL1_PARANGE_48: return 48;
	case ID_AA64MMFR0_EL1_PARANGE_52: return 52;
	default: return CONFIG_ARM64_PA_BITS;
	}
}
static inline bool cpu_has_hw_af(void)
{
	u64 mmfr1;
	if (!IS_ENABLED(CONFIG_ARM64_HW_AFDBM))
		return false;
	mmfr1 = read_sanitised_ftr_reg(SYS_ID_AA64MMFR1_EL1);
	return cpuid_feature_extract_unsigned_field(mmfr1,
						ID_AA64MMFR1_EL1_HAFDBS_SHIFT);
}
static inline bool cpu_has_pan(void)
{
	u64 mmfr1 = read_cpuid(ID_AA64MMFR1_EL1);
	return cpuid_feature_extract_unsigned_field(mmfr1,
						    ID_AA64MMFR1_EL1_PAN_SHIFT);
}
#ifdef CONFIG_ARM64_AMU_EXTN
extern bool cpu_has_amu_feat(int cpu);
#else
static inline bool cpu_has_amu_feat(int cpu)
{
	return false;
}
#endif
extern int get_cpu_with_amu_feat(void);
static inline unsigned int get_vmid_bits(u64 mmfr1)
{
	int vmid_bits;
	vmid_bits = cpuid_feature_extract_unsigned_field(mmfr1,
						ID_AA64MMFR1_EL1_VMIDBits_SHIFT);
	if (vmid_bits == ID_AA64MMFR1_EL1_VMIDBits_16)
		return 16;
	return 8;
}
s64 arm64_ftr_safe_value(const struct arm64_ftr_bits *ftrp, s64 new, s64 cur);
struct arm64_ftr_reg *get_arm64_ftr_reg(u32 sys_id);
extern struct arm64_ftr_override id_aa64mmfr1_override;
extern struct arm64_ftr_override id_aa64pfr0_override;
extern struct arm64_ftr_override id_aa64pfr1_override;
extern struct arm64_ftr_override id_aa64zfr0_override;
extern struct arm64_ftr_override id_aa64smfr0_override;
extern struct arm64_ftr_override id_aa64isar1_override;
extern struct arm64_ftr_override id_aa64isar2_override;
extern struct arm64_ftr_override arm64_sw_feature_override;
u32 get_kvm_ipa_limit(void);
void dump_cpu_features(void);
#endif  
#endif
