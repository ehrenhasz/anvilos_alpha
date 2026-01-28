#ifndef _LINUX_POWERPC_PERF_ISA207_COMMON_H_
#define _LINUX_POWERPC_PERF_ISA207_COMMON_H_
#include <linux/kernel.h>
#include <linux/perf_event.h>
#include <asm/firmware.h>
#include <asm/cputable.h>
#include "internal.h"
#define EVENT_EBB_MASK		1ull
#define EVENT_EBB_SHIFT		PERF_EVENT_CONFIG_EBB_SHIFT
#define EVENT_BHRB_MASK		1ull
#define EVENT_BHRB_SHIFT	62
#define EVENT_WANTS_BHRB	(EVENT_BHRB_MASK << EVENT_BHRB_SHIFT)
#define EVENT_IFM_MASK		3ull
#define EVENT_IFM_SHIFT		60
#define EVENT_THR_CMP_SHIFT	40	 
#define EVENT_THR_CMP_MASK	0x3ff
#define EVENT_THR_CTL_SHIFT	32	 
#define EVENT_THR_CTL_MASK	0xffull
#define EVENT_THR_SEL_SHIFT	29	 
#define EVENT_THR_SEL_MASK	0x7
#define EVENT_THRESH_SHIFT	29	 
#define EVENT_THRESH_MASK	0x1fffffull
#define EVENT_SAMPLE_SHIFT	24	 
#define EVENT_SAMPLE_MASK	0x1f
#define EVENT_CACHE_SEL_SHIFT	20	 
#define EVENT_CACHE_SEL_MASK	0xf
#define EVENT_IS_L1		(4 << EVENT_CACHE_SEL_SHIFT)
#define EVENT_PMC_SHIFT		16	 
#define EVENT_PMC_MASK		0xf
#define EVENT_UNIT_SHIFT	12	 
#define EVENT_UNIT_MASK		0xf
#define EVENT_COMBINE_SHIFT	11	 
#define EVENT_COMBINE_MASK	0x1
#define EVENT_COMBINE(v)	(((v) >> EVENT_COMBINE_SHIFT) & EVENT_COMBINE_MASK)
#define EVENT_MARKED_SHIFT	8	 
#define EVENT_MARKED_MASK	0x1
#define EVENT_IS_MARKED		(EVENT_MARKED_MASK << EVENT_MARKED_SHIFT)
#define EVENT_PSEL_MASK		0xff	 
#define EVENT_LINUX_MASK	\
	((EVENT_EBB_MASK  << EVENT_EBB_SHIFT)			|	\
	 (EVENT_BHRB_MASK << EVENT_BHRB_SHIFT)			|	\
	 (EVENT_IFM_MASK  << EVENT_IFM_SHIFT))
#define EVENT_VALID_MASK	\
	((EVENT_THRESH_MASK    << EVENT_THRESH_SHIFT)		|	\
	 (EVENT_SAMPLE_MASK    << EVENT_SAMPLE_SHIFT)		|	\
	 (EVENT_CACHE_SEL_MASK << EVENT_CACHE_SEL_SHIFT)	|	\
	 (EVENT_PMC_MASK       << EVENT_PMC_SHIFT)		|	\
	 (EVENT_UNIT_MASK      << EVENT_UNIT_SHIFT)		|	\
	 (EVENT_COMBINE_MASK   << EVENT_COMBINE_SHIFT)		|	\
	 (EVENT_MARKED_MASK    << EVENT_MARKED_SHIFT)		|	\
	  EVENT_LINUX_MASK					|	\
	  EVENT_PSEL_MASK)
#define ONLY_PLM \
	(PERF_SAMPLE_BRANCH_USER        |\
	 PERF_SAMPLE_BRANCH_KERNEL      |\
	 PERF_SAMPLE_BRANCH_HV)
