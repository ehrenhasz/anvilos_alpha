
 
#include <linux/kernel.h>
#include <linux/jump_label.h>
#include <asm/msr.h>
#include <asm/cpufeature.h>

#include "../perf_event.h"

#define BRS_POISON	0xFFFFFFFFFFFFFFFEULL  

 
union amd_debug_extn_cfg {
	__u64 val;
	struct {
		__u64	rsvd0:2,   
			brsmen:1,  
			rsvd4_3:2, 
			vb:1,      
			rsvd2:10,  
			msroff:4,  
			rsvd3:4,   
			pmc:3,     
			rsvd4:37;  
	};
};

static inline unsigned int brs_from(int idx)
{
	return MSR_AMD_SAMP_BR_FROM + 2 * idx;
}

static inline unsigned int brs_to(int idx)
{
	return MSR_AMD_SAMP_BR_FROM + 2 * idx + 1;
}

static __always_inline void set_debug_extn_cfg(u64 val)
{
	 
	__wrmsr(MSR_AMD_DBG_EXTN_CFG, val | 3ULL << 3, val >> 32);
}

static __always_inline u64 get_debug_extn_cfg(void)
{
	return __rdmsr(MSR_AMD_DBG_EXTN_CFG);
}

static bool __init amd_brs_detect(void)
{
	if (!cpu_feature_enabled(X86_FEATURE_BRS))
		return false;

	switch (boot_cpu_data.x86) {
	case 0x19:  
		x86_pmu.lbr_nr = 16;

		 
		x86_pmu.lbr_sel_map = NULL;
		x86_pmu.lbr_sel_mask = 0;
		break;
	default:
		return false;
	}

	return true;
}

 
static int amd_brs_setup_filter(struct perf_event *event)
{
	u64 type = event->attr.branch_sample_type;

	 
	if (!x86_pmu.lbr_nr)
		return -EOPNOTSUPP;

	 
	if ((type & ~PERF_SAMPLE_BRANCH_PLM_ALL) != PERF_SAMPLE_BRANCH_ANY)
		return -EINVAL;

	return 0;
}

static inline int amd_is_brs_event(struct perf_event *e)
{
	return (e->hw.config & AMD64_RAW_EVENT_MASK) == AMD_FAM19H_BRS_EVENT;
}

int amd_brs_hw_config(struct perf_event *event)
{
	int ret = 0;

	 
	if (!is_sampling_event(event))
		return -EINVAL;

	 
	if (!amd_is_brs_event(event))
		return -EINVAL;

	 
	if (event->attr.freq)
		return -EINVAL;
	 
	if (event->attr.sample_period <= x86_pmu.lbr_nr)
		return -EINVAL;

	 
	ret = amd_brs_setup_filter(event);

	 
	if (!ret)
		event->hw.flags |= PERF_X86_EVENT_AMD_BRS;

	return ret;
}

 
static inline int amd_brs_get_tos(union amd_debug_extn_cfg *cfg)
{
	 
	return (cfg->msroff ? cfg->msroff : x86_pmu.lbr_nr) - 1;
}

 
void amd_brs_reset(void)
{
	if (!cpu_feature_enabled(X86_FEATURE_BRS))
		return;

	 
	set_debug_extn_cfg(0);

	 
	wrmsrl(brs_to(0), BRS_POISON);
}

int __init amd_brs_init(void)
{
	if (!amd_brs_detect())
		return -EOPNOTSUPP;

	pr_cont("%d-deep BRS, ", x86_pmu.lbr_nr);

	return 0;
}

void amd_brs_enable(void)
{
	struct cpu_hw_events *cpuc = this_cpu_ptr(&cpu_hw_events);
	union amd_debug_extn_cfg cfg;

	 
	if (++cpuc->brs_active > 1)
		return;

	cfg.val    = 0;  
	cfg.brsmen = 1;  

	 
	set_debug_extn_cfg(cfg.val);
}

void amd_brs_enable_all(void)
{
	struct cpu_hw_events *cpuc = this_cpu_ptr(&cpu_hw_events);
	if (cpuc->lbr_users)
		amd_brs_enable();
}

