#ifndef __ARM_KVM_INIT_H__
#define __ARM_KVM_INIT_H__
#ifndef __ASSEMBLY__
#error Assembly-only header
#endif
#include <asm/kvm_arm.h>
#include <asm/ptrace.h>
#include <asm/sysreg.h>
#include <linux/irqchip/arm-gic-v3.h>
.macro __init_el2_sctlr
	mov_q	x0, INIT_SCTLR_EL2_MMU_OFF
	msr	sctlr_el2, x0
	isb
.endm
.macro __init_el2_hcrx
	mrs	x0, id_aa64mmfr1_el1
	ubfx	x0, x0, #ID_AA64MMFR1_EL1_HCX_SHIFT, #4
	cbz	x0, .Lskip_hcrx_\@
	mov_q	x0, HCRX_HOST_FLAGS
	msr_s	SYS_HCRX_EL2, x0
.Lskip_hcrx_\@:
.endm
.macro __check_hvhe fail, tmp
	mrs	\tmp, hcr_el2
	and	\tmp, \tmp, #HCR_E2H
	cbz	\tmp, \fail
.endm
.macro __init_el2_timers
	mov	x0, #3				 
	__check_hvhe .LnVHE_\@, x1
	lsl	x0, x0, #10
.LnVHE_\@:
	msr	cnthctl_el2, x0
	msr	cntvoff_el2, xzr		 
.endm
.macro __init_el2_debug
	mrs	x1, id_aa64dfr0_el1
	sbfx	x0, x1, #ID_AA64DFR0_EL1_PMUVer_SHIFT, #4
	cmp	x0, #1
	b.lt	.Lskip_pmu_\@			 
	mrs	x0, pmcr_el0			 
	ubfx	x0, x0, #11, #5			 
.Lskip_pmu_\@:
	csel	x2, xzr, x0, lt			 
	ubfx	x0, x1, #ID_AA64DFR0_EL1_PMSVer_SHIFT, #4
	cbz	x0, .Lskip_spe_\@		 
	mrs_s	x0, SYS_PMBIDR_EL1               
	and	x0, x0, #(1 << PMBIDR_EL1_P_SHIFT)
	cbnz	x0, .Lskip_spe_el2_\@		 
	mov	x0, #(1 << PMSCR_EL2_PCT_SHIFT | \
		      1 << PMSCR_EL2_PA_SHIFT)
	msr_s	SYS_PMSCR_EL2, x0		 
.Lskip_spe_el2_\@:
	mov	x0, #(MDCR_EL2_E2PB_MASK << MDCR_EL2_E2PB_SHIFT)
	orr	x2, x2, x0			 
.Lskip_spe_\@:
	ubfx	x0, x1, #ID_AA64DFR0_EL1_TraceBuffer_SHIFT, #4
	cbz	x0, .Lskip_trace_\@		 
	mrs_s	x0, SYS_TRBIDR_EL1
	and	x0, x0, TRBIDR_EL1_P
	cbnz	x0, .Lskip_trace_\@		 
	mov	x0, #(MDCR_EL2_E2TB_MASK << MDCR_EL2_E2TB_SHIFT)
	orr	x2, x2, x0			 
.Lskip_trace_\@:
	msr	mdcr_el2, x2			 
.endm
.macro __init_el2_lor
	mrs	x1, id_aa64mmfr1_el1
	ubfx	x0, x1, #ID_AA64MMFR1_EL1_LO_SHIFT, 4
	cbz	x0, .Lskip_lor_\@
	msr_s	SYS_LORC_EL1, xzr
.Lskip_lor_\@:
.endm
.macro __init_el2_stage2
	msr	vttbr_el2, xzr
.endm
.macro __init_el2_gicv3
	mrs	x0, id_aa64pfr0_el1
	ubfx	x0, x0, #ID_AA64PFR0_EL1_GIC_SHIFT, #4
	cbz	x0, .Lskip_gicv3_\@
	mrs_s	x0, SYS_ICC_SRE_EL2
	orr	x0, x0, #ICC_SRE_EL2_SRE	 
	orr	x0, x0, #ICC_SRE_EL2_ENABLE	 
	msr_s	SYS_ICC_SRE_EL2, x0
	isb					 
	mrs_s	x0, SYS_ICC_SRE_EL2		 
	tbz	x0, #0, .Lskip_gicv3_\@		 
	msr_s	SYS_ICH_HCR_EL2, xzr		 