#define p9_EVENT_COMBINE_SHIFT	10	 
#define p9_EVENT_COMBINE_MASK	0x3ull
#define p9_EVENT_COMBINE(v)	(((v) >> p9_EVENT_COMBINE_SHIFT) & p9_EVENT_COMBINE_MASK)
#define p9_SDAR_MODE_SHIFT	50
#define p9_SDAR_MODE_MASK	0x3ull
#define p9_SDAR_MODE(v)		(((v) >> p9_SDAR_MODE_SHIFT) & p9_SDAR_MODE_MASK)
#define p9_EVENT_VALID_MASK		\
	((p9_SDAR_MODE_MASK   << p9_SDAR_MODE_SHIFT		|	\
	(EVENT_THRESH_MASK    << EVENT_THRESH_SHIFT)		|	\
	(EVENT_SAMPLE_MASK    << EVENT_SAMPLE_SHIFT)		|	\
	(EVENT_CACHE_SEL_MASK << EVENT_CACHE_SEL_SHIFT)		|	\
	(EVENT_PMC_MASK       << EVENT_PMC_SHIFT)		|	\
	(EVENT_UNIT_MASK      << EVENT_UNIT_SHIFT)		|	\
	(p9_EVENT_COMBINE_MASK << p9_EVENT_COMBINE_SHIFT)	|	\
	(EVENT_MARKED_MASK    << EVENT_MARKED_SHIFT)		|	\
	 EVENT_LINUX_MASK					|	\
	 EVENT_PSEL_MASK))
#define p10_SDAR_MODE_SHIFT		22
#define p10_SDAR_MODE_MASK		0x3ull
#define p10_SDAR_MODE(v)		(((v) >> p10_SDAR_MODE_SHIFT) & \
					p10_SDAR_MODE_MASK)
#define p10_EVENT_L2L3_SEL_MASK		0x1f
#define p10_L2L3_SEL_SHIFT		3
#define p10_L2L3_EVENT_SHIFT		40
#define p10_EVENT_THRESH_MASK		0xffffull
#define p10_EVENT_CACHE_SEL_MASK	0x3ull
#define p10_EVENT_MMCR3_MASK		0x7fffull
#define p10_EVENT_MMCR3_SHIFT		45
#define p10_EVENT_RADIX_SCOPE_QUAL_SHIFT	9
#define p10_EVENT_RADIX_SCOPE_QUAL_MASK	0x1
#define p10_MMCR1_RADIX_SCOPE_QUAL_SHIFT	45
#define p10_EVENT_THR_CMP_SHIFT        0
#define p10_EVENT_THR_CMP_MASK 0x3FFFFull
#define p10_EVENT_VALID_MASK		\
	((p10_SDAR_MODE_MASK   << p10_SDAR_MODE_SHIFT		|	\
	(p10_EVENT_THRESH_MASK  << EVENT_THRESH_SHIFT)		|	\
	(EVENT_SAMPLE_MASK     << EVENT_SAMPLE_SHIFT)		|	\
	(p10_EVENT_CACHE_SEL_MASK  << EVENT_CACHE_SEL_SHIFT)	|	\
	(EVENT_PMC_MASK        << EVENT_PMC_SHIFT)		|	\
	(EVENT_UNIT_MASK       << EVENT_UNIT_SHIFT)		|	\
	(p9_EVENT_COMBINE_MASK << p9_EVENT_COMBINE_SHIFT)	|	\
	(p10_EVENT_MMCR3_MASK  << p10_EVENT_MMCR3_SHIFT)	|	\
	(EVENT_MARKED_MASK     << EVENT_MARKED_SHIFT)		|	\
	(p10_EVENT_RADIX_SCOPE_QUAL_MASK << p10_EVENT_RADIX_SCOPE_QUAL_SHIFT)	|	\
	 EVENT_LINUX_MASK					|	\
	EVENT_PSEL_MASK))