void amd_brs_disable(void)
{
	struct cpu_hw_events *cpuc = this_cpu_ptr(&cpu_hw_events);
	union amd_debug_extn_cfg cfg;

	 
	if (!cpuc->brs_active)
		return;

	 
	if (--cpuc->brs_active)
		return;

	 
	cfg.val = get_debug_extn_cfg();

	 
	if (cfg.brsmen) {
		cfg.brsmen = 0;
		set_debug_extn_cfg(cfg.val);
	}
}

void amd_brs_disable_all(void)
{
	struct cpu_hw_events *cpuc = this_cpu_ptr(&cpu_hw_events);
	if (cpuc->lbr_users)
		amd_brs_disable();
}

static bool amd_brs_match_plm(struct perf_event *event, u64 to)
{
	int type = event->attr.branch_sample_type;
	int plm_k = PERF_SAMPLE_BRANCH_KERNEL | PERF_SAMPLE_BRANCH_HV;
	int plm_u = PERF_SAMPLE_BRANCH_USER;

	if (!(type & plm_k) && kernel_ip(to))
		return 0;

	if (!(type & plm_u) && !kernel_ip(to))
		return 0;

	return 1;
}

 
void amd_brs_drain(void)
{
	struct cpu_hw_events *cpuc = this_cpu_ptr(&cpu_hw_events);
	struct perf_event *event = cpuc->events[0];
	struct perf_branch_entry *br = cpuc->lbr_entries;
	union amd_debug_extn_cfg cfg;
	u32 i, nr = 0, num, tos, start;
	u32 shift = 64 - boot_cpu_data.x86_virt_bits;

	 
	if (!event)
		goto empty;

	cfg.val = get_debug_extn_cfg();

	 
	if (WARN_ON_ONCE(cfg.msroff >= x86_pmu.lbr_nr))
		goto empty;

	 
	if (cfg.vb == 0)
		goto empty;

	 
	start = 0;
	tos = amd_brs_get_tos(&cfg);

	num = tos - start + 1;

	 
	for (i = 0; i < num; i++) {
		u32 brs_idx = tos - i;
		u64 from, to;

		rdmsrl(brs_to(brs_idx), to);

		 
		if (to == BRS_POISON)
			break;

		 
		to = (u64)(((s64)to << shift) >> shift);

		if (!amd_brs_match_plm(event, to))
			continue;

		rdmsrl(brs_from(brs_idx), from);

		perf_clear_branch_entry_bitfields(br+nr);

		br[nr].from = from;
		br[nr].to   = to;

		nr++;
	}
empty:
	 
	cpuc->lbr_stack.nr = nr;
}

 
static void amd_brs_poison_buffer(void)
{
	union amd_debug_extn_cfg cfg;
	unsigned int idx;

	 
	cfg.val = get_debug_extn_cfg();

	 
	idx = amd_brs_get_tos(&cfg);

	 
	wrmsrl(brs_to(idx), BRS_POISON);
}

 
void amd_pmu_brs_sched_task(struct perf_event_pmu_context *pmu_ctx, bool sched_in)
{
	struct cpu_hw_events *cpuc = this_cpu_ptr(&cpu_hw_events);

	 
	if (!cpuc->lbr_users)
		return;

	 
	if (sched_in)
		amd_brs_poison_buffer();
}

 
void noinstr perf_amd_brs_lopwr_cb(bool lopwr_in)
{
	struct cpu_hw_events *cpuc = this_cpu_ptr(&cpu_hw_events);
	union amd_debug_extn_cfg cfg;

	 
	if (cpuc->brs_active) {
		cfg.val = get_debug_extn_cfg();
		cfg.brsmen = !lopwr_in;
		set_debug_extn_cfg(cfg.val);
	}
}

DEFINE_STATIC_CALL_NULL(perf_lopwr_cb, perf_amd_brs_lopwr_cb);
EXPORT_STATIC_CALL_TRAMP_GPL(perf_lopwr_cb);

void __init amd_brs_lopwr_init(void)
{
	static_call_update(perf_lopwr_cb, perf_amd_brs_lopwr_cb);
}