.Lskip_gicv3_\@:
.endm
.macro __init_el2_hstr
	msr	hstr_el2, xzr			 
.endm
.macro __init_el2_nvhe_idregs
	mrs	x0, midr_el1
	mrs	x1, mpidr_el1
	msr	vpidr_el2, x0
	msr	vmpidr_el2, x1
.endm
.macro __init_el2_cptr
	__check_hvhe .LnVHE_\@, x1
	mov	x0, #(CPACR_EL1_FPEN_EL1EN | CPACR_EL1_FPEN_EL0EN)
	msr	cpacr_el1, x0
	b	.Lskip_set_cptr_\@
.LnVHE_\@:
	mov	x0, #0x33ff
	msr	cptr_el2, x0			 
.Lskip_set_cptr_\@:
.endm
.macro __init_el2_fgt
	mrs	x1, id_aa64mmfr0_el1
	ubfx	x1, x1, #ID_AA64MMFR0_EL1_FGT_SHIFT, #4
	cbz	x1, .Lskip_fgt_\@
	mov	x0, xzr
	mrs	x1, id_aa64dfr0_el1
	ubfx	x1, x1, #ID_AA64DFR0_EL1_PMSVer_SHIFT, #4
	cmp	x1, #3
	b.lt	.Lset_debug_fgt_\@
	orr	x0, x0, #(1 << 62)
.Lset_debug_fgt_\@:
	msr_s	SYS_HDFGRTR_EL2, x0
	msr_s	SYS_HDFGWTR_EL2, x0
	mov	x0, xzr
	mrs	x1, id_aa64pfr1_el1
	ubfx	x1, x1, #ID_AA64PFR1_EL1_SME_SHIFT, #4
	cbz	x1, .Lset_pie_fgt_\@
	orr	x0, x0, #HFGxTR_EL2_nSMPRI_EL1_MASK
	orr	x0, x0, #HFGxTR_EL2_nTPIDR2_EL0_MASK
.Lset_pie_fgt_\@:
	mrs_s	x1, SYS_ID_AA64MMFR3_EL1
	ubfx	x1, x1, #ID_AA64MMFR3_EL1_S1PIE_SHIFT, #4
	cbz	x1, .Lset_fgt_\@
	orr	x0, x0, #HFGxTR_EL2_nPIR_EL1
	orr	x0, x0, #HFGxTR_EL2_nPIRE0_EL1
.Lset_fgt_\@:
	msr_s	SYS_HFGRTR_EL2, x0
	msr_s	SYS_HFGWTR_EL2, x0
	msr_s	SYS_HFGITR_EL2, xzr
	mrs	x1, id_aa64pfr0_el1		 
	ubfx	x1, x1, #ID_AA64PFR0_EL1_AMU_SHIFT, #4
	cbz	x1, .Lskip_fgt_\@
	msr_s	SYS_HAFGRTR_EL2, xzr
.Lskip_fgt_\@:
.endm
.macro __init_el2_nvhe_prepare_eret
	mov	x0, #INIT_PSTATE_EL1
	msr	spsr_el2, x0
.endm
.macro init_el2_state
	__init_el2_sctlr
	__init_el2_hcrx
	__init_el2_timers
	__init_el2_debug
	__init_el2_lor
	__init_el2_stage2
	__init_el2_gicv3
	__init_el2_hstr
	__init_el2_nvhe_idregs
	__init_el2_cptr
	__init_el2_fgt
.endm
#ifndef __KVM_NVHE_HYPERVISOR__
.macro __check_override idreg, fld, width, pass, fail, tmp1, tmp2
	ubfx	\tmp1, \tmp1, #\fld, #\width
	cbz	\tmp1, \fail
	adr_l	\tmp1, \idreg\()_override
	ldr	\tmp2, [\tmp1, FTR_OVR_VAL_OFFSET]
	ldr	\tmp1, [\tmp1, FTR_OVR_MASK_OFFSET]
	ubfx	\tmp2, \tmp2, #\fld, #\width
	ubfx	\tmp1, \tmp1, #\fld, #\width
	cmp	\tmp1, xzr
	and	\tmp2, \tmp2, \tmp1
	csinv	\tmp2, \tmp2, xzr, ne
	cbnz	\tmp2, \pass
	b	\fail