#define CNST_FAB_MATCH_VAL(v)	(((v) & EVENT_THR_CTL_MASK) << 56)
#define CNST_FAB_MATCH_MASK	CNST_FAB_MATCH_VAL(EVENT_THR_CTL_MASK)
#define CNST_THRESH_VAL(v)	(((v) & EVENT_THRESH_MASK) << 32)
#define CNST_THRESH_MASK	CNST_THRESH_VAL(EVENT_THRESH_MASK)
#define CNST_THRESH_CTL_SEL_VAL(v)	(((v) & 0x7ffull) << 32)
#define CNST_THRESH_CTL_SEL_MASK	CNST_THRESH_CTL_SEL_VAL(0x7ff)
#define p10_CNST_THRESH_CMP_VAL(v) (((v) & 0x7ffull) << 43)
#define p10_CNST_THRESH_CMP_MASK   p10_CNST_THRESH_CMP_VAL(0x7ff)
#define CNST_EBB_VAL(v)		(((v) & EVENT_EBB_MASK) << 24)
#define CNST_EBB_MASK		CNST_EBB_VAL(EVENT_EBB_MASK)
#define CNST_IFM_VAL(v)		(((v) & EVENT_IFM_MASK) << 25)
#define CNST_IFM_MASK		CNST_IFM_VAL(EVENT_IFM_MASK)
#define CNST_L1_QUAL_VAL(v)	(((v) & 3) << 22)
#define CNST_L1_QUAL_MASK	CNST_L1_QUAL_VAL(3)
#define CNST_SAMPLE_VAL(v)	(((v) & EVENT_SAMPLE_MASK) << 16)
#define CNST_SAMPLE_MASK	CNST_SAMPLE_VAL(EVENT_SAMPLE_MASK)
#define CNST_CACHE_GROUP_VAL(v)	(((v) & 0xffull) << 55)
#define CNST_CACHE_GROUP_MASK	CNST_CACHE_GROUP_VAL(0xff)
#define CNST_CACHE_PMC4_VAL	(1ull << 54)
#define CNST_CACHE_PMC4_MASK	CNST_CACHE_PMC4_VAL
#define CNST_L2L3_GROUP_VAL(v)	(((v) & 0x1full) << 55)
#define CNST_L2L3_GROUP_MASK	CNST_L2L3_GROUP_VAL(0x1f)
#define CNST_RADIX_SCOPE_GROUP_VAL(v)	(((v) & 0x1ull) << 21)
#define CNST_RADIX_SCOPE_GROUP_MASK	CNST_RADIX_SCOPE_GROUP_VAL(1)
#define CNST_NC_SHIFT		12
#define CNST_NC_VAL		(1 << CNST_NC_SHIFT)
#define CNST_NC_MASK		(8 << CNST_NC_SHIFT)
#define ISA207_TEST_ADDER	(3 << CNST_NC_SHIFT)
#define CNST_PMC_SHIFT(pmc)	((pmc - 1) * 2)
#define CNST_PMC_VAL(pmc)	(1 << CNST_PMC_SHIFT(pmc))
#define CNST_PMC_MASK(pmc)	(2 << CNST_PMC_SHIFT(pmc))
#define ISA207_ADD_FIELDS	\
	CNST_PMC_VAL(1) | CNST_PMC_VAL(2) | CNST_PMC_VAL(3) | \
	CNST_PMC_VAL(4) | CNST_PMC_VAL(5) | CNST_PMC_VAL(6) | CNST_NC_VAL
#define MMCR1_UNIT_SHIFT(pmc)		(60 - (4 * ((pmc) - 1)))
#define MMCR1_COMBINE_SHIFT(pmc)	(35 - ((pmc) - 1))
#define MMCR1_PMCSEL_SHIFT(pmc)		(24 - (((pmc) - 1)) * 8)
#define MMCR1_FAB_SHIFT			36
#define MMCR1_DC_IC_QUAL_MASK		0x3
#define MMCR1_DC_IC_QUAL_SHIFT		46
#define p9_MMCR1_COMBINE_SHIFT(pmc)	(38 - ((pmc - 1) * 2))
#define MMCRA_SAMP_MODE_SHIFT		1
#define MMCRA_SAMP_ELIG_SHIFT		4
#define MMCRA_SAMP_ELIG_MASK		7
#define MMCRA_THR_CTL_SHIFT		8
#define MMCRA_THR_SEL_SHIFT		16
#define MMCRA_THR_CMP_SHIFT		32
#define MMCRA_SDAR_MODE_SHIFT		42
#define MMCRA_SDAR_MODE_TLB		(1ull << MMCRA_SDAR_MODE_SHIFT)
#define MMCRA_SDAR_MODE_NO_UPDATES	~(0x3ull << MMCRA_SDAR_MODE_SHIFT)
#define MMCRA_SDAR_MODE_DCACHE		(2ull << MMCRA_SDAR_MODE_SHIFT)
#define MMCRA_IFM_SHIFT			30
#define MMCRA_THR_CTR_MANT_SHIFT	19
#define MMCRA_THR_CTR_MANT_MASK		0x7Ful
#define MMCRA_THR_CTR_MANT(v)		(((v) >> MMCRA_THR_CTR_MANT_SHIFT) &\
						MMCRA_THR_CTR_MANT_MASK)
