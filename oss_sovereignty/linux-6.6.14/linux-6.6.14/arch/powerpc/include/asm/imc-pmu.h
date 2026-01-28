#ifndef __ASM_POWERPC_IMC_PMU_H
#define __ASM_POWERPC_IMC_PMU_H
#include <linux/perf_event.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/io.h>
#include <asm/opal.h>
#define IMC_DTB_COMPAT			"ibm,opal-in-memory-counters"
#define IMC_DTB_UNIT_COMPAT		"ibm,imc-counters"
#define THREAD_IMC_LDBAR_MASK           0x0003ffffffffe000ULL
#define THREAD_IMC_ENABLE               0x8000000000000000ULL
#define TRACE_IMC_ENABLE		0x4000000000000000ULL
#define IMC_CNTL_BLK_OFFSET		0x3FC00
#define IMC_CNTL_BLK_CMD_OFFSET		8
#define IMC_CNTL_BLK_MODE_OFFSET	32
struct imc_mem_info {
	u64 *vbase;
	u32 id;
};
struct imc_events {
	u32 value;
	char *name;
	char *unit;
	char *scale;
};
struct trace_imc_data {
	u64 tb1;
	u64 ip;
	u64 val;
	u64 cpmc1;
	u64 cpmc2;
	u64 cpmc3;
	u64 cpmc4;
	u64 tb2;
};
#define IMC_FORMAT_ATTR		0
#define IMC_EVENT_ATTR		1
#define IMC_CPUMASK_ATTR	2
#define IMC_NULL_ATTR		3
#define IMC_EVENT_OFFSET_MASK	0xffffffffULL
#define IMC_TRACE_RECORD_TB1_MASK      0x3ffffffffffULL
#define IMC_TRACE_RECORD_VAL_HVPR(x)	((x) >> 62)
struct imc_pmu {
	struct pmu pmu;
	struct imc_mem_info *mem_info;
	struct imc_events *events;
	const struct attribute_group *attr_groups[4];
	u32 counter_mem_size;
	int domain;
	bool imc_counter_mmaped;
};
struct imc_pmu_ref {
	spinlock_t lock;
	unsigned int id;
	int refc;
};
enum {
	IMC_TYPE_THREAD		= 0x1,
	IMC_TYPE_TRACE		= 0x2,
	IMC_TYPE_CORE		= 0x4,
	IMC_TYPE_CHIP           = 0x10,
};
#define IMC_DOMAIN_NEST		1
#define IMC_DOMAIN_CORE		2
#define IMC_DOMAIN_THREAD	3
#define IMC_DOMAIN_TRACE	4
extern int init_imc_pmu(struct device_node *parent,
				struct imc_pmu *pmu_ptr, int pmu_id);
extern void thread_imc_disable(void);
extern int get_max_nest_dev(void);
extern void unregister_thread_imc(void);
#endif  
