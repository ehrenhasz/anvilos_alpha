


#ifndef __CPUPOWER_CPUFREQ_H__
#define __CPUPOWER_CPUFREQ_H__

struct cpufreq_policy {
	unsigned long min;
	unsigned long max;
	char *governor;
};

struct cpufreq_available_governors {
	char *governor;
	struct cpufreq_available_governors *next;
	struct cpufreq_available_governors *first;
};

struct cpufreq_available_frequencies {
	unsigned long frequency;
	struct cpufreq_available_frequencies *next;
	struct cpufreq_available_frequencies *first;
};


struct cpufreq_affected_cpus {
	unsigned int cpu;
	struct cpufreq_affected_cpus *next;
	struct cpufreq_affected_cpus *first;
};

struct cpufreq_stats {
	unsigned long frequency;
	unsigned long long time_in_state;
	struct cpufreq_stats *next;
	struct cpufreq_stats *first;
};



#ifdef __cplusplus
extern "C" {
#endif



unsigned long cpufreq_get_freq_kernel(unsigned int cpu);

unsigned long cpufreq_get_freq_hardware(unsigned int cpu);

#define cpufreq_get(cpu) cpufreq_get_freq_kernel(cpu);



unsigned long cpufreq_get_transition_latency(unsigned int cpu);




int cpufreq_get_hardware_limits(unsigned int cpu,
				unsigned long *min,
				unsigned long *max);




char *cpufreq_get_driver(unsigned int cpu);

void cpufreq_put_driver(char *ptr);





struct cpufreq_policy *cpufreq_get_policy(unsigned int cpu);

void cpufreq_put_policy(struct cpufreq_policy *policy);





struct cpufreq_available_governors
*cpufreq_get_available_governors(unsigned int cpu);

void cpufreq_put_available_governors(
	struct cpufreq_available_governors *first);




struct cpufreq_available_frequencies
*cpufreq_get_available_frequencies(unsigned int cpu);

void cpufreq_put_available_frequencies(
		struct cpufreq_available_frequencies *first);

struct cpufreq_available_frequencies
*cpufreq_get_boost_frequencies(unsigned int cpu);

void cpufreq_put_boost_frequencies(
		struct cpufreq_available_frequencies *first);




struct cpufreq_affected_cpus *cpufreq_get_affected_cpus(unsigned
							int cpu);

void cpufreq_put_affected_cpus(struct cpufreq_affected_cpus *first);




struct cpufreq_affected_cpus *cpufreq_get_related_cpus(unsigned
							int cpu);

void cpufreq_put_related_cpus(struct cpufreq_affected_cpus *first);




struct cpufreq_stats *cpufreq_get_stats(unsigned int cpu,
					unsigned long long *total_time);

void cpufreq_put_stats(struct cpufreq_stats *stats);

unsigned long cpufreq_get_transitions(unsigned int cpu);




int cpufreq_set_policy(unsigned int cpu, struct cpufreq_policy *policy);




int cpufreq_modify_policy_min(unsigned int cpu, unsigned long min_freq);
int cpufreq_modify_policy_max(unsigned int cpu, unsigned long max_freq);
int cpufreq_modify_policy_governor(unsigned int cpu, char *governor);




int cpufreq_set_frequency(unsigned int cpu,
				unsigned long target_frequency);



unsigned long cpufreq_get_sysfs_value_from_table(unsigned int cpu,
						 const char **table,
						 unsigned int index,
						 unsigned int size);

#ifdef __cplusplus
}
#endif

#endif 
