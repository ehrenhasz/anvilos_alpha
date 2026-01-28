#ifndef _ASM_POWERPC_CPUIDLE_H
#define _ASM_POWERPC_CPUIDLE_H
#ifdef CONFIG_PPC_POWERNV
#define PNV_THREAD_RUNNING              0
#define PNV_THREAD_NAP                  1
#define PNV_THREAD_SLEEP                2
#define PNV_THREAD_WINKLE               3
#define NR_PNV_CORE_IDLE_LOCK_BIT		28
#define PNV_CORE_IDLE_LOCK_BIT			(1ULL << NR_PNV_CORE_IDLE_LOCK_BIT)
#define PNV_CORE_IDLE_WINKLE_COUNT_SHIFT	16
#define PNV_CORE_IDLE_WINKLE_COUNT		0x00010000
#define PNV_CORE_IDLE_WINKLE_COUNT_BITS		0x000F0000
#define PNV_CORE_IDLE_THREAD_WINKLE_BITS_SHIFT	8
#define PNV_CORE_IDLE_THREAD_WINKLE_BITS	0x0000FF00
#define PNV_CORE_IDLE_THREAD_BITS       	0x000000FF
#define PSSCR_HV_DEFAULT_VAL    (PSSCR_ESL | PSSCR_EC |		    \
				PSSCR_PSLL_MASK | PSSCR_TR_MASK |   \
				PSSCR_MTL_MASK)
#define PSSCR_HV_DEFAULT_MASK   (PSSCR_ESL | PSSCR_EC |		    \
				PSSCR_PSLL_MASK | PSSCR_TR_MASK |   \
				PSSCR_MTL_MASK | PSSCR_RL_MASK)
#define PSSCR_EC_SHIFT    20
#define PSSCR_ESL_SHIFT   21
#define GET_PSSCR_EC(x)   (((x) & PSSCR_EC) >> PSSCR_EC_SHIFT)
#define GET_PSSCR_ESL(x)  (((x) & PSSCR_ESL) >> PSSCR_ESL_SHIFT)
#define GET_PSSCR_RL(x)   ((x) & PSSCR_RL_MASK)
#define ERR_EC_ESL_MISMATCH		-1
#define ERR_DEEP_STATE_ESL_MISMATCH	-2
#ifndef __ASSEMBLY__
#define PNV_IDLE_NAME_LEN    16
struct pnv_idle_states_t {
	char name[PNV_IDLE_NAME_LEN];
	u32 latency_ns;
	u32 residency_ns;
	u64 psscr_val;
	u64 psscr_mask;
	u32 flags;
	bool valid;
};
extern struct pnv_idle_states_t *pnv_idle_states;
extern int nr_pnv_idle_states;
unsigned long pnv_cpu_offline(unsigned int cpu);
int __init validate_psscr_val_mask(u64 *psscr_val, u64 *psscr_mask, u32 flags);
static inline void report_invalid_psscr_val(u64 psscr_val, int err)
{
	switch (err) {
	case ERR_EC_ESL_MISMATCH:
		pr_warn("Invalid psscr 0x%016llx : ESL,EC bits unequal",
			psscr_val);
		break;
	case ERR_DEEP_STATE_ESL_MISMATCH:
		pr_warn("Invalid psscr 0x%016llx : ESL cleared for deep stop-state",
			psscr_val);
	}
}
#endif
#endif
#endif
