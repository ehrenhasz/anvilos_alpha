#include <linux/types.h>
#include <asm/hw_irq.h>
#include <linux/device.h>
#include <uapi/asm/perf_event.h>
#define MAX_HWEVENTS		8
#define MAX_EVENT_ALTERNATIVES	8
#define MAX_LIMITED_HWCOUNTERS	2
struct perf_event;
struct mmcr_regs {
	unsigned long mmcr0;
	unsigned long mmcr1;
	unsigned long mmcr2;
	unsigned long mmcra;
	unsigned long mmcr3;
};
struct power_pmu {
	const char	*name;
	int		n_counter;
	int		max_alternatives;
	unsigned long	add_fields;
	unsigned long	test_adder;
	int		(*compute_mmcr)(u64 events[], int n_ev,
				unsigned int hwc[], struct mmcr_regs *mmcr,
				struct perf_event *pevents[], u32 flags);
	int		(*get_constraint)(u64 event_id, unsigned long *mskp,
				unsigned long *valp, u64 event_config1);
	int		(*get_alternatives)(u64 event_id, unsigned int flags,
				u64 alt[]);
	void		(*get_mem_data_src)(union perf_mem_data_src *dsrc,
				u32 flags, struct pt_regs *regs);
	void		(*get_mem_weight)(u64 *weight, u64 type);
	unsigned long	group_constraint_mask;
	unsigned long	group_constraint_val;
	u64             (*bhrb_filter_map)(u64 branch_sample_type);
	void            (*config_bhrb)(u64 pmu_bhrb_filter);
	void		(*disable_pmc)(unsigned int pmc, struct mmcr_regs *mmcr);
	int		(*limited_pmc_event)(u64 event_id);
	u32		flags;
	const struct attribute_group	**attr_groups;
	int		n_generic;
	int		*generic_events;
	u64		(*cache_events)[PERF_COUNT_HW_CACHE_MAX]
			       [PERF_COUNT_HW_CACHE_OP_MAX]
			       [PERF_COUNT_HW_CACHE_RESULT_MAX];
	int		n_blacklist_ev;
	int 		*blacklist_ev;
	int		bhrb_nr;
	int		capabilities;
	int		(*check_attr_config)(struct perf_event *ev);
};
#define PPMU_LIMITED_PMC5_6	0x00000001  
#define PPMU_ALT_SIPR		0x00000002  
#define PPMU_NO_SIPR		0x00000004  
#define PPMU_NO_CONT_SAMPLING	0x00000008  
#define PPMU_SIAR_VALID		0x00000010  
#define PPMU_HAS_SSLOT		0x00000020  
#define PPMU_HAS_SIER		0x00000040  
#define PPMU_ARCH_207S		0x00000080  
#define PPMU_NO_SIAR		0x00000100  
#define PPMU_ARCH_31		0x00000200  
#define PPMU_P10_DD1		0x00000400  
#define PPMU_HAS_ATTR_CONFIG1	0x00000800  
#define PPMU_LIMITED_PMC_OK	1	 
#define PPMU_LIMITED_PMC_REQD	2	 
#define PPMU_ONLY_COUNT_RUN	4	 
int __init register_power_pmu(struct power_pmu *pmu);
struct pt_regs;
extern unsigned long perf_misc_flags(struct pt_regs *regs);
extern unsigned long perf_instruction_pointer(struct pt_regs *regs);
extern unsigned long int read_bhrb(int n);
#ifdef CONFIG_PPC_PERF_CTRS
#define perf_misc_flags(regs)	perf_misc_flags(regs)
#endif
extern ssize_t power_events_sysfs_show(struct device *dev,
				struct device_attribute *attr, char *page);
#define	EVENT_VAR(_id, _suffix)		event_attr_##_id##_suffix
#define	EVENT_PTR(_id, _suffix)		&EVENT_VAR(_id, _suffix).attr.attr
#define	EVENT_ATTR(_name, _id, _suffix)					\
	PMU_EVENT_ATTR(_name, EVENT_VAR(_id, _suffix), _id,		\
			power_events_sysfs_show)
#define	GENERIC_EVENT_ATTR(_name, _id)	EVENT_ATTR(_name, _id, _g)
#define	GENERIC_EVENT_PTR(_id)		EVENT_PTR(_id, _g)
#define	CACHE_EVENT_ATTR(_name, _id)	EVENT_ATTR(_name, _id, _c)
#define	CACHE_EVENT_PTR(_id)		EVENT_PTR(_id, _c)
#define	POWER_EVENT_ATTR(_name, _id)	EVENT_ATTR(_name, _id, _p)
#define	POWER_EVENT_PTR(_id)		EVENT_PTR(_id, _p)
