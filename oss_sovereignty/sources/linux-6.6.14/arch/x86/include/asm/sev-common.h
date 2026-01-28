


#ifndef __ASM_X86_SEV_COMMON_H
#define __ASM_X86_SEV_COMMON_H

#define GHCB_MSR_INFO_POS		0
#define GHCB_DATA_LOW			12
#define GHCB_MSR_INFO_MASK		(BIT_ULL(GHCB_DATA_LOW) - 1)

#define GHCB_DATA(v)			\
	(((unsigned long)(v) & ~GHCB_MSR_INFO_MASK) >> GHCB_DATA_LOW)


#define GHCB_MSR_SEV_INFO_RESP		0x001
#define GHCB_MSR_SEV_INFO_REQ		0x002

#define GHCB_MSR_SEV_INFO(_max, _min, _cbit)	\
				\
	((((_max) & 0xffff) << 48) |		\
	 			\
	 (((_min) & 0xffff) << 32) |		\
	 			\
	 (((_cbit) & 0xff)  << 24) |		\
	 GHCB_MSR_SEV_INFO_RESP)

#define GHCB_MSR_INFO(v)		((v) & 0xfffUL)
#define GHCB_MSR_PROTO_MAX(v)		(((v) >> 48) & 0xffff)
#define GHCB_MSR_PROTO_MIN(v)		(((v) >> 32) & 0xffff)


#define GHCB_MSR_CPUID_REQ		0x004
#define GHCB_MSR_CPUID_RESP		0x005
#define GHCB_MSR_CPUID_FUNC_POS		32
#define GHCB_MSR_CPUID_FUNC_MASK	0xffffffff
#define GHCB_MSR_CPUID_VALUE_POS	32
#define GHCB_MSR_CPUID_VALUE_MASK	0xffffffff
#define GHCB_MSR_CPUID_REG_POS		30
#define GHCB_MSR_CPUID_REG_MASK		0x3
#define GHCB_CPUID_REQ_EAX		0
#define GHCB_CPUID_REQ_EBX		1
#define GHCB_CPUID_REQ_ECX		2
#define GHCB_CPUID_REQ_EDX		3
#define GHCB_CPUID_REQ(fn, reg)				\
					\
	(GHCB_MSR_CPUID_REQ |				\
					\
	(((unsigned long)(reg) & 0x3) << 30) |		\
					\
	(((unsigned long)fn) << 32))


#define GHCB_MSR_AP_RESET_HOLD_REQ	0x006
#define GHCB_MSR_AP_RESET_HOLD_RESP	0x007


#define GHCB_MSR_REG_GPA_REQ		0x012
#define GHCB_MSR_REG_GPA_REQ_VAL(v)			\
					\
	(((u64)((v) & GENMASK_ULL(51, 0)) << 12) |	\
					\
	GHCB_MSR_REG_GPA_REQ)

#define GHCB_MSR_REG_GPA_RESP		0x013
#define GHCB_MSR_REG_GPA_RESP_VAL(v)			\
					\
	(((u64)(v) & GENMASK_ULL(63, 12)) >> 12)


enum psc_op {
	SNP_PAGE_STATE_PRIVATE = 1,
	SNP_PAGE_STATE_SHARED,
};

#define GHCB_MSR_PSC_REQ		0x014
#define GHCB_MSR_PSC_REQ_GFN(gfn, op)			\
					\
	(((u64)((op) & 0xf) << 52) |			\
					\
	((u64)((gfn) & GENMASK_ULL(39, 0)) << 12) |	\
					\
	GHCB_MSR_PSC_REQ)

#define GHCB_MSR_PSC_RESP		0x015
#define GHCB_MSR_PSC_RESP_VAL(val)			\
					\
	(((u64)(val) & GENMASK_ULL(63, 32)) >> 32)


#define GHCB_MSR_HV_FT_REQ		0x080
#define GHCB_MSR_HV_FT_RESP		0x081
#define GHCB_MSR_HV_FT_RESP_VAL(v)			\
					\
	(((u64)(v) & GENMASK_ULL(63, 12)) >> 12)

#define GHCB_HV_FT_SNP			BIT_ULL(0)
#define GHCB_HV_FT_SNP_AP_CREATION	BIT_ULL(1)


#define VMGEXIT_PSC_MAX_ENTRY		64

struct psc_hdr {
	u16 cur_entry;
	u16 end_entry;
	u32 reserved;
} __packed;

struct psc_entry {
	u64	cur_page	: 12,
		gfn		: 40,
		operation	: 4,
		pagesize	: 1,
		reserved	: 7;
} __packed;

struct snp_psc_desc {
	struct psc_hdr hdr;
	struct psc_entry entries[VMGEXIT_PSC_MAX_ENTRY];
} __packed;

#define GHCB_MSR_TERM_REQ		0x100
#define GHCB_MSR_TERM_REASON_SET_POS	12
#define GHCB_MSR_TERM_REASON_SET_MASK	0xf
#define GHCB_MSR_TERM_REASON_POS	16
#define GHCB_MSR_TERM_REASON_MASK	0xff

#define GHCB_SEV_TERM_REASON(reason_set, reason_val)	\
					\
	(((((u64)reason_set) &  0xf) << 12) |		\
	 				\
	((((u64)reason_val) & 0xff) << 16))


#define SEV_TERM_SET_GEN		0
#define GHCB_SEV_ES_GEN_REQ		0
#define GHCB_SEV_ES_PROT_UNSUPPORTED	1
#define GHCB_SNP_UNSUPPORTED		2


#define SEV_TERM_SET_LINUX		1
#define GHCB_TERM_REGISTER		0	
#define GHCB_TERM_PSC			1	
#define GHCB_TERM_PVALIDATE		2	
#define GHCB_TERM_NOT_VMPL0		3	
#define GHCB_TERM_CPUID			4	
#define GHCB_TERM_CPUID_HV		5	

#define GHCB_RESP_CODE(v)		((v) & GHCB_MSR_INFO_MASK)


#define GHCB_ERR_NOT_REGISTERED		1
#define GHCB_ERR_INVALID_USAGE		2
#define GHCB_ERR_INVALID_SCRATCH_AREA	3
#define GHCB_ERR_MISSING_INPUT		4
#define GHCB_ERR_INVALID_INPUT		5
#define GHCB_ERR_INVALID_EVENT		6

#endif
