


#ifndef _CPUFREQ_GOVERNOR_H
#define _CPUFREQ_GOVERNOR_H

#include <linux/atomic.h>
#include <linux/irq_work.h>
#include <linux/cpufreq.h>
#include <linux/sched/cpufreq.h>
#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/mutex.h>


enum {OD_NORMAL_SAMPLE, OD_SUB_SAMPLE};




struct dbs_data {
	struct gov_attr_set attr_set;
	struct dbs_governor *gov;
	void *tuners;
	unsigned int ignore_nice_load;
	unsigned int sampling_rate;
	unsigned int sampling_down_factor;
	unsigned int up_threshold;
	unsigned int io_is_busy;
};

static inline struct dbs_data *to_dbs_data(struct gov_attr_set *attr_set)
{
	return container_of(attr_set, struct dbs_data, attr_set);
}

#define gov_show_one(_gov, file_name)					\
static ssize_t file_name##_show						\
(struct gov_attr_set *attr_set, char *buf)				\
{									\
	struct dbs_data *dbs_data = to_dbs_data(attr_set);		\
	struct _gov##_dbs_tuners *tuners = dbs_data->tuners;		\
	return sprintf(buf, "%u\n", tuners->file_name);			\
}

#define gov_show_one_common(file_name)					\
static ssize_t file_name##_show						\
(struct gov_attr_set *attr_set, char *buf)				\
{									\
	struct dbs_data *dbs_data = to_dbs_data(attr_set);		\
	return sprintf(buf, "%u\n", dbs_data->file_name);		\
}

#define gov_attr_ro(_name)						\
static struct governor_attr _name = __ATTR_RO(_name)

#define gov_attr_rw(_name)						\
static struct governor_attr _name = __ATTR_RW(_name)


struct policy_dbs_info {
	struct cpufreq_policy *policy;
	
	struct mutex update_mutex;

	u64 last_sample_time;
	s64 sample_delay_ns;
	atomic_t work_count;
	struct irq_work irq_work;
	struct work_struct work;
	
	struct dbs_data *dbs_data;
	struct list_head list;
	
	unsigned int rate_mult;
	unsigned int idle_periods;	
	
	bool is_shared;		
	bool work_in_progress;	
};

static inline void gov_update_sample_delay(struct policy_dbs_info *policy_dbs,
					   unsigned int delay_us)
{
	policy_dbs->sample_delay_ns = delay_us * NSEC_PER_USEC;
}


struct cpu_dbs_info {
	u64 prev_cpu_idle;
	u64 prev_update_time;
	u64 prev_cpu_nice;
	
	unsigned int prev_load;
	struct update_util_data update_util;
	struct policy_dbs_info *policy_dbs;
};


struct dbs_governor {
	struct cpufreq_governor gov;
	struct kobj_type kobj_type;

	
	struct dbs_data *gdbs_data;

	unsigned int (*gov_dbs_update)(struct cpufreq_policy *policy);
	struct policy_dbs_info *(*alloc)(void);
	void (*free)(struct policy_dbs_info *policy_dbs);
	int (*init)(struct dbs_data *dbs_data);
	void (*exit)(struct dbs_data *dbs_data);
	void (*start)(struct cpufreq_policy *policy);
};

static inline struct dbs_governor *dbs_governor_of(struct cpufreq_policy *policy)
{
	return container_of(policy->governor, struct dbs_governor, gov);
}


int cpufreq_dbs_governor_init(struct cpufreq_policy *policy);
void cpufreq_dbs_governor_exit(struct cpufreq_policy *policy);
int cpufreq_dbs_governor_start(struct cpufreq_policy *policy);
void cpufreq_dbs_governor_stop(struct cpufreq_policy *policy);
void cpufreq_dbs_governor_limits(struct cpufreq_policy *policy);

#define CPUFREQ_DBS_GOVERNOR_INITIALIZER(_name_)			\
	{								\
		.name = _name_,						\
		.flags = CPUFREQ_GOV_DYNAMIC_SWITCHING,			\
		.owner = THIS_MODULE,					\
		.init = cpufreq_dbs_governor_init,			\
		.exit = cpufreq_dbs_governor_exit,			\
		.start = cpufreq_dbs_governor_start,			\
		.stop = cpufreq_dbs_governor_stop,			\
		.limits = cpufreq_dbs_governor_limits,			\
	}


struct od_ops {
	unsigned int (*powersave_bias_target)(struct cpufreq_policy *policy,
			unsigned int freq_next, unsigned int relation);
};

unsigned int dbs_update(struct cpufreq_policy *policy);
void od_register_powersave_bias_handler(unsigned int (*f)
		(struct cpufreq_policy *, unsigned int, unsigned int),
		unsigned int powersave_bias);
void od_unregister_powersave_bias_handler(void);
ssize_t sampling_rate_store(struct gov_attr_set *attr_set, const char *buf,
			    size_t count);
void gov_update_cpu_data(struct dbs_data *dbs_data);
#endif 