.endm
.macro check_override idreg, fld, pass, fail, tmp1, tmp2
	mrs	\tmp1, \idreg\()_el1
	__check_override \idreg \fld 4 \pass \fail \tmp1 \tmp2
.endm
#else
.macro __check_override idreg, fld, width, pass, fail, tmp, ignore
	ldr_l	\tmp, \idreg\()_el1_sys_val
	ubfx	\tmp, \tmp, #\fld, #\width
	cbnz	\tmp, \pass
	b	\fail
.endm
.macro check_override idreg, fld, pass, fail, tmp, ignore
	__check_override \idreg \fld 4 \pass \fail \tmp \ignore
.endm
#endif
.macro finalise_el2_state
	check_override id_aa64pfr0, ID_AA64PFR0_EL1_SVE_SHIFT, .Linit_sve_\@, .Lskip_sve_\@, x1, x2
.Linit_sve_\@:	 
	__check_hvhe .Lcptr_nvhe_\@, x1
	mrs	x0, cpacr_el1			 
	orr	x0, x0, #(CPACR_EL1_ZEN_EL1EN | CPACR_EL1_ZEN_EL0EN)
	msr	cpacr_el1, x0
	b	.Lskip_set_cptr_\@
.Lcptr_nvhe_\@:  
	mrs	x0, cptr_el2			 
	bic	x0, x0, #CPTR_EL2_TZ
	msr	cptr_el2, x0
.Lskip_set_cptr_\@:
	isb
	mov	x1, #ZCR_ELx_LEN_MASK		 
	msr_s	SYS_ZCR_EL2, x1			 
.Lskip_sve_\@:
	check_override id_aa64pfr1, ID_AA64PFR1_EL1_SME_SHIFT, .Linit_sme_\@, .Lskip_sme_\@, x1, x2
.Linit_sme_\@:	 
	__check_hvhe .Lcptr_nvhe_sme_\@, x1
	mrs	x0, cpacr_el1			 
	orr	x0, x0, #(CPACR_EL1_SMEN_EL0EN | CPACR_EL1_SMEN_EL1EN)
	msr	cpacr_el1, x0
	b	.Lskip_set_cptr_sme_\@
.Lcptr_nvhe_sme_\@:  
	mrs	x0, cptr_el2			 
	bic	x0, x0, #CPTR_EL2_TSM
	msr	cptr_el2, x0
.Lskip_set_cptr_sme_\@:
	isb
	mrs	x1, sctlr_el2
	orr	x1, x1, #SCTLR_ELx_ENTP2	 
	msr	sctlr_el2, x1
	isb
	mov	x0, #0				 
	mrs_s	x1, SYS_ID_AA64SMFR0_EL1
	__check_override id_aa64smfr0, ID_AA64SMFR0_EL1_FA64_SHIFT, 1, .Linit_sme_fa64_\@, .Lskip_sme_fa64_\@, x1, x2
.Linit_sme_fa64_\@:
	orr	x0, x0, SMCR_ELx_FA64_MASK
.Lskip_sme_fa64_\@:
	mrs_s	x1, SYS_ID_AA64SMFR0_EL1
	__check_override id_aa64smfr0, ID_AA64SMFR0_EL1_SMEver_SHIFT, 4, .Linit_sme_zt0_\@, .Lskip_sme_zt0_\@, x1, x2
.Linit_sme_zt0_\@:
	orr	x0, x0, SMCR_ELx_EZT0_MASK
.Lskip_sme_zt0_\@:
	orr	x0, x0, #SMCR_ELx_LEN_MASK	 
	msr_s	SYS_SMCR_EL2, x0		 
	mrs_s	x1, SYS_SMIDR_EL1		 
	ubfx    x1, x1, #SMIDR_EL1_SMPS_SHIFT, #1
	cbz     x1, .Lskip_sme_\@
	msr_s	SYS_SMPRIMAP_EL2, xzr		 
.Lskip_sme_\@:
.endm
#endif  