#define MMCRA_THR_CTR_EXP_SHIFT		27
#define MMCRA_THR_CTR_EXP_MASK		0x7ul
#define MMCRA_THR_CTR_EXP(v)		(((v) >> MMCRA_THR_CTR_EXP_SHIFT) &\
						MMCRA_THR_CTR_EXP_MASK)
#define P10_MMCRA_THR_CTR_MANT_MASK	0xFFul
#define P10_MMCRA_THR_CTR_MANT(v)	(((v) >> MMCRA_THR_CTR_MANT_SHIFT) &\
						P10_MMCRA_THR_CTR_MANT_MASK)
#define p9_MMCRA_THR_CMP_SHIFT	45
#define MMCR2_FCS(pmc)			(1ull << (63 - (((pmc) - 1) * 9)))
#define MMCR2_FCP(pmc)			(1ull << (62 - (((pmc) - 1) * 9)))
#define MMCR2_FCWAIT(pmc)		(1ull << (58 - (((pmc) - 1) * 9)))
#define MMCR2_FCH(pmc)			(1ull << (57 - (((pmc) - 1) * 9)))
#define MAX_ALT				2
#define MAX_PMU_COUNTERS		6
#define MMCR3_SHIFT(pmc)		(49 - (15 * ((pmc) - 1)))
#define ISA207_SIER_TYPE_SHIFT		15
#define ISA207_SIER_TYPE_MASK		(0x7ull << ISA207_SIER_TYPE_SHIFT)
#define ISA207_SIER_LDST_SHIFT		1
#define ISA207_SIER_LDST_MASK		(0x7ull << ISA207_SIER_LDST_SHIFT)
#define ISA207_SIER_DATA_SRC_SHIFT	53
#define ISA207_SIER_DATA_SRC_MASK	(0x7ull << ISA207_SIER_DATA_SRC_SHIFT)
#define P10_SIER2_FINISH_CYC(sier2)	(((sier2) >> (63 - 37)) & 0x7fful)
#define P10_SIER2_DISPATCH_CYC(sier2)	(((sier2) >> (63 - 13)) & 0x7fful)
#define P(a, b)				PERF_MEM_S(a, b)
#define PH(a, b)			(P(LVL, HIT) | P(a, b))
#define PM(a, b)			(P(LVL, MISS) | P(a, b))
#define LEVEL(x)			P(LVLNUM, x)
#define REM				P(REMOTE, REMOTE)
int isa207_get_constraint(u64 event, unsigned long *maskp, unsigned long *valp, u64 event_config1);
int isa207_compute_mmcr(u64 event[], int n_ev,
				unsigned int hwc[], struct mmcr_regs *mmcr,
				struct perf_event *pevents[], u32 flags);
void isa207_disable_pmc(unsigned int pmc, struct mmcr_regs *mmcr);
int isa207_get_alternatives(u64 event, u64 alt[], int size, unsigned int flags,
					const unsigned int ev_alt[][MAX_ALT]);
void isa207_get_mem_data_src(union perf_mem_data_src *dsrc, u32 flags,
							struct pt_regs *regs);
void isa207_get_mem_weight(u64 *weight, u64 type);
int isa3XX_check_attr_config(struct perf_event *ev);
#endif
